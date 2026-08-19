/* GBVitaEX — src/gb/gb_engine.c
 * Drives mGBA's GB/GBC emulation core as a standalone engine.
 *
 * mGBA uses a vtable-based struct mCore for all operations.
 * We call GBCoreCreate(), wire in our vita2d-friendly pixel buffer and the
 * PSP2 audio pipeline (borrowed from vendor/mgba/src/platform/psp2/), then
 * drive core->runFrame() once per vsync.
 *
 * Audio resampling (GB native 131072 Hz → 48000 Hz) reuses mGBA's
 * mAudioResampler which is already tuned for the PSP2 target.
 */

#include "gb_engine.h"
#include "gb_link.h"    /* gb_link_active(), gb_link_run_frame() */
#include "gbvitaex.h"

/* mGBA core interface */
#include <mgba/core/core.h>
#include <mgba/gb/core.h>
#include <mgba/gb/interface.h>
#include <mgba/internal/gb/gb.h>   /* struct GB, gb->video.frameskip */
#include <mgba-util/vfs.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>
#include <mgba-util/memory.h>

/* PSP2 system */
#include <psp2/power.h>
#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
   Audio pipeline constants (mirror psp2-context.c)
   ────────────────────────────────────────────────────────────────────────── */
#define PSP2_SAMPLES          512
#define PSP2_AUDIO_BUFFERS    16
#define PSP2_SAMPLE_RATE      48000

typedef struct {
    int16_t samples[PSP2_SAMPLES * 2] __attribute__((__aligned__(64)));
    volatile bool full;
} AudioBuffer;

static struct {
    AudioBuffer     bufs[PSP2_AUDIO_BUFFERS];
    int             write_idx;
    int             read_idx;
    SceUID          thread;
    SceUID          mutex;
    SceUID          cond;
    bool            running;
    struct mAudioBuffer    src;
    struct mAudioResampler resampler;
} s_audio;

/* ──────────────────────────────────────────────────────────────────────────
   Pixel buffer (RGB565, 160×144, stride 256 to match vita2d texture)
   ────────────────────────────────────────────────────────────────────────── */
#define GB_TEX_STRIDE 256
static uint16_t s_pixels[GB_SCREEN_H * GB_TEX_STRIDE];

/* ──────────────────────────────────────────────────────────────────────────
   mGBA core instance
   ────────────────────────────────────────────────────────────────────────── */
static struct mCore *s_core     = NULL;
static struct VFile *s_rom_vf   = NULL;
static struct VFile *s_save_vf  = NULL;
static bool          s_loaded   = false;

/* Accessor used by cheat_gb.c — avoids exposing the static pointer directly */
struct mCore *gb_engine_get_core(void) { return s_core; }

/* ──────────────────────────────────────────────────────────────────────────
   Audio thread
   ────────────────────────────────────────────────────────────────────────── */
static int audio_thread_fn(SceSize sz, void *arg) {
    (void)sz; (void)arg;
    int16_t silence[PSP2_SAMPLES * 2];
    memset(silence, 0, sizeof(silence));

    int port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,
                                   PSP2_SAMPLES, PSP2_SAMPLE_RATE,
                                   SCE_AUDIO_OUT_MODE_STEREO);

    /* Apply initial volume */
    int vol = (int)(SCE_AUDIO_OUT_MAX_VOL * (g_emu.audio_volume / 100.0f));
    sceAudioOutSetVolume(port, SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO, &vol);
    int last_vol = g_emu.audio_volume;

    while (s_audio.running) {
        /* Sync volume if changed from settings */
        if (g_emu.audio_volume != last_vol) {
            last_vol = g_emu.audio_volume;
            int v = (int)(SCE_AUDIO_OUT_MAX_VOL * (last_vol / 100.0f));
            sceAudioOutSetVolume(port, SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO, &v);
        }

        sceKernelLockMutex(s_audio.mutex, 1, NULL);

        AudioBuffer *buf = &s_audio.bufs[s_audio.read_idx];
        /* Respect audio_enabled — fall through to silence if muted */
        const void  *out = silence;
        if (buf->full) {
            if (g_emu.audio_enabled) out = buf->samples;
            s_audio.read_idx = (s_audio.read_idx + 1) % PSP2_AUDIO_BUFFERS;
            buf->full = false;
            sceKernelSignalCond(s_audio.cond);
        }
        sceKernelUnlockMutex(s_audio.mutex, 1);
        sceAudioOutOutput(port, out);
    }

    sceAudioOutReleasePort(port);
    sceKernelExitThread(0);
    return 0;
}

/* Called by mGBA after each audio batch is rendered — push to our ring. */
static void on_audio_buffer(struct mAVStream *stream, struct mAudioBuffer *buf) {
    (void)stream; (void)buf;
    sceKernelLockMutex(s_audio.mutex, 1, NULL);
    mAudioResamplerProcess(&s_audio.resampler);
    while (mAudioBufferAvailable(&s_audio.src) >= PSP2_SAMPLES) {
        AudioBuffer *dst = &s_audio.bufs[s_audio.write_idx];
        if (dst->full) {
            /* Buffer full — wait or drop (use wait for correct timing) */
            sceKernelWaitCond(s_audio.cond, NULL);
        }
        mAudioBufferRead(&s_audio.src, dst->samples, PSP2_SAMPLES);
        dst->full     = true;
        s_audio.write_idx = (s_audio.write_idx + 1) % PSP2_AUDIO_BUFFERS;
    }
    sceKernelUnlockMutex(s_audio.mutex, 1);
}

static struct mAVStream s_av_stream = {
    .postVideoFrame   = NULL,
    .postAudioFrame   = NULL,
    .postAudioBuffer  = on_audio_buffer,
    .videoDimensionsChanged = NULL,
    .audioRateChanged = NULL,
};

/* ──────────────────────────────────────────────────────────────────────────
   One-time audio subsystem init
   ────────────────────────────────────────────────────────────────────────── */
static bool s_audio_inited = false;

static void audio_init(unsigned native_hz) {
    if (s_audio_inited) return;

    memset(&s_audio, 0, sizeof(s_audio));
    mAudioBufferInit(&s_audio.src, PSP2_SAMPLES * PSP2_AUDIO_BUFFERS, 2);
    mAudioResamplerInit(&s_audio.resampler, mINTERPOLATOR_COSINE);
    mAudioResamplerSetDestination(&s_audio.resampler, &s_audio.src, PSP2_SAMPLE_RATE);
    mAudioResamplerSetSource(&s_audio.resampler,
                              s_core->getAudioBuffer(s_core),
                              native_hz, true);

    s_audio.mutex   = sceKernelCreateMutex("gb_audio_mx", 0, 0, NULL);
    s_audio.cond    = sceKernelCreateCond("gb_audio_cv", 0, s_audio.mutex, NULL);
    s_audio.running = true;
    s_audio.thread  = sceKernelCreateThread("gb_audio",
                          audio_thread_fn, 0x10000100, 0x10000, 0, 0, NULL);
    sceKernelStartThread(s_audio.thread, 0, NULL);

    s_audio_inited = true;
}

static void audio_shutdown(void) {
    if (!s_audio_inited) return;
    s_audio.running = false;
    sceKernelSignalCond(s_audio.cond);
    sceKernelWaitThreadEnd(s_audio.thread, NULL, NULL);
    sceKernelDeleteThread(s_audio.thread);
    sceKernelDeleteCond(s_audio.cond);
    sceKernelDeleteMutex(s_audio.mutex);
    mAudioResamplerDeinit(&s_audio.resampler);
    mAudioBufferDeinit(&s_audio.src);
    s_audio_inited = false;
}

/* ──────────────────────────────────────────────────────────────────────────
   Public API
   ────────────────────────────────────────────────────────────────────────── */

bool gb_engine_load(const char *rom_path, const char *save_path, bool is_gbc) {
    (void)is_gbc; /* model is auto-detected from cart header by mGBA */

    if (s_loaded) gb_engine_unload();

    /* Boost CPU clock */
    scePowerSetArmClockFrequency(g_emu.cpu_clock_mhz);

    /* Create and init mGBA GB core */
    s_core = GBCoreCreate();
    if (!s_core || !s_core->init(s_core)) {
        fprintf(stderr, "[gb_engine] Failed to create/init GB core\n");
        if (s_core) { free(s_core); s_core = NULL; }
        return false;
    }

    /* Wire up the pixel buffer (RGB565, stride=256 for vita2d) */
    s_core->setVideoBuffer(s_core, (mColor *)s_pixels, GB_TEX_STRIDE);
    memset(s_pixels, 0, sizeof(s_pixels));

    /* Audio buffer size = one batch */
    s_core->setAudioBufferSize(s_core, PSP2_SAMPLES);

    /* Set AVStream for the audio callback */
    s_core->setAVStream(s_core, &s_av_stream);

    /* Load ROM */
    s_rom_vf = VFileOpen(rom_path, O_RDONLY);
    if (!s_rom_vf || !s_core->loadROM(s_core, s_rom_vf)) {
        fprintf(stderr, "[gb_engine] Failed to load ROM: %s\n", rom_path);
        s_core->deinit(s_core);
        s_core = NULL;
        if (s_rom_vf) { s_rom_vf->close(s_rom_vf); s_rom_vf = NULL; }
        return false;
    }

    /* Load save file (SRAM) if it exists */
    s_save_vf = VFileOpen(save_path, O_RDWR | O_CREAT);
    if (s_save_vf) s_core->loadSave(s_core, s_save_vf);

    /* Load official BIOS if available */
    char bios_path[512];
    snprintf(bios_path, sizeof(bios_path), "%s/gb_bios.bin", BIOS_PATH);
    struct VFile *bios_vf = VFileOpen(bios_path, O_RDONLY);
    if (bios_vf) {
        s_core->loadBIOS(s_core, bios_vf, 0);
        bios_vf->close(bios_vf);
    }

    /* Reset: applies model detection (DMG/CGB/AGB), attaches renderer.
     * IMPORTANT: call audio_init BEFORE reset so the resampler source
     * pointer is registered against the buffer that already exists.
     * After reset(), re-register the source since reset reinitialises
     * the GB audio subsystem (the mAudioBuffer address stays stable,
     * but the internal read/write positions are cleared). */
    unsigned native_hz = s_core->audioSampleRate(s_core);
    if (!native_hz) native_hz = 131072;
    audio_init(native_hz);   /* sets up resampler against current buffer */

    s_core->reset(s_core);   /* may reinit audio internals */

    /* Re-register source after reset to keep pointers valid */
    mAudioResamplerSetSource(&s_audio.resampler,
                              s_core->getAudioBuffer(s_core),
                              native_hz, true);

    s_loaded = true;
    return true;
}

void gb_engine_unload(void) {
    if (!s_loaded) return;

    audio_shutdown();

    /* Flush save file before unloading */
    if (s_save_vf) {
        s_core->loadSave(s_core, NULL); /* detach */
        s_save_vf->close(s_save_vf);
        s_save_vf = NULL;
    }

    s_core->unloadROM(s_core);
    s_core->deinit(s_core);
    s_core = NULL;

    if (s_rom_vf) { s_rom_vf->close(s_rom_vf); s_rom_vf = NULL; }

    s_loaded = false;
}

void gb_engine_reset(void) {
    if (!s_loaded) return;
    s_core->reset(s_core);
    /* Re-register audio source after reset */
    if (s_audio_inited) {
        unsigned hz = s_core->audioSampleRate(s_core);
        if (!hz) hz = 131072;
        mAudioResamplerSetSource(&s_audio.resampler,
                                  s_core->getAudioBuffer(s_core),
                                  hz, true);
    }
}

void gb_engine_run_frame(void) {
    if (!s_loaded) return;
    /* Apply frameskip via mGBA's built-in frameskip counter */
    struct GB *gb = s_core->board;
    gb->video.frameskip = g_emu.frameskip;
    s_core->runFrame(s_core);
    /* If GB link-cable is active, step P2 immediately after P1 in lock-step.
     * The GBSIOLockstep driver synchronises SIO byte transfers during runFrame
     * via timing events — both cores must execute the same frame together. */
    if (gb_link_active()) gb_link_run_frame();
}

/* Map unified EMU_KEY_* → mGBA GBA_KEY_* (same bit layout for GB) */
void gb_engine_set_input(uint32_t emu_keys) {
    if (!s_loaded) return;
    s_core->setKeys(s_core, emu_keys); /* layout matches mGBA's GBA_KEY_* */
}

const uint16_t *gb_engine_get_framebuffer(void) {
    return s_pixels;
}

/* GB audio is delivered synchronously via the mAVStream callback.
 * For the drain path (used by the standalone audio mixer) we expose an empty
 * implementation — actual audio comes through the PSP2 audio thread above. */
int gb_engine_drain_audio(int16_t *buf, int max_frames) {
    (void)buf; (void)max_frames;
    return 0; /* audio flows directly through the audio thread */
}

bool gb_engine_save_sram(const char *path) {
    if (!s_loaded || !s_core) return false;
    void  *sram = NULL;
    size_t sz   = s_core->savedataClone(s_core, &sram);
    if (!sz || !sram) return false;
    FILE *f = fopen(path, "wb");
    bool ok = false;
    if (f) { ok = fwrite(sram, 1, sz, f) == sz; fclose(f); }
    free(sram);
    return ok;
}

bool gb_engine_load_sram(const char *path) {
    if (!s_loaded || !s_core) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return false; }
    void *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return false; }
    fread(buf, 1, (size_t)sz, f);
    fclose(f);
    bool ok = s_core->savedataRestore(s_core, buf, (size_t)sz, true);
    free(buf);
    return ok;
}

bool gb_engine_save_state(const char *path) {
    if (!s_loaded || !s_core) return false;
    size_t sz  = s_core->stateSize(s_core);
    void  *buf = malloc(sz);
    if (!buf) return false;
    bool ok = s_core->saveState(s_core, buf);
    if (ok) {
        FILE *f = fopen(path, "wb");
        ok = false;
        if (f) { ok = fwrite(buf, 1, sz, f) == sz; fclose(f); }
    }
    free(buf);
    return ok;
}

bool gb_engine_load_state(const char *path) {
    if (!s_loaded || !s_core) return false;
    size_t sz  = s_core->stateSize(s_core);
    void  *buf = malloc(sz);
    if (!buf) return false;
    FILE *f  = fopen(path, "rb");
    bool  ok = false;
    if (f) {
        ok = fread(buf, 1, sz, f) == sz;
        fclose(f);
    }
    if (ok) ok = s_core->loadState(s_core, buf);
    free(buf);
    return ok;
}

void gb_engine_shutdown(void) {
    gb_engine_unload();
}

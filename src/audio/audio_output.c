/* GBAVitaEX — src/audio/audio_output.c
 * GBA audio output with TDHS pitch correction for fast-forward.
 *
 * Normal path: gpSP ring buffer → sceAudioOutOutput (direct, zero overhead)
 *
 * FF pitch-corrected path (ff_pitch_correct=true):
 *   At N× speed the GBA produces N× as many samples per real-time frame.
 *   We time-compress them back to 1× duration using TDHS so pitch is unchanged.
 *   stretch_samples() is called with ratio = 1/N  (e.g. 0.5 at 2× speed).
 *   ratio < 1 → output shorter than input (what we want for FF compression).
 *
 * TDHS library: https://github.com/dbry/audio-stretch (BSD)
 *   stretch_init(shortest_period, longest_period, num_chans, flags)
 *     shortest/longest period in SAMPLES (not Hz).
 *     At gpSP's 65 536 Hz:  55 Hz ≈ 1192 samples,  333 Hz ≈ 196 samples.
 *   stretch_samples(handle, in, num_in, out, ratio) → num_out samples
 *   stretch_output_capacity(handle, max_in, max_ratio) → safe out buf size
 */

#include "audio_output.h"
#include "stretch.h"

#include <psp2/audioout.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── sceAudioOut ── */
#define MAX_BATCH 1024
static int   s_port      = -1;
static int   s_spf       = 0;
static float s_vol_scale = 1.0f;

/* ── TDHS stretcher ── */
#define STRETCH_CHUNK   1024   /* input frames fed to stretcher at once */

static StretchHandle s_stretcher   = NULL;
static float         s_last_ratio  = 0.0f;

/* Dynamically-allocated output buffer — sized by stretch_output_capacity() */
static int16_t *s_out_buf      = NULL;
static int      s_out_buf_cap  = 0;

/* Small carry buffer for leftover output samples < one sceAudioOut batch */
#define CARRY_CAP  (MAX_BATCH * 4)
static int16_t s_carry[CARRY_CAP * 2];
static int     s_carry_frames = 0;

/* Ensure the output buffer is large enough for up to max_in input frames
 * at the given ratio. */
static void ensure_out_buf(int max_in, float ratio) {
    if (!s_stretcher) return;
    int needed = stretch_output_capacity(s_stretcher, max_in, ratio);
    if (needed > s_out_buf_cap) {
        free(s_out_buf);
        s_out_buf = malloc((size_t)needed * 2 * sizeof(int16_t));
        s_out_buf_cap = s_out_buf ? needed : 0;
    }
}

/* Build or rebuild the stretcher for a new FF ratio. */
static void ensure_stretcher(float compress_ratio) {
    /* compress_ratio = 1/ff_speed (e.g. 0.5 at 2×) */
    if (s_stretcher && s_last_ratio == compress_ratio) return;
    if (s_stretcher) { stretch_deinit(s_stretcher); s_stretcher = NULL; }
    free(s_out_buf); s_out_buf = NULL; s_out_buf_cap = 0;
    s_carry_frames = 0;

    /* Period limits in samples at 65 536 Hz:
     *   333 Hz (upper) → 65536/333 ≈ 196 samples (shortest period)
     *    55 Hz (lower)  → 65536/55  ≈ 1192 samples (longest period)  */
    int flags = (compress_ratio < 0.5f) ? STRETCH_DUAL_FLAG : 0;
    s_stretcher  = stretch_init(196, 1192, 2, flags);
    s_last_ratio = compress_ratio;
}

/* Flush carry buffer: submit full batches to sceAudioOut. */
static void flush_carry(void) {
    while (s_carry_frames >= s_spf) {
        sceAudioOutOutput(s_port, s_carry);
        /* Shift remaining carry samples down */
        int remaining = s_carry_frames - s_spf;
        if (remaining > 0)
            memmove(s_carry, s_carry + s_spf * 2,
                    (size_t)remaining * 2 * sizeof(int16_t));
        s_carry_frames = remaining;
    }
}

/* Append frames to carry buffer and flush complete batches. */
static void carry_append(const int16_t *src, int frames) {
    while (frames > 0) {
        int space = CARRY_CAP - s_carry_frames;
        int copy  = frames < space ? frames : space;
        if (copy <= 0) break;  /* overflow safety */
        memcpy(s_carry + s_carry_frames * 2, src,
               (size_t)copy * 2 * sizeof(int16_t));
        s_carry_frames += copy;
        src    += copy * 2;
        frames -= copy;
        flush_carry();
    }
}

/* ── Public API ── */

void audio_output_init(int sample_rate, int samples_per_frame) {
    if (s_port >= 0) return;
    if (samples_per_frame > MAX_BATCH) samples_per_frame = MAX_BATCH;
    s_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,
                                  samples_per_frame, sample_rate,
                                  SCE_AUDIO_OUT_MODE_STEREO);
    if (s_port < 0)
        fprintf(stderr, "[audio] sceAudioOutOpenPort failed: 0x%08X\n", s_port);
    s_spf = samples_per_frame;
}

void audio_output_shutdown(void) {
    if (s_stretcher) { stretch_deinit(s_stretcher); s_stretcher = NULL; }
    free(s_out_buf); s_out_buf = NULL; s_out_buf_cap = 0;
    if (s_port >= 0) { sceAudioOutReleasePort(s_port); s_port = -1; }
}

void audio_output_submit_ff(const int16_t *buf, int frames,
                             bool pitch_correct, float ff_ratio) {
    if (s_port < 0 || !buf || frames <= 0) return;

    if (!pitch_correct || ff_ratio <= 1.01f) {
        /* ── Normal path: direct output ── */
        if (s_stretcher) { stretch_deinit(s_stretcher); s_stretcher = NULL; }
        s_carry_frames = 0;
        int done = 0;
        while (done < frames) {
            int batch = frames - done;
            if (batch > MAX_BATCH) batch = MAX_BATCH;
            sceAudioOutOutput(s_port, buf + done * 2);
            done += batch;
        }
        return;
    }

    /* ── Pitch-corrected FF path ── */
    /* At ff_ratio = N, GBA generated N× samples. Compress to 1× = ratio 1/N */
    float compress = 1.0f / ff_ratio;
    ensure_stretcher(compress);

    if (!s_stretcher) {
        /* Fallback: drop extra samples to keep near-correct pitch */
        int keep = (int)(frames * compress);
        if (keep < 1) keep = 1;
        if (keep > frames) keep = frames;
        int done = 0;
        while (done < keep) {
            int batch = keep - done;
            if (batch > MAX_BATCH) batch = MAX_BATCH;
            sceAudioOutOutput(s_port, buf + done * 2);
            done += batch;
        }
        return;
    }

    /* Feed input to stretcher in STRETCH_CHUNK-sized pieces */
    int done = 0;
    while (done < frames) {
        int chunk = frames - done;
        if (chunk > STRETCH_CHUNK) chunk = STRETCH_CHUNK;

        ensure_out_buf(chunk, compress);
        if (!s_out_buf) break;

        int produced = stretch_samples(s_stretcher,
                                       buf + done * 2, chunk,
                                       s_out_buf, compress);
        if (produced > 0)
            carry_append(s_out_buf, produced);
        done += chunk;
    }
}

void audio_output_set_volume(int vol_0_to_100) {
    if (vol_0_to_100 < 0)   vol_0_to_100 = 0;
    if (vol_0_to_100 > 100) vol_0_to_100 = 100;
    s_vol_scale = vol_0_to_100 / 100.0f;
    if (s_port >= 0) {
        int v = (int)(SCE_AUDIO_OUT_MAX_VOL * s_vol_scale);
        sceAudioOutSetVolume(s_port, SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO, &v);
    }
}

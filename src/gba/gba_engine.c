/* GBAVitaEX — src/gba/gba_engine.c
 * Drives gpSP's GBA engine directly, without going through libretro.
 *
 * Design:
 *   - We reuse gpSP's entire emulation core (main.c, gba_memory.c, cpu.cc,
 *     video.cc, sound.c, etc.) verbatim from vendor/gpsp/.
 *   - The libretro shim (vendor/gpsp/libretro/libretro.c) is NOT compiled.
 *     Instead, this file provides the thin platform side that libretro.c
 *     would have provided: JIT cache setup, input feeding, framebuffer access,
 *     and audio draining.
 *   - On PSVita with HAVE_DYNAREC, the ARMv7 JIT is enabled automatically.
 */

#include "gba_engine.h"

/* common.h must come first — it defines u8/u16/u32/s16/INLINE/fixed8_24
 * and then includes all other gpSP headers (gba_memory.h, main.h, sound.h,
 * savestate.h, video.h, input.h, cheats.h, cpu.h) transitively. */
#include "common.h"
#include "gpsp_config.h"  /* ROM/RAM translation cache size constants */

#include "gbavitaex.h"

#ifdef HAVE_DYNAREC
#  include <psp2/kernel/sysmem.h>
#endif

#include <psp2/power.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
   Internal state
   ────────────────────────────────────────────────────────────────────────── */

static bool s_initialised  = false;
static bool s_rom_loaded   = false;

/* gpSP exposes these globals; declare them here for the JIT setup */
#ifdef HAVE_DYNAREC
extern u8 *rom_translation_cache;
extern u8 *ram_translation_cache;
extern u8 *rom_translation_ptr;
extern u8 *ram_translation_ptr;
extern int  dynarec_enable;

static SceUID s_jit_block  = -1;

/* Allocate a single RWX memblock for both JIT caches.
 * sceKernelOpenVMDomain() makes the already-allocated memory executable. */
static bool init_jit_caches(void) {
    size_t total = ROM_TRANSLATION_CACHE_SIZE + RAM_TRANSLATION_CACHE_SIZE;
    /* Align to 1 MB boundary */
    size_t aligned = (total + 0xFFFFF) & ~0xFFFFF;

    s_jit_block = sceKernelAllocMemBlock("gpsp_jit",
                      SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, aligned, NULL);
    if (s_jit_block < 0) {
        fprintf(stderr, "[gba_engine] Failed to allocate JIT block: 0x%08X\n",
                s_jit_block);
        return false;
    }

    void *base = NULL;
    if (sceKernelGetMemBlockBase(s_jit_block, &base) < 0) {
        sceKernelFreeMemBlock(s_jit_block);
        s_jit_block = -1;
        return false;
    }

    rom_translation_cache = (u8 *)base;
    ram_translation_cache = rom_translation_cache + ROM_TRANSLATION_CACHE_SIZE;
    rom_translation_ptr   = rom_translation_cache;
    ram_translation_ptr   = ram_translation_cache;

    /* Open VM domain so the CPU can execute our JIT output.
     * On Ensō / HENkaku this should always succeed. */
    int vmd = sceKernelOpenVMDomain();
    if (vmd < 0) {
        fprintf(stderr, "[gba_engine] sceKernelOpenVMDomain failed: 0x%08X"
                        " — falling back to interpreter\n", vmd);
        sceKernelFreeMemBlock(s_jit_block);
        s_jit_block = -1;
        return false;
    }

    dynarec_enable = 1;
    return true;
}

static void free_jit_caches(void) {
    if (s_jit_block >= 0) {
        sceKernelCloseVMDomain();
        sceKernelFreeMemBlock(s_jit_block);
        s_jit_block = -1;
    }
    rom_translation_cache = NULL;
    ram_translation_cache = NULL;
}
#endif /* HAVE_DYNAREC */

/* ──────────────────────────────────────────────────────────────────────────
   Framebuffer
   gpSP stores the rendered frame in the global u16* gba_screen_pixels
   (RGB565, 240×160).  We just hand the caller that pointer.
   ────────────────────────────────────────────────────────────────────────── */
extern uint16_t *gba_screen_pixels;

/* ──────────────────────────────────────────────────────────────────────────
   Initialisation (called once)
   ────────────────────────────────────────────────────────────────────────── */
static bool one_time_init(void) {
    if (s_initialised) return true;

    /* Boost CPU clock for better emulation performance */
    scePowerSetArmClockFrequency(g_emu.cpu_clock_mhz);

    /* Allocate the pixel framebuffer that gpSP's scanline renderer writes to */
    if (!gba_screen_pixels) {
        gba_screen_pixels = (uint16_t *)malloc(GBA_SCREEN_W * GBA_SCREEN_H * sizeof(uint16_t));
        if (!gba_screen_pixels) {
            fprintf(stderr, "[gba_engine] Failed to allocate framebuffer\n");
            return false;
        }
        memset(gba_screen_pixels, 0, GBA_SCREEN_W * GBA_SCREEN_H * sizeof(uint16_t));
    }

    /* gpSP ROM page buffer */
    init_gamepak_buffer();

    /* gpSP audio (PSG + DirectSound ring buffer) */
    init_sound();

#ifdef HAVE_DYNAREC
    if (g_emu.dynarec_enabled) {
        if (!init_jit_caches()) {
            /* Fall back gracefully to interpreter */
            g_emu.dynarec_enabled = false;
            fprintf(stderr, "[gba_engine] JIT cache init failed — using interpreter\n");
        }
    }
#endif

    s_initialised = true;
    return true;
}

/* ──────────────────────────────────────────────────────────────────────────
   Public API
   ────────────────────────────────────────────────────────────────────────── */

bool gba_engine_load(const char *rom_path, const char *save_path) {
    if (!one_time_init()) return false;

    if (s_rom_loaded) gba_engine_unload();

    /* Load BIOS — try official first, fall back to built-in open-source BIOS */
    char bios_file[512];
    snprintf(bios_file, sizeof(bios_file), "%s/gba_bios.bin", BIOS_PATH);
    if (load_bios(bios_file) != 0) {
        /* Use the open-source BIOS bundled in vendor/gpsp/bios_data.S */
        extern u8 open_gba_bios_rom[16 * 1024];
        memcpy(bios_rom, open_gba_bios_rom, sizeof(bios_rom));
    }

    /* Clear backup memory before loading */
    memset(gamepak_backup, 0xFF, sizeof(gamepak_backup));

    /* Build a minimal retro_game_info-like struct that load_gamepak expects */
    struct retro_game_info info = { .path = rom_path, .data = NULL, .size = 0 };
    if (load_gamepak(&info, rom_path,
                     /* force_rtc    */ -1,   /* auto-detect */
                     /* force_rumble */ -1,
                     /* force_serial */ SERIAL_MODE_AUTO) != 0) {
        fprintf(stderr, "[gba_engine] Failed to load ROM: %s\n", rom_path);
        return false;
    }

    /* Load SRAM if it exists */
    gba_engine_load_sram(save_path);

    /* Full hardware reset */
    reset_gba();

    s_rom_loaded = true;
    return true;
}

void gba_engine_unload(void) {
    if (!s_rom_loaded) return;
#ifdef HAVE_DYNAREC
    if (dynarec_enable) flush_dynarec_caches();
#endif
    s_rom_loaded = false;
}

void gba_engine_reset(void) {
    if (!s_rom_loaded) return;
    reset_gba();
}

void gba_engine_run_frame(void) {
    if (!s_rom_loaded) return;

    /* Apply frameskip: skip_next_frame tells gpSP's video renderer to skip
     * blitting this frame, saving CPU on the pixel output path. We set it
     * from the user setting, cycling through skip counts so audio stays
     * continuous (every Nth frame is rendered, others skipped). */
    if (g_emu.frameskip > 0) {
        static int s_skip_counter = 0;
        s_skip_counter++;
        skip_next_frame = (s_skip_counter % (g_emu.frameskip + 1)) != 0 ? 1 : 0;
        if (s_skip_counter >= (g_emu.frameskip + 1)) s_skip_counter = 0;
    } else {
        skip_next_frame = 0;
    }

    /* execute_arm_translate / execute_arm run until frame_complete is set.
     * render_gbc_sound() is called internally at vblank inside update_gba()
     * (main.c line ~233), so we must NOT call it again here. */
#ifdef HAVE_DYNAREC
    if (dynarec_enable)
        execute_arm_translate(execute_cycles);
    else
#endif
    {
        clear_gamepak_stickybits();
        execute_arm(execute_cycles);
    }
}

/* Map our unified EMU_KEY_* bits to gpSP's key_status bits.
 * gpSP tracks key state via the GBA hardware REG_P1 register (active-low).
 * We write directly to the io register — the same path gpSP's input.c uses. */
void gba_engine_set_input(uint32_t emu_keys) {
    /* REG_P1: 0 = pressed (active low), bits 0-9 = A,B,Select,Start,R,L,Up,Down,Right,Left */
    uint32_t p1 = 0x3FF; /* all released */
    if (emu_keys & EMU_KEY_A)      p1 &= ~(1 << 0);
    if (emu_keys & EMU_KEY_B)      p1 &= ~(1 << 1);
    if (emu_keys & EMU_KEY_SELECT) p1 &= ~(1 << 2);
    if (emu_keys & EMU_KEY_START)  p1 &= ~(1 << 3);
    if (emu_keys & EMU_KEY_RIGHT)  p1 &= ~(1 << 4);
    if (emu_keys & EMU_KEY_LEFT)   p1 &= ~(1 << 5);
    if (emu_keys & EMU_KEY_UP)     p1 &= ~(1 << 6);
    if (emu_keys & EMU_KEY_DOWN)   p1 &= ~(1 << 7);
    if (emu_keys & EMU_KEY_R)      p1 &= ~(1 << 8);
    if (emu_keys & EMU_KEY_L)      p1 &= ~(1 << 9);
    write_ioreg(REG_P1, p1);
}

const uint16_t *gba_engine_get_framebuffer(void) {
    return gba_screen_pixels;
}

/* Drain PCM audio from gpSP's ring buffer into caller's stereo int16 buffer.
 * Returns number of stereo frames (sample pairs) written. */
int gba_engine_drain_audio(int16_t *buf, int max_frames) {
    return (int)sound_read_samples(buf, (u32)max_frames);
}

bool gba_engine_save_sram(const char *path) {
    if (!s_rom_loaded) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fwrite(gamepak_backup, 1, sizeof(gamepak_backup), f);
    fclose(f);
    return true;
}

bool gba_engine_load_sram(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fread(gamepak_backup, 1, sizeof(gamepak_backup), f);
    fclose(f);
    return true;
}

bool gba_engine_save_state(const char *path) {
    if (!s_rom_loaded) return false;
    void *buf = malloc(GBA_STATE_MEM_SIZE);
    if (!buf) return false;
    gba_save_state(buf);
    FILE *f = fopen(path, "wb");
    bool ok = false;
    if (f) {
        ok = fwrite(buf, 1, GBA_STATE_MEM_SIZE, f) == GBA_STATE_MEM_SIZE;
        fclose(f);
    }
    free(buf);
    return ok;
}

bool gba_engine_load_state(const char *path) {
    if (!s_rom_loaded) return false;
    void *buf = malloc(GBA_STATE_MEM_SIZE);
    if (!buf) return false;
    FILE *f = fopen(path, "rb");
    bool ok = false;
    if (f) {
        ok = fread(buf, 1, GBA_STATE_MEM_SIZE, f) == GBA_STATE_MEM_SIZE;
        fclose(f);
    }
    if (ok) {
        ok = gba_load_state(buf);
        /* Fix known gpSP issue: loading a save state kills audio because
         * PSG frequency step tables are not rebuilt from restored state.
         * sound_frequency_changed() recomputes all derived audio constants. */
        if (ok) sound_frequency_changed();
    }
    free(buf);
    return ok;
}

void gba_engine_shutdown(void) {
    gba_engine_unload();
    memory_term();
#ifdef HAVE_DYNAREC
    free_jit_caches();
#endif
    s_initialised = false;
}

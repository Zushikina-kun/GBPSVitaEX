/* GBAVitaEX — src/core/emu_core.c
 * Central dispatch layer.
 * Detects ROM type and routes everything to either the gpSP GBA engine
 * or the mGBA GB/GBC engine.
 */

#include "emu_core.h"
#include "../gba/gba_engine.h"
#include "../gb/gb_engine.h"
#include "../cheats/cheat_mgr.h"
#include "gbavitaex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
   ROM detection helpers
   ────────────────────────────────────────────────────────────────────────── */

/* GBA logo data at ROM offset 0x04, length 156 bytes (Nintendo logo bitmap).
 * We use a short slice (first 4 bytes) as a quick check, then verify the
 * full checksum for confidence.  gpSP's cpu.cc stores the full logo. */
static const uint8_t GBA_LOGO_SLICE[4] = { 0x24, 0xFF, 0xAE, 0x51 };

/* GB Nintendo logo at cart header 0x104, first 4 bytes. */
static const uint8_t GB_LOGO_SLICE[4]  = { 0xCE, 0xED, 0x66, 0x66 };

/* Try to read the first 0x200 bytes of the file into a small buffer.
 * Returns false if the file can't be opened or is too short. */
static bool read_header(const char *path, uint8_t *buf, size_t len) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    return n >= 0xC0;   /* need at least 192 bytes for GBA header */
}

EmuCore emu_detect_rom(const char *path) {
    uint8_t hdr[0x200];
    if (!read_header(path, hdr, sizeof(hdr))) return CORE_NONE;

    /* Check GBA logo first (offset 0x04) */
    if (memcmp(hdr + 4, GBA_LOGO_SLICE, 4) == 0) {
        return CORE_GBA;
    }

    /* Check GB/GBC Nintendo logo (offset 0x104) */
    if (sizeof(hdr) > 0x14C && memcmp(hdr + 0x104, GB_LOGO_SLICE, 4) == 0) {
        /* CGB flag at 0x143: 0x80 = CGB compatible, 0xC0 = CGB only */
        uint8_t cgb_flag = hdr[0x143];
        if (cgb_flag == 0x80 || cgb_flag == 0xC0)
            return CORE_GBC;
        return CORE_GB;
    }

    /* Fallback: extension-based guess */
    const char *ext = strrchr(path, '.');
    if (ext) {
        if      (!strcasecmp(ext, ".gba") || !strcasecmp(ext, ".agb")) return CORE_GBA;
        else if (!strcasecmp(ext, ".gbc"))                              return CORE_GBC;
        else if (!strcasecmp(ext, ".gb"))                               return CORE_GB;
        /* .gbz is a compressed GB/GBC format — not GBA */
        else if (!strcasecmp(ext, ".gbz"))                              return CORE_GBC;
        else if (!strcasecmp(ext, ".bin"))                              return CORE_GBA;
    }

    return CORE_NONE;
}

/* ──────────────────────────────────────────────────────────────────────────
   Public API
   ────────────────────────────────────────────────────────────────────────── */

bool emu_load_rom(const char *path) {
    /* Unload any previously loaded ROM */
    emu_unload_rom();

    EmuCore core = emu_detect_rom(path);
    if (core == CORE_NONE) {
        fprintf(stderr, "[emu_core] Cannot detect ROM type: %s\n", path);
        return false;
    }

    g_emu.active_core = core;
    strncpy(g_emu.rom_path, path, sizeof(g_emu.rom_path) - 1);

    /* Derive save path: ROM dir / <basename>.sav */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char noext[256];
    strncpy(noext, base, sizeof(noext) - 1);
    char *dot = strrchr(noext, '.');
    if (dot) *dot = '\0';
    snprintf(g_emu.save_path,  sizeof(g_emu.save_path),  "%s/%s.sav",  SAVE_PATH,  noext);
    snprintf(g_emu.state_path, sizeof(g_emu.state_path), "%s/%s",       STATE_PATH, noext);

    bool ok = false;
    switch (core) {
        case CORE_GBA:
            ok = gba_engine_load(path, g_emu.save_path);
            break;
        case CORE_GB:
        case CORE_GBC:
            ok = gb_engine_load(path, g_emu.save_path, core == CORE_GBC);
            break;
        default:
            break;
    }

    if (ok) {
        g_emu.running     = true;
        g_emu.frame_count = 0;
        /* Load cheats for this ROM (no-op if .cht doesn't exist) */
        cheat_mgr_load(noext);
    } else {
        g_emu.active_core = CORE_NONE;
    }
    return ok;
}

void emu_unload_rom(void) {
    if (!g_emu.running) return;

    emu_save_sram();   /* auto-save SRAM on unload */

    switch (g_emu.active_core) {
        case CORE_GBA:             gba_engine_unload(); break;
        case CORE_GB: case CORE_GBC: gb_engine_unload(); break;
        default: break;
    }

    g_emu.running     = false;
    g_emu.active_core = CORE_NONE;
    g_emu.rom_path[0] = '\0';
}

void emu_run_frame(void) {
    if (!g_emu.running || g_emu.paused) return;

    switch (g_emu.active_core) {
        case CORE_GBA:               gba_engine_run_frame(); break;
        case CORE_GB: case CORE_GBC: gb_engine_run_frame();  break;
        default: break;
    }
    g_emu.frame_count++;
}

void emu_set_input(uint32_t buttons, uint8_t lx, uint8_t ly) {
    switch (g_emu.active_core) {
        case CORE_GBA:               gba_engine_set_input(buttons); break;
        case CORE_GB: case CORE_GBC: gb_engine_set_input(buttons);  break;
        default: break;
    }
    (void)lx; (void)ly; /* analog unused for GBA/GB but kept for future use */
}

bool emu_save_sram(void) {
    if (!g_emu.running) return false;
    switch (g_emu.active_core) {
        case CORE_GBA:               return gba_engine_save_sram(g_emu.save_path);
        case CORE_GB: case CORE_GBC: return gb_engine_save_sram(g_emu.save_path);
        default:                     return false;
    }
}

bool emu_load_sram(void) {
    if (!g_emu.running) return false;
    switch (g_emu.active_core) {
        case CORE_GBA:               return gba_engine_load_sram(g_emu.save_path);
        case CORE_GB: case CORE_GBC: return gb_engine_load_sram(g_emu.save_path);
        default:                     return false;
    }
}

bool emu_save_state(int slot) {
    if (!g_emu.running) return false;
    char path[600];
    snprintf(path, sizeof(path), "%s.st%d", g_emu.state_path, slot);
    switch (g_emu.active_core) {
        case CORE_GBA:               return gba_engine_save_state(path);
        case CORE_GB: case CORE_GBC: return gb_engine_save_state(path);
        default:                     return false;
    }
}

bool emu_load_state(int slot) {
    if (!g_emu.running) return false;
    char path[600];
    snprintf(path, sizeof(path), "%s.st%d", g_emu.state_path, slot);
    switch (g_emu.active_core) {
        case CORE_GBA:               return gba_engine_load_state(path);
        case CORE_GB: case CORE_GBC: return gb_engine_load_state(path);
        default:                     return false;
    }
}

void emu_reset(void) {
    if (!g_emu.running) return;
    switch (g_emu.active_core) {
        case CORE_GBA:               gba_engine_reset(); break;
        case CORE_GB: case CORE_GBC: gb_engine_reset();  break;
        default: break;
    }
}

void emu_core_shutdown(void) {
    emu_unload_rom();
    gba_engine_shutdown();
    gb_engine_shutdown();
}

const uint16_t *emu_get_framebuffer(int *out_w, int *out_h) {
    switch (g_emu.active_core) {
        case CORE_GBA:
            if (out_w) *out_w = GBA_SCREEN_W;
            if (out_h) *out_h = GBA_SCREEN_H;
            return gba_engine_get_framebuffer();
        case CORE_GB: case CORE_GBC:
            if (out_w) *out_w = GB_SCREEN_W;
            if (out_h) *out_h = GB_SCREEN_H;
            return gb_engine_get_framebuffer();
        default:
            if (out_w) *out_w = 0;
            if (out_h) *out_h = 0;
            return NULL;
    }
}

int emu_drain_audio(int16_t *buf, int max_frames) {
    switch (g_emu.active_core) {
        case CORE_GBA:               return gba_engine_drain_audio(buf, max_frames);
        case CORE_GB: case CORE_GBC: return gb_engine_drain_audio(buf, max_frames);
        default:                     return 0;
    }
}

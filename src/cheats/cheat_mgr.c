/* GBAVitaEX — src/cheats/cheat_mgr.c
 * Top-level cheat manager — routes to engine-specific loaders.
 *
 * IMPORTANT: This file must NOT include headers from both gpSP and mGBA,
 * as their common.h files define conflicting type aliases (u8/u16/u32).
 * Engine-specific cheat loading lives in separate TUs (cheat_gba.c / cheat_gb.c).
 *
 * Cheat file format (ux0:data/GBAVitaEX/cheats/<romname>.cht):
 *   # comment
 *   GS XXXXXXXX YYYYYYYY  GameShark / AR v1-v2 (GBA)
 *   AR XXXXXXXX YYYYYYYY  Action Replay v3 (GBA)
 *   CB XXXXX YYYY         CodeBreaker (GBA)
 *   GB XXXXXXXXX          GameShark GB/GBC (9 hex digits)
 *   GG XXXX-XXXX-XXX      Game Genie GB/GBC
 */

#include "cheat_mgr.h"
#include "gbavitaex.h"
#include <stdio.h>
#include <string.h>

/* Forward declarations — implemented in engine-specific TUs */
void cheat_gba_load_file(FILE *f);
void cheat_gb_load_file(FILE *f);

bool cheat_mgr_load(const char *rom_basename) {
    char path[512];
    snprintf(path, sizeof(path),
             "ux0:data/GBAVitaEX/cheats/%s.cht", rom_basename);
    FILE *f = fopen(path, "r");
    if (!f) return false;

    switch (g_emu.active_core) {
        case CORE_GBA:               cheat_gba_load_file(f); break;
        case CORE_GB: case CORE_GBC: cheat_gb_load_file(f);  break;
        default: break;
    }

    fclose(f);
    return true;
}

void cheat_mgr_apply(void) {
    /* Both engines apply cheats automatically per-frame. Nothing to do. */
}

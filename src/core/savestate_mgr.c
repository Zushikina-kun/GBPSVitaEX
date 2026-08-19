/* GBVitaEX — src/core/savestate_mgr.c
 * Thin wrapper: slot-based save/load states through emu_core.
 */
#include "savestate_mgr.h"
#include "emu_core.h"
#include <psp2/io/stat.h>
#include <stdio.h>

bool savestate_mgr_save(int slot) {
    return emu_save_state(slot);
}

bool savestate_mgr_load(int slot) {
    return emu_load_state(slot);
}

bool savestate_mgr_has_slot(int slot) {
    extern GBVitaEXState g_emu;
    char path[600];
    snprintf(path, sizeof(path), "%s.st%d", g_emu.state_path, slot);
    SceIoStat st;
    return sceIoGetstat(path, &st) >= 0;
}

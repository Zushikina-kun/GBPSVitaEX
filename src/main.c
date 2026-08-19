/* GBAVitaEX — src/main.c
 * PSVita application entry point.
 */

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
/* sceKernelPowerTick is in processmgr.h, already included above */
#include <psp2/power.h>
#include <psp2/sysmodule.h>
#include <psp2/appmgr.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/apputil.h>
#include <psp2/io/fcntl.h>       /* sceIoMkdir */
#include <vita2d.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gbavitaex.h"
#include "platform/psp2/psp2_ctx.h"
#include "ui/ui.h"
#include "core/emu_core.h"

/* ── Newlib heap — 128 MB for ROM buffers, JIT cache, audio, textures ── */
unsigned int _newlib_heap_size_user = 128 * 1024 * 1024;

/* ── Physical button name strings ── */
const char * const g_vbtn_names[VBTN_COUNT] = {
    [VBTN_CROSS]    = "Cross",
    [VBTN_CIRCLE]   = "Circle",
    [VBTN_SQUARE]   = "Square",
    [VBTN_TRIANGLE] = "Triangle",
    [VBTN_START]    = "Start",
    [VBTN_SELECT]   = "Select",
    [VBTN_UP]       = "Up",
    [VBTN_DOWN]     = "Down",
    [VBTN_LEFT]     = "Left",
    [VBTN_RIGHT]    = "Right",
    [VBTN_L1]       = "L Trigger",
    [VBTN_R1]       = "R Trigger",
    [VBTN_L2]       = "Back-Touch L",
    [VBTN_R2]       = "Back-Touch R",
};

/* ── Global emulator state ── */
GBAVitaEXState g_emu;

/* Apply the default button mapping (GBA layout on Vita) */
void emu_reset_key_map(void) {
    for (int i = 0; i < VBTN_COUNT; i++) g_emu.key_map[i] = 0;
    g_emu.key_map[VBTN_CROSS]    = EMU_KEY_A;
    g_emu.key_map[VBTN_CIRCLE]   = EMU_KEY_B;
    g_emu.key_map[VBTN_SQUARE]   = EMU_KEY_B;     /* Square also = B by default */
    g_emu.key_map[VBTN_TRIANGLE] = 0;             /* unmapped */
    g_emu.key_map[VBTN_START]    = EMU_KEY_START;
    g_emu.key_map[VBTN_SELECT]   = EMU_KEY_SELECT;
    g_emu.key_map[VBTN_UP]       = EMU_KEY_UP;
    g_emu.key_map[VBTN_DOWN]     = EMU_KEY_DOWN;
    g_emu.key_map[VBTN_LEFT]     = EMU_KEY_LEFT;
    g_emu.key_map[VBTN_RIGHT]    = EMU_KEY_RIGHT;
    g_emu.key_map[VBTN_L1]       = EMU_KEY_L;
    g_emu.key_map[VBTN_R1]       = EMU_KEY_R;
    g_emu.key_map[VBTN_L2]       = 0;
    g_emu.key_map[VBTN_R2]       = 0;
}

static void init_global_state(void) {
    memset(&g_emu, 0, sizeof(g_emu));
    g_emu.active_core      = CORE_NONE;
    g_emu.cpu_clock_mhz    = 444;
    g_emu.dynarec_enabled  = true;
    g_emu.color_correct    = true;
    g_emu.interframe_blend = false;
    g_emu.frameskip        = 0;
    g_emu.screen_mode      = 0;
    g_emu.audio_enabled    = true;
    g_emu.audio_volume     = 80;
    g_emu.ff_speed_pct     = 200;    /* default fast-forward = 2× */
    g_emu.ff_button        = VBTN_R1; /* R trigger = fast-forward */
    emu_reset_key_map();
}

static void create_data_dirs(void) {
    sceIoMkdir("ux0:data/GBAVitaEX",             0777);
    sceIoMkdir("ux0:data/GBAVitaEX/roms",         0777);
    sceIoMkdir("ux0:data/GBAVitaEX/saves",        0777);
    sceIoMkdir("ux0:data/GBAVitaEX/states",       0777);
    sceIoMkdir("ux0:data/GBAVitaEX/screenshots",  0777);
    sceIoMkdir("ux0:data/GBAVitaEX/cheats",       0777);
}

static void load_modules(void) {
    /* Load NET module — used for future networking features (RFU, etc.).
     * Non-fatal: if unavailable the rest of the app still works fine. */
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    /* Note: HTTPS intentionally not loaded — we have no HTTPS usage. */
}

int main(void) {
    init_global_state();
    load_modules();
    create_data_dirs();

    /* Load saved settings; if absent the defaults set above are used */
    config_load();

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK,  SCE_TOUCH_SAMPLING_STATE_START);

    psp2_ctx_init();
    ui_mainloop();
    emu_core_shutdown();
    psp2_ctx_shutdown();

    sceKernelExitProcess(0);
    return 0;
}

/* GBAVitaEX — src/main.c
 * PSVita application entry point.
 * Initialises the platform (vita2d, audio, input), then hands off to
 * the UI / emulation loop.
 */

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/sysmodule.h>
#include <psp2/appmgr.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/apputil.h>
#include <psp2/io/fcntl.h>   /* sceIoMkdir */
#include <vita2d.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gbavitaex.h"
#include "platform/psp2/psp2_ctx.h"
#include "ui/ui.h"
#include "core/emu_core.h"

/* ── Newlib heap ── */
unsigned int _newlib_heap_size_user = 128 * 1024 * 1024; /* 128 MB */

/* Global emulator state */
GBAVitaEXState g_emu = {
    .active_core      = CORE_NONE,
    .running          = false,
    .paused           = false,
    .show_menu        = false,
    .fast_forward     = false,
    .fps              = 0.0f,
    .frame_count      = 0,
    .cpu_clock_mhz    = 444,
    .dynarec_enabled  = true,
    .color_correct    = true,
    .interframe_blend = false,
    .frameskip        = 0,
    .screen_mode      = 0,    /* aspect-correct by default */
    .audio_enabled    = true,
    .audio_volume     = 80,
};

/* ── Create required data directories ── */
static void create_data_dirs(void) {
    sceIoMkdir("ux0:data/GBAVitaEX",             0777);
    sceIoMkdir("ux0:data/GBAVitaEX/roms",         0777);
    sceIoMkdir("ux0:data/GBAVitaEX/saves",        0777);
    sceIoMkdir("ux0:data/GBAVitaEX/states",       0777);
    sceIoMkdir("ux0:data/GBAVitaEX/screenshots",  0777);
    sceIoMkdir("ux0:data/GBAVitaEX/cheats",       0777);
}

/* ── Load optional system modules ── */
static void load_modules(void) {
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS);
    /* Motion/gyro sampling is started on-demand when a ROM needs it,
     * not as a sysmodule (it's built into the firmware). */
}

int main(void) {
    /* ── System setup ── */
    load_modules();
    create_data_dirs();

    /* Controller sampling: extended for analog + second controller */
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK,  SCE_TOUCH_SAMPLING_STATE_START);

    /* ── Platform context (vita2d + audio thread) ── */
    psp2_ctx_init();

    /* ── UI main loop: ROM browser → emulation → menu ── */
    ui_mainloop();

    /* ── Teardown ── */
    emu_core_shutdown();
    psp2_ctx_shutdown();

    sceKernelExitProcess(0);
    return 0;
}

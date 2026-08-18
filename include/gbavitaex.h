/* GBAVitaEX — unified GBA/GBC/GB emulator for PlayStation Vita
 * Master header: shared types, constants, and forward declarations.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ──────────────────────────────────────────────
   Versioning
   ────────────────────────────────────────────── */
#define GBAVITAEX_VER_MAJOR 1
#define GBAVITAEX_VER_MINOR 0
#define GBAVITAEX_VER_PATCH 0

/* ──────────────────────────────────────────────
   Path constants  (overridable at cmake time)
   ────────────────────────────────────────────── */
#ifndef SAVE_PATH
#define SAVE_PATH       "ux0:data/GBAVitaEX/saves"
#endif
#ifndef STATE_PATH
#define STATE_PATH      "ux0:data/GBAVitaEX/states"
#endif
#ifndef BIOS_PATH
#define BIOS_PATH       "ux0:data/GBAVitaEX"
#endif
#ifndef ROM_PATH
#define ROM_PATH        "ux0:data/GBAVitaEX/roms"
#endif
#ifndef SCREENSHOT_PATH
#define SCREENSHOT_PATH "ux0:data/GBAVitaEX/screenshots"
#endif

/* ──────────────────────────────────────────────
   Screen geometry
   ────────────────────────────────────────────── */
#define VITA_SCREEN_W   960
#define VITA_SCREEN_H   544

#define GBA_SCREEN_W    240
#define GBA_SCREEN_H    160

#define GB_SCREEN_W     160
#define GB_SCREEN_H     144

/* ──────────────────────────────────────────────
   ROM type / active core
   ────────────────────────────────────────────── */
typedef enum {
    CORE_NONE = 0,
    CORE_GBA,   /* gpSP ARMv7 dynarec */
    CORE_GB,    /* mGBA SM83 interpreter */
    CORE_GBC,   /* mGBA SM83 interpreter (CGB mode) */
} EmuCore;

/* ──────────────────────────────────────────────
   Emulator state (shared between modules)
   ────────────────────────────────────────────── */
typedef struct GBAVitaEXState {
    EmuCore   active_core;
    char      rom_path[512];
    char      save_path[512];
    char      state_path[512];

    bool      running;        /* emulation loop active */
    bool      paused;         /* user-requested pause */
    bool      show_menu;      /* in-game menu open */
    bool      fast_forward;   /* 2× speed */

    /* Per-frame stats */
    float     fps;
    uint32_t  frame_count;

    /* Settings */
    int       cpu_clock_mhz;  /* 333/444/500 */
    bool      dynarec_enabled;
    bool      color_correct;  /* GBA LCD gamma correction */
    bool      interframe_blend;
    int       frameskip;      /* 0 = auto */
    int       screen_mode;    /* 0=aspect 1=fullscreen 2=integer-scale */
    bool      audio_enabled;
    int       audio_volume;   /* 0–100 */
} GBAVitaEXState;

extern GBAVitaEXState g_emu;

/* ──────────────────────────────────────────────
   Input key bits  (GBA/GB share same layout)
   ────────────────────────────────────────────── */
#define EMU_KEY_A       (1 << 0)
#define EMU_KEY_B       (1 << 1)
#define EMU_KEY_SELECT  (1 << 2)
#define EMU_KEY_START   (1 << 3)
#define EMU_KEY_RIGHT   (1 << 4)
#define EMU_KEY_LEFT    (1 << 5)
#define EMU_KEY_UP      (1 << 6)
#define EMU_KEY_DOWN    (1 << 7)
#define EMU_KEY_R       (1 << 8)
#define EMU_KEY_L       (1 << 9)

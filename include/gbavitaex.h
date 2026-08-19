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
#define GBAVITAEX_VER_MAJOR  1
#define GBAVITAEX_VER_MINOR  4
#define GBAVITAEX_VER_PATCH  0
#define GBAVITAEX_VERSION_STR "1.4.0"

/* ──────────────────────────────────────────────
   Path constants  (overridable at cmake time)
   ────────────────────────────────────────────── */
#ifndef SAVE_PATH
#define SAVE_PATH        "ux0:data/GBAVitaEX/saves"
#endif
#ifndef STATE_PATH
#define STATE_PATH       "ux0:data/GBAVitaEX/states"
#endif
#ifndef BIOS_PATH
#define BIOS_PATH        "ux0:data/GBAVitaEX"
#endif
#ifndef ROM_PATH
#define ROM_PATH         "ux0:data/GBAVitaEX/roms"
#endif
#ifndef SCREENSHOT_PATH
#define SCREENSHOT_PATH  "ux0:data/GBAVitaEX/screenshots"
#endif
#define CONFIG_PATH      "ux0:data/GBAVitaEX/config.ini"

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
    CORE_GBA,    /* gpSP ARMv7 dynarec */
    CORE_GB,     /* mGBA SM83 interpreter */
    CORE_GBC,    /* mGBA SM83 interpreter (CGB mode) */
} EmuCore;

/* ──────────────────────────────────────────────
   EMU_KEY_* — unified active-high button bitmask
   These bits are what every module uses internally.
   The mapping from SCE_CTRL_* to these is in psp2_ctx.c,
   and is fully user-remappable via g_emu.key_map[].
   ────────────────────────────────────────────── */
#define EMU_KEY_A       (1u <<  0)
#define EMU_KEY_B       (1u <<  1)
#define EMU_KEY_SELECT  (1u <<  2)
#define EMU_KEY_START   (1u <<  3)
#define EMU_KEY_RIGHT   (1u <<  4)
#define EMU_KEY_LEFT    (1u <<  5)
#define EMU_KEY_UP      (1u <<  6)
#define EMU_KEY_DOWN    (1u <<  7)
#define EMU_KEY_R       (1u <<  8)
#define EMU_KEY_L       (1u <<  9)
#define EMU_KEY_COUNT   10

/* Physical Vita buttons — used as indices into key_map[] */
typedef enum {
    VBTN_CROSS = 0,
    VBTN_CIRCLE,
    VBTN_SQUARE,
    VBTN_TRIANGLE,
    VBTN_START,
    VBTN_SELECT,
    VBTN_UP,
    VBTN_DOWN,
    VBTN_LEFT,
    VBTN_RIGHT,
    VBTN_L1,
    VBTN_R1,
    VBTN_L2,        /* back-touch left  (mapped via touch) */
    VBTN_R2,        /* back-touch right (mapped via touch) */
    VBTN_COUNT,
} VitaButton;

/* String names for display in settings screen */
extern const char * const g_vbtn_names[VBTN_COUNT];

/* ──────────────────────────────────────────────
   Fast-forward speeds
   ────────────────────────────────────────────── */
/* Multiplier is stored as a percentage: 100=1x 150=1.5x 200=2x etc. */
#define FF_SPEED_MIN   125   /* 1.25× */
#define FF_SPEED_MAX   800   /* 8×    */
#define FF_SPEED_STEP   25

/* ──────────────────────────────────────────────
   Emulator state (shared between modules)
   ────────────────────────────────────────────── */
typedef struct GBAVitaEXState {
    EmuCore   active_core;
    char      rom_path[512];
    char      save_path[512];
    char      state_path[512];

    bool      running;          /* emulation loop active */
    bool      paused;           /* user-requested pause */
    bool      show_menu;        /* in-game menu open */
    bool      fast_forward;     /* fast-forward active this frame */

    /* Per-frame stats */
    float     fps;
    uint32_t  frame_count;

    /* ── Display settings ── */
    int       screen_mode;      /* ScreenMode enum value */
    bool      color_correct;    /* GBA LCD gamma LUT */
    bool      interframe_blend;

    /* ── Performance settings ── */
    int       cpu_clock_mhz;    /* 333 / 444 / 500 */
    bool      dynarec_enabled;
    int       frameskip;        /* 0=auto, 1-5=fixed */

    /* ── Fast-forward ── */
    int       ff_speed_pct;     /* 125–800, step 25 (percentage of normal) */
    int       ff_button;        /* VitaButton index, -1=disabled */
    bool      ff_pitch_correct; /* true = TDHS pitch preservation during FF */

    /* ── Audio settings ── */
    bool      audio_enabled;
    int       audio_volume;     /* 0–100 */

    /* ── Button remapping ──
     * key_map[VitaButton] = EMU_KEY_* bitmask (0 = unmapped).
     * Multiple Vita buttons can map to the same EMU_KEY, and a single
     * Vita button can map to multiple EMU_KEY bits (OR'd together). */
    uint32_t  key_map[VBTN_COUNT];
} GBAVitaEXState;

extern GBAVitaEXState g_emu;

/* Default key map — applied when no config file exists */
void emu_reset_key_map(void);

/* Config persistence */
bool config_save(void);
bool config_load(void);

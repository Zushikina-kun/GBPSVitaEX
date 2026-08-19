/* GBAVitaEX — src/ui/settings.c
 * Settings and key-remapper screens.
 */

#include "settings.h"
#include "gbavitaex.h"
#include "../platform/psp2/psp2_ctx.h"
#include <psp2/ctrl.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
   SETTINGS SCREEN
   ────────────────────────────────────────────────────────────────────────── */

typedef struct {
    const char *label;
    int        *int_val;    /* NULL if bool */
    bool       *bool_val;   /* NULL if int  */
    int         min, max, step;
} Setting;

static const char *screen_mode_names[SCREEN_MODE_MAX] = {
    [SCREEN_MODE_ASPECT]  = "Aspect Ratio",
    [SCREEN_MODE_FULL]    = "Fullscreen Stretch",
    [SCREEN_MODE_INTEGER] = "Integer Scale",
};

/* Clock options: 333/444/500 — we cycle through these */
static const int clock_opts[] = {333, 444, 500};
#define CLOCK_OPT_COUNT 3

static int clock_idx_for(int mhz) {
    for (int i = 0; i < CLOCK_OPT_COUNT; i++)
        if (clock_opts[i] == mhz) return i;
    return 1; /* default 444 */
}

/* FF button options */
static const int ff_btn_opts[]   = { -1, VBTN_R1, VBTN_L2, VBTN_R2, VBTN_TRIANGLE };
static const char *ff_btn_names[]= { "Off", "R Trigger", "Back-L", "Back-R", "Triangle" };
#define FF_BTN_OPT_COUNT 5

static int ff_btn_idx_for(int btn) {
    for (int i = 0; i < FF_BTN_OPT_COUNT; i++)
        if (ff_btn_opts[i] == btn) return i;
    return 1;
}

enum SettingID {
    S_CPU_CLOCK = 0,
    S_SCREEN_MODE,
    S_VOLUME,
    S_FRAMESKIP,
    S_FF_SPEED,
    S_FF_BUTTON,
    S_FF_PITCH,      /* new: pitch correction toggle */
    S_COLOR_CORRECT,
    S_INTERFRAME,
    S_AUDIO,
    S_DYNAREC,
    S_KEYMAPPER,
    S_COUNT
};

static const char *setting_labels[S_COUNT] = {
    [S_CPU_CLOCK]    = "CPU Clock",
    [S_SCREEN_MODE]  = "Screen Mode",
    [S_VOLUME]       = "Volume",
    [S_FRAMESKIP]    = "Frameskip",
    [S_FF_SPEED]     = "Fast-Forward Speed",
    [S_FF_BUTTON]    = "Fast-Forward Button",
    [S_FF_PITCH]     = "FF Pitch Correction",
    [S_COLOR_CORRECT]= "GBA Colour Correction",
    [S_INTERFRAME]   = "Interframe Blending",
    [S_AUDIO]        = "Audio",
    [S_DYNAREC]      = "JIT Dynarec (GBA)",
    [S_KEYMAPPER]    = "Button Remapping  >",
};

static int  s_cursor     = 0;
static bool s_want_kmap  = false;
static uint32_t s_prev   = 0;
#define PRESSED(btn) ((buttons & (btn)) && !(s_prev & (btn)))

static void get_value_str(int id, char *buf, size_t sz) {
    switch (id) {
    case S_CPU_CLOCK:
        snprintf(buf, sz, "%d MHz", g_emu.cpu_clock_mhz); break;
    case S_SCREEN_MODE:
        snprintf(buf, sz, "%s", screen_mode_names[g_emu.screen_mode % SCREEN_MODE_MAX]); break;
    case S_VOLUME:
        snprintf(buf, sz, "%d%%", g_emu.audio_volume); break;
    case S_FRAMESKIP:
        if (g_emu.frameskip == 0) snprintf(buf, sz, "Off");
        else snprintf(buf, sz, "%d frame%s", g_emu.frameskip, g_emu.frameskip>1?"s":"");
        break;
    case S_FF_SPEED:
        snprintf(buf, sz, "%.2gx  (%d%%)", g_emu.ff_speed_pct/100.0, g_emu.ff_speed_pct); break;
    case S_FF_BUTTON:
        snprintf(buf, sz, "%s", ff_btn_names[ff_btn_idx_for(g_emu.ff_button)]); break;
    case S_FF_PITCH:
        snprintf(buf, sz, g_emu.ff_pitch_correct ? "On" : "Off"); break;
    case S_COLOR_CORRECT:
        snprintf(buf, sz, g_emu.color_correct    ? "On" : "Off"); break;
    case S_INTERFRAME:
        snprintf(buf, sz, g_emu.interframe_blend ? "On" : "Off"); break;
    case S_AUDIO:
        snprintf(buf, sz, g_emu.audio_enabled    ? "On" : "Off"); break;
    case S_DYNAREC:
        snprintf(buf, sz, g_emu.dynarec_enabled  ? "On" : "Off"); break;
    case S_KEYMAPPER:
        buf[0] = '\0'; break;
    default:
        buf[0] = '\0'; break;
    }
}

bool settings_wants_keymapper(void) {
    bool w = s_want_kmap;
    s_want_kmap = false;
    return w;
}

bool settings_update(uint32_t buttons) {
    bool done = false;
    s_want_kmap = false;

    if (PRESSED(EMU_KEY_UP))   { if (s_cursor > 0) s_cursor--; }
    if (PRESSED(EMU_KEY_DOWN)) { if (s_cursor < S_COUNT-1) s_cursor++; }

    bool left  = PRESSED(EMU_KEY_LEFT);
    bool right = PRESSED(EMU_KEY_RIGHT);
    bool enter = PRESSED(EMU_KEY_A);

    switch (s_cursor) {
    case S_CPU_CLOCK: {
        int idx = clock_idx_for(g_emu.cpu_clock_mhz);
        if (right && idx < CLOCK_OPT_COUNT-1) g_emu.cpu_clock_mhz = clock_opts[++idx];
        if (left  && idx > 0)                 g_emu.cpu_clock_mhz = clock_opts[--idx];
        break; }
    case S_SCREEN_MODE:
        if (right) g_emu.screen_mode = (g_emu.screen_mode+1) % SCREEN_MODE_MAX;
        if (left)  g_emu.screen_mode = (g_emu.screen_mode+SCREEN_MODE_MAX-1) % SCREEN_MODE_MAX;
        break;
    case S_VOLUME:
        if (right && g_emu.audio_volume < 100) g_emu.audio_volume += 5;
        if (left  && g_emu.audio_volume > 0)   g_emu.audio_volume -= 5;
        break;
    case S_FRAMESKIP:
        if (right && g_emu.frameskip < 5) g_emu.frameskip++;
        if (left  && g_emu.frameskip > 0) g_emu.frameskip--;
        break;
    case S_FF_SPEED:
        if (right && g_emu.ff_speed_pct < FF_SPEED_MAX) g_emu.ff_speed_pct += FF_SPEED_STEP;
        if (left  && g_emu.ff_speed_pct > FF_SPEED_MIN) g_emu.ff_speed_pct -= FF_SPEED_STEP;
        break;
    case S_FF_BUTTON: {
        int idx = ff_btn_idx_for(g_emu.ff_button);
        if (right && idx < FF_BTN_OPT_COUNT-1) g_emu.ff_button = ff_btn_opts[++idx];
        if (left  && idx > 0)                  g_emu.ff_button = ff_btn_opts[--idx];
        break; }
    case S_FF_PITCH:
        if (left || right || enter) g_emu.ff_pitch_correct = !g_emu.ff_pitch_correct; break;
    case S_COLOR_CORRECT:
        if (left || right || enter) g_emu.color_correct = !g_emu.color_correct; break;
    case S_INTERFRAME:
        if (left || right || enter) g_emu.interframe_blend = !g_emu.interframe_blend; break;
    case S_AUDIO:
        if (left || right || enter) g_emu.audio_enabled = !g_emu.audio_enabled; break;
    case S_DYNAREC:
        if (left || right || enter) g_emu.dynarec_enabled = !g_emu.dynarec_enabled; break;
    case S_KEYMAPPER:
        if (enter) { s_want_kmap = true; } break;
    }

    if (PRESSED(EMU_KEY_B)) done = true;
    s_prev = buttons;
    return done;
}

void settings_draw(void) {
    vita2d_draw_rectangle(0, 0, VITA_SCREEN_W, VITA_SCREEN_H, RGBA8(0,0,0,210));
    vita2d_pgf *font = g_pgf_font;
    if (!font) return;

    vita2d_pgf_draw_text(font, 20, 30, RGBA8(255,215,0,255), 1.2f,
                         "Settings  — GBAVitaEX " GBAVITAEX_VERSION_STR);
    vita2d_draw_line(20, 46, VITA_SCREEN_W-20, 46, RGBA8(80,80,80,255));

    const int ROW = 37;
    for (int i = 0; i < S_COUNT; i++) {
        int y = 58 + i * ROW;
        unsigned bg  = (i == s_cursor) ? RGBA8(60,120,200,160) : 0;
        unsigned col = (i == s_cursor) ? RGBA8(255,255,255,255) : RGBA8(200,200,200,200);
        if (i == s_cursor)
            vita2d_draw_rectangle(18, y-3, VITA_SCREEN_W-36, ROW-2, bg);
        vita2d_pgf_draw_text(font, 30, y+22, col, 0.95f, setting_labels[i]);

        char val[64]; get_value_str(i, val, sizeof(val));
        if (val[0])
            vita2d_pgf_draw_text(font, VITA_SCREEN_W-220, y+22, col, 0.95f, val);
    }

    /* Volume slider bar */
    int vy = 58 + S_VOLUME*ROW;
    vita2d_draw_rectangle(VITA_SCREEN_W-220, vy+6, 200, 10, RGBA8(50,50,50,200));
    vita2d_draw_rectangle(VITA_SCREEN_W-220, vy+6, (int)(200*g_emu.audio_volume/100.0f), 10,
                          RGBA8(100,200,100,255));

    /* FF speed slider bar */
    int fv = 58 + S_FF_SPEED*ROW;
    int ff_range  = FF_SPEED_MAX - FF_SPEED_MIN;
    int ff_offset = g_emu.ff_speed_pct - FF_SPEED_MIN;
    vita2d_draw_rectangle(VITA_SCREEN_W-220, fv+6, 200, 10, RGBA8(50,50,50,200));
    vita2d_draw_rectangle(VITA_SCREEN_W-220, fv+6, (int)(200.0f*ff_offset/ff_range), 10,
                          RGBA8(255,160,30,255));

    vita2d_pgf_draw_text(font, 20, VITA_SCREEN_H-20,
                         RGBA8(150,150,150,255), 0.75f,
                         "[Up/Down]=navigate  [Left/Right]=change  [Cross]=select  [Circle]=back");
}

/* ──────────────────────────────────────────────────────────────────────────
   BUTTON REMAPPER
   ────────────────────────────────────────────────────────────────────────── */

/* EMU key names */
static const char *emu_key_names[EMU_KEY_COUNT] = {
    "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L"
};

/* Maps a raw SCE_CTRL_* sample to which VBTN index was just pressed. -1 if none. */
static int detect_new_vbtn(uint32_t raw_now, uint32_t raw_prev) {
    static const uint32_t btn_masks[VBTN_COUNT] = {
        SCE_CTRL_CROSS, SCE_CTRL_CIRCLE, SCE_CTRL_SQUARE, SCE_CTRL_TRIANGLE,
        SCE_CTRL_START, SCE_CTRL_SELECT,
        SCE_CTRL_UP, SCE_CTRL_DOWN, SCE_CTRL_LEFT, SCE_CTRL_RIGHT,
        SCE_CTRL_LTRIGGER, SCE_CTRL_RTRIGGER, 0, 0
    };
    for (int i = 0; i < VBTN_COUNT; i++) {
        if (btn_masks[i] && (raw_now & btn_masks[i]) && !(raw_prev & btn_masks[i]))
            return i;
    }
    return -1;
}

static int  km_cursor    = 0;  /* which VitaButton we are editing */
static int  km_emu_bit   = 0;  /* which EMU_KEY_* bit we are assigning (cycle with Left/Right) */
static bool km_waiting   = false; /* true = waiting for a button press to remap */
static uint32_t km_prev  = 0;

bool keymapper_update(uint32_t raw_sce) {
    bool done = false;

    /* While waiting for a button press to assign */
    if (km_waiting) {
        int new_vbtn = detect_new_vbtn(raw_sce, km_prev);
        km_prev = raw_sce;
        if (new_vbtn >= 0) {
            /* Assign the selected EMU_KEY bit to the pressed Vita button */
            uint32_t bit = (uint32_t)(1u << km_emu_bit);
            /* Remove this bit from any previous assignment */
            for (int i = 0; i < VBTN_COUNT; i++) g_emu.key_map[i] &= ~bit;
            g_emu.key_map[new_vbtn] |= bit;
            km_waiting = false;
        }
        return false;
    }

    /* Normal navigation */
    uint32_t prev = km_prev;
    km_prev = raw_sce;
#define KM_PRESSED(mask) ((raw_sce & (mask)) && !(prev & (mask)))
    if (KM_PRESSED(SCE_CTRL_UP))    { if (km_cursor > 0) km_cursor--; }
    if (KM_PRESSED(SCE_CTRL_DOWN))  { if (km_cursor < EMU_KEY_COUNT-1) km_cursor++; }
    if (KM_PRESSED(SCE_CTRL_LEFT))  { if (km_emu_bit == km_cursor) km_emu_bit = km_cursor; }
    if (KM_PRESSED(SCE_CTRL_RIGHT)) { /* reserved for future multi-bit */ }
    if (KM_PRESSED(SCE_CTRL_CROSS)) {
        km_emu_bit = km_cursor;
        km_waiting = true;
        km_prev    = raw_sce;  /* don't re-trigger on same frame */
    }
    if (KM_PRESSED(SCE_CTRL_TRIANGLE)) {
        /* Reset current EMU_KEY to default by restoring from a scratch */
        GBAVitaEXState tmp; memset(&tmp, 0, sizeof(tmp));
        /* We can't call emu_reset_key_map() without clobbering everything.
         * Just clear all bindings for this key and let the user re-bind. */
        uint32_t bit = (uint32_t)(1u << km_cursor);
        for (int i = 0; i < VBTN_COUNT; i++) g_emu.key_map[i] &= ~bit;
        (void)tmp;
    }
    if (KM_PRESSED(SCE_CTRL_SQUARE)) {
        /* Reset ALL to defaults */
        emu_reset_key_map();
    }
    if (KM_PRESSED(SCE_CTRL_CIRCLE)) done = true;
#undef KM_PRESSED

    return done;
}

void keymapper_draw(void) {
    vita2d_draw_rectangle(0, 0, VITA_SCREEN_W, VITA_SCREEN_H, RGBA8(0,0,0,220));
    vita2d_pgf *font = g_pgf_font;
    if (!font) return;

    vita2d_pgf_draw_text(font, 20, 30, RGBA8(255,215,0,255), 1.2f,
                         "Button Remapping");
    vita2d_draw_line(20, 46, VITA_SCREEN_W-20, 46, RGBA8(80,80,80,255));

    if (km_waiting) {
        vita2d_pgf_draw_text(font, 80, 200, RGBA8(255,200,0,255), 1.3f,
                             "Press the Vita button to assign to:");
        char label[64];
        snprintf(label, sizeof(label), "\"%s\"", emu_key_names[km_emu_bit]);
        vita2d_pgf_draw_text(font, 260, 260, RGBA8(255,255,255,255), 1.5f, label);
        vita2d_pgf_draw_text(font, 100, 340, RGBA8(150,150,150,255), 0.9f,
                             "Press any Vita button — or wait and press Circle to cancel");
        return;
    }

    const int ROW = 40;
    for (int i = 0; i < EMU_KEY_COUNT; i++) {
        int y = 58 + i * ROW;
        bool sel = (i == km_cursor);
        if (sel)
            vita2d_draw_rectangle(18, y-3, VITA_SCREEN_W-36, ROW-2, RGBA8(60,120,200,160));

        unsigned col = sel ? RGBA8(255,255,255,255) : RGBA8(200,200,200,200);
        vita2d_pgf_draw_textf(font, 30, y+22, col, 0.95f,
                              "GBA %-7s", emu_key_names[i]);

        /* Show which Vita buttons map to this emu key */
        char binds[128] = "(none)";
        bool first = true;
        uint32_t bit = (1u << i);
        for (int j = 0; j < VBTN_COUNT; j++) {
            if (g_emu.key_map[j] & bit) {
                if (!first) strncat(binds, " + ", sizeof(binds)-strlen(binds)-1);
                strncat(binds, g_vbtn_names[j], sizeof(binds)-strlen(binds)-1);
                first = false;
            }
        }
        vita2d_pgf_draw_text(font, 200, y+22, col, 0.9f, binds);
    }

    vita2d_pgf_draw_text(font, 20, VITA_SCREEN_H-40,
                         RGBA8(150,150,150,255), 0.75f,
                         "[Up/Down]=select key  [Cross]=remap  [Triangle]=clear  [Square]=reset all");
    vita2d_pgf_draw_text(font, 20, VITA_SCREEN_H-20,
                         RGBA8(150,150,150,255), 0.75f,
                         "[Circle]=back");
}

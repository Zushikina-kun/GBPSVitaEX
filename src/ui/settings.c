/* GBAVitaEX — src/ui/settings.c
 * In-game settings screen.
 */

#include "settings.h"
#include "gbavitaex.h"
#include "../platform/psp2/psp2_ctx.h"  /* ScreenMode, SCREEN_MODE_MAX */
#include <vita2d.h>
#include <stdio.h>

typedef struct {
    const char *label;
    int        *value;        /* NULL if this is an on/off bool cast */
    bool       *flag;
    int         min, max, step;
    const char *fmt;          /* printf format for value display */
    /* For flags: shown as On/Off */
} Setting;

static int s_cursor  = 0;
static uint32_t s_prev = 0;

#define PRESSED(btn) ((buttons & (btn)) && !(s_prev & (btn)))

/* Setting entries */
static const Setting SETTINGS[] = {
    { "CPU Clock (MHz)",    &g_emu.cpu_clock_mhz,    NULL, 333, 500, 111, "%d MHz", },
    { "Screen Mode",        &g_emu.screen_mode,       NULL, 0, SCREEN_MODE_MAX-1, 1, NULL },
    { "Volume",             &g_emu.audio_volume,      NULL, 0, 100, 10, "%d%%" },
    { "Frameskip",          &g_emu.frameskip,         NULL, 0, 5, 1, "%d (0=auto)" },
    { "Colour Correction",  NULL, &g_emu.color_correct,    0,0,0, NULL },
    { "Interframe Blend",   NULL, &g_emu.interframe_blend, 0,0,0, NULL },
    { "Audio",              NULL, &g_emu.audio_enabled,     0,0,0, NULL },
    { "Dynarec (GBA)",      NULL, &g_emu.dynarec_enabled,   0,0,0, NULL },
};
#define SETTINGS_COUNT ((int)(sizeof(SETTINGS)/sizeof(SETTINGS[0])))

static const char *screen_mode_names[SCREEN_MODE_MAX] = {
    [SCREEN_MODE_ASPECT]  = "Aspect",
    [SCREEN_MODE_FULL]    = "Fullscreen",
    [SCREEN_MODE_INTEGER] = "Integer",
};

bool settings_update(uint32_t buttons) {
    bool done = false;
    if (PRESSED(EMU_KEY_UP))   { if (s_cursor > 0) s_cursor--; }
    if (PRESSED(EMU_KEY_DOWN)) { if (s_cursor < SETTINGS_COUNT - 1) s_cursor++; }

    const Setting *s = &SETTINGS[s_cursor];
    if (s->flag) {
        /* Boolean toggle */
        if (PRESSED(EMU_KEY_LEFT) || PRESSED(EMU_KEY_RIGHT) || PRESSED(EMU_KEY_A))
            *s->flag = !(*s->flag);
    } else if (s->value) {
        if (PRESSED(EMU_KEY_RIGHT)) {
            *s->value += s->step;
            if (*s->value > s->max) *s->value = s->max;
        }
        if (PRESSED(EMU_KEY_LEFT)) {
            *s->value -= s->step;
            if (*s->value < s->min) *s->value = s->min;
        }
    }
    if (PRESSED(EMU_KEY_B)) done = true;

    s_prev = buttons;
    return done;
}

void settings_draw(void) {
    vita2d_draw_rectangle(0, 0, VITA_SCREEN_W, VITA_SCREEN_H, RGBA8(0, 0, 0, 200));
    vita2d_pgf *font = g_pgf_font;
    if (!font) return;

    vita2d_pgf_draw_text(font, 20, 30, RGBA8(255, 215, 0, 255), 1.2f, "Settings");
    vita2d_draw_line(20, 45, VITA_SCREEN_W - 20, 45, RGBA8(80,80,80,255));

    for (int i = 0; i < SETTINGS_COUNT; i++) {
        const Setting *s = &SETTINGS[i];
        int y = 65 + i * 40;
        unsigned lc = (i == s_cursor) ? RGBA8(255,255,255,255) : RGBA8(200,200,200,200);

        if (i == s_cursor)
            vita2d_draw_rectangle(18, y - 4, VITA_SCREEN_W - 36, 34,
                                  RGBA8(60, 120, 200, 150));

        vita2d_pgf_draw_text(font, 30, y + 22, lc, 1.0f, s->label);

        /* Value string */
        char val[64] = "";
        if (s->flag) {
            snprintf(val, sizeof(val), *s->flag ? "On" : "Off");
        } else if (s->value) {
            if (s->value == &g_emu.screen_mode)
                snprintf(val, sizeof(val), "%s",
                         screen_mode_names[*s->value % SCREEN_MODE_MAX]);
            else if (s->fmt)
                snprintf(val, sizeof(val), s->fmt, *s->value);
            else
                snprintf(val, sizeof(val), "%d", *s->value);
        }
        vita2d_pgf_draw_text(font, VITA_SCREEN_W - 200, y + 22, lc, 1.0f, val);
    }

    vita2d_pgf_draw_text(font, 20, VITA_SCREEN_H - 20,
                         RGBA8(150,150,150,255), 0.75f,
                         "[Up/Down]=navigate  [Left/Right]=change  [Circle]=back");
}

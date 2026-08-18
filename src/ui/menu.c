/* GBAVitaEX — src/ui/menu.c
 * Pause / in-game menu overlay with save state slot selection.
 */

#include "menu.h"
#include "gbavitaex.h"
#include "../core/emu_core.h"
#include "../core/savestate_mgr.h"
#include "../platform/psp2/psp2_ctx.h"   /* g_pgf_font */

#include <vita2d.h>
#include <stdio.h>
#include <string.h>

/* ── Menu items ── */
typedef struct { const char *label; MenuAction action; } MenuItem;
static const MenuItem ITEMS[] = {
    { "Resume",           MENU_ACTION_RESUME      },
    { "Save State",       MENU_ACTION_SAVE_STATE  },
    { "Load State",       MENU_ACTION_LOAD_STATE  },
    { "Reset Game",       MENU_ACTION_RESET       },
    { "Change ROM",       MENU_ACTION_LOAD_ROM    },
    { "Settings",         MENU_ACTION_SETTINGS    },
    { "Screenshot",       MENU_ACTION_SCREENSHOT  },
    { "Exit",             MENU_ACTION_EXIT        },
};
#define ITEM_COUNT ((int)(sizeof(ITEMS)/sizeof(ITEMS[0])))

static int s_cursor    = 0;
static int s_slot      = 0;   /* active save state slot 0–9 */
static uint32_t s_prev = 0;

#define PRESSED(btn) ((buttons & (btn)) && !(s_prev & (btn)))

MenuAction menu_update(uint32_t buttons) {
    MenuAction result = MENU_ACTION_NONE;

    if (PRESSED(EMU_KEY_UP))   { if (s_cursor > 0) s_cursor--; }
    if (PRESSED(EMU_KEY_DOWN)) { if (s_cursor < ITEM_COUNT - 1) s_cursor++; }

    /* Slot selection with L/R */
    if (PRESSED(EMU_KEY_L)) { if (s_slot > 0) s_slot--; }
    if (PRESSED(EMU_KEY_R)) { if (s_slot < 9) s_slot++; }

    if (PRESSED(EMU_KEY_A)) {
        MenuAction a = ITEMS[s_cursor].action;
        switch (a) {
        case MENU_ACTION_SAVE_STATE:
            savestate_mgr_save(s_slot);
            result = MENU_ACTION_NONE;   /* stay in menu */
            break;
        case MENU_ACTION_LOAD_STATE:
            if (savestate_mgr_has_slot(s_slot)) {
                savestate_mgr_load(s_slot);
                result = MENU_ACTION_RESUME;
            }
            break;
        case MENU_ACTION_SCREENSHOT: {
            char path[512];
            snprintf(path, sizeof(path), "%s/shot%04d.png",
                     SCREENSHOT_PATH, (int)g_emu.frame_count);
            psp2_ctx_screenshot(path);
            result = MENU_ACTION_NONE;
            break;
        }
        default:
            result = a;
            break;
        }
    }
    if (PRESSED(EMU_KEY_B)) result = MENU_ACTION_RESUME;

    s_prev = buttons;
    return result;
}

void menu_draw(void) {
    /* Semi-transparent overlay */
    vita2d_draw_rectangle(0, 0, VITA_SCREEN_W, VITA_SCREEN_H,
                          RGBA8(0, 0, 0, 160));

    vita2d_pgf *font = g_pgf_font;
    if (!font) return;

    /* Title */
    vita2d_pgf_draw_text(font, 20, 30, RGBA8(255, 215, 0, 255), 1.2f, "GBAVitaEX");

    /* ROM name */
    const char *base = strrchr(g_emu.rom_path, '/');
    base = base ? base + 1 : g_emu.rom_path;
    vita2d_pgf_draw_text(font, 20, 58, RGBA8(180, 180, 180, 255), 0.85f, base);

    /* FPS */
    char fps_str[32];
    snprintf(fps_str, sizeof(fps_str), "%.1f fps", g_emu.fps);
    vita2d_pgf_draw_text(font, VITA_SCREEN_W - 130, 30,
                         RGBA8(200, 255, 200, 255), 0.9f, fps_str);

    vita2d_draw_line(20, 70, VITA_SCREEN_W - 20, 70, RGBA8(80, 80, 80, 255));

    /* Menu items */
    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = 90 + i * 40;
        if (i == s_cursor) {
            vita2d_draw_rectangle(18, y - 4, 380, 34, RGBA8(60, 120, 200, 180));
            vita2d_pgf_draw_text(font, 30, y + 22,
                                 RGBA8(255, 255, 255, 255), 1.0f, ITEMS[i].label);
        } else {
            vita2d_pgf_draw_text(font, 30, y + 22,
                                 RGBA8(200, 200, 200, 200), 1.0f, ITEMS[i].label);
        }
    }

    /* Save state slot indicator */
    char slot_str[64];
    snprintf(slot_str, sizeof(slot_str), "State slot: %d %s",
             s_slot, savestate_mgr_has_slot(s_slot) ? "(exists)" : "(empty)");
    vita2d_pgf_draw_text(font, 450, 90, RGBA8(200, 200, 200, 255), 0.85f, slot_str);
    vita2d_pgf_draw_text(font, 450, 115, RGBA8(150, 150, 150, 255), 0.75f,
                         "[L/R] change slot");

    /* Controls hint */
    vita2d_pgf_draw_text(font, 20, VITA_SCREEN_H - 20,
                         RGBA8(150, 150, 150, 255), 0.75f,
                         "[Cross]=Select  [Circle]=Resume");
}

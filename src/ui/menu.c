/* GBVitaEX — src/ui/menu.c
 * In-game pause menu with save states, multiplayer, and link cable options.
 * Menu items are shown/hidden based on the active core and current state.
 */

#include "menu.h"
#include "gbvitaex.h"
#include "../core/emu_core.h"
#include "../core/savestate_mgr.h"
#include "../core/rfu_vita_net.h"
#include "../gb/gb_link.h"
#include "../platform/psp2/psp2_ctx.h"

#include <vita2d.h>
#include <stdio.h>
#include <string.h>

/* ── Static items always shown ── */
typedef struct { const char *label; MenuAction action; } MenuItem;

static const MenuItem ITEMS_ALWAYS[] = {
    { "Resume",           MENU_ACTION_RESUME     },
    { "Save State",       MENU_ACTION_SAVE_STATE },
    { "Load State",       MENU_ACTION_LOAD_STATE },
    { "Reset Game",       MENU_ACTION_RESET      },
    { "Change ROM",       MENU_ACTION_LOAD_ROM   },
    { "Settings",         MENU_ACTION_SETTINGS   },
    { "Screenshot",       MENU_ACTION_SCREENSHOT },
};
#define ITEMS_ALWAYS_COUNT ((int)(sizeof(ITEMS_ALWAYS)/sizeof(ITEMS_ALWAYS[0])))

/* Built dynamically based on core type and active state */
#define MAX_ITEMS 16
static MenuItem s_items[MAX_ITEMS];
static int      s_item_count = 0;

static int s_cursor = 0;
static int s_slot   = 0;
static uint32_t s_prev = 0;

#define PRESSED(btn) ((buttons & (btn)) && !(s_prev & (btn)))

/* Rebuild the menu item list based on current state */
static void rebuild_items(void) {
    s_item_count = 0;
    /* Always-present items */
    for (int i = 0; i < ITEMS_ALWAYS_COUNT && s_item_count < MAX_ITEMS; i++)
        s_items[s_item_count++] = ITEMS_ALWAYS[i];

    /* GBA-only: RFU WiFi multiplayer */
    if (g_emu.active_core == CORE_GBA) {
        if (rfu_vita_net_active()) {
            s_items[s_item_count++] = (MenuItem){ "Stop WiFi Multiplayer", MENU_ACTION_RFU_STOP };
        } else {
            s_items[s_item_count++] = (MenuItem){ "WiFi Multiplayer: Host", MENU_ACTION_RFU_HOST };
            s_items[s_item_count++] = (MenuItem){ "WiFi Multiplayer: Join", MENU_ACTION_RFU_CLIENT };
        }
    }

    /* GB/GBC-only: link cable */
    if (g_emu.active_core == CORE_GB || g_emu.active_core == CORE_GBC) {
        if (gb_link_active()) {
            s_items[s_item_count++] = (MenuItem){ "Stop Link Cable",  MENU_ACTION_LINK_STOP };
        } else {
            s_items[s_item_count++] = (MenuItem){ "Link Cable (2P)",  MENU_ACTION_LINK_START };
        }
    }

    s_items[s_item_count++] = (MenuItem){ "Exit", MENU_ACTION_EXIT };

    /* Clamp cursor */
    if (s_cursor >= s_item_count) s_cursor = s_item_count - 1;
}

MenuAction menu_update(uint32_t buttons) {
    rebuild_items();

    MenuAction result = MENU_ACTION_NONE;

    if (PRESSED(EMU_KEY_UP))   { if (s_cursor > 0) s_cursor--; }
    if (PRESSED(EMU_KEY_DOWN)) { if (s_cursor < s_item_count - 1) s_cursor++; }

    /* Slot selection with L/R */
    if (PRESSED(EMU_KEY_L)) { if (s_slot > 0) s_slot--; }
    if (PRESSED(EMU_KEY_R)) { if (s_slot < 9) s_slot++; }

    if (PRESSED(EMU_KEY_A)) {
        MenuAction a = s_items[s_cursor].action;
        switch (a) {
        case MENU_ACTION_SAVE_STATE:
            savestate_mgr_save(s_slot);
            result = MENU_ACTION_NONE;  /* stay in menu after save */
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
        case MENU_ACTION_RFU_HOST:
            rfu_vita_net_start(true);
            result = MENU_ACTION_NONE;
            break;
        case MENU_ACTION_RFU_CLIENT:
            rfu_vita_net_start(false);
            result = MENU_ACTION_NONE;
            break;
        case MENU_ACTION_RFU_STOP:
            rfu_vita_net_stop();
            result = MENU_ACTION_NONE;
            break;
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
    rebuild_items();

    vita2d_draw_rectangle(0, 0, VITA_SCREEN_W, VITA_SCREEN_H, RGBA8(0,0,0,160));

    vita2d_pgf *font = g_pgf_font;
    if (!font) return;

    /* Title */
    vita2d_pgf_draw_text(font, 20, 30, RGBA8(255,215,0,255), 1.2f, "GBVitaEX");

    /* ROM name */
    const char *base = strrchr(g_emu.rom_path, '/');
    base = base ? base + 1 : g_emu.rom_path;
    vita2d_pgf_draw_text(font, 20, 58, RGBA8(180,180,180,255), 0.85f, base);

    /* FPS + multiplayer status */
    char fps_str[64];
    if (rfu_vita_net_active())
        snprintf(fps_str, sizeof(fps_str), "%.1f fps  |  %s",
                 g_emu.fps, rfu_vita_net_status());
    else if (gb_link_active())
        snprintf(fps_str, sizeof(fps_str), "%.1f fps  |  Link Cable active", g_emu.fps);
    else
        snprintf(fps_str, sizeof(fps_str), "%.1f fps", g_emu.fps);
    vita2d_pgf_draw_text(font, VITA_SCREEN_W - 380, 30, RGBA8(200,255,200,255), 0.9f, fps_str);

    vita2d_draw_line(20, 70, VITA_SCREEN_W-20, 70, RGBA8(80,80,80,255));

    /* Menu items */
    for (int i = 0; i < s_item_count; i++) {
        int y = 85 + i * 38;
        bool sel = (i == s_cursor);
        unsigned bg  = sel ? RGBA8(60,120,200,180) : 0;
        unsigned col = sel ? RGBA8(255,255,255,255) : RGBA8(200,200,200,200);

        /* Colour-code multiplayer items */
        if (s_items[i].action == MENU_ACTION_RFU_HOST   ||
            s_items[i].action == MENU_ACTION_RFU_CLIENT ||
            s_items[i].action == MENU_ACTION_RFU_STOP   ||
            s_items[i].action == MENU_ACTION_LINK_START ||
            s_items[i].action == MENU_ACTION_LINK_STOP) {
            col = sel ? RGBA8(255,220,100,255) : RGBA8(200,170,80,220);
        }

        if (sel) vita2d_draw_rectangle(18, y-4, 480, 34, bg);
        vita2d_pgf_draw_text(font, 30, y+22, col, 1.0f, s_items[i].label);
    }

    /* Save state slot indicator */
    char slot_str[64];
    snprintf(slot_str, sizeof(slot_str), "State slot: %d %s",
             s_slot, savestate_mgr_has_slot(s_slot) ? "(exists)" : "(empty)");
    vita2d_pgf_draw_text(font, 520, 85, RGBA8(200,200,200,255), 0.85f, slot_str);
    vita2d_pgf_draw_text(font, 520, 108, RGBA8(150,150,150,255), 0.75f, "[L/R] change slot");

    /* RFU status box if active */
    if (rfu_vita_net_active()) {
        vita2d_draw_rectangle(18, VITA_SCREEN_H-70, VITA_SCREEN_W-36, 44,
                              RGBA8(20,60,20,220));
        vita2d_pgf_draw_text(font, 30, VITA_SCREEN_H-50,
                             RGBA8(100,255,100,255), 0.85f, rfu_vita_net_status());
    }

    vita2d_pgf_draw_text(font, 20, VITA_SCREEN_H-20,
                         RGBA8(150,150,150,255), 0.75f,
                         "[Cross]=Select  [Circle]=Resume");
}

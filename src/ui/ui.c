/* GBAVitaEX — src/ui/ui.c
 * Top-level application loop.
 *
 * States:
 *   UI_STATE_BROWSER  — ROM file browser (no game running)
 *   UI_STATE_RUNNING  — game running; show frame + poll for menu button
 *   UI_STATE_MENU     — in-game menu overlay
 */

#include "ui.h"
#include "rom_browser.h"
#include "menu.h"
#include "settings.h"
#include "gbavitaex.h"
#include "../core/emu_core.h"
#include "../platform/psp2/psp2_ctx.h"
#include "../audio/audio_output.h"

#include <psp2/ctrl.h>
#include <psp2/rtc.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
   FPS counter
   ────────────────────────────────────────────────────────────────────────── */
#define FPS_WINDOW 60   /* average over this many frames */
static uint64_t s_frame_times[FPS_WINDOW];
static int      s_ft_idx = 0;

static void fps_tick(void) {
    SceRtcTick tick;
    sceRtcGetCurrentTick(&tick);
    s_frame_times[s_ft_idx % FPS_WINDOW] = tick.tick;
    s_ft_idx++;
    if (s_ft_idx >= FPS_WINDOW * 2) {
        uint64_t oldest = s_frame_times[(s_ft_idx) % FPS_WINDOW];
        uint64_t newest = tick.tick;
        uint64_t hz     = sceRtcGetTickResolution();
        if (newest > oldest && hz > 0)
            g_emu.fps = (float)(FPS_WINDOW - 1) / ((float)(newest - oldest) / hz);
    }
}

/* ──────────────────────────────────────────────────────────────────────────
   Audio drain (GBA path — GB has its own thread)
   ────────────────────────────────────────────────────────────────────────── */
#define AUDIO_DRAIN_FRAMES 735   /* ~1 frame at 44100 Hz */
static int16_t s_audio_buf[AUDIO_DRAIN_FRAMES * 2];

static void drain_gba_audio(void) {
    if (g_emu.active_core != CORE_GBA) return;
    int got = emu_drain_audio(s_audio_buf, AUDIO_DRAIN_FRAMES);
    if (got > 0 && g_emu.audio_enabled)
        audio_output_submit(s_audio_buf, got);
}

/* ──────────────────────────────────────────────────────────────────────────
   UI state machine
   ────────────────────────────────────────────────────────────────────────── */
typedef enum {
    UI_STATE_BROWSER = 0,
    UI_STATE_RUNNING,
    UI_STATE_MENU,
    UI_STATE_SETTINGS,
} UIState;

static UIState s_state = UI_STATE_BROWSER;

/* Hold-to-open-menu: require SELECT+START held for 30 frames */
#define MENU_HOLD_FRAMES 30
static int s_menu_hold = 0;

void ui_mainloop(void) {
    /* Initialise GBA audio output (gpSP mono/stereo, default 44100 Hz) */
    audio_output_init(44100, AUDIO_DRAIN_FRAMES);
    audio_output_set_volume(g_emu.audio_volume);

    while (true) {
        psp2_ctx_begin_frame();

        uint8_t  lx, ly;
        uint32_t buttons = psp2_ctx_poll_input(&lx, &ly);

        switch (s_state) {

        /* ── ROM BROWSER ─────────────────────────────────────────────── */
        case UI_STATE_BROWSER: {
            const char *selected = rom_browser_update(buttons);
            rom_browser_draw();
            if (selected) {
                if (emu_load_rom(selected)) {
                    psp2_ctx_set_clock(g_emu.cpu_clock_mhz);
                    s_state = UI_STATE_RUNNING;
                    s_menu_hold = 0;
                }
            }
            break;
        }

        /* ── EMULATION RUNNING ───────────────────────────────────────── */
        case UI_STATE_RUNNING: {
            /* Check for menu combo: SELECT + START held */
            if ((buttons & (EMU_KEY_SELECT | EMU_KEY_START)) ==
                           (EMU_KEY_SELECT | EMU_KEY_START)) {
                s_menu_hold++;
            } else {
                s_menu_hold = 0;
            }

            if (s_menu_hold >= MENU_HOLD_FRAMES) {
                s_menu_hold = 0;
                g_emu.show_menu = true;
                s_state = UI_STATE_MENU;
                break;
            }

            /* Pass input to emulator (mask out the menu combo during hold) */
            emu_set_input(buttons, lx, ly);

            /* Run one frame */
            emu_run_frame();
            drain_gba_audio();
            fps_tick();

            /* Blit to screen */
            int fw, fh;
            const uint16_t *fb = emu_get_framebuffer(&fw, &fh);
            if (fb)
                psp2_ctx_blit(fb, fw, fh,
                              (ScreenMode)g_emu.screen_mode,
                              g_emu.interframe_blend,
                              g_emu.color_correct);

            /* Overlay: FPS counter (top-right) */
            if (g_pgf_font) {
                char fps_str[32];
                snprintf(fps_str, sizeof(fps_str), "%.1f fps", g_emu.fps);
                vita2d_pgf_draw_text(g_pgf_font, VITA_SCREEN_W - 120, 20,
                                     RGBA8(255, 255, 255, 180), 1.0f, fps_str);
            }
            break;
        }

        /* ── IN-GAME MENU ────────────────────────────────────────────── */
        case UI_STATE_MENU: {
            MenuAction action = menu_update(buttons);
            menu_draw();
            switch (action) {
            case MENU_ACTION_RESUME:
                s_state = UI_STATE_RUNNING;
                g_emu.show_menu = false;
                break;
            case MENU_ACTION_RESET:
                emu_reset();
                s_state = UI_STATE_RUNNING;
                g_emu.show_menu = false;
                break;
            case MENU_ACTION_LOAD_ROM:
                emu_unload_rom();
                rom_browser_reset();
                s_state = UI_STATE_BROWSER;
                g_emu.show_menu = false;
                break;
            case MENU_ACTION_SETTINGS:
                s_state = UI_STATE_SETTINGS;
                break;
            case MENU_ACTION_EXIT:
                goto exit_loop;
            default:
                break;
            }
            break;
        }

        /* ── SETTINGS ────────────────────────────────────────────────── */
        case UI_STATE_SETTINGS: {
            bool done = settings_update(buttons);
            settings_draw();
            if (done) {
                /* Apply changed clock speed immediately */
                psp2_ctx_set_clock(g_emu.cpu_clock_mhz);
                audio_output_set_volume(g_emu.audio_volume);
                s_state = UI_STATE_MENU;
            }
            break;
        }

        } /* switch */

        psp2_ctx_end_frame();
        continue;

exit_loop:
        break;
    } /* while */

    audio_output_shutdown();
}

/* GBAVitaEX — src/ui/ui.c
 * Top-level application loop.
 *
 * States:
 *   UI_STATE_BROWSER  — ROM file browser
 *   UI_STATE_RUNNING  — game running
 *   UI_STATE_MENU     — in-game pause menu
 *   UI_STATE_SETTINGS — settings screen
 *   UI_STATE_KEYMAPPER— button remapping screen
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
/* sceKernelPowerTick defined in processmgr.h, included via gbavitaex.h chain */
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
   FPS counter
   ────────────────────────────────────────────────────────────────────────── */
#define FPS_WINDOW 60
static uint64_t s_frame_times[FPS_WINDOW];
static int      s_ft_idx = 0;

static void fps_tick(void) {
    SceRtcTick tick;
    sceRtcGetCurrentTick(&tick);
    s_frame_times[s_ft_idx % FPS_WINDOW] = tick.tick;
    s_ft_idx++;
    if (s_ft_idx >= FPS_WINDOW * 2) {
        uint64_t oldest = s_frame_times[s_ft_idx % FPS_WINDOW];
        uint64_t newest = tick.tick;
        uint64_t hz     = sceRtcGetTickResolution();
        if (newest > oldest && hz > 0)
            g_emu.fps = (float)(FPS_WINDOW-1) / ((float)(newest-oldest) / hz);
    }
}

/* ──────────────────────────────────────────────────────────────────────────
   GBA audio drain
   ────────────────────────────────────────────────────────────────────────── */
#define AUDIO_DRAIN_FRAMES 735
static int16_t s_audio_buf[AUDIO_DRAIN_FRAMES * 2];

static void drain_gba_audio(void) {
    if (g_emu.active_core != CORE_GBA) return;
    int got = emu_drain_audio(s_audio_buf, AUDIO_DRAIN_FRAMES);
    if (got > 0 && g_emu.audio_enabled)
        audio_output_submit(s_audio_buf, got);
}

/* ──────────────────────────────────────────────────────────────────────────
   Fast-forward
   Uses a frame-accumulator: when FF is active, run extra frames per vsync
   proportional to ff_speed_pct.  A remainder accumulator prevents drift.
   ────────────────────────────────────────────────────────────────────────── */
static int s_ff_accumulator = 0;  /* in units of 100 */

static int ff_extra_frames(void) {
    /* Returns how many *extra* frames to run this vsync (0 at 1×). */
    s_ff_accumulator += g_emu.ff_speed_pct;
    int extra = (s_ff_accumulator / 100) - 1;  /* subtract the base 1 frame */
    s_ff_accumulator -= (extra + 1) * 100;
    if (extra < 0) extra = 0;
    return extra;
}

/* ──────────────────────────────────────────────────────────────────────────
   UI state machine
   ────────────────────────────────────────────────────────────────────────── */
typedef enum {
    UI_STATE_BROWSER = 0,
    UI_STATE_RUNNING,
    UI_STATE_MENU,
    UI_STATE_SETTINGS,
    UI_STATE_KEYMAPPER,
} UIState;

static UIState s_state     = UI_STATE_BROWSER;
static int     s_menu_hold = 0;
#define MENU_HOLD_FRAMES 30

void ui_mainloop(void) {
    audio_output_init(44100, AUDIO_DRAIN_FRAMES);
    audio_output_set_volume(g_emu.audio_volume);

    while (true) {
        /* ── Watchdog: prevent Vita auto-suspend during emulation ── */
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);

        psp2_ctx_begin_frame();

        uint8_t  lx, ly;
        uint32_t raw_buttons = psp2_ctx_poll_raw(&lx, &ly);
        /* Apply button remapping for emulator keys */
        uint32_t emu_buttons = psp2_ctx_remap_buttons(raw_buttons);
        /* Detect fast-forward trigger (physical button, not remapped) */
        bool ff_held = (g_emu.ff_button >= 0 &&
                        g_emu.ff_button < VBTN_COUNT &&
                        psp2_ctx_vbtn_pressed(raw_buttons, (VitaButton)g_emu.ff_button));
        g_emu.fast_forward = ff_held;

        /* ── Suppress R trigger from emu input when it's the FF button ── */
        if (ff_held && g_emu.ff_button == VBTN_R1)
            emu_buttons &= ~EMU_KEY_R;

        switch (s_state) {

        /* ── ROM BROWSER ─────────────────────────────────────────── */
        case UI_STATE_BROWSER: {
            const char *sel = rom_browser_update(emu_buttons);
            rom_browser_draw();
            if (sel && emu_load_rom(sel)) {
                psp2_ctx_set_clock(g_emu.cpu_clock_mhz);
                s_state    = UI_STATE_RUNNING;
                s_menu_hold = 0;
                s_ff_accumulator = 0;
            }
            break;
        }

        /* ── EMULATION RUNNING ───────────────────────────────────── */
        case UI_STATE_RUNNING: {
            /* Menu combo: SELECT+START held */
            if ((emu_buttons & (EMU_KEY_SELECT|EMU_KEY_START)) ==
                               (EMU_KEY_SELECT|EMU_KEY_START)) {
                if (++s_menu_hold >= MENU_HOLD_FRAMES) {
                    s_menu_hold = 0;
                    g_emu.show_menu = true;
                    s_state = UI_STATE_MENU;
                    break;
                }
            } else {
                s_menu_hold = 0;
            }

            emu_set_input(emu_buttons, lx, ly);

            /* ── Fast-forward: disable vsync so swap_buffers doesn't block ── */
            static bool s_ff_was_active = false;
            if (g_emu.fast_forward != s_ff_was_active) {
                psp2_ctx_set_vsync(!g_emu.fast_forward);
                s_ff_was_active = g_emu.fast_forward;
            }

            /* Run base frame + any fast-forward extra frames.
             * During FF we skip blitting intermediate frames — only the
             * final frame is uploaded and presented, keeping GPU load low. */
            emu_run_frame();
            int extra = g_emu.fast_forward ? ff_extra_frames() : 0;
            for (int i = 0; i < extra; i++) {
                emu_run_frame();
                drain_gba_audio();
            }
            drain_gba_audio();
            fps_tick();

            /* Blit — skip rendering extra FF frames (only blit last one) */
            int fw, fh;
            const uint16_t *fb = emu_get_framebuffer(&fw, &fh);
            if (fb)
                psp2_ctx_blit(fb, fw, fh,
                              (ScreenMode)g_emu.screen_mode,
                              g_emu.interframe_blend && !g_emu.fast_forward,
                              g_emu.color_correct);

            /* HUD overlay */
            if (g_pgf_font) {
                char hud[64];
                if (g_emu.fast_forward)
                    snprintf(hud, sizeof(hud), "%.1f fps  >> %.1fx",
                             g_emu.fps, g_emu.ff_speed_pct / 100.0f);
                else
                    snprintf(hud, sizeof(hud), "%.1f fps", g_emu.fps);
                vita2d_pgf_draw_text(g_pgf_font, VITA_SCREEN_W - 200, 20,
                                     RGBA8(255, 255, 0, 200), 0.9f, hud);
            }
            break;
        }

        /* ── IN-GAME MENU ────────────────────────────────────────── */
        case UI_STATE_MENU: {
            /* Restore vsync when menu opens (FF is implicitly off) */
            psp2_ctx_set_vsync(true);
            g_emu.fast_forward = false;
            MenuAction action = menu_update(emu_buttons);
            menu_draw();
            switch (action) {
            case MENU_ACTION_RESUME:
                s_state = UI_STATE_RUNNING;
                g_emu.show_menu = false;
                s_ff_accumulator = 0;
                break;
            case MENU_ACTION_RESET:
                emu_reset();
                s_state = UI_STATE_RUNNING;
                g_emu.show_menu = false;
                break;
            case MENU_ACTION_LOAD_ROM:
                emu_unload_rom();
                rom_browser_reset();
                psp2_ctx_set_vsync(true);  /* browser always vsync-locked */
                s_state = UI_STATE_BROWSER;
                g_emu.show_menu = false;
                break;
            case MENU_ACTION_SETTINGS:
                s_state = UI_STATE_SETTINGS;
                break;
            case MENU_ACTION_EXIT:
                config_save();
                goto exit_loop;
            default:
                break;
            }
            break;
        }

        /* ── SETTINGS ────────────────────────────────────────────── */
        case UI_STATE_SETTINGS: {
            bool done = settings_update(emu_buttons);
            settings_draw();
            if (done) {
                /* Key mapper entered from settings */
                if (settings_wants_keymapper()) {
                    s_state = UI_STATE_KEYMAPPER;
                    break;
                }
                psp2_ctx_set_clock(g_emu.cpu_clock_mhz);
                audio_output_set_volume(g_emu.audio_volume);
                config_save();
                s_state = UI_STATE_MENU;
            }
            break;
        }

        /* ── BUTTON REMAPPER ─────────────────────────────────────── */
        case UI_STATE_KEYMAPPER: {
            bool done = keymapper_update(raw_buttons);
            keymapper_draw();
            if (done) {
                config_save();
                s_state = UI_STATE_SETTINGS;
            }
            break;
        }

        } /* switch */

        psp2_ctx_end_frame();
        continue;

exit_loop:
        break;
    }

    audio_output_shutdown();
}

/* GBAVitaEX — src/gb/gb_link.h
 * Single-device GB/GBC link cable emulation via mGBA's GBSIOLockstep.
 *
 * Connects two mGBA GB cores running in the same process.
 * Player 1 is the "normal" single-player game; Player 2 runs in a
 * secondary core that is stepped in lock-step alongside Player 1.
 *
 * Both cores share the same display buffer area (Player 2 renders to
 * the right half of a wider texture) and the same SceAudioOut port
 * (Player 2 audio is mixed 50/50 with Player 1's output).
 *
 * Usage:
 *   gb_link_start(rom1, save1, rom2, save2)  -- attach two ROMs
 *   gb_link_run_frame()                       -- step both cores together
 *   gb_link_stop()                            -- detach, free resources
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Start link-cable mode.
 * rom1/save1 = player 1 paths (same as the currently loaded ROM is fine)
 * rom2/save2 = player 2 paths (can be the same ROM or a different version)
 * Returns false if either ROM fails to load. */
bool gb_link_start(const char *rom1, const char *save1,
                   const char *rom2, const char *save2);

/* Step both cores for one video frame in lock-step. */
void gb_link_run_frame(void);

/* Stop and free both secondary resources. */
void gb_link_stop(void);

/* True if link mode is currently active. */
bool gb_link_active(void);

/* Get the framebuffer of the secondary (P2) core.
 * Width/height match GB_SCREEN_W / GB_SCREEN_H. */
const uint16_t *gb_link_get_p2_framebuffer(void);

/* Mix P2 audio into a caller-supplied buffer (same format as P1). */
int gb_link_drain_p2_audio(int16_t *buf, int max_frames);

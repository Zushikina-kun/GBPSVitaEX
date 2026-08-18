/* GBAVitaEX — src/platform/psp2/psp2_ctx.h
 * vita2d rendering context, input polling, clock control.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <vita2d.h>

/* Screen scaling modes */
typedef enum {
    SCREEN_MODE_ASPECT    = 0,  /* aspect-correct centred (black bars)    */
    SCREEN_MODE_FULL      = 1,  /* stretched to fill 960×544              */
    SCREEN_MODE_INTEGER   = 2,  /* largest integer scale (4× for GBA)     */
    SCREEN_MODE_MAX
} ScreenMode;

/* Initialise vita2d, audio device, touch. Loads the system PGF font once. */
void psp2_ctx_init(void);

/* Shut down vita2d and audio. */
void psp2_ctx_shutdown(void);

/* Cached system PGF font — loaded once in psp2_ctx_init(), freed in
 * psp2_ctx_shutdown().  All UI code must use this pointer instead of
 * calling vita2d_load_default_pgf() per-frame (which leaks). */
extern vita2d_pgf *g_pgf_font;

/* Begin a vita2d frame. */
void psp2_ctx_begin_frame(void);

/* Blit an emulator framebuffer (RGB565) into a vita2d texture and draw it.
 * w/h: source resolution (240×160 for GBA, 160×144 for GB).
 * interframe_blend: mix previous and current frames (ghosting effect). */
void psp2_ctx_blit(const uint16_t *pixels, int w, int h,
                   ScreenMode mode, bool interframe_blend, bool color_correct);

/* End the vita2d frame and present. */
void psp2_ctx_end_frame(void);

/* Poll Vita controls → unified EMU_KEY_* bitmask + raw analog. */
uint32_t psp2_ctx_poll_input(uint8_t *lx_out, uint8_t *ly_out);

/* Set CPU clock (MHz: 41/83/111/166/222/333/444/500). */
void psp2_ctx_set_clock(int mhz);

/* Screenshot: save current framebuffer as PNG to path. */
bool psp2_ctx_screenshot(const char *path);

/* GBAVitaEX — src/platform/psp2/psp2_ctx.c
 * PSVita platform context: vita2d rendering, input, clock control.
 *
 * Rendering approach
 * ──────────────────
 * We keep two vita2d textures (double-buffer) per active system:
 *   GBA:  SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB  (native RGB565, 240 wide)
 *   GB:   SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB  (RGB565 after mGBA write, 160 wide)
 *
 * Stride for both is the next power-of-two ≥ source width (256 px each).
 * vita2d draws the texture scaled to the chosen ScreenMode on the 960×544 screen.
 *
 * Interframe blending: we mix the previous and current texture via vita2d's
 * blend state — same approach as mGBA's PSP2 context.
 *
 * Colour correction: applied in software before uploading to the texture when
 * enabled.  Uses the same look-up table as gpSP's post_process_cc path.
 */

#include "psp2_ctx.h"
#include "gbavitaex.h"

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/power.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <vita2d.h>
#include <png.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* ──────────────────────────────────────────────────────────────────────────
   Internal helpers
   ────────────────────────────────────────────────────────────────────────── */

/* Next power-of-two ≥ x */
static inline int next_pow2(int x) {
    int p = 1;
    while (p < x) p <<= 1;
    return p;
}

/* ──────────────────────────────────────────────────────────────────────────
   Texture double-buffer
   ────────────────────────────────────────────────────────────────────────── */
#define NUM_TEX 2
static vita2d_texture *s_tex[NUM_TEX];
static int             s_cur_tex = 0;
static int             s_tex_w   = 0;  /* actual source width (GBA=240/GB=160) */
static int             s_tex_h   = 0;

/* Reallocate textures if resolution changes (switching GBA↔GB). */
static void ensure_textures(int w, int h) {
    if (s_tex_w == w && s_tex_h == h) return;

    for (int i = 0; i < NUM_TEX; i++) {
        if (s_tex[i]) { vita2d_free_texture(s_tex[i]); s_tex[i] = NULL; }
    }

    int tw = next_pow2(w);
    int th = next_pow2(h);
    for (int i = 0; i < NUM_TEX; i++) {
        s_tex[i] = vita2d_create_empty_texture_format(
                       tw, th, SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB);
        if (!s_tex[i]) {
            fprintf(stderr, "[psp2_ctx] Failed to allocate texture %d\n", i);
            continue;
        }
        memset(vita2d_texture_get_datap(s_tex[i]), 0,
               vita2d_texture_get_stride(s_tex[i]) * th);
    }
    s_tex_w = w;
    s_tex_h = h;
    s_cur_tex = 0;
}

/* ──────────────────────────────────────────────────────────────────────────
   GBA LCD colour correction LUT (5-bit R/G/B in, 8-bit R/G/B out)
   Derived from gpSP's gba_cc_lut and the Higan author's measurements.
   Applied inline when ENABLE_COLOR_CORRECT is defined.
   ────────────────────────────────────────────────────────────────────────── */
#ifdef ENABLE_COLOR_CORRECT
/* Declare without pulling in gpSP's common.h chain (which needs retro_inline.h) */
extern const uint16_t gba_cc_lut[];
#endif

/* Upload RGB565 pixels (with optional colour correction) to the current tex. */
static void upload_pixels(const uint16_t *src, int src_w, int src_h,
                           bool color_correct) {
    vita2d_texture *t = s_tex[s_cur_tex];
    if (!t) return;

    uint16_t *dst    = (uint16_t *)vita2d_texture_get_datap(t);
    uint32_t  stride = vita2d_texture_get_stride(t) / sizeof(uint16_t);

    for (int y = 0; y < src_h; y++) {
        const uint16_t *src_row = src + y * src_w;
        uint16_t       *dst_row = dst + y * stride;
#ifdef ENABLE_COLOR_CORRECT
        if (color_correct) {
            for (int x = 0; x < src_w; x++)
                dst_row[x] = gba_cc_lut[src_row[x]];
        } else
#else
        (void)color_correct;
#endif
        {
            memcpy(dst_row, src_row, src_w * sizeof(uint16_t));
        }
    }
}

/* ──────────────────────────────────────────────────────────────────────────
   Screen-mode destination rectangles on the 960×544 display
   ────────────────────────────────────────────────────────────────────────── */
static void calc_dest_rect(ScreenMode mode, int src_w, int src_h,
                            float *dx, float *dy, float *dw, float *dh) {
    switch (mode) {
    case SCREEN_MODE_FULL:
        *dx = 0;  *dy = 0;
        *dw = VITA_SCREEN_W;
        *dh = VITA_SCREEN_H;
        break;

    case SCREEN_MODE_INTEGER: {
        int sx = VITA_SCREEN_W / src_w;
        int sy = VITA_SCREEN_H / src_h;
        int s  = (sx < sy) ? sx : sy;
        if (s < 1) s = 1;
        *dw = src_w * s;
        *dh = src_h * s;
        *dx = (VITA_SCREEN_W - *dw) / 2.0f;
        *dy = (VITA_SCREEN_H - *dh) / 2.0f;
        break;
    }
    case SCREEN_MODE_ASPECT:
    default: {
        float ratio_w = (float)VITA_SCREEN_W / src_w;
        float ratio_h = (float)VITA_SCREEN_H / src_h;
        float ratio   = (ratio_w < ratio_h) ? ratio_w : ratio_h;
        *dw = src_w * ratio;
        *dh = src_h * ratio;
        *dx = (VITA_SCREEN_W - *dw) / 2.0f;
        *dy = (VITA_SCREEN_H - *dh) / 2.0f;
        break;
    }
    }
}

/* ──────────────────────────────────────────────────────────────────────────
   Public API
   ────────────────────────────────────────────────────────────────────────── */

/* Globally cached PGF font — loaded once, never re-allocated per frame. */
vita2d_pgf *g_pgf_font = NULL;

void psp2_ctx_init(void) {
    vita2d_init();
    vita2d_set_clear_color(RGBA8(0, 0, 0, 255));
    memset(s_tex, 0, sizeof(s_tex));
    /* Load system font once here — UI files reference g_pgf_font directly */
    g_pgf_font = vita2d_load_default_pgf();
}

void psp2_ctx_shutdown(void) {
    for (int i = 0; i < NUM_TEX; i++) {
        if (s_tex[i]) { vita2d_free_texture(s_tex[i]); s_tex[i] = NULL; }
    }
    if (g_pgf_font) { vita2d_free_pgf(g_pgf_font); g_pgf_font = NULL; }
    vita2d_fini();
}

void psp2_ctx_begin_frame(void) {
    vita2d_start_drawing();
    vita2d_clear_screen();
}

void psp2_ctx_blit(const uint16_t *pixels, int w, int h,
                   ScreenMode mode, bool interframe_blend, bool color_correct) {
    ensure_textures(w, h);
    upload_pixels(pixels, w, h, color_correct);

    float dx, dy, dw, dh;
    calc_dest_rect(mode, w, h, &dx, &dy, &dw, &dh);

    vita2d_texture *cur  = s_tex[s_cur_tex];
    vita2d_texture *prev = s_tex[s_cur_tex ^ 1];

#ifdef ENABLE_INTERFRAME
    if (interframe_blend && prev) {
        /* Draw previous frame at half opacity then current at full */
        vita2d_draw_texture_tint_part_scale(
            prev,
            dx, dy,
            0, 0, (float)w, (float)h,
            dw / w, dh / h,
            RGBA8(255, 255, 255, 128));   /* 50% alpha */
        vita2d_draw_texture_part_scale(
            cur,
            dx, dy,
            0, 0, (float)w, (float)h,
            dw / w, dh / h);
    } else
#else
    (void)interframe_blend;
#endif
    {
        vita2d_draw_texture_part_scale(cur, dx, dy,
                                       0, 0, (float)w, (float)h,
                                       dw / w, dh / h);
    }

    /* Flip double-buffer */
    s_cur_tex ^= 1;
}

void psp2_ctx_end_frame(void) {
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

/* ──────────────────────────────────────────────────────────────────────────
   Input
   ────────────────────────────────────────────────────────────────────────── */

/* Map SCE_CTRL_* → EMU_KEY_* */
uint32_t psp2_ctx_poll_input(uint8_t *lx_out, uint8_t *ly_out) {
    SceCtrlData pad;
    sceCtrlPeekBufferPositiveExt2(0, &pad, 1);

    uint32_t keys = 0;
    if (pad.buttons & SCE_CTRL_CROSS)    keys |= EMU_KEY_A;
    if (pad.buttons & SCE_CTRL_CIRCLE)   keys |= EMU_KEY_B;
    if (pad.buttons & SCE_CTRL_SELECT)   keys |= EMU_KEY_SELECT;
    if (pad.buttons & SCE_CTRL_START)    keys |= EMU_KEY_START;
    if (pad.buttons & SCE_CTRL_RIGHT)    keys |= EMU_KEY_RIGHT;
    if (pad.buttons & SCE_CTRL_LEFT)     keys |= EMU_KEY_LEFT;
    if (pad.buttons & SCE_CTRL_UP)       keys |= EMU_KEY_UP;
    if (pad.buttons & SCE_CTRL_DOWN)     keys |= EMU_KEY_DOWN;
    if (pad.buttons & SCE_CTRL_RTRIGGER) keys |= EMU_KEY_R;
    if (pad.buttons & SCE_CTRL_LTRIGGER) keys |= EMU_KEY_L;

    /* D-pad from analog left stick if digital d-pad is neutral */
    if (!(keys & (EMU_KEY_RIGHT|EMU_KEY_LEFT|EMU_KEY_UP|EMU_KEY_DOWN))) {
        if (pad.lx > 192)  keys |= EMU_KEY_RIGHT;
        if (pad.lx < 64)   keys |= EMU_KEY_LEFT;
        if (pad.ly > 192)  keys |= EMU_KEY_DOWN;
        if (pad.ly < 64)   keys |= EMU_KEY_UP;
    }

    if (lx_out) *lx_out = pad.lx;
    if (ly_out) *ly_out = pad.ly;
    return keys;
}

void psp2_ctx_set_clock(int mhz) {
    scePowerSetArmClockFrequency(mhz);
}

bool psp2_ctx_screenshot(const char *path) {
    /* Grab the current display framebuffer and encode it as PNG. */
    SceDisplayFrameBuf fb;
    fb.size = sizeof(SceDisplayFrameBuf);
    if (sceDisplayGetFrameBuf(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME) < 0)
        return false;
    if (!fb.base || fb.width == 0 || fb.height == 0)
        return false;

    FILE *f = fopen(path, "wb");
    if (!f) return false;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              NULL, NULL, NULL);
    if (!png) { fclose(f); return false; }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, NULL); fclose(f); return false; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(f);
        return false;
    }

    png_init_io(png, f);
    png_set_IHDR(png, info,
                 fb.width, fb.height, 8,
                 PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    /* The framebuf is BGRA8888 — convert to RGB row by row */
    uint8_t *row = malloc(fb.width * 3);
    if (row) {
        const uint32_t *src = (const uint32_t *)fb.base;
        for (unsigned y = 0; y < fb.height; y++) {
            const uint32_t *line = src + y * fb.pitch;
            for (unsigned x = 0; x < fb.width; x++) {
                uint32_t px  = line[x];
                row[x*3+0]   = (px >>  0) & 0xFF;  /* B → R (BGRA) */
                row[x*3+1]   = (px >>  8) & 0xFF;
                row[x*3+2]   = (px >> 16) & 0xFF;
            }
            png_write_row(png, row);
        }
        free(row);
    }

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(f);
    return true;
}

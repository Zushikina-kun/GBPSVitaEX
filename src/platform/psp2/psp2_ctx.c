/* GBVitaEX — src/platform/psp2/psp2_ctx.c
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
#include "gbvitaex.h"

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/power.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/threadmgr.h>   /* sceKernelCreateThread / StartThread */
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

/* Map a VitaButton enum → its SCE_CTRL_* bitmask */
static const uint32_t s_sce_mask[VBTN_COUNT] = {
    [VBTN_CROSS]    = SCE_CTRL_CROSS,
    [VBTN_CIRCLE]   = SCE_CTRL_CIRCLE,
    [VBTN_SQUARE]   = SCE_CTRL_SQUARE,
    [VBTN_TRIANGLE] = SCE_CTRL_TRIANGLE,
    [VBTN_START]    = SCE_CTRL_START,
    [VBTN_SELECT]   = SCE_CTRL_SELECT,
    [VBTN_UP]       = SCE_CTRL_UP,
    [VBTN_DOWN]     = SCE_CTRL_DOWN,
    [VBTN_LEFT]     = SCE_CTRL_LEFT,
    [VBTN_RIGHT]    = SCE_CTRL_RIGHT,
    [VBTN_L1]       = SCE_CTRL_LTRIGGER,
    [VBTN_R1]       = SCE_CTRL_RTRIGGER,
    [VBTN_L2]       = 0,  /* back-touch: handled separately */
    [VBTN_R2]       = 0,
};

bool psp2_ctx_vbtn_pressed(uint32_t raw_sce, VitaButton btn) {
    if ((int)btn < 0 || btn >= VBTN_COUNT) return false;
    uint32_t mask = s_sce_mask[btn];
    if (!mask) return false;
    return (raw_sce & mask) != 0;
}

uint32_t psp2_ctx_remap_buttons(uint32_t raw_sce) {
    uint32_t result = 0;
    for (int i = 0; i < VBTN_COUNT; i++) {
        uint32_t mask = s_sce_mask[i];
        if (mask && (raw_sce & mask))
            result |= g_emu.key_map[i];
    }
    return result;
}

uint32_t psp2_ctx_poll_raw(uint8_t *lx_out, uint8_t *ly_out) {
    SceCtrlData pad;
    sceCtrlPeekBufferPositiveExt2(0, &pad, 1);
    if (lx_out) *lx_out = pad.lx;
    if (ly_out) *ly_out = pad.ly;
    return pad.buttons;
}

/* Legacy: poll + remap in one call */
uint32_t psp2_ctx_poll_input(uint8_t *lx_out, uint8_t *ly_out) {
    uint32_t raw = psp2_ctx_poll_raw(lx_out, ly_out);
    uint32_t mapped = psp2_ctx_remap_buttons(raw);

    /* Analog stick D-pad fallback when no digital d-pad mapped */
    uint8_t lx = lx_out ? *lx_out : 128;
    uint8_t ly = ly_out ? *ly_out : 128;
    if (lx > 192) mapped |= EMU_KEY_RIGHT;
    if (lx < 64)  mapped |= EMU_KEY_LEFT;
    if (ly > 192) mapped |= EMU_KEY_DOWN;
    if (ly < 64)  mapped |= EMU_KEY_UP;

    return mapped;
}

void psp2_ctx_set_clock(int mhz) {
    scePowerSetArmClockFrequency(mhz);
}

void psp2_ctx_set_vsync(bool enable) {
    vita2d_set_vblank_wait(enable ? 1 : 0);
}

/* ──────────────────────────────────────────────────────────────────────────
   Async screenshot
   The PNG encode of a 960×544 BGRA8888 framebuffer takes ~60–100 ms —
   doing it synchronously on the main thread drops one or two frames.
   We copy the raw pixel data to a heap buffer, spin up a kernel thread,
   and let it do the encode + file write in the background.
   ────────────────────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t  *pixels;     /* heap-allocated BGRA8888 copy of the framebuffer */
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;      /* in pixels, not bytes */
    char      path[512];
} ScreenshotJob;

static int screenshot_thread_fn(SceSize sz, void *arg) {
    (void)sz;
    ScreenshotJob *job = (ScreenshotJob *)arg;

    FILE *f = fopen(job->path, "wb");
    if (!f) { free(job->pixels); free(job); sceKernelExitThread(1); return 1; }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop   info = png ? png_create_info_struct(png) : NULL;

    if (!png || !info || setjmp(png_jmpbuf(png))) {
        if (png) png_destroy_write_struct(&png, info ? &info : NULL);
        fclose(f);
        free(job->pixels);
        free(job);
        sceKernelExitThread(1);
        return 1;
    }

    png_init_io(png, f);
    png_set_IHDR(png, info,
                 job->width, job->height, 8,
                 PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    /* BGRA8888 → RGB888 row by row */
    uint8_t *row = malloc(job->width * 3);
    if (row) {
        const uint32_t *src = (const uint32_t *)job->pixels;
        for (uint32_t y = 0; y < job->height; y++) {
            const uint32_t *line = src + y * job->pitch;
            for (uint32_t x = 0; x < job->width; x++) {
                uint32_t px = line[x];
                /* Vita framebuf is BGRA: B=bits[7:0] G=bits[15:8] R=bits[23:16] */
                row[x*3+0] = (px >> 16) & 0xFF; /* R */
                row[x*3+1] = (px >>  8) & 0xFF; /* G */
                row[x*3+2] = (px >>  0) & 0xFF; /* B */
            }
            png_write_row(png, row);
        }
        free(row);
    }

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(f);

    free(job->pixels);
    free(job);
    sceKernelExitThread(0);
    return 0;
}

bool psp2_ctx_screenshot(const char *path) {
    /* Grab the current display framebuffer */
    SceDisplayFrameBuf fb;
    fb.size = sizeof(SceDisplayFrameBuf);
    if (sceDisplayGetFrameBuf(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME) < 0)
        return false;
    if (!fb.base || fb.width == 0 || fb.height == 0)
        return false;

    /* Copy pixel data immediately (framebuf may be overwritten next frame) */
    size_t byte_size = fb.pitch * fb.height * 4; /* BGRA8888 */
    uint8_t *pixels = malloc(byte_size);
    if (!pixels) return false;
    memcpy(pixels, fb.base, byte_size);

    /* Build job struct */
    ScreenshotJob *job = malloc(sizeof(ScreenshotJob));
    if (!job) { free(pixels); return false; }
    job->pixels = pixels;
    job->width  = fb.width;
    job->height = fb.height;
    job->pitch  = fb.pitch;
    strncpy(job->path, path, sizeof(job->path) - 1);
    job->path[sizeof(job->path) - 1] = '\0';

    /* Spawn background thread — SCE priority 0x10000100, stack 64 KB */
    SceUID tid = sceKernelCreateThread("gbvitaex_screenshot",
                                       screenshot_thread_fn,
                                       0x10000100, 64 * 1024, 0, 0, NULL);
    if (tid < 0) {
        free(pixels);
        free(job);
        return false;
    }
    /* Pass job pointer as arg; thread frees it when done */
    sceKernelStartThread(tid, sizeof(ScreenshotJob *), &job);
    /* Don't join — we're fire-and-forget.  Thread cleans itself up. */
    return true;
}

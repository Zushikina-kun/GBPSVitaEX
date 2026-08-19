/* GBVitaEX — src/ui/rom_browser.c
 * Simple paginated file browser for ROM selection.
 * Supported extensions: .gba .agb .bin .gbz .gbc .gb
 */

#include "rom_browser.h"
#include "gbvitaex.h"
#include "../platform/psp2/psp2_ctx.h"   /* g_pgf_font */

#include <psp2/io/dirent.h>
#include <vita2d.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

/* ──────────────────────────────────────────────────────────────────────────
   File list
   ────────────────────────────────────────────────────────────────────────── */
#define MAX_FILES    1024
#define NAME_LEN     256
#define VISIBLE_ROWS 18     /* how many entries fit on screen */
#define ROW_HEIGHT   28

static char  s_files[MAX_FILES][NAME_LEN];
static int   s_file_count = 0;
static int   s_cursor     = 0;
static int   s_scroll     = 0;   /* top visible entry */
static char  s_cur_dir[512];
static bool  s_loaded     = false;
static char  s_selected[512];    /* result path */

static bool is_rom_ext(const char *name) {
    const char *e = strrchr(name, '.');
    if (!e) return false;
    return !strcasecmp(e, ".gba") || !strcasecmp(e, ".agb") ||
           !strcasecmp(e, ".gbz") || !strcasecmp(e, ".gbc") ||
           !strcasecmp(e, ".gb")  || !strcasecmp(e, ".bin");
}

static int cmp_names(const void *a, const void *b) {
    return strcasecmp((const char *)a, (const char *)b);
}

static void load_dir(const char *dir) {
    strncpy(s_cur_dir, dir, sizeof(s_cur_dir) - 1);
    s_file_count = 0;
    s_cursor     = 0;
    s_scroll     = 0;

    SceUID fd = sceIoDopen(dir);
    if (fd < 0) return;

    SceIoDirent entry;
    while (sceIoDread(fd, &entry) > 0 && s_file_count < MAX_FILES) {
        if (entry.d_name[0] == '.') continue;
        if (SCE_S_ISDIR(entry.d_stat.st_mode)) {
            /* Prefix dirs with '/' for visual distinction */
            snprintf(s_files[s_file_count], NAME_LEN, "/%s", entry.d_name);
            s_file_count++;
        } else if (is_rom_ext(entry.d_name)) {
            strncpy(s_files[s_file_count], entry.d_name, NAME_LEN - 1);
            s_file_count++;
        }
    }
    sceIoDclose(fd);
    qsort(s_files, s_file_count, NAME_LEN, cmp_names);
    s_loaded = true;
}

/* ──────────────────────────────────────────────────────────────────────────
   Input debounce
   ────────────────────────────────────────────────────────────────────────── */
static uint32_t s_prev_buttons = 0;
#define PRESSED(btn) ((buttons & (btn)) && !(s_prev_buttons & (btn)))

/* ──────────────────────────────────────────────────────────────────────────
   Public API
   ────────────────────────────────────────────────────────────────────────── */
const char *rom_browser_update(uint32_t buttons) {
    if (!s_loaded) load_dir(ROM_PATH);

    const char *result = NULL;

    if (PRESSED(EMU_KEY_UP)) {
        if (s_cursor > 0) s_cursor--;
        if (s_cursor < s_scroll) s_scroll = s_cursor;
    }
    if (PRESSED(EMU_KEY_DOWN)) {
        if (s_cursor < s_file_count - 1) s_cursor++;
        if (s_cursor >= s_scroll + VISIBLE_ROWS) s_scroll = s_cursor - VISIBLE_ROWS + 1;
    }
    if (PRESSED(EMU_KEY_L)) {
        /* Page up */
        s_cursor -= VISIBLE_ROWS;
        if (s_cursor < 0) s_cursor = 0;
        s_scroll = s_cursor;
    }
    if (PRESSED(EMU_KEY_R)) {
        /* Page down */
        s_cursor += VISIBLE_ROWS;
        if (s_cursor >= s_file_count) s_cursor = s_file_count - 1;
        if (s_cursor >= s_scroll + VISIBLE_ROWS) s_scroll = s_cursor - VISIBLE_ROWS + 1;
    }
    if (PRESSED(EMU_KEY_A) && s_file_count > 0) {
        const char *name = s_files[s_cursor];
        if (name[0] == '/') {
            /* Enter subdirectory */
            char subdir[512];
            snprintf(subdir, sizeof(subdir), "%s%s", s_cur_dir, name);
            load_dir(subdir);
        } else {
            snprintf(s_selected, sizeof(s_selected), "%s/%s", s_cur_dir, name);
            result = s_selected;
        }
    }
    if (PRESSED(EMU_KEY_B)) {
        /* Navigate up */
        char *slash = strrchr(s_cur_dir, '/');
        if (slash && slash != s_cur_dir) {
            *slash = '\0';
            load_dir(s_cur_dir);
        }
    }

    s_prev_buttons = buttons;
    return result;
}

void rom_browser_draw(void) {
    vita2d_pgf *font = g_pgf_font;
    if (!font) return;

    /* Header */
    vita2d_pgf_draw_text(font, 20, 30, RGBA8(255, 215, 0, 255), 1.0f,
                         "GBVitaEX — Select ROM");
    vita2d_pgf_draw_text(font, 20, 55, RGBA8(180, 180, 180, 255), 0.8f,
                         s_cur_dir);

    /* Separator line */
    vita2d_draw_line(20, 65, VITA_SCREEN_W - 20, 65, RGBA8(80, 80, 80, 255));

    if (s_file_count == 0) {
        vita2d_pgf_draw_text(font, 40, 100, RGBA8(200, 100, 100, 255), 1.0f,
                             "No ROMs found. Place .gba/.gbc/.gb files in:");
        vita2d_pgf_draw_text(font, 40, 130, RGBA8(255, 255, 255, 255), 0.9f,
                             ROM_PATH);
        return;
    }

    int end = s_scroll + VISIBLE_ROWS;
    if (end > s_file_count) end = s_file_count;

    for (int i = s_scroll; i < end; i++) {
        int y = 80 + (i - s_scroll) * ROW_HEIGHT;
        unsigned bg   = (i == s_cursor) ? RGBA8(60, 120, 200, 200) : RGBA8(0, 0, 0, 0);
        unsigned col  = (i == s_cursor) ? RGBA8(255, 255, 255, 255)
                                        : RGBA8(200, 200, 200, 255);
        if (i == s_cursor)
            vita2d_draw_rectangle(20, y - 4, VITA_SCREEN_W - 40, ROW_HEIGHT - 2, bg);

        /* Colour dirs gold, files white */
        if (s_files[i][0] == '/')
            col = (i == s_cursor) ? RGBA8(255, 220, 80, 255) : RGBA8(200, 180, 60, 255);

        vita2d_pgf_draw_text(font, 30, y + 18, col, 0.9f, s_files[i]);
    }

    /* Scrollbar */
    if (s_file_count > VISIBLE_ROWS) {
        int bar_h  = (VISIBLE_ROWS * VISIBLE_ROWS * ROW_HEIGHT) / s_file_count;
        int bar_y  = 76 + (s_scroll * VISIBLE_ROWS * ROW_HEIGHT) / s_file_count;
        vita2d_draw_rectangle(VITA_SCREEN_W - 12, bar_y, 6, bar_h,
                              RGBA8(120, 120, 255, 200));
    }

    /* Controls hint */
    vita2d_pgf_draw_text(font, 20, VITA_SCREEN_H - 20,
                         RGBA8(150, 150, 150, 255), 0.75f,
                         "[Cross]=Open  [Circle]=Back  [L/R]=Page  [Select+Start]=Menu");
}

void rom_browser_reset(void) {
    s_loaded = false;
    load_dir(ROM_PATH);
}

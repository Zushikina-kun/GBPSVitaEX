/* GBAVitaEX — src/cheats/cheat_gba.c
 * GBA cheat loader — gpSP engine only.
 * Uses gpSP's cheat_parse(index, code_string) API.
 * Kept in a separate TU from cheat_gb.c to avoid header conflicts.
 */

/* gpSP headers must come first */
#include "retro_inline.h"
#include "common.h"    /* u8/u16/u32 and all gpSP includes */
#include "cheats.h"    /* cheat_parse, cheat_clear, MAX_CHEATS */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char *ltrim_gba(char *s) {
    while (isspace((unsigned char)*s)) s++;
    return s;
}
static void rtrim_gba(char *s) {
    char *e = s + strlen(s) - 1;
    while (e >= s && isspace((unsigned char)*e)) *e-- = '\0';
}

void cheat_gba_load_file(FILE *f) {
    cheat_clear();   /* reset previously loaded cheats */
    unsigned idx = 0;
    char line[256];

    while (fgets(line, sizeof(line), f) && idx < MAX_CHEATS) {
        char *s = ltrim_gba(line); rtrim_gba(s);
        if (*s == '#' || *s == '\0') continue;

        char type[4]   = {0};
        char code1[16] = {0};
        char code2[16] = {0};
        int  n = sscanf(s, "%3s %15s %15s", type, code1, code2);
        if (n < 2) continue;

        /* Only GBA-type prefixes */
        if (strcasecmp(type,"GS") && strcasecmp(type,"AR") && strcasecmp(type,"CB"))
            continue;

        /* gpSP wants "XXXXXXXX YYYYYYYY" or "XXXXX YYYY" (CodeBreaker) */
        char code[40] = {0};
        if (n >= 3 && code2[0])
            snprintf(code, sizeof(code), "%s %s", code1, code2);
        else
            snprintf(code, sizeof(code), "%s", code1);

        if (cheat_parse(idx, code) == CheatNoError)
            idx++;
    }
}

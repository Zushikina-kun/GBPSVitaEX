/* GBAVitaEX — src/cheats/cheat_gb.c
 * GB/GBC cheat loader — mGBA engine only.
 * Uses mGBA's mCheatAddLine API.
 * Kept in a separate TU from cheat_gba.c to avoid header conflicts.
 */

/* mGBA headers — include common.h first for CXX_GUARD_START */
#include <mgba-util/common.h>
#include <mgba/core/core.h>    /* struct mCore — needed to call ->cheatDevice() */
#include <mgba/core/cheats.h>

/* Forward declaration of the gb_engine accessor */
struct mCore *gb_engine_get_core(void);

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char *ltrim_gb(char *s) {
    while (isspace((unsigned char)*s)) s++;
    return s;
}
static void rtrim_gb(char *s) {
    char *e = s + strlen(s) - 1;
    while (e >= s && isspace((unsigned char)*e)) *e-- = '\0';
}

void cheat_gb_load_file(FILE *f) {
    struct mCore *s_core = gb_engine_get_core();
    if (!s_core) return;
    struct mCheatDevice *dev = s_core->cheatDevice(s_core);
    if (!dev) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *s = ltrim_gb(line); rtrim_gb(s);
        if (*s == '#' || *s == '\0') continue;

        char type[4]  = {0};
        char code[48] = {0};
        if (sscanf(s, "%3s %47s", type, code) < 2) continue;

        if (strcasecmp(type,"GB") && strcasecmp(type,"GG")) continue;

        /* Description: everything after "TYPE CODE " */
        char *desc = s;
        while (*desc && !isspace((unsigned char)*desc)) desc++;  /* skip type */
        while (*desc &&  isspace((unsigned char)*desc)) desc++;
        while (*desc && !isspace((unsigned char)*desc)) desc++;  /* skip code */
        while (*desc &&  isspace((unsigned char)*desc)) desc++;

        /* 0 = GameShark, 1 = Game Genie */
        int cheat_type = !strcasecmp(type, "GG") ? 1 : 0;

        struct mCheatSet *set = dev->createSet(dev, *desc ? desc : code);
        if (!set) continue;
        mCheatAddLine(set, code, cheat_type);
        mCheatAddSet(dev, set);
    }
}

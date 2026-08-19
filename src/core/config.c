/* GBAVitaEX — src/core/config.c
 * Persistent settings saved as a simple INI file.
 * Format:
 *   [settings]
 *   cpu_clock=444
 *   volume=80
 *   ...
 *   [keymap]
 *   cross=1
 *   circle=2
 *   ...  (value = EMU_KEY_* bitmask)
 */

#include "gbavitaex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *ltrim(char *s) { while(isspace((uint8_t)*s)) s++; return s; }
static void  rtrim(char *s) {
    char *e = s+strlen(s)-1;
    while(e>=s && isspace((uint8_t)*e)) *e--='\0';
}

bool config_save(void) {
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) return false;

    fprintf(f, "[settings]\n");
    fprintf(f, "cpu_clock=%d\n",       g_emu.cpu_clock_mhz);
    fprintf(f, "dynarec=%d\n",         (int)g_emu.dynarec_enabled);
    fprintf(f, "color_correct=%d\n",   (int)g_emu.color_correct);
    fprintf(f, "interframe=%d\n",      (int)g_emu.interframe_blend);
    fprintf(f, "frameskip=%d\n",       g_emu.frameskip);
    fprintf(f, "screen_mode=%d\n",     g_emu.screen_mode);
    fprintf(f, "audio_enabled=%d\n",   (int)g_emu.audio_enabled);
    fprintf(f, "volume=%d\n",          g_emu.audio_volume);
    fprintf(f, "ff_speed=%d\n",        g_emu.ff_speed_pct);
    fprintf(f, "ff_button=%d\n",       g_emu.ff_button);

    fprintf(f, "[keymap]\n");
    /* key_map indexed by VitaButton */
    static const char *keys[] = {
        "cross","circle","square","triangle","start","select",
        "up","down","left","right","l1","r1","l2","r2"
    };
    for (int i = 0; i < VBTN_COUNT; i++)
        fprintf(f, "%s=%u\n", keys[i], g_emu.key_map[i]);

    fclose(f);
    return true;
}

bool config_load(void) {
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return false;   /* no config yet, use defaults */

    static const char *vbtn_keys[] = {
        "cross","circle","square","triangle","start","select",
        "up","down","left","right","l1","r1","l2","r2"
    };

    char   line[128];
    int    section = 0; /* 0=none 1=settings 2=keymap */

    while (fgets(line, sizeof(line), f)) {
        char *s = ltrim(line); rtrim(s);
        if (*s == '#' || *s == ';' || *s == '\0') continue;

        if (*s == '[') {
            if (strncmp(s,"[settings]",10)==0)      section = 1;
            else if (strncmp(s,"[keymap]",8)==0)    section = 2;
            else                                    section = 0;
            continue;
        }

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = s;
        char *val = ltrim(eq+1);
        rtrim(key);

        if (section == 1) {
            int v = atoi(val);
            if      (!strcmp(key,"cpu_clock"))     g_emu.cpu_clock_mhz    = (v==333||v==444||v==500) ? v : 444;
            else if (!strcmp(key,"dynarec"))        g_emu.dynarec_enabled  = v != 0;
            else if (!strcmp(key,"color_correct"))  g_emu.color_correct    = v != 0;
            else if (!strcmp(key,"interframe"))     g_emu.interframe_blend = v != 0;
            else if (!strcmp(key,"frameskip"))      g_emu.frameskip        = (v>=0&&v<=5) ? v : 0;
            else if (!strcmp(key,"screen_mode"))    g_emu.screen_mode      = (v>=0&&v<3)  ? v : 0;
            else if (!strcmp(key,"audio_enabled"))  g_emu.audio_enabled    = v != 0;
            else if (!strcmp(key,"volume"))         g_emu.audio_volume     = (v>=0&&v<=100) ? v : 80;
            else if (!strcmp(key,"ff_speed"))       g_emu.ff_speed_pct     = (v>=FF_SPEED_MIN&&v<=FF_SPEED_MAX) ? v : 200;
            else if (!strcmp(key,"ff_button"))      g_emu.ff_button        = (v>=-1&&v<VBTN_COUNT) ? v : VBTN_R1;
        } else if (section == 2) {
            for (int i = 0; i < VBTN_COUNT; i++) {
                if (!strcmp(key, vbtn_keys[i])) {
                    g_emu.key_map[i] = (uint32_t)strtoul(val, NULL, 10);
                    break;
                }
            }
        }
    }

    fclose(f);
    return true;
}

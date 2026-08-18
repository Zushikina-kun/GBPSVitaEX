#pragma once
#include <stdint.h>
/* Returns the selected ROM path, or NULL if nothing chosen this frame. */
const char *rom_browser_update(uint32_t buttons);
void        rom_browser_draw(void);
void        rom_browser_reset(void);   /* go back to root ROM dir */

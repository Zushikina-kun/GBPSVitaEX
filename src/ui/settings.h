#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Returns true when user exits settings. */
bool settings_update(uint32_t buttons);
void settings_draw(void);
/* Returns true if user navigated to the key mapper from settings. */
bool settings_wants_keymapper(void);

/* Button remapper screen */
bool keymapper_update(uint32_t raw_sce_buttons);
void keymapper_draw(void);

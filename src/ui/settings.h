#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Returns true when the user exits the settings screen. */
bool settings_update(uint32_t buttons);
void settings_draw(void);

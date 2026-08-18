#pragma once
#include <stdbool.h>
bool savestate_mgr_save(int slot);
bool savestate_mgr_load(int slot);
bool savestate_mgr_has_slot(int slot);

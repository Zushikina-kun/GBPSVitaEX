/* GBAVitaEX — src/core/emu_core.h
 * Central dispatch: ROM detection, core selection, per-frame loop.
 */
#pragma once
#include "gbavitaex.h"

/* Detect ROM type from file magic and header; returns CORE_NONE on failure. */
EmuCore emu_detect_rom(const char *path);

/* Load a ROM: detect type, init the right core, reset.
 * Returns true on success. */
bool emu_load_rom(const char *path);

/* Unload current ROM, free core resources. */
void emu_unload_rom(void);

/* Run exactly one video frame.  Call once per vsync. */
void emu_run_frame(void);

/* Feed current Vita button state (raw SCE_CTRL_* bitmask + analog). */
void emu_set_input(uint32_t buttons, uint8_t lx, uint8_t ly);

/* Save / load SRAM to the default path derived from the ROM filename. */
bool emu_save_sram(void);
bool emu_load_sram(void);

/* Save states (slot 0-9). */
bool emu_save_state(int slot);
bool emu_load_state(int slot);

/* Reset the current ROM (soft reset). */
void emu_reset(void);

/* Shutdown all core resources (called on app exit). */
void emu_core_shutdown(void);

/* Get current pointer to the finished RGB565 frame pixel data.
 * Width / height match the active system (GBA: 240×160, GB: 160×144). */
const uint16_t *emu_get_framebuffer(int *out_w, int *out_h);

/* Drain audio samples into a caller-supplied PCM-16 stereo buffer.
 * Returns number of stereo frames written. */
int  emu_drain_audio(int16_t *buf, int max_frames);

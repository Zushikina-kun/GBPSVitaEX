/* GBAVitaEX — src/gb/gb_engine.h
 * mGBA GB/GBC engine wrapper.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

bool            gb_engine_load(const char *rom_path, const char *save_path,
                               bool is_gbc);
void            gb_engine_unload(void);
void            gb_engine_reset(void);
void            gb_engine_run_frame(void);
void            gb_engine_set_input(uint32_t emu_keys);
const uint16_t *gb_engine_get_framebuffer(void);
int             gb_engine_drain_audio(int16_t *buf, int max_frames);
bool            gb_engine_save_sram(const char *path);
bool            gb_engine_load_sram(const char *path);
bool            gb_engine_save_state(const char *path);
bool            gb_engine_load_state(const char *path);
void            gb_engine_shutdown(void);
/* Returns the active mCore instance (NULL if no ROM loaded).
 * Used by cheat_gb.c to access the cheat device. */
struct mCore   *gb_engine_get_core(void);

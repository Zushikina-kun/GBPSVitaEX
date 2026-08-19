/* GBVitaEX — src/gba/gba_engine.h
 * gpSP GBA engine wrapper (standalone — no libretro).
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

bool            gba_engine_load(const char *rom_path, const char *save_path);
void            gba_engine_unload(void);
void            gba_engine_reset(void);
void            gba_engine_run_frame(void);
void            gba_engine_set_input(uint32_t emu_keys);
const uint16_t *gba_engine_get_framebuffer(void);
int             gba_engine_drain_audio(int16_t *buf, int max_frames);
bool            gba_engine_save_sram(const char *path);
bool            gba_engine_load_sram(const char *path);
bool            gba_engine_save_state(const char *path);
bool            gba_engine_load_state(const char *path);
void            gba_engine_shutdown(void);

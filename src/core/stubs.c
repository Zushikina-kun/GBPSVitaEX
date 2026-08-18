/* GBAVitaEX — src/core/stubs.c
 * Definitions for symbols normally provided by gpSP's libretro.c and
 * mGBA's CMake-generated version.c — both of which we exclude from the
 * build.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Pull in gpSP types so the declarations below match exactly */
#include "common.h"   /* u8/u16/u32, fixed types, boot_mode, etc. */
#include "cpu.h"      /* MAX_TRANSLATION_GATES */

/* ──────────────────────────────────────────────────────────────────────────
   gpSP globals (normally in vendor/gpsp/libretro/libretro.c)
   ────────────────────────────────────────────────────────────────────────── */

/* JIT dynarec toggle: 1 = use dynarec, 0 = interpreter.
 * gba_engine.c sets this after JIT cache allocation. */
int dynarec_enable = 0;

/* Idle-loop optimisation target PC */
u32 idle_loop_target_pc = 0xFFFFFFFF;

/* Translation gate: holds PC values where the dynarec yields back to the
 * update_gba scheduler so it can process timers/DMA mid-block. */
u32 translation_gate_target_pc[MAX_TRANSLATION_GATES];
u32 translation_gate_targets = 0;

/* Frame skip flag — set by frameskip logic, read by video_run */
u32 skip_next_frame = 0;

/* Boot mode: boot_game means skip BIOS animation (default) */
boot_mode selected_boot_mode = boot_game;

/* Sprite limit: 1 = hardware limit (128 sprites/frame), 0 = unlimited */
int sprite_limit = 1;

/* Netplay / RFU stubs — not used in standalone Vita build */
u32 netplay_client_id   = 0;
u32 netplay_num_clients = 0;

void netpacket_send(uint16_t client_id, const void *buf, size_t len) {
    (void)client_id; (void)buf; (void)len;
}

void netpacket_poll_receive(void) {
    /* no-op: no network in standalone build */
}

/* Fast-forward override from RetroArch — no-op standalone */
void set_fastforward_override(bool fastforward) {
    (void)fastforward;
}

/* ──────────────────────────────────────────────────────────────────────────
   mGBA version strings (normally generated from version.c.in by CMake)
   ────────────────────────────────────────────────────────────────────────── */
const char* const projectName    = "GBAVitaEX";
const char* const projectVersion = "1.0.0";

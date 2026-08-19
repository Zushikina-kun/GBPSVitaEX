/* GBVitaEX — src/core/stubs.c
 * Definitions for symbols normally provided by gpSP's libretro.c and
 * mGBA's CMake-generated version.c — both of which we exclude from the build.
 *
 * NOTE: netpacket_send / netpacket_poll_receive / netplay_client_id /
 *       netplay_num_clients are now defined in rfu_vita_net.c (real UDP
 *       implementation). They must NOT be defined here too.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Pull in gpSP types */
#include "common.h"   /* u8/u16/u32, fixed types, boot_mode, etc. */
#include "cpu.h"      /* MAX_TRANSLATION_GATES */

/* ──────────────────────────────────────────────────────────────────────────
   gpSP globals (from libretro.c)
   ────────────────────────────────────────────────────────────────────────── */

/* JIT dynarec toggle — gba_engine.c sets this after JIT cache allocation */
int dynarec_enable = 0;

/* Idle-loop optimisation target PC — 0xFFFFFFFF = none detected */
u32 idle_loop_target_pc = 0xFFFFFFFF;

/* Translation gates: PC values where the dynarec yields mid-block */
u32 translation_gate_target_pc[MAX_TRANSLATION_GATES];
u32 translation_gate_targets = 0;

/* Frame skip flag — set by our frameskip logic in gba_engine_run_frame() */
u32 skip_next_frame = 0;

/* Boot mode: boot_game = skip BIOS animation (default) */
boot_mode selected_boot_mode = boot_game;

/* Sprite limit: 1 = 128-sprite hardware cap, 0 = unlimited */
int sprite_limit = 1;

/* Fast-forward override hook (RetroArch-specific, no-op in standalone) */
void set_fastforward_override(bool fastforward) {
    (void)fastforward;
}

/* ──────────────────────────────────────────────────────────────────────────
   mGBA version strings (from CMake-generated version.c.in)
   ────────────────────────────────────────────────────────────────────────── */
const char* const projectName    = "GBVitaEX";
const char* const projectVersion = "1.4.0";

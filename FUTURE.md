# MGBAVitaEX — Future Plans & Technical Roadmap

This document records the research, decisions, and recommendations for future improvements.
Updated after v2.1.3.

---

## Current performance ceiling — why it exists

mGBA uses a **pure C interpreter** for the GBA ARM7TDMI CPU. Every single GBA instruction
requires: read opcode from memory, decode, dispatch through a C function pointer vtable,
execute, update cycle counter. On the Vita's Cortex-A9 this costs roughly 8–12 host CPU
cycles per GBA instruction.

The GBA needs ~280,896 cycles per frame × 60fps = ~16.8 million GBA cycles/sec for simple
games, up to 50M+ for CPU-heavy scenes. At 444 MHz / ~10 cycles per instruction, the Vita
has ~44 million GBA instructions/sec of headroom — thin for demanding games.

PSP emulators (gpSP, TempGBA) were faster because they had a **JIT recompiler** that
translated GBA ARM code to native ARM/MIPS code at runtime. A JIT batch-processes 4–10 GBA
instructions into direct host instructions, costing ~2–4 host cycles per GBA instruction
instead of 8–12. That's a **3–5× speedup** — the difference between 40fps and 60fps for
most games that currently struggle.

---

## Priority 1 — JIT dynarec (highest impact)

### What it is

A Just-In-Time recompiler translates GBA ARM7TDMI machine code into native Cortex-A9
ARMv7 machine code at runtime. Translated blocks are cached. On re-entry to the same
address the native code runs directly — no decode, no dispatch, no vtable overhead.

### Expected gain

- **3–5× CPU throughput** for GBA emulation
- Near-100% full-speed (60fps) for the entire GBA library, including currently-struggling
  titles (Fire Emblem, some Castlevania sections, Pokémon in dense areas)
- GB/GBC games already run at full speed (interpreter is fast enough); JIT benefits GBA only

### Feasibility on PSVita

The Vita enforces W^X (write-XOR-execute) memory. JIT code must use the dedicated API:

```c
#include <psp2/kernel/sysmem.h>

// 1. Allocate JIT-capable memory
SceUID block = sceKernelAllocMemBlockForVM("jit_cache", size);
void *base;
sceKernelGetMemBlockBase(block, &base);

// 2. Open for writing, emit code, close, sync icache
sceKernelOpenVMDomain();
emit_arm_code(base, ...);
sceKernelCloseVMDomain();
sceKernelSyncVMDomain(block, base, bytes_written);

// 3. Execute (set LSB=1 for Thumb entry)
((void (*)(void))((uintptr_t)base | 1))();
```

This API is confirmed working — it is used by at least VitaExhumed and documented by
yifanlu. There is no barrier to JIT on Vita from the OS side.

### Implementation paths (ranked by effort)

**Option A — Native VitaSDK port of gpSP (lowest effort, fastest result)**

gpSP's libretro fork already has a complete, actively-maintained ARMv7 dynarec in
`arm/arm.s`. It targets the same ISA as the Vita's Cortex-A9. Current Vita usage is
via Adrenaline (PSP emulation layer) — a native VitaSDK port doesn't exist yet.

Work required:
1. Replace `mmap(PROT_EXEC)` / `__clear_cache()` calls with `sceKernelAllocMemBlockForVM`
   + `sceKernelOpenVMDomain` + `sceKernelSyncVMDomain` (approximately 3 call sites)
2. Wire up the PSP2 input, vita2d display, and sceAudioOut backend (reuse our
   MGBAVitaEX psp2-context.c work as a template)
3. Test the ARM dynarec block cache with `HAVE_DYNAREC` enabled

Trade-off: gpSP is less accurate than mGBA. No reliable RTC, some audio edge cases,
a few games are broken. But it runs the vast majority of the GBA library perfectly.

Estimated effort: **2–4 weeks** for someone already familiar with VitaSDK.

**Option B — Resume mGBA ARMv7 dynarec PR #378 (best accuracy + speed)**

GitHub: https://github.com/mgba-emu/mgba/pull/378 (author: merryhime, status: abandoned WIP)

What was completed in the PR:
- Phase 1: faithful interpreter reproduction ✓
- Phase 2: register cache (maps GBA R0–R7 → host ARM R4–R11) ✓
- Phase 3: deferred PC/prefetch updates ✓
- Phase 4: branch target prediction ✓

What is still missing:
- Block linking (chaining translated blocks without returning to dispatcher)
- Idle loop detection in the JIT path
- Self-modifying code handling (invalidate cache on VRAM/IWRAM writes)
- Thumb/ARM interworking edge cases
- Vita VM domain integration (replace generic JIT memory allocator)

Estimated effort: **2–4 months** for one developer with ARM JIT experience. The hardest
design work (register allocation, code generation) is already done.

This is the **recommended long-term path** — mGBA accuracy + JIT speed in one app.

**Option C — Integrate existing dynarec library**

Dynaram / ArFM / similar ARM→ARM recompiler libraries exist but are not maintained
and have no mGBA integration. Not recommended.

### Recommendation

Start with **Option A** (gpSP native VitaSDK port) as a separate app to validate the
JIT memory API and measure real-world speedup. If the result is satisfactory, use that
experience to tackle **Option B** (resume mGBA PR #378) for the accuracy benefits.

---

## Priority 2 — OpenGL high-res renderer via PVR_PSP2 (visual enhancement, not a speed fix)

### What it is

mGBA 0.8.0+ includes an OpenGL renderer that renders GBA frames at arbitrary integer
scales (2×, 4×, 8×) on the GPU, with hardware color correction and optional shaders.
It is available on PC, macOS, Switch — but not the Vita port.

The PSVita has a **PowerVR SGX543MP2 GPU**. The
[PVR_PSP2 project](https://github.com/GrapheneCt/PVR_PSP2) by GrapheneCt is a complete
port of the PowerVR SGX DDK 1.8 to Vita, providing:
- PVR2D (completed)
- OpenGL ES 1.1 (completed)
- OpenGL ES 2.0 (completed)
- Confirmed working: pvr2d_test, gles2test1 pass

SDL3 on Vita supports GLES via `-DVIDEO_VITA_PVR=ON` when built with PVR_PSP2.

### What this would give us

- Render GBA frames at 2× (480×320) or native Vita resolution with upscaling
- Hardware color correction (GBA LCD gamma curve)
- CRT / LCD pixel shader filters
- Sharper image in fullscreen mode vs vita2d nearest-neighbour

### What it would NOT give us

This is **not a performance improvement**. mGBA's software renderer uses ~3% of frame
time. The bottleneck is the CPU interpreter. Even a 10× slower renderer would be
imperceptible vs a 1.1× faster interpreter.

### Feasibility

Medium-high effort. Requires:
1. Adding PVR_PSP2 as a build dependency (separate from VitaSDK standard libs)
2. Enabling mGBA's OpenGL renderer (currently compiled out for PSP2 builds via
   `MINIMAL_CORE` / `BUILD_GL` flags)
3. Writing a GXM/GLES2 display backend or using SDL3+VITA_PVR as the window system
4. Pre-compiling any GLSL shaders to GXP bytecode with `psp2spvc`

Estimated effort: **4–8 weeks** once the JIT is working and the platform is stable.

### Recommendation

**Do this after JIT** as a cosmetic/visual enhancement pass. Not worth pursuing before
the speed problem is solved — a blurry-but-fast emulator is better than a sharp-but-slow one.

---

## Priority 3 — GPU-accelerated GBA PPU rendering (not recommended)

### What it is

Replace mGBA's software tile renderer with GPU shaders that read tile maps, tile data,
and OAM directly to produce each scanline on the GPU. Projects like DSHBA and webgba
do this on PC with OpenGL 3.3 / WebGL.

### Why we are not doing this

1. The software renderer uses **~3% of frame time** — it is not the bottleneck
2. The GBA's PPU has non-standard blend modes that don't map cleanly to GPU blend ops
   (requires rendering layers twice and compositing)
3. GXM shaders must be written in PSSL/GLSL and compiled to GXP bytecode with Sony's
   offline shader compiler — not standard GLSL
4. Estimated effort: 3–9 months for correct output, all for a 3% potential gain

**Verdict: wrong optimization target. Skip entirely.**

---

## Priority 4 — Back-touch L2/R2/L3/R3 mapping (mGBA issue #3054)

The Vita's back touchpad can be divided into four zones (top-left, top-right,
bottom-left, bottom-right) and exposed as virtual L2/R2/L3/R3 buttons. mGBA's PSP2
back-touch reader (`mPSP2ReadTouchLR` in psp2-context.c) already reads back-touch,
but the remap UI currently only exposes L3/R3 (analog stick clicks) as names — the
back-touch zones are not individually labeled or exposed for remapping.

Fix: expose all 4 back-touch zones as separate named inputs in `keySources.keyNames`
and wire them through `mPSP2ReadTouchLR` with distinct bit positions.

Estimated effort: **1–2 days**.

---

## Priority 5 — ROM browser L/R page skip (mGBA issue #3039)

The ROM browser currently navigates one item at a time. L/R triggers should jump to
the first/last item in the current directory (or page forward/back). This requires a
small change to `src/util/gui/file-select.c` in the mGBA source.

Estimated effort: **half a day**.

---

## What we have already done (v2.0.0 – v2.1.2)

| Fix | Version | Details |
|---|---|---|
| Rebase on mGBA PSVita port | v2.0.0 | Replace custom engine with battle-tested mGBA base |
| sceKernelPowerTick (auto-sleep fix) | v2.0.0 | Fixes issue #1970 |
| Audio buffer increase (crackling fix) | v2.0.0 | Buffers 16→24, samples 512→1024; fixes #3044 |
| Default save/state directories | v2.0.0 | ux0:data/mGBA/saves/ and states/ |
| Triangle freed from menu key | v2.0.0 | Issue #3039 |
| FF mute + FPS counter options | v2.0.0 | configExtra additions |
| Rename to MGBAVitaEX | v2.1.0 | |
| Fast-forward default binding | v2.1.0 | R Trigger = held, L Trigger = toggle |
| SELECT+START menu combo | v2.1.0 | _pollInput 30-frame counter |
| CPU clock speed option in Settings | v2.1.0 | 333 / 444 MHz |
| idleOptimization = detect default | v2.1.0 | Pokémon save lag improvement |
| GPU clock raised to 222 MHz | v2.1.1 | From OS default 111 MHz |
| Bus clock raised to 222 MHz | v2.1.1 | |
| threadedVideo.flushScanline = 0 | v2.1.1 | Batched scanline rendering on core 1 |
| Bilinear filter off by default | v2.1.1 | Removes per-frame CPU texture padding |
| Interframe blending off by default | v2.1.1 | One draw call per frame instead of two |
| Plain text remap button labels | v2.1.2 | "Triangle" instead of icon glyph |
| Triangle = A, Square = B defaults | v2.1.2 | Both fully remappable |
| Dual Primary+Alt bindings per key | v2.1.3 | mgbaex_altGameMap + mgbaex_altGuiMap, merged at poll time |
| "Open in-game menu" labelled + configurable | v2.1.3 | Visible in Interface keys section with both slots |
| Interface key labels renamed for clarity | v2.1.3 | Cancel→"Open in-game menu", Menu: prefixes, FF at top |

---

## Known limitations that cannot be fixed without JIT

| Issue | Root cause | Fix |
|---|---|---|
| Sub-60fps in CPU-heavy GBA scenes | Pure C interpreter, ~10 cycles/instruction | JIT dynarec |
| Audio dropouts when frame drops | Audio ring buffer starves when emulation is slow | JIT (fixes the root cause) |
| FF speed ratio (2×, 3× etc.) | PSP2 FF is unbounded; ratio requires sync target changes | Medium effort, separate task |

---

## References

- mGBA ARMv7 dynarec WIP: https://github.com/mgba-emu/mgba/pull/378
- Vita JIT memory API: https://gist.github.com/yifanlu/43a35324f3b76391cd15c6b96ae8b831
- gpSP ARM dynarec: https://github.com/libretro/gpsp/tree/master/arm
- PVR_PSP2 (OpenGL ES 2.0 on Vita): https://github.com/GrapheneCt/PVR_PSP2
- DSHBA GPU-accelerated GBA PPU (PC reference): https://github.com/DenSinH/DSHBA
- mGBA PSVita open issues: https://github.com/mgba-emu/mgba/issues?q=psp2+OR+vita

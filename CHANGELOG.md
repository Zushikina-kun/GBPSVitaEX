# Changelog

All notable changes to MGBAVitaEX will be documented in this file.

---

## [2.1.2] — 2026-08-18

### Fixed

**Remap screen showed icon glyphs instead of button names**
- Triangle, Circle, Cross, Square were displayed as raw font icon bytes (`\1\xC` etc.)
  instead of readable names in the Remap screen.
- Now shows plain text: "Triangle", "Circle", "Cross", "Square", "L Trigger", "R Trigger".

**Triangle and Square had no default game bindings**
- Triangle is now mapped to GBA A by default (second A button — useful for many games).
- Square is now mapped to GBA B by default (second B button).
- Both are fully remappable in Remap → Game keys.

---

## [2.1.1] — 2026-08-18

### Performance improvements

**GPU clock raised to 222 MHz** (from OS default 111 MHz)
- `scePowerSetGpuClockFrequency(222)` — vita2d texture blits, `vita2d_clear_screen()`
  and buffer swaps are all GPU operations. At 111 MHz they consumed a noticeable
  fraction of the 16.7ms frame budget. At 222 MHz the GPU completes drawing sooner,
  leaving more time for the CPU interpreter.
- `scePowerSetBusClockFrequency(222)` — bus clock also raised to 222 MHz.

**`threadedVideo.flushScanline = 0` default**
- The video proxy thread (enabled by `threadedVideo=1`) now batches all dirty scanline
  updates per frame instead of flushing one at a time. Reduces mutex contention between
  the emulation thread and the renderer thread running on the second Cortex-A9 core.

**Bilinear filtering off by default**
- Bilinear mode required per-frame CPU pixel-padding: 160 pixel writes (column seam)
  + 1024-byte memcpy (row seam) per frame to prevent texture bleed at the 240→256 and
  160→256 texture boundaries. Nearest-neighbour has no such cost.
- Re-enable in Configure → Screen filtering if you prefer smooth scaling.

**Interframe blending off by default**
- Blending submitted two vita2d draw calls per frame (previous + current frame
  composited). Off by default = one draw call per frame.
- Re-enable in Configure if you want LCD ghosting simulation.

**Frameskip default explicitly set to 0**
- Ensures a clean baseline. Configure → Frameskip can be raised to 1 or 2 for games
  that still struggle after the above improvements.

---

## [2.1.0] — 2026-08-18

### Project renamed: GBVitaEX → MGBAVitaEX

Reflects the actual engine used (mGBA's PSVita port).

### Fixed

**Fast-forward was not working**
- `mGUI_INPUT_FAST_FORWARD_HELD` and `mGUI_INPUT_FAST_FORWARD_TOGGLE` now have default
  bindings set at startup via `mInputBindKey`.
- R Trigger (hold) = fast-forward while held. L Trigger (press) = toggle on/off.
- Both are rebindable in Remap. Previously no default was set — fast-forward appeared
  broken unless the user manually assigned a button.

**Triangle hardcoded as menu key (mGBA issue #3039)**
- Stock mGBA PSVita mapped `SCE_CTRL_TRIANGLE` to `GUI_INPUT_CANCEL` (the key that
  opens the in-game menu). That mapping could not be removed.
- Triangle is now fully free to be assigned to any game button.

**No way to open menu without Triangle**
- Holding SELECT + START for ~0.5 seconds now opens the in-game menu.
- Implemented as a 30-frame counter in `_pollInput` — fires `GUI_INPUT_CANCEL` once,
  then requires release before firing again.

**Pokemon Emerald save lag**
- `idleOptimization = "detect"` set as default. mGBA's idle loop detector eliminates
  busy-wait loops in RSE, reducing CPU work during Flash 128 KB erase/write.

### Added

**CPU Clock Speed setting in Configure menu**
- 333 MHz (battery saver) or 444 MHz (default).
- Applied via `scePowerSetArmClockFrequency` on every launch from config.

### Changed

- App LiveArea title renamed to MGBAVitaEX.
- TITLEID remains `GBVX00001` — upgrade replaces existing entry, saves untouched.
- VPK filename: `MGBAVitaEX-v2.1.0.vpk`.

---

## [2.0.0] — 2026-08-17

### Project rebase: custom engine → mGBA PSVita port

The original custom engine (gpSP JIT + mGBA hybrid) had an untested dynarec, crashes on
ROM load, and a broken UI. Rebased on mGBA's official PSVita port which ships working VPKs
and has a battle-tested mGUI ROM browser.

### Added / patched on top of mGBA base

- `sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND)` every frame — fixes
  Vita auto-sleep during gameplay (mGBA issue #1970)
- Audio buffer 512→1024, BUFFERS 16→24 — reduces crackling (#3044)
- Default save dir: `ux0:data/mGBA/saves/` — saves no longer clutter ROM folder (#3039)
- Default state dir: `ux0:data/mGBA/states/`
- Triangle freed from `GUI_INPUT_CANCEL` — Triangle no longer hardcoded as menu key (#3039)
- configExtra additions: Mute during fast-forward, Show FPS counter
- TITLEID set to `GBVX00001`
- `PSP2` added to `OS_DEFINES` (fixes VFS/directory on Vita)
- `BUILD_LTO=OFF` — LTO produces bitcode-only `.a` files that break `--start-group`
- `PATH_MAX` guard in `directories.h` for VitaSDK build compatibility

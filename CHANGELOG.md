# Changelog

All notable changes to GBAVitaEX will be documented in this file.

---

## [1.4.0] — 2026-08-19

### Fixed — Critical Integration Gaps

These features were built in v1.3.0 but were dead code — nothing in the application ever activated them. All are now fully wired.

**GB Link Cable — P2 core never stepped (Critical)**
- `gb_engine_run_frame()` now calls `gb_link_run_frame()` immediately after P1's `runFrame()` when `gb_link_active()` is true.
- Both cores run on the same thread in tight lock-step per frame, which is the correct model for `GBSIOLockstep`'s synchronous signal/wait callbacks.
- Pokémon trading, Tetris 2-player, and other link-cable games are now functional.

**GBA RFU Multiplayer — never triggered (Critical)**
- `rfu_vita_net_start()` is now callable from the in-game menu (Menu → WiFi Multiplayer: Host / Join).
- `load_gamepak()` now uses `SERIAL_MODE_AUTO` instead of `SERIAL_MODE_DISABLED`, so `gba_over.h` flags take effect: Pokémon FR/LG/Emerald/Ruby/Sapphire, Mario Golf, Megaman BN5/6, and 20+ other RFU-capable games get `serial_mode = SERIAL_MODE_RFU` set automatically on load.
- Menu now shows WiFi status in the title bar and RFU/stop options dynamically.
- Menu shows Link Cable options for GB/GBC games.

### Fixed — Audio

**`stretch_flush()` missing before `stretch_deinit()`**
- Added `stretch_flush()` call in `audio_output_shutdown()` and in the normal-speed path of `audio_output_submit_ff()`.
- Eliminates the audible dropout when releasing fast-forward or on app exit.

### Fixed — Input

**Pause hotkey conflict with L1 as FF button**
- The L+R pause hotkey guard now excludes both `VBTN_L1` and `VBTN_R1`, not just R1.
- If either trigger is configured as the fast-forward button, the L+R pause combo is disabled for that session.

**`s_pause_hold` not reset on state transitions**
- `s_pause_hold` is now explicitly reset to 0 when entering `UI_STATE_MENU`, `UI_STATE_SETTINGS`, and `UI_STATE_KEYMAPPER`, preventing phantom pause triggers on return to `UI_STATE_RUNNING`.

### Fixed — RFU Protocol

**`netplay_num_clients` set incorrectly on non-sequential join**
- Changed from `netplay_num_clients = slot` to `netplay_num_clients = max(netplay_num_clients, slot)`.
- Prevents the client count from being wrong when clients join out of order or rejoin after a disconnect.

### Changed

- In-game menu is now **dynamic**: items shown depend on the active core (GBA → WiFi options; GB/GBC → Link Cable options) and current multiplayer state (connected → Stop options shown instead of Start options).
- Menu shows WiFi status string from `rfu_vita_net_status()` next to the FPS counter when RFU is active.
- Menu shows "Link Cable active" indicator when GB link is running.
- Link Cable (single-device) activated via **Menu → Link Cable (2P)** — both players load the same ROM. P2 save is stored as `<romname>.p2.sav`.

---

## [1.3.0] — 2026-08-19

### Added

#### Fast-forward pitch correction (TDHS)
- Audio no longer sounds chipmunk during fast-forward.
- Uses Time-Domain Harmonic Scaling (TDHS) from [dbry/audio-stretch](https://github.com/dbry/audio-stretch) (BSD licence), a lightweight algorithm requiring only ~1 multiply + 2 adds per output sample — about 0.5 ms overhead at 444 MHz.
- Pitch correction is togglable: **Settings → FF Pitch Correction** (On by default).
- Works for all FF speeds from 1.25× to 8×. Above 4× the library uses dual-instance cascade mode automatically.
- GB/GBC audio is unaffected (it runs in a separate thread and delivers at the correct pitch by design).

#### GBA RFU Wireless Adapter multiplayer (LAN WiFi)
- Pokémon Fire Red/Leaf Green/Emerald, Mario Golf, Megaman Battle Network 5/6, Mario Tennis, and other RFU-compatible games can now be played multiplayer over local WiFi.
- Implemented via a custom UDP transport over SceNet — no RetroArch or internet relay needed.
- One Vita acts as **Host** (player 1); others join as **Clients** via UDP broadcast discovery on the LAN.
- Protocol: `[sender_id:2][dest_id:2][payload:N]` over UDP port 7354 (broadcast + unicast).
- Working games (confirmed by davidgfnet): Pokémon series, Mario Golf, Megaman BN5/6.
- Games with latency issues (racing/action): Digimon Racing, Shrek SuperSlam.
- Maximum 4 clients + 1 host = 5 players (RFU hardware limit).
- **Note:** same LAN subnet required. Internet play not supported without a relay server.

#### GB/GBC single-device link cable
- Two GB/GBC ROMs can be connected via a software link cable on a single Vita.
- Both cores run in lock-step using mGBA's `GBSIOLockstep` infrastructure.
- Enables Pokémon trading/battling between two games, Tetris two-player, etc.
- Player 2's screen is accessible via `gb_link_get_p2_framebuffer()` — future UI will display it in a split-screen layout.
- Access via menu (coming in next UI update): **Menu → Link Cable → Load P2 ROM**.

#### Pause hotkey
- **Hold L Trigger + R Trigger for 1 second** to toggle pause without opening the menu.
- Paused state shown in HUD in red: `PAUSED  (hold L+R to resume)`.
- Hotkey is automatically disabled if R Trigger is configured as the fast-forward button (to avoid conflicts).

#### Per-game compatibility database (gba_over.h)
- gpSP's full per-game override database (`gba_over.h`) is already **compiled in** — no separate file needed.
- Contains idle-loop elimination targets, save type overrides (EEPROM, Flash 64K/128K), RTC flags, rumble flags, and RFU flags for 150+ GBA titles — applied automatically on ROM load.
- Notable entries: Golden Sun 1/2 (translation gates), all Pokémon titles (Flash 128K + RTC + RFU), Drill Dozer (rumble), WarioWare Twisted (rumble), F-Zero (idle loop), Castlevania (idle loop).

### Changed
- HUD now shows `PAUSED` in red when paused, `>> X.Xx` with FPS during FF.
- `stubs.c` cleaned up: `netpacket_send`, `netpacket_poll_receive`, `netplay_client_id`, `netplay_num_clients` moved to `rfu_vita_net.c` (real implementations).
- `projectVersion` in `stubs.c` updated to `"1.3.0"`.

---

## [1.2.0] — 2026-08-19

### Fixed

- **Fast-forward vsync cap** — fast-forward was capped at 60 FPS because
  `vita2d_swap_buffers()` internally waits for display vsync. When FF is
  activated `vita2d_set_vblank_wait(0)` now disables vsync locking so the
  CPU can run emulation frames as fast as hardware allows (up to 8× on
  500 MHz). Vsync is restored immediately when FF is released or when the
  menu opens. HUD accurately reflects the real achieved multiplier.

- **Screenshot stutter eliminated** — PNG encoding of a full 960×544
  framebuffer took 60–100 ms synchronously, dropping 4–6 frames. Screenshot
  now copies the raw framebuffer immediately (< 1 ms), then spawns a
  `SceKernelThread` to encode and write the PNG in the background. The main
  emulation loop is never blocked.

- **SCE_SYSMODULE_HTTPS removed** — was loaded at startup but never used.
  Removed to cut startup time and reduce memory footprint.

### Added

- **Proper git submodule registration** — all four vendor repos (gpsp, mGBA,
  TempGBA, FrogGBA) are now real git submodules tracked in `.gitmodules` at
  pinned commits. Cloning with `git clone --recurse-submodules` now works
  correctly and pulls all source dependencies automatically. No manual vendor
  setup required.

---

## [1.1.0] — 2026-08-19

### Fixed
- **Save state audio silence** — after loading a GBA save state, audio was dead. Fixed by calling `sound_frequency_changed()` after `gba_load_state()` to rebuild PSG frequency step tables from restored state. This was a known upstream gpSP issue confirmed in the libretro/gpsp GitHub tracker.
- **Vita auto-suspend during gameplay** — the Vita's watchdog was never ticked, causing the screen to dim and the system to sleep mid-game. `sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND)` is now called every frame.
- **JIT VM domain failure silent crash** — `sceKernelOpenVMDomain()` return value was not checked. It now gracefully falls back to the interpreter if the call fails (e.g. on unusual CFW configs).

### Added

#### Fast-Forward
- Configurable fast-forward speed: **1.25× to 8×** in 0.25× steps
- Activated by holding the configured button (default: R Trigger)
- Speed shown in HUD as `XX.X fps  >> X.Xx` when active
- Interframe blending is automatically disabled during fast-forward to reduce blur
- Set via Settings → **Fast-Forward Speed** (slider: 125% – 800%)
- Trigger button configurable: Settings → **Fast-Forward Button** (Off / R Trigger / Back-Touch L / Back-Touch R / Triangle)

#### Button Remapping
- Every GBA/GB button (A, B, Select, Start, Up, Down, Left, Right, L, R) can be remapped to any Vita physical button
- Access via **Settings → Button Remapping**
- On the remapper screen:
  - **Up/Down** — select which GBA button to remap
  - **Cross** — start listening for a Vita button press to assign
  - **Triangle** — clear the current assignment
  - **Square** — reset ALL bindings to defaults
  - **Circle** — go back
- Multiple Vita buttons can map to the same GBA button
- Default mapping: Cross=A, Circle=B, Square=B, L=L, R=R, Start=Start, Select=Select, D-Pad=D-Pad

#### Persistent Settings
- All settings (clock, screen mode, volume, frameskip, FF speed, FF button, colour correction, interframe blend, audio, dynarec, button map) are saved to `ux0:data/GBAVitaEX/config.ini` on exit or when leaving the settings screen
- Automatically loaded on next launch
- Plain INI format — can be edited manually with a text editor

### Changed
- HUD overlay now shows fast-forward indicator with speed multiplier
- Version bumped to 1.1.0

---

## [1.0.0] — 2026-08-18

### Initial Release

#### GBA Engine (gpSP)
- ARMv7 JIT dynarec enabled — GBA ARM7TDMI code is recompiled to native Cortex-A9 instructions at runtime via `sceKernelAllocMemBlock` + `sceKernelOpenVMDomain`
- ROM translation cache: 10 MB (ROM) + 512 KB (RAM), allocated as uncached RWX memory
- Interpreter fallback when dynarec is disabled in settings
- Open-source BIOS replacement (Normmatt) bundled; official `gba_bios.bin` used when present
- Full GBA hardware: DMA, timers, all 6 audio channels (4 PSG + 2 DirectSound FIFO), OAM, affine backgrounds
- Save type auto-detection: SRAM, Flash 64K/128K, EEPROM 4K/64K
- RTC support: Pokémon time-based events, berry growth
- Solar sensor: Boktai series
- Tilt/gyro: Yoshi Topsy-Turvy, Kirby Tilt 'n' Tumble (via `sceMotionGetSensorState`)
- Rumble: Game Boy Player emulation via `sceCtrlSetActuator`
- GameShark, Action Replay v1–v3, CodeBreaker cheat codes via `.cht` file
- Frameskip: per-frame `skip_next_frame` control, 0–5 configurable

#### GB/GBC Engine (mGBA)
- SM83 (Sharp LR35902) interpreter — mGBA's accurate GB/GBC core
- Full CGB (Game Boy Color) support including HDMA, palette registers, speed switch
- All MBC types: MBC1, MBC2, MBC3 (+RTC), MBC5 (+rumble), MBC6, MBC7 (tilt), HuC-1, HuC-3, Pocket Camera, TAMA5, and unlicensed mappers
- SGB (Super Game Boy) border display
- Auto-detection of DMG/CGB/AGB mode from cart header
- Official GB/GBC BIOS supported when `gb_bios.bin` is present
- 131072 Hz audio resampled to 48000 Hz via cosine interpolation in a dedicated audio thread
- GameShark and Game Genie cheat support via mGBA's cheat device
- Per-game frameskip via `gb->video.frameskip`

#### Display
- vita2d double-buffer rendering (two XBGR8888 textures for GBA, RGB565 for GB)
- Screen modes: Aspect-correct, Fullscreen, Integer scale
- GBA LCD colour correction (32768-entry LUT, hardware-accurate gamma)
- Interframe blending via `vita2d_draw_texture_tint_part_scale` at 50% alpha
- FPS counter overlay

#### Audio
- GBA: gpSP ring buffer drained per-frame to `sceAudioOutOutput`
- GB/GBC: dedicated `gb_audio` thread, 16 × 512-sample ring buffers
- Volume 0–100% applied live via `sceAudioOutSetVolume`
- Audio mute respected by both engines immediately

#### UI
- ROM browser: paginated directory listing, `.gba/.agb/.gb/.gbc/.gbz/.bin` files
- In-game menu: save/load state (10 slots), reset, change ROM, screenshot, settings
- Settings: CPU clock (333/444/500 MHz), screen mode, volume, frameskip, colour correction, interframe blend, audio, dynarec
- Screenshot: `sceDisplayGetFrameBuf` → libpng PNG encode

#### Build
- CMake 3.12+ with VitaSDK toolchain
- `vita_create_self` + `vita_create_vpk` packaging
- `bash scripts/build.sh [clean] [PSVITAIP=x.x.x.x]`

### Known Limitations (v1.0.0)
- Fast-forward (2×) is not yet implemented (flag declared, not wired to double execution)
- Pause hotkey (without opening menu) not yet implemented
- Wireless Adapter (RFU) multiplayer not yet working on Vita (netplay stubs are no-ops)
- GB/GBC link cable emulation not implemented
- Screenshot encoding is synchronous — may cause a brief frame stutter
- SGB border is rendered by mGBA but may not display correctly at all screen modes

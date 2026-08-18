# Changelog

All notable changes to GBAVitaEX will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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

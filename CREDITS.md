# Credits & Attributions

GBVitaEX is built entirely on the work of the open-source emulation and homebrew communities.
Every engine, library, and platform layer listed here was essential to this project.

---

## Primary Engines

### gpSP — GBA Emulation Core
**Source:** https://github.com/libretro/gpsp  
**License:** GNU General Public License v2.0

| Author | Role |
|---|---|
| **Exophase** | Original gpSP (gameplaySP) author. Created the GBA ARMv7 JIT dynarec that makes this the fastest GBA emulator on ARM hardware. |
| **notaz** | notaz's gpSP fork — portability improvements and early accuracy fixes. |
| **Frangar (davidgfnet)** | Current libretro/gpsp maintainer. Fixed the broken ARMv7 dynarec on PSVita in 2019, added the new video renderer, interframe blending, colour correction LUT, RFU wireless adapter emulation, improved audio, and many compatibility fixes. |
| **Normmatt** | Open-source GBA BIOS replacement (`bios_data.S`) bundled in gpSP. |

GBVitaEX uses gpSP's GBA emulation core directly (without the libretro shim), replacing the libretro callbacks with our own PSVita-native equivalents.

**Key gpSP components used:**
- ARMv7 JIT dynarec (`cpu_threaded.c`, `arm/arm_stub.S`, `arm/arm_emit.h`)
- GBA hardware: `main.c`, `gba_memory.c`, `video.cc`, `cpu.cc`
- Audio: `sound.c` (6-channel PSG + DirectSound FIFO)
- Save states: `savestate.c` (BSON format)
- Per-game overrides: `gba_over.h` (150+ games with idle loops, save types, RTC, RFU, rumble flags)
- Serial / RFU: `rfu.c`, `serial.c`, `serial_proto.c`
- Cheats: `cheats.c` (GameShark / Action Replay / CodeBreaker)

---

### mGBA — GB/GBC Emulation Core
**Source:** https://github.com/mgba-emu/mgba  
**License:** Mozilla Public License 2.0

| Author | Role |
|---|---|
| **endrift (Jeffrey Pfau)** | mGBA creator and primary author. The most accurate GB/GBC/GBA emulator available. |
| **Contributors** | See https://github.com/mgba-emu/mgba/graphs/contributors |

GBVitaEX uses mGBA's GB/GBC subsystem (`src/gb/`, `src/sm83/`) for all Game Boy and Game Boy Color games, and adapts mGBA's PSVita platform layer (`src/platform/psp2/`) for audio, rendering, and memory.

**Key mGBA components used:**
- SM83 (Sharp LR35902) CPU interpreter: `src/sm83/`
- GB/GBC board: `src/gb/` — audio, MBC, memory, SIO, timer, video, serialize
- All MBC types: MBC1–7, HuC-1/3, Pocket Camera, TAMA5, unlicensed
- Link cable lockstep: `src/gb/sio/lockstep.c` (`GBSIOLockstep`)
- Audio resampler: `src/util/audio-resampler.c` (cosine interpolation, 131072→48000 Hz)
- PSP2 memory backend: `src/platform/psp2/psp2-memory.c` (`anonymousMemoryMap`)
- PSP2 VFS: `src/platform/psp2/sce-vfs.c`
- VFS memory backend: `src/util/vfs/vfs-mem.c`
- INI parser: `src/third-party/inih/ini.c` (by Ben Hoyt, MIT licence)

---

## Reference Projects

### gpSP-kai (takka's fork) / UO gpSP-kai (ErikPshat, Oldvic)
**Source:** https://github.com/PSP-Archive/unofficial-gpSP-kai  
**License:** GPL-2.0

Takka's unofficial fork of gpSP that set the standard for GBA emulation on PSP. Informed our understanding of per-game compatibility tuning, CPU clock adjustment, and PSP/Vita hardware-specific optimisations. ErikPshat and Oldvic's further refinements (ZIP ROM support, improved font, CWF compatibility) were also studied.

### TempGBA / TempGBA4PSP
**Source:** https://github.com/PSP-Archive/TempGBA  
**License:** GPL-2.0  
**Authors:** Nebuleon, Normmatt, BassAceGold

Port of the NDSGBA emulator (Supercard DSTWO) to PSP. Contains Nebuleon's ReGBA recompiler — a rewrite of the gpSP dynarec that improves accuracy. Referenced for dynarec design patterns and the open-source BIOS integration approach.

### FrogGBA
**Source:** https://github.com/tzubertowski/FrogGBA  
**License:** GPL-2.0  
**Author:** tzubertowski

Modernised TempGBA4PSP fork with gpSP-kai optimisations (10–25% performance gains). Referenced for performance tuning techniques, particularly the water-section fix in Castlevania that was historically problematic on PSP/Vita.

### gPSP-mod
**Source:** https://github.com/BASLQC/gPSP-mod  
**License:** GPL-2.0  
**Author:** Anonymous (Japanese developer, ~2009)

Further-improved fork of gpSP-kai. Referenced for additional compatibility patches and the approach to handling ROM hacks.

---

## Audio

### audio-stretch — TDHS Pitch Correction
**Source:** https://github.com/dbry/audio-stretch  
**License:** BSD 3-Clause  
**Author:** David Bryant

Time-Domain Harmonic Scaling (TDHS) library used to preserve audio pitch during fast-forward. Allows music to play at normal pitch even at 8× speed. An O(n) algorithm (~1 multiply + 2 adds per output sample) — adds approximately 0.5 ms overhead at 444 MHz.

---

## Platform & SDK

| Project | URL | Role |
|---|---|---|
| **VitaSDK** | https://vitasdk.org/ | ARM cross-compiler, system headers, VPK toolchain |
| **vita2d** (xerpi) | https://github.com/xerpi/vita2d | 2D GPU-accelerated rendering, texture management, PGF font |
| **HENkaku / Ensō** | https://henkaku.xyz/ | PSVita custom firmware enabling homebrew execution |
| **PSVshell** (Electry) | https://github.com/Electry/PSVshell | Reference for `scePowerSetArmClockFrequency()` usage |
| **libpng** | http://www.libpng.org/ | PNG screenshot encoding (bundled with VitaSDK) |
| **inih** (Ben Hoyt) | https://github.com/benhoyt/inih | INI file parser (MIT) bundled with mGBA |

---

## GBA Wireless Adapter Research

The RFU (Radio Frequency Unit) wireless adapter emulation in GBVitaEX is entirely built on:

| Resource | Author | Contribution |
|---|---|---|
| [Emulating the GBA Wireless Adapter](https://www.davidgf.net/2024/01/13/gba-wireless-adapter/index.html) | davidgfnet | Protocol reverse engineering, Netpacket API design |
| [gba-link-connection](https://github.com/afska/gba-link-connection) | Rodrigo Alfonso (afska) | Further protocol reversal, complete command documentation |
| [gba-wireless-adapter.md](https://blog.kuiper.dev/gba-wireless-adapter) | Corwin | Original protocol documentation |
| Pokémon FireRed/Emerald reversed sources | Various | Confirmed game-side RFU behaviour |

---

## GBVitaEX Authors

| Person | Contribution |
|---|---|
| **Zushikina-kun** | Project concept, Vita platform integration, emulation dispatch layer, native UI (vita2d), build system, audio pipeline, multiplayer transport, configuration system |

---

## Licence Notice

GBVitaEX is licensed under the **GNU General Public License v2.0**, inheriting the licence of its primary dependency (gpSP). mGBA components are MPL-2.0 (compatible with GPL-2.0 in this combination). audio-stretch is BSD-3-Clause (compatible with GPL-2.0).

See [LICENSE](LICENSE) for the full GPL-2.0 text.

---

*If your work has been omitted or incorrectly attributed, please open an issue at  
https://github.com/Zushikina-kun/GBPSVitaEX/issues*

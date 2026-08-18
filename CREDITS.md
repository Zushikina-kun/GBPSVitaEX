# Credits & Attributions

GBAVitaEX is built on the shoulders of many excellent open-source emulator projects. Full credit and deep gratitude to everyone listed below.

---

## GBA Emulation Engine — gpSP (libretro fork)

**Source:** https://github.com/libretro/gpsp

GBAVitaEX uses the gpSP GBA emulation core, specifically the libretro/davidgfnet fork which contains the fixed ARMv7 dynarec and many accuracy and audio improvements.

| Person | Contribution |
|---|---|
| **Exophase** | Original gpSP (gameplaySP) author — the foundational GBA dynarec engine |
| **notaz** | notaz's gpSP fork — improved accuracy and portability |
| **Frangar** | Fixed the broken ARMv7 dynarec on PSVita/ARM32 in 2019 |
| **davidgfnet** | Current libretro/gpsp maintainer — new video renderer, audio fixes, RFU/wireless adapter, interframe blending, colour correction |
| **Normmatt** | Open-source GBA BIOS replacement bundled with gpSP |
| **takka** | gpSP-kai fork — original PSP-specific improvements |
| **ErikPshat / Oldvic** | UO gpSP Kai — further PSP improvements and bug fixes |

### gpSP License
GNU General Public License v2.0 — https://github.com/libretro/gpsp/blob/master/COPYING

---

## GB/GBC Emulation Engine — mGBA

**Source:** https://github.com/mgba-emu/mgba

GBAVitaEX uses mGBA's GB/GBC emulation core (SM83 CPU + GB board) for Game Boy and Game Boy Color games. mGBA is the most accurate GB/GBC/GBA emulator available.

| Person | Contribution |
|---|---|
| **endrift (Jeffrey Pfau)** | mGBA creator and primary author |
| **Contributors** | See https://github.com/mgba-emu/mgba/graphs/contributors |

mGBA's PSP2 platform code (psp2-context.c, psp2-memory.c, sce-vfs.c) was a direct reference and partial basis for GBAVitaEX's own PSP2 platform layer.

### mGBA License
Mozilla Public License 2.0 — https://github.com/mgba-emu/mgba/blob/master/LICENSE

---

## Reference Projects

### TempGBA / TempGBA4PSP
**Source:** https://github.com/PSP-Archive/TempGBA

A GBA emulator for the Nintendo DS Supercard DSTWO and PSP, derived from gpSP-kai.
Authors: Nebuleon, Normmatt, BassAceGold

Consulted for: ReGBA recompiler approach, open-source BIOS integration, save state design.

### FrogGBA
**Source:** https://github.com/tzubertowski/FrogGBA

A modernised TempGBA4PSP fork with gpSP-kai optimisations.
Author: tzubertowski

Consulted for: performance optimisation techniques on PSP/Vita hardware.

### gPSP-mod
**Source:** https://github.com/BASLQC/gPSP-mod

An improved anonymous Japanese fork of gpSP-kai (2009).
Consulted for: early compatibility and accuracy fixes.

---

## Platform & SDK

| Project | URL |
|---|---|
| **VitaSDK** | https://vitasdk.org/ |
| **vita2d** | https://github.com/xerpi/vita2d — 2D rendering library for PSVita |
| **HENkaku / Ensō** | https://henkaku.xyz/ — PSVita custom firmware that makes homebrew possible |
| **PSVshell** | https://github.com/Electry/PSVshell — reference for CPU clock control |
| **inih** | https://github.com/benhoyt/inih — INI parser (bundled with mGBA) |

---

## GBAVitaEX Authors

| Person | Contribution |
|---|---|
| **Zushikina-kun** | Project lead, Vita integration, platform layer, UI, build system |

---

## Additional Notes

- The open-source GBA BIOS replacement was written by Normmatt and is distributed with gpSP under GPL-2.
- The GBA LCD colour correction LUT (`gba_cc_lut`) was authored by davidgfnet as part of the gpSP libretro fork.
- mGBA's audio resampler (cosine interpolator) was authored by endrift.

If you believe your work has been omitted or misattributed, please open an issue at https://github.com/Zushikina-kun/GBPSVitaEX/issues.

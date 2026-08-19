# GBAVitaEX

**The best Game Boy Advance / Game Boy Color / Game Boy emulator for PlayStation Vita.**

GBAVitaEX is a standalone native VPK emulator combining the fastest and most accurate open-source cores available, purpose-built for the PSVita hardware. It runs GBA games via a working ARMv7 JIT dynarec and GB/GBC games through the most accurate GB emulator in existence.

---

## Features

### Performance
- **ARMv7 JIT dynarec** for GBA — the same dynamic recompiler that makes gpSP the fastest GBA emulator on ARM hardware, tuned for the Vita's Cortex-A9
- **Automatic CPU clock boost** — configurable from 333 to 500 MHz (via PSVshell-compatible `scePowerSetArmClockFrequency`)
- **Per-system frameskip** — user-adjustable 0–5, applied natively to each engine
- **Fast-forward support** — built into the emulation loop

### Accuracy
- GBA: based on gpSP (libretro/davidgfnet fork) — open-source BIOS, accurate DMA/timer/audio
- GB/GBC: based on mGBA 0.10.x — the most accurate GB/GBC emulator, supporting all mapper types including HuC-3 RTC, MBC7 tilt, GB Camera, and unlicensed carts

### Display
- **Four screen modes**: Aspect-correct (default), Fullscreen, Integer scale, Backdrop
- **GBA LCD colour correction** — applies the authentic GBA screen gamma curve to compensate for the washed-out original palette
- **Interframe blending** — reduces flicker in games that use alternating-frame transparency tricks (e.g. many GBA games)
- **vita2d hardware-accelerated rendering** — textures uploaded once per frame, scaled on the SGX543MP GPU

### Audio
- GBA: gpSP's full 6-channel PSG + DirectSound FIFO audio at 65536 Hz, drained per-frame
- GB/GBC: mGBA's SM83 audio at 131072 Hz, resampled to 48000 Hz via a cosine resampler in a dedicated audio thread
- **Per-game volume control** (0–100%) applied live to the SCE audio port
- **Audio mute** toggle respected by both engines

### Saves
- **SRAM auto-save** on ROM unload / app exit
- **10 save state slots** (0–9) per ROM, stored as `ux0:data/GBAVitaEX/states/<romname>.stN`
- **IPS/UPS/BPS patch loading** supported by mGBA for GB/GBC

### Cheats
- GBA: GameShark, Action Replay v1-v3, CodeBreaker via gpSP's cheat engine
- GB/GBC: GameShark, Game Genie via mGBA's cheat device
- Loaded automatically from `ux0:data/GBAVitaEX/cheats/<romname>.cht` on ROM load

### Hardware Features (GBA)
- **Real-time clock (RTC)** — Pokémon Ruby/Sapphire, Emerald, FireRed/LeafGreen etc.
- **Solar sensor** — Boktai series
- **Tilt/gyro sensor** — Yoshi Topsy-Turvy, Kirby Tilt 'n' Tumble (via Vita motion)
- **Rumble** — Game Boy Player emulation through `sceCtrlSetActuator`

### UI
- **Paginated ROM browser** with directory navigation
  - Shows `.gba`, `.agb`, `.gb`, `.gbc`, `.gbz`, `.bin` files
  - L/R for page scroll, Circle to go up a directory
- **In-game menu** (hold SELECT+START for 0.5 s)
  - Save/load state with slot selector
  - Screenshot saved to `ux0:data/GBAVitaEX/screenshots/`
  - Reset, change ROM, settings
- **Settings screen** — clock speed, screen mode, volume, frameskip, colour correction, interframe blend, audio toggle, dynarec toggle
- **FPS counter** overlay during gameplay

---

## Installation

1. Install the `.vpk` via VitaShell or a package manager like VPK Installer
2. **Place your GBA BIOS** at `ux0:data/GBAVitaEX/gba_bios.bin` (optional — built-in open-source BIOS will be used if not found)
3. Place GB/GBC BIOS at `ux0:data/GBAVitaEX/gb_bios.bin` (optional)
4. Place ROMs in `ux0:data/GBAVitaEX/roms/` or any subdirectory
5. Launch **GBAVitaEX** from the LiveArea

> **Requires:** HENkaku / Ensō custom firmware

---

## Controls

| Vita Button | GBA/GB Function |
|---|---|
| Cross | A |
| Circle | B |
| Select | Select |
| Start | Start |
| D-Pad / Left Analog | D-Pad |
| L Trigger | L |
| R Trigger | R |
| SELECT + START (hold 0.5s) | Open in-game menu |

---

## ROM Browser Controls

| Button | Action |
|---|---|
| Up / Down | Navigate list |
| L / R | Page up / down |
| Cross | Open ROM or enter directory |
| Circle | Go up one directory |
| Select+Start | Open menu |

---

## Cheat File Format

Place a `.cht` file at `ux0:data/GBAVitaEX/cheats/<rom-name-without-extension>.cht`.

```
# Lines starting with # are comments
# GBA cheats (GameShark / Action Replay)
GS 01234567 89ABCDEF  Infinite HP
AR DEADBEEF CAFEF00D  Max Gold
CB 12345 6789         Unlock All

# GB/GBC cheats
GB 0101CFCD  Infinite Lives (GameShark)
GG 00C-E9A   All Items (Game Genie)
```

---

## Directory Structure

```
ux0:data/GBAVitaEX/
├── gba_bios.bin          (optional, official GBA BIOS)
├── gb_bios.bin           (optional, official GB/GBC BIOS)
├── roms/                 (place ROMs here, subdirs supported)
├── saves/                (SRAM files, auto-managed)
├── states/               (save state files)
├── cheats/               (cheat files)
└── screenshots/          (screenshot PNGs)
```

---

## Building from Source

### Prerequisites
- [VitaSDK](https://vitasdk.org/) installed at `/usr/local/vitasdk`
- devkitPro msys2 shell (or any shell with VitaSDK in PATH)
- CMake ≥ 3.12 (bundled with devkitPro msys2)

### Build

```bash
# Clone the repo (submodules included automatically)
git clone --recurse-submodules https://github.com/Zushikina-kun/GBPSVitaEX.git
cd GBPSVitaEX

# Build (clean)
bash scripts/build.sh clean

# Build and deploy to Vita via FTP
bash scripts/build.sh PSVITAIP=192.168.1.x
```

The VPK will be at `build/GBAVitaEX.vpk`.

---

## License

GBAVitaEX is licensed under the **GNU General Public License v2** (GPLv2), inheriting the license of its primary dependency (gpSP). mGBA components are licensed under MPL-2.0, which is compatible with GPLv2 in this combination.

See [LICENSE](LICENSE) for full text.

---

## Credits & Attributions

See [CREDITS.md](CREDITS.md) for full attribution to all upstream projects.

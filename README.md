# GBAVitaEX v1.4.0

**The best GBA / GBC / GB emulator for PlayStation Vita.**

Combines the fastest GBA emulator (gpSP ARMv7 JIT dynarec) with the most accurate GB/GBC emulator (mGBA), in a single standalone native VPK — no RetroArch required.

---

## Features

### Engines

| System | Engine | Method |
|---|---|---|
| GBA | gpSP (libretro/davidgfnet fork) | ARMv7 JIT dynarec — GBA ARM7TDMI → native Cortex-A9 at runtime |
| GB / GBC | mGBA 0.10.x (endrift) | SM83 interpreter — most accurate GB/GBC emulator available |

### Performance
- **CPU clock**: 333 / 444 / 500 MHz (user-configurable)
- **Fast-forward**: 1.25× – 8× (step 0.25×), hold configurable button (default: R Trigger)
  - Vsync disabled during FF via `vita2d_set_vblank_wait(0)` — no display cap
  - **Pitch correction** (TDHS): music stays at normal pitch even at 8× speed — toggle in Settings
- **Frameskip**: 0–5 per-frame skip for both GBA and GB/GBC
- **JIT dynarec**: `sceKernelOpenVMDomain()` checked; graceful fallback to interpreter if unavailable
- **Watchdog**: `sceKernelPowerTick()` called every frame — Vita never sleeps during play

### Display
- **3 screen modes**: Aspect-correct (default), Fullscreen stretch, Integer scale
- **GBA LCD colour correction**: 32 768-entry hardware-accurate gamma LUT
- **Interframe blending**: auto-disabled during fast-forward

### Audio
- GBA: gpSP 65 536 Hz ring buffer → `sceAudioOut`, with TDHS pitch correction during FF
- GB/GBC: mGBA 131 072 Hz → cosine-resampled to 48 000 Hz in a dedicated kernel thread
- Volume 0–100% applied live; mute flag respected by both engines

### Saves
- **SRAM auto-save** on ROM unload / app exit
- **10 save state slots** (0–9) per ROM
- **IPS/UPS/BPS patch loading** for GB/GBC ROMs (via mGBA)

### Cheats
- GBA: GameShark / Action Replay v1–v3 / CodeBreaker via gpSP's `cheat_parse()`
- GB/GBC: GameShark / Game Genie via mGBA's cheat device
- Auto-loaded from `ux0:data/GBAVitaEX/cheats/<romname>.cht`

### GBA Hardware Features (auto-detected from gba_over.h)
- **RTC**: Pokémon Ruby/Sapphire/Emerald, berry growth, time events
- **Solar sensor**: Boktai series
- **Tilt/gyro**: Yoshi Topsy-Turvy, Kirby Tilt 'n' Tumble (via `sceMotionGetSensorState`)
- **Rumble**: Drill Dozer, WarioWare: Twisted (via `sceCtrlSetActuator`)
- **150+ per-game overrides** (idle loops, save type, serial mode) from `gba_over.h`

### Multiplayer

#### GBA — RFU Wireless Adapter (WiFi LAN)
- Access via **Menu → WiFi Multiplayer: Host** or **Join**
- Uses SceNet UDP broadcast on port 7354 for peer discovery; unicast for game data
- Working games: Pokémon FireRed/LeafGreen/Emerald, Mario Golf: Advance Tour, Megaman BN5/6, Mario Tennis Power Tour, and others flagged in `gba_over.h`
- Maximum 5 players (1 host + 4 clients)
- Same LAN subnet required; internet play needs a relay server

#### GB/GBC — Single-Device Link Cable
- Access via **Menu → Link Cable (2P)**
- Runs two mGBA GB cores in lock-step on the same Vita via `GBSIOLockstepInit`
- Enables Pokémon trading, Tetris 2-player, etc.
- Both players use the same ROM; P2 save stored as `<romname>.p2.sav`

### Input
- **Full button remapping**: every GBA/GB input bindable to any Vita button
- **Fast-forward button**: configurable (Off / R Trigger / Back-L / Back-R / Triangle)
- Analog left stick as D-Pad fallback
- **Pause hotkey**: hold L + R triggers for 1 second (disabled if either trigger is the FF button)

### Settings
All settings saved to `ux0:data/GBAVitaEX/config.ini` on exit, auto-loaded on launch.

| Setting | Options |
|---|---|
| CPU Clock | 333 / 444 / 500 MHz |
| Screen Mode | Aspect / Fullscreen / Integer |
| Volume | 0–100% |
| Frameskip | Off / 1–5 frames |
| Fast-Forward Speed | 1.25× – 8× |
| Fast-Forward Button | Off / R Trigger / Back-L / Back-R / Triangle |
| FF Pitch Correction | On / Off (TDHS audio time-stretch) |
| GBA Colour Correction | On / Off |
| Interframe Blending | On / Off |
| Audio | On / Off |
| JIT Dynarec (GBA) | On / Off |
| Button Remapping | → sub-screen |

### Screenshot
- Captured from `sceDisplayGetFrameBuf()` → PNG via libpng
- Saved to `ux0:data/GBAVitaEX/screenshots/shot####.png`
- Saved asynchronously on a background thread — no frame stutter

---

## Installation

1. Install `GBAVitaEX-v1.4.0.vpk` via VitaShell
2. Place your **GBA BIOS** at `ux0:data/GBAVitaEX/gba_bios.bin` *(optional — built-in open-source BIOS used if absent)*
3. Place your **GB/GBC BIOS** at `ux0:data/GBAVitaEX/gb_bios.bin` *(optional)*
4. Place ROMs in `ux0:data/GBAVitaEX/roms/` (subdirectories supported)
5. Launch **GBAVitaEX** from the LiveArea

> **Requires:** HENkaku / Ensō custom firmware

---

## Controls

| Vita | GBA / GB |
|---|---|
| Cross | A (default) |
| Circle | B (default) |
| Square | B (default) |
| Start | Start |
| Select | Select |
| D-Pad / Left Analog | D-Pad |
| L Trigger | L |
| R Trigger | R (or Fast-Forward) |
| SELECT + START (hold 0.5 s) | Open pause menu |
| L + R (hold 1 s) | Toggle pause (if neither is FF button) |

---

## In-Game Menu

Open with **SELECT + START held for 0.5 seconds**.

| Item | Description |
|---|---|
| Resume | Return to game |
| Save State | Save to current slot (change with L/R) |
| Load State | Load from current slot |
| Reset Game | Soft reset |
| Change ROM | Return to ROM browser |
| Settings | Open settings screen |
| Screenshot | Save PNG to screenshots folder |
| WiFi Multiplayer: Host | *(GBA only)* Start as host |
| WiFi Multiplayer: Join | *(GBA only)* Join a host on LAN |
| Stop WiFi Multiplayer | *(when active)* Disconnect |
| Link Cable (2P) | *(GB/GBC only)* Start two-player link mode |
| Stop Link Cable | *(when active)* Stop link mode |
| Exit | Quit to LiveArea |

---

## Cheat File Format

`ux0:data/GBAVitaEX/cheats/<romname>.cht`

```
# GBA cheats
GS XXXXXXXX YYYYYYYY   GameShark / Action Replay
AR XXXXXXXX YYYYYYYY   Action Replay v3
CB XXXXX YYYY          CodeBreaker

# GB/GBC cheats
GB XXXXXXXXX           GameShark GB (9 hex digits)
GG XXXX-XXXX-XXX       Game Genie
```

---

## Directory Layout

```
ux0:data/GBAVitaEX/
├── gba_bios.bin        (optional)
├── gb_bios.bin         (optional)
├── config.ini          (auto-saved settings)
├── roms/               (ROMs, subdirectories OK)
├── saves/              (SRAM .sav files)
├── states/             (save states .stN)
├── cheats/             (cheat files .cht)
└── screenshots/        (PNG screenshots)
```

---

## Building from Source

```bash
git clone --recurse-submodules https://github.com/Zushikina-kun/GBPSVitaEX.git
cd GBPSVitaEX

# Requires VitaSDK at /usr/local/vitasdk (devkitPro msys2 shell)
bash scripts/build.sh clean

# Build + FTP deploy
bash scripts/build.sh PSVITAIP=192.168.x.x
```

---

## License

GPL-2.0 (inherited from gpSP). See [LICENSE](LICENSE).  
mGBA components: MPL-2.0 (compatible with GPL-2.0 in this combination).  
audio-stretch: BSD-3-Clause.

See [CREDITS.md](CREDITS.md) for full attribution.

---

## Known Limitations

| Item | Status |
|---|---|
| RFU internet play | LAN only — no relay server |
| GB link cable over network | Not feasible (microsecond-precise synchronous protocol) |
| Fast-forward > 6× actual speed | Display capped at monitor refresh; extra frames computed but not shown |
| SGB border in all screen modes | May clip in integer scale mode |
| Link Cable P2 ROM selection UI | Currently uses same ROM as P1; different-version support is a future UI task |

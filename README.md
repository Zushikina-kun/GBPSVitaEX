# MGBAVitaEX

**The best GBA / GBC / GB emulator for PlayStation Vita.**

A surgical improvement of mGBA's official PSVita port — same battle-tested engine, with all the known bugs fixed and the most-requested features added. No RetroArch required.

Based on [mGBA](https://mgba.io/) by endrift (MPL-2.0).

---

## What's different from stock mGBA PSVita

| Fix / Feature | Details |
|---|---|
| **Fast-forward works out of the box** | R Trigger (hold) = speed up · L Trigger (toggle) = lock FF on. Rebindable in Settings → Remap |
| **mGBA menu always accessible** | Hold SELECT+START for ~0.5 s to open the in-game menu — no button is hardcoded |
| **Triangle fully remappable** | Triangle is no longer locked as the menu key — assign it to any game button in the Remap screen |
| **CPU clock choice** | 333 MHz (battery saver) or 444 MHz (default) — pick in Settings |
| **Pokémon save lag reduced** | `idleOptimization = detect` enabled by default — mGBA auto-detects Emerald/Ruby/Sapphire idle loops |
| **Saves in their own folder** | `ux0:data/mGBA/saves/` and `ux0:data/mGBA/states/` — no more save files next to ROMs |
| **No auto-sleep during gameplay** | `sceKernelPowerTick` called every frame (#1970) |
| **Reduced audio crackling** | Audio buffer 512→1024, BUFFERS 16→24 (#3044) |
| **Mute during fast-forward** | Toggle in Settings |
| **FPS counter** | Toggle in Settings |

---

## Features (from mGBA base)

- Full **GBA + GB + GBC** emulation in one app
- mGUI ROM browser with thumbnails, save states (9 slots), cheats, IPS patches, rewind
- Bilinear filtering, aspect/stretch/backdrop screen modes
- 48 kHz audio via dedicated thread (cosine resampler)
- vita2d double-buffer rendering
- RTC, solar sensor, gyro, rumble already wired in
- Screenshot via ScePhotoExport
- imc0/xmc0 storage support
- 150+ per-game overrides from `gba_over.h` (idle loops, save type, serial mode)

---

## Installation

1. Install `MGBAVitaEX-v2.1.0.vpk` via VitaShell
2. Place ROMs anywhere on the Vita storage — the ROM browser lets you navigate to them
3. Launch **MGBAVitaEX** from the LiveArea

> **Requires:** HENkaku / Ensō custom firmware

### Upgrading from GBVitaEX v2.0.0

MGBAVitaEX installs as a **separate app** (same Title ID `GBVX00001`, so it replaces the old entry). Your saves in `ux0:data/mGBA/saves/` are untouched.

If you had a stale `config.ini` from a previous install that locked Triangle to the menu, delete it once:

```
ux0:data/mGBA/config.ini
```

Then relaunch — MGBAVitaEX will write a fresh config with the correct defaults.

---

## Controls

| Vita Button | Default Action |
|---|---|
| Cross / Circle | Confirm / Back (follows system setting) |
| D-Pad / Left Analog | D-Pad |
| Start / Select | Start / Select |
| L Trigger | Fast-forward toggle (rebindable) |
| R Trigger | Fast-forward hold (rebindable) |
| Square | Cycle screen mode |
| Triangle | Free — assign in Remap |
| SELECT+START (hold ~0.5 s) | Open in-game menu |

All buttons are rebindable in **Settings → Remap**.

---

## In-Game Menu

Open with **SELECT + START held for ~0.5 seconds**.

| Item | Description |
|---|---|
| Resume | Return to game |
| Save State | Save to current slot |
| Load State | Load from current slot |
| Reset | Soft reset |
| Change ROM | Return to ROM browser |
| Settings | Open settings screen |
| Screenshot | Save PNG |
| Exit | Quit to LiveArea |

---

## Settings

| Setting | Options |
|---|---|
| Screen Mode | With Background / Without Background / Stretched / Fit Aspect Ratio |
| Screen Filtering | None / Bilinear |
| Camera | None / Front / Back |
| Mute during fast-forward | Off / On |
| Show FPS counter | Off / On |
| CPU Clock Speed | 333 MHz / 444 MHz |

---

## Directory Layout

```
ux0:data/mGBA/
├── config.ini        (auto-saved settings)
├── saves/            (SRAM .sav files)
└── states/           (save states)
```

---

## Building from Source

```bash
git clone --recurse-submodules https://github.com/Zushikina-kun/GBVitaEX.git
cd GBVitaEX

# Requires VitaSDK — devkitPro msys2 shell
bash /tmp/build_v2_final.sh
```

VPK output: `build/psp2/MGBAVitaEX.vpk`

---

## Credits & Sources

| Component | Author | License |
|---|---|---|
| [mGBA](https://mgba.io/) | endrift | MPL-2.0 |
| PSVita port base | mGBA contributors | MPL-2.0 |
| MGBAVitaEX patches | MGBAVitaEX contributors | MPL-2.0 |

See [CREDITS.md](CREDITS.md) for full attribution.

---

## License

MPL-2.0 (inherited from mGBA). See [LICENSE](LICENSE).

---

## Known Issues / TODO

| Item | Status |
|---|---|
| Fast-forward speed ratio (e.g. 2×, 4×) | PSVita FF is unbounded — ratio control requires Qt-side sync changes not present in PSP2 port |
| Back touch zones (L2/R2/L3/R3 mapping) | mGBA #3054 — not yet wired in rebind UI |
| ROM browser L/R page skip | mGBA #3039 — upstream GUI feature |
| Custom Vita bubbles | Out of scope |

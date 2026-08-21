# MGBAVitaEX

**The best GBA / GBC / GB emulator for PlayStation Vita.**

A surgical improvement of mGBA's official PSVita port — same battle-tested engine, with all the known bugs fixed and the most-requested features added. No RetroArch required.

Based on [mGBA](https://mgba.io/) by endrift (MPL-2.0).

---

## What's different from stock mGBA PSVita

| Fix / Feature | Details |
|---|---|
| **Fast-forward works out of the box** | R Trigger (hold) = speed up · L Trigger (toggle) = lock FF on. Rebindable in Remap |
| **mGBA menu always accessible** | Hold SELECT+START for ~0.5 s to open the in-game menu — no button is hardcoded |
| **All buttons fully remappable** | Triangle, Square, Cross, Circle, Start, Select, L/R Trigger, L1/R1, L3/R3, back-touch all appear in the Remap screen by name |
| **CPU clock choice** | 333 MHz (battery saver) or 444 MHz (default) — pick in Settings |
| **GPU clock raised to 222 MHz** | Vita OS defaults GPU to 111 MHz; we raise it to 222 MHz for faster vita2d throughput |
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

1. Install `MGBAVitaEX-v2.1.2.vpk` via VitaShell
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

| Vita Button | Default Game Action | Default Interface Action |
|---|---|---|
| Cross | A | Confirm |
| Circle | B | Back |
| Triangle | A (remappable) | — |
| Square | B (remappable) | Cycle screen mode |
| Start | Start | — |
| Select | Select | — |
| D-Pad / Left Analog | D-Pad | Navigate menus |
| L Trigger | L button | Fast-forward toggle |
| R Trigger | R button | Fast-forward hold |
| L1 | L button | — |
| R1 | R button | — |
| SELECT+START (hold ~0.5 s) | — | Open in-game menu |

Every entry above is rebindable. See the Remap section below.

---

## Button Remapping

Open the remap screen via **SELECT+START (hold) → Configure → Remap keys**.

The screen has two sections:

**Game keys** — what each GBA/GB button does on the Vita hardware:
- A, B, Start, Select, Up, Down, Left, Right, L, R
- Each can be assigned to any physical button: Select, L3, R3, Start, Up, Right, Down, Left, L Trigger, R Trigger, L1, R1, Triangle, Circle, Cross, Square

**Interface keys** — what each emulator function does:
- Fast forward (held), Fast forward (toggle), Screen mode, Take screenshot, Mute (toggle)
- **Cancel** — opens the in-game menu (SELECT+START combo always works too)

### Example: swap Square/Triangle to act as Start/Select

1. Remap → Game keys → **Start** → set to "Square"
2. Remap → Game keys → **Select** → set to "Triangle"
3. Remap → Interface keys → **Cancel** → set to "Start" (physical Start now opens menu)
4. Remap → Interface keys → **Fast forward (toggle)** → set to "Select" (optional)
5. Save

### Example: put fast-forward on back-touch

1. Remap → Interface keys → **Fast forward (held)** → set to "L3" or "R3" (back-touch zones)
2. Save

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

## Performance Tips

MGBAVitaEX runs the full GBA library at 60fps for the vast majority of games. For the
small number of CPU-heavy titles (certain Fire Emblem maps, some Castlevania sections),
these settings help:

1. **Configure → Frameskip → 1** — emulates at full speed but renders every other frame.
   Most games look fine at frameskip 1; saves ~30% GPU time per frame.
2. **CPU Clock Speed → 444 MHz** — make sure you're not on 333 MHz.
3. **Screen Filtering → None** — already the default; bilinear adds per-frame CPU cost.
4. **Interframe Blending → Off** — already the default; blending doubles GPU draw calls.

If a game still stutters after the above: the root cause is that mGBA uses a **pure
interpreter** (no JIT recompiler). A JIT is planned — see [FUTURE.md](FUTURE.md).

---

## Known Issues & Planned Improvements

| Item | Status |
|---|---|
| Sub-60fps in CPU-heavy GBA scenes | Root cause: no JIT dynarec. Planned — see FUTURE.md |
| Fast-forward speed ratio (2×, 3×, etc.) | PSVita FF is unbounded; ratio requires core sync changes |
| Back-touch zone remapping (all 4 zones) | Planned: 1–2 days of work — see FUTURE.md Priority 4 |
| ROM browser L/R page skip | Planned: ~half a day — see FUTURE.md Priority 5 |
| OpenGL high-res renderer | Planned after JIT — see FUTURE.md Priority 2 |
| Custom Vita bubbles | Out of scope |

See [FUTURE.md](FUTURE.md) for the full technical roadmap including JIT and GPU renderer plans.


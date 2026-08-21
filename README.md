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
| **All buttons fully remappable with dual bindings** | Every game button and interface function has a Primary and Alt slot — assign two physical buttons to the same action. Triangle, Square, and "Open menu" all configurable. |
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

1. Install `MGBAVitaEX-v2.1.3.vpk` via VitaShell
2. Place ROMs anywhere on the Vita storage — the ROM browser lets you navigate to them
3. Launch **MGBAVitaEX** from the LiveArea

> **Requires:** HENkaku / Ensō custom firmware

### Upgrading from any previous version

MGBAVitaEX uses Title ID `GBVX00001` — installing a new VPK replaces the existing entry on LiveArea. Your saves in `ux0:data/mGBA/saves/` are never touched.

**Always delete `ux0:data/mGBA/config.ini` when upgrading** so the new defaults and key bindings take effect cleanly. Relaunch once — a fresh config is written automatically.

---

## Controls

| Vita Button | Default Game Action | Default Interface Action |
|---|---|---|
| Cross | A (Primary) | Menu: Confirm |
| Circle | B (Primary) | Menu: Back |
| Triangle | A (Alt — remappable) | — |
| Square | B (Alt — remappable) | Cycle screen mode |
| Start | Start | — |
| Select | Select | — |
| D-Pad / Left Analog | D-Pad | Navigate menus |
| L Trigger | L button | Fast forward (toggle) |
| R Trigger | R button | Fast forward (hold) |
| L1 | L button | — |
| R1 | R button | — |
| SELECT+START (hold ~0.5 s) | — | Open in-game menu (hardcoded fallback) |

Every row is remappable via the Remap screen — see Button Remapping below.

---

## Button Remapping

Open the remap screen via **SELECT+START (hold) → Configure → Remap keys**.

Every key has **two binding slots — Primary and Alt**. Both fire simultaneously, so you can have two physical buttons trigger the same GBA action. Set either slot to "Unmapped" to leave it unused.

The screen has two sections:

**Game keys** — what each GBA/GB button does:
- A, B, Select, Start, Right, Left, Up, Down, R, L
- Each has a `[Primary]` row and an `[Alt]` row
- All 16 Vita physical buttons appear by name in the dropdown: Select, L3, R3, Start, Up, Right, Down, Left, L Trigger, R Trigger, L1, R1, Triangle, Circle, Cross, Square

**Interface keys** — what each emulator function does:
- Open in-game menu, Fast forward (hold), Fast forward (toggle), Cycle screen mode, Take screenshot, Mute (toggle), Menu: Confirm, Menu: Back, Menu: Up/Down/Left/Right, solar brightness
- Each also has a `[Primary]` and `[Alt]` row

Hit **Save** to apply all changes. Hit **Cancel (discard)** to throw away all changes.

> SELECT+START hold always opens the in-game menu regardless of any remap — it is a hardcoded fallback.

---

### Example: Square+Start = GBA Start, Triangle+Select = GBA Select, physical Start = open menu

1. Remap → Game keys → **Start [Primary]** → Square
2. Remap → Game keys → **Start [Alt]** → Start  *(physical Start still fires GBA Start)*
3. Remap → Game keys → **Select [Primary]** → Triangle
4. Remap → Game keys → **Select [Alt]** → Select  *(physical Select still fires GBA Select)*
5. Remap → Interface keys → **Open in-game menu [Primary]** → Start
6. Hit **Save**

Result: Square and physical Start both = GBA Start. Triangle and physical Select both = GBA Select. Physical Start also opens the emulator menu.

---

### Example: two buttons for A, two for B

1. Remap → Game keys → **A [Primary]** → Cross *(default)*
2. Remap → Game keys → **A [Alt]** → Triangle
3. Remap → Game keys → **B [Primary]** → Circle *(default)*
4. Remap → Game keys → **B [Alt]** → Square
5. Hit **Save**

---

### Example: fast-forward on back-touch

1. Remap → Interface keys → **Fast forward (hold) [Primary]** → R Trigger *(default)*
2. Remap → Interface keys → **Fast forward (hold) [Alt]** → R3 *(back-touch right zone)*
3. Hit **Save**

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


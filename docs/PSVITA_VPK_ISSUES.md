# PSVita VPK Development — Known Issues & Fixes

Comprehensive reference of every issue encountered while developing homebrew for PSVita.
Covers VPK packaging, LiveArea assets, build system, and runtime behaviour.
Use this as a checklist for any new PSVita project.

**Sources:**
- [hammerill/livearea-specs](https://github.com/hammerill/livearea-specs) — definitive LiveArea image specification
- [psdevwiki.com/vita/Title_ID](https://www.psdevwiki.com/vita/Title_ID) — TITLEID format
- [VitaShell Issue #312](https://github.com/TheOfficialFloW/VitaShell/issues/312) — 0x8010113D investigation
- [gbatemp.net VitaSDK PSA](https://gbatemp.net/threads/psa-for-developers-if-you-have-performance-issues-in-your-homebrew-recompile-with-latest-vitasdk.466917/) — SDK performance bug
- Hands-on debugging during GBVitaEX development

---

## 1. Error 0x8010113D — VPK fails to install at 99%

The most common homebrew packaging error. The Vita's firmware validates every asset in the VPK before writing anything to storage. One bad file = full rollback.

### 1.1 PNG bit depth must be exactly 8-bit palette

**Error:** VPK installs to 99% then fails with 0x8010113D  
**Cause:** The Vita firmware requires PNG files with `bit_depth=8` and `color_type=3` (indexed palette) in the IHDR chunk. **It will reject:**
- Raw RGB/RGBA PNGs (colortype 2 or 6)
- Grayscale PNGs (colortype 0)
- Palette PNGs with depth=1, 2, or 4 (even though these are technically valid PNG)

**Critical trap:** `PIL/Pillow`'s `.convert('P')` automatically picks the *minimum* bit depth (1-bit for a near-solid-color image). This produces a valid PNG that still fails on Vita.

**Fix — Python (build the PNG manually to force depth=8):**
```python
import struct, zlib, os
from PIL import Image

def build_vita_png(path, width, height, bg_color=(10, 20, 50)):
    img = Image.new('RGB', (width, height), bg_color)
    img_p = img.convert('P', palette=Image.ADAPTIVE, colors=256)
    
    # Get palette, pad to exactly 256 entries (768 bytes)
    pal = img_p.palette.tobytes()
    pal = pal + b'\x00' * (768 - len(pal))
    pixels = img_p.tobytes()  # 1 byte per pixel
    
    def chunk(tag, data):
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', crc)

    # Build raw filtered rows (filter byte 0x00 = None)
    raw = b''.join(b'\x00' + pixels[y*width:(y+1)*width] for y in range(height))

    png  = b'\x89PNG\r\n\x1a\n'
    # CRITICAL: bit_depth=8 hardcoded, color_type=3 (palette)
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 3, 0, 0, 0))
    png += chunk(b'PLTE', pal)
    png += chunk(b'IDAT', zlib.compress(raw, 9))
    png += chunk(b'IEND', b'')

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(png)
```

**Fix — using pngquant (Linux/MSYS2):**
```bash
# pngquant forces proper 8-bit palette indexing
pngquant input.png --output output.png --force
```

**Fix — using ffmpeg:**
```bash
# -pix_fmt ya8 = 8-bit grayscale+alpha intermediate, then pngquant indexes it
ffmpeg -i source.png -vf scale=WxH:flags=neighbor -pix_fmt ya8 middle.png
pngquant middle.png -o output.png
rm middle.png
```

**Verification — check bit depth inside a built VPK:**
```python
import zipfile, struct

with zipfile.ZipFile('app.vpk', 'r') as z:
    for info in z.infolist():
        if not info.filename.endswith('.png'): continue
        data = z.read(info.filename)
        pos = 8
        while pos + 8 <= len(data):
            length = struct.unpack('>I', data[pos:pos+4])[0]
            if data[pos+4:pos+8] == b'IHDR':
                w, h = struct.unpack('>II', data[pos+8:pos+16])
                bd, ct = data[pos+16], data[pos+17]
                ok = (bd == 8 and ct == 3)
                print(f'{"OK" if ok else "FAIL"} {info.filename}: {w}x{h} depth={bd} colortype={ct}')
                break
            pos += 12 + length
```

---

### 1.2 Required PNG files, sizes, and rules

All files go in `sce_sys/`. Do not rename them.

| File | Size | Rules |
|---|---|---|
| `sce_sys/icon0.png` | **128 × 128** | 8-bit palette, **NO alpha channel** (Vita renders as circle, alpha rejected) |
| `sce_sys/pic0.png` | **960 × 544** | 8-bit palette, 256 colors indexed (use ffmpeg palettegen+paletteuse for real images) |
| `sce_sys/livearea/contents/bg0.png` | **840 × 500** | 8-bit palette |
| `sce_sys/livearea/contents/startup.png` | **280 × 158** | 8-bit palette, alpha channel allowed here |

**pic0.png is mandatory** — the Vita will reject the VPK if it is absent. This is the fullscreen loading image shown when launching the app.

**icon0.png must not have an alpha channel** — even though it is displayed as a circle on the LiveArea, the PNG itself must be fully opaque.

---

### 1.3 Wrong filename: `bg.png` vs `bg0.png`

**Cause:** The livearea background must be named `bg0.png`. Any other name fails.  
**Fix:** Always name it `bg0.png`, and reference it as `bg0.png` in `template.xml`.

---

### 1.4 Wrong `template.xml` style or image reference

**Wrong:**
```xml
<livearea style="psmobile-game" ...>
  <livearea-background><image>bg.png</image></livearea-background>
```

**Correct:**
```xml
<?xml version="1.0" encoding="utf-8"?>
<livearea style="a1" format-ver="01.00" content-rev="1">
  <livearea-background>
    <image>bg0.png</image>
  </livearea-background>
  <gate>
    <startup-image>startup.png</startup-image>
  </gate>
</livearea>
```

**Style options:**
- `a1` — startup.png displayed in the centre of the livearea bubble
- `psmobile` — startup.png displayed on the right side

---

### 1.5 TITLEID format: exactly 4 uppercase letters + 5 digits

**Source:** [psdevwiki.com/vita/Title_ID](https://www.psdevwiki.com/vita/Title_ID)

The Vita's SFO validator enforces the format `XXXXYYYYY` strictly.

| Example | Valid? | Why |
|---|---|---|
| `GBVX00001` | ✅ | 4 letters + 5 digits |
| `ABCD12345` | ✅ | 4 letters + 5 digits |
| `GBAVEX001` | ❌ | 6 letters + 3 digits |
| `myap00001` | ❌ | lowercase not allowed |
| `MYAPP0001` | ❌ | 5 letters + 4 digits |
| `ABCD1234`  | ❌ | only 8 chars (needs 9) |

In CMakeLists.txt:
```cmake
set(VITA_TITLEID "MYAP00001")  # 4 uppercase letters + 5 digits
```

---

## 2. CMakeLists.txt — vita_create_vpk best practices

### 2.1 Cleanest way to include LiveArea assets

Instead of listing every file individually, copy the entire `sce_sys/` directory:

```cmake
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    VERSION ${VITA_VERSION}
    NAME    ${VITA_APP_NAME}
    FILE sce_sys sce_sys
)
```

`FILE sce_sys sce_sys` means: copy `./sce_sys/` from the source tree into `sce_sys/` in the VPK. This includes icon0, pic0, livearea, and anything else you add later without touching CMakeLists again.

### 2.2 Required version format

`VITA_VERSION` must be formatted as `MM.mm` (two digits dot two digits):

```cmake
set(VITA_VERSION "01.00")  # not "1.0" or "1.00" or "01.0"
```

### 2.3 `vita_create_self` UNSAFE flag

For homebrew (not signed retail content), always add `UNSAFE`:
```cmake
vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME}.elf UNSAFE)
```
Without `UNSAFE`, the toolchain generates a self that requires official signing and will fail to run.

### 2.4 VitaSDK `vita.cmake` compatibility with CMake 4.x

VitaSDK's bundled `vita.cmake` uses `cmake_minimum_required(2.8)` internally, which CMake 4.x rejects. Always pass:
```bash
cmake ... -DCMAKE_POLICY_VERSION_MINIMUM=3.5
```
Or add to CMakeLists.txt before the `project()` call:
```cmake
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
```

---

## 3. Runtime crashes and instability

### 3.1 Newlib heap — set it explicitly

If your app crashes on launch or when allocating memory, the default newlib heap is too small. Set it globally in your main source file:
```c
unsigned int _newlib_heap_size_user = 192 * 1024 * 1024; // 192 MB
```
The Vita gives user apps ~352 MB. Common values: 128–256 MB depending on your app's needs.

### 3.2 Vita auto-suspend kills emulation / long tasks

Without periodic watchdog ticks, the Vita dims the screen and suspends after ~1 minute. Call this every frame or in your main loop:
```c
#include <psp2/kernel/processmgr.h>
sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
```
Options:
- `SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND` — prevents sleep but allows screen dimming
- `SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING` — prevents screen dimming too
- `SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF` — prevents screen off too

### 3.3 `sceKernelPowerTick` is in `processmgr.h`, not `power.h`

A common include mistake:
```c
// WRONG — sceKernelPowerTick does not live here
#include <psp2/power.h>

// CORRECT
#include <psp2/kernel/processmgr.h>
```

### 3.4 JIT/dynarec memory — use `sceKernelOpenVMDomain()`

For JIT code (emulators, JIT compilers), allocate memory with the uncached type and open the VM domain:
```c
SceUID block = sceKernelAllocMemBlock("jit_cache",
    SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);
void *base;
sceKernelGetMemBlockBase(block, &base);

int rc = sceKernelOpenVMDomain();
if (rc < 0) {
    // VM domain failed — fall back to interpreter, do NOT execute JIT code
}
// JIT code can now be written to base and executed
```
Always check `sceKernelOpenVMDomain()` return value. On unusual CFW configs it can fail silently and cause an access violation when JIT code runs.

### 3.5 SceNet requires specific init order

```c
// CORRECT ORDER — do not swap these
sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
sceNetCtlInit();                     // NetCtl BEFORE Net
SceNetInitParam np = { .memory = buf, .size = bufsize, .flags = 0 };
sceNetInit(&np);
```

### 3.6 `SCE_SYSMODULE_HTTPS` — don't load it unless you use HTTPS

Loading it adds startup time and consumes memory for no benefit if you never make HTTPS calls. Only load what you actually use.

---

## 4. Performance — VitaSDK slow I/O bug

**Source:** [gbatemp.net VitaSDK PSA](https://gbatemp.net/threads/psa-for-developers-if-you-have-performance-issues-in-your-homebrew-recompile-with-latest-vitasdk.466917/)

Old VitaSDK builds (pre SDK v274, roughly pre-2017) had a newlib reentrancy/TLS bug that made `fopen`, `fread`, `fwrite`, `std::fstream`, and `pthreads` up to **25× slower** than they should be.

**Fix:** Recompile with VitaSDK ≥ v274. No code changes needed.

To check your SDK version:
```bash
cat $VITASDK/version_info.txt | head -3
```

The devkitPro bundled VitaSDK (as of 2022+) is fixed.

---

## 5. vita2d specific issues

### 5.1 `vita2d_load_default_pgf()` leaks memory if called every frame

This function allocates a new font object each call. Call it **once at startup** and cache the pointer:
```c
// In init:
vita2d_pgf *g_font = vita2d_load_default_pgf();

// In draw (reuse g_font, don't call vita2d_load_default_pgf() here):
vita2d_pgf_draw_text(g_font, x, y, color, scale, text);

// In shutdown:
vita2d_free_pgf(g_font);
```

### 5.2 `vita2d_set_vblank_wait(0)` for uncapped frame rate

For fast-forward or benchmark modes, disable vsync to remove the 60 Hz cap:
```c
vita2d_set_vblank_wait(0);  // unlocked
// ... do your fast work ...
vita2d_set_vblank_wait(1);  // restore normal vsync
```

### 5.3 vita2d texture format for emulators

| System | Pixel format | vita2d format constant |
|---|---|---|
| GBA (RGB565) | 16-bit | `SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB` |
| GB/GBC/output (XBGR) | 32-bit | `SCE_GXM_TEXTURE_FORMAT_X8U8U8U8_1BGR` |

Texture stride must be a power of 2:
```c
vita2d_texture *tex = vita2d_create_empty_texture_format(
    next_pow2(width), next_pow2(height),
    SCE_GXM_TEXTURE_FORMAT_U5U6U5_RGB);
```

---

## 6. Screenshot — sceDisplayGetFrameBuf channel order

The Vita framebuffer is **BGRA**, not RGBA. When converting to PNG:
```c
uint32_t px = framebuf[y * pitch + x];
row[x*3+0] = (px >> 16) & 0xFF;  // R (bits 23:16)
row[x*3+1] = (px >>  8) & 0xFF;  // G (bits 15:8)
row[x*3+2] = (px >>  0) & 0xFF;  // B (bits 7:0)
// alpha (bits 31:24) is ignored for PNG RGB output
```
Getting this backwards produces blue-tinted screenshots.

---

## 7. Audio

### 7.1 `sceAudioOutSetVolume` — correct parameter type

```c
// WRONG — mixing up format enum as channel flag causes implicit conversion warning
sceAudioOutSetVolume(port, SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO, &vol);

// CORRECT flag for stereo volume set
sceAudioOutSetVolume(port, SCE_AUDIO_OUT_PARAM_MODE_STEREO, &vol);
// Or check your SDK's SceAudioOutChannelFlag definition
```

### 7.2 sceAudioOut batch size limit — max 1024 frames per call

```c
#define MAX_AUDIO_BATCH 1024
// Split larger buffers:
int done = 0;
while (done < total_frames) {
    int batch = MIN(total_frames - done, MAX_AUDIO_BATCH);
    sceAudioOutOutput(port, buf + done * channels);
    done += batch;
}
```

---

## 8. Quick checklist for new PSVita projects

Before building your first VPK, verify all of these:

```
VPK PACKAGING
[ ] TITLEID = exactly 4 uppercase letters + 5 digits (e.g. MYAP00001)
[ ] VERSION = "MM.mm" format (e.g. "01.00")
[ ] vita_create_self() uses UNSAFE flag
[ ] sce_sys/icon0.png — 128x128, 8-bit palette, no alpha
[ ] sce_sys/pic0.png — 960x544, 8-bit palette, 256 colors  ← often forgotten
[ ] sce_sys/livearea/contents/bg0.png — 840x500, 8-bit palette  ← must be bg0 not bg
[ ] sce_sys/livearea/contents/startup.png — 280x158, 8-bit palette
[ ] template.xml — style="a1", references bg0.png
[ ] CMakeLists uses FILE sce_sys sce_sys or lists all files individually
[ ] -DCMAKE_POLICY_VERSION_MINIMUM=3.5 passed to cmake (CMake 4.x compat)

RUNTIME
[ ] _newlib_heap_size_user set in main source (128-256 MB)
[ ] sceKernelPowerTick called every frame (prevents auto-suspend)
[ ] All sceKernelOpenVMDomain() return values checked
[ ] vita2d_load_default_pgf() called once, pointer cached — NOT called per frame

BUILD
[ ] VitaSDK version >= v274 (fixes 25x fopen slowdown)
[ ] SceNetCtlInit() called before sceNetInit() if using networking
[ ] Only load SCE modules you actually use
```

---

## 9. Quick PNG generator script (no external tools needed)

Save as `scripts/gen_vita_assets.py` — run from project root:

```python
"""
Generate all required PSVita LiveArea PNG assets.
Forces 8-bit palette depth in IHDR — required by Vita firmware.
PIL's convert('P') is NOT sufficient (picks min bit depth = 1/2/4 for simple images).
"""
import struct, zlib, os
from PIL import Image

def build_vita_png(path, width, height, bg_color=(10, 20, 50)):
    img = Image.new('RGB', (width, height), bg_color)
    img_p = img.convert('P', palette=Image.ADAPTIVE, colors=256)
    pal = img_p.palette.tobytes()
    pal += b'\x00' * (768 - len(pal))  # pad to 256 entries
    pixels = img_p.tobytes()

    def chunk(tag, data):
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', crc)

    raw = b''.join(b'\x00' + pixels[y*width:(y+1)*width] for y in range(height))
    png  = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 3, 0, 0, 0))
    png += chunk(b'PLTE', pal)
    png += chunk(b'IDAT', zlib.compress(raw, 9))
    png += chunk(b'IEND', b'')

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(png)
    print(f'Generated {path} ({width}x{height}, depth=8 palette)')

# Generate all required assets
build_vita_png('sce_sys/icon0.png',                            128, 128)
build_vita_png('sce_sys/pic0.png',                             960, 544)
build_vita_png('sce_sys/livearea/contents/bg0.png',            840, 500)
build_vita_png('sce_sys/livearea/contents/startup.png',        280, 158)

# Write template.xml
xml = '''<?xml version="1.0" encoding="utf-8"?>
<livearea style="a1" format-ver="01.00" content-rev="1">
  <livearea-background>
    <image>bg0.png</image>
  </livearea-background>
  <gate>
    <startup-image>startup.png</startup-image>
  </gate>
</livearea>
'''
os.makedirs('sce_sys/livearea/contents', exist_ok=True)
with open('sce_sys/livearea/contents/template.xml', 'w') as f:
    f.write(xml)
print('Generated template.xml')

print('\nRun this from your project root before building the VPK.')
print('Replace placeholder colors with your actual artwork.')
```

Run it:
```bash
python3 scripts/gen_vita_assets.py
```

Then verify the output is correct before building:
```bash
python3 -c "
import zipfile, struct
with zipfile.ZipFile('build/MyApp.vpk','r') as z:
    for i in z.infolist():
        if not i.filename.endswith('.png'): continue
        d = z.read(i.filename); pos = 8
        while pos+8<=len(d):
            L=struct.unpack('>I',d[pos:pos+4])[0]
            if d[pos+4:pos+8]==b'IHDR':
                w,h=struct.unpack('>II',d[pos+8:pos+16])
                bd,ct=d[pos+16],d[pos+17]
                print(f'{'OK' if bd==8 and ct==3 else 'FAIL'} {i.filename}: {w}x{h} depth={bd} ct={ct}')
                break
            pos+=12+L
"
```

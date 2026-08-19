from PIL import Image
import struct, zlib, os

def build_vita_png(path, width, height, bg_color=(10, 20, 50)):
    img = Image.new("RGB", (width, height), bg_color)
    img_p = img.convert("P", palette=Image.ADAPTIVE, colors=256)
    pal = img_p.palette.tobytes()
    pal += b"\x00" * (768 - len(pal))
    pixels = img_p.tobytes()

    def chunk(tag, data):
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = b"".join(b"\x00" + pixels[y*width:(y+1)*width] for y in range(height))
    png  = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0))
    png += chunk(b"PLTE", pal)
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)
    print(f"Generated {path} ({width}x{height}, depth=8 palette)")

build_vita_png("sce_sys/icon0.png",                           128, 128)
build_vita_png("sce_sys/pic0.png",                            960, 544)
build_vita_png("sce_sys/livearea/contents/bg0.png",           840, 500)
build_vita_png("sce_sys/livearea/contents/startup.png",       280, 158)

xml = """<?xml version="1.0" encoding="utf-8"?>
<livearea style="a1" format-ver="01.00" content-rev="1">
  <livearea-background>
    <image>bg0.png</image>
  </livearea-background>
  <gate>
    <startup-image>startup.png</startup-image>
  </gate>
</livearea>
"""
os.makedirs("sce_sys/livearea/contents", exist_ok=True)
with open("sce_sys/livearea/contents/template.xml", "w") as f:
    f.write(xml)
print("Generated template.xml")
print("All PSVita LiveArea assets generated. Replace colors/images with your artwork.")

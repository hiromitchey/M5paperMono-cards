#!/usr/bin/env python3
"""アイコン画像を 240×240・4階調にディザし、PNGとスケッチ埋め込み用ヘッダを作る。

  python3 tools/make_icon.py icon_src.jpg

出力:
  papermono-proto/data/icon.png   (LittleFSに置く場合)
  papermono-proto/icon_png.h      (スケッチに埋め込む場合。あれば自動で使われる)

要 Pillow:  sudo apt install python3-pil  または  pip install pillow
"""
import pathlib
import sys

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
SKETCH = ROOT / "papermono-proto"

if len(sys.argv) < 2:
    sys.exit(__doc__)

im = Image.open(sys.argv[1]).convert("L")
# 中央を正方形に切り抜く
w, h = im.size
s = min(w, h)
im = im.crop(((w - s) // 2, (h - s) // 2, (w - s) // 2 + s, (h - s) // 2 + s)).resize((240, 240), Image.LANCZOS)

pal = Image.new("P", (1, 1))
pal.putpalette([0, 0, 0, 85, 85, 85, 170, 170, 170, 255, 255, 255] + [0] * 756)
out = im.convert("RGB").quantize(palette=pal, dither=Image.Dither.FLOYDSTEINBERG).convert("L")

png_path = SKETCH / "data/icon.png"
png_path.parent.mkdir(parents=True, exist_ok=True)
out.save(png_path, optimize=True)

data = png_path.read_bytes()
lines = []
for i in range(0, len(data), 16):
    lines.append("  " + ", ".join(f"0x{b:02x}" for b in data[i : i + 16]) + ",")
header = (
    "// tools/make_icon.py で生成。手で編集しない\n"
    "#pragma once\n#include <cstdint>\n\n"
    f"static const uint8_t kIconPng[{len(data)}] = {{\n" + "\n".join(lines) + "\n};\n"
)
(SKETCH / "icon_png.h").write_text(header, encoding="utf-8")
print(f"{png_path} ({len(data)} bytes) と icon_png.h を作成")

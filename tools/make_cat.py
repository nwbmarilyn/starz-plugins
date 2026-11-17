import sys
from PIL import Image, ImageDraw

src, out_png, out_h = sys.argv[1], sys.argv[2], sys.argv[3]
im = Image.open(src).convert("RGB")
w, h = im.size
s = min(w, h)
im = im.crop(((w - s) // 2, (h - s) // 2, (w - s) // 2 + s, (h - s) // 2 + s)).resize((192, 192), Image.LANCZOS)
mask = Image.new("L", (192 * 4, 192 * 4), 0)
ImageDraw.Draw(mask).ellipse((0, 0, 192 * 4 - 1, 192 * 4 - 1), fill=255)
mask = mask.resize((192, 192), Image.LANCZOS)
rgba = im.convert("RGBA")
rgba.putalpha(mask)
rgba.save(out_png, optimize=True)
data = open(out_png, "rb").read()
with open(out_h, "w") as f:
    f.write("#pragma once\n#include <cstddef>\nstatic const unsigned char kCatPng[] = {\n")
    for i in range(0, len(data), 24):
        f.write(",".join(str(b) for b in data[i:i + 24]) + ",\n")
    f.write("};\nstatic const size_t kCatPngSize = %d;\n" % len(data))
print(len(data))

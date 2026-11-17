import sys
import numpy as np
from PIL import Image

src, out_png, out_h = sys.argv[1], sys.argv[2], sys.argv[3]
im = Image.open(src).convert('L')
a = np.array(im)
ys, xs = np.where(a > 128)
cx = (xs.min() + xs.max()) / 2
cy = (ys.min() + ys.max()) / 2
r = max(xs.max() - xs.min(), ys.max() - ys.min()) / 2 + 3
box = (int(cx - r), int(cy - r), int(cx + r), int(cy + r))
c = im.crop(box).resize((192, 192), Image.LANCZOS)
yy, xx = np.mgrid[0:192, 0:192]
d = np.sqrt((xx - 95.5) ** 2 + (yy - 95.5) ** 2)
alpha = np.clip((96.5 - d) * 2.2, 0, 1) * 255
rgba = np.dstack([np.array(c)] * 3 + [alpha.astype(np.uint8)])
Image.fromarray(rgba.astype(np.uint8), 'RGBA').save(out_png, optimize=True)
data = open(out_png, 'rb').read()
lines = ['#pragma once', '#include <cstddef>', 'static const unsigned char kLogoPng[] = {']
for i in range(0, len(data), 24):
    lines.append(','.join(str(b) for b in data[i:i + 24]) + ',')
lines += ['};', 'static const size_t kLogoPngSize = sizeof(kLogoPng);']
open(out_h, 'w').write('\n'.join(lines) + '\n')
print(len(data), box)

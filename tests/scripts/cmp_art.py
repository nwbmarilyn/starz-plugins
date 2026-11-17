import sys
import numpy as np, soundfile as sf, librosa

GM = [7, 9, 10, 0, 2, 3, 5]
ref = 436.0
names = ["dry", "art", "ours"]
tracks = {}
for n in names:
    y, sr = sf.read(r"D:\arktest\v_%s.wav" % n)
    x = y.mean(1) if y.ndim > 1 else y
    z = librosa.resample(x, orig_sr=sr, target_sr=22050)
    f, _, _ = librosa.pyin(z, fmin=70, fmax=900, sr=22050, frame_length=2048, hop_length=256, fill_na=np.nan)
    tracks[n] = (f, x, sr)
    print("pyin done", n, flush=True)


def gd(f):
    m = 69 + 12 * np.log2(f / ref)
    best = np.full(len(m), 99.0)
    for o in range(-12, 140, 12):
        for s in GM:
            best = np.minimum(best, np.abs(m - (s + o)))
    return best * 100


hop = 256 / 22050
fd = tracks["dry"][0]
windows = {"whole": (0, 1e9), "3 takes 19-24s": (19.0, 24.0), "after 50s": (50, 1e9)}
for wn, (a, b) in windows.items():
    ia, ib = int(a / hop), min(len(fd), int(b / hop))
    print("--", wn)
    for n in names:
        f = tracks[n][0][ia:ib]
        ok = ~np.isnan(f) & (f > 75)
        if ok.sum() < 10:
            print("   %-5s too few voiced" % n)
            continue
        g = gd(f[ok])
        print("   %-5s voiced frames %5d  grid med %5.1f c  <10c %.2f  <25c %.2f  >50c %.2f" % (n, ok.sum(), np.median(g), (g < 10).mean(), (g < 25).mean(), (g > 50).mean()))

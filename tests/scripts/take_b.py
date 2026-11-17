import numpy as np, soundfile as sf, librosa

GM = [7, 9, 10, 0, 2, 3, 5]
ref = 436.0


def trk(path):
    y, sr = sf.read(path)
    x = y if y.ndim == 1 else y[:, 0]
    z = librosa.resample(x, orig_sr=sr, target_sr=22050)
    f, _, _ = librosa.pyin(z, fmin=70, fmax=900, sr=22050, frame_length=2048, hop_length=256, fill_na=np.nan)
    return f


def gd1(f):
    m = 69 + 12 * np.log2(f / ref)
    best = 99.0
    bn = 0
    for o in range(-12, 140, 12):
        for s in GM:
            d = abs(m - (s + o))
            if d < best:
                best, bn = d, s + o
    return best * 100, 440.0 * 2 ** ((bn - 69) / 12.0) * 2 ** (-16 / 1200)


fd = trk(r"D:\arktest\m3_b.wav")
fo = trk(r"D:\arktest\m3s_b.wav")
dd = np.loadtxt(r"D:\arktest\m3s_b.wav.f0.csv", delimiter=",")
te, fe = dd[:, 0], dd[:, 1]
hop = 256 / 22050
rows = []
for i in range(len(fo)):
    if np.isnan(fo[i]):
        continue
    g, tf = gd1(fo[i])
    if g > 50:
        t = i * hop
        e = fe[(np.searchsorted(te, t) - 1).clip(0, len(fe) - 1)]
        rows.append((t, fo[i], fd[i] if i < len(fd) else np.nan, e, tf))
print("frames >50c: %d" % len(rows))
for r in rows[:30:2]:
    print("t %.2f  out %.0f Hz | dry(pyin) %s Hz | engine %.0f Hz | nearest grid %.0f Hz" % (r[0], r[1], "%.0f" % r[2] if not np.isnan(r[2]) else "-", r[3], r[4]))

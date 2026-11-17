import os, sys, subprocess
import numpy as np, soundfile as sf, librosa

d = r"D:\class FLPjcts\сведение\_работа\test_222"
names = {"a": "11-26-44", "b": "11-26-50", "c": "11-27-04"}
files = {k: [f for f in os.listdir(d) if v in f and f.endswith(".wav")][0] for k, v in names.items()}
bar = 4 * 60 / 107.484
start = {"a": 9.55, "b": 9.52, "c": 9.55}
t0 = min(start.values())
tr = {}
sr0 = None
for k, f in files.items():
    y, sr0 = sf.read(os.path.join(d, f))
    x = y.mean(1) if y.ndim > 1 else y
    tr[k] = x
n = int(max((start[k] - t0) * bar * sr0 + len(tr[k]) for k in tr)) + 100
mix = np.zeros(n)
for k in tr:
    o = int((start[k] - t0) * bar * sr0)
    mix[o:o + len(tr[k])] += tr[k]
    one = np.zeros(n)
    one[o:o + len(tr[k])] = tr[k]
    sf.write(r"D:\arktest\m3_%s.wav" % k, one.astype(np.float32), sr0, subtype="FLOAT")
mix = mix / max(1.0, np.abs(mix).max() / 0.9)
sf.write(r"D:\arktest\m3_sum.wav", mix.astype(np.float32), sr0, subtype="FLOAT")
print("sr", sr0, "dur %.2f" % (n / sr0), "offsets ms", {k: round((start[k] - t0) * bar * 1000) for k in start})


def stats(name, extra):
    inp = r"D:\arktest\m3_%s.wav" % name
    subprocess.run([r"D:\arktest\ark_test.exe", inp, r"D:\arktest\m3o.wav", "artist", "128", "ref", "436", "minf0", "110"] + extra, capture_output=True, timeout=120)
    dd = np.loadtxt(r"D:\arktest\m3o.wav.f0.csv", delimiter=",")
    te, fe = dd[:, 0], dd[:, 1]
    x, sr = sf.read(inp)
    seg = (te > 0.0) & (te < len(x) / sr)
    act = np.sqrt(np.convolve(x ** 2, np.ones(2048) / 2048, "same")) > 0.01
    ia = act[(te[seg] * sr).astype(int).clip(0, len(x) - 1)]
    v = fe[seg] > 0
    both = v[1:] & v[:-1]
    c = 1200 * np.abs(np.log2(np.where(both, fe[seg][1:] / np.where(fe[seg][:-1] > 0, fe[seg][:-1], 1), 1)))
    jumps = (c > 100) & both
    print("%-4s active-voice time %.1f s | engine voiced during activity %.2f | jumps>100c per second of voiced %.2f | median |step| %.1f c" % (
        name, act.mean() * len(x) / sr, v[ia].mean() if ia.sum() else 0, jumps.sum() / max(0.1, v.sum() * 128 / sr), np.median(c[both]) if both.sum() else 0))


for nm in ("a", "b", "c", "sum"):
    stats(nm, [])

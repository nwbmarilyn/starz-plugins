import sys, subprocess
import numpy as np, soundfile as sf

extra = sys.argv[1:]
for nm in ("a", "b", "c", "sum"):
    inp = r"D:\arktest\m3_%s.wav" % nm
    subprocess.run([r"D:\arktest\ark_test.exe", inp, r"D:\arktest\m3o.wav", "artist", "128", "ref", "436", "minf0", "110"] + extra, capture_output=True, timeout=120)
    dd = np.loadtxt(r"D:\arktest\m3o.wav.f0.csv", delimiter=",")
    te, fe = dd[:, 0], dd[:, 1]
    x, sr = sf.read(inp)
    act = np.sqrt(np.convolve(x ** 2, np.ones(2048) / 2048, "same")) > 0.01
    seg = (te > 0) & (te < len(x) / sr)
    ia = act[(te[seg] * sr).astype(int).clip(0, len(x) - 1)]
    v = fe[seg] > 0
    both = v[1:] & v[:-1]
    c = 1200 * np.abs(np.log2(np.where(both, fe[seg][1:] / np.where(fe[seg][:-1] > 0, fe[seg][:-1], 1), 1)))
    jumps = ((c > 100) & both).sum()
    print("%-4s voiced during activity %.2f  jumps>100c per s %.2f" % (nm, v[ia].mean(), jumps / max(0.1, v.sum() * 128 / sr)))

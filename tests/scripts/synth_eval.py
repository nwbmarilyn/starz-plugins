import sys, subprocess
import numpy as np

extra = sys.argv[1:]
r = subprocess.run([r"D:\arktest\ark_test.exe", r"D:\arktest\syn.wav", r"D:\arktest\syn_out.wav", "artist", "512", "ref", "440"] + extra, capture_output=True, timeout=120)
t, fo = np.load(r"D:\arktest\syn_truth.npy")
d = np.loadtxt(r"D:\arktest\syn_out.wav.f0.csv", delimiter=",")
te, fe = d[:, 0], d[:, 1]
res = {}
for sh in (0.0, 0.0145):
    idx = np.searchsorted(t, te + sh).clip(0, len(t) - 1)
    ft = fo[idx]
    ok = fe > 0
    c = 1200 * np.log2(fe[ok] / ft[ok])
    oc = np.abs(np.abs(c) - 1200) < 200
    cc = c[~oc]
    res[sh] = (ok.mean(), oc.mean(), np.percentile(np.abs(cc), 50), np.percentile(np.abs(cc), 90), (np.abs(cc) < 10).mean())
best = min(res, key=lambda k: res[k][2])
v = res[best]
print("syn: voiced %.2f octave %.3f | err p50 %.1f p90 %.1f c within10 %.2f (time offset %.1f ms)" % (v[0], v[1], v[2], v[3], v[4], best * 1000))

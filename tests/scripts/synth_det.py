import sys, subprocess
import numpy as np, soundfile as sf

sr = 44100
rng = np.random.default_rng(7)
notes = [110, 123, 130.8, 146.8, 164.8, 196, 220, 246.9, 261.6, 293.7, 329.6, 392, 164.8, 123, 98, 130.8]
dur_each = 0.7
segs = []
f = []
for i, n in enumerate(notes):
    seg = np.full(int(sr * dur_each), n)
    segs.append(seg)
fo = np.concatenate(segs)
k = int(0.06 * sr)
fo = np.convolve(np.pad(fo, (k, k), mode="edge"), np.ones(k) / k, "same")[k:-k]
t = np.arange(len(fo)) / sr
fo = fo * 2 ** ((30 * np.sin(2 * np.pi * 5.5 * t) * (t > 0.3)) / 1200.0)
ph = 2 * np.pi * np.cumsum(fo) / sr
x = np.zeros_like(fo)
for h in range(1, 30):
    form = 1.0 / h
    for cf, bw, g in ((700, 130, 1.0), (1220, 70, 0.6), (2600, 160, 0.3)):
        form *= 1 + g * np.exp(-0.5 * ((h * 0) + 0) ** 2)
    fh = h * fo
    x += np.sin(h * ph) * (1.0 / h) * (1 + 2 * np.exp(-0.5 * ((fh - 700) / 160) ** 2) + 1.2 * np.exp(-0.5 * ((fh - 1220) / 120) ** 2))
env = np.ones_like(x)
a = int(0.015 * sr)
for i in range(len(notes)):
    s = int(i * dur_each * sr)
    env[s:s + a] = np.linspace(0.2, 1, a)
x = x / np.abs(x).max() * 0.4 * env
x += rng.normal(0, 0.004, len(x))
sf.write(r"D:\arktest\syn.wav", x.astype(np.float32), sr, subtype="FLOAT")
np.save(r"D:\arktest\syn_truth.npy", np.stack([t, fo]))
print("syn ok %.1f s" % (len(x) / sr))

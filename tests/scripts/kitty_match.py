import subprocess, sys
import numpy as np, soundfile as sf, scipy.signal as ss

srcs = [r"D:\arktest\m3_a.wav", r"D:\arktest\m3_b.wav", r"D:\arktest\m3_c.wav"]
extra = sys.argv[1:]


def bands(y, v, n):
    m = y[:n].mean(1)
    f, P = ss.welch(m[v], 44100, nperseg=2048)
    b = lambda a, c: 10 * np.log10(P[(f >= a) & (f < c)].mean() + 1e-20)
    return np.array([b(200, 1000), b(1000, 4000), b(4000, 8000), b(8000, 16000)]), 10 * np.log10((m[v] ** 2).mean() + 1e-20)


for vn in (2, 8, 24):
    rows = []
    for s in srcs:
        x, sr = sf.read(s)
        x = x if x.ndim == 1 else x.mean(1)
        env = np.sqrt(np.convolve(x ** 2, np.ones(882) / 882, "same"))
        v = env > 0.02
        subprocess.run([r"D:\arktest\ark_test.exe", s, r"D:\arktest\kk0.wav", "artist", "128", "ref", "436"], capture_output=True)
        subprocess.run([r"D:\arktest\ark_test.exe", s, r"D:\arktest\kk1.wav", "kitty", "128", "ref", "436", "voices", str(vn)] + extra, capture_output=True)
        y0, _ = sf.read(r"D:\arktest\kk0.wav")
        y1, _ = sf.read(r"D:\arktest\kk1.wav")
        b0, r0 = bands(y0, v, len(x))
        b1, r1 = bands(y1, v, len(x))
        rows.append(np.append(b1 - b0, r1 - r0))
    m = np.mean(rows, 0)
    print("voices %2d  band diff vs tuner dB: 0.2-1k %.1f  1-4k %.1f  4-8k %.1f  8-16k %.1f  | broadband %.1f" % (vn, *m))

import subprocess
import numpy as np, soundfile as sf

src = r"D:\arktest\m3_a.wav"
x, sr = sf.read(src)
x = x if x.ndim == 1 else x.mean(1)
cases = {
    "reverb 2.5s": ["rmix", "0.35", "decay", "2.5", "pre", "25", "rduck", "0.6"],
    "delay 330ms": ["dmix", "0.4", "dtime", "330", "dfeed", "0.45", "ping", "1", "dduck", "0.6"],
    "both": ["rmix", "0.3", "decay", "2.2", "dmix", "0.35", "dtime", "330", "dfeed", "0.4", "rduck", "0.5", "dduck", "0.5"],
}
win = int(0.02 * sr)
env = np.sqrt(np.convolve(x ** 2, np.ones(win) / win, "same"))
voiced = env > 0.02
quiet = env < 0.003
for nm, a in cases.items():
    subprocess.run([r"D:\arktest\fx_test.exe", src, r"D:\arktest\fxv.wav"] + a, check=True, capture_output=True)
    y, _ = sf.read(r"D:\arktest\fxv.wav")
    n = len(x)
    wet = y[:n] - x[:, None]
    wm = wet.mean(1)
    fin = np.isfinite(y).all()
    d = np.abs(np.diff(wm))
    sm = np.convolve(np.abs(wm), np.ones(64) / 64, "same") + 1e-9
    spike = (d / sm[1:]).max()
    r = lambda s: 20 * np.log10(np.sqrt((wm[s] ** 2).mean()) + 1e-12)
    tl = y[n:]
    tail_db = 20 * np.log10(np.sqrt((tl[:int(.5 * sr)] ** 2).mean()) + 1e-12), 20 * np.log10(np.sqrt((tl[-sr:] ** 2).mean()) + 1e-12)
    print("%-12s finite %s  peak out %.2f | wet rms in voice %.1f dB, in gaps %.1f dB, dry rms %.1f dB | worst step/local level %.1f | tail first 0.5s %.1f dB -> last 1s %.1f dB" % (nm, fin, np.abs(y).max(), r(voiced[:n]), r(quiet[:n]), 20 * np.log10(np.sqrt((x[voiced] ** 2).mean())), spike, *tail_db))

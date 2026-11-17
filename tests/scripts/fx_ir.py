import numpy as np, soundfile as sf, scipy.signal as ss

for nm, want in (("a", 1.8), ("b", 4.0), ("c", 0.8)):
    x, sr = sf.read(r"D:\arktest\imp_%s.wav" % nm)
    x = x.copy(); x[100, :] -= 1.0
    m = x.mean(1)
    ok = np.isfinite(x).all()
    e = np.cumsum((m[::-1] ** 2))[::-1]
    e = 10 * np.log10(e / e[0] + 1e-20)
    t = np.arange(len(e)) / sr
    i5 = np.argmax(e < -5)
    i35 = np.argmax(e < -35)
    rt60 = 2 * (t[i35] - t[i5]) if i35 > i5 else float("nan")
    L, R = x[:, 0], x[:, 1]
    c = np.corrcoef(L[:int(1.5 * sr)], R[:int(1.5 * sr)])[0, 1]
    f, P = ss.welch(m[:int(2 * sr)], sr, nperseg=4096)
    def bd(a, b):
        k = (f >= a) & (f < b)
        return 10 * np.log10(P[k].mean() + 1e-20)
    pk = np.abs(m).max()
    d = np.abs(np.diff(m))
    print("%s want RT60 %.1f got %.2f s | finite %s peak %.3f | L/R corr %.2f | spectrum dB: 200-500 %.1f 1-2k %.1f 4-6k %.1f 8-12k %.1f" % (nm, want, rt60, ok, pk, c, bd(200, 500), bd(1000, 2000), bd(4000, 6000), bd(8000, 12000)))
    t0 = np.argmax(np.abs(m) > 1e-4) / sr
    print("   first energy at %.1f ms, early energy in 0-80 ms %.1f dB vs 80-300 ms %.1f dB" % (t0 * 1000, 10 * np.log10((m[:int(.08 * sr)] ** 2).sum() + 1e-20), 10 * np.log10((m[int(.08 * sr):int(.3 * sr)] ** 2).sum() + 1e-20)))

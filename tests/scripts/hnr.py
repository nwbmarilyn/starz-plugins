import sys
import numpy as np, soundfile as sf


def hnr_stats(path):
    y, sr = sf.read(path)
    x = y.mean(1) if y.ndim > 1 else y
    n = int(0.04 * sr)
    hop = int(0.01 * sr)
    lo, hi = int(sr / 400), int(sr / 70)
    vals = []
    for i in range(0, len(x) - n - hi, hop):
        seg = x[i:i + n + hi]
        a = seg[:n]
        if np.sqrt(np.mean(a ** 2)) < 0.01:
            continue
        a = a - a.mean()
        best = 0
        for lag in range(lo, hi, 2):
            b = seg[lag:lag + n] - seg[lag:lag + n].mean()
            den = np.sqrt(np.sum(a * a) * np.sum(b * b)) + 1e-12
            r = np.sum(a * b) / den
            if r > best:
                best = r
        if best > 0.5:
            vals.append(10 * np.log10(best / max(1e-3, 1 - best)))
    v = np.array(vals)
    return len(v), np.median(v), np.percentile(v, 25)


if __name__ == "__main__":
  for p in sys.argv[1:]:
    n, m, q = hnr_stats(p)
    print("%-22s frames %4d  HNR median %.1f dB  p25 %.1f dB" % (p.split("\\")[-1], n, m, q))

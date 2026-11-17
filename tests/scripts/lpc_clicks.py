import os
TOL = float(os.environ.get("TOL", "0.03"))
import sys
import numpy as np, soundfile as sf, librosa
from scipy.signal import fftconvolve, lfilter

dry_p, wet_p = sys.argv[1], sys.argv[2]


def mono(p, ch=None):
    y, sr = sf.read(p)
    if y.ndim == 1:
        return y, sr
    return (y.mean(1) if ch is None else y[:, ch]), sr


xd, sr = mono(dry_p)
xw, _ = mono(wet_p)
n = min(len(xd), len(xw))
xd, xw = xd[:n], xw[:n]
seg = slice(int(2 * sr), int(12 * sr))
c = fftconvolve(xw[seg], xd[seg][::-1], "full")
mid = len(xd[seg]) - 1
win = c[mid - 400:mid + 401]
lag = int(np.argmax(np.abs(win)) - 400)
print("lag wet vs dry: %d samples = %.1f ms" % (lag, lag / sr * 1000))


def lpc_events(x, thr=7.0, order=28, blk=2048):
    ev = []
    res = np.zeros_like(x)
    for s in range(order, len(x) - blk, blk):
        seg = x[s - order:s + blk]
        if np.sqrt(np.mean(seg ** 2)) < 0.004:
            continue
        a = librosa.lpc(seg.astype(np.float64) + 1e-9 * np.random.randn(len(seg)), order=order)
        r = lfilter(a, [1.0], seg)[order:]
        res[s:s + blk] = r
        mad = np.median(np.abs(r)) * 1.4826 + 1e-7
        idx = np.where(np.abs(r) > thr * mad)[0]
        for i in idx:
            t = (s + i) / sr
            if not ev or t - ev[-1][0] > 0.02:
                ev.append([t, abs(r[i]) / mad])
            elif abs(r[i]) / mad > ev[-1][1]:
                ev[-1][1] = abs(r[i]) / mad
    return ev


for ch in (0, 1):
    xw_c, _ = mono(wet_p, ch)
    xw_c = xw_c[:n]
    if lag > 0:
        xw_a = xw_c[lag:]
        xd_a = xd[:len(xw_a)]
    else:
        xw_a = xw_c
        xd_a = xd
    for thr in (15.0,):
        ed = lpc_events(xd_a, thr)
        ew = lpc_events(xw_a, thr)
        td = np.array([e[0] for e in ed]) if ed else np.array([])
        new = [e for e in ew if not len(td) or np.min(np.abs(td - e[0])) > TOL]
        print("ch %d thr %.0f: dry %d wet %d new-in-wet %d : %s" % (ch, thr, len(ed), len(ew), len(new), " ".join("%.2f(%.0f)" % (e[0], e[1]) for e in new[:20])))

import numpy as np, soundfile as sf, scipy.signal as ss

x, sr = sf.read(r"D:\class FLPjcts\сведение\_работа\pharaoh_fx\acap.wav")
m = x.mean(1) if x.ndim > 1 else x
print("sr", sr, "dur %.1f s" % (len(m) / sr), "stereo" if x.ndim > 1 else "mono")
if x.ndim > 1:
    L, R = x[:, 0], x[:, 1]
    print("L/R corr %.3f  side/mid dB %.1f" % (np.corrcoef(L, R)[0, 1], 20 * np.log10(np.std(L - R) / np.std(L + R) + 1e-9)))
hop = int(0.005 * sr)
env = np.sqrt(np.convolve(m ** 2, np.ones(hop) / hop, "same"))[::hop]
db = 20 * np.log10(env + 1e-7)
pk = np.percentile(db, 98)
print("peak env %.1f dB" % pk)

act = db > pk - 30
tails = []
i = 0
while i < len(db) - 2:
    if act[i] and not act[i + 1]:
        j = i + 1
        while j < len(db) and not act[j]:
            j += 1
        gap = (j - i) * hop / sr
        if gap > 0.4:
            tails.append((i, j))
        i = j
    else:
        i += 1
print("gaps >0.4 s:", len(tails))
rt = []
for i, j in tails[:60]:
    seg = db[max(0, i - 6):j]
    t = np.arange(len(seg)) * hop / sr
    top = seg.max()
    lo = np.where(seg < top - 5)[0]
    hi = np.where(seg < top - 25)[0]
    if len(lo) and len(hi) and hi[0] > lo[0]:
        sl = (seg[hi[0]] - seg[lo[0]]) / (t[hi[0]] - t[lo[0]] + 1e-9)
        rt.append(-60 / sl if sl < 0 else np.nan)
rt = np.array(rt)
rt = rt[np.isfinite(rt) & (rt > 0) & (rt < 20)]
print("RT60 estimate from tails: median %.2f s  p25 %.2f  p75 %.2f  n=%d" % (np.median(rt), np.percentile(rt, 25), np.percentile(rt, 75), len(rt)))

d = env - env.mean()
ac = np.correlate(d, d, "full")[len(d) - 1:]
ac /= ac[0]
lags = np.arange(len(ac)) * hop / sr
pks, _ = ss.find_peaks(ac[:int(2.5 / (hop / sr))], prominence=0.03)
print("envelope autocorr peaks (s, strength):", [(round(lags[k], 3), round(ac[k], 2)) for k in pks[:10]])

f, t, S = ss.spectrogram(m, sr, nperseg=2048, noverlap=1536)
for i, j in tails[:20]:
    pass
bands = [(100, 400), (400, 1500), (1500, 4000), (4000, 8000), (8000, 16000)]
res = {b: [] for b in bands}
for i, j in tails:
    t0 = (i + 1) * hop / sr
    ia = np.searchsorted(t, t0 + 0.02)
    ib = np.searchsorted(t, t0 + 0.25)
    if ib - ia < 2 or ib >= len(t):
        continue
    ref = S[:, np.searchsorted(t, t0 - 0.1):np.searchsorted(t, t0)]
    if ref.shape[1] == 0:
        continue
    for b in bands:
        k = (f >= b[0]) & (f < b[1])
        res[b].append(10 * np.log10(S[k, ia:ib].mean() / (ref[k].mean() + 1e-20) + 1e-20))
print("tail level vs voice just before (dB), 20-250 ms after stop:")
for b in bands:
    print("  %5d-%5d Hz: median %.1f" % (b[0], b[1], np.median(res[b]) if res[b] else float("nan")))

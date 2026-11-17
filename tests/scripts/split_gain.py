import subprocess, sys
import numpy as np, soundfile as sf, librosa

GM = [7, 9, 10, 0, 2, 3, 5]
ref = 436.0


def trk(path):
    y, sr = sf.read(path)
    x = y if y.ndim == 1 else y[:, 0]
    z = librosa.resample(x, orig_sr=sr, target_sr=22050)
    f, _, _ = librosa.pyin(z, fmin=70, fmax=900, sr=22050, frame_length=2048, hop_length=256, fill_na=np.nan)
    return f


def gd(f):
    m = 69 + 12 * np.log2(f / ref)
    best = np.full(len(m), 99.0)
    for o in range(-12, 140, 12):
        for s in GM:
            best = np.minimum(best, np.abs(m - (s + o)))
    return best * 100


dry = {}
out = {}
for k in "abc":
    dry[k] = trk(r"D:\arktest\m3_%s.wav" % k)
    subprocess.run([r"D:\arktest\ark_test.exe", r"D:\arktest\m3_%s.wav" % k, r"D:\arktest\m3s_%s.wav" % k, "artist", "128", "ref", "436", "retune", "0.5", "minf0", "110"] + sys.argv[1:], capture_output=True, timeout=120)
    out[k] = trk(r"D:\arktest\m3s_%s.wav" % k)
    ok = ~np.isnan(out[k])
    g = gd(out[k][ok])
    print("take %s alone, tuned: voiced %d  <10c %.2f  <25c %.2f  >50c %.2f | dry <10c %.2f" % (k, ok.sum(), (g < 10).mean(), (g < 25).mean(), (g > 50).mean(), (gd(dry[k][~np.isnan(dry[k])]) < 10).mean()))
tot=[]
for k in "abc":
    ok = ~np.isnan(out[k])
    g = gd(out[k][ok])
    tot.append(((g < 10).mean(), (g > 50).mean(), ok.sum()))
print("MEAN over takes: <10c %.3f  >50c %.3f  voiced %d" % (np.mean([t[0] for t in tot]), np.mean([t[1] for t in tot]), sum(t[2] for t in tot)))

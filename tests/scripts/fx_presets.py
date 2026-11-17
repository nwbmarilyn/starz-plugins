import subprocess
import numpy as np, soundfile as sf

src = r"D:\arktest\m3_a.wav"
x, sr = sf.read(src)
x = x if x.ndim == 1 else x.mean(1)
n = len(x)
R = {
    "Klyukva Air": "mix 0.35 dry 1 decay 0.9 pre 15 damp 12000 lowcut 200 width 1 duck 0.3 mod 0.4",
    "Vocal Plate": "mix 0.3 dry 1 decay 1.4 pre 12 damp 9000 lowcut 220 width 1 duck 0.4 mod 0.3",
    "Tight Room": "mix 0.22 dry 1 decay 0.45 pre 5 damp 6500 lowcut 250 width 0.8 duck 0.2 mod 0.1",
    "Dark Hall": "mix 0.28 dry 1 decay 2.8 pre 30 damp 3500 lowcut 150 width 1 duck 0.5 mod 0.5",
    "Huge Bright": "mix 0.3 dry 1 decay 5 pre 45 damp 13000 lowcut 250 width 1 duck 0.6 mod 0.6",
    "Ghost": "mix 0.4 dry 1 decay 4 pre 0 damp 4500 lowcut 300 width 1 duck 0.8 mod 0.8",
    "Cathedral Dream": "mix 0.34 dry 1 decay 8 pre 60 damp 6000 lowcut 200 width 1 duck 0.7 mod 0.9",
    "Send Only": "mix 1 dry 0 decay 2 pre 20 damp 8000 lowcut 200 width 1 duck 0 mod 0.4",
}
D = {
    "Eighth Ping-Pong": "mix 0.35 dry 1 time 250 feed 0.4 low 250 high 5500 ping 1 duck 0.5 wobble 0 drive 0.2",
    "Slapback": "mix 0.28 dry 1 time 95 feed 0.08 low 200 high 6000 ping 0 duck 0.2 wobble 0 drive 0.1",
    "Dark Tape": "mix 0.33 dry 1 time 380 feed 0.55 low 300 high 2800 ping 0.6 duck 0.5 wobble 0.35 drive 0.45",
    "Quarter Echo": "mix 0.3 dry 1 time 500 feed 0.35 low 250 high 5000 ping 1 duck 0.6 wobble 0 drive 0.15",
    "Long Wash": "mix 0.3 dry 1 time 700 feed 0.7 low 350 high 3500 ping 1 duck 0.7 wobble 0.2 drive 0.3",
    "Triplet Bounce": "mix 0.32 dry 1 time 220 feed 0.45 low 250 high 6000 ping 1 duck 0.5 wobble 0 drive 0.2",
    "Wide Doubler": "mix 0.25 dry 1 time 45 feed 0.15 low 200 high 9000 ping 1 duck 0 wobble 0.5 drive 0.1",
    "Send Only": "mix 1 dry 0 time 300 feed 0.45 low 250 high 5500 ping 1 duck 0 wobble 0 drive 0.2",
}
win = int(0.02 * sr)
env = np.sqrt(np.convolve(x ** 2, np.ones(win) / win, "same"))
dry_rms = np.sqrt((x[env > 0.02] ** 2).mean())
for kind, table in (("reverb", R), ("delay", D)):
    for nm, a in table.items():
        subprocess.run([r"D:\arktest\fx_test.exe", kind, src, r"D:\arktest\fxp.wav"] + a.split(), check=True, capture_output=True)
        y, _ = sf.read(r"D:\arktest\fxp.wav")
        fin = np.isfinite(y).all()
        pk = np.abs(y).max()
        tail = 20 * np.log10(np.sqrt((y[-sr:] ** 2).mean()) + 1e-12)
        body = y[:n]
        wet = body - (x[:, None] if a.split()[3] != "0" else 0)
        wrms = 20 * np.log10(np.sqrt((wet.mean(1) ** 2).mean()) + 1e-12)
        sm = np.convolve(np.abs(wet.mean(1)), np.ones(64) / 64, "same") + 1e-4
        spike = (np.abs(np.diff(wet.mean(1))) / sm[1:]).max()
        print("%-6s %-17s finite %s peak %.2f | wet %.1f dB vs voice %.1f | step ratio %.1f | tail last 1s %.0f dB" % (kind, nm, fin, pk, wrms, 20 * np.log10(dry_rms), spike, tail))

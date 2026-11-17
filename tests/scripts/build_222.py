import os, re, struct, sys
sys.path.insert(0, r"D:\class FLPjcts\сведение")
from fltools.flp import Project
from fltools import edit

d = r"D:\class FLPjcts\сведение\_работа\test_222"
src = [f for f in os.listdir(d) if f.endswith(".flp") and not f.startswith("v_")][0]
mapping = {f: os.path.join(d, f) for f in os.listdir(d) if f.endswith(".wav")}
IDX = {"mix": 1, "ref": 4, "retune": 5, "voices": 7, "swarm": 19, "stepped": 15, "spread": 8, "drift": 9, "flange": 11, "fldepth": 12, "delspread": 13}
ARK = [(3, 1), (5, 1), (6, 1)]
ART = [(3, 0), (5, 0)]
clean = {"swarm": 0.0, "retune": 0.5 / 300.0, "ref": (436.0 - 415.0) / 51.0, "stepped": 0.0, "flange": 0.0, "fldepth": 0.0, "delspread": 0.0, "spread": 0.0, "drift": 0.0}
variants = {
    "v_dry": (False, False, {}),
    "v_art": (True, False, {}),
    "v_ours": (False, True, clean),
}
only = sys.argv[1:]
for name, (art_on, ark_on, ch) in variants.items():
    if only and name not in only:
        continue
    p = Project(os.path.join(d, src))
    edit.set_sample_paths(p, mapping)
    for ins, sl in ART:
        edit.set_slot_enabled(p, ins, sl, art_on)
    for ins, sl in ARK:
        edit.set_slot_enabled(p, ins, sl, ark_on)
    if ch:
        for pl in edit.plugins(p):
            if "state" not in pl:
                continue
            s = bytearray(p.ev[pl["state"]][1])
            if b"Arkham" not in s:
                continue
            for m in re.finditer(rb"\x01\x00\x00\x00\x14\x00\x00\x00", bytes(s)):
                i = m.end()
                vals = struct.unpack("<20d", bytes(s[i:i + 20 * 8]))
                if all(0 <= v <= 1.0000001 for v in vals):
                    for k, v in ch.items():
                        s[i + IDX[k] * 8:i + IDX[k] * 8 + 8] = struct.pack("<d", v)
                    break
            p.ev[pl["state"]] = (p.ev[pl["state"]][0], bytes(s))
    p.save(os.path.join(d, name + ".flp"))
    print("saved", name)

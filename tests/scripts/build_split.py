import os, sys
sys.path.insert(0, r"D:\class FLPjcts\сведение")
sys.path.insert(0, r"D:\class FLPjcts\сведение\_работа\telo")
from fltools.flp import Project
from fltools import edit
from clone import clone_insert

d = r"D:\class FLPjcts\сведение\_работа\test_222"
src = [f for f in os.listdir(d) if f.endswith(".flp") and not f.startswith("v_") and not f.startswith("split")][0]
mapping = {f: os.path.join(d, f) for f in os.listdir(d) if f.endswith(".wav")}
p = Project(os.path.join(d, src))
edit.set_sample_paths(p, mapping)
p.save(os.path.join(d, "v_orig.flp"))
srcp = Project(os.path.join(d, src))
moves = {13: 7, 14: 8, 6: 9, 7: 10}
for dst in sorted(set(moves.values())):
    clone_insert(p, srcp, 3, dst)
for ch, dst in moves.items():
    edit.set_channel_insert(p, ch, dst)
p.save(os.path.join(d, "v_split.flp"))
print("saved")

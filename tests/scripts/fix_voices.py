import re, struct, sys, os
sys.path.insert(0, r"D:\class FLPjcts\сведение")
from fltools.flp import Project
from fltools import edit

d = r"D:\class FLPjcts\сведение\_работа\test_split2"
p = Project(os.path.join(d, "v_split.flp"))
n = 0
for pl in edit.plugins(p):
    if pl["name"] != "ArkhamVocal":
        continue
    k = pl["state"]
    s = bytearray(p.ev[k][1])
    m = re.search(rb"\x01\x00\x00\x00\x15\x00\x00\x00", bytes(s))
    i = m.end() + 7 * 8
    old = struct.unpack("<d", bytes(s[i:i + 8]))[0]
    s[i:i + 8] = struct.pack("<d", old * 11 / 23.0)
    p.ev[k] = (p.ev[k][0], bytes(s))
    print("insert", pl["insert"], "voices %.0f -> norm %.3f" % (1 + old * 11, old * 11 / 23.0))
    n += 1
p.save(os.path.join(d, "v_split.flp"))
print("fixed", n)
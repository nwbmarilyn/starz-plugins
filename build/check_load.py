import ctypes, sys, struct

p = sys.argv[1]
print("python bits", struct.calcsize("P") * 8)
dll = ctypes.WinDLL(p)
print("loaded")
try:
    init = dll.InitDll
    init.restype = ctypes.c_bool
    print("InitDll", init())
except Exception as e:
    print("no InitDll", e)
gf = dll.GetPluginFactory
gf.restype = ctypes.c_void_p
fac = gf()
print("factory ptr", bool(fac))
vt = ctypes.cast(ctypes.cast(fac, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
class PFactoryInfo(ctypes.Structure):
    _fields_ = [("vendor", ctypes.c_char * 64), ("url", ctypes.c_char * 256), ("email", ctypes.c_char * 128), ("flags", ctypes.c_int32)]
class PClassInfo(ctypes.Structure):
    _fields_ = [("cid", ctypes.c_ubyte * 16), ("cardinality", ctypes.c_int32), ("category", ctypes.c_char * 32), ("name", ctypes.c_char * 64)]
getInfo = ctypes.WINFUNCTYPE(ctypes.c_int32, ctypes.c_void_p, ctypes.POINTER(PFactoryInfo))(vt[3])
countClasses = ctypes.WINFUNCTYPE(ctypes.c_int32, ctypes.c_void_p)(vt[4])
getClassInfo = ctypes.WINFUNCTYPE(ctypes.c_int32, ctypes.c_void_p, ctypes.c_int32, ctypes.POINTER(PClassInfo))(vt[5])
fi = PFactoryInfo()
print("getFactoryInfo", getInfo(fac, ctypes.byref(fi)), fi.vendor)
n = countClasses(fac)
print("classes", n)
for i in range(n):
    ci = PClassInfo()
    print("classInfo", getClassInfo(fac, i, ctypes.byref(ci)), ci.category, ci.name)

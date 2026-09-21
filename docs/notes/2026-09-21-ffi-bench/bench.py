# Times one frame of reads over the toy ABI in abi.c, five ways, for 200 and 1,700 units.
# Usage:  clang -O2 -shared -fPIC -o libabi.so abi.c && python3 bench.py
# Run from this directory. Results and their reading are in ../2026-09-21-backend-agnostic-bwapi.md.
import ctypes, time, sys
lib = ctypes.CDLL('./libabi.so'); lib.init()
lib.unit_get.restype = ctypes.c_int32; lib.unit_get.argtypes = [ctypes.c_int32, ctypes.c_int32]
lib.snapshot.restype = ctypes.c_int32; lib.snapshot.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32]
lib.map_base.restype = ctypes.c_void_p
class Unit(ctypes.Structure): _fields_ = [('f', ctypes.c_int32*60), ('d', ctypes.c_double*3)]
FIELDS = 20
def frame_calls(alive):
    s = 0
    get = lib.unit_get
    for i in range(alive):
        for k in range(FIELDS): s += get(i, k)
    return s
buf = (Unit*1700)()
def frame_snapshot(alive):
    lib.snapshot(ctypes.addressof(buf), 1700, alive)
    s = 0
    for i in range(alive):
        f = buf[i].f
        for k in range(FIELDS): s += f[k]
    return s
view = ctypes.cast(lib.map_base(), ctypes.POINTER(Unit*1700)).contents
def frame_inplace(alive):
    s = 0
    for i in range(alive):
        f = view[i].f
        for k in range(FIELDS): s += f[k]
    return s
def frame_snapshot_only(alive):
    lib.snapshot(ctypes.addressof(buf), 1700, alive)
try:
    import numpy as np
    dt = np.dtype([('f', np.int32, 60), ('d', np.float64, 3)])
    def frame_numpy(alive):
        lib.snapshot(ctypes.addressof(buf), 1700, alive)
        a = np.frombuffer(buf, dtype=dt, count=alive)
        return int(a['f'][:, :FIELDS].sum())
except ImportError:
    frame_numpy = None
def bench(fn, alive, reps=200):
    fn(alive); t=time.perf_counter()
    for _ in range(reps): fn(alive)
    return (time.perf_counter()-t)/reps*1e6
print(f"{'frame shape (Python '+sys.version.split()[0]+')':48s} {'200 units':>12s} {'1700 units':>12s}")
for name, fn in [("per-field ctypes calls (20 fields/unit)", frame_calls),
                 ("one snapshot call + ctypes field reads", frame_snapshot),
                 ("in-place mmap view + ctypes field reads", frame_inplace),
                 ("snapshot memcpy only, no reads", frame_snapshot_only),
                 ("snapshot + numpy vectorised reads", frame_numpy)]:
    if fn is None: print(f"{name:48s} {'(no numpy)':>12s}"); continue
    print(f"{name:48s} {bench(fn,200):9.0f} us {bench(fn,1700):9.0f} us")

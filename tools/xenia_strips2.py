"""Sesion 21: busca en la memoria fisica de Xenia (host 0x200000000 + fisica) bloques de >= 60 vertices de 32 bytes con posicion
float3 pequena (|x| < 0.3), como las tiras del pelo, y resume su tamano."""
import sys, ctypes, numpy as np
from ctypes import wintypes
sys.path.insert(0, sys.argv[2]); import xsearch
k32 = ctypes.windll.kernel32; hp = k32.OpenProcess(0x0410, False, int(sys.argv[1]))
m = xsearch.MBI(); a = 0x200000000; hi = 0x220000000; found = 0
while a < hi and k32.VirtualQueryEx(hp, ctypes.c_void_p(a), ctypes.byref(m), ctypes.sizeof(m)) and found < 60:
    base = m.BaseAddress or 0; size = m.RegionSize
    if m.State == 0x1000 and not (m.Protect & 0x101) and size >= 0x1000:
        buf = ctypes.create_string_buffer(size); got = ctypes.c_size_t(0)
        if k32.ReadProcessMemory(hp, ctypes.c_void_p(base), buf, size, ctypes.byref(got)):
            d = np.frombuffer(buf.raw[:got.value // 32 * 32], dtype=">f4").reshape(-1, 8)
            p = d[:, :3].astype(np.float64)
            ok = np.isfinite(p).all(1) & (np.abs(p) < 0.3).all(1) & (np.abs(p).max(1) > 1e-3)
            # el 4.o dword (normal empaquetada) suele ser "raro"; el 5.o-8.o son UV half: no se miran
            idx = np.flatnonzero(np.diff(np.concatenate(([0], ok.astype(np.int8), [0]))))
            for s, e in zip(idx[::2], idx[1::2]):
                if e - s >= 60:
                    q = p[s:e]; ph = base + s * 32 - 0x200000000
                    print("fisica %08X: %4d vertices, x %.3f..%.3f y %.3f..%.3f z %.3f..%.3f" % (ph, e - s, *[v for k in range(3) for v in (q[:, k].min(), q[:, k].max())]), flush=True)
                    found += 1
    a = base + size
print("bloques", found)

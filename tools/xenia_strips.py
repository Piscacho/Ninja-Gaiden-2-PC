"""Sesion 21: lee en Xenia la zona fisica 0x14560000-0x145E0000 (alias 0xB4560000) y resume los bloques que parecen tiras del pelo
(vertices de 32 bytes: posicion float3 pequena). Uso desde xenia_drive.py: XMEM="96:PY=tools/xenia_strips.py" """
import sys, struct, math
sys.path.insert(0, sys.argv[2]); import xsearch, ctypes
hp = ctypes.windll.kernel32.OpenProcess(0x0410, False, int(sys.argv[1]))
def rd(host, n):
    buf = ctypes.create_string_buffer(n); got = ctypes.c_size_t(0)
    ctypes.windll.kernel32.ReadProcessMemory(hp, ctypes.c_void_p(host), buf, n, ctypes.byref(got)); return buf.raw[:got.value]
for alias, host in ((0x14560000, 0x200000000 + 0x14560000), (0xB4560000, 0x100000000 + 0xB4560000), (0x14560000, 0x100000000 + 0x14560000)):
    b = rd(host, 0x80000)
    print("host %X: leidos %d" % (host, len(b)), flush=True)
    if len(b) < 0x80000: continue
    nz = sum(1 for i in range(0, len(b), 4096) if any(b[i:i+4096]))
    print("alias %08X: paginas con datos %d" % (alias, nz), flush=True)
    if not nz: continue
    for blk in range(0, len(b), 0x1000):
        best = None
        n = 0; mn = [1e9]*3; mx = [-1e9]*3
        for v in range(0, 0x1000 // 32):
            o = blk + v * 32
            p = struct.unpack(">3f", b[o:o+12])
            if all(math.isfinite(x) and abs(x) < 2 for x in p) and any(abs(x) > 1e-4 for x in p):
                n += 1
                for k in range(3): mn[k] = min(mn[k], p[k]); mx[k] = max(mx[k], p[k])
            else: break
        if n >= 20:
            print("  %08X: %3d vertices, x %.3f..%.3f y %.3f..%.3f z %.3f..%.3f" % (alias + blk, n, mn[0], mx[0], mn[1], mx[1], mn[2], mx[2]), flush=True)
    break

"""Sesion 21: para los mechones en 0x84B56580..., lee en Xenia los punteros +108/+112/+116 y su contenido (floats)."""
import sys, struct, ctypes
sys.path.insert(0, sys.argv[2]); import xsearch
hp = ctypes.windll.kernel32.OpenProcess(0x0410, False, int(sys.argv[1]))
for obj in (0x84B56580, 0x84B56790):
    b = xsearch.read(hp, obj, 0xC0); u = lambda o: struct.unpack(">I", b[o:o+4])[0]
    print("obj %08X cnt %d +4 %08X +100 %08X +108 %08X +112 %08X +116 %08X" % (obj, b[20], u(4), u(100), u(108), u(112), u(116)), flush=True)
    print("  obj+20..99:", b[20:100].hex().upper(), flush=True)
    for off in (108, 112, 116):
        p = u(off)
        if p:
            d = xsearch.read(hp, p, 96)
            print("  *(+%d) @%08X: %s" % (off, p, d.hex().upper()), flush=True)
            print("     floats:", [round(x, 5) for x in struct.unpack(">24f", d)], flush=True)

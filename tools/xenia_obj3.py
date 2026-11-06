"""Sesion 21: busca los mechones en Xenia y lee *(obj+112) con desplazamientos de -0x1000, 0 y +0x1000; ademas busca nuestro patron
de corte en la zona 0xE0000000-0xFFFFFFFF."""
import sys, struct, ctypes, traceback
try:
    sys.path.insert(0, sys.argv[2]); import xsearch
    pid = int(sys.argv[1])
    hp, hits = xsearch.search(pid, bytes.fromhex("3D4CCCCD3B03126F3F8000003F00000000000000000000000000000000000000000000003F800000"))
    print("mechones", ["%08X" % (h - 24) for h in hits[:4]], flush=True)
    if hits:
        obj = hits[0] - 24; b = xsearch.read(hp, obj, 0xC0); u = lambda o: struct.unpack(">I", b[o:o+4])[0]
        p = u(112); print("obj %08X +112 %08X" % (obj, p), flush=True)
        for d in (-0x1000, 0, 0x1000):
            x = xsearch.read(hp, p + d, 48)
            print("  %+05X: %s" % (d, x.hex().upper()), flush=True)
    hp, h2 = xsearch.search(pid, bytes.fromhex("3B13B7D8000000003B9EF523"), 0x100000000 + 0xE0000000, 0x200000000)
    print("patron de corte en 0xE...:", ["%08X" % h for h in h2[:8]], flush=True)
    hp, h3 = xsearch.search(pid, bytes.fromhex("3B13B7D8000000003B9EF523"), 0x200000000, 0x220000000)
    print("patron de corte en fisica:", ["%08X" % (h + 0x100000000 - 0x200000000) for h in h3[:8]], flush=True)
except Exception:
    traceback.print_exc()

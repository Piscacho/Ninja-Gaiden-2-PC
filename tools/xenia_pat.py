"""Sesion 21: busca en Xenia (memoria virtual 0x100000000 y fisica 0x200000000) el patron de los desplazamientos del corte de las
tiras de nuestra version y lo vuelca."""
import sys, struct, ctypes
sys.path.insert(0, sys.argv[2]); import xsearch
pid = int(sys.argv[1])
pat = bytes.fromhex("3B13B7D8000000003B9EF523")
for lo, hi, base in ((0x100000000, 0x200000000, 0x100000000), (0x200000000, 0x220000000, 0x200000000)):
    hp, hits = xsearch.search(pid, pat, lo, hi)
    print("rango %X: %d" % (lo, len(hits)), ["%08X" % (h + 0x100000000 - base) for h in hits[:10]], flush=True)
    for h in hits[:2]:
        d = xsearch.read(hp, h + 0x100000000 - base + (base - 0x100000000), 96) if False else None

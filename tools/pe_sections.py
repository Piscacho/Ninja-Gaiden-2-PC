"""Secciones PE de basefile.bin (imagen mapeada, offset = VA - base).
Uso: python -I pe_sections.py basefile.bin"""
import struct, sys

def sections(img):
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    assert img[pe:pe + 4] == b"PE\0\0"
    machine, nsec = struct.unpack_from("<HH", img, pe + 4)
    optsz = struct.unpack_from("<H", img, pe + 20)[0]
    opt = pe + 24
    base = struct.unpack_from("<I", img, opt + 28)[0]
    # data directories: exception (.pdata) = indice 3
    ndd = struct.unpack_from("<I", img, opt + 92)[0]
    dd = [struct.unpack_from("<II", img, opt + 96 + i * 8) for i in range(ndd)]
    secs = []
    so = opt + optsz
    for i in range(nsec):
        name = img[so:so + 8].rstrip(b"\0").decode("latin-1")
        vsz, va, rsz, roff = struct.unpack_from("<IIII", img, so + 8)
        ch = struct.unpack_from("<I", img, so + 36)[0]
        secs.append((name, base + va, vsz, ch))
        so += 40
    return machine, base, secs, dd

if __name__ == "__main__":
    img = open(sys.argv[1], "rb").read()
    m, base, secs, dd = sections(img)
    print("machine=0x%X base=0x%08X" % (m, base))
    for n, va, sz, ch in secs:
        kind = "code" if ch & 0x20000000 else ("RW" if ch & 0x80000000 else "RO")
        print("%-10s 0x%08X 0x%08X %s" % (n, va, sz, kind))
    names = ["export", "import", "resource", "exception", "security", "reloc", "debug", "arch", "gp", "tls", "loadcfg"]
    for i, (rva, sz) in enumerate(dd):
        if sz: print("dd[%d] %-9s 0x%08X 0x%X" % (i, names[i] if i < len(names) else "?", base + rva, sz))

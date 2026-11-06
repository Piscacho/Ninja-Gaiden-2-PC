import sys, struct
sys.path.insert(0, sys.argv[2]); import xsearch
pid = int(sys.argv[1])
pat = bytes.fromhex("3D4CCCCD3B03126F3F8000003F00000000000000000000000000000000000000000000003F800000")
hp, hits = xsearch.search(pid, pat)
print("candidatos", len(hits), ["%08X" % (h - 24) for h in hits[:16]], flush=True)
for h in hits[:3]:
    obj = h - 24; b = xsearch.read(hp, obj, 0xC0)
    u = lambda o: struct.unpack(">I", b[o:o+4])[0]; f = lambda o: struct.unpack(">f", b[o:o+4])[0]
    cnt = b[20]; pts = u(100)
    print("obj %08X cnt %d +4 %08X +100 %08X +108 %08X +112 %08X +116 %08X +120 %08X +124 %08X +12 %08X | +48 %s | +176 %s" % (obj, cnt, u(4), pts, u(108), u(112), u(116), u(120), u(124), u(12),
          [round(f(48 + 4*i), 3) for i in range(4)], [round(f(176 + 4*i), 3) for i in range(4)]))
    for off in (112, 116):
        q = u(off)
        if q:
            d = xsearch.read(hp, q, 64)
            print("   *(+%d) @%08X floats: %s" % (off, q, [round(x, 5) for x in struct.unpack(">16f", d)]), flush=True)
    if 0x40000000 < pts < 0xC0000000 and cnt < 20:
        pb = xsearch.read(hp, pts, 64 * cnt)
        for i in range(min(cnt, 8)):
            v = [round(struct.unpack(">f", pb[i*64+4*k:i*64+4*k+4])[0], 3) for k in range(8)]
            print("   p%d %s" % (i, v))

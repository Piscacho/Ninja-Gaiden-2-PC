"""Destinos de lis/addi (punteros a funcion construidos en codigo) sin funcion generada y con pinta de inicio de funcion.
python -I lisaddi_functions.py basefile.bin ppc/ppc_func_mapping.cpp > salida.txt"""
import struct, re, sys, bisect
img = open(sys.argv[1], "rb").read(); B = 0x82000000
w = lambda a: struct.unpack_from(">I", img, a - B)[0]
gen = sorted(int(x, 16) for x in re.findall(r"\{ 0x(\w+), \w+ \}", open(sys.argv[2]).read())); gs = set(gen)
C0, C1 = 0x822F0000, 0x83A70000
def is_end(x): return x in (0x4E800020, 0) or (x >> 26 == 18 and x & 3 == 0) or (x >> 26 == 19 and (x >> 1) & 0x3FF == 16 and x & 1 == 0 and ((x >> 21) & 0x14) == 0x14)
hits = {}
labels = set(int(v, 16) for v in re.findall(r"^\s+0x(\w+),", open(sys.argv[3]).read(), re.M)) if len(sys.argv) > 3 else set()
for a in range(C0, 0x83A5FFF0, 4):
    x = w(a)
    if x >> 26 != 15 or (x >> 16) & 31: continue   # lis rT,hi (addis rT,0,hi)
    rt = (x >> 21) & 31; hi = x & 0xFFFF
    for k in range(1, 12):
        y = w(a + 4 * k)
        if y >> 26 == 14 and (y >> 16) & 31 == rt:
            lo = y & 0xFFFF; lo -= 0x10000 if lo & 0x8000 else 0
            t = ((hi << 16) + lo) & 0xFFFFFFFF
            if C0 <= t < C1 and t & 3 == 0 and t not in gs and w(t) != 0 and is_end(w(t - 4)) and t not in labels:
                hits.setdefault(t, a)
            break
        if (y >> 21) & 31 == rt and y >> 26 not in (36, 37, 38, 44, 54, 55, 62): break   # rT reescrito
for t in sorted(hits):
    i = bisect.bisect_right(gen, t) - 1
    print("0x%08X ref=0x%08X dentro_de=0x%08X" % (t, hits[t], gen[i]))
sys.stderr.write("candidatos: %d\n" % len(hits))

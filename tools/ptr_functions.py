"""Punteros a codigo en .rdata/.data que parecen inicios de funcion pero no tienen funcion generada.
python -I ptr_functions.py basefile.bin ppc/ppc_func_mapping.cpp > salida.txt"""
import struct, re, sys, bisect
img = open(sys.argv[1], "rb").read(); B = 0x82000000
w = lambda a: struct.unpack_from(">I", img, a - B)[0]
gen = sorted(int(x, 16) for x in re.findall(r"\{ 0x(\w+), \w+ \}", open(sys.argv[2]).read()))
gs = set(gen)
C0, C1 = 0x822F0000, 0x83A70000
def is_end(x):  # blr, b (no bl), relleno, bctr
    return x in (0x4E800020, 0, 0x4E800420) or (x >> 26 == 18 and x & 3 == 0)
hits = {}
for s0, s1 in ((0x82000600, 0x82291A00), (0x83A70000, 0x859E1E00)):
    for a in range(s0, s1, 4):
        t = w(a)
        if C0 <= t < C1 and t & 3 == 0 and t not in gs and is_end(w(t - 4)) and w(t) != 0:
            hits.setdefault(t, []).append(a)
for t in sorted(hits):
    i = bisect.bisect_right(gen, t) - 1
    print("0x%08X refs=%d dentro_de=0x%08X ej_ref=0x%08X" % (t, len(hits[t]), gen[i], hits[t][0]))
sys.stderr.write("candidatos: %d\n" % len(hits))

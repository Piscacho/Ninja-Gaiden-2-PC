"""Sesion 24: busca en basefile.bin el codigo que construye una direccion (lis rX,hi + addi/ori/lwz rY,lo(rX) cerca).
Uso: python tools/xref.py DIRECCION_HEX [DIRECCION_HEX ...]"""
import sys, numpy as np
b = open("xex/basefile.bin", "rb").read(); B = 0x82000000
w = np.frombuffer(b[: len(b) // 4 * 4], dtype=">u4").astype(np.uint32)
op = w >> 26; rD = (w >> 21) & 31; rA = (w >> 16) & 31; imm = (w & 0xFFFF).astype(np.int64); simm = np.where(imm >= 0x8000, imm - 0x10000, imm)
for a in [int(x, 16) for x in sys.argv[1:]]:
    lo = a & 0xFFFF; slo = lo - 0x10000 if lo >= 0x8000 else lo; hi = ((a - slo) >> 16) & 0xFFFF
    lis = np.flatnonzero((op == 15) & (rA == 0) & (imm == hi))
    found = []
    for i in lis:
        r = rD[i]
        for j in range(i + 1, min(i + 12, len(w))):
            if op[j] in (14, 32, 34, 36, 38, 40, 44, 48, 52) and rA[j] == r and simm[j] == slo: found.append(B + 4 * j); break
    print("%08X: %d referencias" % (a, len(found)), " ".join("%08X" % f for f in found[:12]))

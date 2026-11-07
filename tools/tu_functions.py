"""Sesion 26: traslada la lista functions = [...] de config/ng2.toml (funciones sin .pdata: huecos con switch y manuales) a la TU.
Cada funcion se busca por su contenido completo normalizado (tools/tu_map.norm) cerca de la posicion prevista (ancla anterior
de tu/funcmap.txt). Escribe config/ng2_tu_functions.txt (address/size) y lista las que no aparecen.
Uso: python -I tools/tu_functions.py"""
import re, os, struct, bisect, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(root, "tools"))
import tu_map as T
import numpy as np
B = 0x82000000

def main():
    bd = open(os.path.join(root, "xex", "basefile.bin"), "rb").read(); td = open(os.path.join(root, "xex", "tu_basefile.bin"), "rb").read()
    fm = sorted((int(a, 16), int(b, 16)) for a, b, s, h in (l.split() for l in open(os.path.join(root, "tu", "funcmap.txt"))))
    ak = [a for a, _ in fm]
    funcs = [(int(a, 16), int(s, 16)) for a, s in re.findall(r"address = 0x(\w+), size = 0x(\w+)", open(os.path.join(root, "config", "ng2.toml")).read())]
    tw = np.frombuffer(td[: len(td) // 4 * 4], dtype=">u4").astype(np.uint32)
    def nrm(arr):
        op = arr >> 26; out = arr.copy()
        out = np.where(op == 18, arr & 0xFC000003, out); out = np.where(op == 16, arr & 0xFFFF0003, out)
        return np.where(np.isin(op, list(T.MASK_IMM)), arr & 0xFFFF0000, out)
    tn = nrm(tw)
    res, miss = [], []
    for a, s in funcs:
        bw = nrm(np.frombuffer(bd[a - B:a - B + s], dtype=">u4").astype(np.uint32))
        k = bisect.bisect_right(ak, a) - 1; pred = fm[k][1] + (a - fm[k][0]) if k >= 0 else a
        L = min(len(bw), 64)
        cand = np.flatnonzero(tn[: len(tn) - L] == bw[0])
        hits = [int(c) for c in cand if np.array_equal(tn[c:c + L], bw[:L]) and (len(bw) <= 64 or np.array_equal(tn[c:c + len(bw)], bw))]
        if not hits: miss.append((a, s)); continue
        t = min(hits, key=lambda c: abs(B + 4 * c - pred))
        res.append((B + 4 * t, s, a, len(hits)))
    with open(os.path.join(root, "config", "ng2_tu_functions.txt"), "w") as f:
        for t, s, a, n in sorted(res): f.write("{ address = 0x%08X, size = 0x%X }  # original 0x%08X (%d)\n" % (t, s, a, n))
    print("trasladadas %d de %d; sin encontrar %d" % (len(res), len(funcs), len(miss)))
    for a, s in miss: print("  sin encontrar 0x%08X 0x%X" % (a, s))
    amb = [r for r in res if r[3] > 1]
    print("ambiguas (se eligio la mas cercana): %d" % len(amb))

if __name__ == "__main__":
    main()

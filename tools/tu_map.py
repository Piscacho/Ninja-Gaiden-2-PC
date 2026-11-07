"""Sesion 26: mapa de funciones y datos del ejecutable original (xex/basefile.bin) al de la actualizacion (xex/tu_basefile.bin).
Firma de una funcion (.pdata) = sus instrucciones con los campos que cambian al reenlazar enmascarados (destinos de b/bl/bc,
inmediatos de lis/addi/addis/ori/oris y desplazamientos de cargas y almacenamientos). Se emparejan primero las firmas unicas en las
dos versiones; luego, entre anclas, las que quedan por orden si tienen el mismo tamano. Las direcciones de datos se sacan de los
pares lis+addi/lwz/... de las funciones emparejadas (misma instruccion en las dos versiones -> direccion vieja -> nueva).
Uso: python -I tools/tu_map.py  ->  tu/funcmap.txt (vieja nueva tamano metodo), tu/datamap.txt (vieja nueva votos)"""
import struct, sys, os, collections
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(root, "tools"))
B = 0x82000000

def load(path):
    d = open(path, "rb").read()
    e = struct.unpack_from("<I", d, 0x3C)[0]; nsec = struct.unpack_from("<H", d, e + 6)[0]; opt = struct.unpack_from("<H", d, e + 20)[0]
    secs = {}
    for i in range(nsec):
        o = e + 24 + opt + i * 40; nm = d[o:o + 8].rstrip(b"\0").decode(); vs, va = struct.unpack_from("<II", d, o + 8); secs[nm] = (va, vs)
    va, vs = secs[".pdata"]; funcs = []
    for i in range(vs // 8):
        beg, info = struct.unpack_from(">II", d, va + i * 8)
        if beg: funcs.append((beg, ((info >> 8) & 0x3FFFFF) * 4))
    funcs.sort()
    return d, secs, funcs

MASK_IMM = {14, 15, 24, 25} | set(range(32, 56)) | {58, 62}   # addi addis ori oris, cargas/almacenamientos D/DS
def norm(w):
    op = w >> 26
    if op == 18: return w & 0xFC000003          # b/bl
    if op == 16: return w & 0xFFFF0003          # bc: conserva BO/BI
    if op in MASK_IMM: return w & 0xFFFF0000
    return w

def sig(d, a, size):
    ws = struct.unpack_from(">%dI" % (size // 4), d, a - B)
    return hash(tuple(norm(w) for w in ws)), ws

def main():
    bd, bs, bf = load(os.path.join(root, "xex", "basefile.bin"))
    td, ts, tf = load(os.path.join(root, "xex", "tu_basefile.bin"))
    bsig = {a: sig(bd, a, s)[0] for a, s in bf}; tsig = {a: sig(td, a, s)[0] for a, s in tf}
    cb = collections.Counter(bsig.values()); ct = collections.Counter(tsig.values())
    tby = {}
    for a, h in tsig.items(): tby.setdefault(h, []).append(a)
    m = {}; how = {}
    for a, h in bsig.items():
        if cb[h] == 1 and ct.get(h) == 1: m[a] = tby[h][0]; how[a] = "firma"
    # entre anclas consecutivas (en orden creciente en las dos versiones): emparejar por orden si los huecos coinciden
    bsize = dict(bf); tsize = dict(tf)
    bl = [a for a, _ in bf]; tl = [a for a, _ in tf]; tidx = {a: i for i, a in enumerate(tl)}
    anchors = sorted((a, m[a]) for a in m)
    mono = []; last = -1
    for a, t in anchors:   # quitar anclas que rompen el orden
        if tidx[t] > last: mono.append((a, t)); last = tidx[t]
    bidx = {a: i for i, a in enumerate(bl)}
    for (a0, t0), (a1, t1) in zip(mono, mono[1:]):
        gb = bl[bidx[a0] + 1:bidx[a1]]; gt = tl[tidx[t0] + 1:tidx[t1]]
        if gb and len(gb) == len(gt):
            for x, y in zip(gb, gt):
                if x not in m and (bsize[x] == tsize[y] or abs(bsize[x] - tsize[y]) <= max(16, bsize[x] // 10)): m[x] = y; how[x] = "orden"
    # aproximado: para las que quedan, posicion prevista segun el ancla anterior y la funcion sin pareja mas parecida cerca
    import bisect, difflib
    used = set(m.values()); manc = sorted(m.items())
    ak = [a for a, _ in manc]
    tfree = sorted(set(tl) - used)
    for a in bl:
        if a in m: continue
        k = bisect.bisect_right(ak, a) - 1
        if k < 0: continue
        pred = manc[k][1] + (a - manc[k][0])
        lo = bisect.bisect_left(tfree, pred - 0x6000); hi = bisect.bisect_right(tfree, pred + 0x6000)
        nb = [norm(w) for w in struct.unpack_from(">%dI" % (bsize[a] // 4), bd, a - B)]
        bestr, bt = 0.0, None
        for t in tfree[lo:hi]:
            if t in used or abs(tsize[t] - bsize[a]) > max(64, bsize[a] // 3): continue
            nt = [norm(w) for w in struct.unpack_from(">%dI" % (tsize[t] // 4), td, t - B)]
            r = difflib.SequenceMatcher(None, nb, nt, autojunk=False).ratio() if len(nb) < 3000 else (1.0 if nb == nt else 0.0)
            r -= abs(t - pred) / 0x60000   # desempate: mas cerca de lo previsto
            if r > bestr: bestr, bt = r, t
        if bt is not None and bestr >= 0.8: m[a] = bt; how[a] = "aprox%.2f" % bestr; used.add(bt)
    # direcciones de datos: lis rX,hi + (addi|lwz|lfs|stw...) rY, lo(rX) en la misma posicion de funciones emparejadas
    votes = collections.Counter()
    for a, t in m.items():
        s = bsize[a]
        if tsize[t] != s: continue
        wb = struct.unpack_from(">%dI" % (s // 4), bd, a - B); wt = struct.unpack_from(">%dI" % (s // 4), td, t - B)
        for i in range(len(wb)):
            if wb[i] >> 26 != 15 or (wb[i] >> 16) & 31 != 0 or wt[i] >> 26 != 15: continue
            r = (wb[i] >> 21) & 31
            for j in range(i + 1, min(i + 10, len(wb))):
                o = wb[j] >> 26
                if o in (14, 24, 32, 34, 36, 38, 40, 44, 48, 50, 52, 54) and (wb[j] >> 16) & 31 == r and (wt[j] >> 16) & 31 == r:
                    def addr(hi, lo, o): lo = lo - 0x10000 if (lo >= 0x8000 and o != 24) else lo; return ((hi << 16) + lo) & 0xFFFFFFFF
                    va = addr(wb[i] & 0xFFFF, wb[j] & 0xFFFF, o); vt = addr(wt[i] & 0xFFFF, wt[j] & 0xFFFF, o)
                    if 0x82000000 <= va < 0x86000000: votes[(va, vt)] += 1
                    break
    # destinos de llamada: bl X (original) <-> bl Y (TU) en la misma posicion de funciones emparejadas del mismo tamano
    cvotes = collections.Counter()
    for a, t in m.items():
        s = bsize[a]
        if tsize[t] != s: continue
        wb = struct.unpack_from(">%dI" % (s // 4), bd, a - B); wt = struct.unpack_from(">%dI" % (s // 4), td, t - B)
        for i in range(len(wb)):
            if wb[i] >> 26 == 18 and wt[i] >> 26 == 18 and (wb[i] & 3) == 1 and (wt[i] & 3) == 1:
                def tgt(w, pc): li = w & 0x03FFFFFC; li = li - 0x04000000 if li & 0x02000000 else li; return (pc + li) & 0xFFFFFFFF
                cvotes[(tgt(wb[i], a + 4 * i), tgt(wt[i], t + 4 * i))] += 1
    cbest = {}
    for (x, y), n in cvotes.items():
        if x not in cbest or n > cbest[x][1]: cbest[x] = (y, n)
    with open(os.path.join(root, "tu", "callmap.txt"), "w") as f:
        for x in sorted(cbest): f.write("%08X %08X %d\n" % (x, cbest[x][0], cbest[x][1]))
    print("destinos de llamada: %d" % len(cbest))
    best = {}
    for (va, vt), n in votes.items():
        if va not in best or n > best[va][1]: best[va] = (vt, n)
    with open(os.path.join(root, "tu", "funcmap.txt"), "w") as f:
        for a in sorted(m): f.write("%08X %08X %X %s\n" % (a, m[a], bsize[a], how[a]))
    with open(os.path.join(root, "tu", "datamap.txt"), "w") as f:
        for va in sorted(best): f.write("%08X %08X %d\n" % (va, best[va][0], best[va][1]))
    nf = sum(1 for v in how.values() if v == "firma"); no = sum(1 for v in how.values() if v == "orden"); na = len(m) - nf - no
    print("funciones: original %d, TU %d, emparejadas %d (%d por firma, %d por orden, %d aproximadas), sin pareja %d" % (len(bf), len(tf), len(m), nf, no, na, len(bf) - len(m)))
    print("direcciones de datos: %d" % len(best))

if __name__ == "__main__":
    main()

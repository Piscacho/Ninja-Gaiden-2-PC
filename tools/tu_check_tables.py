"""Sesion 27: comprueba las tablas de saltos de config/ng2_tu_switch_tables_fixed.toml contra los datos reales de la TU
(xex/tu_basefile.bin). Las tablas trasladadas desde el original con tu_switches.py pueden estar mal cuando la actualizacion cambio
la funcion (p. ej. sub_83656870: 16 de 153 entradas mal -> el juego se quedaba en un bucle a mitad del capitulo 1).
Tipos reconocidos tras el lis/addi de la base (como jt_finder.py):
  ABS : rlwinm rI,rIdx,2 ; lwzx            -> u32[i]
  SHRT: rlwinm rI,rIdx,1 ; lhzx ; lis/addi -> base2 + u16[i]
  BYTE: lbzx [; rlwinm s] ; lis/addi       -> base2 + (u8[i] << s)
Uso: python -I tools/tu_check_tables.py [--fix]   (--fix reescribe las etiquetas con las reales)"""
import re, struct, os, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
td = open(os.path.join(root, "xex", "tu_basefile.bin"), "rb").read(); B = 0x82000000
w = lambda a: struct.unpack_from(">I", td, a - B)[0]
path = os.path.join(root, "config", "ng2_tu_switch_tables_fixed.toml")
src = open(path).read()
blocks = re.split(r"(?=\[\[switch\]\])", src)

def s16(v): return v - 0x10000 if v & 0x8000 else v

def decode(base, n):
    """Lee n entradas reales a partir de las instrucciones que siguen a 'base' (lis rT / addi rT). None si no se reconoce."""
    i0, i1 = w(base), w(base + 4)
    if i0 >> 26 != 15 or i1 >> 26 != 14: return None
    tab = (((i0 & 0xFFFF) << 16) + s16(i1 & 0xFFFF)) & 0xFFFFFFFF
    seq = [w(base + 8 + 4 * k) for k in range(12)]
    ops = [x >> 26 for x in seq]
    # ABS: rlwinm (21) .. lwzx (31, xo 23)
    xo = lambda x: (x >> 1) & 0x3FF
    kinds = [(x >> 26, xo(x)) for x in seq]
    if any(k == (31, 23) for k in kinds[:4]):
        return [w(tab + 4 * i) for i in range(n)]
    if any(k == (31, 279) for k in kinds[:4]):   # lhzx
        for k in range(len(seq) - 1):
            if seq[k] >> 26 == 15 and seq[k + 1] >> 26 == 14:
                b2 = (((seq[k] & 0xFFFF) << 16) + s16(seq[k + 1] & 0xFFFF)) & 0xFFFFFFFF
                return [(b2 + struct.unpack_from(">H", td, tab + 2 * i - B)[0]) & 0xFFFFFFFF for i in range(n)]
        return None
    if any(k == (31, 87) for k in kinds[:4]):    # lbzx
        sh = 0
        for k in range(len(seq) - 1):
            if seq[k] >> 26 == 21 and sh == 0: sh = (seq[k] >> 11) & 31
            if seq[k] >> 26 == 15 and seq[k + 1] >> 26 == 14:
                b2 = (((seq[k] & 0xFFFF) << 16) + s16(seq[k + 1] & 0xFFFF)) & 0xFFFFFFFF
                return [(b2 + (td[tab + i - B] << sh)) & 0xFFFFFFFF for i in range(n)]
        return None
    return None

fix = "--fix" in sys.argv
out = []; bad = 0; unk = 0; ok = 0
for b in blocks:
    m = re.search(r"base = 0x(\w+)", b)
    if not m: out.append(b); continue
    base = int(m.group(1), 16)
    labs = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{8})", b.split("labels")[1])]
    real = decode(base, len(labs))
    if real is None: unk += 1; out.append(b); continue
    if real == labs: ok += 1; out.append(b); continue
    bad += 1
    d = [i for i in range(len(labs)) if labs[i] != real[i]]
    print("tabla %08X: %d de %d entradas distintas (p. ej. [%d] %08X -> real %08X)" % (base, len(d), len(labs), d[0], labs[d[0]], real[d[0]]))
    if fix:   # solo las entradas reales plausibles (codigo cercano); el resto se deja (p. ej. una entrada de mas al final)
        good = [real[i] if (real[i] & 3) == 0 and abs(real[i] - labs[i]) < 0x10000 else labs[i] for i in range(len(labs))]
        head = b.split("labels")[0]
        out.append(head + "labels = [\n" + "".join("    0x%08X,\n" % v for v in good) + "]\n\n")
    else: out.append(b)
print("bien %d, mal %d, sin reconocer %d" % (ok, bad, unk))
if fix and bad: open(path, "w").write("".join(out)); print("reescrito", path)

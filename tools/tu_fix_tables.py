"""Sesion 26: tablas de la TU recortadas porque una entrada salta al trozo siguiente (otra funcion de .pdata, salto de cola).
Para cada tabla absoluta lis/addi de config/ng2_tu_switch_tables_fixed.toml se leen las entradas reales de la TU y, si el
original tenia mas casos en la tabla trasladada equivalente o la lectura real continua con destinos cercanos que son inicio de
funcion de .pdata, se amplia. Uso: python -I tools/tu_fix_tables.py"""
import re, struct, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
td = open(os.path.join(root, "xex", "tu_basefile.bin"), "rb").read(); B = 0x82000000
pd = set(int(l.split()[0], 16) for l in open(os.path.join(root, "xex", "tu_functions_pdata.txt")))
path = os.path.join(root, "config", "ng2_tu_switch_tables_fixed.toml")
blocks = re.split(r"(?=\[\[switch\]\])", open(path).read()); out = []; fixed = []
for b in blocks:
    m = re.search(r"base = 0x(\w+)", b)
    if not m: out.append(b); continue
    base = int(m.group(1), 16); labs = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+),", b.split("labels")[1])]
    w0, w1 = struct.unpack_from(">II", td, base - B)
    if not (w0 >> 26 == 15 and w1 >> 26 == 14): out.append(b); continue
    lo = w1 & 0xFFFF; lo = lo - 0x10000 if lo >= 0x8000 else lo
    tab = (((w0 & 0xFFFF) << 16) + lo) & 0xFFFFFFFF
    if tab - B + 4 * len(labs) + 4 > len(td): out.append(b); continue
    ent = list(struct.unpack_from(">%dI" % len(labs), td, tab - B))
    if ent != labs: out.append(b); continue
    lo_, hi_ = min(labs), max(labs); extra = []
    a = tab + 4 * len(labs)
    while True:
        v = struct.unpack_from(">I", td, a - B)[0]
        if v & 3 or not (lo_ - 0x4000 <= v <= hi_ + 0x4000): break
        if a in labs or a == base: break
        extra.append(v); a += 4
    # solo si alguna de las nuevas es inicio de funcion de .pdata (el trozo de cola) y las demas caen en el rango de la tabla
    if extra and any(v in pd for v in extra) and all(lo_ - 0x4000 <= v <= hi_ + 0x4000 for v in extra):
        labs += extra; fixed.append((base, len(labs) - len(extra), len(labs)))
        b = re.sub(r"labels = \[.*?\]", "labels = [\n" + "".join("    0x%08X,\n" % l for l in labs) + "]", b, flags=re.S)
    out.append(b)
open(path, "w").write("".join(out))
for f in fixed: print("0x%08X: %d -> %d entradas" % f)
print("tablas ampliadas:", len(fixed))

"""Sesion 26: tablas de saltos de la actualizacion. Las del original (config/ng2_switch_tables_all.toml, ya corregidas a mano en
las sesiones 2, 5 y 15) se trasladan a la TU si su funcion tiene pareja del mismo tamano y el bloque de codigo coincide
(base y etiquetas + desplazamiento); el resto se toma de XenonAnalyse sobre la TU (config/ng2_tu_switch_tables.toml).
Uso: python -I tools/tu_switches.py -> config/ng2_tu_switch_tables_all.toml"""
import re, os, struct, bisect, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(root, "tools"))
import tu_map as T
B = 0x82000000

def parse(path):
    out = []
    for blk in re.split(r"(?=\[\[switch\]\])", open(path).read()):
        m = re.search(r"base = 0x(\w+)", blk)
        if not m: continue
        r = int(re.search(r"\nr = (\d+)", blk).group(1)); d = int(re.search(r"default = 0x(\w+)", blk).group(1), 16)
        labels = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]+),", blk.split("labels")[1])]
        out.append({"base": int(m.group(1), 16), "r": r, "default": d, "labels": labels})
    return out

def write(path, sw, head):
    with open(path, "w") as f:
        f.write(head + "\n")
        for s in sorted(sw, key=lambda s: s["base"]):
            f.write("[[switch]]\nbase = 0x%08X\nr = %d\ndefault = 0x%X\nlabels = [\n" % (s["base"], s["r"], s["default"]))
            for l in s["labels"]: f.write("    0x%08X,\n" % l)
            f.write("]\n\n")

def main():
    fm = {}
    for l in open(os.path.join(root, "tu", "funcmap.txt")):
        a, b, sz, h = l.split(); fm[int(a, 16)] = (int(b, 16), int(sz, 16))
    bd = open(os.path.join(root, "xex", "basefile.bin"), "rb").read(); td = open(os.path.join(root, "xex", "tu_basefile.bin"), "rb").read()
    _, _, bf = T.load(os.path.join(root, "xex", "basefile.bin"))[0:3]
    _, _, tf = T.load(os.path.join(root, "xex", "tu_basefile.bin"))[0:3]
    tsize = dict(tf); bstarts = [a for a, _ in bf]; bsz = dict(bf)
    # funciones manuales del TOML original (sin .pdata) tambien sirven de contenedor si tienen pareja por llamadas
    cm = {}
    for l in open(os.path.join(root, "tu", "callmap.txt")):
        x, y, n = l.split(); cm[int(x, 16)] = int(y, 16)
    base_sw = parse(os.path.join(root, "config", "ng2_switch_tables_all.toml"))
    tu_sw = {s["base"]: s for s in parse(os.path.join(root, "config", "ng2_tu_switch_tables.toml"))}
    out = {}; moved = 0; lost = []; leidas = [0]
    mk = sorted(fm)
    def pat(d, a, before, after):
        o = a - B - 4 * before
        if o < 0 or o + 4 * (before + after) > len(d): return None
        return [T.norm(w) for w in struct.unpack_from(">%dI" % (before + after), d, o)]
    for s in base_sw:
        k = bisect.bisect_right(bstarts, s["base"]) - 1
        f0 = bstarts[k] if k >= 0 else None
        delta = None
        if f0 is not None and s["base"] < f0 + bsz[f0] and f0 in fm and tsize.get(fm[f0][0]) == bsz[f0]: delta = fm[f0][0] - f0
        if delta is None:
            # buscar el mismo bloque (bctr y alrededores) cerca de la posicion prevista por el TOML de la TU
            cands = [b for b in tu_sw if abs(b - s["base"]) < 0x40000 and len(tu_sw[b]["labels"]) == len(s["labels"])
                     and all(tu_sw[b]["labels"][i] - b == s["labels"][i] - s["base"] for i in range(len(s["labels"])))]
            if len(cands) == 1: delta = cands[0] - s["base"]
        if delta is None and f0 is not None and f0 in fm: delta = fm[f0][0] - f0   # prevision por la funcion (tamano distinto)
        if delta is None:
            k2 = bisect.bisect_left(mk, s["base"]) - 1
            if k2 >= 0: delta = fm[mk[k2]][0] - mk[k2]
        if delta is None: lost.append(s); continue
        nb = pat(bd, s["base"], 0, 6)
        found = None
        if pat(td, s["base"] + delta, 0, 6) == nb: found = s["base"] + delta
        else:
            for dd in range(4, 0x4000, 4):
                for sg in (dd, -dd):
                    if pat(td, s["base"] + delta + sg, 0, 6) == nb: found = s["base"] + delta + sg; break
                if found: break
        if not found: lost.append(s); continue
        d0 = found - s["base"]; labs = []
        for l in s["labels"]:
            pl = pat(bd, l, 0, 6); hit = None
            for dd in [0] + [x for y in range(4, 0x200, 4) for x in (y, -y)]:
                if pat(td, l + d0 + dd, 0, 6) == pl: hit = l + d0 + dd; break
            if hit is None: break
            labs.append(hit)
        if len(labs) != len(s["labels"]):
            # tercera via: la tabla esta en la TU (mismo bloque lis/addi/.../bctr): leer sus entradas de la propia TU
            w0, w1 = struct.unpack_from(">II", td, found - B)
            if w0 >> 26 == 15 and w1 >> 26 == 14:
                lo = w1 & 0xFFFF; lo = lo - 0x10000 if lo >= 0x8000 else lo
                tab = (((w0 & 0xFFFF) << 16) + lo) & 0xFFFFFFFF
                ent = list(struct.unpack_from(">%dI" % len(s["labels"]), td, tab - B))
                if all(abs(e - found) < 0x8000 and e & 3 == 0 for e in ent):
                    out[found] = {"base": found, "r": s["r"], "default": (s["default"] + d0) if s["default"] else 0, "labels": ent}
                    moved += 1; leidas[0] += 1; continue
            lost.append(s); continue
        # aceptar solo si XenonAnalyse ve tambien un switch ahi, o si todas las etiquetas quedan cerca (no en una funcion gemela)
        if found not in tu_sw and any(abs(l - found) > 0x2000 for l in labs): lost.append(s); continue
        out[found] = {"base": found, "r": s["r"], "default": (s["default"] + d0) if s["default"] else 0, "labels": labs}
        moved += 1
    added = 0
    for b, s in tu_sw.items():
        if b not in out: out[b] = s; added += 1
    write(os.path.join(root, "config", "ng2_tu_switch_tables_all.toml"), out.values(),
          "# Sesion 26 (TU): tablas del original trasladadas (tools/tu_switches.py) + XenonAnalyse sobre la TU para el resto")
    print("leidas de la tabla de la TU: %d" % leidas[0])
    print("trasladadas %d, sin trasladar %d, tomadas de XenonAnalyse %d, total %d" % (moved, len(lost), added, len(out)))
    for s in lost[:20]: print("  sin trasladar: 0x%08X (%d etiquetas)" % (s["base"], len(s["labels"])))

if __name__ == "__main__":
    main()

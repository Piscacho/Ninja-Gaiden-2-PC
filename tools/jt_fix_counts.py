"""Corrige tablas de saltos recortadas por XenonAnalyse. Para cada switch cuya tabla va justo detras del bctr
(lis/addi -> bctr+4), el numero real de entradas = hasta la primera etiqueta (donde empieza el codigo).
python -I jt_fix_counts.py basefile.bin entrada.toml salida.toml"""
import re, struct, sys
img = open(sys.argv[1], "rb").read(); B = 0x82000000
w = lambda a: struct.unpack_from(">I", img, a - B)[0]
text = open(sys.argv[2]).read()
blocks = re.split(r"(?=\[\[switch\]\])", text)
out = []; changed = 0
for b in blocks:
    m = re.search(r"base = 0x(\w+)", b)
    if not m: out.append(b); continue
    base = int(m.group(1), 16)
    labels = [int(x, 16) for x in re.findall(r"^\s+0x(\w+),", b, re.M)]
    # buscar bctr en las 8 instrucciones siguientes a base y lis/addi con el destino de la tabla
    bctr = next((base + 4 * k for k in range(8) if w(base + 4 * k) == 0x4E800420), None)
    tbl = None
    for k in range(6):
        x = w(base + 4 * k)
        if x >> 26 == 15 and not (x >> 16) & 31:
            rt = (x >> 21) & 31; hi = x & 0xFFFF
            for j in range(k + 1, k + 5):
                y = w(base + 4 * j)
                if y >> 26 == 14 and (y >> 16) & 31 == rt:
                    lo = y & 0xFFFF; lo -= 0x10000 if lo & 0x8000 else 0
                    tbl = ((hi << 16) + lo) & 0xFFFFFFFF; break
            break
    if bctr is None or tbl != bctr + 4:
        out.append(b); continue
    real = []; end = 0xFFFFFFFF; a = tbl
    while a < end and len(real) < 4096:
        l = w(a)
        if not (bctr < l < bctr + 0x20000) or l & 3: break
        real.append(l); end = min(end, l); a += 4
    if len(real) > len(labels) and real[:len(labels)] == labels:
        changed += 1
        sys.stderr.write("0x%08X: %d -> %d etiquetas\n" % (base, len(labels), len(real)))
        b = re.sub(r"labels = \[.*?\]", "labels = [\n" + "".join("    0x%08X,\n" % l for l in real) + "]", b, flags=re.S)
    out.append(b)
open(sys.argv[3], "w").write("".join(out))
sys.stderr.write("tablas ampliadas: %d\n" % changed)

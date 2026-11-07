"""Genera entradas 'functions' del TOML para switches en huecos sin .pdata.
Inicio = ultimo destino de bl (o inicio del hueco) antes del switch; fin = siguiente destino de bl
tras el ultimo caso (o fin del hueco). Lee los switches de el TOML de switches.
python -I gap_functions.py basefile.bin functions_pdata.txt switch_tables.toml recomp.log"""
import struct, bisect, re, sys
img = open(sys.argv[1], "rb").read(); B = 0x82000000
fn = [(int(a, 16), int(b, 16)) for a, b in (l.split() for l in open(sys.argv[2]))]
st = [f[0] for f in fn]
w = lambda a: struct.unpack_from(">I", img, a - B)[0]
sys.path.insert(0, __import__('os').path.dirname(__file__))
from pe_sections import sections
_t = [x for x in sections(img)[2] if x[0] == '.text'][0]
T0, T1 = _t[1], _t[1] + _t[2]
bl = set()
for a in range(T0, T1, 4):
    x = w(a)
    if x >> 26 == 18 and x & 3 == 1:
        d = x & 0x3FFFFFC
        if d & 0x2000000: d -= 0x4000000
        bl.add(a + d)
# NG2: los punteros desde .rdata/.data a codigo precedido de blr/b/relleno tambien son inicios de funcion
def _is_end(x): return x in (0x4E800020, 0, 0x4E800420) or (x >> 26 == 18 and x & 3 == 0)
_rd = [x for x in sections(img)[2] if x[0] in (".rdata", ".data")]   # sesion 26: rangos de la propia imagen (original o TU)
for _s0, _s1 in ((x[1], x[1] + x[2]) for x in _rd):
    for _a in range(_s0, _s1, 4):
        _t = w(_a)
        if T0 <= _t < T1 and _t & 3 == 0 and w(_t) != 0 and _is_end(w(_t - 4)): bl.add(_t)
bl = sorted(bl)
# switches: base -> labels
sw = {}; cur = None
for l in open(sys.argv[3]):
    m = re.match(r"base = 0x(\w+)", l)
    if m: cur = int(m.group(1), 16); sw[cur] = []
    m = re.match(r"\s+0x(\w+),", l)
    if m and cur is not None: sw[cur].append(int(m.group(1), 16))
bad = set(int(m.group(1), 16) for m in (re.match(r"ERROR: Switch case at (\w+)", l) for l in open(sys.argv[4])) if m)
out = {}
for a in sorted(bad):
    i = bisect.bisect_right(st, a) - 1
    s, e = fn[i][0] + fn[i][1], fn[i + 1][0]
    if not (s <= a < e): continue
    while w(s) == 0: s += 4
    while w(e - 4) == 0: e -= 4
    j = bisect.bisect_right(bl, a) - 1
    if j >= 0 and bl[j] > s: s = bl[j]
    cand = [b for b in sw if a - 0x40 <= b <= a]
    hi = max(sw[max(cand)]) if cand else a
    k = bisect.bisect_right(bl, hi)
    if k < len(bl) and bl[k] < e: e = bl[k]
    out[s] = max(out.get(s, 0), e - s)
print("functions = [")
for s in sorted(out): print("    { address = 0x%08X, size = 0x%X }," % (s, out[s]))
print("]")

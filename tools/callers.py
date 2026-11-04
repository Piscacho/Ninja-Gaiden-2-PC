"""Arbol de llamadores a partir del C++ generado. python tools/callers.py SUB_HEX [niveles]
Tambien busca la direccion como dato (punteros/vtables) en .rdata/.data."""
import re, sys, glob, os, pickle, struct
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cache = os.path.join(root, "logs", "callgraph.pkl")
if os.path.exists(cache) and os.path.getmtime(cache) > os.path.getmtime(os.path.join(root, "ppc", "ppc_func_mapping.cpp")):
    callers = pickle.load(open(cache, "rb"))
else:
    callers = {}
    for f in glob.glob(os.path.join(root, "ppc", "ppc_recomp.*.cpp")):
        cur = None
        for line in open(f):
            m = re.match(r"PPC_FUNC_IMPL\(__imp__(sub_\w+|\w+)\)", line)
            if m: cur = m.group(1); continue
            for c in re.findall(r"\b(sub_[0-9A-F]{8})\(ctx, base\)", line):
                if cur and c != cur: callers.setdefault(c, set()).add(cur)
    pickle.dump(callers, open(cache, "wb"))
img = open(os.path.join(root, "xex", "basefile.bin"), "rb").read()
def datarefs(a):
    p = struct.pack(">I", a); out = []; j = img.find(p)
    while j != -1 and len(out) < 5:
        va = 0x82000000 + j
        if not (0x82291A00 <= va < 0x822E4A40): out.append(va)   # .pdata no cuenta
        j = img.find(p, j + 1)
    return out
def walk(fn, depth, seen):
    a = int(fn[4:], 16)
    refs = datarefs(a)
    print("  " * depth + fn + ("  [puntero en " + ", ".join("%08X" % r for r in refs) + "]" if refs else ""))
    if depth >= maxd or fn in seen: return
    seen.add(fn)
    for c in sorted(callers.get(fn, ())): walk(c, depth + 1, seen)
maxd = int(sys.argv[2]) if len(sys.argv) > 2 else 5
def coderefs(a):
    # lis rX,hi ; addi rY,rX,lo en el C++ generado: "ctx.rY.s64 = ctx.rX.s64 + lo;" tras "ctx.rX.s64 = HI;"
    hi = (a + 0x8000) >> 16 & 0xFFFF; lo = a & 0xFFFF; lo = lo - 0x10000 if lo >= 0x8000 else lo
    his = str(((hi << 16) ^ 0x80000000) - 0x80000000)
    out = []
    for f in glob.glob(os.path.join(root, "ppc", "ppc_recomp.*.cpp")):
        lines = open(f).read().splitlines(); cur = None
        for i, line in enumerate(lines):
            m = re.match(r"PPC_FUNC_IMPL\(__imp__(\w+)\)", line)
            if m: cur = m.group(1)
            if line.endswith(" + %d;" % lo) and any((" = " + his + ";") in lines[k] for k in range(max(0, i - 40), i)):
                out.append(cur)
    return sorted(set(out))
walk("sub_" + sys.argv[1].upper(), 0, set())
if len(sys.argv) > 3: print("cargado con lis/addi en:", coderefs(int(sys.argv[1], 16)))

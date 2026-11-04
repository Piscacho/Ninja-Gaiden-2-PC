"""Nombra imports con las tablas de Xenia (BSD).
Uso: python -I name_imports.py imports.txt xam_table.inc xboxkrnl_table.inc salida_named.txt"""
import re, sys
def table(p):
    t = {}
    for m in re.finditer(r"XE_EXPORT\(\s*\w+\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*(\w+)", open(p).read()):
        t[int(m.group(1), 16)] = m.group(2)
    return t
tabs = {"xam.xex": table(sys.argv[2]), "xboxkrnl.exe": table(sys.argv[3])}
out = open(sys.argv[4], "w"); miss = 0; names = set()
for l in open(sys.argv[1]):
    if l.startswith("#"): continue
    lib, o, _, kind, at = l.split("\t")
    n = tabs[lib].get(int(o[4:]), "")
    if not n: miss += 1; n = "%s_ord%s" % (lib.split(".")[0], o[4:])
    names.add((lib, n, kind))
    out.write("%s\t%s\t%s\t%s\t%s\n" % (lib, o[4:], kind, at.strip(), n))
print("entradas sin nombre:", miss, "unicos:", len({(a, b) for a, b, c in names}))

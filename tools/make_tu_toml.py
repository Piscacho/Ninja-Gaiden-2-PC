"""Sesion 26: genera config/ng2_tu.toml (XenonRecomp para la actualizacion del titulo #2) a partir de config/ng2.toml.
Funciones sin .pdata = las del original trasladadas (config/ng2_tu_functions.txt, tools/tu_functions.py) + las manuales a las
que se llega por punteros (tu/datamap.txt) + huecos con switch de la TU (config/ng2_tu_gap_functions.txt, si existe).
Uso: python -I tools/make_tu_toml.py"""
import os, re
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cfg = open(os.path.join(root, "config", "ng2.toml")).read()
head = cfg[:cfg.find("functions = [")]
rep = {
    'file_path = "../xex/default.xex"': 'file_path = "../tu/tu_default.xex"',
    'out_directory_path = "../ppc"': 'out_directory_path = "../ppc_tu"',
    'switch_table_file_path = "ng2_switch_tables_all.toml"': 'switch_table_file_path = "ng2_tu_switch_tables_fixed.toml"',
    "restgprlr_14_address = 0x83954280": "restgprlr_14_address = 0x83979AD0",
    "savegprlr_14_address = 0x83954230": "savegprlr_14_address = 0x83979A80",
    "restfpr_14_address   = 0x8395582C": "restfpr_14_address   = 0x8397B07C",
    "savefpr_14_address   = 0x839557E0": "savefpr_14_address   = 0x8397B030",
    "restvmx_14_address   = 0x83957178": "restvmx_14_address   = 0x8397C9C8",
    "savevmx_14_address   = 0x83956EE0": "savevmx_14_address   = 0x8397C730",
    "restvmx_64_address   = 0x8395720C": "restvmx_64_address   = 0x8397CA5C",
    "savevmx_64_address   = 0x83956F74": "savevmx_64_address   = 0x8397C7C4",
    "longjmp_address = 0x83955930": "longjmp_address = 0x8397B180",
    "setjmp_address  = 0x83955C50": "setjmp_address  = 0x8397B4A0",
}
for a, b in rep.items():
    assert a in head, a
    head = head.replace(a, b)
head = head.replace("# XenonRecomp - Ninja Gaiden II (X360, 544307D5)", "# XenonRecomp - Ninja Gaiden II TU#2 (generado por tools/make_tu_toml.py; no editar a mano)", 1)
fs = {}
for a, s in re.findall(r"address = 0x(\w+), size = 0x(\w+)", open(os.path.join(root, "config", "ng2_tu_functions.txt")).read()):
    fs[int(a, 16)] = int(s, 16)
# hojas por punteros (sesion 4 del original) via el mapa de datos
MANUAL_PTR = {0x8247FEA0: 0x80, 0x837EFD90: 0x50, 0x837EFDE0: 0x2C, 0x8386BD48: 0x368, 0x8386C0B0: 0x30, 0x8386C0E0: 0x44}
dm = {int(a, 16): int(b, 16) for a, b, n in (l.split() for l in open(os.path.join(root, "tu", "datamap.txt")))}
manual = set()
for a, s in MANUAL_PTR.items():
    t = dm[a]; fs[t] = s; manual.add(t)
gp = os.path.join(root, "config", "ng2_tu_gap_functions.txt")
if os.path.exists(gp):
    for a, s in re.findall(r"address = 0x(\w+), size = 0x(\w+)", open(gp).read()):
        a = int(a, 16); s = int(s, 16)
        if a not in fs: fs[a] = s
items = sorted(fs.items())
for i in range(len(items) - 1):   # solapes: se recorta la anterior
    (a, s), (b, _) = items[i], items[i + 1]
    if a + s > b: fs[a] = b - a
items = sorted(fs.items())
out = head + "functions = [\n" + ",\n".join("{ address = 0x%08X, size = 0x%X }" % x for x in items) + "\n]\n"
open(os.path.join(root, "config", "ng2_tu.toml"), "w").write(out)
print(len(items), "funciones")

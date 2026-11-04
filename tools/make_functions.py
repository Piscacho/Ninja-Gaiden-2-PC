"""Reescribe la lista functions de config/ng2.toml = config/ng2_gap_functions.txt + entradas manuales.
python -I tools/make_functions.py"""
import re, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANUAL = {
    0x83955C50: 0x2CC,  # setjmp
    0x83955F20: 0x1C, 0x83955F40: 0x1C, 0x83955F60: 0x1C,  # hojas tras setjmp (tabla de tipos de caracter)
    0x823B5B90: 0xC0, 0x823B5C50: 0xA8,  # dos switches seguidos sin .pdata (tabla de 0x823B5B90 corregida a mano)
    0x83957FA8: 0x28, 0x83963BF0: 0x18, 0x83964BD0: 0x18,  # reanudacion __except del CRT
    # sesion 4: hojas sin .pdata a las que se llega por punteros lis/addi (tools/lisaddi_functions.py); 0x8386BD48 = decodificador WMV
    0x8247FEA0: 0x80, 0x837EFD90: 0x50, 0x837EFDE0: 0x2C, 0x8386BD48: 0x368, 0x8386C0B0: 0x30, 0x8386C0E0: 0x44,
}
cfg = os.path.join(root, "config", "ng2.toml")
gap = dict((int(a, 16), int(s, 16)) for a, s in re.findall(r"address = 0x(\w+), size = 0x(\w+)", open(os.path.join(root, "config", "ng2_gap_functions.txt")).read()))
gap.update(MANUAL)
# una funcion manual dentro de una de hueco estimada: la de hueco se recorta (comprobado que no deja fuera etiquetas de switch)
items = sorted(gap.items())
for i in range(len(items) - 1):
    (a, s_), (b, _) = items[i], items[i + 1]
    if a + s_ > b and b in MANUAL and a not in MANUAL: gap[a] = b - a
fs = sorted(gap.items())
bad = [(hex(a), hex(b)) for (a, s), (b, _) in zip(fs, fs[1:]) if a + s > b]
assert not bad, bad
t = open(cfg).read()
i = t.find("functions = [")
t = (t[:i] if i >= 0 else t.rstrip() + "\n") + "functions = [\n" + ",\n".join("{ address = 0x%08X, size = 0x%X }" % x for x in fs) + "\n]\n"
open(cfg, "w").write(t)
print(len(fs), "funciones")

"""Comprueba el registro de indice (r = N) de cada tabla de saltos del TOML contra el codigo: busca, desde la base hasta el bctr,
la instruccion que escala el indice (slwi/rlwinm r0, rN, 2). Las tablas indexadas por byte (lbzx r0 ... ; slwi r0, r0, 2) se omiten.
Sesion 15: tres tablas sin comprobacion de rango tenian el registro mal (r9 en vez de r6...) y el juego caia al empezar la partida.
Uso: python tools/jt_check_regs.py [xex/basefile.bin] [config/ng2_switch_tables_all.toml]"""
import re, sys, os
import capstone

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
img_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "xex", "basefile.bin")
toml_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, "config", "ng2_switch_tables_all.toml")
img = open(img_path, "rb").read(); B = 0x82000000
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_64 | capstone.CS_MODE_BIG_ENDIAN)
txt = open(toml_path).read()
bad = 0
for m in re.finditer(r"base = (0x[0-9A-Fa-f]+)\s*\nr = (\d+)", txt):
    base, r = int(m.group(1), 16), int(m.group(2))
    ea, idx, byte_table = base, None, False
    for _ in range(14):
        ins = list(md.disasm(img[ea - B:ea - B + 4], ea, 1))
        if ins:
            t = ins[0].mnemonic + " " + ins[0].op_str
            if t.startswith("lbzx r0"): byte_table = True
            mm = re.match(r"(slwi|rlwinm) r0, r(\d+), 2", t)
            if mm: idx = int(mm.group(2))
            if t.startswith("bctr"): break
        ea += 4
    if byte_table or idx is None: continue
    if idx != r:
        bad += 1
        print("tabla 0x%08X: el TOML dice r%d, el codigo usa r%d" % (base, r, idx))
print("%d tablas con el registro mal" % bad)

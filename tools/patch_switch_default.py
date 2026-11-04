"""default de los switch: salto indirecto real a ctr en vez de __builtin_unreachable(). Idempotente.
python tools/patch_switch_default.py XenonRecomp/XenonRecomp/recompiler.cpp"""
import sys
p = sys.argv[1]; s = open(p, encoding="utf-8").read()
old = r'println("\t\t__builtin_unreachable();");'
new = r'println("\t\tPPC_SWITCH_DEFAULT({}.u32, {}.u32);\n\t\treturn;", r(switchTable->second.r), ctr());'
if old in s:
    s = s.replace(old, new, 1); open(p, "w", encoding="utf-8").write(s); print("parcheado")
else:
    print("ya estaba parcheado" if "PPC_SWITCH_DEFAULT" in s else "NO ENCONTRADO")

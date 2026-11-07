"""Sesion 26: genera runtime/tu_remap.h: #define sub_<original> sub_<TU> (y __imp__) para las funciones del juego que usa el runtime.
Se incluye solo al compilar la version con la actualizacion (NG2_TU), despues de <ppc_recomp_shared.h>.
Las parejas salen de tu/funcmap.txt / tu/callmap.txt / busqueda por patron (sesion 26) y se comprobaron una a una.
Uso: python -I tools/make_tu_remap.py"""
import os, re, glob
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAIRS = {   # original -> TU (comprobado en NOTAS_SESION26.md)
    0x822F9FB8: 0x822F9FB8, 0x822FF5A8: 0x822FF5A8, 0x82385A00: 0x82385AB0, 0x823D6F00: 0x823D72A0, 0x82418130: 0x82418980,
    0x8247B2A8: 0x8247F4B8, 0x8365EA70: 0x83681548, 0x83660658: 0x836833C0, 0x83661650: 0x836843A8, 0x836EC7F0: 0x83710FE8,
    0x836ECF20: 0x83711718, 0x836ED380: 0x83711B78, 0x836ED768: 0x83711F60, 0x836EDAF0: 0x837122E8, 0x836FA630: 0x8371EE18,
    0x8374F078: 0x83774818, 0x837CC920: 0x837F2000, 0x8380E4F8: 0x83833BE0, 0x8380E5D8: 0x83833CC0, 0x8380E638: 0x83833D20,
}
used = set()
for f in glob.glob(os.path.join(root, "runtime", "*.cpp")):
    used |= set(int(x, 16) for x in re.findall(r"sub_(8[0-9A-F]{7})", open(f, encoding="utf-8", errors="ignore").read()))
lines = ["// Generado por tools/make_tu_remap.py (sesion 26): funciones del original -> actualizacion del titulo #2. No editar a mano.",
         "#pragma once", "#ifdef NG2_TU"]
for a in sorted(PAIRS):
    if a == PAIRS[a]: continue
    lines.append("#define sub_%08X sub_%08X" % (a, PAIRS[a]))
    lines.append("#define __imp__sub_%08X __imp__sub_%08X" % (a, PAIRS[a]))
lines.append("#endif")
open(os.path.join(root, "runtime", "tu_remap.h"), "w").write("\n".join(lines) + "\n")
missing = sorted(used - set(PAIRS))
print("%d parejas; usadas en el runtime sin pareja (deben ir con #ifndef NG2_TU o en comentarios): %s" % (len(PAIRS), " ".join("%08X" % a for a in missing)))

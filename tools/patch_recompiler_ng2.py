"""Instrucciones que usa Ninja Gaiden II y no trae XenonRecomp (ni el parche de GH).
Idempotente: python tools/patch_recompiler_ng2.py XenonRecomp/XenonRecomp/recompiler.cpp
Recordatorio: los vectores se guardan invertidos (elemento i de PPC = indice N-1-i); las operaciones
por carril son simetricas y el desplazamiento del carril i esta en su byte bajo (u8[i*tam])."""
import sys

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
MARK = "// ---- NG2 instrucciones anadidas ----"
ANCHOR = "// ---- GH:WoR instrucciones anadidas ----"

V3 = 'println("\\tsimde_mm_store_si128((simde__m128i*){{}}.u8, {fn}(simde_mm_load_si128((simde__m128i*){{}}.u8), simde_mm_load_si128((simde__m128i*){{}}.u8)));", v(insn.operands[0]), v(insn.operands[{a}]), v(insn.operands[{b}]));'
RC16 = '        if (strchr(insn.opcode->name, \'.\'))\n            println("\\t{}.setFromMask(simde_mm_load_si128((simde__m128i*){}.u8), 0xFFFF);", cr(6), v(insn.operands[0]));\n'


def simple(inst, fn, a=1, b=2, rc=False):
    s = "    case PPC_INST_%s:\n        %s\n" % (inst, V3.format(fn=fn, a=a, b=b))
    if rc:
        s += RC16
    return s + "        break;\n\n"


def lanes(inst, n, typ, expr):
    # expr usa {d} {a} {s}: destino, origen, cantidad de desplazamiento
    s = "    case PPC_INST_%s:\n" % inst
    s += "        for (size_t i = 0; i < %d; i++)\n" % n
    s += '            println("\\t{}.%s[{}] = %s;", %s);\n' % (typ, expr[0], expr[1])
    return s + "        break;\n\n"


code = MARK + "\n"
# --- enteros de 16 bits (decodificador WMV / librerias del XDK) ---
code += lanes("VSLH", 8, "u16", ("{}.u16[{}] << ({}.u8[{}] & 0xF)", "v(insn.operands[0]), i, v(insn.operands[1]), i, v(insn.operands[2]), i * 2"))
code += lanes("VSRH", 8, "u16", ("{}.u16[{}] >> ({}.u8[{}] & 0xF)", "v(insn.operands[0]), i, v(insn.operands[1]), i, v(insn.operands[2]), i * 2"))
code += lanes("VSRAH", 8, "s16", ("{}.s16[{}] >> ({}.u8[{}] & 0xF)", "v(insn.operands[0]), i, v(insn.operands[1]), i, v(insn.operands[2]), i * 2"))
code += lanes("VSRAB", 16, "s8", ("{}.s8[{}] >> ({}.u8[{}] & 0x7)", "v(insn.operands[0]), i, v(insn.operands[1]), i, v(insn.operands[2]), i"))
code += lanes("VRLH", 8, "u16", ("uint16_t(({}.u16[{}] << ({}.u8[{}] & 0xF)) | ({}.u16[{}] >> ((16 - ({}.u8[{}] & 0xF)) & 0xF)))",
                                  "v(insn.operands[0]), i, v(insn.operands[1]), i, v(insn.operands[2]), i * 2, v(insn.operands[1]), i, v(insn.operands[2]), i * 2"))
code += ("    case PPC_INST_VSPLTISH:\n"
         '        println("\\tsimde_mm_store_si128((simde__m128i*){}.u16, simde_mm_set1_epi16(int16_t({})));", v(insn.operands[0]), int16_t(insn.operands[1]));\n'
         "        break;\n\n")
code += simple("VMAXSH", "simde_mm_max_epi16")
code += simple("VMINSH", "simde_mm_min_epi16")
code += simple("VAVGUH", "simde_mm_avg_epu16")
code += simple("VADDSBS", "simde_mm_adds_epi8")
code += simple("VSUBUBM", "simde_mm_sub_epi8")
code += simple("VCMPEQUH", "simde_mm_cmpeq_epi16", rc=True)
code += simple("VCMPGTSH", "simde_mm_cmpgt_epi16", rc=True)
code += simple("VCMPGTSW", "simde_mm_cmpgt_epi32", rc=True)
code += ("    case PPC_INST_VNOR:\n"
         '        println("\\tsimde_mm_store_si128((simde__m128i*){}.u8, simde_mm_xor_si128(simde_mm_or_si128(simde_mm_load_si128((simde__m128i*){}.u8), simde_mm_load_si128((simde__m128i*){}.u8)), simde_mm_set1_epi32(-1)));", v(insn.operands[0]), v(insn.operands[1]), v(insn.operands[2]));\n'
         "        break;\n\n")
# --- empaquetado (operandos invertidos por la inversion del vector, como VPKSHUS) ---
code += simple("VPKSHSS", "simde_mm_packs_epi16", a=2, b=1)
code += simple("VPKSWUS", "simde_mm_packus_epi32", a=2, b=1)
code += ("    case PPC_INST_VPKUHUS:\n"
         '        println("\\tsimde_mm_store_si128((simde__m128i*){}.u8, simde_mm_packus_epi16(simde_mm_min_epu16(simde_mm_load_si128((simde__m128i*){}.u8), simde_mm_set1_epi16(255)), simde_mm_min_epu16(simde_mm_load_si128((simde__m128i*){}.u8), simde_mm_set1_epi16(255))));", v(insn.operands[0]), v(insn.operands[2]), v(insn.operands[1]));\n'
         "        break;\n\n")
# --- float -> u32 con saturacion ---
code += ("    case PPC_INST_VCTUXS:\n"
         "        printSetFlushMode(true);\n"
         "        for (size_t i = 0; i < 4; i++)\n"
         '            println("\\t{{ float t = {}.f32[{}] * {}.0f; {}.u32[{}] = (t != t || t <= 0.0f) ? 0u : (t >= 4294967296.0f ? 0xFFFFFFFFu : uint32_t(t)); }}", v(insn.operands[1]), i, 1u << insn.operands[2], v(insn.operands[0]), i);\n'
         "        break;\n\n")
# --- enteros escalares ---
code += ("    case PPC_INST_MULHD:\n"
         '        println("\\t{}.s64 = int64_t((__int128)({}.s64) * (__int128)({}.s64) >> 64);", r(insn.operands[0]), r(insn.operands[1]), r(insn.operands[2]));\n'
         "        if (strchr(insn.opcode->name, '.'))\n"
         '            println("\\t{}.compare<int64_t>({}.s64, 0, {});", cr(0), r(insn.operands[0]), xer());\n'
         "        break;\n\n")
# --- operaciones de bits de CR (operandos = numero de bit 0..31) ---
code += ("    case PPC_INST_CROR:\n"
         "    case PPC_INST_CRORC:\n"
         "    {\n"
         '        static const char* crb[] = { "lt", "gt", "eq", "so" };\n'
         "        const bool c = insn.opcode->id == PPC_INST_CRORC;\n"
         '        println("\\t{}.{} = {}.{} | ({}.{}{});", cr(insn.operands[0] / 4), crb[insn.operands[0] % 4], cr(insn.operands[1] / 4), crb[insn.operands[1] % 4],\n'
         '            cr(insn.operands[2] / 4), crb[insn.operands[2] % 4], c ? " ^ 1" : "");\n'
         "        break;\n"
         "    }\n\n")
code += "// ---- fin NG2 ----\n"

changed = False
if MARK not in src:
    assert ANCHOR in src, "no encuentro el bloque de GH (aplicar antes el parche de GH)"
    src = src.replace(ANCHOR, code + ANCHOR, 1)
    changed = True

# alias: variantes "l" (pista LRU) = misma semantica
for base, alias in (("LVX128", "LVXL128"), ("STVLX128", "STVLXL128")):
    a = "    case PPC_INST_%s:\n" % base
    if "case PPC_INST_%s:" % alias not in src:
        assert a in src, base
        src = src.replace(a, a + "    case PPC_INST_%s:\n" % alias, 1)
        changed = True

# vcmpgtuh. : falta el bit RC
old = 'simde_mm_cmpgt_epu16(simde_mm_load_si128((simde__m128i*){}.u16), simde_mm_load_si128((simde__m128i*){}.u16)));", v(insn.operands[0]), v(insn.operands[1]), v(insn.operands[2]));\n        break;'
if old in src:
    src = src.replace(old, old[:-len("        break;")] + RC16 + "        break;", 1)
    changed = True

if changed:
    open(path, "w", encoding="utf-8").write(src)
print("parcheado" if changed else "ya estaba parcheado")

"""Desensambla PPC (Xenon, big-endian) con capstone.
Uso: python ppcdasm.py basefile.bin DIRECCION_HEX [N_INSTR]"""
import struct, sys, capstone
img = open(sys.argv[1], "rb").read(); B = 0x82000000
a = int(sys.argv[2], 16); n = int(sys.argv[3]) if len(sys.argv) > 3 else 32
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_64 | capstone.CS_MODE_BIG_ENDIAN)
md.skipdata = False
for k in range(n):
    ea = a + 4 * k; raw = img[ea - B: ea - B + 4]
    ins = list(md.disasm(raw, ea, 1))
    t = "%s %s" % (ins[0].mnemonic, ins[0].op_str) if ins else ".long 0x%08X  ; (VMX128/no decodificada)" % struct.unpack(">I", raw)[0]
    print("0x%08X  %s  %s" % (ea, raw.hex().upper(), t))

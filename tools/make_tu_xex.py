"""Sesion 26: XEX de la actualizacion para XenonAnalyse/XenonRecomp: cabeceras ya parcheadas (tu/work/new_hdr.bin) con el formato
cambiado a sin cifrar y sin comprimir (0x3FF: cifrado 0, compresion 0) + la imagen parcheada (tu/work/tu_basefile.bin).
Uso: python -I tools/make_tu_xex.py  ->  tu/tu_default.xex y xex/tu_basefile.bin"""
import struct, shutil
h = bytearray(open("tu/work/new_hdr.bin", "rb").read()); img = open("tu/work/tu_basefile.bin", "rb").read()
n = struct.unpack_from(">I", h, 0x14)[0]
for i in range(n):
    k, v = struct.unpack_from(">II", h, 0x18 + i * 8)
    if k == 0x3FF: struct.pack_into(">HH", h, v + 4, 0, 0)
pe = struct.unpack_from(">I", h, 8)[0]; assert pe == len(h), (pe, len(h))
open("tu/tu_default.xex", "wb").write(bytes(h) + img)
shutil.copy("tu/work/tu_basefile.bin", "xex/tu_basefile.bin")
print("tu/tu_default.xex %d bytes (cabeceras 0x%X + imagen 0x%X)" % (len(h) + len(img), len(h), len(img)))

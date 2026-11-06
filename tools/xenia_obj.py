import sys, struct
sys.path.insert(0, sys.argv[2]); import xsearch, ctypes
hp = ctypes.windll.kernel32.OpenProcess(0x0410, False, int(sys.argv[1]))
for obj in (0x84B56580, 0x84B56790, 0x84B569A0):
    b = xsearch.read(hp, obj, 0xC0); u = lambda o: struct.unpack(">I", b[o:o+4])[0]
    print("obj %08X cnt %d +4 %08X +12 %08X +100 %08X +108 %08X +112 %08X +116 %08X +120 %08X +124 %08X" % (obj, b[20], u(4), u(12), u(100), u(108), u(112), u(116), u(120), u(124)), flush=True)

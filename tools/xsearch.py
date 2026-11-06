import ctypes, sys, struct, subprocess, time, os
from ctypes import wintypes
# uso: xsearch.py <pid> <hex patron> : busca en la memoria de invitado de Xenia (0x100000000 + 0..0xFFFFFFFF)
k32 = ctypes.windll.kernel32
class MBI(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p), ("AllocationBase", ctypes.c_void_p), ("AllocationProtect", wintypes.DWORD), ("PartitionId", wintypes.WORD),
                ("RegionSize", ctypes.c_size_t), ("State", wintypes.DWORD), ("Protect", wintypes.DWORD), ("Type", wintypes.DWORD)]
def search(pid, pat, lo=0x100000000 + 0x40000000, hi=0x100000000 + 0x100000000):
    hp = k32.OpenProcess(0x0410, False, pid); a = lo; hits = []
    m = MBI()
    while a < hi and k32.VirtualQueryEx(hp, ctypes.c_void_p(a), ctypes.byref(m), ctypes.sizeof(m)):
        base = m.BaseAddress or 0; size = m.RegionSize
        if m.State == 0x1000 and not (m.Protect & 0x101):
            buf = ctypes.create_string_buffer(size); got = ctypes.c_size_t(0)
            if k32.ReadProcessMemory(hp, ctypes.c_void_p(base), buf, size, ctypes.byref(got)):
                data = buf.raw[:got.value]; i = data.find(pat)
                while i >= 0: hits.append(base + i - 0x100000000); i = data.find(pat, i + 1)
        a = base + size
    return hp, hits
def read(hp, ga, n):
    buf = ctypes.create_string_buffer(n); got = ctypes.c_size_t(0)
    k32.ReadProcessMemory(hp, ctypes.c_void_p(0x100000000 + ga), buf, n, ctypes.byref(got)); return buf.raw
if __name__ == "__main__":
    pid = int(sys.argv[1]); pat = bytes.fromhex(sys.argv[2])
    hp, hits = search(pid, pat); print("hits", ["%08X" % h for h in hits[:20]])

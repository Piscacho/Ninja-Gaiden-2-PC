"""Lanza Xenia con el ISO de NG2, le manda pulsaciones y captura la ventana (sesion 20).
Uso: python tools/xenia_drive.py salida.png "30:START,38:A,..." "62,80"   (XARGS="--dump_shaders=dir" opcional)
Requiere keyboard_mode = 1 en ref_xenia/xenia-canary.config.toml (START = tecla X, A = tecla ';').
Pone la ventana de Xenia en primer plano en cada pulsacion. Llega a la cinematica de Sonia hacia el segundo 62."""
import subprocess, time, sys, os, ctypes
from ctypes import wintypes
root = r"G:\ninja gaiden 2\Recomp"
xd = os.path.join(root, "ref_xenia"); exe = os.path.join(xd, "xenia_canary.exe")
iso = os.path.join(root, "..", "Base", "juego", "Ninja Gaiden II (World) (En,Ja,Fr,De,Es,It,Zh,Ko,Pl,Ru)", "Ninja Gaiden II (World) (En,Ja,Fr,De,Es,It,Zh,Ko,Pl,Ru).iso")
out = sys.argv[1]; presses = sys.argv[2]; shots = [float(s) for s in sys.argv[3].split(",")]
ev = []  # (t, key)
for spec in presses.split(","):
    t, k = spec.split(":"); ev.append((float(t), k))
for s in shots: ev.append((s, "SHOT"))
for spec in os.environ.get("XMEM", "").split(","):
    if spec: t, k = spec.split(":", 1); ev.append((float(t), k))
ev.sort(key=lambda e: e[0])
u = ctypes.windll.user32
p = subprocess.Popen([exe] + os.environ.get("XARGS","").split() + [iso], cwd=xd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
t0 = time.time()
VK = {"START": 0x58, "A": 0xBA}
def hwnd():
    res = []
    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(h, l):
        pid = wintypes.DWORD(); u.GetWindowThreadProcessId(h, ctypes.byref(pid))
        if pid.value == p.pid and u.IsWindowVisible(h): res.append(h)
        return True
    u.EnumWindows(cb, 0); return res[0] if res else None
ps = r'''
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
public struct RECT { public int L, T, R, B; } }
"@
$h = [IntPtr]HWND; $r = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null
$b = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($b)
$g.CopyFromScreen($r.L, $r.T, 0, 0, $b.Size); $b.Save("OUT");
'''
for t, k in ev:
    time.sleep(max(0, t0 + t - time.time()))
    h = hwnd()
    if not h: continue
    u.ShowWindow(h, 9); u.SetForegroundWindow(h)
    if k.startswith("PY="):   # PY=<script>: ejecuta python <script> <pid de Xenia> <carpeta del script>
        sc = k[3:]; r = subprocess.run([sys.executable, sc, str(p.pid), os.path.dirname(sc)], capture_output=True, text=True); print(r.stdout, r.stderr, flush=True); continue
    if k.startswith("MEM"):   # MEM-<dir hex>-<bytes hex>: lee la memoria de invitado de Xenia (base 0x100000000) a salida_<dir>.bin
        _, a, n = k.split("-"); a = int(a, 16); n = int(n, 16)
        k32 = ctypes.windll.kernel32; hp = k32.OpenProcess(0x0410, False, p.pid)
        buf = ctypes.create_string_buffer(n); got = ctypes.c_size_t(0)
        for basev in (0x100000000,):
            ok = k32.ReadProcessMemory(hp, ctypes.c_void_p(basev + a), buf, n, ctypes.byref(got))
            print("MEM %08X ok=%d leidos=%d" % (a, ok, got.value), flush=True)
            if ok: open(out.replace(".png", "_%08X_t%d.bin" % (a, int(t))), "wb").write(buf.raw)
        k32.CloseHandle(hp); continue
    if k == "SHOT":
        o = out.replace(".png", "_%03d.png" % int(t))
        subprocess.run(["powershell", "-NoProfile", "-Command", ps.replace("HWND", str(h)).replace("OUT", o)], capture_output=True)
        print("captura", o, flush=True)
    else:
        vk = VK[k]; sc = u.MapVirtualKeyW(vk, 0)
        u.keybd_event(vk, sc, 0, 0); time.sleep(0.25); u.keybd_event(vk, sc, 2, 0)
p.kill()

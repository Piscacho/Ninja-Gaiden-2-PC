"""Lanza Xenia con el ISO de NG2, espera N segundos y captura la ventana (PNG por PowerShell). python xenia_shot.py salida.png segundos"""
import subprocess, time, sys, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
xd = os.path.join(root, "ref_xenia"); exe = os.path.join(xd, "xenia_canary.exe")
iso = os.path.join(root, "..", "Base", "juego", "Ninja Gaiden II (World) (En,Ja,Fr,De,Es,It,Zh,Ko,Pl,Ru)", "Ninja Gaiden II (World) (En,Ja,Fr,De,Es,It,Zh,Ko,Pl,Ru).iso")
out = os.path.abspath(sys.argv[1]); secs = [float(s) for s in sys.argv[2].split(",")]
p = subprocess.Popen([exe, iso], cwd=xd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
t0 = time.time()
ps = r'''
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
public struct RECT { public int L, T, R, B; } }
"@
$h = (Get-Process -Id PID).MainWindowHandle; $r = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null
$b = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [System.Drawing.Graphics]::FromImage($b)
$g.CopyFromScreen($r.L, $r.T, 0, 0, $b.Size); $b.Save("OUT"); 
'''
for i, s in enumerate(secs):
    time.sleep(max(0, t0 + s - time.time()))
    o = out.replace(".png", "_%d.png" % int(s))
    subprocess.run(["powershell", "-NoProfile", "-Command", ps.replace("PID", str(p.pid)).replace("OUT", o)], capture_output=True)
    print("captura", o)
p.kill()

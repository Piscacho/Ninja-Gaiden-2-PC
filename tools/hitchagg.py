"""Resume las muestras de NG2_HITCHPROF por tiron: por hilo, muestras y funciones (propias e incluidas) mas frecuentes.
Uso: python hitchagg.py logs/runNN.log [n funciones]"""
import subprocess, sys, os, collections
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
exe = os.path.join(root, "project", "build", "NG2.exe")
sym = os.path.join("G:/Guitar Hero Warrior of rock/Recomp", "toolchain", "clang+llvm-23.1.4-x86_64-pc-windows-msvc", "bin", "llvm-symbolizer.exe")
top = int(sys.argv[2]) if len(sys.argv) > 2 else 6
hitches = []
for l in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if l.startswith("[prof] ---- tiron"): hitches.append((l.strip()[7:], collections.defaultdict(list)))
    elif l.startswith("[prof] T") and hitches:
        f = l.split()[1:]; hitches[-1][1][f[0]].append(f[1:])
addrs = sorted({a for _, th in hitches for ss in th.values() for s in ss for a in s})
names = {}
for i in range(0, len(addrs), 400):
    chunk = addrs[i:i + 400]
    out = subprocess.run([sym, "--obj=" + exe, "--functions=short", "--no-inlines"] + ["0x" + a for a in chunk], capture_output=True, text=True).stdout.split("\n\n")
    for a, block in zip(chunk, out): names[a] = block.strip().split("\n")[0].replace("__imp__", "")
for title, th in hitches:
    print("=====", title)
    for t, ss in sorted(th.items(), key=lambda kv: -len(kv[1]))[:10]:
        own = collections.Counter(names.get(s[0], s[0]) for s in ss if s)
        incl = collections.Counter(f for s in ss for f in set(names.get(a, a) for a in s))
        print(f"  {t}: {len(ss)} muestras | propia: " + ", ".join(f"{k} {v}" for k, v in own.most_common(top)))
        print(f"      incluida: " + ", ".join(f"{k} {v}" for k, v in incl.most_common(top + 4)))

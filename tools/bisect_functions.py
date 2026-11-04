"""Busca que entrada de functions hace caer XenonRecomp (rc != 0) en la fase de analisis."""
import re, subprocess, sys, time
toml = open("ng2.toml").read()
head = toml[:toml.index("functions = [")]
ent = re.findall(r"\{ address = 0x\w+, size = 0x\w+ \}", toml)
def run(sub):
    open("_bisect.toml", "w").write(head + "functions = [\n" + ",\n".join(sub) + "\n]\n")
    p = subprocess.Popen([r"..\XenonRecomp\build-msvc\XenonRecomp\XenonRecomp.exe", "_bisect.toml", r"..\XenonRecomp\XenonUtils\ppc_context.h"],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    # basta con ver si llega a "Recompiling"
    for line in p.stdout:
        if "Recompiling" in line:
            p.kill(); return True
    return p.wait() == 0
lo = ent
while len(lo) > 1:
    a, b = lo[:len(lo)//2], lo[len(lo)//2:]
    if not run(a): lo = a
    elif not run(b): lo = b
    else: print("falla solo combinada"); break
print("culpable:", lo if len(lo) <= 4 else len(lo))

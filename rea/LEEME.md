# REA en Ninja Gaiden II

Probado el 2026-11-01 (sesión 1):
- `open_binary` sobre `xex/basefile.bin`: el modo `raw-binary` solo acepta 6502/x86/x86_64/arm/arm64/m68k; el PE es PowerPC (0x1F2), no soportado.
- `execute_rizin_command`: no disponible en esta instalación (`ERR_REA_WINDOWS_NATIVE`, rizin no instalado).
- `open_binary` sobre los paquetes STFS (LIVE) de los DLC: formato no soportado.

Por eso el análisis de código se hace con scripts propios (`tools/`) y capstone (`tools/ppcdasm.py`).
REA sigue siendo útil para el lado PC (el ejecutable recompilado x86_64 y el runtime) a partir de la sesión 3.
Si se instala rizin (`rz-bin`/`rizin` en PATH), reintentar `execute_rizin_command` con `e asm.arch=ppc; e cfg.bigendian=true`.

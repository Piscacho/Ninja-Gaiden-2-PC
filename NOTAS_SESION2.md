# Sesión 2 — XenonRecomp, C++ generado y primera compilación

Fecha: 2026-11-01. Sin commits.

## Resultado

- **XenonRecomp/XenonAnalyse compilados aquí** (`tools/build_msvc.bat`, MSVC 14.44 + `compat/msvc_compat.h`).
  `XenonRecomp/` es una copia del clon parcheado de GH (sin `build-msvc/`) más el parche nuevo de NG2.
- **C++ generado completo en `ppc/`**: 298 archivos, ~562 MB, **74.682 funciones**,
  **0 errores de switch y 0 instrucciones sin reconocer** (`python tools/run_recomp.py` → `logs/recomp9.log`).
- Las 8 secciones `.embsec_` se recorren y traducen como código normal (315 funciones). Duda de la sesión 1 resuelta.
- **Compilación**: ver el apartado final.

## Cambios a XenonRecomp

Sobre el parche de GH (`xenonrecomp_gh6.diff`), `tools/patch_recompiler_ng2.py` (idempotente) añade lo que usa NG2
(sobre todo el decodificador WMV y librerías del XDK al final de `.text` y en `.embsec_`):
- VMX de 16/8 bits: `vslh vsrh vsrah vsrab vrlh vspltish vmaxsh vminsh vavguh vaddsbs vsububm vnor`.
- Comparaciones con bit de condición: `vcmpequh(.) vcmpgtsh(.) vcmpgtsw(.)` y el `.` de `vcmpgtuh.` (cr6 vía `movemask_epi8`, 0xFFFF).
- Empaquetado: `vpkshss vpkswus vpkuhus` (operandos invertidos, igual que `vpkshus`). Float→u32: `vctuxs` (saturación, sin bit SAT).
- Escalares: `mulhd`; bits de CR: `cror`, `crorc`.
- `lvxl128`/`stvlxl128` = `lvx128`/`stvlx128` (solo cambia la pista de caché).
Pendiente de validar con casos reales: `vrlh`, `vpkuhus` y el orden de operandos de los empaquetados.

## Config (`config/ng2.toml`)

- Tablas de saltos: `config/ng2_switch_tables_all.toml` (**1.046**) = XenonAnalyse (1.023, quitando 2 mal leídas)
  + `tools/jt_finder.py` (15) + `tools/jt_nobound.py` (8, nuevo: switches al inicio de función sin `cmplwi`,
  tamaño = hasta la primera etiqueta).
  - Corregidas a mano: `0x823B5B90` (XenonAnalyse leyó r11 y 161 entradas; son r5 y 14) y `0x824210C4`
    (la entrada 19 apunta a relleno: se queda en 18).
  - `tools/jt_missing.py` deja 8 `bctr` indexados sin tabla: son saltos de cola por punteros a función (vtables y tablas
    en `.data`), que se traducen bien como llamada indirecta.
- `functions`: **174** (`tools/make_functions.py` las escribe = `config/ng2_gap_functions.txt` + manuales):
  - 164 huecos sin `.pdata` con switch (`tools/gap_functions.py`). Mejora respecto a GH: además de los destinos de `bl`,
    corta en los **destinos de punteros desde `.rdata`/`.data`** (si no, se tragaba funciones hoja de tablas de punteros).
    Para regenerarla: XenonRecomp sin esas funciones → `logs/switch_errors_all.log` → `gap_functions.py`.
  - Manuales: setjmp + 3 hojas, los dos switches de 0x823B5B90/0x823B5C50 y 3 reanudaciones `__except` del CRT
    (0x83957FA8, 0x83963BF0, 0x83964BD0; empiezan con el marcador `mr r8,r8`).
- **Ojo**: un comentario `# ...` dentro del array `functions` hace caer XenonRecomp (0xC0000409) sin mensaje. Los comentarios van encima.
- `tools/ptr_functions.py` comprueba que todo puntero de datos a un inicio de función tenga función generada:
  solo queda 0x83968398 (falso positivo en mitad de una función).

## Runtime mínimo (`runtime/`) y proyecto (`project/`)

- `runtime.h`: utilidades de memoria, `LOG`, `GUEST_IMPL`, direcciones (`kEntryPoint = 0x83818DA8`).
  La tabla de llamadas indirectas de XenonRecomp va justo tras la imagen (0x85A80000 … ~0x88963A60):
  la zona propia de cabeceras/variables empieza en **0x89000000**.
- `main.cpp`: 4 GB de invitado, copia `basefile.bin` en 0x82000000, registra las 74.682 funciones, crea las 11 variables
  importadas (a cero), pila provisional y salta a `_xstart`. Manejador de fallos con la función de invitado aproximada
  y `GuestCallIndirect` que avisa de destinos sin función.
- `import_stubs.cpp` / `var_imports.inc`: generados por `tools/gen_import_stubs.py` (227 stubs que devuelven 0).
- `atomics.cpp` (lwarx/stwcx con reserva versionada, `mftb` a 50 MHz) y `compat/ppc_indirect.h`: copiados de GH.
- `project/CMakeLists.txt`: `NG2RecompLib` (todo `ppc/`, `/O1`) + `NG2.exe`. Script `tools/build_game.bat`
  (clang-cl + lld-link del LLVM portable que está en el proyecto GH; no se ha duplicado).

## Comandos

```
tools\build_msvc.bat                                   # XenonAnalyse + XenonRecomp
python tools/patch_recompiler_ng2.py XenonRecomp/XenonRecomp/recompiler.cpp
python -I tools/make_functions.py                      # reescribe functions = [...] del TOML
python tools/run_recomp.py logs/recompN.log            # genera ppc/ y resume errores
python -I tools/gen_import_stubs.py                    # stubs de imports
tools\build_game.bat                                   # project\build\NG2.exe
```

## Compilación y primera ejecución

- **`project/build/NG2.exe` compila y enlaza** (108 MB). Los 291 trozos de `ppc/` tardan unos 3,6 min con 12 hilos
  (en GH, ~45 s con 128 MB de C++; aquí son 562 MB). Una vez compilados, cambiar el runtime solo reenlaza (segundos).
- Error de enlace resuelto: «duplicate symbol `__imp__sub_...`» = restos `ppc_recomp.291..296.cpp` de una generación
  anterior con más trozos. `tools/run_recomp.py` ahora mueve a `ppc_viejos/` los que no escribe la ejecución actual,
  y el glob de CMake usa `CONFIGURE_DEPENDS`.
- **Prueba de humo** (`NG2.exe xex\basefile.bin`, `logs/run1.log`): registra 74.682 funciones, entra en `_xstart` y el CRT
  llama a 13 imports (stubs): `NtAllocateVirtualMemory`, `KeGetCurrentProcessType`, `RtlInitializeCriticalSection`,
  `RtlEnter/LeaveCriticalSection`, `XexCheckExecutablePrivilege`, `KeTlsAlloc/SetValue`, `KeQuerySystemTime`,
  `RtlInitAnsiString`, … Como el stub de `NtAllocateVirtualMemory` no devuelve memoria, el CRT abandona con
  `HalReturnToFirmware` + `KeBugCheck` y termina en desbordamiento de pila (0xC00000FD en sub_838096C8). Es lo esperado sin kernel.

## Próximos pasos (sesión 3: runtime base)

1. Kernel mínimo a partir del de GH (`G:\Guitar Hero Warrior of rock\Recomp\runtime\kernel.cpp`, `threads.cpp`, código propio):
   asignadores de memoria virtual/física, PCR/TLS (r13), cabeceras XEX en 0x89000000 (`RtlImageXexHeaderField`),
   secciones críticas, `KeTls*`, `KeBugCheck` que pare el proceso con mensaje, variables del kernel con valores reales.
2. Hilos nativos (`ExCreateThread`), eventos y esperas; ojo a lo aprendido en GH: KTHREAD+0x14C único y
   `KeTimeStampBundle` actualizado cada ms.
3. Sistema de archivos `game:\` leyendo del ISO (`tools/xdvdfs.py` ya lo localiza) y, más adelante, los `.ng2`.
4. Compilación: valorar `/O1` → `/Od` en trozos concretos o más hilos si el ciclo de compilación molesta; solo hace falta recompilar `ppc/`
   cuando cambie XenonRecomp o el TOML.

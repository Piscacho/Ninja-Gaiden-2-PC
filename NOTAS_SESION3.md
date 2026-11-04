# Sesión 3 — Runtime base (kernel, hilos, archivos) + primeros pasos de GPU

Fecha: 2026-11-01. Sin commits.

## Resultado

- **Runtime de GH portado** a `runtime/` (copia original intacta en `runtime_gh_orig/`). Quitado lo específico de GH
  (ganchos `hooks.cpp`/`cfunc_hooks.inc`, ganchos de script en `xam.cpp`, `DumpJobState`), renombrado `GH6_`→`NG2_`,
  textos/rutas/ventana de NG2, mando = gamepad normal (subtipo 1), Title ID 544307D5 en los datos de contenido,
  `xam_ordinals.inc` regenerado con los thunks de NG2, thunks falsos de XAM en relleno de NG2 (0x83955F1C / 0x83955F3C).
  El gancho de enumeración de contenido de GH (`sub_826C5BD0`) queda en `#if 0` hasta localizar el de NG2 (sesión 8).
- **Imports**: 175 de 227 implementados (142 venían de GH). Nuevo `runtime/kernel_ng2.cpp`:
  - `sprintf`/`_snprintf`/`_vsnprintf`/`swprintf`/`_snwprintf`/`vswprintf`/`_vscwprintf` (formateador propio; varargs en r5..r10
    y luego pila r1+0x50, ranuras de 8 bytes; **ojo**: en `PPCContext` r3..r10 no son contiguos).
  - Cadenas Rtl (Unicode/ANSI, `RtlCompareStringN`, `RtlTimeFieldsToTime`), semáforos Ke, `NtSignalAndWaitForSingleObjectEx`,
    `NtSuspendThread`/`KeResumeThread`, IRQL, pool, `MmCreateKernelStack`, `NtQueryVirtualMemory`, `XeCryptSha` (SHA-1 con bcrypt).
  - `VdInitializeScalerCommandBuffer` (12 argumentos; destino y tamaño en la pila r1+0x64/0x6C; se rellena con NOPs tipo 2 y se
    devuelve el tamaño, como Xenia) y `VdPersistDisplay`.
  - `XamUserCreateAchievementEnumerator` (en `xam.cpp`): enumerador vacío. Con el stub, el juego leía un tamaño sin inicializar y pedía 1 GB.
- `main.cpp`: acepta la imagen de 0x3A78000 (la declarada es 0x3A80000).
- **GPU (gpu_cp.cpp)** — el juego reinicializa el ring buffer (D3D se suspende/reanuda para el vídeo):
  - rptr a 0 por **generación** de ring (con mutex; un lote del ring viejo no pisa el rptr nuevo).
  - `WPTR` a 0 de forma síncrona en `VdInitializeRingBuffer` (si no, la GPU ejecutaba basura del ring nuevo → escrituras
    aleatorias en registros, `SCRATCH_UMSK/ADDR` corruptos → «Unanticipated CPU_INTERRUPT» y llamada a 0x0BADF00D).
  - `WAIT_REG_MEM` con plazo por tiempo (2 s) en vez de 200000 vueltas.
  - Resultado: la GPU procesa todo (2.608 swaps, 7.823 interrupciones en 45 s) y el juego corre a ~60 fps estable, sin caídas.

## Herramientas nuevas

- `tools/run_game.py logs/runN.log [segundos] [VAR=valor...]` — ejecuta en modo desarrollo (imagen + XEX + ISO) con plazo.
- `NG2_IMPTRACE=1` (2 = también las frecuentes) — traza de imports estilo Xenia: `d> <tid> Nombre(r3, r4, r5, r6) lr=`.
- `NG2_READLOG=1` — cada `NtReadFile`. `NG2_PM4RING=1` — historial de los últimos 64 paquetes PM4 + volcado crudo
  y registros SCRATCH cuando falla un `WAIT_REG_MEM`. `NG2_HOOKLOG=1` — ganchos de `runtime/hooks_ng2.cpp`.
- `tools/callers.py SUB [niveles] [x]` — árbol de llamadores (índice desde `ppc/`), punteros en datos (sin `.pdata`) y,
  con `x`, código que carga la dirección con lis/addi.
- `tools/symsample.py` (NG2_SAMPLE) y `tools/bmp2png.py` copiados de GH.
- **Referencia Xenia**: `ref_xenia/` (copia de Xenia Canary del proyecto GH; no se toca la de GH).
  `xenia_ng2.log` = 60 s con `--log_level=3` (registra cada llamada al kernel con argumentos, salvo las de alta frecuencia:
  eventos, esperas, `XNotifyGetNext`, `KeDelayExecutionThread`...).

## Estado actual / bloqueo

Pantalla negra: el juego hace su bucle (main `sub_822F34B0`, render `sub_822F16D0` con `VdSwap` cada frame), pero **no pasa
a inicializar el sonido ni a reproducir el vídeo de intro**. Comparación con Xenia (mismos puntos del arranque):

| | Xenia | Nosotros |
|---|---|---|
| Abre `.ng2` y lee `pack_img.bin` | sí | sí |
| Hilo principal: XACT (`ng2_sound.xgs`, `stream_*.xwb/xsb`), hilo 0x824154C0, `load_img.bin` | sí | **no** |
| Hilo 0x822F16D0: `XamGetSystemVersion`, `XMsgInProcessCall` (XMP), abre `NinjaVI.wmv` | sí | **no** (solo presenta frames) |
| `ExGetXConfigSetting` / `XGetGameRegion` / `NtWriteFile` (caché) | 182 / 180 / 23 | 1 / 0 / 0 |

- La inicialización de sonido es `sub_83650E20` (usa la cadena `game:\ng2_sound.xgs`), llamada desde el método `sub_83651158`,
  que se registra como puntero en `sub_83653AF0` / `sub_8365C070` (métodos virtuales; punteros en `.data` 0x83A97528/0x83A97500).
  Con `NG2_HOOKLOG` se ve que **ninguno de ellos se ejecuta**.
- `sub_8233B6F8` no es la máquina de estados (es el reinicio por frame de un asignador).
- El hilo cargador `sub_8365D410` espera para siempre el evento 0xF8000098 (`sub_83818F90`).
- Descartado: la rama de `XamInputGetCapabilities` en `sub_8396C8C8` solo compara `XboxKrnlVersion` (< 2.0.5611; parche de vibración).
- Sospechas para la sesión 4: (1) alguna llamada de las de alta frecuencia que Xenia no registra devuelve otro valor
  (`XNotifyGetNext` con las notificaciones de arranque heredadas de GH, `NtWaitForMultipleObjectsEx` de 4 objetos en el bucle
  principal, lr 0x8380E11C); (2) algo que espera al hilo cargador; (3) un valor de configuración (`ExGetXConfigSetting(3, 0xA)`).
  Siguiente paso concreto: registrar los 4 handles y el resultado de `NtWaitForMultipleObjectsEx` del bucle principal y probar
  sin las notificaciones de arranque (`NG2_NOTIFY_FIRSTONLY`/`NG2_NODLCNOTIFY`), y comparar contra Xenia con
  `--log_high_frequency_kernel_calls` (si existe en esa versión).

## Plan revisado

Sesión 4 = desbloquear el arranque (intro WMV + XACT) y GPU; 5 = shaders/dibujo; 6 = audio; 7 = entrada y vídeo WMV;
8 = DLC y partidas; 9 = lanzador.

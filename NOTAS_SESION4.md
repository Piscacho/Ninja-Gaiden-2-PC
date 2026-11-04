# Sesión 4 — Arranque desbloqueado (fibras, APC, funciones por punteros)

Fecha: 2026-11-01. Sin commits.

## Causa del bloqueo de la sesión 3: fibras

NG2 ejecuta su lógica en **tareas sobre fibras** del XDK (cientos de funciones llaman a `SwitchToFiber`). El `SwitchToFiber` del XDK
(`sub_8380E638` → `b 0x83819200`, ensamblador a mano sin `.pdata`) guarda r14–r31/f14–f31/VMX en la fibra actual
(`[KTHREAD+0x164]`), carga los de la destino y cambia de pila con `KeSetCurrentStackPointers`. XenonRecomp lo tradujo como
`// ERROR 83819200; return`: **todos los cambios de fibra eran vacíos** y las tareas nunca corrían.

Solución: `runtime/fibers_ng2.cpp` — cada fibra del juego tiene una **fibra de Windows** con su propio `PPCContext` y pila host.
- `sub_8380E638` (SwitchToFiber) se sustituye: crea la fibra de Windows la primera vez (registros iniciales desde la estructura de
  la fibra: r1 `+0x30`, r14..r31 `+0x98`, f14..f31 `+0x1A8`; arranca en el lr `+0x1C` = 0x838191F0), actualiza `[KTHREAD+0x164]`
  y la pila de PCR/KTHREAD (`+4/+8/+0xC` de la fibra) y hace `SwitchToFiber`. Al volver, r13 = PCR del hilo actual.
- `sub_8380E5D8` (DeleteFiber) libera también la fibra de Windows. ConvertThreadToFiber (`sub_8380E418`) y CreateFiber
  (`sub_8380E4F8`, estructura de 0xA50 bytes) no necesitan cambios.
- `NG2_FIBERLOG=1` registra los cambios.

Hay otros 54 `// ERROR` en `ppc/`, casi todos en «funciones» basura que el barrido crea 8 bytes antes de funciones con `__try`
(los 8 bytes son el puntero al manejador SEH y a la tabla de ámbitos). Pendiente revisar los que caen en trozos partidos de funciones
SEH (`sub_83825698`, `sub_83866D54`, `sub_839655EC`, `sub_83965104`) y `0x828238F0` (desde `sub_82823448`).

## Otros arreglos

- **Funciones por punteros lis/addi**: `tools/lisaddi_functions.py` busca destinos de `lis/addi` en código sin función generada
  (hojas tras `blr`). 6 añadidas (`0x8386BD48` del decodificador WMV, etc.). `make_functions.py` recorta las funciones de hueco
  que las contenían (comprobando etiquetas de switch). Ahora quedan 0 candidatos.
- `tools/run_recomp.py`: los trozos viejos se detectan por índice (mayor que el del trozo de la última función del mapa), no por fecha:
  XenonRecomp no reescribe los trozos que no cambian.
- **APC de E/S** (`fs.cpp`): `NtReadFile`/`NtReadFileScatter` llaman a la rutina APC al terminar (`apc(contexto, io_status, 0)`);
  el bit bajo de la dirección es un indicador de XAPI (`0x838199D9` → `BasepIoCompletion` 0x838199D8).
- `NG2_SKIPVIDEO=1`: los `.wmv` «no existen» y el juego se salta los vídeos.
- Traza completa de Xenia: `ref_xenia/xenia_ng2_full.log` (30 s, `--log_high_frequency_kernel_calls=true`, 1,8 M líneas).

## Estado

- Con fibras + APC el juego inicializa XACT/XAudio, carga `load_img.bin`, abre los tres vídeos de la intro y arranca su decodificador
  WMV (4 hilos por vídeo).
- **Vídeo**: se queda esperando. El tercer decodificador lee `NinjaVI_Right.wmv` hasta el final (EOF legítimo) y se suspende;
  en Xenia el decodificador equivalente lee `NinjaVI.wmv` desde 0x7C0000 (el reparto vídeo↔decodificador no coincide).
  Pendiente para la sesión 7 (vídeo). De momento usar `NG2_SKIPVIDEO=1`.
- **Sin vídeo**: el juego sigue, configura la caché del disco duro y carga `cmn.ng2`, `char.ng2`, `other_rtm.ng2`, `rtm.ng2`.
  Corre a 60 fps con ~90 draws por frame y 5 shaders, pero la imagen es **gris uniforme** (cambia de tono) → sesión 5.

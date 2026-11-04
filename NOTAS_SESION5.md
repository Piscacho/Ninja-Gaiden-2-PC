# Sesión 5 — Intro y pantalla de título

Fecha: 2026-11-01. Sin commits.

## Resultado

**La intro se reproduce y aparece la pantalla de título** («NINJA GAIDEN II / PRESS START BUTTON / © TECMO, LTD. Team NINJA 2008»),
dibujada correctamente por el backend D3D12 de GH (`run/montage2.png`, `run/montage3.png`). Ya no hace falta `NG2_SKIPVIDEO`.

## Arreglos

- **`RtlNtStatusToDosError`** (`kernel.cpp`): tabla de conversión real. Antes todo error era 317; el decodificador WMV no reconocía el
  fin de archivo (`STATUS_END_OF_FILE` → `ERROR_HANDLE_EOF` = 38, Xenia lo registra como `0x26`) y abandonaba la intro.
  Con la intro terminada, el juego sigue su flujo normal hasta el título (con el vídeo omitido tomaba una ruta de error).
- **`switch` traducidos**: el `default` era `__builtin_unreachable()`; con un índice fuera de rango clang saltaba a cualquier punto del
  código host (fallo en `NG2.exe+0x5FFxxx`, bytes `00 00` ejecutados como escritura). Ahora es `PPC_SWITCH_DEFAULT` → aviso `[switch]`
  + salto indirecto real a `ctr` (`tools/patch_switch_default.py`, `compat/ppc_indirect.h`, `GuestSwitchDefault` en `kernel.cpp`).
- **Tablas de saltos recortadas** (`tools/jt_fix_counts.py`): para las tablas que van justo detrás del `bctr`, el número de entradas es
  hasta la primera etiqueta. 6 corregidas; la del menú (`0x824265E8`) tenía 5 de 23 → la entrada 8 provocaba la caída al pulsar START.
  Copia anterior: `config/ng2_switch_tables_all.antes_fix.toml`. Quedan 2 avisos de etiquetas fuera de función (entradas inalcanzables:
  una apunta a relleno `0x00000000`).
- **Llamadas indirectas** (`GuestCallIndirect`): se enmascaran los 2 bits bajos del destino (como `bctrl`).
- **Fibras**: `DeleteFiber` solo libera fibras creadas por el runtime (no la de `ConvertThreadToFiber` del hilo).
- **Disco duro**: `\Device\Harddisk0\partitionN` abierta como archivo = imagen en bruto `<datos>\Harddisk0_partitionN.img`
  (lecturas tras el final = ceros). `NtDeviceIoControlFile`: geometría (0x70000) y tamaño (0x74004) de las particiones de caché (2 GB).
- **APC de E/S**: cola por hilo; se entregan en la siguiente espera alertable (`KeDelayExecutionThread`, `Ke/NtWaitFor*`,
  `NtSignalAndWait...`) devolviendo `STATUS_USER_APC` (0xC0), como en la consola.
- Manejador de fallos: registros x64 y bytes de la instrucción.

## Herramientas

- `tools/xenia_shot.py salida.png 20,40,60` — capturas de la ventana de Xenia a esos segundos (referencia visual).
  Xenia (sin pulsar nada): 40 s = tarjeta «CHAPTER 1 – Sky City Tokyo», 60 s = ciudad (modo demostración).
- `scripts/start_game.txt` — guion de entrada: START a los 42 s, A cada pocos segundos, capturas `run/shot_NNN.bmp`.
- `ref_xenia/xenia_ng2_full.log` también sirve para comparar las lecturas una a una.

## Estado / bloqueo actual

- El título muestra «NOW LOADING…» (carga de fondo). La carga lee `other.ng2` **exactamente igual que Xenia hasta la lectura 13**
  (12 MB en 0xC7C8000) y aplica las mismas dos `MmSetAddressProtect`; Xenia sigue (borra muchas fibras = tareas terminadas, y más tarde
  lee en 0x26188000 y reabre `s_chap_01..14.ng2`), el nuestro no avanza. START no hace nada mientras carga (o la carga nunca termina).
- Las capturas alternan título / gris (comprobar si hay parpadeo real o es el momento del volcado).
- Primeros frames de la intro en negro (revisar dibujo del vídeo en la sesión 7).
- Pistas para la sesión 6: tareas (fibras) que no terminan tras la lectura 13 → registrar con `NG2_FIBERLOG` qué fibras quedan vivas
  y en qué esperan; `MmSetAddressProtect` es un no-op (Xenia cambia la protección); comprobar si alguna tarea espera a la GPU
  (texturas cargadas en memoria física 0xA8AB5000, 0xB27000 bytes).

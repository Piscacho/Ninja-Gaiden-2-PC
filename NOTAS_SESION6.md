# Sesión 6 — Del título a la partida

Fecha: 2026-11-01. Sin commits.

## Resultado

START en el título → «NOW LOADING» → **tarjeta del capítulo 1 «Sky City Tokyo»** con su texto y «Proceed» (`run/montage12.png`).
El juego reabre `s_chap_01..14.ng2` y abre `aurora12.wmv` (vídeo de apertura del capítulo), igual que Xenia.

## Arreglos

1. **Audio** (`threads.cpp`): el byte alto de `creation_flags` de `ExCreateThread` es la máscara de CPU. XAudio crea sus 2 hilos en
   las CPU 4 y 5 (`0x10000001`/`0x20000001`) y sus barreras (`sub_8374F078`) usan `KPCR+0x10C`; con CPUs por turno nunca se
   completaban y el callback de audio se quedaba esperando. Ahora el juego envía todas las tramas (`[audio] N pedidos, N enviados`).
2. **Caché del disco duro**: `XeKeysConsoleSignatureVerification` = válida y `XeKeysConsolePrivateKeySign` = firma vacía (con el stub el
   juego daba la cabecera por mala e intentaba reformatear); `\Device\Harddisk0\CacheN` (sin barra final) = disco en bruto con escritura
   (`<datos>\Harddisk0_CacheN.img`). La secuencia coincide con Xenia (`Cache1`, `Cache1\` ×2).
3. **Protección de memoria**: `MmSetAddressProtect` / `MmQueryAddressProtect` registran la protección por páginas.
4. **Perfil** (`xam.cpp`), la clave del bloqueo:
   - Con un usuario con sesión, el sistema de partidas (`sub_83661650`, objeto `0x85309728`) se quedaba ocupado para siempre tras START.
   - Ahora **sin perfil por defecto** (como Xenia; `NG2_SIGNIN=1` = antes). El juego pregunta «You are not signed in…
     Do you want to sign in?» y se responde automáticamente **«No, don't sign in»** (`NG2_MSGBOX_BUTTON=<n>` fuerza un botón).
   - **No se envían** `XN_SYS_UI` abierta/cerrada al terminar un diálogo (`NG2_UINOTIFY=1` = antes): con ellas el juego volvía a
     comprobar la sesión y repetía la pregunta sin fin.
   - Consecuencia: de momento **no se puede guardar**. Pendiente: perfil real con guardado (revisar qué espera `sub_83661650`).
5. **START**: pulsaciones de 100 ms se pierden (la lógica del título va a ~20 Hz). En los guiones, `START 1000` (mantener 1 s).

## Diagnóstico (para no repetirlo)

- Tabla de tareas del juego: 24 entradas de 0x1C en `0x83BF1C98` (marca: bit0 activa, bit1 bloqueada, bit2 arrancando; `+4` contador
  de frames; `+8` fibra; `+0xC` rutina). Planificador `sub_822F2B48`, `CreateTask` `sub_822F2CA0`, `DestroyTask` `sub_822F2DA8`.
- Gestor de escenas: tarea `sub_82444F18` → `sub_82444FC0` (petición de escena en `0x84152730`, `0xFFFF` = ninguna).
- Estado del sistema: `sub_83649B90` → `sub_83649C30(0x852E1360)` (tipo en `+0`: 1 = título, 6 = tras START; contador en `+0x10`).
- Herramientas nuevas: `NG2_DUMPMEM=dir:bytes:segundos[;...]` (volcado de memoria), `NG2_RET_HOOK` en `hooks_ng2.cpp`
  (valor devuelto cuando cambia), `NG2_FIBERLOG` con «destinos» (fibras vivas, parámetro y lr desde el que ceden).
- Guiones: `scripts/start_long.txt`, `scripts/start_menu.txt`, `scripts/play.txt`.

## Pendiente

- La escena 3D/cinemática: tras la tarjeta, pulsar A («Proceed») y comprobar el dibujo 3D (sesión 7).
- Capturas grises intermitentes durante las cargas (¿parpadeo real?).
- Perfil con guardado.

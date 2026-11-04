# Sesión 7 — Shaders con saltos y la escena 3D

Fecha: 2026-11-01. Sin commits.

## Resultado

- Tras «Proceed» ya se ven el **menú de dificultad** (Path of the Acolyte / Warrior) y la **pantalla de carga** del capítulo
  (pagoda, «NOW LOADING…») (`run/montage17.png`, `run/montage18.png`).
- Después la escena 3D sale roja oscura / blanca / negra y **la GPU se cuelga** (`DXGI_ERROR_DEVICE_HUNG` 0x887A0006) a los ~250 s
  de partida. Es el bloqueo actual.

## Arreglos

1. **Saltos condicionales en shaders** (`gpu_shader.cpp`): los shaders con `COND_JMP` (op 11 del control de flujo) se traducen como
   máquina de estados: `int pc`, `switch (pc)` dentro de un bucle con límite (`guard < 1024`), contadores de bucle por nivel
   (`lcnt`/`lstp`), `LOOP_START`/`LOOP_END` (con ruptura predicada). Antes ~1700 dibujos por frame se descartaban.
2. **Errores internos de fxc**:
   - `uint pc` → «internal error: l-value expected». Con `int pc` compila.
   - 5 PS daban «invalid access of unbound variable» en `XDot3`. Si la compilación falla se reintenta con `XDot2/3/4` = `dot()` nativo
     (solo se pierde la regla 0 × inf = 0 en esos shaders). Se registra `[d3d12] ... compilado con dot() nativo`.
3. **Lectura de vértices acotada**: el búfer de vértices va como SRV raíz (sin comprobación de límites). `XFetch[i].z` = tamaño
   copiado y el shader hace `min(índice * paso + desplazamiento, tamaño)`. Con esto desapareció un fallo de página de la GPU
   (VA 0x70956000), pero el cuelgue sigue.

## Herramientas de diagnóstico nuevas

- `NG2_DRED=1`: activa DRED (rastro de la GPU). Al perder el dispositivo vuelca las listas de comandos sin terminar y la dirección del
  fallo de página (`[dred]`).
- `NG2_NOCFSM=1`: desactiva la máquina de estados (los shaders con saltos vuelven a omitirse) para ver si el cuelgue viene de ella.
- `[d3d12] PSO fallido hr=...`: ahora con el HRESULT, los hashes y el estado (0x887A0005 = dispositivo ya perdido).
- `scripts/play_long.txt`: START + A cada 15 s con capturas.
- fxc para probar HLSL a mano: `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe /T vs_5_1 /E main /O1`
  sobre `run/hlsl_err/*.hlsl` (shaders que no compilan) o `run/hlsl_dump/` (`NG2_HLSLDUMP=1 NG2_NOSHADERCACHE=1`).

## Lo averiguado del cuelgue

- Con `NG2_NOCFSM=1` **no** hay cuelgue (`logs/run115.log`): lo provocan los dibujos que ahora sí se hacen con los shaders con saltos.
- No son bucles infinitos: en los 24 shaders con máquina de estados todos los saltos van hacia delante y no hay bucles.
- DRED (`logs/run116.log`, `logs/run117.log`): **fallo de página de la GPU** en direcciones que cambian (0x70956000, 0xC936C000,
  0xFCEB7000), sin recurso vivo ni liberado cerca, y lejos del anillo (0x09000000 y 0x0D000000, 64 MB cada uno). Las listas que
  DRED registra están todas terminadas.
- Descartado: índices relativos `C[]`/`r[]` (ya se acotan con `clamp`), texturas en VS (no hay), `XFetch` (ranuras reasignadas 0-7).
- Siguiente idea: dar nombre a los recursos (`SetName`) para que DRED diga cuál falla; revisar los caminos que solo usan estos
  dibujos (MRT `rt1`, `WriteBackResolvedForDraw`, índices muy grandes) y probar a omitir por hash VS/PS hasta aislar el dibujo.

## Pendiente

- Cuelgue de la GPU en la escena 3D (ver arriba).
- Vídeo WMV: el texto de los créditos se dibuja pero no los frames del vídeo.
- 1200 dibujos de puntos por frame (prim 1) sin soporte (CMASK=0 en los vistos, inofensivos).

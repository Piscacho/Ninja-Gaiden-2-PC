# Sesión 16 — Rendimiento

Fecha: 2026-11-03. Sin commit de esta sesión todavía.

## Resultado

- **Combate: de ~10-25 fps a 43-50 fps.** Menús, título y escenas ligeras a 60 fps; las cinemáticas a 30 (límite del juego).
  Imagen correcta (`run/montage57.png`).
- La partida avanza mucho más rápido: la cinemática de apertura termina hacia los 150 s de proceso (antes, unos 440 s).

## Medición

- Perfil estadístico de todos los hilos: `NG2_PROFILE=<segundos>:<muestras> NG2_PROFILEALL=1` y
  `python tools/profagg.py logs/runNN.log [n]` (copiado de GH y adaptado).
- Nuevo registro **`[fps]`** cada 5 s (fps medios y peor frame; `NG2_NOFPSLOG=1` lo quita).
- `NG2_CPURESOLVESTATS=1`: qué resolves van por la CPU y por qué.
- Antes, el hilo de la GPU (traducción PM4 → D3D12) estaba ocupado el 79-84 % del tiempo: 73 % en `Resolve` por la CPU
  (copia y espera a la GPU + conversión píxel a píxel con `ldexpf`). El hilo principal del juego esperaba a la GPU (espera activa
  en `sub_822F34B0`). Los hilos de XAudio giran en `sub_8374F078` (barrera); gastan núcleos pero no frenan.
- Tres resolves por CPU de 1120×584 por frame: dos de la escena HDR (RT `k_2_10_10_10_FLOAT`, emulado como R16G16B16A16_FLOAT,
  MSAA 2x) a `k_16_16_16_16_FLOAT` y uno a 8888.

## Cambios (`gpu_d3d12.cpp`)

1. `HalfToFloat` con operaciones de bits (sin `ldexpf`).
2. Vía rápida en el resolve por CPU: RT R16G16B16A16_FLOAT → `k_16_16_16_16_FLOAT` sin sesgo de exponente = copia directa de
   los 16 bits (con el intercambio R/B si toca).
3. **Resolve por GPU desde RT float a 8888** (shader de media de muestras ya existente, SRV con el formato del RT).
   `NG2_NOGPUF6=1` vuelve a la CPU.
4. Resolve por GPU a `k_16_16_16_16_FLOAT` (PSO `g_colorResolvePso16F`, lectura de texturas fmt 32/29 desde la copia de la GPU,
   volcado a memoria de 8 bytes por píxel). **Sale negro: desactivado** (`NG2_GPUF32=1` para investigarlo). Pista: las lecturas
   tienen signo o `exp_adjust` (`[writeback]` de 18A00000 y 15130000), y aun con el volcado la imagen es negra, así que la copia de
   la GPU de ese formato está mal (revisar el shader con mode.x = 4 y off.w = 2, y el estado del RT tras la copia).
5. `NG2_CPUFLOATRESOLVE=1`: todo como antes (CPU).

## Pendiente

- Arreglar el punto 4: quitaría los dos resolves por CPU que quedan y su espera a la GPU (el combate podría llegar a 60).
- Hilo de la GPU: `BindTexture` (subida y destileado de texturas) es lo siguiente en el perfil.

# Sesión 8 — Cuelgue de la GPU resuelto: primera escena 3D

Fecha: 2026-11-01. Sin commits.

## Resultado

- **La GPU ya no se cuelga** al entrar en el capítulo 1. Con 360 s de partida: 0 `Close` fallidos y 0 `DEVICE_HUNG`.
- **Se dibuja la escena 3D** (geometría de la ciudad, cinemática de apertura en el motor) (`run/montage21.png`). Aún con muchos
  fallos visuales: superficies planas o sin textura, colores lavados, algunos frames negros.

## Causa del cuelgue (la misma que en GH sesión 24)

La llave de la caché de texturas se calculaba con XOR (`(base<<20)^(fmt<<14)^(w<<32)^(h<<46)...`) y se solapaba. Dos texturas
distintas compartían recurso y se subía una textura grande a un recurso más pequeño. Eso provocaba `Close` con `E_INVALIDARG`,
se descartaba la lista y la GPU acababa colgada (`0x887A0006`). Arreglo: `TexKey` (FNV de base, formato, ancho, alto, extra y tipo)
para 2D, 3D, cubo y la memoria de hashes, copiado de GH.
**Lección: ante un fallo de GPU, buscar primero en las notas de GH** (`G:\Guitar Hero Warrior of rock\Recomp\NOTAS_SESION*.md`).

## Otros arreglos de la sesión

- **Resolve de profundidad por CPU**: copiaba una región parcial de una textura de profundidad/stencil (D3D12 solo permite copiar el
  subrecurso entero). Ahora copia el plano completo y lee desde el origen del rectángulo.
- **Subida de texturas con el anillo lleno**: la textura se quedaba en `COPY_DEST` (o recién creada sin datos). Ahora vuelve a
  `PIXEL_SHADER_RESOURCE` antes de salir. El anillo de subida pasa de 64 a 128 MB por frame (se llenaba en cada frame de la escena 3D).
- **Lecturas de vértice «mini»** (`gpu_shader.cpp`): heredan la constante de fetch de la última lectura completa, como en Xenia.
  Antes leían color y UV de otro búfer.
- **Saltos solo hacia delante**: los shaders con `COND_JMP` sin bucles se traducen como bloques `if (pc == k)` en orden, sin
  `for` ni `switch` (`NG2_CFSWITCH=1` vuelve al `switch`).
- La traza de comandos (`[d3d12]   ...` tras un `Close fallo`) guarda ahora las **últimas** 400 operaciones, no las primeras.

## Diagnóstico nuevo (todo por variable de entorno, sin coste si no se activa)

- `NG2_NOCFSM=1|vs|ps`: desactiva la máquina de estados de los shaders con saltos (todos, solo VS o solo PS).
- `NG2_SKIPSH=<hash>,<hash>`: omite los dibujos con esos VS/PS (búsqueda por mitades).
- `NG2_VSCLAMP=1`: descarta vértices con posición absurda.
- `NG2_FLUSHEACH=t<s>`: cierra la lista al inicio de cada dibujo a partir de ese segundo (aísla el dibujo que la invalida;
  antes cerraba a mitad de un dibujo y tumbaba el driver).
- `NG2_FLUSHRESOLVE=1`: cierra la lista antes y después de cada resolve.
- `NG2_PROBEBARRIER=1` y `NG2_PROBECALLS=1`: repiten cada barrera o llamada del dibujo en una lista de prueba y la cierran. Si falla,
  la registran (`[barrera]` / `[probe]`). Sustituyen a la capa de depuración de D3D12, que no está instalada (faltan las
  «Herramientas de gráficos» de Windows; no se han instalado).
- `NG2_DRED=1`: rastro de la GPU y fallo de página al perder el dispositivo.
- `Close de la lista fallo (..., t=<s>)`: ahora con el segundo del arranque.

## Pendiente

- Calidad de la imagen 3D: texturas, iluminación y posproceso (comparar con `tools/xenia_shot.py` a los mismos segundos).
- Portar también `CheckedCopy` de GH (comprueba cada `CopyTextureRegion` y omite las inválidas) como red de seguridad.
- Vídeo WMV sin imagen; dibujos de puntos (prim 1) sin soporte.
- Sesiones siguientes: mando y entrada, DLC, perfil y guardado, lanzador y distribución.

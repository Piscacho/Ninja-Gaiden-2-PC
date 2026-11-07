# Sesión 25 — Efectos, sangre, almas y pelo arreglados (un solo fallo de EDRAM)

Fecha: 2026-11-07. El usuario pidió seguir 5 horas con los efectos que faltan (sin Xenia como referencia).

## La causa

- El juego dibuja la escena en una superficie de color **formato 3** (`2_10_10_10_FLOAT`, 7e3) en la base 0 de la EDRAM y
  después, sobre la **misma base**, la pasada de transparentes y efectos con **formato 12**
  (`2_10_10_10_FLOAT_AS_16_16_16_16`). En la consola son los mismos datos (el «AS_16_16_16_16» solo cambia la precisión de la
  mezcla).
- `GetEdramRt` usaba el formato en la clave de la superficie, así que el formato 12 tenía su propia superficie, que nunca se
  resolvía a pantalla: **todo lo dibujado en esa pasada se perdía**.
- Arreglo: 12 → 3 y 10 → 2 (`2_10_10_10_AS_10_10_10_10`) antes de buscar la superficie. `NG2_SPLITAS16=1` = antes.

## Qué ha vuelto

- Sangre (charcos en el suelo, salpicaduras en las paredes, también en las cinemáticas), estelas de la espada, esferas de esencia
  (almas), brillo de la estatua de guardado y haces de luz.
- **El pelo**: Sonia tiene su melena rubia y Muramasa su pelo y barba blancos (sesiones 18-21: el pelo es una pasada de
  transparencia con el mismo formato 12). Quitado de los problemas conocidos del LEEME.
- El agua del canal de la intro del capítulo 1 se ve parecida al gameplay de 360 (ondas con reflejos). No he podido llegar a
  zonas con más agua (capítulo 2 en adelante).
- Rendimiento igual: 60 fps en combate, 30 en las cinemáticas (a propósito). Menús correctos (`scripts/menus.txt`).

## Lo que estaba mal en la sesión 24

- «Solo 8 efectos en 4 min»: el registro de `NG2_VBRANGE` se cortaba en 200 líneas y se llenaba en 4 frames. Ahora además
  resume todos los draws con vértices en el rango cada 600 frames (`[vbrsum]`). En combate hay ~200-300 draws de efectos por
  segundo (VS 4ADD4846 con PS 59D38808 / FE97356E): se creaban bien, solo que no se veían.
- Pista que lo destapó: `NG2_PSOUT` con color sólido en esos PS no pintaba nada, y `NG2_DRAWLOGEVERY` mostraba esos draws con
  `COLOR0=000C0000` frente a `00030000` de la escena.

## Otros

- `scripts/largo_fx.txt` (largo.txt + capturas cada 6 s del combate) y `scripts/cine_fx.txt` (capturas cada 4 s de la intro y la
  cinemática de la tienda).
- Las capturas de `DUMP` de menos de 100 s se llaman `shot_0NN.bmp`.
- Distribución regenerada (`tools\make_dist.bat`).

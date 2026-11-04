# Sesión 10 — Color correcto

Fecha: 2026-11-01/02. Sin commits.

## Resultado

- **Los colores coinciden con Xenia**: ciudad con resplandor rojizo, pagoda iluminada en rojo, cerezos rosados, puente rojo
  (`run/montage_color_s10.png`). Piel natural en la cinemática de la tienda (`run/montage32.png`).
- El modo demostración (sin pulsar nada) llega a la tarjeta del capítulo 1 y a la escena del río con los cerezos.

## Causa

El resolve por CPU desde un render target float (la escena HDR, `k_2_10_10_10_FLOAT` emulado como R16G16B16A16_FLOAT) a 8888
ignoraba `copy_dest_swap` (bit 24 de `RB_COPY_DEST_INFO`). El camino 8888 → 8888 sí lo aplicaba, y por eso la interfaz salía
bien y la escena 3D salía con el rojo y el azul cambiados. Arreglo: `std::swap(v[0], v[2])` antes de `PackResolvePixel` para los
formatos de 4 componentes (6, 7, 26 y 32).

## Herramientas

- `tools/montage_png.py salida.png columnas [--crop x0,y0,x1,y1] imgs...`: montaje con Pillow (PNG o BMP). Útil para las
  capturas de Xenia (`--crop 102,120,1382,840` recorta la ventana). Ojo: `xenia_shot.py` captura la pantalla, y si otra ventana
  tapa la de Xenia sale en la captura.
- `scripts/play_short.txt`: START, A cada 15 s y capturas a los 300-315 s (cinemática del capítulo 1).

## Pendiente: el pelo de Sonia no se ve

- Dibujos: VS `9D065E47AE1B06B2` + PS `BDFC3C17954A99BA` (Xenia: `shader_0568BD42B0AD8058` / `shader_0471DD6F856B4761`),
  ~10 tiras de 130-200 índices, prueba alfa GREATER 0,25 (`CC=8700000C`), mezcla `07060706`.
- Con `NG2_NOALPHATEST=1` tampoco aparece, así que el alfa llega a 0 o la mezcla lo oculta.
- En el PS, `oC0.w = r1.y * r6.w`: `r1.y` = alfa de la textura tf0 (swizzle `xwyz`) y `r6.w` = interpolador 6 (`o6 = r5` en el VS).
  Revisar `r5.w` en el VS, la carga de alfa de tf0 y el `kill_gt` / `cexec b128` (constantes booleanas 128+ de PS).
- Este PS es uno de los 5 que compilan con `dot()` nativo (error interno de fxc).

## Otros pendientes

- Partículas (prim 1, puntos), `tfetch` 1D, `CheckedCopy` y mejoras visuales de GH.
- Mando y entrada, DLC, perfil y guardado, lanzador y distribución.

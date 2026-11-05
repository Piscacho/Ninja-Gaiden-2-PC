# Sesión 18 — Partículas y pelo (investigación)

Fecha: 2026-11-03. Sin cambios de código (solo guiones de prueba). Sin commit.

## Partículas: probablemente no faltan

- Los ~1 700 dibujos de puntos por frame que se omitían (`prim=1 n=1`, VS 89F5495C / PS 5B8EA0EB) tienen **todos `CMASK=0`**
  (sin escritura de color): son **consultas de oclusión**, no partículas. Ya se responden con 1 000 muestras visibles
  (`EVENT_WRITE_ZPD`, `NG2_ZPDSAMPLES`).
- En combate no se omite ningún otro dibujo (solo copias de EDRAM con `MODE=6`). No hay `memexport` en los shaders volcados por
  Xenia. No hay texturas sin cargar. Ningún dibujo usa factores de mezcla constantes (el runtime no llama a `OMSetBlendFactor`;
  pendiente si algún juego/escena lo usa).
- Falta confirmarlo a ojo con más resolución (las capturas `shot_*.bmp` son de 640×360). `DUMP8` en los guiones vuelca 8 frames
  seguidos (`run/seq_*.bmp`).

## Pelo de Sonia: sin resolver

- La «cáscara» beige de la cabeza la dibuja el **shader general de personajes, PS 985C50752EBCBC61** (el mismo de la cara, el
  cuerpo y la ropa de Muramasa). Uno de los 5 que compilan con `dot()` nativo. En esa zona:
  - el alfa de `tf0` (DXT1) vale 1 y `b129` está desactivado: el juego no pide recortar (el recorte con `kill_gt` existe en el
    shader: `r2.w = alfa − c9.x`);
  - el color de `tf0` ya es beige liso (no son los mips: con `NG2_NOMIPS=1` igual).
- Hipótesis: la cáscara es la base del pelo y los **mechones van en otra malla** que no se ve. **No son** los dibujos
  VS 9D065E47 + PS BDFC3C17 (pintados de rojo resultan ser jarrones y cerámica).
- Siguiente: pintar por grupos el resto de PS del plano (`NG2_PSOUT=<hash>:float4(...)`, varios separados por `;`) y mirar los
  dibujos que no se ven en pantalla. Comparar con Xenia en la misma escena: hace falta mandarle entrada para llegar a la
  cinemática.

## Herramientas y trucos de esta sesión

- `NG2_PSOUT=<hash PS>:<expresion HLSL>[;...]`: sustituye la salida de ese PS. Ejemplos útiles:
  - `float4(1,0,0,1)` para identificar quién dibuja qué;
  - `float4(XTex[0].Sample(XSamp, i.t[0].xy).xyz, 1)` para ver la textura;
  - `float4(XBool(129) ? 1.0 : 0.0, 0, 0, 1)` para ver una constante booleana.
- `NG2_DUMPVS=<hash VS>`: registra el frame en que aparece ese VS (`NG2_DUMPVSMIN` cuenta desde el primer dibujo, no desde el
  arranque).
- Guiones: `scripts/sonia.txt` y `sonia2.txt` (capturas de la cinemática con Sonia a los 116, 140 y 148 s),
  `scripts/combate_rafaga.txt`.
- **El autoguardado de capítulo funciona**: las pruebas de combate crearon `userdata\content\00000001_ng2stryd20.dat`. Con una
  partida de capítulo guardada, el menú cambia y los guiones dejan de ser repetibles: se movió a
  `run\userdata\respaldo_pruebas\`. Moverla otra vez si reaparece.

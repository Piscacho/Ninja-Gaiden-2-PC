# Sesión 9 — La escena 3D se ve bien

Fecha: 2026-11-01. Sin commits.

## Resultado

- **Intro en el motor** (vuelo sobre Sky City Tokyo con los créditos): pagodas, cerezos, edificios iluminados y helicópteros, todo
  con su forma y sus texturas (`run/montage_intro_s9.png`).
- **Cinemática del capítulo 1** (tienda de Muramasa, Sonia, ninjas de la Araña Negra): personajes y escenario texturizados, con
  profundidad de campo (`run/montage30.png`). Sin cuelgues ni fallos en 360 s.
- Lo que falta: tono de color (la piel sale pálida o azulada, el cielo gris oscuro en vez del resplandor rojizo de Xenia).

## Arreglos

1. **Reinicio de primitiva** (`gpu_d3d12.cpp`): con `PA_SU_SC_MODE_CNTL` bit 21 (`MULTI_PRIM_IB_ENA`), el índice
   `VGT_MULTI_PRIM_IB_RESET_INDX` (0x2103) corta la tira. Pasa a 0xFFFFFFFF y los PSO usan `IBStripCutValue = 0xFFFFFFFF`.
   Antes se tomaba como el vértice 65535 y todo el escenario salía con púas y planos gigantes. Era la causa principal.
   `NG2_NOPRIMRESET=1` = antes.
2. **Formatos de textura `_EXPAND`** (27, 28 y 29): se suben como half float, igual que los `_FLOAT` (como Xenia). NG2 lee así el
   búfer HDR de la escena (1120×584, formato 29); antes no estaba soportado.
3. **Opcodes 13/14 de control de flujo** (`cond_exec_pred_clean`): dependen de una constante booleana, como 3/4, no del
   predicado (como en Xenia).

## Herramientas y método

- **Shaders de Xenia como referencia**: `dump_shaders` en `ref_xenia/xenia-canary.config.toml` o
  `xenia_canary.exe --dump_shaders=<dir> <iso>` vuelca la microcódigo y el desensamblado (`shader_<hash>.ucode.vert/.frag`) en
  `ref_xenia/shaders_dump/` (360 shaders en 70 s). Para emparejarlos con los nuestros: FNV-1a de los dwords del `.ucode.bin`
  (little endian), con el bit 63 para los PS. Ejemplo: nuestro VS 8FF8D96C3B465928 = `shader_BE91B5F6EF2F8E88`.
- `tools/xenia_shot.py` da PNG (el montaje solo acepta BMP; las capturas de Xenia se miran sueltas).
- Guiones: `scripts/idle_demo3.txt` (sin pulsar nada, capturas de 100 a 205 s = intro en el motor).
  Ojo: si alguien usa el mando o el teclado durante la prueba, el juego recibe esa entrada.
- Comparado el runtime de GH actual con nuestra copia: lo que GH añadió después es sobre todo mejora visual (FXAA, nitidez CAS,
  caché de PSO en disco, contador de FPS) y `CheckedCopy` (comprobación de copias de textura). Pendiente de traer.

## Pendiente

- Tono de color e iluminación (piel, cielo, niebla); comparar con Xenia en la misma toma.
- Portar `CheckedCopy` y, si interesa, el FXAA, la nitidez y la caché de PSO de GH.
- Dibujos de puntos (prim 1, ~1.500 por frame): partículas sin soporte.
- `tfetch` 1D leído como 2D (8 shaders).
- Mando y entrada, DLC, perfil y guardado, lanzador y distribución.

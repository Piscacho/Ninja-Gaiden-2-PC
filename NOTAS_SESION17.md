# Sesión 17 — 60 fps

Fecha: 2026-11-03. Sin commit de esta sesión todavía.

## Resultado

- **Casi siempre a 60 fps** en `scripts/combate.txt` (300 s: menús, cinemática y combate), con caídas breves a 44-55 fps en el
  combate. Antes de la sesión 16 iba a 10-25 fps en combate; tras la 16, a 43-50.
- Imagen correcta en el combate (`run/montage62.png`).

## Arreglo: resolve de la escena HDR por GPU (`k_16_16_16_16_FLOAT`)

Ya está activo por defecto. `NG2_NOGPUF32=1` lo devuelve a la CPU; `NG2_NOGPUF6=1` hace lo mismo con el de 8888 desde float.
Por qué salía negro:
1. Las lecturas de esas texturas llevan marcas de signo o `exp_adjust`, y el camino de GPU las rechazaba. Pero para half float la
   subida desde memoria las ignora, así que la copia de la GPU también debe ignorarlas.
2. **La causa del negro**: 0x03EB0000 se lee como 280×146 sobre una copia de 281×146. Eso pasaba por la «copia de región
   derivada», que no funciona con 8 bytes por píxel. Ahora, para el formato 32, solo se usa la copia de la GPU con el tamaño
   exacto. Si no coincide, se vuelca a memoria (`WriteBackResolved`, ahora con 8 bytes por píxel) y se sube desde ahí.
3. `WriteBackResolved` crea el búfer de lectura si no existe (antes solo lo creaba el resolve por CPU y el volcado no hacía nada).

Variable de diagnóstico `NG2_WB32=1`: fuerza el volcado a memoria de las copias de formato 32. Sirvió para ver que la copia de la
GPU estaba bien y que el fallo estaba al leerla.

## Método

- Comparar el resultado con y sin cada camino (`NG2_NOGPUF32`, `NG2_NOGPUF6`, `NG2_WB32`) en `scripts/cine_corta.txt`
  (capturas a los 100 y 120 s).
- Registrar las búsquedas de copia por dirección y tamaño hasta encontrar la que no coincidía.
- Ojo: alguna prueba se queda parada al arrancar (registro de ~75 líneas). Repetirla.

## Pendiente

- La copia de región derivada con 8 bytes por píxel (el caso de 281 → 280), para no volcar a memoria.
- Las caídas a 44-55 fps en el combate (perfilar de nuevo con `NG2_PROFILE`).
- Pelo de Sonia, partículas, autoguardado de capítulo.

# Sesión 24 — Efectos que faltan (comparación con un gameplay de Xbox 360)

Fecha: 2026-11-07. Referencia: vídeo de YouTube «Ninja Gaiden II Full Xbox 360 gameplay» (9,6 h, 1080p), fotogramas sacados con
el navegador integrado (canvas sobre la página). Sin arreglo todavía.

## Qué falta (visto por el usuario y en la comparación)

- **Sangre**: en la 360 el suelo y las paredes se llenan de charcos y salpicaduras en cada combate; aquí no hay ni una gota.
- **Almas** (orbes de esencia que sueltan los enemigos), **agua** y **pelo** (sesiones 20-21).
- Destellos de las técnicas (la explosión blanca de la técnica definitiva).

## Lo descartado

- No es censura por región: devolvemos idioma inglés, país 103 (EE. UU.) y región «todas».
- No se omiten draws con color: los `prim 1` omitidos tienen todos CMASK=0 (consultas de oclusión) y los demás «omitidos» son
  copias de EDRAM (MODE=6).
- No hay memexport: el traductor avisa ahora de exportaciones descartadas (`[shader] exportacion descartada`) y en todo el combate
  no sale ninguna.
- Los archivos del juego se abren todos bien (78 aperturas, los efectos van dentro de los .ng2).
- Pintados con `NG2_PSOUT` en combate todos los draws con mezcla de los frames registrados (`NG2_DRAWLOGEVERY=150:12`): son la
  interfaz, cristales, cielo y brillos del escenario. **Ningún draw corresponde a sangre, almas o salpicaduras**: el juego no los emite.

## Conclusión y siguiente paso

El sistema de efectos/partículas del juego no genera partículas (fallo del lado de la CPU, no del render). Siguiente:
1. Localizar el gestor de efectos (cadenas `effect\effpk_*`, carga de los paquetes de efectos) y ver si las partículas se crean.
2. Mirar si se queda sin un recurso (tiempo, aleatorio, memoria de un pool propio, un hilo que no arranca) o si una instrucción
   mal traducida deja tamaño/alfa a 0.

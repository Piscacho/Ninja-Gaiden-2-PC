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

## Segunda parte (sin Xenia: había otra sesión con Guitar Hero abierto)

Descartado además:
- Reloj `mftb`: devuelve 50 MHz como la consola (`atomics.cpp`).
- Las 24 instrucciones VMX/CR añadidas en la sesión 2 (`tools/patch_recompiler_ng2.py`): revisadas una a una (orden de
  elementos en los empaquetados, operandos de restas y comparaciones, saturaciones): correctas. Las recompilaciones recientes no
  tienen instrucciones sin reconocer.
- Predicado de los paquetes PM4: se ignora (se ejecutan todos); vallas/escrituras de la GPU (`EVENT_WRITE_SHD`, `MEM_WRITE`,
  `REG_TO_MEM`) como en Xenia.
- Consultas de oclusión: con `NG2_ZPDSAMPLES=100000` tampoco aparece la sangre.
- Shaders: ninguno falla al traducirse ni usa memexport; ningún ajuste de XConfig desconocido; privilegios = permitido.
- Resolución de nombres de recursos (`sub_8365D730`, tabla fija de 5099 nombres en 0x83AEBC98): todo resuelto salvo
  `mission.tdp` (Mission Mode).

Encontrado:
- La tabla de rutas de efectos está en 0x83AF0774 (308 entradas, dentro de la de 5099 nombres); `effpk_common` = índice 4791.
- Se carga en `0x82443498`: `sub_822FF5A8(4791, 0x83BBC1C4)` (¿ya cargado?) y si no, `sub_823D6F00(4791, 0xA54C6000,
  0xA5E49000)` (petición 0x1B). **El paquete llega**: el búfer empieza por `GMD\0` y tiene datos (`NG2_EFFLOG=1`).
- Algunos efectos sí se ven (pétalos de cerezo); faltan los de golpes y muertes (sangre, almas) y el agua.
- En combate nadie lee la cabecera del paquete común ni el puntero `0x83BBC1C4` (solo durante la carga).

Siguiente (con Xenia y sin otra ventana delante):
1. Llevar Xenia al primer combate con `--dump_shaders` y comparar con los nuestros: los shaders que solo use Xenia serán los de los
   efectos que faltan, y con su hash se puede buscar quién los dibuja.
2. Ver si los pétalos salen del mismo sistema de efectos (si sí, el fallo está en el disparo de los efectos por golpes).

Diagnósticos nuevos: `NG2_EFFLOG=1` (ganchos en `sub_8365D730`, `sub_822FF5A8`, `sub_823D6F00` y estado del búfer cada minuto),
`tools/xref.py` (código que construye una dirección con `lis`+`addi`).

## Tercera parte

- Los pétalos de cerezo son partículas del mismo tipo (VS D2AE0887 + PS BDBE927E, tiras con búferes que la CPU rellena cada
  frame) y sí se dibujan, también en combate: **el renderizador de partículas funciona; fallan los efectos que disparan los
  golpes y las muertes** (sangre, almas, destellos).
- Hipótesis principal: esos efectos se buscan por un identificador calculado (p. ej. un hash del nombre guardado en
  `ng2_hit_data.dat`) que no coincide con el que calculamos, mientras que los del escenario se piden por índice. Siguiente: localizar
  la búsqueda de efectos por golpe (llamadas desde el código de daño/muerte) y comprobar si encuentra el efecto.
- La estructura 0x83BBC090 es un gestor global de recursos (409 referencias): no sirve para llegar rápido al disparador.

## Cuarta parte

- `ng2_hit_data.dat` se carga en 0xA549C000 (0xD600 bytes; `NG2_READLOG=1` ahora registra el destino de cada lectura). En
  combate lo lee el código de golpes: `sub_82417060` (← `sub_82416DD8` ← `sub_82416B80` ← `sub_823778F8`, actualización del
  personaje) y sus llamadas `sub_82418130` / `sub_82418008`.
- `sub_82418130` llama a `sub_8370AB48(indice)`, que busca el bloque de tipo 33 de un recurso en la tabla de *(0x83BBC090 + 112)
  (326 entradas). **Nunca falla** (10 001 llamadas en combate, 0 nulas): descartado.
- Siguiente: seguir desde `sub_82417060` hacia lo que hace con un golpe acertado (¿llama a un generador de efectos?), y con Xenia,
  volcar los shaders de un combate para identificar los de sangre/almas.

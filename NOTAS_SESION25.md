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

## Segunda parte: tirones y Mission Mode

### Tirones

- Perfilador nuevo `NG2_HITCHPROF=<ms>` (`diag.cpp`): muestrea todos los hilos cada ~1-2 ms en un anillo y, si un frame tarda más
  de `<ms>`, vuelca las muestras de ese intervalo (las esperas, marcadas con `W`, 1 de cada 8). Resumen por tirón:
  `python tools\hitchagg.py logs\X.log`.
- **Cortes de escena**: ~60 PSO nuevos de golpe (35-50 ms). Ahora cada PSO nuevo se apunta en `shadercache/psolist.bin` y en el
  siguiente arranque, después de precompilar los shaders, se crean todos en segundo plano (168 PSO, al instante). Solo se usa
  uno si el bytecode de sus shaders coincide byte a byte. Peor corte: **92 → 60 ms** (queda la subida de 535 texturas, ~26 ms).
  `NG2_NOPSOPREWARM=1` = antes.
- **Tirones de ~120 ms al cargar zonas**: el hilo principal del juego en `sub_82385A00` (búsqueda de ruta en un grafo de
  ≤256 nodos), llamada ~40 000 veces a ~3 µs por el precálculo de la tabla de rutas de la zona (`sub_82386E48`). Es trabajo
  del propio juego (en la 360 sería más lento) y no es una función pura (escribe la ruta en un búfer y depende del grafo): no
  se toca. `NG2_PATHLOG=1` mide sus llamadas.
- `sub_83737AE0` es la espera activa del D3D del juego (bucle `db16cyc`) hasta que la GPU avanza.

### Mission Mode

- No se desbloquea al terminar el juego: era DLC de pago (800 MSP, julio de 2008) que salió junto con una actualización del
  título. El paquete (`dlc/00000002_f591…`) trae `battle\s_missionNN_nml.btl`, `actsel\g_mission.grp` y archivos base
  sustituidos (`char\cmn\cmn.mtn`…), pero el menú y el código que los carga están en el XEX de la actualización.
- El XEX base sí conoce los nombres de los combates (`battle\s_missionNN_*.btl`), `mission.tdp` y los marcadores «MISSION
  01-25», pero no el menú ni `cmn\mes_mission.bin`. Para jugarlo haría falta la actualización oficial (su `default.xex`),
  sacada de la consola del usuario, y recompilarla.

## Tercera parte: audio, contador, launcher y panorámico

### El audio se cortaba (arreglado)

- Síntoma: a los 1-8 minutos de partida se dejaba de oír todo (música, voces y efectos); el juego seguía a 60 fps. En el registro
  dejan de salir las líneas `[wasapi] t=` y `[xma]` se queda fijo.
- Causa: la barrera de los dos hilos de XAudio del juego (`sub_8374F078`, hilos 8374E740/8374E808 en las CPU 4 y 5). Cada hilo pone
  a 1 el byte de su CPU (KPCR+0x10C) en una palabra de 64 bits y espera a que estén todos los de la máscara; el que la ve completa
  la pone a 0. Si ese hilo vuelve a entrar en la ronda siguiente antes de que el otro vea el 0 (en el PC, con el otro hilo
  desplanificado unos ms), los dos esperan para siempre y la función de render de audio (`sub_8374EDE8`, en nuestro `AudioPump`)
  se queda en `KeWaitForMultipleObjects` sin volver.
- Arreglo (`hooks_ng2.cpp`): barrera con contador de generación en el host; la palabra del juego se sigue actualizando.
  Dos partidas de 8 min con sonido hasta el final (antes 10-95 líneas de `[wasapi]` según la suerte). `NG2_OLDAUDIOBARRIER=1` = antes.

### Contador en pantalla (F3)

- Como el de Guitar Hero con más datos: FPS, ms por frame (media, máximo y 1% más lento), CPU del proceso, draws por frame,
  tirones (>40 ms), VRAM usada/presupuesto, RAM, resolución interna → ventana y cola de audio. `mostrar_fps=1` / `NG2_FPS=1`.

### Launcher

- `runtime/launcher_ui.cpp`, adaptado del de Guitar Hero (mismo aspecto, colores de Ninja Gaiden). Páginas Inicio, Juego,
  Gráficos, Controles, Audio y Ayuda. `portada.png` y `logo.png` junto al exe (de SteamGridDB: hero 2b763a9f… y logo cffa2aed…;
  no se guardan en git). `--jugar` lo salta, `--launcher` o Mayús lo fuerzan; `mostrar_launcher=0` en ng2.ini.
- ng2.ini nuevo: `saltar_intro`, `grano_pelicula`, `desenfoque_movimiento`, `vsync`, `escalado`, `latencia_audio`
  (baja/normal/segura = 25/40/80 ms), `pantalla_completa`, `ventana=WxH`, `panoramico`, `mostrar_launcher`. `SaveConfig` ya no
  borra el resto del ini.

### Panorámico (21:9 y 32:9)

- El juego elige la proporción de la cámara entre 16/9 (0x8228925C) y 4/3 (0x822817DC). El frustum (`sub_837CC920`) usa
  tanX = tan(campo × proporción / 2) y tanY = tanX / proporción: con proporciones mayores el ángulo horizontal crece en lineal
  (a 32:9 casi 180°). Cambiar esa constante no da un Hor+ correcto y solo afecta a algunas cámaras.
- Solución: la cámara del juego se queda en 16:9 y el Hor+ se hace en el VS: en las superficies proporcionales a la pantalla
  (pitch 1120/560/280/1280/640/320) los vértices en perspectiva se comprimen hacia el centro (x / kx, kx = proporción / (16/9),
  en `XVte.w` del VS) y al presentar la imagen se estira a la proporción elegida. La interfaz 2D (superficie final 1280 en 8888,
  w = 1) pasa por un GS que comprime hacia el centro los triángulos que no van de borde a borde.
- Limitación: el juego sigue descartando objetos con su campo de 16:9, así que en los bordes puede faltar geometría; la imagen
  interna sigue siendo de 1280x720 (se estira a lo ancho).

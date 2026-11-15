# Sesión 26 — Actualización del título #2 (Mission Mode): parche aplicado

Fecha: 2026-11-07. El usuario aportó la TU oficial en `Base\Update\TU_1A461UL_000000G000000.00000000000G4` (paquete LIVE,
tipo 000B0000, «Ninja Gaiden 2 Title Update #2», título 544307D5, media ID 69F555CB = el del disco). Mission Mode es un DLC de pago
(el paquete f591… ya está en `dlc/`) que necesita esta actualización.

## Aplicar el parche (hecho)

- `tools/stfs_extract.py` → `tu/…/default.xexp` (2,3 MB, XEX2 con flags 0x50 = parche delta, 2 cabeceras: 0x3FF y 0x5FF).
- Descriptor delta (0x5FF): versión 0x4 → 0x204, cabeceras nuevas de 0x7000 bytes a partir de las viejas (0x7000), imagen
  0x3A80000 → 0x3A90000. Las cabeceras van en un registro LZX dentro del propio descriptor.
- Registro delta: `old_addr(4) new_addr(4) uncompressed_len(2) compressed_len(2)` + datos; compressed_len 0 = ceros, 1 = copia
  de old, si no LZX con la ventana precargada con los `uncompressed_len` bytes de old (datos de referencia).
- `tools/lzxdelta.cpp` (propio, según la especificación pública de LZX: bloques verbatim/aligned/sin comprimir, árbol previo,
  3 desplazamientos repetidos, tramas de 32 KB con realineado): aplica los registros. Compilar con clang-cl.
- Claves: la clave de sesión del ejecutable nuevo sale de las cabeceras ya parcheadas (clave AES de su información de seguridad,
  descifrada con la clave retail). Los datos del parche se descifran con la clave AES del parche **descifrada con esa clave nueva**
  (no con la retail). Comprobación: `image_key_source` del descriptor descifrada con la clave nueva = clave de sesión del original.
- Datos del parche: bloques encadenados [tamaño del siguiente(4) + SHA-1 del siguiente(20)] + registros (no cruzan bloques; el
  bloque termina con relleno de ceros). 36 bloques, SHA-1 correctos en todos. 1109 registros (13 ceros, 268 copias, 828 LZX).
- `tools/xexp_records.py default.xexp tu/work` (necesita antes `tu/work/new_hdr.bin`: `lzxdelta hdr_records.bin 0x8000
  base_hdr.bin 0x7000 new_hdr.bin`, con base_hdr = los primeros 0x7000 bytes del default.xex del disco) y luego
  `lzxdelta img_records.bin 0x8000 basefile.bin 0x3A90000 tu_basefile.bin`.
- Resultado `tu/work/tu_basefile.bin`: punto de entrada 0x8383E490 (prólogo `mflr r12`), contiene los archivos de Mission Mode
  (`sprite\ranking_mission\…`, `replay_mission_thumbnail…`) que el original no tiene.
- Los hashes de página no los sé validar aún (mi cálculo falla en las mismas páginas del original y de la TU: 454/455).

## Lo que cambia

- 42 504 → 42 773 funciones (.pdata); solo 383 siguen en la misma dirección. `.text` crece 0x26000 y cambia desde 0x822F0004;
  `.data` pasa de 0x1A70000 a 0x1A90000. Todo el código se ha vuelto a enlazar.

## Siguiente

1. Recompilar `tu_basefile.bin` (XenonAnalyse para los switches, XenonRecomp con un TOML propio, funciones manuales, tablas de
   saltos) como un ejecutable aparte.
2. Mapa de funciones original → TU por similitud de instrucciones (ignorando desplazamientos de saltos e inmediatos de
   direcciones) para trasladar todos los ganchos de `hooks_ng2.cpp`, la barrera de audio, el guardado, los efectos, las constantes
   (16/9, etc.) y las direcciones de datos.
3. Que el runtime cargue la TU (default.xexp junto al ISO o en `dlc/`) y probar Mission Mode.

## Segunda parte: recompilación de la TU

- `tu/tu_default.xex` (`tools/make_tu_xex.py`): cabeceras nuevas con formato sin cifrar/sin comprimir + imagen parcheada.
  **Idéntico byte a byte** al que genera el parcheador de XenonRecomp (`patch_file_path`), así que el parche está bien aplicado.
- `tools/tu_map.py` → `tu/funcmap.txt` (41 750 de 42 504 funciones emparejadas: 22 097 por firma única, 6 662 por orden entre
  anclas, 12 991 por parecido ≥ 0,8 cerca de la posición prevista), `tu/callmap.txt` (destinos de `bl` en funciones emparejadas
  del mismo tamaño: resuelve también funciones sin .pdata) y `tu/datamap.txt` (pares lis+addi/lwz…).
- Ayudantes de la TU (`find_helpers.py`): savegprlr 0x83979A80, restgprlr 0x83979AD0, savefpr 0x8397B030, restfpr 0x8397B07C,
  savevmx 0x8397C730/64 0x8397C7C4, restvmx 0x8397C9C8/64 0x8397CA5C; setjmp 0x8397B4A0, longjmp 0x8397B180.
- Tablas de saltos (`tools/tu_switches.py`): las 1046 corregidas del original se trasladan si su bloque y sus etiquetas aparecen
  en la TU (351; las etiquetas se buscan a ±0x200 y solo se aceptan si XenonAnalyse ve el switch ahí o todas quedan cerca: había
  funciones gemelas y se colaban etiquetas de la otra) + XenonAnalyse sobre la TU para el resto (713). `jt_fix_counts` (2 ampliadas)
  y `jt_check_regs` (0 con el registro mal) → `config/ng2_tu_switch_tables_fixed.toml` (1064).
- Funciones sin .pdata: las manuales del original trasladadas por contenido (`tools/tu_functions.py`; las 3 de `__except` del CRT
  con 17 copias se eligieron por contexto) o por el mapa de datos (hojas por punteros, WMV 0x8386BD48 → 0x83891448) + huecos con
  switch de la TU (`gap_functions.py` sobre el log de una primera pasada; ahora toma los rangos de .rdata/.data de la imagen).
  `tools/make_tu_toml.py` → `config/ng2_tu.toml` (183 funciones). Tercera pasada de XenonRecomp: **0 errores** → `ppc_tu/`.
- Runtime: `runtime/tu_remap.h` (`tools/make_tu_remap.py`, 20 funciones; p. ej. barrera de audio 0x8374F078 → 0x83774818,
  guardado 0x83661650 → 0x836843A8, SwitchToFiber 0x8380E638 → 0x83833D20) y `NG2ADDR(original, tu)` para datos (usuario 0
  0x83B271B0 → 0x83B3D020, gestor de partidas 0x85309728 → 0x85324120, arranque de fibra 0x838191F0 → 0x8383E8D0). Los ganchos
  de diagnóstico sub_8365D730 / sub_8370AB48 no se compilan en la TU.
- `tools/build_game_tu.bat` → `project/build_tu/NG2TU.exe` (CMake `-DNG2_TU=ON`).

## Tercera parte: NG2TU.exe arranca y **Mission Mode funciona**

- Menú principal con la opción nueva **MISSIONS**; se entra, carga la misión desde el DLC (`dlc:\cmn\mes_mission.bin`,
  `dlc:\battle\s_mission04_nml.btl`…) y se juega (combate, efectos, sangre, ninpo, contador de golpes y Karma Burst).
  Guion `scripts/misiones.txt`; `python tools/run_game.py logs/X.log 140 RUN_TU=1 NG2_INPUTSCRIPT=../scripts/misiones.txt`.
- Lo que hubo que arreglar por el camino (todo en `tools/tu_rebuild.sh`, que repite la cadena completa):
  - Trozos viejos de `ppc_tu/` de pasadas anteriores → símbolos duplicados: el script borra `ppc_tu/` antes de cada pasada.
  - Ganchos de diagnóstico con macros (`NG2_TRACE_HOOK`, `NG2_RET_HOOK`, `NG2_DRIVER_HOOK`, `NG2_TYPEWALK_HOOK`) con direcciones
    del original: fuera de la TU con `#ifndef NG2_TU`.
  - Tablas sin trasladar: el patrón de la base ahora llega solo hasta el `bctr` (las tablas pegadas detrás llevan direcciones
    absolutas) y, si las etiquetas no cuadran, se leen de la tabla de la propia TU. 1028 de 1046 trasladadas.
  - `jt_finder.py` (ahora con `JT_IMG`/`JT_TOML`/`JT_PDATA`) y `jt_nobound.py` sobre la TU: 3 + 1 tablas que faltaban
    (p. ej. 0x836C7EB8 sin comprobación de rango).
  - `tools/tu_fix_tables.py`: la tabla del intérprete de misiones (0x823F5F24) tiene 91 casos; el 84 salta al trozo de cola
    0x823F82D0 (otra función en .pdata, como en el original) y XenonAnalyse la cortaba en 84: los casos 85-90 (los de las
    misiones) caían en el default.
- Quedan 2 avisos de XenonRecomp que también tiene el original (entradas que saltan al final o al trozo de cola).
- La versión normal (`NG2.exe`) sigue compilando y arrancando igual.

## Pendiente

1. Modo usuario: que el launcher aplique la TU por su cuenta (portar `lzxdelta.cpp` y la lógica de claves de
   `xexp_records.py` a C++) a partir del ISO + `default.xexp` del usuario, y elegir NG2TU.exe cuando esté la TU; distribución.
2. Probar más misiones y el modo historia completo con la TU (guardado, DLC de trajes, efectos, audio, panorámico).

## Cuarta parte: la actualización en modo usuario y en la distribución

- `runtime/xexp_patch.cpp` (`XexpApply`): lo mismo que `lzxdelta.cpp` + `xexp_records.py`, en C++ con BCrypt (AES y SHA-1 de
  cada bloque). Comprueba la versión del disco (0x4) y la clave (`image_key_source`).
- Arranque (`main.cpp`, modo usuario): `ImportDlc` va ahora antes de leer el XEX. El paquete de la TU se deja en `dlc/` como el
  DLC y se importa a `datos\content\000b0000_*\default.xexp` (`FindTitleUpdate`, `launcher.cpp`).
  - `NG2TU.exe`: aplica la TU en memoria al `default.xex` del ISO (~1 s) y sigue; sin TU avisa y sale.
  - `NG2.exe`: si la TU está instalada, `actualizacion=1` (por defecto; `NG2_NOTU=1` la ignora) y `NG2TU.exe` está al lado,
    abre `NG2TU.exe --jugar` y termina (su ng2.log lo escribe NG2TU.exe).
- Launcher: página Juego, «Actualización del título #2 (Mission Mode)» con su estado. LEEME con las instrucciones.
- `make_dist.bat` copia `NG2TU.exe`. Prueba desde una carpeta nueva con los 4 DLC y la TU originales en `dlc/`: importa todo,
  aplica la TU (36 bloques, 1109 registros) y entra en una misión.

## Sesión 27 (2026-11-08): tirones del primer arranque, launcher con listas y panorámico mejor

- Tirones de la partida del usuario (dist nueva, 32:9 a 5120x1440): 56, mediana 99 ms, máximo 1,3 s; casi todos **PSO**
  (29 PSO = 1245 ms en un corte; 40-200 ms cada uno sin caché del controlador). Ahora los PSO se crean en un grupo de hilos propio
  (`EnqueuePso`): el draw se omite ese frame y toma el PSO ya hecho en cuanto está. Sin ninguna caché (`NG2_NOSHADERCACHE=1`):
  9 tirones, máximo 196 ms, 0 ms de PSO. `NG2_SYNCPSO=1` = antes.
- Launcher: al pulsar en el centro de un selector se despliega la lista de opciones (menú con tema oscuro, marca en la actual);
  las flechas siguen pasando de una en una; Intro/Espacio/F4/abajo también la abren.
- Panorámico: solo en los frames con escena 3D (draws con prueba de profundidad en la superficie de la escena, pitch 1120; margen
  de 30 frames). Vídeos, menús y pantallas de carga se presentan a 16:9 con bandas negras. El GS de la interfaz también actúa en la
  superficie de 1120 (el HUD se dibuja ahí) y con las listas de quads expandidas (antes la barra de vida no se comprimía).
- Panorámico nítido (como GH): las superficies de pitch 1120/560/280/1280/640/320 se crean S veces más anchas (S = kx redondeado
  hacia arriba: 2 en 21:9 y 32:9 → 2560x720 internos). Viewport, recortes, borrados, resolves y copias de profundidad en píxeles
  de superficie; ResolvedTex/ResolvedDepth.sx (regiones derivadas, volcado a memoria y capturas toman una columna de cada sx);
  lecturas en texeles con la escala en XTexMode.z bits 8-15 (x16); PsParamGen en X sin redondear (bit 6 de XAlpha.w; bit 7 =
  X - 0.5 con NG2_PGENHALF=1); color→profundidad con columnas de 40·S. El HLSL solo cambia con S > 1 (16:9 conserva la caché).
  NG2_PANOSTRETCH=1 = antes.
- Causa de los laterales "estirados" que vio el usuario: el juego compone la escena HDR en 1120 con una rejilla de 16 quads y el GS
  de interfaz (activado en 1120 para el HUD) comprimía los del centro y dejaba los de los bordes a su tamaño. Ahora los draws que
  leen una textura resuelta en este frame o el anterior nunca pasan por el GS de interfaz. Verificado 32:9 y 21:9 en combate
  (imagen continua, sin rayas en personajes, 60 fps) y menú a 16:9 con bandas.
- Partida del usuario con la TU a 32:9 (dist): 13 tirones de 57-157 ms en ~15 min, ninguno de GPU (PSO/texturas/compilar 0):
  trabajo del propio juego al cargar zona (ya conocido). Audio: la mezcla 5.1 -> estereo pasaba de 1 (picos de 1,5; 11 ventanas
  de 5 s por encima) y se recortaba en duro = chasquidos/distorsion en combate. Ahora limitador suave en audio_wasapi.cpp
  (techo 0,95, ataque inmediato, recuperacion ~50 ms; NG2_NOLIMITER=1 = antes). Comprobado con NG2_WAVDUMP en combate: mezcla
  hasta 1,137, salida maxima 0,95, 0 muestras recortadas. Los "sin entrada" del XMA son finales de sonidos cortos (normal).
- Segunda partida del usuario: NO tenia Mission Mode porque su ng2.ini apunta dlc= a Base\DLC y la TU estaba en dist\NG2\dlc
  (nunca se importo; se jugo con NG2.exe base). ImportDlc ahora mira dlc=, <exe>\dlc y la carpeta Update hermana de dlc=.
  Comprobado con su ini: se importa 000b0000_tu_* y se abre NG2TU.exe. Tirones de esa partida: racimo de 120-743 ms al cargar la
  partida guardada (frames 4500-6700, con huecos de audio 23 -> 60); despues solo 2 en ~9 min y 0 huecos de salida. No se
  reproduce aqui (loadgame.txt, 32:9 a 5120x1440 pantalla completa, NG2_HITCHPROF=100: sin tirones). Log en logs/usuario_s27b.log.
- "El audio se traba" en combate (usuario): con NG2_WAVDUMP + tools/wavdips.py se ven silencios TOTALES de la mezcla de 20-70 ms
  (sonido continuo -> 0 -> mismo nivel), sin XMA sin entrada (NG2_DIPLOG=1 lo registra por frame con los contextos XMA que se
  quedaron sin entrada) y con el hilo del XMA siempre a tiempo (hueco max 3 ms; ahora prioridad critica, NG2_XMAPRIO=0 = antes).
  Causa: AudioPump llamaba al callback del juego dos veces seguidas por cada periodo de WASAPI (10 ms); el callback entrega el
  frame que mezclaron los hilos de XAudio y les pide el siguiente, y si no han terminado entrega uno vacio. Ahora separacion minima
  de 4,8 ms entre callbacks (salvo cola < 15 ms); NG2_AUDIOBURST=1 = antes. Misma prueba de combate: frames mudos 25 -> 1.
- Partida del usuario con la TU: PANTALLA NEGRA a ~1 min del capitulo 1 (hilos parados, GPU sin trabajo, audio mudo). Se reproduce
  con su partida (scripts/cargar_combate.txt = loadgame + combate) y NO con el original. Vigilante nuevo (diag.cpp, tambien en modo
  usuario): sin presentaciones 8 s -> vuelca las pilas ([bloqueo] + [sample], RUN_TU=1 tools/symsample.py). El hilo principal
  daba vueltas en sub_83656870 (= 83633E88 del original, cambiada por la TU): su tabla de 153 casos trasladada del original tenia
  16 entradas mal. tools/tu_check_tables.py compara cada tabla con los datos reales de la TU (--fix): 9 tablas mal; ademas
  82421C64 (19 casos, no 18; el 10 apunta a relleno -> epilogo) y 8284FDE4 (27, no 25, segun su cmplwi). Con NG2TU.exe nuevo la
  misma prueba sigue 4 min y llega a la cinematica. Los "default" trasladados tambien estan mal en ~50 tablas, pero no se usan
  (el cmplwi/bgt se traduce como codigo normal). Copia previa: config/ng2_tu_switch_tables_fixed.antes_s27.toml.
- Partida del usuario (logs/usuario_s27c.log): 32 min con la TU, capitulos con videos aurora07-12, sin bloqueos, 10 tirones de
  100-185 ms, sin huecos de audio tras el arranque. Quejas: videos/cinematicas que "de repente se ven borrosos" (no reproducido:
  intro WMV y cinematica de la tienda iguales con y sin superficies anchas) y la vida de los jefes "se bugea" en panoramico.
  Cambio: la excepcion del GS de interfaz (draws que leen una textura resuelta) ahora solo cuenta texturas resueltas desde
  superficies de pantalla (ResolvedTex/ResolvedDepth.screen, pitch 1120/560/280/1280/640/320); las pequenas que el juego prepara
  para la interfaz vuelven a comprimirse. Sin verificar con un jefe.
- "Videos borrosos" (captura de la ventana del usuario): el contador decia 1280x720 -> 3413x960. Su monitor 5120x1440 esta al
  150 % y la ventana del juego no declaraba DPI: Windows le daba 3413x960 y estiraba TODA la imagen. Ahora el proceso es
  PER_MONITOR_AWARE_V2 (SetProcessDpiAwarenessContext antes de crear la ventana; solo el hilo no basta porque el hilo de la GPU
  pregunta el tamano). NG2_DPIVIRTUAL=1 = antes. Verificado: pantalla completa 5120x1440, cadena de intercambio 5120x1440, 60 fps.
- Partida siguiente del usuario (logs/usuario_s27d.log, 14 min, capitulo de Nueva York, 40 capturas de la ventana): sin bloqueos,
  panoramico continuo, HUD bien; huecos de audio solo al cargar el capitulo; tirones de 300-750 ms solo en cargas. PERO seguia a
  3413x960: make_dist no habia copiado NG2TU.exe (copia fallida en silencio). Ahora make_dist avisa y para si no puede copiar.
  Comprobado dist\NG2 (NG2.exe --jugar -> NG2TU): cadena de intercambio 5120x1440.
- "Pop" de escenario en los laterales (usuario): la camara sigue en 16:9 (Hor+ en el VS) y el juego descartaba lo que quedaba fuera
  de ese campo. sub_837CC920 solo arma el frustum de descarte (tanX +0x20/-tanX +0x24, tanY +0x28/+0x2C, planos +0x30/+0x34; no
  hay matriz de proyeccion): ahora se multiplica tanX por kx tras la llamada (NG2_PANOCULL=0 = antes). 32:9 en combate: +15 % de
  draws, 60 fps igual, imagen correcta.
- Auditoria de la TU: XexGetProcedureAddress devolvia thunks del original (runtime/xam_ordinals_tu.inc generado con los registros de
  importacion reales de la TU, 65/65); ganchos activos revisados (barrera de XAudio identica; guardado con NG2ADDR; los demas son
  #ifndef NG2_TU); jt_finder sobre la TU: 0 bctr sin tabla.
- RESOLUCION INTERNA (como GH): NG2_RES = 1 | 2 | 3 | auto (alto del monitor / 720), resolucion_interna= en ng2.ini y "Resolucion
  interna" en Graficos del launcher (Automatica por defecto). Todas las superficies se crean Rs veces mayores en X e Y
  (rt->sx = Rs x ancho panoramico si es de pantalla, rt->sy = Rs); resolves, borrados, recortes, viewports, copias de profundidad,
  regiones derivadas, volcado a memoria y capturas con sx/sy; XTexMode.z bits 16-23 = escala Y; PsParamGen con escala codificada
  (bits 0-4 X, 5-9 Y, 10/11 sin redondear); medio pixel de superficie en los draws de restauracion de profundidad; variantes del
  color->profundidad por escala; bufer de lectura x min(S*Rs*Rs, 8) y resolve por CPU omitido si no cabe. Verificado: 32:9 a x2
  (5120x1440 nativo, 60 fps en combate, sin rayas) y 16:9 a x2 (2560x1440, intro y cinematica de la tienda).
- Tirones de carga (NG2_HITCHPROF=100 + tools/hitchagg.py): el de ~170 ms es el hilo principal en sub_82385A00 (Dijkstra sobre un
  grafo de 256 nodos; ~40000 llamadas de ~4 us en las cargas de zona, trabajo del propio juego) y el de ~110 ms un hilo del juego
  (sub_822F16D0 -> sub_8373A4D0/sub_83737AE0) con el principal esperando. En las partidas del usuario los de 300-850 ms son del
  arranque y de cargas, casi todos con 0 ms nuestros; lo nuestro eran shaders de la cache de disco cargados en el momento
  (hasta 146 ms en una carga). Ahora presupuesto de 8 ms por frame (NG2_SHADERBUDGET); el resto se traduce y carga en el grupo de
  hilos (CompileJob con type | 0x100).

## Sesion 28
- Partida del usuario (1440p interno, TU, capitulo 3, 16 min): 60 fps casi siempre, 0 bloqueos, 39 huecos de salida de audio en
  total. Quejas: "poco responsivo en ocasiones" y "el audio empieza a trabarse". Causa: NUESTRA captura de la ventana cada 20 s
  (scratchpad watch_play.py -> PrintWindow con PW_RENDERFULLCONTENT de 5120x1440, 0,5-0,8 s por captura). Prueba de combate con
  NG2_WAVDUMP: sin capturas 0 caidas de la mezcla y 60 fps; con captura cada 5 s 10 caidas y 47-55 fps. No capturar mientras
  el usuario juega (solo revisar ng2.log).
- "Cinematicas borrosas" (captura del usuario en la tienda de Muramasa): planos enteros desenfocados, tambien la cara del
  personaje enfocado; igual a 720p y 1440p. La profundidad de campo (y las sombras) leen la profundidad resuelta como
  k_24_8_FLOAT (fmt 23, superficies D24FS8); el resolve de profundidad por GPU solo servia fmt 22 y 6, asi que fmt 23 subia la
  memoria, que ese camino no escribe -> profundidad vieja. Ahora fmt 23 usa la copia R32_FLOAT (rz); la copia empaquetada (8888)
  y el resolve por CPU codifican en 20e4 si la superficie es D24FS8 (EncodeDepth24F). NG2_DEPTHFS8=0 = antes.
  NG2_DEPTHREADLOG=1 registra con que formato se lee cada profundidad resuelta. Verificado: tienda (inicio del capitulo 1) nitida
  en las caras con el fondo desenfocado, a 720p y 1440p; combate de Mission Mode sin cambios visibles en las sombras.
  Prueba: scripts/tienda_shots1.txt con NG2.exe base (partida nueva; respaldar run/userdata antes, la sobrescribe).
- Partida siguiente del usuario (logs/usuario_s28b.log, ~9 min, TU, sin capturas): 60 fps en 105 de 109 tramos de 5 s, 0 bloqueos,
  22 huecos de salida solo en los primeros 6 s (arranque), 0 silencios totales. Aun asi oyo "un corte del audio en un combate".
  El contador "cortes en la mezcla" solo ve silencios totales. Ahora siempre activos: caidas de -20 dB de un frame a otro
  ([audio] caida t=..., y "caidas -20 dB" en la linea [wasapi]; NG2_NODIPLOG=1 = solo contar), cada hueco de salida con su hora
  ([wasapi] hueco t=...), callback del juego > 8 ms o sin frame ([audio] callback lento / sin frame) y F9 = marca en el log
  ([marca] F9 t=...; F12 tambien deja marca). Combate de 7 min (scripts/combate_largo.txt, RUN_TU=1, NG2_RES=2, rutas relativas a
  run/): una vez, racha de tirones del propio juego (80-238 ms, frames 22069-22108, todo lo nuestro a 0 ms) con 11 huecos de salida
  en esos segundos; repetido con NG2_HITCHPROF=150 no se reprodujo (0 huecos, 1 callback de 11 ms). Las 6 caidas de la pantalla de
  titulo (pares cada 148 s) son de la musica del titulo. Siguiente: con la marca F9 del usuario, cruzar caidas/huecos/tirones.

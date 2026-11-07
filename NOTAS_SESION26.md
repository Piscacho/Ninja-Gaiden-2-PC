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

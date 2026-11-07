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

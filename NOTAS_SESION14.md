# Sesión 14 — Lanzador y distribución

Fecha: 2026-11-02. Sin commits.

## Resultado

- **Carpeta de distribución `dist\NG2`** (`tools\make_dist.bat`, ~205 MB). Contiene:
  - `NG2.exe`, `avcodec-62.dll`, `avutil-60.dll` y `swresample-6.dll` (FFmpeg, LGPL).
  - `ng2.ini` de ejemplo, `LEEME.txt` (instalación, controles, opciones y problemas conocidos).
  - `licenses\` (FFmpeg LGPL y XenonRecomp MIT) y `dlc\` vacía.
  - **Nada del juego.** `make_dist.bat /cache` añade la caché de shaders compilados de `run\` (solo para pruebas propias).
- **Probado desde cero**: carpeta limpia con solo la ruta del ISO en `ng2.ini` y los 4 paquetes DLC **originales** (STFS) en `dlc\`.
  El exe:
  1. Descifra `default.xex` del ISO (no hace falta `basefile.bin`).
  2. Importa los 4 DLC a `userdata\content`.
  3. Pide dispositivo y crea la partida del sistema.
  4. Llega al menú principal y a LOAD GAME (`run/montage49.png`).
- **Lanzador** (`runtime/launcher.cpp`, heredado de GH): sin ruta válida abre el diálogo para elegir el ISO y guarda `ng2.ini`.
  Opciones nuevas para NG2: `perfil=1/0` (con o sin perfil y guardado) y `vibracion=1/0`. Se quitaron las de GH
  (`saltar_intro`, `grano_pelicula`, `desenfoque_movimiento`), que en NG2 no hacían nada: el vídeo de logos y los hashes de
  shaders eran de GH. Cualquier `NG2_*=valor` en `ng2.ini` pasa como variable de entorno (diagnóstico).
- `dist_files\`: plantillas `ng2.ini` y `LEEME.txt` (CRLF).

## Notas

- Las rutas relativas de `ng2.ini` (`datos=userdata`, `dlc=dlc`) son relativas al exe (el programa cambia al directorio del exe).
- El exe vuelca la microcódigo de los shaders vistos a `shaders\` y compila a `shadercache\` (precarga en la siguiente
  partida). `NG2_NODUMPSHADERS=1` lo desactiva.
- Pendiente para una release como la de GH: icono y recurso de versión (`make_icon.py`, `.rc`), `make_release.ps1` (zip + SHA-256),
  ventana de lanzador con opciones, actualizador.

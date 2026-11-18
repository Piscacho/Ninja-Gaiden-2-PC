# Ninja Gaiden II para PC

Recompilación estática para PC de **Ninja Gaiden II** (Xbox 360, 544307D5) para Windows 10/11 con DirectX 12.

> **No se incluyen archivos del juego.** Necesitas tu propio ISO de Xbox 360 de Ninja Gaiden II
> (versión World: *Ninja Gaiden II (World) (En,Ja,Fr,De,Es,It,Zh,Ko,Pl,Ru)*).
> Ninja Gaiden es una marca de Koei Tecmo. Este proyecto no está afiliado a Koei Tecmo, Team NINJA ni Microsoft.

## Descargar y jugar

1. Descarga el `.zip` desde la página de
   [Releases](https://github.com/Piscacho/Ninja-Gaiden-2-PC/releases/latest).
2. Descomprímelo en una carpeta, por ejemplo `C:\Juegos\Ninja Gaiden II`.
3. Abre **`NG2.exe`**. Si Windows muestra "Windows protegió tu PC", pulsa
   **Más información → Ejecutar de todas formas**.
4. En el launcher, página **Juego**, elige el ISO y pulsa **JUGAR**.

- **Controles**: mando de Xbox (XInput) o de PlayStation (DualSense/DualShock 4, con vibración), o teclado
  (WASD, IJKL para la cámara; la lista completa está en `LEEME.txt`).
- **DLC**: copia tus paquetes de Xbox 360 (archivos LIVE/CON) en `dlc\`; se importan solos al arrancar.
  Funcionan los trajes (Biometal, Fiend, Shadow Walker...).
- **Mission Mode**: copia también en `dlc\` la **actualización del título #2** de tu consola (archivo `TU_1A461UL_...`)
  y el DLC de Mission Mode. `NG2.exe` aplica la actualización en memoria al arrancar (abre `NG2TU.exe`) y aparece
  **MISSIONS** en el menú principal. Se desactiva en la página **Juego** del launcher.
- **Imagen**: resolución interna ×2/×3 (1440p, 4K), panorámico **21:9 y 32:9** (Hor+, con la interfaz en su sitio),
  filtrado anisotrópico, escalado y opciones de desenfoque y grano en el launcher.
- **Partidas**: carpeta `userdata\` junto al ejecutable (consérvala si actualizas a mano).
- **Actualizar**: el launcher avisa de versiones nuevas (página **Juego → Actualizaciones y launcher**) y se actualiza
  solo con un botón, conservando `ng2.ini`, las partidas, el DLC y la caché de shaders.

### Teclas

F11 o Alt+Enter: pantalla completa · F3: contador de FPS · F12: captura (`shot_9xx.bmp`) ·
F9: marca en `ng2.log` y guarda los últimos 30 s de audio en `corte_NN.wav` (para informar de cortes de sonido).

### Problemas conocidos

- La primera vez que aparece un efecto puede faltar un instante mientras se compila su shader (luego queda en `shadercache\`).
- Las cargas de zona tienen tirones de 100-300 ms (trabajo del propio juego).
- En panorámico, el marco de la barra de vida de los jefes se estira a todo el ancho.
- Si tienes otros programas que usan mucha CPU abiertos a la vez, el sonido puede cortarse.

Para informar de un fallo, adjunta `ng2.log` (el registro de la última partida).

## Compilar desde el código

El repositorio **no contiene nada del juego**: ni el ISO, ni el XEX, ni el código C++ que XenonRecomp genera a partir
de él (`ppc/`, `ppc_tu/`). Cada persona lo genera desde su propio ISO.

Requisitos:
- Visual Studio 2022 Build Tools (MSVC 14.4x, con CMake y Ninja).
- LLVM 23 para Windows (clang-cl + lld-link) y FFmpeg 8 compartido (`avcodec-62`, `avutil-60`, `swresample-6`).
  Las rutas están al principio de `tools/build_game.bat`, `tools/build_game_tu.bat` y `project/CMakeLists.txt`.
- Python 3.8+ y `openssl` en el PATH (para `tools/xex2.py`).

Pasos (desde la raíz del repositorio):

```bat
git clone https://github.com/Piscacho/Ninja-Gaiden-2-PC.git
cd Ninja-Gaiden-2-PC

rem 1. Sacar el ejecutable del ISO y descifrarlo (genera xex\basefile.bin)
python -I tools\xdvdfs.py "<ruta al ISO>" extract default.xex xex\default.xex
python -I tools\xex2.py xex\default.xex xex

rem 2. XenonRecomp con los cambios para este juego
git clone --recursive https://github.com/hedge-dev/XenonRecomp.git
cd XenonRecomp
git apply ..\xenonrecomp_gh6.diff
cd ..
python tools\patch_recompiler_ng2.py XenonRecomp\XenonRecomp\recompiler.cpp
tools\build_msvc.bat

rem 3. Generar el código C++ del juego (ppc\)
mkdir ppc
python tools\run_recomp.py

rem 4. Compilar y crear la carpeta para jugar / la release
tools\build_game.bat
tools\make_dist.bat
powershell -ExecutionPolicy Bypass -File tools\make_release.ps1
```

`NG2TU.exe` (Mission Mode) se genera igual a partir de la actualización del título #2 de tu consola: el parche se
aplica con `tools/lzxdelta.cpp` + `tools/xexp_records.py`, `tools/make_tu_xex.py` crea el XEX y `tools/tu_rebuild.sh`
genera `ppc_tu/` y compila. Los detalles están en `NOTAS_SESION26.md`.

Modo desarrollo: `project\build\NG2.exe xex\basefile.bin` (sin launcher) o `python tools\run_game.py <log> <segundos>`.
Las opciones de diagnóstico `NG2_*` están documentadas en `NOTAS_SESION*.md`.

### Estructura

| Carpeta | Contenido |
|---|---|
| `runtime/` | Kernel y XAM de Xbox 360, GPU Xenos → Direct3D 12, audio (XMA/WASAPI), entrada, launcher y parche de la actualización |
| `config/` | Configuración de XenonRecomp (funciones y tablas de saltos del juego y de la actualización) |
| `project/` | CMake del ejecutable |
| `tools/` | Extracción del ISO/XEX, análisis, compilación, distribución y release |
| `scripts/` | Guiones de entrada para pruebas automáticas (`NG2_INPUTSCRIPT`) |
| `dist_files/` | Archivos que acompañan al ejecutable (`ng2.ini`, `LEEME.txt`) |
| `NOTAS_SESION*.md` | Diario técnico de cada sesión de trabajo |

## Requisitos para jugar

- Windows 10 / 11 de 64 bits.
- Tarjeta gráfica con Direct3D 12 y controlador actualizado.

## Créditos

- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp): recompilador de PowerPC a C++.
- [Xenia](https://github.com/xenia-project/xenia): documentación y referencia del comportamiento de la GPU y del kernel de Xbox 360.
- [FFmpeg](https://ffmpeg.org): decodificación de audio XMA y de los vídeos WMV.
- Imágenes del launcher: [SteamGridDB](https://www.steamgriddb.com/) (autores en `CREDITOS.txt`; solo van en la release).

## Licencia

MIT ([LICENSE](LICENSE)). Los componentes de terceros mantienen sus propias licencias
([THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt)).

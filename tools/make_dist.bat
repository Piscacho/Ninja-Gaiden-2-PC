@echo off
rem Crea la carpeta de distribucion dist\NG2 (sesion 14, como la de GH): ejecutable, DLL de FFmpeg (LGPL), ng2.ini de ejemplo,
rem LEEME y licencias. No copia nada del juego. Con /cache copia tambien la cache de shaders compilados de run\ (solo para pruebas).
rem Uso (desde Recomp\): tools\make_dist.bat [/cache]
setlocal
set R=%~dp0..
set DIST=%R%\dist\NG2
if not exist "%DIST%" mkdir "%DIST%"
if not exist "%DIST%\dlc" mkdir "%DIST%\dlc"
if not exist "%DIST%\licenses" mkdir "%DIST%\licenses"
copy /y "%R%\project\build\NG2.exe" "%DIST%\NG2.exe" >nul || (echo Falta project\build\NG2.exe & exit /b 1)
rem sesion 26: version con la actualizacion del titulo #2 (Mission Mode); NG2.exe le cede el paso si la TU esta instalada
if exist "%R%\project\build_tu\NG2TU.exe" copy /y "%R%\project\build_tu\NG2TU.exe" "%DIST%\NG2TU.exe" >nul || (echo ERROR: no se pudo copiar NG2TU.exe, esta abierto? & exit /b 1)
copy /y "%R%\project\build\avcodec-62.dll" "%DIST%" >nul
copy /y "%R%\project\build\avutil-60.dll" "%DIST%" >nul
copy /y "%R%\project\build\swresample-6.dll" "%DIST%" >nul
copy /y "%R%\dist_files\LEEME.txt" "%DIST%" >nul
rem sesion 25: imagenes del launcher (SteamGridDB)
if exist "%R%\dist_files\portada.png" copy /y "%R%\dist_files\portada.png" "%DIST%" >nul
if exist "%R%\dist_files\logo.png" copy /y "%R%\dist_files\logo.png" "%DIST%" >nul
copy /y "%R%\dist_files\CREDITOS.txt" "%DIST%" >nul
set GHTC=G:\Guitar Hero Warrior of rock\Recomp\toolchain
if exist "%GHTC%\ffmpeg\LICENSE.txt" copy /y "%GHTC%\ffmpeg\LICENSE.txt" "%DIST%\licenses\FFmpeg-LGPL-3.0.txt" >nul
if exist "%R%\XenonRecomp\LICENSE.md" copy /y "%R%\XenonRecomp\LICENSE.md" "%DIST%\licenses\XenonRecomp-MIT.txt" >nul
rem ng2.ini: no se pisa el del usuario
if not exist "%DIST%\ng2.ini" copy /y "%R%\dist_files\ng2.ini" "%DIST%" >nul
if /i "%1"=="/cache" if exist "%R%\run\shadercache" xcopy /d /y /q /i "%R%\run\shadercache\*.cso" "%DIST%\shadercache\" >nul
echo Distribucion creada en %DIST%
endlocal

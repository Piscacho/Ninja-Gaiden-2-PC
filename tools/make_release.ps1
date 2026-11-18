# Crea la release para GitHub (sesion 28), como la de GH:
#   release_staging\Ninja Gaiden II\                       carpeta limpia (sin nada del juego ni de pruebas)
#   <Out>\Ninja-Gaiden-2-PC-v<ver>-windows.zip (+ .sha256)  para subir como asset de la release
# Uso (desde Recomp\):  powershell -ExecutionPolicy Bypass -File tools\make_release.ps1 -Ver 1.0.0 [-Out <carpeta>] [-NoBuild]
param([Parameter(Mandatory)][string]$Ver, [string]$Out = "", [switch]$NoBuild)
$ErrorActionPreference = "Stop"
$R = Split-Path $PSScriptRoot -Parent
if (-not $Out) { $Out = Split-Path $R -Parent }
$name = "Ninja Gaiden II"

if (-not $NoBuild) {
    cmd /c "`"$R\tools\build_game.bat`"" | Out-Null; if ($LASTEXITCODE -ne 0) { throw "La compilacion de NG2.exe fallo" }
    cmd /c "`"$R\tools\build_game_tu.bat`"" | Out-Null; if ($LASTEXITCODE -ne 0) { throw "La compilacion de NG2TU.exe fallo" }
}
$bin = "$R\project\build"; $binTu = "$R\project\build_tu"

# 1. Carpeta limpia. Sin portada.png/logo.png: son de SteamGridDB (terceros); el launcher funciona sin ellas
$stage = "$R\release_staging\$name"
Remove-Item "$R\release_staging" -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$stage\licenses", "$stage\dlc" | Out-Null
Copy-Item "$bin\NG2.exe", "$binTu\NG2TU.exe", "$bin\avcodec-62.dll", "$bin\avutil-60.dll", "$bin\swresample-6.dll" $stage
Copy-Item "$R\dist_files\LEEME.txt", "$R\dist_files\ng2.ini", "$R\README.md", "$R\LICENSE", "$R\THIRD-PARTY-NOTICES.txt" $stage
Copy-Item "G:\Guitar Hero Warrior of rock\Recomp\toolchain\ffmpeg\LICENSE.txt" "$stage\licenses\FFmpeg-LGPL-3.0.txt"
Copy-Item "$R\XenonRecomp\LICENSE.md" "$stage\licenses\XenonRecomp-MIT.txt"
[IO.File]::WriteAllText("$stage\dlc\LEEME.txt", "Copia aqui tus paquetes de DLC de Xbox 360 (archivos LIVE/CON) y, para Mission Mode, la actualizacion del titulo #2 (TU_1A461UL_...). Se importan solos al arrancar.`r`n", (New-Object Text.UTF8Encoding $false))
# ng2.ini sin rutas personales
$ini = Get-Content "$stage\ng2.ini" -Raw
if ($ini -notmatch '(?m)^iso=\r?$' -or $ini -notmatch '(?m)^dlc=dlc\r?$') { throw "dist_files\ng2.ini tiene rutas personales" }
# Nada del juego ni de pruebas en el paquete
$bad = Get-ChildItem $stage -Recurse -File | Where-Object { $_.Extension -in ".iso", ".xex", ".xexp", ".bin", ".cso", ".log", ".bmp", ".wav", ".png", ".dat" -or $_.Name -like "TU_*" }
if ($bad) { throw "Archivos que no deben ir en la release: $($bad.FullName -join ', ')" }

# 2. Zip (una carpeta superior con el nombre del juego) + suma SHA-256
$zip = "$Out\Ninja-Gaiden-2-PC-v$Ver-windows.zip"
Remove-Item $zip -ErrorAction SilentlyContinue
& "$env:SystemRoot\System32\tar.exe" -a -cf $zip -C "$R\release_staging" $name
if ($LASTEXITCODE -ne 0) { throw "No se pudo crear el zip" }
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
"$hash  $(Split-Path $zip -Leaf)" | Set-Content "$zip.sha256" -Encoding ASCII
Write-Host "Zip:     $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB)"
Write-Host "SHA-256: $hash"

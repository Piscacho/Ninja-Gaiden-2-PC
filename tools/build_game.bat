@echo off
rem Compila project\build\NG2.exe con clang-cl + lld-link (LLVM portable del proyecto GH) y las librerias de MSVC 14.44
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set LLVM=G:\Guitar Hero Warrior of rock\Recomp\toolchain\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin
set PATH=%LLVM%;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
cd /d "G:\ninja gaiden 2\Recomp\project"
if not exist build\build.ninja cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link || exit /b 1
cmake --build build %* -- -k 0

@echo off
rem Sesion 26: compila project\build_tu\NG2TU.exe (version con la actualizacion del titulo #2, codigo de ppc_tu\)
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set LLVM=G:\Guitar Hero Warrior of rock\Recomp\toolchain\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin
set PATH=%LLVM%;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
cd /d "G:\ninja gaiden 2\Recomp\project"
rem Sesion 28: icono y version con llvm-rc (como en GH)
"%LLVM%\llvm-rc.exe" -no-preprocess -fo ng2.res ng2.rc || exit /b 1
if not exist build_tu\build.ninja cmake -S . -B build_tu -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link -DNG2_TU=ON || exit /b 1
cmake --build build_tu %* -- -k 0

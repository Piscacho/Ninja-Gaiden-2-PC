cmake_minimum_required(VERSION 3.20)
project(NG2Recomp CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

set(RECOMP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
# Sesion 26: -DNG2_TU=ON compila la version con la actualizacion del titulo #2 (codigo de ppc_tu/, NG2TU.exe)
option(NG2_TU "Version con la actualizacion del titulo #2" OFF)
if(NG2_TU)
  set(PPC_DIR ${RECOMP_ROOT}/ppc_tu)
  set(NG2_OUT NG2TU)
else()
  set(PPC_DIR ${RECOMP_ROOT}/ppc)
  set(NG2_OUT NG2)
endif()
file(GLOB PPC_SOURCES CONFIGURE_DEPENDS ${PPC_DIR}/ppc_recomp.*.cpp ${PPC_DIR}/ppc_func_mapping.cpp)

# Codigo PPC traducido (enorme): libreria aparte con flags propias
add_library(NG2RecompLib STATIC ${PPC_SOURCES})
target_include_directories(NG2RecompLib PUBLIC ${PPC_DIR} ${RECOMP_ROOT}/XenonRecomp/thirdparty/simde)
target_compile_options(NG2RecompLib PRIVATE /O1 -fno-strict-aliasing -msse4.1 -Wno-everything -gline-tables-only /Z7 "/FI${RECOMP_ROOT}/compat/ppc_indirect.h")

set(FFMPEG "G:/Guitar Hero Warrior of rock/Recomp/toolchain/ffmpeg")
set(RT ${RECOMP_ROOT}/runtime)
add_executable(NG2 ${RT}/main.cpp ${RT}/kernel.cpp ${RT}/kernel_ng2.cpp ${RT}/hooks_ng2.cpp ${RT}/fibers_ng2.cpp ${RT}/import_stubs.cpp ${RT}/threads.cpp ${RT}/fs.cpp ${RT}/gpu_stub.cpp ${RT}/diag.cpp
    ${RT}/gpu_cp.cpp ${RT}/gpu_window.cpp ${RT}/atomics.cpp ${RT}/xam.cpp ${RT}/audio_wasapi.cpp ${RT}/launcher.cpp ${RT}/launcher_ui.cpp ${RT}/dualsense.cpp
    ${RT}/gpu_d3d12.cpp ${RT}/gpu_shader.cpp ${RT}/gpu_shadertest.cpp ${RT}/xma.cpp)
target_include_directories(NG2 PRIVATE ${RT} ${FFMPEG}/include)
target_compile_options(NG2 PRIVATE /O2 -fno-strict-aliasing -msse4.1 -Wno-unused-parameter /Z7)
target_compile_definitions(NG2 PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN $<$<BOOL:${NG2_TU}>:NG2_TU>)
set_target_properties(NG2 PROPERTIES OUTPUT_NAME ${NG2_OUT})
target_link_libraries(NG2 PRIVATE NG2RecompLib user32 gdi32 d3d12 dxgi d3dcompiler winmm ole32 xinput bcrypt comdlg32 hid setupapi comctl32 dwmapi uxtheme shell32 windowscodecs msimg32 psapi ${FFMPEG}/lib/avcodec.lib ${FFMPEG}/lib/avutil.lib)
add_custom_command(TARGET NG2 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FFMPEG}/bin/avcodec-62.dll ${FFMPEG}/bin/avutil-60.dll ${FFMPEG}/bin/swresample-6.dll $<TARGET_FILE_DIR:NG2>)
target_link_options(NG2 PRIVATE /DEBUG /SUBSYSTEM:CONSOLE)

#!/bin/bash
# Sesion 26: regenera ppc_tu/ y NG2TU.exe con las tablas de config/ng2_tu_switch_tables_fixed.toml
# (huecos con switch: primera pasada sin ellos -> gap_functions.py -> segunda pasada limpia -> compilacion)
set -e
cd "$(dirname "$0")/.."
python -I tools/tu_fix_tables.py
rm -f config/ng2_tu_gap_functions.txt
python -I tools/make_tu_toml.py
rm -rf ppc_tu && mkdir ppc_tu
(cd config && ../XenonRecomp/build-msvc/XenonRecomp/XenonRecomp.exe ng2_tu.toml ../XenonRecomp/XenonUtils/ppc_context.h > ../logs/tu_recomp_a.log 2>&1)
python -I tools/gap_functions.py xex/tu_basefile.bin xex/tu_functions_pdata.txt config/ng2_tu_switch_tables_fixed.toml logs/tu_recomp_a.log > config/ng2_tu_gap_functions.txt
python -I tools/make_tu_toml.py
rm -rf ppc_tu && mkdir ppc_tu
(cd config && ../XenonRecomp/build-msvc/XenonRecomp/XenonRecomp.exe ng2_tu.toml ../XenonRecomp/XenonUtils/ppc_context.h > ../logs/tu_recomp_b.log 2>&1)
echo "errores de XenonRecomp: $(grep -c ERROR logs/tu_recomp_b.log || true)"
cmd //c "tools\build_game_tu.bat" > logs/build_tu.log 2>&1 || true
echo "errores de compilacion: $(grep -c ' error' logs/build_tu.log || true)"
ls -la project/build_tu/NG2TU.exe

# Sesión 1 — Extracción y mapeo del XEX (Ninja Gaiden II, X360)

Fecha: 2026-11-01. Sin commits (igual que en Guitar Hero WoR).
Plan por sesiones, como en GH: 1 XEX → 2 TOML + C++ → 3 runtime → 4 GPU → 5 audio/entrada/vídeo → 6 DLC → 7 lanzador.

## Carpetas

- `G:\ninja gaiden 2\Base\` — originales (ISO en `juego\`, DLC STFS en `DLC\`). **Solo lectura.**
- `G:\ninja gaiden 2\Recomp\` — todo el trabajo:
  - `tools\` scripts (copiados de GH y adaptados), `xex\` XEX y análisis, `config\` TOML de XenonRecomp,
    `dlc\` DLC extraídos, `ref\` tablas de Xenia (BSD), `rea\` notas de REA, `logs\`.

## Qué se hizo

1. **ISO → default.xex** (`tools/xdvdfs.py`). XGD2, partición en `0xFD90000`.
   - `xex/default.xex`: 29.814.784 bytes, SHA1 `6505f2f4171461ef596f1bf8b77c8feb04ad4e3a`.
   - `xex/iso_filelist.txt`: solo 66 archivos (todo empaquetado en `.ng2`, ver abajo).
2. **XEX → imagen** (`tools/xex2.py`): AES-128 retail + compresión **basic** (sin LZX).
   - `xex/basefile.bin`: 61.308.928 bytes (0x3A78000; imagen 0x3A80000), SHA1 `3c49fbfc2ab0345d4ade290d6ae95b518b426503`.
     Imagen mapeada: offset = VA − 0x82000000.
3. **Secciones PE** (`tools/pe_sections.py`, nuevo) → `xex/pe_sections.txt`. Los scripts `find_helpers.py` y `analyze.py`
   ya leen las secciones del binario (en GH estaban fijas).
4. **Imports** (`tools/imports.py` + `tools/name_imports.py`, nuevo, con `ref/*_table.inc` de Xenia) → `xex/imports_named.txt`.
   238 únicos: **227 funciones** (xam 65, xboxkrnl 162) + 11 variables. Todos con nombre.
5. **Funciones** (`tools/analyze.py`) → `xex/functions_pdata.txt`: **42.504** en `.pdata` (GH: 19.191).
6. **Huecos sin .pdata** → `xex/pdata_gaps.txt`: 3.084 huecos con datos no nulos (~151.000 palabras).
   Muchos serán tablas de saltos incrustadas; se depuran en la sesión 2 con el log de XenonRecomp.
7. **Tablas de saltos**: `XenonAnalyse.exe` (el compilado en el proyecto GH) → `config/ng2_switch_tables.toml`, **1.025 switches**.
8. **TOML borrador**: `config/ng2.toml` con todas las direcciones de abajo.
9. **DLC** (`tools/stfs_extract.py`) → `dlc/00000002_<archivo>\` + `.xcd`. Los 4 paquetes son `LIVE`, tipo 2, título 544307D5:
   | Paquete | Nombre | Entradas | Bytes |
   |---|---|---|---|
   | F80413BA… | Biometal Hayabusa | 9 | 11,8 MB |
   | 863C5F74… | Fiend Hayabusa | 9 | 11,9 MB |
   | EC14BBD2… | Shadow Walker Hayabusa | 9 | 10,7 MB |
   | F591AD78… | Mission Mode | 242 | 139 MB |
   Los DLC traen archivos **sueltos** (`char/ryu_dlc/*.gmd`, `battle/*.btl`, `message/dlc0N.bin`, `sprite/*.spr`…).
10. **Desensamblador**: `tools/ppcdasm.py` (capstone 5.0.7 instalado con `pip --user`).
    Uso: `python tools/ppcdasm.py xex/basefile.bin 83955930 40`. No decodifica VMX128 (salen como `.long`).
    El LLVM del toolchain de GH **no** trae backend PowerPC (llvm-mc no sirve).

## REA

No soporta PowerPC (ver `rea/LEEME.md`): `raw-binary` sin PPC, rizin no instalado, STFS no soportado.
Servirá para el ejecutable x86_64 recompilado a partir de la sesión 3.

## Hallazgos

| Dato | Valor |
|---|---|
| Title ID | 544307D5 (versión 4, disco 1/1, media ID 69F555CB) |
| Base de carga | 0x82000000 |
| Tamaño imagen | 0x3A80000 (~3,7 veces GH) |
| **Entry point** | **0x83818DA8** |
| XDK | agosto 2007 (rutas `D:\xenon\aug07\...`) |
| Vídeo | **WMV** (decodificador de Microsoft enlazado estáticamente, con shaders DXVA); en GH era Bink |
| Audio | XACT (`stream_bgm.xwb/xsb`, `stream_system.xwb/xsb`, `ng2_sound.xgs`) |

### Secciones PE (VA)

| Sección | VA | Tamaño | Tipo |
|---|---|---|---|
| .rdata | 0x82000600 | 0x291350 | RO |
| .pdata | 0x82291A00 | 0x53040 | RO |
| **.text** | **0x822F0000** | **0x171F444** | código |
| .embsec_ ×8 | 0x83A0F600 … 0x83A61D30 | ~0x52000 en total | código (thunks de import en 0x83A0E614) |
| .data | 0x83A70000 | 0x1F71C5C | RW |
| .XBMOVIE | 0x859E1E00 | 0xC | RW |
| .idata | 0x859F0000 | 0x42E | RW |
| .XBLD | 0x85A00000 | 0xF0 | RO |
| .reloc | 0x85A00200 | 0x244774 | RO |

Regiones de páginas: RO 0x82000000-0x822F0000, código 0x822F0000-0x83A70000, datos 0x83A70000-0x859F0000, RO 0x859F0000-0x85A80000.
**Revisar en la sesión 2**: qué son las `.embsec_` (tienen entradas en `.pdata`: la última función está en 0x83A5F000) y si XenonRecomp las recorre.

### Direcciones para XenonRecomp (ya en `config/ng2.toml`)

```toml
restgprlr_14_address = 0x83954280
savegprlr_14_address = 0x83954230
restfpr_14_address   = 0x8395582C
savefpr_14_address   = 0x839557E0
restvmx_14_address   = 0x83957178
savevmx_14_address   = 0x83956EE0
restvmx_64_address   = 0x8395720C
savevmx_64_address   = 0x83956F74
longjmp_address = 0x83955930
setjmp_address  = 0x83955C50
```

Verificación:
- Helpers de registros: patrones exactos, un acierto cada uno (`tools/find_helpers.py` → `xex/helpers.txt`).
  La separación save/rest 14↔64 es 0x94, igual que en GH.
- **longjmp 0x83955930** (.pdata, len 0x314, igual que GH): `mr r7,r3`, llama a 0x83962340 (unwind, por confirmar),
  restaura f14–f31 desde +0x00, r13–r31 desde +0x98 y v64+ con `lvx128`.
- **setjmp 0x83955C50** (sin .pdata, tamaño 0x2CC hasta el `blr` de 0x83955F18):
  `lis r4,0x859E; lwz r0,0x1C4C(r4); cmpwi r0,0; mtctr r0; bnectr` (gancho opcional en **0x859E1C4C**),
  luego `mflr r0; mfcr r4` y `stfd f14,0(r3)`…
- Tras setjmp hay 3 funciones hoja sin .pdata (0x83955F20/F40/F60, 0x1C cada una; consultan una tabla de tipos de carácter). Añadidas a `functions`.

### Formato de los paquetes del disco

| Archivo | Magic | Notas |
|---|---|---|
| cmn.ng2 (448 MB), char.ng2 (268 MB) | `chr_dat2` | cabecera con versión `ff000100`, tamaño 0x30 |
| s_chap_01..14.ng2, rtm.ng2, other*.ng2 | `GMD\0` | mismo esquema de cabecera |

En el XEX aparecen 891 rutas `.gmd` y 71 `.chr` (`char\kage\kage.chr`, `stage\tokyo_60\tokyo_60.gmd`…): los `.ng2` son
contenedores de esos archivos. Rutas de juego con `game:\` y caché con `cache:\$cache$`.

## Herramientas (`tools/`)

- Copiadas de GH: `xdvdfs.py`, `xex2.py`, `imports.py`, `find_helpers.py`*, `analyze.py`*, `stfs_extract.py`, `ppcdis.py`,
  `gap_functions.py`, `jt_finder.py`, `jt_patterns.py` (*adaptadas a secciones dinámicas; `gap_functions.py` y `jt_*` aún tienen rangos de GH).
- Nuevas: `pe_sections.py`, `name_imports.py`, `ppcdasm.py`.

## Próximos pasos (sesión 2)

1. Copiar el clon parcheado de XenonRecomp de GH (`G:\Guitar Hero Warrior of rock\Recomp\XenonRecomp` + `xenonrecomp_gh6.diff`)
   a `Recomp\XenonRecomp` y compilarlo aquí con el mismo toolchain (VS 2022 Build Tools + clang-cl de GH).
2. Ejecutar XenonRecomp con `config/ng2.toml`; usar su log para completar `functions` (adaptar `gap_functions.py` a
   las secciones de NG2) y los switches que fallen (`jt_finder.py`).
3. Decidir el tratamiento de las secciones `.embsec_`.
4. Proyecto CMake con `main` mínimo + stubs de los 227 imports y conseguir que el C++ generado compile
   (con 42.500 funciones y 23 MB de código, compilar puede ser lento: repartir en varios `.cpp` y usar Ninja).

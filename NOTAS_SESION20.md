# Sesión 20 — El pelo de Sonia (investigación)

Fecha: 2026-11-04/05. Sin arreglo todavía: solo diagnósticos nuevos (todos con variable de entorno, sin efecto por defecto).

## Resultado

- **Sonia sigue calva.** En Xenia, en la misma cinemática, tiene una melena larga rubio platino con flequillo
  (`run/xen1/a_062.png`, herramienta nueva `tools/xenia_drive.py`).
- Lo que NO es (descartado con pruebas):
  - ni prueba de alfa, ni eliminación de caras, ni `discard`, ni profundidad, ni el alfa de la textura;
  - ni el morph del VS 431ADCD7: sus 6 dibujos son la piel (5.564 y 5.934 índices) y el traje. La «cáscara» de la sesión 18
    era esa piel mal deformada, no el pelo. La sesión 19 la arregló bien;
  - ni las matrices de huesos de la GPU (búfer vf1, sin NaN);
  - ni shaders que falten: los 124 shaders que Xenia usa hasta esa escena tienen todos pareja en los nuestros;
  - el dibujo de 4.453 índices en dos pasadas (VS F8010105) es de Muramasa.
- **Lo que sí es:** el pelo son **tiras generadas por la CPU en cada frame** (VS 9D065E47 + PS BDFC3C17, `prim 6`,
  ~70-110 vértices de 32 bytes: posición float3, normal 10_11_11, UV half2). Las posiciones son pequeñas (±0,1, locales a la
  cabeza) y la matriz c21-c24 que el juego manda con ellas las deja en **y ≈ −1 de la pantalla, a la altura del origen del
  personaje** (detrás del mostrador), no en la cabeza. Falta la transformación del hueso de la cabeza.
- Quien escribe las tiras: **`sub_836ED380`** (← `sub_836ED768` ← `sub_836ED858` ← `sub_836EB2E0`, en un hilo de trabajo
  `sub_822F27B8`). Revisadas a mano sus instrucciones VMX (`vpermwi128`, `vrlimi128`, `vmsum3fp128`, `lvlx`, `vmaddfp`,
  `vnmsubfp`, `vsel`, `vslw`, `vcfsx`): todas correctas. El dato malo le llega de antes.

## Siguiente paso

1. Mirar qué matriz usa `sub_836EB2E0`/`sub_836ED858` para el anclaje (el hueso de la cabeza) y de dónde la lee. Hipótesis:
   - la lee de un búfer que rellena otra tarea del juego y que aquí aún está vacío o es la identidad (orden de las tareas o
     sincronización entre hilos distinta a la de la consola);
   - o una instrucción mal traducida en la cadena que calcula esa matriz.
2. Para comprobarlo: `NG2_WRITEWATCH` sobre esa matriz, o registrar los argumentos de `sub_836ED858` con un gancho.

## Diagnósticos nuevos (gpu_d3d12.cpp)

- `NG2_NODISCARD=1` (todos) o `=<hash PS>,...`: quita los `discard` de esos PS.
- `NG2_NOZFORN=<n>`: sin profundidad los draws de n índices. `NG2_SKIPN=<n>,...`: omite los draws de esos n índices.
- `NG2_VSCONSTN=<n>`: constantes del VS, búferes de vértices 95-93 e índices de hueso de los draws de n índices.
- `NG2_VSCONSTVS=<hash VS>`: c49/c200/c201/c255 y los búferes vf0-vf5 de los draws de ese VS.
- `NG2_NANLOG=1`: búferes de vértices con más de un 2 % de NaN/Inf (mucho ruido de datos empaquetados).
- `NG2_DYNVBLOG=1`: búferes de vértices que cambian de un frame a otro (así se encontró el pelo); para el VS del pelo da el
  tamaño de las tiras y c21-c24.
- `NG2_WRITEWATCH=B4575000:50000 NG2_WRITEWATCHDELAY=105` + `python tools/symsample.py logs/run.log`: quién escribe las tiras.

## Pruebas: ojo

- La partida de sistema `ng2sysd.dat` cambió el flujo de la cinemática (se saltaba). Se apartó a
  `run\userdata\respaldo_pruebas\sysd_2311\`; el juego crea una nueva al arrancar. El autoguardado de capítulo volvió a aparecer
  (apartado a `respaldo_pruebas\s20\`).
- `scripts/sonia2.txt` vuelve a llegar a la cinemática con la partida de sistema nueva; `scripts/sonia3.txt` es una variante con
  capturas entre los segundos 96 y 134.
- Xenia: para mandarle teclas hace falta `keyboard_mode = 1` en su configuración (se dejó otra vez en 0).

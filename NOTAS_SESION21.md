# Sesión 21 — El pelo de Sonia (investigación, segunda parte)

Fecha: 2026-11-05. Sin arreglo todavía: diagnósticos nuevos (todos con variable de entorno).

## Lo que cambia respecto a la sesión 20

La sesión 20 se equivocó en dos cosas:
- Las tiras de la CPU (VS 9D065E47) **no** están mal ancladas: la matriz c21-c24 es «anclaje × vista-proyección» y en el plano
  de la barra (148 s) caen sobre la cabeza. Son 11 mechones finos (cintas de ~1 cm) y en Xenia tienen **los mismos datos**
  (mismas direcciones 0x84B56580…, mismos puntos). La simulación funciona.
- La cabeza «calva» no es solo piel. En Xenia, Sonia lleva una **melena bob** platino con flequillo, y a **Muramasa** le falta el
  pelo blanco y la barba (`run/xen7/m.png`). Falta todo el pelo, no solo el de Sonia.

## Mapa del modelo de Sonia (búfer de vértices 0x144A9000, 11 630 vértices de 44 bytes)

`NG2_VBUSE=144A9000 NG2_VSCONSTMIN=147.9` lista los draws que lo usan:
- vértices **1384-4212 = la melena** (huesos 22-78), VS **E7323601** (4 huesos), draw de **6863** índices, opaco;
- 8555-9336 = la cara, VS E7323601, 1627 índices;
- 9337-11629 = casquete pegado al cráneo (solo hueso 4), VS F8010105, 4453 índices, dos pasadas (prueba de alfa y mezcla) con la
  textura del pelo 14067000 (DXT5, alfa correcto);
- el resto (cuerpo y traje), VS F8010105.

Calculado en la CPU (con el desensamblado de Xenia), la melena cae sobre la cabeza y mide ~17 cm: es el «casquete» liso que
vemos. **Los 57 huesos del pelo (22-78) tienen en la paleta la misma matriz que la cabeza (hueso 4)**, y 57 = la suma de los
puntos de los 11 mechones. Lo normal es que la simulación pase cada punto a su hueso y que la melena se abra con flequillo.

## Pista principal

Al final de `sub_836ED380` (el escritor de las tiras) hay `if (obj+124) sub_836F0F48(pelo, r29, obj+124, obj+12)`: en
nuestra versión **obj+124 = 0 y obj+12 = 0** en todos los mechones, así que esa llamada no se hace nunca. Puede ser la que
copia la simulación a los huesos 22-78. `obj+124` viene del asset (`sub_836ECA50`: obj+124 = *(asset+92)).

Siguiente:
1. Leer `sub_836F0F48` (¿escribe matrices de huesos?).
2. Ver en Xenia cuánto valen obj+12/obj+120/obj+124 (`tools/xenia_obj.py` con `tools/xenia_drive.py`, entre los segundos 60 y 66;
   requiere `keyboard_mode = 1` y que ninguna otra ventana le quite el foco). Si en Xenia no son 0, buscar quién los rellena.
3. Comparar la paleta de huesos 22-78 con la de Xenia.

## Herramientas nuevas

- `tools/xenia_drive.py`: `XMEM="64:MEM-<dir>-<bytes>"` lee memoria del invitado de Xenia (base del host 0x100000000; en
  0x82000000 está la cabecera MZ); `XMEM="64:PY=<script>"` ejecuta un script con el pid de Xenia.
- `tools/xsearch.py` (búsqueda de bytes en la memoria de Xenia), `tools/xenia_hair.py` (busca y decodifica los mechones),
  `tools/xenia_obj.py` (lee los mechones en sus direcciones fijas).
- Gancho `NG2_HAIRLOG=1` (hooks_ng2.cpp) en `sub_836ED768`: matriz de anclaje, tipo, puntos y campos de cada mechón.
- gpu_d3d12.cpp: `NG2_VBUSE`, `NG2_VSCONSTMIN` (desde el segundo N del proceso), `NG2_DYNVBMIN`; con `NG2_VSCONSTN` el VS
  F8010105 se evalúa en la CPU (posiciones en el mundo y en pantalla) y con `NG2_VBUSE` también el E7323601.

## Ojo

- Mientras otra ventana esté en primer plano (había un *Guitar Hero (recompilado)* abierto), Xenia no recibe las teclas y las
  pulsaciones pueden ir a esa ventana. No automatizar Xenia en ese caso.
- Los volcados de EDRAM (`NG2_DUMPTEXDRAW`) no incluyen la superficie de la escena (pitch 1120): no sirven para ver qué pinta un
  draw de la escena.

## Tercera parte (pista de los huesos del pelo)

- La paleta de huesos la escribe `sub_8232EA98` desde `sub_8247ECA0`. Los nodos del esqueleto de Sonia están en 0x84E78000
  (0x90 bytes por nodo, matriz de mundo al principio; volcado con `NG2_HAIRLOG=1` → `run/nodos_84E78000.bin`). Los nodos
  hijos de cada mechón tienen la **misma rotación que su raíz** (p. ej. 0x84E793F0 = 0x84E78820): pose de reposo, la simulación
  no les llega. Los escribe la actualización del esqueleto `sub_8247B2A8` (← `sub_8247A730`).
- Cada mechón crea un «conductor» de 20 bytes (`sub_836EDAF0`, vtable 0x82282CD0) colgado del nodo del hueso raíz
  (+112/+116 = lista). Sus métodos solo reenvían al siguiente de la lista; el tipo (entrada [10], `sub_836EDBA0`) es 4.
  `sub_836EC7F0` es una inicialización (12 llamadas en toda la partida, normal).
- La tabla de componentes del pelo (vtable 0x82282CB0): [4] `sub_836ED858` actualización, [5] `sub_836ED900` busca el nodo raíz,
  [6] `sub_836EDAF0` crea el conductor.
- `sub_8247B2A8` llama a `sub_836FA630(entrada+36, modelo)` para los modelos de una tabla (0x84C22380, 48 bytes por entrada).
  Se llama cada frame para un solo objeto (0x84BA7920, modelo 0x84C23DD8) y sale enseguida porque su estado (+12) es 0.
  **Forzarlo a 2 (`NG2_FORCEPHYS=1`) no cambia nada**: ese objeto no es el pelo de Sonia.
- `obj+124 = 0` en los mechones: `sub_836F0F48` (al final del escritor) toca vértices de malla, no huesos; puede ser normal.

Siguiente: encontrar dónde la actualización del esqueleto (`sub_8247B2A8`) trata los nodos con conductor de tipo 4 (la que
debería tomar los puntos simulados), y comparar los nodos 0x84E78000 con Xenia (`MEM-84E78000-2000` con `tools/xenia_drive.py`)
cuando no haya otra ventana en primer plano.

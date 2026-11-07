# Sesión 23 — Memoria que se agotaba y tirones de los cortes de escena

Fecha: 2026-11-06. El usuario pidió «mejora el juego todo, puedes darle 5 horas».

## 1. El juego se quedaba sin memoria a los ~6 minutos (arreglado)

- Síntoma (partida larga `scripts/largo.txt`, 18 min): a partir del minuto 6 la pantalla se queda negra con el juego a
  60 fps; el juego pasa de ~2500 draws por frame a 1. En el registro: `[mem] sin memoria de invitado (size=0x3000)`.
- Causa: el asignador del invitado (`kernel.cpp`) era de solo avance y `NtFreeVirtualMemory`, `MmFreePhysicalMemory` y
  `ExFreePool` no hacían nada. Cada liberación + nueva reserva del juego gastaba memoria nueva.
- Arreglo: montones (virtual 0x40000000-0x7EFF0000, física 0xA0000000-0xBFFF0000, kernel) con reservas registradas y lista de
  huecos ordenada y fusionada; primer hueco que quepa (con su alineación) y si no, se avanza el tope. `NtFreeVirtualMemory` solo
  libera con `MEM_RELEASE` (0x8000). `GuestFree()` en `runtime.h`. `NG2_NOGUESTFREE=1` = comportamiento antiguo.
  Cada minuto se registra `[mem] en uso: ...` por montón.
- Además, el juego consulta los logros ~1 vez por segundo (`XamUserCreateAchievementEnumerator`, 58 000 veces en 18 min):
  ya no reserva memoria por consulta y sus mensajes se limitan a los primeros.
- Comprobado: 18 min sin `sin memoria` y dibujando hasta el final (Ryu llega a la estatua de guardado).

## 2. Tirones de los cortes de escena (de 345 a ~100 ms)

Desglose nuevo en `[tiron]`: `texturas N ms (crear X ms xN, subir Y ms xN)`.
- **Crear texturas** era lo caro: 535 texturas nuevas en un corte = 116 ms de `CreateCommittedResource`. Ahora son recursos
  colocados en montones de 64 MB (`CreateTextureResource`; la caché de texturas nunca libera): 4-8 ms. `NG2_TEXCOMMITTED=1` = antes.
- **Destilear** se reparte por franjas de filas en un grupo fijo de hilos (`ParallelFor`, 6 hilos en un equipo de 12) para
  texturas de más de 32 768 bloques. Ejemplo: profundidad 1024×1024 (formato 23) de 188 ms a 34 ms. `NG2_TEXST=1` = un hilo.
- `DecodeDepth24` (20e4) con bits en vez de `ldexpf` (comprobado idéntico en los 2^24 valores).
- Aviso `[d3d12] subida lenta: ...` para subidas de más de 8 ms.
- Biblioteca de PSO en disco (`shadercache/pso.lib`, `ID3D12PipelineLibrary`): funciona pero no mejora nada (el controlador
  ya cachea); solo con `NG2_PSOLIB=1`.
- **Lectura de memoria de subida**: el destileado escribía cada texel en el anillo de subida (memoria de escritura combinada) y
  luego lo volvía a leer para convertirlo (profundidad 20e4, 16 bits → half). Ahora convierte en un búfer local y escribe una vez:
  la profundidad de 1024×1024 pasa de 34 ms a menos de 8 y desaparecen los avisos de subida lenta.
- Lo que queda del peor corte (**89 ms**, antes 345): ~25 ms de 61 PSO, ~25 ms de texturas y el resto del propio juego.
- El tirón de ~115 ms hacia el segundo 60 es la carga del capítulo del propio juego (pantalla «NOW LOADING»).

## 3. Filtrado anisótropo

- El muestreador s0 (lineal) pasa a anisótropo ×16 por defecto (`NG2_ANISO`, `anisotropico=` en ng2.ini; 0 = lineal como
  antes). Sin coste medible (60 fps igual) ni cambios en planos frontales.

## 4. Otras comprobaciones

- Bajada de fps: no es un fallo (cinemáticas a 30 fps a propósito; sesión 22).
- Volcado por frame de 046D0000: ya no aparece (0 volcados por frame en toda la partida).
- La distribución funciona desde una carpeta nueva (copia de `dist\NG2` con la ruta del ISO): llega a la cinemática.
- LOAD GAME con el autoguardado de capítulo funciona (`scripts/cargar.txt`). OPTIONS (ajustes y mando) y NINJA CINEMA se ven
  bien (`scripts/menus.txt`).
- Para pruebas repetibles: la partida de sistema (`ng2sysd.dat`) marca las cinemáticas como vistas y las pulsaciones de A del
  guion las saltan; se aparta a `run\userdata\respaldo_pruebas\sysd_s23*` antes de las pruebas.

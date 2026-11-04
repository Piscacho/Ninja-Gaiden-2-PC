# Sesión 13 — Menú principal y Mission Mode

Fecha: 2026-11-02. Sin commits.

## Resultado

- **Menú principal**: NEW GAME, LOAD GAME, LEADERBOARDS, NINJA CINEMA, OPTIONS, Xbox LIVE Marketplace (`run/montage47.png`).
  - Desde el título se entra con **A**. Con START el juego encadena título → NEW GAME → dificultad → partida, porque los menús
    también aceptan START y se ven pasar en cuanto se pulsa.
  - Guiones: `scripts/menu13b.txt` (menú y dificultad), `scripts/newgame.txt` (partida nueva), `scripts/loadgame.txt`.
- **LOAD GAME**: pantalla de partidas con las ranuras de autoguardado Acolyte / Warrior («Slot empty»), ranuras manuales,
  cambio de dispositivo, borrar y cargar (`run/montage48.png`). El juego enumera las partidas (tipo 1) en el disco duro.
- **Trajes DLC**: se montan al empezar partida (pantalla de elegir traje).

## Mission Mode: no es posible con el disco base

- El juego enumera los 4 paquetes, pero al buscar trajes monta solo 3. Se salta a propósito el elemento de Mission Mode
  (`0x84B03270` en el búfer enumerado), que no es un traje y no trae `message\dlc0N.bin`.
- El ejecutable base **no hace referencia** a ninguno de los archivos propios del paquete: `cmn\mes_mission.bin`,
  `actsel\g_mission.grp`, `sprite\game_mission`, `ranking_mission`, `replay_mission` y `spa.bin`. Solo conoce `mission.spr`,
  `s_missionNN_*.btl` y `g_venib`.
- Conclusión: Mission Mode necesita la **actualización del título** (su `default.xex` trae el código y la opción del menú).
  No está entre los originales (`Base\` solo tiene el ISO y los 4 DLC). Con una TU habría que recompilar su XEX (otro
  `basefile`, otras direcciones).

## Pendiente

- Jugar un capítulo para comprobar el autoguardado y la carga de partida.
- Pelo de Sonia, partículas, lanzador y distribución.

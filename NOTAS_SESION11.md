# Sesión 11 — Mando, teclado y DLC

Fecha: 2026-11-02. Sin commits.

## Resultado

- **Entrada lista para jugar** (`xam.cpp`):
  - Mando XInput con sticks y gatillos (ya venía de GH) y ahora con **vibración** (`XamInputSetState` → `XInputSetState`;
    `NG2_NOVIBRACION=1` la quita). El mando de PlayStation por HID sigue igual.
  - **Teclado para NG2** (solo con la ventana del juego en primer plano):
    | Tecla | Mando | | Tecla | Mando |
    |---|---|---|---|---|
    | WASD | stick izquierdo (moverse) | | Q | RT (bloquear) |
    | IJKL | stick derecho (cámara) | | Z | LT |
    | Espacio | A (saltar) | | C / V | LB / RB (ninpo) |
    | F | X (golpe rápido) | | Flechas | cruceta |
    | R | Y (golpe fuerte) | | Enter / Retroceso | START / BACK |
    | E o Esc | B (proyectil / cancelar) | | | |
  - El teclado manda sobre el stick físico solo en los ejes que use.
  - Guiones (`NG2_INPUTSCRIPT`): nuevos `LUP LDOWN LLEFT LRIGHT RUP RDOWN RLEFT RRIGHT LT RT` (p. ej. `265 LUP+RLEFT 3000`).
- **DLC detectados y cargados**: con los 4 paquetes en `<datos>\content\` (`run/userdata/content`, copia de `dlc/`), el juego
  los enumera (`XamContentCreateEnumerator` tipo 2 → 4 elementos), monta los tres trajes (Biometal, Fiend y Shadow Walker
  Hayabusa) como `dlc:` y lee `message\dlc0N.bin` y `sprite\costume_dlcN\*.spr`. En el juego final lo hace `ImportDlc` del
  lanzador (heredado de GH): copia de la carpeta `dlc` junto al exe a `userdata\content`.
- **Arreglo clave**: `NtQueryInformationFile` clase 6 (`FileInternalInformation`, identificador del archivo). El juego la pide al
  abrir cada archivo del DLC. Sin ella se quedaba en «NOW LOADING» con cuadros de mensaje repetidos y más de 100 montajes.
- `XMsgInProcessCall`: app 0xFA mensaje 0x70009 (`XMPGetStatus`, reproductor de música del sistema) devuelve «parado». Los
  mensajes sin implementar se registran (`[xmsg]`): app 0xFC (XGI) 0x58004 y 0x58046.
- El texto de los cuadros de mensaje se registra cada vez que cambia.

## Sin terminar

- **Mission Mode** (paquete F591AD78) no se monta: sin perfil, START en el título lleva directo a una partida nueva y no se ve el
  menú principal (donde estaría el modo).
- **Perfil con guardado**: con `NG2_SIGNIN=1` sigue en «NOW LOADING» en el título (igual que en la sesión 6). Tras START el juego
  solo hace por frame `XamUserGetSigninInfo` y `XMsgInProcessCall` XMP; no pide nada de contenido. Siguiente: el gestor de partidas
  `sub_83661650` (objeto `0x85309728`, campos `+0x12C/+0x130/+0x134`), los mensajes XGI 0x58004/0x58046 y comparar con
  Xenia con perfil iniciado.
- **Jugabilidad sin comprobar**: la cinemática de apertura dura unos 7 minutos y no se salta con START. Ryu aparece hacia los
  440 s de partida. Guion de prueba: `scripts/move2.txt`.
- El pelo de Sonia, las partículas y el lanzador siguen pendientes.

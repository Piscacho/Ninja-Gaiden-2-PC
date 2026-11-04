# Sesión 12 — Perfil y guardado

Fecha: 2026-11-02. Sin commits.

## Resultado

- **El guardado funciona, con perfil iniciado por defecto.** Flujo como en la consola:
  1. «Please choose the storage device…» → se elige el disco duro.
  2. «A NINJA GAIDEN II system preferences file was not found» → se crea.
  3. Se guarda `ng2sysd.dat` (2048 bytes) en `userdata\content\00000001_ng2sysd.dat\`, con «NOW SAVING…» en pantalla.
- En el arranque siguiente el juego **lee la partida del sistema** y muestra la **pantalla de elegir traje, con los trajes DLC**
  (`run/montage44.png`).
- `NG2_SIGNIN=0` vuelve a «sin perfil» (como antes: se responde «No, don't sign in» y no se guarda).

## Causa del atasco en «NOW LOADING» con perfil

Gestor de partidas `sub_83661650` (objeto `0x85309728`) y almacenamiento por usuario en `0x83B271B0` (4 × 0x6E0 bytes):
- Objeto: `+0x130` = petición en curso (12 = esperar a que termine la operación del usuario), `+0x134` = paso,
  `+0x12C` = tamaño (0x800). `sub_836676C0` («¿listo?») devuelve 1 cuando `+0x12C/+0x130/+0x134` vuelven a 0.
- Usuario: palabra 0 = estado de la capa de contenido del XDK (`sub_837DDF88` la inicializa a 0x00111110 | 1 o 4).
  Los bits 8-11 son el estado de la operación: 1 = libre, 2 = en curso, 4 = terminada. `+4` = escucha de notificaciones (máscara 1),
  `+8` = índice de usuario, `+0x14` = dispositivo, `+0x6DC` = estado de la máquina del usuario (6 = inactiva, 2 = arrancar).
- Al emitir una petición, el gestor llama a `sub_83660658(usuario)`. Si `+0x6DC == 6`, la pone a 2 y la operación arranca: selector
  de dispositivo, enumeración y creación del contenido, y luego bits 8-11 = 4. Después pasa a la petición 12 para esperar.
- En el recompilado, la inicialización de la estructura del usuario llega **después** de emitir la petición, la deja
  inactiva (`+0x6DC = 6`, operación = 1) y la petición 12 espera para siempre.
- Arreglo (`hooks_ng2.cpp`, gancho de `sub_83661650`): si la petición 12 está pendiente, la estructura ya está inicializada
  (palabra 0 ≠ 0), la operación está libre y el usuario inactivo, se relanza `sub_83660658(0)` (máximo 3 veces por partida).
  `NG2_NOSAVEFIX=1` lo quita; `NG2_SAVELOG=1` registra el estado cada 5 s.
- Pendiente de entender por qué la inicialización llega tarde (probablemente el orden de las notificaciones de inicio o el
  momento en que se crean las escuchas). El arreglo actual es un parche localizado.

## Herramientas y método

- Desensamblado: `python tools/ppcdasm.py xex/basefile.bin <dir> <n>` (sin `-I`: capstone está en el site del usuario).
- `NG2_DUMPMEM=dir:bytes:segundos;...` para seguir los campos del objeto en cada momento (ojo con volcar antes de tiempo).
- Comparar las importaciones con y sin perfil (`NG2_IMPTRACE=1`, `sort -u` de los nombres) señaló las diferencias.

## Pendiente

- Ver el menú principal completo (LOAD GAME, Mission Mode) y comprobar la carga de una partida de capítulo.
- Pelo de Sonia, partículas y lanzador o distribución.

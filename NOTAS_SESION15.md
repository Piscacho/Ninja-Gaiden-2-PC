# Sesión 15 — Primera partida jugada

Fecha: 2026-11-03. Sin commit de esta sesión todavía.

## Resultado

- **Se juega.** Con `scripts/jugar.txt` (NEW GAME → Acolyte → traje → cinemática de unos 7 min → combate):
  - Ryu en el tejado de la tienda de Muramasa, con la barra de vida y el tutorial (Block / Quick Attack / Strong Attack).
  - Se mueve con el stick izquierdo, ataca con X («2 HITS») y salta con A contra los ninjas de la Araña Negra.
  - Tras el combate llega la **siguiente cinemática** (Ryu y Muramasa en la tienda).
  - Casi 12 minutos sin caídas ni cuelgues (`run/montage50.png`, `run/montage51.png`).
- **Rendimiento**: 13 407 imágenes en 705 s (unos 19/s de media). En combate muchos frames tardan entre 40 y 135 ms
  (unos 10-25 fps). Jugable pero lento: es lo siguiente a mirar.

## Arreglo

- **Caída al empezar el combate** (`[switch] indice 308 fuera de la tabla` → llamada a 0x824215F4 sin traducir).
  Tres tablas de saltos **sin comprobación de rango** tenían mal el registro de índice en `config/ng2_switch_tables_all.toml`:
  | Tabla | TOML | Código |
  |---|---|---|
  | 0x824215C0 | r9 | r6 |
  | 0x824220F4 | r11 | r10 |
  | 0x8242443C | r6 | r10 |
  El código traducido leía el índice de otro registro (basura). Corregido y regenerado el C++ (`tools/run_recomp.py`).
- Herramienta nueva **`tools/jt_check_regs.py`**: compara el registro de cada tabla con la instrucción que escala el índice
  (`slwi/rlwinm r0, rN, 2`). Omite las tablas indexadas por byte (`lbzx`). Ahora da 0 errores; pasarla tras tocar las tablas.

## Pendiente

- Rendimiento en combate (perfilar: CPU traducida o envío de dibujos; `tools/symsample.py`).
- Pelo de Sonia, partículas, autoguardado de capítulo y carga.

# Sesión 19 — Personajes bien deformados (predicados anidados)

Fecha: 2026-11-04. Sin commit de esta sesión todavía.

## Resultado

- **Sonia se ve bien**: traje negro con correas y rayas, guantes, brazos y el pelo rubio recogido (`run/pelo10.png`). La «cáscara»
  beige de la sesión 18 no era el pelo: era la **malla del cuerpo mal deformada** alrededor de la cabeza.
- Combate a ~60 fps, sin cambios. Distribución `dist\NG2` regenerada con todo lo nuevo.

## Causa: `PRED_SETxx_PUSH` mal traducido (`gpu_shader.cpp`, operaciones vectoriales 20-23)

- Xenos: `setp_eq/ne/gt/ge_push d, a, b` → `p0 = (a.w == 0) && (b.w <op> 0)`; `d = p0 ? 0 : a.w + 1`. El primer operando es el
  **contador de anidamiento** de los predicados.
- Lo traducíamos como `p0 = (a.w <op> 0) && (b.w <op> 0)` y `d = 0`. El VS del cuerpo de los personajes (431ADCD73050DE5A:
  morph con varias posiciones `vf0`, `vf2`-`vf5` y pesos `c49`/`c200`, más matrices de huesos leídas de `vf1` con el índice del
  vértice) anida predicados (`setp_ne_push r7.w, r7.wwww, r8.zzzz`) y tomaba mal las ramas.
- El arreglo afecta a todos los shaders con esas instrucciones (personajes en general).

## Cómo se encontró

1. Pintar PS por grupos con `NG2_PSOUT` → la cáscara la dibuja el PS de personajes 985C5075.
2. Omitir VS con `NG2_SKIPSH` → sin el VS 431ADCD7 desaparece la cáscara (y debajo aparece la cabeza con el pelo); sin E7323601
   desaparece la piel de la cara.
3. Comparar el desensamblado de Xenia (`ref_xenia/shaders_dump/shader_38A9F8BB4FBAAA93.ucode.vert`) con nuestro HLSL
   (`NG2_HLSLDUMP=1 NG2_NOSHADERCACHE=1`, que tarda más en llegar a cada escena).

## Otros cambios

- `EnsureReadback()`: el búfer de lectura se crea también en el resolve de profundidad por CPU (antes solo en el de color por CPU;
  con todos los resolves de color por GPU, la copia de profundidad por CPU no se hacía).
- **Capturas internas negras** (`DUMP` de los guiones) en el título y los menús, aunque la ventana se veía bien. Causa: la captura
  lee la imagen presentada con `g_readback`, que no existía si ningún resolve iba por la CPU. Arreglado: `D3D12PresentResolved`
  llama a `EnsureReadback()`. Para comparar con la ventana real sirve `$TEMP/winshot.py` (copia de la idea de
  `tools/xenia_shot.py`).
- Las primeras partidas lanzadas justo después de compilar a veces no arrancan (registro vacío): repetirlas.
- `LEEME.txt` de la distribución: quitados el pelo y las partículas de los problemas conocidos; añadido Mission Mode.

## Pendiente

- Copia de región derivada con 8 bytes por píxel (una textura pequeña de 281×146 aún pasa por memoria).
- Seguir probando capítulos (jefes, fases nuevas).

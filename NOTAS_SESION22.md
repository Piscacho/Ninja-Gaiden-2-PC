# Sesión 22 — La «bajada de fps» de la transición

Fecha: 2026-11-06. Sin cambios de código.

## Resultado: no es un fallo

- `scripts/combate.txt` con `NG2_HITCHMS=25` (`logs/run_fps1.log`): el juego va a **60 fps en el juego** (115-155 s) y a
  **30 fps exactos en las cinemáticas con el motor** (85-110 s: créditos sobre la escena; 160-195 s: los ninjas atrapan a
  Sonia). Los fotogramas de 33,3 ms apenas trabajan (4-10 ms de texturas, sin esperas de la GPU ni compilaciones): es espera.
- El D3D del juego presenta con intervalo 2 en esos tramos (ver el comentario de `WAIT_REG_MEM` en `gpu_cp.cpp`): son 30 fps
  a propósito, como en la consola. La «caída a 44-48 fps» de las notas anteriores era la media de 5 s del `[fps]` al pasar de
  un tramo de 60 a uno de 30.
- En esas cinemáticas se suben en cada fotograma tres texturas de formato 2 (320×580 y dos de 160×290): planos Y/U/V de un
  vídeo superpuesto. Cuestan 4-10 ms por fotograma, dentro del presupuesto de 33 ms.

## Lo único mejorable

- Tirones puntuales en los cortes de escena: 345 ms (frame ~4059: 264 ms de texturas, 61 PSO nuevos) y 221 ms (frame ~4600:
  199 ms de texturas). Son cargas de texturas nuevas al cambiar de plano; se podrían repartir o acelerar (destileado/conversión
  en varios hilos), pero son de una vez por corte.

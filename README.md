# brunoKytyPs5

Fork del emulador de PlayStation 5 [KytyPS5](https://github.com/KytyPS5/KytyPS5), hecho a partir
de [BryKytyPS5](https://github.com/BryanKAdams/BryKytyPS5). Todo el crédito del emulador es de
esos proyectos y de [Kyty](https://github.com/InoriRus/Kyty), en el que se basan.

## Objetivo

Subir los cuadros por segundo en tarjetas **AMD RDNA 2** (Radeon RX 6000), la misma arquitectura
gráfica de la PS5.

| | |
| --- | --- |
| Equipo de pruebas | Radeon RX 6800 XT, Intel Core i7-14700KF, Windows 11 |
| Juego de referencia | ASTRO's PLAYROOM (PPSA01325) |
| Punto de partida | 34–43 fps en juego (GPU Jungle y CPU Plaza) |
| Ahora (0.2.0) | ≈50 fps en GPU Jungle, ≈52 fps en CPU Plaza |
| Meta | 60 fps estables |

Todo a 3840×2160, que es como dibuja el juego, y sin generación de cuadros.

## Estado

Versión **0.2.0**. Medido en la RX 6800 XT, con el mismo juego y en el mismo sitio antes y después:

| Zona | 0.1.0 | 0.2.0 | Qué limita ahora |
| --- | --- | --- | --- |
| GPU Jungle (hierba, al empezar) | 34 fps | ≈50 fps | El procesador |
| CPU Plaza | ≈52 fps | ≈52 fps | La tarjeta gráfica (≈18 ms por cuadro) |

De dónde sale la mejora:

- El juego emite unos 8000 dibujos por cuadro y un solo hilo los prepara todos. Más de la mitad
  van a un mapa de sombras de 16 capas, y por cada uno se recorría toda su tabla de páginas para
  volver a encontrarlo. Ahora se recuerda, igual que el búfer de profundidad con stencil.
- Las copias de memoria que acompañan a cada dibujo las hace el hilo que graba los comandos, que
  estaba casi siempre libre.
- El hilo que se adelanta a preparar los shaders ya no se duerme entre dibujos ni entre envíos.
- Los shaders que no escriben en sus búferes los declaran de solo lectura, lo que quita cerca de
  un 10 % de carga a la tarjeta.

Además:

- Generación de cuadros con AMD FSR 3, opcional y apagada por defecto. Suaviza el movimiento, no
  hace que el juego vaya más rápido.
- En el lanzador, cada juego tiene casillas para la generación de cuadros y para la lectura
  relajada, y ASTRO's PLAYROOM recibe sus ajustes recomendados la primera vez que aparece.

Falta para la meta: en la selva hay que seguir quitando trabajo al hilo que prepara los dibujos,
y en la plaza hay que aligerar la carga de la tarjeta.

## Cómo se trabaja

Se mide dónde se va el tiempo de cada cuadro, se cambia una sola cosa y se vuelve a medir en la
misma escena. Lo que no mejora la medición no se queda. Cada mejora se anotará aquí con sus
números.

También se irán incorporando los cambios recientes de KytyPS5 que ayuden al rendimiento.

## Compilar (Windows)

Hace falta Git, CMake, Ninja, Visual Studio Build Tools 2022 con clang-cl, y `glslangValidator`
en el `PATH`. Solo el emulador, sin el lanzador:

```powershell
git submodule update --init --recursive
cmake -S . -B _Build/windows-no-qt -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DKYTY_BUILD_LAUNCHER=OFF
cmake --build _Build/windows-no-qt --target kyty_emulator
```

El fork se desarrolla y se prueba solo en Windows.

## Más información

- Trabajo de rendimiento en AMD heredado de BryKytyPS5: [docs/performance-amd.md](docs/performance-amd.md)
- Documentación general del emulador: [KytyPS5](https://github.com/KytyPS5/KytyPS5)

## Licencia

GPL-2.0, como el proyecto original. Ver [LICENSE](LICENSE).

No está afiliado a Sony Interactive Entertainment ni a PlayStation. No distribuye juegos ni
software del sistema.

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
| Meta | 60 fps estables |

## Estado

Versión **0.1.0**: el código de BryKytyPS5 con el nombre de este fork. Todavía sin mejoras propias.

Lo medido hasta ahora en la RX 6800 XT:

- El límite es la tarjeta gráfica: ocupa unos 22.5 ms de cada cuadro.
- El juego dibuja siempre a 3840×2160, sea cual sea el tamaño de la ventana, y no baja su
  resolución aunque se le pida.
- Un solo shader de pantalla completa (`6b517f3b5e6b7160`) cuesta entre 5.4 y 6.7 ms por cuadro.

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

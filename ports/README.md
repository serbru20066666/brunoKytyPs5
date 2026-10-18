# Ports de PS5 para PC

Cada port es un paquete con un solo programa, `<Juego>.exe`: la página del juego (su imagen, sus ajustes principales,
**Jugar**) en el estilo de los ports de PS4, que arranca el motor de la carpeta oculta `motor` con lo que las pruebas
encontraron mejor.

| Port | Motor | Lo principal |
| --- | --- | --- |
| Demon's Souls (PPSA01340/01341/01342) | edición `demons-souls` (rama de este repositorio) | precompilación de shaders con progreso, generación de cuadros FSR 3, resolución 3D, calidad |
| EA SPORTS FC 27 (PPSA34015) | brunoKytyPs5 | resolución según la memoria de vídeo, núcleos de rendimiento, contador de FPS |
| ASTRO's PLAYROOM (PPSA01325) | brunoKytyPs5 | parches de resolución interna (1440p, 1080p, 4K), FSR, contador de FPS |

- `launcher/`: el programa (C++20, SDL 3 y Dear ImGui, un solo exe). Compilar: `cmake -S ports/launcher -B ports/launcher/_Build
  -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl`, luego
  `cmake --build ports/launcher/_Build`.
- `empaquetar_ports_ps5.ps1`: arma un paquete (`-Publico -Zip` para una release: sin imágenes de los editores, sin
  partidas, sin cachés ni código de shaders del juego). Lo propio de cada PC (rutas, ajustes privados) va en un
  `empaquetar_local.ps1` junto al script, que no se sube. `recursos_exe.ps1` pone el icono y el nombre al exe.
- `juego.ini` (junto al exe) dice qué juego es (`perfil`), dónde buscar su carpeta (`buscar`, relativo al paquete) y
  qué variables solo para ese paquete lleva el motor (`entorno`).

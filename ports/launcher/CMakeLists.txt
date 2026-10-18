cmake_minimum_required(VERSION 3.24)
project(brunoPortLauncher VERSION 1.0.0 LANGUAGES C CXX)

# The launcher of a PC port of a PS5 game: one window with the game's picture, its main settings and the play button.
# One program for every port; each package names it after its game and says in juego.ini which game it is.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# SDL3 built in, as a static library: the package's program is one file.
# Inside brunoKytyPs5 (ports/launcher) its SDL3 submodule; alone, the one of brunoKytyPs5 next to it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/SDL3/CMakeLists.txt")
    set(SDL3_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/SDL3")
else()
    set(SDL3_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../brunoKytyPs5/3rdparty/SDL3")
endif()
set(SDL3_SOURCE_DIR "${SDL3_DEFAULT}" CACHE PATH "SDL3 sources")
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG ON CACHE BOOL "" FORCE)
add_subdirectory("${SDL3_SOURCE_DIR}" sdl3 EXCLUDE_FROM_ALL)

set(IMGUI third_party/imgui)
add_library(imgui STATIC
    ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_sdl3.cpp ${IMGUI}/backends/imgui_impl_sdlrenderer3.cpp)
target_include_directories(imgui PUBLIC ${IMGUI} ${IMGUI}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_DEMO_WINDOWS)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)

add_executable(brunoPortLauncher WIN32
    src/main.cpp src/juego.cpp src/sistema.cpp src/ui.cpp)
target_include_directories(brunoPortLauncher PRIVATE src third_party/stb)
target_link_libraries(brunoPortLauncher PRIVATE imgui SDL3::SDL3-static)
target_compile_definitions(brunoPortLauncher PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
                           LAUNCHER_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(brunoPortLauncher PRIVATE /W3 /utf-8)
    target_compile_options(imgui PRIVATE /utf-8)
endif()

cmake_minimum_required(VERSION 3.25)
project(lineage_cliente LANGUAGES C CXX)

# C++20 en estilo "C con plantillas": sin excepciones ni RTTI (ver analisis/PLAN_CLIENTE.md §2.2)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LIN_ASAN "Compilar con AddressSanitizer y UBSan" OFF)
option(LIN_TESTS "Compilar los tests" ON)
# Control en vivo para pruebas (src/app/control.cpp): ON en los presets de desarrollo, OFF en release.
option(LIN_CONTROL "Compilar el control en vivo (socket de pruebas)" OFF)

# Dependencias antes de las opciones de advertencias: estas aplican solo a nuestro código.
include(cmake/dependencias.cmake)

if(MSVC)
    # MULTIPLATAFORMA (Windows): equivalentes de -fno-exceptions / -fno-rtti en MSVC
    add_compile_options(/W4 /EHs-c- /GR- /utf-8)
    add_compile_definitions(_HAS_EXCEPTIONS=0)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti)
endif()

if(LIN_ASAN AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()


# datos/ (tabla de sprites) junto a los ejecutables
file(COPY ${CMAKE_SOURCE_DIR}/datos DESTINATION ${CMAKE_BINARY_DIR})

# --- Módulos (librerías estáticas). Solo plataforma, render y audio ven SDL3. -----------------------
add_library(lin_core INTERFACE)
target_include_directories(lin_core INTERFACE src)

add_library(lin_plataforma STATIC
    src/plataforma/archivo_mapeado.cpp
    src/plataforma/socket.cpp
    src/plataforma/rutas.cpp)
target_link_libraries(lin_plataforma PUBLIC lin_core PRIVATE SDL3::SDL3)

add_library(lin_proto STATIC
    src/proto/cifrado.cpp
    src/proto/opcodes.cpp
    src/proto/decodificar.cpp
    src/proto/comandos.cpp
    src/proto/hilo_red.cpp)
target_link_libraries(lin_proto PUBLIC lin_core lin_plataforma)

add_library(lin_recursos STATIC
    src/recursos/des.cpp
    src/recursos/pak.cpp
    src/recursos/imagen.cpp
    src/recursos/tiles.cpp
    src/recursos/sprites.cpp
    src/recursos/tabla_sprites.cpp
    src/recursos/mapa.cpp
    src/recursos/fuente.cpp
    src/recursos/cp949.cpp
    src/recursos/textos.cpp
    src/recursos/precarga.cpp
    src/recursos/hechizos.cpp
    src/recursos/html.cpp
    src/recursos/sonidos.cpp)
target_link_libraries(lin_recursos PUBLIC lin_core lin_plataforma PRIVATE stb)

add_library(lin_mundo STATIC
    src/mundo/cargador_segmentos.cpp
    src/mundo/chat.cpp
    src/mundo/mundo.cpp
    src/mundo/inventario.cpp)
target_link_libraries(lin_mundo PUBLIC lin_core lin_proto lin_recursos)

add_library(lin_render STATIC
    src/render/atlas.cpp
    src/render/gpu.cpp
    src/render/graficos.cpp)
target_link_libraries(lin_render PUBLIC lin_core lin_recursos SDL3::SDL3)

add_library(lin_audio STATIC
    src/audio/audio.cpp)
target_link_libraries(lin_audio PUBLIC lin_core PRIVATE SDL3::SDL3 dr_mp3)

add_library(lin_ui STATIC
    src/ui/ui.cpp)
target_link_libraries(lin_ui PUBLIC lin_core lin_render)

add_executable(lineage
    src/app/main.cpp
    src/app/app.cpp
    src/app/pantalla_login.cpp
    src/app/pantalla_personajes.cpp
    src/app/pantalla_crear.cpp
    src/app/pantalla_juego.cpp
    src/app/inventario_ui.cpp
    src/app/hechizos_ui.cpp
    src/app/barra_ui.cpp
    src/app/forma_ui.cpp
    src/app/npc_ui.cpp
    src/app/ventanas.cpp
    src/app/cursores.cpp
    src/app/social_ui.cpp
    src/app/sonido.cpp
    src/app/clima.cpp
    src/app/cantidad_ui.cpp
    src/app/correo_ui.cpp
    src/app/casas_ui.cpp
    src/app/castillos_ui.cpp
    src/app/equipo_ui.cpp
    src/app/reinicio_ui.cpp
    src/app/tablon_ui.cpp
    src/app/tienda_ui.cpp
    src/app/mascotas_ui.cpp
    src/app/marcadores_ui.cpp
    src/core/asignaciones.cpp)  # operator new instrumentado (ver core/asignaciones.h)
target_link_libraries(lineage PRIVATE lin_mundo lin_ui lin_audio)  # traen proto, recursos, render y SDL3
if(LIN_CONTROL)
    target_sources(lineage PRIVATE src/app/control.cpp)
    target_compile_definitions(lineage PRIVATE LIN_CONTROL=1)
else()
    target_compile_definitions(lineage PRIVATE LIN_CONTROL=0)
    # La versión final no debe llevar el control: ni el código ni el texto del socket.
    add_custom_command(TARGET lineage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DBIN=$<TARGET_FILE:lineage> -P ${CMAKE_SOURCE_DIR}/cmake/sin_control.cmake
        COMMENT "Comprobando que el ejecutable no incluya el control en vivo")
endif()
if(APPLE)
    # MULTIPLATAFORMA: en macOS se compila como ejecutable de consola para depurar;
    # el empaquetado .app (MACOSX_BUNDLE, Info.plist, icono) queda para el entregable 11.
    target_compile_definitions(lineage PRIVATE LIN_MACOS=1)
endif()

add_executable(explorador tools/explorador.cpp)
target_link_libraries(explorador PRIVATE lin_ui)

if(LIN_TESTS)
    enable_testing()
    add_executable(lin_tests
        tests/main.cpp
        tests/test_core.cpp
        tests/test_cifrado.cpp
        tests/test_des.cpp
        tests/test_proto.cpp
        tests/test_recursos.cpp
        tests/test_mundo.cpp
        tests/test_chat.cpp
        tests/test_atlas.cpp
        tests/test_asignaciones.cpp
        tests/test_hechizos.cpp
        tests/test_npc.cpp
        tests/test_sonidos.cpp
        src/core/asignaciones.cpp)
    target_link_libraries(lin_tests PRIVATE lin_mundo lin_render doctest)
    add_test(NAME lin_tests COMMAND lin_tests)
endif()

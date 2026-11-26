cmake_minimum_required(VERSION 3.25)
project(lineage_cliente LANGUAGES C CXX)

# C++23 en estilo "C con plantillas": sin excepciones ni RTTI (ver analisis/PLAN_CLIENTE.md §2.2)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LIN_ASAN "Compilar con AddressSanitizer y UBSan" OFF)
option(LIN_TESTS "Compilar los tests" ON)
# Avisos como errores (solo nuestro código; las dependencias se agregan antes). El CI lo activa.
option(LIN_AVISOS_ERROR "Tratar los avisos del compilador como errores" OFF)
# Control en vivo para pruebas (src/app/control.cpp): ON en los presets de desarrollo, OFF en release.
option(LIN_CONTROL "Compilar el control en vivo (socket de pruebas)" OFF)
# Para todo el código: también decide LIN_CONFIG (otro settings.ini), que solo existe en desarrollo.
if(LIN_CONTROL)
    add_compile_definitions(LIN_CONTROL=1)
else()
    add_compile_definitions(LIN_CONTROL=0)
endif()

# Versión del paquete: la pone el empaquetado (herramientas/empaquetar_*); en Windows va en el .exe.
set(LIN_VERSION "0.0.0" CACHE STRING "Versión del paquete (X.Y.Z, admite un sufijo: 0.2.0-abc1234)")

# MSVC: runtime de C++ estático (/MT), para todo (también SDL3): el .exe no necesita el redistribuible de
# Visual C++ instalado. herramientas/empaquetar_windows.ps1 comprueba que no dependa de sus DLL.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Dependencias antes de las opciones de advertencias: estas aplican solo a nuestro código.
include(cmake/dependencias.cmake)

if(MSVC)
    # Equivalentes de -fno-exceptions / -fno-rtti. CMake pone /EHsc por omisión: se saca para que no
    # choque con /EHs-c- (aviso D9025 en cada archivo).
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    # C4324: relleno por alignas (buscado, en los anillos y estructuras por hilo).
    add_compile_options(/W4 /EHs-c- /GR- /utf-8 /wd4324)
    # _CRT_SECURE_NO_WARNINGS: getenv, fopen... son las funciones estándar que usa el código portable.
    add_compile_definitions(_HAS_EXCEPTIONS=0 _CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti)
endif()
if(LIN_AVISOS_ERROR)
    if(MSVC)
        add_compile_options(/WX)
    else()
        add_compile_options(-Werror)
    endif()
endif()

if(LIN_ASAN AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()


# datos/ (tabla de sprites) junto a los ejecutables
file(COPY ${CMAKE_SOURCE_DIR}/datos DESTINATION ${CMAKE_BINARY_DIR})
file(GLOB_RECURSE ARCHIVOS_DATOS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/datos/*)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ARCHIVOS_DATOS})

# --- Módulos (librerías estáticas). Solo plataforma, render y audio ven SDL3. -----------------------
add_library(lin_core INTERFACE)
target_include_directories(lin_core INTERFACE src)

add_library(lin_plataforma STATIC
    src/plataforma/archivo_mapeado.cpp
    src/plataforma/lanzador.cpp
    src/plataforma/socket.cpp
    src/plataforma/rutas.cpp
    src/i18n/textos.cpp)
target_link_libraries(lin_plataforma PUBLIC lin_core PRIVATE SDL3::SDL3)
if(WIN32)
    target_link_libraries(lin_plataforma PUBLIC ws2_32)  # Winsock2 (plataforma/socket.cpp)
endif()

add_library(lin_proto STATIC
    src/proto/cifrado.cpp
    src/proto/opcodes.cpp
    src/proto/decodificar.cpp
    src/proto/comandos.cpp
    src/proto/hilo_red.cpp)
target_link_libraries(lin_proto PUBLIC lin_core lin_plataforma)

add_library(lin_recursos STATIC
    src/recursos/aes.cpp
    src/recursos/des.cpp
    src/recursos/pak.cpp
    src/recursos/pe_resources.cpp
    src/recursos/imagen.cpp
    src/recursos/hd_pack.cpp
    src/recursos/tiles.cpp
    src/recursos/sprites.cpp
    src/recursos/tabla_sprites.cpp
    src/recursos/lista_sprites.cpp
    src/recursos/mapa.cpp
    src/recursos/fuente.cpp
    src/recursos/fuente_hd.cpp
    src/recursos/fuente_ttf.cpp
    src/recursos/cp949.cpp
    src/recursos/big5.cpp
    src/recursos/textos.cpp
    src/recursos/zonas.cpp
    src/recursos/precarga.cpp
    src/recursos/hechizos.cpp
    src/recursos/html.cpp
    src/recursos/sonidos.cpp)
target_link_libraries(lin_recursos PUBLIC lin_core lin_plataforma PRIVATE stb)

add_library(lin_mundo STATIC
    src/mundo/autobuffs.cpp
    src/mundo/autopocion.cpp
    src/mundo/cargador_segmentos.cpp
    src/mundo/chat.cpp
    src/mundo/clases.cpp
    src/mundo/luz.cpp
    src/mundo/mundo.cpp
    src/mundo/inventario.cpp)
target_link_libraries(lin_mundo PUBLIC lin_core lin_proto lin_recursos)

add_library(lin_render STATIC
    src/render/atlas.cpp
    src/render/gpu.cpp
    src/render/gpu_anime4k.cpp
    src/render/graficos.cpp
    src/render/icon_aura.cpp)
target_link_libraries(lin_render PUBLIC lin_core lin_recursos SDL3::SDL3)
if(WIN32)
    # D3D12 shaders: HLSL -> DXIL (dxc) -> generado/shaders.h (cmake/shaders.cmake).
    include(cmake/shaders.cmake)
    target_sources(lin_render PRIVATE ${LIN_SHADERS_H})
    target_include_directories(lin_render PRIVATE ${LIN_SHADERS_DIR})
endif()

add_library(lin_audio STATIC
    src/audio/audio.cpp)
target_link_libraries(lin_audio PUBLIC lin_core lin_plataforma PRIVATE SDL3::SDL3 dr_mp3)

add_library(lin_ui STATIC
    src/ui/ui.cpp)
target_link_libraries(lin_ui PUBLIC lin_core lin_render)

add_executable(lineage
    src/app/cartel_ui.cpp
    src/app/cocina_ui.cpp
    src/app/main.cpp
    src/app/ajustes_ui.cpp
    src/app/app.cpp
    src/app/pantalla_login.cpp
    src/app/pantalla_personajes.cpp
    src/app/pantalla_crear.cpp
    src/app/pantalla_juego.cpp
    src/app/inventario_ui.cpp
    src/app/item_icon.cpp
    src/app/hechizos_ui.cpp
    src/app/barra_ui.cpp
    src/app/autobuffs_ui.cpp
    src/app/autopocion_ui.cpp
    src/app/forma_ui.cpp
    src/app/npc_ui.cpp
    src/app/pesca_ui.cpp
    src/app/original_window.cpp
    src/app/ventanas.cpp
    src/app/cursores.cpp
    src/app/social_ui.cpp
    src/app/float_hpmp_ui.cpp
    src/app/volume_ui.cpp
    src/app/sonido.cpp
    src/app/clima.cpp
    src/app/lista_ui.cpp
    src/app/luz_ui.cpp
    src/app/ventana_ui.cpp
    src/app/cantidad_ui.cpp
    src/app/correo_ui.cpp
    src/app/casas_ui.cpp
    src/app/castillos_ui.cpp
    src/app/equipo_ui.cpp
    src/app/reinicio_ui.cpp
    src/app/tablon_ui.cpp
    src/app/tienda_ui.cpp
    src/app/map_ui.cpp
    src/app/mascotas_ui.cpp
    src/app/marcadores_ui.cpp
    src/core/asignaciones.cpp)  # operator new instrumentado (ver core/asignaciones.h)
target_link_libraries(lineage PRIVATE lin_mundo lin_ui lin_audio)  # traen proto, recursos, render y SDL3
if(LIN_CONTROL)
    target_sources(lineage PRIVATE src/app/control.cpp)
else()
    # La versión final no debe llevar el control: ni el código ni el texto del socket.
    add_custom_command(TARGET lineage POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DBIN=$<TARGET_FILE:lineage> -P ${CMAKE_SOURCE_DIR}/cmake/sin_control.cmake
        COMMENT "Comprobando que el ejecutable no incluya el control en vivo")
endif()
# Windows: ícono y versión del .exe (cmake/windows.rc.in) y, en la versión final, subsistema GUI, sin
# ventana de consola (SDL_main.h pone el WinMain). En desarrollo queda de consola para ver el log.
function(lin_windows objetivo descripcion icono)
    if(NOT WIN32)
        return()
    endif()
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${LIN_VERSION}")
    if(CMAKE_MATCH_COUNT EQUAL 3)
        set(RC_VERSION_COMAS "${CMAKE_MATCH_1},${CMAKE_MATCH_2},${CMAKE_MATCH_3},0")
    else()
        set(RC_VERSION_COMAS "0,0,0,0")
    endif()
    set(RC_EJECUTABLE ${objetivo})
    set(RC_DESCRIPCION "${descripcion}")
    set(RC_ICONO "${CMAKE_SOURCE_DIR}/datos/iconos/${icono}.ico")
    configure_file(${CMAKE_SOURCE_DIR}/cmake/windows.rc.in ${CMAKE_BINARY_DIR}/generado/${objetivo}.rc @ONLY)
    target_sources(${objetivo} PRIVATE ${CMAKE_BINARY_DIR}/generado/${objetivo}.rc)
    if(NOT LIN_CONTROL)
        set_target_properties(${objetivo} PROPERTIES WIN32_EXECUTABLE ON)
    endif()
endfunction()
lin_windows(lineage "Lineage" lineage)
if(APPLE)
    # MULTIPLATAFORMA: en macOS se compila como ejecutable de consola para depurar;
    # el empaquetado .app (MACOSX_BUNDLE, Info.plist, icono) queda para el entregable 11.
    target_compile_definitions(lineage PRIVATE LIN_MACOS=1)
endif()

# Instalación (Linux, BSD y macOS; `cmake --install <build> --prefix ~/.local`, o `ninja install` / `make install`):
#   <prefix>/share/lineage-cliente/lineage y datos/ (el cliente busca datos/ junto a su ejecutable),
#   <prefix>/bin/lineage (un lanzador hacia el anterior) y, fuera de macOS, el acceso directo y el ícono del escritorio.
# Windows y el .app de macOS se arman con herramientas/empaquetar_*.
if(NOT WIN32)
    include(GNUInstallDirs)
    set(LIN_DIR_INSTALADO "${CMAKE_INSTALL_DATADIR}/lineage-cliente")
    install(TARGETS lineage RUNTIME DESTINATION "${LIN_DIR_INSTALADO}")
    install(DIRECTORY datos DESTINATION "${LIN_DIR_INSTALADO}")
    install(PROGRAMS herramientas/linux/lineage.sh DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME lineage)
    if(NOT APPLE)
        install(FILES herramientas/linux/lineage.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
        install(FILES herramientas/linux/lineage.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps")
    endif()
endif()

add_executable(explorador tools/explorador.cpp)
target_link_libraries(explorador PRIVATE lin_ui)

if(LIN_TESTS)
    enable_testing()
    add_executable(lin_tests
        tests/main.cpp
        tests/test_core.cpp
        tests/test_cifrado.cpp
        tests/test_aes.cpp
        tests/test_des.cpp
        tests/test_proto.cpp
        tests/test_recursos.cpp
        tests/test_muerte_ogro.cpp
        tests/test_lista_sprites.cpp
        tests/test_pe_resources.cpp
        tests/test_mundo.cpp
        tests/test_chat.cpp
        tests/test_clases.cpp
        tests/test_atlas.cpp
        tests/test_icon_aura.cpp
        tests/test_hd_pack.cpp
        tests/test_asignaciones.cpp
        tests/test_hechizos.cpp
        tests/test_luz.cpp
        tests/test_npc.cpp
        tests/test_sonidos.cpp
        tests/test_fuente.cpp
        tests/test_fuente_hd.cpp
        tests/test_textos.cpp
        tests/test_ajustes.cpp
        tests/test_vista.cpp
        tests/test_autobuffs.cpp
        tests/test_autopocion.cpp
        tests/test_ataque.cpp
        tests/test_socket.cpp
        tests/test_gpu.cpp
        src/core/asignaciones.cpp)
    target_link_libraries(lin_tests PRIVATE lin_mundo lin_render doctest)
    add_test(NAME lin_tests COMMAND lin_tests)
endif()

# Dependencias vendorizadas con FetchContent y fijadas a una versión exacta.
# No hace falta instalar nada fuera del compilador y CMake.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# SDL3: ventana, entrada, audio, GPU (Metal en macOS, Vulkan en Linux, D3D12 en Windows)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE)

# stb: solo stb_image (PNG de la interfaz original)
FetchContent_Declare(stb_src
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5)

# doctest: tests
FetchContent_Declare(doctest_src
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR sin_cmake)  # solo se usa el header; su CMakeLists no es compatible con CMake 4

FetchContent_MakeAvailable(SDL3 stb_src doctest_src)

# stb no trae CMakeLists: se expone como librería de solo headers
add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${stb_src_SOURCE_DIR})

# doctest: solo header; con -fno-exceptions se activa su modo sin ellas
add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE ${doctest_src_SOURCE_DIR})
target_compile_definitions(doctest INTERFACE DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)

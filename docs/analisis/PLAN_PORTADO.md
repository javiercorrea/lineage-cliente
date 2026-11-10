# Plan de portado: Windows (x64 y arm64) y Linux (x64 y arm64)

Estado al escribirlo (2026-11-09): el cliente funciona solo en macOS (Apple Silicon y, desde el
empaquetado, también Intel). Lo que falta está marcado en el código con `MULTIPLATAFORMA`
(`grep -rn MULTIPLATAFORMA src tools CMakeLists.txt cmake`). Este plan ordena ese trabajo en fases que se
pueden mergear por separado; cada una deja `main` compilando y con los tests en verde en macOS.

**Decisión:** compilar en las máquinas nativas de GitHub Actions (`windows-latest`, `windows-11-arm`,
`ubuntu-latest`, `ubuntu-24.04-arm`), no cruzar desde el Mac. Así se corren los tests en cada plataforma.
La compilación cruzada (mingw-w64 o llvm-mingw para Windows, Docker para Linux) queda como opción local
para probar rápido, no para las releases.

## Fase 0: CI de compilación en las 4 plataformas (medio día)

Antes de portar nada, ver dónde rompe. Un workflow nuevo `.github/workflows/compilar.yml` (en cada PR y
en `main`) con una matriz:

| Job | Runner | Compilador |
|---|---|---|
| macOS | `macos-latest` | clang (ya funciona) |
| Linux x64 | `ubuntu-latest` | gcc o clang; paquetes de SDL3: `libx11-dev libxext-dev libwayland-dev libxkbcommon-dev libasound2-dev libpulse-dev libvulkan-dev` |
| Linux arm64 | `ubuntu-24.04-arm` | igual |
| Windows x64 | `windows-latest` | MSVC (`ilammy/msvc-dev-cmd`) o clang-cl, con Ninja |
| Windows arm64 | `windows-11-arm` | igual, `-A ARM64` / `vcvarsall arm64` |

- Pasos: `cmake --preset debug`, `cmake --build`, `lin_tests` (sin `LINEAGE_DIR` se saltan los que
  necesitan los recursos del juego). Empezar con `continue-on-error: true` en los 4 nuevos.
- **Terminado cuando:** la matriz corre y muestra los errores reales de cada plataforma.

## Fase 1: shader en HLSL → SPIR-V y DXIL (la más grande: 1 a 2 días)

Es lo que desbloquea Windows y Linux a la vez: hoy `src/render/gpu.cpp` tiene un solo shader en MSL
(`SHADER_MSL`, con 6 funciones: `vs_sprite`/`fs_sprite`, `vs_brillo`/`fs_brillo` y `vs_mundo`/`fs_mundo`) y
crea el dispositivo con `SDL_GPU_SHADERFORMAT_MSL` (línea ~134).

1. Escribir los 6 shaders en HLSL, en `src/render/shaders/*.hlsl`, uno por etapa. Respetar el contrato de
   recursos de SDL_GPU (ver la documentación de `SDL_CreateGPUShader`):
   - vértice: storage buffers en `t[n], space0`, uniformes en `b[n], space1`;
   - fragmento: texturas y samplers en `t[n]/s[n], space2`, uniformes en `b[n], space3`.
   Mantener las mismas semánticas que el MSL: alfa premultiplicado, `discard` con alfa 0, mezcla aditiva
   con alfa de salida 0 (`fs_sprite`), el brillo por celda y el pase de multiplicar (`fs_mundo`).
2. Compilarlos a SPIR-V, DXIL **y MSL** con **SDL_shadercross** (`shadercross x.hlsl -o x.spv`), como paso
   de CMake (`add_custom_command`) o una vez y guardarlos generados en `src/render/shaders/generados/`
   como arreglos `const unsigned char[]`. Recomendado: generarlos en CMake y no versionarlos.
   - SDL_shadercross necesita DirectXShaderCompiler (DXC) y SPIRV-Cross; en CI usar su release binaria.
   - Alternativa sin shadercross: `dxc -spirv` para SPIR-V y `dxc` para DXIL, más el MSL actual a mano.
3. En `Gpu::iniciar`: pedir `MSL | SPIRV | DXIL`, ver qué eligió SDL con `SDL_GetGPUShaderFormats(dev_)` y
   pasar el código y el `entrypoint` de ese formato (en SPIR-V/DXIL suele ser `main`).
4. Reemplazar el MSL escrito a mano por el generado desde el HLSL, para tener una sola fuente.
- **Verificar:** en macOS, capturas antes y después iguales píxel a píxel (o casi) en login, pueblo de
  noche (luz), agua y efectos aditivos (braseros de Giran, gfx 1171). `tests/test_asignaciones.cpp` debe
  seguir en verde.
- **Terminado cuando:** macOS se ve igual con el shader generado y el job de Linux dibuja (Fase 3).

## Fase 2: Windows (1 día)

Archivos chicos y aislados; hoy son un `#error`.

1. **`src/plataforma/socket.cpp` / `socket.h`** (86 líneas): Winsock2.
   - `WSAStartup(MAKEWORD(2, 2))` una vez (en `Socket::conectar` con un `static`, o en `main`);
   - `SOCKET` en vez de `int` (`socket.h:19`), `INVALID_SOCKET`, `closesocket`, `ioctlsocket` para no
     bloquear, `WSAGetLastError()`; `send` sin `MSG_NOSIGNAL` (Windows no tiene SIGPIPE).
   - El hilo lector y el escritor no cambian.
2. **`src/plataforma/archivo_mapeado.cpp`** (78 líneas): `CreateFileW` + `CreateFileMappingW` +
   `MapViewOfFile` (y `UnmapViewOfFile`/`CloseHandle` al cerrar); rutas UTF-8 → UTF-16 con
   `MultiByteToWideChar`; `PrefetchVirtualMemory` en vez de `madvise` (línea 55); `_wfopen` en
   `leer_archivo` (línea 67).
3. **`CMakeLists.txt:25`:** banderas de MSVC equivalentes a `-fno-exceptions -fno-rtti` (`/EHs-c- /GR-`) y
   `/W4`; linkear `ws2_32`. Revisar los avisos que trate como errores.
4. **`src/app/control.cpp:14`** (solo `debug`): AF_UNIX existe desde Windows 10 1803 (`afunix.h`,
   `closesocket`, `ioctlsocket`). Si complica, compilar Windows sin `LIN_CONTROL` al principio.
5. Revisar `main` en Windows: subsistema GUI (`WIN32_EXECUTABLE`) para que no abra una consola en release.
- **Verificar:** en una VM o PC Windows, login contra el servidor local y caminar; `lin_tests` en CI.

## Fase 3: Linux (medio día)

1. **Mayúsculas en rutas** (Linux distingue; macOS y Windows no): una función en `plataforma/rutas.cpp`
   (ya hay un comentario en la línea 230) que, si `carpeta/Nombre` no existe, busque en el directorio una
   entrada igual ignorando mayúsculas. Usarla en:
   - `Recursos::abrir` (`Sprite*.pak/.idx`, `Text.pak`, `Tile.pak`);
   - `src/mundo/mundo.cpp:532` (carpeta `map` / `Map`);
   - `src/audio/audio.cpp:175` (sonidos y música);
   - `carpeta_juego_valida`.
2. Barra de ventana propia (`src/ui/ui.cpp:197`, `ui.h:92`): botones a la derecha en Linux y Windows; ya
   está previsto, falta probarlo.
3. `SDL_GetPrefPath` ya da `~/.local/share` (`rutas.h:70`): solo verificar dónde queda `settings.ini`.
- **Verificar:** en CI `lin_tests`; a mano, en una VM Ubuntu con Vulkan (o `lavapipe`), login y caminar.

## Fase 4: empaquetado y releases (1 día)

Extender `.github/workflows/release.yml` (hoy solo macOS) con un job por plataforma, cada uno con su
paquete adjunto a la misma release:

| Plataforma | Paquete | Cómo |
|---|---|---|
| Windows x64 / arm64 | `.zip` con `lineage.exe`, `settings.exe`, `datos/` y, si hace falta, el runtime de MSVC; luego un instalador (Inno Setup o WiX) | ícono `.ico` desde `datos/iconos/*.png` (agregar `--ico` a `tools/generar_iconos.py`) y un `.rc` con el ícono y la versión |
| Linux x64 / arm64 | AppImage (`linuxdeploy`) con `.desktop` e ícono PNG | `datos/` junto al ejecutable dentro del AppImage |

- Los programas buscan `datos/` con `SDL_GetBasePath()`: en Windows y en el AppImage es la carpeta del
  ejecutable, así que alcanza con copiarla al lado.
- Firmar en Windows (certificado de código) queda opcional, como la notarización en macOS.

## Fase 5: Homebrew (después de la Fase 4)

Ya está detallado en `PLAN_CLIENTE.md` (entregable 11, "Empaquetado"): cask con el `.dmg` y fórmula desde
el código (sin red: dependencias como `resource`, `datos/` en `share/lineage/datos`, token por ser privado).
En Linux, la misma fórmula sirve para Homebrew en Linux.

## Riesgos

| Riesgo | Mitigación |
|---|---|
| El shader generado no queda igual que el MSL a mano | comparar capturas en macOS antes de borrar el MSL (Fase 1, paso 4) |
| SDL_shadercross o DXC no están en todos los runners | bajar sus binarios en CI, o generar los shaders una vez y versionarlos |
| Linux sin GPU real en CI | los tests no dibujan; para la ventana, probar a mano o con `lavapipe` |
| Windows arm64: dependencias sin build oficial | SDL3, stb, dr_mp3 y doctest compilan desde el código (FetchContent), así que no deberían faltar |

## Pendientes ajenos a este plan

- **Releases:** `v0.1.0` publicada con el `.dmg` de macOS (el tag `v0.1` se borró: era de antes del
  arreglo del runner). Las siguientes: `git tag -a vX.Y.Z -m ... && git push origin vX.Y.Z`.
- **Notarización en macOS:** necesita una cuenta de Apple Developer y sus secretos en el repo.
- **Tabla de sprites real** (`list.spz`): ver `docs/DESCUBRIMIENTOS.md`, "Gfx sin fila en la tabla".

# Lineage Client: Linux, macOS and Windows, original 2009 look (C++23 + SDL3)

A from-scratch Lineage 1 client in C++23 on SDL3 (SDL_GPU). It reads the resources of an original game
installation and speaks the L1J protocol. It runs natively on **Linux, macOS and Windows** (x64 and arm64) with a
look and feel faithful to the **2009 US client** (`S3EP1U`), which was reverse engineered to get windows, layouts,
fonts and colours right. The original-client interface and the Linux port came from
[fxd0h's fork](https://github.com/fxd0h/lineage-cliente), merged back in v0.2.0.

No game assets are included: you need your own Lineage installation (the folder with `Sprite*.pak`, `Tile.pak`,
`Text.pak`, `map/`, `font/`).

![Original 2009 UI with HD text, on the l1j-en classic server](docs/capturas/fork_2026-11-17.png)

## Install

On **Linux, macOS or BSD**, one command downloads the latest version, builds it and installs it for your user
(no root needed, except to install missing build tools, which it asks for first):

```sh
curl -fsSL https://raw.githubusercontent.com/javiercorrea/lineage-cliente/latest/instalar.sh | sh
```

Run it again to update. It installs `~/.local/bin/lineage` plus a desktop entry on Linux and BSD, and
`~/Applications/Lineage.app` on macOS. Options (environment variables) are described at the top of
[instalar.sh](instalar.sh), e.g. `LIN_REF=v0.2.1` for a given version. BSD is untested.

### Step by step

The same, by hand:

1. **Install the build tools:** CMake 3.25+, Ninja, Git, a C++23 compiler (GCC 13+ or Clang 17+) and, on Linux and
   BSD, the X11/Wayland, ALSA/PulseAudio and Vulkan headers that SDL3 builds against.

   ```sh
   # Debian, Ubuntu
   sudo apt-get install build-essential cmake ninja-build git pkg-config libx11-dev libxext-dev libxrandr-dev \
     libxcursor-dev libxi-dev libxss-dev libxfixes-dev libxtst-dev libwayland-dev libxkbcommon-dev \
     libasound2-dev libpulse-dev libvulkan-dev
   # Fedora
   sudo dnf install gcc-c++ cmake ninja-build git pkgconf libX11-devel libXext-devel libXrandr-devel \
     libXcursor-devel libXi-devel libXScrnSaver-devel libXfixes-devel libXtst-devel wayland-devel \
     libxkbcommon-devel alsa-lib-devel pulseaudio-libs-devel vulkan-loader-devel
   # Arch
   sudo pacman -S --needed base-devel cmake ninja git libx11 libxext libxrandr libxcursor libxi libxss \
     libxfixes libxtst wayland libxkbcommon alsa-lib libpulse vulkan-icd-loader vulkan-headers
   # FreeBSD
   sudo pkg install cmake ninja git pkgconf libX11 libXext libXrandr libXcursor libXi libXScrnSaver libXfixes \
     libXtst wayland libxkbcommon vulkan-loader vulkan-headers
   # macOS (Xcode Command Line Tools and Homebrew)
   xcode-select --install
   brew install cmake ninja git
   ```

2. **Download the source** of the latest version (`latest` always points to it; or a version such as `v0.2.1`):

   ```sh
   git clone --depth 1 --branch latest https://github.com/javiercorrea/lineage-cliente.git
   cd lineage-cliente
   ```

3. **Build** (SDL3 and the other libraries are downloaded and built by CMake, pinned to exact versions):

   ```sh
   cmake --preset release
   cmake --build --preset release
   ```

4. **Install** for your user (`--prefix /usr/local` with `sudo` for every user):

   ```sh
   cmake --install build/release --prefix ~/.local
   ```

   This installs `~/.local/bin/lineage`, the client and its `datos/` in `~/.local/share/lineage-cliente/`, and on
   Linux and BSD a desktop entry and icon. On macOS, `herramientas/empaquetar_macos.sh` builds `Lineage.app`
   instead (in `dist/`).

5. **Run** `lineage` (or `~/.local/bin/lineage` if `~/.local/bin` is not in your `PATH`). The first time it asks
   for the folder of your Lineage installation.

To uninstall, delete `~/.local/bin/lineage`, `~/.local/share/lineage-cliente/`,
`~/.local/share/applications/lineage.desktop` and `~/.local/share/icons/hicolor/256x256/apps/lineage.png`.

### Prebuilt packages

The [Releases](https://github.com/javiercorrea/lineage-cliente/releases/latest) page has, for every version:

- **macOS:** `.dmg` (Apple Silicon and Intel). Not notarized: the first time, right-click the app and choose Open.
- **Windows** (x64 and arm64): `.zip`. Unzip anywhere and run `lineage.exe`. Not signed: SmartScreen warns the
  first time ("More info" > "Run anyway"). Requires Windows 10 or 11 with DirectX 12.
- **Linux** (x64 and arm64): AppImage (`chmod +x` and run) or `.tar.gz`. Requires glibc 2.39 or newer (Ubuntu
  24.04, Debian 13, Fedora 40) and a Vulkan driver.

The game itself is not included: the first time, the client asks for the folder of your Lineage installation.

## Features

- **Platforms:** Linux (Vulkan, SPIR-V shaders compiled from GLSL), macOS (Metal, hand-written MSL twins of every
  shader) and Windows (Direct3D 12, HLSL shaders compiled to DXIL; Winsock2, memory-mapped files with UTF-16 paths).
  x64 and arm64 everywhere. CI builds and tests all five on every pull request. Played on Intel Iris Xe (Mesa)
  and Apple M5 Pro; Windows tested on Windows 11 arm64 in a VM.
- **Original 2009 client UI is the default** (`ui_simple=0`): framed inventory, status, spellbook, options, volume,
  pet, party/pledge/buddy and private-shop windows, shop list and tooltips, F-key bar with 3 pages, HP/MP floating
  window, NPC HTML pages in the original GDI-style face, original cursors, buff icons, overhead bars, level-up
  effect, game-time orb, zone type label. Layers, offsets, colours and fonts are read from the 2009 binary.
  The simpler text interface is still there (`ui_simple=1`, F2).
- **Original window behaviour:** windows drag only by the title bar, at most four open at once, Esc closes them all
  (or opens the Esc/restart menu, which also pops up on death), right double-click closes one; original default
  positions, status page bars, spellbook tabs and slots per class, trade window and yes/no bar, inventory
  double-click use, 25 px drag threshold and equipped/sealed marks, sign-post info cards, Ctrl+T chat sizes.
- **Original sprite rendering:** the blend modes of the sprite attribute table (50% translucency, additive,
  multiply, fade, pulse), "clothes" overlay layers, alternating half-step walk for large monsters (Valakas,
  Antharas...), see-through houses and fading roofs taken from the map, poison pulse, the exact invisibility rule,
  polymorph effect, per-frame sounds and lightning/meteor light flashes.
- **Original HUD details:** chat and name colours, names on mouse-over, monster level shown by the outline colour,
  speech bubbles, cursors, buff icons with the original tooltips and end messages (154-slot table from the
  binary), shout direction icons, original mail compose window, Ctrl+P
  screenshots to `Capture/`, Alt+1..4 emotes, `/rank`, `/propose`, `/divorce`, `/duel`, `/checkpk`, `/loc`, `/telcon` and the
  original command aliases.
- **The view fills the window** (`vista=amplia`, default), or an optional fixed 640x480 view (`vista=fija`), with filters: integer, xBR, FSR (default), CRT,
  Anime4K (`escalado=`, Shift+F5).
- **HD text** (`texto_hd=1`, default): UI and world text drawn after the scaling filter at output resolution,
  sharp at any scale with identical layout ([docs/fork/hd_text.md](docs/fork/hd_text.md)). Optional 4x HD UI
  image pack (`hd=<dir>`, not distributed).
- **Protocol profiles:** `jp363` (L1J-JP 3.63c), `tw380` (Taiwan 3.80C, with Big5 server text decoded to UTF-8)
  and `s3ep1` (US 2009).
- **Multi-folder game data:** `juego_extra=` fallback folders for sprites, tiles, maps, sounds, HTML pages and
  sprite lists missing from the main install; `lista_sprites=` / `list.spz` support for newer gfx.
- **Gameplay and social:** clan emblems (upload, cache, overhead draw), alliances (`C_Rank`), ship arrival
  (`C_Ship`), combat music, polymorph pages, pets and summons, player shops, mail, bulletin boards, auto-buffs and
  auto potion, sticky attack.
- **Three local test servers** with reproducible installers (below).
- **Test tooling:** live-control socket (debug builds), Python driver and scripted live tests, invisible test
  instances on Xvfb (`herramientas/lanzar_prueba.sh`).

- **Core game:** login, character selection and creation, movement, chat, combat, inventory, spells, NPCs and
  trading, party/trade/clan, houses, castles and sieges, boards and mail, shops, weather, zone music, five interface
  languages, an in-game settings window and a resource explorer. The status write-up of these is in
  [docs/upstream/](docs/upstream/README.md).

## Building for development

The build tools are those of [Step by step](#step-by-step) (CMake 3.25+, Ninja, a C++23 compiler; SDL3 3.4.18, stb,
dr_mp3, doctest and the zlib and brotli decoders are fetched by CMake, pinned). On **Windows**, build from the Visual Studio developer console
(x64 or arm64) with Ninja (`pip install ninja`): `dxc.exe` from the Windows SDK compiles the shaders.

```sh
cmake --preset debug          # or release / debug-asan
cmake --build --preset debug
LINEAGE_DIR="/path/to/game" ctest --preset debug   # without LINEAGE_DIR, resource tests are skipped
```

A shader change goes into the GLSL file (`src/render/shaders/*.frag`, `*.vert`), its HLSL twin (`*.hlsl`) and its
MSL twin (`src/render/gpu.cpp` or `shaders_msl_anime4k.h`); `herramientas/compilar_shaders.sh` (needs
`glslangValidator`) regenerates the embedded SPIR-V. A review of every shader and its cost is in
[docs/analisis/REVISION_SHADERS.md](docs/analisis/REVISION_SHADERS.md).

Programs: `build/<preset>/lineage` (the game; its settings are edited in-game), `explorador` (resource explorer),
`lin_tests`. Packaging: `herramientas/empaquetar_macos.sh [VERSION]` (`Lineage.app` and a `.dmg`, not notarized:
right-click, Open), `herramientas/empaquetar_windows.ps1` (`.zip`) and `herramientas/empaquetar_linux.sh` (AppImage and `.tar.gz`).
A `v*` tag runs `.github/workflows/release.yml`, which publishes the release with all of them (macOS, Windows and
Linux x64/arm64).

## Configuration

The client takes no command-line parameters. Everything comes from `settings.ini`:
`~/.local/share/Lineage/LineageCliente/settings.ini` on Linux and BSD,
`~/Library/Application Support/Lineage/LineageCliente/settings.ini` on macOS and
`%APPDATA%\Lineage\LineageCliente\settings.ini` on Windows. Every option is in the settings window (Settings on the
login screen, Options in game), which applies changes at once or says when a restart is needed; the file can also
be edited by hand with the game closed. Debug builds accept `LIN_CONFIG=/path/own.ini` to use another file.

```ini
juego=/path/to/Lineage-US-2009
juego_extra=/path/to/newer-client-data
servidor=Classic|127.0.0.1|2002|s3ep1|fx
servidor=JP|127.0.0.1|2000|jp363|
servidor=Whale|127.0.0.1|7001|tw380|
servidor_elegido=Classic
ui_simple=0
texto_hd=1
```

| Key | Meaning |
|---|---|
| `juego` | Main game folder |
| `juego_extra` | Fallback game folder, repeatable, tried in order |
| `servidor`, `servidor_elegido` | One line per server: `name\|host\|port\|profile\|last account[\|launcher]`; profile is `jp363`, `tw380` or `s3ep1`. The chosen one is `servidor_elegido`. The optional launcher is a command (rest of the line) for servers reached through a local proxy that their own launcher opens: on login the client runs it and connects to `host` at the port it prints (the number after `port`, else after an address like `127.0.0.1:`, else the last number; 30 s limit). Before another launcher and on exit it runs the previous one again with `stop`; `port` then only names the server. The command runs through the system shell, so only use a `settings.ini` you trust |
| `ui_simple` | `0` (default): original 2009 UI; `1`: the simple text UI (F2 toggles) |
| `texto_hd` | `1` (default): HD text pass; `0`: pixel text in the 640x480 frame |
| `fuente_original` | `1` (default): game's `eng.fnt` for ASCII; `0`: Departure Mono |
| `vista`, `escalado` | `amplia` (default, fills the window) or `fija` (640x480 with black bars); `fsr` (default), `entero`, `xbr`, `crt` or `anime4k` |
| `hd`, `lista_sprites`, `idioma`, `logo` | HD UI pack folder, plaintext `list.spz` path, interface language, simple-login logo |

The full list (volume, effects, auto-buffs, shortcut bars...) is in [docs/fork/REFERENCE.md](docs/fork/REFERENCE.md).

## Local test servers

Development and testing happen only against local servers; never connect the client to a third-party server.
Each folder has `instalar.sh` (once; downloads a pinned server, applies our patches, creates the database),
`arrancar.sh` and `detener.sh`. Linux uses apt, macOS Homebrew (`servidor_local/comun.sh`).

| Folder | Server | Port | Profile | Database |
|---|---|---|---|---|
| `servidor_classic/` | l1j-en classic, US 2009 (**main one**) | 2002 | `s3ep1` | MariaDB 3309 |
| `servidor_local/` | L1J-JP 3.63c | 2000 | `jp363` | MariaDB 3307 |
| `servidor_380/` | L1JGO-Whale, Taiwan 3.80C/3.81C | 7001 | `tw380` | PostgreSQL 5433 |
| `servidor_classic363/` | l1j-en classic, 3.63 branch ([notes](servidor_classic363/README.md)) | 2001 | `jp363` | MariaDB 3310 |

```sh
servidor_classic/instalar.sh                      # once
(cd servidor_classic && nohup setsid ./arrancar.sh > servidor.log 2>&1 < /dev/null &)
build/debug/lineage
servidor_classic/detener.sh
```

Accounts are created on first login with the password typed that time. Server changes are kept reproducible as
`*.patch` files, idempotent `corregir_db.sql` and migration scripts (e.g. `migrar_fx.py`). Evaluations:
[l1j-en classic](docs/fork/SERVER_L1JEN_EVALUATION.md), [Whale 3.80C](docs/fork/SERVER_380_EVALUATION.md),
[3.81C data coverage](docs/fork/DATA_COVERAGE_381.md). The classic server also carries our patches for alliances,
the Giran Bugbear Race and several l1j-en issues; the Whale server has Linux support, alliance and English item
names.

## Controls

F1 lists all keys in game (simple UI). Highlights (original-client hotkeys): Ctrl+I inventory, Ctrl+A status, Ctrl+S magic,
Ctrl+C social, Ctrl+V options, Ctrl+Q restart/quit, F5-F12 spells/items, Ctrl+1..8 = F5..F12, Ctrl+R floating
HP/MP, Ctrl+T chat size, Ctrl+X attack mode, Ctrl+P screenshot. Hold the left button to walk or attack; Ctrl+click
forces an attack on NPCs/players; F2 toggles the UI mode; Shift+F5 cycles the scaling filter; Alt+Enter toggles
full screen (on Mac also Cmd+Enter or Ctrl+Cmd+F). Complete list: [docs/fork/REFERENCE.md](docs/fork/REFERENCE.md).

## Reverse engineering notes

What the 2009 client does and where (addresses in `S3EP1U.bin`) is written down in [docs/fork/](docs/fork/README.md):
sprites and rendering (`RE_SPRITES_RENDER.md`), protocol (`RE_PROTOCOL.md`), windows (`RE_WINDOWS.md`), HUD
(`RE_HUD.md`) and the later rounds (`RE_ROUND3.md`, `RE_ROUND4.md`, ...). The remaining differences from the
original UI are tracked in [docs/fork/AUDIT_ORIGINAL_UI.md](docs/fork/AUDIT_ORIGINAL_UI.md).

## Development and testing

Rules, architecture and recipes for people and coding agents are in [AGENTS.md](AGENTS.md). Tests: doctest
(`lin_tests`). Debug builds open a live-control socket (a Unix socket; TCP on Windows) driven by
`herramientas/lin_ctl.py` and the scripts in `herramientas/pruebas/`; on Linux, `herramientas/lanzar_prueba.sh
<own.ini>` runs test clients on an invisible display. `perf on` logs frame, CPU and GPU times. Version history:
[CHANGES.md](CHANGES.md). Notes, changes and audits of the original-UI work: [docs/fork/](docs/fork/README.md).

## Known gaps

- Shop and options windows are partly from the simple UI; the full list of differences from the 2009 UI is in
  [docs/fork/AUDIT_ORIGINAL_UI.md](docs/fork/AUDIT_ORIGINAL_UI.md).
- New-monster animations from the encrypted `list.spz` are not decoded.
- Packages are not signed: macOS asks to confirm the first launch and Windows SmartScreen warns; no Windows
  installer yet (the `.zip` only).
- BSD builds are untested.
- Not yet matched to the original: the time-orb sparkle overlays, cursor states 14 / 16 and the selected-spell variant of 8, the cooking
  progress bar (the l1j-en server never starts it) (see the audit, `RE_ROUND5.md` and `RE_ROUND6.md`).
- Approximate: the bubble text block position inside its rectangle and the downward flip of a bubble clipped at the top
  (inferred, RE_ROUND7 section 1).
- Interface translations (es, en, pt, de, fr) cover login, character screens and the settings window only.
- Server text for `tw380` depends on Whale's Big5 output and the data in the game folder; see the evaluations above.

## Issues

Bug reports, feature requests and setup questions go to
[Issues](https://github.com/javiercorrea/lineage-cliente/issues). When reporting a UI difference from the original
client, a screenshot of both helps a lot. Game assets are never accepted.

## Credits and license

Client by Javier Correa; the original 2009 interface, the Linux port and the `tw380`/`s3ep1` profiles by fxd0h
([fork](https://github.com/fxd0h/lineage-cliente), merged in v0.2.0). The earlier README and status tracker are kept
in [docs/upstream/](docs/upstream/README.md). The format code derives
from [l1j-client](https://github.com/uglyoldbob/l1j-client) (GPLv3), as does `datos/sprite_table.txt`, so the client
is distributed under **GPL-3.0** ([LICENSE](LICENSE)). Servers: L1J-JP, l1j-en (classic) and L1JGO-Whale
(eddy7697 fork), each under its own license. Interface font Departure Mono: SIL OFL 1.1
(`licencias/DepartureMono-OFL.txt`).

# Plan para completar l1j-client como cliente multiplataforma

**Objetivo:** un cliente de Lineage 1 que corra en Windows, macOS (Intel y Apple Silicon) y Linux, con
el aspecto y la sensación del original.

**Recursos:** los gráficos, mapas, textos, fuentes y sonidos siguen siendo los del juego original
(abandonware). El cliente los lee directamente de la carpeta de instalación (`Sprite*.pak`,
`Tile.pak`, `Text.pak`, `map/`, `font/`, `sound/`…). Para acelerar el arranque, guarda en una caché
local versiones ya decodificadas, siempre derivadas de esos originales.

**Diseño:** orientado a datos (*data oriented programming*), en C++20 con estilo cercano a C. El
rendimiento es un requisito medible desde el primer entregable.

**Organización:** el trabajo se divide en **entregables jugables**. Cada uno es un ejecutable que se
puede probar contra un servidor local, y la infraestructura crece solo lo que pide cada entregable.

---

## 1. Punto de partida

| Parte de [uglyoldbob/l1j-client](https://github.com/uglyoldbob/l1j-client) | Estado |
|---|---|
| C++ (`src/`, unas 18 000 líneas, **GPLv3**) | SDL 1.2.14 y autotools. Orientado a objetos: widgets con herencia, recursos con punteros y un mutex por objeto. Implementa login, selección de personaje, mapa, sprites y chat, con unos 34 paquetes. Sus parsers de formatos se **portan adaptados**: se conserva la lógica de decodificación y se cambia dónde queda el resultado. |
| Rust (`rust/`) | Esqueleto; no se usa |
| Análisis propio (`analisis/`) | Framing y cifrado verificados contra 1500 paquetes reales, catálogo de opcodes 2009 y tablas de opcodes de 5 versiones |

**Licencia:** si se porta código del C++ original, el cliente nuevo es una obra derivada y debe
publicarse como **GPLv3**. Si se quiere otra licencia, los parsers se reescriben solo a partir de la
documentación de formatos (sección 1.1) sin copiar código.

### 1.1 Formatos de recursos (según el C++)

| Archivo | Contenido | Referencia C++ |
|---|---|---|
| `*.idx` / `*.pak` | Contenedor: índice (nombre, offset, tamaño) más datos. El índice de `Text` va cifrado con DES (clave `~!@#%^$<`) | `resources/pack.cpp`, `lindes.cpp` |
| `N.til` (en `Tile.pak`) | Juego de tiles: `u16` cantidad, tabla de offsets `u32` y datos. Cada tile es un medio rombo de 24×24 píxeles en RGB555. El bit 1 del primer byte indica tile con transparencia (codificado en tramos, RLE) | `resources/tile.cpp`, `sdl_graphic.cpp` |
| `A-B.spr` (en `Sprite*.pak`) | Sprite: paleta RGB555, tiles en tramos y frames. Cada frame tiene una caja (x1, y1, x2, y2) y una lista de tiles posicionados | `widgets/sprite_motion.cpp` |
| `sprite_table.txt` | Definición textual de cada sprite: acciones, frames por dirección, sombra, ropa y alias (`N=M`) | `widgets/sprite.cpp` |
| `map/<id>/*.s32`, `*.seg` | Segmentos de mapa de 64×64 celdas. Cada celda tiene 2 medios tiles de suelo (`u32`: juego de tiles y tile) y atributos por medio tile (`u16`: pasable, zona segura…). Después vienen objetos del mapa (listas de tiles por capa) y portales | `widgets/sdl_lin_map.cpp` |
| `N.img` / `N.png` (en los `.pak`) | Imágenes de la interfaz: fondos, botones, ventanas. Por ejemplo, el fondo de login es `814.png` y la linterna animada son las `.img` 273–300. Las coordenadas de cada pantalla están en el C++. El `skin/` de la instalación es solo del launcher | `resources/sdl_graphic.cpp`, `drawmode/draw_*.cpp` |
| `font/`, `Text.pak` | Fuentes bitmap del original y tablas de texto en CP949 | `resources/small_font.cpp`, `reg_font.cpp`, `table.cpp` |
| `sound/` | Efectos y música | `resources/music.cpp` |

---

## 2. Tecnología (C/C++)

### 2.1 ¿Cambia el plan al pasar de Rust a C/C++?

**La arquitectura no cambia.** El diseño orientado a datos (tablas por columna, índices en vez de
punteros, procesamiento por lotes, arenas) es natural en C y C++; de hecho, es de donde viene el
enfoque. Cambian estas cosas:

| Aspecto | Antes (Rust) | Ahora (C++) |
|---|---|---|
| Capa de plataforma | winit + wgpu | **SDL3 + SDL_GPU**: en C++ es la opción nativa y desaparece el problema de bindings |
| Build | Cargo | **CMake ≥ 3.25** con presets y dependencias vendorizadas (git submodules o `FetchContent`). Nada que instalar fuera del compilador |
| Seguridad al parsear binarios | La daba el lenguaje | **ASan + UBSan** en CI, **fuzzing** de los parsers (libFuzzer) y lectura acotada siempre por `std::span` |
| Profiler | `tracy-client` | **Tracy**, nativo en C++ |
| Reutilización | Ninguna directa | Los parsers de l1j-client se pueden portar (con la condición GPLv3) |

### 2.2 ¿C o C++?

**Recomendación: C++20 en estilo "C con plantillas"** (a veces llamado *orthodox C++*).

**Se usa:**
- `struct` POD y funciones libres;
- `std::span`, `std::array`, `constexpr`;
- plantillas solo para contenedores (`SoaTable<...>`, `Pool<T>`);
- `enum class`.

**No se usa:**
- herencia ni `virtual`;
- excepciones (`-fno-exceptions`) ni RTTI;
- `shared_ptr`, `iostream` ni `std::function` en código caliente.

**Por qué C++ y no C puro:** las plantillas generan tablas columnares tipadas sin macros, `span`
acota los accesos y se puede portar el C++ original. En C11 puro el diseño es idéntico, pero las
tablas se generan con macros X. Es una opción válida si prefieres C; cambia solo la sintaxis.

Compiladores: MSVC o clang-cl en Windows, Apple Clang en macOS, GCC o Clang en Linux.

### 2.3 Capa de plataforma y gráficos

| Opción | Ventana y entrada | Gráficos | Audio | Evaluación |
|---|---|---|---|---|
| **SDL3 + SDL_GPU** (elegida) | sí, más gamepad | Metal / Vulkan / D3D12 | incluido | Una sola dependencia en C (licencia zlib) que cubre todo. SDL_GPU da instancing, texture arrays y buffers estáticos. Shaders: se escriben en HLSL y se compilan offline con `SDL_shadercross` a SPIR-V, MSL y DXIL |
| sokol (`sokol_app` + `sokol_gfx`) | sí | Metal / D3D11 / GL | básico | **Mejor alternativa:** unos pocos headers en C, muy orientado a datos, compila en segundos. Su audio y su manejo de ventanas son más limitados que SDL3 |
| bgfx | no (necesita GLFW o SDL) | todos | no | Potente pero pesado de compilar; demasiado para un juego 2D |
| raylib | sí | OpenGL | incluido | Muy simple, pero va sobre OpenGL (ver abajo) y hace difícil controlar el batching |
| GLFW + OpenGL | sí | GL 3.3/4.1 | no | Viable, pero OpenGL está obsoleto en macOS (congelado en 4.1 y sin herramientas en Apple Silicon) |
| GLFW + Vulkan | sí | Vulkan | no | En macOS necesita MoltenVK. Mucho código de bajo nivel para un juego 2D |
| SFML | sí | 2D sobre OpenGL legado | incluido | No: API de objetos con una llamada de dibujo por sprite y soporte irregular en macOS |
| SDL2 (C++ original usa SDL 1.2) | sí | renderer 2D sin instancing | incluido | Insuficiente para el rendimiento buscado; SDL3 lo reemplaza |

**Aislamiento:** toda dependencia de SDL3 queda en los módulos `platform` y `render`. El resto
(tablas, sistemas, protocolo y recursos) no incluye headers de SDL y se podría pasar a sokol sin
tocarlo.

### 2.4 Dependencias (todas vendorizadas y multiplataforma)

| Necesidad | Librería |
|---|---|
| Ventana, entrada, audio, GPU, hilos | SDL3 |
| Empaquetar atlas | `stb_rect_pack` |
| Decodificar WAV y MP3 | `dr_wav`, `dr_mp3` |
| Fuente de respaldo con Hangul | `stb_truetype` |
| CP949 → Unicode | Tabla generada en un header desde el índice EUC-KR de WHATWG (sin iconv, que no está en Windows) |
| DES de `Text.pak` | Portar `lindes.cpp` o una implementación propia de unas 200 líneas |
| Mapear archivos en memoria | Wrapper propio de unas 50 líneas (`mmap` / `CreateFileMapping`) |
| Tests | doctest |
| Benchmarks | nanobench |
| Profiler | Tracy (opcional, `-DLIN_TRACY=ON`) |

---

## 3. Principios de diseño orientado a datos

1. **Tablas, no objetos.** Cada tipo de dato vive en una tabla columnar (*struct of arrays*): un
   arreglo contiguo por campo. Los sistemas recorren solo las columnas que usan.
2. **Índices, no punteros.** Las referencias son `uint32_t` o handles `{índice, generación}`. No hay
   `new` por entidad ni grafos de objetos.
3. **Datos calientes separados de los fríos.** Posición y animación van separadas de nombre, título
   y clan.
4. **Procesar en lotes.** La red produce eventos que se aplican juntos al inicio del frame. El dibujo
   es una lista de comandos que se ordena y se envía con instancing.
5. **Cero allocations en régimen.** Hay una arena por frame que se resetea, buffers reutilizados y
   textos internados (`StringArena`, handle `uint32_t`). Se verifica en un test con `operator new`
   instrumentado.
6. **Decodificar una vez.** RGB555 y RLE se convierten a RGBA8 en atlas de GPU y se guardan en caché
   en disco. El suelo de cada segmento de mapa es un buffer estático en la GPU.
7. **Datos inmutables compartidos.** Lo cargado de recursos es de solo lectura, así que los hilos lo
   leen sin locks. La única comunicación entre hilos son colas SPSC (un productor, un consumidor) sin
   locks.

---

## 4. Módulos

Un repositorio con CMake. Cada módulo es una librería estática con sus tests. Dependencias:
`core ← proto, assets ← world ← render, ui, audio ← app`. Solo `platform` y `render` ven SDL3.

### 4.1 `core`

- Tipos de dominio:
  - `ObjId` (id del servidor) y `Slot` (índice local en una tabla);
  - `Handle { uint32_t idx, gen; }`;
  - `TilePos { uint16_t x, y; }`, `Heading : uint8_t`, `GfxId : uint16_t`, `MapId : uint16_t`.
- `SoaTable<Cols...>`: una tabla columnar con su mapa `ObjId → Slot` (hash abierto propio), inserción,
  borrado por intercambio con el último (*swap-remove*, que mantiene la tabla compacta) e iteración
  por columnas.
- `StringArena` (persistente y por frame), `Arena` (bump allocator), `Ring<T, N>` de capacidad fija,
  `BitSet` y `SpscQueue<T>`.
- `Span` acotado con chequeo en debug para leer binarios.

### 4.2 `proto`: protocolo de red

- `framing`: longitud `u16` LE más cuerpo. Reensambla sobre un buffer circular fijo de 64 KiB.
- `cipher`: semilla, derivación de clave, cifrado, descifrado y actualización de clave, según
  `PROTOCOLO_RED.md` §4.
- `reader` / `writer`: los tipos `c h d s S`, leyendo sin copiar desde el buffer.
- `opcodes`:
  - una tabla `uint8_t → Internal` traduce número de cable a id interno, y otra hace lo inverso;
  - se cargan de los `opcodes.txt` ya generados (`jp363`, `s3ep1`, `kr380`…).
- `decode`: una tabla de punteros a función indexada por id interno. Cada función convierte el
  cuerpo en un `Event` de 32 bytes o menos (POD) y lo agrega a un arreglo reutilizado.
- `encode`: los comandos del cliente (`Cmd`: mover, chat, atacar, usar ítem…) se serializan con la
  tabla inversa.
- `net_thread`: socket TCP bloqueante en su propio hilo (SDL3 o un wrapper de sockets BSD/Winsock),
  comunicado con el hilo principal por dos `SpscQueue`.

### 4.3 `assets`: recursos originales

- `pak`: `.idx` y `.pak` mapeados en memoria. El índice se guarda ordenado por hash del nombre y se
  busca con búsqueda binaria; el resultado es un `span`.
- `text`: tablas de `Text.pak` (DES y CP949 → UTF-8) en un `StringArena`.
- `tiles`: decodifica `.til` a RGBA8 (medios rombos de 24×24, sólidos o en tramos).
- `sprites`:
  - decodifica `.spr`;
  - **aplanado:** todas las animaciones forman un solo arreglo de `FrameDef` y una tabla
    `(gfx, acción, dirección) → rango`.
- `sprite_table`: `sprite_table.txt` en columnas: acciones, duraciones, sombra, ropa y alias.
- `maps`:
  - decodifica `.s32/.seg` en tablas por segmento: `floor[64*128]`, `attr[64*128]`, objetos y
    portales;
  - consultar si una celda es pasable es acceso directo a `attr`.
- `fonts`: fuentes bitmap del original convertidas a un atlas de glifos, con `stb_truetype` como
  respaldo para Hangul.
- `bake`: atlas de 2048×2048 con `stb_rect_pack`, decodificados en un pool de hilos y guardados en
  caché en disco (clave = tamaño y fecha de cada `.pak`).
- `streaming`: carga bajo demanda por segmento de mapa y por `GfxId`, con prioridad según la
  distancia a la cámara. Las páginas de atlas se reemplazan con una política LRU.

### 4.4 `world`: estado del juego y sistemas

| Tabla | Columnas calientes | Columnas frías |
|---|---|---|
| `Objects` (PCs, NPCs, monstruos) | `pos`, `prev_pos`, `move_t`, `heading`, `gfx`, `action`, `anim_start`, `hp_pct`, `light`, `flags` | `name`, `title`, `clan`, `lawful`, `poly_gfx` |
| `GroundItems` | `pos`, `gfx` | `name`, `count` |
| `Effects` | `pos`/`obj`, `gfx`, `start`, `dur` | — |
| `Inventory` | `obj_id`, `gfx`, `count`, `flags` | `name`, `desc` |
| `Buffs` | `skill`, `ends_at`, `icon` | — |
| `Player` (único) | stats, HP/MP, exp, peso, comida, CA, resistencias (los campos del op 59 del catálogo) | — |
| `MapState` | los 9 segmentos alrededor del jugador | — |
| `Chat` | `Ring` de (canal, emisor, texto) | — |

Los sistemas son funciones libres `void sistema(World&, const FrameCtx&)` y se ejecutan en este
orden:

1. `apply_events`
2. `movement`: interpolación según velocidad normal, haste o brave.
3. `animation`
4. `lifetimes`
5. `visibility`
6. `input_to_cmds`: incluye la predicción del movimiento propio.

### 4.5 `render`

- **Atlas:** un *texture array* por tipo (tiles, sprites, UI y glifos).
- **Draw list:**
  - cada `DrawCmd` mide 24 bytes: `key` (`u64`), `page`, `src`, `dst` y `tint`;
  - `key` = capa | profundidad isométrica | página;
  - se ordena con un radix sort.
- **Pasadas,** cada una con instancing (una llamada por página y capa):
  1. suelo (buffers estáticos por segmento);
  2. mundo (intercalado por profundidad);
  3. efectos (mezcla aditiva, luz y noche);
  4. UI y texto.
- **Escala:** se renderiza a la resolución lógica del original y se escala con factor entero.
- **Proyección isométrica:** se porta de `screen_coord.cpp`, `map_coord.cpp` y `pixel_coord.cpp`.

### 4.6 `ui`

- Inmediata (*immediate mode*): emite `DrawCmd` a la misma lista. Cada ventana es una struct plana
  y el layout es data-driven con las imágenes numeradas de la UI original (`.img`/`.png` en los `.pak`).
- La entrada se traduce a acciones mediante una tabla configurable.

### 4.7 `audio`

Tabla `SoundId → buffer decodificado` en caché LRU, sobre streams de SDL3. Los sonidos se piden en
lote y la música va en streaming según el mapa.

### 4.8 `platform` y `app`

- `platform`: ventana, entrada, tiempo, rutas de configuración y caché por sistema.
- `app`: en cada frame ejecuta:

```
eventos del SO → drenar cola de red → decode → apply_events
→ movement → animation → lifetimes → visibility
→ ui + input_to_cmds → encolar Cmd → armar draw_list → radix sort → GPU
→ reset de arenas del frame
```

### 4.9 `tools`

- `explorer`: visor de sprites, tiles, mapas y fuentes.
- `pcapdump`: decodifica capturas y sirve de regresión del protocolo.
- `bench`: escenas sintéticas.
- `fuzz_*`: un objetivo de libFuzzer por parser.

---

## 5. Presupuestos de rendimiento

| Métrica | Objetivo | Desde el entregable |
|---|---|---|
| CPU por frame con 500 objetos visibles | < 2 ms | 3 |
| Llamadas de dibujo por frame | < 50 | 1 |
| Allocations por frame en régimen | 0 | 1 |
| Decodificar y aplicar 1000 paquetes | < 1 ms | 3 |
| Arranque con caché | < 2 s | 1 |
| Arranque sin caché | < 15 s, con barra de progreso | 3 |
| FPS | Sincronizado con el monitor; la simulación no depende del FPS | 1 |

---

## 6. Entorno de pruebas

1. **Servidor local L1J JP 3.63c:** ya está clonado y sus opcodes son conocidos en ambos sentidos.
   Funciona con MariaDB local; todo el desarrollo se hace sin tocar el servidor privado.
2. **Capturas:** `2009.pcap` y `KR380.pcap` se copian a `analisis/` y sirven como fixtures.
3. **Tests con recursos reales:** se ejecutan solo si `LINEAGE_DIR` apunta a una instalación.
4. **CI:** GitHub Actions en Windows, macOS y Linux. Compila en Debug con ASan/UBSan y en Release,
   ejecuta los tests, corre cada fuzzer 60 s y compara los benchmarks contra la línea base.

---

## 7. Entregables jugables

Cada entregable es un ejecutable que se prueba contra el servidor local. La columna "Se construye"
indica qué partes de la infraestructura nacen o crecen en ese entregable.

### Entregable 1: Pantalla de login

- **El jugador puede:**
  - abrir el cliente y ver la pantalla de login original (fondo, botones animados, música);
  - escribir usuario y contraseña y conectarse;
  - ver los errores del servidor (contraseña incorrecta, cuenta en uso) y las noticias.
- **Se construye:**
  - `core`;
  - `platform`, con selección de la carpeta del juego en el primer arranque;
  - `assets` (`pak`, `text` y las imágenes de UI `.img`/`.png`);
  - `render` mínimo: atlas de UI y draw list;
  - `ui` (input box y botones), `audio` mínimo (música);
  - `proto` completo para pre-juego: framing, cifrado, key, versión, login y noticias;
  - `net_thread`, CI, CMake y vendorizado de dependencias.
- **Terminado cuando:**
  - el login se completa contra el servidor local y los mensajes de error son los del original;
  - ningún frame hace allocations y hay menos de 50 llamadas de dibujo por frame;
  - corre en los 3 sistemas.

### Entregable 2: Selección y creación de personaje

- **El jugador puede:**
  - ver sus personajes con su sprite animado, nivel y clase;
  - crear uno (clase, sexo, reparto de stats con las reglas del original) y borrarlo;
  - pulsar "entrar".
- **Se construye:**
  - `assets` (`sprites`, `sprite_table`, fuentes bitmap);
  - el atlas de sprites;
  - el sistema `animation`;
  - los paquetes de número de personajes, lista, creación, borrado y entrada al juego.
- **Terminado cuando:** se crea, borra y elige personaje en el servidor local, con las animaciones
  del original.

### Entregable 3: Dentro del juego, moverse

- **El jugador puede:**
  - aparecer en el mundo y caminar con clic o teclado, con colisiones;
  - ver NPCs, monstruos y otros jugadores aparecer, moverse y desaparecer;
  - ver el ciclo de día y noche y la luz.
- **Se construye:**
  - `assets` (`tiles`, `maps`, `bake`, `streaming`);
  - las pasadas de suelo y mundo de `render`;
  - `world` (`Objects`, `MapState` y los sistemas 1–6);
  - predicción del movimiento propio;
  - los paquetes de mapa, poner, mover y quitar objeto, cambiar dirección, luz y hora;
  - `bench`.
- **Terminado cuando:**
  - se camina por Talking Island sin saltos;
  - se cumplen todos los presupuestos de la sección 5 con 500 objetos sintéticos;
  - el orden de profundidad coincide con capturas de pantalla del cliente original.

### Entregable 4: Chat y estado

- **El jugador puede:**
  - chatear por los canales normal, gritar, global, comercio, clan, grupo y susurro, con los
    prefijos del original;
  - leer los mensajes del sistema;
  - ver las barras de HP, MP y EXP y abrir la ventana de estado.
- **Se construye:** `Chat`, `Player`, ventanas de chat y estado, y los paquetes de chat, mensajes,
  HP/MP/EXP y estado (op 59 y relacionados).
- **Terminado cuando:** dos clientes conversan entre sí y el estado se actualiza en vivo.

### Entregable 5: Combate

- **El jugador puede:**
  - atacar monstruos y ver la animación de ataque, el daño y la barra de vida del objetivo;
  - morir y reiniciar;
  - ver a otros pelear.
- **Se construye:** `Effects`, los sistemas `lifetimes` y de acciones, los sonidos de combate y los
  paquetes de ataque, acción, HP del objetivo, muerte y reinicio.
- **Terminado cuando:** se puede matar un monstruo de Talking Island y reaparecer tras morir.

### Entregable 6: Inventario e ítems

- **El jugador puede:**
  - abrir el inventario, equipar armas y armaduras (el sprite cambia) y usar pociones;
  - recoger y soltar ítems del suelo, viendo el peso.
- **Se construye:** `Inventory` y `GroundItems`, la ventana de inventario con arrastrar y soltar, y
  los paquetes de ítems: lista, agregar, quitar, estado, bendición, suelo y uso.
- **Terminado cuando:** se recoge el botín de un monstruo, se equipa y se usa una poción.

### Entregable 7: NPCs y comercio

- **El jugador puede:** hablar con NPCs, comprar y vender en tiendas, usar el almacén y teletransportarse
  con NPCs.
- **Se construye:** el diálogo NPC (subconjunto del HTML del servidor), las ventanas de tienda y
  almacén, y los paquetes de acción NPC, HTML, compra, venta y almacén.
- **Terminado cuando:** se vende el botín, se compra equipo y se guarda en el almacén.

### Entregable 8: Magia y habilidades

- **El jugador puede:** abrir el libro de hechizos, lanzar magia con efectos visuales, ver íconos de
  buffs con su duración y moverse más rápido con haste o brave.
- **Se construye:** `Buffs`, la ventana de skills, los efectos aditivos y los paquetes de
  aprender, usar y efecto de skill, buffs y velocidad.
- **Terminado cuando:** un mago lanza Energy Bolt y un caballero usa poción de haste, con los
  íconos y duraciones correctos.

### Entregable 9: Social

- **El jugador puede:** formar grupo, unirse a un clan, intercambiar ítems con otro jugador y ver
  títulos, emblemas y alineamiento.
- **Se construye:** las ventanas de grupo, clan e intercambio, y los paquetes correspondientes.
- **Terminado cuando:** dos clientes forman grupo e intercambian un ítem.

### Entregable 10: Mundo completo

- **El jugador puede:**
  - recorrer todos los mapas con portales y teletransportes;
  - ver el clima y escuchar música por zona y efectos ambientales;
  - configurar opciones y atajos F1–F12;
  - redimensionar la ventana.
- **Se construye:** la pasada de clima, `audio` completo y la ventana de opciones con persistencia.
- **Terminado cuando:** se viaja por los mapas principales sin errores y los presupuestos se
  mantienen en zonas concurridas.

### Entregable 11: Distribución y otras versiones

- **El jugador puede:** instalar el cliente sin conocimientos técnicos (`.exe` con instalador, `.app`
  y AppImage) y elegir la versión de protocolo del servidor.
- **Se construye:** el empaquetado, la tabla `s3ep1` completa y, si se puede, la del cliente 2014.
- **Terminado cuando:** se instala en una máquina limpia de cada sistema y conecta con un servidor de
  otra versión, con autorización de su operador.

**Opcional, antes del 1:** un entregable 0 con el `explorer` (visor de sprites, tiles y mapas). No es
jugable, pero valida los decodificadores y el aspecto antes de tener red.

---

## 8. Riesgos

| Riesgo | Mitigación |
|---|---|
| Errores de memoria al parsear binarios en C++ | `span` acotado, ASan/UBSan en CI y fuzzing de cada parser |
| El servidor privado usa el protocolo 2014, que se conoce solo en parte | Desarrollar contra JP 3.63c; una versión nueva es una tabla más (entregable 11) |
| Variantes de formato (`.spr`, `.til`, `.s32`) entre versiones | Validar en el `explorer`; detectar la variante por cabecera |
| El orden de profundidad isométrica no coincide con el original | Portar la lógica de l1j-client y comparar capturas de pantalla con el cliente original |
| Fuentes bitmap sin Hangul | Respaldo con `stb_truetype` en el mismo atlas de glifos |
| Shaders para tres backends | Escribirlos una vez en HLSL y compilarlos offline con `SDL_shadercross`; los binarios se versionan en el repositorio |
| Licencia | Si se porta código de l1j-client, el cliente es GPLv3 |

---

## 9. Primeros pasos (inicio del entregable 1)

1. Repositorio con CMake y presets (`debug-asan`, `release`), SDL3 vendorizado y CI en los 3 sistemas.
2. `core`: `SoaTable`, `Arena`, `StringArena` y `SpscQueue`, con tests.
3. `assets::pak` con mapeo en memoria más DES, con tests contra `Text.pak` real.
4. `proto::cipher` y `framing`, con las capturas como fixtures, y un fuzzer del reader.
5. Ventana SDL3 + SDL_GPU mostrando el fondo de login (`814.png`) escalado sin perder la nitidez.

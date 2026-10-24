# Cliente Lineage (C++20 + SDL3)

Cliente nuevo de Lineage 1 que usa los recursos de la instalación original (abandonware) y habla
el protocolo L1J. Implementa los **entregables 1 a 9** del plan (`docs/analisis/PLAN_CLIENTE.md`):
login, selección y creación de personaje, entrar al juego y moverse, chat y estado, combate,
inventario e ítems, hechizos y polymorph, NPCs y comercio, magia y lo social (grupo, intercambio, clan). Por ahora se probó
**solo en macOS** (Apple Silicon); los puntos que faltan para Windows y Linux están marcados en el
código con `MULTIPLATAFORMA` (ver la sección final).

Estado detallado y evidencia de cada entregable: **[ENTREGABLES.md](ENTREGABLES.md)**.
Para trabajar en el proyecto (personas o agentes, también en paralelo): **[AGENTS.md](AGENTS.md)**.

![Juego](docs/capturas/3_juego.png)

## Requisitos (macOS)

- Xcode Command Line Tools (Apple Clang con C++20)
- CMake ≥ 3.25 y Ninja: `brew install cmake ninja`
- La instalación del juego (la carpeta con `Sprite*.pak`, `Tile.pak`, `Text.pak`, `map/`, `font/`)

SDL3 3.4.18, stb_image y doctest se descargan solos en la primera configuración (FetchContent,
versiones fijas). No hace falta instalar nada más.

## Compilar y probar

```sh
cd cliente
cmake --preset debug          # o release / debug-asan
cmake --build --preset debug
LINEAGE_DIR="/ruta/al/juego" ctest --preset debug   # sin LINEAGE_DIR se saltan los tests con recursos
```

## Servidor local de pruebas

Todo el desarrollo se hace contra un servidor L1J-JP 3.63c local (puerto 2000); no se toca el
servidor privado.

```sh
servidor_local/instalar.sh    # una vez: baja el servidor, crea la base en MariaDB (puerto 3307)
servidor_local/arrancar.sh    # arranca MariaDB y el servidor (Ctrl+C para detenerlo)
servidor_local/detener.sh     # detiene MariaDB
```

`instalar.sh` corrige lo necesario para que ese servidor (de 2013) funcione con Java y MariaDB
actuales:

- reemplaza el conector MySQL 5.1.25 por la versión 5.1.49;
- agrega JAXB, que ya no viene con Java;
- usa `zeroDateTimeBehavior=round` para las fechas `0000-00-00`;
- arranca con idioma inglés y zona horaria `Asia/Tokyo`;
- arregla filas inválidas de la base (`corregir_db.sql`).

Las cuentas se crean solas al primer login.

`herramientas/prueba_login.py` es un cliente mínimo en Python: recorre login, lista, creación de
personaje y entrada al juego, e imprime cada paquete. Sirve para depurar el servidor o el protocolo.

## Ejecutar

```sh
build/debug/lineage --juego="/ruta/al/juego" [--host=127.0.0.1] [--puerto=2000] [--usuario=X]
```

- La configuración se guarda en `~/Library/Application Support/Lineage/LineageCliente/settings.ini`.
- Si no se indica la carpeta del juego, en el primer arranque se abre el diálogo nativo para elegirla.
- **Controles en el juego:** mantener el clic izquierdo para caminar hacia el puntero, o las flechas
  (combinadas dan las diagonales).
- **Atacar:** mantener el clic sobre un monstruo (o cualquier personaje). Si está lejos, el personaje camina hacia él; al lado, ataca con el ritmo de su animación de ataque. Soltar el clic deja de atacar. El objetivo se marca con el nombre en amarillo.
- **Atacar** (ajuste del entregable 6): el clic ataca solo a monstruos (tipo de gfx 10). A otros personajes y NPCs, solo con Ctrl/⌘ apretado.
- **Al morir:** aparece un aviso con el botón **Restart**, que manda `C_Restart`.
- **Alt+I:** abre y cierra el inventario.
  - Muestra los íconos originales (`N.tbt`), el nombre al pasar el mouse, los equipados con una "E" y el peso.
  - Doble clic usa el ítem: equipa o quita armas y armaduras, bebe pociones.
  - Arrastrar un ítem fuera de la ventana lo suelta al suelo.
- **Ítems del suelo:** clic sobre un ítem para recogerlo; si está lejos, el personaje camina hasta él. El nombre aparece al pasar el mouse por encima.
- **Chat:**
  - escribir y Enter para enviar; Esc borra lo escrito;
  - rueda del mouse sobre el panel, o Re Pág / Av Pág, para ver mensajes anteriores;
  - prefijos: `!` grito, `&` global, `$` comercio, `@` clan, `#` grupo, `%` alianza, `*` grupo de chat, `"nombre texto` susurro.
- **Alt+C:** abre y cierra la ventana de estado (a la izquierda del inventario si los dos están abiertos).
- **Vista del juego:** se adapta a la ventana. Si la agrandas o la estiras se ve más mapa, y el panel queda centrado abajo. Las pantallas de login y personajes siguen en 640×480.
- **Ctrl/⌘ + y Ctrl/⌘ −:** acercan y alejan el zoom (escala entera; alejarlo muestra más mapa).
- **Ctrl/⌘ 0:** vuelve al zoom automático, el mayor con el que entran 640×480.
- **Zoom guardado:** queda en `settings.ini` (`zoom=`, 0 = automático) y también se puede pasar como `--zoom=N`.
- **F2:** cambia la UI del juego entre la simple (por defecto: franja de texto con chat, entrada y barras ASCII, y ventana de estado de texto) y el panel original con sus imágenes. Se guarda en `settings.ini` (`ui_simple=1/0`); también `--ui_simple=0`.
- **F3:** fps, CPU por frame (promedio, peor frame y decodificación), allocations del peor frame, instancias, llamadas de dibujo, objetos y posición.
- **F4:** cambia la cámara entre "suave" (por defecto: sigue la posición continua y el personaje queda centrado) y "a saltos" (como el original: sigue los frames de caminar, de a ¼ de celda). Se guarda en `settings.ini` (`camara_a_saltos=0/1`).
- **Alt+1..Alt+0:** barra de atajos (abajo al centro). Se arrastra un hechizo (desde Alt+S) o un ítem (desde Alt+I) a un casillero; clic o Alt+número lo usa, clic derecho lo vacía, arrastrar entre casilleros los mueve. Se guarda por personaje (`barra=` en `settings.ini`).
- **Clic sobre un NPC:** camina hasta él y le habla (su página aparece arriba a la izquierda; Escape la cierra). En la tienda o el almacén: clic suma 1, Shift+clic 10, clic derecho resta; OK confirma.
- **Ventanas:** se mueven arrastrando la barra de título (la posición se guarda). Se abren también con los botones: los del panel original o los de texto a la derecha del chat (UI simple). **Options** tiene la interfaz, la cámara, la sombra, la opacidad de las ventanas, el zoom y "Reset" de posiciones.
- **Aprender magia:** en el maestro de magia (Gereng en Talking Island...), "Learn magic" abre la lista: clic elige y OK compra. Los libros de magia se usan con doble clic dentro de un templo.
- **Alt+S:** ventana de hechizos (clic lanza; con el puntero sobre un hechizo, F5–F12 lo asigna a esa tecla).
- **F5–F12:** lanzan el hechizo asignado (se guardan por personaje en `settings.ini`, `atajos=`). Un hechizo con objetivo espera el clic sobre el objetivo; clic derecho cancela.
- **Social** (botón, o el del panel original): grupo, intercambio y clan con botones y ayuda. Las preguntas del servidor (invitaciones, pedidos de intercambio o de entrar al clan) aparecen con **Yes/No** (Enter/Escape).
- **Intercambio:** hay que estar al lado del otro y mirándolo. Se arrastra un ítem del inventario a la ventana (o doble clic con el intercambio abierto), y OK cuando los dos están conformes.
- **Comandos de chat:** `/invite Nombre`, `/party`, `/leave`, `/kick Nombre` (grupo); `/trade`; `/clan Nombre` (crear, príncipe con 30000 adena), `/join` (frente a un príncipe), `/pledge`, `/leaveclan`, `/banclan Nombre`, `/emblem` (sube un emblema de muestra); `/title [Nombre] Título`; `/who [Nombre]`. Lo que no es un comando va al chat.
- **Nombres:** azul con alineamiento ≥ 500, rojo con ≤ −500; el título arriba y el emblema del clan a la izquierda.
- **Shift+F4:** oscurece a los personajes parados en la sombra de un árbol (por defecto) o no. Se guarda en `settings.ini` (`sombra_personajes=1/0`).
- **Shift+F12:** guarda `captura.bmp`.
- **Esc:** en el login, sale.

### Automatización (pruebas y capturas)

| Argumento | Efecto |
|---|---|
| `--auto` | Hace login con `--usuario`/`--clave`, acepta las noticias y entra con el personaje |
| `--personaje=Nombre` | Personaje a elegir (por defecto, el primero) |
| `--crear` | Crea `--personaje` (príncipe, puntos repartidos en orden) si no existe |
| `--pasos=N` | Al entrar, camina N pasos solo |
| `--captura=x.bmp --captura-pantalla=login\|personajes\|crear\|juego --captura-espera=F` | Guarda una captura F frames después de llegar a esa pantalla |
| `--salir` | Sale después de la captura |
| `--depuracion` | Muestra el overlay de F3 desde el inicio |
| `--perf` | Una línea por segundo en el registro: fps, CPU promedio y peor frame, decodificación, allocations, páginas del atlas, reemplazos y precargas en curso |
| `LIN_ATLAS_PAGINAS=4 LIN_ATLAS_TAM=1024` (variables de entorno) | Achican el atlas para probar el reemplazo de páginas |
| `--decir=texto` | Al entrar, escribe esa línea en el chat (se puede repetir; una por segundo, con prefijos) |
| `--estado` | Abre la ventana de estado al entrar |
| `--ventana=1600x900` | Tamaño inicial de la ventana, en puntos |
| `--ruta=355` | Al entrar, camina esas direcciones (dígitos 0–7, una por paso) |
| `--atacar=Rabbit` | Ataca al personaje vivo más cercano cuyo nombre contenga ese texto, uno tras otro |
| `--reiniciar` | Al morir, reinicia solo después de 5 s |
| `--usar=Potion` / `--soltar=Adena` | Usa o suelta el primer ítem cuyo nombre lo contenga (se pueden repetir; una acción cada 2 s) |
| `--recoger` | Camina hasta el ítem del suelo más cercano y lo recoge |
| `--inventario` | Abre el inventario al entrar |
| `--hechizos` | Abre la ventana de hechizos al entrar |
| `--hablar=Fraoun` | Camina hasta el NPC más cercano cuyo nombre lo contenga y le habla |
| `--npc-accion=buy` | En la página del NPC, sigue ese vínculo (o manda la acción); se puede repetir, uno cada 2 s |
| `--tienda="Healing:3"` / `--tienda="#0:2"` | En la próxima lista de tienda o almacén, elige esa cantidad (por nombre o por fila) y confirma |
| `--forma="death knight"` | Polymorph: cuando se abre el diálogo (pergamino o Shape Change), elige ese monstruo |
| `--subir=str` | En la página RaiseAttr (nivel 51+), sube ese atributo (`str`, `dex`, `con`, `int`, `wis`, `cha`) |
| `--salida` | Abre la ventana Exit (Restart / Quit) al entrar |
| `--opciones` | Abre la ventana de opciones al entrar |
| `--captura-proyectil=x.bmp` | Guarda una captura cuando el primer proyectil (flecha o hechizo) va por la mitad del vuelo, y sale |
| `--arrastrar=x1,y1,x2,y2` | Simula arrastrar el mouse (coordenadas lógicas) desde el frame 300 del juego: apretar, mover en 30 frames y soltar |
| `--alt=3` | Como apretar Alt+3 (barra de atajos); se puede repetir, uno cada 3 s. La barra se puede precargar con `--barra="Nombre:h3,i80:0,..."` |
| `--lanzar="Energy Bolt@Goblin"` / `--lanzar=Shield` | Lanza ese hechizo (sobre el más cercano cuyo nombre contenga lo que sigue a `@`); se puede repetir, uno cada 3 s |
| `--aceptar` | Responde que sí a toda pregunta del servidor (grupo, intercambio, clan) y da OK en el intercambio cuando el otro ya ofreció algo |
| `--girar=2` | Mira hacia esa dirección (0–7) en el frame 200 del juego: para quedar frente a otro personaje (intercambio, `/join`) |
| `--ofrecer=Healing` | Cuando se abre un intercambio, ofrece todo el primer ítem que lo contenga y da OK 1,5 s después |
| `--social` | Abre la ventana Social al entrar |
| `LIN_CONFIG=/tmp/x.ini` (variable de entorno) | Usa ese `settings.ini` en lugar del del usuario (pruebas en paralelo) |
| `LIN_HTML=1` (variable de entorno) | Registra cada página HTML que se abre, con sus datos |
| `LIN_TRAZA=1` (variable de entorno) | Registra cada paquete enviado y recibido, con sus primeros 16 bytes |

Ejemplo, la captura del entregable 3:

```sh
build/release/lineage --juego="$LINEAGE_DIR" --usuario=prueba --clave=prueba --auto \
    --personaje=Prueba --pasos=20 --depuracion --captura=juego.bmp --captura-espera=600 --salir
```

### Explorador de recursos

```sh
build/debug/explorador --juego="$LINEAGE_DIR" --gfx=0 --accion=3   # ←/→ gfx, ↑/↓ acción
build/debug/explorador --juego="$LINEAGE_DIR" --png=814            # imagen suelta (--img=N)
```

Muestra un gfx en las 8 direcciones sobre rombos de referencia. Con él se calibró el ancla de los
sprites.

## Estructura (diseño orientado a datos)

| Módulo | Contenido |
|---|---|
| `src/core` | Tipos, `Arena` (asignación lineal por frame), `MapaHash` (direccionamiento abierto), `TablaSoa` (tabla columnar con borrado por intercambio), `AnilloBytes` (cola SPSC sin locks), lector binario acotado |
| `src/plataforma` | Archivos mapeados en memoria, socket TCP y configuración. Solo este módulo y `render` dependen del sistema o de SDL |
| `src/proto` | Cifrado, tabla de opcodes (JP 3.63c), decodificador con tabla de punteros a función (paquete → `Evento` POD), comandos e hilos de red (lector y escritor) |
| `src/recursos` | `.idx/.pak` (búsqueda por hash del nombre en `SpriteNN.pak`), DES de `Text.pak`, `.img`/`.png` (incluido el PNG ofuscado), `.til`, `.spr` (frames compuestos), tabla de sprites, `.s32`, fuentes `eng.fnt`/`kor.fnt`, tablas de texto |
| `src/mundo` | Tabla de objetos SoA, estado del jugador, segmentos de mapa y aplicación de eventos en lote. La posición interpolada y el frame de animación son funciones puras del tiempo. `chat.h`: registro en anillo de tamaño fijo, globos, mensajes del servidor con `string-e.tbl` y prefijos de canal |
| `src/render` | SDL_GPU: atlas en *texture array*, lista de dibujo con radix sort y **una sola llamada instanciada por frame**; render a 640×480 escalado con factor entero |
| `src/ui` | UI inmediata: botones con imagen N/N+1 y cajas de texto |
| `src/app` | Bucle principal y pantallas (login, personajes, creación, juego) |
| `tools/explorador.cpp` | Explorador de recursos |
| `tests/` | doctest: core, cifrado (vectores de `docs/analisis/scripts/lineage_proto.py`), DES, protocolo, recursos reales, mapa contra el servidor, mundo y chat |
| `datos/sprite_table.txt` | Tabla de acciones de sprites de l1j-client (GPLv3); esta instalación solo trae `list.enx`, cifrado |

## Pendiente para Windows y Linux (`grep -rn MULTIPLATAFORMA`)

| Lugar | Qué falta |
|---|---|
| `src/render/gpu.cpp` | El shader está en MSL (Metal). Escribirlo en HLSL, compilarlo con SDL_shadercross a SPIR-V (Vulkan/Linux) y DXIL (D3D12/Windows) y elegir el formato con `SDL_GetGPUShaderFormats()` |
| `src/plataforma/archivo_mapeado.cpp` | Windows: `CreateFileMappingW` / `MapViewOfFile`, y rutas UTF-8 → UTF-16 (`_wfopen`). Hoy hay un `#error` |
| `src/plataforma/socket.cpp` / `socket.h` | Windows: Winsock2 (`WSAStartup`, `closesocket`, tipo `SOCKET`). Hoy hay un `#error`. Linux ya está cubierto (POSIX y `MSG_NOSIGNAL`) |
| `src/plataforma/rutas.cpp`, `src/mundo/mundo.cpp` | Linux distingue mayúsculas: buscar `Sprite.pak`, `map/`... sin distinguirlas |
| `CMakeLists.txt` | MSVC: opciones equivalentes ya puestas, sin probar. Empaquetado `.app` / `.exe` / AppImage (entregable 11) |
| `servidor_local/*.sh` | Linux: paquetes del sistema en lugar de brew. Windows: los `.bat` del propio servidor |

## Licencia

El código de formatos se escribió a partir de la lectura de l1j-client (GPLv3), y
`datos/sprite_table.txt` viene de ese proyecto. Por eso este cliente debe distribuirse como
**GPLv3**. Los recursos del juego no se incluyen: se leen de la instalación del usuario.

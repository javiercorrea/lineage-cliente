# Cliente Lineage (C++20 + SDL3)

Cliente nuevo de Lineage 1 que usa los recursos de la instalación original (abandonware) y habla
el protocolo L1J. Implementa los **entregables 1 a 10** del plan (`docs/analisis/PLAN_CLIENTE.md`):
login, selección y creación de personaje, entrar al juego y moverse, chat y estado, combate,
inventario e ítems, hechizos y polymorph, NPCs y comercio, magia, lo social (grupo, intercambio, clan) y
el mundo completo (música por zona, sonidos, clima y portales). Por ahora se probó
**solo en macOS** (Apple Silicon); los puntos que faltan para Windows y Linux están marcados en el
código con `MULTIPLATAFORMA` (ver la sección final).

Estado detallado y evidencia de cada entregable: **[ENTREGABLES.md](ENTREGABLES.md)**.
Descubrimientos (formatos, tiempos, dibujo, servidor) y decisiones de diseño: **[docs/DESCUBRIMIENTOS.md](docs/DESCUBRIMIENTOS.md)**.
Para trabajar en el proyecto (personas o agentes, también en paralelo): **[AGENTS.md](AGENTS.md)**.

![Juego](docs/capturas/3_juego.png)

## Requisitos (macOS)

- Xcode Command Line Tools (Apple Clang con C++20)
- CMake ≥ 3.25 y Ninja: `brew install cmake ninja`
- La instalación del juego (la carpeta con `Sprite*.pak`, `Tile.pak`, `Text.pak`, `map/`, `font/`)

SDL3 3.4.18, stb_image, dr_mp3 y doctest se descargan solos en la primera configuración (FetchContent,
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
build/debug/lineage
```

- **Sin parámetros:** todo sale de `settings.ini`
  (`~/Library/Application Support/Lineage/LineageCliente/settings.ini`) y de la interfaz.
- **Editor de ajustes:** `build/<preset>/ajustes` abre un formulario con pestañas (Juego, Interfaz,
  Audio, Servidores, Personajes) para editar `settings.ini` sin texto plano. Usa la misma UI simple
  del juego y escribe el mismo archivo. `--config=RUTA` edita otro archivo.
- **Carpeta del juego:** en el primer arranque se elige con el diálogo nativo y queda guardada
  (`juego=`).
- **Servidores:** se eligen en el menú del login. Hay uno por línea en `settings.ini`:

  ```ini
  servidor=Local de pruebas|127.0.0.1|2000|jp363|cuenta
  servidor=Otro|play.ejemplo.com|2000|s3ep1|
  servidor_elegido=Local de pruebas
  ```

  Los campos son nombre, host, puerto, versión de protocolo (`jp363` o `tw380`, el 3.80C de Taiwán) y
  la última cuenta usada en ese servidor.
  La clave no se guarda. Un `settings.ini` viejo (`host=`, `puerto=`...) se convierte solo.
- **El tamaño de la ventana** se guarda al cerrar (`ventana=AxB`).
- **Controles en el juego:** mantener el clic izquierdo para caminar hacia el puntero, o las flechas
  (combinadas dan las diagonales).
- **Atacar:** mantener el clic sobre un monstruo (o cualquier personaje). Si está lejos, el personaje camina hacia él; al lado, ataca con el ritmo de su animación de ataque. Soltar el clic deja de atacar. El objetivo se marca con el nombre en amarillo.
- **Atacar** (ajuste del entregable 6): el clic ataca solo a monstruos (tipo de gfx 10). A otros personajes y NPCs, solo con Ctrl/⌘ apretado.
- **Al morir:** aparece un aviso con el botón **Restart**, que manda `C_Restart`.
- **Alt+I:** abre y cierra el inventario.
  - Muestra los íconos originales (`N.tbt`), el nombre al pasar el mouse, los equipados con una "E" y el peso.
  - Doble clic usa el ítem: equipa o quita armas y armaduras, bebe pociones.
  - Arrastrar un ítem fuera de la ventana lo suelta al suelo.
  - **Detalle de un ítem:** al pasar el mouse se ve el nombre y, si está identificado, sus atributos (daño o defensa, material, peso, encantamiento, clases que lo usan, bonos de atributos, MR, resistencias...). Si no lo está, dice "Unidentified".
  - **Pergaminos que se usan sobre otro ítem** (identificar, encantar arma o armadura): doble clic en el pergamino y después clic en el ítem; Esc cancela.
  - **Basurero** (abajo a la derecha del inventario): arrastrar un ítem encima lo elimina, para lo que no se puede soltar ni vender. Siempre pregunta antes: cuántos de una pila, o que confirme si es uno. El servidor rechaza lo equipado y lo que no se puede borrar.
- **Techos y árboles:** al entrar a una casa o pasar detrás de un árbol, lo que tapa al personaje se vuelve translúcido y se ve su silueta.
- **Puertas:** clic sobre una puerta para abrirla o cerrarla (se camina hasta ella). Las cerradas no se pueden atravesar.
- **Dar ítems:** arrastrar un ítem del inventario sobre un NPC o una mascota se lo da; de una pila, pregunta cuántos.
- **Mascotas e invocaciones:** su lista va a la izquierda de la barra de atajos, con la vida. Clic en el nombre abre su menú de órdenes (atacar, defender, quedarse...); "Bag" muestra su inventario, donde un clic usa o equipa el ítem. Con "Attack", el próximo clic elige su objetivo.
- **Reparar armas:** en un herrero, el vínculo de reparar muestra las armas dañadas con su costo; un clic repara.
- **Marcadores:** "/bookmark nombre" guarda el lugar (si el mapa lo permite) y "/delbookmark nombre" lo borra. Con marcadores, el pergamino de teleport y Teleport preguntan el destino.
- **Cambiar de personaje:** Exit > "Character select".
- **Estados alterados:** envenenado (verde), congelado (gris), ciego (pantalla negra salvo el propio personaje) y un aviso arriba si está paralizado, dormido, aturdido o atado (no se puede mover ni actuar).
- **Monstruos sin sprite:** los que faltan en esta instalación se ven como un bloque magenta y se atacan con el clic como cualquier otro.
- **Atacar:** clic sobre un monstruo y mantener: el personaje camina hasta él y lo ataca aunque la cámara se mueva y el puntero ya no esté encima (el objetivo se fija al apretar). Soltar deja de atacar, salvo con **A** (con el chat cerrado): sigue atacando al objetivo, o al monstruo bajo el puntero, hasta que muera, se camine con las flechas o se haga otro clic.
- **Forzar ataque:** Ctrl+clic (o ⌘+clic) ataca a un NPC o a otro jugador. **Modo PK:** "/pk" en el chat lo activa o desactiva (indicador rojo "PK" arriba a la derecha): un clic sobre otro jugador lo ataca sin Ctrl. Es del cliente; el servidor decide si el ataque vale (en zonas seguras no hace daño).
- **Ítems del suelo:** clic sobre un ítem para recogerlo; si está lejos, el personaje camina hasta él. El nombre aparece al pasar el mouse por encima.
- **Chat:**
  - **Enter abre la línea del chat** (abajo dice "Enter: chat"); se escribe y Enter envía y la cierra; Esc la vacía y la cierra. Mientras está cerrada, las teclas son acciones (1–0 usan la barra). En Options, "Chat: Always typing" vuelve a escribir siempre, con la barra en Alt+número (`chat_con_enter=0` en `settings.ini`);
  - rueda del mouse sobre el panel, o Re Pág / Av Pág, para ver mensajes anteriores;
  - prefijos: `!` grito, `&` global, `$` comercio, `@` clan, `#` grupo, `%` alianza, `*` grupo de chat, `"nombre texto` susurro.
- **Alt+C:** abre y cierra la ventana de estado (a la izquierda del inventario si los dos están abiertos).
- **Vista del juego:** se adapta a la ventana. Si la agrandas o la estiras se ve más mapa, y el panel queda centrado abajo. Las pantallas de login y personajes siguen en 640×480.
- **Ctrl/⌘ + y Ctrl/⌘ −:** acercan y alejan el zoom (escala entera; alejarlo muestra más mapa).
- **Ctrl/⌘ 0:** vuelve al zoom automático, el mayor con el que entran 640×480.
- **Zoom guardado:** queda en `settings.ini` (`zoom=`, 0 = automático).
- **F2:** cambia la UI del juego entre la simple (por defecto: franja de texto con chat, entrada y barras ASCII, y ventana de estado de texto) y el panel original con sus imágenes. Se guarda en `settings.ini` (`ui_simple=1/0`). También vale para el login, la selección y la creación de personaje: con la UI simple son recuadros de texto (la lista de todos los personajes con sus datos, borrar pide confirmación).
- **Logo propio en el login (UI simple):** `logo=mi_logo.png` en `settings.ini`: un PNG con transparencia de hasta 2048×2048, ruta absoluta o relativa a la carpeta de `settings.ini`. Se achica para entrar en 560×190 (mejor darlo de ese tamaño, porque se escala sin suavizado). Sin logo, o si no se puede leer, se muestra el título en texto.
- **F3:** fps, CPU por frame (promedio, peor frame y decodificación), allocations del peor frame, instancias, llamadas de dibujo, objetos y posición.
- **F4:** cambia la cámara entre "suave" (por defecto: sigue la posición continua y el personaje queda centrado) y "a saltos" (como el original: sigue los frames de caminar, de a ¼ de celda). Se guarda en `settings.ini` (`camara_a_saltos=0/1`).
- **1..0 (o Alt+1..Alt+0):** barra de atajos (abajo al centro). Se arrastra un hechizo (desde Alt+S) o un ítem (desde Alt+I) a un casillero; clic, el número o Alt+número lo usa, clic derecho lo vacía, arrastrar entre casilleros los mueve. Clic del medio (rueda) sobre un casillero lo elige (marco dorado) y el botón del medio sobre un personaje o monstruo usa ahí el hechizo elegido; mantenido, lo repite cada vez que termina la espera (un ítem, cada medio segundo). Los casilleros de hechizos (barra y F5–F12) muestran la espera hasta el próximo hechizo: una sombra que se vacía y los segundos que faltan. Se guarda por personaje (`barra=` en `settings.ini`).
- **Clic sobre un NPC:** camina hasta él y le habla (su página aparece arriba a la izquierda; Escape la cierra). En la tienda o el almacén: clic suma 1, Shift+clic 10, clic derecho resta; OK confirma.
- **Ventanas:** se mueven arrastrando la barra de título (la posición se guarda). Al abrirse buscan un lugar libre para no tapar a las otras; si se superponen, el clic trae una adelante y solo esa recibe el mouse. Se abren también con los botones: los del panel original o los de texto a la derecha del chat (UI simple). **Options** tiene la interfaz, la cámara, la sombra, la opacidad de las ventanas, el zoom y "Reset" de posiciones.
- **Aprender magia:** en el maestro de magia (Gereng en Talking Island...), "Learn magic" abre la lista: clic elige y OK compra. Los libros de magia se usan con doble clic dentro de un templo.
- **Alt+S:** ventana de hechizos (clic lanza; con el puntero sobre un hechizo, F5–F12 lo asigna a esa tecla).
- **F5–F12:** lanzan el hechizo asignado (se guardan por personaje en `settings.ini`, `atajos=`). Un hechizo con objetivo espera el clic sobre el objetivo; clic derecho cancela.
- **Social** (botón, o el del panel original): grupo, intercambio y clan con botones y ayuda. Las preguntas del servidor (invitaciones, pedidos de intercambio o de entrar al clan) aparecen con **Yes/No** (Enter/Escape).
- **Intercambio:** hay que estar al lado del otro y mirándolo. Se arrastra un ítem del inventario a la ventana (o doble clic con el intercambio abierto), y OK cuando los dos están conformes.
- **Comandos de chat:** `/invite Nombre`, `/party`, `/leave`, `/kick Nombre` (grupo); `/trade`; `/clan Nombre` (crear, príncipe con 30000 adena), `/join` (frente a un príncipe), `/pledge`, `/leaveclan`, `/banclan Nombre`, `/emblem` (sube un emblema de muestra); `/title [Nombre] Título`; `/who [Nombre]`; `/exclude Nombre` (bloquea o desbloquea; sin nombre, la lista); `/buddy`, `/addbuddy Nombre`, `/delbuddy Nombre` (amigos); `/chatparty`, `/chatoutparty`, `/chatbanish Nombre` (grupo de chat). Lo que no es un comando va al chat.
- **Tienda de jugadores** (en las salas del mercado): clic en un vendedor (sentado, con el título encima) muestra lo que vende y lo que compra; clic en una línea pregunta cuántos. `/shop Título` arma la propia: clic en un ítem del inventario pide el precio; "Open" la abre y `/shop` otra vez la cierra.
- **Tablón de los pueblos:** clic en el tablón: los temas, leerlos, "Write" publica (300 adena) y el autor borra el suyo. **Correo:** `/mail` (bandejas Inbox, Clan y Saved; "Write" a un personaje por 50 adena o a todo el clan).
- **Grupo:** a la izquierda, a media altura, la lista de los otros miembros con su vida; el líder con "*".
- **Nombres:** azul con alineamiento ≥ 500, rojo con ≤ −500; el título arriba y el emblema del clan a la izquierda.
- **F1:** ayuda con todas las teclas.
- **Sonido:** música por zona, efectos de las animaciones y ambiente. El volumen de música y efectos está en **Options**.
- **Portales:** para entrar a una mazmorra o subir una escalera, caminar hacia la entrada con las flechas o hacer clic justo sobre ella.
- **Shift+F4:** oscurece a los personajes parados en la sombra de un árbol (por defecto) o no. Se guarda en `settings.ini` (`sombra_personajes=1/0`).
- **Shift+F12:** guarda `captura.bmp`.
- **Esc:** en el login, sale.

### Control en vivo (pruebas y capturas; solo en desarrollo)

Las versiones `debug` y `debug-asan` (opción de CMake `LIN_CONTROL`) abren un socket de archivo
(Unix domain socket) y reciben comandos de a una línea. La versión `release` no lo incluye: un paso
de CMake (`cmake/sin_control.cmake`) falla si se cuela.

- **Socket:** `$LIN_CONTROL`, o `$TMPDIR/lineage-control-<pid>.sock`. Al arrancar, la ruta se escribe
  en la salida.
- **Respuestas:** cada respuesta termina con `ok` o `error <motivo>`.
- **Esperas:** los comandos que esperan algo (`login`, `personaje`, `esperar`, `captura`) responden
  cuando se cumple o vence el plazo, sin frenar el juego.
- **Codificación:** el texto va en CP949, como el del juego.
- **`LIN_CONFIG=/tmp/x.ini`** (solo en desarrollo): usa otro `settings.ini`, para no pisar el del
  usuario y correr varios clientes a la vez.

| Comandos | Qué hacen |
|---|---|
| `ayuda` | Lista los comandos |
| `estado`, `objetos [radio]`, `inventario`, `html`, `ventanas`, `grupo`, `chat [n]`, `paquetes [n]` | Consultas: pantalla, vista y personaje (y asedios), objetos cercanos (id, nombre, gfx, celda y posición en pantalla), ítems, la página del NPC con sus vínculos, ventanas abiertas (y dónde están las móviles), el grupo, el chat, los últimos paquetes |
| `login <cuenta> <clave> [servidor]`, `personaje <nombre>`, `crear <nombre> <clase>` | Entrar (esperan la lista de personajes o estar en el juego) |
| `decir "texto"` | Como escrito en el chat (comandos `/` y `.` del GM incluidos) |
| `hablar <obj>`, `atacar <obj>`, `parar`, `clic-objeto <obj> [der]` | `<obj>`: id, `#gfx` o parte del nombre (el más cercano) |
| `accion <vínculo>`, `responder si\|no\|"texto"`, `cantidad <n>` | Página del NPC, preguntas del servidor, diálogo de cantidad |
| `usar <ítem> [sobre <ítem>]`, `soltar`, `dar <ítem> <obj>`, `borrar`, `lanzar <hechizo> [obj]` | Ítems y hechizos |
| `girar <0-7>`, `ofrecer <ítem>`, `aceptar-intercambio`, `cancelar-intercambio` | Intercambio entre dos clientes |
| `abrir\|cerrar inventario\|estado\|hechizos\|opciones\|social\|salida\|teclas` | Ventanas |
| `clic <x> <y> [der\|medio]`, `arrastrar x1 y1 x2 y2`, `mantener x y frames`, `tecla <nombre>` (`ctrl+`, `alt+`, `shift+`), `texto "..."`, `ctrl on\|off` | Entrada cruda (coordenadas lógicas) |
| `esperar pantalla=\|paquete=\|html=\|ventana=\|dialogo=\|chat=\|frames=\|ms= [plazo=ms]` | Esperar algo en vez de contar frames (`paquete=` y `chat=` cuentan también lo llegado desde el comando anterior) |
| `captura <ruta.bmp>`, `mudo on\|off`, `traza on\|off`, `traza-html on\|off`, `perf on\|off`, `depuracion on\|off`, `salir` | Capturas y depuración |

`herramientas/lin_ctl.py` lo usa desde la terminal (`lin_ctl.py <socket> estado`, o interactivo) o
como módulo para escribir pruebas.

**Pruebas en vivo** (`herramientas/pruebas/`):
- una por tema: login, moverse, chat y estado, combate y hechizos, inventario, equipo, NPC, tablón y
  correo, subastas, social (dos clientes);
- **las que modifican y reinician el servidor local:** casas, castillos y guerra. Las restauran solas
  con `herramientas/pruebas/servidor.py`.

```sh
herramientas/pruebas/todas.py /tmp/prueba.ini /tmp/capturas              # las que no dejan cambios
herramientas/pruebas/todas.py /tmp/prueba.ini /tmp/capturas --servidor   # todas
herramientas/pruebas/npc.py /tmp/prueba.ini /tmp/capturas                # una sola
```

El `settings.ini` de prueba necesita `juego=`. Cada prueba deja a los personajes donde estaban y
cierra sus clientes aunque falle. Por ejemplo, el comienzo de `subastas.py`:

```python
c = Control.arrancar(config="/tmp/prueba.ini")
c("mudo on"); c("login prueba2 prueba2"); c("personaje Charla")
c("hablar Auction"); c("esperar ventana=subastas")
c("captura /tmp/subastas.bmp"); c.cerrar()
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

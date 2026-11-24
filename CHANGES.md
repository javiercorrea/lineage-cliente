# Cambios

Cambios entre versiones del cliente. Cada versión es un tag `vX.Y.Z` sobre `main`; al subir el tag,
`.github/workflows/release.yml` publica la release con el `.dmg` de macOS. El detalle de cada función,
con su verificación y lo pendiente, está en [docs/upstream/ENTREGABLES.md](docs/upstream/ENTREGABLES.md).

Al hacer una versión nueva: agregar su sección arriba de todo, con los PRs mergeados desde el tag
anterior (`git log --first-parent <tag anterior>..main`).

## v0.2.3 (2026-11-23)

### Agregado
- **Tabla de sprites real del juego:** el cliente descifra solo el `list.spz` de `Text.pak` (AES propio, sin
  dependencias) y lo usa cuando no hay una lista en texto plano: 13.540 entradas, con las filas reales de los
  polymorphs 75/80 y de muchos gfx que antes no tenían fila (#59).
- **Efectos fijos de los mapas:** fuegos, braseros, chispas, vapor y algas de la capa 8 de los `.s32` se dibujan en
  bucle, y las luces fijas del mapa (y las de los fuegos) alumbran de noche y en las mazmorras, con los radios del
  juego original (#60).
- **Lanzador por servidor (opcional):** un comando en la línea del servidor que abre un proxy local e imprime su
  puerto; el cliente lo corre al entrar, se conecta a ese puerto y lo detiene al salir o antes de otro lanzador.
  Plazo de 30 s (#61, #62).

### Cambiado
- **Animaciones como el exe:** las acciones, ataques y pasos que llegan mientras un objeto está ocupado esperan en
  una cola en vez de cortar la animación, y los cuadros duran según haste, brave, wafer, tercer acelerador y Triple
  Arrow / Foe Slayer. El factor se fija al empezar cada acción (#61, #62).
- **README al día:** Windows hecho, paquetes, ajustes en el juego e issues de este repo (#57).
- **Formatos de pak documentados:** `docs/analisis/FORMATOS_PAK.md` (índices clásico, `_EXT`, `_RMS` y `IDXC`, el
  AES de `list.spz`, tiles Remastered y la capa 8) (#59, #60).

## v0.2.2 (2026-11-22)

### Agregado
- **Botón Sort en la interfaz original:** junto a la X del inventario; el orden guardado se aplica en las dos
  interfaces (#55).
- **`perf on` informa el tiempo de GPU** por cuadro (#54).

### Cambiado
- **Con un filtro de escalado, la escena se dibuja a la grilla lógica** y el filtro escribe la imagen final: menos
  memoria y trabajo de GPU a escala alta (#54).
- **Flechas de las barras de scroll:** triángulos hacia arriba y hacia abajo en lugar de "^" y "v" (#55).

### Corregido
- **Temblor al moverse en diagonal** con un filtro de escalado (xBR, FSR, CRT, Anime4K) (#54).
- **Pergamino de teleport:** el normal va siempre al azar; solo el bendito deja elegir un marcador, y lo deja aunque
  no se tenga el Ring of Teleport Control (#55).

## v0.2.1 (2026-11-22)

### Agregado
- **Instalar con un comando** en Linux, macOS y BSD:
  `curl -fsSL https://raw.githubusercontent.com/javiercorrea/lineage-cliente/latest/instalar.sh | sh`.
  Instala lo que falte para compilar (pregunta antes), baja el código, compila e instala en el usuario; volver a
  correrlo actualiza. El README trae también el paso a paso a mano (#53).
- **Paquetes de Linux** (x64 y arm64) en la release: AppImage y `.tar.gz` (#53).
- **`cmake --install`** (o `make install` / `ninja install`) instala el cliente, su lanzador y, en Linux y BSD, el
  acceso directo del escritorio (#53).
- **Tag `latest`:** apunta siempre a la última versión publicada; lo mueve la release (#53).

## v0.2.0 (2026-11-22)

### Agregado
- **Fork de fxd0h** ([fxd0h/lineage-cliente](https://github.com/fxd0h/lineage-cliente)), traído con sus correcciones (#51):
  - **Interfaz original del cliente US 2009** (`ui_simple=0`, F2 alterna): panel, ventanas, cursores, íconos de buff,
    globos, colores, orbe de la hora y el resto del HUD, con capas, posiciones y fuentes leídas del binario de 2009.
  - **Protocolos:** además de `jp363`, `tw380` (Taiwán 3.80C, texto Big5) y `s3ep1` (US 2009), con un servidor de
    pruebas local para cada uno.
  - **Linux** probado en el juego (Vulkan), con instancias de prueba invisibles en Xvfb.
  - **Dibujo de sprites del original:** modos de mezcla, capas de ropa, techos que se desvanecen, casas que se
    transparentan, destellos de luz y sonidos por cuadro.
  - **Filtros de escalado** (`escalado=`, Shift+F5): entero, xBR, FSR, CRT y Anime4K, y una vista fija de 640×480
    opcional (`vista=fija`).
  - **Carpetas extra del juego** (`juego_extra=`) para lo que le falta a la instalación principal, y pack HD de la
    interfaz opcional (`hd=`).
  - **Juego:** emblemas de clan, alianzas, barco, música de combate, pesca, cocina, mapas del mundo, tiendas
    privadas, mascotas e invocaciones.
- **Ajustes:** filas para carpetas extra, pack HD, fuente original, escalado, vista y transparencia original (#51).
- **Pantalla completa en Mac:** también con Cmd+Enter y Ctrl+Cmd+F (#51).

### Cambiado
- **C++23.** Los comentarios del código están en español; el README queda en inglés como portada (#51).
- **Íconos de buff:** la interfaz simple usa los del cliente original, con su tiempo restante (#51).
- **Casilleros Auto:** si no entran junto a la barra de atajos, van en una fila arriba de ella (#51).
- **`CHANGES.md`** vuelve a la raíz del repo (#52).

### Corregido
- **Taiwán 3.80C:** los nombres con caracteres chinos se mandan en Big5 (susurros, intercambio, correo, clan y
  grupo); antes salían en UTF-8 y el servidor no los encontraba (#51).
- **Protocolo:** no se mandan comandos que no existen en la versión del servidor; el barco mandaba cada 180 s un
  paquete con opcode 0 en `jp363` y `tw380` (#51).
- **Cursor en Retina:** se veía al doble de tamaño (#51).
- **Zoom:** con la vista fija no hacía nada; ahora acerca y aleja el mapa (#51).
- **Desplegables de Ajustes:** al abrirse se veía el texto de los controles de abajo y costaba elegir (#51).

## v0.1.5 (2026-11-20)

### Agregado
- **Tabla real de sprites del cliente:** se lee `TW13032701.txt` (o un `list.spz` descifrado) de la carpeta del
  juego, o la que indique `lista_sprites=`, y agrega los gfx que `sprite_table.txt` no tiene (llega al 8682),
  validados contra los `.pak`. Con la del cliente de Reborn: 2156 gfx con animaciones y 1384 con atributos.
  Antes se veían quietos o como un bloque magenta (#47).
- **Ventana de ajustes en el juego:** el formulario de configuración (Juego, Interfaz, Audio, Servidores,
  Personajes) se abre desde el login (Ajustes, u Options en la interfaz original) y en el juego (Options).
  Guardar aplica los cambios en el momento; la carpeta del juego y la tabla de sprites avisan que valen al
  reiniciar el cliente. Suma Texto HD, globos con recuadro, tabla de sprites y quitar el orden del
  inventario de un personaje (#48).

### Cambiado
- **Opciones y "Lineage Settings":** la ventana Options y el programa aparte de ajustes se reemplazan por la
  ventana de ajustes; el `.dmg` y el `.zip` de Windows traen solo el juego (#48).
- **CI:** no compila cuando un cambio es solo de documentación (Markdown, `docs/`, `LICENSE`) (#48).

### Corregido
- **Muertes:** el cuerpo ya no se levanta y vuelve a caer al morir; la caída termina en el cadáver (#47).

## v0.1.4 (2026-11-19)

### Agregado
- **Windows (x64 y arm64):** el cliente compila con Visual Studio y dibuja con D3D12 (Winsock2, archivos
  mapeados con `MapViewOfFile` y rutas UTF-16, control en vivo por TCP). La release trae un `.zip` por
  arquitectura con ícono, versión y el runtime de C++ estático. Probado en Windows 11 arm64 en una VM (#20).
- **Linux (x64 y arm64):** compila y pasa los tests (Vulkan); los archivos del juego se buscan sin distinguir
  mayúsculas (`Sprite.pak`, `Map/`, `Music1012.mp3`). Falta probar el juego completo en Linux (#20).
- **Texto HD** (`texto_hd=1`, Options "HD text"): los glifos se suavizan a la resolución real de la
  pantalla en vez de agrandar cada píxel de la fuente (#44).

### Cambiado
- **Shaders:** una sola fuente en HLSL, compilada a SPIR-V, DXIL y MSL. En macOS hace falta
  `brew install glslang spirv-cross` para compilar (#20).
- **Zoom con la UI simple:** Ctrl −/+ cambia solo el mapa; el panel, las ventanas y los nombres sobre los
  personajes quedan del mismo tamaño (#44).
- **CI:** compila y corre los tests en macOS, Windows y Linux (x64 y arm64), sin avisos de MSVC `/W4` y con
  los avisos como errores (#20).

### Corregido
- **Ítems:** el byte de bendición se leía al revés (0 es bendito, 1 normal) y los ítems normales salían
  en amarillo, como benditos (#44).
- **CA** por debajo de -128 se mostraba positiva (#44).

## v0.1.3 (2026-11-18)

### Agregado
- `CHANGES.md` con los cambios entre versiones (#40).

### Corregido
- **Inventario:** el orden (Sort o mover a otra celda) se guarda por personaje en settings.ini y se
  mantiene al reiniciar el cliente (#41).
- **Mapas negros:** se leen los segmentos en el formato viejo `.seg`, que algunas zonas tienen en vez de
  `.s32` (el Chaotic Temple, alrededor de Gludio y partes del mapa 371) (#42).

## v0.1.2 (2026-11-17)

### Agregado
- **Inventario:** botón "Sort" (equipado primero, después por tipo y por nombre) y arrastrar un ítem a
  otra celda para ordenarlo a mano. Shift+clic marca varios ítems, y arrastrarlos lleva todos al suelo,
  al basurero (una sola confirmación), al intercambio, a un NPC o a otra celda (#38).
- **Barra Auto:** pociones de maná (azul), sabiduría, haste (común y mayor) y brave, y la elven wafer,
  además de los hechizos (#35).
- **Tienda de NPC:** la cantidad de una pila se escribe directamente en el diálogo (#36).

### Cambiado
- **Auto-buffs y auto-poción** comparten un turno: si vencen varios a la vez salen de a uno, con
  200 ms entre ellos, primero la curación (#39).
- **Invocaciones propias:** un clic sobre ellas les habla en vez de atacarlas (#37).

## v0.1.1 (2026-11-11)

### Agregado
- **Barra de atajos:** páginas con selector y rueda (#27); atajo elegido sin objetivo, con borde y punto
  de página (#28); mantener 1–0 lanza curaciones y buffs sobre uno mismo (#30).
- **Auto-buffs:** seis casilleros que relanzan los buffs al terminarse (#31, #33, #34).
- **Auto-poción** por personaje, con umbral de HP (#32).
- Plan de jugabilidad en `docs/` (#26).

### Corregido
- **Red:** cancelar la conexión, keepalive en el hilo de red y otras caídas (#21).
- **Render:** la interfaz ya no se pierde con muchas instancias, los emblemas vuelven y menos caídas de la
  GPU (#22); con todas las páginas del atlas en uso se vacía la menos dibujada (#25).
- **Control en vivo:** `forzar-hora` respondía dos `ok` y desfasaba las respuestas de `lin_ctl` (#23).

### Rendimiento
- Anchos de la fuente precalculados, audio cargado en otro hilo y listas que se vacían (#24).

## v0.1.0 (2026-11-09)

Primera versión publicada. Cliente de Lineage 1 en C++20 + SDL3 (SDL_GPU) para macOS que lee los
recursos de una instalación original y habla el protocolo L1J (JP 3.63c; también 3.80C).

### Incluye
- **Entregables 1 a 10:** login, selección y creación de personaje, moverse, chat y estado, combate,
  inventario e ítems, hechizos, polymorph, NPCs y comercio, magia, social (grupo, intercambio, clan) y
  mundo completo (música, sonidos, clima, portales).
- Casas y castillos, tienda de jugadores, tablón y correo, equipo del personaje, almacén, basurero.
- Editor visual de `settings.ini` (#2, #8); interfaz en 5 idiomas con fuente TTF (#3, #5, #6).
- Luz: noche, mazmorras, agua, visión de elfo y borrachera (#7, #9).
- Ventana sin borde con barra propia y pantalla completa (#10, #11, #12).
- Jugabilidad: teleport, polymorph, scroll en ventanas y tiendas, muerte transformada, mascotas, globos
  de cómic (#13, #14, #15); colores de títulos y lawful, transparencias y miles (#16, #17).
- Íconos y empaquetado como `.dmg`, con release automática al subir un tag (#18, #19).

# Cambios

Cambios entre versiones del cliente. Cada versión es un tag `vX.Y.Z` sobre `main`; al subir el tag,
`.github/workflows/release.yml` publica la release con el `.dmg` de macOS. El detalle de cada función,
con su verificación y lo pendiente, está en [ENTREGABLES.md](ENTREGABLES.md).

Al hacer una versión nueva: agregar su sección arriba de todo, con los PRs mergeados desde el tag
anterior (`git log --first-parent <tag anterior>..main`).

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

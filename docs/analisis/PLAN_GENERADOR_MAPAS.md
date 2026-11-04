# Plan: generador de mapas (isla/abierto y dungeon/laberinto)

**Estado:** propuesta, sin empezar. Alcance elegido: **generado offline + explorar con el cliente
actual**. Cero cambios de protocolo; funciona con los servidores que ya tenemos (jp363, s3ep1/classic,
tw380). Lo "dinámico en vivo" y el encoder `.s32` quedan como extensiones (sección 6).

## 1. Cómo funcionan los mapas hoy (la base del plan)

La geometría **no viaja por el protocolo**. Existe por duplicado, indexada por `id`:

- **Cliente** (instalación del usuario, no en el repo): `map/<id>/<XXXXYYYY>.s32`, segmentos de 64×64
  celdas con tiles visuales + atributos de colisión. Es lo que se **dibuja** y lo que el cliente usa
  para pasabilidad y proyectiles. Formato en `src/recursos/mapa.h`; todo se convierte a `SegmentoMapa`.
  La carga está en `mundo.cpp:segmento()` y en el hilo `mundo/cargador_segmentos.cpp`, ambas leyendo
  `rec.carpeta() + nombre_segmento(id, x, y)`.
- **Servidor** (l1j): `maps/<id>.txt`, un CSV de **1 byte por celda** (`ancho×alto`), más una fila en
  la tabla `mapids` (`startX/endX/startY/endY`, flags: `underwater`, `teleportable`, `escapable`,
  `markable`, `take_pets`, `penalty`…). Es lo que el server usa para validar movimiento, pathfinding y
  LOS. Lector: `TextMapReader.java`; cacheado al arrancar por `MapsTable`/`CachedMapReader`.
- **Protocolo:** solo `S_MapID` (id + flag submarino) al cambiar de mapa, más objetos y movimiento. El
  cliente carga los tiles de ese id de disco. **Por eso un mapa generado se expresa como el par de
  archivos de un id nuevo**, no como un paquete.

**Restricción de arte:** un mapa generado solo puede usar **tiles que ya existen** en la instalación
(pastos/agua/arena; pisos/paredes de mazmorra). No se inventan sprites. El valor de suelo es
`juego_de_tiles << 8 | tile` y el atributo es la máscara de pasabilidad por media celda.

## 2. Idea central

Un **generador offline** produce, para un `id` de prueba libre, lo mínimo de cada lado para que cliente
y servidor carguen ese id como cualquier mapa:

```
generar(id, modo, semilla, ancho, alto)
  ├─ servidor:  maps/<id>.txt           (CSV de colisión, formato estándar l1j)
  │             corregir_db.sql         (fila mapids idempotente)
  └─ cliente:   mapas_generados/<id>/…  (formato propio liviano -> SegmentoMapa)
```

La colisión del `.txt` y los atributos del cliente **salen del mismo grid**: una sola fuente de verdad
por celda (pasable / bloqueada / agua), traducida a los dos formatos. Así no se desincronizan.

**Paleta de tiles por cosecha:** en vez de hardcodear ids, un script lee qué tiles usan mapas
existentes (29 = isla/pueblo, un mapa de mazmorra = pisos/paredes) y arma una paleta por bioma. El
generador muestrea de ahí. Bootstrapping con arte real, sin adivinar ids.

## 3. Lado servidor (estándar, sin tocar código Java)

1. **`maps/<id>.txt`**: el grid de colisión como CSV. El byte por celda sigue la convención de l1j
   (bits de pasabilidad/puente/sin-techo; se replican los valores que ya usan los mapas vecinos para
   "pasable" y "bloqueada", verificados leyendo un `.txt` existente).
2. **`mapids`**: fila idempotente en `servidor_classic/corregir_db.sql` (y el equivalente de cada
   servidor que se quiera usar), con `startX/endX/startY/endY` = el rectángulo del mapa y flags
   razonables para pruebas (`teleportable=1`, `escapable=1`, `penalty=0`).
3. **Reproducible** como pide AGENTS.md §1.5: el generador **emite** estos archivos; no se editan a
   mano. Instalarlos = copiar el `.txt` a `maps/` y correr el `.sql`. El server los toma al **reiniciar**
   (cachea al arrancar; recargar en caliente es la extensión de la sección 6).

## 4. Lado cliente (el único código nuevo del cliente)

Cargador liviano para mapas generados, que **evita escribir un encoder `.s32`**:

1. **Formato propio** `mapas_generados/<id>/<seg>.mg` (o un solo archivo si entra): por celda, el
   tile de suelo (de la paleta) y el atributo. Objetos del mapa (árboles, paredes decorativas):
   opcional en v1; un laberinto puede dibujarse solo con tiles de piso/pared sin objetos levantados.
2. **Un `decodificar_mg(...) -> SegmentoMapa`** análogo a `decodificar_s32`. `SegmentoMapa` es la
   moneda común; el resto del render y la pasabilidad no cambian.
3. **Directorio override:** `mundo.cpp:segmento()` y `cargador_segmentos.cpp` prueban primero una
   carpeta de mapas generados (config `mapas_generados=` o env `LIN_MAPAS`), y si no, la instalación.
   Así los mapas de prueba **no ensucian la instalación del usuario** (AGENTS.md §5). Es el enganche
   mínimo: una función `ruta_segmento(id, x, y)` que decide carpeta y extensión.
4. **Tests:** `decodificar_mg` con bytes fijos -> `SegmentoMapa` esperado (como hace
   `test_recursos.cpp` con `.s32`).

## 5. Generación: dos modos

Módulo aparte (Python en `herramientas/mapas/`, junto a las otras herramientas; no entra al binario).
Produce el grid lógico (una sola fuente) y lo vuelca a los formatos de §3 y §4.

- **isla / abierto:** ruido (valor/Perlin) -> altura; umbral = costa; borde de agua garantizado;
  suavizado; caminos. Agua = celda bloqueada (o nadable si se quiere, según flag del mapa).
- **dungeon / laberinto:** salas por BSP + corredores que las conectan, **o** un laberinto
  (DFS/Prim). Pared = celda bloqueada.
- **Self-check obligatorio** (regla "lógica nueva = un chequeo"): el grid es **conexo** — un BFS desde
  la celda de spawn alcanza toda celda caminable. Un mapa con zonas aisladas es un bug, no un mapa.
  Además: hay al menos un spawn válido y el borde está cerrado (no se sale del rectángulo).

## 6. Explorar (sin herramienta nueva)

Se reusa el cliente actual. Flujo: generar id N -> copiar `maps/N.txt` + correr el `.sql` ->
reiniciar el server -> apuntar `LIN_MAPAS` a la carpeta generada -> entrar con el socket de control
(`herramientas/lin_ctl.py`), teleport al mapa N (`.moveloc`/`.move`), caminar y capturar. La
exploración y la validación visual salen gratis con lo que ya existe.

## 7. Fuera de este alcance (extensiones posteriores)

- **Dinámico en vivo (regenerar sin reiniciar):** parche al server con un comando admin "recargar
  mapa N" (invalidar la caché de `MapsTable`/`CachedMapReader`) + recargar la caché de segmentos del
  cliente para ese id. Es la única fricción real del "dinámico de verdad"; el generador es el mismo.
- **Encoder `.s32`:** emitir el formato oficial en vez del liviano, para que un cliente stock también
  cargue el mapa. Más caro (tilesets, objetos, portales) y solo hace falta si se quiere interoperar
  con el cliente oficial.
- **Objetos del mapa** (árboles/casas levantadas) y **portales** entre mapas generados.
- **Spawns de monstruos/NPC** en el mapa generado (tablas `spawnlist`), para que sea jugable y no
  solo caminable.

## 8. Riesgos

- **Valores de tile/atributo mal elegidos** dibujan basura o rompen la colisión. Se mitiga cosechando
  de mapas reales y comparando contra un `.s32` existente decodificado.
- **Desincronía cliente/servidor:** si el grid de colisión del `.txt` no coincide con el atributo del
  cliente, el jugador "rebota" (el server rechaza un movimiento que el cliente creyó válido). Por eso
  §2 insiste en **un solo grid** traducido a ambos.
- **Id en uso:** elegir un id que ningún mapa real ocupe (rango alto libre) para no pisar contenido.
- **El override de carpeta** toca `segmento()` y el hilo cargador: cambiar la construcción de la ruta
  en un solo lugar compartido para no divergir.

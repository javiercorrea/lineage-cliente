# Formatos de .idx/.pak y archivos cifrados

**Estado:** implementados el formato clásico, `_EXT` y `_RMS` (con zlib, brotli y el DES de Text), el AES de
`list.spz` y `Data` en lugar de `Text` (`src/recursos/pak.cpp`, `des.cpp`, `aes.cpp`; pruebas en `tests/test_pak.cpp`).
Falta `_EXTB$` (dos descripciones distintas, sin un archivo real para decidir) y los tiles Remastered de 48×48 (§4).
`_EXT` / `_RMS` están probados con índices armados a mano, todavía no con un cliente Remastered real.

**Fuentes:** [srwh1234/L1MapViewer](https://github.com/srwh1234/L1MapViewer) (`Reader/L1IdxReader.cs`,
`Reader/L1PakReader.cs`, `Algorithm.cs`, `Helper/L1MapHelper.cs`), su fork más nuevo
[tony1223/L1MapViewer](https://github.com/tony1223/L1MapViewer) (`364fcff`, 2026-07-16: `plans/til-block-spec.md`,
`CLI/S32Parser.cs`, `Reader/L1IdxReader.cs`) y [tony1223/PakViewer](https://github.com/tony1223/PakViewer) (`fbad692`:
`PakReader.cs`, `src/Lin.Helper.Core/Pak/IdxHandler.cs`). **Ninguno de los L1MapViewer tiene licencia:** de ahí tomamos
el formato y las constantes (datos), no el código; nuestra implementación es propia (AES según FIPS-197). Ninguno
menciona `IDXC`.

Todos los enteros son little-endian.

## 1. Cómo reconocer el índice

| Primeros bytes | Formato | Entrada | Estado |
|---|---|---|---|
| `u32` cantidad (sin marca) | clásico | 28 bytes | implementado |
| `_EXTB$` | extendido de bloques | largo variable | sin implementar |
| `_EXT` (sin `B$` después) | extendido | 128 bytes | implementado (§3) |
| `_RMS` | Remastered | 276 bytes | implementado (§3) |
| `IDXC 02 01` | propio de Lineage Justice (cifrado por bloques) | ? | **no**: ver §6 |

## 2. Clásico (implementado)

- `.idx`: `u32 n`, después `n` entradas de 28 bytes: `u32 offset`, `char nombre[20]` (terminado en 0),
  `u32 tamaño`.
- **Text.idx:** las `n × 28` bytes de entradas van cifradas con DES-ECB, clave `~!@#%^$<`
  (`CLAVE_DES_LINEAGE`), solo los bloques completos de 8; el resto queda igual. Los archivos de Text.pak
  también van con DES (`Recursos::texto`).
- **Sprites:** `SpriteNN.pak` con `NN = (suma de bytes del nombre en minúsculas) & 15`, después `Sprite.pak`.

### 2.1 Archivos con AES dentro de Text/Data (implementado para list.spz)

Algunos archivos, una vez sacados del pak (y del DES si van en Text), tienen una segunda capa:

| Archivo | Bytes en claro al principio | Cómo se reconoce |
|---|---|---|
| `list.spz` | 5 (`S1354` en la instalación de 2009) | empieza con `S` |
| `.xml`, `.json`, `.ui` | 4 | empieza con `X`; al descifrar, la `X` se cambia por `<` |

El resto va con **AES-128-CBC sin relleno**:
- Clave `DC 84 01 21 2A 40 20 0A DD 25 B9 A7 0D B9 C9 4E`.
- Vector inicial `3E 09 78 AA C4 D5 30 63 30 0C 5F 9A 80 7F 22 46`.
- Solo los bloques completos de 16 bytes. Los `n % 16` bytes finales no pasan por AES: cada uno va con XOR
  contra el byte de la misma posición del **último bloque completo todavía cifrado**.

Implementación: `descifrar_aes_lineage` (`src/recursos/aes.cpp`), probada con FIPS-197 C.1 y NIST SP 800-38A
F.2.2 (`tests/test_aes.cpp`). El cliente descifra el `list.spz` de Text.pak cuando no hay una lista en texto
plano (`cargar_lista_sprites` en `src/app/app.cpp`). El de la instalación de 2009 da 13.540 entradas, ids
0–13543; trae, entre otras, las filas reales de los polymorphs 75/80 que antes copiábamos a mano.

Los `.xml` no los necesitamos hoy (son la UI del Remastered). Para leerlos basta llamar a
`descifrar_aes_lineage` desde el byte 4 y cambiar la `X` inicial por `<`.

## 3. `_EXT` y `_RMS` (implementado)

Las dos fuentes coinciden en la entrada. La de PakViewer (`IdxHandler.cs`, `FromExtEntry`) es la más completa:

```
Cabecera (8 bytes):   char marca[4] ("_EXT" o "_RMS"), i32 cantidad
Entrada:              u32 offset en el .pak
                      i32 tamaño sin comprimir
                      i32 tamaño guardado (comprimido; 0 si no lo está)
                      i32 compresión: 0 ninguna, 1 zlib, 2 brotli
                      char nombre[112] (_EXT, entrada de 128) o nombre[260] (_RMS, entrada de 276),
                      terminado en 0; en _RMS puede traer subcarpetas ("data/xxx.html")
```

- **Leer un archivo:** se leen `tamaño guardado` bytes (o `tamaño sin comprimir` si no está comprimido) desde
  el offset y se descomprimen. zlib trae su cabecera (`78 9C`/`78 DA`); brotli es un flujo crudo.
- **Text en `_EXT`:** en algunas versiones (V880, según PakViewer) las entradas y los archivos van además con
  el DES de §2. PakViewer prueba primero sin descifrar y, si los nombres no tienen sentido, con DES; para las
  entradas, el DES empieza en el byte 8 (después de la cabecera), no en el 4.
- **`_EXTB$`**: hay dos descripciones y hay que verificar con un archivo real cuál vale.
  - Entradas fijas (`PakReader.cs:470` de PakViewer y `LoadExtB` del fork de L1MapViewer, que la usa para
    `Tile.idx`): cabecera de 16 bytes y entradas de 128: `u32 ?` (clave de orden), `i32 compresión`,
    `char nombre[112]`, `u32 offset`, `i32 tamaño sin comprimir`. No guarda el tamaño comprimido: se calcula como
    la distancia al offset siguiente en orden.
  - Entradas variables (`ExtBHandler` de PakViewer): después de la marca de 6 bytes, `i32 cantidad` y entradas
    `u32 offset`, `i32 tamaño`, `i32 tamaño comprimido`, nombre terminado en 0. Datos en brotli y, si falla,
    zlib sin los 2 bytes de cabecera.

### Implementación

- `Pak::abrir` mira los primeros bytes y elige el lector; `_EXTB$` e `IDXC` caen en el clásico y se rechazan (la
  cantidad sale enorme).
- Cada entrada guarda el tamaño guardado y la compresión. `Pak::buscar` devuelve una vista al `.pak` mapeado si el
  archivo no va comprimido ni cifrado; si no, lo decodifica (DES, después zlib o brotli) la primera vez y lo guarda
  en el `Pak`, así la vista sigue valiendo (`decodificados_`; `shortcut:` no se libera, pasar a un LRU si hace falta).
  Un archivo que no se puede decodificar devuelve vacío y deja una línea en el log.
- zlib (`v1.3.1`, solo inflate) y brotli (`v1.1.0`, solo el decodificador) se compilan desde sus fuentes en
  `cmake/dependencias.cmake`.
- Los nombres con carpeta (`data/list.spz`) se buscan por el nombre solo; ante dos iguales en carpetas distintas
  gana el último del índice.
- Sin `Text.idx`, `Recursos` abre `Data` como texto, y `carpeta_juego_valida` acepta `Data.idx/.pak` en su lugar.
  Text en `_EXT` / `_RMS`: si menos de la mitad de las entradas son creíbles sin descifrar, se descifran con DES
  (entradas desde el byte 8 y datos antes de descomprimir).

## 4. Mapas y tiles Remastered (para implementar)

Según `Helper/L1MapHelper.cs`:

- **Detección:** L1MapViewer usa la carpeta `bin32/` de la instalación. Mejor detectarlo por el tamaño de los
  tiles (abajo), que no depende de la instalación.
- **`.s32`:** mismo formato que el clásico (capa 1: 64×128 celdas de `u8 índice, u16 til, u8 ?`; capa 2:
  `u16 n` × 6 bytes; capa 3: 64×64 × 4 bytes de atributos; capa 4: objetos). En Remastered los `.seg` se
  ignoran y solo se usan los `.s32`.
- **Tiles al doble:** cada medio rombo mide 48×24 en vez de 24×12, así que un bloque de mapa ocupa
  64·48·2 × 64·24·2 px (6144 × 3072) en vez de 3072 × 1536. En `drawTilBlockR` el origen de cada celda es
  `x0 = −48·(x/2)`, `y0 = 63·24 − 24·(x/2)`.
- **El tipo de cada bloque dice el tamaño** (`plans/til-block-spec.md` del fork). Un `.til` es `i32 n` (casi
  siempre 256), `n + 1` offsets relativos al inicio de los datos (el último marca el final; varios bloques
  pueden compartir datos) y los bloques, cada uno con un byte de tipo:

  | Tipo | Forma | Tamaño |
  |---|---|---|
  | 0, 1, 8, 9 | rombo simple (RGB555 fila por fila, 2, 4… 24… 4, 2 px; 1/9 con transparencia) | 24×24 |
  | 16, 17 | rombo simple | **48×48, Remastered** |
  | 2, 3, 6, 7, 22, 23, 34, 35 | comprimido: `u8 x, u8 y, u8 ancho, u8 alto` + filas en RLE | 24×24 |
  | 18, 19 | comprimido | **48×48, Remastered** |

  Cualquier otro tipo es un bloque dañado. Hay además tiles "híbridos" con coordenadas de 48 y píxeles de 24
  (un comprimido con `x`/`y` > 24 pero ancho y alto ≤ 24). Para clasificar un `.til` entero, el fork mira el bloque
  más grande: ≥ 1800 bytes es Remastered, 10–1000 es clásico, y si no, revisa los bloques.
- Los tiles irregulares ya los resolvemos por el bit 2 del tipo en `src/recursos/tiles.cpp`; lo nuevo es el bit 4
  (16), que marca los de 48.

### Plan

1. `tiles.cpp`: decidir el tamaño por el bit 4 del tipo (16 = 48×48) en vez de suponer 24×24, y guardar la
   escala en la tabla de tiles. Un bloque de tipo desconocido se descarta.
2. Render: dibujar el bloque Remastered a la mitad (a la cuadrícula lógica de 24×12, igual que hoy) o
   aprovecharlo como textura de doble resolución en las vistas con escala ≥ 2.
3. Probar con un `.til` Remastered de cada tipo y un `.s32` contra una captura de L1MapViewer.

## 5. Capa 8 del `.s32`: luces y efectos fijos del mapa (implementado)

Después de los portales, el `.s32` trae una octava capa (formato según `CLI/S32Parser.cs` del fork, confirmado
con el cargador del exe de 2009, `0x460580`):

```
u16 n          bit alto en 1: cada entrada trae 4 bytes más (solo el cliente nuevo de Justice)
n × { u16 gfx, u16 x, u16 y [, i32 ?] }      x, y: celda absoluta del juego
```

En la instalación de 2009, 8158 de 8375 `.s32` la tienen, **2801 con entradas**, y el archivo termina justo ahí
(0 bytes sobrantes en todos). En el cliente nuevo de Justice, 1408 usan la forma extendida.

El original crea un objeto por entrada al cargar el segmento (`0x45f840` -> `FUN_004cc980`), con una luz que depende
solo del gfx. Los "dot range" 3442–3446 son luces sin imagen de radio 5–13; braseros, fuegos, chispas y lava traen su
luz; algas y vapor solo se dibujan. Implementación: `SegmentoMapa::efectos` (`src/recursos/mapa.cpp`),
`luz_efecto_mapa` (`src/mundo/luz.cpp`, con la tabla completa del exe) y `dibujar_efectos_mapa`
(`src/app/pantalla_juego.cpp`).

Falta saber qué guarda el `i32` de las entradas extendidas (se salta).

## 6. Nombres de zona (opcional)

`zone3desc-c.tbl` (Text) y `zone3-c.xml` (Tile o Data; `zone3.tbl`/`zone2-c.tbl` en clientes viejos) dan el
nombre de cada zona del mapa (`<zoneinfo><zone><zone3desc>…</zone3desc><map>…</map>`). Servirían para mostrar
el nombre de la zona en el minimapa.

## 7. Lineage Justice (`IDXC`): no

El cliente nuevo de Justice usa índices `IDXC 02 01` con el cuerpo cifrado por bloques de 16 bytes, y su
launcher prohíbe expresamente analizarlo. No aparece en ninguna de las fuentes de arriba. Para leerlo hace
falta que sus operadores publiquen el formato o lo autoricen. Sus mapas sueltos (`map/`) sí son el formato
clásico y se pueden usar con `juego_extra=`.

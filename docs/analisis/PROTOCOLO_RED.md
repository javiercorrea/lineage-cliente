# Protocolo de red del cliente Lineage

Este documento describe cómo se comunica el cliente con el servidor: el transporte, el entramado de paquetes, el cifrado y el despacho de opcodes. La implementación de referencia está en `analisis/scripts/lineage_proto.py`.

## Fuentes y nivel de certeza

| Archivo | Qué es | Utilidad para el análisis |
|---|---|---|
| `Lineage.exe` | Lanzador oficial de NCSoft (2014), sin empaquetar | Lanza `Lin.bin` (o `lito.bin`). Acepta `/host:` y `/portno:`. |
| `lin.bin` | Cliente real (compilado el 12-11-2014) | **Empaquetado/protegido** (secciones con nombres aleatorios y entropía de ~8, al estilo Themida/WinLicense). No se puede leer estáticamente. |
| `TW13032701.bin` | Cliente taiwanés (27-03-2013) | También empaquetado. |
| `S3EP1.bin` / **`S3EP1U.bin`** | Cliente de 2009 y **su volcado desempaquetado** | Es la fuente de todo lo que sigue. |
| `Login.exe` + `Login.dll` + `Login.ini` | Lanzador del servidor privado ("Loginer363") | `Login.ini` guarda la IP y el puerto de "Justice Live" y "Justice Test" cifrados en `ServerData`. |

**Verificado** significa que se leyó en el código desensamblado de `S3EP1U.bin` y, en el caso del cifrado, que se ejecutaron en emulación (Unicorn) las rutinas originales del cliente y coinciden byte a byte con `lineage_proto.py`: 1500 paquetes aleatorios y 300 semillas.

**Inferido**: que `lin.bin` (2014) use el mismo entramado y el mismo cifrado. La razón es que este esquema se mantuvo durante muchas versiones y los emuladores (L1J y derivados) lo usan con clientes posteriores. Lo que **sí cambia entre versiones son los números de opcode**. Los de `lin.bin` no se pudieron extraer porque el binario está protegido.

---

## 1. Transporte

- **TCP**, un solo socket por sesión. Winsock en modo asíncrono con `WSAAsyncSelect`. Los eventos llegan como mensajes de ventana:
  - `0x401` (`WM_USER+1`): evento del socket. La palabra baja es el evento (`FD_READ`, `FD_CLOSE`…) y la alta el código de error. El cliente trata en particular `10061` (`WSAECONNREFUSED`) y `10060` (`WSAETIMEDOUT`), con reintento hacia otro servidor.
  - `0x402`: resultado de `WSAAsyncGetHostByName`. A continuación hace `connect()` con un `sockaddr_in` de 16 bytes.
- Host y puerto: el lanzador los pasa a `lin.bin` por línea de comandos (`/host:`, `/portno:`). El puerto habitual en los servidores Lineage/L1J es 2000, pero ese valor **no se verificó** en estos binarios.

## 2. Entramado (framing)

Todos los paquetes, en ambos sentidos, tienen esta forma:

```
+--------+--------+----------------------------------+
| len lo | len hi |  cuerpo (len - 2 bytes)          |
+--------+--------+----------------------------------+
  uint16 little-endian = longitud TOTAL, incluida la cabecera
```

- El cuerpo viaja **cifrado**. La cabecera de longitud nunca se cifra.
- El primer byte del cuerpo, una vez descifrado, es el **opcode**.
- La recepción es una máquina de dos estados (`0x4937e5`–`0x4938a2`): primero lee 2 bytes, calcula `len-2`, luego lee el cuerpo completo, lo descifra y lo despacha.
- **Relleno del cliente** (`mySend`, `0x4a7a30`): antes de cifrar, el cliente redondea el cuerpo hacia arriba a un **múltiplo de 4** (`total = ((total+1) & ~3) + 2`) y reescribe la cabecera con esa longitud. Los bytes de relleno son basura o ceros del búfer. El servidor no debe asumir que el cuerpo termina justo donde acaban los datos útiles.
- Control de ráfagas en el envío: cada 60 paquetes el cliente consulta `GetTickCount()` y no envía si no han pasado 3000 ms desde la ventana anterior. Es un detalle del cliente, no forma parte del formato.

## 3. Tipos de campo

El cliente lee los paquetes con un intérprete de cadenas de formato (`0x483100`, al estilo de `scanf`). Los códigos son:

| Código | Tipo | Codificación |
|---|---|---|
| `c` | byte | 1 byte |
| `h` | uint16 | 2 bytes, **little-endian** |
| `d` | uint32 | 4 bytes, **little-endian** |
| `s` | cadena | bytes terminados en `\0` (página de códigos local: CP949 en KR, Big5 en TW) |
| `S` | cadena ancha | UTF-16LE terminada en `\0\0` |

Ejemplos que aparecen en el propio binario: `"d"`, `"c"`, `"cc"`, `"h"`, `"cdddddcc"`, `"sdh"`, `"csss"`, `"chcs"`.

El envío usa un constructor análogo (`0x4a7be0`) que arma el cuerpo, antepone la longitud y llama a `mySend`.

## 4. Cifrado

### 4.1 Paquete de inicio y semilla

1. El cliente conecta. El **servidor habla primero**.
2. El primer paquete del servidor es el de inicio, con opcode **`0x7D` (125) en el cliente de 2009**. Llega **sin cifrar**: el receptor solo omite el descifrado mientras no hay clave (`flag 0x8ed654 == 0`) y el opcode es `0x7D`.
3. Su cuerpo: `[0x7D] [d: semilla uint32 LE]`. Las versiones posteriores añaden más bytes después de la semilla.
4. A partir de la semilla se derivan **dos claves de 8 bytes idénticas**: una para enviar (`0x8ed660`) y otra para recibir (`0x8ed658`). Cada una evoluciona por separado.

### 4.2 Derivación de la clave

En el binario, la derivación (`0x481b20`) aparece como una variante de **Blowfish**: usa las tablas P y S estándar, 16 rondas y está ofuscada. Al emularla, **el resultado es exactamente** esta fórmula simple:

```python
k0 = rotl32(seed ^ 0x9C30D539, 19)
k1 = 0x930FD7E2 ^ k0 ^ 0x7C72E993
key = pack('<II', k0, k1)       # 8 bytes
```

Las tres constantes salen de las S-box de Blowfish.

### 4.3 Cifrado de un cuerpo (cliente → servidor), `0x4a7980`

```
d[0] ^= k[0]
for i in 1..n-1:  d[i] ^= d[i-1] ^ k[i & 7]     # encadenado con el byte YA cifrado
d[3] ^= k[2]
d[2] ^= d[3] ^ k[3]
d[1] ^= d[2] ^ k[4]
d[0] ^= d[1] ^ k[5]
```

### 4.4 Descifrado (servidor → cliente), `0x4a7900`

Es la operación inversa: primero deshace el mezclado de los bytes 0–3 y después el XOR encadenado (ver `Cipher.decrypt`).

### 4.5 Actualización de la clave tras cada paquete

Después de cifrar o descifrar **cada** paquete, en cada sentido por separado:

```
P = primeros 4 bytes del cuerpo EN CLARO, como uint32 LE (opcode incluido)
k0 ^= P
k1 = (k1 + 0x287EFFC3) mod 2^32
```

Por eso el flujo es **con estado**: si se pierde un paquete o se descifra fuera de orden, la sesión queda desincronizada. Para decodificar una captura (pcap) hay que procesar el flujo TCP completo desde el paquete de inicio.

## 5. Despacho de opcodes (servidor → cliente)

El receptor (`0x493240`) tiene dos tablas según el estado del cliente:

- **Fuera del juego** (login y selección de personaje): una tabla dispersa de 18 opcodes. Cualquier otro se ignora.
- **En el juego** (estado 3): una tabla de 128 entradas (`0x00`–`0x7F`), de las cuales 110 tienen un manejador propio. Un opcode `> 0x7F` se trata como error fatal.

Opcodes previos al juego en el cliente de 2009, con lo identificado hasta ahora:

| Opcode | Formato | Función (identificada en el manejador y contrastada con l1j-client) |
|---|---|---|
| 125 `0x7D` | `d` | **Inicio y semilla de cifrado** (verificado) |
| 89 `0x59` | `c`, luego `cdddddcc` `c` | **Versión del servidor**: `serverCode`, versiones, hora de arranque, `canMakeNewAccount`, `englishOnly`, `countryCode` (define la página de códigos). Coincide con `server_version_packet` de l1j-client |
| 66 `0x42` | `s` | Noticias o anuncio. El cliente lo guarda en `Capture\News%04d.txt` |
| 74 `0x4A` | `h` `c` `s`… | Mensaje del servidor por ID: el número indexa la tabla de textos (`Text.pak`) y le siguen hasta 3 argumentos |
| 80 `0x50` | `cc` (`c` en builds antiguas) | Número de personajes y máximo. Reinicia 8 ranuras de 0x30 bytes |
| 56 `0x38` | `sscchhhccccccccc` | Una entrada de la lista de personajes: nombre, clan, clase, sexo, alineamiento, PV, PM, CA, nivel y atributos |
| 102 `0x66` | (mismo manejador que 56) | Variante de la entrada de personaje, probablemente "personaje recién creado" |
| 41 `0x29` | `c` | Resultado de login; 22 = cuenta en uso, el mismo código que en L1J |
| 62 `0x3E` | `c` subtipo + … | Paquete multipropósito con unos 75 subtipos (clan, coliseo, estadísticas de usuarios…) |
| 5, 23, 29, 63, 79, 84, 116 | `cdsch`, `cd`… | Comparten un manejador. Sin identificar |
| 88 `0x58` | `cccccc` | Entrada al juego (`S_LoginGame` en L1J): guarda 6 bytes en variables protegidas y cambia de estado |
| 4 | `c` | Sin identificar; abre una ventana que responde con el opcode 12 |

Los formatos de los 110 opcodes del juego están en `resultados/formatos_opcodes_juego.txt` y los de cliente → servidor en `resultados/opcodes_cliente_servidor.txt`. El **catálogo completo**, con el nombre L1J, la certeza y la evidencia de cada opcode en los dos sentidos, está en **`opcodes/CATALOGO_2009.md`**. El cruce con los nombres de l1j-client está en `COMPARACION_L1J_CLIENT.md`.

Algunos formatos del juego que son únicos, y por tanto identifican el opcode con certeza alta:

| Opcode | Formato | Mensaje |
|---|---|---|
| 126 `0x7E` | `dhhc` | Mover objeto (id, x, y, dirección) |
| 59 `0x3B` | `dcdcccccchhhhcdcchcccc` | Estado del propio personaje |
| 96 `0x60` | `ccccc` | CA y resistencias elementales |
| 1 `0x01` | `hhdhccccdhsscdsscccc` | Objeto o personaje en el mapa |

Y del cliente al servidor: 50 `0x32` `cscccccccc` es crear personaje.

> **Importante:** estos números corresponden a la versión de 2009. Para recrear el comportamiento con `lin.bin` (2014) hacen falta los opcodes de esa versión concreta. La forma práctica de obtenerlos se describe en la sección 6.

## 6. Cómo completar el mapa de opcodes de `lin.bin`

1. **Captura**: jugar en el servidor real con Wireshark, filtrando la conexión TCP del cliente.
2. **Decodificación**: tomar la semilla del primer paquete del servidor y descifrar cada sentido con `lineage_proto.Cipher`, en orden. Hay que respetar el relleno a múltiplos de 4 de los paquetes del cliente.
3. **Correlación**: relacionar cada opcode con lo que se ve en pantalla (moverse, hablar, atacar, abrir el inventario…).
4. Como alternativa, volcar `lin.bin` ya desempaquetado en memoria desde Windows (como alguien hizo con `S3EP1U.bin`) y repetir este análisis. Las direcciones cambiarán, pero se pueden localizar los mismos patrones:
   - la constante `0x287EFFC3` en la actualización de clave;
   - la tabla de 128 punteros del despacho.

## Ejemplo de uso

```python
from lineage_proto import Cipher, Reader, frame

seed = 0x12345678                 # leído del paquete 0x7D
srv_to_cli = Cipher(seed)         # descifra lo que envía el servidor
cli_to_srv = Cipher(seed)         # descifra lo que envía el cliente

body = srv_to_cli.decrypt(encrypted_body)   # cuerpo sin la cabecera de 2 bytes
r = Reader(body)
opcode = r.c()
```

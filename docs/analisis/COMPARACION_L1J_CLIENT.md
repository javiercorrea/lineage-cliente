# Comparación con uglyoldbob/l1j-client

Repositorio analizado: https://github.com/uglyoldbob/l1j-client, commit `83052f6`. Partes relevantes:

- `src/packet.cpp`: entramado, cifrado y manejadores de paquetes.
- `src/globals.h`: enum con los opcodes "internos".
- `src/cot_opcodes.txt`: traducción de opcodes para otro servidor.
- `analyzer/`: analizador de tráfico en Java, con su propio `Opcodes.java` y `LineageBlowfish.java`.

Contraparte binaria: `S3EP1U.bin`, el cliente de 2009 desempaquetado. Los resultados en bruto están en `resultados/formatos_opcodes_juego.txt` y `resultados/opcodes_cliente_servidor.txt`.

## 1. Lo que coincide exactamente (mecánica del protocolo)

| Aspecto | l1j-client | Cliente 2009 (binario) |
|---|---|---|
| Cabecera | `uint16` LE con la longitud total, incluidos los 2 bytes de cabecera (`get_packet`, `send_packet`) | Igual (`0x49384f`) |
| Primer paquete | Del servidor, opcode `SERVER_KEY` + `d` semilla, sin cifrar | Igual (`0x7D` + `d`) |
| Derivación de clave | `rotr(seed ^ 0x9C30D539, 13)` y `k1 = k0 ^ 0x930FD7E2 ^ 0x7C72E993` | Igual (rotl 19 equivale a rotr 13). Confirmado por emulación |
| Cifrado y descifrado | `encrypt` / `decrypt` | Byte a byte idénticos a `0x4a7980` / `0x4a7900` |
| Cambio de clave | `key[0..3] ^= 4 bytes en claro`, `key[4..7] += 0x287EFFC3` | Igual |
| Lectura de campos | `packet_data`: byte, `uint16`/`uint32` LE, cadenas con `\0` | Igual, más `S` (UTF-16) |
| Tabla de páginas de códigos (`server_version_packet`) | `countryCode` 3→0x3B6, 4→0x3A4, 0→…, 5→0x3A8 | Mismas constantes en el manejador `0x483a00` |

**El "Blowfish" se explica.** `analyzer/.../LineageBlowfish.getSeeds()` llama a `blowfish_init(keys)` y luego aplica la fórmula simple, de modo que el resultado de Blowfish nunca se usa. En el binario ocurre lo mismo. Es un vestigio, o una ofuscación, del código original de NCSoft. Al reimplementarlo se puede omitir.

**Conclusión:** la mecánica (entramado, cifrado y tipos) es la misma en las tres fuentes. **Solo cambia la numeración de los opcodes**, y l1j-client ya contempla eso: carga un `opcodes.txt` por servidor que traduce los números (`convert_client_packets` / `convert_server_packets`).

## 2. Tres numeraciones de opcodes distintas

| Conjunto | Fuente | Opcode KEY |
|---|---|---|
| **A**: interno de l1j-client | `src/globals.h` | 65 |
| **B**: "COT" | `src/cot_opcodes.txt` y `analyzer/Opcodes.java`. Las dos fuentes concuerdan entre sí | 50 |
| **C**: cliente 2009 | `S3EP1U.bin` | 125 |

Para `lin.bin` (2014) habrá una cuarta numeración, D.

### Servidor → cliente

El formato de la columna C es el que realmente lee el binario. La certeza indica qué tan seguro es asignar ese opcode de C según la coincidencia de formato o de comportamiento.

| Mensaje (nombre l1j-client) | A | B | **C (2009)** | Formato real en C | Certeza |
|---|---|---|---|---|---|
| `SERVER_KEY` (inicio y semilla) | 65 | 50 | **125** | `d` | verificada (código) |
| `SERVER_VERSIONS` | 10 | 119 | **89** | `c` [`cdddddcc`] `c` | alta (formato y páginas de códigos idénticos) |
| `SERVER_NEWS` | 90 | 118 | **66** | `s` | verificada (se guarda en `Capture\News%04d.txt`) |
| `SERVER_NUM_CHARS` | 113 | 70 | **80** | `cc` (`c` en builds antiguas) | alta |
| `SERVER_LOGIN_CHAR` (lista de personajes) | 99 | 30 | **56** | `sscchhhccccccccc` | alta |
| `SERVER_MESSAGE` (mensaje por ID) | 87 | 127 | **74** (antes del juego) / **65** (en el juego) | `h` `c` `s`… | alta |
| `SERVER_ENTERGAME` / resultado de login | 63 | 19 | **41** | `c`; 22 = cuenta en uso, igual que en L1J | alta |
| `SERVER_MOVE_OBJECT` | 61 | 87 | **126** | `dhhc` | alta (formato único) |
| `SERVER_CHAR_STAT` | 69 | 104 | **59** | `dcdcccccchhhhcdcchcccc` | alta (formato único) |
| `SERVER_AC_ELEMENTAL_UPDATE` | 41 | 27 | **96** | `ccccc` | alta (formato único) |
| `SERVER_GROUNDITEM` (objeto o personaje en el mapa) | 64 | 62 | **1** (15 es una variante) | `hhdhccccdhsscdsscccc` | alta |
| `SERVER_CHAT_SHOUT` (NPC) | 42 | 114 | **50** | `cdshh` | media |
| `SERVER_CHAT_NORM` | 8 | 61 | **71** | `cdshh`; el tipo 2 usa las coordenadas | media |
| `SERVER_CHAT_GLOBAL` | 105 | 17 | **3** | `c` `s` | media |
| `SERVER_CHAT_WHISPER` | 91 | 49 | **47** | `ss` ("(%s) %s") | alta |
| `SERVER_MAP` | 76 | 31 | **73** | `hc` (guarda el mapa en 0x816764) | alta |
| `SERVER_TITLE` | 77 | 26 | 38 o 39 | `ds` | ambigua |
| `SERVER_CHANGE_HEADING` | 127 | 5 | (sin identificar) | `dc` | ambigua |
| `SERVER_REMOVE_OBJECT` | 9 | 4 | **0** | `d` | media |
| `SERVER_HPVALS` / `SERVER_MPVALS` | 101 / 96 | 53 / 23 | **34** / **92** | `hh` | verificada (mismas variables que el 59) |
| `SERVER_CHAR_ALIGNMENT` | 27 | 67 | **119** | `dh` | verificada |
| `SERVER_TIME` / `SERVER_WEATHER` / `SERVER_LIGHT` | 70 / 83 / 102 | 64 / — / 9 | **58** / **31** / **124** | `d` / `c` / `dc` | alta |
| `SERVER_DEX_UPDATE` / `SERVER_STR_UPDATE` | 112 / 104 | 12 / 35 | **51** / **106** | `hc` | alta |
| `SERVER_INV_ITEMS` / `SERVER_ITEM_BLESS_STATUS` | 47 / 107 | — / 20 | **8** / **107** | `[dcchcdcsc]` / `dc` | alta / media |

### Cliente → servidor

| Mensaje (l1j-client) | A | B | **C (2009)** | Formato en C | Certeza |
|---|---|---|---|---|---|
| `CLIENT_CREATE_CHAR` | 72 | 72 | **50** | `cscccccccc` | alta (formato único, coincide campo a campo) |
| `CLIENT_VERSION` | 71 | 85 | **34** | `chdch` (l1j-client envía `chdcd`) | verificada (se envía justo después de recibir la semilla, `0x45c0b0`) |
| `CLIENT_CLICK` | 43 | 39 | **46** | `c` (l1j-client envía `cdd`) | verificada (se envía al cerrar las noticias, `0x46cf60`) |
| `CLIENT_USE_CHAR` | 83 | 18 | **89** | `cs` (nombre del personaje) | alta (`0x470a80`) |
| `CLIENT_INITGAME` | 92 | — | **49** | `ccdd` | media (formato único) |
| `CLIENT_LOGIN` (usuario, contraseña) | 12 | 40 | — | — | no existe: el cliente KR envía un bloque de autenticación del lanzador (`0x45bf50`) |
| `CLIENT_CHAT_WHISPER` | 13 | 59 | **92** | `css` | media |
| `CLIENT_CHAT` / `CLIENT_CHAT_GLOBAL` | 104 / 40 | 35 / 71 | **18** / **115** | `ccs` | verificada (caja de chat: `&` y `$` van al 115, el resto al 18) |
| `CLIENT_ALIVE` | 57 | 4 | varios `cdd` | `cdd` | ambigua |
| `CLIENT_DELETE_CHAR` | 34 | 26 | 54 (candidato) | `csc` | baja |

Los valores A y B de las filas nuevas salen de `globals.h` y `cot_opcodes.txt`. El catálogo completo del cliente 2009, con los opcodes que l1j-client no usa, está en `opcodes/CATALOGO_2009.md`. Las ambigüedades que quedan se resuelven con una captura de tráfico (sección 6 de `PROTOCOLO_RED.md`).

## 3. Diferencias y errores encontrados en l1j-client

El binario es la referencia. Si el emulador sigue a l1j-client, conviene revisar estos puntos:

1. **`char_status`: el alineamiento es de 16 bits.** l1j-client lo lee como `int8_t`. El cliente real lee `... c(food) c(weight) h(alignment) cccc(resistencias)`. Con l1j-client, las cuatro resistencias quedan desplazadas un byte.
2. **`update_hp` nunca lee `max_hp`.** El código `data >> cur_hp, max_hp;` usa el operador coma de C++, así que `max_hp` queda sin inicializar. Debería ser `data >> cur_hp >> max_hp;`.
3. **`login_char_packet` lee un byte menos.** El real tiene 9 campos `c` después de `hhh`; l1j-client lee 8. No rompe nada porque es el último campo, pero se pierde el dato.
4. **`num_char_packet`.** En el cliente 2009 depende de la build: las nuevas leen `cc` (número y máximo) y las antiguas solo `c`. l1j-client siempre lee `cc` y avisa cuando el máximo es 0.
5. **Sin relleno a múltiplo de 4.** El cliente real redondea el cuerpo de cada paquete a un múltiplo de 4 bytes antes de cifrarlo. l1j-client no lo hace. Los servidores L1J lo toleran, pero un servidor estricto o un analizador podría notar la diferencia.
6. **`CLIENT_VERSION`.** l1j-client envía `h(0x33) d(-1) c(32) d(101101)` con valores fijos. El paquete equivalente del cliente 2009 parece ser `chdch`, así que el último campo sería `h` y no `d`. Hay que confirmarlo con una captura.
7. **Codificación de cadenas.** El cliente real también tiene cadenas UTF-16 (`S`), por ejemplo en los opcodes 16 y 127. `packet_data` solo maneja cadenas de 8 bits.

## 4. Recomendación para el emulador

- Mantener la mecánica tal cual: ya está bien implementada y coincide con el binario.
- Tratar los opcodes como datos: un `opcodes.txt` por versión, como ya hace l1j-client. Ya están generados en `opcodes/`: el del cliente 2009 y los de las familias públicas (KR 3.80, TW 3.80c, TW 3.50c/EN, JP 3.63c). Para `lin.bin` (2014), ver la sección de homologación de `opcodes/README.md`.
- Corregir los puntos 1 y 2 de la sección anterior.

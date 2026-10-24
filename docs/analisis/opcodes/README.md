# Archivos `opcodes.txt` para l1j-client

Son traducciones de opcodes en el formato que carga uglyoldbob/l1j-client (`sdl_user.cpp`, archivo `opcodes.txt` en los datos específicos del servidor, `FILE_REGULAR3`). En el commit `83052f6` no encontré dónde resuelve el cliente esa ruta: solo aparece el enum y la llamada.

Formato: 512 líneas, un número por línea, sin comentarios. `sscanf("%d")` no admite otra cosa.

- Líneas 1–256: `convert_client_packets[opcode interno] = opcode en el cable`.
- Líneas 257–512: `convert_server_packets[opcode en el cable] = opcode interno`.
- `255` significa sin traducción.

Para usar uno, se copia con el nombre `opcodes.txt`. Los archivos se generan con `scripts/generar_opcodes.py`.

| Archivo | Para qué cliente o servidor | Origen | Cobertura (cliente / servidor) |
|---|---|---|---|
| `opcodes_2009_S3EP1.txt` | Cliente coreano 2009 (`S3EP1.bin`) | Análisis propio de `S3EP1U.bin` (ver `CATALOGO_2009.md`) | 8 / 28 |
| `opcodes_KR_3.80.txt` | Emulador KR 3.80 (hillpath/L1J-KR_3.80, `data/Opcode.xml`) | Tabla pública | 11 / 33 |
| `opcodes_TW_3.80c.txt` | Emulador TW 3.80c (cliente `TW1308xxxx`) | Tabla pública | 10 / 33 (3.80c no tiene `COMMONCLICK`) |
| `opcodes_TW_3.50c_EN.txt` | TW 3.50c y l1j-en (las dos tablas son idénticas) | Tabla pública | 11 / 33 |
| `opcodes_JP_3.63c.txt` | Emulador JP 3.63c | Tabla pública | 11 / 33 |

## Contenido de `opcodes_2009_S3EP1.txt`

El catálogo completo de opcodes del cliente 2009 está en **`CATALOGO_2009.md`**. Recoge todos los opcodes servidor → cliente con manejador (18 antes del juego y 110 dentro) y los 120 cliente → servidor, con su formato, certeza y evidencia. Este archivo solo traduce los que tienen equivalente en l1j-client, con certeza media o mayor:

| Interno l1j-client | Cable 2009 | Certeza |
|---|---|---|
| `CLIENT_VERSION` / `CLICK` / `USE_CHAR` / `CREATE_CHAR` | 34 / 46 / 89 / 50 | verificada / verificada / alta / alta |
| `CLIENT_INITGAME` | 49 | media |
| `CLIENT_CHAT` / `CHAT_GLOBAL` / `CHAT_WHISPER` | 18 / 115 / 92 | verificada / verificada / media |
| `SERVER_KEY` / `VERSIONS` / `NEWS` | 125 / 89 / 66 | verificada / alta / verificada |
| `SERVER_NUM_CHARS` / `LOGIN_CHAR` / `ENTERGAME` | 80 / 56 / 41 | alta |
| `SERVER_MESSAGE` | 74 (antes del juego) y 65 (en el juego) | alta |
| `SERVER_CHAR_STAT` / `AC_ELEMENTAL_UPDATE` / `HPVALS` / `MPVALS` / `CHAR_ALIGNMENT` | 59 / 96 / 34 / 92 / 119 | verificada |
| `SERVER_TIME` / `DEX_UPDATE` / `STR_UPDATE` / `MAP` / `WEATHER` / `LIGHT` | 58 / 51 / 106 / 73 / 31 / 124 | alta |
| `SERVER_MOVE_OBJECT` / `GROUNDITEM` / `INV_ITEMS` / `CHAT_WHISPER` | 126 / 1 / 8 / 47 | alta |
| `SERVER_CHAT_NORM` / `CHAT_SHOUT` / `CHAT_GLOBAL` / `ITEM_BLESS_STATUS` / `REMOVE_OBJECT` | 71 / 50 / 3 / 107 / 0 | media |

**Sin traducción en 2009, y por qué:**
- **`CLIENT_LOGIN`.** El cliente coreano no tiene un paquete de login con usuario y contraseña. Envía sin cifrar un bloque de autenticación que prepara el lanzador o el portal del ISP (`0x45bf50`, con soporte para `kornet`, `dreamx`, `hitel`). Ese mismo código hace referencia al puerto 2000 (`0x7d0`). Con este cliente, el login depende del lanzador.
- **`CLIENT_ALIVE` y `CLIENT_DELETE_CHAR`.** No hay un envío periódico reconocible para el keepalive. Borrar personaje tiene un candidato (54, `csc`) con certeza baja.
- **`SERVER_CHANGE_HEADING`, `SERVER_CHANGE_SPMR`, `SERVER_DISCONNECT`, `SERVER_CHAR_DELETE`, `SERVER_CREATE_STAT`.** No se encontraron en el binario. `SERVER_TITLE` tiene dos candidatos (38 y 39).

## Cómo se emparejaron los nombres en las tablas públicas

`generar_opcodes.py` traduce cada nombre interno de l1j-client a un nombre L1J, por ejemplo `SERVER_KEY` → `S_OPCODE_INITPACKET`. El emparejamiento se validó con dos fuentes del propio l1j-client que asignan número a la misma versión: `cot_opcodes.txt` (interno ↔ cable) y `analyzer/Opcodes.java` (nombres L1J). Concuerdan en **38 de 39** nombres comparables (resultado en `resultados/generar_opcodes.txt`). La excepción es `SERVER_ITEM_BLESS_STATUS`: `cot_opcodes.txt` dice 20 (`PACKETBOX`), pero el formato `d`+`c` coincide con `S_ItemColor`, y se dejó este último.

## Limitación importante

Estos archivos solo traducen **números**. Entre versiones también cambia el contenido de algunos paquetes:
- El paquete de inicio de 3.80 lleva 11 bytes extra después de la semilla. l1j-client solo lee la semilla, así que no le afecta.
- El paquete de versión de 3.80 es más largo.
- `CLIENT_VERSION` cambia de formato: `chdch` en 2009 y `chdcd` en l1j-client.

Para jugar de verdad con otra versión habrá que ajustar también los manejadores.

---

# Homologación con el cliente 2014 (`lin.bin`)

## Qué se puede afirmar

1. **El protocolo base es el mismo.** Entramado, cifrado y semilla coinciden en el cliente 2009, en l1j-client y en todos los emuladores públicos revisados (TW 3.50c/3.80c, KR 3.80, JP 3.63c, EN), con la misma clase `LineageEncryption` y las constantes `0x930FD7E2` y `0x287EFFC3`. Es casi seguro que `lin.bin` también lo usa.
2. **La tabla de opcodes de `lin.bin` no se puede extraer estáticamente.** El binario está protegido (secciones con nombres aleatorios y entropía de ~7,8–8). `Login.dll`, el lanzador del servidor privado, parchea la memoria del cliente con `WriteProcessMemory` (lo mismo que hacen lanzadores públicos como r0ptik/L1J-3.8-launcher: "esperar a que el juego se descifre y parchear"), pero su contenido útil también está empaquetado.
3. **Ninguna tabla pública coincide con el cliente 2009.** Se compararon los 35 opcodes identificados del cliente 2009 que tienen nombre L1J con 9 tablas de emuladores y con la del analizador de l1j-client, y ninguna coincide en ninguno (`resultados/comparacion_tablas_publicas.txt`, generado con `scripts/comparar_tablas.py`). Por tanto, los clientes coreanos oficiales usan numeraciones propias, que los emuladores rehacen por versión.
4. **Indicios de época:**
   - El cliente 2009 solo tiene una tabla de despacho de 128 entradas (0–127).
   - Las tablas 3.63/3.80 usan opcodes hasta 255.
   - En el lanzador `Lineage.exe` aparece `lito.bin` como alternativa a `lin.bin`.
   - En la carpeta del juego también está `TW13032701.bin` (cliente taiwanés del 27-03-2013), de la época de TW 3.80c (que usa `TW13081901`). Además, el lanzador `Login.exe` (Loginer363) es de abril de 2013.

   Es posible que el servidor "Justice" funcione en realidad con un cliente de la familia 3.80. Hay que confirmarlo.

## Huella de cada familia: el primer paquete del servidor

El paquete de inicio viaja sin cifrar, así que basta con ver sus primeros bytes:

| Opcode de inicio | Cuerpo | Familia | Opcode de versión del servidor (confirma) |
|---|---|---|---|
| 125 (`7D`) | 5 bytes | Cliente KR 2009 (`S3EP1`) | 89 |
| 150 (`96`) | 16 bytes | **KR 3.80** o **TW 3.80c** | 77 (KR) / 139 (TW) |
| 161 (`A1`) | — | JP 3.63c | 151 |
| 27 (`1B`) | — | TW 3.50c / EN | 17 |
| 65 (`41`) | 5 bytes | l1j-client interno | 10 |
| 50 (`32`) | — | COT (l1j-client) | 119 |
| otro | — | **Familia nueva**: probablemente la de `lin.bin` 2014 | — |

## Procedimiento (una sola captura)

1. En Windows, o en una máquina virtual, capturar con Wireshark una sesión del cliente real contra el servidor: conectar, entrar al juego, moverse y hablar. Guardarla como `.pcap` o `.pcapng`.
2. Ejecutar `python3 analisis/scripts/identificar_version.py captura.pcapng --puerto <puerto>`. La herramienta:
   - reensambla el flujo TCP;
   - toma la semilla del primer paquete;
   - descifra ambos sentidos con `lineage_proto.py`;
   - identifica la familia y la confirma con el paquete de versión;
   - lista cada paquete descifrado con su nombre, si la familia es conocida.
3. **Si es una familia conocida**, se usa su `opcodes_*.txt`.
4. **Si es nueva**, se usa la lista descifrada para asignar los opcodes: el orden del arranque es fijo (inicio → versión → noticias → número de personajes → lista de personajes), y en el juego cada acción produce paquetes reconocibles. Después se añade la tabla a `generar_opcodes.py`.

`scripts/test_identificar.py` comprueba la herramienta con capturas sintéticas de las familias 2009 y KR 3.80, con retransmisiones y paquetes partidos en varios segmentos (`resultados/test_identificar.txt`).

**Atajo sin cliente Windows:** basta con abrir una conexión TCP al servidor y leer el primer paquete, porque el servidor habla primero y sin cifrar. Eso identifica la familia sin jugar. Requiere conectarse al servidor privado, así que lo dejo a tu decisión. La IP está cifrada en `Login.ini` (`ServerData`).

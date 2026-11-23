# Compatibilidad con L1J-Wanted (servidor coreano de la era Remastered)

**Estado:** análisis y plan; nada implementado. Fuente: el código de
[baboqoo/L1J-Wanted](https://github.com/baboqoo/L1J-Wanted) (Java + MySQL, opcodes del cliente
coreano del 2022-04-08). Solo se leyó el código: no se ejecutó el servidor ni sus herramientas.
Regla de siempre: si se prueba, solo contra una copia local del servidor.

## 1. Resumen

| Tema | Veredicto |
|---|---|
| Cifrado | Igual al nuestro (XOR L1J, constante `0x930FD7E2`). No hay que tocar `cifrado.cpp`. |
| Opcodes | Tabla nueva (`Opcodes.java`, 271 opcodes): un perfil más, `wanted`. |
| Formato de los cuerpos | **Mixto:** un núcleo binario clásico (como el nuestro) y un **segundo formato, protobuf**, dentro de un solo opcode. |
| Login | Distinto: login HTTP en un "connector" (el launcher), y después un token en el mensaje protobuf `A_NpLogin`. `C_LOGIN` no se usa. |
| Datos del cliente | Hace falta una instalación Remastered: con los paks de 2009 faltan 3340 de los gfx que define su `list.spz`. |

Lo importante: **el protobuf no es opcional ni se puede dejar para después**. Los objetos del
mundo (personajes, NPC, invocaciones), el inventario, el chat que envía el cliente y la versión del
cliente van en protobuf. Sin él no se pasa del login ni se ve nada en pantalla.

## 2. Qué es protobuf y en qué difiere de lo que tenemos

### 2.1 Lo que tenemos: campos por posición

Un paquete L1J clásico es una fila de campos **sin nombre**, en un orden fijo y con anchos fijos.
`S_HPUpdate` es `[opcode][h hp actual][h hp máx]`: el lector sabe que los bytes 1–2 son la vida
porque así está escrito en el decodificador, y nada en el paquete lo dice.

Consecuencias, que ya conocemos bien:
- Si una versión agrega, saca o ensancha un campo, todo lo que viene después se lee corrido
  (desalineado). Por eso existe `PerfilProtocolo` (`src/proto/opcodes.h`): `bytes_vida` 2 o 4,
  `bytes_golpe_ataque` 1 o 2, la cola de 11 bytes del ítem...
- No hay manera de saltar un campo desconocido: hay que saber su ancho.
- Un campo que no se usa igual se manda (relleno, ceros).

### 2.2 Protobuf: cada campo trae su etiqueta

[Protocol Buffers](https://protobuf.dev/programming-guides/encoding/) (de Google) codifica un
mensaje como una lista de pares **etiqueta → valor**. La etiqueta dice qué campo es y cómo leer su
valor:

```
etiqueta = (número_de_campo << 3) | tipo_de_cable
```

| Tipo de cable | Significado | Cómo se lee el valor |
|---|---|---|
| 0 | varint | entero de largo variable (ver abajo) |
| 2 | con largo | un varint con el largo y después esos bytes: texto, bytes o **un submensaje** |
| 1 / 5 | fijo de 64 / 32 bits | 8 / 4 bytes (en Wanted casi no aparecen) |

**Varint:** 7 bits por byte, el bit alto en 1 significa "sigue otro byte". Los números chicos
ocupan 1 byte y los grandes hasta 5 (o 10).
- `5` → `05`
- `300` → `ac 02` (300 = 0b10_0101100: primero `0101100|0x80` = `ac`, después `0b10` = `02`)

Ejemplo real, `S_BuffIconOnOffNoti` (código `0x021e`, icono 1234 encendido):

```
13            opcode S_EXTENDED_PROTOBUF
1e 02         código del mensaje 0x021e (u16 little-endian)
08 d2 09      campo 1 (icon_id), varint 1234
10 01         campo 2 (on), varint 1 = true
00 00         cola fija (ver 3.2)
```

`08` = campo 1, tipo 0 (`1<<3|0`). `10` = campo 2, tipo 0. Un texto, como el `desc` de
`WORLD_PUT_OBJECT`, es `4a <largo> <bytes>` (`4a` = campo 9, tipo 2). Cuando el número de campo
pasa de 15 la etiqueta ocupa dos bytes: `WorldPutObjectStream` escribe `writeH(0x0180)`, que en
el cable es `80 01`, el varint 128 = campo 16 (`isuser`), tipo 0.

### 2.3 Las diferencias que importan

| | Clásico (lo nuestro) | Protobuf |
|---|---|---|
| Identifica el campo | su posición | su etiqueta |
| Orden | obligatorio | libre (en la práctica se respeta, pero no hace falta) |
| Ancho de los enteros | fijo (c/h/d) | varint: según el valor |
| Campo ausente | no existe: siempre van todos | se omite y vale 0 / vacío |
| Campo desconocido | desalinea todo lo que sigue | se salta usando el tipo de cable |
| Anidado | no hay; listas a mano (`c cuenta` + filas) | submensajes (tipo 2) y campos repetidos |
| Cambios entre versiones | `PerfilProtocolo` + decodificador | campos nuevos u omitidos: el mismo decodificador sigue sirviendo |
| Texto | `writeS`: termina en `\0` | con largo delante, sin `\0` |

Para nuestro cliente esto es buena noticia: un decodificador protobuf **no necesita perfiles por
versión**. Lee las etiquetas que conoce, salta las que no y deja en 0 lo que falta. Encaja con la
regla de PLAN_MULTIVERSION.md (ningún `if` de versión en los decodificadores).

### 2.4 Cómo lo hace Wanted (y por qué no lo copiaremos)

Wanted no usa la librería de Google ni archivos `.proto`. Está **hecho a mano**:
- **Al escribir** (`ServerBasePacket`): `writeRaw(etiqueta)` + `writeBit(valor)` (varint) +
  `writeStringWithLength` / `writeBytesWithLength`. Es protobuf válido.
- **Al leer** (`ProtoHandler`, por ejemplo `A_NpLogin`): `readP(1)` **se salta la etiqueta
  suponiendo el orden** y lee el valor siguiente. O sea, vuelve a leer por posición. Si el cliente
  omite un campo, se desalinea.
- Los enteros negativos se escriben como el `int32` sin signo (5 bytes), no como los 10 bytes del
  estándar. Un lector que acumula en `u64` y castea a `i32` acepta las dos formas.
- El texto va en EUC-KR (compatible con CP949, la codificación de nuestro caso base), tanto en
  `writeS` como dentro de protobuf.

Nosotros haríamos un **lector protobuf de verdad, guiado por etiquetas** (varint, etiqueta, saltar
un campo, submensaje): unas 100–150 líneas, con pruebas armadas con bytes que reproducen lo que
escribe Wanted. No hace falta ninguna dependencia, y tampoco `.proto`, porque los mensajes se
describen en el decodificador como hoy (número de campo → campo de `Evento`).

## 3. El sobre: cómo viaja el protobuf dentro de L1J

Todo el protobuf va dentro de **un solo opcode por sentido**. El primer nivel sigue siendo el
paquete L1J cifrado de siempre:

### 3.1 Cliente → servidor: `C_EXTENDED_PROTOBUF` (0x9d)

```
9d  [código u16 LE]  [largo u16 LE]  [mensaje protobuf de `largo` bytes]
```

`ProtoPacketExecutor` lee el código de los bytes 1–2 y lo busca en la tabla `proto_packet` de la
BD (código → clase; 186 clases `A_*`). El largo está en los bytes 3–4.

### 3.2 Servidor → cliente: `S_EXTENDED_PROTOBUF` (0x13)

```
13  [código u16 LE]  [mensaje protobuf]  00 00
```

**Sin largo:** el mensaje va hasta el final del paquete. Los `S_*` cierran con `writeH(0)`: dos
ceros que el lector debe tolerar (con un lector por etiquetas, `00` sería un campo 0, que es
inválido, así que hay que cortar ahí). **A verificar** con capturas del servidor local si esos
ceros son siempre exactamente dos. Hay unos 313 códigos de servidor y 332 clases que mandan este
opcode.

### 3.3 Cómo encaja en nuestro decodificador

Hoy: `cuerpo → tabla.srv(opcode) → decodificador → Evento`. Con Wanted se agrega **un segundo
nivel de despacho** solo para `0x13`: `código u16 → decodificador protobuf → el mismo Evento`. La
clave del diseño es que **los mensajes protobuf producen los eventos que ya existen**:
`WORLD_PUT_OBJECT` produce el mismo evento que `S_CharPack` (con `OB_X`, `OB_GFX`, `OB_DIR`...) y
`S_AddInventoryNoti` el mismo que `S_AddItem`. Así la UI y el mundo no se enteran de que el
servidor habla otro formato.

Para enviar es lo mismo: `comandos.cpp` arma con un escritor protobuf los comandos que en Wanted
son protobuf (chat, versión, lista de personajes), detrás del mismo `Cli::` interno.

## 4. Qué va por cada formato en Wanted

Clasificado leyendo qué opcode escribe cada clase `S_*` y qué casos atiende `PacketHandler`.

| Fase | Binario clásico | Protobuf (código) |
|---|---|---|
| Saludo | `S_KEY` 0xe7 (paquete inicial) | `A_ClientVersion` 0x0334 |
| Login | `S_LOGIN_CHECK` (resultado) | `A_NpLogin` 0x03e5 (token del connector) |
| Selección | `S_NUM_CHARACTER`, `S_CHARACTER_INFO` (con 2 campos extra al final: `d 0`, `s código`), `C_DELETE_CHARACTER`, `C_CREATE_CUSTOM_CHARACTER` | `A_CharacterListRequest` 0x01cc, `S_TamPointNoti`, `S_ClientGateAppToken` |
| Entrar | `C_ENTER_WORLD` 0xa8, `S_STATUS`, `S_AC`, `S_MAGIC_STATUS`, `S_WEATHER`, `S_ENTER_WORLD_CHECK` | `S_StatusBaseNoti`, `S_AvailableSpellNoti`, `S_SpellBuffNoti` y ~40 más de UI |
| Mundo | `S_MOVE_OBJECT`, `S_REMOVE_OBJECT`, `S_CHANGE_DIRECTION`, `S_ACTION`, `S_ATTACK`, `S_HIT_POINT`, `S_MANA_POINT`, `S_PUT_OBJECT` (solo ítems en el suelo) | **`WORLD_PUT_OBJECT` 0x0077** (personajes, NPC, invocaciones, compañeros, trampas, efectos) |
| Inventario | `S_REMOVE_INVENTORY`, `S_CHANGE_ITEM_DESC_EX` | **`S_AddInventoryNoti`** (con submensaje `item_info`) |
| Chat | `S_SAY`, `S_MESSAGE`, `S_MESSAGE_CODE` (el servidor) | **`A_Chatting` 0x0202** (el cliente: no hay `C_CHAT` clásico) |
| Comandos | `C_MOVE` 0xaf, `C_ATTACK` 0x2b, `C_USE_ITEM`, `C_USE_SPELL`, `C_DIALOG`, `C_GET`, `C_DROP`... (95 casos) | sistemas nuevos: craft, alquimia, logros, asistencia, compañeros, arena... |

Lo que se ve: **los comandos del cliente siguen siendo casi todos clásicos** (mover, atacar, usar,
hablar con NPC), y del servidor también lo dinámico (moverse, golpear, vida). Lo que pasó a
protobuf son **los paquetes grandes con muchos campos opcionales**: un objeto que aparece, un ítem
y la lista de buffs. Justo donde el formato por posición dolía más.

## 5. Login por connector

El cliente oficial no manda usuario y clave por el socket del juego. El flujo es:

1. **HTTP al servidor web de Wanted** (el mismo proceso Java): el launcher manda `account`,
   `password`, `mac_address`, `hdd_id`, `board_id`, `nic_id` y `process` (`LoginDefine.java`). La
   ruta sale de la tabla `dispatcher` de la BD. Responde un `auth_token`. Otra consulta
   (`HttpConnectorInfoResult`) entrega la IP y el puerto del juego cifrados.
2. **Socket del juego:** saludo `S_KEY`, después `A_NpLogin` con los campos 1 IP (varint), 2 OTP,
   3 bytes "auth NP", 4 el token y 5 un hash de la MAC.
3. El servidor descifra el token (**AES/CBC/PKCS5 + Base64** con `CONNECTOR_ENCRYPT_KEY` de
   `config/connector.properties`), busca la sesión HTTP y autentica la cuenta.

Para nosotros hay dos caminos:
- **A (fiel):** un cliente HTTP mínimo (un POST de HTTP/1.1 sobre los sockets que ya tenemos, sin
  TLS porque el servidor es local) + AES-CBC para el token. Es más trabajo, pero sirve para
  cualquier servidor Wanted.
- **B (atajo de laboratorio):** en la copia local del servidor, aceptar el `C_LOGIN` clásico. Deja
  avanzar con el protobuf del mundo sin esperar el login, pero es un parche nuestro al servidor.

Recomendación: **B para arrancar la fase 4 y A en paralelo o después**. B no se publica: queda como
un parche en la configuración local del servidor, igual que `servidor_classic/`.

## 6. Datos del cliente y `list.spz`

- **gfx y mapas:** el `list.spz` en texto plano que trae Wanted (`tool/spritor/list.spz`, 21972
  entradas) se carga con nuestro `lista_sprites=`: 10268 gfx (3637 nuevos con acciones, 6631 solo
  con atributos, 4169 ya conocidos). A 3340 les falta el archivo en los paks de 2009. Para ver lo
  nuevo hace falta una instalación Remastered del cliente, que el usuario debe conseguir por su
  cuenta. **Ese `list.spz` es dato de NCsoft: no se commitea.**
- **Spritor** (`tool/spritor/Spritor.exe`, .NET) **no descifra** el `.spz`: solo lee el texto plano.
  El descifrado está en "PackViewer", que no está en el repo. Lo que sí aporta son los nombres de
  los atributos: 101 shadow, 102 type, 104 attr, 105 clothes, 107 size, 108 flying type,
  109 effect, 111 stride, 113 MorphEffect, 114 FurnitureSize, 115 expand breath action, 116 dir,
  117 lev, 118 switching, 120 box. Nosotros interpretamos hasta el 113.
- **Texto:** EUC-KR/CP949, igual que el caso base. No hace falta conversión (al revés que el Big5
  de tw380).

## 7. Plan por fases

Cada fase en su propia rama y PR.

| Fase | Qué | Cómo se sabe que está |
|---|---|---|
| 0. Entorno local | Servidor Wanted en local: Java + MySQL, importar `db/l1remaster.sql`, todo ligado a `127.0.0.1`. Como `servidor_classic/`: un script y un `LEEME`, sin datos de NC en el repo. | El servidor arranca y escucha en local. |
| 1. Lector y escritor protobuf | `src/proto/protobuf.{h,cpp}`: varint, etiqueta, saltar, submensaje, texto; escritor simétrico. Pruebas doctest con bytes de `S_BuffIconOnOffNoti`, `WORLD_PUT_OBJECT` y `S_AddInventoryNoti` reconstruidos de su código. | Las pruebas pasan, incluidos un campo desconocido, un negativo de 5 bytes y la cola `00 00`. |
| 2. Perfil `wanted` | Tabla de opcodes desde `Opcodes.java` (`docs/analisis/opcodes/`). Un `Srv::EXTENDED_PROTOBUF` con subdespacho por código hacia decodificadores que producen los `Evento` existentes. La cola de `S_CHARACTER_INFO` como dato de `PerfilProtocolo`. | Con un paquete grabado, el decodificador da los mismos eventos que su equivalente clásico. |
| 3. Saludo y login | `A_ClientVersion`, login (camino B primero), `A_CharacterListRequest`, selección de personaje. | Se ve la lista de personajes del servidor local. |
| 4. Entrar y ver el mundo | `WORLD_PUT_OBJECT`, `S_AddInventoryNoti`, `S_StatusBaseNoti`, chat (`A_Chatting` y los mensajes del servidor). Códigos desconocidos: se registran una vez y se ignoran. | Entrar, moverse, ver NPC y personajes, el inventario y chatear. |
| 5. Login fiel | Camino A: HTTP + AES del token. | Login sin parche en el servidor. |
| 6. Sistemas | Buffs, hechizos y después los sistemas propios de Remastered según interese. | Por sistema. |

Lo más incierto, para verificar en la fase 0 con capturas:
1. El servidor tiene varias tablas de opcodes (`Opcodes_211006`, `Opcodes_220121`...). Hay que
   confirmar cuál está activa y que coincida con el cliente cuyos datos usemos.
2. La cola `00 00` de los `S_EXTENDED_PROTOBUF`.
3. Qué espera el servidor en los campos de `A_ClientVersion`.
4. Los atributos 114–120 del `list.spz`, para la parte gráfica.

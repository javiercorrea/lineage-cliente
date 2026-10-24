# Catálogo de opcodes del cliente 2009 (`S3EP1U.bin`)

Todo sale del binario desempaquetado del cliente coreano de 2009 (`S3EP1.bin`, compilado el 19-01-2009). Los nombres son los de los servidores L1J (`S_OPCODE_*`, `C_OPCODE_*`), para poder cruzarlos con emuladores y con l1j-client.

## Cómo se identificó cada opcode

Se combinan cuatro fuentes de evidencia:

1. **Formato.** Lo que lee cada manejador (`resultados/formatos_opcodes_juego.txt`) o lo que envía cada llamada a `sendPacket` (`resultados/opcodes_cliente_servidor.txt`). Se compara con los formatos de dos servidores L1J públicos (`resultados/emparejamiento_formatos.txt`):
   - l1j-wk (EN/TW 3.50c, commit `63f37b7`);
   - L1J-JP 3.63c (commit `403e35a`).

   Solo es concluyente cuando el formato es largo o único.
2. **Variables del personaje.** El manejador del estado del personaje (59, `S_OwnCharStatus`) reparte cada campo en una dirección fija. Por ejemplo, el HP actual va a una variable protegida en `0x8ed488` y el HP máximo a `0x8f07e4`. Cualquier otro manejador que escriba esas mismas direcciones actualiza ese mismo dato. Así se verificaron HP, MP, experiencia, alineamiento, STR/DEX, hora, CA y resistencias.
3. **Cadenas.** Las cadenas que usa cada manejador o su entorno (`resultados/pistas_manejadores.txt` y `resultados/contexto_envios.txt`). Por ejemplo, `LinInv1` para el inventario, `deposit`/`withdraw` para el almacén y `music.def` para la música.
4. **Descompilación.** La de los manejadores pequeños, para ver qué hacen con cada campo.

**Certeza:**
- **verificada:** el código lo demuestra (misma variable, misma ventana o el mismo flujo).
- **alta:** el comportamiento y el formato coinciden sin alternativa razonable.
- **media:** encaja, pero hay otra explicación posible.
- **—:** sin identificar. Se describe lo que se sabe.

## Servidor → cliente

El cliente tiene dos tablas de despacho:
- **antes de entrar al juego:** 18 opcodes (índice en `0x493720`);
- **dentro del juego:** 128 entradas en `0x4934f0`, de las cuales 110 tienen manejador propio.

El formato se escribe sin el byte del opcode. Un formato entre corchetes `[...]` indica un bloque que se repite.

### Antes de entrar al juego

| Op | Nombre L1J | Formato | Certeza | Evidencia |
|---|---|---|---|---|
| 125 | `INITPACKET` | `d` | verificada | Primer paquete, sin cifrar; inicializa las claves (`0x48bee0`) |
| 89 | `SERVERVERSION` | `c[cdddddcc]c` | alta | Misma tabla de páginas de códigos que l1j-client |
| 66 | `COMMONNEWS` | `s` | verificada | Guarda el texto en `Capture\News%04d.txt` |
| 74 | `SERVERMSG` (antes del juego) | `h c s…` | alta | Busca el texto por número en la tabla de mensajes; trata aparte 0x111–0x113, 307, 317 y 437 |
| 80 | `CHARAMOUNT` | `cc` | alta | Número de personajes y máximo; reinicia 8 ranuras |
| 56 | `CHARLIST` | `sscchhhccccccccc` | alta | Una ranura de la pantalla de selección |
| 102 | `NEWCHARPACK` | igual que 56 | media | Usa el mismo manejador que 56 |
| 41 | `LOGINRESULT` | `c` | alta | Código de resultado; 22 = cuenta en uso (`REASON_ACCOUNT_IN_USE` de L1J). También existe dentro del juego |
| 88 | `LOGINTOGAME` | `cccccc` | alta | Guarda 6 bytes en variables protegidas, sobrescribe el búfer y entra al juego (`0x475150`). Equivale a `S_LoginGame` (8 bytes en L1J) |
| 4 | — | `c` | — | Si vale 0 abre una ventana (`0x46d460`) que responde con el opcode 12 del cliente. También existe dentro del juego |
| 62 | multipropósito | `c` + subtipo | — | Lista de bots, coliseo, `TotalUsers=%d`… Parecido a `S_PacketBox` |
| 5, 23, 29, 63, 79, 84, 116 | — | varios | — | Comparten un manejador grande (`0x46e0c0`) que distingue el opcode; el 63 lee `cdsch` |

### Dentro del juego: identificados

| Op | Nombre L1J | Formato | Certeza | Evidencia |
|---|---|---|---|---|
| 1 | `CHARPACK` | `hhdhccccdhsscdsscccc` | alta | Coincide con `S_OwnCharPack`/`S_NPCPack` campo a campo |
| 126 | `MOVEOBJECT` | `dhhc` | alta | Formato único |
| 59 | `OWNCHARSTATUS` | `dcdcccccchhhhcdcchcccc` | verificada | Igual que `S_OwnCharStatus`; referencia de las variables del personaje |
| 19 | `OWNCHARSTATUS2` | 7 bytes | verificada | Escribe STR, INT, WIS, DEX, CON, CHA y peso: las mismas variables que el 59 |
| 96 | `OWNCHARATTRDEF` | `ccccc` | verificada | CA y 4 resistencias, mismas variables que el 59 |
| 34 | `HPUPDATE` | `hh` | verificada | HP protegido (`0x8ed488`) y HP máximo (`0x8f07e4`) |
| 92 | `MPUPDATE` | `hh` | verificada | MP protegido (`0x8ed478`) y MP máximo (`0x8f07e2`) |
| 95 | `EXP` | `cd` | verificada | Nivel y experiencia (`0x8f07ec`) |
| 119 | `LAWFUL` | `dh` | verificada | Guarda el alineamiento en el objeto y, si es el jugador, en `0x8f07e0` |
| 58 | `GAMETIME` | `d` | alta | Hora del juego (`0x8f07d4`) |
| 51 | `DEXUP` | `hc` | alta | Duración + DEX (`0x8f07db`) |
| 106 | `STRUP` | `hc` | alta | Duración + STR (`0x8f07d8`) + peso |
| 73 | `MAPID` | `hc` | alta | Número de mapa (`0x816764`) + modo submarino |
| 31 | `WEATHER` | `c` | alta | Nibble alto 0 = nieve, 16 = lluvia; nibble bajo = intensidad 0–3 |
| 124 | `LIGHT` | `dc` | alta | Tamaño de luz del objeto (mínimo 5 para el jugador) |
| 104 | `HPMETER` | `dc` | alta | Porcentaje de HP del objeto (tope 100) |
| 13 | `DOACTIONGFX` | `dc` (`dcc` desde la build 50309) | alta | Lanza la animación de acción del objeto |
| 101 | `SKILLSOUNDGFX` | `dh` | alta | Crea el efecto gráfico (número de gráfico) sobre el objeto |
| 20 | `SKILLBRAVE` | `dch` | alta | Tipos 0–6 con duración e icono, como `S_SkillBrave` |
| 49 | `SKILLHASTE` | `dch` | alta | Tipos 1/2 con duración e icono, como `S_SkillHaste` |
| 28 | `ADDSKILL` | `c` + máscaras | alta | Por cada bit añade un hechizo a la lista (`unknown spell` si no está en la tabla) |
| 57 | `DELSKILL` | `c` + máscaras | alta | Igual, pero quita hechizos |
| 48 | `SKILLBUY` | `dh` + `[d]` | alta | Lista de hechizos con precio (nivel² × factor); si está vacía, voz del NPC |
| 109 | `SKILLBUY` (variante) | `h` + `[d]` | media | Igual sin precio; voz `HorunEv3` |
| 8 | `INVLIST` | `[dcchcdcsc]` | alta | Inicializa la ventana de inventario (`LinInv1`, `.inv`) |
| 33 | `ADDITEM` | `dcchcdcsc` | alta | Un solo objeto con el formato de `S_AddItem` |
| 75 | `DELETEINVENTORYITEM` | `d` | media | Quita un objeto del inventario por id |
| 9 | `ITEMNAME` | `ds` | media | Cambia el nombre de un objeto del inventario por id |
| 25 | `ITEMSTATUS` / `ITEMAMOUNT` | `dsdc` | media | Las dos clases L1J tienen el mismo formato |
| 107 | `ITEMCOLOR` | `dc` | media | Estado del objeto; el bit 0x80 muestra un mensaje |
| 12 | `SHOWSHOPSELLLIST` | `dh` + `[dd]` | alta | Lista de objetos y precios de compra del NPC, como `S_ShopBuyList` |
| 60 | `DEPOSIT` | `d` | alta | Ventana de almacén (`deposit`) |
| 67 | `DRAWAL` | `dd` | alta | Ventana de retirada (`withdraw`) |
| 37 | `SHOWHTML` | `dssh` | alta | Diálogos HTML del NPC (`colos`, `pledge`, `buddy`, `agdeny`…) |
| 21 | `TRADEADDITEM` | `chscc` | media | Formato único, igual que `S_TradeAddItem` |
| 10 | `POISON` | `dcc` | media | Activa o desactiva dos indicadores del objeto (verde/gris) |
| 22 | `INVIS` | `dc` | media | Marca el objeto como invisible y lo quita de los objetivos seleccionados |
| 0 | `REMOVE_OBJECT` | `d` | media | Busca el objeto por id y lo elimina |
| 65 | `SERVERMSG` | `hc` + `s`… | alta | Mensaje por número de la tabla del juego, con hasta N parámetros |
| 3 | `GLOBALCHAT` | `cs` | media | Tipo de chat (1–12) y texto |
| 47 | `WHISPERCHAT` | `ss` | alta | Lo muestra como `(%s) %s` (remitente y texto) |
| 71 | `NORMALCHAT` | `cdshh` | media | El tipo 2 (grito) calcula la dirección con las coordenadas, como el grito de `S_ChatPacket` |
| 50 | `NPCSHOUT` | `cdshh` | media | Mismo formato, con casos especiales según el gráfico del NPC |
| 120 | `TAXRATE` | `dccc` | media | Abre la ventana `tax` |
| 26 | `WARTIME` | `hs` / `cd` | media | Formatea fechas para `timetable` |
| 127 | `MAIL` | `sS…` | media | Correo (`NewMail : Mail Read Error`); usa cadenas UTF-16 |
| 100 | ping | `c` | alta | Muestra `Ping time[Server/NPC Server/Auth Server/Cache Server]`. No existe en L1J |
| 55 | aviso de acceso | `ss` | alta | `Account %s has just logged in from %s`. No existe en L1J |
| 43 | comida | `dc` | alta | Escribe la variable de comida del 59 |

### Dentro del juego: sin identificar

Tienen manejador, pero el formato y las pistas no bastan para ponerles nombre. Para cada uno se indica lo observado.

| Op | Formato | Lo observado |
|---|---|---|
| 2 | `dhc` | Efecto sobre un objeto según su gráfico (0x17ce/0x17c0) |
| 4 | `c` | Ver la tabla previa al juego |
| 6 | `dc` + `hh` | Si el segundo campo vale 0x47, lee coordenadas y crea un objeto auxiliar (posible pesca) |
| 7 | `d` | Añade el id a una lista de hasta 0x200 elementos |
| 11, 53, 85, 97 | — | Envían respuesta (opcodes 41/79, 41, 55 y 114 del cliente) |
| 14 | `dc` | Busca algo por id y guarda `c - 1` en una estructura global |
| 15 | `dc` + charpack | Variante de `CHARPACK` |
| 16 | `dhhssSScs` | Incluye cadenas UTF-16 |
| 17, 114 | `dh`, `hc` | Solo ponen un icono de efecto con duración |
| 18 | `dh` + `[dc]` | Lista de NPC parecida al 12; voces `touma3`/`gayle5` si está vacía |
| 24, 46, 115 | varios | Tienda personal (`usershop`) |
| 27 | `hhcc` / `cchhhcccccc` | Usa el nivel del personaje |
| 32, 36, 40, 54, 69, 81, 82, 83, 86, 90, 99, 103, 110, 111, 112, 113, 117, 118, 122, 123 | varios | Sin pistas suficientes |
| 35 | `cddccd` / `hchhhhcs` | Crea varios efectos gráficos |
| 38 | `ds` | Cadena en el campo 0x17 del objeto: candidato a `CHARTITLE` |
| 39 | `ds` | Cadena en el campo 0x1a, más un sufijo de la tabla de textos en el 0x18: candidato a nombre de clan |
| 42 | `dd` | Cambia un valor de un objeto del inventario |
| 44 | `dcdh` | Llama a la reproducción de música (`music.def`) |
| 45, 77 | `dh` / `ddh` | Diálogo `npcselect` |
| 52, 61 | `ch`, `dc` | Los lee y los descarta |
| 64 | `dc` | Llama a un método del objeto de inventario |
| 68, 121 | `dhhh` | `npcdeloy` y `tax` (castillos) |
| 70 | `dhhs` | Formato parecido a `S_Letter` |
| 72 | `dh` / `dshccd` | — |
| 87 | `dc` | Pone un indicador en el campo 0x14 del objeto, que cambia el color del nombre (posible `PINKNAME`) |
| 94 | `css` | Mismo formato que `S_War`; una subrutina usa música MIDI |
| 98 | `hc` | Cadenas `pearl` y `kaimo` |
| 105 | `h` / `s` / `ss` | Cadenas `RaiseAttr` y `monlist` |

**No encontrados.** No aparecen `CHANGEHEADING` (`dc`, muchos candidatos), `SPMR`, `DISCONNECT`, `DETELECHAROK` ni `NEWCHARWRONG`. Para asignarlos hace falta una captura.

## Cliente → servidor

Hay 368 llamadas a `sendPacket`, que envían 120 opcodes distintos (`resultados/opcodes_cliente_servidor.txt`). En `resultados/contexto_envios.txt` está, para cada uno, la función que lo envía y sus cadenas.

| Op | Nombre L1J | Formato | Certeza | Evidencia |
|---|---|---|---|---|
| 34 | `CLIENTVERSION` | `chdch` | verificada | Se envía al recibir la semilla (`0x45c0b0`) |
| 46 | `COMMONCLICK` | `c` | verificada | Se envía al cerrar las noticias (`0x46cf60`) |
| 67, 95, 108 | — | `c` | media | También los envía la ventana de noticias (`0x46ce90`); el 95 sale además de otras dos pantallas previas al juego |
| 89 | `LOGINTOSERVER` | `cs` | alta | Nombre del personaje elegido (`0x470a80`) |
| 50 | `NEWCHAR` | `cscccccccc` | alta | Formato único, igual que `C_CreateChar` |
| 54 | `DELETECHAR` | `csc` | media | Se envía desde la pantalla de personajes (`*BANNED*`) con un nombre |
| 49 | `LOGINTOSERVEROK` | `ccdd` | media | Formato único, igual que el que envía l1j-client |
| 18 | `CHAT` | `ccs` | verificada | Caja de chat (`0x420020`). El tipo sale del prefijo: 0 normal, 2 grito, 4 `@`, 11 `#`, 13 `%`, 14 `*`, 15 `~` |
| 115 | `CHATGLOBAL` | `ccs` | verificada | Misma caja de chat, solo para `&` (tipo 3) y `$` (tipo 12), como `C_ChatGlobal` |
| 92 | `CHATWHISPER` | `css` | media | Destinatario y texto, enviado por la función contigua a la del chat (`0x41ffa0`) |
| 13 | comandos de GM | `cc…` | verificada | 32 envíos desde los comandos `/` (`/wishmon`, `/suspend`, `setstat`, `notice`…), con un subcódigo |
| 3, 8, 10, 11, 22, 30 | comandos de GM | varios | alta | Los envía el intérprete de comandos (`teleport`, `banuser`, `kick`, `listuser`…) |
| 15 | `MOVECHAR` | `chhc` | alta | Mismo formato que `C_MoveChar`; la otra variante es `cdc` |
| 119 | `USESKILL` | `cd…` | alta | 36 envíos desde las funciones de hechizos (`%s is out of the range`, `No monsters nearby`) |
| 129 | — | `ccc…` | — | Los envían las mismas funciones de hechizos que el 119 |
| 65 | `NPCACTION` | `cdss` | media | 17 envíos desde diálogos de NPC (`select`) |
| 63 | — | `chcs` | — | Diálogos de NPC (`exitghost`, `deposit`, `RaiseAttr`…) |
| 52 | `PICKUPITEM` / `DROPITEM` | `chhdd` | media | Se envía desde el inventario; las dos clases L1J leen `hhdd` |
| 85 | `GIVEITEM` | `cdhhdd` | media | Formato único, igual que `C_GiveItem` |
| 124 | `AMOUNT` | `cddcs` | media | Formato único, igual que `C_Amount` |
| 43, 44 | informes de GameGuard | `ccb`, `cdcccc` | verificada | Se envían desde el control de GameGuard (`Speedhack detected`…) |
| 127 | correo | `cchsSS` / `ccd` / `cc` | media | Envío y lectura de correo con cadenas UTF-16 |
| 12 | — | `cc` | — | Respuesta a la ventana que abre el opcode 4 del servidor |

No se identificó `KEEPALIVE`: ningún envío periódico tiene un formato propio claro. Tampoco hay `LOGINPACKET`, porque este cliente se autentica con el bloque que prepara el lanzador (ver `README.md`).

## Uso en l1j-client

`opcodes_2009_S3EP1.txt` incluye las entradas de este catálogo que tienen equivalente en l1j-client, con certeza media o mayor:
- **cliente (8):** versión, clic, elegir personaje, crear personaje, iniciar juego, chat, chat global y susurro;
- **servidor (28):** los mensajes de las tablas anteriores que l1j-client sabe tratar.

El mensaje del servidor se traduce desde los dos opcodes, 74 y 65.

Algunas entradas de certeza media pueden estar mal: los chats 50/71/3, el objeto 0, el estado de objeto 107 y el susurro 92. Si l1j-client muestra algo raro con alguno de ellos, basta con quitarlo de `TABLA_2009_*` en `scripts/generar_opcodes.py` y regenerar el archivo.

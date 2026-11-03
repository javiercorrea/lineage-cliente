# Opcodes del servidor que el cliente no implementa

Dos servidores de prueba, dos tablas de `src/proto/opcodes.cpp`:
- **`jp363`** (L1J-JP 3.63c, `servidor_local/l1j-jp`): el grueso del documento.
- **`tw380`** (3.80C de Taiwán, L1JGO-Whale, `servidor_380/l1jgo`): sección propia al final.

Revisión de `servidor_local/l1j-jp` (L1J-JP 3.63c: `codes/Opcodes.java`, `packets/PacketHandler.java`)
contra la tabla `jp363` de `src/proto/opcodes.cpp`. Solo cuenta lo que el servidor usa de verdad: un
`S_OPCODE_*` que aparece en alguna clase que lo envía, o un `C_OPCODE_*` con un `case` en
`PacketHandler`. El número entre paréntesis es el del cable.

Para repetirla: el script está descrito al final. Al implementar uno, tacharlo aquí (o moverlo a
"Hecho") en el mismo commit.

## Resumen

Última revisión: 2026-10-31.

- **Del servidor al cliente:** el servidor envía 107 opcodes; faltan 3, además de subtipos de
  `S_PacketBox` (40) y de `S_SkillIconGFX`.
- **Del cliente al servidor:** el servidor atiende 91; el cliente envía 76. De los 15 restantes,
  `C_ArrowAttack` (247) y `C_ChatGlobal` (62) no faltan: van al mismo manejador que `C_Attack` y
  `C_Chat`, que el cliente ya usa. Faltan 13.

## Hecho

- Puertas: `S_Door` (35), `C_Door` (199).
- Estados alterados: `S_Paralysis` (165), `S_Poison` (93), `S_CurseBlind` (238).
- Dar ítems: `C_GiveItem` (244). Cantidad: `S_InputAmount` (253), `C_Amount` (109).
- Mascotas: `S_PetCtrlMenu` (33), `C_PetMenu` (217), `C_UsePetItem` (213), `S_SelectTarget` (177),
  `C_SelectTarget` (155), `S_TrueTarget` (110).

- Reparar armas: `S_SelectList` (208), `C_FixWeaponList` (106), `C_SelectList` (238).
- Marcadores: `S_Bookmarks` (11), `C_Bookmark` (134), `C_BookmarkDelete` (223).
- Volver a la selección de personaje: `C_ChangeChar` (237).
- Basurero del inventario: `C_DeleteInventoryItem` (209).
- Identificar: `S_IdentifyDesc` (43), y `C_UseItem` con el ítem objetivo (también encantar). Los
  atributos del ítem identificado no son un paquete aparte: van en `S_AddItem`, `S_InvList` y
  `S_ItemStatus`, y antes se descartaban.
- Invisibilidad, resucitar y modo espectador: `S_Invis` (57), `S_Resurrection` (227),
  `C_ExitGhost` (210). Volver al login: `C_ReturnToLogin` (218).
- Portales del mapa del cliente: `C_EnterPortal` (249), con la lista del final del `.s32`.
- Íconos de Enchant Mighty/Dexterity y Dress Mighty/Dexterity: `S_Strup` (120), `S_Dexup` (28).
- Nombre rosa del PK: `S_PinkName` (252).
- Borracho y tercer acelerador: `S_Liquor` (31). Bless of Eva: `S_SkillIconBlessOfEva` (12).
  Anillos de control: `S_Ability` (116).
- Reiniciar atributos (Candle of Reminiscence): `S_CharReset` (33, el mismo número que
  `S_PetCtrlMenu`: subtipos 1–3), `C_CharReset` (236).
- Grupo: `S_PacketBox` 104, 105, 106 y 110 (el panel del grupo). Bloqueados: `C_Exclude` (101) y
  `S_PacketBox` 17–19. Amigos: `C_Buddy` (60), `C_AddBuddy` (99), `C_DeleteBuddy` (211). Grupo de
  chat: `C_ChatParty` (113). Sonidos del servidor: `S_Sound` (84).
- Tienda de jugadores: `S_PrivateShop` (190), `C_Shop` (16), `C_ShopList` (193; en la lista de
  opcodes se llama `C_PrivateShopList`); comprar y vender a la tienda van por `C_Result`.
- Tablón de los pueblos: `S_Board` (64), `S_BoardRead` (56), `C_Board` (73), `C_BoardRead` (59),
  `C_BoardNext` (221, `C_BoardBack`), `C_BoardWrite` (14), `C_BoardDelete` (12). Correo: `S_Mail`
  (1), `C_Mail` (22).
- Casas y castillos: `S_HouseList` (24), `S_HouseMap` (44), `S_War` (123), `S_CastleMaster` (66),
  `S_TaxRate` (72), `S_Deposit` (203), `S_Drawal` (224), `S_WarTime` (150, el número de `S_MapID`),
  `S_PacketBox` 0–4 (asedio); `C_War` (235), `C_TaxRate` (200), `C_Deposit` (35), `C_Drawal` (192),
  `C_ChangeWarTime` (4), y `C_Attr` con texto (512, el nombre de la casa).

Detalle y verificación en ENTREGABLES.md: "Puertas, estados alterados, cantidad y mascotas",
"Reparar armas, marcadores y cambiar de personaje", "Basurero, detalle de los ítems e identificar" e
"Invisibilidad, resucitar, modo espectador y volver al login", "Portales", "Buffs: íconos y
encantar arma o armadura" y "Casas y castillos".

## 1. Más impacto en el juego

| Función | Paquetes |
|---|---|

## 2. Avisos y efectos visuales

- `S_ChangeName` (81), `S_UseMap` (100): cambio de nombre, mapas del tesoro.
- `S_SkillIconGFX`: el cliente dibuja 34 (poción azul) y 40 (Immune to Harm); faltan 15, 35, 36, 56 y 75.
- `S_PacketBox` (40): el cliente usa peso (10), comida (11) y el aura (0x16). Faltan, de los que el
  servidor envía: duelo (5), nivel máximo (12),
  esquiva (88, 101), tiempo en mapas con límite (159), Bless of Ain (82),
  cierre de sesión (42), llamado de un GM (45), cambio de rango (27), líder del pueblo (23),
  emblema del clan (`PLEDGE_EMBLEM_STATUS`), registro del almacén del clan (117).

## 3. Sistemas completos

- **Duelo, matrimonio, PK:** `C_Fight` (47), `C_Propose` (185), `C_CheckPK` (137).
- **Mercenarios del castillo:** `S_HireSoldier` (126), `C_HireSoldier` (97). El servidor no los
  implementa ("employ" y "arrange" del chambelán no hacen nada).
- **Clan recomendado:** `S_PledgeRecommendation` (192), `C_PledgeRecommendation` (228).
- **Varios:** `C_FishClick` (26, pesca), `C_Ship` (117), `C_Rank` (88), `C_CallPlayer` (144, GM).
- **Opciones del original al servidor:** `C_LoginToServerOK` (75: chat global y susurros
  abiertos o cerrados), `C_CharacterConfig` (129: disposición de la interfaz), `C_SendLocation` (41),
  `C_ExtraCommand` (157).

## Servidor 3.80C de Taiwán (tw380)

Revisión de `servidor_380/l1jgo` (L1JGO-Whale, commit 9a19405: `internal/net/packet/opcodes.go` y los
handlers de `internal/handler`) contra la tabla `tw380` de `src/proto/opcodes.cpp`. La mayoría de los
paquetes son los mismos que en 3.63 con otro número de cable; acá va solo lo distinto.

Última revisión: 2026-11-02.

**Funciona** (verificado en vivo; ENTREGABLES.md "Servidor 3.80C"): login, lista y creación de
personaje, entrar, `S_CharPack`, movimiento, chat, inventario, páginas de NPC y ataques. La vida y el
maná van en 4 bytes (no 2); `S_Door` es el 209 (`S_ChangeAttr`) y `S_WarTime` el 231.

**El servidor envía y el cliente no decodifica** (quedan como "?" en la traza):

- `S_CharSynAck` (64): multiuso — casilleros de equipo, emblema, reinicio de personaje y
  sincronización de la info del personaje. Lo mandan 7 handlers: es el de más impacto.
- `S_RedMessage` (105): texto rojo al centro de la pantalla.
- `S_ChangeName` (46), `S_ClanName` (72), `S_ClanAttention` (200): cambio de nombre y avisos de
  estado del clan.
- `S_ClanMatching` (0): emparejamiento de clanes. El número 0 choca con "paquete vacío", así que
  habría que tratarlo aparte.

**Sin opcode en 3.80** (la función existe en el cliente pero no se puede usar contra este servidor;
quedan en 0 en la tabla): noticias (`S_CommonNews`), `S_Teleport`, mensaje azul, borrar hechizos
(`S_DelSkill`), menú de mascotas (`S_PetCtrlMenu`); del cliente: pedir emblema (`C_Clan`), menú e
ítem de mascota, elegir objetivo (`C_SelectTarget`), lista de reparar (`C_SelectList`), volver al
login (`C_ReturnToLogin`), página siguiente del tablón (`C_BoardBack`) y elegir fecha del asedio
(`C_ChangeWarTime`). El servidor usa otros mecanismos (las mascotas van por un subtipo de
`S_HpMeter`).

**Sin probar en 3.80:** tiendas, almacén, grupo, intercambio, clanes, casas y castillos. Las pruebas
en vivo (`herramientas/pruebas`) usan el servidor 3.63.

**Limitación de la instalación:** el 3.80 manda los textos en chino (Big5) y Lineage Justice solo
trae la fuente coreana, así que los nombres de ítems y los mensajes del sistema se ven ilegibles.
Cómo se detectó: la tabla sale de `opcodes.go` del servidor por nombre, y los opcodes sin decodificar
aparecen como "?" con el comando `traza on` del control en vivo (`paquetes` los lista).

## Cómo se hizo

1. Números: `public static final int (S|C)_OPCODE_X = n` de `Opcodes.java` (algunos nombres
   comparten número: `CHARRESET` y `PETCTRL` son 33).
2. Enviados por el servidor: los `S_OPCODE_X` que aparecen en algún `.java` fuera de `Opcodes.java`.
3. Atendidos por el servidor: los `case C_OPCODE_X:` de `PacketHandler.java`.
4. Cliente: los números de `{n, Srv::X}` y `{Cli::X, n}` en `src/proto/opcodes.cpp`.
5. Subtipos: `new S_PacketBox(S_PacketBox.X` y `new S_SkillIconGFX(n` en el servidor, contra la
   función `caja` de `src/proto/decodificar.cpp`.

# Opcodes del servidor que el cliente no implementa

Revisión de `servidor_local/l1j-jp` (L1J-JP 3.63c: `codes/Opcodes.java`, `packets/PacketHandler.java`)
contra la tabla `jp363` de `src/proto/opcodes.cpp`. Solo cuenta lo que el servidor usa de verdad: un
`S_OPCODE_*` que aparece en alguna clase que lo envía, o un `C_OPCODE_*` con un `case` en
`PacketHandler`. El número entre paréntesis es el del cable.

Para repetirla: el script está descrito al final. Al implementar uno, tacharlo aquí (o moverlo a
"Hecho") en el mismo commit.

## Resumen

Última revisión: 2026-10-28.

- **Del servidor al cliente:** el servidor envía 107 opcodes; faltan 22 (21 números, más
  `S_CharReset`, que comparte el 33 con `S_PetCtrlMenu`), además de subtipos de `S_PacketBox` (40)
  y de `S_SkillIconGFX`.
- **Del cliente al servidor:** el servidor atiende 91; el cliente envía 56. De los 35 restantes,
  `C_ArrowAttack` (247) y `C_ChatGlobal` (62) no faltan: van al mismo manejador que `C_Attack` y
  `C_Chat`, que el cliente ya usa. Faltan 33.

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

Detalle y verificación en ENTREGABLES.md: "Puertas, estados alterados, cantidad y mascotas",
"Reparar armas, marcadores y cambiar de personaje", "Basurero, detalle de los ítems e identificar" e
"Invisibilidad, resucitar, modo espectador y volver al login".

## 1. Más impacto en el juego

| Función | Paquetes |
|---|---|
| Entrar a portales como el original (el cliente manda la celda; hace falta la lista de portales del `.s32`) | `C_EnterPortal` (249) |

## 2. Avisos y efectos visuales

- `S_PinkName` (252): nombre rosa del PK.
- `S_Liquor` (31): efecto de borracho.
- `S_Sound` (84): sonidos que dispara el servidor.
- `S_Strup` (120), `S_Dexup` (28): pociones de STR/DEX.
- `S_BlessOfEva` (12): respirar bajo el agua. `S_Ability` (116): anillos.
- `S_ChangeName` (81), `S_UseMap` (100): cambio de nombre, mapas del tesoro.
- `S_SkillIconGFX`: el cliente dibuja 34 (poción azul) y 40 (Immune to Harm); faltan 15, 35, 36, 56 y 75.
- `S_PacketBox` (40): el cliente usa peso (10), comida (11) y el aura (0x16). Faltan, de los que el
  servidor envía: actualizaciones del grupo (104–110), duelo (5), guerra (0–2), nivel máximo (12),
  esquiva (88, 101), tiempo en mapas con límite (159), Bless of Ain (82), bloqueados (18, 19),
  cierre de sesión (42), llamado de un GM (45), cambio de rango (27), líder del pueblo (23),
  emblema del clan (`PLEDGE_EMBLEM_STATUS`), registro del almacén del clan (117).

## 3. Sistemas completos

- **Tiendas de jugadores:** `S_PrivateShop` (190), `C_Shop` (16), `C_PrivateShopList` (193).
- **Tablón de los pueblos:** `S_Board` (64), `S_BoardRead` (56), `C_Board` (73), `C_BoardRead` (59),
  `C_BoardNext` (221), `C_BoardWrite` (14), `C_BoardDelete` (12).
- **Correo:** `S_Mail` (1), `C_Mail` (22).
- **Amigos y bloqueados:** `C_AddBuddy` (99), `C_DeleteBuddy` (211), `C_BuddyList` (60), `C_Exclude` (101).
- **Duelo, matrimonio, PK:** `C_Fight` (47), `C_Propose` (185), `C_CheckPK` (137).
- **Castillos, guerras y casas:** `S_War` (123), `S_CastleMaster` (66), `S_TaxRate` (72),
  `S_Deposit` (203), `S_Drawal` (224), `S_HouseList` (24), `S_HouseMap` (44); `C_War` (235),
  `C_TaxRate` (200), `C_Deposit` (35), `C_Drawal` (192), `C_HireSoldier` (97), `C_ChangeWarTime` (4).
- **Clan recomendado:** `S_PledgeRecommendation` (192), `C_PledgeRecommendation` (228).
- **Reiniciar atributos (Candle of Reminiscence):** `S_CharReset` (33), `C_CharReset` (236).
- **Varios:** `C_FishClick` (26, pesca), `C_Ship` (117), `C_Rank` (88), `C_ChatParty` (113),
  `C_CallPlayer` (144, GM).
- **Opciones del original al servidor:** `C_LoginToServerOK` (75: chat global y susurros
  abiertos o cerrados), `C_CharacterConfig` (129: disposición de la interfaz), `C_SendLocation` (41),
  `C_ExtraCommand` (157).

## Cómo se hizo

1. Números: `public static final int (S|C)_OPCODE_X = n` de `Opcodes.java` (algunos nombres
   comparten número: `CHARRESET` y `PETCTRL` son 33).
2. Enviados por el servidor: los `S_OPCODE_X` que aparecen en algún `.java` fuera de `Opcodes.java`.
3. Atendidos por el servidor: los `case C_OPCODE_X:` de `PacketHandler.java`.
4. Cliente: los números de `{n, Srv::X}` y `{Cli::X, n}` en `src/proto/opcodes.cpp`.
5. Subtipos: `new S_PacketBox(S_PacketBox.X` y `new S_SkillIconGFX(n` en el servidor, contra la
   función `caja` de `src/proto/decodificar.cpp`.

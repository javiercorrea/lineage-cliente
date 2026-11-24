# Sprites and rendering in S3EP1U.bin (reverse-engineered)

Findings from the 2009 US client, used to implement sprite attributes and blend modes in the fork.
Tags: (C) confirmed from decompile/disassembly, (I) inferred, (U) unknown.

## Frame renderer and blend flags

- `FUN_00465c30` (0x465c30) is the frame renderer. Per map cell it draws the ground tile, then the per-cell
  list at cell+0x48c (cells at `DAT_0081675c + cell*0x494`). Entries are 16 bytes: +0 layer (sort key, ascending,
  inserted by `FUN_00460230`), +4 sprite, +8 flags, +12 byte param. (C)
- Some flags are gated by user options: bits 0x1000, 0x2000, 0x4000, 0x8000, 0x10000, 0x20000 tested against
  `FUN_0044bf40(0x3b6/0x3e9/0x527/0x8b8/0xcdc|0xcde)`. (C)

Dispatch order of the flags word (0x466200-0x4663b0) (C):

```
if flags & 0x40:   50% average, only when local_90 is set (else not drawn)  (gate: [0x8ed57c] in 1..3,
                   [0x6f5d1c]==0, FUN_0044c150(&DAT_006de438); meaning U)
elif flags & 0xc00: alpha fade, level = byte param 0..31 (DAT_008ebf64)
elif flags & 0x02:  not drawn
elif flags & 0x08:  screen       (DAT_008ebf58)
elif flags & 0x101: 50% average  (DAT_0082470c)
elif flags & 0x80:  multiply     (DAT_00824710)
elif flags & 0x10:  additive, clamped (DAT_00824708)
elif flags & 0x200: tint with a global colour DAT_005005c4 (math U)
elif flags & 0x20:  pulsing brightness, param |0xc - tick%0x18| + 0x10 (math U)
else:               opaque (FUN_0049cfc0)
```

Pixel math (C): `FUN_004a03b0` builds per-channel 32x32 tables (5-bit channels, 565 or 555); the generic
blitter `FUN_0049d420` does `out = T[(dst<<5)+src]` per channel. With a = destination, b = source (0..31):

| Flag | Math |
|---|---|
| 0x8 | screen: `max + min*(31-max)/31` (truncated) |
| 0x10 | additive: `min(a+b, 31)` |
| 0x1, 0x100 | average: `(a+b)>>1` |
| 0x80 | multiply: `a*b/31` (white leaves the ground unchanged: the disc under street torches, gfx 1523) |

Map loader tile objects (`FUN_00461ff0`, `FUN_00460580`) (C): object attribute bit 0x10 -> flag 0x8 (screen),
0x04 -> 0x100 (average), 0x20 -> 0x80 (multiply), in that priority.

Body flags (C), `FUN_004cc980`: `obj+0x1c = attr104[gfx]`, then ORs from the object packet status byte:
`&1` -> 0x20 (pulse), `&8` -> 0x200 (tint), `&0x80` -> 0x2 (hidden), else `&2` -> 0x100 for the local player,
0x40 for others. The shadow (attr 101) part is drawn with `(flags & 0xffff0fff) | 5`. Clothes (attr 105) use
their own attr 104 `|0x40` or `|1` from the object flags, and are skipped when the body has 0x2.

## Sprite list (list.spz) loader `FUN_00440100`

- Number reader `FUN_0043ffe0` skips every non-digit; `-` right before digits makes it negative. (C)
- Header `#id N[=M] name`: N image count (`DAT_00701328`), `=M` file group (`DAT_007012fc`, default 0xffff). (C/I)
- Action `N=G` copies action N's frames from gfx G (images also from G). `N.name(dirFlag count, frames...)`
  defines a body; frame `group.idx:dur`, duration `dur*1000/framerate` (attr 110, default 24), scaled by
  `DAT_00701350` percent for actions 0, 4, 11, 20, 24, 40, 46, 50, 54, 58, 62. Image index = group + heading
  when dirFlag != 0. (C)
- Fallback: if actions 0x2e..0x31 are missing and 4..7 exist, copy 4..7 into them; same for 0x32..0x35. (C)

| Id | Meaning | Parse | Behaviour |
|---|---|---|---|
| 100 | see-through trigger (see Round 2) | n, n x (x1,y1,x2,y2) | `FUN_004cba00` after object creation: for x in x1..x2 step 2 (half tiles), y in y1..y2, relative to the object's tile, mark the tile blocked (`FUN_00460360(2)`); if the player is inside, reposition (`FUN_00468240`). (C) |
| 101 | shadow gfx | 1 | same image index as the body, -1 none. (C) |
| 102 | type | 1 | 0 effect, 1 scenery, 4 sign, 5 player, 6 NPC, 7 chest, 8 door, 9 ground item, 10 monster, 11 gate, 12 shop NPC, 14 board, 15 auction board. (C, values from data) |
| 103 | interaction radius | 1 | `FUN_004c7460` mode 2: reached when `|dx| <= 2v, |dy| <= v`. (I) |
| 104 | draw flags | 1 | see above. (C) |
| 105 | clothes | n, n gfx | overlays at the body's image index. (C) |
| 106 | weapon overlays | 5 shorts (global list) | slot = table `DAT_006e51bc[baseAction]` (4->1, 11->2, 20->3, 24->4, 40->5, 46->1); overlay gfx short[slot-1] at the same image index with its own attr 104. (C) |
| 107 | size | 2 numbers, stored `a | b<<4` | click/arrival box half extents (x doubled). (C) |
| 108 | flying type (see Round 2) | 1 | 2 = weapon-stance humanoid: action 0x2c end -> stance 2, base action 4 (11 if stride); 0x2d end -> stance 1, base 0. (C) |
| 109 | effect class, next gfx | 2 | object subclass at spawn (`FUN_00487780`): 1 default, 2, 3, 4/5/7 get the next gfx, 8 none. (C; class meanings I) |
| 110 | framerate | 1 | ticks per second for later actions on the line. (C) |
| 111 | stride | 1 | walk action from `{0,4 | 11,20 | ...}` by stance and a per-object step counter (`FUN_004c6ff0`). (C) |
| 112 | alternative gfx | 1 | when the object has no frames for its action and heading is 6, use this gfx's images. (C) |
| 113 | morph effect | 1 | after an object changes to this gfx (`FUN_00488f60`), spawn that effect gfx at its position. (C) |
| 114 | footprint | 1 | 2, 3, 8 allocate 1, 3, 5 occupancy tiles. (I) |

Frame suffixes (C): `<N` sound (overlap allowed), `[N` sound (only if not already playing), `^N` sound scaled by
distance; up to 3 ids per run, one picked at random when the frame shows. `]N` spawns effect gfx N at the frame.
`!` impact frame (applies the pending hit). `>` flag bit 2 (reader U). `{` `}` do not exist in 2009 (newer lists).

## Equipment window images (audit U-C2)

`[0x81b328]` is the sex byte of the selected character (char-list entry +0x1f; +0x1e is the class). (C)
There is no `1516.img` / `1518.img` in this installation (only unrelated monster thumbnails `1516.png`,
`1518.png`); the 1440 / 983 fallbacks are correct. (C)

## Bookmark picker (audit U-F1..U-F4)

- Picker condition (0x409690 item use, 0x4ac830 spell cast): ring ability in 1..3, or class 2 (elf) with
  level >= 50 and the `/telcon` flag `[0x6f1e4a]`. `/telcon` flips it for elves level 50+, else forces it off;
  cleared at login unless class 2. (C; game meaning of telcon I)
- Without the picker: item sends `cdhhh 0x77, objid, curMap, 0, 0`; spell sends `ccchhh 0x81, id>>3, id&7,
  curMap, 0, 0`. Refused on maps 2, 5, 6, 0x53, 0x54, 0x1be, 0x1bf, >0x3fff. (C)
- Confirm (0x418b50): first selected row; item `cdhhh 0x77, objid, map, x, y`, spell `ccchhh 0x81 ...`. A row
  with x = y = 0 sends the current map. With no row selected nothing happens (empty window is inert). (C)
- Delete: Del/Backspace on the selected row opens the yes/no text prompt string 0x34e; on `y` sends `cs 0x80,
  name` (by name) and removes the row. (C)
- `/bookmark` takes no argument: opens a name prompt (string 0x246), then `cs 0x00, name`; name over 27 chars ->
  string 0xcc; refused maps (above plus 8..0xf, 0x12, 0x14, 0x15 and four rectangles on map 4) -> string 0xd6. (C)
- Scroll bar internals and default position: not decoded (U).

## Implementation status in the fork

- Blend (`recursos/modo_mezcla.h`): attribute 104 -> hidden / screen / 50% average / multiply / additive in the exe's order,
  for bodies, clothes layers (the 0x80 ones are no longer skipped) and effects. Tile type byte: 0x10 screen, 0x04 average
  (baked alpha 128, shadows), 0x20 multiply (no longer baked). Multiply is a third pipeline (DST_COLOR / ZERO) whose shader
  outputs white for transparent texels. Not implemented: 0x40, 0x20 (pulse), 0x200 (tint), 0xc00 (fade), the body-flag
  ORs of `FUN_004cc980`. Painter order is kept: `Gpu` splits the sorted instance stream into consecutive runs of the same
  class (normal / screen / multiply; HD text never splits a run) and issues one draw per run with the run's first index
  in the vertex uniform (`relleno.x`) and the matching pipeline. Without screen/multiply instances it stays one draw;
  the world / upper-layer split is kept.
- Frame suffixes: `<N`, `[N`, `^N` keep up to 3 ids per frame (random pick each time); `<` may overlap, `[` only starts
  if the id is not playing. Distance gain is still applied to all three (the exe's rule for `<`/`[` is not confirmed).
  `]N` now also works in list.spz rows. `{`, `}` and `>` are skipped.
- Attribute 113: `TablaSprites::morfo`; `CAMBIO_FORMA` spawns that effect at the object when its gfx changes.
- Attribute 100 (see-through trigger, round 2): parsed from sprite_table.txt and list.spz as n x (x1,y1,x2,y2) into
  `TablaSprites` (`dentro_transparencia`); cell dx = x >> 1 (x in half tiles), dy in tiles, relative to the object's
  cell. `dibujar_personajes` halves the object's alpha while the local player's cell is inside any rectangle. Not
  blocked cells. Only sprite objects (doors, gates...) are covered; map tile objects (house walls) use the fork's older
  `jugador_tapado` alpha 110 heuristic.
- Poison (status &1): the green tint is gone; the exe's pulse L = |12 - tick%24| + 16 (50 ms tick) is approximated by
  an additive flat green silhouette (`Mezcla::SILUETA_ADITIVA`, green = L*4/16/31 of full scale) drawn over the body
  sprite. The exe adds per pixel to the sprite's own colour; the flat add also lifts black outlines. Clothes layers
  are not overlaid.
- Invisible (status &2): the local player's own body is drawn at 50%; other objects are skipped entirely (no sprite,
  name, bars or click target) unless the inventory holds "Ring of Sayha" ($431), then 50%. The exe also requires an
  ability slot 1..3 and no pending teleport (not tracked). l1j-en GMs may see invisibles by other means; not modelled.
- Roof fade (0xc00): the fork has an instant version (`jugador_tapado`: pieces of a map object in front of the player
  that covers the body are drawn at alpha 110, plus a silhouette). TODO: exe timing (50 ms steps of 4 over 0..31, about
  0.4 s; kind 0 fades to 50%, kind 1 to hidden) needs per-group state and the roof kind from the map loader, which
  the segment format here does not carry.

## Round 2 findings (corrections included)

- **Ordering (C):** blends are interleaved with opaque draws. The renderer walks cells in painter order and, per cell,
  draws the ground tile and then every list entry in array order, applying each entry's blend in place (no deferred
  blend pass). The layer byte comes from the sprite data: byte +8 of each 12-byte part record of the .spr frame
  (`FUN_004b0350`, 0x4b03fb); tile objects use 0 or the map entry byte at offset -2. Insertion stops at the first
  entry with layer >= new, so among equal layers the later-added entry is drawn first.
- **Status byte -> flags (C):** &1 -> 0x20 pulse (poison), &8 -> 0x200 tint (freeze; l1j-en never sends it),
  &0x80 -> 0x2 hidden (ghost; never sent), &2 -> 0x100 for the local player / 0x40 for others (invisible).
- **Pulse 0x20 (C):** level `L = |12 - tick%24| + 16` (tick every 50 ms, period 1.2 s); per channel
  `out = min(31, s + ((L*c) >> 4))` with c = 0x100 -> (0,4,0) in 565: a green glow of +4..+7/31 on green.
- **Tint 0x200 (C):** `out = (s >> 1) + c`, c = 0x0c63 -> 565 (1,17,3) dark green cast.
- **0x40 (I):** others' invisible sprites draw at 50% only when the viewer has ability slot 0x8ed57c in 1..3, no pending
  teleport `[0x6f5d1c]`, and "Ring of Sayha" ($431, item +0x6a == 0x57) in the inventory; otherwise they are skipped.
- **Roof fade 0xc00 (C):** map loaders register roof groups per cell (`FUN_00460360(kind)`, kind 0 -> fade to 50%,
  kind 1 -> fade to hidden). `FUN_00468240` fades groups attached to the local player's cell and restores the others;
  a 50 ms timer steps the 0..31 level by 4 (about 0.4 s), the 50% target stops near 16.
- **Attr 100 is a see-through trigger, not blocked cells (C):** each rectangle cell (x in half tiles step 2, y in tiles,
  relative to the object) is registered with flag byte 5; while the local player stands in one of those cells the
  object (houses, shops, doors, gates: 205 gfx) is drawn with flag 1 (50% average), and back to 0 when they leave.
- **Attr 108 is "flying type" (C):** 26 flying monsters (harpies, drakes, griffons, Lindvior, Valakas forms); type 2 with
  packet state nibble 2 -> base action 4 (11 with stride). Attr 106 and 114 have no uses in the 2009 data.
- **0x7026d0 (C/U):** reference count of effects whose frame byte +0x13 == 2 forcing ambient light level 15 (flash);
  trigger unknown.

## Combat animation (2026-11-22, S_AttackPacket 0x487780 -> 0x4c64d0, SetAction 0x4c96c0, tick 0x4c8950)

- The attacker turns to its target when the target object is known (0x4cc6c0); the packet heading only otherwise.
- Only actions 1 (attack) and 2 (damage) add the weapon stance; every other action id is absolute (spells 18 / 19, emotes).
- The target's damage starts at the attacker's "!" frame (frame byte +0x12 bit 0); an attack with no "!" never flinches.
  That damage plays at half length ([+0x11d]); S_DoActionGFX(2) plays it at full length.
- A frame of n ticks lasts n * 1000 / rate ms (list.spz loader 0x440ba8), not 960.
- Frame length modifiers (0x4cda90, `ritmo_accion`), multiplied together:
  - queue of 2+ waiting entries ([+0x78]) x1/2; damage from an attack hit ([+0x11d], action == weapon + 2) x1/2;
  - Triple Arrow / Foe Slayer ([+0x120], set to 4 / 3 by S_SkillSound gfx 4394 / 7020 at 0x48babd, -1 on each SetAction(1)):
    attack rows 5, 12, 21, 25, 51 at (t+2)/3;
  - third accelerator ([+0x127], S_CharPack speed byte 8 or S_Liquor 8) x29/32;
  - haste [+0x20]: 1 x3/4, 2 x2 (17 / 18 the same but only on steps);
  - brave [+0x25] (S_CharPack status >> 4 & 7, or S_SkillBrave type): 1 x3/4, 3 x7/8, 4 x3/4 on steps only, 6 x3/4 on
    non-step actions only, 5 x3/8 (when the global 0x8168a5 is clear);
  - not modelled: [+0x124] (S_PacketBox 44, x2 on actions; l1j-en never sends it) and the global 0x8168a5.
  "Step" means the current action is a walk stance ([+0x10] in {0, 4, 11, 20, 24, 40, 46, 50, 54, 58, 62}, 0x4cd970).
- Busy queue (0x4cb080 push, 0x4cb8a0 IsBusy): S_MoveCharPacket, S_DoActionGFX and S_AttackPacket for an object that is
  mid-step, mid-action or already has queued entries are queued and start when the current one ends (fork: `encolar` /
  `avanzar_colas`, started at the exact end time so steps chain). The own character is not queued (local prediction).
  The attack's projectile and the target's flinch are deferred with it.

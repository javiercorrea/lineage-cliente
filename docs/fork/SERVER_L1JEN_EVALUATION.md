# l1j-en/classic (US 2009, s3ep1) evaluation

`servidor_classic/` is the user's new server. `servidor_local` (jp363, 2000, MariaDB 3307) and Whale (7001, PostgreSQL 5433) were not touched.

## Verdict and pin

- **Branch:** `master` (s3ep1, the final 2009 US client the fork's original UI replicates, profile `s3ep1`). It works; `servidor_classic363/` (branch 363, jp363 protocol) was not needed.
- **Pin:** `103e6f6` = upstream `master` HEAD (checked with `git ls-remote`, Nov 2026; `instalar.sh` already pinned it).
- **Linux:** `servidor_classic/instalar.sh` / `arrancar.sh` / `arrancar_db.sh` / `detener.sh` use `../servidor_local/comun.sh` (apt on Linux, Homebrew on macOS). Own MariaDB datadir `servidor_classic/datos`, port **3309** (socket `datos/my.sock`), db `l1jdb`, user l1j/l1j. Game port **2002** (2000 old server, 2001 classic363, 7001 Whale). Packages: `ant` and `default-jdk-headless` were already present, so nothing was installed with sudo. `detener.sh` kills only `-Dl1j.instancia=s3ep1`.
- **Reproducible changes:** `servidor_classic/patches/*.patch` (`git format-patch`, applied by `instalar.sh`, skipped when already applied, then `ant`), `corregir_db.sql` (idempotent, applied on every install), `migrar_fx.py` (character).

## Feature matrix (live-tested with test instances unless noted)

| Area | L1J-JP (servidor_local) | Whale 3.80C | l1j-en classic + fork |
|---|---|---|---|
| Login / auto account, char list | yes | yes | OK (`traza on`/`paquetes` showed no unknown opcode) |
| World, sprites (Giran, Hidden Dock, ships) | yes | yes | OK, `sprite=ok` |
| NPC dialogs, teleporters | yes | yes | OK. Wizard Merlin (Giran) -> "prison" = Giran Dungeon map 53 with the 3 h timer (`GiranPrisonController`) OK. Decavia hunting grounds were broken (issue 47, fixed) |
| Shops | yes | yes | buy list opens (SHOWSHOPBUYLIST); purchase click not driven |
| Warehouse (personal / pledge) | yes | yes | dialog and retrieve OK; pledge storage not driven |
| Combat, drops | yes | yes | OK (frog killed, "Frog gave you Meat") |
| Spells, GM buffs | yes | yes | `.allbuff` exists (`L1AllBuff`, 29 buffs): effects/animations (SKILLSOUNDGFX, SKILLICONSHIELD, messages) play. Needs access 200 |
| Polymorph | yes | yes | `.poly` OK (POLY packet) |
| Clan create / emblem | yes | yes | Pledge Administrator `create` asks the name (string 83) and the server answers "Only a Prince or Princess..." for the knight (correct). Prince flow and emblem not driven |
| Alliance | partial | patch | OK (patch `0100-clan-alliance`, live-tested with two clients): the US client has it (`alliancelist` / `alliancejoin` / `allianceleave`, packet `0x4e` sub 2 / 3 / 4, format `cc`, no name: the server takes the lord you face). Up to 4 clans, lord of level 25+, table `clan_alliance`; invite = Y/N 223, S_War 6 / 7 + messages 1198-1204. The list answer is a system message (the list packet of the 2009 client is not identified) |
| Bugbear Race | no (was missing) | patch | OK (patch `0101-bugbear-race`, ported from L1J-JP `L1BugBearRace`, live-tested): Maeno 70041 / Polly 70035 / Secil 70042 sell one ticket per runner (item 40309, 500 adena, named `<round>-<runner>`, runner names `$1212 + n`) while a round is open (~9 min), then 5 of the 20 runners (npc 91350-91369) race; winners' tickets are bought back for 500 * allotment. Pages `maeno1` (status / buy / sell), `maeno3`, `maeno4` (records), `maeno5` by race state. Table `race_tickets`, runner NPCs and shop rows in `corregir_db.sql`. Client sprites 3497-3512 draw fine |
| War, castles, pets, trade, mail | yes | partial | code present (`C_War`, `C_Trade*`, `C_Mail`); not live-driven in this pass |
| Boards (Giran message board, Oren ranking board) | yes | n/a | window `tablon` opens (issue 16 not reproduced); ranking content not checked |
| Boat rides (ship maps 5, 6, 83, 84, 446, 447) | yes | n/a | OK: the original sends `C_Ship` (33, `chhh` map, x, y) from its ship clock (0x4a6630..0x4a6bc8: second 90 of a 180 s wall-clock cycle on maps 6 / 83 / 446, second 0 on 5 / 84 / 447); the fork does the same (`barco_llegada`). Live: map 6 -> Talking Island and map 5 -> Aden arrive at the right docks |

## Findings

- GM characters log in invisible and invulnerable (`commands.run_on_login`). NPCs and mobs ignore invisible GMs, so NPCs (cow, Daisy, hens) stand still and no MOVEOBJECT arrives. This is the cause of "wandering NPCs do not move" (issue 6) in GM tests. `corregir_db.sql` turns both off at login; use `.invis` / `.invul` when wanted.
- `.tele x y map` is the move command (`.move` does not exist). `accion "teleport giranD"` needs the quotes in `lin_ctl` (a space splits arguments).
- NPC names in the client: Decavia is "Dekabia", Wizard Merlin "Teleporter^Wizard Merlin" (class prefix before `^`).
- No decoder issue found in the paths exercised; the pledge rank nibble and shop/NPC formats match the `s3ep1` profile.

## l1j-en open issues (55 listed; client-download/support 54, 64, 68, 69 ignored)

| Issue | Verdict | Commit / note |
|---|---|---|
| 47 Decavia teleport | fixed | 1e0d541, `0047` (also Sharial 80146; Sharial order/prices assumed same as Decavia) |
| 43 Lasta / Resistance scrolls | partly fixed | 1e0d541 `corregir_db.sql` (40083, 40853 landing); per-floor Lasta escape landing needs work |
| 3 buy limit 1000 | fixed | 1e0d541, `0003` (long arithmetic, cap 2e9, non-stackable 1000) |
| 42 spell on dead target | fixed | 8a5d31a, `0042` ("Spell failed", no MP spent) |
| 39 Firewall from safety zone | fixed | 8a5d31a, `0039` (untested live) |
| 18 friends list DC | fixed (unverified) | 8a5d31a, `0018` clips the lists to 250 characters |
| 35 skill reuse timers | partly fixed | 8a5d31a: Fire/Lightning Storm 2.5 s; 380 skills still 0, needs audit |
| 6 wandering NPCs | not reproducible | cause was the GM auto-invisibility (see Findings); fixed for GMs by `corregir_db.sql` |
| 16 message / ranking boards | not reproducible | board windows open |
| 15 BP storage | not reproducible | upstream cannot reproduce either; "retrieve without title" is intended |
| 4 mob dies at screen edge | not reproducible | upstream cannot reproduce it either |
| 5 Pirate Island boat | fixed (fork) | `barco_llegada` sends C_Ship like the original; maps 446 / 447 use the same table (only 5 and 6 driven live) |
| 20 Ship Grave Yard / Deep Sea connections | needs work | no detail in the issue |
| 37 invisible mobs | needs work | not reproducible quickly |
| 19 G-haste spellbook name | won't fix | name comes from client string `$1651` |
| 26 food icon / description | needs work | client string / icon ids |
| 46 dice dagger hit animation | won't fix (server) | `L1Attack` turns 0 damage into a miss; not a fork bug |
| 53 death freeze (363 branch) | n/a | not the master/s3ep1 branch |
| 11 fid weapon sprites | won't fix | missing sprites in the client data |
| 9 GM shop prices | fixed | `corregir_db.sql`: shops 81027 / 81028 / 81029 cost 1 adena |
| 31 several rings / helmets equipped | mitigated upstream | `L1EquipmentSlot.setArmor` and `L1PcInventory.loadItems` already refuse the extra items; nothing more to do |
| 14 HV/SI kick at 15 vs client text 13 | won't fix | the server is right (15), the text is in the client |
| 7 spawnlist, 10 kennel, 12 Gereng, 13 stray NPCs (32549,33006), 36 bookmark in houses | needs upstream data | the maintainer plans a full spawnlist / NPC table update; 36 has no detail |
| 50 NPC 80064 silent | needs work | no known html id for the message |
| 1, 34, 49 | needs work | server logic, not reproduced |
| 2, 8, 17, 21-25, 27-30, 32, 33, 38, 40, 41, 44, 45, 48, 51, 52 | needs work | balance / skill / upstream merge items, not triaged |

## Character migration

`servidor_classic/migrar_fx.py` copies knight `fx` (objid 268479040) from `servidor_local/datos/mysql.sock` (SELECT only) into `l1jdb`: account fx/fx (old SHA1 hash; l1j-en rehashes on first login), access level GameMaster (200), level 92, stats, 14 skills, position Giran, and 78 of 82 items with enchant, equipped, identified, attribute enchant. Item ids matched by id+name, else by name (Greater Haste Potion 41477 -> 40018, Greater Mana Potion 41481 -> 90008). Skipped: Create Zombie (47041 is a JP skill-icon pseudo item; skill 41 and the spellbook 40186 exist in l1j-en and the US client), Royal Valor Necklace (21248) and Royal Earring (21249) (names `$13779` / `$13776` are not in the US string table and the items are stat-less dummies even in L1J-JP). Greater Potion of Bravery (41480, `$7736`) is not in the US client either: it is remapped to Potion of Bravery 40014 (`MANUAL` in the script; the 28 of fx were added with `fx_extra_items.sql`). Run `python3 -I servidor_classic/migrar_fx.py` with the DB up and the server stopped.

## settings.ini for the user's client

```
servidor=Classic|127.0.0.1|2002|s3ep1|fx
servidor_elegido=Classic
ui_simple=0
fuente_original=1
texto_hd=1
juego=/path/to/Lineage Justice/
lista_sprites=/path/to/spritelist/list.spz
```

Start with `(cd servidor_classic && nohup setsid ./arrancar.sh > server.log 2>&1 < /dev/null &)`, stop with `servidor_classic/detener.sh`. Log in as fx / fx.

## Missing html pages (scan of l1j-en vs primary + extra data)

Page ids come from the live `npcaction` table plus the string literals of `S_NPCTalkReturn` / `S_ShowHTML` in the Java sources
(~1900 ids). The original client (S3EP1U 0x437440) loads only `<id>-<lang>.html` and, when it is missing, opens no window at all
(0x43a9ca); the fork now does the same (it used to print "page X not found"). Ids missing from both data sets (the many
`horosa`, `vbk`, `nober`... entries in the raw scan are only prefixes of numbered pages that exist):

| id | NPC | status |
|----|-----|--------|
| perkin | Maeno 70041 | fixed: Maeno is the Bugbear Race Manager, page `maeno1` (corregir_db.sql) |
| biyan1 | Biyan 70752 | fixed: page is `biyan` |
| secil | Polly 70035 | no page anywhere (silent, like the original) |
| polee | Secil 70042 | no page anywhere |
| sabin | Sabin 70758 | no page anywhere |
| lesta | Resta 70934 | no page anywhere |
| candle, c_belt, nobelt, gunterde1, shadowevent2 | script-only literals | no page anywhere |

The DB fixes need a server restart (npcaction is cached at start).

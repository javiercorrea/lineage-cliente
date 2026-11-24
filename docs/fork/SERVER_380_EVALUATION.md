# L1JGO-Whale (TW 3.80C/3.81C) evaluation

Phase 1: install and probe. Nothing migrated; `servidor_local` (jp363, port 2000, MariaDB 3307) untouched.

## Verdict

- **Target:** the README says 3.80C, but `internal/net/packet/opcodes.go` is headed "Server Opcodes (V381)"
  and the commit log mentions yiwei (a 3.81C emulator) alignment. It is a 3.80C/3.81C TW server. Client
  opcodes work unchanged with the fork's `tw380` table.
- **Pin:** `f95af7d` of the newer fork `eddy7697/L1JGO-Whale` (master, 2026-10-15, 59 commits on top of r0ptik's `9a19405`: Java-parity fixes for loot, buffs, enchant, inn). `instalar.sh` moves an older clone to the pin; our patches apply unchanged; migrations (goose, schema 40) run at server start.
- **Linux:** `servidor_380/instalar.sh` now works (apt `golang-go` + `postgresql-16`, user cluster on 5433,
  Go toolchain `go1.23.4` auto-downloaded because go.mod wants 1.23 and Ubuntu ships 1.22; server binds
  127.0.0.1:7001). `sudo` was passwordless here; on a box that prompts, run
  `sudo apt-get install -y golang-go postgresql-16` first and re-run the script.
- **Result:** playable on the fork. Login, character creation, world, NPC dialogs, combat, polymorph,
  warehouse and GM commands work. The server is a far larger, more modern feature set than servidor_local,
  but it is Traditional Chinese (server text in Big5), and the fork cannot decode that yet.

## Test setup

`LIN_CONFIG` ini with `servidor=W380|127.0.0.1|7001|tw380|fx`, `juego=` either L1Justice or the TW 3.81C
client, run through `herramientas/lin_ctl.py` (Xvfb). Accounts are auto-created on first login. There is
no access-level check: `.level`, `.item`, `.gold`, `.move`, `.spawn`, `.poly`, `.class`, `.allskill`... work
for everyone (`.help` lists ~50).

## Feature matrix (live-tested unless noted)

| Area | servidor_local (363c) | Whale 3.80C | Fork on Whale |
|---|---|---|---|
| Login, auto account, char list | yes | yes | OK |
| Char create (knight) | yes | yes | OK (new char starts on map 2005 tutorial area) |
| World render (map 2005, 4) | yes | 1039 map files | OK with L1Justice data, sprites `ok` |
| NPC names | desc-e `$N` | server sends `$N` ids | OK with Justice desc-e; see 3.81C below |
| NPC dialogs (client html) | yes | yes | OK: Pledge Manager, teleporter, alchemist, blacksmith, storage, Dio show English pages |
| Warehouse | yes | yes | OK: retrieve list opens (SHOWRETRIEVELIST) |
| Shop | yes | yes | list opens in English (Arieh, 3 columns of fireworks, adena total); the OK click was not driven by the control socket, so the actual C_Result buy/sell is unverified (wire format read from `shop.go`: same as the fork's) |
| Combat vs mobs, death/drops | yes | yes | OK (frogs killed, "xp/drop" messages) |
| Inventory, +19 items via `.item` | yes | yes | OK |
| Polymorph (`.poly`) | yes | yes | OK (POLY packet) |
| Spells (`.allskill`, spell window) | yes | yes | OK: mage casts Light (C_UseSkill, SKILLSOUNDGFX/SPMR back); `lanzar` takes names, not ids |
| Stat raise page (level > 50 bonus points) | n/a | `RaiseAttr` page at 51+ | NOT Whale config: `bonusStatMinLevel = 51` is a constant in `handler/statalloc.go` (Java 3.80 C_Attr 479 / S_Bonusstats behaviour); shown at login and on level-up while points are unspent, blocks other dialogs until spent |
| Clan create / emblem | yes | yes | OK: `create` on `bpledge2` is a client action (Whale has no handler): the fork asks string 83 and sends C_CreateClan (222); emblem upload (`/emblem file.bmp`) answered with S_Emblem and cached; `servidor_380/emblem_id.patch` makes the char packs carry the clan's emblem id (Whale sent 0), CLAN (72) = emblem download |
| Alliance (`handler/alliance.go`) | partial | `servidor_380/alliance.patch` (applied by instalar.sh) implements invite (C_Rank 3: lord of level 25+ names a clan, or faces the other lord; Y/N 223 to the other lord; max 4 clans; only the alliance prince invites; not in a siege war), withdraw (C_Rank 4 + Y/N 1210; the alliance dissolves with one clan left) and persistence in `character_alliance` (loaded at start); S_War 6/7 + messages 1200/1204 | live-tested with two instances (Alpha/Beta: invite, accept, list, withdraw, DB row removed). Needed a client fix: tw380 answers Y/N in the short form `[h type][c yes]` (Whale reads one byte), the long form made every answer read as no |
| War, castles, pets | partial | implemented in Whale | not live-tested (needs siege setup / taming); pet panel decodes through CHARSYNACK subtype 0x0c |

## Protocol (`TW380_SRV`)

Mapped now: `CHARSYNACK` 64 shares PETCTRL's decoder (subtype in the first byte: 0x0c pet menu, 1-3 char reset
with 4-byte HP/MP, 0x2a bookmark list with 127 slots, 0x3c emblem sync -> `CLAN_EMBLEMA_OBJETO`; others
ignored), `REDMESSAGE` 105 (as a server message), `CLANNAME` 72, `CLANATTENTION` 200, `CHANGENAME` 46 (decoded
and ignored, only named so `paquetes` stops showing `?`). `CLANMATCHING` 0 is unmapped. The prince's rank nibble
is 10 in 3.80C (4 in 3.63): `PerfilProtocolo::rango_principe`. Whale's clan id field in the char packs is the
emblem id (0 without emblem), so the fork also checks the own rank (0xb = no pledge) before an emblem upload.

## Text and language

- Server text is **Big5** (`client_language_code = "MS950"`). The tw380 profile converts every server string to
  UTF-8 on decode (`recursos/big5.cpp`, table generated by `herramientas/generar_big5.py`). CJK glyphs are still
  not drawable (UI font is Latin-1 TTF), so Chinese text that remains shows blank; English is the goal.
- Chat clipping is UTF-8 aware (it used to cut multi-byte characters and push the whole line back to CP949).
- Item and NPC names: the yaml `name` fields are Chinese. `servidor_380/translate_names.py` +
  `servidor_380/names_en.tsv` (kind, id, chinese, english, source) rewrite them, applied by `instalar.sh` on the
  pristine yaml. `mapped` rows (2910) come from the l1j-jp Taiwan CSVs (`servidor_local/l1j-jp/db/csv/tw`,
  `identified_name_id` = `$N` of Justice's desc-e, matched by id + Chinese name or gfx, else by unique name) with
  `name_fixes.tsv` English for plain ids; `translated` rows (1274, incl. 73 NPCs with an empty `nameid`) are
  hand/agent translations in US naming, reviewable in the TSV (regenerate with `translate_names.py generate`,
  translated rows are kept). Whale ships no English variants. Not done: Chinese literals inside the Go handlers
  (GM command output such as `.help`/`.move`, a few system lines): they need a server patch or a client-side
  table. `.item <name>` takes the new English names, ids still work.
- NPC names and dialogs are **not** server text: names come as `$N` ids resolved by `desc-e.tbl`, dialogs
  are client `Text.pak` html. With L1Justice both are English.

## TW 3.81C client as data (a local `休閒天堂381/` install)

- The loader opens it unchanged: `Sprite*.pak/idx`, `Tile`, encrypted `Text.idx/pak`, `map/` all read; login,
  enter-world and render of the same scene work (screenshot compared with Justice: same terrain, same
  sprites for the player, Cuse, Karen, the pink Beginner helper).
- Text: it **does** have both `desc-e.tbl` / `string-e.tbl` / `spelldesc-e.tbl` and the `-c` versions
  (Chinese). But its `desc-e.tbl` is 114 KB against 365 KB in Justice, so many `$N` names stay raw
  (`$8446`, `$5085` in the screenshot) and some plain names appear without their title (`Cuse`, `Karen`).
  English coverage is therefore worse than Justice; the Chinese `desc-c.tbl` (340 KB) has them all, but
  the fork has no language switch for it (`idioma=` exists in settings; not tried with `c`) and no Big5 font.
- Same server content renders with both: with 3.81C the scene was identical for the starter area; sprite list
  format (`List.spr.bin` vs Justice `spritelist/list.spz` + `sprite_table.txt`) was not needed for this scene.
  Newer gfx ids (post-3.63) are more likely to be in 3.81C; not measured systematically.
- Recommendation: keep Justice as `juego=` for English; use the 3.81C only as a fallback source of newer
  sprites/maps, or add a text-source override (desc-e from Justice, sprites from 3.81C).

## Data gaps

- Server `map/` has 1039 maps; map 2005 and 4 render with Justice. Other maps and polymorph/gfx ids not
  swept; do a sweep of `data/yaml/spawn_list.yaml`/`npc_list.yaml` gfx ids against the sprite table.
- `data/yaml/maps_missing_sql.txt` lists maps the server itself lacks.

## Remaining work before migration

1. Chinese literals in Go handlers (GM output) -> English (patch or table).
2. Live-test shop buy/sell confirmation, alliance (invite is a Whale stub), war/castle, pets.
4. Sweep gfx ids used by the server against Justice and 3.81C.

## Character `fx` (migrated)

`servidor_380/migrar_fx.py` (needs the old MariaDB running; it only SELECTs from `servidor_local`'s `l1jdb_363`, char
268479040) creates in Whale's PostgreSQL the account `fx` / password `fx` (access 200) and the knight `fx` (male, class 61):
level 92, exp 1570544826, STR 31 CON 20 DEX 31 CHA 12 INT 9 WIS 14, 42 bonus points spent, HP/MP 1967/109, lawful 938, title fx,
map 4 (33437,32824), GM access 200, the 14 knight skills (1-8, 87-91, 108) and all 80 inventory items with enchant, equipped
state and identified flag (+19 Iron/Demon/Death Knight/Kurtz sets, Dragon Slayer, 100004223 adena...). Item ids are the same
official ids except: Greater Haste/Bravery/Mana potions 41477/41480/41481 -> 49302/49305/49306 and the Create Zombie spellbook
47041 -> 40186; Royal Valor Necklace (21248) and Royal Earring (21249) are jp-only and were skipped. Re-running replaces the
character. It was not logged in.

## Pointing the client at the 3.80C server

Do not edit the running client's file by hand while it is open; in `settings.ini` (`~/.local/share/Lineage/LineageCliente/`)
add a server line and select it (or pick it in the login server menu):

```
servidor=Whale 3.80C|127.0.0.1|7001|tw380|fx
servidor_elegido=Whale 3.80C
juego=/path/Lineage Justice/
juego_extra=/path/休閒天堂381/
juego_extra=/path/servidor_380/l1jgo/吃檔/
```

Start the server first with `servidor_380/arrancar.sh` (Postgres 5433, server 127.0.0.1:7001; `instalar.sh` once). Log in as
`fx` / `fx` and enter `fx`. The first line is `nombre|host|puerto|protocolo|cuenta`; the protocol must be `tw380`
(S_MessageYN answers use the short form on it). `servidor_local` (jp363, port 2000) stays as it is; switch back by selecting
its entry again. Optional: `lista_sprites=` pointing at `juego/spritelist/list.spz`, as for the main server.

## Operation

`servidor_380/arrancar.sh` / `detener.sh`, log `servidor_380/servidor.log`, Postgres on 127.0.0.1:5433,
DB `l1jgo`. Neither the clone (`servidor_380/l1jgo/`) nor `datos/` is committed.

## Pruned data (instalar.sh -> `prune_data.py apply servidor_380/prune_ids.tsv`)

Applied to the pristine translated yaml, reproducible and reviewable (`prune_ids.tsv`: kind, id, reason; regenerate with
`prune_data.py generate`, which needs the game folders and a decrypted 3.81C `desc-c.tbl`, see `dump_text.cpp`).
The gfx test uses `gfx_sources.py` over Justice + the 3.81C install + the 吃檔 folder (`.spr`/`.tbt`/maps; see DATA_COVERAGE_381.md).

| Removed | Count | Rule |
|---|---|---|
| NPC/monster ids (spawn_list, light_spawn_list, drop_list entries: 1507 yaml entries) | 366 | spawned NPC whose gfx exists in no source |
| Polymorph entries | 259 | poly gfx in no source |
| Dolls (and their items) | 4 | doll gfx in no source |
| Items, `gfx` reason | 21 + 4 dolls | inventory icon / ground gfx in no source |
| Items, `private` reason | ~750 | Whale's `translated` rows of `names_en.tsv` (id not in the official TW tables) whose Chinese name is also absent from the 3.81C `desc-c.tbl`: ids 300000+, 400000+, 600000+ (custom weapons/armor/scrolls), 47054-47xxx enchant stones, 70000-79999 etc. |

Items are also dropped from every list that names them by `item_id` (drops, shops, recipes, `item_power.yaml`, dolls...).
`item_vip.yaml` is already empty in this pin. `npc_list.yaml` definitions are kept (summons and quest spawns may still name
them, and the client draws a placeholder); `item_power.yaml` is an official mechanic (enchant bonuses) and stays for the
remaining items. Entries naming a removed id under another key (for example `result_item`) are not rewritten; the server
started with no warnings. The old 3.81C-official test is name-based, so an official item whose Chinese name differs
between Whale and `desc-c.tbl` can be pruned wrongly; review `prune_ids.tsv` rows with `private`.

# Plan: páginas de atajos, atajo elegido, curarse con la tecla, auto-buffs y auto-poción

Cinco mejoras de jugabilidad pedidas sobre la barra de atajos y el combate. No son del cliente original:
son comodidades nuevas, todas opcionales. Es solo el plan: todavía no hay nada implementado.

**Fuentes.** El código del cliente (`src/app/barra_ui.cpp`, `hechizos_ui.cpp`, `inventario_ui.cpp`,
`mundo/mundo.h`) y el servidor `l1j-jp` (`data/xml/Item/HealingPotion.xml`, `C_UseItem.java`,
`L1HealingPotion.java`) con su base `l1jdb_363` (`etc_items`). Cada dato lleva su estado: **[V]** leído en
el código o en el servidor, **[H]** hipótesis, **[?]** incógnita.

## 0. Lo que ya existe

- **Barra de atajos [V]** (`barra_ui.cpp`): 10 casilleros (`CASILLEROS_BARRA`) abajo al centro, encima del
  panel. Se llenan arrastrando un hechizo (ventana de magia) o un ítem (inventario); entre casilleros se
  arrastra para mover; clic derecho o soltar fuera los vacía. Clic, Alt+número o (con "chat con Enter" y el
  chat cerrado) el número 1–0 usan el casillero (`usar_casillero`). Se guarda por personaje en
  `settings.ini`: `barra=Nombre:h4,i80:1234*,0,...;Otro:...` (`h` hechizo, `i` gfx:id de ítem, `*` el
  elegido).
- **Casillero elegido [V]**: clic del medio sobre un casillero lo elige (marco dorado de 2 px; otra vez lo
  suelta). Con el botón del medio apretado sobre el mundo, `usar_elegido` lo usa sobre el objeto bajo el
  puntero y lo repite mientras se mantiene (hechizos al terminar su espera; ítems cada 500 ms). Un hechizo
  con objetivo **sin nada bajo el puntero no hace nada** (es lo que cambia el punto 2).
- **Hechizos [V]**: `pedir_hechizo(id, objetivo)`; con objetivo y sin él, `hechizo_apuntando` espera el
  clic. `Hechizo` (`recursos/hechizos.h`) sabe `con_objetivo`, `de_ataque`, `duracion_s`, `mp`, `alcance`.
- **Efectos activos [V]** (`EstadoJugador::efectos`, `mundo.h`): clase, tipo y fin en ms. Llegan del
  servidor (haste, brave, escudos, aura, FUE/DES...) o los registra el cliente (`EA_HECHIZO`: buffs sin
  ícono del servidor, al ver su gráfico sobre el personaje). `hechizo_de_efecto` (`hechizos_ui.cpp`)
  traduce un efecto al hechizo que lo da.
- **Daño recibido [V]**: `procesar_red` compara `jugador.hp` con `hp_anterior` en cada frame.
- **Pociones de curación [V]** (`etc_items` y `HealingPotion.xml`): el cliente las reconoce por el nombre
  que manda el servidor (el primer `$N`):

  | Familia | Roja | Naranja | Transparente | Demora (`delay_time`) |
  |---|---|---|---|---|
  | Condensada | `$1251` 15–55 | `$1252` 45–75 | `$1253` 70–110 | 1000 ms |
  | Antigua | `$2575` 10–30 | `$2576` 20–45 | `$2577` 30–85 | 500 ms |
  | Normal | `$26` 15–55 | `$235` 45–75 | `$238` 70–110 | 1000 ms |

  Con Decay Potion activa (habilidad 71) el servidor no deja beber: mensaje 698 [V].

## 1. Páginas de atajos

```
+--+--+--+--+--+--+--+--+--+--+  ^
|  |  |  |  |  |  |  |  |  |  |  1
+--+--+--+--+--+--+--+--+--+--+  v
```

- **Datos**: `Atajo barra[PAGINAS_MAX][CASILLEROS_BARRA]` y `pagina_barra`. Cantidad de páginas en
  `settings.ini` (`paginas_barra=3`, de 1 a `PAGINAS_MAX` = 9). Con 1 página no se dibuja el selector.
- **Selector**: a la derecha de la barra, del alto de un casillero: `^` arriba, el número en el medio y `v`
  abajo (la fuente de la interfaz es Latin-1: no tiene `↑`/`↓`; se dibujan `^`/`v` como en las barras de
  scroll, o dos triángulos con rectángulos). Clic en `^`/`v` cambia de página (con vuelta: de la última a
  la primera).
- **Rueda del mouse** sobre la barra o el selector: cambia de página. La rueda en otras zonas no cambia
  (chat, ventanas y listas la usan).
- **Teclas**: 1–0 y Alt+número usan el casillero de la página visible. Arrastrar entre casilleros, a la
  barra y fuera de ella actúa sobre la página visible.
- **Guardado** compatible con el formato actual: las páginas separadas por `/`
  (`barra=Nombre:h4,0,.../i80:12,0,.../...`). Sin `/` es la página 1 (lo que ya hay guardado).
- **Pruebas**: test de leer/escribir el formato (con y sin `/`); control en vivo: `barra-pagina <n>` y la
  rueda con `raton`/`clic`.

## 2. Atajo elegido con el botón del medio

Casi todo existe (sección 0). Cambia:

- **Sin objetivo bajo el puntero**: hoy no hace nada; pasa a ser lo mismo que la tecla 0–9
  (`usar_casillero`): un hechizo con objetivo queda esperando el clic (`hechizo_apuntando`), uno sin
  objetivo se lanza y un ítem se usa. Mantener el botón no repite en ese caso (si no, repetiría el "elegir
  objetivo").
- **Marca**: borde dorado de 3 px (hoy 2) para que se distinga del resaltado del puntero.
- **Con páginas**: el elegido es un (página, casillero). El botón del medio lo usa aunque su página no esté
  a la vista; el selector muestra un punto junto al número si el elegido está en otra página. **[decisión]**
- Guardado: el `*` sigue igual, en la página donde está.

## 3. Mantener 1–0 sobre un hechizo de curación: sobre uno mismo

- **Qué hechizos**: los que piden objetivo y no son de ataque (`con_objetivo && !de_ataque`: curaciones y
  buffs que se pueden dar a otro, como Lesser Heal, Heal, Greater Heal, Nature's Touch, Haste). Los de
  ataque nunca. **[decisión: ¿solo curaciones o todos los no ofensivos?]**
- **Cómo**: al apretar, igual que hoy (queda esperando el clic del objetivo). Si la tecla sigue apretada
  `MS_MANTENER` = 300 ms, se cancela la espera y se lanza sobre el propio personaje
  (`pedir_hechizo(id, jugador.id)`). Así un toque no se retrasa.
- **Repetir**: mientras se mantenga, vuelve a lanzar en cuanto termina la espera del hechizo (como el botón
  del medio). **[decisión: ¿una vez por pulsación o repetir?]**
- **Entrada**: hoy `EntradaFrame` solo ve la tecla al bajar (`numero`). Hace falta saber cuál número está
  apretado y desde cuándo: `SDL_GetKeyboardState` por frame, o guardar la bajada y la subida
  (`SDL_EVENT_KEY_UP`), ignorando las repeticiones de SDL.
- Solo con "chat con Enter" y el chat cerrado (con el chat siempre abierto los números son texto). Alt+número
  también, si se mantiene Alt.
- **Pruebas**: el control en vivo necesita mantener una tecla: `tecla-mantener <nombre> <ms>`. Prueba con
  agente7 (GM: `.addskill` para Heal) bajando su HP.

## 4. Auto-buffs

- **Barra**: a la derecha del selector de páginas, `CASILLEROS_AUTO` = 4 casilleros con un borde de otro
  color (verde) y un interruptor general (encendido/apagado, apagado por defecto). Se llena arrastrando
  hechizos desde la ventana de magia; clic derecho vacía. Se guarda por personaje (`autobuffs=Nombre:1|42,43,0,0`:
  encendido y los hechizos).
- **Qué se acepta**: solo buffs al propio personaje: `!de_ataque && duracion_s > 0` y que se puedan lanzar
  sobre uno mismo (sin objetivo, o con objetivo que admita al propio personaje). Al soltar uno que no lo es,
  un aviso en el chat y no se pone.
- **Cuándo se lanza** (`procesar_autobuffs`, una vez por frame), el primer casillero que cumpla:
  1. el efecto no está activo, o le quedan menos de `MARGEN_AUTO` = 2 s;
  2. tampoco se lanzó hace menos de su `duracion_s` (por si el cliente no detecta el efecto: hay buffs sin
     ícono ni gráfico, y sin esto se relanzaría sin parar);
  3. alcanza el MP, el personaje no está muerto, inmóvil, fantasma ni en un intercambio, no hay otro
     hechizo pendiente ni apuntando, y ya pasó la espera del anterior (`hechizo_listo_ms`);
  4. el jugador no está caminando ni atacando en ese momento (no le corta lo que hace; se lanza al
     detenerse).
- **"Activo"**: `efecto_de_hechizo(id)` = algún `EstadoJugador::efectos` cuyo `hechizo_de_efecto` sea `id`
  (incluye los `EA_HECHIZO` que registra el cliente).
- **Si falla**: con los mensajes del servidor de MP insuficiente o hechizo fallido, ese casillero espera
  `ESPERA_FALLO` = 10 s antes de reintentar.
- **Riesgo**: es automatización; algunos servidores la consideran macro. Apagado por defecto y avisado en
  las opciones. **[?] reglas del servidor privado real**
- **Pruebas**: función pura `elegir_autobuff(estado, casilleros, ahora) → id o 0`, con tests de cada
  condición. En vivo con agente7 y un buff corto.

## 5. Auto-poción

- **Opciones** (ventana de opciones, por personaje): encendida/apagada (apagada por defecto) y umbral de HP
  (`UMBRAL_POCION`, 60 % por defecto). **[decisión: ¿por personaje o global?]**
- **Cuándo**: en el frame en que el HP baja (`hp < hp_anterior`) y también mientras siga por debajo del
  umbral (si una poción no alcanzó), si `hp * 100 / hp_max < umbral`, el personaje está vivo y ya pasó la
  demora de la última poción (1000 ms; 500 si fue antigua).
- **Qué poción**: primero la familia (condensada, después antigua, después normal) y dentro de la familia el
  color según lo que falta (`falta = hp_max - hp`):
  - transparente si `falta >=` su curación media (normal/condensada 90, antigua 57);
  - naranja si `falta >=` su media (60; antigua 32);
  - si no, roja.
  Si ese color no está en la familia, el más cercano de la misma familia (primero uno menor, para no
  desperdiciar); si la familia no tiene ninguno, la siguiente familia.
- **Reconocerlas**: por el `$N` del nombre del ítem (tabla de la sección 0), no por el gfx (las de otras
  versiones pueden compartirlo). **[H]** los mismos `$N` en jp363, tw380 y s3ep1; verificar con
  `desc-e.tbl` de cada instalación.
- **Si no puede beber** (mensaje 698, Decay Potion): espera 5 s antes de volver a intentar.
- **Pruebas**: función pura `elegir_pocion(inventario, hp, hp_max) → id o 0` con tests (orden de familias,
  color por lo que falta, falta de un color). En vivo con agente7: `.item 40019 10`, `.item 40022 10`,
  `.item 40010 10` y bajar el HP (un monstruo con `.spawn`).

## 6. Fases

| Fase | Entrega | Verificación |
|---|---|---|
| F1 | Páginas, selector y rueda; formato con `/` | test del formato; en vivo: cambiar de página y usar un casillero |
| F2 | Elegido: fallback a 0–9, borde de 3 px, elegido en otra página | en vivo: botón del medio con y sin objetivo |
| F3 | Mantener 1–0 sobre uno mismo; `tecla-mantener` en el control | en vivo con Heal de agente7 |
| F4 | Auto-buffs | tests de `elegir_autobuff`; en vivo con un buff corto |
| F5 | Auto-poción | tests de `elegir_pocion`; en vivo con pociones de las tres familias |

Cada fase en su PR. F1 primero (cambia la estructura de la barra que usan F2–F4); F4 y F5 son
independientes entre sí.

## 7. Decisiones pendientes

1. **Elegido con páginas** (punto 2): ¿el botón del medio usa el elegido aunque su página no esté a la vista
   (propuesta), o solo si está a la vista?
2. **Mantener sobre uno mismo** (punto 3): ¿todos los hechizos con objetivo no ofensivos (propuesta) o solo
   los de curación? ¿Una vez por pulsación o repetir mientras se mantenga (propuesta: repetir)?
3. **Auto-buffs** (punto 4): ¿4 casilleros? ¿Se pausan en zonas seguras (pueblos) para no gastar MP?
4. **Auto-poción** (punto 5): umbral por defecto (60 %), ¿por personaje (propuesta) o global?
5. **Servidor real**: ¿permite auto-buff y auto-poción?

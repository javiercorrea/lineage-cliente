# Plan: páginas de atajos, atajo elegido, curarse con la tecla, auto-buffs y auto-poción

Cinco mejoras de jugabilidad pedidas sobre la barra de atajos y el combate. No son del cliente original:
son comodidades nuevas, todas opcionales. Auto-buff y auto-poción son macros del lado del cliente, y se
aceptan como tales. Es solo el plan: todavía no hay nada implementado. Las decisiones están en la sección 8.

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
                 atajos (10 × 38 px)          página      auto-buffs (6 × 34 px)
+--+--+--+--+--+--+--+--+--+--+  ^   +-+-+-+-+-+-+ [A]
|  |  |  |  |  |  |  |  |  |  |  1   | | | | | | |
+--+--+--+--+--+--+--+--+--+--+  v   +-+-+-+-+-+-+
```

- **Ancho**: 10 × 38 − 2 = 378 px de atajos, 8 de separación, 18 del selector, 8 de separación y
  6 × 34 − 2 = 202 de auto-buffs (más el interruptor `[A]`, que va encima del primer casillero de
  auto-buffs para no sumar ancho): ~614 px, centrado. Entra en la vista mínima de 640 px y en el panel
  original (640 de ancho).

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
  a la vista; el selector muestra un punto junto al número si el elegido está en otra página. Elegir otro
  casillero (en cualquier página) reemplaza al anterior: hay uno solo.
- Guardado: el `*` sigue igual, en la página donde está.

## 3. Mantener 1–0 sobre un hechizo de curación: sobre uno mismo

- **Qué hechizos**: los que piden objetivo y no son de ataque (`con_objetivo && !de_ataque`: curaciones y
  buffs que se pueden dar a otro, como Lesser Heal, Heal, Greater Heal, Nature's Touch, Haste). Los de
  ataque nunca. Un hechizo sin objetivo ya se lanza sobre uno mismo con un toque; un ítem no cambia.
- **Cómo**: al apretar, igual que hoy (queda esperando el clic del objetivo). Si la tecla sigue apretada
  el tiempo de espera configurado (`cfg.ms_mantener_tecla`, 300 ms por defecto), se cancela la espera y se
  lanza sobre el propio personaje (`pedir_hechizo(id, jugador.id)`). Así un toque no se retrasa.
- **Configurable** en el juego y en el programa de configuración (sección 6): de 150 a 1000 ms. Es una
  preferencia de la persona, no del personaje: global.
- **Repetir**: mientras se mantenga, vuelve a lanzar sobre uno mismo en cuanto termina la espera del
  hechizo (`hechizo_listo_ms`) y alcanza el MP, como el botón del medio. Soltar la tecla corta la
  repetición (lo ya pedido se lanza igual). Sin MP, no pide más: queda esperando hasta que alcance o se
  suelte.
- **Entrada**: hoy `EntradaFrame` solo ve la tecla al bajar (`numero`). Hace falta saber cuál número está
  apretado y desde cuándo: `SDL_GetKeyboardState` por frame, o guardar la bajada y la subida
  (`SDL_EVENT_KEY_UP`), ignorando las repeticiones de SDL.
- Solo con "chat con Enter" y el chat cerrado (con el chat siempre abierto los números son texto). Alt+número
  también, si se mantiene Alt.
- **Pruebas**: el control en vivo necesita mantener una tecla: `tecla-mantener <nombre> <ms>`. Prueba con
  agente7 (GM: `.addskill` para Heal) bajando su HP.

## 4. Auto-buffs

- **Barra**: a la derecha del selector de páginas, `CASILLEROS_AUTO` = 6 casilleros de 34 px con un borde
  de otro color (verde) y un interruptor general `[A]` (encendido/apagado, apagado por defecto; encendido
  con fondo verde). Se llena arrastrando hechizos desde la ventana de magia o desde un casillero de atajos;
  entre casilleros de auto-buff se arrastra para ordenar; clic derecho vacía. Debajo de cada casillero, lo
  que le queda al efecto (como en los íconos de efectos activos). Se guarda por personaje
  (`autobuffs=Nombre:1|42,43,0,0,0,0`: encendido y los hechizos), no en una página: es una sola fila.
- **Orden**: si varios vencen a la vez, se lanzan de izquierda a derecha, de a uno (el siguiente al terminar
  la espera del anterior).
- **No se pausa** en zonas seguras ni en pueblos: mientras esté encendido, mantiene los buffs siempre.
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
- **Macro del lado del cliente**: no manda nada que el jugador no pudiera mandar a mano (el mismo
  C_UseSkill, con la misma espera entre hechizos), así que el servidor lo ve como uso normal. Apagado por
  defecto.
- **Pruebas**: función pura `elegir_autobuff(estado, casilleros, ahora) → id o 0`, con tests de cada
  condición. En vivo con agente7 y un buff corto.

## 5. Auto-poción

- **Opciones** por personaje: encendida/apagada (apagada por defecto) y umbral de HP (60 % por defecto; de
  10 % a 95 %, de a 5). Se guardan en `settings.ini` (`autopocion=Nombre:1,60;Otro:0,70`), como los
  atajos: cada personaje con su encendido y su umbral, y sin entrada, apagada con 60 %. Se cambian en el
  juego y en el programa de configuración (sección 6).
- **Indicador**: encendida, un ícono chico de poción junto a la barra de HP (rojo si no queda ninguna
  poción de curación en el inventario).
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
- **Macro del lado del cliente**, como los auto-buffs: el mismo C_UseItem que un doble clic, con la demora
  de la poción.
- **Pruebas**: función pura `elegir_pocion(inventario, hp, hp_max) → id o 0` con tests (orden de familias,
  color por lo que falta, falta de un color). En vivo con agente7: `.item 40019 10`, `.item 40022 10`,
  `.item 40010 10` y bajar el HP (un monstruo con `.spawn`).

## 6. Configuración: en el juego y en el programa de configuración

Las mismas opciones en los dos lugares, sobre las mismas claves de `settings.ini` (`rutas.{h,cpp}`:
`Config`, `leer_config`, `escribir_config`), con los mismos límites.

| Opción | Clave | Alcance | Valores |
|---|---|---|---|
| Espera para lanzar sobre uno mismo (punto 3) | `ms_mantener_tecla=300` | global | 150–1000 ms |
| Páginas de atajos (punto 1) | `paginas_barra=3` | global | 1–9 |
| Auto-poción encendida (punto 5) | `autopocion=Nombre:1,60` | por personaje | sí / no |
| Umbral de la auto-poción (punto 5) | (la misma clave) | por personaje | 10–95 %, de a 5 |

**En el juego** (ventana de opciones, `ventanas.cpp`), una fila por opción, como las demás:

- "Hold to self-cast": 150 → 200 → 300 → 400 → 500 → 700 → 1000 ms → 150 (clic avanza, clic derecho
  retrocede).
- "Hotbar pages": 1 → 2 → ... → 9 → 1. Al bajar la cantidad, las páginas que sobran no se borran
  (vuelven al subirla); la visible pasa a la última que queda.
- "Auto potion (Agente7)": On / Off, del personaje con el que se juega.
- "Auto potion at HP": 60 %; clic +5, clic derecho −5, entre 10 y 95.
- Los cambios se guardan al momento (`guardar_config`, como hoy).

**En el programa de configuración** (`tools/settings.cpp`):

- Pestaña Interfaz: "Espera para lanzar sobre uno mismo" (desplegable con los mismos valores) y "Páginas
  de atajos" (1–9).
- Pestaña Personajes: por cada personaje, "Auto-poción" sí/no y "Umbral" (desplegable 10–95 %), junto a
  lo que ya muestra (atajos y barra). `personajes_con_datos` también tiene que listar a los que solo
  tienen `autopocion=` o `autobuffs=`, y borrar un personaje borra también esas claves.
- Textos en el catálogo (`i18n/textos.h`) en los 5 idiomas, como el resto del programa.
- Valores fuera de rango en un `settings.ini` escrito a mano: se recortan al leer (`leer_config`), y el
  test de `rutas` lo cubre.

## 7. Fases

| Fase | Entrega | Verificación |
|---|---|---|
| F1 | Páginas, selector y rueda; formato con `/`; cantidad de páginas en opciones y editor | test del formato; en vivo: cambiar de página y usar un casillero |
| F2 | Elegido: fallback a 0–9, borde de 3 px, elegido en otra página | en vivo: botón del medio con y sin objetivo |
| F3 | Mantener 1–0 sobre uno mismo, con repetición; espera configurable (opciones y editor); `tecla-mantener` en el control | en vivo con Heal de agente7: una pulsación larga lanza varias veces sobre sí |
| F4 | Auto-buffs: 6 casilleros, interruptor, guardado por personaje | tests de `elegir_autobuff`; en vivo con un buff corto, también en un pueblo |
| F5 | Auto-poción con opciones por personaje, en el juego y en el editor (pestaña Personajes) | tests de `elegir_pocion` y del guardado; en vivo con pociones de las tres familias y dos personajes con umbrales distintos |

Cada fase en su PR. F1 primero (cambia la estructura de la barra que usan F2–F4); F4 y F5 son
independientes entre sí.

## 8. Decisiones

| # | Tema | Decisión |
|---|---|---|
| 1 | Elegido con páginas (punto 2) | El botón del medio lo usa aunque su página no esté a la vista; un punto junto al número lo indica. (Propuesta, sin objeción.) |
| 2 | Mantener 1–0 (punto 3) | Todos los hechizos con objetivo no ofensivos; **repite mientras se mantiene**. |
| 3 | Auto-buffs (punto 4) | **6 casilleros**; **no se pausan** en pueblos ni zonas seguras. |
| 4 | Auto-poción (punto 5) | **Configurable por personaje** (encendido y umbral; 60 % por defecto). |
| 5 | Servidor | Auto-buff y auto-poción son **macros del lado del cliente**: mandan lo mismo que el jugador a mano. |
| 6 | Dónde se configura | Auto-poción (encendido y umbral) y la espera de mantener 1–0 se cambian **en el juego y en el programa de configuración** (sección 6). La espera es global; la auto-poción, por personaje. |

Quedan por verificar en la implementación (no son decisiones): los `$N` de las pociones en tw380 y s3ep1
(sección 5) y los mensajes exactos del servidor para "sin MP" y "hechizo fallido" (sección 4).

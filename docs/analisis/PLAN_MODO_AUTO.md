# Plan: modo auto programable

Un modo automático dentro del cliente, que sigue un **programa** escrito por el jugador: cómo moverse
(recorrer el mapa, ir entre puntos o seguir a alguien) y **reglas** de qué hacer según el estado del
personaje y del entorno (atacar, lanzar hechizos con objetivo, tomar pociones, meditar, escapar). Es una
macro del lado del cliente, como los auto-buffs y la auto-poción (`PLAN_JUGABILIDAD.md`): manda los
mismos paquetes que el jugador a mano (C_MoveChar, C_Attack, C_UseSkill, C_UseItem). Pensado para el
servidor local de pruebas; en un servidor público, automatizar el juego suele ir contra sus reglas.

## 1. El programa

Un archivo de texto por programa, en `planes/` junto a `settings.ini` (`planes/piso51_turn.txt`). El
personaje recuerda cuál usa (`modo_auto=Nombre:piso51_turn`). Una instrucción por línea; `#` comenta.

**Bilingüe:** cada palabra clave tiene su forma en español y en inglés, y se pueden mezclar
(`si mp < 10% then stop`). Sin distinguir mayúsculas ni tildes. Los nombres de hechizos e ítems son los
del juego, en inglés, comparados sin espacios ni mayúsculas (`Turn Undead` = `TurnUndead` = `turn undead`);
un nombre que no existe es un error al cargar, con la línea.

### Encabezado

| Español | English | Qué hace |
|---|---|---|
| `plan "Nombre"` | `plan "Name"` | Nombre que se muestra |
| `mapa 151` | `map 151` | Mapa donde corre (también por nombre: `mapa "Tower of Insolence 51F"`); fuera de él, el programa espera |
| `arma "Sword of Red Desire"` | `weapon "..."` | La equipa al empezar si no la tiene puesta |
| `pociones: siempre \| atacando \| nunca` | `potions: always \| attacking \| never` | Cuándo actúa la auto-poción mientras corre el programa |

### Moverse (`moverse` / `move`)

| Español | English | Qué hace |
|---|---|---|
| `moverse recorrer` | `move wander` | Elige puntos al azar transitables y camina hacia ellos (A*) |
| `moverse puntos (32800,32800) (32810,32790) ...` | `move waypoints ...` | Va de un punto al siguiente, en bucle (`... una vez` / `once`: se detiene al final) |
| `moverse seguir a "Nombre"` | `move follow "Name"` | Se mantiene a `distancia 2` (`distance`) del personaje; si no se lo ve, espera |
| `moverse quieto` | `move stay` | No camina (solo las reglas) |

### Reglas (`si ... entonces ...` / `when ... then ...`)

Se evalúan **de arriba hacia abajo, una vez por frame**; corre la primera cuya condición se cumple. Si
ninguna, se mueve según `moverse`. Una regla con `hasta` / `until` **retiene** el control hasta que su
condición de salida se cumpla (o hasta que una regla de más arriba la interrumpa: por eso el escape va
primero).

```
si <condición> [y <condición> ...] entonces <acción> [; <acción> ...] [hasta <condición>]
when <condition> [and <condition> ...] then <action> [; <action> ...] [until <condition>]
```

`o` / `or` y `no` / `not` también valen. `cada 60s` / `every 60s` limita una regla a una vez por período.

### Condiciones

| Español | English | Verdadera si |
|---|---|---|
| `hp < 10%`, `mp >= 90%`, `hp < 300` | igual | Vida o maná propios, en % o absolutos |
| `seguro [a 8]` | `safe [within 8]` | Ningún monstruo a 8 celdas o menos (8 por defecto) |
| `sin jugadores [a 15]` | `no players [within 15]` | Ningún otro personaje a la vista (o a esa distancia) |
| `monstruos >= 4 a 2` | `monsters >= 4 within 2` | Al menos 4 monstruos a 2 celdas: "rodeado" |
| `atacando` | `attacking` | Hay un objetivo de ataque vivo |
| `sin pociones` | `no potions` | No queda ninguna poción de curación (`elegir_pocion`) |
| `sin "Scroll of Escape"` | `no "..."` | No queda ese ítem |
| `tiene "Immune to Harm"` | `has "..."` | Ese efecto está activo (los de `EfectoActivo`) |
| `"Nombre" hp < 50%` | `"Name" hp < 50%` | La vida de otro personaje, si el servidor la manda (grupo: S_HpMeter) |
| `objetivo hp < 30%` | `target hp < 30%` | La del objetivo actual |
| `en mapa 151` | `on map 151` | El mapa actual |

### Acciones

| Español | English | Qué hace |
|---|---|---|
| `atacar [más cercano]` | `attack [nearest]` | Elige el monstruo más cercano (o sigue con el actual) y lo ataca con el arma |
| `lanzar "Hechizo" [a <objetivo>]` | `cast "Spell" [on <target>]` | C_UseSkill; sin objetivo, el que pide el hechizo |
| `usar "Ítem" [a <objetivo>]` | `use "Item" [on <target>]` | C_UseItem (pociones, pergaminos) |
| `equipar "Arma"` | `equip "..."` | La equipa |
| `detener` | `stop` | Deja de caminar y de atacar |
| `esperar` | `wait` | No hace nada (con `hasta`) |
| `terminar` | `end` | Apaga el modo auto |
| `ir a (x,y)` | `go to (x,y)` | Camina hasta ahí y sigue con las reglas |

**Objetivos** (`a` / `on`): `mí` / `me`, `objetivo` / `target`, `más cercano` / `nearest` (monstruo),
`"Nombre"` (un personaje por su nombre), `aliado más herido` / `most hurt ally` (del grupo).

### Siempre activo (no se programa)

- Muerto, inmóvil o desconectado: se pausa; al morir, se apaga.
- Cualquier tecla o clic del jugador pausa el modo (la barra muestra "Auto: paused"); se retoma con el
  botón o la tecla.
- Un hechizo que el servidor rechaza ("falló", MP insuficiente) no se reintenta antes de 10 s, como los
  auto-buffs. Un monstruo con el que Turn Undead falla tres veces se saltea para ese hechizo (el cliente
  no sabe cuáles son no-muertos).

## 2. Los ejemplos

**Turn Undead en el piso 51** (Turn Undead mata o falla; el escape va primero porque interrumpe todo):

```
plan "Piso 51 - Turn Undead"
mapa 151
pociones: siempre
moverse recorrer

si hp < 10% o sin pociones entonces usar "Scroll of Escape"; terminar
si mp < 10% entonces detener; lanzar "Meditate" hasta mp >= 90%
si no seguro entonces lanzar "Turn Undead" a más cercano
```

**Elfo con espada y Sun Burst** (en inglés, para mostrar que vale igual):

```
plan "Elf 51F"
map 151
weapon "Sword of Red Desire"
potions: attacking
move wander

when hp < 10% or no potions then use "Scroll of Escape"; end
when mp < 20% and safe then cast "Blood to Soul"; cast "Greater Heal" on me
when attacking and mp >= 30% every 4s then cast "Sun Burst" on target
when not safe then attack nearest
```

**Mago en el piso 11: juntar, Immune to Harm y Fire Storm:**

```
plan "Mago 11F"
mapa 111
moverse recorrer

si hp < 15% entonces usar "Scroll of Escape"; terminar
si monstruos >= 4 a 2 y no tiene "Immune to Harm" entonces detener; lanzar "Immune to Harm" a mí
si monstruos >= 4 a 2 entonces detener; lanzar "Fire Storm" hasta seguro a 3
si mp < 20% y seguro entonces lanzar "Meditate" hasta mp >= 90%
```

(Moverse por el mapa sin atacar hace que los monstruos sigan al mago: así se juntan.)

**Seguir a otro y curarlo:**

```
plan "Sanador"
moverse seguir a "Andromeda" distancia 2

si "Andromeda" hp < 40% entonces lanzar "Full Heal" a "Andromeda"
si "Andromeda" hp < 70% entonces lanzar "Greater Heal" a "Andromeda"
si sin jugadores a 30 entonces esperar
```

## 3. Cómo encaja en el código

- **`mundo/modo_auto.{h,cpp}`** (sin estado de la app, para los tests): el *parser* (texto → `Programa`
  con sus reglas, errores con número de línea) y `decidir(programa, entorno, ahora) → Accion`. El
  `Entorno` se arma con lo que ya existe: `EstadoJugador` (HP, MP, efectos), las distancias a monstruos y
  personajes (`tipo_objeto`), el inventario (`elegir_pocion`) y el objetivo.
- **`mundo/ruta.{h,cpp}`**: A* sobre la transitabilidad de los segmentos del mapa (hoy el cliente
  camina un paso por clic; no hay búsqueda de ruta). Lo necesitan `recorrer`, `puntos`, `seguir a` e `ir a`.
- **`app/modo_auto_ui.cpp`**: ejecuta la acción con lo que usa el jugador (`pedir_hechizo`, `usar_item`, el
  ataque y el paso de `mover_jugador`), el botón "Auto" de la barra con el nombre del programa, y la línea
  de estado (regla que corre, o el error al cargar).
- **Hechizos e ítems por nombre**: `TablaHechizos` y los nombres del inventario (`$N` traducido).

## 4. Fases

1. **Parser y evaluador**, con tests: las dos lenguas dan el mismo `Programa`; cada condición; la
   prioridad y el `hasta`. Sin moverse todavía: `moverse quieto` y reglas.
2. **Ruta (A\*)** y `recorrer` / `puntos` / `ir a`, con tests sobre un mapa de prueba.
3. **Ejecutar en el juego**: atacar, lanzar con objetivo, usar ítems, el botón y la pausa. Prueba en vivo con
   los cuatro ejemplos.
4. **`seguir a`**, el grupo (`aliado más herido`, la vida de otros) y el editor (cargar y ver el programa
   desde Opciones).

## 5. Pendiente de verificar

- Los nombres que usa el cliente para los hechizos (los de la tabla del cliente, no los del servidor:
  `TurnUndead` en el servidor puede ser `Turn Undead` en el cliente).
- Immune to Harm gasta dos gemas mágicas (ítem 40318): conviene `sin "Magic Gem"` en las reglas.
- La vida de otros personajes solo llega si están en el grupo (S_HpMeter); fuera del grupo, `"Nombre" hp`
  es desconocido y la condición es falsa.
- Recorrer con puertas, portales o agua: A* solo conoce la transitabilidad; empezar en pisos abiertos.

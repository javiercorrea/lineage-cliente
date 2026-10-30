# Plan: castillos y casas (agit)

Objetivo: jugar con el cliente lo que el servidor L1J-JP 3.63c ya tiene de casas de clan y de
castillos:
- subastar y comprar una casa, entrar y usar su mayordomo;
- declarar guerras de clanes;
- asediar un castillo: puertas, torre y corona;
- administrarlo como señor: impuestos, fondos y fecha del próximo asedio.

Todo el comportamiento lo decide el servidor. El cliente tiene que mostrar los paquetes que hoy
ignora, mandar los que le faltan y verificar en vivo los flujos que pasan por HTML.

Fuentes:
- servidor: `servidor_local/l1j-jp/src/jp/l1j/server`, en `packets/server/S_*.java`, `packets/client/C_*.java`,
  `C_NpcAction.java`, `model/instance/L1{Crown,Tower,Door,Housekeeper,AuctionBoard}Instance.java`
  y `controller/timer/WarTimeController.java`;
- base de datos `l1jdb_363`: tablas `castles`, `houses`, `auction_houses` y `clans`.

---

## 1. Qué hay y qué falta

**Ya funciona en el cliente** (no se rehace):
- **Puertas:** `S_Door`, `C_Door`, estados 28 abierta y 29 cerrada, y bloqueo de celdas. Las puertas
  de las casas y los portones de los castillos son las mismas `L1DoorInstance`.
- **Clanes:** crear, unirse, página del clan y emblema. Hoy hay un clan, `Prueberos`: Charla
  (príncipe, nivel 81) es la líder y Prueba (príncipe, nivel 1) es miembro.
- **Visor HTML de NPCs:** acciones, `<var>` y botones con imagen. La mayor parte de lo que hacen el
  mayordomo, el chambelán y los guardias son páginas HTML con acciones.
- **Diálogo de cantidad** (`S_InputAmount` / `C_Amount`): el decodificador ya reconoce "agapply" y
  "agsell", pero no se probó con una subasta.
- **`S_MessageYN`** (preguntas sí/no).

**Falta: paquetes del servidor que hoy se ignoran**

| Opcode | Paquete | Formato | Quién lo manda |
|---|---|---|---|
| 24 | `S_HouseList` | d tablón, h n, n × (d casa, s nombre, h área, c mes, c día, d precio) | clic en el tablón de subastas |
| 44 | `S_HouseMap` | d NPC, d casa | acción "map" del tablón de subastas |
| 123 | `S_War` | c tipo, s clan1, s clan2 (1 declara, 2 se rinde, 3 termina, 4 gana, 6/7 alianza, 8 "en guerra con") | guerras de clanes; al entrar al juego |
| 66 | `S_CastleMaster` | c castillo (1–8; 0 lo quita), d personaje | señor del castillo: la corona sobre el nombre |
| 72 | `S_TaxRate` | d personaje, c mínimo 10, c máximo 50 | acción "tax" del chambelán |
| 203 | `S_Deposit` | d personaje | acción "cdeposit": poner fondos |
| 224 | `S_Drawal` | d personaje, d fondos | acción "withdrawal": sacar fondos |
| 150 | `S_WarTime` | h 6, s zona horaria, 3 × c, 6 × (d tiempo, c) | elegir la fecha del asedio |
| 126 | `S_HireSoldier` | — | mercenarios: el servidor no los implementa ("employ" y "arrange" vacíos). **Fuera del plan.** |

**Falta: subtipos de `S_PacketBox` (40)**
- 0 empieza la guerra, 1 termina, 2 sigue en curso: c castillo, h 0.
- 3 iniciativa y 4 ocupación del castillo.

**Falta: paquetes del cliente**

| Opcode | Paquete | Formato | Uso |
|---|---|---|---|
| 235 | `C_War` | c tipo (0 declara, 2 se rinde, 3 termina), s clan | `/war`, `/surrender` y fin de la guerra |
| 200 | `C_TaxRate` | d personaje, c tasa | ventana de impuestos |
| 35 | `C_Deposit` | d personaje, d cantidad | poner fondos |
| 192 | `C_Drawal` | d personaje, d cantidad | sacar fondos |
| 4 | `C_ChangeWarTime` | sin datos | pide `S_WarTime` (el servidor no implementa elegir la fecha: solo la muestra) |

**Un hallazgo que cambia el tablón actual.** El tablón de subastas es un NPC `L1AuctionBoard` con
el **gfx 1544, el mismo que los tablones de los pueblos**. `es_tablon` ya le manda `C_Board` y
`C_Board` llama a `onAction`, que en la subasta responde `S_HouseList` en lugar de `S_Board`. Por eso:
- no hay que distinguir el tablón al hacer clic;
- el cliente reacciona al paquete que llegue, y hoy `S_HouseList` se pierde;
- hay cuatro tablones de subastas: Giran (33421, 32823), Heine (33585, 33235), Aden
  (33959, 33253) y Gludin (32611, 32775).

---

## 2. Entorno de prueba

Primero se prepara el entorno, porque sin datos no se puede verificar nada.

1. **Dos clanes rivales.**
   - Charla sigue al frente de `Prueberos`.
   - Prueba (príncipe, cuenta `prueba`) sale de ese clan con `/leaveclan` y crea "Rivales" con
     `/clan`.
   - Prueba sube a nivel 30 con `.level` desde Aprendiz, que es GM. `C_War` exige príncipe de nivel
     15 o más, y el asedio, 25 o más.
   - Al terminar, Prueba vuelve a `Prueberos` y a su nivel, o queda documentado si conviene dejarlo
     así.
2. **Forzar un asedio.** No hay comando GM: `WarTimeController` lee `castles.war_time`, que hoy está
   en `0000-00-00` en los 8 castillos.
   - Un script reproducible, `servidor_local/pruebas/asedio_kent.sql`, pone la guerra de Kent
     (`id = 1`) en `NOW()`.
   - Duración: `WarTime=2h` en `config/altsettings.properties.xml`.
   - Otro script deja todo como estaba.
   - Ni `corregir_db.sql` ni `instalar.sh` cambian: el asedio forzado es solo para pruebas.
3. **Casas.** Las 129 casas están a la venta (`is_on_sale = 1`, precio 100.000, cierre el
   2026-11-02).
   - Para probar el mayordomo hace falta ganar una subasta: el script adelanta el `deadline` de una
     casa y `AuctionTimeController` la adjudica.
   - Otra opción es asignar `clans.house_id` directamente. Las dos van en el mismo script de
     pruebas, que también las deshace.
4. **Automatización.**
   - Se reusan `--hablar`, `--npc-accion`, `--cantidad`, `--pulsar` y `--decir`.
   - Se agrega solo lo que falte, por ejemplo `--atacar=<nombre>` para la torre, el portón y la
     corona, si `--arrastrar` con Ctrl no alcanza.

---

## 3. Fases

Cada fase se cierra con:
- tests de bytes en `tests/test_proto.cpp`;
- una prueba en vivo contra el servidor local;
- una captura en `docs/capturas/`;
- su sección en `ENTREGABLES.md`;
- commit y push.

### Fase 1: protocolo (sin interfaz)

- **Opcodes y eventos:** los de la tabla en `Srv`, `Cli` y `opcodes.cpp`. Un `Ev` por tema, con
  prefijos en `v[]`, como el resto:
  - `Ev::CASAS`, multi-evento como `S_Board`;
  - `Ev::CASA_MAPA`, `Ev::GUERRA`, `Ev::SENOR_CASTILLO`, `Ev::CASTILLO_FONDOS` (impuesto, poner y
    sacar) y `Ev::FECHA_ASEDIO`.
- **`caja`:** los subtipos 0–4 de guerra.
- **`comandos.h`:** `guerra`, `tasa`, `depositar_castillo`, `retirar_castillo` y `fecha_asedio`.
- **`S_WarTime`:** la fecha se codifica en unidades de 182 minutos desde 1997-01-01 17:00, menos
  1200 minutos. Convertirlo a fecha real y comprobarlo con un test con bytes armados desde la
  fórmula del servidor.

### Fase 2: casas

- **Tablón de subastas** (`casas_ui.cpp`): ventana con el estilo del tablón, simple u original según
  `ui_simple`.
  - La lista de `S_HouseList`: nombre, área, cierre (mes y día) y precio actual.
  - Clic en una casa: `C_NPCAction "select <id>"`. El servidor manda la página `agsel` con 9 datos
    (nombre, ubicación, área, dueño anterior, postor actual, precio y cierre), que se muestra en el
    visor HTML que ya existe.
  - Desde ahí, "map" y "apply".
- **Mapa de la casa** (`S_HouseMap`): el original muestra una imagen con la ubicación.
  - Primero buscar esa imagen en `Sprite.pak` con las hojas de miniaturas.
  - Si no está, mostrar la ubicación de `houses.location` con una marca en el mapa, si ya existe un
    minimapa, o como texto.
- **Pujar:** "apply" responde `S_InputAmount` "agapply". Verificar que el diálogo de cantidad
  devuelve `agapply <casa>` y que el servidor descuenta la adena y guarda la puja. El servidor exige
  líder del clan, príncipe, nivel 15 o más y que el clan no tenga ya casa.
- **Mayordomo** (`L1HousekeeperInstance`): todo es HTML. Verificar cada acción:
  - "pay"/"payfee": impuesto de la casa, página `agpay`;
  - "name": `S_MessageYN` 512 pide un nombre. **Investigar** cómo devuelve el texto el original
    (¿`C_Attr` con una cadena?);
  - "rem": quitar los muebles;
  - "tel0…": teletransportes;
  - "upgrade": sótano;
  - "open"/"close": la puerta;
  - "expel": echar a los de afuera.
- **Puertas de la casa:** solo el clan dueño las abre (`C_Door`). Verificar con un miembro y con
  alguien de afuera.
- **Vender la casa:** "agsell" con el diálogo de cantidad.

### Fase 3: castillos en paz

- **Corona del señor** (`S_CastleMaster`): guardar el castillo de cada personaje y dibujar una corona
  junto al nombre.
  - Buscar el ícono del original en `Sprite.pak`.
  - Con `ui_simple`, un dibujo propio y simple, sin el gráfico del original (lo pidió el usuario
    para la interfaz simple).
- **Guardias:** "askwartime" responde una página con la fecha del próximo asedio (`htmldata`). Ya
  debería verse: verificar.
- **Porteros:** "openigate" y "closeigate" abren y cierran los portones (puertas ya soportadas).
- **Chambelán** (`castillo_ui.cpp`):
  - "inex": el servidor responde con un mensaje de chat, sin código nuevo;
  - "tax" → `S_TaxRate`: un selector de 10 a 50 % y `C_TaxRate`;
  - "cdeposit" → `S_Deposit` y "withdrawal" → `S_Drawal`: el diálogo de cantidad que ya existe, con
    otro destino (`C_Deposit` y `C_Drawal` en lugar de `C_Amount`); en `S_Drawal`, máximo = fondos;
  - fecha del asedio: `C_ChangeWarTime` → `S_WarTime`, que se muestra como una lista de 6 horarios.
    Elegir uno no llega al servidor: no hay paquete que lo reciba.
- **Especialistas de armas y armaduras:** "encw" y "enca" son HTML. Verificar.

### Fase 4: guerras y asedio

- **Guerra de clanes:**
  - comandos `/war <clan>`, `/surrender <clan>` y `/ceasefire <clan>` → `C_War` 0, 2 y 3; revisar
    los nombres de los comandos del original;
  - `S_War` va al chat con los textos del servidor (`$` del `string-e.tbl` si los hay);
  - la página del clan muestra "en guerra con X" (`S_War` 8 al entrar).
- **Avisos del asedio** (`S_PacketBox` 0–4): mensaje en el chat y un aviso fijo arriba mientras dura
  ("Siege of Kent Castle in progress").
- **Portones con vida:** `S_DoActionGFX` 32–36 son los daños y 37 la destrucción.
  - Revisar que el cliente dibuje esas acciones del gfx de la puerta y no solo la 28 y la 29.
  - Destruido, el portón se puede pasar: llega `S_Door` y verificarlo.
  - Atacarlo con Ctrl+clic, que ya ataca a cualquier objeto: verificar con un portón.
- **Torre de guardia** (`L1TowerInstance`): grietas 32–34 y destrucción 35, igual que los portones.
- **Corona** (`L1CrownInstance`): aparece cuando cae la torre. El príncipe líder, sin transformar y a
  una celda, la "ataca" (`C_Attack` → `onAction`) y su clan toma el castillo. Después llegan
  `S_CastleMaster` y `S_PacketBox` 4.
  - Revisar que un clic sobre la corona mande `C_Attack` y no `C_NPCTalk`, igual que con el tablón
    (`es_tablon`).
- **Prueba de punta a punta:**
  1. Kent con la guerra forzada.
  2. Ningún castillo tiene dueño, así que la corona se toma sin declarar guerra.
  3. Charla rompe el portón y la torre y toma la corona: `Prueberos` queda como señor de Kent.
  4. Aparece la corona sobre su nombre y se prueba el chambelán.
  5. Segundo asedio: Prueba, con "Rivales", declara la guerra a `Prueberos` (`C_War` 0 con el
     nombre del **clan** dueño, dentro del horario y desde fuera del castillo) y la toma.
  6. Se dejan los datos como estaban.

### Fase 5: documentación

- `ENTREGABLES.md`, con una sección por fase.
- `OPCODES_PENDIENTES.md`: sacar los opcodes hechos.
- `DESCUBRIMIENTOS.md`: el gfx compartido de los tablones, la fecha de `S_WarTime` y cómo se toma
  la corona.

---

## 4. Fuera de este plan

- **Mercenarios** (`C_HireSoldier` / `S_HireSoldier`): el servidor no hace nada con ellos.
- **Catapultas, la Fortaleza de Diad (castillo 8) y las reglas de la zona de guerra:** las reglas
  (PK sin castigo, etc.) las aplica el servidor. El cliente solo muestra lo que llega.
- **Muebles de la casa:** son ítems que se colocan. Si funcionan con lo que ya hay, se verifica; si
  no, queda pendiente.

## 5. Riesgos y cosas por investigar

1. **Gráficos del original:** la corona sobre el nombre, el mapa de las casas y la ventana de
   impuestos. Puede que no estén en `Sprite.pak`. En ese caso se dibujan propios; con la UI simple
   siempre propios.
2. **Nombre de la casa:** cómo devuelve el original el texto (`S_MessageYN` 512 con una caja de
   texto).
3. **`S_WarTime`:** el "−1200 minutos de error" del servidor puede dar fechas corridas. Comparar con
   lo que muestra el guardia (`askwartime`).
4. **Acciones de daño de puertas y torres:** el sprite debe tener las acciones 32–37. Si alguna
   falta, se queda en la última que tenga.
5. **El asedio forzado por SQL:** confirmar que `WarTimeController` lo lee sin reiniciar el
   servidor. Si no, reiniciarlo, sin tocar a Hermione ni a Mago.

## 6. Orden y tamaño

| Fase | Contenido | Tamaño |
|---|---|---|
| 1 | Protocolo | 1 sesión |
| 2 | Casas | 1–2 sesiones, según el mapa y el nombre |
| 3 | Castillos en paz | 1 sesión |
| 4 | Guerras y asedio | 2 sesiones: la prueba en vivo es lo más largo |
| 5 | Documentación | Va con cada fase |

Se puede empezar por la fase 1 junto con el tablón de subastas: es lo único que hoy se ve roto (un
clic en el tablón de subastas no muestra nada).

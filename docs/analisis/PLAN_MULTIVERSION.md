# Plan: varias versiones de protocolo sin `if (versión)` en los decodificadores

**Estado:** fases 1 y 2 hechas; fase 3 avanzada (rama `agente/multiversion`). `s3ep1` juega contra
l1j-en/classic en local (`servidor_classic/`): login, **crear personaje**, entrar, moverse, chat,
objetos, render, **atacar, lanzar hechizos y diálogo de NPC**, **inventario, usar/equipar, borrar,
tienda (comprar) y almacén (guardar/retirar)**, **grupo (invitar/aceptar/salir, ventana y chat),
clan (crear y ver), título y emblema**, **intercambio (ofrecer/aceptar entre dos clientes) y
mascotas (invocar, menú de control y elegir objetivo)**, **almacén retirar (S_RetrieveList +
C_Result), combate (atacar, acertar/fallar y matar, con el campo de golpe parametrizado) y skills
(aprender con S_AddSkill y lanzar con C_UseSkill)**.

Objetivo: que el cliente hable con servidores de distintas versiones (hoy `jp363` y `tw380`; a
futuro `s3ep1`, el US 2009 de l1j-en/classic, u otras) **sin que las funciones que arman o leen
paquetes tengan un `if` por versión**. Esas funciones deben quedar agnósticas; toda la variación
vive en datos por versión.

## 1. Estado actual

- **Los números de opcode ya son datos por versión:** `TablaOpcodes` con una `cargar(...)` por
  versión (`src/proto/opcodes.cpp`). Esto es el modelo a seguir, no a cambiar.
- **El anti-patrón a reemplazar:** el bool `v380` en `TablaOpcodes` y el `Lector::vida()` que hace
  `v380 ? d() : h()`. Es un `if` de versión; hoy está en un helper (no en el decodificador, que ya
  llama `l.vida()`), pero no escala: con `s3ep1` habría que sumar `v2009`, `vXXX`, y multiplicar los
  ifs.

## 2. La idea central

Toda diferencia entre versiones se expresa como **un dato en un perfil por versión**, no como una
rama. Hay dos lugares de datos y una sola regla:

1. **Números de cable** → la tabla de opcodes (ya existe).
2. **Formato de los cuerpos** → un `PerfilProtocolo`: una struct de **parámetros numéricos**
   (anchos de campo, cantidades de bytes opcionales, variantes) que se carga una vez por versión,
   junto a la tabla.

Regla: los decodificadores (`decodificar.cpp`) y los armadores (`comandos.h`) solo llaman **helpers
tipados parametrizados por el perfil**. Nunca miran la versión.

### Cómo se traduce cada tipo de diferencia

| Diferencia | Hoy / ejemplo | Solución sin `if` |
|---|---|---|
| **Número de opcode** | `S_CharPack` = 3 (jp) / 1 (us) | la tabla de opcodes (ya) |
| **Ancho de un campo** | vida 2 bytes (jp) / 4 (tw) | `Lector::leer(perfil.bytes_vida)`: lee N bytes; `vida()` pasa a ser `leer(p.bytes_vida)`, sin `?:` |
| **Campo extra/opcional** | `S_OwnCharPack` del US mete un byte `currentWeapon` de más | `l.saltar(perfil.bytes_arma_charpack)` (0 si no está): el decodificador siempre hace la misma llamada |
| **Semilla y opcode inicial** | init 161 (jp) / 150 (tw) / 125 (us) | `cable_init` en la tabla (ya); la semilla fija del US no importa (el cliente la lee del paquete) |
| **Estructura irreconciliable** | un paquete con campos reordenados que ningún parámetro arregla | **excepción:** un puntero a función por opcode en la tabla de esa versión. La versión elige la función entera; adentro no hay `if`. Se documenta caso por caso |

El `PerfilProtocolo` es parte de `TablaOpcodes` (que ya es "lo de esta versión, cargado una vez"), y
el `Lector` guarda una referencia a él (como hoy guarda `v380`). Los armadores (`Escritor`) toman el
mismo perfil para los cuerpos que difieran al enviar.

**Lo que NO se hace** (sería sobre-ingeniería): un lenguaje de descripción de paquetes (schema/DSL)
que arme los campos por reflexión. Los cuerpos se siguen leyendo con llamadas explícitas; solo que
los números que varían salen del perfil.

## 3. Fases

Cada fase compila, pasa los tests y no cambia el comportamiento de `jp363` ni `tw380`.

1. **Perfil, reemplazando `v380`** (refactor, sin funcionalidad nueva):
   - `struct PerfilProtocolo { u8 bytes_vida = 2; ... };` en `opcodes.h`, dentro de `TablaOpcodes`.
   - `vida()` pasa a `leer(perfil.bytes_vida)` (lee N bytes LE a un `i32`), sin el `?:`.
   - `jp363` deja `bytes_vida = 2`, `tw380` lo pone en 4. Se borra el bool `v380`.
   - Test: el de `tw380` (vida en 4 bytes) y el de `jp363` (2 bytes) ya existen; deben seguir verdes.
2. **Tabla `s3ep1` y conexión** (US 2009, l1j-en/classic):
   - tabla de opcodes desde el `Opcodes.java` de classic (203 opcodes), por nombre, como se hizo con `tw380`;
   - `cable_init = 125` (0x7D);
   - el perfil de esa versión con lo que ya se sepa.
   - Cifrado: no se toca, es el mismo XOR (verificado: mismas máscaras y misma derivación de clave).
   - **Hecho.** Login y lista de personajes OK contra classic en local. El `docker-compose` de classic
     no sirvió (la VM de Rancher no alcanza Docker Hub), así que se levanta nativo con ant + la
     MariaDB de Homebrew en `servidor_classic/` (igual que `servidor_local`), puerto 2000.
3. **Recorrer los cuerpos con la traza** (`traza on` del control en vivo):
   - por cada cuerpo que difiera, **agregar un parámetro al perfil** y cambiar la llamada del
     decodificador a la versión parametrizada (p.ej. el byte extra de `S_OwnCharPack`);
   - solo si un paquete es irreconciliable por parámetros, un puntero de función por opcode en esa
     versión (la excepción de la tabla de §2), documentado.
   - Verificar en vivo con el control (login, entrar, mover, chat, NPC) y capturas.
   - **Verificado en vivo contra classic** (personaje sembrado en la base): entrar al mundo (mapa 4),
     el estado propio (posición, HP/MP/nivel), moverse, chat (con eco), objetos cercanos con nombres
     de NPC correctos y el render. Sin opcodes "?": los cuerpos del mundo coinciden lo suficiente (los
     decodificadores son tolerantes: `poner` lee lo que necesita e ignora el resto). Capturas:
     `docs/capturas/classic_mundo.png`, `classic_login.png`.
   - **Jugabilidad verificada** (personaje sembrado, GM con el prefijo `.`): **atacar** (melee: una
     rana murió de un golpe; `C_Attack` y `S_AttackPacket` decodifican), **hechizos** (un mago lanza
     Energy Bolt: `C_UseSkill`, `S_AddSkill`, `S_SkillSound` y el daño), y **diálogo de NPC** (la
     página HTML con sus vínculos renderiza). Capturas `classic_skill.png`, `classic_npc.png`.
   - **Notas de las pruebas** (no son del protocolo): el camino del cliente es ingenuo y se traba con
     edificios, así que para hablar/atacar conviene teletransportarse adyacente (`.tele x y mapid`,
     que pide los 3 args); en la base de classic la columna `Class` guarda el gfx del classId (0
     príncipe, 734 mago), no 0–6.
   - **Crear personaje: arreglado.** No era el protocolo: el cuerpo de `C_CreateChar` y las tablas de
     base/puntos flotantes coinciden con el US 2009. El bug era del cliente (nunca se había probado
     crear): la asignación automática volcaba todos los puntos en Fuerza (Str 21) y superaba el
     máximo absoluto por stat (Str 20). Se agregó la tabla `MAX` (los `maxFixedStats` del servidor,
     estándar del juego) en `pantalla_crear.cpp` y tanto la asignación automática como el +/- la
     respetan. Verificado: un mago creado desde el cliente (stats suman 75, dentro de los topes) y
     entró al mundo.
   - **Ítems, tienda y almacén: una divergencia de cuerpo, parametrizada.** El cuerpo por ítem de
     `S_AddItem`/`S_InvList` difiere: el 3.63/3.80 ponen tras el id un campo `h` de catalizador
     mágico (2 bytes) y cierran con una cola fija de 11 bytes (`c 10, d 0, d 0, h 0`); el US 2009 no
     tiene ni uno ni otra. Se agregaron al perfil `bytes_catalizador_item` y `bytes_cola_item` (2 y
     11 por defecto, 0 y 0 en `s3ep1`) y `leer_item` pasó de `l.h()` / `l.saltar(1+4+4+2)` a
     `l.saltar(perfil.bytes_catalizador_item)` / `l.saltar(perfil.bytes_cola_item)`. Sin esto el
     inventario se desalineaba ítem a ítem. Verificado en vivo: inventario con nombres, gfx, encanto
     y cantidades correctos; **usar/equipar** (poción 20→19, daga "(Armed)"), **borrar** (quita el
     ítem), **tienda** (la lista de compra de un mercader con nombres y precios, y la adena propia) y
     **almacén** (la lista para guardar). Capturas `classic_tienda.png`, `classic_almacen.png`.
   - **Nota (no es del protocolo):** en `s3ep1` el `C_DeleteInventoryItem` solo lleva el id (lee
     `readD()` y borra el montón entero con `removeItem(item, item.getCount())`); la cantidad que
     agrega el cliente la ignora el servidor (los 4 bytes de más son inocuos por el relleno a 4). Así
     que en classic "borrar" vacía toda la pila, no la cantidad pedida.
   - **Pregunta sí/no (S_MessageYN) y su respuesta (C_Attr): otra divergencia parametrizada.** El
     3.63/3.80 anteponen un `h 0` y un `d` con el número de pregunta, y responden con
     número+tipo+respuesta (la respuesta en 2 bytes); el US 2009 manda solo el tipo (que ya elige la
     pregunta) y responde con 1 byte. Se agregaron al perfil `bytes_pre_pregunta` y
     `bytes_numero_pregunta` (2 y 4 por defecto, 0 y 0 en `s3ep1`): `pregunta` lee
     `l.saltar(pre)` + `l.leer(numero)` + `h tipo` sin `if`, y `responder` elige forma larga/corta
     según si el perfil trae número. Era lo que faltaba para **aceptar invitaciones de grupo** (y
     sirve a todas las ventanas sí/no: unirse a un clan, confirmar intercambio, aceptar/rendir una
     guerra, teletransportes...). Test de bytes de las dos formas en `tests/test_proto.cpp`.
   - **Social verificado en vivo** (contra classic): **título** (`/title`, `S_CharTitle`, se dibuja
     sobre el nombre), **clan** (`/clan` lo crea; `/pledge` muestra la lista `pledgeM`), **emblema**
     (`/emblem` sube 384 bytes y `S_Emblem` se decodifica y dibuja junto al nombre) y **grupo**
     (invitar con `/invite`, aceptar el sí/no, `/party` abre la ventana HTML con líder y miembros,
     chat de grupo con `#` en los dos sentidos, salir con `/leaveparty`). Capturas
     `classic_titulo.png`, `classic_clan.png`, `classic_emblema2.png`, `classic_grupo_B.png`.
   - **Nota (no es del protocolo):** en `s3ep1` la membresía del grupo se comunica con `S_HpMeter`
     (barras de vida) y la ventana `S_Party` es una página HTML a pedido (`/party`); no hay el
     `S_PacketBox` de grupo (sub 104/105/106) que llena el panel nativo de miembros del cliente, así
     que ese panel lateral queda vacío aunque el grupo exista y funcione. Poblarlo desde la página
     HTML sería una mejora del cliente, no del protocolo.
   - **Intercambio: sin cambios, lo cubrió el fix del sí/no.** El flujo completo entre dos clientes
     anda sin tocar código: A `/trade` frente a frente (FaceToFace exige adyacencia y que ambos se
     miren) dispara `S_Message_YN(252)`; B acepta con la forma corta ya parametrizada y ambos abren
     la ventana (`S_Trade`); A ofrece ítems (`C_TradeAddItem` = `d ítem, d cantidad` ↔ `S_TradeAddItem`
     con lado/gfx/nombre/bendición) y, con los dos `C_TradeOK`, los ítems se transfieren de verdad
     (A recibe `DELETEINVENTORYITEM`, B `ADDITEM`, ambos `TRADESTATUS`). Capturas `classic_trade_B.png`,
     `classic_trade_B_recibido.png`. **Detalle del server classic:** los GM entran invisibles (comando
     `invis` con `run_on_login=1`); como el trade exige visibilidad mutua, el personaje GM hace `.invis`
     para verse antes de comerciar.
   - **Mascotas: la mascota se dibuja como personaje y el menú es HTML (como el grupo).** `S_PetPack`
     es un `S_CharPack` (opcode CHARPACK), así que la mascota invocada se renderiza en el mapa sin
     nada especial. El **menú de control** (`S_PetMenuPacket`) es en realidad un `SHOWHTML` con la
     página `anicom`/`moncom` (atacar, modo agresivo/defensivo, quieto, desplegar, alerta, recoger,
     despedir, poner nombre), no el binario `S_PetCtrlMenu` del 3.63/3.80; se abre al hablarle a la
     mascota (`C_NPCTalk` → `onTalkAction`). La acción "atacar" (`attackchr`) devuelve `S_SelectTarget`
     (opcode 61, `d id`), que el cliente decodifica y usa para entrar en modo "elegir objetivo". Por
     eso, igual que con el grupo, el **panel lateral nativo de mascotas** (`App::mascotas`, que se
     llena con `S_PetCtrlMenu`/PETCTRL) queda vacío en `s3ep1`: classic controla la mascota por HTML.
     Prueba contra classic con `.createpet Wolf 10` + `.create 41160` (Calling Flute) y `usar` el
     collar. Captura `classic_pet_menu.png`.
   - **Almacén — retirar: sin cambios de protocolo.** El depósito y el retiro usan `C_Result` (tipo 2
     guardar, 3 retirar); la lista de lo guardado llega por `S_RetrieveList` (SHOWRETRIEVELIST). Ciclo
     completo verificado con Storage^Karim: `accion deposit` → `C_Result` tipo 2; `accion retrieve` →
     `S_RetrieveList` abre la ventana con los ítems; elegir y OK → `C_Result` tipo 3 (con el id que vino
     en la lista) y el ítem vuelve al inventario (cobra 30 adena por ítem). Captura `classic_almacen_retiro.png`.
   - **Combate: una divergencia de cuerpo, parametrizada (`bytes_golpe_ataque`).** En `S_AttackPacket`
     y todas sus variantes (`S_AttackMissPacket`, `S_AttackPacketForNpc`, `S_UseAttackSkill`,
     `S_UseArrowSkill`) el campo de golpe/acierto va en **1 byte** (`writeC`) en el US 2009 y en **2**
     (`writeH`) desde el 3.3c; el propio `S_AttackPacket.java` del jp363 conserva ambas formas
     comentadas (`writeC(0x01) // 3.0c` vs `writeH(0x01) // 3.3c`). Leer el ancho de 2 bytes contra
     classic desalineaba la dirección y, peor, leía un **fallo con dirección** (`00 04`) como acierto
     (`0x0400`). Se agregó `bytes_golpe_ataque` al perfil (2 por defecto, 1 en `s3ep1`); `ataque` lee
     `leer(perfil.bytes_golpe_ataque)`. Verificado en vivo: Ensayo acierta (`golpe 01`), el mob falla
     (`golpe 00, dir 04`) y, con `.spawn 45005` (Frog, 1 HP), muere (`DOACTIONGFX` de muerte + `EXP`).
     Captura `classic_combate.png`.
   - **Skills: sin cambios de protocolo.** `.addskill` manda `S_AddSkill`, que el cliente decodifica
     (la cabecera 32/50/100 y las filas de bits; `l.error = false` perdona el relleno que varía) y
     puebla la ventana de magia con los hechizos, su nivel y su coste. Lanzar (`C_UseSkill`) funciona:
     `lanzar Shield` encadenó `SKILLSOUNDGFX` (sonido/gfx de conjuro), `SKILLICONSHIELD` (ícono del
     buff), `OWNCHARATTRDEF` (cambia la AC), `DOACTIONGFX` y `MPUPDATE` (gasta maná). Capturas
     `classic_skills.png`, `classic_skills_cast.png`.
4. **Lado de envío** (`comandos.h`): el mismo criterio para los cuerpos que cambien al mandar (son
   menos; el cliente manda paquetes simples). Si ninguno difiere, esta fase es un repaso.
5. **Documentación:** sección `s3ep1` en `OPCODES_PENDIENTES.md` (como la de `tw380`); nota en
   `ENTREGABLES.md`; el perfil documentado en `opcodes.h`.

## 4. Fuera de este plan

- **Recursos del juego:** el cliente dibuja con los assets 3.6x del install (sprites, ids de NPC,
  mapas, tablas de texto). El US 2009 tiene otros; aunque el protocolo conecte, los gfx, nombres de
  ítems, mapas y páginas HTML pueden no coincidir. Es un problema aparte (de datos, no de red) y se
  ve recién al jugar.
- **Cuánto se puede jugar** en cada versión: lo mide la fase 3 en vivo, no este plan.

## 5. Riesgos

- **La divergencia de cuerpos del US 2009 es mayor que la del 3.80C.** Puede que algún paquete
  necesite el puntero de función por versión (la excepción), no solo parámetros. Si eso se vuelve la
  norma y no la excepción, conviene repensar (quizá sí un schema), pero no antes de medirlo.
- **El perfil puede crecer.** Si llega a decenas de parámetros, es señal de que dos versiones son
  demasiado distintas para una sola familia de decodificadores; se evalúa ahí, no de entrada.
- **Un parámetro con el valor por defecto equivocado** rompe en silencio una versión. Cada parámetro
  nuevo arranca con el valor del 3.63 (el caso base) y se cubre con un test de bytes al tocarlo.

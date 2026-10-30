# Descubrimientos y decisiones

Lo que se averiguó de los datos del juego, del protocolo y del servidor, y lo que se decidió con
eso, por tema. Es la referencia para no volver a descubrirlo: cada punto dice qué pasa, por qué se
hizo así y dónde está en el código. El detalle cronológico y la verificación de cada cosa están en
[ENTREGABLES.md](../ENTREGABLES.md); el protocolo, en [analisis/PROTOCOLO_RED.md](analisis/PROTOCOLO_RED.md).

Convenciones: "el original" es el cliente de Lineage de la instalación; "el servidor" es L1J-JP
3.63c (`servidor_local/l1j-jp`), que es la fuente de verdad del protocolo. Las celdas son del mapa;
los px, lógicos (640×480 en el original).

---

## 1. Recursos del juego

### Archivos `.pak` / `.idx`
- **Índice:** el `.idx` lista nombre, desplazamiento y tamaño. El cliente lo ordena por un hash del
  nombre en minúsculas (FNV-1a de 64 bits) para buscar en forma binaria (`recursos/pak.cpp`).
- **Sprites repartidos:** `Sprite00.pak` … `Sprite15.pak` además de `Sprite.pak`; un archivo va al
  `SpriteNN` que da la suma de los bytes de su nombre en minúsculas, módulo 16, y si no está se busca
  en `Sprite.pak` (`Recursos::sprite`).
- **`Text.pak` está cifrado con DES**: el índice y cada archivo por separado (`CLAVE_DES_LINEAGE`).
- **Mapeados en memoria:** `Tile.pak` (450 MB) y los `Sprite*.pak` (~5 GB) no se leen: se mapean.
  Por eso lo caro es la primera lectura de cada archivo (el disco), no decodificarlo (ver §4).

### Imágenes
- **`.img`:** u16 ancho, u16 alto, u16 ?, u16 color transparente, y los píxeles en RGB555.
- **`.png` "mangled":** algunos vienen con la firma `\x89PNX` y un XOR encadenado con 0x52
  (`check_fix_png` de l1j-client); se reparan antes de decodificar (`decodificar_png`).
- **Fondo negro de los PNG y de los efectos:** el negro puro es transparente y los oscuros unidos a
  él (resplandores) pasan a alfa proporcional a su brillo, con el color compensado: al dibujarse
  equivale a sumarlo al fondo. Lo oscuro dentro de una figura queda opaco (`quitar_fondo_negro`).
  Las imágenes propias (el logo del login) no pasan por esto: traen su alfa.
- **Gráficos aditivos:** el atributo 104, bit 8, de la tabla de sprites marca 1921 gfx (efectos de
  hechizos) que se dibujan sumando luz (`Mezcla::ADITIVA`); su fondo oscuro no tapa.
- **Íconos:** `N.tbt`; el de cada hechizo sale de `spellicon.idx` (en `Sprite.pak`).

### Tiles (`.til`) y sombras
- Cada medio tile es de 24 px de ancho.
- **Sombras translúcidas:** el byte de tipo de un tile tiene un bit 0x04 (tipos 0x06/0x07) que llevan
  todas las sombras de árboles (las 15 996 piezas de capa 8 del mapa 2005) y casi ninguna pieza que
  se levanta (1 de 35 395). Esos tiles se dibujan al 50 % (`ALFA_TRANSLUCIDO`).
- **Personajes en la sombra:** dibujar la sombra encima del personaje lo oscurecería también donde el
  cuerpo sube por encima de ella; en cambio, quien pisa una celda con sombra se dibuja oscurecido
  (×150/255). Shift+F4 lo apaga (`sombra_personajes`). No hay forma de compararlo con el original.

### Mapas (`.s32`)
- **Segmentos de 64×64 celdas**, archivo `map/<mapa>/<XXXXYYYY>.s32` con
  X = (x >> 6) + 0x7E00. Formato en `recursos/mapa.h`.
- **Orientación:** las celdas van "y exterior, x interior"; comprobado contra `maps/2005.txt` del
  servidor en los 35 segmentos del mapa 2005 (la otra orientación mezcla bloqueadas y libres).
- **Bloqueo:** bit 0 del atributo de cada media celda; una celda se pasa si alguna mitad lo está.
- **Objetos del mapa:** vienen agrupados (árbol, casa, cerca), cada uno con sus piezas de media celda
  y su capa. Las piezas de capa ≤ 15 (`CAPA_MAX_SUELO`: sombras, arena, flores) van pegadas al suelo;
  lo que se levanta empieza en la capa 16 y sube de a 4. Al cargar el segmento se guarda por objeto
  el rango de piezas y su rectángulo de celdas (`ObjetoMapa`), para descartar objetos enteros fuera
  de pantalla y para la transparencia (§3).
- **Pie de cada pieza** (`calcular_bases`): la fila más baja del objeto en su columna de pantalla y
  en las vecinas (±`COLUMNAS_BASE` = 6 medias columnas). Así la copa de un árbol se ordena con su
  tronco y una pared larga con su propio tramo (§3).
- **Portales:** el mapa del cliente marca casi todas las celdas de entrada a mazmorras como
  bloqueadas y ningún bit las distingue (se probó 0x08). El `.s32` tiene al final una lista de
  portales con nombre, celda y mapa destino (formato en `recursos/mapa.h`); coincide con la tabla
  `dungeons` del servidor en la mayoría (ver "Portales" en ENTREGABLES.md).
- **Mapa 2005** (el valle escondido de esta versión) es una copia del 69: los sonidos y la música lo
  tratan como 69.

### Tabla de sprites (`datos/sprite_table.txt`)
- Viene de l1j-client: por gfx, el alias de archivos, la sombra, el tipo (atributo 102), las acciones
  y sus frames.
- **Errores del parser original** que afectaban todas las animaciones:
  - un sufijo suelto (`4>`, `6!`) se comía el primer número del frame siguiente;
  - `110 N` no es la velocidad de todo el sprite sino de las acciones que siguen, y cada tick dura
    **960/N ms** (como `spr_actions` del servidor), no 1000/N;
  - los frames de duración 0 no cuentan como 1 tick.
  Con esto, de 9837 acciones del servidor, 5882 dan la misma duración, 44 difieren por 1 ms de
  redondeo y 28 de verdad (sprites distintos entre versiones); 3883 no están en la tabla.
- **Sufijos de un frame:** `!` = frame de impacto (cuando el golpe llega y sale el proyectil),
  `[n` / `<n` = sonido `n.wav` al empezar ese frame.
- **Tipos (atributo 102):** 5 personaje, 8 puerta, 9 ítem del suelo, 10 monstruo, 6/12 NPC de pueblo.
- **Los frames de caminar ya traen el avance:** el cuerpo avanza 0, ¼, ½ y ¾ de celda respecto de la
  celda de origen (en el gfx 0, los pies van a 0, 12, 24 y 36 px). Por eso el sprite se dibuja en
  la celda de origen del paso y la animación hace el resto (`origen_paso`); la posición continua
  queda para la cámara, el orden de dibujo, los nombres y los globos. Interpolar además la posición
  hacía que el cuerpo recorriera casi 2 celdas y volviera atrás.
- **Ancla:** la esquina superior izquierda de la celda; los pies quedan en el centro del rombo
  (calibrado con el explorador).
- **Gfx que no existen en esta instalación:** 30 de los monstruos de la base (islas nuevas, Depraved
  Soul) y algunos NPCs (el almacén de Talking Island, gfx 10669) no están ni en la tabla ni en los
  `.pak`. Se dibujan como un bloque magenta del tamaño de un cuerpo (ver §6).

### Textos
- **Codificación CP949**; fuentes `eng.fnt` y `kor.fnt`.
- **`$N`** en nombres y mensajes es la línea N de `desc-e.tbl` (la tabla tiene 2 líneas de cabecera:
  `$5201` es la línea 5203). Los NPCs de los pueblos se llaman "Oficio^Nombre" (`Storage^Dorin`).
- **Mensajes del servidor:** `string-e.tbl`; fichas de hechizos: `spellexdesc-e.tbl`.
- **Páginas de NPC:** `<id>-e.html` en `Text.pak` (y `<id>.html` si no). Soportan `<a action>`,
  `<a link>` (otra página), `<img src="#N">` con `action`, `link` y `tooltip`, `<font fg>`, `<var>`.
  Algunos vínculos los resuelve el cliente sin el servidor: `fix` (herrero → `C_FixWeaponList`),
  `buyspell` (→ `C_SkillBuy`), `deposit`, `raise-*`.
- **Cursores:** no están en la instalación (ni archivos ni en los `.pak`: seguramente van en el
  ejecutable cifrado). Los del cliente son dibujos propios de 16×16 (`app/cursores.cpp`).

---

## 2. Tiempos y velocidad

El servidor controla los intervalos con un anti-acelerador (`AcceleratorChecker`,
`CheckStrictness=102`): si el cliente va más rápido que la animación, avisa "Overspeed detected" y
desconecta. Todos los tiempos propios salen de la duración de la animación del sprite.

- **Caminar:** un paso dura lo que la animación de caminar del sprite **y su arma** (el personaje
  sin arma: 640 ms). El propio usa ×1,05 de margen: con 640 ms justos el servidor contaba cada paso
  como rápido y a los 10 desconectaba. Los pasos se encadenan (cada uno empieza donde terminó el
  anterior, `MS_ENCADENAR_PASO`) y la animación de caminar se estira a la duración del paso.
- **Factores** (los del anti-acelerador, `intervalo_propio`): haste ×0,75; lento ÷0,75; poción de
  brave ×0,75; waffle ×0,87; Holy Walk ×0,75 al caminar; Bloodlust ×0,65 al atacar. Se mantiene el
  5 % de margen; al caminar, el encadenado de pasos agrega lo que hace falta para que con haste y Holy
  Walk no se acumule el adelanto (con el 5 % solo, no alcanzaba).
- **Otros objetos:** cada uno camina con la duración de su sprite (goblin 760 ms, orco 800, conejo
  960, kobold 1280, rana 1440: igual que el servidor), no con 640 fijos. Un movimiento que llega
  antes de terminar el paso sigue desde la posición interpolada (`O_XANT/O_YANT` en double), y la
  animación de caminar sigue un cuarto de paso más porque el siguiente paquete suele llegar unos ms
  tarde (sin eso, parpadeaba entre pasos).
- **Atacar:** cada ataque espera la duración de la animación de ataque ×1,05 (con el conejo: ≥ 841 ms
  para 800 ms de animación). La animación propia empieza al pedirlo; el eco del servidor no la
  reinicia.
- **Daño visible:** el servidor no le manda a quien ataca la acción de daño de su objetivo; el
  cliente la muestra en el frame de impacto (`!`) del ataque, o cuando llega el proyectil.
- **Hechizos:** la acción es la 18 para los de ataque y la 19 para el resto, y el servidor mide un
  intervalo distinto para cada una. Además hay una espera global (`reuse_delay` de la tabla `skills`:
  10 ms o 0 en casi todos, 1 s en Lightning, Fireball…) durante la cual el servidor **ignora en
  silencio** cualquier hechizo, sin avisar. El cliente espera la mayor de las dos (animación o
  `useDelay` de los datos del cliente) y la muestra sobre los casilleros (§6).
- **Proyectiles:** salen en el frame de impacto del lanzador y vuelan **40 ms por celda** (estimado:
  no está en los datos).
- **Alcance por arma** (`alcance_ataque`): arco (20) y guantelete (62) disparan a lo que esté "en
  pantalla" para el servidor (18 celdas, `Point.isInScreen`); lanza (24), 2 celdas; el resto, cuerpo
  a cuerpo. El arma sale del estado visual de `S_CharPack` / `S_CharVisualUpdate`.
- **Línea de vista:** el servidor la exige para hechizos y flechas (`glanceCheck`, con la
  pasabilidad de flecha del mapa). Con una pared en medio, el hechizo se descarta en silencio y la
  flecha cuenta como fallo; el cliente no la comprueba.
- **Hora del juego:** `S_GameTime` en segundos; hora = (t % 86 400) / 3600; noche fuera de 6–17.

---

## 3. Dibujo y orden

- **Una sola llamada de dibujo por frame:** todas las imágenes viven en un atlas (texture array de 8
  páginas de 2048×2048), cada cosa es una instancia de 48 bytes con una clave de orden u64, se
  ordenan con radix sort estable y se dibujan juntas (`render/gpu.*`).
- **Clave de orden:** byte alto = capa (`CAPA_FONDO` … `CAPA_ARRASTRE`); en el mundo, 32 bits de
  profundidad = fila de pantalla del pie (23 bits, en cuartos de px) + orden dentro de la fila
  (6 bits: piezas más altas primero, personajes al final) + subcapa (3 bits) (`profundidad`).
- **Atlas lleno:** se vacía la página usada hace más tiempo (nunca la del frame actual ni la 0, que
  tiene el blanco, los glifos y la interfaz); `Graficos` olvida sus regiones y las vuelve a
  decodificar si se piden. El uso normal de un pueblo es de 2 páginas.
- **Cámara suave:** se dibuja en una textura de tamaño lógico × escala entera, con posiciones
  fraccionarias llevadas al píxel real más cercano: el arte sigue nítido y la cámara avanza de a
  1/escala de px. La proyección usa double: las coordenadas (~32 000) en float perdían ~0,2 px.
  F4 cambia a la cámara "a saltos" del original (sigue los frames, de a ¼ de celda).
- **Vista:** en el juego, ventana ÷ escala (mínimo 640×480); login y personajes, 640×480 centrados.
  Con `PcRecognizeRange=-1` el servidor manda solo lo que entra en la pantalla del original: con una
  vista grande, lo lejano aparece recién al acercarse.
- **Techos y árboles translúcidos** (`dibujar_objetos_mapa`):
  - si una pieza de un objeto del mapa está delante del personaje y le cubre el centro del cuerpo, se
    dibuja translúcido (alfa 110) **solo lo de ese objeto que queda delante**; el interior y la pared
    de atrás siguen opacos, y así al entrar a una casa se ve adentro;
  - se usa el rectángulo del personaje del frame anterior; solo se miran objetos a ≤ 6 celdas;
  - varias piezas translúcidas superpuestas casi lo esconden, así que cuando algo lo tapa el
    personaje se dibuja otra vez encima de todo con alfa 120 (silueta);
  - la transparencia es inmediata (sin fundido) y vale para el objeto entero: un muro largo de un
    pueblo se vuelve translúcido en todo su largo.
- **Ceguera:** un velo negro sobre el mundo (nombres incluidos) con el propio personaje encima.
- **Ventanas:** cada una se dibuja en su propio nivel: mientras se dibuja, `Gpu::nivel_ventana` lleva
  sus claves de `CAPA_UI`/`CAPA_UI_TEXTO` a `CAPA_VENTANAS` con el nivel por encima de la
  profundidad (5 bits de nivel, 1 de texto). Sin esto, el texto de una ventana de atrás quedaba sobre
  el fondo de la de adelante. Lo que se arrastra va en `CAPA_ARRASTRE`; los diálogos (pregunta,
  polymorph, cantidad, destino, F1, muerte, noticias del login) en el nivel 31, sobre todas.
- **Opacidad de ventanas:** la elegida en Opciones contra el mapa; una ventana que tapa a otra va
  opaca (si no, se lee el texto de la de abajo).

---

## 4. Rendimiento y memoria

Presupuesto: 120 fps con margen. Medido en Release, 1600×900 en Retina: ~1,1–1,2 ms de CPU por
frame, peor frame de cada segundo 2–5 ms al caminar.

- **El costo era E/S, no CPU:** decodificar un medio tile cuesta 0,3 µs, pero la primera lectura de un
  `.til`/`.spr` mapeado esperaba al disco (~50 µs; 20–40 ms en el primer frame de una zona). Un hilo
  (`recursos/precarga.*`) trae sus páginas a memoria y el hilo principal los decodifica uno o dos
  frames después; el suelo y los objetos se piden con margen fuera de la pantalla.
- **Segmentos de mapa en un hilo** (`mundo/cargador_segmentos.*`): leer y decodificar un `.s32` toma
  0,5–1 ms y al cruzar a una zona nueva entraban varios en el mismo frame. El render los pide sin
  esperar (`segmento_cargado`); la lógica (celdas pasables) los carga en el momento (`segmento`). Se
  liberan los que quedan a más de 2 segmentos del borde de lo dibujado (~100 KB cada uno; antes nunca
  se liberaban).
- **Objetos del mapa por rectángulo:** el render descarta objetos enteros sin mirar sus piezas (era
  lo más caro del frame, ~0,8 ms).
- **Cero asignaciones de memoria por frame** (`tests/test_asignaciones.cpp`): arena del frame,
  `TablaSoa`, buffers reutilizados. Las arenas de textos (nombres de objetos e ítems) solo crecían
  —cada cantidad nueva de flechas agrega el nombre—: pasados 256 KB se compactan.
- **Descartado:** una caché en disco (decodificar es barato; una caché RGBA8 ocupa el doble que el
  RGB555 y también hay que leerla) y una variante del radix sort con los histogramas en una pasada
  (no mejoró nada medible). La variación entre corridas iguales es de ±0,07 ms: comparar siempre
  varias.
- **Fugas:** `leaks --atExit` encuentra solo ~26 KB de macOS (conexiones XPC de AppIntents).

---

## 5. Protocolo y el servidor L1J-JP 3.63c

### Formatos que no son obvios
- **Opcodes compartidos:** `S_PetCtrlMenu` y `S_CharReset` son el 33 (se distinguen por el primer
  byte: 0x0c es la mascota); `S_ItemStatus` usa el número de `S_ItemAmount`; `S_PacketBox` (40) es una
  caja con decenas de subtipos (peso 10, comida 11, aura 0x16, íconos 34/40…); el inventario de una
  mascota llega como lista de almacén (`S_RetrieveList`) con tipo 0x0b.
- **Un mismo opcode, varios tamaños:** `S_AttackPacket` (142) es golpe, fallo, hechizo o flecha
  según el largo (el de hechizo/flecha trae el gráfico y las celdas de origen y destino).
- **Estado de `S_CharPack`:** el byte tras el gfx es el arma (se suma a la acción base), 8 = muerto
  (cadáver), y en una puerta 28 abierta / 29 cerrada. Los flags: 1 veneno, 2 invisible, 4 personaje,
  8 congelado, 128 fantasma.
- **Lo que el servidor no manda:** el número de daño (el campo de `S_AttackPacket` es una marca de
  acierto: 1 en un golpe cuerpo a cuerpo, 6 en una flecha o un hechizo, 0 si falló), la vida
  de los monstruos (`S_NPCPack` trae 0xFF; `S_HpMeter` solo va al grupo y a las mascotas), los
  marcadores guardados al entrar (ver abajo).
- **`S_InputAmount`:** los textos dependen de quién lo manda: "request" + acción (fabricar),
  "agapply" + "agapply N" (subasta), o la página + h 1 + h 2 + NPC + precio (posada). El cliente
  devuelve la acción en `C_Amount`.
- **Páginas con el id del propio personaje:** RaiseAttr (subir un atributo) y la lista de Summon
  Monster con el anillo de control llegan como `S_ShowHTML` del propio personaje, y la elección vuelve
  como `C_NPCAction` con ese id.
- **Teleport con marcador:** el pergamino (usos 6 y 29) manda en `C_UseItem` el mapa y el id del
  marcador (0 = al azar); el hechizo Teleport, igual en `C_UseSkill`.
- **Atributos de un ítem** (`L1ItemInstance.getStatusBytes`): van al final de `S_AddItem`,
  `S_InvList` y `S_ItemStatus` como c largo + bytes, y solo si está identificado (si no, largo 0).
  Son pares etiqueta + valor de tamaño fijo según la etiqueta (2 encantamiento, 5 acierto, 7 clases
  en bits, 8–13 FUE…CAR, 14 HP en h, 15 MR en h…). Las etiquetas 1 (daño), 19 (AC), 21 (comida),
  22 (luz) y 23 llevan detrás, sin etiqueta propia, el material (c) y el peso (d). Un 15 seguido de
  33 y un tipo es una resistencia a un estado, no MR. Una etiqueta desconocida corta la lectura,
  porque no se sabe cuánto ocupa.
- **Hechizos sobre un ítem:** Enchant Weapon, Blessed Armor y Shadow Fang leen en `C_UseSkill` el
  id del ítem en el lugar del objetivo; Bless Weapon y Holy Weapon usan el arma equipada. Ninguno
  manda ícono: solo el mensaje 161. Enchant Mighty/Dexterity mandan `S_Strup`/`S_Dexup` (h
  segundos, la estadística, en FUE el peso, c tipo).
- **Pociones de FUE/DES:** `S_Strup`/`S_Dexup` llevan en el campo "tipo" el aumento de la poción
  (6), y 5 o 2 para las magias de mago y de elfo: el cliente no sabe qué poción fue más que por el
  ítem recién usado.
- **Opcode 33:** `S_PetCtrlMenu` (primer byte 0x0c) y `S_CharReset` (1, 2 o 3); al entrar llegan otros
  paquetes con ese número y otro primer byte (0x04, 0x42, 0x43, 0x2a) que se ignoran.
- **Reiniciar atributos:** el servidor no valida los atributos iniciales de `C_CharReset` 1 (los asigna
  tal cual): el cliente aplica las reglas de la creación.
- **Nivel en `S_CharPack`:** `S_NPCPack` manda el nivel del monstruo en el segundo byte después del
  HP (`S_OtherCharPacks` manda 0); de ahí sale el color del nombre según la diferencia con el jugador.
- **Nombre rosa:** `S_PinkName` solo se manda si los dos son legales (lawful ≥ 0), ninguno ya rosa,
  y ambos en zona normal (no en pueblos) y fuera de una guerra.
- **Hechizos de área alrededor del lanzador** (Tornado, Fire Storm, Blizzard, Freezing Blizzard):
  sin objetivo (no tienen `range=` en `spells`); `S_RangeSkill` tipo 0. El sprite es uno solo: el
  original lo repite (4 tornados; la ventisca en toda la pantalla), según la clase de `spells`
  (`SpellTornado`, `SpellBlizzard`) y su `areaEffect`.
- **Íconos de los buffs:** el servidor manda ícono solo para algunos (`S_SkillIconShield`,
  `S_SkillBrave`, `S_SkillHaste`, el aura de `S_PacketBox`, `S_Strup`/`S_Dexup`, `S_SkillIconGFX`).
  Para el resto, el original lleva el tiempo con la duración de `spells`; el cliente pone el ícono al
  ver el gráfico de su hechizo sobre sí mismo. `spellicon.idx` cubre solo 168 magias y deja en 0
  varias de caballero, elfo y otras clases.
- **Usar un ítem sobre otro:** con los usos 7 (identificar), 14, 26 y 27 (encantar) `C_UseItem` lleva
  el id del objetivo después del del pergamino. `S_IdentifyDesc` es un `S_ServerMessage` (mensajes
  134–138) con un h (la descripción del ítem) adelante.
- **Campos que el servidor no usa** pero hay que mandar: x, y de `C_GiveItem` y de `C_Door`, el tipo
  de `C_SelectTarget`.

### Particularidades del servidor (no son errores del cliente)
- **Un GM no muere** (al llegar a 0 vuelve a HP máximo) **ni gasta MP**. El GM se decide por el
  `access_level` del **personaje**, que hereda el de la cuenta al crearse: los personajes creados en
  una cuenta GM son inmortales.
- **Marcadores:** ningún mapa de la base tiene `markable` = 1 (ni siquiera un GM puede marcar), y
  `C_LoginToServer.bookmarks()`, que manda los guardados al entrar, existe pero nunca se llama.
- **Espera de hechizos y línea de vista:** ver §2; los dos descartan en silencio.
- **Zonas seguras:** atacar a otro jugador en un pueblo se acepta pero no hace daño.
- **Morir se anima con retraso:** `death()` manda la acción de morir (8) un momento después de dejar
  la vida en 0. Si en ese intervalo alguien lo resucita (solo pasa con `.death` y `.ress` de GM
  seguidos), `S_Resurrection` llega antes que la acción de morir.
- **Borrar un ítem** (`C_DeleteInventoryItem`): con cantidad 0 el servidor borra toda la pila, pero
  lee la cantidad del ítem antes de revisar que exista, así que un id inexistente con cantidad 0 le
  tira una excepción. El cliente siempre manda una cantidad > 0 de un ítem que tiene.
- **Domesticar:** dar el ítem de domesticación (Monster Eye Meat, 40057, en la mayoría) a un animal
  con menos de un tercio de su vida y con carisma libre ≥ 6.
- **Portales:** `C_MoveChar` revisa `DungeonTable` en la celda de destino antes que nada y toma las
  coordenadas del paquete como origen del paso. Por eso, como el cliente no sabe qué celdas son
  portales, al intentar entrar a una celda bloqueada vecina (con el teclado o un clic justo sobre
  ella) se le pide el paso al servidor, a lo sumo una vez por segundo por celda; si es un portal,
  teletransporta. El original lo hacía con `C_EnterPortal` y la lista de portales del `.s32`
  (pendiente).
- **Teleport:** `S_Teleport` espera la respuesta `C_Teleport` antes de mover; tras un teleport el
  original espera `S_Paralysis` 7 para soltar el bloqueo.
- **Lo que el servidor envía y el cliente no implementa:** [analisis/OPCODES_PENDIENTES.md](analisis/OPCODES_PENDIENTES.md).

---

## 6. Decisiones de interfaz

- **UI simple por defecto** (texto, F2 la cambia por el panel original), también en login, selección y
  creación de personaje. Logo propio en el login con `logo=`.
- **Chat con Enter:** las teclas son acciones (1–0 usan la barra, A ataca); Enter abre la línea, Enter
  envía y la cierra, Esc la cancela. "Always typing" en Opciones vuelve al modo anterior.
- **Barra de atajos:** 10 casilleros (1–0 o Alt+número). Clic del medio sobre un casillero lo elige;
  el botón del medio sobre el mundo usa el elegido sobre lo que haya bajo el puntero y, mantenido, lo
  repite en cuanto termina la espera (un ítem, cada 500 ms). La espera entre hechizos se ve sobre los
  casilleros con centésimas de segundo.
- **Atacar:** el objetivo se fija al apretar el botón y se mantiene mientras siga apretado aunque la
  cámara se mueva (antes se recalculaba en cada paso y el personaje terminaba caminando hacia el
  suelo). Se decide antes de esperar el fin del paso para no perder el clic. **A** sigue atacando sin
  mantener el botón hasta que el objetivo muere o se camina. **Ctrl** (o ⌘) fuerza el ataque a NPCs y
  jugadores; **"/pk"** (del cliente: el servidor no tiene ese comando) hace que el clic sobre un
  jugador lo ataque sin Ctrl.
- **Cursores:** mano; espada sobre monstruos (o con Ctrl, o jugadores en modo PK); labios sobre NPCs;
  círculo al elegir objetivo.
- **Monstruos sin sprite:** un gfx sin acciones en la tabla cuenta como monstruo (bloque magenta
  atacable), salvo que el nombre sea "Oficio^Nombre" (NPC de pueblo: se le habla).
- **Dar ítems:** soltar un ítem del inventario sobre un NPC o mascota se lo da (de una pila pregunta
  cuántos); sobre el suelo, cae; sobre una ventana, no pasa nada (antes caía al suelo).
- **Ventanas:** la que se abre o se toca pasa adelante; solo la de más arriba recibe el mouse (a las
  demás se les esconde el puntero, salvo durante un arrastre); al abrir se busca un lugar libre si la
  de siempre tapa a otra.
- **Borrar un personaje** pide confirmación en la UI simple.
- **Mascotas:** lista junto a la barra (clic: su menú del servidor; "Bag": su inventario).
- **Marcadores:** "/bookmark nombre"; con marcadores, el pergamino y Teleport preguntan el destino.

### Audio
- Un dispositivo de SDL3 y varios streams que SDL mezcla: música MP3 (dr_mp3, porque SDL3 no decodifica
  MP3), 16 streams de efectos (el más viejo se corta; el mismo efecto dentro de 60 ms no se repite) y
  un bucle de ambiente.
- **La música y el ambiente se alimentan desde el hilo de audio** (callbacks de los streams), no
  desde el frame. Antes se mantenía medio segundo en cola desde el bucle principal, y en macOS el
  bucle se detiene más de 1 s al cambiar de aplicación (medido: 1091 ms al volver la ventana al
  frente): el sonido daba un salto. Con una pausa forzada de 1,5 s, el código anterior vaciaba el
  stream y el nuevo no. Los cambios de tema bloquean el stream (`SDL_LockAudioStream`) porque el
  callback usa el mismo decodificador.
- **Música por zona** (`music.def`): vale la primera zona que contiene la posición, en el orden del
  archivo (los pueblos van antes que el mapa que los rodea); "Login" y "Death" son temas fijos.
- **Ambiente** (`envsound.def`): un bucle por zona y momento del día, más sonidos sueltos de cada
  grupo cada tanto (entre la mitad y 1,5 veces su intervalo).
- **Efectos** de las animaciones (`[n`/`<n` de la tabla): más bajos con la distancia (nada a 20
  celdas); sin paneo.
- **Clima:** lluvia y nieve (`S_Weather` 1–3 nieve, 17–19 lluvia) solo en una lista fija de mapas al
  aire libre.

---

## 7. Probar

- **Solo el servidor local**, con `--mudo` y `LIN_CONFIG` propio (reglas en [AGENTS.md](../AGENTS.md)).
- **Automatización:** la tabla de opciones está en el [README](../README.md). Para lo que necesita
  mouse o teclado: `--arrastrar` (repetible), `--mantener`, `--mantener-sobre=Nombre,N` (el punto
  queda fijo donde estaba el objeto: el caso de la cámara que se mueve), `--medio`, `--ctrl`,
  `--pulsar` (teclas como eventos de SDL, también en el login), `--cantidad`, `--dar`, `--destino`.
- **Las ventanas se acomodan solas** (lugar libre): antes de un clic simulado, sacar una captura para
  ver dónde quedaron.
- **Trazas:** `LIN_TRAZA=1` (paquetes con sus primeros bytes), `LIN_HTML=1` (páginas), `--perf`.
- **Comandos de GM útiles:** `.move x y mapa`, `.item id n`, `.spawn npc n alcance`, `.summon npc`,
  `.level n`, `.ress` (cura), `.addskill`, `.buff me id seg`, `.weather n`.
- **Lecciones:**
  - un objeto que camina no está donde estaba en la captura anterior: ubicarlo en la misma corrida
    (`--mantener-sobre`, `--dar`, `--atacar`);
  - las invocaciones y monstruos de pruebas anteriores siguen ahí (cambian a quién elige "el más
    cercano");
  - los personajes de la cuenta `prueba2` son GM (no mueren, no gastan MP), y Hermione y Mago son
    del usuario.

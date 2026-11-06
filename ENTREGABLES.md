# Entregables 1–10: estado y verificación

Todo se probó en macOS 26.5 (Apple Silicon, pantalla de 120 Hz) contra el servidor L1J-JP 3.63c
local (`servidor_local/`), con la instalación de Lineage Justice. Las capturas están en
`docs/capturas/` y se generaron con las opciones de automatización del README.

## Entregable 1: Pantalla de login ✅

![Login](docs/capturas/1_login.png)

**Funciona:**
- Fondo `814.png`: viene ofuscado (`\x89PNX` + XOR encadenado) en `Sprite00.pak`, que se elige por el hash del nombre.
- Recuadro `59.img` y botones Login/Change/New/Exit (`53/65/55/57.img`, con imagen de resaltado N+1).
- Texto de bienvenida de `intro-e.tbl`, de `Text.pak` descifrado con DES.
- Fuente original `eng.fnt`.
- Cajas de usuario y clave con cursor y asteriscos, Tab y Enter.
- Conexión: paquete inicial sin cifrar, derivación de clave y cifrado en ambos sentidos.
- Versión y login. Errores del servidor (usuario/clave, cuenta en uso) y ventana de noticias.

**Verificado:**
- Login real contra el servidor, con la traza `LIN_TRAZA=1`.
- Tests de cifrado con vectores de la implementación Python que coincide con el cliente original.
- Test de DES con el vector estándar.
- `.pak` y `814.png` probados contra los archivos reales.

**Pendiente:**
- "Change" y "New" no hacen nada: L1J crea las cuentas solas.
- Música del login (`lin_audio`, entregable 10).
- Opciones (entregable 10).

**UI simple del login** (con `ui_simple=1`, la de siempre; F2 vuelve a la original):
- Fondo oscuro, logo arriba y un recuadro con Account, Password, Login y Quit; abajo, el servidor.
- **Logo propio:** `logo=` en `settings.ini` (o `--logo=`): un PNG con alfa de hasta 2048×2048, ruta absoluta o relativa a la carpeta de `settings.ini`, achicado para entrar en 560×190. Sin logo, o si no se puede leer, "LINEAGE" en texto (y un aviso en el registro).
- Capturas: `docs/capturas/1_login_simple.png` y `1_login_logo.png`.

## Entregable 2: Selección y creación de personaje ✅

![Selección](docs/capturas/2_seleccion.png) ![Creación](docs/capturas/2_creacion.png)

**Funciona:**
- **Selección:**
  - fondo `815.png` y 4 ranuras con las animaciones PNG por clase y sexo (posiciones de `sdl_animate_button.cpp`);
  - la ranura seleccionada se anima; las vacías dicen "New Character" y se animan al pasar el mouse;
  - panel con nombre, clan, clase, alineamiento, HP, MP, CA, nivel y los 6 stats;
  - botones entrar, volver y borrar, y páginas.
- **Creación:**
  - fondo `824.img`, 7 clases y 2 sexos (radios con imagen), vista previa animada;
  - stats con botones +/− y puntos restantes;
  - las reglas de `C_CreateChar.java`: mínimo y máximo por clase, total 75;
  - nombre y mensajes de error del servidor.

**Verificado:**
- Se creó "Exploradora" en el servidor real (NEWCHARWRONG 2 + NEWCHARPACK) y aparece en la lista seleccionada.
- La lista coincide con lo que manda el servidor.
- Test de decodificación de `S_CharPacks`.

**Resplandor de las animaciones** (antes: un disco oscuro detrás del personaje seleccionado):
- Las animaciones PNG traen, sobre negro, un resplandor en degradé de 0x01 a ~0x40 que el original sumaba al fondo. Solo se quitaba el negro y los casi negros, y el resto del degradé quedaba opaco.
- **Ahora** (`quitar_fondo_negro`): desde el negro puro se recorren los píxeles oscuros unidos a él y cada uno pasa a alfa proporcional a su brillo, con el color compensado; al dibujarse equivale a sumarlo al fondo. Lo oscuro dentro de una figura (las versiones apagadas) queda opaco. Verificado en captura y con un test.

**UI simple** (con `ui_simple=1`; F2 vuelve a la original):
- **Selección:** lista de todos los personajes (nombre, nivel, clase, clan) y "+ New character" en las ranuras libres; a la derecha, la animación y los datos del elegido; Enter, Delete (pide un segundo clic: "Sure?") y Back.
- **Creación:** clase y sexo a la izquierda; atributos con −/+, puntos libres, nombre y la animación a la derecha; Create y Back.
- Capturas: `docs/capturas/2_seleccion_simple.png` y `2_crear_simple.png`.

**Pendiente:**
- Las flechas de página (`1765/1767.img`) no se ven con esta instalación: falta buscar sus nombres nuevos.
- Borrar no pide confirmación en la pantalla original (sí en la simple).
- Crear y borrar desde la UI simple solo se probó con la automatización, no a mano.

## Entregable 3: Dentro del juego, moverse ✅

![Juego](docs/capturas/3_juego.png) ![Caminando](docs/capturas/3_juego_caminando_depuracion.png)

**Funciona:**
- **Mapa:**
  - suelo isométrico de los `.s32` (segmentos de 64×64 cargados bajo demanda);
  - medios tiles de `Tile.pak`;
  - objetos del mapa (árboles, cercas, piedras) ordenados por profundidad.
- **Personajes y NPCs** (S_CharPack):
  - sprites compuestos desde los `.spr`;
  - acción caminar o quieto según el arma, en 8 direcciones, con la tabla de acciones y sombra;
  - nombres `$N` resueltos desde `desc-e.tbl` (p. ej. "Red Key Quest'Tio");
  - nombres coreanos con `kor.fnt` ("도도로도").
- **Objetos del mundo:** aparecen, se mueven con interpolación y desaparecen (S_MoveCharPacket, S_RemoveObject, S_ChangeHeading).
- **Movimiento propio:**
  - clic mantenido o flechas;
  - predicción local y C_MoveChar al servidor;
  - colisiones con el bit 0 del atributo del `.s32`;
  - giro si la celda está bloqueada.
- **Salida:** C_QuitGame al salir, para que el servidor guarde la posición.
- **HUD mínimo:** HP, MP, nivel, CA y últimos mensajes de chat.

**Verificado:**
- **Movimiento contra el servidor:** con `--pasos=4` el cliente envió 4 C_MoveChar desde (32691, 32864) hacia la dirección 3, y la base quedó en (32695, 32868). El servidor reubica al personaje en (32691, 32864) en cada login porque el mapa 2005 tiene punto de reinicio (`RestartLocationTable`).
- **Ritmo de los pasos:** el paso propio dura 672 ms (640 × 1,05, como el original). Con 640 ms justos, el anti-acelerador de L1J (`AcceleratorChecker`, `CheckStrictness=102`) contaba cada paso como demasiado rápido: a los 10 pasos seguidos mostraba "Overspeed detected" y desconectaba. Verificado: 40 pasos seguidos de un personaje no GM, con un intervalo mínimo de 674 ms y sin avisos.
- **Cámara suave y pasos continuos:**
  - **Antes:** todo se dibujaba a 640×480 con la cámara redondeada a píxeles enteros. El mundo avanzaba de a saltos de 1 px a un ritmo irregular, el personaje oscilaba ±1 px respecto del suelo y cada paso empezaba un frame tarde, con una pausa.
  - **Ahora:**
    - se dibuja en una textura de 640×480 × la escala entera de la ventana, con posiciones fraccionarias llevadas al píxel real más cercano: el arte sigue nítido y la cámara avanza de a 1/escala de píxel;
    - la proyección y la interpolación usan `double`, porque las coordenadas (~32 000) en `float` perdían ~0,2 px;
    - los pasos se encadenan, de modo que cada uno empieza donde terminó el anterior;
    - la animación de caminar se estira a la duración del paso.
  - **Medido** con 12 pasos seguidos a escala ×3: la cámara se mueve en 996 de 1050 frames, en saltos de 1/3 px, sin cortes entre pasos, con intervalos de envío de 666–676 ms y sin avisos del servidor.
- **Movimiento de monstruos y NPC:**
  - **Error en el parser de `sprite_table.txt` (afectaba a todas las animaciones):**
    - un sufijo suelto (`4>`, `6!`) se comía el primer número del frame siguiente y desfasaba la acción (el caminar del goblin daba 15 s);
    - `110 N` se tomaba como velocidad de todo el sprite con la fórmula 1000/N, pero es la velocidad de las acciones que siguen, y cada tick dura 960/N ms (como `spr_actions` del servidor);
    - los frames de duración 0 se contaban como 1 tick.
  - **Resultado:** de 9837 acciones de `spr_actions` del servidor, 5882 dan la misma duración en el cliente, 44 difieren solo por 1 ms de redondeo, 28 difieren de verdad (sprites distintos entre versiones) y 3883 no están en la tabla de l1j-client.
  - **Duración del paso:** cada objeto usa la animación de caminar de su sprite y su arma, ×3/4 con haste y ×4/3 con lentitud. Antes eran 640 ms fijos, así que un monstruo cruzaba la celda y se quedaba quieto hasta el paso siguiente. Si el sprite no está en la tabla, se usan 640 ms.
  - **Sin saltos:** un movimiento que llega antes de terminar el paso sigue desde la posición interpolada (columnas `O_XANT/O_YANT` en `double`).
  - **Sin parpadeo entre pasos:** la animación de caminar sigue un cuarto de paso más, porque el siguiente paquete suele llegar unos ms tarde.
  - **Medido** con 10 monstruos invocados por GM (`.spawn`): los pasos duran lo mismo que en el servidor (goblin 760 ms, orco 800, conejo 960, kobold 1280, rana 1440). El desplazamiento máximo entre frames fue de 0,57 px, sin saltos de más de 4 px, con 1 pausa corta en ~25 000 frames con movimiento.
- **Los frames de caminar ya traen el avance:**
  - **Qué hacen los `.spr`:** el cuerpo avanza 0, ¼, ½ y ¾ de celda respecto de la celda de origen.
    - En el personaje (archivos `3225-*.spr`, el alias del gfx 0), los pies avanzan 0, 12, 24 y 36 px en horizontal.
    - Lo mismo pasa en los monstruos (rana, goblin, orco, conejo), con su cantidad de frames.
  - **El error:** el cliente además interpolaba la posición, así que el cuerpo recorría casi 2 celdas por paso y volvía a la real al terminar. Era el "paso largo que vuelve atrás".
  - **Ahora:**
    - los sprites se dibujan en la celda de origen durante el paso (`origen_paso`) y la animación hace el resto, como el original, con los pies fijos sobre el suelo;
    - la posición continua queda para la cámara, el orden de dibujo, los nombres y los globos.
  - **Cámara:**
    - suave por defecto: tu personaje se compensa con `fraccion_frame` para quedar centrado;
    - con F4, "a saltos": sigue los frames, de a ¼ de celda.
  - **Verificado:** 4 pasos contra el servidor sin avisos, y una captura a mitad de paso con el personaje centrado.
- **Vista ampliable:**
  - **Tamaño lógico:** dentro del juego es ventana ÷ escala (mínimo 640×480), y la escala se ajusta con Ctrl/⌘ +/−/0 (`zoom` en `settings.ini`).
  - **Disposición:**
    - el panel original queda centrado abajo y la ventana de estado se apoya en el borde derecho;
    - el suelo, los objetos del mapa (con más segmentos cargados alrededor) y el recorte de los personajes se calculan según el tamaño de la vista.
  - **Fuera del juego:** el login y los personajes siguen en 640×480 centrados.
  - **Medido** (Release, ventana 1600×900 en Retina):
    - con zoom automático (×3): 1008×551 lógicos, 3830 instancias y 0,78 ms de CPU;
    - con zoom ×2: 1512×827 lógicos, 6681 instancias y 1,57 ms de CPU por frame;
    - 1 llamada de dibujo y 120 fps en ambos casos.
  - **Límite del servidor:** con `PcRecognizeRange=-1` (configuración actual), L1J manda los personajes y NPC dentro de la pantalla del original. Con una vista muy grande, el mapa se ve completo, pero los personajes lejanos aparecen recién al acercarse. En el servidor local se puede subir ese valor en `config/server.properties.xml`.
- **Orientación del `.s32` y bit de bloqueo:** comparados contra el mapa del servidor (`maps/2005.txt`) en los 35 segmentos del mapa 2005.
  - Leyendo "y exterior, x interior", las combinaciones quedan limpias: tile transitable 31 ↔ atributos 4/4 en 63 341 celdas, y tiles bloqueados 0/16 ↔ atributos con el bit 0 activo.
  - La orientación opuesta mezcla transitables con bloqueados.
  - El test `Mapa 2005` exige al menos 95 % de coincidencia en el segmento inicial.
- **Ancla de los sprites:** calibrada con el explorador; los pies quedan en el centro del rombo (`docs/capturas/explorador_gfx0.png`).
- **Rendimiento (Release, overlay F3):**
  - **0,50 ms de CPU por frame** con 2088 instancias, frente a los 2 ms presupuestados;
  - **1 llamada de dibujo por frame**;
  - 1 página de atlas;
  - 120 fps (vsync).

**Pendiente o conocido:**
- **Orden entre objetos del mapa y personajes** (antes: cada pieza por su propia fila, así que un personaje detrás de un árbol quedaba sobre la copa):
  - **Ahora:** cada pieza de un objeto se ordena por el pie del objeto en su columna de pantalla y en las vecinas (±3 celdas, `COLUMNAS_BASE`). La copa usa la fila del tronco y una pared larga, la de su tramo. Dentro de la misma fila de pie van primero las piezas más altas y al final los personajes.
  - **Verificado** con `--ruta` en el árbol del punto de inicio: detrás, la copa tapa al personaje; delante, el personaje queda sobre el tronco. Hay un test con el `.s32` real.
  - **Pendiente:** compararlo con el original en zonas con casas y muros grandes.
- **Piezas pegadas al suelo** (antes: las sombras de los árboles y algunos parches de arena se dibujaban sobre los personajes):
  - **Ahora:** las piezas de capa 15 o menor (`CAPA_MAX_SUELO`: sombras, arena, flores) se dibujan con el suelo, debajo de todo lo que se levanta, y no cuentan para el pie de los objetos. En el mapa 2005 hay ~36 000 en las capas 1–9 y casi ninguna en 10–15; los objetos que se levantan empiezan en la capa 16.
- **Sombras translúcidas** (antes: manchas negras opacas):
  - **El dato:** el byte de tipo de cada tile tiene un bit 0x04 (tipos 0x06/0x07) que llevan todas las sombras de árboles (las 15 996 piezas de capa 8 en el mapa 2005) y casi ninguna pieza que se levanta (1 de 35 395). Todos sus píxeles son grises oscuros o negros.
  - **Ahora:** esos tiles se decodifican con alfa 50 % (`ALFA_TRANSLUCIDO`) y se mezclan con el suelo.
  - **Personajes en la sombra:** dibujar la sombra encima del personaje lo oscurecería también donde el cuerpo sube por encima de ella. En su lugar, el personaje cuya celda tiene una sombra se dibuja oscurecido (×150/255). F5 lo apaga (`sombra_personajes` en `settings.ini`). No hay forma de compararlo con el original desde los datos.
  - **Verificado:** capturas con F5 encendido y apagado junto al árbol del punto de inicio: el personaje en la sombra se oscurece y fuera de ella queda igual. Hay un test con el `.s32` real.
- Luz y noche: hechas después, ver [PLAN_LUZ_AGUA.md](docs/analisis/PLAN_LUZ_AGUA.md) (noche, mazmorras, luces, agua, visión de elfo y borrachera).
  Costo de la oscuridad contra `efecto_luz=0`, vista de 2560×1654 a zoom 1, CPU por frame: de noche en un lugar poblado (37 240 instancias), 2,08 contra 1,69 ms en optimizado y 9,13 contra 6,18 en `debug`; en mazmorra, 2,98 contra 2,80 y 5,44 contra 4,30. Sin asignaciones por frame. Cinco muestras de un segundo: la base varía entre corridas, así que vale la diferencia. Contador de fps en pantalla: Shift+F3 (`luz_contador_fps.png`).
- Las acciones puntuales (ataque, daño) se guardan y animan, pero el combate es el entregable 5.
- Rendimiento: ver la sección [Rendimiento](#rendimiento) (precarga en un hilo, reemplazo LRU del atlas y test de cero allocations).

## Entregable 4: Chat y estado ✅

![Chat](docs/capturas/4_chat.png) ![Estado](docs/capturas/4_estado.png)

**Funciona:**
- **Panel inferior original:** `1028.img` (barras), `1042.img` (chat), `1019.img` (teclas F5–F12) y `1039.img` (entrada), en las posiciones de l1j-client.
  - Barras de EXP (`1029`), HP (`1030`) y MP (`1031`): se recortan según el valor y el rótulo queda siempre visible.
  - Casillas de CA, alineamiento, peso y comida.
- **Chat:**
  - el registro es un anillo de 256 líneas de tamaño fijo, así que agregar un mensaje no reserva memoria;
  - las líneas se ajustan al ancho del panel sin partir caracteres coreanos;
  - se desplaza con la rueda del mouse sobre el panel o con Re Pág / Av Pág;
  - la caja de entrada tiene siempre el foco, como en el original: se escribe, Enter envía y Esc borra;
  - los prefijos del original eligen el canal: `!` grito, `&` global, `$` comercio, `@` clan, `#` grupo, `%` alianza, `*` grupo de chat y `"nombre texto` para susurrar;
  - cada canal tiene su color;
  - el chat normal y el grito aparecen 5 s sobre la cabeza de quien habla.
- **Mensajes del servidor:**
  - `S_ServerMessage` y `S_BlueMessage` se arman con `string-e.tbl` (id = línea): `%0`/`%1` se reemplazan por los argumentos y los `$N` por `desc-e.tbl`;
  - los códigos de color `\fX` se quitan y el primero colorea la línea;
  - se muestran los mensajes del sistema (`S_SystemMessage`), los anuncios del servidor y lo que dicen los NPC (`S_NpcChatPacket`, con `$N` resuelto).
- **Estado en vivo:**
  - `S_OwnCharStatus`, `S_HPUpdate`, `S_MPUpdate`, `S_Exp`, `S_Lawful`, `S_SpMr`, `S_OwnCharAttrDef`, `S_OwnCharStatus2` y `S_PacketBox` (peso y comida) actualizan el estado del jugador (`EstadoJugador`, el módulo "Player" del plan);
  - el porcentaje de EXP usa la tabla de `ExpTable.java`.
- **Ventana de estado (Alt+C):** muestra nombre, clase, nivel, EXP, HP, MP, los 6 atributos, CA, MR, SP, alineamiento, las 4 resistencias elementales, peso y comida. Se cierra con la X del marco.

**Verificado:**
- **Dos clientes conversan** (`4_chat.png` es lo que ve A y `4_estado.png` lo que ve B), con dos cuentas a la vez contra el servidor local:
  - B dice "Hola A, subo de nivel" y A responde "Te leo, B": cada uno lo ve en su registro y sobre la cabeza del otro.
  - Grito: llega como `<Charla> ...` en naranja.
  - Global de B (GM): A lo recibe como `[******] ...` en amarillo.
  - Global de A (nivel 1): el servidor responde con el mensaje 195, que se muestra como "You do not have the ability to use global chat.".
  - Susurro: B ve el eco `-> (Prueba) ...` y A recibe `<- (Charla) ...` en rosa. Antes del nivel 5, el servidor responde con el mensaje 404 ("A character below level %0 cannot whisper...").
- **El estado se actualiza en vivo:** B escribe `.level 12` (comando de GM del servidor). Sin salir del juego, la barra y la ventana de estado pasan a nivel 12, HP 20/142, MP 2/33 y CA 9, y la regeneración llega después en `S_HPUpdate`.
- El mensaje 3247 que manda el servidor al entrar coincide con su línea de `string-e.tbl` ("Inform the creation of the clan!" ↔ 血盟を創設して…お知らせ).
- Los dos clientes completos corrieron con ASan + UBSan durante la conversación, sin reportes.
- **Rendimiento (Release, overlay F3):** con el panel, el chat y la ventana de estado abiertos, 0,52 ms de CPU por frame, 1 llamada de dibujo y 120 fps.

**UI simple (por defecto; F2 alterna con el panel original):**
- **Qué muestra:** una franja oscura de ancho completo abajo, solo con texto: 5 líneas de chat, la entrada `> ` y una línea de estado con barras ASCII (`Lv 1 [--------] 0.00%  HP [########] 14/14  MP [...]  AC  Lawful  Weight  Food`). La ventana de estado (Alt+C) es un recuadro de texto con `[x]`.
- **Lo que se comparte:** el chat (registro, desplazamiento, entrada, prefijos) y el contenido de la ventana de estado son los mismos en los dos modos; solo cambia dónde y cómo se dibujan (`AreaChat`, `Vista::panel_y`). La franja ocupa menos que el panel original, así que el área de juego crece.
- **Verificado** contra el servidor en 853×551 y en 640×480: la línea de estado entra con valores altos (nivel 12, HP 34/142), y el panel original sigue igual con `--ui_simple=0`. Captura: `docs/capturas/4_ui_simple.png`.

**Para repetir la prueba:**
- En el servidor local, `Charla` (cuenta `prueba2`) quedó como GM: `characters.access_level = 200`. Así se puede usar `.level`.
- Comandos:
  ```sh
  build/release/lineage --juego="$LINEAGE_DIR" --usuario=prueba2 --clave=prueba2 --auto --personaje=Charla --crear \
      --estado --decir="Hola A" --decir=".level 12" --decir="&Global" --decir='"Prueba psst' \
      --captura=b.bmp --captura-espera=1500 --salir &
  build/release/lineage --juego="$LINEAGE_DIR" --usuario=prueba --clave=prueba --auto --personaje=Prueba \
      --decir="Te leo, B" --captura=a.bmp --captura-espera=600 --salir
  ```

**Pendiente o conocido:**
- **Ventana de estado:** no encontré la imagen exacta del original en esta instalación. Se usa el marco genérico `4131.img` con el contenido escrito encima.
- **Colores:** los de cada canal y los de los códigos `\fX` son aproximados. La paleta real está en `lin.bin`.
- **Entrada de texto:** acepta solo ASCII. Para escribir en coreano falta la conversión de UTF-8 a CP949 de la entrada IME.
- **Lo que todavía no hace el panel:**
  - las flechas de desplazamiento y el botón para agrandar el chat (`1045`–`1056`) son gráficos sin función;
  - las teclas F5–F12 no hacen nada (eso llega con los atajos, en el entregable 6);
  - no hay filtros por canal, que en el original se configuran en las opciones.
- **Chat de clan y de grupo:** se envían y se decodifican (tipos 4, 11, 13 y 14), pero no se pudieron probar porque todavía no se pueden crear clanes ni grupos.

## Entregable 5: Combate ✅

![Combate](docs/capturas/5_combate.png) ![Muerte](docs/capturas/5_muerte.png)

**Funciona:**
- **Atacar:**
  - mantener el clic sobre un personaje lo convierte en objetivo (nombre en amarillo);
  - lejos, se camina hacia él; al lado, se manda `C_Attack` (op 68: id, x, y del objetivo) y la animación de ataque propia empieza enseguida;
  - el eco del servidor no la reinicia;
  - el ritmo es la duración de la animación de ataque del sprite × 1,05, porque el anti-acelerador del servidor también controla los ataques (`CheckAttackInterval`).
- **Paquetes de ataque:** `S_AttackPacket`, `S_AttackMissPacket`, `S_AttackPacketForNpc` y `S_UseAttackSkill`/`S_UseArrowSkill` (op 142: acción, atacante, objetivo, golpe y dirección) animan al atacante.
- **Daño:**
  - si el golpe acierta, el objetivo muestra la animación de daño en el frame de impacto del ataque (sufijo `!` de la tabla de sprites, que ahora se guarda);
  - el servidor no le manda a quien ataca el `S_DoActionGFX` de daño de su objetivo, así que el cliente lo deduce del golpe.
- **Muerte:**
  - `S_DoActionGFX` 8 o el estado 8 en `S_CharPack` dejan al objeto como cadáver, con la animación de morir detenida en el último frame, hasta que el servidor lo quita;
  - un cadáver no reacciona a más acciones.
- **Muerte propia:** aparece el aviso "You have died." con **Restart**, que manda `C_Restart` (op 71), descarta los objetos conocidos y espera a que el servidor reenvíe la escena.
- **Barra de vida:** se dibuja sobre los objetos cuyo HP se conoce (`S_HpMeter`, op 128).

**Verificado contra el servidor:**
- **Matar un monstruo** (conejo, gfx 998):
  - 10 `C_Attack` separados por al menos 841 ms (800 ms de animación × 1,05), sin avisos del anti-acelerador;
  - `S_DoActionGFX` con acción 8 y `S_Exp`: el personaje pasó de 0 a 5 de experiencia y luego a 10 (se repitió con el servidor recién reiniciado).
- **Morir y reiniciar:**
  - dos hombres lagarto agresivos (`.spawn 45144`) mataron a Prueba; el aviso aparece con HP 0/14 (`5_muerte.png`);
  - con `--reiniciar`, 5 s después sale `C_Restart` y el servidor responde con `S_HPUpdate`, `S_MapID`, el propio `S_CharPack` y los objetos visibles (el ciclo se repitió 3 veces).
- **Ver a otros pelear:** los ataques de los monstruos contra otro personaje se procesan, y su HP baja en vivo.
- **Sanitizers:** los tests pasan con ASan + UBSan.

**Pendiente o conocido:**
- **Barra de vida de los monstruos:** L1J-JP 3.63c no la manda (`S_NPCPack` lleva 0xFF y `S_HpMeter` solo va al grupo y las mascotas).
- **Daño:** el servidor solo indica si acertó, no el número, así que no hay cifras de daño.
- **Distancia:** resuelto más tarde; ver "Ataques con arco" abajo.
- **Sonidos de combate:** van con el audio, en el entregable 10.
- **Aviso de muerte:** no es el diálogo original.
- **NPC no monstruos:** hacer clic sobre ellos también los "ataca" (el servidor no les hace daño); hablar con NPCs llega con `C_NPCTalk` en un entregable posterior.
- **En el servidor local:**
  - Charla quedó en nivel 70 por la prueba;
  - reinicié el servidor para quitar los hombres lagarto que invoqué.

## Entregable 6: Inventario e ítems ✅

![Inventario](docs/capturas/6_inventario.png) ![Botín](docs/capturas/6_botin.png)

**Funciona:**
- **Inventario (`Inventario` en `mundo/inventario.h`, tabla columnar por id):**
  - lo llena `S_InvList` (op 180, varios eventos por paquete) y lo actualizan `S_AddItem` (63), `S_DeleteInventoryItem` (148), `S_ItemName` (195), `S_ItemAmount`/`S_ItemStatus` (127) y `S_ItemColor` (144);
  - un ítem está equipado si el servidor agrega " ($9)" (arma) o " ($117)" (armadura) a su nombre.
- **Ventana (Alt+I, `app/inventario_ui.cpp`):**
  - grilla de 6×7 con los íconos originales y desplazamiento con la rueda;
  - cantidad, marca "E" en los equipados y nombre con los `$N` resueltos al pasar el mouse (amarillo si es bendito, rojo si es maldito);
  - peso abajo;
  - doble clic manda `C_UseItem` (op 44: id + 0): equipar o quitar, beber;
  - arrastrar un ítem fuera de la ventana manda `C_DropItem` (op 54) en la celda propia.
  - Con la UI simple usa el mismo recuadro de texto que la ventana de estado (`ui::ventana`).
- **Íconos:** son los `N.tbt` de los SpriteNN.pak (2180 archivos). Tienen el mismo formato que un tile con transparencia, sin el byte de tipo (`decodificar_tbt`).
- **Ítems del suelo:**
  - llegan como `S_DropItem` con el mismo opcode y formato que `S_CharPack` (el campo de experiencia es la cantidad);
  - se reconocen por el tipo de gfx (atributo 102 de la tabla de sprites: 9 ítem, 10 monstruo, 5 personaje; los NPC son 6 y 12);
  - no tienen acciones, así que se dibuja el frame 0 de `N-0.spr`;
  - clic: si está lejos se camina hasta él y se manda `C_PickUpItem` (op 188);
  - el nombre se ve solo con el puntero encima.
- **Clic sobre el mundo según el tipo:** un ítem se recoge, un monstruo se ataca, y a otros personajes y NPCs solo con Ctrl. Antes cualquier clic atacaba, también a los mercaderes.

**Verificado contra el servidor:**
- **Equipar:** doble clic (o `--usar`) sobre la espada de una mano. Llegan `S_ItemName` "$3279 ($9)" y `S_CharVisualUpdate` con arma 4, y el sprite pasa a la variante con espada (se ve en las capturas).
- **Poción:** `.item 40010 5` y después usarla. HP de 20 a 69, `S_ItemAmount` "$26 (4)" y los mensajes "Obtained Lesser Healing Potion (5)" y "You feel better.".
- **Botín:** Charla mató dos goblins.
  - Con `AutoLoot=1` (la configuración del servidor local) el botín entra al inventario: adena 52 → 75 → 85 y una poción más.
  - En el chat aparece "고블린 gave you Adena (23)" (el servidor manda el nombre del monstruo en coreano).
- **Soltar y recoger:** al soltar llegan `S_DeleteInventoryItem` y el ítem en el suelo. Con `--recoger` el personaje camina hasta él, manda `C_PickUpItem` y vuelve con `S_AddItem`. La adena soltada se ve en el suelo con su sprite.
- **Peso:** se actualiza en vivo (`S_PacketBox` 10).
- **Sanitizers:** los tests pasan con ASan + UBSan.

**Pendiente o conocido:**
- **Ventanas:** no identifiqué las originales de inventario y estado en esta instalación; las dos usan el marco 4131 (o el recuadro simple).
- **Usos especiales:** los ítems que piden un objetivo (pergaminos de teletransporte o identificación, varitas, hechizos) necesitan campos extra en `C_UseItem`; llegan con sus entregables.
- **Cantidades:** no hay diálogo de cantidad: soltar suelta todo el montón.
- **Ítems no transferibles:** las armas "Ivory Tower" de principiante no se pueden soltar; el servidor ignora el pedido sin avisar.
- **Durabilidad y encantamiento:** se reciben dentro del nombre pero no se muestran aparte.

## Entregable 6.5: Hechizos y habilidades ✅

![Ventana de hechizos](docs/capturas/6.5_ventana_hechizos.png) ![Energy Bolt](docs/capturas/6.5_energy_bolt.png) ![Haste](docs/capturas/6.5_haste.png) ![Barra de atajos](docs/capturas/6.5_barra_atajos.png)

Adelanta la parte de uso del entregable 8 del plan: lanzar hechizos, sus efectos visuales, los efectos activos y la velocidad con haste y brave. Quedan para el 8 el arte original de la ventana y los íconos.

**Datos** (del cliente original, en `Text.pak`; `recursos/hechizos.*`):
- **Qué trae cada archivo:**
  - `spelldesc-e.tbl`: nombre y costo ("Lesser Heal (4/0)");
  - `spellexdesc-e.tbl`: la ficha que se muestra al pasar el mouse;
  - `spells`: alcance, gráfico, demora entre usos (`useDelay`) y clase.
- **Objetivo:** si el hechizo pide elegir un personaje o monstruo sale del "Targets:" de la ficha y del alcance. Coincide con la tabla `skills` del servidor en 175 de 183 hechizos; los 8 restantes son casos que el servidor resuelve solo.
- **Acción con la que se lanza:** sale de la clase. Las de ataque (`SpellAttack`, `SpellMassAttack`...) usan la 18 y las demás la 19, como `action_id` del servidor salvo en 1 hechizo. El servidor mide un intervalo distinto para cada una.

**Protocolo:**
- **Lo que se sabe:** `S_AddSkill` / `S_DelSkill` llegan como filas de bits (hechizo = fila × 8 + bit + 1).
- **Lanzar:** `C_UseSkill` (fila, columna y, si hay objetivo, id y celda).
- **Efectos visuales:**
  - el hechizo de ataque llega en el mismo paquete que el golpe (`S_UseAttackSkill`), con el gráfico del proyectil y las celdas de origen y destino; se distingue por el largo;
  - `S_SkillSound` pone el gráfico sobre un objeto, `S_EffectLocation` en una celda, y `S_RangeSkill` hace la acción del lanzador y pone el gráfico sobre cada objetivo.
- **Velocidad y efectos activos:**
  - `S_SkillHaste` / `S_SkillBrave` dan la velocidad;
  - `S_SkillIconShield` y las auras e íconos de `S_PacketBox` (subtipos 0x16, 34 y 40) dan los efectos activos.

**Interfaz:**
- **Ventana de hechizos (Alt+S):**
  - los hechizos sabidos por nivel, con su MP (gris si no alcanza) y la tecla asignada;
  - con el mouse encima se ve la ficha; clic lanza;
  - con el puntero sobre un hechizo, F5–F12 lo asigna a esa tecla.
- **F5–F12:** lanzan el hechizo asignado. Se guardan por personaje en `settings.ini` (`atajos=`).
- **Hechizo con objetivo:**
  - espera el clic sobre un personaje o monstruo (también uno mismo); clic derecho o Escape cancelan;
  - si está más lejos que el alcance, se camina hasta él y se lanza;
  - un clic nuevo en el suelo cancela el hechizo en camino.
- **Animación propia:** empieza al pedirlo (el eco del servidor no la reinicia). No se hace otra acción hasta que pasa el intervalo que mide el servidor, ni se lanza otro hechizo antes de su `useDelay`.
- **Barra de atajos (Alt+1..Alt+0, `app/barra_ui.cpp`):**
  - diez casilleros abajo al centro, sobre el panel;
  - se llena arrastrando un hechizo desde su ventana o un ítem desde el inventario (soltarlo sobre la barra no lo tira al suelo);
  - entre casilleros se arrastra para moverlos; clic derecho o soltar fuera los vacía;
  - clic o Alt+número usa: el hechizo (si pide objetivo, espera el clic) o el ítem (como el doble clic del inventario);
  - muestra los íconos originales (`spellicon.idx` de `Sprite.pak` da el `N.tbt` de cada hechizo), la cantidad de los ítems y oscurece lo que no se puede usar (sin MP, hechizo no sabido, ítem que ya no está);
  - un ítem se guarda por su id y su gfx: si la pila se acabó y se consiguió otra, se usa esa;
  - se guarda por personaje en `settings.ini` (`barra=`);
  - clic del medio sobre un casillero lo elige (marco dorado; se guarda con un `*` en `barra=`) y el botón del medio sobre el mundo usa el elegido sobre lo que hay bajo el puntero, como en el original. Un hechizo con objetivo sin nada bajo el puntero no se lanza. El usuario lo probó en vivo;
  - **mantenido, repite:** un hechizo, en cuanto termina la espera (y no hay otro en camino), sobre lo que esté bajo el puntero en ese momento; un ítem, cada 500 ms. Verificado con `--medio=322,205,600` y Lesser Heal sobre Charla: 4 `C_UseSkill` en 5 s (uno al apretar y uno cada ~1,3 s), aceptados por el servidor ("You feel better"), y ninguno después de soltar;
  - **1–0 sin Alt y chat con Enter:** con el chat cerrado (lo normal), las teclas son acciones y 1–0 usan la barra; Enter abre la línea del chat, Enter envía y la cierra, Esc la vacía y la cierra (y no cierra ventanas). En Options, "Chat: Always typing" vuelve al modo anterior (`chat_con_enter=0`). Verificado inyectando teclas (`--pulsar`): "1" lanzó Shield; "1#hola1#" abrió el chat y envió "hola1" sin que el "1" usara la barra; "#abc~" no envió nada; con `chat_con_enter=0`, "hey1#" envió "hey1". Captura: `docs/capturas/4_chat_con_enter.png`;
  - **espera entre hechizos:** el servidor (L1J) tiene una sola espera para todos los hechizos (`reuse_delay`: 10 ms o 0 en casi todos, 1 s en unos pocos como Fireball) y no avisa al cliente: un hechizo durante la espera se ignora en silencio. Lo que más pesa es la animación del lanzado, que el servidor también controla. El cliente toma la mayor de las dos y la muestra en los casilleros de hechizos (barra y F5–F12): una sombra que se vacía de arriba abajo y los segundos con centésimas. Captura: `docs/capturas/8_espera_hechizos.png`;
  - en algunos teclados Alt con un número escribe un carácter (Option+2 = "@" en el teclado español del Mac): si sale un carácter ASCII se toma como texto, no como atajo.
- **F5–F12** siguen como atajos solo de hechizos.
- **Efectos activos:** arriba a la izquierda, con lo que les queda ("Shield 29:57", "Haste 4:49").
- **Teclas movidas:** el oscurecimiento en la sombra pasó a Shift+F4 y la captura de pantalla a Shift+F12.

**Gráficos:**
- **Cómo se dibuja cada efecto:**
  - la acción 0 del gráfico, una vez, sobre su objeto o en su celda;
  - el proyectil sale en el frame de impacto del lanzador y vuela 40 ms por celda (estimado; no está en los datos);
  - el objetivo muestra el daño cuando llega el proyectil.
- **Mezcla aditiva:** el atributo 104 de `sprite_table` con el bit 8 (1921 gráficos, todos efectos) marca que se dibujan sumando su luz al fondo. Sin eso, Energy Bolt se veía con un óvalo oscuro. El pipeline pasó a alfa premultiplicado: la mezcla normal y la aditiva van en la misma llamada de dibujo.
- **Efecto encadenado:** el atributo 109 dice qué gráfico sigue al terminar, por ejemplo la explosión de Energy Bolt (219) al llegar.

**Velocidad propia:**
- **Antes:** el paso propio era fijo (672 ms).
- **Ahora:** caminar, atacar y lanzar usan la duración del sprite con los factores del anti-acelerador del servidor (`AcceleratorChecker`): × 0,75 con haste, ÷ 0,75 lento, × 0,75 con poción de brave, × 0,87 con waffle, × 0,75 al caminar con Holy Walk, × 0,65 al atacar con Bloodlust.
- **Margen:** se mantiene el 5 %, y al caminar alcanza para encadenar pasos (con haste y Holy Walk el 5 % solo no alcanzaba).

**Verificado contra el servidor local** (Charla, GM, con `.addskill`):
- **Energy Bolt sobre un goblin invocado:**
  - el servidor respondió con la acción 18 y el gráfico 167;
  - en la captura se ve el proyectil y la explosión encadenada, sin fondo oscuro;
  - el goblin murió.
- **Shield:** llegaron el gráfico 221, la acción 19 y el ícono de 1800 s, y la CA bajó 2.
- **Lesser Heal sobre uno mismo:** "You feel better", +HP y los destellos del gráfico 744.
- **Haste y Holy Walk** (`.buff me 43 300` y `.buff me 52 300`): 27 pasos a 382–384 ms, sin avisos de exceso de velocidad. La lista de efectos muestra los dos.
- **Barra de atajos** (`--barra=` y `--alt=N`): con Shield en el 1 y las pociones de curación y de velocidad en el 2 y el 3, Alt+1 lanzó Shield, Alt+2 bebió la curación (5 → 4) y Alt+3 la de velocidad (3 → 2, llegó el haste). Las pociones se guardaron sin id y se encontraron por gfx.
- **Tests:** 7 casos en `tests/test_hechizos.cpp`:
  - paquetes con los bytes reales del servidor y `C_UseSkill`;
  - proyectil y momento del daño;
  - efectos activos y `S_RangeSkill`;
  - intervalos con haste y brave;
  - clasificación del objetivo;
  - datos reales del cliente;
  - valores por personaje de `settings.ini`.

**Pendiente o conocido:**
- **Interacción manual:** elegir el objetivo con el mouse, asignar teclas y arrastrar a la barra no se pudieron automatizar; la lógica está cubierta, pero falta probarlo a mano.
- **Hechizos con campos propios:**
  - Teleport necesita el marcador (`C_UseSkill` con mapa e id del marcador) y el ida y vuelta de `S_Teleport`;
  - también Fire Wall (celda) y Call Clan/Run Clan (nombre).
  - Hoy se mandan sin esos campos.
- **Íconos:** los efectos activos se listan como texto. Falta encontrar los íconos originales (`SpellIcon.idx` según l1j-client) y la ventana de magia original (imágenes 934–968).
- **Paquetes sin manejar:** `S_Paralysis`, `S_Poison`, `S_CurseBlind` y los sonidos.
- **Velocidad de los demás:** el brave de los demás no cambia su animación; sus pasos llegan antes y se encadenan.

## Entregable 6.6: Polymorph ✅

![Diálogo](docs/capturas/6.6_polymorph_dialogo.png) ![Pergamino: death knight](docs/capturas/6.6_polymorph_pergamino.png) ![.allbuff](docs/capturas/6.6_polymorph_allbuff.png)

**Antes:**
- **`S_ChangeShape` se ignoraba:** quien se transformaba seguía con su sprite.
- **Riesgo de desconexión:** el servidor mide caminar, atacar y lanzar con el sprite de la forma actual (`getTempCharGfx`), y el cliente seguía usando el original. Con una forma más lenta, el cliente mandaba los pasos demasiado rápido y el servidor lo desconectaba ("Overspeed detected").
- **El pergamino no servía:** se mandaba como una poción, sin el nombre del monstruo.

**Ahora:**
- **`S_ChangeShape` (opcode 164):**
  - cambia el gfx del objeto y, si se sacó el arma, también su estado;
  - con eso cambian su animación, la duración de sus pasos y los intervalos propios (caminar, atacar, lanzar), que ya se calculaban con el sprite actual.
- **Diálogo de polymorph (`app/forma_ui.cpp`):**
  - **cuándo se abre:** al usar un pergamino (uso 16; con doble clic, desde la barra o con `--usar`);
  - **cómo funciona:** como en el original, se escribe el nombre del monstruo, en inglés como en `PolyTable` del servidor ("death knight");
  - **sugerencias:** las formas de `polymorph.tbl` que contienen lo escrito; clic en una la elige;
  - **botones:** Undo vuelve a la forma normal ("none") y Cancel o Escape cierran. Mientras está abierto, lo escrito no va al chat.
- **Respuestas al servidor:**
  - el pergamino responde con `C_UseItem` (id + nombre);
  - Shape Change (hechizo 67, "Polymorph" en los datos del cliente) llega como la página HTML "monlist" (`S_ShowHTML`, opcode 119) y se responde con `C_NPCAction` (id propio + nombre).

**Verificado contra el servidor local** (Charla, GM):
- **Pergamino** (`.item 40088 3`, `--usar=Polymorph --forma="death knight"`): llegó el polymorph al gfx 3784 y Charla se ve como death knight. Caminó 20 pasos a 666–683 ms, sin avisos de velocidad.
- **Duraciones:** las de 3784 en la tabla de sprites coinciden con `spr_actions` del servidor (caminar 640, atacar 600, hechizos 880/960 ms).
- **`.allbuff`** (polymorph 5641, haste y brave): pasos de 191–201 ms, sin avisos (el servidor exige 180 × 0,97). Antes el cliente caminaba a 672 ms con esa forma. La lista de efectos muestra los seis buffs.
- **Shape Change** (agregado a Charla en la base local): el servidor mandó "monlist", el diálogo respondió "death knight" y llegó el polymorph.
- **Tests:** `S_ChangeShape` con los bytes reales, el cambio de sprite y de paso, el arma, "monlist" y los dos comandos de respuesta.

**Pendiente o conocido:**
- **Restricciones de equipo:** el servidor saca o bloquea armas y armaduras según la forma (`weapon_equip` / `armor_equip`), y el cliente solo refleja el arma que se saca.
- **Lista de formas:**
  - `polymorph.tbl` tiene 97 formas (las "re ..." y "death 52"...); el servidor acepta 298, y las otras se escriben a mano;
  - no se filtra por nivel; el servidor responde con un mensaje si no se puede.
- **Duración:** no se muestra cuánto le queda al polymorph; el servidor no manda un ícono con la duración.
- **Ventana HTML:** "monlist" usa el mismo diálogo; la ventana HTML original llega con el entregable 7, que también traerá la página "RaiseAttr" que manda el servidor al entrar.

## Entregable 7: NPCs y comercio ✅

![Diálogo](docs/capturas/7_dialogo_npc.png) ![Comprar](docs/capturas/7_comprar.png) ![Vender](docs/capturas/7_vender.png) ![Almacén](docs/capturas/7_almacen.png) ![Teletransporte](docs/capturas/7_teletransporte.png)

**Hablar con NPCs:**
- **Clic sobre un NPC** (no monstruo, ítem ni personaje): se camina hasta 2 celdas y se le habla (`C_NPCTalk`), aunque se suelte el botón. Ctrl+clic sigue atacando.
- **La página:** el servidor responde con su id (`S_NPCTalkReturn`, con los datos de `<var>`) y se muestra la página `<id>-e.html` de `Text.pak` en el diálogo, arriba a la izquierda, con desplazamiento.
- **Cierre:** con Escape, con [x] o al alejarse más de 6 celdas. El servidor la cierra con una página vacía.

**HTML** (`recursos/html.*`):
- **Qué se interpreta:** un subconjunto, sacado de las 6499 páginas en inglés:
  - `<font fg>`, `<p align>`, `<center>`, `<br>` y entidades;
  - `<a link>` (otra página, sin pasar por el servidor) y `<a action>` (`C_NPCAction`);
  - `<img src="#N">` (botones `N.img`, con `N+1` resaltado, y tooltip);
  - `<var src="#N">` (datos del servidor) y `<username>`.
- **Resto:** se ignora.
- **Maquetación:** sin GPU, con corte de líneas por ancho, centrado y vínculos. Un test maqueta todas las páginas.
- **Ejemplo real:** al entrar, el servidor manda "RaiseAttr" (el reparto de puntos de atributo de Charla) y ya se ve con sus vínculos.

**Tienda y almacén** (en la misma ventana):
- **Listas:**
  - comprar: `S_ShopSellList`, con índice, ícono, precio y nombre;
  - vender: `S_ShopBuyList`, que solo trae id y precio; el resto sale del inventario;
  - retirar: `S_RetrieveList`;
  - guardar: lo resuelve el cliente con el propio inventario, sin lo equipado, porque la acción "deposit" no tiene respuesta del servidor.
- **Elegir:** clic suma 1, Shift+clic suma 10 (o todo) y clic derecho resta (con Shift, nada). El pie muestra el total, la adena que se tiene o la comisión del almacén (30 por ítem).
- **OK:** manda `C_Result` (tipo 0 comprar, 1 vender, 2 guardar, 3 retirar; lista de id y cantidad).

**Teletransporte:** el servidor local tiene `SendPacketBeforeTeleport=true`. Manda `S_Teleport` y recién mueve al personaje cuando recibe `C_Teleport`. Antes el cliente no respondía y cualquier teletransporte quedaba trabado (también el del hechizo y los pergaminos).

**Verificado contra el servidor local** (Charla; `--hablar`, `--npc-accion` y `--tienda`):
- **Hablar con Fraoun** (mercader): se camina hasta él y se ve su página; "Weapons" y los demás vínculos cambian de página localmente.
- **Comprar:** 2 Lesser Healing Potion, la fila 0, a 42 cada una. Adena 20 083 → 19 999, pociones 5 → 7. Antes, con 83 de adena, el servidor contestó el mensaje 189 (adena insuficiente).
- **Vender:** 3 pociones. Pociones 7 → 4, adena +54 (3 × 18).
- **Almacén** (Karim, invocado con `.spawn 60007 1 2`):
  - guardar 1 Haste Potion: 3 → 2;
  - retirar: la lista la muestra, se retira (2 → 3) y se cobran 30 de adena.
- **Teletransporte con Illdrath** ("teleport talking silverknight-in"): efecto, `S_Teleport` → `C_Teleport` → mapa 4. Charla aparece en Silver Knight Town. Volvió con `.move`.
- **ASan:** sin errores en el flujo de hablar, navegar y comprar.
- **Tests** (`tests/test_npc.cpp`): `S_NPCTalkReturn` con datos, `S_Teleport`, las tres listas, `C_NPCTalk`, `C_Result` y `C_Teleport`, la maquetación de una página tipo fraoun y las 6499 páginas reales.

**Arreglos posteriores** (al probarlo a mano):
- **Teletransportadores del pueblo de inicio:**
  - **Illdrath:** su página (`illdrath1`, igual en inglés y en coreano) ofrece "llevame a Silver Knight Town" con la acción `teleport hidden-velley-for-newbie`. El servidor de pruebas (L1J-JP 3.63c, más viejo que los datos de esta instalación) tiene esa acción solo para otro NPC (50004), así que el clic no hacía nada.
    - **Arreglo:** en el servidor local, `data/xml/NpcActions/Teleporter.xml` acepta la acción también para Illdrath (`NpcId="50003,50004"`).
    - **Verificado:** hablar → acción → `S_Teleport` → `C_Teleport` → mapa 4.
  - **Barnia, Karen, Asha y Feaena:** llevan solo a su clase (elfos, elfos oscuros, ilusionistas y caballeros dragón). A las demás clases el servidor les manda la página de rechazo; eso es lo esperado.
  - **Beginner helper:** sus páginas por clase y nivel (`tutorp1`… con los teletransportes H a K) no existen en esta instalación. Su página inicial solo tiene "Thank you." (acción "l"), que el servidor no usa. Es una diferencia de datos entre versiones y queda sin arreglo.
- **Mapa que el cliente no tiene** (Asdf quedó "en un estado raro"):
  - **Qué pasó:** Asdf usó el teletransportador a Singing Island (NPC 71035, `teleport si-in`). El servidor lo mandó al mapa 8011, que esta instalación no trae (`map/8011/` no existe). Comparando los bloqueos del servidor con todos los mapas del cliente, ninguno pasa del 76 % (el 2005 con el suyo da más del 95 %).
  - **Cómo se veía:** todo negro, con los NPCs flotando, y sin poder caminar, porque sin el mapa ninguna celda es transitable. Reiniciar tampoco servía: el lugar de retorno de esa isla es la isla misma.
  - **Arreglo en el cliente:**
    - si el servidor manda a un mapa que la instalación no tiene, avisa en el chat;
    - la ventana **Exit** (botón de la puerta en el panel original, "Exit" en la UI simple) tiene "Restart" (`C_Restart`: vuelve al lugar de retorno de la zona, vivo o muerto; el servidor deja el HP en el nivel) y "Quit".
  - **Arreglo en el servidor local:**
    - el teletransporte a Singing Island queda desactivado en `Teleporter.xml`;
    - el lugar de retorno de la isla y sus mazmorras (8011, 85, 87) pasa a Silver Knight Town (`corregir_db.sql`).
    - Asdf se movió en la base al punto de inicio, con HP y MP al máximo (habían quedado 128/63 y 71/56).
  - **Verificado:** `.move` al mapa 8011 → aviso en el chat → Exit → Restart → mapa 4.
- **Hablar con el arco equipado:** con el alcance de 18 celdas del arco, el clic sobre un NPC lo atacaba en vez de caminar hasta él. Ahora a un NPC al que se va a hablar nunca se lo ataca.
- **RaiseAttr** (subir un atributo desde el nivel 51): los vínculos de la página son `raise-str`, `raise-dex`... El servidor no los atiende con `C_NPCAction`: el cliente original los manda como `C_Attr` (opcode 61) con el código 479, un 1 y el atributo ("str").
  - **Ahora:** el cliente los traduce y cierra la página.
  - **Verificado:** con `--subir=str`, la fuerza de Charla pasó de 21 a 22 en la base y llegó `S_OwnCharStatus2`.
  - **Límite del servidor:** vuelve a mandar la página solo al entrar o al subir de nivel, no después de elegir. Así que los puntos pendientes se reparten de a uno por sesión.

**Pendiente o conocido:**
- **Nombres en la tienda:** el servidor manda los nombres de lo que vende tal como están en su base de datos. La del servidor local está en coreano, y así se ven. El cliente original hace lo mismo; con un servidor en inglés llegarían en inglés. En el inventario el servidor manda referencias ("$26") y se ven en inglés.
- **Cantidades:** no hay diálogo para escribir la cantidad; se elige con clics.
- **Otros almacenes y comercios:** almacén de clan, de elfos y adicional, tiendas privadas, fabricación (`S_HowManyMake`) y `S_InputAmount`.
- **HTML:** se ignoran `<b>`, tamaños y caras de letra. No hay páginas en otros idiomas: siempre se usa `-e`.
- **Silver Knight Town:** se ven manchas blancas y rosadas junto a las cercas (piezas del mapa translúcidas). Falta revisar si deberían dibujarse de otra manera.

## Entregable 8: Magia y habilidades ✅

![Íconos de efectos](docs/capturas/8_iconos_efectos.png) ![Ventana de magia original](docs/capturas/8_ventana_magia_original.png) ![Caballero con haste](docs/capturas/8_caballero_haste.png)

Completa lo que el paso 6.5 había adelantado (lanzar, efectos visuales y aditivos, velocidad con haste y brave).

**Íconos de los efectos activos** (antes, texto):
- **Dónde:** una fila arriba al centro, sobre las ventanas, con el tiempo restante debajo ("5m", "45s"; los minutos redondeados hacia arriba).
- **Detalles:** parpadea en los últimos 10 s; con el puntero encima, el nombre y el tiempo exacto.
- **De dónde sale el ícono:**
  - del hechizo que lo da (`spellicon.idx`), según el paquete: haste o lento, escudos, auras, Holy Walk, Immune to Harm;
  - o del ítem: poción de brave (79), waffle de elfo (130), poción azul (75).
  - Sin ícono, las primeras letras del nombre.

**Ventana de magia original** (Alt+S con la UI original):
- **Partes:**
  - el marco 934 con una grilla de 4 × 6 íconos (celdas de 34 px desde (37, 43), medidas en la imagen);
  - la tira de pestañas 936 + n: niveles 1 a 10 y las habilidades de clase;
  - SP y MR en los recuadros de abajo.
- **Uso:** como la lista de la UI simple. Clic lanza, se arrastra a la barra o a una tecla F, F5–F12 asignan, la ficha se ve con el puntero encima y los íconos se oscurecen sin MP suficiente.
- **Casilleros F5–F12 del panel original** (1019, 2 × 4 de 34 px): muestran los hechizos asignados. Clic lanza, clic derecho vacía, y se les puede soltar un hechizo arrastrado.

**Verificado** (lo que pide el plan, con personajes nuevos y sin GM):
- **Personajes de prueba:** se crearon con `--crear --clase=3` (mago) y `--clase=1` (caballero). Para preparar la prueba fueron GM un momento en la base local (`.addskill` y `.item`); después se les quitó.
- **Maga (nivel 1) lanza Energy Bolt sobre un goblin:** `C_UseSkill`, el servidor responde con la acción 18 e impacto, y el MP baja de 6 a 3. Los íconos de la ventana se oscurecen sin MP.
- **Caballero toma una poción de velocidad:** `S_SkillHaste` de 300 s; el ícono de haste dice "5m" y las pociones bajan de 3 a 2. Camina 24 pasos a 500–509 ms (antes 672), sin avisos del servidor.
- **`.allbuff` con Charla:** seis íconos (haste, poción de brave, dos auras, Burning Weapon, Iron Skin) con sus tiempos.

**Pendiente o conocido:**
- **Teleport:** el hechizo sin marcador no se pudo probar porque Maga no tiene MP suficiente (cuesta 10). Los marcadores (`C_UseSkill` con mapa e id) faltan.
- **Pestañas de la ventana original:** hay tres juegos de tiras (936–946, 947–957, 958–968); se usa el primero para todas las clases.
- **Recuadros de abajo:** que muestren SP y MR es una suposición (la imagen 935 tiene los rótulos "S.power" y "M.resist").
- **Estados sin manejar:** `S_Paralysis` (no bloquea el movimiento), `S_Poison` (sin tinte verde) y `S_CurseBlind` (sin oscurecer la pantalla).
- **Karim:** el enano invocado en el entregable 7 quedó en el punto de inicio y tapa a quien aparece ahí, hasta que se reinicie el servidor.

## Entregable 9: Social ✅

![Grupo e intercambio](docs/capturas/9_grupo_intercambio.png) ![Clan creado y título](docs/capturas/9_clan_titulo.png) ![Página del clan y emblema](docs/capturas/9_clan_emblema.png)

**Preguntas del servidor** (`S_MessageYN`): el texto sale de string-e.tbl con sus argumentos, con botones Yes/No (Enter/Escape). La respuesta va en `C_Attr` (h 0, d número, h tipo, h 1/0).

**Grupo:** `/invite` (o el botón con un personaje marcado) manda `C_CreateParty`; `/party` pide la página "party" (líder y miembros); `/leave` y `/kick`.

**Intercambio:**
- **Cómo empieza:** `C_Trade` no lleva a quién; el servidor elige al que está al lado y enfrente (por eso `--girar`).
- **Ventana:** dos grillas de 6 × 3 (lo propio y lo del otro, con íconos y nombres), OK y Cancel. Después de OK dice "Waiting..." hasta `S_TradeStatus`.
- **Ofrecer:** soltando un ítem del inventario sobre la ventana, o doble clic mientras está abierta. Se ofrece la pila entera.

**Clan:**
- **Comandos:** `/clan` crea (`C_CreateClan`), `/join` pide entrar al príncipe que se tiene enfrente, `/pledge` abre la información, `/leaveclan` y `/banclan`.
- **Página del clan:** `S_Pledge` manda 3 datos (clan, conectados, todos), pero las páginas "pledge" y "pledgeM" leen 5 (`#1` y `#3` son las cantidades). El cliente las cuenta y reordena.
- **Emblema:** se pide una vez por clan con `C_Clan` y llega en `S_Emblem` (16 × 12 píxeles RGB555); se dibuja a la izquierda del nombre. `/emblem` sube uno de muestra (el original lo carga de un archivo).

**Títulos y alineamiento:** el título (`S_CharTitle`) va arriba del nombre. El nombre de los jugadores es azul con alineamiento ≥ 500, rojo con ≤ −500 y blanco en el medio; el objetivo sigue en amarillo.

**Verificado** (dos clientes a la vez contra el servidor local):
- **Grupo:** Charla (`--decir="/invite Prueba"`) invita a Prueba (`--aceptar`), que recibe la pregunta 953 y acepta. "/party" muestra "Leader: Charla" con los dos miembros.
- **Intercambio:**
  - Charla ofrece sus 2 Lesser Healing Potion (`--ofrecer`) y los dos dan OK. Llega `S_TradeStatus` 0 ("The trade is complete").
  - En la base, Prueba pasa de 2 a 4 pociones y Charla queda sin ninguna.
- **Clan:**
  - Charla crea "Prueberos" (adena con `.item`) y se pone el título "Leader", que se ve sobre su nombre.
  - Prueba hace `/join` frente a ella; Charla acepta la pregunta 97 y Prueba entra al clan.
  - "/pledge" de Prueba muestra el clan y "Charla Prueba" como conectados.
  - Charla sube el emblema y Prueba lo recibe por `S_Emblem` y lo ve junto a su nombre.
- **Tests:** decodificación de pregunta, intercambio, título, emblema y página del clan, y bytes de los comandos.

**Pendiente o conocido:**
- **Servidor:** no traía la carpeta `emblem/` y `C_Emblem` fallaba. `arrancar.sh` ahora la crea.
- **Emblema propio:** no hay selector de archivo; `/emblem` sube uno fijo.
- **Al crear o entrar a un clan:** el servidor no reenvía el `S_CharPack`, así que el emblema del propio personaje aparece después de reaparecer (al volver a entrar o teletransportarse).
- **Sin probar:** alianzas, guerras de clan y el reparto automático del grupo.
- **Datos de prueba:** Charla y Prueba quedaron en el clan Prueberos en la base local.

## Entregable 10: Mundo completo ✅

![Lluvia](docs/capturas/10_lluvia.png) ![Giran con nieve](docs/capturas/10_giran_nieve.png) ![Portal a la mazmorra](docs/capturas/10_portal_mazmorra.png) ![Opciones con volumen](docs/capturas/10_opciones_volumen.png)

**Audio** (módulo nuevo `src/audio`, sobre SDL3):
- **Arquitectura:** un dispositivo y varios streams que SDL mezcla: música, ambiente y 16 para efectos. Si todos los de efectos están ocupados, se corta el más viejo.
- **Música** (`sound/music<n>.mp3`): se decodifica de a poco con dr_mp3, que es la única dependencia nueva.
  - Fuera del juego suena la del login.
  - Dentro, la de la zona según `music.def` de Text.pak. Vale la primera zona que contiene la posición, así los pueblos ganan al campo que los rodea. Se respeta la pausa `INTERVAL` entre repeticiones.
  - Muerto, el tema "Death".
  - El valle escondido de esta versión (mapa 2005) usa las zonas del 69, del que es copia.
- **Efectos:** los frames de la tabla de sprites con sonido (`[n`, `<n`) suenan al empezar a dibujarse: golpes, daño, muerte y los efectos de hechizos. El volumen baja con la distancia al personaje y es cero a 20 celdas. El mismo sonido no se repite dentro de 60 ms.
- **Ambiente** (`envsound.def`): el sonido en bucle de la zona y, de cada grupo, uno al azar cada tanto. Cambia de día a noche con la hora del juego (noche fuera de 6:00–17:59, como el servidor).
- **Volumen:** música y efectos en la ventana de opciones (de 20 en 20 %, "Off"), guardados en `settings.ini`.
- **`--mudo`:** todo corre igual pero sin volumen. Las pruebas automáticas lo usan, y `--perf` muestra los efectos tocados y la música vigente.

**Clima** (`S_Weather`: 1–3 nieve, 17–19 lluvia; ".weather N" con un GM):
- **Dibujo:** gotas en diagonal o copos que se mecen, más un velo oscuro con lluvia.
- **Sin estado:** cada partícula es función del tiempo y de su número.
- **Dónde:** solo al aire libre (mapas 0, 4, 68, 69, 70 y 2005); en las mazmorras no.

**Portales:**
- **El problema:** el servidor teletransporta si el destino de un paso es un portal de su tabla, pero el mapa del cliente marca casi todas esas celdas como bloqueadas (393 de 1305) y ningún atributo las distingue: el 0x08 lo tienen 39 de 74 portales del mapa 4 y también 3857 celdas que no lo son.
- **La solución:** al intentar entrar a una celda bloqueada vecina, con el teclado o con un clic justo sobre ella, se manda el paso sin moverse aquí (una vez por segundo por celda). Si es un portal, llega el teletransporte. Si no, no pasa nada visible: el servidor toma el origen del paso siguiente del propio paquete.

**F1:** ayuda con todas las teclas. F2–F4 y F5–F12 ya existían (interfaz, depuración, cámara y hechizos asignados).

**Verificado:**
- **Música por zona:** Charla recorrió, con `.move`, Gludio, Kent, Giran, Oren, Silver Knight Town, Woodbeck, Heine, el pueblo élfico, Aden, el Valle del Dragón y las mazmorras de Talking Island y de Gludio. En cada lugar la música cambió a la de la zona (83, 14, 20, 54, 57, 52, 23, 13, 41, 19, 63 y 65), sin avisos.
- **Presupuestos** (`--perf`):
  - CPU de 0,4 a 1,2 ms por frame, con picos de 4,7 ms;
  - 120 fps, salvo el primer segundo de cada mapa (92 fps, con 18–27 ms de decodificación);
  - sin asignaciones de memoria en el frame después de cargar.
- **Combate con sonido:** 8 efectos en un combate. El valle escondido elige la música 53 y Talking Island la 30.
- **Portal:** desde (32477, 32852) un paso al norte entra a la mazmorra de Talking Island (`S_MapID` 1, música 63).
- **Clima:** `.weather 19` y `.weather 3`: lluvia y nieve en Talking Island y Giran, y nada en la mazmorra de Gludio.
- **Tests:** `tests/test_sonidos.cpp` cubre `music.def` y `envsound.def`, de muestra y los reales.

**Prueba de oído:** el usuario confirmó en vivo que la música, el ambiente y los efectos se escuchan bien.

**Monstruos sin sprite** (`tipo_objeto` en `app/npc_ui.cpp`):
- **Qué son:** 30 de los monstruos de la base de esta versión usan un gfx que falta en la tabla de sprites y en los `.pak` (Depraved Soul, gfx 8457, y los de las islas nuevas). Se dibujan como un bloque magenta.
- **Antes:** el bloque era de 8×8 y no se podía elegir. Además, como su gfx no tiene tipo (atributo 102), se lo trataba como un NPC: el clic caminaba hasta él para hablarle.
- **Ahora:** el bloque es del tamaño de un cuerpo (24×36), entra en la lista de cosas clicables, y un gfx sin acciones en la tabla cuenta como monstruo: el clic lo ataca y el cursor es la espada.
- **Verificado:** `.spawn 46186` junto a Charla y un clic simulado (`--arrastrar`): caminó hasta él y atacó (3 efectos de ataque). Captura: `docs/capturas/10_monstruo_sin_sprite.png`.

**Techos y árboles translúcidos** (`dibujar_objetos_mapa` en `app/pantalla_juego.cpp`):
- **Qué hace:** si una pieza de un objeto del mapa (casa, árbol, muro) está delante del personaje y le cubre el centro del cuerpo, todo lo que ese objeto tiene delante del personaje se dibuja translúcido (alfa 110). El interior y la pared de atrás siguen opacos: al entrar a una casa se ve adentro.
- **Silueta:** cuando algo lo tapa, el personaje se dibuja otra vez encima de todo con alfa 120. Varias piezas translúcidas superpuestas casi lo escondían.
- **Objetos:** el `.s32` trae las piezas agrupadas por objeto. Al cargar el segmento se guarda, por objeto, su rango de piezas y su rectángulo de celdas (`ObjetoMapa`).
- **Verificado:** dentro de la posada de Talking Island (32600, 32928) y detrás de una palmera (32589, 32940). Capturas: `docs/capturas/10_techo_posada.png` y `10_arbol_translucido.png`.

**Pendiente o conocido:**
- **Paneo:** los efectos no se panean a izquierda o derecha.
- **Sonidos sin conectar:** los de equipar (`equipsound.def`) y los de la voz del personaje (`charsound.def`) faltan.
- **Mapas "al aire libre":** la lista es fija; otros mapas abiertos (Aden nueva, islas nuevas) no tienen clima.
- **Monstruos sin sprite:** se pueden atacar, pero siguen siendo un bloque magenta: no hay de dónde sacar su dibujo en esta instalación.
- **Transparencia:** es inmediata (sin fundido) y vale para el objeto entero: un muro largo de un pueblo se vuelve translúcido en todo su largo.
- **Monstruos de prueba:** quedaron en el pueblo de Talking Island un Cotrot y un Depraved Soul invocados con `.spawn`; desaparecen al reiniciar el servidor.
- **Personajes al final:** Charla quedó de vuelta en el valle (2005) y el clima del servidor, despejado (`.weather 0`).

## Puertas, estados alterados, cantidad y mascotas

Primera tanda de la revisión de opcodes (`docs/analisis/OPCODES_PENDIENTES.md`).

- **Puertas** (`S_Door`, `C_Door`):
  - una puerta llega como cualquier objeto (`S_CharPack`, gfx de tipo 8) con su estado: 28 abierta, 29 cerrada. Se dibuja quieta en el último frame de esa acción, y `S_DoActionGFX` 28/29 la cambia;
  - `S_Door` dice si la celda de la puerta (y las de su ancho) se puede pasar: las cerradas quedan en `Mundo::puertas_cerradas` y `celda_pasable` las bloquea;
  - clic sobre una puerta: se camina hasta 2 celdas y se manda `C_Door` (las de llave la consumen en el servidor);
  - **verificado:** la puerta de la posada de Talking Island (32597, 32934): `C_Door`, el servidor responde con la acción 28 y `S_Door` pasable, y se ve abierta (`docs/capturas/11_puerta_abierta.png`).
- **Estados alterados:**
  - `S_Paralysis`: parálisis, sueño, congelado, aturdido y atado. Mientras dura, el personaje no se mueve ni actúa y arriba se lee el estado. El fin solo quita el estado que empezó;
  - `S_Poison` (y los flags de `S_CharPack`): verde si está envenenado, gris si congelado, para cualquier personaje o monstruo;
  - `S_CurseBlind`: velo negro sobre el mundo (con Floating Eye, menos), sin nombres, y el propio personaje encima;
  - **verificado:** la ceguera en vivo con una Blinding Potion (`docs/capturas/11_ceguera.png`); parálisis y veneno con bytes en `tests/test_proto.cpp`.
- **Diálogo de cantidad** (`S_InputAmount`, `C_Amount`; `app/cantidad_ui.cpp`): número con el teclado, −/+, la rueda y "Max"; con precio, el total. Responde con la acción que mandó el servidor. **Verificado:** en la posada, "room" pidió cantidad (300 adena cada llave, 1 a 8); con 2, el servidor cobró 600 y dio 2 llaves.
- **Dar ítems** (`C_GiveItem`): soltar un ítem del inventario sobre un NPC, monstruo o mascota se lo da; de una pila, el diálogo pregunta cuántos. **Verificado:** 5 de adena a una invocación (39.295 → 39.290), y aparecen en su bolsa.
- **Mascotas** (`app/mascotas_ui.cpp`):
  - `S_PetCtrlMenu` (opcode 33, compartido con `S_CharReset`: se distinguen por el primer byte) agrega o quita una mascota o invocación de la lista, a la izquierda de la barra de atajos, con su vida;
  - clic en el nombre le habla (menú del servidor: atacar, defender, quedarse, dispersar...); "Bag" pide su inventario (`C_PetMenu` → `S_PetInventory`, que llega como lista de almacén con tipo 0x0b), y un clic en un ítem lo usa o equipa (`C_UsePetItem`);
  - "Attack" hace que el servidor mande `S_SelectTarget`: el próximo clic elige el objetivo (`C_SelectTarget`); clic derecho o Esc cancelan;
  - `S_TrueTarget`: una marca roja y el mensaje sobre el objetivo, 16 s;
  - **verificado** con `.summon 45034` (Shepherd): la lista, su menú, `S_SelectTarget` → `C_SelectTarget`, y la bolsa con lo que se le dio (`docs/capturas/11_mascota_menu.png`, `11_mascota_bolsa.png`), también con ASan.
- **Domesticar** (lo pidió el usuario): darle Floating Eye Meat (40057) a un animal domesticable con menos de un tercio de su vida. **Verificado** con Aprendiz (mago nivel 1, cuenta GM) contra un oso invocado (45040) en la plaza de Talking Island: Energy Bolt cada 3 s y la carne cada 2 s con `--dar="Eye Meat@Bear" --cantidad=1`. Tras varios "You failed to tame it", el servidor dio el amuleto ("Pet Amulet [Lv.5 Bear]") y `S_PetCtrlMenu` lo agregó a la lista (`docs/capturas/11_mascota_domesticada.png`). Dos lecciones: junto a la casa de Gereng los hechizos no salían (el servidor exige línea de vista: `glanceCheck`), y con lobos de pruebas anteriores `--dar` elegía al más cercano, no al herido.
- **Summon Monster y el anillo de control** (Ring of Summon Control, 20284): sin el anillo, el servidor invoca una criatura según el nivel; con él, manda la página "summonlist" con el id del propio personaje (imágenes por nivel 28–72 que llevan a subpáginas con las criaturas). Elegir una manda `C_NPCAction` con su número y el servidor la invoca si el nivel y el carisma alcanzan (8 por criatura). No hizo falta código nuevo: el visor de HTML ya tenía imágenes con `link`, `action` y `tooltip`. **Verificado** con Aprendiz (subido a nivel 30 con `.level`, curado con `.ress` porque el hechizo cuesta HP): "Level 28" → Hobgoblin, acción "7", y el Hobgoblin aparece a su lado y en la lista de mascotas (`docs/capturas/11_summon_lista.png`, `11_summon_hobgoblin.png`). `--npc-accion` no sirve en esta página (ignora a propósito las que llegan con el id del propio personaje, como RaiseAttr); se recorrió con clics simulados.
- **Corregido de paso:** dos accesos fuera de rango que el tipo nuevo de lista habría disparado (el arreglo de tipos de tienda y el de títulos de la ventana), ahora con `static_assert`.

**Pendiente o conocido:**
- Con la mascota fuera de la vista, la lista muestra el nombre que manda `S_PetCtrlMenu`, que en este servidor está en coreano.
- `C_SelectTarget` manda el tipo 0 (el servidor no lo usa).
- El índice de la fila de `C_UsePetItem` es el orden de la lista; si el servidor salteara un ítem nulo, no coincidiría.
- No hay ventana propia de mascota como en el original: lista, menú del servidor y bolsa.
- **Estado del servidor de pruebas:** Charla quedó en la posada de Talking Island con 2 llaves y 605 adena menos; hay dos invocaciones Shepherd que se van solas con el tiempo o al reiniciar el servidor.

## Reparar armas, marcadores y cambiar de personaje

- **Reparar armas** (`S_SelectList`, `C_FixWeaponList`, `C_SelectList`): el vínculo `action="fix"` de los herreros (está en las páginas del juego: kuen1, randith1, cuse1, batr1...) pide la lista (`C_FixWeaponList`); `S_FixWeaponList` llega como una lista de tienda (`LISTA_REPARAR`) y la ventana del NPC muestra cada arma dañada con su daño y costo (200 adena por punto). Un clic la repara (`C_SelectList`: arma y herrero, a 3 celdas o menos). **Verificado:** con la daga de Charla en daño 3 (puesto en la base), Blacksmith Kuehn en Gludio (32633, 32801): la ventana "Repair" ("Damage 3: 600 adena"), el clic, 600 adena menos y daño 0 en la base (`docs/capturas/11_reparar.png`).
- **Volver a la selección de personaje** (`C_ChangeChar`): botón "Character select" en la ventana Exit. El servidor saca al personaje (`S_PacketBox` 42) y manda la lista de personajes; el cliente limpia el mundo al recibirla. **Verificado:** salir y volver a entrar con Charla (`docs/capturas/11_cambiar_personaje.png`).
- **Marcadores** (`S_Bookmarks`, `C_AddBookmark`, `C_DeleteBookmark`; `app/marcadores_ui.cpp`): "/bookmark nombre" en el chat guarda el lugar, "/delbookmark nombre" lo borra. Con marcadores, el pergamino de teleport (usos 6 y 29: `C_UseItem` con mapa y marcador) y los hechizos Teleport y Mass Teleport (`C_UseSkill` con mapa y marcador) abren la lista de destinos ("Random" o un marcador; clic derecho borra uno). Sin marcadores, el pergamino va al azar como antes (antes se mandaba sin mapa ni marcador).
  - **El servidor de pruebas no los permite:** ningún mapa de la base tiene `markable` = 1 ("You cannot bookmark this location", también para un GM), y `C_LoginToServer.bookmarks()`, que manda los guardados al entrar, existe pero nunca se llama. Se verificó en vivo que "/bookmark" manda `C_AddBookmark` y que el pergamino sin marcadores teletransporta al azar; el decodificador y los comandos, con tests.

## Atacar manteniendo el clic y tecla A

- **Antes:** mientras se mantenía el clic, el objetivo se volvía a calcular en cada paso con lo que hubiera bajo el puntero. Al caminar hacia un monstruo lejano la cámara se movía, el puntero quedaba sobre el suelo y el personaje empezaba a caminar en otra dirección. Además, el frame en que se apretaba se perdía si el personaje estaba a mitad de un paso.
- **Ahora** (`mover_jugador`, `app/pantalla_juego.cpp`):
  - lo que está bajo el puntero al apretar (monstruo, ítem, NPC) queda fijo como objetivo mientras se mantiene el botón; apretado sobre el suelo se camina hacia el puntero y lo primero que el puntero toque queda fijo;
  - la elección se hace antes de esperar el fin del paso;
  - **tecla A** (con el chat cerrado): ataque sostenido al objetivo, o al monstruo bajo el puntero, sin mantener el botón. Termina si el objetivo muere o desaparece, al caminar con las flechas o con un clic nuevo en el mundo.
- **Verificado** con un Jack-O-Lantern (45166) en la plaza de Talking Island y `--mantener-sobre` (el puntero queda fijo donde estaba el monstruo al apretar):
  - sin arma (alcance 1), 5 celdas: 4 pasos hacia el monstruo con la cámara moviéndose y 2 ataques;
  - con el arco (alcance 18): ataca desde donde está;
  - clic de 30 frames + A: 5 pasos y 9 ataques en 10 s después de soltar; el mismo clic sin A: 1 paso y nada más.

## Almacén, forzar ataque y modo PK

- **Almacén (regresión):** el NPC del almacén de Talking Island (Storage^Dorin, gfx 10669) no está en la tabla de sprites, y desde el cambio de los monstruos sin sprite contaba como monstruo: el clic lo atacaba. Ahora un gfx desconocido cuenta como monstruo salvo que el nombre sea "Oficio^Nombre", como el de los NPCs de los pueblos (ningún monstruo sin sprite de esta base lo tiene). **Verificado:** el clic le habla y abre su página.
- **Forzar ataque (Ctrl+clic, también ⌘):** ya estaba; verificado con `--ctrl` sobre el almacén: `C_Attack` aceptado por el servidor (gastó una flecha y respondió el ataque).
- **Modo PK** ("/pk", del cliente: el servidor no tiene ese comando): el clic sobre otro jugador lo ataca sin Ctrl (también la tecla A), con el cursor de espada e indicador "PK". **Verificado** con dos clientes en el mapa 2005: Prueba en segundo plano y Charla con "/pk" y un clic sobre él: 2 ataques (sin daño: zona segura) (`docs/capturas/9_modo_pk.png`).

## Aprender magia: maestros y libros

![Lista del maestro de magia](docs/capturas/8_aprender_magia.png)

**Maestro de magia** (Gereng en Talking Island, Siriss, Sedia...):
- **Por qué no andaba:** sus páginas tienen el vínculo `action="buyspell"`, que el cliente original no manda como `C_NPCAction` (el servidor lo ignora) sino como `C_SkillBuy` (d NPC).
- **Lista:** el servidor responde `S_SkillBuy` con los índices que todavía no se saben y que la clase y el nivel permiten. Se muestra en la ventana del NPC como una tienda: ícono, nombre y precio (100, 400 o 900 adena según el nivel del hechizo).
- **Compra:** clic elige y OK manda `C_SkillBuyOK` (h n, n × d índice). El servidor cobra, manda el efecto 224 y `S_AddSkill`.
- **Lista vacía:** dice "Nothing to learn now".

**Libro de magia:** se usa como cualquier ítem (doble clic) y ya funcionaba. Las condiciones son del servidor:
- hay que leerlo dentro de un templo: Gludio, Oren o Aden según el alineamiento;
- hace falta el nivel suficiente para el nivel del hechizo;
- un libro del alineamiento contrario se pierde y hace daño;
- un GM lo aprende en cualquier lugar.

**Verificado:**
- **Aprendiz** (mago nuevo en la cuenta GM `prueba2`) le habla a Gereng (`--hablar=Gereng --npc-accion=buyspell`): 22 hechizos en la lista.
- **Compra:** elige Shield y Light (`--tienda=`). En la base pasa a saber 2, 3 y 4, y la ventana de magia los muestra.
- **Libro:** con el de Slow (`.item 40174`, `--usar=Spellbook`), aprende el 29 y el libro desaparece.
- **Charla** (príncipe 81) recibe la lista vacía, porque ya sabe todo lo de su clase.
- **Test:** `S_SkillBuy` y los dos comandos en `tests/test_proto.cpp`.

**Pendiente o conocido:** `action="xchgspell"` (Horun, magia de la torre de marfil) no se probó.

## Ataques con arco

![Flecha en vuelo](docs/capturas/5_flecha.png)

- **Alcance por arma** (`alcance_ataque` en `mundo.h`): el arma sale del estado visual de `S_CharPack` / `S_CharVisualUpdate`, que es el tipo de arma de L1J.
  - **Arco (20) y guantelete (62):** disparan a lo que esté "en pantalla" para el servidor (`L1Attack.calcHit` → `Point.isInScreen`: 18 celdas), sin acercarse.
  - **Lanza (24):** llega a 2 celdas.
  - **El resto:** cuerpo a cuerpo.
  - **Línea de vista:** el servidor también la pide y el cliente no la comprueba; con un obstáculo en medio, el servidor cuenta el disparo como fallo.
- **La flecha:**
  - el servidor responde con `S_UseArrowSkill` (el mismo paquete que el golpe, con el gráfico de la flecha, 66, y las celdas de origen y destino);
  - el proyectil sale en el frame de impacto del ataque con arco (acción 21 = 1 + 20) y vuela hasta el objetivo con su sprite de 8 direcciones;
  - el objetivo muestra el daño cuando llega.
  - Lo mismo vale para los monstruos arqueros (el servidor usa el mismo paquete) y para las púas del guantelete (2989).
- **Verificado contra el servidor local:**
  - Charla con el Ivory Tower Bow disparó a goblins a varias celdas sin moverse;
  - flechas 1000 → 999 → 998, impactos confirmados y un goblin muerto;
  - captura con la flecha a mitad de vuelo (`--captura-proyectil`).
  - **Test:** el `S_UseArrowSkill` real y el proyectil que genera.
- **Para la prueba:** el escudo sellado de Charla no se puede sacar desde el juego, y el arco es a dos manos. Se lo desequipó en la base local.
- **Pendiente:**
  - sin flechas el servidor cuenta el disparo como fallo y no avisa; el cliente tampoco;
  - el arco a dos manos con un escudo puesto da "You cannot take that off" si el escudo está sellado.

## Basurero, detalle de los ítems e identificar

![Ítem identificado](docs/capturas/item_identificado.png) ![Basurero](docs/capturas/basurero.png)

- **Basurero** (`C_DeleteInventoryItem`; `app/inventario_ui.cpp`, `app/cantidad_ui.cpp`): ícono abajo a la derecha del inventario. Arrastrar un ítem encima lo elimina, también lo que no se puede soltar ni vender. Siempre pregunta, con el diálogo de cantidad: cuántos de una pila, o confirmar si es uno. El servidor rechaza lo equipado, lo que no se puede borrar y lo sellado (con su mensaje). **Verificado:** una pila de 5 Potion of Bravery de Charla quedó en 3 (`S_ItemAmount` y la base); arrastrar al basurero abre la confirmación.
- **Detalle de un ítem:** al pasar el mouse, el nombre y, si está identificado, sus atributos, uno por línea: daño o AC, material, peso, encantamiento, dos manos, clases que lo usan, acierto y daño (también con arco), FUE…CAR, HP, MP, SP, MR, resistencias a elementos y a estados, regeneración, bono de experiencia, robo de HP o MP. Si no está identificado, "Unidentified". Los atributos (`getStatusBytes` del servidor) llegan en `S_AddItem`, `S_InvList` y `S_ItemStatus`; antes el decodificador los saltaba. Se guardan por ítem (`I_ATRIBUTOS`) y `describir_atributos` (`mundo/inventario.cpp`) los pasa a texto.
- **Pergaminos que se usan sobre otro ítem** (identificar, encantar arma y armadura: usos 7, 14, 26 y 27): el servidor lee en `C_UseItem` el ítem objetivo, y el cliente no lo mandaba, así que no funcionaban. Ahora, al usarlo, el pie del inventario pide "Click the target item" y el clic en un ítem manda `C_UseItem` con el objetivo; Esc cancela. `S_IdentifyDesc` sale como mensaje del servidor en el chat.
- **Verificado:** un "Helmet of Magic Resistance" +2 sin identificar ("Unidentified"); `--usar="Scroll of Identify"` y un clic sobre el casco: `S_ItemStatus` con los atributos y `S_IdentifyDesc`; el detalle muestra AC 2, hierro, peso 35, Enchant +2, todas las clases y MR +6. Tests del formato (un arma y una armadura), de los paquetes y de los comandos.
- **Pendiente o conocido:** los pergaminos de encantar no se probaron (mismo camino que identificar); el grado de los accesorios llega pero no se muestra; los textos del detalle están en inglés como el resto de la interfaz.

## Invisibilidad, resucitar, modo espectador y volver al login

![Resucitado](docs/capturas/resucitar.png)

- **Invisible** (`S_Invis`: d id, c 1/0): el servidor se lo manda al propio personaje (capa de invisibilidad, hechizo, `.invisible` de GM); a los demás les llega `S_RemoveObject`. El personaje se dibuja translúcido (`FLAG_INVISIBLE`, el mismo flag de `S_CharPack`, así que un GM ve igual a los invisibles). **Verificado:** `.invisible` con Charla: `S_Invis` 1 y el personaje translúcido.
- **Resucitar** (`S_Resurrection`: d quién, c tipo, d quién lo revivió, d clase): va al resucitado y a quienes lo ven. El cadáver vuelve a estar vivo (sin la acción de morir) y, si es el jugador, desaparece el cuadro "You have died". **Verificado** con dos clientes: Prueba (no GM) con `--aceptar` y Charla con `.recall Prueba`, `.death Prueba` y, unos segundos después, `.ress`: la pregunta 322, el sí (`C_Attr`), `S_Resurrection` y Prueba de pie con 89/89.
  - Si `.ress` llega en el mismo instante que `.death`, el servidor manda la acción de morir después de `S_Resurrection` (la muerte se anima con retraso) y el cliente queda mostrando el cadáver de alguien vivo. Solo pasa con los comandos de GM seguidos.
- **Modo espectador** (fantasma: mirar el Ultimate Battle por 100 adena, o después de perder ahí): con el flag 128 del propio personaje aparece arriba el botón "Leave spectator mode", que manda `C_ExitGhost` y el servidor lo devuelve a donde estaba. **No probado en vivo** (hace falta un Ultimate Battle); el comando, con test.
- **Volver al login** ("Back" en la selección de personaje): avisa con `C_ReturnToLogin` antes de cortar, como el original. **Verificado:** el paquete sale y el login siguiente entra sin "ya conectado". `--pulsar` ahora también funciona en la selección de personaje.

## Portales

- **Formato** (`recursos/mapa.*`): el final del `.s32`, después de los objetos, tiene tres bloques: `u32 n` × 5 bytes (los "HideSwitches" de l1j-client), `u32 n` × u32 (juegos de tiles) y `u16 n` portales × {u8 largo, nombre, u8 x en medias celdas, u8 y, u16 mapa destino, i16, i16}. l1j-client los leía con otro formato (registros fijos de 9 bytes) y salía basura.
- **Contra el servidor** (mapa 4): el cliente tiene 120 portales (67 son cruces dentro del mismo mapa); de los 74 de la tabla `dungeons`, 42 están en la misma celda (38 con el mismo mapa destino), 6 en una vecina y 26 solo los tiene el servidor.
- **Uso** (`mover_jugador`): al intentar entrar a una celda bloqueada que es un portal del mapa, `C_EnterPortal` con su celda, como el original. Los que solo tiene el servidor siguen con el paso hacia la celda bloqueada (`C_MoveChar`, que también mira la tabla). En una celda pasable alcanza el paso normal: así un portal que el servidor no tiene no impide pisarla.
- **Verificado:** Charla en (32686, 33377) y un paso al norte: `C_EnterPortal` (32686, 33376) y el teletransporte al castillo de Windawood (mapa 29). Test del formato con el segmento real (`tests/test_recursos.cpp`).

## Buffs: íconos y encantar arma o armadura

![Íconos de buffs](docs/capturas/buffs_iconos.png)

- **Íconos que faltaban:** Enchant Strength (Mighty) y Enchant Dexterity, y los de elfo (Dress Mighty/Dexterity), llegan en `S_Strup` (120) y `S_Dexup` (28), que no se decodificaban. Ahora son efectos activos con el ícono del hechizo (tipo 5 mago, 2 elfo; las pociones, "STR"/"DEX").
- **Bless Weapon, Holy Weapon, Enchant Weapon, Blessed Armor, Shadow Fang:** el servidor no manda ícono, solo el mensaje 161 ("... brilla"). El cliente registra el efecto al recibirlo después de lanzarlo, con la duración de `spells` (`duration=`).
- **Enchant Weapon y Blessed Armor no funcionaban:** el servidor lee en `C_UseSkill`, en el lugar del objetivo, el ítem a encantar (`_itemobjid`), y el cliente no lo mandaba (respondía 79, "no pasó nada"). Ahora manda el arma o la armadura (cuerpo) equipada; la clase de `spells` dice cuál (`SpellEnchantWeapon`, `SpellBlessedArmor`; `SpellEnchantCurrentWeapon` usa la equipada sin indicarla).
- **Verificado** con Aprendiz (maga) y la daga equipada: Enchant Strength (`S_Strup` 1200 s), Bless Weapon (161), Enchant Dexterity (`S_Dexup`) y Enchant Weapon (con el id de la daga: 161) muestran sus cuatro íconos (20m, 20m, 20m, 30m). Blessed Armor sin armadura equipada: 79, como corresponde.
- **Buffs sin paquete de ícono** (Advance Spirit, Light, Counter Magic, muchos de elfo, caballero y elfo oscuro): el cliente original trae en `spells` la duración de cada uno, así que lleva el tiempo él mismo. Ahora, al lanzar un buff sobre sí mismo, el ícono se pone cuando llega su gráfico (`S_SkillSound` con el `graphic=` de `spells`) sobre el personaje en los 3 s siguientes; sin `graphic=` (Light), vale el primero que llegue en 1,5 s. Si el servidor también manda el suyo (Holy Walk, Shield...), se dibuja uno solo. **Verificado** con Aprendiz: Advance Spirit (20m), Light (60m) y Holy Walk (59s, una sola vez) (`docs/capturas/buffs_propios.png`); con Cloud (caballero, nivel y vida subidos solo para la prueba y después restaurados): Reduction Armor y Solid Carriage quedan registrados.
- **Magias sin ícono en los datos:** `spellicon.idx` tiene 168 entradas y en 0 las de caballero (Shock Stun, Reduction Armor, Bounce Attack, Solid Carriage, Counter Barrier), Armor Break, Avatar, Triple Arrow, Elemental Falldown, Counter Mirror, Aqua Protect, las de elfo 169–176 y Bloodlust; las de caballero dragón (181–195) e ilusionista (201–220) quedan fuera del archivo. Se ven como un recuadro con las primeras letras del nombre. Hay una serie de íconos de estado (1576–1600, el estilo del de Holy Walk, 1578) que probablemente son esos, pero ningún archivo de datos legible dice cuál es cuál: esa tabla debe estar en `lin.bin` (cifrado). Pendiente: asignarlos a mano, comparando con el original.

## Hechizos de área alrededor del lanzador

![Tornado](docs/capturas/area_tornado.png) ![Blizzard](docs/capturas/area_blizzard.png)

- **Sin objetivo:** Tornado, Fire Storm, Blizzard y Freezing Blizzard afectan el área alrededor del lanzador (en el servidor, objetivo "none" con área 4, 3, 10 y 10). La ficha dice "Area of Effect Within N cells", igual que Lightning Storm y Meteor Strike, que sí van sobre un objetivo; la diferencia está en `spells`: las primeras no tienen `range=`. Ahora se lanzan al instante, sin pedir objetivo.
- **El gráfico:** llega en `S_RangeSkill` tipo 0 con la lista de afectados; el cliente lo ponía sobre cada monstruo (y sin monstruos, no se veía nada). Ahora va en la celda del lanzador y se reparte según la clase de `spells`: `SpellTornado`, 4 copias (arriba, abajo, izquierda y derecha en pantalla, a la mitad del área); `SpellBlizzard`, una cada 3 celdas en todo el círculo del área (10 celdas: toda la pantalla original). Los sprites (758, 1819, 757, 3933) son de un solo cuadro de efecto: el original arma la composición en el ejecutable, así que la disposición es aproximada.
- **Verificado** con Aprendiz: Tornado y Blizzard sin objetivo (`C_UseSkill` de 4 bytes; el servidor responde con `S_RangeSkill` tipo 0) y las capturas. Tests de la clasificación y del paquete.
- **Pendiente:** comparar con el original la distancia de los 4 tornados y la densidad de la ventisca.

## Colores de los nombres: PK y nivel de los monstruos

![Nivel](docs/capturas/nombres_nivel.png) ![Rosa](docs/capturas/nombre_rosa.png)

- **Nombre rosa** (`S_PinkName`: d id, d segundos): quien ataca a otro jugador legal en zona normal queda rosa 180 s (a él y a quienes lo ven); con 0 segundos, termina. Se guarda por objeto (`O_ROSA`). **Verificado** con dos clientes en un campo del mapa 4: Charla con Ctrl ataca a Prueba; llega `S_PinkName` 180 a los dos y Charla se ve rosa. (Después, los dos de vuelta en Talking Island.)
- **Monstruos según su nivel:** `S_NPCPack` trae el nivel del monstruo (el segundo byte después del HP; en los personajes es 0). El nombre va blanco si es mucho más bajo que el jugador (≤ −10), verde si es parecido, amarillo si es algo más alto (≥ +5) y rojo si es mucho más alto (≥ +10) (`nivel_relativo`, `DIF_*` en `mundo/mundo.h`). Los NPC siguen en celeste. **Verificado** con Aprendiz (nivel 30): Rock Golem (5) blanco, Scorpion (26) verde, Elekkadom (35) amarillo, Jack-O-Lantern (45) rojo.
- **Pendiente:** los umbrales son aproximados (como lo recordaba el usuario; no están en los datos); los jefes no se distinguen: el paquete no dice si un monstruo es jefe.

## Pociones de FUE y DES, otros efectos y reiniciar atributos

![Reiniciar atributos](docs/capturas/reiniciar_atributos.png) ![Poción de FUE](docs/capturas/pocion_fue.png)

- **Pociones de FUE y DES** (Flora, estímulo, talento): llegan como `S_Strup`/`S_Dexup` con el aumento en el lugar del tipo (6), así que no se distinguen de las magias por el paquete; el ícono es el del ítem recién usado. **Verificado** con Aprendiz y la poción 40922: `S_Strup` 300 s y el ícono de la poción (5m).
- **`S_Liquor`:** 1 borracho (la pantalla se mece hasta el 0), 8 tercer acelerador: todas las acciones × 0,87 (como en el `AcceleratorChecker` del servidor) y su efecto en la barra (600 s; el paquete no trae el tiempo). **`S_SkillIconBlessOfEva`:** respirar bajo el agua, con su tiempo. **`S_Ability`:** anillos de control de teletransporte e invocación; se guardan (el cliente ya pregunta el destino del teleport y la invocación llega como página del servidor).
- **Reiniciar atributos** (`app/reinicio_ui.cpp`; `S_CharReset` comparte el número 33 con el menú de mascota: subtipos 1–3): con la Candle of Reminiscence se habla con Lolo (Gludio, acción "ent"); el servidor lleva al personaje al mapa 997 y abre la ventana:
  1. los 75 puntos iniciales como al crear el personaje (el servidor no los valida: cada atributo entre la base y base + bonus de la clase, y la suma 75; las tablas de `pantalla_crear.cpp`);
  2. subir de nivel: de a 1 o de a 10 hasta el 50; desde ahí cada nivel suma un punto en un atributo elegido; el último nivel termina (`C_CharReset` 2, 8 + atributo);
  3. si hay elixires, repartir esos puntos (`S_CharReset` 3, `C_CharReset` 3).
  **Verificado** con Aprendiz (maga, nivel 30) y `--reiniciar-atributos`: `S_CharReset` 1 (12/6/10/30), los atributos iniciales, 2 × 10 niveles y 9 × 1 con su `S_CharReset` 2, el fin, la vela consumida y de vuelta en Gludio con nivel 30. Después se restauró su fila de la base.
- **Pendiente:** el reparto de los elixires no se probó (Aprendiz no tiene); el original muestra la ventana con imágenes propias, esta es una ventana de texto.

## Grupo, bloqueados, amigos y sonidos del servidor

![Panel del grupo](docs/capturas/grupo_panel.png)

- **Panel del grupo** (`dibujar_grupo`, `app/social_ui.cpp`): `S_PacketBox` 104 trae la lista con el líder y la vida de cada uno (al que se une), 105 el que se une (a los demás), 106 el líder nuevo; la vida se actualiza con `S_HpMeter`, que el servidor manda a todo el grupo aunque estén lejos. Se vacía con los mensajes 418 (disuelto), 419 (echado) o 420 con el propio nombre. A la izquierda, a media altura, los otros miembros con su barra de vida y el líder con "*". **Verificado** con dos clientes en el mapa 2005: Charla `/invite Prueba`, Prueba con `--aceptar`: a Prueba le llega 104 y ve "* Charla" con su vida; a Charla, 105 y "Prueba entered the party".
  - El servidor manda la vida de la lista 104 como 0 o 100 (calcula `(int)(hp/max) * 100`); la real llega enseguida con `S_HpMeter`.
- **Bloqueados:** `/exclude Nombre` (`C_Exclude`) bloquea o desbloquea; el servidor confirma con `S_PacketBox` 18 o 19 y manda la lista con 17. **Verificado:** `/exclude Hooper` → 18.
- **Amigos:** `/buddy` (`C_Buddy`: la página "buddy" con la lista y quién está conectado), `/addbuddy` y `/delbuddy`. **Verificado:** `/buddy` → `S_ShowHTML` "buddy".
- **Grupo de chat:** `/chatparty` (la lista), `/chatoutparty`, `/chatbanish Nombre` (`C_ChatParty`). **Verificado:** `/chatparty` fuera de un grupo de chat → mensaje 425.
- **`S_Sound`:** el sonido que pide el servidor (`sound/<n>.wav`). Con test; no se buscó en vivo qué ítem lo dispara.

## Tienda de jugadores

![Tienda](docs/capturas/tienda_jugador.png)

- **Dónde:** solo en las salas del mercado (mapas 340, 350, 360 y 370; fuera, el servidor responde el mensaje 876).
- **Vendedores** (`app/tienda_ui.cpp`): `S_DoActionGFX` con la acción 70 trae el título de la tienda (también a quien llega después). Se los dibuja sentados (el último cuadro de la acción 70) con el título en un recuadro sobre el nombre. Un clic camina hasta 2 celdas y pide la lista (`C_ShopList`); `S_PrivateShop` tipo 0 trae lo que vende (orden, bendición, ícono, cantidad, precio y nombre) y tipo 1 lo que compra, solo con los ítems propios que le sirven.
- **Comprar y venderle:** la ventana tiene "Sells" y "Buys"; un clic en una línea pregunta cuántos con el diálogo de cantidad y manda `C_Result` tipo 0 (orden y cantidad) o tipo 1 (ítem propio, cantidad en 3 bytes y orden); después pide la lista otra vez.
- **La propia:** `/shop Título` abre la ventana para armarla: un clic en un ítem del inventario pide el precio por unidad y lo agrega con toda la pila; "Open" la abre (`C_Shop` 0 con la lista y el título) y "Close shop" (o `/shop` otra vez) la cierra (`C_Shop` 1).
- **Verificado** con dos clientes en el mapa 340 y `--tienda-propia` / `--cantidad`: Charla abre "Vendo pociones" con 3 Potion of Bravery a 50; Prueba la ve sentada con el título, hace clic (a 5 celdas: 3 pasos y la lista), compra 1: `S_AddItem`, su adena 106 → 56 y la lista con 2. En la base: Prueba 56 adena y 1 poción; Charla +50 adena y 2 pociones. (Después, las dos de vuelta en Talking Island.)
- **Pendiente:** la propia solo vende (la lista de compra de `C_Shop` va vacía); "Buys" (venderle a otro) no se probó en vivo; el precio y el título no se pueden editar en la ventana (el título va con `/shop Título`).

## Tablón de los pueblos y correo

![Tablón](docs/capturas/tablon.png) ![Correo](docs/capturas/correo.png) ![Correo nuevo](docs/capturas/correo_sobre.png)

- **Tablón** (`app/tablon_ui.cpp`; gfx 1544, 1546, 1557, 1559, 2205, 2207): un clic camina hasta él y manda `C_Board` (antes iba `C_NPCTalk`, que el servidor no responde para el tablón). `S_Board` trae una página de hasta 8 temas (id, autor, fecha, título; la primera además el costo, 300); "Next" pide la siguiente con el último tema (`C_BoardBack`); un clic lee el tema (`C_BoardRead` / `S_BoardRead`); "Write" (título y texto) publica (`C_BoardWrite`) y el autor puede borrar el suyo (`C_BoardDelete`). El servidor cobra las 300 adena sin revisar que alcancen: el cliente no deja publicar sin ellas.
- **Correo** (`app/correo_ui.cpp`): `/mail` abre la bandeja (Inbox, Clan, Saved: `C_Mail` 0–2); un clic lee la carta (0x10+), "Delete" (0x30+) y "Keep" (0x40, a guardadas). "Write" escribe a un personaje (50 adena) o a todo el clan (1000 por miembro): destinatario, asunto y texto. El asunto y el texto van en UTF-16LE (asunto, 0 0, texto, 0 0); el cliente escribe y muestra solo ASCII (lo demás, "?"). Una carta nueva (`S_Mail` 0x50) avisa en el chat. El servidor manda la bandeja al entrar al juego.
- **Verificado** en el tablón de Talking Island (mapa 0) y con dos personajes, escribiendo con `--pulsar` y clics simulados: Charla publica "Hola tablon" (`C_BoardWrite`, la página con 1 tema, 300 adena menos en la base), lo lee ("Charla 26/10/31 Hola tablon") y lo borra (la página vacía; 0 temas en la base). Charla le escribe a Prueba "Hola correo" (`S_Mail` 0x50 de la copia y 0x20 entregada); Prueba abre `/mail` y lee "From Charla: Hola correo".
- **Gráficos del original:** la ventana de correo usa los de `Sprite.pak` (buscados en hojas de miniaturas de las 1444 `N.img`): el papel 656, la marca de leída 657, cerrar 658, Write 662, Delete 664, Cancel 578 y Send 582 (cada botón con su resaltado en N + 1); tinta oscura sobre el papel. Ninguna imagen es un sobre de "correo nuevo": el aviso es un dibujo propio, un sobre con sello arriba a la izquierda con la cantidad sin leer (de la bandeja que el servidor manda al entrar y de cada `S_Mail` 0x50), que titila y abre el correo con un clic. **Verificado:** Charla escribe "Sobre nuevo" desde la ventana nueva; Prueba entra, ve el sobre con 1 y al tocarlo se abre la bandeja (una carta leída con la marca, la nueva sin leer).
- **De paso:** el basurero del inventario usa el del original (1003; 1004 resaltado con un ítem encima).
- **Pendiente:** el texto de una línea (se muestra cortado en varias); sin acentos ni caracteres no ASCII; las flechas de página del original (666/667, 672–675) no se usan (se muestran 10 cartas).

## Equipo del personaje

![Equipo](docs/capturas/equipo.png) ![Equipo, UI simple](docs/capturas/equipo_simple.png) ![Correo, UI simple](docs/capturas/correo_simple.png)

- **Pestañas en la ventana de estado** (Alt+C): "Status" (los atributos, como antes) y "Equipment" (`app/equipo_ui.cpp`), como el original, que los muestra en la misma ventana.
- **La imagen del original:** 981 de `Sprite.pak` (179×275), encontrada en las hojas de miniaturas junto a la página de estado (982) y el marco (983/984): la silueta con un recuadro por parte del cuerpo y "Armor Class". En cada recuadro, el ícono de lo equipado: la parte sale del tipo de uso del ítem (1 arma, 2 armadura, 18 camiseta, 19 capa, 20 guantes, 21 botas, 22 casco, 24 collar, 25 escudo, 37 cinturón, 40 aro) y "equipado" del sufijo del nombre. Los anillos (23) no tienen recuadro: van más chicos en el espacio libre de abajo a la derecha. Con el puntero encima, el nombre y los atributos del ítem. La AC va en su campo. Sin la imagen, recuadros oscuros en el mismo lugar.
- **Con la UI simple** (F2) no se usan los fondos del original: el equipo es una lista de las partes, de la cabeza a los pies (rótulo, ícono y nombre de lo equipado; los anillos en la última fila; "Armor Class" abajo), y el correo usa el recuadro oscuro con texto claro y botones de texto en vez del papel 656 y sus botones. Los nombres largos se cortan sin partir caracteres CP949 (`bytes_que_caben`).
- **Verificado** con Charla: casco, capa, camiseta, armadura, guantes, arco, botas y AC −13 en sus recuadros.
- **Pendiente:** qué parte va en cada recuadro se dedujo de dónde cae sobre la silueta (casco en la cabeza, aro al costado, botas en los pies): conviene compararlo con el original; los talismanes y patrones (usos 43–48) no se muestran; la página "Status" sigue con el marco propio (la 982 del original queda para después).

## Casas y castillos

![Subasta](docs/capturas/casas_subasta.png) ![Mapa de la casa](docs/capturas/casas_mapa.png) ![Nombre de la casa](docs/capturas/casas_nombre.png) ![Puerta](docs/capturas/casas_puerta.png)
![Torre](docs/capturas/asedio_torre.png) ![Corona](docs/capturas/asedio_corona.png) ![Castillo tomado](docs/capturas/asedio_tomado.png) ![Impuesto](docs/capturas/castillo_impuesto.png) ![Fecha del asedio](docs/capturas/castillo_fecha.png)
![Guerra declarada](docs/capturas/guerra_declarada.png) ![Al entrar](docs/capturas/guerra_al_entrar.png) ![Rendición](docs/capturas/guerra_rendicion.png)

Plan: [docs/analisis/PLAN_CASTILLOS_CASAS.md](docs/analisis/PLAN_CASTILLOS_CASAS.md).

**Protocolo** (tests de bytes en `tests/test_proto.cpp`):
- `S_HouseList`, `S_HouseMap`, `S_War`, `S_CastleMaster`, `S_TaxRate`, `S_Deposit`, `S_Drawal`;
- `S_WarTime` comparte el opcode 150 con `S_MapID` y se distingue por el largo;
- `S_PacketBox` 0–4 (el asedio);
- `C_War`, `C_TaxRate`, `C_Deposit`, `C_Drawal`, `C_ChangeWarTime` y `C_Attr` con texto;
- la página "agsel" trae la casa antes de los datos.

**Casas** (`app/casas_ui.cpp`):
- **Tablón de subastas:** tiene el mismo gfx que los tablones de los pueblos (1544): el clic ya mandaba `C_Board` y el servidor respondía `S_HouseList`, que se perdía.
  - Ahora es una ventana con cada casa: "Giran house N" (los nombres de la base vienen en coreano), área, cierre y puja; se desplaza con la rueda.
  - Un clic manda "select" + casa. La página "agsel" va al diálogo del NPC y la lista se corre a la derecha. Sus acciones "map" y "apply" llevan la casa.
- **Mapa de la casa:** la imagen del original no se encontró en `Sprite.pak`. El mapa es propio: los terrenos de las casas del pueblo, sacados de `L1HouseLocation.java` del servidor (62 casas: Giran, Heine y Gludin; las de Aden no tienen coordenadas en el servidor). Va en isométrico, con la casa resaltada y el personaje.
- **Pujar:** "apply" abre el diálogo de cantidad ("agapply").
- **Mayordomo:** todo es HTML.
  - "name" pregunta el nombre (`S_MessageYN` 512). El diálogo de preguntas ahora tiene una caja de texto para esa pregunta y responde `C_Attr` con el texto.
  - Los datos "$N" de las páginas (el nombre del mayordomo en "agpay") se traducen con `desc-e.tbl`.

**Castillos** (`app/castillos_ui.cpp`):
- **Señor del castillo** (`S_CastleMaster`): una corona dibujada a la izquierda del nombre (y del emblema). No se encontró la del original.
- **Asedio:**
  - avisos en el chat y un cartel arriba mientras dura;
  - los portones (gfx de tipo 11 con 28/29) son puertas;
  - el daño de portones y torres (`S_DoActionGFX` 32–37) se queda en el último frame, como abrir y cerrar;
  - la torre sana (sin acción quieta) se dibuja con su primera acción;
  - la torre y la corona (gfx 1482) se atacan con un clic: tomar la corona es "atacarla" (`C_Attack` → `onAction`).
- **Chambelán:**
  - "tax" (`S_TaxRate`), "cdeposit" (`S_Deposit`) y "withdrawal" (`S_Drawal`) abren el diálogo de cantidad, que responde `C_TaxRate`, `C_Deposit` o `C_Drawal`;
  - "Declare next siege time" (acción "asktime", que el servidor no atiende) manda `C_ChangeWarTime` y los 6 horarios de `S_WarTime` van al chat. El servidor no deja elegir uno.
- **Guerras de clanes:** `/war`, `/surrender` y `/ceasefire` + clan (`C_War` 0, 2, 3). `S_War` va al chat y se lleva la lista de clanes enemigos.

**Servidor:**
- `HouseTable.updateHouse` guarda fechas con el `DateFormat` del idioma. En inglés MariaDB las rechazaba y no se guardaban ni el dueño ni el nombre de la casa. `arrancar.sh` usa ahora formato japonés (`user.language.format=ja`).
- Scripts solo para pruebas en `servidor_local/pruebas/`:
  - `subasta_cerrar.sql` cierra ya la subasta de Giran 1;
  - `asedio_kent.sql` empieza el asedio de Kent y baja la vida de la torre y del portón (se leen al arrancar);
  - `restaurar.sql` deja todo como estaba.

**Verificado contra el servidor local** (Charla: príncipe nivel 81, GM, líder de Prueberos):
- **Casa:**
  1. Lista de Giran, "agsel" de Giran 1 y su mapa.
  2. Puja de 100.000; con la subasta cerrada, Prueberos ganó la casa.
  3. Mayordomo Emma: "Casa Prueba" quedó en la base; la puerta se abre ("open": acción 28 y `S_Door` pasable); "pay" muestra el impuesto.
- **Asedio de Kent:**
  1. Cartel y aviso.
  2. Grietas 32, 33, 34 y la torre destruida (35).
  3. Aparece la corona; Charla la toma: `S_CastleMaster` 1 y Prueberos queda con `castle_id` 1. La corona sale junto a su nombre.
- **Chambelán Ishmael:** impuesto al 20 % (en la base), depósito de 1000 (fondos 1000), retiro de los fondos y la fecha del próximo asedio. La primera fecha queda a menos de 182 minutos de la real: el servidor redondea a esa unidad.
- **Guerra:** Prueba (subida a nivel 30 con permisos de GM temporales) fundó "Rivales" y declaró la guerra a Prueberos. Charla, al entrar, ve "Your pledge is at war with the Rivales pledge"; después Prueba se rindió.
- **Restauración:** todo se restauró: casa, castillo, clanes, Prueba, y la adena y las flechas de Charla.

**Pendiente:**
- el daño del portón no se vio en vivo: con arco, el servidor exige línea de vista hasta la celda del portón, que está bloqueada; hace falta un arma cuerpo a cuerpo;
- el fin del asedio (`S_PacketBox` 1) no se esperó;
- mercenarios: el servidor no los implementa;
- muebles de la casa.

## Sin parámetros: servidores en el login y control en vivo

![Login con servidores](docs/capturas/login_servidores_simple.png) ![Menú abierto](docs/capturas/login_menu_servidores.png) ![Interfaz original](docs/capturas/login_servidores_original.png)

Plan: [docs/analisis/PLAN_CONTROL_VIVO.md](docs/analisis/PLAN_CONTROL_VIVO.md).

- **El cliente no tiene parámetros:**
  - se quitaron los 53 `--algo`, la estructura `Automatizacion` y sus ~114 usos en 16 archivos;
  - también `LINEAGE_DIR` y la regla de pasar cualquier clave de `settings.ini` como `--clave=valor`.
- **Servidores** (`plataforma/rutas.*`):
  - una línea por servidor en `settings.ini`: `servidor=Nombre|host|puerto|protocolo|cuenta`, y `servidor_elegido=`;
  - el formato anterior se convierte solo;
  - la clave ya no se lee ni se guarda; la cuenta se recuerda por servidor al entrar bien;
  - la tabla de opcodes se elige al conectar, con el protocolo del servidor elegido (un protocolo desconocido es un aviso en el login, no un cierre);
  - `leer_config` y `escribir_config` trabajan sobre texto, con su test (`tests/test_core.cpp`).
- **Login:** `ui::desplegable` con los servidores (la lista abierta va en su propio nivel de ventana, encima de todo), en la UI simple (una fila "Server" arriba de la cuenta) y en la original (arriba del recuadro), con `host:puerto protocolo` debajo.
- **Tamaño de la ventana:** se guarda al cerrar (`ventana=AxB`).
- **Control en vivo** (`app/control.cpp`, solo con la opción de CMake `LIN_CONTROL`, encendida en `debug` y `debug-asan`):
  - **El socket:** de archivo, `0600`; se revisa una vez por frame en el hilo principal, sin hilos.
  - **Los comandos:** usan las mismas funciones que la interfaz. Se hicieron públicas `iniciar_login`, `entrar_juego`, `enviar_chat`, `buscar_item`, `buscar_hechizo`, `activar`, `responder_pregunta`, `aceptar_cantidad` y `crear_personaje_rapido`.
  - **Las esperas** responden cuando se cumplen.
  - **La versión final:** `release` lo deja afuera, y `cmake/sin_control.cmake` falla si el ejecutable trae el texto del socket. También `LIN_CONFIG` existe solo con `LIN_CONTROL`.
- **Herramientas:**
  - `herramientas/lin_ctl.py` (terminal y módulo; varios clientes a la vez);
  - `herramientas/pruebas/subastas.py`, la primera prueba en vivo como script.
- **Verificado:**
  - login malo (el motivo vuelve como error), login, personaje, `estado` y captura del menú abierto con clic y Esc;
  - la prueba de subastas completa (lista, una casa, su mapa y la vuelta de Charla a su lugar), sin contar frames;
  - dos clientes a la vez (Charla y Prueba);
  - `release` sin el control.
- **Pruebas en vivo como scripts** (`herramientas/pruebas/`, `todas.py`):
  - las 13 pasan juntas contra el servidor local: login, moverse, chat_estado, combate_hechizos, inventario, equipo, npc, tablon_correo, subastas, social; y con `--servidor`, casas, castillos y guerra;
  - estas tres últimas reinician el servidor y restauran la base solas (`servidor.py`: respaldo de las filas de los personajes, su inventario y los clanes).
  - Pasarlas a scripts encontró:
    - **un error del cliente:** `C_Attr` mandaba 0 en el primer campo, así que aceptar una guerra o una rendición no hacía nada (ver DESCUBRIMIENTOS);
    - **una carrera en el control:** la respuesta a un comando podía llegar antes del `esperar` que lo sigue. Ahora `esperar paquete=`/`chat=` cuentan también lo llegado desde el comando anterior;
    - **`esperar ms=`:** hacía falta para el servidor, que ignora un cambio de equipo justo después de otro.
- **Pendiente:**
  - el socket en Windows (`afunix.h`, MULTIPLATAFORMA);
  - editar la lista de servidores desde el login.

## Servidor 3.80C (L1JGO-Whale)

![Entrar](docs/capturas/380_entrar.png) ![NPC](docs/capturas/380_npc.png) ![Combate](docs/capturas/380_combate.png)

- **El servidor:** [L1JGO-Whale](https://github.com/r0ptik/L1JGO-Whale), un servidor 3.80C de Taiwán en Go con PostgreSQL. `servidor_380/instalar.sh` lo deja local:
  - commit fijo, Go y PostgreSQL de Homebrew;
  - base propia en `servidor_380/datos`, puerto 5433;
  - configuración generada en `l1jgo/local.toml`, sin el `#npc:gfx` de depuración en los nombres;
  - `arrancar.sh` lo levanta en el puerto 7001 y `detener.sh` lo detiene.
- **Protocolo `tw380`** (`proto/opcodes.cpp`):
  - la tabla sale de `internal/net/packet/opcodes.go` del servidor, por nombre;
  - el paquete inicial es el opcode 150 (semilla y 11 bytes fijos); el cifrado es el mismo de 3.63;
  - **diferencias de formato:**
    - vida y maná en 4 bytes en `S_OwnCharStatus`, `S_HPUpdate` y `S_MPUpdate` (`Lector::vida`, con `PerfilProtocolo::bytes_vida`);
    - `S_Door` es el opcode 209, con el mismo formato;
    - `S_WarTime` tiene su propio opcode (231): va al decodificador de `S_MapID`, que lo distingue por el largo.
  - **El resto** (login, lista y creación de personajes, entrar, `S_CharPack`, movimiento, chat, inventario, páginas de NPC, ataques) funcionó sin cambios.
  - **En `settings.ini`:** `servidor=Whale 3.80|127.0.0.1|7001|tw380|`.
- **Verificado en vivo** con el control (cuenta `prueba`, personaje Ensayo, creado desde el cliente; GM puesto en la base para las pruebas):
  - login, crear un príncipe y entrar (mapa 2005, la isla de los principiantes);
  - estado con la vida correcta, caminar, chat (con su eco);
  - inventario, `.item` (pociones), `.level 20`;
  - hablar con NPCs (`cuse1`, `tutor`, con los vínculos de las páginas del juego);
  - `.spawn` de un Jack-O-Lantern y pelear (9–11 por golpe; el monstruo le saca 21–25 y lo mata: aparece el aviso de muerte).
- **Limitaciones (del servidor o de la instalación):**
  - **Texto en chino:** los nombres de ítems y los mensajes del sistema llegan en chino (Big5; el servidor solo acepta Big5 o GBK), y la instalación trae solo la fuente coreana: se ven ilegibles. Los nombres de NPC usan `$N` y se ven bien.
  - **Sin equivalente en `tw380`** (quedan sin mandar o sin reconocer): noticias, `S_Teleport`, el mensaje azul, borrar hechizos, el menú de mascotas, volver al login, la página siguiente del tablón y la fecha del asedio.
  - **Opcode 64** (`S_CharSynAck`: casilleros de equipo, emblema, reinicio): sin decodificar.
  - **No probado:** tiendas, almacén, grupo, intercambio, clanes, casas y castillos en 3.80; las pruebas de `herramientas/pruebas` usan personajes del servidor 3.63.

## Interfaz: ventanas móviles, transparencia y botones

![UI simple](docs/capturas/ui_ventanas_simple.png) ![UI original](docs/capturas/ui_ventanas_original.png)

- **Mover las ventanas** (`app/ventanas.cpp`):
  - estado, inventario, hechizos, diálogo de NPC y opciones se arrastran por la barra de título;
  - la posición se guarda en `settings.ini` (`ventanas=`) y se mantiene siempre con el título dentro de la pantalla;
  - las que no se movieron siguen en su lugar de siempre y se acomodan solo junto a las que también siguen en su lugar.
- **Semitransparentes:** el fondo de las ventanas deja ver el juego; el marco y el texto quedan algo más opacos. La opacidad se elige en Opciones (40–100 %; `opacidad_ventanas=` en `settings.ini`, 170 de 255 por defecto). Una ventana que tapa a otra se dibuja con fondo opaco: si no, se leía el texto de la de abajo.
- **Orden entre ventanas** (`actualizar_ventanas` y `dibujar_ventanas`):
  - la que se abre y la que se toca con el clic pasan adelante (`App::orden_ventanas`, de atrás hacia adelante);
  - cada una se dibuja en su propio nivel: mientras se dibuja, `Gpu::nivel_ventana` lleva sus claves de `CAPA_UI` y `CAPA_UI_TEXTO` a `CAPA_VENTANAS` con el nivel por encima de la profundidad, así una ventana tapa entera a las de abajo, texto incluido. Lo que se arrastra va en `CAPA_ARRASTRE`, encima de todo; los diálogos (pregunta, polymorph, F1, muerte), encima de las ventanas;
  - solo la de más arriba bajo el puntero recibe el mouse: a las demás se les esconde el puntero mientras se dibujan (salvo durante un arrastre, que lo resuelve la ventana de donde salió). `sobre_ventana` también es solo la de más arriba;
  - un ítem arrastrado del inventario va al suelo solo si se suelta sobre el mundo, no sobre otra ventana.
- **Sin solaparse al abrir:** si el lugar de siempre de una ventana que se abre tapa a otra abierta, se busca el primer lugar libre, de arriba a la derecha hacia abajo a la izquierda, sobre la barra de atajos (no se guarda; al cerrarla vuelve a su lugar de siempre). Si no hay lugar, queda en el de siempre y adelante. Las que el usuario movió no se tocan.
- **Botones para abrir las ventanas**, además de las teclas:
  - **UI original:** los botones del juego (`1005`, `1007`, `1011` y `1009.img`, con su versión resaltada), en la franja de arriba del panel derecho: magia, estado, inventario y opciones. Con el puntero encima se ve cuál es y su tecla.
  - **UI simple:** botones de texto a la derecha de la línea del chat: Magic, Status, Items y Options.
- **Ventana de opciones:** interfaz (simple u original), cámara, oscurecimiento en la sombra, opacidad de las ventanas, zoom y "Reset" para devolver las ventanas a su lugar.
- **Verificado:**
  - arrastrar el inventario con `--arrastrar=1100,45,700,300` lo mueve, guarda `ventanas=inventario:626,291` y el estado ocupa el lugar que dejó;
  - capturas con las dos interfaces;
  - cinco ventanas abiertas a la vez con 1600×900 lógicos: ninguna se solapa (`docs/capturas/ui_ventanas_sin_solapar.png`); con 1008×552 no entran y se apilan con fondo opaco, y un clic simulado en una parte visible de Opciones la trae adelante (`ui_ventanas_al_frente.png`).
- **Pendiente o conocido:**
  - **Ventana que se abre:** se ve un frame en su lugar de siempre antes de pasar al lugar libre (se detecta al frame siguiente).
  - **Botones del panel original:** su lugar es aproximado (la franja libre del panel). Los otros tres botones de la serie (salir 1013, grupo/intercambio 1015, ayuda 1017) llegan con sus funciones.
  - **Arrastrar a mano:** el arrastre se probó simulado; falta probarlo con el mouse.

## Interfaz: cursores

![Cursores](docs/capturas/cursores.png)

- **Cuándo cambia** (`app/cursores.cpp`):
  - **mano:** para moverse y sobre la interfaz;
  - **espada:** sobre un monstruo, o con Ctrl sobre otro personaje o NPC (atacarlo);
  - **círculo de objetivo:** mientras un hechizo espera su objetivo.
- **Cómo se dibujan:** son cursores del sistema (`SDL_CreateColorCursor`, con una versión de 64 px para pantallas Retina). Se mueven sin el retraso de un frame que tendría dibujarlos con el juego.
- **Los originales no están en esta instalación:**
  - no hay archivos de cursor en la carpeta del juego;
  - los cursores de `Lineage.exe` son los estándar de Windows (ayuda y flechas de desplazamiento);
  - entre todas las imágenes y sprites chicos de los `.pak` tampoco aparecen.
  - Seguramente van en el código cifrado (`lin.bin`). Estos son dibujos propios en el mismo estilo; si aparecen los originales, se reemplazan los tres dibujos de 16×16.
- **Labios** sobre un NPC (el clic le habla); con Ctrl, la espada (forzar ataque). Dibujo propio de 16×16 como los demás, revisado en una imagen aparte.
- **Pendiente:** el cambio de cursor no sale en las capturas (son del juego, no del escritorio), y falta probarlo a mano.

## Rendimiento

Medido con `--perf` (una línea por segundo en el registro; los mismos datos en el overlay F3), Release, ventana 1600×900 en Retina, caminando 20 pasos desde el punto de inicio:

| | Antes | Ahora |
|---|---|---|
| CPU por frame, promedio | ~2,2 ms | ~1,5 ms |
| Peor frame de cada segundo, caminando | 5–9 ms | 1,6–3,2 ms |
| Decodificando en el hilo principal, por segundo | 4–14 ms | 0,2–1 ms |
| Allocations por frame del cliente, en régimen | ~15–30 | 0 (solo al cargar algo nuevo) |

**Medición:**
- La CPU del frame incluye el orden de la lista de dibujo y excluye solo `presentar` (espera del vsync y llamadas al driver). Antes el orden quedaba afuera.
- Allocations: `operator new` instrumentado (`core/asignaciones.cpp`, contador por hilo). Las del driver de Metal dentro de `SDL_GPU` (subir y presentar, ~40 por frame) no se cuentan: no dependen del cliente. Con `--perf` quedan 4 por segundo, las de la propia línea del registro.

**Precarga en un hilo** (`recursos/precarga.*`):
- **El problema era E/S, no CPU:**
  - decodificar un medio tile cuesta 0,3 µs, pero en el cliente salían ~50 µs;
  - los `.pak` están mapeados en memoria (`Tile.pak` 450 MB, `Sprite*.pak` ~5 GB), y la primera lectura de un `.til` o un `.spr` esperaba al disco;
  - eran 20–40 ms en el primer frame de una zona.
- **Ahora:**
  - el hilo principal pide el archivo y sigue dibujando sin él;
  - un hilo trae sus páginas a memoria (`madvise(WILLNEED)` y una lectura por página);
  - uno o dos frames después, el hilo principal lo decodifica ya en memoria;
  - las colas son SPSC sin locks (`AnilloBytes`).
- **No se nota:** el suelo y los objetos del mapa se piden en un margen fuera de la pantalla. Un personaje nuevo aparece un frame más tarde.
- **Siguen en el momento** las imágenes de interfaz, los PNG, los íconos y los glifos: son pocos y van en pantallas fijas.

**Reemplazo LRU del atlas** (`render/atlas.*`):
- **Antes:** con las 8 páginas (128 MB) llenas, lo nuevo dejaba de dibujarse.
- **Ahora:**
  - se recuerda en qué frame se dibujó cada página y, al llenarse, se vacía la usada hace más tiempo;
  - nunca se vacía una página dibujada en el frame actual ni la 0 (blanco, glifos e interfaz);
  - `Graficos` olvida las regiones de la página vaciada y las vuelve a decodificar si se piden.
- **Verificado:**
  - tests con un atlas de juguete;
  - en el cliente con `LIN_ATLAS_PAGINAS=4 LIN_ATLAS_TAM=1024`: 9 reemplazos caminando, la escena completa y sin errores con ASan y UBSan.
- **Medido:** el uso normal del pueblo es de 2 páginas.

**Caché en disco: no se hizo.**
- Decodificar es barato (0,3 µs por tile); lo caro era leer del disco, y una caché en RGBA8 ocuparía el doble que el RGB555 original y también habría que leerla.
- Lo único caro en CPU son los PNG de la selección de personaje (~9 ms en total), una vez, al cambiar de pantalla.

**Segmentos de mapa en un hilo** (`mundo/cargador_segmentos.*`):
- **Antes:** los `.s32` se leían en el hilo principal (0,5–1 ms cada uno); al cruzar a una zona nueva entraban varios en el mismo frame (hasta ~5 ms). Nunca se liberaban: ~100 KB por segmento visitado.
- **Ahora:**
  - el render los pide sin esperar (`segmento_cargado`) y los recibe uno o dos frames después; la lógica del juego (celdas pasables) los sigue cargando en el momento (`segmento`);
  - `actualizar_segmentos`, una vez por frame, guarda los terminados y libera los que quedan a más de 2 segmentos del borde de lo que se dibuja.
- **Objetos del mapa por rectángulo:** el render descarta objetos enteros fuera de la pantalla sin mirar sus piezas. Dibujarlos pasó de ~0,8 ms a menos de la mitad, y paga la comprobación de transparencia.
- **Medido** (Release, 1600×900, `.move` a Gludio y 180 pasos): CPU media 1,13–1,20 ms, igual que antes de agregar la transparencia (1,14 ms; la variación entre corridas es de ±0,07). Peor frame después del segundo 3: 2,3–5 ms (antes 4,25).
- **Memoria:** 12 teletransportes por el continente con el RSS plano en ~160 MB.

**Arenas de textos:** los nombres de los objetos y del inventario se guardaban en arenas que solo crecían (cada cantidad nueva de flechas agrega el nombre). Ahora, pasados 256 KB, se compactan con solo los vivos (`compactar_textos`).

**Fugas:** `leaks --atExit` en una sesión de juego: 415 bloques (26 KB), todos de macOS (conexiones XPC de AppIntents al abrir la ventana); ninguno del cliente.

**Probado y descartado:** un radix sort con los 5 histogramas en una pasada y pares (clave, índice) contiguos no mejoró el orden de la lista de dibujo (dentro de la variación).

**Pendiente o conocido:**
- **Primer frame del juego:** el primer segundo todavía tiene un frame de ~20 ms: las imágenes de la interfaz y el armado inicial.
- **Ritmo de pantalla:** con vsync a 120 Hz quedan ~6 ms de margen en el peor frame medido.

## Tests

```
LINEAGE_DIR=... ./build/debug/lin_tests
[doctest] test cases: 62 | 62 passed | 0 failed
```

El último, en `tests/test_mundo.cpp`: los atributos de un ítem identificado en texto (un arma y una armadura). Resucitar e invisible están en el caso de muerte y reinicio de `tests/test_mundo.cpp`; sus paquetes y comandos, en `tests/test_proto.cpp`.

Los 2 últimos, en `tests/test_mundo.cpp`: la arena de textos se compacta sin cambiar los nombres, y los segmentos del hilo de carga (iguales a los cargados en el momento, objetos con su rango y rectángulo, y liberación de los lejanos).

El del entregable 9 está en `tests/test_proto.cpp`: pregunta, intercambio, título, emblema, la página del clan (3 datos que pasan a 5) y los bytes de los comandos sociales.

Los 6 últimos:
- en `tests/test_recursos.cpp`: las sombras del mapa 2005 son tiles translúcidos, y el resplandor de los PNG pasa a alfa;
- en `tests/test_atlas.cpp`: estantes, reemplazo de la página usada hace más tiempo y páginas protegidas;
- en `tests/test_asignaciones.cpp`: 10 vueltas de decodificar movimientos y chat de 50 objetos, aplicarlos, interpolar, precarga y atlas, sin una sola allocation (con un control de que el contador cuenta).

Los 7 casos del entregable 4 están en `tests/test_chat.cpp`:
- prefijos de canal y formato de `C_Chat` / `C_ChatWhisper`;
- decodificación de chat normal, grito, sistema, susurro, NPC, mensajes y paquetes de estado;
- anillo del chat, recorte CP949 y globos;
- `string-e.tbl` y `desc-e.tbl` reales;
- porcentaje de EXP y ajuste de línea.

Sin `LINEAGE_DIR` se saltan los tests que necesitan los recursos del juego, como en CI.

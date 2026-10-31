# Plan: sin parámetros, servidores en el login y control en vivo para pruebas

**Estado:** pasos 1 (servidores en `settings.ini` y en el login) y 2 (control en vivo base) hechos.

Objetivos:
1. El cliente se usa sin parámetros `--algo`, tampoco para el servidor. Todo sale de
   `settings.ini` y de la propia interfaz.
2. El login tiene un menú desplegable con los servidores registrados en `settings.ini`.
3. Las pruebas usan una interfaz **en vivo**, un socket de archivo (Unix domain socket), para mandarle
   comandos al cliente que ya está corriendo y consultar su estado.
4. Esa interfaz se compila solo en las versiones de desarrollo (`#if LIN_CONTROL`). La versión final
   no la incluye: ni el código ni el socket.

---

## 1. Lo que hay hoy

- **53 parámetros** en `iniciar_app` (`src/app/app.cpp`), más 3 del servidor y de la carpeta del
  juego en `cargar_config` (`src/plataforma/rutas.cpp`). Además, cualquier clave de `settings.ini`
  se puede pasar como `--clave=valor`.
  - **Servidor y carpeta:** `--juego`, `--host`, `--puerto` (y `--protocolo`, `--usuario`, `--clave`
    por la regla general).
  - **Usuario y depuración:** `--ventana=AxB`, `--perf`, `--depuracion`, `--mudo`.
  - **Abrir ventanas:** `--inventario`, `--estado`, `--hechizos`, `--opciones`, `--teclas`,
    `--social`, `--salida`.
  - **Automatización (unos 40):**
    - `--auto`, `--personaje`, `--crear`, `--clase`;
    - `--captura*`, `--salir`;
    - `--decir`, `--pulsar`, `--arrastrar`, `--mantener*`, `--medio`, `--ctrl`;
    - `--hablar`, `--npc-accion`, `--tienda*`, `--cantidad`, `--aceptar`;
    - `--atacar`, `--lanzar`, `--usar*`, `--soltar`, `--dar`, `--borrar`, `--recoger`;
    - `--pasos`, `--ruta`, `--girar`, `--ofrecer`, `--forma`, `--destino`, `--subir`, `--alt`;
    - `--reiniciar*`, `--captura-proyectil`.
- **Variables de entorno:**
  - `LIN_CONFIG` (otro `settings.ini`);
  - `LINEAGE_DIR`, `LINEAGE_HOST`, `LINEAGE_PORT`;
  - `LIN_TRAZA` (paquetes), `LIN_HTML` (páginas);
  - `LIN_ATLAS_PAGINAS`, `LIN_ATLAS_TAM`.
- **La automatización** es la estructura `Automatizacion` (`app.h`). Se consulta en unos **114
  lugares de 16 archivos** (`a.aut.*`): cada sistema revisa "si hay algo pendiente para mí", con
  tiempos en frames (`--captura-espera`, el `_` de `--pulsar`, un `--arrastrar` cada 180 frames).
- **El problema de fondo:** las pruebas se escriben a ciegas.
  - Hay que adivinar en qué frame va a estar abierta una ventana. La velocidad cambia entre
    máquinas (60 o 120 fps).
  - Un clic que llega antes de tiempo arruina la prueba (pasó con el tablón y con el nombre de la
    casa).
  - No se puede mirar el estado sin una captura o la traza.
  - Cada prueba arranca un cliente nuevo y repite el login (unos 5 s).
- **El servidor:** `host`, `puerto` y `protocolo` en `settings.ini`. La tabla de opcodes se elige al
  arrancar (`app.cpp`) y la conexión se abre al apretar "login" (`iniciar_login`).

---

## 2. Servidores en `settings.ini` y en el login

### Formato

Una línea por servidor, en el orden en que aparecen en el menú:

```ini
servidor=Local de pruebas|127.0.0.1|2000|jp363
servidor=Otro|play.ejemplo.com|2000|s3ep1
servidor_elegido=Local de pruebas
```

- **Campos:** nombre, host, puerto y protocolo, separados por `|`. El nombre no puede tener `|`; el
  separador permite espacios en el nombre.
- **`servidor_elegido`:** el último usado. Se guarda al conectar.
- **Migración:** si el `settings.ini` tiene `host=`/`puerto=`/`protocolo=` y ningún `servidor=`, se
  convierte en `servidor=Servidor|host|puerto|protocolo` y se guarda en el formato nuevo.
- **Sin ninguno** (primer arranque): se agrega `Local|127.0.0.1|2000|jp363`.
- **En código:** `Config` cambia `host`, `puerto` y `protocolo` por `std::vector<Servidor>` y el
  índice elegido. `guardar_config` escribe una línea por servidor.

### La tabla de opcodes se elige al conectar

Hoy se elige una sola vez al arrancar. Pasa a `iniciar_login`: `tabla_opcodes(servidor.protocolo,
a.op)` justo antes de `a.red.iniciar(...)`. Si el protocolo no existe, el login muestra "Unknown
protocol version" en lugar de cerrar el cliente.

### La pantalla de login

- **El menú de servidores:** un control nuevo, `ui::desplegable`, en `src/ui/`.
  - Cerrado muestra el elegido; abierto, la lista encima de todo (`nivel_ventana`). Clic para elegir,
    Esc para cerrar.
  - Debajo del menú, en chico, `host:puerto · protocolo` del elegido. Hoy eso va en el pie de la
    UI simple.
- **"Reducida" = la UI simplificada** (confirmado). El menú va en las dos: en la simple, una fila
  "Server" arriba de la cuenta; en la original, arriba del recuadro 59.img, con `host:puerto
  protocolo` debajo.
- **La clave** (confirmado): `clave=` de `settings.ini` ya no se lee ni se guarda. La cuenta se
  recuerda por servidor (quinto campo de `servidor=`) al entrar bien.
- **Editar la lista de servidores** queda fuera de este plan: se hace editando `settings.ini`. Si
  hace falta después, "Add / Edit" en el mismo login.

---

## 3. Control en vivo (solo desarrollo)

### Compilación

- **Opción de CMake** `LIN_CONTROL`, con `ON` en los presets `debug` y `debug-asan` y `OFF` en
  `release`. Define `LIN_CONTROL=1` o `0`.
- **`src/app/control.cpp`** se compila solo con `LIN_CONTROL`, igual que la tabla `Automatizacion`
  que reemplaza. En el resto del código, los ganchos van entre `#if LIN_CONTROL`. Se busca que sean
  pocos (§3.4).
- **Comprobación de que no se cuela:** un paso de CMake (o un test del preset `release`) busca en el
  ejecutable final el texto del socket (`"lineage-control"`) con `strings`. Si lo encuentra, falla.

### El socket

- **Unix domain socket** (`AF_UNIX`, `SOCK_STREAM`). Lo tienen macOS, Linux y Windows 10 (1803) o
  más nuevo (`afunix.h`). Es el mismo código con `#ifdef _WIN32` solo para los includes y
  `closesocket`. MULTIPLATAFORMA.
- **Ruta:** `$LIN_CONTROL` si está definida; si no, `$TMPDIR/lineage-control-<pid>.sock`. Al
  arrancar se escribe en la salida (`Control en vivo: /ruta`), así una prueba con varios clientes
  sabe a cuál hablarle.
  - macOS limita la ruta a 104 bytes: si no entra, se avisa y no se abre.
- **Permisos:** `0600`, solo el usuario. Solo local: no hay puerto TCP.
- **Sin hilo nuevo:** el socket y las conexiones son no bloqueantes y se revisan una vez por frame,
  al principio de `frame_juego` (con `poll` o `accept`/`recv` con `O_NONBLOCK`). Los comandos corren
  en el hilo principal, entre los eventos de red y el dibujo, sin locks. Varias conexiones a la vez
  (máximo 4).
- **Al cerrar:** se borra el archivo del socket.

### El protocolo: líneas de texto

Una petición por línea; la respuesta son líneas que terminan con `ok` o `error <motivo>`. Los
datos van como `clave=valor` separados por espacios; un valor con espacios va entre comillas.

```
> estado
pantalla=juego personaje=Charla mapa=4 x=33421 y=32826 hp=995/995 mp=211/211 nivel=81
ok
> hablar Auction
ok
> esperar ventana=subastas 5000
ok
> clic 300 101
ok
> esperar html=agsel 3000
ok
> captura /tmp/x.bmp
ok
```

Un comando que tiene que esperar algo (`esperar`, `login`, `entrar`) no bloquea el juego: la
conexión queda "en espera" y se responde en el frame en que se cumple o vence el plazo.

### Los comandos

Cada uno reemplaza uno o más parámetros de hoy.

| Grupo | Comandos | Reemplaza a |
|---|---|---|
| Sesión | `login [servidor] usuario clave`, `personaje <nombre>`, `crear <nombre> <clase>`, `salir`, `volver-login` | `--auto --usuario --clave --personaje --crear --clase --salir` |
| Entrada cruda | `clic x y [der\|medio]`, `arrastrar x1 y1 x2 y2`, `mantener x y ms`, `tecla <nombre>` (con `ctrl+`, `alt+`, `shift+`), `texto "..."` | `--arrastrar --mantener --medio --pulsar --ctrl --alt` |
| Juego | `decir "..."`, `mover x y`, `ruta 0123`, `girar d`, `atacar <nombre\|#gfx>`, `lanzar <hechizo> [objetivo]`, `recoger` | `--decir --pasos --ruta --girar --atacar --lanzar --recoger` |
| NPC y comercio | `hablar <nombre>`, `accion <vínculo>`, `tienda <fila:N...>`, `cantidad N`, `responder si\|no\|"texto"` | `--hablar --npc-accion --tienda --cantidad --aceptar` |
| Ítems | `usar <ítem> [sobre <ítem>]`, `soltar <ítem>`, `dar <ítem> <a quién>`, `borrar <ítem>`, `ofrecer <ítem>` | `--usar --usar-sobre --soltar --dar --borrar --ofrecer` |
| Ventanas | `abrir <inventario\|estado\|hechizos\|opciones\|social\|teclas\|salida>`, `cerrar <...>` | `--inventario --estado ...` |
| Consultas | `estado`, `objetos [radio]`, `inventario`, `ventanas`, `html`, `chat [n]`, `paquetes [n]` | capturas y `LIN_TRAZA` para mirar |
| Esperas | `esperar pantalla=X\|ventana=X\|html=X\|paquete=OPCODE\|chat~"texto"\|objeto=nombre [ms]` | `--captura-espera` y los `_` de `--pulsar` |
| Captura | `captura <ruta.bmp> [region x y w h]` | `--captura --captura-pantalla --captura-proyectil` |
| Depuración | `traza on\|off`, `html-traza on\|off`, `perf on\|off`, `depuracion on\|off`, `mudo on\|off`, `ventana AxB`, `config clave=valor` | `LIN_TRAZA`, `LIN_HTML`, `--perf`, `--depuracion`, `--mudo`, `--ventana` |

- **Los que no tienen equivalente** porque eran atajos para una prueba puntual (`--reiniciar-atributos`,
  `--tienda-propia`, `--subir`, `--forma`, `--destino`, `--captura-proyectil`): se hacen con los
  comandos de arriba (clics sobre la ventana, `accion`, `decir "/shop ..."`) o con un comando chico
  propio si hace falta.
- **Las consultas** devuelven lo que hoy se adivina mirando capturas: posición, vida, ventana
  abierta, página HTML con sus vínculos, ítems con id y nombre, objetos cercanos con id, gfx,
  nombre y posición en pantalla. Con eso una prueba hace clic donde está el objeto, sin coordenadas
  fijas.

### Cómo se conecta con el juego (los 114 `a.aut.*`)

La idea es invertir la dirección:
- **Hoy:** cada sistema pregunta "¿la automatización quiere algo de mí?" en cada frame.
- **Ahora:** `control.cpp` llama a las mismas funciones que usa la interfaz:
  - `activar` (`npc_ui`), `dar_item` y `borrar_item` (`cantidad_ui`);
  - `enviar_chat`, `abrir_pagina`, `responder_pregunta`, `elegir_personaje`;
  - la cola de entrada (clics y teclas inyectados en `EntradaFrame`, como hoy `--arrastrar`).

Consecuencias:
- La mayoría de los `a.aut.*` desaparecen. Las funciones que hoy son internas y hagan falta se
  declaran en `app.h`, como `adena_inventario`.
- Quedan pocos ganchos entre `#if LIN_CONTROL`:
  - el `poll` del socket al principio del frame;
  - la inyección de entrada en `leer_entrada`;
  - el aviso de eventos para `esperar` (pantalla, ventana, página HTML, paquete recibido, línea de
    chat).
- **El aviso de eventos** es una función `control_evento(App&, const Evento&)`, llamada desde el
  mismo bucle de `app.cpp` que reparte los eventos. Más `control_pantalla(App&)` al cambiar de
  pantalla.
- **Las respuestas automáticas** (`--aceptar`, `--cantidad`) pasan a ser explícitas:
  `esperar pregunta` y después `responder si`. Así la prueba ve qué preguntó el servidor.

### Herramienta del lado de las pruebas

`herramientas/lin_ctl.py`, sin dependencias, con dos usos:
- **En la terminal:** `lin_ctl.py /ruta.sock estado`, o interactivo, una línea por comando.
- **Como módulo** para escribir pruebas en Python:

  ```python
  c = Control.arrancar(config="...", juego="...")  # lanza el cliente y lee la ruta del socket
  c.login("prueba2", "prueba2")
  c.personaje("Charla")
  c.hablar("Auction")
  c.esperar(ventana="subastas")
  c.clic_en_objeto(...)
  ```

Las pruebas en vivo que hoy se repiten a mano (tablón, correo, casas, asedio…) quedan como scripts
en `herramientas/pruebas/` y se pueden volver a correr.

---

## 4. Qué queda como parámetro o variable de entorno

- **Versión final:** **ningún parámetro.**
  - La carpeta del juego se elige con el diálogo del primer arranque, como hoy.
  - El servidor se elige en el login.
  - El tamaño de la ventana se guarda en `settings.ini` al cerrar (`ventana=AxB`); hoy no se
    guarda.
  - `LINEAGE_DIR`, `LINEAGE_HOST` y `LINEAGE_PORT` se quitan; no hacen falta con el menú.
- **Desarrollo (`LIN_CONTROL`):**
  - `LIN_CONFIG`: otro `settings.ini`. Hace falta antes de que exista el socket, para no pisar el
    del usuario y para correr varios clientes a la vez.
  - `LIN_CONTROL`: la ruta del socket.
  - `LIN_ATLAS_*` (pruebas de memoria del atlas): se quedan como variables de desarrollo.
- **Tests (`lin_tests`):** siguen con `LINEAGE_DIR`. Es otro ejecutable y no cambia.

---

## 5. Orden

Cada paso compila, pasa los tests y deja el cliente usable.

1. **Servidores en `settings.ini`:** el formato nuevo con migración, la tabla de opcodes al
   conectar y el desplegable en el login. Test: leer y escribir el formato (`tests/test_core.cpp`).
2. **Control en vivo, base:**
   - la opción `LIN_CONTROL`, el socket y el protocolo de líneas;
   - `estado`, `login`, `personaje`, `decir`, `clic`, `tecla`, `texto`, `esperar` (pantalla y
     paquete), `captura`, `salir`;
   - `lin_ctl.py`.

   Test: el comando de `strings` en el ejecutable de `release`.
3. **Control en vivo, el juego:** el resto de los comandos y consultas, llamando a las funciones de
   la interfaz.
4. **Pasar las pruebas:** reescribir con `lin_ctl.py` algunas de las pruebas en vivo ya hechas
   (tablón, correo, casas, chambelán) para validar que alcanza.
5. **Quitar lo viejo:**
   - los 53 parámetros, la estructura `Automatizacion` y los `a.aut.*`;
   - `--clave=valor` de `cargar_config`;
   - `LINEAGE_HOST` y `LINEAGE_PORT`.

   Y actualizar:
   - `AGENTS.md`: cómo se prueba ahora;
   - `ENTREGABLES.md`;
   - `DESCUBRIMIENTOS.md`.
6. **Guardar el tamaño de la ventana** en `settings.ini`.

**Tamaño:** el paso 1 y el paso 2 son una sesión cada uno; el 3, una o dos; el 4 y el 5, una.

---

## 6. Riesgos y decisiones abiertas

1. ~~"Login reducido"~~ y ~~la clave~~: confirmados (§2).
3. **Agentes en paralelo:** `AGENTS.md` describe pruebas con parámetros que usan otras sesiones (por
   ejemplo, la rama 3D).
   - Mientras dure la migración, los parámetros y el control conviven (paso 5 al final).
   - La rama `agente/modelos-3d` tendrá que actualizar sus scripts al unirse.
4. **Windows y `AF_UNIX`:** requiere Windows 10 1803 o más nuevo. Si hiciera falta algo más viejo,
   el mismo protocolo puede ir por un named pipe; queda fuera de este plan.
5. **Comandos que esperan:** un `esperar` sin plazo podría colgar una prueba. Todos llevan plazo
   (por defecto 10 s) y responden `error plazo`.

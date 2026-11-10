# Guía para trabajar en el cliente (personas y agentes)

Esta guía es para quien retoma el proyecto sin el contexto de las conversaciones anteriores, incluidos
varios agentes que trabajan **en paralelo** y hacen push al mismo repo. Léela entera antes de tocar
código. El idioma del proyecto es el **español**: código (identificadores), comentarios, docs y commits.

- **Qué es:** cliente nuevo de Lineage 1 en C++20 + SDL3 (SDL_GPU), con diseño orientado a datos.
  Lee los recursos de una instalación original del juego y habla el protocolo L1J (versión JP 3.63c).
- **Estado:** entregables 1 a 10 terminados. El detalle, con la evidencia y lo pendiente, está en
  [ENTREGABLES.md](ENTREGABLES.md).
- **Plan:** [docs/analisis/PLAN_CLIENTE.md](docs/analisis/PLAN_CLIENTE.md) (11 entregables jugables).
- **Plan 3D** (modelos animados junto a los sprites): [docs/analisis/PLAN_3D.md](docs/analisis/PLAN_3D.md).
- **Plan de portado** (Windows y Linux, x64 y arm64): [docs/analisis/PLAN_PORTADO.md](docs/analisis/PLAN_PORTADO.md).
- **Protocolo:** [docs/analisis/PROTOCOLO_RED.md](docs/analisis/PROTOCOLO_RED.md) y `docs/analisis/opcodes/`.
- **Uso, flags y controles:** [README.md](README.md).
- **Descubrimientos y decisiones** (formatos, tiempos de los sprites, orden de dibujo, transparencias,
  particularidades del servidor, por qué la interfaz es así): [docs/DESCUBRIMIENTOS.md](docs/DESCUBRIMIENTOS.md).
  Leerlo antes de tocar el render, el movimiento o el combate.

## 1. Reglas que no se discuten

1. **Solo el servidor local.** Todas las pruebas van contra `servidor_local/` (127.0.0.1:2000).
   Nunca conectarse al servidor privado real ni a ningún otro servidor público.
2. **No subir recursos del juego.** Ni `.pak`, `.idx`, mapas, sprites, sonidos ni imágenes extraídas.
   Se leen de la instalación del usuario en tiempo de ejecución. Las capturas de `docs/capturas/` son
   la única excepción, porque documentan el trabajo. El repo es **privado**: no cambiar la visibilidad.
3. **Licencia GPLv3** ([LICENSE](LICENSE)): el código de formatos se escribió leyendo l1j-client (GPLv3)
   y `datos/sprite_table.txt` viene de ahí.
4. **Nada de asignaciones de memoria en el bucle del frame.** `tests/test_asignaciones.cpp` lo verifica.
   Usar la `Arena` del frame, `TablaSoa` y buffers reutilizados.
5. **Los cambios al servidor deben ser reproducibles.** `servidor_local/l1j-jp/` no está en el repo y se
   baja con `instalar.sh`. Cualquier cambio a ese servidor va en:
   - `servidor_local/instalar.sh` (pasos);
   - `servidor_local/*.patch` (archivos del servidor; ver `teleporter.patch`);
   - `servidor_local/corregir_db.sql` (datos de la base, idempotente).

   Un cambio hecho solo a mano en la base o en el servidor no existe para los demás.
6. **Antes de cada push:** compila sin warnings y pasan los tests (sección 4).

## 2. Puesta en marcha

```sh
# Requisitos (macOS, Apple Silicon): Xcode CLT, brew install cmake ninja
cmake --preset debug && cmake --build --preset debug        # también: release, debug-asan
servidor_local/instalar.sh                                   # una vez (OpenJDK, MariaDB en 3307, base, parches)
servidor_local/arrancar.sh                                   # deja el servidor en primer plano; servidor.log
export LINEAGE_DIR="/ruta/a/la/instalación/del/juego"        # la carpeta con Sprite*.pak, Text.pak, map/
LINEAGE_DIR="$LINEAGE_DIR" build/debug/lin_tests             # 62 casos; sin LINEAGE_DIR se saltan 7
```

- **Ejecutable:** `build/<preset>/lineage`, sin parámetros (la carpeta del juego y los servidores
  salen de `settings.ini`). La configuración del usuario está en
  `~/Library/Application Support/Lineage/LineageCliente/settings.ini`.
- **Servidor 3.80C** (protocolo `tw380`): `servidor_380/instalar.sh` y `arrancar.sh` (L1JGO-Whale, puerto
  7001; ver ENTREGABLES.md, "Servidor 3.80C"). Solo local, como el de 3.63.
- **No pisar esa configuración en las pruebas:** usar `LIN_CONFIG=/ruta/propia.ini` (sección 5; solo
  en `debug`). Ese `.ini` necesita `juego=` y, para el servidor local, nada más: sin `servidor=` se
  usa `127.0.0.1:2000 jp363`.
- **Cuentas:** se crean solas en el primer login (usuario = clave).
- **Personajes de prueba existentes** (en la base local de la máquina original, no en el repo):
  - cuenta `prueba`: Prueba (príncipe nivel 1), Maga (maga), Caballero (caballero), Exploradora;
  - cuenta `prueba2` (GM): Charla (príncipe nivel 81, líder del clan Prueberos) y Aprendiz (mago, en
    Talking Island junto al maestro Gereng). Hermione y Mago son del usuario: no tocarlos.
- **Volver GM a una cuenta**, para usar `.item`, `.level`, `.spawn`, `.addskill`, `.allbuff` y `.move`:

  ```sh
  "$(brew --prefix mariadb)/bin/mariadb" --socket=servidor_local/datos/mysql.sock -uroot l1jdb_363 \
    -e "UPDATE accounts SET access_level=200 WHERE login='micuenta'"
  ```

  El cambio vale en el próximo login.

## 3. Arquitectura en una página

```
socket ─► hilo lector (descifra) ─► anillo SPSC ─► decodificar() ─► Evento POD (en la Arena del frame)
                                                                     │
                     aplicar_eventos() (mundo: tabla SoA, jugador, chat, inventario)
                     + manejadores de la app: eventos_npc, eventos_forma, eventos_social...
                                                                     │
pantallas (app/pantalla_*.cpp) ─► lista de dibujo ─► radix sort ─► 1 llamada instanciada (SDL_GPU)
comandos (proto/comandos.h: Escritor) ─► anillo ─► hilo escritor (cifra) ─► socket
```

| Módulo | Responsabilidad | Depende de |
|---|---|---|
| `src/core` | tipos, `Arena`, `MapaHash`, `TablaSoa`, `AnilloBytes`, conteo de asignaciones | nada |
| `src/plataforma` | archivos mapeados, socket, rutas y `settings.ini` | core, SDL |
| `src/proto` | cifrado, tabla de opcodes, decodificador, eventos, comandos, hilos de red | core, plataforma |
| `src/recursos` | `.pak/.idx`, DES de Text.pak, imágenes, tiles, sprites, mapas, fuentes, textos, HTML | core, plataforma |
| `src/mundo` | estado del juego: objetos (SoA), jugador, inventario, chat, mapas (segmentos cargados en un hilo); aplicar eventos | core, proto, recursos |
| `src/render` | SDL_GPU, atlas (texture array con LRU), gráficos de alto nivel | core, recursos, SDL |
| `src/audio` | dispositivo y streams de SDL3: música MP3 (dr_mp3), efectos WAV, ambiente | core, SDL |
| `src/ui` | UI inmediata (botones, cajas de texto) | render |
| `src/app` | bucle principal, pantallas, ventanas y una `*_ui.cpp` por función de juego | todo |

Hechos que conviene saber:

- **Opcodes:** el código usa ids internos (`Srv`, `Cli` en `proto/opcodes.h`). El número de cable sale de
  la tabla `jp363` en `opcodes.cpp`.
- **Decodificador:** `TABLA[]` en `decodificar.cpp` es un arreglo de punteros a función, **en el mismo
  orden que `enum class Srv`**. Un `static_assert` controla el largo, pero no el orden.
- **Eventos:** `Evento` (`proto/eventos.h`) es POD. El tipo es `e.tipo` (enum `Ev`), los números van en
  `e.v[]` con índices con nombre (`IT_GFX`, `PR_NUMERO`...) y los textos en `e.s[0..1]`, apuntando a la
  Arena del frame. Esos textos no viven más que el frame.
- **`Escritor`:** se arma con `cmd::...(tabla, ...)`. Mantenerlo vivo mientras se usa su `cuerpo()`.
- **Estado de la app:** `App` (`app/app.h`) tiene todo. Las flags de automatización están en `App::aut`
  y se parsean en `app.cpp`.
- **Ventanas:** `IdVentana` en `app.h`; nombres, rect por defecto y `abierta()` en `ventanas.cpp`. Se
  dibujan desde `pantalla_juego.cpp`.

### Receta: agregar un paquete del servidor

1. `opcodes.h`: agregar el id **al final** de `Srv`, antes de `CANTIDAD`. En `opcodes.cpp`: su número de
   cable en la tabla `jp363` y su nombre en `nombre_srv` (mismo orden).
2. `decodificar.cpp`: escribir la función (comentario de una línea con el formato) y ponerla **al final
   de `TABLA[]`**, en la misma posición que en el enum.
3. `eventos.h`: el valor de `Ev` y los índices de `v[]` que haga falta.
4. Aplicarlo en `mundo.cpp` (estado del juego) o en el manejador de la app que corresponda.
5. Test de decodificación en `tests/test_proto.cpp`, con bytes reales, que se pueden sacar con el comando `traza on` del control en vivo.

### Receta: agregar un comando del cliente

`Cli` en `opcodes.h` (al final), su número en `opcodes.cpp`, una función `inline` en `comandos.h`, y un
test de bytes en `test_proto.cpp`. El formato exacto sale del servidor:
`servidor_local/l1j-jp/src/jp/l1j/server/packets/client/C_*.java` (y `server/S_*.java` para los paquetes
del servidor). **Es la fuente de verdad del protocolo.**

### Receta: agregar una ventana o función de UI

Un archivo nuevo `src/app/<tema>_ui.cpp` (agregarlo a `CMakeLists.txt`). Luego:
- `IdVentana` en `app.h`, más su nombre y rect en `ventanas.cpp`;
- su botón (panel original y botones de texto);
- llamar a su `dibujar_*` y `procesar_*` desde `pantalla_juego.cpp`.

Mantener la lógica en el archivo propio; en los archivos compartidos, solo los enganches.

## 4. Verificación

- **Tests:** `cmake --build --preset debug && LINEAGE_DIR=... build/debug/lin_tests`. Antes de un push
  grande, también `debug-asan`.
- **Probar contra el servidor: control en vivo.** El cliente no tiene parámetros. En `debug` abre un
  socket (`LIN_CONTROL=/ruta` o `$TMPDIR/lineage-control-<pid>.sock`) y recibe comandos de a una
  línea (`ayuda` los lista; tabla en el README).
  - **Desde Python:** `herramientas/lin_ctl.py`: `c = Control.arrancar(config=...)`, después
    `c("login ...")`, `c("personaje ...")`, `c("esperar ...")`, `c("captura ...")`, `c.cerrar()`.
    Las pruebas en vivo ya escritas están en `herramientas/pruebas/` (`comun.py` arranca, entra,
    devuelve a los personajes a su lugar y cierra; `servidor.py` reinicia el servidor y restaura la
    base para las que lo modifican). `todas.py` las corre todas; con `--servidor`, también casas,
    castillos y guerra. **Antes de un push grande, `todas.py` (y `--servidor` si se tocó algo de
    eso).** Una prueba nueva: copiar la más parecida.
  - **Desde la terminal:** `herramientas/lin_ctl.py <socket> estado`, o interactivo.
  - **Empezar siempre con `mudo on`:** si no, se escucha en la máquina del usuario.
  - **Terminar siempre con `c.cerrar()`** (o el comando `salir`), o el proceso queda abierto.
  - **Usar `esperar`** (pantalla, paquete, html, ventana, diálogo, chat) en lugar de contar frames.
  - **Para hacer clic sobre algo,** consultar primero dónde está: `objetos` da la posición en
    pantalla; `clic-objeto` la usa directo.
  - **`release` no lo compila:** un paso de CMake (`cmake/sin_control.cmake`) falla si se cuela.
- **Trazas** (comandos del control):
  - `traza on`: cada paquete enviado y recibido, con sus primeros 16 bytes;
  - `traza-html on`: las páginas HTML con sus datos;
  - `perf on`: rendimiento;
  - `paquetes`: los últimos recibidos, sin traza.
- **Dos clientes a la vez** (grupo, intercambio, clan): dos `Control.arrancar` con distinto `config`.
  Con `girar <0-7>` quedan frente a frente.
- **Base de datos:** para verificar el efecto real (ítems, adena), consultar por el socket de arriba.
- **Evidencia:** cada entregable deja capturas en `docs/capturas/<n>_<tema>.png` (PNG, convertidas con
  `sips -s format png x.bmp --out y.png`) y una sección en ENTREGABLES.md con: qué hace, cómo se
  verificó (paquetes, base, capturas) y "Pendiente o conocido".

## 5. Trabajo en paralelo (varios agentes)

### Git

- `main` siempre compila y pasa los tests. No se hace push directo de trabajo a medio hacer.
- Una rama por tarea: `agente/<tema>` (ej. `agente/clima`, `agente/windows-socket`).
- Integrar seguido: `git fetch && git rebase origin/main`. Al terminar, PR a `main` con `gh pr create`
  (descripción: qué, cómo se verificó y qué queda pendiente). Si nadie revisa, el mismo agente puede
  mergear después de rebasear y volver a correr los tests.
- **Credenciales:** el repo es privado de la cuenta `javiercorrea`. El llavero de macOS y la cuenta activa
  de `gh` pueden ser otras (si `git push` dice "Repository not found", es eso). Hacer push con el token
  de esa cuenta, sin cambiar la configuración del usuario:
  `git -c credential.helper= -c 'credential.helper=!f() { echo username=javiercorrea; echo "password=$(gh auth token -u javiercorrea)"; }; f' push`.
- Para trabajar en la misma máquina sin pisarse:
  - `git worktree add ../cliente-<tema> -b agente/<tema>`;
  - cada worktree con su propio `build/`.
- **Commits:** en español, en imperativo y con alcance: `social: contar miembros en la página del clan`.

### Repartir el trabajo

- **Tomar una tarea:** abrir o asignarse un issue en GitHub (`gh issue create` / `gh issue edit --add-assignee @me`)
  con la etiqueta del área (`proto`, `render`, `ui`, `mundo`, `servidor`, `docs`).
- **Antes de empezar:** mirar los issues abiertos y las ramas `agente/*` (`git branch -r`), para no
  duplicar trabajo.
- **Buenas divisiones** (casi no se tocan entre sí):
  - **pendientes del entregable 10:** paneo, sonidos de equipar y de voz;
  - **portabilidad:** Windows y Linux (`grep -rn MULTIPLATAFORMA`; tabla en el README);
  - **entregable 11:** empaquetado y otras versiones de protocolo (tablas de opcodes desde
    `docs/analisis/opcodes`);
  - **pendientes de ENTREGABLES.md:** `S_Paralysis`, `S_Poison`, `S_CurseBlind`, marcadores de teleport,
    selector de emblema, alianzas.

### Archivos donde chocan los cambios (y cómo evitarlo)

| Archivo | Por qué choca | Cómo |
|---|---|---|
| `src/proto/opcodes.h/.cpp`, `decodificar.cpp` (`TABLA[]`) | todos los paquetes nuevos | agregar siempre **al final**; en un conflicto conservar ambos y revisar que `TABLA[]` siga el orden del enum |
| `src/proto/eventos.h` | valores de `Ev` e índices | al final del enum; índices con prefijo propio |
| `src/app/app.h` | estado y flags | agrupar los campos nuevos en un `struct` propio del tema |
| `src/app/pantalla_juego.cpp`, `ventanas.cpp`, `app.cpp` | enganches | solo llamadas de una línea; la lógica en `<tema>_ui.cpp` |
| `CMakeLists.txt` | lista de fuentes | una línea por archivo, en orden alfabético |
| `ENTREGABLES.md`, `README.md` (tabla de flags) | documentación | cada uno edita solo su sección o fila; las filas nuevas de flags van al final de la tabla |

### Recursos compartidos de la máquina

- **Servidor:** hay **un solo** servidor local (puerto 2000) y **una sola** base.
  - No reiniciarlo ni cambiar la base sin necesidad. Si hay que reiniciar, avisar en el issue.
  - Si ya está corriendo (`lsof -i :2000`), no arrancar otro.
- **Cuentas:** cada agente usa las suyas para no interferir con los personajes de otro: `agente<N>`,
  `agente<N>b`... (se crean solas). `prueba`/`prueba2` son de pruebas compartidas; no dejarlos en
  estados raros (mapas inexistentes, sin HP).
- **`settings.ini`:** es del usuario. En las pruebas usar `LIN_CONFIG=$TMPDIR/<agente>.ini`.
- **Temporales:** cada agente en su propio directorio temporal, no en `/tmp` directo.

## 6. Estilo de código

- C++20, 4 espacios, ~120 columnas. Nombres en español: funciones y variables en `minusculas_con_guion`,
  tipos en `PascalCase`, enums y constantes en `MAYUSCULAS`.
- **Comentarios:** en español. Explican el *porqué* y el formato de los paquetes (`// S_Trade: s con quién`).
  Seguir la densidad del código que rodea al cambio.
- **Datos:** sin herencia ni polimorfismo dinámico en el camino caliente. Datos planos, tablas y funciones
  libres.
- **Dependencias:** solo las de `cmake/dependencias.cmake` (SDL3, stb_image, dr_mp3, doctest, con
  versiones fijas). No agregar otras sin una razón fuerte.
- **Portabilidad:** lo específico de una plataforma queda en `plataforma/` y `render/`. Lo que falta para
  otra plataforma se marca con `// MULTIPLATAFORMA: ...`.
- Antes de realizar algún cambio, lista los archivos que necesitas y leelos en un paso. Has las ediciones para luego ejecutar los tests.

## 7. Mapa de documentos

| Documento | Para qué |
|---|---|
| `README.md` | compilar, ejecutar, controles, flags de automatización |
| `docs/DESCUBRIMIENTOS.md` | lo que se averiguó de los datos, el protocolo y el servidor, y las decisiones tomadas con eso |
| `ENTREGABLES.md` | estado, verificación y pendientes de cada entregable |
| `docs/analisis/PLAN_CLIENTE.md` | plan completo, arquitectura y presupuestos |
| `docs/analisis/PROTOCOLO_RED.md`, `docs/analisis/opcodes/` | cifrado, opcodes por versión, formatos |
| `docs/analisis/scripts/` | análisis del protocolo y del cliente original (Python; `requirements.txt`) |
| `servidor_local/` | instalar, arrancar y parchar el servidor de pruebas |
| `herramientas/prueba_login.py` | cliente mínimo en Python para depurar el protocolo |

## Agent skills

### Issue tracker

Issues de GitHub en `javiercorrea/lineage-cliente`, con `gh`. Ver `docs/agents/issue-tracker.md`.

### Triage labels

Los cinco roles por defecto (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). Ver `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` y `docs/adr/` en la raíz del repo. Ver `docs/agents/domain.md`.

# Plan: el Guerrero de Lineage 1 Reborn sin romper los clientes estándar

Estado: propuesta (2026-11-24). Nada implementado.

## 0. Qué es y qué sabemos

"Lineage 1 Reborn" es un servidor privado cuyo cliente (copia local en `~/Projects/Reborn`) es el
TW13032701 de Taiwán con datos agregados. Trae una clase nueva, el **Guerrero** (dos hachas; Howl,
Power Grip, Tomahawk). La versión del lanzador revisada es `20261123-warrior-public-r121`.

Hallazgos (revisión estática, sin ejecutar nada):

| Qué | Dónde (Reborn) | ¿Lo leemos ya? |
|---|---|---|
| Animación de la selección/creación | PNG 1132–1302 (200×270; primero hombre, al final mujer; el corte exacto está entre 1218 y 1302) | Sí (`g.png(n)`, igual que las 7 clases: PNG 25–1125) |
| Cuerpo en el juego | gfx 12490/12491 (hombre) y 12494/12495 (mujer), 288 `.spr` cada uno, en Sprite00–15.pak | Sí (Sprite00–15) |
| Acciones de esos gfx | `text/list.spz` de Text.pak (no están en `TW13032701.txt`) | Sí, desde #59 (`list.spz` con AES) |
| Descripción de la clase | `text/warrior-e.tbl` / `-k.tbl` | No (tabla nueva, trivial) |
| Íconos y textos de las habilidades | `spelldesc-*.tbl`, `spellexdesc-*.tbl`, img 3801–3935 y 7301+ (por confirmar) | Parcial |
| Lógica extra del cliente | `reborn_client.dll`: barras de MP del grupo (hook DirectDraw + paquete de grupo), "darkness patch", paquete "visual settings" | No: son extras, no hacen falta para el Guerrero |
| UI de creación | `xml/CreateUI.xml` cifrado | No (y no hace falta: dibujamos la pantalla nosotros) |

Desconocido (lo resuelve la fase 1): el **número de clase** que usa el servidor (en los servidores
coreanos L1J el Guerrero es la clase 7, después del ilusionista 6), la **versión de protocolo**
(TW13032701 hace pensar en `tw380`, sin confirmar), si hay **opcodes propios** de Reborn y el
**código de arma** de las dos hachas.

## 1. Principio de compatibilidad

Con un servidor estándar, el cliente se comporta **byte por byte igual que hoy**. El Guerrero
existe solo cuando la entrada de servidor elegida en el login carga la extensión `reborn`. No hay
`if (reborn)` repartidos por el código: todo lo que varía es **un dato** (tabla de clases) o vive
**en un módulo de extensión** (como los perfiles de `PLAN_MULTIVERSION.md`).

## 2. Configuración: un archivo por servidor, no un cliente distinto

`settings.ini` ya define servidores como `servidor=Nombre|host|puerto|protocolo|cuenta`. Propuesta:
un 6.º campo opcional con la extensión, que carga un archivo propio junto a `settings.ini`:

```ini
; settings.ini
servidor=Lineage 1 Reborn|<host>|2000|tw380|micuenta|reborn
```

```ini
; extensiones/reborn.ini (datos, sin código)
[clase.7]
nombre=Warrior            ; o clave de texto traducible
gfx_hombre=12490
gfx_mujer=12494
previa_hombre=1132,?      ; primer PNG, cantidad (por medir en la fase 1)
previa_mujer=?,?          ; el corte está entre 1218 y 1302
boton=?                   ; img del botón de clase en la pantalla de creación (por ubicar)
base=?,?,?,?,?,?          ; FUE DES CON SAB CAR INT: los que pida el servidor de Reborn
bonus=?
max=?,?,?,?,?,?
descripcion=warrior-e.tbl
```

- Sin el 6.º campo no se carga nada: los servidores actuales siguen igual.
- El mismo archivo puede llevar más adelante otros ajustes de Reborn (por ejemplo, encender las barras
  de MP del grupo) sin tocar `settings.ini`.

## 3. "Plugins": módulos de extensión compilados, no DLL cargables

Recomendación: **extensiones estáticas** (código C++ dentro del binario, registradas por nombre) en
vez de cargar `.dll`/`.dylib`/`.so` en tiempo de ejecución.

- Con 3 plataformas, firma de macOS y sanitizers, una ABI de plugins sería mucho trabajo, y cargar
  código ajeno desde una carpeta del juego es un riesgo de seguridad (justo lo que hace Reborn con
  `reborn_client.dll`).
- Interfaz mínima, solo con los puntos que de verdad se usen:

```cpp
// src/extensiones/extension.h
struct Extension {
    virtual ~Extension() = default;
    virtual void cargar_datos(const Ini&, TablaClases&) {}          // agrega clases, textos
    virtual bool paquete(u8 opcode, Lector&, Eventos&) { return false; }  // opcodes propios
};
Extension* extension_por_nombre(std::string_view);                   // "reborn" -> RebornExt
```

- **El Guerrero no necesita código de extensión** si la fase 1 confirma que el servidor usa los
  paquetes estándar con clase 7: todo se resuelve con la tabla de clases y `reborn.ini`. El módulo
  `src/extensiones/reborn/` aparece recién si hay opcodes propios (paquete de grupo con MP,
  "visual settings").

## 4. Fases

### Fase 1: verificar antes de programar (sin cambios de código)
1. Conseguir una cuenta de Reborn y **consultar a los administradores** si permiten un cliente de
   terceros: su lanzador tiene anti-trampas (GameGuard, `reborn_client.dll` lee los paquetes) y
   conectarse con otro cliente podría costar la cuenta.
2. Conectar con nuestro cliente, perfil `tw380`, usando el socket de control (`lin_ctl.py`) y
   registrando los opcodes desconocidos. Si el login o la lista de personajes fallan, el perfil
   no alcanza y se agrega uno (`reborn` en `opcodes.cpp`) siguiendo `PLAN_MULTIVERSION.md`.
3. En la lista de personajes (`S_CharPacks`), ver qué número de clase trae un Guerrero ya creado
   (con el cliente oficial en Windows, o con una cuenta que ya tenga uno).
4. Medir el corte hombre/mujer de los PNG 1132–1302 y ubicar el img del botón de clase.

**Salida:** número de clase, protocolo confirmado y lista de opcodes desconocidos. Si el servidor no
acepta nuestro cliente, el plan termina acá.

### Fase 2: tabla de clases desde datos (refactor sin cambio visible)
Pasar las 7 clases fijas a una `TablaClases` con las 7 de hoy como valor por defecto. Lugares que hoy
asumen 7:
- `src/app/pantalla_crear.cpp`: `BASE`, `BONUS`, `MAX`, `IMG_CLASE`, `POS_CLASE`, `PREVIA`, bucles `c < 7`.
- `src/app/pantalla_personajes.cpp`: `nombre_clase` y la animación de ranura (`clase*2+sexo`).
- `src/app/pantalla_juego.cpp:608`, `src/mundo/inventario.cpp:103,152` (clases que pueden usar el ítem),
  `src/app/original_window.cpp:144` (`DIV[cls]`).
- Revisar `hechizos_ui.cpp`, `social_ui.cpp`, `luz_ui.cpp`, `reinicio_ui.cpp`, `npc_ui.cpp`, que leen
  `PJ_CLASE`.

**Prueba:** capturas de login, selección y creación idénticas antes y después contra el servidor
local, y la prueba de cero allocs sin cambios.

### Fase 3: extensión `reborn` y el Guerrero en la selección y creación
- 6.º campo de `servidor=` + lector de `extensiones/<nombre>.ini` (en `rutas.cpp`, junto al resto de
  la configuración).
- La clase 7 aparece en la creación con su botón, animación, stats y descripción (`warrior-e.tbl`).
- Una clase desconocida (≥ cantidad de la tabla) se muestra como "?" y no rompe nada (ya pasa hoy).

### Fase 4: el Guerrero en el juego
- Cuerpo 12490/12494: sale de `list.spz`; verificar caminar, atacar, recibir daño y morir.
- Las dos hachas: el servidor manda un código de arma para la variante de acción
  (`tabla_sprites.h`: acción + código de arma). Ver qué código llega y si `list.spz` define esas filas.
- Habilidades: íconos y nombres desde las tablas de Reborn; ventana de habilidades del Guerrero
  (`hechizos_ui.cpp`, si agrupa por clase).
- Ítems "solo para Guerrero": bit de clase en el cuerpo del ítem (`inventario.cpp`).

### Fase 5 (opcional): opcodes propios de Reborn
Solo si la fase 1 encontró paquetes desconocidos que afectan el juego. Se decodifican en
`src/extensiones/reborn/` mediante `Extension::paquete`. Las barras de MP del grupo y el "darkness
patch" son extras estéticos: baja prioridad.

### Pruebas sin el servidor real
El servidor local Go (`servidor_380/l1jgo`) no tiene Guerrero. Para no depender de Reborn en cada
prueba, agregar ahí una clase 7 mínima (crear personaje con gfx 12490/12494, stats base) **en una
rama de pruebas**. Así las fases 2–4 se prueban en local con `lin_ctl.py`.

## 5. Datos y distribución

Los PNG, sprites y tablas son de Reborn: **no se copian al repo ni a los paquetes**. El cliente los lee
de la carpeta de juego de Reborn que configure el usuario (igual que hoy con la instalación
original). El repo guarda solo `extensiones/reborn.ini` de ejemplo (números, sin assets).

## 6. Riesgos

- **Política del servidor.** Usar un cliente no oficial puede violar las reglas de Reborn. Es lo
  primero que hay que aclarar (fase 1.1).
- **Protocolo modificado.** Si Reborn cambió paquetes base (no solo agregó opcodes), harán falta un
  perfil propio y pruebas contra su servidor, que no controlamos.
- **Datos que cambian.** Reborn se actualiza seguido (versión r121); los números de gfx o PNG pueden
  moverse, y por eso van en `reborn.ini` y no en el código.

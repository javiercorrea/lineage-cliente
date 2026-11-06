# Plan: noche, luz, mazmorras y efecto bajo el agua

Hoy el cliente recibe la hora, la luz de cada objeto y el modo submarino, pero no dibuja nada con eso
(`ENTREGABLES.md:161`). Este plan reproduce el comportamiento del cliente original a partir de su binario.
Es solo el plan: todavía no hay nada implementado.

**Fuentes.** `S3EP1U.bin` (cliente 2009 desempaquetado) y `TW13032701.DMP` (cliente 2013 en memoria), leídos
con capstone; servidor `l1j-jp`; base `classic363`. Las direcciones son de `S3EP1U.bin` salvo que se indique
otra cosa. Cada dato lleva su estado: **[V]** leído en el binario o en el servidor, **[H]** hipótesis,
**[?]** incógnita.

## 1. Cómo lo hacía el original

### 1.1 Las dos entradas: nivel ambiente y mapa de luz

Por frame (`sub_4626c0`), el original combina un **nivel ambiente** de 0 a 15 con un **mapa de luz** a media
resolución. Después oscurece el mundo ya dibujado, por píxel y por canal:

```
brillo = LUT[nivel][mascara >> 3]          // 0..31
canal  = canal * brillo / 31               // 49ce30 + tablas 8c9714/8c9f14/8ca714, generadas con /31
```

Si `brillo == 31` no toca el píxel. Por eso de día (nivel 15) no cuesta nada.

### 1.2 Nivel ambiente

**Mapas al aire libre: el nivel depende de la hora y de la estación [V] (`sub_4b2cc0`).** El amanecer y el
atardecer cambian con el día del año, en minutos del día:

| Día del año (`yd`) | Amanecer | Atardecer |
|---|---|---|
| hasta 172 | `452 - yd*192/172` | `986 + yd*197/172` |
| desde 173 | `260 + (yd-173)` | `1183 + (173-yd)*197/192` |

- Entre amanecer y atardecer, `t = 15`. Fuera de ese tramo, `t = 2`.
- Durante los 40 min previos al amanecer y los 40 min posteriores al atardecer, hay transición:
  `t = a*15/40 + 1`, con `a` entre 0 y 40.
- Resultado: `L = t*2/3 + 5`, con tope 15 y piso 5.
- **Clima:** si `L >= 10` y hay lluvia o nieve, `L - 1`.
- En el dibujo (`465fb3`), si `L < 15` se suma 1. **De día queda en 15 y de noche en 7.** A mediodía el clima
  no se nota (15 → 14 → 15): solo oscurece un punto en las transiciones. El velo de `dibujar_clima` es nuestro.

**Mapas oscuros: nivel fijo, sin hora [V].** El mapa es oscuro si está en la lista de 297 mapas de `0x6ddae8`
(anexo A) o si `mapa >= 0x4000` (casas). Con estas excepciones (valor final, ya con el +1):

| Mapas | Nivel | Brillo sin luz |
|---|---|---|
| todos los demás de la lista | 3 | 11/31 |
| 72-74, 310, 330, 400, 401, 522-524 | 5 | 14/31 |
| 307-309, 320 | 7 | 18/31 |
| 75-77 | 12 | 26/31 |
| 430 | 14 | 29/31 |

Fuera de la lista: mapas 5140-5142 → nivel 5; 5143-5145 → 15; mapa 1000 → 7.

**Hechizo LIGHT [V] (`468210`).** Solo si `S_Light` del propio jugador trae 20: guarda radio 14 y suma 5 al
nivel, con tope 15. **El servidor L1J manda 14, no 20**, así que aquí nunca se activa. Se implementa igual:
son dos líneas y otro servidor sí podría usarlo.

### 1.3 LUT de brillo

`LUT[n][v]` es casi lineal desde el brillo ambiente `A[n]` hasta 31, con
`A = {0,4,10,11,13,14,16,18,19,21,23,24,26,28,29,31}`. La fórmula
`A[n] + (31-A[n])*v/31` se desvía de la tabla como máximo en 1, medido en toda la matriz. La excepción es el
nivel 1 (hasta 4), que no se usa. **No hace falta copiar la tabla.**

### 1.4 Mapa de luz [V] (`4626c0`, `49a0d0`)

- **Búfer:** 1 byte por celda a media resolución (320×204 en el original), en 0 al empezar cada frame.
- **Fuentes:** objetos a ±35 × ±15 celdas del jugador, con `luz > 0`. Se corta cuando la suma de radios llega
  a 700.
- **Centro:** posición en pantalla de la celda + (24, 6) + el desplazamiento de movimiento.
- **Combinación:** **máximo**, no suma. Dos antorchas juntas no brillan más que una.
- **Máscaras:** archivo **`source.lmp`** de `Sprite.pak`: 16 offsets `u32`, 16 pares `(w, h)` `i32` y los datos.
  Son conos lineales elípticos 2:1.

| Radio (`S_Light`) | Fuente | Alcance en pantalla (±x, ±y) |
|---|---|---|
| 0, 1 | | 16 × 8 |
| 2, 3 | | 32 × 16 |
| 4, 5 | **jugador sin nada** (mínimo 5 en `468204`) | 72 × 36 |
| 6, 7 | | 144 × 72 |
| 8, 9 | **vela** (40005) | 200 × 100 |
| 10, 11 | **lámpara** (40001) | 240 × 120 |
| 12, 13 | **linterna** (40002, 40004) | 288 × 144 |
| 14 | **hechizo LIGHT** | 312 × 156 |

Los radios de los ítems están en `L1Item.getLightRange`. Los NPC usan `npc.light_size` (de 0 a 14). Los
ítems encendidos en el suelo también emiten (`S_DropItem`).

Otros detalles del original:

- `S_OwnCharPack` manda luz 0, pero el jugador siempre vale al menos 5.
- Si otro objeto trae `S_Light == 20`, se trata como 5.
- Los gfx `0x17ba` y su vecino se fuerzan a 14 (`4ccf15`). **[?]** No identifiqué el primer gfx: hay que
  releer `4ccf0c` en la implementación.

### 1.5 Bajo el agua [V] (`4891e0`, `4668c9`, `466a2f`, `4675d3`)

`S_MapID` trae un byte de modo submarino. Al entrar:

1. **Sin oscuridad:** se salta toda la pasada de luz. Bajo el agua no es de noche.
2. **Sonido:** arranca el sonido 330 (`0x14a`) y se detiene al salir. **[H]** Es un bucle: lo sugiere el par
   arrancar/detener (`4a9a50`/`4a9a90`), cuyo interior no leí.
3. **Ola:** si la bandera está activa (ver 1.6), cada **columna** de 1 px del área de juego se desplaza en
   vertical:

   ```
   dy(x) = (int)( sin((x + fase) * PI / 50) * 3.75 )     // periodo 100 px, ±3 px
   fase  = (fase + 1) % 100, cada 50 ms                  // la onda da una vuelta cada 5 s
   ```

   Constantes: periodo 5, amplitud 4 y resta 0,25, idénticas en el volcado de 2013 (allí 800×480).
4. **Tinte:** multiplica por canal la vista por **`1416.img`** (640×380, en `Sprite.pak`), un degradado azul
   casi negro arriba y casi blanco abajo. Va después de la ola, así que el degradado no ondula.

La interfaz no se toca, porque todo esto ocurre antes de dibujarla.

Los mapas con `mapids.underwater=1` son 63, 65, 253, 420, 558, 604 y 1011-1016. El servidor manda ese byte
según la base, y el cliente no necesita lista propia.

### 1.6 Lo que no encaja

- **[?] La bandera de la ola (`0x8169b4`, en 2013 `0x8bf816`) no tiene ningún escritor directo** en ninguno de
  los dos binarios. En el volcado vale 0, aunque allí el personaje no estaba en un mapa submarino (fase 0).
  Puede ser código desactivado o una escritura indirecta. **Recuerdas haber visto la ola, así que el plan la
  activa** y deja una constante para apagarla (decisión D1).
- **[?] `shade.dat` queda fuera.** La tabla se carga (`sub_462ed0`) y su temporizador corre, pero la rutina que
  la dibuja (`sub_464320`) no tiene llamadores ni punteros en 2009. Esa rutina hace
  `dst = dst/2 + gris(valor)`, un velo gris moteado que se mueve con la cámara. En 2013 la tabla sí tiene
  referencias en `0x4e12e0`, sin llamador identificado. **[H]** Es el efecto de agua antiguo (el archivo es de
  2001). Si comparar con el juego real muestra un moteado que falta, se agrega como fase aparte.
- **[?] `0x7026d0`** es un contador que fuerza nivel 15 mientras está activo (`450697`). No sé qué lo activa.
- **[?] Máscara por píxel de los mapas 303 y 606** (`4669a8`, tabla `0x7009cc`): excluye píxeles de la
  oscuridad. Queda fuera por ahora.

### 1.7 Otras piezas del mismo sistema [V]

- **Tinte fijo por mapa** (`467653`, `49ab70`): multiplicación por un color, mapa 430 `(203,170,218)` y mapa 601
  `(253,216,225)`. Reutiliza el pipeline nuevo: se incluye.
- **Clima:** el original no dibuja lluvia ni nieve en mapas oscuros (`4666a6`). La lista del anexo A reemplaza
  a `al_aire_libre()` de `clima.cpp:23`, cuyo comentario dice justamente que faltaba ese dato.
- **Borrachera** (`466b31`, también mapas 67 y 420): la misma técnica por filas, `sin((y+fase)*2π/100)
  * nivel * 3.5`. **[H]** Es `S_Liquor`: lo activa el manejador `4893a0`, cuyo opcode no comprobé. Hoy se mece
  toda la pantalla (`pantalla_juego.cpp`). Queda como mejora opcional (F6).
- **Ceguera:** ya está resuelta con un velo propio (`PROF_CEGUERA`) y no se toca. El original usa nivel 0 con
  el jugador en radio 5; el resultado se parece.

## 2. Diseño

### 2.1 Lógica sin GPU

Un módulo nuevo `src/mundo/luz.{h,cpp}`, con funciones puras y testeables:

```cpp
bool mapa_oscuro(MapId m);
int  nivel_ambiente(MapId m, i32 hora_seg, u8 clima, bool luz_20);
struct Mascaras { ... };                                  // sobre source.lmp, sin copiar
bool abrir_mascaras(Bytes lmp, Mascaras& m);
void estampar(u8* buf, int w, int h, const Mascaras&, int radio, int cx, int cy);   // máximo
void aplicar_lut(u8* buf, size_t n, int nivel);           // máscara -> brillo 0..255
```

**El mapa de luz se calcula en CPU**, igual que el original, en lugar de dibujar máscaras en la GPU:

- Se puede testear sin ventana.
- No requiere blending `MAX` ni un pase extra.
- Da el mismo resultado en todos los backends. Solo hay que portar shaders muy cortos (`MULTIPLATAFORMA`).
- El peor caso es acotado: el corte en 700 da unas 50 máscaras de radio 14, unos 2,4 MB de `max` por frame.

El búfer tiene el tamaño de la vista / 2. Se reserva solo cuando cambia la vista, nunca en el frame (regla 4).
Se cuenta un `perf` para confirmarlo.

**Ventanas más grandes que 640×480:** las máscaras conservan su tamaño en píxeles lógicos, así que la luz
alcanza lo mismo en el mundo. Queda oscuro lo que el original ni siquiera mostraba. Es lo correcto: el mundo
tiene la misma escala.

### 2.2 Dibujo

Hoy hay una sola llamada instanciada, ordenada por clave. Se parte en tramos:

```
Pase: [CAPA_FONDO, SUELO, MUNDO]          instanciado, como hoy
      multiplicar por el mapa de luz      si hay oscuridad
      -- fin de pase, copia + ola --      solo bajo el agua con ola
      multiplicar por 1416.img o el tinte
      [CAPA_EFECTOS ... ARRASTRE]         instanciado
```

- **Multiplicar:** un segundo pipeline con la misma entrada y blend `(ZERO, SRC_COLOR)`. Un rectángulo que
  cubre la vista hasta `panel_y`. Para el tinte fijo sirve con un texel blanco × color.
- **Mapa de luz:** textura `R8_UNORM`, subida por frame con un transfer buffer reutilizado y muestreo `NEAREST`.
  El aspecto en bloques de 2×2 y las 32 bandas también son del original.
- **Ola:** se cierra el pase, se copia el área a una textura y se dibuja un rectángulo con `fs_ola`:
  `x_log = floor(px.x / escala)`, `dy` con la fórmula de 1.5 y el borde en clamp. Sin agua no hay copia ni
  coste extra.
- **Sin efectos** (menús, día, sin agua): un solo tramo, igual que ahora. El contador `llamadas_` pasa a medir
  tramos; en `perf` se documenta el nuevo máximo.

API, un solo POD por frame para no multiplicar métodos:

```cpp
struct Postproceso {
    int w = 0, h = 0;                 // área de juego; h = 0: sin postproceso
    const u8* luz = nullptr; int luz_w = 0, luz_h = 0;
    bool ola = false; int fase = 0;
    const RegionAtlas* imagen = nullptr;  // 1416.img, multiplicada
    u32 tinte = BLANCO;
};
void Gpu::postproceso(const Postproceso& p);  // vale solo para este frame
```

**Qué se oscurece.** El corte entre tramos es el inicio de `CAPA_EFECTOS`. Quedan oscurecidos el suelo, los
objetos, los personajes y los hechizos (que van en `CAPA_MUNDO`). Quedan brillantes los nombres, las barras y
los globos. La lluvia y la nieve hoy están en `CAPA_EFECTOS`, pero el original sí las oscurece: se mueven a
`CAPA_MUNDO` con profundidad máxima, por debajo de `PROF_CEGUERA`, que está en otra capa y sigue igual.

**1416.img en el atlas:** se dibuja con el atlas actual escalada a la vista. Si una imagen de 640×380 no
conviviera bien con el LRU, se sube a su propia textura (se resuelve en F4 midiendo).

### 2.3 Cambios por archivo

| Archivo | Cambio |
|---|---|
| `src/mundo/mundo.h`, `mundo.cpp` | `Mundo::submarino`, leído de `e.v[1]` en `Ev::MAPA` (hoy se descarta); `Ev::LUZ` y `PONER_OBJETO` aplican las reglas de 1.4 |
| `src/mundo/luz.{h,cpp}` (nuevo) | sección 2.1 + anexo A |
| `src/render/gpu.{h,cpp}` | `Postproceso`, pipeline de multiplicar, `fs_ola`, textura de luz, tramos en `presentar()` |
| `src/app/luz_ui.cpp` (nuevo) | arma el búfer y el `Postproceso` desde `App`, con la fase de la ola y las máscaras cargadas al entrar al mundo |
| `src/app/pantalla_juego.cpp` | una línea en `frame_juego`, antes de `dibujar_clima` |
| `src/app/clima.cpp` | `al_aire_libre` → `!mapa_oscuro`; capa de la lluvia y la nieve |
| `src/app/sonido.cpp` | ambiente 330 mientras `mundo.submarino` |
| `src/app/control*` | comando de depuración `forzar-hora <seg>` (solo `debug`, ver 4) |
| `CMakeLists.txt`, `tests/` | archivos nuevos y tests |
| `docs/DESCUBRIMIENTOS.md`, `ENTREGABLES.md`, `README.md` | sección nueva, quitar el pendiente, capturas |

## 3. Fases

Cada fase compila, pasa los tests y se puede mergear sola.

| Fase | Entrega | Se ve |
|---|---|---|
| **F1** Datos | `Mundo::submarino`; reglas de `S_Light`; `luz.cpp` con nivel, lista y máscaras; `forzar-hora` | nada, solo tests |
| **F2** Pipeline | `Postproceso` con multiplicar y tramos; probado con un tinte fijo en mapas 430 y 601 | tinte de esos 2 mapas |
| **F3** Noche y mazmorras | mapa de luz, jugador, NPC e ítems; clima con la lista nueva; orden de capas | **noche, velas, mazmorras** |
| **F4** Agua: tinte | saltar la luz, `1416.img`, sonido 330 | **azul bajo el agua** |
| **F5** Agua: ola | copia, `fs_ola`, fase | **distorsión** |
| **F6** Opcional | borrachera por filas; máscara de 303 y 606; `shade.dat` si hace falta | según lo que falte |

## 4. Verificación

**Tests en `lin_tests`:**

- `nivel_ambiente`: mediodía → 15; medianoche → 7; 1 de enero a las 7:20 → 13; dentro de la transición,
  intermedio y monótono; un día de enero amanece más tarde que uno de junio; lluvia a mediodía → 15 y en la
  transición un punto menos; mapa 1 → 3; 75 → 12; 0x4000 → 3.
- `mapa_oscuro`: 1 y 2 sí; 0 y 4 no.
- LUT: la fórmula contra los valores del anexo B.
- `estampar`: dos máscaras solapadas dan el máximo; recorte en los cuatro bordes sin salirse del búfer.
- Con `LINEAGE_DIR`: `source.lmp` da `w=312, h=156` para el radio 14 y `1416.img` da 640×380.
- Decodificación de `S_MapID` con byte 1 y `Mundo::submarino`.
- `test_asignaciones`: 0 asignaciones por frame con la luz activa.

**En vivo, nueva `herramientas/pruebas/prueba_luz.py`, solo contra el servidor local**, con cuenta de agente,
`LIN_CONFIG` propio, `mudo on` y `c.cerrar()`:

1. Con `forzar-hora`, capturar mediodía, atardecer y medianoche en el mismo lugar: el brillo medio de una zona
   sin luz debe caer hasta alrededor de 18/31.
2. Encender vela, lámpara y linterna con `.item`: el radio medido en la captura debe crecer (±200, ±240, ±288 px).
3. Junto a un NPC con `light_size > 0`: tiene su propia zona iluminada.
4. Dos clientes: cada uno ve la luz del otro al encender un ítem.
5. `.move` a una mazmorra: oscura también a mediodía. Sin lluvia aunque el clima la indique.
6. `.move` al mapa 63: sin oscuridad, degradado azul, panel sin teñir, ola en capturas sucesivas y sonido
   330 según el log del control. Al salir vuelve todo a la normal.
7. `perf on` en la zona más poblada, de noche: el coste del frame no debe subir más de 1 ms, y debe verse con el
   número medido en `ENTREGABLES.md`.

**Comparación con el original:** capturas del juego real hechas por ti, en la misma posición, para ajustar lo
que no se puede deducir (ver D1).

## 5. Criterios de aceptación

- De noche se ve oscuro, salvo alrededor de personajes y NPC. Con vela o lámpara el radio crece de forma visible.
- Las mazmorras se ven oscuras a cualquier hora, y el mundo cambia con la hora, con transición de 40 min de juego.
- Bajo el agua la vista se tiñe de azul con ola; panel, ventanas y chat no cambian. Con ventana grande y con
  `escala > 1` la ola sigue viéndose por columnas de 1 px lógico.
- Nombres y barras se leen igual de noche.
- Tests sin warnings, `todas.py` en verde, 0 asignaciones por frame, y `release` sin cambios de control.

## 6. Riesgos

| Riesgo | Mitigación |
|---|---|
| **Portabilidad:** el shader solo existe en MSL (hoy también) | shaders mínimos, marcados con `MULTIPLATAFORMA`; la lógica está en CPU |
| **Rendimiento** con muchas luces grandes | corte de 700 igual al original; se mide en la prueba 7 |
| **La hora del servidor:** `S_GameTime` da segundos de juego, pero `4bad30` podría usar zona local | cambia el día del año como mucho en 1 y no se nota |
| **El servidor y el cliente discrepan:** el servidor considera noche de 18 a 6 fijo; el original usa estaciones | es igual en el original; el cliente manda en lo visual |
| **Se rompe el orden de dibujo** al mover la lluvia de capa | capturas de lluvia antes y después |
| **La lista de mapas es de 2009** y el servidor puede tener mapas nuevos | esos mapas quedan como "aire libre"; se corrige la lista si aparece alguno |
| **Datos del juego en el repo** (regla 2) | `source.lmp` y `1416.img` se leen de la instalación; en el repo solo van la lista de mapas y números, como los opcodes |

## 7. Decisiones

Con estos valores se puede empezar. Corregir cualquiera cuesta poco.

- **D1. Ola activada por defecto**, a pesar de 1.6, porque la recuerdas así. Constante `OLA_ACTIVA` en `luz_ui.cpp`.
  **Tu comparación con el juego real es lo que la confirma.**
- **D2. Fidelidad exacta en el nivel de noche (18/31, 58 %).** Es menos oscuro de lo que podría sugerir la
  memoria. Si se ve flojo frente al recuerdo, se ajusta `A[]` y queda documentado que se aparta del original.
- **D3. Sin opción en `ajustes` para apagar los efectos.** Se agrega si alguien la pide.
- **D4. Fuera: `shade.dat`, máscara de 303 y 606 y borrachera por filas** (F6, si hacen falta).

## Anexo A: mapas oscuros (`0x6ddae8`, 297 entradas)

Además, todo `mapa >= 0x4000`. La lista incluye 8 valores de ese rango que quedan cubiertos por la regla.

```
1-3, 7-28, 30-41, 43-56, 59-64, 72-82, 85-87, 101-237, 240-243, 248-251, 301, 304-310, 320, 330,
400, 401, 410, 420, 430, 441-444, 450-457, 460-467, 470-478, 481, 490-496, 522-524, 530-536,
541-543, 666, 781, 782, 1001-1005
```

## Anexo B: muestras de la LUT (`0x6e3638`, para el test)

```
nivel 3:  11 11 12 12 13 14 14 15 16 16 17 18 18 19 20 20 21 22 22 23 24 24 25 26 26 27 28 28 29 30 30 31
nivel 7:  18 18 18 19 19 20 20 21 21 21 22 22 23 23 24 24 25 25 25 26 26 27 27 28 28 28 29 29 30 30 31 31
nivel 12: 26 26 26 26 26 26 27 27 27 27 27 28 28 28 28 28 29 29 29 29 29 29 30 30 30 30 30 31 31 31 31 31
```

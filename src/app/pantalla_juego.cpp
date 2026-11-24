// Entregables 3 y 4: dentro del juego. Suelo isométrico de los .s32, objetos del mapa,
// personajes y NPCs animados con la tabla de sprites, movimiento propio con predicción, y la UI:
// simple (texto, estilo ASCII; por defecto) o el panel inferior original, con chat y ventana de
// estado en los dos casos (F2 alterna).
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include "app/app.h"
#include "mundo/ataque.h"
#include "mundo/luz.h"

namespace lin {

namespace {

// Barra de HP/MP sobre la cabeza, dibujada con las imágenes del cliente original (pak de Sprite): 930.img es la barra
// vacía, 931.img / 932.img el relleno rojo / azul, todas de 44x5 con marco dorado. El relleno se recorta a
// la fracción actual, como las barras del panel. (x, y) es la esquina superior izquierda.
constexpr u32 IMG_OVERHEAD_EMPTY = 930, IMG_OVERHEAD_HP = 931, IMG_OVERHEAD_MP = 932;

void overhead_bar(Graficos& g, int x, int y, u32 fill_img, i32 value, i32 max) {
    const RegionAtlas& empty = g.img(IMG_OVERHEAD_EMPTY);
    if (empty.valida()) g.gpu().dibujar(empty, x, y, clave_orden(CAPA_EFECTOS, 0));
    RegionAtlas fill = g.img(fill_img);
    if (!fill.valida() || max <= 0) return;
    fill.w = static_cast<u16>(fill.w * std::clamp(value, 0, max) / max);
    if (fill.w > 0) g.gpu().dibujar(fill, x, y, clave_orden(CAPA_EFECTOS, 1));
}


// Ataque: el anti-acelerador del servidor también controla el intervalo entre C_Attack (con la
// duración de la animación de ataque del sprite). Como al caminar, se usa un 5 % más
// (intervalo_propio, con haste y brave).
constexpr u32 ATAQUE_DESCONOCIDO_MS = 800;
// Si se sigue caminando, el paso nuevo empieza donde terminó el anterior (no en el frame en que
// se detecta), para que no haya una pausa por paso. Solo si el retraso es menor que esto: así el
// intervalo entre dos C_MoveChar nunca baja de lo que acepta el anti-acelerador (~660 ms).
constexpr u64 MS_ENCADENAR_PASO = 10;
// (intervalo_propio lo tiene en cuenta: ver mundo.h.)

// Panel inferior original: posiciones de draw_game.cpp y chat_window.cpp de l1j-client, relativas
// al bloque de 640×480 del original (Vista::hx, hy).
constexpr int HUD_Y = 366;  // desde esta fila hacia abajo está el panel: el clic no camina
constexpr u32 IMG_PANEL_IZQ = 1028, IMG_PANEL_DER = 1019, IMG_CHAT = 1042, IMG_ENTRADA_CHAT = 1039,
              IMG_CHAT_ADORNO = 1043, IMG_CHAT_ALAS = 1044;
// Orbe de la hora del juego (S3EP1U 0x416160, tabla de grupos 0x500228, tabla de imágenes de la interfaz [0x8f0b1c]): 12 grupos de dos horas del juego,
// {primera imagen, cantidad de cuadros, x del destello, y del destello}; la imagen en reposo es el último cuadro del grupo, dibujado en (295, 359).
struct GrupoOrbe { u16 primera, frames, x, y; };
constexpr GrupoOrbe ORBE_GRUPOS[12] = {{1237, 2, 305, 381}, {1239, 2, 298, 379}, {1241, 3, 293, 374}, {1244, 2, 292, 367},
                                       {1316, 2, 293, 360}, {1318, 2, 298, 354}, {1213, 2, 305, 352}, {1215, 2, 313, 354},
                                       {1217, 5, 318, 360}, {1222, 3, 319, 367}, {1225, 2, 317, 374}, {1227, 5, 312, 379}};
constexpr int ORBE_X = 295, ORBE_Y = 359;
constexpr u32 IMG_BARRA_EXP = 1029, IMG_BARRA_HP = 1030, IMG_BARRA_MP = 1031;
// Original chat log: top y 402 at size 0, 12 px row pitch, 5/8/11/14 rows for Ctrl+T sizes 0..3 (RE_ROUND4 U-A12).
constexpr int CHAT_PASO_ORIG = 12, CHAT_Y_ORIG = 402;
constexpr int CHAT_X = 124 + 20, CHAT_ANCHO = 342, CHAT_FILAS = 5;
constexpr u64 DUR_COMBATE = 900;  // ms que dura un texto de combate flotante (sube y se desvanece)
constexpr int ENTRADA_X = 124 + 30, ENTRADA_Y = 363 + 102;
constexpr size_t MAX_ENTRADA_CHAT = 60;

// UI simple: franja de texto abajo, de ancho completo. De arriba hacia abajo: CHAT_FILAS líneas de
// chat, la entrada ("> ") y una línea de estado con barras ASCII.
constexpr int MARGEN_SIMPLE = 6;
constexpr int ALTO_PANEL_SIMPLE = 4 + (CHAT_FILAS + 2) * ALTO_GLIFO + 6;
constexpr int CELDAS_BARRA = 8;    // caracteres de cada barra: [########]
constexpr int SEPARACION_SIMPLE = 10;  // entre campos de la línea de estado (entra en 640 px)

// La vista del juego crece con la ventana (Gpu::ajustar_vista). El panel queda abajo (el original,
// de 640 de ancho, centrado; el simple, de ancho completo) y el jugador, centrado en el área de
// juego que queda arriba.
struct Vista {
    int w, h;              // tamaño lógico
    int hx, hy;            // origen del bloque de 640×480 del original (panel inferior)
    int panel_y;           // desde esta fila hacia abajo está el panel: el clic no camina...
    int panel_x0, panel_x1;  // ...entre estas columnas
    int cx, cy;            // celda del jugador en pantalla (esquina superior izquierda del rombo + 24, 12)
};
Vista vista(const App& a) {
    Vista v;
    v.w = a.gpu.ancho();
    v.h = a.gpu.alto();
    v.hx = (v.w - 640) / 2;
    v.hy = v.h - 480;
    if (a.cfg.ui_simple) {
        v.panel_y = v.h - ALTO_PANEL_SIMPLE;
        v.panel_x0 = 0;
        v.panel_x1 = v.w;
    } else {
        v.panel_y = v.hy + HUD_Y;
        v.panel_x0 = v.hx;
        v.panel_x1 = v.hx + 640;
    }
    v.cx = v.w / 2;
    v.cy = v.panel_y / 2 + 37;  // 220 en 640×480 con el panel original, como antes
    return v;
}

// La misma vista en píxeles lógicos del mundo. Con el zoom solo del mapa (UI simple) el mundo tiene su
// propia escala y la GPU lo lleva a la interfaz multiplicando por zoom_mundo: así un zoom menor muestra más
// mapa sin achicar la interfaz. Sin ese zoom es igual a vista(a).
Vista vista_mundo(const App& a) {
    Vista v = vista(a);
    const float z = a.gpu.zoom_mundo();
    if (z == 1.0f) return v;
    auto m = [z](int x) { return static_cast<int>(std::lround(x / z)); };
    v.w = static_cast<int>(std::ceil(v.w / z));
    v.h = static_cast<int>(std::ceil(v.h / z));
    v.hx = m(v.hx), v.hy = m(v.hy), v.panel_y = m(v.panel_y), v.panel_x0 = m(v.panel_x0), v.panel_x1 = m(v.panel_x1);
    v.cx = m(v.cx), v.cy = m(v.cy);
    return v;
}
// El puntero en píxeles lógicos del mundo (para compararlo con lo que se dibujó en el mundo).
float raton_mundo_x(const App& a) { return a.entrada.mx / a.gpu.zoom_mundo(); }
float raton_mundo_y(const App& a) { return a.entrada.my / a.gpu.zoom_mundo(); }

// Profundidad de dibujo en la capa del mundo (32 bits): fila de pantalla del pie (23 bits, en
// cuartos de píxel), orden dentro de esa fila (6 bits: piezas de objetos de arriba hacia abajo y
// al final los personajes) y subcapa (3 bits).
constexpr u32 ORDEN_PERSONAJE = 63;
u32 profundidad(float fila_pantalla, u32 orden, u32 sub) {
    i32 v = static_cast<i32>(fila_pantalla * 4.0f) + (1 << 22);
    return (static_cast<u32>(v < 0 ? 0 : v) << 9) | ((orden & 63) << 3) | (sub & 7);
}

// Orden de dibujo de la interfaz original (renderizador de cuadro 0x465c30, inserción 0x460230): las celdas se recorren por fila de pantalla (12 px) de arriba
// abajo, después por columna de pantalla (24 px) de izquierda a derecha; cada celda dibuja su lista en orden de capa. Una pieza del mapa está en su propia
// celda con su propio byte de capa; el cuerpo de un personaje está en su celda en la capa 18 (la parte de los pies del .spr). Dentro de una celda
// y capa la pieza del mapa va después del objeto (las entradas agregadas después se dibujan primero). `sx`, `sy`: la esquina de la celda en pantalla.
u32 clave_celda(float sx, float sy, u32 capa, u32 sub) {
    const i32 r = std::clamp(static_cast<i32>(std::floor(sy / 12.0f)) + 512, 0, 1023);
    const i32 c = std::clamp(static_cast<i32>(std::floor(sx / 24.0f)) + 512, 0, 1023);
    return static_cast<u32>(r) << 22 | static_cast<u32>(c) << 12 | std::min<u32>(capa, 255) << 4 | (sub & 15);
}
constexpr u32 CAPA_CUERPO_ORIGINAL = 18;

// Acción a mostrar y desde cuándo:
//  - muerto: morir, detenida en el último frame (el cadáver);
//  - una acción puntual (ataque, daño, morir) ya empezada: esa, una vez;
//  - si no, caminar o quieto, en bucle.
// Las acciones llevan la variante del arma (acción + arma), salvo morir.
struct AccionVisible {
    const RangoAccion* rango = nullptr;
    u64 inicio = 0;
    bool puntual = false;  // al terminar se vuelve a caminar/quieto
    bool bucle = true;
    u8 accion = 0;  // acción que se dibuja (con el arma), para sus factores de cuadro (ritmo_de)
};
AccionVisible elegir_accion(const App& a, Slot s, u64 ahora) {
    const TablaObjetos& o = a.mundo.objetos;
    GfxId gfx = o.col<O_GFX>()[s];
    const bool muerto = o.col<O_ESTADO>()[s] == ESTADO_MUERTO;
    const u8 accion = o.col<O_ACCION>()[s];
    u8 arma = muerto ? 0 : o.col<O_ESTADO>()[s];
    AccionVisible v;
    // Puerta o estructura de castillo: la última de abrir, cerrar o dañarse, detenida en su último
    // frame (el estado la recuerda).
    if (accion_fija(a.sprites.tipo(gfx), o.col<O_ESTADO>()[s]) && a.sprites.accion(gfx, o.col<O_ESTADO>()[s])) {
        v.rango = a.sprites.accion(gfx, o.col<O_ESTADO>()[s]);
        v.inicio = o.col<O_TACCION>()[s];
        v.bucle = false;
        return v;
    }
    // Vendedor con la tienda abierta: sentado (el último cuadro de la acción 70).
    if (!muerto && vendedor(a, o.ids()[s]))
        if (const RangoAccion* r = a.sprites.accion(gfx, 70)) {
            v.rango = r;
            v.bucle = false;
            return v;
        }
    u8 base;
    if (muerto) {
        base = ACC_MORIR;
        v.inicio = o.col<O_TACCION>()[s];
        v.bucle = false;
    } else if (accion != 0xFF && ahora >= o.col<O_TACCION>()[s]) {
        base = accion;
        v.inicio = o.col<O_TACCION>()[s];
        v.puntual = true;
        // Solo el ataque y el daño suman la postura del arma; todo otro id de acción es absoluto (SetAction 0x4c96c0): un hechizo
        // (18, 19) o un gesto no debe volverse la fila de otra arma.
        if (base != ACC_ATACAR && base != ACC_DANADO) arma = 0;
    } else if (caminando(a.mundo, s, ahora)) {
        base = ACC_CAMINAR;
        v.inicio = o.col<O_TMOV>()[s];
        // Sprites de zancada (list.spz 111): los pasos alternan las medias zancadas 0 y 4, sea cual sea el arma.
        if (a.sprites.zancada(gfx)) {
            base = a.sprites.accion_caminar(gfx, o.col<O_ZANCADA>()[s]);
            arma = 0;
        }
    } else {
        base = ACC_QUIETO;
    }
    v.rango = a.sprites.accion(gfx, static_cast<u8>(base + arma));
    v.accion = v.rango ? static_cast<u8>(base + arma) : base;
    // El byte de estado de un monstruo (S_NPCPack) es la acción en la que reposa: el exe la guarda como acción actual
    // y, cuando estado + 3 (quieto) no tiene tabla, sigue mostrando la primera entrada de esa acción (la pose oculta, p. ej.
    // Antharas 20, no muerto hundido 13). Sin esto un monstruo hundido dibujaba la pose quieta de pie.
    if (!v.rango && base == ACC_QUIETO && arma > ACC_QUIETO && !muerto && tipo_objeto(a, s) != TIPO_PERSONAJE) {
        if (const RangoAccion* r = a.sprites.accion(gfx, arma)) {
            v.rango = r;
            v.inicio = ahora;
            v.bucle = false;
            return v;
        }
    }
    if (!v.rango) v.rango = a.sprites.accion(gfx, base);
    // TODO(PARCHE PROVISIONAL): sprite_table.txt no trae el "quieto" de estas formas. El list.spz de Text.pak sí
    // (las 75/80 ya lo usan), pero no reemplaza las filas que sprite_table.txt ya tiene (ver fusionar_lista).
    // Un personaje transformado en una forma sin "quieto" (los dark elf 52-80): el quieto de otra arma y,
    // si no hay, parado en el primer cuadro de caminar, no dando pasos en el lugar. Los objetos sin
    // quieto (los braseros, por ejemplo) sí se animan con caminar.
    if (!v.rango && base == ACC_QUIETO && !muerto && tipo_objeto(a, s) == TIPO_PERSONAJE) {
        for (int otra_arma : {4, 11, 20, 24, 40, 46, 50, 54, 58, 62})
            if (!v.rango) v.rango = a.sprites.accion(gfx, static_cast<u8>(ACC_QUIETO + otra_arma));
        if (!v.rango) {
            v.rango = a.sprites.accion(gfx, ACC_CAMINAR);
            v.inicio = ahora;
        }
    }
    if (!v.rango) v.rango = a.sprites.accion(gfx, ACC_QUIETO);
    if (!v.rango) v.rango = a.sprites.accion(gfx, ACC_CAMINAR);
    // Sin quieto ni caminar (la torre de guardia sana, la corona): su primera acción, quieta.
    for (int k = 0; !v.rango && k < 64; ++k) {
        v.rango = a.sprites.accion(gfx, static_cast<u8>(k));
        v.bucle = false;
    }
    return v;
}

// Personajes en la sombra de un árbol (F5). La sombra no se dibuja encima del personaje: lo
// taparía también donde el cuerpo sube por encima de ella. Quien pisa una celda con sombra se
// dibuja oscurecido, un poco menos que el suelo bajo la sombra (negro con alfa
// ALFA_TRANSLUCIDO, y con huecos).
constexpr u8 LUZ_EN_SOMBRA = 150;
bool en_sombra(App& a, int x, int y) {
    const SegmentoMapa* s = segmento_cargado(a.mundo, a.rec, x, y);
    if (!s) return false;
    if (s->sombra.empty()) {
        s->sombra.assign(CELDAS_SEGMENTO, 0);
        bool falta = false;
        for (const TileObjetoMapa& t : s->tiles_objetos) {
            // Las piezas de los objetos del borde pueden caer en el segmento vecino: esas no cuentan.
            if (!pieza_de_suelo(t) || t.y >= CELDAS_SEGMENTO || t.x / 2 >= CELDAS_SEGMENTO) continue;
            bool pendiente = false;
            if (a.g->tile_translucido(t.tile, pendiente)) s->sombra[t.y] |= 1ull << (t.x / 2);
            falta |= pendiente;
        }
        if (falta) {  // algún .til todavía no está en memoria: se vuelve a calcular después
            s->sombra.clear();
            return false;
        }
    }
    return (s->sombra[y - s->base_y] >> (x - s->base_x)) & 1;
}

void dibujar_suelo(App& a, double cam_x, double cam_y, double ox, double oy) {
    Graficos& g = *a.g;
    int cx = static_cast<int>(std::lround(cam_x)), cy = static_cast<int>(std::lround(cam_y));
    u64 k = clave_orden(CAPA_SUELO, 0);
    // Recorrido en coordenadas de pantalla: u = dx + dy (columnas de 24 px), v = dy - dx (filas
    // de 12 px), con margen para los rombos que entran a medias por los bordes.
    const Vista vi = vista_mundo(a);
    const int u_max = vi.w / 2 / 24 + 3, v_min = -(vi.cy / 12 + 4), v_max = (vi.h - vi.cy) / 12 + 4;
    for (int v = v_min; v <= v_max; ++v) {
        for (int u = -u_max; u <= u_max; ++u) {
            if ((u + v) & 1) continue;
            int x = cx + (u - v) / 2, y = cy + (u + v) / 2;
            const SegmentoMapa* s = segmento_cargado(a.mundo, a.rec, x, y);
            if (!s) continue;
            int lx = x - s->base_x, ly = y - s->base_y;
            float sx = static_cast<float>(mundo_px(x, y) + ox);
            float sy = static_cast<float>(mundo_py(x, y) + oy);
            u32 izq = s->suelo_en(lx, ly, 0), der = s->suelo_en(lx, ly, 1);
            g.gpu().dibujar_en(g.medio_tile(izq, false), sx, sy, k);
            g.gpu().dibujar_en(g.medio_tile(der, true), sx + 24, sy, k);
        }
    }
}

// Radio en celdas que cubre la pantalla (más los objetos altos que asoman desde abajo). Se dibujan
// los segmentos de 64×64 alrededor de la cámara que lo contienen, y se conservan en memoria dos
// más por lado (se cargan con margen y no se liberan y recargan yendo y viniendo).
int radio_dibujo(const Vista& vi) { return (vi.w / 24 + vi.h / 12) / 2 + 8; }
int radio_segmentos(const Vista& vi) { return radio_dibujo(vi) / 64 + 3; }

// Objetos del mapa que tapan al propio personaje (el techo de la casa en la que entró, el árbol
// detrás del que pasa): si una pieza que se levanta está delante de él y cubre el centro de su
// cuerpo, todo lo que ese objeto tiene delante del personaje se dibuja translúcido. Se usa el
// rectángulo del personaje del frame anterior (dibujar_personajes llena en_pantalla después).
constexpr u8 ALFA_TAPA = 110;
constexpr int CELDAS_TAPA = 6;  // una pieza que cubre al personaje está a pocas celdas
constexpr u8 ALFA_SILUETA = 120;
// Ceguera (S_CurseBlind): un velo negro sobre el mundo, nombres y efectos incluidos, y el propio
// personaje encima. Con Floating Eye se ve algo a través.
constexpr u32 PROF_CEGUERA = 0xF00000;

// Mezcla de un atributo de sprite / tile. PROMEDIO (50%) y OCULTO los maneja quien llama (alfa / saltar).
Mezcla mezcla_de(ModoMezcla m) {
    switch (m) {
    case ModoMezcla::PANTALLA: return Mezcla::PANTALLA;
    case ModoMezcla::MULTIPLICAR: return Mezcla::MULTIPLICAR;
    case ModoMezcla::ADITIVA: return Mezcla::ADITIVA;
    default: return Mezcla::NORMAL;
    }
}

// Los tiles de luz (llamas, brillos) van en screen sobre el fondo, las manchas multiplican (byte de tipo del tile).
Mezcla mezcla_tile(Graficos& g, u32 tile) {
    bool pendiente = false;
    return mezcla_de(g.tile_modo(tile, pendiente));
}

// Lo mismo para sprites: atributo 104 del gfx. El promedio del 50% reduce a la mitad el alfa del tinte; oculto devuelve false.
bool preparar_mezcla(const TablaSprites& t, GfxId gfx, u32& color, Mezcla& mezcla) {
    const ModoMezcla m = t.modo(gfx);
    if (m == ModoMezcla::OCULTO) return false;
    mezcla = mezcla_de(m);
    if (m == ModoMezcla::PROMEDIO) color = (color & 0x00FFFFFFu) | (((color >> 24) / 2) << 24);
    return true;
}

// Desvanecido de techos del cliente original (0xc00 / FUN_00468240): los grupos registrados en la celda del jugador local van hacia
// 50% (tipo 0, nivel 16 de 0..31) u ocultos (tipo 1, nivel 31); los demás vuelven a 0. El nivel avanza de a 4 cada 50 ms.
void actualizar_techos(App& a, int cx, int cy, u64 ahora) {
    const Slot sy = a.mundo.objetos.buscar(a.mundo.jugador.id);
    if (sy != SIN_SLOT) {
        cx = a.mundo.objetos.col<O_X>()[sy];
        cy = a.mundo.objetos.col<O_Y>()[sy];
    }
    for (u32 i = 0; i < a.n_techos_fade; ++i) a.techos_fade[i].activo = false;
    if (const SegmentoMapa* s = segmento_cargado(a.mundo, a.rec, cx, cy)) {
        const u32 clave = static_cast<u32>(s->base_x) << 16 | s->base_y;
        for (const TechoMapa& t : techos_en(*s, cx, cy)) {
            u32 i = 0;
            while (i < a.n_techos_fade && !(a.techos_fade[i].segmento == clave && a.techos_fade[i].gid == t.gid)) ++i;
            if (i == a.n_techos_fade) {
                if (i == App::MAX_TECHOS_FADE) continue;
                a.techos_fade[a.n_techos_fade++] = {clave, t.gid, t.tipo, 0, true};
            }
            a.techos_fade[i].activo = true;
            a.techos_fade[i].tipo = t.tipo;
        }
    }
    if (a.techos_proximo_ms == 0 || ahora - a.techos_proximo_ms > 1000) a.techos_proximo_ms = ahora;
    for (int pasos = 0; ahora >= a.techos_proximo_ms && pasos < 20; ++pasos, a.techos_proximo_ms += 50) {
        for (u32 i = 0; i < a.n_techos_fade; ++i) {
            App::TechoFade& f = a.techos_fade[i];
            const int meta = !f.activo ? 0 : f.tipo ? 31 : 16;
            f.nivel = static_cast<u8>(f.nivel < meta ? std::min(meta, f.nivel + 4) : std::max(meta, f.nivel - 4));
        }
    }
    for (u32 i = 0; i < a.n_techos_fade;)
        if (!a.techos_fade[i].activo && a.techos_fade[i].nivel == 0) a.techos_fade[i] = a.techos_fade[--a.n_techos_fade];
        else ++i;
}

void dibujar_objetos_mapa(App& a, double cam_x, double cam_y, double ox, double oy) {
    Graficos& g = *a.g;
    int cx = static_cast<int>(cam_x), cy = static_cast<int>(cam_y);
    const Vista vi = vista_mundo(a);
    const App::ObjetoEnPantalla* yo = nullptr;
    a.jugador_tapado = false;
    const bool original = !a.cfg.ui_simple;
    if (original) actualizar_techos(a, cx, cy, a.entrada.ahora_ms);
    for (const App::ObjetoEnPantalla& r : a.en_pantalla)
        if (r.id == a.mundo.jugador.id) yo = &r;
    // Centro del cuerpo: el rectángulo del frame trae márgenes (arma, capa).
    float cuerpo_x0 = 0, cuerpo_x1 = 0, cuerpo_y0 = 0, cuerpo_y1 = 0;
    if (yo) {
        const float medio = (yo->x0 + yo->x1) / 2, alto = yo->y1 - yo->y0;
        cuerpo_x0 = medio - 8;
        cuerpo_x1 = medio + 8;
        cuerpo_y0 = yo->y0 + alto * 0.25f;
        cuerpo_y1 = yo->y1 - alto * 0.15f;
    }
    const int radio = radio_dibujo(vi), segs = radio / 64 + 1;
    for (int dy = -segs; dy <= segs; ++dy) {
        for (int dx = -segs; dx <= segs; ++dx) {
            const SegmentoMapa* s = segmento_cargado(a.mundo, a.rec, cx + dx * 64, cy + dy * 64);
            if (!s) continue;
            for (const ObjetoMapa& obj : s->objetos) {
                // Objeto entero fuera del radio: ni se miran sus piezas.
                const int ox0 = s->base_x + obj.x0, ox1 = s->base_x + obj.x1, oy0 = s->base_y + obj.y0, oy1 = s->base_y + obj.y1;
                if (ox1 < cx - radio || ox0 > cx + radio || oy1 < cy - radio || oy0 > cy + radio) continue;
                const TileObjetoMapa* piezas = s->tiles_objetos.data() + obj.primera;
                // Desvanecido del grupo de techo: alfa del objeto entero (interfaz original), 0 = oculto.
                u8 alfa_techo = 255;
                if (a.n_techos_fade && obj.gid != 0xFFFF) {
                    const u32 clave = static_cast<u32>(s->base_x) << 16 | s->base_y;
                    for (u32 i = 0; i < a.n_techos_fade; ++i) {
                        const App::TechoFade& f = a.techos_fade[i];
                        if (f.segmento == clave && f.gid == obj.gid) alfa_techo = f.nivel >= 31 ? 0 : static_cast<u8>(255 * (32 - f.nivel) / 32);
                    }
                }
                // ¿Tapa al personaje? Solo si alguna de sus piezas está a pocas celdas.
                bool tapa = false;
                const bool cerca = yo && !original && ox1 >= cx - CELDAS_TAPA && ox0 <= cx + CELDAS_TAPA && oy1 >= cy - CELDAS_TAPA &&
                                   oy0 <= cy + CELDAS_TAPA;
                for (u32 k = 0; cerca && k < obj.cantidad && !tapa; ++k) {
                    const TileObjetoMapa& t = piezas[k];
                    int x = s->base_x + t.x / 2, y = s->base_y + t.y;
                    if (pieza_de_suelo(t) || std::abs(x - cx) > CELDAS_TAPA || std::abs(y - cy) > CELDAS_TAPA) continue;
                    const RegionAtlas& r = g.medio_tile(t.tile, (t.x & 1) != 0);
                    if (!r.valida()) continue;
                    float x0 = static_cast<float>(mundo_px(x, y) + ox) + ((t.x & 1) ? 24 : 0) + r.ox;
                    float y0 = static_cast<float>(mundo_py(x, y) + oy) + r.oy;
                    if (x0 >= cuerpo_x1 || x0 + r.w <= cuerpo_x0 || y0 >= cuerpo_y1 || y0 + r.h <= cuerpo_y0) continue;
                    float fila_pie = static_cast<float>(mundo_py(x, y) + t.base * 12.0) + 12.0f;
                    tapa = profundidad(fila_pie, ORDEN_PERSONAJE - 1 - std::min(62, static_cast<int>(t.base)), t.capa) > yo->profundidad;
                }
                a.jugador_tapado |= tapa;
                for (u32 k = 0; k < obj.cantidad; ++k) {
                    const TileObjetoMapa& t = piezas[k];
                    int x = s->base_x + t.x / 2, y = s->base_y + t.y;
                    if (std::abs(x - cx) > radio || std::abs(y - cy) > radio) continue;
                    float sx = static_cast<float>(mundo_px(x, y) + ox) + ((t.x & 1) ? 24 : 0);
                    float sy = static_cast<float>(mundo_py(x, y) + oy);
                    if (sx < -96 || sx > vi.w + 60 || sy < -96 || sy > vi.h + 80) continue;
                    if (pieza_de_suelo(t)) {
                        // Pegada al suelo (sombra, arena): sobre las baldosas, por capa y fila, y debajo
                        // de todo lo que se levanta (también las sombras: ver en_sombra).
                        i32 fila = static_cast<i32>(mundo_py(x, y)) + (1 << 20);
                        u32 orden = (static_cast<u32>(t.capa + 1) << 21) | (static_cast<u32>(fila < 0 ? 0 : fila) & 0x1FFFFF);
                        g.gpu().dibujar_en(g.medio_tile(t.tile, (t.x & 1) != 0), sx, sy, clave_orden(CAPA_SUELO, orden), BLANCO,
                                           mezcla_tile(g, t.tile));
                        continue;
                    }
                    // Pie de la pieza (ver COLUMNAS_BASE en mapa.h); dentro de la misma fila de pie, las
                    // piezas más altas primero y los personajes después.
                    float fila_pie = static_cast<float>(mundo_py(x, y) + t.base * 12.0) + 12.0f;
                    const u32 prof = original ? clave_celda(sx, sy, t.capa, 12)
                                              : profundidad(fila_pie, ORDEN_PERSONAJE - 1 - std::min(62, static_cast<int>(t.base)), t.capa);
                    // Del objeto que tapa, solo lo que queda delante (techo, pared del frente): el
                    // interior y la pared de atrás siguen opacos.
                    const bool delante = tapa && prof > yo->profundidad;
                    if (alfa_techo == 0) continue;
                    g.gpu().dibujar_en(g.medio_tile(t.tile, (t.x & 1) != 0), sx, sy, clave_orden(CAPA_MUNDO, prof),
                                       delante ? rgba(255, 255, 255, ALFA_TAPA) : alfa_techo != 255 ? rgba(255, 255, 255, alfa_techo) : BLANCO,
                                       mezcla_tile(g, t.tile));
                }
            }
        }
    }
}

// Título centrado en x, con sus códigos de color (\fX).
void dibujar_titulo(Graficos& g, std::string_view t, float x, float y, u64 clave, u32 normal, bool contorno = false) {
    const bool doble_byte = !es_utf8(t.data(), t.size());
    int ancho = 0;
    recorrer_codigos(t, normal, doble_byte, [&](std::string_view s, u32) { ancho += ancho_texto(s.data(), s.size()); });
    float cx = x - ancho / 2;
    recorrer_codigos(t, normal, doble_byte, [&](std::string_view s, u32 c) {
        cx += contorno ? g.texto_contorno(s, cx, y, clave, c) : g.texto_sombra(s, cx, y, clave, c);
    });
}

// Lo último que dijo el objeto, sobre su nombre (hasta 3 filas centradas). Se quita el
// "Nombre: " / "<Nombre> " que agrega el servidor porque el nombre ya está debajo.
// Private shop message (0x4ce3b0): at most 80 bytes; byte 0xff ends a line and starts the next, line 0 is cut at 40 bytes
// without one, and only two lines are kept.
void lineas_cartel_tienda(std::string_view t, std::string_view (&lineas)[2]) {
    t = t.substr(0, std::min<size_t>(t.size(), 80));
    const size_t ff = t.find('\xff');
    const size_t fin0 = std::min(ff, std::min<size_t>(t.size(), 40));
    lineas[0] = t.substr(0, fin0);
    std::string_view resto = t.substr(fin0 == ff ? fin0 + 1 : fin0);
    lineas[1] = resto.substr(0, std::min(resto.find('\xff'), std::min<size_t>(resto.size(), 40)));
}

// Filtro del texto de globos del original (0x41a9f0, lo llama una vez 0x41a600): \f y \b con '0'..'d' y \i con '0'..'5' se
// quitan (3 bytes); cualquier otra barra invertida se copia tal cual, así "\\f0f2..." conserva "\f2..." para el dibujo que entiende los códigos.
std::string_view filtrar_globo(std::string_view t, char* buf, size_t cap) {
    size_t n = 0;
    for (size_t i = 0; i < t.size() && n < cap; ++i) {
        if (t[i] == '\\' && i + 2 < t.size()) {
            const char c = t[i + 1], x = t[i + 2];
            if (((c == 'f' || c == 'b') && es_codigo_color(x)) || (c == 'i' && x >= '0' && x <= '5')) {
                i += 2;
                continue;
            }
        }
        buf[n++] = t[i];
    }
    return {buf, n};
}

void dibujar_globo(App& a, std::string_view t, float centro_x, float base_y, bool original = false, GloboChat* gl = nullptr, float foot_y = 0,
                   bool quitar_nombre = true) {
    if (t.empty()) return;
    char filtrado[256];
    if (original) t = filtrar_globo(t, filtrado, sizeof filtrado);
    size_t corte = std::min(t.find(": "), t.find("> "));
    if (quitar_nombre && corte != std::string_view::npos && corte < 40) t.remove_prefix(corte + 2);
    // Cliente original (S_Say -> 0x41a600): sin recuadro 20 caracteres (de 6 px) por línea, con recuadro 16; paso de 12 px.
    const bool caja = a.cfg.globos_comic || (a.cfg.opciones_orig >> OPC_BUBBLE & 1);
    const int ANCHO = original ? (caja ? 96 : 120) : 160;
    constexpr int FILAS = 3;
    std::string_view filas[FILAS];
    int n = 0;
    while (!t.empty() && n < FILAS) {
        size_t c = bytes_que_caben(t.data(), t.size(), ANCHO);
        filas[n++] = t.substr(0, c);
        t.remove_prefix(c);
        while (!t.empty() && t[0] == ' ') t.remove_prefix(1);
    }
    // Rectángulo original (S3EP1U 0x419e70 / 0x41a040, docs/fork/RE_ROUND7.md sección 1): w = línea ajustada más ancha en celdas de 6 px,
    // L líneas; ancla (ax, ay) = (x en pantalla + 24, tope del cuadro del sprite - 2); el borde inferior en el ancla.
    int rx0 = 0, ry0 = 0, desp = 0, volteado = 0;
    if (original) {
        int cells = 0;
        for (int i = 0; i < n; ++i) cells = std::max(cells, (ancho_texto(filas[i].data(), filas[i].size()) + 5) / 6);
        const int ax = static_cast<int>(centro_x), ay = static_cast<int>(base_y);
        const int half = 3 * cells + (caja ? 5 : 0);
        int left = ax - half, right = ax + half, top = ay - 12 * n - (caja ? 14 : 6), bottom = ay;
        int desp_x = half;  // [+0x394]: dónde queda el puntero dentro del rect (sigue al ancla a través de los límites)
        if (left < 0x30) {  // límites horizontales de 0x419e70 (corrimiento de 2 - left, como en el desensamblado)
            const int d = 2 - left;
            left += d, right += d;
            desp_x -= d;
            if (desp_x < 8) return;  // puntero demasiado cerca del borde izquierdo: se descarta
        } else if (right > 0x2b0) {
            const int d = right - 0x2b0;
            desp_x += d;
            if (desp_x > (right - left) - 9) return;
            left -= d, right -= d;
        }
        if (top < 0x30) {  // empujado bajo el sprite (0x419f5e): top = el borde inferior del cuadro (cuadro + 0x1c), flag +0x36a = 4
            const int h = bottom - top;
            top = static_cast<int>(foot_y), bottom = top + h;
            volteado = 1;
        }
        desp = desp_x;
        if (top < 0x30 || bottom >= 0x1c9) return;  // dropped, never shown
        if (gl) {
            gl->x0 = left, gl->x1 = right, gl->y0 = top, gl->y1 = bottom;
            if (!gl->revisado) {  // globo nuevo: los globos anteriores cuyo rect se superpone estrictamente quedan grises para siempre
                gl->revisado = true;
                for (GloboChat& o : a.chat.globos)
                    if (&o != gl && o.id != 0 && o.con_rect && o.inicio < gl->inicio && o.x0 < gl->x1 && o.y0 < gl->y1 &&
                        gl->x0 < o.x1 && gl->y0 < o.y1)
                        o.gris = true;
            }
            gl->con_rect = true;
        }
        centro_x = static_cast<float>(left + right) / 2;
        rx0 = left, ry0 = top;
        // Inicio del texto 0x419d3e..: x = left (+ 5 con recuadro), y = top (+ 5 con recuadro); el puntero lo baja 6 (4 con recuadro) si está invertido.
        base_y = static_cast<float>(top + 12 * n);  // las filas de texto empiezan en el tope del rect (+5 con recuadro)
    }
    const int paso = original ? 12 : 14;
    if (caja) {  // option bubbleon (0x41a6c7): balloon frame; sin relleno: contorno redondeado y cola; el texto queda con sombra
        int ancho = 0;
        for (int i = 0; i < n; ++i) ancho = std::max(ancho, ancho_texto(filas[i].data(), filas[i].size()));
        const int pad = original ? 5 : 6;
        const int x0 = original ? rx0 : static_cast<int>(centro_x) - ancho / 2 - pad;
        const int w = original ? gl ? gl->x1 - gl->x0 : ancho + 2 * pad : ancho + 2 * pad;
        const int y0 = original ? ry0 : static_cast<int>(base_y) - 14 * n - 6;
        const int h = original ? 12 * n + 10 : 14 * n + 8;
        const int x1 = x0 + w - 1, y1 = y0 + h - 1, cx = static_cast<int>(centro_x);
        const u64 k = clave_orden(CAPA_EFECTOS, 1);
        const u32 borde = rgba(235, 235, 230, 230);
        Gpu& gpu = a.g->gpu();
        gpu.rectangulo(x0 + 3, y0, w - 6, 1, k, borde);  // lados
        gpu.rectangulo(x0 + 3, y1, w - 6, 1, k, borde);
        gpu.rectangulo(x0, y0 + 3, 1, h - 6, k, borde);
        gpu.rectangulo(x1, y0 + 3, 1, h - 6, k, borde);
        auto punto = [&](int x, int y) { gpu.rectangulo(x, y, 1, 1, k, borde); };
        for (int d = 0; d < 3; ++d) {  // esquinas redondeadas
            punto(x0 + 2 - d, y0 + d), punto(x1 - 2 + d, y0 + d);
            punto(x0 + 2 - d, y1 - d), punto(x1 - 2 + d, y1 - d);
        }
        punto(cx - 2, y1 + 1), punto(cx + 2, y1 + 1), punto(cx - 1, y1 + 2), punto(cx + 1, y1 + 2), punto(cx, y1 + 3);
    }
    const u32 color_txt = gl && gl->gris ? rgba(148, 148, 148) : rgba(255, 255, 254);
    if (original) {
        // 0x4334f0 por línea: todas las líneas empiezan en la misma x (alineadas a la izquierda en el rect), paso de 12 px, contorno negro de 1 px, el color del globo.
        const float tx = static_cast<float>(rx0 + (caja ? 5 : 0));
        const float ty = static_cast<float>(ry0 + (caja ? 5 : 0) + (volteado ? (caja ? 4 : 6) : 0));
        // Entiende los códigos como 0x4334f0: un \fX que sobrevivió al filtro colorea el resto del globo.
        const bool doble = !es_utf8(t.data(), t.size());
        u32 color_fila = color_txt;
        for (int i = 0; i < n; ++i) {
            float x = tx;
            recorrer_codigos(filas[i], color_fila, doble, [&](std::string_view tramo, u32 c) {
                x += a.g->texto_contorno(tramo, x, ty + 12.0f * static_cast<float>(i), clave_orden(CAPA_EFECTOS, 2), c);
                color_fila = c;
            });
        }
        if (!caja) {
            // Puntero del habla (0x419aeb / 0x419b7f): un triángulo negro de 9 x 5 con el de 7 x 4 del color del globo encima, bajo el texto
            // (apuntando abajo), o arriba de él (apuntando arriba) cuando el globo se empujó bajo el sprite. 1 bit por píxel, MSB primero.
            const int px = rx0 + desp - 3, h = 12 * n + 6;
            static constexpr u8 FUERA_ABAJO[5][2] = {{0xff, 0x80}, {0x7f, 0x00}, {0x3e, 0x00}, {0x1c, 0x00}, {0x08, 0x00}};
            static constexpr u8 DENTRO_ABAJO[4] = {0xfe, 0x7c, 0x38, 0x10};
            const u64 kp = clave_orden(CAPA_EFECTOS, 1);
            Gpu& gpu = a.g->gpu();
            const int py = ry0;  // tope del rect; el puntero va abajo (normal) o arriba (invertido)
            for (int f = 0; f < 5; ++f)
                for (int c = 0; c < 9; ++c) {
                    const int fila = volteado ? 4 - f : f;
                    if (FUERA_ABAJO[fila][c / 8] >> (7 - c % 8) & 1)
                        gpu.rectangulo(px + c, (volteado ? py : py + h - 5) + f, 1, 1, kp, rgba(0, 0, 0));
                }
            for (int f = 0; f < 4; ++f)
                for (int c = 0; c < 7; ++c)
                    if (DENTRO_ABAJO[volteado ? 3 - f : f] >> (7 - c) & 1)
                        gpu.rectangulo(px + c, (volteado ? py + 1 : py + h - 4) + f, 1, 1, clave_orden(CAPA_EFECTOS, 2), color_txt);
        }
        return;
    }
    for (int i = 0; i < n; ++i) {
        int w = ancho_texto(filas[i].data(), filas[i].size());
        a.g->texto_sombra(filas[i], centro_x - w / 2, base_y - paso * (n - i), clave_orden(CAPA_EFECTOS, 2), color_txt);
    }
}

// Un emblema de clan (16 x 12) en (x, y); la primera vez que se ve un clan se le pide al servidor (C_Clan)
// salvo que la caché de emblemas (Emblem.dat) ya lo tenga.
void dibujar_emblema(App& a, u32 clan, float x, float y) {
    Graficos& g = *a.g;
    const RegionAtlas& em = g.emblema(clan);
    if (em.valida()) {
        g.gpu().dibujar_en(em, x, y, clave_orden(CAPA_EFECTOS, 0));
    } else if (std::find(a.emblemas_pedidos.begin(), a.emblemas_pedidos.end(), clan) == a.emblemas_pedidos.end()) {
        a.emblemas_pedidos.push_back(clan);
        enviar(a, cmd::pedir_emblema(a.op, clan));
    }
}

// True cuando el inventario tiene "Ring of Sayha" ($431): el exe deja que quien lo usa vea los cuerpos invisibles de otros jugadores
// al 50% (flag 0x40, FUN_004cc980). La condición del exe: índice 3 de S_Ability en 1..3, ningún teleport pendiente (el destino
// elegido se mandó y el teleport todavía no ocurrió) y el anillo en el inventario. l1j-en nunca manda el
// índice de habilidad 3, así que ahí queda apagado, como en el exe.
bool tiene_anillo_sayha(const Inventario& inv) {
    for (Slot s = 0; s < inv.items.tamano(); ++s) {
        const std::string_view n = nombre_item(inv, s);
        for (std::string_view clave : {std::string_view("$431"), std::string_view("Ring of Sayha")})
            if (n.starts_with(clave) && (n.size() == clave.size() || n[clave.size()] == ' ' || n[clave.size()] == '('))
                return true;
    }
    return false;
}

// Pulso de veneno del exe (flag 0x20): por canal out = s + ((L * c) >> 4), c = (0, 4, 0) / 31 y
// L = |12 - tick % 24| + 16 con un tick de 50 ms. Se aproxima con una silueta verde plana aditiva sobre el sprite
// (el exe suma a los píxeles propios del sprite; la silueta suma lo mismo a cada píxel opaco).
u32 pulso_veneno(u64 ahora) {
    const int tick = static_cast<int>((ahora / 50) % 24);
    const int nivel = std::abs(12 - tick) + 16;
    return rgba(0, static_cast<u8>(std::min(255, nivel * 4 * 255 / (16 * 31))), 0, 255);
}

void dibujar_personajes(App& a, u64 ahora, double ox, double oy) {
    Graficos& g = *a.g;
    TablaObjetos& o = a.mundo.objetos;
    const u8 habilidad3 = a.mundo.jugador.habilidad[3];
    const bool ve_invisibles = habilidad3 >= 1 && habilidad3 <= 3 && !a.destino.enviado && tiene_anillo_sayha(a.mundo.inventario);
    i32 yo_x = 0, yo_y = 0;
    {
        const Slot sy_ = o.buscar(a.mundo.jugador.id);
        if (sy_ != SIN_SLOT) {
            yo_x = o.col<O_X>()[sy_];
            yo_y = o.col<O_Y>()[sy_];
        }
    }
    const Vista vi = vista_mundo(a);
    const float mx = raton_mundo_x(a), my = raton_mundo_y(a);
    a.en_pantalla.clear();
    a.luces.clear();
    for (Slot s = 0; s < o.tamano(); ++s) {
        // Posición continua (orden de dibujo, nombre, globo) y celda desde la que se dibuja el sprite:
        // la de origen del paso, porque los frames de caminar ya traen el avance (ver mundo.h).
        double fx, fy;
        posicion_interpolada(a.mundo, s, ahora, fx, fy);
        float sx = static_cast<float>(mundo_px(fx, fy) + ox), sy = static_cast<float>(mundo_py(fx, fy) + oy);
        const bool propio = o.ids()[s] == a.mundo.jugador.id;
        const int radio_luz = radio_de_luz(propio, o.col<O_LUZ>()[s]);
        auto fuera = [&](float mx, float my) { return sx < -mx || sx > vi.w + mx || sy < -my || sy > vi.h + my; };
        // Una luz alcanza más lejos que el sprite: hasta ±312×±156 px (radio 14), desde los pies.
        if (radio_luz > 0 ? fuera(360, 200) : fuera(150, 150)) continue;
        // El sprite se dibuja en (spr) y el frame suma el avance hasta (vis), donde se ve el cuerpo.
        double vis_x, vis_y;
        posicion_visible(a.mundo, s, ahora, vis_x, vis_y);
        i32 cx0, cy0;
        origen_paso(a.mundo, s, ahora, cx0, cy0);
        double spr_x = cx0, spr_y = cy0;
        if (o.ids()[s] == a.mundo.jugador.id && !a.cfg.camara_a_saltos) {
            // Cámara suave: el propio personaje se corre para que el cuerpo quede en la posición
            // continua, es decir, siempre centrado.
            spr_x += fx - vis_x;
            spr_y += fy - vis_y;
            vis_x = fx;
            vis_y = fy;
        }
        float px = static_cast<float>(mundo_px(spr_x, spr_y) + ox), py = static_cast<float>(mundo_py(spr_x, spr_y) + oy);
        // Diferencia vertical entre el cuerpo visible y la posición continua (para el nombre).
        float desfase_y = static_cast<float>(mundo_py(vis_x, vis_y) + oy) - sy;
        // Ancla de nombre y barras. Con la cámara a saltos (como el original) van pegados al cuerpo
        // visible, que avanza con los frames de caminar; si no, siguen la posición continua.
        float ex = sx;
        if (a.cfg.camara_a_saltos) {
            ex = static_cast<float>(mundo_px(vis_x, vis_y) + ox);
            desfase_y = 0;
        }
        // Los pies: el centro del rombo de la celda del cuerpo (el ancla del sprite es su esquina + (24, 12)).
        if (radio_luz > 0) {
            a.luces.push_back({radio_luz, static_cast<float>(mundo_px(vis_x, vis_y) + ox) + 24,
                               static_cast<float>(mundo_py(vis_x, vis_y) + oy) + 12});
            if (propio) std::swap(a.luces.front(), a.luces.back());  // el corte de 700 no la apaga
        }
        if (fuera(150, 150)) continue;  // alumbra, pero no se dibuja
        // Los cuerpos invisibles de otros no se dibujan para nada (sin nombre, barras ni objetivo de clic) sin un Ring of Sayha.
        if ((o.col<O_FLAGS>()[s] & FLAG_INVISIBLE) && !propio && !ve_invisibles) continue;
        u32 prof = profundidad(static_cast<float>(mundo_py(fx, fy)) + 12.0f, ORDEN_PERSONAJE, 4);
        float celda_sx = 0, celda_sy = 0;  // interfaz original: esquina en pantalla de la celda del cuerpo
        if (!a.cfg.ui_simple) {
            const int cx = static_cast<int>(std::lround(fx)), cy = static_cast<int>(std::lround(fy));
            celda_sx = static_cast<float>(mundo_px(cx, cy) + ox), celda_sy = static_cast<float>(mundo_py(cx, cy) + oy);
            prof = clave_celda(celda_sx, celda_sy, CAPA_CUERPO_ORIGINAL, 4);
        }
        // Capas del cuerpo: el original registra cada parte del .spr en su propia celda (0x4b0350: la parte de los pies en la celda del cuerpo en
        // la capa 18, las partes de arriba una fila más atrás cada una, +4 de capa por fila), así una pared en una fila posterior también tapa
        // la cabeza. El fork dibuja el cuadro en bandas de 12 px, una por fila de celdas sobre los pies, con esas claves.
        auto dibujar_cuerpo = [&](const RegionAtlas& r, GfxId archivo, u16 sub_p, u16 frame_p, bool partes, float x, float y, u32 extra,
                                  u32 col, Mezcla mz) {
            if (a.cfg.ui_simple) return g.gpu().dibujar_en(r, x, y, clave_orden(CAPA_MUNDO, prof + extra), col, mz);
            // Las partes del .spr en sí (0x4b0350): el rectángulo de cada parte del cuadro compuesto, con la clave de su propia celda
            // (x en medias celdas, y en celdas desde la celda del cuerpo) y su propio byte de capa.
            bool pend = false;
            const ArchivoSpr* sp = partes ? g.archivo_spr(archivo, sub_p, pend) : nullptr;
            if (sp && frame_p < sp->frames.size() && sp->frames[frame_p].ntiles) {
                const FrameSpr& fr = sp->frames[frame_p];
                const int sc = static_cast<int>(r.escala_texel());
                for (u32 j = 0; j < fr.ntiles; ++j) {
                    const TileDeFrame& t = sp->tiles_frame[fr.primer_tile + j];
                    if (t.tile >= sp->tiles.size() || sp->tiles[t.tile].size() < 4) continue;
                    const Bytes d = sp->tiles[t.tile];
                    int cx_off, cy_off;
                    posicion_tile(t, cx_off, cy_off);
                    const int rx = cx_off + d[0] - r.ox, ry = cy_off + d[1] - r.oy;  // en la imagen compuesta
                    const int x0 = std::max(0, rx), y0 = std::max(0, ry);
                    const int x1 = std::min(static_cast<int>(r.w), rx + d[2]), y1 = std::min(static_cast<int>(r.h), ry + d[3]);
                    if (x1 <= x0 || y1 <= y0) continue;
                    RegionAtlas parte = r;
                    parte.x = static_cast<u16>(r.x + x0 * sc);
                    parte.y = static_cast<u16>(r.y + y0 * sc);
                    parte.w = static_cast<u16>(x1 - x0);
                    parte.h = static_cast<u16>(y1 - y0);
                    parte.ox = static_cast<i16>(r.ox + x0);
                    parte.oy = static_cast<i16>(r.oy + y0);
                    const u32 clave = clave_celda(celda_sx + static_cast<float>(cx_off), celda_sy + static_cast<float>(cy_off), t.h, 4 + extra);
                    g.gpu().dibujar_en(parte, x, y, clave_orden(CAPA_MUNDO, clave), col, mz);
                }
                return;
            }
            // Bloques de 24 x 12 sobre la grilla de celdas (un bloque por media celda, el tamaño de la celda de una parte del .spr).
            const float t = y + r.oy, b = t + r.h, l = x + r.ox, d = l + r.w, limite = celda_sy + 12.0f;
            const int sc = static_cast<int>(r.escala_texel());
            const int j0 = static_cast<int>(std::floor((l - celda_sx) / 24.0f)), j1 = static_cast<int>(std::ceil((d - celda_sx) / 24.0f));
            for (int k = 0; k < 16; ++k) {
                const float abajo = k == 0 ? b : limite - 12.0f * static_cast<float>(k - 1);
                const float arriba = std::max(t, limite - 12.0f * static_cast<float>(k));
                if (abajo <= t) break;
                const int corte_arriba = static_cast<int>(std::lround(arriba - t)), corte_abajo = static_cast<int>(std::lround(b - std::min(abajo, b)));
                if (corte_arriba + corte_abajo >= r.h) continue;
                for (int j = j0; j < j1; ++j) {
                    const float izq = std::max(l, celda_sx + 24.0f * static_cast<float>(j)), der = std::min(d, celda_sx + 24.0f * static_cast<float>(j + 1));
                    const int corte_izq = static_cast<int>(std::lround(izq - l)), corte_der = static_cast<int>(std::lround(d - der));
                    if (corte_izq + corte_der >= r.w) continue;
                    RegionAtlas bloque = r;
                    bloque.x = static_cast<u16>(r.x + corte_izq * sc);
                    bloque.w = static_cast<u16>(r.w - corte_izq - corte_der);
                    bloque.ox = static_cast<i16>(r.ox + corte_izq);
                    bloque.y = static_cast<u16>(r.y + corte_arriba * sc);
                    bloque.h = static_cast<u16>(r.h - corte_arriba - corte_abajo);
                    bloque.oy = static_cast<i16>(r.oy + corte_arriba);
                    const u32 clave = clave_celda(celda_sx + 24.0f * static_cast<float>(j), celda_sy - 12.0f * static_cast<float>(k),
                                                  CAPA_CUERPO_ORIGINAL + 4u * static_cast<u32>(k), 4 + extra);
                    g.gpu().dibujar_en(bloque, x, y, clave_orden(CAPA_MUNDO, clave), col, mz);
                }
            }
        };
        GfxId gfx = o.col<O_GFX>()[s];
        const AccionVisible acc = elegir_accion(a, s, ahora);
        const RangoAccion* r = acc.rango;
        float top = sy - 40, pie = sy;  // pie: el borde inferior del cuadro del sprite (cuadro + 0x1c), donde empieza un globo empujado abajo
        float caja_x0 = ex, caja_x1 = ex + 48;  // rect del cuadro del sprite, izquierda / derecha (exe 0x4cbf70)
        RegionAtlas rg;
        bool rg_pendiente = false;
        bool frame_vacio = false;  // el .spr existe y este frame no tiene tiles: sin dibujo a propósito
        const FrameAccion* frame_cuerpo = nullptr;  // for the clothes layers (attribute 105)
        u16 sub_cuerpo = 0;
        // pendiente() compara la dirección: se consulta sobre la referencia, antes de copiarla a rg.
        u16 spr_sub = 0, spr_frame = 0;
        bool spr_partes = false;  // rg es exactamente ese cuadro (no el anterior que se conserva mientras carga)
        auto pedir_frame = [&](u16 sub_pedido, u16 frame) {
            const RegionAtlas& r = g.frame_sprite(a.sprites.gfx_archivos(gfx), sub_pedido, frame);
            rg_pendiente = g.pendiente(r);
            rg = r;
            spr_sub = sub_pedido, spr_frame = frame, spr_partes = !rg_pendiente && rg.valida();
            if (s >= a.last_frames.size()) return;  // more objects than tracked: no fallback
            App::LastFrame& last = a.last_frames[s];
            if (!rg_pendiente) {
                if (rg.valida()) last = {o.ids()[s], rg};
            } else if (last.id == o.ids()[s] && last.region.valida()) {
                rg = last.region;  // conserva el cuadro anterior hasta que se cargue el nuevo
                rg_pendiente = false;
            }
        };
        if (r) {
            bool termino = false;
            bool camina = !acc.puntual && acc.bucle && caminando(a.mundo, s, ahora);
            // Factores de largo de cuadro de 0x4cda90 (no al caminar, que sigue el tiempo del paso): haste, lentitud, brave, wafer,
            // tercer acelerador, Triple Arrow, el daño a la mitad de un golpe y la recuperación de una cola (ritmo_de).
            u64 transcurrido = ahora - acc.inicio;
            if (!camina) {
                const Ritmo rt = ritmo_de(a.mundo, s, acc.accion);
                transcurrido = transcurrido * rt.den / rt.num;
            }
            const FrameAccion* f =
                &a.sprites.frame_en(*r, transcurrido, termino, camina ? o.col<O_DURMOV>()[s] : 0, acc.bucle);
            if (termino && acc.puntual) o.col<O_ACCION>()[s] = 0xFF;  // fin de la acción puntual
            sonido_de_frame(a, o.col<O_SONIDO>()[s], *f, vis_x, vis_y, ahora, false, o.col<O_DIR>()[s]);
            u16 sub = static_cast<u16>(f->sub + (r->direccional ? o.col<O_DIR>()[s] : 0));
            frame_cuerpo = f;
            sub_cuerpo = sub;
            i16 sombra = a.sprites.sombra(gfx);
            if (sombra >= 0)
                g.gpu().dibujar_en(g.frame_sprite(static_cast<GfxId>(sombra), sub, f->frame), px, py,
                                   clave_orden(CAPA_MUNDO, prof - 1), rgba(255, 255, 255, 160));
            pedir_frame(sub, f->frame);
            // Un frame sin tiles (el topo bajo tierra, acción de caminar) no es un sprite que falta.
            if (!rg.valida() && !rg_pendiente) {
                char nombre[32];
                std::snprintf(nombre, sizeof nombre, "%u-%u.spr", static_cast<unsigned>(a.sprites.gfx_archivos(gfx)), static_cast<unsigned>(sub));
                frame_vacio = !a.rec.sprite(nombre).empty();
            }
        } else {
            // Sin acciones en la tabla (los ítems del suelo, por ejemplo): el primer frame de N-0.spr.
            // A gfx with no table row (newer monsters) still has 8 headings per sub group: group 0 by heading.
            const u16 sub_dir = TablaSprites::sub_sin_fila(o.col<O_DIR>()[s]);
            pedir_frame(sub_dir, 0);
            if (sub_dir != 0 && !rg.valida() && !rg_pendiente) pedir_frame(0, 0);  // objetos en el suelo: solo "-0"
        }
        bool bajo_puntero = false;
        if (rg.valida()) {
            // El ancla de los .spr es la esquina superior izquierda de la celda (calibrada con el
            // explorador: los pies quedan en el centro del rombo).
            u32 color = BLANCO;
            if (a.cfg.sombra_personajes && en_sombra(a, static_cast<int>(std::lround(vis_x)), static_cast<int>(std::lround(vis_y))))
                color = rgba(LUZ_EN_SOMBRA, LUZ_EN_SOMBRA, LUZ_EN_SOMBRA);
            // Congelado: gris azulado (S_Poison y los flags de S_CharPack). El veneno es una capa pulsante que se dibuja después del cuerpo.
            if (o.col<O_FLAGS>()[s] & FLAG_CONGELADO) color = rgba(150, 160, 185);
            // Invisible (S_Invis, flag 0x100 / 0x40): el cuerpo propio al 50%; los de otros solo con un Ring of Sayha, si no ocultos.
            const bool invisible = o.col<O_FLAGS>()[s] & FLAG_INVISIBLE;
            if (invisible) color = (color & 0x00FFFFFFu) | (127u << 24);
            // Atributo 100: el objeto pasa al 50% mientras el jugador local está en uno de sus rectángulos.
            if (!propio && a.sprites.tiene_transparencia(gfx) &&
                a.sprites.dentro_transparencia(gfx, yo_x - o.col<O_X>()[s], yo_y - o.col<O_Y>()[s]))
                color = (color & 0x00FFFFFFu) | (((color >> 24) / 2) << 24);
            // Attribute 104 (fire, braziers, spell bodies: screen; flag 0x2: hidden; 0x1: 50%...).
            {
                u32 color_cuerpo = color;
                Mezcla mezcla_cuerpo = Mezcla::NORMAL;
                if (preparar_mezcla(a.sprites, gfx, color_cuerpo, mezcla_cuerpo)) {
                    const GfxId archivo = a.sprites.gfx_archivos(gfx);
                    dibujar_cuerpo(rg, archivo, spr_sub, spr_frame, spr_partes, px, py, 0, color_cuerpo, mezcla_cuerpo);
                    if ((o.col<O_FLAGS>()[s] & FLAG_VENENO) && mezcla_cuerpo == Mezcla::NORMAL)
                        dibujar_cuerpo(rg, archivo, spr_sub, spr_frame, spr_partes, px, py, 4, (pulso_veneno(ahora) & 0x00FFFFFFu) | (color_cuerpo & 0xFF000000u),
                                       Mezcla::SILUETA_ADITIVA);
                }
            }
            // Ropa (atributo 105): después del cuerpo, mismo subsprite y cuadro, cada una con su propio atributo
            // 104. Como en el exe (0x4cbc50), si falta el archivo de alguna capa no se dibuja ninguna.
            if (const auto ropa = a.sprites.ropa(gfx); frame_cuerpo && !ropa.empty()) {
                RegionAtlas capas[3];  // subcapas 5-7 sobre el cuerpo (4)
                GfxId gfx_capa[3];
                size_t n = 0;
                bool todas = true;
                for (GfxId c : ropa) {
                    if (n == std::size(capas)) break;
                    const RegionAtlas& rc = g.frame_sprite(c, sub_cuerpo, frame_cuerpo->frame);
                    if (g.pendiente(rc)) continue;  // todavía se carga: se salta en este cuadro
                    if (!rc.valida()) {
                        char nombre[32];
                        std::snprintf(nombre, sizeof nombre, "%u-%u.spr", static_cast<unsigned>(c), static_cast<unsigned>(sub_cuerpo));
                        if (a.rec.sprite(nombre).empty()) todas = false;
                        continue;
                    }
                    gfx_capa[n] = c;
                    capas[n++] = rc;
                }
                if (todas)
                    for (size_t i = 0; i < n; ++i) {
                        u32 color_capa = color;
                        Mezcla mezcla_capa = Mezcla::NORMAL;
                        if (!preparar_mezcla(a.sprites, gfx_capa[i], color_capa, mezcla_capa)) continue;
                        dibujar_cuerpo(capas[i], gfx_capa[i], sub_cuerpo, frame_cuerpo->frame, true, px, py, 1 + static_cast<u32>(i), color_capa,
                                       mezcla_capa);
                    }
            }
            // Tapado por un objeto del mapa: también su silueta encima de todo (varias piezas
            // translúcidas superpuestas casi lo esconden).
            if (a.cfg.ui_simple && o.ids()[s] == a.mundo.jugador.id && a.mundo.jugador.ceguera)  // ciego: solo se ve a sí mismo
                g.gpu().dibujar_en(rg, px, py, clave_orden(CAPA_EFECTOS, PROF_CEGUERA + 1), color);
            else if (a.jugador_tapado && o.ids()[s] == a.mundo.jugador.id)
                g.gpu().dibujar_en(rg, px, py, clave_orden(CAPA_MUNDO, 0xFFFFFFFEu), rgba(255, 255, 255, ALFA_SILUETA));  // bajo la lluvia, sobre el resto
            top = py + rg.oy - desfase_y;
            caja_x0 = px + rg.ox;
            caja_x1 = caja_x0 + rg.w;
            pie = top + rg.h;
            const float x0 = px + rg.ox, y0 = py + rg.oy, x1 = x0 + rg.w, y1 = y0 + rg.h;
            bajo_puntero = mx >= x0 && mx < x1 && my >= y0 && my < y1;
            a.en_pantalla.push_back({o.ids()[s], x0, y0, x1, y1, prof, 0});  // también los cadáveres: el cursor muestra 10 sobre ellos
        } else {
            // Sin sprite: un bloque del tamaño de un cuerpo, que se puede elegir con el clic. Mientras
            // el sprite se carga (o el atlas no tiene lugar) es invisible; magenta si de verdad falta.
            const float x0 = static_cast<float>(mundo_px(vis_x, vis_y) + ox) + 12, y0 = static_cast<float>(mundo_py(vis_x, vis_y) + oy) - 22;
            const float x1 = x0 + 24, y1 = y0 + 36;
            // Mientras se carga (uno o dos frames) no se dibuja nada, como el original: el bloque verde
            // parpadeaba al aparecer cada sprite. Sigue siendo elegible con el clic. Magenta si falta.
            if (!rg_pendiente && !frame_vacio)
                g.gpu().rectangulo(static_cast<int>(x0), static_cast<int>(y0), 24, 36, clave_orden(CAPA_MUNDO, prof), rgba(255, 0, 255, 170));
            top = y0;
            caja_x0 = x0;
            caja_x1 = x1;
            pie = y0 + 36;
            bajo_puntero = mx >= x0 && mx < x1 && my >= y0 && my < y1;
            a.en_pantalla.push_back({o.ids()[s], x0, y0, x1, y1, prof, static_cast<u8>(rg_pendiente ? 1 : frame_vacio ? 0 : 2)});
        }
        // Con zoom solo del mapa, lo de encima de la cabeza (barras, nombre, título, globo, daño) va al tamaño
        // de la interfaz para que se lea: el ancla pasa a coordenadas de la interfaz y la GPU no lo escala.
        const float z = g.gpu().zoom_mundo();
        if (z != 1.0f) {
            sx = (sx + 24) * z - 24;
            ex = (ex + 24) * z - 24;
            caja_x0 *= z;
            caja_x1 *= z;
            sy *= z;
            top *= z;
            pie *= z;
            g.gpu().sin_zoom_mundo(true);
        }
        const float top_sprite = top;
        // Barra de vida si el servidor la da (S_HpMeter: grupo y mascotas; el % de S_CharPack es
        // 0xFF para los monstruos en L1J-JP) y nombre; el objetivo del ataque, en amarillo.
        const bool soy_yo = o.ids()[s] == a.mundo.jugador.id;
        const u8 hp = o.col<O_HP>()[s];
        // Un cadáver no tiene barra: el exe clasifica a un monstruo muerto como 2 (0x4cdd30), y solo la clase 0xa recibe la barra (0x467856).
        if (hp <= 100 && !soy_yo && o.col<O_ESTADO>()[s] != ESTADO_MUERTO) {
            if (a.cfg.ui_simple) {
                overhead_bar(g, static_cast<int>(ex) + 2, static_cast<int>(top) - 7, IMG_OVERHEAD_HP, hp, 100);
                top -= 8;
            } else {  // exe 0x466c43: centrada en el rect del cuadro (x = l + (r - l) / 2 - 21), y = top - 4; el nombre no se mueve
                const int izq = static_cast<int>(caja_x0), der = static_cast<int>(caja_x1);
                overhead_bar(g, izq + (der - izq) / 2 - 21, static_cast<int>(top) - 4, IMG_OVERHEAD_HP, hp, 100);
            }
        }
        // Barra de HP/MP propia (opción): dos barras finas sobre el personaje, con los valores reales.
        if (soy_yo && a.cfg.barra_personaje) {
            const EstadoJugador& j = a.mundo.jugador;
            // Como el original: HP arriba y MP justo debajo, compartiendo el borde del marco.
            if (a.cfg.ui_simple) {
                overhead_bar(g, static_cast<int>(ex) + 2, static_cast<int>(top) - 11, IMG_OVERHEAD_HP, j.hp, j.hp_max);
                overhead_bar(g, static_cast<int>(ex) + 2, static_cast<int>(top) - 7, IMG_OVERHEAD_MP, j.mp, j.mp_max);
                top -= 12;
            } else {  // exe 0x466ddb: en el borde izquierdo de la celda, HP en top - 12 y MP en top - 8; el nombre no se mueve
                overhead_bar(g, static_cast<int>(ex), static_cast<int>(top) - 12, IMG_OVERHEAD_HP, j.hp, j.hp_max);
                overhead_bar(g, static_cast<int>(ex), static_cast<int>(top) - 8, IMG_OVERHEAD_MP, j.mp, j.mp_max);
            }
        }
        // Texto de combate flotante (opción): daño recibido / "Miss", sube y se desvanece (DUR_COMBATE).
        if (soy_yo && a.n_textos_combate) {
            u8 w = 0;
            for (u8 i = 0; i < a.n_textos_combate; ++i) {
                App::TextoCombate& f = a.textos_combate[i];
                u64 dt = ahora - f.t0;
                if (dt >= DUR_COMBATE) continue;  // expirado: se compacta fuera
                std::string_view tx(f.t);
                int tw = ancho_texto(f.t, tx.size());
                // Arranca encima del nombre/barras y sube 26 px desvaneciéndose.
                float ty = sy - 66 - static_cast<float>(dt) / DUR_COMBATE * 26.0f;
                u8 alfa = static_cast<u8>(255 * (1.0 - static_cast<double>(dt) / DUR_COMBATE));
                g.texto_sombra(tx, ex + 24 - tw / 2, ty, clave_orden(CAPA_EFECTOS, 3), rgba(f.r, f.g, f.b, alfa));
                if (w != i) a.textos_combate[w] = f;
                ++w;
            }
            a.n_textos_combate = w;
        }
        // Nombre con los "$N" resueltos en cualquier parte ("$4 (85)" es "Adena (85)"). Los ítems
        // del suelo lo muestran solo con el puntero encima, para no tapar el mapa.
        char nombre_buf[128];
        std::string_view nombre(nombre_buf, expandir_desc(a.desc, texto(a.mundo, o.col<O_NOMBRE>()[s]), nombre_buf,
                                                          sizeof nombre_buf));
        if (a.sprites.tipo(gfx) == TIPO_ITEM && !bajo_puntero) nombre = {};
        // Panel original: solo se rotula lo que está bajo el puntero y el objetivo del ataque.
        if (!a.cfg.ui_simple && !bajo_puntero) nombre = {};
        // NPC con título: el nombre llega como "Merchant^Nombre"; se muestra "[Merchant]" encima del
        // nombre normal (la misma franja que el título de un personaje, más abajo).
        char titulo_npc_buf[64] = "";
        std::string_view titulo_npc;
        if (auto p = nombre.find('^'); p != std::string_view::npos) {
            int n = std::snprintf(titulo_npc_buf, sizeof titulo_npc_buf, "[%.*s]", static_cast<int>(p), nombre.data());
            titulo_npc = {titulo_npc_buf, std::min(static_cast<size_t>(n), sizeof titulo_npc_buf - 1)};
            nombre = nombre.substr(p + 1);
        }
        if (!nombre.empty()) {
            // Nombres y títulos pasan por el dibujo de texto del exe que entiende los códigos (0x4334f0): colores \fX, los códigos ocultos no ocupan lugar.
            const bool nombre_doble = !es_utf8(nombre.data(), nombre.size());
            int w = 0;
            recorrer_codigos(nombre, 0, nombre_doble, [&](std::string_view t, u32) { w += ancho_texto(t.data(), t.size()); });
            // Los personajes (nombre propio; los NPC y monstruos vienen como "$N") según el
            // alineamiento: legal azul, caótico rojo, neutral blanco; rosa mientras dura el PK
            // (S_PinkName). Los monstruos, según su nivel respecto del jugador (nivel_relativo).
            const bool personaje = !texto(a.mundo, o.col<O_NOMBRE>()[s]).empty() && texto(a.mundo, o.col<O_NOMBRE>()[s])[0] != '$';
            u32 color = a.cfg.ui_simple ? rgba(200, 220, 255) : BLANCO;
            u32 contorno_nivel = 0;  // interfaz original: el rango de nivel del monstruo es un contorno de 1 px, no el relleno
            // Original: el color del nombre de todo objeto sale de su valor de legalidad (0x4cc140, se fija al crearlo en 0x4cccb6), NPC
            // y monstruos incluidos; un monstruo vivo además recibe el contorno de rango de nivel de abajo.
            if (personaje || !a.cfg.ui_simple) color = ahora < o.col<O_ROSA>()[s] ? rgb555(0x767d) : color_alineamiento(o.col<O_LEGAL>()[s]);
            // Rango de nivel solo para un monstruo vivo: el exe le da a uno muerto la clase 2, no 0xa (0x4cdd30, 0x467856).
            if (!personaje && o.col<O_NIVEL>()[s] && a.mundo.jugador.nivel && tipo_objeto(a, s) == TIPO_MONSTRUO &&
                     o.col<O_ESTADO>()[s] != ESTADO_MUERTO) {
                static const u32 COLORES[] = {rgba(240, 240, 240), rgba(120, 230, 120), rgba(255, 215, 80), rgba(255, 90, 80)};
                if (a.cfg.ui_simple) color = COLORES[static_cast<int>(nivel_relativo(o.col<O_NIVEL>()[s], a.mundo.jugador.nivel))];
                else contorno_nivel = color_nivel_original(o.col<O_NIVEL>()[s], a.mundo.jugador.nivel);
            }
            if (a.cfg.ui_simple && o.ids()[s] == a.objetivo) color = rgba(255, 230, 90);
            // Título encima del nombre: el del NPC ("Merchant^...") o el del personaje (O_TITULO).
            const std::string_view titulo = !titulo_npc.empty()                  ? titulo_npc
                                            : personaje || o.col<O_MASCOTA>()[s] ? texto(a.mundo, o.col<O_TITULO>()[s])
                                                                                 : std::string_view{};
            const bool con_clan = o.col<O_CLAN>()[s] != 0;
            float nombre_x = ex + 24 - w / 2, nombre_y = top - 14, emblema_x = nombre_x - 19, emblema_y = top - 14;
            float titulo_cx = ex + 24, titulo_y = top - 28;
            if (!a.cfg.ui_simple) {
                // Caja de etiqueta del objeto bajo el puntero (exe 0x4cc030): 14 px de alto (27 con título) arriba del rect del cuadro,
                // pasa abajo si se saldría por arriba; ancho max(nombre, título) + 2, + 18 por el emblema,
                // centrada en el rect del cuadro (personaje propio: en la celda), dentro de la pantalla. Nombre en tope de la caja - 3 (+13
                // bajo un título), corrido 10 a la derecha con emblema; emblema 20 a la izquierda del nombre, 1 px más arriba; título en
                // tope de la caja - 3 (0x4676bb..0x46758e). Las barras no la empujan hacia arriba.
                int wt = 0;
                if (!titulo.empty()) {
                    const bool doble = !es_utf8(titulo.data(), titulo.size());
                    recorrer_codigos(titulo, 0, doble, [&](std::string_view t, u32) { wt += ancho_texto(t.data(), t.size()); });
                }
                const float alto = titulo.empty() ? 14.0f : 27.0f;
                float caja_y = top_sprite - alto;
                if (caja_y <= 0) caja_y = pie + 1;
                const float ancho = static_cast<float>(std::max(w, wt) + 2 + (con_clan ? 18 : 0));
                const float centro = soy_yo ? ex + 24 : (caja_x0 + caja_x1) / 2;
                const float l = std::clamp(centro - ancho / 2, 0.0f, std::max(0.0f, static_cast<float>(a.gpu.ancho()) - ancho));
                nombre_x = l + (ancho - static_cast<float>(w)) / 2 + (con_clan ? 10.0f : 0.0f);
                nombre_y = caja_y - 3 + (titulo.empty() ? 0.0f : 13.0f);
                emblema_x = nombre_x - 20;
                emblema_y = nombre_y - 1;
                titulo_cx = l + ancho / 2;
                titulo_y = caja_y - 3;
            }
            if (a.cfg.ui_simple) g.texto_sombra(nombre, nombre_x, nombre_y, clave_orden(CAPA_EFECTOS, 0), color);
            else {
                float x = nombre_x;
                recorrer_codigos(nombre, color, nombre_doble, [&](std::string_view t, u32 c) {
                    x += g.texto_nombre(t, x, nombre_y, clave_orden(CAPA_EFECTOS, 0), c, contorno_nivel);
                });
            }
            if (const Vendedor* v = vendedor(a, o.ids()[s]); a.cfg.ui_simple && v && !v->titulo.empty()) {  // título de la tienda
                dibujar_titulo(g, v->titulo, ex + 24, top - 28, clave_orden(CAPA_EFECTOS, 1), rgba(255, 220, 130));
            }
            // Marcado con True Target: la marca y su mensaje encima del nombre.
            if (o.ids()[s] == a.marca_objetivo) {
                const int wm = ancho_texto(a.marca_texto.data(), a.marca_texto.size());
                g.texto_sombra("V", ex + 24 - 3, top - 28, clave_orden(CAPA_EFECTOS, 0), rgba(255, 80, 60));
                if (wm) g.texto_sombra(a.marca_texto, ex + 24 - wm / 2, top - 42, clave_orden(CAPA_EFECTOS, 0), rgba(255, 160, 140));
            }
            // El señor de un castillo: una corona a la izquierda (del emblema, si tiene).
            if (castillo_de(a, o.ids()[s]))
                dibujar_corona(g, ex + 24 - w / 2 - (o.col<O_CLAN>()[s] ? 19 : 0) - 16, top - 12, clave_orden(CAPA_EFECTOS, 0));
            // Emblema del clan a la izquierda del nombre (se pide una vez por clan).
            if (con_clan) dibujar_emblema(a, o.col<O_CLAN>()[s], emblema_x, emblema_y);
            top -= 14;
            if (!titulo.empty()) {
                // Original: el mismo color que el nombre (ambos toman [obj+0x2c], 0x46778e / 0x467a11); los códigos \f del título lo reemplazan.
                dibujar_titulo(g, titulo, titulo_cx, titulo_y, clave_orden(CAPA_EFECTOS, 0), a.cfg.ui_simple ? rgba(230, 200, 130) : color,
                               !a.cfg.ui_simple);
                top -= 14;
            }
        }
        // Sin etiqueta de nombre el emblema flota sobre la cabeza (0x462d60): ancla de la celda + (17, -52).
        if (nombre.empty() && o.col<O_CLAN>()[s] && !(o.col<O_FLAGS>()[s] & FLAG_INVISIBLE)) {
            const float ay = a.cfg.camara_a_saltos ? static_cast<float>(mundo_py(vis_x, vis_y) + oy) * z : sy;
            dibujar_emblema(a, o.col<O_CLAN>()[s], ex + 17, ay - 52);
        }
        // checkbmp: el bitmap revisado flota sobre el personaje durante 10 s (0x4669d1).
        if (soy_yo && ahora < a.t_vista_emblema) {
            const float ay = a.cfg.camara_a_saltos ? static_cast<float>(mundo_py(vis_x, vis_y) + oy) * z : sy;
            const RegionAtlas& em = g.emblema(Graficos::CLAN_VISTA);
            if (em.valida()) g.gpu().dibujar_en(em, ex - 32, ay - 86, clave_orden(CAPA_EFECTOS, 0));
        }
        if (a.cfg.ui_simple) dibujar_globo(a, a.chat.globo(o.ids()[s], ahora), ex + 24, top);
        else if (const std::string_view dicho = a.chat.globo(o.ids()[s], ahora, soy_yo ? 5000 : 8000); !dicho.empty())
            // original: 8 s para los demás, 5 s para el personaje propio (0x41a530), anclado en el tope del sprite
            dibujar_globo(a, dicho, ex + 24, top_sprite - 2, true, a.chat.globo_activo(o.ids()[s], ahora, soy_yo ? 5000 : 8000), pie);
        else if (const Vendedor* v = vendedor(a, o.ids()[s]); v && !v->titulo.empty()) {
            // Cartel de tienda privada (0x487140 -> 0x4ce3b0, 0x41a400): hasta dos líneas, como globo solo sobre el personaje propio
            // o en los mapas de mercado 340 / 350 / 360 / 370, cambiando de línea cada 5 s (una segunda línea vacía se salta).
            // Agregado del fork: el mapa 800, el mercado de servidores más nuevos (no está en el exe 2009).
            const MapId m = a.mundo.mapa;
            if (soy_yo || m == 340 || m == 350 || m == 360 || m == 370 || m == 800) {
                std::string_view lineas[2];
                lineas_cartel_tienda(v->titulo, lineas);
                const std::string_view linea = !lineas[1].empty() && (ahora / 5000) % 2 ? lineas[1] : lineas[0];
                dibujar_globo(a, linea, ex + 24, top_sprite - 2, true, nullptr, pie, false);
            }
        }
        g.gpu().sin_zoom_mundo(false);
    }
}

// Gráficos de hechizos (Mundo::efectos): la acción 0 del gfx una vez, sobre su objeto o en su
// celda; un proyectil, en bucle mientras vuela. Se ordenan como un personaje parado en esa celda,
// un poco por encima de él.
void dibujar_efectos(App& a, u64 ahora, double ox, double oy) {
    Graficos& g = *a.g;
    Mundo& m = a.mundo;
    for (EfectoVisual& ef : m.efectos) {
        if (ef.gfx == 0 || ahora < ef.inicio) continue;
        const RangoAccion* r = a.sprites.accion(ef.gfx, 0);
        if (!r) {
            ef.gfx = 0;
            continue;
        }
        const u64 t = ahora - ef.inicio;
        double x = ef.x0, y = ef.y0;
        bool termino = false;
        const FrameAccion* f;
        if (ef.vuelo_ms) {
            if (t >= ef.vuelo_ms) {
                // Llegó: sigue el gráfico encadenado (la explosión), en la celda de destino.
                EfectoVisual sig;
                sig.gfx = a.sprites.siguiente(ef.gfx);
                sig.x0 = sig.x1 = ef.x1;
                sig.y0 = sig.y1 = ef.y1;
                sig.inicio = ef.inicio + ef.vuelo_ms;
                ef.gfx = 0;
                if (sig.gfx) agregar_efecto(m, sig);
                continue;
            }
            double k = static_cast<double>(t) / ef.vuelo_ms;
            x = ef.x0 + (ef.x1 - ef.x0) * k;
            y = ef.y0 + (ef.y1 - ef.y0) * k;
            f = &a.sprites.frame_en(*r, t, termino, 0, true);
        } else {
            f = &a.sprites.frame_en(*r, t, termino, 0, false);
            if (termino) {
                ef.gfx = 0;
                continue;
            }
            if (ef.objeto) {
                Slot s = m.objetos.buscar(ef.objeto);
                if (s == SIN_SLOT) {
                    ef.gfx = 0;
                    continue;
                }
                posicion_visible(m, s, ahora, x, y);
            }
        }
        sonido_de_frame(a, ef.sonido, *f, x, y, ahora, true);
        // Destello: desde su cuadro de disparo hasta que termina, el efecto mantiene el brillo máximo (0x450650 / 0x450970).
        if (ef.flash && f - &a.sprites.frame_en(*r, 0, termino, 0, false) >= (ef.flash == 1 ? 2 : 1)) a.flash_luz_frame = a.frame;
        u16 sub = static_cast<u16>(f->sub + (r->direccional ? ef.dir : 0));
        const RegionAtlas& rg = g.frame_sprite(a.sprites.gfx_archivos(ef.gfx), sub, f->frame);
        float px = static_cast<float>(mundo_px(x, y) + ox), py = static_cast<float>(mundo_py(x, y) + oy);
        u32 color_ef = BLANCO;
        Mezcla mezcla_ef = Mezcla::NORMAL;
        if (!preparar_mezcla(a.sprites, ef.gfx, color_ef, mezcla_ef)) continue;
        const u32 prof_ef = a.cfg.ui_simple ? profundidad(static_cast<float>(mundo_py(x, y)) + 12.0f, ORDEN_PERSONAJE, 6)
                                            : clave_celda(px, py, CAPA_CUERPO_ORIGINAL, 6);
        g.gpu().dibujar_en(rg, px, py, clave_orden(CAPA_MUNDO, prof_ef),
                           color_ef, mezcla_ef);
    }
}

// Efectos fijos del mapa (capa 8 del .s32, SegmentoMapa::efectos): el sprite en bucle en su celda, ordenado como un personaje parado
// ahí, y su luz (luz_efecto_mapa) desde el centro del rombo, como la de un objeto. Va después de dibujar_personajes, que vacía a.luces.
void dibujar_efectos_mapa(App& a, u64 ahora, double ox, double oy) {
    Graficos& g = *a.g;
    const Vista vi = vista_mundo(a);
    for (const SegmentoMapa& seg : a.mundo.segmentos)
        for (const EfectoMapa& e : seg.efectos) {
            const float px = static_cast<float>(mundo_px(e.x, e.y) + ox), py = static_cast<float>(mundo_py(e.x, e.y) + oy);
            auto fuera = [&](float mx, float my) { return px < -mx || px > vi.w + mx || py < -my || py > vi.h + my; };
            if (const int radio = luz_efecto_mapa(e.gfx); radio > 0 && !fuera(360, 200)) a.luces.push_back({radio, px + 24, py + 12});
            if (fuera(150, 150)) continue;
            const RangoAccion* r = a.sprites.accion(e.gfx, ACC_QUIETO);
            if (!r) r = a.sprites.accion(e.gfx, ACC_CAMINAR);
            if (!r) continue;
            bool termino = false;
            const FrameAccion& f = a.sprites.frame_en(*r, ahora, termino, 0, true);
            u32 color = BLANCO;
            Mezcla mezcla = Mezcla::NORMAL;
            if (!preparar_mezcla(a.sprites, e.gfx, color, mezcla)) continue;
            const RegionAtlas& rg = g.frame_sprite(a.sprites.gfx_archivos(e.gfx), f.sub, f.frame);
            const u32 prof = a.cfg.ui_simple ? profundidad(static_cast<float>(mundo_py(e.x, e.y)) + 12.0f, ORDEN_PERSONAJE, 6)
                                             : clave_celda(px, py, CAPA_CUERPO_ORIGINAL, 6);
            g.gpu().dibujar_en(rg, px, py, clave_orden(CAPA_MUNDO, prof), color, mezcla);
        }
}

}  // namespace

// El corcho de pesca propio (pesca_ui.cpp): gfx 6299, la picada 6385, en bucle sobre su celda.
void dibujar_pesca(App& a, u64 ahora, double ox, double oy) {
    const App::Pesca& p = a.pesca;
    if (!p.activa) return;
    const GfxId gfx = p.picada ? 6385 : 6299;
    const RangoAccion* r = a.sprites.accion(gfx, 0);
    if (!r) return;
    bool termino = false;
    const FrameAccion& f = a.sprites.frame_en(*r, ahora - p.inicio, termino, 0, true);
    const RegionAtlas& rg = a.g->frame_sprite(a.sprites.gfx_archivos(gfx), f.sub, f.frame);
    const float px = static_cast<float>(mundo_px(p.x, p.y) + ox), py = static_cast<float>(mundo_py(p.x, p.y) + oy);
    u32 color = BLANCO;
    Mezcla mezcla = Mezcla::NORMAL;
    if (!preparar_mezcla(a.sprites, gfx, color, mezcla)) return;
    a.g->gpu().dibujar_en(rg, px, py, clave_orden(CAPA_MUNDO, clave_celda(px, py, 18, 6)), color, mezcla);
}

namespace {

int direccion_hacia(int dx, int dy) {
    double ang = std::atan2(dy * 2.0, static_cast<double>(dx));  // se deshace la compresión isométrica
    int paso = static_cast<int>(std::lround(ang / (3.14159265358979 / 4)));
    return ((3 + paso) % 8 + 8) % 8;
}

int direccion_teclas(const EntradaFrame& e) {
    if (e.arriba && e.derecha) return 2;
    if (e.arriba && e.izquierda) return 0;
    if (e.abajo && e.derecha) return 4;
    if (e.abajo && e.izquierda) return 6;
    if (e.arriba) return 1;
    if (e.abajo) return 5;
    if (e.derecha) return 3;
    if (e.izquierda) return 7;
    return -1;
}

// Ataque al objetivo (adyacente o, con arco, a distancia): la animación propia empieza ya (el eco
// del servidor no la reinicia, ver Ev::ATAQUE) y no se vuelve a atacar hasta que termine. La
// flecha la dibuja el eco (S_UseArrowSkill trae su gráfico y las celdas: ver mundo.cpp).
void atacar(App& a, Slot yo, Slot obj, u64 ahora) {
    TablaObjetos& o = a.mundo.objetos;
    int dir = direccion_de(o.col<O_X>()[obj] - o.col<O_X>()[yo], o.col<O_Y>()[obj] - o.col<O_Y>()[yo]);
    if (dir >= 0) o.col<O_DIR>()[yo] = static_cast<u8>(dir);
    enviar(a, cmd::atacar(a.op, o.ids()[obj], o.col<O_X>()[obj], o.col<O_Y>()[obj]));
    combate_musica(a, obj, ahora);
    GfxId gfx = o.col<O_GFX>()[yo];
    // Compensación de ataque (opcional, en vivo): sin esperar el eco del servidor, mostrar ya el golpe
    // -el swing propio y el flinch del objetivo en el frame de impacto- para que con latencia alta el
    // combate se sienta instantáneo. Puede "errar" en un fallo o fuera de rango; el servidor lo corrige
    // (mundo.cpp no reinicia un flinch ya en curso). Apagado: la animación espera el eco (como el stock).
    // Overweight: the server refuses (message 110) and never echoes, so no local swing either.
    // Not against a character when either side stands in a safety zone: the server refuses that attack and never echoes
    // it, so the original (which always waits for the echo) shows nothing.
    const bool pvp_seguro = tipo_objeto(a, obj) == TIPO_PERSONAJE &&
                            (tipo_zona_celda(a.mundo, a.rec, o.col<O_X>()[yo], o.col<O_Y>()[yo]) == TipoZona::SEGURA ||
                             tipo_zona_celda(a.mundo, a.rec, o.col<O_X>()[obj], o.col<O_Y>()[obj]) == TipoZona::SEGURA);
    if (a.cfg.compensar_ataque && !sobrecargado(a.mundo.jugador.peso) && !pvp_seguro) {
        o.col<O_ACCION>()[yo] = ACC_ATACAR;
        o.col<O_TACCION>()[yo] = ahora;
        o.col<O_DANO_RAPIDO>()[yo] = 0;
        // El SetAction(1) del original pasa acá: el eco ya no gasta (mundo.cpp, ataque_objeto con `previsto`).
        gastar_rafaga(o, yo, ACC_ATACAR);
        fijar_ritmo(a.mundo, yo);
        if (o.col<O_ESTADO>()[obj] != ESTADO_MUERTO) {
            const u8 acc_ataque = accion_con_arma(a.mundo, yo, ACC_ATACAR);
            u64 impacto = a.sprites.ms_hasta_impacto(gfx, acc_ataque);
            if (impacto == TablaSprites::SIN_IMPACTO) impacto = 0;
            const Ritmo rt = ritmo_de(a.mundo, yo, acc_ataque);  // el "!" se corre con haste, brave... como en el eco
            impacto = impacto * rt.num / rt.den;
            o.col<O_ACCION>()[obj] = ACC_DANADO;
            o.col<O_TACCION>()[obj] = ahora + impacto;
            o.col<O_DANO_RAPIDO>()[obj] = 1;
            fijar_ritmo(a.mundo, obj);
        }
    }
    u32 ms = a.sprites.duracion_ms(gfx, static_cast<u8>(ACC_ATACAR + o.col<O_ESTADO>()[yo]));
    if (ms == 0) ms = a.sprites.duracion_ms(gfx, ACC_ATACAR);
    if (ms == 0) ms = ATAQUE_DESCONOCIDO_MS;
    a.fin_ataque = ahora + intervalo_propio(a.mundo.jugador, Intervalo::ATACAR, ms);
}

// Movimiento propio con predicción: se avanza localmente y se avisa al servidor (C_MoveChar
// lleva la celda de origen). Si la celda está bloqueada solo se gira.

// Portales (entradas de mazmorras, escaleras): los que trae el mapa del cliente (el final del .s32)
// se piden con C_EnterPortal, como el original (mover_jugador). Pero el servidor tiene algunos que
// el cliente no (en el mapa 4, 26 de 74), y también teletransporta cuando el destino de un paso es
// un portal de su tabla (C_MoveChar mira DungeonTable antes que nada). Para esos, al
// intentar entrar a una celda bloqueada vecina con el teclado o con un clic justo sobre ella, se
// manda el paso sin moverse aquí: si es un portal llega el teletransporte; si no, no pasa nada
// visible (el servidor toma el origen del próximo paso del paquete). Una vez por segundo por celda.
bool intento_portal(App& a, int nx, int ny, int dir, bool teclado, u64 ahora) {
    const u32 celda = static_cast<u32>(nx & 0xFFFF) << 16 | static_cast<u32>(ny & 0xFFFF);
    if (celda == a.celda_intento_portal && ahora < a.t_intento_portal + 1000) return false;
    if (!teclado) {
        // ¿El puntero está sobre esa celda? (inversa de mundo_px / mundo_py desde el centro del personaje)
        const Vista vi = vista_mundo(a);
        const double rx = (raton_mundo_x(a) - vi.cx) / 24.0, ry = (raton_mundo_y(a) - vi.cy) / 12.0;
        const int cx = static_cast<int>(std::lround((rx - ry) / 2)), cy = static_cast<int>(std::lround((rx + ry) / 2));
        if (cx != DIR_DX[dir] || cy != DIR_DY[dir]) return false;
    }
    a.celda_intento_portal = celda;
    a.t_intento_portal = ahora;
    return true;
}

// La mascota o invocación propia (a.mascotas, S_PetCtrlMenu): un clic le habla (su menú de órdenes), no
// la ataca. Con Ctrl sí se la ataca.
bool mascota_propia(const App& a, Slot s) {
    if (s == SIN_SLOT || !a.mundo.objetos.col<O_MASCOTA>()[s]) return false;
    const ObjId id = a.mundo.objetos.ids()[s];
    return std::any_of(a.mascotas.begin(), a.mascotas.end(), [id](const Mascota& m) { return m.id == id; });
}

// Una mascota (o invocación) de otro jugador no se ataca con un clic: solo con Ctrl.
bool mascota_sin_ctrl(const App& a, Slot s) {
    if (s == SIN_SLOT || !a.mundo.objetos.col<O_MASCOTA>()[s] || a.entrada.ctrl) return false;
    return !mascota_propia(a, s);
}

}  // namespace

// Clase de objeto del cliente original (0x4cdd30), según el tipo de sprite: un monstruo vivo 10 (muerto 2, mascota cuyo
// amo es el jugador 6, mascota de otro 5), personajes 5 y NPC 6 / 12 (muertos 3), los demás tipos tal cual.
// Elige el cursor (0x469e40) y, entre objetos superpuestos bajo el puntero, el del hover (0x46a450).
u8 clase_objeto(const App& a, Slot s) {
    const TablaObjetos& o = a.mundo.objetos;
    const bool muerto = o.col<O_ESTADO>()[s] == ESTADO_MUERTO;
    const u8 tipo = tipo_objeto(a, s);
    if (tipo == TIPO_MONSTRUO) return muerto ? 2 : mascota_propia(a, s) ? 6 : o.col<O_MASCOTA>()[s] ? 5 : TIPO_MONSTRUO;
    if (tipo == 5 || tipo == 6 || tipo == 12) return muerto ? 3 : tipo;
    return tipo;
}

namespace {

// Objetivo bajo el puntero según su tipo de gfx: un ítem se recoge, un monstruo se ataca, a un
// NPC se le habla (con Ctrl se lo ataca) y a otros personajes se los ataca con Ctrl o en modo PK
// ("/pk"). 0 si no hay (se camina).
void elegir_objetivo_bajo_puntero(App& a) {
    const TablaObjetos& o = a.mundo.objetos;
    const EntradaFrame& e = a.entrada;
    a.objetivo = objeto_bajo_puntero(a);
    Slot b = a.objetivo ? o.buscar(a.objetivo) : SIN_SLOT;
    u8 tipo = b != SIN_SLOT ? tipo_objeto(a, b) : 0;
    // Un NPC (sin Ctrl): se camina hasta él y se le habla, aunque se suelte el botón.
    a.hablar_objetivo = b != SIN_SLOT && (es_npc(a, b) || vendedor(a, a.objetivo) || mascota_propia(a, b)) && !e.ctrl;
    const bool forzar = e.ctrl || ((a.modo_pk || a.modo_ataque) && tipo == TIPO_PERSONAJE);
    if (b != SIN_SLOT && tipo != TIPO_ITEM && tipo != TIPO_MONSTRUO && !a.hablar_objetivo && !forzar) a.objetivo = 0;
    if (mascota_sin_ctrl(a, b)) {  // la de otro jugador: solo con Ctrl
        a.objetivo = 0;
        a.hablar_objetivo = false;
    }
}

void mover_jugador(App& a, u64 ahora) {
    TablaObjetos& o = a.mundo.objetos;
    Slot s = o.buscar(a.mundo.jugador.id);
    const EntradaFrame& e = a.entrada;
    const Vista vi = vista(a);
    bool sobre_panel = (e.my >= vi.panel_y && e.mx >= vi.panel_x0 && e.mx < vi.panel_x1) || sobre_inventario(a) ||
                       sobre_hechizos(a) || sobre_barra(a) || sobre_forma(a) || sobre_cantidad(a) || sobre_tienda(a) || sobre_tablon(a) || sobre_correo(a) || sobre_casas(a) || sobre_mapa(a) || sobre_mascotas(a) || sobre_destino(a) || sobre_npc(a) || a.arrastrando != 0 || a.arrastrando_hechizo != 0 ||
                       a.arrastrando_atajo >= 0 || a.autobuffs.arrastrando >= 0;
    // Objetivo: lo que hay bajo el puntero al apretar el botón, y se mantiene mientras siga
    // apretado aunque la cámara se mueva y el puntero ya no esté encima (se camina hasta él y se
    // lo ataca). Apretado sobre el suelo se camina hacia el puntero, y si el puntero pasa sobre
    // algo, eso queda como objetivo. Soltar lo abandona, salvo con la tecla A (ataque sostenido).
    bool clic_en_mundo = e.presionado && !a.clic_consumido && !sobre_panel && !sobre_alguna_ventana(a) &&
                         a.ventana_arrastrada < 0;
    // Pesca: un clic en el mundo con el corcho afuera lo recoge (0x469220).
    if (clic_en_mundo && e.pulsado && clic_pesca(a)) {
        a.clic_consumido = true;
        return;
    }
    // Invitar al grupo (0x402490): el clic solo elige el objetivo.
    if (a.invitando && clic_en_mundo && e.pulsado) {
        elegir_invitado(a);
        a.clic_consumido = true;
        return;
    }
    // Shift+clic en el mundo: solo girar hacia ahí, sin caminar ni elegir objetivo (por ejemplo, para
    // quedar frente a otro personaje e intercambiar). Mantener el botón tampoco camina. Como un clic
    // nuevo, cancela lo que estuviera haciendo; inmóvil, a mitad de un paso o de un ataque no gira.
    if (clic_en_mundo && e.pulsado && e.shift && s != SIN_SLOT && !a.mundo.jugador.muerto) {
        a.hechizo_pendiente = 0;
        a.ataque_sostenido = a.objetivo_fijo = a.hablar_objetivo = false;
        a.objetivo = 0;
        a.clic_consumido = true;
        if (!a.mundo.jugador.inmovil && !moviendose(a.mundo, s, ahora) && ahora >= a.fin_ataque) {
            const u8 d = static_cast<u8>(direccion_hacia(e.mx - vi.cx, e.my - vi.cy));
            o.col<O_DIR>()[s] = d;
            enviar(a, cmd::girar(a.op, d));
        }
        return;
    }
    // Antes de esperar el fin del paso: el frame en que se aprieta no se pierde.
    if (clic_en_mundo && e.pulsado) {
        a.hechizo_pendiente = 0;  // un clic nuevo cancela el hechizo en camino
        a.ataque_sostenido = false;
        elegir_objetivo_bajo_puntero(a);
        a.objetivo_fijo = a.objetivo != 0;
    }
    // Como el cliente original: presionar sobre un monstruo y soltar el botón sobre el panel inferior, y el
    // ataque sigue solo (como la tecla A) hasta un nuevo clic en el mundo, caminar, o que el objetivo
    // muera o desaparezca.
    const bool released_on_panel = e.my >= vi.panel_y && e.mx >= vi.panel_x0 && e.mx < vi.panel_x1;
    if (e.clic && released_on_panel && a.objetivo && !a.hablar_objetivo) {
        const Slot b = o.buscar(a.objetivo);
        const bool atacable = b != SIN_SLOT && tipo_objeto(a, b) != TIPO_ITEM && !mascota_sin_ctrl(a, b);
        if (sostener_ataque_al_soltar(a.objetivo_fijo, atacable, true)) a.ataque_sostenido = true;
    }
    if (!e.presionado) a.objetivo_fijo = false;
    // Tecla A (con el chat cerrado): atacar al objetivo, o al monstruo bajo el puntero, hasta que
    // muera o se camine.
    if (e.tecla_a && a.cfg.chat_con_enter && !a.escribiendo_chat && !a.cantidad.abierto && !escribiendo_en_ventana(a)) {
        ObjId t = a.objetivo && !a.hablar_objetivo ? a.objetivo : objeto_bajo_puntero(a);
        Slot b = t ? o.buscar(t) : SIN_SLOT;
        if (b != SIN_SLOT && !mascota_sin_ctrl(a, b) && !(mascota_propia(a, b) && !e.ctrl) &&
            (tipo_objeto(a, b) == TIPO_MONSTRUO || e.ctrl || ((a.modo_pk || a.modo_ataque) && tipo_objeto(a, b) == TIPO_PERSONAJE))) {
            a.objetivo = t;
            a.hablar_objetivo = false;
            a.ataque_sostenido = true;
        }
    }
    if (s == SIN_SLOT || a.mundo.jugador.muerto || moviendose(a.mundo, s, ahora) || ahora < a.fin_ataque) return;
    if (a.mundo.jugador.inmovil) return;  // paralizado, dormido, congelado...: el servidor no lo dejaría
    u64 fin_paso = o.col<O_TMOV>()[s] + o.col<O_DURMOV>()[s];
    u64 inicio_paso = (o.col<O_TMOV>()[s] > 0 && ahora - fin_paso < MS_ENCADENAR_PASO) ? fin_paso : ahora;
    int dir = direccion_teclas(e);
    const int dir_teclas = dir;
    if (dir_teclas >= 0 && a.ataque_sostenido) {  // caminar con el teclado lo termina
        a.ataque_sostenido = false;
        a.objetivo = 0;
    }
    bool ruta = false;
    // Hechizo pendiente: un paso hacia el objetivo o lanzarlo.
    if (dir < 0 && !clic_en_mundo) {
        int d = paso_hechizo_pendiente(a, s, ahora);
        if (d == -2) return;
        dir = d;
    }
    if (clic_en_mundo && !a.objetivo_fijo) {
        // Apretado sobre el suelo: lo que el puntero toque al caminar queda como objetivo.
        elegir_objetivo_bajo_puntero(a);
        a.objetivo_fijo = a.objetivo != 0;
    } else if (!e.presionado && !a.hablar_objetivo && !a.ataque_sostenido) {
        a.objetivo = 0;
    }
    Slot obj = a.objetivo ? o.buscar(a.objetivo) : SIN_SLOT;
    if (obj != SIN_SLOT && o.col<O_ESTADO>()[obj] == ESTADO_MUERTO) obj = SIN_SLOT;
    if (obj == SIN_SLOT) {  // murió o ya no está
        a.objetivo = 0;
        a.hablar_objetivo = false;
        a.ataque_sostenido = false;
        a.objetivo_fijo = false;
    }
    if (dir < 0 && obj != SIN_SLOT) {
        int dx = o.col<O_X>()[obj] - o.col<O_X>()[s], dy = o.col<O_Y>()[obj] - o.col<O_Y>()[s];
        if (a.hablar_objetivo && std::max(std::abs(dx), std::abs(dy)) <= DISTANCIA_HABLAR_NPC) {
            // A una puerta no se le habla: se abre o se cierra (C_Door).
            if (vendedor(a, a.objetivo))
                abrir_tienda_de(a, a.objetivo);
            else if (a.sprites.tipo(o.col<O_GFX>()[obj]) == TIPO_CARTEL)
                abrir_cartel(a, obj);  // los carteles abren una ficha local (exe 0x408300); el servidor ignora los clics sobre ellos
            else if (es_tablon(o.col<O_GFX>()[obj]))
                enviar(a, cmd::tablon(a.op, Cli::BOARD, a.objetivo));
            else if (tipo_objeto(a, obj) == TIPO_PUERTA)
                enviar(a, cmd::puerta(a.op, static_cast<u16>(o.col<O_X>()[obj]), static_cast<u16>(o.col<O_Y>()[obj]), a.objetivo));
            else
                enviar(a, cmd::hablar_npc(a.op, a.objetivo));
            a.objetivo = 0;
            a.hablar_objetivo = false;
            a.clic_consumido = true;  // mantener el clic no le vuelve a hablar
            if (dir < 0 && (dx || dy)) {
                int d = direccion_de(dx, dy);
                if (d >= 0 && o.col<O_DIR>()[s] != d) {
                    enviar(a, cmd::girar(a.op, static_cast<u8>(d)));
                    o.col<O_DIR>()[s] = static_cast<u8>(d);
                }
            }
            return;
        }
        const bool es_item = a.sprites.tipo(o.col<O_GFX>()[obj]) == TIPO_ITEM;
        // A un NPC al que se va a hablar no se lo ataca, aunque esté al alcance del arco: se camina
        // hasta él.
        if (!a.hablar_objetivo && std::max(std::abs(dx), std::abs(dy)) <= (es_item ? 1 : alcance_ataque(o.col<O_ESTADO>()[s]))) {
            if (es_item) {
                // Recoger (el servidor acepta hasta 3 celdas); se espera un poco antes de reintentar.
                pedir_recoger(a, o.col<O_X>()[obj], o.col<O_Y>()[obj], a.objetivo, o.col<O_CANTIDAD>()[obj]);
                a.fin_ataque = ahora + 400;
                a.objetivo = 0;
            } else {
                atacar(a, s, obj, ahora);
            }
            return;
        }
        dir = direccion_de(dx, dy);  // todavía lejos: un paso hacia el objetivo
    }
    if (dir < 0 && clic_en_mundo)
        dir = direccion_hacia(e.mx - vi.cx, e.my - vi.cy);  // el jugador está siempre en (cx, cy)
    if (dir < 0) return;
    u16 x = o.col<O_X>()[s], y = o.col<O_Y>()[s];
    int nx = x + DIR_DX[dir], ny = y + DIR_DY[dir];
    const u32 celda = static_cast<u32>(nx & 0xFFFF) << 16 | static_cast<u32>(ny & 0xFFFF);
    const bool pasable = celda_pasable(a.mundo, a.rec, nx, ny);
    if (!pasable && es_portal(a.mundo, a.rec, nx, ny) && !(celda == a.celda_intento_portal && ahora < a.t_intento_portal + 1000)) {
        // Portal conocido por el mapa del cliente, en una celda bloqueada: como el original,
        // C_EnterPortal con su celda. (En una pasable alcanza el paso: C_MoveChar también la revisa,
        // y así un portal que el servidor no tiene no impide pisarla.)
        enviar(a, cmd::entrar_portal(a.op, static_cast<u16>(nx), static_cast<u16>(ny)));
        a.celda_intento_portal = celda;
        a.t_intento_portal = ahora;
        o.col<O_DIR>()[s] = static_cast<u8>(dir);
    } else if (pasable) {
        enviar(a, cmd::mover(a.op, x, y, static_cast<u8>(dir)));
        o.col<O_XANT>()[s] = x;
        o.col<O_YANT>()[s] = y;
        o.col<O_X>()[s] = static_cast<u16>(nx);
        o.col<O_Y>()[s] = static_cast<u16>(ny);
        o.col<O_TMOV>()[s] = inicio_paso;
        o.col<O_ZANCADA>()[s] ^= 1;
        o.col<O_DURMOV>()[s] = static_cast<u16>(intervalo_propio(
            a.mundo.jugador, Intervalo::CAMINAR, duracion_paso(a.mundo, o.col<O_GFX>()[s], o.col<O_ESTADO>()[s], 0),
            MS_ENCADENAR_PASO));
        o.col<O_DIR>()[s] = static_cast<u8>(dir);
    } else if (intento_portal(a, nx, ny, dir, dir_teclas >= 0 || ruta, ahora)) {
        enviar(a, cmd::mover(a.op, x, y, static_cast<u8>(dir)));
        o.col<O_DIR>()[s] = static_cast<u8>(dir);
    } else if (o.col<O_DIR>()[s] != dir) {
        enviar(a, cmd::girar(a.op, static_cast<u8>(dir)));
        o.col<O_DIR>()[s] = static_cast<u8>(dir);
    }
}

}  // namespace

// F4 (cliente original 0x4087c0): recoge el primer objeto del suelo dentro de 2 celdas a lo ancho y 1 hacia abajo del mapa,
// como mucho alrededor de una vez por segundo (el original espera 0x3da ms entre recogidas).
void pick_up_nearby(App& a) {
    TablaObjetos& o = a.mundo.objetos;
    const Slot yo = o.buscar(a.mundo.jugador.id);
    const u64 ahora = a.entrada.ahora_ms;
    if (yo == SIN_SLOT || ahora < a.next_pickup) return;
    const int px = o.col<O_X>()[yo], py = o.col<O_Y>()[yo];
    for (Slot s = 0; s < o.tamano(); ++s) {
        if (s == yo || a.sprites.tipo(o.col<O_GFX>()[s]) != TIPO_ITEM) continue;
        const int x = o.col<O_X>()[s], y = o.col<O_Y>()[s];
        if (std::abs(x - px) > 2 || std::abs(y - py) > 1) continue;
        pedir_recoger(a, x, y, o.ids()[s], o.col<O_CANTIDAD>()[s]);
        a.next_pickup = ahora + 0x3da;
        return;
    }
}

namespace {

// La fuente de números del HUD original (S3EP1U 0x431410, tabla de glifos en 0x501410): glifos de 4x6 píxeles, 3 bytes cada uno (dos filas
// por byte, nibble bajo primero, bit 0 = el de más a la izquierda). Avance: '1' y '/' 4 px, '.' 2 px, todo lo demás 5 px.
constexpr u8 FUENTE_HUD[] = {0x96, 0x99, 0x69, 0x32, 0x22, 0x72, 0x96, 0x68, 0xf1, 0x96, 0x84, 0x69, 0x64, 0x56,
                             0x4f, 0x1f, 0x87, 0x69, 0x16, 0x97, 0x69, 0x8f, 0x44, 0x22, 0x96, 0x96, 0x69, 0x96,
                             0xe9, 0x68, 0x44, 0x22, 0x11, 0x00, 0x00, 0x10, 0x59, 0x24, 0x9a, 0x00, 0x0f, 0x00};

int glifo_hud(char c) {
    if (c >= '0' && c <= '9') return (c - '0') * 3;
    switch (c) {
        case '/': return 0x1e;
        case '.': return 0x21;
        case '%': return 0x24;
        case '-': return 0x27;
        default: return -1;
    }
}

int avance_hud(char c) { return c == '1' || c == '/' ? 4 : c == '.' ? 2 : 5; }

void texto_hud_pasada(Graficos& g, int x, int y, std::string_view t, u64 k, u32 color) {
    for (const char c : t) {
        if (const int o = glifo_hud(c); o >= 0) {
            if (g.texto_hd_para(k)) {  // pasada HD: la misma máscara de 4x6 como campo suave
                u8 filas[6];
                for (int fila = 0; fila < 6; ++fila) filas[fila] = (FUENTE_HUD[o + fila / 2] >> (fila % 2 ? 4 : 0)) & 15;
                g.gpu().dibujar_en(g.glifo_hd_mascara(0x1000 + static_cast<u32>(o), 4, 6, filas), static_cast<float>(x),
                                   static_cast<float>(y), k, color, Mezcla::TEXTO_HD);
                x += avance_hud(c);
                continue;
            }
            for (int fila = 0; fila < 6; ++fila) {
                const u8 bits = (FUENTE_HUD[o + fila / 2] >> (fila % 2 ? 4 : 0)) & 15;
                for (int col = 0; col < 4; ++col)
                    if (bits >> col & 1) g.gpu().rectangulo(x + col, y + fila, 1, 1, k, color);
            }
        }
        x += avance_hud(c);
    }
}

// Texto numérico del HUD inferior: liso, sin sombra. `contorno` (distinto de cero) agrega el contorno de 1 px en 4 direcciones que tienen
// los números de HP/MP (0x4b13b8, 0x4b15bc). `derecha` alinea a la derecha como el original: x = derecha - 2 * largo.
void texto_hud(Graficos& g, int x, int y, std::string_view t, u32 contorno = 0) {
    const u64 k = clave_orden(CAPA_UI_TEXTO, 0);
    if (contorno) {
        texto_hud_pasada(g, x - 1, y, t, k, contorno);
        texto_hud_pasada(g, x + 1, y, t, k, contorno);
        texto_hud_pasada(g, x, y - 1, t, k, contorno);
        texto_hud_pasada(g, x, y + 1, t, k, contorno);
    }
    texto_hud_pasada(g, x, y, t, k, rgba(255, 255, 255));
}

int texto_hud_derecha(Graficos& g, int derecha, int y, std::string_view t, u32 contorno = 0) {
    const int x = derecha - 2 * static_cast<int>(t.size());
    texto_hud(g, x, y, t, contorno);
    return x;
}

// Imagen de barra recortada a `w` píxeles (la etiqueta grabada "Lev"/"Hp"/"Mp" es parte del panel 1028); nada en 0.
void barra(Graficos& g, u32 img, int x, int y, int w) {
    RegionAtlas r = g.img(img);
    if (!r.valida() || w <= 0) return;
    r.w = static_cast<u16>(std::min<int>(r.w, w));
    g.gpu().dibujar(r, x, y, clave_orden(CAPA_UI, 2));
}

// Orbe de 0x416160. tm_hour = hora del dword de la hora del juego; grupo = ((tm_hour + 23) % 24) / 2. S_GameTime rearma la transición
// (0x4876c0) siempre que la hora nueva es impar y el minuto es 0; t = ms desde entonces. t < 1350: el último cuadro del grupo anterior
// (primero - 1; 1213 -> 1319, 1316 -> 1245) hasta los 900 ms, después un cuadro cada 150 ms, más los destellos en mezcla screen 1251 + t/150
// en la posición de la tabla del grupo y, para 450 <= t < 1050, 1244 + t/150 sobre el orbe. t >= 1350: el cuadro que avanza; el flag
// se apaga cuando t >= (cuadros * 3 + 18) * 50.
void dibujar_orbe(App& a, int X, int Y) {
    Graficos& g = *a.g;
    const int seg = static_cast<int>(hora_de_juego(a) % 86400);
    const int hora = seg / 3600, minuto = seg / 60 % 60;
    const u64 ahora = a.entrada.ahora_ms;
    if (a.orbe_hora_n != a.mundo.hora_n) {
        a.orbe_hora_n = a.mundo.hora_n;
        if ((hora & 1) && minuto == 0) { a.orbe_anim = true; a.orbe_t0 = ahora; }
    }
    const GrupoOrbe& gr = ORBE_GRUPOS[(hora + 23) % 24 / 2];
    const int ultima = gr.primera + gr.frames - 1, subida = 34 * a.chat_tam, oy = Y + ORBE_Y - subida;
    const u64 k = clave_orden(CAPA_UI, 2), kb = clave_orden(CAPA_UI, 3);
    int img = ultima;
    const u64 t = a.orbe_anim ? ahora - a.orbe_t0 : 0;
    if (a.orbe_anim) {
        if (t < 900) img = gr.primera == 1213 ? 1319 : gr.primera == 1316 ? 1245 : gr.primera - 1;
        else img = std::min<int>(gr.primera + static_cast<int>((t - 900) / 150), ultima);
        if (t >= 1350 && t >= static_cast<u64>(gr.frames * 3 + 18) * 50) a.orbe_anim = false;
    }
    g.gpu().dibujar(g.img(static_cast<u32>(img)), X + ORBE_X, oy, k);
    if (a.orbe_anim && t < 1350) {
        g.gpu().dibujar_en(g.img(1251 + static_cast<u32>(t / 150)), static_cast<float>(X + gr.x), static_cast<float>(Y + gr.y - subida), kb,
                           BLANCO, Mezcla::PANTALLA);
        if (t >= 450 && t < 1050)
            g.gpu().dibujar_en(g.img(1244 + static_cast<u32>(t / 150)), static_cast<float>(X + ORBE_X), static_cast<float>(oy), kb, BLANCO,
                               Mezcla::PANTALLA);
    }
}

void dibujar_panel(App& a) {
    Graficos& g = *a.g;
    const EstadoJugador& j = a.mundo.jugador;
    const Vista vi = vista(a);
    const int X = vi.hx, Y = vi.hy;
    // Diseño original (S3EP1U 0x416160): fondo 1042 en (115,358), alas 1044 un píxel más abajo, adornos 1043 en
    // (121,371), todo dibujado bajo los paneles laterales (1028 en (0,368), 1019 en (485,366)) que tapan sus extremos.
    // Todo sube 34 px por paso de Ctrl+T con el orbe (0x416160: y = ... - 34 * [0x7030dc]).
    const int subida = 34 * a.chat_tam;
    g.gpu().dibujar(g.img(IMG_CHAT), X + 115, Y + 358 - subida, clave_orden(CAPA_UI, 0));
    g.gpu().dibujar(g.img(IMG_CHAT_ALAS), X + 115, Y + 359 - subida, clave_orden(CAPA_UI, 0));
    g.gpu().dibujar(g.img(IMG_CHAT_ADORNO), X + 121, Y + 371 - subida, clave_orden(CAPA_UI, 0));
    g.gpu().dibujar(g.img(IMG_PANEL_IZQ), X, Y + 368, clave_orden(CAPA_UI, 1));
    g.gpu().dibujar(g.img(IMG_PANEL_DER), X + 485, Y + 366, clave_orden(CAPA_UI, 1));
    dibujar_teclas_f(a, X + 485, Y + 366);
    g.gpu().dibujar(g.img(IMG_ENTRADA_CHAT), X + 120, Y + 462, clave_orden(CAPA_UI, 0));  // su extremo izquierdo queda bajo 1028
    {
        // Franja de página de teclas F (S3EP1U 0x4b2260, callback 0x456960): el botón de página arriba 1021 en (489,387) muestra la siguiente de las 3
        // páginas, el dígito 1025 + página en (489,414), el botón de página abajo 1023 en (489,429) muestra la anterior.
        const u64 kb = clave_orden(CAPA_UI, 2);
        g.gpu().dibujar(g.img(1021), X + 489, Y + 387, kb);
        g.gpu().dibujar(g.img(1025 + a.pag_f), X + 489, Y + 414, kb);
        g.gpu().dibujar(g.img(1023), X + 489, Y + 429, kb);
        if (a.entrada.clic && !a.clic_consumido) {
            const RegionAtlas arriba = g.img(1021), abajo = g.img(1023);
            if (ui::sobre(a.entrada, X + 489, Y + 387, arriba.w, arriba.h)) cambiar_pagina_f(a, 1);
            else if (ui::sobre(a.entrada, X + 489, Y + 429, abajo.w, abajo.h)) cambiar_pagina_f(a, -1);
        }
        // Botón "A" de modo de chat (globales 0x6f5cf8/0x6f5cf4 = 136, 464) y botón de scroll 1045 (0x4594d8: 470, 463).
        g.gpu().dibujar(g.img(1045), X + 470, Y + 463, kb);
        g.gpu().dibujar(g.img(1041), X + 136, Y + 464, kb);
    }
    dibujar_orbe(a, X, Y);
    dibujar_botones_panel(a, X, Y);
    // Nombre de la zona, abajo a la izquierda sobre el panel, con el color de su clase (zone3-e.tbl).
    const Slot yo = a.mundo.objetos.buscar(j.id);
    if (const ZonaNombre* z = yo == SIN_SLOT ? nullptr
                              : a.zonas.buscar(a.mundo.mapa, a.mundo.objetos.col<O_X>()[yo], a.mundo.objetos.col<O_Y>()[yo])) {
        const u32 c = a.zonas.color(z->clase);
        // Nombre de la zona (0x4d2410): la letra proporcional del sistema, blanca con contorno de 1 px en el color de la clase de zona, topes de los glifos en
        // y 350. Coincide con la captura del HUD original ("Thebes Desert" se ve tostado claro: trazos blancos finos con borde naranja).
        const u32 borde = rgba(static_cast<u8>(c >> 16), static_cast<u8>(c >> 8), static_cast<u8>(c));
        const float zx = static_cast<float>(X + 16), zy = static_cast<float>(Y + 348);
        for (const auto& [dx, dy] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}})
            g.texto_html(z->nombre, zx + static_cast<float>(dx), zy + static_cast<float>(dy), clave_orden(CAPA_UI_TEXTO, 0), borde);
        g.texto_html(z->nombre, zx, zy, clave_orden(CAPA_UI_TEXTO, 1), BLANCO);
    }
    if (yo != SIN_SLOT) {
        // Tipo de zona, abajo a la derecha: según el atributo de la mitad izquierda de la celda del personaje.
        // Etiqueta de tipo de zona de 0x4d2410 (rama US): las imágenes de la tabla de íconos 1226 segura / 1224 combate / 1225 normal en (530, 345), con
        // el texto incluido (RE_ROUND7 sección 3; verificado dibujándolas).
        const TipoZona tz = tipo_zona_celda(a.mundo, a.rec, a.mundo.objetos.col<O_X>()[yo], a.mundo.objetos.col<O_Y>()[yo]);
        g.gpu().dibujar(g.icono(tz == TipoZona::SEGURA ? 1226 : tz == TipoZona::COMBATE ? 1224 : 1225), X + 530, Y + 345,
                        clave_orden(CAPA_UI_TEXTO, 0));
    }
    char t[48];
    // Nivel y EXP (0x4b0c80): "%2d" en (37,391); "%d.%02d%%" en (80,391) bajo el nivel 40, "%d.%04d%%" en (72,391) desde el 40.
    {
        const bool alto = j.nivel >= 40;
        const i64 escala = alto ? 1000000 : 10000;
        const i64 e = exp_escalada(j.nivel, j.exp, escala);  // porcentaje * 100 (o * 10000)
        const i64 entero = e / (alto ? 10000 : 100), fraccion = e % (alto ? 10000 : 100);
        std::snprintf(t, sizeof t, "%2d", j.nivel);
        texto_hud(g, X + 37, Y + 391, t);
        std::snprintf(t, sizeof t, alto ? "%d.%04d%%" : "%d.%02d%%", static_cast<int>(entero), static_cast<int>(fraccion));
        texto_hud(g, X + (alto ? 72 : 80), Y + 391, t);
        barra(g, IMG_BARRA_EXP, X + 3, Y + 386, static_cast<int>(std::min<i64>(entero * 129 / 100, 129)));
    }
    // Barra de HP: imagen según el estado (veneno 1357, congelado 1358, si no 1030); no se dibuja con HP <= 0 (0x4b1180).
    {
        const u32 img = yo == SIN_SLOT ? IMG_BARRA_HP
                        : (a.mundo.objetos.col<O_FLAGS>()[yo] & FLAG_CONGELADO) ? 1358u
                        : (a.mundo.objetos.col<O_FLAGS>()[yo] & FLAG_VENENO)    ? 1357u
                                                                                 : IMG_BARRA_HP;
        if (j.hp > 0 && j.hp_max > 0) barra(g, img, X + 3, Y + 402, std::min(j.hp * 129 / j.hp_max, 129));
        std::snprintf(t, sizeof t, "%d/%d", j.hp, j.hp_max);
        texto_hud_derecha(g, X + 83, Y + 410, t, rgba(181, 82, 74));
        if (j.mp > 0 && j.mp_max > 0) barra(g, IMG_BARRA_MP, X + 3, Y + 423, std::min(j.mp * 129 / j.mp_max, 129));
        std::snprintf(t, sizeof t, "%d/%d", j.mp, j.mp_max);
        texto_hud_derecha(g, X + 83, Y + 429, t, rgba(99, 132, 165));
    }
    // Cuatro cajas de íconos (0x4b0c10 CA, 0x4b2970 alineación, 0x4b2660 peso, 0x4b2800 comida): números alineados a la derecha en
    // x = 43 / 109 menos 2 px por carácter; peso/comida también tienen un indicador recortado a 43 px.
    std::snprintf(t, sizeof t, "%d", j.ca);
    texto_hud_derecha(g, X + 43, Y + 449, t);
    // Alignment icon 1032 (< 0) / 1033 (0..500) / 1034 (> 500) at (70,446).
    g.gpu().dibujar(g.img(j.legal < 0 ? 1032 : j.legal <= 500 ? 1033 : 1034), X + 70, Y + 446, clave_orden(CAPA_UI, 3));
    std::snprintf(t, sizeof t, "%d", j.legal);
    texto_hud_derecha(g, X + 109, Y + 449, t);
    {
        const int w = std::clamp(j.peso * 43 / 240, 0, 43);
        const int pct = j.peso * 100 / 240;
        RegionAtlas r = g.img(pct < 50 ? 1035 : pct < 83 ? 1036 : 1037);
        if (r.valida() && w > 0) {
            r.w = static_cast<u16>(std::min<int>(r.w, w));
            g.gpu().dibujar(r, X + 22, Y + 464, clave_orden(CAPA_UI, 2));
        }
        std::snprintf(t, sizeof t, "%d%%", pct);
        texto_hud_derecha(g, X + 43, Y + 466, t);
    }
    {
        const int w = std::clamp(j.comida * 43 / 225, 0, 43);
        RegionAtlas r = g.img(j.comida >= 225 ? 1495 : 1038);
        if (r.valida() && w > 0) {
            r.w = static_cast<u16>(std::min<int>(r.w, w));
            g.gpu().dibujar(r, X + 88, Y + 464, clave_orden(CAPA_UI, 2));
        }
        std::snprintf(t, sizeof t, "%d%%", j.comida * 100 / 225);
        texto_hud_derecha(g, X + 109, Y + 466, t);
    }
}

// Barra de texto "[####------]" con la parte llena en `color`; devuelve el ancho dibujado.
int barra_ascii(Graficos& g, float x, float y, float proporcion, u32 color, u64 k) {
    int llenas = static_cast<int>(std::clamp(proporcion, 0.0f, 1.0f) * CELDAS_BARRA + 0.5f);
    char lleno[CELDAS_BARRA + 1] = {}, vacio[CELDAS_BARRA + 1] = {};
    for (int i = 0; i < llenas; ++i) lleno[i] = '#';
    for (int i = 0; i < CELDAS_BARRA - llenas; ++i) vacio[i] = '-';
    int w = g.texto("[", x, y, k, rgba(200, 200, 200));
    w += g.texto(lleno, x + w, y, k, color);
    w += g.texto(vacio, x + w, y, k, rgba(110, 110, 110));
    return w + g.texto("]", x + w, y, k, rgba(200, 200, 200));
}

// UI simple: franja oscura con una línea de estado (nivel/EXP, HP, MP, CA, alineamiento, peso,
// comida) en texto y barras ASCII. El chat va arriba de esta línea (dibujar_chat).
void dibujar_panel_simple(App& a) {
    Graficos& g = *a.g;
    const EstadoJugador& j = a.mundo.jugador;
    const Vista vi = vista(a);
    g.gpu().rectangulo(0, vi.panel_y, vi.w, vi.h - vi.panel_y, clave_orden(CAPA_UI, 0), rgba(0, 0, 0, 170));
    g.gpu().rectangulo(0, vi.panel_y, vi.w, 1, clave_orden(CAPA_UI, 1), rgba(150, 130, 90));
    u64 k = clave_orden(CAPA_UI_TEXTO, 0);
    const u32 rotulo = rgba(214, 190, 140), valor = rgba(255, 255, 254);
    float x = MARGEN_SIMPLE, y = static_cast<float>(vi.h - ALTO_GLIFO - 4);
    char t[48];
    auto campo = [&](const char* r, const char* v) {
        x += g.texto(r, x, y, k, rotulo);
        x += g.texto(v, x, y, k, valor) + SEPARACION_SIMPLE;
    };
    float pexp = porcentaje_exp(j.nivel, j.exp);
    std::snprintf(t, sizeof t, "%d ", j.nivel);
    x += g.texto("Lv ", x, y, k, rotulo);
    x += g.texto(t, x, y, k, valor);
    x += barra_ascii(g, x, y, pexp / 100.0f, rgba(240, 220, 90), k);
    std::snprintf(t, sizeof t, " %.2f%%", pexp);
    x += g.texto(t, x, y, k, valor) + SEPARACION_SIMPLE;
    x += g.texto("HP ", x, y, k, rotulo);
    x += barra_ascii(g, x, y, j.hp_max > 0 ? static_cast<float>(j.hp) / j.hp_max : 0.0f, rgba(230, 70, 60), k);
    std::snprintf(t, sizeof t, " %d/%d", j.hp, j.hp_max);
    x += g.texto(t, x, y, k, valor) + SEPARACION_SIMPLE;
    if (int w = dibujar_indicador_pocion(a, x, y, k)) x += w + SEPARACION_SIMPLE;
    x += g.texto("MP ", x, y, k, rotulo);
    x += barra_ascii(g, x, y, j.mp_max > 0 ? static_cast<float>(j.mp) / j.mp_max : 0.0f, rgba(80, 130, 240), k);
    std::snprintf(t, sizeof t, " %d/%d", j.mp, j.mp_max);
    x += g.texto(t, x, y, k, valor) + SEPARACION_SIMPLE;
    std::snprintf(t, sizeof t, "%d", j.ca);
    campo("AC ", t);
    std::snprintf(t, sizeof t, "%d", j.legal);
    campo("Lawful ", t);
    std::snprintf(t, sizeof t, "%d%%", j.peso * 100 / 240);
    campo("Weight ", t);
    std::snprintf(t, sizeof t, "%d%%", j.comida * 100 / 225);
    campo("Food ", t);
}

// Minimapa en la esquina inferior derecha, del tamaño de la zona de chat. Muestra la pasabilidad de
// lo ya cargado (segmento_cargado no toca disco: lo no explorado queda como fondo) centrada en el
// personaje, más un punto por objeto. cam_x/cam_y es el centro de cámara (= la celda del personaje).
constexpr int MM_CELL = 2;  // px por celda en el minimapa
void dibujar_minimapa(App& a, double cam_x, double cam_y) {
    Graficos& g = *a.g;
    const Vista vi = vista(a);
    // Cuadrado del alto de la zona de chat. UI simple: dentro de la franja, a la derecha de los
    // botones; UI original: sobre el panel, en la esquina.
    const int W = (CHAT_FILAS + 1) * ALTO_GLIFO, H = W;
    const int x0 = vi.w - MARGEN_SIMPLE - W;
    const int y0 = a.cfg.ui_simple ? vi.panel_y + 4 : vi.panel_y - H - 8;
    if (y0 < 0 || x0 < 0) return;
    g.gpu().rectangulo(x0, y0, W, H, clave_orden(CAPA_UI, 3), rgba(10, 12, 10, 205));  // fondo = sin explorar
    g.gpu().rectangulo(x0 - 1, y0 - 1, W + 2, 1, clave_orden(CAPA_UI, 4), rgba(150, 130, 90));
    const int cx = static_cast<int>(std::lround(cam_x)), cy = static_cast<int>(std::lround(cam_y));
    const int cols = W / MM_CELL, rows = H / MM_CELL;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            const int wx = cx + c - cols / 2, wy = cy + r - rows / 2;
            const SegmentoMapa* s = segmento_cargado(a.mundo, a.rec, wx, wy);
            if (!s) continue;
            const int lx = wx - s->base_x, ly = wy - s->base_y;
            const bool bloq = (s->atributo_en(lx, ly, 0) & 1) && (s->atributo_en(lx, ly, 1) & 1);
            g.gpu().rectangulo(x0 + c * MM_CELL, y0 + r * MM_CELL, MM_CELL, MM_CELL, clave_orden(CAPA_UI, 4),
                               bloq ? rgba(55, 50, 45) : rgba(90, 110, 85));
        }
    TablaObjetos& o = a.mundo.objetos;
    for (Slot s = 0; s < o.tamano(); ++s) {
        if (!o.ids()[s]) continue;
        const int dx = static_cast<int>(o.col<O_X>()[s]) - cx, dy = static_cast<int>(o.col<O_Y>()[s]) - cy;
        if (std::abs(dx) > cols / 2 || std::abs(dy) > rows / 2) continue;
        u32 col;
        if (o.ids()[s] == a.mundo.jugador.id)
            col = rgba(255, 255, 255);
        else {
            const u8 t = tipo_objeto(a, s);
            col = t == TIPO_MONSTRUO    ? rgba(230, 70, 60)
                  : t == TIPO_PERSONAJE ? rgba(90, 200, 230)
                  : t == TIPO_ITEM      ? rgba(150, 150, 150)
                                        : rgba(230, 210, 90);
        }
        g.gpu().rectangulo(x0 + (cols / 2 + dx) * MM_CELL - 1, y0 + (rows / 2 + dy) * MM_CELL - 1, 3, 3,
                           clave_orden(CAPA_UI, 5), col);
    }
}

}  // namespace

// Ctrl+P (S3EP1U 0x402680, camino no GM): crea "<dir>Capture", guarda "<dir>Capture\\LinC%04d.bmp" con un contador que
// empieza en 0 e imprime "Saved current screen to <ruta>" en blanco (color [0x500528]). El camino GM (flag [0x8ed548] = 1) es
// silencioso y pide un nombre de archivo; l1j-en nunca activa ese flag, así que no se modela. El <dir> del exe es su propia carpeta;
// aquí es la carpeta de ajustes.
void tomar_captura(App& a) {
    if (a.cfg.ui_simple) {
        a.gpu.pedir_captura("captura.bmp");
        a.chat.agregar("Screenshot saved: captura.bmp", rgba(255, 200, 120), CHAT_SISTEMA);
        return;
    }
    const std::string cfg = ruta_configuracion();
    const std::string dir = cfg.substr(0, cfg.rfind('/') + 1) + "Capture";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    char nombre[32];
    std::snprintf(nombre, sizeof nombre, "/LinC%04u.bmp", a.capturas++);
    const std::string ruta = dir + nombre;
    a.gpu.pedir_captura(ruta);
    a.chat.agregar("Saved current screen to " + ruta, rgba(255, 255, 255), CHAT_SISTEMA);
}



void enviar_chat(App& a, std::string_view linea) {
    if (linea == "/infracon") {  // alterna en el cliente el filtro de infravisión del elfo; no se manda nada
        a.infracon = !a.infracon;
        return;
    }
    if (comando_social(a, linea)) return;
    EntradaChat c;
    if (!interpretar_entrada(linea, c)) return;
    if (c.susurro) enviar(a, cmd::susurrar(a.op, c.destino, c.texto));
    else enviar(a, cmd::chat(a.op, c.tipo, c.texto));
}

void empujar_texto_combate(App& a, const char* t, u8 r, u8 g, u8 b) {
    constexpr u8 N = sizeof a.textos_combate / sizeof a.textos_combate[0];
    if (a.n_textos_combate >= N) {  // descartar el más viejo
        std::memmove(a.textos_combate, a.textos_combate + 1, (N - 1) * sizeof a.textos_combate[0]);
        a.n_textos_combate = N - 1;
    }
    App::TextoCombate& f = a.textos_combate[a.n_textos_combate++];
    std::snprintf(f.t, sizeof f.t, "%s", t);
    f.t0 = a.entrada.ahora_ms;
    f.r = r, f.g = g, f.b = b;
}

namespace {

// Dónde va el chat: registro (x, y, ancho; CHAT_FILAS filas), caja de entrada y zona en la que
// la rueda del mouse lo desplaza.
struct AreaChat {
    int x, y, ancho;
    int entrada_x, entrada_y, entrada_ancho;
    int zona_x, zona_y, zona_w, zona_h;
};
AreaChat area_chat(const App& a) {
    const Vista vi = vista(a);
    if (a.cfg.ui_simple) {
        const int y = vi.panel_y + 4, entrada_y = y + CHAT_FILAS * ALTO_GLIFO;
        // A la derecha: minimapa cuadrado (lado = alto de la zona de chat) + bloque de botones, con
        // un hueco de 8 px a cada lado. El chat ocupa lo que queda a la izquierda.
        const int lado = (CHAT_FILAS + 1) * ALTO_GLIFO;
        const int ancho = std::max(80, vi.w - 2 * MARGEN_SIMPLE - lado - 16 - ancho_botones_texto());
        return {MARGEN_SIMPLE, y, ancho, MARGEN_SIMPLE + 12, entrada_y, ancho - 12,
                0, vi.panel_y, MARGEN_SIMPLE + ancho, vi.h - vi.panel_y};
    }
    const int X = vi.hx, Y = vi.hy;
    const int extra = CHAT_PASO_ORIG * 3 * a.chat_tam;  // Ctrl+T agranda el registro hacia arriba, 3 filas (36 px) por paso (0x421330)
    return {X + CHAT_X, Y + CHAT_Y_ORIG - extra, CHAT_ANCHO, X + ENTRADA_X, Y + ENTRADA_Y, 330, X + 124, Y + 363 - extra, 390, 117 + extra};
}

// Ctrl+O original (0x456dc3): 16 muestras de color (2 filas de 8, paso de 17 px) arriba de la caja de entrada. Un clic escribe el
// código de color "\fX" en la línea del chat (callback 0x434090); Esc la cierra.
void dibujar_paleta_chat(App& a) {
    if (!a.paleta_chat || a.cfg.ui_simple) return;
    constexpr char CODIGOS[16] = {'0', '9', '5', 'R', 'F', 'E', 'B', '4', '>', '7', '3', '=', '2', 'U', '1', 'Z'};
    EntradaFrame& e = a.entrada;
    const Vista vi = vista(a);
    const int x0 = vi.hx + 237, y0 = vi.hy + 424 - 2 * 17 - 6;
    if (e.escape) {
        a.paleta_chat = false;
        e.escape = false;
        return;
    }
    const u64 k = clave_orden(CAPA_UI, 6);
    a.gpu.rectangulo(x0, y0, 8 * 17 + 4, 2 * 17 + 4, k, rgba(20, 18, 15, 235));
    for (int i = 0; i < 16; ++i) {
        const int x = x0 + 4 + (i % 8) * 17, y = y0 + 3 + (i / 8) * 17;
        a.gpu.rectangulo(x, y, 16, 16, k + 1, color_codigo(CODIGOS[i], rgba(255, 255, 255)));
        if (e.clic && ui::sobre(e, x, y, 16, 16) && a.chat_entrada.size() + 3 <= MAX_ENTRADA_CHAT) {
            a.chat_entrada += '\\';
            a.chat_entrada += 'f';
            a.chat_entrada += CODIGOS[i];
            if (a.cfg.chat_con_enter) a.escribiendo_chat = true;
        }
    }
}

// Barra de scroll original del chat (S3EP1U 0x4599ea, ctor 0x47cd80): riel 1047 en (470,401), flecha arriba 974 en (470,393),
// flecha abajo 972 en (471,449), botón 976 (15x16, x=469) sobre el riel de 51 px. El botón queda abajo
// mientras se ve la línea más nueva; las flechas avanzan una línea, el riel y el botón se arrastran, la rueda se maneja en dibujar_chat.
void dibujar_barra_chat(App& a, u32 total, int filas) {
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const Vista vi = vista(a);
    const int X = vi.hx + 470, Y = vi.hy;
    // El tamaño n agranda hacia arriba la imagen del riel (1047 + n) y la flecha arriba, 36 px por paso; la flecha abajo queda fija.
    const int TRACK_Y = 401 - 36 * a.chat_tam, TRACK_H = 51 + 36 * a.chat_tam;
    constexpr int THUMB_H = 16;
    const int LIBRE = TRACK_H - THUMB_H;
    const u32 max = total > static_cast<u32>(filas) ? total - static_cast<u32>(filas) : 0;
    a.chat_desplazamiento = std::min(a.chat_desplazamiento, max);
    const bool sobre_sube = ui::sobre(e, X, Y + 393 - 36 * a.chat_tam, 12, 10), sobre_baja = ui::sobre(e, X + 1, Y + 449, 11, 12);
    if (e.pulsado && sobre_sube && a.chat_desplazamiento < max) ++a.chat_desplazamiento;
    if (e.pulsado && sobre_baja && a.chat_desplazamiento > 0) --a.chat_desplazamiento;
    auto thumb_y = [&] {
        return Y + TRACK_Y + (max ? LIBRE - static_cast<int>(static_cast<i64>(LIBRE) * a.chat_desplazamiento / max) : LIBRE);
    };
    if (e.pulsado && max && ui::sobre(e, X - 1, Y + TRACK_Y, 15, TRACK_H)) {
        const int ty = thumb_y();
        a.chat_barra_agarre = ui::sobre(e, X - 1, ty, 15, THUMB_H) ? e.my - ty : THUMB_H / 2;
        a.clic_consumido = true;
    }
    if (a.chat_barra_agarre >= 0) {
        if (e.presionado && max) {
            const int arriba = std::clamp(e.my - a.chat_barra_agarre - (Y + TRACK_Y), 0, LIBRE);
            a.chat_desplazamiento = static_cast<u32>((static_cast<i64>(LIBRE - arriba) * max + LIBRE / 2) / LIBRE);
        } else {
            a.chat_barra_agarre = -1;
        }
    }
    const u64 kb = clave_orden(CAPA_UI, 2);
    g.gpu().dibujar(g.img(1047 + a.chat_tam), X, Y + TRACK_Y, kb);
    g.gpu().dibujar(g.img(sobre_sube && e.presionado ? 975 : 974), X, Y + 393 - 36 * a.chat_tam, kb);
    g.gpu().dibujar(g.img(sobre_baja && e.presionado ? 973 : 972), X + 1, Y + 449, kb);
    g.gpu().dibujar(g.img(976), X - 1, thumb_y(), kb);
}

// Manejador del mouse de la ventana original del chat (S3EP1U 0x41f680, región 0x4f1740): un doble clic en una fila del chat toma la
// palabra bajo el puntero (0x41f980: letras, dígitos y bytes >= 0x80; un clic sobre un separador toma la palabra que
// empieza justo después). Doble clic izquierdo (WM_LBUTTONDBLCLK) agrega "palabra " a la línea de entrada (0x445010);
// doble clic derecho (WM_RBUTTONDBLCLK) reemplaza la línea por "/exclude palabra" (prefijo, texto 0x5004c0, 0x445090).
// Con la línea cerrada (chat con Enter) la abre con ese texto (callback 0x407e50). El registro no guarda el
// remitente: la palabra se toma del texto de la fila, como hace el original.
bool es_letra_chat(char c) { return std::isalnum(static_cast<unsigned char>(c)) || static_cast<unsigned char>(c) >= 0x80; }

std::string_view palabra_en(std::string_view t, size_t col) {
    size_t ini = col;
    if (col < t.size() && es_letra_chat(t[col])) {
        while (ini > 0 && es_letra_chat(t[ini - 1])) --ini;
    } else {
        ini = col + 1;
    }
    if (ini >= t.size()) return {};
    size_t fin = ini;
    while (fin < t.size() && es_letra_chat(t[fin])) ++fin;
    return t.substr(ini, fin - ini);
}

// Byte de la fila bajo la columna de píxeles px (aquí los glifos no son de ancho fijo, así que se recorren sus anchos).
size_t byte_en_pixel(std::string_view t, int px) {
    size_t i = 0;
    while (i < t.size()) {
        const size_t w = static_cast<u8>(t[i]) >= 0x81 && i + 1 < t.size() ? 2 : 1;
        if (ancho_texto(t.data(), i + w) > px) break;
        i += w;
    }
    return i;
}

void clic_doble_chat(App& a, std::string_view palabra, bool derecho) {
    if (palabra.empty()) return;
    const bool cerrada = a.cfg.chat_con_enter && !a.escribiendo_chat;
    if (derecho || cerrada) a.chat_entrada.clear();
    if (derecho) a.chat_entrada += "/exclude ";
    a.chat_entrada.append(palabra);
    if (!derecho) a.chat_entrada += ' ';
    if (a.chat_entrada.size() > MAX_ENTRADA_CHAT) a.chat_entrada.resize(MAX_ENTRADA_CHAT);
    if (a.cfg.chat_con_enter) a.escribiendo_chat = true;
}

// Registro (las últimas filas, ajustadas al ancho) y caja de entrada, que siempre tiene el foco
// como en el original: se escribe y Enter envía; Esc borra; rueda o Re Pág/Av Pág desplazan.
void dibujar_chat(App& a) {
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const AreaChat ac = area_chat(a);
    u32 n = a.chat.cantidad();
    const int filas = CHAT_FILAS + (a.cfg.ui_simple ? 0 : 3 * a.chat_tam);
    if (!ui::sobre(e, ac.zona_x, ac.zona_y, ac.zona_w, ac.zona_h)) rueda_teclas_f(a);
    int paso = (ui::sobre(e, ac.zona_x, ac.zona_y, ac.zona_w, ac.zona_h) ? e.rueda : 0) + (e.pagina_arriba ? filas : 0) -
               (e.pagina_abajo ? filas : 0);
    i64 d = static_cast<i64>(a.chat_desplazamiento) + paso;
    a.chat_desplazamiento = static_cast<u32>(std::clamp<i64>(d, 0, n > static_cast<u32>(filas) ? n - static_cast<u32>(filas) : 0));

    if (!a.cfg.ui_simple) dibujar_barra_chat(a, n, filas);
    u64 kt = clave_orden(CAPA_UI_TEXTO, 0);
    const int pitch = a.cfg.ui_simple ? ALTO_GLIFO : CHAT_PASO_ORIG;
    std::string_view visibles[CHAT_FILAS + 9] = {};  // texto de cada fila dibujada, para el doble clic
    int fila = filas - 1;
    for (u32 i = a.chat_desplazamiento; i < n && fila >= 0; ++i) {
        const LineaChat& l = a.chat.reciente(i);
        std::string_view t = a.chat.texto(l);
        std::string_view partes[8];
        int k = 0;
        while (!t.empty() && k < 8) {
            // Un grito empieza con su ícono de dirección de 12 px (el código "\d<n>" del exe), que ocupa lugar en la primera fila.
            size_t c = bytes_que_caben(t.data(), t.size(), ac.ancho - (k == 0 && l.dir >= 0 ? 12 : 0));
            partes[k++] = t.substr(0, c);
            t.remove_prefix(c);
            while (!t.empty() && t[0] == ' ') t.remove_prefix(1);
        }
        // Los códigos de color \fX (y los ocultos \b \i \k \j \c) se interpretan como en el dibujo de texto del exe (0x4334f0):
        // el color pasa a las filas ajustadas que siguen.
        const bool doble = !es_utf8(l.texto, l.largo);
        u32 color_fila[8];
        u32 color = l.color;
        for (int p = 0; p < k; ++p) {
            color_fila[p] = color;
            recorrer_codigos(partes[p], color, doble, [&](std::string_view, u32 c) { color = c; });
            // Un código justo al final de una fila igual cambia el color de la siguiente.
            for (size_t j = 0; j + 2 < partes[p].size(); ++j)
                if (partes[p][j] == '\\' && partes[p][j + 1] == 'f' && j + 3 == partes[p].size() && es_codigo_color(partes[p][j + 2]))
                    color = color_codigo(partes[p][j + 2], l.color);
        }
        for (int p = k - 1; p >= 0 && fila >= 0; --p, --fila) {
            const bool icono = p == 0 && l.dir >= 0;
            if (icono) g.gpu().dibujar(g.img(605 + static_cast<u32>(l.dir)), ac.x, ac.y + fila * pitch, kt);
            float x = static_cast<float>(ac.x + (icono ? 12 : 0));
            recorrer_codigos(partes[p], color_fila[p], doble, [&](std::string_view tramo, u32 c) {
                x += g.texto_sombra(tramo, x, static_cast<float>(ac.y + fila * pitch), kt, c);
            });
            visibles[fila] = partes[p];
        }
    }
    if ((e.doble_izquierdo || e.doble_derecho) && e.mx >= ac.x && e.mx < ac.x + ac.ancho && e.my >= ac.y - 4) {
        const int r = std::max(0, e.my - ac.y) / pitch;
        if (r < filas) {
            const std::string_view t = visibles[r];
            clic_doble_chat(a, palabra_en(t, byte_en_pixel(t, e.mx - ac.x)), e.doble_derecho);
        }
    }
    if (a.chat_desplazamiento > 0) g.texto_sombra("^", ac.x + ac.ancho - 6, ac.y, kt, rgba(255, 220, 120));
    dibujar_paleta_chat(a);

    // Línea de entrada. Con "chat con Enter" está cerrada hasta apretar Enter (así las teclas son
    // acciones); Enter envía y la cierra, Esc la vacía y la cierra (y no llega a las ventanas).
    const bool con_enter = a.cfg.chat_con_enter;
    if (con_enter && !a.escribiendo_chat && e.enter && !escribiendo_en_ventana(a)) {
        a.escribiendo_chat = true;
        e.enter = false;
    }
    const bool abierta = !con_enter || a.escribiendo_chat;
    if (a.cfg.ui_simple) g.texto(">", ac.entrada_x - 12, ac.entrada_y, kt, abierta ? rgba(214, 190, 140) : rgba(110, 100, 80));
    if (a.chat_prompt.activo && !a.escribiendo_chat) a.escribiendo_chat = true;  // the input box takes the line (0x4451d0)
    if (abierta && e.escape && (con_enter || !a.chat_entrada.empty() || a.chat_prompt.activo)) {  // sin nada que borrar, Esc sigue a las ventanas
        if (a.chat_prompt.activo) cancelar_prompt(a);
        a.chat_entrada.clear();
        if (con_enter) a.escribiendo_chat = false;
        e.escape = false;
    }
    if (!abierta) {
        g.texto("Enter: chat", ac.entrada_x, ac.entrada_y, kt, rgba(110, 100, 80));
    } else if (ui::caja_texto(g, e, a.chat_entrada, ac.entrada_x, ac.entrada_y, ac.entrada_ancho, true, false, MAX_ENTRADA_CHAT)) {
        if (a.chat_prompt.activo) confirmar_prompt(a, a.chat_entrada);
        else if (!a.chat_entrada.empty()) enviar_chat(a, a.chat_entrada);
        a.chat_entrada.clear();
        a.chat_desplazamiento = 0;
        if (con_enter) a.escribiendo_chat = false;
    }
}

void dibujar_estado(App& a) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    // PENDIENTE: no se identificó la ventana de estado original; se usa el marco de ui::ventana.
    arrastrar_ventana(a, IdVentana::ESTADO);
    const Rect rv = rect_ventana(a, IdVentana::ESTADO);
    const int ESTADO_X = rv.x, ESTADO_Y = rv.y;
    if (uses_original_frame(a, IdVentana::ESTADO)) return draw_original_status(a, ESTADO_X, ESTADO_Y);
    if (ui::ventana(g, e, a.cfg.ui_simple, ESTADO_X, ESTADO_Y, "Status", opacidad_ventanas(a))) a.ventana_estado = false;
    // Dos pestañas, como el original: los atributos y el equipo.
    if (ui::boton_texto(g, e, "Status", ESTADO_X + 20, ESTADO_Y + ui::VENTANA_ALTO - 24, 96, 18)) a.pestana_estado = 0;
    if (ui::boton_texto(g, e, "Equipment", ESTADO_X + 128, ESTADO_Y + ui::VENTANA_ALTO - 24, 96, 18)) a.pestana_estado = 1;
    if (a.pestana_estado == 1) return dibujar_equipo(a, ESTADO_X, ESTADO_Y);

    const EstadoJugador& j = a.mundo.jugador;
    Slot s = a.mundo.objetos.buscar(j.id);
    std::string_view nombre = s != SIN_SLOT ? texto(a.mundo, a.mundo.objetos.col<O_NOMBRE>()[s]) : std::string_view{};
    int clase = (a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size())) ? a.pjs[a.pj_sel].v[PJ_CLASE] : -1;

    u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    u32 rotulo = rgba(214, 190, 140), valor = rgba(255, 255, 254);
    int x = ESTADO_X + 20, y = ESTADO_Y + 34;
    g.texto_sombra(nombre, x, y, kt, rgba(255, 230, 160));
    g.texto_sombra(clase >= 0 ? nombre_clase(clase) : "", x + 120, y, kt, valor);
    y += 20;
    char t[48];
    auto par = [&](int col, const char* r, const char* v) {
        g.texto(r, x + col, y, kt, rotulo);
        g.texto(v, x + col + 48, y, kt, valor);
    };
    auto fila_num = [&](const char* r1, int v1, const char* r2, int v2) {
        char a1[16], a2[16];
        std::snprintf(a1, sizeof a1, "%d", v1);
        std::snprintf(a2, sizeof a2, "%d", v2);
        par(0, r1, a1);
        par(110, r2, a2);
        y += 15;
    };
    std::snprintf(t, sizeof t, "%.2f%%", porcentaje_exp(j.nivel, j.exp));
    std::snprintf(t + 24, sizeof t - 24, "%d", j.nivel);
    par(0, "Level", t + 24);
    par(110, "Exp", t);
    y += 15;
    std::snprintf(t, sizeof t, "%d/%d", j.hp, j.hp_max);
    par(0, "HP", t);
    y += 15;
    std::snprintf(t, sizeof t, "%d/%d", j.mp, j.mp_max);
    par(0, "MP", t);
    y += 21;
    fila_num("STR", j.fue, "CON", j.con);
    fila_num("DEX", j.des, "INT", j.int_);
    fila_num("WIS", j.sab, "CHA", j.car);
    y += 6;
    fila_num("AC", j.ca, "MR", j.mr);
    fila_num("SP", j.sp, "Lawful", j.legal);
    y += 6;
    fila_num("Fire", j.fuego, "Water", j.agua);
    fila_num("Wind", j.viento, "Earth", j.tierra);
    y += 6;
    std::snprintf(t, sizeof t, "%d%%", j.peso * 100 / 240);
    std::snprintf(t + 24, sizeof t - 24, "%d%%", j.comida * 100 / 225);
    par(0, "Weight", t);
    par(110, "Food", t + 24);
}

// Ctrl/Cmd + y - cambian la escala (menos escala = más mapa); Ctrl/Cmd 0 vuelve a la automática
// (la mayor con la que entran 640×480, como el original). Se guarda en settings.ini y se aplica
// desde el frame siguiente (Gpu::ajustar_vista al comienzo del frame).
void cambiar_zoom(App& a) {
    int z = a.entrada.zoom;
    // Con la vista fija el zoom es solo del mapa, en las dos interfaces: la grilla queda en 640×480.
    if (z == 0) return;
    int maxima = a.gpu.vista_fija() ? a.gpu.escala() : a.gpu.escala_maxima();
    int actual = a.cfg.zoom > 0 ? std::min(a.cfg.zoom, maxima) : maxima;
    int nuevo = z == 2 ? 0 : std::clamp(actual + z, 1, maxima);
    if (nuevo == maxima) nuevo = 0;  // la máxima es la automática: sigue a la ventana
    if (nuevo == a.cfg.zoom) return;
    a.cfg.zoom = nuevo;
    guardar_config(a.cfg);
}

// Aviso de muerte con el botón para reiniciar (C_Restart). PENDIENTE: el diálogo original.
// Las ventanas, de atrás hacia adelante, cada una en su nivel (Gpu::nivel_ventana). Las que no
// son la de más arriba bajo el puntero no lo ven, así sus botones no reciben el clic de otra;
// salvo mientras se arrastra algo: soltarlo lo resuelve la ventana de donde salió.

void dibujar_ventanas(App& a) {
    EntradaFrame& e = a.entrada;
    const int mx = e.mx, my = e.my;
    const bool arrastre = a.arrastrando || a.arrastrando_hechizo || a.arrastrando_atajo >= 0 || a.ventana_arrastrada >= 0;
    // Esc cierra todas las ventanas. NPC y Social ya lo hacen al dibujarse (los ajustes son modales: app.cpp); los diálogos, el
    // chat y un hechizo o mascota eligiendo objetivo se lo quedan antes. Con un pergamino esperando su
    // objetivo, Esc solo cancela eso (inventario_ui.cpp). Arrastrando algo no cierra: soltarlo lo
    // resuelve la ventana de donde salió. El intercambio tampoco: dejaría el trade abierto.
    if (e.escape && !a.usar_sobre && !arrastre) {
        // Original: Esc sin nada abierto muestra el menú de salida (0x457352).
        const bool abierta_alguna = alguna_ventana_abierta(a);
        close_listed_windows(a);
        if (!a.cfg.ui_simple && !abierta_alguna) a.ventana_salida = true;
    }
    for (int i = 0; i < static_cast<int>(IdVentana::CANTIDAD); ++i) {
        const IdVentana v = static_cast<IdVentana>(a.orden_ventanas[i]);
        if (!arrastre && a.ventana_arriba >= 0 && a.ventana_arriba != static_cast<int>(v)) e.mx = e.my = -10000;
        a.gpu.nivel_ventana(static_cast<u32>(i + 1));
        a.ventana_sobre_otra = tapa_a_otra(a, v);
        switch (v) {
        case IdVentana::ESTADO: if (a.ventana_estado) dibujar_estado(a); break;
        case IdVentana::INVENTARIO: if (a.ventana_inventario) dibujar_inventario(a); break;
        case IdVentana::HECHIZOS: if (a.ventana_hechizos) dibujar_hechizos(a); break;
        case IdVentana::NPC: dibujar_npc(a); break;
        case IdVentana::SALIDA: dibujar_salida(a); break;
        case IdVentana::SOCIAL: dibujar_social(a); break;
        case IdVentana::INTERCAMBIO: dibujar_intercambio(a); break;
        case IdVentana::VOLUMEN: dibujar_volumen(a); break;
        case IdVentana::HPMP: dibujar_hpmp_flotante(a); break;
        case IdVentana::TIENDA_PRIV: dibujar_tienda_original(a); break;
        case IdVentana::LISTA: dibujar_lista_orig(a); break;
        case IdVentana::MASCOTA: dibujar_mascota_orig(a); break;
        case IdVentana::DESTINO: dibujar_destino_orig(a); break;
        case IdVentana::CARTEL: dibujar_cartel(a); break;
        case IdVentana::COCINA: dibujar_cocina(a); break;
        default: break;
        }
        e.mx = mx;
        e.my = my;
    }
    a.ventana_sobre_otra = false;
    a.gpu.nivel_ventana(0);
}

// Ceguera y el aviso de parálisis, sueño, congelamiento... (arriba al centro del mundo).
void dibujar_estados_alterados(App& a, int w, int h) {
    const EstadoJugador& j = a.mundo.jugador;
    if (j.ceguera && a.cfg.ui_simple) {  // el velo va en el mundo: con zoom solo del mapa, en sus píxeles lógicos
        const float z = a.gpu.zoom_mundo();
        a.g->gpu().rectangulo(0, 0, static_cast<int>(std::ceil(w / z)), static_cast<int>(std::ceil(h / z)), clave_orden(CAPA_EFECTOS, PROF_CEGUERA), rgba(0, 0, 0, j.ceguera == 2 ? 200 : 245));
    }
    const char* t = nullptr;
    switch (j.inmovil) {
    case PARALISIS: case PARALISIS_2: t = "Paralyzed"; break;
    case SUENO: t = "Asleep"; break;
    case CONGELADO: t = "Frozen"; break;
    case ATURDIDO: t = "Stunned"; break;
    case ATADO: t = "Bound"; break;
    default: break;
    }
    if (a.modo_pk) {  // recordatorio del modo PK, arriba a la derecha
        a.g->gpu().rectangulo(w - 40, 40, 32, ALTO_GLIFO + 8, clave_orden(CAPA_UI, 20), rgba(110, 20, 20, 220));
        a.g->texto_sombra("PK", static_cast<float>(w - 32), 44.0f, clave_orden(CAPA_UI_TEXTO, 20), rgba(255, 210, 200));
    }
    if (!t) return;
    const int tw = ancho_texto(t, std::strlen(t));
    a.g->gpu().rectangulo(w / 2 - tw / 2 - 8, 40, tw + 16, ALTO_GLIFO + 8, clave_orden(CAPA_UI, 20), rgba(60, 20, 20, 210));
    a.g->texto_sombra(t, static_cast<float>(w / 2 - tw / 2), 44.0f, clave_orden(CAPA_UI_TEXTO, 20), rgba(255, 200, 160));
}

void dibujar_muerte(App& a) {
    if (!a.mundo.jugador.muerto) {
        a.t_muerte = 0;  // también si lo resucitaron (S_Resurrection)
        return;
    }
    if (!a.cfg.ui_simple) {  // original: sin diálogo de muerte, aparece el menú de Esc (0x457620); su botón 1 reinicia
        if (a.t_muerte == 0) {
            a.t_muerte = a.entrada.ahora_ms;
            a.ventana_salida = true;
        }
        return;
    }
    Graficos& g = *a.g;
    const Vista vi = vista(a);
    const int w = 200, h = 64, x = vi.w / 2 - w / 2, y = vi.panel_y / 2 - h / 2;
    g.gpu().rectangulo(x, y, w, h, clave_orden(CAPA_UI, 20), rgba(10, 8, 6, 235));
    g.gpu().rectangulo(x, y, w, 1, clave_orden(CAPA_UI, 21), rgba(150, 130, 90));
    g.gpu().rectangulo(x, y + h - 1, w, 1, clave_orden(CAPA_UI, 21), rgba(150, 130, 90));
    const char* t = "You have died.";
    g.texto(t, x + (w - ancho_texto(t, std::strlen(t))) / 2, y + 12, clave_orden(CAPA_UI_TEXTO, 20), rgba(255, 120, 100));
    bool pedir = ui::boton_texto(g, a.entrada, "Restart", x + w / 2 - 40, y + 34, 80, 18);
    if (a.t_muerte == 0) a.t_muerte = a.entrada.ahora_ms;
    if (pedir) reiniciar(a);
}

// Modo espectador (flag fantasma del propio personaje): botón para volver (C_ExitGhost), arriba al centro.
void dibujar_espectador(App& a) {
    const Slot s = a.mundo.objetos.buscar(a.mundo.jugador.id);
    if (s == SIN_SLOT || !(a.mundo.objetos.col<O_FLAGS>()[s] & FLAG_FANTASMA)) return;
    const Vista vi = vista(a);
    if (ui::boton_texto(*a.g, a.entrada, "Leave spectator mode", vi.w / 2 - 80, 70, 160, 20)) enviar(a, Escritor(a.op, Cli::EXITGHOST));
}

void dibujar_depuracion(App& a) {
    Graficos& g = *a.g;
    Slot s = a.mundo.objetos.buscar(a.mundo.jugador.id);
    char t[200];
    std::snprintf(t, sizeof t, "%.0f fps  cpu %.2f ms (max %.2f, decod %.2f)  asign %llu  %u inst  %u draw  %zu obj  %u pag  %dx%d x%d  mapa %u (%u,%u)",
                  a.fps, a.ms_cpu, a.ms_cpu_max, a.ms_decod, static_cast<unsigned long long>(a.asign_frame_max), a.gpu.instancias_ultimo_frame(), a.gpu.llamadas_ultimo_frame(), a.mundo.objetos.tamano(),
                  a.gpu.paginas_usadas(), a.gpu.ancho(), a.gpu.alto(), a.gpu.escala(), a.mundo.mapa, s != SIN_SLOT ? a.mundo.objetos.col<O_X>()[s] : 0,
                  s != SIN_SLOT ? a.mundo.objetos.col<O_Y>()[s] : 0);
    g.texto_sombra(t, 4, 4, clave_orden(CAPA_UI_TEXTO, 0), rgba(180, 255, 180));
}

// Contador de Shift+F3: el tiempo es el del frame completo; F3 da además el de la CPU.
void dibujar_fps(App& a) {
    if (a.fps <= 0) return;  // todavía no pasó el primer segundo de medición
    char t[32];
    const int n = std::snprintf(t, sizeof t, "%.0f fps  %.1f ms", a.fps, 1000.0f / a.fps);
    a.g->texto_sombra(t, static_cast<float>(a.gpu.ancho() - ancho_texto(t, static_cast<size_t>(n)) - 6), 4,
                      clave_orden(CAPA_UI_TEXTO, 0), rgba(180, 255, 180));
}

// Cursor: círculo si un hechizo espera su objetivo; espada sobre un monstruo (o con Ctrl sobre otro
// personaje o NPC, que es atacarlo); si no, la mano (también sobre la interfaz).
// Uso pendiente de "Select a target." (0x469f1f): el setter del exe 0x402490 guarda el uso en [0x81654c] y su alcance en [0x816548], que es
// 15 para todos los que lo llaman salvo el reintento de hechizo fuera de alcance; el objetivo (objeto bajo el puntero, si no la celda) está en alcance si
// |dx| <= alcance y |dy| <= alcance (el exe compara la x duplicada contra 2 * alcance).
constexpr int PENDING_USE_RANGE = 15;
bool pending_target_in_range(App& a, ObjId id) {
    const TablaObjetos& o = a.mundo.objetos;
    const Slot me = o.buscar(a.mundo.jugador.id);
    if (me == SIN_SLOT) return false;
    const int ox = o.col<O_X>()[me], oy = o.col<O_Y>()[me];
    int tx, ty;
    const Slot s = id ? o.buscar(id) : SIN_SLOT;
    if (s != SIN_SLOT) {
        tx = o.col<O_X>()[s];
        ty = o.col<O_Y>()[s];
    } else {  // sin objeto: la celda bajo el puntero (inversa de mundo_px / mundo_py desde el centro del personaje)
        const Vista vi = vista_mundo(a);
        const double rx = (raton_mundo_x(a) - vi.cx) / 24.0, ry = (raton_mundo_y(a) - vi.cy) / 12.0;
        tx = ox + static_cast<int>(std::lround((rx - ry) / 2));
        ty = oy + static_cast<int>(std::lround((rx + ry) / 2));
    }
    return std::abs(tx - ox) <= PENDING_USE_RANGE && std::abs(ty - oy) <= PENDING_USE_RANGE;
}

// Selector de la interfaz original (0x469e40, docs/fork/RE_HUD.md): elige uno de los 16 estados de cursor. El fork no tiene
// flags de tile, datos de tipo 7 / 13, atajo de hechizo elegido ni capa de acción "ocupado", así que los estados 14, 16 y la variante
// de atajo del 8 nunca se eligen (RE_ROUND6).
void elegir_cursor_original(App& a, u64 ahora) {
    // El setter del exe 0x477c90 es persistente: un estado queda hasta que otro manejador fija otro. `seguir` conserva el último.
    auto poner = [&](int estado) {
        a.cursor_estado_orig = estado;
        if (a.cursores[static_cast<int>(cursor_estado(estado))]) a.cursor = cursor_estado(estado);
    };
    auto seguir = [&] { poner(a.cursor_estado_orig); };
    const int html = a.cursor_html;
    a.cursor_html = 0;  // la página lo vuelve a fijar mientras se dibuja
    const bool alt = (SDL_GetModState() & SDL_KMOD_ALT) != 0;  // [0x702dd5]; Ctrl es [0x702dd6] (0x456bb0)
    const bool ataque = a.modo_ataque, pk = a.modo_pk;  // Ctrl+X [0x703105], la opción "pk" [0x6f1dac]
    const Vista vi = vista(a);
    const bool sobre_ventana = sobre_alguna_ventana(a);
    const bool sobre_hud = !sobre_ventana && (a.entrada.my >= vi.panel_y || sobre_barra(a));
    if (a.gm_apuntando) return poner(11);
    // Arrastrar un objeto del inventario pasado el umbral de arrastre (grilla 0x44f306 / 0x44f48b): 5 en todos lados; en el mundo 13 sobre un
    // objeto al que puede ir (0x469e74: 0x4cdfc0, personaje / NPC / monstruo vivo), si no 5.
    if (a.arrastrando) {
        if (sobre_ventana || sobre_hud) return poner(5);
        const ObjId id = objeto_bajo_puntero(a, false, false);
        const Slot s = id ? a.mundo.objetos.buscar(id) : SIN_SLOT;
        const u8 c = s != SIN_SLOT ? clase_objeto(a, s) : 0;
        return poner(c == 5 || c == 6 || c == 10 || c == 12 ? 13 : 5);
    }
    // Las ventanas toman el mouse antes que el mundo (despacho 0x4576e0). La página del NPC: 1 sobre texto, 3 sobre un enlace (0x436f8a /
    // 0x436f9f); botones, grillas, filas de lista, campos de edición y barras de scroll: 3 (0x4133e3, 0x45208a, 0x4be1a9, 0x4c3d9f, 0x4223c3).
    // Una grilla de inventario con callback de elección (un pergamino que espera el objeto sobre el que usarse, 0x44f3a5): 9 encima.
    if (sobre_ventana && a.usar_sobre && lin::sobre_ventana(a, IdVentana::INVENTARIO)) return poner(9);
    if (sobre_ventana) return poner(html ? html : 3);
    if (sobre_hud) {
        // Panel inferior: el registro del chat 1 (0x41f8ef); sus botones, casillas rápidas y línea de edición 3 (0x41c02a, 0x4b1cd8, 0x4c3d9f).
        const AreaChat ac = area_chat(a);
        if (ui::sobre(a.entrada, ac.zona_x, ac.zona_y, ac.zona_w, ac.zona_h)) return poner(1);
        return poner(3);
    }
    // Un portal del mapa en la celda bajo el puntero (flag de tile 2, fijado desde los registros de portal del .s32 en 0x460ddc) sin objeto
    // ahí y sin nada pendiente: 14, antes del selector de objetos (0x469792).
    const bool pendiente = a.hechizo_apuntando || a.mascota_eligiendo || a.item_apuntando;
    auto portal_bajo_puntero = [&] {
        const TablaObjetos& o = a.mundo.objetos;
        const Slot me = o.buscar(a.mundo.jugador.id);
        if (me == SIN_SLOT) return false;
        const Vista vm = vista_mundo(a);
        const double rx = (raton_mundo_x(a) - vm.cx) / 24.0, ry = (raton_mundo_y(a) - vm.cy) / 12.0;
        const int tx = o.col<O_X>()[me] + static_cast<int>(std::lround((rx - ry) / 2));
        const int ty = o.col<O_Y>()[me] + static_cast<int>(std::lround((rx + ry) / 2));
        return es_portal(a.mundo, a.rec, tx, ty);
    };
    if (!pendiente && !objeto_bajo_puntero(a, false, true) && portal_bajo_puntero()) return poner(14);
    if (a.pesca.activa) return poner(16);  // la caña de pescar mientras el corcho está afuera (0x469e53)
    if (pendiente) {
        // Objetivo en alcance: el círculo de objetivo (8), o 15 con Ctrl o la opción pk (0x469f85); fuera de alcance: 11 (0x46a344).
        if (!pending_target_in_range(a, objeto_bajo_puntero(a))) return poner(11);
        return poner(a.entrada.ctrl || pk ? 15 : 8);
    }
    const ObjId id = objeto_bajo_puntero(a, false, true);
    const Slot s = id ? a.mundo.objetos.buscar(id) : SIN_SLOT;
    // Ctrl mantenido, con o sin objeto (0x469fb1): Alt 7, modo ataque 15, si no 6.
    if (a.entrada.ctrl) return poner(alt ? 7 : ataque ? 15 : 6);
    if (s == SIN_SLOT) return poner(3);
    if (vendedor(a, id)) return poner(4);  // una tienda privada (byte del objeto +4, 0x46a060): 4
    const TablaObjetos& o = a.mundo.objetos;
    const bool yo = id == a.mundo.jugador.id;
    switch (clase_objeto(a, s)) {  // tabla de saltos 0x46a358
    case 2: case 3: return poner(10);                   // cadáver: 10 (8 en modo ataque con un arma de hechizo, sin modelar)
    case 4: case 14: case 15: return poner(9);
    case 7: case 13: return poner(14);
    case 6: case 12: return poner(ataque && pk ? 15 : 4);  // NPC / mascota propia: labios; 15 necesita modo ataque y pk (0x46a1ed)
    case TIPO_MONSTRUO:                                 // 0x46a09f: Alt 7; modo ataque: pk 15, si no 8; si no la espada 6
        if (yo) return poner(3);
        return poner(alt ? 7 : ataque ? (pk ? 15 : 8) : 6);
    case 5: case 11: {                                  // personaje o mascota de otro (0x46a27a)
        if (yo) return poner(ataque ? (pk ? 15 : 8) : 3);
        // Uno rosa (marcado PK, +0x50) es un objetivo: Alt 7, modo ataque 8 / 15, si no la espada 6.
        if (ahora < o.col<O_ROSA>()[s]) return poner(alt ? 7 : ataque ? (pk ? 15 : 8) : 6);
        if (ataque) return poner(pk ? 15 : 8);
        if (pk) return poner(15);
        // Ninguno de los dos: sin setter; unos labios / espada que quedaron de antes pasan a 3 (0x46a31e).
        if (a.cursor_estado_orig == 4 || a.cursor_estado_orig == 6) return poner(3);
        return seguir();
    }
    default: return poner(3);                           // objetos 9, puertas 8 (el caso por flag de tile no se modela), el resto
    }
}

void elegir_cursor(App& a, u64 ahora) {
    if (!a.cfg.ui_simple) return elegir_cursor_original(a, ahora);
    if (a.hechizo_apuntando || a.mascota_eligiendo || a.item_apuntando || a.gm_apuntando) {
        a.cursor = Cursor::OBJETIVO;
        return;
    }
    const Vista vi = vista(a);
    if (a.entrada.my >= vi.panel_y || sobre_alguna_ventana(a) || sobre_barra(a)) return;
    ObjId id = objeto_bajo_puntero(a);
    Slot s = id ? a.mundo.objetos.buscar(id) : SIN_SLOT;
    if (s == SIN_SLOT) return;
    // Tienda de un jugador: el clic camina hasta él y pide su lista. Con Ctrl, el clic lo ataca.
    if (vendedor(a, id) && !a.entrada.ctrl) {
        a.cursor = Cursor::MONEDA;
        return;
    }
    u8 tipo = tipo_objeto(a, s);
    const bool propia = mascota_propia(a, s) && !a.entrada.ctrl;  // sin Ctrl, la propia le habla (no se ataca)
    if ((tipo == TIPO_MONSTRUO && !mascota_sin_ctrl(a, s) && !propia) || (a.entrada.ctrl && tipo != TIPO_ITEM) ||
        (a.modo_pk && tipo == TIPO_PERSONAJE))
        a.cursor = Cursor::ESPADA;
    else if ((es_npc(a, s) || propia) && tipo != TIPO_PUERTA && !mascota_sin_ctrl(a, s))  // sin Ctrl, el clic le habla
        a.cursor = Cursor::LABIOS;
}

}  // namespace

void reiniciar(App& a) {
    enviar(a, cmd::reiniciar(a.op));
    reiniciar_jugador(a.mundo);
    a.objetivo = 0;
    a.hablar_objetivo = false;
    a.fin_ataque = 0;
    a.t_muerte = 0;
}

// Objeto vivo bajo el puntero (el de adelante si hay varios), con los rectángulos del frame
// anterior.
ObjId objeto_bajo_puntero(const App& a, bool incluir_propio, bool incluir_muertos) {
    const float mx = raton_mundo_x(a), my = raton_mundo_y(a);  // los rectángulos están en el mundo
    const TablaObjetos& o = a.mundo.objetos;
    const App::ObjetoEnPantalla* mejor = nullptr;
    int mejor_clase = -1;
    for (const App::ObjetoEnPantalla& r : a.en_pantalla) {
        if ((r.id == a.mundo.jugador.id && !incluir_propio) || mx < r.x0 || mx >= r.x1 || my < r.y0 || my >= r.y1) continue;
        const Slot s = o.buscar(r.id);
        if (s == SIN_SLOT || (!incluir_muertos && o.col<O_ESTADO>()[s] == ESTADO_MUERTO)) continue;
        // Interfaz original (0x469d90 ordena los aciertos con 0x46a450): el personaje propio al final, después la clase de objeto más alta
        // (un monstruo sobre un NPC sobre un personaje sobre un cadáver); la interfaz simple toma el de más adelante.
        const int clase = a.cfg.ui_simple ? 0 : r.id == a.mundo.jugador.id ? -2 : clase_objeto(a, s);
        if (!mejor || clase > mejor_clase || (clase == mejor_clase && r.profundidad > mejor->profundidad)) {
            mejor = &r;
            mejor_clase = clase;
        }
    }
    return mejor ? mejor->id : 0;
}

// Dirección (0–7) del paso de (dx, dy) con componentes recortadas a -1..1; -1 si es (0, 0).
int direccion_de(int dx, int dy) {
    dx = (dx > 0) - (dx < 0);
    dy = (dy > 0) - (dy < 0);
    for (int d = 0; d < 8; ++d)
        if (DIR_DX[d] == dx && DIR_DY[d] == dy) return d;
    return -1;
}


void dibujar_captura(App& a) {
    // Render no interactivo (LIN_RENDER): dibuja el mapa centrado en la celda pedida y, pasados unos
    // frames (para que el atlas transmita los tiles, como el explorador), guarda el PNG y sale.
    const Vista vi = vista_mundo(a);
    const double cam_x = a.render.x, cam_y = a.render.y;
    segmento(a.mundo, a.rec, a.render.x, a.render.y);  // fuerza el segmento central (síncrono) ya en el 1er frame
    const double ox = vi.cx - 24 - mundo_px(cam_x, cam_y);
    const double oy = vi.cy - 12 - mundo_py(cam_x, cam_y);
    actualizar_segmentos(a.mundo, a.render.x, a.render.y, radio_segmentos(vi));
    dibujar_suelo(a, cam_x, cam_y, ox, oy);
    dibujar_objetos_mapa(a, cam_x, cam_y, ox, oy);
    if (a.frames_en_pantalla >= 45) {
        a.gpu.pedir_captura(a.render.salida);
        a.salir = true;
    }
}

// Atajos de GM originales Ctrl+Y / Ctrl+Z (0x409430 / 0x409380): "Select a target." (texto 179), después el id del objeto
// cliqueado sale con el opcode 83 (cd). Ctrl+Z lo reenvía cada 1,5 s, 20 veces más (temporizador 0x409320).
void procesar_gm_apuntado(App& a, u64 ahora) {
    EntradaFrame& e = a.entrada;
    if (a.gm_apuntando) {
        if (!a.es_gm || e.clic_derecho || e.escape) {
            a.gm_apuntando = 0;
            e.escape = false;
        } else if (e.pulsado && !sobre_alguna_ventana(a) && !sobre_barra(a)) {
            a.clic_consumido = true;
            const ObjId obj = objeto_bajo_puntero(a, true);
            if (obj && a.mundo.objetos.buscar(obj) != SIN_SLOT) {
                enviar(a, cmd::inventario_mascota(a.op, obj));
                a.gm_objetivo = a.gm_apuntando == 2 ? obj : 0;
                a.gm_repeticiones = 0;
                a.gm_proximo_ms = ahora + 1500;
            }
            a.gm_apuntando = 0;  // un clic en el suelo cancela
        }
    }
    if (a.gm_objetivo && ahora >= a.gm_proximo_ms) {
        enviar(a, cmd::inventario_mascota(a.op, a.gm_objetivo));
        a.gm_proximo_ms = ahora + 1500;
        if (++a.gm_repeticiones >= 20) a.gm_objetivo = 0;
    }
}

void frame_juego(App& a) {
    Graficos& g = *a.g;
    u64 ahora = a.entrada.ahora_ms;
    if (a.pantalla == Pantalla::CARGANDO || !a.mundo.en_juego) {
        g.texto("Cargando...", a.gpu.ancho() / 2 - 30, a.gpu.alto() / 2 - 6, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
        return;
    }
    if (a.entrada.alt_c) a.ventana_estado = !a.ventana_estado;
    if (a.entrada.alt_i) a.ventana_inventario = !a.ventana_inventario;
    switch (a.entrada.atajo_ctrl) {  // cliente original: Ctrl+letra abre sus ventanas
    case 'I': a.ventana_inventario = !a.ventana_inventario; break;
    case 'A': a.ventana_estado = !a.ventana_estado; break;
    case 'S': a.ventana_hechizos = !a.ventana_hechizos; break;
    case 'Q': a.ventana_salida = !a.ventana_salida; break;
    case 'C': alternar_comunidad(a); break;
    case 'V': alternar_opciones(a); break;
    case 'H': alternar_ayuda(a); break;
    case 'P':  // original: captura de pantalla (BMP) en la carpeta Capture
        tomar_captura(a);
        break;
    // Solo la interfaz original tiene estos (Ctrl+R: ver abajo; Ctrl+M no tiene manejador en la tabla de atajos del cliente US).
    case 'X':  // modo ataque: invierte el byte local [0x703105] (0x456e30), en silencio y sin paquete
        if (!a.cfg.ui_simple) a.modo_ataque = !a.modo_ataque;
        break;
    case 'T':  // chat window size cycle (0x421330)
        if (!a.cfg.ui_simple) a.chat_tam = static_cast<u8>((a.chat_tam + 1) % 4);
        break;
    case 'O':  // chat colour palette (0x456dc3)
        if (!a.cfg.ui_simple) a.paleta_chat = !a.paleta_chat;
        break;
    case 'R':  // ventana flotante de HP/MP: invierte el byte de opción [0x6ddf90] (0x456d26)
        if (!a.cfg.ui_simple) a.cfg.opciones_orig ^= 1 << OPC_FLOATHPMP, guardar_config(a.cfg);
        break;
    case 'Y': case 'Z':  // solo GM: elige un objetivo, manda el opcode 83 (Ctrl+Z repite)
        if (!a.cfg.ui_simple && a.es_gm) {
            a.gm_apuntando = a.entrada.atajo_ctrl == 'Y' ? 1 : 2;
            a.chat.agregar("Select a target.", rgba(255, 230, 120), CHAT_SISTEMA);
        }
        break;
    default: break;
    }

    cambiar_zoom(a);
    const Vista vi = vista(a);
    // Primero el movimiento y después la cámara, para que en el frame en que empieza un paso
    // la cámara y el personaje usen la misma posición. El jugador queda siempre en el centro.
    actualizar_ventanas(a);
    procesar_hechizos(a, ahora);
    procesar_item_apuntado(a);
    procesar_gm_apuntado(a, ahora);
    procesar_mascotas(a);
    teclas_lista_tienda(a);  // antes de que la barra de atajos y la línea del chat tomen los dígitos
    procesar_barra(a);
    if (a.cfg.ui_simple) {  // macros de auto-buff y auto-poción: solo interfaz simple
        procesar_autopocion(a);  // antes de los auto-buffs: si tocan los dos, primero cura
        procesar_autobuffs(a);
    }
    procesar_npc(a, ahora);
    elegir_cursor(a, ahora);
    mover_jugador(a, ahora);
    Slot s = a.mundo.objetos.buscar(a.mundo.jugador.id);
    double cam_x = 0, cam_y = 0;
    if (s != SIN_SLOT) {
        // A saltos (como el original) la cámara sigue al cuerpo visible; si no, la posición continua.
        if (a.cfg.camara_a_saltos) posicion_visible(a.mundo, s, ahora, cam_x, cam_y);
        else posicion_interpolada(a.mundo, s, ahora, cam_x, cam_y);
    }
    // Desplazamiento mundo -> pantalla, fraccionario: la GPU lo lleva al píxel real más cercano
    // (1/escala de píxel lógico), así la cámara se mueve suave y todo el mundo se desplaza junto.
    const Vista vm = vista_mundo(a);  // el mundo, en sus píxeles lógicos (zoom solo del mapa)
    double ox = vm.cx - 24 - mundo_px(cam_x, cam_y);
    double oy = vm.cy - 12 - mundo_py(cam_x, cam_y);

    actualizar_segmentos(a.mundo, static_cast<int>(cam_x), static_cast<int>(cam_y), radio_segmentos(vm));
    dibujar_suelo(a, cam_x, cam_y, ox, oy);
    dibujar_objetos_mapa(a, cam_x, cam_y, ox, oy);
    dibujar_personajes(a, ahora, ox, oy);
    dibujar_efectos_mapa(a, ahora, ox, oy);
    dibujar_efectos(a, ahora, ox, oy);
    dibujar_pesca(a, ahora, ox, oy);
    aplicar_luz(a, ahora);
    dibujar_clima(a, vm.w, vm.panel_y, ahora);
    dibujar_estados_alterados(a, vi.w, vi.h);
    a.teclas_f_y = -1;  // dibujar_panel los ubica si se dibuja el panel original
    if (a.cfg.ui_simple) {
        dibujar_panel_simple(a);
        // Botones en 2 columnas, justo a la izquierda del minimapa (lado = alto de la zona de chat).
        const int lado = (CHAT_FILAS + 1) * ALTO_GLIFO;
        dibujar_botones_texto(a, vi.w - MARGEN_SIMPLE - lado - 8, vi.panel_y + 10);
    } else {
        dibujar_panel(a);
    }
    dibujar_barra(a, vi.panel_y - 36 - 6);
    if (a.cfg.ui_simple) {  // macros del cliente, no están en el cliente original
        dibujar_autobuffs(a);
        dibujar_boton_pocion(a);
    }
    dibujar_mascotas(a);
    if (a.cfg.ui_simple) dibujar_grupo(a);  // fork extra: the original has no always-on party list (S_Party opens a page)
    // Tienda, tablón y correo encima de las ventanas (nivel 30); el diálogo de cantidad, encima de todo.
    a.gpu.nivel_ventana(30);
    dibujar_tienda(a);
    dibujar_tablon(a);
    dibujar_correo(a);
    dibujar_casas(a);
    dibujar_mapa_item(a);
    a.gpu.nivel_ventana(0);
    dibujar_castillos(a);
    a.gpu.nivel_ventana(NIVEL_SOBRE_VENTANAS);
    dibujar_forma(a);
    dibujar_pregunta(a);
    a.gpu.nivel_ventana(NIVEL_SOBRE_VENTANAS);
    dibujar_cantidad(a);
    a.gpu.nivel_ventana(0);
    dibujar_reinicio(a);
    dibujar_destino(a);
    if (a.cfg.ui_simple) dibujar_minimapa(a, cam_x, cam_y);  // el panel original no tiene minimapa
    a.gpu.nivel_ventana(0);
    dibujar_chat(a);
    dibujar_ventanas(a);
    dibujar_efectos_activos(a, ahora);
    // Diálogos y avisos: encima de todas las ventanas.
    a.gpu.nivel_ventana(NIVEL_SOBRE_VENTANAS);
    dibujar_ayuda_teclas(a);
    dibujar_muerte(a);
    dibujar_espectador(a);
    if (a.cfg.mostrar_fps) dibujar_fps(a);
    a.gpu.nivel_ventana(0);
    if (a.depuracion) dibujar_depuracion(a);
}

}  // namespace lin

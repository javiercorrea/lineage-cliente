// Entregables 3 y 4: dentro del juego. Suelo isométrico de los .s32, objetos del mapa,
// personajes y NPCs animados con la tabla de sprites, movimiento propio con predicción, y la UI:
// simple (texto, estilo ASCII; por defecto) o el panel inferior original, con chat y ventana de
// estado en los dos casos (F2 alterna).
#include <cmath>
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr u64 MS_MANTENER_VIVO = 20000;
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
constexpr u32 IMG_PANEL_IZQ = 1028, IMG_PANEL_DER = 1019, IMG_CHAT = 1042, IMG_ENTRADA_CHAT = 1039;
constexpr u32 IMG_BARRA_EXP = 1029, IMG_BARRA_HP = 1030, IMG_BARRA_MP = 1031;
constexpr int ROTULO_BARRA = 28;  // ancho del rótulo "Lev"/"Hp"/"Mp" pintado al principio de cada barra
constexpr int CHAT_X = 124 + 20, CHAT_Y = 363 + 32, CHAT_ANCHO = 342, CHAT_FILAS = 5;
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

// Profundidad de dibujo en la capa del mundo (32 bits): fila de pantalla del pie (23 bits, en
// cuartos de píxel), orden dentro de esa fila (6 bits: piezas de objetos de arriba hacia abajo y
// al final los personajes) y subcapa (3 bits).
constexpr u32 ORDEN_PERSONAJE = 63;
u32 profundidad(float fila_pantalla, u32 orden, u32 sub) {
    i32 v = static_cast<i32>(fila_pantalla * 4.0f) + (1 << 22);
    return (static_cast<u32>(v < 0 ? 0 : v) << 9) | ((orden & 63) << 3) | (sub & 7);
}

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
};
AccionVisible elegir_accion(const App& a, Slot s, u64 ahora) {
    const TablaObjetos& o = a.mundo.objetos;
    GfxId gfx = o.col<O_GFX>()[s];
    const bool muerto = o.col<O_ESTADO>()[s] == ESTADO_MUERTO;
    const u8 accion = o.col<O_ACCION>()[s];
    u8 arma = muerto ? 0 : o.col<O_ESTADO>()[s];
    AccionVisible v;
    // Puerta: la última de abrir o cerrar, detenida en su último frame (el estado la recuerda).
    if (a.sprites.tipo(gfx) == TIPO_PUERTA && (o.col<O_ESTADO>()[s] == ACC_ABRIR || o.col<O_ESTADO>()[s] == ACC_CERRAR)) {
        v.rango = a.sprites.accion(gfx, o.col<O_ESTADO>()[s]);
        v.inicio = o.col<O_TACCION>()[s];
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
        if (base == ACC_MORIR) arma = 0;
    } else if (caminando(a.mundo, s, ahora)) {
        base = ACC_CAMINAR;
        v.inicio = o.col<O_TMOV>()[s];
    } else {
        base = ACC_QUIETO;
    }
    v.rango = a.sprites.accion(gfx, static_cast<u8>(base + arma));
    if (!v.rango) v.rango = a.sprites.accion(gfx, base);
    if (!v.rango) v.rango = a.sprites.accion(gfx, ACC_QUIETO);
    if (!v.rango) v.rango = a.sprites.accion(gfx, ACC_CAMINAR);
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
    const Vista vi = vista(a);
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

void dibujar_objetos_mapa(App& a, double cam_x, double cam_y, double ox, double oy) {
    Graficos& g = *a.g;
    int cx = static_cast<int>(cam_x), cy = static_cast<int>(cam_y);
    const Vista vi = vista(a);
    const App::ObjetoEnPantalla* yo = nullptr;
    a.jugador_tapado = false;
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
                // ¿Tapa al personaje? Solo si alguna de sus piezas está a pocas celdas.
                bool tapa = false;
                const bool cerca = yo && ox1 >= cx - CELDAS_TAPA && ox0 <= cx + CELDAS_TAPA && oy1 >= cy - CELDAS_TAPA &&
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
                        g.gpu().dibujar_en(g.medio_tile(t.tile, (t.x & 1) != 0), sx, sy, clave_orden(CAPA_SUELO, orden));
                        continue;
                    }
                    // Pie de la pieza (ver COLUMNAS_BASE en mapa.h); dentro de la misma fila de pie, las
                    // piezas más altas primero y los personajes después.
                    float fila_pie = static_cast<float>(mundo_py(x, y) + t.base * 12.0) + 12.0f;
                    const u32 prof = profundidad(fila_pie, ORDEN_PERSONAJE - 1 - std::min(62, static_cast<int>(t.base)), t.capa);
                    // Del objeto que tapa, solo lo que queda delante (techo, pared del frente): el
                    // interior y la pared de atrás siguen opacos.
                    const bool delante = tapa && prof > yo->profundidad;
                    g.gpu().dibujar_en(g.medio_tile(t.tile, (t.x & 1) != 0), sx, sy, clave_orden(CAPA_MUNDO, prof),
                                       delante ? rgba(255, 255, 255, ALFA_TAPA) : BLANCO);
                }
            }
        }
    }
}

// Lo último que dijo el objeto, sobre su nombre (hasta 3 filas centradas). Se quita el
// "Nombre: " / "<Nombre> " que agrega el servidor porque el nombre ya está debajo.
void dibujar_globo(App& a, std::string_view t, float centro_x, float base_y) {
    if (t.empty()) return;
    size_t corte = std::min(t.find(": "), t.find("> "));
    if (corte != std::string_view::npos && corte < 40) t.remove_prefix(corte + 2);
    constexpr int ANCHO = 160, FILAS = 3;
    std::string_view filas[FILAS];
    int n = 0;
    while (!t.empty() && n < FILAS) {
        size_t c = bytes_que_caben(t.data(), t.size(), ANCHO);
        filas[n++] = t.substr(0, c);
        t.remove_prefix(c);
        while (!t.empty() && t[0] == ' ') t.remove_prefix(1);
    }
    for (int i = 0; i < n; ++i) {
        int w = ancho_texto(filas[i].data(), filas[i].size());
        a.g->texto_sombra(filas[i], centro_x - w / 2, base_y - 14 * (n - i), clave_orden(CAPA_EFECTOS, 2),
                          rgba(255, 255, 254));
    }
}

// Color del nombre según el alineamiento (S_CharPack / S_Lawful): legal (>= 500) azul, caótico
// (<= -500) rojo, neutral blanco. Las graduaciones del original no están en los datos.
u32 color_alineamiento(i32 legal) {
    if (legal >= 500) return rgba(130, 190, 255);
    if (legal <= -500) return rgba(255, 100, 90);
    return rgba(240, 240, 240);
}

void dibujar_personajes(App& a, u64 ahora, double ox, double oy) {
    Graficos& g = *a.g;
    TablaObjetos& o = a.mundo.objetos;
    const Vista vi = vista(a);
    a.en_pantalla.clear();
    for (Slot s = 0; s < o.tamano(); ++s) {
        // Posición continua (orden de dibujo, nombre, globo) y celda desde la que se dibuja el sprite:
        // la de origen del paso, porque los frames de caminar ya traen el avance (ver mundo.h).
        double fx, fy;
        posicion_interpolada(a.mundo, s, ahora, fx, fy);
        float sx = static_cast<float>(mundo_px(fx, fy) + ox), sy = static_cast<float>(mundo_py(fx, fy) + oy);
        if (sx < -150 || sx > vi.w + 150 || sy < -150 || sy > vi.h + 150) continue;
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
        u32 prof = profundidad(static_cast<float>(mundo_py(fx, fy)) + 12.0f, ORDEN_PERSONAJE, 4);
        GfxId gfx = o.col<O_GFX>()[s];
        const AccionVisible acc = elegir_accion(a, s, ahora);
        const RangoAccion* r = acc.rango;
        float top = sy - 40;
        RegionAtlas rg;
        if (r) {
            bool termino = false;
            bool camina = !acc.puntual && acc.bucle && caminando(a.mundo, s, ahora);
            const FrameAccion* f =
                &a.sprites.frame_en(*r, ahora - acc.inicio, termino, camina ? o.col<O_DURMOV>()[s] : 0, acc.bucle);
            if (termino && acc.puntual) o.col<O_ACCION>()[s] = 0xFF;  // fin de la acción puntual
            sonido_de_frame(a, o.col<O_SONIDO>()[s], *f, vis_x, vis_y, ahora, false);
            u16 sub = static_cast<u16>(f->sub + (r->direccional ? o.col<O_DIR>()[s] : 0));
            i16 sombra = a.sprites.sombra(gfx);
            if (sombra >= 0)
                g.gpu().dibujar_en(g.frame_sprite(static_cast<GfxId>(sombra), sub, f->frame), px, py,
                                   clave_orden(CAPA_MUNDO, prof - 1), rgba(255, 255, 255, 160));
            rg = g.frame_sprite(a.sprites.gfx_archivos(gfx), sub, f->frame);
        } else {
            // Sin acciones en la tabla (los ítems del suelo, por ejemplo): el primer frame de N-0.spr.
            rg = g.frame_sprite(a.sprites.gfx_archivos(gfx), 0, 0);
        }
        bool bajo_puntero = false;
        if (rg.valida()) {
            // El ancla de los .spr es la esquina superior izquierda de la celda (calibrada con el
            // explorador: los pies quedan en el centro del rombo).
            u32 color = BLANCO;
            if (a.cfg.sombra_personajes && en_sombra(a, static_cast<int>(std::lround(vis_x)), static_cast<int>(std::lround(vis_y))))
                color = rgba(LUZ_EN_SOMBRA, LUZ_EN_SOMBRA, LUZ_EN_SOMBRA);
            // Envenenado: verdoso; congelado: gris azulado (S_Poison y los flags de S_CharPack).
            if (o.col<O_FLAGS>()[s] & FLAG_CONGELADO) color = rgba(150, 160, 185);
            else if (o.col<O_FLAGS>()[s] & FLAG_VENENO) color = rgba(140, 230, 120);
            // Invisible (S_Invis; un GM también ve así a los demás): translúcido.
            if (o.col<O_FLAGS>()[s] & FLAG_INVISIBLE) color = (color & 0x00FFFFFFu) | (110u << 24);
            g.gpu().dibujar_en(rg, px, py, clave_orden(CAPA_MUNDO, prof), color);
            // Tapado por un objeto del mapa: también su silueta encima de todo (varias piezas
            // translúcidas superpuestas casi lo esconden).
            if (o.ids()[s] == a.mundo.jugador.id && a.mundo.jugador.ceguera)  // ciego: solo se ve a sí mismo
                g.gpu().dibujar_en(rg, px, py, clave_orden(CAPA_EFECTOS, PROF_CEGUERA + 1), color);
            else if (a.jugador_tapado && o.ids()[s] == a.mundo.jugador.id)
                g.gpu().dibujar_en(rg, px, py, clave_orden(CAPA_EFECTOS, 0), rgba(255, 255, 255, ALFA_SILUETA));
            top = py + rg.oy - desfase_y;
            const float x0 = px + rg.ox, y0 = py + rg.oy, x1 = x0 + rg.w, y1 = y0 + rg.h;
            bajo_puntero = a.entrada.mx >= x0 && a.entrada.mx < x1 && a.entrada.my >= y0 && a.entrada.my < y1;
            if (o.col<O_ESTADO>()[s] != ESTADO_MUERTO) a.en_pantalla.push_back({o.ids()[s], x0, y0, x1, y1, prof});
        } else {
            // Sin sprite: un bloque magenta del tamaño de un cuerpo, que se puede elegir con el clic.
            const float x0 = static_cast<float>(mundo_px(vis_x, vis_y) + ox) + 12, y0 = static_cast<float>(mundo_py(vis_x, vis_y) + oy) - 22;
            const float x1 = x0 + 24, y1 = y0 + 36;
            g.gpu().rectangulo(static_cast<int>(x0), static_cast<int>(y0), 24, 36, clave_orden(CAPA_MUNDO, prof), rgba(255, 0, 255, 170));
            top = y0;
            bajo_puntero = a.entrada.mx >= x0 && a.entrada.mx < x1 && a.entrada.my >= y0 && a.entrada.my < y1;
            if (o.col<O_ESTADO>()[s] != ESTADO_MUERTO) a.en_pantalla.push_back({o.ids()[s], x0, y0, x1, y1, prof});
        }
        // Barra de vida si el servidor la da (S_HpMeter: grupo y mascotas; el % de S_CharPack es
        // 0xFF para los monstruos en L1J-JP) y nombre; el objetivo del ataque, en amarillo.
        const u8 hp = o.col<O_HP>()[s];
        if (hp <= 100) {
            constexpr int ANCHO_BARRA = 40;
            g.gpu().rectangulo(static_cast<int>(sx) + 4, static_cast<int>(top) - 5, ANCHO_BARRA, 3, clave_orden(CAPA_EFECTOS, 0),
                               rgba(30, 30, 30, 200));
            g.gpu().rectangulo(static_cast<int>(sx) + 4, static_cast<int>(top) - 5, ANCHO_BARRA * hp / 100, 3,
                               clave_orden(CAPA_EFECTOS, 1), rgba(220, 50, 40));
            top -= 6;
        }
        // Nombre con los "$N" resueltos en cualquier parte ("$4 (85)" es "Adena (85)"). Los ítems
        // del suelo lo muestran solo con el puntero encima, para no tapar el mapa.
        char nombre_buf[128];
        std::string_view nombre(nombre_buf, expandir_desc(a.desc, texto(a.mundo, o.col<O_NOMBRE>()[s]), nombre_buf,
                                                          sizeof nombre_buf));
        if (a.sprites.tipo(gfx) == TIPO_ITEM && !bajo_puntero) nombre = {};
        if (!nombre.empty()) {
            int w = ancho_texto(nombre.data(), nombre.size());
            // Los personajes (nombre propio; los NPC y monstruos vienen como "$N") según el
            // alineamiento: legal azul, caótico rojo, neutral blanco.
            const bool personaje = !texto(a.mundo, o.col<O_NOMBRE>()[s]).empty() && texto(a.mundo, o.col<O_NOMBRE>()[s])[0] != '$';
            u32 color = rgba(200, 220, 255);
            if (personaje) color = color_alineamiento(o.col<O_LEGAL>()[s]);
            if (o.ids()[s] == a.objetivo) color = rgba(255, 230, 90);
            g.texto_sombra(nombre, sx + 24 - w / 2, top - 14, clave_orden(CAPA_EFECTOS, 0), color);
            // Marcado con True Target: la marca y su mensaje encima del nombre.
            if (o.ids()[s] == a.marca_objetivo) {
                const int wm = ancho_texto(a.marca_texto.data(), a.marca_texto.size());
                g.texto_sombra("V", sx + 24 - 3, top - 28, clave_orden(CAPA_EFECTOS, 0), rgba(255, 80, 60));
                if (wm) g.texto_sombra(a.marca_texto, sx + 24 - wm / 2, top - 42, clave_orden(CAPA_EFECTOS, 0), rgba(255, 160, 140));
            }
            // Emblema del clan a la izquierda del nombre (se pide una vez por clan).
            if (u32 clan = o.col<O_CLAN>()[s]) {
                const RegionAtlas& em = g.emblema(clan);
                if (em.valida()) {
                    g.gpu().dibujar_en(em, sx + 24 - w / 2 - 19, top - 14, clave_orden(CAPA_EFECTOS, 0));
                } else if (std::find(a.emblemas_pedidos.begin(), a.emblemas_pedidos.end(), clan) == a.emblemas_pedidos.end()) {
                    a.emblemas_pedidos.push_back(clan);
                    enviar(a, cmd::pedir_emblema(a.op, clan));
                }
            }
            top -= 14;
            // Título encima del nombre.
            std::string_view titulo = texto(a.mundo, o.col<O_TITULO>()[s]);
            if (!titulo.empty() && personaje) {
                int wt = ancho_texto(titulo.data(), titulo.size());
                g.texto_sombra(titulo, sx + 24 - wt / 2, top - 14, clave_orden(CAPA_EFECTOS, 0), rgba(230, 200, 130));
                top -= 14;
            }
        }
        dibujar_globo(a, a.chat.globo(o.ids()[s], ahora), sx + 24, top);
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
            if (!a.aut.captura_proyectil.empty() && k >= 0.4) {  // prueba automática
                a.gpu.pedir_captura(a.aut.captura_proyectil);
                a.aut.captura_proyectil.clear();
                a.salir = true;
            }
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
        u16 sub = static_cast<u16>(f->sub + (r->direccional ? ef.dir : 0));
        const RegionAtlas& rg = g.frame_sprite(a.sprites.gfx_archivos(ef.gfx), sub, f->frame);
        float px = static_cast<float>(mundo_px(x, y) + ox), py = static_cast<float>(mundo_py(x, y) + oy);
        g.gpu().dibujar_en(rg, px, py, clave_orden(CAPA_MUNDO, profundidad(static_cast<float>(mundo_py(x, y)) + 12.0f, ORDEN_PERSONAJE, 6)),
                           BLANCO, a.sprites.aditivo(ef.gfx) ? Mezcla::ADITIVA : Mezcla::NORMAL);
    }
}

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

// Prueba automática (--atacar=texto): el objeto vivo más cercano cuyo nombre contenga el texto.
ObjId objetivo_automatico(const App& a, Slot yo) {
    const TablaObjetos& o = a.mundo.objetos;
    ObjId mejor = 0;
    int mejor_d = 1 << 30;
    for (Slot s = 0; s < o.tamano(); ++s) {
        if (s == yo || o.col<O_ESTADO>()[s] == ESTADO_MUERTO || tipo_objeto(a, s) == TIPO_ITEM) continue;
        if (a.desc.resolver(texto(a.mundo, o.col<O_NOMBRE>()[s])).find(a.aut.atacar) == std::string_view::npos) continue;
        if (std::any_of(a.mascotas.begin(), a.mascotas.end(), [&](const Mascota& m) { return m.id == o.ids()[s]; })) continue;
        int d = std::max(std::abs(o.col<O_X>()[s] - o.col<O_X>()[yo]), std::abs(o.col<O_Y>()[s] - o.col<O_Y>()[yo]));
        if (d < mejor_d) {
            mejor_d = d;
            mejor = o.ids()[s];
        }
    }
    return mejor;
}

// Prueba automática (--recoger): el ítem del suelo más cercano.
ObjId item_mas_cercano(const App& a, Slot yo) {
    const TablaObjetos& o = a.mundo.objetos;
    ObjId mejor = 0;
    int mejor_d = 1 << 30;
    for (Slot s = 0; s < o.tamano(); ++s) {
        if (a.sprites.tipo(o.col<O_GFX>()[s]) != TIPO_ITEM) continue;
        int d = std::max(std::abs(o.col<O_X>()[s] - o.col<O_X>()[yo]), std::abs(o.col<O_Y>()[s] - o.col<O_Y>()[yo]));
        if (d < mejor_d) {
            mejor_d = d;
            mejor = o.ids()[s];
        }
    }
    return mejor;
}

// Ataque al objetivo (adyacente o, con arco, a distancia): la animación propia empieza ya (el eco
// del servidor no la reinicia, ver Ev::ATAQUE) y no se vuelve a atacar hasta que termine. La
// flecha la dibuja el eco (S_UseArrowSkill trae su gráfico y las celdas: ver mundo.cpp).
void atacar(App& a, Slot yo, Slot obj, u64 ahora) {
    TablaObjetos& o = a.mundo.objetos;
    int dir = direccion_de(o.col<O_X>()[obj] - o.col<O_X>()[yo], o.col<O_Y>()[obj] - o.col<O_Y>()[yo]);
    if (dir >= 0) o.col<O_DIR>()[yo] = static_cast<u8>(dir);
    enviar(a, cmd::atacar(a.op, o.ids()[obj], o.col<O_X>()[obj], o.col<O_Y>()[obj]));
    o.col<O_ACCION>()[yo] = ACC_ATACAR;
    o.col<O_TACCION>()[yo] = ahora;
    GfxId gfx = o.col<O_GFX>()[yo];
    u32 ms = a.sprites.duracion_ms(gfx, static_cast<u8>(ACC_ATACAR + o.col<O_ESTADO>()[yo]));
    if (ms == 0) ms = a.sprites.duracion_ms(gfx, ACC_ATACAR);
    if (ms == 0) ms = ATAQUE_DESCONOCIDO_MS;
    a.fin_ataque = ahora + intervalo_propio(a.mundo.jugador, Intervalo::ATACAR, ms);
}

// Movimiento propio con predicción: se avanza localmente y se avisa al servidor (C_MoveChar
// lleva la celda de origen). Si la celda está bloqueada solo se gira.

// Portales (entradas de mazmorras, escaleras): el servidor teletransporta cuando el destino de un
// paso es un portal de su tabla (C_MoveChar mira DungeonTable antes que nada), pero el mapa del
// cliente marca casi todas esas celdas como bloqueadas y no dice cuáles son portales (en el mapa
// 4, 39 de 74 tienen el atributo 0x08, que también tienen 3857 celdas que no lo son). Así que, al
// intentar entrar a una celda bloqueada vecina con el teclado o con un clic justo sobre ella, se
// manda el paso sin moverse aquí: si es un portal llega el teletransporte; si no, no pasa nada
// visible (el servidor toma el origen del próximo paso del paquete). Una vez por segundo por celda.
bool intento_portal(App& a, int nx, int ny, int dir, bool teclado, u64 ahora) {
    const u32 celda = static_cast<u32>(nx & 0xFFFF) << 16 | static_cast<u32>(ny & 0xFFFF);
    if (celda == a.celda_intento_portal && ahora < a.t_intento_portal + 1000) return false;
    if (!teclado) {
        // ¿El puntero está sobre esa celda? (inversa de mundo_px / mundo_py desde el centro del personaje)
        const Vista vi = vista(a);
        const double rx = (a.entrada.mx - vi.cx) / 24.0, ry = (a.entrada.my - vi.cy) / 12.0;
        const int cx = static_cast<int>(std::lround((rx - ry) / 2)), cy = static_cast<int>(std::lround((rx + ry) / 2));
        if (cx != DIR_DX[dir] || cy != DIR_DY[dir]) return false;
    }
    a.celda_intento_portal = celda;
    a.t_intento_portal = ahora;
    return true;
}

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
    a.hablar_objetivo = b != SIN_SLOT && es_npc(a, b) && !e.ctrl;
    const bool forzar = e.ctrl || (a.modo_pk && tipo == TIPO_PERSONAJE);
    if (b != SIN_SLOT && tipo != TIPO_ITEM && tipo != TIPO_MONSTRUO && !a.hablar_objetivo && !forzar) a.objetivo = 0;
}

void mover_jugador(App& a, u64 ahora) {
    TablaObjetos& o = a.mundo.objetos;
    Slot s = o.buscar(a.mundo.jugador.id);
    const EntradaFrame& e = a.entrada;
    const Vista vi = vista(a);
    bool sobre_panel = (e.my >= vi.panel_y && e.mx >= vi.panel_x0 && e.mx < vi.panel_x1) || sobre_inventario(a) ||
                       sobre_hechizos(a) || sobre_barra(a) || sobre_forma(a) || sobre_cantidad(a) || sobre_mascotas(a) || sobre_destino(a) || sobre_npc(a) || a.arrastrando != 0 || a.arrastrando_hechizo != 0 ||
                       a.arrastrando_atajo >= 0;
    // Objetivo: lo que hay bajo el puntero al apretar el botón, y se mantiene mientras siga
    // apretado aunque la cámara se mueva y el puntero ya no esté encima (se camina hasta él y se
    // lo ataca). Apretado sobre el suelo se camina hacia el puntero, y si el puntero pasa sobre
    // algo, eso queda como objetivo. Soltar lo abandona, salvo con la tecla A (ataque sostenido).
    bool clic_en_mundo = e.presionado && !a.clic_consumido && !sobre_panel && !sobre_alguna_ventana(a) &&
                         a.ventana_arrastrada < 0;
    // Antes de esperar el fin del paso: el frame en que se aprieta no se pierde.
    if (clic_en_mundo && e.pulsado) {
        a.hechizo_pendiente = 0;  // un clic nuevo cancela el hechizo en camino
        a.ataque_sostenido = false;
        elegir_objetivo_bajo_puntero(a);
        a.objetivo_fijo = a.objetivo != 0;
    }
    if (!e.presionado) a.objetivo_fijo = false;
    // Tecla A (con el chat cerrado): atacar al objetivo, o al monstruo bajo el puntero, hasta que
    // muera o se camine.
    if (e.tecla_a && a.cfg.chat_con_enter && !a.escribiendo_chat && !a.cantidad.abierto && !a.ventana_forma) {
        ObjId t = a.objetivo && !a.hablar_objetivo ? a.objetivo : objeto_bajo_puntero(a);
        Slot b = t ? o.buscar(t) : SIN_SLOT;
        if (b != SIN_SLOT && (tipo_objeto(a, b) == TIPO_MONSTRUO || e.ctrl || (a.modo_pk && tipo_objeto(a, b) == TIPO_PERSONAJE))) {
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
    } else if (!e.presionado && a.aut.atacar.empty() && !a.aut.recoger && !a.hablar_objetivo && !a.ataque_sostenido) {
        a.objetivo = 0;
    }
    if (a.objetivo == 0 && a.frames_en_pantalla > 60) {
        if (!a.aut.atacar.empty()) a.objetivo = objetivo_automatico(a, s);
        if (a.objetivo == 0 && a.aut.recoger) a.objetivo = item_mas_cercano(a, s);
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
            if (tipo_objeto(a, obj) == TIPO_PUERTA)
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
                enviar(a, cmd::recoger(a.op, o.col<O_X>()[obj], o.col<O_Y>()[obj], a.objetivo,
                                       std::max<u32>(1, o.col<O_CANTIDAD>()[obj])));
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
    if (dir < 0 && a.aut.ruta_hecha < a.aut.ruta.size() && a.frames_en_pantalla > 60) {
        // Prueba automática: direcciones fijas (dígitos 0–7), una por paso.
        dir = (a.aut.ruta[a.aut.ruta_hecha++] - '0') & 7;
        ruta = true;
    }
    if (dir < 0 && a.aut.pasos > a.pasos_auto_hechos && a.frames_en_pantalla > 60) {
        // Prueba automática: caminar en la primera dirección libre empezando por la derecha.
        for (int i = 0; i < 8 && dir < 0; ++i) {
            int d = (3 + i) % 8;
            if (celda_pasable(a.mundo, a.rec, o.col<O_X>()[s] + DIR_DX[d], o.col<O_Y>()[s] + DIR_DY[d])) dir = d;
        }
        ++a.pasos_auto_hechos;
    }
    if (dir < 0) return;
    u16 x = o.col<O_X>()[s], y = o.col<O_Y>()[s];
    int nx = x + DIR_DX[dir], ny = y + DIR_DY[dir];
    if (celda_pasable(a.mundo, a.rec, nx, ny)) {
        enviar(a, cmd::mover(a.op, x, y, static_cast<u8>(dir)));
        o.col<O_XANT>()[s] = x;
        o.col<O_YANT>()[s] = y;
        o.col<O_X>()[s] = static_cast<u16>(nx);
        o.col<O_Y>()[s] = static_cast<u16>(ny);
        o.col<O_TMOV>()[s] = inicio_paso;
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

// Barra con imagen: el rótulo queda siempre y el resto se recorta según la proporción.
void barra(Graficos& g, u32 img, int x, int y, float proporcion, std::string_view t) {
    RegionAtlas r = g.img(img);
    if (!r.valida()) return;
    proporcion = std::clamp(proporcion, 0.0f, 1.0f);
    int rotulo = std::min<int>(ROTULO_BARRA, r.w);
    r.w = static_cast<u16>(rotulo + static_cast<int>((r.w - rotulo) * proporcion));
    g.gpu().dibujar(r, x, y, clave_orden(CAPA_UI, 2));
    g.texto_sombra(t, x + 32, y + (g.img(img).h - ALTO_GLIFO) / 2, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
}

void dibujar_panel(App& a) {
    Graficos& g = *a.g;
    const EstadoJugador& j = a.mundo.jugador;
    const Vista vi = vista(a);
    const int X = vi.hx, Y = vi.hy;
    g.gpu().dibujar(g.img(IMG_PANEL_IZQ), X, Y + 368, clave_orden(CAPA_UI, 0));
    g.gpu().dibujar(g.img(IMG_PANEL_DER), X + 485, Y + 366, clave_orden(CAPA_UI, 0));
    dibujar_teclas_f(a, X + 485, Y + 366);
    dibujar_botones_panel(a, X + 485 + 26, Y + 366 + 3);
    g.gpu().dibujar(g.img(IMG_CHAT), X + 124, Y + 363, clave_orden(CAPA_UI, 1));
    g.gpu().dibujar(g.img(IMG_ENTRADA_CHAT), X + 124 + 13, Y + 363 + 99, clave_orden(CAPA_UI, 2));
    char t[48];
    float pexp = porcentaje_exp(j.nivel, j.exp);
    std::snprintf(t, sizeof t, "%d  %.2f%%", j.nivel, pexp);
    barra(g, IMG_BARRA_EXP, X + 3, Y + 386, pexp / 100.0f, t);
    std::snprintf(t, sizeof t, "%d/%d", j.hp, j.hp_max);
    barra(g, IMG_BARRA_HP, X + 3, Y + 402, j.hp_max > 0 ? static_cast<float>(j.hp) / j.hp_max : 0.0f, t);
    std::snprintf(t, sizeof t, "%d/%d", j.mp, j.mp_max);
    barra(g, IMG_BARRA_MP, X + 3, Y + 423, j.mp_max > 0 ? static_cast<float>(j.mp) / j.mp_max : 0.0f, t);
    // Cuatro casillas con ícono: CA, alineamiento, peso y comida.
    u64 kt = clave_orden(CAPA_UI_TEXTO, 0);
    u32 c = rgba(255, 255, 254);
    std::snprintf(t, sizeof t, "%d", j.ca);
    g.texto(t, X + 24, Y + 447, kt, c);
    std::snprintf(t, sizeof t, "%d", j.legal);
    g.texto(t, X + 88, Y + 447, kt, c);
    std::snprintf(t, sizeof t, "%d%%", j.peso * 100 / 240);
    g.texto(t, X + 24, Y + 464, kt, c);
    std::snprintf(t, sizeof t, "%d%%", j.comida * 100 / 225);
    g.texto(t, X + 88, Y + 464, kt, c);
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

void enviar_chat(App& a, std::string_view linea) {
    if (comando_social(a, linea)) return;
    EntradaChat c;
    if (!interpretar_entrada(linea, c)) return;
    if (c.susurro) enviar(a, cmd::susurrar(a.op, c.destino, c.texto));
    else enviar(a, cmd::chat(a.op, c.tipo, c.texto));
}

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
        return {MARGEN_SIMPLE, y, vi.w - 2 * MARGEN_SIMPLE, MARGEN_SIMPLE + 12, entrada_y, vi.w - 2 * MARGEN_SIMPLE - 12,
                0, vi.panel_y, vi.w, vi.h - vi.panel_y};
    }
    const int X = vi.hx, Y = vi.hy;
    return {X + CHAT_X, Y + CHAT_Y, CHAT_ANCHO, X + ENTRADA_X, Y + ENTRADA_Y, 330, X + 124, Y + 363, 390, 117};
}

// Registro (las últimas filas, ajustadas al ancho) y caja de entrada, que siempre tiene el foco
// como en el original: se escribe y Enter envía; Esc borra; rueda o Re Pág/Av Pág desplazan.
void dibujar_chat(App& a) {
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const AreaChat ac = area_chat(a);
    u32 n = a.chat.cantidad();
    int paso = (ui::sobre(e, ac.zona_x, ac.zona_y, ac.zona_w, ac.zona_h) ? e.rueda : 0) + (e.pagina_arriba ? CHAT_FILAS : 0) -
               (e.pagina_abajo ? CHAT_FILAS : 0);
    i64 d = static_cast<i64>(a.chat_desplazamiento) + paso;
    a.chat_desplazamiento = static_cast<u32>(std::clamp<i64>(d, 0, n > 0 ? n - 1 : 0));

    u64 kt = clave_orden(CAPA_UI_TEXTO, 0);
    int fila = CHAT_FILAS - 1;
    for (u32 i = a.chat_desplazamiento; i < n && fila >= 0; ++i) {
        const LineaChat& l = a.chat.reciente(i);
        std::string_view t = a.chat.texto(l);
        std::string_view partes[8];
        int k = 0;
        while (!t.empty() && k < 8) {
            size_t c = bytes_que_caben(t.data(), t.size(), ac.ancho);
            partes[k++] = t.substr(0, c);
            t.remove_prefix(c);
            while (!t.empty() && t[0] == ' ') t.remove_prefix(1);
        }
        for (int p = k - 1; p >= 0 && fila >= 0; --p, --fila)
            g.texto_sombra(partes[p], ac.x, ac.y + fila * ALTO_GLIFO, kt, l.color);
    }
    if (a.chat_desplazamiento > 0) g.texto_sombra("^", ac.x + ac.ancho - 6, ac.y, kt, rgba(255, 220, 120));

    // Línea de entrada. Con "chat con Enter" está cerrada hasta apretar Enter (así las teclas son
    // acciones); Enter envía y la cierra, Esc la vacía y la cierra (y no llega a las ventanas).
    const bool con_enter = a.cfg.chat_con_enter;
    if (con_enter && !a.escribiendo_chat && e.enter && !a.ventana_forma) {
        a.escribiendo_chat = true;
        e.enter = false;
    }
    const bool abierta = !con_enter || a.escribiendo_chat;
    if (a.cfg.ui_simple) g.texto(">", ac.entrada_x - 12, ac.entrada_y, kt, abierta ? rgba(214, 190, 140) : rgba(110, 100, 80));
    if (abierta && e.escape) {
        a.chat_entrada.clear();
        if (con_enter) {
            a.escribiendo_chat = false;
            e.escape = false;
        }
    }
    if (!abierta) {
        g.texto("Enter: chat", ac.entrada_x, ac.entrada_y, kt, rgba(110, 100, 80));
    } else if (ui::caja_texto(g, e, a.chat_entrada, ac.entrada_x, ac.entrada_y, ac.entrada_ancho, true, false, MAX_ENTRADA_CHAT)) {
        if (!a.chat_entrada.empty()) enviar_chat(a, a.chat_entrada);
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
    if (ui::ventana(g, e, a.cfg.ui_simple, ESTADO_X, ESTADO_Y, "Status", opacidad_ventanas(a))) a.ventana_estado = false;

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
    if (z == 0) return;
    int maxima = a.gpu.escala_maxima();
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
constexpr u32 NIVEL_SOBRE_VENTANAS = 31;

void dibujar_ventanas(App& a) {
    EntradaFrame& e = a.entrada;
    const int mx = e.mx, my = e.my;
    const bool arrastre = a.arrastrando || a.arrastrando_hechizo || a.arrastrando_atajo >= 0 || a.ventana_arrastrada >= 0;
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
        case IdVentana::OPCIONES: dibujar_opciones(a); break;
        case IdVentana::SALIDA: dibujar_salida(a); break;
        case IdVentana::SOCIAL: dibujar_social(a); break;
        case IdVentana::INTERCAMBIO: dibujar_intercambio(a); break;
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
    if (j.ceguera)
        a.g->gpu().rectangulo(0, 0, w, h, clave_orden(CAPA_EFECTOS, PROF_CEGUERA), rgba(0, 0, 0, j.ceguera == 2 ? 200 : 245));
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
    if (a.aut.reiniciar && a.entrada.ahora_ms - a.t_muerte > 5000) pedir = true;  // prueba automática
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

// Cursor: círculo si un hechizo espera su objetivo; espada sobre un monstruo (o con Ctrl sobre otro
// personaje o NPC, que es atacarlo); si no, la mano (también sobre la interfaz).
void elegir_cursor(App& a) {
    if (a.hechizo_apuntando || a.mascota_eligiendo) {
        a.cursor = Cursor::OBJETIVO;
        return;
    }
    const Vista vi = vista(a);
    if (a.entrada.my >= vi.panel_y || sobre_alguna_ventana(a) || sobre_barra(a)) return;
    ObjId id = objeto_bajo_puntero(a);
    Slot s = id ? a.mundo.objetos.buscar(id) : SIN_SLOT;
    if (s == SIN_SLOT) return;
    u8 tipo = tipo_objeto(a, s);
    if (tipo == TIPO_MONSTRUO || (a.entrada.ctrl && tipo != TIPO_ITEM) || (a.modo_pk && tipo == TIPO_PERSONAJE))
        a.cursor = Cursor::ESPADA;
    else if (es_npc(a, s) && tipo != TIPO_PUERTA)  // sin Ctrl, el clic le habla
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
ObjId objeto_bajo_puntero(const App& a, bool incluir_propio) {
    const EntradaFrame& e = a.entrada;
    const App::ObjetoEnPantalla* mejor = nullptr;
    for (const App::ObjetoEnPantalla& r : a.en_pantalla) {
        if ((r.id == a.mundo.jugador.id && !incluir_propio) || e.mx < r.x0 || e.mx >= r.x1 || e.my < r.y0 || e.my >= r.y1) continue;
        if (!mejor || r.profundidad > mejor->profundidad) mejor = &r;
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


void frame_juego(App& a) {
    Graficos& g = *a.g;
    u64 ahora = a.entrada.ahora_ms;
    if (a.pantalla == Pantalla::CARGANDO || !a.mundo.en_juego) {
        g.texto("Cargando...", a.gpu.ancho() / 2 - 30, a.gpu.alto() / 2 - 6, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
        return;
    }
    if (a.aut.estado) {
        a.ventana_estado = true;
        a.aut.estado = false;
    }
    if (a.entrada.alt_c) a.ventana_estado = !a.ventana_estado;
    if (a.entrada.alt_i) a.ventana_inventario = !a.ventana_inventario;
    acciones_item_automaticas(a);
    // Prueba automática: una línea de --decir por segundo después de entrar.
    if (a.aut.dichas < a.aut.decir.size() && a.frames_en_pantalla > 90 + 60 * a.aut.dichas)
        enviar_chat(a, a.aut.decir[a.aut.dichas++]);

    cambiar_zoom(a);
    const Vista vi = vista(a);
    // Primero el movimiento y después la cámara, para que en el frame en que empieza un paso
    // la cámara y el personaje usen la misma posición. El jugador queda siempre en el centro.
    actualizar_ventanas(a);
    procesar_hechizos(a, ahora);
    procesar_mascotas(a);
    procesar_barra(a);
    procesar_npc(a, ahora);
    procesar_social(a, ahora);
    elegir_cursor(a);
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
    double ox = vi.cx - 24 - mundo_px(cam_x, cam_y);
    double oy = vi.cy - 12 - mundo_py(cam_x, cam_y);
    if (ahora - a.t_mantener_vivo > MS_MANTENER_VIVO) {
        enviar(a, cmd::mantener_vivo(a.op));
        a.t_mantener_vivo = ahora;
    }

    actualizar_segmentos(a.mundo, static_cast<int>(cam_x), static_cast<int>(cam_y), radio_segmentos(vi));
    dibujar_suelo(a, cam_x, cam_y, ox, oy);
    dibujar_objetos_mapa(a, cam_x, cam_y, ox, oy);
    dibujar_personajes(a, ahora, ox, oy);
    dibujar_efectos(a, ahora, ox, oy);
    dibujar_clima(a, vi.w, vi.panel_y, ahora);
    dibujar_estados_alterados(a, vi.w, vi.h);
    a.teclas_f_y = -1;  // dibujar_panel los ubica si se dibuja el panel original
    if (a.cfg.ui_simple) {
        dibujar_panel_simple(a);
        dibujar_botones_texto(a, vi.w - MARGEN_SIMPLE, area_chat(a).entrada_y - 2);
    } else {
        dibujar_panel(a);
    }
    dibujar_barra(a, vi.panel_y - 36 - 6);
    dibujar_mascotas(a);
    a.gpu.nivel_ventana(NIVEL_SOBRE_VENTANAS);
    dibujar_forma(a);
    dibujar_pregunta(a);
    dibujar_cantidad(a);
    dibujar_destino(a);
    a.gpu.nivel_ventana(0);
    dibujar_chat(a);
    dibujar_ventanas(a);
    dibujar_efectos_activos(a, ahora);
    // Diálogos y avisos: encima de todas las ventanas.
    a.gpu.nivel_ventana(NIVEL_SOBRE_VENTANAS);
    dibujar_ayuda_teclas(a);
    dibujar_muerte(a);
    dibujar_espectador(a);
    a.gpu.nivel_ventana(0);
    if (a.depuracion) dibujar_depuracion(a);
}

}  // namespace lin

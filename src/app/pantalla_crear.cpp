// Entregable 2: creación de personaje. Imágenes y posiciones de drawmode/draw_new_char.cpp de
// l1j-client; reglas de C_CreateChar.java (L1J-JP): cada stat entre la base de la clase y
// base + bonus de la clase, y la suma de los seis debe ser 75. Con la UI simple (F2), lo mismo en
// recuadros de texto.
#include "app/app.h"
#include "i18n/textos.h"

namespace lin {

namespace {

// Atributos, topes, botones y animaciones de cada clase: a.clases (mundo/clases.h). Stats en el orden del
// paquete: fue, des, con, sab, car, int.
constexpr int TOTAL_STATS = 75;
constexpr int POS_STAT[6][2] = {{406, 317}, {406, 332}, {406, 347}, {525, 317}, {525, 332}, {525, 347}};
constexpr int POS_BAJAR[6][2] = {{424, 317}, {424, 332}, {424, 347}, {498, 317}, {498, 332}, {498, 347}};

const Clase& clase_elegida(const App& a) { return a.clases.de(a.clase_nueva); }

// Puntos que se le pueden sumar a un stat: los flotantes de la clase, sin pasar el máximo absoluto.
int tope_extra(const Clase& k, int i) { return std::min<int>(k.bonus, k.tope[i] - k.base[i]); }

int puntos_libres(const App& a) {
    int usados = 0;
    for (int i = 0; i < 6; ++i) usados += a.extra[i] + clase_elegida(a).base[i];
    return TOTAL_STATS - usados;
}

// Cuadro de la animación de vista previa (las mismas que en la selección); 0 si la clase no tiene.
u32 cuadro_previa(const App& a) {
    const AnimClase& an = clase_elegida(a).anim[a.sexo_nuevo];
    return an.cantidad ? an.primero + static_cast<u32>((a.entrada.ahora_ms - a.t_anim) / 67 % an.cantidad) : 0;
}

const char* motivo_crear(int c) {
    switch (c) {
    case 6: return tr(Tx::CREAR_ERR_NOMBRE_EXISTE);
    case 9: return tr(Tx::CREAR_ERR_NOMBRE_INVALIDO);
    case 21: return tr(Tx::CREAR_ERR_REPARTO);
    default: return tr(Tx::CREAR_ERR_GENERICO);
    }
}

// Radio con imagen: N normal, N+1 marcado (sdl_check_button).
bool radio(Graficos& g, const EntradaFrame& e, u32 n, int x, int y, bool marcado) {
    RegionAtlas r = g.img(marcado ? n + 1 : n);
    if (!r.valida()) r = g.img(n);
    g.gpu().dibujar(r, x, y, clave_orden(CAPA_UI, 1));
    return r.valida() && e.clic && ui::sobre(e, x + r.ox, y + r.oy, r.w, r.h);
}

void elegir_clase(App& a, int c) {
    a.clase_nueva = c;
    std::memset(a.extra, 0, sizeof a.extra);
    a.t_anim = a.entrada.ahora_ms;
}

void elegir_sexo(App& a, int s) {
    a.sexo_nuevo = s;
    a.t_anim = a.entrada.ahora_ms;
}

void bajar_stat(App& a, int i) {
    if (a.extra[i] > 0) --a.extra[i];
}

void subir_stat(App& a, int i) {
    if (puntos_libres(a) > 0 && a.extra[i] < tope_extra(clase_elegida(a), i)) ++a.extra[i];
}

// Pantalla original (draw_new_char.cpp). Igual que crear_simple.
bool crear_original(App& a, bool& volver) {
    Graficos& g = *a.g;
    Gpu& gpu = g.gpu();
    const EntradaFrame& e = a.entrada;
    gpu.dibujar(g.img(824), 0, 0, clave_orden(CAPA_FONDO, 0));
    for (int c = 0; c < a.clases.n; ++c) {
        const Clase& k = a.clases.c[c];
        if (k.boton && radio(g, e, k.boton, k.x, k.y, a.clase_nueva == c)) elegir_clase(a, c);
    }
    if (radio(g, e, 306, 348, 248, a.sexo_nuevo == 0)) elegir_sexo(a, 0);
    if (radio(g, e, 304, 533, 248, a.sexo_nuevo == 1)) elegir_sexo(a, 1);

    if (const u32 cuadro = cuadro_previa(a)) gpu.dibujar(g.png(cuadro), 410, 0, clave_orden(CAPA_UI, 0));

    // Stats con botones de bajar (556) y subir (554).
    char t[16];
    for (int i = 0; i < 6; ++i) {
        int base = clase_elegida(a).base[i];
        if (ui::boton(g, e, 556, POS_BAJAR[i][0], POS_BAJAR[i][1])) bajar_stat(a, i);
        if (ui::boton(g, e, 554, POS_BAJAR[i][0] + 11, POS_BAJAR[i][1])) subir_stat(a, i);
        std::snprintf(t, sizeof t, "%2d", base + a.extra[i]);
        g.texto(t, POS_STAT[i][0], POS_STAT[i][1], clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
    }
    std::snprintf(t, sizeof t, "%2d", puntos_libres(a));
    g.texto(t, 468, 334, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));

    ui::caja_texto(g, e, a.nombre_nuevo, 440, 381, 120, true, false, 16);
    if (!a.aviso.empty()) g.texto_sombra(a.aviso, 332, 460, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 120, 100));

    volver = ui::boton(g, e, 827, 476, 430);
    return ui::boton(g, e, 825, 476, 403);
}

// UI simple: clase y sexo a la izquierda; atributos, nombre y la animación a la derecha.
// Devuelve true si se pidió crear, y en `volver` si se canceló.
bool crear_simple(App& a, bool& volver) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 0);
    const u32 tenue = rgba(170, 160, 140);
    char t[32];
    g.gpu().rectangulo(0, 0, g.gpu().ancho(), g.gpu().alto(), clave_orden(CAPA_FONDO, 0), ui::COLOR_FONDO_SIMPLE);
    g.texto(tr(Tx::CREAR_TITULO), 20, 18, kt, ui::COLOR_TITULO);

    ui::recuadro(g, 20, 38, 180, 384);
    g.texto(tr(Tx::CREAR_CLASE), 32, 48, kt, ui::COLOR_TITULO);
    const int paso = a.clases.n > 7 ? 24 : 26;  // con 8 clases la lista llega justo antes de "Sexo"
    for (int c = 0; c < a.clases.n; ++c) {
        const int y = 66 + c * paso;
        const bool encima = ui::sobre(e, 26, y, 168, 22);
        if (c == a.clase_nueva || encima)
            g.gpu().rectangulo(26, y, 168, 22, clave_orden(CAPA_UI, 3), c == a.clase_nueva ? rgba(90, 70, 40, 230) : rgba(50, 40, 28, 230));
        g.texto(nombre_clase(c), 36, y + 5, kt, c == a.clase_nueva ? ui::COLOR_TEXTO : tenue);
        if (encima && e.clic) elegir_clase(a, c);
    }
    g.texto(tr(Tx::CREAR_SEXO), 32, 262, kt, ui::COLOR_TITULO);
    const char* sexos[2] = {tr(Tx::CREAR_HOMBRE), tr(Tx::CREAR_MUJER)};
    for (int s = 0; s < 2; ++s) {
        const int y = 280 + s * 26;
        const bool encima = ui::sobre(e, 26, y, 168, 22);
        if (s == a.sexo_nuevo || encima)
            g.gpu().rectangulo(26, y, 168, 22, clave_orden(CAPA_UI, 3), s == a.sexo_nuevo ? rgba(90, 70, 40, 230) : rgba(50, 40, 28, 230));
        g.texto(sexos[s], 36, y + 5, kt, s == a.sexo_nuevo ? ui::COLOR_TEXTO : tenue);
        if (encima && e.clic) elegir_sexo(a, s);
    }

    ui::recuadro(g, 214, 38, 406, 384);
    if (const u32 cuadro = cuadro_previa(a)) g.gpu().dibujar(g.png(cuadro), 450, 30, clave_orden(CAPA_UI, 4));

    // Atributos en el orden del paquete: fue, des, con, sab, car, int.
    const char* nombres[6] = {"STR", "DEX", "CON", "WIS", "CHA", "INT"};
    g.texto(tr(Tx::CREAR_ATRIBUTOS), 228, 48, kt, ui::COLOR_TITULO);
    for (int i = 0; i < 6; ++i) {
        const int y = 70 + i * 28;
        g.texto(nombres[i], 232, y + 4, kt, tenue);
        if (ui::boton_texto(g, e, "-", 280, y, 22, 20)) bajar_stat(a, i);
        std::snprintf(t, sizeof t, "%2d", clase_elegida(a).base[i] + a.extra[i]);
        g.texto(t, 312, y + 4, kt, a.extra[i] ? rgba(255, 230, 150) : ui::COLOR_TEXTO);
        if (ui::boton_texto(g, e, "+", 336, y, 22, 20)) subir_stat(a, i);
    }
    std::snprintf(t, sizeof t, tr(Tx::CREAR_PUNTOS_LIBRES), puntos_libres(a));
    g.texto(t, 232, 244, kt, puntos_libres(a) ? rgba(255, 230, 150) : tenue);

    g.texto(tr(Tx::CREAR_NOMBRE), 232, 300, kt, ui::COLOR_TITULO);
    ui::campo(g, e, a.nombre_nuevo, 280, 300, 150, true, false, 16);
    if (!a.aviso.empty()) g.texto(a.aviso, 232, 334, kt, rgba(255, 120, 100));
    g.texto(tr(Tx::UI_F2_ORIGINAL), 20, 449, kt, rgba(120, 110, 90));

    volver = ui::boton_texto(g, e, tr(Tx::UI_VOLVER), 528, 444, 92, 22);
    return ui::boton_texto(g, e, tr(Tx::CREAR_CREAR), 428, 444, 92, 22);
}

}  // namespace

void frame_crear(App& a) {
    const EntradaFrame& e = a.entrada;
    bool volver = false;
    bool aceptar = a.cfg.ui_simple ? crear_simple(a, volver) : crear_original(a, volver);
    aceptar |= e.enter;
    volver |= e.escape;

    if (aceptar) {
        if (puntos_libres(a) != 0) {
            a.aviso = tr(Tx::CREAR_FALTA_PUNTOS);
        } else if (a.nombre_nuevo.empty()) {
            a.aviso = tr(Tx::CREAR_FALTA_NOMBRE);
        } else {
            u8 stats[6];
            for (int i = 0; i < 6; ++i) stats[i] = static_cast<u8>(clase_elegida(a).base[i] + a.extra[i]);
            enviar(a, cmd::crear_personaje(a.op, a.nombre_nuevo, static_cast<u8>(a.clase_nueva),
                                           static_cast<u8>(a.sexo_nuevo), stats));
            a.aviso.clear();
        }
    }
    if (volver) {
        a.aviso.clear();
        cambiar_pantalla(a, Pantalla::PERSONAJES);
    }

    // Resultado de la creación (S_CharCreateStatus); el éxito llega como PERSONAJE_NUEVO.
    for (const Evento& ev : a.eventos)
        if (ev.tipo == Ev::RESULTADO_CREAR && ev.v[0] != 2) a.aviso = motivo_crear(ev.v[0]);
}

// Crear sin pasar por la pantalla (control en vivo): la clase, los puntos libres repartidos en orden
// y el pedido al servidor. El resultado llega como en la pantalla (PERSONAJE_NUEVO o RESULTADO_CREAR).
void crear_personaje_rapido(App& a, std::string_view nombre, int clase) {
    a.nombre_nuevo = nombre;
    a.clase_nueva = std::clamp(clase, 0, a.clases.n - 1);
    for (int i = 0; i < 6; ++i) a.extra[i] = 0;
    for (int i = 0; i < 6 && puntos_libres(a) > 0; ++i)
        while (puntos_libres(a) > 0 && a.extra[i] < tope_extra(clase_elegida(a), i)) ++a.extra[i];
    u8 stats[6];
    for (int i = 0; i < 6; ++i) stats[i] = static_cast<u8>(clase_elegida(a).base[i] + a.extra[i]);
    enviar(a, cmd::crear_personaje(a.op, a.nombre_nuevo, static_cast<u8>(a.clase_nueva), static_cast<u8>(a.sexo_nuevo), stats));
}

const u8* stats_base_clase(const App& a, int clase) { return a.clases.de(clase).base; }
int bonus_clase(const App& a, int clase) { return a.clases.de(clase).bonus; }

}  // namespace lin

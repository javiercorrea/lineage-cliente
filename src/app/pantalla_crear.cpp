// Entregable 2: creación de personaje. Imágenes y posiciones de drawmode/draw_new_char.cpp de
// l1j-client; reglas de C_CreateChar.java (L1J-JP): cada stat entre la base de la clase y
// base + bonus de la clase, y la suma de los seis debe ser 75. Con la UI simple (F2), lo mismo en
// recuadros de texto.
#include "app/app.h"

namespace lin {

namespace {

// Orden de stats en el paquete: fue, des, con, sab, car, int.
constexpr u8 BASE[7][6] = {
    {13, 10, 10, 11, 13, 10},  // príncipe
    {16, 12, 14, 9, 12, 8},    // caballero
    {11, 12, 12, 12, 9, 12},   // elfo
    {8, 7, 12, 12, 8, 12},     // mago
    {12, 15, 8, 10, 9, 11},    // elfo oscuro
    {13, 11, 14, 12, 8, 11},   // caballero dragón
    {11, 10, 12, 12, 8, 12},   // ilusionista
};
constexpr int BONUS[7] = {8, 4, 7, 16, 10, 6, 10};
constexpr int TOTAL_STATS = 75;

constexpr u32 IMG_CLASE[7] = {1753, 1755, 1757, 1759, 1761, 1749, 1751};
constexpr int POS_CLASE[7][2] = {{332, 11}, {542, 11}, {332, 67}, {542, 67}, {332, 118}, {542, 118}, {332, 166}};
// Animación de vista previa por clase × 2 + sexo (las mismas que en la selección).
constexpr u16 PREVIA[14][2] = {{714, 86}, {629, 82}, {378, 71}, {315, 60}, {245, 67}, {166, 76}, {531, 95},
                               {452, 76}, {90, 73},  {25, 62},  {841, 65}, {908, 58}, {969, 68}, {1039, 87}};
constexpr int POS_STAT[6][2] = {{406, 317}, {406, 332}, {406, 347}, {525, 317}, {525, 332}, {525, 347}};
constexpr int POS_BAJAR[6][2] = {{424, 317}, {424, 332}, {424, 347}, {498, 317}, {498, 332}, {498, 347}};

int puntos_libres(const App& a) {
    int usados = 0;
    for (int i = 0; i < 6; ++i) usados += a.extra[i];
    int base = 0;
    for (int i = 0; i < 6; ++i) base += BASE[a.clase_nueva][i];
    return TOTAL_STATS - base - usados;
}

const char* motivo_crear(int c) {
    switch (c) {
    case 6: return "Ese nombre ya existe.";
    case 9: return "Nombre inválido.";
    case 21: return "Reparto de puntos inválido o sin ranuras libres.";
    default: return "No se pudo crear el personaje.";
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
    if (puntos_libres(a) > 0 && a.extra[i] < BONUS[a.clase_nueva]) ++a.extra[i];
}

// Pantalla original (draw_new_char.cpp). Igual que crear_simple.
bool crear_original(App& a, bool& volver) {
    Graficos& g = *a.g;
    Gpu& gpu = g.gpu();
    const EntradaFrame& e = a.entrada;
    gpu.dibujar(g.img(824), 0, 0, clave_orden(CAPA_FONDO, 0));
    for (int c = 0; c < 7; ++c) {
        if (radio(g, e, IMG_CLASE[c], POS_CLASE[c][0], POS_CLASE[c][1], a.clase_nueva == c)) elegir_clase(a, c);
    }
    if (radio(g, e, 306, 348, 248, a.sexo_nuevo == 0)) elegir_sexo(a, 0);
    if (radio(g, e, 304, 533, 248, a.sexo_nuevo == 1)) elegir_sexo(a, 1);

    const u16* prev = PREVIA[a.clase_nueva * 2 + a.sexo_nuevo];
    gpu.dibujar(g.png(prev[0] + static_cast<u32>(((e.ahora_ms - a.t_anim) / 67) % prev[1])), 410, 0, clave_orden(CAPA_UI, 0));

    // Stats con botones de bajar (556) y subir (554).
    char t[16];
    for (int i = 0; i < 6; ++i) {
        int base = BASE[a.clase_nueva][i];
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
    g.texto("New character", 20, 18, kt, ui::COLOR_TITULO);

    ui::recuadro(g, 20, 38, 180, 384);
    g.texto("Class", 32, 48, kt, ui::COLOR_TITULO);
    for (int c = 0; c < 7; ++c) {
        const int y = 66 + c * 26;
        const bool encima = ui::sobre(e, 26, y, 168, 22);
        if (c == a.clase_nueva || encima)
            g.gpu().rectangulo(26, y, 168, 22, clave_orden(CAPA_UI, 3), c == a.clase_nueva ? rgba(90, 70, 40, 230) : rgba(50, 40, 28, 230));
        g.texto(nombre_clase(c), 36, y + 5, kt, c == a.clase_nueva ? ui::COLOR_TEXTO : tenue);
        if (encima && e.clic) elegir_clase(a, c);
    }
    g.texto("Sex", 32, 262, kt, ui::COLOR_TITULO);
    const char* sexos[2] = {"Male", "Female"};
    for (int s = 0; s < 2; ++s) {
        const int y = 280 + s * 26;
        const bool encima = ui::sobre(e, 26, y, 168, 22);
        if (s == a.sexo_nuevo || encima)
            g.gpu().rectangulo(26, y, 168, 22, clave_orden(CAPA_UI, 3), s == a.sexo_nuevo ? rgba(90, 70, 40, 230) : rgba(50, 40, 28, 230));
        g.texto(sexos[s], 36, y + 5, kt, s == a.sexo_nuevo ? ui::COLOR_TEXTO : tenue);
        if (encima && e.clic) elegir_sexo(a, s);
    }

    ui::recuadro(g, 214, 38, 406, 384);
    const u16* prev = PREVIA[a.clase_nueva * 2 + a.sexo_nuevo];
    g.gpu().dibujar(g.png(prev[0] + static_cast<u32>(((e.ahora_ms - a.t_anim) / 67) % prev[1])), 450, 30, clave_orden(CAPA_UI, 4));

    // Atributos en el orden del paquete: fue, des, con, sab, car, int.
    const char* nombres[6] = {"STR", "DEX", "CON", "WIS", "CHA", "INT"};
    g.texto("Attributes", 228, 48, kt, ui::COLOR_TITULO);
    for (int i = 0; i < 6; ++i) {
        const int y = 70 + i * 28;
        g.texto(nombres[i], 232, y + 4, kt, tenue);
        if (ui::boton_texto(g, e, "-", 280, y, 22, 20)) bajar_stat(a, i);
        std::snprintf(t, sizeof t, "%2d", BASE[a.clase_nueva][i] + a.extra[i]);
        g.texto(t, 312, y + 4, kt, a.extra[i] ? rgba(255, 230, 150) : ui::COLOR_TEXTO);
        if (ui::boton_texto(g, e, "+", 336, y, 22, 20)) subir_stat(a, i);
    }
    std::snprintf(t, sizeof t, "Points left: %d", puntos_libres(a));
    g.texto(t, 232, 244, kt, puntos_libres(a) ? rgba(255, 230, 150) : tenue);

    g.texto("Name", 232, 300, kt, ui::COLOR_TITULO);
    ui::campo(g, e, a.nombre_nuevo, 280, 300, 150, true, false, 16);
    if (!a.aviso.empty()) g.texto(a.aviso, 232, 334, kt, rgba(255, 120, 100));
    g.texto("F2: original interface", 20, 449, kt, rgba(120, 110, 90));

    volver = ui::boton_texto(g, e, "Back", 528, 444, 92, 22);
    return ui::boton_texto(g, e, "Create", 428, 444, 92, 22);
}

}  // namespace

void frame_crear(App& a) {
    const EntradaFrame& e = a.entrada;
    bool volver = false;
    bool aceptar = a.cfg.ui_simple ? crear_simple(a, volver) : crear_original(a, volver);
    aceptar |= e.enter;
    volver |= e.escape;

    // Automatización: repartir los puntos libres en orden y aceptar.
    if (a.aut.activa && a.frames_en_pantalla == 30 && a.aut.pantalla_captura != Pantalla::CREAR) {
        a.clase_nueva = a.aut.clase;
        for (int i = 0; i < 6; ++i) a.extra[i] = 0;
        for (int i = 0; i < 6 && puntos_libres(a) > 0; ++i)
            while (puntos_libres(a) > 0 && a.extra[i] < BONUS[a.clase_nueva]) ++a.extra[i];
        aceptar = true;
    }
    if (aceptar) {
        if (puntos_libres(a) != 0) {
            a.aviso = "Hay que repartir todos los puntos.";
        } else if (a.nombre_nuevo.empty()) {
            a.aviso = "Falta el nombre.";
        } else {
            u8 stats[6];
            for (int i = 0; i < 6; ++i) stats[i] = static_cast<u8>(BASE[a.clase_nueva][i] + a.extra[i]);
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

}  // namespace lin

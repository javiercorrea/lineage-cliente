// Entregable 2: creación de personaje. Imágenes y posiciones de drawmode/draw_new_char.cpp de
// l1j-client; reglas de C_CreateChar.java (L1J-JP): cada stat entre la base de la clase y
// base + bonus de la clase, y la suma de los seis debe ser 75.
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

}  // namespace

void frame_crear(App& a) {
    Graficos& g = *a.g;
    Gpu& gpu = g.gpu();
    EntradaFrame& e = a.entrada;
    gpu.dibujar(g.img(824), 0, 0, clave_orden(CAPA_FONDO, 0));

    for (int c = 0; c < 7; ++c) {
        if (radio(g, e, IMG_CLASE[c], POS_CLASE[c][0], POS_CLASE[c][1], a.clase_nueva == c)) {
            a.clase_nueva = c;
            std::memset(a.extra, 0, sizeof a.extra);
            a.t_anim = e.ahora_ms;
        }
    }
    if (radio(g, e, 306, 348, 248, a.sexo_nuevo == 0)) { a.sexo_nuevo = 0; a.t_anim = e.ahora_ms; }
    if (radio(g, e, 304, 533, 248, a.sexo_nuevo == 1)) { a.sexo_nuevo = 1; a.t_anim = e.ahora_ms; }

    const u16* prev = PREVIA[a.clase_nueva * 2 + a.sexo_nuevo];
    gpu.dibujar(g.png(prev[0] + static_cast<u32>(((e.ahora_ms - a.t_anim) / 67) % prev[1])), 410, 0, clave_orden(CAPA_UI, 0));

    // Stats con botones de bajar (556) y subir (554).
    int libres = puntos_libres(a);
    char t[16];
    for (int i = 0; i < 6; ++i) {
        int base = BASE[a.clase_nueva][i];
        if (ui::boton(g, e, 556, POS_BAJAR[i][0], POS_BAJAR[i][1]) && a.extra[i] > 0) --a.extra[i];
        if (ui::boton(g, e, 554, POS_BAJAR[i][0] + 11, POS_BAJAR[i][1]) && libres > 0 && a.extra[i] < BONUS[a.clase_nueva])
            ++a.extra[i];
        std::snprintf(t, sizeof t, "%2d", base + a.extra[i]);
        g.texto(t, POS_STAT[i][0], POS_STAT[i][1], clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
    }
    std::snprintf(t, sizeof t, "%2d", puntos_libres(a));
    g.texto(t, 468, 334, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));

    ui::caja_texto(g, e, a.nombre_nuevo, 440, 381, 120, true, false, 16);
    if (!a.aviso.empty()) g.texto_sombra(a.aviso, 332, 460, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 120, 100));

    // Automatización: repartir los puntos libres en orden y aceptar.
    bool aceptar = ui::boton(g, e, 825, 476, 403) || e.enter;
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
    if (ui::boton(g, e, 827, 476, 430) || e.escape) {
        a.aviso.clear();
        cambiar_pantalla(a, Pantalla::PERSONAJES);
    }

    // Resultado de la creación (S_CharCreateStatus); el éxito llega como PERSONAJE_NUEVO.
    for (const Evento& ev : a.eventos)
        if (ev.tipo == Ev::RESULTADO_CREAR && ev.v[0] != 2) a.aviso = motivo_crear(ev.v[0]);
}

}  // namespace lin

// Entregable 2: selección de personaje. Imágenes y posiciones de drawmode/draw_char_sel.cpp y
// widgets/sdl_animate_button.cpp / sdl_char_info.cpp de l1j-client:
// fondo 815.png, 4 ranuras con la animación PNG de cada clase y sexo, panel de datos. Con la UI
// simple (F2), una lista de texto con todos los personajes y la animación del elegido al lado.
#include "app/app.h"

namespace lin {

namespace {

// Animación de ranura por (clase × 2 + sexo): primer PNG, cantidad, PNG quieto y resaltado.
struct AnimRanura {
    u16 primero, cantidad, quieto, resaltado;
};
constexpr AnimRanura ANIM[] = {
    {714, 86, 799, 801},  {629, 82, 711, 713},   // príncipe / princesa
    {378, 71, 449, 451},  {315, 60, 375, 377},   // caballero
    {245, 67, 312, 314},  {166, 76, 242, 244},   // elfo
    {531, 95, 626, 628},  {452, 76, 528, 530},   // mago
    {90, 73, 163, 165},   {25, 62, 87, 89},      // elfo oscuro
    {841, 65, 906, 907},  {908, 58, 966, 967},   // caballero dragón
    {969, 68, 1037, 1038}, {1039, 87, 1126, 1127},  // ilusionista
};
constexpr AnimRanura ANIM_VACIA = {1, 24, 0, 0};  // "nuevo personaje"
constexpr int X_RANURA[4] = {0x013, 0x0b0, 0x14d, 0x1ea};
constexpr int ANCHO_RANURA = 150, ALTO_RANURA = 250;
constexpr int MS_FRAME_RANURA = 67;

const AnimRanura& anim_de(const PersonajeLista* p) {
    if (!p) return ANIM_VACIA;
    int i = p->v[PJ_CLASE] * 2 + p->v[PJ_SEXO];
    return (i >= 0 && i < 14) ? ANIM[i] : ANIM_VACIA;
}

bool hay_elegido(const App& a) { return a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size()); }

void entrar_juego(App& a) {
    enviar(a, cmd::entrar(a.op, a.pjs[a.pj_sel].nombre));
    cambiar_pantalla(a, Pantalla::CARGANDO);
}

void volver_login(App& a) {
    a.red.cerrar();
    a.estado_red_previo = EstadoRed::CERRADA;
    a.esperando = false;
    cambiar_pantalla(a, Pantalla::LOGIN);
}

void nuevo_personaje(App& a) {
    a.nombre_nuevo.clear();
    a.clase_nueva = 0;
    a.sexo_nuevo = 0;
    std::memset(a.extra, 0, sizeof a.extra);
    cambiar_pantalla(a, Pantalla::CREAR);
}

// UI simple: lista a la izquierda (un renglón por ranura; las libres crean), y a la derecha la
// animación y los datos del elegido. Borrar pide un segundo clic.
constexpr int FILA_PJ = 40, LISTA_X = 20, LISTA_Y = 44, LISTA_W = 290, DATOS_X = 330, DATOS_W = 290;

void seleccion_simple(App& a) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 0);
    char t[96];
    g.gpu().rectangulo(0, 0, g.gpu().ancho(), g.gpu().alto(), clave_orden(CAPA_FONDO, 0), ui::COLOR_FONDO_SIMPLE);
    g.texto("Select a character", LISTA_X, 18, kt, ui::COLOR_TITULO);
    ui::recuadro(g, LISTA_X, LISTA_Y - 6, LISTA_W, 384);
    ui::recuadro(g, DATOS_X, LISTA_Y - 6, DATOS_W, 384);

    const int ranuras = std::min(9, std::max(a.max_pj, static_cast<int>(a.pjs.size())));
    for (int i = 0; i < ranuras; ++i) {
        const int y = LISTA_Y + i * FILA_PJ;
        const PersonajeLista* p = i < static_cast<int>(a.pjs.size()) ? &a.pjs[i] : nullptr;
        const bool encima = ui::sobre(e, LISTA_X + 4, y, LISTA_W - 8, FILA_PJ - 4);
        if (i == a.pj_sel || encima)
            g.gpu().rectangulo(LISTA_X + 4, y, LISTA_W - 8, FILA_PJ - 4, clave_orden(CAPA_UI, 3),
                               i == a.pj_sel ? rgba(90, 70, 40, 230) : rgba(50, 40, 28, 230));
        if (p) {
            g.texto(p->nombre, LISTA_X + 12, y + 5, kt, ui::COLOR_TEXTO);
            std::snprintf(t, sizeof t, "Lv %d %s%s%s", p->v[PJ_NIVEL], nombre_clase(p->v[PJ_CLASE]), p->clan.empty() ? "" : "  -  ",
                          p->clan.c_str());
            g.texto(t, LISTA_X + 12, y + 20, kt, rgba(170, 160, 140));
        } else {
            g.texto("+ New character", LISTA_X + 12, y + 12, kt, encima ? ui::COLOR_TITULO : rgba(120, 110, 90));
        }
        if (encima && e.clic) {
            if (!p) return nuevo_personaje(a);
            if (a.pj_sel != i) a.t_anim = e.ahora_ms;
            a.pj_sel = i;
            a.confirmar_borrado = false;
        }
    }

    if (hay_elegido(a)) {
        const PersonajeLista& p = a.pjs[a.pj_sel];
        const AnimRanura& an = anim_de(&p);
        const u32 imagen = an.primero + static_cast<u32>((e.ahora_ms - a.t_anim) / MS_FRAME_RANURA % an.cantidad);
        g.gpu().dibujar(g.png(imagen), DATOS_X + (DATOS_W - ANCHO_RANURA) / 2, LISTA_Y - 20, clave_orden(CAPA_UI, 4));
        ui::texto_centrado(g, p.nombre, DATOS_X, LISTA_Y + 226, DATOS_W, ui::COLOR_TITULO);
        const struct { const char* n; int v; } datos[] = {
            {"HP", p.v[PJ_HP]},     {"STR", p.v[PJ_FUE]}, {"MP", p.v[PJ_MP]},   {"DEX", p.v[PJ_DES]},
            {"AC", p.v[PJ_CA]},     {"CON", p.v[PJ_CON]}, {"Lawful", p.v[PJ_LEGAL]}, {"WIS", p.v[PJ_SAB]},
            {"Level", p.v[PJ_NIVEL]}, {"CHA", p.v[PJ_CAR]}, {"", 0},             {"INT", p.v[PJ_INT]},
        };
        for (int k = 0; k < 12; ++k) {
            if (!*datos[k].n) continue;
            const int x = DATOS_X + 24 + (k % 2) * 140, y = LISTA_Y + 248 + (k / 2) * 16;
            g.texto(datos[k].n, x, y, kt, rgba(170, 160, 140));
            std::snprintf(t, sizeof t, "%d", datos[k].v);
            g.texto(t, x + 60, y, kt, ui::COLOR_TEXTO);
        }
    } else {
        ui::texto_centrado(g, "Choose a character from the list", DATOS_X, LISTA_Y + 170, DATOS_W, rgba(120, 110, 90));
    }

    const int by = 444;
    if ((ui::boton_texto(g, e, "Enter", DATOS_X, by, 92, 22) || e.enter) && hay_elegido(a)) return entrar_juego(a);
    if (ui::boton_texto(g, e, a.confirmar_borrado ? "Sure?" : "Delete", DATOS_X + 99, by, 92, 22) && hay_elegido(a)) {
        if (a.confirmar_borrado) enviar(a, cmd::borrar_personaje(a.op, a.pjs[a.pj_sel].nombre));
        a.confirmar_borrado = !a.confirmar_borrado;
    }
    if (ui::boton_texto(g, e, "Back", DATOS_X + 198, by, 92, 22) || e.escape) return volver_login(a);
    g.texto("F2: original interface", LISTA_X, by + 5, kt, rgba(120, 110, 90));
}

}  // namespace

const char* nombre_clase(int c) {
    static const char* n[] = {"Prince", "Knight", "Elf", "Wizard", "Dark Elf", "Dragon Knight", "Illusionist"};
    return (c >= 0 && c < 7) ? n[c] : "?";
}

void eventos_personajes(App& a, const Evento& e) {
    switch (e.tipo) {
    case Ev::CANTIDAD_PERSONAJES:
        // También al volver desde el juego (C_ChangeChar): lo del personaje anterior ya no vale.
        a.mundo.limpiar();
        a.cantidad_pj = e.v[0];
        a.max_pj = e.v[1];
        a.pjs.clear();
        a.pj_sel = -1;
        a.pagina = 0;
        a.esperando = false;
        cambiar_pantalla(a, Pantalla::PERSONAJES);
        break;
    case Ev::PERSONAJE:
    case Ev::PERSONAJE_NUEVO: {
        PersonajeLista p;
        p.nombre.assign(e.s[0]);
        p.clan.assign(e.s[1]);
        std::memcpy(p.v, e.v, sizeof p.v);
        a.pjs.push_back(std::move(p));
        if (e.tipo == Ev::PERSONAJE_NUEVO) {
            a.pj_sel = static_cast<int>(a.pjs.size()) - 1;
            a.t_anim = a.entrada.ahora_ms;
            cambiar_pantalla(a, Pantalla::PERSONAJES);
        }
        break;
    }
    case Ev::PERSONAJE_BORRADO:
        if (a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size())) a.pjs.erase(a.pjs.begin() + a.pj_sel);
        a.pj_sel = -1;
        break;
    case Ev::ENTRAR_JUEGO:
        a.mascotas.clear();  // las de esta sesión llegan con S_PetCtrlMenu (y los marcadores, con S_Bookmarks)
        a.marcadores.clear();
        a.mascota_eligiendo = 0;
        cambiar_pantalla(a, Pantalla::JUEGO);
        break;
    default: break;
    }
}

void frame_personajes(App& a) {
    Graficos& g = *a.g;
    Gpu& gpu = g.gpu();
    EntradaFrame& e = a.entrada;

    // Automatización: elegir (o crear) el personaje pedido y entrar.
    if (a.aut.activa && a.frames_en_pantalla == 10) {
        int elegido = a.pjs.empty() ? -1 : 0;
        if (!a.aut.personaje.empty()) {
            elegido = -1;
            for (size_t i = 0; i < a.pjs.size(); ++i)
                if (a.pjs[i].nombre == a.aut.personaje) elegido = static_cast<int>(i);
        }
        if (elegido >= 0) {
            a.pj_sel = elegido;
            if (a.aut.pantalla_captura != Pantalla::PERSONAJES) return entrar_juego(a);
        } else if (a.aut.crear || a.aut.pantalla_captura == Pantalla::CREAR) {
            a.nombre_nuevo = a.aut.personaje.empty() ? "Prueba" : a.aut.personaje;
            cambiar_pantalla(a, Pantalla::CREAR);
            return;
        }
    }

    if (a.cfg.ui_simple) return seleccion_simple(a);
    gpu.dibujar(g.png(815), 0, 0, clave_orden(CAPA_FONDO, 0));
    for (int i = 0; i < 4; ++i) {
        int idx = a.pagina * 4 + i;
        const PersonajeLista* p = idx < static_cast<int>(a.pjs.size()) ? &a.pjs[idx] : nullptr;
        bool disponible = p || idx < a.max_pj;
        if (!disponible) continue;
        const AnimRanura& an = anim_de(p);
        bool encima = ui::sobre(e, X_RANURA[i], 0, ANCHO_RANURA, ALTO_RANURA);
        bool seleccionado = (idx == a.pj_sel);
        u32 imagen;
        if (seleccionado || (!p && encima)) {
            u64 t = (a.entrada.ahora_ms - a.t_anim) / MS_FRAME_RANURA;
            imagen = an.primero + static_cast<u32>(t % an.cantidad);
        } else {
            imagen = encima ? an.resaltado : an.quieto;
        }
        gpu.dibujar(g.png(imagen), X_RANURA[i], 0, clave_orden(CAPA_UI, 0));
        if (encima && e.clic) {
            if (p) {
                if (a.pj_sel != idx) a.t_anim = a.entrada.ahora_ms;
                a.pj_sel = idx;
            } else {
                return nuevo_personaje(a);
            }
        }
    }

    // Panel de datos del personaje seleccionado (sdl_char_info.cpp).
    if (hay_elegido(a)) {
        const PersonajeLista& p = a.pjs[a.pj_sel];
        u64 k = clave_orden(CAPA_UI_TEXTO, 0);
        u32 c = rgba(255, 255, 254);
        char t[32];
        auto num = [&](int v, int x, int y) {
            std::snprintf(t, sizeof t, "%d", v);
            g.texto(t, x, y, k, c);
        };
        g.texto(p.nombre, 167, 297, k, c);
        g.texto(p.clan, 167, 316, k, c);
        g.texto(nombre_clase(p.v[PJ_CLASE]), 167, 340, k, c);
        num(p.v[PJ_LEGAL], 167, 360);
        num(p.v[PJ_HP], 167, 397);
        num(p.v[PJ_MP], 167, 419);
        num(p.v[PJ_CA], 167, 441);
        num(p.v[PJ_NIVEL], 403, 315);
        num(p.v[PJ_FUE], 403, 350);
        num(p.v[PJ_DES], 403, 369);
        num(p.v[PJ_CON], 403, 387);
        num(p.v[PJ_SAB], 404, 405);
        num(p.v[PJ_CAR], 403, 423);
        num(p.v[PJ_INT], 404, 442);
    }

    // Botones: páginas, entrar, volver, borrar.
    int paginas = (std::max(a.max_pj, static_cast<int>(a.pjs.size())) + 3) / 4;
    if (ui::boton(g, e, 0x6e5, 0x0f7, 0x10b) && a.pagina > 0) --a.pagina;
    if (ui::boton(g, e, 0x6e7, 0x16c, 0x10b) && a.pagina + 1 < paginas) ++a.pagina;
    bool entrar = ui::boton(g, e, 0x334, 0x20d, 0x185) || (e.enter && a.pj_sel >= 0);
    if (entrar && hay_elegido(a)) return entrar_juego(a);
    if (ui::boton(g, e, 0x336, 0x20d, 0x19a) || e.escape) return volver_login(a);
    if (ui::boton(g, e, 0x134, 0x20d, 0x1b5) && hay_elegido(a))
        enviar(a, cmd::borrar_personaje(a.op, a.pjs[a.pj_sel].nombre));
}

}  // namespace lin

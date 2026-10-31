// Entregable 7: NPCs y comercio.
//  - Hablar: clic sobre un NPC (no monstruo, ítem ni personaje) camina hasta él y manda C_NPCTalk;
//    el servidor responde con la página (S_NPCTalkReturn) y se muestra en el diálogo, a la
//    izquierda. Los vínculos "link" cambian de página sin pasar por el servidor; los "action"
//    se mandan con C_NPCAction ("buy", "sell", "retrieve", "teleport ..."). "deposit" lo resuelve
//    el cliente: abre el almacén con el propio inventario.
//  - Tienda y almacén: S_ShopSellList (comprar), S_ShopBuyList (vender) y S_RetrieveList
//    (retirar) abren la lista en la misma ventana. Clic suma 1 (con Shift, 10 o todo), clic
//    derecho resta 1 (con Shift, nada); OK manda C_Result.
//  - Teletransporte: el servidor manda S_Teleport y espera C_Teleport para mover al personaje.
//  - Maestros de magia: el vínculo "buyspell" no va con C_NPCAction sino con C_SkillBuy; la lista
//    (S_SkillBuy) se elige como una tienda (un hechizo por fila) y OK manda C_SkillBuyOK.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr int MARGEN = 14, ARRIBA = 30;
constexpr int ANCHO_PAGINA = ui::VENTANA_ANCHO - 2 * MARGEN;
constexpr int ALTO_PAGINA = ui::VENTANA_ALTO - ARRIBA - 12;
constexpr int FILA_TIENDA = 34, FILAS_TIENDA = 6;
constexpr int DISTANCIA_CIERRE = 6;  // al alejarse más, el diálogo se cierra
constexpr u32 PRECIO_RETIRO = 30;    // adena por ítem retirado (C_Result del servidor)

bool medir_imagen(void* ctx, u32 n, int& w, int& h) {
    const RegionAtlas& r = static_cast<App*>(ctx)->g->img(n);
    if (!r.valida()) return false;
    w = r.w;
    h = r.h;
    return true;
}

void abrir_pagina(App& a, ObjId npc, std::string_view id, std::string_view datos) {
    std::vector<u8> html;
    std::string nombre = std::string(id) + "-e.html";
    if (!a.rec.texto(nombre, html)) {
        nombre = std::string(id) + ".html";
        if (!a.rec.texto(nombre, html)) {
            std::string aviso = "(page \"" + std::string(id) + "\" not found)";
            html.assign(aviso.begin(), aviso.end());
        }
    }
    if (g_traza_html)
        std::fprintf(stderr, "[html %.*s] datos=\"%.*s\"\n%.*s\n", static_cast<int>(id.size()), id.data(), static_cast<int>(datos.size()),
                     datos.data(), static_cast<int>(html.size()), reinterpret_cast<const char*>(html.data()));
    a.npc = npc;
    a.npc_pagina = id;
    // Los datos que son un "$N" (el nombre del mayordomo en "agpay", por ejemplo) van traducidos.
    // (`datos` puede ser a.npc_datos: los vínculos locales vuelven a pasar los mismos.)
    std::string traducidos;
    for (size_t i = 0; i <= datos.size();) {
        const size_t fin = std::min(datos.find(SEPARADOR_DATOS, i), datos.size());
        if (i) traducidos += SEPARADOR_DATOS;
        traducidos += a.desc.resolver(datos.substr(i, fin - i));
        i = fin + 1;
    }
    a.npc_datos = std::move(traducidos);
    a.npc_desplazamiento = 0;
    a.tienda = TipoTienda::NINGUNA;
    maquetar_html({reinterpret_cast<const char*>(html.data()), html.size()}, a.npc_datos, SEPARADOR_DATOS,
                  nombre_personaje(a), ANCHO_PAGINA, medir_imagen, &a, a.npc_html);
}

void cerrar(App& a) {
    a.npc = 0;
    a.tienda = TipoTienda::NINGUNA;
    a.tienda_filas.clear();
}

}  // namespace

// Adena en el inventario (el ítem "$4"; no "$40...").
u32 adena_inventario(const App& a) {
    const TablaItems& t = a.mundo.inventario.items;
    for (Slot s = 0; s < t.tamano(); ++s) {
        std::string_view n = nombre_item(a.mundo.inventario, s);
        if (n.substr(0, 2) == "$4" && (n.size() == 2 || n[2] == ' ')) return t.col<I_CANTIDAD>()[s];
    }
    return 0;
}

namespace {

void abrir_tienda(App& a, ObjId npc, TipoTienda tipo) {
    a.tienda = tipo;
    a.tienda_npc = npc;
    a.tienda_filas.clear();
    a.tienda_desplazamiento = 0;
    if (tipo == TipoTienda::DEPOSITO) {  // del propio inventario, sin lo equipado
        const TablaItems& t = a.mundo.inventario.items;
        for (Slot s = 0; s < t.tamano(); ++s) {
            std::string_view n = nombre_item(a.mundo.inventario, s);
            if (item_equipado(n)) continue;
            FilaTienda f;
            f.id = t.ids()[s];
            f.gfx = t.col<I_GFX>()[s];
            f.maximo = t.col<I_CANTIDAD>()[s];
            f.bendicion = t.col<I_BENDICION>()[s];
            f.nombre = n;
            a.tienda_filas.push_back(std::move(f));
        }
    }
}

}  // namespace

// Un vínculo de la página: otra página local, el almacén o una acción para el servidor.
void activar(App& a, std::string_view vinculo, bool es_enlace) {
    if (vinculo.empty()) return;
    if (es_enlace) {
        abrir_pagina(a, a.npc, vinculo, a.npc_datos);
    } else if (vinculo == "buyspell") {
        enviar(a, cmd::pedir_magia(a.op, a.npc));
    } else if (vinculo == "fix") {  // herrero: la lista de armas dañadas (S_FixWeaponList)
        enviar(a, cmd::armas_a_reparar(a.op));
    } else if (vinculo == "deposit") {
        abrir_tienda(a, a.npc, TipoTienda::DEPOSITO);
    } else if (vinculo.substr(0, 6) == "raise-") {  // RaiseAttr: subir un atributo (C_Attr 479)
        enviar(a, cmd::subir_atributo(a.op, vinculo.substr(6)));
        cerrar(a);
    } else if (vinculo == "asktime") {  // chambelán: la fecha del próximo asedio (castillos_ui.cpp)
        enviar(a, cmd::fecha_asedio(a.op));
    } else if (a.npc_casa && (vinculo == "map" || vinculo == "apply")) {  // tablón de subastas: con la casa
        enviar(a, cmd::accion_npc(a.op, a.npc, vinculo, std::to_string(a.npc_casa)));
    } else {
        enviar(a, cmd::accion_npc(a.op, a.npc, vinculo));
    }
}

namespace {

// Total de la selección: lo que se paga (compra, retiro) o se cobra (venta).
u64 total(const App& a) {
    u64 t = 0;
    for (const FilaTienda& f : a.tienda_filas) {
        if (a.tienda == TipoTienda::RETIRO) t += f.elegido ? PRECIO_RETIRO : 0;
        else t += static_cast<u64>(f.precio) * f.elegido;
    }
    return t;
}

void confirmar(App& a) {
    if (a.tienda == TipoTienda::MASCOTA || a.tienda == TipoTienda::REPARAR) {
        cerrar(a);
        return;
    }
    if (a.tienda == TipoTienda::MAGIA) {
        u32 indices[64];
        size_t n = 0;
        for (const FilaTienda& f : a.tienda_filas)
            if (f.elegido && n < 64) indices[n++] = f.id;
        if (n) enviar(a, cmd::aprender_magia(a.op, {indices, n}));
        cerrar(a);
        return;
    }
    LineaResultado lineas[255];
    size_t n = 0;
    for (const FilaTienda& f : a.tienda_filas)
        if (f.elegido && n < 255) lineas[n++] = {f.id, f.elegido};
    static constexpr TipoResultado TIPOS[] = {RESULTADO_COMPRA, RESULTADO_COMPRA, RESULTADO_VENTA, RESULTADO_RETIRO,
                                              RESULTADO_DEPOSITO};
    if (n) enviar(a, cmd::resultado(a.op, a.tienda_npc, TIPOS[static_cast<int>(a.tienda)], {lineas, n}));
    cerrar(a);
}

size_t nombre_fila(const App& a, const FilaTienda& f, char* buf, size_t cap) {
    return expandir_desc(a.desc, f.nombre, buf, cap);
}

int distancia_npc(const App& a) {
    const TablaObjetos& o = a.mundo.objetos;
    Slot yo = o.buscar(a.mundo.jugador.id), n = o.buscar(a.tienda != TipoTienda::NINGUNA ? a.tienda_npc : a.npc);
    if (yo == SIN_SLOT || n == SIN_SLOT) return 1 << 20;
    return std::max(std::abs(o.col<O_X>()[yo] - o.col<O_X>()[n]), std::abs(o.col<O_Y>()[yo] - o.col<O_Y>()[n]));
}

void dibujar_pagina(App& a, int x0, int y0) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const PaginaHtml& p = a.npc_html;
    const int max_desp = std::max(0, p.alto - ALTO_PAGINA);
    if (sobre_npc(a) && e.rueda) a.npc_desplazamiento = std::clamp(a.npc_desplazamiento - e.rueda * 30, 0, max_desp);
    a.npc_desplazamiento = std::clamp(a.npc_desplazamiento, 0, max_desp);
    const int ox = x0 + MARGEN, oy = y0 + ARRIBA - a.npc_desplazamiento;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 12), ki = clave_orden(CAPA_UI, 13);
    const ElementoHtml* bajo = nullptr;
    for (const ElementoHtml& el : p.elementos) {
        const int x = ox + el.x, y = oy + el.y;
        // Fuera de la vista (por la primera línea de texto: un botón un poco más alto que el resto
        // de su línea no se pierde).
        if (y < y0 + ARRIBA - 2 || y + ALTO_GLIFO > y0 + ARRIBA + ALTO_PAGINA + 2) continue;
        const bool encima = el.vinculo_len && ui::sobre(e, x, y - 1, el.w, el.h + 2);
        if (encima) bajo = &el;
        if (el.tipo == ElementoHtml::Tipo::IMAGEN) {
            // Botones: N.img y, con el puntero encima, N+1 si existe.
            const RegionAtlas& alt = encima ? g.img(el.imagen + 1) : g.img(el.imagen);
            g.gpu().dibujar(alt.valida() ? alt : g.img(el.imagen), x, y, ki);
        } else {
            g.texto(p.texto(el), static_cast<float>(x), static_cast<float>(y), kt, encima ? rgba(255, 255, 254) : el.color);
            if (el.vinculo_len) g.gpu().rectangulo(x, y + ALTO_GLIFO, el.w, 1, ki, el.color);
        }
    }
    if (max_desp > 0) {  // barra de desplazamiento
        int h = ALTO_PAGINA * ALTO_PAGINA / p.alto, yb = y0 + ARRIBA + (ALTO_PAGINA - h) * a.npc_desplazamiento / max_desp;
        g.gpu().rectangulo(x0 + ui::VENTANA_ANCHO - 8, yb, 3, h, ki, rgba(160, 130, 80, 200));
    }
    if (bajo) {
        std::string_view ayuda = p.ayuda(*bajo);
        if (!ayuda.empty()) {
            int w = ancho_texto(ayuda.data(), ayuda.size());
            g.gpu().rectangulo(e.mx + 10, e.my + 14, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
            g.texto(ayuda, static_cast<float>(e.mx + 14), static_cast<float>(e.my + 17), clave_orden(CAPA_UI_TEXTO, 30));
        }
        if (e.clic) activar(a, p.vinculo(*bajo), bajo->es_enlace);
    }
}

void dibujar_tienda(App& a, int x0, int y0) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 12), kf = clave_orden(CAPA_UI, 12), ki = clave_orden(CAPA_UI, 13);
    const int n = static_cast<int>(a.tienda_filas.size());
    const u32 max_desp = n > FILAS_TIENDA ? static_cast<u32>(n - FILAS_TIENDA) : 0;
    if (sobre_npc(a) && e.rueda) {
        i64 d = static_cast<i64>(a.tienda_desplazamiento) - e.rueda;
        a.tienda_desplazamiento = static_cast<u32>(std::clamp<i64>(d, 0, max_desp));
    }
    a.tienda_desplazamiento = std::min(a.tienda_desplazamiento, max_desp);
    char buf[256];
    for (int k = 0; k < FILAS_TIENDA; ++k) {
        int i = static_cast<int>(a.tienda_desplazamiento) + k;
        if (i >= n) break;
        FilaTienda& f = a.tienda_filas[static_cast<size_t>(i)];
        const int x = x0 + MARGEN, y = y0 + ARRIBA + k * FILA_TIENDA, w = ui::VENTANA_ANCHO - 2 * MARGEN;
        const bool encima = ui::sobre(e, x, y, w, FILA_TIENDA - 2);
        g.gpu().rectangulo(x, y, w, FILA_TIENDA - 2, kf, f.elegido ? rgba(80, 64, 20, 230) : encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
        g.gpu().dibujar(g.icono(f.gfx), x + 1, y, ki);
        size_t m = nombre_fila(a, f, buf, sizeof buf);
        u32 color = f.bendicion == 1 ? rgba(255, 230, 120) : f.bendicion == 2 ? rgba(255, 110, 100) : rgba(255, 255, 254);
        // El nombre en una línea (cortado si no entra) y debajo precio y cantidad elegida.
        while (m > 0 && ancho_texto(buf, m) > w - 40) --m;
        g.texto({buf, m}, static_cast<float>(x + 38), static_cast<float>(y + 3), kt, color);
        char linea[64];
        if (a.tienda == TipoTienda::DEPOSITO || a.tienda == TipoTienda::RETIRO || a.tienda == TipoTienda::MASCOTA)
            std::snprintf(linea, sizeof linea, "x%u", f.maximo);
        else if (a.tienda == TipoTienda::REPARAR)
            std::snprintf(linea, sizeof linea, "Damage %u: %u adena", f.maximo, f.precio);
        else
            std::snprintf(linea, sizeof linea, "%u adena", f.precio);
        g.texto(linea, static_cast<float>(x + 38), static_cast<float>(y + 17), kt, rgba(170, 160, 140));
        if (f.elegido) {
            std::snprintf(linea, sizeof linea, "%u", f.elegido);
            g.texto(linea, static_cast<float>(x + w - 6 - ancho_texto(linea, std::strlen(linea))), static_cast<float>(y + 17), kt,
                    rgba(255, 230, 120));
        }
        if (encima && e.clic && a.tienda == TipoTienda::REPARAR && f.precio) {
            // Reparar esta arma (C_SelectList; el servidor cobra y la deja sin daño).
            enviar(a, cmd::reparar_arma(a.op, f.id, a.tienda_npc));
            f.precio = 0;
            f.nombre += " (repaired)";
            continue;
        }
        if (encima && e.clic && a.tienda == TipoTienda::MASCOTA) {
            // Usar o equipar el ítem de la mascota (por su fila) y volver a pedir la lista.
            enviar(a, cmd::usar_item_mascota(a.op, a.tienda_npc, static_cast<u8>(i)));
            enviar(a, cmd::inventario_mascota(a.op, a.tienda_npc));
            continue;
        }
        if (encima && e.clic) f.elegido = std::min(f.maximo, f.elegido + (e.shift ? (f.maximo > 10 ? 10u : f.maximo) : 1u));
        if (encima && e.clic_derecho) f.elegido = e.shift ? 0 : (f.elegido ? f.elegido - 1 : 0);
    }
    const int yp = y0 + ARRIBA + FILAS_TIENDA * FILA_TIENDA + 4;
    char pie[96];
    const u64 t = total(a);
    switch (a.tienda) {
    case TipoTienda::MAGIA:
        if (a.tienda_filas.empty()) {
            std::snprintf(pie, sizeof pie, "Nothing to learn now");
            break;
        }
        [[fallthrough]];
    case TipoTienda::COMPRA: std::snprintf(pie, sizeof pie, "Total %llu / %u adena", static_cast<unsigned long long>(t), adena_inventario(a)); break;
    case TipoTienda::VENTA: std::snprintf(pie, sizeof pie, "You get %llu adena", static_cast<unsigned long long>(t)); break;
    case TipoTienda::RETIRO: std::snprintf(pie, sizeof pie, "Fee %llu adena (30 each)", static_cast<unsigned long long>(t)); break;
    case TipoTienda::MASCOTA: std::snprintf(pie, sizeof pie, a.tienda_filas.empty() ? "The pet carries nothing" : "Click: use or equip"); break;
    case TipoTienda::REPARAR: std::snprintf(pie, sizeof pie, a.tienda_filas.empty() ? "No damaged weapons" : "Click a weapon to repair it"); break;
    default: std::snprintf(pie, sizeof pie, "Click: +1  Shift: +10  Right: -1"); break;
    }
    g.texto(pie, static_cast<float>(x0 + MARGEN), static_cast<float>(yp), kt, rgba(214, 190, 140));
    if (ui::boton_texto(g, e, "OK", x0 + MARGEN, yp + 18, 90, 18)) confirmar(a);
    if (ui::boton_texto(g, e, "Cancel", x0 + ui::VENTANA_ANCHO - MARGEN - 90, yp + 18, 90, 18)) cerrar(a);
}

}  // namespace

u8 tipo_objeto(const App& a, Slot s) {
    const GfxId gfx = a.mundo.objetos.col<O_GFX>()[s];
    const u8 tipo = a.sprites.tipo(gfx);
    // Sin el gfx en la tabla de sprites (se dibuja como un bloque magenta): un monstruo de una
    // versión más nueva que esta instalación, así se lo puede atacar; salvo los NPCs de los
    // pueblos, que se llaman "Oficio^Nombre" ("Storage^Dorin") y a los que se les habla.
    if (tipo == 0 && !a.sprites.accion(gfx, ACC_QUIETO) && !a.sprites.accion(gfx, ACC_CAMINAR) &&
        s != a.mundo.objetos.buscar(a.mundo.jugador.id) &&
        a.desc.resolver(texto(a.mundo, a.mundo.objetos.col<O_NOMBRE>()[s])).find('^') == std::string_view::npos)
        return TIPO_MONSTRUO;
    // Asedio: los portones son puertas; la torre de guardia y la corona se atacan con un clic (la
    // corona se toma "atacándola": C_Attack y el servidor llama a su onAction).
    if (tipo == TIPO_ESTRUCTURA) return a.sprites.accion(gfx, ACC_ABRIR) ? TIPO_PUERTA : TIPO_MONSTRUO;
    if (gfx == GFX_CORONA) return TIPO_MONSTRUO;
    return tipo;
}

bool es_npc(const App& a, Slot s) {
    u8 tipo = tipo_objeto(a, s);
    return tipo != TIPO_MONSTRUO && tipo != TIPO_ITEM && tipo != TIPO_PERSONAJE && s != a.mundo.objetos.buscar(a.mundo.jugador.id);
}

void eventos_npc(App& a, const Evento& e) {
    switch (e.tipo) {
    case Ev::HTML:
        if (e.s[0] == "monlist") break;  // Shape Change: forma_ui.cpp
        if (e.s[0].empty()) {
            if (a.npc == e.id) cerrar(a);
            break;
        }
        abrir_pagina(a, e.id, e.s[0], e.s[1]);
        a.npc_casa = static_cast<u32>(e.v[HT_CASA]);
        break;
    case Ev::TELETRANSPORTE:
        if (e.id == a.mundo.jugador.id) {
            enviar(a, cmd::teletransporte(a.op));
            cerrar(a);
        }
        break;
    case Ev::LISTA_TIENDA: {
        static constexpr TipoTienda TIPOS[] = {TipoTienda::COMPRA, TipoTienda::VENTA, TipoTienda::RETIRO, TipoTienda::MASCOTA,
                                               TipoTienda::REPARAR};
        static_assert(std::size(TIPOS) == LISTA_REPARAR + 1);
        if (a.npc == 0) a.npc = e.id;
        // La de reparar no trae el NPC: es el herrero con el que se habla.
        abrir_tienda(a, e.v[LT_TIPO] == LISTA_REPARAR ? a.npc : e.id, TIPOS[e.v[LT_TIPO]]);
        break;
    }
    case Ev::MAGIA_EN_VENTA: {
        // Precio del servidor (C_SkillBuyOK): 100 adena el nivel 1, 400 el 2 y 900 el 3 (k / 8 = nivel - 1).
        abrir_tienda(a, a.npc, TipoTienda::MAGIA);
        for (u32 k = 0; k < 64; ++k) {
            if (!(static_cast<u32>(e.v[MV_MASCARA + k / 32]) >> (k % 32) & 1)) continue;
            const Hechizo& h = a.hechizos[k + 1];
            FilaTienda f;
            f.id = k;
            f.gfx = h.icono;
            f.maximo = 1;
            f.precio = 100 * (k / 8 + 1) * (k / 8 + 1);
            f.nombre = h.existe() ? std::string(h.nombre) : "Spell " + std::to_string(k + 1);
            a.tienda_filas.push_back(std::move(f));
        }
        break;
    }
    case Ev::ITEM_TIENDA: {
        FilaTienda f;
        f.id = e.id;
        f.precio = static_cast<u32>(e.v[TI_PRECIO]);
        if (a.tienda == TipoTienda::VENTA || a.tienda == TipoTienda::REPARAR) {  // solo id y precio: el resto, del inventario
            Slot s = a.mundo.inventario.items.buscar(e.id);
            if (s == SIN_SLOT) break;
            f.gfx = a.mundo.inventario.items.col<I_GFX>()[s];
            f.maximo = a.tienda == TipoTienda::REPARAR ? static_cast<u32>(e.v[IT_CANTIDAD]) : a.mundo.inventario.items.col<I_CANTIDAD>()[s];
            f.bendicion = a.mundo.inventario.items.col<I_BENDICION>()[s];
            f.nombre = nombre_item(a.mundo.inventario, s);
        } else {
            f.gfx = static_cast<u16>(e.v[IT_GFX]);
            f.maximo = a.tienda == TipoTienda::COMPRA ? 9999 : static_cast<u32>(e.v[IT_CANTIDAD]);
            f.bendicion = static_cast<u8>(e.v[IT_BENDICION]);
            f.nombre = e.s[0];
            if (a.tienda == TipoTienda::MASCOTA && e.v[IT_IDENTIFICADO] >= 2) f.nombre += " (worn)";  // 2/3: equipado
        }
        a.tienda_filas.push_back(std::move(f));
        break;
    }
    default: break;
    }
}

bool sobre_npc(const App& a) { return sobre_ventana(a, IdVentana::NPC); }

void procesar_npc(App& a, u64 ahora) {
    if ((a.npc || a.tienda != TipoTienda::NINGUNA) && distancia_npc(a) > DISTANCIA_CIERRE) cerrar(a);
}

void dibujar_npc(App& a) {
    if (!a.npc && a.tienda == TipoTienda::NINGUNA) return;
    Graficos& g = *a.g;
    char titulo[96] = "";
    if (a.tienda != TipoTienda::NINGUNA) {
        static const char* NOMBRES[] = {"", "Buy", "Sell", "Retrieve", "Deposit", "Learn magic", "Pet's bag", "Repair"};
        static_assert(std::size(NOMBRES) == static_cast<size_t>(TipoTienda::REPARAR) + 1);
        std::snprintf(titulo, sizeof titulo, "%s", NOMBRES[static_cast<int>(a.tienda)]);
    } else if (Slot s = a.mundo.objetos.buscar(a.npc); s != SIN_SLOT) {
        size_t n = expandir_desc(a.desc, texto(a.mundo, a.mundo.objetos.col<O_NOMBRE>()[s]), titulo, sizeof titulo - 1);
        titulo[n] = 0;
    }
    arrastrar_ventana(a, IdVentana::NPC);
    const Rect rv = rect_ventana(a, IdVentana::NPC);
    if (ui::ventana(g, a.entrada, a.cfg.ui_simple, rv.x, rv.y, titulo, opacidad_ventanas(a)) || a.entrada.escape) {
        cerrar(a);
        return;
    }
    if (a.tienda != TipoTienda::NINGUNA) dibujar_tienda(a, rv.x, rv.y);
    else dibujar_pagina(a, rv.x, rv.y);
}

}  // namespace lin

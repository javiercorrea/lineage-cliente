#include <algorithm>
#include "ui/ui.h"

namespace lin::ui {

bool sobre(const EntradaFrame& e, int x, int y, int w, int h) { return e.mx >= x && e.my >= y && e.mx < x + w && e.my < y + h; }

void imagen(Graficos& g, u32 n, int x, int y, Capa capa) { g.gpu().dibujar(g.img(n), x, y, clave_orden(capa, 0)); }

bool boton(Graficos& g, const EntradaFrame& e, u32 n, int x, int y) {
    RegionAtlas r = g.img(n);
    bool encima = r.valida() && sobre(e, x + r.ox, y + r.oy, r.w, r.h);
    if (encima) {
        RegionAtlas r2 = g.img(n + 1);
        if (r2.valida()) r = r2;
    }
    g.gpu().dibujar(r, x, y, clave_orden(CAPA_UI, 1));
    return encima && e.clic;
}

bool caja_texto(Graficos& g, const EntradaFrame& e, std::string& valor, int x, int y, int ancho, bool foco,
                bool es_clave, size_t maximo) {
    if (foco) {
        for (int i = 0; i < e.ntexto; ++i) {
            char c = e.texto[i];
            if (c >= 0x20 && c < 0x7F && valor.size() < maximo) valor.push_back(c);
        }
        if (e.retroceso && !valor.empty()) valor.pop_back();
    }
    char buf[64];
    size_t n = valor.size() < sizeof buf - 1 ? valor.size() : sizeof buf - 1;
    for (size_t i = 0; i < n; ++i) buf[i] = es_clave ? '*' : valor[i];
    std::string_view t(buf, n);
    int w = g.texto(t, x, y, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
    // Cursor parpadeante como en el original.
    if (foco && (e.ahora_ms / 500) % 2 == 0 && w + 1 < ancho)
        g.gpu().rectangulo(x + w + 1, y, 1, ALTO_GLIFO, clave_orden(CAPA_UI_TEXTO, 0), rgba(255, 255, 254));
    return foco && e.enter;
}

bool boton_texto(Graficos& g, const EntradaFrame& e, std::string_view t, int x, int y, int w, int h) {
    bool encima = sobre(e, x, y, w, h);
    u64 k = clave_orden(CAPA_UI, 100);  // por encima de los paneles
    g.gpu().rectangulo(x, y, w, h, k, encima ? rgba(90, 70, 40, 230) : rgba(40, 30, 20, 210));
    g.gpu().rectangulo(x, y, w, 1, k + (1u << 24), rgba(160, 130, 80));
    g.gpu().rectangulo(x, y + h - 1, w, 1, k + (1u << 24), rgba(160, 130, 80));
    int tw = ancho_texto(t.data(), t.size());
    g.texto(t, x + (w - tw) / 2, y + (h - ALTO_GLIFO) / 2, clave_orden(CAPA_UI_TEXTO, 100), rgba(255, 240, 200));
    return encima && e.clic;
}

void recuadro(Graficos& g, int x, int y, int w, int h, u8 opacidad) {
    const u32 borde = rgba(150, 130, 90);
    const u64 k = clave_orden(CAPA_UI, 2);
    g.gpu().rectangulo(x, y, w, h, k, rgba(10, 8, 6, opacidad));
    g.gpu().rectangulo(x, y, w, 1, k + (1ull << 24), borde);
    g.gpu().rectangulo(x, y + h - 1, w, 1, k + (1ull << 24), borde);
    g.gpu().rectangulo(x, y, 1, h, k + (1ull << 24), borde);
    g.gpu().rectangulo(x + w - 1, y, 1, h, k + (1ull << 24), borde);
}

bool campo(Graficos& g, const EntradaFrame& e, std::string& valor, int x, int y, int ancho, bool foco, bool es_clave,
           size_t maximo) {
    g.gpu().rectangulo(x - 4, y - 4, ancho + 8, ALTO_GLIFO + 8, clave_orden(CAPA_UI, 3),
                       foco ? rgba(60, 48, 30, 255) : rgba(30, 24, 16, 255));
    g.gpu().rectangulo(x - 4, y + ALTO_GLIFO + 3, ancho + 8, 1, clave_orden(CAPA_UI, 4), rgba(150, 130, 90));
    return caja_texto(g, e, valor, x, y, ancho, foco, es_clave, maximo);
}

void texto_centrado(Graficos& g, std::string_view t, int x, int y, int w, u32 color) {
    g.texto(t, static_cast<float>(x + (w - ancho_texto(t.data(), t.size())) / 2), static_cast<float>(y),
            clave_orden(CAPA_UI_TEXTO, 0), color);
}

bool ventana(Graficos& g, const EntradaFrame& e, bool simple, int x, int y, std::string_view titulo, u8 opacidad) {
    const u8 marco = static_cast<u8>(std::min(255, opacidad + 60));
    constexpr u32 IMG_MARCO = 4131;  // PENDIENTE: las ventanas propias del original (estado, inventario)
    const int w = VENTANA_ANCHO, h = VENTANA_ALTO;
    if (simple) {
        const u32 borde = rgba(150, 130, 90);
        g.gpu().rectangulo(x, y, w, h, clave_orden(CAPA_UI, 10), rgba(10, 8, 6, opacidad));
        g.gpu().rectangulo(x + 1, y + 1, w - 2, ALTO_TITULO - 2, clave_orden(CAPA_UI, 10) + (1ull << 24), rgba(40, 32, 22, marco));
        g.gpu().rectangulo(x, y, w, 1, clave_orden(CAPA_UI, 11), borde);
        g.gpu().rectangulo(x, y + h - 1, w, 1, clave_orden(CAPA_UI, 11), borde);
        g.gpu().rectangulo(x, y, 1, h, clave_orden(CAPA_UI, 11), borde);
        g.gpu().rectangulo(x + w - 1, y, 1, h, clave_orden(CAPA_UI, 11), borde);
        g.texto(titulo, x + 8, y + 6, clave_orden(CAPA_UI_TEXTO, 10), rgba(214, 190, 140));
        g.texto("[x]", x + w - 26, y + 6, clave_orden(CAPA_UI_TEXTO, 10), rgba(255, 255, 254));
    } else {
        // El marco original tiene el interior transparente: se rellena detrás.
        g.gpu().rectangulo(x + 6, y + 24, w - 12, h - 30, clave_orden(CAPA_UI, 10), rgba(14, 12, 10, opacidad));
        g.gpu().dibujar(g.img(IMG_MARCO), x, y, clave_orden(CAPA_UI, 11), rgba(255, 255, 255, marco));
        g.texto(titulo, x + 12, y + 8, clave_orden(CAPA_UI_TEXTO, 10), rgba(214, 190, 140));
    }
    // Cerrar: la X del marco (o el "[x]"), arriba a la derecha.
    return e.clic && sobre(e, x + w - 28, y + 2, 26, 20);
}

}  // namespace lin::ui

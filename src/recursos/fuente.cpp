#include "plataforma/archivo_mapeado.h"
#include "recursos/cp949.h"
#include "recursos/fuente.h"
#include "recursos/fuente_ttf.h"

namespace lin {

bool FuenteBitmap::cargar(const std::string& carpeta) {
    if (!leer_archivo(carpeta + "font/eng.fnt", ascii) || ascii.size() < 95 * 12) return false;
    if (!leer_archivo(carpeta + "font/kor.fnt", coreana) || coreana.size() < 2401 * 24) coreana.clear();
    return true;
}

bool FuenteBitmap::rasterizar(u32 glifo, ImagenRGBA& salida) const {
    if (glifo >= GLIFO_TTF_BASE) return rasterizar_ttf(glifo - GLIFO_TTF_BASE, salida);
    if (glifo < 95) {
        salida.preparar(ANCHO_GLIFO_ASCII, ALTO_GLIFO);
        for (int y = 0; y < ALTO_GLIFO; ++y) {
            u8 fila = ascii[glifo * 12 + y];
            for (int x = 0; x < ANCHO_GLIFO_ASCII; ++x)
                if (fila & (0x80 >> x)) salida.px[y * ANCHO_GLIFO_ASCII + x] = 0xFFFFFFFFu;
        }
        return true;
    }
    u32 k = glifo - 1000;
    if (glifo < 1000 || coreana.empty() || k >= 2401) return false;
    salida.preparar(ANCHO_GLIFO_KOR, ALTO_GLIFO);
    for (int y = 0; y < ALTO_GLIFO; ++y) {
        u16 fila = static_cast<u16>((coreana[k * 24 + y * 2] << 8) | coreana[k * 24 + y * 2 + 1]);
        for (int x = 0; x < ANCHO_GLIFO_KOR; ++x)
            if (fila & (0x8000 >> x)) salida.px[y * ANCHO_GLIFO_KOR + x] = 0xFFFFFFFFu;
    }
    return true;
}

size_t largo_utf8(const char* t, size_t n, size_t i, u32& cp) {
    const u8 c = static_cast<u8>(t[i]);
    const size_t largo = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (largo == 0 || i + largo > n) return 0;
    cp = largo == 1 ? c : largo == 2 ? (c & 0x1Fu) : largo == 3 ? (c & 0x0Fu) : (c & 0x07u);
    for (size_t k = 1; k < largo; ++k) {
        const u8 b = static_cast<u8>(t[i + k]);
        if ((b & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (b & 0x3Fu);
    }
    // Rechaza codificaciones largas (overlong), surrogates y puntos fuera de Unicode.
    if ((largo == 2 && cp < 0x80) || (largo == 3 && cp < 0x800) || (largo == 4 && (cp < 0x10000 || cp > 0x10FFFF)) ||
        (cp >= 0xD800 && cp <= 0xDFFF))
        return 0;
    return largo;
}

bool es_utf8(const char* t, size_t n) {
    for (size_t i = 0; i < n;) {
        u32 cp = 0;
        const size_t largo = largo_utf8(t, n, i, cp);
        if (largo == 0) return false;
        i += largo;
    }
    return true;
}

void glifos_de_texto(const char* t, size_t n, std::vector<GlifoTexto>& salida) {
    salida.clear();
    if (es_utf8(t, n)) {  // textos de la interfaz: TTF, proporcional
        for (size_t i = 0; i < n;) {
            u32 cp = 0;
            const size_t largo = largo_utf8(t, n, i, cp);
            salida.push_back({GLIFO_TTF_BASE + cp, static_cast<u8>(ancho_ttf(cp))});
            i += largo;
        }
        return;
    }
    for (size_t i = 0; i < n; ++i) {  // CP949 del servidor: fuente bitmap
        u8 c = static_cast<u8>(t[i]);
        if (c >= 0x20 && c < 0x7F) {
            salida.push_back({static_cast<u32>(c - 0x20), ANCHO_GLIFO_ASCII});
        } else if (c >= 0x81 && i + 1 < n) {
            int k = indice_glifo_coreano(c, static_cast<u8>(t[i + 1]));
            salida.push_back({k >= 0 ? 1000u + static_cast<u32>(k) : static_cast<u32>('?' - 0x20),
                              static_cast<u8>(k >= 0 ? ANCHO_GLIFO_KOR : ANCHO_GLIFO_ASCII)});
            ++i;
        }
    }
}

int ancho_texto(const char* t, size_t n) {
    int w = 0;
    if (es_utf8(t, n)) {
        for (size_t i = 0; i < n;) {
            u32 cp = 0;
            i += largo_utf8(t, n, i, cp);
            w += ancho_ttf(cp);
        }
        return w;
    }
    for (size_t i = 0; i < n; ++i) {
        u8 c = static_cast<u8>(t[i]);
        if (c >= 0x20 && c < 0x7F) w += ANCHO_GLIFO_ASCII;
        else if (c >= 0x81 && i + 1 < n) {
            w += indice_glifo_coreano(c, static_cast<u8>(t[i + 1])) >= 0 ? ANCHO_GLIFO_KOR : ANCHO_GLIFO_ASCII;
            ++i;
        }
    }
    return w;
}

size_t bytes_que_caben(const char* t, size_t n, int ancho_max) {
    // Un carácter (UTF-8 o CP949) nunca se parte. Se corta en el último espacio si hay uno.
    const bool utf8 = es_utf8(t, n);
    int w = 0;
    size_t i = 0, ultimo_espacio = 0;
    while (i < n) {
        size_t largo = 1;
        int cw = 0;
        if (utf8) {
            u32 cp = 0;
            largo = largo_utf8(t, n, i, cp);
            cw = ancho_ttf(cp);
        } else {
            u8 c = static_cast<u8>(t[i]);
            largo = (c >= 0x81 && i + 1 < n) ? 2 : 1;
            cw = ancho_texto(t + i, largo);
        }
        if (w + cw > ancho_max && i > 0) return ultimo_espacio > 0 ? ultimo_espacio : i;
        if (t[i] == ' ') ultimo_espacio = i + 1;
        w += cw;
        i += largo;
    }
    return n;
}

}  // namespace lin

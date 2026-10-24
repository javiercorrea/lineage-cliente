#include "plataforma/archivo_mapeado.h"
#include "recursos/cp949.h"
#include "recursos/fuente.h"

namespace lin {

bool FuenteBitmap::cargar(const std::string& carpeta) {
    if (!leer_archivo(carpeta + "font/eng.fnt", ascii) || ascii.size() < 95 * 12) return false;
    if (!leer_archivo(carpeta + "font/kor.fnt", coreana) || coreana.size() < 2401 * 24) coreana.clear();
    return true;
}

bool FuenteBitmap::rasterizar(u32 glifo, ImagenRGBA& salida) const {
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

void glifos_de_texto(const char* t, size_t n, std::vector<GlifoTexto>& salida) {
    salida.clear();
    for (size_t i = 0; i < n; ++i) {
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
    int w = 0;
    size_t i = 0, ultimo_espacio = 0;
    while (i < n) {
        u8 c = static_cast<u8>(t[i]);
        size_t largo = (c >= 0x81 && i + 1 < n) ? 2 : 1;
        int cw = ancho_texto(t + i, largo);
        if (w + cw > ancho_max && i > 0) return ultimo_espacio > 0 ? ultimo_espacio : i;
        if (c == ' ') ultimo_espacio = i + 1;
        w += cw;
        i += largo;
    }
    return n;
}

}  // namespace lin

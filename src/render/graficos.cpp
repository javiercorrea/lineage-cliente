#include "render/graficos.h"
#include <cstdio>
#include "plataforma/archivo_mapeado.h"

namespace lin {

const RegionAtlas* Graficos::buscar(u64 k) const {
    const u32* i = indice_.buscar(k);
    return i ? &regiones_[*i] : nullptr;
}

template <class F>
const RegionAtlas& Graficos::cargar(u64 k, F&& decodificar) {
    if (const RegionAtlas* r = buscar(k)) return *r;
    u64 t0 = SDL_GetPerformanceCounter();
    Carga c = decodificar();
    if (c == Carga::PENDIENTE) {
        return pendiente_;  // no se recuerda: se vuelve a pedir el frame siguiente
    }
    const RegionAtlas& r = guardar(k, c == Carga::OK);
    ms_decodificacion_ += static_cast<double>(SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency();
    ++decodificados_;
    return r;
}

const RegionAtlas& Graficos::guardar(u64 k, bool ok) {
    RegionAtlas r;
    if (ok && !gpu_.cabe_en_atlas(tmp_)) {
        // Más grande que una página: no entraría nunca. Se recuerda como fallo, si no se volvería a
        // leer y decodificar en cada frame.
        LIN_LOG("Imagen de %ux%u: no entra en el atlas", tmp_.ancho, tmp_.alto);
        ok = false;
    }
    if (ok && !gpu_.subir(tmp_, r)) {
        // Sin lugar este frame (buffer de subida lleno o atlas sin página libre): no se recuerda, se reintenta.
        return pendiente_;
    }
    if (int p = gpu_.tomar_pagina_reemplazada(); p >= 0) olvidar_pagina(static_cast<u16>(p));
    u32 i;
    if (!libres_.empty()) {
        i = libres_.back();
        libres_.pop_back();
    } else {
        i = static_cast<u32>(regiones_.size());
        regiones_.emplace_back();
        claves_.emplace_back();
    }
    regiones_[i] = ok ? r : RegionAtlas{};
    claves_[i] = k;
    indice_.poner(k, i);
    return regiones_[i];
}

// La página se reemplazó en el atlas: lo que estaba ahí se vuelve a decodificar si se pide.
void Graficos::olvidar_pagina(u16 pagina) {
    for (u32 i = 0; i < regiones_.size(); ++i) {
        if (!regiones_[i].valida() || regiones_[i].pagina != pagina) continue;
        indice_.quitar(claves_[i]);
        regiones_[i] = {};
        libres_.push_back(i);
    }
}

const RegionAtlas& Graficos::img(u32 n) {
    return cargar(clave(T_IMG, n), [&] {
        char nombre[32];
        std::snprintf(nombre, sizeof nombre, "%ue.img", n);
        Bytes b = rec_.sprite(nombre);
        if (b.empty()) {
            std::snprintf(nombre, sizeof nombre, "%u.img", n);
            b = rec_.sprite(nombre);
        }
        return !b.empty() && decodificar_img(b, tmp_) ? Carga::OK : Carga::FALLO;
    });
}

const RegionAtlas& Graficos::icono(u32 n) {
    return cargar(clave(T_ICONO, n), [&] {
        char nombre[32];
        std::snprintf(nombre, sizeof nombre, "%u.tbt", n);
        Bytes b = rec_.sprite(nombre);
        return !b.empty() && decodificar_tbt(b, tmp_) ? Carga::OK : Carga::FALLO;
    });
}

namespace {
constexpr int ANCHO_EMBLEMA = 16, ALTO_EMBLEMA = 12, BYTES_EMBLEMA = ANCHO_EMBLEMA * ALTO_EMBLEMA * 2;
}

// Si su página del atlas se reemplazó, se vuelve a subir desde los datos guardados (el servidor no lo
// manda otra vez: la app lo pide una sola vez por clan).
const RegionAtlas& Graficos::emblema(u32 clan) {
    static const RegionAtlas vacia;
    const u32* i = datos_emblema_.buscar(clan);
    if (!i) return vacia;
    return cargar(clave(T_EMBLEMA, clan), [&] {
        const u8* d = emblemas_.data() + *i;
        tmp_.preparar(ANCHO_EMBLEMA, ALTO_EMBLEMA);
        for (int k = 0; k < ANCHO_EMBLEMA * ALTO_EMBLEMA; ++k) {  // el negro puro es el fondo transparente
            const u16 c = static_cast<u16>(d[2 * k] | (d[2 * k + 1] << 8));
            tmp_.px[k] = (c & 0x7FFF) == 0 ? 0 : rgb555(c);
        }
        return Carga::OK;
    });
}

void Graficos::poner_emblema(u32 clan, Bytes datos) {
    if (datos.size() < BYTES_EMBLEMA) return;
    const u32* i = datos_emblema_.buscar(clan);
    const u32 ofs = i ? *i : static_cast<u32>(emblemas_.size());
    if (!i) {
        emblemas_.resize(emblemas_.size() + BYTES_EMBLEMA);
        datos_emblema_.poner(clan, ofs);
    }
    std::memcpy(emblemas_.data() + ofs, datos.data(), BYTES_EMBLEMA);
    // Uno nuevo reemplaza al anterior: se sube con el próximo emblema(). El casillero viejo se libera
    // (si no, olvidar_pagina, con la misma clave, podría borrar del índice al nuevo).
    const u64 k = clave(T_EMBLEMA, clan);
    if (const u32* r = indice_.buscar(k)) {
        regiones_[*r] = {};
        libres_.push_back(*r);
        indice_.quitar(k);
    }
}

const RegionAtlas& Graficos::png(u32 n) {
    return cargar(clave(T_PNG, n), [&] {
        char nombre[32];
        std::snprintf(nombre, sizeof nombre, "%u.png", n);
        Bytes b = rec_.sprite(nombre);
        return !b.empty() && decodificar_png(b, tmp_) ? Carga::OK : Carga::FALLO;
    });
}

const RegionAtlas& Graficos::png_archivo(const std::string& ruta) {
    return cargar(clave(T_ARCHIVO, std::hash<std::string>{}(ruta) >> 8), [&] {
        std::vector<u8> b;
        bool ok = leer_archivo(ruta, b) && decodificar_png(b, tmp_, false) && gpu_.cabe_en_atlas(tmp_);
        if (!ok) std::fprintf(stderr, "No se pudo cargar la imagen %s (PNG de hasta %d×%d)\n", ruta.c_str(), TAM_PAGINA - 1, TAM_PAGINA - 1);
        return ok ? Carga::OK : Carga::FALLO;
    });
}

const JuegoTiles* Graficos::juego_tiles(u32 n, bool& pendiente) {
    pendiente = false;
    if (const u32* i = indice_tiles_.buscar(n)) return juegos_[*i].cantidad ? &juegos_[*i] : nullptr;
    char nombre[32];
    std::snprintf(nombre, sizeof nombre, "%u.til", n);
    Bytes b = rec_.tile(nombre);
    if (!b.empty() && !precarga_.listo(clave(T_TILE, n), b)) {
        pendiente = true;
        return nullptr;
    }
    JuegoTiles j;
    if (!j.abrir(b)) j.cantidad = 0;
    indice_tiles_.poner(n, static_cast<u32>(juegos_.size()));
    juegos_.push_back(j);
    return j.cantidad ? &juegos_.back() : nullptr;
}

const RegionAtlas& Graficos::medio_tile(u32 valor, bool derecha) {
    return cargar(clave(T_TILE, (static_cast<u64>(valor) << 1) | (derecha ? 1 : 0)), [&] {
        bool pendiente;
        const JuegoTiles* j = juego_tiles(valor >> 8, pendiente);
        if (pendiente) return Carga::PENDIENTE;
        return j && j->decodificar(valor & 0xFF, derecha, tmp_) ? Carga::OK : Carga::FALLO;
    });
}

bool Graficos::tile_translucido(u32 valor, bool& pendiente) {
    const JuegoTiles* j = juego_tiles(valor >> 8, pendiente);
    return j && j->translucido(valor & 0xFF);
}

const ArchivoSpr* Graficos::archivo_spr(GfxId gfx, u16 sub, bool& pendiente) {
    pendiente = false;
    u64 k = (static_cast<u64>(gfx) << 16) | sub;
    if (const u32* i = indice_spr_.buscar(k)) return sprs_[*i].frames.empty() ? nullptr : &sprs_[*i];
    char nombre[32];
    std::snprintf(nombre, sizeof nombre, "%u-%u.spr", gfx, sub);
    Bytes b = rec_.sprite(nombre);
    if (!b.empty() && !precarga_.listo(clave(T_SPR, k), b)) {
        pendiente = true;
        return nullptr;
    }
    ArchivoSpr a;
    if (b.empty() || !a.abrir(b)) a.frames.clear();
    indice_spr_.poner(k, static_cast<u32>(sprs_.size()));
    sprs_.push_back(std::move(a));
    return sprs_.back().frames.empty() ? nullptr : &sprs_.back();
}

const RegionAtlas& Graficos::frame_sprite(GfxId gfx, u16 sub, u16 frame) {
    return cargar(clave(T_SPR, (static_cast<u64>(gfx) << 32) | (static_cast<u64>(sub) << 16) | frame), [&] {
        bool pendiente;
        const ArchivoSpr* a = archivo_spr(gfx, sub, pendiente);
        if (pendiente) return Carga::PENDIENTE;
        return a && a->componer_frame(frame, tmp_) ? Carga::OK : Carga::FALLO;
    });
}

const RegionAtlas& Graficos::glifo(u32 g) {
    return cargar(clave(T_GLIFO, g), [&] { return fuente_.rasterizar(g, tmp_) ? Carga::OK : Carga::FALLO; });
}

int Graficos::texto(std::string_view t, float x, float y, u64 clave_orden, u32 color) {
    glifos_de_texto(t.data(), t.size(), glifos_tmp_);
    int ancho = 0;
    for (const GlifoTexto& g : glifos_tmp_) {
        gpu_.dibujar_en(glifo(g.glifo), x + ancho, y, clave_orden, color);
        ancho += g.ancho;
    }
    return ancho;
}

int Graficos::texto_sombra(std::string_view t, float x, float y, u64 clave_orden, u32 color) {
    texto(t, x + 1, y + 1, clave_orden, rgba(0, 0, 0));
    return texto(t, x, y, clave_orden + (1u << 24), color);
}

}  // namespace lin

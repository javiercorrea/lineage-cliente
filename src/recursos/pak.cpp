#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <brotli/decode.h>
#include <zlib.h>
#include "recursos/des.h"
#include "recursos/pak.h"

namespace lin {

static char minuscula(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

u64 hash_nombre(std::string_view nombre) {
    u64 h = 0xcbf29ce484222325ull;
    for (char c : nombre) {
        h ^= static_cast<u8>(minuscula(c));
        h *= 0x100000001b3ull;
    }
    return h;
}

bool Pak::abrir(const std::string& base, bool puede_ir_cifrado, bool avisar) {
    std::vector<u8> idx;
    if (!leer_archivo(base + ".idx", idx) || idx.size() < 4) {
        if (avisar) LIN_LOG("No se pudo leer %s.idx", base.c_str());
        return false;
    }
    if (!datos_.abrir(base + ".pak")) {
        if (avisar) LIN_LOG("No se pudo abrir %s.pak", base.c_str());
        return false;
    }
    entradas_.clear();
    nombres_.clear();
    const std::string_view marca(reinterpret_cast<const char*>(idx.data()), 4);
    bool ok;
    if (marca == "_EXT" && !(idx.size() >= 6 && idx[4] == 'B' && idx[5] == '$')) {
        formato_ = Formato::EXT;
        ok = leer_extendido(idx, 128, puede_ir_cifrado);
    } else if (marca == "_RMS") {
        formato_ = Formato::RMS;
        ok = leer_extendido(idx, 276, puede_ir_cifrado);
    } else {
        // _EXTB$ e IDXC (Justice) no se leen: la cantidad sale enorme y el índice se rechaza (FORMATOS_PAK.md §3 y §7).
        formato_ = Formato::CLASICO;
        ok = leer_clasico(idx, puede_ir_cifrado);
    }
    if (!ok) {
        if (avisar) LIN_LOG("Índice no válido: %s.idx", base.c_str());
        return false;
    }
    // Ante nombres repetidos gana la última entrada del índice: stable_sort + búsqueda por el final.
    std::stable_sort(entradas_.begin(), entradas_.end(), [](const Entrada& a, const Entrada& b) { return a.hash < b.hash; });
    return true;
}

void Pak::agregar(u32 offset, u32 guardado, u32 tam, u8 compresion, std::string_view nombre) {
    Entrada e{};
    e.offset = offset;
    e.guardado = guardado;
    e.tam = tam;
    e.compresion = compresion;
    e.nombre = static_cast<u32>(nombres_.size());
    e.largo_nombre = static_cast<u16>(nombre.size());
    for (char c : nombre) nombres_ += minuscula(c);
    const size_t barra = nombre.find_last_of("/\\");  // se busca por el nombre solo
    e.hash = hash_nombre(barra == std::string_view::npos ? nombre : nombre.substr(barra + 1));
    entradas_.push_back(e);
}

bool Pak::leer_clasico(std::vector<u8>& idx, bool cifrado) {
    LectorBinario l{idx};
    const u32 n = l.u32_();
    if (static_cast<size_t>(n) * 28 + 4 > idx.size()) return false;
    if (cifrado) Des(CLAVE_DES_LINEAGE).descifrar(idx.data() + 4, static_cast<size_t>(n) * 28);
    des_ = cifrado;
    entradas_.reserve(n);
    for (u32 i = 0; i < n; ++i) {
        const u32 offset = l.u32_();
        const Bytes nom = l.tomar(20);
        const u32 tam = l.u32_();
        size_t len = 0;
        while (len < 20 && nom[len]) ++len;
        // Se descartan entradas que salen del .pak (el índice original tiene algunas rotas).
        if (static_cast<u64>(offset) + tam > datos_.datos().size()) continue;
        agregar(offset, tam, tam, 0, {reinterpret_cast<const char*>(nom.data()), len});
    }
    return true;
}

bool Pak::leer_extendido(std::vector<u8>& idx, size_t tam_entrada, bool puede_ir_cifrado) {
    if (idx.size() < 8) return false;
    const size_t largo_nombre = tam_entrada - 16;
    u32 n = static_cast<u32>(idx[4]) | static_cast<u32>(idx[5]) << 8 | static_cast<u32>(idx[6]) << 16 | static_cast<u32>(idx[7]) << 24;
    const size_t caben = (idx.size() - 8) / tam_entrada;
    if (n > caben) n = static_cast<u32>(caben);  // la cantidad va sin cifrar; con DES igual se acota por el largo
    const u64 tam_pak = datos_.datos().size();
    // Lee las entradas y cuenta cuántas son creíbles (nombre imprimible, dentro del .pak): así se sabe si iban con DES.
    auto leer = [&](bool guardar) {
        u32 buenas = 0;
        LectorBinario l{idx};
        l.saltar(8);
        for (u32 i = 0; i < n; ++i) {
            const u32 offset = l.u32_();
            const u32 tam = l.u32_(), guardado_c = l.u32_(), compresion = l.u32_();
            const Bytes nom = l.tomar(largo_nombre);
            size_t len = 0;
            bool imprimible = true;
            while (len < largo_nombre && nom[len]) imprimible = imprimible && nom[len] >= 0x20 && nom[len] < 0x7F, ++len;
            const u32 guardado = compresion ? guardado_c : tam;
            if (!len || !imprimible || compresion > 2 || static_cast<u64>(offset) + guardado > tam_pak) continue;
            ++buenas;
            if (guardar) agregar(offset, guardado, tam, static_cast<u8>(compresion), {reinterpret_cast<const char*>(nom.data()), len});
        }
        return buenas;
    };
    if (n == 0) return true;
    des_ = false;
    if (puede_ir_cifrado && leer(false) < n / 2) {
        Des(CLAVE_DES_LINEAGE).descifrar(idx.data() + 8, static_cast<size_t>(n) * tam_entrada);
        des_ = true;
    }
    entradas_.reserve(n);
    return leer(true) > 0;
}

bool Pak::decodificar(const Entrada& e, std::vector<u8>& salida) const {
    const Bytes crudo = datos_.datos().subspan(e.offset, e.guardado);
    std::vector<u8> descifrado;
    Bytes fuente = crudo;
    if (des_) {
        descifrado.assign(crudo.begin(), crudo.end());
        Des(CLAVE_DES_LINEAGE).descifrar(descifrado.data(), descifrado.size());
        fuente = descifrado;
    }
    switch (e.compresion) {
    case 0:
        salida.assign(fuente.begin(), fuente.end());
        return true;
    case 1: {
        salida.resize(e.tam);
        uLongf largo = e.tam;
        return uncompress(salida.data(), &largo, fuente.data(), static_cast<uLong>(fuente.size())) == Z_OK && largo == e.tam;
    }
    case 2: {
        salida.resize(e.tam);
        size_t largo = e.tam;
        return BrotliDecoderDecompress(fuente.size(), fuente.data(), &largo, salida.data()) == BROTLI_DECODER_RESULT_SUCCESS &&
               largo == e.tam;
    }
    default:
        return false;
    }
}

Bytes Pak::buscar(std::string_view nombre) const {
    const u64 h = hash_nombre(nombre);
    auto it = std::upper_bound(entradas_.begin(), entradas_.end(), h, [](u64 v, const Entrada& e) { return v < e.hash; });
    if (it == entradas_.begin() || (it - 1)->hash != h) return {};
    const Entrada& e = *(it - 1);
    if (!des_ && e.compresion == 0) return datos_.datos().subspan(e.offset, e.tam);
    std::lock_guard l(mu_);
    auto [pos, nuevo] = decodificados_.try_emplace(static_cast<u32>(&e - entradas_.data()));
    if (nuevo && !decodificar(e, pos->second)) {
        LIN_LOG("No se pudo decodificar %.*s del pak", static_cast<int>(e.largo_nombre), nombres_.data() + e.nombre);
        pos->second.clear();
    }
    return pos->second;
}

bool Recursos::abrir(const std::string& carpeta, const std::vector<std::string>& extras) {
    carpeta_ = carpeta;
    fuentes_.clear();
    bool ok = true;
    auto agregar = [&](const std::string& c, bool primaria) {
        auto f = std::make_unique<Fuente>();
        f->carpeta = c;
        bool bien = f->sprite_base.abrir(c + "Sprite", false, primaria);
        bien = f->tile.abrir(c + "Tile", false, primaria) && bien;
        // Los clientes Remastered no traen Text: las tablas, los html y list.spz van en Data ("data/...").
        const bool con_text = f->texto.abrir(c + "Text", true, false) || f->texto.abrir(c + "Data", true, false);
        if (!con_text && primaria) LIN_LOG("No se pudo abrir %sText ni %sData", c.c_str(), c.c_str());
        bien = con_text && bien;
        char nombre[16];
        for (int i = 0; i < 16; ++i) {
            std::snprintf(nombre, sizeof nombre, "Sprite%02d", i);
            f->sprite_hash[i].abrir(c + nombre, false, false);  // opcionales: instalaciones viejas no los tienen
        }
        if (primaria) ok = bien;
        fuentes_.push_back(std::move(f));  // una carpeta extra sin paks igual sirve sus archivos sueltos
    };
    agregar(carpeta, true);
    for (const std::string& e : extras) {
        if (e.empty()) continue;
        agregar(e.back() == '/' ? e : e + '/', false);
    }
    return ok;
}

// Archivo suelto <dir>/<nombre> de una fuente (sin distinguir mayúsculas en Linux), mapeado una vez y guardado en caché.
Bytes Recursos::suelto(const Fuente& f, std::string_view dir, std::string_view nombre) {
    std::string clave(dir);
    clave += nombre;
    std::lock_guard l(f.mu);
    auto it = f.sueltos.find(clave);
    if (it == f.sueltos.end()) {
        ArchivoMapeado m;
        std::error_code ec;
        const std::string ruta = ruta_real(f.carpeta + clave);
        if (std::filesystem::is_regular_file(ruta, ec)) m.abrir(ruta);
        it = f.sueltos.emplace(std::move(clave), std::move(m)).first;
    }
    return it->second.datos();
}

Bytes Recursos::en_fuente(const Fuente& f, std::string_view nombre) {
    unsigned suma = 0;
    for (char c : nombre) suma += static_cast<u8>(minuscula(c));
    Bytes b = f.sprite_hash[suma & 15].buscar(nombre);
    if (b.empty()) b = f.sprite_base.buscar(nombre);
    return b.empty() ? suelto(f, "sprite/", nombre) : b;
}

Bytes Recursos::sprite(std::string_view nombre) const {
    for (const auto& f : fuentes_)
        if (Bytes b = en_fuente(*f, nombre); !b.empty()) return b;
    return {};
}

Bytes Recursos::sprite_base(std::string_view nombre) const {
    for (const auto& f : fuentes_)
        if (Bytes b = f->sprite_base.buscar(nombre); !b.empty()) return b;
    return {};
}

int Recursos::fuente_de_sprite(std::string_view nombre) const {
    for (size_t i = 0; i < fuentes_.size(); ++i)
        if (!en_fuente(*fuentes_[i], nombre).empty()) return static_cast<int>(i);
    return -1;
}

Bytes Recursos::tile(std::string_view nombre) const {
    for (const auto& f : fuentes_) {
        Bytes b = f->tile.buscar(nombre);
        if (b.empty()) b = suelto(*f, "Tile/", nombre);
        if (!b.empty()) return b;
    }
    return {};
}

static bool es_html(std::string_view n) {
    auto fin = [&](std::string_view e) { return n.size() >= e.size() && n.substr(n.size() - e.size()) == e; };
    return fin(".html") || fin(".htm");
}

bool Recursos::texto(std::string_view nombre, std::vector<u8>& salida) const {
    const size_t n = es_html(nombre) ? fuentes_.size() : 1;  // ids de texto solo de la principal
    for (size_t i = 0; i < n; ++i) {
        Bytes b = fuentes_[i]->texto.buscar(nombre);
        if (b.empty()) continue;
        salida.assign(b.begin(), b.end());  // Pak ya lo devuelve descifrado y descomprimido
        return true;
    }
    return false;
}

std::string Recursos::ruta_archivo(const std::string& relativa) const {
    std::error_code ec;
    for (const auto& f : fuentes_)
        if (std::filesystem::exists(ruta_real(f->carpeta + relativa), ec)) return f->carpeta + relativa;
    return carpeta_ + relativa;
}

}  // namespace lin

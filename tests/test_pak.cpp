// Índices _EXT y _RMS del Remastered con datos armados a mano (sin archivos del juego).
#include <cstring>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "recursos/des.h"
#include "recursos/pak.h"

using namespace lin;

namespace {

const std::string CLARO = "hola pak remastered, hola pak remastered, hola!\n";
// CLARO comprimido con zlib (nivel 9) y con brotli (CLI de brotli 1.1).
const std::vector<u8> ZLIB = {0x78, 0xda, 0xcb, 0xc8, 0xcf, 0x49, 0x54, 0x28, 0x48, 0xcc, 0x56, 0x28, 0x4a, 0xcd, 0x4d, 0x2c, 0x2e,
                              0x49, 0x2d, 0x4a, 0x4d, 0xd1, 0x51, 0xc8, 0xc0, 0x25, 0xa8, 0xc8, 0x05, 0x00, 0xaf, 0x13, 0x11, 0x00};
const std::vector<u8> BROTLI = {0xa1, 0x78, 0x01, 0x80, 0x60, 0x9c, 0xe4, 0xda, 0x37, 0xe3, 0xdc, 0xc6, 0xd6, 0xb5, 0xdf, 0x3d, 0x42, 0x82, 0x50,
                                0x50, 0x14, 0x0a, 0x29, 0x09, 0x3b, 0x04, 0x05, 0xc6, 0x79, 0x87, 0x38, 0x36, 0x5e, 0x24, 0x55, 0xc2, 0x09};

struct Archivo {
    std::string nombre;
    std::vector<u8> guardado;  // como va en el .pak
    u32 compresion;
};

void poner32(std::vector<u8>& v, u32 x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<u8>(x >> (8 * i)));
}

// Escribe <dir>/<base>.idx y .pak en el formato extendido (marca "_EXT" o "_RMS"); con `des`, entradas y datos cifrados.
std::string escribir(const std::string& base, const char* marca, size_t tam_entrada, const std::vector<Archivo>& archivos, bool des) {
    const auto dir = std::filesystem::temp_directory_path() / "lin_test_pak";
    std::filesystem::create_directories(dir);
    std::vector<u8> idx(marca, marca + 4), pak;
    poner32(idx, static_cast<u32>(archivos.size()));
    Des d(CLAVE_DES_LINEAGE);
    for (const Archivo& a : archivos) {
        std::vector<u8> g = a.guardado;
        if (des)
            for (size_t i = 0; i + 8 <= g.size(); i += 8) d.cifrar_bloque(g.data() + i);
        poner32(idx, static_cast<u32>(pak.size()));
        poner32(idx, static_cast<u32>(CLARO.size()));
        poner32(idx, a.compresion ? static_cast<u32>(g.size()) : 0);
        poner32(idx, a.compresion);
        std::vector<u8> nombre(tam_entrada - 16, 0);
        std::memcpy(nombre.data(), a.nombre.data(), a.nombre.size());
        idx.insert(idx.end(), nombre.begin(), nombre.end());
        pak.insert(pak.end(), g.begin(), g.end());
    }
    if (des)
        for (size_t i = 8; i + 8 <= idx.size(); i += 8) d.cifrar_bloque(idx.data() + i);
    const std::string ruta = (dir / base).string();
    std::ofstream(ruta + ".idx", std::ios::binary).write(reinterpret_cast<const char*>(idx.data()), static_cast<std::streamsize>(idx.size()));
    std::ofstream(ruta + ".pak", std::ios::binary).write(reinterpret_cast<const char*>(pak.data()), static_cast<std::streamsize>(pak.size()));
    return ruta;
}

std::string texto(Bytes b) { return {reinterpret_cast<const char*>(b.data()), b.size()}; }

const std::vector<Archivo> ARCHIVOS = {
    {"Plano.txt", std::vector<u8>(CLARO.begin(), CLARO.end()), 0},
    {"data/zlib.txt", ZLIB, 1},
    {"data/brotli.txt", BROTLI, 2},
};

}  // namespace

TEST_CASE("Pak _EXT: sin comprimir, zlib y brotli; los nombres con carpeta se encuentran por el nombre solo") {
    Pak p;
    REQUIRE(p.abrir(escribir("Ext", "_EXT", 128, ARCHIVOS, false), false));
    CHECK(p.formato() == Pak::Formato::EXT);
    CHECK_FALSE(p.cifrado());
    CHECK(p.cantidad() == 3);
    CHECK(texto(p.buscar("plano.txt")) == CLARO);  // sin distinguir mayúsculas
    CHECK(texto(p.buscar("zlib.txt")) == CLARO);
    CHECK(texto(p.buscar("BROTLI.TXT")) == CLARO);
    CHECK(p.buscar("brotli.txt").data() == p.buscar("brotli.txt").data());  // decodificado una vez y guardado
    CHECK(p.buscar("no_esta.txt").empty());
}

TEST_CASE("Pak _RMS: entradas de 276 bytes") {
    Pak p;
    REQUIRE(p.abrir(escribir("Rms", "_RMS", 276, ARCHIVOS, false), false));
    CHECK(p.formato() == Pak::Formato::RMS);
    CHECK(texto(p.buscar("zlib.txt")) == CLARO);
    CHECK(texto(p.buscar("brotli.txt")) == CLARO);
}

TEST_CASE("Pak _EXT de texto con DES: se detecta y se descifra antes de descomprimir") {
    Pak p;
    REQUIRE(p.abrir(escribir("ExtDes", "_EXT", 128, ARCHIVOS, true), true));
    CHECK(p.cifrado());
    CHECK(texto(p.buscar("plano.txt")) == CLARO);
    CHECK(texto(p.buscar("zlib.txt")) == CLARO);
    CHECK(texto(p.buscar("brotli.txt")) == CLARO);
    // El mismo índice sin cifrar, abierto como texto: no se descifra por error.
    Pak q;
    REQUIRE(q.abrir(escribir("ExtSinDes", "_EXT", 128, ARCHIVOS, false), true));
    CHECK_FALSE(q.cifrado());
    CHECK(texto(q.buscar("brotli.txt")) == CLARO);
}

TEST_CASE("Pak: un brotli dañado devuelve vacío en vez de basura") {
    std::vector<Archivo> malos = {{"malo.txt", std::vector<u8>(BROTLI.begin(), BROTLI.begin() + 10), 2}};
    Pak p;
    REQUIRE(p.abrir(escribir("Malo", "_EXT", 128, malos, false), false));
    CHECK(p.buscar("malo.txt").empty());
}

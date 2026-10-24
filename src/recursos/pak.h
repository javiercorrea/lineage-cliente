// Contenedores .idx/.pak del juego original.
//
// .idx: u32 cantidad + cantidad × {u32 offset, char nombre[20], u32 tamaño}. En Text.idx las
// entradas van cifradas con DES. El .pak se mapea en memoria y un archivo es un slice de él.
//
// Sprites: un archivo se busca primero en SpriteNN.pak, con NN = (suma de los bytes del nombre)
// & 15, y si no está, en Sprite.pak (getHashIndex de l1j-client). Los SpriteNN tienen las
// versiones más nuevas (p. ej. 814.png del login está en Sprite00 y no en Sprite.pak).
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "plataforma/archivo_mapeado.h"

namespace lin {

class Pak {
public:
    bool abrir(const std::string& ruta_sin_extension, bool indice_cifrado);
    // Devuelve el contenido (vista al archivo mapeado) o vacío si no existe. Nombre en minúsculas.
    Bytes buscar(std::string_view nombre) const;
    size_t cantidad() const { return entradas_.size(); }
    // Para el explorador: nombre de la entrada i.
    std::string_view nombre(size_t i) const { return entradas_[i].nombre; }

private:
    struct Entrada {
        u64 hash;
        u32 offset, tam;
        char nombre[21];
    };
    std::vector<Entrada> entradas_;  // ordenadas por hash para búsqueda binaria
    ArchivoMapeado datos_;
};

u64 hash_nombre(std::string_view nombre);  // FNV-1a de 64 bits sobre el nombre en minúsculas

// Todos los contenedores del juego.
class Recursos {
public:
    bool abrir(const std::string& carpeta_juego);
    const std::string& carpeta() const { return carpeta_; }

    Bytes sprite(std::string_view nombre) const;  // SpriteNN.pak y luego Sprite.pak
    Bytes tile(std::string_view nombre) const { return tile_.buscar(nombre); }
    // Text.pak: los archivos también van cifrados con DES; se devuelven ya descifrados.
    bool texto(std::string_view nombre, std::vector<u8>& salida) const;

    const Pak& pak_sprites_base() const { return sprite_base_; }
    const Pak& pak_textos() const { return texto_; }

private:
    std::string carpeta_;
    Pak sprite_base_;
    Pak sprite_hash_[16];
    Pak tile_;
    Pak texto_;
};

}  // namespace lin

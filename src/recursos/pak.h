// Contenedores .idx/.pak del juego original y del Remastered (formatos en docs/analisis/FORMATOS_PAK.md).
//
// Clásico: .idx = u32 cantidad + cantidad × {u32 offset, char nombre[20], u32 tamaño}. En Text.idx las entradas y los
// archivos van cifrados con DES.
// _EXT / _RMS (Remastered): "_EXT" o "_RMS" + i32 cantidad, y entradas de 128 / 276 bytes: {u32 offset, i32 tamaño,
// i32 tamaño guardado, i32 compresión (0 nada, 1 zlib, 2 brotli), nombre[112 / 260]}. El nombre puede traer carpeta
// ("data/list.spz"): se busca por el nombre solo. Text en _EXT puede ir además con DES (se detecta).
// El .pak se mapea en memoria: un archivo sin comprimir ni cifrar es un slice de él; los demás se decodifican la primera
// vez que se piden y quedan en memoria (la vista devuelta sigue valiendo mientras viva el Pak).
//
// Sprites: un archivo se busca primero en SpriteNN.pak, con NN = (suma de los bytes del nombre)
// & 15, y si no está, en Sprite.pak (getHashIndex de l1j-client). Los SpriteNN tienen las
// versiones más nuevas (p. ej. 814.png del login está en Sprite00 y no en Sprite.pak).
#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include "plataforma/archivo_mapeado.h"

namespace lin {

class Pak {
public:
    enum class Formato : u8 { CLASICO, EXT, RMS };
    // puede_ir_cifrado: Text (el clásico va siempre con DES; en _EXT / _RMS se prueba sin y con). avisar: al log si falla.
    bool abrir(const std::string& ruta_sin_extension, bool puede_ir_cifrado, bool avisar = true);
    // Devuelve el contenido (descifrado y descomprimido) o vacío si no existe o no se pudo decodificar. Sin distinguir
    // mayúsculas; un nombre con carpeta en el índice también se encuentra por su nombre solo.
    Bytes buscar(std::string_view nombre) const;
    size_t cantidad() const { return entradas_.size(); }
    // Para el explorador: nombre de la entrada i (con su carpeta, si tiene).
    std::string_view nombre(size_t i) const { return {nombres_.data() + entradas_[i].nombre, entradas_[i].largo_nombre}; }
    Formato formato() const { return formato_; }
    bool cifrado() const { return des_; }

private:
    struct Entrada {
        u64 hash;
        u32 offset, guardado, tam;  // guardado: bytes en el .pak; tam: sin comprimir
        u32 nombre;                 // posición en nombres_
        u16 largo_nombre;
        u8 compresion;
    };
    bool leer_clasico(std::vector<u8>& idx, bool cifrado);
    bool leer_extendido(std::vector<u8>& idx, size_t tam_entrada, bool puede_ir_cifrado);
    bool decodificar(const Entrada& e, std::vector<u8>& salida) const;
    void agregar(u32 offset, u32 guardado, u32 tam, u8 compresion, std::string_view nombre);

    std::vector<Entrada> entradas_;  // ordenadas por hash para búsqueda binaria
    std::string nombres_;
    ArchivoMapeado datos_;
    Formato formato_ = Formato::CLASICO;
    bool des_ = false;
    // shortcut: lo decodificado no se libera nunca; pasar a un LRU si un cliente Remastered usa demasiada memoria.
    mutable std::mutex mu_;
    mutable std::unordered_map<u32, std::vector<u8>> decodificados_;  // índice de entrada -> contenido (vacío = falló)
};

u64 hash_nombre(std::string_view nombre);  // FNV-1a de 64 bits sobre el nombre en minúsculas

// Todos los contenedores del juego. La primera fuente es la instalación principal (juego=); las siguientes son
// las carpetas de juego_extra=, probadas en orden para lo que les falte a las anteriores (sprites, tiles, archivos sueltos de
// sprite/ y map/, páginas html). Las tablas de texto (.tbl, .def, hechizos) salen solo de la principal.
class Recursos {
public:
    bool abrir(const std::string& carpeta_juego, const std::vector<std::string>& extras = {});
    const std::string& carpeta() const { return carpeta_; }

    Bytes sprite(std::string_view nombre) const;  // SpriteNN.pak, Sprite.pak, loose sprite/ per source
    Bytes tile(std::string_view nombre) const;    // Tile.pak, loose Tile/ per source
    // Text.pak (o Data.pak en los clientes Remastered): se devuelven ya descifrados y descomprimidos.
    bool texto(std::string_view nombre, std::vector<u8>& salida) const;
    // Primera fuente que tiene `relativa` (p. ej. "map/4/7fff8000.s32", "sound/12.wav"); la ruta de la principal si ninguna.
    std::string ruta_archivo(const std::string& relativa) const;
    // No se registra qué fuente (0 = principal) atendió las últimas búsquedas; esto dice dónde se resuelve el nombre de un sprite.
    // La misma búsqueda pero solo en Sprite.pak (el archivo base): se saltan los parches SpriteNN.pak con hash. El cliente 2009 capturado
    // todavía muestra el arte base de algunas imágenes de la interfaz (botones de la franja inferior) que los parches reemplazaron.
    Bytes sprite_base(std::string_view nombre) const;
    int fuente_de_sprite(std::string_view nombre) const;
    size_t fuentes() const { return fuentes_.size(); }
    const std::string& carpeta_de(size_t i) const { return fuentes_[i]->carpeta; }

    const Pak& pak_sprites_base() const { return fuentes_[0]->sprite_base; }
    const Pak& pak_textos() const { return fuentes_[0]->texto; }

private:
    struct Fuente {
        std::string carpeta;
        Pak sprite_base, sprite_hash[16], tile, texto;
        mutable std::mutex mu;  // caché de archivos sueltos (sprite() se llama desde varios hilos)
        mutable std::unordered_map<std::string, ArchivoMapeado> sueltos;  // mapeo vacío = se sabe que falta
    };
    static Bytes suelto(const Fuente& f, std::string_view dir, std::string_view nombre);
    static Bytes en_fuente(const Fuente& f, std::string_view nombre);
    std::string carpeta_;
    std::vector<std::unique_ptr<Fuente>> fuentes_;
};

}  // namespace lin

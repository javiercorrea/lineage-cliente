// Caché de gráficos: recurso del juego -> región del atlas. Decodifica la primera vez que se
// pide y recuerda también los fallos (región inválida) para no reintentar cada frame.
//
// Los tiles y los frames de sprites esperan a que su .til o .spr esté en memoria (Precarga): hasta
// entonces devuelven una región vacía, sin recordarla, y se dibujan uno o dos frames después. Las
// imágenes de interfaz, los íconos y los glifos se leen en el momento (son pocos y van en las
// pantallas fijas).
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "core/mapa_hash.h"
#include "recursos/fuente.h"
#include "recursos/pak.h"
#include "recursos/precarga.h"
#include "recursos/sprites.h"
#include "recursos/tabla_sprites.h"
#include "recursos/tiles.h"
#include "render/gpu.h"

namespace lin {

class Graficos {
public:
    Graficos(Gpu& gpu, const Recursos& rec, const FuenteBitmap& fuente) : gpu_(gpu), rec_(rec), fuente_(fuente) {}

    // Imagen de interfaz N: "Ne.img", "N.img" (con color transparente propio).
    const RegionAtlas& img(u32 n);
    // Imagen N.png (fondos de pantalla, animaciones de la selección de personaje).
    const RegionAtlas& png(u32 n);
    // PNG fuera del juego (logo propio), con su alfa. Región vacía si no se pudo leer.
    const RegionAtlas& png_archivo(const std::string& ruta);
    // Medio tile de suelo: valor del .s32 (juego << 8 | tile) y mitad.
    const RegionAtlas& medio_tile(u32 valor, bool derecha);
    // Tile translúcido (sombra; ver tiles.h). `pendiente`: su .til todavía no está en memoria.
    bool tile_translucido(u32 valor, bool& pendiente);
    // Frame de sprite ya compuesto: archivo "<gfx>-<sub>.spr", frame.
    const RegionAtlas& frame_sprite(GfxId gfx_archivos, u16 sub, u16 frame);
    // La región que se devolvió porque el dato todavía no está (se está cargando, o el atlas no tiene
    // lugar este frame): no es un fallo, se pide de nuevo el frame siguiente. Ver `pendiente_`.
    bool pendiente(const RegionAtlas& r) const { return &r == &pendiente_; }
    const RegionAtlas& glifo(u32 g);
    // Ícono de inventario N.tbt (N = gfx de inventario del ítem).
    const RegionAtlas& icono(u32 n);
    // Emblema de clan (S_Emblem: 16 × 12 píxeles RGB555); región vacía si todavía no llegó.
    const RegionAtlas& emblema(u32 clan);
    void poner_emblema(u32 clan, Bytes datos);

    // Texto CP949 con la fuente original; devuelve el ancho en píxeles.
    int texto(std::string_view t, float x, float y, u64 clave, u32 color = BLANCO);
    // Texto con sombra negra de 1 px (como los nombres sobre los personajes).
    int texto_sombra(std::string_view t, float x, float y, u64 clave, u32 color = BLANCO);

    Gpu& gpu() { return gpu_; }
    // Al comienzo de cada frame: lo que terminó de traer la precarga.
    void nuevo_frame() { precarga_.actualizar(); }
    u32 precargas_en_curso() const { return precarga_.en_curso(); }
    // Medición: tiempo de decodificación y gráficos nuevos desde la última llamada.
    double tomar_ms_decodificacion() { double v = ms_decodificacion_; ms_decodificacion_ = 0; return v; }
    u32 tomar_decodificados() { u32 v = decodificados_; decodificados_ = 0; return v; }

private:
    enum Tipo : u64 { T_IMG = 1, T_PNG, T_TILE, T_SPR, T_GLIFO, T_ICONO, T_EMBLEMA, T_ARCHIVO };
    static u64 clave(Tipo t, u64 v) { return (static_cast<u64>(t) << 56) | v; }
    enum class Carga { OK, FALLO, PENDIENTE };
    // Busca k; si no está, decodifica en tmp_ con `decodificar` (devuelve Carga) y lo sube al atlas.
    template <class F>
    const RegionAtlas& cargar(u64 k, F&& decodificar);
    const RegionAtlas& guardar(u64 k, bool ok);
    void olvidar_pagina(u16 pagina);
    const RegionAtlas* buscar(u64 k) const;
    // nullptr si no existe o si todavía no está en memoria (`pendiente`).
    const JuegoTiles* juego_tiles(u32 n, bool& pendiente);
    const ArchivoSpr* archivo_spr(GfxId gfx, u16 sub, bool& pendiente);

    Gpu& gpu_;
    const Recursos& rec_;
    const FuenteBitmap& fuente_;
    MapaHash indice_{4096};
    std::vector<RegionAtlas> regiones_;
    RegionAtlas pendiente_;  // región vacía que se devuelve mientras algo está pendiente (no se guarda)
    std::vector<u64> claves_;   // clave de cada región (para olvidarlas)
    std::vector<u32> libres_;   // regiones olvidadas, para reusar
    MapaHash indice_tiles_, indice_spr_;
    std::vector<JuegoTiles> juegos_;
    std::vector<ArchivoSpr> sprs_;
    Precarga precarga_;
    ImagenRGBA tmp_;
    std::vector<GlifoTexto> glifos_tmp_;
    double ms_decodificacion_ = 0;

    u32 decodificados_ = 0;
};

}  // namespace lin

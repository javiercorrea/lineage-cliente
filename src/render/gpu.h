// Renderer 2D sobre SDL_GPU.
//
// - Todas las imágenes viven en un atlas: un texture array de páginas de 2048×2048 RGBA8. Cuando
//   se llenan, se reemplaza la página usada hace más tiempo (ver atlas.h).
// - Cada frame se arma una lista de comandos (instancias de 48 bytes + clave de orden u64), se
//   ordena con radix sort (estable) y se dibuja con UNA llamada instanciada: como todas las
//   páginas están en el mismo texture array, no hace falta cortar el lote por textura.
// - Las coordenadas son lógicas. Fuera del juego el tamaño lógico es el del original (640×480);
//   en el juego es "ampliable": ventana / escala, así una ventana más grande o un zoom menor
//   muestran más mapa (mínimo 640×480). Se dibuja en una textura de tamaño lógico × escala:
//   con muestreo "nearest" los píxeles del arte siguen nítidos, y las posiciones fraccionarias
//   (cámara) se mueven en pasos de 1/escala de píxel lógico, suaves también en Retina.
#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "recursos/imagen.h"
#include "render/atlas.h"

namespace lin {

struct Instancia {
    float x, y, w, h;
    float u0, v0, u1, v1;
    u32 pagina;
    u32 color;  // RGBA8 multiplicativo (0xAABBGGRR)
    u32 mezcla; // Mezcla
    u32 relleno;
};
static_assert(sizeof(Instancia) == 48, "debe coincidir con el struct del shader");

// Normal (tapa según el alfa) o aditiva (suma su luz al fondo: efectos de hechizos).
enum class Mezcla : u32 { NORMAL = 0, ADITIVA = 1 };

// Capas de dibujo (byte alto de la clave de orden). CAPA_VENTANAS no se usa directo: es donde
// Gpu::nivel_ventana lleva lo que se dibuja en CAPA_UI y CAPA_UI_TEXTO mientras se dibuja una
// ventana. CAPA_ARRASTRE: lo que se arrastra con el mouse, encima de todo.
enum Capa : u8 {
    CAPA_FONDO = 0, CAPA_SUELO = 1, CAPA_MUNDO = 2, CAPA_EFECTOS = 3, CAPA_UI = 4, CAPA_UI_TEXTO = 5,
    CAPA_VENTANAS = 6, CAPA_ARRASTRE = 7,
};

inline u64 clave_orden(Capa capa, u32 profundidad) { return (static_cast<u64>(capa) << 56) | (static_cast<u64>(profundidad) << 24); }

constexpr u32 BLANCO = 0xFFFFFFFFu;
inline u32 rgba(u8 r, u8 g, u8 b, u8 a = 255) { return r | (g << 8) | (b << 16) | (static_cast<u32>(a) << 24); }

constexpr int TAM_PAGINA = 2048;
constexpr int PAGINAS_ATLAS = 8;           // 8 × 16 MB; LIN_ATLAS_PAGINAS y LIN_ATLAS_TAM achican el atlas para probar el reemplazo
constexpr u32 MAX_INSTANCIAS = 1u << 16;
constexpr u32 TAM_SUBIDA_ATLAS = 32u << 20; // máximo de píxeles nuevos por frame

class Gpu {
public:
    // ancho/alto lógicos mínimos (los del original).
    bool iniciar(SDL_Window* ventana, int ancho_logico, int alto_logico);
    // Al comienzo de cada frame: fija la escala y el tamaño lógico según la ventana. `zoom` es la
    // escala deseada (píxeles reales por píxel lógico; 0 = la mayor que entra, como el original).
    // Sin `ampliable`, el tamaño lógico es el mínimo y la imagen queda centrada.
    void ajustar_vista(bool ampliable, int zoom);
    // Mayor escala con la que entra el tamaño mínimo en la ventana actual.
    int escala_maxima() const;
    void cerrar();

    // Reserva lugar en el atlas y encola la subida. false si no hay lugar este frame.
    bool subir(const ImagenRGBA& img, RegionAtlas& salida);
    // Página del atlas vaciada por el último subir, o -1: sus regiones ya no valen.
    int tomar_pagina_reemplazada() { return empaquetado_.tomar_reemplazada(); }

    void dibujar(const RegionAtlas& r, int x, int y, u64 clave, u32 color = BLANCO);
    // Igual que dibujar, en una posición lógica fraccionaria (mundo con cámara suave).
    void dibujar_en(const RegionAtlas& r, float x, float y, u64 clave, u32 color = BLANCO, Mezcla mezcla = Mezcla::NORMAL);
    void dibujar_escalado(const RegionAtlas& r, float x, float y, float w, float h, u64 clave, u32 color = BLANCO,
                          Mezcla mezcla = Mezcla::NORMAL);
    void rectangulo(int x, int y, int w, int h, u64 clave, u32 color);
    // Este frame, lo que va por debajo de CAPA_EFECTOS (mundo, no interfaz) se multiplica por
    // `tinte`, por `brillo` (bw × bh celdas de celda × celda píxeles lógicos, 255 = sin cambio) y,
    // si se pasan, por el color de cada fila de la vista (`filas`, n_filas colores RGBA que cubren
    // la altura de la vista de arriba abajo; nullptr = blanco). Para solo tinte: brillo de 1 × 1 y una
    // celda mayor que la vista. Vale para el frame actual.
    void multiplicar_mundo(const u8* brillo, int bw, int bh, int celda, u32 tinte, const u32* filas = nullptr,
                           int n_filas = 0);
    // Este frame, el mundo se vuelve a escribir: `fase_ola` >= 0 lo desplaza por columnas (ola bajo el
    // agua; fase 0..99) y `modo_vision` lo transforma (1 elfo, 2 elfo oscuro; 0 ninguno).
    void reemplazar_mundo(int fase_ola, int modo_vision);
    // Mientras se dibuja una ventana (nivel 1..30 de abajo hacia arriba; 0 = ninguna): lo que va
    // en CAPA_UI y CAPA_UI_TEXTO se dibuja en CAPA_VENTANAS, todo junto a ese nivel. Así una
    // ventana tapa entera a las de abajo, su texto incluido.
    void nivel_ventana(u32 nivel) { nivel_ventana_ = nivel; }
    u32 nivel_ventana() const { return nivel_ventana_; }

    // Ordena la lista del frame (lo hace presentar si no se llamó antes; separado para medirlo
    // aparte de la espera del vsync).
    void preparar_frame();
    // Ordena si hace falta, sube y presenta el frame. Si hay una captura pedida, guarda la imagen lógica.
    void presentar();
    void pedir_captura(const std::string& ruta_bmp) { ruta_captura_ = ruta_bmp; }

    // Ventana (píxeles) -> coordenadas lógicas.
    void a_logico(float wx, float wy, int& lx, int& ly) const;

    int ancho() const { return ancho_; }
    // Píxeles reales por píxel lógico (la del último frame). Para redondear la cámara a 1/escala.
    int escala() const { return escala_; }
    int alto() const { return alto_; }
    u32 llamadas_ultimo_frame() const { return llamadas_; }
    u32 instancias_ultimo_frame() const { return instancias_previas_; }
    u32 paginas_usadas() const { return empaquetado_.paginas_usadas(); }
    u32 reemplazos_atlas() const { return empaquetado_.reemplazos(); }

private:
    struct SubidaPendiente {
        RegionAtlas r;
        u32 offset;  // en el buffer de subida
    };
    bool crear_pipeline();
    bool crear_destino();
    void ordenar();
    SDL_GPUCommandBuffer* guardar_captura(SDL_GPUCommandBuffer* cmd);

    SDL_Window* ventana_ = nullptr;
    SDL_GPUDevice* dev_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_brillo_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_mundo_ = nullptr;   // reemplazar el mundo (ola, visión)
    SDL_GPUTexture* copia_ = nullptr;                      // copia de destino_ para leerla al reemplazar
    SDL_GPUTexture* filas_tex_ = nullptr;                  // color por fila (1 × LUZ_TAM RGBA)
    SDL_GPUTransferBuffer* subida_filas_ = nullptr;
    std::vector<u32> brillo_filas_;
    bool reemplazo_activo_ = false;
    int reemplazo_fase_ = -1, reemplazo_modo_ = 0;
    SDL_GPUTexture* luz_ = nullptr;             // brillo del mundo (R8), tamaño fijo
    SDL_GPUTransferBuffer* subida_luz_ = nullptr;
    std::vector<u8> brillo_buf_;                // copia del frame, reservada al iniciar
    bool brillo_activo_ = false;
    int brillo_w_ = 0, brillo_h_ = 0, brillo_celda_ = 1;
    u32 brillo_tinte_ = 0xFFFFFFFFu;
    u32 n_mundo_ = 0;                           // instancias de CAPA_FONDO..CAPA_MUNDO (prefijo de la lista)
    SDL_GPUTexture* atlas_ = nullptr;
    SDL_GPUTexture* destino_ = nullptr;  // resolución lógica
    SDL_GPUSampler* muestreo_ = nullptr;
    SDL_GPUBuffer* instancias_gpu_ = nullptr;
    SDL_GPUTransferBuffer* subida_inst_ = nullptr;
    SDL_GPUTransferBuffer* subida_atlas_ = nullptr;
    int ancho_ = 0, alto_ = 0;            // tamaño lógico del frame actual
    int ancho_min_ = 0, alto_min_ = 0;
    int escala_ = 1;
    int destino_w_ = 0, destino_h_ = 0;   // tamaño real de destino_

    EmpaquetadoAtlas empaquetado_{TAM_PAGINA, PAGINAS_ATLAS};
    u64 frame_ = 1;
    std::vector<SubidaPendiente> pendientes_;
    std::vector<u8> staging_atlas_;
    bool atlas_lleno_avisado_ = false;

    // Lista de dibujo del frame.
    std::vector<Instancia> inst_;
    std::vector<u64> claves_;
    u32 nivel_ventana_ = 0;
    std::vector<u32> orden_, orden_tmp_;
    std::vector<Instancia> inst_ordenadas_;
    u32 llamadas_ = 0, instancias_previas_ = 0;
    RegionAtlas blanco_;

    std::string ruta_captura_;
    bool preparado_ = false;
};

}  // namespace lin

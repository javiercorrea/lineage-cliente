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
#include "recursos/modelo.h"
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

// Modelos 3D (rama agente/modelos-3d): cada modelo visible se dibuja en una ranura de una capa extra
// del atlas (PAGINA_MODELOS, fuera del reemplazo LRU) en un pase previo, y la ranura se dibuja como
// el frame de un sprite: así queda ordenada con el mundo 2D, se tiñe, se vuelve translúcida y se
// elige con el mouse igual que un sprite. Ver docs/analisis/PLAN_3D.md.
constexpr u16 PAGINA_MODELOS = PAGINAS_ATLAS;
constexpr int TAM_RANURA = 512;  // px reales (la ranura se dibuja a TAM_RANURA / escala px lógicos)
constexpr int RANURAS_MODELOS = (TAM_PAGINA / TAM_RANURA) * (TAM_PAGINA / TAM_RANURA);

// Malla de un modelo subida a la GPU (una vez, al cargarlo).
struct MallaGpu {
    SDL_GPUBuffer* vertices = nullptr;  // VerticeModelo (storage buffer: el shader los lee por índice)
    SDL_GPUBuffer* indices = nullptr;   // u32
    SDL_GPUTexture* textura = nullptr;
    u32 n_indices = 0;
    bool valida() const { return vertices && indices && textura; }
};

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
    // Modelos 3D. subir_malla una vez por modelo (bloquea hasta terminar); dibujar_modelo cada frame:
    // encola el modelo para el pase de modelos y devuelve la región de su ranura en el atlas (inválida
    // si no quedan ranuras), que se dibuja como cualquier otra (dibujar_escalado). `mvp` lleva el
    // espacio del modelo al de la ranura (NDC: x, y en −1..1, z en 0..1); `rotacion`, al de la cámara
    // (para la luz); `huesos`, las matrices de Modelo::matrices_huesos.
    bool subir_malla(const Modelo& m, MallaGpu& salida);
    void liberar_malla(MallaGpu& m);
    RegionAtlas dibujar_modelo(const MallaGpu& m, const float* mvp, const float* rotacion, const float (*huesos)[16], int n_huesos);
    // Mientras se dibuja una ventana (nivel 1..30 de abajo hacia arriba; 0 = ninguna): lo que va
    // en CAPA_UI y CAPA_UI_TEXTO se dibuja en CAPA_VENTANAS, todo junto a ese nivel. Así una
    // ventana tapa entera a las de abajo, su texto incluido.
    void nivel_ventana(u32 nivel) { nivel_ventana_ = nivel; }

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
    bool crear_pipeline_modelos();
    void pase_modelos(SDL_GPUCommandBuffer* cmd);
    bool crear_destino();
    void ordenar();
    SDL_GPUCommandBuffer* guardar_captura(SDL_GPUCommandBuffer* cmd);

    SDL_Window* ventana_ = nullptr;
    SDL_GPUDevice* dev_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_modelos_ = nullptr;
    SDL_GPUTexture* profundidad_modelos_ = nullptr;
    // Modelos del frame (los dibuja pase_modelos antes del pase principal). Uniformes del shader:
    // mvp, rotación (para la luz) y las matrices de los huesos.
    struct ModeloEncolado {
        const MallaGpu* malla;
        float uniformes[(2 + MAX_HUESOS) * 16];
        u32 bytes_uniformes;
    };
    ModeloEncolado modelos_[RANURAS_MODELOS];
    int n_modelos_ = 0;
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

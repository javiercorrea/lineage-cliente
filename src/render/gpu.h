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
// 6 MB de buffer. Una vista grande (5K con zoom ×1) ya pide ~55 000 solo de suelo, más la lluvia.
constexpr u32 MAX_INSTANCIAS = 1u << 17;
// Topes por capa, porque se descarta lo que llega con la lista llena y el suelo se manda primero: el
// fondo y el suelo pueden ocupar hasta MAX − 2 × RESERVA, los objetos, personajes y efectos hasta
// MAX − RESERVA, y la interfaz todo. Así en una vista enorme se pierde suelo del borde, no personajes
// ni la interfaz.
constexpr u32 RESERVA_CAPAS = 1u << 14;
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
    bool sin_luz = false;  // Modelo::sin_luz
    bool valida() const { return vertices && indices && textura; }
};

class Gpu {
public:
    // ancho/alto lógicos mínimos (los del original).
    bool iniciar(SDL_Window* ventana, int ancho_logico, int alto_logico);
    // Al comienzo de cada frame: fija la escala y el tamaño lógico según la ventana. `zoom` es la
    // escala deseada (píxeles reales por píxel lógico; 0 = la mayor que entra, como el original).
    // Sin `ampliable`, el tamaño lógico es el mínimo y la imagen queda centrada.
    // `franja`: píxeles lógicos reservados arriba (la barra de la ventana); el contenido va debajo.
    void ajustar_vista(bool ampliable, int zoom, int franja = 0);
    // Mayor escala con la que entra el tamaño mínimo en la ventana actual.
    int escala_maxima(int franja = 0) const;
    void cerrar();

    // Reserva lugar en el atlas y encola la subida. false si no hay lugar este frame.
    bool subir(const ImagenRGBA& img, RegionAtlas& salida);
    // false si la imagen es más grande que una página del atlas: no entraría nunca.
    bool cabe_en_atlas(const ImagenRGBA& img) const { return empaquetado_.cabe(img.ancho, img.alto); }
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
    // Este frame, lo que va por debajo de CAPA_EFECTOS (mundo, no interfaz) se multiplica por
    // `tinte`, por `brillo` (bw × bh celdas de celda × celda píxeles lógicos, 255 = sin cambio) y,
    // si se pasan, por el color de cada fila de la vista (`filas`, n_filas colores RGBA que cubren
    // la altura de la vista de arriba abajo; nullptr = blanco). Para solo tinte: brillo de 1 × 1 y una
    // celda mayor que la vista. Vale para el frame actual.
    // Deja lista la textura de brillo para bw × bh celdas, para no reservarla durante un frame.
    void reservar_brillo(int bw, int bh) { asegurar_luz(bw, bh); }
    void multiplicar_mundo(const u8* brillo, int bw, int bh, int celda, u32 tinte, const u32* filas = nullptr,
                           int n_filas = 0);
    // Este frame, el mundo se vuelve a escribir: `fase_ola` >= 0 lo desplaza por columnas (ola bajo el
    // agua; fase 0..99) y `modo_vision` lo transforma (1 elfo, 2 elfo oscuro; 0 ninguno).
    // `fase_borracho` >= 0 mece el mundo por filas (borrachera, S_Liquor 1; fase 0..99).
    void reemplazar_mundo(int fase_ola, int modo_vision, int fase_borracho = -1);
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
    // Ventana (píxeles) -> cuadrícula, sin descontar el origen del contenido.
    void a_grilla(float wx, float wy, int& gx, int& gy) const;
    // La cuadrícula cubre la ventana. El contenido (ancho() × alto()) va desplazado por el origen;
    // lo que va fuera del contenido (la barra) se dibuja en coordenadas negativas del contenido.
    int grid_ancho() const { return grid_w_; }
    int grid_alto() const { return grid_h_; }
    int origen_x() const { return origen_x_; }
    int origen_y() const { return origen_y_; }

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
    // Vacía la lista de dibujo y los efectos del frame (con o sin presentarlo).
    void terminar_frame(u32 instancias_dibujadas);
    bool asegurar_luz(int bw, int bh);
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
        float uniformes[(2 + MAX_HUESOS) * 16 + 4];  // + opciones (x: sin luz)
        u32 bytes_uniformes;
    };
    ModeloEncolado modelos_[RANURAS_MODELOS];
    int n_modelos_ = 0;
    SDL_GPUGraphicsPipeline* pipeline_brillo_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_mundo_ = nullptr;   // reemplazar el mundo (ola, visión)
    SDL_GPUTexture* copia_ = nullptr;                      // copia de destino_ para leerla al reemplazar
    SDL_GPUTexture* filas_tex_ = nullptr;                  // color por fila (1 × LUZ_TAM RGBA)
    SDL_GPUTransferBuffer* subida_filas_ = nullptr;
    std::vector<u32> brillo_filas_;
    bool reemplazo_activo_ = false;
    int reemplazo_fase_ = -1, reemplazo_modo_ = 0, reemplazo_fase_b_ = -1;
    SDL_GPUTexture* luz_ = nullptr;             // brillo del mundo (R8); crece con la vista
    SDL_GPUTransferBuffer* subida_luz_ = nullptr;
    std::vector<u8> brillo_buf_;                // copia del frame, reservada al iniciar
    bool brillo_activo_ = false;
    int luz_w_ = 0, luz_h_ = 0;                 // tamaño de luz_
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
    int grid_w_ = 0, grid_h_ = 0;         // cuadrícula de la ventana (píxeles lógicos, >= el contenido)
    int origen_x_ = 0, origen_y_ = 0;     // dónde empieza el contenido dentro de la cuadrícula
    int ancho_min_ = 0, alto_min_ = 0;
    int escala_ = 1;
    int destino_w_ = 0, destino_h_ = 0;   // tamaño real de destino_

    EmpaquetadoAtlas empaquetado_{TAM_PAGINA, PAGINAS_ATLAS};
    u64 frame_ = 1;
    std::vector<SubidaPendiente> pendientes_;
    std::vector<u8> staging_atlas_;
    bool atlas_lleno_avisado_ = false;
    u64 proximo_aviso_instancias_ = 0;  // frame desde el que se vuelve a avisar de la lista llena

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

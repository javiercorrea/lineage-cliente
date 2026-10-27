// Modelos 3D animados (.glb, glTF 2.0) para dibujar un gfx en lugar de su sprite (rama
// agente/modelos-3d; plan en docs/analisis/PLAN_3D.md).
//
// Convenciones de los modelos (las del proceso de prueba_3d): +Y arriba, el modelo mira a +Z, los
// pies en y mínima, una malla con skin (≤ MAX_HUESOS huesos), una textura base PNG y clips con el
// nombre de la acción ("quieto", "caminar", "atacar", "dano", "morir"). Caminar es en el lugar: el
// desplazamiento lo pone el juego.
//
// Las poses se muestrean en tiempo continuo (posición y escala lineales, rotación con slerp), así
// que a 120 fps se ven 120 poses distintas por segundo y no los pocos frames del sprite.
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "core/tipos.h"
#include "recursos/imagen.h"

namespace lin {

constexpr int MAX_HUESOS = 32;  // por modelo (las matrices van como uniformes del shader)
constexpr int MAX_NODOS = 64;
constexpr int RECORRIDO_N = 96;

struct VerticeModelo {
    float pos[3];
    float normal[3];
    float uv[2];
    u8 huesos[4];
    float pesos[4];
};

// Transformación local de un nodo: traslación, rotación (cuaternión x, y, z, w) y escala.
struct Trs {
    float t[3] = {0, 0, 0};
    float r[4] = {0, 0, 0, 1};
    float s[3] = {1, 1, 1};
};

struct Pose {
    Trs local[MAX_NODOS];
};

class Modelo {
public:
    // `glb`: el archivo completo. false si no es un glTF que se pueda usar (sin malla, sin skin,
    // demasiados huesos o nodos).
    bool cargar(Bytes glb);

    // Índice del clip con ese nombre, o -1.
    int clip(std::string_view nombre) const;
    float duracion(int clip) const { return clips_[static_cast<size_t>(clip)].duracion; }
    size_t cantidad_clips() const { return clips_.size(); }
    std::string_view nombre_clip(int clip) const { return clips_[static_cast<size_t>(clip)].nombre; }

    // La pose de reposo, y la de un clip en el instante t (segundos; en bucle, t se toma módulo la
    // duración; si no, se detiene en el último instante).
    void pose_reposo(Pose& p) const;
    void muestrear(int clip, float t, bool bucle, Pose& p) const;
    // `salida` = a + (b − a)·f, nodo por nodo (rotaciones con slerp).
    void mezclar(const Pose& a, const Pose& b, float f, Pose& salida) const;
    // Matrices de los huesos (global × inversa de bind), columna mayor, para el skinning.
    void matrices_huesos(const Pose& p, float (*salida)[16]) const;
    // Matrices globales de los nodos (la traslación de un hueso, en [12..14], es su articulación).
    void globales(const Pose& p, float (*salida)[16]) const;
    // Índice del nodo con ese nombre, o -1.
    int nodo(std::string_view nombre) const;
    // Recorrido del clip de caminar (unidades del modelo): r[i] = cuánto avanzó el cuerpo en el instante
    // i/RECORRIDO_N del ciclo, medido por lo que retrocede el pie apoyado. Es creciente;
    // r[RECORRIDO_N] es el ciclo entero (dos pasos). Todo 0 si no tiene nodos "pie.L" / "pie.R".
    void recorrido(int clip, float* r) const;

    const std::vector<VerticeModelo>& vertices() const { return vertices_; }
    const std::vector<u32>& indices() const { return indices_; }
    const ImagenRGBA& textura() const { return textura_; }
    int cantidad_huesos() const { return static_cast<int>(huesos_.size()); }
    // Caja de la malla en reposo.
    const float* minimo() const { return min_; }
    const float* maximo() const { return max_; }

private:
    struct Canal {
        u16 nodo;
        u8 camino;  // 0 traslación, 1 rotación, 2 escala
        u8 escalon; // interpolación STEP
        u32 primer_tiempo, n;
        u32 primer_valor;  // en valores_ (3 o 4 floats por clave)
    };
    struct Clip {
        std::string nombre;
        float duracion = 0;
        u32 primer_canal = 0, n_canales = 0;
    };

    std::vector<VerticeModelo> vertices_;
    std::vector<u32> indices_;
    ImagenRGBA textura_;
    std::vector<std::string> nombres_; // por nodo
    std::vector<i16> padre_;          // por nodo (-1 = raíz)
    std::vector<u16> orden_;          // nodos con el padre antes que los hijos
    std::vector<Trs> reposo_;         // por nodo
    std::vector<u16> huesos_;         // hueso -> nodo
    std::vector<float> bind_inversa_; // 16 por hueso
    std::vector<Clip> clips_;
    std::vector<Canal> canales_;
    std::vector<float> tiempos_, valores_;
    float min_[3] = {}, max_[3] = {};
};

// Matemática mínima, columna mayor (como glTF y Metal).
void mat_identidad(float* m);
void mat_mult(const float* a, const float* b, float* salida);  // salida = a · b
void mat_de_trs(const Trs& t, float* m);
void cuat_slerp(const float* a, const float* b, float f, float* salida);

}  // namespace lin

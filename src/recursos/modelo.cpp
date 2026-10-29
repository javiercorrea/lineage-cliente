#include "recursos/modelo.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace lin {

void mat_identidad(float* m) {
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1;
}

void mat_mult(const float* a, const float* b, float* salida) {
    float r[16];
    for (int c = 0; c < 4; ++c)
        for (int f = 0; f < 4; ++f) {
            float v = 0;
            for (int k = 0; k < 4; ++k) v += a[k * 4 + f] * b[c * 4 + k];
            r[c * 4 + f] = v;
        }
    std::memcpy(salida, r, sizeof r);
}

void mat_de_trs(const Trs& t, float* m) {
    const float x = t.r[0], y = t.r[1], z = t.r[2], w = t.r[3];
    m[0] = (1 - 2 * (y * y + z * z)) * t.s[0];
    m[1] = (2 * (x * y + z * w)) * t.s[0];
    m[2] = (2 * (x * z - y * w)) * t.s[0];
    m[3] = 0;
    m[4] = (2 * (x * y - z * w)) * t.s[1];
    m[5] = (1 - 2 * (x * x + z * z)) * t.s[1];
    m[6] = (2 * (y * z + x * w)) * t.s[1];
    m[7] = 0;
    m[8] = (2 * (x * z + y * w)) * t.s[2];
    m[9] = (2 * (y * z - x * w)) * t.s[2];
    m[10] = (1 - 2 * (x * x + y * y)) * t.s[2];
    m[11] = 0;
    m[12] = t.t[0];
    m[13] = t.t[1];
    m[14] = t.t[2];
    m[15] = 1;
}

void cuat_slerp(const float* a, const float* b, float f, float* salida) {
    float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    float s = 1;
    if (d < 0) {  // por el camino corto
        d = -d;
        s = -1;
    }
    float ka, kb;
    if (d > 0.9995f) {  // casi iguales: lineal (y se normaliza)
        ka = 1 - f;
        kb = f;
    } else {
        const float th = std::acos(d), sn = std::sin(th);
        ka = std::sin((1 - f) * th) / sn;
        kb = std::sin(f * th) / sn;
    }
    float n = 0;
    for (int i = 0; i < 4; ++i) {
        salida[i] = ka * a[i] + kb * s * b[i];
        n += salida[i] * salida[i];
    }
    n = 1.0f / std::sqrt(n);
    for (int i = 0; i < 4; ++i) salida[i] *= n;
}

namespace {

void leer(const cgltf_accessor* a, cgltf_size i, float* v, cgltf_size n) { cgltf_accessor_read_float(a, i, v, n); }

}  // namespace

bool Modelo::cargar(Bytes glb) {
    cgltf_options opciones{};
    cgltf_data* d = nullptr;
    if (cgltf_parse(&opciones, glb.data(), glb.size(), &d) != cgltf_result_success) return false;
    struct Liberar {
        cgltf_data* d;
        ~Liberar() { cgltf_free(d); }
    } liberar{d};
    if (cgltf_load_buffers(&opciones, d, nullptr) != cgltf_result_success) return false;
    if (d->nodes_count == 0 || d->nodes_count > MAX_NODOS || d->skins_count == 0 || d->meshes_count == 0) return false;
    const cgltf_skin& skin = d->skins[0];
    if (skin.joints_count == 0 || skin.joints_count > MAX_HUESOS) return false;

    // Nodos: padre, transformación de reposo y un orden con los padres primero.
    const size_t n_nodos = d->nodes_count;
    auto indice = [&](const cgltf_node* n) { return static_cast<int>(n - d->nodes); };
    padre_.assign(n_nodos, -1);
    nombres_.clear();
    reposo_.assign(n_nodos, Trs{});
    for (size_t i = 0; i < n_nodos; ++i) {
        const cgltf_node& n = d->nodes[i];
        if (n.parent) padre_[i] = static_cast<i16>(indice(n.parent));
        nombres_.emplace_back(n.name ? n.name : "");
        Trs& t = reposo_[i];
        if (n.has_translation) std::memcpy(t.t, n.translation, sizeof t.t);
        if (n.has_rotation) std::memcpy(t.r, n.rotation, sizeof t.r);
        if (n.has_scale) std::memcpy(t.s, n.scale, sizeof t.s);
    }
    orden_.clear();
    std::vector<bool> puesto(n_nodos, false);
    while (orden_.size() < n_nodos) {
        const size_t antes = orden_.size();
        for (size_t i = 0; i < n_nodos; ++i)
            if (!puesto[i] && (padre_[i] < 0 || puesto[static_cast<size_t>(padre_[i])])) {
                orden_.push_back(static_cast<u16>(i));
                puesto[i] = true;
            }
        if (orden_.size() == antes) return false;  // ciclo
    }

    huesos_.clear();
    bind_inversa_.assign(skin.joints_count * 16, 0);
    for (size_t j = 0; j < skin.joints_count; ++j) {
        huesos_.push_back(static_cast<u16>(indice(skin.joints[j])));
        if (skin.inverse_bind_matrices) leer(skin.inverse_bind_matrices, j, &bind_inversa_[j * 16], 16);
        else mat_identidad(&bind_inversa_[j * 16]);
    }

    // Malla: la primera primitiva de la primera malla.
    const cgltf_primitive& p = d->meshes[0].primitives[0];
    const cgltf_accessor *pos = nullptr, *nor = nullptr, *uv = nullptr, *jo = nullptr, *we = nullptr;
    for (size_t i = 0; i < p.attributes_count; ++i) {
        const cgltf_attribute& at = p.attributes[i];
        if (at.type == cgltf_attribute_type_position) pos = at.data;
        else if (at.type == cgltf_attribute_type_normal) nor = at.data;
        else if (at.type == cgltf_attribute_type_texcoord && at.index == 0) uv = at.data;
        else if (at.type == cgltf_attribute_type_joints && at.index == 0) jo = at.data;
        else if (at.type == cgltf_attribute_type_weights && at.index == 0) we = at.data;
    }
    if (!pos || !jo || !we) return false;
    vertices_.assign(pos->count, VerticeModelo{});
    for (int k = 0; k < 3; ++k) {
        min_[k] = 1e9f;
        max_[k] = -1e9f;
    }
    for (size_t i = 0; i < pos->count; ++i) {
        VerticeModelo& v = vertices_[i];
        leer(pos, i, v.pos, 3);
        if (nor) leer(nor, i, v.normal, 3);
        if (uv) leer(uv, i, v.uv, 2);
        cgltf_uint h[4] = {};
        cgltf_accessor_read_uint(jo, i, h, 4);
        for (int k = 0; k < 4; ++k) v.huesos[k] = static_cast<u8>(std::min<cgltf_uint>(h[k], skin.joints_count - 1));
        leer(we, i, v.pesos, 4);
        const float suma = v.pesos[0] + v.pesos[1] + v.pesos[2] + v.pesos[3];
        for (float& w : v.pesos) w = suma > 0 ? w / suma : 0;
        for (int k = 0; k < 3; ++k) {
            min_[k] = std::min(min_[k], v.pos[k]);
            max_[k] = std::max(max_[k], v.pos[k]);
        }
    }
    indices_.clear();
    if (p.indices) {
        indices_.resize(p.indices->count);
        for (size_t i = 0; i < p.indices->count; ++i) indices_[i] = static_cast<u32>(cgltf_accessor_read_index(p.indices, i));
    } else {
        for (u32 i = 0; i < pos->count; ++i) indices_.push_back(i);
    }

    // Textura base (PNG dentro del .glb).
    textura_ = {};
    sin_luz_ = p.material && p.material->unlit;
    if (p.material && p.material->has_pbr_metallic_roughness) {
        const cgltf_texture* tx = p.material->pbr_metallic_roughness.base_color_texture.texture;
        if (tx && tx->image && tx->image->buffer_view) {
            const cgltf_buffer_view* bv = tx->image->buffer_view;
            const u8* datos = static_cast<const u8*>(bv->buffer->data) + bv->offset;
            decodificar_png(Bytes(datos, bv->size), textura_, false);
        }
    }

    // Clips.
    clips_.clear();
    canales_.clear();
    tiempos_.clear();
    valores_.clear();
    for (size_t a = 0; a < d->animations_count; ++a) {
        const cgltf_animation& an = d->animations[a];
        Clip c;
        c.nombre = an.name ? an.name : "";
        c.primer_canal = static_cast<u32>(canales_.size());
        for (size_t k = 0; k < an.channels_count; ++k) {
            const cgltf_animation_channel& ch = an.channels[k];
            if (!ch.target_node || !ch.sampler) continue;
            u8 camino;
            if (ch.target_path == cgltf_animation_path_type_translation) camino = 0;
            else if (ch.target_path == cgltf_animation_path_type_rotation) camino = 1;
            else if (ch.target_path == cgltf_animation_path_type_scale) camino = 2;
            else continue;
            const cgltf_accessor* in = ch.sampler->input;
            const cgltf_accessor* out = ch.sampler->output;
            const int comp = camino == 1 ? 4 : 3;
            Canal cn{static_cast<u16>(indice(ch.target_node)), camino,
                     static_cast<u8>(ch.sampler->interpolation == cgltf_interpolation_type_step), static_cast<u32>(tiempos_.size()),
                     static_cast<u32>(in->count), static_cast<u32>(valores_.size())};
            const bool cubica = ch.sampler->interpolation == cgltf_interpolation_type_cubic_spline;
            for (size_t i = 0; i < in->count; ++i) {
                float t;
                leer(in, i, &t, 1);
                tiempos_.push_back(t);
                c.duracion = std::max(c.duracion, t);
                float v[4];
                leer(out, cubica ? i * 3 + 1 : i, v, static_cast<cgltf_size>(comp));  // cúbica: el valor, sin tangentes
                valores_.insert(valores_.end(), v, v + comp);
            }
            canales_.push_back(cn);
        }
        c.n_canales = static_cast<u32>(canales_.size()) - c.primer_canal;
        clips_.push_back(std::move(c));
    }
    return !vertices_.empty() && !indices_.empty();
}

int Modelo::clip(std::string_view nombre) const {
    for (size_t i = 0; i < clips_.size(); ++i)
        if (clips_[i].nombre == nombre) return static_cast<int>(i);
    return -1;
}

void Modelo::pose_reposo(Pose& p) const {
    for (size_t i = 0; i < reposo_.size(); ++i) p.local[i] = reposo_[i];
}

void Modelo::muestrear(int clip, float t, bool bucle, Pose& p) const {
    pose_reposo(p);
    if (clip < 0 || static_cast<size_t>(clip) >= clips_.size()) return;
    const Clip& c = clips_[static_cast<size_t>(clip)];
    if (c.duracion > 0) t = bucle ? std::fmod(std::max(t, 0.0f), c.duracion) : std::clamp(t, 0.0f, c.duracion);
    for (u32 k = c.primer_canal; k < c.primer_canal + c.n_canales; ++k) {
        const Canal& cn = canales_[k];
        const float* ts = &tiempos_[cn.primer_tiempo];
        const int comp = cn.camino == 1 ? 4 : 3;
        // Clave anterior a t (búsqueda binaria) y la siguiente.
        const u32 sig = static_cast<u32>(std::upper_bound(ts, ts + cn.n, t) - ts);
        const u32 i0 = sig == 0 ? 0 : sig - 1, i1 = std::min(sig, cn.n - 1);
        float f = 0;
        if (i1 != i0 && !cn.escalon) f = (t - ts[i0]) / (ts[i1] - ts[i0]);
        const float* a = &valores_[cn.primer_valor + i0 * comp];
        const float* b = &valores_[cn.primer_valor + i1 * comp];
        Trs& d = p.local[cn.nodo];
        if (cn.camino == 1) {
            cuat_slerp(a, b, f, d.r);
        } else {
            float* v = cn.camino == 0 ? d.t : d.s;
            for (int i = 0; i < 3; ++i) v[i] = a[i] + (b[i] - a[i]) * f;
        }
    }
}

void Modelo::mezclar(const Pose& a, const Pose& b, float f, Pose& salida) const {
    for (size_t n = 0; n < reposo_.size(); ++n) {
        const Trs &x = a.local[n], &y = b.local[n];
        Trs& z = salida.local[n];
        for (int i = 0; i < 3; ++i) {
            z.t[i] = x.t[i] + (y.t[i] - x.t[i]) * f;
            z.s[i] = x.s[i] + (y.s[i] - x.s[i]) * f;
        }
        cuat_slerp(x.r, y.r, f, z.r);
    }
}

void Modelo::globales(const Pose& p, float (*global)[16]) const {
    for (u16 n : orden_) {
        float local[16];
        mat_de_trs(p.local[n], local);
        if (padre_[n] < 0) std::memcpy(global[n], local, sizeof local);
        else mat_mult(global[padre_[n]], local, global[n]);
    }
}

void Modelo::matrices_huesos(const Pose& p, float (*salida)[16]) const {
    float global[MAX_NODOS][16];
    globales(p, global);
    for (size_t j = 0; j < huesos_.size(); ++j) mat_mult(global[huesos_[j]], &bind_inversa_[j * 16], salida[j]);
}

int Modelo::nodo(std::string_view nombre) const {
    for (size_t i = 0; i < nombres_.size(); ++i)
        if (nombres_[i] == nombre) return static_cast<int>(i);
    return -1;
}

void Modelo::recorrido(int clip, float* r) const {
    for (int i = 0; i <= RECORRIDO_N; ++i) r[i] = 0;
    const int pies[2] = {nodo("pie.L"), nodo("pie.R")};
    if (clip < 0 || pies[0] < 0 || pies[1] < 0) return;
    // El pie apoyado es el que retrocede (−Z: el modelo mira a +Z), y si retroceden los dos, el más
    // bajo; lo que retrocede es lo que avanza el cuerpo. No sirve "el más bajo" a secas: sin IK, la
    // pierna estirada hacia atrás sube (péndulo) y el pie de adelante queda más bajo antes de
    // terminar de avanzar. Con un mínimo por tramo: si nadie retrocede, la inversa saltaría ese tramo.
    float z[RECORRIDO_N + 1][2], y[RECORRIDO_N + 1][2];
    Pose p;
    float g[MAX_NODOS][16];
    for (int i = 0; i <= RECORRIDO_N; ++i) {
        muestrear(clip, duracion(clip) * static_cast<float>(i) / RECORRIDO_N, true, p);
        globales(p, g);
        for (int k = 0; k < 2; ++k) {
            y[i][k] = g[pies[k]][13];
            z[i][k] = g[pies[k]][14];
        }
    }
    float d[RECORRIDO_N], total = 0;
    for (int i = 0; i < RECORRIDO_N; ++i) {
        const float atras[2] = {z[i][0] - z[i + 1][0], z[i][1] - z[i + 1][1]};
        const int k = atras[0] > 0 && atras[1] > 0 ? (y[i][0] <= y[i][1] ? 0 : 1) : (atras[0] > atras[1] ? 0 : 1);
        d[i] = std::max(0.0f, atras[k]);
        total += d[i];
    }
    if (total <= 0) return;
    const float minimo = 0.15f * total / RECORRIDO_N;
    for (int i = 0; i < RECORRIDO_N; ++i) r[i + 1] = r[i] + std::max(d[i], minimo);
}

}  // namespace lin

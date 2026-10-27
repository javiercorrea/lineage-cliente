// Modelos 3D en lugar de sprites (rama agente/modelos-3d; plan en docs/analisis/PLAN_3D.md).
//
//  - <modelos>/modelos.txt asigna gfx → .glb con el registro de cámara de prueba_3d (giro, sentido
//    y elevación: la dirección h de Lineage se ve desde el azimut giro + sentido·45·h) y la escala
//    (px por unidad del modelo, medida contra el sprite). Los modelos no van en el repo: derivan del
//    arte del juego y se generan en la máquina del usuario.
//  - Cada frame, el objeto con modelo toma el clip de su acción: morir (detenido al final), la acción
//    puntual (ataque, daño), caminar o quieto. El tiempo del clip se estira a la duración de la acción
//    del sprite (el servidor mide los intervalos con el sprite), caminar avanza un ciclo cada dos
//    pasos sin reiniciarse entre pasos, entre clips hay una transición y el giro entre direcciones es
//    gradual. La pose se muestrea en tiempo continuo: una distinta en cada frame.
//  - El modelo se dibuja en su ranura del atlas con una cámara ortográfica y la ranura se ubica con
//    los pies en la posición continua del objeto (los clips caminan en el lugar; los sprites no).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "app/app.h"

namespace lin {

namespace {

constexpr float PI = 3.14159265f;
constexpr u64 MS_TRANSICION = 150;
constexpr float GRADOS_POR_MS = 0.9f;        // giro: 90° en 100 ms
constexpr u64 MS_PAUSA_ENTRE_PASOS = 400;    // hasta esto sin caminar, el ciclo se congela (no pasa a quieto)
constexpr float PIE_EN_RANURA = 0.8f;        // los pies, al 80 % del alto de la ranura
constexpr float Z_POR_UNIDAD = 0.08f;        // profundidad en la ranura (NDC 0..1)
enum ClipBase { CLIP_QUIETO, CLIP_CAMINAR, CLIP_ATACAR, CLIP_DANO, CLIP_MORIR, CLIPS_BASE };
constexpr const char* NOMBRES_CLIP[CLIPS_BASE] = {"quieto", "caminar", "atacar", "dano", "morir"};

ModeloGfx* modelo_de(App& a, GfxId gfx) {
    const u32* i = a.indice_modelos.buscar(gfx);
    return i ? &a.modelos[*i] : nullptr;
}

// Estado de animación del objeto (se reusa el visto hace más tiempo si no hay lugar).
AnimModelo& anim_de(App& a, ObjId id, u64 ahora) {
    AnimModelo* viejo = &a.anim_modelos[0];
    for (AnimModelo& am : a.anim_modelos) {
        if (am.id == id) return am;
        if (am.visto < viejo->visto) viejo = &am;
    }
    *viejo = AnimModelo{};
    viejo->id = id;
    viejo->visto = ahora;
    return *viejo;
}

float diferencia_angulo(float a, float b) {  // b − a por el camino corto, en grados
    float d = std::fmod(b - a, 360.0f);
    if (d > 180) d -= 360;
    if (d < -180) d += 360;
    return d;
}

}  // namespace

void cargar_modelos(App& a) {
    if (a.cfg.modelos.empty()) return;
    std::string carpeta = a.cfg.modelos;
    if (carpeta.back() != '/') carpeta += '/';
    std::vector<u8> texto;
    if (!leer_archivo(carpeta + "modelos.txt", texto)) {
        LIN_LOG("Aviso: no hay %smodelos.txt (modelos 3D)", carpeta.c_str());
        return;
    }
    std::string_view t(reinterpret_cast<const char*>(texto.data()), texto.size());
    a.modelos.reserve(64);
    while (!t.empty()) {
        std::string_view linea = t.substr(0, t.find('\n'));
        t.remove_prefix(std::min(t.size(), linea.size() + 1));
        if (linea.empty() || linea[0] == '#') continue;
        char archivo[256] = {};
        unsigned gfx = 0;
        float giro = 0, sentido = 1, elevacion = 20, px = 30;
        if (std::sscanf(std::string(linea).c_str(), "%u %255s %f %f %f %f", &gfx, archivo, &giro, &sentido, &elevacion, &px) < 2)
            continue;
        std::vector<u8> glb;
        ModeloGfx m;
        if (!leer_archivo(carpeta + archivo, glb) || !m.modelo.cargar(glb)) {
            LIN_LOG("Aviso: no se pudo cargar el modelo %s%s", carpeta.c_str(), archivo);
            continue;
        }
        if (!a.gpu.subir_malla(m.modelo, m.malla)) continue;
        m.gfx = static_cast<GfxId>(gfx);
        m.giro = giro;
        m.sentido = sentido;
        m.elevacion = elevacion;
        m.px_unidad = px;
        for (int c = 0; c < CLIPS_BASE; ++c) m.clip[c] = m.modelo.clip(NOMBRES_CLIP[c]);
        m.modelo.recorrido(m.clip[CLIP_CAMINAR], m.recorrido);
        a.indice_modelos.poner(m.gfx, static_cast<u32>(a.modelos.size()));
        a.modelos.push_back(std::move(m));
        LIN_LOG("Modelo 3D para el gfx %u: %s (%zu clips, zancada %.2f)", gfx, archivo, a.modelos.back().modelo.cantidad_clips(),
                a.modelos.back().recorrido[RECORRIDO_N] / 2);
    }
}

void liberar_modelos(App& a) {
    for (ModeloGfx& m : a.modelos) a.gpu.liberar_malla(m.malla);
    a.modelos.clear();
    a.indice_modelos.limpiar();
}

bool preparar_modelo(App& a, Slot s, double fx, double fy, double ox, double oy, u64 ahora, DibujoModelo& d) {
    if (!a.cfg.modelos_3d || a.modelos.empty()) return false;
    TablaObjetos& o = a.mundo.objetos;
    const GfxId gfx = o.col<O_GFX>()[s];
    ModeloGfx* mg = modelo_de(a, gfx);
    if (!mg) return false;
    const Modelo& m = mg->modelo;
    AnimModelo& am = anim_de(a, o.ids()[s], ahora);

    // Giro gradual hacia la dirección del objeto.
    const u64 dt = am.visto && ahora - am.visto < 1000 ? ahora - am.visto : 0;
    const float objetivo = mg->giro + mg->sentido * 45.0f * o.col<O_DIR>()[s];
    if (dt == 0) {
        am.yaw = objetivo;
    } else {
        const float dif = diferencia_angulo(am.yaw, objetivo), paso = GRADOS_POR_MS * static_cast<float>(dt);
        am.yaw += std::fabs(dif) <= paso ? dif : (dif > 0 ? paso : -paso);
    }
    am.visto = ahora;
    const double fx_ant = am.fx, fy_ant = am.fy;
    am.fx = fx;
    am.fy = fy;
    const float ay = -am.yaw * PI / 180, e = mg->elevacion * PI / 180;
    const float ca = std::cos(ay), sa = std::sin(ay), ce = std::cos(e), se = std::sin(e);

    // Clip y tiempo, con la duración de la acción del sprite.
    int base = CLIP_QUIETO;
    float frac = 0;   // 0..1 de la acción (puntual o morir)
    bool bucle = true;
    const u8 estado = o.col<O_ESTADO>()[s];
    const u8 accion = o.col<O_ACCION>()[s];
    auto duracion = [&](u8 acc) {
        u32 ms = a.sprites.duracion_ms(gfx, acc);
        return ms ? ms : 1000u;
    };
    if (estado == ESTADO_MUERTO) {
        base = CLIP_MORIR;
        frac = static_cast<float>(ahora - o.col<O_TACCION>()[s]) / static_cast<float>(duracion(ACC_MORIR));
        bucle = false;
    } else if (accion != 0xFF && ahora >= o.col<O_TACCION>()[s]) {
        const u32 dur = a.sprites.accion(gfx, static_cast<u8>(accion + estado)) ? duracion(static_cast<u8>(accion + estado)) : duracion(accion);
        frac = static_cast<float>(ahora - o.col<O_TACCION>()[s]) / static_cast<float>(dur);
        if (frac >= 1) {
            o.col<O_ACCION>()[s] = 0xFF;  // terminó (lo mismo que hace el sprite)
        } else {
            base = accion == ACC_DANADO ? CLIP_DANO : accion == ACC_MORIR ? CLIP_MORIR : CLIP_ATACAR;
            bucle = false;
        }
    }
    float t;
    if (!bucle) {
        t = std::min(frac, 1.0f);
    } else if (caminando(a.mundo, s, ahora)) {
        base = CLIP_CAMINAR;
        // Sin patinar: la fase sale de lo que avanzó el cuerpo, no del reloj. El pie apoyado retrocede
        // en el clip exactamente eso (recorrido), así que queda fijo en el piso aunque el clip no lo
        // mueva a velocidad constante. El avance se mide en pantalla y se pasa a unidades del modelo
        // con lo que mide en pantalla una unidad hacia adelante del modelo.
        const float* r = mg->recorrido;
        const double paso_px = std::hypot((fx - fx_ant + fy - fy_ant) * 24, (fy - fy_ant - fx + fx_ant) * 12);
        if (r[RECORRIDO_N] > 0 && dt && paso_px < 48) {
            am.distancia += paso_px / (mg->px_unidad * std::hypot(sa, se * ca));
            am.distancia = std::fmod(am.distancia, static_cast<double>(r[RECORRIDO_N]));
            const int i = static_cast<int>(std::upper_bound(r, r + RECORRIDO_N + 1, static_cast<float>(am.distancia)) - r) - 1;
            const float f = (static_cast<float>(am.distancia) - r[i]) / (r[i + 1] - r[i]);
            am.fase = (static_cast<float>(i) + f) / RECORRIDO_N;
        } else if (r[RECORRIDO_N] <= 0) {
            am.fase = std::fmod(am.fase + static_cast<float>(dt) / (2.0f * std::max<float>(o.col<O_DURMOV>()[s], 200)), 1.0f);  // un ciclo cada dos pasos
        }
        am.fin_caminar = ahora;
        t = static_cast<float>(am.fase);
    } else if (ahora - am.fin_caminar < MS_PAUSA_ENTRE_PASOS && am.clip == mg->clip[CLIP_CAMINAR] && am.clip >= 0) {
        // Entre un paso y el siguiente (los monstruos se detienen un momento): el ciclo se detiene
        // donde está, con el pie apoyado quieto, en vez de pasar a "quieto" y volver.
        base = CLIP_CAMINAR;
        t = static_cast<float>(am.fase);
    } else {
        t = std::fmod(static_cast<float>(ahora) / 2000.0f, 1.0f);
    }
    int clip = mg->clip[base];
    if (clip < 0) clip = mg->clip[CLIP_QUIETO];
    const float t_clip = clip >= 0 ? t * m.duracion(clip) : 0;

    // Transición desde el clip anterior.
    if (clip != am.clip) {
        am.clip_ant = am.clip;
        am.t_ant = am.t;
        am.bucle_ant = am.bucle;
        am.cambio = ahora;
        am.clip = clip;
    }
    am.t = t_clip;
    am.bucle = bucle;
    Pose pose, anterior;
    m.muestrear(clip, t_clip, bucle, pose);
    if (am.clip_ant >= 0 && ahora - am.cambio < MS_TRANSICION) {
        m.muestrear(am.clip_ant, am.t_ant, am.bucle_ant, anterior);
        m.mezclar(anterior, pose, static_cast<float>(ahora - am.cambio) / MS_TRANSICION, pose);
    }

    // Cámara ortográfica: la dirección se ve desde el azimut yaw, con la elevación del registro. En
    // lugar de mover la cámara se gira el modelo −yaw sobre Y. Filas de la rotación a la cámara:
    // derecha, arriba y hacia la cámara.
    const float rot[3][3] = {{ca, 0, sa}, {se * sa, ce, -se * ca}, {-ce * sa, se, ce * ca}};
    const float escala = static_cast<float>(a.gpu.escala());
    const float k = 2 * mg->px_unidad * escala / TAM_RANURA;  // unidades del modelo -> NDC de la ranura
    // Ancla como en el registro de prueba_3d: el borde de abajo y el centro horizontal de la caja del
    // modelo proyectada en esta dirección (con la cámara elevada, la punta de los pies queda más abajo
    // que el punto (0, y mínima, 0)).
    float xmin = 1e9f, xmax = -1e9f, pie_y = 1e9f;
    for (int i = 0; i < 8; ++i) {
        const float p[3] = {(i & 1) ? m.maximo()[0] : m.minimo()[0], (i & 2) ? m.maximo()[1] : m.minimo()[1],
                            (i & 4) ? m.maximo()[2] : m.minimo()[2]};
        const float x = rot[0][0] * p[0] + rot[0][2] * p[2], y = rot[1][0] * p[0] + rot[1][1] * p[1] + rot[1][2] * p[2];
        xmin = std::min(xmin, x);
        xmax = std::max(xmax, x);
        pie_y = std::min(pie_y, y);
    }
    float mvp[16] = {}, rotacion[16] = {};
    for (int c = 0; c < 3; ++c) {
        mvp[c * 4 + 0] = k * rot[0][c];
        mvp[c * 4 + 1] = k * rot[1][c];
        mvp[c * 4 + 2] = -Z_POR_UNIDAD * rot[2][c];
        for (int f = 0; f < 3; ++f) rotacion[c * 4 + f] = rot[f][c];
    }
    mvp[12] = -k * (xmin + xmax) / 2;
    mvp[13] = -k * pie_y - (2 * PIE_EN_RANURA - 1);
    mvp[14] = 0.5f;
    mvp[15] = rotacion[15] = 1;
    float huesos[MAX_HUESOS][16];
    m.matrices_huesos(pose, huesos);
    d.region = a.gpu.dibujar_modelo(mg->malla, mvp, rotacion, huesos, m.cantidad_huesos());
    if (!d.region.valida()) return false;  // sin ranuras libres: el sprite

    // Ubicación: los pies en el centro del rombo de la posición continua.
    const float pie_x = static_cast<float>(mundo_px(fx, fy) + ox) + 24, pie_y_pant = static_cast<float>(mundo_py(fx, fy) + oy) + 12;
    d.lado = TAM_RANURA / escala;
    d.x = pie_x - d.lado / 2;
    d.y = pie_y_pant - d.lado * PIE_EN_RANURA;
    // Rectángulo para el mouse y el nombre: la caja del modelo en reposo.
    float ancho = 0;
    for (int i : {0, 2}) ancho = std::max({ancho, std::fabs(m.minimo()[i]), std::fabs(m.maximo()[i])});
    const float alto = (m.maximo()[1] - m.minimo()[1]) * ce * mg->px_unidad;
    d.x0 = pie_x - ancho * 0.8f * mg->px_unidad;
    d.x1 = pie_x + ancho * 0.8f * mg->px_unidad;
    d.y0 = pie_y_pant - alto;
    d.y1 = pie_y_pant + 4;

    // Medición (LIN_MODELO_PIES=1): dónde cae en pantalla el pie apoyado (el más bajo en altura, no
    // en pantalla: con la cámara elevada el pie de adelante se ve más abajo), relativo al ancla del
    // cuerpo; la posición absoluta se arma afuera en double (en float, a 1.5 M px, se pierde precisión).
    static const bool medir_pies = std::getenv("LIN_MODELO_PIES") != nullptr;
    if (medir_pies && m.nodo("pie.L") >= 0) {
        float g[MAX_NODOS][16];
        m.globales(pose, g);
        const float* q = &g[m.nodo("pie.L")][12];
        const float* r = &g[m.nodo("pie.R")][12];
        const int cual = r[1] < q[1] ? 1 : 0;
        if (cual) q = r;
        const float rx = (rot[0][0] * q[0] + rot[0][2] * q[2] - (xmin + xmax) / 2) * mg->px_unidad;
        const float ry = -(rot[1][0] * q[0] + rot[1][1] * q[1] + rot[1][2] * q[2] - pie_y) * mg->px_unidad;
        std::fprintf(stderr, "PIES %u %llu %d %d %.3f %.3f %.4f %.4f\n", o.ids()[s], static_cast<unsigned long long>(ahora), base, cual,
                     rx, ry, fx, fy);
    }
    return true;
}

}  // namespace lin

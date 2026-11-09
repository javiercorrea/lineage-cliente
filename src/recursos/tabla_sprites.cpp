#include <algorithm>
#include <cctype>
#include "plataforma/archivo_mapeado.h"
#include "recursos/tabla_sprites.h"

namespace lin {

namespace {

// Lector de enteros sobre un registro, tolerante a los separadores raros del formato.
struct Cursor {
    const char* p;
    const char* fin;

    void saltar_espacios() {
        while (p < fin && std::isspace(static_cast<u8>(*p))) ++p;
    }
    bool hay_numero() {
        saltar_espacios();
        return p < fin && (std::isdigit(static_cast<u8>(*p)) || (*p == '-' && p + 1 < fin && std::isdigit(static_cast<u8>(p[1]))));
    }
    int numero() {
        saltar_espacios();
        int signo = 1, v = 0;
        if (p < fin && *p == '-') { signo = -1; ++p; }
        while (p < fin && std::isdigit(static_cast<u8>(*p))) v = v * 10 + (*p++ - '0');
        return signo * v;
    }
    // Número pegado a un sufijo ("4<41", "3[250"): solo los dígitos que siguen sin espacios. Un
    // sufijo suelto ("4>", "6!") no lleva número y no debe comerse el del frame siguiente.
    int numero_pegado() {
        int v = 0;
        while (p < fin && std::isdigit(static_cast<u8>(*p))) v = v * 10 + (*p++ - '0');
        return v;
    }
    // Avanza hasta el próximo dígito (como el parser de l1j-client entre campos de un frame).
    void hasta_digito() {
        while (p < fin && !std::isdigit(static_cast<u8>(*p))) ++p;
    }
};

}  // namespace

bool TablaSprites::cargar(const std::vector<u8>& texto) {
    frames_.clear();
    rangos_.clear();
    indice_.limpiar();
    alias_.clear();
    sombra_.clear();
    tipo_.clear();
    atributos_.clear();
    siguiente_.clear();
    const char* p = reinterpret_cast<const char*>(texto.data());
    const char* fin = p + texto.size();
    if (p < fin) ++p;  // el primer byte del archivo no es parte del registro 0
    for (GfxId gfx = 0; p < fin; ++gfx) {
        const char* fin_reg = static_cast<const char*>(std::memchr(p, 0, fin - p));
        if (!fin_reg) fin_reg = fin;
        Cursor c{p, fin_reg};
        alias_.push_back(gfx);
        sombra_.push_back(-1);
        tipo_.push_back(0);
        atributos_.push_back(0);
        siguiente_.push_back(0);
        u8 velocidad = 24;       // "110 N": vale para las acciones siguientes del registro
        c.numero();              // cantidad de sub-sprites
        if (c.p < c.fin && *c.p == '=') {
            ++c.p;
            alias_.back() = static_cast<GfxId>(c.numero());
        }
        while (c.hay_numero()) {
            int v = c.numero();
            switch (v) {
            case 100: {  // switches: n × 4 números
                int n = c.numero();
                for (int i = 0; i < n * 4 && c.hay_numero(); ++i) c.numero();
                break;
            }
            case 101: sombra_.back() = static_cast<i16>(c.numero()); break;
            case 102: tipo_.back() = static_cast<u8>(c.numero()); break;
            case 104: atributos_.back() = static_cast<u16>(c.numero()); break;
            case 103: case 108: case 111: case 112: case 113: case 114: c.numero(); break;
            case 105: {  // ropa: n números
                int n = c.numero();
                for (int i = 0; i < n && c.hay_numero(); ++i) c.numero();
                break;
            }
            case 106: break;
            case 107: c.numero(); c.numero(); break;
            case 109: {  // "109 n gfx": al terminar (el proyectil, al llegar) sigue ese gráfico; -1 ninguno
                c.numero();
                int g = c.numero();
                siguiente_.back() = static_cast<GfxId>(g > 0 && g < 0xFFFF ? g : 0);
                break;
            }
            case 110: {
                int n = c.numero();
                velocidad = static_cast<u8>(n > 0 && n < 256 ? n : 24);
                break;
            }
            default: {
                if (c.p < c.fin && *c.p == '=') {  // acción alias de otra: no se usa
                    ++c.p;
                    c.numero();
                    break;
                }
                int dir = c.numero();
                c.hasta_digito();
                int n = c.numero();
                RangoAccion r{static_cast<u32>(frames_.size()), 0, static_cast<u8>(dir), velocidad};
                for (int i = 0; i < n && c.p < c.fin; ++i) {
                    FrameAccion f{};
                    f.sonido = -1;
                    f.sub = static_cast<u16>(c.numero());
                    c.hasta_digito();
                    f.frame = static_cast<u16>(c.numero());
                    c.hasta_digito();
                    f.duracion = static_cast<u8>(c.numero());
                    while (c.p < c.fin && !std::isspace(static_cast<u8>(*c.p))) {  // sufijos
                        char s = *c.p++;
                        if (s == '!') f.impacto = true;
                        else if (s == '[' || s == '<') f.sonido = static_cast<i16>(c.numero_pegado());
                        else if (s == ']' || s == '^' || s == '>' || s == '{') c.numero_pegado();
                    }
                    frames_.push_back(f);
                    ++r.cantidad;
                }
                if (v >= 0 && v < 256 && r.cantidad > 0) {
                    indice_.poner((static_cast<u64>(gfx) << 8) | static_cast<u64>(v), static_cast<u32>(rangos_.size()));
                    rangos_.push_back(r);
                }
            }
            }
        }
        p = fin_reg + 1;
    }
    copiar_filas_faltantes();
    return !rangos_.empty();
}

// shortcut: esta tabla (l1j-client) llega al gfx 8682; la del juego va cifrada en list.spz. Se agregan a
// mano los gfx conocidos que no tiene, hasta tener la real.
void TablaSprites::copiar_filas_faltantes() {
    // Dark elf 75 y 80 (polymorphs): los de 52 a 70 usan todos los archivos de 1125.
    static constexpr std::pair<GfxId, GfxId> FILAS[] = {{9225, 6160}, {9226, 6160}};
    for (const auto& [nuevo, origen] : FILAS) {
        if (origen >= alias_.size()) continue;  // tabla reducida (tests)
        if (nuevo >= alias_.size()) {
            const size_t n = nuevo + 1u;
            for (size_t g = alias_.size(); g < n; ++g) alias_.push_back(static_cast<GfxId>(g));
            sombra_.resize(n, -1);
            tipo_.resize(n, 0);
            atributos_.resize(n, 0);
            siguiente_.resize(n, 0);
        }
        alias_[nuevo] = alias_[origen];
        sombra_[nuevo] = sombra_[origen];
        tipo_[nuevo] = tipo_[origen];
        atributos_[nuevo] = atributos_[origen];
        siguiente_[nuevo] = siguiente_[origen];
        for (u64 a = 0; a < 256; ++a)
            if (const u32* i = indice_.buscar((static_cast<u64>(origen) << 8) | a)) {
                const u32 rango = *i;  // poner() puede mover la tabla
                indice_.poner((static_cast<u64>(nuevo) << 8) | a, rango);
            }
    }
}

bool TablaSprites::cargar_archivo(const std::string& ruta) {
    std::vector<u8> t;
    return leer_archivo(ruta, t) && cargar(t);
}

const RangoAccion* TablaSprites::accion(GfxId gfx, u8 accion) const {
    const u32* i = indice_.buscar((static_cast<u64>(gfx) << 8) | accion);
    return i ? &rangos_[*i] : nullptr;
}

u32 TablaSprites::ticks(const RangoAccion& r) const {
    u32 t = 0;
    for (u32 i = 0; i < r.cantidad; ++i) t += frames_[r.primer + i].duracion;
    return t;
}

u32 TablaSprites::ms_hasta_impacto(GfxId gfx, u8 acc) const {
    const RangoAccion* r = accion(gfx, acc);
    if (!r) return 0;
    u32 antes = 0;
    for (u32 i = 0; i < r->cantidad; ++i) {
        if (frames_[r->primer + i].impacto) return antes * 960 / r->velocidad;
        antes += frames_[r->primer + i].duracion;
    }
    return ticks(*r) * 960 / r->velocidad / 2;
}

u32 TablaSprites::duracion_ms(GfxId gfx, u8 acc) const {
    const RangoAccion* r = accion(gfx, acc);
    return r ? ticks(*r) * 960 / r->velocidad : 0;
}

const FrameAccion& TablaSprites::frame_en(const RangoAccion& r, u64 transcurrido, bool& termino, u64 ciclo_ms,
                                          bool bucle) const {
    u64 total = ticks(r);
    // Tiempo en ticks (enteros de 1/960 s × velocidad, sin acumular redondeos).
    u64 t = ciclo_ms > 0 ? transcurrido * total / ciclo_ms : transcurrido * r.velocidad / 960;
    termino = t >= total;
    if (termino && !bucle) {
        for (u32 i = r.cantidad; i-- > 0;)  // último frame visible (duración > 0)
            if (frames_[r.primer + i].duracion > 0) return frames_[r.primer + i];
        return frames_[r.primer + r.cantidad - 1];
    }
    t = total ? t % total : 0;
    for (u32 i = 0; i < r.cantidad; ++i) {
        u64 d = frames_[r.primer + i].duracion;  // los frames de duración 0 son instantáneos: no se ven
        if (t < d) return frames_[r.primer + i];
        t -= d;
    }
    return frames_[r.primer];
}

}  // namespace lin

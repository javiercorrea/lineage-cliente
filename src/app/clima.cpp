// Entregable 10: clima. S_Weather manda un número: 1–3 nieve y 17–19 lluvia (de menos a más);
// el resto, despejado (WeatherTimeController de L1J elige al azar entre 0 y 19 cada hora;
// el GM lo cambia con ".weather N").
// Las partículas no tienen estado: la posición de cada una es una función del tiempo y de su
// número (un hash), así no hay nada que crear ni borrar. Solo en los mapas al aire libre.
#include <cmath>
#include "app/app.h"

namespace lin {

namespace {

u32 hash(u32 x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    return x ^ (x >> 16);
}

// Mapas al aire libre (los demás son mazmorras, casas o castillos por dentro). Sin un dato del
// cliente que lo diga, la lista es la del mundo principal y las islas.
bool al_aire_libre(MapId m) { return m == 0 || m == 4 || m == 68 || m == 69 || m == 70 || m == 2005; }

}  // namespace

void dibujar_clima(App& a, int w, int h, u64 ahora) {
    const u8 c = a.mundo.clima;
    const bool nieve = c >= 1 && c <= 3, lluvia = c >= 17 && c <= 19;
    if ((!nieve && !lluvia) || !al_aire_libre(a.mundo.mapa)) return;
    const int intensidad = nieve ? c : c - 16;
    Gpu& gpu = a.g->gpu();
    const u64 k = clave_orden(CAPA_EFECTOS, 3);
    if (lluvia) gpu.rectangulo(0, 0, w, h, k, rgba(10, 16, 30, static_cast<u8>(35 + 20 * intensidad)));  // cielo cubierto
    const int n = (w * h / 4000) * intensidad;
    for (int i = 0; i < n; ++i) {
        const u32 r = hash(static_cast<u32>(i) * 2654435761u + 17);
        const double fase = (r & 0xFFFF) / 65536.0;
        if (lluvia) {
            // Gotas de 14 px que caen en diagonal a ~700 px/s.
            const double vel = 600 + (r >> 24) % 200;
            const double recorrido = h + 40;
            const double t = std::fmod(ahora * vel / 1000.0 + fase * recorrido, recorrido);
            const int x = static_cast<int>((r >> 8) % static_cast<u32>(w + 60)) - static_cast<int>(t * 0.25);
            const int y = static_cast<int>(t) - 20;
            gpu.rectangulo(x, y, 1, 7, k, rgba(190, 210, 240, 170));
            gpu.rectangulo(x - 1, y + 7, 1, 7, k, rgba(190, 210, 240, 140));
        } else {
            // Copos de 2 px que bajan despacio y se mecen de costado.
            const double vel = 30 + (r >> 24) % 40;
            const double recorrido = h + 10;
            const double t = std::fmod(ahora * vel / 1000.0 + fase * recorrido, recorrido);
            const double vaiven = std::sin(ahora / 900.0 + fase * 6.283) * 12;
            const int x = static_cast<int>((r >> 8) % static_cast<u32>(w)) + static_cast<int>(vaiven);
            const int lado = 2 + static_cast<int>((r >> 4) & 1), y = static_cast<int>(t) - 5;
            gpu.rectangulo(x, y, lado, lado, k, rgba(250, 252, 255, 235));
            gpu.rectangulo(x - 1, y + 1, lado + 2, lado - 2, k, rgba(250, 252, 255, 110));  // borde suave
        }
    }
}

}  // namespace lin

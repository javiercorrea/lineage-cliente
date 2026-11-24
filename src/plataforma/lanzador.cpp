#include "plataforma/lanzador.h"
#include <chrono>
#include <cstdio>
#include <thread>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace lin {

namespace {

u64 ahora_ms() {
    using namespace std::chrono;
    return static_cast<u64>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// Corre el comando por la shell del sistema (como una línea de terminal) y llama a `linea` con cada línea de su salida
// hasta que devuelva false o se cierre la salida. Un comando que deja un proceso en segundo plano con la salida heredada
// no la cierra nunca: por eso quien llama puede cortar apenas tenga lo que busca.
template <typename F>
void ejecutar(const std::string& comando, F&& linea) {
    FILE* f = popen(comando.c_str(), "r");
    if (!f) return;
    char buf[256];
    bool seguir = true;
    while (seguir && std::fgets(buf, sizeof buf, f)) seguir = linea(std::string_view(buf));
    pclose(f);
}

// Número de 1 a 65535 que empieza en `i` (saltando espacios, '=' y ':'), o 0.
u16 numero_en(std::string_view s, size_t i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '=' || s[i] == ':' || s[i] == '\t')) ++i;
    unsigned n = 0;
    size_t j = i;
    for (; j < s.size() && s[j] >= '0' && s[j] <= '9' && j - i < 6; ++j) n = n * 10 + static_cast<unsigned>(s[j] - '0');
    const bool entero = j > i && (j == s.size() || s[j] < '0' || s[j] > '9');
    return entero && n >= 1 && n <= 65535 ? static_cast<u16>(n) : 0;
}

}  // namespace

// ':' justo después de una IPv4 ("127.0.0.1:") o de "localhost:"; una hora ("12:30:45") no cuenta.
static bool tras_direccion(std::string_view s, size_t dos_puntos) {
    if (dos_puntos >= 9 && s.substr(dos_puntos - 9, 9) == "localhost") return true;
    size_t i = dos_puntos, puntos = 0;
    while (i > 0 && ((s[i - 1] >= '0' && s[i - 1] <= '9') || s[i - 1] == '.')) puntos += s[--i] == '.';
    return puntos == 3;
}

u16 puerto_marcado(std::string_view s) {
    u16 tras_port = 0, tras_dos_puntos = 0;
    const auto letra = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
    for (size_t i = 0; i < s.size(); ++i) {
        const auto minus = [&](size_t k) { return static_cast<char>(s[k] | 0x20); };
        if (i + 4 <= s.size() && (i == 0 || !letra(s[i - 1])) && minus(i) == 'p' && minus(i + 1) == 'o' && minus(i + 2) == 'r' &&
            minus(i + 3) == 't')
            if (const u16 p = numero_en(s, i + 4)) tras_port = p;
        if (s[i] == ':' && tras_direccion(s, i))
            if (const u16 p = numero_en(s, i + 1)) tras_dos_puntos = p;
    }
    return tras_port ? tras_port : tras_dos_puntos;
}

u16 puerto_en_salida(std::string_view s) {
    if (const u16 p = puerto_marcado(s)) return p;
    u16 puerto = 0;
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] >= '0' && s[i] <= '9' && (i == 0 || s[i - 1] < '0' || s[i - 1] > '9'))
            if (const u16 p = numero_en(s, i)) puerto = p;
    return puerto;
}

void Lanzador::iniciar(const std::string& comando) {
    const std::string anterior = comando_;
    estado_ = std::make_shared<Estado>();
    estado_->inicio_ms = ahora_ms();
    comando_ = comando;
    // Suelto: cerrar el cliente nunca espera a un lanzador lento; el estado compartido sobrevive al hilo.
    std::thread([e = estado_, comando, anterior] {
        if (!anterior.empty()) ejecutar(anterior + " stop", [](std::string_view) { return true; });
        std::string salida;
        ejecutar(comando, [&](std::string_view l) {
            salida += l;
            if (const u16 p = puerto_marcado(l)) {  // ya está: no esperar a que el comando cierre su salida
                e->puerto = p;
                e->listo = true;
                return false;
            }
            return true;
        });
        if (!e->listo) {
            e->puerto = puerto_en_salida(salida);
            e->listo = true;
        }
    }).detach();
}

bool Lanzador::terminado(u16& puerto) {
    if (!estado_) return false;
    if (!estado_->listo.load()) {
        if (ahora_ms() - estado_->inicio_ms < PLAZO_MS) return false;
        puerto = 0;  // venció el plazo: el hilo sigue solo hasta que el comando termine
    } else {
        puerto = estado_->puerto;
    }
    estado_.reset();
    return true;
}

void Lanzador::parar() {
    if (comando_.empty()) return;
    auto hecho = std::make_shared<std::atomic<bool>>(false);
    std::thread([hecho, c = comando_ + " stop"] {
        ejecutar(c, [](std::string_view) { return true; });
        *hecho = true;
    }).detach();
    const u64 fin = ahora_ms() + PLAZO_STOP_MS;
    while (!*hecho && ahora_ms() < fin) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    comando_.clear();
}

}  // namespace lin

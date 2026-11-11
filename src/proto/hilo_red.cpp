#include "proto/hilo_red.h"
#include "proto/comandos.h"

namespace lin {

namespace {
constexpr auto INTERVALO_MANTENER_VIVO = std::chrono::seconds(20);
}

void HiloRed::iniciar(const std::string& host, u16 puerto, const TablaOpcodes& tabla) {
    cerrar();
    host_ = host;
    puerto_ = puerto;
    tabla_ = tabla;
    error_.clear();
    // Lo que quedó de la conexión anterior (un paso, un keepalive encolados al caerse) no debe salir
    // antes de la versión y el login de esta. Los hilos ya terminaron: nadie más toca las colas.
    entrada_.vaciar();
    salida_.vaciar();
    en_juego_ = false;
    parar_ = false;
    estado_ = EstadoRed::CONECTANDO;
    lector_ = std::thread(&HiloRed::leer, this);
}

void HiloRed::cerrar() {
    parar_ = true;
    socket_.cerrar();  // desbloquea recv (connect lo corta parar_)
    {
        std::lock_guard<std::mutex> lk(mtx_);
    }
    cv_.notify_all();
    if (lector_.joinable()) lector_.join();
    if (escritor_.joinable()) escritor_.join();
    socket_.cerrar();  // el que haya conectado el lector mientras se cerraba
    if (estado_ != EstadoRed::INACTIVA) estado_ = EstadoRed::CERRADA;
}

void HiloRed::enviar(Bytes cuerpo) {
    salida_.empujar(cuerpo);
    {
        std::lock_guard<std::mutex> lk(mtx_);  // con el lock: el escritor no se pierde el aviso entre mirar y esperar
    }
    cv_.notify_one();
}

void HiloRed::leer() {
    if (!socket_.conectar(host_, puerto_, 5000, &parar_)) {
        if (!parar_) {
            error_ = "No se pudo conectar a " + host_ + ":" + std::to_string(puerto_);
            estado_ = EstadoRed::ERROR;
        }
        return;
    }
    std::vector<u8> buf(65536);
    bool primero = true;
    while (!parar_) {
        u8 cab[2];
        if (!socket_.leer_exacto(cab, 2)) break;
        size_t n = static_cast<size_t>(cab[0] | (cab[1] << 8));
        if (n < 2) break;
        n -= 2;
        if (!socket_.leer_exacto(buf.data(), n)) break;
        if (primero) {
            // INITPACKET: [opcode][u32 semilla][...] sin cifrar
            if (n < 5 || buf[0] != tabla_.cable_init) {
                error_ = "El servidor no envió el paquete inicial esperado. Recibido " + std::to_string(n ? buf[0] : 0) +
                         " vs Esperado " + std::to_string(tabla_.cable_init);
                estado_ = EstadoRed::ERROR;
                return;  // el socket lo cierra cerrar(), en el hilo principal
            }
            u32 semilla = static_cast<u32>(buf[1]) | (static_cast<u32>(buf[2]) << 8) | (static_cast<u32>(buf[3]) << 16) |
                          (static_cast<u32>(buf[4]) << 24);
            rx_ = std::make_unique<Cifrado>(semilla);
            tx_ = std::make_unique<Cifrado>(semilla);
            primero = false;
            escritor_ = std::thread(&HiloRed::escribir, this);
            estado_ = EstadoRed::CONECTADA;
            continue;
        }
        if (n == 0) continue;  // sin opcode: nada que decodificar
        rx_->descifrar(buf.data(), n);
        // Cola llena (el hilo principal no saca: ventana oculta, depurador): esperar sin gastar CPU.
        while (!entrada_.empujar({buf.data(), n}) && !parar_) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (!parar_) {
        if (error_.empty()) error_ = "El servidor cerró la conexión";
        estado_ = EstadoRed::CERRADA;
    }
    {
        std::lock_guard<std::mutex> lk(mtx_);
    }
    cv_.notify_all();
}

void HiloRed::escribir() {
    std::vector<u8> cuerpo, trama;
    auto mandar = [&] {
        tx_->cifrar(cuerpo.data(), cuerpo.size());
        trama.resize(cuerpo.size() + 2);
        trama[0] = static_cast<u8>(trama.size());
        trama[1] = static_cast<u8>(trama.size() >> 8);
        std::memcpy(trama.data() + 2, cuerpo.data(), cuerpo.size());
        return socket_.escribir(trama.data(), trama.size());
    };
    // C_KeepAlive desde aquí y no desde el frame: si el bucle principal se detiene (ventana minimizada
    // u oculta en macOS), el servidor igual lo recibe y no corta por inactividad.
    auto ultimo_vivo = std::chrono::steady_clock::now();
    while (!parar_) {
        const auto ahora = std::chrono::steady_clock::now();
        if (!en_juego_) ultimo_vivo = ahora;
        if (ahora - ultimo_vivo >= INTERVALO_MANTENER_VIVO) {
            Escritor vivo = cmd::mantener_vivo(tabla_);
            const Bytes b = vivo.cuerpo();
            cuerpo.assign(b.begin(), b.end());
            if (!mandar()) break;
            ultimo_vivo = ahora;
        }
        if (salida_.sacar(cuerpo)) {
            if (!mandar()) break;
            continue;
        }
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait_for(lk, std::chrono::seconds(1), [&] { return parar_.load() || !salida_.vacio(); });
    }
}

}  // namespace lin

// Mascotas e invocaciones.
//  - S_PetCtrlMenu agrega o quita una de la lista: a la izquierda de la barra de atajos, con su
//    vida (S_HpMeter).
//    Clic en el nombre le habla (el servidor responde con su menú "anicom": atacar, defender,
//    quedarse, juntar ítems, liberarla...) y "Bag" pide su inventario (C_PetMenu -> S_PetInventory,
//    que se muestra en la ventana del NPC: un clic usa o equipa el ítem con C_UsePetItem).
//  - "Attack" del menú hace que el servidor mande S_SelectTarget: el próximo clic sobre un
//    personaje o monstruo es su objetivo (C_SelectTarget); clic derecho o Esc cancelan.
//  - S_TrueTarget (habilidad de elfo): una marca sobre el objetivo durante 16 s.
#include "app/app.h"

namespace lin {

namespace {

constexpr int FILA = ALTO_GLIFO + 8, ANCHO_LISTA = 122, BOTON = 32;
constexpr u64 MS_MARCA = 16000;

// A la izquierda de la barra de atajos, con la última fila a su altura (debajo de las ventanas).
int y_lista(const App& a) { return a.barra_y + 36 - static_cast<int>(a.mascotas.size()) * FILA; }

}  // namespace

void eventos_mascotas(App& a, const Evento& e) {
    switch (e.tipo) {
    case Ev::MASCOTA: {
        auto it = std::find_if(a.mascotas.begin(), a.mascotas.end(), [&](const Mascota& m) { return m.id == e.id; });
        if (!e.v[MA_AGREGAR]) {
            if (it != a.mascotas.end()) a.mascotas.erase(it);
        } else if (it == a.mascotas.end()) {
            a.mascotas.push_back({e.id, std::string(e.s[0])});
        }
        break;
    }
    case Ev::ELEGIR_OBJETIVO_MASCOTA: a.mascota_eligiendo = e.id; break;
    case Ev::MARCA_OBJETIVO:
        a.marca_objetivo = e.id;
        a.marca_texto.assign(e.s[0]);
        a.fin_marca = a.entrada.ahora_ms + MS_MARCA;
        break;
    default: break;
    }
}

bool sobre_mascotas(const App& a) {
    return !a.mascotas.empty() && ui::sobre(a.entrada, 4, y_lista(a), ANCHO_LISTA + 4, static_cast<int>(a.mascotas.size()) * FILA);
}

void procesar_mascotas(App& a) {
    if (!a.mascota_eligiendo) return;
    EntradaFrame& e = a.entrada;
    if (a.mundo.objetos.buscar(a.mascota_eligiendo) == SIN_SLOT || e.clic_derecho || e.escape) {
        a.mascota_eligiendo = 0;
        return;
    }
    if (e.pulsado && !sobre_alguna_ventana(a) && !sobre_barra(a) && !sobre_mascotas(a)) {
        a.clic_consumido = true;  // el clic no hace atacar ni caminar al propio personaje
        const ObjId obj = objeto_bajo_puntero(a);
        if (obj == a.mascota_eligiendo) return;  // la misma mascota: se sigue eligiendo
        if (obj) enviar(a, cmd::objetivo_mascota(a.op, a.mascota_eligiendo, obj));
        a.mascota_eligiendo = 0;  // un clic en el suelo cancela
    }
}

void dibujar_mascotas(App& a) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const u64 kf = clave_orden(CAPA_UI, 4), kt = clave_orden(CAPA_UI_TEXTO, 4);
    const TablaObjetos& o = a.mundo.objetos;
    int y = y_lista(a);
    for (size_t i = 0; i < a.mascotas.size(); ++i, y += FILA) {
        const Mascota& m = a.mascotas[i];
        const Slot s = o.buscar(m.id);
        const int x = 4, w_nombre = ANCHO_LISTA - BOTON - 4;
        const bool encima = ui::sobre(e, x, y, w_nombre, FILA - 2);
        g.gpu().rectangulo(x, y, w_nombre, FILA - 2, kf, encima ? rgba(60, 50, 35, 220) : rgba(20, 18, 15, 190));
        // Vida, si el servidor la mandó (S_HpMeter: % o 0xFF desconocida).
        const u8 hp = s != SIN_SLOT ? o.col<O_HP>()[s] : 0xFF;
        if (hp <= 100) g.gpu().rectangulo(x, y + FILA - 4, w_nombre * hp / 100, 2, kf + (1ull << 24), rgba(220, 50, 40));
        // El nombre del objeto en el mundo ("$N", traducido); S_PetCtrlMenu lo trae en el idioma del servidor.
        char nombre[128];
        const size_t n = expandir_desc(a.desc, s != SIN_SLOT ? texto(a.mundo, o.col<O_NOMBRE>()[s]) : std::string_view(m.nombre),
                                       nombre, sizeof nombre);
        g.texto({nombre, n}, static_cast<float>(x + 5), static_cast<float>(y + 3), kt,
                s != SIN_SLOT ? ui::COLOR_TEXTO : rgba(130, 120, 100));  // gris: fuera de la vista
        if (encima && e.clic) enviar(a, cmd::hablar_npc(a.op, m.id));
        if (ui::boton_texto(g, e, "Bag", x + w_nombre + 4, y, BOTON, FILA - 2)) enviar(a, cmd::inventario_mascota(a.op, m.id));
    }
    if (a.mascota_eligiendo)
        g.texto_sombra("Choose the pet's target", static_cast<float>(e.mx + 14), static_cast<float>(e.my + 4),
                       clave_orden(CAPA_ARRASTRE, 1), rgba(255, 230, 120));
    if (a.marca_objetivo && e.ahora_ms >= a.fin_marca) a.marca_objetivo = 0;
}

}  // namespace lin

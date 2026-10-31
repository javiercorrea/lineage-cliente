// Cliente Lineage multiplataforma (entregables 1–4: login, personajes, moverse, chat y estado).
#include "app/app.h"

int main() {
    lin::App app;
    if (!lin::iniciar_app(app)) {
        lin::cerrar_app(app);
        return 1;
    }
    lin::bucle_app(app);
    lin::cerrar_app(app);
    return 0;
}

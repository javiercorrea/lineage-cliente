// Cliente Lineage multiplataforma (entregables 1–4: login, personajes, moverse, chat y estado).
#include "app/app.h"

int main(int argc, char** argv) {
    lin::App app;
    if (!lin::iniciar_app(app, argc, argv)) {
        lin::cerrar_app(app);
        return 1;
    }
    lin::bucle_app(app);
    lin::cerrar_app(app);
    return 0;
}

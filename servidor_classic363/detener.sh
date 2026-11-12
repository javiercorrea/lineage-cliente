#!/bin/bash
# Detiene el servidor l1j-en/classic rama 363 y su MariaDB (puerto 3310).
# Filtra por el marcador -Dl1j.instancia=363 para NO matar al s3ep1 (servidor_classic) si corre.
cd "$(dirname "$0")"
pkill -f "l1j.instancia=363" || true
"$(brew --prefix mariadb)/bin/mariadb-admin" --socket="$PWD/datos/my.sock" -uroot shutdown 2>/dev/null || true

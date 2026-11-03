#!/bin/bash
# Detiene el servidor l1j-en/classic y su MariaDB.
cd "$(dirname "$0")"
pkill -f "l1jen.jar" || true
"$(brew --prefix mariadb)/bin/mariadb-admin" --socket="$PWD/datos/my.sock" -uroot shutdown 2>/dev/null || true

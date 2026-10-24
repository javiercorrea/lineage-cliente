#!/bin/bash
# Arranca MariaDB con el directorio de datos local (puerto 3307), si no está corriendo.
set -euo pipefail
cd "$(dirname "$0")"
AQUI="$(pwd)"
MARIA="$(brew --prefix mariadb)/bin"
if "$MARIA/mariadb-admin" --socket="$AQUI/datos/mysql.sock" -uroot ping >/dev/null 2>&1; then
    exit 0
fi
"$MARIA/mariadbd-safe" --datadir="$AQUI/datos" --port=3307 --bind-address=127.0.0.1 \
    --socket="$AQUI/datos/mysql.sock" --pid-file="$AQUI/datos/mariadb.pid" >/dev/null 2>&1 &
for _ in $(seq 1 30); do
    "$MARIA/mariadb-admin" --socket="$AQUI/datos/mysql.sock" -uroot ping >/dev/null 2>&1 && exit 0
    sleep 1
done
echo "MariaDB no arrancó; ver $AQUI/datos/*.err"; exit 1

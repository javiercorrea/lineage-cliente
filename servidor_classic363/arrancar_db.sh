#!/bin/bash
# Arranca la MariaDB de servidor_classic363/datos (puerto 3310) si no está corriendo.
set -euo pipefail
cd "$(dirname "$0")"
MARIA="$(brew --prefix mariadb)/bin"
"$MARIA/mariadb-admin" --socket="$PWD/datos/my.sock" -uroot ping >/dev/null 2>&1 && exit 0
"$MARIA/mariadbd-safe" --datadir="$PWD/datos" --port=3310 --bind-address=127.0.0.1 \
    --socket="$PWD/datos/my.sock" --pid-file="$PWD/datos/m.pid" >/dev/null 2>&1 &
for i in $(seq 1 30); do
    "$MARIA/mariadb-admin" --socket="$PWD/datos/my.sock" -uroot ping >/dev/null 2>&1 && exit 0
    sleep 1
done
echo "la base no arrancó" >&2; exit 1

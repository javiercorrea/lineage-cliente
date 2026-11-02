#!/bin/bash
# Instala un servidor 3.80C local para probar el cliente: L1JGO-Whale (Go + PostgreSQL), macOS con
# Homebrew.
#
# - Baja el servidor (r0ptik/L1JGO-Whale, commit fijo) en servidor_380/l1jgo y lo compila
# - PostgreSQL con su propio directorio de datos (servidor_380/datos), en el puerto 5433 para no
#   chocar con otra instalación; base l1jgo, usuario postgres sin clave (solo local)
# - La configuración se genera en servidor_380/l1jgo/local.toml (la del repo no se toca)
#
# MULTIPLATAFORMA: en Linux cambiar brew por el gestor de paquetes (golang, postgresql).
set -euo pipefail
cd "$(dirname "$0")"
REPO=https://github.com/r0ptik/L1JGO-Whale.git
COMMIT=9a19405

for p in go postgresql@16; do
    brew list --versions "$p" >/dev/null || brew install "$p"
done

if [ ! -d l1jgo ]; then
    git clone -q "$REPO" l1jgo
    git -C l1jgo checkout -q "$COMMIT"
fi
(cd l1jgo && "$(brew --prefix go)/bin/go" build -o bin/l1jgo ./cmd/l1jgo)

PG="$(brew --prefix postgresql@16)/bin"
if [ ! -d datos ]; then
    "$PG/initdb" -D datos -U postgres --auth=trust -E UTF8 --locale=C >/dev/null
fi
./arrancar_db.sh
"$PG/psql" -h 127.0.0.1 -p 5433 -U postgres -tc "SELECT 1 FROM pg_database WHERE datname = 'l1jgo'" | grep -q 1 ||
    "$PG/createdb" -h 127.0.0.1 -p 5433 -U postgres l1jgo

# Configuración local: la base de arriba (las migraciones las corre el servidor al arrancar), sin
# el "#npc:gfx" de depuración en los nombres de los NPC y con el registro en "info".
sed -e 's#^dsn = .*#dsn = "postgres://postgres@127.0.0.1:5433/l1jgo?sslmode=disable"#' \
    -e 's#^show_npc_id = true#show_npc_id = false#' \
    -e 's#^level = "debug"#level = "info"#' \
    l1jgo/config/server.toml > l1jgo/local.toml
echo "Listo: ./arrancar.sh (puerto 7001)"

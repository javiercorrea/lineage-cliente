#!/bin/bash
# Arranca el PostgreSQL de servidor_380/datos (puerto 5433) si no está corriendo.
set -euo pipefail
cd "$(dirname "$0")"
PG="$(brew --prefix postgresql@16)/bin"
"$PG/pg_ctl" -D datos status >/dev/null 2>&1 && exit 0
"$PG/pg_ctl" -D datos -o "-p 5433 -k /tmp -c listen_addresses=127.0.0.1" -l datos/postgres.log -w start >/dev/null

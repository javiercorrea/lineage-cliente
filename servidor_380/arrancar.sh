#!/bin/bash
# Arranca PostgreSQL y el servidor L1JGO-Whale (3.80C) en el puerto 7001. Ctrl+C lo detiene.
set -euo pipefail
cd "$(dirname "$0")"
./arrancar_db.sh
cd l1jgo
L1JGO_CONFIG=local.toml exec ./bin/l1jgo

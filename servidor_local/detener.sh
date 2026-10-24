#!/bin/bash
# Detiene MariaDB local (el servidor L1J se detiene con Ctrl+C en su terminal).
cd "$(dirname "$0")"
"$(brew --prefix mariadb)/bin/mariadb-admin" --socket="$(pwd)/datos/mysql.sock" -uroot shutdown

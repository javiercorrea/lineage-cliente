#!/bin/bash
# Detiene el servidor L1JGO-Whale y su PostgreSQL.
cd "$(dirname "$0")"
pkill -f "bin/l1jgo" || true
"$(brew --prefix postgresql@16)/bin/pg_ctl" -D datos -m fast stop >/dev/null 2>&1 || true

#!/bin/bash
# Arranca MariaDB (3310) y el servidor l1j-en/classic rama 363 en el puerto 2001. Ctrl+C sale.
# Instancia aparte del s3ep1 (servidor_classic, puerto 2000): se pueden correr los dos a la vez.
set -euo pipefail
cd "$(dirname "$0")"
./arrancar_db.sh
cd l1jen
# -Dl1j.instancia=363: marcador inocuo para que detener.sh mate solo esta instancia (no el s3ep1).
exec "$(brew --prefix openjdk)/bin/java" -Dl1j.instancia=363 -Djava.util.logging.config.file=config/log.properties \
    -Xms1024m -Xmx1024m -cp "l1jen.jar:lib/*" l1j.server.Server

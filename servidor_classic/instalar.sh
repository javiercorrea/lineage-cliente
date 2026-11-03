#!/bin/bash
# Instala un servidor l1j-en/classic local (3.0 English / US 2009, protocolo s3ep1) para probar el
# cliente. macOS con Homebrew. Como los otros servidores de prueba: solo local, base propia.
#
# - Baja el servidor (l1j-en/classic, commit fijo) en servidor_classic/l1jen y lo compila con ant
#   (produce l1jen.jar). Java derivado de l1j-jp; compila con el openjdk de Homebrew.
# - MariaDB con su propio directorio de datos (servidor_classic/datos) en el puerto 3309 (para no
#   chocar con el 3307 del 3.63 ni el 3308 que pudiera usar otra prueba); base l1jdb, usuario l1j/l1j.
# - La configuración se deja en l1jen/config/server.properties apuntando a esa base.
#
# MULTIPLATAFORMA: en Linux cambiar brew por el gestor de paquetes (openjdk, ant, mariadb-server).
set -euo pipefail
cd "$(dirname "$0")"
REPO=https://github.com/l1j-en/classic.git
COMMIT=103e6f6

for p in openjdk ant mariadb; do
    brew list --versions "$p" >/dev/null || brew install "$p"
done
JAVA="$(brew --prefix openjdk)/bin"
MARIA="$(brew --prefix mariadb)/bin"

if [ ! -d l1jen ]; then
    git clone "$REPO" l1jen
    git -C l1jen checkout -q "$COMMIT"
fi
# Compilar (l1jen.jar). El código es viejo pero compila con el openjdk actual (solo avisos).
(cd l1jen && PATH="$JAVA:$PATH" ant >/dev/null)

# MariaDB en un directorio de datos propio, puerto 3309.
if [ ! -d datos ]; then
    "$MARIA/mariadb-install-db" --datadir="$PWD/datos" --auth-root-authentication-method=normal >/dev/null
fi
./arrancar_db.sh
"$MARIA/mariadb" --socket="$PWD/datos/my.sock" -uroot <<'SQL'
CREATE DATABASE IF NOT EXISTS l1jdb DEFAULT CHARACTER SET utf8;
CREATE USER IF NOT EXISTS 'l1j'@'localhost' IDENTIFIED BY 'l1j';
CREATE USER IF NOT EXISTS 'l1j'@'127.0.0.1' IDENTIFIED BY 'l1j';
GRANT ALL ON l1jdb.* TO 'l1j'@'localhost';
GRANT ALL ON l1jdb.* TO 'l1j'@'127.0.0.1';
SQL
if [ "$("$MARIA/mariadb" --socket="$PWD/datos/my.sock" -uroot -N -e 'SELECT COUNT(*) FROM information_schema.tables WHERE table_schema="l1jdb"')" = "0" ]; then
    echo "Importando la base de datos..."
    "$MARIA/mariadb" --socket="$PWD/datos/my.sock" -uroot l1jdb < l1jen/db/l1jdb_m10.sql
    for f in update_085 update_086 pledge_name_fix; do
        "$MARIA/mariadb" --socket="$PWD/datos/my.sock" -uroot l1jdb < "l1jen/db/$f.sql"
    done
fi

# Config: la base de arriba (puerto 3309), usuario l1j. El conector 5.1.31 que trae anda con MariaDB.
sed -i.orig \
    -e 's#^URL=.*#URL=jdbc:mysql://127.0.0.1:3309/l1jdb?autoReconnect=true\&useUnicode=True\&characterEncoding=UTF-8#' \
    -e 's#^Login=.*#Login=l1j#' -e 's#^Password=.*#Password=l1j#' \
    l1jen/config/server.properties
echo "Listo: ./arrancar.sh (puerto 2000, protocolo s3ep1)"

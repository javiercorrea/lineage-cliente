#!/bin/bash
# Instala un servidor l1j-en/classic local en su RAMA 363 (protocolo 3.63, compatible con el perfil
# jp363 del cliente) para probar esa versión. Es una instancia APARTE de servidor_classic (s3ep1):
# clon, base y puertos propios, así no pisa al server s3ep1 que ya está instalado y probado.
#
# - Baja l1j-en/classic en la rama 363 (commit fijo) en servidor_classic363/l1jen y compila con ant.
# - MariaDB con su propio directorio de datos (servidor_classic363/datos) en el puerto 3310 (3309 es
#   del s3ep1); base l1jdb, usuario l1j/l1j.
# - Server de juego en el puerto 2001 (2000 es del s3ep1), SSH 16484 (16483 es del s3ep1).
#
# MULTIPLATAFORMA: en Linux cambiar brew por el gestor de paquetes (openjdk, ant, mariadb-server).
set -euo pipefail
cd "$(dirname "$0")"
REPO=https://github.com/l1j-en/classic.git
COMMIT=7571f606  # rama 363 (Merge branch 'master' into 363, 2020-07-05)

for p in openjdk ant mariadb; do
    brew list --versions "$p" >/dev/null || brew install "$p"
done
JAVA="$(brew --prefix openjdk)/bin"
MARIA="$(brew --prefix mariadb)/bin"

if [ ! -d l1jen ]; then
    git clone "$REPO" l1jen
    git -C l1jen fetch -q origin 363
    git -C l1jen checkout -q "$COMMIT"
fi
# Parche: un personaje transformado que muere nunca recibía el S_DoActionGFX de muerte (Death.run se
# reprogramaba cada 100 ms para siempre). Se aplica una sola vez (--check falla si ya está).
git -C l1jen apply --check ../muerte_polymorph.patch 2>/dev/null && git -C l1jen apply ../muerte_polymorph.patch
# Compilar (l1jen.jar). Viejo pero compila con el openjdk actual (solo avisos).
(cd l1jen && PATH="$JAVA:$PATH" ant >/dev/null)

# MariaDB en un directorio de datos propio, puerto 3310.
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
    echo "Importando la base de datos (rama 363)..."
    "$MARIA/mariadb" --socket="$PWD/datos/my.sock" -uroot l1jdb < l1jen/db/l1jdb_m10.sql
    # La rama 363 trae update_085 y pledge_name_fix (no update_086, que es de master).
    for f in update_085 pledge_name_fix; do
        "$MARIA/mariadb" --socket="$PWD/datos/my.sock" -uroot l1jdb < "l1jen/db/$f.sql"
    done
fi

# Config: base en 3310, usuario l1j, puerto de juego 2001, SSH 16484 (todo propio para no chocar).
sed -i.orig \
    -e 's#^URL=.*#URL=jdbc:mysql://127.0.0.1:3310/l1jdb?autoReconnect=true\&useUnicode=True\&characterEncoding=UTF-8#' \
    -e 's#^Login=.*#Login=l1j#' -e 's#^Password=.*#Password=l1j#' \
    -e 's#^GameserverPort=.*#GameserverPort=2001#' \
    -e 's#^SSHPort *=.*#SSHPort=16484#' -e 's#^TelnetServerPort *=.*#TelnetServerPort=2301#' \
    l1jen/config/server.properties
echo "Listo: ./arrancar.sh (puerto 2000->2001, protocolo jp363). No afecta a servidor_classic (s3ep1)."

#!/bin/bash
# Arranca MariaDB local y el servidor L1J JP 3.63c en el puerto 2000.
# - Idioma inglés: el recurso base (locale/lang.xml) está incompleto y con un locale español
#   el servidor no arranca.
# - Classpath explícito para sumar JAXB (lib_extra/) a las librerías del servidor.
# - TZ=Asia/Tokyo: el servidor usa el id "JST", obsoleto en Java moderno.
# - Fechas con formato japonés (user.language.format=ja): HouseTable y otros guardan fechas con
#   DateFormat del idioma por defecto; en inglés ("Nov 10, 2026, 10:00:00 PM") MariaDB las
#   rechaza y no se guardan las casas (dueño, nombre, impuestos). "2026/11/10 22:00:00" sí.
set -euo pipefail
cd "$(dirname "$0")"
./arrancar_db.sh
cd l1j-jp
mkdir -p emblem  # C_Emblem guarda ahí los emblemas de los clanes (no viene en la distribución)
exec "$(brew --prefix openjdk)/bin/java" -Duser.language=en -Duser.country=US -Duser.language.format=ja -Duser.country.format=JP -Duser.timezone=Asia/Tokyo \
    -Xms512m -Xmx1024m -cp "l1jserver.jar:lib/*:lib_extra/*" jp.l1j.Server

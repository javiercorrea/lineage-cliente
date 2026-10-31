"""El servidor local para las pruebas que lo modifican (casas, castillos, guerras).

Solo servidor_local (127.0.0.1:2000, MariaDB en servidor_local/datos). Las fechas de los asedios y
la vida de torres y portones se leen al arrancar, y los clanes viven en memoria: por eso estas
pruebas detienen el servidor, cambian la base y lo vuelven a arrancar, y al final restauran todo de
la misma manera.
"""
import os
import socket
import subprocess
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
SERVIDOR = os.path.normpath(os.path.join(AQUI, "..", "..", "servidor_local"))
SOCKET_DB = os.path.join(SERVIDOR, "datos", "mysql.sock")
BASE = "l1jdb_363"


def _maria(programa):
    prefijo = subprocess.run(["brew", "--prefix", "mariadb"], capture_output=True, text=True, check=True).stdout.strip()
    return os.path.join(prefijo, "bin", programa)


def sql(texto):
    """Ejecuta SQL en la base del servidor local y devuelve la salida (tabulada, sin encabezados)."""
    r = subprocess.run([_maria("mariadb"), "--socket=" + SOCKET_DB, "-uroot", "-N", BASE], input=texto,
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("SQL: " + r.stderr.strip())
    return r.stdout


def sql_archivo(nombre):
    """Un script de servidor_local/pruebas/."""
    with open(os.path.join(SERVIDOR, "pruebas", nombre), encoding="utf-8") as f:
        return sql(f.read())


def respaldar(personajes):
    """Las filas de esos personajes, su inventario y los clanes, como SQL para restaurar."""
    ids = sql("SELECT GROUP_CONCAT(id) FROM characters WHERE name IN (%s)" %
              ",".join("'%s'" % p for p in personajes)).strip()
    volcado = [_maria("mariadb-dump"), "--socket=" + SOCKET_DB, "-uroot", "--no-create-info", "--replace", BASE]
    partes = [subprocess.run(volcado + [tabla, "--where=" + donde], capture_output=True, text=True, check=True).stdout
              for tabla, donde in (("characters", "id IN (%s)" % ids), ("inventory_items", "owner_id IN (%s)" % ids))]
    partes.append(subprocess.run(volcado + ["clans"], capture_output=True, text=True, check=True).stdout)
    # Los clanes que aparezcan después (el rival de la guerra) se borran; los de antes se reponen.
    nombres = sql("SELECT GROUP_CONCAT(QUOTE(name)) FROM clans").strip() or "''"
    # Ítems nuevos de esos personajes (adena de .adena, por ejemplo): se borran antes de reponer.
    return ("DELETE FROM inventory_items WHERE owner_id IN (%s);\nDELETE FROM clans WHERE name NOT IN (%s);\n" % (ids, nombres)
            + "".join(partes))


def en_marcha():
    with socket.socket() as s:
        s.settimeout(1)
        return s.connect_ex(("127.0.0.1", 2000)) == 0


def detener():
    """Detiene el servidor L1J (no la base) y espera a que suelte el puerto."""
    subprocess.run(["pkill", "-f", "jp.l1j.Server"])
    for _ in range(60):
        if not en_marcha():
            time.sleep(2)  # que termine de guardar
            return
        time.sleep(1)
    raise RuntimeError("el servidor no se detuvo")


def arrancar():
    """Arranca el servidor en segundo plano y espera a que acepte conexiones y cargue el mundo."""
    with open(os.path.join(SERVIDOR, "servidor.log"), "w") as log:
        subprocess.Popen(["./arrancar.sh"], cwd=SERVIDOR, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    for _ in range(180):
        if en_marcha():
            time.sleep(15)  # el puerto abre antes de que termine de cargar NPCs y mapas
            return
        time.sleep(1)
    raise RuntimeError("el servidor no arrancó (ver servidor_local/servidor.log)")


def reiniciar_con(*scripts):
    """Detener, aplicar esos scripts de servidor_local/pruebas/ y arrancar."""
    detener()
    for s in scripts:
        sql_archivo(s)
    arrancar()

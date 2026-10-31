-- Solo para pruebas del cliente: el asedio de Kent (castillo 1) empieza ahora y dura WarTime (2 h,
-- config/altsettings.properties.xml). El servidor corre con hora de Tokio (arrancar.sh).
UPDATE castles SET war_time = CONVERT_TZ(UTC_TIMESTAMP(), '+00:00', '+09:00') WHERE id = 1;
-- Torre de guardia (81111) y portón principal de Kent (puerta 2001) con poca vida, para no tardar
-- 15 minutos a golpes. Se leen al arrancar el servidor.
UPDATE npcs SET hp = 300 WHERE id = 81111;
UPDATE spawn_doors SET hp = 300 WHERE id = 2001;

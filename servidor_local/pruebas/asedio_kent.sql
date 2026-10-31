-- Solo para pruebas del cliente: el asedio de Kent (castillo 1) empieza ahora y dura WarTime (2 h,
-- config/altsettings.properties.xml). El servidor corre con hora de Tokio (arrancar.sh).
UPDATE castles SET war_time = CONVERT_TZ(UTC_TIMESTAMP(), '+00:00', '+09:00') WHERE id = 1;

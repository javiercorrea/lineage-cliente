-- Correcciones a la base l1jdb_363 del repositorio L1J-JP 3.63c para que el servidor arranque.
-- 15 NPCs (guardianes de espíritus) tienen el nombre coreano en la columna impl en lugar de la clase.
UPDATE npcs SET impl = 'L1Monster' WHERE impl NOT LIKE 'L1%';
-- NpcTable lee str/con/dex/wis/int con getByte(): valores mayores a 127 rompen la carga.
UPDATE npcs SET str = 127 WHERE str > 127;
UPDATE npcs SET con = 127 WHERE con > 127;
UPDATE npcs SET dex = 127 WHERE dex > 127;
UPDATE npcs SET wis = 127 WHERE wis > 127;
UPDATE npcs SET `int` = 127 WHERE `int` > 127;
-- Varios campos de texto terminan con una barra invertida sobrante (p. ej. armor_sets: "20009\").
-- Se quita de todas las columnas de texto de la base.
DELIMITER //
CREATE OR REPLACE PROCEDURE quitar_barras()
BEGIN
  DECLARE fin INT DEFAULT 0;
  DECLARE t, c VARCHAR(128);
  DECLARE cur CURSOR FOR SELECT table_name, column_name FROM information_schema.columns
    WHERE table_schema = DATABASE() AND data_type IN ('varchar', 'char', 'text', 'mediumtext', 'longtext');
  DECLARE CONTINUE HANDLER FOR NOT FOUND SET fin = 1;
  OPEN cur;
  bucle: LOOP
    FETCH cur INTO t, c;
    IF fin THEN LEAVE bucle; END IF;
    SET @q = CONCAT('UPDATE `', t, '` SET `', c, '` = LEFT(`', c, '`, CHAR_LENGTH(`', c, '`) - 1) WHERE RIGHT(`', c, '`, 1) = CHAR(92)');
    PREPARE s FROM @q; EXECUTE s; DEALLOCATE PREPARE s;
  END LOOP;
  CLOSE cur;
END//
DELIMITER ;
CALL quitar_barras();
DROP PROCEDURE quitar_barras;

-- Esta instalación del cliente no tiene el mapa 8011 (Singing Island) ni sus mazmorras 85 y 87:
-- al reiniciar ahí se vuelve a Silver Knight Town (el teletransporte a la isla está desactivado en
-- data/xml/NpcActions/Teleporter.xml).
UPDATE return_locations SET getback_x1=33083, getback_y1=33387, getback_x2=33083, getback_y2=33387,
    getback_x3=33083, getback_y3=33387, getback_map_id=4 WHERE area_map_id IN (85, 87, 8011);

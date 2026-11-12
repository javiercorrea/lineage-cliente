-- Deja casas, castillos y clanes como estaban antes de las pruebas de castillos y casas.
-- Con el servidor detenido: tiene los clanes en memoria y los vuelve a escribir.
SET NAMES utf8mb4;
UPDATE clans SET house_id = 0, castle_id = 0;
-- El clan rival de la prueba de guerras (Prueba lo fundó).
DELETE FROM clans WHERE name = 'Rivales';
INSERT INTO auction_houses (house_id, note, deadline, price, owner_id, bidder_id)
    VALUES (262145, '기란아지트 1', '2026-11-02 00:00:00', 100000, 0, 0)
    ON DUPLICATE KEY UPDATE note = VALUES(note), deadline = VALUES(deadline), price = 100000, owner_id = 0, bidder_id = 0;
UPDATE houses SET name = '기란아지트 1', is_on_sale = 1, is_purchase_basement = 0, tax_deadline = '0000-00-00 00:00:00' WHERE id = 262145;
UPDATE castles SET war_time = '0000-00-00 00:00:00', tax_rate = 10, public_money = 48 WHERE id = 1;
UPDATE npcs SET hp = 5000 WHERE id = 81111;
UPDATE spawn_doors SET hp = 10000 WHERE id = 2001;

-- Solo para pruebas del cliente: cierra ya la subasta de la casa 1 de Giran (262145).
-- AuctionTimeController la revisa cada minuto y se la da al clan del postor.
UPDATE auction_houses SET deadline = '2000-01-01 00:00:00' WHERE house_id = 262145;

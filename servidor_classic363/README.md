# servidor_classic363 — l1j-en/classic en la rama 363 (protocolo 3.63)

Instancia **aparte** del `servidor_classic` (s3ep1). Sirve el mismo l1j-en/classic pero en su rama
**363**, que habla el protocolo **3.63** — el mismo que el perfil **`jp363`** del cliente (init 161,
cifrado XOR de la familia l1j; opcodes verificados: coinciden con `jp363`).

No pisa al s3ep1: clon, base y puertos propios.

| | s3ep1 (`servidor_classic`) | 363 (`servidor_classic363`) |
|---|---|---|
| protocolo del cliente | `s3ep1` | `jp363` |
| puerto de juego | 2000 | **2001** |
| MariaDB | puerto 3309, `datos/` | **puerto 3310, `datos/`** |
| rama/commit | master `103e6f6` | 363 `7571f606` |

Se pueden correr **los dos a la vez** (puertos distintos).

## Instalar y arrancar

```sh
servidor_classic363/instalar.sh     # una vez: clona la rama 363, compila (ant), crea e importa la base
servidor_classic363/arrancar.sh     # deja el server en primer plano (puerto 2001); Ctrl+C sale
servidor_classic363/detener.sh      # para el server (solo esta instancia) y su MariaDB
```

## Conectar el cliente

Agregar un servidor con protocolo `jp363` apuntando al 2001. En `settings.ini` del usuario (o desde la
pantalla de login del cliente) agregar la línea:

```
servidor=Classic 363|127.0.0.1|2001|jp363
```

Para pruebas sin tocar el `settings.ini` del usuario, usar `LIN_CONFIG=/ruta/propia.ini` con esa línea
(ver AGENTS.md §5). La cuenta se crea sola en el primer login (usuario = clave).

## Estado

- **Compatibilidad de protocolo verificada por código** (handshake, cifrado y opcodes coinciden con
  `jp363`). Lo que falta es la verificación **en vivo** de los cuerpos de los paquetes (login, mover,
  chat, combate, tienda, estados…), con la misma disciplina de "pendiente de probar": `jp363` se modeló
  sobre l1j-jp (japonés) y esta rama es l1j-en, así que algún formato de cuerpo podría diferir.
- Para volver a GM una cuenta: igual que el s3ep1 pero con el socket de la base 3310
  (`--socket=servidor_classic363/datos/my.sock -uroot l1jdb`, `UPDATE accounts SET access_level=200 ...`).

# Análisis del cliente Lineage

- `PROTOCOLO_RED.md`: descripción del protocolo de red (entramado, cifrado, opcodes).
- `opcodes/CATALOGO_2009.md`: **catálogo de todos los opcodes del cliente 2009** (`S3EP1U.bin`) con formato, nombre L1J, certeza y evidencia.
- `opcodes/`: archivos `opcodes.txt` para l1j-client (cliente 2009 y familias públicas), sus fuentes y la **homologación con el cliente 2014** (`opcodes/README.md`).
- `COMPARACION_L1J_CLIENT.md`: cruce con https://github.com/uglyoldbob/l1j-client (commit `83052f6`). Incluye la tabla de opcodes en tres numeraciones y los errores encontrados en l1j-client.
- `PLAN_CLIENTE.md`: plan para completar l1j-client como cliente multiplataforma: C++20 orientado a datos sobre SDL3, módulos, presupuestos de rendimiento y 11 entregables jugables. Los entregables 1–3 están implementados en `../cliente/` (ver `cliente/ENTREGABLES.md`).

## scripts/

| Script | Qué hace | Requiere |
|---|---|---|
| `lineage_proto.py` | Implementación de referencia: cifrado, entramado y lector de campos | Python 3 |
| `secciones_pe.py` | Secciones PE y entropía, para detectar binarios empaquetados | Python 3 |
| `tablas_opcodes.py` | Extrae las tablas de despacho de opcodes de `S3EP1U.bin` | Python 3 |
| `emu_key.py` | Emula la derivación de clave del cliente y la compara con la fórmula simple | unicorn |
| `test_cipher.py` | Emula el cifrado y descifrado reales del cliente y los compara con `lineage_proto.py` | unicorn |
| `formatos_opcodes.py` | Para cada opcode del juego, extrae las cadenas de formato que usa el manejador | capstone |
| `opcodes_cliente.py` | Extrae los paquetes cliente → servidor (opcode y formato) de las llamadas a `sendPacket` | capstone |
| `pistas_manejadores.py` | Para cada manejador servidor → cliente, lista las cadenas que usa y los paquetes que envía | capstone |
| `contexto_envios.py` | Para cada opcode cliente → servidor, lista la función que lo envía, sus cadenas y los prefijos de chat que compara | capstone |
| `formatos_l1j.py` | Extrae el formato de cada paquete de un servidor L1J en Java (no incluido; se pasa la ruta) | Python 3 |
| `emparejar_formatos.py` | Compara los formatos del binario con los de L1J y propone nombres | Python 3 |
| `generar_opcodes.py` | Genera los `opcodes/*.txt` y valida el emparejamiento de nombres internos con nombres L1J | Python 3 |
| `comparar_tablas.py` | Compara los opcodes del cliente 2009 con las tablas públicas | Python 3 |
| `identificar_version.py` | Lee una captura pcap o pcapng (o los bytes del primer paquete), identifica la familia de opcodes y descifra la sesión | Python 3 |
| `test_identificar.py` | Prueba `identificar_version.py` con capturas sintéticas | Python 3 |
| `desensamblar.py` | Desensambla una dirección o busca llamadas a una función en `S3EP1U.bin` | capstone |

Para instalar las dependencias: `python3 -m venv venv && venv/bin/pip install -r scripts/requirements.txt`

Ejemplos, desde la carpeta del juego:

```
python3 analisis/scripts/tablas_opcodes.py S3EP1U.bin
venv/bin/python analisis/scripts/test_cipher.py S3EP1U.bin analisis/scripts
venv/bin/python analisis/scripts/desensamblar.py S3EP1U.bin dis 0x4a7980 200
venv/bin/python analisis/scripts/desensamblar.py S3EP1U.bin calls 0x4a7a30
```

## resultados/

| Archivo | Contenido |
|---|---|
| `secciones_pe.txt` | Secciones de cada ejecutable. Muestra que `lin.bin` está empaquetado |
| `tablas_opcodes.txt` | Opcodes servidor → cliente, dentro y fuera del juego (cliente 2009) |
| `derivacion_clave_emulada.txt` | Comparación de la clave derivada por el cliente con la fórmula simple |
| `verificacion_cifrado.txt` | Resultado de la comparación con las rutinas reales (1500 paquetes) |
| `formatos_opcodes_juego.txt` | Formato de cada uno de los 110 opcodes servidor → cliente del juego |
| `opcodes_cliente_servidor.txt` | Opcodes y formatos cliente → servidor (368 llamadas de envío) |
| `generar_opcodes.txt` | Validación del emparejamiento de nombres (38/39) y cobertura de cada `opcodes.txt` |
| `comparacion_tablas_publicas.txt` | Los 35 opcodes identificados del cliente 2009 frente a 10 tablas públicas (0 coincidencias) |
| `pistas_manejadores.txt` | Cadenas y envíos de cada manejador servidor → cliente |
| `contexto_envios.txt` | Función, cadenas y prefijos de chat de cada envío cliente → servidor |
| `formatos_l1j_wk.txt`, `formatos_l1j_jp363.txt` | Formatos de paquete de l1j-wk (commit `63f37b7`) y L1J-JP 3.63c (commit `403e35a`) |
| `emparejamiento_formatos.txt` | Candidatos L1J para cada opcode 2009 según el formato |
| `test_identificar.txt` | Resultado de las pruebas de la herramienta de identificación |
| `desensamblado_rutinas_clave.txt` | Desensamblado de cifrado, descifrado, envío, recepción e inicio de clave |

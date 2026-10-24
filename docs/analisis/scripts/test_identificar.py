"""Prueba identificar_version.py con capturas pcap sintéticas (sesiones 2009 y KR 3.80).
Uso: python3 test_identificar.py <carpeta temporal>"""
import os, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lineage_proto as lp

def ip_tcp(src, dst, sp, dp, seq, flags, payload):
    tcp = struct.pack('>HHIIBBHHH', sp, dp, seq, 0, 5 << 4, flags, 65535, 0, 0) + payload
    ip = struct.pack('>BBHHHBBH4s4s', 0x45, 0, 20 + len(tcp), 0, 0, 64, 6, 0, bytes(src), bytes(dst))
    return b'\x00' * 12 + b'\x08\x00' + ip + tcp

def pcap(frames):
    out = struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1)
    for f in frames:
        out += struct.pack('<IIII', 0, 0, len(f), len(f)) + f
    return out

def sesion(init_body, srv_bodies, cli_bodies, port=2000):
    C, S = [10, 0, 0, 2], [10, 0, 0, 1]
    seed = struct.unpack_from('<I', init_body, 1)[0]
    tx_s, tx_c = lp.Cipher(seed), lp.Cipher(seed)
    frames = [ip_tcp(C, S, 50000, port, 0, 0x02, b'')]
    sseq, cseq = 1000, 1
    def add(src_is_srv, raw):
        nonlocal sseq, cseq
        # se parte en dos segmentos y se repite el primero (retransmisión)
        half = max(1, len(raw) // 2)
        for chunk_off, chunk in ((0, raw[:half]), (0, raw[:half]), (half, raw[half:])):
            if not chunk: continue
            if src_is_srv: frames.append(ip_tcp(S, C, port, 50000, sseq + chunk_off, 0x18, chunk))
            else: frames.append(ip_tcp(C, S, 50000, port, cseq + chunk_off, 0x18, chunk))
        if src_is_srv: sseq += len(raw)
        else: cseq += len(raw)
    add(True, lp.frame(init_body))
    for i in range(max(len(srv_bodies), len(cli_bodies))):
        if i < len(cli_bodies): add(False, lp.frame(tx_c.encrypt(lp.pad_client_body(cli_bodies[i]))))
        if i < len(srv_bodies): add(True, lp.frame(tx_s.encrypt(srv_bodies[i])))
    return pcap(frames)

tmp = sys.argv[1]
casos = {
    '2009': (bytes([125]) + struct.pack('<I', 0x1234abcd),
             [bytes([89, 0, 1]) + b'\x00' * 22, bytes([66]) + b'hola\x00'],
             [bytes([34]) + b'\x33\x00' + b'\xff' * 4 + b'\x20\x00\x00', bytes([46])],
             '2009_S3EP1'),
    'KR380': (bytes([150]) + struct.pack('<I', 0) + bytes.fromhex('9dd1d67af462e7a06602fa'),
              [bytes([77, 0, 1]) + b'\x00' * 30],
              [bytes([106]) + b'\x00' * 10],
              'KR_3.80'),
}
fallos = 0
for nombre, (init, s, c, esperado) in casos.items():
    ruta = os.path.join(tmp, nombre + '.pcap')
    open(ruta, 'wb').write(sesion(init, s, c))
    out = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'identificar_version.py'),
                          ruta, '--puerto', '2000'], capture_output=True, text=True).stdout
    ok = ('familia: ' + esperado) in out and 'confirmada' in out
    descifrado_ok = all(('S %3d' % b[0]) in out for b in s) and all(('C %3d' % b[0]) in out for b in c)
    print('%-6s identificación %s, descifrado %s' % (nombre, 'OK' if ok else 'FALLO', 'OK' if descifrado_ok else 'FALLO'))
    if not (ok and descifrado_ok):
        fallos += 1; print(out)
sys.exit(fallos)

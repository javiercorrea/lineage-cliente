import sys, os, struct, random
from unicorn import *
from unicorn.x86_const import *
bin_path, proto_dir = sys.argv[1], sys.argv[2]
sys.path.insert(0, proto_dir)
import lineage_proto as lp
d=open(bin_path,'rb').read()
mu=Uc(UC_ARCH_X86,UC_MODE_32); mu.mem_map(0x400000,0x600000); mu.mem_write(0x401000,d[0x1000:0x4f4000])
mu.mem_map(0x100000,0x10000)
def call(fn, keyaddr, key, buf):
    mu.mem_write(keyaddr, bytes(key)); mu.mem_write(0x108000, bytes(buf))
    sp=0x10f000-4; mu.mem_write(sp,struct.pack('<I',0x10a000)); mu.mem_write(0x10a000,b'\xf4')
    mu.reg_write(UC_X86_REG_ESP,sp); mu.reg_write(UC_X86_REG_ECX,0x108000); mu.reg_write(UC_X86_REG_EDX,len(buf))
    mu.emu_start(fn,0x10a000,count=100000)
    return bytes(mu.mem_read(0x108000,len(buf)))
random.seed(1); ok=0
for t in range(300):
    seed=random.getrandbits(32); n=random.choice([4,8,12,16,32,64,100,252])
    msgs=[os.urandom(n) for _ in range(5)]
    cli=lp.Cipher(seed); srv=lp.Cipher(seed)
    for m in msgs:
        k=bytes(cli.key)
        ce=call(0x4a7980,0x8ed660,k,m)          # client encrypt (real code)
        pe=cli.encrypt(m)
        assert ce==pe, ('enc',t)
        k2=bytes(srv.key)
        cd=call(0x4a7900,0x8ed658,k2,ce)        # client decrypt routine (real code)
        pd=srv.decrypt(ce)
        assert cd==pd==m, ('dec',t)
        ok+=1
print('OK', ok, 'packets match real client routines')

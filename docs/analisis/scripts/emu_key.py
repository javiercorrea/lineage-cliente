import sys, struct
from unicorn import *
from unicorn.x86_const import *
d=open(sys.argv[1],'rb').read()
def run(seed):
    mu=Uc(UC_ARCH_X86,UC_MODE_32)
    mu.mem_map(0x400000,0x600000)
    mu.mem_write(0x401000,d[0x1000:0x4f4000])

    mu.mem_map(0x100000,0x10000); sp=0x10f000
    key=0x108000; mu.mem_write(key,struct.pack('<II',seed,0x930fd7e2))
    ret=0x10a000; mu.mem_write(ret,b'\xf4')
    sp-=4; mu.mem_write(sp,struct.pack('<I',ret))
    mu.reg_write(UC_X86_REG_ESP,sp); mu.reg_write(UC_X86_REG_ECX,key); mu.reg_write(UC_X86_REG_EBP,sp+0x100)
    mu.emu_start(0x481b20,ret,count=2000000)
    return struct.unpack('<II',mu.mem_read(key,8))
rotl=lambda x,n:((x<<n)|(x>>(32-n)))&0xffffffff
for seed in [0,1,0x12345678,0xdeadbeef]:
    k=run(seed)
    a=rotl(seed^0x9c30d539,19); b=0x930fd7e2^a^0x7c72e993
    print(hex(seed),'client:',[hex(x) for x in k],' L1J-formula:',hex(a),hex(b))

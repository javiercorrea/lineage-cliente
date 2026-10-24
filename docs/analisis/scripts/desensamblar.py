import sys,struct
from capstone import *
d=open(sys.argv[1],'rb').read()
code=d[0x1000:0x4f4000]; base=0x401000
md=Cs(CS_ARCH_X86,CS_MODE_32)
def calls_to(t):
    r=[]
    for i in range(len(code)-5):
        if code[i]==0xe8 and (base+i+5+struct.unpack_from('<i',code,i+1)[0])&0xffffffff==t: r.append(hex(base+i))
    return r
def dis(a,n):
    for ins in md.disasm(code[a-base:a-base+n],a): print(hex(ins.address),ins.mnemonic,ins.op_str)
cmd=sys.argv[2]
if cmd=='calls': print(calls_to(int(sys.argv[3],16)))
else: dis(int(sys.argv[3],16),int(sys.argv[4]))

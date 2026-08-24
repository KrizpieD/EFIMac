from capstone import *
from capstone.ppc import *
code = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\region.bin","rb").read()
base = 0x40B26300
md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
md.detail = True
for i in md.disasm(code, base):
    print(f"{i.address:#010x}  {i.mnemonic:10s} {i.op_str}")

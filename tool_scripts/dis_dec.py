import sys
sys.path.insert(0, r"C:\Users\clayc\AppData\Local\Temp\opencode")
from capstone import *
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
base = 0x40800000
start = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x40B230C0
n = int(sys.argv[2]) if len(sys.argv) > 2 else 24
md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
off = start - base
for i in md.disasm(data[off:off + n * 4], start):
    print(f"{i.address:08x}: {i.bytes.hex()}  {i.mnemonic} {i.op_str}")

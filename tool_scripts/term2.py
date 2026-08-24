from capstone import *
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
off = 0x40B2731C - BASE
for i in md.disasm(rom[off:off+4*80], 0x40B2731C):
    print(f"{i.address:08x}: {i.bytes.hex():>8}  {i.mnemonic} {i.op_str}")

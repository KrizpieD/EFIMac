from capstone import *
import pathlib
d = pathlib.Path(r'C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin').read_bytes()
base = 0x40800000
md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN + CS_MODE_32)
start, end = 0x40B26400, 0x40B27550
for i in md.disasm(d[(start-base):(end-base)], start):
    print('0x%08x: %-10s %-24s %s' % (i.address, bytes(i.bytes).hex(), i.mnemonic, i.op_str))

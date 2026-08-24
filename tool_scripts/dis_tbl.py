from capstone import *
import pathlib
d = pathlib.Path(r'C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin').read_bytes()
base = 0x40800000
def dis(start, end):
    md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN + CS_MODE_32)
    code = d[(start-base):(end-base)]
    for i in md.disasm(code, start):
        print('0x%08x: %-24s %s %s' % (i.address, bytes(i.bytes).hex(), i.mnemonic, i.op_str))
dis(0x40B28E00, 0x40B290B0)

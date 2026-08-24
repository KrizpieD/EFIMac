from capstone import *
import pathlib
d = pathlib.Path(r'C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin').read_bytes()
base = 0x40800000
md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN + CS_MODE_32)
for (start,end,label) in [(0x40B23700,0x40B237B0,'SPIN 0x40B2375C'), (0x40B22E20,0x40B22E60,'LR target 0x40B22E48')]:
    print('=== %s ===' % label)
    for i in md.disasm(d[(start-base):(end-base)], start):
        print('0x%08x: %-10s %-30s %s' % (i.address, bytes(i.bytes).hex(), i.mnemonic, i.op_str))
    print()

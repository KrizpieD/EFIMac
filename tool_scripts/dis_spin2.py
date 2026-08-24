from capstone import *
import pathlib
d = pathlib.Path(r'C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin').read_bytes()
base = 0x40800000
md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN + CS_MODE_32)
for (start,end,label) in [(0x40b237b0,0x40b23830,'after stable read'), (0x40b23614,0x40b23700,'bl 0x40b23614 target'), (0x40b2281c,0x40b228a0,'alloc helper')]:
    print('=== %s ===' % label)
    for i in md.disasm(d[(start-base):(end-base)], start):
        print('0x%08x: %-10s %-30s %s' % (i.address, bytes(i.bytes).hex(), i.mnemonic, i.op_str))
    print()

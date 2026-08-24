import sys
from capstone import *

path = r'C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin'
ROM_BASE = 0x40800000
data = open(path, 'rb').read()

start = int(sys.argv[1], 0)
end = int(sys.argv[2], 0)
count = sys.argv[3] if len(sys.argv) > 3 else None

off = start - ROM_BASE
length = (end - start) if count is None else int(count, 0) * 4
code = data[off:off + length]

md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN + CS_MODE_32)
md.detail = False
n = 0
for i in md.disasm(code, start):
    print('0x%08x:  %s %s' % (i.address, i.mnemonic, i.op_str))
    n += 1
    if count is not None and n >= int(count, 0):
        break

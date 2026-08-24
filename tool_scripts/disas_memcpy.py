import os, struct
from capstone import *
p = os.path.join(os.environ['TEMP'], 'opencode', 'macosrom_flat.bin')
data = open(p, 'rb').read()
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)

print('--- raw words 0x4690-0x46C0 ---')
for off in range(0x4690, 0x46C0, 2):
    w = struct.unpack('>H', data[off:off + 2])[0]
    print(f'{0x40800000+off:08X}: {w:04X}')

print('--- disasm from 0x46BA ---')
for i in md.disasm(data[0x46BA:0x47B0], 0x408046BA):
    print(f'{i.address:08X}: {i.bytes.hex():<12} {i.mnemonic} {i.op_str}')

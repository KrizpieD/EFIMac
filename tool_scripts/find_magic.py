import os, struct
from capstone import *
p = os.path.join(os.environ['TEMP'], 'opencode', 'macosrom_flat.bin')
data = open(p, 'rb').read()
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)

print('=== u32 0x00000DB0 occurrences (abs.L refs) ===')
needle = struct.pack('>I', 0x00000DB0)
idx = 0
found = []
while True:
    i = data.find(needle, idx)
    if i < 0:
        break
    found.append(i)
    idx = i + 1
for off in found:
    print(f'offset {off:#x}')

# try disassembling each as instruction start and also one word earlier
for off in found[:20]:
    for back in (0, 2):
        s = off - back
        ins = next(md.disasm(data[s:s + 14], 0x40800000 + s), None)
        if ins and ('0db0' in ins.op_str.lower() or '$db0' in ins.op_str.lower()):
            print(f'  @{s:#x}: {ins.mnemonic} {ins.op_str}')

print('=== context around data magic @0xce8 ===')
start = 0xcc0
for i in md.disasm(data[start:start + 0x50], 0x40800000 + start):
    print(f'{i.address:08X}: {i.bytes.hex():<12} {i.mnemonic} {i.op_str}')

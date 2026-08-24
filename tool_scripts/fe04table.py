"""Disassemble FE0A handler + locate the FE04 selector byte-table."""
import struct
from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN

ROM_PATH = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'
ROM_BASE = 0x40800000
with open(ROM_PATH, 'rb') as f:
    rom = f.read()
md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)

def dump(start, end, label):
    print(f"=== {label}: {start:08X}-{end:08X} ===")
    for insn in md.disasm(rom[start-ROM_BASE:end-ROM_BASE], start):
        print(f"  {insn.address:08X}: {insn.bytes.hex():<8} {insn.mnemonic:<10} {insn.op_str}")

dump(0x40B6DA44, 0x40B6DAB0, "FE06/FE0A handler (VMDispatch)")

print()
print("=== candidate FE04 byte tables at xx..xD850 in ROM ===")
# table addr = r30|0xD850 -> try every 64KB window whose low16 == 0xD850
hits = []
for i in range(0, len(rom) - 32, 2):
    addr = ROM_BASE + i
    if (addr & 0xFFFF) == 0xD850:
        vals = list(rom[i:i+27])
        # plausible: all <= 0xF8 and mostly small, non-uniform
        if max(vals) <= 0xF8 and len(set(vals)) > 5:
            hits.append((addr, vals))
for addr, vals in hits[:12]:
    print(f"  @{addr:08X}: {vals}")

"""Disassemble FE04/FE05 selector handler stubs assuming ED = 0x40B70000."""
import struct
from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN

ROM_PATH = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'
ROM_BASE = 0x40800000
with open(ROM_PATH, 'rb') as f:
    rom = f.read()
md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)

TBL = [0xD5, 0xDC, 0xE1, 0xE3, 0xE2, 0xDD, 0xD7, 0xCD, 0xC1, 0xB4,
       0xA4, 0x96, 0x8B, 0x83, 0x7B, 0x74, 0x6D, 0x67, 0x61, 0x5B,
       0x56, 0x51, 0x4D, 0x4A, 0x47, 0x45]

# Sanity-check hypothesis: dump raw region ED+0x40..ED+0xF0 first
ED = 0x40B70000
print("raw @ED+0x40:", rom[(ED+0x40)-ROM_BASE:(ED+0x100)-ROM_BASE].hex(' '))
print()

start = ED + 0x40
end   = ED + 0x100
for insn in md.disasm(rom[start-ROM_BASE:end-ROM_BASE], start):
    print(f"  {insn.address:08X} (ED+{insn.address-ED:02X}): {insn.bytes.hex():<8} {insn.mnemonic:<10} {insn.op_str}")

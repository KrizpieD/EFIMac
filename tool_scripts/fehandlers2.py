"""Disassemble the F-line software-function handlers region."""
import struct
from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN

ROM_PATH = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'
ROM_BASE = 0x40800000
with open(ROM_PATH, 'rb') as f:
    rom = f.read()
md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)

start = 0x40B6D760
end   = 0x40B6DB30
code = rom[start-ROM_BASE:end-ROM_BASE]
for insn in md.disasm(code, start):
    print(f"  {insn.address:08X}: {insn.bytes.hex():<8} {insn.mnemonic:<8} {insn.op_str}")

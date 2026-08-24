"""Disassemble 68K code regions in the flat Mac OS ROM around the FE04/FE0A call sites."""
import struct
from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_040

ROM_PATH = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'
ROM_BASE = 0x40800000

with open(ROM_PATH, 'rb') as f:
    rom = f.read()

md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

def dis(start, end):
    off = start - ROM_BASE
    code = rom[off:end - ROM_BASE]
    for insn in md.disasm(code, start):
        print(f"  {insn.address:08X}: {insn.bytes.hex():<12} {insn.mnemonic} {insn.op_str}")

print("=== Region around FE04 call site 0x408A8DC0-0x408A8E60 ===")
dis(0x408A8DC0, 0x408A8E60)

print()
print("=== Region around FE0A call site A: 0x40807B80-0x40807D10 ===")
dis(0x40807B80, 0x40807D10)

print()
print("=== Region around caller/return path: 0x408079E0-0x40807A80 ===")
dis(0x408079E0, 0x40807A80)

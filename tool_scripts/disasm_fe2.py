"""Disassemble 68K regions with raw word fallback."""
import struct
from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_040

ROM_PATH = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'
ROM_BASE = 0x40800000

with open(ROM_PATH, 'rb') as f:
    rom = f.read()

md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

def dis(start, end):
    off = start - ROM_BASE
    addr = start
    while addr < end:
        code = rom[addr - ROM_BASE:end - ROM_BASE]
        got = False
        for insn in md.disasm(code, addr):
            print(f"  {insn.address:08X}: {insn.bytes.hex():<12} {insn.mnemonic} {insn.op_str}")
            addr += insn.size
            got = True
            break
        if not got:
            w = struct.unpack('>H', rom[addr - ROM_BASE:addr - ROM_BASE + 2])[0]
            print(f"  {addr:08X}: {w:04x}       dc.w ${w:04x}")
            addr += 2

print("=== 0x40807B80-0x40807D10 ===")
dis(0x40807B80, 0x40807D10)
print()
print("=== 0x408079E0-0x40807AF0 ===")
dis(0x408079E0, 0x40807AF0)

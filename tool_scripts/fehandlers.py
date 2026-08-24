"""Compute branch targets of dispatch-table entries and disassemble the handlers."""
import struct
from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN

ROM_PATH = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'
ROM_BASE = 0x40800000
TBL = 0x380000
EMU = 0x36D000   # emulator code region start within ROM

with open(ROM_PATH, 'rb') as f:
    rom = f.read()

md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)

def branch_target(entry_addr, word):
    li = word & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return (entry_addr + li) & 0xFFFFFFFF

ops = list(range(0xFE00, 0xFE10))
targets = {}
for op in ops:
    e_off = TBL + op * 8
    w1_off = e_off + 4
    pc_guest = ROM_BASE + w1_off
    tgt = branch_target(pc_guest, rd := struct.unpack('>I', rom[w1_off:w1_off+4])[0])
    targets[op] = tgt
    print(f"op {op:04X}: entry0={struct.unpack('>I',rom[e_off:e_off+4])[0]:08X} -> handler {tgt:08X} (rom+{tgt-ROM_BASE:06X})")

print()
print("=== Handler code: 0x%08X - 0x%08X ===" % (min(targets.values()) - 0x20, max(targets.values()) + 0x60))
start = min(targets.values()) - 0x20
end = max(targets.values()) + 0x60
code = rom[start-ROM_BASE:end-ROM_BASE]
for insn in md.disasm(code, start):
    print(f"  {insn.address:08X}: {insn.bytes.hex():<8} {insn.mnemonic:<8} {insn.op_str}")

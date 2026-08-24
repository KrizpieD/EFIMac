"""Dump 68K context around F-line software-opcode call sites."""
import struct
from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_040

ROM_PATH = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'
ROM_BASE = 0x40800000
with open(ROM_PATH, 'rb') as f:
    rom = f.read()
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

def show(pc, back=4, fwd=3):
    start = pc - back*2
    print(f"--- context @ {pc:08X} ---")
    addr = start
    while addr < pc + fwd*2:
        w = struct.unpack('>H', rom[addr-ROM_BASE:addr-ROM_BASE+2])[0]
        mark = ' <<<' if addr == pc else ''
        # try decode
        ins = next(md.disasm(rom[addr-ROM_BASE:addr-ROM_BASE+10], addr), None)
        txt = f"{ins.mnemonic} {ins.op_str}" if ins else ""
        print(f"  {addr:08X}: {w:04x}  {txt}{mark}")
        addr += 2

for pc in [0x4080ff1a, 0x4080ff24, 0x4081001e, 0x40828508, 0x40858192,
           0x4088392e, 0x408a8e7a, 0x408aa8c8, 0x408d82a2]:
    show(pc)

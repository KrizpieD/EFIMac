#!/usr/bin/env python3
"""68000/68020 disassembler for the EFIMac guest ROM boot code (capstone).

Usage:
  python dis_m68k.py <addr_start> [<addr_end>] [--rom <flat_rom_path>]
Helps trace the ROM's 68K boot stub + A-line trap dispatch that the embedded
PPC DR emulator executes.
"""
import sys
from capstone import Cs, CS_ARCH_M68K, CS_MODE_M68K_000

ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin"
ROM_BASE = 0x40800000

def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__); return
    start = int(args[0], 16)
    end = int(args[1], 16) if len(args) > 1 else start + 0x200
    if "--rom" in args:
        i = args.index("--rom"); ROM_arg = args[i+1]
        global ROM
        ROM = ROM_arg
    d = bytearray(open(ROM, "rb").read())
    phy = start - ROM_BASE
    code = bytes(d[phy: end - ROM_BASE])
    md = Cs(CS_ARCH_M68K, CS_MODE_M68K_000)
    count = 0
    for insn in md.disasm(code, start):
        print("0x%08X:  %-24s %s %s" % (insn.address, insn.bytes.hex(), insn.mnemonic, insn.op_str))
        count += 1
        if count > 400:
            print("... truncated")
            break

if __name__ == "__main__":
    main()

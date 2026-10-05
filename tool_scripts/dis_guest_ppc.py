"""Disassemble a guest PowerPC range from the decoded flat New World ROM.

The on-disk `Mac OS ROM` file is a CHRP parcel container, so its bytes do NOT
match guest memory; `decoderom.py` expands the `rom ` parcel into the flat
image that the bootloader installs at 0x40800000. Use that flat image here:

    python tool_scripts/decoderom.py "<esp>\\System\\MacOS\\ROM" flat.bin
    python tool_scripts/dis_guest_ppc.py flat.bin 0x40B24300 0x40B24600

Verified against the runtime dumps the emulator prints (DISPATCH/SCHEDENT/
PR9HI/PATCHSITE/INITLOOP): 320/320 words match.
"""

import struct
import sys

from capstone import CS_ARCH_PPC, CS_MODE_BIG_ENDIAN, Cs

ROM_BASE = 0x40800000


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    path = sys.argv[1]
    start = int(sys.argv[2], 0)
    stop = int(sys.argv[3], 0)
    data = open(path, "rb").read()
    md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN)
    print("=== %#x .. %#x ===" % (start, stop))
    for addr in range(start, stop, 4):
        off = addr - ROM_BASE
        word = data[off:off + 4]
        if len(word) < 4:
            print("  %#x: <past end of image>" % addr)
            break
        value = struct.unpack(">I", word)[0]
        ins = list(md.disasm(word, addr))
        if ins:
            print("  %#010x: %08X  %-8s %s" % (addr, value, ins[0].mnemonic,
                                                ins[0].op_str))
        else:
            print("  %#010x: %08X  <invalid>" % (addr, value))
    return 0


if __name__ == "__main__":
    sys.exit(main())

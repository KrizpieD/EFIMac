import struct, sys

ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin"
BASE = 0x40800000
data = open(ROM, "rb").read()

def off(pc):
    # ROM alias: 0x40800000..0x40C00000 maps directly; FFFFxxxx maps to low file
    if 0x40800000 <= pc < 0x40C00000:
        return pc - BASE
    return pc & 0x3FFFFF

def rd16(pc):
    o = off(pc)
    return struct.unpack(">H", data[o:o+2])[0]

def rd32(pc):
    o = off(pc)
    return struct.unpack(">I", data[o:o+4])[0]

# 1. Dump the full thunk table at 0xFFFF0000 region (file 0x3F0000-0x3F0300)
print("=== THUNK TABLE 0xFFFF0000-0xFFFF0300 (file 0x3F0000-0x3F0300) ===")
for addr in range(0xFFFF0000, 0xFFFF0300, 2):
    w = rd16(addr)
    if w == 0 and addr > 0xFFFF0280:
        break
    # Mark the DR-opcode slots (every 8 bytes, offset 4)
    rec = (addr - 0xFFFF0000)
    if (rec % 8) == 0:
        print()
        print(f"  Record {rec//8:#x} @ {addr:#010x}: ", end="")
    print(f"{w:#06x} ", end="")
print("\n")

# 2. Dump handler body at 0x40865E40-0x40866200
print("=== HANDLER 0x40865E40-0x40866200 ===")
for addr in range(0x40865E40, 0x40866200, 2):
    w = rd16(addr)
    print(f"  {addr:#010x}: {w:#06x}")
print()

# 3. Search for any MOVE.L #xxx,0x069C pattern in the ROM
#    MOVE.L #imm,Dn = 0x203C/223C/etc, MOVE.L #imm,(abs) = 0x23FC
print("=== SEARCH for writes to 0x069C in ROM ===")
for off in range(0, len(data) - 6, 2):
    w1 = struct.unpack(">H", data[off:off+2])[0]
    w2 = struct.unpack(">H", data[off+2:off+4])[0]
    w3 = struct.unpack(">H", data[off+4:off+6])[0]
    pc = off + BASE
    # MOVE.L #imm,(abs.W) = 0x23FC <imm32> <addr16>
    if w1 == 0x23FC and w3 == 0x069C:
        imm = struct.unpack(">I", data[off+2:off+6])[0]  # overlap, let me fix
        print(f"  {pc:#010x}: MOVE.L #{imm:#08x},(0x069C)")
    # MOVE.L Dn,(abs.W) where addr = 0x069C: 0x21C0/23C0/etc + 0x069C
    if w1 & 0xF1FF == 0x21C0 and w2 == 0x069C:
        print(f"  {pc:#010x}: MOVE.L D{(w1>>9)&7},(0x069C)")
    # MOVE.L An,(abs.W) where addr = 0x069C
    if w1 & 0xF1FF == 0x23C8 and w2 == 0x069C:
        print(f"  {pc:#010x}: MOVE.L A{(w1>>9)&7},(0x069C)")
print("  (done)")

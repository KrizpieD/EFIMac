import struct, sys

ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin"
BASE = 0x40800000
data = open(ROM, "rb").read()

def rd16(pc):
    off = pc - BASE
    return struct.unpack(">H", data[off:off+2])[0]

def rd32(pc):
    off = pc - BASE
    return struct.unpack(">I", data[off:off+4])[0]

# Decode the 2278 at 0x408661E4
pc = 0x408661E4
op = rd16(pc)
ext = rd16(pc + 2)
print(f"PC={pc:#x} opcode={op:#06x}  abs_word_addr={ext:#06x}")
print(f"  2278 = MOVE.L ({ext:#06x}).W,A1")
print(f"  Reading long from absolute-short {ext:#06x}: value = {rd32(ext):#010x}")

# Also show the 10 bytes around the JMP(A1)
print("\nBytes around 0x408661E4:")
for addr in range(0x408661E0, 0x40866200, 2):
    print(f"  {addr:#010x}: {rd16(addr):#06x}")

# Show what's at the low-RAM slot
slot = ext
print(f"\nLow-RAM slot at {slot:#06x} (long): {rd32(slot):#010x}")
print(f"  Word 0: {rd16(slot):#06x}")
print(f"  Word 2: {rd16(slot+2):#06x}")

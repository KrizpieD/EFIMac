import struct
ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin"
data = open(ROM, "rb").read()
ROM_BASE = 0x40800000
def dis(op):
    w = struct.unpack_from(">I", data, op)[0]
    print(f"  0x{ROM_BASE+op:08X}: 0x{w:08X}")
print("== 0x40B10000 window (0x310000) ==")
for o in range(0x310000, 0x310060, 4):
    dis(o)
print()
print("== 0x40B126C0 window (0x3126C0) ==")
for o in range(0x3126C0, 0x312760, 4):
    dis(o)
print()
print("== 0x40B104A8 window (0x3104A8) ==")
for o in range(0x3104A0, 0x3104E0, 4):
    dis(o)

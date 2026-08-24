import struct
ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin"
data = open(ROM, "rb").read()
print("rom size:", len(data))
for o in (0x310018, 0x31001C, 0x3126E8, 0x3104A8):
    w = struct.unpack_from(">I", data, o)[0]
    print(f"offset 0x{o:X} guest 0x{0x40800000+o:08X}: 0x{w:08X}")

# sanity: count bl (opcode 18, LK=1) in rom
import sys
sys.path.insert(0, r"C:\Users\clayc\AppData\Local\Temp\opencode")
import ppcdis
print("ppcdis module:", ppcdis)
print(ppcdis.dis(struct.unpack_from(">I", data, 0x310018)[0], 0x40B10018))
print(ppcdis.dis(struct.unpack_from(">I", data, 0x31001C)[0], 0x40B1001C))
print(ppcdis.dis(struct.unpack_from(">I", data, 0x3126E8)[0], 0x40B126E8))
print(ppcdis.dis(struct.unpack_from(">I", data, 0x3104A8)[0], 0x40B104A8))

import struct
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
base = 0x40800000
targets = [0x40b26880, 0x40b268ec, 0x40b26520]
for tgt in targets:
    print(f"callers of {tgt:08x}:")
    for off in range(0x300000, len(data) - 4, 4):
        w = int.from_bytes(data[off:off+4], "big")
        if (w >> 26) == 18 and (w & 1):  # bl
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            src = base + off
            if src + 4 + li == tgt:
                print(f"  {src:08x}")

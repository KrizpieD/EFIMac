data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
base = 0x40800000
lo, hi = 0x310000, 0x340000
for off in range(lo, hi, 4):
    w = int.from_bytes(data[off:off+4], "big")
    if (w >> 26) == 14 and (w & 0xFFFF) == 0xF2C:  # addi
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F
        print(f"  {base+off:08x}: {w:08x}  addi r{rt}, r{ra}, 0xf2c")
    if (w >> 26) == 15 and ((w & 0xFFFF) == 0xF2C):  # addis (would be 0x0F2C0000-ish)
        pass
# also check ori-based construction of 0xF2C nearby whole ROM NK region for 'ori rX, rX, 0xf2c'
for off in range(lo, hi, 4):
    w = int.from_bytes(data[off:off+4], "big")
    if (w >> 26) == 24 and (w & 0xFFFF) == 0xF2C:
        rs = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F
        print(f"  {base+off:08x}: {w:08x}  ori r{rs}, r{ra}, 0xf2c")

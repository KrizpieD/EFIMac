data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
base = 0x40800000
lo, hi = 0x310000, 0x330000
print("=== stores to 0xF2C(rX) ===")
for off in range(lo, hi, 4):
    w = int.from_bytes(data[off:off+4], "big")
    if (w >> 26) == 36 and (w & 0xFFFF) == 0xF2C:
        rs = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F
        print(f"  {base+off:08x}: {w:08x}  stw r{rs}, 0xf2c(r{ra})")
    if (w >> 26) == 32 and (w & 0xFFFF) == 0xF2C:
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F
        print(f"  {base+off:08x}: {w:08x}  lwz r{rt}, 0xf2c(r{ra})")

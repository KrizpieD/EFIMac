data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
base = 0x40800000
# NK region
lo, hi = 0x310000, 0x330000
print("=== stw rX, -0x10(r1)  (KDP-0x10 flag stores) ===")
for off in range(lo, hi, 4):
    w = int.from_bytes(data[off:off+4], "big")
    op = w >> 26
    if op == 36 and (w & 0xFFFF) == 0xFFF0:  # stw
        rs = (w >> 21) & 0x1F
        ra = (w >> 16) & 0x1F
        if ra == 1:
            print(f"  {base+off:08x}: {w:08x}  stw r{rs}, -0x10(r1)")
print("=== lwz rX, -0x10(r1) ===")
for off in range(lo, hi, 4):
    w = int.from_bytes(data[off:off+4], "big")
    op = w >> 26
    if op == 32 and (w & 0xFFFF) == 0xFFF0:
        rt = (w >> 21) & 0x1F
        ra = (w >> 16) & 0x1F
        if ra == 1:
            print(f"  {base+off:08x}: {w:08x}  lwz r{rt}, -0x10(r1)")

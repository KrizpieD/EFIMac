data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
base = 0x40800000
hits = []
for off in range(0x310000, 0x330000, 4):
    w = int.from_bytes(data[off:off+4], "big")
    op = w >> 26
    # ori rA,rS,0x8000 or oris ...0x8000
    if op in (24, 25) and (w & 0xFFFF) == 0x8000:
        hits.append((base + off, w, op))
print(f"{len(hits)} hits")
for a, w, op in hits[:40]:
    rs = (w >> 21) & 0x1F
    ra = (w >> 16) & 0x1F
    mn = "ori" if op == 24 else "oris"
    print(f"  {a:08x}: {w:08x}  {mn} r{ra}, r{rs}, 0x8000")

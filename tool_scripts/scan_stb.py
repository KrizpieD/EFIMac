data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
base = 0x40800000
hits = []
for off in range(0x300000, len(data) - 4, 4):
    w = int.from_bytes(data[off:off+4], "big")
    op = w >> 26
    ra = (w >> 16) & 0x1F
    d = w & 0xFFFF
    # stb rS, 2(r28) / sth / stw with r28 base, offset 2
    if op == 38 and ra == 28 and d == 2:
        hits.append((base + off, w, "stb"))
    elif op == 39 and ra == 28 and d == 2:
        hits.append((base + off, w, "stbu"))
print(f"{len(hits)} hits")
for a, w, mn in hits[:40]:
    rs = (w >> 21) & 0x1F
    print(f"  {a:08x}: {w:08x}  {mn} r{rs}, 2(r28)")

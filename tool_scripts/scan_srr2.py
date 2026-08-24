data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
base = 0x40800000
srr_writes = []
for off in range(0x310000, 0x330000, 4):
    w = int.from_bytes(data[off:off+4], "big")
    if (w >> 26) != 31:
        continue
    xo = (w >> 1) & 0x3FF
    if xo != 467:  # mtspr
        continue
    rs = (w >> 21) & 0x1F
    spr = (((w >> 11) & 0x1F) << 5) | ((w >> 16) & 0x1F)
    if spr in (26, 27):
        srr_writes.append((base + off, w, spr, rs))
print(f"SRR writes: {len(srr_writes)}")
for a, w, spr, rs in srr_writes:
    print(f"  {a:08x}: {w:08x}  mtspr {'SRR0' if spr==26 else 'SRR1'}, r{rs}")

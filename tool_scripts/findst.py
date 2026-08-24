import pathlib, struct
d = pathlib.Path(r'C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin').read_bytes()
NK = 0x310000
lo, hi = 0x310000, 0x318000
# stw rs, 0x6B4(r1) => 0x90010000 | (rs<<21) | 0x6B4 ; search word pattern
for off in range(lo, hi, 4):
    w = int.from_bytes(d[off:off+4],'big')
    op = w >> 26
    if op != 36:  # stw
        continue
    rs = (w >> 21) & 0x1F
    ra = (w >> 16) & 0x1F
    dd = w & 0xFFFF
    if ra == 1 and dd in (0x6A8, 0x6AC, 0x6B0, 0x6B4, 0x6B8, 0x6BC, 0x6C0):
        print('stw r%d, 0x%x(r1) at 0x%08x' % (rs, dd, 0x40B00000+off))

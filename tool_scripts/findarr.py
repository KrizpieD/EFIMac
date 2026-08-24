import pathlib
d = pathlib.Path(r'C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin').read_bytes()
# search code region 0x40B10000..0x40B13000 for stw rs, d(r1) where d in 0x70..0xF8 step8
lo, hi = 0x310000, 0x330000
offs = [0x70,0x78,0x80,0x88,0x90,0x98,0xA0,0xA8,0xB0,0xB8,0xC0,0xC8,0xD0,0xD8,0xE0,0xE8,0xF0,0xF8]
for off in range(lo, hi, 4):
    w = int.from_bytes(d[off:off+4],'big')
    op = w >> 26
    if op != 36: continue
    rs = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; dd = w & 0xFFFF
    if ra == 1 and dd in offs:
        print('stw r%d, 0x%x(r1) at guest 0x%08x' % (rs, dd, 0x40800000+off))

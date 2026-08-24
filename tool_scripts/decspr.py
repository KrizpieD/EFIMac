for w in (0x7D2802A6, 0x7D6000A6, 0x7C0000A6, 0x7D9A03A6, 0x7D7B03A6, 0x7C8803A6, 0x7D6B5078):
    op = w >> 26
    spr = ((w >> 16) & 0x1F) << 5 | ((w >> 11) & 0x1F)
    rt = (w >> 21) & 0x1F
    rs = rt
    ra = (w >> 16) & 0x1F
    rb = (w >> 11) & 0x1F
    xo = (w >> 1) & 0x3FF
    print(f"0x{w:08X} op={op} spr={spr} rt/rs={rt} ra={ra} rb={rb} xo=0x{xo:03X}")

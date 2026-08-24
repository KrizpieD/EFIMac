def SPR(w): return (((w >> 16) & 0x1F) | (((w >> 11) & 0x1F) << 5))
for (pc, w, name) in [
    (0x40B10010, 0x7C0000A6, "mfspr r0"),
    (0x40B10014, 0x54003036, "rlwinm r0,r0,0,27,27"),
    (0x40B1001C, 0x7D2802A6, "mfspr r9"),
    (0x40B10020, 0x3949FFE4, "addi r9,r9,-28"),
    (0x40B10028, 0x7D6000A6, "mfspr r11"),
    (0x40B10030, 0x7D6B5078, "andc r11,r11,r10"),
    (0x40B10034, 0x7D7B03A6, "mtspr SRR1, r11"),
    (0x40B10038, 0x7D9A03A6, "mtspr SRR0, r12"),
    (0x40B126EC, 0x7C8803A6, "mtspr SPRG4, r8"),
]:
    spr = SPR(w)
    print(f"PC=0x{pc:08X} w=0x{w:08X} {name}: SPR={spr} (0x{spr:X})")

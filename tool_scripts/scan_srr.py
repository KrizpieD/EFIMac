data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
base = 0x40800000

def decode(off):
    w = int.from_bytes(data[off:off+4], "big")
    return w

# classify words of interest in NK region
srr0_writes = []  # mtspr 26
srr1_writes = []  # mtspr 27
rfis = []
for off in range(0x310000, 0x330000, 4):
    w = decode(off)
    op = w >> 26
    if op != 31:
        continue
    xo = (w >> 1) & 0x3FF
    rs = (w >> 21) & 0x1F
    spr_lo = (w >> 11) & 0x1F   # spr[0:4]
    spr_hi = (w >> 16) & 0x1F   # spr[5:9]
    spr = spr_lo | (spr_hi << 5)
    if xo == 467:  # mtspr
        if spr == 26: srr0_writes.append(base + off)
        elif spr == 27: srr1_writes.append(base + off)
    elif xo == 19 and op == 19: pass
    if op == 19 and ((w >> 1) & 0x3FF) == 18:  # rfi XO=18? rfi XO=18? rfi is XO=18? no rfi XO=18? rfi = 19? RFCI? rfi XO=18
        pass
# rfi: opcode 19, XO=18, Rc=0 -> word 0x4C000064
for off in range(0x310000, 0x330000, 4):
    w = decode(off)
    if w == 0x4C000064:
        rfis.append(base + off)

print(f"SRR0 writes: {len(srr0_writes)}")
print(f"SRR1 writes: {len(srr1_writes)}")
print(f"rfi sites ({len(rfis)}):")
for a in rfis:
    print(f"  {a:08x}")

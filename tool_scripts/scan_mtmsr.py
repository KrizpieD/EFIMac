import sys
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
base = 0x40800000
# mtmsr rS: opcode31 XO=146 -> word & 0xFC1FFFFF == 0x7C000124
# mfmsr rD: opcode31 XO=83  -> word & 0xFC1FFFFF == 0x7C0000A6
for name, pat, mask in [("mtmsr", 0x7C000124, 0xFC1FFFFF), ("mfmsr", 0x7C0000A6, 0xFC1FFFFF)]:
    hits = []
    for off in range(0x310000, 0x330000, 4):  # NK region 0x40B10000-0x40B30000
        w = int.from_bytes(data[off:off+4], "big")
        if (w & mask) == pat:
            hits.append((base + off, w))
    print(f"{name}: {len(hits)} hits")
    for a, w in hits[:30]:
        rs = (w >> 21) & 0x1F
        print(f"  {a:08x}: {w:08x}  {name} r{rs}")

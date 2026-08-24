import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
BASE = 0x40800000
# find prologue: scan back from 0x40B1F3B8 for stwu r1 / stmu r1 patterns
start = 0x40B1F3B8 - BASE
for off in range(start, start-0x400, -4):
    w = struct.unpack_from(">I", rom, off)[0]
    # stwu r1, -X(r1): 0x9421xxxx
    if (w & 0xFFFF0000) == 0x94210000:
        print(f"stwu r1 at {BASE+off:08X}: {w:08X}")
        break
# find all bl targeting 0x40B1F380-0x40B1F420 region
targets = []
for off in range(0, len(rom)-4, 4):
    w = struct.unpack_from(">I", rom, off)[0]
    if (w >> 26) == 18:  # b/bl
        li = w & 0x03FFFFFC
        if li & 0x02000000: li -= 0x04000000
        tgt = BASE + off + li
        if 0x40B1F380 <= tgt <= 0x40B1F430:
            targets.append((BASE+off, tgt, bool(w & 1)))
for a,t,l in targets:
    print(f"branch at {a:08X} -> {t:08X} link={l}")

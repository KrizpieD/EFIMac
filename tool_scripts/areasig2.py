import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
BASE = 0x40800000
# lis rD,0x4152 => 0x3Cxx4152 ; ori rD,rD,0x4541 => 0x60xx4541
hits = []
for off in range(0, len(rom)-4, 4):
    w = struct.unpack_from(">I", rom, off)[0]
    if (w & 0xFFFF0000) == 0x41520000 and (w >> 16) != 0:
        pass
for off in range(0, len(rom)-8, 4):
    w1 = struct.unpack_from(">I", rom, off)[0]
    if (w1 & 0xFFFF0000) == 0x3C00_0000 | 0x4152_0000 - 0x3C000000 + 0x3C000000:  # placeholder
        pass
# simpler: scan for addis with D field any, value 0x4152
cnt = 0
for off in range(0, len(rom)-8, 4):
    w1 = struct.unpack_from(">I", rom, off)[0]
    if (w1 >> 16) == 0x3C00 + ((0x4152) >> 0):  # wrong; do explicit
        break
# explicit: opcode 15 (addis): bits: 15 D(5) A(5) imm16
import re
def find_addis_imm(imm_hi):
    res=[]
    target=(15<<26)|(imm_hi&0xFFFF)
    for off in range(0, len(rom)-8, 4):
        w=struct.unpack_from(">I", rom, off)[0]
        if (w & 0xFC00FFFF)==(target & 0xFC00FFFF) and (w&0xFFFF)==imm_hi:
            # paired ori A,D,lo within next 6 instrs?
            d=(w>>21)&31
            a=(w>>16)&31
            for k in range(1,7):
                w2=struct.unpack_from(">I", rom, off+4*k)[0]
                if (w2>>26)==24 and ((w2>>16)&31)==a and (w2>>21)==a and (w2&0xFFFF)==0x4541:
                    res.append(BASE+off)
                    break
    return res
hits = find_addis_imm(0x4152)
print("AREA-construct sites:", [f"{h:08X}" for h in hits[:20]], "total", len(hits))

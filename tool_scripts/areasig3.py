import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
BASE = 0x40800000
# any instruction pair storing 0x41524541 via lis+ori (any regs, any order, within 8 instrs)
lis_hits=[]
for off in range(0, len(rom)-32, 4):
    w=struct.unpack_from(">I", rom, off)[0]
    if (w>>26)!=15: continue
    if (w&0xFFFF)!=0x4152: continue
    d=(w>>21)&31; a=(w>>16)&31
    if a!=0: continue
    for k in range(1,9):
        w2=struct.unpack_from(">I", rom, off+4*k)[0]
        if (w2>>26)==24 and ((w2>>16)&31)==d and ((w2>>21)&31)==d and (w2&0xFFFF)==0x4541:
            lis_hits.append((BASE+off, BASE+off+4*k))
            break
print("sites:", [(f"{a:08X}",f"{b:08X}") for a,b in lis_hits[:10]], "total",len(lis_hits))

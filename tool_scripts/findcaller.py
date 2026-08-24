import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
target = 0x40B6E8B0
hits=[]
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    op = w >> 26
    if op == 18:  # b/bl
        li = w & 0x03FFFFFC
        if li & 0x02000000: li -= 0x04000000
        if base + i + li == target:
            hits.append((base+i, w))
print("callers of 0x40B6E8B0:", [(hex(a), hex(w)) for a,w in hits])

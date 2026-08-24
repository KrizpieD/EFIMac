import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
hits=[]
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    ra = (w>>16)&0x1F
    if ra == 31 and (w & 0xFFFF) == 0x0818:
        hits.append((base+i,w))
print("any-op r31+0x818:", len(hits))
for a,w in hits:
    print(f"  0x{a:x}: {w:08x}")
# also look for addis/addi building 0x818 constant
hits=[]
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    op=w>>26
    if op in (12,13,14,15,24,25) and (w&0xFFFF)==0x0818:
        hits.append((base+i,w))
print("imm 0x818 ops:", len(hits))
for a,w in hits[:20]:
    print(f"  0x{a:x}: {w:08x}")

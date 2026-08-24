import capstone, struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
# scan all words; find stw-family storing to disp 0x810-0x820
hits=[]
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    op = w >> 26
    if op in (36,37):  # stw / stwu
        d = w & 0xFFFF
        if 0x810 <= d <= 0x820:
            hits.append((base+i,w))
print("stw hits:", len(hits))
for a,w in hits[:24]:
    print(f"  0x{a:x}: {w:08x}")
# also D-form loads of 0x818 with any base (context may use another reg)
hits2=[]
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    op = w >> 26
    if op in (32,33):  # lwz/lwzu
        d = w & 0xFFFF
        ra = (w>>16)&0x1F
        if d == 0x818 and ra != 31:
            hits2.append((base+i,w))
print("lwz 0x818 non-r31:", len(hits2))
for a,w in hits2[:12]:
    print(f"  0x{a:x}: {w:08x}")

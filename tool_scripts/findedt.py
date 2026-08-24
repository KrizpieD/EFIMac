import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
n=len(rom)
# scan every offset (step 4) where w[i+0x818] nonzero and w[i+0x814]/w[i+0x81c] also pointer-like
hits=[]
for i in range(0, n-0x900, 4):
    v18 = struct.unpack(">I", rom[i+0x818:i+0x81c])[0]
    if v18 == 0: continue
    v14 = struct.unpack(">I", rom[i+0x814:i+0x818])[0]
    v1c = struct.unpack(">I", rom[i+0x81c:i+0x820])[0]
    vals=[v for v in (v14,v18,v1c) if v]
    # require at least 2 nonzero, all similar high pattern
    if len(vals)>=2:
        hi = set(v>>16 for v in vals)
        if len(hi)<=2 and any(0xF000<=h<=0xFFFF or 0x4000<=h<=0x6900 for h in hi):
            hits.append((i,v14,v18,v1c))
print(f"{len(hits)} candidate templates")
for i,a,b,c in hits[:20]:
    print(f"  @0x{i:x} (+0x{(i)%0x1000:x} page 0x{i&~0xFFF:x}): 814={a:08x} 818={b:08x} 81c={c:08x}")

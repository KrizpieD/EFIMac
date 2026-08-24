import sys
rom_path = sys.argv[1]
out_path = sys.argv[2]
data = open(rom_path, 'rb').read()

def be32(p): return int.from_bytes(data[p:p+4], 'big')

# CHRP descriptor constants (documented in bootloader_impl.c)
parcels_offset = 0x1BFC0
parcels_size = 0x259C8C
print("parcels offset/size:", hex(parcels_offset), hex(parcels_size))
par = data[parcels_offset:parcels_offset+parcels_size]
assert par[0:4] == b'prcl', "not prcl"
P = parcels_offset
base = 0x14
lz_off = lz_sz = None
while True:
    next_o = be32(P + base)
    typ = be32(P + base + 4)
    loff = be32(P + base + 8)
    print(f"  parcel base=0x{base:05X} next=0x{next_o:05X} type=0x{typ:08X} ({typ.to_bytes(4,'big')}) lzoff=0x{loff:05X}")
    if typ == 0x726F6D20:  # 'rom '
        lz_off = loff; lz_sz = next_o - (base + loff)
        print("  rom parcel LZSS at", hex(base+loff), "size", lz_sz)
        break
    if next_o == 0 or next_o <= base: break
    base = next_o

src = par[base+lz_off : base+lz_off+lz_sz]
dict_ = bytearray(0x1000)
RunMask = 0
Remaining = len(src)
sp = 0
out = bytearray()
DictIdx = 0xFEE
while Remaining >= 0:
    if RunMask < 0x100:
        if Remaining <= 0: break
        RunMask = src[sp] | 0xFF00; sp += 1; Remaining -= 1
    if RunMask & 1:
        if Remaining <= 0: break
        c = src[sp]; sp += 1; Remaining -= 1
        dict_[DictIdx & 0xFFF] = c; out.append(c); DictIdx = (DictIdx+1) & 0xFFF
    else:
        if Remaining <= 0: break
        idx = src[sp]; sp += 1; Remaining -= 1
        if Remaining <= 0: break
        cnt = src[sp]; sp += 1; Remaining -= 1
        start = idx | ((cnt << 4) & 0xF00)
        n = (cnt & 0x0F) + 3
        for _ in range(n):
            c = dict_[start & 0xFFF]
            dict_[DictIdx & 0xFFF] = c; out.append(c)
            start = (start+1) & 0xFFF; DictIdx = (DictIdx+1) & 0xFFF
    RunMask >>= 1

print("decompressed size:", len(out))
open(out_path, 'wb').write(out)

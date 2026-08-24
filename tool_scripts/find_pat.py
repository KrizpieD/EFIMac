import struct

iso = r"C:\Users\clayc\AppData\Local\Temp\opencode\mac\Mac OS 9.2.2.iso"
data = open(iso, "rb").read()
base = 0x2424600
parcels_offset = 0x01BFC0
parcels_size = 0x259C8C
p = data[base + parcels_offset: base + parcels_offset + parcels_size]

def be32(o):
    return struct.unpack(">I", p[o:o + 4])[0]

off = 0x14
while off != 0:
    nxt = be32(off)
    typ = be32(off + 4)
    lz = be32(off + 8)
    if typ == 0x726F6D20:
        break
    off = nxt

lzss = p[off + lz:nxt]
dict_buf = bytearray(0x1000)
runmask = 0
remaining = len(lzss)
si = 0
dict_idx = 0xFEE
out = bytearray()
while remaining >= 0:
    if runmask < 0x100:
        remaining -= 1
        if remaining < 0:
            break
        runmask = lzss[si] | 0xFF00
        si += 1
    if runmask & 1:
        remaining -= 1
        if remaining < 0:
            break
        val = lzss[si]
        si += 1
        out.append(val)
        dict_buf[dict_idx] = val
        dict_idx = (dict_idx + 1) & 0xFFF
    else:
        remaining -= 2
        if remaining < 0:
            break
        offs = ((lzss[si] << 4) | (lzss[si + 1] >> 4))
        ln = (lzss[si + 1] & 0xF) + 3
        si += 2
        for _ in range(ln):
            val = dict_buf[offs]
            dict_buf[dict_idx] = val
            dict_idx = (dict_idx + 1) & 0xFFF
            out.append(val)
    runmask >>= 1

rom = bytes(out)
print("rom size 0x%x" % len(rom))
for pat, name in [
    (bytes.fromhex("8BDC0002"), "lbz r30,2(r28)"),
    (bytes.fromhex("4D9E0020"), "bclr+ 4D9E0020"),
    (bytes.fromhex("73DE0004"), "andi 73DE0004"),
    (bytes.fromhex("7C0006AC"), "eieio 7C0006AC"),
]:
    idx = 0
    found = []
    while True:
        i = rom.find(pat, idx)
        if i < 0:
            break
        found.append(i)
        idx = i + 1
    print("%s: %s" % (name, [hex(f) for f in found[:12]]))

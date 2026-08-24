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
target = None
while off != 0:
    nxt = be32(off)
    typ = be32(off + 4)
    lz = be32(off + 8)
    if typ == 0x726F6D20:
        target = (off, nxt, lz)
        break
    off = nxt

off, nxt, lz = target
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
        c = lzss[si]
        si += 1
        dict_buf[dict_idx & 0xFFF] = c
        out.append(c)
        dict_idx = (dict_idx + 1) & 0xFFF
    else:
        remaining -= 1
        if remaining < 0:
            break
        idx = lzss[si]
        si += 1
        remaining -= 1
        if remaining < 0:
            break
        cnt = lzss[si]
        si += 1
        start = idx | ((cnt << 4) & 0xF00)
        n = (cnt & 0x0F) + 3
        for _ in range(n):
            c = dict_buf[start & 0xFFF]
            dict_buf[dict_idx & 0xFFF] = c
            out.append(c)
            start = (start + 1) & 0xFFF
            dict_idx = (dict_idx + 1) & 0xFFF
    runmask >>= 1

print("decompressed rom parcel: %d bytes (0x%X)" % (len(out), len(out)))

seq = bytes([
    0x83, 0xC1, 0xFB, 0xFC, 0x9B, 0xBE, 0x00, 0x00, 0x3B, 0xDE, 0x00, 0x01,
    0x73, 0xDD, 0x0F, 0xFF, 0x93, 0xC1, 0xFB, 0xFC, 0x4C, 0x82, 0x00, 0x20,
])
idx = out.find(seq)
print("executed tail-call loop sequence (8 instr, 24 bytes) found at offset:",
      ("0x%X" % idx) if idx >= 0 else "NOT FOUND")

seq2 = bytes([
    0x48, 0x00, 0x07, 0x05, 0x8F, 0xA8, 0x00, 0x01, 0x2C, 0x1D, 0x00, 0x00,
    0x41, 0x82, 0x00, 0xD4, 0x2C, 0x1D, 0x00, 0x0A, 0x41, 0x82, 0x00, 0x74,
])
idx2 = out.find(seq2)
print("string-parser entry sequence found at offset:", ("0x%X" % idx2) if idx2 >= 0 else "NOT FOUND")

seq3 = bytes([0x4E, 0x80, 0x00, 0x20, 0xB8, 0x41, 0xFC, 0x18])
idx3 = out.find(seq3)
print("blr+lmw sequence found at offset:", ("0x%X" % idx3) if idx3 >= 0 else "NOT FOUND")

for label, seqb in [("tail-loop", seq), ("parser", seq2)]:
    pos = 0
    hits = []
    while True:
        pos = out.find(seqb, pos)
        if pos < 0:
            break
        hits.append(pos)
        pos += 4
    print("%s: %d occurrence(s): %s" % (label, len(hits), [hex(h) for h in hits]))

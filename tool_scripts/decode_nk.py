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


def word(a):
    return struct.unpack(">I", out[a:a + 4])[0]


NAMES31 = {19: "mfcr", 83: "mfspr", 144: "mtcrf", 146: "mtspr", 120: "mfmsr", 210: "mtsr", 339: "mfspr", 467: "mtspr",
           595: "mfsr", 625: "mtsr", 60: "andc", 266: "add", 40: "subf", 444: "or", 28: "and", 124: "nor",
           20: "lwarx", 23: "lwzx", 151: "stwx", 87: "lbzx", 279: "lhzx", 339: "mfspr"}
NAMES19 = {0: "mcrf", 16: "bclr", 33: "crnor", 50: "rfi", 129: "crandc", 150: "isync", 193: "crxor",
           225: "crnand", 257: "crand", 289: "creqv", 417: "crorc", 449: "cror", 528: "bctr"}

for region, start, count in [("0x340000", 0x340000, 24), ("0x320000", 0x320000, 16), ("0x310000", 0x310000, 16)]:
    print("=== region +%s ===" % region)
    for a in range(start, start + count * 4, 4):
        w = word(a)
        op = w >> 26
        nm = "op%d" % op
        if op == 31:
            nm = NAMES31.get((w >> 1) & 0x3FF, "X?%d" % ((w >> 1) & 0x3FF))
        elif op == 19:
            nm = NAMES19.get((w >> 1) & 0x3FF, "XL?%d" % ((w >> 1) & 0x3FF))
        elif op == 14:
            nm = "addi"
        elif op == 18:
            nm = "b"
        elif op == 16:
            nm = "bc"
        elif op == 11:
            nm = "cmpli"
        elif op == 10:
            nm = "cmplw"
        elif op == 15:
            nm = "lis"
        elif op == 24:
            nm = "ori"
        elif op == 28:
            nm = "and"
        elif op == 32:
            nm = "lwz"
        elif op == 36:
            nm = "stw"
        elif op == 40:
            nm = "lhz"
        elif op == 44:
            nm = "sth"
        print("+0x%06X: 0x%08X  %s" % (a, w, nm))
    print()

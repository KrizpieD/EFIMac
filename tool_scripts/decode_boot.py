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


def signed16(v):
    return v - 0x10000 if v & 0x8000 else v


def decode(a):
    w = word(a)
    op = w >> 26
    lines = []
    if op == 18:  # b
        li = signed16((w >> 2) & 0xFFFF) << 2
        tgt = a + li if not (w & 2) else li
        lines.append("b +0x%X -> 0x%08X" % (li & 0xFFFFFFFF, (0x310000 + a) + li if w & 2 == 0 else li))
    return lines


for a in range(0x310000, 0x310060, 4):
    w = word(a)
    op = w >> 26
    if op == 18:
        li = signed16((w >> 2) & 0xFFFF) << 2
        tgt = (0x310000 + a) + li
        print("+0x%06X: 0x%08X  b +0x%X (->0x%08X)  AA=%d LK=%d" % (a, w, li & 0xFFFFFFFF, tgt, (w >> 1) & 1, w & 1))
    elif op == 19:
        xo = (w >> 1) & 0x3FF
        name = {0: "mcrf", 16: "bclr", 33: "crnor", 50: "rfi", 129: "crandc", 150: "isync", 193: "crxor", 225: "crnand", 257: "crand", 289: "creqv", 417: "crorc", 449: "cror", 528: "bctr"}.get(xo, "XL?%d" % xo)
        print("+0x%06X: 0x%08X  %s" % (a, w, name))
    elif op == 31:
        xo = (w >> 1) & 0x3FF
        name = {83: "mfspr", 146: "mtspr", 19: "mfcr", 144: "mtcrf", 120: "mfmsr", 146: "mtspr", 60: "andc"}.get(xo, "X?%d" % xo)
        print("+0x%06X: 0x%08X  %s" % (a, w, name))
    elif op == 14:
        print("+0x%06X: 0x%08X  addi" % (a, w))
    elif op == 21:
        print("+0x%06X: 0x%08X  rlwinm" % (a, w))
    elif op == 16:
        bo, bi = (w >> 21) & 0x1F, (w >> 16) & 0x1F
        bd = signed16((w >> 2) & 0xFFFF) << 2
        print("+0x%06X: 0x%08X  bc BO=%d BI=%d +0x%X (->0x%08X)" % (a, w, bo, bi, bd & 0xFFFFFFFF, (0x310000 + a) + bd))
    else:
        print("+0x%06X: 0x%08X  op%d" % (a, w, op))
print()
for a in range(0x322060, 0x3220A0, 4):
    w = word(a)
    op = w >> 26
    xo = (w >> 1) & 0x3FF
    nm = ""
    if op == 19:
        nm = {16: "bclr", 50: "rfi", 150: "isync", 193: "crxor", 528: "bctr"}.get(xo, "XL%d" % xo)
    elif op == 31:
        nm = {83: "mfspr", 146: "mtspr", 19: "mfcr", 144: "mtcrf", 120: "mfmsr", 60: "andc", 266: "add", 40: "subf", 444: "or", 28: "and", 124: "nor", 20: "lwarx", 23: "lwzx", 151: "stwx"}.get(xo, "X%d" % xo)
    elif op == 14:
        nm = "addi"
    elif op == 32:
        nm = "lwz"
    elif op == 36:
        nm = "stw"
    elif op == 18:
        nm = "b"
    print("+0x%06X: 0x%08X  %s" % (a, w, nm))

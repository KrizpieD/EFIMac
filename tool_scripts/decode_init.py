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


X31 = {19: "mfcr", 83: "mfspr", 144: "mtcrf", 146: "mtspr", 120: "mfmsr", 210: "mtsr",
       339: "mfspr", 467: "mtspr", 595: "mfsr", 625: "mtsr", 60: "andc", 266: "add",
       40: "subf", 444: "or", 28: "and", 124: "nor", 23: "lwzx", 151: "stwx",
       87: "lbzx", 279: "lhzx", 55: "lwzux", 339: "mfspr", 302: "doz", 313: "mullw",
       234: "addc", 329: "addme", 459: "rlwimi", 536: "addmef", 233: "mulli", 273: "mulhw",
       75: "mulhwu", 266: "add", 779: "addze", 491: "mr", 113: "mr"}
X19 = {0: "mcrf", 16: "bclr", 33: "crnor", 50: "rfi", 129: "crandc", 150: "isync",
       193: "crxor", 225: "crnand", 257: "crand", 289: "creqv", 417: "crorc", 449: "cror",
       528: "bctr"}


def disasm(w, a):
    op = w >> 26
    if op == 31:
        return X31.get((w >> 1) & 0x3FF, "X%d" % ((w >> 1) & 0x3FF))
    if op == 19:
        return X19.get((w >> 1) & 0x3FF, "XL%d" % ((w >> 1) & 0x3FF))
    names = {14: "addi", 12: "addic", 13: "addic.", 18: "b", 16: "bc", 11: "cmpli",
             10: "cmplw", 15: "lis", 24: "ori", 28: "and", 32: "lwz", 36: "stw",
             40: "lhz", 44: "sth", 46: "stmw", 34: "lbz", 38: "stb", 41: "lha",
             2: "cmpi", 48: "lfd", 52: "stfd", 33: "lbzu", 25: "oril", 26: "oris",
             20: "lwzu", 6: "lwzu", 39: "stbu", 47: "stm", 45: "sthu", 56: "lfs",
             21: "lbzu", 35: "lbzu", 57: "lfsu"}
    return names.get(op, "op%d" % op)


for start, count in [(0x310060, 40), (0x310080, 24)]:
    print("=== 0x%06X ===" % start)
    for a in range(start, start + count * 4, 4):
        w = word(a)
        print("0x%06X: 0x%08X  %s" % (a, w, disasm(w, a)))
    print()

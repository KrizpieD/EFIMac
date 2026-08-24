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


def spr(w):
    return (((w >> 16) & 0x1F) | (((w >> 11) & 0x1F) << 5))


SPR_NAMES = {1: "xer", 8: "lr", 9: "ctr", 18: "dsisr", 19: "dar", 26: "srr0", 27: "srr1",
             22: "dec", 25: "sdr1", 528: "ibr", 530: "ibr", 536: "dbr", 538: "dbr",
             272: "sprg4", 273: "sprg5", 274: "sprg6", 275: "sprg7", 287: "pvr"}


def crf(bi):
    return "cr%d,%d" % (bi >> 2, bi & 3)


BO = {0: "ge", 1: "le", 2: "ne", 3: "eq", 4: "gt", 5: "lt", 6: "so", 7: "ns",
      8: "ge", 9: "le", 10: "ne", 11: "eq", 12: "gt", 13: "lt", 14: "so", 15: "ns"}


def disasm(w, a):
    op = w >> 26
    rD = (w >> 21) & 0x1F
    rA = (w >> 16) & 0x1F
    rB = (w >> 11) & 0x1F
    imm = w & 0xFFFF
    simm = imm if imm < 0x8000 else imm - 0x10000
    xo = (w >> 1) & 0x3FF
    if op == 18:
        li = (w & 0x03FFFFFC) >> 2
        if li >= 0x200000:
            li -= 0x400000
        tgt = (a + (li << 2)) if not (w & 2) else (li << 2)
        return ("b%s" % ("l" if w & 1 else "")) + " -> 0x%08x" % tgt
    if op == 16:
        bo = (w >> 21) & 0x1F
        bi = (w >> 16) & 0x1F
        bd = (w & 0xFFFC) if (w & 0xFFFC) < 0x8000 else (w & 0xFFFC) - 0x10000
        tgt = a + bd if not (w & 2) else bd
        nm = ""
        if (bo & 0x10) == 0:
            nm = "bdnz" if (bo & 0x0C) == 0 else "bdz"
        else:
            nm = "b%s" % BO.get(bo & 0xF, "?")
        return nm + ("l" if w & 1 else "") + " crf," + crf(bi) + " -> 0x%08x" % tgt
    if op == 19:
        nm = {16: "bclr", 528: "bctr", 50: "rfi", 150: "isync"}.get(xo, "X19_%d" % xo)
        if xo in (16, 528):
            nm += "+"
        return nm
    if op == 31:
        nm = {19: "mfcr", 83: "mfspr", 144: "mtcrf", 146: "mtspr", 120: "mfmsr",
              339: "mfspr", 467: "mtspr", 595: "mfsr", 625: "mtsr", 60: "andc",
              266: "add", 40: "subf", 444: "or", 28: "and", 124: "nor", 23: "lwzx",
              151: "stwx", 87: "lbzx", 279: "lhzx", 55: "lwzux", 75: "mulhwu",
              234: "addc", 313: "mullw", 233: "mulli", 491: "mr", 113: "mr",
              412: "orc", 476: "nand", 122: "cntlzw", 779: "addze", 536: "addmef",
              329: "addme", 302: "doz", 459: "rlwimi", 171: "rlwinm", 84: "lwbrx"}.get(xo)
        if nm is None:
            return "X_%d" % xo
        if nm == "mfcr":
            return "mfcr r%d" % rD
        if nm == "mtspr":
            return "mtspr %s, r%d" % (SPR_NAMES.get(spr(w), spr(w)), rD)
        if nm == "mfspr":
            return "mfspr r%d, %s" % (rD, SPR_NAMES.get(spr(w), spr(w)))
        if nm == "mtcrf":
            return "mtcrf 0x%x" % ((w >> 12) & 0xFF)
        if nm in ("add", "subf", "andc", "or", "and", "nor", "addc", "mullw"):
            return "%s r%d,r%d,r%d" % (nm, rD, rA, rB)
        if nm == "mr":
            return "mr r%d,r%d" % (rD, rB)
        if nm in ("lwzx", "stwx", "lbzx", "lhzx"):
            return "%s r%d,r%d(r%d)" % (nm, rD, rB, rA)
        if nm in ("mfsr",):
            return "mfsr r%d,%d" % (rD, rA)
        return nm + " r%d,r%d,r%d" % (rD, rA, rB)
    if op == 14:
        return "addi r%d,r%d,0x%x" % (rD, rA, simm & 0xFFFFFFFF)
    if op == 13:
        return "addic. r%d,r%d,0x%x" % (rD, rA, simm & 0xFFFFFFFF)
    if op == 15:
        return "lis r%d,0x%x" % (rD, imm)
    if op == 24:
        return "ori r%d,r%d,0x%x" % (rD, rA, imm)
    if op == 26:
        return "oris r%d,r%d,0x%x" % (rD, rA, imm)
    if op == 32:
        return "lwz r%d,0x%x(r%d)" % (rD, simm & 0xFFFFFFFF, rA)
    if op == 36:
        return "stw r%d,0x%x(r%d)" % (rD, simm & 0xFFFFFFFF, rA)
    if op == 34:
        return "lbz r%d,0x%x(r%d)" % (rD, simm & 0xFFFFFFFF, rA)
    if op == 38:
        return "stb r%d,0x%x(r%d)" % (rD, simm & 0xFFFFFFFF, rA)
    if op == 40:
        return "lhz r%d,0x%x(r%d)" % (rD, simm & 0xFFFFFFFF, rA)
    if op == 44:
        return "sth r%d,0x%x(r%d)" % (rD, simm & 0xFFFFFFFF, rA)
    if op == 33:
        return "lbzu r%d,1(r%d)" % (rD, rA)
    if op == 46:
        return "stmw r%d,0x%x(r%d)" % (rD, simm & 0xFFFFFFFF, rA)
    if op == 2:
        return "cmpi %d,r%d,0x%x" % (rA, rD, simm & 0xFFFFFFFF)
    if op == 11:
        return "cmpli %d,r%d,0x%x" % (rA, rD, imm)
    if op == 10:
        return "cmplw %d,r%d,r%d" % (rA, rD, rB)
    if op == 28:
        return "and r%d,r%d,r%d" % (rD, rA, rB)
    if op == 20:
        return "lwzu r%d,0x%x(r%d)" % (rD, simm & 0xFFFFFFFF, rA)
    if op == 21:
        return "lbzu r%d,0x%x(r%d)" % (rD, simm & 0xFFFFFFFF, rA)
    return "op%d" % op


for start, count in [(0x326440, 220)]:
    print('=== 0x%06X ===' % start)
    for a in range(start, start + count * 4, 4):
        w = word(a)
        print('0x%06X: 0x%08X  %s' % (a, w, disasm(w, a)))
    print()


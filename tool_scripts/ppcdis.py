import re, sys

R = [f"r{i}" for i in range(32)]
F = [f"f{i}" for i in range(32)]
CRF = [f"cr{i}" for i in range(8)]

def s16(x): return x - 0x10000 if x & 0x8000 else x
def s14(x): return x - 0x4000 if x & 0x2000 else x

def dform(w, op):
    rt, ra, d = (w >> 21) & 31, (w >> 16) & 31, s16(w & 0xFFFF)
    def base():
        return f", {ra}(r1)" if False else ("0" if ra == 0 else R[ra])
    return base()

XO31 = {
    0: "cmpw", 32: "cmplw", 33: "cmplwi", 266: "add", 234: "addc", 138: "adde",
    40: "subf", 136: "subfc", 200: "subfe", 266: "add", 10: "mcrf", 19: "mfcr",
    144: "mtcrf", 339: "mfspr", 467: "mtspr", 536: "srw", 792: "sraw", 824: "srawi",
    28: "and", 60: "andc", 444: "or", 476: "nor", 124: "orc", 316: "xor", 26: "cntlzw",
    597: "lswi", 531: "lswx", 919: "stswi", 727: "stswx", 491: "mftb",
    87: "lwarx", 150: "stwcx.", 193: "lbzx", 23: "lwzx", 87: "lwarx", 279: "lhzx",
    103: "lhax", 343: "lhbrx", 531: "lswx", 791: "stbx", 663: "stwbrx", 823: "sthx",
    695: "stbux", 183: "lwzu", 183: "lwzux", 407: "stwx", 247: "stwx", 439: "sthbrx",
    127: "lwbrx", 373: "lwbrx", 21: "slw", 449: "srw", 282: "slw", 20: "rlw?",
    233: "eieio", 854: "eieio", 884: "lwsync", 598: "sync", 983: "icbi", 982: "icbt",
    953: "dcbst", 470: "dcbi", 278: "dcbt", 86: "dcbf", 182: "dcbtst", 64: "adde",
}

def dec31(w):
    xo = (w >> 1) & 0x3FF
    ra = (w >> 16) & 31
    rb = (w >> 11) & 31
    rt = (w >> 21) & 31
    rc = w & 1
    oe = (w >> 10) & 1
    xt = (w >> 21) & 31
    xra = (w >> 16) & 31
    xrb = (w >> 11) & 31
    sh = (w >> 11) & 31
    # special: XO>=600 are special ops (icbi, dcbz...)
    if xo == 854: return "eieio"
    if xo == 884: return "lwsync"
    if xo == 598: return "sync"
    if xo == 982: return "icbt"
    if xo == 983: return "icbi"
    if xo == 953: return f"dcbst {R[ra]}, {R[rb]}" if ra != 0 or True else f"dcbst 0, {R[rb]}"
    if xo == 470: return f"dcbi {R[ra]}, {R[rb]}"
    if xo == 278: return f"dcbt {R[ra]}, {R[rb]}"
    if xo == 182: return f"dcbtst {R[ra]}, {R[rb]}"
    if xo == 86:  return f"dcbf {R[ra]}, {R[rb]}"
    if xo == 467:
        spr = ((w >> 16) & 31) | (((w >> 11) & 31) << 5)
        spr = ((spr & 0x1F) << 5) | ((spr >> 5) & 0x1F)
        return f"mtspr {spr}, {R[rt]}"
    if xo == 339:
        spr = ((w >> 16) & 31) | (((w >> 11) & 31) << 5)
        spr = ((spr & 0x1F) << 5) | ((spr >> 5) & 0x1F)
        return f"mfspr {R[rt]}, {spr}"
    if xo == 144 or xo == 146:
        return f"mtcrf 0x{w>>12&0xFF:x}, {R[rt]}"
    if xo == 19: return "mfcr"
    if xo == 10: return f"mcrf {CRF[(w>>23)&7]}, {CRF[(w>>18)&7]}"
    names = {
        0: "cmpw", 32: "cmplw", 33: "cmplwi",
        266: "add", 234: "addc", 138: "adde", 40: "subf", 136: "subfc", 200: "subfe",
        28: "and", 60: "andc", 444: "or", 476: "nor", 124: "orc", 316: "xor", 26: "cntlzw",
        536: "srw", 792: "sraw", 824: "srawi", 21: "slw", 449: "srw", 282: "slw",
        193: "lbzx", 23: "lwzx", 279: "lhzx", 103: "lhax", 343: "lhbrx", 215: "stbx",
        662: "stwbrx", 407: "stwx", 151: "stwx", 695: "stbux", 127: "lwbrx", 55: "lwzux",
        534: "lwbrx", 918: "sthbrx",
        87: "lwarx", 150: "stwcx.", 20: "rlwnm", 536: "srw",
    }
    n = names.get(xo)
    if n is None:
        # fallback guesses
        if xo == 87: n = "lwarx"
        elif xo == 150: n = "stwcx."
        else:
            return f"<xo{xo}>"
    if n == "cmpw" or n == "cmplw":
        cr = (w >> 23) & 7
        return f"{n} cr{cr}, {R[ra]}, {R[rb]}"
    if n == "cmplwi":
        cr = (w >> 23) & 7
        return f"{n} cr{cr}, {R[ra]}, {w&0xFFFF}"
    if n == "srawi":
        return f"{n} {R[ra]}, {R[rt]}, {sh}" + ("." if rc else "")
    if n in ("slw","srw","sraw"):
        return f"{n} {R[ra]}, {R[rt]}, {R[rb]}" + ("." if rc else "")
    if n in ("rlwnm",):
        mb = (w >> 6) & 31; me = (w >> 1) & 31
        return f"rlwnm {R[ra]}, {R[rt]}, {R[rb]}, {mb}, {me}" + ("." if rc else "")
    if n in ("lwarx",):
        return f"{n} {R[rt]}, 0, {R[rb]}"
    if n in ("stwcx.",):
        return f"{n} {R[rt]}, 0, {R[rb]}"
    if n in ("lwzux","lwzx","lbzx","lhzx","lhax","lhbrx"):
        base = R[ra] if ra != 0 else "0"
        return f"{n} {R[rt]}, {base}, {R[rb]}"
    if n in ("stwx","stbx","stwbrx","stbux","sthbrx","lwbrx"):
        base = R[ra] if ra != 0 else "0"
        return f"{n} {R[rt]}, {base}, {R[rb]}"
    if n == "cntlzw":
        return f"{n} {R[ra]}, {R[rt]}" + ("." if rc else "")
    if n in ("add","addc","adde","subf","subfc","subfe","and","andc","or","nor","orc","xor"):
        if n == "add": n = "add" if not oe else "addo"
        return f"{n} {R[ra]}, {R[rt]}, {R[rb]}" + ("." if rc else "")
    return f"{n} {R[ra]}, {R[rt]}, {R[rb]}" + ("." if rc else "")

def dec19(w):
    xo = (w >> 1) & 0x3FF
    if xo == 528 or xo == 560 or xo == 16:
        # bclr
        bo = (w >> 21) & 31; bi = (w >> 16) & 31
        lk = (w >> 0) & 1
        cond = condname(bo, bi)
        return f"bclr{'l' if lk else ''} {cond}"
    if xo == 32:
        bo = (w >> 21) & 31; bi = (w >> 16) & 31
        lk = (w >> 0) & 1
        return f"bcctr{'l' if lk else ''} {condname(bo, bi)}"
    if xo == 0:
        return f"mcrf {CRF[(w>>23)&7]}, {CRF[(w>>18)&7]}"
    if xo == 50:
        return "rfi"
    if xo == 258:
        return "sync"
    if xo == 18:
        # isync
        return "isync"
    if xo in (130,):
        return "mtcrf"
    if xo == 150:
        # creqv etc XL-form
        crd = (w >> 21) & 31; cra = (w >> 16) & 31; crb = (w >> 11) & 31
        return f"creqv {crd}, {cra}, {crb}"
    if xo == 258: return "sync"
    xl = {64:"crandc",129:"crand",193:"crxor",225:"crnand",257:"crand",289:"creqv",417:"crorc",449:"cror",33:"crnor",193:"crxor",449:"cror",289:"creqv",225:"crnand",129:"crand",417:"crorc",64:"crandc",33:"crnor"}
    if xo in xl:
        crd = (w >> 21) & 31; cra = (w >> 16) & 31; crb = (w >> 11) & 31
        return f"{xl[xo]} cr{crd}, cr{cra}, cr{crb}"
    return f"<xl{xo}>"

def condname(bo, bi):
    # very simplified
    return f"bo{bo} bi{bi}"

def dec16(w, pc):
    bo = (w >> 21) & 31; bi = (w >> 16) & 31
    bd = (w & 0xFFFC)
    if bd & 0x8000: bd -= 0x10000
    aa = (w >> 1) & 1
    lk = (w >> 0) & 1
    c = None
    if bo == 12: c = "b"      # branch always
    elif bo == 4: c = "bge"
    elif bo == 20: c = "bgt"
    elif bo == 0: c = "blt"
    elif bo == 14: c = "bnz"
    elif bo == 8: c = "bns"
    elif bo == 16: c = "beq"
    elif bo == 18: c = "bne"
    elif bo == 24: c = "bge"   # CR0 ge with LK? no
    elif bo == 10: c = "bnl"
    else: c = f"bo{bo}"
    tgt = bd if aa else (pc + bd)
    return f"{c}{'l' if lk else ''} {tgt:#x}"

def dec18(w, pc):
    li = (w & 0x03FFFFFC)
    if li & 0x2000000: li -= 0x4000000
    aa = (w >> 1) & 1
    lk = (w & 1)
    tgt = li if aa else (pc + li)
    return f"b{'l' if lk else ''} {tgt:#x}"

def dec20(w):
    rs = (w >> 21) & 31; ra = (w >> 16) & 31
    sh = (w >> 11) & 31; mb = (w >> 6) & 31; me = (w >> 1) & 31; rc = w & 1
    return f"rlwimi {R[ra]}, {R[rs]}, {sh}, {mb}, {me}" + ("." if rc else "")

def dec21(w):
    rs = (w >> 21) & 31; ra = (w >> 16) & 31
    sh = (w >> 11) & 31; mb = (w >> 6) & 31; me = (w >> 1) & 31; rc = w & 1
    return f"rlwinm {R[ra]}, {R[rs]}, {sh}, {mb}, {me}" + ("." if rc else "")

def dec23(w):
    rs = (w >> 21) & 31; ra = (w >> 16) & 31
    rb = (w >> 11) & 31; mb = (w >> 6) & 31; me = (w >> 1) & 31; rc = w & 1
    return f"rlwnm {R[ra]}, {R[rs]}, {R[rb]}, {mb}, {me}" + ("." if rc else "")

def dec_mem(w, name, upd=False):
    rt = (w >> 21) & 31; ra = (w >> 16) & 31; d = s16(w & 0xFFFF)
    if ra == 0:
        return f"{name} {R[rt]}, {d}"
    return f"{name} {R[rt]}, {d}({R[ra]})"

def dec1(w):
    # opcode 1 - special
    return "special"

def dis(w, pc):
    op = (w >> 26) & 0x3F
    if op == 18: return dec18(w, pc)
    if op == 16: return dec16(w, pc)
    if op == 19: return dec19(w)
    if op == 14: return dec14(w)
    if op == 15: return dec15(w)
    if op == 12: return f"addic {R[(w>>21)&31]}, {R[(w>>16)&31]}, {s16(w&0xFFFF)}"
    if op == 13: return f"addic. {R[(w>>21)&31]}, {R[(w>>16)&31]}, {s16(w&0xFFFF)}"
    if op == 10: return f"cmpli {CRF[(w>>23)&7]}, {R[(w>>16)&31]}, {w&0xFFFF}"
    if op == 11: return f"cmpi {CRF[(w>>23)&7]}, {R[(w>>16)&31]}, {s16(w&0xFFFF)}"
    if op == 24: return f"ori {R[(w>>21)&31]}, {R[(w>>16)&31]}, {w&0xFFFF}"
    if op == 25: return f"oris {R[(w>>21)&31]}, {R[(w>>16)&31]}, {w&0xFFFF}"
    if op == 26: return f"xori {R[(w>>21)&31]}, {R[(w>>16)&31]}, {w&0xFFFF}"
    if op == 27: return f"xoris {R[(w>>21)&31]}, {R[(w>>16)&31]}, {w&0xFFFF}"
    if op == 28: return f"andi. {R[(w>>21)&31]}, {R[(w>>16)&31]}, {w&0xFFFF}"
    if op == 29: return f"andis. {R[(w>>21)&31]}, {R[(w>>16)&31]}, {w&0xFFFF}"
    if op == 20: return dec20(w)
    if op == 21: return dec21(w)
    if op == 23: return dec23(w)
    if op == 32: return dec_mem(w, "lwz")
    if op == 33: return dec_mem(w, "lwzu")
    if op == 34: return dec_mem(w, "lbz")
    if op == 35: return dec_mem(w, "lbzu")
    if op == 40: return dec_mem(w, "lhz")
    if op == 41: return dec_mem(w, "lhzu")
    if op == 42: return dec_mem(w, "lha")
    if op == 43: return dec_mem(w, "lhau")
    if op == 36: return dec_mem(w, "stw")
    if op == 37: return dec_mem(w, "stwu")
    if op == 38: return dec_mem(w, "stb")
    if op == 39: return dec_mem(w, "stbu")
    if op == 44: return dec_mem(w, "sth")
    if op == 45: return dec_mem(w, "sthu")
    if op == 46: return dec_mem(w, "lmw")
    if op == 47: return dec_mem(w, "stmw")
    if op == 17: return f"sc"
    if op == 2: return f"tdi {s16(w&0xFFFF)}"  # trap immediate
    if op == 3: return f"twi {s16(w&0xFFFF)}"
    if op == 31: return dec31(w)
    if op == 1: return f"<op1 {w&0x3FFFFFF}>"
    # FP
    if 48 <= op <= 63:
        return f"fp-op{op}"
    if op == 4:
        return f"altivec-op4"
    return f"<op{op}>"

def dec14(w):
    rt = (w >> 21) & 31; ra = (w >> 16) & 31; d = s16(w & 0xFFFF)
    return f"addi {R[rt]}, {R[ra]}, {d}"
def dec15(w):
    rt = (w >> 21) & 31; ra = (w >> 16) & 31; d = (w & 0xFFFF)
    return f"addis {R[rt]}, {R[ra]}, {d}"

def main():
    p = r"C:\Users\clayc\AppData\Local\Temp\opencode\romlines.txt"
    mem = {}
    for line in open(p, encoding="utf-8"):
        m = re.match(r"\s*ROM\[0x([0-9A-Fa-f]+)\]\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})", line)
        if m:
            base = int(m.group(1), 16)
            for i, g in enumerate(m.groups()[1:]):
                mem[base + i*4] = int(g, 16)
    lo = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x40B26300
    hi = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x40B27600
    for a in range(lo, hi, 4):
        if a not in mem:
            continue
        w = mem[a]
        print(f"{a:08x}: {w:08x}  {dis(w, a)}")

if __name__ == "__main__":
    main()

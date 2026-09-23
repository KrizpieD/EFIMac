import sys

ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\flat_rom.bin"
ROM_BASE = 0x40800000
data = open(ROM, "rb").read()

def s16(x):
    x &= 0xFFFF
    return x - 0x10000 if x & 0x8000 else x

def s24(x):
    x &= 0x3FFFF
    return x - 0x40000 if x & 0x20000 else x

def dis(w, a):
    op = (w >> 26) & 0x3F
    rs = (w >> 21) & 0x1F
    rt = (w >> 16) & 0x1F
    ra = (w >> 11) & 0x1F
    rb = (w >> 6) & 0x1F
    if op == 14:
        return "addi r%d,r%d,0x%x" % (rt, rs if rs else 0, s16(w))
    if op == 15:
        return "addis r%d,r%d,0x%x" % (rt, rs if rs else 0, s16(w))
    if op == 13:
        return "addic. r%d,r%d,0x%x" % (rt, rs, s16(w))
    if op == 12:
        return "addic r%d,r%d,0x%x" % (rt, rs, s16(w))
    if op == 24:
        return "ori r%d,r%d,0x%x" % (rs, rt, w & 0xFFFF)
    if op == 25:
        return "oris r%d,r%d,0x%x" % (rs, rt, w & 0xFFFF)
    if op == 26:
        return "xori r%d,r%d,0x%x" % (rs, rt, w & 0xFFFF)
    if op == 27:
        return "xoris r%d,r%d,0x%x" % (rs, rt, w & 0xFFFF)
    if op == 28:
        return "andi. r%d,r%d,0x%x" % (rs, rt, w & 0xFFFF)
    if op == 29:
        return "andis. r%d,r%d,0x%x" % (rs, rt, w & 0xFFFF)
    if op == 32:
        return "lwz r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 33:
        return "lwzu r%d,%d(r%d)" % (rt, s16(w), rs)
    if op == 34:
        return "lbz r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 35:
        return "lbzu r%d,%d(r%d)" % (rt, s16(w), rs)
    if op == 36:
        return "stw r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 37:
        return "stwu r%d,%d(r%d)" % (rt, s16(w), rs)
    if op == 38:
        return "stb r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 39:
        return "stbu r%d,%d(r%d)" % (rt, s16(w), rs)
    if op == 40:
        return "lhz r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 42:
        return "lha r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 43:
        return "lhau r%d,%d(r%d)" % (rt, s16(w), rs)
    if op == 44:
        return "sth r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 46:
        return "lmw r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 47:
        return "stmw r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 48:
        return "lfs r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 50:
        return "lfd r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 54:
        return "stfd r%d,%d(r%d)" % (rt, s16(w), rs if rs else 0)
    if op == 16:
        bo = (w >> 21) & 0x1F
        bi = (w >> 16) & 0x1F
        t = a + s16(w & 0xFFFC)
        lk = "l" if w & 1 else ""
        return "bc%s 0x%x,0x%x,0x%x" % (lk, bo, bi, t)
    if op == 18:
        t = a + s24(w & 0x3FFFFFC)
        lk = "l" if w & 1 else ""
        abs_ = "a" if w & 2 else ""
        return "b%s%s 0x%x" % (lk, abs_, t)
    if op == 17:
        sh = (w >> 11) & 0x1F
        mb = (w >> 6) & 0x1F
        me = (w >> 1) & 0x1F
        rc = "l" if w & 0x10 else ""
        lk = "l" if w & 1 else ""
        return "sc"
    if op == 20:
        sh = (w >> 11) & 0x1F
        mb = (w >> 6) & 0x1F
        me = (w >> 1) & 0x1F
        rc = "." if w & 1 else ""
        return "rlwimi r%d,r%d,0x%x,%d,%d%s" % (rt, rs, sh, mb, me, rc)
    if op == 21:
        sh = (w >> 11) & 0x1F
        mb = (w >> 6) & 0x1F
        me = (w >> 1) & 0x1F
        rc = "." if w & 1 else ""
        return "rlwinm r%d,r%d,0x%x,%d,%d%s" % (rt, rs, sh, mb, me, rc)
    if op == 23:
        sh = (w >> 11) & 0x1F
        mb = (w >> 6) & 0x1F
        me = (w >> 1) & 0x1F
        rc = "." if w & 1 else ""
        return "rlwnm r%d,r%d,r%d,%d,%d%s" % (rt, rs, ra, mb, me, rc)
    if op == 19:
        xo = (w >> 1) & 0x3FF
        lk = "l" if w & 1 else ""
        if xo == 16:
            bo = (w >> 21) & 0x1F
            bi = (w >> 16) & 0x1F
            return "bclr%s 0x%x,0x%x" % (lk, bo, bi)
        if xo == 528:
            bo = (w >> 21) & 0x1F
            bi = (w >> 16) & 0x1F
            return "bcctr%s 0x%x,0x%x" % (lk, bo, bi)
        if xo == 193:
            return "crxor %d,%d,%d" % (rt & 0x1C, (w >> 16) & 0x1C, (w >> 11) & 0x1C)
        if xo == 225:
            return "crnand %d,%d,%d" % (rt & 0x1C, (w >> 16) & 0x1C, (w >> 11) & 0x1C)
        if xo == 289:
            return "creqv %d,%d,%d" % (rt & 0x1C, (w >> 16) & 0x1C, (w >> 11) & 0x1C)
        if xo == 257:
            return "crand %d,%d,%d" % (rt & 0x1C, (w >> 16) & 0x1C, (w >> 11) & 0x1C)
        if xo == 129:
            return "crandc %d,%d,%d" % (rt & 0x1C, (w >> 16) & 0x1C, (w >> 11) & 0x1C)
        if xo == 417:
            return "crorc %d,%d,%d" % (rt & 0x1C, (w >> 16) & 0x1C, (w >> 11) & 0x1C)
        if xo == 33:
            return "crnor %d,%d,%d" % (rt & 0x1C, (w >> 16) & 0x1C, (w >> 11) & 0x1C)
        if xo == 449:
            return "cror %d,%d,%d" % (rt & 0x1C, (w >> 16) & 0x1C, (w >> 11) & 0x1C)
        if xo == 150:
            return "isync"
        if xo == 0:
            return "mcrf %d,%d" % ((w >> 21) & 0x1C, (w >> 16) & 0x1C)
        return "op19 xo=%d" % xo
    if op == 31:
        xo = (w >> 1) & 0x3FF
        rc = "." if w & 1 else ""
        oe = "o" if (w & 0x400) and xo in (8,10,40,136,138,266,520,522,650,778,1003) else ""
        if xo == 0:
            return "cmpw r%d,r%d,r%d" % (rt & 0x1C, rs, ra)
        if xo == 32:
            return "cmplw r%d,r%d,r%d" % (rt & 0x1C, rs, ra)
        if xo == 10:
            return "addc%s r%d,r%d,r%d" % (oe, rt, rs, ra)
        if xo == 8:
            return "subfc%s r%d,r%d,r%d" % (oe, rt, rs, ra)
        if xo == 40:
            return "subf%s r%d,r%d,r%d" % (oe, rt, rs, ra)
        if xo == 136:
            return "subfe%s r%d,r%d,r%d" % (oe, rt, rs, ra)
        if xo == 138:
            return "adde%s r%d,r%d,r%d" % (oe, rt, rs, ra)
        if xo == 266:
            return "add%s r%d,r%d,r%d" % (oe, rt, rs, ra)
        if xo == 104:
            return "neg%s r%d,r%d" % (oe, rt, rs)
        if xo == 24:
            return "slw r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 536:
            return "srw r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 792:
            return "sraw r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 824:
            sh = rb
            return "srawi r%d,r%d,0x%x%s" % (rt, rs, sh, rc)
        if xo == 28:
            return "and r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 60:
            return "andc r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 124:
            return "nor r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 444:
            return "or r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 412:
            return "orc r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 316:
            return "xor r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 284:
            return "eqv r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 476:
            return "nand r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 26:
            return "cntlzw r%d,r%d%s" % (rt, rs, rc)
        if xo == 235:
            return "mullw r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 75:
            return "mulhw r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 491:
            return "divw r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 459:
            return "divwu r%d,r%d,r%d%s" % (rt, rs, ra, rc)
        if xo == 922:
            return "extsh r%d,r%d%s" % (rt, rs, rc)
        if xo == 954:
            return "extsb r%d,r%d%s" % (rt, rs, rc)
        if xo == 23:
            return "lwzx r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 55:
            return "lwzux r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 87:
            return "lbzx r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 119:
            return "lbzux r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 279:
            return "lhzx r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 311:
            return "lhzux r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 343:
            return "lhax r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 375:
            return "lhaux r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 151:
            return "stwx r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 183:
            return "stwux r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 215:
            return "stbx r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 247:
            return "stbux r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 407:
            return "sthx r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 439:
            return "sthux r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 20:
            return "lwarx r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 150:
            return "stwcx. r%d,r%d,r%d" % (rt, rs, ra)
        if xo == 371:
            return "mftb r%d,r%d" % (rt, ra)
        if xo == 467:
            spr = rs | (rt << 5)
            return "mtspr 0x%x,r%d" % (spr, ra)
        if xo == 339:
            spr = rs | (rt << 5)
            return "mfspr r%d,0x%x" % (ra, spr)
        if xo == 19:
            return "mfcr r%d" % (rt)
        if xo == 144:
            return "mtcrf 0x%x,r%d" % ((w >> 12) & 0xFF, rt)
        if xo == 598:
            return "sync"
        if xo == 854:
            return "eieio"
        if xo == 86:
            return "dcbf r%d,r%d" % (rs, ra)
        if xo == 982:
            return "icbi r%d,r%d" % (rs, ra)
        if xo == 0x1C0:  # 448? no
            return "X31"
        return "X31 xo=%d%s" % (xo, rc)
    if op == 11:
        return "cmpli 0x%x,r%d,0x%x" % ((w >> 21) & 0x1C, rs, w & 0xFFFF)
    if op == 10:
        return "cmpi 0x%x,r%d,0x%x" % ((w >> 21) & 0x1C, rs, s16(w))
    if op == 17:
        sh = (w >> 11) & 0x1F
        return "sc"
    if op == 7:
        return "mulli r%d,r%d,0x%x" % (rt, rs, s16(w))
    if op == 8:
        return "subfic r%d,r%d,0x%x" % (rt, rs, s16(w))
    if op == 59 or op == 63:
        return "fp op%d" % op
    if op == 6:
        return "EMUL_EXT opcode"
    return "op%d?" % op

def main():
    start = int(sys.argv[1], 0)
    stop = int(sys.argv[2], 0)
    a = start
    while a < stop:
        w = int.from_bytes(data[a - ROM_BASE:a - ROM_BASE + 4], "big")
        print("0x%08X: 0x%08X  %s" % (a, w, dis(w, a)))
        a += 4

main()

import sys

def s32(x):
    x &= 0xFFFFFFFF
    return x - 0x100000000 if x & 0x80000000 else x

def s16(x): return s32(x & 0xFFFF)

SPR = {1:'XER',8:'LR',9:'CTR',26:'SRR0',27:'SRR1',272:'SPRG4',273:'SPRG5',
       280:'IBAT0U',281:'IBAT0L',282:'IBAT1U',283:'IBAT1L',284:'IBAT2U',
       285:'IBAT2L',286:'IBAT3U',287:'IBAT3L',528:'IBAT4U',529:'IBAT4L',
       530:'IBAT5U',531:'IBAT5L',532:'IBAT6U',533:'IBAT6L',534:'IBAT7U',
       535:'IBAT7L',536:'DBAT0U',537:'DBAT0L',538:'DBAT1U',539:'DBAT1L',
       540:'DBAT2U',541:'DBAT2L',542:'DBAT3U',543:'DBAT3L',560:'DBAT4U',
       561:'DBAT4L',562:'DBAT5U',563:'DBAT5L',564:'DBAT6U',565:'DBAT6L',
       566:'DBAT7U',567:'DBAT7L',0:'MQ',2:'TCR',4:'RTCL',5:'RTCR',
       10:'DEC',22:'SDR1',25:'SDR1(601?)',512:'HID0',1008:'PIR'}

XO = {0:'cmp',4:'tw',8:'subfc',10:'addc',11:'mulhwu',19:'mfcr',20:'lwarx',
      23:'lwzx',24:'slw',26:'cntlzw',28:'and',32:'cmpl',40:'subf',
      54:'dcbst',55:'lwzux',60:'andc',75:'mulhw',78:'tlbie',83:'mfmsr',
      86:'dcbf',87:'lbzx',104:'neg',106:'mtmsr',119:'lbzux',124:'nor',
      136:'subfe',138:'adde',144:'mtcrf',146:'mtmsr',150:'stwcx.',
      151:'stwx',183:'stwux',200:'subfze',202:'addze',215:'stbx',
      232:'subfme',234:'addme',235:'mullw',242:'mtsrin',246:'dcbtst',
      247:'stbux',266:'add',278:'dcbt',279:'lhzx',284:'eqv',306:'tlbie',
      311:'lhzux',316:'xor',339:'mfspr',343:'lhax',370:'tlbia',371:'mftb',
      375:'lhaux',407:'sthx',412:'orc',439:'sthux',444:'or',459:'divwu',
      467:'mtspr',470:'dcbi',476:'nand',491:'divw',512:'mcrxr',533:'lswx',
      534:'lwbrx',536:'srw',595:'mfsr',597:'lswi',598:'sync',566:'tlbsync',
      210:'mtsr',659:'mfsrin',661:'stswx',662:'stwbrx',725:'stswi',
      790:'lhbrx',792:'sraw',824:'srawi',854:'eieio',918:'sthbrx',
      982:'icbi',1014:'dcbz'}

def disasm(w, a):
    op = w >> 26
    r = []
    if op == 18:  # b / bl
        li = (w & 0x03FFFFFC) >> 2
        li = li - 0x800000 if li & 0x400000 else li
        aa = (w >> 1) & 1; lk = w & 1
        t = li*4 + (a if not aa else 0)
        return f"b{'l' if lk else ''} 0x{t:08X}"
    if op == 16:  # bc
        bo = (w >> 21) & 0x1F; bi = (w >> 16) & 0x1F
        bd = (w & 0xFFFC); bd = bd - 0x10000 if bd & 0x8000 else bd
        aa = (w >> 1) & 1; lk = w & 1
        t = bd + (a if not aa else 0)
        return f"bc {bo},{bi},0x{t:08X}"
    if op == 14:  # addi
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; im = s16(w)
        if ra == 0: return f"li r{rt},0x{im & 0xFFFFFFFF:X}"
        return f"addi r{rt},r{ra},0x{im & 0xFFFFFFFF:X}"
    if op == 15:  # addis
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; im = s16(w)
        if ra == 0: return f"lis r{rt},0x{im & 0xFFFF:X}"
        return f"addis r{rt},r{ra},0x{im & 0xFFFF:X}"
    if op == 13:  # addic
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; im = s16(w)
        return f"addic r{rt},r{ra},0x{im & 0xFFFFFFFF:X}"
    if op == 7:  # mulli
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; im = s16(w)
        return f"mulli r{rt},r{ra},0x{im & 0xFFFFFFFF:X}"
    if op == 11:  # cmpi / cmpli
        crf = (w >> 23) & 0x7; ra = (w >> 16) & 0x1F; im = s16(w)
        return f"cmpi cr{crf},r{ra},0x{im & 0xFFFFFFFF:X}"
    if op == 10:  # cmpli
        crf = (w >> 23) & 0x7; ra = (w >> 16) & 0x1F; im = s16(w)
        return f"cmpli cr{crf},r{ra},0x{im & 0xFFFF:X}"
    if op in (32, 33, 34, 35):  # lwz lwzu lbz lbzu
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; d = s16(w)
        names = {32:'lwz',33:'lwzu',34:'lbz',35:'lbzu'}
        s = f"r{rt},{d}" + (f"(r{ra})" if ra else "")
        return f"{names[op]} {s}"
    if op in (36, 37, 40, 41):  # stw stwu stb stbu
        rs = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; d = s16(w)
        names = {36:'stw',37:'stwu',40:'stb',41:'stbu'}
        s = f"r{rs},{d}" + (f"(r{ra})" if ra else "")
        return f"{names[op]} {s}"
    if op in (40, 42, 43, 44, 45, 46, 47):  # lhz lhzu lha lhau lwz... 
        pass
    if op in (42, 43):  # lhz lhzu
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; d = s16(w)
        return f"{'lhz' if op==42 else 'lhzu'} r{rt},{d}(r{ra})" if ra else f"{'lhz' if op==42 else 'lhzu'} r{rt},{d}"
    if op == 44:  # lha
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; d = s16(w)
        return f"lha r{rt},{d}(r{ra})" if ra else f"lha r{rt},{d}"
    if op in (48, 49, 50, 51):  # lfs lfsu lfd lfdu
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; d = s16(w)
        names = {48:'lfs',49:'lfsu',50:'lfd',51:'lfdu'}
        return f"{names[op]} f{rt},{d}(r{ra})" if ra else f"{names[op]} f{rt},{d}"
    if op in (52, 53, 54, 55):  # stfs stfsu stfd stfdu
        rs = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; d = s16(w)
        names = {52:'stfs',53:'stfsu',54:'stfd',55:'stfdu'}
        return f"{names[op]} f{rs},{d}(r{ra})" if ra else f"{names[op]} f{rs},{d}"
    if op == 31:
        rt = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; rb = (w >> 11) & 0x1F
        xo10 = (w >> 1) & 0x3FF
        if xo10 == 20: return f"lwarx r{rt},r{ra},r{rb}"
        if xo10 == 150: return f"stwcx. r{rt},r{ra},r{rb}"
        if xo10 == 339:  # mfspr
            spr = ((w >> 16) & 0x1F) | ((w >> 11) & 0x1F) << 5
            return f"mfspr r{rt},{SPR.get(spr, spr)}"
        if xo10 == 467:  # mtspr
            spr = ((w >> 16) & 0x1F) | ((w >> 11) & 0x1F) << 5
            return f"mtspr {SPR.get(spr, spr)},r{rt}"
        if xo10 == 83: return f"mfmsr r{rt}"
        if xo10 == 146: return f"mtmsr r{rt}"
        if xo10 == 19: return f"mfcr r{rt}"
        if xo10 == 144: return f"mtcrf 0x{(w>>12)&0xFF:X},r{rt}"
        if xo10 == 0:
            crf = (w >> 23) & 0x7
            return f"cmp cr{crf},r{ra},r{rb}"
        if xo10 == 32:
            crf = (w >> 23) & 0x7
            return f"cmpl cr{crf},r{ra},r{rb}"
        if xo10 == 595: return f"mfsr r{rt},r{ra}"   # mfsr rD,SR(ra)
        if xo10 == 210: return f"mtsr r{ra},r{rt}"
        if xo10 == 659: return f"mfsrin r{rt},r{ra},r{rb}"
        if xo10 == 242: return f"mtsrin r{rt},r{ra},r{rb}"
        if xo10 == 24: return f"slw r{ra},r{rt},r{rb}"
        if xo10 == 536: return f"srw r{ra},r{rt},r{rb}"
        if xo10 == 792: return f"sraw r{ra},r{rt},r{rb}"
        if xo10 == 824: return f"srawi r{ra},r{rt},0x{(w>>11)&0x1F:X}"
        if xo10 == 26: return f"cntlzw r{ra},r{rt}"
        if xo10 == 28: return f"and r{ra},r{rt},r{rb}"
        if xo10 == 60: return f"andc r{ra},r{rt},r{rb}"
        if xo10 == 124: return f"nor r{ra},r{rt},r{rb}"
        if xo10 == 444: return f"or r{ra},r{rt},r{rb}"
        if xo10 == 412: return f"orc r{ra},r{rt},r{rb}"
        if xo10 == 316: return f"xor r{ra},r{rt},r{rb}"
        if xo10 == 284: return f"eqv r{ra},r{rt},r{rb}"
        if xo10 == 476: return f"nand r{ra},r{rt},r{rb}"
        if xo10 == 266: return f"add r{rt},r{ra},r{rb}"
        if xo10 == 40: return f"subf r{rt},r{rb},r{ra}"  # subf rD,rA,rB -> ra rb reversed: subf RT,RA,RB
        if xo10 == 104: return f"neg r{rt},r{ra}"
        if xo10 == 235: return f"mullw r{rt},r{ra},r{rb}"
        if xo10 == 459: return f"divwu r{rt},r{ra},r{rb}"
        if xo10 == 491: return f"divw r{rt},r{ra},r{rb}"
        if xo10 == 138: return f"adde r{rt},r{ra},r{rb}"
        if xo10 == 136: return f"subfe r{rt},r{ra},r{rb}"
        if xo10 == 202: return f"addze r{rt},r{ra}"
        if xo10 == 200: return f"subfze r{rt},r{ra}"
        if xo10 == 234: return f"addme r{rt},r{ra}"
        if xo10 == 232: return f"subfme r{rt},r{ra}"
        if xo10 == 2: return f"lswi r{rt},r{ra},0x{(w>>11)&0xFF:X}"
        if xo10 == 597: return f"lswi r{rt},r{ra},0x{(w>>11)&0xFF:X}"
        if xo10 == 661: return f"stswx r{rt},r{ra},r{rb}"
        if xo10 == 725: return f"stswi r{rt},r{ra},0x{(w>>11)&0xFF:X}"
        if xo10 == 533: return f"lswx r{rt},r{ra},r{rb}"
        if xo10 == 23: return f"lwzx r{rt},r{ra},r{rb}"
        if xo10 == 55: return f"lwzux r{rt},r{ra},r{rb}"
        if xo10 == 87: return f"lbzx r{rt},r{ra},r{rb}"
        if xo10 == 119: return f"lbzux r{rt},r{ra},r{rb}"
        if xo10 == 279: return f"lhzx r{rt},r{ra},r{rb}"
        if xo10 == 311: return f"lhzux r{rt},r{ra},r{rb}"
        if xo10 == 343: return f"lhax r{rt},r{ra},r{rb}"
        if xo10 == 375: return f"lhaux r{rt},r{ra},r{rb}"
        if xo10 == 151: return f"stwx r{rt},r{ra},r{rb}"
        if xo10 == 183: return f"stwux r{rt},r{ra},r{rb}"
        if xo10 == 215: return f"stbx r{rt},r{ra},r{rb}"
        if xo10 == 247: return f"stbux r{rt},r{ra},r{rb}"
        if xo10 == 407: return f"sthx r{rt},r{ra},r{rb}"
        if xo10 == 439: return f"sthux r{rt},r{ra},r{rb}"
        if xo10 == 534: return f"lwbrx r{rt},r{ra},r{rb}"
        if xo10 == 662: return f"stwbrx r{rt},r{ra},r{rb}"
        if xo10 == 790: return f"lhbrx r{rt},r{ra},r{rb}"
        if xo10 == 918: return f"sthbrx r{rt},r{ra},r{rb}"
        if xo10 == 512: return f"mcrxr cr{(w>>23)&0x7}"
        if xo10 == 371: return f"mftb r{rt}"
        if xo10 == 11: return f"mulhwu r{rt},r{ra},r{rb}"
        if xo10 == 75: return f"mulhw r{rt},r{ra},r{rb}"
        if xo10 == 8: return f"subfc r{rt},r{ra},r{rb}"
        if xo10 == 10: return f"addc r{rt},r{ra},r{rb}"
        if xo10 == 4: return f"tw {rt},{ra},{rb}"
        if xo10 == 598: return "sync"
        if xo10 == 854: return "eieio"
        if xo10 == 278: return "dcbt r{ra},r{rb}"
        if xo10 == 246: return "dcbtst r{ra},r{rb}"
        if xo10 == 86: return "dcbf r{ra},r{rb}"
        if xo10 == 54: return "dcbst r{ra},r{rb}"
        if xo10 == 470: return "dcbi r{ra},r{rb}"
        if xo10 == 1014: return "dcbz r{ra},r{rb}"
        if xo10 == 982: return "icbi r{ra},r{rb}"
        if xo10 == 306: return "tlbie r{rb}"
        if xo10 == 370: return "tlbia"
        if xo10 == 566: return "tlbsync"
        if xo10 == 0x37A: return f"rlwinm r{ra},r{rt},0x{(w>>11)&0x1F:X},0x{(w>>6)&0x1F:X},0x{(w>>1)&0x1F:X}"
        if xo10 == 0x358: return f"rlwimi r{ra},r{rt},0x{(w>>11)&0x1F:X},0x{(w>>6)&0x1F:X},0x{(w>>1)&0x1F:X}"
        if xo10 == 0x357: return f"rlwnm r{ra},r{rt},r{rb},0x{(w>>6)&0x1F:X},0x{(w>>1)&0x1F:X}"
        if xo10 in (266+0x200, 40+0x200, 28+0x200, 444+0x200, 60+0x200, 124+0x200, 316+0x200):
            return f"(OE form) {disasm(w & ~0x1000, a)}"
        # rlwinm family also 0x37A is actually the XO for all rotate-mask
        if xo10 == 0x152: return f"mfsr r{rt},r{ra}"  # mfsr uses XO 0x152? (fallback)
        if xo10 == 0x1D2: return f"mtsr r{ra},r{rt}"   # fallback
        if xo10 == 0x053: return f"mfmsr r{rt}"
        if w == 0x4C000064: return "rfi"
        if w == 0x7C0000A6: return "mfmsr r0"
        if xo10 == 0x192: return "rfsvc"
        return f".long 0x{w:08X}"
    # rlwinm handled above via XO; also M-form is opcode 21
    if op == 21:  # rlwinm
        rs = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F
        sh = (w >> 11) & 0x1F; mb = (w >> 6) & 0x1F; me = (w >> 1) & 0x1F
        return f"rlwinm r{ra},r{rs},0x{sh:X},0x{mb:X},0x{me:X}"
    if op == 20:  # rlwimi
        rs = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F
        sh = (w >> 11) & 0x1F; mb = (w >> 6) & 0x1F; me = (w >> 1) & 0x1F
        return f"rlwimi r{ra},r{rs},0x{sh:X},0x{mb:X},0x{me:X}"
    if op == 23:  # rlwnm
        rs = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; rb = (w >> 11) & 0x1F
        mb = (w >> 6) & 0x1F; me = (w >> 1) & 0x1F
        return f"rlwnm r{ra},r{rs},r{rb},0x{mb:X},0x{me:X}"
    if op == 3:  # twi
        to = (w >> 21) & 0x1F; ra = (w >> 16) & 0x1F; im = s16(w)
        return f"twi {to},r{ra},0x{im & 0xFFFFFFFF:X}"
    if op == 17:  # sc
        return "sc"
    if op == 19:
        # XL form: some common
        xo10 = (w >> 1) & 0x3FF
        if xo10 == 0xC0: return "crxor"   # not right; crxor is op31
        return f".long 0x{w:08X} (XL)"
    if op == 17: return "sc"
    if op == 0: return f".long 0x{w:08X} (illegal)"
    return f".long 0x{w:08X} (op{op})"

def disassemble(data, base, start, end):
    for a in range(start, end, 4):
        w = int.from_bytes(data[a:a+4], 'big')
        try:
            s = disasm(w, base + a)
        except Exception as e:
            s = f".long 0x{w:08X} (err {e})"
        print(f"0x{base+a:08X}: {w:08X}  {s}")

if __name__ == '__main__':
    path, base, start, end = sys.argv[1], int(sys.argv[2], 0), int(sys.argv[3], 0), int(sys.argv[4], 0)
    data = open(path, 'rb').read()
    disassemble(data, base, start, end)

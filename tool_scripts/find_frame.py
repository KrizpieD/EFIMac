import sys, struct
sys.path.insert(0, r"C:\Users\clayc\AppData\Local\Temp\opencode")
import decode_tail as dt
def g2f(g): return 0x310000 + (g - 0x40B10000)
def word_at(off): return struct.unpack(">I", dt.out[off:off+4])[0]
# search decompressed rom for stw rX, 0x648(r1) and related frame slots
hits = {}
import re
for off in range(0, len(dt.out)-4, 4):
    w = word_at(off)
    op = w >> 26
    rA = (w >> 16) & 0x1F
    rS = (w >> 21) & 0x1F
    if op == 36 and rA == 1:  # stw
        d = w & 0xFFFF
        if d >= 0x8000: d -= 0x10000
        if d in (0x648, 0x5A0, 0x5A4, -0x964, 0x420):
            key = "0x%X(r1)" % d
            hits.setdefault(key, []).append(off)
for k, v in sorted(hits.items(), key=lambda x: len(x[1])):
    print("%-12s count=%d  first at file 0x%06X (guest 0x%08X)" % (k, len(v), v[0], 0x40800000+v[0]))

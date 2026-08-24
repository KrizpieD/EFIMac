import sys, struct
sys.path.insert(0, r"C:\Users\clayc\AppData\Local\Temp\opencode")
import decode_tail as dt
def g2f(g): return 0x310000 + (g - 0x40B10000)
def word_at(a): return struct.unpack(">I", dt.out[a:a+4])[0]
def dump(guest, n):
    a = g2f(guest)
    print("=== guest 0x%08X ===" % guest)
    for k in range(a, a + n*4, 4):
        w = word_at(k)
        print("0x%08X: 0x%08X  %s" % (0x40B10000 + (k-0x310000), w, dt.disasm(w, k)))
    print()
dump(0x40B10740, 18)
# find bl targets into 0x310700-0x310800 range
import re
start = g2f(0x40B10000); end = g2f(0x40B29F00)
for k in range(start, end, 4):
    w = word_at(k)
    if (w >> 26) == 18:  # bl
        li = w & 0x3FFFFFF
        if li & 0x2000000: li -= 0x4000000
        tgt = 0x40B10000 + (k - start) + li
        if 0x40B10700 <= tgt <= 0x40B10820:
            print("bl @ 0x%08X -> 0x%08X" % (0x40B10000+(k-start), tgt))

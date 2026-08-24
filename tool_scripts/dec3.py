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
dump(0x40B10040, 24)
print("data words:")
for name, g in [("0x310084", 0x310084), ("0x3100AC", 0x3100AC), ("0x310360", 0x310360), ("0x31035C", 0x31035C)]:
    print("%s = 0x%08X" % (name, word_at(g)))

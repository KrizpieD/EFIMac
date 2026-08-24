import sys, struct
sys.path.insert(0, r"C:\Users\clayc\AppData\Local\Temp\opencode")
import decode_tail as dt
def g2f(g): return 0x310000 + (g - 0x40B10000)
def dump(guest, n):
    a = g2f(guest)
    print("=== guest 0x%08X (file 0x%06X) ===" % (guest, a))
    for k in range(a, a + n*4, 4):
        w = struct.unpack(">I", dt.out[k:k+4])[0]
        print("0x%08X: 0x%08X  %s" % (0x40B10000 + (k-0x310000), w, dt.disasm(w, k)))
    print()
dump(0x40B10080, 40)
dump(0x40B107E0, 16)

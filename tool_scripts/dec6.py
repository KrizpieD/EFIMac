import sys
sys.path.insert(0, r"C:\Users\clayc\AppData\Local\Temp\opencode")
import decode_tail as dt
def g2f(g): return 0x310000 + (g - 0x40B10000)
a = g2f(0x40B12660)
b = g2f(0x40B126C0)
blob = dt.out[a:b]
print("bytes 0x40B12660..0x40B126C0 (len %d):" % len(blob))
print(" ".join("%02x" % x for x in blob))
print("ASCII:", repr(blob))

def XO10(w): return (w >> 1) & 0x3FF
def SPR(w): return (((w >> 16) & 0x1F) | (((w >> 11) & 0x1F) << 5))

words = [
    0x7C6803A6,  # mtspr lr, r3 (self-test)
    0x7D2802A6,  # mfspr r9, lr (self-test)
    0x7D9A03A6,  # boot TRACE[12] mtspr SRR0, r12
    0x7D7B03A6,  # boot TRACE[13] mtspr SRR1, r11
    0x7C0000A6,  # boot TRACE[3]  mfspr r0, ?
    0x7D6000A6,  # boot TRACE[9]  mfspr r11, ?
    0x7D6B5078,  # boot TRACE[11]
    0x7C0000A6,
    0x4C000064,  # rfi
]
for w in words:
    print("0x%08X  XO=%d  SPR=%d  rD/rS=%d  RA=%d  RB=%d" %
          (w, XO10(w), SPR(w), (w >> 21) & 0x1F, (w >> 16) & 0x1F, (w >> 11) & 0x1F))

# Self-test word sanity
print()
print("mtlr (0x7C6803A6): XO=467 (MTSPR), SPR=%d -> expect 8 (LR)" % SPR(0x7C6803A6))
print("mfspr r9,lr (0x7D2802A6): XO=%d, SPR=%d -> expect 8" % (XO10(0x7D2802A6), SPR(0x7D2802A6)))
print("mtspr SRR0 (0x7D9A03A6): SPR=%d -> expect 26" % SPR(0x7D9A03A6))
print("mtspr SRR1 (0x7D7B03A6): SPR=%d -> expect 27" % SPR(0x7D7B03A6))
print("mfspr r0,?  (0x7C0000A6): SPR=%d" % SPR(0x7C0000A6))
print("mfspr r11,? (0x7D6000A6): SPR=%d" % SPR(0x7D6000A6))

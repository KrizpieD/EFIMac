import sys, struct
from capstone import *
ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin"
ROM_BASE = 0x40800000
HDR = 12
data = open(ROM, "rb").read()
md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN)
md.detail = True
def dis(start, stop):
    print("=== %#x .. %#x ===" % (start, stop))
    for addr in range(start, stop, 4):
        off = (addr - ROM_BASE) + HDR
        w = data[off:off+4]
        if len(w) < 4: break
        ins = list(md.disasm(w, addr))
        if ins:
            i = ins[0]
            print("  %#x: 0x%08X  %s %s" % (i.address, struct.unpack(">I", w)[0], i.mnemonic, i.op_str))
        else:
            print("  %#x: 0x%08X  <invalid>" % (addr, struct.unpack(">I", w)[0]))
for a, b in [(int(x, 0), int(y, 0)) for x, y in zip(sys.argv[1::2], sys.argv[2::2])]:
    dis(a, b)

import sys, struct
from capstone import *
ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat.bin"
ROM_BASE = 0x40800000
data = open(ROM,"rb").read()
md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN)
def dis(start, stop):
    print("=== %#x .. %#x ===" % (start, stop))
    for addr in range(start, stop, 4):
        off = addr - ROM_BASE
        w = data[off:off+4]
        if len(w) < 4: break
        ins = list(md.disasm(w, addr))
        if ins:
            i = ins[0]
            print("  %#x: 0x%08X  %s %s" % (i.address, struct.unpack(">I", w)[0], i.mnemonic, i.op_str))
        else:
            print("  %#x: 0x%08X  <invalid>" % (addr, struct.unpack(">I", w)[0]))
dis(int(sys.argv[1],0), int(sys.argv[2],0))

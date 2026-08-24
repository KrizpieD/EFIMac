from capstone import *
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin","rb").read()
BASE = 0x40800000
def dis(start, count, label=""):
    print("=== %s 0x%08x ===" % (label, start))
    off = start - BASE
    md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN + CS_MODE_32)
    for i in md.disasm(data[off:off+count*4], start):
        print("0x%08x: %s %s" % (i.address, i.mnemonic, i.op_str))
dis(0x40B1F500, 40, "free-list walk region")
dis(0x40B272F8, 12, "Termination entry")

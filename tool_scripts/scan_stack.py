import struct
from capstone import Cs, CS_ARCH_PPC, CS_MODE_BIG_ENDIAN, CS_MODE_32

ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin"
BASE = 0x40800000
data = open(ROM, "rb").read()

targets = [0x638, 0x63C, 0x6A8, 0x6AC, 0x6B0, 0x6B4, 0x6BC]
md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN | CS_MODE_32)

def scan(start, stop):
    hits = []
    text = data[start:BASE - BASE + stop] if False else data[start:stop]
    for insn in md.disasm(text, BASE + start):
        op = insn.op_str
        for t in targets:
            if ("0x%x(r1)" % t) in op:
                hits.append((insn.address, insn.mnemonic, insn.op_str))
    return hits

hits = scan(0, len(data))
for h in hits:
    print("0x%08x: %s %s" % h)
print("total", len(hits))

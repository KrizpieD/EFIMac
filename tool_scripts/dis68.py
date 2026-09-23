import sys, struct
from capstone import *

ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin"

def dis(rom_off, start_addr, count):
    data = open(ROM, "rb").read()
    md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN)
    md.detail = False
    seg = data[rom_off:rom_off + count * 2]
    print("=== file off %#x as 68K ===" % rom_off)
    for addr in range(start_addr, start_addr + len(seg), 2):
        w = seg[addr - start_addr:addr - start_addr + 2]
        if len(w) < 2:
            break
        op = struct.unpack(">H", w)[0]
        ins = list(md.disasm(w, addr))
        if ins:
            i = ins[0]
            print("  %#08x: %04x  %-22s %s" % (addr, op, i.mnemonic, i.op_str))
        else:
            print("  %#08x: %04x  <invalid>" % (addr, op))

if __name__ == "__main__":
    # args: file_offset_hex  addr_hex  count_words
    rom_off = int(sys.argv[1], 0)
    addr = int(sys.argv[2], 0)
    n = int(sys.argv[3], 0)
    dis(rom_off, addr, n)
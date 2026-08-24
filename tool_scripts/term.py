from capstone import *
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
def dis(addr, n):
    off = addr - BASE
    print(f"--- {addr:#010x} ---")
    for i in md.disasm(rom[off:off+4*n], addr):
        print(f"{i.address:08x}: {i.bytes.hex():>8}  {i.mnemonic} {i.op_str}")
dis(0x40B1F5C0, 48)   # both call sites region
dis(0x40B272C0, 24)   # terminal entry

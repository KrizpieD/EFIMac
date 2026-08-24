from capstone import *

ROM = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

def dis(off, n, label):
    print(f"--- {label} (guest {BASE+off:#x}) ---")
    cnt = 0
    for i in md.disasm(ROM[off:off+n*6], BASE+off):
        print(f"{i.address:08x}: {i.bytes.hex():<16} {i.mnemonic} {i.op_str}")
        cnt += 1
        if cnt >= n: break

dis(0xAD7C, 14, "after 6700 AD7C")
dis(0xE1A0, 26, "around FFFF region")

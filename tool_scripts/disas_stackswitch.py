import sys
from capstone import *

ROM = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000

def dis(off, n, label):
    md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)
    code = ROM[off:off+n*8]
    print(f"--- {label} (guest {BASE+off:#x}) ---")
    cnt = 0
    for i in md.disasm(code, BASE+off):
        print(f"{i.address:08x}: {i.bytes.hex():<12} {i.mnemonic} {i.op_str}")
        cnt += 1
        if cnt >= n: break

# Routine that loads A7 from memory and jmps away
dis(0xA8D50, 40, "VM svc / frame build 0x408A8D50")

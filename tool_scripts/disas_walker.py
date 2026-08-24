from capstone import *

ROM = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

def dis(off, n, label):
    print(f"--- {label} ---")
    addr = BASE + off
    code = ROM[off:off+n*16]
    cnt = 0
    for i in md.disasm(code, BASE+off):
        print(f"{i.address:08x}: {i.bytes.hex():<26} {i.mnemonic} {i.op_str}")
        addr += i.size
        cnt += 1
        if cnt >= n: break

dis(0x8178, 24, "walker 8178-8208")

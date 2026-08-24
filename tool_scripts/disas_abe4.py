from capstone import *

ROM = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

def dis(off, n, label):
    print(f"--- {label} ---")
    cnt = 0
    for i in md.disasm(ROM[off:off+n*14], BASE+off):
        print(f"{i.address:08x}: {i.bytes.hex():<26} {i.mnemonic} {i.op_str}")
        cnt += 1
        if cnt >= n: break

dis(0xABE4, 24, "ABE4 continuation after HNoF match")

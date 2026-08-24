from capstone import *

ROM = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

def dis(off, n, label):
    print(f"--- {label} ---")
    cnt = 0
    for i in md.disasm(ROM[off:off+n*12], BASE+off):
        print(f"{i.address:08x}: {i.bytes.hex():<24} {i.mnemonic} {i.op_str}")
        cnt += 1
        if cnt >= n: break

dis(0xAFD4, 22, "after magic check")
dis(0xAB68, 20, "alt path AB68")

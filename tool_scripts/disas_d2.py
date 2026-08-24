from capstone import *

ROM = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

def dis(off, n, label):
    print(f"--- {label} ---")
    cnt = 0
    for i in md.disasm(ROM[off:off+n*8], BASE+off):
        print(f"{i.address:08x}: {i.bytes.hex():<20} {i.mnemonic} {i.op_str}")
        cnt += 1
        if cnt >= n: break

dis(0xAA10, 18, "dispatcher2 AA10")
dis(0xAB4E, 12, "park AB4E")
dis(0xAFB4, 6, "park AFB4")

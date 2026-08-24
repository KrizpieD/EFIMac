from capstone import *

ROM = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

def dis(off, n, label):
    print(f"--- {label} (guest {BASE+off:#x}) ---")
    cnt = 0
    for i in md.disasm(ROM[off:off+n*14], BASE+off):
        print(f"{i.address:08x}: {i.bytes.hex():<26} {i.mnemonic} {i.op_str}")
        cnt += 1
        if cnt >= n: break

# early references at 0x42c / 0x50a — likely the CREATOR of the struct!
dis(0x400, 16, "early 0x42c refs (creator?)")
dis(0x500, 10, "early 0x50a refs")

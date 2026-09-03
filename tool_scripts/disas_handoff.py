import struct
from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_040
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)
for lo,hi,tag in [(0xAA00,0xAB30,"AA00 dispatch"),(0xAFB4,0xB040,"AFB4 target")]:
    print(f"\n=== {tag} ===")
    for i in md.disasm(data[lo:hi], BASE+lo):
        print(f"{i.address:#010x}: {i.bytes.hex():<22} {i.mnemonic:<9} {i.op_str}")

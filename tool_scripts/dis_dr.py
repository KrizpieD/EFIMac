from capstone import Cs, CS_ARCH_PPC, CS_MODE_BIG_ENDIAN, CS_MODE_32
import struct
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN|CS_MODE_32)
md.detail = True
BASE = 0x40800000
for lo,hi,tag in [(0x40B67B60-0x40800000, 0x40B67C60-0x40800000, "DR dispatch 0x40B67B60"),
                  (0x40B6E964-0x40800000, 0x40B6EB00-0x40800000, "DR start 0x40B6E964")]:
    print(f"\n=== {tag} ===")
    for i in md.disasm(data[lo:hi], BASE+lo):
        print(f"{i.address:#010x}: {i.bytes.hex():<16} {i.mnemonic:<9} {i.op_str}")

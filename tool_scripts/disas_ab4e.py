import struct
from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_040

data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

print("=== 0x4080AB30..0x4080AB80 ===")
for i in md.disasm(data[0xAB30:0xAB80], BASE + 0xAB30):
    print(f"{i.address:#010x}: {i.bytes.hex():<12} {i.mnemonic:<8} {i.op_str}")

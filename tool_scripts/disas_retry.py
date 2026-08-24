import struct
from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_040

data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
BASE = 0x40800000

md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)

print("=== caller 0x408081F8..0x40808300 ===")
for i in md.disasm(data[0x81F8:0x8300], BASE + 0x81F8):
    print(f"{i.address:#010x}: {i.mnemonic:<8} {i.op_str}")

def words(a, n):
    off = a - BASE
    ws = struct.unpack_from(f">{n}H", data, off)
    out = []
    for i in range(0, n, 8):
        row = " ".join(f"{w:04x}" for w in ws[i:i+8])
        out.append(f"  {a+i*2:#010x}: {row}")
    return "\n".join(out)

print("\n=== 0x4080B8C0..0x4080B9E0 words ===")
print(words(BASE + 0xB8C0, 144))

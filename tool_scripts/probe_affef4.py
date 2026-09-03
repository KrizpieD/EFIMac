import struct
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin", "rb").read()
# 0x40AFC000 -> offset 0x2FC000 (flat, base 0x40800000)
for addr in [0x40AFC000, 0x40AFFEF4, 0x40A8C0B4, 0x40ACC0B4]:
    off = addr - 0x40800000
    w = struct.unpack(">I", data[off:off+4])[0]
    print(f"{addr:#010x}: 0x{w:08X}")
print("\n=== ASCII around 0x40AFFEF4 (string context) ===")
off = 0x40AFFEF4 - 0x40800000
s = data[off-64:off+64]
print(s)
print("\n=== PPC bytes at 0x40AFC000 ==")
print(data[0x2FC000:0x2FC040].hex(" "))

from capstone import *
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)
for base, start, end in [(0x408081A8, 0x81A8, 0x8220),
                         (0x408000EC, 0x00EC, 0x0130)]:
    print(f"=== {base:#x} ===")
    code = data[start:end]
    for i in md.disasm(code, base):
        print(f"{i.address:08x}: {i.bytes.hex():<12} {i.mnemonic} {i.op_str}")

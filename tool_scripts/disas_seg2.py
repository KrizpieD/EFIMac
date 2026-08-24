from capstone import *
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)
for base, start, end in [(0x4080AAD8, 0xAAD8, 0xAB62),
                         (0x4080820A, 0x820A, 0x8280)]:
    print(f"=== {base:#x} ===")
    for i in md.disasm(data[start:end], base):
        print(f"{i.address:08x}: {i.bytes.hex():<12} {i.mnemonic} {i.op_str}")

from capstone import *
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_040)
for i in md.disasm(data[0x0624:0x06B0], 0x40800624):
    print(f"{i.address:08x}: {i.bytes.hex():<12} {i.mnemonic} {i.op_str}")

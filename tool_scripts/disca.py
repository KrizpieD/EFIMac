import capstone
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
start,end = 0x36ca30, 0x36ca80
for i in md.disasm(rom[start:end], base+start):
    print(f"0x{i.address:x}: {i.bytes.hex()} {i.mnemonic} {i.op_str}")

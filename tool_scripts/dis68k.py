import capstone
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
md = capstone.Cs(capstone.CS_ARCH_M68K, capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_M68K_000)
start = 0x80ae60
for i in md.disasm(rom[start:start+0xa0], base+start):
    print(f"0x{i.address:x}: {i.bytes.hex():<12} {i.mnemonic} {i.op_str}")

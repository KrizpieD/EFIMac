import capstone
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
print("68K bytes @0xB0-0xD0:", rom[0xB0:0xD0].hex())
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
off = 0x40B6CA30 - base
for i in md.disasm(rom[off:0x40B6CA80-base], 0x40B6CA30):
    print(f"0x{i.address:x}: {i.bytes.hex()} {i.mnemonic} {i.op_str}")

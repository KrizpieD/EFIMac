import capstone
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
off = 0x409365C0 - base
for i in md.disasm(rom[off:0x40936660-base], 0x409365C0):
    print(f"0x{i.address:x}: {i.bytes.hex()} {i.mnemonic} {i.op_str}")

import capstone
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
for start, end in [(0x40B126E0, 0x40B12790)]:
    off = start - base
    for i in md.disasm(rom[off:end-base], start):
        print(f"0x{i.address:x}: {i.bytes.hex()} {i.mnemonic} {i.op_str}")

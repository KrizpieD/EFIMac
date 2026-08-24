import capstone
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
for start,end in [(0x36c4a0, 0x36c580), (0x36c600, 0x36c6b0)]:
    print(f"---- {base+start:#x}")
    for i in md.disasm(rom[start:end], base+start):
        print(f"0x{i.address:x}: {i.bytes.hex()} {i.mnemonic} {i.op_str}")

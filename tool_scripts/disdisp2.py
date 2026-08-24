import capstone
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
def dump(a,b,label):
    print(f"=== {label} ===")
    off=a-base
    for i in md.disasm(rom[off:b-base], a):
        print(f"0x{i.address:x}: {i.bytes.hex()} {i.mnemonic} {i.op_str}")
dump(0x40B6C340, 0x40B6C420, "reader1 ctx")
dump(0x40B6C600, 0x40B6C660, "target of second reader")
dump(0x40B6C550, 0x40B6C584, "loop tail")

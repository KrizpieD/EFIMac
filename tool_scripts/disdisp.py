import capstone
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
def dump(a,b):
    off=a-base
    for i in md.disasm(rom[off:b-base], a):
        print(f"0x{i.address:x}: {i.bytes.hex()} {i.mnemonic} {i.op_str}")
print("=== crash dispatch path ===")
dump(0x40B6C4F4, 0x40B6C548)
print("=== second reader ===")
dump(0x40B803E0, 0x40B80420)

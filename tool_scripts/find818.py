import capstone
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
hits = []
for i in range(len(rom)-3):
    b = rom[i]
    if rom[i+1] == 0xFF and rom[i+2] == 0x08 and rom[i+3] == 0x18:
        if 0x80 <= b <= 0x9F:
            hits.append((i,b))
print("hits:", len(hits))
for h,b in hits[:20]:
    addr = base + h
    print(f"\n--- 0x{addr:x}")
    start = max(0, h-20)
    for ins in md.disasm(rom[start:h+4], base+start):
        print(f"  0x{ins.address:x}: {ins.bytes.hex()} {ins.mnemonic} {ins.op_str}")

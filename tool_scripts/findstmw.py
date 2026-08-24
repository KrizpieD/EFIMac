import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
# stmw (op47): covers regs rS..r31, store d(rA). If rA=31 and d in [0x600..0xA00], it hits ED+0x8xx
print("=== stmw r?,d(r31) ===")
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    if (w>>26)==47 and ((w>>16)&0x1F)==31:
        d = w & 0xFFFF
        if d >= 0x400:
            print(f"  0x{base+i:x}: {w:08x}  stmw d=0x{d:x}")
print("=== stmw any base, d 0x700-0x900 ===")
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    if (w>>26)==47:
        d = w & 0xFFFF
        ra=(w>>16)&0x1F
        if 0x700 <= d <= 0x900 and ra!=0:
            print(f"  0x{base+i:x}: {w:08x}  stmw rA={ra} d=0x{d:x}")

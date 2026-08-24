import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    op=w>>26
    ra=(w>>16)&0x1F
    d=w&0xFFFF
    if ra==31 and 0x60 <= d <= 0x80 and op in (32,33,34,35,36,38,40,44,46,47,12,14,13):
        print(f"0x{base+i:x}: {w:08x}  op={op} d={d:#x}")

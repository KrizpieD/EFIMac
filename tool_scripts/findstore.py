import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
print("stw rX, 0x818(rY):")
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    if (w>>26)==36 and (w&0xFFFF)==0x818:
        rt=(w>>21)&0x1F; ra=(w>>16)&0x1F
        print(f"  0x{base+i:x}: {w:08x}  stw r{rt},0x818(r{ra})")
# also addis/ori pairs building 0x818 offsets nearby, and stmw covering 0x818
print("stmw rX, -0x800..(rY) wide saves:")
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    if (w>>26)==47:  # stmw
        d=w&0xFFFF
        if d>=0xF800:  # negative >= -0x800
            rs=(w>>21)&0x1F; ra=(w>>16)&0x1F
            print(f"  0x{base+i:x}: {w:08x}  stmw r{rs},{d-0x10000:#x}(r{ra})")

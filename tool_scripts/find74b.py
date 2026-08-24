import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40800000
# lwz rX, 0x74(r31): any RT
print("lwz rX,0x74(r31):")
for i in range(0, len(rom)-3, 4):
    w = struct.unpack(">I", rom[i:i+4])[0]
    if (w>>26)==32 and ((w>>16)&0x1F)==31 and (w&0xFFFF)==0x74:
        rt=(w>>21)&0x1F
        print(f"  0x{base+i:x}: {w:08x}  lwz r{rt},0x74(r31)")

import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
bad=[]
for off in range(0x36f7cc, 0x36f900, 4):
    w=struct.unpack(">I", rom[off:off+4])[0]
    if w != 0x60000000: bad.append((hex(off),hex(w)))
print("non-nop words in 0x36f7cc-0x36f900:", bad if bad else "NONE - all free")

rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
a = rom[0x53000:0x73000]; b = rom[0x360000:0x380000]
print("EmulatorCode: 0x53000 == 0x360000 ?", a == b)
c = rom[0x73000:0xF3000]; d = rom[0x380000:0x400000]
print("OpcodeTable : 0x73000 == 0x380000 ?", c == d)
if c != d:
    diff = sum(1 for x,y in zip(c,d) if x!=y)
    print(f"  differing bytes: {diff} / {len(c)}")
# peek first entries of the table at 0x73000
import struct
for k in range(6):
    w1,w2 = struct.unpack(">II", rom[0x73000+k*8:0x73000+k*8+8])
    print(f"  tbl[{k}] @0x{0x40873000+k*8:x}: {w1:08x} {w2:08x}")

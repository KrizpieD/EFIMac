import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
base = 0x40B80000  # hypothesized opcode-table base (file 0x380000)
# Sample entries across the table; count branches targeting emulator-code range 0x40B60000-0x40B80000
import random
random.seed(1)
tot=0; hit=0
samples=[]
for k in random.sample(range(0x10000), 2000):
    off = k*8
    w1,w2 = struct.unpack(">II", rom[0x380000+off:0x380000+off+8])
    tot+=1
    for w in (w1,w2):
        op=w>>26
        if op==18:  # b/bl
            li=w&0x03FFFFFC
            if li&0x02000000: li-=0x04000000
            tgt = base+off+li
            if 0x40B60000 <= tgt < 0x40B80000: hit+=1
    if k in (0x2700,0x4E71,0x0038,0x2088, 0x6100) and len(samples)<8:
        samples.append((k, w1, w2))
print(f"branches from table into code: {hit}/{tot} sampled entries")
for k,w1,w2 in samples:
    print(f"  op 0x{k:04x}: {w1:08x} {w2:08x}")

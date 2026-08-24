import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
BASE = 0x40800000
sig = b"AREA"
hits = []
i = rom.find(sig)
while i != -1:
    hits.append(BASE + i)
    i = rom.find(sig, i+1)
print(f"{len(hits)} 'AREA' hits")
for h in hits[:40]:
    off = h - BASE
    ctx = rom[off-16:off+20]
    print(f"{h:08X}: ...{ctx.hex()}...")

import pathlib, struct
d = pathlib.Path(r'C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin').read_bytes()
for rt in range(32):
    ori = struct.pack('>I', 0x60000000 | (rt<<16) | 0x22F4)
    i = d.find(ori)
    if i >= 0:
        print('ori r%d, 0x22F4 at', hex(i), 'guest', hex(0x40800000+i))
        print('  ctx:', d[i-16:i+16])

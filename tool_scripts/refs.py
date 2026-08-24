import pathlib, struct
d = pathlib.Path(r'C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin').read_bytes()
target = 0x40B122F4
ba = target.to_bytes(4,'big')
i = 0
hits = []
while True:
    i = d.find(ba, i)
    if i < 0: break
    hits.append(i)
    i += 1
print('big-endian refs:', [hex(h) for h in hits])
# also search for the value stored little-endian (data might be LE?)
le = struct.pack('<I', target)
i = 0
hits2 = []
while True:
    i = d.find(le, i)
    if i < 0: break
    hits2.append(i)
    i += 1
print('little-endian refs:', [hex(h) for h in hits2])
# NK base is 0x310000 in image; string at 0x3122f4. Search addis/ori pattern: lis 0x40B1 -> 0x3C6040B1 (r3) etc.
# Print surrounding context of the string
print('ctx:', d[0x3122d0:0x312350])

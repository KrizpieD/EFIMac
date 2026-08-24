import struct
data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
target = 0xAB4E
# 1. absolute longword refs to guest addr
needle = struct.pack(">I", 0x40800000 + target)
i = data.find(needle)
while i != -1:
    print(f"abs ref @image {i:#x} (guest {0x40800000+i:#08x})")
    i = data.find(needle, i + 1)
# 2. rel.w (0x60xx/0x66xx etc 16-bit disp ending at target): scan all opcodes
#    generic: any 16-bit displacement where pos+2+disp == target
hits = []
for off in range(0, min(len(data), 0x40000)):
    op = struct.unpack_from(">H", data, off)[0]
    if (op & 0xFF00) == 0x6000:  # Bcc.S/BRA.S
        disp = data[off + 1]
        if disp >= 0x80:
            disp -= 0x100
        if off + 2 + disp == target:
            hits.append((off, f"{op:04x} .s"))
    elif (op & 0xF0FF) == 0x6600 and False:
        pass
for off in range(0, min(len(data), 0x40000)):
    op = struct.unpack_from(">H", data, off)[0]
    if (op & 0xFF00) == 0x6600 or (op & 0xFF00) == 0x6700 or \
       (op & 0xFF00) == 0x6600:
        pass
# 3. rel.w 6600/6700 style with 16-bit disp
for off in range(0, min(len(data), 0x40000) - 4):
    op = struct.unpack_from(">H", data, off)[0]
    if op in (0x6600, 0x6700, 0x6602):
        disp = struct.unpack_from(">h", data, off + 2)[0]
        if off + 4 + disp == target:
            hits.append((off, f"{op:04x} .w"))
# 4. jmp/jsr abs.l 4EF9/4EB9 to guest target
for off in range(0, min(len(data), 0x40000) - 6):
    op = struct.unpack_from(">H", data, off)[0]
    if op in (0x4EF9, 0x4EB9):
        v = struct.unpack_from(">I", data, off + 2)[0]
        if v == 0x40800000 + target:
            hits.append((off, f"{op:04x} abs"))
print("branch hits:", [(hex(h[0]), h[1]) for h in hits[:20]])

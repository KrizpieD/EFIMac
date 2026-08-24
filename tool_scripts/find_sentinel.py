data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
import struct
start = 0xAB4E
i = start
while i < len(data) - 4:
    v = struct.unpack_from(">I", data, i)[0]
    if v == 0xFFFFFFFF:
        print(f"sentinel at image 0x{i:x} (guest 0x{0x40800000+i:08x}), "
              f"len={i-start:#x}")
        print("header:", data[start:start+16].hex())
        break
    i += 4
else:
    print("no sentinel found")

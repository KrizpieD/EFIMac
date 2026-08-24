data = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin", "rb").read()
import struct
# LA_DispatchTable = 0x40B80000 -> file offset 0x380000
off = 0x380000
words = struct.unpack(">16I", data[off:off+64])
print("DispatchTable@0x380000:", " ".join(f"{w:08x}" for w in words))
nz = sum(1 for i in range(0, 0x10000, 4) if int.from_bytes(data[off+i:off+i+4],"big") != 0)
print(f"nonzero words in first 64KB: {nz}/{0x10000//4}")
# opcode for PC 0x4080002a: what does the ROM contain there?
pc_off = 0x2a
print("68K start bytes @ROM+0x2a:", data[pc_off:pc_off+16].hex())

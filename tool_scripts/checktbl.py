import struct
rom = open(r"C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin","rb").read()
def words(off, n):
    return [struct.unpack(">I", rom[off+i*4:off+i*4+4])[0] for i in range(n)]
print("=== ROM+0x200000 (candidate OpcodeTable) ===")
w = words(0x200000, 16)
for i in range(0, 16, 2):
    print(f"  op[{i//2:#06x}]: {w[i]:#010x} {w[i+1]:#010x}")
print("=== sample mid-table op[0x4e70>>?] area ===")
# SheepShaver: handler = table + opcode*8. For opcode 0x2700: offset 0x2700*8 = 0x13800
w2 = words(0x200000 + 0x2700*8, 4)
print(f"  op 0x2700 entry: {w2[0]:#010x} {w2[1]:#010x}")
# also check dispatch-table page 0x380000
print("=== ROM+0x380000 (DispatchTable) ===")
for v in words(0x380000, 12): print(f"  {v:#010x}")

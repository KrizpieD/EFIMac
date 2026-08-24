"""Read DR emulator dispatch table entries (ROM+0x380000 + opcode*8) for F-line opcodes."""
import struct

ROM_PATH = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'
ROM_BASE = 0x40800000
TBL = 0x380000  # LA_DispatchTable offset within ROM

with open(ROM_PATH, 'rb') as f:
    rom = f.read()

def rd32(off):
    return struct.unpack('>I', rom[off:off+4])[0]

print("opcode : entry[0]   entry[1]   (guest addrs)")
for op in list(range(0xFE00, 0xFE10)) + [0xFC1E, 0xF3C0]:
    e_off = TBL + op * 8
    a0 = rd32(e_off)
    a1 = rd32(e_off + 4)
    print(f"{op:04X}   : {a0:08X} {a1:08X}")

# Also check what a 'normal' opcode entry looks like for comparison
for op in [0x4E75, 0x2069, 0x1234]:
    e_off = TBL + op * 8
    print(f"{op:04X}   : {rd32(e_off):08X} {rd32(e_off+4):08X}")

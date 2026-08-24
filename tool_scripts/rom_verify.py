import struct

rom = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'

with open(rom, 'rb') as f:
    data = f.read()

def read(addr):
    off = addr - 0x40800000
    return data[off]

def dump(addr, count):
    off = addr - 0x40800000
    return list(data[off:off+count])

def hexdump(addr, count):
    off = addr - 0x40800000
    for i in range(0, count, 16):
        chunk = data[off+i:off+i+16]
        hexstr = ' '.join(f'{b:02X}' for b in chunk)
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        print(f'  0x{addr+i:08X}: {hexstr:<48s} {ascii_str}')

print("=== CRITICAL: Bytes at 0x4080AA2C (where emulator sees 8801 but Capstone sees 4E7B) ===")
bytes_at_aa2c = dump(0x4080AA2C, 4)
print(f'  ROM bytes at 0x4080AA2C: {" ".join(f"{b:02X}" for b in bytes_at_aa2c)}')
print(f'  Big-endian word: 0x{(bytes_at_aa2c[0] << 8) | bytes_at_aa2c[1]:04X}')
print()

# Check trace claim: instruction 33 at PC=0x4080AA2C has Op=0x8801
print("=== Trace says Op=0x8801 at 0x4080AA2C ===")
print(f'  ROM has: 0x{(bytes_at_aa2c[0] << 8) | bytes_at_aa2c[1]:04X}')
if (bytes_at_aa2c[0] << 8) | bytes_at_aa2c[1] == 0x8801:
    print('  MATCH! ROM matches trace.')
else:
    print('  MISMATCH! ROM does NOT match trace!')
    print(f'  Trace expects 88 01, ROM has {bytes_at_aa2c[0]:02X} {bytes_at_aa2c[1]:02X}')
print()

print("=== Full hex dump 0x4080AA20 - 0x4080AA50 ===")
hexdump(0x4080AA20, 0x30)
print()

print("=== Full hex dump 0x4080AB50 - 0x4080AB70 ===")
hexdump(0x4080AB50, 0x20)
print()

print("=== Full hex dump 0x4080AFB0 - 0x4080B010 ===")
hexdump(0x4080AFB0, 0x60)
print()

# Also verify: bytes at the trace start (RESET vector area)
print("=== Bytes at RESET (0x408000BA) and after ===")
hexdump(0x408000B8, 0x30)

import struct
from capstone import Cs, CS_ARCH_M68K, CS_MODE_BIG_ENDIAN

rom = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'

with open(rom, 'rb') as f:
    data = f.read()

md = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN)

def read_word(off):
    return struct.unpack('>H', data[off:off+2])[0]

def read_long(off):
    return struct.unpack('>I', data[off:off+4])[0]

def to_file(addr):
    return addr - 0x40800000

def dump_bytes(addr, count):
    off = to_file(addr)
    return data[off:off+count]

def disasm_area(addr, size):
    off = to_file(addr)
    result = []
    for i in md.disasm(data[off:off+size], addr):
        raw = ' '.join(f'{b:02X}' for b in i.bytes)
        result.append(f'  0x{i.address:08X}: {raw:16s} {i.mnemonic:8s} {i.op_str}')
    return result

print("=== Raw bytes at BRA.L (0x4080AB5A) ===")
bra_bytes = dump_bytes(0x4080AB5A, 6)
print(f'  Bytes: {" ".join(f"{b:02X}" for b in bra_bytes)}')
print(f'  Opcode: 0x{bra_bytes[0]:02X}{bra_bytes[1]:02X}')
disp32 = struct.unpack('>i', bytes(bra_bytes[2:6]))[0]
disp32_u = struct.unpack('>I', bytes(bra_bytes[2:6]))[0]
target = 0x4080AB5A + 6 + disp32
print(f'  Disp32: {disp32} (0x{disp32_u:08X})')
print(f'  Target: 0x{target:08X}')
print()

print("=== Bytes at 0x4080AB5C (displacement) ===")
d = dump_bytes(0x4080AB5C, 4)
print(f'  Hex: {" ".join(f"{b:02X}" for b in d)}')
print(f'  As big-endian signed: {struct.unpack(">i", d)[0]}')
print(f'  As big-endian unsigned: {struct.unpack(">I", d)[0]}')
print()

print("=== Trampoline at 0x4080AFB0 ===")
tramp = dump_bytes(0x4080AFB0, 8)
print(f'  Bytes: {" ".join(f"{b:02X}" for b in tramp)}')
for i in md.disasm(tramp, 0x4080AFB0):
    raw = ' '.join(f'{b:02X}' for b in i.bytes)
    print(f'  0x{i.address:08X}: {raw:16s} {i.mnemonic:8s} {i.op_str}')
print()

print("=== Bytes at BRA.L target 0x4080AFB8 ===")
target_bytes = dump_bytes(0x4080AFB8, 16)
print(f'  Bytes: {" ".join(f"{b:02X}" for b in target_bytes)}')
for i in md.disasm(target_bytes, 0x4080AFB8):
    raw = ' '.join(f'{b:02X}' for b in i.bytes)
    print(f'  0x{i.address:08X}: {raw:16s} {i.mnemonic:8s} {i.op_str}')
print()

print("=== Full disassembly 0x4080AB4C - 0x4080AB70 ===")
for line in disasm_area(0x4080AB4C, 0x24):
    print(line)
print()

print("=== Disassembly 0x4080AF90 - 0x4080B010 ===")
for line in disasm_area(0x4080AF90, 0x80):
    print(line)
print()

print("=== Disassembly 0x4080AA24 - 0x4080AA50 ===")
for line in disasm_area(0x4080AA24, 0x2C):
    print(line)
print()

# Now check: what does Capstone think BRA.L is?
print("=== Capstone decoding of 60 FF 00 00 04 58 ===")
test_bytes = bytes([0x60, 0xFF, 0x00, 0x00, 0x04, 0x58])
for i in md.disasm(test_bytes, 0x40000000):
    raw = ' '.join(f'{b:02X}' for b in i.bytes)
    print(f'  0x{i.address:08X}: {raw:16s} {i.mnemonic:8s} {i.op_str}')
print()

# Check: what about Capstone decoding just 60FF?
print("=== Capstone decoding of just 60 FF ===")
test_bytes = bytes([0x60, 0xFF])
for i in md.disasm(test_bytes, 0x40000000):
    raw = ' '.join(f'{b:02X}' for b in i.bytes)
    print(f'  0x{i.address:08X}: {raw:16s} {i.mnemonic:8s} {i.op_str}')
print()

# Check what's at 0x4080AB52-0x4080AB60
print("=== Disassembly 0x4080AB50 - 0x4080AB68 (careful) ===")
for line in disasm_area(0x4080AB50, 0x18):
    print(line)

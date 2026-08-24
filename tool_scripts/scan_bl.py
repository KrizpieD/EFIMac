import struct, sys

ROM = r"C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin"
data = open(ROM, "rb").read()
ROM_BASE = 0x40800000

def s32(v):
    v &= 0xFFFFFFFF
    if v & 0x80000000:
        v -= 0x100000000
    return v

# Decode opcode-18 (b/bl) targets. pc = guest addr of the instruction.
def b_target(w, pc):
    if (w >> 26) != 18:
        return None
    aa = (w >> 30) & 1
    lk = (w >> 31) & 1
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    if aa:
        return li, lk
    return (pc + li) & 0xFFFFFFFF, lk

targets_of_interest = {0x40B10018, 0x40B1001C, 0x40B126E8, 0x40B104A8, 0x40B10000}
hits = {}
for o in range(0, len(data)-3, 4):
    w = struct.unpack_from(">I", data, o)[0]
    pc = ROM_BASE + o
    t = b_target(w, pc)
    if t is None:
        continue
    target, lk = t
    if target in targets_of_interest and lk:
        hits.setdefault(target, []).append((pc, w))
    # also plain branch (no link) to the trampoline entry
    if target == 0x40B10018 and not lk:
        hits.setdefault("b->0x40B10018", []).append((pc, w))
    if target == 0x40B1001C and not lk:
        hits.setdefault("b->0x40B1001C", []).append((pc, w))

for k, v in sorted(hits.items(), key=lambda kv: (kv[0] if isinstance(kv[0], str) else 0x1000000000)):
    print(f"=== callers of {k:#x} ({len(v)}) ===")
    for pc, w in v[:40]:
        print(f"  0x{pc:08X}: 0x{w:08X}")
    print()

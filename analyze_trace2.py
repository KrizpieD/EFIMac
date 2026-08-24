import struct

ROM_PATH = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'
ROM_BASE = 0x40800000

with open(ROM_PATH, 'rb') as f:
    rom = f.read()

def read8(addr):
    return rom[addr - ROM_BASE]

def read16(addr):
    off = addr - ROM_BASE
    return struct.unpack('>H', rom[off:off+2])[0]

def read32(addr):
    off = addr - ROM_BASE
    return struct.unpack('>I', rom[off:off+4])[0]

# Verify BVC.L at 0x4080AFB8 displacement
print("=== BVC.L at 0x4080AFB8 ===")
opcode = read16(0x4080AFB8)
disp_hi = read16(0x4080AFBA)
disp_lo = read16(0x4080AFBC)
disp32 = (disp_hi << 16) | disp_lo
disp32s = struct.unpack('>i', struct.pack('>I', disp32))[0]
ret_pc = 0x4080AFBE  # opcode + 6
target = ret_pc + disp32s
print(f"  Opcode: 0x{opcode:04X}")
print(f"  Displacement: 0x{disp32:08X} ({disp32s})")
print(f"  ReturnPC: 0x{ret_pc:08X}")
print(f"  Target: 0x{target:08X}")
print(f"  In DR range: {0x30500000 <= target < 0x30600000}")

# Verify BVC.L at 0x4080AB70 displacement
print("\n=== BVC.L at 0x4080AB70 ===")
opcode2 = read16(0x4080AB70)
disp_hi2 = read16(0x4080AB72)
disp_lo2 = read16(0x4080AB74)
disp32_2 = (disp_hi2 << 16) | disp_lo2
disp32_2s = struct.unpack('>i', struct.pack('>I', disp32_2))[0]
ret_pc2 = 0x4080AB76
target2 = ret_pc2 + disp32_2s
print(f"  Opcode: 0x{opcode2:04X}")
print(f"  Displacement: 0x{disp32_2:08X} ({disp32_2s})")
print(f"  ReturnPC: 0x{ret_pc2:08X}")
print(f"  Target: 0x{target2:08X}")
print(f"  In DR range: {0x30500000 <= target2 < 0x30600000}")

# Now trace what happens at step [29] - the BVC.L at 0x4080AB70
# The trace shows it's taken (V=0) and goes to 0x4080AA3C
# But 0x4080AA3C != A6 (0x4080AB60)
# Let's check: is the BVC.L at 0x4080AB70 even the right instruction?
print("\n=== Checking 0x4080AB70 instructions ===")
for i in range(8):
    addr = 0x4080AB70 + i*2
    w = read16(addr)
    print(f"  0x{addr:08X}: 0x{w:04X}")

# Check if the BVC.L at 0x4080AB70 might NOT be intercepted
# Maybe the trace shows the branch NOT taken?
# Step [29]: SR=0x2708, V=0 -> BVC IS taken
# But what if the BVC target is NOT in DR range?
print(f"\n  Target 0x{target2:08X} is {'in DR range' if 0x30500000 <= target2 < 0x30600000 else 'NOT in DR range'}")

# Maybe the issue is that there are TWO BVC.L instructions and the second one 
# at step [29] goes to 0x4080AA3C because A6 was changed?
# Let me check: what sets A6 to 0x4080AA3C?
# Step [19]: LEA at 0x4080AA32 sets A6 = PC_of_ext + d16 = 0x4080AA34 + 8 = 0x4080AA3C
print("\n=== Checking if A6=0x4080AA3C at step [29] ===")
# LEA at step [19] (0x4080AA32): EA = PC_of_ext + d16
pc_of_ext_19 = 0x4080AA34  # opcode(0x4080AA32) + 2
d16_19 = read16(0x4080AA34)
if d16_19 & 0x8000:
    d16_19s = d16_19 - 0x10000
else:
    d16_19s = d16_19
a6_19 = pc_of_ext_19 + d16_19s
print(f"  LEA at 0x4080AA32: PC_of_ext=0x{pc_of_ext_19:08X}, d16={d16_19s}, A6=0x{a6_19:08X}")

# So A6 from step [19] = 0x4080AA3C!
# But then step [23] LEA at 0x4080AB56 sets A6 = 0x4080AB60
# So at step [29], A6 should be 0x4080AB60 (from step [23])
# Unless the BVC.L interception at step [25] changed A6 somehow

# Wait - the BVC.L interception at step [25] pushes ReturnPC and redirects to A6
# But it doesn't MODIFY A6. It just READS A6.
# So A6 should still be 0x4080AB60 at step [29]

# But step [30] shows PC=0x4080AA3C!
# This means either:
# 1. A6 was changed to 0x4080AA3C between steps [23] and [29]
# 2. The BVC.L at step [29] was NOT intercepted (branch not taken or target not in DR range)
# 3. There's a bug in the interception code

# Let me check option 2: maybe the BVC.L condition is different
# Step [28]: CMP.L at 0x4080AB68
# Step [29]: BVC.L at 0x4080AB70, SR=0x2708
# SR=0x2708: CCR = 0x08 = N=1, Z=0, V=0, C=0
# BVC = branch if V=0 -> TAKEN

# But wait - maybe the BVC.L at 0x4080AB70 is NOT a BVC.L!
# Let me check the opcode more carefully
print("\n=== Checking opcode at 0x4080AB70 ===")
op = read16(0x4080AB70)
print(f"  Opcode: 0x{op:04X} = {op:016b}")
# BVC.L: 0110 1000 xxxx xxxx (BVC = condition 8)
# 0x68FF = 0110 1000 1111 1111
# condition = (0x68FF >> 8) & 0xF = 0x68 >> 0 & 0xF... 
# Actually: bits 15-12 = 0110 (Bcc), bits 11-8 = condition
cond = (op >> 8) & 0xF
print(f"  Condition: 0x{cond:X} (8=BVC, 9=BVS)")
print(f"  Is BVC: {cond == 8}")
print(f"  Is BVS: {cond == 9}")

# Check the BRA.L at step [24] (0x4080AB5A) to understand the flow
print("\n=== BRA.L at 0x4080AB5A ===")
op_bra = read16(0x4080AB5A)
disp_bra_hi = read16(0x4080AB5C)
disp_bra_lo = read16(0x4080AB5E)
disp_bra = (disp_bra_hi << 16) | disp_bra_lo
disp_bra_s = struct.unpack('>i', struct.pack('>I', disp_bra))[0]
ret_bra = 0x4080AB5E  # opcode+2 + 4 for 32-bit displacement
target_bra = ret_bra + disp_bra_s
print(f"  ReturnPC: 0x{ret_bra:08X}")
print(f"  Target: 0x{target_bra:08X}")

# BRA.L at step [111] (0x4080AB5A) - same instruction
print(f"  (used at both step [24] and step [111])")

# Now check: after the BVC.L interception at step [25], the code goes to A6=0x4080AB60
# Step [26]: PC=0x4080AB60, Op=0x2C49
print("\n=== Opcode handler at 0x4080AB60 ===")
for i in range(16):
    addr = 0x4080AB60 + i*2
    w = read16(addr)
    print(f"  0x{addr:08X}: 0x{w:04X}")

# Step [26] at 0x4080AB60: Op=0x2C49 = MOVE.L (A1),D6
# Step [27] at 0x4080AB62: Op=0x6704 = BEQ.S +4
# Step [28] at 0x4080AB68: Op=0x0CB0 = CMP.L #imm,(A0)+
# Step [29] at 0x4080AB70: Op=0x68FF = BVC.L

# Wait - step [27] is BEQ.S +4 at 0x4080AB62
# BEQ.S +4 -> target = 0x4080AB62 + 2 + 4 = 0x4080AB68
# But step [28] is at 0x4080AB68!
# So the BEQ at step [27] IS taken (Z=1 from step [26])
# And it goes to 0x4080AB68 (CMP.L)

# Actually wait - BEQ.S +4 means branch to PC+2+offset = 0x4080AB64 + 4 = 0x4080AB68
# Hmm, no: BEQ.S at 0x4080AB62, offset = 4
# Target = (opcode+2) + offset = 0x4080AB64 + 4 = 0x4080AB68
# Step [28] is at 0x4080AB68 - so BEQ IS taken

# But wait - step [26] shows SR=0x2704 (Z=1), step [27] shows SR=0x2704 (Z=1)
# So Z=1 at step [27], BEQ is taken -> goes to 0x4080AB68
# Then CMP.L at step [28] sets flags, step [29] shows SR=0x2708 (N=1, Z=0, V=0)
# BVC at step [29] with V=0 -> TAKEN

# The BVC.L at 0x4080AB70 targets 0x305A0B66 (DR range)
# So interception fires: push ReturnPC=0x4080AB76, redirect to A6

# But step [30] shows PC=0x4080AA3C, not A6 (0x4080AB60)!
# This means A6 = 0x4080AA3C at step [29]

# But LEA at step [23] set A6 = 0x4080AB60
# And no instruction between [23] and [29] modifies A6

# UNLESS... the BVC.L interception at step [25] modified A6!
# Let me re-read the interception code:
# if (M68kIsDrEmulatorAddress(TargetPC)) {
#     M68kPushLong(ReturnPC);
#     TargetPC = g_M68kContext.A[6];
# }
# g_M68kContext.PC = TargetPC;

# This doesn't modify A6. It just reads A6.

# So A6 should be 0x4080AB60 at step [29].
# But step [30] shows PC=0x4080AA3C.

# WAIT - maybe the BVC.L at step [29] is NOT the one at 0x4080AB70!
# Maybe the BEQ at step [27] was NOT taken, and the code continued differently!

# Let me re-check: step [27] BEQ.S +4 at 0x4080AB62
# If BEQ is NOT taken, PC goes to 0x4080AB64 (next instruction after BEQ)
# What's at 0x4080AB64?
op_ab64 = read16(0x4080AB64)
print(f"\n  0x4080AB64: 0x{op_ab64:04X}")

# Hmm, but the trace clearly shows step [28] at 0x4080AB68, not 0x4080AB64
# So BEQ IS taken

# I'm confused. Let me just check what's at 0x4080AA3C
print("\n=== Code at 0x4080AA3C (step [30]) ===")
for i in range(8):
    addr = 0x4080AA3C + i*2
    w = read16(addr)
    print(f"  0x{addr:08X}: 0x{w:04X}")

# And what's at 0x4080AB76 (ReturnPC of BVC.L at 0x4080AB70)
print("\n=== Code at 0x4080AB76 (expected ReturnPC) ===")
for i in range(8):
    addr = 0x4080AB76 + i*2
    w = read16(addr)
    print(f"  0x{addr:08X}: 0x{w:04X}")

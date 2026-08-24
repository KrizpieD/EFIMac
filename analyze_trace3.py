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

# Read the BRA.L displacement bytes directly
print("=== BRA.L at 0x4080AB5A - displacement bytes ===")
print(f"  0x4080AB5A: {read16(0x4080AB5A):04X} (opcode)")
print(f"  0x4080AB5C: {read16(0x4080AB5C):04X} (disp high)")
print(f"  0x4080AB5E: {read16(0x4080AB5E):04X} (disp low)")
disp_bra = (read16(0x4080AB5C) << 16) | read16(0x4080AB5E)
disp_bra_s = struct.unpack('>i', struct.pack('>I', disp_bra))[0]
# The interpreter: g_M68kContext.PC = opcode+2 = 0x4080AB5C
# ReturnPC = g_M68kContext.PC + 4 = 0x4080AB60
ret_bra_correct = 0x4080AB60
target_bra = ret_bra_correct + disp_bra_s
print(f"  Displacement: 0x{disp_bra:08X} ({disp_bra_s})")
print(f"  ReturnPC (correct): 0x{ret_bra_correct:08X}")
print(f"  Target: 0x{target_bra:08X}")

# Hmm, let me check - maybe the BRA.L at 0x4080AB5A is NOT what I think.
# The trace shows step [24] at 0x4080AB5A, Op=0x60FF
# But the opcode handler dump shows 0x4080AB60: 0x2C49
# So 0x4080AB5A is BEFORE the opcode handler entry

# Let me read bytes around 0x4080AB5A
print("\n=== Bytes around 0x4080AB5A ===")
for addr in range(0x4080AB50, 0x4080AB70, 2):
    w = read16(addr)
    print(f"  0x{addr:08X}: 0x{w:04X}")

# Now the KEY question: at step [29], BVC.L at 0x4080AB70 fires
# The trace shows step [30] at 0x4080AA3C
# But if interception fires, it should redirect to A6=0x4080AB60
# 
# UNLESS: the BVC.L at step [29] is NOT intercepted!
# Maybe the branch is NOT taken despite V=0?
# Or maybe there's a different instruction at step [29]?
#
# Actually, wait - let me re-read the trace more carefully:
# Step [29]: PC=0x4080AB70 Op=0x68FF SP=0x9FFC SR=0x2708
# Step [30]: PC=0x4080AA3C Op=0x0800 SP=0x9FF8 SR=0x2708
#
# SP changed from 0x9FFC to 0x9FF8! That's a PUSH of 4 bytes.
# So the BVC.L interception DID fire (it pushes ReturnPC).
# But the target is 0x4080AA3C, not A6 (0x4080AB60)!
#
# This means A6 = 0x4080AA3C at step [29].
# But LEA at step [23] set A6 = 0x4080AB60.
#
# UNLESS: the BVC.L interception at step [25] MODIFIED A6!
# Let me re-read the interception code...

print("\n=== CRITICAL: SP change at step [29] ===")
print("Step [29]: SP=0x9FFC, Step [30]: SP=0x9FF8")
print("SP changed by -4 (push). So BVC.L interception DID fire.")
print("But target is 0x4080AA3C, not A6 (0x4080AB60)")
print()
print("This means A6 = 0x4080AA3C at step [29]!")
print("But LEA at step [23] set A6 = 0x4080AB60")
print()
print("HYPOTHESIS: The BVC.L interception at step [25] reads A6 BEFORE")
print("the LEA at step [23] sets it. But that's impossible since [23] < [25].")
print()
print("ALTERNATIVE: Maybe the opcode at step [29] is NOT the BVC.L at 0x4080AB70.")
print("Maybe the BEQ at step [27] was NOT taken, and the code went somewhere else.")

# Check: if BEQ at step [27] (0x4080AB62) is NOT taken,
# what instruction follows?
print("\n=== If BEQ NOT taken ===")
print(f"BEQ.S at 0x4080AB62 with offset +4")
print(f"Next instruction (not taken): 0x4080AB64")
print(f"  0x4080AB64: 0x{read16(0x4080AB64):04X}")
# This is BRA with offset 0 (16-bit displacement follows)
# BRA.S offset 0 -> read 16-bit displacement from 0x4080AB66
bra_disp = (INT16 := struct.unpack('>h', struct.pack('>H', read16(0x4080AB66)))[0])
print(f"  0x4080AB66: 0x{read16(0x4080AB66):04X} (BRA displacement)")
bra_target = 0x4080AB68 + bra_disp
print(f"  BRA target: 0x{bra_target:08X}")

# Hmm wait, let me re-check. The trace shows:
# [26] PC=0x4080AB60 Op=0x2C49 SR=0x2704 (Z=1)
# [27] PC=0x4080AB62 Op=0x6704 SR=0x2704 (Z=1)
# [28] PC=0x4080AB68 Op=0x0CB0 SR=0x2704
#
# BEQ at [27] with Z=1 -> TAKEN -> target = 0x4080AB68
# Step [28] is at 0x4080AB68 -> CONFIRMED BEQ taken

# But what if the trace is wrong about step [29]?
# What if step [29] is NOT at 0x4080AB70?

# Actually, let me re-read the trace line:
# Line 760: 68K[29] PC=0x4080AB70 Op=0x68FF SP=0x00009FFC SR=0x2708
# This is clearly 0x4080AB70, Op=0x68FF (BVC.L)

# So the BVC.L at 0x4080AB70 fires, interception pushes ReturnPC,
# and redirects to A6. But A6 = 0x4080AA3C, not 0x4080AB60.

# Wait - maybe I need to check if the LEA at step [23] actually executes correctly.
# Let me verify: what does the LEA handler compute?

# LEA at 0x4080AB56: LEA (d16,PC), A6
# g_M68kContext.PC after fetch = 0x4080AB58
# M68kComputeEA for mode 7, reg 2 (d16,PC):
#   PCOfExt = g_M68kContext.PC = 0x4080AB58
#   Disp = read16(0x4080AB58) = 8
#   EA = 0x4080AB58 + 8 = 0x4080AB60
# So A6 = 0x4080AB60

# But the BVC.L interception at step [29] redirects to 0x4080AA3C.
# This means A6 = 0x4080AA3C at step [29].

# The ONLY way A6 could be 0x4080AA3C is if the LEA at step [19] (0x4080AA32)
# set A6 = 0x4080AA3C and the LEA at step [23] (0x4080AB56) did NOT override it.

# But step [23] comes AFTER step [19]! So A6 should be 0x4080AB60 after step [23].

# UNLESS: the BVC.L interception at step [25] reads A6 BEFORE step [23] sets it.
# But that's impossible since [23] < [25] in the trace.

# Wait, maybe the BVC.L interception at step [25] sets PC = A6 = 0x4080AB60,
# and then the code at 0x4080AB60 executes. But the BVC.L interception at step [29]
# reads A6 AGAIN. If A6 was not modified between steps [25] and [29], it should still
# be 0x4080AB60.

# But step [30] shows PC=0x4080AA3C. So A6 = 0x4080AA3C at step [29].

# UNLESS: the instruction at step [25] is NOT the BVC.L interception!
# Maybe the BVC.L at step [25] is NOT taken (V=1)?

# Step [24]: BRA.L at 0x4080AB5A -> goes to 0x4080AFB8
# Step [25]: BVC.L at 0x4080AFB8, SR=0x2704
# SR=0x2704: CCR = 0x04 = Z=1, V=0
# BVC (V=0) -> TAKEN

# But wait - the BRA.L at step [24] targets 0x4080AFB8. But the Python script
# said the BRA.L targets 0x4080AFB6. Let me re-check.

print("\n=== RE-CHECKING BRA.L target ===")
# The BRA.L at 0x4080AB5A:
# g_M68kContext.PC = 0x4080AB5C (after fetch)
# For 32-bit disp: ReturnPC = g_M68kContext.PC + 4 = 0x4080AB60
# Wait no - the code says:
#   ReturnPC = g_M68kContext.PC + 4;
# But g_M68kContext.PC is at opcode+2 = 0x4080AB5C
# So ReturnPC = 0x4080AB5C + 4 = 0x4080AB60? No wait...
# 
# Actually, looking at the code more carefully:
#   UINT32 ReturnPC = g_M68kContext.PC;  // opcode+2
#   ...
#   if (Offset8 == (INT8)0xFF) {
#       UINT16 Hi = M68kFetchWord(g_M68kContext.PC);
#       UINT16 Lo = M68kFetchWord(g_M68kContext.PC + 2);
#       INT32 Disp32 = ...;
#       ReturnPC = g_M68kContext.PC + 4;
#       TargetPC = ReturnPC + (UINT32)Disp32;
#   }
#
# g_M68kContext.PC = 0x4080AB5C (opcode+2)
# ReturnPC = 0x4080AB5C + 4 = 0x4080AB60
# Hi = read16(0x4080AB5C), Lo = read16(0x4080AB5E)
# But wait - g_M68kContext.PC is NOT incremented by the fetch!
# The code reads from g_M68kContext.PC without incrementing.
# So Hi = read16(0x4080AB5C), Lo = read16(0x4080AB5E)

hi = read16(0x4080AB5C)
lo = read16(0x4080AB5E)
disp32 = (hi << 16) | lo
disp32_s = struct.unpack('>i', struct.pack('>I', disp32))[0]
ret_pc = 0x4080AB60
target = ret_pc + disp32_s
print(f"  Hi=0x{hi:04X}, Lo=0x{lo:04X}")
print(f"  Disp32=0x{disp32:08X} ({disp32_s})")
print(f"  ReturnPC=0x{ret_pc:08X}")
print(f"  Target=0x{target:08X}")

# Hmm wait, the BVC.L at 0x4080AFB8 also reads from the same area?
# No, the BVC.L displacement is at 0x4080AFBA and 0x4080AFBC.

# The BRA.L displacement at 0x4080AB5C/AB5E:
# These bytes are: read16(0x4080AB5C) and read16(0x4080AB5E)
# Let me check what's actually there
print(f"\n  0x4080AB5C raw: {rom[0x4080AB5C - ROM_BASE]:02X} {rom[0x4080AB5C - ROM_BASE + 1]:02X}")
print(f"  0x4080AB5E raw: {rom[0x4080AB5E - ROM_BASE]:02X} {rom[0x4080AB5E - ROM_BASE + 1]:02X}")

# Hmm, but the BVC.L displacement at 0x4080AFBA/AFBC is different
print(f"\n  0x4080AFBA raw: {rom[0x4080AFBA - ROM_BASE]:02X} {rom[0x4080AFBA - ROM_BASE + 1]:02X}")
print(f"  0x4080AFBC raw: {rom[0x4080AFBC - ROM_BASE]:02X} {rom[0x4080AFBC - ROM_BASE + 1]:02X}")

# These should be different! The BRA.L and BVC.L have different displacements.
# But the Python script earlier said both have displacement 0xEFD00070.
# That was reading from the wrong addresses!

# Let me check: the first Python script read from 0x4080AFBA for the BVC.L at 0x4080AFB8.
# And for the BRA.L, it should read from 0x4080AB5C.
# But the script might have read the wrong addresses.

# Actually, looking at the first Python script output:
# "BVC.L at 0x4080AFB8: opcode=0x68FF disp=-271581072"
# "BVC.L at 0x4080AB70: opcode=0x68FF disp=-271581072 target=0x3050ABE6"
# Both have the same displacement! That's suspicious.

# But the BRA.L at 0x4080AB5A should have a DIFFERENT displacement.
# The first script might have incorrectly read the BRA.L displacement as the BVC.L displacement.

print("\n=== Final check: what does BRA.L at 0x4080AB5A ACTUALLY target? ===")
print(f"  BRA.L opcode at 0x4080AB5A: 0x{read16(0x4080AB5A):04X}")
print(f"  Disp high at 0x4080AB5C: 0x{read16(0x4080AB5C):04X}")
print(f"  Disp low at 0x4080AB5E: 0x{read16(0x4080AB5E):04X}")
print(f"  Displacement: 0x{disp32:08X} ({disp32_s})")
print(f"  ReturnPC: 0x{ret_pc:08X}")
print(f"  Target: 0x{target:08X}")
print(f"  Expected (trace): 0x4080AFB8")
print(f"  Match: {target == 0x4080AFB8}")

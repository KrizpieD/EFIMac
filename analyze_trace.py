import struct

ROM_PATH = r'C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin'
ROM_BASE = 0x40800000

with open(ROM_PATH, 'rb') as f:
    rom = f.read()

def read16(addr):
    off = addr - ROM_BASE
    return struct.unpack('>H', rom[off:off+2])[0]

def read32(addr):
    off = addr - ROM_BASE
    return struct.unpack('>I', rom[off:off+4])[0]

# Key analysis of the trace flow
print("=== TRACE FLOW ANALYSIS ===")
print()
print("Step [25] BVC.L at 0x4080AFB8 -> interception fires")
print("  Pushes ReturnPC, redirects to A6")
print("  Step [26] PC=0x4080AB60 (so A6=0x4080AB60 at this point)")
print()

# LEA at step [23] (0x4080AB56): LEA d16(PC),A6
pc_after_lea = 0x4080AB56 + 4  # = 0x4080AB5A
disp = read16(0x4080AB58)
if disp & 0x8000:
    disp = disp - 0x10000
a6_from_lea = pc_after_lea + disp
print(f"LEA at 0x4080AB56: PC_after=0x{pc_after_lea:08X}, d16={disp} (0x{disp & 0xFFFF:04X})")
print(f"  A6 should be = 0x{a6_from_lea:08X}")
print(f"  But trace shows BVC.L redirected to 0x4080AB60")
print(f"  Discrepancy: 0x{a6_from_lea:08X} vs 0x4080AB60 = {a6_from_lea - 0x4080AB60} bytes")
print()

# Check the other LEA at step [19] (0x4080AA32)
pc_after_lea2 = 0x4080AA32 + 4  # = 0x4080AA36
disp2 = read16(0x4080AA34)
if disp2 & 0x8000:
    disp2 = disp2 - 0x10000
a6_from_lea2 = pc_after_lea2 + disp2
print(f"LEA at 0x4080AA32: PC_after=0x{pc_after_lea2:08X}, d16={disp2} (0x{disp2 & 0xFFFF:04X})")
print(f"  A6 = 0x{a6_from_lea2:08X}")
print()

# Check LEA at step [56] (0x4080AA78)
pc_after_lea3 = 0x4080AA78 + 4  # = 0x4080AA7C
disp3 = read16(0x4080AA7A)
if disp3 & 0x8000:
    disp3 = disp3 - 0x10000
a6_from_lea3 = pc_after_lea3 + disp3
print(f"LEA at 0x4080AA78: PC_after=0x{pc_after_lea3:08X}, d16={disp3} (0x{disp3 & 0xFFFF:04X})")
print(f"  A6 = 0x{a6_from_lea3:08X}")
print()

# Check what A6 is at step [30] - the BVC.L at step [29] redirects to A6
# Step [30] shows PC=0x4080AA3C
# 0x4080AA3C is the instruction right after the BRA.L at step [20] (0x4080AA36)
# Wait, BRA.L at 0x4080AA36 has ReturnPC = 0x4080AA3C
# But BRA doesn't push ReturnPC. So how does step [30] get to 0x4080AA3C?
# Unless the BVC.L interception at step [29] redirected to A6=0x4080AA3C
# But that doesn't match any LEA result...
# Unless A6 was set by step [19] LEA: A6 = 0x4080AA3E (pc_after=0x4080AA36 + 8)
# 0x4080AA3E is close to 0x4080AA3C but off by 2
# This is the SAME 2-byte discrepancy pattern!

print("=== KEY DISCOVERY: 2-byte LEA discrepancy pattern ===")
print(f"LEA at 0x4080AA32: computed A6=0x{a6_from_lea2:08X}, trace shows 0x4080AA3C (diff={a6_from_lea2 - 0x4080AA3C})")
print(f"LEA at 0x4080AB56: computed A6=0x{a6_from_lea:08X}, trace shows 0x4080AB60 (diff={a6_from_lea - 0x4080AB60})")
print()

# Both are off by 2! The LEA handler is computing PC_before instead of PC_after
# Or: the LEA handler uses PC which is already past the instruction
# In the interpreter, g_M68kContext.PC = opcode_addr + 2 when the handler runs
# For LEA (d16,PC), EA = PC + d16 where PC = opcode_addr + 2
# But if the handler uses the PC after fetch (opcode+2), and the displacement
# is added to that, we get: EA = (opcode+2) + d16
# 
# Correct: EA = (opcode+4) + d16  [PC after full instruction]
# Our: EA = (opcode+2) + d16
# Difference = 2 bytes

print("=== ROOT CAUSE OF A6 DISCREPANCY ===")
print("The LEA (d16,PC) handler likely uses g_M68kContext.PC which is opcode+2")
print("But the correct EA for (d16,PC) should use PC = opcode + instruction_size")
print("For LEA (d16,PC): instruction = 4 bytes (opcode + disp16)")
print("So correct EA = (opcode + 4) + d16")
print("But if handler uses g_M68kContext.PC = opcode + 2, then:")
print("  EA = (opcode + 2) + d16 = correct_EA - 2")
print("This explains the 2-byte discrepancy!")
print()

# Now analyze the crash flow
print("=== CRASH FLOW ANALYSIS ===")
print()
print("Stack at step [131] (JMP(A6) at 0x4080AFD8):")
print("  SP = 0x00009FF0")
print("  [0x9FF0] = BVC.L ReturnPC from step [112] = 0x4080AFBE (VALID)")
print()
print("JMP(A6) at step [131] pushes return addr 0x4080AFDA:")
print("  SP = 0x00009FEC")
print("  [0x9FEC] = 0x4080AFDA (GARBAGE - data area, contains 0x0000)")
print("  [0x9FF0] = 0x4080AFBE (valid)")
print()
print("DR stub runs, JMP(A4)==0 -> RTS pops [0x9FEC] = 0x4080AFDA")
print("  Execution falls into data area -> crash")
print()
print("FIX: When JMP(A4)==0 -> RTS, validate popped address.")
print("  If invalid (points to 0x0000 data), pop again to get BVC.L ReturnPC")
print()

# Check what's at 0x4080AFDA
opcode_at_0x4080afda = read16(0x4080AFDA)
print(f"Opcode at 0x4080AFDA: 0x{opcode_at_0x4080afda:04X}")
print(f"  This is {'0x0000 (data/garbage)' if opcode_at_0x4080afda == 0 else f'valid opcode 0x{opcode_at_0x4080afda:04X}'}")
print()

# Check what's at 0x4080AFBE
opcode_at_0x4080afbe = read16(0x4080AFBE)
print(f"Opcode at 0x4080AFBE: 0x{opcode_at_0x4080afbe:04X}")
print(f"  This is CMP.L #imm,D0 - VALID CODE")
print()

# Now analyze: what should the CORRECT behavior be?
print("=== CORRECT ROM BEHAVIOR (with PPC DR emulator) ===")
print()
print("1. BVC.L at 0x4080AFB8 targets DR range (0x3050B02E)")
print("2. ROM intercepts: pushes ReturnPC=0x4080AFBE, jumps to A6 (opcode handler)")
print("3. Opcode handler at 0x4080AB60 processes opcode")
print("4. Opcode handler calls JMP(A6) at 0x4080AFD8 to dispatch through DR stub")
print("5. DR stub at 0x4080AA8C checks: not DR opcode -> restores D0, JMP(A4)")
print("6. With PPC emulator: A4 points to DR return trampoline")
print("7. Return trampoline pops 0x4080AFBE from stack, returns there")
print()
print("In our case (no PPC emulator):")
print("5. DR stub checks: not DR opcode -> restores D0, JMP(A4)==0 -> RTS")
print("6. RTS pops WRONG address (0x4080AFDA from JMP(A6) push)")
print("7. Execution goes to garbage -> crash")
print()
print("The fundamental issue: JMP(A6) at step [131] is a TAIL CALL.")
print("The ROM expects the DR emulator to handle the return, not RTS.")
print("Our interception pushes a return address that the ROM doesn't expect.")

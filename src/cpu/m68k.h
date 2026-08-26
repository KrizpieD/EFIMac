#ifndef __M68K_INTERPRETER_H__
#define __M68K_INTERPRETER_H__

#include <efi.h>

// ---------------------------------------------------------------------------
// Motorola 68000 CPU context
//
// The 68K registers as the New World ROM's DR emulator expects them. The
// SheepShaver DR emulator maps 68K registers to PPC registers: D0-D7 = r8-r15,
// A0-A6 = r16-r22, A7 (USP) = r1. Our native interpreter keeps them in a
// dedicated context struct that lives alongside the PPC context.
// ---------------------------------------------------------------------------
typedef struct {
    // Diagnostics: monotonic counters that must NEVER be reset except by
    // explicit M68kInitialize. If these rewind at runtime, host memory
    // around the image is being clobbered. Placed FIRST so a stomp that
    // reaches here covers the entire context.
    UINT32 DiagGen;
    UINT32 DiagCanary;

    // Data registers D0-D7 (32-bit, accessible as byte/halfword/word)
    UINT32  D[8];

    // Address registers A0-A7 (32-bit, A7 is the USP; SSP tracked separately)
    UINT32  A[8];

    // Supervisor stack pointer (A7 in supervisor mode)
    UINT32  SSP;

    // Program counter (32-bit, but 68000 uses only lower 24 bits)
    UINT32  PC;

    // Status register (16-bit):
    //   bits 15-8: system byte (T1/T0/S/I2/I0)
    //   bits  7-0: CCR (X/N/Z/V/C/0/0/0)
    UINT16  SR;

    // Current instruction word (for debugging/tracing)
    UINT32  CurrentOpcode;

    // 1 = supervisor mode, 0 = user mode
    BOOLEAN Supervisor;

    // Set to TRUE when the interpreter encounters an unimplemented opcode
    // or an exception; PpcRunGuest can check this to stop.
    BOOLEAN Halted;

    // Set by STOP #imm: the 68K halts until an interrupt arrives. The PPC
    // hook clears this when a decrementer interrupt is pending, matching
    // real hardware where STOP resumes on an unmasked interrupt.
    BOOLEAN Stopped;
} M68K_CPU_CONTEXT;

// Global 68K CPU context
extern M68K_CPU_CONTEXT g_M68kContext;

// ---------------------------------------------------------------------------
// CCR flag bit positions (in the low byte of SR)
// ---------------------------------------------------------------------------
#define M68K_CCR_C  0x01    // Carry
#define M68K_CCR_V  0x02    // Overflow
#define M68K_CCR_Z  0x04    // Zero
#define M68K_CCR_N  0x08    // Negative
#define M68K_CCR_X  0x10    // Extend

// SR system byte bits
#define M68K_SR_T1   0x8000  // Trace mode 1
#define M68K_SR_T0   0x4000  // Trace mode 0
#define M68K_SR_S    0x2000  // Supervisor (1) / User (0)
#define M68K_SR_I2   0x0400  // Interrupt mask bit 2 (bit 10)
#define M68K_SR_I1   0x0200  // Interrupt mask bit 1 (bit 9)
#define M68K_SR_I0   0x0100  // Interrupt mask bit 0 (bit 8)

// ---------------------------------------------------------------------------
// 68K exception vector numbers
// ---------------------------------------------------------------------------
#define M68K_VEC_RESET_SSP     0x00   // Initial SSP
#define M68K_VEC_RESET_PC      0x04   // Initial PC
#define M68K_VEC_BUS_ERROR     0x08
#define M68K_VEC_ADDRESS_ERROR 0x0C
#define M68K_VEC_ILLEGAL_INSTR 0x10
#define M68K_VEC_ZERO_DIVIDE   0x14
#define M68K_VEC_CHK           0x18
#define M68K_VEC_TRAPV         0x1C
#define M68K_VEC_PRIV_VIOLATION 0x20
#define M68K_VEC_TRACE         0x24
#define M68K_VEC_LINE_1010     0x28
#define M68K_VEC_LINE_1111     0x2C
#define M68K_VEC_FORMAT_ERROR  0x38
#define M68K_VEC_UNINITIALIZED 0x3C
#define M68K_VEC_SPURIOUS      0x60
#define M68K_VEC_LEVEL1        0x64
#define M68K_VEC_LEVEL2        0x68
#define M68K_VEC_LEVEL3        0x6C
#define M68K_VEC_LEVEL4        0x70
#define M68K_VEC_LEVEL5        0x74
#define M68K_VEC_LEVEL6        0x78
#define M68K_VEC_LEVEL7        0x7C
#define M68K_VEC_TRAP0         0x80
#define M68K_VEC_TRAP1         0x84  // Mac OS system call (TRAP #1)
#define M68K_VEC_TRAP(n)       (0x80 + (n) * 4)

// Mac OS A-line trap: TRAP #A (line F = 0xA). These are the Toolbox calls.
#define M68K_VEC_ALINE         0xB0

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Initialize the 68K CPU context to reset state (reads reset vector from
// guest memory at 0x0000/0x0004).
VOID
M68kInitialize (
    VOID
    );

// Reset the 68K CPU: reload PC and SSP from the vector table at 0x0000,
// set SR to supervisor mode with interrupts masked (0x2700).
VOID
M68kReset (
    VOID
    );

// Execute a single 68K instruction from the current PC. Returns the number
// of cycles consumed (approximate). Sets g_M68kContext.Halted on fatal error.
// The PC is advanced past the instruction.
UINT32
M68kExecuteInstruction (
    VOID
    );

// Execute multiple 68K instructions (up to MaxInstructions). Returns the
// number of instructions actually executed. Stops early if Halted is set.
UINTN
M68kExecuteBlock (
    IN UINTN MaxInstructions
    );

// Called from the PPC interpreter when the PPC code enters the 68K opcode
// table (the trampoline). This reads the 68K opcode from guest memory at
// the 68K PC, decodes and executes it, then returns to the PPC DR emulator
// loop. Returns EFI_SUCCESS on success, or an error on fatal exception.
EFI_STATUS
M68kExecuteFromPPC (
    VOID
    );

// Patch the 68K opcode table (ROM+0x380000) so that regular 68K opcode
// entries branch to the C trampoline instead of the PPC DR emulator code.
// EMUL_OP entries (0xFE40+) are left as-is (they use the mulli marker
// interception already in the PPC interpreter).
VOID
M68kPatchOpcodeTable (
    VOID
    );

// Read a byte from the 68K address space (uses the PPC guest memory path).
UINT8
M68kReadByte (
    IN UINT32 Address
    );

// Read a word (16-bit, big-endian) from the 68K address space.
UINT16
M68kReadWord (
    IN UINT32 Address
    );

// Read a long (32-bit, big-endian) from the 68K address space.
UINT32
M68kReadLong (
    IN UINT32 Address
    );

// Write a byte to the 68K address space.
VOID
M68kWriteByte (
    IN UINT32 Address,
    IN UINT8  Value
    );

// Write a word (16-bit, big-endian) to the 68K address space.
VOID
M68kWriteWord (
    IN UINT32 Address,
    IN UINT16 Value
    );

// Write a long (32-bit, big-endian) to the 68K address space.
VOID
M68kWriteLong (
    IN UINT32 Address,
    IN UINT32 Value
    );

// Read a word from the 68K address space without advancing PC (for
// prefetch/peek operations).
UINT16
M68kFetchWord (
    IN UINT32 Address
    );

// Read a long from the 68K address space without advancing PC.
UINT32
M68kFetchLong (
    IN UINT32 Address
    );

// Push a word onto the 68K supervisor stack.
VOID
M68kPushWord (
    IN UINT16 Value
    );

// Push a long onto the 68K supervisor stack.
VOID
M68kPushLong (
    IN UINT32 Value
    );

// Pop a word from the 68K supervisor stack.
UINT16
M68kPopWord (
    VOID
    );

// Pop a long from the 68K supervisor stack.
UINT32
M68kPopLong (
    VOID
    );

// Set a CCR flag bit.
VOID
M68kSetFlag (
    IN UINT8 Flag
    );

// Clear a CCR flag bit.
VOID
M68kClearFlag (
    IN UINT8 Flag
    );

// Test a CCR flag bit (returns TRUE if set).
BOOLEAN
M68kTestFlag (
    IN UINT8 Flag
    );

// Set X, N, Z, V, C flags based on a result value.
// ResultSize: 1 = byte, 2 = word, 4 = long
VOID
M68kSetFlagsFromResult (
    IN UINT32 Result,
    IN UINT8  ResultSize
    );

// Set CCR from a subtraction result (result, src, dst) for proper
// carry/overflow computation.
VOID
M68kSetFlagsFromSub (
    IN UINT32 Result,
    IN UINT32 Src,
    IN UINT32 Dst,
    IN UINT8  Size
    );

// Set CCR from an addition result (result, src, dst).
VOID
M68kSetFlagsFromAdd (
    IN UINT32 Result,
    IN UINT32 Src,
    IN UINT32 Dst,
    IN UINT8  Size
    );

// Get the current SR (status register, 16-bit).
UINT16
M68kGetSR (
    VOID
    );

// Set the SR (status register, 16-bit). Handles supervisor/user stack
// switching if the S bit changes.
VOID
M68kSetSR (
    IN UINT16 Value
    );

// Get the CCR (condition code register, low 8 bits of SR).
UINT8
M68kGetCCR (
    VOID
    );

// Set the CCR (condition code register, low 8 bits of SR).
VOID
M68kSetCCR (
    IN UINT8 Value
    );

// Raise a 68K exception. Pushes the current PC and SR onto the
// supervisor stack, then vectors through the exception table.
VOID
M68kRaiseException (
    IN UINT8 VectorNumber
    );

// Decode a single 68K instruction to a mnemonic string.
VOID
M68kDecodeInstruction (
    IN  UINT16 Opcode,
    OUT CHAR16* Buffer,
    IN  UINTN   BufferSize
    );

// Synchronize 68K CPU context from PPC interpreter registers.
// Maps: PPC r8-r15 -> D0-D7, r16-r22 -> A0-A6, r1 -> A7, r24 -> PC, r25 -> SR
VOID
M68kSyncFromPPC (
    VOID
    );

// Synchronize PPC interpreter registers from 68K CPU context (reverse of above).
VOID
M68kSyncToPPC (
    VOID
    );

// Run the 68K self-test suite.
EFI_STATUS
M68kRunSelfTest (
    VOID
    );

#endif // __M68K_INTERPRETER_H__

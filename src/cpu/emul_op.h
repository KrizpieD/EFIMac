#ifndef __EMUL_OP_H__
#define __EMUL_OP_H__

#include <efi.h>

// ---------------------------------------------------------------------------
// PHASE C: EMUL_OP device handlers (SheepShaver-faithful).
//
// The ROM's 68K opcode table carries EMUL_OP extended opcodes (0xFE40+).
// When the guest executes one -- either through the DR emulator's opcode
// table (PPC-side marker interception in interpreter.c) or natively (the
// 68K interpreter's Line-F group) -- the selector dispatches here. Each
// selector implements one host-side device/service backed by UEFI
// protocols, exactly the layer SheepShaver's emul_op.cpp provides.
//
// Register conventions inside the handlers follow the DR emulator map:
//   D0-D7 = PPC r8-r15, A0-A6 = r16-r22, A7 = r1.
// ---------------------------------------------------------------------------

// Interrupt flag bits (SheepShaver InterruptFlags.h semantics)
#define INTFLAG_VIA     0x01u   // 60 Hz tick / VBL / timer aggregation
#define INTFLAG_SERIAL  0x02u
#define INTFLAG_ETHER   0x04u
#define INTFLAG_ADB     0x08u
#define INTFLAG_TIMER   0x10u
#define INTFLAG_AUDIO   0x20u

// Execute an EMUL_OP selector (>2; markers 0..2 are EXEC_* control words
// handled by the callers). Reads/writes 68K-visible state through the PPC
// register file and the guest memory path.
VOID
EmulOpDispatch (
    IN UINT32 Selector
    );

// Advance the emulated microsecond clock and raise periodic interrupt
// flags (60 Hz VIA tick). Called from the execution loops with the number
// of guest instructions executed since the previous call.
VOID
EmulOpAdvanceClock (
    IN UINTN InstructionsExecuted
    );

// Monotonic emulated microseconds since boot.
UINT64
EmulOpGetMicroseconds (
    VOID
    );

// Return and clear the pending interrupt flags.
UINT32
EmulOpGetAndClearInterruptFlags (
    VOID
    );

// Raise flags without clearing (OR-in).
VOID
EmulOpSignalInterrupt (
    IN UINT32 Flags
    );

// If a primed timer task is due, deactivate it and return its TMTask
// guest pointer; else return 0. The caller (68K loop) invokes the task's
// tmAddr with A1 = task pointer.
UINT32
EmulOpPopDueTimer (
    OUT UINT32* DelayUsRemaining
    );

#endif // __EMUL_OP_H__

#ifndef __PPC_INTERPRETER_H__
#define __PPC_INTERPRETER_H__

#include <efi.h>

// PowerPC CPU execution context.
//
// This is the register file and state that the interpreter operates on. It is
// intentionally a separate module from the public translation API so that the
// decoder/executor can be built and unit-tested on its own.
typedef struct {
    UINT32  Gpr[32];        // General Purpose Registers R0..R31
    UINT32  Cr;             // Condition Register (8 fields of 4 bits)
    UINT32  Xer;            // Fixed-point exception register (SO/OV/CA)
    UINT32  Msr;            // Machine State Register
    UINT32  Srr0;           // Save/Restore Register 0
    UINT32  Srr1;           // Save/Restore Register 1
    UINT32  Ctr;            // Count Register
    UINT32  Lr;             // Link Register
    UINT32  Pc;             // Program Counter (guest address)
    UINT32  Spr[1024];      // Special Purpose Register file
    UINT32  TimeBaseL;      // Time base lower (TBL)
    UINT32  TimeBaseH;      // Time base upper (TBU)
    UINT64  Fpr[32];        // Floating-point registers (IEEE-754 double bit patterns)
    UINT32  Fpscr;          // Floating-point status/control register (classic 32-bit layout)
    UINT32  ExceptionPending;  // 0 = none, else PPC_EXCEPTION_*
} PPC_CPU_CONTEXT;

// Global CPU context
extern PPC_CPU_CONTEXT g_PpcContext;

// Update a single 4-bit Condition Register field
VOID
PpcSetCrField (
    IN UINT32 Field,
    IN UINT32 Value
    );

// Read a single 4-bit Condition Register field
UINT32
PpcGetCrField (
    IN UINT32 Field
    );

// Set XER[CA] (carry) to the given value, preserving other XER bits
VOID
PpcSetXerCarry (
    IN UINT32 Carry
    );

// Set XER[OV] and, if set, the sticky XER[SO] bit
VOID
PpcSetXerOverflow (
    IN UINT32 Overflow
    );

#endif // __PPC_INTERPRETER_H__

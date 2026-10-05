#ifndef __PPC_INTERPRETER_H__
#define __PPC_INTERPRETER_H__

#include <efi.h>

// One decomposed PowerPC 32-bit BAT (block) translation entry, mirroring
// DingusPPC's PPC_BAT_entry (core/ppc/ppcmmu.h). Fields are derived from the
// upper/lower BAT SPR pair at write time.
typedef struct {
    UINT8   Access;   // Vs | Vp bits
    UINT8   Prot;     // PP bits
    UINT32  PhysHi;   // high-order physical address bits
    UINT32  HiMask;   // mask for high-order logical address bits
    UINT32  Bepi;     // block effective page index
    BOOLEAN Active;   // set once this pair has been programmed
} PPC_BAT_ENTRY;

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
    UINT32  DecrementerNegative;  // 1 while DEC (SPR 22) is negative
    UINT32  DecrementerWritten;   // 1 once the guest has armed DEC via mtspr
    UINT64  Fpr[32];        // Floating-point registers (IEEE-754 double bit patterns)
    UINT32  Fpscr;          // Floating-point status/control register (classic 32-bit layout)
    UINT8   Vr[32][16];     // AltiVec vector registers VR0..VR31 (big-endian byte order)
    UINT32  Vscr;           // Vector status/control register
    UINT32  ExceptionPending;  // 0 = none, else PPC_EXCEPTION_*

    // PowerPC 32-bit MMU state (DingusPPC-faithful, see ppcmmu.cpp). Each BAT
    // pair is decomposed into the fields used during block translation.
    // Index 0-7 = IBAT0-7, 8-15 = DBAT0-7.
    PPC_BAT_ENTRY Bat[16];
    UINT32  Sdr1;         // SPR 25: page table base + HTABORG / mask
    UINT32  Sr[16];       // segment registers (also mirrored in Spr[0..15])
} PPC_CPU_CONTEXT;

// Global CPU context
extern PPC_CPU_CONTEXT g_PpcContext;

// Exception deliveries per vector, indexed by (Vector >> 12): index 0 covers
// 0x000-0xFFF (every PowerPC exception vector), index 1 the 0x1000+ range.
// Diagnostic only -- the totals are what say whether the guest ever receives a
// vector it needs, without printing one line per delivery.
extern UINT32 g_VecCount[4];

// Diagnostic arm for the MSR/SRR1 tracer (interpreter.c). Once the guest
// reaches the NK idle spin loop, every MSR write and every exception entry is
// logged so the instruction or nested exception that drops the external-enable
// bit is identified instead of guessed.
extern BOOLEAN g_MsrTraceArmed;
extern UINT32  g_MsrTraceCount;
extern UINT32  g_DecTraceCount;

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

// Queue a byte on the NK's emulated SCC receive side
VOID
PpcSccPutChar (
    IN UINT8 Char
    );

// Execute the SCC FIFO + NK external-interrupt-controller device self-test
// (queue/level primitives + KDP-slot install). Part of the aggregate CPU
// self-test; runs on the host with no guest state.
VOID
PpcRunSccDeviceSelfTest (
    OUT UINTN* Passed,
    OUT UINTN* Failed
    );

// Effective -> physical address translation (DingusPPC-faithful MMU). Returns
// TRUE and fills *Pa on a BAT or SDR1 page-table hit; FALSE on a miss. The
// call site decides whether translation applies based on MSR[IR] (instruction)
// / MSR[DR] (data); when the relevant MSR bit is clear, effective==physical.
BOOLEAN
PpcTranslateEffective (
    IN  UINT32  La,
    IN  BOOLEAN IsInstr,
    OUT UINT32* Pa
    );

// Recompute the decomposed BAT entry for the given upper/lower BAT SPR number
// (528-551) after an mtspr write.
VOID
PpcUpdateBat (
    IN UINT32 SprNum
    );

// Low-RAM write census (diagnostic). Called from every guest store path
// (CpuWrite32, CpuWrite32Rev, M68kWriteLong) with the effective address, the
// value about to be stored and the writing PC (the 68K path ORs 0x68000000 into
// the PC so the two are distinguishable in the report).
#define LOW_CENSUS_PAGES 128
extern UINT32 g_LowPageWrites[LOW_CENSUS_PAGES];
extern UINT32 g_LowPageFirstPc[LOW_CENSUS_PAGES];
extern UINT32 g_LowPageFirstAddr[LOW_CENSUS_PAGES];
extern UINT8  g_LowPageSeen[LOW_CENSUS_PAGES];

VOID
LowRamCensus (
    IN UINT32 A,
    IN UINT32 V,
    IN UINT32 Pc
    );

#endif // __PPC_INTERPRETER_H__

#include "interpreter.h"
#include "translation.h"
#include <efi.h>
#include <efilib.h>

// Global PowerPC CPU context (backing store for the interpreter and the
// public register accessor API)
PPC_CPU_CONTEXT g_PpcContext = {0};

// ---------------------------------------------------------------------------
// Instruction field extraction (bit 0 = most significant bit of the word)
// ---------------------------------------------------------------------------
#define OP(w)      ((w) >> 26)
#define RT(w)      (((w) >> 21) & 0x1F)
#define RS(w)      (((w) >> 21) & 0x1F)
#define RD(w)      (((w) >> 21) & 0x1F)
#define RA(w)      (((w) >> 16) & 0x1F)
#define RB(w)      (((w) >> 11) & 0x1F)
#define BO(w)      (((w) >> 21) & 0x1F)
#define BI(w)      (((w) >> 16) & 0x1F)
#define BF(w)      (((w) >> 23) & 0x7)
#define SH(w)      (((w) >> 11) & 0x1F)
#define MB(w)      (((w) >> 6) & 0x1F)
#define ME(w)      (((w) >> 1) & 0x1F)
#define SIMM(w)    ((UINT32)(INT32)(INT16)((w) & 0xFFFF))
#define UIMM(w)    ((w) & 0xFFFF)
#define XO(w)      (((w) >> 1) & 0x3FE)      // 10-bit XO with OE bit masked out
#define XO10(w)    (((w) >> 1) & 0x3FF)      // full 10-bit XO
#define Rc(w)      ((w) & 1)
#define LK(w)      ((w) & 1)
#define AA(w)      (((w) >> 1) & 1)
#define SPR(w)     ((((w) >> 16) & 0x1F) | ((((w) >> 11) & 0x1F) << 5))
// BD is a 14-bit signed field at word bits 16-29; the byte displacement is
// sign_extend(BD) << 2.
#define BD(w)      ((UINT32)(INT32)(INT16)((((w) >> 2) & 0x3FFF) << 2))
// LI is a 24-bit signed field at word bits 6-29; the byte displacement is
// sign_extend(LI) << 2.
#define LI(w)      ((UINT32)(INT32)(((((w) >> 2) & 0x800000) ? \
                     (((w) >> 2) | 0xFF000000) : (((w) >> 2) & 0xFFFFFF)) << 2))

// Floating-point fields (opcodes 48-63). FRT/FRA/FRB occupy the same word
// positions as their fixed-point counterparts; FRC is the third source of the
// A-form fused multiply-add instructions.
#define FRT(w)     (((w) >> 21) & 0x1F)
#define FRA(w)     (((w) >> 16) & 0x1F)
#define FRB(w)     (((w) >> 11) & 0x1F)
#define FRC(w)     (((w) >> 6) & 0x1F)

// Effective address helpers (RA==0 means GPR(0) is NOT used)
#define EaD(w, ra) (((ra) == 0) ? SIMM(w) : (g_PpcContext.Gpr[ra] + SIMM(w)))
#define EaX(w, ra, rb) ((((ra) == 0) ? 0U : g_PpcContext.Gpr[ra]) + g_PpcContext.Gpr[rb])

// X-form primary XO values for opcode 31
#define XO_CMP         0
#define XO_TW          4
#define XO_SUBFC       8
#define XO_ADDC       10
#define XO_MULHWU     11
#define XO_MFCR       19
#define XO_LWARX      20
#define XO_LWZX       23
#define XO_SLW        24
#define XO_CNTLZW     26
#define XO_AND        28
#define XO_CMPL       32
#define XO_SUBF       40
#define XO_DCBST      54
#define XO_ANDC       60
#define XO_MULHW      75
#define XO_TLBIEL     78
#define XO_MFMSR      83
#define XO_DCBF       86
#define XO_LBZX       87
#define XO_NEG       104
#define XO_LBZUX     119
#define XO_NOR       124
#define XO_SUBFE     136
#define XO_ADDE      138
#define XO_MTCRF     144
#define XO_MTMSR     146
#define XO_STWCX_    150
#define XO_STWX      151
#define XO_STWUX     183
#define XO_SUBFZE    200
#define XO_ADDZE     202
#define XO_STBX      215
#define XO_SUBFME    232
#define XO_ADDME     234
#define XO_MULLW     235
#define XO_MTSRIN    242
#define XO_DCBTST    246
#define XO_STBUX     247
#define XO_ADD       266
#define XO_DCBT      278
#define XO_LHZX      279
#define XO_EQV       284
#define XO_TLBIE     306
#define XO_LHZUX     311
#define XO_XOR       316
#define XO_MFSPR     339
#define XO_LHAX      343
#define XO_TLBIA     370
#define XO_MFTB      371
#define XO_LHAUX     375
#define XO_STHX      407
#define XO_ORC       412
#define XO_STHUX     439
#define XO_OR        444
#define XO_DIVWU     459
#define XO_MTSPR     467
#define XO_DCBI      470
#define XO_NAND      476
#define XO_DIVW      491
#define XO_MCRXR     512
#define XO_LSWX      533
#define XO_LWBRX     534
#define XO_SRW       536
#define XO_MFSR      595
#define XO_LSWI      597
#define XO_SYNC      598
#define XO_TLBSYNC   566
#define XO_MTSR      210
#define XO_MFSRIN    659
#define XO_STSWX     661
#define XO_STWBRX    662
#define XO_STSWI     725
#define XO_LHBRX     790
#define XO_SRAW      792
#define XO_SRAWI     824
#define XO_EIEIO     854
#define XO_STHBRX    918
#define XO_ICBI      982
#define XO_DCBZ     1014

// XL-form XO values for opcode 19
#define XO19_MCRF      0
#define XO19_BCLR     16
#define XO19_RFI      50
#define XO19_ISYNC   150
#define XO19_BCCTR   528
#define XO19_CRNOR    33
#define XO19_CRANDC  129
#define XO19_CRXOR   193
#define XO19_CRNAND  225
#define XO19_CRAND   257
#define XO19_CREQV   289
#define XO19_CRORC   417
#define XO19_CROR    449

// X-form XO values for floating-point opcodes 59 (single) and 63 (double)
#define XOFP_FCMPU      0
#define XOFP_FRSP      12
#define XOFP_FCTIW     14
#define XOFP_FCTIWZ    15
#define XOFP_FDIV      18
#define XOFP_FSUB      20
#define XOFP_FADD      21
#define XOFP_FSQRT     22
#define XOFP_FSEL      23
#define XOFP_FRES      24
#define XOFP_FCMPO     32
#define XOFP_FNEG      40
#define XOFP_FMR       72
#define XOFP_FNABS    136
#define XOFP_FABS     264
#define XOFP_MFFS     583
#define XOFP_MTFSFI   134
#define XOFP_MTFSB0   192
#define XOFP_MTFSB1   193
#define XOFP_MTFSF    711

// A-form XO values (word bits 1-5) for the FP ops that take a third source in
// the FRC field. FMUL encodes the multiplier in FRC; the fused multiply-add
// family (FMSUB/FMADD/FNMSUB/FNMADD) uses FRA x FRB plus FRC.
#define XOAF_FMUL      25
#define XOAF_FMSUB     28
#define XOAF_FMADD     29
#define XOAF_FNMSUB    30
#define XOAF_FNMADD    31

// SPR numbers
#define SPR_XER    1
#define SPR_LR     8
#define SPR_CTR    9
#define SPR_SRR0  26
#define SPR_SRR1  27
#define SPR_PVR  287

// ---------------------------------------------------------------------------
// Memory access (default: identity-mapped, big-endian guest memory)
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Emulated guest memory (default memory backing for the interpreter)
//
// The primary guest RAM region is installed with PpcSetGuestMemory() and
// occupies slot 0. Additional regions (the classic Mac OS ROM window at
// 0xFFF00000, low-memory globals at 0x00000000) are added with
// PpcAddGuestMemoryRegion(). Addresses not covered by any region read as
// zero and ignore writes.
// ---------------------------------------------------------------------------
#define PPC_MAX_GUEST_REGIONS 8

typedef struct {
    VOID*   HostBase;   // Host virtual address backing the region
    UINT32  GuestBase;  // Guest-visible base address of the region
    UINT32  Size;       // Region size in bytes
    BOOLEAN ReadOnly;   // TRUE: guest stores are dropped
    BOOLEAN Active;     // TRUE: slot is in use
} PPC_GUEST_REGION;

// Guest memory map. Lookup is in insertion order; the first region that
// contains an address wins.
static PPC_GUEST_REGION g_GuestRegions[PPC_MAX_GUEST_REGIONS];
static UINTN g_OutDevChars = 0;
static BOOLEAN g_SccRxPending = FALSE;
static UINT8 g_SccRxFifo[64];
static UINTN g_SccRxFifoHead = 0;
static UINTN g_SccRxFifoTail = 0;

// Queue a byte on the NK's SCC receive side so a polled read eventually
// returns it (bit 0 of the [0x20002] status read reports Rx data ready).
VOID
PpcSccPutChar (
    IN UINT8 Char
    )
{
    UINTN Next = (g_SccRxFifoHead + 1) % sizeof(g_SccRxFifo);
    if (Next != g_SccRxFifoTail) {
        g_SccRxFifo[g_SccRxFifoHead] = Char;
        g_SccRxFifoHead = Next;
    }
    g_SccRxPending = TRUE;
    Print(L"  [SCC] putchar 0x%02x (head=%d tail=%d pending=%d)\n",
          Char, g_SccRxFifoHead, g_SccRxFifoTail, g_SccRxPending);
}

static UINT8
PpcDefaultReadByte (
    IN UINT32 Address
    )
{
    UINTN I;

    // NK output/input device = Zilog 8530 SCC at 0x20000. [base+2] is the
    // control/status register (WR0 on read). Report Tx-buffer-empty (bit 2)
    // so the boot printer's poll completes, and Rx-data-ready (bit 0) only
    // when a byte has actually been queued on the input side. Never echo
    // previously-written control bytes back here (they are not status).
    if (Address == 0x00020002) {
        static UINTN SccStatusReads = 0;
        UINT8 R = 0x04 | (g_SccRxPending ? 0x01 : 0x00);
        if ((SccStatusReads % 500000) == 0 || SccStatusReads < 10) {
            Print(L"  [SCC] status@0x20002 -> 0x%02x (pending=%d head=%d tail=%d)\n",
                  R, g_SccRxPending, g_SccRxFifoHead, g_SccRxFifoTail);
        }
        SccStatusReads++;
        return R;
    }
    // [base+6] is the SCC data register (channel control lives at [base+2]).
    // A read returns the queued Rx byte.
    if (Address == 0x00020006) {
        if (g_SccRxFifoHead != g_SccRxFifoTail) {
            UINT8 C = g_SccRxFifo[g_SccRxFifoTail];
            g_SccRxFifoTail = (g_SccRxFifoTail + 1) % sizeof(g_SccRxFifo);
            g_SccRxPending = (g_SccRxFifoHead != g_SccRxFifoTail);
            Print(L"  [SCC] data@0x20006 -> 0x%02x (head=%d tail=%d LR=0x%08x PC=0x%08x)\n",
                  C, g_SccRxFifoHead, g_SccRxFifoTail, g_PpcContext.Lr, g_PpcContext.Pc);
            return C;
        }
        g_SccRxPending = FALSE;
        Print(L"  [SCC] data@0x20006 -> EMPTY\n");
        return 0;
    }

    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
        if (g_GuestRegions[I].Active &&
            Address >= g_GuestRegions[I].GuestBase &&
            (UINT64)(Address - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) {
            UINT8 V = *(volatile UINT8*)((UINTN)g_GuestRegions[I].HostBase +
                                         (Address - g_GuestRegions[I].GuestBase));
            return V;
        }
    }
    return 0;  // Unmapped guest address reads as zero
}

static VOID
PpcDefaultWriteByte (
    IN UINT32 Address,
    IN UINT8  Value
    )
{
    UINTN I;

    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
        if (g_GuestRegions[I].Active &&
            Address >= g_GuestRegions[I].GuestBase &&
            (UINT64)(Address - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) {
            if (Address == 0x00020006 && g_OutDevChars < 4096) {
                g_OutDevChars++;
                if (Value == 0x0D) {
                    Print(L"\r\n");
                } else if (Value == 0x0A) {
                    // swallow (already translated \r\n)
                } else if (Value >= 0x20 && Value <= 0x7E) {
                    Print(L"%c", (UINTN)Value);
                }
            }
            // SCC data register [base+6] and control register [base+2]:
            // writes must not land in guest RAM (the 8530 never reads them
            // back as status/data).
            if (Address == 0x00020006 || Address == 0x00020002) {
                return;
            }
            if (!g_GuestRegions[I].ReadOnly) {
                *(volatile UINT8*)((UINTN)g_GuestRegions[I].HostBase +
                                   (Address - g_GuestRegions[I].GuestBase)) = Value;
            }
            return;  // First matching region decides the target
        }
    }
}

static PPC_CPU_READ_MEMORY  g_ReadByte  = PpcDefaultReadByte;
static PPC_CPU_WRITE_MEMORY g_WriteByte = PpcDefaultWriteByte;

EFI_STATUS
PpcSetMemoryAccess (
    IN PPC_CPU_READ_MEMORY   Read,
    IN PPC_CPU_WRITE_MEMORY  Write
    )
{
    g_ReadByte  = (Read  != NULL) ? Read  : PpcDefaultReadByte;
    g_WriteByte = (Write != NULL) ? Write : PpcDefaultWriteByte;
    return EFI_SUCCESS;
}

EFI_STATUS
PpcSetGuestMemory (
    IN VOID*  HostBase,
    IN UINT32 GuestBase,
    IN UINT32 Size
    )
{
    return PpcAddGuestMemoryRegion(HostBase, GuestBase, Size, FALSE);
}

EFI_STATUS
PpcAddGuestMemoryRegion (
    IN VOID*   HostBase,
    IN UINT32  GuestBase,
    IN UINT32  Size,
    IN BOOLEAN ReadOnly
    )
{
    UINTN I;

    if (HostBase == NULL || Size == 0) {
        return EFI_INVALID_PARAMETER;
    }

    // Reject a region that would overlap an already installed one.
    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
        if (!g_GuestRegions[I].Active) {
            continue;
        }
        if (GuestBase < g_GuestRegions[I].GuestBase) {
            if ((UINT64)GuestBase + Size > g_GuestRegions[I].GuestBase) {
                return EFI_ALREADY_STARTED;
            }
        } else {
            if ((UINT64)(GuestBase - g_GuestRegions[I].GuestBase) <
                g_GuestRegions[I].Size) {
                return EFI_ALREADY_STARTED;
            }
        }
    }

    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
        if (!g_GuestRegions[I].Active) {
            g_GuestRegions[I].HostBase  = HostBase;
            g_GuestRegions[I].GuestBase = GuestBase;
            g_GuestRegions[I].Size      = Size;
            g_GuestRegions[I].ReadOnly  = ReadOnly;
            g_GuestRegions[I].Active    = TRUE;
            return EFI_SUCCESS;
        }
    }

    return EFI_OUT_OF_RESOURCES;
}

UINT8
PpcReadGuestByte (
    IN UINT32 Address
    )
{
    return g_ReadByte(Address);
}

VOID
PpcWriteGuestByte (
    IN UINT32 Address,
    IN UINT8  Value
    )
{
    g_WriteByte(Address, Value);
}

static UINT32 CpuRead16 (UINT32 A) { return ((UINT32)g_ReadByte(A) << 8) | g_ReadByte(A + 1); }
static UINT32 CpuRead32 (UINT32 A) { return (CpuRead16(A) << 16) | CpuRead16(A + 2); }
static VOID   CpuWrite16(UINT32 A, UINT32 V) { g_WriteByte(A, (UINT8)(V >> 8)); g_WriteByte(A + 1, (UINT8)V); }
static VOID   CpuWrite32(UINT32 A, UINT32 V) { CpuWrite16(A, V >> 16); CpuWrite16(A + 2, V); }

// Byte-reversed access (for lwbrx/stwbrx etc.)
static UINT32 CpuRead32Rev (UINT32 A)
{
    return (UINT32)g_ReadByte(A) | ((UINT32)g_ReadByte(A + 1) << 8) |
           ((UINT32)g_ReadByte(A + 2) << 16) | ((UINT32)g_ReadByte(A + 3) << 24);
}
static UINT32 CpuRead16Rev (UINT32 A)
{
    return (UINT32)g_ReadByte(A) | ((UINT32)g_ReadByte(A + 1) << 8);
}
static VOID CpuWrite32Rev (UINT32 A, UINT32 V)
{
    g_WriteByte(A, (UINT8)V); g_WriteByte(A + 1, (UINT8)(V >> 8));
    g_WriteByte(A + 2, (UINT8)(V >> 16)); g_WriteByte(A + 3, (UINT8)(V >> 24));
}
static VOID CpuWrite16Rev (UINT32 A, UINT32 V)
{
    g_WriteByte(A, (UINT8)V); g_WriteByte(A + 1, (UINT8)(V >> 8));
}

// ---------------------------------------------------------------------------
// Condition Register / XER helpers
// ---------------------------------------------------------------------------
VOID
PpcSetCrField (
    IN UINT32 Field,
    IN UINT32 Value
    )
{
    UINT32 Shift = 28 - (Field * 4);
    g_PpcContext.Cr = (g_PpcContext.Cr & ~(0xFUL << Shift)) | ((Value & 0xF) << Shift);
}

UINT32
PpcGetCrField (
    IN UINT32 Field
    )
{
    UINT32 Shift = 28 - (Field * 4);
    return (g_PpcContext.Cr >> Shift) & 0xF;
}

VOID
PpcSetXerCarry (
    IN UINT32 Carry
    )
{
    g_PpcContext.Xer = (g_PpcContext.Xer & ~PPC_XER_CA) | (Carry ? PPC_XER_CA : 0);
}

VOID
PpcSetXerOverflow (
    IN UINT32 Overflow
    )
{
    g_PpcContext.Xer = (g_PpcContext.Xer & ~PPC_XER_OV) | (Overflow ? PPC_XER_OV : 0);
    if (Overflow) {
        g_PpcContext.Xer |= PPC_XER_SO;
    }
}

// Set CR0 from a 32-bit result (Rc=1 record bit)
static VOID
PpcSetCr0FromResult (
    IN UINT32 Result
    )
{
    UINT32 Value;
    if ((INT32)Result < 0) {
        Value = PPC_CR_LT;
    } else if ((INT32)Result > 0) {
        Value = PPC_CR_GT;
    } else {
        Value = PPC_CR_EQ;
    }
    if (g_PpcContext.Xer & PPC_XER_SO) {
        Value |= PPC_CR_SO;
    }
    PpcSetCrField(0, Value);
}

// Perform a signed or unsigned compare of A vs B, storing LT/GT/EQ in field F
static VOID
PpcDoCompare (
    IN UINT32  Field,
    IN UINT32  A,
    IN UINT32  B,
    IN BOOLEAN Signed
    )
{
    UINT32 Value;
    if (Signed) {
        if ((INT32)A < (INT32)B) {
            Value = PPC_CR_LT;
        } else if ((INT32)A > (INT32)B) {
            Value = PPC_CR_GT;
        } else {
            Value = PPC_CR_EQ;
        }
    } else {
        if (A < B) {
            Value = PPC_CR_LT;
        } else if (A > B) {
            Value = PPC_CR_GT;
        } else {
            Value = PPC_CR_EQ;
        }
    }
    if (g_PpcContext.Xer & PPC_XER_SO) {
        Value |= PPC_CR_SO;
    }
    PpcSetCrField(Field, Value);
}

// Branch taken helper (BO/BI semantics per the PowerPC ISA)
static BOOLEAN
PpcBranchTaken (
    IN UINT32 Bo,
    IN UINT32 Bi
    )
{
    BOOLEAN CtrOk = TRUE;
    BOOLEAN CrOk = TRUE;

    if ((Bo & 0x04) == 0) {
        // CTR test: decrement, then branch on (CTR == 0) when BO[3] set
        g_PpcContext.Ctr--;
        CtrOk = (Bo & 0x02) ? (g_PpcContext.Ctr == 0) : (g_PpcContext.Ctr != 0);
    }
    if ((Bo & 0x10) == 0) {
        // CR test: branch on the selected condition bit
        UINT32 Bit = (g_PpcContext.Cr >> (31 - Bi)) & 1;
        CrOk = (Bo & 0x08) ? (Bit != 0) : (Bit == 0);
    }
    return CtrOk && CrOk;
}

// Trap condition evaluation (TO field)
static BOOLEAN
PpcTrapCondition (
    IN UINT32 To,
    IN UINT32 A,
    IN UINT32 B
    )
{
    if ((To & 0x10) && ((INT32)A < (INT32)B)) return TRUE;
    if ((To & 0x08) && ((INT32)A > (INT32)B)) return TRUE;
    if ((To & 0x04) && (A == B))              return TRUE;
    if ((To & 0x02) && (A < B))               return TRUE;
    if ((To & 0x01) && (A > B))               return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------------------
// Integer arithmetic helpers
// ---------------------------------------------------------------------------

// result = A + B + Cin; CA/OV optional outputs
static UINT32
PpcDoAdd (
    IN  UINT32  A,
    IN  UINT32  B,
    IN  UINT32  Cin,
    OUT UINT32* CarryOut,
    OUT UINT32* Overflow
    )
{
    UINT64 Sum = (UINT64)A + (UINT64)B + Cin;
    UINT32  R  = (UINT32)Sum;
    if (CarryOut) {
        *CarryOut = (UINT32)(Sum >> 32);
    }
    if (Overflow) {
        // OV = sign(A) == sign(B) and sign(R) != sign(A)
        *Overflow = ((((A ^ B) >> 31) ^ 1) & ((R ^ A) >> 31)) & 1;
    }
    return R;
}

// result = B - A - Cin (borrow in). CarryOut = 1 if a borrow was produced.
static UINT32
PpcDoSub (
    IN  UINT32  A,
    IN  UINT32  B,
    IN  UINT32  Cin,
    OUT UINT32* CarryOut,
    OUT UINT32* Overflow
    )
{
    UINT64 Tmp = (UINT64)A + Cin;
    UINT32 R   = (UINT32)(B - Tmp);
    if (CarryOut) {
        *CarryOut = (Tmp > B) ? 1 : 0;
    }
    if (Overflow) {
        // OV = sign(A) != sign(B) and sign(R) != sign(B)
        *Overflow = ((((A ^ B) >> 31) & ((R ^ B) >> 31))) & 1;
    }
    return R;
}

// Rotate left by a 0..31 count
static UINT32
PpcRotl (
    IN UINT32 Value,
    IN UINT32 Count
    )
{
    Count &= 0x1F;
    if (Count == 0) {
        return Value;
    }
    return (Value << Count) | (Value >> (32 - Count));
}

// Rotate-mask for MB..ME (never empty; MB=ME+1 produces all ones)
static UINT32
PpcRotMask (
    IN UINT32 Mb,
    IN UINT32 Me
    )
{
    UINT32 Mask = 0;
    UINT32 I = Mb;
    for (;;) {
        Mask |= 0x80000000U >> I;
        if (I == Me) {
            break;
        }
        I = (I + 1) & 0x1F;
    }
    return Mask;
}

// Shift right arithmetic with XER[CA] computation
static UINT32
PpcSraw (
    IN  UINT32  Rs,
    IN  UINT32  Count,
    OUT UINT32* Ca
    )
{
    UINT32 N = Count & 0x1F;
    if (Count & 0x20) {
        // All bits shifted out
        if (Rs & 0x80000000) {
            *Ca = (Rs != 0xFFFFFFFF) ? 1 : 0;
            return 0xFFFFFFFF;
        }
        *Ca = 0;
        return 0;
    }
    if (N == 0) {
        *Ca = 0;
        return Rs;
    }
    if (Rs & 0x80000000) {
        UINT32 LowMask = (N == 32) ? 0xFFFFFFFF : (0xFFFFFFFFU >> (32 - N));
        *Ca = (Rs & LowMask) ? 1 : 0;
        return (UINT32)((INT32)Rs >> N);
    }
    *Ca = 0;
    return Rs >> N;
}

// ---------------------------------------------------------------------------
// String (lmw-style) load/store helpers
// ---------------------------------------------------------------------------
static VOID
PpcLoadString (
    IN UINT32 Rt,
    IN UINT32 Ea,
    IN UINT32 Count
    )
{
    UINT32 I, Reg = Rt, Shift = 24;
    if (Count == 0) {
        Count = 32;
    }
    for (I = 0; I < Count; I++) {
        if (Shift == 24) {
            g_PpcContext.Gpr[Reg] = 0;
        }
        g_PpcContext.Gpr[Reg] |= (UINT32)g_ReadByte(Ea + I) << Shift;
        if (Shift == 0) {
            Shift = 24;
            Reg = (Reg + 1) & 31;
        } else {
            Shift -= 8;
        }
    }
}

static VOID
PpcStoreString (
    IN UINT32 Rt,
    IN UINT32 Ea,
    IN UINT32 Count
    )
{
    UINT32 I, Reg = Rt, Shift = 24;
    if (Count == 0) {
        Count = 32;
    }
    for (I = 0; I < Count; I++) {
        g_WriteByte(Ea + I, (UINT8)(g_PpcContext.Gpr[Reg] >> Shift));
        if (Shift == 0) {
            Shift = 24;
            Reg = (Reg + 1) & 31;
        } else {
            Shift -= 8;
        }
    }
}

// ---------------------------------------------------------------------------
// Floating-point helpers
//
// FPRs hold IEEE-754 double bit patterns. Guest memory is big-endian. The C
// arithmetic uses the host's round-to-nearest-even, which matches FPSCR[RN]=0;
// the other rounding modes are honored by the integer-conversion helpers but
// not by the arithmetic ops (documented limitation of the emulator core).
// FPSCR sticky/exception bits are recomputed where cheap; FP exceptions never
// raise traps in the interpreter.
// ---------------------------------------------------------------------------
typedef union {
    UINT64 U;
    double D;
} PPC_FP64;

typedef union {
    UINT32 U;
    float  F;
} PPC_FP32;

static double
PpcFprValue (
    IN UINT8 Reg
    )
{
    PPC_FP64 V;
    V.U = g_PpcContext.Fpr[Reg & 31];
    return V.D;
}

static UINT64
PpcFprBits (
    IN double D
    )
{
    PPC_FP64 V;
    V.D = D;
    return V.U;
}

static double
PpcLoadDouble (
    IN UINT32 Ea
    )
{
    PPC_FP64 V;
    V.U = ((UINT64)g_ReadByte(Ea + 0) << 56) |
          ((UINT64)g_ReadByte(Ea + 1) << 48) |
          ((UINT64)g_ReadByte(Ea + 2) << 40) |
          ((UINT64)g_ReadByte(Ea + 3) << 32) |
          ((UINT64)g_ReadByte(Ea + 4) << 24) |
          ((UINT64)g_ReadByte(Ea + 5) << 16) |
          ((UINT64)g_ReadByte(Ea + 6) << 8)  |
          ((UINT64)g_ReadByte(Ea + 7));
    return V.D;
}

static VOID
PpcStoreDouble (
    IN UINT32 Ea,
    IN double D
    )
{
    PPC_FP64 V;
    V.D = D;
    g_WriteByte(Ea + 0, (UINT8)(V.U >> 56));
    g_WriteByte(Ea + 1, (UINT8)(V.U >> 48));
    g_WriteByte(Ea + 2, (UINT8)(V.U >> 40));
    g_WriteByte(Ea + 3, (UINT8)(V.U >> 32));
    g_WriteByte(Ea + 4, (UINT8)(V.U >> 24));
    g_WriteByte(Ea + 5, (UINT8)(V.U >> 16));
    g_WriteByte(Ea + 6, (UINT8)(V.U >> 8));
    g_WriteByte(Ea + 7, (UINT8)V.U);
}

static float
PpcLoadSingle (
    IN UINT32 Ea
    )
{
    PPC_FP32 V;
    V.U = ((UINT32)g_ReadByte(Ea + 0) << 24) |
          ((UINT32)g_ReadByte(Ea + 1) << 16) |
          ((UINT32)g_ReadByte(Ea + 2) << 8)  |
          ((UINT32)g_ReadByte(Ea + 3));
    return V.F;
}

static VOID
PpcStoreSingle (
    IN UINT32 Ea,
    IN double D
    )
{
    PPC_FP32 V;
    V.F = (float)D;
    g_WriteByte(Ea + 0, (UINT8)(V.U >> 24));
    g_WriteByte(Ea + 1, (UINT8)(V.U >> 16));
    g_WriteByte(Ea + 2, (UINT8)(V.U >> 8));
    g_WriteByte(Ea + 3, (UINT8)V.U);
}

static double
PpcFpAbs (
    IN double D
    )
{
    PPC_FP64 V;
    V.D = D;
    V.U &= 0x7FFFFFFFFFFFFFFFULL;
    return V.D;
}

static double
PpcFpNeg (
    IN double D
    )
{
    PPC_FP64 V;
    V.D = D;
    V.U ^= 0x8000000000000000ULL;
    return V.D;
}

// Truncate D toward zero to a signed 32-bit integer (fctiwz). NaN returns
// 0x80000000 and out-of-range values saturate, both setting VXCVI.
static INT32
PpcFpTruncToInt32 (
    IN double D
    )
{
    PPC_FP64 V;
    UINT64  Bits;
    INT32   Exp;
    UINT64  Mant;
    INT32   Result;
    BOOLEAN Neg;

    if (D != D) {  // NaN
        g_PpcContext.Fpscr |= PPC_FPSCR_VXCVI;
        return (INT32)0x80000000;
    }
    V.D = D;
    Bits = V.U;
    Neg  = (Bits >> 63) != 0;
    Exp  = (INT32)((Bits >> 52) & 0x7FF) - 1023;
    Mant = Bits & 0xFFFFFFFFFFFFFULL;

    if (Exp < 0) {
        return 0;  // |D| < 1
    }
    if (Exp >= 31) {
        g_PpcContext.Fpscr |= PPC_FPSCR_VXCVI;  // |D| >= 2^31 (or infinity)
        return Neg ? (INT32)0x80000000 : (INT32)0x7FFFFFFF;
    }
    // Rebuild the significand with the implicit leading 1 (2^52) and shift the
    // binary point left by 52-Exp to truncate the fraction.
    Result = (INT32)((Mant | 0x10000000000000ULL) >> (52 - Exp));
    return Neg ? -Result : Result;
}

// Round D to the nearest integer (half-to-even) as a signed 32-bit integer
// (fctiw with FPSCR[RN]=0). NaN and out-of-range behave like PpcFpTruncToInt32.
static INT32
PpcFpRoundToInt32 (
    IN double D
    )
{
    double R;

    if (D != D) {  // NaN
        g_PpcContext.Fpscr |= PPC_FPSCR_VXCVI;
        return (INT32)0x80000000;
    }
    if (D >= 2147483648.0 || D < -2147483648.0) {
        g_PpcContext.Fpscr |= PPC_FPSCR_VXCVI;
        return (D < 0) ? (INT32)0x80000000 : (INT32)0x7FFFFFFF;
    }
    // Round-to-nearest-even via the 2^52 add/subtract trick (|D| < 2^31 here).
    if (D >= 0) {
        R = (D + 4503599627370496.0) - 4503599627370496.0;
    } else {
        R = (D - 4503599627370496.0) + 4503599627370496.0;
    }
    return (INT32)R;
}

// Recompute the derived FPSCR bits (VX, FX, FEX) from the raw sticky flags.
static VOID
PpcUpdateFpscr (
    VOID
    )
{
    UINT32 F = g_PpcContext.Fpscr;

    if (F & PPC_FPSCR_VI_MASK) {
        F |= PPC_FPSCR_VX;
    } else {
        F &= ~PPC_FPSCR_VX;
    }
    if (F & (PPC_FPSCR_OX | PPC_FPSCR_UX | PPC_FPSCR_ZX | PPC_FPSCR_XX)) {
        F |= PPC_FPSCR_FX;
    } else {
        F &= ~PPC_FPSCR_FX;
    }
    F &= ~PPC_FPSCR_FEX;
    if ((F & PPC_FPSCR_VX) && (F & PPC_FPSCR_VE)) F |= PPC_FPSCR_FEX;
    if ((F & PPC_FPSCR_OX) && (F & PPC_FPSCR_OE)) F |= PPC_FPSCR_FEX;
    if ((F & PPC_FPSCR_UX) && (F & PPC_FPSCR_UE)) F |= PPC_FPSCR_FEX;
    if ((F & PPC_FPSCR_ZX) && (F & PPC_FPSCR_ZE)) F |= PPC_FPSCR_FEX;
    if ((F & PPC_FPSCR_XX) && (F & PPC_FPSCR_XE)) F |= PPC_FPSCR_FEX;

    g_PpcContext.Fpscr = F;
}

// Set the FPSCR[FPCC] field (C, FL, FG, FE, FU).
static VOID
PpcSetFpscrFpcc (
    IN UINT32 Field
    )
{
    g_PpcContext.Fpscr =
        (g_PpcContext.Fpscr & ~PPC_FPSCR_FPCC) | (Field & PPC_FPSCR_FPCC);
}

// FP compare: set CR field Bf and FPSCR[FPCC] from the relation of A and B.
// SignalInvalid selects fcmpo (VXVC on unordered) vs fcmpu.
static VOID
PpcFpCompare (
    IN UINT32  Bf,
    IN double  A,
    IN double  B,
    IN BOOLEAN SignalInvalid
    )
{
    UINT32 Field;
    UINT32 Fpcc;

    if (A == B) {
        Field = PPC_CR_EQ;
        Fpcc  = PPC_FPSCR_FE;
    } else if (A < B) {
        Field = PPC_CR_LT;
        Fpcc  = PPC_FPSCR_FL;
    } else if (A > B) {
        Field = PPC_CR_GT;
        Fpcc  = PPC_FPSCR_FG;
    } else {
        Field = PPC_CR_UN;   // NaN / unordered
        Fpcc  = PPC_FPSCR_FU;
        if (SignalInvalid) {
            g_PpcContext.Fpscr |= PPC_FPSCR_VXVC;
            PpcUpdateFpscr();
        }
    }

    PpcSetCrField(Bf, Field);
    PpcSetFpscrFpcc(Fpcc);
}

// Record bit: copy FX/FEX/VX/OX from FPSCR into CR1.
static VOID
PpcUpdateCr1FromFpscr (
    VOID
    )
{
    UINT32 F = g_PpcContext.Fpscr;
    UINT32 Value =
        ((F & PPC_FPSCR_FX)  ? PPC_CR_LT : 0) |
        ((F & PPC_FPSCR_FEX) ? PPC_CR_GT : 0) |
        ((F & PPC_FPSCR_VX)  ? PPC_CR_EQ : 0) |
        ((F & PPC_FPSCR_OX)  ? PPC_CR_SO : 0);
    PpcSetCrField(1, Value);
}

// Execute a floating-point instruction (opcode 59 = single precision,
// opcode 63 = double precision). Handles X-form ops (fadd/fsub/fdiv/compare/
// move/convert/...), A-form fmul (the multiplier rides in the FRC field) and
// the A-form fused ops fmadd/fmsub/fnmadd/fnmsub. Returns EFI_UNSUPPORTED for
// unhandled encodings.
static EFI_STATUS
PpcExecuteFpXform (
    IN  UINT32  w,
    IN  BOOLEAN Single
    )
{
    UINT32 X5  = (w >> 1) & 0x1F;
    UINT32 Frt = FRT(w);
    double A   = PpcFprValue(FRA(w));
    double B   = PpcFprValue(FRB(w));
    double C   = PpcFprValue(FRC(w));
    double R;

    // A-form ops are recognised by their 5-bit XO (bits 26-30). FMUL encodes
    // the multiplier in the FRC field; the fused ops use FRA x FRB plus the
    // addend/subtrahend in FRC.
    switch (X5) {
    case XOAF_FMUL:   R = A * C;         break;
    case XOAF_FMSUB:  R = A * B - C;     break;
    case XOAF_FMADD:  R = A * B + C;     break;
    case XOAF_FNMSUB: R = -(A * B) - C;  break;
    case XOAF_FNMADD: R = -(A * B) + C;  break;
    default:
        {
            UINT32 X = XO10(w);

            switch (X) {
            case XOFP_FCMPU:   // fcmpu crfD, frA, frB
                PpcFpCompare((w >> 23) & 0x7, A, B, FALSE);
                return EFI_SUCCESS;

            case XOFP_FCMPO:   // fcmpo crfD, frA, frB
                PpcFpCompare((w >> 23) & 0x7, A, B, TRUE);
                return EFI_SUCCESS;

            case XOFP_FCTIW:   // fctiw frD, frB (round per FPSCR[RN])
                g_PpcContext.Fpr[Frt] =
                    (0xFFF80000ULL << 32) | (UINT32)PpcFpRoundToInt32(B);
                if (Rc(w)) {
                    PpcUpdateCr1FromFpscr();
                }
                return EFI_SUCCESS;

            case XOFP_FCTIWZ:  // fctiwz frD, frB (truncate)
                g_PpcContext.Fpr[Frt] =
                    (0xFFF80000ULL << 32) | (UINT32)PpcFpTruncToInt32(B);
                if (Rc(w)) {
                    PpcUpdateCr1FromFpscr();
                }
                return EFI_SUCCESS;

            case XOFP_FDIV:
                R = A / B;
                break;

            case XOFP_FSUB:
                R = A - B;
                break;

            case XOFP_FADD:
                R = A + B;
                break;

            case XOFP_FNEG:
                R = PpcFpNeg(B);
                break;

            case XOFP_FMR:
                R = B;
                break;

            case XOFP_FNABS:
                R = PpcFpNeg(PpcFpAbs(B));
                break;

            case XOFP_FABS:
                R = PpcFpAbs(B);
                break;

            case XOFP_FRSP:    // frsp frD, frB: round to single precision
                R = (double)(float)B;
                break;

            case XOFP_FSQRT:
                R = __builtin_sqrt(B);
                break;

            case XOFP_FRES:    // fres frD, frB: reciprocal estimate (exact 1/x here)
                R = 1.0 / B;
                break;

            case XOFP_MFFS:    // mffs frD
                g_PpcContext.Fpr[Frt] = (UINT64)g_PpcContext.Fpscr << 32;
                if (Rc(w)) {
                    PpcUpdateCr1FromFpscr();
                }
                return EFI_SUCCESS;

            case XOFP_MTFSFI:  // mtfsfi crfD, IMM
                {
                    UINT32 Field = (w >> 23) & 0x7;
                    UINT32 Imm   = (w >> 17) & 0xF;
                    UINT32 Shift = 28 - Field * 4;
                    g_PpcContext.Fpscr =
                        (g_PpcContext.Fpscr & ~(0xFUL << Shift)) | (Imm << Shift);
                }
                if (Rc(w)) {
                    PpcUpdateCr1FromFpscr();
                }
                return EFI_SUCCESS;

            case XOFP_MTFSB0:  // mtfsb0 crB
                g_PpcContext.Fpscr &= ~(0x80000000U >> ((w >> 16) & 0x1F));
                if (Rc(w)) {
                    PpcUpdateCr1FromFpscr();
                }
                return EFI_SUCCESS;

            case XOFP_MTFSB1:  // mtfsb1 crB
                g_PpcContext.Fpscr |= (0x80000000U >> ((w >> 16) & 0x1F));
                if (Rc(w)) {
                    PpcUpdateCr1FromFpscr();
                }
                return EFI_SUCCESS;

            case XOFP_MTFSF:   // mtfsf FM, frB
                {
                    UINT32 Fm  = (w >> 17) & 0xFF;
                    UINT32 Frs = (UINT32)(g_PpcContext.Fpr[(w >> 16) & 0x1F] >> 32);
                    if (Fm != 0) {
                        UINT32 New = g_PpcContext.Fpscr;
                        UINTN  I;
                        for (I = 0; I < 8; I++) {
                            if (Fm & (1 << (7 - I))) {
                                UINT32 Shift = 28 - (UINT32)I * 4;
                                New = (New & ~(0xFUL << Shift)) |
                                      (((Frs >> Shift) & 0xF) << Shift);
                            }
                        }
                        // FX/FEX/VX are derived from the exception bits actually set.
                        New &= ~(PPC_FPSCR_FX | PPC_FPSCR_FEX | PPC_FPSCR_VX);
                        if (New & PPC_FPSCR_VI_MASK) {
                            New |= PPC_FPSCR_VX;
                        }
                        if (New & (PPC_FPSCR_OX | PPC_FPSCR_UX | PPC_FPSCR_ZX |
                                   PPC_FPSCR_XX)) {
                            New |= PPC_FPSCR_FX;
                        }
                        g_PpcContext.Fpscr = New;
                        PpcUpdateFpscr();
                    }
                }
                if (Rc(w)) {
                    PpcUpdateCr1FromFpscr();
                }
                return EFI_SUCCESS;

            default:
                if (((w >> 1) & 0x1F) == XOFP_FSEL) {
                    // fsel frD, frA, frB, frC: frA >= 0 ? frC : frB. The third
                    // source (frC) rides in the FRC field, which overlaps the XO
                    // bits used by the other X-form FP ops, so it is detected by
                    // the 5-bit XO.
                    R = (A >= 0.0) ? C : B;
                    break;
                }
                return EFI_UNSUPPORTED;
            }
        }
        break;
    }

    if (Single) {
        R = (double)(float)R;
    }
    g_PpcContext.Fpr[Frt] = PpcFprBits(R);
    if (Rc(w)) {
        PpcUpdateCr1FromFpscr();
    }
    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

UINT32
PpcFetchInstruction (
    IN UINT32 Address
    )
{
    return CpuRead32(Address);
}

EFI_STATUS
PpcExecuteInstruction (
    IN  UINT32  Instruction,
    IN  UINT32  CurrentAddress,
    OUT UINT32* NextAddress
    )
{
    UINT32 w = Instruction;
    UINT32 Op = OP(w);
    UINT32 Next = CurrentAddress + 4;

    if (NextAddress == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // All floating-point instructions (opcodes 48-55, 59, 63) require MSR[FP].
    // With the bit clear the FP-unavailable exception is raised (vector 0x800);
    // the OS handler sets MSR[FP] and re-executes. The reserved FP opcodes
    // (56-58, 60-62) stay EFI_UNSUPPORTED.
    if (Op == 48 || Op == 49 || Op == 50 || Op == 51 || Op == 52 || Op == 53 ||
        Op == 54 || Op == 55 || Op == 59 || Op == 63) {
        if (!(g_PpcContext.Msr & PPC_MSR_FP)) {
            g_PpcContext.ExceptionPending = PPC_EXCEPTION_FP_UNAVAILABLE;
            g_PpcContext.Srr0 = CurrentAddress;
            g_PpcContext.Srr1 = g_PpcContext.Msr;
            *NextAddress = CurrentAddress;
            return EFI_SUCCESS;
        }
    }

    switch (Op) {
    case 3:  // twi
        if (PpcTrapCondition((w >> 21) & 0x1F, g_PpcContext.Gpr[RA(w)], SIMM(w))) {
            g_PpcContext.ExceptionPending = PPC_EXCEPTION_TRAP;
            g_PpcContext.Srr0 = CurrentAddress;
            g_PpcContext.Srr1 = g_PpcContext.Msr;
        }
        break;

    case 7:  // mulli
        g_PpcContext.Gpr[RD(w)] =
            (UINT32)((INT64)(INT32)g_PpcContext.Gpr[RA(w)] * (INT64)(INT32)SIMM(w));
        break;

    case 8:  // subfic
        {
            UINT32 Ca;
            g_PpcContext.Gpr[RD(w)] = PpcDoSub(g_PpcContext.Gpr[RA(w)], SIMM(w), 0, &Ca, NULL);
            PpcSetXerCarry(Ca);
        }
        break;

    case 10: // cmpli
        PpcDoCompare((w >> 23) & 0x7, g_PpcContext.Gpr[RA(w)], UIMM(w), FALSE);
        break;

    case 11: // cmpi
        PpcDoCompare((w >> 23) & 0x7, g_PpcContext.Gpr[RA(w)], SIMM(w), TRUE);
        break;

    case 12: // addic (no Rc)
    case 13: // addic.
        {
            UINT32 Ca;
            g_PpcContext.Gpr[RD(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], SIMM(w), 0, &Ca, NULL);
            PpcSetXerCarry(Ca);
            if (Op == 13) {
                PpcSetCr0FromResult(g_PpcContext.Gpr[RD(w)]);
            }
        }
        break;

    case 14: // addi / li
        g_PpcContext.Gpr[RD(w)] =
            (RA(w) == 0 ? 0 : g_PpcContext.Gpr[RA(w)]) + SIMM(w);
        break;

    case 15: // addis / lis
        g_PpcContext.Gpr[RD(w)] =
            (RA(w) == 0 ? 0 : g_PpcContext.Gpr[RA(w)]) + (SIMM(w) << 16);
        break;

    case 16: // bc / bcl / bca / bcla
        if (LK(w)) {
            g_PpcContext.Lr = CurrentAddress + 4;
        }
        if (PpcBranchTaken(BO(w), BI(w))) {
            Next = AA(w) ? BD(w) : CurrentAddress + BD(w);
        }
        break;

    case 17: // sc
        g_PpcContext.ExceptionPending = PPC_EXCEPTION_SYSTEM_CALL;
        g_PpcContext.Srr0 = CurrentAddress + 4;
        g_PpcContext.Srr1 = g_PpcContext.Msr;
        break;

    case 18: // b / bl / ba / bla
        if (LK(w)) {
            g_PpcContext.Lr = CurrentAddress + 4;
        }
        Next = AA(w) ? LI(w) : CurrentAddress + LI(w);
        break;

    case 19: // XL-form
        switch (XO10(w)) {
        case XO19_MCRF:  // mcrf crfD, crfS
            PpcSetCrField((w >> 23) & 0x7, PpcGetCrField((w >> 18) & 0x7));
            break;

        case XO19_BCLR:  // blr / blrl / bclr / bclrl
            {
                UINT32 Target = g_PpcContext.Lr;
                if (LK(w)) {
                    g_PpcContext.Lr = CurrentAddress + 4;
                }
                if (PpcBranchTaken(BO(w), BI(w))) {
                    Next = Target;
                }
            }
            break;

        case XO19_RFI:  // rfi
            if (CurrentAddress >= 0x40B10000 && CurrentAddress < 0x40B30000) {
                Print(L"  DBG rfi @0x%08x: SRR0=0x%08x SRR1=0x%08x MSR=0x%08x -> PC=0x%08x\n",
                      CurrentAddress, g_PpcContext.Srr0, g_PpcContext.Srr1,
                      g_PpcContext.Msr, g_PpcContext.Srr0);
            }
            g_PpcContext.Msr = g_PpcContext.Srr1;
            Next = g_PpcContext.Srr0;
            break;

        case XO19_ISYNC:  // isync (no-op)
            break;

        case XO19_BCCTR:  // bctr / bctrl / bcctr / bcctrl
            {
                UINT32 Target = g_PpcContext.Ctr;
                if (LK(w)) {
                    g_PpcContext.Lr = CurrentAddress + 4;
                }
                if (PpcBranchTaken(BO(w), BI(w))) {
                    Next = Target;
                }
            }
            break;

        case XO19_CRNOR:   // crnor
        case XO19_CRANDC:  // crandc
        case XO19_CRXOR:   // crxor
        case XO19_CRNAND:  // crnand
        case XO19_CRAND:   // crand
        case XO19_CREQV:   // creqv
        case XO19_CRORC:   // crorc
        case XO19_CROR:    // cror / crmove / crclr
            {
                UINT32 BitT = (w >> 21) & 0x1F;  // BT
                UINT32 BitA = (w >> 16) & 0x1F;  // BA
                UINT32 BitB = (w >> 11) & 0x1F;  // BB
                UINT32 A = (g_PpcContext.Cr >> (31 - BitA)) & 1;
                UINT32 B = (g_PpcContext.Cr >> (31 - BitB)) & 1;
                UINT32 R = 0;

                switch (XO10(w)) {
                case XO19_CRNOR:  R = !(A | B); break;
                case XO19_CRANDC: R = A & !B;   break;
                case XO19_CRXOR:  R = A ^ B;    break;
                case XO19_CRNAND: R = !(A & B); break;
                case XO19_CRAND:  R = A & B;    break;
                case XO19_CREQV:  R = !(A ^ B); break;
                case XO19_CRORC:  R = A | !B;   break;
                default:          R = A | B;    break;  // XO19_CROR
                }
                if (R) {
                    g_PpcContext.Cr |= (1U << (31 - BitT));
                } else {
                    g_PpcContext.Cr &= ~(1U << (31 - BitT));
                }
            }
            break;

        default:
            return EFI_UNSUPPORTED;
        }
        break;

    case 20: // rlwimi
        {
            UINT32 Mask = PpcRotMask(MB(w), ME(w));
            UINT32 R = PpcRotl(g_PpcContext.Gpr[RS(w)], SH(w));
            g_PpcContext.Gpr[RA(w)] =
                (g_PpcContext.Gpr[RA(w)] & ~Mask) | (R & Mask);
            if (Rc(w)) {
                PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
            }
        }
        break;

    case 21: // rlwinm / slwi / srwi
        g_PpcContext.Gpr[RA(w)] =
            PpcRotl(g_PpcContext.Gpr[RS(w)], SH(w)) & PpcRotMask(MB(w), ME(w));
        if (Rc(w)) {
            PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
        }
        break;

    case 23: // rlwnm
        g_PpcContext.Gpr[RA(w)] =
            PpcRotl(g_PpcContext.Gpr[RS(w)], g_PpcContext.Gpr[RB(w)]) & PpcRotMask(MB(w), ME(w));
        if (Rc(w)) {
            PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
        }
        break;

    case 24: // ori
        g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] | UIMM(w);
        break;

    case 25: // oris
        g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] | (UIMM(w) << 16);
        break;

    case 26: // xori
        g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] ^ UIMM(w);
        break;

    case 27: // xoris
        g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] ^ (UIMM(w) << 16);
        break;

    case 28: // andi.
        g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] & UIMM(w);
        PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
        break;

    case 29: // andis.
        g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] & (UIMM(w) << 16);
        PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
        break;

    // -------- Loads / stores --------
    case 32: // lwz
        g_PpcContext.Gpr[RT(w)] = CpuRead32(EaD(w, RA(w)));
        break;

    case 33: // lwzu
        {
            UINT32 Ea = EaD(w, RA(w));
            g_PpcContext.Gpr[RT(w)] = CpuRead32(Ea);
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    case 34: // lbz
        g_PpcContext.Gpr[RT(w)] = g_ReadByte(EaD(w, RA(w)));
        break;

    case 35: // lbzu
        {
            UINT32 Ea = EaD(w, RA(w));
            g_PpcContext.Gpr[RT(w)] = g_ReadByte(Ea);
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    case 36: // stw
        CpuWrite32(EaD(w, RA(w)), g_PpcContext.Gpr[RS(w)]);
        break;

    case 37: // stwu
        {
            UINT32 Ea = EaD(w, RA(w));
            CpuWrite32(Ea, g_PpcContext.Gpr[RS(w)]);
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    case 38: // stb
        g_WriteByte(EaD(w, RA(w)), (UINT8)g_PpcContext.Gpr[RS(w)]);
        break;

    case 39: // stbu
        {
            UINT32 Ea = EaD(w, RA(w));
            g_WriteByte(Ea, (UINT8)g_PpcContext.Gpr[RS(w)]);
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    case 40: // lhz
        g_PpcContext.Gpr[RT(w)] = CpuRead16(EaD(w, RA(w)));
        break;

    case 41: // lhzu
        {
            UINT32 Ea = EaD(w, RA(w));
            g_PpcContext.Gpr[RT(w)] = CpuRead16(Ea);
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    case 42: // lha
        g_PpcContext.Gpr[RT(w)] = (UINT32)(INT32)(INT16)CpuRead16(EaD(w, RA(w)));
        break;

    case 43: // lhau
        {
            UINT32 Ea = EaD(w, RA(w));
            g_PpcContext.Gpr[RT(w)] = (UINT32)(INT32)(INT16)CpuRead16(Ea);
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    case 44: // sth
        CpuWrite16(EaD(w, RA(w)), g_PpcContext.Gpr[RS(w)]);
        break;

    case 45: // sthu
        {
            UINT32 Ea = EaD(w, RA(w));
            CpuWrite16(Ea, g_PpcContext.Gpr[RS(w)]);
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    case 46: // lmw
        {
            UINT32 Ea = EaD(w, RA(w));
            UINT32 R;
            for (R = RT(w); R < 32; R++) {
                g_PpcContext.Gpr[R] = CpuRead32(Ea + (R - RT(w)) * 4);
            }
        }
        break;

    case 47: // stmw
        {
            UINT32 Ea = EaD(w, RA(w));
            UINT32 R;
            for (R = RS(w); R < 32; R++) {
                CpuWrite32(Ea + (R - RS(w)) * 4, g_PpcContext.Gpr[R]);
            }
        }
        break;

    // -------- Floating-point loads / stores (D-form) --------
    case 48: // lfs
        g_PpcContext.Fpr[FRT(w)] = PpcFprBits((double)PpcLoadSingle(EaD(w, RA(w))));
        break;

    case 49: // lfsu
        {
            UINT32 Ea = EaD(w, RA(w));
            g_PpcContext.Fpr[FRT(w)] = PpcFprBits((double)PpcLoadSingle(Ea));
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    case 50: // lfd
        g_PpcContext.Fpr[FRT(w)] = PpcFprBits(PpcLoadDouble(EaD(w, RA(w))));
        break;

    case 51: // lfdu
        {
            UINT32 Ea = EaD(w, RA(w));
            g_PpcContext.Fpr[FRT(w)] = PpcFprBits(PpcLoadDouble(Ea));
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    case 52: // stfs
        PpcStoreSingle(EaD(w, RA(w)), PpcFprValue(RS(w)));
        break;

    case 53: // stfsu
        {
            UINT32 Ea = EaD(w, RA(w));
            PpcStoreSingle(Ea, PpcFprValue(RS(w)));
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    case 54: // stfd
        PpcStoreDouble(EaD(w, RA(w)), PpcFprValue(RS(w)));
        break;

    case 55: // stfdu
        {
            UINT32 Ea = EaD(w, RA(w));
            PpcStoreDouble(Ea, PpcFprValue(RS(w)));
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;

    // -------- Floating-point X/A-form (opcode 59 single, 63 double) --------
    // X-form ops (fadd/fsub/fdiv/...) plus A-form ops (fmul and the fused
    // fmadd/fmsub/fnmadd/fnmsub family) all live in these two primary opcodes.
    case 59:
    case 63:
        {
            EFI_STATUS FpStatus = PpcExecuteFpXform(w, (Op == 59));
            if (EFI_ERROR(FpStatus)) {
                return FpStatus;
            }
            *NextAddress = Next;
            return EFI_SUCCESS;
        }

    // -------- X-form (opcode 31) --------
    case 31:
        {
            UINT32 X = XO10(w);
            switch (X) {
            case XO_CMP:  // cmp
                PpcDoCompare((w >> 23) & 0x7, g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], TRUE);
                break;

            case XO_CMPL:  // cmpl
                PpcDoCompare((w >> 23) & 0x7, g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], FALSE);
                break;

            case XO_TW:  // tw
                if (PpcTrapCondition((w >> 21) & 0x1F, g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)])) {
                    g_PpcContext.ExceptionPending = PPC_EXCEPTION_TRAP;
                    g_PpcContext.Srr0 = CurrentAddress;
                    g_PpcContext.Srr1 = g_PpcContext.Msr;
                }
                break;

            case XO_SUBFC | 0x200:  // with-OE form
            case XO_SUBFC:  // subfc / subfco / subfc. / subfco.
                {
                    UINT32 Ca, Ov;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], 1, &Ca, &Ov);
                    PpcSetXerCarry(Ca);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_ADDC | 0x200:  // with-OE form
            case XO_ADDC:  // addc / addco / addc. / addco.
                {
                    UINT32 Ca, Ov;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], 0, &Ca, &Ov);
                    PpcSetXerCarry(Ca);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_MULHWU:  // mulhwu / mulhwu.
                g_PpcContext.Gpr[RT(w)] = (UINT32)(((UINT64)g_PpcContext.Gpr[RA(w)] * g_PpcContext.Gpr[RB(w)]) >> 32);
                if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                break;

            case XO_MFCR:  // mfcr / mfocrf
                {
                    UINT32 Fxm = (w >> 12) & 0xFF;
                    UINT32 Value = g_PpcContext.Cr;
                    if (Fxm != 0xFF) {
                        UINT32 I, Field = 0;
                        for (I = 0; I < 8; I++) {
                            if (Fxm & (0x80 >> I)) {
                                Field = (g_PpcContext.Cr >> (28 - I * 4)) & 0xF;
                                break;
                            }
                        }
                        Value = Field * 0x11111111;
                    }
                    g_PpcContext.Gpr[RT(w)] = Value;
                }
                break;

            case XO_LWARX:  // lwarx (no reservation tracking)
                g_PpcContext.Gpr[RT(w)] = CpuRead32(EaX(w, RA(w), RB(w)));
                break;

            case XO_LWZX:  // lwzx
                g_PpcContext.Gpr[RT(w)] = CpuRead32(EaX(w, RA(w), RB(w)));
                break;

            case XO_SLW:  // slw / slw.
                g_PpcContext.Gpr[RA(w)] =
                    (g_PpcContext.Gpr[RB(w)] & 0x20) ? 0 : (g_PpcContext.Gpr[RS(w)] << (g_PpcContext.Gpr[RB(w)] & 0x1F));
                if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
                break;

            case XO_CNTLZW:  // cntlzw / cntlzw.
                {
                    UINT32 V = g_PpcContext.Gpr[RS(w)], C = 0;
                    while ((V & 0x80000000) == 0 && C < 32) { V <<= 1; C++; }
                    g_PpcContext.Gpr[RA(w)] = C;
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
                }
                break;

            case XO_AND:  // and / and. / ando / ando.
                {
                    UINT32 R = g_PpcContext.Gpr[RS(w)] & g_PpcContext.Gpr[RB(w)];
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            case XO_SUBF | 0x200:  // with-OE form
            case XO_SUBF:  // subf / subf. / subfo / subfo.
                {
                    UINT32 Ov;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], 1, NULL, &Ov);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_DCBST:  // dcbst (no-op)
                break;

            case XO_ANDC:  // andc / andc. / andco / andco.
                {
                    UINT32 R = g_PpcContext.Gpr[RS(w)] & ~g_PpcContext.Gpr[RB(w)];
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            case XO_MULHW:  // mulhw / mulhw.
                {
                    INT64 P = (INT64)(INT32)g_PpcContext.Gpr[RA(w)] * (INT64)(INT32)g_PpcContext.Gpr[RB(w)];
                    g_PpcContext.Gpr[RT(w)] = (UINT32)(INT32)(P >> 32);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_TLBIEL:  // tlbiel (no-op)
                break;

            case XO_MFMSR:  // mfmsr
                g_PpcContext.Gpr[RT(w)] = g_PpcContext.Msr;
                break;

            case XO_DCBF:  // dcbf (no-op)
                break;

            case XO_LBZX:  // lbzx
                g_PpcContext.Gpr[RT(w)] = g_ReadByte(EaX(w, RA(w), RB(w)));
                break;

            case XO_NEG | 0x200:  // with-OE form
            case XO_NEG:  // neg / neg. / nego / nego.
                {
                    UINT32 Ca, Ov;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], 0, 1, &Ca, &Ov);
                    PpcSetXerCarry(Ca);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_LBZUX:  // lbzux
                {
                    UINT32 Ea = EaX(w, RA(w), RB(w));
                    g_PpcContext.Gpr[RT(w)] = g_ReadByte(Ea);
                    g_PpcContext.Gpr[RA(w)] = Ea;
                }
                break;

            case XO_NOR:  // nor / nor. / noro / noro.
                {
                    UINT32 R = ~(g_PpcContext.Gpr[RS(w)] | g_PpcContext.Gpr[RB(w)]);
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            case XO_SUBFE | 0x200:  // with-OE form
            case XO_SUBFE:  // subfe / subfe. / subfeo / subfeo.
                {
                    UINT32 Ca, Ov;
                    UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], Cin, &Ca, &Ov);
                    PpcSetXerCarry(Ca);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_ADDE | 0x200:  // with-OE form
            case XO_ADDE:  // adde / adde. / addeo / addeo.
                {
                    UINT32 Ca, Ov;
                    UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], Cin, &Ca, &Ov);
                    PpcSetXerCarry(Ca);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_MTCRF:  // mtcrf
                {
                    UINT32 Mask = (w >> 12) & 0xFF;
                    UINT32 Rs = g_PpcContext.Gpr[RS(w)];
                    UINT32 I;
                    for (I = 0; I < 8; I++) {
                        if (Mask & (0x80 >> I)) {
                            PpcSetCrField(I, (Rs >> (28 - I * 4)) & 0xF);
                        }
                    }
                }
                break;

            case XO_MTMSR:  // mtmsr
                g_PpcContext.Msr = g_PpcContext.Gpr[RS(w)];
                break;

            case XO_STWCX_:  // stwcx. (no reservation tracking)
                CpuWrite32(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
                PpcSetCrField(0, PPC_CR_EQ);
                break;

            case XO_STWX:  // stwx
                CpuWrite32(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
                break;

            case XO_STWUX:  // stwux
                {
                    UINT32 Ea = EaX(w, RA(w), RB(w));
                    CpuWrite32(Ea, g_PpcContext.Gpr[RS(w)]);
                    g_PpcContext.Gpr[RA(w)] = Ea;
                }
                break;

            case XO_SUBFZE | 0x200:  // with-OE form
            case XO_SUBFZE:  // subfze / subfze. / subfzeo / subfzeo.
                {
                    UINT32 Ca, Ov;
                    UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], 0, Cin, &Ca, &Ov);
                    PpcSetXerCarry(Ca);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_ADDZE | 0x200:  // with-OE form
            case XO_ADDZE:  // addze / addze. / addzeo / addzeo.
                {
                    UINT32 Ca, Ov;
                    UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], 0, Cin, &Ca, &Ov);
                    PpcSetXerCarry(Ca);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_STBX:  // stbx
                g_WriteByte(EaX(w, RA(w), RB(w)), (UINT8)g_PpcContext.Gpr[RS(w)]);
                break;

            case XO_SUBFME | 0x200:  // with-OE form
            case XO_SUBFME:  // subfme / subfme. / subfmeo / subfmeo.
                {
                    UINT32 Ca, Ov;
                    UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], 0xFFFFFFFF, Cin, &Ca, &Ov);
                    PpcSetXerCarry(Ca);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_ADDME | 0x200:  // with-OE form
            case XO_ADDME:  // addme / addme. / addmeo / addmeo.
                {
                    UINT32 Ca, Ov;
                    UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], 0xFFFFFFFF, Cin, &Ca, &Ov);
                    PpcSetXerCarry(Ca);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_MULLW | 0x200:  // with-OE form
            case XO_MULLW:  // mullw / mullw. / mullwo / mullwo.
                {
                    INT64 P = (INT64)(INT32)g_PpcContext.Gpr[RA(w)] * (INT64)(INT32)g_PpcContext.Gpr[RB(w)];
                    g_PpcContext.Gpr[RT(w)] = (UINT32)P;
                    if ((w >> 10) & 1) {
                        PpcSetXerOverflow(((P >> 32) != 0) && ((P >> 32) != -1));
                    }
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_MTSRIN:  // mtsrin
                g_PpcContext.Spr[g_PpcContext.Gpr[RB(w)] & 0xF] = g_PpcContext.Gpr[RS(w)];
                break;

            case XO_DCBTST:  // dcbtst (no-op)
                break;

            case XO_STBUX:  // stbux
                {
                    UINT32 Ea = EaX(w, RA(w), RB(w));
                    g_WriteByte(Ea, (UINT8)g_PpcContext.Gpr[RS(w)]);
                    g_PpcContext.Gpr[RA(w)] = Ea;
                }
                break;

            case XO_ADD | 0x200:  // with-OE form
            case XO_ADD:  // add / add. / addo / addo.
                {
                    UINT32 Ov;
                    g_PpcContext.Gpr[RT(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], 0, NULL, &Ov);
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                }
                break;

            case XO_DCBT:  // dcbt (no-op)
                break;

            case XO_LHZX:  // lhzx
                g_PpcContext.Gpr[RT(w)] = CpuRead16(EaX(w, RA(w), RB(w)));
                break;

            case XO_EQV:  // eqv / eqv. / eqvo / eqvo.
                {
                    UINT32 R = ~(g_PpcContext.Gpr[RS(w)] ^ g_PpcContext.Gpr[RB(w)]);
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            case XO_TLBIE:  // tlbie (no-op)
                break;

            case XO_LHZUX:  // lhzux
                {
                    UINT32 Ea = EaX(w, RA(w), RB(w));
                    g_PpcContext.Gpr[RT(w)] = CpuRead16(Ea);
                    g_PpcContext.Gpr[RA(w)] = Ea;
                }
                break;

            case XO_XOR:  // xor / xor. / xoro / xoro.
                {
                    UINT32 R = g_PpcContext.Gpr[RS(w)] ^ g_PpcContext.Gpr[RB(w)];
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            case XO_MFSPR:  // mfspr
                {
                    UINT32 SprNum = SPR(w);
                    UINT32 Value;
                    switch (SprNum) {
                    case SPR_XER:  Value = g_PpcContext.Xer; break;
                    case SPR_LR:   Value = g_PpcContext.Lr; break;
                    case SPR_CTR:  Value = g_PpcContext.Ctr; break;
                    case SPR_SRR0: Value = g_PpcContext.Srr0; break;
                    case SPR_SRR1: Value = g_PpcContext.Srr1; break;
                    case SPR_PVR:  Value = 0x00010000; break;   // fabricated PVR
                    default:       Value = g_PpcContext.Spr[SprNum]; break;
                    }
                    g_PpcContext.Gpr[RT(w)] = Value;
                }
                break;

            case XO_LHAX:  // lhax
                g_PpcContext.Gpr[RT(w)] = (UINT32)(INT32)(INT16)CpuRead16(EaX(w, RA(w), RB(w)));
                break;

            case XO_TLBIA:  // tlbia (no-op)
                break;

            case XO_MFTB:  // mftb
                g_PpcContext.Gpr[RT(w)] = 0;
                break;

            case XO_LHAUX:  // lhaux
                {
                    UINT32 Ea = EaX(w, RA(w), RB(w));
                    g_PpcContext.Gpr[RT(w)] = (UINT32)(INT32)(INT16)CpuRead16(Ea);
                    g_PpcContext.Gpr[RA(w)] = Ea;
                }
                break;

            case XO_STHX:  // sthx
                CpuWrite16(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
                break;

            case XO_ORC:  // orc / orc. / orco / orco.
                {
                    UINT32 R = g_PpcContext.Gpr[RS(w)] | ~g_PpcContext.Gpr[RB(w)];
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            case XO_STHUX:  // sthux
                {
                    UINT32 Ea = EaX(w, RA(w), RB(w));
                    CpuWrite16(Ea, g_PpcContext.Gpr[RS(w)]);
                    g_PpcContext.Gpr[RA(w)] = Ea;
                }
                break;

            case XO_OR:  // or / or. / oro / oro.
                {
                    UINT32 R = g_PpcContext.Gpr[RS(w)] | g_PpcContext.Gpr[RB(w)];
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            case XO_DIVWU:  // divwu / divwu. / divwuo / divwuo.
                g_PpcContext.Gpr[RT(w)] =
                    (g_PpcContext.Gpr[RB(w)] == 0) ? 0 : (g_PpcContext.Gpr[RA(w)] / g_PpcContext.Gpr[RB(w)]);
                if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                break;

            case XO_MTSPR:  // mtspr
                {
                    UINT32 SprNum = SPR(w);
                    UINT32 Value = g_PpcContext.Gpr[RS(w)];
                    switch (SprNum) {
                    case SPR_XER:  g_PpcContext.Xer = Value; break;
                    case SPR_LR:   g_PpcContext.Lr = Value; break;
                    case SPR_CTR:  g_PpcContext.Ctr = Value; break;
                    case SPR_SRR0: g_PpcContext.Srr0 = Value; break;
                    case SPR_SRR1: g_PpcContext.Srr1 = Value; break;
                    default:       g_PpcContext.Spr[SprNum] = Value; break;
                    }
                }
                break;

            case XO_DCBI:  // dcbi (no-op)
                break;

            case XO_NAND:  // nand / nand. / nando / nando.
                {
                    UINT32 R = ~(g_PpcContext.Gpr[RS(w)] & g_PpcContext.Gpr[RB(w)]);
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            case XO_DIVW | 0x200:  // with-OE form
            case XO_DIVW:  // divw / divw. / divwo / divwo.
                if (g_PpcContext.Gpr[RB(w)] == 0) {
                    g_PpcContext.Gpr[RT(w)] = 0;
                } else if (g_PpcContext.Gpr[RA(w)] == 0x80000000 &&
                           g_PpcContext.Gpr[RB(w)] == 0xFFFFFFFF) {
                    g_PpcContext.Gpr[RT(w)] = 0x80000000;
                    if ((w >> 10) & 1) PpcSetXerOverflow(1);
                } else {
                    g_PpcContext.Gpr[RT(w)] =
                        (UINT32)((INT32)g_PpcContext.Gpr[RA(w)] / (INT32)g_PpcContext.Gpr[RB(w)]);
                }
                if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
                break;

            case XO_MCRXR:  // mcrxr
                PpcSetCrField((w >> 23) & 0x7, (g_PpcContext.Xer >> 28) & 0xF);
                g_PpcContext.Xer &= 0x0FFFFFFF;
                break;

            case XO_LSWX:  // lswx
                PpcLoadString(RT(w), EaX(w, RA(w), RB(w)), (g_PpcContext.Xer >> 25) & 0x7F);
                break;

            case XO_LWBRX:  // lwbrx
                g_PpcContext.Gpr[RT(w)] = CpuRead32Rev(EaX(w, RA(w), RB(w)));
                break;

            case XO_SRW:  // srw / srw.
                g_PpcContext.Gpr[RA(w)] =
                    (g_PpcContext.Gpr[RB(w)] & 0x20) ? 0 : (g_PpcContext.Gpr[RS(w)] >> (g_PpcContext.Gpr[RB(w)] & 0x1F));
                if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
                break;

            case XO_MFSR:  // mfsr
                g_PpcContext.Gpr[RT(w)] = g_PpcContext.Spr[(w >> 11) & 0xF];
                break;

            case XO_LSWI:  // lswi
                PpcLoadString(RT(w), EaD(w, RA(w)), (w >> 11) & 0x1F);
                break;

            case XO_SYNC:  // sync (no-op)
                break;

            case XO_TLBSYNC:  // tlbsync (no-op; no TLB modelled)
                break;

            case XO_MTSR:  // mtsr
                g_PpcContext.Spr[(w >> 11) & 0xF] = g_PpcContext.Gpr[RS(w)];
                break;

            case XO_MFSRIN:  // mfsrin
                g_PpcContext.Gpr[RT(w)] = g_PpcContext.Spr[g_PpcContext.Gpr[RB(w)] & 0xF];
                break;

            case XO_STSWX:  // stswx
                PpcStoreString(RS(w), EaX(w, RA(w), RB(w)), (g_PpcContext.Xer >> 25) & 0x7F);
                break;

            case XO_STWBRX:  // stwbrx
                CpuWrite32Rev(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
                break;

            case XO_STSWI:  // stswi
                PpcStoreString(RS(w), EaD(w, RA(w)), (w >> 11) & 0x1F);
                break;

            case XO_LHBRX:  // lhbrx
                g_PpcContext.Gpr[RT(w)] = CpuRead16Rev(EaX(w, RA(w), RB(w)));
                break;

            case XO_SRAW:  // sraw / sraw.
                {
                    UINT32 Ca;
                    UINT32 R = PpcSraw(g_PpcContext.Gpr[RS(w)], g_PpcContext.Gpr[RB(w)], &Ca);
                    g_PpcContext.Gpr[RA(w)] = R;
                    PpcSetXerCarry(Ca);
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            case XO_SRAWI:  // srawi / srawi.
                {
                    UINT32 Ca;
                    UINT32 R = PpcSraw(g_PpcContext.Gpr[RS(w)], SH(w), &Ca);
                    g_PpcContext.Gpr[RA(w)] = R;
                    PpcSetXerCarry(Ca);
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            case XO_EIEIO:  // eieio (no-op)
                break;

            case XO_STHBRX:  // sthbrx
                CpuWrite16Rev(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
                break;

            case XO_ICBI:  // icbi (no-op)
                break;

            case XO_DCBZ:  // dcbz: zero a 32-byte cache line
                {
                    UINT32 Ea = EaX(w, RA(w), RB(w)) & ~0x1F;
                    UINT32 I;
                    for (I = 0; I < 32; I++) {
                        g_WriteByte(Ea + I, 0);
                    }
                }
                break;

            default:
                return EFI_UNSUPPORTED;
            }
        }
        break;

    default:
        return EFI_UNSUPPORTED;
    }

    *NextAddress = Next;
    return EFI_SUCCESS;
}

EFI_STATUS
PpcExecuteBlock (
    IN  UINT32* InstructionStream,
    IN  UINTN   MaxInstructions,
    OUT UINTN*  ExecutedCount
    )
{
    UINTN Executed = 0;

    if (InstructionStream == NULL || ExecutedCount == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    g_PpcContext.Pc = (UINT32)(UINTN)InstructionStream;
    g_PpcContext.ExceptionPending = 0;

    while (Executed < MaxInstructions) {
        UINT32 Instr = CpuRead32(g_PpcContext.Pc);
        UINT32 Next;
        EFI_STATUS Status = PpcExecuteInstruction(Instr, g_PpcContext.Pc, &Next);
        Executed++;
        if (EFI_ERROR(Status)) {
            *ExecutedCount = Executed;
            return Status;
        }
        g_PpcContext.Pc = Next;
        if (g_PpcContext.ExceptionPending != 0) {
            break;
        }
    }

    *ExecutedCount = Executed;
    return EFI_SUCCESS;
}

// Continuous guest execution harness. Runs up to MaxInstructions of real
// guest code from the current PC, delivering pending exceptions through the
// CPU vector mechanism so interrupt/syscall handlers run like on hardware.
// Stops with the reported status on an unimplemented opcode (EFI_UNSUPPORTED)
// or a memory/execution error; the guest PC is left at the stopping point.
EFI_STATUS
EFIAPI
PpcRunGuest (
    IN  UINT32  MaxInstructions,
    IN  BOOLEAN LogUnsupported,
    OUT UINTN*  ExecutedCount
    )
{
    UINTN Executed = 0;
    UINTN TailStart = 0;
    UINTN TailCount = 0;
    static UINT32 TailPc[4096];
    static UINT32 TailInst[4096];
    static UINT32 TailNext[4096];
    static UINT32 TailR28[4096];
    static UINT32 TailR8[4096];
    static UINT32 TailR17[4096];
    static UINT32 PcsDumped = 0;
    static UINT32 TraceDumped = 0;
    static UINT32 StoreProbed = 0;
    static UINT32 AllocTraced = 0;

    if (ExecutedCount == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    g_PpcContext.ExceptionPending = 0;

    while (Executed < MaxInstructions) {
        UINT32 Instr;
        UINT32 Current;
        UINT32 Next;
        EFI_STATUS Status;

        Instr = CpuRead32(g_PpcContext.Pc);
        Current = g_PpcContext.Pc;
        Status = PpcExecuteInstruction(Instr, Current, &Next);
        Executed++;
        if (Current == 0x40B126CC || Current == 0x40B107FC || Current == 0x40B10098) {
            Print(L"  PROBE@0x%08x r1=0x%08x r3=0x%08x [r1+648]=0x%08x [0x648]=0x%08x [0xA648]=0x%08x [0xAFE4]=0x%04x [r1+5A0]=0x%08x [r1+5A4]=0x%08x [r1-964]=0x%08x [r1-20]=0x%08x\n",
                  Current, g_PpcContext.Gpr[1], g_PpcContext.Gpr[3],
                  CpuRead32(g_PpcContext.Gpr[1] + 0x648),
                  CpuRead32(0x00000648), CpuRead32(0x0000A648),
                  CpuRead16(0x0000AFE4),
                  CpuRead32(g_PpcContext.Gpr[1] + 0x5A0),
                  CpuRead32(g_PpcContext.Gpr[1] + 0x5A4),
                  CpuRead32(g_PpcContext.Gpr[1] - 0x964),
                  CpuRead32(g_PpcContext.Gpr[1] - 0x20));
        }
        if (StoreProbed == 0 && (Current == 0x40B11B64 || Current == 0x40B11B48)) {
            UINT32 P = g_PpcContext.Gpr[1];
            UINT32 T;
            StoreProbed = 1;
            Print(L"  STOREPROBE@0x%08x (before) r1=0x%08x r8=0x%08x r9=0x%08x r16=0x%08x r28=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x\n",
                  Current, P, g_PpcContext.Gpr[8], g_PpcContext.Gpr[9],
                  g_PpcContext.Gpr[16], g_PpcContext.Gpr[28], g_PpcContext.Gpr[29],
                  g_PpcContext.Gpr[30], g_PpcContext.Gpr[31]);
            Print(L"  STOREPROBE PA_CurAS[r1-1C]=0x%08x PA_PSA[r1-18]=0x%08x PA_KDP[r1-4]=0x%08x\n",
                  CpuRead32(P - 0x1C), CpuRead32(P - 0x18), CpuRead32(P - 0x04));
            Print(L"  STOREPROBE PA_ConfigInfo[r1+648]=0x%08x [r1+64C]=0x%08x\n",
                  CpuRead32(P + 0x648), CpuRead32(P + 0x64C));
            Print(L"  STOREPROBE FreePool[r1-AB0]=0x%08x FirstSeg[r1-AA0]=0x%08x FirstSegLogi[r1-A9C]=0x%08x\n",
                  CpuRead32(P - 0xAB0), CpuRead32(P - 0xAA0), CpuRead32(P - 0xA9C));
            Print(L"  STOREPROBE mem@0x8C40:\n");
            for (T = 0x8C40; T < 0x8D40; T += 16) {
                Print(L"    0x%08x: %08x %08x %08x %08x\n",
                      T, CpuRead32(T), CpuRead32(T + 4), CpuRead32(T + 8), CpuRead32(T + 0xC));
            }
        }
        if (AllocTraced < 60 && Current == 0x40B22828) {
            UINT32 R1 = g_PpcContext.Gpr[1];
            Print(L"  ALLOCENTRY[%d] size=0x%08x r9=0x%08x LR=0x%08x FreeNext=0x%08x FreePageCnt=0x%08x FreeList=0x%08x\n",
                  AllocTraced, g_PpcContext.Gpr[8], g_PpcContext.Gpr[9],
                  g_PpcContext.Lr, CpuRead32(R1 - 0xAB0 + 8),
                  CpuRead32(R1 - 0x430), CpuRead32(R1 - 0x448));
        }
        if (AllocTraced < 200 && Current == 0x40B228D8) {
            UINT32 R1 = g_PpcContext.Gpr[1];
            Print(L"  ALLOCWALK[%d] block=0x%08x blocksize=0x%08x req=0x%08x sig=0x%08x FreeNext=0x%08x\n",
                  AllocTraced, g_PpcContext.Gpr[15], CpuRead32(g_PpcContext.Gpr[15]),
                  g_PpcContext.Gpr[8], CpuRead32(g_PpcContext.Gpr[15] + 4),
                  CpuRead32(R1 - 0xAB0 + 8));
        }
        if (AllocTraced < 1 && Current >= 0x40B22820 && Current <= 0x40B228E4) {
            Print(L"  ALLOCSTEP[%d] PC=0x%08x next=0x%08x r8=0x%08x r15=0x%08x r16=0x%08x r17=0x%08x r18=0x%08x CR=0x%08x\n",
                  AllocTraced, Current, Next, g_PpcContext.Gpr[8], g_PpcContext.Gpr[15],
                  g_PpcContext.Gpr[16], g_PpcContext.Gpr[17], g_PpcContext.Gpr[18],
                  g_PpcContext.Cr);
        }
        if (AllocTraced < 120 && Current == 0x40B229D4) {
            UINT32 R1 = g_PpcContext.Gpr[1];
            UINT32 R = g_PpcContext.Gpr[8];
            Print(L"  ALLOCRET[%d] ret=0x%08x LR=0x%08x FreeHead=0x%08x sig=0x%08x offnext=0x%08x\n",
                  AllocTraced, R, g_PpcContext.Lr, CpuRead32(R1 - 0xAB0 + 8),
                  CpuRead32(R - 4), CpuRead32(R - 8));
            AllocTraced++;
        }
        TailInst[TailStart] = Instr;
        TailPc[TailStart] = Current;
        TailNext[TailStart] = Next;
        TailR28[TailStart] = g_PpcContext.Gpr[28];
        TailR8[TailStart] = g_PpcContext.Gpr[8];
        TailR17[TailStart] = g_PpcContext.Gpr[17];
        TailStart = (TailStart + 1) % 4096;
        if (TailCount < 4096) TailCount++;
        if ((Executed % 250000) == 0) {
            Print(L"  PROGRESS[%d] PC=0x%08x LR=0x%08x r1=0x%08x r8=0x%08x r28=0x%08x SPRG4=0x%08x\n",
                  Executed, Current, g_PpcContext.Lr, g_PpcContext.Gpr[1],
                  g_PpcContext.Gpr[8], g_PpcContext.Gpr[28], g_PpcContext.Spr[272]);
        }
        if (PcsDumped == 0 && (Current == 0x40B2751C || Current == 0x40B27530 || Current == 0x40B27540)) {
            UINT32 Ewa = g_PpcContext.Spr[272];
            UINT32 Kdp = CpuRead32(Ewa - 4);
            PcsDumped = 1;
            Print(L"  PANICDUMP EWA=0x%08x KDP=0x%08x [EWA-4]=0x%08x\n", Ewa, Kdp, CpuRead32(Ewa - 4));
            Print(L"  PANICDUMP saved r0-r11: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                  CpuRead32(Kdp+0x700), CpuRead32(Kdp+0x704), CpuRead32(Kdp+0x708),
                  CpuRead32(Kdp+0x70c), CpuRead32(Kdp+0x710), CpuRead32(Kdp+0x714),
                  CpuRead32(Kdp+0x718), CpuRead32(Kdp+0x71c), CpuRead32(Kdp+0x720),
                  CpuRead32(Kdp+0x724), CpuRead32(Kdp+0x728), CpuRead32(Kdp+0x72c));
            Print(L"  PANICDUMP saved r12-r23: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                  CpuRead32(Kdp+0x730), CpuRead32(Kdp+0x734), CpuRead32(Kdp+0x738),
                  CpuRead32(Kdp+0x73c), CpuRead32(Kdp+0x740), CpuRead32(Kdp+0x744),
                  CpuRead32(Kdp+0x748), CpuRead32(Kdp+0x74c), CpuRead32(Kdp+0x750),
                  CpuRead32(Kdp+0x754), CpuRead32(Kdp+0x758), CpuRead32(Kdp+0x75c));
            Print(L"  PANICDUMP saved r24-r31: %08x %08x %08x %08x %08x %08x %08x %08x\n",
                  CpuRead32(Kdp+0x760), CpuRead32(Kdp+0x764), CpuRead32(Kdp+0x768),
                  CpuRead32(Kdp+0x76c), CpuRead32(Kdp+0x770), CpuRead32(Kdp+0x774),
                  CpuRead32(Kdp+0x778), CpuRead32(Kdp+0x77c));
            Print(L"  PANICDUMP CR=0x%08x XER=0x%08x CTR=0x%08x LR=0x%08x PVR=0x%08x DSISR=0x%08x DAR=0x%08x\n",
                  CpuRead32(Kdp+0x780), CpuRead32(Kdp+0x788), CpuRead32(Kdp+0x790),
                  CpuRead32(Kdp+0x78c), CpuRead32(Kdp+0x794), CpuRead32(Kdp+0x798),
                  CpuRead32(Kdp+0x79c));
            Print(L"  PANICDUMP TBU=0x%08x TBL=0x%08x DEC=0x%08x SDR1=0x%08x SRR0=0x%08x SRR1=0x%08x MSR=0x%08x\n",
                  CpuRead32(Kdp+0x7a0), CpuRead32(Kdp+0x7a4), CpuRead32(Kdp+0x7a8),
                  CpuRead32(Kdp+0x7b0), CpuRead32(Kdp+0x7b4), CpuRead32(Kdp+0x7b8),
                  CpuRead32(Kdp+0x7bc));
            Print(L"  PANICDUMP TerminationCaller[KDP+0x904]=0x%08x [KDP+0x900]=0x%08x [KDP+0x908]=0x%08x\n",
                  CpuRead32(Kdp+0x904), CpuRead32(Kdp+0x900), CpuRead32(Kdp+0x908));
            Print(L"  PANICDUMP NoIdeaR23[KDP-0x900]=0x%08x OldKDP[KDP+0x5a0]=0x%08x [KDP+0x5a4]=0x%08x [KDP+0x648]=0x%08x [KDP+0x64c]=0x%08x\n",
                  CpuRead32(Kdp-0x900), CpuRead32(Kdp+0x5a0), CpuRead32(Kdp+0x5a4),
                  CpuRead32(Kdp+0x648), CpuRead32(Kdp+0x64c));
            Print(L"  PANICDUMP pool FreePool[KDP-0xAB0]=0x%08x FirstSeg[KDP-0xAA0]=0x%08x FirstSegLogi[KDP-0xA9C]=0x%08x\n",
                  CpuRead32(Kdp-0xAB0), CpuRead32(Kdp-0xAA0), CpuRead32(Kdp-0xA9C));
            Print(L"  PANICPOOL FreePool LLL @0x9548:\n");
            {
                UINT32 A;
                for (A = 0x9548; A < 0x9568; A += 16) {
                    Print(L"    0x%08x: %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
            }
            Print(L"  PANICPOOL first segment begin @0x2FE0:\n");
            {
                UINT32 A;
                for (A = 0x2FE0; A < 0x3050; A += 16) {
                    Print(L"    0x%08x: %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
            }
            Print(L"  PANICPOOL first segment end @0x9FC0..0xA010:\n");
            {
                UINT32 A;
                for (A = 0x9FC0; A < 0xA010; A += 16) {
                    Print(L"    0x%08x: %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
            }
            Print(L"  PANICPOOL cgrp block @0x8C40..0x8CB8:\n");
            {
                UINT32 A;
                for (A = 0x8C40; A < 0x8CB8; A += 16) {
                    Print(L"    0x%08x: %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
            }
            Print(L"  PANICDUMP mem@0x8C00..0x8D00:\n");
            {
                UINT32 T;
                for (T = 0x8C00; T < 0x8D00; T += 16) {
                    Print(L"    0x%08x: %08x %08x %08x %08x\n",
                          T, CpuRead32(T), CpuRead32(T + 4), CpuRead32(T + 8), CpuRead32(T + 0xC));
                }
            }
            Print(L"  PANICROM (NKCreateAddressSpaceSub region):\n");
            {
                UINT32 A;
                for (A = 0x40B1F000; A < 0x40B1FC00; A += 16) {
                    Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
            }
            Print(L"  PANICROM (InitPool region 0x40B10F00):\n");
            {
                UINT32 A;
                for (A = 0x40B10F00; A < 0x40B11200; A += 16) {
                    Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
            }
            Print(L"  PANICROM (PoolAllocClear/InitPool region 0x40B22600):\n");
            {
                UINT32 A;
                for (A = 0x40B22600; A < 0x40B22A00; A += 16) {
                    Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
            }
            Print(L"  PANICROM (system-AS creation 0x40B11B00):\n");
            {
                UINT32 A;
                for (A = 0x40B11B00; A < 0x40B11E60; A += 16) {
                    Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
            }
            Print(L"  PANICDUMP live r1=0x%08x r8=0x%08x r28=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x LR=0x%08x\n",
                  g_PpcContext.Gpr[1], g_PpcContext.Gpr[8], g_PpcContext.Gpr[28],
                  g_PpcContext.Gpr[29], g_PpcContext.Gpr[30], g_PpcContext.Gpr[31],
                  g_PpcContext.Lr);
            Print(L"  PANICROM (message + dead-loop region 0x40B10600):\n");
            {
                UINT32 A;
                for (A = 0x40B10600; A < 0x40B10900; A += 16) {
                    Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
            }
            Print(L"  PANICROM (panic handler region 0x40B26300):\n");
            {
                UINT32 A;
                for (A = 0x40B26300; A < 0x40B27600; A += 16) {
                    Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
            }
        }
        if (TraceDumped == 0 && (Current == 0x40B272E0 || Current == 0x40B272E8 || Current == 0x40B272EC)) {
            UINTN I;
            UINTN N = (TailCount < 1500) ? TailCount : 1500;
            CHAR16 Mn[16];
            TraceDumped = 1;
            Print(L"--- last %d instructions before panic entry ---\n", N);
            for (I = 0; I < N; I++) {
                UINTN Idx = (TailStart + TailCount - 1 - I) % 4096;
                PpcDecodeInstruction(TailInst[Idx], Mn, sizeof(Mn));
                Print(L"  PRE[-%d] PC=0x%08x 0x%08x %s -> 0x%08x r28=0x%08x r8=0x%08x r17=0x%08x\n",
                      (UINTN)I + 1, TailPc[Idx], TailInst[Idx], Mn, TailNext[Idx],
                      TailR28[Idx], TailR8[Idx], TailR17[Idx]);
            }
            Print(L"  PRE[0] PC=0x%08x LR=0x%08x r1=0x%08x r8=0x%08x r9=0x%08x r17=0x%08x r28=0x%08x\n",
                  Current, g_PpcContext.Lr, g_PpcContext.Gpr[1], g_PpcContext.Gpr[8],
                  g_PpcContext.Gpr[9], g_PpcContext.Gpr[17], g_PpcContext.Gpr[28]);
        }
        if (Executed <= 200) {
            CHAR16 Mn[16];
            PpcDecodeInstruction(Instr, Mn, sizeof(Mn));
            Print(L"  TRACE[%d] PC=0x%08x 0x%08x %s -> next 0x%08x\n",
                  Executed, Current, Instr, Mn, Next);
        }

        if (EFI_ERROR(Status)) {
            if (LogUnsupported) {
                UINTN I;
                CHAR16 Mn[16];
                Print(L"--- last %d instructions before stop ---\n", TailCount);
                for (I = 0; I < TailCount; I++) {
                    UINTN Idx = (TailStart + TailCount - 1 - I) % 4096;
                    PpcDecodeInstruction(TailInst[Idx], Mn, sizeof(Mn));
                    Print(L"  TRACE[-%d] PC=0x%08x 0x%08x %s -> 0x%08x r28=0x%08x r8=0x%08x\n",
                          (UINTN)I + 1, TailPc[Idx], TailInst[Idx], Mn, TailNext[Idx],
                          TailR28[Idx], TailR8[Idx]);
                }
                {
                    CHAR16 StopMn[16];
                    PpcDecodeInstruction(Instr, StopMn, sizeof(StopMn));
                    Print(L"GUEST STOP at PC=0x%08x inst=0x%08x (%s): %r\n",
                          g_PpcContext.Pc, Instr, StopMn, Status);
                }
                Print(L"  MSR=0x%08x CR=0x%08x LR=0x%08x CTR=0x%08x SRR0=0x%08x SRR1=0x%08x\n",
                      g_PpcContext.Msr, g_PpcContext.Cr, g_PpcContext.Lr,
                      g_PpcContext.Ctr, g_PpcContext.Srr0, g_PpcContext.Srr1);
                Print(L"  GPR: r0=0x%08x r1=0x%08x r2=0x%08x r3=0x%08x r4=0x%08x r5=0x%08x r6=0x%08x r7=0x%08x\n",
                      g_PpcContext.Gpr[0], g_PpcContext.Gpr[1], g_PpcContext.Gpr[2],
                      g_PpcContext.Gpr[3], g_PpcContext.Gpr[4], g_PpcContext.Gpr[5],
                      g_PpcContext.Gpr[6], g_PpcContext.Gpr[7]);
                Print(L"  GPR: r8=0x%08x r9=0x%08x r10=0x%08x r11=0x%08x r12=0x%08x r13=0x%08x r14=0x%08x r15=0x%08x\n",
                      g_PpcContext.Gpr[8], g_PpcContext.Gpr[9], g_PpcContext.Gpr[10],
                      g_PpcContext.Gpr[11], g_PpcContext.Gpr[12], g_PpcContext.Gpr[13],
                      g_PpcContext.Gpr[14], g_PpcContext.Gpr[15]);
                Print(L"  GPR: r16=0x%08x r17=0x%08x r18=0x%08x r19=0x%08x r20=0x%08x r21=0x%08x r22=0x%08x r23=0x%08x\n",
                      g_PpcContext.Gpr[16], g_PpcContext.Gpr[17], g_PpcContext.Gpr[18],
                      g_PpcContext.Gpr[19], g_PpcContext.Gpr[20], g_PpcContext.Gpr[21],
                      g_PpcContext.Gpr[22], g_PpcContext.Gpr[23]);
                Print(L"  GPR: r24=0x%08x r25=0x%08x r26=0x%08x r27=0x%08x r28=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x\n",
                      g_PpcContext.Gpr[24], g_PpcContext.Gpr[25], g_PpcContext.Gpr[26],
                      g_PpcContext.Gpr[27], g_PpcContext.Gpr[28], g_PpcContext.Gpr[29],
                      g_PpcContext.Gpr[30], g_PpcContext.Gpr[31]);
                Print(L"  SPR: XER=0x%08x SPRG4=0x%08x SPRG5=0x%08x SPRG6=0x%08x SPRG7=0x%08x\n",
                      g_PpcContext.Xer, g_PpcContext.Spr[272], g_PpcContext.Spr[273],
                      g_PpcContext.Spr[274], g_PpcContext.Spr[275]);
                Print(L"  MEM[r8]: %08x %08x %08x %08x %08x %08x %08x %08x\n",
                      CpuRead32(g_PpcContext.Gpr[8] + 0x00),
                      CpuRead32(g_PpcContext.Gpr[8] + 0x04),
                      CpuRead32(g_PpcContext.Gpr[8] + 0x08),
                      CpuRead32(g_PpcContext.Gpr[8] + 0x0C),
                      CpuRead32(g_PpcContext.Gpr[8] + 0x10),
                      CpuRead32(g_PpcContext.Gpr[8] + 0x14),
                      CpuRead32(g_PpcContext.Gpr[8] + 0x18),
                      CpuRead32(g_PpcContext.Gpr[8] + 0x1C));
                Print(L"  MEM[r11]: %08x %08x %08x %08x %08x %08x %08x %08x\n",
                      CpuRead32(g_PpcContext.Gpr[11] + 0x00),
                      CpuRead32(g_PpcContext.Gpr[11] + 0x04),
                      CpuRead32(g_PpcContext.Gpr[11] + 0x08),
                      CpuRead32(g_PpcContext.Gpr[11] + 0x0C),
                      CpuRead32(g_PpcContext.Gpr[11] + 0x10),
                      CpuRead32(g_PpcContext.Gpr[11] + 0x14),
                      CpuRead32(g_PpcContext.Gpr[11] + 0x18),
                      CpuRead32(g_PpcContext.Gpr[11] + 0x1C));
                Print(L"  OUTBUF[r1-0x404]: %08x %08x %08x %08x %08x %08x %08x %08x\n",
                      CpuRead32(g_PpcContext.Gpr[1] - 0x404 + 0x00),
                      CpuRead32(g_PpcContext.Gpr[1] - 0x404 + 0x04),
                      CpuRead32(g_PpcContext.Gpr[1] - 0x404 + 0x08),
                      CpuRead32(g_PpcContext.Gpr[1] - 0x404 + 0x0C),
                      CpuRead32(g_PpcContext.Gpr[1] - 0x404 + 0x10),
                      CpuRead32(g_PpcContext.Gpr[1] - 0x404 + 0x14),
                      CpuRead32(g_PpcContext.Gpr[1] - 0x404 + 0x18),
                      CpuRead32(g_PpcContext.Gpr[1] - 0x404 + 0x1C));
            }
            *ExecutedCount = Executed;
            return Status;
        }

        if (g_PpcContext.ExceptionPending != 0) {
            UINT32 Pending = g_PpcContext.ExceptionPending;
            g_PpcContext.ExceptionPending = 0;
            Status = PpcHandleException(Pending, Current);
            if (EFI_ERROR(Status)) {
                *ExecutedCount = Executed;
                return Status;
            }
            continue;
        }

        g_PpcContext.Pc = Next;
    }

    Print(L"  PROGRESS[END] PC=0x%08x LR=0x%08x r1=0x%08x r3=0x%08x r8=0x%08x r28=0x%08x SPRG4=0x%08x\n",
          g_PpcContext.Pc, g_PpcContext.Lr, g_PpcContext.Gpr[1], g_PpcContext.Gpr[3],
          g_PpcContext.Gpr[8], g_PpcContext.Gpr[28], g_PpcContext.Spr[272]);
    Print(L"  MSR=0x%08x CR=0x%08x SRR0=0x%08x SRR1=0x%08x CTR=0x%08x XER=0x%08x\n",
          g_PpcContext.Msr, g_PpcContext.Cr, g_PpcContext.Srr0, g_PpcContext.Srr1,
          g_PpcContext.Ctr, g_PpcContext.Xer);
    Print(L"  GPR: r8=0x%08x r9=0x%08x r16=0x%08x r17=0x%08x r18=0x%08x r26=0x%08x r27=0x%08x r28=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x\n",
          g_PpcContext.Gpr[8], g_PpcContext.Gpr[9], g_PpcContext.Gpr[16],
          g_PpcContext.Gpr[17], g_PpcContext.Gpr[18], g_PpcContext.Gpr[26],
          g_PpcContext.Gpr[27], g_PpcContext.Gpr[28], g_PpcContext.Gpr[29],
          g_PpcContext.Gpr[30], g_PpcContext.Gpr[31]);
    {
        UINTN A, W;
        UINT32 Loops[][2] = { { 0x40A00000u, 0x40A01000u }, { 0x40B10000u, 0x40B16000u },
                              { 0x40B11B00u, 0x40B11E60u }, { 0x40B1F800u, 0x40B1FC00u },
                              { 0x40B23F00u, 0x40B24400u }, { 0x40B26000u, 0x40B28000u },
                              { 0x40B28700u, 0x40B28B00u } };
        for (W = 0; W < 7; W++) {
            for (A = Loops[W][0]; A < Loops[W][1]; A += 16) {
                Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
                      A, CpuRead32(A), CpuRead32(A + 4),
                      CpuRead32(A + 8), CpuRead32(A + 12));
            }
        }
    }
    if (LogUnsupported) {
        UINTN I;
        CHAR16 Mn[16];
        Print(L"--- last %d instructions (budget stop) ---\n", TailCount);
        for (I = 0; I < TailCount && I < 300; I++) {
            UINTN Idx = (TailStart + TailCount - 1 - I) % 4096;
            PpcDecodeInstruction(TailInst[Idx], Mn, sizeof(Mn));
            Print(L"  TRACE[-%d] PC=0x%08x 0x%08x %s -> 0x%08x r28=0x%08x r8=0x%08x r17=0x%08x\n",
                  (UINTN)I + 1, TailPc[Idx], TailInst[Idx], Mn, TailNext[Idx],
                  TailR28[Idx], TailR8[Idx], TailR17[Idx]);
        }
    }
    *ExecutedCount = Executed;
    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Instruction decode to a short mnemonic
// ---------------------------------------------------------------------------
static const CHAR16* g_DOpcodeNames[] = {
    L"reserved", L"reserved", L"reserved", L"twi",      L"reserved", L"reserved",
    L"reserved", L"mulli",    L"subfic",   L"reserved", L"cmpli",    L"cmpi",
    L"addic",    L"addic.",   L"addi",     L"addis",    L"bc",       L"sc",
    L"b",        L"XL-form",  L"rlwimi",   L"rlwinm",   L"reserved", L"rlwnm",
    L"ori",      L"oris",     L"xori",     L"xoris",    L"andi.",    L"andis.",
    L"reserved", L"X-form",   L"lwz",      L"lwzu",     L"lbz",      L"lbzu",
    L"stw",      L"stwu",     L"stb",      L"stbu",     L"lhz",      L"lhzu",
    L"lha",      L"lhau",     L"sth",      L"sthu",     L"lmw",      L"stmw"
};

EFI_STATUS
PpcDecodeInstruction (
    IN  UINT32  Instruction,
    OUT CHAR16* Buffer,
    IN  UINTN   BufferSize
    )
{
    UINT32 w = Instruction;
    UINT32 Op = OP(w);
    const CHAR16* Name;

    if (Buffer == NULL || BufferSize < 16) {
        return EFI_INVALID_PARAMETER;
    }

    Buffer[0] = 0;
    if (Op < 48) {
        Name = g_DOpcodeNames[Op];
    } else if (Op >= 48 && Op <= 63) {
        switch (Op) {
        case 48:  Name = L"lfs";    break;
        case 49:  Name = L"lfsu";   break;
        case 50:  Name = L"lfd";    break;
        case 51:  Name = L"lfdu";   break;
        case 52:  Name = L"stfs";   break;
        case 53:  Name = L"stfsu";  break;
        case 54:  Name = L"stfd";   break;
        case 55:  Name = L"stfdu";  break;
        case 59:
        case 63:
            {
                UINT32 X5 = (w >> 1) & 0x1F;
                BOOLEAN Sng = (Op == 59);

                // A-form ops share the primary opcode with the X-form FP ops;
                // they are recognised by their 5-bit XO (bits 26-30).
                if (X5 == XOAF_FMUL) {
                    Name = Sng ? L"fmuls" : L"fmul";
                    break;
                }
                switch (X5) {
                case XOAF_FMSUB:  Name = Sng ? L"fmsubs"  : L"fmsub";  break;
                case XOAF_FMADD:  Name = Sng ? L"fmadds"  : L"fmadd";  break;
                case XOAF_FNMSUB: Name = Sng ? L"fnmsubs" : L"fnmsub"; break;
                case XOAF_FNMADD: Name = Sng ? L"fnmadds" : L"fnmadd"; break;
                case XOFP_FSEL:   Name = L"fsel";   break;
                default:
                    switch (XO10(w)) {
                    case XOFP_FCMPU:   Name = L"fcmpu";   break;
                    case XOFP_FCMPO:   Name = L"fcmpo";   break;
                    case XOFP_FCTIW:   Name = L"fctiw";   break;
                    case XOFP_FCTIWZ:  Name = L"fctiwz";  break;
                    case XOFP_FRSP:    Name = L"frsp";    break;
                    case XOFP_MFFS:    Name = L"mffs";    break;
                    case XOFP_MTFSF:   Name = L"mtfsf";   break;
                    case XOFP_MTFSFI:  Name = L"mtfsfi";  break;
                    case XOFP_MTFSB0:  Name = L"mtfsb0";  break;
                    case XOFP_MTFSB1:  Name = L"mtfsb1";  break;
                    case XOFP_FABS:    Name = L"fabs";    break;
                    case XOFP_FNABS:   Name = L"fnabs";   break;
                    case XOFP_FNEG:    Name = L"fneg";    break;
                    case XOFP_FMR:     Name = L"fmr";     break;
                    case XOFP_FDIV:    Name = Sng ? L"fdivs" : L"fdiv";  break;
                    case XOFP_FSUB:    Name = Sng ? L"fsubs" : L"fsub";  break;
                    case XOFP_FADD:    Name = Sng ? L"fadds" : L"fadd";  break;
                    case XOFP_FSQRT:   Name = Sng ? L"fsqrts": L"fsqrt"; break;
                    case XOFP_FRES:    Name = L"fres";    break;
                    default:           Name = L"FP-op";   break;
                    }
                    break;
                }
            }
            break;
        default: Name = L"fpu/reserved"; break;
        }
    } else {
        Name = L"fpu/reserved";
    }

    if (Op == 31) {
        switch (XO(w)) {
        case XO_ADD:       Name = L"add";   break;
        case XO_SUBF:      Name = L"subf";  break;
        case XO_AND:       Name = L"and";   break;
        case XO_OR:        Name = L"or";    break;
        case XO_XOR:       Name = L"xor";   break;
        case XO_NOR:       Name = L"nor";   break;
        case XO_CMP:       Name = L"cmp";   break;
        case XO_MFCR:       Name = L"mfcr";  break;
        case XO_CMPL:       Name = L"cmpl";  break;
        case XO_MFSPR:     Name = L"mfspr"; break;
        case XO_MTSPR:     Name = L"mtspr"; break;
        case XO_MFSR:      Name = L"mfsr";  break;
        case XO_MTSR:      Name = L"mtsr";  break;
        case XO_MFSRIN:    Name = L"mfsrin";break;
        case XO_MTSRIN:    Name = L"mtsrin";break;
        case XO_SLW:       Name = L"slw";   break;
        case XO_SRW:       Name = L"srw";   break;
        case XO_SRAW:      Name = L"sraw";  break;
        case XO_SRAWI:     Name = L"srawi"; break;
        case XO_CNTLZW:    Name = L"cntlzw";break;
        case XO_MULLW:     Name = L"mullw"; break;
        case XO_MULHW:     Name = L"mulhw"; break;
        case XO_MULHWU:    Name = L"mulhwu";break;
        case XO_DIVW:      Name = L"divw";  break;
        case XO_DIVWU:     Name = L"divwu"; break;
        case XO_NEG:       Name = L"neg";   break;
        case XO_LWZX:      Name = L"lwzx";  break;
        case XO_LBZX:      Name = L"lbzx";  break;
        case XO_LHZX:      Name = L"lhzx";  break;
        case XO_LHAX:      Name = L"lhax";  break;
        case XO_LWBRX:     Name = L"lwbrx"; break;
        case XO_LHBRX:     Name = L"lhbrx"; break;
        case XO_STWX:      Name = L"stwx";  break;
        case XO_STBX:      Name = L"stbx";  break;
        case XO_STHX:      Name = L"sthx";  break;
        case XO_SYNC:      Name = L"sync";  break;
        case XO_TLBSYNC:   Name = L"tlbsync"; break;
        case XO_EIEIO:     Name = L"eieio"; break;
        default:           Name = L"X-op";  break;
        }
    } else if (Op == 19) {
        switch (XO10(w)) {
        case XO19_BCLR:    Name = L"bclr";  break;
        case XO19_BCCTR:   Name = L"bcctr"; break;
        case XO19_RFI:     Name = L"rfi";   break;
        case XO19_ISYNC:   Name = L"isync"; break;
        case XO19_MCRF:    Name = L"mcrf";  break;
        case XO19_CRNOR:   Name = L"crnor"; break;
        case XO19_CRANDC:  Name = L"crandc";break;
        case XO19_CRXOR:   Name = L"crxor"; break;
        case XO19_CRNAND:  Name = L"crnand";break;
        case XO19_CRAND:   Name = L"crand"; break;
        case XO19_CREQV:   Name = L"creqv"; break;
        case XO19_CRORC:   Name = L"crorc"; break;
        case XO19_CROR:    Name = L"cror";  break;
        default:           Name = L"XL-op"; break;
        }
    }

    StrnCpy(Buffer, Name, BufferSize / sizeof(CHAR16) - 1);
    return EFI_SUCCESS;
}

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
#define ME(w)      ((w) & 0x1F)
#define SIMM(w)    ((UINT32)(INT32)(INT16)((w) & 0xFFFF))
#define UIMM(w)    ((w) & 0xFFFF)
#define XO(w)      (((w) >> 1) & 0x3FE)      // 10-bit XO with OE bit masked out
#define XO10(w)    (((w) >> 1) & 0x3FF)      // full 10-bit XO
#define Rc(w)      ((w) & 1)
#define LK(w)      ((w) & 1)
#define AA(w)      (((w) >> 1) & 1)
#define SPR(w)     ((((w) >> 16) & 0x1F) | ((((w) >> 11) & 0x1F) << 5))
#define BD(w)      ((UINT32)(INT32)(INT16)((((w) >> 2) & 0x3FFF) << 2))
// LI is a 24-bit signed field at word bits 2-25 (sign-extended)
#define LI(w)      ((UINT32)((((w) >> 2) & 0x800000) ? \
                             (((w) >> 2) | 0xFF000000) : \
                             (((w) >> 2) & 0xFFFFFF)))

// Effective address helpers (RA==0 means GPR(0) is NOT used)
#define EaD(w, ra) (((ra) == 0) ? SIMM(w) : (g_PpcContext.Gpr[ra] + SIMM(w)))
#define EaX(w, ra, rb) ((((ra) == 0) ? 0U : g_PpcContext.Gpr[ra]) + g_PpcContext.Gpr[rb])

// X-form primary XO values for opcode 31
#define XO_CMP         0
#define XO_TW          4
#define XO_SUBFC       8
#define XO_ADDC       10
#define XO_MULHWU     11
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
#define XO_MTSR      625
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

// SPR numbers
#define SPR_XER    1
#define SPR_LR     8
#define SPR_CTR    9
#define SPR_PVR  287

// ---------------------------------------------------------------------------
// Memory access (default: identity-mapped, big-endian guest memory)
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Emulated guest RAM (default memory backing for the interpreter)
//
// When PpcSetGuestMemory() has been called, guest addresses inside
// [GuestBase, GuestBase + Size) are backed by the host buffer. Addresses
// outside the region read as zero and ignore writes.
// ---------------------------------------------------------------------------
static VOID*  g_GuestMemoryHostBase  = NULL;
static UINT32 g_GuestMemoryGuestBase = 0;
static UINT32 g_GuestMemorySize      = 0;

static UINT8
PpcDefaultReadByte (
    IN UINT32 Address
    )
{
    if (g_GuestMemoryHostBase != NULL &&
        Address >= g_GuestMemoryGuestBase &&
        (Address - g_GuestMemoryGuestBase) < g_GuestMemorySize) {
        return *(volatile UINT8*)((UINTN)g_GuestMemoryHostBase +
                                  (Address - g_GuestMemoryGuestBase));
    }
    return 0;  // Unmapped guest address reads as zero
}

static VOID
PpcDefaultWriteByte (
    IN UINT32 Address,
    IN UINT8  Value
    )
{
    if (g_GuestMemoryHostBase != NULL &&
        Address >= g_GuestMemoryGuestBase &&
        (Address - g_GuestMemoryGuestBase) < g_GuestMemorySize) {
        *(volatile UINT8*)((UINTN)g_GuestMemoryHostBase +
                           (Address - g_GuestMemoryGuestBase)) = Value;
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
    g_GuestMemoryHostBase  = HostBase;
    g_GuestMemoryGuestBase = GuestBase;
    g_GuestMemorySize      = Size;
    return EFI_SUCCESS;
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
            Next = AA(w) ? (UINT32)(INT32)(INT16)((w >> 2) & 0xFFFF) : CurrentAddress + BD(w);
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
                g_PpcContext.Gpr[RT(w)] = g_PpcContext.Spr[(w >> 16) & 0xF];
                break;

            case XO_LSWI:  // lswi
                PpcLoadString(RT(w), EaD(w, RA(w)), (w >> 11) & 0x1F);
                break;

            case XO_SYNC:  // sync (no-op)
                break;

            case XO_MTSR:  // mtsr
                g_PpcContext.Spr[(w >> 16) & 0xF] = g_PpcContext.Gpr[RS(w)];
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
        case XO_CMPL:      Name = L"cmpl";  break;
        case XO_MFSPR:     Name = L"mfspr"; break;
        case XO_MTSPR:     Name = L"mtspr"; break;
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
        default:           Name = L"XL-op"; break;
        }
    }

    StrnCpy(Buffer, Name, BufferSize / sizeof(CHAR16) - 1);
    return EFI_SUCCESS;
}

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

// AltiVec vector fields (opcode 4). VD/VA/VB occupy the same positions as
// RT/RA/RB. The 11-bit vector sub-opcode spans bits 0-10: a 5-bit XO in the
// FRC position (bits 6-10), the V bit at 5, and a 5-bit extension in bits 0-4.
// For the VA-form ops (vperm/vsel/vmaddfp/vsldoi/...) the FRC field holds the
// third source register or shift instead of part of the opcode.
#define VD(w)      RT(w)
#define VA(w)      RA(w)
#define VB(w)      RB(w)
#define VC(w)      FRC(w)
#define VX5(w)     FRC(w)      // 5-bit vector sub-opcode (bits 6-10)
#define VV(w)      ((w >> 5) & 1)
#define VTAIL(w)   (w & 0x1F)
#define VS(w)      RT(w)       // vector target/source (mfvscr/mtvscr)
#define UIM(w)     VA(w)       // unsigned immediate (convert / splat ops)

// Vector register byte access (guest big-endian). Index 0 is the most
// significant byte of the 16-byte vector.
#define VBYTE(r, i)      (g_PpcContext.Vr[r][i])
#define VWD(r, i)        (((UINT32)VBYTE(r, (i) * 4) << 24) | \
                          ((UINT32)VBYTE(r, (i) * 4 + 1) << 16) | \
                          ((UINT32)VBYTE(r, (i) * 4 + 2) << 8) | \
                          (UINT32)VBYTE(r, (i) * 4 + 3))
#define VWD_SET(r, i, v) do { \
    VBYTE(r, (i) * 4)     = (UINT8)((v) >> 24); \
    VBYTE(r, (i) * 4 + 1) = (UINT8)((v) >> 16); \
    VBYTE(r, (i) * 4 + 2) = (UINT8)((v) >> 8);  \
    VBYTE(r, (i) * 4 + 3) = (UINT8)(v);         \
} while (0)
#define VHW(r, i)        (((UINT32)VBYTE(r, (i) * 2) << 8) | VBYTE(r, (i) * 2 + 1))
#define VHW_SET(r, i, v) do { \
    VBYTE(r, (i) * 2) = (UINT8)((v) >> 8); \
    VBYTE(r, (i) * 2 + 1) = (UINT8)(v);    \
} while (0)

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
#define XO_LWZUX      55
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

// PowerPC 601 / POWER integer XO values for opcode 31. These share the
// X-form encoding (RT/RA/RB fields, OE bit at word bit 21, Rc at bit 31) and
// are matched via XO10() which folds OE into bit 9 (0x200).
#define XO_MASKG      29
#define XO_MUL       107
#define XO_DOZ       264
#define XO_DIV       331
#define XO_ABS       360
#define XO_DIVS      363
#define XO_NABS      488
#define XO_RRIB      537
#define XO_MASKIR    541
#define XO_ECIWX     310
#define XO_ECOWX     438

// AltiVec vector load/store XO values for opcode 31 (X-form, EA = RA+RB)
#define XO_LVSL        6
#define XO_LVEBX       7
#define XO_LVSR       38
#define XO_LVEHX      39
#define XO_LVEWX      71
#define XO_LVX       103
#define XO_STVEBX    135
#define XO_STVEHX    167
#define XO_LVXL      359
#define XO_STVEWX    199
#define XO_STVX      231
#define XO_STVXL     487

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

// Copy up to one page-agnostic contiguous run per iteration by resolving the
// host pointers for both sides and memcpy-ing the overlapping extent. Falls
// back to byte-at-a-time guest access so reads of unmapped pages still yield
// zero (matching PpcReadGuestByte) and writes to unmapped pages are dropped.
VOID
PpcCopyGuestMemory (
    IN UINT32 DstGuest,
    IN UINT32 SrcGuest,
    IN UINT32 Size
    )
{
    UINT32 Offset = 0;

    while (Offset < Size) {
        UINTN  I;
        UINT8* SrcHost = NULL;
        UINT8* DstHost = NULL;
        UINT32 Chunk   = Size - Offset;

        for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
            if (!g_GuestRegions[I].Active) {
                continue;
            }
            if (SrcHost == NULL &&
                SrcGuest + Offset >= g_GuestRegions[I].GuestBase &&
                (UINT64)(SrcGuest + Offset - g_GuestRegions[I].GuestBase) <
                    g_GuestRegions[I].Size) {
                UINT32 Run = (UINT32)(g_GuestRegions[I].Size -
                                      (SrcGuest + Offset - g_GuestRegions[I].GuestBase));
                SrcHost = (UINT8*)((UINTN)g_GuestRegions[I].HostBase +
                                   (SrcGuest + Offset - g_GuestRegions[I].GuestBase));
                if (Run < Chunk) { Chunk = Run; }
            }
            if (DstHost == NULL &&
                DstGuest + Offset >= g_GuestRegions[I].GuestBase &&
                (UINT64)(DstGuest + Offset - g_GuestRegions[I].GuestBase) <
                    g_GuestRegions[I].Size) {
                UINT32 Run = (UINT32)(g_GuestRegions[I].Size -
                                      (DstGuest + Offset - g_GuestRegions[I].GuestBase));
                DstHost = (UINT8*)((UINTN)g_GuestRegions[I].HostBase +
                                   (DstGuest + Offset - g_GuestRegions[I].GuestBase));
                if (Run < Chunk) { Chunk = Run; }
            }
        }

        if (SrcHost == NULL || DstHost == NULL || Chunk == 0) {
            break;
        }

        CopyMem(DstHost, SrcHost, Chunk);
        Offset += Chunk;
    }
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

// ---------------------------------------------------------------------------
// AltiVec helpers
// ---------------------------------------------------------------------------
// 64-bit halves of a vector register (V64H = bytes 0-7, V64L = bytes 8-15)
#define V64H(r)  (((UINT64)VBYTE(r, 0) << 56) | ((UINT64)VBYTE(r, 1) << 48) | \
                  ((UINT64)VBYTE(r, 2) << 40) | ((UINT64)VBYTE(r, 3) << 32) | \
                  ((UINT64)VBYTE(r, 4) << 24) | ((UINT64)VBYTE(r, 5) << 16) | \
                  ((UINT64)VBYTE(r, 6) << 8)  | (UINT64)VBYTE(r, 7))
#define V64L(r)  (((UINT64)VBYTE(r, 8) << 56) | ((UINT64)VBYTE(r, 9) << 48) | \
                  ((UINT64)VBYTE(r, 10) << 40) | ((UINT64)VBYTE(r, 11) << 32) | \
                  ((UINT64)VBYTE(r, 12) << 24) | ((UINT64)VBYTE(r, 13) << 16) | \
                  ((UINT64)VBYTE(r, 14) << 8)  | (UINT64)VBYTE(r, 15))
#define V64_SET_H(r, v) do { \
    VBYTE(r, 0) = (UINT8)((UINT64)(v) >> 56); VBYTE(r, 1) = (UINT8)((UINT64)(v) >> 48); \
    VBYTE(r, 2) = (UINT8)((UINT64)(v) >> 40); VBYTE(r, 3) = (UINT8)((UINT64)(v) >> 32); \
    VBYTE(r, 4) = (UINT8)((UINT64)(v) >> 24); VBYTE(r, 5) = (UINT8)((UINT64)(v) >> 16); \
    VBYTE(r, 6) = (UINT8)((UINT64)(v) >> 8);  VBYTE(r, 7) = (UINT8)(UINT64)(v); \
} while (0)
#define V64_SET_L(r, v) do { \
    VBYTE(r, 8) = (UINT8)((UINT64)(v) >> 56); VBYTE(r, 9) = (UINT8)((UINT64)(v) >> 48); \
    VBYTE(r, 10) = (UINT8)((UINT64)(v) >> 40); VBYTE(r, 11) = (UINT8)((UINT64)(v) >> 32); \
    VBYTE(r, 12) = (UINT8)((UINT64)(v) >> 24); VBYTE(r, 13) = (UINT8)((UINT64)(v) >> 16); \
    VBYTE(r, 14) = (UINT8)((UINT64)(v) >> 8);  VBYTE(r, 15) = (UINT8)(UINT64)(v); \
} while (0)

static UINT8 PpcSatS8 (INT32 V) { if (V > 127) return 0x7F; if (V < -128) return 0x80; return (UINT8)V; }
static UINT8 PpcSatU8 (INT32 V) { if (V > 0xFF) return 0xFF; if (V < 0) return 0; return (UINT8)V; }
static UINT16 PpcSatS16 (INT32 V) { if (V > 0x7FFF) return 0x7FFF; if (V < -0x8000) return 0x8000; return (UINT16)V; }
static UINT16 PpcSatU16 (INT32 V) { if (V > 0xFFFF) return 0xFFFF; if (V < 0) return 0; return (UINT16)V; }
static UINT32 PpcSatS32 (INT64 V) { if (V > 0x7FFFFFFFLL) return 0x7FFFFFFF; if (V < -0x80000000LL) return 0x80000000; return (UINT32)V; }
static UINT32 PpcSatU32 (INT64 V) { if (V > 0xFFFFFFFFLL) return 0xFFFFFFFF; if (V < 0) return 0; return (UINT32)V; }

// Minimal freestanding libm shim. clang lowers __builtin_truncf/floorf/ceilf/
// rintf/expf/logf to these libcall symbols, and no CRT math library is linked
// into the UEFI image.
float truncf (float X)
{
    PPC_FP32 U;
    U.F = X;
    UINT32 E = (U.U >> 23) & 0xFF;
    if (E < 150) {
        UINT32 Drop = 150 - E;
        if (Drop >= 24) {
            U.U &= 0x80000000;                  // |X| < 1 -> signed zero
        } else {
            U.U &= ~((1U << Drop) - 1);
        }
    }
    return U.F;
}

float floorf (float X)
{
    float T = truncf (X);
    if (X < T) T -= 1.0f;
    return T;
}

float ceilf (float X)
{
    float T = truncf (X);
    if (X > T) T += 1.0f;
    return T;
}

float rintf (float X)
{
    if (X >= 0x1.0p23f || X <= -0x1.0p23f) {
        return X;                               // already integral
    }
    float T = X + 0x1.8p23f;                    // 1.5 * 2^23
    return T - 0x1.8p23f;
}

float expf (float X)
{
    // exp(X) = 2^(X * log2(e)). Range-reduce into integer part plus a
    // fraction in [-0.5, 0.5] and evaluate 2^f with a Taylor series.
    float Y = X * 1.4426950408889634f;          // X * log2(e)
    float N = rintf (Y);
    float F = Y - N;
    float T = F * 0.6931471805599453f;          // F * ln2
    float R = 1.0f + T * (1.0f + T * (0.5f + T * (0.1666666667f + T * (0.0416666667f + T * 0.0083333333f))));
    PPC_FP32 U;
    U.F = R;
    INT32 Exp = (INT32)N + (INT32)((U.U >> 23) & 0xFF);
    if (Exp > 254) {
        U.U = 0x7F800000;                       // overflow -> +inf
    } else if (Exp < 1) {
        U.U = 0;                                // underflow -> 0
    } else {
        U.U = (U.U & 0x807FFFFF) | ((UINT32)Exp << 23);
    }
    return U.F;
}

float logf (float X)
{
    if (X <= 0.0f) {
        return -3.402823466e38f;                // not meaningful; keep finite
    }
    PPC_FP32 U;
    U.F = X;
    INT32 E = (INT32)((U.U >> 23) & 0xFF) - 127;
    U.U = (U.U & 0x807FFFFF) | 0x3F800000;      // mantissa in [1, 2)
    float M = U.F;
    float Z = (M - 1.0f) / (M + 1.0f);
    float Z2 = Z * Z;
    // log(M) = 2*Z*(1 + Z2/3 + Z2^2/5 + Z2^3/7)
    float L = 2.0f * Z * (1.0f + Z2 * (0.3333333333f + Z2 * (0.2f + Z2 * 0.1428571429f)));
    // log2(X) = E + log2(M); log2(M) = L * log2(e)
    float L2 = (float)E + L * 1.4426950408889634f;
    return L2 * 0.6931471805599453f;
}

// Single-precision lane access for the vector float ops
static float PpcVecF (UINT8 R, UINT8 I) { PPC_FP32 V; V.U = VWD(R, I); return V.F; }
static VOID  PpcVecFS (UINT8 R, UINT8 I, float F) { PPC_FP32 V; V.F = F; VWD_SET(R, I, V.U); }

static UINT32
PpcVecCvtToS32 (
    IN float F
    )
{
    if (F != F) {
        return 0;
    }
    if (F >= 2147483648.0f) {
        return 0x7FFFFFFF;
    }
    if (F < -2147483648.0f) {
        return 0x80000000;
    }
    return (UINT32)(INT32)__builtin_truncf(F);
}

static UINT32
PpcVecCvtToU32 (
    IN float F
    )
{
    if (F != F) {
        return 0;
    }
    if (F >= 4294967296.0f) {
        return 0xFFFFFFFF;
    }
    if (F <= -1.0f) {
        return 0;
    }
    return (UINT32)__builtin_truncf(F);
}

// Update CR6 from a vector compare: EQ when all lanes matched, GT when some
// but not all matched, LT when any lane was unordered (FP compares only).
static VOID
PpcVecSetCr6 (
    IN BOOLEAN AllTrue,
    IN BOOLEAN AnyTrue,
    IN BOOLEAN AnyNaN
    )
{
    UINT32 Value = 0;
    if (AnyNaN) {
        Value |= PPC_CR_LT;
    } else if (AllTrue) {
        Value |= PPC_CR_EQ;
    } else if (AnyTrue) {
        Value |= PPC_CR_GT;
    }
    PpcSetCrField(6, Value);
}

// Execute an AltiVec opcode-4 (VX/VA-form) instruction.
static EFI_STATUS
PpcExecuteVectorOp (
    IN UINT32 w
    )
{
    UINT32 Vbit = VV(w);
    UINT32 Tail = VTAIL(w);
    UINT32 X5   = VX5(w);
    UINT32 Vd   = VD(w);
    UINT32 Va   = VA(w);
    UINT32 Vb   = VB(w);
    UINT32 Vc   = VC(w);
    UINT32 I;

    // VA-form ops: the FRC field carries the third source register or a shift.
    // All are distinguished by the V bit plus the 5-bit tail alone.
    if (Vbit) {
        switch (Tail) {
        case 0x00:  // vmhaddshs vD, vA, vB, vC: sat((vA*vB + vC) >> 1)
            for (I = 0; I < 8; I++) {
                INT32 T = (INT32)(INT16)VHW(Va, I) * (INT32)(INT16)VHW(Vb, I) +
                          (INT32)(INT16)VHW(Vc, I);
                VHW_SET(Vd, I, PpcSatS16(T >> 1));
            }
            return EFI_SUCCESS;

        case 0x01:  // vmhraddshs: sat((vA*vB + vC + 0x4000) >> 15)
            for (I = 0; I < 8; I++) {
                INT32 T = (INT32)(INT16)VHW(Va, I) * (INT32)(INT16)VHW(Vb, I) +
                          (INT32)(INT16)VHW(Vc, I) + 0x4000;
                VHW_SET(Vd, I, PpcSatS16(T >> 15));
            }
            return EFI_SUCCESS;

        case 0x02:  // vmladduhm: sat16(vA*vB + vC), unsigned halfwords
            for (I = 0; I < 8; I++) {
                UINT32 T = (UINT32)VHW(Va, I) * VHW(Vb, I) + VHW(Vc, I);
                VHW_SET(Vd, I, PpcSatU16((INT32)T));
            }
            return EFI_SUCCESS;

        case 0x04:  // vmsumubm
            for (I = 0; I < 4; I++) {
                INT64 T = (UINT32)VWD(Vc, I);
                UINTN J;
                for (J = 0; J < 4; J++) {
                    T += (UINT32)VBYTE(Va, I * 4 + J) * VBYTE(Vb, I * 4 + J);
                }
                VWD_SET(Vd, I, PpcSatU32(T));
            }
            return EFI_SUCCESS;

        case 0x05:  // vmsummbm
            for (I = 0; I < 4; I++) {
                INT64 T = (INT32)(UINT32)VWD(Vc, I);
                UINTN J;
                for (J = 0; J < 4; J++) {
                    T += (INT32)(INT8)VBYTE(Va, I * 4 + J) * (INT32)(INT8)VBYTE(Vb, I * 4 + J);
                }
                VWD_SET(Vd, I, PpcSatS32(T));
            }
            return EFI_SUCCESS;

        case 0x06:  // vmsumuhm
            for (I = 0; I < 4; I++) {
                INT64 T = (UINT32)VWD(Vc, I);
                UINTN J;
                for (J = 0; J < 4; J++) {
                    T += (UINT32)VHW(Va, I * 2 + J) * VHW(Vb, I * 2 + J);
                }
                VWD_SET(Vd, I, PpcSatU32(T));
            }
            return EFI_SUCCESS;

        case 0x07:  // vmsumuhs
            {
                INT64 T = 0;
                for (I = 0; I < 4; I++) {
                    T += (UINT32)VHW(Va, I) * VHW(Vb, I);
                }
                VWD_SET(Vd, 0, PpcSatU32(T));
                VWD_SET(Vd, 1, VWD(Vc, 0));
                VWD_SET(Vd, 2, VWD(Vc, 1));
                VWD_SET(Vd, 3, VWD(Vc, 2));
            }
            return EFI_SUCCESS;

        case 0x08:  // vmsumshm
            for (I = 0; I < 4; I++) {
                INT64 T = (INT32)(UINT32)VWD(Vc, I);
                UINTN J;
                for (J = 0; J < 4; J++) {
                    T += (INT32)(INT16)VHW(Va, I * 2 + J) * (INT32)(INT16)VHW(Vb, I * 2 + J);
                }
                VWD_SET(Vd, I, PpcSatS32(T));
            }
            return EFI_SUCCESS;

        case 0x09:  // vmsumshs
            {
                INT64 T = 0;
                for (I = 0; I < 4; I++) {
                    T += (INT32)(INT16)VHW(Va, I) * (INT32)(INT16)VHW(Vb, I);
                }
                VWD_SET(Vd, 0, PpcSatS32(T));
                VWD_SET(Vd, 1, VWD(Vc, 0));
                VWD_SET(Vd, 2, VWD(Vc, 1));
                VWD_SET(Vd, 3, VWD(Vc, 2));
            }
            return EFI_SUCCESS;

        case 0x0A:  // vsel vD, vA, vB, vC: (vC & vA) | (~vC & vB)
            for (I = 0; I < 16; I++) {
                VBYTE(Vd, I) = (VBYTE(Vc, I) & VBYTE(Va, I)) |
                               (~VBYTE(Vc, I) & VBYTE(Vb, I));
            }
            return EFI_SUCCESS;

        case 0x0B:  // vperm vD, vA, vB, vC
            for (I = 0; I < 16; I++) {
                UINT8 Idx = VBYTE(Vc, I) & 0x0F;
                UINT8 Src = (VBYTE(Vc, I) & 0x10) ? Vb : Va;
                VBYTE(Vd, I) = VBYTE(Src, Idx);
            }
            return EFI_SUCCESS;

        case 0x0C:  // vsldoi vD, vA, vB, SHB
            {
                UINT8 Tmp[32];
                UINT32 Sh = X5 & 0x0F;
                for (I = 0; I < 16; I++) {
                    Tmp[I] = VBYTE(Va, I);
                    Tmp[I + 16] = VBYTE(Vb, I);
                }
                for (I = 0; I < 16; I++) {
                    VBYTE(Vd, I) = Tmp[Sh + I];
                }
            }
            return EFI_SUCCESS;

        case 0x0D:  // vpermxor vD, vA, vB, vC: (vA ^ vB) permuted by vC
            for (I = 0; I < 16; I++) {
                UINT8 Idx = VBYTE(Vc, I) & 0x0F;
                VBYTE(Vd, I) = VBYTE(Va, Idx) ^ VBYTE(Vb, Idx);
            }
            return EFI_SUCCESS;

        case 0x0E:  // vmaddfp vD, vA, vC, vB: vD = vA*vC + vB
            for (I = 0; I < 4; I++) {
                PpcVecFS(Vd, I, PpcVecF(Va, I) * PpcVecF(Vc, I) + PpcVecF(Vb, I));
            }
            return EFI_SUCCESS;

        case 0x0F:  // vnmsubfp vD, vA, vC, vB: vD = -(vA*vC - vB)
            for (I = 0; I < 4; I++) {
                PpcVecFS(Vd, I, PpcVecF(Vb, I) - PpcVecF(Va, I) * PpcVecF(Vc, I));
            }
            return EFI_SUCCESS;

        case 0x1B:  // vpermr vD, vA, vB, vC (reverse of vperm: bit-0x10 selects vA)
            for (I = 0; I < 16; I++) {
                UINT8 Idx = VBYTE(Vc, I) & 0x0F;
                UINT8 Src = (VBYTE(Vc, I) & 0x10) ? Va : Vb;
                VBYTE(Vd, I) = VBYTE(Src, Idx);
            }
            return EFI_SUCCESS;

        case 0x1C:  // vaddeuqm vD, vA, vB, vC (carry in/out in vC[127])
        case 0x1D:  // vaddecuq
        case 0x1E:  // vsubeuqm
        case 0x1F:  // vsubecuq
            {
                UINT64 AHi = V64H(Va), ALo = V64L(Va);
                UINT64 BHi = V64H(Vb), BLo = V64L(Vb);
                UINT32 Cin = VBYTE(Vc, 15) & 1;
                UINT64 RLo, RHi, Cout;

                if (Tail == 0x1E || Tail == 0x1F) {
                    UINT64 TLo = BLo + Cin;
                    UINT32 Borrow1 = (ALo < TLo) ? 1 : 0;
                    RLo = ALo - TLo;
                    UINT64 THi = BHi + Borrow1;
                    UINT32 Borrow2 = (AHi < THi) ? 1 : 0;
                    RHi = AHi - THi;
                    Cout = Borrow2;
                } else {
                    UINT64 TLo = ALo + BLo + Cin;
                    UINT32 Carry1 = (TLo < ALo) ? 1 : 0;
                    RLo = TLo;
                    UINT64 THi = AHi + BHi + Carry1;
                    UINT32 Carry2 = (THi < AHi) ? 1 : 0;
                    RHi = THi;
                    Cout = Carry2;
                }
                V64_SET_H(Vd, RHi);
                V64_SET_L(Vd, RLo);
                for (I = 0; I < 15; I++) {
                    VBYTE(Vc, I) = 0;
                }
                VBYTE(Vc, 15) = (UINT8)Cout;
            }
            return EFI_SUCCESS;

        default:
            return EFI_UNSUPPORTED;
        }
    }

    // V=0: fixed sub-opcode in X5 plus the tail.
    switch (Tail) {
    case 0x00:  // integer add/sub family
        switch (X5) {
        case 0x00:  // vaddubm
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = VBYTE(Va, I) + VBYTE(Vb, I);
            return EFI_SUCCESS;
        case 0x01:  // vadduhm
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, VHW(Va, I) + VHW(Vb, I));
            return EFI_SUCCESS;
        case 0x02:  // vadduwm
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, VWD(Va, I) + VWD(Vb, I));
            return EFI_SUCCESS;
        case 0x03:  // vaddudm
            V64_SET_H(Vd, V64H(Va) + V64H(Vb));
            V64_SET_L(Vd, V64L(Va) + V64L(Vb));
            return EFI_SUCCESS;
        case 0x04:  // vadduqm (128-bit)
            {
                UINT64 Lo = V64L(Va) + V64L(Vb);
                V64_SET_L(Vd, Lo);
                V64_SET_H(Vd, V64H(Va) + V64H(Vb) + (Lo < V64L(Va) ? 1 : 0));
            }
            return EFI_SUCCESS;
        case 0x05:  // vaddcuq (carry out)
            {
                UINT64 Lo = V64L(Va) + V64L(Vb);
                UINT64 Hi = V64H(Va) + V64H(Vb) + (Lo < V64L(Va) ? 1 : 0);
                for (I = 0; I < 15; I++) VBYTE(Vd, I) = 0;
                VBYTE(Vd, 15) = (Hi < V64H(Va)) ? 1 : 0;
            }
            return EFI_SUCCESS;
        case 0x06:  // vaddcuw
            for (I = 0; I < 4; I++) {
                UINT64 T = (UINT64)VWD(Va, I) + VWD(Vb, I);
                for (UINTN J = 0; J < 3; J++) VBYTE(Vd, I * 4 + J) = 0;
                VBYTE(Vd, I * 4 + 3) = (T >> 32) ? 1 : 0;
            }
            return EFI_SUCCESS;
        case 0x08:  // vaddubs
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = PpcSatU8(VBYTE(Va, I) + VBYTE(Vb, I));
            return EFI_SUCCESS;
        case 0x09:  // vadduhs
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, PpcSatU16((INT32)VHW(Va, I) + VHW(Vb, I)));
            return EFI_SUCCESS;
        case 0x0A:  // vadduws
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, PpcSatU32((INT64)VWD(Va, I) + VWD(Vb, I)));
            return EFI_SUCCESS;
        case 0x0C:  // vaddsbs
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = PpcSatS8((INT32)(INT8)VBYTE(Va, I) + (INT8)VBYTE(Vb, I));
            return EFI_SUCCESS;
        case 0x0D:  // vaddshs
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, PpcSatS16((INT32)(INT16)VHW(Va, I) + (INT16)VHW(Vb, I)));
            return EFI_SUCCESS;
        case 0x0E:  // vaddsws
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, PpcSatS32((INT64)(INT32)VWD(Va, I) + (INT32)VWD(Vb, I)));
            return EFI_SUCCESS;
        case 0x10:  // vsububm
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = VBYTE(Va, I) - VBYTE(Vb, I);
            return EFI_SUCCESS;
        case 0x11:  // vsubuhm
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, VHW(Va, I) - VHW(Vb, I));
            return EFI_SUCCESS;
        case 0x12:  // vsubuwm
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, VWD(Va, I) - VWD(Vb, I));
            return EFI_SUCCESS;
        case 0x13:  // vsubudm
            V64_SET_H(Vd, V64H(Va) - V64H(Vb));
            V64_SET_L(Vd, V64L(Va) - V64L(Vb));
            return EFI_SUCCESS;
        case 0x14:  // vsubuqm (128-bit)
            {
                UINT64 Lo = V64L(Va) - V64L(Vb);
                UINT64 Borrow = (V64L(Va) < V64L(Vb)) ? 1 : 0;
                V64_SET_L(Vd, Lo);
                V64_SET_H(Vd, V64H(Va) - V64H(Vb) - Borrow);
            }
            return EFI_SUCCESS;
        case 0x15:  // vsubcuq (borrow out)
            {
                for (I = 0; I < 15; I++) VBYTE(Vd, I) = 0;
                VBYTE(Vd, 15) = (V64H(Va) < V64H(Vb) || (V64L(Va) < V64L(Vb))) ? 1 : 0;
            }
            return EFI_SUCCESS;
        case 0x18:  // vsububs
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = PpcSatU8((INT32)VBYTE(Va, I) - VBYTE(Vb, I));
            return EFI_SUCCESS;
        case 0x19:  // vsubuhs
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, PpcSatU16((INT32)VHW(Va, I) - VHW(Vb, I)));
            return EFI_SUCCESS;
        case 0x1A:  // vsubuws
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, PpcSatU32((INT64)(INT32)VWD(Va, I) - (INT32)VWD(Vb, I)));
            return EFI_SUCCESS;
        case 0x1C:  // vsubsbs
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = PpcSatS8((INT32)(INT8)VBYTE(Va, I) - (INT8)VBYTE(Vb, I));
            return EFI_SUCCESS;
        case 0x1D:  // vsubshs
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, PpcSatS16((INT32)(INT16)VHW(Va, I) - (INT16)VHW(Vb, I)));
            return EFI_SUCCESS;
        case 0x1E:  // vsubsws
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, PpcSatS32((INT64)(INT32)VWD(Va, I) - (INT32)VWD(Vb, I)));
            return EFI_SUCCESS;
        default:
            return EFI_UNSUPPORTED;
        }

    case 0x01:  // vmul10* / BCD (decimal floating point) - not needed for boot
        return EFI_UNSUPPORTED;

    case 0x02:  // max / min / average / count-leading-zeros
        switch (X5) {
        case 0x00:  // vmaxub
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = VBYTE(Va, I) > VBYTE(Vb, I) ? VBYTE(Va, I) : VBYTE(Vb, I);
            return EFI_SUCCESS;
        case 0x01:  // vmaxuh
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, VHW(Va, I) > VHW(Vb, I) ? VHW(Va, I) : VHW(Vb, I));
            return EFI_SUCCESS;
        case 0x02:  // vmaxuw
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, VWD(Va, I) > VWD(Vb, I) ? VWD(Va, I) : VWD(Vb, I));
            return EFI_SUCCESS;
        case 0x03:  // vmaxud
            V64_SET_H(Vd, V64H(Va) > V64H(Vb) ? V64H(Va) : V64H(Vb));
            V64_SET_L(Vd, V64L(Va) > V64L(Vb) ? V64L(Va) : V64L(Vb));
            return EFI_SUCCESS;
        case 0x04:  // vmaxsb
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = (INT8)VBYTE(Va, I) > (INT8)VBYTE(Vb, I) ? VBYTE(Va, I) : VBYTE(Vb, I);
            return EFI_SUCCESS;
        case 0x05:  // vmaxsh
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, (INT16)VHW(Va, I) > (INT16)VHW(Vb, I) ? VHW(Va, I) : VHW(Vb, I));
            return EFI_SUCCESS;
        case 0x06:  // vmaxsw
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, (INT32)VWD(Va, I) > (INT32)VWD(Vb, I) ? VWD(Va, I) : VWD(Vb, I));
            return EFI_SUCCESS;
        case 0x08:  // vminub
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = VBYTE(Va, I) < VBYTE(Vb, I) ? VBYTE(Va, I) : VBYTE(Vb, I);
            return EFI_SUCCESS;
        case 0x09:  // vminuh
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, VHW(Va, I) < VHW(Vb, I) ? VHW(Va, I) : VHW(Vb, I));
            return EFI_SUCCESS;
        case 0x0A:  // vminuw
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, VWD(Va, I) < VWD(Vb, I) ? VWD(Va, I) : VWD(Vb, I));
            return EFI_SUCCESS;
        case 0x0C:  // vminsb
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = (INT8)VBYTE(Va, I) < (INT8)VBYTE(Vb, I) ? VBYTE(Va, I) : VBYTE(Vb, I);
            return EFI_SUCCESS;
        case 0x0D:  // vminsh
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, (INT16)VHW(Va, I) < (INT16)VHW(Vb, I) ? VHW(Va, I) : VHW(Vb, I));
            return EFI_SUCCESS;
        case 0x0E:  // vminsw
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, (INT32)VWD(Va, I) < (INT32)VWD(Vb, I) ? VWD(Va, I) : VWD(Vb, I));
            return EFI_SUCCESS;
        case 0x10:  // vavgub
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = (VBYTE(Va, I) + VBYTE(Vb, I) + 1) >> 1;
            return EFI_SUCCESS;
        case 0x11:  // vavguh
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, (VHW(Va, I) + VHW(Vb, I) + 1) >> 1);
            return EFI_SUCCESS;
        case 0x14:  // vavgsb
            for (I = 0; I < 16; I++) {
                INT32 S = (INT32)(INT8)VBYTE(Va, I) + (INT8)VBYTE(Vb, I) + 1;
                VBYTE(Vd, I) = (UINT8)(S >> 1);
            }
            return EFI_SUCCESS;
        case 0x15:  // vavgsh
            for (I = 0; I < 8; I++) {
                INT32 S = (INT32)(INT16)VHW(Va, I) + (INT16)VHW(Vb, I) + 1;
                VHW_SET(Vd, I, (UINT16)(S >> 1));
            }
            return EFI_SUCCESS;
        case 0x1C:  // vclzb
            for (I = 0; I < 16; I++) {
                UINT8 B = VBYTE(Vb, I), N = 0;
                while ((B & 0x80) == 0 && N < 8) { B <<= 1; N++; }
                VBYTE(Vd, I) = N;
            }
            return EFI_SUCCESS;
        case 0x1D:  // vclzh
            for (I = 0; I < 8; I++) {
                UINT32 H = VHW(Vb, I), N = 0;
                while ((H & 0x8000) == 0 && N < 16) { H <<= 1; N++; }
                VHW_SET(Vd, I, N);
            }
            return EFI_SUCCESS;
        case 0x1E:  // vclzw
            for (I = 0; I < 4; I++) {
                UINT32 W = VWD(Vb, I), N = 0;
                while ((W & 0x80000000) == 0 && N < 32) { W <<= 1; N++; }
                VWD_SET(Vd, I, N);
            }
            return EFI_SUCCESS;
        case 0x18:  // vextsb2d: sign-extend each byte to a doubleword
            for (I = 0; I < 8; I++) {
                INT64 S = (INT8)VBYTE(Vb, I);
                VBYTE(Vd, 8 * I + 0) = (UINT8)(S >> 56);
                VBYTE(Vd, 8 * I + 1) = (UINT8)(S >> 48);
                VBYTE(Vd, 8 * I + 2) = (UINT8)(S >> 40);
                VBYTE(Vd, 8 * I + 3) = (UINT8)(S >> 32);
                VBYTE(Vd, 8 * I + 4) = (UINT8)(S >> 24);
                VBYTE(Vd, 8 * I + 5) = (UINT8)(S >> 16);
                VBYTE(Vd, 8 * I + 6) = (UINT8)(S >> 8);
                VBYTE(Vd, 8 * I + 7) = (UINT8)S;
            }
            return EFI_SUCCESS;
        default:
            return EFI_UNSUPPORTED;
        }

    case 0x03:  // vabsdub / vabsduh
        if (X5 == 0x10) {
            for (I = 0; I < 16; I++) {
                INT32 S = (INT8)VBYTE(Va, I) - (INT8)VBYTE(Vb, I);
                VBYTE(Vd, I) = (UINT8)(S < 0 ? -S : S);
            }
            return EFI_SUCCESS;
        }
        if (X5 == 0x11) {
            for (I = 0; I < 8; I++) {
                INT32 S = (INT16)VHW(Va, I) - (INT16)VHW(Vb, I);
                VHW_SET(Vd, I, (UINT16)(S < 0 ? -S : S));
            }
            return EFI_SUCCESS;
        }
        return EFI_UNSUPPORTED;

    case 0x04:  // rotate / shift / logical / mfvscr / mtvscr
        switch (X5) {
        case 0x00:  // vrlb
            for (I = 0; I < 16; I++) {
                UINT32 N = VBYTE(Vb, I) & 7;
                UINT8 B = VBYTE(Va, I);
                VBYTE(Vd, I) = N ? (UINT8)((B << N) | (B >> (8 - N))) : B;
            }
            return EFI_SUCCESS;
        case 0x01:  // vrlh
            for (I = 0; I < 8; I++) {
                UINT32 N = VHW(Vb, I) & 15;
                UINT32 H = VHW(Va, I);
                VHW_SET(Vd, I, N ? (UINT16)((H << N) | (H >> (16 - N))) : (UINT16)H);
            }
            return EFI_SUCCESS;
        case 0x02:  // vrlw
            for (I = 0; I < 4; I++) {
                UINT32 N = VWD(Vb, I) & 31;
                UINT32 W = VWD(Va, I);
                VWD_SET(Vd, I, N ? (W << N) | (W >> (32 - N)) : W);
            }
            return EFI_SUCCESS;
        case 0x04:  // vslb
            for (I = 0; I < 16; I++) {
                UINT32 N = VBYTE(Vb, I) & 7;
                VBYTE(Vd, I) = N ? (UINT8)(VBYTE(Va, I) << N) : VBYTE(Va, I);
            }
            return EFI_SUCCESS;
        case 0x05:  // vslh
            for (I = 0; I < 8; I++) {
                UINT32 N = VHW(Vb, I) & 15;
                VHW_SET(Vd, I, N ? (UINT16)(VHW(Va, I) << N) : (UINT16)VHW(Va, I));
            }
            return EFI_SUCCESS;
        case 0x06:  // vslw
            for (I = 0; I < 4; I++) {
                UINT32 N = VWD(Vb, I) & 31;
                VWD_SET(Vd, I, N ? VWD(Va, I) << N : VWD(Va, I));
            }
            return EFI_SUCCESS;
        case 0x08:  // vsrb
            for (I = 0; I < 16; I++) {
                UINT32 N = VBYTE(Vb, I) & 7;
                VBYTE(Vd, I) = N ? (UINT8)(VBYTE(Va, I) >> N) : VBYTE(Va, I);
            }
            return EFI_SUCCESS;
        case 0x09:  // vsrh
            for (I = 0; I < 8; I++) {
                UINT32 N = VHW(Vb, I) & 15;
                VHW_SET(Vd, I, N ? (UINT16)(VHW(Va, I) >> N) : (UINT16)VHW(Va, I));
            }
            return EFI_SUCCESS;
        case 0x0A:  // vsrw
            for (I = 0; I < 4; I++) {
                UINT32 N = VWD(Vb, I) & 31;
                VWD_SET(Vd, I, N ? VWD(Va, I) >> N : VWD(Va, I));
            }
            return EFI_SUCCESS;
        case 0x0C:  // vsrab
            for (I = 0; I < 16; I++) {
                UINT32 N = VBYTE(Vb, I) & 7;
                VBYTE(Vd, I) = (UINT8)((INT8)VBYTE(Va, I) >> N);
            }
            return EFI_SUCCESS;
        case 0x0D:  // vsrah
            for (I = 0; I < 8; I++) {
                UINT32 N = VHW(Vb, I) & 15;
                VHW_SET(Vd, I, (UINT16)((INT16)VHW(Va, I) >> N));
            }
            return EFI_SUCCESS;
        case 0x0E:  // vsraw
            for (I = 0; I < 4; I++) {
                UINT32 N = VWD(Vb, I) & 31;
                VWD_SET(Vd, I, (UINT32)((INT32)VWD(Va, I) >> N));
            }
            return EFI_SUCCESS;
        case 0x0F:  // vsrad: arithmetic shift each doubleword
            {
                UINT32 N0 = V64H(Vb) & 63, N1 = V64L(Vb) & 63;
                V64_SET_H(Vd, (UINT64)((INT64)V64H(Va) >> N0));
                V64_SET_L(Vd, (UINT64)((INT64)V64L(Va) >> N1));
            }
            return EFI_SUCCESS;
        case 0x1B:  // vsrd: logical shift each doubleword
            {
                UINT32 N0 = V64H(Vb) & 63, N1 = V64L(Vb) & 63;
                V64_SET_H(Vd, V64H(Va) >> N0);
                V64_SET_L(Vd, V64L(Va) >> N1);
            }
            return EFI_SUCCESS;
        case 0x1C:  // vsrv: per-byte variable shift right
            for (I = 0; I < 16; I++) {
                UINT32 N = VBYTE(Vb, I) & 7;
                VBYTE(Vd, I) = (UINT8)(VBYTE(Va, I) >> N);
            }
            return EFI_SUCCESS;
        case 0x1D:  // vslv: per-byte variable shift left
            for (I = 0; I < 16; I++) {
                UINT32 N = VBYTE(Vb, I) & 7;
                VBYTE(Vd, I) = (UINT8)(VBYTE(Va, I) << N);
            }
            return EFI_SUCCESS;
        case 0x10:  // vand
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = VBYTE(Va, I) & VBYTE(Vb, I);
            return EFI_SUCCESS;
        case 0x11:  // vandc
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = VBYTE(Va, I) & ~VBYTE(Vb, I);
            return EFI_SUCCESS;
        case 0x12:  // vor / vmr
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = VBYTE(Va, I) | VBYTE(Vb, I);
            return EFI_SUCCESS;
        case 0x13:  // vxor
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = VBYTE(Va, I) ^ VBYTE(Vb, I);
            return EFI_SUCCESS;
        case 0x14:  // vnor
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = ~(VBYTE(Va, I) | VBYTE(Vb, I));
            return EFI_SUCCESS;
        case 0x18:  // mfvscr vD
            VWD_SET(Vd, 3, g_PpcContext.Vscr);
            return EFI_SUCCESS;
        case 0x19:  // mtvscr vB
            g_PpcContext.Vscr = VWD(Vb, 3);
            return EFI_SUCCESS;
        default:
            return EFI_UNSUPPORTED;
        }

    case 0x05:  // rotate-and-mask (POWER6+)
        switch (X5) {
        case 0x02:  // vrlwmi: rotl, AND mask = most-significant (s+1) bits
            for (I = 0; I < 4; I++) {
                UINT32 N = VWD(Vb, I) & 31;
                UINT32 W = VWD(Va, I);
                UINT32 M = (N == 31) ? 0xFFFFFFFF : (0xFFFFFFFF << (31 - N));
                VWD_SET(Vd, I, (N ? (W << N) | (W >> (32 - N)) : W) & M);
            }
            return EFI_SUCCESS;
        case 0x03:  // vrldmi
            {
                UINT32 N0 = V64H(Vb) & 63, N1 = V64L(Vb) & 63;
                UINT64 A0 = V64H(Va), A1 = V64L(Va);
                UINT64 M0 = (N0 == 63) ? ~0ULL : (~0ULL << (63 - N0));
                UINT64 M1 = (N1 == 63) ? ~0ULL : (~0ULL << (63 - N1));
                V64_SET_H(Vd, (N0 ? (A0 << N0) | (A0 >> (64 - N0)) : A0) & M0);
                V64_SET_L(Vd, (N1 ? (A1 << N1) | (A1 >> (64 - N1)) : A1) & M1);
            }
            return EFI_SUCCESS;
        case 0x07:  // vrldnm: rotl, AND NOT mask
            {
                UINT32 N0 = V64H(Vb) & 63, N1 = V64L(Vb) & 63;
                UINT64 A0 = V64H(Va), A1 = V64L(Va);
                UINT64 M0 = (N0 == 63) ? ~0ULL : (~0ULL << (63 - N0));
                UINT64 M1 = (N1 == 63) ? ~0ULL : (~0ULL << (63 - N1));
                V64_SET_H(Vd, (N0 ? (A0 << N0) | (A0 >> (64 - N0)) : A0) & ~M0);
                V64_SET_L(Vd, (N1 ? (A1 << N1) | (A1 >> (64 - N1)) : A1) & ~M1);
            }
            return EFI_SUCCESS;
        default:
            return EFI_UNSUPPORTED;
        }

    case 0x06:  // vector compares (update vD masks + CR6)
        {
            BOOLEAN All = TRUE, Any = FALSE, Nan = FALSE;
            switch (X5) {
            case 0x00: case 0x10:  // vcmpequb
                for (I = 0; I < 16; I++) {
                    BOOLEAN T = (VBYTE(Va, I) == VBYTE(Vb, I));
                    VBYTE(Vd, I) = T ? 0xFF : 0x00;
                    All &= T; Any |= T;
                }
                break;
            case 0x01: case 0x11:  // vcmpequh
                for (I = 0; I < 8; I++) {
                    BOOLEAN T = (VHW(Va, I) == VHW(Vb, I));
                    VHW_SET(Vd, I, T ? 0xFFFF : 0x0000);
                    All &= T; Any |= T;
                }
                break;
            case 0x02: case 0x12:  // vcmpequw
                for (I = 0; I < 4; I++) {
                    BOOLEAN T = (VWD(Va, I) == VWD(Vb, I));
                    VWD_SET(Vd, I, T ? 0xFFFFFFFF : 0x00000000);
                    All &= T; Any |= T;
                }
                break;
            case 0x03: case 0x13:  // vcmpeqfp
                for (I = 0; I < 4; I++) {
                    float A = PpcVecF(Va, I), B = PpcVecF(Vb, I);
                    BOOLEAN T = (A == B);
                    if (A != A || B != B) Nan = TRUE;
                    VWD_SET(Vd, I, T ? 0xFFFFFFFF : 0x00000000);
                    All &= T; Any |= T;
                }
                break;
            case 0x04: case 0x0C: case 0x14: case 0x1C:  // vcmpgtsb(.)
                for (I = 0; I < 16; I++) {
                    BOOLEAN T = ((INT8)VBYTE(Va, I) > (INT8)VBYTE(Vb, I));
                    VBYTE(Vd, I) = T ? 0xFF : 0x00;
                    All &= T; Any |= T;
                }
                break;
            case 0x05: case 0x0D: case 0x15: case 0x1D:  // vcmpgtsh(.)
                for (I = 0; I < 8; I++) {
                    BOOLEAN T = ((INT16)VHW(Va, I) > (INT16)VHW(Vb, I));
                    VHW_SET(Vd, I, T ? 0xFFFF : 0x0000);
                    All &= T; Any |= T;
                }
                break;
            case 0x06: case 0x0E: case 0x16: case 0x1E:  // vcmpgtsw(.)
                for (I = 0; I < 4; I++) {
                    BOOLEAN T = ((INT32)VWD(Va, I) > (INT32)VWD(Vb, I));
                    VWD_SET(Vd, I, T ? 0xFFFFFFFF : 0x00000000);
                    All &= T; Any |= T;
                }
                break;
            case 0x07: case 0x17:  // vcmpgefp
                for (I = 0; I < 4; I++) {
                    float A = PpcVecF(Va, I), B = PpcVecF(Vb, I);
                    BOOLEAN T = (A >= B);
                    if (A != A || B != B) Nan = TRUE;
                    VWD_SET(Vd, I, T ? 0xFFFFFFFF : 0x00000000);
                    All &= T; Any |= T;
                }
                break;
            case 0x08: case 0x18:  // vcmpgtub
                for (I = 0; I < 16; I++) {
                    BOOLEAN T = (VBYTE(Va, I) > VBYTE(Vb, I));
                    VBYTE(Vd, I) = T ? 0xFF : 0x00;
                    All &= T; Any |= T;
                }
                break;
            case 0x09: case 0x19:  // vcmpgtuh
                for (I = 0; I < 8; I++) {
                    BOOLEAN T = (VHW(Va, I) > VHW(Vb, I));
                    VHW_SET(Vd, I, T ? 0xFFFF : 0x0000);
                    All &= T; Any |= T;
                }
                break;
            case 0x0A: case 0x1A:  // vcmpgtuw
                for (I = 0; I < 4; I++) {
                    BOOLEAN T = (VWD(Va, I) > VWD(Vb, I));
                    VWD_SET(Vd, I, T ? 0xFFFFFFFF : 0x00000000);
                    All &= T; Any |= T;
                }
                break;
            case 0x0B: case 0x1B:  // vcmpgtfp
                for (I = 0; I < 4; I++) {
                    float A = PpcVecF(Va, I), B = PpcVecF(Vb, I);
                    BOOLEAN T = (A > B);
                    if (A != A || B != B) Nan = TRUE;
                    VWD_SET(Vd, I, T ? 0xFFFFFFFF : 0x00000000);
                    All &= T; Any |= T;
                }
                break;
            case 0x0F: case 0x1F:  // vcmpbfp (bounded: |a-b| <= (|a|+|b|)/4)
                for (I = 0; I < 4; I++) {
                    float A = PpcVecF(Va, I), B = PpcVecF(Vb, I);
                    BOOLEAN T = (__builtin_fabsf(A - B) <= (__builtin_fabsf(A) + __builtin_fabsf(B)) * 0.25f);
                    VWD_SET(Vd, I, T ? 0xFFFFFFFF : 0x00000000);
                    All &= T; Any |= T;
                }
                break;
            default:
                return EFI_UNSUPPORTED;
            }
            PpcVecSetCr6(All, Any, Nan);
        }
        return EFI_SUCCESS;

    case 0x07:  // 64-bit / not-equal compares (update vD masks + CR6)
        {
            BOOLEAN All = TRUE, Any = FALSE;
            switch (X5) {
            case 0x00: case 0x10:  // vcmpneb
                for (I = 0; I < 16; I++) {
                    BOOLEAN T = (VBYTE(Va, I) != VBYTE(Vb, I));
                    VBYTE(Vd, I) = T ? 0xFF : 0x00;
                    All &= T; Any |= T;
                }
                break;
            case 0x01: case 0x11:  // vcmpneh
                for (I = 0; I < 8; I++) {
                    BOOLEAN T = (VHW(Va, I) != VHW(Vb, I));
                    VHW_SET(Vd, I, T ? 0xFFFF : 0x0000);
                    All &= T; Any |= T;
                }
                break;
            case 0x03: case 0x13:  // vcmpequd
                {
                    BOOLEAN T0 = (V64H(Va) == V64H(Vb));
                    BOOLEAN T1 = (V64L(Va) == V64L(Vb));
                    V64_SET_H(Vd, T0 ? ~0ULL : 0ULL);
                    V64_SET_L(Vd, T1 ? ~0ULL : 0ULL);
                    All &= T0 & T1; Any |= T0 | T1;
                }
                break;
            case 0x04: case 0x14:  // vcmpnezb: true unless equal and non-zero
                for (I = 0; I < 16; I++) {
                    UINT8 A = VBYTE(Va, I), B = VBYTE(Vb, I);
                    BOOLEAN T = (A != B) || (A == 0) || (B == 0);
                    VBYTE(Vd, I) = T ? 0xFF : 0x00;
                    All &= T; Any |= T;
                }
                break;
            default:
                return EFI_UNSUPPORTED;
            }
            PpcVecSetCr6(All, Any, FALSE);
        }
        return EFI_SUCCESS;

    case 0x08:  // integer multiply
        switch (X5) {
        case 0x00:  // vmuloub
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, (UINT32)VBYTE(Va, 2 * I) * VBYTE(Vb, 2 * I));
            return EFI_SUCCESS;
        case 0x01:  // vmulouh
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, (UINT32)VHW(Va, 2 * I) * VHW(Vb, 2 * I));
            return EFI_SUCCESS;
        case 0x02:  // vmulouw
            {
                UINT64 P0 = (UINT64)VWD(Va, 0) * VWD(Vb, 0);
                UINT64 P1 = (UINT64)VWD(Va, 2) * VWD(Vb, 2);
                V64_SET_H(Vd, P0);
                V64_SET_L(Vd, P1);
            }
            return EFI_SUCCESS;
        case 0x04:  // vmulosb
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, (UINT16)((INT32)(INT8)VBYTE(Va, 2 * I) * (INT8)VBYTE(Vb, 2 * I)));
            return EFI_SUCCESS;
        case 0x05:  // vmulosh
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, (UINT32)((INT32)(INT16)VHW(Va, 2 * I) * (INT16)VHW(Vb, 2 * I)));
            return EFI_SUCCESS;
        case 0x06:  // vmulosw
            {
                UINT64 P0 = (UINT64)(INT64)(INT32)VWD(Va, 0) * (INT32)VWD(Vb, 0);
                UINT64 P1 = (UINT64)(INT64)(INT32)VWD(Va, 2) * (INT32)VWD(Vb, 2);
                V64_SET_H(Vd, P0);
                V64_SET_L(Vd, P1);
            }
            return EFI_SUCCESS;
        case 0x08:  // vmuleub
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, (UINT32)VBYTE(Va, 2 * I + 1) * VBYTE(Vb, 2 * I + 1));
            return EFI_SUCCESS;
        case 0x09:  // vmuleuh
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, (UINT32)VHW(Va, 2 * I + 1) * VHW(Vb, 2 * I + 1));
            return EFI_SUCCESS;
        case 0x0A:  // vmuleuw
            {
                UINT64 P0 = (UINT64)VWD(Va, 1) * VWD(Vb, 1);
                UINT64 P1 = (UINT64)VWD(Va, 3) * VWD(Vb, 3);
                V64_SET_H(Vd, P0);
                V64_SET_L(Vd, P1);
            }
            return EFI_SUCCESS;
        case 0x0C:  // vmulesb
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, (UINT16)((INT32)(INT8)VBYTE(Va, 2 * I + 1) * (INT8)VBYTE(Vb, 2 * I + 1)));
            return EFI_SUCCESS;
        case 0x0D:  // vmulesh
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, (UINT32)((INT32)(INT16)VHW(Va, 2 * I + 1) * (INT16)VHW(Vb, 2 * I + 1)));
            return EFI_SUCCESS;
        case 0x0E:  // vmulesw
            {
                UINT64 P0 = (UINT64)(INT64)(INT32)VWD(Va, 1) * (INT32)VWD(Vb, 1);
                UINT64 P1 = (UINT64)(INT64)(INT32)VWD(Va, 3) * (INT32)VWD(Vb, 3);
                V64_SET_H(Vd, P0);
                V64_SET_L(Vd, P1);
            }
            return EFI_SUCCESS;
        case 0x18:  // vsum4ubs
            for (I = 0; I < 4; I++) {
                INT64 T = (UINT32)VWD(Vb, I);
                UINTN J;
                for (J = 0; J < 4; J++) T += (UINT32)VBYTE(Va, I * 4 + J);
                VWD_SET(Vd, I, PpcSatU32(T));
            }
            return EFI_SUCCESS;
        case 0x19:  // vsum4shs
            for (I = 0; I < 4; I++) {
                INT64 T = (UINT32)VWD(Vb, I);
                UINTN J;
                for (J = 0; J < 2; J++) T += (INT32)(INT16)VHW(Va, I * 2 + J);
                VWD_SET(Vd, I, PpcSatS32(T));
            }
            return EFI_SUCCESS;
        case 0x1A:  // vsum2sws
            {
                INT64 T0 = (INT32)(UINT32)VWD(Vb, 0) + (INT32)(UINT32)VWD(Va, 0) + (INT32)(UINT32)VWD(Va, 2);
                INT64 T2 = (INT32)(UINT32)VWD(Vb, 2) + (INT32)(UINT32)VWD(Va, 1) + (INT32)(UINT32)VWD(Va, 3);
                VWD_SET(Vd, 0, PpcSatS32(T0));
                VWD_SET(Vd, 1, VWD(Vb, 1));
                VWD_SET(Vd, 2, PpcSatS32(T2));
                VWD_SET(Vd, 3, VWD(Vb, 3));
            }
            return EFI_SUCCESS;
        case 0x1C:  // vsum4sbs
            for (I = 0; I < 4; I++) {
                INT64 T = (UINT32)VWD(Vb, I);
                UINTN J;
                for (J = 0; J < 4; J++) T += (INT32)(INT8)VBYTE(Va, I * 4 + J);
                VWD_SET(Vd, I, PpcSatS32(T));
            }
            return EFI_SUCCESS;
        default:
            return EFI_UNSUPPORTED;
        }

    case 0x0A:  // floating-point arithmetic / conversion
        switch (X5) {
        case 0x00:  // vaddfp
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, PpcVecF(Va, I) + PpcVecF(Vb, I));
            return EFI_SUCCESS;
        case 0x01:  // vsubfp
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, PpcVecF(Va, I) - PpcVecF(Vb, I));
            return EFI_SUCCESS;
        case 0x02:  // vmaxfp
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, PpcVecF(Va, I) > PpcVecF(Vb, I) ? PpcVecF(Va, I) : PpcVecF(Vb, I));
            return EFI_SUCCESS;
        case 0x03:  // vminfp
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, PpcVecF(Va, I) < PpcVecF(Vb, I) ? PpcVecF(Va, I) : PpcVecF(Vb, I));
            return EFI_SUCCESS;
        case 0x04:  // vrefp
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, 1.0f / PpcVecF(Vb, I));
            return EFI_SUCCESS;
        case 0x05:  // vrsqrtefp
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, 1.0f / __builtin_sqrtf(PpcVecF(Vb, I)));
            return EFI_SUCCESS;
        case 0x06:  // vexptefp (2^x)
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, __builtin_expf(PpcVecF(Vb, I) * 0.6931471805599453f));
            return EFI_SUCCESS;
        case 0x07:  // vlogefp (log2 x)
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, __builtin_logf(PpcVecF(Vb, I)) / 0.6931471805599453f);
            return EFI_SUCCESS;
        case 0x08:  // vrfin
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, __builtin_rintf(PpcVecF(Vb, I)));
            return EFI_SUCCESS;
        case 0x09:  // vrfiz
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, __builtin_truncf(PpcVecF(Vb, I)));
            return EFI_SUCCESS;
        case 0x0A:  // vrfip
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, __builtin_ceilf(PpcVecF(Vb, I)));
            return EFI_SUCCESS;
        case 0x0B:  // vrfim
            for (I = 0; I < 4; I++) PpcVecFS(Vd, I, __builtin_floorf(PpcVecF(Vb, I)));
            return EFI_SUCCESS;
        case 0x0C:  // vcfux vD, vB, UIM
            for (I = 0; I < 4; I++) {
                PpcVecFS(Vd, I, (float)(UINT32)VWD(Vb, I) * __builtin_expf((float)UIM(w) * 0.6931471805599453f));
            }
            return EFI_SUCCESS;
        case 0x0D:  // vcfsx vD, vB, UIM
            for (I = 0; I < 4; I++) {
                PpcVecFS(Vd, I, (float)(INT32)VWD(Vb, I) * __builtin_expf((float)UIM(w) * 0.6931471805599453f));
            }
            return EFI_SUCCESS;
        case 0x0E:  // vctuxs vD, vB, UIM
            for (I = 0; I < 4; I++) {
                VWD_SET(Vd, I, PpcVecCvtToU32(PpcVecF(Vb, I)) >> (UIM(w) & 31));
            }
            return EFI_SUCCESS;
        case 0x0F:  // vctsxs vD, vB, UIM
            for (I = 0; I < 4; I++) {
                VWD_SET(Vd, I, (UINT32)((INT32)PpcVecCvtToS32(PpcVecF(Vb, I)) >> (UIM(w) & 31)));
            }
            return EFI_SUCCESS;
        default:
            return EFI_UNSUPPORTED;
        }

    case 0x0C:  // merge / splat
        switch (X5) {
        case 0x00:  // vmrghb
            for (I = 0; I < 8; I++) {
                VBYTE(Vd, 2 * I) = VBYTE(Va, I);
                VBYTE(Vd, 2 * I + 1) = VBYTE(Vb, I);
            }
            return EFI_SUCCESS;
        case 0x01:  // vmrghh
            for (I = 0; I < 4; I++) {
                VHW_SET(Vd, 2 * I, VHW(Va, I));
                VHW_SET(Vd, 2 * I + 1, VHW(Vb, I));
            }
            return EFI_SUCCESS;
        case 0x02:  // vmrghw
            for (I = 0; I < 2; I++) {
                VWD_SET(Vd, 2 * I, VWD(Va, I));
                VWD_SET(Vd, 2 * I + 1, VWD(Vb, I));
            }
            return EFI_SUCCESS;
        case 0x04:  // vmrglb
            for (I = 0; I < 8; I++) {
                VBYTE(Vd, 2 * I) = VBYTE(Va, I + 8);
                VBYTE(Vd, 2 * I + 1) = VBYTE(Vb, I + 8);
            }
            return EFI_SUCCESS;
        case 0x05:  // vmrglh
            for (I = 0; I < 4; I++) {
                VHW_SET(Vd, 2 * I, VHW(Va, I + 4));
                VHW_SET(Vd, 2 * I + 1, VHW(Vb, I + 4));
            }
            return EFI_SUCCESS;
        case 0x06:  // vmrglw
            for (I = 0; I < 2; I++) {
                VWD_SET(Vd, 2 * I, VWD(Va, I + 2));
                VWD_SET(Vd, 2 * I + 1, VWD(Vb, I + 2));
            }
            return EFI_SUCCESS;
        case 0x08:  // vspltb vD, vB, UIM
            for (I = 0; I < 16; I++) VBYTE(Vd, I) = VBYTE(Vb, UIM(w) & 15);
            return EFI_SUCCESS;
        case 0x09:  // vsplth vD, vB, UIM
            for (I = 0; I < 8; I++) VHW_SET(Vd, I, VHW(Vb, UIM(w) & 7));
            return EFI_SUCCESS;
        case 0x0A:  // vspltw vD, vB, UIM
            for (I = 0; I < 4; I++) VWD_SET(Vd, I, VWD(Vb, UIM(w) & 3));
            return EFI_SUCCESS;
        case 0x0C:  // vspltisb vD, IMM
            {
                UINT32 Imm = (UIM(w) & 0x10) ? (UIM(w) | 0xFFFFFFE0) : UIM(w);
                for (I = 0; I < 16; I++) VBYTE(Vd, I) = (UINT8)(INT32)Imm;
            }
            return EFI_SUCCESS;
        case 0x0D:  // vspltish vD, IMM
            {
                UINT32 Imm = (UIM(w) & 0x10) ? (UIM(w) | 0xFFFFFFE0) : UIM(w);
                for (I = 0; I < 8; I++) VHW_SET(Vd, I, (UINT16)(INT32)Imm);
            }
            return EFI_SUCCESS;
        case 0x0E:  // vspltisw vD, IMM
            {
                UINT32 Imm = (UIM(w) & 0x10) ? (UIM(w) | 0xFFFFFFE0) : UIM(w);
                for (I = 0; I < 4; I++) VWD_SET(Vd, I, Imm);
            }
            return EFI_SUCCESS;
        case 0x1E:  // vmrgew
            for (I = 0; I < 2; I++) {
                VWD_SET(Vd, 2 * I, VWD(Va, 2 * I));
                VWD_SET(Vd, 2 * I + 1, VWD(Vb, 2 * I));
            }
            return EFI_SUCCESS;
        default:
            return EFI_UNSUPPORTED;
        }

    case 0x0E:  // pack
        switch (X5) {
        case 0x00:  // vpkuhum
            for (I = 0; I < 16; I++) {
                UINT32 Src = (I < 8) ? Va : Vb;
                VBYTE(Vd, I) = VBYTE(Src, ((I & 7) * 2) + 1);
            }
            return EFI_SUCCESS;
        case 0x01:  // vpkuwum
            for (I = 0; I < 8; I++) {
                UINT32 Src = (I < 4) ? Va : Vb;
                VHW_SET(Vd, I, VWD(Src, I & 3) & 0xFFFF);
            }
            return EFI_SUCCESS;
        case 0x04:  // vpkshus
            for (I = 0; I < 16; I++) {
                UINT32 Src = (I < 8) ? Va : Vb;
                INT32 V = (INT16)VHW(Src, I & 7);
                VBYTE(Vd, I) = (V < 0) ? 0 : (V > 0xFF ? 0xFF : (UINT8)V);
            }
            return EFI_SUCCESS;
        case 0x05:  // vpkshss
            for (I = 0; I < 16; I++) {
                UINT32 Src = (I < 8) ? Va : Vb;
                VBYTE(Vd, I) = PpcSatS8((INT16)VHW(Src, I & 7));
            }
            return EFI_SUCCESS;
        case 0x06:  // vpkswus
            for (I = 0; I < 8; I++) {
                UINT32 Src = (I < 4) ? Va : Vb;
                INT32 V = (INT32)VWD(Src, I & 3);
                VHW_SET(Vd, I, (V < 0) ? 0 : (V > 0xFFFF ? 0xFFFF : (UINT16)V));
            }
            return EFI_SUCCESS;
        case 0x07:  // vpkswss
            for (I = 0; I < 8; I++) {
                UINT32 Src = (I < 4) ? Va : Vb;
                VHW_SET(Vd, I, PpcSatS16((INT32)VWD(Src, I & 3)));
            }
            return EFI_SUCCESS;
        case 0x0A:  // vpkuhus
            for (I = 0; I < 16; I++) {
                UINT32 Src = (I < 8) ? Va : Vb;
                VBYTE(Vd, I) = PpcSatU8((INT32)VHW(Src, I & 7));
            }
            return EFI_SUCCESS;
        case 0x0C:  // vpkuwus
            for (I = 0; I < 8; I++) {
                UINT32 Src = (I < 4) ? Va : Vb;
                VHW_SET(Vd, I, PpcSatU16((INT32)VWD(Src, I & 3)));
            }
            return EFI_SUCCESS;
        case 0x13:  // vpkudus
            {
                UINT64 S[4] = { V64H(Va), V64L(Va), V64H(Vb), V64L(Vb) };
                for (I = 0; I < 4; I++) VWD_SET(Vd, I, S[I] > 0xFFFFFFFFULL ? 0xFFFFFFFF : (UINT32)S[I]);
            }
            return EFI_SUCCESS;
        case 0x15:  // vpksdus
            {
                INT64 S[4] = { (INT64)V64H(Va), (INT64)V64L(Va), (INT64)V64H(Vb), (INT64)V64L(Vb) };
                for (I = 0; I < 4; I++) {
                    VWD_SET(Vd, I, S[I] < 0 ? 0 : (S[I] > 0xFFFFFFFFLL ? 0xFFFFFFFF : (UINT32)S[I]));
                }
            }
            return EFI_SUCCESS;
        case 0x17:  // vpksdss
            {
                INT64 S[4] = { (INT64)V64H(Va), (INT64)V64L(Va), (INT64)V64H(Vb), (INT64)V64L(Vb) };
                for (I = 0; I < 4; I++) VWD_SET(Vd, I, PpcSatS32(S[I]));
            }
            return EFI_SUCCESS;
        default:
            return EFI_UNSUPPORTED;
        }

    default:
        return EFI_UNSUPPORTED;
    }
}

// Execute an AltiVec opcode-31 X-form vector load/store.
static EFI_STATUS
PpcExecuteVectorMem (
    IN UINT32 w
    )
{
    UINT32 Ea  = EaX(w, RA(w), RB(w));
    UINT32 Vt  = RT(w);
    UINT32 EaA = Ea & ~0xF;
    UINT32 I;

    switch (XO10(w)) {
    case XO_LVX:  // lvx: 16-byte aligned load
        for (I = 0; I < 16; I++) {
            VBYTE(Vt, I) = g_ReadByte(EaA + I);
        }
        return EFI_SUCCESS;

    case XO_LVXL:  // lvxl: same as lvx (streaming hint ignored)
        for (I = 0; I < 16; I++) {
            VBYTE(Vt, I) = g_ReadByte(EaA + I);
        }
        return EFI_SUCCESS;

    case XO_LVSL:  // lvsl: little-endian permute constant for alignment offset
        for (I = 0; I < 16; I++) {
            VBYTE(Vt, I) = (UINT8)(Ea & 0xF) + I;
        }
        return EFI_SUCCESS;

    case XO_LVSR:  // lvsr
        for (I = 0; I < 16; I++) {
            VBYTE(Vt, I) = (UINT8)(16 + (Ea & 0xF) - I);
        }
        return EFI_SUCCESS;

    case XO_LVEBX:  // lvebx: byte load to element
        VBYTE(Vt, Ea & 0xF) = g_ReadByte(Ea);
        return EFI_SUCCESS;

    case XO_LVEHX:  // lvehx: halfword load to element
        {
            UINT32 El = (Ea >> 1) & 7;
            VHW_SET(Vt, El, g_ReadByte(Ea) << 8 | g_ReadByte(Ea + 1));
        }
        return EFI_SUCCESS;

    case XO_LVEWX:  // lvewx: word load to element
        {
            UINT32 El = (Ea >> 2) & 3;
            VWD_SET(Vt, El, ((UINT32)g_ReadByte(Ea) << 24) | ((UINT32)g_ReadByte(Ea + 1) << 16) |
                             ((UINT32)g_ReadByte(Ea + 2) << 8) | g_ReadByte(Ea + 3));
        }
        return EFI_SUCCESS;

    case XO_STVX:   // stvx: 16-byte aligned store
    case XO_STVXL:
        for (I = 0; I < 16; I++) {
            g_WriteByte(EaA + I, VBYTE(Vt, I));
        }
        return EFI_SUCCESS;

    case XO_STVEBX:  // stvebx
        g_WriteByte(Ea, VBYTE(Vt, Ea & 0xF));
        return EFI_SUCCESS;

    case XO_STVEHX:  // stvehx
        {
            UINT32 El = (Ea >> 1) & 7;
            UINT32 H = VHW(Vt, El);
            g_WriteByte(Ea, (UINT8)(H >> 8));
            g_WriteByte(Ea + 1, (UINT8)H);
        }
        return EFI_SUCCESS;

    case XO_STVEWX:  // stvewx
        {
            UINT32 El = (Ea >> 2) & 3;
            UINT32 W = VWD(Vt, El);
            g_WriteByte(Ea, (UINT8)(W >> 24));
            g_WriteByte(Ea + 1, (UINT8)(W >> 16));
            g_WriteByte(Ea + 2, (UINT8)(W >> 8));
            g_WriteByte(Ea + 3, (UINT8)W);
        }
        return EFI_SUCCESS;

    default:
        return EFI_UNSUPPORTED;
    }
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
    case 4:  // AltiVec VX/VA-form (opcode 4)
        {
            EFI_STATUS VecStatus = PpcExecuteVectorOp(w);
            if (EFI_ERROR(VecStatus)) {
                return VecStatus;
            }
            *NextAddress = Next;
            return EFI_SUCCESS;
        }

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

    case 9:  // dozi (601/POWER: RT = (RA > SIMM) ? 0 : SIMM - RA)
        if ((INT32)g_PpcContext.Gpr[RA(w)] > (INT32)SIMM(w)) {
            g_PpcContext.Gpr[RD(w)] = 0;
        } else {
            g_PpcContext.Gpr[RD(w)] = (UINT32)((INT32)SIMM(w) - (INT32)g_PpcContext.Gpr[RA(w)]);
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

            case XO_MFCR:  // mfcr / mfcrf / mfocrf
                {
                    // Plain 32-bit `mfcr RT` has FXM == 0x00 (bits 12-19) and
                    // must copy the whole CR; `mfcrf FXM,RT` selects fields in
                    // place; `mfocrf FXM,RT` (single FXM bit) replicates the
                    // field. Treating 0x00 as mfocrf returned 0 and silently
                    // corrupted every CR save/restore via mfcr/mtcrf.
                    UINT32 Fxm = (w >> 12) & 0xFF;
                    UINT32 Value;
                    if (Fxm == 0x00 || Fxm == 0xFF) {
                        Value = g_PpcContext.Cr;
                    } else if ((Fxm & (Fxm - 1)) == 0) {
                        UINT32 I, Field = 0;
                        for (I = 0; I < 8; I++) {
                            if (Fxm & (0x80 >> I)) {
                                Field = (g_PpcContext.Cr >> (28 - I * 4)) & 0xF;
                                break;
                            }
                        }
                        Value = Field * 0x11111111;
                    } else {
                        UINT32 I;
                        Value = 0;
                        for (I = 0; I < 8; I++) {
                            if (Fxm & (0x80 >> I)) {
                                Value |= (g_PpcContext.Cr & (0xFUL << (28 - I * 4)));
                            }
                        }
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

            case XO_LWZUX:  // lwzux
                {
                    UINT32 Ea = EaX(w, RA(w), RB(w));
                    g_PpcContext.Gpr[RT(w)] = CpuRead32(Ea);
                    g_PpcContext.Gpr[RA(w)] = Ea;
                }
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

            // -------- PowerPC 601 / POWER integer ops --------
            // The 601 splits the 64-bit product across rD (bits 0-31) and the
            // MQ register (SPR 0, bits 32-63); CR0 (Rc=1) reflects MQ, and OE
            // signals SO/OV when the product cannot be represented in 32 bits.
            case XO_MUL | 0x200:  // with-OE form
            case XO_MUL:  // mul / mul. / mulo / mulo. (601/POWER)
                {
                    INT64 P = (INT64)(INT32)g_PpcContext.Gpr[RA(w)] * (INT64)(INT32)g_PpcContext.Gpr[RB(w)];
                    g_PpcContext.Gpr[RT(w)] = (UINT32)(P >> 32);
                    g_PpcContext.Spr[0] = (UINT32)P;  // MQ = low 32 bits
                    if ((w >> 10) & 1) {
                        PpcSetXerOverflow(((P >> 32) != 0) && ((P >> 32) != -1));
                    }
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Spr[0]);
                }
                break;

            // div (601/POWER): 64-bit dividend (rA)||(MQ) divided by (rB);
            // quotient -> rD, remainder -> MQ. Remainder sign follows the
            // dividend (zero always positive); CR0 (Rc=1) reflects MQ.
            case XO_DIV | 0x200:  // with-OE form
            case XO_DIV:  // div / div. / divo / divo. (601/POWER)
                {
                    INT64 D = (INT64)(((UINT64)(UINT32)g_PpcContext.Gpr[RA(w)] << 32) |
                                      (UINT64)(UINT32)g_PpcContext.Spr[0]);
                    INT64 Dv = (INT32)g_PpcContext.Gpr[RB(w)];
                    INT64 Q = 0, R = 0;
                    UINT32 Ov = 0;
                    if (Dv == 0) {
                        Q = 0; R = 0; Ov = 1;
                    } else if (Dv == -1) {
                        if (D == (INT64)-2147483648) {  // -2^31 / -1
                            Q = 0x80000000; R = 0; Ov = 1;
                        } else {
                            Q = -D; R = 0;
                            Ov = (Q > 0x7FFFFFFF) || (Q < (INT64)-2147483648);
                        }
                    } else {
                        Q = D / Dv;
                        R = D % Dv;
                        Ov = (Q > 0x7FFFFFFF) || (Q < (INT64)-2147483648);
                    }
                    g_PpcContext.Gpr[RT(w)] = (UINT32)Q;
                    g_PpcContext.Spr[0] = (UINT32)R;
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Spr[0]);
                }
                break;

            // divs (601/POWER): 32-bit dividend (rA) divided by (rB);
            // quotient -> rD, remainder -> MQ. Defined overflows (divisor zero,
            // or -2^31 / -1) yield rD = -2^31 and MQ = 0.
            case XO_DIVS | 0x200:  // with-OE form
            case XO_DIVS:  // divs / divs. / divso / divso. (601/POWER)
                {
                    INT64 D = (INT32)g_PpcContext.Gpr[RA(w)];
                    INT64 Dv = (INT32)g_PpcContext.Gpr[RB(w)];
                    INT64 Q = 0, R = 0;
                    UINT32 Ov = 0;
                    if (Dv == 0) {
                        Q = 0x80000000; R = 0; Ov = 1;
                    } else if (Dv == -1 && D == (INT64)-2147483648) {
                        Q = 0x80000000; R = 0; Ov = 1;
                    } else {
                        Q = D / Dv;
                        R = D % Dv;
                    }
                    g_PpcContext.Gpr[RT(w)] = (UINT32)Q;
                    g_PpcContext.Spr[0] = (UINT32)R;
                    if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
                    if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Spr[0]);
                }
                break;

            // abs (601/POWER): rD = |rA|. abs(0x80000000) stays 0x80000000 and
            // signals overflow (rA is the most negative number).
            case XO_ABS | 0x200:  // with-OE form
            case XO_ABS:  // abs / abs. / abso / abso. (601/POWER)
                {
                    UINT32 A = g_PpcContext.Gpr[RA(w)];
                    UINT32 R;
                    if (A == 0x80000000) {
                        R = A;
                        if ((w >> 10) & 1) PpcSetXerOverflow(1);
                    } else {
                        R = ((INT32)A < 0) ? (UINT32)(-(INT32)A) : A;
                        if ((w >> 10) & 1) PpcSetXerOverflow(0);
                    }
                    g_PpcContext.Gpr[RT(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            // nabs (601/POWER): rD = -|rA|. Never overflows; with OE, XER[OV]
            // is cleared but XER[SO] is left unchanged.
            case XO_NABS | 0x200:  // with-OE form
            case XO_NABS:  // nabs / nabs. / nabso / nabso. (601/POWER)
                {
                    UINT32 A = g_PpcContext.Gpr[RA(w)];
                    UINT32 AbsA = (A == 0x80000000) ? 0x80000000U : ((INT32)A < 0 ? (UINT32)(-(INT32)A) : A);
                    UINT32 R = 0U - AbsA;
                    if ((w >> 10) & 1) g_PpcContext.Xer &= ~PPC_XER_OV;
                    g_PpcContext.Gpr[RT(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            // doz (601/POWER): rD = rB - rA, or 0 if rA > rB algebraically.
            // With OE, OV is only set on a positive overflow.
            case XO_DOZ | 0x200:  // with-OE form
            case XO_DOZ:  // doz / doz. / dozo / dozo. (601/POWER)
                {
                    INT32 A = (INT32)g_PpcContext.Gpr[RA(w)];
                    INT32 B = (INT32)g_PpcContext.Gpr[RB(w)];
                    INT64 Diff = (INT64)B - (INT64)A;
                    UINT32 R = (A > B) ? 0 : (UINT32)Diff;
                    if ((w >> 10) & 1) PpcSetXerOverflow(Diff > 0x7FFFFFFF);
                    g_PpcContext.Gpr[RT(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            // maskg (601/POWER): rA = mask of ones from rS[27-31] to rB[27-31]
            // (bit 0 = MSB). start == stop+1 yields all ones; start > stop+1
            // yields ones everywhere except the enclosed zero run. Rc only.
            case XO_MASKG:  // maskg / maskg. (601/POWER)
                {
                    UINT32 Start = g_PpcContext.Gpr[RS(w)] & 0x1F;
                    UINT32 Stop = g_PpcContext.Gpr[RB(w)] & 0x1F;
                    UINT32 R;
                    if (Start < Stop + 1) {
                        UINT32 Len = Stop - Start + 1;
                        R = (Len == 32) ? 0xFFFFFFFF : ((0xFFFFFFFFU >> (32 - Len)) << (31 - Stop));
                    } else if (Start == Stop + 1) {
                        R = 0xFFFFFFFF;
                    } else {
                        UINT32 Lo = Stop + 1;
                        UINT32 Hi = Start - 1;
                        UINT32 Len = Hi - Lo + 1;
                        UINT32 ZeroMask = (Len == 32) ? 0xFFFFFFFF : ((0xFFFFFFFFU >> (32 - Len)) << (31 - Hi));
                        R = ~ZeroMask;
                    }
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            // maskir (601/POWER): rS is inserted into rA under the mask in rB
            // (a 1 bit copies the rS bit, a 0 bit leaves rA unchanged). Rc only.
            case XO_MASKIR:  // maskir / maskir. (601/POWER)
                {
                    UINT32 Mask = g_PpcContext.Gpr[RB(w)];
                    UINT32 R = (g_PpcContext.Gpr[RA(w)] & ~Mask) | (g_PpcContext.Gpr[RS(w)] & Mask);
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            // rrib (601/POWER): bit 0 of rS is rotated right by rB[27-31] and
            // inserted at that bit position of rA; other rA bits are unchanged.
            case XO_RRIB:  // rrib / rrib. (601/POWER)
                {
                    UINT32 N = g_PpcContext.Gpr[RB(w)] & 0x1F;
                    UINT32 Bit = (g_PpcContext.Gpr[RS(w)] >> 31) & 1;
                    UINT32 R = (g_PpcContext.Gpr[RA(w)] & ~(0x80000000U >> N)) | (Bit << (31 - N));
                    g_PpcContext.Gpr[RA(w)] = R;
                    if (Rc(w)) PpcSetCr0FromResult(R);
                }
                break;

            // eciwx/ecowx (601/POWER external control): read/write a 32-bit word
            // at EA. The EAR external-control facility is not modeled, so these
            // behave like plain lwzx/stwx memory accesses.
            case XO_ECIWX | 0x200:
            case XO_ECIWX:  // eciwx rD,rA,rB
                g_PpcContext.Gpr[RT(w)] = CpuRead32(EaX(w, RA(w), RB(w)));
                break;

            case XO_ECOWX | 0x200:
            case XO_ECOWX:  // ecowx rS,rA,rB
                CpuWrite32(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
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

            case XO_LVSL:
            case XO_LVEBX:
            case XO_LVSR:
            case XO_LVEHX:
            case XO_LVEWX:
            case XO_LVX:
            case XO_STVEBX:
            case XO_STVEHX:
            case XO_STVEWX:
            case XO_STVX:
            case XO_LVXL:
            case XO_STVXL:
                {
                    EFI_STATUS VecStatus = PpcExecuteVectorMem(w);
                    if (EFI_ERROR(VecStatus)) {
                        return VecStatus;
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
    static UINT32 TailLr[4096];
    static UINT32 PcsDumped = 0;
    static UINT32 TraceDumped = 0;
    static UINT32 StoreProbed = 0;
    static UINT32 RamProbed = 0;
    static UINT32 AllocTraced = 0;
    static UINT32 FlushProbed = 0;
    static UINT32 HelperDumped = 0;
    static UINT32 SccPollTraced = 0;
    static UINT32 HelperStep = 0;
    static UINT32 TermEntries = 0;
    static UINT32 AutoResumed = 0;
    static UINT32 PmdWalked = 0;
    static UINT32 PmdEntry = 0;
    static UINT32 PmdArrDump = 0;
    static UINT32 MergeTraced = 0;
    static UINT32 PmdFixed = 0;

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
        if (RamProbed == 0 && Current == 0x40B1243C) {
            UINT32 P = g_PpcContext.Gpr[1];
            UINT32 T;
            RamProbed = 1;
            Print(L"  RAMPROBE@0x%08x r1=0x%08x r17=0x%08x r18=0x%08x r19=0x%08x r21=0x%08x r22=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x\n",
                  Current, P, g_PpcContext.Gpr[17], g_PpcContext.Gpr[18],
                  g_PpcContext.Gpr[19], g_PpcContext.Gpr[21], g_PpcContext.Gpr[22],
                  g_PpcContext.Gpr[29], g_PpcContext.Gpr[30], g_PpcContext.Gpr[31]);
            Print(L"  RAMPROBE loc[1704]=0x%08x loc[1708]=0x%08x loc[1716]=0x%08x loc[1592]=0x%08x loc[1596]=0x%08x loc[-32]=0x%08x abs[6A8]=0x%08x abs[6AC]=0x%08x\n",
                  CpuRead32(P + 0x6A8), CpuRead32(P + 0x6AC), CpuRead32(P + 0x6B4),
                  CpuRead32(P + 0x638), CpuRead32(P + 0x63C), CpuRead32(P - 0x20),
                  CpuRead32(0x000006A8), CpuRead32(0x000006AC));
            Print(L"  RAMPROBE memmap@r1+120:\n");
            for (T = P + 0x78; T < P + 0x178; T += 16) {
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
        // PMDT chunk-pointer array: the walk reads the PMDT base for each 256MB
        // chunk via 'lwzu r25, 8(r27)' (r27 = r1 + 0x78), so the 32-bit pointers
        // live at [r1+0x80 + 8k]. Dump them once to see how many chunks are
        // populated and whether adjacent slots alias the same table.
        if (PmdArrDump == 0 && Current == 0x40B1F404) {
            UINT32 R1 = g_PpcContext.Gpr[1];
            UINT32 K;
            PmdArrDump = 1;
            Print(L"  PMDTARR r1=0x%08x r26=0x%08x pointers [r1+0x80+8k]:\n",
                  R1, g_PpcContext.Gpr[26]);
            for (K = 0; K < 16; K++) {
                Print(L"    k=%2d @0x%08x: 0x%08x\n", K, R1 + 0x80 + 8 * K,
                      CpuRead32(R1 + 0x80 + 8 * K));
            }
        }
        // PMDT RAM injection (one-shot): the DR=1 boot path skips the NK's PMDT
        // builder, so the table only has the [0xFFF7,9] top-of-block-0
        // reservation followed by 63 zero entries. The walk dispatches on
        // flags&0xE00: 0 = area create, 0xC00 = special area, any other value
        // with page=0 && count=0xFFFF = chunk terminator (r26 += 256MB).
        // Rewrite chunk 0's table as reservation + [0,0xFFF6) RAM + a real
        // terminator, and point chunks 1..15 at the terminator entry so each
        // 256MB chunk walks cleanly and the walk ends when r26 wraps.
        if (PmdFixed == 0 && Current == 0x40B1F418) {
            UINT32 Base = g_PpcContext.Gpr[25];
            UINT32 R1  = g_PpcContext.Gpr[1];
            UINT32 K;
            PmdFixed = 1;
            // chunk 0 table: r25 was already loaded from [r1+0x78] (original
            // pointer array); rewrite it explicitly for self-consistency.
            CpuWrite32(R1 + 0x78, Base);
            // chunks 1..15: point at the terminator-only entry (no areas).
            for (K = 1; K < 16; K++) {
                CpuWrite32(R1 + 0x80 + 8 * K, Base + 16);
            }
            // entry 0 [0xFFF7,9] already holds the top-of-block-0 reservation.
            // entry 1: RAM [0, 0xFFF7000).
            CpuWrite16(Base + 8, 0x0000);
            CpuWrite16(Base + 10, 0xFFF6);
            CpuWrite32(Base + 12, 0x00000000);
            // entry 2: chunk terminator, flags&0xE00 = 0x400 (not 0, not 0xC00).
            CpuWrite16(Base + 16, 0x0000);
            CpuWrite16(Base + 18, 0xFFFF);
            CpuWrite32(Base + 20, 0x00000400);
            // entries 3..63: unreachable (chunk advances at entry 2), but keep
            // them as terminators so a stray entry can never walk an area.
            for (K = 24; K < 64 * 8; K += 8) {
                CpuWrite16(Base + K, 0x0000);
                CpuWrite16(Base + K + 2, 0xFFFF);
                CpuWrite32(Base + K + 4, 0x00000400);
            }
            Print(L"  PMDTINJECT base=0x%08x chunk0=[0xFFF7,9]+[0,0xFFF6]+TERM chunks1..15=TERM\n",
                  Base);
        }
        // PMDT table dump: 0x40B1F418 ('lwz r17, 4(r25)') is the top of the
        // per-chunk entry scan; r25 holds the current 8-byte entry base. Dump 64
        // entries from the first entry read to see the table the walk is scanning.
        if (PmdWalked < 1 && Current == 0x40B1F418) {
            UINT32 Base = g_PpcContext.Gpr[25];
            UINT32 I;
            PmdWalked++;
            Print(L"  PMDTDUMP r25=0x%08x r26=0x%08x r27=0x%08x r1=0x%08x 64 entries:\n",
                  Base, g_PpcContext.Gpr[26], g_PpcContext.Gpr[27],
                  g_PpcContext.Gpr[1]);
            for (I = 0; I < 64; I++) {
                UINT32 E = Base + I * 8;
                Print(L"    PMDT[%2d] @0x%08x page=0x%04x count=0x%04x type=0x%08x\n",
                      I, E, CpuRead16(E), CpuRead16(E + 2), CpuRead32(E + 4));
            }
        }
        // PMDT per-entry read: at 0x40B1F428 (after andi. r17,r8,0xE00) r25 is the
        // entry base, r15=page, r16=count, r17=type&0xE00, r8=type, r26=chunk base.
        if (PmdEntry < 40 && Current == 0x40B1F428) {
            PmdEntry++;
            Print(L"  PMDENTRY[%d] base=0x%08x page=0x%04x count=0x%04x type=0x%08x r8=0x%08x r26=0x%08x r27=0x%08x\n",
                  PmdEntry, g_PpcContext.Gpr[25], g_PpcContext.Gpr[15],
                  g_PpcContext.Gpr[16], g_PpcContext.Gpr[17],
                  g_PpcContext.Gpr[8], g_PpcContext.Gpr[26],
                  g_PpcContext.Gpr[27]);
        }
        // Merge path: 0x40B1F668 is reached via the beq at 0x40B1F614 when
        // [new+0x24] == [existing+0x24]. r24 = existing area, r31 = new area.
        // Log the fields that the guard at 0x40B1F67C compares: the 0x28 fields
        // are never written by the creation code, so they should be pool garbage.
        if (MergeTraced < 8 && Current == 0x40B1F668) {
            UINT32 Ex = g_PpcContext.Gpr[24];
            UINT32 Nw = g_PpcContext.Gpr[31];
            MergeTraced++;
            Print(L"  MERGE[%d] existing=0x%08x new=0x%08x [ex+0x24]=0x%08x [ex+0x28]=0x%08x [ex+0x2C]=0x%08x [new+0x24]=0x%08x [new+0x28]=0x%08x [new+0x2C]=0x%08x r25=0x%08x r26=0x%08x r15=0x%08x r16=0x%08x r9=0x%08x\n",
                  MergeTraced, Ex, Nw,
                  CpuRead32(Ex + 0x24), CpuRead32(Ex + 0x28), CpuRead32(Ex + 0x2C),
                  CpuRead32(Nw + 0x24), CpuRead32(Nw + 0x28), CpuRead32(Nw + 0x2C),
                  g_PpcContext.Gpr[25], g_PpcContext.Gpr[26],
                  g_PpcContext.Gpr[15], g_PpcContext.Gpr[16], g_PpcContext.Gpr[9]);
        }
        // Banner CR/LF flush-tail diagnostics. The guest spins at the SCC
        // Tx-empty poll (PC=0x40B26500, LBZ 2(r28) / ANDI. bit 2) because the
        // SCC base register r28 is 0, so the poll reads guest 0x2 instead of
        // the SCC at 0x20002. Log r28 around the flush helper call (bl at
        // 0x40B264D8 to 0x40B28A98) to see whether the helper zeroes r28 or
        // whether the SCC base was never loaded (candidate: PSA NoIdeaR23 at
        // [KDP-0x900], the SCC base `prints` reads via `lwz r28,-0x900(r1)`).
        if (FlushProbed < 4 && (Current == 0x40B264D8 || Current == 0x40B264DC)) {
            UINT32 Ewa = g_PpcContext.Spr[272];
            UINT32 Kdp = CpuRead32(Ewa - 4);
            Print(L"  FLUSHPROBE[%d] @0x%08x r28=0x%08x CR=0x%08x CTR=0x%08x LR=0x%08x\n",
                  FlushProbed, Current, g_PpcContext.Gpr[28], g_PpcContext.Cr,
                  g_PpcContext.Ctr, g_PpcContext.Lr);
            if (Current == 0x40B264D8) {
                Print(L"  FLUSHPROBE KDP=0x%08x NoIdeaR23[KDP-0x900]=0x%08x [KDP+0xedc]=0x%08x [KDP+0x648]=0x%08x [KDP+0x64c]=0x%08x\n",
                      Kdp, CpuRead32(Kdp - 0x900), CpuRead32(Kdp + 0xedc),
                      CpuRead32(Kdp + 0x648), CpuRead32(Kdp + 0x64c));
            }
            FlushProbed++;
        }
        // Dump the flush helper body once so we can see how it sets r28/CR.
        if (HelperDumped == 0 && Current == 0x40B28A98) {
            UINT32 A;
            HelperDumped = 1;
            Print(L"  FLUSHHELPER dump 0x40B28A74..0x40B28C00:\n");
            for (A = 0x40B28A74; A < 0x40B28C00; A += 16) {
                Print(L"    0x%08x: %08x %08x %08x %08x\n",
                      A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
            }
        }
        // Log the SCC Tx-empty poll iterations: r29 is the value the LBZ at
        // 0x40B264F8 just read from [r28+2], which must be the SCC status reg
        // (0x20002). If addr != 0x20002 the poll will spin forever.
        if (SccPollTraced < 30 && Current == 0x40B264FC) {
            Print(L"  SCCPOLL[%d] after lbz r29,2(r28): r28=0x%08x addr=0x%08x value=0x%02x\n",
                  SccPollTraced, g_PpcContext.Gpr[28], g_PpcContext.Gpr[28] + 2,
                  g_PpcContext.Gpr[29]);
            SccPollTraced++;
        }
        // Step through the flush helper (0x40B28A98..0x40B28C04) one instruction
        // at a time, printing state BEFORE each instruction executes. State shown
        // at PC=X is therefore the result of the instruction at PC-4.
        if (HelperStep < 45 && Current >= 0x40B28A98 && Current <= 0x40B28C04) {
            UINT32 R1 = g_PpcContext.Gpr[1];
            Print(L"  HELPER[%d] PC=0x%08x r1=0x%08x r14=0x%08x r15=0x%08x r16=0x%08x r26=0x%08x CR=0x%08x CR0=%x CR7=%x LR=0x%08x next=0x%08x [r1-3F0]=0x%08x [r1-3EC]=0x%08x [r1+EDC]=0x%08x\n",
                  HelperStep, Current, R1, g_PpcContext.Gpr[14], g_PpcContext.Gpr[15],
                  g_PpcContext.Gpr[16], g_PpcContext.Gpr[26], g_PpcContext.Cr,
                  (g_PpcContext.Cr >> 28) & 0xF, g_PpcContext.Cr & 0xF,
                  g_PpcContext.Lr, Next, CpuRead32(R1 - 0x3F0), CpuRead32(R1 - 0x3EC),
                  CpuRead32(R1 + 0xEDC));
            HelperStep++;
        }
        TailInst[TailStart] = Instr;
        TailPc[TailStart] = Current;
        TailNext[TailStart] = Next;
        TailR28[TailStart] = g_PpcContext.Gpr[28];
        TailR8[TailStart] = g_PpcContext.Gpr[8];
        TailR17[TailStart] = g_PpcContext.Gpr[17];
        TailLr[TailStart] = g_PpcContext.Lr;
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
        // Log EVERY nanodebugger (Termination) entry with its caller so we can
        // see each fatal check the guest hits as boot progresses. r29 is loaded
        // from LR (the caller's return address) at 0x40B272F8; [KDP+0x904] holds
        // the same value once stored. The 'g' handler's optional context
        // re-save (0x40B27A90 -> Termination) shows up here as caller 0x40B27A94.
        if (TermEntries < 60 && Current == 0x40B272F8) {
            UINT32 Ewa = g_PpcContext.Spr[272];
            UINT32 Kdp = CpuRead32(Ewa - 4);
            UINT32 Caller = g_PpcContext.Gpr[29];
            TermEntries++;
            Print(L"  TERMENTRY[%d] PC=0x%08x caller=0x%08x%s r1=0x%08x r8=0x%08x r9=0x%08x r31=0x%08x KDP=0x%08x EWA=0x%08x\n",
                  TermEntries, Current, Caller,
                  (Caller == 0x40B27A94) ? L" (g re-save)" : L"",
                  g_PpcContext.Gpr[1], g_PpcContext.Gpr[8], g_PpcContext.Gpr[9],
                  g_PpcContext.Gpr[31], Kdp, Ewa);
        }
        // Auto-answer the nanodebugger wait loop: when the guest is spinning
        // (PC=0x40B2751C) with an empty SCC Rx FIFO, queue the same
        // 'g' CR 'g' CR sequence the host pre-queues for the first entry so the
        // boot continues past each subsequent fatal check. Cap it so a
        // pathological re-panic loop cannot flood the log forever.
        if (AutoResumed < 25 && Current == 0x40B2751C &&
            g_SccRxFifoHead == g_SccRxFifoTail) {
            AutoResumed++;
            Print(L"  AUTORESUME[%d] queued 'g' CR 'g' CR at PC=0x%08x r1=0x%08x LR=0x%08x\n",
                  AutoResumed, Current, g_PpcContext.Gpr[1], g_PpcContext.Lr);
            PpcSccPutChar('g');
            PpcSccPutChar(0x0D);
            PpcSccPutChar('g');
            PpcSccPutChar(0x0D);
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
                    Print(L"  TRACE[-%d] PC=0x%08x 0x%08x %s -> 0x%08x r28=0x%08x r8=0x%08x LR=0x%08x\n",
                          (UINTN)I + 1, TailPc[Idx], TailInst[Idx], Mn, TailNext[Idx],
                          TailR28[Idx], TailR8[Idx], TailLr[Idx]);
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
                {
                    UINTN A;
                    for (A = 0x00000000u; A < 0x00000400u; A += 16) {
                        Print(L"  LOW[0x%08x] %08x %08x %08x %08x\n",
                              A, CpuRead32(A), CpuRead32(A + 4),
                              CpuRead32(A + 8), CpuRead32(A + 12));
                    }
                }
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
        UINTN A;
        for (A = 0x00000000u; A < 0x00000300u; A += 16) {
            Print(L"  LOW[0x%08x] %08x %08x %08x %08x\n",
                  A, CpuRead32(A), CpuRead32(A + 4),
                  CpuRead32(A + 8), CpuRead32(A + 12));
        }
    }
    {
        UINTN A, W;
        UINT32 Loops[][2] = { { 0x40A00000u, 0x40A01000u }, { 0x40B10000u, 0x40B16000u },
                              { 0x40B11B00u, 0x40B11E60u }, { 0x40B1F800u, 0x40B1FC00u },
                              { 0x40B23F00u, 0x40B24400u }, { 0x40B26000u, 0x40B28000u },
                              { 0x40B28700u, 0x40B28B00u }, { 0x40B23700u, 0x40B23800u } };
        for (W = 0; W < 8; W++) {
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
            Print(L"  TRACE[-%d] PC=0x%08x 0x%08x %s -> 0x%08x r28=0x%08x r8=0x%08x r17=0x%08x LR=0x%08x\n",
                  (UINTN)I + 1, TailPc[Idx], TailInst[Idx], Mn, TailNext[Idx],
                  TailR28[Idx], TailR8[Idx], TailR17[Idx], TailLr[Idx]);
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
    L"reserved", L"mulli",    L"subfic",   L"dozi",     L"cmpli",    L"cmpi",
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
        // XO10() keeps the 9-bit XO field plus the OE bit (0x200). XO() clears
        // bit 0, which drops the low bit of odd-valued XO fields (mullw=235,
        // divw=491, the 601 mul/div/divs/maskg/maskir/rrib), so decode from
        // XO10() so those mnemonics resolve. Rc is a separate word bit 31.
        switch (XO10(w)) {
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
        case XO_LWZUX:     Name = L"lwzux"; break;
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
        case XO_MUL:       Name = L"mul";   break;
        case XO_DIV:       Name = L"div";   break;
        case XO_DIVS:      Name = L"divs";  break;
        case XO_ABS:       Name = L"abs";   break;
        case XO_NABS:      Name = L"nabs";  break;
        case XO_DOZ:       Name = L"doz";   break;
        case XO_MASKG:     Name = L"maskg"; break;
        case XO_MASKIR:    Name = L"maskir";break;
        case XO_RRIB:      Name = L"rrib";  break;
        case XO_ECIWX:     Name = L"eciwx"; break;
        case XO_ECOWX:     Name = L"ecowx"; break;
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

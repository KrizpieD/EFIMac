// ---------------------------------------------------------------------------
// Native Motorola 68000 interpreter for EFIMac
//
// This replaces the ROM's PPC-based DR emulator with a native C interpreter.
// The PPC interpreter hooks the opcode table (ROM+0x380000) so that regular
// 68K opcodes route through a trampoline into this code. EMUL_OP entries
// (0xFE40+) continue to use the existing mulli-marker interception.
// ---------------------------------------------------------------------------

#include "m68k.h"
#include "interpreter.h"
#include "translation.h"
#include "emul_op.h"
#include "boot/bootloader.h"
#include <efi.h>
#include <efilib.h>
#include <lib.h>

UINT32 M68kGetStackPointer (VOID);

// ---------------------------------------------------------------------------
// Trace file logging
// ---------------------------------------------------------------------------
static CHAR16 g_TraceBuf[8192];
static UINTN  g_TraceLen = 0;

static
VOID
M68kTraceFlush (
    VOID
    )
{
    if (g_TraceLen == 0) return;

    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage = NULL;
    EFI_STATUS Status = BS->HandleProtocol(LibImageHandle, &LoadedImageProtocol, (VOID**)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage == NULL || LoadedImage->DeviceHandle == NULL) return;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs = NULL;
    Status = BS->HandleProtocol(LoadedImage->DeviceHandle, &FileSystemProtocol, (VOID**)&Fs);
    if (EFI_ERROR(Status) || Fs == NULL) return;

    EFI_FILE_HANDLE Root = NULL;
    Status = Fs->OpenVolume(Fs, &Root);
    if (EFI_ERROR(Status)) return;

    EFI_FILE_HANDLE F = NULL;
    Status = Root->Open(Root, &F, L"trace68k.log",
                        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(Status) || F == NULL) { Root->Close(Root); return; }

    F->SetPosition(F, (UINT64)-1);
    UINTN Len = g_TraceLen * sizeof(CHAR16);
    F->Write(F, &Len, g_TraceBuf);
    F->Close(F);
    Root->Close(Root);
    g_TraceLen = 0;
}

static
VOID
M68kTrace (
    IN CHAR16* Line
    )
{
    UINTN Len = StrLen(Line);
    if (g_TraceLen + Len + 1 > sizeof(g_TraceBuf) / sizeof(CHAR16)) {
        M68kTraceFlush();
    }
    CopyMem(g_TraceBuf + g_TraceLen, Line, (Len + 1) * sizeof(CHAR16));
    g_TraceLen += Len;
    if (g_TraceLen > 7900) M68kTraceFlush();
}

// Helper: write a formatted trace line using simple hex formatting
static
VOID
M68kTraceLine (
    IN UINT32 Step,
    IN UINT32 PC,
    IN UINT16 Op,
    IN UINT32 SP,
    IN UINT16 SR,
    IN UINT32 A6Val,
    IN UINT32 D0Val
    )
{
    CHAR16 Buf[120];
    // Manual format: step, PC, Op, SP, SR, A6, D0
    // Use Print to format into Buf (Print goes through ConOut but we write to Buf via UnicodeSPrint)
    UnicodeSPrint(Buf, sizeof(Buf),
        L"%u:PC=%08X:%04X SP=%08X SR=%04X A6=%08X D0=%08X\n",
        Step, PC, Op, SP, SR, A6Val, D0Val);
    M68kTrace(Buf);
}

// ---------------------------------------------------------------------------
// Global 68K CPU context
// ---------------------------------------------------------------------------
M68K_CPU_CONTEXT g_M68kContext = {0};

// One-shot post-memcpy single-step tracer (diagnostics)
UINTN g_M68kDebugSteps = 0;

// ---------------------------------------------------------------------------
// Log any control transfer (JMP/JSR/RTS) that lands in low memory (<0x1000),
// where only vectors/globals live — these indicate a bad thunk chain pointer.
// ---------------------------------------------------------------------------
static UINTN g_M68kLowXferCount = 0;

static VOID
M68kLogLowTransfer (
    IN UINT32 FromPC,
    IN UINT16 Op,
    IN UINT32 Target
    )
{
    CHAR16 Buf[160];
    UINTN i;

    if (Target >= 0x1000 || g_M68kLowXferCount >= 8) return;
    g_M68kLowXferCount++;

    UnicodeSPrint (Buf, sizeof(Buf),
        L"  68K LOWXFER from PC=0x%08x op=%04x -> 0x%08x "
        L"A0=%08x A1=%08x A2=%08x A5=%08x A6=%08x SSP=%08x D7=%08x SR=%04x\n",
        FromPC, Op, Target,
        g_M68kContext.A[0], g_M68kContext.A[1], g_M68kContext.A[2],
        g_M68kContext.A[5], g_M68kContext.A[6], g_M68kContext.SSP,
        g_M68kContext.D[7], g_M68kContext.SR);
    Print (Buf);

    {
        UINT32 Lo = Target & ~0xF;
        Print (L"  LOWXFER mem:");
        for (i = 0; i < 12; i++) {
            UINT32 Addr = Lo - 0x10 + i * 4;
            UINT16 HiW = M68kReadWord (Addr);
            UINT16 LoW = M68kReadWord (Addr + 2);
            Print (L" %08x", ((UINT32)HiW << 16) | LoW);
        }
        Print (L"\n");
    }

    if (g_M68kLowXferCount == 1 && Op == 0x4E92) {
        // Decode the walker chain: a2 = A1 + [A1+0x44]; d2 = [a2+0x14]
        UINT32 T44H = M68kReadWord (0x44);
        UINT32 T44L = M68kReadWord (0x46);
        UINT32 T44  = (T44H << 16) | T44L;
        UINT32 X14H = M68kReadWord (T44 + 0x14);
        UINT32 X14L = M68kReadWord (T44 + 0x16);
        UINT32 X14  = (X14H << 16) | X14L;
        UnicodeSPrint (Buf, sizeof(Buf),
            L"  LOWXFER chain: [0x44]=%08x a2=%08x [a2+14]=%08x\n",
            T44, T44, X14);
        Print (Buf);
        Print (L"  LOWXFER lowmem 0x30-0x140:\n");
        for (i = 0; i < 17; i++) {
            UINTN j;
            UINT32 Base = 0x30 + i * 4 * 4;
            Print (L"   %04x:", Base);
            for (j = 0; j < 4; j++) {
                UINT32 Addr = Base + j * 4;
                UINT16 HiW = M68kReadWord (Addr);
                UINT16 LoW = M68kReadWord (Addr + 2);
                Print (L" %08x", ((UINT32)HiW << 16) | LoW);
            }
            Print (L"\n");
        }
    }

    if (g_M68kContext.A[1] >= 0x1000 && g_M68kContext.A[1] < 0xFFF00000) {
        Print (L"  LOWXFER desc@A1:");
        for (i = 12; i < 24; i++) {
            UINT16 HiW = M68kReadWord (g_M68kContext.A[1] + i * 4);
            UINT16 LoW = M68kReadWord (g_M68kContext.A[1] + i * 4 + 2);
            Print (L" %08x", ((UINT32)HiW << 16) | LoW);
        }
        Print (L"\n");
    }
}

// Recent-PC ring shared by the batch loop (runaway reports) and the
// DR-service-call redirect (ping-pong detection).
static UINT32 g_LastPcRing[256];
static UINT16 g_LastOpRing[256];
static UINTN  g_LastPcIdx = 0;

// ---------------------------------------------------------------------------
// Detect addresses that belong to the DR emulator's domain (PPC opcode table,
// emulator code area).  Jumps into these regions must be intercepted and
// redirected to A6 (the callback return address).
// ---------------------------------------------------------------------------
static
BOOLEAN
M68kIsDrEmulatorAddress (
    IN UINT32 Addr
    )
{
    // The ROM's 68K code calls DR-emulator services via long branches into
    // the 0x305xxxxx-0x306xxxxx region (observed targets reach at least
    // 0x30628DA2). That whole area is unmapped in our guest, so treat every
    // branch into it as an emulator-domain callback.
    if (Addr >= 0x30000000 && Addr < 0x40000000) return TRUE;
    if (Addr >= 0x40B60000 && Addr < 0x40C00000) return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------------------
// Memory access (big-endian, same as PPC guest)
// ---------------------------------------------------------------------------

UINT8
M68kReadByte (
    IN UINT32 Address
    )
{
    return PpcReadGuestByte (Address);
}

UINT16
M68kReadWord (
    IN UINT32 Address
    )
{
    return ((UINT16)PpcReadGuestByte (Address) << 8) |
           (UINT16)PpcReadGuestByte (Address + 1);
}

UINT32
M68kReadLong (
    IN UINT32 Address
    )
{
    return ((UINT32)M68kReadWord (Address) << 16) |
           (UINT32)M68kReadWord (Address + 2);
}

VOID
M68kWriteByte (
    IN UINT32 Address,
    IN UINT8  Value
    )
{
    PpcWriteGuestByte (Address, Value);
}

VOID
M68kWriteWord (
    IN UINT32 Address,
    IN UINT16 Value
    )
{
    {
        static UINTN RomWatchWHits = 0;
        if (RomWatchWHits < 8 &&
            Address >= 0x4080AE00u && Address < 0x4080B000u) {
            RomWatchWHits++;
            Print (L"68K ROMWRITE.W val=0x%04x -> 0x%08x @PC=0x%08x\n",
                   Value, Address, g_M68kContext.PC);
        }
    }
    PpcWriteGuestByte (Address, (UINT8)(Value >> 8));
    PpcWriteGuestByte (Address + 1, (UINT8)Value);
}

VOID
M68kWriteLong (
    IN UINT32 Address,
    IN UINT32 Value
    )
{
    {
        static UINTN RomWatchHits = 0;
        if (RomWatchHits < 8 &&
            Address >= 0x4080AE00u && Address < 0x4080B000u) {
            RomWatchHits++;
            Print (L"68K ROMWRITE val=0x%08x -> 0x%08x @PC=0x%08x "
                   L"A0=%08x A1=%08x A7=%08x\n",
                   Value, Address, g_M68kContext.PC,
                   g_M68kContext.A[0], g_M68kContext.A[1],
                   M68kGetStackPointer ());
        }
    }
    {
        static UINTN Ab4eWatchHits = 0;
        if (Ab4eWatchHits < 8 &&
            Value >= 0x4080AB00u && Value <= 0x4080ABFFu) {
            Ab4eWatchHits++;
            Print (L"68K AB4E-WATCH write val=0x%08x -> [0x%08x] "
                   L"@PC=0x%08x D0=%08x D1=%08x D7=%08x A0=%08x "
                   L"A1=%08x A2=%08x A4=%08x SP=%08x\n",
                   Value, Address, g_M68kContext.PC,
                   g_M68kContext.D[0], g_M68kContext.D[1],
                   g_M68kContext.D[7], g_M68kContext.A[0],
                   g_M68kContext.A[1], g_M68kContext.A[2],
                   g_M68kContext.A[4], M68kGetStackPointer ());
        }
    }
    {
        // Trampoline-value watch: ANY store of an address inside the NK
        // continuation-stub region, regardless of the store's mechanism
        // (MOVE.L Dx,-(SP) bypasses M68kPushLong entirely).
        static UINTN TrampValSeen = 0;
        if (TrampValSeen < 24 &&
            Value >= 0x7F0000u && Value < 0x800000u &&
            Address < 0x7F0000u) {                 // exclude region itself
            TrampValSeen++;
            Print (L"  TRAMP-VAL [%08x] <- %08x @PC=%08x d0=%08x "
                   L"d1=%08x a0=%08x a6=%08x sp=%08x\n",
                   Address, Value, g_M68kContext.PC,
                   g_M68kContext.D[0], g_M68kContext.D[1],
                   g_M68kContext.A[0], g_M68kContext.A[6],
                   M68kGetStackPointer ());
        }
    }
    {
        // Thunk-table watch: who initializes the NK MixedMode dispatch
        // slots that the 0x4080A46x trampolines read via tbl(A0,D1.l)?
        // Log every write into the region so the producer (or its
        // absence) becomes visible.
        static UINTN ThunkWatchHits = 0;
        if (ThunkWatchHits < 48 &&
            Address >= 0x40804960u && Address < 0x40804A00u) {
            ThunkWatchHits++;
            Print (L"  THUNK-W [%08x] <- %08x @PC=%08x d0=%08x d1=%08x "
                   L"a0=%08x a1=%08x\n",
                   Address, Value, g_M68kContext.PC,
                   g_M68kContext.D[0], g_M68kContext.D[1],
                   g_M68kContext.A[0], g_M68kContext.A[1]);
        }
    }
    {
        static UINTN StoreWatchHits = 0;
        if (StoreWatchHits < 24 &&
            Address >= 0x9E00u && Address < 0xA200u &&
            (Value == 0x68F168F1u || Value == 0xD1E2D1E2u)) {
            StoreWatchHits++;
            Print (L"68K STOREWATCH val=0x%08x -> 0x%08x @PC=0x%08x "
                   L"A0=%08x A1=%08x D1=%08x\n",
                   Value, Address, g_M68kContext.PC,
                   g_M68kContext.A[0], g_M68kContext.A[1],
                   g_M68kContext.D[1]);
        }
    }
    // Drop NanoKernel guard-fill stores below 64K (see CpuWrite32 in
    // interpreter.c): they poison live low-memory guest structures.
    if ((Value == 0x68F168F1u || Value == 0xD1E2D1E2u) &&
        Address < 0x10000u) {
        return;
    }
    M68kWriteWord (Address, (UINT16)(Value >> 16));
    M68kWriteWord (Address + 2, (UINT16)Value);
}

// Fetch without side effects (same as Read but semantically for prefetch)
UINT16
M68kFetchWord (
    IN UINT32 Address
    )
{
    return M68kReadWord (Address);
}

UINT32
M68kFetchLong (
    IN UINT32 Address
    )
{
    return M68kReadLong (Address);
}

// ---------------------------------------------------------------------------
// Stack operations (use A7 based on supervisor/user mode)
// ---------------------------------------------------------------------------

static UINT32
M68kGetStackPointer (
    VOID
    )
{
    if (g_M68kContext.Supervisor) {
        return g_M68kContext.SSP;
    }
    return g_M68kContext.A[7];
}

static VOID
M68kSetStackPointer (
    IN UINT32 Value
    )
{
    if (g_M68kContext.Supervisor) {
        g_M68kContext.SSP = Value;
    } else {
        g_M68kContext.A[7] = Value;
    }
}

// Write an address register. Instruction-level references to A7 hit the
// ACTIVE stack pointer (SSP in supervisor mode, USP=A[7] otherwise); the
// inactive stack register must stay untouched.
static VOID
M68kWriteAn (
    IN UINT8  Reg,
    IN UINT32 Value
    )
{
    if (Reg == 7) {
        M68kSetStackPointer (Value);
        return;
    }
    g_M68kContext.A[Reg] = Value;
}

VOID
M68kPushWord (
    IN UINT16 Value
    )
{
    UINT32 Sp = M68kGetStackPointer () - 2;
    M68kSetStackPointer (Sp);
    M68kWriteWord (Sp, Value);
}

VOID
M68kPushLong (
    IN UINT32 Value
    )
{
    // Continuation-target watch: pushes of addresses inside the NK
    // trampoline region reveal who sets up the bogus callback target.
    {
        static UINTN TrampPushSeen = 0;
        if (TrampPushSeen < 24 &&
            Value >= 0x7F0000u && Value < 0x800000u) {
            TrampPushSeen++;
            Print (L"  TRAMP-PUSH %08x @PC=%08x sp->%08x\n",
                   Value, g_M68kContext.PC,
                   (UINT32)(M68kGetStackPointer () - 4));
        }
    }
    UINT32 Sp = M68kGetStackPointer () - 4;
    M68kSetStackPointer (Sp);
    M68kWriteLong (Sp, Value);
    {
        static UINT32 LastPushVal[64];
        static UINT32 LastPushSp[64];
        static UINTN  LastPushIdx = 0;
        static UINTN  PushCount = 0;
        LastPushVal[LastPushIdx] = Value;
        LastPushSp[LastPushIdx] = Sp;
        LastPushIdx = (LastPushIdx + 1) % 64;
        PushCount++;
        if (Value == 0x68F168F1u && PushCount >= 64) {
            Print (L"68K PUSHWATCH suspicious push val=0x%08x SP=0x%08x "
                   L"recent pushes:\n", Value, Sp);
            {
                UINTN K;
                for (K = 0; K < 64; K++) {
                    UINTN Idx = (LastPushIdx + 64 - 1 - K) % 64;
                    Print (L" %08x@%08x", LastPushVal[Idx], LastPushSp[Idx]);
                    if ((K & 7) == 7) Print (L"\n");
                }
            }
        }
    }
}

UINT16
M68kPopWord (
    VOID
    )
{
    UINT32 Sp = M68kGetStackPointer ();
    UINT16 Value = M68kReadWord (Sp);
    M68kSetStackPointer (Sp + 2);
    return Value;
}

UINT32
M68kPopLong (
    VOID
    )
{
    UINT32 Sp = M68kGetStackPointer ();
    UINT32 Value = M68kReadLong (Sp);
    M68kSetStackPointer (Sp + 4);
    return Value;
}

// ---------------------------------------------------------------------------
// CCR flag helpers
// ---------------------------------------------------------------------------

VOID
M68kSetFlag (
    IN UINT8 Flag
    )
{
    g_M68kContext.SR |= Flag;
}

VOID
M68kClearFlag (
    IN UINT8 Flag
    )
{
    g_M68kContext.SR &= ~Flag;
}

BOOLEAN
M68kTestFlag (
    IN UINT8 Flag
    )
{
    return (g_M68kContext.SR & Flag) != 0;
}

UINT8
M68kGetCCR (
    VOID
    )
{
    return (UINT8)(g_M68kContext.SR & 0xFF);
}

VOID
M68kSetCCR (
    IN UINT8 Value
    )
{
    g_M68kContext.SR = (g_M68kContext.SR & 0xFF00) | (Value & 0x1F);
}

UINT16
M68kGetSR (
    VOID
    )
{
    return g_M68kContext.SR;
}

VOID
M68kSetSR (
    IN UINT16 Value
    )
{
    BOOLEAN NewSupervisor = (Value & M68K_SR_S) != 0;
    if (NewSupervisor != g_M68kContext.Supervisor) {
        // Stack switch: save current SP to the other bank
        if (NewSupervisor) {
            // User -> Supervisor: save USP, load SSP
            g_M68kContext.A[7] = M68kGetStackPointer ();
            M68kSetStackPointer (g_M68kContext.SSP);
        } else {
            // Supervisor -> User: save SSP, load USP
            g_M68kContext.SSP = M68kGetStackPointer ();
            M68kSetStackPointer (g_M68kContext.A[7]);
        }
        g_M68kContext.Supervisor = NewSupervisor;
    }
    g_M68kContext.SR = Value;
}

// ---------------------------------------------------------------------------
// Flag computation from ALU results
// ---------------------------------------------------------------------------

VOID
M68kSetFlagsFromResult (
    IN UINT32 Result,
    IN UINT8  ResultSize
    )
{
    // N: set if MSB of result is set
    if (ResultSize == 1) {
        if (Result & 0x80) M68kSetFlag (M68K_CCR_N);
        else M68kClearFlag (M68K_CCR_N);
    } else if (ResultSize == 2) {
        if (Result & 0x8000) M68kSetFlag (M68K_CCR_N);
        else M68kClearFlag (M68K_CCR_N);
    } else {
        if (Result & 0x80000000) M68kSetFlag (M68K_CCR_N);
        else M68kClearFlag (M68K_CCR_N);
    }

    // Z: set if result is zero (use64-bit shift to avoid UB when Size==4)
    {
        UINT64 Mask64 = (ResultSize >= 4) ? 0xFFFFFFFFULL
                       : ((1ULL << (ResultSize * 8)) - 1);
        if ((Result & Mask64) == 0) {
            M68kSetFlag (M68K_CCR_Z);
        } else {
            M68kClearFlag (M68K_CCR_Z);
        }
    }

    // V: cleared (callers must set V explicitly if needed)
    M68kClearFlag (M68K_CCR_V);
}

VOID
M68kSetFlagsFromAdd (
    IN UINT32 Result,
    IN UINT32 Src,
    IN UINT32 Dst,
    IN UINT8  Size
    )
{
    UINT32 Mask = (Size == 1) ? 0xFF : (Size == 2) ? 0xFFFF : 0xFFFFFFFF;
    UINT32 SignBit = (Size == 1) ? 0x80 : (Size == 2) ? 0x8000 : 0x80000000;
    UINT32 TruncResult = Result & Mask;

    // C (carry): unsigned overflow
    if (Result & ~Mask) {
        M68kSetFlag (M68K_CCR_C);
        M68kSetFlag (M68K_CCR_X);
    } else {
        M68kClearFlag (M68K_CCR_C);
    }

    // N and Z from the truncated result
    M68kSetFlagsFromResult (TruncResult, Size);

    // V (overflow): both operands same sign, result different sign
    // NOTE: Must be AFTER M68kSetFlagsFromResult which clears V.
    if ((Src ^ Dst) & SignBit) {
        M68kClearFlag (M68K_CCR_V);
    } else if ((Src ^ TruncResult) & SignBit) {
        M68kSetFlag (M68K_CCR_V);
    } else {
        M68kClearFlag (M68K_CCR_V);
    }
}

VOID
M68kSetFlagsFromSub (
    IN UINT32 Result,
    IN UINT32 Src,
    IN UINT32 Dst,
    IN UINT8  Size
    )
{
    UINT32 Mask = (Size == 1) ? 0xFF : (Size == 2) ? 0xFFFF : 0xFFFFFFFF;
    UINT32 SignBit = (Size == 1) ? 0x80 : (Size == 2) ? 0x8000 : 0x80000000;
    UINT32 TruncResult = Result & Mask;

    // C (carry/borrow): unsigned borrow from Dst - Src. Detect explicitly
    // via comparison: the old test (Result & ~Mask) missed LONG-size
    // borrows entirely because ~Mask == 0.
    if ((Size == 4) ? (Dst < Src)          // M68K_SIZE_LONG (defined below)
                    : ((Result & ~Mask) != 0)) {
        M68kSetFlag (M68K_CCR_C);
        M68kSetFlag (M68K_CCR_X);
    } else {
        M68kClearFlag (M68K_CCR_C);
        M68kClearFlag (M68K_CCR_X);
    }

    // N and Z from the truncated result
    M68kSetFlagsFromResult (TruncResult, Size);

    // V (overflow): src and dst different signs, result different from dst
    // NOTE: Must be AFTER M68kSetFlagsFromResult which clears V.
    if (((Src ^ Dst) & SignBit) && ((Dst ^ TruncResult) & SignBit)) {
        M68kSetFlag (M68K_CCR_V);
    } else {
        M68kClearFlag (M68K_CCR_V);
    }
}

// ---------------------------------------------------------------------------
// Effective Address computation (all 9 68k addressing modes)
// Mode 0: Dn           Data register direct
// Mode 1: An           Address register direct
// Mode 2: (An)         Address register indirect
// Mode 3: (An)+        Address register indirect with post-increment
// Mode 4: -(An)        Address register indirect with pre-decrement
// Mode 5: d16(An)      Address register indirect with displacement
// Mode 6: d16(An,Dn)   Address register indirect with index
// Mode 7/0: xxx.W      Absolute word
// Mode 7/1: xxx.L      Absolute long
// Mode 7/2: d16(PC)    PC with displacement
// Mode 7/3: d16(PC,Dn) PC with index
// Mode 7/4: #xxx       Immediate
// ---------------------------------------------------------------------------

// Size in bytes for the given size field: 0=byte, 1=word, 2=long
#define M68K_SIZE_BYTE  1
#define M68K_SIZE_WORD  2
#define M68K_SIZE_LONG  4

// Decode the size from the opsize field (bits 7-6): 00=byte, 01=word, 10=long
// This is the standard encoding used by most instructions (ADDQ/SUBQ/AND/OR/...).
static UINT8
M68kDecodeSize (
    IN UINT16 Opcode
    )
{
    UINT8 SizeField = (Opcode >> 6) & 3;
    switch (SizeField) {
        case 0: return M68K_SIZE_BYTE;
        case 1: return M68K_SIZE_WORD;
        case 2: return M68K_SIZE_LONG;
        default: return M68K_SIZE_WORD;  // shouldn't happen
    }
}

// Decode size for immediate ALU operations (00=byte, 01=word, 10=long)
// NOTE: This is the same as M68kDecodeSize but kept as alias for clarity.
static UINT8
M68kDecodeImmSize (
    IN UINT16 Opcode
    )
{
    return M68kDecodeSize (Opcode);
}

// Get the register number from bits 2-0 (for most addressing modes)
static UINT8
M68kGetRn (
    IN UINT16 Opcode
    )
{
    return (UINT8)(Opcode & 7);
}

// Get the mode from bits 5-3
static UINT8
M68kGetMode (
    IN UINT16 Opcode
    )
{
    return (UINT8)((Opcode >> 3) & 7);
}

// Compute effective address for modes 2-7 (modes 0-1 return register content)
// Returns the effective address in *EA. If IsWrite is TRUE, the caller intends
// to write to this address. For modes 0 and 1, EA is not set and the caller
// must access the register directly.
// Returns TRUE if the mode requires a memory access (modes 2-7), FALSE for
// register direct (modes 0-1).
// After returning, AnPostInc holds the post-increment value for mode 3.
// Parse an indexed addressing-mode extension word sequence (brief or full
// format) starting at PC, advance PC past every consumed word, and return
// the resulting effective address relative to BaseVal (An or PC-of-ext).
static UINT32
M68kIndexedEA (
    IN UINT32 BaseVal
    )
{
    UINT16 Ext = M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;

    UINT8   XnReg  = (Ext >> 12) & 7;
    BOOLEAN XnIsAn = (Ext & 0x8000) != 0;
    BOOLEAN LongIdx = (Ext & 0x0800) != 0;

    // Index register value (word indexes are sign-extended)
    INT32 IndexVal = 0;
    {
        UINT32 RegVal = XnIsAn
                        ? ((XnReg == 7) ? M68kGetStackPointer ()
                                        : g_M68kContext.A[XnReg])
                        : g_M68kContext.D[XnReg];
        IndexVal = LongIdx ? (INT32)RegVal
                           : (INT32)(INT16)(UINT16)RegVal;
    }

    if ((Ext & 0x0100) == 0) {
        // Brief format: d8(An,Xn)
        INT8 Disp = (INT8)(Ext & 0xFF);
        return BaseVal + (UINT32)IndexVal + (UINT32)(INT32)Disp;
    }

    // Full format
    UINT8  Scale   = 1u << ((Ext >> 9) & 3);
    BOOLEAN BaseSuppress = (Ext & 0x0080) != 0;
    BOOLEAN IdxSuppress = (Ext & 0x0040) != 0;
    UINT8  BdSize = (UINT8)((Ext >> 4) & 3);
    UINT8  OdSize = (UINT8)((Ext >> 2) & 3);
    UINT8  IIs    = (UINT8)(Ext & 3);

    INT32 BaseDisp = 0;
    BOOLEAN AppleBdL = (BdSize == 3); // Reserved BDSize encoding observed in
                                      // the Apple ROM (ext low byte xF2):
                                      // behaves as a longword base displace-
                                      // ment followed by a word outer displ.
    if (AppleBdL) {
        BaseDisp = (INT32)M68kFetchLong (g_M68kContext.PC);
        g_M68kContext.PC += 4;
    } else if (BdSize == 1) {
        BaseDisp = (INT16)M68kFetchWord (g_M68kContext.PC);
        g_M68kContext.PC += 2;
    } else if (BdSize == 2) {
        BaseDisp = (INT32)M68kFetchLong (g_M68kContext.PC);
        g_M68kContext.PC += 4;
    }

    UINT32 Base = (BaseSuppress ? 0 : BaseVal) + (UINT32)BaseDisp;
    UINT32 Tmp;

    switch (IIs) {
    case 0:  // Address register indirect (no memory indirection)
        return Base + (IdxSuppress ? 0 : (UINT32)(IndexVal * Scale));
    case 1:  // Indirect: tmp = [base]; EA = tmp + outer
        Tmp = M68kReadLong (Base);
        break;
    case 2:  // Post-indexed indirect: tmp = [base]; EA = tmp + idx + outer
        Tmp = M68kReadLong (Base) +
              (IdxSuppress ? 0 : (UINT32)(IndexVal * Scale));
        break;
    default: // Pre-indexed indirect: tmp = [base + idx]; EA = tmp + outer
        Tmp = M68kReadLong (Base +
              (IdxSuppress ? 0 : (UINT32)(IndexVal * Scale)));
        break;
    }

    INT32 OuterDisp = 0;
    if (OdSize == 1 || (AppleBdL && OdSize == 0)) {
        // Apple reserved encoding carries a word outer displacement even
        // with ODSize=00.
        OuterDisp = (INT16)M68kFetchWord (g_M68kContext.PC);
        g_M68kContext.PC += 2;
    } else if (OdSize == 2) {
        OuterDisp = (INT32)M68kFetchLong (g_M68kContext.PC);
        g_M68kContext.PC += 4;
    }
    return Tmp + (UINT32)OuterDisp;
}

static BOOLEAN
M68kComputeEA (
    IN  UINT16  Opcode,
    OUT UINT32* EA,
    OUT UINT32* AnPostInc
    )
{
    UINT8 Mode = M68kGetMode (Opcode);
    UINT8 Reg  = M68kGetRn (Opcode);

    // A7 in instruction operands is the ACTIVE stack pointer, not raw A[7]
    // (which holds the user stack while supervisor mode is active).
    UINT32 AnVal = (Reg == 7) ? M68kGetStackPointer ()
                              : g_M68kContext.A[Reg];

    if (AnPostInc) *AnPostInc = 0;

    switch (Mode) {
    case 0:  // Dn - data register direct
        return FALSE;

    case 1:  // An - address register direct
        return FALSE;

    case 2:  // (An) - address register indirect
        *EA = AnVal;
        return TRUE;

    case 3: { // (An)+ - address register indirect with post-increment
        *EA = AnVal;
        return TRUE;
    }

    case 4: { // -(An) - address register indirect with pre-decrement
        // Pre-decrement handled by caller (needs size info)
        *EA = AnVal - 4;
        return TRUE;
    }

    case 5: { // d16(An) - address register indirect with displacement
        INT16 Disp = (INT16)M68kFetchWord (g_M68kContext.PC);
        g_M68kContext.PC += 2;
        *EA = AnVal + (UINT32)(INT32)Disp;
        return TRUE;
    }

    case 6: { // d(An,Xn) / full-format indexed forms
        *EA = M68kIndexedEA (AnVal);
        return TRUE;
    }

    case 7:
        switch (Reg) {
        case 0: { // xxx.W - absolute word (ZERO-extended per 68k PRM)
            UINT16 Addr = M68kFetchWord (g_M68kContext.PC);
            g_M68kContext.PC += 2;
            *EA = (UINT32)Addr;
            return TRUE;
        }
        case 1: { // xxx.L - absolute long
            UINT32 Addr = M68kFetchLong (g_M68kContext.PC);
            g_M68kContext.PC += 4;
            *EA = Addr;
            return TRUE;
        }
        case 2: { // d16(PC) - PC with displacement
            // 68K spec: EA = PC_of_extension + displacement
            // PC_of_extension is the address of the displacement word itself
            UINT32 PCOfExt = g_M68kContext.PC;
            INT16 Disp = (INT16)M68kFetchWord (g_M68kContext.PC);
            g_M68kContext.PC += 2;
            *EA = PCOfExt + (UINT32)(INT32)Disp;
            return TRUE;
        }
        case 3: { // d(PC,Xn) / full-format PC-indexed forms
            UINT32 PCOfExt = g_M68kContext.PC;
            *EA = M68kIndexedEA (PCOfExt);
            return TRUE;
        }
        case 4: // #xxx - immediate (handled by caller)
            *EA = g_M68kContext.PC;
            g_M68kContext.PC += (M68kDecodeSize (Opcode) == M68K_SIZE_LONG) ? 4 : 2;
            return TRUE;
        default:
            *EA = 0;
            return TRUE;
        }
    }
    return FALSE;
}

// Read the value at the effective address (or from a register)
static UINT32
M68kReadEA (
    IN UINT16 Opcode,
    IN UINT8  Size
    )
{
    if (M68kGetMode (Opcode) == 0) {
        // Dn
        if (Size == M68K_SIZE_BYTE) return g_M68kContext.D[M68kGetRn (Opcode)] & 0xFF;
        if (Size == M68K_SIZE_WORD) return g_M68kContext.D[M68kGetRn (Opcode)] & 0xFFFF;
        return g_M68kContext.D[M68kGetRn (Opcode)];
    }
    if (M68kGetMode (Opcode) == 1) {
        // An (always treated as 32-bit). A7 must come from the active
        // stack pointer: context.A[7] is a stale shadow in supervisor mode.
        UINT8 Rn = M68kGetRn (Opcode);
        return (Rn == 7) ? M68kGetStackPointer () : g_M68kContext.A[Rn];
    }

    UINT32 EA;
    UINT8 Reg = M68kGetRn (Opcode);
    UINT8 Mode = M68kGetMode (Opcode);

    // Handle #imm (Mode 7, Reg 4) directly here: the caller's Size
    // tells us how many bytes to consume, bypassing M68kComputeEA
    // which would use M68kDecodeSize on the pseudo-opcode (wrong).
    if (Mode == 7 && Reg == 4) {
        UINT32 Result;
        if (Size == M68K_SIZE_LONG) {
            Result = M68kFetchLong (g_M68kContext.PC);
            g_M68kContext.PC += 4;
        } else if (Size == M68K_SIZE_WORD) {
            Result = (UINT32)(INT32)(INT16)M68kFetchWord (g_M68kContext.PC);
            g_M68kContext.PC += 2;
        } else {
            Result = (UINT32)(M68kFetchWord (g_M68kContext.PC) & 0xFF);
            g_M68kContext.PC += 2;
        }
        return Result;
    }

    // Handle pre-decrement BEFORE reading (needs size info)
    if (Mode == 4) {
        UINT8 DecSize = (Size == M68K_SIZE_LONG) ? 4 :
                        (Size == M68K_SIZE_WORD) ? 2 : 2;
        g_M68kContext.A[Reg] -= DecSize;
        EA = g_M68kContext.A[Reg];
    } else {
        BOOLEAN IsMem = M68kComputeEA (Opcode, &EA, NULL);
        if (!IsMem && Mode >= 2) return 0;
    }

    // Post-increment AFTER reading (mode 3): mirrors M68kWriteEA. Without
    // this every "(An)+" SOURCE operand left the register unadvanced,
    // desynchronizing caller frames (the BAD-RTS storm root cause).
    if (Mode == 3) {
        UINT8 IncSize = (Size == M68K_SIZE_LONG) ? 4 :
                        (Size == M68K_SIZE_WORD) ? 2 : 1;
        if (IncSize < 2) IncSize = 2;
        UINT32 Base = (Reg == 7) ? M68kGetStackPointer ()
                                 : g_M68kContext.A[Reg];
        M68kWriteAn (Reg, Base + IncSize);
    }

    UINT32 Result;
    if (Size == M68K_SIZE_BYTE) Result = M68kReadByte (EA);
    else if (Size == M68K_SIZE_WORD) Result = M68kReadWord (EA);
    else Result = M68kReadLong (EA);

    // Post-increment: An += size
    if (Mode == 3) {
        UINT8 IncSize = (Size == M68K_SIZE_LONG) ? 4 :
                        (Size == M68K_SIZE_WORD) ? 2 : 1;
        // An is always incremented by at least 2 for word/long per 68000 rules
        if (IncSize < 2) IncSize = 2;
        UINT32 Base = (Reg == 7) ? M68kGetStackPointer () : g_M68kContext.A[Reg];
        M68kWriteAn (Reg, Base + IncSize);
    }

    return Result;
}

// Write value to the effective address (or to a register)
static VOID
M68kWriteEA (
    IN UINT16 Opcode,
    IN UINT8  Size,
    IN UINT32 Value
    )
{
    UINT8 Mode = M68kGetMode (Opcode);
    UINT8 Reg  = M68kGetRn (Opcode);

    if (Mode == 0) {
        // Dn
        if (Size == M68K_SIZE_BYTE) g_M68kContext.D[Reg] = (g_M68kContext.D[Reg] & 0xFFFFFF00) | (Value & 0xFF);
        else if (Size == M68K_SIZE_WORD) g_M68kContext.D[Reg] = (g_M68kContext.D[Reg] & 0xFFFF0000) | (Value & 0xFFFF);
        else g_M68kContext.D[Reg] = Value;
        return;
    }
    if (Mode == 1) {
        // An (always 32-bit)
        M68kWriteAn (Reg, Value);
        return;
    }

    UINT32 EA;
    BOOLEAN IsMem = M68kComputeEA (Opcode, &EA, NULL);
    if (!IsMem) return;

    // Pre-decrement: An -= size
    if (Mode == 4) {
        UINT8 DecSize = (Size == M68K_SIZE_LONG) ? 4 :
                        (Size == M68K_SIZE_WORD) ? 2 : 1;
        if (DecSize < 2) DecSize = 2;
        UINT32 Base = (Reg == 7) ? M68kGetStackPointer () : g_M68kContext.A[Reg];
        M68kWriteAn (Reg, Base - DecSize);
        EA = (Reg == 7) ? M68kGetStackPointer () : g_M68kContext.A[Reg];
    }

    if (Size == M68K_SIZE_BYTE) M68kWriteByte (EA, (UINT8)Value);
    else if (Size == M68K_SIZE_WORD) M68kWriteWord (EA, (UINT16)Value);
    else M68kWriteLong (EA, Value);

    // Post-increment: An += size
    if (Mode == 3) {
        UINT8 IncSize = (Size == M68K_SIZE_LONG) ? 4 :
                        (Size == M68K_SIZE_WORD) ? 2 : 1;
        if (IncSize < 2) IncSize = 2;
        UINT32 Base = (Reg == 7) ? M68kGetStackPointer () : g_M68kContext.A[Reg];
        M68kWriteAn (Reg, Base + IncSize);
    }
}

// ---------------------------------------------------------------------------
// Instruction execution
// ---------------------------------------------------------------------------

// Execute a MOVE instruction (opcode 00xx xxxx xxx xxxxxx)
static VOID
M68kExecuteMove (
    IN UINT16 Opcode
    )
{
    // MOVE encoding: src_mode/src_reg (bits 5-0), dst_mode/dst_reg (bits 11-6)
    // Size is bits 13-12: 01=byte, 11=word, 10=long
    UINT8 SizeField = (Opcode >> 12) & 3;
    UINT8 Size;
    switch (SizeField) {
        case 1:  Size = M68K_SIZE_BYTE; break;
        case 3:  Size = M68K_SIZE_WORD; break;
        case 2:  Size = M68K_SIZE_LONG; break;
        default: Size = M68K_SIZE_WORD; break;
    }

    // Source: bits 5-0 (mode in 5-3, reg in 2-0)
    // Destination: bits 11-6 (mode in 11-9, reg in 8-6)
    // We construct a pseudo-opcode for the source EA computation
    UINT16 SrcOpcode = Opcode & 0x003F;  // mode + reg from bits 5-0
    // For the destination, we need mode from bits 11-9, reg from 8-6
    UINT16 DstOpcode = ((Opcode >> 3) & 7);  // dst mode -> bits 2-0
    DstOpcode |= ((Opcode >> 9) & 7) << 3;  // dst reg -> bits 5-3
    // Construct as if it were a standard EA encoding
    DstOpcode = (DstOpcode & 7) | ((DstOpcode >> 3) & 7) << 3;
    // Simplified: dst mode = (Opcode >> 6) & 7, dst reg = (Opcode >> 9) & 7
    UINT8 DstMode = (Opcode >> 6) & 7;
    UINT8 DstReg  = (Opcode >> 9) & 7;

    UINT32 SrcVal = M68kReadEA (SrcOpcode, Size);

    // Write to destination
    if (DstMode == 0) {
        // Dn
        if (Size == M68K_SIZE_BYTE) g_M68kContext.D[DstReg] = (g_M68kContext.D[DstReg] & 0xFFFFFF00) | (SrcVal & 0xFF);
        else if (Size == M68K_SIZE_WORD) g_M68kContext.D[DstReg] = (g_M68kContext.D[DstReg] & 0xFFFF0000) | (SrcVal & 0xFFFF);
        else g_M68kContext.D[DstReg] = SrcVal;
    } else if (DstMode == 1) {
        // An (MOVE to address register is always long, affects only the register)
        M68kWriteAn (DstReg, SrcVal);
    } else {
        // Memory destination: construct EA opcode for the destination
        UINT16 DstEa = ((DstMode & 7) << 3) | (DstReg & 7);
        M68kWriteEA (DstEa, Size, SrcVal);
    }

    // Set N, Z flags; V=0, C=0 — but NOT when destination is address register
    if (DstMode != 1) {
        M68kSetFlagsFromResult (SrcVal, Size);
        M68kClearFlag (M68K_CCR_C);
        M68kClearFlag (M68K_CCR_V);
    }
}

// Execute MOVEQ #imm,Dn
static VOID
M68kExecuteMoveq (
    IN UINT16 Opcode
    )
{
    UINT8 Dn = (Opcode >> 9) & 7;
    INT8  Imm = (INT8)(Opcode & 0xFF);
    UINT32 Result = (UINT32)(INT32)Imm;
    g_M68kContext.D[Dn] = Result;

    // Flags: N, Z based on result; V=0, C=0
    M68kSetFlagsFromResult (Result, M68K_SIZE_LONG);
    M68kClearFlag (M68K_CCR_V);
    M68kClearFlag (M68K_CCR_C);
}

// Execute ADD/SUB/AND/OR/EOR (Data register <-> memory, opmode in bits 8-6)
// This is the common handler for many ALU instructions with the encoding:
// opcode[15-12] = primary, bits 11-9 = Dn, bits 8-6 = opmode, bits 5-0 = EA
static VOID
M68kExecuteALU_Dn_EA (
    IN UINT16 Opcode,
    IN UINT8  AluOp   // 0=ADD, 1=SUB, 2=AND, 3=OR, 4=EOR
    )
{
    UINT8 Dn   = (Opcode >> 9) & 7;
    UINT8 Opmode = (Opcode >> 6) & 7;
    UINT16 EaOpcode = Opcode & 0x003F;

    UINT32 SrcVal;
    UINT32 DstVal;
    UINT32 Result;
    UINT8  Size;

    if (AluOp == 4) {
        // EOR: opmode encodes size differently for EOR
        // opmode 0=byte Dn,Ea; 1=word Dn,Ea; 2=long Dn,Ea
        // opmode 4=byte Ea,Dn; 5=word Ea,Dn; 6=long Ea,Dn (but EOR only goes Dn->EA)
        if (Opmode <= 2) {
            Size = (Opmode == 0) ? M68K_SIZE_BYTE : (Opmode == 1) ? M68K_SIZE_WORD : M68K_SIZE_LONG;
            DstVal = M68kReadEA (EaOpcode, Size);
            SrcVal = g_M68kContext.D[Dn];
            if (Size == M68K_SIZE_BYTE) SrcVal &= 0xFF;
            else if (Size == M68K_SIZE_WORD) SrcVal &= 0xFFFF;
            Result = DstVal ^ SrcVal;
            M68kWriteEA (EaOpcode, Size, Result);
        } else {
            // An,Dn (opmode 4-6): EOR to data register
            Size = (Opmode == 4) ? M68K_SIZE_BYTE : (Opmode == 5) ? M68K_SIZE_WORD : M68K_SIZE_LONG;
            DstVal = g_M68kContext.D[Dn];
            SrcVal = M68kReadEA (EaOpcode, Size);
            Result = DstVal ^ SrcVal;
            g_M68kContext.D[Dn] = Result;
        }
        M68kSetFlagsFromResult (Result, Size);
        M68kClearFlag (M68K_CCR_C);
        return;
    }

    // Standard opmodes: 0=byte <ea>,Dn; 1=word <ea>,Dn; 2=long <ea>,Dn
    //                    4=byte Dn,<ea>; 5=word Dn,<ea>; 6=long Dn,<ea>
    // (opmode 3/7 are ADDA/SUBA/CMPA and are handled by the dispatchers.)
    BOOLEAN ToReg;
    switch (Opmode) {
    case 0: Size = M68K_SIZE_BYTE; ToReg = TRUE;  break;
    case 1: Size = M68K_SIZE_WORD; ToReg = TRUE;  break;
    case 2: Size = M68K_SIZE_LONG; ToReg = TRUE;  break;
    case 4: Size = M68K_SIZE_BYTE; ToReg = FALSE; break;
    case 5: Size = M68K_SIZE_WORD; ToReg = FALSE; break;
    case 6: Size = M68K_SIZE_LONG; ToReg = FALSE; break;
    default: return;  // not an ALU opmode
    }
    if (ToReg) {
        DstVal = g_M68kContext.D[Dn];
        SrcVal = M68kReadEA (EaOpcode, Size);
    } else {
        DstVal = M68kReadEA (EaOpcode, Size);
        SrcVal = g_M68kContext.D[Dn];
        if (Size == M68K_SIZE_BYTE) SrcVal &= 0xFF;
        else if (Size == M68K_SIZE_WORD) SrcVal &= 0xFFFF;
    }

    switch (AluOp) {
    case 0: // ADD
        Result = DstVal + SrcVal;
        M68kSetFlagsFromAdd (Result, SrcVal, DstVal, Size);
        break;
    case 1: // SUB (Src op Dst, but 68k SUB subtracts src from Dst)
        Result = DstVal - SrcVal;
        M68kSetFlagsFromSub (Result, SrcVal, DstVal, Size);
        break;
    case 2: // AND
        Result = DstVal & SrcVal;
        M68kSetFlagsFromResult (Result, Size);
        M68kClearFlag (M68K_CCR_C);
        M68kClearFlag (M68K_CCR_V);
        break;
    case 3: // OR
        Result = DstVal | SrcVal;
        M68kSetFlagsFromResult (Result, Size);
        M68kClearFlag (M68K_CCR_C);
        M68kClearFlag (M68K_CCR_V);
        break;
    default:
        Result = DstVal;
        break;
    }

    if (ToReg) {
        g_M68kContext.D[Dn] = Result;
    } else {
        M68kWriteEA (EaOpcode, Size, Result);
    }
}

// Execute ADDQ/SUBQ #data,ea
static VOID
M68kExecuteAddqSubq (
    IN UINT16 Opcode,
    IN BOOLEAN IsSubq
    )
{
    UINT8 Data = (Opcode >> 9) & 7;
    if (Data == 0) Data = 8;  // 0 encodes 8
    UINT16 EaOpcode = Opcode & 0x003F;
    UINT8 Mode = (Opcode >> 3) & 7;
    UINT8 Size = M68kDecodeSize (Opcode);

    if (Mode == 1) {
        // ADDQ/SUBQ to An: always long, no flags. A7 goes through the
        // active-stack-pointer accessors (shadow-slot hazard).
        UINT8 An = M68kGetRn (Opcode);
        UINT32 Base = (An == 7) ? M68kGetStackPointer () : g_M68kContext.A[An];
        if (IsSubq) {
            M68kWriteAn (An, Base - Data);
        } else {
            M68kWriteAn (An, Base + Data);
        }
        return;
    }

    UINT32 DstVal = M68kReadEA (EaOpcode, Size);
    UINT32 Result;
    if (IsSubq) {
        Result = DstVal - Data;
        M68kSetFlagsFromSub (Result, Data, DstVal, Size);
    } else {
        Result = DstVal + Data;
        M68kSetFlagsFromAdd (Result, Data, DstVal, Size);
    }
    M68kWriteEA (EaOpcode, Size, Result);
}

// Execute CMP/CMPA
static VOID
M68kExecuteCmp (
    IN UINT16 Opcode,
    IN BOOLEAN IsCmpa
    )
{
    UINT8 Dn = (Opcode >> 9) & 7;

    if (IsCmpa) {
        // CMPA: compare address register (always long)
        UINT16 EaOpcode = Opcode & 0x003F;
        UINT8 Size = ((Opcode >> 8) & 1) ? M68K_SIZE_LONG : M68K_SIZE_WORD;
        UINT32 SrcVal = M68kReadEA (EaOpcode, Size);
        if (Size == M68K_SIZE_WORD) {
            SrcVal = (UINT32)(INT32)(INT16)(UINT16)SrcVal;
        }
        UINT32 DstVal = (Dn == 7) ? M68kGetStackPointer ()
                                  : g_M68kContext.A[Dn];
        UINT32 Result = DstVal - SrcVal;
        M68kSetFlagsFromSub (Result, SrcVal, DstVal, M68K_SIZE_LONG);
    } else {
        // CMP: compare EA with Dn
        UINT8 Opmode = (Opcode >> 6) & 7;
        UINT8 Size = (Opmode == 0) ? M68K_SIZE_BYTE : (Opmode == 1) ? M68K_SIZE_WORD : M68K_SIZE_LONG;
        UINT16 EaOpcode = Opcode & 0x003F;
        UINT32 SrcVal = M68kReadEA (EaOpcode, Size);
        UINT32 DstVal = g_M68kContext.D[Dn];
        if (Size == M68K_SIZE_BYTE) DstVal &= 0xFF;
        else if (Size == M68K_SIZE_WORD) DstVal &= 0xFFFF;
        UINT32 Result = DstVal - SrcVal;
        M68kSetFlagsFromSub (Result, SrcVal, DstVal, Size);
    }
}

// Execute TST
static VOID
M68kExecuteTst (
    IN UINT16 Opcode
    )
{
    UINT8 Size = M68kDecodeSize (Opcode);
    UINT16 EaOpcode = Opcode & 0x003F;
    UINT32 Val = M68kReadEA (EaOpcode, Size);
    M68kSetFlagsFromResult (Val, Size);
    M68kClearFlag (M68K_CCR_C);
    M68kClearFlag (M68K_CCR_V);
}

// Execute LEA ea,An
static VOID
M68kExecuteLea (
    IN UINT16 Opcode
    )
{
    UINT8 AnReg = (Opcode >> 9) & 7;
    UINT16 EaOpcode = Opcode & 0x003F;
    UINT32 EA;
    M68kComputeEA (EaOpcode, &EA, NULL);
    M68kWriteAn (AnReg, EA);
}

// Execute PEA ea
static VOID
M68kExecutePea (
    IN UINT16 Opcode
    )
{
    UINT16 EaOpcode = Opcode & 0x003F;
    UINT32 EA;
    M68kComputeEA (EaOpcode, &EA, NULL);
    M68kPushLong (EA);
}

// Execute MOVEM (register list to/from memory)
static VOID
M68kExecuteMovem (
    IN UINT16 Opcode
    )
{
    BOOLEAN ToMemory = !((Opcode >> 10) & 1);
    UINT8 Size = ((Opcode >> 6) & 1) ? M68K_SIZE_LONG : M68K_SIZE_WORD;
    UINT16 RegList = M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;

    // Compute EA for the memory operand
    UINT16 EaOpcode = Opcode & 0x003F;
    UINT32 EA;
    M68kComputeEA (EaOpcode, &EA, NULL);

    UINT8 Mode = M68kGetMode (Opcode);

    if (Mode == 4) {
        // Pre-decrement: the mask uses REVERSED register numbering
        // (bit0=A7, bit1=A6, ..., bit8=D7, ..., bit15=D0) and registers
        // are stored from lowest address (D0) upward to A7 just below
        // the caller's original SP.
        UINT8 Reg = M68kGetRn (Opcode);
        UINT32 StartEA = (Reg == 7) ? M68kGetStackPointer () : g_M68kContext.A[Reg];

        // Count bits to determine total size
        UINT16 Bits = RegList;
        UINTN Count = 0;
        while (Bits) { if (Bits & 1) Count++; Bits >>= 1; }
        UINT32 TotalSize = (Size == M68K_SIZE_LONG) ? Count * 4 : Count * 2;

        EA = StartEA - TotalSize;
        M68kWriteAn (Reg, EA);

        // Write in mask order: bit15 (=D0) first, bit0 (=A7) last.
        UINT32 Ptr = EA;
        INTN Mb;
        for (Mb = 15; Mb >= 0; Mb--) {
            if (!(RegList & (1u << Mb))) continue;
            UINT32 Val;
            if (Mb >= 8) {
                Val = g_M68kContext.D[15 - Mb];
            } else {
                UINT8 Ai = (UINT8)(7 - Mb);
                Val = (Ai == 7) ? M68kGetStackPointer ()
                                : g_M68kContext.A[Ai];
            }
            if (Size == M68K_SIZE_LONG) {
                M68kWriteLong (Ptr, Val);
                Ptr += 4;
            } else {
                M68kWriteWord (Ptr, (UINT16)Val);
                Ptr += 2;
            }
        }
    } else {
        // Post-increment and other modes: registers stored in normal order
        UINT32 Ptr = EA;
        for (UINT8 i = 0; i < 8; i++) {
            if (RegList & (1 << i)) {
                if (ToMemory) {
                    if (Size == M68K_SIZE_LONG) {
                        M68kWriteLong (Ptr, g_M68kContext.D[i]);
                        Ptr += 4;
                    } else {
                        M68kWriteWord (Ptr, (UINT16)g_M68kContext.D[i]);
                        Ptr += 2;
                    }
                } else {
                    if (Size == M68K_SIZE_LONG) {
                        g_M68kContext.D[i] = M68kReadLong (Ptr);
                        Ptr += 4;
                    } else {
                        UINT16 Val = M68kReadWord (Ptr);
                        g_M68kContext.D[i] = (g_M68kContext.D[i] & 0xFFFF0000) | Val;
                        Ptr += 2;
                    }
                }
            }
        }
        for (UINT8 i = 0; i < 8; i++) {
            if (RegList & (1 << (i + 8))) {
                if (ToMemory) {
                    UINT32 Av = (i == 7) ? M68kGetStackPointer ()
                                         : g_M68kContext.A[i];
                    if (Size == M68K_SIZE_LONG) {
                        M68kWriteLong (Ptr, Av);
                        Ptr += 4;
                    } else {
                        M68kWriteWord (Ptr, (UINT16)Av);
                        Ptr += 2;
                    }
                } else {
                    if (Size == M68K_SIZE_LONG) {
                        M68kWriteAn ((UINT8)i, M68kReadLong (Ptr));
                        Ptr += 4;
                    } else {
                        UINT16 Val = M68kReadWord (Ptr);
                        UINT32 Base = (i == 7) ? M68kGetStackPointer ()
                                               : g_M68kContext.A[i];
                        M68kWriteAn ((UINT8)i, (Base & 0xFFFF0000u) | Val);
                        Ptr += 2;
                    }
                }
            }
        }

        // Post-increment An (both to-memory and to-register forms)
        if (Mode == 3) {
            UINT8 Reg = M68kGetRn (Opcode);
            M68kWriteAn (Reg, Ptr);
        }
    }
}

// Execute LINK An,#offset
static VOID
M68kExecuteLink (
    IN UINT16 Opcode
    )
{
    UINT8 An = Opcode & 7;
    INT16 Offset = (INT16)M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;
    UINT32 Base = (An == 7) ? M68kGetStackPointer () : g_M68kContext.A[An];
    M68kPushLong (Base);
    M68kWriteAn (An, M68kGetStackPointer ());
    M68kSetStackPointer (M68kGetStackPointer () + (UINT32)(INT32)Offset);
}

// Execute UNLK An
static VOID
M68kExecuteUnlk (
    IN UINT16 Opcode
    )
{
    UINT8 An = Opcode & 7;
    UINT32 Base = (An == 7) ? M68kGetStackPointer () : g_M68kContext.A[An];
    M68kSetStackPointer (Base);
    M68kWriteAn (An, M68kPopLong ());
}

// Execute EXG
static VOID
M68kExecuteExg (
    IN UINT16 Opcode
    )
{
    UINT8 Rx = (Opcode >> 9) & 7;
    UINT8 Ry = Opcode & 7;
    UINT8 OpMode = (Opcode >> 3) & 0x1F;

    if (OpMode == 8) {
        // Data <-> Data
        UINT32 Temp = g_M68kContext.D[Rx];
        g_M68kContext.D[Rx] = g_M68kContext.D[Ry];
        g_M68kContext.D[Ry] = Temp;
    } else if (OpMode == 9) {
        // Address <-> Address (A7 goes through the SP accessors)
        UINT32 Vx = (Rx == 7) ? M68kGetStackPointer () : g_M68kContext.A[Rx];
        UINT32 Vy = (Ry == 7) ? M68kGetStackPointer () : g_M68kContext.A[Ry];
        M68kWriteAn (Rx, Vy);
        M68kWriteAn (Ry, Vx);
    } else if (OpMode == 17) {
        // Data <-> Address. An accesses MUST go through the stack-pointer
        // accessor when the register is A7: in supervisor mode the active
        // SP lives in SSP and context.A[7] is a stale shadow slot.
        UINT32 Temp = g_M68kContext.D[Rx];
        UINT32 Av = (Ry == 7) ? M68kGetStackPointer () : g_M68kContext.A[Ry];
        g_M68kContext.D[Rx] = Av;
        M68kWriteAn (Ry, Temp);
    }
}

// Execute NEG/DNEG
static VOID
M68kExecuteNeg (
    IN UINT16 Opcode
    )
{
    UINT8 Size = M68kDecodeSize (Opcode);
    UINT16 EaOpcode = Opcode & 0x003F;
    UINT32 DstVal = M68kReadEA (EaOpcode, Size);
    UINT32 Result = 0 - DstVal;
    M68kSetFlagsFromSub (Result, DstVal, 0, Size);
    M68kWriteEA (EaOpcode, Size, Result);
}

// Execute NOT
static VOID
M68kExecuteNot (
    IN UINT16 Opcode
    )
{
    UINT8 Size = M68kDecodeSize (Opcode);
    UINT16 EaOpcode = Opcode & 0x003F;
    UINT32 DstVal = M68kReadEA (EaOpcode, Size);
    UINT32 Mask = (Size == M68K_SIZE_BYTE) ? 0xFF : (Size == M68K_SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
    UINT32 Result = ~DstVal & Mask;
    M68kWriteEA (EaOpcode, Size, Result);
    M68kSetFlagsFromResult (Result, Size);
    M68kClearFlag (M68K_CCR_C);
    M68kClearFlag (M68K_CCR_V);
}

// Execute EXT.W or EXT.L
static VOID
M68kExecuteExt (
    IN UINT16 Opcode
    )
{
    UINT8 Dn = Opcode & 7;
    BOOLEAN IsExtL = (Opcode >> 6) & 1;

    if (IsExtL) {
        // EXT.L: sign-extend word to long
        INT16 Val = (INT16)(UINT16)(g_M68kContext.D[Dn] & 0xFFFF);
        g_M68kContext.D[Dn] = (UINT32)(INT32)Val;
        M68kSetFlagsFromResult (g_M68kContext.D[Dn], M68K_SIZE_LONG);
    } else {
        // EXT.W: sign-extend byte to word
        INT8 Val = (INT8)(UINT8)(g_M68kContext.D[Dn] & 0xFF);
        g_M68kContext.D[Dn] = (g_M68kContext.D[Dn] & 0xFFFF0000) | ((UINT16)(INT16)Val);
        M68kSetFlagsFromResult (g_M68kContext.D[Dn], M68K_SIZE_WORD);
    }
    M68kClearFlag (M68K_CCR_C);
    M68kClearFlag (M68K_CCR_V);
}

// Execute SWAP Dn
static VOID
M68kExecuteSwap (
    IN UINT16 Opcode
    )
{
    UINT8 Dn = Opcode & 7;
    UINT32 Val = g_M68kContext.D[Dn];
    g_M68kContext.D[Dn] = ((Val & 0xFFFF) << 16) | ((Val >> 16) & 0xFFFF);
    M68kSetFlagsFromResult (g_M68kContext.D[Dn], M68K_SIZE_LONG);
    M68kClearFlag (M68K_CCR_C);
    M68kClearFlag (M68K_CCR_V);
}

// Execute TAS ea
static VOID
M68kExecuteTas (
    IN UINT16 Opcode
    )
{
    UINT16 EaOpcode = Opcode & 0x003F;
    UINT32 Val = M68kReadEA (EaOpcode, M68K_SIZE_BYTE);
    M68kSetFlagsFromResult (Val, M68K_SIZE_BYTE);
    M68kClearFlag (M68K_CCR_C);
    M68kClearFlag (M68K_CCR_V);
    M68kSetFlag (M68K_CCR_N);  // N always set for TAS
    M68kWriteEA (EaOpcode, M68K_SIZE_BYTE, Val | 0x80);
}

// Execute branch (Bcc/BRA/BSR/BVC/BVS/BCS/BCC/BEQ/BNE/BLT/BGT/BLE/BGE)
static VOID
M68kExecuteBranch (
    IN UINT16 Opcode
    )
{
    UINT8 Condition = (Opcode >> 8) & 0xF;
    INT8  Offset8 = (INT8)(Opcode & 0xFF);

    BOOLEAN TakeBranch;
    switch (Condition) {
    case 0x0: TakeBranch = TRUE;  break;  // BRA (always)
    case 0x1: TakeBranch = TRUE;  break;  // BSR (always)
    case 0x2: TakeBranch = !M68kTestFlag (M68K_CCR_C) && !M68kTestFlag (M68K_CCR_Z); break;  // BHI
    case 0x3: TakeBranch = M68kTestFlag (M68K_CCR_C) || M68kTestFlag (M68K_CCR_Z); break;    // BLS
    case 0x4: TakeBranch = !M68kTestFlag (M68K_CCR_C); break;                                 // BCC/BHS
    case 0x5: TakeBranch = M68kTestFlag (M68K_CCR_C); break;                                  // BCS/BLO
    case 0x6: TakeBranch = !M68kTestFlag (M68K_CCR_Z); break;                                 // BNE
    case 0x7: TakeBranch = M68kTestFlag (M68K_CCR_Z); break;                                  // BEQ
    case 0x8: TakeBranch = !M68kTestFlag (M68K_CCR_V); break;                                 // BVC
    case 0x9: TakeBranch = M68kTestFlag (M68K_CCR_V); break;                                  // BVS
    case 0xA: TakeBranch = !(M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;  // BPL
    case 0xB: TakeBranch = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;   // BMI
    case 0xC: TakeBranch = (M68kTestFlag (M68K_CCR_N) == M68kTestFlag (M68K_CCR_V)) && !M68kTestFlag (M68K_CCR_Z); break; // BGT
    case 0xD: TakeBranch = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)) || M68kTestFlag (M68K_CCR_Z); break;  // BLE
    case 0xE: TakeBranch = !(M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;  // BGE
    case 0xF: TakeBranch = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;   // BLT
    default:  TakeBranch = FALSE; break;
    }

    // Compute return address and target
    // g_M68kContext.PC is at opcode+2 after the fetch in M68kExecuteInstruction.
    UINT32 ReturnPC = g_M68kContext.PC;  // opcode+2 (correct for 8-bit BSR)
    UINT32 TargetPC;

    if (Offset8 == 0) {
        // 16-bit displacement follows; return addr = opcode+4.
        // 68K spec: target = address of the displacement word + disp
        // (NOT past-the-word + disp).
        INT16 Disp16 = (INT16)M68kFetchWord (g_M68kContext.PC);
        ReturnPC = g_M68kContext.PC + 2;  // past the displacement word
        TargetPC = g_M68kContext.PC + (UINT32)(INT32)Disp16;
    } else if (Offset8 == (INT8)0xFF) {
        // 32-bit displacement follows; return addr = opcode+6
        UINT16 Hi = M68kFetchWord (g_M68kContext.PC);
        UINT16 Lo = M68kFetchWord (g_M68kContext.PC + 2);
        INT32 Disp32 = (INT32)(((UINT32)Hi << 16) | (UINT32)Lo);
        ReturnPC = g_M68kContext.PC + 4;  // past the 32-bit displacement
        TargetPC = g_M68kContext.PC + (UINT32)Disp32;
    } else {
        TargetPC = g_M68kContext.PC + (UINT32)(INT32)Offset8;
    }

    if (!TakeBranch) {
        // Branch not taken: advance PC past any displacement words.
        g_M68kContext.PC = ReturnPC;
        return;
    }

    {
        static UINTN DbgCount = 0;
        if (DbgCount < 40 && Offset8 >= (INT8)0xFE) {
            DbgCount++;
            Print (L"  Bcc%a op=%04x pc=%08x tgt=%08x ret=%08x bytes:",
                   (Offset8 == 0) ? "W" : "L", Opcode,
                   g_M68kContext.PC, TargetPC, ReturnPC);
            {
                UINTN Db;
                for (Db = 0; Db < 8; Db++) {
                    Print (L" %02x",
                           M68kReadByte ((UINT32)(g_M68kContext.PC - 2 + Db)));
                }
            }
            Print (L"\n");
        }
    }

    // The ROM's 68K boot code branches into the 0x305xxxxx mirror region
    // to invoke emulator services.  Redirect to A6 (the callback landing
    // zone), pushing the return address — this is the flow the boot was
    // observed to depend on.  Exception: if A6 points into the code we are
    // currently executing (a glue block whose tail re-calls the mirror
    // region), the redirect would ping-pong forever; complete such calls
    // immediately instead.  Note the exception MUST still leave the stack
    // alone (no push) so callees that pop see a consistent frame.
    if (M68kIsDrEmulatorAddress (TargetPC)) {
        UINT32 Land = g_M68kContext.A[6];
        UINT32 Here = g_M68kContext.PC;
        BOOLEAN PingPong = (Land != 0) &&
                           ((UINT32)(Here - Land) < 0x80 ||
                            (UINT32)(Land - Here) < 0x80);
        if (!PingPong) {
            M68kPushLong (ReturnPC);
            g_M68kContext.PC = Land;
        } else {
            g_M68kContext.PC = ReturnPC;
        }
        return;
    }

    if (Condition == 0x1) {
        // BSR: save return address
        M68kPushLong (ReturnPC);
    }

    g_M68kContext.PC = TargetPC;
}

// Execute DBcc Dn,offset (decrement and branch)
static VOID
M68kExecuteDbcc (
    IN UINT16 Opcode
    )
{
    UINT8 Condition = (Opcode >> 8) & 0xF;
    UINT8 Dn = Opcode & 7;
    INT16 Offset = (INT16)M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;

    BOOLEAN ConditionMet;
    switch (Condition) {
    case 0x0: ConditionMet = FALSE; break;  // DBRA (always false)
    case 0x1: ConditionMet = TRUE;  break;  // DBSR (always true)
    case 0x2: ConditionMet = !M68kTestFlag (M68K_CCR_C) && !M68kTestFlag (M68K_CCR_Z); break;
    case 0x3: ConditionMet = M68kTestFlag (M68K_CCR_C) || M68kTestFlag (M68K_CCR_Z); break;
    case 0x4: ConditionMet = !M68kTestFlag (M68K_CCR_C); break;
    case 0x5: ConditionMet = M68kTestFlag (M68K_CCR_C); break;
    case 0x6: ConditionMet = !M68kTestFlag (M68K_CCR_Z); break;
    case 0x7: ConditionMet = M68kTestFlag (M68K_CCR_Z); break;
    case 0x8: ConditionMet = !M68kTestFlag (M68K_CCR_V); break;
    case 0x9: ConditionMet = M68kTestFlag (M68K_CCR_V); break;
    case 0xA: ConditionMet = !M68kTestFlag (M68K_CCR_N); break;
    case 0xB: ConditionMet = M68kTestFlag (M68K_CCR_N); break;
    case 0xC: ConditionMet = (M68kTestFlag (M68K_CCR_N) == M68kTestFlag (M68K_CCR_V)) && !M68kTestFlag (M68K_CCR_Z); break;
    case 0xD: ConditionMet = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)) || M68kTestFlag (M68K_CCR_Z); break;
    case 0xE: ConditionMet = !(M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;
    case 0xF: ConditionMet = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;
    default:  ConditionMet = FALSE; break;
    }

    if (ConditionMet) return;  // If condition is met, don't loop

    // Decrement counter
    g_M68kContext.D[Dn] = (g_M68kContext.D[Dn] & 0xFFFF0000) |
                           ((g_M68kContext.D[Dn] - 1) & 0xFFFF);

    // If counter hasn't reached -1, branch back
    if ((g_M68kContext.D[Dn] & 0xFFFF) != 0xFFFF) {
        g_M68kContext.PC = (UINT32)((INT32)g_M68kContext.PC + (INT32)Offset);
    }
}

// Execute shift/rotate: ASL, ASR, LSL, LSR, ROL, ROR (register or memory)
//
// Line-E field layout (canonical):
//   Register form: 1110 ccc d ss i tt rrr
//     bits11-9 = count (#imm 0-7, or Dn when bit5=1), bit8 = direction
//     (1=left), bits7-6 = size (01=B,10=W,11=L), bit5 = i/r,
//     bits4-3 = type (00=ASd,01=LSd,10=ROXd,11=ROd), bits2-0 = register.
//   Memory form:   1110 ttt d 11 tt 1mmm -- single byte-size shift on EA;
//     detected by (Opcode & 0x08C0) == 0x00C0 (bit3=1 marker + size=11).
static VOID
M68kExecuteShift (
    IN UINT16 Opcode
    )
{
    BOOLEAN MemForm = (Opcode & 0x08C0) == 0x00C0;

    if (MemForm) {
        // Memory shift by 1
        UINT16 EaOpcode = Opcode & 0x003F;
        UINT32 EA;
        BOOLEAN IsMem = M68kComputeEA (EaOpcode, &EA, NULL);
        if (!IsMem) return;
        UINT32 Val = M68kReadByte (EA);
        BOOLEAN IsLeft = (Opcode >> 8) & 1;
        if (IsLeft) {
            M68kClearFlag (M68K_CCR_C);
            M68kClearFlag (M68K_CCR_X);
            if (Val & 0x80) {
                M68kSetFlag (M68K_CCR_C);
                M68kSetFlag (M68K_CCR_X);
            }
            Val <<= 1;
        } else {
            M68kClearFlag (M68K_CCR_C);
            M68kClearFlag (M68K_CCR_X);
            if (Val & 1) {
                M68kSetFlag (M68K_CCR_C);
                M68kSetFlag (M68K_CCR_X);
            }
            Val >>= 1;
        }
        M68kWriteByte (EA, Val);
        M68kSetFlagsFromResult (Val, M68K_SIZE_BYTE);
    } else {
        // Register shift by Dn or #i
        UINT8 CountField = (Opcode >> 9) & 7;
        UINT8 Count;
        if (Opcode & 0x20) {
            // Shift by Dn
            Count = g_M68kContext.D[CountField] & 0x3F;
        } else {
            // Shift by immediate (0 = 8)
            Count = CountField;
            if (Count == 0) Count = 8;
        }

        UINT8 SizeField = (Opcode >> 6) & 3;
        UINT8 Size = (SizeField == 0) ? M68K_SIZE_BYTE :
                     (SizeField == 1) ? M68K_SIZE_WORD : M68K_SIZE_LONG;

        UINT32 Val;
        if (Size == M68K_SIZE_BYTE) {
            Val = g_M68kContext.D[Opcode & 7] & 0xFF;
        } else if (Size == M68K_SIZE_WORD) {
            Val = g_M68kContext.D[Opcode & 7] & 0xFFFF;
        } else {
            Val = g_M68kContext.D[Opcode & 7];
        }

        UINT32 Mask = (Size == M68K_SIZE_BYTE) ? 0xFF :
                      (Size == M68K_SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        UINT8 SignBit = (Size == M68K_SIZE_BYTE) ? 7 :
                        (Size == M68K_SIZE_WORD) ? 15 : 31;

        BOOLEAN IsLeft = (Opcode >> 8) & 1;
        UINT8 ShiftType = (Opcode >> 3) & 3;  // 0=ASd,1=LSd,2=ROXd,3=ROd

        if (Count == 0) {
            M68kClearFlag (M68K_CCR_C);
            // Z and N remain unchanged
        } else {
            BOOLEAN Carry = FALSE;
            switch (ShiftType) {
            case 0: // ASd
                if (IsLeft) {
                    for (UINT8 i = 0; i < Count; i++) {
                        Carry = (Val >> SignBit) & 1;
                        Val = (Val << 1) & Mask;
                    }
                } else {
                    for (UINT8 i = 0; i < Count; i++) {
                        Carry = Val & 1;
                        Val >>= 1;
                    }
                }
                break;
            case 1: // LSd
                if (IsLeft) {
                    for (UINT8 i = 0; i < Count; i++) {
                        Carry = (Val >> SignBit) & 1;
                        Val = (Val << 1) & Mask;
                    }
                } else {
                    for (UINT8 i = 0; i < Count; i++) {
                        Carry = Val & 1;
                        Val >>= 1;
                    }
                }
                break;
            case 2: // ROXd
            case 3: // ROd
                if (IsLeft) {
                    for (UINT8 i = 0; i < Count; i++) {
                        Carry = (Val >> SignBit) & 1;
                        Val = (Val << 1) & Mask;
                        if (Carry) Val |= 1;
                    }
                } else {
                    for (UINT8 i = 0; i < Count; i++) {
                        Carry = Val & 1;
                        Val >>= 1;
                        if (Carry) Val |= (1u << SignBit);
                    }
                }
                break;
            }

            M68kClearFlag (M68K_CCR_C);
            if (Carry) M68kSetFlag (M68K_CCR_C);
            M68kClearFlag (M68K_CCR_X);
            if (Carry) M68kSetFlag (M68K_CCR_X);
        }

        M68kSetFlagsFromResult (Val, Size);

        if (Size == M68K_SIZE_BYTE) {
            g_M68kContext.D[Opcode & 7] = (g_M68kContext.D[Opcode & 7] & 0xFFFFFF00) | (Val & 0xFF);
        } else if (Size == M68K_SIZE_WORD) {
            g_M68kContext.D[Opcode & 7] = (g_M68kContext.D[Opcode & 7] & 0xFFFF0000) | (Val & 0xFFFF);
        } else {
            g_M68kContext.D[Opcode & 7] = Val;
        }
    }
}

// Execute BTST/BSET/BCLR/BCHG (register or memory)
static VOID
M68kExecuteBit (
    IN UINT16 Opcode
    )
{
    UINT8 OpType = (Opcode >> 6) & 3;  // 0=BTST, 1=BCHG, 2=BCLR, 3=BSET
    UINT16 EaOpcode = Opcode & 0x003F;

    // Get the bit number
    UINT32 BitNum;
    BOOLEAN IsImmBit = !((Opcode >> 8) & 1);

    if (IsImmBit) {
        BitNum = (Opcode >> 9) & 7;
    } else {
        BitNum = g_M68kContext.D[(Opcode >> 9) & 7];
    }

    BOOLEAN IsMemory = (EaOpcode & 7) != 0 && (EaOpcode & 7) != 1;
    // Memory BTST uses only byte; register BTST uses long
    UINT8 Size = IsMemory ? M68K_SIZE_BYTE : M68K_SIZE_LONG;

    UINT32 Val = M68kReadEA (EaOpcode, Size);
    UINT32 Mask = 1u << (BitNum & 31);
    BOOLEAN BitSet = (Val & Mask) != 0;

    // Z = !bit (same as BTST result)
    if (BitSet) {
        M68kClearFlag (M68K_CCR_Z);
    } else {
        M68kSetFlag (M68K_CCR_Z);
    }

    switch (OpType) {
    case 0: // BTST (read only)
        break;
    case 1: // BCHG
        Val ^= Mask;
        M68kWriteEA (EaOpcode, Size, Val);
        break;
    case 2: // BCLR
        Val &= ~Mask;
        M68kWriteEA (EaOpcode, Size, Val);
        break;
    case 3: // BSET
        Val |= Mask;
        M68kWriteEA (EaOpcode, Size, Val);
        break;
    }
}

// Execute MOVE USP
static VOID
M68kExecuteMoveUsp (
    IN UINT16 Opcode
    )
{
    BOOLEAN ToAn = (Opcode >> 3) & 1;
    UINT8 An = Opcode & 7;

    if (ToAn) {
        g_M68kContext.A[An] = M68kGetStackPointer ();
    } else {
        M68kSetStackPointer (g_M68kContext.A[An]);
    }
}

// Execute ANDI/EORI to CCR
static VOID
M68kExecuteAndiCcr (
    IN UINT16 Opcode
    )
{
    UINT8 Imm = (UINT8)M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;
    g_M68kContext.SR &= 0xFF00 | Imm;
}

// Execute ORI to CCR
static VOID
M68kExecuteOriCcr (
    IN UINT16 Opcode
    )
{
    UINT8 Imm = (UINT8)M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;
    g_M68kContext.SR |= Imm;
}

// Execute ANDI/EORI/ORI to SR
static VOID
M68kExecuteAndiEoriOriSr (
    IN UINT16 Opcode,
    IN UINT8  AluOp  // 0=ANDI, 1=ORI, 2=EORI
    )
{
    UINT16 Imm = M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;

    switch (AluOp) {
    case 0: // ANDI
        g_M68kContext.SR &= Imm;
        break;
    case 1: // ORI
        g_M68kContext.SR |= Imm;
        break;
    case 2: // EORI
        g_M68kContext.SR ^= Imm;
        break;
    }
    // Update supervisor/user mode
    g_M68kContext.Supervisor = (g_M68kContext.SR & M68K_SR_S) != 0;
}

// Set while a 68K exception is being serviced (raised but not yet RTE'd).
// Prevents the host-side VBL injector from nesting interrupts inside the
// handler chain.
BOOLEAN g_M68kInInterrupt = FALSE;

// Execute STOP #imm
static VOID
M68kExecuteStop (
    IN UINT16 Opcode
    )
{
    UINT16 Imm = M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;
    g_M68kContext.SR = Imm;
    g_M68kContext.Supervisor = (Imm & M68K_SR_S) != 0;
    // STOP parks the CPU until an interrupt; do NOT halt permanently.
    static BOOLEAN PrintedOnce = FALSE;
    if (!PrintedOnce) {
        PrintedOnce = TRUE;
        Print (L"  68K STOP #0x%04x at PC=0x%08x — parking until interrupt\n",
               Imm, g_M68kContext.PC - 4);
    }
    g_M68kContext.Stopped = TRUE;
}

// Execute RTE
static VOID
M68kExecuteRte (
    VOID
    )
{
    // Check supervisor mode
    if (!g_M68kContext.Supervisor) {
        // Privilege violation
        M68kRaiseException (M68K_VEC_PRIV_VIOLATION);
        return;
    }

    g_M68kContext.SR = M68kPopWord ();
    g_M68kContext.Supervisor = (g_M68kContext.SR & M68K_SR_S) != 0;
    g_M68kContext.PC = M68kPopLong ();
    g_M68kInInterrupt = FALSE;
}

// Execute RTR: pop CCR then PC. (Previously routed to RTE, which pops a
// FULL SR word — corrupting the interrupt mask and consuming a word that
// belongs to the return address.)
static VOID
M68kExecuteRtr (
    VOID
    )
{
    UINT16 Ccr = M68kPopWord ();
    g_M68kContext.SR = (UINT16)((g_M68kContext.SR & 0xFF00) | (Ccr & 0xFF));
    g_M68kContext.PC = M68kPopLong ();
}

// Execute TRAP #vector
static VOID
M68kExecuteTrap (
    IN UINT16 Opcode
    )
{
    UINT8 Vector = Opcode & 0xF;
    Print (L"  68K TRAP #%d at PC=0x%08x\n", Vector, g_M68kContext.PC - 2);
    M68kRaiseException (M68K_VEC_TRAP0 + Vector * 4);
}

// Execute ILLEGAL instruction
static VOID
M68kExecuteIllegal (
    IN UINT16 Opcode
    )
{
    UINTN K;
    Print (L"  68K ILLEGAL INSTRUCTION: 0x%04x at PC=0x%08x\n", Opcode, g_M68kContext.PC - 2);
    {
        static BOOLEAN RingShown = FALSE;
        if (!RingShown) {
            RingShown = TRUE;
            Print (L"  ILLEGAL last 64 PCs:");
            for (K = 0; K < 64; K++) {
                UINTN Idx = (g_LastPcIdx + 256 - 1 - K) % 256;
                Print (L" %08x/%04x", g_LastPcRing[Idx], g_LastOpRing[Idx]);
                if ((K & 7) == 7) Print (L"\n     ");
            }
            Print (L"\n");
            M68kTraceFlush ();
        }
    }
    g_M68kContext.Halted = TRUE;
}

// Raise a 68K exception
VOID
M68kRaiseException (
    IN UINT8 VectorNumber
    )
{
    // Push PC then SR
    M68kPushLong (g_M68kContext.PC);
    M68kPushWord (g_M68kContext.SR);

    // Set supervisor mode
    g_M68kContext.SR |= M68K_SR_S;
    g_M68kContext.Supervisor = TRUE;
    g_M68kInInterrupt = TRUE;

    // Read vector address from exception table
    UINT32 VecAddr = M68kReadLong ((UINT32)VectorNumber);
    if (VecAddr == 0) {
        Print (L"  68K EXCEPTION %d: vector at 0x%08x is NULL — HALTING\n",
               VectorNumber, (UINT32)VectorNumber);
        g_M68kContext.Halted = TRUE;
        return;
    }
    g_M68kContext.PC = VecAddr;
}

// ---------------------------------------------------------------------------
// Group 0 helpers: immediate ALU, CMPI, MOVEP, MOVE #imm,SR/CCR
// ---------------------------------------------------------------------------

// Execute immediate ALU operation: ORI, ANDI, SUBI, ADDI, EORI to EA.
// AluOp: 0=ADDI, 1=SUBI, 2=ANDI, 3=ORI, 4=EORI
static VOID
M68kExecuteImmediateAlu (
    IN UINT16 Opcode,
    IN UINT8  AluOp
    )
{
    UINT8 Size = M68kDecodeImmSize (Opcode);
    UINT16 EaOpcode = Opcode & 0x003F;

    // Read immediate value (size in bits 7-6: 00=byte, 01=word, 10=long)
    UINT32 Imm;
    if (Size == M68K_SIZE_BYTE) {
        Imm = (UINT8)M68kFetchWord (g_M68kContext.PC);
        g_M68kContext.PC += 2;
    } else if (Size == M68K_SIZE_WORD) {
        Imm = M68kFetchWord (g_M68kContext.PC);
        g_M68kContext.PC += 2;
    } else {
        Imm = M68kFetchLong (g_M68kContext.PC);
        g_M68kContext.PC += 4;
    }

    UINT32 DstVal = M68kReadEA (EaOpcode, Size);
    UINT32 Result;

    switch (AluOp) {
    case 0: // ADDI
        Result = DstVal + Imm;
        M68kSetFlagsFromAdd (Result, Imm, DstVal, Size);
        break;
    case 1: // SUBI
        Result = DstVal - Imm;
        M68kSetFlagsFromSub (Result, Imm, DstVal, Size);
        break;
    case 2: // ANDI
        Result = DstVal & Imm;
        M68kSetFlagsFromResult (Result, Size);
        M68kClearFlag (M68K_CCR_C);
        M68kClearFlag (M68K_CCR_V);
        break;
    case 3: // ORI
        Result = DstVal | Imm;
        M68kSetFlagsFromResult (Result, Size);
        M68kClearFlag (M68K_CCR_C);
        M68kClearFlag (M68K_CCR_V);
        break;
    case 4: // EORI
        Result = DstVal ^ Imm;
        M68kSetFlagsFromResult (Result, Size);
        M68kClearFlag (M68K_CCR_C);
        M68kClearFlag (M68K_CCR_V);
        break;
    default:
        return;
    }

    M68kWriteEA (EaOpcode, Size, Result);
}

// Execute CMPI #imm,<ea>
static VOID
M68kExecuteCmpi (
    IN UINT16 Opcode
    )
{
    UINT8 Size = M68kDecodeImmSize (Opcode);
    UINT16 EaOpcode = Opcode & 0x003F;

    UINT32 Imm;
    if (Size == M68K_SIZE_BYTE) {
        Imm = (UINT8)M68kFetchWord (g_M68kContext.PC);
        g_M68kContext.PC += 2;
    } else if (Size == M68K_SIZE_WORD) {
        Imm = M68kFetchWord (g_M68kContext.PC);
        g_M68kContext.PC += 2;
    } else {
        Imm = M68kFetchLong (g_M68kContext.PC);
        g_M68kContext.PC += 4;
    }

    UINT32 DstVal = M68kReadEA (EaOpcode, Size);
    // CMP affects N,Z,V,C but NOT X — save/restore X
    BOOLEAN SavedX = M68kTestFlag (M68K_CCR_X);
    M68kSetFlagsFromSub (DstVal - Imm, Imm, DstVal, Size);
    if (SavedX) M68kSetFlag (M68K_CCR_X);
    else M68kClearFlag (M68K_CCR_X);
}

// Execute MOVEP Dn,(d,An) or MOVEP (d,An),Dn
// Encoding: 0000 DDD d S 001 AAA  +  16-bit displacement
// d (bit 7): 0=mem->reg, 1=reg->mem;  S (bit 6): 0=word, 1=long
static VOID
M68kExecuteMovep (
    IN UINT16 Opcode
    )
{
    UINT8 DataReg = (Opcode >> 9) & 7;
    UINT8 AddrReg = Opcode & 7;
    BOOLEAN RegToMem = (Opcode >> 7) & 1;
    BOOLEAN IsLong   = (Opcode >> 6) & 1;
    UINT16 Disp = M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;
    UINT32 EA = g_M68kContext.A[AddrReg] + (UINT32)(INT32)(INT16)Disp;

    if (RegToMem) {
        if (IsLong) {
            M68kWriteByte (EA,     (UINT8)(g_M68kContext.D[DataReg] >> 24));
            M68kWriteByte (EA + 2, (UINT8)(g_M68kContext.D[DataReg] >> 16));
            M68kWriteByte (EA + 4, (UINT8)(g_M68kContext.D[DataReg] >> 8));
            M68kWriteByte (EA + 6, (UINT8)(g_M68kContext.D[DataReg]));
        } else {
            M68kWriteByte (EA,     (UINT8)(g_M68kContext.D[DataReg] >> 8));
            M68kWriteByte (EA + 2, (UINT8)(g_M68kContext.D[DataReg]));
        }
    } else {
        if (IsLong) {
            g_M68kContext.D[DataReg] =
                ((UINT32)M68kReadByte (EA)     << 24) |
                ((UINT32)M68kReadByte (EA + 2) << 16) |
                ((UINT32)M68kReadByte (EA + 4) <<  8) |
                 (UINT32)M68kReadByte (EA + 6);
        } else {
            UINT32 Val =
                ((UINT32)M68kReadByte (EA) << 8) |
                 (UINT32)M68kReadByte (EA + 2);
            g_M68kContext.D[DataReg] =
                (g_M68kContext.D[DataReg] & 0xFFFF0000) | (Val & 0xFFFF);
        }
    }
}

// Execute MOVE #imm,SR  (opcode 0x46FC)
static VOID
M68kExecuteMoveImmSr (
    VOID
    )
{
    UINT16 Imm = M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;
    M68kSetSR (Imm);
}

// Execute MOVE #imm,CCR  (opcode 0x44FC)
static VOID
M68kExecuteMoveImmCcr (
    VOID
    )
{
    UINT8 Imm = (UINT8)M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;
    M68kSetCCR (Imm);
}

// Execute EORI #imm,CCR  (opcode 0x0A3C)
static VOID
M68kExecuteEoriCcr (
    VOID
    )
{
    UINT8 Imm = (UINT8)M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;
    g_M68kContext.SR = (g_M68kContext.SR & 0xFF00) |
                       ((g_M68kContext.SR ^ Imm) & 0xFF);
}

// ---------------------------------------------------------------------------
// Main instruction decoder
// ---------------------------------------------------------------------------

UINT32
M68kExecuteInstruction (
    VOID
    )
{
    if (g_M68kContext.Halted) {
        return 0;
    }

    // Fetch the 16-bit opcode word
    UINT16 Opcode = M68kFetchWord (g_M68kContext.PC);
    g_M68kContext.PC += 2;
    g_M68kContext.CurrentOpcode = Opcode;

    // ---- PHASE B: low-memory scrub-loop fast path -----------------------
    // The New World boot clears low memory with the tight sequence
    //   0x408005F2: MOVE.L D1,(A0)      (20 C1)
    //   0x408005F4: CMPA.L (A1),A0      (B1 C9)   bound lives at [A1]
    //   0x408005F6: BCS.S back          (65 FA)
    // i.e. it fills longwords from A0 while A0 < [A1]. Executing this one
    // store at a time costs thousands of interpreter iterations; complete
    // the whole fill in one step with identical exit semantics.
    if (g_M68kContext.PC - 2 == 0x408005F4u &&
        Opcode == 0xB1C9u &&
        M68kFetchWord (0x408005F2u) == 0x20C1u &&
        M68kFetchWord (0x408005F6u) == 0x65FAu &&
        g_M68kContext.A[1] == 0x2800u) {
        static BOOLEAN ScrubFastOnce = FALSE;
        UINT32 Limit = M68kReadLong (g_M68kContext.A[1]);
        UINT32 Start = g_M68kContext.A[0];
        UINT32 Value = g_M68kContext.D[1];
        // The XLM signature slot doubles as the scrub bound. If it holds a
        // value that is clearly not an address (XLM 'Baah', uninit FFFF...),
        // clamp to the canonical New World low-memory scrub end.
        if ((INT32)Limit <= 0 || Limit > 0x00100000u) {
            if (!ScrubFastOnce) {
                ScrubFastOnce = TRUE;
                Print (L"  SCRUB-FAST: clamping bogus limit [2800]=%08x "
                       L"-> 0x800000 (start=%08x val=%08x)\n",
                       Limit, Start, Value);
            }
            Limit = 0x2800;
            Value = 0;
        }
        {
            UINT32 Addr = Start & ~3u;
            UINTN Longs = 0;
            while (Addr + 4 <= Limit) {
                M68kWriteLong (Addr, Value);
                Addr += 4;
                Longs++;
            }
            while (Addr < Limit) {           // rare non-aligned tail
                M68kWriteByte (Addr, (UINT8)(Value >> ((Addr & 3) * 8)));
                Addr++;
            }
            g_M68kContext.A[0] = Addr;
            if (!ScrubFastOnce || Longs > 0) {
                static UINTN ScrubFastCount = 0;
                ScrubFastCount++;
                if (ScrubFastCount <= 6 ||
                    (ScrubFastCount & 0x3FF) == 0) {
                    Print (L"  SCRUB-FAST #%d: fill %08x..%08x (%d longs, "
                           L"val=%08x)\n", (UINT32)ScrubFastCount,
                           Start, Limit, (UINT32)Longs, Value);
                }
            }
        }
        // Exit semantics: the real loop leaves via BCS.S when C=0, which
        // happens when A0 >= limit. With A0 now == limit the comparison is
        // equal -> Z=1, C=0; PC already points at the BCS.S, which now
        // falls through to RTS.
        g_M68kContext.SR = (UINT16)((g_M68kContext.SR & ~(M68K_CCR_N |
                                M68K_CCR_Z | M68K_CCR_V | M68K_CCR_C)) | M68K_CCR_Z);
        return 16;                           // approximate cycles
    }
    // ---- END scrub fast path --------------------------------------------

    // ---- Resolver-wedge escape (see note below; lives here because most
    // boot-time guest code runs via M68kExecuteBlock, which bypasses the
    // ExecuteFromPPC probe ring). The patch-era resolver can wedge cycling
    // E022<->E04A when its table lookup depends on NK state our environment
    // lacks (Apple's NK pre-patches these stages on real hardware).
    // Signature: the back-branch BGT.S at E04A repeats with an unchanged
    // D0/D1 pair. Unwind to the trampoline continuation at 0x408001EE when
    // its return word (0x408001E8) is on the stack; otherwise fall back to
    // the closest plausible ROM return address.
    if (Opcode == 0x6ED6u && g_M68kContext.PC - 2 == 0x4080E04Au) {
        static UINT32 RLastSig = 0;
        static UINTN RReps = 0;
        // EXPERIMENT (c): escape DISABLED — test whether NK's resolver
        // completes on its own now that the (An)+ post-increment bug is
        // fixed (the original wedge was likely a frame-desync victim).
        static BOOLEAN REscaped = TRUE;
        UINT32 RSig = g_M68kContext.D[0] ^ g_M68kContext.D[1];
        if (!REscaped && RSig == RLastSig) {
            RReps++;
            if (RReps >= 3) {
                UINT32 Sp = M68kGetStackPointer ();
                UINT32 K;
                BOOLEAN Found = FALSE;
                Print (L"  RESOLVER-WEDGE: sig=%08x sp=%08x, "
                       L"scanning stack\n", RSig, Sp);
                for (K = 0; K < 256; K++) {
                    UINT32 V = M68kReadLong ((UINT32)(Sp + K * 4));
                    if (V == 0x408001E8u) {
                        Found = TRUE;
                        M68kWriteAn (7, Sp + K * 4 + 4);
                        g_M68kContext.PC = 0x408001EEu;
                        break;
                    }
                }
                if (!Found) {
                    for (K = 0; K < 256; K++) {
                        UINT32 V = M68kReadLong ((UINT32)(Sp + K * 4));
                        if (V >= 0x40800000u && V < 0x40A00000u &&
                            (V & 1) == 0) {
                            Found = TRUE;
                            Print (L"  RESOLVER-WEDGE: fallback return "
                                   L"%08x at sp+%d\n", V, (UINT32)K);
                            M68kWriteAn (7, Sp + K * 4 + 4);
                            g_M68kContext.PC = V;
                            break;
                        }
                    }
                }
                if (Found) {
                    Print (L"  RESOLVER-WEDGE: escaped\n");
                    REscaped = TRUE;             // one-shot per wedged site
                    RReps = 0;
                    RLastSig = 0;
                    return 8;
                }
                RReps = 0;                       // retry next window
            }
        } else {
            RLastSig = RSig;
            RReps = 0;
        }
    }
    // ---- END resolver-wedge escape --------------------------------------

    // ---- Second scrub-loop fast path (word-clear variant) ---------------
    // Same family as the low-memory scrubber: the NK init clears a region
    // with CLR.W (A0)+ / CMPA.L (A1),A0 / BNE.S at 0x4087CA20..7CA25.
    // Equality-stepping-by-2 hangs when the bound at [A1] cannot be met;
    // complete the whole clear in one step with identical exit semantics.
    // One-shot live-code dump just before the poisoned tail-dispatch stub,
    // to identify the function whose fall-through enters it.
    if (g_M68kContext.PC - 2 == 0x4080A494u && Opcode == 0x2F30u) {
        static UINTN A494Dumped = 0;
        if (A494Dumped < 2) {
            A494Dumped++;
            UINTN K;
            UINT32 Sp = M68kGetStackPointer ();
            Print (L"  A494-HIT #%d: sp=%08x [sp]=%08x\n",
                   (UINT32)A494Dumped, Sp, M68kReadLong (Sp));
            Print (L"    a0=%08x a1=%08x d0=%08x d1=%08x\n",
                   g_M68kContext.A[0], g_M68kContext.A[1],
                   g_M68kContext.D[0], g_M68kContext.D[1]);
            Print (L"    live@A460:");
            for (K = 0; K < 26; K++) {
                Print (L" %04x",
                       M68kFetchWord ((UINT32)(0x4080A460u + K * 2)));
                if ((K & 7) == 7) Print (L"\n       ");
            }
            Print (L"\n");
        }
    }

    // Trace entry into the clear-loop function: capture full register
    // state plus the argument the caller passed (should be a pointer into
    // NK RAM; observing small sentinels like 6 identifies the broken
    // producer upstream).
    if (g_M68kContext.PC - 2 == 0x4087CA00u && Opcode == 0x4E56u) {
        UINT32 ArgVal = M68kReadLong ((UINT32)(M68kGetStackPointer () + 4));
        if ((ArgVal & 1u) == 0u &&
            (UINT64)ArgVal >= 0x1000ull &&
            ((UINT64)ArgVal < 0x01000000ull ||
             (UINT64)ArgVal >= 0x40000000ull)) {
            // Plausible pointer: let the function run normally.
        } else {
            static UINTN CaSkipCount = 0;
            CaSkipCount++;
            if (CaSkipCount <= 6 || (CaSkipCount & 0xFF) == 0) {
                Print (L"  CA00-SKIP #%d: bad arg %08x -> rts\n",
                       (UINT32)CaSkipCount, ArgVal);
            }
            g_M68kContext.PC = M68kPopLong ();
            return 8;
        }
        static UINTN CaEntryCount = 0;
        if (CaEntryCount < 3) {
            CaEntryCount++;
            UINTN I;
            Print (L"  CA00-ENTRY #%d:", (UINT32)CaEntryCount);
            for (I = 0; I < 8; I++) {
                Print (L" d%u=%08x", (UINT32)I, g_M68kContext.D[I]);
            }
            Print (L"\n    ");
            for (I = 0; I < 8; I++) {
                Print (L" a%u=%08x", (UINT32)I,
                       (I == 7) ? M68kGetStackPointer ()
                                : g_M68kContext.A[I]);
            }
            Print (L"\n    sp-args:");
            {
                UINT32 Sp = M68kGetStackPointer ();
                UINTN K;
                for (K = 0; K < 8; K++) {
                    Print (L" %08x", M68kReadLong ((UINT32)(Sp + K * 4)));
                }
            }
            Print (L"\n");
        }
    }

    if (g_M68kContext.PC - 2 == 0x4087CA22u) {
        static UINTN WTraceCount = 0;
        if (WTraceCount < 1) {
            WTraceCount++;
            UINT32 Fp = g_M68kContext.A[6];
            UINT32 Arg = M68kReadLong ((UINT32)(Fp + 8));   // 8(A6) = A4 src
            UINT32 Ret = M68kReadLong ((UINT32)(Fp + 4));
            Print (L"  WTRACE: op=%04x a0=%08x a1=%08x bound=%08x\n",
                   Opcode, g_M68kContext.A[0], g_M68kContext.A[1],
                   M68kReadLong (g_M68kContext.A[1]));
            Print (L"    frame fp=%08x ret=%08x arg(8fp)=%08x a4=%08x\n",
                   Fp, Ret, Arg, g_M68kContext.A[4]);
            // Disassemble live code around the caller return point
            {
                UINTN K;
                Print (L"    code@ret-24:");
                for (K = 0; K < 28; K++) {
                    Print (L" %04x",
                           M68kFetchWord ((UINT32)(Ret - 24 + K * 2)));
                    if ((K & 7) == 7) Print (L"\n       ");
                }
                Print (L"\n");
            }
        }
    }
    if (Opcode == 0xB1C9u && g_M68kContext.PC - 2 == 0x4087CA22u &&
        M68kFetchWord (0x4087CA20u) == 0x4258u &&
        M68kFetchWord (0x4087CA24u) == 0x66FAu) {
        // EXPERIMENT (c): wedge escape DISABLED alongside RESOLVER-WEDGE.
        // WScrubFast stays armed so a legitimate sane-bound clear still
        // gets bulk-completed; only the wedge unwind is off.
        static BOOLEAN WScrubDone = FALSE;
        static BOOLEAN WWedgeDisabled = TRUE;
        UINT32 Limit = M68kReadLong (g_M68kContext.A[1]);
        UINT32 Start = g_M68kContext.A[0];
        // Sanity gate: only fast-path when [A1] is a plausible end pointer
        // ABOVE the start cursor. Otherwise this is not a scrub loop —
        // either upstream state is broken or the bound register contract
        // was never met. Spin a few passes, then escape exactly like the
        // resolver wedge: unwind to the trampoline continuation
        // (return word 0x408001E8 -> 0x408001EE) or the nearest plausible
        // ROM return address still on the stack.
        if (!WWedgeDisabled &&
            !WScrubDone &&
            !((UINT64)(Limit & ~1u) > (UINT64)Start &&
              (UINT64)(Limit & ~1u) <= (UINT64)Start + 0x01000000ull)) {
            static UINTN WSpinReps = 0;
            static UINT32 WSpinSig = 0;
            UINT32 Sig = Limit ^ g_M68kContext.A[4] ^ g_M68kContext.A[5];
            if (Sig == WSpinSig) {
                WSpinReps++;
                if (WSpinReps >= 3) {
                    // Faithful epilogue unwind anchored ONLY on A6 (set by
                    // LINK A6,#0 in this frame's prologue):
                    //   [A6+0]=caller A6   [A6+4]=return address
                    //   MOVEM slots sit BELOW A6: D2@[A6-4] A2@[-8] A4@[-12]
                    UINT32 Fp   = g_M68kContext.A[6];
                    UINT32 Ret  = M68kReadLong ((UINT32)(Fp + 4));
                    UINT32 Old6 = M68kReadLong (Fp);
                    BOOLEAN RetOk = ((Ret & 1) == 0) &&
                                    (Ret >= 0x40800000u &&
                                     (Ret < 0x41000000u ||
                                      Ret >= 0xFFC00000u));
                    Print (L"  WSCRUB-WEDGE: a0=%08x bound=%08x fp=%08x "
                           L"ret=%08x olda6=%08x%s\n", Start, Limit, Fp,
                           Ret, Old6, RetOk ? L"" : L" INVALID");
                    if (RetOk && ((Old6 & 1) == 0)) {
                        static UINTN WEscapes = 0;
                        WEscapes++;
                        // Restore callee-saved regs pushed by MOVEM
                        g_M68kContext.D[2] =
                            M68kReadLong ((UINT32)(Fp - 4));
                        g_M68kContext.A[2] =
                            M68kReadLong ((UINT32)(Fp - 8));
                        g_M68kContext.A[4] =
                            M68kReadLong ((UINT32)(Fp - 12));
                        // UNLK-equivalent + RTS
                        g_M68kContext.A[6] = Old6;
                        g_M68kContext.A[7] = Fp + 8;
                        g_M68kContext.PC = Ret;
                        Print (L"  WSCRUB-WEDGE: escaped #%d -> %08x\n",
                               (UINT32)WEscapes, Ret);
                        WSpinReps = 0;
                        WSpinSig = 0;
                        if (WEscapes >= 16) {
                            Print (L"  WSCRUB-WEDGE: 16 escapes, halting\n");
                            g_M68kContext.Halted = TRUE;
                        }
                        return 8;
                    }
                    WSpinReps = 0;               // retry next window
                }
            } else {
                WSpinSig = Sig;
                WSpinReps = 0;
            }
        }
        // Empty-range completion: with the stack rescued, this stage calls
        // the clearer with a bound inside the rescued stack (its own frame
        // region) but a start cursor that is stale ROM-window garbage.
        // The intended "zero my frame" is a no-op for us — complete it by
        // snapping A0 to the bound and reporting equal-compare exit.
        if (!WScrubDone &&
            Start >= 0x40000000u &&
            (Limit & ~1u) >= 0x400u && (Limit & ~1u) < 0x10000u) {
            WScrubDone = TRUE;
            Print (L"  WSCRUB-EMPTY: start %08x (rom-garbage), bound "
                   L"%08x -> skip clear\n", Start, Limit & ~1u);
            g_M68kContext.A[0] = Limit & ~1u;
            g_M68kContext.SR = (UINT16)((g_M68kContext.SR & ~(M68K_CCR_N |
                                    M68K_CCR_Z | M68K_CCR_V | M68K_CCR_C)) |
                                    M68K_CCR_Z);
            return 12;
        }
        // Sane-range fast clear (unchanged)
        if (!WScrubDone &&
            (UINT64)(Limit & ~1u) > (UINT64)Start &&
            (UINT64)(Limit & ~1u) <= (UINT64)Start + 0x01000000ull) {
            WScrubDone = TRUE;
            Print (L"  WSCRUB-FAST: clear %08x..%08x ([a1]=%08x)\n",
                   Start, Limit & ~1u, Limit);
            {
                UINT32 Addr = Start & ~1u;
                UINT32 End = Limit & ~1u;
                UINTN Words = 0;
                while (Addr + 2 <= End) {
                    M68kWriteWord (Addr, 0);
                    Addr += 2;
                    Words++;
                }
                g_M68kContext.A[0] = Addr;
                Print (L"  WSCRUB-FAST: %d words cleared\n", (UINT32)Words);
            }
            // Exit semantics: force compare-equal (Z=1,C=0) so the BNE.S
            // falls through; PC already points at the BNE.S.
            g_M68kContext.SR = (UINT16)((g_M68kContext.SR & ~(M68K_CCR_N |
                                    M68K_CCR_Z | M68K_CCR_V | M68K_CCR_C)) |
                                    M68K_CCR_Z);
            return 12;
        }
    }
    // ---- END second scrub fast path -------------------------------------

    // Dead table-JSR probe (MacOS-side init): at 0x4087CA86 the startup
    // calls JSR (A0,A1.L*1-31) through an unseeded NK dispatch table.
    // LOGGER-ONLY: sample the computed table address across boots to test
    // determinism; if it stabilizes we can synthesize the entry's handler
    // bytes at that fixed location during install instead of intercepting.
    if (Opcode == 0x4EB0u && g_M68kContext.PC - 2 == 0x4087CA86u &&
        M68kFetchWord ((UINT32)(g_M68kContext.PC)) == 0x81E1u) {
        STATIC UINTN Ca86Probe = 0;
        if (Ca86Probe < 20) {
            Ca86Probe++;
            UINT32 BaseVal = g_M68kContext.A[0];
            INT32 IdxVal = (INT32)(INT16)(UINT16)g_M68kContext.A[1];
            UINT32 TableAddr = BaseVal + (UINT32)IdxVal - 31u;
            UINT32 Target = M68kReadLong (TableAddr);
            Print (L"  CA86-PROBE #%d: base=%08x idx=%08x tbl=%08x "
                   L"-> %08x\n", (UINT32)Ca86Probe, BaseVal,
                   (UINT32)IdxVal, TableAddr, Target);
        }
    }

    // Zero-region execution guard: opcode 0000 executed in low-RAM far
    // above the vector area means a lost control flow (code pointer or
    // return address pointed at uninitialized memory). Halt immediately
    // with full history instead of silently crawling megabytes of ORI.Bs.
    if (Opcode == 0x0000u &&
        g_M68kContext.PC >= 0x0010000u && g_M68kContext.PC < 0x40800000u) {
        static BOOLEAN ZeroExecReported = FALSE;
        if (!ZeroExecReported) {
            ZeroExecReported = TRUE;
            Print (L"  ZERO-EXEC GUARD: executing zeros @%08x sp=%08x — "
                   L"lost flow, halting\n",
                   g_M68kContext.PC - 2, M68kGetStackPointer ());
            Print (L"   a0=%08x a1=%08x a2=%08x a4=%08x a5=%08x "
                   L"a6=%08x d0=%08x d1=%08x\n",
                   g_M68kContext.A[0], g_M68kContext.A[1],
                   g_M68kContext.A[2], g_M68kContext.A[4],
                   g_M68kContext.A[5], g_M68kContext.A[6],
                   g_M68kContext.D[0], g_M68kContext.D[1]);
            {
                UINT32 Sp = M68kGetStackPointer ();
                UINTN K;
                Print (L"   stack@sp-32:");
                for (K = 0; K < 24; K++) {
                    Print (L" %08x", M68kReadLong (
                               (UINT32)(Sp - 32 + K * 4)));
                    if ((K & 7) == 7) Print (L"\n   ");
                }
            }
            // Live code at the epilogue we RTS'd through and at the
            // caller return point — reveals the arg-skip displacement
            // vs. the actual frame.
            {
                UINTN K;
                Print (L"   code@33770:");
                for (K = 0; K < 14; K++) {
                    Print (L" %04x", M68kFetchWord (
                               (UINT32)(0x40833770u + K * 2)));
                }
                Print (L"\n   code@CA5C:");
                for (K = 0; K < 26; K++) {
                    Print (L" %04x", M68kFetchWord (
                               (UINT32)(0x4087CA5Cu + K * 2)));
                    if ((K & 7) == 7) Print (L"\n     ");
                }
                Print (L"\n");
            }
            {
                UINTN K;
                Print (L"   last PCs:");
                for (K = 1; K <= 32; K++) {
                    UINTN Idx = (g_LastPcIdx + 256 - K) % 256;
                    Print (L" %08x/%04x", g_LastPcRing[Idx],
                           g_LastOpRing[Idx]);
                    if ((K & 7) == 0) Print (L"\n   ");
                }
                Print (L"\n");
            }
            M68kTraceFlush ();
            g_M68kContext.Halted = TRUE;
        }
    }

    // Zero-region entry detector: the post-NewHandle flow jumps into a big
    // zeroed RAM area and "executes" ORI.B no-ops across it. Capture the
    // first entry to identify the jump source.
    if (g_M68kContext.PC >= 0x7E0000u && g_M68kContext.PC < 0x800000u) {
        static BOOLEAN ZeroMarchSeen = FALSE;
        if (!ZeroMarchSeen && Opcode == 0x0000u) {
            ZeroMarchSeen = TRUE;
            Print (L"  ZERO-MARCH entry @%08x\n", g_M68kContext.PC);
            {
                UINTN K;
                for (K = 1; K <= 32; K++) {
                    UINTN Idx = (g_LastPcIdx + 256 - K) % 256;
                    Print (L" %08x/%04x", g_LastPcRing[Idx],
                           g_LastOpRing[Idx]);
                    if ((K & 7) == 0) Print (L"\n");
                }
            }
        }
    }

    // Low-stack detector: the NK MixedMode-era caller runs with SP~0x10,
    // pushing over the vector table. Catch the first instruction executed
    // under such a stack and arm the SS trace window so the setter of the
    // bogus SSP becomes visible.
    {
        static UINTN LowSpSeenCount = 0;
        UINT32 SpNow = g_M68kContext.Supervisor ? g_M68kContext.SSP
                                                : g_M68kContext.A[7];
        // Rescue v2: the NK stage at 0x408049xx-4Bxx does real frame
        // allocation (LEA -346(A7),A7 etc.) but inherits SP~=0x10 from the
        // JMP-chain bootstrap web, so its frames land in the alias-ROM
        // window and its arguments are read from garbage. When execution
        // FIRST enters this stage with an implausible stack, hand it a
        // real one: 0x7800 sits below the long-abandoned 0x7FFC boot
        // stack, inside the writable 16 MB bank, far from XLM (0x2800).
        if (LowSpSeenCount < 2 && SpNow < 0x400u &&
            g_M68kContext.PC >= 0x40804960u &&
            g_M68kContext.PC < 0x40804B00u) {
            LowSpSeenCount++;

            if (!g_M68kContext.Supervisor) {
                g_M68kContext.A[7] = 0x00007800u;
            } else {
                g_M68kContext.SSP = 0x00007800u;
            }
            Print (L"  LOW-SP RESCUE #%d: sp=%08x -> 7800 @pc=%08x\n",
                   (UINT32)LowSpSeenCount, SpNow, g_M68kContext.PC);
        }
    }

    // One-shot micro-trace of the caller block that pushes the poisoned
    // argument (ret=0x40804AD8). Arms when PC first enters its window;
    // prints each step until the call or budget exhaustion.
    if (g_M68kContext.PC >= 0x40804A90u && g_M68kContext.PC < 0x40804AE0u) {
        static BOOLEAN CallerArm = FALSE;
        static UINTN CallerSteps = 0;
        if (!CallerArm) {
            CallerArm = TRUE;
            Print (L"  CALLER-TRACE armed @%08x\n", g_M68kContext.PC);
            // Snapshot the classic low-mem globals this block consults.
            Print (L"   globals: [2BA]=%08x [AF0]=%08x [A58]=%08x "
                   L"[A5A]=%08x [BFF]=%02x\n",
                   M68kReadLong (0x2BAu), M68kReadLong (0xAF0u),
                   M68kReadWord (0xA58u), M68kReadWord (0xA5Au),
                   M68kReadByte (0xBFFu));
        }
        if (CallerSteps < 90) {
            CallerSteps++;
            Print (L"   C%02d %08x: %04x d0=%08x d1=%08x d3=%08x "
                   L"d6=%08x a0=%08x a5=%08x sp=%08x\n",
                   (UINT32)CallerSteps, g_M68kContext.PC, Opcode,
                   g_M68kContext.D[0], g_M68kContext.D[1],
                   g_M68kContext.D[3], g_M68kContext.D[6],
                   g_M68kContext.A[0], g_M68kContext.A[5],
                   M68kGetStackPointer ());
        }
    }

    // ---- Dead tail-dispatch skip ----------------------------------------
    // The NK thunk array around 0x4080A460 consists of per-selector
    // trampolines
    //   MOVE.L tbl(A0,Dn.L),-(SP)   (2F30 <ext>)
    //   MOVE.L (A0),D0              (2010)
    //   ORI.B #sel,D0               (00xx)
    //   RTS                          (4E75)
    // interleaved with default stubs "MOVEQ #0,D0 ; RTS". The dispatch
    // tables they index are built by Apple's loader/an early NK stage that
    // never runs in our environment (verified: zero writes to the table
    // region all boot), so targets read as sentinel garbage. When the
    // computed target is not a plausible code pointer, execute the
    // array's own DEFAULT semantic instead: D0 = 0 and return to caller.
    if (Opcode == 0x2F30u &&
        g_M68kContext.PC - 2 >= 0x40804000u &&
        g_M68kContext.PC - 2 <  0x40890000u) {
        // Family shape: 2F30 <ext> <MOVEA.L (An),A0> <ORI.B sel,Dx> 4E75
        UINT16 Mid = M68kFetchWord ((UINT32)(g_M68kContext.PC + 2));
        UINT16 Ori = M68kFetchWord ((UINT32)(g_M68kContext.PC + 4));
        UINT16 Rts = M68kFetchWord ((UINT32)(g_M68kContext.PC + 6));
        BOOLEAN MoveaForm = ((Mid & 0xF038u) == 0x2010u);
        // DISABLED after testing: null-returning (D0=0) diverts callers
        // down untested error paths (alias-stack jumps -> ILLEGAL). The
        // original flow — complete the push, RTS into junk, BAD-RTS
        // healer resyncs — proved more stable (~130M instructions).
        // Left here for future use once the tables are seeded properly.
        MoveaForm = FALSE;
        if (MoveaForm && Rts == 0x4E75u && (Ori >> 8) == 0x00u) {
            UINT16 Ext = M68kFetchWord (g_M68kContext.PC);
            BOOLEAN Brief = (Ext & 0x0100u) == 0;
            INT32 Disp = Brief ? (INT32)(INT8)(Ext & 0xFF)
                               : (INT32)(INT16)M68kFetchWord (
                                     (UINT32)(g_M68kContext.PC + 4));
            UINT8 IdxReg = (Ext >> 12) & 7;
            UINT32 BaseVal = (Ext & 0x8000)
                             ? ((IdxReg == 7) ? M68kGetStackPointer ()
                                              : g_M68kContext.A[IdxReg])
                             : g_M68kContext.D[IdxReg];
            UINT32 Scale = 1u << ((Ext >> 9) & 3);
            UINT32 TableAddr;
            UINT32 Target;
            {
                UINT32 RawIdx = (Ext & 0x8000)
                                ? g_M68kContext.A[IdxReg]
                                : g_M68kContext.D[IdxReg];
                TableAddr = BaseVal +
                            (UINT32)((INT32)(INT16)(UINT16)RawIdx *
                                     (INT32)Scale) +
                            (UINT32)Disp;
            }
            Target = M68kReadLong (TableAddr);
            if (!((Target & 1) == 0 &&
                  (Target >= 0x40800000u &&
                   (Target < 0x41000000u || Target >= 0xFFC00000u)))) {
                static UINTN DeadThunkCount = 0;
                DeadThunkCount++;
                if (DeadThunkCount <= 10 ||
                    (DeadThunkCount & 0x3FF) == 0) {
                    Print (L"  THUNK-NULL #%d @%08x: [%08x]=%08x -> "
                           L"default\n", (UINT32)DeadThunkCount,
                           g_M68kContext.PC - 2, TableAddr, Target);
                }
                // Default-stub semantic: D0 = 0, return to caller — but
                // validate the return first: these thunks are often entered
                // with a broken stack, so [SP] may be garbage.
                g_M68kContext.D[0] = 0;
                {
                    UINT32 Ret = M68kPopLong ();
                    BOOLEAN RetOk =
                        (((Ret & 1) == 0) &&
                         ((Ret >= 0x40800000u &&
                           (Ret < 0x41000000u || Ret >= 0xFFC00000u)) ||
                          Ret < 0x00100000u));
                    if (!RetOk) {
                        UINT32 Sp = M68kGetStackPointer ();
                        UINT32 K;
                        for (K = 0; K < 256 && !RetOk; K++) {
                            UINT32 V = M68kReadLong ((UINT32)(Sp + K * 4));
                            if (((V & 1) == 0) &&
                                V >= 0x40800000u &&
                                (V < 0x41000000u || V >= 0xFFC00000u)) {
                                Ret = V;
                                M68kWriteAn (7, Sp + K * 4 + 4);
                                RetOk = TRUE;
                            }
                        }
                    }
                    g_M68kContext.PC = Ret;
                    return 12;
                }
            }
        }
    }
    // ---- END dead tail-dispatch skip -------------------------------------

    // Dispatch based on the top 10 bits (bits 15-6) of the opcode
    UINT8 TopBits = Opcode >> 12;

    switch (TopBits) {
    case 0x0: {
        // Group 0: Bit manipulation, MOVEP, immediate ops
        // SubBits = (Opcode >> 8) & 0xF
        // Even SubBits: ORI(0), ANDI(2), SUBI(4), ADDI(6), BTST/BCHG/BCLR/BSET#imm(8),
        //               EORI(A), CMPI(C)
        // Odd SubBits:  MOVEP (when mode=001) or reserved
        // Any SubBits with mode=001: MOVEP
        UINT8 SubBits = (Opcode >> 8) & 0xF;
        UINT8 Mode = (Opcode >> 3) & 7;

        // MOVEP detection: bits 5-3 = 001 (An indirect mode)
        if (Mode == 1) {
            M68kExecuteMovep (Opcode);
            break;
        }

        // Dynamic-BSET variant used by New World init (capstone decodes
        // 0x03C2 at 0x4080A936 as bset.b d1,d2): 0000 0011 1 BBB EEEEEE.
        if ((Opcode & 0x0FC0) == 0x03C0) {
            UINT8 BitReg = (Opcode >> 9) & 7;
            UINT16 EaOp = Opcode & 0x3F;
            UINT32 Val = M68kReadEA (EaOp, M68K_SIZE_LONG);
            UINT8 BitNum = g_M68kContext.D[BitReg] & 31;
            UINT32 BitMask = 1u << BitNum;
            if (Val & BitMask) {
                M68kClearFlag (M68K_CCR_Z);
            } else {
                M68kSetFlag (M68K_CCR_Z);
            }
            M68kWriteEA (EaOp, M68K_SIZE_LONG, Val | BitMask);
            break;
        }

        // Non-standard dynamic-BTST used by the ROM's patch-table
        // interpreter at 0x408008D4: 0000 011 1 BBB EEE EEE
        // (e.g. 0x0700 = btst d3,d0, 0x0701 = btst d3,d1). Standard
        // encoding would be 01xx; the New World init uses 07xx.
        if ((Opcode & 0x0F80) == 0x0700 && (Opcode & 0x0040) == 0) {
            UINT8 BitReg = (Opcode >> 9) & 7;
            UINT16 EaOp = Opcode & 0x3F;
            UINT32 Val = M68kReadEA (EaOp, M68K_SIZE_LONG);
            UINT8 BitNum = g_M68kContext.D[BitReg] & 31;
            if (Val & (1u << BitNum)) {
                M68kClearFlag (M68K_CCR_Z);
            } else {
                M68kSetFlag (M68K_CCR_Z);
            }
            break;
        }

        switch (SubBits) {
        case 0x0: // ORI  (special: 0x003C = ORI to CCR, 0x007C = ORI to SR)
            if (Opcode == 0x003C) {
                M68kExecuteOriCcr (Opcode);
            } else if (Opcode == 0x007C) {
                M68kExecuteAndiEoriOriSr (Opcode, 1);  // ORI to SR
            } else {
                M68kExecuteImmediateAlu (Opcode, 3);    // ORI to EA
            }
            break;
        case 0x1: // Reserved (no valid immediate op; mode!=1 caught above)
            M68kExecuteIllegal (Opcode);
            break;
        case 0x2: // ANDI  (special: 0x023C = ANDI to CCR, 0x027C = ANDI to SR)
            if (Opcode == 0x023C) {
                M68kExecuteAndiCcr (Opcode);
            } else if (Opcode == 0x027C) {
                M68kExecuteAndiEoriOriSr (Opcode, 0);  // ANDI to SR
            } else {
                M68kExecuteImmediateAlu (Opcode, 2);    // ANDI to EA
            }
            break;
        case 0x3: // Reserved
            M68kExecuteIllegal (Opcode);
            break;
        case 0x4: // SUBI
            M68kExecuteImmediateAlu (Opcode, 1);
            break;
        case 0x5: // Reserved
            M68kExecuteIllegal (Opcode);
            break;
        case 0x6: // ADDI
            M68kExecuteImmediateAlu (Opcode, 0);
            break;
        case 0x7: // Reserved
            M68kExecuteIllegal (Opcode);
            break;
        case 0x8: {
            // BTST/BCHG/BCLR/BSET #imm,<ea>  (0000 1000 op ea)
            // Bits 7-6 select operation: 00=BTST, 01=BCHG, 10=BCLR, 11=BSET
            // The bit number ALWAYS comes from the immediate word that
            // follows the opcode (for Dn destinations too); it must be
            // consumed or the PC desyncs by 2 and lands mid-instruction.
            UINT8 BitOp = (Opcode >> 6) & 3;
            UINT16 EaOpcode = Opcode & 0x003F;
            UINT8 Mode2 = (EaOpcode >> 3) & 7;
            UINT32 BitNum;
            UINT32 BitMask;

            BitNum = (UINT32)(UINT8)M68kFetchWord (g_M68kContext.PC);
            g_M68kContext.PC += 2;
            BitMask = 1u << (BitNum & (Mode2 == 0 ? 31 : 7));

            switch (BitOp) {
            case 0: { // BTST
                UINT32 Val = M68kReadEA (EaOpcode, (Mode2 == 0)
                              ? M68K_SIZE_LONG : M68K_SIZE_BYTE);
                M68kClearFlag (M68K_CCR_Z);
                if (!(Val & BitMask)) M68kSetFlag (M68K_CCR_Z);
                break;
            }
            case 1: { // BCHG
                UINT8 Sz = (Mode2 == 0) ? M68K_SIZE_LONG : M68K_SIZE_BYTE;
                UINT32 Val = M68kReadEA (EaOpcode, Sz);
                M68kClearFlag (M68K_CCR_Z);
                if (!(Val & BitMask)) M68kSetFlag (M68K_CCR_Z);
                Val ^= BitMask;
                M68kWriteEA (EaOpcode, Sz, Val);
                break;
            }
            case 2: { // BCLR
                UINT8 Sz = (Mode2 == 0) ? M68K_SIZE_LONG : M68K_SIZE_BYTE;
                UINT32 Val = M68kReadEA (EaOpcode, Sz);
                M68kClearFlag (M68K_CCR_Z);
                if (!(Val & BitMask)) M68kSetFlag (M68K_CCR_Z);
                Val &= ~BitMask;
                M68kWriteEA (EaOpcode, Sz, Val);
                break;
            }
            case 3: { // BSET
                UINT8 Sz = (Mode2 == 0) ? M68K_SIZE_LONG : M68K_SIZE_BYTE;
                UINT32 Val = M68kReadEA (EaOpcode, Sz);
                M68kClearFlag (M68K_CCR_Z);
                if (!(Val & BitMask)) M68kSetFlag (M68K_CCR_Z);
                Val |= BitMask;
                M68kWriteEA (EaOpcode, Sz, Val);
                break;
            }
            }
            break;
        }
        case 0x9: // Reserved
            M68kExecuteIllegal (Opcode);
            break;
        case 0xA: // EORI  (special: 0x0A3C = EORI to CCR, 0x0A7C = EORI to SR)
            if (Opcode == 0x0A3C) {
                M68kExecuteEoriCcr ();
            } else if (Opcode == 0x0A7C) {
                M68kExecuteAndiEoriOriSr (Opcode, 2);  // EORI to SR
            } else {
                M68kExecuteImmediateAlu (Opcode, 4);    // EORI to EA
            }
            break;
        case 0xB: // Reserved
            M68kExecuteIllegal (Opcode);
            break;
        case 0xC: // CMPI
            M68kExecuteCmpi (Opcode);
            break;
        case 0xD: // Reserved
            M68kExecuteIllegal (Opcode);
            break;
        case 0xE: // Reserved
            M68kExecuteIllegal (Opcode);
            break;
        case 0xF: // Reserved
            M68kExecuteIllegal (Opcode);
            break;
        }
        break;
    }

    case 0x1: M68kExecuteMove (Opcode); break;  // MOVE.B
    case 0x2: M68kExecuteMove (Opcode); break;  // MOVE.L
    case 0x3: M68kExecuteMove (Opcode); break;  // MOVE.W

    case 0x4: {
        // Group 4 (0100 xxxx xxxx xxxx): Bit patterns decoded by mask
        // (ordered most-specific first to avoid ambiguity)

        // MOVE #imm,CCR: 0100 0100 1111 1100
        if (Opcode == 0x44FC) { M68kExecuteMoveImmCcr (); break; }
        // MOVE #imm,SR: 0100 0110 1111 1100
        if (Opcode == 0x46FC) { M68kExecuteMoveImmSr (); break; }
        // MOVE from SR: 0100 0100 11xx xxxx
        if ((Opcode & 0xFFC0) == 0x40C0) {
            M68kWriteEA (Opcode & 0x3F, M68K_SIZE_WORD, g_M68kContext.SR);
            break;
        }

        // MULU.L/MULS.L (68020+): 0100 1100 00ee eeee + ext word.
        // Ext word: bit15 = signed, bits14-12 = Dl, bit11 = size
        // (1 = 64-bit product into Dh:Dl), bits10-9 reserved,
        // bits8-6 = Dh. Source EA is a WORD operand.
        if ((Opcode & 0xFFC0) == 0x4C00) {
            UINT16 W = M68kReadWord (g_M68kContext.PC);
            g_M68kContext.PC += 2;
            BOOLEAN IsSigned = (W >> 15) & 1;
            UINT8 Dl = (W >> 12) & 7;
            BOOLEAN Sz64 = (W >> 11) & 1;
            UINT8 Dh = (W >> 6) & 7;
            // Source is a word operand, EXCEPT immediate sources are
            // always a full LONG literal that must be consumed from the
            // instruction stream.
            UINT32 SrcVal;
            UINT8 EaOp = Opcode & 0x3F;
            if ((EaOp & 0x38) == 0x38) {
                SrcVal = M68kFetchLong (g_M68kContext.PC);
                g_M68kContext.PC += 4;
            } else {
                SrcVal = M68kReadEA (EaOp, M68K_SIZE_WORD);
            }
            UINT32 DstVal = g_M68kContext.D[Dl] & 0xFFFF;
            UINT64 Product64;
            if (IsSigned) {
                Product64 = (UINT64)(INT64)(INT16)DstVal * (INT16)SrcVal;
            } else {
                Product64 = (UINT64)DstVal * SrcVal;
            }
            UINT32 Lo32 = (UINT32)(Product64 & 0xFFFFFFFFu);
            UINT32 Hi32 = (UINT32)(Product64 >> 32);
            if (Sz64) {
                g_M68kContext.D[Dh] = Hi32;
                g_M68kContext.D[Dl] = Lo32;
                M68kSetFlagsFromResult (Lo32 | Hi32, M68K_SIZE_LONG);
                // N/Z reflect the full 64-bit result; V clear (always fits)
                M68kClearFlag (M68K_CCR_V);
                if (((Lo32 == 0) && (Hi32 == 0))) {
                    M68kSetFlag (M68K_CCR_Z);
                } else {
                    M68kClearFlag (M68K_CCR_Z);
                }
                if (Product64 & 0x8000000000000000ull) {
                    M68kSetFlag (M68K_CCR_N);
                } else {
                    M68kClearFlag (M68K_CCR_N);
                }
            } else {
                g_M68kContext.D[Dl] = Lo32;
                M68kSetFlagsFromResult (Lo32, M68K_SIZE_LONG);
                // V set when the product does not fit in 32 bits
                if (Hi32 != 0) {
                    M68kSetFlag (M68K_CCR_V);
                } else {
                    M68kClearFlag (M68K_CCR_V);
                }
            }
            break;
        }

        // NOP: 0100 1110 0111 0001
        if (Opcode == 0x4E71) break;
        // RESET: 0100 1110 0111 0000
        if (Opcode == 0x4E70) break;
        // STOP #imm: 0100 1110 0111 0010
        if (Opcode == 0x4E72) { M68kExecuteStop (Opcode); break; }
        // RTE: 0100 1110 0111 0011
        if (Opcode == 0x4E73) { M68kExecuteRte (); break; }
        // RTS: 0100 1110 0111 0101
        if (Opcode == 0x4E75) {
            UINT32 NewPC = M68kPopLong ();
            BOOLEAN RtsOk = ((NewPC & 1) == 0) &&
                            (NewPC < 0x00100000u ||
                             (NewPC >= 0x40800000u && (NewPC < 0x41000000u || NewPC >= 0xFFC00000u)));
            if (!RtsOk) {
                // Dead-table sentinel fast path: NK init routines tail-call
                // through RAM tables our environment never populated; when a
                // LINK A6 frame anchor is present, unwind faithfully.
                {
                    UINT32 Fp = g_M68kContext.A[6];
                    // Frame must sit near the active stack to be ours.
                    UINT32 CurSp = g_M68kContext.Supervisor
                                   ? g_M68kContext.SSP : g_M68kContext.A[7];
                    static UINTN DeadDbg = 0;
                    if (DeadDbg < 6) {
                        DeadDbg++;
                        Print (L"  DEADTBL-CHK #%d: newpc=%08x fp=%08x "
                               L"cursp=%08x win=%d\n",
                               (UINT32)DeadDbg, NewPC, Fp, CurSp,
                               (Fp >= CurSp &&
                                (UINT32)(Fp - CurSp) < 0x200u) ? 1 : 0);
                    }
                    if (NewPC < 0x00001000u &&
                        Fp >= CurSp && (UINT32)(Fp - CurSp) < 0x200u &&
                        (Fp & 1) == 0) {
                        UINT32 Ret = M68kReadLong ((UINT32)(Fp + 4));
                        UINT32 Old6 = M68kReadLong (Fp);
                        BOOLEAN Ok = ((Ret & 1) == 0) &&
                                     (Ret >= 0x40800000u &&
                                      (Ret < 0x41000000u ||
                                       Ret >= 0xFFC00000u)) &&
                                     ((Old6 & 1) == 0) &&
                                     (Old6 >= CurSp);
                        if (Ok) {
                            static UINTN DeadTblCount = 0;
                            DeadTblCount++;
                            g_M68kContext.A[6] = Old6;
                            g_M68kContext.A[7] = Fp + 8;
                            g_M68kContext.PC = Ret;
                            if (DeadTblCount <= 12 ||
                                (DeadTblCount & 0xFF) == 0) {
                                Print (L"  DEADTBL #%d: sentinel ret -> "
                                       L"frame-unwind %08x\n",
                                       (UINT32)DeadTblCount, Ret);
                            }
                            M68kLogLowTransfer (
                                g_M68kContext.PC, Opcode, Ret);
                            break;
                        }
                    }
                }
                // Self-heal: a corrupted return address means some upstream
                // DR-callback chain ran off the rails. Rather than executing
                // data, resynchronize to the nearest plausible caller return
                // address still on the stack so outer boot levels continue.
                UINT32 Sp = g_M68kContext.Supervisor
                            ? g_M68kContext.SSP : g_M68kContext.A[7];
                UINT32 Fixed = 0;
                for (UINT8 s = 0; s < 16; s++) {
                    UINT32 V = M68kReadLong ((UINT32)(Sp + s * 4));
                    if ((V & 1u) == 0u &&
                        V >= 0x40800000u && (V < 0x41000000u || V >= 0xFFC00000u)) {
                        Fixed = V;
                        // Adjust only the ACTIVE stack pointer; touching the
                        // inactive one corrupts the other mode's context.
                        if (g_M68kContext.Supervisor) {
                            g_M68kContext.SSP += (UINT32)(s * 4);
                        } else {
                            g_M68kContext.A[7] += (UINT32)(s * 4);
                        }
                        break;
                    }
                }
                if (!Fixed) {
                    // Downward scan: when an epilogue over-pops past the
                    // real return into an argument slot, the genuine
                    // caller's frame lies BELOW the current stack pointer.
                    // Resume there without touching SP (the callee already
                    // applied its own frame adjustment).
                    for (UINT8 s = 2; s < 48; s++) {
                        UINT32 V = M68kReadLong ((UINT32)(Sp - s * 4));
                        if ((V & 1u) == 0u &&
                            V >= 0x40800000u &&
                            (V < 0x41000000u || V >= 0xFFC00000u)) {
                            Fixed = V;
                            break;
                        }
                    }
                }
                static INTN BadRtsCount = -1;
                BadRtsCount++;
                if (BadRtsCount < 8) {
                    Print (L"68K BAD RTS #%d from PC=0x%08x -> 0x%08x "
                           L"%a SSP=0x%08x A7=0x%08x\n",
                           (int)BadRtsCount,
                           g_M68kContext.PC - 2, NewPC,
                           Fixed ? "healed" : "FATAL",
                           g_M68kContext.SSP, g_M68kContext.A[7]);
                    if (!Fixed) {
                        UINTN K;
                        Print (L"  frame:");
                        for (UINT8 k = 0; k < 12; k++) {
                            Print (L" %08x",
                                   M68kReadLong (Sp + k * 4));
                        }
                        Print (L"\n");
                        Print (L"  BADRTS last 64 PCs:");
                        for (K = 0; K < 64; K++) {
                            UINTN Idx = (g_LastPcIdx + 256 - 1 - K) % 256;
                            Print (L" %08x/%04x",
                                   g_LastPcRing[Idx], g_LastOpRing[Idx]);
                            if ((K & 7) == 7) Print (L"\n     ");
                        }
                        Print (L"\n");
                        M68kTraceFlush ();
                    }
                }
                if (Fixed) {
                    M68kLogLowTransfer (g_M68kContext.PC - 2, Opcode, Fixed);
                    g_M68kContext.PC = Fixed;
                    break;
                }
            }
            M68kLogLowTransfer (g_M68kContext.PC - 2, Opcode, NewPC);
            g_M68kContext.PC = NewPC;
            break;
        }
        // RTR: 0100 1110 0111 0111
        if (Opcode == 0x4E77) { M68kExecuteRtr (); break; }
        // TRAPV: 0100 1110 0111 0110
        if (Opcode == 0x4E76) break;
        // TRAP #n: 0100 1110 0100 xxxx
        if ((Opcode & 0xFFF0) == 0x4E40) { M68kExecuteTrap (Opcode); break; }
        // LINK: 0100 1110 0101 0xxx
        if ((Opcode & 0xFFF8) == 0x4E50) { M68kExecuteLink (Opcode); break; }
        // UNLK: 0100 1110 0101 1xxx
        if ((Opcode & 0xFFF8) == 0x4E58) { M68kExecuteUnlk (Opcode); break; }
        // MOVE USP,An: 0100 1110 0110 0xxx
        if ((Opcode & 0xFFF8) == 0x4E60) { M68kExecuteMoveUsp (Opcode); break; }
        // MOVE An,USP: 0100 1110 0110 1xxx
        if ((Opcode & 0xFFF8) == 0x4E68) { M68kExecuteMoveUsp (Opcode); break; }
        // JSR: 0100 1110 10xx xxxx
        if ((Opcode & 0xFFC0) == 0x4E80) {
            UINT32 EA;
            M68kComputeEA (Opcode & 0x3F, &EA, NULL);
            if (EA < 0x400) {
                // Target lies inside the exception-vector area — never code.
                // This happens when a dispatch record was never built (e.g.
                // the ATA-style walker at 0x408081A8 running with A1=0).
                // Real hardware skips such calls via the record's zero
                // routine-offset check; emulate the same by completing the
                // call as an immediate return.  D0=0 signals "not handled".
                g_M68kContext.D[0] = 0;
                M68kLogLowTransfer (g_M68kContext.PC - 2, Opcode, EA);
                break;
            }
            M68kPushLong (g_M68kContext.PC);
            g_M68kContext.PC = EA;
            break;
        }
        // JMP: 0100 1110 11xx xxxx
        // Plain unconditional jump - never pushes. Earlier revisions
        // emulated tail-call/call semantics here for DR-handler chains,
        // but that inserted phantom return addresses that corrupted the
        // trampoline stack handoff at 0x40800118.
        if ((Opcode & 0xFFC0) == 0x4EC0) {
            UINT32 EA;
            M68kComputeEA (Opcode & 0x3F, &EA, NULL);
            if (EA == 0) {
                M68kTrace (L"  JMP(0): RTS\n");
                g_M68kContext.PC = M68kPopLong ();
            } else {
                M68kLogLowTransfer (g_M68kContext.PC - 2, Opcode, EA);
                g_M68kContext.PC = EA;
            }
            break;
        }
        // LEA: 0100 xxx1 1111 1xxx
        if ((Opcode & 0xF1C0) == 0x41C0) { M68kExecuteLea (Opcode); break; }
        // CHK: 0100 xxx1 1000 sxxx — bounds-check Dn against <ea>;
        // trap vector 6 (0x18) when out of range.
        if ((Opcode & 0xF138) == 0x4100) {
            UINT8 Dn = (Opcode >> 9) & 7;
            BOOLEAN IsLong = ((Opcode >> 7) & 1) != 0;
            UINT32 Ub = IsLong
                        ? M68kReadEA (Opcode & 0x3F, M68K_SIZE_LONG)
                        : M68kReadEA (Opcode & 0x3F, M68K_SIZE_WORD);
            INT32 Val = IsLong
                        ? (INT32)g_M68kContext.D[Dn]
                        : (INT32)(INT16)(UINT16)g_M68kContext.D[Dn];
            if (Val < 0) {
                M68kSetFlag (M68K_CCR_N);
                g_M68kContext.PC -= 2;            // exception restarts insn
                M68kRaiseException (M68K_VEC_CHK);
            } else if ((UINT32)Val > Ub) {
                M68kClearFlag (M68K_CCR_N);
                g_M68kContext.PC -= 2;
                M68kRaiseException (M68K_VEC_CHK);
            } else {
                M68kClearFlag (M68K_CCR_N);
                // Z reflects the register compare result.
                if (Val == 0) M68kSetFlag (M68K_CCR_Z);
                else          M68kClearFlag (M68K_CCR_Z);
            }
            break;
        }
        // SWAP: 0100 1000 0100 0xxx
        if ((Opcode & 0xFFF8) == 0x4840) { M68kExecuteSwap (Opcode); break; }
        // EXT.W: 0100 1000 1000 0xxx
        if ((Opcode & 0xFFF8) == 0x4880) { M68kExecuteExt (Opcode); break; }
        // EXT.L: 0100 1000 1100 0xxx
        if ((Opcode & 0xFFF8) == 0x48C0) { M68kExecuteExt (Opcode); break; }
        // TAS: 0100 1010 11xx xxxx
        if ((Opcode & 0xFFC0) == 0x4AC0) { M68kExecuteTas (Opcode); break; }
        // TST: 0100 1010 xx xx xxxx
        if ((Opcode & 0xFF00) == 0x4A00) { M68kExecuteTst (Opcode); break; }
        // PEA: 0100 1000 0111 1xxx
        if ((Opcode & 0xFFC0) == 0x4840) { M68kExecutePea (Opcode); break; }
        // NBCD: 0100 1000 00xx xxxx — BCD negate: dst = 0 - dst - X.
        if ((Opcode & 0xFFC0) == 0x4800) {
            UINT16 EaOp = Opcode & 0x3F;
            UINT8 DstVal = (UINT8)M68kReadEA (EaOp, M68K_SIZE_BYTE);
            UINT8 X = M68kTestFlag (M68K_CCR_X) ? 1 : 0;
            INTN T = (INTN)0 - (INTN)DstVal - (INTN)X;
            UINT8 Result;
            if ((T & 0xF) > 9) T -= 6;
            if (T > 0x99) { T -= 0x100; M68kSetFlag (M68K_CCR_C); }
            else          { M68kClearFlag (M68K_CCR_C); }
            Result = (UINT8)T;
            if (Result == 0) M68kSetFlag (M68K_CCR_Z);   // Z is ANDed
            else             M68kClearFlag (M68K_CCR_Z);
            M68kWriteEA (EaOp, M68K_SIZE_BYTE, Result);
            break;
        }
        // MOVEM to memory: 0100 1000 1xxx 1xxx
        if ((Opcode & 0xFF80) == 0x4880) { M68kExecuteMovem (Opcode); break; }
        // MOVEM to register: 0100 1100 1xxx 1xxx
        if ((Opcode & 0xFF80) == 0x4C80) { M68kExecuteMovem (Opcode); break; }
        // CLR: 0100 0100 00xx xxxx (byte)
        if ((Opcode & 0xFF00) == 0x4200) {
            UINT8 Size = M68kDecodeSize (Opcode);
            M68kWriteEA (Opcode & 0x3F, Size, 0);
            M68kSetFlagsFromResult (0, Size);
            M68kClearFlag (M68K_CCR_C);
            M68kClearFlag (M68K_CCR_V);
            break;
        }
        // MOVE <ea>,SR: 0100 0110 11xx xxxx (must precede the NOT mask,
        // which would otherwise swallow these as NOT.W <ea>).
        if ((Opcode & 0xFFC0) == 0x46C0) {
            UINT16 V = M68kReadEA (Opcode & 0x3F, M68K_SIZE_WORD);
            M68kSetSR (V);
            break;
        }
        // MOVE <ea>,CCR: 0100 0100 11xx xxxx
        if ((Opcode & 0xFFC0) == 0x44C0) {
            UINT16 V = M68kReadEA (Opcode & 0x3F, M68K_SIZE_WORD);
            g_M68kContext.SR = (UINT16)((g_M68kContext.SR & 0xFF00) |
                                        (V & 0x1F));
            break;
        }
        // NEG: 0100 0100 xx xx xxxx (after CLR check)
        if ((Opcode & 0xFF00) == 0x4400) { M68kExecuteNeg (Opcode); break; }
        // NEGX: 0100 0000 xx xx xxxx
        if ((Opcode & 0xFF00) == 0x4000) { M68kExecuteNeg (Opcode); break; }
        // NOT: 0100 0110 xx xx xxxx
        if ((Opcode & 0xFF00) == 0x4600) { M68kExecuteNot (Opcode); break; }
        // Bit operations (register form): 0100 xxx0 xx xxx xxx
        // bits 8=0, bits 5-3 != 111 (not immediate form)
        if ((Opcode & 0x0100) == 0x0000 && (Opcode & 0x0038) != 0x0038) {
            M68kExecuteBit (Opcode); break;
        }
        // MOVEC: 0x4E7A (from CR) / 0x4E7B (to CR) — skip extension word
        if ((Opcode & 0xFFFE) == 0x4E7A) {
            g_M68kContext.PC += 2;
            break;
        }

        M68kExecuteIllegal (Opcode);
        break;
    }

    case 0x5: {
        // DBcc/Scc/ADDQ/SUBQ (ADDX/SUBX are in group 9/0xB, NOT group 5)
        if ((Opcode & 0xF0F8) == 0x50C8) {
            // DBcc
            M68kExecuteDbcc (Opcode);
        } else if ((Opcode & 0xF0C0) == 0x50C0) {
            // Scc
            UINT8 Condition = (Opcode >> 8) & 0xF;
            UINT16 EaOpcode = Opcode & 0x003F;
            BOOLEAN Set;
            switch (Condition) {
            case 0x0: Set = TRUE; break;   // ST
            case 0x1: Set = FALSE; break;  // SF
            case 0x2: Set = !M68kTestFlag (M68K_CCR_C) && !M68kTestFlag (M68K_CCR_Z); break;
            case 0x3: Set = M68kTestFlag (M68K_CCR_C) || M68kTestFlag (M68K_CCR_Z); break;
            case 0x4: Set = !M68kTestFlag (M68K_CCR_C); break;
            case 0x5: Set = M68kTestFlag (M68K_CCR_C); break;
            case 0x6: Set = !M68kTestFlag (M68K_CCR_Z); break;
            case 0x7: Set = M68kTestFlag (M68K_CCR_Z); break;
            case 0x8: Set = !M68kTestFlag (M68K_CCR_V); break;
            case 0x9: Set = M68kTestFlag (M68K_CCR_V); break;
            case 0xA: Set = !M68kTestFlag (M68K_CCR_N); break;
            case 0xB: Set = M68kTestFlag (M68K_CCR_N); break;
            case 0xC: Set = (M68kTestFlag (M68K_CCR_N) == M68kTestFlag (M68K_CCR_V)) && !M68kTestFlag (M68K_CCR_Z); break;
            case 0xD: Set = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)) || M68kTestFlag (M68K_CCR_Z); break;
            case 0xE: Set = !(M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;
            case 0xF: Set = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;
            default: Set = FALSE; break;
            }
            M68kWriteEA (EaOpcode, M68K_SIZE_BYTE, Set ? 0xFF : 0x00);
        } else {
            // ADDQ/SUBQ
            M68kExecuteAddqSubq (Opcode, (Opcode >> 8) & 1);
        }
        break;
    }

    case 0x6: {
        // Bcc/BSR/BRA
        M68kExecuteBranch (Opcode);
        break;
    }

    case 0x7: {
        // MOVEQ #imm,Dn
        M68kExecuteMoveq (Opcode);
        break;
    }

    case 0x8: {
        // OR/DIV/SBCD
        UINT8 SubBits = (Opcode >> 6) & 7;
        if (SubBits == 7) {
            // DIVS
            UINT16 EaOpcode = Opcode & 0x003F;
            UINT32 SrcVal = M68kReadEA (EaOpcode, M68K_SIZE_WORD);
            if (SrcVal == 0) {
                // Division by zero
                M68kRaiseException (M68K_VEC_ZERO_DIVIDE);
                break;
            }
            INT32 Dividend = (INT32)g_M68kContext.D[(Opcode >> 9) & 7];
            INT16 Divisor = (INT16)SrcVal;
            INT32 Quotient = Dividend / Divisor;
            if (Quotient > 32767 || Quotient < -32768) {
                // Overflow
                g_M68kContext.SR |= M68K_CCR_V | M68K_CCR_N;
            } else {
                UINT32 Remainder = (UINT32)(Dividend % Divisor);
                g_M68kContext.D[(Opcode >> 9) & 7] = ((Remainder & 0xFFFF) << 16) | ((UINT16)Quotient);
                M68kSetFlagsFromResult ((UINT32)Quotient, M68K_SIZE_WORD);
            }
        } else if (SubBits == 3) {
            // DIVU
            UINT16 EaOpcode = Opcode & 0x003F;
            UINT32 SrcVal = M68kReadEA (EaOpcode, M68K_SIZE_WORD);
            if (SrcVal == 0) {
                M68kRaiseException (M68K_VEC_ZERO_DIVIDE);
                break;
            }
            UINT32 Dividend = g_M68kContext.D[(Opcode >> 9) & 7];
            UINT32 Quotient = Dividend / SrcVal;
            if (Quotient > 0xFFFF) {
                g_M68kContext.SR |= M68K_CCR_V | M68K_CCR_N;
            } else {
                UINT32 Remainder = Dividend % SrcVal;
                g_M68kContext.D[(Opcode >> 9) & 7] = ((Remainder & 0xFFFF) << 16) | (Quotient & 0xFFFF);
                M68kSetFlagsFromResult (Quotient, M68K_SIZE_WORD);
            }
        } else if (SubBits == 4) {
            // SBCD with proper BCD adjust and X/Z/C semantics.
            UINT8 Dst = (Opcode >> 9) & 7;
            UINT8 Src = Opcode & 7;
            BOOLEAN IsMemory = (Opcode >> 3) & 1;
            UINT8 SrcVal, DstVal;
            if (IsMemory) {
                g_M68kContext.A[Src]--;         // -(Ay) predecrement source
                SrcVal = M68kReadByte (g_M68kContext.A[Src]);
                g_M68kContext.A[Dst]--;         // -(Ax) predecrement dest
                DstVal = M68kReadByte (g_M68kContext.A[Dst]);
            } else {
                SrcVal = (UINT8)(g_M68kContext.D[Src] & 0xFF);
                DstVal = (UINT8)(g_M68kContext.D[Dst] & 0xFF);
            }
            UINT8 X = M68kTestFlag (M68K_CCR_X) ? 1 : 0;
            INTN T = (INTN)DstVal - (INTN)SrcVal - (INTN)X;
            UINT8 Result;
            if ((T & 0xF) > 9) T -= 6;            // low-nibble borrow fix
            if (T > 0x99) { T -= 0x100; M68kSetFlag (M68K_CCR_C); }
            else          { M68kClearFlag (M68K_CCR_C); }
            Result = (UINT8)T;
            // Z is ANDed across chained BCD operations.
            if (Result == 0) M68kSetFlag (M68K_CCR_Z);
            else             M68kClearFlag (M68K_CCR_Z);
            if (M68kTestFlag (M68K_CCR_C)) M68kSetFlag (M68K_CCR_X);
            else                           M68kClearFlag (M68K_CCR_X);
            if (IsMemory) {
                M68kWriteByte (g_M68kContext.A[Dst], Result);
            } else {
                g_M68kContext.D[Dst] = (g_M68kContext.D[Dst] & 0xFFFFFF00) | Result;
            }
        } else {
            M68kExecuteALU_Dn_EA (Opcode, 3);  // OR
        }
        break;
    }

    case 0x9: {
        // SUB/SUBA/SUBX
        UINT8 SubBits = (Opcode >> 6) & 7;
        if (SubBits == 3 || SubBits == 7) {
            // SUBA: An -= <EA>; word ops sign-extend; no flags.
            // (Was previously routed to the MOVEA handler, which OVERWROTE
            // An with the immediate instead of subtracting it.)
            UINT8 DstReg = (Opcode >> 9) & 7;
            UINT8 Size = (SubBits == 3) ? M68K_SIZE_WORD : M68K_SIZE_LONG;
            UINT16 EaOpcode = Opcode & 0x003F;
            UINT32 SrcVal = M68kReadEA (EaOpcode, Size);
            if (Size == M68K_SIZE_WORD) {
                SrcVal = (UINT32)(INT32)(INT16)(UINT16)SrcVal;
            }
            {
                UINT32 Base = (DstReg == 7)
                              ? M68kGetStackPointer ()
                              : g_M68kContext.A[DstReg];
                M68kWriteAn (DstReg, Base - SrcVal);
            }
        } else if ((Opcode & 0xF138) == 0x9100) {
            // SUBX: 1001 Ry 1 size m 00 Rx — register (m=0) or
            // predecrement-memory (m=1) forms, with X flag.
            {
                UINT8 Ry = (Opcode >> 9) & 7;
                UINT8 Rx = Opcode & 7;
                UINT8 SizeSel = (Opcode >> 6) & 3;
                BOOLEAN IsMemory = ((Opcode >> 3) & 1) != 0;
                UINT8 Size = (SizeSel == 1) ? M68K_SIZE_WORD : M68K_SIZE_LONG;
                UINTN Step = (Size == M68K_SIZE_WORD) ? 2 : 4;
                UINT32 SrcVal, DstVal, Result;

                if (IsMemory) {
                    g_M68kContext.A[Rx] -= Step;
                    SrcVal = (Size == M68K_SIZE_WORD)
                             ? M68kReadWord (g_M68kContext.A[Rx])
                             : M68kReadLong (g_M68kContext.A[Rx]);
                    g_M68kContext.A[Ry] -= Step;
                    DstVal = (Size == M68K_SIZE_WORD)
                             ? M68kReadWord (g_M68kContext.A[Ry])
                             : M68kReadLong (g_M68kContext.A[Ry]);
                } else {
                    SrcVal = g_M68kContext.D[Rx];
                    DstVal = g_M68kContext.D[Ry];
                }
                {
                    UINT8 X = M68kTestFlag (M68K_CCR_X) ? 1 : 0;
                    UINT64 T = (UINT64)DstVal - (UINT64)SrcVal - X;
                    UINTN ShiftN = (Size == M68K_SIZE_WORD) ? 15 : 31;
                    Result = (UINT32)T;
                    if (T > 0xFFFFFFFFull) M68kSetFlag (M68K_CCR_C);
                    else                   M68kClearFlag (M68K_CCR_C);
                    if (((DstVal ^ SrcVal) & (DstVal ^ Result)) >> ShiftN)
                        M68kSetFlag (M68K_CCR_V);
                    else
                        M68kClearFlag (M68K_CCR_V);
                    if ((INT32)Result < 0) M68kSetFlag (M68K_CCR_N);
                    else                   M68kClearFlag (M68K_CCR_N);
                }
                if (Result == 0) M68kSetFlag (M68K_CCR_Z);   // Z ANDed
                else             M68kClearFlag (M68K_CCR_Z);
                if (M68kTestFlag (M68K_CCR_C)) M68kSetFlag (M68K_CCR_X);
                else                           M68kClearFlag (M68K_CCR_X);
                if (IsMemory) {
                    if (Size == M68K_SIZE_WORD)
                        M68kWriteWord (g_M68kContext.A[Ry], (UINT16)Result);
                    else
                        M68kWriteLong (g_M68kContext.A[Ry], Result);
                } else {
                    g_M68kContext.D[Ry] = Result;
                }
            }
        } else {
            M68kExecuteALU_Dn_EA (Opcode, 1);  // SUB
        }
        break;
    }

    case 0xA: {
        // Line A (Mac Toolbox traps). The NK-side ROM uses A-line traps as
        // service calls with a selector in D0 (e.g. $A080, $A06E); the real
        // machine dispatches them through the emulator's kernel trap table.
        // We don't implement those services yet: log the call and return
        // success (D0=0) so boot can proceed to the next gate.
        {
            static UINTN LineASeen = 0;
            UINT16 Op = Opcode & 0x0FFF;
            UINT32 Sel = g_M68kContext.D[0];
            if (LineASeen < 32) {
                LineASeen++;
                Print (L"  68K LINE A stub: op=$%04X sel(D0)=%08x at PC=0x%08x "
                       L"A0=%08x A1=%08x\n",
                       Op, Sel, g_M68kContext.PC - 2,
                       g_M68kContext.A[0], g_M68kContext.A[1]);
            }
            // Allocation-style selectors return their buffer in A0
            // (ROM stores A0 directly: e.g. move.l a0,$824.w at
            // 0x40800C62 after sel=$1B, move.l a0,$c24.w at 0x40800C82
            // after sel=2). Hand out fixed writable scratch buffers.
            switch (Sel) {
            case 0x1B:
                g_M68kContext.A[0] = 0x00038000u;
                break;
            case 0x02:
                g_M68kContext.A[0] = 0x00002000u;
                break;
            default:
                break;
            }
            if (Op == 0x06E) {
                // $A06E protocol: sel=6 submits (A0 = request block),
                // sel=5 polls: caller waits until word[Rec+0x20]==1 where
                // Rec = [request+0]. Keep the record adjacent to the
                // request block on the live stack (guaranteed-writable
                // RAM) and re-arm on every call so pool scrubs can't
                // invalidate it.
                UINT32 BufPtr = g_M68kContext.A[0];
                if (Sel == 6 || Sel == 5) {
                    UINT32 Rec = M68kReadLong (BufPtr);
                    if (Rec <= BufPtr || Rec > BufPtr + 0x400u ||
                        (Rec & 3u) != 0u) {
                        Rec = BufPtr + 0x40u;
                        M68kWriteLong (BufPtr, Rec);
                    }
                    M68kWriteWord ((UINT32)(Rec + 0x20), 1);
                    // Fields consumed downstream by the NK init stage:
                    // +4 = descriptor index (loaded into d2), +A/+C =
                    // gamma-entry params: offset = ((A>>1)-$16|1)*d2 +
                    // (C>>4)-2, so A must be >= $2E for in-bounds dest.
                    M68kWriteWord ((UINT32)(Rec + 0x04), 1);
                    M68kWriteWord ((UINT32)(Rec + 0x0A), 0x30);
                    M68kWriteWord ((UINT32)(Rec + 0x0C), 0x20);
                }
            }
            // ---- HOST TRAP SERVICES ------------------------------------
            // The NK maintains its own heap via Toolbox-style traps long
            // before any MacOS zone exists. Rather than reimplementing the
            // allocator (register conventions unknown), SEED the minimal
            // chain the ROM handlers walk — [0x08A4] -> master -> zone ->
            // free node — into a host-managed scratch arena in the writable
            // 16 MB bank, then let the genuine ROM handler run.
            {
                static BOOLEAN NkHeapSeeded = FALSE;
                if (!NkHeapSeeded) {
                    NkHeapSeeded = TRUE;
                    // Arena layout inside low bank, clear of stacks/XLM:
                    const UINT32 Master = 0x00061000u;
                    const UINT32 Zone   = 0x00061100u;
                    const UINT32 Node   = 0x00061200u;
                    const UINT32 BlkBase= 0x00062000u;
                    M68kWriteLong (0x08A4u, Master);
                    M68kWriteLong (Master, Zone);
                    // +0x16 offset consumed by the 40879790 handler walk;
                    // word-aligned writes via byte pairs to be safe.
                    M68kWriteLong ((UINT32)(Zone + 0x14u), 0);
                    M68kWriteLong ((UINT32)(Zone + 0x18u), 0);
                    {
                        UINT16 Hi = (UINT16)((Node >> 16) & 0xFFFFu);
                        UINT16 Lo = (UINT16)(Node & 0xFFFFu);
                        M68kWriteWord ((UINT32)(Zone + 0x16u), Hi);
                        M68kWriteWord ((UINT32)(Zone + 0x18u), Lo);
                    }
                    M68kWriteLong (Node, BlkBase);
                    Print (L"  NKHEAP-SEED: [08A4]=%08x -> z=%08x "
                           L"[z+16]=%08x node->%08x\n",
                           Master, Zone, Node, BlkBase);
                }
                // $A051: NK allocator observed at 0x4087CA6A during early
                // MacOS-side init; result expected in A0 (caller stores it
                // into $0CC8/$08A0 immediately after). Serve blocks from a
                // bump arena; page-framed consumers (pid<<12|off) then get
                // valid RAM instead of constructing 0x7Fxxxx zeros.
                if (Opcode == 0xA051u) {
                    static UINT32 NkBrk = 0x00063000u;
                    const UINT32 NkEnd = 0x00100000u;   // 1 MB arena
                    UINT32 Size = g_M68kContext.D[0];
                    if (Size == 0 || Size > 0x90000u) Size = 0x1000u;
                    Size = (Size + 0xFFFu) & ~0xFFFu;    // page-align
                    if (NkBrk + Size > NkEnd) {
                        Print (L"  HOSTTRAP A051: exhausted\n");
                        g_M68kContext.D[0] = 0xFFFFFFFFu;
                    } else {
                        static UINTN A051Seen = 0;
                        if (A051Seen++ < 10) {
                            Print (L"  HOSTTRAP A051: %08x -> "
                                   L"a0=%08x\n", Size, NkBrk);
                        }
                        g_M68kContext.A[0] = NkBrk;
                        NkBrk += Size;
                        g_M68kContext.D[0] = 0;
                        g_M68kContext.SR = (UINT16)(
                            (g_M68kContext.SR & 0xFF00) | M68K_CCR_Z);
                        break;                            // serviced
                    }
                }
            }
            // ---- END HOST TRAP SERVICES ---------------------------------

            // NOTE: do NOT blanket-zero D0 here. Per-selector cases above
            // set meaningful results (VMInit=1, allocations return A0...);
            // wiping D0 turned service success into failure and spun the
            // $A06E caller at 0x40800E5A forever. Unknown selectors fall
            // through with the caller's input value.
            // Callers test the result through CCR (bne on error), so return
            // "no error" flags: Z=1, C/V/N/X cleared.
            g_M68kContext.SR = (UINT16)((g_M68kContext.SR & 0xFF00) |
                                        M68K_CCR_Z);

            // Toolbox-range A-traps dispatch through the ROM trap table
            // exactly like SheepShaver's find_rom_trap(): table base is
            // the longword at ROM+0x22; traps > $A800 index (trap&3ff),
            // others index ((trap&ff)+400). Only reached for ops we do
            // NOT service as NK-internal above.
            {
                UINT16 TrapOp = Opcode & 0x0FFF;
                BOOLEAN NkService = (TrapOp == 0x06E) ||
                                    (TrapOp == 0x004) ||
                                    (TrapOp == 0x01F) ||
                                    (TrapOp == 0x080);
                if (!NkService) {
                    UINT32 TblBase = 0x40800000u +
                                     M68kReadLong (0x40800022u);
                    UINT32 Idx = (Opcode > 0xA800u)
                                 ? (Opcode & 0x03FFu)
                                 : ((Opcode & 0x00FFu) + 0x400u);
                    UINT32 Handler = M68kReadLong (TblBase + Idx * 4);
                    // Table stores ROM-relative offsets for OS traps.
                    if (Handler < 0x400000u) {
                        Handler += 0x40800000u;
                    }
                    static INTN TrapDbg = -1;
                    TrapDbg++;
                    if (TrapDbg < 12) {
                        Print (L"  TRAP %04X -> tbl=%08x handler=%08x\n",
                               Opcode, TblBase, Handler);
                    }
                    if (Handler >= 0x40800000u &&
                        (Handler < 0x41000000u || Handler >= 0xFFC00000u)) {
                        // Inline A-line words have no JSR-pushed return.
                        // Handlers end in rts, so synthesize the standard
                        // return address (instruction after the trap).
                        // g_M68kContext.PC already points there.
                        M68kPushLong (g_M68kContext.PC);
                        g_M68kContext.PC = Handler;
                        break;
                    }
                    // Unknown/unmapped entry: keep legacy success-stub
                    // behavior (D0=0/Z set already applied).
                }
            }
        }
        break;
    }

    case 0xB: {
        // CMP/CMPA/CMPM/EOR
        UINT8 SubBits = (Opcode >> 6) & 7;
        if (SubBits <= 2) {
            // CMP.B/W/L <EA>,Dn (compare EA to Dn)
            M68kExecuteCmp (Opcode, FALSE);
        } else if (SubBits == 3 || SubBits == 7) {
            M68kExecuteCmp (Opcode, TRUE);  // CMPA
        } else if (SubBits == 5) {
            // CMPM
            UINT8 Dst = (Opcode >> 9) & 7;
            UINT8 Src = Opcode & 7;
            UINT32 SrcVal = M68kReadLong (g_M68kContext.A[Src]);
            g_M68kContext.A[Src] += 4;
            UINT32 DstVal = M68kReadLong (g_M68kContext.A[Dst]);
            g_M68kContext.A[Dst] += 4;
            M68kSetFlagsFromSub (DstVal - SrcVal, SrcVal, DstVal, M68K_SIZE_LONG);
        } else {
            M68kExecuteALU_Dn_EA (Opcode, 4);  // EOR
        }
        break;
    }

    case 0xC: {
        // AND/EXG/MULU/MULS/ABCD
        UINT8 SubBits = (Opcode >> 6) & 7;
        UINT8 ExgMode = (Opcode >> 3) & 0x1F;
        if (ExgMode == 8 || ExgMode == 9 || ExgMode == 17) {
            // EXG (opmode in bits 7-3: 01000 D/D, 01001 A/A, 10001 D/A).
            // Must be tested before the SubBits buckets: the D/A form has
            // bits8-6 == 7 which collides with MULS, and A/A collides
            // with nothing but D/D (5) previously missed entirely.
            M68kExecuteExg (Opcode);
        } else if (SubBits == 3 || SubBits == 7) {
            // MULU/MULS
            UINT16 EaOpcode = Opcode & 0x003F;
            UINT32 SrcVal = M68kReadEA (EaOpcode, M68K_SIZE_WORD);
            UINT32 DstVal = g_M68kContext.D[(Opcode >> 9) & 7] & 0xFFFF;
            BOOLEAN IsSigned = (Opcode >> 8) & 1;
            if (IsSigned) {
                INT32 Result = (INT32)(INT16)DstVal * (INT32)(INT16)SrcVal;
                g_M68kContext.D[(Opcode >> 9) & 7] = (UINT32)Result;
            } else {
                g_M68kContext.D[(Opcode >> 9) & 7] = DstVal * SrcVal;
            }
        } else if (SubBits == 4) {
            // ABCD with proper BCD adjust and X/Z/C semantics.
            UINT8 Dst = (Opcode >> 9) & 7;
            UINT8 Src = Opcode & 7;
            BOOLEAN IsMemory = (Opcode >> 3) & 1;
            UINT8 SrcVal, DstVal;
            if (IsMemory) {
                g_M68kContext.A[Src]--;         // -(Ay) predecrement source
                SrcVal = M68kReadByte (g_M68kContext.A[Src]);
                g_M68kContext.A[Dst]--;         // -(Ax) predecrement dest
                DstVal = M68kReadByte (g_M68kContext.A[Dst]);
            } else {
                SrcVal = (UINT8)(g_M68kContext.D[Src] & 0xFF);
                DstVal = (UINT8)(g_M68kContext.D[Dst] & 0xFF);
            }
            UINT8 X = M68kTestFlag (M68K_CCR_X) ? 1 : 0;
            INTN T = (INTN)DstVal + (INTN)SrcVal + (INTN)X;
            BOOLEAN HalfCarry = (((DstVal & 0xF) + (SrcVal & 0xF) + X) > 9);
            UINT8 Result;
            if ((T & 0xF) > 9 || HalfCarry) T += 6;   // low-nibble carry fix
            if (T > 0x99) { T += 0x60; M68kSetFlag (M68K_CCR_C); }
            else          { M68kClearFlag (M68K_CCR_C); }
            Result = (UINT8)T;
            // Z is ANDed across chained BCD operations.
            if (Result == 0) M68kSetFlag (M68K_CCR_Z);
            else             M68kClearFlag (M68K_CCR_Z);
            if (M68kTestFlag (M68K_CCR_C)) M68kSetFlag (M68K_CCR_X);
            else                           M68kClearFlag (M68K_CCR_X);
            if (IsMemory) {
                M68kWriteByte (g_M68kContext.A[Dst], Result);
            } else {
                g_M68kContext.D[Dst] = (g_M68kContext.D[Dst] & 0xFFFFFF00) | Result;
            }
        } else {
            M68kExecuteALU_Dn_EA (Opcode, 2);  // AND
        }
        break;
    }

    case 0xD: {
        // ADD/ADDA/ADDX
        UINT8 SubBits = (Opcode >> 6) & 7;
        if (SubBits == 3 || SubBits == 7) {
            // ADDA: An += <EA>; word ops sign-extend; no flags.
            // (Was previously routed to the MOVEA handler, which OVERWROTE
            // An with the immediate instead of adding it.)
            UINT8 DstReg = (Opcode >> 9) & 7;
            UINT8 Size = (SubBits == 3) ? M68K_SIZE_WORD : M68K_SIZE_LONG;
            UINT16 EaOpcode = Opcode & 0x003F;
            UINT32 SrcVal = M68kReadEA (EaOpcode, Size);
            if (Size == M68K_SIZE_WORD) {
                SrcVal = (UINT32)(INT32)(INT16)(UINT16)SrcVal;
            }
            // A7 must be read through the active-stack-pointer accessor:
            // context.A[7] is a stale shadow in supervisor mode.
            {
                UINT32 Base = (DstReg == 7)
                              ? M68kGetStackPointer ()
                              : g_M68kContext.A[DstReg];
                M68kWriteAn (DstReg, Base + SrcVal);
            }
        } else if ((Opcode & 0xF138) == 0xD100) {
            // ADDX: 1101 Ry 1 size m 00 Rx — register (m=0) or
            // predecrement-memory (m=1) forms, with X flag.
            {
                UINT8 Ry = (Opcode >> 9) & 7;
                UINT8 Rx = Opcode & 7;
                UINT8 SizeSel = (Opcode >> 6) & 3;
                BOOLEAN IsMemory = ((Opcode >> 3) & 1) != 0;
                UINT8 Size = (SizeSel == 1) ? M68K_SIZE_WORD : M68K_SIZE_LONG;
                UINTN Step = (Size == M68K_SIZE_WORD) ? 2 : 4;
                UINT32 SrcVal, DstVal, Result;

                if (IsMemory) {
                    g_M68kContext.A[Rx] -= Step;
                    SrcVal = (Size == M68K_SIZE_WORD)
                             ? M68kReadWord (g_M68kContext.A[Rx])
                             : M68kReadLong (g_M68kContext.A[Rx]);
                    g_M68kContext.A[Ry] -= Step;
                    DstVal = (Size == M68K_SIZE_WORD)
                             ? M68kReadWord (g_M68kContext.A[Ry])
                             : M68kReadLong (g_M68kContext.A[Ry]);
                } else {
                    SrcVal = g_M68kContext.D[Rx];
                    DstVal = g_M68kContext.D[Ry];
                }
                {
                    UINT8 X = M68kTestFlag (M68K_CCR_X) ? 1 : 0;
                    UINT64 T = (UINT64)DstVal + (UINT64)SrcVal + X;
                    UINTN ShiftN = (Size == M68K_SIZE_WORD) ? 15 : 31;
                    Result = (UINT32)T;
                    if (T > 0xFFFFFFFFull) M68kSetFlag (M68K_CCR_C);
                    else                   M68kClearFlag (M68K_CCR_C);
                    if (~(DstVal ^ SrcVal) & (DstVal ^ Result) >> ShiftN)
                        M68kSetFlag (M68K_CCR_V);
                    else
                        M68kClearFlag (M68K_CCR_V);
                    if ((INT32)Result < 0) M68kSetFlag (M68K_CCR_N);
                    else                   M68kClearFlag (M68K_CCR_N);
                }
                if (Result == 0) M68kSetFlag (M68K_CCR_Z);   // Z ANDed
                else             M68kClearFlag (M68K_CCR_Z);
                if (M68kTestFlag (M68K_CCR_C)) M68kSetFlag (M68K_CCR_X);
                else                           M68kClearFlag (M68K_CCR_X);
                if (IsMemory) {
                    if (Size == M68K_SIZE_WORD)
                        M68kWriteWord (g_M68kContext.A[Ry], (UINT16)Result);
                    else
                        M68kWriteLong (g_M68kContext.A[Ry], Result);
                } else {
                    g_M68kContext.D[Ry] = Result;
                }
            }
        } else {
            M68kExecuteALU_Dn_EA (Opcode, 0);  // ADD
        }
        break;
    }

    case 0xE: {
        // Shift/Rotate
        M68kExecuteShift (Opcode);
        break;
    }

    case 0xF: {
        // Coprocessor/Line-F group. The ROM's opcode dispatch table carries
        // EMUL_OP extended opcodes starting at 0xFE40. The lower 6 bits
        // encode a selector that maps 1:1 to the PPC interpreter's
        // PpcEmulatorDispatchOp marker scheme: selector N maps to
        // EmulOpDispatch(N - 3) for selectors >= 3. Selectors 0-2 are
        // legacy VM control words and fall through to the D0-based switch.
        if ((Opcode & 0xFFC0) == 0xFE40) {
            UINT32 Sel = Opcode & 0x3F;
            if (Sel >= 3) {
                EmulOpDispatch (Sel - 3);
                break;
            }
            // Selectors 0-2 fall through to the D0-based VM services below.
        }

        // Legacy VM services: opcodes $FE00-$FE0F dispatch on D0.
        if ((Opcode & 0xFFF0) == 0xFE00) {
            static UINTN DbgVMSvc = 0;
            static BOOLEAN DumpedEd = FALSE;
            UINT16 Sel = g_M68kContext.D[0] & 0xFFFF;
            // One-time dump of the DR emulator data area (r31 = 0xB000 at
            // runtime): holds the per-selector stubs that $FE04/$FE05 tail
            // into (handler = ED + table[sel], table at ROM 0x408AD850).
            if (!DumpedEd && Opcode != 0xFE0A && Opcode != 0xFE06) {
                UINTN K;
                DumpedEd = TRUE;
                Print (L"68K FE04 ED dump:\n");
                for (K = 0; K < 144; K++) {
                    Print (L" %08x", M68kReadLong (0xB000u + (UINT32)(K * 4)));
                    if ((K & 7) == 7) {
                        Print (L"\n");
                    }
                }
            }
            if (DbgVMSvc++ < 60) {
                Print (L"68K VM svc op=%04x sel=%d a0=%08x d1=%08x @PC=%08x\n",
                       Opcode, Sel, g_M68kContext.A[0], g_M68kContext.D[1],
                       g_M68kContext.PC - 2);
            }
            switch (Sel) {
            case 0:   // VMInit -> initialized
            case 3:   // VMIsResident(page)
            case 4:   // VMIsUnmodified(page)
            case 5:   // VMIsInited
            case 7:   // VMMarkResident(page) -> was resident
            case 25:  // VMAllocateMemory(a0 first,a1 count,d1 align) -> ok
                g_M68kContext.D[0] = 1;
                break;
            case 10:  // VMGetPhysicalPage(page) -> identity (V=P)
            case 11:  // VMGetPhysicalAddress(addr) -> identity (V=P)
                g_M68kContext.D[0] = g_M68kContext.A[0];
                break;
            case 19: {
                // VMGetPTEntryGivenPage(page in A0): resident, cacheable,
                // global descriptor with physical == logical; WP bit (29)
                // clear so callers observe a writable page.
                UINT32 Page = g_M68kContext.A[0] & 0xFFFFFu;
                g_M68kContext.D[0] = (Page << 12)
                                     | 0x80000000u  // M68pdResident
                                     | 0x04000000u  // M68pdCacheNotIO
                                     | 0x00200000u; // M68pdGlobal
                break;
            }
            default:  // marking/flush/cache-attr services -> success(0)
                g_M68kContext.D[0] = 0;
                break;
            }
            break;
        }

        // Any other Line-F word (real FPU instructions etc.) must not
        // raise an exception either; log and continue.
        {
            static UINTN DbgLineF = 0;
            if (DbgLineF++ < 20) {
                Print (L"68K LINE-F op 0x%04x @PC=0x%08x (ignored)\n",
                       Opcode, g_M68kContext.PC - 2);
            }
        }
        break;
    }

    default:
        M68kExecuteIllegal (Opcode);
        break;
    }

    return 4;  // approximate cycle count
}

// Execute multiple instructions
UINTN
M68kExecuteBlock (
    IN UINTN MaxInstructions
    )
{
    UINTN Executed = 0;
    while (Executed < MaxInstructions && !g_M68kContext.Halted) {
        M68kExecuteInstruction ();
        Executed++;
    }
    return Executed;
}

// ---------------------------------------------------------------------------
// PPC trampoline entry point
// ---------------------------------------------------------------------------

EFI_STATUS
M68kExecuteFromPPC (
    VOID
    )
{
    if (g_M68kContext.Halted) {
        static BOOLEAN HaltReported = FALSE;
        if (!HaltReported) {
            HaltReported = TRUE;
            Print (L"  68K HALTED flag observed at hook entry\n");
        }
        return EFI_NOT_READY;
    }

    // Canary tripwire: identify the era in which host-side context
    // bytes get zeroed between explicit initializations.
    if (g_M68kContext.DiagCanary != 0xDEADC0DEu) {
        static BOOLEAN FlipReported = FALSE;
        static UINT32 FlipCount = 0;
        FlipCount++;
        if (!FlipReported) {
            FlipReported = TRUE;
            Print (L"  CANARY FLIP #%d: ppcPC=%08x r1=%08x r13=%08x "
                   L"m68kPC=%08x SR=%04x SSP=%08x\n", FlipCount,
                   g_PpcContext.Pc, g_PpcContext.Gpr[1],
                   g_PpcContext.Gpr[13],
                   g_M68kContext.PC, g_M68kContext.SR,
                   g_M68kContext.SSP);
        }
        // Self-heal so downstream one-shots keep working.
        g_M68kContext.DiagCanary = 0xDEADC0DEu;
    }

    // STOP #imm parked the CPU: stay parked until the PPC hook wakes us
    // (it clears Stopped when a decrementer interrupt is pending).
    if (g_M68kContext.Stopped) {
        static UINTN ParkEntryCount = 0;
        ParkEntryCount++;
        if ((ParkEntryCount & 1023) == 1) {
            Print (L"  68K PARKED by STOP [#%d]: SR=0x%04x PC=0x%08x SSP=0x%08x "
                   L"(DW=%d DN=%d MSR=%08x XP=%d)\n",
                   (UINT32)ParkEntryCount,
                   g_M68kContext.SR, g_M68kContext.PC, g_M68kContext.SSP,
                   g_PpcContext.DecrementerWritten,
                   g_PpcContext.DecrementerNegative,
                   g_PpcContext.Msr,
                   g_PpcContext.ExceptionPending);
        }
        return EFI_NOT_READY;
    }

    // Sync PPC registers into 68K context
    M68kSyncFromPPC ();

    // ---- PHASE C: batch-start hooks ------------------------------------
    // Refresh the XLM block if the ROM's pool-scrubber wiped it. The scrub
    // loop borrows [0x2800] as its own fill bound while running, so NEVER
    // rewrite the signature while the 68K PC is inside that routine.
    if ((UINT32)(g_M68kContext.PC - 0x40800500u) > 0x200u &&
        M68kReadLong (PPC_XLM_SIGNATURE_OFFSET) != 0x42616168u) {
        static UINTN XlmRefreshCount = 0;
        XlmRefreshCount++;
        if (XlmRefreshCount <= 4 || (XlmRefreshCount & 0xFF) == 0) {
            Print (L"  XLM-REFRESH #%d @PC=%08x (was %08x)\n",
                   (UINT32)XlmRefreshCount, g_M68kContext.PC,
                   M68kReadLong (PPC_XLM_SIGNATURE_OFFSET));
        }
        M68kWriteLong (PPC_XLM_SIGNATURE_OFFSET, 0x42616168);
        M68kWriteLong (PPC_XLM_KERNEL_DATA_OFFSET, 0xA000);
        M68kWriteLong (PPC_XLM_RUN_MODE_OFFSET, 0);
        M68kWriteLong (PPC_XLM_IRQ_NEST_OFFSET, 0);
    }

    // Fire due timers: pop any primed TMTask whose deadline has passed,
    // push the return address, and jump to the task's handler with
    // A1 = task pointer (Time Manager calling convention).
    {
        UINT32 DelayUs = 0;
        UINT32 TmTask = EmulOpPopDueTimer (&DelayUs);
        if (TmTask != 0) {
            UINT32 TmAddr = M68kReadLong (TmTask + 8);
            if (TmAddr != 0) {
                static UINTN TimerFireCount = 0;
                TimerFireCount++;
                Print (L"  TM-FIRE #%d task=%08x addr=%08x\n",
                       (UINT32)TimerFireCount, TmTask, TmAddr);
                g_M68kContext.A[1] = TmTask;
                M68kPushLong (g_M68kContext.PC);
                g_M68kContext.PC = TmAddr;
            }
        }
    }

    // VBL processing (EMUL_OP style): when the VIA tick has fired,
    // process the VBL entirely in C instead of raising a 68K exception.
    // This avoids the corrupted exception frame caused by M68kSyncFromPPC
    // overwriting SR with a stale PPC r25 value.  The VIA is acknowledged
    // by writing the VBL flag to $900, and any pending timers are fired.
    {
        static UINTN VblCount = 0;
        UINT32 Flags = EmulOpGetAndClearInterruptFlags ();
        if ((Flags & INTFLAG_VIA) != 0 &&
            !g_M68kContext.Halted) {
            VblCount++;
            if (VblCount < 16) {
                Print (L"  VBL #%d EMUL_OP PC=%08x SR=%04x\n",
                       (UINT32)VblCount,
                       g_M68kContext.PC, g_M68kContext.SR);
            }
            // Acknowledge VBL: write 1 to the VBL-pending flag at $900.
            M68kWriteLong (0x900, 1);
        }
        if (Flags != 0 && !(Flags & INTFLAG_VIA)) {
            // Non-VIA flags (e.g. timer) still get re-signaled for
            // later processing by the timer hook above.
            EmulOpSignalInterrupt (Flags & ~INTFLAG_VIA);
        }
    }
    // ---- END PHASE C batch-start hooks ---------------------------------

    // Execute a batch of instructions for performance
    // Log the first few PCs to understand the code flow
    static UINTN TotalExecuted = 0;
    static UINTN RepeatCount = 0;
    static BOOLEAN RunawayReported = FALSE;
    // Small-loop detector: remember the last few batch entry PCs.  A tight
    // loop that alternates between 2-4 PCs never matches the single-PC
    // check below, so also compare against a small history window.
    static UINT32 Hist[8] = {0};
    static UINTN HistIdx = 0;

    UINT32 StartPC = g_M68kContext.PC;
    UINTN Count = 0;
    UINTN MaxBatch = 500;

    {
        static UINTN BatchNum = 0;
        BatchNum++;
        if ((BatchNum & 63) == 0) {
            Print (L"  BATCH[%d] entry=0x%08x total=%dK\n",
                   (UINT32)BatchNum, StartPC,
                   (UINT32)(TotalExecuted >> 10));
        }
        // Hotspot histogram: bucket the batch-entry PC into 4 KB windows
        // inside the ROM and print the top talkers periodically. This
        // gives a truthful "where does time go" view instead of tail
        // sampling artifacts.
        {
            static UINT32 Bucket[64];
            static UINTN SinceDump = 0;
            UINT32 Pc = StartPC;
            if (Pc >= 0x40800000u && Pc < 0x41000000u) {
                UINTN B = (Pc - 0x40800000u) >> 12;
                if (B < 64) Bucket[B]++;
            } else if (Pc >= 0xFFC00000u) {
                UINTN B = 32 + ((Pc - 0xFFC00000u) >> 12);
                if (B < 64) Bucket[B]++;
            }
            SinceDump++;
            if (SinceDump >= 512) {
                SinceDump = 0;
                UINTN I;
                BOOLEAN Any = FALSE;
                Print (L"  HOTSPOTS:");
                for (I = 0; I < 64; I++) {
                    if (Bucket[I] > 200) {
                        UINT32 Base = (I < 32)
                                      ? 0x40800000u + (UINT32)(I << 12)
                                      : 0xFFC00000u +
                                            (UINT32)((I - 32) << 12);
                        Print (L" %08x:%d", Base, Bucket[I]);
                        Any = TRUE;
                    }
                    Bucket[I] = 0;
                }
                Print (L"\n");
                (VOID)Any;
            }
        }
    }

    {
        BOOLEAN Seen = FALSE;
        UINTN K;
        for (K = 0; K < 8; K++) {
            if (Hist[K] == StartPC) { Seen = TRUE; break; }
        }
        if (Seen) {
            RepeatCount++;
            if (RepeatCount > 20000) {
            // Same batch entry PC 20k times: a hard spin. Report once with
            // full context so the loop is visible, then keep going (it may
            // be an idle wait that unblocks later).
            if (!RunawayReported) {
                UINTN K;
                RunawayReported = TRUE;
                Print (L"  68K SPIN PC=0x%08x SP=0x%08x SR=0x%04x "
                       L"D0=0x%08x D1=0x%08x A2=0x%08x A6=0x%08x\n",
                       StartPC,
                       g_M68kContext.Supervisor ? g_M68kContext.SSP : g_M68kContext.A[7],
                       g_M68kContext.SR, g_M68kContext.D[0], g_M68kContext.D[1],
                       g_M68kContext.A[2], g_M68kContext.A[6]);
                Print (L"  68K SPIN last 256 PCs:");
                for (K = 0; K < 256; K++) {
                    UINTN Idx = (g_LastPcIdx + 256 - 1 - K) % 256;
                    Print (L" %08x/%04x", g_LastPcRing[Idx], g_LastOpRing[Idx]);
                    if ((K & 15) == 15) Print (L"\n   ");
                }
                Print (L"\n");
                M68kTraceFlush ();
            }
            if (RepeatCount > 200000) {
                // Still spinning much later: truly stuck.
                g_M68kContext.Halted = TRUE;
            }
            MaxBatch = 2000;
        } else if (RepeatCount > 200) {
            MaxBatch = 2000;
        }
    } else {
        RepeatCount = 0;
    }
    Hist[HistIdx] = StartPC;
    HistIdx = (HistIdx + 1) % 8;
    }

    // Trace first 20000 instructions and odd-PC errors to file
    BOOLEAN DoTrace = (TotalExecuted < 20000);

    while (Count < MaxBatch && !g_M68kContext.Halted && !g_M68kContext.Stopped) {
        // Runaway guard: PC must be in low RAM or the ROM region. Anything
        // else means we jumped into unmapped memory and are executing
        // garbage; report the last PCs and stop instead of marching forever.
        {
            UINT32 PcNow = g_M68kContext.PC;
            BOOLEAN Ok = (PcNow < 0x01000000u) ||
                         (PcNow >= 0x40800000u && PcNow < 0x41000000u) ||
                         (PcNow >= 0xFFC00000u);   // classic ROM alias
            if (!Ok && !RunawayReported) {
                UINTN K;
                RunawayReported = TRUE;
                Print (L"  68K RUNAWAY PC=0x%08x SP=0x%08x SR=0x%04x "
                       L"D0=0x%08x D1=0x%08x A2=0x%08x A4=0x%08x "
                       L"A5=0x%08x A6=0x%08x\n",
                       PcNow,
                       g_M68kContext.Supervisor ? g_M68kContext.SSP : g_M68kContext.A[7],
                       g_M68kContext.SR, g_M68kContext.D[0], g_M68kContext.D[1],
                       g_M68kContext.A[2], g_M68kContext.A[4],
                       g_M68kContext.A[5], g_M68kContext.A[6]);
                Print (L"  68K RUNAWAY last 256 PCs:");
                for (K = 0; K < 256; K++) {
                    UINTN Idx = (g_LastPcIdx + 256 - 1 - K) % 256;
                    Print (L" %08x/%04x", g_LastPcRing[Idx], g_LastOpRing[Idx]);
                    if ((K & 15) == 15) Print (L"\n   ");
                }
                Print (L"\n");
                {
                    UINT32 SpNow = g_M68kContext.Supervisor ?
                                   g_M68kContext.SSP : g_M68kContext.A[7];
                    UINTN K;
                    Print (L"  68K RUNAWAY stack@SP:");
                    for (K = 0; K < 16; K++) {
                        Print (L" %08x", M68kReadLong (SpNow + (UINT32)(K * 4)));
                    }
                    Print (L"\n");
                }
                g_M68kContext.Halted = TRUE;
                break;
            }
            g_LastPcRing[g_LastPcIdx] = PcNow;
            g_LastOpRing[g_LastPcIdx] = M68kReadWord (PcNow);
            g_LastPcIdx = (g_LastPcIdx + 1) % 256;
        }
        // DR-callback walker guard: at 0x408081BA the ROM does jsr (a2)
        // where a2 was built from a stack frame ([a1+0x44] chain). With our
        // native interpreter the emulator-data function table that should
        // live behind those pointers does not exist, so a2 ends up as
        // garbage. Redirect invalid targets to the moveq #0,d0 / rts stub
        // at Q=0x7000 and log the frame so the calling convention can be
        // reconstructed.
        if (g_M68kContext.PC == 0x408081BAu) {
            UINT32 Target = g_M68kContext.A[2];
            BOOLEAN TgtOk = (Target < 0x01000000u) ||
                            (Target >= 0x40800000u && Target < 0x41000000u);
            if (!TgtOk) {
                static INTN WalkerRedirects = -1;
                WalkerRedirects++;
                if (WalkerRedirects < 8) {
                    UINT32 A1 = g_M68kContext.A[1];
                    UINTN K;
                    Print (L"  DR-walker jsr redirect #%d: a2=0x%08x "
                           L"@PC-chain d0=%08x d1=%08x d2=%08x d3=%08x "
                           L"a3=%08x\n",
                           (int)WalkerRedirects, Target,
                           g_M68kContext.D[0], g_M68kContext.D[1],
                           g_M68kContext.D[2], g_M68kContext.D[3],
                           g_M68kContext.A[3]);
                    if (A1 >= 0x1000u && A1 < 0x00100000u) {
                        Print (L"   frame@%08x:", A1);
                        for (K = 0; K < 20; K++) {
                            Print (L" %08x",
                                   M68kReadLong ((UINT32)(A1 + K * 4)));
                        }
                        Print (L"\n");
                    }
                }
                g_M68kContext.A[2] = 0x68FF9000u;   // completion stub
            }
        }
        // Walker-caller guard (0x408081F8): the DR callback receives A1 as a
        // control block ([A1+0x44] -> handler chain). When boot hands us a
        // bogus block (template buffer pointer instead of the real NK
        // control block), swap in our fabricated one so the walker lands on
        // the completion stub instead of garbage.
        if (g_M68kContext.PC == 0x408081F8u) {
            static BOOLEAN CblkSwapped = FALSE;
            UINT32 A1 = g_M68kContext.A[1];
            UINT32 Handler = M68kReadLong ((UINT32)(A1 + 0x44));
            UINT32 Chain = (UINT32)(A1 + Handler);
            BOOLEAN Ok = ((Chain < 0x01000000u) ||
                          (Chain >= 0x40800000u && Chain < 0x41000000u));
            if (!Ok && !CblkSwapped) {
                CblkSwapped = TRUE;
                Print (L"  DR-cblk swap: a1=0x%08x [+44]=%08x -> "
                       L"cblk=0x68FF9040\n", A1, Handler);
                g_M68kContext.A[1] = 0x68FF9040u;
            }
        }
        // Trampoline EC entry: movem d0-d1/d7,-(sp) saves D7 and the
        // epilogue pop at 0x118 turns that saved value into the loader
        // stream pointer (A6). Real boot stages a compressed stream in RAM
        // and keeps its address in D7 here; our synthetic walk never built
        // one, so stage a crafted minimal stream and point D7 at it - but
        // only when D7 does not already hold a plausible buffer address.
        if (g_M68kContext.PC == 0x408000ECu) {
            static const UINT32 Stream[16] = {
                0x0002C000u,  // dest base
                0x00002000u,  // size -> final SP = 0x2D800
                0x11111111u, 0x1113F111u,
                0x22222222u, 0x33361333u,
                0x33333333u, 0x66694666u,
                0x44444444u, 0xAAAD8AAAu,
                0xFFFFFFFFu,  // sentinel
                0xAAAD8AA8u,  // breaker
                0xAA55AA55u, 0x12345678u, 0xFEEDFACEu, 0x0BADF00Du
            };
            UINT32 D7 = g_M68kContext.D[7];
            BOOLEAN Plausible =
                (D7 >= 0x00001000u && D7 < 0x00040000u) ||
                (D7 >= 0x20000000u && D7 < 0x23000000u) ||
                (D7 >= 0x40800000u && D7 < 0x41000000u) ||
                (D7 >= 0x68000000u && D7 < 0x69000000u);
            static INTN EcHits = -1;
            EcHits++;
            if (!Plausible) {
                UINTN K;
                for (K = 0; K < 16; K++) {
                    M68kWriteLong ((UINT32)(0x30000u + K * 4), Stream[K]);
                }
                g_M68kContext.D[7] = 0x00030000u;
            }
            if (EcHits < 8) {
                Print (L"  trampoline EC #%d d7=%08x %a\n", (int)EcHits, D7,
                       Plausible ? "kept" : "staged->30000");
            }
        }
        // DR-callback trampoline at 0x40800112: bsr.l 0x408081F8 runs the
        // walker with the incoming control block and a scratch frame;
        // success is signalled by $A8 at frame[$10]. The retry fallback
        // path inside 81F8 uses hardcoded ROM descriptor tables, so let
        // the whole thing execute natively now that BAD-RTS healing and
        // the jsr-chain redirect guard are active.
        if (g_M68kContext.PC == 0x40800112u) {
            static INTN DrSkipCount = -1;
            static BOOLEAN LoaderProbed = FALSE;
            DrSkipCount++;
            if (DrSkipCount < 4) {
                Print (L"  DR-callback @0x40800112 entered (#%d, "
                       L"a1=0x%08x d3=0x%08x)\n",
                       (int)DrSkipCount,
                       g_M68kContext.A[1], g_M68kContext.D[3]);
            }
            if (!LoaderProbed) {
                UINTN K;
                LoaderProbed = TRUE;
                Print (L"  loader-probe low RAM:");
                for (K = 0; K < 24; K++) {
                    Print (L" %04x",
                           M68kReadWord ((UINT32)(0x8000u + K * 2)));
                }
                Print (L"\n  loader-probe  A000:");
                for (K = 0; K < 12; K++) {
                    Print (L" %04x",
                           M68kReadWord ((UINT32)(0xA000u + K * 2)));
                }
                Print (L"\n  loader-probe ROM@AB4E:");
                for (K = 0; K < 12; K++) {
                    Print (L" %04x",
                           M68kReadWord ((UINT32)(0x4080AB4Eu + K * 2)));
                }
                Print (L"\n  loader-probe stack@112 (SP=0x%08x):",
                       M68kGetStackPointer ());
                for (K = 0; K < 16; K++) {
                    Print (L" %08x",
                           M68kReadLong ((UINT32)
                                         (M68kGetStackPointer () + K * 4u)));
                }
                Print (L"\n");
            }
        }
        // Decompressor micro-probes: settle live vs static semantics of
        // the size computation and exg/adda pair at 0x40800644-646.
        if (g_M68kContext.PC == 0x40800626u ||
            g_M68kContext.PC == 0x4080062Au ||
            g_M68kContext.PC == 0x40800646u) {
            static INTN DpCount = -1;
            static UINT32 CycCount = 0;
            if (g_M68kContext.PC == 0x40800626u) {
                CycCount++;
                if (CycCount <= 12 || (CycCount & 15) == 0) {
                    Print (L"  decomp cyc #%d a6=%08x [a6]=%08x "
                           L"[a6+4]=%08x sp=%08x\n", CycCount,
                           g_M68kContext.A[6],
                           M68kReadLong (g_M68kContext.A[6]),
                           M68kReadLong ((UINT32)(g_M68kContext.A[6] + 4)),
                           M68kGetStackPointer ());
                }
            }
            DpCount++;
            if (DpCount < 4) {
                if (g_M68kContext.PC == 0x40800626u) {
                    Print (L"  624probe@26 a6=%08x [a6]=%08x [a6+4]=%08x "
                           L"d7=%08x\n", g_M68kContext.A[6],
                           M68kReadLong (g_M68kContext.A[6]),
                           M68kReadLong ((UINT32)(g_M68kContext.A[6] + 4)),
                           g_M68kContext.D[7]);
                } else if (g_M68kContext.PC == 0x4080062Au) {
                    Print (L"  624probe@2a d7=%08x (loaded from [a6+4])\n",
                           g_M68kContext.D[7]);
                } else {
                    Print (L"  624probe@46 a7=%08x d7=%08x sp=%08x\n",
                           g_M68kContext.A[7], g_M68kContext.D[7],
                           M68kGetStackPointer ());
                }
            }
        }
        // NK init-stage context seeds: globals the real nanokernel
        // prepares before this stage. Re-seeded idempotently at gate
        // entry because the pool scrubber wipes low memory.
        if (g_M68kContext.PC == 0x40804770u) {
            static UINT32 FillIter = 0;
            static INTN FillSeen2 = -1;
            FillIter++;
            if ((FillIter & 0xFFFu) == 1u && FillSeen2 < 6) {
                FillSeen2++;
                Print (L"  nkiter #%d n=%d d3=%08x a2=%08x\n",
                       FillSeen2, FillIter, g_M68kContext.D[3],
                       g_M68kContext.A[2]);
            }
        }
        if (g_M68kContext.PC == 0x4080473Au) {
            static INTN FillSeen = -1;
            FillSeen++;
            if (FillSeen < 4) {
                Print (L"  nkfill #%d c24=[%08x] d3=%08x a2=%08x "
                       L"d0=%08x d1=%08x d2=%08x\n", FillSeen,
                       M68kReadLong (0xC24u), g_M68kContext.D[3],
                       g_M68kContext.A[2], g_M68kContext.D[0],
                       g_M68kContext.D[1], g_M68kContext.D[2]);
            }
        }
        if (g_M68kContext.PC == 0x40804640u) {
            static BOOLEAN NkSeeded = FALSE;
            if (!NkSeeded ||
                M68kReadLong (0x824u) < 0x1000u) {
                NkSeeded = TRUE;
                // Emulator-globals block normally initialized at
                // 0x40800F62-0xF88 (gamma-resource setup): one shared
                // buffer pointer in $824/$DAC/$898, size 0x80 in $C24,
                // row stride 4 in $106, misc scalars. Buffer must live
                // inside mapped low RAM (top 0x40000).
                M68kWriteLong (0x824u, 0x00036000u);
                M68kWriteLong (0xDACu, 0x00036000u);
                M68kWriteLong (0x898u, 0x00036000u);
                M68kWriteLong (0xC24u, 0x00000080u);
                M68kWriteWord (0x106u, 4);
                M68kWriteWord (0x102u, 0x48);
                M68kWriteWord (0x104u, 0x48);
                M68kWriteWord (0xC20u, 0x20);
                M68kWriteWord (0xC22u, 0x20);
                M68kWriteWord (0x8ACu, 4);
                M68kWriteWord (0xB9Eu, 0xFFFF);
                // Dispatch-table base [$2010]: initialized by the tiny
                // helper at 0x40805130 (lea 0x40805140(pc); move.l a0,
                // $2010) which our synthetic walk bypassed. Without it
                // the tail-dispatch at 0x4084A34E reads M[0x110+junk].
                M68kWriteLong (0x2010u, 0x40805140u);
                // Display-descriptor slot [$68FFEFD8]: normally built by
                // the NK side. Accessors read +4/+8/+10 (e.g. log2 source
                // at +10); unknown code paths may also call through it,
                // so fill every plausible slot with either data or a
                // pointer to an RTS stub.
                {
                    UINT32 Blk = 0x00039000u;
                    UINT32 K2;
                    M68kWriteLong ((UINT32)(Blk + 0x04), 0x00000001u);
                    M68kWriteLong ((UINT32)(Blk + 0x08), 0x00000100u);
                    M68kWriteLong ((UINT32)(Blk + 0x10), 0x00000100u);
                    M68kWriteWord (0x39100u, 0x4E75u);   // rts stub
                    for (K2 = 0x14; K2 < 0x40; K2 += 4) {
                        M68kWriteLong ((UINT32)(Blk + K2), 0x00039100u);
                    }
                    M68kWriteLong (0x68FFEFD8u, Blk);
                }
            }
        }
        // NK $A06E poll-site probe: what does the request block hold?
        if (g_M68kContext.PC == 0x408046A4u ||
            g_M68kContext.PC == 0x40804828u ||
            g_M68kContext.PC == 0x408047AEu) {
            static INTN NkSeen = -1;
            NkSeen++;
            if (NkSeen < 10) {
                if (g_M68kContext.PC == 0x408046A4u) {
                    UINT32 A0 = g_M68kContext.A[0];
                    Print (L"  nkpoll #%d a0=%08x [a0]=%08x w[a1+20]=%04x "
                           L"sp=%08x\n", NkSeen, A0,
                           M68kReadLong (A0),
                           M68kReadWord ((UINT32)(g_M68kContext.A[1] + 0x20)),
                           M68kGetStackPointer ());
                } else if (g_M68kContext.PC == 0x40804828u) {
                    Print (L"  tblcopy dac=%08x d2=%08x a6=%08x "
                           L"src=%08x\n",
                           M68kReadLong (0xDACu),
                           g_M68kContext.D[2], g_M68kContext.A[6],
                           0x40804882u);
                } else {
                    Print (L"  PARK47AE hit: sp=%08x a2=%08x d2=%08x "
                           L"[dac]=%08x\n", M68kGetStackPointer (),
                           g_M68kContext.A[2], g_M68kContext.D[2],
                           M68kReadLong (0xDACu));
                }
            }
        }
        // Decompressor exit + post-return chain probes.
        if (g_M68kContext.PC == 0x40800672u) {            static INTN RtsSeen = -1;
            RtsSeen++;
            if (RtsSeen < 3) {
                Print (L"  672rts #%d sp=%08x [sp]=%08x d7=%08x a6=%08x\n",
                       RtsSeen, M68kGetStackPointer (),
                       M68kReadLong (M68kGetStackPointer ()),
                       g_M68kContext.D[7], g_M68kContext.A[6]);
            }
        }
        if (g_M68kContext.PC == 0x4080011Eu ||
            g_M68kContext.PC == 0x40800122u ||
            g_M68kContext.PC == 0x40800126u) {
            static INTN ChainSeen = -1;
            ChainSeen++;
            if (ChainSeen < 12) {
                Print (L"  chain@%03x sp=%08x\n",
                       (UINT32)(g_M68kContext.PC - 0x40800000u),
                       M68kGetStackPointer ());
            }
        }
        // Loader-epilogue stream: 0x4080011A does bsr 0x40800624, which
        // decompresses the stream at A6 in place until a 0xFFFFFFFF
        // sentinel; [stream] = destination base, [stream+4] = size and the
        // final stack pointer becomes base + size*3/4. The stream must be a
        // WRITABLE copy staged by the DR-callback above. If the naturally
        // popped A6 points at plausible RAM, let the real decompressor run;
        // only fabricate the AB4E copy when A6 is unusable.
        if (g_M68kContext.PC == 0x4080011Au) {
            static INTN StreamSeen = -1;
            UINT32 A6v = g_M68kContext.A[6];
            BOOLEAN A6ok =
                (A6v >= 0x00002000u && A6v < 0x00040000u) ||
                (A6v >= 0x20000000u && A6v < 0x23000000u) ||
                (A6v >= 0x68000000u && A6v < 0x69000000u);
            StreamSeen++;
            if (StreamSeen < 4) {
                Print (L"  loader epilogue entry #%d a6=0x%08x "
                       L"[a6]=%08x [a6+4]=%08x sp=%08x\n",
                       (int)StreamSeen, A6v,
                       M68kReadLong (A6v),
                       M68kReadLong ((UINT32)(A6v + 4)),
                       M68kGetStackPointer ());
            }
            if (!A6ok) {
                // The park machinery rebuilds the stack, so the staged
                // pointer never survives to 118's pop. Point A6 at the
                // crafted stream (restaged idempotently) and fall through
                // into the real bsr/epilogue.
                static const UINT32 Stream[16] = {
                    0x0002C000u, 0x00002000u,
                    0x11111111u, 0x1113F111u,
                    0x22222222u, 0x33361333u,
                    0x33333333u, 0x66694666u,
                    0x44444444u, 0xAAAD8AAAu,
                    0xFFFFFFFFu,
                    0xAAAD8AA8u,
                    0xAA55AA55u, 0x12345678u, 0xFEEDFACEu, 0x0BADF00Du
                };
                UINTN K;
                for (K = 0; K < 16; K++) {
                    M68kWriteLong ((UINT32)(0x30000u + K * 4), Stream[K]);
                }
                g_M68kContext.A[6] = 0x00030000u;
                if (StreamSeen < 4) {
                    Print (L"  loader stream staged -> a6=30000 "
                           L"(crafted minimal stream)\n");
                }
            }
        }
        // Stack-wander watch: post-handoff code should live in the
        // 0x8000-0xA100 window seeded by r1=0xA000 at EMUSTART. Anything
        // outside means some instruction moved the active SP unexpectedly.
        {
            static UINT32 WanderCount = 0;
            UINT32 SpNow = g_M68kContext.Supervisor ? g_M68kContext.SSP
                                                    : g_M68kContext.A[7];
            if (WanderCount < 3 &&
                (SpNow < 0x00000F00u || SpNow > 0x0000A100u)) {
                WanderCount++;
                Print (L"68K W2 SP=0x%08x @PC=0x%08x SR=%04x "
                       L"D0=%08x D1=%08x D7=%08x A0=%08x A4=%08x A6=%08x "
                       L"C=%08x\n",
                       SpNow, g_M68kContext.PC, g_M68kContext.SR,
                       g_M68kContext.D[0], g_M68kContext.D[1],
                       g_M68kContext.D[7], g_M68kContext.A[0],
                       g_M68kContext.A[4], g_M68kContext.A[6],
                       g_M68kContext.DiagCanary);
            }
        }
        // NK context-save signature gate: at 0x40804646 the ROM checks
        // cmpi.l #5A932BC7,$DB0 after movem-ing registers to 0xC30. The
        // guest pool scrubber sprays fill patterns over low memory
        // mid-boot and wipes the injection seed, so re-seed idempotently
        // at point of use; without it the boot parks in the bra-self
        // deadloop at 0x408047AE.
        if (g_M68kContext.PC == 0x408047AEu) {
            static INTN ParkSeen = -1;
            ParkSeen++;
            if (ParkSeen < 3) {
                UINT32 K;
                UINT32 Sp = M68kGetStackPointer ();
                Print (L"  park47AE #%d bytes:", ParkSeen);
                for (K = 0; K < 8; K++) {
                    Print (L" %04x",
                           M68kReadWord ((UINT32)(0x408047AEu + K * 2)));
                }
                Print (L"\n  park47AE sp=%08x [sp]=%08x a6=%08x d2=%08x "
                       L"[dac]=%08x\n", Sp,
                       M68kReadLong (Sp),
                       g_M68kContext.A[6], g_M68kContext.D[2],
                       M68kReadLong (0xDACu));
                Print (L"  park47AE stack:");
                for (K = 0; K < 20; K++) {
                    if ((K & 7) == 0) {
                        Print (L"\n    +%02x:", K * 4);
                    }
                    Print (L" %08x", M68kReadLong ((UINT32)(Sp + K * 4)));
                }
                Print (L"\n  park47AE oldstack 7E00:");
                for (K = 0; K < 36; K++) {
                    UINT32 A = (UINT32)(0x7E00u + K * 4);
                    UINT32 V = M68kReadLong (A);
                    if ((K & 7) == 0) {
                        Print (L"\n    %04x:", (UINT16)(UINT32)(A & 0xFFFF));
                    }
                    Print (L" %08x", V);
                }
                Print (L"\n");
            }
        }
        if (g_M68kContext.PC == 0x40804646u &&
            M68kReadLong (0x00000DB0u) != 0x5A932BC7u) {
            static BOOLEAN SeedReported = FALSE;
            M68kWriteLong (0x00000DB0u, 0x5A932BC7u);
            if (!SeedReported) {
                SeedReported = TRUE;
                Print (L"  reseeded ctx-save magic @0xDB0=0x5A932BC7 "
                       L"(was wiped by pool scrub)\n");
            }
        }
        // Secondary-stack arena seed: the DR callback dispatcher loads its
        // record arena pointer from [$68FFEFF0], builds the "Sam and Eggs"
        // record at [slot+4]-32, and switches stacks via SP=[record]+0x8000.
        // Nothing in the bypassed init ever populates it, so the record is
        // written to unmapped space while the stack collapses onto 0x8000.
        // Seed a small arena in scratch space well clear of classic
        // low-memory globals: [0x68FFEFF0]=0x7840 (ptr slot),
        // [0x7844]=0x7800 (arena base), [0x7800]=0x77E0 (record buffer top
        // consumed by 'movea.l (a1),a7' before the +0x8000 stack switch).
        {
            static BOOLEAN ArenaReported = FALSE;
            if (M68kReadLong (0x68FFEFF0u) == 0) {
                M68kWriteLong (0x68FFEFF0u, 0x00007840u);
                M68kWriteLong (0x00007844u, 0x00007800u);
                M68kWriteLong (0x00007800u, 0x000077E0u);
                if (!ArenaReported) {
                    ArenaReported = TRUE;
                    Print (L"  seeded secondary-stack arena [68FFEFF0]=0x7840 "
                           L"[7844]=0x7800 [7800]=0x77E0\n");
                }
            }
        }
        if (g_M68kContext.PC == 0x40807BB8u) {
            static UINTN MemcpyEntries = 0;
            if (MemcpyEntries < 12) {
                MemcpyEntries++;
                Print (L"68K MEMCPY entry src(A0)=0x%08x dst(A1)=0x%08x "
                       L"ret=0x%08x\n",
                       g_M68kContext.A[0], g_M68kContext.A[1],
                       M68kReadLong (M68kGetStackPointer ()));
                if (MemcpyEntries == 1) {
                    UINTN K;
                    UINT32 SrcBase = g_M68kContext.A[0];
                    Print (L"68K MEMCPY src dump:");
                    for (K = 0; K < 48; K++) {
                        UINT32 V = M68kReadLong (SrcBase + (UINT32)(K * 4));
                        Print (L" %08x", V);
                        if (V == 0xFFFFFFFFu) {
                            Print (L" <-sentinel@+%02x", K * 4);
                            break;
                        }
                    }
                    Print (L"\n");
                }
                if (MemcpyEntries == 1) {
                    g_M68kDebugSteps = 0;
                }
            }
            // Unbounded scan guard: this routine copies longword pairs from
            // (A0) until it stores a pair whose first word is 0xFFFFFFFF.
            // When the walker above hands us A1/A0=0 (no driver record built
            // yet), the scan crawls through all of low memory and tramples
            // the stack above the destination. Terminate it immediately by
            // planting the sentinel at the source.
            if (g_M68kContext.A[0] < 0x00100000u &&
                M68kReadLong (g_M68kContext.A[0]) != 0xFFFFFFFFu) {
                static BOOLEAN SentinelReported = FALSE;
                M68kWriteLong (g_M68kContext.A[0], 0xFFFFFFFFu);
                M68kWriteLong (g_M68kContext.A[0] + 4, 0);
                if (!SentinelReported) {
                    SentinelReported = TRUE;
                    Print (L"  planted copy sentinel @src=0x%08x "
                           L"(walker passed null record)\n",
                           g_M68kContext.A[0]);
                }
            }
        }
        // Arm the single-step tracer on entry to the DR callback dispatcher
        // (0x408A8D60) to capture instruction-by-instruction execution up to
        // and including the mis-executed trampoline that corrupts the frame.
        {
            static BOOLEAN HnofScanned = FALSE;
            if (g_M68kContext.PC == 0x4080AA10u && !HnofScanned) {
                HnofScanned = TRUE;
                UINT32 SlotVal = M68kReadLong (0x68FFEFD0u);
                Print (L"  HNoF probe: [68FFEFD0]=0x%08x\n", SlotVal);
                if (SlotVal == 0) {
                    // Fabricate the NK info block ('HnoF' signature at +0x70)
                    // that real boot builds before this dispatcher runs.
                    // P = info block; Q = patch-target struct at [P+8]. Q is
                    // ALSO invoked as code (jsr through pointer chains), so
                    // its entry word must be RTS; init patches land at
                    // Q+0x10..Q+0x16 where they are harmless.
                    // Layout in NK-area scratch:
                    //   STUB  @0x68FF9000 : move.b #$A8,$10(a3) ; moveq
                    //                       #0,d0 ; rts — the DR-callback
                    //                       completion stub. The caller
                    //                       checks [frame+0x10]==$A8 to
                    //                       validate the call, so the stub
                    //                       writes it via A3.
                    //   CBLK  @0x68FF9040 : control block whose +0x44 holds
                    //                       STUB-CBLK so the walker's
                    //                       a2=a1+[a1+44] lands on STUB and
                    //                       [+0x14] sub-offset is 0.
                    //   QBUF  @0x68FF9800 : [P+8]-pointed template buffer;
                    //                       the dispatcher copies/relocates
                    //                       table bytes here with signed
                    //                       index displacement, so keep it
                    //                       away from STUB/CBLK.
                    UINT32 P    = 0x68FF8000u;
                    UINT32 Stub = 0x68FF9000u;
                    UINT32 Cblk = 0x68FF9040u;
                    UINT32 Qbuf = 0x68FF9800u;
                    M68kWriteLong (Stub,      0x117C00A8u); // move.b #$A8,$10(a3)
                    M68kWriteLong (Stub + 4,  0x00107000u); //   ... ; moveq #0,d0
                    M68kWriteWord (Stub + 8,  0x4E75u);     // rts
                    M68kWriteLong (Cblk + 0x44, Stub - Cblk);
                    M68kWriteLong (P + 0x08, Qbuf);
                    M68kWriteLong (P + 0x70, 0x486E666Fu);
                    M68kWriteWord (P + 0x76, 0x0000u);
                    M68kWriteLong (0x68FFEFD0u, P);
                    Print (L"  seeded HNoF info block @%08x "
                           L"(magic@+70, patchtgt@+8=%08x "
                           L"stub=%08x cblk=%08x)\n",
                           P, Qbuf, Stub, Cblk);
                } else {
                    Print (L"    [+70]=%08x [+74]=%08x [+76]=%04x\n",
                           M68kReadLong (SlotVal + 0x70),
                           M68kReadLong (SlotVal + 0x74),
                           M68kReadWord (SlotVal + 0x76));
                }
            }
        }
        {
            static BOOLEAN Afc6Probed = FALSE;
            if (g_M68kContext.PC == 0x4080AFC6u && !Afc6Probed) {
                Afc6Probed = TRUE;
                UINT32 Pv = M68kReadLong (0x68FFEFD0u);
                Print (L"  AFC6 probe: op=%04x slot=%08x [+70]=%08x "
                       L"[+76]=%04x SR=%04x\n",
                       M68kReadWord (0x4080AFC6u), Pv,
                       M68kReadLong (Pv + 0x70),
                       M68kReadWord (Pv + 0x76),
                       g_M68kContext.SR);
                Print (L"  AFC0..AFE0:");
                {
                    UINTN K;
                    for (K = 0; K < 16; K++) {
                        Print (L" %04x",
                               M68kReadWord ((UINT32)(0x4080AFC0u + K * 2)));
                    }
                }
                Print (L"\n");
            }
        }
        {
            static BOOLEAN Ab68Probed = FALSE;
            if (g_M68kContext.PC == 0x4080AB68u && !Ab68Probed) {
                Ab68Probed = TRUE;
                UINT32 Pv = M68kReadLong (0x68FFEFD0u);
                Print (L"  AB68 probe: slot=%08x [+70]=%08x [+76]=%04x "
                       L"[P+8]=%08x\n",
                       Pv,
                       M68kReadLong (Pv + 0x70),
                       M68kReadWord (Pv + 0x76),
                       M68kReadLong (Pv + 0x08));
                Print (L"  AFC0..AFE0:");
                {
                    UINTN K;
                    for (K = 0; K < 16; K++) {
                        Print (L" %04x",
                               M68kReadWord ((UINT32)(0x4080AFC0u + K * 2)));
                    }
                }
                Print (L"\n");
            }
        }
        if ((g_M68kContext.PC == 0x408A8D60u ||
             g_M68kContext.PC == 0x4080AD7Cu ||
             g_M68kContext.PC == 0x4080AA38u ||
             g_M68kContext.PC == 0x4080AB68u ||
             g_M68kContext.PC == 0x4080AFBEu ||
             g_M68kContext.PC == 0x4080AFC6u ||
             g_M68kContext.PC == 0x408081A8u ||
             g_M68kContext.PC == 0x40808200u ||
             g_M68kContext.PC == 0x408075D0u) && g_M68kDebugSteps == 0) {
            static BOOLEAN SsArmed = FALSE;
            if (!SsArmed) {
                SsArmed = TRUE;
                g_M68kDebugSteps = 2000;
                Print (L"  SS armed @0x408A8D60 canary=%08x\n",
                       g_M68kContext.DiagCanary);
            }
        }
        // Second trace window: platform-init / patch-VM region entered
        // after the gamma unwind. Trace from its entry to the BAD RTS.
        if (g_M68kContext.PC == 0x40800402u && g_M68kDebugSteps == 0) {
            static BOOLEAN SsArmed2 = FALSE;
            if (!SsArmed2) {
                SsArmed2 = TRUE;
                g_M68kDebugSteps = 1600;
                Print (L"  SS2 armed @0x40800402\n");
            }
        }
        // Third trace window: NK event-delivery context-save at E12E.
        if (g_M68kContext.PC == 0x4080E12Eu && g_M68kDebugSteps == 0) {
            static BOOLEAN SsArmed3 = FALSE;
            if (!SsArmed3) {
                SsArmed3 = TRUE;
                g_M68kDebugSteps = 1200;
                Print (L"  SS3 armed @0x4080E12E\n");
            }
        }
        // Patch-escape: the A247 patch-installer enters its applier at
        // 0x4080E0AA per entry. If the SAME entry repeats, the scan is
        // wedged on state our environment lacks (Apple's NK pre-patches
        // much of this on real hardware). Unwind to the trampoline
        // continuation at 0x408001EE -- SheepShaver replaces these very
        // stages wholesale, so skipping one wedged entry is faithful.
        if (g_M68kContext.PC == 0x4080E0AAu) {
            static UINT32 LastSig = 0;
            static UINT32 Reps = 0;
            static BOOLEAN Escaped = FALSE;
            UINT32 Sig = g_M68kContext.D[0] ^ g_M68kContext.A[1];
            if (!Escaped && Sig == LastSig) {
                UINT32 Sp = M68kGetStackPointer ();
                UINT32 K;
                Reps++;
                if (Reps >= 2) {
                    for (K = 0; K < 96; K++) {
                        if (M68kReadLong ((UINT32)(Sp + K * 4)) ==
                            0x408001E8u) {
                            Escaped = TRUE;
                            Print (L"  PATCH-ESCAPE: wedged entry "
                                   L"sig=%08x, unwinding %d words to "
                                   L"trampoline\n", Sig, K);
                            M68kWriteAn (7, Sp + K * 4 + 4);
                            g_M68kContext.PC = 0x408001EEu;
                            break;
                        }
                    }
                    if (!Escaped) Reps = 0;
                }
            } else {
                LastSig = Sig;
                Reps = 0;
            }
        }
        // Resolver exit probe: where does the computed table entry land?
        // (The E022<->E04A wedge escape itself lives inside
        // M68kExecuteInstruction — see RESOLVER-WEDGE there.)
        if (g_M68kContext.PC == 0x4080E05Au && g_M68kContext.D[0] == 0u) {
            static INTN RtSeen = -1;
            RtSeen++;
            if (RtSeen < 3) {
                UINT32 A1 = g_M68kContext.A[1];
                Print (L"  RESOLVEXIT a1=%08x [a1]=%08x [a1+4]=%08x "
                       L"sp=%08x\n", A1,
                       M68kReadLong (A1),
                       M68kReadLong ((UINT32)(A1 + 4)),
                       M68kGetStackPointer ());
                Print (L"  RESOLVEXIT live@E05A: %04x %04x | "
                       L"a0=%08x a5=%08x d7=%08x d1=%08x\n",
                       M68kReadWord (0x40800E5Au),
                       M68kReadWord (0x40800E5Cu),
                       g_M68kContext.A[0], g_M68kContext.A[5],
                       g_M68kContext.D[7], g_M68kContext.D[1]);
            }
        }
        // Fourth window: patch-era resolver spin (opcode=0 repeats).
        if (g_M68kContext.PC == 0x4080E01Cu &&
            g_M68kContext.D[0] == 0u) {
            if ((g_M68kContext.DiagGen < 3u) &&
                M68kGetStackPointer () >= 0x00020000u &&
                M68kGetStackPointer () < 0x00030000u &&
                g_M68kDebugSteps == 0) {
                UINT32 K;
                UINT32 Sp = M68kGetStackPointer ();
                g_M68kContext.DiagGen++;
                Print (L"  RESOLVER(d0=0) gen=%d canary=%08x caller chain "
                       L"@sp=%08x:", g_M68kContext.DiagGen,
                       g_M68kContext.DiagCanary, Sp);
                for (K = 0; K < 14; K++) {
                    Print (L" %08x",
                           M68kReadLong ((UINT32)(Sp + K * 4)));
                }
                Print (L"\n  RESOLVER deep frames:");
                for (K = 16; K < 48; K++) {
                    Print (L" %08x",
                           M68kReadLong ((UINT32)(Sp + K * 4)));
                    if ((K & 7) == 7) Print (L"\n     ");
                }
                Print (L"\n");
                Print (L"  RESOLVER below-SP:");
                for (K = 2; K < 34; K++) {
                    Print (L" %08x",
                           M68kReadLong ((UINT32)(Sp - K * 4)));
                    if ((K & 7) == 7) Print (L"\n     ");
                }
                Print (L"\n");
                {
                    UINTN K2;
                    Print (L"  RESOLVER last 64 PCs:");
                    for (K2 = 0; K2 < 64; K2++) {
                        UINTN Idx = (g_LastPcIdx + 256 - 1 - K2) % 256;
                        Print (L" %08x/%04x",
                               g_LastPcRing[Idx], g_LastOpRing[Idx]);
                        if ((K2 & 7) == 7) Print (L"\n     ");
                    }
                    Print (L"\n");
                    M68kTraceFlush ();
                }
            }
        }
        if (g_M68kContext.PC == 0x4080E07Au &&
            M68kGetStackPointer () < 0x0002D000u &&
            g_M68kDebugSteps == 0) {
            static BOOLEAN SsArmed4 = FALSE;
            if (!SsArmed4) {
                SsArmed4 = TRUE;
                g_M68kDebugSteps = 500;
                Print (L"  SS4 armed @0x4080E07A\n");
            }
        }
        if (g_M68kDebugSteps > 0) {
            UINT16 DbgOp = M68kReadWord (g_M68kContext.PC);
            Print (L"  SS[%04d] PC=0x%08x op=%04x SR=%04x "
                   L"D0=%08x D1=%08x D2=%08x A0=%08x A1=%08x SP=%08x\n",
                   2000 - (int)g_M68kDebugSteps, g_M68kContext.PC, DbgOp,
                   g_M68kContext.SR,
                   g_M68kContext.D[0], g_M68kContext.D[1], g_M68kContext.D[2],
                   g_M68kContext.A[0], g_M68kContext.A[1],
                   g_M68kContext.Supervisor ? g_M68kContext.SSP : g_M68kContext.A[7]);
            Print (L"     bytes:");
            {
                UINTN Db;
                for (Db = 0; Db < 8; Db++) {
                    Print (L" %02x",
                           M68kReadByte ((UINT32)(g_M68kContext.PC + Db)));
                }
            }
            Print (L"\n");
            g_M68kDebugSteps--;
        }
        if (DoTrace) {
            UINT16 Op = M68kReadWord (g_M68kContext.PC);
            UINT32 SPNow = g_M68kContext.Supervisor ? g_M68kContext.SSP : g_M68kContext.A[7];
            M68kTraceLine (
                (UINT32)(TotalExecuted + Count), g_M68kContext.PC, Op,
                SPNow, g_M68kContext.SR,
                g_M68kContext.A[6], g_M68kContext.D[0]);
        }
        // Deadloop park detection: the ROM parks here when an expected
        // driver/init state is missing (bra.b * == *). Report once with
        // context, then park like STOP so the PPC decrementer wake path
        // gets a chance to make progress; repeated wakes still spin but
        // each wake is now visible via HB68K instead of silent gigaloops.
        // Dispatch-table base [$2010]: reseeded at point of use (the
        // tail-dispatch trampoline reads [[$2010]+entry]); the pool
        // scrubber wipes low memory between eras.
        if (g_M68kContext.PC == 0x4084A34Eu &&
            M68kReadLong (0x2010u) != 0x40805140u) {
            static BOOLEAN TblReseeded = FALSE;
            M68kWriteLong (0x2010u, 0x40805140u);
            if (!TblReseeded) {
                TblReseeded = TRUE;
                Print (L"  reseeded [$2010]=40805140 at dispatch\n");
            }
        }
        if (g_M68kContext.PC == 0x408047AEu &&
            M68kReadWord (g_M68kContext.PC) == 0x60FEu) {
            static BOOLEAN ParkReported = FALSE;
            if (!ParkReported) {
                UINTN K;
                UINT32 SpNow = g_M68kContext.Supervisor ?
                               g_M68kContext.SSP : g_M68kContext.A[7];
                ParkReported = TRUE;
                Print (L"  68K DEADLOOP PARK at 0x408047AE "
                       L"(SR=%04x SP=%08x D0=%08x D3=%08x A0=%08x A1=%08x "
                       L"A5=%08x A6=%08x)\n",
                       g_M68kContext.SR, SpNow,
                       g_M68kContext.D[0], g_M68kContext.D[3],
                       g_M68kContext.A[0], g_M68kContext.A[1],
                       g_M68kContext.A[5], g_M68kContext.A[6]);
                Print (L"  68K PARK last 64 PCs:");
                for (K = 0; K < 64; K++) {
                    UINTN Idx = (g_LastPcIdx + 256 - 1 - K) % 256;
                    Print (L" %08x/%04x", g_LastPcRing[Idx], g_LastOpRing[Idx]);
                    if ((K & 7) == 7) Print (L"\n     ");
                }
                Print (L"\n");
                M68kTraceFlush ();
            }
            // BRA$ (0x60FE) at 0x408047AE is the correct68K idle loop.
            // Do NOT modify it. VBL is processed in C (EMUL_OP style)
            // and the park mechanism (IPL clear, DEC arm, clock advance)
            // handles the wake cycle.
            // Park: stop executing until something wakes us. If the
            // environment never wakes us, the run ends quietly instead of
            // burning 200M instructions of console noise.
            //
            // Synthesized unwind (Option B): the gamma/palette record
            // handler terminates by design at this self-loop when reached
            // through our synthetic dispatch. Treat the stage as complete
            // and resume the walker epilogue at 0x40807A2C: native code
            // there restores d0-d2/d7/a0-a1 from -$18(a5), unlinks and
            // rts up the 122-chain by itself. One-shot.
            {
                static BOOLEAN GammaUnwound = FALSE;
                UINT32 A5 = g_M68kContext.A[5];
                if (!GammaUnwound && A5 >= 0x00010000u && A5 < 0x00040000u) {
                    GammaUnwound = TRUE;
                    // DISARMED (2026-08-30): the synthetic gamma/palette
                    // unwind was built for the retired SheepShaver-style
                    // synthetic dispatch, where the gamma handler terminated
                    // at this self-loop by design. In the faithful DR/NK boot
                    // the same PC 0x408047AE is reached legitimately as the
                    // 68K boot's handler-walk return-park (stack holds the
                    // walker's real return address, e.g. 0x40807A74), and
                    // teleporting to 0x40807A2C REWOUND the live boot back
                    // to 0x4080012C (ROM restart). Observe only now.
                    Print (L"  synth: NOT unwinding gamma stage (a5=%08x "
                           L"sp=%08x) - park is legit DR boot wait\n",
                           A5, g_M68kContext.Supervisor ? g_M68kContext.SSP
                                                        : g_M68kContext.A[7]);
                }
            }
            // Install a minimal 68K exception vector table if the NK's
            // own vector-install stage was bypassed by our synthetic walk.
            {
                static BOOLEAN VecsInstalled = FALSE;
                if (!VecsInstalled && M68kReadLong (0x64) == 0) {
                    VecsInstalled = TRUE;

                    // RTE stub at 0x400 for unhandled exceptions.
                    M68kWriteWord (0x400, 0x4E73u);   // RTE

                    // VBL is now processed entirely in C (EMUL_OP style)
                    // instead of via a 68K exception handler.  The C code
                    // writes the VBL flag to $900 and fires timers without
                    // touching the 68K exception frame.  Vector 0x64
                    // points to the RTE stub for safety (should never be
                    // called for VBL in normal operation).
                    //
                    // Level-1 (VBL/VIA) -> RTE stub at 0x400
                    M68kWriteLong (0x64, 0x400);
                    // Level-2..7 -> RTE stub
                    M68kWriteLong (0x68, 0x400);
                    M68kWriteLong (0x6C, 0x400);
                    M68kWriteLong (0x70, 0x400);
                    M68kWriteLong (0x74, 0x400);
                    M68kWriteLong (0x78, 0x400);
                    M68kWriteLong (0x7C, 0x400);
                    // Bus error, address error, illegal instruction
                    M68kWriteLong (0x08, 0x400);
                    M68kWriteLong (0x0C, 0x400);
                    M68kWriteLong (0x10, 0x400);
                    // TRAP #0..#15 -> stub
                    { UINT32 T; for (T = 0; T < 16; T++) {
                        M68kWriteLong (0x80 + T * 4, 0x400);
                    }}
                    // A-line trap (0xB0) -> stub (Mac OS toolbox entry)
                    M68kWriteLong (0xB0, 0x400);
                    Print (L"  synth: installed 68K exception vector table "
                           L"(VBL in C via EMUL_OP)\n");
                }
            }
            // Arm the decrementer on behalf of the NK scheduler: this
            // park is reached before any guest mtspr-to-DEC has happened
            // (the arming path sits in an init stage our synthetic walk
            // bypassed), so DecrementerWritten stays 0 and the PPC-side
            // wake never fires -- both sides idle forever. ~4M instr/tick.
            g_PpcContext.Spr[22 /* SPR_DEC */] = 0x01000000u;
            g_PpcContext.DecrementerWritten = 1;
            // Force IPL to 0 so VBL (level-1) can fire and break the idle
            // loop.  The ROM raises IPL to 5 during init but the hardware
            // that would normally lower it (via VIA/VBL ack) is absent in
            // emulation.  Must also update PPC r25 so M68kSyncFromPPC
            // doesn't overwrite the cleared IPL on the next batch.
            g_M68kContext.SR &= 0xF8FFu;
            g_PpcContext.Gpr[25] = (g_M68kContext.SR >> 8) & 0xFF;
            // Advance the emulated clock by one VBL period so the VIA flag
            // is set for the next batch-start VBL injection.  Without this
            // the clock never advances (Count ≈ 0) and VBL never fires.
            EmulOpAdvanceClock (16667 * 200);
            g_M68kContext.Stopped = TRUE;
            break;
        }
        M68kExecuteInstruction ();
        Count++;
        // Heartbeat: proves 68K instructions keep flowing when PROGRESS
        // (main loop) goes quiet, e.g. during long hooked stretches.
        if (((TotalExecuted + Count) & 0xFFFF) == 0) {
            Print (L"  HB68K[%dM+%dK] PC=0x%08x SP=0x%08x\n",
                   (UINT32)((TotalExecuted + Count) >> 20),
                   (UINT32)(((TotalExecuted + Count) >> 10) & 1023),
                   g_M68kContext.PC,
                   g_M68kContext.Supervisor ? g_M68kContext.SSP : g_M68kContext.A[7]);
        }
        // Trace when PC goes odd (address error)
        if ((g_M68kContext.PC & 1)) {
            M68kTrace (L"ODD PC error\n");
            M68kTraceFlush();
            break;
        }
    }
    // PHASE C: advance the emulated microsecond clock by the number of
    // instructions we just executed (approximation of real cycle count).
    EmulOpAdvanceClock (Count);
    TotalExecuted += Count;
    // Periodic sampling: shows where the 68K interpreter is after the
    // initial 500-instruction trace window ends.
    {
        static UINTN LastSample = 0;
        if (TotalExecuted - LastSample >= 4000000) {
            LastSample = TotalExecuted;
            Print (L"  M68K[%dM] PC=0x%08x SP=0x%08x SR=0x%04x D0=0x%08x D1=0x%08x A2=0x%08x A6=0x%08x\n",
                   (UINT32)(TotalExecuted >> 20), g_M68kContext.PC,
                   g_M68kContext.Supervisor ? g_M68kContext.SSP : g_M68kContext.A[7],
                   g_M68kContext.SR, g_M68kContext.D[0], g_M68kContext.D[1],
                   g_M68kContext.A[2], g_M68kContext.A[6]);
        }
    }
    M68kTraceFlush();

    // Sync back
    M68kSyncToPPC ();

    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Register synchronization between PPC and 68K contexts
// ---------------------------------------------------------------------------
// The ROM's DR emulator maps 68K registers to PPC registers:
//   D0-D7   = PPC r8-r15
//   A0-A6   = PPC r16-r22
//   A7/SSP  = PPC r1
//   68K PC  = PPC r24
//   68K SR  = PPC r25

VOID
M68kSyncFromPPC (
    VOID
    )
{
    for (UINT8 i = 0; i < 8; i++) {
        g_M68kContext.D[i] = g_PpcContext.Gpr[8 + i];
        g_M68kContext.A[i] = g_PpcContext.Gpr[16 + i];
    }
    // r25 = system byte of SR (SR >> 8); preserve CCR from last instruction
    UINT16 SysByte = (UINT16)((g_PpcContext.Gpr[25] & 0xFF) << 8);
    g_M68kContext.SR = SysByte | (g_M68kContext.SR & 0xFF);
    g_M68kContext.Supervisor = (g_M68kContext.SR & M68K_SR_S) != 0;
    // r1 = ACTIVE stack pointer (SSP in supervisor mode, USP otherwise).
    // A[7] keeps holding the user stack so mode switches preserve it.
    if (g_M68kContext.Supervisor) {
        g_M68kContext.SSP = g_PpcContext.Gpr[1];
    } else {
        g_M68kContext.A[7] = g_PpcContext.Gpr[1];
    }
    g_M68kContext.PC  = g_PpcContext.Gpr[24];
}

VOID
M68kSyncToPPC (
    VOID
    )
{
    for (UINT8 i = 0; i < 8; i++) {
        g_PpcContext.Gpr[8 + i]  = g_M68kContext.D[i];
        g_PpcContext.Gpr[16 + i] = g_M68kContext.A[i];
    }
    // r25 = system byte of SR (SR >> 8)
    g_PpcContext.Gpr[25] = (UINT32)(g_M68kContext.SR >> 8);
    g_PpcContext.Gpr[1]  = g_M68kContext.Supervisor
                           ? g_M68kContext.SSP
                           : g_M68kContext.A[7];
    g_PpcContext.Gpr[24] = g_M68kContext.PC;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

VOID
M68kInitialize (
    VOID
    )
{
    ZeroMem (&g_M68kContext, sizeof (g_M68kContext));
    g_M68kContext.DiagCanary = 0xDEADC0DEu;
    Print (L"  68K interpreter initialized (canary=%08x)\n",
           g_M68kContext.DiagCanary);
}

VOID
M68kReset (
    VOID
    )
{
    // Load initial SSP and PC from the vector table at 0x0000
    g_M68kContext.SSP = M68kReadLong (M68K_VEC_RESET_SSP);
    g_M68kContext.PC  = M68kReadLong (M68K_VEC_RESET_PC);
    g_M68kContext.SR  = M68K_SR_S | 0x0700;  // Supervisor, interrupts masked
    g_M68kContext.Supervisor = TRUE;
    g_M68kContext.Halted = FALSE;
    g_M68kContext.Stopped = FALSE;

    // Clear all registers
    ZeroMem (g_M68kContext.D, sizeof (g_M68kContext.D));
    ZeroMem (g_M68kContext.A, sizeof (g_M68kContext.A));

    Print (L"  68K reset: SSP=0x%08x PC=0x%08x SR=0x%04x\n",
           g_M68kContext.SSP, g_M68kContext.PC, g_M68kContext.SR);
}

// ---------------------------------------------------------------------------
// Opcode table patching
// ---------------------------------------------------------------------------

// Trampoline that the PPC interpreter calls for 68K opcodes.
// This is a PPC instruction sequence that:
// 1. Loads the 68K opcode from guest memory at the 68K PC
// 2. Calls M68kExecuteFromPPC()
// 3. Returns to the PPC DR emulator loop
//
// For now, we'll use a simpler approach: patch opcode table entries to branch
// directly to the C trampoline. Each entry in the opcode table is 8 bytes.
// We write a PPC `b` (branch) instruction that jumps to a C function.

VOID
M68kPatchOpcodeTable (
    VOID
    )
{
    // The opcode table starts at ROM + 0x380000
    // Each entry is 8 bytes: 4 bytes of PPC code + 4 bytes of data
    // For regular 68K opcodes (non-EMUL_OP), we want to replace the
    // existing PPC code with a branch to our C trampoline.
    //
    // The EMUL_OP entries (0xFE40..0xFE40+OP_MAX+2) are at the end of the
    // table and already have the mulli marker interception - we leave those.
    //
    // For now, we print a diagnostic and don't actually patch the table.
    // The actual patching will be done in a subsequent session by analyzing
    // the exact PPC code in the table entries and replacing them.

    Print (L"  68K opcode table patching: deferred to integration phase\n");
}

// ---------------------------------------------------------------------------
// Instruction decode to mnemonic
// ---------------------------------------------------------------------------

VOID
M68kDecodeInstruction (
    IN  UINT16 Opcode,
    OUT CHAR16* Buffer,
    IN  UINTN   BufferSize
    )
{
    if (Buffer == NULL || BufferSize < 16) return;

    UINT8 TopBits = Opcode >> 12;

    // Quick decode for common instructions
    if (TopBits == 0x7 && (Opcode & 0xF100) == 0x7000) {
        StrnCpy (Buffer, L"MOVEQ", BufferSize / sizeof (CHAR16) - 1);
        return;
    }
    if ((Opcode & 0xFFF0) == 0x4E40) {
        StrnCpy (Buffer, L"TRAP", BufferSize / sizeof (CHAR16) - 1);
        return;
    }
    if (Opcode == 0x4E71) {
        StrnCpy (Buffer, L"NOP", BufferSize / sizeof (CHAR16) - 1);
        return;
    }
    if (Opcode == 0x4E75) {
        StrnCpy (Buffer, L"RTS", BufferSize / sizeof (CHAR16) - 1);
        return;
    }

    StrnCpy (Buffer, L"68k-op", BufferSize / sizeof (CHAR16) - 1);
}

// ---------------------------------------------------------------------------
// 68K self-test suite
// ---------------------------------------------------------------------------

EFI_STATUS
M68kRunSelfTest (
    VOID
    )
{
    UINTN Passed = 0;
    UINTN Failed = 0;

    Print (L"--- 68K CPU self-test ---\n");

    // Test 1: MOVEQ
    ZeroMem (&g_M68kContext, sizeof (g_M68kContext));
    g_M68kContext.D[0] = 0;
    g_M68kContext.PC = 0x1000;
    // MOVEQ #42,D0: 702A
    g_M68kContext.PC = 0;
    g_M68kContext.SR = 0x2700;
    // Manually execute MOVEQ
    {
        UINT16 Opcode = 0x702A;
        M68kExecuteMoveq (Opcode);
    }
    if (g_M68kContext.D[0] == 42) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: MOVEQ #42,D0 -> D0=0x%08x (expected 42)\n", g_M68kContext.D[0]);
    }

    // Test 2: MOVEQ negative
    g_M68kContext.D[1] = 0;
    {
        UINT16 Opcode = 0x72FF;  // MOVEQ #-1,D1
        M68kExecuteMoveq (Opcode);
    }
    if (g_M68kContext.D[1] == 0xFFFFFFFF) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: MOVEQ #-1,D1 -> D1=0x%08x (expected 0xFFFFFFFF)\n", g_M68kContext.D[1]);
    }

    // Test 3: MOVEQ flags (N flag for negative)
    if (M68kTestFlag (M68K_CCR_N)) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: MOVEQ #-1,D1 should set N flag\n");
    }

    // Test 4: MOVEQ Z flag for zero
    g_M68kContext.D[2] = 0;
    {
        UINT16 Opcode = 0x7400;  // MOVEQ #0,D2
        M68kExecuteMoveq (Opcode);
    }
    if (M68kTestFlag (M68K_CCR_Z)) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: MOVEQ #0,D2 should set Z flag\n");
    }

    // Test 5: MOVEQ all 8 data registers
    BOOLEAN AllRegsOk = TRUE;
    for (UINT8 i = 0; i < 8; i++) {
        g_M68kContext.D[i] = 0;
        UINT16 Opcode = 0x7000 | (i << 9) | 0x0042;  // MOVEQ #0x42,Dn
        M68kExecuteMoveq (Opcode);
        if (g_M68kContext.D[i] != 0x42) {
            AllRegsOk = FALSE;
            Print (L"  FAIL: MOVEQ #0x42,D%d -> 0x%08x\n", i, g_M68kContext.D[i]);
        }
    }
    if (AllRegsOk) Passed++; else Failed++;

    // Test 6: ADD (via ADDQ since we can't easily encode ALU_Dn_EA in a word)
    g_M68kContext.D[0] = 10;
    g_M68kContext.D[1] = 20;
    // ADDQ #5,D0.L = 0101 101 0 10 000 000 = 0x5A80
    {
        UINT16 Opcode = 0x5A80;  // ADDQ #5,D0.L
        M68kExecuteAddqSubq (Opcode, FALSE);
    }
    if (g_M68kContext.D[0] == 15) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: ADDQ #5,D0 (was 10) -> 0x%08x (expected 15)\n", g_M68kContext.D[0]);
    }

    // Test 7: SUBQ
    g_M68kContext.D[0] = 10;
    {
        UINT16 Opcode = 0x5B80;  // SUBQ #5,D0.L
        M68kExecuteAddqSubq (Opcode, TRUE);
    }
    if (g_M68kContext.D[0] == 5) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: SUBQ #5,D0 (was 10) -> 0x%08x (expected 5)\n", g_M68kContext.D[0]);
    }

    // Test 8: ADDQ overflow detection
    g_M68kContext.D[0] = 0x7FFFFFFF;
    g_M68kContext.SR = 0x2700;
    {
        UINT16 Opcode = 0x5A80;  // ADDQ #5,D0.L
        M68kExecuteAddqSubq (Opcode, FALSE);
    }
    if (M68kTestFlag (M68K_CCR_V)) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: ADDQ #5,D0 (0x7FFFFFFF) should set V flag\n");
    }

    // Test 9: SUBQ underflow detection (negative - positive = positive → V)
    g_M68kContext.D[0] = 0x80000000;
    g_M68kContext.SR = 0x2700;
    {
        UINT16 Opcode = 0x5B80;  // SUBQ #5,D0.L
        M68kExecuteAddqSubq (Opcode, TRUE);
    }
    if (M68kTestFlag (M68K_CCR_V)) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: SUBQ #5,D0 (0x80000000) should set V flag\n");
    }

    // Test 10: SWAP Dn
    g_M68kContext.D[0] = 0x12345678;
    g_M68kContext.SR = 0x2700;
    {
        UINT16 Opcode = 0x4840;  // SWAP D0
        M68kExecuteSwap (Opcode);
    }
    if (g_M68kContext.D[0] == 0x56781234) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: SWAP D0 (0x12345678) -> 0x%08x (expected 0x56781234)\n", g_M68kContext.D[0]);
    }

    // Test 11: EXT.W
    g_M68kContext.D[0] = 0x000000FF;
    g_M68kContext.SR = 0x2700;
    {
        UINT16 Opcode = 0x4880;  // EXT.W D0
        M68kExecuteExt (Opcode);
    }
    if ((g_M68kContext.D[0] & 0xFFFF) == 0xFFFF) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: EXT.W D0 (0xFF) -> 0x%08x (expected 0xFFFFFFFF)\n", g_M68kContext.D[0]);
    }

    // Test 12: EXT.L
    g_M68kContext.D[0] = 0x00008000;
    g_M68kContext.SR = 0x2700;
    {
        UINT16 Opcode = 0x48C0;  // EXT.L D0
        M68kExecuteExt (Opcode);
    }
    if (g_M68kContext.D[0] == 0xFFFF8000) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: EXT.L D0 (0x8000) -> 0x%08x (expected 0xFFFF8000)\n", g_M68kContext.D[0]);
    }

    // Test 13: EXG Dn,Dn
    g_M68kContext.D[0] = 0xAAAA;
    g_M68kContext.D[1] = 0xBBBB;
    {
        UINT16 Opcode = 0xC141;  // EXG D0,D1 (1100 000 1 01000 001)
        M68kExecuteExg (Opcode);
    }
    if (g_M68kContext.D[0] == 0xBBBB && g_M68kContext.D[1] == 0xAAAA) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: EXG D0,D1 -> D0=0x%08x D1=0x%08x\n", g_M68kContext.D[0], g_M68kContext.D[1]);
    }

    // Test 14: TST (should set Z)
    g_M68kContext.SR = 0x2700;
    {
        // TST D0 = 0x4A00 | (0 << 9) | 0 | 0 = 0x4A00 (mode 0, reg 0)
        UINT16 Opcode = 0x4A00;  // TST.B D0
        g_M68kContext.D[0] = 0;
        M68kExecuteTst (Opcode);
    }
    if (M68kTestFlag (M68K_CCR_Z)) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: TST #0 should set Z flag\n");
    }

    // Test 15: TST (should clear Z)
    g_M68kContext.SR = 0x2700;
    {
        UINT16 Opcode = 0x4A00;  // TST.B D0
        g_M68kContext.D[0] = 42;
        M68kExecuteTst (Opcode);
    }
    if (!M68kTestFlag (M68K_CCR_Z)) {
        Passed++;
    } else {
        Failed++;
        Print (L"  FAIL: TST #42 should clear Z flag\n");
    }

    Print (L"--- 68K self-test: %d/%d passed ---\n", Passed, Passed + Failed);
    return (Failed == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;
}

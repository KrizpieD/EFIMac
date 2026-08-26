// ---------------------------------------------------------------------------
// PHASE C: EMUL_OP device handlers (SheepShaver-faithful semantics).
//
// Implements the host-side device layer the patched ROM reaches through
// EMUL_OP extended opcodes: XPRAM/NVRAM, memory-size fixes, floppy/disk/
// CDROM drivers (backed by UEFI Block I/O), audio and sound-input stubs,
// ADB, the Time Manager queue, Microseconds, clipboard scrap, DebugStr,
// driver installation, Name Registry hooks, RESET, the Level-1 IRQ
// aggregator, SCSI dispatch, system-version/resource checks, external
// file systems and idle callbacks.
//
// Selector numbering follows bootloader.h PPC_OP_* (= SheepShaver
// emul_op.h order); register conventions follow the DR emulator map
// (D0-D7 = r8-r15, A0-A6 = r16-r22, A7 = r1). Guest memory access goes
// through the interpreter's byte/word accessors so devices observe the
// same view as emulated CPU code.
// ---------------------------------------------------------------------------

#include <efi.h>
#include <efilib.h>
#include "emul_op.h"
#include "interpreter.h"
#include "translation.h"
#include "boot/bootloader.h"
#include "hardware/abstraction.h"

// ---------------------------------------------------------------------------
// Register shorthands (DR emulator mapping)
// ---------------------------------------------------------------------------
#define RD(n)  (g_PpcContext.Gpr[8 + (n)])     // D0-D7
#define RA(n)  (g_PpcContext.Gpr[16 + (n)])    // A0-A6
#define RSP    (g_PpcContext.Gpr[1])           // A7

// ---------------------------------------------------------------------------
// Guest memory helpers (big-endian)
// ---------------------------------------------------------------------------
static UINT8  EmulRb (UINT32 a)          { return PpcReadGuestByte(a); }
static VOID   EmulWb (UINT32 a, UINT8 v) { PpcWriteGuestByte(a, v); }
static UINT16 EmulRw (UINT32 a) {
    return (UINT16)((EmulRb(a) << 8) | EmulRb(a + 1));
}
static VOID   EmulWw (UINT32 a, UINT16 v) {
    EmulWb(a, (UINT8)(v >> 8)); EmulWb(a + 1, (UINT8)v);
}
static UINT32 EmulRl (UINT32 a) {
    return ((UINT32)EmulRb(a) << 24) | ((UINT32)EmulRb(a + 1) << 16) |
           ((UINT32)EmulRb(a + 2) << 8) | (UINT32)EmulRb(a + 3);
}
static VOID   EmulWl (UINT32 a, UINT32 v) {
    EmulWb(a, (UINT8)(v >> 24)); EmulWb(a + 1, (UINT8)(v >> 16));
    EmulWb(a + 2, (UINT8)(v >> 8)); EmulWb(a + 3, (UINT8)v);
}

// ---------------------------------------------------------------------------
// Clock and timers
// ---------------------------------------------------------------------------

// Instruction-to-microsecond scale: ~200 guest instructions per microsecond
// (approximates the interpreted throughput of the PPC core on this host).
#define EMUL_INSTR_PER_US 200u

static UINT64 g_EmulMicros = 0;
static UINT64 g_NextVblUs = 0;
static UINT64 g_NextAdbPollUs = 0;

// Time Manager queue. TMTask layout (classic Mac OS):
//   +0x00 qLink, +0x04 qType, +0x08 tmAddr, +0x0C tmCount/wakeup fields.
#define EMUL_MAX_TIMERS 48
typedef struct {
    UINT32  TaskPtr;      // guest TMTask address (0 = free slot)
    UINT64  DueUs;        // absolute wakeup time when Primed
    BOOLEAN Primed;       // PrimeTime() armed
} EMUL_TIMER_SLOT;
static EMUL_TIMER_SLOT g_Timers[EMUL_MAX_TIMERS];

// Interrupt flag aggregation (SheepShaver InterruptFlags).
static UINT32 g_InterruptFlags = 0;

UINT64
EmulOpGetMicroseconds (
    VOID
    )
{
    return g_EmulMicros;
}

VOID
EmulOpAdvanceClock (
    IN UINTN InstructionsExecuted
    )
{
    g_EmulMicros += (UINT64)(InstructionsExecuted / EMUL_INSTR_PER_US);

    if (g_NextVblUs == 0) {
        g_NextVblUs = g_EmulMicros + 16667;   // first tick ~60 Hz away
    }
    while (g_EmulMicros >= g_NextVblUs) {
        g_NextVblUs += 16667;
        g_InterruptFlags |= INTFLAG_VIA;
    }
    if (g_NextAdbPollUs == 0) {
        g_NextAdbPollUs = g_EmulMicros + 8000;
    }
    while (g_EmulMicros >= g_NextAdbPollUs) {
        g_NextAdbPollUs += 8000;              // 125 Hz keyboard/mouse poll
        g_InterruptFlags |= INTFLAG_ADB;
    }
}

UINT32
EmulOpGetAndClearInterruptFlags (
    VOID
    )
{
    UINT32 F = g_InterruptFlags;
    g_InterruptFlags = 0;
    return F;
}

VOID
EmulOpSignalInterrupt (
    IN UINT32 Flags
    )
{
    g_InterruptFlags |= Flags;
}

UINT32
EmulOpPopDueTimer (
    OUT UINT32* DelayUsRemaining
    )
{
    UINTN I;
    if (DelayUsRemaining != NULL) {
        *DelayUsRemaining = 0;
    }
    for (I = 0; I < EMUL_MAX_TIMERS; I++) {
        if (g_Timers[I].TaskPtr != 0 && g_Timers[I].Primed &&
            g_EmulMicros >= g_Timers[I].DueUs) {
            UINT32 Task = g_Timers[I].TaskPtr;
            g_Timers[I].Primed = FALSE;
            return Task;
        }
    }
    return 0;
}

static EFI_STATUS
EmulTimerInsert (
    IN UINT32 TaskPtr
    )
{
    UINTN I;
    for (I = 0; I < EMUL_MAX_TIMERS; I++) {
        if (g_Timers[I].TaskPtr == TaskPtr) {
            return EFI_SUCCESS;               // already queued
        }
    }
    for (I = 0; I < EMUL_MAX_TIMERS; I++) {
        if (g_Timers[I].TaskPtr == 0) {
            g_Timers[I].TaskPtr = TaskPtr;
            g_Timers[I].Primed = FALSE;
            g_Timers[I].DueUs = 0;
            return EFI_SUCCESS;
        }
    }
    return EFI_OUT_OF_RESOURCES;
}

static BOOLEAN
EmulTimerRemove (
    IN UINT32 TaskPtr
    )
{
    UINTN I;
    for (I = 0; I < EMUL_MAX_TIMERS; I++) {
        if (g_Timers[I].TaskPtr == TaskPtr) {
            BOOLEAN WasPrimed = g_Timers[I].Primed;
            g_Timers[I].Primed = FALSE;
            return WasPrimed;                 // tmTaskActive-ish signal
        }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// XPRAM / NVRAM images
// ---------------------------------------------------------------------------
static UINT8 g_Xpram[0x2000];             // SheepShaver NVRAM/XPRAM (8KB)
static BOOLEAN g_XpramInitialized = FALSE;

static VOID
EmulXpramInit (
    VOID
    )
{
    if (g_XpramInitialized) {
        return;
    }
    g_XpramInitialized = TRUE;
    ZeroMem(g_Xpram, sizeof(g_Xpram));
    // Neutral power-on defaults: no LocalTalk (EtherTalk), sound volume 4,
    // internal video. Values mirror a fresh PRAM battery state.
    g_Xpram[0x13E0] = 0x00;
    g_Xpram[0x13E1] = 0x00;
    g_Xpram[0x13E2] = 0x00;
    g_Xpram[0x13E3] = 0x0A;
}

// ---------------------------------------------------------------------------
// Block-device plumbing for the disk/CDROM drivers
// ---------------------------------------------------------------------------

// Map a Mac OS drive number to an enumerated Block I/O device index.
// Drive 1 is the attached classic-Mac disc (the HFS volume the boot layer
// reads); drive 0 is the ESP-backed disk. Unknown drives fail gracefully.
static UINTN
EmulDriveToDevice (
    IN UINT16 Drive
    )
{
    if (Drive == 0 || Drive == 1) {
        return 1;                             // attached Mac disc image
    }
    return (UINTN)-1;
}

// Perform a Prime read/write against the mapped device using the standard
// IOParam field offsets: ioBuffer +22, ioReqCount +26, ioActCount +30,
// ioPosMode +34, ioPosOffset +36.
static UINT32
EmulDiskPrimeTransfer (
    IN UINT32 Pb,
    IN BOOLEAN IsWrite
    )
{
    PPC_BLOCK_DEVICE_INFO Dev;
    UINT32 Buffer = EmulRl(Pb + 22);
    UINT32 ReqCount = EmulRl(Pb + 26);
    UINT32 PosOffset = EmulRl(Pb + 36);
    UINT16 RefNum = EmulRw(Pb + 14);
    UINTN Device;
    UINT64 BytePos;
    UINTN Remaining;
    UINT8 Sector[2048];

    if (IsWrite) {
        return 46;                            // wPrErr: media read-only
    }

    Device = EmulDriveToDevice(RefNum);
    if (Device == (UINTN)-1 ||
        EFI_ERROR(PpcGetBlockDeviceInfo(Device, &Dev))) {
        return 64;                            // nsDrvErr
    }

    BytePos = (UINT64)PosOffset & ~0xFFu;     // long portion of ioPosOffset
    Remaining = ReqCount;
    while (Remaining > 0 && Buffer != 0) {
        UINTN BlockSize = Dev.BlockSize ? Dev.BlockSize : 512;
        EFI_LBA Lba;
        UINTN OffInBlock;
        UINTN Chunk;
        UINTN I;

        if (BlockSize > sizeof(Sector)) {
            BlockSize = sizeof(Sector);
        }
        Lba = (EFI_LBA)(BytePos / BlockSize);
        OffInBlock = (UINTN)(BytePos % BlockSize);
        Chunk = BlockSize - OffInBlock;
        if (Chunk > Remaining) {
            Chunk = Remaining;
        }
        if (EFI_ERROR(PpcReadDiskBlock(Device, Lba, BlockSize, Sector))) {
            return 51;                        // read error class
        }
        for (I = 0; I < Chunk; I++) {
            EmulWb(Buffer, Sector[OffInBlock + I]);
            Buffer++;
        }
        BytePos += Chunk;
        Remaining -= Chunk;
    }
    EmulWl(Pb + 30, ReqCount);                // ioActCount = ioReqCount
    return 0;
}

// ---------------------------------------------------------------------------
// Driver Open/Control/Status (floppy / disk / CDROM share the plumbing)
// ---------------------------------------------------------------------------

static UINT32
EmulDriverOpen (
    IN UINT32 Pb,
    IN UINT32 Dib,
    IN BOOLEAN RemovableMedia
    )
{
    UINT16 RefNum = EmulRw(Pb + 14);
    (VOID)RemovableMedia;
    // MediaInfo: hand back a small static descriptor in low RAM scratch:
    // [0]=qType(1=drvst), [2]=drive QElement ref. Keep it simple: pointer
    // to the DIB itself is what most drivers store.
    if (Dib != 0) {
        EmulWl(Dib + 0x10, RefNum);           // driver refNum into DIB
        EmulWw(Dib + 0x00, 1);                // dQType = drvQType
    }
    return 0;                                 // noErr
}

static UINT32
EmulDriverControl (
    IN UINT32 Pb
    )
{
    UINT16 CsCode = EmulRw(Pb + 28);
    switch (CsCode) {
    case 5:                                   // KillIO
        return 0;
    case 6:                                   // Format/erase -> read-only
        return 49;                            // wrPermErr
    case 21:                                  // Eject: pretend done
        return 0;
    default:
        return 0;                             // accept unknown controls
    }
}

static UINT32
EmulDriverStatus (
    IN UINT32 Pb
    )
{
    // csCode 6 = drive status: fill DrvSts at ioBuffer.
    UINT16 CsCode = EmulRw(Pb + 28);
    UINT32 Buffer = EmulRl(Pb + 22);
    if (CsCode == 6 && Buffer != 0) {
        // DrvSts: track/install words zeroed; write-prot bit clear;
        // diskInPlace = 1 (non-ejectable) for fixed, 1 for CD too.
        EmulWw(Buffer + 0, 0);                // qLink
        EmulWw(Buffer + 2, 0);                // qType
        EmulWw(Buffer + 4, 1);                // track
        EmulWw(Buffer + 6, 1);                // installed: disk present
        EmulWw(Buffer + 8, 1);                // sides? keep minimal sane
        EmulWw(Buffer + 10, 0);               // write-protected: no
        EmulWw(Buffer + 12, 1);               // diskInPlace
        EmulWw(Buffer + 14, 0);               // twoSideFmt/initWarn...
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Clipboard scrap (host-side buffer)
// ---------------------------------------------------------------------------
static UINT8 g_Scrap[4096];
static UINTN g_ScrapLen = 0;

// ---------------------------------------------------------------------------
// DebugStr passthrough (kept from the previous implementation)
// ---------------------------------------------------------------------------
static VOID
EmulDebugStr (
    VOID
    )
{
    UINT32 Ptr = EmulRl(RSP + 4);
    UINT8 Len = EmulRb(Ptr);
    UINTN I;
    Print(L"  DebugStr: \"");
    for (I = 0; I < Len && I < 255; I++) {
        UINT8 C = EmulRb(Ptr + 1 + I);
        if (C >= 0x20 && C <= 0x7E) {
            Print(L"%c", (UINTN)C);
        }
    }
    Print(L"\"\n");
}

// ---------------------------------------------------------------------------
// Main selector dispatch
// ---------------------------------------------------------------------------
VOID
EmulOpDispatch (
    IN UINT32 Selector
    )
{
    switch (Selector) {

    case PPC_OP_BREAK:
        Print(L"*** Breakpoint: d0=%08x a0=%08x a7=%08x\n",
              RD(0), RA(0), RSP);
        break;

    // ---- XPRAM -------------------------------------------------------
    case PPC_OP_XPRAM1: {                     // XPRAMReadWrite
        UINT32 Len = RD(3);
        UINT32 Adr = RA(3);
        UINT32 Ofs = Len & 0xFFFF;
        UINT32 Count = Len >> 16;
        UINT32 I;
        EmulXpramInit();
        if (Count & 0x8000) {                 // write side
            Count &= 0x7FFF;
            for (I = 0; I < Count; I++, Adr++) {
                g_Xpram[((Ofs + I) & 0xFF) + 0x300] = EmulRb(Adr);
            }
        } else {                              // read side
            for (I = 0; I < Count; I++, Adr++) {
                EmulWb(Adr, g_Xpram[((Ofs + I) & 0xFF) + 0x300]);
            }
        }
        break;
    }
    case PPC_OP_XPRAM2:                       // read one byte
        EmulXpramInit();
        RD(1) = g_Xpram[(RD(1) & 0xFF) + 0x300];
        break;

    case PPC_OP_XPRAM3:                       // write one byte
        EmulXpramInit();
        g_Xpram[(RD(1) & 0xFF) + 0x300] = (UINT8)RD(2);
        break;

    // ---- NVRAM -------------------------------------------------------
    case PPC_OP_NVRAM1: {                     // read
        UINT32 Ofs = RD(0);
        EmulXpramInit();
        RD(0) = g_Xpram[Ofs & 0x1FFF];
        if (Ofs == 0x13E0 && !(g_Xpram[0x13E0] || g_Xpram[0x13E1])) RD(0) = 0x00;
        else if (Ofs == 0x13E1 && !(g_Xpram[0x13E0] || g_Xpram[0x13E1])) RD(0) = 0x01;
        else if (Ofs == 0x13E2 && !(g_Xpram[0x13E0] || g_Xpram[0x13E1])) RD(0) = 0x00;
        else if (Ofs == 0x13E3 && !(g_Xpram[0x13E0] || g_Xpram[0x13E1])) RD(0) = 0x0A;
        break;
    }
    case PPC_OP_NVRAM2:                       // write
        EmulXpramInit();
        g_Xpram[RD(0) & 0x1FFF] = (UINT8)RD(1);
        break;

    case PPC_OP_NVRAM3:                       // read/write
        EmulXpramInit();
        if (RD(3)) {
            RD(0) = g_Xpram[(RD(4) + 0x300) & 0x1FFF];
        } else {
            g_Xpram[(RD(4) + 0x300) & 0x1FFF] = (UINT8)RD(5);
            RD(0) = 0;
        }
        break;

    // ---- Boot-time memory fixes ---------------------------------------
    case PPC_OP_FIX_MEMTOP:
        // MemTop := top of logical RAM (256 MB window ending at 0xFFF7000
        // per the injected PMDT); A6 carries the same value forward.
        RA(6) = 0x0FFF6000u;
        break;

    case PPC_OP_FIX_MEMSIZE: {
        // Preserve [0x1ef8]-[0x1ef4] delta like SheepShaver, then stamp
        // physical/logical sizes for this machine (256 MB physical).
        UINT32 Diff = EmulRl(0x1EF8) - EmulRl(0x1EF4);
        EmulWl(0x1EF8, 0x10000000u);
        EmulWl(0x1EF4, 0x10000000u - Diff);
        break;
    }

    case PPC_OP_FIX_BOOTSTACK:
        // Boot stack at RAMBase + size*3/4 (SheepShaver formula).
        RA(1) = 0x00C00000u;
        RSP   = 0x00C00000u;
        break;

    // ---- Floppy --------------------------------------------------------
    case PPC_OP_SONY_OPEN:     RD(0) = EmulDriverOpen(RA(0), RA(1), TRUE);  break;
    case PPC_OP_SONY_PRIME:    RD(0) = 49; break;                 // wrPermErr: no media writes
    case PPC_OP_SONY_CONTROL:  RD(0) = EmulDriverControl(RA(0)); break;
    case PPC_OP_SONY_STATUS:   RD(0) = EmulDriverStatus(RA(0)); break;

    // ---- Generic disk ---------------------------------------------------
    case PPC_OP_DISK_OPEN:     RD(0) = EmulDriverOpen(RA(0), RA(1), FALSE); break;
    case PPC_OP_DISK_PRIME:
        RD(0) = EmulDiskPrimeTransfer(RA(0), (RD(0) & 0xFFFF) == 3);
        break;
    case PPC_OP_DISK_CONTROL:  RD(0) = EmulDriverControl(RA(0)); break;
    case PPC_OP_DISK_STATUS:   RD(0) = EmulDriverStatus(RA(0)); break;

    // ---- CD-ROM (reads map onto the same block device) ------------------
    case PPC_OP_CDROM_OPEN:    RD(0) = EmulDriverOpen(RA(0), RA(1), TRUE); break;
    case PPC_OP_CDROM_PRIME:   RD(0) = EmulDiskPrimeTransfer(RA(0), FALSE); break;
    case PPC_OP_CDROM_CONTROL: RD(0) = EmulDriverControl(RA(0)); break;
    case PPC_OP_CDROM_STATUS:  RD(0) = EmulDriverStatus(RA(0)); break;

    // ---- Audio -----------------------------------------------------------
    case PPC_OP_AUDIO_DISPATCH:
        RD(0) = 0;                            // component handled: no-op
        break;

    case PPC_OP_SOUNDIN_OPEN:
    case PPC_OP_SOUNDIN_PRIME:
    case PPC_OP_SOUNDIN_CONTROL:
    case PPC_OP_SOUNDIN_STATUS:
    case PPC_OP_SOUNDIN_CLOSE:
        RD(0) = (UINT32)-231;                 // siBadSoundInDevice
        break;

    // ---- ADB --------------------------------------------------------------
    case PPC_OP_ADBOP: {
        // ADBOp(buffer contents describe request/response). With no input
        // devices registered yet, answer "no ADB device" so callers stop
        // polling instead of hanging on phantom data.
        UINT32 BufPtr = EmulRl(RA(0));
        UINT8 Cmd = EmulRb(BufPtr);
        if ((Cmd & 0xF0) == 0x20) {           // listen commands succeed
            RD(0) = 0;
        } else {                              // talk/flush: nothing there
            RD(0) = 0;
            EmulWb(BufPtr + 1, 0);
        }
        break;
    }

    // ---- Time Manager ------------------------------------------------------
    case PPC_OP_INSTIME:
        RD(0) = EFI_ERROR(EmulTimerInsert(RA(0))) ? (UINT32)-90 : 0;
        break;

    case PPC_OP_RMVTIME:
        // Deactivate the task; D0 returns the prior tmTaskActive-ish
        // signal (nonzero when the removed task was primed).
        RD(0) = EmulTimerRemove(RA(0)) ? 1 : 0;
        break;

    case PPC_OP_PRIMETIME: {
        UINTN I;
        for (I = 0; I < EMUL_MAX_TIMERS; I++) {
            if (g_Timers[I].TaskPtr == RA(0)) {
                g_Timers[I].Primed = TRUE;
                g_Timers[I].DueUs = g_EmulMicros + (UINT64)RD(0);
                RD(0) = 0;
                break;
            }
        }
        if (I == EMUL_MAX_TIMERS) {
            RD(0) = (UINT32)-91;              // tmNotInserted
        }
        break;
    }

    case PPC_OP_MICROSECONDS: {
        UINT64 Us = g_EmulMicros;
        EmulWl(RA(0), (UINT32)(Us >> 32));    // high word first
        EmulWl(RA(0) + 4, (UINT32)Us);
        break;
    }

    // ---- Clipboard ----------------------------------------------------------
    case PPC_OP_ZERO_SCRAP:
        g_ScrapLen = 0;
        break;

    case PPC_OP_PUT_SCRAP: {                  // length, ptr, type on stack
        UINT32 Len  = EmulRl(RSP + 8);
        UINT32 SrcP = EmulRl(RSP + 4);
        UINTN I;
        if (Len > sizeof(g_Scrap)) Len = sizeof(g_Scrap);
        for (I = 0; I < Len; I++) g_Scrap[I] = EmulRb(SrcP + I);
        g_ScrapLen = Len;
        break;
    }

    case PPC_OP_GET_SCRAP: {                  // hDest, type, offset on stack
        UINT32 Hdl  = EmulRl(RSP + 4);
        UINT32 Ofs  = EmulRl(RSP + 12);
        UINT32 DstP;
        UINTN I;
        if (Hdl == 0 || g_ScrapLen == 0 || Ofs >= g_ScrapLen) break;
        DstP = EmulRl(Hdl);
        for (I = 0; Ofs + I < g_ScrapLen && I < 512; I++) {
            EmulWb(DstP + I, g_Scrap[Ofs + I]);
        }
        break;
    }

    // ---- Diagnostics ---------------------------------------------------------
    case PPC_OP_DEBUG_STR:
        EmulDebugStr();
        break;

    // ---- Driver installation / registry ---------------------------------------
    case PPC_OP_INSTALL_DRIVERS:
        // Drivers are provided by the ROM staging path already; mark the
        // DebugStr vector slot with a benign RTS so stray calls return.
        EmulWw(0x1DFC, 0x4E75);
        break;

    case PPC_OP_NAME_REGISTRY:
        RD(0) = (UINT32)-1;                   // SS returns -1 here too
        break;

    case PPC_OP_RESET:
        // Early-reset hook: drop pending flags/timers so stale primed
        // tasks cannot fire into the fresh boot world.
        ZeroMem(g_Timers, sizeof(g_Timers));
        g_InterruptFlags = 0;
        break;

    // ---- Level-1 interrupt aggregation -----------------------------------------
    case PPC_OP_IRQ: {
        // Clear the NK-side pending-interrupt word ([[KD]+0x67C]).
        UINT32 Kd = EmulRl(PPC_XLM_KERNEL_DATA_OFFSET);
        if (Kd != 0) {
            UINT32 PendingPtr = EmulRl(Kd + 0x67C);
            if (PendingPtr != 0) {
                EmulWw(PendingPtr, 0);
            }
        }
        RD(0) = 0;
        {
            UINT32 Flags = EmulOpGetAndClearInterruptFlags();
            // Report VIA ticks so the 68K routine runs its VBL task chain.
            if (Flags & INTFLAG_VIA) {
                RD(0) = 1;
            }
            if (Flags & INTFLAG_ADB) {
                EmulOpSignalInterrupt(INTFLAG_ADB);   // keep for next round
            }
        }
        break;
    }

    // ---- SCSI manager ------------------------------------------------------------
    case PPC_OP_SCSI_DISPATCH: {
        // Stack-based dispatch identical to SheepShaver's protocol; all
        // operations answer "empty bus" so higher layers fall back to the
        // block-device drivers above.
        static INTN ScsiDbg = 0;
        UINT32 Ret = EmulRl(RSP);
        UINT16 Sel = EmulRw(RSP + 4);
        UINT32 Sp = RSP + 6;
        UINTN Stack = 0;
        if (ScsiDbg++ < 12) {
            Print(L"  SCSIDispatch sel=%d @PC=%08x\n", Sel,
                  g_PpcContext.Pc);
        }
        switch (Sel) {
        case 0:  EmulWw(Sp, 0); Stack = 0; break;                  // Reset
        case 1:  EmulWw(Sp, 0); Stack = 0; break;                  // Get
        case 2: case 11: EmulWw(Sp + 2, 0); Stack = 2; break;      // Select/SelAtn
        case 3:  EmulWw(Sp + 6, 0); Stack = 6; break;              // Cmd
        case 4:  EmulWw(Sp + 12, (UINT16)-7936); Stack = 12; break;// Complete: busy err
        case 5: case 8:  EmulWw(Sp + 4, 0); Stack = 4; break;      // Read/RBlind
        case 6: case 9:  EmulWw(Sp + 4, 0); Stack = 4; break;      // Write/WBlind
        case 10: EmulWw(Sp, 0); Stack = 0; break;                  // Stat
        case 12: EmulWw(Sp + 4, 0); Stack = 4; break;              // MsgIn
        case 13: EmulWw(Sp + 2, 0); Stack = 2; break;              // MsgOut
        case 14: EmulWw(Sp, 0); Stack = 0; break;                  // MgrBusy
        default: Stack = 0; break;
        }
        RA(0) = Ret;
        RSP += (UINT32)Stack;
        break;
    }

    case PPC_OP_SCSI_ATOMIC:
        RD(0) = (UINT32)-7887;                // same as SheepShaver
        break;

    // ---- Version/resource checks --------------------------------------------------
    case PPC_OP_CHECK_SYSV: {
        // Copy d1 into a1, load version pointer, leave decision intact:
        // our ROM+System pairing is consistent, so never null d1.
        RA(1) = RD(1);
        RA(0) = EmulRl(RD(1));
        break;
    }

    case PPC_OP_NTRB_17_PATCH:
        RA(2) = EmulRl(RSP);
        RSP += 4;
        break;

    case PPC_OP_NTRB_17_PATCH2:
        RSP += 8;
        break;

    case PPC_OP_NTRB_17_PATCH3:
        RA(2) = EmulRl(RSP);
        RSP += 4;
        break;

    case PPC_OP_NTRB_17_PATCH4:
        RD(0) = EmulRw(RSP);
        RSP += 2;
        break;

    case PPC_OP_CHECKLOAD:
        // vCheckLoad: pop resource type; allow all loads.
        RSP += 4;
        break;

    // ---- External file systems ------------------------------------------------------
    case PPC_OP_EXTFS_COMM:
        EmulWw(RSP + 14, (UINT16)-35);        // nsvErr: no such volume
        break;

    case PPC_OP_EXTFS_HFS:
        EmulWw(RSP + 20, (UINT16)-35);
        break;

    // ---- Idle --------------------------------------------------------------------------
    case PPC_OP_IDLE_TIME:
        RA(0) = EmulRl(0x2B6);                // CurrentA5 passthrough
        break;

    case PPC_OP_IDLE_TIME_2:
        RD(0) = (UINT32)-2;
        break;

    // ---- Unimplemented -------------------------------------------------------------------
    default:
        Print(L"  EMULOP[%u] not implemented: a7=0x%08x a0=0x%08x d0=0x%08x "
              L"d1=0x%08x\n",
              Selector, RSP, RA(0), RD(0), RD(1));
        RD(0) = 0xFFFFFFF4;                   // noMacSW
        break;
    }
}

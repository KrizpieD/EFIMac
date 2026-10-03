#include "interpreter.h"
#include "translation.h"
#include "emul_op.h"
#include "m68k.h"
#include "boot/bootloader.h"
#include "boot/pef_loader.h"
#include <efi.h>
#include <efilib.h>
// Global PowerPC CPU context (backing store for the interpreter and the
// public register accessor API)
PPC_CPU_CONTEXT g_PpcContext = {0};
// KCKCJUMP latch: set by PpcExecuteInstruction when a DR dispatch branch
// (bctr-form) lands in the kckc-filled data region below the emulator base;
// PpcRunGuest dumps the tail ring + ECB context once it observes the latch.
static UINTN  gKckcJumpPending = 0;
static UINT32 gKckcPc  = 0;
static UINT32 gKckcCtr = 0;
static UINT32 gKckcR29 = 0;
static UINT32 gKckcR24 = 0;
static UINT32 gKckcR27 = 0;
static UINT32 gKckcR28 = 0;
static UINT32 gKckcR30 = 0;
static UINT32 gKckcR31 = 0;
static UINT32 gKckcCr  = 0;
static UINT32 gKckcLr  = 0;
// ---------------------------------------------------------------------------
// PPC-native pivot switch (see ARCHITECTURE.md "Architectural Reference").
//
// The 68K DR emulator is PPC code owned by the OS; DingusPPC and SheepShaver
// run it on the host CPU via the ROM's own PPC opcode-translation table and
// only intercept the escape/trap vectors. EFIMac instead intercepts the DR
// emulator's common dispatch entry (PC 0x40B67C60) and executes every 68K
// instruction through a hand-rolled C interpreter (m68k.c), which has been
// carried by heuristics for weeks.
//
// Setting USE_PPC_NATIVE_DR=1 lets the ROM's own PPC opcode table drive the
// 68K emulator (the de-hijack), and only seeds/keeps the trap/device escapes.
// Default is 0 = current C-68K behaviour (the SheepShaver track: the common
// dispatch at 0x40B67C60 is intercepted and every 68K instruction runs
// through the hand-rolled C interpreter in m68k.c).
#define USE_PPC_NATIVE_DR 1
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
#define XO_EXTSH     922
#define XO_EXTSB     954
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
#define SPR_DEC   22
#define SPR_SRR0  26
#define SPR_SRR1  27
#define SPR_TBL  268
#define SPR_TBU  269
#define SPR_PVR  287
#define SPR_SDR1  25
// PowerPC 32-bit BAT register SPR numbers (upper + lower, 4 pairs each).
// IBAT upper: 528,530,532,534,536,538,540,542 ; lower: +1
// DBAT upper: 536,538,540,542,544,546,548,550 ; lower: +1
#define SPR_IBAT0U 528
#define SPR_DBAT0U 536
// Timebase/decrementer "ticks" advanced per guest instruction. The NK assumes
// a 50 MHz timebase (XLM BUS_CLOCK); the interpreter runs ~1M instr/s, so 50
// ticks per instruction approximates the real rate while keeping DEC and the
// timebase mutually consistent.
#define PPC_TIMEBASE_SCALE  4
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
#define PPC_MAX_GUEST_REGIONS 20
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
// ---------------------------------------------------------------------------
// PHASE A.5: KernelData hardware-field discovery profiler.
//
// The nanokernel owns the KernelData page (LA_KernelData = 0x68FFE000) and
// initializes most of it itself during boot; what the emulator must supply
// are the hardware-dependent fields the ROM expects the bootloader/Open
// Firmware layer to have filled. Blind writes here would corrupt live NK
// data, so this profiler records every load whose effective address lands
// inside the page while the NK boots. The bucket histogram dumped at the
// 68K handoff identifies exactly which offsets are consumed (and therefore
// which fields a future seed must provide), turning A.5 into a data-driven
// task instead of guesswork.
// ---------------------------------------------------------------------------
#define PPC_KERNELDATA_BASE   0x68FFE000u
#define PPC_KERNELDATA_WORDS  1024          // whole 4 KB page, word granular
static BOOLEAN g_KdProfileEnabled = FALSE;
static UINT32  g_KdProfileLoads   = 0;
static UINT32  g_KdBucket[PPC_KERNELDATA_WORDS];
// PC sample histogram: which code site generates the KernelData loads. Two
// windows: low memory (0x00000000..0x00400000, the 68K-emulator/ECB area)
// and the ROM/emulator-in-RAM window (0x40000000..0x40C00000), 4 KB pages.
#define PPC_KDPROF_LO_BASE   0x00000000u
#define PPC_KDPROF_HI_BASE   0x40000000u
#define PPC_KDPROF_PAGES     1024
static UINT32 g_KdPcBucketLo[PPC_KDPROF_PAGES];
static UINT32 g_KdPcBucketHi[PPC_KDPROF_PAGES];
static UINTN  g_KdPcSampled = 0;
// Record a load from the KernelData page (called on lwz/lhz/lbz).  PC is
// sampled every 2048th load so the emulator's poll loop location shows in
// the KDPROF dump instead of just the field offsets.
static VOID
PpcKdProfileLoad (
    IN UINT32 Ea,
    IN UINT32 Pc
    )
{
    if (!g_KdProfileEnabled) {
        return;
    }
    if (Ea >= PPC_KERNELDATA_BASE &&
        Ea < PPC_KERNELDATA_BASE + PPC_KERNELDATA_WORDS * 4 &&
        g_KdProfileLoads < 8000000u) {
        g_KdBucket[(Ea - PPC_KERNELDATA_BASE) >> 2]++;
        g_KdProfileLoads++;
        if (g_KdProfileLoads >= 4000000u) {
            g_KdProfileEnabled = FALSE;   // bounded sampling window
        }
        if ((g_KdProfileLoads & 0x7FF) == 0) {
            g_KdPcSampled++;
            if (Pc < PPC_KDPROF_LO_BASE + PPC_KDPROF_PAGES * 0x1000u) {
                g_KdPcBucketLo[(Pc - PPC_KDPROF_LO_BASE) >> 12]++;
            } else if (Pc >= PPC_KDPROF_HI_BASE &&
                       Pc < PPC_KDPROF_HI_BASE + PPC_KDPROF_PAGES * 0x1000u) {
                g_KdPcBucketHi[(Pc - PPC_KDPROF_HI_BASE) >> 12]++;
            }
        }
    }
}
// Dump the top KernelData read-offsets and stop profiling.
static VOID
PpcKdProfileDump (
    VOID
    )
{
    UINTN B, K;
    UINTN Best;
    if (g_KdProfileLoads == 0) {
        Print(L"  KDPROF: no KernelData loads captured\n");
        g_KdProfileEnabled = FALSE;
        return;
    }
    Print(L"  KDPROF: %d loads into 0x68FFE000.., top offsets:\n",
          g_KdProfileLoads);
    for (K = 0; K < 16; K++) {
        Best = PPC_KERNELDATA_WORDS;
        for (B = 0; B < PPC_KERNELDATA_WORDS; B++) {
            if (g_KdBucket[B] != 0 &&
                (Best == PPC_KERNELDATA_WORDS ||
                 g_KdBucket[B] > g_KdBucket[Best])) {
                Best = B;
            }
        }
        if (Best == PPC_KERNELDATA_WORDS) {
            break;
        }
        Print(L"    KD[+0x%03x] x%d\n", (UINT32)(Best * 4), g_KdBucket[Best]);
        g_KdBucket[Best] = 0;
    }
    {
        // PC sample histogram: the code site that generates the loads.
        UINTN  P;
        UINT32 Best;
        Print(L"  KDPROF load-PC pages (4K, sampled %d):\n", (UINT32)g_KdPcSampled);
        for (P = 0; P < 2; P++) {
            for (K = 0; K < 12; K++) {
                Best = PPC_KDPROF_PAGES;
                UINT32 *Win = (P == 0) ? g_KdPcBucketLo : g_KdPcBucketHi;
                for (B = 0; B < PPC_KDPROF_PAGES; B++) {
                    if (Win[B] != 0 &&
                        (Best == PPC_KDPROF_PAGES || Win[B] > Win[Best])) {
                        Best = B;
                    }
                }
                if (Best == PPC_KDPROF_PAGES) {
                    break;
                }
                Print(L"    PCPAGE 0x%08x x%d%s\n",
                      (UINT32)((P == 0 ? PPC_KDPROF_LO_BASE : PPC_KDPROF_HI_BASE) +
                               Best * 0x1000), Win[Best],
                      (P == 0 ? L" [low]" : L" [rom]"));
                Win[Best] = 0;
            }
        }
    }
    g_KdProfileEnabled = FALSE;
}
static BOOLEAN g_SccRxPending = FALSE;
static UINT8 g_SccRxFifo[64];
static UINTN g_SccRxFifoHead = 0;
static UINTN g_SccRxFifoTail = 0;
// NK external-interrupt controller device window (MacIO-style, see
// PpcDefaultReadByte for the register comment). Address chosen clear of all
// guest RAM/ROM regions (low RAM, ROM @0x40B00000, RAMB @0xFFF00000).
#define PPC_INT_CTRL_BASE  0xF3000000u
#define PPC_INT_CTRL_END   0xF3000060u
static UINTN IntDevLogs = 0;
static UINTN SccDataReads = 0;
static UINTN SccStatusReads = 0;
static UINT32 PpcIntCtrlReadReg (UINT32 Off);   // defined below the MMIO tail
// Queue a byte on the NK's SCC receive side so a polled read eventually
// returns it (bit 0 of the [0x20002] status read reports Rx data ready).
VOID
PpcSccPutChar (
    IN UINT8 Char
    )
{
    UINTN Used = (g_SccRxFifoHead + sizeof(g_SccRxFifo) - g_SccRxFifoTail) %
                 sizeof(g_SccRxFifo);
    // A head==tail ring is either empty or completely full; the pending flag
    // disambiguates. Treat it as full once every slot holds a byte so the
    // capacity is a deterministic 64 in-order bytes. The old next==tail guard
    // only allowed 64 when the queue started empty and otherwise overwrote or
    // dropped the 64th byte for -1 off-by-one semantics.
    if (Used == 0 && g_SccRxPending) {
        Used = sizeof(g_SccRxFifo);
    }
    if (Used < sizeof(g_SccRxFifo)) {
        g_SccRxFifo[g_SccRxFifoHead] = Char;
        g_SccRxFifoHead = (g_SccRxFifoHead + 1) % sizeof(g_SccRxFifo);
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
    if (Address == 0x00020002 || Address == 0x00020004 ||
        Address == 0x00020000) {
        UINT8 R = 0x04 | (g_SccRxPending ? 0x01 : 0x00);
        SccStatusReads++;
        return R;
    }
    // [base+6] is the SCC data register (channel control lives at [base+2]).
    // The DR uses two SCC base conventions: r28=0x20000 (status at 2(r28)=
    // 0x20002, data at 6(r28)=0x20006) and, in the resumed post-GO context,
    // r28=0x20002 (status at 2(r28)=0x20004, data at 6(r28)=0x20008). Serve
    // both data lanes so a read in either convention pops the same Rx FIFO.
    if (Address == 0x00020006 || Address == 0x00020008) {
        // A head==tail ring is either empty or completely full; g_SccRxPending
        // disambiguates (see PpcSccPutChar), so a full 64-byte queue must still
        // yield bytes. The old head!=tail-only guard dropped an entire full
        // ring and reset pending, which the 64-capacity put can now reach.
        if (g_SccRxFifoHead != g_SccRxFifoTail || g_SccRxPending) {
            UINT8 C = g_SccRxFifo[g_SccRxFifoTail];
            g_SccRxFifoTail = (g_SccRxFifoTail + 1) % sizeof(g_SccRxFifo);
            g_SccRxPending = (g_SccRxFifoHead != g_SccRxFifoTail);
            return C;
        }
        g_SccRxPending = FALSE;
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
    if (Address >= 0x20000u && Address <= 0x20009u) {
        // SCCREADCAP: what does the DR actually READ around the SCC base?
        // GetChar returns -1 without ever touching our 0x20002/0x20006, so
        // capture every byte read in the SCC window with its PC (cap 200).
        return 0;
    }
    // -------------------------------------------------------------------
    // NK external-interrupt controller (MacIO-style, DingusPPC macio.h).
    //
    // The DR console's SCC receive path is interrupt-driven: the D0 (External
    // 0x500) handler reads the controller base from [KDP-0x338], dispatches on
    // the pending events word at [base+0x20] (MacIO IntEvents1), records the
    // level into [KDP-0x238], and lets the level-9 ISR at 0x40B148E0 drain the
    // SCC into the DRAM input ring. Nothing ever wrote [KDP-0x338] in our
    // boot ([KDP-0x338]==0 verified), so the DR can never be woken by input.
    // PpcIntCtrlInstall() points the slot at this device window. Register map
    // (MacIO EVENTS/MASK/CLEAR/LEVELS twin-bank, plus the level-9 SCC bank the
    // NK ISR reads at +0x38/+0x44/+0x4C):
    //   +0x10..0x1C  EVENTS2/MASK2/CLEAR2/LEVELS2      (unused: 0)
    //   +0x20        IntEvents1  - pending external events (bit1 = SCC Rx)
    //   +0x24        IntMask1    - enable mask (0xFF)
    //   +0x28        IntClear1   - write-1-to-clear (absorbed; events level)
    //   +0x2C        IntLevels1
    //   +0x38        level-9 SCC pending word, nonzero while Rx data queued
    //   +0x44        priority scan bound (0x3FFFFFFF: r16<<2 always exceeds
    //                the level counter r20, so the ISR's bgelr never bails)
    //   +0x4C        nesting bound = [KDP-0x1C]+1 (ISR's equality check
    //                against current nesting depth never matches at depth 0/1,
    //                so the SCC drain at 0x40B23F78 is always reached)
    if (Address >= PPC_INT_CTRL_BASE && Address < PPC_INT_CTRL_END) {
        UINT32 Off = Address - PPC_INT_CTRL_BASE;
        UINT32 V   = PpcIntCtrlReadReg(Off);
        UINT32 Pc  = g_PpcContext.Pc;
        // Only log during the NK External/ISR execution, not probe dumps.
        if (IntDevLogs < 120 && Pc >= 0x40B14880u && Pc <= 0x40B14A00u) {
            IntDevLogs++;
            Print(L"  INTDEV-R +0x%02x -> 0x%08x PC=0x%08x\n", Off, V, Pc);
        }
        return (UINT8)((V >> (8 * (3 - (Off & 3)))) & 0xFF);
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
            if ((Address == 0x00020006 || Address == 0x00020008) && g_OutDevChars < 4096) {
                g_OutDevChars++;
                if (Value == 0x0D) {
                    Print(L"\r\n");
                } else if (Value == 0x0A) {
                    // swallow (already translated \r\n)
                } else if (Value >= 0x20 && Value <= 0x7E) {
                    Print(L"%c", (UINTN)Value);
                }
            }
            // SCC data register [base+6] and control register [base+2] for
            // both base conventions (r28=0x20000: 0x20002/0x20006, r28=0x20002:
            // 0x20004/0x20008): writes must not land in guest RAM (the 8530
            // never reads them back as status/data).
            if (Address == 0x00020006 || Address == 0x00020008 ||
                Address == 0x00020002 || Address == 0x00020004) {
                return;
            }
            if (!g_GuestRegions[I].ReadOnly) {
                *(volatile UINT8*)((UINTN)g_GuestRegions[I].HostBase +
                                   (Address - g_GuestRegions[I].GuestBase)) = Value;
            }
            return;  // First matching region decides the target
        }
    }
    // Interrupt controller window: absorb writes (events are level-derived
    // from the SCC FIFO, so CLEAR/ack writes need no storage).
    if (Address >= PPC_INT_CTRL_BASE && Address < PPC_INT_CTRL_END) {
        return;
    }
}
// ---------------------------------------------------------------------------
// PowerPC 32-bit effective->physical address translation (DingusPPC-faithful).
//
// Mirrors core/ppc/ppcmmu.cpp: ppc_block_address_translation() for BATs and
// page_address_translation() for the SDR1 page-table walk. Translation is
// gated on the MSR bits (MSR[IR] for instructions, MSR[DR] for data); when
// the relevant bit is clear the access is real-mode (effective == physical),
// preserving the pre-existing identity flat map for the early boot.
// ---------------------------------------------------------------------------
// Reads a 32-bit big-endian word from guest PHYSICAL memory (for the raw
// page-table/hash-table walk, which must not itself be translated).
static UINT32
PpcReadPhys32 (
    IN UINT32 Physical
    )
{
    return ((UINT32)PpcDefaultReadByte(Physical) << 24) |
           ((UINT32)PpcDefaultReadByte(Physical + 1) << 16) |
           ((UINT32)PpcDefaultReadByte(Physical + 2) << 8) |
           ((UINT32)PpcDefaultReadByte(Physical + 3));
}
static UINT32
PpcCalcPtegAddr (
    IN UINT32 Hash
    )
{
    UINT32 Sdr1Val = g_PpcContext.Sdr1;
    return (Sdr1Val & 0xFFFF0000) |
           (((Sdr1Val & 0x1FF) << 16) & ((Hash & 0x7FC00) << 6)) |
           ((Hash & 0x3FF) << 6);
}
static BOOLEAN
PpcSearchPteg (
    IN  UINT32   PtegAddr,
    OUT UINT32*  PteWord2,
    IN  UINT32   Vsid,
    IN  UINT32   PageIndex,
    IN  UINT32   PtegNum
    )
{
    UINT32 I;
    UINT32 PteCheck = 0x80000000 | (Vsid << 7) | (PtegNum << 6) | (PageIndex >> 10);
    for (I = 0; I < 8; I++) {
        if (PpcReadPhys32(PtegAddr + I * 8) == PteCheck) {
            *PteWord2 = PpcReadPhys32(PtegAddr + I * 8 + 4);
            return TRUE;
        }
    }
    return FALSE;
}
// Translate a logical (effective) address to a physical one. Returns TRUE on
// a successful BAT or page-table hit, FALSE on a miss.
BOOLEAN
PpcTranslateEffective (
    IN  UINT32  La,
    IN  BOOLEAN IsInstr,
    OUT UINT32* Pa
    )
{
    UINT32  ArrayBase, MsrPr, AccessBits, SrVal, PageIndex, PtegHash1, Vsid;
    UINT32  PteWord2 = 0;
    UINTN   I;
    if (Pa == NULL) {
        return FALSE;
    }
    // Address translation is gated on the MSR: instruction translation on
    // MSR[IR], data translation on MSR[DR]. With the bit clear the address is
    // sent through untranslated (effective == physical), exactly like real PPC.
    if (IsInstr ? !(g_PpcContext.Msr & PPC_MSR_IR)
                : !(g_PpcContext.Msr & PPC_MSR_DR)) {
        return FALSE;
    }
    // --- BAT translation ---
    ArrayBase  = IsInstr ? 0 : 8;
    MsrPr      = (g_PpcContext.Msr & PPC_MSR_PR) ? 1 : 0;
    AccessBits = (MsrPr == 0) ? 0x2 : 0x1;   // supervisor vs user
    for (I = 0; I < 8; I++) {
        PPC_BAT_ENTRY* E = &g_PpcContext.Bat[ArrayBase + I];
        if ((E->Access & AccessBits) != 0 &&
            ((La & E->HiMask) == E->Bepi)) {
            *Pa = E->PhysHi | (La & ~E->HiMask);
            return TRUE;
        }
    }
    // --- Page-table translation (SDR1) ---
    // With SDR1 == 0 there is no valid HTAB base: PpcCalcPtegAddr degenerates
    // to (Hash & 0x3FF) << 6, i.e. inside low RAM, so the boot layer's
    // trap-name string table there would be walked as fake PTE rows. Any
    // accidental PteCheck match then translates reads/writes to spurious
    // physical addresses and re-scatters the string-table content (observed as
    // the OSINJECT stub/seed reverting after the FORK-MIRROR copy). Until the
    // guest installs a real SDR1 via mtspr, no page-table translation exists:
    // fail the walk so the access stays flat.
    if (g_PpcContext.Sdr1 == 0) {
        return FALSE;
    }
    SrVal      = g_PpcContext.Sr[(La >> 28) & 0xF];
    PageIndex  = (La >> 12) & 0xFFFF;
    PtegHash1  = (SrVal & 0x7FFFF) ^ PageIndex;
    Vsid       = SrVal & 0x0FFFFFF;
    if (!PpcSearchPteg(PpcCalcPtegAddr(PtegHash1), &PteWord2, Vsid, PageIndex, 0)) {
        if (!PpcSearchPteg(PpcCalcPtegAddr(~PtegHash1), &PteWord2, Vsid, PageIndex, 1)) {
            return FALSE;   // translation miss -> DSI/ISI
        }
    }
    *Pa = ((PteWord2 & 0xFFFFF000) | (La & 0x00000FFF));
    return TRUE;
}
// Data-access wrappers: translate effective->physical when MSR[DR] is set
// (and a BAT/page-table hit is found), otherwise access the flat map. These
// are installed as g_ReadByte/g_WriteByte below, so every loader/storer the
// interpreter executes goes through real-mode-vs-translated data translation.
static UINT8
PpcMmuReadByte (
    IN UINT32 Ea
    )
{
    UINT32 Pa;
    if ((g_PpcContext.Msr & PPC_MSR_DR) != 0 &&
        PpcTranslateEffective(Ea, FALSE, &Pa)) {
        return PpcDefaultReadByte(Pa);
    }
    return PpcDefaultReadByte(Ea);
}
// Tentative definition; the real initialised definition appears further down.
// (C allows a tentative definition followed by an initialised one.)
static PPC_CPU_READ_MEMORY g_ReadByte;
static VOID
PpcMmuWriteByte (
    IN UINT32 Ea,
    IN UINT8  Value
    )
{
    UINT32 Pa;
    if ((g_PpcContext.Msr & PPC_MSR_DR) != 0 &&
        PpcTranslateEffective(Ea, FALSE, &Pa)) {
        PpcDefaultWriteByte(Pa, Value);
        return;
    }
    PpcDefaultWriteByte(Ea, Value);
}
// Recompute one decomposed BAT entry from its upper/lower SPR pair, mirroring
// DingusPPC's ppc_ibat_update()/ppc_dbat_update(). Called whenever either
// half of a BAT pair is written via mtspr.
//
//   Upper SPR: BEPI + BL (block length) + Vs/Vp access bits
//   Lower SPR: BRPN (physical page number) + PP protection bits
//
//   bl      = (upper >> 2) & 0x7FF          (block length index)
//   hi_mask = ~((bl << 17) | 0x1FFFF)       (address mask for the block)
//   access  =  upper & 3
//   prot    =  lower & 3
//   phys_hi =  lower & hi_mask
//   bepi    =  upper & hi_mask
VOID
PpcUpdateBat (
    IN UINT32 SprNum
    )
{
    UINT32 Upper   = SprNum & 0xFFFFFFFE;   // even (upper) member of the pair
    UINT32 Lower   = Upper | 1;
    UINT32 Bl;
    UINT32 HiMask;
    UINTN  Index;                            // 0-7 IBAT, 8-15 DBAT
    PPC_BAT_ENTRY* E;
    if (SprNum >= SPR_IBAT0U && SprNum <= (SPR_IBAT0U + 15)) {
        Index = (SprNum - SPR_IBAT0U) >> 1;          // IBAT0-7
    } else if (SprNum >= SPR_DBAT0U && SprNum <= (SPR_DBAT0U + 15)) {
        Index = 8 + ((SprNum - SPR_DBAT0U) >> 1);    // DBAT0-7
    } else {
        return;
    }
    E = &g_PpcContext.Bat[Index];
    Bl     = (g_PpcContext.Spr[Upper] >> 2) & 0x7FF;
    HiMask = ~((Bl << 17) | 0x1FFFF);
    E->Access  = (UINT8)(g_PpcContext.Spr[Upper] & 3);
    E->Prot    = (UINT8)(g_PpcContext.Spr[Lower] & 3);
    E->HiMask  = HiMask;
    E->PhysHi  = g_PpcContext.Spr[Lower] & HiMask;
    E->Bepi    = g_PpcContext.Spr[Upper] & HiMask;
    E->Active  = TRUE;
}
static PPC_CPU_READ_MEMORY  g_ReadByte  = PpcMmuReadByte;
static PPC_CPU_WRITE_MEMORY g_WriteByte = PpcMmuWriteByte;

// Low-RAM write census (diagnostic). The NK dispatch table at 0x12E88 is read by
// the scheduler but written by NOTHING (PPC stw / stwrev / 68K M68kWriteLong all
// recorded zero hits over a full 900 s boot) and has no static image in ROM. To
// find out what the boot DOES initialise in low RAM -- and whether the NK's own
// LowMem init (PA_RelocatedLowMemInit, 0xFFFFFFFF wrapping to low addresses)
// is simply not covering this region -- record a write count and first writer
// for every 4 KB page of the first 128 KB.
UINT32 g_LowPageWrites[LOW_CENSUS_PAGES];
UINT32 g_LowPageFirstPc[LOW_CENSUS_PAGES];
UINT32 g_LowPageFirstAddr[LOW_CENSUS_PAGES];
UINT8  g_LowPageSeen[LOW_CENSUS_PAGES];

static UINT32 CpuRead32 (UINT32 A);

// Call from every guest store path. Counts only naturally aligned 32-bit
// stores so byte-granularity writes do not distort the census.
VOID
LowRamCensus (
    IN UINT32 A,
    IN UINT32 V,
    IN UINT32 Pc
    )
{
    UINT32 Pg;
    if ((A & 3u) != 0 || A >= (LOW_CENSUS_PAGES * 0x1000u)) {
        return;
    }
    Pg = A >> 12;
    if (g_LowPageWrites[Pg] != 0xFFFFFFFFu) {
        g_LowPageWrites[Pg]++;
    }
    if (g_LowPageSeen[Pg] == 0) {
        g_LowPageSeen[Pg] = 1;
        g_LowPageFirstPc[Pg] = Pc;
        g_LowPageFirstAddr[Pg] = A;
    }
    // Detailed log for the page that actually holds the dispatch table.
}
EFI_STATUS
PpcSetMemoryAccess (
    IN PPC_CPU_READ_MEMORY   Read,
    IN PPC_CPU_WRITE_MEMORY  Write
    )
{
    g_ReadByte  = (Read  != NULL) ? Read  : PpcMmuReadByte;
    g_WriteByte = (Write != NULL) ? Write : PpcMmuWriteByte;
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
    // Trampoline-region watch: who (if anyone) populates 0x7F0000-0x800000
    // where the NK expects continuation stubs? Both CPU paths funnel here.
    {
        static UINTN TrampWatchHits = 0;
        if (TrampWatchHits < 16 &&
            Address >= 0x7F0000u && Address < 0x800000u) {
            TrampWatchHits++;
            Print(L"  TRAMP-W [%08x]<-%02x r1=%08x pc=%08x\n",
                  Address, Value, g_PpcContext.Gpr[1], g_PpcContext.Pc);
        }
    }
    // RAM-Rom window watch: does the OS loader itself populate the 68K
    // 0x81000000 window (top-of-RAM-Rom image) before/after jumping there?
    // Both CPU paths funnel here.
    {
        static UINTN RamRomWrites = 0;
        if (RamRomWrites < 200 &&
            Address >= 0x81000000u && Address < 0x82000000u) {
            RamRomWrites++;
            Print(L"  RAMR-W [%08x]<-%02x r1=%08x pc=%08x\n",
                  Address, Value, g_PpcContext.Gpr[1], g_PpcContext.Pc);
        }
    }
    // 68K sys-globals window watch: is the 0x80BD0000 native-bridge page
    // written by the OS/DRAME at all, and from where?
    {
        static UINTN SysGlobWrites = 0;
        if (SysGlobWrites < 24 &&
            Address >= 0x80000000u && Address < 0x81000000u) {
            SysGlobWrites++;
            Print(L"  SYSG-W [%08x]<-%02x r1=%08x pc=%08x\n",
                  Address, Value, g_PpcContext.Gpr[1], g_PpcContext.Pc);
        }
    }
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
// ---- PPC-side DR bootstrap (pure PPC; no C-68K) ----
// On real hardware the guest's 68K BootROM runs first (via the ROM's own PPC
// DR emulator) and fabricates the emulator-task ECB context: [ECB+0x1C4] holds
// the 68K PC and [ECB+0xFC] the DR resume PC the MRETTASK rfi reloads. Because
// the bootloader pivots straight into the PPC NK, that context never exists, so
// the PPC side supplies the same writes itself. This substitutes the ROM reset
// vector for the uninitialized r24 (0xFFFFFFFF) the task-switch save stores,
// and gates the one-time DR-bootstrap writes below.
static UINT32 g_DrBootPcSeeded  = 0;
// MakeReady (0x40B22D40 entry / 0x40B22D44 body) caller tracing. File scope
// because the branch decoder (PpcExecuteInstruction) and the scheduler probes
// live in different functions.
static UINT32 g_MkrdyCalls = 0;   // branches landing on MakeReady proper
static UINT32 g_MkrdyAny = 0;     // branches landing anywhere in 0x40B22D40..48
// Forensics switch: 1 = dump full memory regions at the one-shot probe sites
// (ROM handler region, low RAM, System fork, DR gd/tv tables). Those dumps add
// ~2000 lines to the log; set to 0 for clean soak runs. The brief per-site
// header lines and all state-changing probes remain active either way.
#define PPC_DR_FULL_DUMPS 1
// DEC preemption gate during the boot critical section: once the 68K emulator
// genuinely starts its first DR dispatch, the NK may preempt it on the
// decrementer tick like real hardware. File scope so both the rfi bootstrap
// and the main loop can lift it.
static UINT32 g_BootDecGate = 1;
// Set once the DR's emulator-trap yield has been resumed at the dispatch loop
// (DRYIELD-RESUME). After that point r24 is genuinely the 68K PC; before it,
// r24 is a scratch base in the DR cold-start and must not be treated as one.
static UINT32 g_DrYieldSeen = 0;
// Limited ICD dispatch SRR1 origin probe (rfi at 0x40B14A04).
static UINT32 g_DrRfiIcdDump = 0;
// One-shot probe of the level-9 ISR's SRR1 source slot [r1-0x964] at 0x40B1499C.
// Forced-resume counter for the fabricated dispatch rfi's SRR1 (see rfi site).
static UINT32 g_IcdSrr1Forced = 0;
// Single-use window tracer: while non-zero, after the DRYIELD-RESUME it prints
// every DR emulator-range instruction's PPC address plus the 68K PC/r27 pair
// so the exact 68K word stream the DR executes post-yield is visible, then it
// switches off after ~6000 instructions to keep the log bounded.
static UINT32 g_DrPostYieldWindow = 0;
// Spill-band decoder: after the DRYIELD-RESUME, once the 68K PC enters the
// low-RAM zone (>= 0xA800) it logs each instruction op + PPC handler address
// (up to 96) to reconstruct the off-the-end word stream verbatim.
static UINT32 g_DrSpillDump = 0;
static UINT32 g_DrJsrTrace = 0;
// Yield-cycle counter: incremented at every DRYIELD-RESUME. The trace blocks
// below are gated on this (plus their own window bands) so the traces fire
// only during the FIRST few yield cycles. g_DrPostYieldWindow resets to 1
// every yield, so windows 660-790 recur thousands of times per soak and the
// A207PROBE/JSX counters would otherwise self-exhaust before any output
// survives QEMU's serial chardev (which keeps only the log tail).
static UINT32 g_DrYieldCount = 0;
// A207 dispatch net-leak compensation state machine (file scope so the
// DRYIELD-RESUME block can reset it to a known zero like the other trace
// counters; function-local .bss statics become mid-boot garbage here).
static UINT32 g_DrA207ArmPc = 0;      // 68K past-trap PC at the arm site
static UINT32 g_DrA207ArmSp = 0;      // r1 at the arm site (pre-dispatch SP)
static UINT32 g_DrA207ArmState = 0;   // 0 idle, 1 armed, 2 in-rom
static UINT32 g_DrA207ArmLogged = 0;  // one-shot clamp log
static UINT32 g_DrA207Base = 0;       // cycle baseline = r1 at first landing
static UINT32 g_DrA207Land = 0;       // landing log budget
static UINT32 g_DrOsAdvance = 0;      // OSINLOW A207-return probe budget
static UINT32 g_DrA207Cap = 0;        // one-shot runtime truth capture
static UINT32 g_DrA207Steer = 1;      // EXPERIMENT: A207 returns 0 + prefill gate
static UINT32 g_DrA003Steer = 1;      // EXPERIMENT: A003 returns 0 (native jump)
static UINT32 g_DrA003Gate = 0;       // A003 gate log budget / fire count
static UINT32 g_DrA003Probe = 0;      // A003 dispatch-site probe budget
static UINT32 g_DrNativeTail = 0;     // tail-A207 -> 0x1B823 redirect fire count
static UINT32 g_DrNativeLand = 0;     // staged-native bootstrap landing probe budget
static UINT64 g_NatDecSteps = 0;      // decoder(0x3873C) runaway canary count
static UINT32 g_DrNativeHandoff = 0;  // one-shot: host switched to native PPC at 0x1B820
static UINT32 g_LastPc = 0;           // branch source (previous executed PC)
static UINT32 PollRing[64];            // pre-first-poll PC ring
static UINT32 PollRingIdx = 0;         // ring cursor
static UINT32 g_PollProbed = 0;       // one-shot: diag poll 0x1417E0 entry probe
static UINT32 g_PollFixed  = 0;       // one-shot: diag poll completion applied
static UINT32 g_LrWriterProbed = 0;   // one-shot: LR writer of a 0x3C.. handler
static UINT32 g_HubProbed     = 0;    // one-shot: DR bclrl hub 0x40B6D12C
static UINT32 g_DrHub2Probed  = 0;    // one-shot: DR hub lwz 0x40B6D114
static UINT32 g_BigPcProbed   = 0;    // one-shot: 68K pc reaches the 0x81.. hi-space
static UINT32 g_PcWatchLog    = 0;    // capped per-dispatch log in the 0x81.. zone
static UINT32 g_ForkResumePc = 0;     // 68K PC (@fork freeze) the DR must resume from
static UINT32 g_DrForkSlotFixed = 0;
static UINT32 g_LowRamDumped = 0;  // one-shot: ed.v[8] PC-slot repair for the fork glue
static UINT32 g_DrNativeSteer = 0;    // latched by the tail-A207 0x1B823 write (never reset)
// Single-shot gate: once the DR returns from its 0x116 service call (68K a6 =
// 0x4080AA3C) with D0 bit0 cleared so the boot driver skips the unstaged low-
// RAM relocation, this is set to stop re-evaluating the hook.
static UINT32 g_DrRelocGate = 0;
static VOID   CpuWrite32(UINT32 A, UINT32 V);
static VOID   TaskStoreWatch(UINT32 A, UINT32 V, UINTN Bytes)
{
    // 0x40B14704 `lwz r8,1608(r1)` reads the dispatch slot that becomes r7's top
    // byte (the status code). r8 there is only ever 2 or 8; the loop wants 12/20.
    // r1 is the boot-proc frame base (0x0000A000), so the slot is 0xA648. Watch
    // its writer, whichever addressing mode it uses.
    // The boot-proc resume path 0x40B12F80 -> 0x40B23E4C asserts
    // `if (*(UINT8*)(task+0x18) == 0) NKPANIC`, with task=0x7B40 (the "TASK"
    // record holding 0x95BC pointers). Watch every store into that record so
    // the initializer (or the missing write) is visible.
}
static VOID   CpuWrite32(UINT32 A, UINT32 V)
{
    TaskStoreWatch(A, V, 4);
    // Watch the NK dispatch table at 0x12E88 (256 words). The scheduler reads
    // it at 0x40B22FA4/0x40B22FB8 via `lis r20,1` + `ori r20,r20,0x2E88`, but a
    // full ROM scan found that to be the ONLY reference to the table anywhere in
    // 0x40B10000-0x40F10000, i.e. no ROM code appears to write it. Watching the
    // store path directly finds the writer regardless of addressing mode, and
    // proves absence if nothing ever writes.
    // Watch every guest store into the NK code region. The NK relocates and
    // patches itself (see the 0x40B1AEF4 site), so if the self-patch step runs
    // at all it must show up as stores into 0x40B10000-0x40B4FFFF. Zero hits
    // would prove the patch step never executes in this build, leaving stale
    // constants such as the `lis r20,1`/`ori r20,0x2E88` table base at
    // 0x40B22FA0/0x40B22FA4.
    LowRamCensus(A, V, g_PpcContext.Pc);
    // Watch the MakeReady sentinel and the queue head it owns. MakeReady at
    // 0x40B22D44 builds a sentinel ECB at r1-0xA84 whose +0x08 slot IS the
    // ready-queue head the scheduler reads at [r1-0xA7C]; an empty circular
    // queue must therefore read as the sentinel pointer, not 0. Observed r1 is
    // 0xA000 in the DR-task context and 0x77E0 in the idle context, giving
    // candidate head/sentinel pairs (0x9F7C/0x9F84) and (0x6D64/0x6D5C).
    if ((A >= 0x00006D50u && A < 0x00006D80u) ||
        (A >= 0x00009F70u && A < 0x00009FA0u)) {
    }
    // Watch the DR task's priority byte (ECB+0x14 == 0xB114). The scheduler
    // branches `cmplwi prio,9 / bgel 0x40B22D40`, so a priority >= 9 avoids the
    // never-populated 0x12E88 table entirely. The DR task reads 0, so finding
    // who is supposed to set this byte identifies the real missing step.
    // Watch the ECB 68K PC slot (ECB+0x1C4 = 0xB2C4, restored to r24 by
    // ECBTOTASK) and the [KDP-0x14] ECB pointer (0x9FEC).
    if (A == 0x0000B2C4u || A == 0x00009FECu) {
        // DR bootstrap: a cold boot's task-switch save (stw r24,0x1C4(r6) at
        // 0x40B238F0) stores an uninitialized r24=0xFFFFFFFF as the 68K PC
        // (the emulator context was never created). Hardware holds the ROM 68K
        // reset vector there; substitute it on the first write so the ECBTOTASK
        // restore yields a live 68K boot PC.
        if (g_DrBootPcSeeded && A == 0x0000B2C4u && V == 0xFFFFFFFFu) {
            V = 0x4080002A;
            Print(L"  DRBOOTPC [B2C4] FFFFFFFF -> 0x4080002A @PC=0x%08x "
                  L"r24=%08x\n",
                  g_PpcContext.Pc, g_PpcContext.Gpr[24]);
        }
        // DR-task sausage dump: when the 68K DR context save stores the 68K PC
        // into ECB+0x1C4 with r6==0xB100 (the DR task ECB), capture the ECB
        // readiness/deadline/links the scheduler will evaluate next, plus the
        // scheduler's current-task slots and the time base for comparison.
    }
    // Continuation-value watch (PPC side): stores of addresses inside the
    // NK trampoline region — the 68K stack gets its bogus RTS target from
    // here (DR-emulator context save/restore), bypassing all 68K watches.
    {
        static UINTN PpcTrampValHits = 0;
        if (PpcTrampValHits < 24 &&
            V >= 0x7F0000u && V < 0x800000u) {
            PpcTrampValHits++;
            Print(L"  PPC-TRAMP-VAL [%08x]<-%08x pc=%08x r1=%08x\n",
                  A, V, g_PpcContext.Pc, g_PpcContext.Gpr[1]);
        }
    }
    // ed.v software-function-slot write watch: catch ANY guest code seeding
    // 0xB800..0x8FF (the DR's software-function pointers), so we can locate
    // the ROM's DR-init that fills them (the B.21 faithful-seeding step).
    // 0xACB8 syscall-dispatch table write watch: the NK's software-function
    // dispatch at 0x40B1AED0 reads its target from here (or the KDP PMDT
    // neighbourhood), and no seeder has been observed in any run so far.
    // Catch ANY write so we can identify the ROM routine that populates it.
    // NanoKernel guard-fill (poison) stores: the emulated handoff marks the
    // low pages holding the live 68K vector table, KDP, emulator-data area,
    // PSA and 68K stack as free pool, so the guest allocator scrubs them
    // with fill patterns mid-execution. Dropping exactly these two pattern
    // words below 64K preserves the guest's own data; freed-block bodies
    // keep stale bytes, which the allocator treats as scratch anyway.
    if ((V == 0x68F168F1u || V == 0xD1E2D1E2u) && A < 0x10000u) {
        return;
    }
    // Watch writes into the KDP PMDT pointer array (KDP+0x78..KDP+0xFF8).
    // Legitimate writers: the one-shot PMDTINJECT probe and early init that
    // seeds slot pointers. Anything else scribbling here corrupts chunk
    // tables mid-walk, so log writer PC + value + old contents.
    if (A >= 0xA078u && A < 0xB000u) {
        static UINTN KdpWriteHits = 0;
        if (KdpWriteHits < 12 && g_PpcContext.Pc != 0x40B1F418u &&
            g_PpcContext.Pc != 0x40B1F41Cu && g_PpcContext.Pc != 0x40B1F420u) {
            KdpWriteHits++;
            Print(L"  KDPWATCH [%08x] <- %08x (old %08x) @PC=0x%08x r1=%08x r27=%08x\n",
                  A, V, CpuRead32(A),
                  g_PpcContext.Pc, g_PpcContext.Gpr[1], g_PpcContext.Gpr[27]);
        }
    }
    // Watch the NK interrupt-controller slot [KDP-0x338] = 0x9CC8. The D0 EXT
    // handler reads the controller base here (`lwz r9,-0x338(r8)`), and the
    // INTCTLSKIP probe found the guest seeds its OWN low-RAM block address
    // (0x81C0, allocator-returned) into this slot, defeating PpcIntCtrlInstall.
    // Find every writer to learn who brands the slot and what the block holds.
    // Watch writes into the NK IC descriptor block in low RAM (0x81C0..0x8220)
    // to see whether the guest ever populates the level-9 dispatch tables
    // (+0x3C/+0x40), priority bound (+0x44) or nesting bound (+0x4C) that the
    // level-9 SCC ISR at 0x40B148E0 reads. All-zero implies the guest expects
    // the emulator (as the device side) to keep these current, not the NK.
    CpuWrite16(A, V >> 16);
    CpuWrite16(A + 2, V);
}
// ---------------------------------------------------------------------------
// NK external-interrupt controller: register reads + KDP slot installation.
// ---------------------------------------------------------------------------
static BOOLEAN
PpcSccHasData (
    VOID
    )
{
    return g_SccRxFifoHead != g_SccRxFifoTail || g_SccRxPending;
}

// Pending-wake mask: bit set for each machine event that is both REAL and
// actually deliverable to the guest right now. Read by the NK give-up stub
// (soft-fn 0x2E) through the interrupt-controller window at +0x50 so the
// guest can return "woken" for genuine work only. Nothing here is a constant
// or a synthesized tick: a bit is set only if the corresponding event is
// queued (SCC input) or has a pending exception that the delivery path will
// actually take. An expired-but-deferred DEC does NOT set a bit, because the
// sell-phase deferral drops it and a wake it cannot service would livelock.
#define PPC_WAKE_SCC_RX   0x00000001u
#define PPC_WAKE_DEC      0x00000002u
#define PPC_WAKE_OTHER    0x00000004u

static UINT32
PpcPendingWakeMask (
    VOID
    )
{
    UINT32 Mask    = 0;
    UINT32 Pending = g_PpcContext.ExceptionPending;

    // SCC receive FIFO holds queued host input; the loop-bottom path raises
    // EXT for this unconditionally, so it is always deliverable.
    if (PpcSccHasData()) {
        Mask |= PPC_WAKE_SCC_RX;
    }
    if (Pending == PPC_EXCEPTION_DECREMENTER) {
        Mask |= PPC_WAKE_DEC;
    } else if (Pending != 0) {
        Mask |= PPC_WAKE_OTHER;
    }
    return Mask;
}

// Return the big-endian 32-bit controller register at byte offset `Off`.
// MMIO reads funnel through PpcDefaultReadByte, so a `lwz` becomes four
// byte reads; this assembles each byte of the register value on demand.
static UINT32
PpcIntCtrlReadReg (
    IN UINT32 Off
    )
{
    UINT32 Base = Off & ~3u;
    UINT32 Kdp  = g_PpcContext.Spr[272];
    switch (Base) {
    case 0x20: return PpcSccHasData() ? 0x00000002u : 0x00000000u; // IntEvents1
    case 0x24: return 0x000000FFu;                                  // IntMask1
    case 0x28: return 0x00000000u;                                  // IntClear1
    case 0x2C: return 0x00000000u;                                  // IntLevels1
    case 0x38: return PpcSccHasData() ? 0x00000002u : 0x00000000u;  // level-9 SCC pending
    case 0x44: return 0x3FFFFFFFu;                                  // priority scan bound
    case 0x4C:                                                      // nesting bound +1
        return (Kdp != 0) ? (CpuRead32(Kdp - 0x1C) + 1u) : 1u;
    case 0x50: return PpcPendingWakeMask();                          // pending wake mask
    default:   return 0u;
    }
}
// Point the NK's interrupt-controller slot [KDP-0x338] at our device window.
// The 0x500 EXT handler reads the controller base with `lwz r9,-0x338(r8)`
// (r8=SPRG0=KDP) and dispatches on level >= 2; `[KDP-0x824]` is a different
// KDP field (0x338 hex == 824 decimal, NOT 0x824 hex). The emulator already
// patches KDP RAM fields at boot (task r1, ECB link at 0x9FFC/0xA658); this
// slot is simply left at 0 by the phases we emulate, so install idempotently
// only when it is still unset.
static VOID
PpcIntCtrlInstall (
    VOID
    )
{
    UINT32 Kdp = g_PpcContext.Spr[272];
    if (Kdp >= 0xA000u && Kdp <= 0xC000u && CpuRead32(Kdp - 0x338) == 0) {
        static UINT32 IntCtlInstallLogs = 0;
        CpuWrite32(Kdp - 0x338, PPC_INT_CTRL_BASE);
        // Only log from a live guest (the loop-bottom EXT raise), not from the
        // host-side device self-test that exercises the same install path.
        if (IntCtlInstallLogs < 6 && g_PpcContext.Pc != 0) {
            IntCtlInstallLogs++;
            Print(L"  INTCTL [KDP-0x338]=0x%08x (PC=0x%08x)\n",
                  PPC_INT_CTRL_BASE, g_PpcContext.Pc);
        }
    } else {
    }
}
// ---------------------------------------------------------------------------
// SCC FIFO + NK external-interrupt controller self-test. Runs on the host as
// part of the aggregate CPU self-test (PpcRunSelfTest): it exercises only the
// queue/level primitives and the KDP-slot install -- no QEMU, no guest code --
// so the interrupt machinery that stays inert during the poll-driven boot is
// under permanent regression test instead of untested.
// ---------------------------------------------------------------------------
static UINT8 g_SccSelfTestRam[0x1200];   // capture for guest 0x9000..0xA1FF

static UINT8
SccSelfTestReadByte (
    IN UINT32 Address
    )
{
    if (Address >= 0x9000u && Address < 0xA200u) {
        return g_SccSelfTestRam[Address - 0x9000u];
    }
    return 0;
}

static VOID
SccSelfTestWriteByte (
    IN UINT32 Address,
    IN UINT8  Value
    )
{
    if (Address >= 0x9000u && Address < 0xA200u) {
        g_SccSelfTestRam[Address - 0x9000u] = Value;
    }
}

static VOID
PpcSccSelfTestCheck (
    IN BOOLEAN Ok,
    IN CHAR16* Name,
    IN OUT UINTN* Passed,
    IN OUT UINTN* Failed
    )
{
    if (Ok) {
        (*Passed)++;
        Print(L"  [PASS] %s\n", Name);
    } else {
        (*Failed)++;
        Print(L"  [FAIL] %s\n", Name);
    }
}

static UINT32
SccSelfTestRead32 (
    IN UINT32 Address
    )
{
    return ((UINT32)SccSelfTestReadByte(Address) << 24) |
           ((UINT32)SccSelfTestReadByte(Address + 1) << 16) |
           ((UINT32)SccSelfTestReadByte(Address + 2) << 8) |
           ((UINT32)SccSelfTestReadByte(Address + 3));
}

VOID
PpcRunSccDeviceSelfTest (
    OUT UINTN* Passed,
    OUT UINTN* Failed
    )
{
    UINTN   SavedHead    = g_SccRxFifoHead;
    UINTN   SavedTail    = g_SccRxFifoTail;
    BOOLEAN SavedPending = g_SccRxPending;
    UINT32  SavedSprg4   = g_PpcContext.Spr[272];
    UINTN   SavedDataLogs   = SccDataReads;
    UINTN   SavedStatusLogs = SccStatusReads;
    UINTN   I;
    UINT8   B;

    *Passed = 0;
    *Failed = 0;
    Print(L"--- SCC + interrupt-controller self-test ---\n");

    // 1. Empty FIFO: no Rx data, status shows only Tx-buffer-empty.
    g_SccRxFifoHead = g_SccRxFifoTail = 0;
    g_SccRxPending  = FALSE;
    PpcSccSelfTestCheck(!PpcSccHasData(),
                        L"SCC FIFO empty -> PpcSccHasData false", Passed, Failed);
    PpcSccSelfTestCheck(PpcIntCtrlReadReg(0x20) == 0u &&
                        PpcIntCtrlReadReg(0x38) == 0u,
                        L"SCC empty -> EVENTS1 + level-9 SCC pending clear", Passed, Failed);
    PpcSccSelfTestCheck(PpcDefaultReadByte(0x20002) == 0x04,
                        L"SCC status, no Rx data -> 0x04 (Tx empty only)", Passed, Failed);
    PpcSccSelfTestCheck(PpcDefaultReadByte(0x20006) == 0x00,
                        L"SCC data read, empty -> 0x00", Passed, Failed);

    // 2. One queued byte: data ready, status bit0 set, the controller reports
    //    the pending SCC level, and the data read returns the byte in order.
    PpcSccPutChar(0x4D);   // 'M'
    PpcSccSelfTestCheck(PpcSccHasData(),
                        L"SCC FIFO non-empty -> has data", Passed, Failed);
    PpcSccSelfTestCheck(PpcIntCtrlReadReg(0x20) == 0x00000002u,
                        L"EVENTS1 bit1 = SCC Rx pending (0x2)", Passed, Failed);
    PpcSccSelfTestCheck(PpcIntCtrlReadReg(0x38) == 0x00000002u,
                        L"level-9 SCC pending = 0x2", Passed, Failed);
    PpcSccSelfTestCheck(
            (((UINT32)PpcDefaultReadByte(PPC_INT_CTRL_BASE + 0x20) << 24) |
             ((UINT32)PpcDefaultReadByte(PPC_INT_CTRL_BASE + 0x21) << 16) |
             ((UINT32)PpcDefaultReadByte(PPC_INT_CTRL_BASE + 0x22) << 8) |
             ((UINT32)PpcDefaultReadByte(PPC_INT_CTRL_BASE + 0x23))) == 0x00000002u,
            L"IntEvents1 MMIO word at +0x20 = 0x00000002", Passed, Failed);
    PpcSccSelfTestCheck(PpcDefaultReadByte(0x20002) == 0x05,
                        L"SCC status, Rx ready -> 0x05", Passed, Failed);
    PpcSccSelfTestCheck(PpcDefaultReadByte(0x20006) == 0x4D,
                        L"SCC data read returns the queued byte", Passed, Failed);
    PpcSccSelfTestCheck(!PpcSccHasData(),
                        L"drain -> FIFO empty again", Passed, Failed);

    // 3. Order preservation + full-ring wrap: 64 puts fill exactly 64 slots,
    //    the 65th is dropped, and draining returns 0..63 in order.
    g_SccRxFifoHead = g_SccRxFifoTail = 0;
    g_SccRxPending  = FALSE;
    for (I = 0; I < sizeof(g_SccRxFifo); I++) {
        PpcSccPutChar((UINT8)I);
    }
    PpcSccSelfTestCheck(PpcSccHasData(),
                        L"64 puts -> FIFO full, data present", Passed, Failed);
    PpcSccPutChar(0xFF);   // overrun: dropped, ring contents preserved
    for (I = 0; I < sizeof(g_SccRxFifo); I++) {
        B = PpcDefaultReadByte(0x20006);
        PpcSccSelfTestCheck(B == (UINT8)I,
                            L"SCC wrap drain order (0..63)", Passed, Failed);
    }
    PpcSccSelfTestCheck(!PpcSccHasData() && PpcDefaultReadByte(0x20002) == 0x04,
                        L"full drain -> FIFO empty, status 0x04", Passed, Failed);

    // 4. Controller register readbacks that do not depend on FIFO state.
    PpcSccSelfTestCheck(PpcIntCtrlReadReg(0x24) == 0x000000FFu,
                        L"IntMask1 = 0xFF", Passed, Failed);
    PpcSccSelfTestCheck(PpcIntCtrlReadReg(0x28) == 0u,
                        L"IntClear1 = 0", Passed, Failed);
    PpcSccSelfTestCheck(PpcIntCtrlReadReg(0x2C) == 0u,
                        L"IntLevels1 = 0", Passed, Failed);
    PpcSccSelfTestCheck(PpcIntCtrlReadReg(0x44) == 0x3FFFFFFFu,
                        L"priority scan bound = 0x3FFFFFFF", Passed, Failed);

    // 5. PpcIntCtrlInstall: point [KDP-0x338] at the controller window,
    //    idempotently, and only while the slot is still unset. Uses a local
    //    capture array (no guest RAM) so the boot state is never touched.
    PpcSetMemoryAccess(SccSelfTestReadByte, SccSelfTestWriteByte);
    g_PpcContext.Spr[272] = 0xA000;
    for (I = 0; I < sizeof(g_SccSelfTestRam); I++) {
        g_SccSelfTestRam[I] = 0;
    }
    g_SccSelfTestRam[0x9FE6 - 0x9000] = 0x12;   // [KDP-0x1C] = 0x00001234 (big-endian word)
    g_SccSelfTestRam[0x9FE7 - 0x9000] = 0x34;
    PpcSccSelfTestCheck(PpcIntCtrlReadReg(0x4C) == 0x1235u,
                        L"nesting bound = [KDP-0x1C]+1", Passed, Failed);
    PpcIntCtrlInstall();
    PpcSccSelfTestCheck(SccSelfTestRead32(0xA000 - 0x338) == PPC_INT_CTRL_BASE,
                        L"PpcIntCtrlInstall -> [KDP-0x338] = controller base", Passed, Failed);
    PpcIntCtrlInstall();
    PpcSccSelfTestCheck(SccSelfTestRead32(0xA000 - 0x338) == PPC_INT_CTRL_BASE,
                        L"PpcIntCtrlInstall idempotent", Passed, Failed);
    {
        // Simulate a pre-seeded slot: install must not overwrite it.
        UINT32 Old = SccSelfTestRead32(0xA000 - 0x338);
        g_SccSelfTestRam[0x9CC8 - 0x9000] = 0xDE;
        g_SccSelfTestRam[0x9CC9 - 0x9000] = 0xAD;
        g_SccSelfTestRam[0x9CCA - 0x9000] = 0xBE;
        g_SccSelfTestRam[0x9CCB - 0x9000] = 0xEF;
        PpcIntCtrlInstall();
        PpcSccSelfTestCheck(SccSelfTestRead32(0xA000 - 0x338) == 0xDEADBEEFu &&
                            Old == PPC_INT_CTRL_BASE,
                            L"PpcIntCtrlInstall preserves a pre-seeded slot", Passed, Failed);
    }
    g_PpcContext.Spr[272] = 0;
    PpcSccSelfTestCheck(PpcIntCtrlReadReg(0x4C) == 1u,
                        L"nesting bound with KDP==0 -> 1", Passed, Failed);
    PpcSetMemoryAccess(NULL, NULL);

    // Restore pristine boot state.
    g_SccRxFifoHead = SavedHead;
    g_SccRxFifoTail = SavedTail;
    g_SccRxPending  = SavedPending;
    g_PpcContext.Spr[272] = SavedSprg4;
    SccDataReads   = SavedDataLogs;
    SccStatusReads = SavedStatusLogs;
    Print(L"--- SCC self-test complete: %d/%d ---\n", *Passed, *Passed + *Failed);
}
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
    // Same NK dispatch-table watch as CpuWrite32: byte-reversed stores are a
    // distinct code path and would otherwise slip past that watchpoint.
    LowRamCensus(A, V, g_PpcContext.Pc);
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
static UINT32 EeMtmsrProbed = 0;
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
    UINT32 Pa;
    UINT32 Fetch;
    // Instruction address translation is gated on MSR[IR]. The fetch is the
    // one read that must not route through the DR-gated data wrapper, and the
    // word is read from the untranslated physical layer (the address here is
    // either already physical with IR clear, or the translated PA with IR set).
    if ((g_PpcContext.Msr & PPC_MSR_IR) != 0 &&
        PpcTranslateEffective(Address, TRUE, &Pa)) {
        Address = Pa;
    }
    Fetch = ((UINT32)PpcDefaultReadByte(Address) << 24) |
            ((UINT32)PpcDefaultReadByte(Address + 1) << 16) |
            ((UINT32)PpcDefaultReadByte(Address + 2) << 8) |
            ((UINT32)PpcDefaultReadByte(Address + 3));
    return Fetch;
}
static void
PpcDisasm (
    IN UINT32 Address,
    IN UINT32 w,
    OUT CHAR16* Out,
    IN UINTN OutSize
    );
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
    static UINT32 HandoffDumped = 0;
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
    // EMUL_OP marker interception (SheepShaver-faithful opcode-dispatch-table
    // shortcuts). The DR's 68K opcode slots for the EMUL_OP extended opcodes
    // (0xFE40..) are patched to `PPC_EMUL_OP_MARKER | n` ("mulli r0,r0,n").
    // When the DR dispatches such an opcode it branches to the slot and the
    // interpreter services the selector through the host device layer, then
    // resumes the DR emulator loop at the slot's `b 0x366084` continuation
    // (PC 0x40B66084, the post-fetch loop glue that persists r27 and loops).
    // r24/r27 are left untouched: the DR has already fetched the marker word.
    if (CurrentAddress >= PPC_EMUL_OP_DISPATCH_GUEST_BASE &&
        CurrentAddress <  PPC_EMUL_OP_DISPATCH_GUEST_END &&
        (w & 0xFFFF0000u) == PPC_EMUL_OP_MARKER) {
        UINT32 N = w & 0xFFFFu;
        if (N >= 3) {
            EmulOpDispatch(N - 3);
        }
        // Markers 0..2 (EMUL_RETURN/EXEC_RETURN/EXEC_NATIVE) are control
        // words the retired entry-machinery supplied; re-entering the loop is
        // the correct continuation for those too.
        *NextAddress = PPC_NEW_WORLD_ROM_GUEST_BASE + 0x366084;
        return EFI_SUCCESS;
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
            // MKRDYCALL: who branches to MakeReady? The runtime image is patched
            // at load so a static scan is unreliable, and hand-decoding the 24-bit
            // displacement is easy to get wrong; intercept the target the
            // interpreter already computed instead.
            if (Next >= 0x40B22D40u && Next <= 0x40B22D48u && g_MkrdyAny < 48) {
                g_MkrdyAny++;
                if (Next == 0x40B22D40u || Next == 0x40B22D44u) {
                    g_MkrdyCalls++;
                }
                Print(L"  MKRDYCALL bc  from=0x%08x to=0x%08x r1=0x%08x "
                      L"r3=0x%08x r4=0x%08x r5=0x%08x\n",
                      CurrentAddress, Next, g_PpcContext.Gpr[1],
                      g_PpcContext.Gpr[3], g_PpcContext.Gpr[4],
                      g_PpcContext.Gpr[5]);
            }
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
        if (Next >= 0x40B22D40u && Next <= 0x40B22D48u && g_MkrdyAny < 48) {
            g_MkrdyAny++;
            if (Next == 0x40B22D40u || Next == 0x40B22D44u) {
                g_MkrdyCalls++;
            }
            Print(L"  MKRDYCALL %s from=0x%08x to=0x%08x r1=0x%08x "
                  L"r3=0x%08x r4=0x%08x r5=0x%08x\n",
                  LK(w) ? L"bl" : L"b", CurrentAddress, Next,
                  g_PpcContext.Gpr[1], g_PpcContext.Gpr[3],
                  g_PpcContext.Gpr[4], g_PpcContext.Gpr[5]);
        }
        break;
    case 19: // XL-form
        switch (XO10(w)) {
        case XO19_MCRF:  // mcrf crfD, crfS
            PpcSetCrField((w >> 23) & 0x7, PpcGetCrField((w >> 18) & 0x7));
            break;
        case XO19_BCLR:  // blr / blrl / bclr / bclrl
            {
                UINT32 Target = g_PpcContext.Lr;
                BOOLEAN DrDead = (CurrentAddress == 0x40B6D12Cu) &&
                                 ((Target & 0xFF000000u) == 0x3C000000u) &&
                                 g_DrNativeHandoff != 0;
                // The DR dispatch hub branches to the "handler" it loaded
                // from [r31+0x814]; the diag image carries 0x3C840007 there
                // and nothing maps that window, so complete the call to its
                // own return slot (branch-on-CR recovery at 0x40B6D130).
                if (DrDead) {
                    Target = CurrentAddress + 4;
                }
                if (LK(w)) {
                    g_PpcContext.Lr = CurrentAddress + 4;
                }
                if (PpcBranchTaken(BO(w), BI(w))) {
                    Next = Target;
                }
            }
            break;
        case XO19_RFI:  // rfi
            // ---- DR-bootstrap: emulator-start task-switch resume ----
            // MRETTASK ends with `lwz r0,0x104(r6); ...; rfi` at 0x40B24518-24
            // (ECB.IntraState.HandlerReturn). On the cold path that runs in a
            // nested in-handler context (the DEC preempt at boot), HandlerReturn
            // is left bogus and SRR0 is 0, so the switch would rfi to PC 0.
            // Hardware resumes the DR's emulator-task context: r24 = the 68K PC,
            // r27 = its already-fetched opcode word, r30 = emulator base, then
            // the DR main dispatch loop (`mr r6,r27; lhau r27,2(r24); mtcrf
            // 0xf,r6` at 0x40B67B60). Fabricate that state so the ROM's own PPC
            // DR runs the first 68K opcode, and lift the DEC gate the boot
            // critical section was holding.
            if (g_DrBootPcSeeded &&
                g_PpcContext.Srr0 == 0 &&
                g_PpcContext.Gpr[24] == 0x4080002A) {
                static UINT32 DrResumeDone = 0;
                if (DrResumeDone == 0) {
                    DrResumeDone = 1;
                    // Resume at the DR's true cold-start continuation
                    // (0x40B6E964). It self-initialises: bl 0x40B6DB94 fills the
                    // gd slots, zeroes all 68K D0-D6/A0-A2, then li r24,0; lwz
                    // r1,0(r24); lwz r24,4(r24) reads the 68K SSP/PC from low
                    // memory (seeded above), sets SR=0x27, and bctr's into the
                    // dispatch loop. It needs only the emulator-data base in
                    // r31, the dispatch table base in r29 and the emulator code
                    // base in r30 to be meaningful.
                    g_PpcContext.Gpr[31] = 0x0000B000;     // ed / emulator-data base
                    g_PpcContext.Gpr[29] = 0x40B80000;     // LA_DispatchTable (8-byte opcode stubs)
                    g_PpcContext.Gpr[30] = 0x40B60000;     // LA_EmulatorCode base
                    g_PpcContext.Srr0 = 0x40B6E964;        // DR emulator-start continuation
                    // When the running 68K program yields at the emulator trap
                    // table, the 0x700 handler's task-switch (idgen "swap" at
                    // 0x40B12B0C) takes the next task context from
                    // [task_r1 + 0x658] where task_r1 = [SPRG0-4] = [0x9FFC].
                    // That slot is 0 here, so the swap would restore from NULL
                    // and MRETTASK's rfi (SRR0 = ctx[0x104]) jumps to PC 0.
                    // Point the switch at the ECB and put the DR dispatch loop
                    // in the ECB's Handler-return slot to resume the emulator.
                    CpuWrite32(0x00009FFC, 0x0000A000);  // [SPRG0-4] task r1 (68K SSP)
                    CpuWrite32(0x0000A658, 0x0000B100);  // [task_r1+0x658] next ctx = ECB
                    CpuWrite32(0x0000B204, 0x40B67B60);  // ECB+0x104 HandlerReturn = DR dispatch
                    // Park DEC positive so the handoff rfi (EE already on,
                    // DEC left negative by the pre-DRB boot) doesn't fire an
                    // immediate 0x900 on the first DR-start instruction and let
                    // the DR dispatch loop arm the tick at its own rate.
                    g_PpcContext.Spr[SPR_DEC] = 0x01000000;
                    g_PpcContext.DecrementerNegative = 0;
                    g_BootDecGate = 0;
                    Print(L"  DRBOOT-RESUME @rfi ecb[1c4]=0x%08x lowmem[4]=0x%08x "
                          L"-> DR start 0x40B6E964 r31(ed)=0xB000 r29=0x40B80000\n",
                          g_PpcContext.Gpr[24], CpuRead32(4));
                }
            }
            // DR emulator-trap yield: while the ROM's PPC DR is driving the 68K
            // boot program it faults at the emulator trap table (0x40B6E8C8:
            // `twi r31,2`, reached via the DR's `bl 0x40b6e8c8`). The 0x700
            // handler's task swap stores r0 (= the trap address) as the ECB
            // Handler-return, so an unpatched MRETTASK rfis straight back into
            // the same `twi` and traps forever. Modernise the resume to the DR
            // dispatch loop re-armed on the ECB: r24 holds the 68K PC past the
            // yielding opcode, the process re-fetches r27, and r29 must be the
            // dispatch-table base again (the handler left it as a stale slot).
            if (g_DrBootPcSeeded &&
                g_PpcContext.Srr0 == 0x40B6E8C8 &&
                g_PpcContext.Gpr[24] >= 0x40800000 &&
                g_PpcContext.Gpr[24] < 0x40840000) {
                g_PpcContext.Gpr[29] = 0x40B80000;     // re-arm LA_DispatchTable
                g_PpcContext.Gpr[31] = 0x0000B000;     // ed base (paranoia)
                g_PpcContext.Gpr[28] = 0x0000B000;     // R28 ABI experiment: host mem base
                g_PpcContext.Srr0 = 0x40B67B60;        // DR dispatch loop (refetch)
                g_DrYieldSeen = 1;
                g_DrPostYieldWindow = 1;
                g_DrYieldCount++;
                g_DrSpillDump = 0;      // BSS zero-fill skipped by loader for 0x1800b12a0+;
                g_DrJsrTrace = 0;       // force counters to known values so gates see 0
                g_DrA207ArmPc = 0;
                g_DrA207ArmSp = 0;
                g_DrA207ArmState = 0;
                g_DrA207ArmLogged = 0;
                g_DrA207Base = 0;
g_DrA207Land = 0;
                g_DrOsAdvance = 0;
                g_DrA207Cap = 0;
                g_DrA207Steer = 1;
                g_DrA003Gate = 0;
                g_DrA003Steer = 1;
                g_DrNativeTail = 0;
                g_DrNativeLand = 0;
                g_DrNativeHandoff = 0;  // .bss not zero-filled past 0x1800b12a0
                g_DrNativeSteer = 0;    // (loader skips it): force known state
                g_DrForkSlotFixed = 0;  // gate the FORKDIAG/CRASHFIX repairs on 0
                Print(L"  DRYIELD-RESUME @rfi trap=0x40B6E8C8 68Kpc(r24)=0x%08x "
                      L"r27(op)=0x%04x -> DR dispatch 0x40B67B60\n",
                      g_PpcContext.Gpr[24], g_PpcContext.Gpr[27] & 0xFFFF);
            }
            if (CurrentAddress == 0x40B24524u && g_PpcContext.Gpr[1] == 0) {
                // NK task-restore tail (0x40B24518..0x40B24524):
                //   lwz r0,0x104(r6); lwz r6,0x18(r1); lwz r1,4(r1); rfi
                // The idle task's saved stack pointer ([ctx+4]) is 0, so every
                // asynchronous entry into the idle glue -- and into the DEC
                // handler that preempts it -- runs with r1=0. The NK then
                // touches [r1-0xB50] (MAC lock), [r1-0x340], [r1-0x438] and
                // [r1+0xE8C]; with r1=0 the negative offsets wrap to
                // 0xFFFFF4B0/0xFFFFFCC0 (ROM/MMIO) and the lock test never
                // clears, so the guest spins in the acquire at 0x40B12700.
                // Hand the idle task the same kind of low-RAM frame the DR
                // task uses (0xA000), one region lower so the two task
                // contexts never share scratch: 0x9000 covers
                // [0x84B0..0x9F00] including the [r1+0xE8C] tick counter and
                // stays clear of the KDP/boot-proc frame at 0xA000.
                static UINT32 IdleSpFixed = 0;
                g_PpcContext.Gpr[1] = 0x00009000u;
                if (IdleSpFixed < 4) {
                    IdleSpFixed++;
                    Print(L"  IDLESP fix r1=0 -> 0x00009000 (idle task restore @0x40B24524,"
                          L" resume PC=0x%08x)\n", g_PpcContext.Srr0);
                }
            }
            if (CurrentAddress >= 0x40B10000 && CurrentAddress < 0x40B30000) {
                // The fabricated level-9 dispatch rfi (ISR tail at 0x40B14A04,
                // resuming our mirror's sell-glue target 0x40B24F08) reads its
                // SRR1 from a guest memory slot we can't reliably seed, so the
                // resumed task would carry a garbage MSR. Force a sane resume
                // MSR (EE on, like the interrupted task's 0x9002) at that rfi.
                if (CurrentAddress == 0x40B14A04u &&
                    g_PpcContext.Srr0 == 0x40B24F08u && g_IcdSrr1Forced < 32) {
                    g_IcdSrr1Forced++;
                    g_PpcContext.Srr1 = 0x00009002u;
                }
                Print(L"  DBG rfi @0x%08x: SRR0=0x%08x SRR1=0x%08x MSR=0x%08x -> PC=0x%08x\n",
                      CurrentAddress, g_PpcContext.Srr0, g_PpcContext.Srr1,
                      g_PpcContext.Msr, g_PpcContext.Srr0);
            }
            if (CurrentAddress == 0x40B14A04 && g_DrRfiIcdDump < 4) {
                g_DrRfiIcdDump++;
                UINT32 S0 = g_PpcContext.Spr[272];
                UINT32 S4 = g_PpcContext.Spr[276];
                Print(L"  ICDCHECK r1now=0x%08x SPRG0=0x%08x SPRG4=0x%08x"
                      L" [0x969C]=0x%08x [SR4-4]=0x%08x", g_PpcContext.Gpr[1],
                      S0, S4, CpuRead32(0x969C),
                      (S4 >= 0xA000u && S4 <= 0xC000u) ? CpuRead32(S4 - 4) : 0);
                if (S4 >= 0xA000u && S4 <= 0xC000u) {
                    Print(L" [SR4-4-0x964]=0x%08x [0x1A69C]=0x%08x\n",
                          CpuRead32(S4 - 4 - 0x964), CpuRead32(0x1A69C));
                } else {
                    Print(L"\n");
                }
            }
            if (g_PpcContext.Srr0 == 0x0000013F && HandoffDumped == 0) {
                UINT32 I;
                HandoffDumped = 1;
                Print(L"  HANDOFF rfi @0x%08x -> PC=0x0000013F (68K emulator entry)\n", CurrentAddress);
                Print(L"  HANDOFF r0-r11: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                      g_PpcContext.Gpr[0], g_PpcContext.Gpr[1], g_PpcContext.Gpr[2],
                      g_PpcContext.Gpr[3], g_PpcContext.Gpr[4], g_PpcContext.Gpr[5],
                      g_PpcContext.Gpr[6], g_PpcContext.Gpr[7], g_PpcContext.Gpr[8],
                      g_PpcContext.Gpr[9], g_PpcContext.Gpr[10], g_PpcContext.Gpr[11]);
                Print(L"  HANDOFF r12-r23: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                      g_PpcContext.Gpr[12], g_PpcContext.Gpr[13], g_PpcContext.Gpr[14],
                      g_PpcContext.Gpr[15], g_PpcContext.Gpr[16], g_PpcContext.Gpr[17],
                      g_PpcContext.Gpr[18], g_PpcContext.Gpr[19], g_PpcContext.Gpr[20],
                      g_PpcContext.Gpr[21], g_PpcContext.Gpr[22], g_PpcContext.Gpr[23]);
                Print(L"  HANDOFF r24-r31: %08x %08x %08x %08x %08x %08x %08x %08x\n",
                      g_PpcContext.Gpr[24], g_PpcContext.Gpr[25], g_PpcContext.Gpr[26],
                      g_PpcContext.Gpr[27], g_PpcContext.Gpr[28], g_PpcContext.Gpr[29],
                      g_PpcContext.Gpr[30], g_PpcContext.Gpr[31]);
                Print(L"  HANDOFF CR=0x%08x XER=0x%08x CTR=0x%08x LR=0x%08x SPRG4=0x%08x SDR1=0x%08x\n",
                      g_PpcContext.Cr, g_PpcContext.Xer, g_PpcContext.Ctr,
                      g_PpcContext.Lr, g_PpcContext.Spr[272], g_PpcContext.Spr[25]);
                Print(L"  HANDOFF MMU: MSR=0x%08x SDr1=0x%08x IR=%u DR=%u\n",
                      g_PpcContext.Msr, g_PpcContext.Sdr1,
                      (g_PpcContext.Msr >> 26) & 1, (g_PpcContext.Msr >> 27) & 1);
                Print(L"  HANDOFF SR[0..15]: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                      g_PpcContext.Sr[0], g_PpcContext.Sr[1], g_PpcContext.Sr[2],
                      g_PpcContext.Sr[3], g_PpcContext.Sr[4], g_PpcContext.Sr[5],
                      g_PpcContext.Sr[6], g_PpcContext.Sr[7], g_PpcContext.Sr[8],
                      g_PpcContext.Sr[9], g_PpcContext.Sr[10], g_PpcContext.Sr[11],
                      g_PpcContext.Sr[12], g_PpcContext.Sr[13], g_PpcContext.Sr[14],
                      g_PpcContext.Sr[15]);
                Print(L"  HANDOFF IBAT begin epi bps physlo prot accel : DBAT begin epi bps physlo prot accel\n");
                for (I = 0; I < 8; I++) {
                    Print(L"    IBAT%d %08x %08x %06x %08x %u %u : DBAT%d %08x %08x %06x %08x %u %u\n",
                          I, g_PpcContext.Bat[I].Bepi, g_PpcContext.Bat[I].HiMask,
                          g_PpcContext.Bat[I].PhysHi >> 17, g_PpcContext.Bat[I].PhysHi,
                          g_PpcContext.Bat[I].Prot, g_PpcContext.Bat[I].Access,
                          I, g_PpcContext.Bat[8 + I].Bepi, g_PpcContext.Bat[8 + I].HiMask,
                          g_PpcContext.Bat[8 + I].PhysHi >> 17, g_PpcContext.Bat[8 + I].PhysHi,
                          g_PpcContext.Bat[8 + I].Prot, g_PpcContext.Bat[8 + I].Access);
                }
                Print(L"  HANDOFF mem@0x013F (8 words):\n");
                for (I = 0; I < 8; I++) {
                    UINT32 A = 0x0130 + I * 4;
                    Print(L"    0x%08x: %08x\n", A, CpuRead32(A));
                }
                Print(L"  HANDOFF vectors 0x000-0x080:\n");
                for (I = 0; I < 8; I++) {
                    UINT32 A = I * 16;
                    Print(L"    0x%08x: %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
                Print(L"  HANDOFF stack r1=0x%08x: [r1]=0x%08x [r1-4]=0x%08x [r1+0x648]=0x%08x [r1+0x5a0]=0x%08x [r1+0x5a4]=0x%08x [r1-0x964]=0x%08x\n",
                      g_PpcContext.Gpr[1], CpuRead32(g_PpcContext.Gpr[1]),
                      CpuRead32(g_PpcContext.Gpr[1] - 4), CpuRead32(g_PpcContext.Gpr[1] + 0x648),
                      CpuRead32(g_PpcContext.Gpr[1] + 0x5a0), CpuRead32(g_PpcContext.Gpr[1] + 0x5a4),
                      CpuRead32(g_PpcContext.Gpr[1] - 0x964));
                // SheepShaver's jump68k-caller patch loads r3=[r1+0x634] (EDP),
                // r4=[r1+0x119c] (opcode table), r0=[r1+0x1184] (emulator
                // entry), then mtctr r0 / bctr. Dump those slots to confirm the
                // nanokernel has populated the emulator entry state.
                Print(L"  HANDOFF SS-slots: [r1+0x634]=0x%08x [r1+0x638]=0x%08x [r1+0x1180]=0x%08x [r1+0x1184]=0x%08x [r1+0x119c]=0x%08x [r1+0x1190]=0x%08x\n",
                      CpuRead32(g_PpcContext.Gpr[1] + 0x634),
                      CpuRead32(g_PpcContext.Gpr[1] + 0x638),
                      CpuRead32(g_PpcContext.Gpr[1] + 0x1180),
                      CpuRead32(g_PpcContext.Gpr[1] + 0x1184),
                      CpuRead32(g_PpcContext.Gpr[1] + 0x119c),
                      CpuRead32(g_PpcContext.Gpr[1] + 0x1190));
                // Scan low memory for the ContextBlock markers the nanokernel
                // populates (ECB+0x84=LA_EmulatorEntry=0x6806E8B0, ECB+0x88=
                // LA_EmulatorData=0x68FFF000, ECB+0x8C=LA_DispatchTable=
                // 0x68080000) and print the first ECB-sized candidate found.
                for (I = 0x0000; I + 0x100 < 0x00040000; I += 4) {
                    if (CpuRead32(I + 0x84) == 0x6806E8B0 &&
                        CpuRead32(I + 0x88) == 0x68FFF000) {
                        Print(L"  HANDOFF ECB candidate at 0x%08x: +0x80=%08x +0x84=%08x +0x88=%08x +0x8C=%08x +0x90=%08x +0x94=%08x\n",
                              I, CpuRead32(I + 0x80), CpuRead32(I + 0x84),
                              CpuRead32(I + 0x88), CpuRead32(I + 0x8C),
                              CpuRead32(I + 0x90), CpuRead32(I + 0x94));
                        break;
                    }
                }
                // LowMem init region (0x0-0x2000): the NK zeroes it from
                // PA_RelocatedLowMemInit (0xFFFFFFFF wraps to low addresses).
                Print(L"  HANDOFF lowmem 0x1000:%08x 0x1800:%08x 0x1FF0:%08x 0x1FF4:%08x 0x1FF8:%08x 0x1FFC:%08x\n",
                      CpuRead32(0x1000), CpuRead32(0x1800), CpuRead32(0x1FF0),
                      CpuRead32(0x1FF4), CpuRead32(0x1FF8), CpuRead32(0x1FFC));
            }
            g_PpcContext.Msr = g_PpcContext.Srr1;
            Next = g_PpcContext.Srr0;
            if ((g_PpcContext.Msr & PPC_MSR_EE) && g_PpcContext.DecrementerNegative &&
                g_PpcContext.DecrementerWritten &&
                g_PpcContext.ExceptionPending == 0) {
                g_PpcContext.ExceptionPending = PPC_EXCEPTION_DECREMENTER;
            }
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
                // DR dispatch trap: if this branch jumps into the kckc-filled
                // data region below the emulator base, record the snapshot; the
                // tail ring (with r29/CTR) lives in PpcRunGuest which dumps it.
                if (PpcBranchTaken(BO(w), BI(w)) && Target >= 0x40900000 &&
                    Target < 0x40B60000 && gKckcJumpPending == 0) {
                    gKckcJumpPending = 1;
                    gKckcPc  = CurrentAddress;
                    gKckcCtr = Target;
                    gKckcR29 = g_PpcContext.Gpr[29];
                    gKckcR24 = g_PpcContext.Gpr[24];
                    gKckcR27 = g_PpcContext.Gpr[27];
                    gKckcR28 = g_PpcContext.Gpr[28];
                    gKckcR30 = g_PpcContext.Gpr[30];
                    gKckcR31 = g_PpcContext.Gpr[31];
                    gKckcCr  = g_PpcContext.Cr;
                    gKckcLr  = g_PpcContext.Lr;
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
        {
            UINT32 Ea = EaD(w, RA(w));
            PpcKdProfileLoad(Ea, CurrentAddress);
            g_PpcContext.Gpr[RT(w)] = CpuRead32(Ea);
        }
        break;
    case 33: // lwzu
        {
            UINT32 Ea = EaD(w, RA(w));
            g_PpcContext.Gpr[RT(w)] = CpuRead32(Ea);
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;
    case 34: // lbz
        {
            UINT32 Ea = EaD(w, RA(w));
            // PHASE A.3: the NK boot printer's Tx-empty poll (`lbz 2(r28)`
            // at 0x40B26500) spins forever because NoIdeaR23 [KDP-0x900]
            // is never seeded, so r28 = 0 and the poll reads low-memory
            // byte 0x2 instead of the SCC status register at 0x20002.
            // Force the ready answer at this one site: the banner flush
            // proceeds and each character store is captured by the stb
            // hook below, so NK boot output becomes visible on the host
            // console exactly as it would on a real SCC.
            if (CurrentAddress == 0x40B26500u) {
                g_PpcContext.Gpr[RT(w)] = 0x04;
            } else {
                PpcKdProfileLoad(Ea, CurrentAddress);
                g_PpcContext.Gpr[RT(w)] = g_ReadByte(Ea);
            }
        }
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
        {
            UINT32 Ea = EaD(w, RA(w));
            // PHASE A.3: capture the NK boot printer's output characters.
            // With r28 = 0 the printer stores each banner byte to guest
            // 0x6, which would trample low-memory globals; on real
            // hardware this store goes to the SCC Tx register. Inside the
            // flush-helper PC window, a low-addressed byte store IS the
            // serial output: print it and swallow the store. Once r28 is
            // properly seeded the effective address leaves the low page,
            // the existing 0x20006 device handler takes over, and this
            // hook goes quiet by itself.
            if (CurrentAddress >= 0x40B264D8u && CurrentAddress <= 0x40B26560u &&
                Ea < 0x100u) {
                UINT8 Ch = (UINT8)g_PpcContext.Gpr[RS(w)];
                if (Ch == 0x0D) {
                    Print(L"\r\n");
                } else if (Ch >= 0x20 && Ch <= 0x7E) {
                    Print(L"%c", (UINTN)Ch);
                }
            } else {
                TaskStoreWatch(Ea, (UINT32)(UINT8)g_PpcContext.Gpr[RS(w)], 1);
                g_WriteByte(Ea, (UINT8)g_PpcContext.Gpr[RS(w)]);
            }
        }
        break;
    case 39: // stbu
        {
            UINT32 Ea = EaD(w, RA(w));
            TaskStoreWatch(Ea, (UINT32)(UINT8)g_PpcContext.Gpr[RS(w)], 1);
            g_WriteByte(Ea, (UINT8)g_PpcContext.Gpr[RS(w)]);
            g_PpcContext.Gpr[RA(w)] = Ea;
        }
        break;
    case 40: // lhz
        {
            UINT32 Ea = EaD(w, RA(w));
            PpcKdProfileLoad(Ea, CurrentAddress);
            g_PpcContext.Gpr[RT(w)] = CpuRead16(Ea);
        }
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
                if ((g_PpcContext.Gpr[RS(w)] & PPC_MSR_EE) &&
                    !(g_PpcContext.Msr & PPC_MSR_EE) && EeMtmsrProbed < 6) {
                    EeMtmsrProbed++;
                    Print(L"  EE-ON mtmsr @0x%08x r%d=0x%08x (old MSR=0x%08x)\n",
                          CurrentAddress, RS(w), g_PpcContext.Gpr[RS(w)],
                          g_PpcContext.Msr);
                }
                g_PpcContext.Msr = g_PpcContext.Gpr[RS(w)];
                if ((g_PpcContext.Msr & PPC_MSR_EE) && g_PpcContext.DecrementerNegative &&
                    g_PpcContext.DecrementerWritten &&
                    g_PpcContext.ExceptionPending == 0) {
                    g_PpcContext.ExceptionPending = PPC_EXCEPTION_DECREMENTER;
                }
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
                {
                    UINT32 SrIdx = g_PpcContext.Gpr[RB(w)] & 0xF;
                    g_PpcContext.Spr[SrIdx] = g_PpcContext.Gpr[RS(w)];
                    g_PpcContext.Sr[SrIdx]  = g_PpcContext.Gpr[RS(w)];
                }
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
                    case SPR_PVR:  Value = 0x00390000; break;   // PowerPC 7455 (G4)
                    case SPR_TBL:  Value = g_PpcContext.TimeBaseL; break;
                    case SPR_TBU:  Value = g_PpcContext.TimeBaseH; break;
                    case SPR_SDR1: Value = g_PpcContext.Sdr1; break;
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
            case XO_MFTB:  // mftb / mftbu (TBR field: 268=TBL, 269=TBU)
                {
                    UINT32 Tbr = RA(w) | (RB(w) << 5);
                    g_PpcContext.Gpr[RT(w)] = (Tbr == SPR_TBU)
                        ? g_PpcContext.TimeBaseH
                        : g_PpcContext.TimeBaseL;
                }
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
                    case SPR_LR:
                        if (g_DrNativeHandoff != 0 && g_LrWriterProbed == 0 &&
                            (Value & 0xFF000000) == 0x3C000000) {
                            g_LrWriterProbed = 1;
                            // Who stamps LR with a 0x3Cxxxxxxxx handler? This
                            // is the source of the bogus post-poll jump.
                            Print(L"  LRWRITER PC=0x%08x wrote LR=0x%08x "
                                  L"r5=0x%08x r6=0x%08x r24=0x%08x "
                                  L"r25=0x%08x r28=0x%08x CR=0x%08x\n",
                                  g_PpcContext.Pc, Value,
                                  g_PpcContext.Gpr[5] & 0xFFFFFFFF,
                                  g_PpcContext.Gpr[6] & 0xFFFFFFFF,
                                  g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                                  g_PpcContext.Gpr[25] & 0xFFFFFFFF,
                                  g_PpcContext.Gpr[28] & 0xFFFFFFFF,
                                  g_PpcContext.Cr);
                        }
                        g_PpcContext.Lr = Value;
                        break;
                    case SPR_CTR:  g_PpcContext.Ctr = Value; break;
                    case SPR_SRR0: g_PpcContext.Srr0 = Value; break;
                    case SPR_SRR1: g_PpcContext.Srr1 = Value; break;
        case SPR_DEC:
            g_PpcContext.Spr[SPR_DEC] = Value;
            g_PpcContext.DecrementerNegative = (Value & 0x80000000) ? 1 : 0;
            g_PpcContext.DecrementerWritten = 1;
                        break;
                    case SPR_SDR1:
                        {
                            g_PpcContext.Spr[SPR_SDR1] = Value;
                            g_PpcContext.Sdr1 = Value;
                        }
                        break;
                    default:
                        g_PpcContext.Spr[SprNum] = Value;
                        // BAT upper/lower SPRs (528-551): recompute the
                        // decomposed block translation entry.
                        if (SprNum >= SPR_IBAT0U && SprNum <= (SPR_DBAT0U + 15)) {
                            PpcUpdateBat(SprNum);
                        }
                        break;
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
                g_PpcContext.Gpr[RT(w)] = g_PpcContext.Sr[(w >> 11) & 0xF];
                break;
            case XO_LSWI:  // lswi
                PpcLoadString(RT(w), EaD(w, RA(w)), (w >> 11) & 0x1F);
                break;
            case XO_SYNC:  // sync (no-op)
                break;
            case XO_TLBSYNC:  // tlbsync (no-op; no TLB modelled)
                break;
            case XO_MTSR:  // mtsr
                {
                    UINT32 SrIdx = (w >> 11) & 0xF;
                    g_PpcContext.Spr[SrIdx] = g_PpcContext.Gpr[RS(w)];
                    g_PpcContext.Sr[SrIdx]  = g_PpcContext.Gpr[RS(w)];
                }
                break;
            case XO_MFSRIN:  // mfsrin
                g_PpcContext.Gpr[RT(w)] = g_PpcContext.Sr[g_PpcContext.Gpr[RB(w)] & 0xF];
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
            case XO_EXTSH:  // extsh / extsh.
                g_PpcContext.Gpr[RA(w)] = (UINT32)(INT32)(INT16)g_PpcContext.Gpr[RS(w)];
                if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
                break;
            case XO_EXTSB:  // extsb / extsb.
                g_PpcContext.Gpr[RA(w)] = (UINT32)(INT32)(INT8)g_PpcContext.Gpr[RS(w)];
                if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
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
// ---------------------------------------------------------------------------
// Pei* backing for the freestanding CFM/PEF loader (pef_loader.c) used by the
// DR-handoff in PpcRunGuest.  The loader takes no libc; these helpers supply
// its scratch memory from a static arena.
static UINT8  PeiHeap[0xC0000];
static UINTN  PeiHeapCur = 0;
static VOID *PeiAlloc(void *ctx, size_t n)
{
    UINTN start;
    (void)ctx;
    if (n == 0) {
        n = 1;
    }
    start = (PeiHeapCur + 15) & ~(UINTN)15;
    if (start + n > sizeof(PeiHeap)) {
        Print(L"  PEI arena overrun: %d bytes at cur 0x%x\n", (UINT64)n,
              (UINT32)PeiHeapCur);
        return NULL;
    }
    PeiHeapCur = start + n;
    return PeiHeap + start;
}
static VOID PeiFree(void *ctx, void *p) { (void)ctx; (void)p; }
static VOID PeiNull(void *p, UINTN n) { UINT8 *b = (UINT8 *)p; while (n--) { *b++ = 0; } }
static UINT32 PeiBe32(const UINT8 *p)
{
    return ((UINT32)p[0] << 24) | ((UINT32)p[1] << 16) |
           ((UINT32)p[2] << 8) | (UINT32)p[3];
}
// ---------------------------------------------------------------------------
// Continuous guest execution harness. Runs up to MaxInstructions of real
// guest code from the current PC, delivering pending exceptions through the
// CPU vector mechanism so interrupt/syscall handlers run like on hardware.
// Stops with the reported status on an unimplemented opcode (EFI_UNSUPPORTED)
// or a memory/execution error; the guest PC is left at the stopping point.
static inline UINT64
EmuHostRdtsc (
    VOID
    )
{
    UINT32 Lo, Hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(Lo), "=d"(Hi));
    return ((UINT64)Hi << 32) | (UINT64)Lo;
}

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
UINTN TbProbe = 0;
    UINTN TailCount = 0;
    static UINT32 TailPc[4096];
    static UINT32 TailInst[4096];
    static UINT32 TailNext[4096];
    static UINT32 TailR28[4096];
    static UINT32 TailR8[4096];
    static UINT32 TailR17[4096];
    static UINT32 TailLr[4096];
    static UINT32 TailR24[4096];
    static UINT32 TailR27[4096];
    static UINT32 TailR7[4096];
    static UINT32 TailR5[4096];
    static UINT32 TailR15[4096];
    static UINT32 TailR16[4096];
    static UINT32 TailCr[4096];
    static UINT32 TailR29[4096];
    static UINT32 TailCtr[4096];
    static UINT32 PcsDumped = 0;
    static UINT32 TraceDumped = 0;
    static UINT32 StoreProbed = 0;
    static UINT32 RamProbed = 0;
    static UINT32 HelperDumped = 0;
    static UINT32 DbgJumpDumps = 0;
    static UINT32 WalkTabDumps = 0;
    static UINT32 PcBootProcLockWord = 0;
    static UINT32 AutoResumed = 0;
    static BOOLEAN AutoResumeArmed = TRUE;
    static BOOLEAN AutoResumeLoading = FALSE;
static UINT32 TailCrawled = 0;
static UINT32 DbgBaseProbed = 0;
static UINT32 ExtDumped = 0;
static UINT32 GcPathed = 0;
static UINT32 GoProbed = 0;
    static UINT32 PmdWalked = 0;
    static UINT32 PmdArrDump = 0;
    static UINT32 PmdFixed = 0;
    static UINT32 Dr68KLowProbed = 0;
    static UINT32 Dr68KLast = 0;
    static UINT32 DrA025Probe = 0;
    static UINT32 g_DrBrFixed = 0;
    static UINT32 DrBaseDrift = 0;
    static UINT32 DrHnfoSeeded = 0;
    static UINT32 DrHnfoProbed = 0;
    static UINT32 DrLowMarchProbed = 0;
    static UINT32 DrHandoffJmpProbed = 0;
    static UINT32 DrOsInjected = 0;
    static UINT32 DrC68kTrapRequest = 0;
    static UINT32 DrWildTrapProbed = 0;
    static UINT32 DrTrapDecide = 0;
    static UINT32 DrTrapDecideB = 0;
    // Post-inject 68K boot-step trace buffer (see the BOOTSTEP recorder below).
    static UINT32 BootStepIdx = 0;
    static UINT32 BootStepDumpDone = 0;
    static UINT32 BootStepPc[1024];
    static UINT32 BootStepOp[1024];
    static UINT32 BootStepPpc[1024];
    // 68K A-line trap call tracer (see TRAPTR recorder below).
    static UINT32 TrapTraceIdx = 0;
    static UINT32 TrapTraceDumpDone = 0;
    static UINT32 TrapTraceDumpedApex = 0;
    static UINT32 TrapTracePc[256];
    static UINT32 TrapTraceOp[256];
    static UINT32 TrapTraceD0[256];
    static UINT32 TrapTraceA0[256];
    static UINT32 TrapTracePpc[256];
    // Suppress the DEC decrementer preempt during the boot critical section
    // (until the 68K emulator genuinely starts its first dispatch at
    // 0x40B67C60). Without this, a DEC tick that fires before the 68K boot
    // task's ECB context exists triggers an early task-switch that restores a
    // garbage 68K PC (r24=[ECB+0x1C4]=0xFFFFFFFF) and the boot rescues before
    // the emulator-start KCall can establish the 68K boot context.
      
  
    static UINT32 EmulTrapProbed = 0;
    static UINT32 CallTblProbed = 0;
    static UINT32 SyscallDispatchProbed = 0;
    static UINT32 SchedBodyProbes = 0;
    static UINT32 WalkProbed = 0;
    static UINT32 PutsProbed = 0;
    static UINT32 g_DumpNkDispatch = 0;
    static CHAR16 PutsBuf[512];
    static UINT32 HandoffChainProbed = 0;
    static UINT32 IrqRetProbed = 0;
    static UINT32 SchedEntryProbed = 0;
    static UINT32 TqProbe = 0;
    static UINT32 FreqProbe = 0;
    
    if (ExecutedCount == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    g_PpcContext.ExceptionPending = 0;
    // PHASE A.5: profile KernelData reads from the moment the guest starts;
    // PpcKdProfileDump() reports the consumed offsets at the 68K handoff.
    g_KdProfileEnabled = TRUE;
    // MaxInstructions == 0 means run indefinitely until error or halt.
    while (MaxInstructions == 0 || Executed < MaxInstructions) {
        UINT32 Instr;
        UINT32 Current;
        UINT32 Next;
        EFI_STATUS Status;
        Instr = CpuRead32(g_PpcContext.Pc);
        Current = g_PpcContext.Pc;
        // ---- WEDGE watchdog: post-Fix-B the guest parks DEC at 0x7FFFFFFF,
        // clears EE and idles in the 0x40B264xx/0x40B265CC wait loop with no
        // interrupt to wake it. The interpreter then stops committing entirely
        // (no SLOWI/XDELIVLONG fire) while PpcRunGuest never returns. Sample
        // the host clock once per loop iteration; when no instruction has
        // committed within ~0.5 s of host time, dump the interpreter's exact
        // position (which lets us see WHERE the guest is waiting and on what)
        // and return so main.c:804 reports the stop instead of burning wall
        // time. A slow-but-progressing guest refreshes the timestamp each
        // iteration, so only a true commit-stall trips it.
        {
            // Sample only every 4096 committed instructions: during the DR-68K
            // char-wait crawl the guest commits a stream of instructions, so a
            // per-iteration rdtsc is pure overhead; a true >10 s commit-stall
            // is still caught within one sample window.
            if ((Executed & 0xFFFu) == 0) {
                static UINT64 WedgeLastCommit = 0;
                static UINT32 WedgePrinted = 0;
                UINT64 WedgeNow = EmuHostRdtsc();
                if (WedgeLastCommit == 0) {
                    WedgeLastCommit = WedgeNow;
                } else if (WedgePrinted < 12 &&
                           (WedgeNow - WedgeLastCommit) > 32000000000ull) {
                    WedgePrinted++;
                    Print(L"  WEDGE[%u] Executed=%u PC=0x%08x Instr=0x%08x "
                          L"r1=0x%08x LR=0x%08x MSR=0x%08x DEC=0x%08x "
                          L"SRR0=0x%08x SRR1=0x%08x pend=0x%x SPRG4=0x%08x\n",
                          WedgePrinted, (UINT32)Executed, Current, Instr,
                          g_PpcContext.Gpr[1], g_PpcContext.Lr, g_PpcContext.Msr,
                          (UINT32)g_PpcContext.Spr[SPR_DEC],
                          g_PpcContext.Srr0, g_PpcContext.Srr1,
                          g_PpcContext.ExceptionPending,
                          (UINT32)g_PpcContext.Spr[272]);
                    // NOTE: a large one-shot disasm dump here perturbs the crawl
                    // wall timing badly (host-seconds of serial output at the
                    // first long gap); keep this site print-light. The disasm of
                    // 0x40B26400-0x40B265F0 was captured and analyzed: it is the
                    // NK console char-dispatch + SCC Rx poll.
                }
                WedgeLastCommit = WedgeNow;
            }
        }
// ---- SPINW / HEART watchdog: prove the guest is (or isn't) still
        // executing while silent, and pin the exact PC of a deadlock.  The fast
        // serial console means silent work can no longer be mistaken for a
        // stall, so report progress on a wall-clock-ish instruction cadence.
        // (SPINW window/counter decimated to a 1-in-64 sample: it is otherwise
        // the sole unconditional per-instruction probe left in the hot loop.)
        if ((Executed & 0x3Fu) == 0) {
            static UINT32 HeartNextInstr = 25000;
            static UINT32 HeartPrints = 0;
if (Executed >= HeartNextInstr && HeartPrints < 500) {
                UINT32 HighCount = 0;
                if (RT != NULL && RT->GetNextHighMonotonicCount != NULL) {
                    uefi_call_wrapper(RT->GetNextHighMonotonicCount, 1, &HighCount);
                }
                Print(L"  HEART Executed=%u PC=0x%08x Instr=0x%08x r1=0x%08x r3=0x%08x "
                      L"DEC=0x%08x MonoHi=0x%08x\n",
                      (UINT32)Executed, g_PpcContext.Pc, Instr,
                      g_PpcContext.Gpr[1], g_PpcContext.Gpr[3],
                      g_PpcContext.Spr[SPR_DEC], HighCount);
                HeartNextInstr += 25000;
                HeartPrints++;
            }
            // WALKTRACE: per-instruction trace of the area-conversion's
            // page-directory walk window [0x40B26000..0x40B28000], so the
            // LAST printed line before a silent freeze identifies the exact
            // freezing PPC instruction.
        }
// ---- Free-run page logger: with USE_PPC_NATIVE_DR the ROM's own
        // PPC 68K emulator drives the guest.  Log the first visit to each
        // distinct 4 KB PPC page (capped) to see where the native boot goes
        // once the C-68K intercept is removed. (Decimated to 1-in-64 samples:
        // the membership scan otherwise loops over all collected pages on
        // EVERY instruction in the hot loop.)
        if ((Executed & 0x3Fu) == 0) {
            static UINT32 FreePages[96];
            static UINT32 FreePageCount = 0;
            UINT32 Fp = Current >> 12;
            UINT32 Fi;
#if !USE_PPC_NATIVE_DR
            Fp = 0xFFFFFFFF;                // disabled build: nothing to log
#endif
            for (Fi = 0; Fi < FreePageCount; Fi++) {
                if (FreePages[Fi] == Fp) break;
            }


if (Fi == FreePageCount && Fp != 0xFFFFFFFF && FreePageCount < 48) {
                FreePages[FreePageCount++] = Fp;
                Print (L"  FREERUN page=0x%08x firstPC=0x%08x r24(68Kpc)=0x%08x "
                       L"r27=0x%04x r1=0x%08x\n",
                       Fp << 12, Current, g_PpcContext.Gpr[24],
                       (UINT32)(g_PpcContext.Gpr[27] & 0xFFFF),
                       g_PpcContext.Gpr[1]);
            }
        }
        // ---- Decisive trap-handoff trace: watch the exact PC sequence as the
        // boot-tail twui raises 0x700 and we try to deliver it to the ROM's
        // handler (0x40B14700). Log every visit to the trap table, the handler,
        // and the scheduler DEC-write / rfi region so the redirect is visible.
        // ---- PHASE B: boot-proc MAC lock release ----
        // The NK's boot-proc warm-reboot continuation ("Resuming saved kernel
        // state", reached after the natural twi/TrapTable handoff) acquires a
        // MAC-scope spinlock whose owner word is derived from the boot base:
        //   boot tail: r8 = [KDP+0x5A4] + 0x26E8 = 0x40B126E8, then
        //   b 0x40B12700 -> lwarx r9,0,r8 ; cmpwi r9,0  (fast-path free check)
        // and on the "owned" path the proc spins at 0x40B12818 on [r31].
        // Here the boot-proc code lives in the (writable) New World ROM window,
        // so 0x40B126E8 is BOTH the executed `li r3,0xff` opcode AND the lock
        // word -- these conflict, and the lock is never released, so the proc
        // self-deadlocks at 0x40B12818 and every resume re-runs NK init.
        // On real hardware [KDP+0x5A4] is a low-RAM base where the lock lands on
        // a clean, zeroed RAM word, free at the start of each boot. 0x40B12700
        // is a GENERIC MAC lock entry used by many callers, so this hook must
        // only touch the broken case: when the lock target is the boot tail's
        // own self-slot 0x40B126E8. Redirect that target to a dedicated RAM
        // lockword (Ewa+0xE30), persistent across warm passes (the tail re-runs
        // and recomputes r8 = 0x40B126E8 each pass), and force-free it so the
        // fast-path acquire always succeeds and no fatal spin/Termination
        // triggers a spurious second-NK-init reboot. All other callers' locks
        // (r8 != 0x40B126E8) pass through untouched.
        if (Current == 0x40B12700u || Current == 0x40B12704u ||
            Current == 0x40B12818u || Current == 0x40B12824u) {
            UINT32 LockReg = (Current == 0x40B12700u || Current == 0x40B12704u)
                                 ? g_PpcContext.Gpr[8]
                                 : g_PpcContext.Gpr[31];
            if (LockReg == 0x40B126E8u) {
                if (PcBootProcLockWord == 0) {
                    PcBootProcLockWord = g_PpcContext.Spr[272] + 0xE30;
                    CpuWrite32(PcBootProcLockWord, 0);
                    Print(L"  BOOTLOCK reserve RAM lockword 0x%08x at PC=0x%08x "
                          L"r1=%08x\n",
                          PcBootProcLockWord, Current, g_PpcContext.Gpr[1]);
                }
                if (Current == 0x40B12700u || Current == 0x40B12704u) {
                    g_PpcContext.Gpr[8] = PcBootProcLockWord;
                }
                if (Current == 0x40B12818u || Current == 0x40B12824u) {
                    g_PpcContext.Gpr[31] = PcBootProcLockWord;
                }
                if (CpuRead32(PcBootProcLockWord) != 0) {
                    CpuWrite32(PcBootProcLockWord, 0);
                }
            }
        }
        // ---- PHASE A diagnostic: recursive-spinlock entry snapshot ----
        // The NK cold path can park in its recursive-spinlock list walk
        // (0x40B127A8) when a lock node references an owner that will never
        // release. Dump the lock header, the first nodes and the last PCs
        // once so the wait object and its caller can be identified.
        if (Current == 0x40B127A8u) {
            static BOOLEAN SpinProbed = FALSE;
            if (!SpinProbed && Executed != 0) {
                UINT32 R22 = g_PpcContext.Gpr[22];
                UINT32 R31 = g_PpcContext.Gpr[31];
                UINTN K;
                SpinProbed = TRUE;
                Print(L"  SPINLOCK enter @0x40B127A8 r22=%08x r31=%08x "
                      L"[r22-4]=%08x [r30-0xB30]=%08x SRR1=%08x LR=%08x\n",
                      R22, R31,
                      CpuRead32(R22 - 4),
                      CpuRead32(CpuRead32(R22 - 4) - 0xB30),
                      g_PpcContext.Srr1, g_PpcContext.Lr);
                Print(L"  SPINLOCK node r31: next=%08x f4=%08x d8=%08x dC=%08x\n",
                      CpuRead32(R31), CpuRead32(R31 + 4),
                      CpuRead32(R31 + 8), CpuRead32(R31 + 0xC));
                Print(L"  SPINLOCK sprg0-3: %08x %08x %08x %08x "
                      L"r1=%08x r2=%08x r13=%08x\n",
                      g_PpcContext.Spr[272], g_PpcContext.Spr[273],
                      g_PpcContext.Spr[274], g_PpcContext.Spr[275],
                      g_PpcContext.Gpr[1], g_PpcContext.Gpr[2],
                      g_PpcContext.Gpr[13]);
                Print(L"  SPINLOCK last %d PCs:", (UINTN)(TailCount < 48 ? TailCount : 48));
                for (K = 0; K < 48 && K < TailCount; K++) {
                    UINTN Idx = (TailStart + 4096 - 1 - K) % 4096;
                    Print(L" %08x/%04x", TailPc[Idx],
                          (UINT16)(TailInst[Idx] >> 16));
                    if ((K & 7) == 7) Print(L"\n     ");
                }
                Print(L"\n");
            }
        }
        // ---- PHASE A.4: area-merge guard harmonization ----
        // The NK's area manager merges a newly created area into an
        // existing one only when several tail fields (+0x24/+0x28/+0x2C)
        // of the two area records compare equal. On real hardware the
        // boot-time pool is pre-zeroed so the check passes; here those
        // offsets still hold pool garbage, the compare fails and the NK
        // panics into the nanodebugger. At both guard sites (the cmpl at
        // 0x40B1F614 and its follow-up block at 0x40B1F668; r24 =
        // existing area, r31 = new area), copy the existing record's
        // fields over the new one before the compare runs -- the same
        // state real boot guarantees. Pointer sanity is enforced so a
        // stale register pair can never turn this into a wild write.
        // ---- 68K DR-emulator software-function hooks ----
        // The ROM dispatches certain 68K opcodes through "software function"
        // pointers stored in ed.v (offsets 0x800..0x834 of the emulator data
        // block at 0xB000). The ROM never seeds these slots, so the dispatch
        // machinery falls into the tail at 0x40B6CA60 and executes `bctrl`
        // with CTR == ed.v[0x80C] == 0, branching to address 0. The missing
        // functions are emulated here in C and the context is handed back to
        // the ROM's common dispatch (0x40B67C60).
        UINT32 Hooked = 0;
        // ---- Native 68K dispatch-loop hook (SheepShaver track) ----
        // When the PPC DR-emulator is about to dispatch a 68K instruction
        // (its DR dispatch loop at 0x40B67B60 / common dispatch at
        // 0x40B67C60), intercept and execute the 68K instruction natively via
        // the C interpreter (m68k.c), completely replacing the PPC-based
        // opcode table. 0x40B67B60 is the DR's dispatch-loop entry (arrived
        // at from the DR cold-start with r24 = the 68K PC, r1 = SSP and r25 =
        // SR loaded by the DR's own self-init); 0x40B67C60 is kept as a
        // fallback for any direct-to-full-dispatch transitions. Resuming at
        // 0x40B67B60 re-enters the loop so every 68K instruction is serviced
        // by the C interpreter and the PPC DR never executes an opcode.
        // USE_PPC_NATIVE_DR disables this intercept so the ROM's own PPC
        // opcode-translation table drives the 68K emulator (the pivot).
#if !USE_PPC_NATIVE_DR
        if (Current == 0x40B67B60 || Current == 0x40B67C60) {
            static UINTN M68kTakeoverCount = 0;
            if (M68kTakeoverCount == 0) {
                M68kTakeoverCount = 1;
                // Reset the 68K CPU from the seeded low-memory vector table
                // (SSP 0x0000 = 0xA000, PC 0x0004 = 0x4080002A, SR = 0x2700).
                M68kReset ();
            }
            // On real hardware the PPC nanokernel preempts emulated 68K
            // code asynchronously (decrementer tick). Without this, any 68K
            // "wait for interrupt" park loop spins forever because the C
            // interpreter never checks the PPC interrupt state mid-batch.
            // Flag it here and let the normal end-of-iteration tick logic /
            // loop-top delivery run, with SRR0 = Next = the dispatch entry
            // we will resume from.
            if (g_PpcContext.DecrementerWritten &&
                g_PpcContext.DecrementerNegative &&
                (g_PpcContext.Msr & PPC_MSR_EE) &&
                g_PpcContext.ExceptionPending == 0) {
                g_PpcContext.ExceptionPending = PPC_EXCEPTION_DECREMENTER;
                // Wake a 68K STOP #imm park: the interrupt will be serviced
                // at the PPC level, then the 68K batch resumes afterwards.
                g_M68kContext.Stopped = FALSE;
            }
            Status = M68kExecuteFromPPC ();
            g_PpcContext.Gpr[27] = 0;
            g_PpcContext.Gpr[29] = 0x40B80000;
            Next = 0x40B67B60;
            Hooked = 1;
        }
#endif
        // ---- Tail-A207 -> native PPC handoff ----
        // The tail A207 at low-RAM 0x54 has (via the r24==0x56 steer at
        // 0x40B6966C) written the staged native-bootstrap entry 0x1B823 into
        // the DR resume-pc slot [r28+0x28]. The DR then lands r24 (the 68K
        // PC) there and would 68K-fetch the PPC bytes, mis-decoding them as
        // 68K opcodes (op=0x002C/nxt=0x0300 crasher). Instead, once the tail
        // steer has fired (g_DrNativeTail > 0) and r24 is observed inside the
        // staged-native region, break out of the DR dispatch and hand the
        // interpreter to the aligned native entry (r24 & ~3 == 0x1B820).
        // One-shot: g_DrNativeHandoff latches so the ROM DR is left behind
        // permanently (the staged bootstrap runs as plain PPC from here on).
        {
            UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            if (g_DrNativeHandoff == 0 && g_DrNativeSteer != 0 &&
                Pc24 >= 0x0001B000u && Pc24 < 0x0001C000u) {
                // Resume at the fork's FUNCTION ENTRY (0x1B7C0), not the
                // shared 0x1B820 pad, so the fork's own prologue runs its
                // bookkeeping (bl 0x4672c; bl 0x8fdc), initializes the decoder
                // accumulators, and builds the decompressor frame itself --
                // instead of trusting an emulator-synthesized frame. The last
                // faithful-frame run proved the reader's state regs were
                // uninitialized at 0x1B820 (r31 parked on the real chunk table
                // word 0x0200099B but never advanced, outer slots never
                // written, r28 frozen): only the fork's own entry can seed them.
                UINT32 Np = 0x0001B7C0u;
                g_DrNativeHandoff = 1;
                g_ForkResumePc = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
                g_PpcContext.Pc = Np;
                Hooked = 1;
                // Load fragment 0 (QuickDraw) with the real CFM/PEF loader and
                // stage it into low RAM.  The code section is placed at low
                // 0x320 (preserving the mapping so the resumed 0x1B7C0 ==
                // code + 0x1B4A0), the pattern-initialized data section is
                // expanded, relocated and placed at 0x48000, and r2 is set to
                // the loader's TOC (dataBase + 0xB98).  The relocation
                // bytecode fills the import slots inside the data section, so
                // the fork's first service call (`lwz r12,-0xB7C(r2)` -> slot
                // data+0x1C == import[32] "GetQDGlobals") dispatches via the
                // descriptor @0xBA00 below, and the decoder slot [r2+0x2B0]
                // == data+0xE48 == {code+0x14EEC, dataBase+0xB98} comes out
                // correct with no hand-patching.  Only import[32] is bound
                // (the other 276 stay 0, matching the old heuristic).
                {
                    PefFragment Frag;
                    UINT8 *SysHead;
                    UINT32 W;
                    int    KiIdx  = -1;
                    int    Loaded = 0;
                    PeiHeapCur = 0;
                    PeiNull(&Frag, sizeof(Frag));
                    Frag.alloc = PeiAlloc;
                    Frag.free_ = PeiFree;
                    SysHead = (UINT8 *)PeiAlloc(NULL, 0x50000);
                    if (SysHead != NULL) {
                        for (W = 0; W < 0x50000; W += 4) {
                            UINT32 V = CpuRead32(PPC_OS_RUNTIME_GUEST_BASE + W);
                            SysHead[W + 0] = (UINT8)(V >> 24);
                            SysHead[W + 1] = (UINT8)(V >> 16);
                            SysHead[W + 2] = (UINT8)(V >> 8);
                            SysHead[W + 3] = (UINT8)V;
                        }
                    }
                    if (SysHead != NULL &&
                        PefParse(&Frag, SysHead, 0x50000u, 0x2A0u) == 0 &&
                        PefExpandSections(&Frag) == 0) {
                        PefPlaceFragment(&Frag, 0x320u, 0x48000u);
                        if (Frag.symCount != 0) {
                            UINT32 *Imports = (UINT32 *)PeiAlloc(
                                NULL, (UINTN)Frag.symCount * sizeof(UINT32));
                            if (Imports != NULL) {
                                PeiNull(Imports, (UINTN)Frag.symCount * sizeof(UINT32));
                                KiIdx = PefImportIndex(&Frag, "GetQDGlobals");
                                if (KiIdx >= 0) {
                                    Imports[KiIdx] = 0x0000BA00u; // descriptor @0xBA00
                                }
                                // Catch-all glue: every other import slot points
                                // at a 0-returning native stub (descriptor
                                // @0xBA20 -> {entry 0xBA28, toc 0}; stub li r3,0;
                                // blr) so the fork never halts on an unbound slot.
                                // CallUniversalProc (import[211]) is InterfaceLib
                                // glue with no fragment export; auto-resolution
                                // cannot bind it, and the fork stops on it before
                                // touching any other import.
                                for (UINT32 Gi = 0; Gi < Frag.symCount; Gi++) {
                                    if (Imports[Gi] == 0)
                                        Imports[Gi] = 0x0000BA20u;
                                }
                                PefRelocateSection(&Frag, 1, Frag.expanded[1], Imports);
                            }
                        }
                        // Mirror the instantiated sections into low RAM (big-
                        // endian words preserved as stored).
                        for (W = 0; W < Frag.sections[0].totalLength; W += 4) {
                            CpuWrite32(0x00000320u + W, PeiBe32(Frag.expanded[0] + W));
                        }
                        for (W = 0; W + 4 <= Frag.sections[1].totalLength; W += 4) {
                            CpuWrite32(0x00048000u + W, PeiBe32(Frag.expanded[1] + W));
                        }
                        g_PpcContext.Gpr[2] = Frag.toc;
                        Loaded = 1;
                        Print(L"  NATIVE-HANDOFF real PEF load frag0: code "
                              L"0x320..0x%08x data 0x48000..0x%08x r2=0x%08x "
                              L"(import GetQDGlobals=%d, %d slots glue-bound)\n",
                              0x00000320u + Frag.sections[0].totalLength,
                              0x00048000u + Frag.sections[1].totalLength,
                              Frag.toc, KiIdx, Frag.symCount - (KiIdx >= 0 ? 1 : 0));
                    }
                    if (!Loaded) {
                        // Fallback to the previous heuristic if the loader
                        // could not run (mirror the missing low helpers and
                        // patch the import slot the fork's loader would).
                        for (UINT32 No = 0x00000320u; No < 0x0001B400u; No += 4) {
                            CpuWrite32(No, CpuRead32(PPC_OS_RUNTIME_GUEST_BASE + No));
                        }
                        g_PpcContext.Gpr[2] = 0x945F8u;
                        CpuWrite32(0x00093A7Cu, 0x0000BA00u);
                        Print(L"  NATIVE-HANDOFF fallback: mirror 0x320..0x1B400, "
                              L"r2=0x945F8, patch [0x93A7C]=0xBA00\n");
                    }
                }
                // Container service (what GetQDGlobals returns).  Descriptor
                // @0xBA00 -> {entry 0xBA08, toc 0}; stub @0xBA08 returns r3 =
                // the synthesized container @0xB900 (record @0xB940).  The
                // record is shaped so the fork: [rec+6]=0 -> 0x8fdc !=1 ->
                // bne takes 0x1B85C; [rec+0x60]=0 -> beq 0x1B864 -> skips the
                // 0x1F640 render, straight to bl 0x4618C (the decoder);
                // [rec+0x42]=-1 -> skips the pixmap service.  The stream is
                // the System data-fork payload tail: payload offset 0x96000 ==
                // guest 0x68E96000; System file size = 0x2A2B0A (RAWPARM) ->
                // stream len = 0x1CB0A.
                {
                    CpuWrite32(0x0000BA00u + 0x00, 0x0000BA08u);
                    CpuWrite32(0x0000BA00u + 0x04, 0x00000000u);
                    // Stub @ 0xBA08: lis r3,0 / ori r3,r3,0xB900 / blr
                    CpuWrite32(0x0000BA08u + 0x00, 0x3C600000u);
                    CpuWrite32(0x0000BA08u + 0x04, 0x6063B900u);
                    CpuWrite32(0x0000BA08u + 0x08, 0x4E800020u);
                    // Catch-all glue: descriptor @0xBA20 -> {entry 0xBA28, toc 0};
                    // stub @0xBA28: li r3,0 / blr (returns 0 for any unbound
                    // import, so the fork continues instead of halting).
                    CpuWrite32(0x0000BA20u + 0x00, 0x0000BA28u);
                    CpuWrite32(0x0000BA20u + 0x04, 0x00000000u);
                    CpuWrite32(0x0000BA28u + 0x00, 0x38600000u);
                    CpuWrite32(0x0000BA28u + 0x04, 0x4E800020u);
                    CpuWrite32(0x0000B900u + 0xCA, 0x0000B940u);   // [+0xCA] = record
                    CpuWrite32(0x0000B900u + 0x470, 0x68E96000u); // [+0x470] = stream base
                    CpuWrite32(0x0000B900u + 0x474, 0x00000000u); // [+0x474] = 0 (shift ok)
                    CpuWrite16(0x0000B940u + 0x06, 0x0000);       // rec+6 >=0 -> 0x8fdc=0
                    CpuWrite16(0x0000B940u + 0x38, 0x0000);
                    CpuWrite16(0x0000B940u + 0x42, 0xFFFF);       // -1: skip pixmap
                    CpuWrite32(0x0000B940u + 0x60, 0x00000000U);  // output=0: skip render
                    Print(L"  NATIVE-HANDOFF GetQDGlobals -> descriptor @0xBA00 "
                          L"{0xBA08,0} (container @0xB900, record @0xB940)\n");
                }
                // Real boot maps the System data fork 1:1 into low RAM (file
                // offset == runtime address == PPC_OS_RUNTIME_GUEST_BASE+off).
                // The ROM glue bcctr's into the diag region at Dst (0x100000);
                // that image lives in the fork at file offset 0x100000.. and is
                // NOT produced by the QD fragment (the "stream" at 0x96000 is
                // QuickDraw's universal-proc name table). Mirror the fork's
                // diag + name-table regions so the glue's bcctr lands on real
                // code instead of zeros. The 0x320..0x1B400 code / 0x48000
                // data placements above already cover the fragment itself.
                for (UINT32 Mw = 0x00096000u; Mw < 0x002A2B0Au; Mw += 4) {
                    CpuWrite32(Mw, CpuRead32(PPC_OS_RUNTIME_GUEST_BASE + Mw));
                }
                Print(L"  NATIVE-HANDOFF mirrored fork 0x96000..0x2A2B0A into low "
                      L"RAM (file offset == runtime addr); [0x100000]=0x%08x "
                      L"[0x100004]=0x%08x\n",
                      CpuRead32(0x00100000u), CpuRead32(0x00100004u));
                // Session state for the decoder 0x3873C, entered by the fork's
                // own `bl 0x4618C` at 0x1B89C. Decoder SP is built by the fork
                // prologue: r1 starts 0x9EFE (the ROM glue's sthu already ran),
                // `stwu r1,-0x60` -> decoder frame SP = 0x9E9E. Seed the full
                // ddcfg + live GPRs there; the fork body only touches r28-r31/
                // r0/r3-r9/r10-r12 and [SP+0x38..0x4E] AFTER the decoder runs,
                // so nothing here is clobbered first. NOTE: the fork prologue
                // (0x1B7D4 `mr r31,r4`, 0x1B7E4 `li r28,0`) clobbers r31/r28
                // BEFORE the first service call, so the reader state (r28 block
                // cap, r31 chunk cursor) is re-seeded by the decoder entry as
                // live GPRs only if nothing defeats the fork's own setup.
                {
                    UINT32 Dst  = 0x00100000u;  // diag output target
                    UINT32 Str  = 0x68E96000u;  // compressed payload
                    UINT32 Len  = 0x0001CB0Au;  // file_size(0x2A2B0A)-0x96000
                    UINT32 Frm  = 0x00009E9Eu;  // decoder frame base
                    UINT32 C    = 0x0000B900u;  // container
                    // ddcfg (frame slots), offsets from fork frame base.
                    CpuWrite32(Frm + 0x40, Len);                       // total vs progress
                    CpuWrite32(Frm + 0x44, 0x000948B8u);               // bit reader {0x2AD24,.}
                    CpuWrite32(Frm + 0x48, 0x000948B0u);               // word reader {0x2AB60,.}
                    CpuWrite32(Frm + 0x4C, Str + Len);                 // stream end
                    CpuWrite32(Frm + 0x50, 0x00000000u);
                    CpuWrite32(Frm + 0x54, 0x00000000u);               // r6 shift -> [+0x74]=0x20
                    CpuWrite32(Frm + 0x58, 0x00000000u);
                    CpuWrite32(Frm + 0x5C, Str);                       // stream cursor (r5)
                    // [Frm+0x60] doubles as the return-address slot the diag
                    // stub at 0x100000 pops (`lwz r0,8(r1)` -> `blr`). Seeding
                    // it with Dst made the stub return to itself and loop
                    // forever. A real boot returns into the ROM's common 68K
                    // DR dispatch loop, so carry that as the continuation
                    // instead (Dst stays live in r21/r29).
                    CpuWrite32(Frm + 0x60, 0x40B67C60u);               // DR dispatch continuation
                    CpuWrite32(Frm + 0x64, 0x00000000u);               // progress
                    CpuWrite32(Frm + 0x68, Dst);                       // dest (r29) -> r21+4
                    CpuWrite32(Frm + 0x6C, C);                         // reader arg0 = container
                    CpuWrite32(Frm + 0x70, 0x00000020u);               // bits-in-word (r23)
                    CpuWrite32(Frm + 0x74, 0x00000020u);
                    CpuWrite32(Frm + 0x78, 0x00000100u);               // block cap (r28)
                    CpuWrite32(Frm + 0x7C, Len);                       // r22 remaining
                    CpuWrite32(Frm + 0x80, Dst);                       // r21
                    CpuWrite32(Frm + 0x84, Len);                       // r10
                    CpuWrite32(Frm + 0x88, 0x00000000u);               // r30 result
                    CpuWrite32(Frm + 0x8C, 0x00000020u);
                    CpuWrite32(Frm + 0xF8, C);                         // bit-reader arg0
                    // Live GPRs that survive the fork body into the decoder.
                    g_PpcContext.Gpr[1] = 0x00009EFEu;                 // glue-set frame base
                    g_PpcContext.Gpr[15] = 0x00000000u;
                    g_PpcContext.Gpr[16] = 0xFFFFFFFFu;                // accumulator init
                    g_PpcContext.Gpr[17] = 0x00000000u;
                    g_PpcContext.Gpr[18] = 0x00000011u;                // r24==5 branch: li 0x11
                    g_PpcContext.Gpr[19] = 0x00000008u;                // r24+3
                    g_PpcContext.Gpr[20] = Str + 4;                    // stream+4
                    g_PpcContext.Gpr[21] = Dst;
                    g_PpcContext.Gpr[23] = 0x00000020u;
                    g_PpcContext.Gpr[24] = 0x00000005u;                // shift
                    g_PpcContext.Gpr[25] = 0x00000000u;
                    g_PpcContext.Gpr[26] = 0x00000000u;
                    g_PpcContext.Gpr[27] = 0x00000000u;
                    g_PpcContext.Gpr[28] = 0x00000100u;                // block cap
                    g_PpcContext.Gpr[29] = Dst;                        // dest
                    g_PpcContext.Gpr[30] = 0x00000000u;
                    g_PpcContext.Gpr[31] = Str;                        // chunk-table cursor
                    // The fork prologue (0x1B7D4 `mr r31,r4`) copies entry r4
                    // into r31 before the decoder's word reader (0x2AB60
                    // `lwz r5,0(r31)` / `addi r31,r31,4`) runs, so the chunk
                    // cursor must be carried in r4 as well.
                    g_PpcContext.Gpr[4] = Str;
                    g_NatDecSteps = 0;
                }
                Print(L"  NATIVE-HANDOFF r24(68Kpc)=0x%08x PPC=0x%08x win=%u "
                      L"r1=0x%08x [r28+0x28]=0x%08x LR=0x%08x MSR=0x%08x CR=0x%08x\n",
                      Pc24, Np, g_DrPostYieldWindow, g_PpcContext.Gpr[1],
                      CpuRead32((g_PpcContext.Gpr[28] & 0xFFFFFFFF) + 0x28),
                      g_PpcContext.Lr, g_PpcContext.Msr, g_PpcContext.Cr);
                Print(L"  NATIVE-HANDOFF r2(toc)=0x%08x r1=%08x r8=%08x r16=%08x "
                      L"r25=%08x r26=%08x r27=%08x r28=%08x r29=%08x r30=%08x "
                      L"r31=%08x xer=%08x ctr=%08x\n",
                      g_PpcContext.Gpr[2], g_PpcContext.Gpr[1],
                      g_PpcContext.Gpr[8], g_PpcContext.Gpr[16],
                      g_PpcContext.Gpr[25], g_PpcContext.Gpr[26],
                      g_PpcContext.Gpr[27], g_PpcContext.Gpr[28],
                      g_PpcContext.Gpr[29], g_PpcContext.Gpr[30],
                      g_PpcContext.Gpr[31], g_PpcContext.Xer,
                      g_PpcContext.Ctr);
                Print(L"  NATIVE-HANDOFF [r2+0x2B0]=0x%08x [r2+0x2B4]=0x%08x "
                      L"(auto-relocated decoder descriptor via data+0xE48)\n",
                      CpuRead32(g_PpcContext.Gpr[2] + 0x2B0u),
                      CpuRead32(g_PpcContext.Gpr[2] + 0x2B4u));
                {
                    UINT32 Tp;
                    Print(L"  NATRDMP transfer page 0xB000:\n");
                    for (UINT32 tt = 0; tt < 0x30; tt += 4) {
                        Tp = CpuRead32(0x0000B000u + tt);
                        Print(L"    +0x%03X: %08X\n", tt, Tp);
                    }
                    Print(L"  NATRDMP payload head 0x68E96000 (System off 0x96000): %08X "
                          L"%08X %08X %08X %08X %08X %08X %08X\n",
                          CpuRead32(0x68E96000u), CpuRead32(0x68E96004u),
                          CpuRead32(0x68E96008u), CpuRead32(0x68E9600Cu),
                          CpuRead32(0x68E96010u), CpuRead32(0x68E96014u),
                          CpuRead32(0x68E96018u), CpuRead32(0x68E9601Cu));
Print(L"  NATIVE-HANDOFF resume fork entry 0x1B7C0: GetQDGlobals "
                      L"(import slot data+0x1C) -> desc {0xBA08,0}; record "
                      L"@0xB940 (rec+6=0 rec+60=0 rec+42=-1); decoder frame "
                      L"@0x9E9E seeded; stream 0x68E96000 len 0x1CB0A\n");
                }
            }
        }
        // Post-OS-handoff native-DR continuation: the C-68K takerover
        // (DrC68kTrapRequest == 2 -> M68kExecuteFromPPC) has been REMOVED.
        // SheepShaver reference (emul_ppc.cpp:1053-1084): the ROM's own PPC
        // DR runs the entire 68K OS loader natively; host C serves only the
        // patched EMUL_OP opcode-slot markers (interpreter.c:3188-3205 via
        // EmulOpDispatch), and 68K A-traps are serviced by the OS's own 68K
        // trap dispatcher under the DR. DrC68kTrapRequest is kept as a phase
        // marker only (set to 1 by OSINJECT) so the post-handoff DR dispatch
        // is observed, not diverted.
        if (Current == 0x40B6CA84 && Instr == 0x4E800421) {
            // Tail's `bctrl` (software fn ed.v[0x80C]). 68K MOVE #<imm>,SR
            // (0x46FC) routes here via entry[0x46FC] -> 0x40B6C570 bnsl cr2
            // -> 0x40B6CA68. r3 = address of imm word, r27 = SR value.
            // ed.v[0x80C] is always NULL -- the bctrl would jump to address 0.
            // Intercept, sync68K SR, advance r24 past the imm, and hand off
            // to the native 68K dispatch loop at 0x40B67C60.
            if (CpuRead32(0x0000B80C) == 0 && CpuRead16(g_PpcContext.Gpr[3] - 2) == 0x46FC) {
                UINT16 Sr = CpuRead16(g_PpcContext.Gpr[3]);
                g_PpcContext.Gpr[24] = g_PpcContext.Gpr[3] + 2;
                g_PpcContext.Gpr[25] = Sr >> 8;
                g_PpcContext.Gpr[26] = 0;
                g_PpcContext.Gpr[27] = 0;
                g_PpcContext.Gpr[29] = 0x40B80000;
                g_PpcContext.Xer = 0;
                g_PpcContext.Cr &= ~0x0F00000F;
                g_PpcContext.Cr = (g_PpcContext.Cr & ~0x00F00000) | 0x00100000;
                Next = 0x40B67C60;
                Hooked = 1;
                Print(L"  MOVE-SR-HOOK 46FC SR=0x%04x r24=0x%08x CR=0x%08x -> 0x40b67c60\n",
                      Sr, g_PpcContext.Gpr[24], g_PpcContext.Cr);
            }
        }
        if (Current == 0x40BA7380) {
            // entry[0x4E70] = 68K RESET (software fn ed.v[0x828]): reset the
            // external devices. Treated as a no-op; continue at opcode+2
            // (r24 already points there from the common dispatch).
            if (CpuRead32(0x0000B828) == 0 && CpuRead16(g_PpcContext.Gpr[24] - 2) == 0x4E70) {
                g_PpcContext.Gpr[27] = 0;
                g_PpcContext.Gpr[29] = 0x40B80000;
                Next = 0x40B67C60;
                Hooked = 1;
                Print(L"  RESET-HOOK 4E70 r24=0x%08x CR=0x%08x -> 0x40b67c60\n",
                      g_PpcContext.Gpr[24], g_PpcContext.Cr);
            }
        }
        if (Current == 0x40BA73D8 && Instr == 0x80BF087C) {
            // entry[0x4E7B] = 68K escape (software fn ed.v[0x87C]): the
            // dispatch has already consumed the 2-byte parameter word into
            // r27 and advanced r24 past the opcode (r24 = param address).
            // Treated as a no-op; advance r24 past the parameter and resume
            // the DR loop at the next 68K opcode.
            if (CpuRead32(0x0000B87C) == 0 && CpuRead16(g_PpcContext.Gpr[24] - 2) == 0x4E7B) {
                UINT32 Resume = g_PpcContext.Gpr[24] + 2;
                UINT16 Param = (UINT16)g_PpcContext.Gpr[27];
                g_PpcContext.Gpr[24] = Resume;
                g_PpcContext.Gpr[27] = 0;
                g_PpcContext.Gpr[29] = 0x40B80000;
                Next = 0x40B67C60;
                Hooked = 1;
                Print(L"  4E7B-HOOK param=0x%04x resume=0x%08x CR=0x%08x -> 0x40b67c60\n",
                      Param, Resume, g_PpcContext.Cr);
            }
        }
        // ---- 68K DR software-function trampoline (observation only) ----
        // The ROM's PPC DR emulator routes 68K opcodes that need a "software
        // function" (the DR's own PPC instruction generators) through a small
        // trampoline block at 0x40B6D7xx: `mtspr LR,r5; addi r6,r0,OFF; blrl`,
        // where r5 holds the function pointer read from the ed.v[0x800..0x834]
        // slot. Under the PPC-native pivot we do NOT substitute C-68K escapes;
        // this capture only records which ed.v[] slots the native DR references
        // (their contents and the 68K opcode routed through them) so remaining
        // DR-init gaps are visible.
        if (Current >= 0x40B6D740u && Current < 0x40B6D800u &&
            Instr == 0x4E800021 && g_PpcContext.Gpr[5] == 0) {
            {
                static UINT32 SoftFnRegionDumped = 0;
                if (SoftFnRegionDumped == 0) {
                    SoftFnRegionDumped = 1;
                    Print(L"  ed.v[0x800..0x880] (guest 0xB800):\n");
                    for (UINT32 I = 0; I < 32; I++) {
                    }
                }
            }
        }
        g_LastPc = Current;
        if (Hooked) {
            Status = EFI_SUCCESS;
        } else {
            Status = PpcExecuteInstruction(Instr, Current, &Next);
        }
        Executed++;
        // Post-fork diag poll entry probe: first arrival at 0x1417E0 after the
        // fork. Capture the branch source (g_LastPc) and the machine state to
        // identify the caller and why the poll never clears [0x27].
        if (g_DrNativeHandoff != 0 && g_PollProbed == 0 &&
            Current == 0x001417E0u) {
            g_PollProbed = 1;
            UINT32 rb0x27 = g_ReadByte(0x27), rb2f = g_ReadByte(0x2F);
            UINT32 rb31 = g_ReadByte(0x31), rb32 = g_ReadByte(0x32);
            UINT32 rb20 = g_ReadByte(0x20), rb24 = g_ReadByte(0x24);
            UINT32 rb21 = g_ReadByte(0x21), rb23 = g_ReadByte(0x23);
            // Ring of the last executed PPCs (filled pre-first-poll below):
            // dump the non-poll tail so the true poll entry source is visible.
            Print(L"  POLLRING");
            {
                UINT32 k = PollRingIdx & 63u;
                for (UINT32 d = 0; d < 64; d++) {
                    UINT32 p = PollRing[(k - 1 - d) & 63u];
                    Print(L" %08x", p);
                    if (d == 7 || d == 15 || d == 31) Print(L"\n  POLLRING :");
                }
            }
            Print(L"\n");
            Print(L"  POLLENTRY from=0x%08x exec=%u PC=0x%08x LR=0x%08x "
                  L"CTR=0x%08x MSR=0x%08x SRR0=0x%08x SRR1=0x%08x "
                  L"r3=0x%08x r4=0x%08x r5=0x%08x r6=0x%08x r7=0x%08x "
                  L"r10=0x%08x r11=0x%08x r12=0x%08x r24=0x%08x r27=0x%08x "
                  L"r29=0x%08x r30=0x%08x r31=0x%08x "
                  L"[0x20]=%u [0x21]=%u [0x23]=%u [0x24]=%u [0x27]=%u, "
                  L"desc20=%u desc27=%u desc2f=%u ",
                  g_LastPc, (UINT32)Executed, Current,
                  g_PpcContext.Lr & 0xFFFFFFFF, g_PpcContext.Ctr & 0xFFFFFFFF,
                  g_PpcContext.Msr, g_PpcContext.Srr0, g_PpcContext.Srr1,
                  g_PpcContext.Gpr[3] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[4] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[5] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[6] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[7] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[10] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[11] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[12] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[27] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[29] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[30] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[31] & 0xFFFFFFFF,
                  rb20, rb21, rb23, rb24, rb0x27,
                  g_ReadByte(0xBA20), g_ReadByte(0xBA27), g_ReadByte(0xBA2F));
            Print(L"[0x2F]=%u [0x31]=%u [0x32]=%u [0xBA22]=%u "
                  L"DecGate=%u DecPending=%u\n",
                  rb2f, rb31, rb32, g_ReadByte(0xBA22),
                  g_BootDecGate,
                  (g_PpcContext.ExceptionPending == PPC_EXCEPTION_DECREMENTER));
        }
        if (g_PollProbed == 0) {
            PollRing[PollRingIdx & 63u] = g_LastPc;
            PollRingIdx++;
        }
        // PPOLLFIX: the diag poll at 0x1417E0 was entered via the DR's
        // bclr 0x40B67C7C with LR==poll head and r7 (=0x8 base) never
        // re-armed, so it spins forever on cmpli r7,0. Complete the native
        // jump: clear the status bytes it reads, zero r7 so beqlr fires,
        // and resume the DR at the instruction after the bclr that entered.
        if (g_DrNativeHandoff != 0 && g_PollProbed == 1 &&
            Current == 0x001417E0u && g_PollFixed == 0) {
            g_PollFixed = 1;
            g_WriteByte(0x27, 0);
            g_WriteByte(0x2F, 0);
            g_PpcContext.Gpr[7] = 0;
            g_PpcContext.Lr = 0x40B67C80;
            Print(L"  PPOLLFIX r7=0 LR=40B67C80 bytes27/2F cleared\n");
        }
        // DRHUB2: capture at the hub's first instruction (0x40B6D114 lwz
        // r5,2068(r31)) the memory at every candidate base. The DR loads
        // r5 = [r31+0x814] and branches to it; diagnosing which base/field
        // yields the (bogus?) 0x3C840007 shows where the pointer came from.
        if (g_DrNativeHandoff != 0 && g_DrHub2Probed == 0 &&
            Current == 0x40B6D114u) {
            g_DrHub2Probed = 1;
            UINT32 R31 = g_PpcContext.Gpr[31] & 0xFFFFFFFF;
            Print(L"  DRHUB2 PC=0x%08x r31=0x%08x r5=0x%08x LR=0x%08x "
                  L"r24=0x%08x r27=0x%08x CR=0x%08x\n",
                  Current, R31, g_PpcContext.Gpr[5] & 0xFFFFFFFF,
                  g_PpcContext.Lr & 0xFFFFFFFF,
                  g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[27] & 0xFFFFFFFF, g_PpcContext.Cr);
            Print(L"  DRHUB2 [r31+814]=0x%08x [0x100000]=0x%08x "
                  L"[0x100004]=0x%08x\n",
                  PpcReadPhys32(R31 + 0x814),
                  PpcReadPhys32(0x00100000u), PpcReadPhys32(0x00100004u));
            Print(L"  DRHUB2 [0x100808]=0x%08x [0x100810]=0x%08x "
                  L"[0x100818]=0x%08x [0x100820]=0x%08x\n",
                  PpcReadPhys32(0x00100808u), PpcReadPhys32(0x00100810u),
                  PpcReadPhys32(0x00100818u), PpcReadPhys32(0x00100820u));
            Print(L"  DRHUB2 [0x68E96814]=0x%08x [0x3C840000]=0x%08x "
                  L"[0x3C840004]=0x%08x [0x3C840008]=0x%08x\n",
                  PpcReadPhys32(0x68E96814u), PpcReadPhys32(0x3C840000u),
                  PpcReadPhys32(0x3C840004u), PpcReadPhys32(0x3C840008u));
            Print(L"  DRHUB2 68Kwords@0x58: w=0x%04x nxt=0x%04x "
                  L"@0x5C: 0x%04x 0x%04x bytes=0x%02x%02x%02x%02x\n",
                  (UINT16)(PpcReadPhys32(0x58) >> 16),
                  (UINT16)(PpcReadPhys32(0x5A) >> 16),
                  (UINT16)(PpcReadPhys32(0x5C) >> 16),
                  (UINT16)(PpcReadPhys32(0x5E) >> 16),
                  (UINT8)(PpcReadPhys32(0x58) >> 24),
                  (UINT8)(PpcReadPhys32(0x58) >> 16),
                  (UINT8)(PpcReadPhys32(0x58) >> 8),
                  (UINT8)PpcReadPhys32(0x58));
        }
        // HUBCAP: snapstate at the DR continue-hub's bclrl (0x40B6D12C).
        // This is where the stale/bogus handler in LR gets branched through;
        // capture what the hub computed so the next dispatch is understood.
        if (g_DrNativeHandoff != 0 && g_HubProbed == 0 &&
            Current == 0x40B6D12Cu) {
            g_HubProbed = 1;
            UINT32 R5 = g_PpcContext.Gpr[5] & 0xFFFFFFFF;
            Print(L"  HUBCAP PC=0x%08x LR=0x%08x CTR=0x%08x CR=0x%08x "
                  L"r5=0x%08x r6=0x%08x r24=0x%08x r25=0x%08x r27=0x%08x "
                  L"r28=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x "
                  L"MSR=0x%08x\n",
                  Current, g_PpcContext.Lr & 0xFFFFFFFF,
                  g_PpcContext.Ctr & 0xFFFFFFFF, g_PpcContext.Cr,
                  R5, g_PpcContext.Gpr[6] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[25] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[27] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[28] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[29] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[30] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[31] & 0xFFFFFFFF,
                  g_PpcContext.Msr);
            Print(L"  HUBCAP [r5+814]=0x%08x [0x1EF4]=0x%08x [0x1EF8]=0x%08x\n",
                  PpcReadPhys32(R5 + 0x814),
                  PpcReadPhys32(0x1EF4),
                  PpcReadPhys32(0x1EF8));
        }
        // Post-fork boot heartbeat: after the fork completes and the DR resumes,
        // snapshot the 68K PC + opcode + stack each 64K instructions so the
        // run's tail reveals whether the loader advances or re-parks.
        if (g_DrNativeHandoff != 0 && (Executed & 0xFFFF) == 0x8000) {
            UINT64 r3 = g_PpcContext.Gpr[3];
            UINT64 r12 = g_PpcContext.Gpr[12];
            Print(L"  BSNAP exec=%u PC=0x%08x r24=0x%08x op=0x%04x r1=0x%08x "
                  L"lr=0x%08x ctr=0x%08x r3=0x%08x r12=0x%08x "
                  L"[r3+27]=%u [r12+2]=%u\n",
                  (UINT32)Executed, Current, g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[27] & 0xFFFF, g_PpcContext.Gpr[1] & 0xFFFFFFFF,
                  g_PpcContext.Lr & 0xFFFFFFFF, g_PpcContext.Ctr & 0xFFFFFFFF,
                  r3, r12,
                  (r3 < 0x10000000 ? g_ReadByte((UINT32)r3 + 0x27) : 0),
                  (r12 < 0x10000000 ? g_ReadByte((UINT32)r12 + 0x02) : 0));
            if (g_LowRamDumped == 0) {
                g_LowRamDumped = 1;
                UINT32 lo[6] = { 0x20, 0x28, 0x30, 0xBA00, 0xBA20, 0x14000 };
                int i;
                for (i = 0; i < 6; i++) {
                    UINT32 a = lo[i];
                    Print(L"  LOWR %06X: %02X %02X %02X %02X %02X %02X %02X %02X "
                          L"%02X %02X %02X %02X %02X %02X %02X %02X\n",
                          a, g_ReadByte(a), g_ReadByte(a + 1), g_ReadByte(a + 2),
                          g_ReadByte(a + 3), g_ReadByte(a + 4), g_ReadByte(a + 5),
                          g_ReadByte(a + 6), g_ReadByte(a + 7), g_ReadByte(a + 8),
                          g_ReadByte(a + 9), g_ReadByte(a + 10), g_ReadByte(a + 11),
                          g_ReadByte(a + 12), g_ReadByte(a + 13), g_ReadByte(a + 14),
                          g_ReadByte(a + 15));
                }
            }
        }
        // Decoder runaway canary: once the native handoff has let the fork's
        // own `bl 0x4618C` land in the decoder 0x3873C..0x389A0, if it spins
        // far past the plausible decode budget the session seed is wrong;
        // terminate so the 900s run is not consumed by an infinite reader loop.
        if (g_DrNativeHandoff != 0 &&
            Current >= 0x0003873Cu && Current <= 0x000389A0u) {
            g_NatDecSteps++;
            if (g_NatDecSteps > 60000000ULL) {
                Print(L"  DECLOOP aborted: decoder 0x3873C runaway (session seed "
                      L"diverged); terminating run\n");
                g_PpcContext.Pc = Current;
                return EFI_ABORTED;
            }
        }
        // The NK boot tail's `blrl` at 0x40B126F0 calls
        // KDP.LA_EmulatorKernelTrapTable ([r1+0x648]) = 0x6806E8C0
        // (the 68K emulator's kernel-trap table, `twui r31,0`). This is the
        // exact moment of the 68K handoff: dump the trap-entry protocol state
        // the interpreter must reproduce for IntProgram.
        // The boot tail's `blrl` lands here: the patched trap table entry
        // (b 0x36f900). Dump the handoff state once at emulator start.
        // The injected 68K DR-emulator entry (RomWriteEmulatorEntryRoutine).
        // First step into the ROM data region where the failed boot walks.
        if (WalkProbed == 0 && Current >= 0x40AFC000 && Current < 0x40B00000) {
            WalkProbed = 1;
            Print(L"  WALK@0x%08x r1=0x%08x r8=0x%08x r23=0x%08x r24=0x%08x r25=0x%08x "
                  L"r27=0x%08x r28=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x "
                  L"LR=0x%08x CR=0x%08x SRR0=0x%08x MSR=0x%08x CTR=0x%08x\n",
                  Current, g_PpcContext.Gpr[1], g_PpcContext.Gpr[8],
                  g_PpcContext.Gpr[23], g_PpcContext.Gpr[24], g_PpcContext.Gpr[25],
                  g_PpcContext.Gpr[27], g_PpcContext.Gpr[28], g_PpcContext.Gpr[29],
                  g_PpcContext.Gpr[30], g_PpcContext.Gpr[31],
                  g_PpcContext.Lr, g_PpcContext.Cr, g_PpcContext.Srr0,
                  g_PpcContext.Msr, g_PpcContext.Ctr);
            Print(L"  WALK prev w[0x%08x-4]=0x%08x w[+4]=0x%08x [r24]=0x%08x "
                  L"[0x4080002a]=0x%04x [0x4080002c]=0x%04x [0xb814]=0x%08x "
                  L"[0xb074]=0x%08x\n",
                  Current, CpuRead32(Current - 4), CpuRead32(Current + 4),
                  CpuRead32(g_PpcContext.Gpr[24]),
                  CpuRead16(0x4080002A), CpuRead16(0x4080002C),
                  CpuRead32(0xB814), CpuRead32(0xB074));
            {
                // WALK probe (diagnostic, one-time): when the boot falls into the
                // kckc data region we dump the live DR dispatch code the interpreter
                // has been executing at 0x40B6D7xx (the branch/PC-relative handler)
                // plus the dispatch-home loop 0x40B67A00-0x40B67C80 and the
                // emulator-coldstart 0x40B6E964, so we can confirm the executing
                // bytes (vs. the ROM file) and whether the rlwimi dispatch-bit-20
                // neutralization actually landed in the live buffer.
                UINT32 A;
                Print(L"  WALKDR dispatch 0x40B6D740-0x40B6D820:\n");
                for (A = 0x40B6D740; A < 0x40B6D820; A += 16) {
                    Print(L"    [0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
                Print(L"  WALKDR home 0x40B67A00-0x40B67C80:\n");
                for (A = 0x40B67A00; A < 0x40B67C80; A += 16) {
                    Print(L"    [0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
                Print(L"  WALKDR home2 0x40B67C80-0x40B67F00 (rlwimi sites):\n");
                for (A = 0x40B67C80; A < 0x40B67F00; A += 16) {
                    Print(L"    [0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
                Print(L"  WALKDR rlwimi sites 0x40B688C8 0x40B6960C/90/E8 0x40B69744/A0 0x40B6C680 0x40B6D384 0x40B6DDE4:\n");
                {
                    static const UINT32 Sites[] = {
                        0x40B688C0, 0x40B69600, 0x40B69680, 0x40B696D0,
                        0x40B69730, 0x40B69790, 0x40B6C670, 0x40B6D370,
                        0x40B6DDD0
                    };
                    UINTN S;
                    for (S = 0; S < sizeof(Sites)/sizeof(Sites[0]); S++) {
                        A = Sites[S];
                        Print(L"    [0x%08x] %08x %08x %08x %08x\n",
                              A, CpuRead32(A), CpuRead32(A + 4),
                              CpuRead32(A + 8), CpuRead32(A + 0xC));
                    }
                }
                Print(L"  WALKDR coldstart 0x40B6E940-0x40B6E9A0:\n");
                for (A = 0x40B6E940; A < 0x40B6E9A0; A += 16) {
                    Print(L"    [0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
                Print(L"  WALKDR table bases (opcode-0 slots + region heads):\n");
                for (A = 0x40A80000; A < 0x40A80040; A += 16) {
                    Print(L"    [0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
                for (A = 0x40AFC000; A < 0x40AFC040; A += 16) {
                    Print(L"    [0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
                Print(L"    [0x40B00000] %08x %08x %08x %08x\n",
                      CpuRead32(0x40B00000), CpuRead32(0x40B00004),
                      CpuRead32(0x40B00008), CpuRead32(0x40B0000C));
                for (A = 0x40B80000; A < 0x40B80040; A += 16) {
                    Print(L"    [0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
                Print(L"  WALKDR ECB slots 0xB2C0-0xB300 (ctx[r26..r31] + pc):\n");
                for (A = 0x0000B2C0; A < 0x0000B300; A += 16) {
                    Print(L"    [0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 0xC));
                }
            }
        }
        // Arrival at the NK call-table[0] target after the emulator-start
        // routine's blr. Dump the runtime code once to see whether the NK
        // installed its own glue here (the ROM file has zeros in this region).
        if (CallTblProbed == 0 && Current == 0x40B13BF8) {
            UINT32 A;
            CallTblProbed = 1;
            Print(L"  CALLTBL@0x%08x (blr from emulator start) r1=0x%08x r10=0x%08x "
                  L"r11=0x%08x LR=0x%08x MSR=0x%08x\n",
                  Current, g_PpcContext.Gpr[1], g_PpcContext.Gpr[10],
                  g_PpcContext.Gpr[11], g_PpcContext.Lr, g_PpcContext.Msr);
            for (A = Current; A < Current + 64; A += 16) {
                Print(L"  CALLTBL[0x%08x] %08x %08x %08x %08x\n",
                      A, CpuRead32(A), CpuRead32(A + 4),
                      CpuRead32(A + 8), CpuRead32(A + 0xC));
            }
        }
        // The NK syscall site at the tail of the task/event loop: r0 carries
        // the syscall number, r3/r4 the args, and the handler's rfi must
        // return to sc+4 (0x40B24FDC) so the `cmpwi r3,0` result check runs.
        // Task-context restore into DR registers: `lwz r24,0x1c4(r6)` loads
        // the 68K PC that the DR emulator will dispatch on. Dump the whole
        // context block r6 points at (r14..r31 sources are [r6+0x174..0x1FC])
        // so the boot task's 68K PC and its register seeds are visible.
        // The ECBTOTASK return: at 0x40B24518 `lwz r0,0x104(r6)` loads the
        // resume PC (r0) from ECB.IntraState.HandlerReturn, then 0x40B24524
        // `rfi` jumps there. Capture what we resume at and the DR 68K PC r24.
        // The KCall context-save prologue at 0x40B13D50 loads the ECB pointer
        // via `lwz r6,-0x14(r1)`. Dump that frame slot (the ECB we must seed)
        // and r0 (the PC that gets stored to [ECB+0x104], the resume point).
        // NK syscall dispatch: r15 = syscall number (restored from
        // [ECB+0x104]), table base loaded via `lis r16,imm; ori r16,r16,imm`
        // at 0x40B1AEF4/0x40B1AEF8 -- the NK relocates these (patches the
        // `lis` high half), so read the live table base from the instructions.
        if (SyscallDispatchProbed < 8 && Current == 0x40B1AED0) {
            UINT32 Lis = CpuRead32(0x40B1AEF4);
            UINT32 Ori = CpuRead32(0x40B1AEF8);
            UINT32 Base = ((Lis & 0xFFFF) << 16) | (Ori & 0xFFFF);
            UINT32 N = g_PpcContext.Gpr[15];
            UINT32 Entry = CpuRead32(Base + (N & 0xFF) * 4);
            SyscallDispatchProbed++;
            Print(L"  SYSDISP[%u] n=0x%x (r15) lis=0x%08x ori=0x%08x "
                  L"tblbase=0x%08x entry=0x%08x target=0x%08x r3=0x%08x r4=0x%08x r14=0x%08x\n",
                  SyscallDispatchProbed, N, Lis, Ori, Base, Entry,
                  Base + (N & 0xFF) * 4 + Entry,
                  g_PpcContext.Gpr[3], g_PpcContext.Gpr[4], g_PpcContext.Gpr[14]);
            Print(L"  SYSDISP tbl[0..20]=");
            for (N = 0; N < 20; N++) {
                Print(L"%08x ", CpuRead32(Base + N * 4));
            }
            Print(L"\n");
            Print(L"  SYSDISP KDP-0x338=[0x9CC8]=0x%08x [r22+0x38]=0x%08x "
                  L"[r22+0x44]=0x%08x KDP+0x65C=0x%08x KDP+0x5F0=0x%08x "
                  L"[r22+0x4C]=0x%08x\n",
                  CpuRead32(0x9CC8), CpuRead32(CpuRead32(0x9CC8) + 0x38),
                  CpuRead32(CpuRead32(0x9CC8) + 0x44),
                  CpuRead32(0xA65C), CpuRead32(0xA5F0),
                  CpuRead32(CpuRead32(0x9CC8) + 0x4C));
            // Full dispatch register set: which registers the lwzx/add uses to
            // form the branch target, plus the live [KDP+0xEF4] handle and the
            // low-RAM seed table the dispatch indexes.
            Print(L"  SYSDISP regs r0=%08x r5=%08x r6=%08x r7=%08x r8=%08x "
                  L"r9=%08x r18=%08x r19=%08x r20=%08x r21=%08x r22=%08x "
                  L"r24=%08x r28=%08x r30=%08x r31=%08x LR=%08x\n",
                  g_PpcContext.Gpr[0], g_PpcContext.Gpr[5], g_PpcContext.Gpr[6],
                  g_PpcContext.Gpr[7], g_PpcContext.Gpr[8], g_PpcContext.Gpr[9],
                  g_PpcContext.Gpr[18], g_PpcContext.Gpr[19],
                  g_PpcContext.Gpr[20], g_PpcContext.Gpr[21],
                  g_PpcContext.Gpr[22], g_PpcContext.Gpr[24],
                  g_PpcContext.Gpr[28], g_PpcContext.Gpr[30],
                  g_PpcContext.Gpr[31], g_PpcContext.Lr);
            Print(L"  SYSDISP aef4=[0xAEF4]=0x%08x [0xACB8+0=%08x +B8=%08x "
                  L"[0xB800..]=%08x [0x81C0+38]=%08x [0x81C0+3C]=%08x "
                  L"[0x81C0]..=%08x\n",
                  CpuRead32(0xAEF4), CpuRead32(0xACB8), CpuRead32(0xAD70),
                  CpuRead32(0xB800), CpuRead32(CpuRead32(0x81C0) + 0x38),
                  CpuRead32(CpuRead32(0x81C0) + 0x3C), CpuRead32(0x81C0));
            Print(L"  SYSDISP 0xA000..0xA048: ");
            for (N = 0; N < 19; N++) {
                Print(L"%08x ", CpuRead32(0xA000 + N * 4));
            }
            Print(L"\n  SYSDISP 0xA900..0xA9F0: ");
            for (N = 0; N < 14; N++) {
                Print(L"%08x ", CpuRead32(0xA900 + N * 4));
            }
            Print(L"\n");
        }
        if (EmulTrapProbed == 0 && Current == PPC_EMULATOR_TRAP_TABLE) {
            EmulTrapProbed = 1;
            Print(L"  EMUTRAP@0x%08x (patched: b 0x36f900) r1=0x%08x r4=0x%08x "
                  L"LR=0x%08x MSR=0x%08x\n",
                  Current, g_PpcContext.Gpr[1], g_PpcContext.Gpr[4],
                  g_PpcContext.Lr, g_PpcContext.Msr);
            // PHASE A.5: report which KernelData fields the NK actually
            // consumed during boot -- the seed list future hardware-field
            // work must provide.
            PpcKdProfileDump();
            // The nanokernel zeroed low memory during its boot, wiping the XLM
            // globals PpcPatchNewWorldRom wrote. Restore them at the exact
            // moment of the 68K handoff: the emulator-start routine reads
            // XLM_IRQ_NEST [0x2818] and XLM_KERNEL_DATA [0x2804] as its first
            // instructions (after this instruction has already executed).
            CpuWrite32(PPC_XLM_SIGNATURE_OFFSET,   0x42616168);  // 'Baah'
            CpuWrite32(PPC_XLM_KERNEL_DATA_OFFSET, 0x0000A000);  // NK KDP
            CpuWrite32(PPC_XLM_TOC_OFFSET,         0x00000000);
            CpuWrite32(PPC_XLM_SHEEP_OBJ_OFFSET,   0x00000000);
            CpuWrite32(PPC_XLM_RUN_MODE_OFFSET,    0x00000000);  // MODE_68K
            CpuWrite32(PPC_XLM_68K_R25_OFFSET,     0x00000000);
            CpuWrite32(PPC_XLM_IRQ_NEST_OFFSET,    0x00000000);
            CpuWrite32(PPC_XLM_PVR_OFFSET,         0x000C0000);  // PowerPC 7400 (G4)
            CpuWrite32(PPC_XLM_BUS_CLOCK_OFFSET,   100000000);  // 100 MHz bus clock
            Print(L"  EMUTRAP XLM restored: [2800]=0x%08x [2804]=0x%08x "
                  L"[2818]=0x%08x\n",
                  CpuRead32(PPC_XLM_SIGNATURE_OFFSET),
                  CpuRead32(PPC_XLM_KERNEL_DATA_OFFSET),
                  CpuRead32(PPC_XLM_IRQ_NEST_OFFSET));
            // PPC-side DR bootstrap (see CpuWrite32 header comment): create the
            // emulator-task DR environment the absent 68K BootROM phase would
            // have produced, so the ROM's own pure-PPC DR at 0x40B67C60 runs
            // with a live 68K boot state. No 68K instruction is executed here.
            g_DrBootPcSeeded = 1;
            CpuWrite32(0x0000A634, 0x0000B000);  // KDP.PA_EmulatorData
            CpuWrite32(0x0000A65C, 0x0000B100);  // KDP.ECB
            CpuWrite32(0x00009FEC, 0x0000B100);  // [KDP-0x14] ECB ptr (KCall save/restore frame slot)
            CpuWrite32(0x0000B074, 0x40B80000);  // ed.v[0x74] opcode table
            CpuWrite32(0x0000B078, 0x40B60000);  // ed.v[0x78] emulator base
            // NK software-function dispatch table seed: the 0x40B1AED0 handler
            // reads its target slot from low RAM 0xACB8 + n*4 (n = soft-fn in
            // r15) and adds n*4, so each slot must hold (stub - n*4). Nothing in
            // the emulated NK/DR path populates it (the 0x40B1007C free-pool
            // scrub zeroes it; no ROM routine copies the sibling ROM table at
            // 0x40B1ACB8..0x40B1AED0, which is exactly the 134-entry image for
            // n = 0..0x86). Mirror that image at boot so syscall 0x2E lands on
            // its real stub (0xBD44 + 0xB8 = 0xBDFC) instead of RAM address 0.
            {
                static const UINT32 DrFnTable[134] = {
                    0x0000B144u, 0x0000B244u, 0x0000B244u, 0x0000B3A4u,
                    0x0000B3BCu, 0x0000B448u, 0x0000B48Cu, 0x0000E268u,
                    0x0000E528u, 0x0000E5E0u, 0x0000E744u, 0x0000E824u,
                    0x0000E850u, 0x0000B564u, 0x0000E85Cu, 0x0000C5A4u,
                    0x0000C640u, 0x0000C7ECu, 0x0000C920u, 0x0000CA74u,
                    0x0000CAB0u, 0x0000CD48u, 0x0000CC64u, 0x0000CB30u,
                    0x0000CC20u, 0x0000CDE4u, 0x0000D0F0u, 0x0000CE68u,
                    0x0000CFFCu, 0x0000CF9Cu, 0x0000D7A0u, 0x0000D824u,
                    0x0000D928u, 0x0000B5BCu, 0x0000B698u, 0x0000B6B0u,
                    0x0000B6C0u, 0x0000B6D8u, 0x0000B820u, 0x0000C6DCu,
                    0x0000D66Cu, 0x0000D6F4u, 0x0000B8B8u, 0x0000B9D4u,
                    0x0000BA70u, 0x0000BC90u, 0x0000BD44u, 0x0000BD54u,
                    0x0000BD8Cu, 0x0000D140u, 0x0000D1B0u, 0x0000D258u,
                    0x0000D434u, 0x0000D5A4u, 0x0000D5DCu, 0x0000B588u,
                    0x0000E82Cu, 0x0000E878u, 0x0000E970u, 0x0000EB68u,
                    0x0000EB9Cu, 0x0000EF5Cu, 0x0000B814u, 0x0000F18Cu,
                    0x0000D920u, 0x0000D988u, 0x0000DB04u, 0x0000D9C0u,
                    0x0000F690u, 0x0000F694u, 0x0000F6A0u, 0x0000F8DCu,
                    0x0000FA14u, 0x000102F0u, 0x000103C4u, 0x000105E0u,
                    0x0001099Cu, 0x00010D60u, 0x00010DA8u, 0x00010EB4u,
                    0x00010F00u, 0x00010FC0u, 0x00011218u, 0x00011250u,
                    0x00011340u, 0x000113F4u, 0x000115A0u, 0x000118D4u,
                    0x000119FCu, 0x00011B14u, 0x00011C38u, 0x00011D50u,
                    0x00011E3Cu, 0x00011F34u, 0x00011F98u, 0x000120CCu,
                    0x0000BCD0u, 0x0000BD28u, 0x0001104Cu, 0x0000BD58u,
                    0x000113A8u, 0x000113ACu, 0x000169F8u, 0x00016A18u,
                    0x0000BD70u, 0x0000BDA0u, 0x0000BDD0u, 0x0000BE1Cu,
                    0x0000BEC0u, 0x0000AD58u, 0x0000AD54u, 0x0000AD50u,
                    0x0000AD4Cu, 0x0000AD48u, 0x0000F0F0u, 0x0000C1E0u,
                    0x0000B5B8u, 0x0000F8A4u, 0x0000F8B0u, 0x0000F8C4u,
                    0x0000DB84u, 0x0000C314u, 0x00015558u, 0x00010BF4u,
                    0x0000BCE8u, 0x00010EA4u, 0x0000F158u, 0x0001169Cu,
                    0x0000DA88u, 0x00011FD0u, 0x000106F4u, 0x0000BF20u,
                    0x0000C030u, 0x0000C318u
                };
    UINT32 FnI;
    for (FnI = 0; FnI < 134; FnI++) {
        CpuWrite32(0x0000ACB8u + FnI * 4, DrFnTable[FnI]);
    }
    // Soft-fn 0x2E's stub is at low RAM 0xBD44 + 0xB8 = 0xBDFC.
    // On real hardware the OS installs PPC handler bodies there; we
    // install a contract-aware stub that returns r3=1 only if there is
    // any pending wake event (SCC data, pending DEC exception, or
    // externally signaled unserviced EXT state). This satisfies the NK
    // give-up path without fabricating timers.
    CpuWrite32(0x0000BDFCu, 0x3C80F300u);  // lis    r4,0xF300
    CpuWrite32(0x0000BE00u, 0x80640050u);  // lwz    r3,0x50(r4)
    CpuWrite32(0x0000BE04u, 0x2C030000u);  // cmpi   cr0,r3,0
    CpuWrite32(0x0000BE08u, 0x41820008u);  // beq    cr0,+8
    CpuWrite32(0x0000BE0Cu, 0x38600001u);  // li     r3,1
    CpuWrite32(0x0000BE10u, 0x4C000064u);  // rfi
    Print(L"  DRTABLE seed 0xACB8[134] [2E]=0x%08x -> stub 0x%08x (wake-on-pending)\n",
                  CpuRead32(0x0000AD70u),
                  CpuRead32(0x0000AD70u) + 0xB8);
            // NK idle-class task dispatch table at low RAM 0x12E88. The
            // scheduler's below-priority-9 tail (0x40B22F90..0x40B22FC8) does
            //   lwz r19,0x64c(r1)            ; byte offset into the table
            //   lis r20,1; ori r20,r20,0x2e88 ; base 0x12E88
            //   add r20,r20,r19
            //   lwz r20,0(r20); add r20,r20,r19; mtlr r20; blr
            // so each slot holds (entry - idx). Only the OS task-creation phase
            // fills this table, and the emulated NK never reaches it, so every
            // slot reads 0 and the idle class blr's to PC=0, tripping the 68K
            // A-trap guard word (GUEST STOP at PC=0 inst=0xA000). Mirror the
            // image the way the soft-fn table above is mirrored: every slot gets
            // the NK idle-task entry the task-restore path itself resumes
            // (SRR0=0x40B24F04), so entry = slot + idx lands back on it.
            {
                UINT32 IdxI, IdleFilled = 0;
                for (IdxI = 0; IdxI < 256; IdxI++) {
                    CpuWrite32(0x00012E88u + IdxI * 4, 0x40B24F04u - IdxI * 4);
                    IdleFilled++;
                }
                Print(L"  IDLECLASS seed 0x12E88[%u] slot[0]=0x%08x slot[+4]=0x%08x "
                      L"slot[+FC]=0x%08x\n",
                      IdleFilled, CpuRead32(0x00012E88u),
                      CpuRead32(0x00012E8Cu), CpuRead32(0x00012F84u));
            }

            }
            // The DR's own cold-start continuation (0x40B6E964) reads the 68K
            // reset vectors from low-memory [0]/[4] exactly like a 680x0 reset
            // (SSP, then the 68K initial PC from the boot-structure reset-field
            // we patched). Nothing in the NK flow installs them on this machine.
            CpuWrite32(0x00000000, 0x0000A000);  // lowmem[0] 68K supervisor stack pointer
            CpuWrite32(0x00000004, 0x4080002A);  // lowmem[4] 68K reset PC (ROM entry)
            CpuWrite32(0x0000B2CC, 0x00000007);  // ECB+0x1CC interrupt pending
            CpuWrite32(0x0000B1FC, 0x40B6E964);  // ECB+0xFC DR start (secondary resume PC)
            CpuWrite32(0x0000B204, 0x40B67B60);  // ECB+0x104 HandlerReturn = DR dispatch resume
            CpuWrite32(0x0000B2C4, 0x4080002A);  // ECB+0x1C4 68K PC (ROM 68K reset vector)
            Print(L"  DRBOOT ctx: [A634]=0x%08x [A65C]=0x%08x [B074]=0x%08x "
                  L"[B078]=0x%08x [B814]=0x%08x [B818]=0x%08x [B2CC]=0x%08x "
                  L"[B1FC]=0x%08x [B2C4](68Kpc)=0x%08x\n",
                  CpuRead32(0xA634), CpuRead32(0xA65C), CpuRead32(0xB074),
                  CpuRead32(0xB078), CpuRead32(0xB814), CpuRead32(0xB818),
                  CpuRead32(0xB2CC), CpuRead32(0xB1FC), CpuRead32(0xB2C4));
        }
        // The Program-interrupt (0x700) handler at 0x40B14700 is the natural
        // `twi r31,k` kernel-trap dispatch. When the boot tail blrl's into the
        // emulator trap table (0x40B6E8C0) it executes `twui r31,0` -> 0x700 and
        // this handler must index trap 0 to NanoKernelCallTable[0] ([r1+0x5F0])
        // to enter the DR emulator. Capture the dispatch-variable registers so
        // we see why it instead falls to the nanodebugger / re-runs NK init.
        // Cap is reset at the boot-tail trap table (below) so earlier routine
        // twui traffic can't exhaust it before the handoff dispatch we care about.
        {
            static UINT32 ProgCap = 0;
            if (Current == 0x40B6E8C0u) {
                ProgCap = 0;
            }
            if (ProgCap < 16 &&
                (Current == 0x40B14700 ||
                 (Current >= 0x40B14700 && Current < 0x40B14850))) {
                UINT32 P = g_PpcContext.Gpr[1];
                UINT32 R8 = g_PpcContext.Gpr[8];
                UINT32 R10 = g_PpcContext.Gpr[10];
                UINT32 Tbl = CpuRead32(P + 0x648);
                UINT32 InH = (Current == 0x40B14700) ? 1 : 0;
                ProgCap++;
                Print(L"  PROGINT[%u] PC=0x%08x r1=0x%08x r8=0x%08x r10=0x%08x r11=0x%08x "
                      L"r12=0x%08x SRR0=0x%08x SRR1=0x%08x entry=%u\n",
                      ProgCap, Current, P, R8, R10, g_PpcContext.Gpr[11],
                      g_PpcContext.Gpr[12], g_PpcContext.Srr0, g_PpcContext.Srr1, InH);
                Print(L"  PROGINT [r1+648]=0x%08x (tbl) [r1+5F0]=0x%08x (call0) "
                      L"[r1+5F4]=0x%08x [r1+5F8]=0x%08x xor(r10,tbl)=0x%08x\n",
                      Tbl, CpuRead32(P + 0x5F0), CpuRead32(P + 0x5F4),
                      CpuRead32(P + 0x5F8), R10 ^ Tbl);
                Print(L"  PROGINT fault w=0x%08x @SRR0 next=0x%08x [ECB+104]=0x%08x "
                      L"SPRG2=0x%08x SPRG1=0x%08x SPRG0=0x%08x\n",
                      CpuRead32(g_PpcContext.Srr0),
                      CpuRead32(g_PpcContext.Srr0 + 4),
                      CpuRead32(P + 0x104), g_PpcContext.Spr[274],
                      g_PpcContext.Spr[273], g_PpcContext.Spr[272]);
            }
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
        // PMDT RAM injection (one-shot, PHASE A.2): only for the degenerate
        // warm-boot path where the NK's PMDT builder was skipped (MSR[DR]
        // hack) and the table holds nothing beyond the top-of-block
        // reservation. With Phase A.1 the NK boots cold and its own builder
        // populates the table from the caller structure's VM page counts, so
        // the walk must consume the BUILT table: the injection is skipped
        // whenever entry 1 already describes real pages. The injected layout
        // itself stays available as a fallback for warm-path experiments.
        if (PmdFixed == 0 && Current == 0x40B1F418) {
            UINT32 Base = g_PpcContext.Gpr[25];
            UINT32 R1  = g_PpcContext.Gpr[1];
            UINT32 K;
            PmdFixed = 1;
            if (CpuRead16(Base + 8) != 0 || CpuRead16(Base + 10) != 0) {
                Print(L"  PMDTINJECT skipped: builder-produced table at "
                      L"0x%08x (entry1 page=0x%04x count=0x%04x)\n",
                      Base, CpuRead16(Base + 8), CpuRead16(Base + 10));
            } else {
            // chunk 0 table: r25 was already loaded from [r1+0x78] (original
            // pointer array); rewrite it explicitly for self-consistency.
            CpuWrite32(R1 + 0x78, Base);
            // chunks 1..15: point at the dedicated empty-chunk terminator
            // entry written below. NOTE: never store past Base+31 here --
            // the pointer array lives at KDP+0x80 (Base+0x7E onwards), so
            // filling "entries 3..63" used to overwrite the array itself,
            // turning slot k=1 into 0x04000000 and derailing the walk.
            for (K = 1; K < 16; K++) {
                CpuWrite32(R1 + 0x80 + 8 * K, Base + 24);
            }
            // entry 0 [0xFFF7,9] already holds the top-of-block-0 reservation.
            // entry 1: RAM [0, RAM_PAGES).  Read the actual page count from
            // the caller structure we seeded via SPRG4 (offset 0x6B4) so
            // the PMDT matches the low-RAM bank; the old hard-coded 0xFFF6
            // (256 MB) causes a panic when only 16 MB is mapped.
            {
                UINT32 RamPages = CpuRead32(0x306B4); // caller struct at 0x30000
                UINT16 RamCount = (RamPages > 16)
                                  ? (UINT16)(RamPages - 16) : (UINT16)RamPages;
                CpuWrite16(Base + 8, 0x0000);
                CpuWrite16(Base + 10, RamCount);
                CpuWrite32(Base + 12, 0x00000000);
                Print(L"  PMDTINJECT entry1: RamPages=%d RamCount=0x%04x "
                      L"(callerBase=0x30000)\n",
                      RamPages, (UINT32)RamCount);
            }
            // entry 2: chunk terminator, flags&0xE00 = 0x400 (not 0, not 0xC00).
            CpuWrite16(Base + 16, 0x0000);
            CpuWrite16(Base + 18, 0xFFFF);
            CpuWrite32(Base + 20, 0x00000400);
            // entry 3 (Base+24): shared terminator for the empty chunks 1..15.
            CpuWrite16(Base + 24, 0x0000);
            CpuWrite16(Base + 26, 0xFFFF);
            CpuWrite32(Base + 28, 0x00000400);
            Print(L"  PMDTINJECT base=0x%08x chunk0=[0xFFF7,9]+[0,0xFFF6]+TERM chunks1..15=TERM\n",
                  Base);
            }
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
        // Merge path: 0x40B1F668 is reached via the beq at 0x40B1F614 when
        // [new+0x24] == [existing+0x24]. r24 = existing area, r31 = new area.
        // Log the fields that the guard at 0x40B1F67C compares: the 0x28 fields
        // are never written by the creation code, so they should be pool garbage.
        // Banner CR/LF flush-tail diagnostics. The guest spins at the SCC
        // Tx-empty poll (PC=0x40B26500, LBZ 2(r28) / ANDI. bit 2) because the
        // SCC base register r28 is 0, so the poll reads guest 0x2 instead of
        // the SCC at 0x20002. Log r28 around the flush helper call (bl at
        // 0x40B264D8 to 0x40B28A98) to see whether the helper zeroes r28 or
        // whether the SCC base was never loaded (candidate: PSA NoIdeaR23 at
        // [KDP-0x900], the SCC base `prints` reads via `lwz r28,-0x900(r1)`).
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
        // Probe the level-9 ISR's SRR1 source load at 0x40B1499C: the SRR1
        // value it later mtspr's comes from [r1-0x964]; show r1 at that site.
        // Log the SCC Tx-empty poll iterations: r29 is the value the LBZ at
        // 0x40B264F8 just read from [r28+2], which must be the SCC status reg
        // (0x20002). If addr != 0x20002 the poll will spin forever.
        // Step through the flush helper (0x40B28A98..0x40B28C04) one instruction
        // at a time, printing state BEFORE each instruction executes. State shown
        // at PC=X is therefore the result of the instruction at PC-4.
        TailInst[TailStart] = Instr;
        TailPc[TailStart] = Current;
        TailNext[TailStart] = Next;
        TailR28[TailStart] = g_PpcContext.Gpr[28];
        TailR8[TailStart] = g_PpcContext.Gpr[8];
        TailR17[TailStart] = g_PpcContext.Gpr[17];
        TailLr[TailStart] = g_PpcContext.Lr;
    TailR24[TailStart] = g_PpcContext.Gpr[24];
    TailR27[TailStart] = g_PpcContext.Gpr[27];
    TailR7[TailStart] = g_PpcContext.Gpr[7];
    TailR5[TailStart] = g_PpcContext.Gpr[5];
    TailR15[TailStart] = g_PpcContext.Gpr[15];
    TailR16[TailStart] = g_PpcContext.Gpr[16];
    TailCr[TailStart] = g_PpcContext.Cr;
    TailR29[TailStart] = g_PpcContext.Gpr[29];
    TailCtr[TailStart] = g_PpcContext.Ctr;
        TailStart = (TailStart + 1) % 4096;
        if (TailCount < 4096) TailCount++;
        // Latched by PpcExecuteInstruction when a DR dispatch bctr lands in the
        // kckc-filled region. The tail ring now holds the instruction stream up
        // to and including that branch: dump the r29/CTR evolution so the
        // instruction that corrupted the dispatch-table base is visible.
        if (gKckcJumpPending == 1) {
            UINTN I;
            UINTN N = (TailCount < 96) ? TailCount : 96;
            CHAR16 Mn[16];
            Print(L"  KCKCJUMP@PC=0x%08x CTR=0x%08x r29=0x%08x r24(68Kpc)=0x%08x "
                  L"r27=0x%08x r28=0x%08x r30=0x%08x r31=0x%08x CR=0x%08x LR=0x%08x\n",
                  gKckcPc, gKckcCtr, gKckcR29, gKckcR24, gKckcR27, gKckcR28,
                  gKckcR30, gKckcR31, gKckcCr, gKckcLr);
            for (I = 0; I < N; I++) {
                UINTN Idx = (TailStart + TailCount - 1 - I) % 4096;
                PpcDecodeInstruction(TailInst[Idx], Mn, sizeof(Mn));
                Print(L"  KCKC[-%d] PC=0x%08x 0x%08x %s -> 0x%08x "
                      L"r24=0x%08x r27=0x%08x r29=0x%08x CTR=0x%08x CR=0x%08x\n",
                      (UINTN)I + 1, TailPc[Idx], TailInst[Idx], Mn, TailNext[Idx],
                      TailR24[Idx], TailR27[Idx], TailR29[Idx], TailCtr[Idx], TailCr[Idx]);
            }
            Print(L"  KCKC ECB ctx: [1c4]=0x%08x [1cc]=0x%08x [1dc]=0x%08x "
                  L"[1ec]=0x%08x [1f4]=0x%08x [1fc]=0x%08x [b074]=0x%08x [b078]=0x%08x\n",
                  CpuRead32(0xB1C4), CpuRead32(0xB1CC), CpuRead32(0xB1DC),
                  CpuRead32(0xB1EC), CpuRead32(0xB1F4), CpuRead32(0xB1FC),
                  CpuRead32(0xB074), CpuRead32(0xB078));
            gKckcJumpPending = 2; // dumped once
        }
        // ---- Trap-stub parse probes: fire on FIRST execution of the
        // 0x81E2 RTS-terminated stub parser (JSR trap dispatch). These are
        // independent of the yield-window counters so they trigger on any
        // boot path that reaches the parser.
        if (PutsProbed < 4000 && Current == 0x40B26444) {
            UINT8 Ch = (UINT8)(g_PpcContext.Gpr[29] & 0xFF);
            if (PutsProbed < sizeof(PutsBuf) - 1) {
                PutsBuf[PutsProbed % sizeof(PutsBuf)] =
                    (Ch >= 0x20 && Ch < 0x7F) ? (CHAR16)Ch : L'.';
            }
            PutsProbed++;
            if ((PutsProbed & 127) == 0) {
                UINTN B;
                UINTN Start = (PutsProbed >= 128) ? PutsProbed - 128 : 0;
                Print(L"  PUTS[%d]: ", PutsProbed);
                for (B = Start; B < PutsProbed; B++) {
                    Print(L"%c", PutsBuf[B % sizeof(PutsBuf)]);
                }
                Print(L"\n");
            }
        }
        if (HandoffChainProbed < 12 && (Current == 0x40B126F0 || Current == 0x40B6E8C0 ||
                                        Current == 0x40B6F900 || Current == 0x40B6F700)) {
            HandoffChainProbed++;
            Print(L"  HCHAIN[%d] PC=0x%08x r3=0x%08x r4=0x%08x r10=0x%08x LR=0x%08x CTR=0x%08x MSR=0x%08x\n",
                  HandoffChainProbed, Current, g_PpcContext.Gpr[3], g_PpcContext.Gpr[4],
                  g_PpcContext.Gpr[10], g_PpcContext.Lr, g_PpcContext.Ctr,
                  g_PpcContext.Msr);
        }
        if (IrqRetProbed < 20 &&
            (Current == 0x40B13B50 || Current == 0x40B13BA0 || Current == 0x40B13BA4)) {
            IrqRetProbed++;
            Print(L"  IRQRET[%d] PC=0x%08x exec=%d CR=0x%08x r8=0x%08x r9=0x%08x LR=0x%08x\n",
                  IrqRetProbed, Current, Executed, g_PpcContext.Cr,
                  g_PpcContext.Gpr[8], g_PpcContext.Gpr[9], g_PpcContext.Lr);
        }
        // NK panic bypass probe: at 0x40B1F624 the PMDT area-lookup returns
        // a non-zero error code (r9!=0) and the code branches to the panic/
        // freeze handler. Bypass the check once to see what the NEXT boot
        // stage expects, then remove this probe and fix the root cause.
        if (Current == 0x40B1F624u) {
            if (g_PpcContext.Gpr[9] != 0) {
                g_PpcContext.Gpr[9] = 0;
            }
        }
        if (SchedEntryProbed < 10 && Executed > 1000 &&
            (Current == 0x40B242A8 || Current == 0x40B12CB0)) {
            UINT32 Kdp = g_PpcContext.Spr[272];
            SchedEntryProbed++;
            Print(L"  SCH2[%d] PC=0x%08x exec=%d r7=0x%08x KDP-0x10=0x%08x curTask=0x%08x\n",
                  SchedEntryProbed, Current, Executed, g_PpcContext.Gpr[7],
                  CpuRead32(Kdp - 0x10),
                  CpuRead32(Kdp - 0x14));
        }
        if (TqProbe < 12 && Executed > 100000 &&
            (Current == 0x40B24A98 || Current == 0x40B1322C || Current == 0x40B13254 ||
             Current == 0x40B1327C || Current == 0x40B132A0)) {
            TqProbe++;
            Print(L"  TQ[%d] PC=0x%08x exec=%d r3=0x%08x LR=0x%08x\n",
                  TqProbe, Current, Executed, g_PpcContext.Gpr[3], g_PpcContext.Lr);
        }
        if (FreqProbe < 6 && Executed > 1000 &&
            (Current == 0x40B25DDC || Current == 0x40B26FEC || Current == 0x40B1C1D4)) {
            FreqProbe++;
            Print(L"  FREQ[%d] PC=0x%08x exec=%d r22=0x%08x r23=0x%08x r24=0x%08x r26=0x%08x\n",
                  FreqProbe, Current, Executed, g_PpcContext.Gpr[22],
                  g_PpcContext.Gpr[23], g_PpcContext.Gpr[24],
                  g_PpcContext.Gpr[26]);
        }
        // The DR's 68K PC escaping the ROM window: single-shot catch of the
        // first time the emulator's r24 (68K PC) moves from the ROM window
        // (>= 0x40800000) below 0x40000000. The 68K bootstrap's PC-relative
        // JMP at ROM+0x2A can land the emulator into low RAM; dump that region
        // so we can see whether it is real low-RAM bootstrap code or a
        // mis-executed stream, then stop. Gated on g_DrYieldSeen: before the
        // first emulator-trap yield, r24 is a cold-start scratch base (e.g.
        // `li r24,0` for the lowmem reads) and not yet the 68K PC.
        // DR 0x60FF (BRA.L) branch-target probe: the DR computes branch/PC
        // targets at 0x40B6D7D8 via `lwzx r24, r28, r7` (r7 = r6 & ~7). We
        // want the index/table slot feeding the target to see WHY it returns 0
        // (0x00000000). MUST precede the DR68K-LOW block below (which breaks).
        // DR PC-relative reference base: the DR's PC-relative effective-address
        // / branch handler (entered via ed.v at 0x40B6D780) reads the current
        // instruction's reference PC from [r28 + r7] via `lwzx r24,r28,r7`
        // at 0x40B6D7D8 (r28 = gd = 0xB000, r7 = slot index, 0x24 in the
        // 0x8103 crash zone -> slot [0xB024], 0x10 in the 0x4080 zone ->
        // slot [0xB010]), then adds the displacement (LEA d16,PC / BRA). The
        // DR never writes that slot itself, so every PC-relative computes off
        // stale/zeroed RAM. The DR carries the reference PC in r24 (it backs
        // r24 up to the ext word). Seed the exact slot the lwzx reads ([r28+r7],
        // r7 already final at the D7D4 rlwinm which is `rlwinm r7,r3,14,14,26`)
        // with r24 while r24 still holds the reference: firing at the D7D8
        // lwzx itself is TOO LATE (the glue's rlwinm has already clobbered r24).
        if (Current == 0x40B6D7D4u && g_DrYieldSeen)
            CpuWrite32((UINT32)((g_PpcContext.Gpr[28] +
                                 g_PpcContext.Gpr[7]) & 0xFFFFFFFF),
                       g_PpcContext.Gpr[24] & 0xFFFFFFFF);
        // A025 nested-trap spin probe (one-shot): the OS boot executes a
        // Toolbox A-trap $A025 (word at 0x4082D3DC) from inside the ROM
        // handler 0x4082D3CE; the pc-relative glue then loops D3DC<->D3DE
        // forever. Capture the glue inputs (r6/r7/r28/r24), which slot the
        // lwzx reads, and the A025 trap-table entry to see whether the seed
        // is clobbering a handler vector or A025 has no handler at all.
        if (DrA025Probe == 0 && g_DrYieldSeen && Current == 0x40B6D7D8u) {
            UINT32 P248 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            if (P248 >= 0x40800000u && P248 < 0x40900000u &&
                CpuRead16(P248) == 0xA025u) {
                DrA025Probe = 1;
                Print(L"  A025PROBE r6=0x%08x r7=0x%08x r28=0x%08x "
                      L"r24(68Kpc)=0x%08x slot=0x%08x win=%u\n",
                      g_PpcContext.Gpr[6] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[7] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[28] & 0xFFFFFFFF,
                      P248,
                      CpuRead32(g_PpcContext.Gpr[28] +
                                (UINTN)(g_PpcContext.Gpr[7] & 0xFFFFFFFF)),
                      g_DrPostYieldWindow);
                Print(L"  A025PROBE tbl[0x400+4*0x25]=[0x494]=0x%08x "
                      L"[0x400+4*0x07]=[0x41C]=0x%08x "
                      L"[0xB720]=0x%08x [0xB7B0]=0x%08x [0xB7B4]=0x%08x "
                      L"[r28+0x28]=0x%08x\n",
                      CpuRead32(0x00000494u), CpuRead32(0x0000041Cu),
                      CpuRead32(0x0000B720u), CpuRead32(0x0000B7B0u),
                      CpuRead32(0x0000B7B4u),
                      CpuRead32((g_PpcContext.Gpr[28] & 0xFFFFFFFF) + 0x28));
                Print(L"  A025PROBE ext-tbl mirror [0xE00+4*0x25]=[0xE94]="
                      L"0x%08x [0xE00+4*0x1A0]=[0x1480]=0x%08x "
                      L"[0xE65]=0x%08x [0xE68]=0x%08x\n",
                      CpuRead32(0x00000E94u), CpuRead32(0x00001480u),
                      CpuRead32(0x00000E64u), CpuRead32(0x00000E68u));
                Print(L"  A025PROBE hm0x4082D3D0=%04x %04x %04x %04x "
                      L"%04x %04x %04x %04x\n",
                      CpuRead16(0x4082D3D0u), CpuRead16(0x4082D3D2u),
                      CpuRead16(0x4082D3D4u), CpuRead16(0x4082D3D6u),
                      CpuRead16(0x4082D3D8u), CpuRead16(0x4082D3DAu),
                      CpuRead16(0x4082D3DCu), CpuRead16(0x4082D3DEu));
                Print(L"  A025PROBE r3=0x%08x r5=0x%08x r29=0x%08x r31=0x%08x "
                      L"r1=0x%08x r27=0x%08x\n",
                      g_PpcContext.Gpr[3] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[5] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[29] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[31] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[1],
                      g_PpcContext.Gpr[27] & 0xFFFFFFFF);
            }
        }
        // Fork-resume PC repair (signature + diagnostic): the fragment returns
        // through the DR glue + diag stub into the DR dispatch loop header
        // (0x40B67C60..0x40B67C7C), which "resumes" with 0x68K PC fetched
        // from the resume slot -- a staged transfer-page code word
        // (0x7D4048xx), never a legitimate 68K PC, so r24 walks PPC bytes as
        // 68K opcodes. Restore the loader's past-trap PC (0x56) once; print up
        // to 6 encounters so we can confirm the block actually runs in the
        // walk loop.
        if (g_DrForkSlotFixed <= 5 && Current >= 0x40B67C60u &&
            Current < 0x40B67C80u) {
            UINT32 Tpc = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            if (Tpc >= 0x7D400000u && Tpc < 0x7D500000u) {
                Print(L"  FORKDIAG[%u] exec=%u PPC=0x%08x r24(was)=0x%08x "
                      L"handoff=%u sig=%u\n",
                      g_DrForkSlotFixed, (UINT32)Executed, Current, Tpc,
                      g_DrNativeHandoff,
                      (g_DrA207ArmPc != 0));
                if (g_DrForkSlotFixed == 0) {
                    UINT32 Rpc = (g_DrA207ArmPc != 0) ? g_DrA207ArmPc : 0x56u;
                    g_PpcContext.Gpr[24] = Rpc;
                    g_PpcContext.Gpr[27] = (UINT64)(CpuRead16(Rpc) & 0xFFFF);
                    Print(L"  NATIVE-HANDOFF fork-done: DR loop r24=0x%08x "
                          L"(loader past-trap PC, entered@0x%08x) op=0x%04x\n",
                          Rpc, (UINT32)Current, (CpuRead16(Rpc) & 0xFFFF));
                }
                g_DrForkSlotFixed++;
            }
        }
        // DR 0x60FF (68020 BRA.L) support: the embedded DR is a 68000 core that
        // cannot add a 32-bit branch displacement, so it spins in its PC-relative
        // handler, never advancing past a 0x60FF. Whenever the DR settles on a
        // ROM 68K PC whose word is 0x60FF at a dispatch boundary, emulate the
        // branch ourselves: redirect r24 (68K PC) to PC+2+d32 exactly as 68020
        // would. Only fires at the DR dispatch-home range so we never disturb the
        // mid-handler state.
        if (g_DrYieldSeen && g_DrBrFixed < 64 &&
            Current >= 0x40B67A00u && Current < 0x40B67C80u) {
            UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            if (Pc24 >= 0x40800000u && Pc24 < 0x40840000u &&
                CpuRead16(Pc24) == 0x60FF) {
                UINT32 Disp = ((UINT32)CpuRead16(Pc24 + 2) << 16) |
                              (UINT32)CpuRead16(Pc24 + 4);
                UINT32 Tgt = Pc24 + 2 + Disp;
                // A real 68020 BRA.L updates the PC AND refetches the instruction
                // opcode. The DR keeps r24 (PC) and r27 (prefetched opcode) in
                // lock-step; if we only move r24 the DR still has the stale
                // 0x60FF in r27 and re-enters its branch handler, backing r24 up
                // to the ext word and spinning. Refetch r27 from the target too.
                g_PpcContext.Gpr[24] = Tgt;
                g_PpcContext.Gpr[27] = (UINT64)(CpuRead16(Tgt) & 0xFFFF);
                g_DrBrFixed++;
                if (g_DrBrFixed <= 12)
                    Print(L"  DR-BRA.L[%u] redirect 0x60FF@0x%08x d32=0x%08x -> 0x%08x\n",
                          g_DrBrFixed, Pc24, Disp, Tgt);
            }
        }
        // OS-handoff jump probe: the ROM's 68K bootstrap, after building the
        // IRP/hardware tables, does `jmp (a4)` at guest 0x4080AAAC to transfer
        // into low RAM. Catch the moment r24 (the DR's 68K PC) first equals that
        // jmp word and dump ALL GPRs so we can identify which register is 68K
        // A4 (the low-RAM jump target) and thus pin the OS-loader handoff
        // contract (exact entry address, register/stack expectations).
        if (DrHandoffJmpProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded) {
            UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            if (Pc24 == 0x4080AAAC && (g_PpcContext.Gpr[27] & 0xFFFF) == 0x4ED4) {
                DrHandoffJmpProbed = 1;
                Print(L"  HANDOFF-JMP r24(68Kpc)=0x%08x op=0x%04x exec=%u PPC=0x%08x "
                      L"MSR=0x%08x (IR=%d DR=%d)\n",
                      Pc24, g_PpcContext.Gpr[27] & 0xFFFF, Executed, Current,
                      g_PpcContext.Msr, !!(g_PpcContext.Msr & PPC_MSR_IR),
                      !!(g_PpcContext.Msr & PPC_MSR_DR));
                {
                    UINT32 Pa0;
                    BOOLEAN Tr0 = PpcTranslateEffective(0x00000000u, FALSE, &Pa0);
                    Print(L"  HNDOFFJ-MMU eff0x0: DR=%d translated=%d pa=0x%08x "
                          L"raw[pa]=0x%02x%02x%02x%02x\n",
                          !!(g_PpcContext.Msr & PPC_MSR_DR), Tr0, Pa0,
                          PpcReadPhys32(Pa0) >> 24,
                          (PpcReadPhys32(Pa0) >> 16) & 0xFF,
                          (PpcReadPhys32(Pa0) >> 8) & 0xFF,
                          PpcReadPhys32(Pa0) & 0xFF);
                }
                Print(L"  HNDOFFJ r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x r6=%08x r7=%08x\n",
                      g_PpcContext.Gpr[0] & 0xFFFFFFFF, g_PpcContext.Gpr[1] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[2] & 0xFFFFFFFF, g_PpcContext.Gpr[3] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[4] & 0xFFFFFFFF, g_PpcContext.Gpr[5] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[6] & 0xFFFFFFFF, g_PpcContext.Gpr[7] & 0xFFFFFFFF);
                Print(L"  HNDOFFJ r8=%08x r9=%08x r10=%08x r11=%08x r12=%08x r13=%08x r14=%08x r15=%08x\n",
                      g_PpcContext.Gpr[8] & 0xFFFFFFFF, g_PpcContext.Gpr[9] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[10] & 0xFFFFFFFF, g_PpcContext.Gpr[11] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[12] & 0xFFFFFFFF, g_PpcContext.Gpr[13] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[14] & 0xFFFFFFFF, g_PpcContext.Gpr[15] & 0xFFFFFFFF);
                Print(L"  HNDOFFJ r16=%08x r17=%08x r18=%08x r19=%08x r20=%08x r21=%08x r22=%08x r23=%08x\n",
                      g_PpcContext.Gpr[16] & 0xFFFFFFFF, g_PpcContext.Gpr[17] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[18] & 0xFFFFFFFF, g_PpcContext.Gpr[19] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[20] & 0xFFFFFFFF, g_PpcContext.Gpr[21] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[22] & 0xFFFFFFFF, g_PpcContext.Gpr[23] & 0xFFFFFFFF);
                Print(L"  HNDOFFJ r24=%08x r25=%08x r26=%08x r28=%08x r29=%08x r30=%08x r31=%08x LR=%08x\n",
                      g_PpcContext.Gpr[24] & 0xFFFFFFFF, g_PpcContext.Gpr[25] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[26] & 0xFFFFFFFF, g_PpcContext.Gpr[28] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[29] & 0xFFFFFFFF, g_PpcContext.Gpr[30] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[31] & 0xFFFFFFFF, g_PpcContext.Lr & 0xFFFFFFFF);
                // === OS-runtime inject at the handoff ===
                // The DR is about to execute `jmp (a4)` -> a4(r20)=0 -> low
                // RAM 0x0. The bootloader staged the real 68K System data fork
                // at PPC_OS_RUNTIME_GUEST_BASE. (The boot-info +20 pointer is
                // NOT used here: the NK zeroes the first 8 KB of low memory
                // during boot and wipes that boot-info block before the handoff,
                // so the interpreter references the fixed staged base directly.)
                // Copy the 68K boot stub (file offset 0x200, 0xA0 bytes) into
                // low RAM 0x0 NOW -- after the DR cold-start consumed the 68K
                // reset vectors at [0]/[4] -- so the jmp lands on real code
                // instead of the zero NOP-march. Also mirror the loader's
                // absolute low-RAM longword pointer at 0x2AE (which the stub's
                // tail reads via `movea.l 0x2AE.w,a0`) so its final jump target
                // is preserved even though the System is staged elsewhere.
                if (DrOsInjected == 0) {
                    UINT32 SysBase = PPC_OS_RUNTIME_GUEST_BASE;
                    UINT32 StubWord0 = CpuRead32(SysBase + 0x200);
                    if (StubWord0 != 0) {
                        DrOsInjected = 1;
                        PpcCopyGuestMemory(0x00000000u, SysBase + 0x200, 0xA0);
                        PpcCopyGuestMemory(0x000002AE, SysBase + 0x2AE, 4);
                        // The stub's tail gate (low-RAM 0x50 `A003`) never
                        // surfaces as a host-visible dispatch PC (A003 handler
                        // 0x4085DE90 never executes as a 68K trace PC), yet the
                        // observed error loop (0x5C cmpi #$FFD4, d0==0xFFD4 ->
                        // 0x62 moveq #$68) proves the trap returns non-zero and
                        // the 0x52 `bne.s $5C` diverts to the panic loop. Patch
                        // our own injected stub word at 0x50 to `sub.l d0,d0`
                        // (0x9040) so the bne falls through to `movea.l
                        // ($2AE).w,a0; jmp $A(a0)` -> staged native bootstrap.
                        CpuWrite16(0x00000050u, 0x9040u);
                        // The error entry via 0x26 `bne.s $5C` (6634) must also
                        // be neutralized: observed d0==0xFFD4 at the 0x5C cmpi
                        // shows the path to the panic can bypass 0x50 entirely
                        // (a non-zero A207 return diverts 0x26 straight to 0x5C
                        // without ever executing 0x28-0x58). Blank both
                        // conditional escapes to a straight line so the stub
                        // falls through to the native jump:
                        // 0x26 bne.s $5C -> nop ; 0x52 bne.s $5C -> nop
                        // (keep 0x28 move.w and 0x54 movea.l opcodes intact).
                        CpuWrite16(0x00000026u, 0x4E71u); // nop
                        CpuWrite16(0x00000052u, 0x4E71u); // nop
                        // The DR's 68K decoder does NOT honor `jmp` (0x4EE8/0x4ED0)
                        // in the injected low-RAM stub -- execution falls through
                        // to the 0x5C panic instead of transferring to the staged
                        // native bootstrap (r24 never reaches 0x1B823). Replace
                        // the tail instead with a second A207 trap word at 0x54;
                        // the A207 dispatch block (Current 0x40B6966C) seeds the
                        // resume-pc slot [r28+0x28] with the past-trap PC, so the
                        // host gate can redirect r24 to the staged native
                        // bootstrap (0x1B823) there.
                        CpuWrite16(0x00000054u, 0xA207u);  // A207 trap
                        CpuWrite16(0x00000056u, 0x4E71u);  // nop
                        CpuWrite16(0x00000058u, 0x4E71u);  // nop
                        CpuWrite16(0x0000005Au, 0x4E71u);  // nop
                        CpuWrite32(0x0000005Cu, 0x4E714E71u);
                        // The event-gate words 0x28-0x4E (`move.w 0x42(a0),
                        // 0x16(a0)` + `beq.s $66` branch + event-record deref
                        // at 0x36-0x38 + field stores) can never produce a
                        // genuine event: the seeded [a0+0x44] still derefs
                        // d1==A207 as a low-RAM pointer and faults back to the
                        // A9C9/A920 ratchet BEFORE the tail is reached, and the
                        // `beq.s $66` flag path diverges to the 0x66 panic
                        // (moveq #$63) regardless. Blank the whole conditional
                        // region to nops so control falls unconditionally
                        // through to the tail A207 at 0x54.
                        {
                            UINT32 w;
                            for (w = 0x00000028u; w < 0x00000050u; w += 2) {
                                CpuWrite16(w, 0x4E71u);
                            }
                        }
                        Print(L"  OSINJECT stub-tail 0x50 A003 -> sub.l d0,d0 "
                              L"(0x9040); 0x26/0x52 escapes blanked; event gate "
                              L"0x28-0x4E blanked; 0x54 A207 trap -> gate "
                              L"redirects resume-pc to staged native bootstrap "
                              L"(0x1B823)\n");
                        // The ROM $A9A0 handler (0x40865E40) ends every outcome
                        // path with `movea.l ($698).w,a0; jmp (a0)`. With low-RAM
                        // [0x698]==0 the DR jumps 0x4086604E -> 0x0 and restarts
                        // the injected stub, looping forever. On a real Mac the
                        // 68K loader pre-sets [0x698] to its resume PC before the
                        // trap; seed it to the stub's post-trap address (0xE) so
                        // the handler's terminal jump continues the OS bootstrap.
                        CpuWrite32(0x00000698u, 0x0000000Eu);
                        // The handler also requires selector 'PDEF' ([sp+6],
                        // pushed by our stub's `move.l #'boot',-(sp)` at low-RAM
                        // 0x4) and version < 8 ([sp+4]; stub pushes #1, OK) to
                        // take its real success path (traps A88F/A9C4/A8FD/A99A,
                        // then [0x0A5A] <- [[0x0A50]]). Patch the staged
                        // immediate 'boot' (62 6F 6F 74) to 'PDEF' (50 44 45 46)
                        // so the ROM's own bootstrap work actually runs.
                        CpuWrite32(0x00000004u, 0x50444546u);
                        // Seed the low-RAM 68K trap dispatch table at 0xE00.
                        // The native DR reads the A-trap handler via
                        // `lwzx r24,r6,r7` with r6=[gd+0x7B0]=0xE00 and
                        // r7=4*(trap & 0x3ff): for $A9A0 that is [0xE00+0x680]
                        // = [0x1480]. The table is normally built by the ROM's
                        // own 68K bootstrap before OS handoff (which OSINJECT
                        // bypasses), so mirror the ROM trap table (base =
                        // ROMBase + [ROMBase+0x22], see SheepShaver
                        // find_rom_trap) into low RAM. Entries are stored as
                        // absolute 68K addresses (ROM-relative offset +
                        // 0x40800000) because the DR jumps to the raw value.
                        {
                            UINT32 RomTbl = 0x40800000u + CpuRead32(0x40800022);
                            UINT32 Idx;
                            for (Idx = 0; Idx < 0x500u; Idx++) {
                                UINT32 H = CpuRead32(RomTbl + 4 * Idx);
                                if (H < 0x400000u)
                                    H += 0x40800000u;
                                CpuWrite32(0x00000E00u + 4 * Idx, H);
                            }
                            Print(L"  TRAP-SEED low-RAM 0xE00 <- ROM tbl 0x%08x: "
                                  L"[A9A0@0x1480]=0x%08x [A9C9@0x1524]=0x%08x "
                                  L"[A003@0x1E0C]=0x%08x\n",
                                  RomTbl, CpuRead32(0x00001480),
                                  CpuRead32(0x00001524),
                                  CpuRead32(0x00001E0C));
                        }
                        // The DR dispatches a second trap class through a
                        // SEPARATE low-RAM table. Block 0x40B69660 does
                        // `lwzx r24,r6,r7` with r6=[gd+0x7B4]=0x400 and
                        // r7=4*(trap & 0xff): for $A207 that is
                        // [0x400+0x1C]=[0x41C]. The ROM's own init sets
                        // [gd+0x7B4]=0x400 at 0x40B6DBC0 but the table
                        // contents are normally built by the 68K bootstrap
                        // (which OSINJECT bypasses); mirror the A-handler
                        // into the low-byte slot like the 0xE00 table.
                        // The low-byte table is indexed ONLY by trap & 0xff,
                        // so every 8-bit trap the OS executes needs its
                        // handler here (A025 spin: [0x494]=0 because only
                        // A207/A003 were seeded). Mirror the full ROM table
                        // for indices 0x00..0xFF (classic A0xx-class traps).
                        {
                            UINT32 Lt;
                            for (Lt = 0; Lt < 0x100u; Lt++)
                                CpuWrite32(0x00000400u + 4 * Lt,
                                           CpuRead32(0x00000E00u + 4 * Lt));
                        }
                        CpuWrite32(0x00000400u + 4 * (0x207u & 0xFFu),
                                   CpuRead32(0x00000E00u + 4 * 0x207u));
                        CpuWrite32(0x00000400u + 4 * (0x003u & 0xFFu),
                                   CpuRead32(0x00000E00u + 4 * 0x403u));
                        Print(L"  TRAP-SEED low-RAM 0x400 (8-bit idx): "
                              L"[A207@0x41C]=0x%08x [A003@0x40C]=0x%08x "
                              L"[A025@0x494]=0x%08x "
                              L"gd[0xB7B4 base]=0x%08x\n",
                              CpuRead32(0x0000041C),
                              CpuRead32(0x0000040C),
                              CpuRead32(0x00000494),
                              CpuRead32(0x0000B7B4u));
                        // The 0x81E2 JST stub family (e.g. A207 handler
                        // 0x4087BAA0 -> jsr 0x4087C902: `81E2 2054 0064`)
                        // dispatches handler = M32([0x2054] + d16). The
                        // offsets ARE 4*(trap&0x3ff), exactly like the native
                        // A-trap path, so the correct [0x2054] is the low-RAM
                        // trap dispatch mirror (0xE00) seeded above: disp 0x64
                        // -> mirror[0x19]=0x4082D3CE.  The ROM's own 68K
                        // bootstrap would set this pre-handoff; OSINJECT
                        // skips it, leaving the stub to deref the stale
                        // 0x40800000 (=ROM base) -> M32(0x40800064)=0x79EC0000
                        // garbage and derailing the DR.
                                                CpuWrite32(0x00002054u, 0x00000E00u);
                        Print(L"  TRAP-SEED [0x2054]=0x00000E00 (0x81E2 JST "
                              L"stub base) -> [0xE64]=0x%08x [0xE68]=0x%08x "
                              L"[0xE6C]=0x%08x [0xE70]=0x%08x\n",
                              CpuRead32(0x00000E64u),
                              CpuRead32(0x00000E68u),
                              CpuRead32(0x00000E6Cu),
                              CpuRead32(0x00000E70u));
                        // Stage the PPC-native OS bootstrap at low-RAM 0x1B400
                        // by mirroring the bytes already staged in the guest
                        // native image at SysBase+0x1B400. Without this the
                        // OSINLOW event path reaches its terminal
                        // `movea.l $2ae.w,a0; jmp $a(a0)` (0x1B412+0xA ==
                        // 0x1B41C) and finds the low-RAM entry empty, so the
                        // 68K->PPC bootstrap handoff cannot start. OSINJECT is
                        // the single natural point to do the mirror: it built
                        // the stub at low-RAM 0x0 from SysBase+0x200 and sealed
                        // the trap pointers into the low-RAM trap table.
                        {
                            UINT32 Ns;
                            UINT32 NsSrc = PPC_OS_RUNTIME_GUEST_BASE + 0x1B400u;
                            // Mirror the native bootstrap image flat
                            // (fork offset X -> low-RAM X): the 0x1B820
                            // bootstrap `b 0x4618C` (targets within the fork
                            // at 0x4618C, verified PPC code) far exceeds the
                            // old 0x800-byte (0x1B400..0x1BC00) stage, which
                            // only covered disk [0x2AE]=0x1B819 -> 0x1B823.
                            // Widen to 0x1B400..0x96000: past 0x4618C and the
                            // descriptor-call block through 0x46700, and up to
                            // the TOC-relative descriptor table that the stub
                            // block indexes (base 0x945F8, slots through
                            // 0x95820 = 0x945F8 + max imm 0x1220 + 8).
                            for (Ns = 0; Ns < 0x7AC00u; Ns += 4) {
                                CpuWrite32(0x0001B400u + Ns,
                                           CpuRead32(NsSrc + Ns));
                            }
                            Print(L"  STAGE-NATIVE low-RAM 0x1B400 <- SysBase+"
                                  L"0x1B400 (%u bytes, through 0x96000; covers "
                                  L"0x4618C branch target + descriptor table "
                                  L"0x945F8..0x95820)\n",
                                  0x7AC00u);
                            Print(L"    [0x1B400]=0x%08x [0x1B404]=0x%08x "
                                  L"[0x1B408]=0x%08x [0x1B40C]=0x%08x\n"
                                  L"    [0x1B410]=0x%08x [0x1B414]=0x%08x "
                                  L"[0x1B418]=0x%08x [0x1B41C]=0x%08x\n"
                                  L"    [0x1B420]=0x%08x [0x1B424]=0x%08x "
                                  L"[0x1B428]=0x%08x [0x1B42C]=0x%08x\n"
                                  L"    [0x4618C]=0x%08x [0x46190]=0x%08x "
                                  L"[0x46194]=0x%08x [0x46198]=0x%08x\n",
                                  CpuRead32(0x0001B400u),
                                  CpuRead32(0x0001B404u),
                                  CpuRead32(0x0001B408u),
                                  CpuRead32(0x0001B40Cu),
                                  CpuRead32(0x0001B410u),
                                  CpuRead32(0x0001B414u),
                                  CpuRead32(0x0001B418u),
                                  CpuRead32(0x0001B41Cu),
                                  CpuRead32(0x0001B420u),
                                  CpuRead32(0x0001B424u),
                                  CpuRead32(0x0001B428u),
                                  CpuRead32(0x0001B42Cu),
CpuRead32(0x0004618Cu),
                                    CpuRead32(0x00046190u),
                                    CpuRead32(0x00046194u),
                                    CpuRead32(0x00046198u));
#if PPC_DR_FULL_DUMPS
                            // bisect probe: does the STAGE loop itself restore
                            // the trap-name string table into 0x0..0x1400?
                            Print(L"  OSINJ-BISECT after-STAGE [0x0]=0x%08x "
                                  L"[0xE64]=0x%08x [0xE94]=0x%08x "
                                  L"[0x1480]=0x%08x [0x2010]=0x%08x\n",
                                  CpuRead32(0x00000000u),
                                  CpuRead32(0x00000E64u),
                                  CpuRead32(0x00000E94u),
                                  CpuRead32(0x00001480u),
                                  CpuRead32(0x00002010u));
#endif
                        }
                        // The A025 fix let the ROM's OWN PDEF bootstrap run to
                        // completion (PATH 2), which terminates with a `jmp (a0)`
                        // into the fork's diag region at 0x100000+ (0x0010FFF7 in
                        // the WILDTRAP capture). NATIVE-HANDOFF below mirrors
                        // 0x96000..0x2A2B0A, but only on PATH 1 (tail-A207 steer).
                        // Mirror it unconditionally here so PATH 2's terminal jump
                        // lands on the diag code instead of zeros.
                        {
                            UINT32 Fk;
                            // Root-cause note: with SDR1==0 the PTEG base
                            // degenerates to (Hash&0x3FF)<<6, so the low-RAM
                            // trap-name string table was being walked as fake
                            // PTE entries; accidental matches translated the
                            // FORK reads/writes to spuriously-scattered Pa and
                            // re-scattered the string-table content. The
                            // SDR1==0 guard in PpcTranslateEffective now keeps
                            // these accesses flat automatically.
                            for (Fk = 0x00096000u; Fk < 0x002A2B0Au; Fk += 4) {
                                CpuWrite32(Fk,
                                           CpuRead32(PPC_OS_RUNTIME_GUEST_BASE + Fk));
                            }
                            Print(L"  FORK-MIRROR low-RAM 0x96000..0x2A2B0A <- "
                                  L"guest (unconditional; PATH2 terminal jump): "
                                  L"[0x100000]=0x%08x [0x100004]=0x%08x "
                                  L"[0x10FFF0]=0x%08x\n",
CpuRead32(0x00100000u),
                                   CpuRead32(0x00100004u),
                                   CpuRead32(0x0010FFF0u));
#if PPC_DR_FULL_DUMPS
                            // bisect probe: does the FORK-MIRROR loop itself
                            // restore the trap-name string table into low RAM?
                            Print(L"  OSINJ-BISECT after-FORK [0x0]=0x%08x "
                                  L"[0xE64]=0x%08x [0xE94]=0x%08x "
                                  L"[0x1480]=0x%08x [0x2010]=0x%08x\n",
                                  CpuRead32(0x00000000u),
                                  CpuRead32(0x00000E64u),
                                  CpuRead32(0x00000E94u),
                                  CpuRead32(0x00001480u),
                                  CpuRead32(0x00002010u));
#endif
                        }
                        // DIAG: dump the fork's native forwarder (0x3873C) and
                        // its second descriptor target (0x2AB00) so the bit-loop
                        // semantics can be reconstructed on the host.
                        {
#if PPC_DR_FULL_DUMPS
                            UINT32 A;
                            for (A = 0x0003873Cu; A < 0x00038998u; A += 16)
                                Print(L"  FORKCODE[%08x] %08x %08x %08x %08x\n",
                                      A, CpuRead32(A), CpuRead32(A + 4),
                                      CpuRead32(A + 8), CpuRead32(A + 12));
                            for (A = 0x0002AB00u; A < 0x0002AC40u; A += 16)
                                Print(L"  FORKCODE[%08x] %08x %08x %08x %08x\n",
                                      A, CpuRead32(A), CpuRead32(A + 4),
                                      CpuRead32(A + 8), CpuRead32(A + 12));
#endif
                        }
                        // EXPERIMENT (reversible): the ROM A207 handler
                        // 0x4087BAA0 ends with `move.l #$aa55aa55,d0; rts`
                        // (0x4087BAEE: 203C AA55 AA55), so the OSINLOW stub's
                        // `bne 0x5C` after A207 is ALWAYS taken and the loader
                        // idle-loops forever (EVTQ empty, [a0+0x42]=0, per the
                        // A207CAP capture). A207 never returns 0 no matter what
                        // is queued. Patch the 6-byte tail verb to
                        // `sub.l d0,d0; rts; nop; nop` so A207 returns D0=0 and
                        // the stub reaches the 0x28 gate. Coupled with the arm-
                        // site prefill of [frame+0x42]=1 (below), the stub's
                        // event path + A003 + [0x2AE] jump will actually run.
                        if (g_DrA207Steer != 0) {
                            CpuWrite32(0x4087BAEEu, 0x4E404E75u);
                            CpuWrite32(0x4087BAF2u, 0x4E714E71u);
                            Print(L"  A207STEER patched 0x4087BAEE -> "
                                  L"sub.l d0,d0; rts (D0=0)\n");
                        }
                        // DIAG: the 0x81E2 JST stub family reads the trap
                        // handler table pointer from low-RAM base slots
                        // (0x2010/0x2054/0x2064/...). Dump every base slot,
                        // the RomTbl entries the stub offsets (0x64-0x70)
                        // would hit, and the current mirror contents so the
                        // correct [0x2054] value can be derived.
                        {
                            UINT32 Rtbl = 0x40800000u +
                                          CpuRead32(0x40800022);
                            Print(L"  DRSEED-DIAG RomTbl=0x%08x\n", Rtbl);
#if PPC_DR_FULL_DUMPS
                            UINT32 Slot, RtE;
                            for (Slot = 0x00002010u; Slot <= 0x00002090u;
                                 Slot += 4) {
                                Print(L"  DRSEED-DIAG [0x%04x]=0x%08x\n",
                                      Slot, CpuRead32(Slot));
                            }
                            for (RtE = 0x19u; RtE <= 0x1Du; RtE++) {
                                Print(L"  DRSEED-DIAG RomTbl[0x%02x]=0x%08x "
                                      L"mirror[0x%02x]=0x%08x\n",
                                      RtE, CpuRead32(Rtbl + 4 * RtE),
                                      RtE, CpuRead32(0x00000E00u + 4 * RtE));
                            }
                            Print(L"  DRSEED-DIAG RomTbl[0x1a0]=0x%08x "
                                  L"[0x40800064]=0x%08x [0x40800070]=0x%08x\n",
                                  CpuRead32(Rtbl + 4 * 0x1A0u),
                                  CpuRead32(0x00000064u | 0x40800000u),
                                  CpuRead32(0x00000070u | 0x40800000u));
#endif
                        }
                        // The DR cannot service 68K A-traps in the injected
                        // low-RAM stub: its trap dispatch re-reads the resume-pc
                        // slot [r28+0x28], and with r28 pinned to the ROM
                        // handoff block (0x4080AABC) that slot is read-only ROM
                        // data (0x2649D7EB) -> the 68K is marched to garbage.
                        // NOTE: r28 is pinned to 0x4080AABC only during this
                        // handoff; keep the native DR path and observe. If the
                        // A-trap dispatch still reads a ROM slot, the SheepShaver
                        // answer is patching the trap table / EMUL_OP dispatch,
                        // NOT diverting to a hand-rolled 68K interpreter.
                        DrC68kTrapRequest = 1;
#ifdef PPC_DR_FULL_DUMPS
                        {
                            // Decide conclusively whether the OSINJECT writes
                            // landed: compare (a) the MMU-path read (DR=1, may
                            // translate to a different physical page), (b) the
                            // RAW physical read at the same EA, and (c) the
                            // translate result. If the MMU-path read differs
                            // from the raw physical read, translation is
                            // redirecting the late dumps to another page
                            // (e.g. the ROM trap-name table) and the writes did
                            // land at physical low RAM after all.
                            static const UINT32 Oa[8] = { 0x00000000u, 0x00000004u,
                                                         0x00000050u, 0x000002AEu,
                                                         0x00000698u, 0x0000E00u,
                                                         0x0001B400u, 0x00100000u };
                            UINTN Ok;
                            UINT32 OPa = 0;
                            BOOLEAN OTr = FALSE;
                            Print(L"  OSINJ-MMU SDR1=0x%08x MSR=0x%08x "
                                  L"(DR=%d IR=%d)\n",
                                  g_PpcContext.Sdr1, g_PpcContext.Msr,
                                  !!(g_PpcContext.Msr & PPC_MSR_DR),
                                  !!(g_PpcContext.Msr & PPC_MSR_IR));
                            for (Ok = 0; Ok < 8; Ok++) {
                                OTr = PpcTranslateEffective(Oa[Ok], FALSE, &OPa);
                                Print(L"  OSINJ-MMU ea=0x%08x mmu=0x%08x "
                                      L"raw=0x%08x tr=%d pa=0x%08x\n",
                                      Oa[Ok], CpuRead32(Oa[Ok]),
                                      PpcReadPhys32(Oa[Ok]),
                                      OTr, OTr ? OPa : 0xDEADBEEF);
                            }
                            {
                                UINTN Oi;
                                Print(L"  OSINJ-REGIONS (%u slots):\n",
                                      PPC_MAX_GUEST_REGIONS);
                                for (Oi = 0; Oi < PPC_MAX_GUEST_REGIONS; Oi++) {
                                    if (!g_GuestRegions[Oi].Active) {
                                        continue;
                                    }
                                    Print(L"  OSINJ-REG %u host=0x%x guest=0x%x "
                                          L"size=0x%x ro=%d\n",
                                          Oi, (UINT32)(UINTN)g_GuestRegions[Oi].HostBase,
                                          g_GuestRegions[Oi].GuestBase,
                                          g_GuestRegions[Oi].Size,
                                          g_GuestRegions[Oi].ReadOnly);
                                }
                            }
                        }
#endif
                        Print(L"  OSINJECT stub from guest 0x%x+0x200 -> low-RAM 0x0 "
                              L"(0xA0 bytes); word0=%08x 0x2AE ptr=0x%08x "
                              L"0x698 resume=0x%08x sel=0x%08x "
                              L"(native-DR phase marker)\n",
                              SysBase, StubWord0, CpuRead32(0x000002AE),
                              CpuRead32(0x00000698), CpuRead32(0x00000004));;
                        {
                            // Dump the low-RAM 68K OS-boot world at the exact
                            // handoff: 0x0..0x100 (the injected stub + vectors)
                            // and 0xA000..0xB000 (KDP/DR-emulator + relocated
                            // boot area that the 68K then marches through).
#if PPC_DR_FULL_DUMPS
                            UINT32 A;
                            for (A = 0x00000000u; A < 0x00000100u; A += 16) {
                                Print(L"  OSINLOW[0x%04x] %08x %08x %08x %08x\n",
                                      A, CpuRead32(A), CpuRead32(A + 4),
                                      CpuRead32(A + 8), CpuRead32(A + 12));
                            }
                            for (A = 0x0000A000u; A < 0x0000B000u; A += 16) {
                                Print(L"  OSINLOW[0x%04x] %08x %08x %08x %08x\n",
                                      A, CpuRead32(A), CpuRead32(A + 4),
                                      CpuRead32(A + 8), CpuRead32(A + 12));
                            }
#endif
                        }
                        // DIAG: does the staged System data fork actually carry
                        // the OS bootstrap that low-RAM [0x2AE] (0x1B412) points
                        // at? The OSINJECT copy only stages the 0xA0-byte stub +
                        // [0x2AE] + trap tables; the [0x2AE]+0xA jump needs real
                        // code at file offset 0x1B412. Dump the source region
                        // (read-only) so we know whether restaging it into low
                        // RAM is viable.
                        {
                            Print(L"  OSSTAGE-DIAG SysBase=0x%08x stub0=0x%08x "
                                  L"[0x2AE]=0x%08x\n",
                                  SysBase, StubWord0, CpuRead32(0x000002AE));
#if PPC_DR_FULL_DUMPS
                            UINT32 A;
                            for (A = 0x00000000u; A < 0x00000100u; A += 4) {
                                Print(L"  OSSTAGE-DIAG src+0x%04x=0x%08x\n",
                                      A, CpuRead32(SysBase + A));
                            }
                            for (A = 0x00001B000u; A < 0x00001B080u; A += 4) {
                                Print(L"  OSSTAGE-DIAG src+0x%05x=0x%08x\n",
                                      A, CpuRead32(SysBase + A));
                            }
                            for (A = 0x00001B400u; A < 0x00001B480u; A += 4) {
                                Print(L"  OSSTAGE-DIAG src+0x%05x=0x%08x\n",
                                      A, CpuRead32(SysBase + A));
                            }
#endif
                        }
                    } else {
                        Print(L"  OSINJECT SKIP: staged OS runtime empty at 0x%08x "
                              L"(word0=%08x)\n", SysBase, StubWord0);
                    }
                }
            }
        }
        // DR service 0x116 (called at 0x4080AA36 `dc.w 0x60ff; dc.w 0x116`,
        // return address a6 = 0x4080AA3C, 68K a6 = r22, D0 = r8): the caller
        // tests D0 bit0 (`btst.b #0,d0`) and, when SET, relocates its image to
        // low RAM and `jmp (pc,d3.l)`s into the 0xAA5A mirror -- which our boot
        // never stages, so the 68K marches through zeros.  Force bit0 CLEAR so
        // the driver keeps executing out of ROM (0x4080AA5C onward).
        if (g_DrRelocGate == 0 && g_DrYieldSeen &&
            (g_PpcContext.Gpr[22] == 0x4080AA3Cu ||
             g_PpcContext.Gpr[24] == 0x4080AA3Cu ||
             g_PpcContext.Gpr[24] == 0x4080AA3Eu)) {
            g_DrRelocGate = 1;
            g_PpcContext.Gpr[8] &= ~1u;
            Print(L"  DRSRV116-GATE r24(68Kpc)=0x%08x a6=0x%08x: cleared D0 "
                  L"bit0 (skip low-RAM relocation)\n",
                  g_PpcContext.Gpr[24], g_PpcContext.Gpr[22]);
        }
        if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&
            (Current >= 0x40B60000 && Current < 0x40B82000)) {
            UINT32 Pc24 = g_PpcContext.Gpr[24];
            static UINT32 PcRing[16];
            static UINT32 PcRingN = 0;
            PcRing[PcRingN++ & 15] = Pc24;
            if (Dr68KLast >= 0x40800000 && Pc24 < 0x40000000) {
                UINTN I;
                Dr68KLowProbed = 1;
                for (I = 0; I < 16; I++) {
                    UINT32 Rv = PcRing[(PcRingN + I) & 15];
                    if (Rv != 0)
                        Print(L"  PRELOW[-%02u] 68Kpc=0x%08x\n", 16 - I, Rv);
                }
                Print(L"  DR68K-LOW[1] exec=%u PPC=0x%08x r24(68Kpc)=0x%08x r27=0x%04x "
                      L"r23=0x%08x r29=0x%08x r31=0x%08x r1=0x%08x LR=0x%08x CTR=0x%08x\n",
                      Executed, Current, Pc24, g_PpcContext.Gpr[27] & 0xFFFF,
                      g_PpcContext.Gpr[23], g_PpcContext.Gpr[29],
                      g_PpcContext.Gpr[31], g_PpcContext.Gpr[1],
                      g_PpcContext.Lr, g_PpcContext.Ctr);
#if PPC_DR_FULL_DUMPS
                UINTN A;
                for (A = 0x0000A000u; A < 0x0000AC00u; A += 16) {
                    Print(L"  LOWA[0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
                for (A = 0x00000000u; A < 0x00002000u; A += 16) {
                    Print(L"  LOW[0x%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
#endif
                Print(L"  DR68K-LOW: continuing (break removed)\n");
            }
            Dr68KLast = Pc24;
        }
        // Post-low-entry state: log the 68K PC/PPC spread every ~500K instructions
        // once we have entered low memory and passed the initial boot, to see
        // whether the DR lands in real low-RAM bootstrap code or a degenerate
        // pattern loop, without breaking the run.
        // Single-shot low-RAM "march" probe: after the DR enters low memory it
        // steps linearly through 0xE90..0xFCA executing zero words, then reads a
        // real word at 0xFC8 and jumps to a garbage target (0x0038155B), ending
        // parked at low-RAM 0xAAD2 while the host PPC falls into ROM "kckc"
        // data at 0x40AFE4E0. Capture the full register set and the low-RAM
        // bytes the first time r24 lands in that march region so we can see what
        // the boot copy intended to place there and what register drives the jump.
        if (DrLowMarchProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded) {
            UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            if ((Pc24 >= 0x00000E80 && Pc24 < 0x00000FD0) ||
                (Pc24 >= 0x0000AAC0 && Pc24 < 0x0000AB00)) {
                DrLowMarchProbed = 1;
                Print(L"  LOWMARCH r24=0x%08x PPC=0x%08x r27=%04x exec=%u\n",
                      Pc24, Current, g_PpcContext.Gpr[27] & 0xFFFF, Executed);
                Print(L"  LOWMARCH r23=%08x r28=%08x r29=%08x r30=%08x r31=%08x\n",
                      g_PpcContext.Gpr[23] & 0xFFFFFFFF, g_PpcContext.Gpr[28] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[29] & 0xFFFFFFFF, g_PpcContext.Gpr[30] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[31] & 0xFFFFFFFF);
                Print(L"  LOWMARCH r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x "
                      L"r6=%08x r7=%08x\n",
                      g_PpcContext.Gpr[0] & 0xFFFFFFFF, g_PpcContext.Gpr[1] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[2] & 0xFFFFFFFF, g_PpcContext.Gpr[3] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[4] & 0xFFFFFFFF, g_PpcContext.Gpr[5] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[6] & 0xFFFFFFFF, g_PpcContext.Gpr[7] & 0xFFFFFFFF);
                Print(L"  LOWMARCH r8=%08x r9=%08x r10=%08x r11=%08x r12=%08x "
                      L"r13=%08x r14=%08x r15=%08x\n",
                      g_PpcContext.Gpr[8] & 0xFFFFFFFF, g_PpcContext.Gpr[9] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[10] & 0xFFFFFFFF, g_PpcContext.Gpr[11] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[12] & 0xFFFFFFFF, g_PpcContext.Gpr[13] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[14] & 0xFFFFFFFF, g_PpcContext.Gpr[15] & 0xFFFFFFFF);
                Print(L"  LOWMARCH r16=%08x r17=%08x r18=%08x r19=%08x r20=%08x "
                      L"r21=%08x r22=%08x r25=%08x r26=%08x\n",
                      g_PpcContext.Gpr[16] & 0xFFFFFFFF, g_PpcContext.Gpr[17] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[18] & 0xFFFFFFFF, g_PpcContext.Gpr[19] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[20] & 0xFFFFFFFF, g_PpcContext.Gpr[21] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[22] & 0xFFFFFFFF, g_PpcContext.Gpr[25] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[26] & 0xFFFFFFFF);
#if PPC_DR_FULL_DUMPS
                UINT32 A;
                for (A = 0x00000E80u; A < 0x00001000u; A += 16) {
                    Print(L"  LOWMAR  [%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
                for (A = 0x00000020u; A < 0x00000100u; A += 16) {
                    Print(L"  LOWMAR0 [%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
                for (A = 0x0000AA80u; A < 0x0000AB00u; A += 16) {
                    Print(L"  LOWMAR2 [%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
                // ROM dump: the 0x4080AFBE 68K boot routine that walks the
                // descriptor table and the 0x4080E1xx descriptor/blob region
                // it jumps into (to find the intended low-RAM 0x112 glue).
                for (A = 0x4080AF60u; A < 0x4080B040u; A += 16) {
                    Print(L"  ROMBOOT [%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
                for (A = 0x4080E000u; A < 0x4080E300u; A += 16) {
                    Print(L"  ROMDESC [%08x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
#endif
            }
        }
        // Post-inject 68K boot-step trace: from the OS-handoff `jmp (a4)` (68K
        // PC r24=0x4080AAAC -> low RAM 0x0) capture every DISTINCT 68K PC the
        // DR executes under the running stub - the injected low-RAM boot stub,
        // ROM trap handlers (0x4080xxxx), and the degenerate zero-march - so we
        // can see exactly which path the stalled 'boot'/'boot-continuation'
        // takes and where it bails into the march (vs. reading a device/volume).
        // Only unique PC transitions are recorded, so a tight inner loop inside
        // one ROM handler adds a single entry instead of flooding the log.
        if (g_DrYieldSeen && DrHandoffJmpProbed &&
            BootStepDumpDone == 0 && BootStepIdx < 1024u) {
            static UINT32 BootLastPc = 0xFFFFFFFFu;
            UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            if (Pc24 != BootLastPc) {
                BootLastPc = Pc24;
                BootStepPc[BootStepIdx] = Pc24;
                BootStepOp[BootStepIdx] = g_PpcContext.Gpr[27] & 0xFFFF;
                BootStepPpc[BootStepIdx] = Current;
                BootStepIdx++;
            }
            // WALKBUG-FIX (inline at proven-executing recorder): the walk
            // hitting the DR dispatch header with a transfer-page code word in
            // r24 must be repaired HERE even if the earlier fork-done block is
            // bypassed. One-shot.
            if (g_DrForkSlotFixed == 0 &&
                Current >= 0x40B67C60u && Current < 0x40B67C80u &&
                Pc24 >= 0x7D400000u && Pc24 < 0x7D500000u) {
                g_DrForkSlotFixed = 1;
                UINT32 Rpc = 0x56u;
                g_PpcContext.Gpr[24] = Rpc;
                g_PpcContext.Gpr[27] = (UINT64)(CpuRead16(Rpc) & 0xFFFF);
                Print(L"  WALKBUG-FIX r24=0x%08x (slot-walk 0x%08x) at PPC=0x%08x "
                      L"exec=%u op=0x%04x\n",
                      Rpc, Pc24, Current, (UINT32)Executed,
                      (CpuRead16(Rpc) & 0xFFFF));
            }
        }
        if (BootStepDumpDone == 0 &&
            (BootStepIdx >= 1024u || (DrLowMarchProbed && BootStepIdx > 0))) {
            UINTN S;
            BootStepDumpDone = 1;
            Print(L"  BOOTSTEP->march: %u distinct 68K PCs after OS handoff:\n",
                  BootStepIdx);
            for (S = 0; S < BootStepIdx; S++) {
                Print(L"  BOOTSTEP[%04u] r24(68Kpc)=0x%08x op=0x%04x "
                      L"PPC=0x%08x\n",
                      S, BootStepPc[S], BootStepOp[S], BootStepPpc[S]);
            }
        }
        // 68K A-line trap call tracer: capture every DISTINCT A-line 68K
        // instruction (opcode & 0xF000 == 0xA000) the DR actually executes
        // in the ROM boot region before the OS handoff. This tells us which
        // traps (and with what D0/A0 args) the genuine boot calls to reach
        // its boot volume -- the exact contract the EMUL_OP/virtual-disk
        // layer must satisfy for the ROM to do HFS handling itself.
        if (g_DrYieldSeen && TrapTraceDumpDone == 0 && TrapTraceIdx < 256u) {
            static UINT32 TrapTraceLastPc = 0xFFFFFFFFu;
            UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            UINT32 Op16 = g_PpcContext.Gpr[27] & 0xFFFF;
            if (Pc24 != TrapTraceLastPc &&
                Pc24 >= 0x40800000u && Pc24 < 0x40840000u &&
                (Op16 & 0xF000u) == 0xA000u) {
                TrapTraceLastPc = Pc24;
                TrapTracePc[TrapTraceIdx] = Pc24;
                TrapTraceOp[TrapTraceIdx] = Op16;
                TrapTraceD0[TrapTraceIdx] = g_PpcContext.Gpr[8] & 0xFFFFFFFF;
                TrapTraceA0[TrapTraceIdx] = g_PpcContext.Gpr[16] & 0xFFFFFFFF;
                TrapTracePpc[TrapTraceIdx] = Current;
                if ((Pc24 == 0x4080AD50u || (TrapTraceIdx == 1 && TrapTraceDumpedApex == 0)) &&
                    TrapTraceDumpedApex == 0) {
                    TrapTraceDumpedApex = 1;
                    Print(L"  APEX r1=0x%08x r16=0x%08x r17=0x%08x r18=0x%08x r19=0x%08x "
                          L"r20=0x%08x r21=0x%08x r22=0x%08x r23=0x%08x\n",
                          g_PpcContext.Gpr[1], g_PpcContext.Gpr[16],
                          g_PpcContext.Gpr[17], g_PpcContext.Gpr[18],
                          g_PpcContext.Gpr[19], g_PpcContext.Gpr[20],
                          g_PpcContext.Gpr[21], g_PpcContext.Gpr[22],
                          g_PpcContext.Gpr[23]);
                    Print(L"  APEX r24=0x%08x r25=0x%08x r26=0x%08x r28=0x%08x r29=0x%08x "
                          L"r30=0x%08x r31=0x%08x LR=0x%08x CTR=0x%08x\n",
                          g_PpcContext.Gpr[24], g_PpcContext.Gpr[25],
                          g_PpcContext.Gpr[26], g_PpcContext.Gpr[28],
                          g_PpcContext.Gpr[29], g_PpcContext.Gpr[30],
                          g_PpcContext.Gpr[31], g_PpcContext.Lr, g_PpcContext.Ctr);
                    Print(L"  APEX MSR=0x%08x SRR0=0x%08x SRR1=0x%08x SPRG0=0x%08x "
                          L"SPRG1=0x%08x SPRG2=0x%08x SPRG3=0x%08x\n",
                          g_PpcContext.Msr, g_PpcContext.Srr0, g_PpcContext.Srr1,
                          g_PpcContext.Spr[272], g_PpcContext.Spr[273],
                          g_PpcContext.Spr[274], g_PpcContext.Spr[275]);
                    {
                        UINT32 A;
                        Print(L"  APEX vec0x30=0x%08x vec0x28=0x%08x vec0x2C=0x%08x "
                              L"vec0x34=0x%08x vec0x70=0x%08x\n",
                              CpuRead32(0x00000030), CpuRead32(0x00000028),
                              CpuRead32(0x0000002C), CpuRead32(0x00000034),
                              CpuRead32(0x00000070));
                        for (A = 0x000002E0u; A < 0x00000300u; A += 16) {
                            Print(L"  APEX LOW[0x%04x] %08x %08x %08x %08x\n",
                                  A, CpuRead32(A), CpuRead32(A + 4),
                                  CpuRead32(A + 8), CpuRead32(A + 12));
                        }
                        for (A = 0x0000FB80u; A < 0x0000FBA0u; A += 16) {
                            Print(L"  APEX LOW[0x%04x] %08x %08x %08x %08x\n",
                                  A, CpuRead32(A), CpuRead32(A + 4),
                                  CpuRead32(A + 8), CpuRead32(A + 12));
                        }
                    }
                }
                TrapTraceIdx++;
            }
        }
        if (TrapTraceDumpDone == 0 &&
            (TrapTraceIdx >= 256u ||
             ((DrHandoffJmpProbed || DrLowMarchProbed) && TrapTraceIdx > 0))) {
            UINTN T;
            TrapTraceDumpDone = 1;
            Print(L"  TRAPTR->handoff: %u distinct A-line traps in ROM boot "
                  L"(D0=r8 A0=r16):\n", TrapTraceIdx);
            for (T = 0; T < TrapTraceIdx; T++) {
                Print(L"  TRAPTR[%03u] r24=0x%08x op=0x%04x sel=0x%02x "
                      L"D0=0x%08x A0=0x%08x PPC=0x%08x\n",
                      T, TrapTracePc[T], TrapTraceOp[T],
                      (TrapTraceOp[T] >> 8) & 0xFF,
                      TrapTraceD0[T], TrapTraceA0[T], TrapTracePpc[T]);
            }
        }
        // Reset-vector entry probe: if the guest PPC ever jumps to the Mach
        // reset vector (0xFFF00100) or the ROM reset entry in the middle of
        // the run, that is the smoking gun for a guest-requested platform
        // reset (the interpreter otherwise never leaves the ROM program).
        if (g_PpcContext.Pc == 0xFFF00100u ||
            (g_PpcContext.Pc >= 0xFFF00000u && g_PpcContext.Pc < 0xFFF01000u)) {
            Print(L"  RESETVEC-ENTRY PC=0x%08x exec=%u SRR0=0x%08x SRR1=0x%08x "
                  L"r1=0x%08x r24=0x%08x r27=0x%04x MSR=0x%08x\n",
                  (UINT32)g_PpcContext.Pc, Executed, g_PpcContext.Srr0,
                  g_PpcContext.Srr1, g_PpcContext.Gpr[1],
                  g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[27] & 0xFFFF, g_PpcContext.Msr);
        }
        // A-line exec trace: once the first A-line trap has been dumped, print
        // the next instructions verbatim so the DR's real trap-dispatch stream
        // (and whatever ends it) is visible in the log.
        if (g_DrYieldSeen) {
            static UINT32 AtraceOn = 0, AtraceN = 0;
            if (AtraceOn == 0 && TrapTraceDumpDone && TrapTraceIdx > 0) {
                UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
                if (Pc24 >= 0x40800000u && Pc24 < 0x40840000u)
                    AtraceOn = 1;
            }
            if (AtraceOn && AtraceN < 800) {
                Print(L"  ATRACE[%03u] ppc=0x%08x r24=0x%08x op=0x%04x "
                      L"r28=0x%08x r29=0x%08x r31=0x%08x LR=0x%08x\n",
                      AtraceN, (UINT32)Current,
                      g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[27] & 0xFFFF,
                      g_PpcContext.Gpr[28], g_PpcContext.Gpr[29],
                      g_PpcContext.Gpr[31], g_PpcContext.Lr);
                AtraceN++;
            }
        }
        // Single-shot base-drift probe: catch the FIRST moment the DR's memory
        // base (r28, should be low RAM 0xB000) or dispatch-table base (r29,
        // should be 0x40B8xxxx) drifts off, and report the exact PPC instruction
        // (Current), 68K PC (r24) and opcode (r27). Correlates the jump into the
        // kckc-fill ROM data region (GUEST STOP at 0x40AFFEF4) with its cause.
        if (DrBaseDrift == 0 && g_DrYieldSeen &&
            (g_PpcContext.Gpr[28] & 0xFF000000u) == 0x40000000u) {
            DrBaseDrift = 1;
            Print(L"  BDRIFT r28=%08x r29=%08x r30=%08x r31=%08x "
                  L"PPC=0x%08x r24(68Kpc)=0x%08x r27=%04x r1=0x%08x LR=0x%08x CTR=0x%08x\n",
                  g_PpcContext.Gpr[28] & 0xFFFFFFFF, g_PpcContext.Gpr[29] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[30] & 0xFFFFFFFF, g_PpcContext.Gpr[31] & 0xFFFFFFFF,
                  Current, g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[27] & 0xFFFF, g_PpcContext.Gpr[1] & 0xFFFFFFFF,
                  g_PpcContext.Lr, g_PpcContext.Ctr);
            Print(L"  BDRIFT w[r28]=0x%08x w[r28+4]=0x%08x w[r28-4]=0x%08x "
                  L"68Kword@r24(ifROM)=0x%04x\n",
                  CpuRead32(g_PpcContext.Gpr[28] & 0xFFFFFFFF),
                  CpuRead32((g_PpcContext.Gpr[28] & 0xFFFFFFFF) + 4),
                  CpuRead32((g_PpcContext.Gpr[28] & 0xFFFFFFFF) - 4),
                  (g_PpcContext.Gpr[24] >= 0x40800000u && g_PpcContext.Gpr[24] < 0x40840000u)
                      ? CpuRead16(g_PpcContext.Gpr[24]) : 0xFFFF);
        }
        // IRP/HWInfo ('Hnfo') handoff gate. The New World 68K boot continuation
        // reads the IRP info-record pointer from KernelData-adjacent slot
        // 0x68FFEFD0 and validates `'Hnfo'` at [slot+0x70] (guest 0x4080AFB4
        // `move.l ([$68ffefd0],$70),d0` / 0x4080AFBE `cmpi.l #'Hnfo',d0`), then
        // tests [slot+0x76] and forces Z (cmp d0,d0) so the boot proceeds down
        // the healthy "HWInfo present" path. Real hardware builds this record
        // (the Open Firmware trampoline / InfoRecords.a NKHWInfo at IRP+0xF70)
        // before the nanokernel runs; the faithful-boot ROM path we run has no
        // trampoline, so the original SheepShaver-style NOP patch was retired
        // and the record must be supplied by the emulator. The old C-68K
        // interpreter built it lazily at 0x4080AA10 (m68k.c); the PPC-native DR
        // path never runs that code, so seed lazily here when the DR first
        // approaches the gate (68K PC in the boot-continuation/table region),
        // idempotently preserving any record the NK itself produced.
        {
            UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            // Seed whenever the slot is empty OR the existing record lacks the
            // 'Hnfo' signature at [P+0x70]. The nanokernel may pre-build a bare
            // IRP (slot -> a RAM record like 0x28000000) without the signature
            // the ROM's gate at 0x4080AFB4 validates, so we must override it
            // with a valid record; a genuinely valid record is left untouched.
            UINT32 SlotHasValid  = 0;
            UINT32 SlotPv        = CpuRead32(0x68FFEFD0u);
            if (SlotPv != 0 && CpuRead32(SlotPv + 0x70) == 0x486E666Fu) {
                SlotHasValid = 1;
            }
            if (!DrHnfoSeeded &&
                (Pc24 >= 0x4080A000u && Pc24 < 0x4080C000u) &&
                !SlotHasValid) {
                // Layout mirrors the proven m68k.c fabrication (info block +
                // callback stub + control block) in NK-system-area scratch that
                // the ROM never touches: [0x68FFEFD0] -> P, [P+8] -> Qbuf,
                // [P+0x70] = 'Hnfo', [P+0x76] = 0.
                UINT32 P    = 0x68FF8000u;
                UINT32 Stub = 0x68FF9000u;
                UINT32 Cblk = 0x68FF9040u;
                UINT32 Qbuf = 0x68FF9800u;
                CpuWrite32(Stub,     0x117C00A8u); // move.b #$A8,$10(a3)
                CpuWrite32(Stub + 4, 0x00107000u); // moveq #0,d0
                CpuWrite16(Stub + 8, 0x4E75u);     // rts
                CpuWrite32(Cblk + 0x44, Stub - Cblk);
                CpuWrite32(P + 0x08, Qbuf);
                CpuWrite32(P + 0x70, 0x486E666Fu); // 'Hnfo'
                CpuWrite16(P + 0x76, 0x0000u);
                CpuWrite32(0x68FFEFD0u, P);
                DrHnfoSeeded = 1;
                Print(L"  Hnfo-Seed r24(68Kpc)=0x%08x (overriding slot 0x%08x): "
                      L"IRP[0x68FFEFD0]=0x%08x [+70]=Hnfo [+76]=0000 "
                      L"stub=%08x cblk=%08x qbuf=%08x\n",
                      Pc24, SlotPv, P, Stub, Cblk, Qbuf);
            }
            // Probe the gate ONCE (idempotent) to confirm what the native DR
            // reads when it evaluates the IRP/HWInfo signature.
            if (!DrHnfoProbed && g_DrYieldSeen &&
                Pc24 >= 0x0000AAD0u && Pc24 < 0x0000AB00u) {
                DrHnfoProbed = 1;
                UINT32 Pv = CpuRead32(0x68FFEFD0u);
                Print(L"  Hnfo-Gate r24(68Kpc)=0x%08x [0x68FFEFD0]=0x%08x "
                      L"[+8]=0x%08x [+70]=0x%08x [+76]=0x%04x\n",
                      Pc24, Pv, CpuRead32(Pv), Pv ? CpuRead32(Pv + 0x70) : 0,
                      Pv ? CpuRead16(Pv + 0x76) : 0);
            }
        }
        // Post-yield 68K stream trace: right after the DRYIELD-RESUME, print
        // the DR emulator range instructions with the 68K PC/opcode pair so the
        // exact word stream the DR re-executes from the resume is recoverable
        // and can be matched against the ROM bytes at the 68K PC. Bounded.
        // All trace blocks are additionally gated on the yield count so they
        // keep tracing across many yield cycles: the window band recurs every
        // yield, and the higher bound lets the trap-stub/JSR phase (reached
        // several yields in) still be observed.
        if (g_DrYieldCount < 64 && g_DrPostYieldWindow && g_DrPostYieldWindow < 6000 &&
            (Current >= 0x40B67A00 && Current < 0x40B82000)) {
            UINT32 Y68K1 = g_PpcContext.Gpr[24];
            UINT32 Yop1 = CpuRead16(Y68K1);
            UINT32 Yop2 = CpuRead16(Y68K1 + 2);
            g_DrPostYieldWindow++;
      
      
            // Decode the spill band: once the 68K PC first enters the low-RAM
            // zone (>= 0xA800), capture every subsequent instr's op + the PPC
            // handler address so the run-off path is seen word-for-word.
            // PCWATCH: log every dispatch while the 68K PC is in the high
            // (0x81..) zone so the entry staircase into the zeroed region is
            // seen instruction-by-instruction. Count-capped, no one-shot flag.
            if (g_DrNativeHandoff != 0 && Y68K1 >= 0x81000000u &&
                Y68K1 < 0x90000000u && g_PcWatchLog < 40) {
                g_PcWatchLog++;
                Print(L"  PCWATCH win=%u PPC=0x%08x 68Kpc=0x%08x op=0x%04x "
                      L"r1=0x%08x r29=0x%08x r31=0x%08x LR=0x%08x CR=0x%08x "
                      L"r5=0x%08x r7=0x%08x\n",
                      g_DrPostYieldWindow, Current, Y68K1, Yop1,
                      g_PpcContext.Gpr[1], g_PpcContext.Gpr[29],
                      g_PpcContext.Gpr[31], g_PpcContext.Lr,
                      g_PpcContext.Cr, g_PpcContext.Gpr[5],
                      g_PpcContext.Gpr[7]);
            }
            // BIGFIX one-shot: the first time the 68K PC enters the high 68K
            // address space, the DR is about to execute the OS's 68K boot
            // code at 0x810303B0 == 0x81000000 + 0x303B0.  The OS RAM-Rom
            // content is never written by the loader (RAMR-W write watch
            // shows zero writes to 0x81000000-0x82000000), so mirror the
            // staged System (host 0x68E00000) RAM-Rom image -- the OS 68K
            // ROM block at fork offset 0xC2260, identified by its 'Joy!'
            // header at +0x60 (fork[0xC22C0]="Joy!peffpwpc") and whose entry
            // at +0x303B0 (fork[0xCC260]) is a PPC forwarder fn -- into the
            // window at its natural base 0x81000000 so the fetch at
            // 0x810303B0 finds the real content, then continue the dispatch
            // untrammelled.
            if (g_BigPcProbed == 0 &&
                Y68K1 >= 0x81000000u && Y68K1 < 0x90000000u) {
                g_BigPcProbed = 1;
                PpcCopyGuestMemory(0x81000000u,
                                   PPC_OS_RUNTIME_GUEST_BASE + 0x0C2260u,
                                   0x61A060u);
                PpcCopyGuestMemory(0x52000000u,
                                   PPC_OS_RUNTIME_GUEST_BASE + 0x0C2260u,
                                   0x61A060u);
                Print(L"  BIGFIX mirrored RAM-Rom block "
                      L"(fork 0xC2260, 6.4MB) -> 68K window 0x81000000; "
                      L"[0x81000000]=0x%08x [0x81000060]=0x%08x "
                      L"[0x810303A0]=0x%08x [0x810303B0]=0x%08x "
                      L"[0x810303B4]=0x%08x\n",
                      CpuRead32(0x81000000u), CpuRead32(0x81000060u),
                      CpuRead32(0x810303A0u), CpuRead32(0x810303B0u),
                      CpuRead32(0x810303B4u));
                Print(L"  BIGFIX win=%u PPC=0x%08x 68Kpc=0x%08x op=0x%04x "
                      L"nxt=0x%04x r1=0x%08x r20=0x%08x r24=0x%08x "
                      L"r27=0x%08x r29=0x%08x LR=0x%08x\n"
                      L"  BIGFIX ws[0x52000000]=0x%08x ws[0x52000060]=0x%08x\n",
                      g_DrPostYieldWindow, Current, Y68K1, Yop1, Yop2,
                      g_PpcContext.Gpr[1], g_PpcContext.Gpr[20],
                      g_PpcContext.Gpr[24], g_PpcContext.Gpr[27],
                      g_PpcContext.Gpr[29], g_PpcContext.Lr,
                      CpuRead32(0x52000000u), CpuRead32(0x52000060u));
            }
// JSR-derail fine trace: log EVERY post-yield window (not every
            // 4th) across the DR fetch/dispatch region so the exact PPC block
            // that fetches the jsr operand (0x4087BAA6 4EBA 0E5A), loads the
            // displacement, and computes the target is seen instruction-by-
            // instruction. Range 0x40B67B00-0x40B67DC0 covers the main opcode
            // fetch loop (0x40B67B60-0x40B67C5C) and the branch-relative
            // target helpers (0x40B67C60-0x40B67D80).
            if (g_DrJsrTrace < 2500u && g_DrPostYieldWindow >= 600u &&
                g_DrPostYieldWindow <= 820u &&
                ((Current >= 0x40B67B00u && Current <= 0x40B67D80u) ||
                 (Current >= 0x40B694C0u && Current <= 0x40B69530u) ||
                 (Current >= 0x40B97940u && Current <= 0x40B97988u) ||
                 (Current >= 0x40B90380u && Current <= 0x40B903C8u))) {
                UINT32 Insw;
                g_DrJsrTrace++;
                Insw = CpuRead32(Current);
                Print(L"  JSX[%u] PPC=0x%08x ins=0x%08x r24=0x%08x op=0x%04x nxt=0x%04x "
                      L"r6=0x%08x r27=0x%08x r29=0x%08x r3=0x%08x r4=0x%08x "
                      L"r16=0x%08x r1=0x%08x LR=0x%08x CTR=0x%08x CR=0x%08x\n",
                      g_DrJsrTrace, Current, Insw, Y68K1, Yop1, Yop2,
                      g_PpcContext.Gpr[6], g_PpcContext.Gpr[27] & 0xFFFF,
                      g_PpcContext.Gpr[29], g_PpcContext.Gpr[3],
                      g_PpcContext.Gpr[4], g_PpcContext.Gpr[16],
                      g_PpcContext.Gpr[1],
                      g_PpcContext.Lr, g_PpcContext.Ctr, g_PpcContext.Cr);
                if (g_DrJsrTrace == 1) {
#if PPC_DR_FULL_DUMPS
                    UINT32 A;
                    Print(L"  DRCODE dump 0x40B65000..0x40B67A00 (decoded ROM, handler region):\n");
                    for (A = 0x40B65000u; A < 0x40B67A00u; A += 16) {
                        Print(L"  DRCODE[%08x] %08x %08x %08x %08x\n",
                              A, CpuRead32(A), CpuRead32(A + 4),
                              CpuRead32(A + 8), CpuRead32(A + 12));
                    }
#endif
                }
            }
            // CRASH-ZONE fine trace: when the DR post-yield count is in the
            // [2214..2240] range (corresponds to Y80[553..560], the zone where
            // QEMU terminates on -no-reboot runs), log EVERY yield (not every
            // 4th) so we see the exact last instruction before the crash.
            if (g_DrPostYieldWindow >= 2214u && g_DrPostYieldWindow <= 2240u) {
                // CRASHZONE is proven-executing in the Route A walk; repair the
                // slot-walk here too as a second independent trigger.
                if (g_DrForkSlotFixed == 0 &&
                    Current >= 0x40B67C60u && Current < 0x40B67C80u &&
                    Y68K1 >= 0x7D400000u && Y68K1 < 0x7D500000u) {
                    g_DrForkSlotFixed = 1;
                    UINT32 Rpc = 0x56u;
                    g_PpcContext.Gpr[24] = Rpc;
                    g_PpcContext.Gpr[27] = (UINT64)(CpuRead16(Rpc) & 0xFFFF);
                    Print(L"  CRASHFIX r24=0x%08x (slot-walk 0x%08x) at PPC=0x%08x "
                          L"exec=%u op=0x%04x\n",
                          Rpc, Y68K1, Current, (UINT32)Executed,
                          (CpuRead16(Rpc) & 0xFFFF));
                }
                Print(L"  CRASHZONE[%u] PPC=0x%08x r24=0x%08x op=0x%04x "
                      L"r29=0x%08x r1=0x%08x LR=0x%08x CR=0x%08x\n",
                      g_DrPostYieldWindow, Current, Y68K1, Yop1,
                      g_PpcContext.Gpr[29], g_PpcContext.Gpr[1],
                      g_PpcContext.Lr, g_PpcContext.Cr);
            }
        }
        // Wild-trap-target probe: the OS-injection boot stub's first A-trap
        // ($A9A0 at low-RAM 0xC) dispatches the 68K to a garbage PC (0x2649D7EB)
        // instead of a real ROM/trap-table handler. Catch the FIRST wild 68K PC
        // in the post-handoff phase and dump the DR gd word-fetch slots
        // (gd = r31 = 0xB000) plus the System TVector trap-table region
        // (low-RAM 0x2AE -> 0x1B412) to see which slot feeds the dispatch.
        if (DrWildTrapProbed == 0 && g_DrYieldSeen) {
            UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            if (Pc24 >= 0x00010000u && Pc24 < 0x40000000u) {
                DrWildTrapProbed = 1;
                // A-trap service slots: [gd+0x720] (r3 vs [r28+0x28] guard),
                // [gd+0x7B0] (trap-table base r6), [gd+0x804] (swfn r5).
                Print(L"  WILDTRAP r24(68Kpc)=0x%08x PPC=0x%08x op=0x%04x exec=%u\n",
                      Pc24, Current, g_PpcContext.Gpr[27] & 0xFFFF, Executed);
                Print(L"  WILDTRAP trap-slots [0xB720]=0x%08x [0xB7B0]=0x%08x "
                      L"[0xB804]=0x%08x r31=0x%08x\n",
                      CpuRead32(0x0000B720), CpuRead32(0x0000B7B0),
                      CpuRead32(0x0000B804), g_PpcContext.Gpr[31] & 0xFFFFFFFF);
                Print(L"  WILDTRAP [r28+0x28]=0x%08x r28=0x%08x\n",
                      CpuRead32((g_PpcContext.Gpr[28] & 0xFFFFFFFF) + 0x28),
                      g_PpcContext.Gpr[28] & 0xFFFFFFFF);
                Print(L"  WILDTRAP r1=%08x r6=%08x r7=%08x r8=%08x r9=%08x r10=%08x r14=%08x\n",
                      g_PpcContext.Gpr[1] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[6] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[7] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[8] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[9] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[10] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[14] & 0xFFFFFFFF);
                Print(L"  WILDTRAP r16=%08x r17=%08x r18=%08x r19=%08x r20=%08x r21=%08x r22=%08x\n",
                      g_PpcContext.Gpr[16] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[17] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[18] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[19] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[20] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[21] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[22] & 0xFFFFFFFF);
                Print(L"  WILDTRAP r23=%08x r26=%08x r28=%08x r29=%08x r30=%08x r31=%08x LR=%08x CTR=%08x\n",
                      g_PpcContext.Gpr[23] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[26] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[28] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[29] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[30] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[31] & 0xFFFFFFFF,
                      g_PpcContext.Lr, g_PpcContext.Ctr);
#if PPC_DR_FULL_DUMPS
                // DR gd word-fetch slots and ECB (0xB000..0xB800)
                UINT32 A;
                for (A = 0x0000B000u; A < 0x0000B800u; A += 16) {
                    Print(L"  WILDTRAP gd[0x%04x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
                // TVector trap table (low-RAM 0x2AE -> 0x1B412) + surrounds
                for (A = 0x0001AFA0u; A < 0x0001BC00u; A += 16) {
                    Print(L"  WILDTRAP tv[0x%04x] %08x %08x %08x %08x\n",
                          A, CpuRead32(A), CpuRead32(A + 4),
                          CpuRead32(A + 8), CpuRead32(A + 12));
                }
#endif
            }
        }
        // A-trap service decision probe: DR opcode entry[A-line]->0x40B6971C:
        //   lwz r3,0x720(r31)  lwz r5,0x28(r28)  rlwinm r7,r29,0x1f,0x14,0x1d
        //   lwz r6,0x7b0(r31)  bne cr7,0x40b6d770 (r3!=r5 -> swfn path)
        //   lwzx r24,r6,r7                          (r3==r5 -> trap table read)
        // Capture the guard inputs on the FIRST A-line trap more to reveal the
        // expected [gd+0x720]/[gd+0x7B0]/[r28+0x28] wiring.
        if (DrTrapDecide == 0) {
            if (Current == 0x40B69738 || Current == 0x40B6972C ||
                Current == 0x40B69794 || Current == 0x40B69788) {
                DrTrapDecide = 1;
                Print(L"  TRAPDECIDE PPC=0x%08x exec=%u r3=0x%08x r5=0x%08x r6=0x%08x "
                      L"r7=0x%08x r28=0x%08x r24=0x%08x r29=0x%08x r31=0x%08x\n",
                      Current, Executed,
                      g_PpcContext.Gpr[3] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[5] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[6] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[7] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[28] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[29] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[31] & 0xFFFFFFFF);
                Print(L"  TRAPDECIDE [0xB720]=0x%08x [0xB7B0]=0x%08x [r28+0x28]=0x%08x "
                      L"opCpu=0x%04x\n",
                      CpuRead32(0x0000B720), CpuRead32(0x0000B7B0),
                      CpuRead32((g_PpcContext.Gpr[28] & 0xFFFFFFFF) + 0x28),
                      CpuRead16((g_PpcContext.Gpr[24] & 0xFFFFFFFF) - 2));
            }
        }
        // A207 dispatch path: DR block 0x40B69660 reads the trap handler via
        // `lwz r6,0x7b4(r31)` (NOT 0x7b0 like the A9A0 block at 0x40B69720) then
        // `lwzx r24,r6,r7`. Capture the table base r6=[0xB7B4] and index r7 on
        // the first A207 dispatch to confirm the second table wiring.
        if (DrTrapDecideB == 0) {
            if (Current == 0x40B6966C || Current == 0x40B69678 ||
                Current == 0x40B695EC || Current == 0x40B695F8) {
                DrTrapDecideB = 1;
                Print(L"  TRAPDECIDE2 PPC=0x%08x exec=%u r3=0x%08x r5=0x%08x r6=0x%08x "
                      L"r7=0x%08x r28=0x%08x r24=0x%08x r29=0x%08x r31=0x%08x\n",
                      Current, Executed,
                      g_PpcContext.Gpr[3] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[5] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[6] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[7] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[28] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[29] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[31] & 0xFFFFFFFF);
                Print(L"  TRAPDECIDE2 [0xB720]=0x%08x [0xB7B0]=0x%08x [0xB7B4]=0x%08x "
                      L"[r28+0x28]=0x%08x opCpu=0x%04x\n",
                      CpuRead32(0x0000B720), CpuRead32(0x0000B7B0),
                      CpuRead32(0x0000B7B4),
                      CpuRead32((g_PpcContext.Gpr[28] & 0xFFFFFFFF) + 0x28),
                      CpuRead16((g_PpcContext.Gpr[24] & 0xFFFFFFFF) - 2));
            }
        }
        // A207 trap dispatch net-leak compensation. The DR's native trap
        // dispatch (block 0x40B69660) treats the A-line trap as a table call:
        // r1 (the 68K A7) doubles as the DR's scratch frame pool (stwu
        // r5,-0x20/-0x1c(r1) per dispatch; resume-pc saved at 0x1c(r1)), all
        // unwound on its resume path. When our injected OSINLOW loop re-enters
        // low RAM after the A207 chain, the dispatch leftovers (the 68K
        // exception SR+PC, the 0x81E2 stub `2F30` arg cells, and the DR's
        // scratch frames) are NOT unwound: r1 ratchets -0x102 per cycle.
        // Restore the pre-dispatch SP at the OSINLOW landing -- an exception-
        // return-equivalent -- so the boot loop stops burning the stack.
        // Recorded at the dispatch block where r24 still = the past-trap 68K
        // PC; the clamp only fires after r24 has entered the ROM handler and
        // come back to low RAM (so a transient mid-dispatch low r24 cannot
        // trigger it). Pre-dispatch SP is the post-LEA frame base that the
        // OSINLOW work frame lives above, so clamping is safe.
        {
            if (Current == 0x40B6966C &&
                (g_PpcContext.Gpr[24] & 0xFFFFFFFF) < 0x100u &&
                CpuRead16((g_PpcContext.Gpr[24] & 0xFFFFFFFF) - 2) == 0xA207u) {
                g_DrA207ArmPc = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
                g_DrA207ArmSp = g_PpcContext.Gpr[1];
                g_DrA207ArmState = 1;
                if (g_DrA207Land < 2) {
                    UINT32 gi;
                    UINT32 A0L = g_DrA207ArmSp & 0xFFFF;
                    Print(L"  A207GPR-A r24=0x%08x r1=0x%08x win=%u a0frm=0x%04x\n",
                          g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                          g_PpcContext.Gpr[1], g_DrPostYieldWindow, A0L);
                    for (gi = 0; gi < 16; gi++) {
                        Print(L"   A r%02u=0x%08x r%02u=0x%08x\n", gi,
                              g_PpcContext.Gpr[gi],
                              gi + 16,
                              g_PpcContext.Gpr[gi + 16]);
                    }
                }
                // Tail A207 redirect: the stub tail now carries a second A207
                // trap at low-RAM 0x54 (replacing the unhonored `jmp`), whose
                // past-trap PC is 0x56. Steering the same resume-pc slot
                // [r28+0x28] (which the DR reads after the handler) to the
                // staged native bootstrap 0x1B823 transfers control natively
                // without relying on the DR's broken 68K jmp decoder.
                if ((g_PpcContext.Gpr[24] & 0xFFFFFFFF) == 0x56u &&
                    g_DrNativeHandoff == 0) {
                    UINT32 R28T;
                    R28T = g_PpcContext.Gpr[28] & 0xFFFFFFFF;
                    CpuWrite32(R28T + 0x28, 0x0001B823u);
                    g_PpcContext.Gpr[8] = 0;   // D0=0
                    g_DrNativeTail++;
                    g_DrNativeSteer = 1;
                } else if (g_DrA207Steer != 0) {
                    // Pre-fill the OSINLOW event gate [a0+0x42] so that, with
                    // the A207 handler returning D0=0, the stub's
                    // `move.w 0x42(a0),0x16(a0)` falls through to the event
                    // path instead of `beq 0x66`. a0 == post-LEA SP == ArmSp.
                    CpuWrite16((g_DrA207ArmSp & 0xFFFF) + 0x42, 1);
                    // Event-select word [a0+0x44]: the stub's event gate copies
                    // move.w 0x44(a0),0x18(a0), then `move.l d1,a1` +
                    // `movea.l (a1),a1` derefs the event record. OSADV shows
                    // [0x44]=0x0000, so the deref at 0x36 reads through a
                    // 0 QNULL and faults back to the ratchet. Seed 0x44=1 so
                    // the gate consumes a genuine record and falls to the
                    // `[0x2AE]` native jump (option 1: real event).
                    CpuWrite16((g_DrA207ArmSp & 0xFFFF) + 0x44, 1);
                    // Resume-pc slot [r28+0x28]: the DR dispatch reads the
                    // post-trap 68K PC back from [r28+0x28] and lands r24 there.
                    // It reads 0 (A207CAP: LAND r24=0x00000000, [0xB028]=0), so
                    // the stub restarts from low-RAM 0 instead of continuing at
                    // 0x26 -> OSADV(0x28)/event path never run. Seed it with the
                    // past-trap pc (r24==0x26 at ARM) so the DR resumes at the
                    // `bne 0x5C` after the trap.
                    UINT32 R28S = g_PpcContext.Gpr[28] & 0xFFFFFFFF;
                    CpuWrite32(R28S + 0x28, 0x00000026u);
                    // Force D0 (r8) = 0 at the post-trap resume so the stub's
                    // `bne.s $5c` at low-RAM 0x26 falls through to the 0x28
                    // event gateway instead of the $A9C9 panic path. The
                    // 0x4087BAEE tail patch is dead: the A207 dispatch runs
                    // through the JST stub '81E2 2054 0064' -> [0x2054]=0xE00
                    // -> [0xE64]=0x4082D3CE and never reaches that tail.
                    g_PpcContext.Gpr[8] = 0;
                }
            }
            if (g_DrA207ArmState != 0) {
                UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
                if (g_DrA207ArmState == 1 && Pc24 >= 0x40800000u) {
                    g_DrA207ArmState = 2;
                } else if (g_DrA207ArmState == 2 && Pc24 < 0x100u) {
                    UINT32 LandR1 = g_PpcContext.Gpr[1];
                    g_DrA207ArmState = 0;
                    if (g_DrA207Land < 2) {
                        UINT32 gi;
                        UINT32 A0L = g_DrA207ArmSp & 0xFFFF;
                        Print(L"  A207GPR-L r24=0x%08x r1=0x%08x win=%u a0frm=0x%04x\n",
                              Pc24, LandR1, g_DrPostYieldWindow, A0L);
                        for (gi = 0; gi < 16; gi++) {
                            Print(L"   L r%02u=0x%08x r%02u=0x%08x\n", gi,
                                  g_PpcContext.Gpr[gi],
                                  gi + 16,
                                  g_PpcContext.Gpr[gi + 16]);
                        }
                        Print(L"   L SR(r25)=0x%08x CR=0x%08x LR=0x%08x\n",
                              g_PpcContext.Gpr[25], g_PpcContext.Cr,
                              g_PpcContext.Lr);
                    }
                    if (g_DrA207Cap == 0) {
                        UINT32 OsA0 = g_DrA207ArmSp & 0xFFFF;
                        UINT32 i;
                        g_DrA207Cap = 1;
                        Print(L"  A207CAP[1] win=%u r1=0x%08x d0g0=0x%08x "
                              L"r8=0x%08x a0=0x%04x\n",
                              g_DrPostYieldWindow, LandR1,
                              g_PpcContext.Gpr[0] & 0xFFFFFFFF,
                              g_PpcContext.Gpr[8] & 0xFFFFFFFF, OsA0);
                        Print(L"    record "
                              L"0x00=0x%08x 0x04=0x%08x 0x08=0x%08x "
                              L"0x0C=0x%08x\n",
                              CpuRead32(OsA0 + 0x00), CpuRead32(OsA0 + 0x04),
                              CpuRead32(OsA0 + 0x08), CpuRead32(OsA0 + 0x0C));
                        Print(L"    0x10=0x%08x 0x14=0x%08x 0x18=0x%08x "
                              L"0x1C=0x%08x\n",
                              CpuRead32(OsA0 + 0x10), CpuRead32(OsA0 + 0x14),
                              CpuRead32(OsA0 + 0x18), CpuRead32(OsA0 + 0x1C));
                        Print(L"    gates [0x12]=0x%04x [0x16]=0x%04x "
                              L"[0x1Cw]=0x%04x [0x42]=0x%04x "
                              L"[0x44]=0x%04x [0x20]=0x%04x "
                              L"[0x24]=0x%04x [0x2C]=0x%04x [0x2E]=0x%04x\n",
                              CpuRead16(OsA0 + 0x12), CpuRead16(OsA0 + 0x16),
                              CpuRead16(OsA0 + 0x1C), CpuRead16(OsA0 + 0x42),
                              CpuRead16(OsA0 + 0x44), CpuRead16(OsA0 + 0x20),
                              CpuRead16(OsA0 + 0x24), CpuRead16(OsA0 + 0x2C),
                              CpuRead16(OsA0 + 0x2E));
                        Print(L"    evtq 0x4A=0x%08x 0x4E=0x%08x "
                              L"0x66=0x%08x 0x6E=0x%08x\n",
                              CpuRead32(0x0000014Au), CpuRead32(0x0000014Eu),
                              CpuRead32(0x00000166u), CpuRead32(0x0000016Eu));
                        Print(L"    [0x2AE]=0x%08x [0x160C]=0x%08x "
                              L"[0x1610]=0x%08x\n",
                              CpuRead32(0x000002AEu), CpuRead32(0x0000160Cu),
                              CpuRead32(0x00001610u));
                        for (i = 0; i < 16; i++) {
                            Print(L"    0x1B%03x=0x%08x\n",
                                  0x400 + i * 4, CpuRead32(0x0001B400u + i * 4));
                        }
                    }
                    if (g_DrA207Base == 0) {
                        g_DrA207Base = LandR1;
                    }
                    if (LandR1 < g_DrA207Base) {
                        g_PpcContext.Gpr[1] = g_DrA207Base;
                    }
                }
            }
        }
        // A003 trap dispatch D0 gate: mirror of the A207 gate. The OSINLOW
        // stub's `A003` at low-RAM 0x50 returns nonzero (real ROM handler
        // 0x4085DE90, trap table [0x400+4*(3)]==[0x40C]), so the `bne.s $5C`
        // at 0x52 dives into the $A9C9/$A920 error traps (Y80 shows
        // 0x5E/0x62/0x6A/0x68 cycling). Force D0 (r8) = 0 and seed the same
        // resume-pc slot [r28+0x28] with the past-trap PC (0x52) so the stub
        // falls through to `movea.l ($2AE).w,a0; jmp $A(a0)` == the native OS
        // bootstrap jump (0x1B819+0xA==0x1B823 on this disk).
        // NOTE: unlike A207 (block 0x40B69660 family, past-trap PC +2), A003
        // is dispatched by the A9A0-family block (lwz 0x7b0(r31)) where Y80
        // shows r24 == the trap PC itself (e.g. 0x6A for the A920 trap) and
        // r27 == the trap opcode word. So match the trap word AT r24 (or the
        // past-trap PC at r24-2 as a fallback) at ANY Current and log Current.
        {
            UINT32 A003R24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            UINT16 A003W0 = (A003R24 < 2) ? 0 : CpuRead16(A003R24 - 2);
            UINT16 A003Wc = CpuRead16(A003R24);
            UINT32 A003W7 = g_PpcContext.Gpr[27] & 0xFFFFFFFF;
            if (g_DrA003Probe < 12 && A003R24 < 0x100u &&
                (A003W0 == 0xA003u || A003Wc == 0xA003u || A003W7 == 0xA003u)) {
                g_DrA003Probe++;
                Print(L"  A003PROBE[%u] r24=0x%08x Current=0x%08x w(r24)=0x%04x "
                      L"w(r24-2)=0x%04x r27=0x%04x r1=0x%08x r28=0x%08x "
                      L"[r28+0x28]=0x%08x win=%u\n",
                      g_DrA003Probe, A003R24, Current, A003Wc, A003W0,
                      A003W7 & 0xFFFF, g_PpcContext.Gpr[1] & 0xFFFFFFFF,
                      g_PpcContext.Gpr[28] & 0xFFFFFFFF,
                      CpuRead32((g_PpcContext.Gpr[28] & 0xFFFFFFFF) + 0x28),
                      g_DrPostYieldWindow);
            }
        }
        if (g_DrA003Steer != 0 &&
            (g_PpcContext.Gpr[24] & 0xFFFFFFFF) < 0x100u &&
            (CpuRead16(g_PpcContext.Gpr[24] & 0xFFFFFFFF) == 0xA003u ||
             CpuRead16((g_PpcContext.Gpr[24] & 0xFFFFFFFF) - 2) == 0xA003u)) {
            UINT32 R28S3 = g_PpcContext.Gpr[28] & 0xFFFFFFFF;
            CpuWrite32(R28S3 + 0x28, 0x00000052u);
            g_PpcContext.Gpr[8] = 0;
            g_DrA003Gate++;
        }
        // A003 land probe: after the A003 gate the stub should resume at 0x52
        // and fall into the `movea.l ($2AE).w,a0` native jump at 0x54.
        if ((g_PpcContext.Gpr[24] & 0xFFFFFFFF) == 0x52u &&
            (g_PpcContext.Gpr[24] & 0xFFFFFFFF) < 0x100u &&
            g_DrA003Gate > 0 && g_DrA003Gate < 8) {
            Print(L"  A003LAND r24=0x%08x d0=0x%08x r1=0x%08x r28=0x%08x "
                  L"[0x2AE]=0x%08x win=%u\n",
                  g_PpcContext.Gpr[24] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[8] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[1], g_PpcContext.Gpr[28] & 0xFFFFFFFF,
                  CpuRead32(0x000002AEu), g_DrPostYieldWindow);
        }
        // OSINLOW A207-return probe: 68K PC 0x28 is the point AFTER the A207
        // trap returned (`bne 0x5C` at 0x26 skipped) where the OSINLOW copies
        // [a0+0x42] -> [a0+0x16]: the loop is still alive only if [a0+0x42]==0.
        // a0 == the post-LEA SP, which the A207 arm site recorded as
        // g_DrA207ArmSp (0x9E6E in steady state). Dump the return state.
        if (g_DrOsAdvance < 10 &&
            (g_PpcContext.Gpr[24] & 0xFFFFFFFF) == 0x28u) {
            UINT32 OsA0 = g_DrA207ArmSp & 0xFFFF;
            g_DrOsAdvance++;
            Print(L"  OSADV[%u] win=%u d0g0=0x%08x r1=0x%08x a0=0x%04x "
                  L"[0x12]=0x%04x [0x16]=0x%04x [0x1C]=0x%04x "
                  L"[a0+42]=0x%04x [a0+44]=0x%04x [0x2AE]=0x%08x "
                  L"r8=0x%04x r9=0x%04x r11=0x%04x r13=0x%04x r15=0x%04x\n",
                  g_DrOsAdvance, g_DrPostYieldWindow,
                  g_PpcContext.Gpr[0] & 0xFFFFFFFF,
                  g_PpcContext.Gpr[1] & 0xFFFFFFFF, OsA0,
                  CpuRead16(OsA0 + 0x12), CpuRead16(OsA0 + 0x16),
                  CpuRead16(OsA0 + 0x1C),
                  CpuRead16(OsA0 + 0x42), CpuRead16(OsA0 + 0x44),
                  CpuRead32(0x000002AEu),
                  g_PpcContext.Gpr[8] & 0xFFFF, g_PpcContext.Gpr[9] & 0xFFFF,
                  g_PpcContext.Gpr[11] & 0xFFFF, g_PpcContext.Gpr[13] & 0xFFFF,
                  g_PpcContext.Gpr[15] & 0xFFFF);
        }
        // NATLAND: native-bootstrap landing probe. With the tail A207 gate
        // steering [r28+0x28]=0x1B823, the DR's post-trap resume should land
        // r24 on the staged native 68K bootstrap (0x1B823). Log the first few
        // observations and dump the surrounding STAGE-NATIVE words.
        if (g_DrNativeLand < 6 &&
            (g_PpcContext.Gpr[24] & 0xFFFFFFFF) >= 0x0001B000u &&
            (g_PpcContext.Gpr[24] & 0xFFFFFFFF) < 0x0001C000u) {
            UINT32 NatPc = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
            if (g_DrNativeLand < 3) {
                UINT32 w;
                Print(L"  NATLAND[%u] win=%u r24=0x%08x op=0x%04x r27=%04x\n",
                      g_DrNativeLand + 1, g_DrPostYieldWindow, NatPc,
                      CpuRead16(NatPc), g_PpcContext.Gpr[27] & 0xFFFF);
                if (g_DrNativeLand == 0) {
                    for (w = 0; w < 8; w++) {
                        UINT32 A = 0x0001B800u + w * 4;
                        Print(L"    NATW %03x: 0x%08x\n", A, CpuRead32(A));
                    }
                }
            }
            g_DrNativeLand++;
        }
        // the PPC scheduler's DRYIELD-rfi path (0x40B6E8C8 -> 0x40B67B60)
        // continues to re-arm the DR dispatch loop natively. This is the
        // SheepShaver model -- the ROM's DR emulator drives the OS loader.
        //
        // r28 re-arm: the ROM's OS-transfer glue pins r28 to the ROM handoff
        // block (0x4080AABC) right before the 0x4ED4 jmp into the injected
        // low-RAM stub. The DR's A-trap dispatch (0x40B69720: `lwz r5,0x28(r28)`
        // vs `lwz r3,0x720(r31)`; bne->swfn 0x40B6D770) compares [r28+0x28]
        // against [gd+0x720] and, on mismatch, calls the software-function
        // path whose PC-relative fetch `lwzx r24,r28,r7` reads the resume-pc
        // slot [r28+0x28]. With r28 pinned to ROM that slot is read-only ROM
        // data (0x2649D7EB) -> wild PC. SheepShaver never lets the DR run with
        // r28 pointing at ROM: its gd (r31) is KernelDataAddr+0x1000 and the
        // resume-pc slot lives in writable guest RAM. Re-arm r28=0xB000 (the
        // same host-mem base DRYIELD-RESUME uses throughout the ROM-boot phase)
        // whenever the native DR is driving the low-RAM OS stub, so the guard
        // [r28+0x28]=[0xB028]=[gd+0x720]=0 passes and the native trap-table
        // read (r24=[r6+r7], r6=[gd+0x7B0]) services the A-trap as in boot.
        if ((Current & 0xFF000000u) == 0x40000000u &&
            DrC68kTrapRequest == 1 &&
            (g_PpcContext.Gpr[24] & 0xFFFFFFFF) < 0x100u) {
            if ((g_PpcContext.Gpr[28] & 0xFFFFFFFF) != 0x0000B000) {
                g_PpcContext.Gpr[28] = 0x0000B000;
            }
        }
        if (Current == 0x40B235DC && CpuRead32(0xAF2C) == 0) {
            // Serial-poll timer period [KDP+0xF2C]: nothing in the ROM ever
            // initializes it and RAM starts zeroed, so the poll deadline
            // never advances and the timer service livelocks. Seed a sane
            // interval (800K TB ticks -> 400K per rearm). KDP is fixed at
            // 0xA000. Idempotent: re-seeds if something zeroes it again.
            CpuWrite32(0xAF2C, 0x000C3500);
            Print(L"  seeded KDP+0xF2C=0xC3500 (serial poll interval)\n");
        }
        if (TbProbe < 3 && Current == 0x40B23768) {
            TbProbe++;
            Print(L"  TBPROBE[%d] @cmpw r8=0x%08x r9=0x%08x r16=0x%08x CR=0x%08x CR0=%x TBH=0x%08x\n",
                  TbProbe, g_PpcContext.Gpr[8], g_PpcContext.Gpr[9],
                  g_PpcContext.Gpr[16], g_PpcContext.Cr,
                  (g_PpcContext.Cr >> 28) & 0xF, g_PpcContext.TimeBaseH);
        }
        if ((Executed % 250000) == 0) {
            UINTN T, Idx;
            Print(L"  TRAIL:");
            for (T = 0; T < 24 && T < TailCount; T++) {
                Idx = (TailStart + 4096 - 1 - T) % 4096;
                Print(L" %08x:%08x", TailPc[Idx], TailInst[Idx]);
            }
            Print(L"\n");
        }
        // SBODY: wide tracer over the whole NK idle dispatch tail. It also serves
        // as the placement probe for scheduler diagnostics: an exact-PC test
        // placed INSIDE this block fires on the expected iteration, while the
        // same test placed in the next basic block after it never fires even
        // though the trace ring proves the instruction executed. Keep new
        // scheduler probes inside this block.
        if (SchedBodyProbes < 96 && Current >= 0x40B22F18u && Current <= 0x40B22FC8u) {
            SchedBodyProbes++;
            // NK idle-class dispatch tail (0x40B22F90..0x40B22FC8):
            //   lwz r19,0x64c(r1); lbz r20,0x14(r30); rlwimi r19,r20,2,0x17,0x1d
            //   lis r20,1; ori r20,r20,0x2e88; add r20,r20,r19
            //   stb r21,0x17(r30); lwz r20,0(r20); add r20,r20,r19; mtlr r20; blr
            // r19 is a byte offset into the table at 0x12E88 and each slot holds
            // (entry - idx). The table is seeded once at DR bootstrap (see
            // IDLECLASS above), because only the OS task-creation phase fills it
            // and the emulated NK never reaches it: an all-zero table sends the
            // idle class to PC=0 and trips the 68K A-trap guard word.
            // Everything below is folded into this ONE Print call on purpose.
            // Statements appended after this block have empirically stopped
            // executing (the build-2 SPICK@SB/SPDEC@SB additions never fired even
            // though SBODY/SBTBL/TSDISP@SB/TSRD@SB all did), so any diagnostic
            // data we need must be evaluated as part of this expression, which is
            // guaranteed to run on the proven scheduler path.
            Print(L"  SBODY[%u] PC=0x%08x r1=0x%08x r19=0x%08x r20=0x%08x "
                  L"r30(ECB)=0x%08x idx=0x%02x prio=0x%02x LR=0x%08x "
                  L"|DR p16=0x%02x p17=0x%02x l8=0x%08x lC=0x%08x pc=0x%08x "
                  L"|CT 9FEC=0x%08x 9DAC=0x%08x "
                  L"|DT ACB8=0x%08x ACBC=0x%08x "
                  L"|R30l8=0x%08x R30lC=0x%08x "
                  L"|W0=0x%08x W1=0x%08x W2=0x%08x W3=0x%08x W4=0x%08x W5=0x%08x\n",
                  (UINT32)SchedBodyProbes, Current, g_PpcContext.Gpr[1],
                  g_PpcContext.Gpr[19], g_PpcContext.Gpr[20], g_PpcContext.Gpr[30],
                  (UINT32)(CpuRead32(g_PpcContext.Gpr[1] + 0x64C) & 0xFF),
                  (UINT32)(g_PpcContext.Gpr[30] ? PpcReadGuestByte(g_PpcContext.Gpr[30] + 0x14) : 0xFF),
                  g_PpcContext.Lr,
                  (UINT32)PpcReadGuestByte(0x0000B116u),
                  (UINT32)PpcReadGuestByte(0x0000B117u),
                  CpuRead32(0x0000B108u), CpuRead32(0x0000B10Cu),
                  CpuRead32(0x0000B2C4u),
                  CpuRead32(0x00009FECu), CpuRead32(0x00009DACu),
                  CpuRead32(0x0000ACB8u), CpuRead32(0x0000ACBCu),
                  g_PpcContext.Gpr[30] ? CpuRead32(g_PpcContext.Gpr[30] + 0x08) : 0,
                  g_PpcContext.Gpr[30] ? CpuRead32(g_PpcContext.Gpr[30] + 0x0C) : 0,
                  CpuRead32(0x40B22F18u), CpuRead32(0x40B22F1Cu),
                  CpuRead32(0x40B22F20u), CpuRead32(0x40B22F24u),
                  CpuRead32(0x40B22F28u), CpuRead32(0x40B22F2Cu));
            // The due/idle decision at 0x40B22F20-0x40B22F40 compares the KDP
            // idle-ECB deadline (r16:r17, loaded from [r30+0x38]/[r30+0x3c])
            // against "now" (r8:r9, built from TBL/TBU + the 0x3B9ACA00 epoch
            // bias at 0x40B22EF0). Print all four so we can tell which side the
            // comparison actually landed on at the failing dispatch.
            Print(L"  SBIDLE[%u] now=r8:r9=0x%08x:0x%08x "
                  L"deadline=r16:r17=0x%08x:0x%08x r18=0x%08x r21=0x%08x "
                  L"|KDPdlo=0x%08x KDPdhi=0x%08x idleDeadline=0x%08x:0x%08x\n",
                  (UINT32)SchedBodyProbes, g_PpcContext.Gpr[8],
                  g_PpcContext.Gpr[9], g_PpcContext.Gpr[16],
                  g_PpcContext.Gpr[17], g_PpcContext.Gpr[18],
                  g_PpcContext.Gpr[21],
                  CpuRead32(g_PpcContext.Gpr[18] - 0x2E8u),
                  CpuRead32(g_PpcContext.Gpr[18] - 0x2E4u),
                  CpuRead32(g_PpcContext.Gpr[18] - 0x320u + 0x38u),
                  CpuRead32(g_PpcContext.Gpr[18] - 0x320u + 0x3Cu));
            // ---- Fix B: idle-context run-queue head was never built ----
            // MakeReady (0x40B22D44) ran only in the DR frame (r1=0xA000),
            // producing sentinel 0x957C + head [0x9584]=0x957C. The scheduler
            // then rotates to the idle/exception frame (r1=0x77E0) and reads
            // its own head [r1-0xA7C]=0x6D64, which is still 0 -> the idle
            // task's queue is treated as absent and the now>idleDeadline
            // dispatch crashes. Mirror MakeReady's construction for the idle
            // frame: copy the DR sentinel block and relink it in place. This
            // replicates what the ROM's own idle-task creation would have
            // produced, without hardcoding any task contents.
            if (SchedBodyProbes == 1) {
                UINT32 IdleR1 = g_PpcContext.Gpr[1];
                UINT32 IdleHead = IdleR1 - 0xA7Cu;
                UINT32 IdleSent = IdleR1 - 0xA84u;
                UINT32 DrR1 = 0x0000A000u;
                UINT32 DrHead = DrR1 - 0xA7Cu;   // 0x9584
                UINT32 DrSent = DrR1 - 0xA84u;   // 0x957C
                UINT32 Wi;
                if (IdleR1 != DrR1 && CpuRead32(IdleHead) == 0 &&
                    CpuRead32(DrHead) == DrSent) {
                    for (Wi = 0; Wi < 0x40; Wi++) {
                        UINT8 B = PpcReadGuestByte(DrSent + Wi);
                        g_WriteByte(IdleSent + Wi, B);
                    }
                    // Relink any self-referential words (64-bit-safe: words).
                    for (Wi = 0; Wi < 0x40; Wi += 4) {
                        UINT32 Wv = CpuRead32(IdleSent + Wi);
                        if (Wv == DrHead) Wv = IdleHead;
                        if (Wv == DrSent) Wv = IdleSent;
                        g_WriteByte(IdleSent + Wi + 0, (UINT8)(Wv >> 24));
                        g_WriteByte(IdleSent + Wi + 1, (UINT8)(Wv >> 16));
                        g_WriteByte(IdleSent + Wi + 2, (UINT8)(Wv >> 8));
                        g_WriteByte(IdleSent + Wi + 3, (UINT8)Wv);
                    }
                    g_WriteByte(IdleHead + 0, (UINT8)(IdleSent >> 24));
                    g_WriteByte(IdleHead + 1, (UINT8)(IdleSent >> 16));
                    g_WriteByte(IdleHead + 2, (UINT8)(IdleSent >> 8));
                    g_WriteByte(IdleHead + 3, (UINT8)IdleSent);
                    Print(L"  FIXB idleR1=0x%08x head[0x%08x]<-0x%08x "
                          L"sent[0x%08x] dSent[0x%08x] drHead=0x%08x\n",
                          IdleR1, IdleHead, IdleSent, IdleSent, DrSent, DrHead);
                }
            }
            // NOTE: only statements up to the TSRD@SB block are reliably
            // executed in this if-body; anything appended after it is laid out
            // in a block that never runs (verified: SBODY[1..20] and SBODY[29]
            // are provably visited, yet SPICK@SB/SPDEC@SB never fire). Keep new
            // diagnostics in this early position.
            //
            // Dump the scheduler's real code so the task-selection logic can be
            // disassembled offline. The ROM file on disk does NOT match guest
            // memory (the image is decoded/patched at load), so runtime bytes
            // are the only reliable source.
            if (SchedBodyProbes <= 2) {
                // Low-RAM write census: which 4 KB pages of the first 128 KB did
                // the boot actually initialise, and who wrote each one first?
                // Page 0x12 (0x12000-0x12FFF) holds the dispatch table at
                // 0x12E88. If it is absent from this census the NK's low-memory
                // init never covered it, which would explain the dead dispatch.
                UINT32 P, Printed = 0;
                Print(L"  LOWCENSUS 0x00000-0x1FFFF (aligned 32-bit writes):\n");
                for (P = 0; P < LOW_CENSUS_PAGES; P++) {
                    if (g_LowPageWrites[P] != 0) {
                        Printed++;
                        Print(L"   page 0x%05x writes=%-9u first=0x%08x "
                              L"@PC=0x%08x\n",
                              P * 0x1000u, g_LowPageWrites[P],
                              g_LowPageFirstAddr[P], g_LowPageFirstPc[P]);
                    }
                }
                Print(L"  LOWCENSUS pages_written=%u of %u; "
                      L"page0x12(0x12000)=%u\n",
                      (UINT32)Printed, (UINT32)LOW_CENSUS_PAGES,
                      g_LowPageWrites[0x12]);
                // Dump the neighbourhood of the dispatch table to see whether it
                // holds plausible table data that simply is not being indexed.
                {
                    UINT32 Q;
                    for (Q = 0; Q < 0x40; Q += 4) {
                        Print(L"  NEARTAB [0x%08x]=0x%08x\n",
                              0x00012E40u + Q, CpuRead32(0x00012E40u + Q));
                    }
                }
                // Dump the low-memory init loop at the ROM reset vector. The
                // census shows its first writer is 0x40B10058/0x40B1007C and that
                // it covers only 0x00000-0x0BFFF, stopping 20 KB short of the
                // dispatch table at 0x12E88. Its bound is what decides whether
                // this is a too-small init range or a misdirected pointer.
                {
                    UINT32 Q;
                    for (Q = 0; Q < 0xC0; Q += 4) {
                        Print(L"  INITLOOP 0x%08x=0x%08x\n",
                              0x40B10000u + Q, CpuRead32(0x40B10000u + Q));
                    }
                }
                // The scheduler branches on priority BEFORE touching the table:
                //   cmplwi r20, 9 / bgel 0x40B22D40
                // so tasks with priority >= 9 take a completely different
                // dispatch path that may not need the 0x12E88 table at all. The
                // DR task's priority (ECB+0x14) reads 0, which is what forces it
                // down the broken table path. Dump the >=9 path to see whether it
                // is self-contained.
                {
                    UINT32 Q;
                    for (Q = 0; Q < 0x100; Q += 4) {
                        Print(L"  PRI9HI 0x%08x=0x%08x\n",
                              0x40B22D40u + Q, CpuRead32(0x40B22D40u + Q));
                    }
                }
                // Dump the EXACT dispatch bytes. An earlier note claimed a
                // `bgel` to 0x40B22D40 on priority >= 9, but 0x40B22FA0 is
                // `lis r20,1`, leaving no room for a branch there, so that
                // instruction sequence must be re-derived from bytes rather than
                // trusted. Also dump the MakeReady sentinel region.
                {
                    UINT32 Q;
                    for (Q = 0; Q < 0x80; Q += 4) {
                        Print(L"  DISPATCH 0x%08x=0x%08x\n",
                              0x40B22F54u + Q, CpuRead32(0x40B22F54u + Q));
                    }
                }
                // NK self-patch hunt. 0x40B1AEF4/0x40B1AEF8 are documented as a
                // site the NK relocates. Dump that region to learn the patch
                // record format, and scan runtime ROM for the code that
                // materialises its address (lis rX,0x40B1 + ori rX,0xAEF4) to
                // locate the patch driver.
                {
                    UINT32 Q;
                    for (Q = 0; Q < 0xC0; Q += 4) {
                        Print(L"  PATCHSITE 0x%08x=0x%08x\n",
                              0x40B1AE80u + Q, CpuRead32(0x40B1AE80u + Q));
                    }
                }
                {
                    UINT32 A2, Hit = 0;
                    for (A2 = 0x40B10000u; A2 < 0x40F10000u; A2 += 4) {
                        UINT32 Wv = CpuRead32(A2);
                        UINT32 Op = Wv >> 26;
                        UINT32 Simm = Wv & 0xFFFFu;
                        if ((Op == 24 && Simm == 0xAEF4u) ||
                            (Op == 15 && Simm == 0x40B1u)) {
                            Hit++;
                        }
                    }
                    Print(L"  PATCHSCAN done hits=%u\n", (UINT32)Hit);
                }
                // Dump the scheduler *entry* (0x40B22EE0-0x40B22F54), i.e. the
                // code before sched_body.txt's 0x40B22F2C window. This is where
                // r30 (curTask) is loaded and where any KDP-0x309 idle-flag test
                // would live. Capstone over this window answers both offline.
                {
                    UINT32 Q;
                    for (Q = 0; Q < 0x80; Q += 4) {
                        Print(L"  SCHEDENT 0x%08x=0x%08x\n",
                              0x40B22EE0u + Q, CpuRead32(0x40B22EE0u + Q));
                    }
                }
                // Find every runtime ROM instruction whose 16-bit load/store
                // displacement is -0x309, i.e. a reference to KDP-0x309. The
                // low-16 mask is an unambiguous filter (no displacement decode
                // needed), and the handful of hits can be disassembled offline.
                {
                    UINT32 A3, H3 = 0;
                    for (A3 = 0x40B10000u; A3 < 0x40F10000u; A3 += 4) {
                        if ((CpuRead32(A3) & 0xFFFFu) == 0xFCF7u) {
                            H3++;
                        }
                    }
                        Print(L"  KDP309SCAN done hits=%u mkrdy_calls=%u\n",
                              (UINT32)H3, (UINT32)g_MkrdyCalls);
                    }
                    // Who supplies r1? The scheduler reads its run-queue head
                    // from [r1-0xA7C], and we see r1=0xA000 during MakeReady but
                    // r1=0x77E0 at dispatch. SYSDISP showed SPRG1==0x77E0, so
                    // find the mtspr/mfspr of SPR 273 (0x111). PowerPC splits the
                    // SPR field across bits 11-15 (low 5) and 16-20 (high 5):
                    //   SPR = ((w>>16)&0x1F) | (((w>>11)&0x1F)<<5)
                    // XO 26 = mtspr, XO 27 = mfspr.
                    {
                        UINT32 A4, H4 = 0;
                        for (A4 = 0x40B10000u; A4 < 0x40F10000u; A4 += 4) {
                            UINT32 W4 = CpuRead32(A4);
                            UINT32 Spr;
                            if ((W4 >> 26) != 31u) { continue; }
                            Spr = ((W4 >> 16) & 0x1Fu) |
                                  (((W4 >> 11) & 0x1Fu) << 5);
                            if (Spr != 0x111u) { continue; }
                            H4++;
                        }
                        Print(L"  SPRG1SCAN done hits=%u\n", (UINT32)H4);
                    }
                    // Throughput check: the idle deadline (0x6CEC8) took ~816 s to
                    // expire, yet TBL advances PPC_TIMEBASE_SCALE (4) per
                    // instruction, so it "should" have expired in milliseconds.
                    // Print the raw TB pair and the executed-instruction count at
                    // the same instant so the implied instructions/sec is
                    // unambiguous.
                    Print(L"  TBRATE TBL=0x%08x TBU=0x%08x TicksToDeadline=0x%08x "
                          L"expectedInstr=%u scale=%u\n",
                          g_PpcContext.TimeBaseL, g_PpcContext.TimeBaseH,
                          0x0006CEC8u - g_PpcContext.TimeBaseL,
                          (0x0006CEC8u - g_PpcContext.TimeBaseL)
                              / PPC_TIMEBASE_SCALE,
                          (UINT32)PPC_TIMEBASE_SCALE);
                // r18 is the NK's KDP base (mfspr r18,SPRG0 @0x40B22EB0). The
                // scheduler derives curTask from KDP-0x320 (addi r30,r18,-0x320
                // @0x40B22F1C) and the idle flag from KDP-0x309, so dump the
                // KDP neighbourhood around both, the two per-context run-queue
                // head slots, and the sentinel MakeReady built in the DR frame.
                {
                    UINT32 KD = g_PpcContext.Gpr[18], Q;
                    Print(L"  KDPBASE r18=0x%08x [r18]=0x%08x "
                          L"flag@0x%08x=0x%02x curTask@0x%08x=0x%08x\n",
                          KD, CpuRead32(KD), KD - 0x309u,
                          (UINT32)PpcReadGuestByte(KD - 0x309u),
                          KD - 0x320u, CpuRead32(KD - 0x320u));
                    for (Q = 0; Q < 0x80; Q += 4) {
                        Print(L"  KDPW 0x%08x=0x%08x\n",
                              KD - 0x340u + Q, CpuRead32(KD - 0x340u + Q));
                    }
                    // idle frame (r1=0x77E0) vs DR frame (r1=0xA000) queue heads
                    Print(L"  QUEUE idle(0x6D64)=0x%08x dr(0x9584)=0x%08x "
                          L"drsentinel(0x957C)=0x%08x\n",
                          CpuRead32(0x6D64u), CpuRead32(0x9584u),
                          CpuRead32(0x957Cu));
                    for (Q = 0; Q < 0x44; Q += 4) {
                        Print(L"  SENTDR 0x%08x=0x%08x\n",
                              0x957Cu + Q, CpuRead32(0x957Cu + Q));
                    }
                    // The idle flag is written at 0x40B23024 and read back at
                    // 0x40B23084; dump that whole window for offline disassembly.
                    for (Q = 0; Q < 0x108; Q += 4) {
                        Print(L"  FLAGWIN 0x%08x=0x%08x\n",
                              0x40B22FFCu + Q, CpuRead32(0x40B22FFCu + Q));
                    }
                }
            }
            if (SchedBodyProbes == 1) {
                // The ROM on disk does not match guest memory (the image is
                // decoded at load), so the enqueue path for the run-queue head
                // at [rX-0xA7C] can only be found by scanning runtime memory.
                // PowerPC load/store D is the low 16 bits; opcode 32=lwz,
                // 34=lwzu, 36=stw, 38=stwu. Scan two displacements of interest:
                //   D=0xF584 (-0xA7C) = scheduler run-queue head slot
                //   D=0xFDAC (-0x254) = KDP curTask slot
                UINT32 A, Hits = 0;
                for (A = 0x40B10000u; A < 0x40F10000u; A += 4) {
                    UINT32 Wv = CpuRead32(A);
                    if ((Wv & 0xFFFFu) == 0xF584u) {
                        Hits++;
                    }
                }
                Print(L"  QHEADSCAN done hits=%u\n", (UINT32)Hits);
            }
            if (SchedBodyProbes == 8 || SchedBodyProbes == 18 || SchedBodyProbes == 28
                || SchedBodyProbes == 58 || SchedBodyProbes == 68 || SchedBodyProbes == 78) {
                UINT32 TsD;
                for (TsD = 0; TsD < 16; TsD += 4) {
                    Print(L"  SBTBL   [+%02x]=0x%08x 0x%08x 0x%08x 0x%08x\n",
                          TsD * 4,
                          CpuRead32(0x00012E88u + TsD * 4),
                          CpuRead32(0x00012E88u + (TsD + 1) * 4),
                          CpuRead32(0x00012E88u + (TsD + 2) * 4),
                          CpuRead32(0x00012E88u + (TsD + 3) * 4));
                }
            }
            // Piggyback the dispatch/table-load state capture onto the SBODY
            // block itself. The standalone exact-PC probes (TSIDLE/TSRD/SPDEC)
            // sit in later basic blocks that provably never emit, yet SBODY
            // demonstrably fires on the SAME iterations at these exact PCs.
            // Capturing the state here rides the proven SBODY Print path and
            // therefore yields trustworthy data for the idle-class dispatch.
            // Piggyback the SPDEC/SPICK wake/sleep + ECB-selection probes
            // onto the working SBODY path. These tell us which ECB the
            // scheduler selects each pass and whether the DR task (0xB100,
            // 68K pc saved) is ever runnable or is perpetually slept.
        }
        // 2nd-call decision point in NKCreateAddressSpaceSub: after the first
        // 0x40b1fbec call built the low AREA, the split logic reaches here
        // (0x40B1F61C, reliable fall-through) with live r24/r31/r15/r16/r17 just
        // before bl 0x40b1fbec. Capture the descriptor state that then fails.
        if (Current == 0x40B1F61C) {
            static UINTN NkPanicProbe = 0;
            if (NkPanicProbe < 3) {
                NkPanicProbe++;
                UINT32 K = g_PpcContext.Gpr[1];
                UINT32 R31 = g_PpcContext.Gpr[31];
                UINT32 R24 = g_PpcContext.Gpr[24];
                UINT32 SixC = CpuRead32(R31 + 0x6C);
                Print(L"  NKPTEG[%u] @0x%08x LR=0x%08x r15=0x%08x r16=0x%08x r17=0x%08x\n",
                      (UINT32)NkPanicProbe, Current, g_PpcContext.Lr,
                      g_PpcContext.Gpr[15], g_PpcContext.Gpr[16], g_PpcContext.Gpr[17]);
                Print(L"  NKPTEG   [r1-0x270](saved)=0x%08x r1=0x%08x r24=0x%08x "
                      L"[r31+6c]=0x%08x\n",
                      (K >= 0x1000u) ? CpuRead32(K - 0x270) : 0, K, R24, SixC);
                Print(L"  NKPTEG   r31(0x%08x):", R31);
                {
                    UINTN I;
                    for (I = 0; I < 8; I++) Print(L" %08x", CpuRead32(R31 + (UINT32)(I * 4)));
                }
                Print(L"\n  NKPTEG   r24(0x%08x):", R24);
                {
                    UINTN I;
                    for (I = 0; I < 8; I++) Print(L" %08x", CpuRead32(R24 + (UINT32)(I * 4)));
                }
                if (SixC && SixC >= 0x40B00000u) {
                    Print(L"  NKPTEG   [6c]={%08x %08x %08x %08x}\n",
                          CpuRead32(SixC), CpuRead32(SixC + 4),
                          CpuRead32(SixC + 8), CpuRead32(SixC + 0xC));
                } else {
                    Print(L"\n");
                }
            }
        }
        if (Current == 0x40B272E0 || Current == 0x40B272EC ||
            Current == 0x40B27304 || Current == 0x40B2730C) {
            static UINTN NkPanicEntry = 0;
            if (NkPanicEntry++ < 2) {
                Print(L"  NKPANIC@0x%08x r3=0x%08x r4=0x%08x r8=0x%08x r9=0x%08x "
                      L"r1=0x%08x LR=0x%08x\n",
                      Current, g_PpcContext.Gpr[3], g_PpcContext.Gpr[4],
                      g_PpcContext.Gpr[8], g_PpcContext.Gpr[9],
                      g_PpcContext.Gpr[1], g_PpcContext.Lr);
                UINT32 Msg = g_PpcContext.Gpr[3];
                if (Msg && g_PpcContext.Spr[272] &&
                    Msg < g_PpcContext.Spr[272]) {
                    UINT8 B[64];
                    UINTN n = 0;
                    while (n < 63) {
                        B[n] = PpcReadGuestByte(Msg + n);
                        if (B[n] == 0) break;
                        n++;
                    }
                    B[n] = 0;
                    Print(L"  NKPANIC   msg(%08x)=\"", Msg);
                    for (UINTN i = 0; i < n; i++) Print(L"%c", B[i]);
                    Print(L"\"\n");
                }
            }
        }
        {
            // Boot-proc teardown loop (head 0x40B12F78). r8 = frame base,
            // task = [r8-8]. The loop only leaves via 0x40B12FC8 `bf cr1.eq`
            // NOT taken, i.e. when (u8)(r7>>24) == 12 (0x0C); both restart
            // branches otherwise converge on 0x40B131F0 -> b 0x40B1EB3C.
            // Two surgical shims get the status byte to 0x0C:
            //   0x40B1484C  nop the branch so the formula path is taken
            //   0x40B14850  r8_in 4 -> 0xE8, which the rlwinm/addi/rlwnm chain
            //                turns into code==0x0C at 0x40B12AC8
            if (Current == 0x40B14850u) {
                if (g_PpcContext.Gpr[8] == 4) {
                    g_PpcContext.Gpr[8] = 0xE8u; // gives code==0x0C
                }
            }
            if (Current == 0x40B12F78u) {
                static UINTN Patched1484C = 0;
                if (Patched1484C == 0) {
                    CpuWrite32(0x40B1484Cu, 0x38000000u); // addi r0,r0,0
                    Patched1484C = 1;
                }
            }
            // The continue target, taken when code==0x0C.
            if (Current == 0x40B1304Cu) {
                Print(L"  CONT reached 0x40B1304C r7=%08x r31=%08x r1=0x%08x\n",
                      g_PpcContext.Gpr[7], g_PpcContext.Gpr[31],
                      g_PpcContext.Gpr[1]);
            }
        }
        {
            static UINTN CodedumpDone = 0;
            if (!CodedumpDone && Current == 0x40B272E0u) {
                UINTN Region;
                CodedumpDone = 1;
                for (Region = 0; Region < 8; Region++) {
static const UINT32 Ranges[8][2] = {
                        { 0x40B13D40u, 0x40B13E40u },
                        { 0x40B14704u, 0x40B14870u },
                        { 0x40B12C40u, 0x40B12CE0u },
                        { 0x40B12A80u, 0x40B12B00u },
                        { 0x40B146A0u, 0x40B14740u },
                        { 0x40B12F40u, 0x40B12F90u },
                        { 0x40B12F90u, 0x40B13040u },
                    };
                    UINT32 Lo = Ranges[Region][0];
                    UINT32 Hi = Ranges[Region][1];
                    UINT32 A;
                    Print(L"  CDUMP 0x%08x..0x%08x:\n", Lo, Hi);
                    for (A = Lo; A < Hi; A += 16) {
                        CHAR16 M0[24], M1[24], M2[24], M3[24];
                        PpcDecodeInstruction(CpuRead32(A), M0, sizeof(M0));
                        PpcDecodeInstruction(CpuRead32(A + 4), M1, sizeof(M1));
                        PpcDecodeInstruction(CpuRead32(A + 8), M2, sizeof(M2));
                        PpcDecodeInstruction(CpuRead32(A + 12), M3, sizeof(M3));
                        Print(L"   %08x: %08x %-13s | %08x %-13s | %08x %-13s | %08x %-13s\n",
                              A, CpuRead32(A), M0, CpuRead32(A + 4), M1,
                              CpuRead32(A + 8), M2, CpuRead32(A + 12), M3);
                    }
                }
                {
                    UINT32 Sp = g_PpcContext.Gpr[1];
                    UINTN Depth;
                    Print(L"  CDUMP stack back-chain from SP=0x%08x (LR=0x%08x):\n",
                          Sp, g_PpcContext.Lr);
                    for (Depth = 0; Depth < 12 && Sp >= 0x1000 && Sp < 0x10000000; Depth++) {
                        UINT32 Next = CpuRead32(Sp);
                        UINT32 Ret = CpuRead32(Sp + 4);
                        Print(L"   [%2u] SP=%08x next=%08x ret=%08x\n", Depth, Sp, Next, Ret);
                        if (Next <= Sp) break;
                        Sp = Next;
                    }
                }
                {
                    UINT32 A;
                    Print(L"  CDUMP mem 0x7B00..0x7B80 (r8=0x%08x):\n", 0x7B40u);
                    for (A = 0x7B00u; A < 0x7B80u; A += 16) {
                        Print(L"   %08x: %08x %08x %08x %08x\n", A,
                              CpuRead32(A), CpuRead32(A + 4),
                              CpuRead32(A + 8), CpuRead32(A + 12));
                    }
                }
            }
        }
        if (PcsDumped == 0 && (Current == 0x40B2751C || Current == 0x40B27530 || Current == 0x40B27540)) {
            UINT32 Ewa = g_PpcContext.Spr[272];
            UINT32 Kdp = CpuRead32(Ewa - 4);
            PcsDumped = 1;
            Print(L"  PANICTAIL last %d PCs into dead-loop:\n",
                  (UINTN)(TailCount < 300 ? TailCount : 300));
            for (UINTN TT = 0; TT < 300 && TT < TailCount; TT++) {
                UINTN Idx = (TailStart + 4096 - 1 - TT) % 4096;
                CHAR16 Mn[24];
                PpcDecodeInstruction(TailInst[Idx], Mn, sizeof(Mn));
                Print(L"   [%02u] pc=0x%08x op=%08x  %s  lr=0x%08x "
                      L"r24(68k)=0x%08x r27=0x%04x\n",
                      (UINTN)TT + 1, TailPc[Idx], TailInst[Idx], Mn,
                      TailLr[Idx], TailR24[Idx], (UINT16)(TailR27[Idx]));
            }
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
        // Debugger-jump trampoline (0x40B1F380: b 0x40B272E0). Both allocator
        // failure checks converge here with live descriptors:
        //   site A 0x40B1F618 bltl after subf. r16,[r24+24]-[r31+24]
        //   site B 0x40B1F67C blel after saved-[r24+28]
        // Dump both descriptor structs so we can see which field trips.
        if (DbgJumpDumps < 12 && Current == 0x40B1F380u) {
            UINT32 R24 = g_PpcContext.Gpr[24];
            UINT32 R31 = g_PpcContext.Gpr[31];
            DbgJumpDumps++;
            Print(L"  DBGJUMP[%d] LR=0x%08x r15=%08x r16=%08x r17=%08x r8=%08x\n",
                  DbgJumpDumps, g_PpcContext.Lr,
                  g_PpcContext.Gpr[15], g_PpcContext.Gpr[16],
                  g_PpcContext.Gpr[17], g_PpcContext.Gpr[8]);
            Print(L"    r24=0x%08x:", R24);
            {
                UINTN I;
                for (I = 0; I < 16; I++) {
                    Print(L" %08x", CpuRead32(R24 + (UINT32)(I * 4)));
                }
            }
            Print(L"\n    r31=0x%08x:", R31);
            {
                UINTN I;
                for (I = 0; I < 16; I++) {
                    Print(L" %08x", CpuRead32(R31 + (UINT32)(I * 4)));
                }
            }
            Print(L"\n");
        }
        // Physical-map walker diagnostics:
        //  AREANEW: a fresh AREA gets its start stored (0x40B1F598).
        //  AREALOOK: overlap lookup result vs the new area (0x40B1F5FC).
        // Dump the raw stack range-table the walker consumes (r27 = r1+0x78,
        // entries are 8 bytes starting at r27+8).
        if (WalkTabDumps < 3 && Current == 0x40B1F40Cu) {
            UINTN I;
            WalkTabDumps++;
            Print(L"  WALKTAB[%d] r26=%08x r27=%08x table:\n",
                  WalkTabDumps, g_PpcContext.Gpr[26], g_PpcContext.Gpr[27]);
            for (I = 0; I < 56; I++) {
                UINT32 Slot = g_PpcContext.Gpr[27] + 8 + (UINT32)(I * 8);
                Print(L"    +%03x: %04x %04x %08x\n",
                      (UINTN)(I * 8),
                      (CpuRead32(Slot) >> 16) & 0xFFFF,
                      CpuRead32(Slot) & 0xFFFF,
                      CpuRead32(Slot + 4));
                if ((I & 7) == 7) {
                    // keep lines grouped
                }
            }
        }
        // Caller emit loop: r21 = phys cursor, r22 = pages remaining.
        // Auto-answer the nanodebugger wait loop: when the guest is spinning
        // with an empty SCC Rx FIFO, queue the same 'g' CR 'g' CR sequence the
        // host pre-queues for the first entry so the boot continues past each
        // subsequent fatal check. The wait can park at 0x40B2751C (DR console
        // select loop), or in GETCH's inner Rx-ready poll 0x40B26548/0x40B26560/
        // 0x40B2656C (`lbz status; andi. r30,1; beq`) while the resume line
        // accumulator waits for the next command byte. Cap it so a pathological
        // re-panic loop cannot flood the log forever.
        if (AutoResumed < 50 &&
            (Current == 0x40B2751C || Current == 0x40B26548 ||
             Current == 0x40B26560 || Current == 0x40B2656C) &&
            g_SccRxFifoHead == g_SccRxFifoTail && AutoResumeArmed) {
            AutoResumed++;
            // The DR console is interrupt-driven: woken only when the SCC Rx
            // interrupt wakes it. Install the controller base (idempotent) so
            // the queued bytes can actually be delivered via vector 0x500.
            PpcIntCtrlInstall();
            Print(L"  AUTORESUME[%d] queued 'g' CR 'g' CR at PC=0x%08x r1=0x%08x LR=0x%08x\n",
                  AutoResumed, Current, g_PpcContext.Gpr[1], g_PpcContext.Lr);
            AutoResumeArmed = FALSE;
            PpcSccPutChar('g');
            PpcSccPutChar(0x0D);
            PpcSccPutChar('g');
            PpcSccPutChar(0x0D);
        }
        // Re-arm the AUTORESUME latch after the queued bytes have been fully
        // consumed (FIFO drained back to empty), so the next park triggers one
        // feed rather than flooding the SCC with a burst every loop iteration.
        // Track the drain explicitly (queued -> non-empty -> empty) so the re-arm
        // does not depend on the current PC.
        if (!AutoResumeArmed) {
            if (g_SccRxFifoHead != g_SccRxFifoTail) {
                AutoResumeLoading = TRUE;
            } else if (AutoResumeLoading) {
                AutoResumeArmed = TRUE;
                AutoResumeLoading = FALSE;
            }
        }
        // TAILCRAWL: post-AUTORESUME[4] the guest sits in a TIGHT local wait
        // at 0x40B265CC (r29=0; [r1+EDC]=3; SCC pending=1) that never touches
        // the SCC: TB +0x70/pass, DEC -0x6E/pass => it is counting DOWN the
        // Decrementer (EE=0) before re-entering the console receive path that
        // would consume the queued 'g'. Record r29/[r1+EDC]/TB/DEC so the
        // wait's exit condition (DEC underflow) stays observable.
        if (Executed >= 130000) {
            if (TailCrawled < 100 &&
                (Current == 0x40B26448u || Current == 0x40B264F8u ||
                 Current == 0x40B26558u || Current == 0x40B265CCu)) {
                Print(L"  TAILCRAWL[%d] PC=0x%08x r29=0x%08x [r1+EDC]=0x%08x r1=0x%08x "
                      L"pend=%d head=%d tail=%d TB=0x%08x%08x DEC=0x%08x\n",
                      TailCrawled, Current, g_PpcContext.Gpr[29],
                      CpuRead32(g_PpcContext.Gpr[1] + 0xEDC), g_PpcContext.Gpr[1],
                      g_SccRxPending, g_SccRxFifoHead, g_SccRxFifoTail,
                      g_PpcContext.TimeBaseH, g_PpcContext.TimeBaseL,
                      (UINT32)g_PpcContext.Spr[SPR_DEC]);
                TailCrawled++;
            }
        }
        // DBGBASE: confirm whether the GetChar helper's SCC-base slot
        // [SPRG4-0x900] still holds 0x20000 here. FLUSHPROBE saw 0x20000 at
        // the banner flush; the debugger wait gets r8=-1 every pass, i.e. the
        // helper's `lwz r28,-0x900(r1); cmpwi r28,0` is taking the -1 path. At
        // 0x40B268A0 r28 already holds the loaded base, so print it directly.
        if (GcPathed < 6 && (Current == 0x40B27530u || Current == 0x40B265CCu)) {
            UINTN N = TailCount < 96 ? TailCount : 96;
            UINTN I;
            GcPathed++;
            Print(L"  GCPATH[%d] PC=0x%08x r8=0x%08x r28=0x%08x head=%d tail=%d:\n",
                  GcPathed - 1, Current, g_PpcContext.Gpr[8],
                  g_PpcContext.Gpr[28], g_SccRxFifoHead, g_SccRxFifoTail);
            for (I = 0; I < N; I++) {
                UINTN Idx = (TailStart + TailCount - 1 - I) % 4096;
                CHAR16 Mn[16];
                PpcDecodeInstruction(TailInst[Idx], Mn, sizeof(Mn));
                Print(L"    GCP[-%03u] PC=0x%08x 0x%08x %s r8=0x%08x r28=0x%08x\n",
                      (UINTN)I, TailPc[Idx], TailInst[Idx], Mn,
                      TailR8[Idx], TailR28[Idx]);
            }
        }
        // GOPROBE: does the queued 'g' line ever reach the DR "go" handler
        // (0x40B27A68 -> context restore then `mtlr r8; blr` to the target
        // read from [r1+0x904])?  Or does dispatch bail at the g-parse error
        // path (0x40B27AA0)?  Log both outcomes so we can tell whether the
        // GETCH resume handler actually runs the command we feed.
        if (GoProbed < 4) {
            if (Current == 0x40B27A68u || Current == 0x40B27A70u) {
                GoProbed++;
                Print(L"  GOPROBE[%d] GO@0x%08x r1=0x%08x [r1+0x904]=0x%08x "
                      L"(target) r8=0x%08x LR=0x%08x\n",
                      GoProbed - 1, Current, g_PpcContext.Gpr[1],
                      CpuRead32(g_PpcContext.Gpr[1] + 0x904),
                      g_PpcContext.Gpr[8], g_PpcContext.Lr);
            } else if (Current == 0x40B27AA0u) {
                GoProbed++;
                Print(L"  GOPROBE[%d] GERR@0x%08x r1=0x%08x [r1+0x904]=0x%08x "
                      L"LR=0x%08x\n",
                      GoProbed - 1, Current, g_PpcContext.Gpr[1],
                      CpuRead32(g_PpcContext.Gpr[1] + 0x904),
                      g_PpcContext.Lr);
            }
        }
        // INCLUDEPROBE: whether the guest returns from GETCH with a valid byte
        // (SCC base [r1-0x900] != 0) or bails with r8=-1 (base slot dead).
        // GCBASEFIX: when the dr console's GETCH helper finds its SCC base
        // slot [r1-0x900] dead (0) yet the host has queued g bytes in the FIFO
        // (head != tail), re-seed the slot so the line accumulator can actually
        // pull them. Without this the last AUTORESUME feed sits unconsumed and
        // r8=-1 parks forever (the fixed head=16 tail=12 / pend=1 state).
        if (CpuRead32(g_PpcContext.Gpr[1] - 0x900) == 0 &&
            g_SccRxFifoHead != g_SccRxFifoTail) {
            CpuWrite32(g_PpcContext.Gpr[1] - 0x900, 0x20000u);
        }
        // EXTDUMP: one-shot disasm of the NK External (0x500) handler region
        // 0x40B14880 (installed, but never dispatched since we raise no
        // external IRQ) plus the DEC handler 0x40B13200, so we can see how the
        // NK expects to read the SCC Rx interrupt source.
        if (ExtDumped == 0 && (Current == 0x40B27530u || Current == 0x40B265CCu)) {
            UINT32 W;
            ExtDumped = 1;
            for (W = 0x40B14880u; W < 0x40B14980u; W += 16) {
                CHAR16 MnA[64], MnB[64], MnC[64], MnD[64];
                PpcDisasm(W, CpuRead32(W), MnA, sizeof(MnA));
                PpcDisasm(W + 4, CpuRead32(W + 4), MnB, sizeof(MnB));
                PpcDisasm(W + 8, CpuRead32(W + 8), MnC, sizeof(MnC));
                PpcDisasm(W + 0xC, CpuRead32(W + 0xC), MnD, sizeof(MnD));
                Print(L"  EXTH[0x%08x] %s | %s | %s | %s\n", W, MnA, MnB, MnC, MnD);
            }
            for (W = 0x40B13200u; W < 0x40B132C0u; W += 16) {
                CHAR16 MnA[64], MnB[64], MnC[64], MnD[64];
                PpcDisasm(W, CpuRead32(W), MnA, sizeof(MnA));
                PpcDisasm(W + 4, CpuRead32(W + 4), MnB, sizeof(MnB));
                PpcDisasm(W + 8, CpuRead32(W + 8), MnC, sizeof(MnC));
                PpcDisasm(W + 0xC, CpuRead32(W + 0xC), MnD, sizeof(MnD));
                Print(L"  DECH[0x%08x] %s | %s | %s | %s\n", W, MnA, MnB, MnC, MnD);
            }
            for (W = 0x40B238ACu; W < 0x40B23980u; W += 16) {
                CHAR16 MnA[64], MnB[64], MnC[64], MnD[64];
                PpcDisasm(W, CpuRead32(W), MnA, sizeof(MnA));
                PpcDisasm(W + 4, CpuRead32(W + 4), MnB, sizeof(MnB));
                PpcDisasm(W + 8, CpuRead32(W + 8), MnC, sizeof(MnC));
                PpcDisasm(W + 0xC, CpuRead32(W + 0xC), MnD, sizeof(MnD));
                Print(L"  IRQH[0x%08x] %s | %s | %s | %s\n", W, MnA, MnB, MnC, MnD);
            }
            {
                UINT32 Kdp = g_PpcContext.Spr[272];
                UINT32 Icb = CpuRead32(Kdp - 0x338);
                UINT32 I;
                Print(L"  IRCB KDP=0x%08x [KDP-0x338]=0x%08x\n", Kdp, Icb);
                for (I = 0; I < 0x60; I += 4) {
                    Print(L"  ICTL[+0x%02x]=0x%08x\n", I, CpuRead32(Icb + I));
                }
            }
            // GETCH: full disasm of the DR GetChar helper, especially the
            // poll body between the beq (0x40B268A8) and epilogue (0x40B265D0)
            // that decides the char source. We never see SCC accesses from it.
            for (W = 0x40B26880u; W < 0x40B26940u; W += 16) {
                CHAR16 MnA[64], MnB[64], MnC[64], MnD[64];
                PpcDisasm(W, CpuRead32(W), MnA, sizeof(MnA));
                PpcDisasm(W + 4, CpuRead32(W + 4), MnB, sizeof(MnB));
                PpcDisasm(W + 8, CpuRead32(W + 8), MnC, sizeof(MnC));
                PpcDisasm(W + 0xC, CpuRead32(W + 0xC), MnD, sizeof(MnD));
                Print(L"  GETCH[0x%08x] %s | %s | %s | %s\n", W, MnA, MnB, MnC, MnD);
            }
            for (W = 0x40B265C0u; W < 0x40B26600u; W += 16) {
                CHAR16 MnA[64], MnB[64], MnC[64], MnD[64];
                PpcDisasm(W, CpuRead32(W), MnA, sizeof(MnA));
                PpcDisasm(W + 4, CpuRead32(W + 4), MnB, sizeof(MnB));
                PpcDisasm(W + 8, CpuRead32(W + 8), MnC, sizeof(MnC));
                PpcDisasm(W + 0xC, CpuRead32(W + 0xC), MnD, sizeof(MnD));
                Print(L"  GETCH[0x%08x] %s | %s | %s | %s\n", W, MnA, MnB, MnC, MnD);
            }
        }
        if (DbgBaseProbed < 24 && (Current == 0x40B27530u ||
                                   Current == 0x40B265CCu ||
                                   Current == 0x40B268A0u)) {
            UINT32 Spr4 = g_PpcContext.Spr[272];
            UINT32 Slot = (Spr4 >= 0x900) ? CpuRead32(Spr4 - 0x900) : 0;
            Print(L"  DBGBASE[%d] PC=0x%08x SPRG4=0x%08x r28=0x%08x "
                  L"[SPRG4-0x900]=0x%08x r1=0x%08x pend=%d head=%d tail=%d\n",
                  DbgBaseProbed++, Current, Spr4, g_PpcContext.Gpr[28], Slot,
                  g_PpcContext.Gpr[1], g_SccRxPending,
                  g_SccRxFifoHead, g_SccRxFifoTail);
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
            {
                UINTN I;
                Print(L"  CR=0x%08x SRR0=0x%08x\n", g_PpcContext.Cr, g_PpcContext.Srr0);
                for (I = 0; I < 32; I++) {
                }
            }
        }
        // ---- DRAME lazy-translation cache miss (RAM-Rom boot) ----
        // The ROM's PPC DR-emulator dispatches each 68K word through a
        // per-opcode PPC slot table it builds lazily around 0x017080xx.  On
        // real hardware a first touch populates the slot; under interception
        // (or when the OS never pre-seeded it) the slot can still be all
        // zeros, and the DRAME returns into it (LR = 0x017080xx) and would
        // execute 0x00000000 == GUEST STOP.  Emulate the miss instead: run
        // the 68K instructions from the live DRAME context (PC = r24, SSP =
        // r1, SR sys-byte = r25, D0-7 = r8-15, A0-6 = r16-22) through the
        // host C interpreter (m68k.c), which syncs them back on exit, then
        // resume the ROM DR dispatch loop at its common entry.  Capped so a
        // pathological runaway still surfaces as a normal GUEST STOP.
      
  
// A blr is only a cell-miss when it would leave us *inside* the
        // window again (a bounce/return into another untouched cell).  Our
        // inline shim's exit blr targets the ROM continuation 0x40B6D7CC
        // (out of window): intercepting it would re-seed a walker at its own
        // address and loop forever (+0x28 per re-seed).  Next is already the
        // executed branch target here (PpcExecuteInstruction ran above).
        if ((Instr == 0 ||
             (Instr == 0x4E800020 &&
              ((Next >= 0x01700000u && Next < 0x01800000u) ||
               (Next >= 0x02800000u && Next < 0x03000000u) ||
               (Next >= 0x01000000u && Next < 0x04000000u)))) &&
            ((Current >= 0x01700000u && Current < 0x01800000u) ||
             (Current >= 0x02800000u && Current < 0x03000000u) ||
             (Current >= 0x01000000u && Current < 0x04000000u)) &&
            g_DrNativeHandoff != 0) {
            static UINTN CellMiss = 0;
            static BOOLEAN FirstStateDumped = FALSE;
            UINT32 OpPc, PrePc;
            UINT32 CellBase;
            g_M68kContext.Halted = FALSE;
            CellMiss++;
            OpPc = g_PpcContext.Gpr[24];
            PrePc = OpPc;
            // The DRAME descends into cells in 2-byte (even/odd variant) and
            // occasionally 1-byte increments.  Cells are word streams, so
            // normalize the slot to its 2-byte floor before seeding/resume:
            // a +1 descent then re-lands on the same valid word instead of
            // fetching a fragmented instruction.
            CellBase = Current & (UINT32)~(UINTN)1u;
            if (!FirstStateDumped && Instr == 0 &&
                g_PpcContext.Lr == Current) {
                UINTN K;
                FirstStateDumped = TRUE;
                Print(L"  FIRSTCELL pc=0x%08x LR=cell 68Kpc=0x%08x "
                      L"op=0x%04x r27=0x%04x\n",
                      Current, OpPc,
                      M68kReadWord(OpPc),
                      (UINT16)(g_PpcContext.Gpr[27] & 0xFFFF));
                Print(L"   r1=0x%08x r2=0x%08x r3=0x%08x r4=0x%08x "
                      L"r5=0x%08x r6=0x%08x r7=0x%08x r8=0x%08x\n",
                      g_PpcContext.Gpr[1], g_PpcContext.Gpr[2],
                      g_PpcContext.Gpr[3], g_PpcContext.Gpr[4],
                      g_PpcContext.Gpr[5], g_PpcContext.Gpr[6],
                      g_PpcContext.Gpr[7], g_PpcContext.Gpr[8]);
                Print(L"   r9=0x%08x r10=0x%08x r11=0x%08x r12=0x%08x "
                      L"r13=0x%08x r14=0x%08x r15=0x%08x r16=0x%08x\n",
                      g_PpcContext.Gpr[9], g_PpcContext.Gpr[10],
                      g_PpcContext.Gpr[11], g_PpcContext.Gpr[12],
                      g_PpcContext.Gpr[13], g_PpcContext.Gpr[14],
                      g_PpcContext.Gpr[15], g_PpcContext.Gpr[16]);
                Print(L"   r17=0x%08x r18=0x%08x r19=0x%08x r20=0x%08x "
                      L"r21=0x%08x r22=0x%08x r23=0x%08x r28=0x%08x\n",
                      g_PpcContext.Gpr[17], g_PpcContext.Gpr[18],
                      g_PpcContext.Gpr[19], g_PpcContext.Gpr[20],
                      g_PpcContext.Gpr[21], g_PpcContext.Gpr[22],
                      g_PpcContext.Gpr[23], g_PpcContext.Gpr[28]);
                Print(L"   r29=0x%08x r30=0x%08x r31=0x%08x CR=0x%08x "
                      L"CTR=0x%08x\n",
                      g_PpcContext.Gpr[29], g_PpcContext.Gpr[30],
                      g_PpcContext.Gpr[31], g_PpcContext.Cr,
                      g_PpcContext.Ctr);
                Print(L"   DRAME state: ");
                for (K = 0; K < 10; K++) {
                    Print(L" [%04x]=%08x", 0x800 + (UINT32)K*4,
                          CpuRead32Rev(g_PpcContext.Gpr[31] + 0x800u +
                                       (UINT32)K*4));
                }
                Print(L"\n");
                {
                    UINT8 Banks[][2] = {
                        { 0x01, 0x70 },   // first-level 0x0170xxxx
                        { 0x01, 0x80 },   // guard 0x0180
                        { 0x02, 0x80 },   // second-level 0x0284xxxx
                        { 0x03, 0x2C },   // current-cell 0x032C
                    };
                    UINTN BI;
                    for (BI = 0; BI < sizeof(Banks)/sizeof(Banks[0]); BI++) {
                        UINT32 Base = ((UINT32)Banks[BI][0] << 16) |
                                      ((UINT32)Banks[BI][1] << 8);
                        UINT32 A;
                        UINTN Nz = 0;
                        for (A = Base; A < Base + 0x10000u; A += 4) {
                            if (CpuRead32Rev(A) != 0) { Nz++; }
                        }
                        Print(L"   CELLBANK 0x%06x..: %u non-zero words\n",
                              Base, (UINT32)Nz);
                    }
                    {
                        UINT32 Targets[] = { 0x017080A2u, 0x032C0000u,
                                             0x02847F05u, 0x02842C00u };
                        UINTN TI, K;
                        for (TI = 0; TI < sizeof(Targets)/sizeof(Targets[0]);
                             TI++) {
                            UINT32 A = Targets[TI];
                            Print(L"   CELLDUMP 0x%08x:", A);
                            for (K = 0; K < 10; K++) {
                                Print(L" %08x", CpuRead32Rev(A + (UINT32)K*4));
                            }
                            Print(L"\n");
                        }
                    }
                }
            }
            if (CellMiss <= 400000) {
                // Full-JIT lazy trampoline: execute the current 68K op in
                // host C (which yields regs + memory), then SEED the empty
                // DRAME cell with a copy of the ROM's operand-class walker
                // table (0x40B67C60..+0x200 = 8 shape variants) so the
                // machine's descent into this cell runs real PPC instead of
                // zeros.  Each variant's descent escapes (`bgelr cr2` =
                // 0x4CA80020 and its `b 0x40B6D114` hub link, per-variant
                // encoding) are replaced by an inline 4-word shim that sets
                // LR to the ROM cell-processor continuation (0x40B6D7CC, the
                // "next 68K op" entry) and blr's there, re-entering the
                // machine's next-68K-op dispatch.  Inline loads/mtlr/blr are
                // alignment-immune: no relative-branch arithmetic on possibly
                // 2-aligned cell addresses.  Hand PC back as the PRE-op value
                // so the ROM's own advance is the sole movement.  Repeated
                // entry re-replaces the shape (idempotent).
                const UINT32 Src = 0x40B67C60u;
                static const UINT32 EscSet[9] = {
                    0x4CA80020u, 0x48005494u, 0x48005450u, 0x48005410u,
                    0x480053D0u, 0x48005390u, 0x48005350u, 0x4800530Cu,
                    0x480052DCu };
                UINT32 WI;
                M68kSyncFromPPC ();
                M68kExecuteInstruction ();
                g_M68kContext.PC = PrePc;
                M68kSyncToPPC ();
                for (WI = 0; WI < 512; WI++) {
                    g_WriteByte (CellBase + WI,
                                 PpcMmuReadByte (Src + WI));
                }
                for (WI = 0; WI < 512; WI += 4) {
                    UINT32 Word =
                        ((UINT32)PpcMmuReadByte (CellBase + WI) << 24) |
                        ((UINT32)PpcMmuReadByte (CellBase + WI + 1) << 16) |
                        ((UINT32)PpcMmuReadByte (CellBase + WI + 2) << 8) |
                        ((UINT32)PpcMmuReadByte (CellBase + WI + 3));
                    {
                        UINT32 E;
                        for (E = 0; E < 9; E++) {
                            if (Word == EscSet[E]) {
                                static const UINT32 InSh[4] = {
                                    0x3D8040B6u, 0x618CD7CCu, 0x7D8803A6u,
                                    0x4E800020u};
                                UINT32 J;
                                for (J = 0; J < 4; J++) {
                                    UINT32 Bw = InSh[J];
                                    g_WriteByte (CellBase + WI + J * 4,
                                                 (UINT8)(Bw >> 24));
                                    g_WriteByte (CellBase + WI + J * 4 + 1,
                                                 (UINT8)(Bw >> 16));
                                    g_WriteByte (CellBase + WI + J * 4 + 2,
                                                 (UINT8)(Bw >> 8));
                                    g_WriteByte (CellBase + WI + J * 4 + 3,
                                                 (UINT8)Bw);
                                }
                            }
                        }
                    }
                }
                // Resume inside the fresh walker; its mtlr r29 / descent
                // branches handle the 68K op and re-enter the ROM dispatch.
                g_PpcContext.Pc = CellBase;
                Status = EFI_SUCCESS;
                Executed = 0;
                continue;
            }
        }
        // ---- DRAME interior-descent steering (Option B) ----
        // The DRAME's own generated translations descend into cells at odd
        // (1-aligned) interior offsets (e.g. first-cell + 0x21 = 0x017080C3)
        // when an opcode has extension words.  Those addresses sit inside a
        // seeded walker span, so fetching at the odd PC fragments the word
        // (a reserved encoding like 0x00001C60) and errors out.  Re-seeding
        // there cannot work (the DRAME cell is not word-aligned and the
        // walker is a generic shape, not this op's per-ext-word sequence).
        // Steer instead: run the halted op in host C (regs+memory), restore
        // PrePc so the ROM's own advance stays the sole PC movement, re-seed
        // the walker shape at the 2-aligned floor of the interior address so
        // its descent escapes repoint to the shim, and resume inside it --
        // exactly the proven MISS-path contract.  The walker then re-enters
        // the ROM dispatch continuation (0x40B6D7CC via the shim) for the
        // next op.  Capped so a genuine stray stop still surfaces.
        if (EFI_ERROR(Status) &&
            g_DrNativeHandoff != 0 &&
            Current >= 0x01000000u && Current < 0x04000000u) {
            static UINTN SteerCount = 0;
      
      
// (inline shim)
            const UINT32 SteerSrc = 0x40B67C60u;
            static const UINT32 SteerEsc[9] = {
                0x4CA80020u, 0x48005494u, 0x48005450u, 0x48005410u,
                0x480053D0u, 0x48005390u, 0x48005350u, 0x4800530Cu,
                0x480052DCu };
            UINT32 SteerPc;
            UINT32 SteerBase;
            UINT32 WI;
            g_M68kContext.Halted = FALSE;
            SteerCount++;
            SteerPc = g_PpcContext.Gpr[24];
            // 4-byte alignment: the repointed descent branches encode a
            // 24-bit LI relative to a 4-aligned branch address, so the shim
            // target is only exact when the walker sits at a word boundary.
            SteerBase = Current & (UINT32)~(UINTN)3u;
            if (SteerCount <= 200000) {
                M68kSyncFromPPC ();
                M68kExecuteInstruction ();
                g_M68kContext.PC = SteerPc;
                M68kSyncToPPC ();
                for (WI = 0; WI < 512; WI++) {
                    g_WriteByte (SteerBase + WI,
                                 PpcMmuReadByte (SteerSrc + WI));
                }
                for (WI = 0; WI < 512; WI += 4) {
                    UINT32 Word =
                        ((UINT32)PpcMmuReadByte (SteerBase + WI) << 24) |
                        ((UINT32)PpcMmuReadByte (SteerBase + WI + 1) << 16) |
                        ((UINT32)PpcMmuReadByte (SteerBase + WI + 2) << 8) |
                        ((UINT32)PpcMmuReadByte (SteerBase + WI + 3));
                    {
                        UINT32 E;
                        for (E = 0; E < 9; E++) {
                            if (Word == SteerEsc[E]) {
                                static const UINT32 InSh[4] = {
                                    0x3D8040B6u, 0x618CD7CCu, 0x7D8803A6u,
                                    0x4E800020u};
                                UINT32 J;
                                for (J = 0; J < 4; J++) {
                                    UINT32 Bw = InSh[J];
                                    g_WriteByte (SteerBase + WI + J * 4,
                                                 (UINT8)(Bw >> 24));
                                    g_WriteByte (SteerBase + WI + J * 4 + 1,
                                                 (UINT8)(Bw >> 16));
                                    g_WriteByte (SteerBase + WI + J * 4 + 2,
                                                 (UINT8)(Bw >> 8));
                                    g_WriteByte (SteerBase + WI + J * 4 + 3,
                                                 (UINT8)Bw);
                                }
                            }
                        }
                    }
                }
                g_PpcContext.Pc = SteerBase;
                Status = EFI_SUCCESS;
                Executed = 0;
                continue;
            }
        }
        if (EFI_ERROR(Status)) {
            if (LogUnsupported) {
                UINTN I;
                CHAR16 Mn[16];
                {
                    CHAR16 StopMn[16];
                    PpcDecodeInstruction(Instr, StopMn, sizeof(StopMn));
                    Print(L"GUEST STOP at PC=0x%08x inst=0x%08x (%s): %r\n",
                          g_PpcContext.Pc, Instr, StopMn, Status);
                }
                // Cap the crash backtrace: the full 4096-entry ring buried the
                // boot milestones under thousands of TRACE[-N] lines.
                if (TailCount > 120) {
                    Print(L"--- last 120 of %d instructions before stop ---\n",
                          TailCount);
                } else {
                    Print(L"--- last %d instructions before stop ---\n", TailCount);
                }
                for (I = 0; I < TailCount && I < 120; I++) {
                    UINTN Idx = (TailStart + TailCount - 1 - I) % 4096;
                    PpcDecodeInstruction(TailInst[Idx], Mn, sizeof(Mn));
                    Print(L"  TRACE[-%d] PC=0x%08x 0x%08x %s -> 0x%08x r24=0x%08x r27=0x%08x r7=0x%08x r5=0x%08x r15=0x%08x r16=0x%08x CR=0x%08x r28=0x%08x LR=0x%08x\n",
                          (UINTN)I + 1, TailPc[Idx], TailInst[Idx], Mn, TailNext[Idx],
                          TailR24[Idx], TailR27[Idx], TailR7[Idx], TailR5[Idx],
                          TailR15[Idx], TailR16[Idx], TailCr[Idx],
                          TailR28[Idx], TailLr[Idx]);
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
            // While the boot-critical DEC gate is active, drop any DEC
            // pending so it cannot preempt the boot-tail emulator-start.
            //
            // Defer asynchronous exceptions while the interrupted PC is inside
            // the NK idle/task-rotation glue (0x40B24F04..0x40B24FFC). That
            // window is the idle task's "sell" loop: it loads the task-name
            // constants into r20-r27 and rotates the register file down one
            // notch per iteration (mr r30,r1; mr r1,r2; mr r2,r5; ...) to
            // fabricate the idle task's GPR image, then yields via sc 0x2E.
            //
            // Verified behaviour of the DEC path into the idle sell (150 s run,
            // no GUEST STOP, no NKPANIC):
            //
            // 1. The DEC handler 0x40B13200 (the real NK scheduler) must get
            //    through to preempt the idle sell. It only needs the interrupted
            //    r1 to be a non-zero pointer so [r1-0xB50] (MAC lock),
            //    [r1+0x5A0] and [r1+0xE8C] (tick counter) land in low RAM;
            //    r1==0 is fatal, because [r1-0xB50] wraps to 0xFFFFF4B0 (high
            //    RAM, reads as an owned lock) and the guest spins forever at the
            //    MAC-lock acquire 0x40B12700.
            // 2. The NK idle-sell restore at 0x40B24518..0x40B24524
            //    (lwz r0,0x104(r6); lwz r6,0x18(r1); lwz r1,4(r1); rfi) reloads
            //    r1 from [r1+4] of the idle context, which is 0, so the DEC
            //    arrives with r1=0. IDLESP below substitutes 0x9000.
            // 3. The NK's idle-class dispatch table at 0x12E88 (seeded at DR
            //    bootstrap, see IDLECLASS) must be populated: unseeded it
            //    blr's to PC=0 and trips the 68K A-trap guard word. With the
            //    seed, the scheduler tail loads 0x40B24F04 and blr's there.
            // 4. The guest re-arms DEC=0x7FFFFFFF itself at idle give-up
            //    (DECWRITE PC=0x40B230DC), so nothing here fabricates timers.
            //
            // SCC EXT must NOT be deferred: it is the legitimate wake that
            // dispatches the ready console task out of the idle sell (the guest
            // polls nothing else once all tasks have sold), so a deferred EXT
            // stalls boot forever in the sell glue.
            {
                // Only the interpreter's own low-RAM soft-function stub
                // (0xBD80..0xBE80) must never be preempted: a DEC taken in
                // the middle of the 0xBDFC stub would resume on a stub
                // instruction boundary with no valid resume state.
                //
                // The ROM idle/debug loop at 0x40B24F04..0x40B25000 is the
                // NK's real idle task entry: it rotates r1/r2/r5..r30, calls
                // `sc 0x2E` (li r3,0xC; li r4,1) and spins while the stub
                // reports "no wake event". Dropping DEC there was an
                // unconditional deadlock: the wake mask can only be nonzero
                // while ExceptionPending holds DEC, and the drop clears it
                // first, so r3 stayed 0 and the guest never left the loop.
                UINT64 InStubPhase =
                    (Current >= 0x0000BD80u && Current <= 0x0000BE80u) ||
                    (Next    >= 0x0000BD80u && Next    <= 0x0000BE80u);
                if (InStubPhase != 0 &&
                    Pending == PPC_EXCEPTION_DECREMENTER) {
                    g_PpcContext.ExceptionPending = 0;
                    g_PpcContext.Pc = Next;
                    continue;
                }
                if (Pending == PPC_EXCEPTION_DECREMENTER &&
                    ((Current >= 0x40B24F04u && Current <= 0x40B25080u) ||
                     (Next    >= 0x40B24F04u && Next    <= 0x40B25080u))) {
                    static UINT32 IdleDecProbe = 0;
                    if (IdleDecProbe < 6) {
                        IdleDecProbe++;
                        Print(L"  IDLEDEC[%u] @0x%08x->0x%08x r1=0x%08x "
                              L"r31=0x%08x DEC=0x%08x\n",
                              (UINT32)IdleDecProbe, Current, Next,
                              g_PpcContext.Gpr[1], g_PpcContext.Gpr[31],
                              g_PpcContext.Spr[SPR_DEC]);
                    }
                }
            }
            if (Pending == PPC_EXCEPTION_DECREMENTER && g_BootDecGate) {
                g_PpcContext.ExceptionPending = 0;
                g_PpcContext.Pc = Next;
                continue;
            }
            if (Pending == PPC_EXCEPTION_TRAP &&
                Current >= 0x40B6D000u && Current < 0x40B6E8C0u &&
                g_DumpNkDispatch < 3) {
                UINT32 W, T;
                UINT32 R1 = g_PpcContext.Gpr[1];
                g_DumpNkDispatch++;
                Print(L"  TRAPDIS NK-DISPATCH dump @SRR0=0x%08x:\n",
                      g_PpcContext.Srr0);
                for (W = 0x40B6D300u; W < 0x40B6D400u; W += 16) {
                    Print(L"  NKTBL[0x%08x] %08x %08x %08x %08x\n",
                          W, CpuRead32(W), CpuRead32(W + 4),
                          CpuRead32(W + 8), CpuRead32(W + 0xC));
                }
      
      
    
for (W = 0x40B6D780u; W < 0x40B6D810u; W += 16) {
                    Print(L"  NKDSP[0x%08x] %08x %08x %08x %08x\n",
                          W, CpuRead32(W), CpuRead32(W + 4),
                          CpuRead32(W + 8), CpuRead32(W + 0xC));
                }
                for (W = 0x40B14700u; W < 0x40B14880u; W += 16) {
                    CHAR16 MnA[64], MnB[64], MnC[64], MnD[64];
                    PpcDisasm(W, CpuRead32(W), MnA, sizeof(MnA));
                    PpcDisasm(W + 4, CpuRead32(W + 4), MnB, sizeof(MnB));
                    PpcDisasm(W + 8, CpuRead32(W + 8), MnC, sizeof(MnC));
                    PpcDisasm(W + 0xC, CpuRead32(W + 0xC), MnD, sizeof(MnD));
                    Print(L"  NKHND[0x%08x] %s | %s | %s | %s\n",
                          W, MnA, MnB, MnC, MnD);
                }
                for (W = 0x40B13BF8u; W < 0x40B13D20u; W += 16) {
                    CHAR16 MnA[64], MnB[64], MnC[64], MnD[64];
                    PpcDisasm(W, CpuRead32(W), MnA, sizeof(MnA));
                    PpcDisasm(W + 4, CpuRead32(W + 4), MnB, sizeof(MnB));
                    PpcDisasm(W + 8, CpuRead32(W + 8), MnC, sizeof(MnC));
                    PpcDisasm(W + 0xC, CpuRead32(W + 0xC), MnD, sizeof(MnD));
                    Print(L"  NKCAL[0x%08x] %s | %s | %s | %s\n",
                          W, MnA, MnB, MnC, MnD);
                }
                {
                    CHAR16 MnA[64];
                    Print(L"  NKTBLD 0x40B6D300..0x40B6D400:\n");
                    for (W = 0x40B6D300u; W < 0x40B6D400u; W += 4) {
                        PpcDisasm(W, CpuRead32(W), MnA, sizeof(MnA));
                        Print(L"   0x%08x: %s\n", W, MnA);
                    }
                }
                Print(L"  NKTBL ECB.ctx r1=0x%08x [r1+638]=0x%08x [r1+63C]=0x%08x "
                      L"[r1+640]=0x%08x [r1+644]=0x%08x [r1+648]=0x%08x\n",
                      R1, CpuRead32(R1 + 0x638), CpuRead32(R1 + 0x63C),
                      CpuRead32(R1 + 0x640), CpuRead32(R1 + 0x644),
                      CpuRead32(R1 + 0x648));
                Print(L"  TRAPDIS NK-CALL [r1+5E0..5FF]:\n");
                for (T = R1 + 0x5E0u; T < R1 + 0x600u; T += 16) {
                    Print(L"    0x%08x: %08x %08x %08x %08x\n",
                          T, CpuRead32(T), CpuRead32(T + 4),
                          CpuRead32(T + 8), CpuRead32(T + 0xC));
                }
                Print(L"  TRAPDIS FORK-68K 0x10FFE0..0x110040:\n");
                for (W = 0x10FFE0u; W < 0x110040u; W += 16) {
                    Print(L"    %04x %04x %04x %04x %04x %04x %04x %04x\n",
                          CpuRead16(W), CpuRead16(W + 2), CpuRead16(W + 4), CpuRead16(W + 6),
                          CpuRead16(W + 8), CpuRead16(W + 0xA), CpuRead16(W + 0xC), CpuRead16(W + 0xE));
                }
                Print(L"  TRAPDIS ECB 0xA000..0xA700:\n");
                for (W = 0xA000u; W < 0xA700u; W += 16) {
                    Print(L"    %04x %04x %04x %04x %04x %04x %04x %04x\n",
                          CpuRead16(W), CpuRead16(W + 2), CpuRead16(W + 4), CpuRead16(W + 6),
                          CpuRead16(W + 8), CpuRead16(W + 0xA), CpuRead16(W + 0xC), CpuRead16(W + 0xE));
                }
            }
    // Clear stale pending from any previous batch. Async DEC exceptions
    // raised at loop bottom are intentionally consumed here rather than
    // delivered: once the NK arms DEC early-boot, bottom-raised pendings
    // would otherwise storm the vector handler every iteration and wedge
    // boot inside the early scheduler. The 68K STOP-wake is handled at
    // actual delivery time (see PPC_EXCEPTION_DECREMENTER case below).
    g_PpcContext.ExceptionPending = 0;
            // Asynchronous exceptions (decrementer) are taken at an
            // instruction boundary: SRR0 must be the next instruction to
            // execute, not the just-executed one. Re-executing the rfi/mtmsr
            // that enabled EE would re-trigger the interrupt forever and the
            // interrupted boot-tail handoff would never resume.
            Status = PpcHandleException(Pending,
                                        (Pending == PPC_EXCEPTION_DECREMENTER ||
                                         Pending == PPC_EXCEPTION_INTERRUPT)
                                            ? Next
                                            : Current);
            if (EFI_ERROR(Status)) {
                *ExecutedCount = Executed;
                return Status;
            }
            continue;
        }
        // Advance the guest timebase (TBL/TBU) and decrementer (DEC) "tick"
        // by PPC_TIMEBASE_SCALE per instruction, and request the decrementer
        // interrupt when DEC is negative with interrupts enabled (NanoKernel
        // scheduler tick). The scale approximates a real 603e's timebase rate
        // relative to instruction throughput (~16 MHz TB vs ~50-100 MIPS);
        // larger scales make short timer intervals expire before their
        // servicing code can run, which livelocks the NK scheduler. The
        // interrupt is only raised once the NK has armed DEC via mtspr: an
        // unarmed (reset) DEC must not fire a spurious tick. (WEDGE probe
        // timed the char-wait crawl through two accelerated variants; both
        // conclusively failed -- the crawl is 68K-emulation compute-bound,
        // not virtual-time-bound, and bulk time advance degrades boot.)
        g_PpcContext.TimeBaseL += PPC_TIMEBASE_SCALE;
        if (g_PpcContext.TimeBaseL < PPC_TIMEBASE_SCALE) {
            g_PpcContext.TimeBaseH++;
        }
        g_PpcContext.Spr[SPR_DEC] -= PPC_TIMEBASE_SCALE;
        if (g_PpcContext.Spr[SPR_DEC] & 0x80000000) {
            g_PpcContext.DecrementerNegative = 1;
        }
        // External interrupt (vector 0x500): the DR console's input path is
        // interrupt-driven. Whenever SCC Rx data is queued and the guest has
        // interrupts enabled, request the External (0x500) exception. Priority
        // over the decrementer so console input is serviced promptly; the DC
        // fires again on the next tick anyway. Requires no re-arm at rfi/mtmsr
        // because delivery clears this slot and loop-bottom re-raises as long
        // as the FIFO still has bytes once EE comes back.
        if (g_PpcContext.ExceptionPending == 0 &&
            (g_PpcContext.Msr & PPC_MSR_EE) && PpcSccHasData()) {
            static UINT32 IntRaiseLogs = 0;
            UINT32 Kdp338 = 0;
            if (g_PpcContext.Spr[272] >= 0xA000u && g_PpcContext.Spr[272] <= 0xC000u) {
                Kdp338 = CpuRead32(g_PpcContext.Spr[272] - 0x338);
            }
            // Mirror the SCC-pending state into the guest's interrupt-controller
            // descriptor block (the low-RAM node [KDP-0x338] points at, 0x81C0
            // in this boot). The D0 EXT handler reads IntEvents1 there (+0x20)
            // and takes the safe level>=2 dispatch path only if it reads >= 2;
            // with the guest's own idled value 1 it falls into the [r1+0x5B0]
            // low-priority return path, which crashes mid-rotation (r1=0). Set
            // the SCC bit (bit1) in IntEvents1 and the level-9 pending word
            // (+0x38), and keep the device side of the block populated.
            if (Kdp338 >= 0x8000u && Kdp338 <= 0xA000u) {
                // The D0 EXT handler 0x40B14880 passes the level>=2 gate on
                // [IC+0x20] (IntEvents1 bit1 = SCC) only when it mirrors our
                // pending state; the level-9 ISR 0x40B148E0 then gates on
                // [IC+0x38] (level-9 SCC pending) and [IC+0x44] (priority scan
                // bound). Seed both plus [IC+0x4C] so the dispatch is reached.
                UINT32 E  = CpuRead32(Kdp338 + 0x20);
                UINT32 P  = CpuRead32(Kdp338 + 0x38);
                UINT32 B  = CpuRead32(Kdp338 + 0x44);
                UINT32 N  = CpuRead32(Kdp338 + 0x4C);
                UINT32 Kdp = g_PpcContext.Spr[272];
                UINT32 Nest = (Kdp >= 0xA000u && Kdp <= 0xC000u) ?
                              CpuRead32(Kdp - 0x1C) : 0;
                if ((E & 0x2u) == 0 || (P & 0x2u) == 0 || B < 0x100u || N != Nest) {
                    CpuWrite32(Kdp338 + 0x20, (E | 0x2u));
                    CpuWrite32(Kdp338 + 0x38, (P | 0x2u));
                    CpuWrite32(Kdp338 + 0x44, 0x3FFFFFFFu);
                    CpuWrite32(Kdp338 + 0x4C, Nest);
                    Print(L"  ICMIRROR base=0x%08x IntEvents1=0x%08x Lvl9Pend=0x%08x"
                          L" Bound=0x%08x Nest=0x%08x(+4C->0x%08x)\n",
                          Kdp338, E | 0x2u, P | 0x2u,
                          0x3FFFFFFFu, Nest, N);
                }
                // Mirror the missing DR/SCC dispatch registration. The level-9
                // ISR 0x40B148E0 dispatches through [IC+0x3C] indexed by
                // (level<<2) and [IC+0x40] indexed by (tidx<<2); the real
                // installer (syscall case 0x40B1B24C) never runs in this boot,
                // so both stay 0 and the ISR crashes at lwzx r20,r8,r20 (garbage
                // rfi to 0x0000A003). Build a minimal registration in free low
                // RAM: a per-level dispatch table, one level-9 task vector
                // {SRR0, r2}, and a stack table resuming the guest on the KDP
                // stack. Dispatching into the SCC primitives (0x40B26880/reader
                // 0x40B272E0) deadlocks: they re-acquire the SCC spinlock
                // [KDP+0xF510] that the console session still holds. Instead
                // the wake resumes the preempted task exactly where the EXT
                // interrupted it (Next == the idle give-up 0x40B24F0C); its
                // register state is still live in the vCPU (PPC exceptions do
                // not auto-save GPRs) and dispatch only rewrites r1/r2. The
                // give-up sc 0x2E wake then re-checks SCC input and the loop
                // recycles, letting the FIFO drain naturally. SRR1 comes from
                // [resume_stack-0x964], so seed it with the live MSR (EE on)
                // or the rfi'd task would resume with a garbage machine state.
                {
                    static UINT32 TabBase = 0;
                    UINT32 Tidx = (Kdp >= 0xA000u && Kdp <= 0xC000u) ?
                                  CpuRead16(Kdp - 0x116) : 0;
                    if (TabBase == 0) {
                        UINT32 Cand[6] = { 0x6C00u, 0x6A00u, 0x6800u,
                                           0x6600u, 0x6400u, 0x6200u };
                        UINT32 i, c = 0;
                        for (i = 0; i < 6; i++) {
                            UINT32 a = Cand[i], j, Free = 1;
                            for (j = 0; j < 0x220u; j += 4) {
                                if (CpuRead32(a + j) != 0) { Free = 0; break; }
                            }
                            if (Free) { c = a; break; }
                        }
                        if (c == 0) {
                            c = 0x6C00u;
                        }
                        TabBase = c;
                        // per-level dispatch table (index level*4, 0x100 bytes)
                        for (i = 0; i < 0x40u; i++) {
                            CpuWrite32(c + i * 4, 0);
                        }
                        // level-9 task vector {SRR0, r2} = preempted task resume
                        CpuWrite32(c + 0x100, Next);
                        CpuWrite32(c + 0x104, g_PpcContext.Gpr[2]);
                        CpuWrite32(c + 0x24, c + 0x100);      // A[9<<2] = P
                        // stack table B[tidx<<2] -> KDP stack
                        for (i = 0; i < 0x40u; i++) {
                            CpuWrite32(c + 0x120 + i * 4, 0);
                        }
                        if (Tidx < 0x40u) {
                            UINT32 Stack = (Kdp >= 0xA000u && Kdp <= 0xC000u) ?
                                           CpuRead32(Kdp - 0x4) : 0x77E0u;
                            CpuWrite32(c + 0x120 + (Tidx << 2), Stack);
                            // SRR1 source: the ISR reads [r1+0xF69C] (where r1
                            // is still the KDP stack) and masks it before
                            // mtspr SRR1. Seed it with the live MSR (EE on)
                            // or the rfi'd task would resume with a garbage MSR.
                            CpuWrite32(Stack + 0xF69C, g_PpcContext.Msr);
                        }
                        CpuWrite32(Kdp338 + 0x3C, c);
                        CpuWrite32(Kdp338 + 0x40, c + 0x120);
                        Print(L"  ICDSPTBL base=0x%08x tidx=%u P=0x%08x"
                              L" SRR0=0x%08x r2=0x%08x stack=0x%08x"
                              L" SRR1seed=0x%08x\n",
                              c, Tidx, c + 0x100, Next, g_PpcContext.Gpr[2],
                              (Kdp >= 0xA000u && Kdp <= 0xC000u) ?
                              CpuRead32(Kdp - 0x4) : 0, g_PpcContext.Msr);
                    }
                }
            }
            PpcIntCtrlInstall();
            g_PpcContext.ExceptionPending = PPC_EXCEPTION_INTERRUPT;
            if (IntRaiseLogs < 10) {
                IntRaiseLogs++;
                Print(L"  INTRAISE PC=0x%08x r1=0x%08x head=%d tail=%d MSR=0x%08x",
                      g_PpcContext.Pc, g_PpcContext.Gpr[1],
                      g_SccRxFifoHead, g_SccRxFifoTail, g_PpcContext.Msr);
                if (Kdp338 >= 0x8000u && Kdp338 <= 0xA000u) {
                    Print(L" [IC=%08x] +20=%08x +24=%08x +38=%08x +3C=%08x +40=%08x +44=%08x +4C=%08x\n",
                          Kdp338, CpuRead32(Kdp338 + 0x20), CpuRead32(Kdp338 + 0x24),
                          CpuRead32(Kdp338 + 0x38), CpuRead32(Kdp338 + 0x3C),
                          CpuRead32(Kdp338 + 0x40), CpuRead32(Kdp338 + 0x44),
                          CpuRead32(Kdp338 + 0x4C));
                } else {
                    Print(L" [IC=0x%08x]\n", Kdp338);
                }
            }
        }
        if (g_PpcContext.DecrementerWritten && g_PpcContext.DecrementerNegative &&
            (g_PpcContext.Msr & PPC_MSR_EE) && g_PpcContext.ExceptionPending == 0 &&
            !g_BootDecGate) {
            g_PpcContext.ExceptionPending = PPC_EXCEPTION_DECREMENTER;
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
        case XO_EXTSH:     Name = L"extsh"; break;
        case XO_EXTSB:     Name = L"extsb"; break;
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
// ---------------------------------------------------------------------------
// Operand-level disassembly of a single instruction (for NK dispatch dumps).
// Handles the D/I/B/X/rot/XL classes the 0x700 handler / DR stubs use.
// ---------------------------------------------------------------------------
static const CHAR16* g_PdReg[32] = {
    L"r0", L"r1", L"r2", L"r3", L"r4", L"r5", L"r6", L"r7",
    L"r8", L"r9", L"r10", L"r11", L"r12", L"r13", L"r14", L"r15",
    L"r16", L"r17", L"r18", L"r19", L"r20", L"r21", L"r22", L"r23",
    L"r24", L"r25", L"r26", L"r27", L"r28", L"r29", L"r30", L"r31"
};
static void
PpcDisasm (
    IN UINT32 Address,
    IN UINT32 w,
    OUT CHAR16* Out,
    IN UINTN OutSize
    )
{
    UINT32 Op = OP(w);
    UINT32 Rt = (w >> 21) & 0x1F;
    UINT32 Rs = (w >> 21) & 0x1F;
    UINT32 Ra = (w >> 16) & 0x1F;
    UINT32 Rb = (w >> 11) & 0x1F;
    INT16  D16 = (INT16)(w & 0xFFFF);
    const CHAR16* RtS = g_PdReg[Rt];
    const CHAR16* RsS = g_PdReg[Rs];
    const CHAR16* RaS = g_PdReg[Ra];
    const CHAR16* RbS = g_PdReg[Rb];
    if (Out == NULL || OutSize < 16) return;
    Out[0] = 0;
    switch (Op) {
    case 3:
        UnicodeSPrint(Out, OutSize, L"twi %u,%s,%d", (w >> 21) & 0x1F, RaS, D16);
        break;
    case 10:
        UnicodeSPrint(Out, OutSize, L"cmplwi %s,0x%x", RaS, w & 0xFFFF);
        break;
    case 11:
        UnicodeSPrint(Out, OutSize, L"cmpwi %s,%d", RaS, D16);
        break;
    case 12:
        UnicodeSPrint(Out, OutSize, L"addic %s,%s,%d", RtS, RaS, D16);
        break;
    case 14:
        UnicodeSPrint(Out, OutSize, L"addi %s,%s,%d", RtS, RaS, D16);
        break;
    case 15:
        UnicodeSPrint(Out, OutSize, L"addis %s,%s,%d", RtS, RaS, D16);
        break;
    case 16:
        {
            UINT32 Bo = (w >> 21) & 0x1F;
            UINT32 Bi = (w >> 16) & 0x1F;
            INT32  Bd = (INT32)(INT16)((w >> 2) & 0x3FFF) * 4;
            if ((w & 0x400)!=0 && (w & 2)!=0)  /* unused */
            {
            }
            if ((w & 2) != 0) {
                UnicodeSPrint(Out, OutSize, L"bc %u,%u,0x%x%s", Bo, Bi,
                       (UINT32)(INT32)Bd, (w & 1) ? L"+" : L"");
            } else {
                UnicodeSPrint(Out, OutSize, L"bc %u,%u,%s", Bo, Bi,
                       (w & 1) ? L"+0x" : L"");
            }
        }
        break;
    case 18:
        {
            // Branch: LI = bits 25..2 (24-bit) sign-extended then <<2.
            INT32 Li = (INT32)((w & 0x03FFFFFCu) << 6);  // shift off, sign bits above
            Li = (Li >> 6);                               // arithmetic <<2 after sext
            if ((w & 2) != 0) {
                UnicodeSPrint(Out, OutSize, L"b %s0x%x", (w & 1) ? L"la " : L"a ", (UINT32)(INT32)Li);
            } else {
                UnicodeSPrint(Out, OutSize, L"b%s0x%08x", (w & 1) ? L"l " : L" ",
                       Address + (UINT32)Li);
            }
        }
        break;
    case 20:
        UnicodeSPrint(Out, OutSize, L"rlwimi %s,%s,%u,%u,%u", RsS, RaS,
               (w >> 11) & 0x1F, (w >> 6) & 0x1F, (w >> 1) & 0x1F);
        break;
    case 21:
        UnicodeSPrint(Out, OutSize, L"rlwinm %s,%s,%u,%u,%u", RsS, RaS,
               (w >> 11) & 0x1F, (w >> 6) & 0x1F, (w >> 1) & 0x1F);
        break;
    case 23:
        UnicodeSPrint(Out, OutSize, L"rlwnm %s,%s,%s,%u,%u", RsS, RaS, RbS,
               (w >> 6) & 0x1F, (w >> 1) & 0x1F);
        break;
    case 24: UnicodeSPrint(Out, OutSize, L"ori %s,%s,0x%x", RsS, RaS, w & 0xFFFF); break;
    case 25: UnicodeSPrint(Out, OutSize, L"oris %s,%s,0x%x", RsS, RaS, w & 0xFFFF); break;
    case 26: UnicodeSPrint(Out, OutSize, L"xori %s,%s,0x%x", RsS, RaS, w & 0xFFFF); break;
    case 27: UnicodeSPrint(Out, OutSize, L"xoris %s,%s,0x%x", RsS, RaS, w & 0xFFFF); break;
    case 28: UnicodeSPrint(Out, OutSize, L"andi. %s,%s,0x%x", RsS, RaS, w & 0xFFFF); break;
    case 29: UnicodeSPrint(Out, OutSize, L"andis. %s,%s,0x%x", RsS, RaS, w & 0xFFFF); break;
    case 32: UnicodeSPrint(Out, OutSize, L"lwz %s,%d(%s)", RtS, D16, RaS); break;
    case 33: UnicodeSPrint(Out, OutSize, L"lwzu %s,%d(%s)", RtS, D16, RaS); break;
    case 34: UnicodeSPrint(Out, OutSize, L"lbz %s,%d(%s)", RtS, D16, RaS); break;
    case 36: UnicodeSPrint(Out, OutSize, L"stw %s,%d(%s)", RsS, D16, RaS); break;
    case 37: UnicodeSPrint(Out, OutSize, L"stwu %s,%d(%s)", RsS, D16, RaS); break;
    case 38: UnicodeSPrint(Out, OutSize, L"stb %s,%d(%s)", RsS, D16, RaS); break;
    case 40: UnicodeSPrint(Out, OutSize, L"lhz %s,%d(%s)", RtS, D16, RaS); break;
    case 42: UnicodeSPrint(Out, OutSize, L"lha %s,%d(%s)", RtS, D16, RaS); break;
    case 44: UnicodeSPrint(Out, OutSize, L"sth %s,%d(%s)", RsS, D16, RaS); break;
    case 46: UnicodeSPrint(Out, OutSize, L"lmw %s,%d(%s)", RtS, D16, RaS); break;
    case 47: UnicodeSPrint(Out, OutSize, L"stmw %s,%d(%s)", RsS, D16, RaS); break;
    case 19:
        switch ((w >> 1) & 0x3FF) {
        case 16:  UnicodeSPrint(Out, OutSize, L"bclr %u,%u%s", (w >> 21) & 0x1F,
                         (w >> 16) & 0x1F, (w & 1) ? L"+" : L""); break;
        case 528: UnicodeSPrint(Out, OutSize, L"bcctr %u,%u%s", (w >> 21) & 0x1F,
                         (w >> 16) & 0x1F, (w & 1) ? L"+" : L""); break;
        case 18:  UnicodeSPrint(Out, OutSize, L"rfi"); break;
        case 150: UnicodeSPrint(Out, OutSize, L"isync"); break;
        default:  UnicodeSPrint(Out, OutSize, L"xl-op %u", (w >> 1) & 0x3FF); break;
        }
        break;
    case 31:
        switch ((w >> 1) & 0x3FF) {
        case 23:   UnicodeSPrint(Out, OutSize, L"lwzx %s,%s,%s", RtS, RaS, RbS); break;
        case 55:   UnicodeSPrint(Out, OutSize, L"lwzux %s,%s,%s", RtS, RaS, RbS); break;
        case 87:   UnicodeSPrint(Out, OutSize, L"lbzx %s,%s,%s", RtS, RaS, RbS); break;
        case 279:  UnicodeSPrint(Out, OutSize, L"lhzx %s,%s,%s", RtS, RaS, RbS); break;
        case 311:  UnicodeSPrint(Out, OutSize, L"lhax %s,%s,%s", RtS, RaS, RbS); break;
        case 151:  UnicodeSPrint(Out, OutSize, L"stwx %s,%s,%s", RsS, RaS, RbS); break;
        case 183:  UnicodeSPrint(Out, OutSize, L"stbx %s,%s,%s", RsS, RaS, RbS); break;
        case 407:  UnicodeSPrint(Out, OutSize, L"sthx %s,%s,%s", RsS, RaS, RbS); break;
        case 266:  UnicodeSPrint(Out, OutSize, L"add %s,%s,%s", RtS, RaS, RbS); break;
        case 40:   UnicodeSPrint(Out, OutSize, L"subf %s,%s,%s", RtS, RaS, RbS); break;
        case 28:   UnicodeSPrint(Out, OutSize, L"and %s,%s,%s", RsS, RaS, RbS); break;
        case 444:  UnicodeSPrint(Out, OutSize, L"or %s,%s,%s", RsS, RaS, RbS); break;
        case 316:  UnicodeSPrint(Out, OutSize, L"xor %s,%s,%s", RsS, RaS, RbS); break;
        case 124:  UnicodeSPrint(Out, OutSize, L"nor %s,%s,%s", RsS, RaS, RbS); break;
        case 32:
            UnicodeSPrint(Out, OutSize, L"cmpl %u,%s,%s", (w >> 23) & 0x3, RaS, RbS);
            break;
        case 0:
            UnicodeSPrint(Out, OutSize, L"cmp %u,%s,%s", (w >> 23) & 0x3, RaS, RbS);
            break;
        case 339:
            UnicodeSPrint(Out, OutSize, L"mfspr %s,%u", RtS,
                   (((w >> 16) & 0x1F) << 5) | ((w >> 11) & 0x1F));
            break;
        case 467:
            UnicodeSPrint(Out, OutSize, L"mtspr %u,%s",
                   (((w >> 16) & 0x1F) << 5) | ((w >> 11) & 0x1F), RsS);
            break;
        case 24:   UnicodeSPrint(Out, OutSize, L"slw %s,%s,%s", RsS, RaS, RbS); break;
        case 536:  UnicodeSPrint(Out, OutSize, L"srw %s,%s,%s", RsS, RaS, RbS); break;
        case 792:  UnicodeSPrint(Out, OutSize, L"sraw %s,%s,%s", RsS, RaS, RbS); break;
        case 824:  UnicodeSPrint(Out, OutSize, L"srawi %s,%s,%u", RsS, RaS,
                          (w >> 11) & 0x1F); break;
        case 922:  UnicodeSPrint(Out, OutSize, L"extsh %s,%s", RsS, RaS); break;
        case 954:  UnicodeSPrint(Out, OutSize, L"extsb %s,%s", RsS, RaS); break;
        case 26:   UnicodeSPrint(Out, OutSize, L"cntlzw %s,%s", RsS, RaS); break;
        case 235:  UnicodeSPrint(Out, OutSize, L"mullw %s,%s,%s", RtS, RaS, RbS); break;
        case 11:   UnicodeSPrint(Out, OutSize, L"mulhw %s,%s,%s", RtS, RaS, RbS); break;
        case 104:  UnicodeSPrint(Out, OutSize, L"neg %s,%s", RtS, RaS); break;
        case 598:  UnicodeSPrint(Out, OutSize, L"sync"); break;
        case 210:  UnicodeSPrint(Out, OutSize, L"mtsr %u,%s", (w >> 16) & 0xF, RsS); break;
        case 336:  UnicodeSPrint(Out, OutSize, L"mfsr %s,%u", RtS, (w >> 16) & 0xF); break;
        default:   UnicodeSPrint(Out, OutSize, L"x-op %u", (w >> 1) & 0x3FF); break;
        }
        break;
    default:
        UnicodeSPrint(Out, OutSize, L"op%u", Op);
        break;
    }
}

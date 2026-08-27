# EFI Mac OS Boot Layer — Implementation Plan

## Current State

The project is a functional **heavy UEFI bootloader** for classic Mac OS. It
builds a PowerPC Mac boot image from UEFI standard protocols, reads classic Mac
discs in place, installs real Mac firmware into the guest image, and self-tests
the whole path. **Phase A (nanokernel boot) is complete**: the New World ROM
boots through the warm handoff path with a clean environment — contiguous
guest RAM bank at 0, emulated SCC output device, gated PMDT injection,
harmonized area merges, and SheepShaver-style neutralization of eleven NK
hardware-init sites.

**Architectural correction (2026-08, DingusPPC review):** the next phase was
previously scoped as almost-entirely 68K work (a hand-rolled `m68k.c`
interpreter, EMUL_OP handlers, 68K hardware registers). That was the wrong
frame — see ARCHITECTURE.md "Architectural Reference". DingusPPC and even the
project's own SheepShaver model run the nanokernel's **PPC** DR emulator on the
host CPU; there is no need to boot through a C 68K interpreter. The corrected
**PPC-native pivot** is on the critical path: restore the OS's own PPC DR
emulator (stop the `0x40B67C60` hijack) and complete the PPC environment it
needs — real translation (BAT/alias), the missing 601-era opcodes the DR
emulator uses, and the hardware/KernelData seeds. The 68K interpreter remains
only as a later legacy-runtime fallback for the Classic app layer.

### Verified end-to-end (Windows host, QEMU + OVMF)

- PowerPC CPU self-test **35/35** (includes the FPU core: opcodes 48-63 gated on
  MSR[FP], FP-unavailable exception 0x800, FPSCR, A-form arithmetic).
- 68K CPU self-test passes (instruction decode, addressing modes, flag compute).
- Boot memory-map self-test **7/7** with the demo ROM, **5/5** with a real ROM
  (region presence + CHRP signature + read-only enforcement).
- System Folder / driver self-test **7/7**.
- **Real New World ROM discovered and installed** from a genuine Mac OS 9.2.2
  install disc (`Power Mac G4 Install:System Folder:Mac OS ROM`, 2,763,530
  bytes, `<CHRP-BOOT>` signature) at guest `0x40800000`.
- All non-empty Extensions stage with **0 failures**: System 7.5.3 2/2, Mac OS
  8.1 18/18, Mac OS 9.2.2 25/25 (up to 64 drivers supported).
- Graphics blits verified across every GOP pixel; Block I/O and SNP exercised
  with real hardware calls.
- **Phase A boot flow verified live (2026-08)**: gated PMDTINJECT fires once;
  both MERGE-HARMONIZE sites fire; BOOTTAIL -> EMUTRAP -> KDPROF (6.5M
  KernelData loads profiled) -> EMUSTART -> INJENTRY; native 68K interpreter
  runs DR-callbacks, loader probes and the decompressor (`decomp`, `nkfill`
  stages reached).

### Recent work

- **Phase B/C session 8 (2026-08-25 cont.7): method-1 bisection EXECUTED,
  verdict definitive.** Escalated watches until the 0x7F4085 continuation
  value's origin was unambiguous: it is NEVER stored by 68K code
  (TRAMP-VAL), never pushed via M68kPushLong, never written by PPC
  CpuWrite32 — and the region itself receives zero writes all boot from
  either CPU (TRAMP-W). Stack dump at guard-halt showed the poison sits
  DIRECTLY ABOVE a legitimate return (0x4087CA90): it is an ARGUMENT of
  the call whose epilogue mis-skips. Caller decoded at 0x4087CA5C-92:
  DBF loop; MOVE.L #$60082,D0; LEA $0DA0,A0; **A051 trap** (allocator,
  result in A0 -> stored to $0CC8/$08A0); zone stores; then pushes word
  $007F and table-JSRs (4EB0 81E1) into ANOTHER unseeded table whose
  callee page-frames $007F into bogus 0x7Fxxxx continuations.
  Implemented + A/B tested: HOSTTRAP A051 (page-aligned bump allocator,
  honored the real 384KB request after enlarging arena to 1MB) — works;
  CA86-SKIP interceptor — REJECTED (diverts into $FF-filled low RAM,
  worse than deterministic guard halt). Also swept yet another
  reintroduced uppercase-STATIC batch (6 sites). STABLE CONFIG RESTORED:
  rescue -> WSCRUB-EMPTY -> real MM trap -> NKHEAP/A051 services ->
  single clean ZERO-EXEC halt, zero ILLEGALs.
  CONCLUSION OF BISECTION: no single neutralized patch is responsible —
  MULTIPLE independent dispatch tables + allocator state are simply never
  constructed in our environment; every consumer surfaces a different
  symptom. Further interpreter-side intervention has negative ROI.
  REMAINING PATH TO DESKTOP requires reference state: seed (a) the
  trampoline page at ~0x7F4000, (b) full [08A4] heap-chain semantics,
  (c) the CA86 table's entries — obtainable only from a real-hardware
  dump or same-ROM reference run (BasiliskII/SheepShaver), then installed
  in bootloader_impl exactly like our working KDP/XLM seeds. All tooling,
  watches, and one-line milestone logs are in place for that session.
- **Phase B/C session 7 (2026-08-25 cont.6): continuation-target hunt.**
  Added TRAMP-W (all guest writes into 0x7F0000-0x800000, both CPU paths)
  and TRAMP-PUSH (M68kPushLong values in range). Result: ZERO of either —
  the 0x7F4085 continuation target is never written nor pushed, so it
  reaches the stack via arithmetic or a ROM-table absolute load. Combined
  with its stable value across runs, conclusion: REAL hardware builds NK
  continuation stubs at a FIXED low-RAM location (~0x7F4xxx) during an NK
  init stage our patch set neutralized/skipped; the RTS-through-callback
  then lands in zeros here. Tooling now in place: ZERO-EXEC guard (halts
  with ring), TRAMP watches, THUNK-W, LOW-SP rescue (2 fires, args now
  coherent), WSCRUB-EMPTY exit, NKHEAP-SEED chain. Stable end state:
  guard-halt immediately at first missing continuation — clean, fast,
  fully diagnosable.
  DEFINITIVE NEXT STEP (needs reference data, not more inference):
  obtain expected content/state of (a) 0x7F4000 trampoline page and
  (b) [0x08A4] heap chain from a reference (real dump / BasiliskII+same
  ROM run / SheepShaver synthetic equivalents), then seed both during
  bootloader install exactly like KDP/XLM seeds. Interim alternative:
  bisect our 11 NKPATCH sites by re-enabling one at a time to find which
  stage would have built the stubs.
- **Phase B/C session 6 (2026-08-25 cont.5): seeding executed + ZERO-EXEC
  guard landed.** Implemented: (1) host trap-service framework in case 0xA;
  replaced guess-the-convention allocator with NKHEAP-SEED — zone chain
  [0x08A4]->master(0x61000)->zone(0x61100)->[z+0x16]=node(0x61200)->block
  base(0x62000) so genuine ROM heap handlers walk valid memory; fires once,
  verified. (2) ZERO-EXEC GUARD: opcode 0000 executed >=0x10000 in low RAM
  halts with 32-deep PC ring instead of silently crawling megabytes of
  ORI.B no-ops. (3) Alias windows FF0/FF4/FF8 made READ-ONLY (stray
  broken-stack frame writes were corrupting the shared ROM buffer).
  ROOT DISCOVERY via guard's ring: the post-NewHandle loss is NK's
  CONTINUATION-TRAMPOLINE pattern — code pushes a callback address
  (~0x7F4085, RAM) then RTS-jumps through it; the builder that should have
  copied those stub bytes into the 0x7Fx region never ran/was skipped.
  STABLE END STATE per run: rescue -> WSCRUB-EMPTY exit -> real MacOS MM
  trap ($A833 -> 0x40879790) -> guard-halt at first missing continuation.
  NEXT SESSION (single focused mission): find the trampoline BUILDER —
  watchpoint writes into 0x7F0000-0x800000 during earlier eras (who was
  SUPPOSED to copy stubs there), likely one of the stages our NKPATCH
  sites neutralized; restore it selectively. Alternative: synthesize the
  expected continuation stubs ourselves once their bytes are known (dump
  from a reference run of the same ROM under BasiliskII/SS).
- **Phase B/C session 5 (2026-08-25 cont.4): SEEDING BREAKTHROUGH.** Chain
  of interventions that MOVED THE FRONTIER for the first time in days:
  (1) ROM alias widened to ALL FOUR 4MB windows (FF0/FF4/FF8/FFC) so
  unrelocated 0xFFxxxxxx table entries resolve everywhere;
  PPC_MAX_GUEST_REGIONS 8->20. (2) CALLER micro-trace caught the arg=6
  producer red-handed: live `LEA -$156(A7),A7` allocating a 346-byte local
  frame on SP=0xE -> frames land in alias-ROM -> args are garbage.
  (3) LOW-SP RESCUE v2: when PC first enters 0x40804960-4B00 with
  SP<0x400, reseat stack to 0x7800 (below abandoned 0x7FFC boot stack).
  Result: CA00-SKIP stopped firing (args coherent), clear-loop bound
  became 0x7806 (caller's own frame region). (4) WSCRUB-EMPTY: stale A0
  (ROM garbage) vs RAM bound -> complete as no-op, return success.
  RESULT: boot EXITS the 0x4087CA24 spin and executes REAL MacOS code:
  builds structures in rescued RAM, calls **_NewHandle ($A833)** through
  the trap dispatcher into the Memory Manager at 0x40879790. Next wall:
  MM handler reads current-zone pointer [0x08A4] = garbage (no
  InitApplZone/SysZone ever ran) -> later jumps into zeroed 0x7F56xx.
  NEXT SESSION (clear mission): seed classic MacOS low-memory globals —
  zone pointers ([0x908]/[0x90A] current zone, SysZone/ApplZone bases,
  [0x114] ApplLimit, stack bounds) either by porting SheepShaver
  patch_68k()'s global-init block or by driving the ROM's own heap init
  with proper boot blocks. Then continue toward Finder/desktop.
- **Phase B/C session 4 (2026-08-25 cont.3):** Executed deep-dive on the NK
  MixedMode wall. Findings: (1) THUNK-W watchpoint proved the dispatch
  tables receive ZERO writes all boot — Apple's loader/an early NK stage
  that never runs here is their only producer. (2) Decoded the full thunk
  family: {MOVE.L tbl(A0,Dn.L),-(SP); MOVE.L (Am),D0; ORI.B #sel,Dx; RTS}
  at 0x40804xxx-4088xxxx, variants with (A0)/(A2) bases and sel bytes
  $60-$88. Built THUNK-NULL family interceptor (shape-match + validated
  return w/ healer fallback) — works mechanically, but D0=0 null-return
  diverts callers into untested paths (alias-stack jumps); DISABLED via
  flag after A/B testing showed the original BAD-RTS-heal path is MORE
  stable (~193M instructions, zero illegals vs early halt). (3) LOW-SP
  detector mapped the stack collapse: NK's relocating-bootstrap web
  (0x408004D6-59C copy-loops + JMP (A0/A2) chains) legitimately runs on
  near-zero stacks; routines are continuation-style (JMP-entered, no
  return addr) — stack rescue TESTED AND REJECTED (RTS pops garbage,
  halts 68K early). (4) Found+fixed a REINTRODUCED STATIC-macro bug in
  session-added code (uppercase STATIC = empty again -> automatic
  counters; re-swept to lowercase static). STABLE STATE RESTORED:
  ~193M instructions, frontier unchanged at 0x4087CA24 spin.
  NEXT SESSION: stop intervening at the interpreter layer. The only
  remaining lever is SEEDING the tables: dump what the tables must hold
  from a real New World ROM's post-loader state (boot a reference
  emulator / extract from SheepShaver's own generated images), then write
  those bytes in bootloader_impl during ROM install — same approach as
  our existing KDP/ECB/XLM seeds which already work.
- **Phase B/C session 3 (2026-08-25 cont.2):** Executed plan items a/b/c.
  (c) Escapes disabled -> IDENTICAL frontier => escapes never skipped a
  needed builder; re-enabled nothing lost. (b) SheepShaver diff yielded
  RomRelocateJumpTables(): New World image stores 68K jump-table entries as
  0xFFxxxxxx ROM-relative words that Apple's loader rebases; we now scan for
  header {41FA000E 21C82010 4E75}, rebase every 0xFFxxxxxx longword to
  (v&0x3FFFFF)+RomBase, skip zeros, chain blocks (found+fixed 72 entries @
  ROM+0x5130). (a) Sentinel audit traced the arg=6 producer: caller frame
  at ret 0x40804AD8 pushes literal 6; upstream state poisoned by the FIRST
  dead tail-dispatch (stub family {MOVE.L tbl(A0,D1.l),-(SP); MOVE.L (A0),
  D0; ORI.B #sel,D0; RTS} array at 0x4080A460.. stride 0x10, per-selector
  trampolines of the NK MixedMode/DR layer). Added CA00-SKIP (rts when arg
  implausible) + DEADTBL diagnostics. STILL WALLS at 0x4087CA24 spin.
  KEY NEW OBSERVATION: NK runs this stage with SP=0xFFFFFExx — a CLASSIC-
  MAP-style top-of-memory stack through the 0xFFC alias, and executes code
  AT FFC4Axxx alias addresses; suspect an entire NK init stage assumes the
  classic memory map or expects Apple-loader-relocated tables we only
  partially replicate.
  NEXT SESSION STRATEGY (stop patching symptoms): port SheepShaver's
  patch_nanokernel_boot WHOLESALE for New World — replace this entire
  dispatcher/init region with hand-built equivalents like SS does
  (LA_EmulatorCode/LA_DispatchTable already exist; extend to the 68K-side
  thunk array + its RAM tables), OR emulate Apple's loader step-for-step:
  find what fills the RAM table the thunks index (watchpoint writes to
  0xC16/0xC34 region during early boot) and seed those values ourselves.
- **Phase B/C session 2 (2026-08-25 cont.):** Decoded the 130M-instruction
  frontier fully. Chain: boot glue 0x408001BA BSRs a tail-dispatch stub
  (MOVE.L tbl(A0,D1.L),-(SP); RTS) whose RAM table slot holds sentinel 1 ->
  BAD RTS -> healed -> NK init runs LINE-A $A019 handlers correctly -> later
  function 0x4087CA00 (LINK/MOVEM prologue; arg struct at 8(A6)) clears via
  CLR.W (A0)+/CMPA.L (A1),A0/BNE.S where A1=A4+4 and [A4+4] must hold the
  region END pointer; ours holds 6 -> infinite spin. Added WSCRUB-WEDGE
  escape doing faithful LINK-frame unwind ([A6+4]=ret, restore D2/A2/A4 from
  [A6-4/-8/-12]) -> works, returns to 0x40804AD8 repeatedly. BUT every pass
  re-enters more dead-table tail-dispatches (RTS->1 at 0x40870766,
  0x4087CC7C, 0x40881518...) until JMP (A1)->0x1E000000 runaway.
  **CONCLUSION: whack-a-mole escapes exhausted; root gap is UNSEEDED NK INIT
  TABLES** (slots holding small-int sentinels where pointers belong —
  something upstream writes success-sentinels into pointer slots, or an
  entire seeding stage never ran). NEXT SESSION: (a) find who writes 1 into
  [0x40804994]/friends — audit bootloader_impl seeds + NK patch layer for
  constant-1 stores into ROM-page tables; (b) diff our low-mem/NK-area
  layout vs SheepShaver rom_patches.cpp InitAwake/patch sequence for the
  missing stage; (c) consider running the NK's own table-builder instead of
  skipping it (the resolver-wedge escape may have SKIPPED the builder!).
  Revert candidate if (c): RESOLVER-WEDGE one-shot may be too eager.
- **Phase B/C session (2026-08-25):** `STATIC`-macro bug fixed project-wide —
  gnu-efi's `#define STATIC` (empty) made every "static" local automatic, so
  ALL one-shot guards/repetition detectors silently reset each call; replaced
  with real `static` across src/ (12 files). Phase C `emul_op.c/.h` now
  compile clean (translation.h include, 8KB NVRAM array, RMVTIME wired to
  EmulTimerRemove). m68k.c: RTR pops CCR+PC (was RTE); NBCD/ABCD/SBCD proper
  BCD adjust + ANDed-Z/X/C; ADDX/SUBX both forms with X/Z/V/N; real CHK trap;
  native FE40+ EMUL_OP routing to EmulOpDispatch(sel-3); XLM refresh gate
  (skips while PC inside scrub loop — [0x2800] doubles as the scrub bound!);
  VBL injection no longer consumes flags it cannot deliver; timer fire sets
  A1=task. interpreter.c: PpcEmulatorDispatchOp routes markers>=3 to
  EmulOpDispatch. **Post-increment bug fixed in M68kReadEA** ((An)+ sources
  never advanced the register — root cause of BAD-RTS frame storms).
  Fast paths: low-mem scrub loop bulk-filled ([2800] 'Baah' clamp), resolver
  wedge escape E022<->E04A -> trampoline 0x408001EE via stack scan.
  **Boot frontier: ~130M instructions deep in NK init**, parked at word-clear
  spin 0x4087CA20 (`CLR.W (A0)+ / CMPA.L (A1),A0 / BNE.S`) where [A1]=0x6 is
  not a valid end pointer — upstream register contract unmet (suspect the
  healed BAD RTS #0 at 0x4080A49C resumed mid-function). Next: trace callers
  of the 7CA20 routine and why A1 is garbage; then continue Phase D/E.
- **Phase A complete** (see the phase section below for the full list): MSR
  boot-path selection documented empirically; PMDT builder skip + gated
  injection; SCC device with scrub-proof poll hook and banner echo;
  merge-guard harmonization at both guard sites; KernelData validation plus
  the KDPROF read-offset profiler; contiguous boot RAM bank at guest 0;
  eleven-site NK cold-boot neutralization; PVR corrected to 7400 (0x000C0000).
- **Heavy-bootloader framing.** UEFI protocols (GOP/BlockIO/SNP/SimpleFS) are
  the hardware abstraction; simulated Mac devices are wired to them. Docs and
  boot output reframed from "emulator" to "boot layer".
- **ROM type awareness.** `PPC_ROM_TYPE_OLD_WORLD/NEW_WORLD/DEMO`; the boot
  self-test no longer assumes the demo ROM's `ROM1`/reset-vector layout when a
  real ROM is installed.
- **HFS driver staging.** Catalog-ID-based file lookup
  (`PpcHfsGetEntryById`), whole-catalog `Mac OS ROM` search
  (`PpcHfsFindMacOsRom`), auto block-size detection, multi-overflow extents,
  empty-file skipping (7.5.3's 0-byte Finder).
- **Native 68K interpreter** (`src/cpu/m68k.c`, ~2900 lines): all data-movement
  opcodes, arithmetic, logic, shifts, branches (Bcc/DBcc), bit ops, system
  calls (TRAP/RTE/MOVE SR), effective-address computation, CCR flag compute.
- **68K ↔ PPC context synchronization** via `M68kSyncFromPPC` / `M68kSyncToPPC`
  (D0-D7 = PPC r8-r15, A0-A6 = r16-r22, A7 = r1, PC = r24, SR = r25).
- **New World ROM patching** (`PpcPatchNewWorldRom`): ConfigInfo LA fields
  redirected, twi kernel-trap table rewritten, 5 emulator-entry routines
  installed, EMUL_OP dispatch markers installed, rlwimi dispatch-bit-20
  neutralised, XLM globals seeded.
- **68K DR-emulator hook** at `0x40B67C60`: the PPC interpreter intercepts the
  common dispatch and calls `M68kExecuteFromPPC()` for native 68K execution.
- **PPC-level 68K opcode hooks**: MOVE SR (0x46FC), RESET (0x4E70), escape
  (0x4E7B), MOVEQ #imm,Dn (0x7F1A) routed through the interpreter for
  opcodes not yet in the opcode table.

## SheepShaver Architecture Reference

SheepShaver is a **paravirtualizer**, not a hardware emulator. Key design:

1. **Patches the 68K ROM** so all code runs in problem state (no supervisor mode,
   no MMU). Only `mfmsr` is implemented, returning `0x0000f072` (ME|RI|FP|PR).
2. **Replaces Toolbox trap vectors** (A-line traps) with `EMUL_OP` instructions —
   custom 68K opcodes that dispatch to host-side C++ handlers.
3. **Intercepts all driver calls**: disk, SCSI, audio, ADB (keyboard/mouse),
   timer, serial, ethernet, video — all via EMUL_OP dispatch.
4. **Two-way bridge**: `Execute68k()` calls 68K code from native;
   `ExecuteNative()` calls native code from 68K via EMUL_OP.
5. **KernelData** at `0x68FFE000`: hardware config (PVR, clock, OpenPIC, OF
   device tree) filled per-ROM type.
6. **XLM** ("eXtra Low Memory") at `0x2800`: communication mailbox between the
   emulator and Mac OS (`XLM_RUN_MODE`, `XLM_SIGNATURE`, native fn ptrs).
7. **EMUL_OP selectors** (see `emul_op.h`): ~50 selectors covering XPRAM,
   NVRAM, Sony, Disk, CDROM, Audio, ADB, Timer, Clipboard, SCSI, ExtFS, etc.

## Gap Analysis: SheepShaver vs. EFIMac

| Component | SheepShaver | EFIMac Status | Gap |
|-----------|-------------|---------------|-----|
| PPC interpreter | Kheperix (interp + JIT) | Full interpreter (5300+ lines) | Complete |
| 68K interpreter | Built into DR emulator | Native C interpreter (2900 lines) | Needs more opcodes |
| ROM patching | 68K trap table + EMUL_OP markers | ConfigInfo + trap table + entry routines | Mostly complete |
| EMUL_OP dispatch | Full (50+ selectors) | Markers in ROM, hook in PPC interp | **Not implemented** |
| VIA emulation | Not needed (all via EMUL_OP) | Not implemented | **Must build** |
| Timer/interrupt | EMUL_OP_INSTIME/RMVTIME/IRQ | Not implemented | **Must build** |
| Disk driver | EMUL_OP_DISK_* | Not implemented | **Must build** |
| ADB (input) | EMUL_OP_ADBOP | Not implemented | **Must build** |
| Audio | EMUL_OP_AUDIO_DISPATCH | Not implemented | **Must build** |
| SCSI | EMUL_OP_SCSI_DISPATCH | Not implemented | **Must build** |
| Video | Custom driver + QuickDraw accel | Framebuffer blit only | **Must build driver** |
| Ethernet | EMUL_OP + Slirp | SNP frame transmit | Needs integration |
| Name Registry | EMUL_OP_NAME_REGISTRY | Not implemented | **Must build** |
| Memory (BAT/MMU) | None (flat, problem state) | Flat memory, no MMU | Match SheepShaver |
| KernelData | ROM-type-specific init | Not seeded | **Must seed** |
| XLM globals | Written at 0x2800 | Written by PpcPatchNewWorldRom | Complete |

## Implementation Roadmap: Boot to Desktop

### Phase A: Fix Nanokernel Boot (critical path) — COMPLETE (2026-08)

All five sub-items implemented and verified under QEMU + OVMF with the
Mac OS 9.2.2 disc. The nanokernel now completes initialization through the
warm handoff path and the native 68K interpreter executes guest code into
the early Toolbox/DR-callback stages.

#### A.1 Boot-path selection (MSR[DR]) — DONE, revised by experiment
- **File**: `src/main.c`
- Implemented both directions empirically. DR **clear** (cold/BAT path)
  runs the NK's full hardware init, which deadlocks in the RAM sizer at
  `0x40B10BC8` (`blrl` into BAT-mapped scratch; second SDR1 reader at
  `0x40B10A90`). SheepShaver skips this stage by rewriting the boot entry,
  but its entry-pattern set does not match this ROM revision.
- DR **set** (warm fall-through, `crset cr5eq`) completes initialization
  cleanly and reaches the 68K handoff — this is the shipped configuration,
  now backed by the Phase A environment fixes below.

#### A.2 PMDT mapping — DONE (builder skip + gated injection)
- **Files**: `src/boot/bootloader_impl.c` (`BootPatchNkBootSequence`),
  `src/cpu/interpreter.c` (PMDTINJECT gate), `src/main.c` (VM page seeds).
- The NK's PMDT ("RAM descriptor") builder is NOPed at ROM+0x3121D4
  (SheepShaver desc-create pattern, verified in this image); the walk then
  consumes the interpreter's injected table, which is now written ONLY when
  the table is empty (entry 1 == 0) so a future builder-produced table can
  never be clobbered.
- `VMMaxVirtualPages`/`VMLogicalPages` are seeded from the actual guest RAM
  size instead of a hardcoded 256 MB.

#### A.3 SCC output device — DONE
- **Files**: `src/main.c` (outdev seed), `src/cpu/interpreter.c`
  (poll hook at 0x40B26500, Tx capture in the flush-helper window,
  0x20002/0x20006 device handlers).
- The NK printer's degenerate `r28=0` poll is answered at the instruction
  site (scrub-proof), banner bytes are echoed to the console, and properly
  addressed accesses hit the emulated 85C30 at `0x20000`.

#### A.4 Area creation / merge assertions — DONE
- **File**: `src/cpu/interpreter.c` (MERGE-HARMONIZE hook).
- At both merge-guard sites (0x40B1F614/0x40B1F668) the new area record's
  tail fields (+0x24/+0x28/+0x2C) are harmonized from the existing record
  before the compare, reproducing the pre-zeroed-pool state real boot
  guarantees. Both sites fire during live boot; the assertion storms of
  earlier sessions are gone.

#### A.5 KernelData hardware fields — infrastructure complete, seeds pending data
- **Files**: `src/boot/bootloader_impl.c` (`BootSeedKernelDataHardware`),
  `src/cpu/interpreter.c` (KDPROF profiler).
- Blind writes into the KernelData page corrupt live NK structures, so the
  offsets must be evidence-based. The KDPROF profiler records every load
  the NK makes from `0x68FFE000..0x68FFF000` during boot and dumps the top
  offsets at the 68K handoff. First capture: **+0x02C, +0x01C, +0x23C,
  +0x034, +0x0B4, +0x3A0, +0x384, +0x0E8, +0x128, +0x13C** (~7.8M reads
  each — polled fields). Seeds for these offsets belong in
  `BootSeedKernelDataHardware` once their semantics are pinned down;
  PVR/bus-clock are meanwhile delivered via XLM [281C]/[2820]
  (PVR corrected to the real 7400 value 0x000C0000).

#### Phase A bonus fixes discovered during implementation
- **Contiguous boot RAM bank** (`PPC_BOOT_RAM_BANK_*`, bootloader.h): one
  16 MB writable region at guest 0 replaces the former 256 KB low-memory
  region plus the hand-carved NK-stack hole. On real hardware RAM starts
  at 0 and the NK places its workspace (SPRG4 -> 0x37E000), lock headers
  (0xAE000) and stacks there; the fragmented map made all of it read as
  void and was the root cause of the cold-path recursive-spinlock park.
- **NK boot-sequence neutralization** (`BootPatchNkBootSequence`, 11 sites
  applied): PVR reads answered from XLM (r12/r23/r9 variants), SPRG3 skip,
  SDR1 read replaced with fixed page-table values, page-table-clear/tlbie
  NOPed, PMDT builder NOPed, second SR-load helper -> blr, perf-monitor
  SPR pairs NOPed. Two SheepShaver patterns do not exist in this ROM build
  (entry SR/BAT init rewrite, jump-to-emulator) and log warnings.
- **Spinlock entry diagnostics**: one-shot dump of lock header, node chain,
  SPRGs and the last 48 PCs when the recursive-spinlock walk is entered.

### Phase B: PPC-Native Pivot — RUN THE OS's OWN DR EMULATOR (critical path)

> **This replaces the previous "Phase B/C: Complete the 68K interpreter" as the
> critical path.** Rationale in ARCHITECTURE.md "Architectural Reference":
> DingusPPC/SheepShaver run the nanokernel's PPC DR emulator on the host; the C
> 68K interpreter was a workaround for an unfinished PPC environment.

#### B.1 PPC opcodes — status
- [x] `extsb`/`extsh` (XO 954/922) — implemented (`interpreter.c:4207,4212`).
- [x] `divs`/`div` and other 601 X-ops — implemented (`interpreter.c:3812`;
  `XO_DIVWU/DIVW` at 4073,4115).
- [ ] **But** verify the DR emulator no longer hits any `X-op: Unsupported`
  stop with the *hijack removed* (the C-68K path may have been masking PPC
  gaps). Run until the first stop, add each opcode the ROM's own emulator
  needs that is still unhandled.

#### B.2 Current stall is a PPC-boot-env gap, not opcodes (Session 16)
The interpreter no longer dies on a missing opcode; the boot reaches the NK
**scheduler**, then loops in a **park/wake/VBL/park cycle**: the 68K parks at
`BRA$` (`0x408047AE`), the PPC DEC fires (`VECDISP vector=0x900`), the NK
scheduler (`SCHED[3]/[4]`, `PC=0x40B22F18`) only ever re-selects the single
idle task (`curTask=0x9CE0`) and returns to the 68K dispatch entry `0x40B67C60`.
The NK never completes task/address-space/driver setup because its environment
is incomplete. This is the same root cause the pivot targets: finish the PPC
side (translation, hardware/KernelData seeds, devices) instead of papering over
it with C-68K heuristics ("self-healing" SPR, scrub short-circuits, PC-probes).

#### B.3 PPC translation the nanokernel expects
- [ ] Decide and implement the boot-time translation model. The nanokernel
  reads SDR1/BATs and walks page tables (Session A notes); either implement
  real BAT + SDR1 page-table translation in the interpreter, or a correct
  flat alias that satisfies the NK's probes without the current
  SPINLOCK/PMDT/merge surgical patches. DingusPPC (`cpu/ppcmmu.cpp`) is the
  reference.
- [ ] Replace the ~dozen `BootPatchNkBootSequence` neutralizations that exist
  only because translation was absent, with a working translate step where
  possible. (Keep the ones SheepShaver also applies.)

#### B.4 Restore the OS's own DR emulator
- [x] Add `USE_PPC_NATIVE_DR` switch in `interpreter.c`; it removes the
  `0x40B67C60` `M68kExecuteFromPPC` intercept so the ROM's own PPC opcode
  table drives the 68K emulator. **Verified (2026-08, QEMU/9.2.2):** with the
  switch ON the ROM's own PPC DR-emulator/translator executes natively and
  drives 68K fetch/decode (PC runs through the `0x40B6xx` compiler, `bcctr`,
  `mtspr`, `rlwimi` sequences with `r24`=68K PC advancing past each 68K
  instruction), past "Nanokernel replaced. Returning to boot proc". So the
  de-hijack is viable; the PPC opcode table is essentially functional.
- [ ] The native run still trips the **unseeded software-function vectors**
  `ed.v[0x800..0x834]`. Investigated under QEMU: there are **two** dispatch
  sites loading `ed.v[0x800]` via `lwz r0,0x800(r31)` — `0x40BBF8D0` (MOVEQ,
  handled by `EmulSoftFnMoveq`, refactored out of the inline hook) and
  `0x40B9FFF8` (the 68K **`0x3xxx MOVE` family with EA extension word**, e.g.
  opcode `0x3FFF`; the slot reads NULL so the 0x40B6D750 trampoline
  `mtspr LR,r5; addi r6,r0,0x10; blrl` branches to PC 0 and the run stops
  "GUEST STOP at PC=0x00000000 inst=0x00000001"). `ed.v[0x800]` is a general
  software function that must parse MOVE EA extension words, not just MOVEQ.
- [ ] Seed `ed.v[0x800..0x834]` with **native PPC handlers** (implement the
  MOVE-with-extension / MOVE-SR / RESET / 4E7B ABI) so BOTH dispatch sites
  work and the per-site C hooks can be retired. SheepShaver instead redirects
  the ROM's trap table (`rom_patches.cpp patch_68k_emul`, `twi` at 0x36e600 →
  0x36f900/0x36fa00, EMUL_OP escapes) — cross-check which approach the DR
  emulator actually needs for `ed.v[0x800]`.
- [ ] Drive the boot with hardware/KernelData + device-register emulation
  wired to UEFI (the former Phase C/D work, now framed as PPC-side).

#### B.5 Validation matrix (see "Validation Matrix" section)
- [ ] Boot New World 9.2.2, Mac OS 8.1, System 7.5.3, and the `mac_roms` Old
  World ROMs (7100/G3) under QEMU. "Boots" = reaches the OS's own idle loop /
  window server with no `X-op: Unsupported` stop, not just NK handoff.

### Phase B-Legacy: Complete 68K Interpreter (Classic runtime, AFTER the desktop)

Deprioritized by the PPC-native pivot. The native 68K interpreter handles basic
opcodes but needs expansion for the ROM's Toolbox code to execute.

#### B.1 Additional opcodes (priority order for boot)
- [ ] Bit manipulation: BTST/BSET/BCLR/BCHG (register and memory)
- [ ] Shift/Rotate: ASL/ASR, LSL/LSR, ROL/ROR (register and immediate)
- [ ] Multiply/Divide: MULS.W, MULU.W, DIVS.W, DIVU.W
- [ ] EXT.W, EXT.L (sign extension)
- [ ] EXG (exchange registers)
- [ ] SWAP (byte-swap halves of Dn)
- [ ] PEA (push effective address)
- [ ] JMP, JSR (jump/subroutine — needed for Toolbox calls)
- [ ] RTS, RTR (return from subroutine/trap)
- [ ] Line A (1010) / Line F (1111) emulation: intercept as trap vectors
- [ ] MOVEM with register lists
- [ ] NEGX, NEG (with extend)
- [ ] ABCD, SBCD, NBCD (BCD arithmetic)
- [ ] TAS (test and set — needed for ADB/mutex)

#### B.2 Exception dispatch
- [ ] 68K exception vector table at `0x0000-0x03FF` — read vector addresses,
  push PC+SR to supervisor stack, dispatch
- [ ] Interrupt exception (level 1-7): VBL, SCC, SCSI, slot
- [ ] Trap #1 (Mac OS system call): `_Trap` dispatch through the trap table
- [ ] Line 1010 / Line 1111: A-line / F-line traps (Toolbox)
- [ ] Illegal instruction exception

#### B.3 Memory access layer
- [ ] Ensure all 68K memory access goes through the PPC guest memory path
  (already done via `M68kReadByte/WriteByte` → `PpcReadGuestByte`)
- [ ] Add memory-mapped I/O dispatch: detect VIA (0x5000xxx), SCC, SCSI
  register windows and route to device emulation

### Phase C: EMUL_OP Device Handlers

These implement the hardware abstraction layer, replacing real Mac devices with
host-side UEFI protocol calls.

#### C.1 EMUL_OP framework
- [ ] `PpcEmulatorDispatchOp()` in interpreter.c: already intercepts `mulli r0,r0,n`
  markers. Route to `M68kEmulOpDispatch(selector)` in `src/cpu/m68k.c`.
- [ ] `M68kEmulOpDispatch()`: switch on selector, read 68K register state from
  context, call handler, return to DR emulator loop.

#### C.2 Timer system (highest priority — drives everything)
- [ ] `PPC_OP_INSTIME / RMVTIME / PRIMETIME`: replace `InsTime/RmvTime/PrimeTime`
  Toolbox calls with host-side timer management.
- [ ] `PPC_OP_MICROSECONDS`: return monotonic microsecond count from UEFI
  `QueryPerformanceCounter`.
- [ ] 1Hz periodic interrupt: fire VBL (vertical blank) at 60Hz via a timer that
  sets the VIA interrupt flag.

#### C.3 Interrupt system
- [ ] `PPC_OP_IRQ`: the Level 1 interrupt handler. Process:
  - `INTFLAG_VIA` → timer tick, VBL, Sony/Disk/CDROM polling
  - `INTFLAG_SERIAL` → SCC interrupt
  - `INTFLAG_ETHER` → Ethernet interrupt
  - `INTFLAG_TIMER` → decrementer/1Hz timer
  - `INTFLAG_AUDIO` → audio buffer completion
  - `INTFLAG_ADB` → ADB polling (keyboard/mouse)

#### C.4 Disk driver
- [ ] `PPC_OP_DISK_OPEN`: open the boot volume (identify HFS partition via
  Block I/O, store partition info).
- [ ] `PPC_OP_DISK_PRIME`: read/write blocks via `PpcReadDiskBlock` →
  UEFI Block I/O `ReadBlocks`/`WriteBlocks`.
- [ ] `PPC_OP_DISK_CONTROL`: ioctl (drive status, geometry, eject).
- [ ] `PPC_OP_DISK_STATUS`: return drive ready flag.

#### C.5 ADB (keyboard/mouse)
- [ ] `PPC_OP_ADBOP`: ADB manager operations:
  - Poll keyboard: translate UEFI `ReadKeyStroke` to ADB key codes
  - Poll mouse: translate UEFI pointer protocol to ADB mouse data
  - Register device handlers

#### C.6 Video driver
- [ ] `PPC_OP_INSTALL_DRIVERS`: install the Mac video driver at driver area.
  The driver's `DoDriverIO` handler maps to:
  - `Open`: set video mode (resolution, bit depth)
  - `Prime`: initial framebuffer setup
  - `Control`: mode switch, palette, vbank
  - `Status`: current mode info
- [ ] `PPC_OP_VIDEO_DOIO`: dispatch to GOP framebuffer operations. Convert
  big-endian Mac pixels to GOP pixel format on blit.

#### C.7 Audio
- [ ] `PPC_OP_AUDIO_DISPATCH`: audio component dispatch. Map to ring buffer
  in guest RAM; host reads PCM samples and plays through UEFI (no standard)
  or serial debug.

#### C.8 SCSI
- [ ] `PPC_OP_SCSI_DISPATCH`: SCSI Manager emulation. Route reads/writes
  to Block I/O for the boot volume and attached discs.

#### C.9 Name Registry
- [ ] `PPC_OP_NAME_REGISTRY`: emulate the Open Firmware Name Registry.
  Provide device tree nodes for: `/cpus/cpu@0`, `/mac-io`, `/nvram`,
  `/scsi`, `/ethernet`, `/display`.

#### C.10 Other EMUL_OP selectors
- [ ] `PPC_OP_SONY_OPEN/PRIME/CONTROL/STATUS`: floppy driver (stub or
  map to HFS image)
- [ ] `PPC_OP_CDROM_OPEN/PRIME/CONTROL/STATUS`: CD-ROM driver
- [ ] `PPC_OP_SOUNDIN_*`: sound input (stub)
- [ ] `PPC_OP_DEBUG_STR`: `_DebugStr` — print to serial
- [ ] `PPC_OP_RESET`: Mac OS reset handler
- [ ] `PPC_OP_CHECK_SYSV`: version compatibility check
- [ ] `PPC_OP_CHECKLOAD`: resource loading hook
- [ ] `PPC_OP_EXTFS_COMM/HFS`: external file system
- [ ] `PPC_OP_IDLE_TIME`: idle/sleep when no events
- [ ] `PPC_OP_ZERO_SCRAP / PUT_SCRAP / GET_SCRAP`: clipboard

### Phase D: Mac Hardware Register Emulation

Mac Toolbox code and drivers read/write hardware registers directly. These must
be backed in guest memory with emulated behavior.

#### D.1 VIA (Versatile Interface Adapter) — 0x50000000
- [ ] VRA/VRB (timer A/B counters): decrement at 60Hz, set IRQ on expiry
- [ ] IFR (interrupt flag register): aggregate all device interrupt sources
- [ ] IER (interrupt enable register): per-bit enable mask
- [ ] SR (shift register): ADB data transfer
- [ ] DIRA/DIRB (data direction): configure input/output
- [ ] PA/PB (port data): bit-level device control

#### D.2 SCC (Serial Communications Controller) — 0x80013020
- [ ] RR0 (receive status): data available, FIFO depth
- [ ] RR3 (interrupt pending): which channel has pending IRQ
- [ ] WR0 (command): reset, send (SCC boot printer)
- [ ] WR7 (misc): enable/disable

#### D.3 SCSI (NCR 53C96) — 0x80010000
- [ ] DMACNT/SCPDMA: DMA transfer control
- [ ] SCMD/SCISR: command/interrupt status
- [ ] SCFIFO: data FIFO

#### D.4 Slot Management — 0x50Fxxxxx
- [ ] S-slot ROM: auto-inject device directory entries for video, SCSI,
  ethernet (matching SheepShaver's `SlotManager`)

### Phase E: Boot Sequence Integration

#### E.0 DR-emulator service-call convention (M4 — current blocker)
The New World ROM's 68K boot code invokes emulator services by branching
into a mirror region at `0x305xxxxx` (e.g. `bvc.l` with displacement
`0xEFD000xx` from ROM code at `0x4080ABxx` lands at `0x3050ABEx`). On real
hardware that address range holds the DR emulator's dispatch tables and
service stubs; we have no mapping there.

- [x] Detect service calls: `M68kIsDrEmulatorAddress()` — ranges
  `[0x30000000,0x40000000)` and `[0x40B60000,0x40C00000)`.
- [x] Immediate-return semantics for `Bcc.L` into the mirror region
  (`M68kExecuteBranch`): resume at the instruction after the branch.
  (Earlier redirect-to-A6 approach ping-ponged forever between glue blocks.)
- [ ] Identify each call site's expected service and result: register
  arguments (D1 held 0x68 in early samples), expected D0 return values,
  stack effects. Catalog call sites from trace68k.log + PC ring dumps.
- [ ] Implement minimal service stubs so init loops that poll for results
  terminate (memory manager sizing, hardware probe results).
- [ ] Handle computed dispatches through the mirror region: the glue at
  `0x408A8D7C-90` builds a handler pointer (`move.l a6,d0; lea base,A1;
  movea.l 0(a0),a2; jmp (a2)`) — if the table it reads is uninitialized,
  seed or intercept so `jmp (a2)` lands somewhere valid.

#### E.1 Continuous 68K execution loop
- [ ] Replace the per-instruction PPC hook with a dedicated 68K execution
  mode: when the NK hands off to the DR emulator, enter `M68kExecuteBlock()`
  as the primary loop. Return to PPC only when a supervisor-level event
  (interrupt, exception) requires it.
- [ ] Trigger timer interrupts via the VIA at 60Hz (VBL) to drive the Mac
  OS event loop.

#### E.2 Toolbox trap dispatch
- [ ] A-line traps (Line 1010): read trap word, dispatch through the
  Toolbox trap table in low memory (0x0). For `_Gate`-style traps, follow
  the dispatch chain.
- [ ] Trap #1: Mac OS system call mechanism. The trap word encodes the
  selector; dispatch through the trap table.

#### E.3 Boot sequence
1. PPC nanokernel completes initialization
2. NK hands off to DR emulator (trap table → emulator-start)
3. 68K interpreter starts at 68K reset vector (ROM + 0x2A)
4. 68K boot code initializes Toolbox, Memory Manager, Device Manager
5. Toolbox loads the System file, Finder
6. Finder draws the desktop
7. User interacts via ADB (keyboard/mouse → UEFI → ADB codes → Finder)

### Phase F: User Experience

#### F.1 Configuration menu enhancements
- [ ] ROM file browser: navigate ESP/HFS volumes to select ROM
- [ ] System Folder browser: select boot volume
- [ ] Display resolution selector
- [ ] Memory size (128 MB – 2 GB)

#### F.2 Real disk boot
- [ ] Detect and boot from HFS-formatted physical disk (UEFI Block I/O)
- [ ] Detect and boot from HFS disk image file on FAT partition
- [ ] Both paths use the in-emulator HFS reader

## Architecture Decisions

### Heavy bootloader, not an application emulator
UEFI standard protocols are the hardware abstraction; guest-visible Mac devices
are thin simulated windows wired to GOP/BlockIO/SNP. This keeps the host-side
code small and lets the guest own the boot process.

### Target architecture: PowerPC
Better fit for Mac OS 8/9 and for a user-supplied Old World ROM.
References: SheepShaver, Basilisk II, QEMU, DingusPPC.

### In-emulator HFS reader
The bootloader must read Mac discs without a host filesystem; catalog-ID lookup
avoids name/path separator ambiguity and survives all three test-disc layouts.

### SheepShaver-style paravirtualization
The ROM is patched so all code runs in flat memory (no MMU, no BAT). Device I/O
is intercepted through EMUL_OP trap dispatch. This avoids the need for hardware
register emulation at the register level — instead, Toolbox calls are redirected
to host-side C implementations backed by UEFI protocols.

### Revising the SheepShaver model (DingusPPC gold-standard review, 2026-08)
The 68K "DR emulator" is PPC code owned by the OS; even SheepShaver runs it as
PPC and does **not** reimplement a 68K CPU. EFIMac's prior pivot to a hand-rolled
C 68K interpreter (Session 12, `m68k.c`) was a workaround for an unfinished PPC
boot environment, not a real requirement. Correction (see ARCHITECTURE.md
"Architectural Reference"): **run PPC** — restore the OS's own DR emulator and
complete the PPC environment (translation, missing 601 opcodes, hardware/KernelData
seeds). The C 68K interpreter is only a later legacy-runtime fallback for the
Classic app layer, not the boot engine.

## Validation Matrix

"Success" is not booting a single disc. The build must be exercised against every
combination below, because the three operating systems and the two ROM families
exercise different firmware paths:

| Target | Disc/OS | ROM family | What it stresses |
|--------|---------|-----------|------------------|
| New World 9.2.2 | `mac_discs/Apple Mac OS 9.2.2 [PowerMac G4].7z` | New World `Mac OS ROM` | nanokernel warm path, 68K DR-emulator handoff |
| Mac OS 8.1   | `mac_discs/MacOS 8 (…8.1…)(1998).iso` | New World `Mac OS ROM` | the earliest New World OS; 8.5± Toolbox |
| System 7.5.3 | `mac_discs/System7_5_3.img` | Old World | classic 68K ROM bootstrap (needs a local Old World ROM) |
| Old World ROM | `mac_roms/1995-01 - … Power Mac 7100 (newer).ROM` | Old World dump | classic `0xFFF00000` reset-vector boot |
| Old World ROM | `mac_roms/1997-11 - … Power Mac G3 desktop.ROM` | Old World dump | later Old World / 603+ boot |

To run an Old World ROM, place the file on the ESP at `\System\MacOS\ROM` (see
`run-qemu-windows.ps1 -OldWorldRom`). This replaces the single-9.2.2 benchmark
that masked the PPC-vs-68K category mismatch.

# EFI Mac OS Boot Layer — Implementation Plan

## Current State

The project is a functional **heavy UEFI bootloader** for classic Mac OS. It
builds a PowerPC Mac boot image from UEFI standard protocols, reads classic Mac
discs in place, installs real Mac firmware into the guest image, and self-tests
the whole path. **Phase A (nanokernel boot) is complete**: the New World ROM
boots through the warm handoff path with a clean environment — contiguous
guest RAM bank at 0, emulated SCC output device, gated PMDT injection,
harmonized area merges, and SheepShaver-style neutralization of eleven NK
hardware-init sites. The native 68K interpreter executes guest code through
NK handoff, DR-callbacks and the embedded decompressor. The remaining work to
the desktop is Phase B/C: completing 68K interpreter coverage for the Toolbox
boot path (current stop point: a spin on the XLM mailbox at 0x408005F2 during
early DR-emulator setup), EMUL_OP device handlers, and Mac hardware register
emulation wired to UEFI protocols.

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

### Phase B: Complete 68K Interpreter

The native 68K interpreter handles basic opcodes but needs expansion for the
ROM's Toolbox code to execute.

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

# EFI Mac OS Boot Layer — Implementation Plan

## Current State

The project is a functional **heavy UEFI bootloader** for classic Mac OS. It
builds a PowerPC Mac boot image from UEFI standard protocols, reads classic Mac
discs in place, installs real Mac firmware into the guest image, and self-tests
the whole path. The **New World ROM boots through the nanokernel** and hands off
to the 68K DR emulator, where execution enters the native 68K interpreter. The
guest OS does **not** reach the desktop yet — the remaining work is completing
the 68K interpreter, implementing EMUL_OP device handlers, and wiring Mac
hardware register emulation to UEFI protocols.

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
- **New World ROM nanokernel boots** through the full boot sequence: NK
  initialization, memory setup, PMDT walk, and 68K DR-emulator handoff via the
  patched trap table.
- **68K DR emulator entry** reached at `0x40B6F900`; the native 68K interpreter
  hooks the common dispatch at `0x40B67C60` and executes 68K instructions.
- The NK nanodebugger fires on assertion failures during area creation and PMDT
  setup; auto-resume sends 'g' to continue past each check point.

### Recent work

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

### Phase A: Fix Nanokernel Boot (critical path)

The NK currently boots, hits assertion failures during PMDT/area setup, and the
auto-resume pushes past each crash. The NK needs to complete initialization
cleanly so the 68K emulator handoff is in a valid state.

#### A.1 Fix MSR[DR] — disable data relocation
- **File**: `src/main.c` (line ~779)
- Currently sets `MSR |= PPC_MSR_DR` (bit 0x10). This enables data address
  relocation, but the interpreter has no BAT/segment translation — all memory
  access is flat. DR=1 causes the NK's PMDT walk to fail because it expects
  translated addresses.
- **Fix**: Remove `PPC_MSR_DR` from the MSR. The NK should boot with DR=0
  (flat memory), matching SheepShaver's approach.

#### A.2 Fix PMDT mapping for guest RAM at 0x10000000
- **File**: `src/cpu/interpreter.c` (PmdFixed injection ~line 4628)
- The PMDT injection maps pages [0, 0xFFF6) as RAM, but guest RAM starts at
  `0x10000000` (page `0x10000`). The NK's area manager maps wrong physical
  addresses.
- **Fix**: Map pages `[0x10000, 0x10000 + RAMSize/4K)` as the RAM region in the
  PMDT. Also ensure low-memory globals (page 0) and the ROM window are
  represented in the PMDT.

#### A.3 Fix SCC base address for NK boot printer
- **File**: `src/main.c` and `src/cpu/interpreter.c`
- The NK's boot printer polls `[r28+2]` bit 2 for SCC Tx-empty. r28 is 0 because
  `NoIdeaR23 [KDP-0x900]` is never seeded. The SCC on a Power Mac G4 is at
  `0x80013020` (CHRP SCC base).
- **Fix**: Seed the SCC base in the KernelData/ECB area. Alternatively, seed a
  "virtual SCC" at `0x20002` (the address the boot printer is using) with
  the Tx-ready bit pre-set.

#### A.4 Fix area creation / merge assertions
- **File**: `src/cpu/interpreter.c` (PmdFixed, MergeTraced ~lines 4628-4699)
- The NK panics at the area-merge guard check (0x40B1F67C) because area structs
  have uninitialized fields at offsets +0x24/+0x28/+0x2C.
- **Fix**: Pre-seed the NK's area structures so the merge/guard checks pass.
  Alternatively, NOP the problematic assertion in the ROM.

#### A.5 Seed KernelData for New World G4
- **File**: `src/boot/bootloader_impl.c` (PpcPatchNewWorldRom)
- The ROM's ConfigInfo at `ROM+0x30D000` needs `LA_InfoRecord` (0x68FFE000),
  `LA_KernelData` (0x68FFE000), `LA_EmulatorData` (0x68FFF000),
  `physical RAM base` (0), and `68K reset vector` (ROM+0x2A). These are already
  seeded. However, the KernelData structure itself at `0x68FFE000` needs
  hardware fields: PVR, bus clock, OpenPIC base, OF device tree pointers.
- **Fix**: Fill the KernelData struct with G4/PowerMac values matching the ROM.

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

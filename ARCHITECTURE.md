# EFIMac — Heavy Bootloader for Classic Mac OS — Architecture

This document describes how the project actually works today: a heavy UEFI
bootloader that stages a classic PowerPC Mac boot environment from UEFI standard
protocols, then executes the installed firmware and the OS it boots on a
PowerPC interpreter. Design decisions are noted as they were made.

> EFIMac is a *bootloader*, not an emulator. The guest firmware runs for real in
> a continuous interpreter loop; UEFI protocols act as the x86_64 hardware layer
> underneath the guest's expected device windows.

## Overview

Classic Mac OS (System 7 through Mac OS 9) boots from firmware that owns a
PowerPC Mac: a read-only ROM window at `0xFFF00000`, low-memory system globals
at `0x0`, a boot volume containing the System Folder, and hardware devices.
This project supplies that firmware-side environment as an EFI application:

1. **UEFI is the hardware.** GOP is the display, Block I/O / Simple File System
   is storage, Simple Network Protocol is the network, and UEFI pool/allocation
   services are memory. Simulated Mac devices (framebuffer, audio ring) are
   buffers inside guest RAM that the bootloader copies to/from the real UEFI
   devices.
2. **A classic Mac boot volume is read in place.** An in-emulator HFS reader
   parses the attached disc directly (no host mount required), so the bootloader
   works on raw floppy/disc images as QEMU would see them.
3. **Firmware is installed into the guest image.** A ROM is loaded (boot-volume
   path, ESP file, or HFS `Mac OS ROM` discovery), mapped read-only into the
   guest map, and identified as Old World / New World / demo.
4. **A continuous PowerPC interpreter executes the guest.** A fetch-decode-step
   loop runs the installed firmware for real against a multi-region guest memory
   map: fixed 32-bit opcodes, GPR/SPR/FPU state, big-endian loads/stores with
   read-only ROM enforcement, a timebase/decrementer scheduler tick, and
   exception delivery through the firmware's own vector table (`VecTbl` in
   SPRG3).

## Boot Flow

```
efi_main (src/main.c)
  PpcInitializeUefiInterface      # LoadedImage, Boot Services, console
  PpcInitializeDebug              # boot.log + monotonic timer
  PpcInitializeTranslationContext # PPC register file, MSR/SRR0/1/CTR/LR
  PpcRunSelfTest                  # 35 checks incl. FPU core
  PpcInitializeMemoryManager      # 256 MB guest RAM @ 0x10000000
  PpcSetGuestMemory               # wire UEFI pages into interpreter
  PpcInitializeHardwareAbstraction# GOP, Block I/O, SNP, audio ring
  PpcInitializeBootloader
  PpcSetupBootEnvironment
  PpcInitializeGraphics           # GOP mode + guest framebuffer window
  PpcInstallLowMemory             # 16 KB globals @ 0x0
  PpcInstallSystemRom             # \System\MacOS\ROM -> HFS "Mac OS ROM" -> demo
  PpcPrepareSystemForBoot         # PC = entry, MSR, boot info block
  PpcLocateSystemFolder / PpcLoadSystemFiles / PpcScanExtensionsDirectory /
    PpcLoadDrivers                # stage System, Finder, Mac OS ROM, Extensions
  PpcBootFirmware                 # continuous PpcRunGuest loop (see below)
```

## ROM Sourcing and Types

Priority order (implemented in `PpcInstallSystemRom` /
`PpcLoadSystemRom` / `BootLoadHfsRomToPages`):

1. `\System\MacOS\ROM` on the boot volume — an Old World firmware dump
   (System 7 needs one of these).
2. `\System Folder\Extensions\Mac OS ROM` on the ESP.
3. `Mac OS ROM` found anywhere on an attached Mac disc via
   `PpcHfsFindMacOsRom` (whole-catalog search, largest non-empty match). This is
   how a real Mac OS 9.2.2 install disc yields its 2,763,530-byte New World ROM
   from `Power Mac G4 Install:System Folder:Mac OS ROM`.
4. `PpcInstallDemoRom` — a 4 MB self-contained image with a reset-vector
   program, used to keep the full install + self-test path alive without
   firmware.

`BootIdentifyRomType` classifies by signature: a leading `<CHRP-BOOT>\r` means
New World (PPC, Mac OS 8.5+), otherwise Old World, and the guest boot-info block
records the type.

## Guest Memory Map

Managed by `PpcAddGuestMemoryRegion` (multi-region map in the interpreter,
read-only flag per region). Regions the firmware sees once booting:

| Region              | Guest address | Size       | Access |
|---------------------|---------------|------------|--------|
| Low-memory globals  | `0x00000000`  | 16 KB      | R/W    |
| Guest RAM           | `0x10000000`  | 256 MB     | R/W    |
| Framebuffer window  | `0x18000000`  | 640x480x32 | R/W    |
| Audio ring buffer   | `0x18800000`  | 8 KB       | R/W    |
| System area         | `0x20000000`  | 16 MB      | R/W    |
| Driver area         | `0x21000000`  | 32 MB      | R/W    |
| System ROM         | `0xFFF00000`  | 4 MB       | R (ROM) |

During a real boot the nano-kernel runs with `KDP = 0xA000` (also SPRG0/SPRG4),
the scheduler/DR working set lives around `0x40B00000` (firmware overlays in the
`0x40B00000..0x40B7xxxx` band), and the SCC console device is at `0x20000`.
This is a *host-defined* environment, not a byte-for-byte real Mac; the goal is
that the firmware's own environment expectations (low-memory globals layout,
KDP, VecTbl, SCC) are met so the ROM behaves as it would on hardware.

## In-Emulator HFS Reader

`src/fs/hfs.c` parses classic HFS volumes without the host mounting them:

- **Block-size auto-detection** so raw floppy images (512 B blocks), CD ISO
  images (2048 B blocks), and HFS-with-2-KB-cluster layouts all work.
- **Catalog-based lookup** (`PpcHfsGetEntryById`) that resolves files by their
  catalog FlNum/DirID, so names containing `/` or `:` are handled correctly and
  are not ambiguous with path separators.
- **Extent handling** with multi-overflow-extent support for large files.
- Used by the System Folder probe (`PpcHfsProbeBootFiles`), the whole-catalog
  `Mac OS ROM` search, and driver enumeration (`BootEnumerateExtensionsHfs`).

The reader has been exercised against System 7.5.3 (raw HFS image), Mac OS 8.1
(ISO with non-zero block base), and Mac OS 9.2.2 (ISO with multi-overflow
extents).

## System Folder and Driver Staging

- `PpcLocateSystemFolder` finds the System Folder on the boot volume (ESP FAT or
  attached HFS disc) and detects System, Finder, Extensions, and the Mac OS ROM.
- `PpcLoadSystemFiles` stages System and Finder (empty files are skipped; System
  7.5.3's `Finder` is a genuine 0-byte stub) into the system area at
  `0x20000000`.
- `PpcScanExtensionsDirectory` / `PpcLoadDrivers` enumerate and stage up to 64
  Extensions (drivers) into the driver area at `0x21000000`; empty data forks
  are skipped. Every non-empty extension stages with 0 failures on the test
  discs (7.5.3: 2/2, 8.1: 18/18, 9.2.2: 25/25).

## Device Simulation on UEFI

- **Graphics:** `PpcInitializeGraphics` selects a GOP mode and carves a
  640x480x32 guest framebuffer window. Guest code writes big-endian `0xRRGGBB00`
  pixels there; `PpcGraphicsBlitToDisplay` converts and copies to the real GOP
  framebuffer (byte-exact for RGB and BGR layouts). Verified with full-screen
  solid frames checked pixel-by-pixel on the GOP buffer, band boundaries,
  corners, and out-of-bounds write rejection.
- **Storage:** `PpcInitializeBlockIo` enumerates every Block I/O handle and
  reports real geometry; `PpcReadDiskBlock` issues real `ReadBlocks` calls. The
  HFS reader is layered on top of this.
- **Network:** `PpcInitializeNetwork` starts and initializes every Simple
  Network Protocol interface, snapshots real mode (MAC, media state), and
  transmits a real frame via `Transmit`/`GetStatus`.
- **Console (SCC):** the guest console device is emulated as a Zilog 8530 SCC at
  guest `0x20000`. `[base+2]` reads status (`0x04` Tx-buffer-empty, bit 0
  Rx-data-ready), `[base+6]` reads/receives data; puts from the UEFI console
  queue bytes into an Rx FIFO that raised `RxDataReady`. The boot console drains
  this during the banner flush. A host-to-guest console input path
  (`PpcSccPutChar`) feeds the queue.
- **Audio:** no UEFI audio standard exists, so the device is a fixed ring buffer
  in guest RAM; the host reads PCM samples back and advances a play cursor.

## PowerPC Interpreter and Guest Execution

`src/cpu/interpreter.c` decodes and executes fixed 32-bit big-endian PowerPC
opcodes with a register file (32 GPRs, CR, CTR, LR, MSR, SRR0/1, FP registers +
FPSCR), big-endian guest memory access, FPU core (opcodes 48-63, gated on
MSR[FP] with the FP-unavailable exception), and a continuous fetch-execute loop.

Implemented guest-facing behavior:

- **Timebase/decrementer tick.** Each instruction advances `TBL/TBU` and `DEC`
  by `PPC_TIMEBASE_SCALE`; a negative DEC with `MSR.EE` set and the NK's
  `mtspr`-armed decrementer raises the decrementer exception. The NK scheduler
  run-loop, park/wake, and `g_BootDecGate` deferral policy are handled in this
  loop so the boot-tail handoff is never preempted before the stack is
  established.
- **Exception delivery through the firmware's vector table.** The firmware's
  `VecTbl` base is read from SPRG3; entries for vectors `0x100..0xF00` are
  dispatched as the firmware's own firmware-vector stubs would, including the
  interrupted `r1`/`r7`/LR deposit into `[KDP-0x4]` / `[KDP-0x10]` / SPRG1/2 the
  NK's `InterruptSave` expects. Installed vectors seen in real boots:
  `0x500 external 0x40B14880`, `0x700 program/KCall 0x40B14700`, `0x900
  decrementer 0x40B13200`, `0xC00 syscall 0x40B14AC0`, etc.
- **Device windows.** Loads/stores outside guest regions decode as device
  accesses; the SCC at `0x20000` is one (see above). Unmapped reads return 0.
- **Fabricated external interrupt through the D0 level-9 dispatch.** Because the
  guest's IRQ routing (`[KDP-0x824]`) is not set up at the stalled phase, the
  emulator raises the EXT vector itself, right at the idle give-up
  (`0x40B24F04`, EE cleared in the sell phase), and lets the firmware's own D0
  handler run: KCALLSAVE (`0x40B13D40`) → level-9 ISR (`0x40B148E0`) → the ISR's
  dispatch rfi (`0x40B14A04`). To make the dispatch land correctly the emulator
  seeds the interrupt dispatch tables the guest's code reads: A-table
  `0x6C00`/P-table `0x6D00`/stack-table `0x6D20` (`B[0]=0xA000`), `P[0] = Next`
  (the preempted task's sell-glue continuation `0x40B24F08`), the IC mirrors
  (`[IC+0x20]=3` events, `[IC+0x24]=1`, `[IC+0x38]=2`, `[IC+0x44]=0x3FFFFFFF`,
  `[IC+0x4C]=0x75C0`) and `[KDP-0x338]=0x81C0`. The ISR's SRR1 source slot
  (`0x40B1499C`) is not stable on this harness, so the emulator forces
  `SRR1=0x9002` (EE on) on every fabricated dispatch rfi. This yields a clean,
  repeatable task switch into the resumed give-up — stable over 180 s soaks.
  It does not yet advance boot (the resumed idle task re-sells forever); see
  Open Work.
- **Instrumentation.** The interpreter carries a debug logging path
  (milestone prints, probe lines, a 4096-entry PC/register tail ring for
  pre-panic trace) used heavily to chase the boot; several probe families are
  capped/log-only so production boots stay deterministic.

The guest currently runs the firmware on a **flat alias**: there is no
BAT/SDR1 translation model implemented yet, so firmware that would enable
translation runs on the direct map. This is acceptable while the nano-kernel
boots; implementing the real MMU model remains for later work.

## Build and Run

See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for the clang/lld-link
GNU-EFI cross-build (Windows git-bash script or macOS `make`) and
[USER_GUIDE.md](USER_GUIDE.md) for the QEMU/OVMF boot and disc attachment.

## Architectural Reference: DingusPPC and the PPC-Native Correction

The original plan modeled this project on SheepShaver's **paravirtualization**
(see "SheepShaver Style Paravirtualization" below). An end-to-end review against
**DingusPPC** — the current gold-standard classic-PowerPC emulator — has driven
a fundamental correction to the boot strategy.

### What DingusPPC establishes

- **DingusPPC is pure PowerPC.** It has *no* 68K interpreter. Its `cpu/` tree is
  entirely PowerPC, and it implements a **real MMU** (BATs + SDR1 page tables).
- On **New World** machines, Mac OS 8.5/9 boots almost entirely on PPC. The 68K
  "DR emulator" (the Toolbox 68K emulator) is itself **PPC code the OS keeps in
  its `Mac OS ROM`**; the host CPU runs it natively as PPC. It is *not* a
  separate component the host must supply.
- DingusPPC loads the guest's own 1 MiB `Mac OS ROM` (`tbxi`) directly at
  `0xFFF00000` and lets the nanokernel + DR emulator run as PPC. Its
  fully-working desktop machines are Old World; the classic 68K Toolbox layer is
  served by the OS's own emulator running on the PPC core.

### The correction

EFIMac's early pivot (Phase B/C, Session 12) wrote a hand-rolled **C 68K
interpreter** (`src/cpu/m68k.c`) and *hijacked* the nanokernel's 68K DR-emulator
dispatch (`PpcRunGuest` interceptor at guest `0x40B67C60`) to run it instead of
the OS's own PPC DR emulator. The rationale was "the ROM's built-in PPC DR
emulator crashes because internal structures are not fully set up." That is a
**PPC boot-environment gap**, not a reason to reimplement the 68K CPU.

The consequence has been weeks of symptom-patching: stack-scanning return-address
"self-healing", scrub short-circuits, wedges/escapes/rescues, dozens of
PC-address-specific probes, and ~200 one-off Python tools — all fighting a battle
that a correct PPC environment should make unnecessary.

**The corrected direction (PPC-native pivot):**

1. **Stop replacing the OS's own DR emulator.** Remove / neutralise the
   `0x40B67C60` 68K-dispatch hijack and let the nanokernel's PPC DR emulator run
   like on real hardware and under DingusPPC.
2. **Complete the PPC environment** so that emulator survives: implement the
   PPC **translation model** the nanokernel expects (real BAT/SDR1 or a
   correct flat alias — the missing 601-era opcodes `extsb`/`extsh`/`divs`
   that Session 10/11 hit are now implemented; the remaining block is
   translation, hardware/KernelData seeds and device registers), so the NK
   scheduler can finish task/address-space/driver setup instead of idling in
   the park/wake/VBL loop at `BRA$` (Session 16). These are genuine
   PPC-environment gaps, not 68K problems.
3. Treat the C 68K interpreter as a **legacy runtime-only fallback** (for the
   Classic 68K app layer after the desktop is reached), not as the boot engine.

Directly actionable: the DR-emulator opcodes that once stalled the interpreter
(`extsb`/`extsh` XO 954/922, `divs`/`divw` and the 601 X-ops) have since been
implemented; the boot now reaches the NK scheduler and idles in a
park/wake/VBL cycle (Session 16) still because the *environment* is incomplete —
translation, hardware/KernelData seeds, and device registers. Finishing those
PPC-side pieces is the path, not more C-68K heuristic patching.

### Staged PPC DR bootstrap — reverse-engineered DR mechanics (2026-08-31)

The chosen direction (user-selected) is the **Staged PPC DR bootstrap**: let the
guest ROM's own PPC DR emulator run the 68K boot stub, then chase and fix each
PPC-side gap the DR throws, rather than boot through a C-68K interpreter. C-68K
is fully removed. Reverse-engineering of the ROM's DR (guest `0x40B60000` region)
produced these locked-in facts used by the fixes:

- **DR ABI (cold start).** Enter the DR at `0x40B6E964` with `r31=0xB000` (memory
  base gd), `r29=0x40B80000`, `r30=0x40B60000`, `lowmem[0]=0xA000`,
  `lowmem[4]=0x4080002A`. DR interface registers: `r24`=68K PC, `r27`=prefetched
  opcode, `r25`=SR (0x27), `r23`=0.
- **r28 ABI (new).** `r28` is the DR's host-supplied memory-base register for its
  PC-relative effective-address operator. The DR body never writes r28 (after
  cold init `addi r28,r0,0` @0x40B6E9BC); the correct value is **gd = 0xB000**.
- **PC-relative EA handler.** ed.v[0] = `0x40B6D780` enters the PC-relative
  EA/branch handler. It backs `r24` up to the referring instruction's extension
  word, then reads the *reference PC* from `[r28 + (r6&~7)]` = `[0xB010]` via
  `lwzx r24,r28,r7` @0x40B6D7D8, then adds the displacement (so `LEA (8,PC),A6`
  resolves when A6's ref PC is seeded; confirmed A6=0xC6). The DR never writes
  that slot, so it is 0 by default — our REFSLOT patch seeds `[0xB010]=r24` just
  before the lwzx.
- **DR 68000 core limit: `0x60FF` (68020 BRA.L).** The embedded DR is a 68000
  core and has no 32-bit branch displacement support, so it spins at the boot
  stub's `0x60FF` (68K PC `0x408000C0`). Fixed by redirecting, at the DR
  dispatch-home range (`0x40B67A00-0x40B67C80`) when the DR settles on a ROM
  `0x60FF`, `r24` to `PC+2+d32` exactly as 68020 would. Log-verified the DR then
  reaches the A-line trap dispatch at `0x4080AA10`.

## Open Work

See [TODO.md](TODO.md). Current direction (user-selected), in order:

1. **Advance boot past the idle sell.** The fabricated EXT dispatch (above) is
   stable but resuming the idle give-up re-sells forever. Candidate fixes under
   investigation, in order of preference: (a) trace the give-up's wake-event
   mask and the warm/CSR branch at `0x40B126E8` to deliver the exact event the
   NK "Resuming" tail blocks on (candidate: a DEC/1 s timeout or an unemulated
   KDP service); (b) pivot the dispatch target to the console/shield task's
   real SCC dequeue (`0x40B26548`/`0x40B263E0`) with its saved GPR context
   restored; (c) emulate real give-up wake semantics in the `0x2E` stub
   (block, check the pending-event mask, return woken/nonzero when an event is
   queued).
2. **Machine-level interrupt machinery.** Once the event that unblocks boot is
   identified, implement the classic PowerPC interrupt controller (Grand
   Central / VIA-style device IRQ sources) and route the SCC Rx interrupt into
   the nano-kernel's external (`0x500`) vector instead of a fabricated raise.
   Reference implementations: DingusPPC (`cpu/grandcentral.c`,
   `devices/psa/..`, SCC model) and SheepShaver's IRQ model.
3. **UEFI <-> PPC interface layer.** Treat UEFI protocols as the guest's device
   layer: I/O, interrupts, memory, and display plumbing between the x86_64/UEFI
   host and the guest firmware environment.
4. **MMU/translation.** Implement the firmware's BAT/SDR1 translation handshake
   (or a correct flat alias) so firmware that arms translation proceeds as on
   hardware.
5. **Validate the matrix.** New World 9.2.2, Mac OS 8.1, and the `mac_roms`
   Old World ROMs end-to-end. Legacy 68K support stays as a runtime-only
   fallback for the Classic app layer, not a boot path.

# EFIMac — Architecture

This document describes how EFIMac actually works today: a heavy UEFI
bootloader that stages a classic PowerPC Mac boot environment from UEFI standard
protocols and then executes the guest's own firmware and OS on a PowerPC
interpreter.

> EFIMac is a *bootloader*, not an emulator. The guest firmware runs for real in
> a continuous interpreter loop; UEFI protocols act as the x86_64 hardware layer
> underneath the guest's expected device windows.

## Overview

Classic Mac OS (System 7 through Mac OS 9) boots from firmware that owns a
PowerPC Mac: a read-only ROM window, low-memory system globals at `0x0`, a boot
volume containing the System Folder, and a set of hardware devices. EFIMac
supplies that firmware-side environment as a single EFI application:

1. **UEFI is the hardware.** GOP is the display, Block I/O / Simple File System
   is storage, Simple Network Protocol is the network, and UEFI allocation
   services are memory. The simulated Mac devices the guest expects
   (framebuffer, SCC console, audio ring) are buffers that the bootloader copies
   to and from the real UEFI devices.
2. **Real Mac media is read in place.** An in-emulator HFS/HFS+ reader parses an
   attached disc directly (no host mount), so raw floppy/disc/CD images work.
3. **Guest firmware is installed.** A real New World `Mac OS ROM` is
   auto-discovered on a Mac OS 8.5+ disc, or an Old World dump is supplied on
   the boot volume. The ROM is mapped read-only and executed.
4. **The guest runs on a PowerPC interpreter.** A continuous decode/execute loop
   implements the integer and FPU instruction set, a timebase/decrementer tick,
   and exception delivery through the guest's own vector table.
5. **The guest's managers are hosted.** Where the guest's bootstrap needs a
   working Toolbox (a Memory Manager zone, and the QuickDraw / Window / Dialog /
   Menu / Event managers), the bootloader backs those CFM imports with host
   implementations.

## Boot sequence

`efi_main` (`src/main.c`) runs, in order:

```
InitializeLib / cancel OVMF watchdog
PpcConfigLoad / PpcBootGateWait / PpcShowConfigMenu   # splash + F8 setup
PpcInitializeUefiInterface
PpcInitializeDebug
PpcInitializeTranslationContext
PpcRunSelfTest                                        # PPC CPU 35/35 incl. FPU
PpcInitializeMemoryManager / PpcSetGuestMemory        # guest RAM bank
  (guest-RAM program exec self-check)
PpcInitializeHardwareAbstraction                       # GOP, audio ring
PpcInitializeBootloader / PpcSetupBootEnvironment
PpcInitializeGraphics (+ multi-frame blit self-check)
PpcInitializeAudio    (if enabled)  (+ self-check)
PpcInitializeStorage / PpcInitializeBlockIo (+ self-check)
PpcHfsRunSelfTest
PpcInitializeNetwork  (if enabled)  (+ self-check)
PpcInstallLowMemory                                   # globals at 0x0
PpcInstallSystemRom                                   # disk -> RAM -> guest window
PpcRunBootSelfTest                                    # ROM R/O, low mem, reset prog
PpcPrepareSystemForBoot
PpcRunGuest                                            # continuous guest execution
PpcLocateSystemFolder / PpcLoadSystemFiles /
  PpcScanExtensionsDirectory / PpcLoadDrivers          # staging
PpcRunSystemFilesSelfTest
```

## ROM sourcing and types

Priority order (in `PpcInstallSystemRom` / `PpcLoadSystemRom` /
`BootLoadHfsRomToPages`):

1. `\System\MacOS\ROM` on the boot volume — an Old World firmware dump
   (System 7 needs one of these).
2. `\System Folder\Extensions\Mac OS ROM` on the boot volume.
3. A `Mac OS ROM` file found anywhere on an attached Mac OS 8.5+ disc via the
   HFS reader (whole-catalog search). This is how a genuine Mac OS 9.2.2 install
   disc yields its New World ROM from `Power Mac G4 Install:System Folder:Mac OS
   ROM`.
4. `PpcInstallDemoRom` — a small self-contained image that keeps the install and
   self-test path exercisable without real firmware (it cannot boot an OS).

`BootIdentifyRomType` classifies by signature: a leading `<CHRP-BOOT>` means New
World (PPC, Mac OS 8.5+); otherwise Old World. The type is recorded in the
guest boot-info block.

For New World ROMs, EFIMac applies the same class of bootstrap patches that
SheepShaver applies to a `Mac OS ROM` (`PpcPatchNewWorldRom`): it exposes a
`nanokernel` boot structure, and it rewrites the ROM's 68K-emulator entry so the
nanokernel's boot tail jumps into the emulator-start routines instead of raising
a kernel trap. These patches are ROM-revision sensitive and are only applied
where the expected patterns are found.

## Guest memory map

Managed by `PpcAddGuestMemoryRegion` — a multi-region map in the interpreter
with a read-only flag per region.

| Region                  | Guest address | Size        | Access |
|-------------------------|---------------|-------------|--------|
| Low-memory globals      | `0x00000000`  | 256 KB      | R/W    |
| Boot RAM bank           | `0x00000000`  | 16 MB       | R/W    |
| System/Finder staging   | `0x20000000`  | 16 MB       | R/W    |
| Extensions (drivers)    | `0x21000000`  | 32 MB       | R/W    |
| Nano-kernel system area | `0x68000000`  | 128 MB      | R/W    |
| OS-runtime staging      | `0x68E00000`  | 16 MB       | R/W    |
| New World ROM window    | `0xFFF00000`  | 4 MB        | R (ROM)|

Additional working areas during a real boot: the New World ROM is executed from
`0x40B00000`-range aliases, the nano-kernel/DR working set lives around
`0x40B00000`, `KDP` at `0xA000`, and the SCC console device at `0x20000`. This
is a *host-defined* environment tuned so the guest's own expectations
(low-memory globals layout, KDP, the vector table, SCC) are met — not a
byte-for-byte reproduction of a specific Mac model.

## In-emulator HFS reader

`src/fs/hfs.c` parses classic HFS volumes without the host mounting them:

- **Block-size auto-detection** so raw floppy images (512 B blocks), CD ISO
  images (2048 B blocks), and HFS-with-2 KB-cluster layouts all work.
- **Catalog-based lookup** (`PpcHfsGetEntryById`) resolving files by catalog
  FlNum/DirID, so names containing `/` or `:` are handled unambiguously.
- **Multi-overflow-extent support** for large files.

Exercised against System 7.5.3 (raw HFS), Mac OS 8.1 (ISO), and Mac OS 9.2.2
(ISO with multi-overflow extents).

## System Folder and driver staging

- `PpcLocateSystemFolder` finds the System Folder (ESP FAT or attached HFS disc)
  and detects System, Finder, Extensions, and the Mac OS ROM.
- `PpcLoadSystemFiles` stages System and Finder (empty files are skipped —
  System 7.5.3's `Finder` is a genuine 0-byte stub) into the system area at
  `0x20000000`.
- `PpcScanExtensionsDirectory` / `PpcLoadDrivers` enumerate and stage up to 64
  Extensions into the driver area at `0x21000000`. Every non-empty extension
  stages with 0 failures on the test discs.

## Device layer

- **Graphics:** a GOP mode is selected and a guest framebuffer window is carved.
  Guest code writes big-endian `0xRRGGBB00` pixels; `PpcGraphicsBlitToDisplay`
  converts and copies to the real GOP framebuffer (byte-exact for RGB and BGR
  layouts). Verified with full-screen solid frames checked pixel-by-pixel,
  band boundaries, corners, and out-of-bounds write rejection.
- **Storage:** every Block I/O handle is enumerated with real geometry, and
  `PpcReadDiskBlock` issues real `ReadBlocks` calls. The HFS reader layers on
  top.
- **Network:** every Simple Network Protocol interface is started and
  initialized (real MAC/media state) and transmits a real frame.
- **Console (SCC):** the guest console is a Zilog 8530 model at guest `0x20000`.
  The nano-kernel boot printer polls the status register and writes characters;
  the interpreter drains queued characters during the banner flush. This is what
  produces the nano-kernel milestone text on the serial log.
- **Audio:** a ring buffer in guest RAM, read/advanced by the host.

## PowerPC interpreter

`src/cpu/interpreter.c` implements a continuous fetch-execute loop: the integer
and FPU instruction sets, a register file with SPRs, a multi-region guest memory
map with read-only enforcement, and a timebase/decrementer scheduler tick.

Guest-facing behaviour:

- **Exception delivery through the firmware's vector table.** The vector table
  base is read from SPRG3; entries (`0x100`..`0xF00`) are dispatched the way the
  firmware's own stubs would, including the interrupted register/LR deposits the
  nano-kernel's interrupt-save path expects.
- **Device windows.** Accesses outside guest regions decode as device accesses
  (the SCC is one); unmapped reads return 0.
- **Instrumentation.** A debug logging path (milestone prints, capped probes, a
  PC/register tail ring) is used heavily to chase boot; probe families are
  capped/log-only so production boots stay deterministic.

The guest currently runs firmware on a **flat direct map**; there is no
BAT/SDR1 translation model yet.

## Firmware bring-up: nanokernel and the DR emulator

New World Mac OS 8.5+ boots almost entirely on PowerPC, and the "68K emulator"
(the Toolbox DR layer) is itself PPC code carried in the `Mac OS ROM`. EFIMac
lets the guest's own DR emulator run as the PPC code it is, rather than
replacing it, and fixes the PPC-side environment gaps the DR depends on. The
bring-up path has reached:

- Nano-kernel init: coherence groups, address spaces, BAT setup, ready queues,
  blue/idle tasks, timeslicing.
- VMM zone carving ("converting PMDTs to areas").
- Task creation with PPC/68K interleave.
- The DR console read loop (SCC), then the 68K bootstrap running on the DR.

## The CFM path and the host manager layer

Once the 68K bootstrap runs, the guest loads the System's CFM (PEF) code. EFIMac
loads and relocates that PEF (`src/boot/pef_loader.c`, exercised by the entry
fragment at guest `0x55CB40`) and enters its `main` entry.

The System's entry fragment expects a working Memory Manager zone. Because the
native-CFM shortcut used here skips the System's own 68K Memory Manager
initialization, EFIMac provides a **host Memory Manager** in the interpreter:

- It builds a Memory Manager zone record in a reserved guest region
  (`0xA00000`), wires the zone into the classic low-memory globals
  (`SysZone`, `ApplZone`, `TheZone`), and seeds a free master-pointer chain.
- It binds the Memory Manager Toolbox imports (`NewHandle`, `NewPtr`,
  `DisposeHandle`, `HLock`/`HUnlock`, `MoreMasters`, `GetZone`/`SetZone`,
  `SystemZone`, `MaxApplZone`, `BlockMove`, `GetHandleSize`, `MemError`,
  `CompactMem`, `PurgeMem`, and friends) to host implementations. CFM import
  descriptors are filled with unique stub addresses that the interpreter
  recognizes and dispatches to the corresponding host handler.
- Any Toolbox import that is *not* yet implemented resolves to a shared stub
  that returns zero and logs an `UNBOUNDCALL` trace, so progress is measurable.

With this in place the entry fragment's bootstrap (which loops calling
`MoreMasters` until a free-block chain threshold is met) completes, the System
runs its manager initialization, and it settles into its `WaitNextEvent` event
loop.

## Open work

The immediate wall is **Toolbox coverage**. On the order of ~109 Toolbox imports
are still unimplemented and return zero, so the guest produces no windowing,
drawing, menu or event content and idles in its event loop. These are being
implemented manager by manager — QuickDraw (rects/regions/ports), the Window and
Dialog managers, the Menu manager, TextEdit, the Event/Process manager, and the
Resource manager — each backed by host implementations driven from UEFI.

After that:

1. **On-screen presentation.** Blit the guest framebuffer to GOP as the guest
   draws, so Window/Dialog/Menu output is visible, and feed input back.
2. **MMU/translation.** Implement the firmware's BAT/SDR1 translation handshake
   (or a correct flat alias) so firmware that arms translation proceeds as on
   hardware.
3. **Validate the matrix.** New World 9.2.2, Mac OS 8.1, and Old World ROMs
   (System 7) end to end.

## Prior art

EFIMac is an independent implementation but leans on what others learned. It is
**similar to SheepShaver in some ways** — supplying the guest's low-memory/ROM
bootstrap globals, a paravirtual host, and reuse of the guest's own 68K emulator
— and **takes a little from DingusPPC** — treating the nanokernel/DR as native
PowerPC code, plus its device and interrupt models. Basilisk II and QEMU are
additional references for classic Mac device behaviour.

## Build and run

See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for the clang/lld-link
GNU-EFI cross-build and [USER_GUIDE.md](USER_GUIDE.md) for the QEMU/OVMF boot
and disc attachment. Agent-oriented build/run/harness notes are in
[AGENTS.md](AGENTS.md).

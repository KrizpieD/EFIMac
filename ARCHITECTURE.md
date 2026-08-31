# EFI Mac OS Boot Layer — Architecture

This document describes how the project actually works today: a heavy UEFI
bootloader that stages a classic PowerPC Mac boot environment from UEFI standard
protocols, plus the design decisions behind it.

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
4. **A PowerPC interpreter executes guest code.** Fixed 32-bit opcodes are
   decoded and interpreted against guest memory (with a multi-region map and
   read-only ROM enforcement), including a full FPU core and exception support.

## Boot Flow

```
efi_main (src/main.c)
  PpcInitializeUefiInterface      # LoadedImage, Boot Services, console
  PpcInitializeDebug              # boot.log + monotonic timer
  PpcInitializeTranslationContext # PPC register file, MSR/SRR0/1/CTR/LR
  PpcRunSelfTest                  # 35 checks incl. FPU core
  PpcInitializeMemoryManager      # 256 MB guest RAM @ 0x10000000
  PpcSetGuestMemory               # wire UEFI pages into interpreter
  [RAM-resident PPC program demo] # addi/mullw/stw through the memory path
  PpcInitializeHardwareAbstraction# GOP, Block I/O, SNP, audio ring
  PpcInitializeBootloader
  PpcSetupBootEnvironment
  PpcInitializeGraphics           # GOP mode + guest framebuffer window
  [Graphics self-checks]          # full-screen frames verified on the GOP buffer
  PpcInstallLowMemory             # 16 KB globals @ 0x0
  PpcInstallSystemRom             # \System\MacOS\ROM -> HFS "Mac OS ROM" -> demo
  PpcRunBootSelfTest              # region map, read-only ROM, reset vector
  PpcPrepareSystemForBoot         # PC = reset vector, MSR = ME|RI, boot info block
  PpcLocateSystemFolder / PpcLoadSystemFiles / PpcScanExtensionsDirectory /
    PpcLoadDrivers                # stage System, Finder, Mac OS ROM, Extensions
  PpcRunSystemFilesSelfTest       # staged bytes read back via interpreter
  PpcGetBootInfo -> status report
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
records the type. The boot self-test adapts to the ROM: the `ROM1` magic and
reset-vector execution checks run only for the demo ROM, while a real ROM is
verified for region presence (and the CHRP signature when New World) plus
read-only enforcement.

## Guest Memory Map

Managed by `PpcAddGuestMemoryRegion` (multi-region map in the interpreter,
read-only flag per region):

| Region              | Guest address | Size       | Access |
|---------------------|---------------|------------|--------|
| Low-memory globals  | `0x00000000`  | 16 KB      | R/W    |
| Guest RAM           | `0x10000000`  | 256 MB     | R/W    |
| Framebuffer window  | `0x18000000`  | 640x480x32 | R/W    |
| Audio ring buffer   | `0x18800000`  | 8 KB       | R/W    |
| System area         | `0x20000000`  | 16 MB      | R/W    |
| Driver area         | `0x21000000`  | 32 MB      | R/W    |
| System ROM          | `0xFFF00000`  | 4 MB       | R (ROM) |

The bootloader-defined boot-info block in low memory (magic `"EFI!"` at `0x0`,
then RAM base/size, ROM base/size, ROM type) is entirely host-defined — it is
not a real Mac OS ROM globals table.

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
- **Audio:** no UEFI audio standard exists, so the device is a fixed ring buffer
  in guest RAM; the host reads PCM samples back and advances a play cursor.

## PowerPC Interpreter

`src/cpu/interpreter.c` decodes and executes fixed 32-bit big-endian PowerPC
opcodes with a register file (32 GPRs, CR, CTR, LR, MSR, SRR0/1, FP registers +
FPSCR), big-endian guest memory access, FPU core (opcodes 48-63, gated on
MSR[FP] with the FP-unavailable exception at `0x800`), and exception dispatch
(program `0x700`, FP `0x800`). Execution today is block-at-a-time
(`PpcExecuteBlock`): small hand-checked programs run from guest RAM and the
demo ROM's reset vector. There is no MMU, no timer/interrupt injection, and no
continuous fetch-execute loop — the ROM window is never executed for real.

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

See [TODO.md](TODO.md). Short list, in order: (1) finish the PPC-native pivot —
restore the OS's own DR emulator and complete the PPC environment needed to run
it (translation, KernelData/hardware seeds, device registers); (2) validate
across the new matrix (New World 9.2.2, Mac OS 8.1, and the `mac_roms` Old World
ROMs); (3) only later, legacy 68K runtime support for the Classic app layer.

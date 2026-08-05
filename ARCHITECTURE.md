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

## Open Work

See [TODO.md](TODO.md). The short list: continuous guest execution, MMU and
exception delivery to real firmware, Mac device register emulation in the guest
map, Old World ROM boot testing with System 7, and New World ROM execution.

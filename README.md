# EFI Mac OS Boot Layer

A **heavy UEFI bootloader** for classic Mac OS (System 7, Mac OS 8, Mac OS 9) on
x86_64 UEFI systems. It boots as an EFI application, builds a classic PowerPC
Mac memory image (guest RAM, low-memory globals, a read-only ROM window, staged
System Folder files and drivers), installs Mac firmware into that image, and
simulates Mac devices on top of standard UEFI protocols.

## Project Overview

Classic Mac OS expects a PowerPC Mac: a ROM window at `0xFFF00000`, system globals
in low memory, a boot volume holding the System Folder, and hardware devices
behind specific register windows. This project provides that environment from a
UEFI bootloader:

- **UEFI standard protocols are the hardware abstraction.** Graphics (GOP),
  storage (Block I/O / Simple File System), and networking (Simple Network
  Protocol) are used directly as the platform's I/O; simulated Mac devices
  (framebuffer window, audio ring buffer) are wired to those inputs.
- **An in-emulator HFS reader** reads classic Mac discs directly (System 7
  floppy/disc images and Mac OS 8/9 install discs), with automatic block-size
  detection, catalog-based lookup, and multi-extent file support.
- **A PowerPC instruction interpreter** (fixed 32-bit opcodes, GPR/SPR/FPU
  state, guest memory map, self-test) executes small programs in guest memory.
- **Mac firmware sourcing.** New World `Mac OS ROM` files are auto-discovered on
  attached discs (verified with a genuine Mac OS 9.2.2 install disc); Old World
  firmware is supplied by the user as a ROM dump on the boot volume; a demo ROM
  keeps the full install path exercisable without either.

## Current Status

Working today:

- Builds a valid x86_64 UEFI application on Windows (git-bash + clang/lld-link)
  and macOS (`make`) — see [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md).
- Boots under QEMU + OVMF and runs a full boot sequence with self-tests
  (PowerPC CPU 35/35 including the FPU core, boot memory map, System Folder /
  driver staging 7/7).
- Loads a **real New World ROM** from a Mac OS 8.5+/9 disc:
  `Mac OS 9.2.2.iso` → `System Folder:Mac OS ROM` (2,763,530 bytes,
  `<CHRP-BOOT>` signature) → installed read-only at guest `0xFFF00000` and
  reported as New World.
- Stages System, Finder, and up to 64 Extensions (drivers) from real discs
  (verified against System 7.5.3, Mac OS 8.1, and Mac OS 9.2.2 images).
- Drives real GOP/Block I/O/SNP hardware: framebuffer blits verified
  pixel-by-pixel, disk geometry and block reads, and real NIC transmit.

Not yet:

- **The guest OS does not boot.** The ROM is installed and self-tested but not
  executed for real — there is no MMU emulation, no continuous instruction
  fetch/execute loop, and no Mac device register emulation beyond the
  self-tests. System 7 requires a user-supplied Old World ROM (it cannot be
  derived from a New World `Mac OS ROM` file).

## ROM Priority

1. `\System\MacOS\ROM` on the boot volume (EFI System Partition) — a classic
   Old World firmware dump (4 MB).
2. A user-supplied `\System Folder\Extensions\Mac OS ROM` file on the ESP.
3. `Mac OS ROM` auto-discovered on an attached Mac disc via the in-emulator HFS
   reader (New World, Mac OS 8.5+).
4. Demo ROM fallback (self-check only; cannot boot an OS).

## Guest Memory Map

| Region                | Guest address | Size    |
|-----------------------|---------------|---------|
| Low-memory globals    | `0x00000000`  | 16 KB   |
| Guest RAM             | `0x10000000`  | 256 MB  |
| Framebuffer window    | `0x18000000`  | 640x480x32 |
| Audio ring buffer     | `0x18800000`  | 8 KB    |
| System area           | `0x20000000`  | 16 MB   |
| Driver area           | `0x21000000`  | 32 MB   |
| System ROM window     | `0xFFF00000`  | 4 MB (read-only) |

## Source Layout

```
src/
├── main.c                     # efi_main: subsystem init + self-test orchestration
├── cpu/
│   └── interpreter.c          # PowerPC decode/execute, register file, guest memory map
├── memory/
│   └── manager_impl.c         # Guest RAM (UEFI AllocatePages) + region mapping
├── hardware/
│   └── abstraction_impl.c     # GOP framebuffer, Block I/O, SNP, audio ring
├── boot/
│   ├── bootloader.h           # Guest map constants, ROM types, API
│   └── bootloader_impl.c      # ROM install, HFS boot probe, System Folder staging
├── fs/
│   └── hfs.c                  # In-emulator HFS/HFS+ reader (catalog + extents)
├── utils/
│   └── debug_impl.c           # Debug log (boot.log) + timers
└── platform/
    └── uefi_interface_impl.c  # UEFI protocol discovery
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for design details,
[USER_GUIDE.md](USER_GUIDE.md) for running it, and [TODO.md](TODO.md) for the
roadmap.

## Building

Windows (git-bash):

```bash
bash scripts/build-windows.sh
```

macOS/Linux (`brew install llvm lld`):

```bash
make
make check
```

Output: `build/EFI-Mac-Emulator.efi`. Details in
[BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md).

## Testing

Boot under QEMU + OVMF and attach a classic Mac disc:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run-qemu-windows.ps1 `
  -MacDisc "$env:TEMP\opencode\mac\Mac OS 9.2.2.iso"
```

The serial log is captured to `$env:TEMP\opencode\boot_out.txt`. With the 9.2.2
install disc you should see the real ROM installed:

```
System ROM loaded from HFS volume 'Power Mac G4 Install': 2763530 bytes
System ROM installed: 2763530 bytes at guest 0xFFF00000 (New World)
Boot state: ready=1, kernel=0, ROM at 0xFFF00000 (2763530 bytes, New World (Mac OS ROM)) ...
```

## Important Notes

- Classic Mac OS files are copyrighted by Apple. This project does not include
  any Mac OS or Mac firmware files; you must supply your own ROMs and discs.
- For System 7, place a genuine Old World ROM dump at `\System\MacOS\ROM` on
  the boot volume. A New World `Mac OS ROM` (from Mac OS 8.5+) is detected
  automatically from discs but is only recognized/staged — it cannot currently
  boot the guest.
- Existing open-source emulators (SheepShaver, Basilisk II, QEMU, DingusPPC)
  are valuable references for the PowerPC and Mac device semantics this project
  re-creates on UEFI.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

## Version

- **Version**: 0.2
- **Status**: Functional boot layer — self-tests and real ROM detection work;
  guest OS boot is the remaining milestone.
- **Target**: x86_64 UEFI.

# EFI-Mac-Emulator

A UEFI-based emulator for running classic Mac OS (System 7, Mac OS 8, and Mac OS 9) on modern Intel x86_64 computers.

## Project Overview

This project creates a UEFI executable that provides:
- CPU translation layer for PowerPC architecture
- Graphics and basic I/O handling
- Bootloader compatibility with classic Mac OS versions
- Hardware abstraction for running legacy Mac OS on modern x86_64 systems

## Goals

### Primary Objectives
1. Create a UEFI executable that acts as a translation layer between modern x86_64 hardware and classic Mac OS PowerPC architecture
2. Support booting Mac OS 8, 9, and System 7 on modern Intel computers
3. Implement basic graphics, sound, and I/O emulation
4. Provide compatibility with existing Mac OS software ecosystem

### Technical Approach
- Develop UEFI application that initializes the translation layer
- Implement CPU instruction set translation (PowerPC)
- Create hardware abstraction layer for graphics, storage, and peripheral devices
- Design boot process that loads classic Mac OS from modern storage media

## Architecture

### Components
1. **UEFI Application**: Main entry point that initializes the emulator environment
2. **CPU Translation Layer**: Handles instruction set translation between x86_64 and PowerPC
3. **Hardware Abstraction Layer**: Provides virtualized hardware interfaces for graphics, sound, disk I/O, etc.
4. **Bootloader**: Loads Mac OS kernel and system files
5. **Memory Manager**: Handles virtual memory management for the emulated system

### Target Architectures
- PowerPC (preferred for initial implementation)
  - Supports Mac OS 8/9
  - Better compatibility with existing emulators like SheepShaver
  - More modern architecture than 68k

## Current Status

**Pre-alpha scaffolding, but it builds.** The repository currently contains:

- Module interfaces (headers) and partial, mostly placeholder implementations for the CPU translation layer, memory manager, hardware abstraction, bootloader, debug system, and UEFI interface (`src/*/*_impl.c`).
- A working build: `make` produces a valid x86_64 UEFI application (`build/EFI-Mac-Emulator.efi`) using GNU-EFI headers/runtime with a clang + lld-link cross-toolchain (see [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md)).

What does **not** exist yet:

- No working PowerPC instruction translation — the current code only decodes a handful of opcodes and does not actually translate or execute anything.
- No memory management, graphics, audio, storage, or network emulation beyond placeholder functions that mostly print status messages.
- No bootloader that loads real Mac OS images — kernel loading is simulated.

See [TODO.md](TODO.md) for the phase-by-phase plan and what remains to be done.

### Completed So Far

- **Phase 1: Research and Analysis**
  - Analyzed existing Mac emulators (SheepShaver, Basilisk II, QEMU, DingusPPC)
  - Studied PowerPC vs 68k architecture differences
  - Documented UEFI specifications and implementation guidelines
- **Design**: Module interfaces (headers) and a starting skeleton for each subsystem.

### Not Yet Implemented

- Actual PowerPC instruction set translation / execution engine
- Memory manager and MMU emulation
- Real graphics, audio, storage, and network device emulation
- Bootloader that loads and boots a real Mac OS image

## Source Code Structure

```
src/
├── main.c                       # Main UEFI application entry point
├── cpu/
│   ├── translation.h            # Header for translation functions
│   └── translation_impl.c       # Partial implementation of translation logic
├── memory/
│   ├── manager.h                # Header for memory manager
│   └── manager_impl.c           # Partial implementation of memory manager
├── hardware/
│   ├── abstraction.h            # Header for hardware abstraction
│   └── abstraction_impl.c       # Partial implementation of hardware abstraction
├── boot/
│   ├── bootloader.h             # Header for bootloader functions
│   └── bootloader_impl.c        # Partial implementation of bootloader
├── utils/
│   ├── debug.h                  # Header for debugging functions
│   └── debug_impl.c             # Partial implementation of debugging system
└── platform/
    ├── uefi_interface.h         # Header for UEFI interface functions
    └── uefi_interface_impl.c    # Partial implementation of UEFI interface

Makefile                # Build configuration (make -> build/EFI-Mac-Emulator.efi)
ARCHITECTURE.md            # Design notes for the CPU translation layer
BUILD_INSTRUCTIONS.md      # Build instructions
USER_GUIDE.md              # User documentation
TODO.md                    # Implementation plan and status
```

## Prerequisites for Building

Building this project requires a macOS host with Homebrew:

1. **UEFI headers/runtime**: GNU-EFI — cloned automatically by the Makefile into `third_party/gnu-efi/`
2. **Toolchain**: `brew install llvm lld` (clang cross-compiles to PE/COFF; lld-link links the UEFI image)

## Building Instructions

```bash
brew install llvm lld
make
```

Output: `build/EFI-Mac-Emulator.efi`. Verify the image with `make check`.
See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for details.

## Testing

The EFI application boots and runs under QEMU + OVMF (verified): it initializes
all subsystems, prints its status report, and returns cleanly to the firmware.
See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for the exact steps.

Testing an emulator like this requires:

1. **UEFI firmware capable of running EFI applications** (e.g. QEMU + OVMF)
2. **Classic Mac OS system files**:
   - System 7, Mac OS 8, or Mac OS 9 ROM images
   - Kernel images for the respective systems

## Important Notes

This is an ambitious emulator that requires:
- A proper UEFI development environment (EDK II or GNU-EFI)
- Access to classic Mac OS system files (ROMs, kernels, etc.)
- Understanding of both UEFI and PowerPC architectures

The PowerPC translation layer, in particular, is a large undertaking. Existing
open-source projects such as SheepShaver, Basilisk II, QEMU, and DingusPPC are
valuable references.

## Future Development

### Planned Enhancements:
- Real PowerPC instruction interpreter (and eventually dynamic translation)
- Better audio subsystem
- More comprehensive graphics support
- Improved performance optimization
- Additional hardware device emulation
- Enhanced debugging capabilities

### Compatibility Improvements:
- Support for more Mac OS versions
- Better memory management
- Advanced graphics acceleration
- Network protocol improvements

## License

This project is licensed under the GNU General Public License, version 3 or
later. See the [LICENSE](LICENSE) file for details.

## Version Information

- **Version**: 0.1 (Scaffolding)
- **Status**: Pre-alpha — builds a UEFI application, but emulation is scaffolding/placeholders only
- **Target Platforms**: x86_64 UEFI systems

This is a work in progress.

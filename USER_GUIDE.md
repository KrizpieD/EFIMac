# EFI-Mac-Emulator - User Guide

## Status

**This project is not usable yet.** It is pre-alpha scaffolding: the emulator
does not build and does not boot Mac OS. This guide describes the intended
behavior once the project is functional. Treat everything below as the target
design, not current capability. See [TODO.md](TODO.md) for progress.

## Overview

The EFI-Mac-Emulator is intended to be a UEFI-based application that runs
classic Mac OS versions (System 7, Mac OS 8, and Mac OS 9) on modern Intel
x86_64 computers, by providing an emulated PowerPC environment through the UEFI
boot system.

## System Requirements (Target)

### Hardware Requirements:
- Intel x86_64 processor (modern CPU recommended)
- 2GB+ RAM (4GB+ recommended)
- 10GB+ available disk space
- UEFI-capable motherboard or virtual machine

### Software Requirements:
- UEFI firmware that supports EFI applications
- QEMU (with OVMF) or similar for testing without physical hardware

## Building (Target)

The project must be built with a real UEFI toolchain (EDK II or GNU-EFI). It
does not build yet. See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md).

Once buildable, the result would be an EFI application
(`EFI-Mac-Emulator.efi`) that you copy to a FAT32 USB drive or EFI system
partition, or run from the UEFI shell in QEMU.

## Configuration (Planned)

The emulator is intended to accept boot parameters configured before booting:

- **Boot Mode**: Normal, Recovery, Diagnostic
- **Memory Size**: Amount of guest RAM to allocate (in MB)
- **Video Mode**: Display resolution settings
- **Debug Mode**: Enable detailed logging output

None of this configuration is implemented yet.

## Usage Instructions (Planned)

### Method 1: Direct UEFI Boot
1. Copy `EFI-Mac-Emulator.efi` to your EFI system partition or bootable USB
2. Reboot and enter UEFI setup
3. Select the emulator from the boot menu

### Method 2: UEFI Shell Testing
1. Boot into the UEFI shell
2. Navigate to the location of `EFI-Mac-Emulator.efi`
3. Execute:
   ```
   EFI-Mac-Emulator.efi
   ```

### Method 3: Virtual Environment (Recommended for Testing)
1. Install QEMU with OVMF support
2. Create a virtual machine with UEFI firmware
3. Boot the emulator within the VM

## Loading Mac OS Systems (Planned)

### Required Files:
To run classic Mac OS versions, you will need:

1. **Mac OS ROM images** — System ROM files for the target Mac OS version
2. **Kernel images** — Kernel files for each system version
3. **System folders** — Complete system files and applications

### Loading Process (Planned):
1. The emulator initializes the PowerPC environment
2. It loads the specified ROM and kernel images
3. Hardware abstractions are set up
4. System files are mounted and initialized
5. Control is transferred to the Mac OS kernel

## Features and Capabilities (Planned)

### Supported Systems (Target):
- **System 7** (7.0 - 7.6)
- **Mac OS 8** (8.0 - 8.6)
- **Mac OS 9** (9.0 - 9.2)

### Hardware Support (Target):
- **Graphics**: 640x480, 800x600, 1024x768, and 1280x1024 resolutions
- **Audio**: Basic audio subsystem support
- **Storage**: Multiple storage device emulation
- **Networking**: Network interface abstraction

### Emulation Features (Target):
- **CPU Translation**: PowerPC to x86_64 instruction translation
- **Memory Management**: Guest memory handling
- **Hardware Abstraction**: Consistent hardware interfaces
- **Boot Process**: Boot sequence emulation
- **Debugging**: Logging and debugging capabilities

## Troubleshooting (Planned)

None of the features below are currently available. When the project is
functional, common issues are expected to include:

1. **Emulator fails to load**:
   - Check that you're running on UEFI-capable hardware
   - Verify the EFI application is properly formatted
   - Ensure firmware supports loading EFI applications

2. **System hangs during boot**:
   - Check system requirements (memory, CPU)
   - Verify ROM files are valid for your target system
   - Enable debug logging for more information

3. **Graphics issues**:
   - Try different video modes in boot parameters
   - Ensure your UEFI firmware supports the resolution requested

## Limitations

### Current Limitations:
- The emulator does not build or run yet (pre-alpha)
- Requires UEFI-capable hardware or a virtual environment
- Full system compatibility will depend on available ROM images
- Performance may be lower than native execution
- Some advanced Mac OS features may not be fully supported

### Planned Enhancements:
- Real PowerPC instruction interpreter (and dynamic translation later)
- Better audio subsystem
- More comprehensive graphics support
- Improved performance optimization
- Additional hardware device emulation
- Enhanced debugging capabilities

## License

This project is licensed under the GNU General Public License, version 3 or
later. See the [LICENSE](LICENSE) file for details.

## Version Information

- **Version**: 0.1 (Scaffolding)
- **Status**: Pre-alpha - not yet buildable
- **Target Platforms**: x86_64 UEFI systems

This is a work in progress.

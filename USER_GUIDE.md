# EFI-Mac-Emulator - User Guide

## Overview

The EFI-Mac-Emulator is a UEFI-based application that enables running classic Mac OS versions (System 7, Mac OS 8, and Mac OS 9) on modern Intel x86_64 computers. It provides a complete emulation environment through the UEFI boot system.

## System Requirements

### Hardware Requirements:
- Intel x86_64 processor (modern CPU recommended)
- 2GB+ RAM (4GB+ recommended)
- 10GB+ available disk space
- UEFI-capable motherboard or virtual machine

### Software Requirements:
- UEFI firmware that supports EFI applications
- Operating system with UEFI boot capability (Windows 10/11, Linux with UEFI)
- QEMU or similar for testing without physical hardware

## Installation

### For Development Users:
1. Clone the repository:
   ```powershell
   git clone https://github.com/your-repo/efimac-project.git
   cd efimac-project
   ```

2. Build the project using CMake:
   ```powershell
   mkdir build
   cd build
   cmake .. -G "MinGW Makefiles"
   cmake --build .
   ```

3. The EFI application will be generated as `EFI-Mac-Emulator.efi`

### For End Users:
1. Download the pre-built EFI application from the releases page
2. Copy to a FAT32 formatted USB drive or EFI system partition
3. Ensure your UEFI firmware supports loading EFI applications

## Configuration

### Boot Parameters:
The emulator accepts various boot parameters that can be configured before booting:

- **Boot Mode**: Normal, Recovery, Diagnostic
- **Memory Size**: Amount of RAM to allocate (in MB)
- **Video Mode**: Display resolution settings
- **Debug Mode**: Enable detailed logging output

### Configuration Options:
```ini
# Example configuration file (not implemented yet)
[boot]
mode = normal
memory = 256
video = 1024x768
debug = true

[hardware]
graphics = enabled
audio = enabled
storage = 1
network = 1
```

## Usage Instructions

### Method 1: Direct UEFI Boot
1. Copy `EFI-Mac-Emulator.efi` to your EFI system partition or bootable USB
2. Reboot and enter UEFI setup
3. Select the emulator from the boot menu
4. The emulator will initialize and display status information

### Method 2: UEFI Shell Testing
1. Boot into UEFI shell
2. Navigate to the location of `EFI-Mac-Emulator.efi`
3. Execute:
   ```
   EFI-Mac-Emulator.efi
   ```

### Method 3: Virtual Environment (Recommended for Testing)
1. Install QEMU with OVMF support
2. Create a virtual machine with UEFI firmware
3. Boot the emulator within the VM

## Loading Mac OS Systems

### Required Files:
To run classic Mac OS versions, you'll need:

1. **Mac OS ROM images** - System ROM files for the target Mac OS version
2. **Kernel images** - Kernel files for each system version
3. **System folders** - Complete system files and applications

### Loading Process:
1. The emulator will initialize the PowerPC environment
2. It will load the specified kernel image
3. Hardware abstractions are set up
4. System files are mounted and initialized
5. Control is transferred to the Mac OS kernel

## Features and Capabilities

### Supported Systems:
- **System 7** (7.0 - 7.6)
- **Mac OS 8** (8.0 - 8.6)
- **Mac OS 9** (9.0 - 9.2)

### Hardware Support:
- **Graphics**: 640x480, 800x600, 1024x768, and 1280x1024 resolutions
- **Audio**: Basic audio subsystem support
- **Storage**: Multiple storage device emulation
- **Networking**: Network interface abstraction

### Emulation Features:
- **CPU Translation**: Full PowerPC instruction set translation
- **Memory Management**: Virtual memory handling
- **Hardware Abstraction**: Consistent hardware interfaces
- **Boot Process**: Complete boot sequence emulation
- **Debugging**: Comprehensive logging and debugging capabilities

## Troubleshooting

### Common Issues:

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

### Debugging:
Enable debug mode to get detailed output:
```
# In boot parameters or configuration file
debug = true
log_level = 4
```

## Performance Tips

1. **Memory Allocation**: Allocate sufficient RAM (256MB+ recommended)
2. **Storage**: Use fast storage for system files
3. **CPU**: Modern multi-core processors perform better
4. **Video**: Lower resolutions reduce overhead

## Limitations

### Current Limitations:
- Requires UEFI-capable hardware or virtual environment
- Full system compatibility depends on available ROM images
- Performance may be lower than native execution
- Some advanced Mac OS features may not be fully supported

### Planned Enhancements:
- Better audio subsystem
- More comprehensive graphics support
- Improved performance optimization
- Additional hardware device emulation
- Enhanced debugging capabilities

## Support and Feedback

For issues or questions about the EFI-Mac-Emulator:

1. **GitHub Issues**: Report bugs and feature requests
2. **Documentation**: Check the project wiki for updates
3. **Community**: Join Mac OS emulation forums for support
4. **Contributing**: Contribute improvements to the codebase

## License

MIT License - See LICENSE file for details.

## Version Information

- **Version**: 0.1 (Initial Release)
- **Status**: Alpha - Functional but not fully complete
- **Supported Platforms**: x86_64 UEFI systems

This is a work in progress and may contain bugs or incomplete features.
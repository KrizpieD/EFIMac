# EFI-Mac-Emulator - Build Instructions

## Prerequisites

Before building the EFI-Mac-Emulator, you'll need:

1. **Windows 10/11** with PowerShell 7+ 
2. **Git** for version control
3. **CMake** (version 3.10 or higher)
4. **GCC MinGW** toolchain for x86_64 cross-compilation
5. **UEFI Development Tools**:
   - EDK II (EDK II is required for UEFI development)
   - UEFI SDK or similar toolchain
6. **Visual Studio Build Tools** (optional, for Windows native builds)

## Building the Project

### Step 1: Clone the Repository

```powershell
git clone https://github.com/your-repo/efimac-project.git
cd efimac-project
```

### Step 2: Set up Environment Variables

For UEFI development, you may need to set up EDK II environment:

```powershell
# If using EDK II, set the environment
$env:EDK_TOOLS_PATH = "C:\edk2\EdkTools"
$env:WORKSPACE = "C:\path\to\your\workspace"
```

### Step 3: Create Build Directory and Configure

```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
```

### Step 4: Compile the Project

```powershell
cmake --build . --config Release
```

## Alternative Build Method (Using EDK II)

If you're using EDK II for UEFI development:

1. **Set up EDK II environment**
2. **Create a UEFI application package** in EDK II structure
3. **Compile with build command**:
   ```powershell
   build -p EFI-Mac-Emulator.dsc -b RELEASE -t GCC5
   ```

## Prerequisites for Windows Development

### Required Tools:

1. **MinGW-w64** (GCC compiler)
2. **CMake** 
3. **Git**
4. **EDK II** (for native UEFI compilation)

### Installation Steps:

```powershell
# Install required packages using Chocolatey
choco install mingw cmake git

# Or download and install manually:
# 1. Download MinGW-w64
# 2. Install CMake
# 3. Install Git
```

## Running the Emulator

### Prerequisites for Testing:

1. **UEFI firmware capable of running EFI applications**
2. **Virtual Machine** (like QEMU with UEFI support) or physical hardware with UEFI boot capability
3. **Mac OS system files**:
   - System 7, Mac OS 8, or Mac OS 9 ROM images
   - Kernel images for the respective systems

### Test Procedure:

1. **Boot into UEFI environment**
2. **Load the EFI-Mac-Emulator application**
3. **Configure boot parameters**
4. **Load a Mac OS kernel image**
5. **Execute the boot process**

## Testing with QEMU (Recommended)

To test the emulator without physical hardware:

1. **Install QEMU**:
   ```powershell
   choco install qemu
   ```

2. **Create a UEFI-enabled VM**:
   ```powershell
   qemu-system-x86_64 -bios ovmf.fd -drive file=disk.img,format=raw
   ```

3. **Run the EFI application from UEFI shell**

## Directory Structure

```
efimac-project/
├── src/                    # Source code files
│   ├── main.c             # Main entry point
│   ├── cpu/               # CPU translation components  
│   ├── memory/            # Memory management
│   ├── hardware/          # Hardware abstraction
│   ├── boot/              # Bootloader system
│   ├── utils/             # Utility functions
│   └── platform/          # UEFI interface
├── CMakeLists.txt         # Build configuration
├── README.md              # Project overview
├── BUILD_INSTRUCTIONS.md  # This file
└── TODO.md               # Implementation plan
```

## Troubleshooting

### Common Issues:

1. **CMake errors**: Ensure CMake version is at least 3.10
2. **Missing UEFI headers**: Install EDK II or UEFI SDK
3. **MinGW compilation errors**: Make sure GCC is properly installed and in PATH
4. **Linker errors**: May need to specify additional libraries for UEFI

### Environment Setup:

```powershell
# Check if required tools are available
cmake --version
gcc --version
git --version

# Add MinGW to PATH if needed
$env:PATH += ";C:\mingw64\bin"
```

## Build Artifacts

The build process generates:
- `EFI-Mac-Emulator.efi` - Main EFI application
- Various object files and libraries
- Debug symbols (if built with debug flags)

## Next Steps

1. **Install prerequisites**
2. **Run the build commands**
3. **Test in UEFI environment**
4. **Load Mac OS kernel images**

Note: This is an advanced project that requires proper UEFI development setup. For testing, QEMU with OVMF (Open Virtual Machine Firmware) is recommended as it provides a complete UEFI environment.

## License

MIT License - See LICENSE file for details.
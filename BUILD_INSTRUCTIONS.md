# EFI-Mac-Emulator - Build Instructions

## Status

**The project does not build yet.** The source requires a real UEFI development
environment (EDK II or GNU-EFI), and the UEFI headers/libraries it provides.
The hand-written stub headers and demonstration build scripts that previously
shipped with the repo have been removed. Until the modules are wired to a real
UEFI toolchain (see [TODO.md](TODO.md), Phase 3), do not expect `EFI-Mac-Emulator.efi`
to be produced.

## Prerequisites

1. **UEFI Development Environment** (one of):
   - EDK II (TianoCore) — https://github.com/tianocore/edk2
   - GNU-EFI — https://sourceforge.net/projects/gnu-efi/
2. **Compiler Toolchain**:
   - A cross-compiler targeting x86_64 UEFI (e.g. EDK II's GCC5 toolchain, or
     `x86_64-w64-mingw32-gcc` / clang for GNU-EFI)
3. **CMake** (version 3.10 or higher) — only if using the provided CMakeLists.txt
4. **Git** for version control

## Option 1: Build with CMake

The provided `CMakeLists.txt` expects UEFI headers to be available. Provide the
UEFI include paths via `EFI_INCLUDE_DIRS` and a suitable cross-compiler:

```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DEFI_INCLUDE_DIRS=/path/to/gnu-efi/inc
cmake --build .
```

Note: a CMake build of a UEFI application normally needs to produce a PE32+
image (GNU-EFI style) or be built as part of an EDK II package. The current
CMakeLists.txt is a starting point, not a complete UEFI build.

## Option 2: Build with EDK II

1. Set up the EDK II environment:
   ```bash
   source edksetup.sh
   ```
2. Create a UEFI application package (DSC + INF) that includes the sources in `src/`.
3. Build:
   ```bash
   build -p EFI-Mac-Emulator.dsc -b RELEASE -t GCC5
   ```

## Option 3: Build with GNU-EFI

Compile against GNU-EFI's headers and link with its startup code. For example:

```bash
gcc -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone \
    -maccumulate-outgoing-args -m64 -I/path/to/gnu-efi/inc \
    -c src/main.c src/cpu/translation_impl.c src/memory/manager_impl.c \
    src/hardware/abstraction_impl.c src/boot/bootloader_impl.c \
    src/utils/debug_impl.c src/platform/uefi_interface_impl.c
# then link with gnu-efi's crt0-efi-x86_64.o and libefi.a into a PE32+ image
```

Note: the sources currently include EDK II style headers
(`<Library/UefiLib.h>`, `<Library/BaseLib.h>`, etc.) and use globals such as
`gBS`, `gST`, and `gRT`. They will need adjustment to build with GNU-EFI.

## Testing

To test the emulator (once it builds):

1. **UEFI-capable firmware** — QEMU with OVMF is recommended:
   ```bash
   qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=disk.img,format=raw
   ```
2. Run the EFI application from the UEFI shell.
3. Provide classic Mac OS system files:
   - System 7, Mac OS 8, or Mac OS 9 ROM images
   - Kernel images for the respective systems

## Troubleshooting

1. **CMake errors**: Ensure CMake version is at least 3.10.
2. **Missing UEFI headers**: Install EDK II or GNU-EFI and point the build at
   its include directory.
3. **Linker errors**: A UEFI application must be linked as a PE32+ image with
   UEFI startup code (EDK II or GNU-EFI); a plain host linker will not produce
   a valid `.efi`.

## Directory Structure

```
EFIMac/
├── src/                    # Source code files
│   ├── main.c             # Main entry point
│   ├── cpu/               # CPU translation components
│   ├── memory/            # Memory management
│   ├── hardware/          # Hardware abstraction
│   ├── boot/              # Bootloader system
│   ├── utils/             # Utility functions
│   └── platform/          # UEFI interface
├── CMakeLists.txt         # Build configuration file
├── README.md              # Project overview
├── ARCHITECTURE.md        # Design notes
├── TODO.md               # Implementation plan
└── USER_GUIDE.md         # User documentation
```

## License

This project is licensed under the GNU General Public License, version 3 or
later. See the [LICENSE](LICENSE) file for details.

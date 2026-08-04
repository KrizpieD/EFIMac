# EFI-Mac-Emulator - Build Instructions

## Status

**The project builds.** `make` produces a valid PE32+ UEFI application image
(`build/EFI-Mac-Emulator.efi`). The application is still pre-alpha — it
initializes the emulator scaffolding and prints status, but does not yet
translate or execute PowerPC code.

## Overview

The build cross-compiles from macOS (Apple Silicon or Intel) to an x86_64 UEFI
application:

1. **GNU-EFI** provides the UEFI headers (`efi.h`, `efilib.h`, ...) and a small
   runtime library (memory, string, print, pool helpers).
2. **clang** compiles the sources targeting `x86_64-pc-win32-coff` (PE/COFF
   object files, Microsoft x64 ABI).
3. **lld-link** links the objects into a PE32+ image with
   `Subsystem = EFI_APPLICATION`, entry point `efi_main`.

This is the same approach EDK II's `CLANGPDB` toolchain uses. The classic
GNU-EFI `objcopy`/`ld` flow is not used because the host GNU binutils cannot
emit the `efi-app-x86_64` BFD target on this setup.

## Prerequisites

- **macOS** with Homebrew
- `brew install llvm lld`
- `git` (only needed for the first build, which clones GNU-EFI)

`binutils` is optional (only the `make check` target uses it via
`llvm-objdump`, which actually comes with `llvm`).

## Building

```bash
make
```

On the first run the Makefile clones GNU-EFI into `third_party/gnu-efi/` (it is
git-ignored and fetched on demand). Output:

- `build/EFI-Mac-Emulator.efi` — the UEFI application

Verify the image:

```bash
make check
```

`make clean` removes `build/`. Toolchain locations can be overridden on the
command line if Homebrew is not at the default prefix:

```bash
make CC=/path/to/clang LLD=/path/to/lld-link
```

## How the build works

1. **GNU-EFI runtime library** — the sources under
   `third_party/gnu-efi/lib/` are compiled to COFF objects (see
   `GNUEFI_SRCS` in the Makefile). `entry.c` and the `.S` startup files are
   excluded because they target the ELF/objcopy flow we do not use.
2. **Application sources** — every `src/*/*_impl.c` and `src/main.c` is
   compiled with `-Wall -Werror` against GNU-EFI's headers.
3. **Link** — `lld-link /subsystem:EFI_APPLICATION /entry:efi_main /dll`.
   The firmware performs the image relocations at load time via the `.reloc`
   section, so no crt0 is required. `_fltused` (MS ABI CRT symbol, referenced
   by GNU-EFI's print code) is provided in `src/main.c`.

## Layout notes

The sources use GNU-EFI idioms:

- Includes: `<efi.h>` + `<efilib.h>` (not EDK II's `<Library/...>`).
- Entry point: `efi_main(EFI_HANDLE, EFI_SYSTEM_TABLE*)` calling
  `InitializeLib()` (not `UefiMain`).
- Boot/Runtime tables: `BS`, `ST`, `RT` globals (not `gBS`, `gST`, `gRT`).

## Testing

Run the EFI application from a UEFI shell under QEMU + OVMF:

```bash
brew install qemu edk2-ovmf    # or download OVMF.fd
mkdir -p esp/EFI/BOOT
cp build/EFI-Mac-Emulator.efi esp/EFI/BOOT/BOOTX64.EFI
qemu-system-x86_64 -bios $(brew --prefix edk2-ovmf)/share/edk2-ovmf/OVMF.fd \
    -drive file=fat:rw:esp,format=raw -net none
```

Boot the shell (or let it auto-boot `BOOTX64.EFI`) and observe the emulator's
status output.

## Directory Structure

```
EFIMac/
├── src/                    # Source code files
│   ├── main.c              # Main entry point (efi_main)
│   ├── cpu/                # CPU translation components
│   ├── memory/             # Memory management
│   ├── hardware/           # Hardware abstraction
│   ├── boot/               # Bootloader system
│   ├── utils/              # Utility functions
│   └── platform/           # UEFI interface
├── third_party/gnu-efi/    # GNU-EFI headers + runtime (git-ignored, auto-cloned)
├── Makefile                # Build configuration
├── README.md               # Project overview
├── ARCHITECTURE.md         # Design notes
├── TODO.md                 # Implementation plan
└── USER_GUIDE.md           # User documentation
```

## Troubleshooting

1. **`make` fails to clone GNU-EFI**: `git.code.sf.net` may be unreachable;
   clone it manually into `third_party/gnu-efi`:
   ```bash
   git clone --depth 1 https://git.code.sf.net/p/gnu-efi/code third_party/gnu-efi
   ```
2. **`clang: error: no such file .../efi.h`**: run `make` from the repo root, or
   check that `third_party/gnu-efi/inc/efi.h` exists.
3. **Undefined `_fltused`**: this MS ABI CRT symbol must be defined somewhere in
   the link; `src/main.c` already provides it — do not remove it.
4. **Image won't load in firmware**: confirm `make check` reports
   `Subsystem = EFI application` and a `Base Relocation Directory`; rename the
   file to `BOOTX64.EFI` on a FAT partition.

## License

This project is licensed under the GNU General Public License, version 3 or
later. See the [LICENSE](LICENSE) file for details.

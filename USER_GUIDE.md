# EFI Mac OS Boot Layer — User Guide

## Status

The boot layer is functional as a UEFI application: it builds, boots under
QEMU/OVMF, runs its self-tests, stages System Folder files and drivers from real
classic Mac discs, and detects and installs a genuine New World `Mac OS ROM`
from a Mac OS 8.5+/9 disc. **It does not yet boot a Mac OS guest** — there is no
MMU or continuous execution, so the installed ROM is not run for real. This
guide covers what works today and how to exercise it.

## What You Need

- **Host**: x86_64 with QEMU (and OVMF firmware). Windows and macOS builds are
  supported; see [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md).
- **A classic Mac disc image** for testing: a System 7 floppy/disc image
  (raw HFS, e.g. `System7_5_3.img`), or a Mac OS 8/9 install ISO
  (e.g. `Mac OS 9.2.2.iso`).
- **Firmware** (optional but recommended): a genuine Old World ROM dump for
  System 7. A New World `Mac OS ROM` is auto-discovered from Mac OS 8.5+ discs.

## Build

Windows (git-bash):

```bash
bash scripts/build-windows.sh
```

macOS/Linux (`brew install llvm lld`):

```bash
make
make check
```

Output: `build/EFI-Mac-Emulator.efi`.

## Run Under QEMU

A helper script builds an OVMF ESP and captures the serial log:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run-qemu-windows.ps1
```

Attach a Mac disc:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run-qemu-windows.ps1 `
  -MacDisc "$env:TEMP\opencode\mac\Mac OS 9.2.2.iso"
```

The serial log lands at `$env:TEMP\opencode\boot_out.txt`. Check the result:

```powershell
Select-String -Path "$env:TEMP\opencode\boot_out.txt" -Pattern "self-test complete|Boot state"
```

What to expect:

- PowerPC CPU self-test 35/35 (includes the FPU core).
- With a Mac OS 8.5+ disc attached: the real New World ROM is found and
  installed (`System ROM loaded from HFS volume 'Power Mac G4 Install':
  2763530 bytes` → `System ROM installed: ... (New World)`), and the boot
  memory-map self-test passes 5/5.
- Without such a disc: the demo ROM is installed and the boot self-test passes
  7/7.
- System Folder staging: System, Finder, and up to 64 Extensions are staged from
  the disc; the system-files self-test passes 7/7.
- The app reports `Boot state: ready=1 ...` and returns cleanly to the firmware.

## Providing a ROM

The ROM loader uses this priority:

1. `\System\MacOS\ROM` on the EFI System Partition — an **Old World** firmware
   dump. Required for System 7.
2. `\System Folder\Extensions\Mac OS ROM` on the ESP.
3. `Mac OS ROM` auto-discovered on an attached disc (New World, Mac OS 8.5+).
4. Demo ROM fallback (self-check only).

To test Old World path: copy a real firmware dump to
`esp\System\MacOS\ROM` in the ESP directory used by the run script, then boot.
The log will print `System ROM installed: ... (Old World)`.

## What Works Today

- Full UEFI initialization: GOP framebuffer (blits verified pixel-by-pixel),
  Block I/O (real sector reads), Simple Network Protocol (real frame transmit),
  audio ring buffer self-checks.
- In-emulator HFS reading of System 7 / Mac OS 8 / Mac OS 9 disc images:
  catalog lookup, auto block-size, multi-extent files.
- Real New World ROM discovery + install at `0xFFF00000` (read-only) with CHRP
  signature verification.
- System Folder / driver staging into guest staging areas with read-back
  verification.

## Limitations

- The guest OS does not boot: the ROM is installed but not executed (no MMU, no
  continuous fetch/execute, no Mac device register emulation).
- System 7 requires a user-supplied Old World ROM; a New World `Mac OS ROM`
  cannot serve System 7.
- HFS filename bytes are MacRoman/Latin-1; names with high-bit characters print
  as console garbage (cosmetic).
- Classic Mac OS files are copyrighted by Apple; none are included in this
  repository.

## Troubleshooting

1. **No serial log / app not running**: confirm OVMF is present and the image is
   named `EFI\BOOT\BOOTX64.EFI` on a FAT partition (the run script does this).
2. **Demo ROM installed unexpectedly**: the disc either has no `Mac OS ROM` (a
   pre-8.5 disc) or it was not detected. Check the log lines around `System ROM
   not found` / `Mac OS ROM file not found`.
3. **System 7 disc staged nothing**: System 7.5.3's `Finder` is a genuine
   0-byte stub and is skipped; System and the two real Extensions still stage.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

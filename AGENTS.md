# AGENTS.md — Working on EFIMac

Agent-oriented notes for building, running and debugging EFIMac. Humans should
start with [README.md](README.md); design detail is in
[ARCHITECTURE.md](ARCHITECTURE.md).

## What this is

A single UEFI x86_64 application (the "heavy bootloader") that stages a classic
PowerPC Mac boot environment and runs the guest's own firmware + OS on a
PowerPC interpreter. Output artifact: `build/EFI-Mac-Emulator.efi`.

## Environment

- Host OS: Windows. Shell: PowerShell 7 (`pwsh`).
- Toolchain: LLVM/clang + `lld-link` (`C:\Program Files\LLVM\bin`), QEMU
  (`C:\Program Files\qemu`), git-bash (`C:\Program Files\Git`).
- `make` needs a POSIX shell, so builds run through git-bash.
- Scratch/work dir: `%TEMP%\opencode` (pre-approved). Boot logs, OVMF firmware,
  the ESP directory and staged discs live there.

## Build

```powershell
& "C:\Program Files\Git\bin\bash.exe" -lc 'cd /c/Users/clayc/Desktop/EFIMac && bash scripts/build-windows.sh 2>&1 | tail -8'
```

(`tail`/`head` do not exist in `pwsh`; use git-bash for them, or `Select-Object`.)

The build is `-Wall -Werror`; a clean build has no warnings. On macOS/Linux use
`make` / `make check`. See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md).

## Run (QEMU + OVMF)

Run the PowerShell helper **directly in `pwsh`** — do not wrap it in git-bash:

```powershell
Remove-Item -Recurse -Force "$env:TEMP\opencode\mac_disc\C__" -ErrorAction SilentlyContinue
& "C:\Users\clayc\Desktop\EFIMac\scripts\run-qemu-windows.ps1" `
    -MacDisc "$env:TEMP\opencode\mac_disc\Mac_OS_9.2.2.iso" -Seconds 90
```

The script copies the freshly built `.efi` to the ESP for you
(`$env:TEMP\opencode\esp\EFI\BOOT\BOOTX64.EFI`), stages the disc to a
space-free path, and captures serial output.

Useful switches: `-MacDisc`, `-OldWorldRom`, `-Seconds`, `-StallSeconds 0`
(no-output stall detector off — the guest is quiet for long stretches),
`-NoReboot`, `-StopOnPattern`, `-TimelinePattern`.

**Clear the staged disc scratch dir before a run.** QEMU/Windows can leave a
`C__` folder under `mac_disc\` that breaks the next run.

### Discs and ROMs (local, git-ignored)

- `mac_discs\MacOS 8 (Version 8.1)...iso`, `mac_discs\System7_5_3.img`,
  `mac_discs\Apple Mac OS 9.2.2 [PowerMac G4].7z`.
- `mac_roms\` Old World ROM dumps.
These are copyrighted and never committed.

## Reading the results

- Serial log: `%TEMP%\opencode\boot_out.txt`. Stderr: `boot_out.txt.err`.
  Run outcome: `qemu_result.txt`.
- The serial log separates lines with `..` and contains ANSI escapes; strip
  `\x1b\[[0-9;?]*[A-Za-z]` before parsing.
- Useful markers: `self-test`, `Boot state:`, `System ROM installed`,
  `Hello from the replacement multitasking NanoKernel`, `converting PMDTs to
  areas`, `UNBOUNDCALL <name>` (an unimplemented Toolbox import — the guest got
  back 0), `GUEST STOP` (reserved/unsupported opcode).

## Self-tests

The application runs its own self-tests and prints `PASS`/`FAIL` on serial:
PowerPC CPU 35/35 (incl. FPU), boot memory map, graphics multi-frame blit,
audio, Block I/O, HFS, network, and System Folder/driver staging 7/7.

## Hardware timeouts / gotchas

- The `bash` tool here has a ~120 s default timeout. Long soaks get killed;
  keep `-Seconds` within the timeout or raise the tool `timeout`.
- QEMU occasionally fails to start on the first attempt or leaves an empty
  `boot_out.txt`; re-run.
- Never leave an orphan QEMU; the run script reclaims it, but if you start QEMU
  by hand, kill it (`Stop-Process -Name qemu-system-x86_64`).
- Function-scope `static` counters are **not** zero-initialized in this
  freestanding build. Gate debug probes on an executed-instruction counter, not
  on a zero-init flag.

## Layout

```
src/main.c                          efi_main + boot orchestration
src/cpu/interpreter.c               PPC execute loop, guest map, host managers
src/cpu/m68k.c                      68K engine (guest DR support)
src/cpu/emul_op.c                   EMUL_OP device handlers
src/cpu/translation_impl.c          exceptions + shared PPC helpers
src/boot/bootloader_impl.c          ROM install, HFS boot probe, staging, ROM patches
src/boot/pef_loader.c               PEF/CFM loader for the System fragment
src/fs/hfs.c                        in-emulator HFS/HFS+ reader
src/hardware/abstraction_impl.c     GOP, Block I/O, SNP, audio
src/memory/manager_impl.c           guest RAM + region map
src/ui/ui_impl.c                    on-screen UI + setup menu
src/utils/debug_impl.c              log + timers
src/platform/uefi_interface_impl.c  UEFI protocol discovery
scripts/build-windows.sh            build
scripts/run-qemu-windows.ps1        run under QEMU/OVMF
tool_scripts/                       one-off RE/disassembly helpers (Python, capstone)
tools/                              host copies of clang/lld-link/llvm-objdump
```

`tool_scripts/` holds many small Python analyzers built while reverse-engineering
the ROM (disassemblers, xref finders, HFS/PEF probes). They expect ROM/RAM dumps
under `%TEMP%\opencode`.

## Conventions

- Commit messages use a subsystem prefix: `CFM -`, `NK -`, `DR -`, `PPC -`,
  `M68K -`, `HFS -`, `boot -`.
- Add significant debugging transcripts under `Session_Transcripts/`.
- Only commit when asked. Do not commit Mac OS files, ROMs or disc images.

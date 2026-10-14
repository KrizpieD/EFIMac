# EFIMac — A Heavy UEFI Bootloader for Classic Mac OS

EFIMac boots classic Mac OS (System 7 through Mac OS 9) on ordinary x86_64
machines. It ships as a single UEFI application: the firmware boots it, it
builds the environment a PowerPC Mac expects, and then it runs that Mac's own
firmware and operating system on an interpreter.

> **It is a bootloader, not an emulator.** EFIMac does not reimplement classic
> Mac OS. It provides the memory image, ROM window, low-memory globals, device
> windows and UEFI-backed hardware that a real PowerPC Mac's firmware expects,
> and then executes the guest's *own* firmware and OS. UEFI protocols are the
> hosting layer underneath.

## Why

A PowerPC Mac's firmware (`Mac OS ROM`, or an Old World ROM dump) and the
operating system it starts are ordinary PowerPC code. They only need a
sufficiently faithful hardware environment. EFIMac supplies that environment
using UEFI standard protocols so the result runs on commodity x86_64 hardware —
no PowerPC CPU, no Mac logic board.

The long-term goal is an EFI application that installs and boots a real classic
Mac OS from a real Mac disc, end to end.

## What it does

- **Reads real Mac discs in place.** An in-emulator HFS/HFS+ reader parses
  attached floppy/disc/CD images directly (auto block size, catalog lookup,
  multi-extent files) — no host mount required.
- **Installs guest firmware.** A real New World `Mac OS ROM` is auto-discovered
  on a Mac OS 8.5+ disc; an Old World firmware dump can be supplied on the boot
  volume. A small demo ROM keeps the install path exercisable without either.
- **Builds a classic Mac memory map.** Guest RAM, a 256 KB low-memory globals
  region, the `0x68000000` nano-kernel system area, staging areas for the System
  Folder, and a read-only ROM window are all wired into the interpreter.
- **Runs the guest on a PowerPC interpreter.** Decode/execute of the full
  integer + FPU instruction set, timebase/decrementer tick, exceptions delivered
  through the guest's own vector table, and the guest's own 68K "DR" emulator
  running natively as the PPC code it actually is.
- **Hosts the guest's managers.** Where the guest's bootstrap expects a working
  Toolbox (a Memory Manager zone, QuickDraw, the Window/Dialog/Menu/Event
  managers), the bootloader backs the CFM imports with host implementations.

## Status

EFIMac is an active research/engineering project. It boots real firmware well
past early bring-up; it does not yet reach the desktop.

**Working end to end:**

- Builds on Windows (git-bash + clang/lld-link) and macOS (`make`).
- Self-tests pass: PowerPC CPU 35/35 (incl. FPU), boot memory map, System
  Folder/driver staging 7/7.
- With a Mac OS 9.2.2 install disc, the 2.4 MB New World `Mac OS ROM` is
  discovered, installed read-only, and executed.
- The ROM's nano-kernel boots: coherence groups, address spaces, BAT setup,
  ready queues, blue/idle tasks, timeslicing, then VMM zone carving
  ("converting PMDTs to areas").
- The System's 68K bootstrap runs under the guest's own PPC DR emulator, reaches
  the CFM entry fragment, and — now that a **host Memory Manager** backs the
  Toolbox zone calls — completes its manager initialization
  (`InitGraf`/`InitFonts`/`InitWindows`/`InitMenus`, Resource/Dialog/Event/
  Process managers) and settles into its `WaitNextEvent` event loop.

**Not yet:**

- **The desktop is not reached.** The biggest remaining wall is Toolbox
  coverage: on the order of ~109 Toolbox imports are still unimplemented and
  return zero, so no windowing, drawing, menu or event content is produced and
  the guest idles in its event loop. These are being implemented manager by
  manager.
- **No MMU/translation model.** The guest runs on a flat direct map; the
  firmware's BAT/SDR1 translation handshake is not yet exercised.
- **System 7** needs a user-supplied Old World ROM (a New World `Mac OS ROM`
  cannot serve it); that path is not validated end to end.

See [ARCHITECTURE.md](ARCHITECTURE.md) for how it works and what's next.

## Guest memory map

| Region                  | Guest address | Size        | Access |
|-------------------------|---------------|-------------|--------|
| Low-memory globals      | `0x00000000`  | 256 KB      | R/W    |
| Boot RAM bank           | `0x00000000`  | 16 MB       | R/W    |
| System/Finder staging   | `0x20000000`  | 16 MB       | R/W    |
| Extensions (drivers)    | `0x21000000`  | 32 MB       | R/W    |
| Nano-kernel system area | `0x68000000`  | 128 MB      | R/W    |
| OS-runtime staging      | `0x68E00000`  | 16 MB       | R/W    |
| New World ROM window    | `0xFFF00000`  | 4 MB        | R (ROM)|

The New World ROM is also mapped at `0x68000000 + 0x360000` style aliases and
executed from there; the SCC console device sits at guest `0x20000`. This is a
host-defined environment tuned so the guest's own expectations are met, not a
byte-for-byte reproduction of a specific Mac.

## ROM sourcing

1. `\System\MacOS\ROM` on the boot volume — an Old World firmware dump
   (required for System 7).
2. `\System Folder\Extensions\Mac OS ROM` on the boot volume.
3. A `Mac OS ROM` file auto-discovered on an attached Mac OS 8.5+ disc
   (New World).
4. A demo ROM fallback (self-check only — cannot boot an OS).

## Building

Windows (git-bash):

```bash
bash scripts/build-windows.sh
```

macOS/Linux (`brew install llvm lld`):

```bash
make
```

Output: `build/EFI-Mac-Emulator.efi`. See
[BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md).

## Running

Boot the built image under QEMU + OVMF and attach a classic Mac disc:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run-qemu-windows.ps1 `
  -MacDisc "$env:TEMP\opencode\mac_disc\Mac OS 9.2.2.iso"
```

Serial output is captured to `$env:TEMP\opencode\boot_out.txt`. See
[USER_GUIDE.md](USER_GUIDE.md) for options, ROM staging and what to expect.

## Layout

```
src/
├── main.c                     # efi_main: subsystem init + guest handoff
├── cpu/
│   ├── interpreter.c          # PPC decode/execute, guest map, SCC, host managers
│   ├── m68k.c                 # 68K engine (used by the guest's DR layer)
│   ├── emul_op.c              # EMUL_OP device-selector handlers
│   └── translation_impl.c     # Exceptions + shared PPC helpers
├── memory/manager_impl.c      # Guest RAM (UEFI pages) + region mapping
├── hardware/abstraction_impl.c# GOP framebuffer, Block I/O, SNP, audio ring
├── boot/bootloader_impl.c     # ROM install, HFS boot probe, System Folder staging
├── boot/pef_loader.c          # PEF/CFM loader for the System fragment
├── fs/hfs.c                   # In-emulator HFS/HFS+ reader
├── ui/ui_impl.c               # On-screen UI
├── utils/debug_impl.c         # Log + timers
└── platform/uefi_interface_impl.c
```

## Prior art

EFIMac is an independent take, but it is not the first attempt at this and it
leans on what others learned. It is **similar to SheepShaver in some ways** (the
idea of supplying the guest's low-memory/ROM bootstrap globals and a
paravirtual Host, and reusing the guest's own 68K emulator), and **takes a
little from DingusPPC** (its treatment of the nanokernel/DR as PPC code and its
device/interrupt models). Basilisk II and QEMU are additional references for
classic Mac device behaviour.

## Notes

- Classic Mac OS and Mac firmware are copyrighted by Apple. This repository
  contains none of them; you must supply your own ROMs and discs.
- License: GPL-3.0-or-later. See [LICENSE](LICENSE).

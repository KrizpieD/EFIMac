# EFIMac — Heavy Bootloader for Classic Mac OS on x86_64 UEFI

EFIMac is a **heavy UEFI bootloader** for classic Mac OS (System 7, Mac OS 8,
Mac OS 9) on x86_64 UEFI systems. It boots as an EFI application, builds a
classic PowerPC Mac memory image (guest RAM, low-memory globals, a read-only ROM
window, staged System Folder files and drivers), installs Mac firmware into that
image, and executes the guest's own firmware and operating system on a PowerPC
interpreter backed by standard UEFI protocols.

> Terminology: EFIMac is a *bootloader*, not an emulator. It provides the
> hardware environment a real PowerPC Mac firmware expects and then runs that
> firmware (and the OS it boots) on the host CPU through an interpreter. The
> abstraction surface is UEFI protocol implementations driving real x86_64
> hardware.

## Project Overview

Classic Mac OS expects a PowerPC Mac: a ROM window at `0xFFF00000` (New World)
or the ROM/ISA windows of an Old World Mac, system globals in low memory, a boot
volume holding the System Folder, and hardware devices behind specific register
windows. EFIMac provides that environment from a UEFI bootloader:

- **UEFI standard protocols are the hardware abstraction.** Graphics (GOP),
  storage (Block I/O / Simple File System), and networking (Simple Network
  Protocol) are used directly as the platform's I/O; simulated Mac devices
  (framebuffer window, audio ring buffer) are wired to those inputs.
- **An in-emulator HFS reader** reads classic Mac discs directly (System 7
  floppy/disc images and Mac OS 8/9 install discs), with automatic block-size
  detection, catalog-based lookup, and multi-extent file support.
- **A PowerPC interpreter with a continuous fetch/execute loop** runs the
  installed firmware for real: fixed 32-bit opcodes, GPR/SPR/FPU state, a
  multi-region guest memory map, a timebase/decrementer scheduler tick, and
  PowerPC exception delivery through the firmware's own vector table.
- **Mac firmware sourcing.** New World `Mac OS ROM` files are auto-discovered on
  attached discs (verified with a genuine Mac OS 9.2.2 install disc); Old World
  firmware is supplied by the user as a ROM dump on the boot volume; a demo ROM
  keeps the full install path exercisable without either.

## Current Status

Booting today under QEMU + OVMF with a Mac OS 9.2.2 install disc attached:

- The 2,763,530-byte `Mac OS ROM` (`<CHRP-BOOT>` signature) is auto-discovered,
  installed read-only at guest `0xFFF00000`, and executed continuously by the
  interpreter.
- The nano-kernel engine embedded in the ROM boots live: **"Hello from the
  replacement multitasking NanoKernel. Version: ..."**, coherence group setup,
  address spaces, BAT setup, ready queues, blue task, timeslicing, idle task,
  then **"converting PMDTs to areas"** (VMM zone carving via the free-list
  walker).
- Task creation with PPC/68K interleave: scheduler context restore
  (`RETTASK`), DRYIELD trap round-trips into 68K dispatch, and the DR console
  path is reached.
- The **DR console read loop is live** at guest `0x40B2751C`; the SCC receive
  device is emulated (status `0x20002`, data `0x20006`) and the boot console
  drains queued characters during the banner flush.
- **Fix B (2026-09):** the NK scheduler idle-frame queue-head mirror is
  implemented, eliminating the SBIDLE deadlock; the scheduler loops cleanly
  (26× SBODY / 64× SCHEDENT) and terminal setup proceeds each pass.
- Builds on Windows (git-bash + clang/lld-link) and macOS (`make`); runs a full
  self-test suite (PowerPC CPU 35/35 incl. FPU, boot memory map, System
  Folder/driver staging 7/7); drives real GOP/Block I/O/SNP hardware.

Not yet:

- **Boot advances past the idle sell.** The emulator now fabricates the external
  interrupt through the guest's own D0 level-9 dispatch (see
  [ARCHITECTURE.md](ARCHITECTURE.md)): at the idle give-up (`0x40B24F04`) it
  raises EXT, seeds the IC dispatch tables so the firmware's KCALLSAVE → level-9
  ISR → dispatch rfi runs for real, forces a sane resume MSR, and lands the
  preempted task back at its sell-glue continuation. This is a stable, repeatable
  task switch (180 s soaks, no spinlock/crash), but it does **not yet advance
  boot** — the idle task re-sells forever. The missing wake appears to be a
  timer/DEC, the warm/CSR branch at `0x40B126E8`, or an unemulated KDP service
  rather than more SCC input (the 4 queued `'g' CR 'g' CR` bytes are already
  consumed).
- **SCC receive interrupt → DRAM input ring.** The DR debugger wait ultimately
  needs the SCC's Rx interrupt path to move received bytes from the SCC FIFO
  into the nano-kernel's DRAM input ring; machine-level interrupt routing
  (interrupt controller + SCC IRQ) is on the roadmap once the blocking wake is
  identified.
- No MMU/translation model yet (the nano-kernel runs on a flat alias today; the
  firmware-side BAT/SDR1 handshake is not exercised).
- Finder / OS desktop not reached; System 7 Old World ROM path not yet
  validated end-to-end (a New World `Mac OS ROM` cannot serve System 7).

For the current boot-state details and the interrupt-machinery roadmap, see
[ARCHITECTURE.md](ARCHITECTURE.md) and [TODO.md](TODO.md).

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

Guest firmware executes with KDP at `0xA000` and the nano-kernel/DR working set
around `0x40B00000`; the SCC console device is mapped at `0x20000`.

## Source Layout

```
src/
├── main.c                     # efi_main: subsystem init + guest handoff
├── cpu/
│   ├── interpreter.c          # PowerPC decode/execute loop, register file,
│   │                          #   guest memory map, SCC/timebase/decrementer
│   ├── m68k.c                 # Legacy 68K layer (runtime-only, not the boot path)
│   ├── translation.h/.c       # Exception delivery + shared PPC helpers
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

and then the nano-kernel milestone stream (`Hello from the replacement
multitasking NanoKernel`, `REPLACEMENT_mK`, scheduler ticks, DR console reach).

## Important Notes

- Classic Mac OS files are copyrighted by Apple. This project does not include
  any Mac OS or Mac firmware files; you must supply your own ROMs and discs.
- For System 7, place a genuine Old World ROM dump at `\System\MacOS\ROM` on
  the boot volume. A New World `Mac OS ROM` (from Mac OS 8.5+) is detected
  automatically from discs and booted, but cannot serve System 7.
- The guest currently runs firmware on a flat alias; real MMU/translation
  (BATs/SDR1) serving is future work.
- Existing open-source projects (SheepShaver, Basilisk II, QEMU, DingusPPC)
  are valuable references for the PowerPC and Mac device semantics this project
  re-creates on UEFI. DingusPPC's interrupt-controller (Grand Central / VIA) and
  SCC models are the reference for EFIMac's machine wiring work.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

## Version

- **Version**: 0.3
- **Status**: Nano-kernel boots through init, PMDT->areas, and task creation to
  the live DR console wait; interrupt machinery (SCC->IRQ->input ring) is the
  current work.
- **Target**: x86_64 UEFI.

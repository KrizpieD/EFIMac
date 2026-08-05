# EFI Mac OS Boot Layer — Implementation Plan

## Current State

The project is a functional **heavy UEFI bootloader** for classic Mac OS. It
builds a PowerPC Mac boot image from UEFI standard protocols, reads classic Mac
discs in place, installs real Mac firmware into the guest image, and self-tests
the whole path. The guest OS does **not** boot yet — the remaining milestone is
real execution.

### Verified end-to-end (Windows host, QEMU + OVMF)

- PowerPC CPU self-test **35/35** (includes the FPU core: opcodes 48-63 gated on
  MSR[FP], FP-unavailable exception 0x800, FPSCR, A-form arithmetic).
- Boot memory-map self-test **7/7** with the demo ROM, **5/5** with a real ROM
  (region presence + CHRP signature + read-only enforcement).
- System Folder / driver self-test **7/7**.
- **Real New World ROM discovered and installed** from a genuine Mac OS 9.2.2
  install disc (`Power Mac G4 Install:System Folder:Mac OS ROM`, 2,763,530
  bytes, `<CHRP-BOOT>` signature) at guest `0xFFF00000`.
- All non-empty Extensions stage with **0 failures**: System 7.5.3 2/2, Mac OS
  8.1 18/18, Mac OS 9.2.2 25/25 (up to 64 drivers supported).
- Graphics blits verified across every GOP pixel; Block I/O and SNP exercised
  with real hardware calls.

### Recent work

- **Heavy-bootloader framing.** UEFI protocols (GOP/BlockIO/SNP/SimpleFS) are
  the hardware abstraction; simulated Mac devices are wired to them. Docs and
  boot output reframed from "emulator" to "boot layer".
- **ROM type awareness.** `PPC_ROM_TYPE_OLD_WORLD/NEW_WORLD/DEMO`; the boot
  self-test no longer assumes the demo ROM's `ROM1`/reset-vector layout when a
  real ROM is installed.
- **HFS driver staging.** Catalog-ID-based file lookup
  (`PpcHfsGetEntryById`), whole-catalog `Mac OS ROM` search
  (`PpcHfsFindMacOsRom`), auto block-size detection, multi-overflow extents,
  empty-file skipping (7.5.3's 0-byte Finder).

## Phase Status

### Phase 1: Research and Analysis
- [x] Analyze existing Mac emulators (SheepShaver, Basilisk II, QEMU, DingusPPC)
- [x] Study PowerPC vs 68k architecture differences
- [x] Review UEFI specifications and implementation guidelines
- [x] Document findings in project README

### Phase 2: Architecture Design
- [x] Define module interfaces (headers) for CPU, memory, hardware, boot, utils, platform
- [x] Plan memory management approach (UEFI allocation + multi-region guest map)

### Phase 3: Core Implementation
- [x] Build with a real toolchain (GNU-EFI + clang/lld-link; `-Wall -Werror`)
- [x] PowerPC instruction decoder and interpreter (fixed 32-bit opcodes, GPR/SPR)
- [x] FPU core (opcodes 48-63, FPSCR, MSR[FP] gating, exception 0x800)
- [x] Memory manager backed by UEFI allocation, multi-region guest map with
      read-only ROM enforcement
- [x] Hardware abstraction on real UEFI protocols (GOP, Block I/O, SNP, SimpleFS)

### Phase 4: Emulation Components
- [x] Graphics: GOP mode + guest framebuffer window, verified blits
- [x] Audio: guest RAM ring buffer device (no UEFI audio standard)
- [x] Storage: Block I/O enumeration + real sector reads
- [x] Networking: SNP start/initialize/transmit

### Phase 5: Boot Process
- [x] Multi-region guest memory map (RAM + low-memory globals + read-only ROM)
- [x] ROM load with priority: ESP path → ESP Mac OS ROM → HFS Mac OS ROM →
      demo fallback; Old World/New World identification
- [x] System initialization: CPU reset to reset vector, boot-info block,
      `PpcRunBootSelfTest`
- [x] System Folder scan + System/Finder staging + Extensions driver registry
      (64 drivers) with HFS catalog-ID lookup
- [x] Verify Phase 5 build and boot under QEMU/OVMF on Windows
- [ ] Boot a real guest OS (the headline remaining item)

### Phase 6: Real Boot
- [ ] Continuous guest execution: fetch/decode/execute loop from the ROM reset
      vector, not block-at-a-time self-tests
- [ ] MMU emulation (bat/tlb) so real firmware can set up address translation
- [ ] Exception/interrupt delivery and a timer source for guest scheduling
- [ ] Mac device register emulation in the guest map (VIA/Nubus/PCI-ish windows)
- [ ] Old World ROM boot testing with System 7 (needs a user-supplied ROM dump)
- [ ] New World ROM boot testing (the auto-discovered 9.2.2 ROM)
- [ ] macOS build/run verification (`make clean && make` + QEMU/OVMF)
- [ ] Performance work once the boot loop runs

## Architecture Decisions

### Heavy bootloader, not an application emulator
UEFI standard protocols are the hardware abstraction; guest-visible Mac devices
are thin simulated windows wired to GOP/BlockIO/SNP. This keeps the host-side
code small and lets the guest own the boot process.

### Target architecture: PowerPC
- Better fit for Mac OS 8/9 and for a user-supplied Old World ROM.
- References: SheepShaver, Basilisk II, QEMU, DingusPPC.

### In-emulator HFS reader
The bootloader must read Mac discs without a host filesystem; catalog-ID lookup
avoids name/path separator ambiguity and survives all three test-disc layouts.

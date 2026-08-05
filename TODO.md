# EFI-Mac-Emulator - Implementation Plan

## Current State

Phase 5 (Boot Process) is implemented up to the platform-verification step: the
classic Mac OS boot memory map, ROM loading, system initialization, System
Folder / driver support, and now an initial PowerPC FPU core in the interpreter
(FP registers, FPSCR, FP loads/stores, arithmetic/compare/move/convert).

**FPU core (implemented):** `PpcExecuteInstruction` now gates all FP opcodes
(48-55, 59, 63; 56-58 and 60-62 are reserved and stay `EFI_UNSUPPORTED`) on
MSR[FP] and raises the FP-unavailable exception (0x800) when clear, so an OS
can set MSR[FP] and re-execute. FP D-form loads/stores
(`lfs/lfsu/lfd/lfdu/stfs/stfsu/stfd/stfdu`), X-form execution
(`fcmpu/fcmpo/fctiw/fctiwz/fadd/fsub/fdiv/fneg/fmr/fnabs/fabs/frsp/fsqrt/fres/
mffs/mtfsf/mtfsfi/mtfsb0/mtfsb1/fsel`), and A-form arithmetic (`fmul/fmadd/
fmsub/fnmadd/fnmsub`, single with an `s` suffix; the multiplier rides in the
FRC field) are dispatched. FPSCR handling uses the classic PowerPC 32-bit
layout (bit 0 = MSB: FX/FEX/VX/OX, sticky VX* bits, enable bits VE/OE/UE/ZE/XE,
RN, FPCC), FX/FEX/VX recomputation, CR1 recording, and big-endian guest
single/double loads/stores. FP accessors (`PpcGet/SetFprValue`,
`PpcGet/SetFpscrValue`) are wired into the register API. `PpcHandleException`
maps FP-unavailable to 0x800 and program to 0x700. The self-test gained 17 FP
checks (#17-33), including fmul/fmadd A-form and mtfsfi RN. `PpcDecodeInstruction`
has FP mnemonics for opcodes 48-55 and 59/63. The FPU core is **verified**: the
cross-toolchain (clang/lld-link GNU-EFI) now builds and boots on a Windows host
(chocolatey LLVM 22.1.7 + QEMU/OVMF), and the CPU self-test runs 35/35 checks
including all 17 FP checks (#17-33). **Remaining:** a macOS build/run
(`make clean && make`, then QEMU/OVMF) and real Mac OS boot testing.

**Phase 5 recap:** The CPU guest memory map is now multi-region
(`PpcAddGuestMemoryRegion` in `src/cpu/interpreter.c`, up to 8 regions): the
primary 256 MB guest RAM at guest 0x10000000, a dedicated read/write low-memory
globals region at guest 0x00000000 (16 KB), a read-only system ROM window at
guest 0xFFF00000, and read/write staging areas for system files (0x20000000,
16 MB) and drivers (0x21000000, 8 MB).
`PpcInstallLowMemory`/`PpcInstallSystemRom`/`PpcInstallDemoRom` in
`src/boot/bootloader_impl.c` install the regions; the demo ROM is a
self-contained image with a reset-vector program. `PpcPrepareSystemForBoot`
resets the CPU to the ROM reset vector with `MSR = ME|RI` and writes an emulator
boot info block (magic + RAM/ROM geometry) into low memory.
`PpcRunBootSelfTest` verifies: low-memory read/write, ROM magic word `ROM1`
readable, ROM read-only enforcement, and a cross-region ROM -> RAM program
executed from the reset vector.

**System files & drivers:** `PpcLocateSystemFolder` scans the boot volume for
`\System Folder` and detects System, Finder, Extensions, and the Mac OS ROM
file. `PpcLoadSystemFiles` stages System/Finder/Mac OS ROM into the system
staging area; `PpcScanExtensionsDirectory` enumerates `\System Folder\
Extensions` via real directory reads into a driver registry and `PpcLoadDrivers`
stages each one. The ROM loader falls back from `\System\MacOS\ROM` to the
`Mac OS ROM` file before the demo ROM. `PpcRunSystemFilesSelfTest` verifies
staged files read back through the interpreter memory path, driver readback, and
that the low-memory boot info survives staging. `src/main.c` wires the Phase 5
sequence (install low memory -> install ROM with demo fallback -> memory-map
self-test -> prepare -> scan/stage system files and drivers -> self-test ->
report). Fixed `BootDirectoryExists` to treat `EFI_NOT_FOUND` as a graceful
"directory absent" outcome (mirroring `BootFileExists`) instead of aborting the
System Folder scan, so the scan runs and records itself even when no System
Folder exists; the system-files self-test now passes 5/5 on a minimal test
volume. Verified end-to-end under QEMU/OVMF on Windows: CPU self-test 35/35,
boot self-test 7/7, system-files self-test 5/5, then clean boot to the firmware
UI. A macOS build/run is still pending.

**Phase 4 recap** (complete and verified in a single QEMU/OVMF run alongside
the Phase 3 work):

**Phase 3 recap:** All `_impl.c` files use real UEFI services. Interpreter passes
an 18-check self-test. Memory manager allocates 256 MB of guest RAM via
`AllocatePages`; a small PPC program is executed from guest RAM at boot.
Bootloader `PpcLoadKernel` resolves the boot volume, opens
`\System\MacOS\kernel` from the FAT volume, and reads it into guest RAM;
`PpcVerifyKernel` bounds-checks and reads the first word; `PpcBootSystem`
configures PC/SRR0/SRR1/MSR. Debug writes `boot.log` via real file I/O and uses
`GetNextMonotonicCount` for timers. UEFI interface functions use real
`HandleProtocol`/`LocateHandleBuffer`/`GetMemoryMap`.

**Graphics (GOP framebuffer):** `PpcInitializeGraphics` sets a real GOP mode
and carves a guest-visible framebuffer window out of guest RAM at guest
0x18000000 (640x480x32). Guest code writes big-endian 0xRRGGBB00 pixels there;
`PpcGraphicsBlitToDisplay` converts and copies the window to the real GOP
framebuffer (byte-exact RGB and BGR pixel layouts). `PpcGetFrameBufferInfo`
reports the window plus the GOP base/pitch/pixel format,
`PpcGraphicsClear`/`SetPixel`/`DrawRect` provide host-side helpers. The
self-check now includes a multi-frame test: three solid full-screen frames
(red/green/blue) verified across every pixel of the GOP framebuffer, a
four-band frame checked at centers and boundaries, corner pixels, and
out-of-bounds writes. This test caught and led to a fix of a blit bug where
channels were shifted 8 bits too high (blue landed in the reserved byte).

**Networking (SNP):** `PpcInitializeNetwork` enumerates every UEFI Simple
Network Protocol instance, calls real `Start`/`Initialize` on each, sets the
standard receive filter, snapshots the real mode (MAC, media present, iftype,
max packet), and transmits a real 64-byte ARP-style frame via `Transmit` with a
`GetStatus` poll. Every initialized interface is stored in
`PPC_NETWORK_INFO.Interfaces[]` (up to `PPC_MAX_NETWORK_INTERFACES`), so
multi-NIC systems are fully brought up and tested. `PpcGetNetworkInfo` reports
per-interface results.

**Storage (Block I/O):** `PpcInitializeBlockIo` enumerates every UEFI Block I/O
protocol instance and reports real geometry (block size, block count,
removable/read-only flags). `PpcReadDiskBlock` performs a real `ReadBlocks`
sector read. Self-check reads LBA 0 of every device until it finds the "EFI"
marker. `PpcGetBlockIoInfo` reports the state.

**Audio (emulated device):** UEFI has no standard audio output protocol, so the
audio "device" is a fixed ring buffer inside guest RAM at guest 0x18800000
(8KB, 44100 Hz stereo 16-bit). Guest code fills it with big-endian PCM samples;
the host reads them back and advances a play cursor. `PpcAudioGetBufferInfo`,
`PpcAudioWriteSample`, `PpcAudioReadSample`, `PpcAudioAdvancePlayback`,
`PpcAudioReset` implement the device. Self-check verifies a two-sample write via
the guest RAM path reads back correctly and playback advances.

**Verified in QEMU/OVMF (single boot, mixed devices):** Graphics self-check
PASS plus the full multi-frame test PASS; Audio self-check PASS; Block I/O
self-check PASS (4 devices: FAT ESP + IDE raw disk + virtio-blk raw disk +
partition, marker found); Network self-check PASS for **both** an e1000 NIC
(MAC ...:56) and a virtio-net-pci NIC (MAC ...:57) simultaneously — both
transmit PASS; plus all Phase 3 checks still PASS (18/18 self-test, guest RAM
demo, kernel load/verify/execute, boot.log). Build is clean with
`make clean && make` (GNU-EFI + clang/lld-link on macOS).

## Phase 1: Research and Analysis
- [x] Analyze existing Mac emulators (SheepShaver, Basilisk II, QEMU, DingusPPC)
- [x] Study PowerPC vs 68k architecture differences
- [x] Review UEFI specifications and implementation guidelines
- [x] Document findings in project README

## Phase 2: Architecture Design
- [x] Define module interfaces (headers) for CPU, memory, hardware, boot, utils, platform
- [ ] Complete CPU translation layer requirements (instruction set coverage, register mapping, MMU)
- [ ] Complete hardware abstraction layer specification (graphics, audio, storage, network)
- [ ] Plan memory management approach (UEFI pool vs. guest memory regions)

## Phase 3: Core Implementation
- [x] Get the UEFI application to build with a real toolchain (GNU-EFI + clang/lld-link)
- [x] Implement CPU instruction decoder and interpreter (PowerPC)
- [x] Implement register file and special-purpose registers (MSR, SRR0/1, CTR, LR)
- [x] Implement memory manager backed by UEFI allocation
- [x] Wire up hardware abstraction interface to real UEFI protocols (graphics -> GOP, storage -> Simple File System; audio/network still simulated)
- [x] Remove placeholder/simulated behavior from current `_impl.c` files

## Phase 4: Emulation Components
- [x] Implement graphics subsystem (framebuffer via GOP)
- [x] Add audio handling
- [x] Integrate storage I/O (UEFI file protocols / block I/O)
- [x] Implement basic networking

## Phase 5: Boot Process
- [x] Create bootloader for Mac OS (load ROM image, set up guest memory map) - first deliverable: multi-region guest memory map (RAM + low-memory globals + read-only ROM window), ROM load from volume with demo-ROM fallback
- [x] Implement system initialization routines - CPU reset to ROM reset vector (MSR ME|RI), boot info block in low memory, `PpcRunBootSelfTest` (low-memory R/W, ROM read-only, cross-region ROM->RAM execution)
- [x] Add support for system files and drivers - System Folder scan (`\System Folder` + System/Finder/Extensions/Mac OS ROM detection), guest staging areas (system 0x20000000, drivers 0x21000000) mapped via the multi-region memory map, Extensions directory enumeration with a driver registry, `PpcRunSystemFilesSelfTest`, and Mac OS ROM file fallback in the ROM loader
- [ ] Test boot process with various Mac OS versions
- [x] Verify Phase 5 build and boot under QEMU/OVMF on Windows (chocolatey LLVM + clang/lld-link; CPU 35/35, boot 7/7, system files 5/5 self-tests pass)
- [ ] Verify Phase 5 on macOS (`make clean && make`)

## Phase 6: Testing and Optimization
- [ ] Test with Mac OS 7, 8, and 9
- [ ] Optimize performance
- [ ] Fix compatibility issues
- [ ] Document usage instructions

## Architecture Decisions

### Target Architecture: PowerPC
- Selected over 68k due to:
  - Better existing support from SheepShaver
  - More complete compatibility with Mac OS 8/9
  - Simpler translation layer compared to 68k CISC instructions

### UEFI Approach
- Leverages modern boot infrastructure
- Direct hardware access capabilities
- Better memory management
- Support for large storage devices

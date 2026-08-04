# EFI-Mac-Emulator - Implementation Plan

## Current State

Phase 5 (Boot Process) first deliverable is implemented: the classic Mac OS boot
memory map, ROM loading, and system initialization routines.

**Phase 5 recap:** The CPU guest memory map is now multi-region
(`PpcAddGuestMemoryRegion` in `src/cpu/interpreter.c`): the primary 256 MB guest
RAM at guest 0x10000000, a dedicated read/write low-memory globals region at
guest 0x00000000 (16 KB), and a read-only system ROM window at guest 0xFFF00000.
`PpcInstallLowMemory`/`PpcInstallSystemRom`/`PpcInstallDemoRom` in
`src/boot/bootloader_impl.c` install the regions; the demo ROM is a
self-contained image with a reset-vector program. `PpcPrepareSystemForBoot`
resets the CPU to the ROM reset vector with `MSR = ME|RI` and writes an emulator
boot info block (magic + RAM/ROM geometry) into low memory.
`PpcRunBootSelfTest` verifies: low-memory read/write, ROM magic word `ROM1`
readable, ROM read-only enforcement, and a cross-region ROM -> RAM program
executed from the reset vector. `src/main.c` wires the Phase 5 sequence
(install low memory -> install ROM with demo fallback -> self-test -> prepare ->
report). Code is written against GNU-EFI idioms; a build/run on macOS
(`make clean && make`, then QEMU/OVMF) is still required to verify.

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
- [ ] Add support for system files and drivers
- [ ] Test boot process with various Mac OS versions
- [ ] Verify Phase 5 on macOS (`make clean && make`) and in QEMU/OVMF

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

# EFI-Mac-Emulator - Implementation Plan

## Current State

Phase 3 (Core Implementation) is complete. All `_impl.c` files now use real
UEFI services with no placeholder/simulated code in the critical path.

**Interpreter + registers:** 18-check self-test all PASS (arithmetic, carry/CR,
branches, SPRs, rlwinm, loads/stores, exception on unknown opcode).

**Memory manager:** `PpcInitializeMemoryManager` allocates 256 MB of guest RAM
via `AllocatePages` (with fallback to `AllocateAnyPages`); guest-to-host address
translation is real and bounds-checked. A small PPC program is written into guest
RAM, fetched, decoded, executed, and verified at boot.

**Hardware abstraction:** Graphics uses the real GOP protocol (real framebuffer,
pitch/format/mode info); storage enumerates real SimpleFS volumes. Audio and
network remain simulated stubs.

**Bootloader:** `PpcLoadKernel` resolves the boot volume via
`HandleProtocol(LoadedImageProtocol)` on the boot device, opens
`\System\MacOS\kernel` from the FAT volume, reads it into guest RAM with a real
`File->Read` loop, and reports the byte count. `PpcVerifyKernel` performs a real
bounds check against the guest RAM region and reads the first 4 bytes (big-endian)
to confirm the data was loaded correctly. `PpcBootSystem` configures the CPU
context (PC, SRR0/SRR1, MSR with ME+RI bits) for transfer of control.

**Debug:** `PpcDebugLogToFile` writes to `boot.log` on the boot FAT volume via
real `File->Open`/`Write`/`Close` chain. `PpcDebugStartTimer` /
`PpcDebugStopTimer` use real UEFI `GetNextMonotonicCount`.

**UEFI interface:** `PpcGetFileSystem`, `PpcLoadFile`, `PpcGetBootDevice`, and
`PpcGetSystemInformation` are fully implemented using real UEFI protocols
(`HandleProtocol`, `LocateHandleBuffer`, `GetMemoryMap`). All functions compile
clean with `-Wall -Werror`.

**Verified in QEMU/OVMF:** 18/18 self-test PASS; guest RAM demo PASS; real GOP
graphics (640x480); 1 real SimpleFS volume; `boot.log` written and confirmed on
host; kernel loaded from disk (16 bytes), verify passes (`first word 0x38600064`);
loaded-kernel execution PASS (r5=700); `PpcBootSystem` configures CPU context.
Build is clean with `make clean && make` (GNU-EFI + clang/lld-link on macOS).

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
- [ ] Implement graphics subsystem (framebuffer via GOP)
- [ ] Add audio handling
- [ ] Integrate storage I/O (UEFI file protocols / block I/O)
- [ ] Implement basic networking

## Phase 5: Boot Process
- [ ] Create bootloader for Mac OS (load ROM image, set up guest memory map)
- [ ] Implement system initialization routines
- [ ] Add support for system files and drivers
- [ ] Test boot process with various Mac OS versions

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

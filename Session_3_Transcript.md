# Analyze project status and execute next objective

**Session ID:** ses_03273b2feffeBdCNxU2EJdpr8N
**Created:** 8/4/2026, 12:12:31 PM
**Updated:** 8/4/2026, 12:23:47 PM

---

## User

Please analyse current project status, determine next objective, and execute the completion of that objective.

---

## Assistant (Build · Big Pickle · 4.0s)

_Thinking:_

Let me start by exploring the current project to understand what's going on.

**Tool: bash**

**Input:**
```json
{
  "command": "git log --oneline -20; git status"
}
```

**Output:**
```
fee7247 KPD - Deleted accidental transcript duplicate.
929987f KPD - Phase 4 implemented and tested. Graphics and Networking should be working.
ec7e6d8 KPD - Phase 3 *actual* completion.
ba8258f KPD - Implementation of interpreter
72713a1 KPD - Successful boot test of build. Updated instructions text.
5963ee6 KPD - EFI toolkits installed, sources adapted accordingly. Inital POC build now succeeds.
2384ee1 KPD - License corrections: (Now GNU GPL 3.0)
0483658 KPD - Status correction work
8de6e5e KPD - Second build attempt.
00c8854 KPD - Added transcript updates from build attempt.
be43484 KPD - First build attempt.
b5080e3 KPD - Phase 3 (Emulation Components). Subsystems implementation attempted.
51e2fb8 KPD - Phase 2 (Core Infrastructure). Codebase started. Basics developed. Lots of stubs.
75d2938 KPD - Initial project commit. Phase 1 (Research and Analysis). Planning Artifacts.
On branch main
Your branch is up to date with 'origin/main'.

nothing to commit, working tree clean

```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac</path>
<type>directory</type>
<entries>
.git/
ARCHITECTURE.md
BUILD_INSTRUCTIONS.md
build/
CMakeLists.txt
FINAL_SUMMARY.md
LICENSE
Makefile
README.md
Session_2_Transcript.md
Session_Transcript.md
src/
TODO.md
USER_GUIDE.md

(14 entries)
</entries>
```

---

## Assistant (Build · Big Pickle · 2.7s)

_Thinking:_

Let me explore the project structure more thoroughly to understand what this project is about.

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\README.md"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\README.md</path>
<type>file</type>
<content>
1: # EFI-Mac-Emulator
2: 
3: A UEFI-based emulator for running classic Mac OS (System 7, Mac OS 8, and Mac OS 9) on modern Intel x86_64 computers.
4: 
5: ## Project Overview
6: 
7: This project creates a UEFI executable that provides:
8: - CPU translation layer for PowerPC architecture
9: - Graphics and basic I/O handling
10: - Bootloader compatibility with classic Mac OS versions
11: - Hardware abstraction for running legacy Mac OS on modern x86_64 systems
12: 
13: ## Goals
14: 
15: ### Primary Objectives
16: 1. Create a UEFI executable that acts as a translation layer between modern x86_64 hardware and classic Mac OS PowerPC architecture
17: 2. Support booting Mac OS 8, 9, and System 7 on modern Intel computers
18: 3. Implement basic graphics, sound, and I/O emulation
19: 4. Provide compatibility with existing Mac OS software ecosystem
20: 
21: ### Technical Approach
22: - Develop UEFI application that initializes the translation layer
23: - Implement CPU instruction set translation (PowerPC)
24: - Create hardware abstraction layer for graphics, storage, and peripheral devices
25: - Design boot process that loads classic Mac OS from modern storage media
26: 
27: ## Architecture
28: 
29: ### Components
30: 1. **UEFI Application**: Main entry point that initializes the emulator environment
31: 2. **CPU Translation Layer**: Handles instruction set translation between x86_64 and PowerPC
32: 3. **Hardware Abstraction Layer**: Provides virtualized hardware interfaces for graphics, sound, disk I/O, etc.
33: 4. **Bootloader**: Loads Mac OS kernel and system files
34: 5. **Memory Manager**: Handles virtual memory management for the emulated system
35: 
36: ### Target Architectures
37: - PowerPC (preferred for initial implementation)
38:   - Supports Mac OS 8/9
39:   - Better compatibility with existing emulators like SheepShaver
40:   - More modern architecture than 68k
41: 
42: ## Current Status
43: 
44: **Pre-alpha scaffolding, but it builds.** The repository currently contains:
45: 
46: - Module interfaces (headers) and partial, mostly placeholder implementations for the CPU translation layer, memory manager, hardware abstraction, bootloader, debug system, and UEFI interface (`src/*/*_impl.c`).
47: - A working build: `make` produces a valid x86_64 UEFI application (`build/EFI-Mac-Emulator.efi`) using GNU-EFI headers/runtime with a clang + lld-link cross-toolchain (see [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md)).
48: 
49: What does **not** exist yet:
50: 
51: - No working PowerPC instruction translation — the current code only decodes a handful of opcodes and does not actually translate or execute anything.
52: - No memory management, graphics, audio, storage, or network emulation beyond placeholder functions that mostly print status messages.
53: - No bootloader that loads real Mac OS images — kernel loading is simulated.
54: 
55: See [TODO.md](TODO.md) for the phase-by-phase plan and what remains to be done.
56: 
57: ### Completed So Far
58: 
59: - **Phase 1: Research and Analysis**
60:   - Analyzed existing Mac emulators (SheepShaver, Basilisk II, QEMU, DingusPPC)
61:   - Studied PowerPC vs 68k architecture differences
62:   - Documented UEFI specifications and implementation guidelines
63: - **Design**: Module interfaces (headers) and a starting skeleton for each subsystem.
64: 
65: ### Not Yet Implemented
66: 
67: - Actual PowerPC instruction set translation / execution engine
68: - Memory manager and MMU emulation
69: - Real graphics, audio, storage, and network device emulation
70: - Bootloader that loads and boots a real Mac OS image
71: 
72: ## Source Code Structure
73: 
74: ```
75: src/
76: ├── main.c                       # Main UEFI application entry point
77: ├── cpu/
78: │   ├── translation.h            # Header for translation functions
79: │   └── translation_impl.c       # Partial implementation of translation logic
80: ├── memory/
81: │   ├── manager.h                # Header for memory manager
82: │   └── manager_impl.c           # Partial implementation of memory manager
83: ├── hardware/
84: │   ├── abstraction.h            # Header for hardware abstraction
85: │   └── abstraction_impl.c       # Partial implementation of hardware abstraction
86: ├── boot/
87: │   ├── bootloader.h             # Header for bootloader functions
88: │   └── bootloader_impl.c        # Partial implementation of bootloader
89: ├── utils/
90: │   ├── debug.h                  # Header for debugging functions
91: │   └── debug_impl.c             # Partial implementation of debugging system
92: └── platform/
93:     ├── uefi_interface.h         # Header for UEFI interface functions
94:     └── uefi_interface_impl.c    # Partial implementation of UEFI interface
95: 
96: Makefile                # Build configuration (make -> build/EFI-Mac-Emulator.efi)
97: ARCHITECTURE.md            # Design notes for the CPU translation layer
98: BUILD_INSTRUCTIONS.md      # Build instructions
99: USER_GUIDE.md              # User documentation
100: TODO.md                    # Implementation plan and status
101: ```
102: 
103: ## Prerequisites for Building
104: 
105: Building this project requires a macOS host with Homebrew:
106: 
107: 1. **UEFI headers/runtime**: GNU-EFI — cloned automatically by the Makefile into `third_party/gnu-efi/`
108: 2. **Toolchain**: `brew install llvm lld` (clang cross-compiles to PE/COFF; lld-link links the UEFI image)
109: 
110: ## Building Instructions
111: 
112: ```bash
113: brew install llvm lld
114: make
115: ```
116: 
117: Output: `build/EFI-Mac-Emulator.efi`. Verify the image with `make check`.
118: See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for details.
119: 
120: ## Testing
121: 
122: The EFI application boots and runs under QEMU + OVMF (verified): it initializes
123: all subsystems, prints its status report, and returns cleanly to the firmware.
124: See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for the exact steps.
125: 
126: Testing an emulator like this requires:
127: 
128: 1. **UEFI firmware capable of running EFI applications** (e.g. QEMU + OVMF)
129: 2. **Classic Mac OS system files**:
130:    - System 7, Mac OS 8, or Mac OS 9 ROM images
131:    - Kernel images for the respective systems
132: 
133: ## Important Notes
134: 
135: This is an ambitious emulator that requires:
136: - A proper UEFI development environment (EDK II or GNU-EFI)
137: - Access to classic Mac OS system files (ROMs, kernels, etc.)
138: - Understanding of both UEFI and PowerPC architectures
139: 
140: The PowerPC translation layer, in particular, is a large undertaking. Existing
141: open-source projects such as SheepShaver, Basilisk II, QEMU, and DingusPPC are
142: valuable references.
143: 
144: ## Future Development
145: 
146: ### Planned Enhancements:
147: - Real PowerPC instruction interpreter (and eventually dynamic translation)
148: - Better audio subsystem
149: - More comprehensive graphics support
150: - Improved performance optimization
151: - Additional hardware device emulation
152: - Enhanced debugging capabilities
153: 
154: ### Compatibility Improvements:
155: - Support for more Mac OS versions
156: - Better memory management
157: - Advanced graphics acceleration
158: - Network protocol improvements
159: 
160: ## License
161: 
162: This project is licensed under the GNU General Public License, version 3 or
163: later. See the [LICENSE](LICENSE) file for details.
164: 
165: ## Version Information
166: 
167: - **Version**: 0.1 (Scaffolding)
168: - **Status**: Pre-alpha — builds a UEFI application, but emulation is scaffolding/placeholders only
169: - **Target Platforms**: x86_64 UEFI systems
170: 
171: This is a work in progress.

(End of file - total 171 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\TODO.md"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\TODO.md</path>
<type>file</type>
<content>
1: # EFI-Mac-Emulator - Implementation Plan
2: 
3: ## Current State
4: 
5: Phase 4 (Emulation Components) is complete and verified in a single QEMU/OVMF
6: run alongside the Phase 3 work.
7: 
8: **Phase 3 recap:** All `_impl.c` files use real UEFI services. Interpreter passes
9: an 18-check self-test. Memory manager allocates 256 MB of guest RAM via
10: `AllocatePages`; a small PPC program is executed from guest RAM at boot.
11: Bootloader `PpcLoadKernel` resolves the boot volume, opens
12: `\System\MacOS\kernel` from the FAT volume, and reads it into guest RAM;
13: `PpcVerifyKernel` bounds-checks and reads the first word; `PpcBootSystem`
14: configures PC/SRR0/SRR1/MSR. Debug writes `boot.log` via real file I/O and uses
15: `GetNextMonotonicCount` for timers. UEFI interface functions use real
16: `HandleProtocol`/`LocateHandleBuffer`/`GetMemoryMap`.
17: 
18: **Graphics (GOP framebuffer):** `PpcInitializeGraphics` sets a real GOP mode
19: and carves a guest-visible framebuffer window out of guest RAM at guest
20: 0x18000000 (640x480x32). Guest code writes big-endian 0xRRGGBB00 pixels there;
21: `PpcGraphicsBlitToDisplay` converts and copies the window to the real GOP
22: framebuffer (byte-exact RGB and BGR pixel layouts). `PpcGetFrameBufferInfo`
23: reports the window plus the GOP base/pitch/pixel format,
24: `PpcGraphicsClear`/`SetPixel`/`DrawRect` provide host-side helpers. The
25: self-check now includes a multi-frame test: three solid full-screen frames
26: (red/green/blue) verified across every pixel of the GOP framebuffer, a
27: four-band frame checked at centers and boundaries, corner pixels, and
28: out-of-bounds writes. This test caught and led to a fix of a blit bug where
29: channels were shifted 8 bits too high (blue landed in the reserved byte).
30: 
31: **Networking (SNP):** `PpcInitializeNetwork` enumerates every UEFI Simple
32: Network Protocol instance, calls real `Start`/`Initialize` on each, sets the
33: standard receive filter, snapshots the real mode (MAC, media present, iftype,
34: max packet), and transmits a real 64-byte ARP-style frame via `Transmit` with a
35: `GetStatus` poll. Every initialized interface is stored in
36: `PPC_NETWORK_INFO.Interfaces[]` (up to `PPC_MAX_NETWORK_INTERFACES`), so
37: multi-NIC systems are fully brought up and tested. `PpcGetNetworkInfo` reports
38: per-interface results.
39: 
40: **Storage (Block I/O):** `PpcInitializeBlockIo` enumerates every UEFI Block I/O
41: protocol instance and reports real geometry (block size, block count,
42: removable/read-only flags). `PpcReadDiskBlock` performs a real `ReadBlocks`
43: sector read. Self-check reads LBA 0 of every device until it finds the "EFI"
44: marker. `PpcGetBlockIoInfo` reports the state.
45: 
46: **Audio (emulated device):** UEFI has no standard audio output protocol, so the
47: audio "device" is a fixed ring buffer inside guest RAM at guest 0x18800000
48: (8KB, 44100 Hz stereo 16-bit). Guest code fills it with big-endian PCM samples;
49: the host reads them back and advances a play cursor. `PpcAudioGetBufferInfo`,
50: `PpcAudioWriteSample`, `PpcAudioReadSample`, `PpcAudioAdvancePlayback`,
51: `PpcAudioReset` implement the device. Self-check verifies a two-sample write via
52: the guest RAM path reads back correctly and playback advances.
53: 
54: **Verified in QEMU/OVMF (single boot, mixed devices):** Graphics self-check
55: PASS plus the full multi-frame test PASS; Audio self-check PASS; Block I/O
56: self-check PASS (4 devices: FAT ESP + IDE raw disk + virtio-blk raw disk +
57: partition, marker found); Network self-check PASS for **both** an e1000 NIC
58: (MAC ...:56) and a virtio-net-pci NIC (MAC ...:57) simultaneously — both
59: transmit PASS; plus all Phase 3 checks still PASS (18/18 self-test, guest RAM
60: demo, kernel load/verify/execute, boot.log). Build is clean with
61: `make clean && make` (GNU-EFI + clang/lld-link on macOS).
62: 
63: ## Phase 1: Research and Analysis
64: - [x] Analyze existing Mac emulators (SheepShaver, Basilisk II, QEMU, DingusPPC)
65: - [x] Study PowerPC vs 68k architecture differences
66: - [x] Review UEFI specifications and implementation guidelines
67: - [x] Document findings in project README
68: 
69: ## Phase 2: Architecture Design
70: - [x] Define module interfaces (headers) for CPU, memory, hardware, boot, utils, platform
71: - [ ] Complete CPU translation layer requirements (instruction set coverage, register mapping, MMU)
72: - [ ] Complete hardware abstraction layer specification (graphics, audio, storage, network)
73: - [ ] Plan memory management approach (UEFI pool vs. guest memory regions)
74: 
75: ## Phase 3: Core Implementation
76: - [x] Get the UEFI application to build with a real toolchain (GNU-EFI + clang/lld-link)
77: - [x] Implement CPU instruction decoder and interpreter (PowerPC)
78: - [x] Implement register file and special-purpose registers (MSR, SRR0/1, CTR, LR)
79: - [x] Implement memory manager backed by UEFI allocation
80: - [x] Wire up hardware abstraction interface to real UEFI protocols (graphics -> GOP, storage -> Simple File System; audio/network still simulated)
81: - [x] Remove placeholder/simulated behavior from current `_impl.c` files
82: 
83: ## Phase 4: Emulation Components
84: - [x] Implement graphics subsystem (framebuffer via GOP)
85: - [x] Add audio handling
86: - [x] Integrate storage I/O (UEFI file protocols / block I/O)
87: - [x] Implement basic networking
88: 
89: ## Phase 5: Boot Process
90: - [ ] Create bootloader for Mac OS (load ROM image, set up guest memory map)
91: - [ ] Implement system initialization routines
92: - [ ] Add support for system files and drivers
93: - [ ] Test boot process with various Mac OS versions
94: 
95: ## Phase 6: Testing and Optimization
96: - [ ] Test with Mac OS 7, 8, and 9
97: - [ ] Optimize performance
98: - [ ] Fix compatibility issues
99: - [ ] Document usage instructions
100: 
101: ## Architecture Decisions
102: 
103: ### Target Architecture: PowerPC
104: - Selected over 68k due to:
105:   - Better existing support from SheepShaver
106:   - More complete compatibility with Mac OS 8/9
107:   - Simpler translation layer compared to 68k CISC instructions
108: 
109: ### UEFI Approach
110: - Leverages modern boot infrastructure
111: - Direct hardware access capabilities
112: - Better memory management
113: - Support for large storage devices

(End of file - total 113 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\FINAL_SUMMARY.md"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\FINAL_SUMMARY.md</path>
<type>file</type>
<content>
1: # EFI-Mac-Emulator - Project Status Summary
2: 
3: **Note:** Earlier drafts of this file claimed the project was complete. That was
4: inaccurate. This is a corrected, honest status summary as of the latest cleanup.
5: 
6: ## Project Overview
7: 
8: A UEFI-based emulator for running classic Mac OS (System 7, Mac OS 8, and
9: Mac OS 9) on modern Intel x86_64 computers.
10: 
11: ## What Exists Today
12: 
13: ### Research & Analysis (Phase 1) — Complete
14: - Analyzed existing Mac emulators (SheepShaver, Basilisk II, QEMU, DingusPPC)
15: - Studied PowerPC vs 68k architecture differences
16: - Documented UEFI specifications and implementation guidelines
17: 
18: ### Design & Scaffolding (Phase 2, partial)
19: - Module interface headers for all six subsystems:
20:   - `src/cpu/translation.h` — CPU translation layer
21:   - `src/memory/manager.h` — memory manager
22:   - `src/hardware/abstraction.h` — hardware abstraction
23:   - `src/boot/bootloader.h` — bootloader
24:   - `src/utils/debug.h` — debug/logging
25:   - `src/platform/uefi_interface.h` — UEFI interface
26: - Partial placeholder implementations in `src/*/*_impl.c`
27: 
28: ## What Is NOT Implemented
29: 
30: - **CPU translation is not real.** `PpcTranslateInstruction` decodes a small
31:   number of PowerPC opcodes but does not actually translate or execute
32:   anything. There is no interpreter and no dynamic recompilation.
33: - **Memory management is not implemented.** The memory manager initializes a
34:   context and reads the UEFI memory map but performs no guest memory emulation.
35: - **No device emulation.** Graphics/audio/storage/network functions allocate or
36:   print status but do not emulate hardware.
37: - **No working bootloader.** Kernel loading is simulated with hard-coded values;
38:   nothing loads a real Mac OS ROM or kernel image.
39: - **Emulation does not run yet.** The project builds a valid x86_64 UEFI
40:   application (`build/EFI-Mac-Emulator.efi`) via GNU-EFI + clang/lld-link, but
41:   the application only initializes scaffolding and prints status.
42: 
43: ## Source Code Structure
44: 
45: ```
46: src/
47: ├── main.c                 # Main UEFI application entry point
48: ├── cpu/translation.h/.c   # CPU translation layer (placeholder)
49: ├── memory/manager.h/.c    # Memory manager (placeholder)
50: ├── hardware/abstraction.h/.c  # Hardware abstraction (placeholder)
51: ├── boot/bootloader.h/.c   # Bootloader (placeholder)
52: ├── utils/debug.h/.c       # Debug system (placeholder)
53: └── platform/uefi_interface.h/.c  # UEFI interface (placeholder)
54: 
55: CMakeLists.txt             # Build configuration (needs real UEFI headers)
56: README.md                  # Project overview and status
57: TODO.md                    # Implementation plan and status
58: ARCHITECTURE.md            # Design notes
59: BUILD_INSTRUCTIONS.md      # Build instructions
60: USER_GUIDE.md              # User documentation
61: ```
62: 
63: ## Next Steps
64: 
65: 1. Set up a real UEFI toolchain and make the project build (Phase 3 in TODO.md).
66: 2. Implement the PowerPC CPU interpreter — this is the core of the project.
67: 3. Implement guest memory management backed by UEFI allocation.
68: 4. Replace simulated hardware behavior with real UEFI protocol calls.
69: 5. Load and boot a real Mac OS image, then iterate on compatibility.
70: 
71: ## Final Status
72: 
73: **Pre-alpha scaffolding.** The repository provides interfaces and placeholders,
74: not a working emulator. All functional work remains to be done.

(End of file - total 74 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 2.7s)

_Thinking:_

Let me understand the current state better. The project is a UEFI-based Mac OS emulator. According to TODO.md, Phase 4 is complete and verified. Let me look at the source structure and understand what the next objective (Phase 5) would be.

Let me explore the source code structure.

**Tool: bash**

**Input:**
```json
{
  "command": "Get-ChildItem -Recurse -Path src | Select-Object FullName, Length"
}
```

**Output:**
```

FullName                                                                        Length
--------                                                                        ------
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot                                 
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu                                  
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\hardware                             
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory                               
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\platform                             
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\utils                                
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\main.c                         19705 
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader.h              3163  
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c         12559 
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c              49555 
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.h              1615  
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation.h              9069  
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation_impl.c         12161 
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\hardware\abstraction.h         11166 
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\hardware\abstraction_impl.c    37031 
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory\manager.h               4193  
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory\manager_impl.c          11959 
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\platform\uefi_interface.h      5388  
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\platform\uefi_interface_impl.c 12618 
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\utils\debug.h                  2800  
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\utils\debug_impl.c             8347  



```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\ARCHITECTURE.md"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\ARCHITECTURE.md</path>
<type>file</type>
<content>
1: # EFI-Mac-Emulator - CPU Translation Layer Architecture
2: 
3: > **Status note:** This document is a *design reference* for the CPU
4: > translation layer. None of the components described below are implemented
5: > yet — the current code in `src/cpu/` is placeholder scaffolding. This
6: > document describes the intended architecture, not current behavior.
7: 
8: ## Overview
9: 
10: The CPU translation layer is the core component of the EFI-Mac-Emulator that translates instructions between the host x86_64 architecture and the target PowerPC architecture. This layer must handle instruction set differences, register mapping, and execution context switching.
11: 
12: ## Design Requirements
13: 
14: ### Instruction Set Translation
15: - Translate PowerPC instructions to equivalent x86_64 operations
16: - Handle differences in instruction formats and encoding
17: - Support all necessary PowerPC instructions for Mac OS compatibility
18: - Implement efficient translation with minimal performance overhead
19: 
20: ### Register Mapping
21: - Map PowerPC registers to x86_64 register space
22: - Handle 32-bit vs 64-bit register differences
23: - Manage special-purpose registers (SPR, MSR, etc.)
24: - Ensure proper preservation of state during context switches
25: 
26: ### Execution Context
27: - Maintain execution state across translation boundaries
28: - Handle interrupt and exception processing
29: - Support cooperative multitasking requirements
30: - Implement memory management unit (MMU) simulation
31: 
32: ## Architecture Components
33: 
34: ### 1. Translation Engine
35: The main translation component that:
36: - Receives PowerPC instructions from the emulated system
37: - Translates them into equivalent x86_64 operations
38: - Manages translation cache for performance optimization
39: - Handles instruction boundary detection and alignment
40: 
41: ### 2. Register Manager
42: Manages register state mapping between architectures:
43: - 32-bit GPRs (General Purpose Registers) mapping
44: - Special Purpose Registers (SPRs) translation
45: - Status/Control Registers (MSR) handling
46: - Stack pointer management
47: - Floating-point register handling (when needed)
48: 
49: ### 3. Memory Manager Interface
50: Provides memory access abstraction:
51: - Virtual to physical address translation
52: - Memory protection simulation
53: - Cache coherency handling
54: - Alignment requirements for x86_64
55: 
56: ### 4. Exception Handler
57: Manages exception and interrupt processing:
58: - Trap instruction handling
59: - System call interface
60: - Interrupt vector dispatching
61: - Context switching between modes
62: 
63: ### 5. Performance Optimizer
64: Enhances translation efficiency:
65: - Dynamic recompilation cache
66: - Translation block fusion
67: - Branch prediction
68: - Hot code identification
69: 
70: ## PowerPC to x86_64 Mapping Details
71: 
72: ### General Purpose Registers (GPRs)
73: PowerPC has 32 GPRs (r0-r31), while x86_64 has 16 registers.
74: - Map GPRs r0-r15 to x86_64 registers
75: - Use stack or memory for r16-r31 when needed
76: - Implement register spilling when necessary
77: 
78: ### Special Purpose Registers (SPRs)
79: PowerPC has many SPRs that don't have direct x86_64 equivalents:
80: - MSR (Machine State Register) - must be simulated
81: - DAR (Data Address Register) - for data-related exceptions
82: - SRR0/SRR1 (Save/Restore Registers) - for exception handling
83: - Implement these as memory-mapped values or software emulated
84: 
85: ### Instruction Set Differences
86: Key differences to handle:
87: - PowerPC uses fixed-length 32-bit instructions vs x86_64 variable length
88: - PowerPC is RISC vs x86_64 CISC architecture
89: - Different addressing modes and instruction formats
90: - PowerPC has fewer but more complex instructions vs x86_64 simpler instructions
91: 
92: ## UEFI Integration Points
93: 
94: ### Boot Process
95: 1. UEFI application initializes hardware
96: 2. Sets up translation environment
97: 3. Loads PowerPC ROM image
98: 4. Transfers control to PowerPC bootloader
99: 
100: ### Runtime Environment
101: - Memory allocation through UEFI services
102: - Hardware access via UEFI protocols
103: - System information through UEFI tables
104: - Console I/O management
105: 
106: ### Resource Management
107: - Allocate memory for translation cache
108: - Manage CPU resources for translation
109: - Handle UEFI memory map for system resources
110: - Implement proper cleanup on exit
111: 
112: ## Implementation Strategy
113: 
114: ### Phase 1: Basic Translation
115: - Implement core register mapping
116: - Translate basic arithmetic and logic instructions
117: - Handle simple control flow (branches)
118: - Support essential system calls
119: 
120: ### Phase 2: Enhanced Features
121: - Add floating-point instruction support
122: - Implement memory management simulation
123: - Add exception/interrupt handling
124: - Optimize translation performance
125: 
126: ### Phase 3: Full Compatibility
127: - Support complete PowerPC instruction set
128: - Implement MMU simulation
129: - Add advanced features like AltiVec
130: - Ensure compatibility with Mac OS system calls
131: 
132: ## Performance Considerations
133: 
134: ### Translation Overhead
135: - Minimize translation cache misses
136: - Use block-based translation for better performance
137: - Implement hot code detection and optimization
138: - Cache translated blocks for reuse
139: 
140: ### Memory Usage
141: - Balance translation cache size vs memory consumption
142: - Implement efficient cache replacement policies
143: - Manage register state efficiently
144: - Consider memory alignment for x86_64 requirements
145: 
146: ## Testing Approach
147: 
148: ### Unit Testing
149: - Individual instruction translation tests
150: - Register mapping verification
151: - Memory access pattern testing
152: - Exception handling validation
153: 
154: ### Integration Testing
155: - End-to-end boot process testing
156: - System call compatibility verification
157: - Performance benchmarking
158: - Compatibility with Mac OS applications
159: 
160: This architecture provides a foundation for developing the CPU translation layer that will enable running classic Mac OS on modern x86_64 hardware through UEFI.

(End of file - total 160 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 3.5s)

_Thinking:_

The project is a UEFI-based PowerPC Mac OS emulator. Phase 4 is complete. The next objective is Phase 5: Boot Process.

Let me look at the current state of the code to understand what's there, and what the next objective should be. Let me check the current CPU interpreter, bootloader, and main.c to understand the state.

Let me look at the source files.

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\main.c"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\main.c</path>
<type>file</type>
<content>
1: #include <efi.h>
2: #include <efilib.h>
3: 
4: // MS ABI CRT symbol: signals the linker that floating-point is in use.
5: UINT32 _fltused = 0;
6: 
7: // Include all our module headers
8: #include "cpu/translation.h"
9: #include "memory/manager.h"
10: #include "hardware/abstraction.h"
11: #include "boot/bootloader.h"
12: #include "utils/debug.h"
13: #include "platform/uefi_interface.h"
14: 
15: // True if the GOP pixel at (X,Y) holds exactly the R/G/B channels of the
16: // given guest color (big-endian 0xRRGGBB00), placed according to the GOP
17: // pixel format (byte-exact, so channel-position bugs are caught).
18: STATIC
19: BOOLEAN
20: GopPixelMatches (
21:   IN UINT8* GopBase,
22:   IN UINTN  GopPitch,
23:   IN UINTN  X,
24:   IN UINTN  Y,
25:   IN UINT32 GuestColor,
26:   IN UINT32 PixelFormat
27:   )
28: {
29:   UINT8 R = (UINT8)(GuestColor >> 24);
30:   UINT8 G = (UINT8)(GuestColor >> 16);
31:   UINT8 B = (UINT8)(GuestColor >> 8);
32:   UINT8* P = GopBase + Y * GopPitch + X * 4;
33:   if (PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
34:     return P[0] == B && P[1] == G && P[2] == R;
35:   }
36:   return P[0] == R && P[1] == G && P[2] == B;
37: }
38: 
39: // True if every visible pixel of the GOP framebuffer matches the guest color.
40: STATIC
41: BOOLEAN
42: GopFrameIsSolid (
43:   IN UINT8* GopBase,
44:   IN UINTN  GopPitch,
45:   IN UINTN  W,
46:   IN UINTN  H,
47:   IN UINT32 GuestColor,
48:   IN UINT32 PixelFormat
49:   )
50: {
51:   for (UINTN Y = 0; Y < H; Y++) {
52:     for (UINTN X = 0; X < W; X++) {
53:       if (!GopPixelMatches(GopBase, GopPitch, X, Y, GuestColor, PixelFormat)) {
54:         return FALSE;
55:       }
56:     }
57:   }
58:   return TRUE;
59: }
60: 
61: EFI_STATUS
62: efi_main (
63:   IN EFI_HANDLE        ImageHandle,
64:   IN EFI_SYSTEM_TABLE  *SystemTable
65:   )
66: {
67:   EFI_STATUS Status;
68:   
69:   // Initialize the GNU-EFI library
70:   InitializeLib(ImageHandle, SystemTable);
71:   
72:   // Print welcome message
73:   Print(L"EFI-Mac-Emulator v0.1\n");
74:   Print(L"Initializing PowerPC emulation environment...\n");
75:   
76:   // Initialize UEFI interface
77:   Status = PpcInitializeUefiInterface(ImageHandle, SystemTable);
78:   if (EFI_ERROR(Status)) {
79:     Print(L"Failed to initialize UEFI interface: %r\n", Status);
80:     return Status;
81:   }
82:   
83:   // Initialize debug system
84:   Status = PpcInitializeDebug(PPC_DEBUG_LEVEL_DEBUG, FALSE, NULL);
85:   if (EFI_ERROR(Status)) {
86:     Print(L"Failed to initialize debug system: %r\n", Status);
87:     return Status;
88:   }
89:   
90:   // Initialize PowerPC translation context
91:   Status = PpcInitializeTranslationContext();
92:   if (EFI_ERROR(Status)) {
93:     Print(L"Failed to initialize translation context: %r\n", Status);
94:     return Status;
95:   }
96:   
97:   // Run the CPU self-test suite
98:   Status = PpcRunSelfTest();
99:   if (EFI_ERROR(Status)) {
100:     Print(L"PowerPC CPU self-test FAILED: %r\n", Status);
101:     return Status;
102:   }
103:   
104:   // Initialize memory manager (guest RAM at the classic Mac OS kernel base)
105:   Status = PpcInitializeMemoryManager(0x10000000, 0x10000000);  // 256MB
106:   if (EFI_ERROR(Status)) {
107:     Print(L"Failed to initialize memory manager: %r\n", Status);
108:     return Status;
109:   }
110:   
111:   // Wire the UEFI-allocated guest RAM into the interpreter so that guest
112:   // loads/stores and instruction fetches target the emulated memory region.
113:   {
114:     VOID*  HostBase  = NULL;
115:     UINT64 GuestBase = 0;
116:     UINT64 GuestSize = 0;
117:     Status = PpcGetGuestMemoryRegion(&HostBase, &GuestBase, &GuestSize);
118:     if (EFI_ERROR(Status)) {
119:       Print(L"Failed to get guest memory region: %r\n", Status);
120:       return Status;
121:     }
122:     Status = PpcSetGuestMemory(HostBase, (UINT32)GuestBase, (UINT32)GuestSize);
123:     if (EFI_ERROR(Status)) {
124:       Print(L"Failed to wire guest memory: %r\n", Status);
125:       return Status;
126:     }
127:     Print(L"Guest memory wired: host 0x%x <-> guest 0x%x (%d bytes)\n",
128:           HostBase, GuestBase, GuestSize);
129:   }
130:   
131:   // Execute a small PowerPC program that lives in guest RAM. This proves the
132:   // full pipeline: guest memory allocation -> fetch -> decode -> execute.
133:   {
134:     VOID*  DemoHostBase  = NULL;
135:     UINT64 DemoGuestBase = 0;
136:     UINT64 DemoGuestSize = 0;
137:     Status = PpcGetGuestMemoryRegion(&DemoHostBase, &DemoGuestBase, &DemoGuestSize);
138:     if (EFI_ERROR(Status)) {
139:       Print(L"Demo: guest memory region unavailable: %r\n", Status);
140:       return Status;
141:     }
142: 
143:     // addi r3,r0,40 ; addi r4,r0,2 ; mullw r5,r3,r4 ; stw r5,0(r1)
144:     static const UINT32 GuestProgram[] = {
145:       0x38600028,   // addi r3, r0, 40
146:       0x38800002,   // addi r4, r0, 2
147:       0x7CA321D6,   // mullw r5, r3, r4
148:       0x90A10000,   // stw  r5, 0(r1)
149:     };
150:     const UINT32 ProgramBase = (UINT32)DemoGuestBase;
151:     const UINT32 ResultAddr  = ProgramBase + 0x40;  // store result here
152:     const UINTN  ProgramCount = sizeof(GuestProgram) / sizeof(GuestProgram[0]);
153: 
154:     // Write the program into guest RAM (big-endian byte order)
155:     for (UINTN i = 0; i < ProgramCount; i++) {
156:       UINT32 w = GuestProgram[i];
157:       for (UINTN b = 0; b < 4; b++) {
158:         PpcWriteGuestByte(ProgramBase + (UINT32)i * 4 + (UINT32)b,
159:                           (UINT8)(w >> (24 - (UINTN)b * 8)));
160:       }
161:     }
162: 
163:     Print(L"\n--- Executing PowerPC program from guest RAM ---\n");
164: 
165:     PpcSetGprValue(1, ResultAddr);   // r1 points at the result slot
166:     PpcSetGprValue(3, 0);
167:     PpcSetGprValue(4, 0);
168:     PpcSetGprValue(5, 0);
169: 
170:     // Fetch/decode/execute loop over the guest-memory resident program
171:     UINTN  Executed = 0;
172:     EFI_STATUS ExecStatus = PpcExecuteBlock(
173:         (UINT32*)(UINTN)ProgramBase,
174:         ProgramCount,
175:         &Executed
176:     );
177:     BOOLEAN RanClean = (ExecStatus == EFI_SUCCESS);
178: 
179:     // Read the stored word back out of guest RAM (big-endian)
180:     UINT32 Result = 0;
181:     for (UINTN b = 0; b < 4; b++) {
182:       Result = (Result << 8) | PpcReadGuestByte(ResultAddr + (UINT32)b);
183:     }
184:     UINT32 R5 = PpcGetGprValue(5);
185: 
186:     Print(L"Program executed: %d instructions (status %r)\n", Executed, ExecStatus);
187:     Print(L"r5 = %d (expected 80)\n", R5);
188:     Print(L"Guest RAM[0x%x] = %d (expected 80)\n", ResultAddr, Result);
189: 
190:     if (RanClean && Executed == ProgramCount && R5 == 80 && Result == 80) {
191:       Print(L"Guest RAM execution: PASS\n");
192:     } else {
193:       Print(L"Guest RAM execution: FAIL\n");
194:     }
195:   }
196:   
197:   // Initialize hardware abstraction layer
198:   Status = PpcInitializeHardwareAbstraction();
199:   if (EFI_ERROR(Status)) {
200:     Print(L"Failed to initialize hardware abstraction: %r\n", Status);
201:     return Status;
202:   }
203:   
204:   // Initialize bootloader
205:   Status = PpcInitializeBootloader();
206:   if (EFI_ERROR(Status)) {
207:     Print(L"Failed to initialize bootloader: %r\n", Status);
208:     return Status;
209:   }
210:   
211:   // Setup boot environment
212:   Status = PpcSetupBootEnvironment();
213:   if (EFI_ERROR(Status)) {
214:     Print(L"Failed to setup boot environment: %r\n", Status);
215:     return Status;
216:   }
217:   
218:   // Initialize graphics for the emulator
219:   Status = PpcInitializeGraphics(640, 480, 32);
220:   if (EFI_ERROR(Status)) {
221:     Print(L"Failed to initialize graphics: %r\n", Status);
222:     return Status;
223:   }
224:   
225:   // Graphics self-check: draw into the guest framebuffer, blit to the real
226:   // GOP display, and read pixels back from both buffers to verify the path.
227:   {
228:     PPC_FRAMEBUFFER_INFO FbInfo;
229:     Status = PpcGetFrameBufferInfo(&FbInfo);
230:     if (!EFI_ERROR(Status) && FbInfo.HostBuffer != NULL) {
231:       PpcGraphicsClear(0x10101000);                          // dark grey background
232:       PpcGraphicsDrawRect(0, 0, 640, 120, 0xCC000000);       // red band
233:       PpcGraphicsDrawRect(0, 120, 640, 120, 0x00CC0000);     // green band
234:       PpcGraphicsDrawRect(0, 240, 640, 120, 0x0000CC00);     // blue band
235:       PpcGraphicsDrawRect(0, 360, 640, 120, 0xCCCC0000);     // yellow band
236:       Status = PpcGraphicsBlitToDisplay();
237: 
238:       UINT32 P0 = (UINT32)((UINT8*)FbInfo.HostBuffer)[0] << 24 |
239:                   (UINT32)((UINT8*)FbInfo.HostBuffer)[1] << 16 |
240:                   (UINT32)((UINT8*)FbInfo.HostBuffer)[2] << 8;
241:       UINT8* GopP = (UINT8*)(UINTN)FbInfo.GopFrameBuffer;
242:       UINT32 P1 = ((UINT32)GopP[2] << 16) | ((UINT32)GopP[1] << 8) | (UINT32)GopP[0];
243: 
244:       if (!EFI_ERROR(Status) && (P0 >> 24) == 0xCC && (P0 & 0xFF0000) == 0) {
245:         Print(L"Graphics self-check: PASS (guest fb 0x%08x, GOP fb 0x%08x)\n", P0, P1);
246:       } else {
247:         Print(L"Graphics self-check: FAIL (guest fb 0x%08x, GOP fb 0x%08x)\n", P0, P1);
248:       }
249:     } else {
250:       Print(L"Graphics self-check: SKIP (no framebuffer)\n");
251:     }
252:   }
253: 
254:   // Multi-frame graphics test: full-screen frames written into the guest
255:   // framebuffer are blitted through the real GOP path and verified across
256:   // the entire display, plus band boundaries, corners, and out-of-bounds.
257:   {
258:     PPC_FRAMEBUFFER_INFO FbInfo;
259:     Status = PpcGetFrameBufferInfo(&FbInfo);
260:     if (!EFI_ERROR(Status) && FbInfo.HostBuffer != NULL && FbInfo.GopPitch != 0) {
261:       UINTN  W   = FbInfo.Width;
262:       UINTN  H   = FbInfo.Height;
263:       UINT8* Gop = (UINT8*)(UINTN)FbInfo.GopFrameBuffer;
264:       BOOLEAN MultiOk = TRUE;
265: 
266:       // Frames 1-3: solid red, green, blue across the full visible area.
267:       UINT32 Solids[3] = { 0xCC000000, 0x00CC0000, 0x0000CC00 };
268:       for (UINTN f = 0; f < 3; f++) {
269:         PpcGraphicsClear(Solids[f]);
270:         PpcGraphicsBlitToDisplay();
271:         if (!GopFrameIsSolid(Gop, FbInfo.GopPitch, W, H, Solids[f],
272:                              (UINT32)FbInfo.PixelFormat)) {
273:           MultiOk = FALSE;
274:           Print(L"Multi-frame: solid frame %d FAIL\n", f + 1);
275:         }
276:       }
277:       if (MultiOk) {
278:         Print(L"Multi-frame: 3 solid frames full-coverage PASS\n");
279:       }
280: 
281:       // Frame 4: vertical color bands; verify band centers and boundaries.
282:       PpcGraphicsClear(0x10101000);
283:       PpcGraphicsDrawRect(0, 0, W / 4, H, 0xCC000000);      // red
284:       PpcGraphicsDrawRect(W / 4, 0, W / 4, H, 0x00CC0000);  // green
285:       PpcGraphicsDrawRect(W / 2, 0, W / 4, H, 0x0000CC00);  // blue
286:       PpcGraphicsDrawRect(3 * W / 4, 0, W / 4, H, 0xCCCC0000); // yellow
287:       PpcGraphicsBlitToDisplay();
288: 
289:       BOOLEAN BandsOk =
290:         GopPixelMatches(Gop, FbInfo.GopPitch, W / 8, H / 2, 0xCC000000,
291:                         (UINT32)FbInfo.PixelFormat) &&
292:         GopPixelMatches(Gop, FbInfo.GopPitch, 3 * W / 8, H / 2, 0x00CC0000,
293:                         (UINT32)FbInfo.PixelFormat) &&
294:         GopPixelMatches(Gop, FbInfo.GopPitch, 5 * W / 8, H / 2, 0x0000CC00,
295:                         (UINT32)FbInfo.PixelFormat) &&
296:         GopPixelMatches(Gop, FbInfo.GopPitch, 7 * W / 8, H / 2, 0xCCCC0000,
297:                         (UINT32)FbInfo.PixelFormat) &&
298:         GopPixelMatches(Gop, FbInfo.GopPitch, W / 2 - 1, H / 2, 0x00CC0000,
299:                         (UINT32)FbInfo.PixelFormat) &&
300:         GopPixelMatches(Gop, FbInfo.GopPitch, W / 2, H / 2, 0x0000CC00,
301:                         (UINT32)FbInfo.PixelFormat);
302:       Print(L"Multi-frame: band frame %s\n", BandsOk ? L"PASS" : L"FAIL");
303:       MultiOk = MultiOk && BandsOk;
304: 
305:       // Corner and out-of-bounds handling.
306:       BOOLEAN CornersOk =
307:         GopPixelMatches(Gop, FbInfo.GopPitch, 0, 0, 0xCC000000,
308:                         (UINT32)FbInfo.PixelFormat) &&
309:         GopPixelMatches(Gop, FbInfo.GopPitch, W - 1, H - 1, 0xCCCC0000,
310:                         (UINT32)FbInfo.PixelFormat);
311:       PpcGraphicsSetPixel(W + 100, H + 100, 0xFF000000);   // OOB write is dropped
312:       PpcGraphicsBlitToDisplay();
313:       BOOLEAN OobOk =
314:         GopPixelMatches(Gop, FbInfo.GopPitch, W - 1, H - 1, 0xCCCC0000,
315:                         (UINT32)FbInfo.PixelFormat);
316:       Print(L"Multi-frame: corners %s, OOB dropped %s\n",
317:             CornersOk ? L"PASS" : L"FAIL", OobOk ? L"PASS" : L"FAIL");
318:       MultiOk = MultiOk && CornersOk && OobOk;
319: 
320:       Print(L"Multi-frame graphics self-check: %s\n",
321:             MultiOk ? L"PASS" : L"FAIL");
322:     } else {
323:       Print(L"Multi-frame graphics self-check: SKIP (no framebuffer)\n");
324:     }
325:   }
326:   
327:   // Initialize audio subsystem
328:   Status = PpcInitializeAudio();
329:   if (EFI_ERROR(Status)) {
330:     Print(L"Failed to initialize audio: %r\n", Status);
331:     return Status;
332:   }
333: 
334:   // Audio self-check: the guest writes big-endian PCM samples into the
335:   // ring buffer, the host reads them back, then advances playback.
336:   {
337:     PPC_AUDIO_INFO AudioInfo;
338:     Status = PpcAudioGetBufferInfo(&AudioInfo);
339:     if (!EFI_ERROR(Status) && AudioInfo.HostBuffer != NULL) {
340:       // Guest-side write path: 16-bit big-endian 0x1000 = 4096 sample.
341:       UINT32 GuestAddr = AudioInfo.GuestBase;
342:       PpcWriteGuestByte(GuestAddr + 0, 0x10);
343:       PpcWriteGuestByte(GuestAddr + 1, 0x00);
344:       PpcWriteGuestByte(GuestAddr + 2, 0x20);
345:       PpcWriteGuestByte(GuestAddr + 3, 0x00);
346: 
347:       UINT16 S0 = 0, S1 = 0;
348:       BOOLEAN ReadOk = (PpcAudioReadSample(0, &S0) == EFI_SUCCESS) &&
349:                        (PpcAudioReadSample(1, &S1) == EFI_SUCCESS);
350:       Status = PpcAudioAdvancePlayback(2);
351:       PPC_AUDIO_INFO After;
352:       PpcAudioGetBufferInfo(&After);
353: 
354:       if (ReadOk && S0 == 0x1000 && S1 == 0x2000 && !EFI_ERROR(Status) &&
355:           After.PlayCursor == 2) {
356:         Print(L"Audio self-check: PASS (samples 0x%04x/0x%04x, played %d)\n",
357:               S0, S1, (UINT32)After.PlayCursor);
358:       } else {
359:         Print(L"Audio self-check: FAIL (samples 0x%04x/0x%04x, played %d)\n",
360:               S0, S1, (UINT32)(After.PlayCursor));
361:       }
362:     } else {
363:       Print(L"Audio self-check: SKIP (no buffer)\n");
364:     }
365:   }
366:   
367:   // Initialize storage subsystem
368:   Status = PpcInitializeStorage(1);
369:   if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
370:     Print(L"Failed to initialize storage: %r\n", Status);
371:     return Status;
372:   }
373:   
374:   // Block I/O self-check: enumerate real block devices and read a sector.
375:   Status = PpcInitializeBlockIo(1);
376:   if (!EFI_ERROR(Status)) {
377:     PPC_BLOCK_IO_INFO BioInfo;
378:     Status = PpcGetBlockIoInfo(&BioInfo);
379:     if (!EFI_ERROR(Status)) {
380:       BOOLEAN FoundMarker = FALSE;
381:       for (UINTN i = 0; i < BioInfo.DeviceCount; i++) {
382:         UINT8 Sector[512];
383:         if (!EFI_ERROR(PpcReadDiskBlock(i, 0, 512, Sector)) &&
384:             Sector[0] == 'E' && Sector[1] == 'F' && Sector[2] == 'I') {
385:           Print(L"Block I/O self-check: PASS (device %d, LBA 0 = \"%c%c%c...\", 512-byte sector)\n",
386:                 (UINTN)i, Sector[0], Sector[1], Sector[2]);
387:           FoundMarker = TRUE;
388:           break;
389:         }
390:       }
391:       if (!FoundMarker) {
392:         Print(L"Block I/O self-check: devices enumerated, marker not found\n");
393:       }
394:     }
395:   } else {
396:     Print(L"Block I/O self-check: SKIP (%r)\n", Status);
397:   }
398:   
399:   // Initialize network subsystem
400:   Status = PpcInitializeNetwork(1);
401:   if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
402:     Print(L"Failed to initialize network: %r\n", Status);
403:     return Status;
404:   }
405: 
406:   // Network self-check: report every real SNP interface and its transmit test.
407:   {
408:     PPC_NETWORK_INFO NetInfo;
409:     Status = PpcGetNetworkInfo(&NetInfo);
410:     if (!EFI_ERROR(Status)) {
411:       BOOLEAN AllPassed = NetInfo.InterfaceCount > 0;
412:       for (UINTN i = 0; i < NetInfo.InterfaceCount; i++) {
413:         Print(L"Network self-check: interface %d MAC %02x:%02x:%02x:%02x:%02x:%02x, "
414:               L"media %s, transmit %s\n",
415:               i,
416:               NetInfo.Interfaces[i].MacAddress[0],
417:               NetInfo.Interfaces[i].MacAddress[1],
418:               NetInfo.Interfaces[i].MacAddress[2],
419:               NetInfo.Interfaces[i].MacAddress[3],
420:               NetInfo.Interfaces[i].MacAddress[4],
421:               NetInfo.Interfaces[i].MacAddress[5],
422:               NetInfo.Interfaces[i].MediaPresent ? L"present" : L"absent",
423:               NetInfo.Interfaces[i].TransmitTestPassed ? L"PASS" : L"FAIL");
424:         if (!NetInfo.Interfaces[i].MediaPresent ||
425:             !NetInfo.Interfaces[i].TransmitTestPassed) {
426:           AllPassed = FALSE;
427:         }
428:       }
429:       Print(L"Network self-check: %s (%d interface(s))\n",
430:             AllPassed ? L"PASS" : L"FAIL", NetInfo.InterfaceCount);
431:     } else {
432:       Print(L"Network self-check: SKIP (no SNP interface)\n");
433:     }
434:   }
435:   
436:   // Display system information
437:   Print(L"\n=== EFI-Mac-Emulator System Information ===\n");
438:   Print(L"UEFI Version: %d.%d\n", SystemTable->FirmwareRevision >> 16, SystemTable->FirmwareRevision & 0xFFFF);
439:   Print(L"System Table: 0x%x\n", SystemTable);
440:   Print(L"Image Handle: 0x%x\n", ImageHandle);
441:   
442:   // Get memory info
443:   PPC_MEMORY_INFO MemoryInfo;
444:   Status = PpcGetMemoryInfo(&MemoryInfo);
445:   if (!EFI_ERROR(Status)) {
446:     Print(L"Memory Base: 0x%x\n", MemoryInfo.BaseAddress);
447:     Print(L"Memory Size: %d bytes\n", MemoryInfo.Size);
448:   }
449:   
450:   // Get hardware info
451:   PPC_HARDWARE_INFO HardwareInfo;
452:   Status = PpcGetHardwareInfo(&HardwareInfo);
453:   if (!EFI_ERROR(Status)) {
454:     Print(L"Graphics Mode: 0x%x\n", HardwareInfo.GraphicsMode);
455:     Print(L"Audio Enabled: %d\n", HardwareInfo.AudioEnabled);
456:     Print(L"Storage Devices: %d\n", HardwareInfo.StorageDevices);
457:     Print(L"Network Interfaces: %d\n", HardwareInfo.NetworkInterfaces);
458:   }
459:   
460:   // Write a boot log entry to the ESP — real UEFI file I/O to the FAT drive.
461:   PpcDebugLogToFile(L"EFI-Mac-Emulator: boot self-test passed, initializing hardware\n");
462: 
463:   // Display emulator status
464:   Print(L"\nEFI-Mac-Emulator initialized successfully.\n");
465:   Print(L"Ready to load and boot classic Mac OS.\n");
466:   
467:   // Real kernel load from the FAT volume into guest RAM, then verify and
468:   // execute it — a full disk → RAM → execute pipeline.
469:   EFI_PHYSICAL_ADDRESS KernelAddress;
470:   UINT64 KernelSize;
471:   
472:   Print(L"\n--- Loading kernel from volume ---\n");
473:   Status = PpcLoadKernel(L"\\System\\MacOS\\kernel", &KernelAddress, &KernelSize);
474:   if (!EFI_ERROR(Status)) {
475:     Print(L"Kernel loaded at 0x%x (%d bytes)\n", KernelAddress, KernelSize);
476: 
477:     // Verify kernel integrity (bounds check + first word read).
478:     PpcVerifyKernel(KernelAddress, KernelSize);
479: 
480:     // Set boot parameters (real storage into the bootloader context).
481:     PPC_BOOT_PARAMETERS Params;
482:     Params.BootMode = PPC_BOOT_MODE_NORMAL;
483:     Params.MemorySizeMB = 256;
484:     Params.VideoMode = PPC_GRAPHICS_MODE_1024x768;
485:     Params.EnableDebug = TRUE;
486:     Params.CommandLine = L"console=serial";
487:     PpcSetBootParameters(&Params);
488:     Print(L"Boot parameters set successfully\n");
489: 
490:     // Execute the first 4 words of the loaded kernel image.
491:     Print(L"\n--- Executing loaded kernel image ---\n");
492:     UINT32 EntryPoint = (UINT32)KernelAddress;
493:     const UINTN KernelExecCount = 4;
494:     UINTN KernelExecuted = 0;
495:     Status = PpcExecuteBlock((UINT32*)(UINTN)EntryPoint, KernelExecCount, &KernelExecuted);
496:     BOOLEAN KernelRanClean = (Status == EFI_SUCCESS);
497: 
498:     UINT32 K_R3 = PpcGetGprValue(3);
499:     UINT32 K_R5 = PpcGetGprValue(5);
500: 
501:     Print(L"Loaded kernel executed: %d instructions (status %r)\n", KernelExecuted, Status);
502:     Print(L"r3=%d, r5=%d\n", K_R3, K_R5);
503: 
504:     if (KernelRanClean && KernelExecuted == KernelExecCount && K_R5 == 700) {
505:       Print(L"Loaded kernel execution: PASS\n");
506:     } else if (KernelRanClean && KernelExecuted == KernelExecCount) {
507:       Print(L"Loaded kernel execution: PARTIAL (ran but unexpected register state)\n");
508:     } else {
509:       Print(L"Loaded kernel execution: FAIL\n");
510:     }
511: 
512:     // Configure CPU context for boot.
513:     PpcBootSystem(KernelAddress, KernelSize);
514:   } else {
515:     Print(L"Kernel not found on volume (%r) — skipping kernel execution\n", Status);
516:   }
517:   
518:   Print(L"\n=== EFI-Mac-Emulator Ready ===\n");
519:   
520:   return EFI_SUCCESS;
521: }

(End of file - total 521 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader.h"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader.h</path>
<type>file</type>
<content>
1: #ifndef __PPC_BOOTLOADER_H__
2: #define __PPC_BOOTLOADER_H__
3: 
4: #include <efi.h>
5: 
6: // Boot modes
7: #define PPC_BOOT_MODE_NORMAL    0
8: #define PPC_BOOT_MODE_RECOVERY  1
9: #define PPC_BOOT_MODE_DIAGNOSTIC 2
10: 
11: // Boot parameters structure
12: typedef struct {
13:     UINT32 BootMode;
14:     UINT32 MemorySizeMB;
15:     UINT32 VideoMode;
16:     BOOLEAN EnableDebug;
17:     CHAR16* CommandLine;
18: } PPC_BOOT_PARAMETERS;
19: 
20: // Boot information structure
21: typedef struct {
22:     BOOLEAN IsInitialized;
23:     EFI_PHYSICAL_ADDRESS KernelAddress;
24:     UINT64 KernelSize;
25:     BOOLEAN KernelLoaded;
26: } PPC_BOOT_INFO;
27: 
28: /**
29:   Initialize PowerPC bootloader
30:   @retval EFI_STATUS
31: **/
32: EFI_STATUS
33: EFIAPI
34: PpcInitializeBootloader (
35:     VOID
36:     );
37: 
38: /**
39:   Load kernel image into memory
40:   @param[in]  ImagePath      Path to the kernel image
41:   @param[out] KernelAddress  Pointer to store kernel load address
42:   @param[out] KernelSize     Pointer to store kernel size
43:   @retval EFI_STATUS
44: **/
45: EFI_STATUS
46: EFIAPI
47: PpcLoadKernel (
48:     IN  CHAR16* ImagePath,
49:     OUT EFI_PHYSICAL_ADDRESS* KernelAddress,
50:     OUT UINT64* KernelSize
51:     );
52: 
53: /**
54:   Boot the PowerPC system
55:   @param[in] KernelAddress Address of kernel to boot
56:   @param[in] KernelSize    Size of kernel to boot
57:   @retval EFI_STATUS
58: **/
59: EFI_STATUS
60: EFIAPI
61: PpcBootSystem (
62:     IN EFI_PHYSICAL_ADDRESS KernelAddress,
63:     IN UINT64               KernelSize
64:     );
65: 
66: /**
67:   Load a boot image into memory
68:   @param[in]  ImagePath   Path to the boot image
69:   @param[out] ImageBuffer Pointer to store image buffer address
70:   @param[out] ImageSize   Pointer to store image size
71:   @retval EFI_STATUS
72: **/
73: EFI_STATUS
74: EFIAPI
75: PpcLoadBootImage (
76:     IN  CHAR16* ImagePath,
77:     OUT VOID**  ImageBuffer,
78:     OUT UINT64* ImageSize
79:     );
80: 
81: /**
82:   Set boot parameters for system boot
83:   @param[in] Parameters Boot parameters to set
84:   @retval EFI_STATUS
85: **/
86: EFI_STATUS
87: EFIAPI
88: PpcSetBootParameters (
89:     IN PPC_BOOT_PARAMETERS* Parameters
90:     );
91: 
92: /**
93:   Get current boot information
94:   @param[out] BootInfo Pointer to structure to fill with boot info
95:   @retval EFI_STATUS
96: **/
97: EFI_STATUS
98: EFIAPI
99: PpcGetBootInfo (
100:     OUT PPC_BOOT_INFO* BootInfo
101:     );
102: 
103: /**
104:   Set up the boot environment for the PowerPC system
105:   @retval EFI_STATUS
106: **/
107: EFI_STATUS
108: EFIAPI
109: PpcSetupBootEnvironment (
110:     VOID
111:     );
112: 
113: /**
114:   Verify a loaded kernel image: bounds check against guest RAM and
115:   read the first word to confirm the data was read correctly.
116:   @param[in] KernelAddress Address of the loaded kernel
117:   @param[in] KernelSize    Size of the loaded kernel
118:   @retval EFI_STATUS
119: **/
120: EFI_STATUS
121: EFIAPI
122: PpcVerifyKernel (
123:     IN  EFI_PHYSICAL_ADDRESS KernelAddress,
124:     IN  UINT64               KernelSize
125:     );
126: 
127: /**
128:   Load a system ROM image into memory
129:   @param[in]  RomPath    Path to the ROM image
130:   @param[out] RomBuffer  Pointer to store ROM buffer address
131:   @param[out] RomSize    Pointer to store ROM size
132:   @retval EFI_STATUS
133: **/
134: EFI_STATUS
135: EFIAPI
136: PpcLoadSystemRom (
137:     IN  CHAR16* RomPath,
138:     OUT VOID**  RomBuffer,
139:     OUT UINT64* RomSize
140:     );
141: 
142: #endif // __PPC_BOOTLOADER_H__

(End of file - total 142 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 3.5s)

_Thinking:_

Now I understand the project structure. The project is at Phase 4 completion. The next phase (Phase 5) is the Boot Process:
- [ ] Create bootloader for Mac OS (load ROM image, set up guest memory map)
- [ ] Implement system initialization routines
- [ ] Add support for system files and drivers
- [ ] Test boot process with various Mac OS versions

Let me examine the current bootloader implementation, the interpreter, and other key files to understand what's been done so far and what the next objective should be.

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c</path>
<type>file</type>
<content>
1: #include "bootloader.h"
2: #include <efi.h>
3: #include <efilib.h>
4: #include "cpu/interpreter.h"
5: #include "cpu/translation.h"
6: #include "memory/manager.h"
7: #include "hardware/abstraction.h"
8: #include "platform/uefi_interface.h"
9: 
10: // Bootloader context structure with more complete implementation
11: typedef struct {
12:     BOOLEAN IsInitialized;
13:     CHAR16* BootImagePath;
14:     EFI_PHYSICAL_ADDRESS KernelAddress;
15:     UINT64 KernelSize;
16:     BOOLEAN KernelLoaded;
17:     BOOLEAN SystemBooting;
18:     PPC_BOOT_PARAMETERS BootParams;
19:     EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
20: } PPC_BOOTLOADER_CONTEXT;
21: 
22: // Global bootloader context
23: STATIC PPC_BOOTLOADER_CONTEXT g_BootContext = {0};
24: 
25: EFI_STATUS
26: PpcInitializeBootloader (
27:     VOID
28:     )
29: {
30:     // Initialize the bootloader context
31:     ZeroMem(&g_BootContext, sizeof(g_BootContext));
32:     
33:     g_BootContext.IsInitialized = TRUE;
34:     g_BootContext.BootImagePath = NULL;
35:     g_BootContext.KernelAddress = 0;
36:     g_BootContext.KernelSize = 0;
37:     g_BootContext.KernelLoaded = FALSE;
38:     g_BootContext.SystemBooting = FALSE;
39:     
40:     // Initialize boot parameters
41:     ZeroMem(&g_BootContext.BootParams, sizeof(PPC_BOOT_PARAMETERS));
42:     g_BootContext.BootParams.BootMode = PPC_BOOT_MODE_NORMAL;
43:     g_BootContext.BootParams.MemorySizeMB = 128;  // Default 128MB
44:     g_BootContext.BootParams.VideoMode = PPC_GRAPHICS_MODE_640x480;
45:     g_BootContext.BootParams.EnableDebug = FALSE;
46:     
47:     Print(L"PowerPC Bootloader initialized\n");
48:     
49:     return EFI_SUCCESS;
50: }
51: 
52: EFI_STATUS
53: PpcLoadKernel (
54:     IN  CHAR16* ImagePath,
55:     OUT EFI_PHYSICAL_ADDRESS* KernelAddress,
56:     OUT UINT64* KernelSize
57:     )
58: {
59:     if (ImagePath == NULL || KernelAddress == NULL || KernelSize == NULL) {
60:         return EFI_INVALID_PARAMETER;
61:     }
62: 
63:     Print(L"Loading kernel from: %s\n", ImagePath);
64: 
65:     // Real UEFI file I/O: resolve the file system of the boot device.
66:     EFI_FILE_IO_INTERFACE* Fs = NULL;
67:     EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
68:     if (EFI_ERROR(Status)) {
69:         Print(L"Failed to get boot file system: %r\n", Status);
70:         return Status;
71:     }
72: 
73:     EFI_FILE_HANDLE Root = NULL;
74:     Status = Fs->OpenVolume(Fs, &Root);
75:     if (EFI_ERROR(Status)) {
76:         Print(L"Failed to open boot volume: %r\n", Status);
77:         return Status;
78:     }
79: 
80:     EFI_FILE_HANDLE KernelFile = NULL;
81:     Status = Root->Open(Root, &KernelFile, ImagePath, EFI_FILE_MODE_READ, 0);
82:     if (EFI_ERROR(Status) || KernelFile == NULL) {
83:         Print(L"Kernel image '%s' not found: %r\n", ImagePath, Status);
84:         Root->Close(Root);
85:         return (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
86:     }
87: 
88:     // Get the kernel file size.
89:     UINTN FileInfoSize = SIZE_OF_EFI_FILE_INFO + 260 * sizeof(CHAR16);
90:     EFI_FILE_INFO* FileInfo = AllocateZeroPool(FileInfoSize);
91:     if (FileInfo == NULL) {
92:         KernelFile->Close(KernelFile);
93:         Root->Close(Root);
94:         return EFI_OUT_OF_RESOURCES;
95:     }
96:     Status = KernelFile->GetInfo(KernelFile, &GenericFileInfo, &FileInfoSize, FileInfo);
97:     if (EFI_ERROR(Status)) {
98:         Print(L"Failed to get kernel file info: %r\n", Status);
99:         FreePool(FileInfo);
100:         KernelFile->Close(KernelFile);
101:         Root->Close(Root);
102:         return Status;
103:     }
104: 
105:     UINT64 FileSize = FileInfo->FileSize;
106:     FreePool(FileInfo);
107: 
108:     if (FileSize == 0 || FileSize > 0x10000000) {
109:         Print(L"Kernel image has invalid size %d\n", FileSize);
110:         KernelFile->Close(KernelFile);
111:         Root->Close(Root);
112:         return EFI_LOAD_ERROR;
113:     }
114: 
115:     // Destination: the UEFI-allocated guest RAM region (guest base 0x10000000).
116:     VOID*  GuestBuffer = NULL;
117:     UINT64 GuestBase   = 0;
118:     UINT64 GuestSize   = 0;
119:     Status = PpcGetGuestMemoryRegion(&GuestBuffer, &GuestBase, &GuestSize);
120:     if (EFI_ERROR(Status) || GuestBuffer == NULL || FileSize > GuestSize) {
121:         Print(L"Guest RAM unavailable for kernel load\n");
122:         KernelFile->Close(KernelFile);
123:         Root->Close(Root);
124:         return EFI_NOT_READY;
125:     }
126: 
127:     // Read the whole file into guest RAM (Read may return partial data).
128:     UINTN   BytesRead = 0;
129:     UINT64  Remaining = FileSize;
130:     BOOLEAN Failed    = FALSE;
131:     while (Remaining > 0) {
132:         UINTN Chunk = (UINTN)Remaining;
133:         Status = KernelFile->Read(KernelFile, &Chunk, (UINT8*)GuestBuffer + BytesRead);
134:         if (EFI_ERROR(Status) || Chunk == 0) {
135:             Print(L"Failed while reading kernel: %r\n", Status);
136:             Failed = TRUE;
137:             break;
138:         }
139:         BytesRead += Chunk;
140:         Remaining  -= Chunk;
141:     }
142: 
143:     KernelFile->Close(KernelFile);
144:     Root->Close(Root);
145: 
146:     if (Failed) {
147:         return EFI_LOAD_ERROR;
148:     }
149: 
150:     *KernelAddress = (EFI_PHYSICAL_ADDRESS)GuestBase;
151:     *KernelSize = FileSize;
152: 
153:     g_BootContext.KernelAddress = *KernelAddress;
154:     g_BootContext.KernelSize = *KernelSize;
155:     g_BootContext.KernelLoaded = TRUE;
156: 
157:     Print(L"Kernel loaded: %d bytes into guest RAM at 0x%x\n",
158:           FileSize, (UINT32)GuestBase);
159: 
160:     return EFI_SUCCESS;
161: }
162: 
163: EFI_STATUS
164: PpcBootSystem (
165:     IN EFI_PHYSICAL_ADDRESS KernelAddress,
166:     IN UINT64               KernelSize
167:     )
168: {
169:     if (!g_BootContext.KernelLoaded) {
170:         Print(L"Error: No kernel loaded for boot\n");
171:         return EFI_NOT_READY;
172:     }
173:     if (KernelAddress == 0 || KernelSize == 0) {
174:         return EFI_INVALID_PARAMETER;
175:     }
176: 
177:     // Configure the real CPU context for transfer of control to the kernel:
178:     // PC = kernel entry, MSR enables machine-check handling, SRR0/SRR1 seeded.
179:     g_PpcContext.Pc = (UINT32)KernelAddress;
180:     g_PpcContext.Srr0 = (UINT32)KernelAddress;
181:     g_PpcContext.Srr1 = g_PpcContext.Msr;
182:     g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;
183:     g_PpcContext.ExceptionPending = 0;
184: 
185:     Print(L"Booting system from kernel at 0x%x (size: %d bytes)\n", KernelAddress, KernelSize);
186:     Print(L"PowerPC core configured: PC=0x%x MSR=0x%08x\n",
187:           g_PpcContext.Pc, g_PpcContext.Msr);
188: 
189:     g_BootContext.SystemBooting = TRUE;
190: 
191:     return EFI_SUCCESS;
192: }
193: 
194: EFI_STATUS
195: PpcLoadBootImage (
196:     IN  CHAR16* ImagePath,
197:     OUT VOID**  ImageBuffer,
198:     OUT UINT64* ImageSize
199:     )
200: {
201:     if (ImagePath == NULL || ImageBuffer == NULL || ImageSize == NULL) {
202:         return EFI_INVALID_PARAMETER;
203:     }
204: 
205:     // Real UEFI file I/O into a pool buffer.
206:     EFI_FILE_IO_INTERFACE* Fs = NULL;
207:     EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
208:     if (EFI_ERROR(Status)) {
209:         return Status;
210:     }
211: 
212:     UINTN  Size = 0;
213:     VOID*  Buffer = NULL;
214:     Status = PpcLoadFile(Fs, ImagePath, &Buffer, &Size);
215:     if (EFI_ERROR(Status)) {
216:         return Status;
217:     }
218: 
219:     *ImageBuffer = Buffer;
220:     *ImageSize = Size;
221: 
222:     Print(L"Boot image loaded: %d bytes at 0x%x\n", Size, Buffer);
223: 
224:     return EFI_SUCCESS;
225: }
226: 
227: EFI_STATUS
228: PpcSetBootParameters (
229:     IN PPC_BOOT_PARAMETERS* Parameters
230:     )
231: {
232:     if (Parameters == NULL) {
233:         return EFI_INVALID_PARAMETER;
234:     }
235:     
236:     // Validate boot parameters
237:     if (Parameters->MemorySizeMB == 0 || Parameters->MemorySizeMB > 4096) {
238:         Print(L"Invalid memory size: %d MB\n", Parameters->MemorySizeMB);
239:         return EFI_INVALID_PARAMETER;
240:     }
241:     
242:     // In a real implementation:
243:     // 1. Validate boot parameters
244:     // 2. Store parameters for system boot
245:     // 3. Set up boot environment
246:     
247:     Print(L"Setting boot parameters\n");
248:     Print(L"Boot mode: %d\n", Parameters->BootMode);
249:     Print(L"Memory size: %d MB\n", Parameters->MemorySizeMB);
250:     Print(L"Video mode: %d\n", Parameters->VideoMode);
251:     Print(L"Debug enabled: %s\n", Parameters->EnableDebug ? L"YES" : L"NO");
252:     
253:     // Copy the parameters
254:     g_BootContext.BootParams = *Parameters;
255:     
256:     return EFI_SUCCESS;
257: }
258: 
259: EFI_STATUS
260: PpcGetBootInfo (
261:     OUT PPC_BOOT_INFO* BootInfo
262:     )
263: {
264:     if (BootInfo == NULL) {
265:         return EFI_INVALID_PARAMETER;
266:     }
267:     
268:     // Fill boot information structure
269:     ZeroMem(BootInfo, sizeof(PPC_BOOT_INFO));
270:     
271:     BootInfo->IsInitialized = g_BootContext.IsInitialized;
272:     BootInfo->KernelAddress = g_BootContext.KernelAddress;
273:     BootInfo->KernelSize = g_BootContext.KernelSize;
274:     BootInfo->KernelLoaded = g_BootContext.KernelLoaded;
275:     
276:     return EFI_SUCCESS;
277: }
278: 
279: // Additional bootloader functions for PowerPC-specific boot requirements
280: 
281: EFI_STATUS
282: PpcLoadSystemRom (
283:     IN  CHAR16* RomPath,
284:     OUT VOID**  RomBuffer,
285:     OUT UINT64* RomSize
286:     )
287: {
288:     if (RomPath == NULL || RomBuffer == NULL || RomSize == NULL) {
289:         return EFI_INVALID_PARAMETER;
290:     }
291: 
292:     Print(L"Loading system ROM from: %s\n", RomPath);
293: 
294:     // Real UEFI file I/O into a pool buffer.
295:     EFI_FILE_IO_INTERFACE* Fs = NULL;
296:     EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
297:     if (EFI_ERROR(Status)) {
298:         return Status;
299:     }
300: 
301:     UINTN  Size = 0;
302:     VOID*  Buffer = NULL;
303:     Status = PpcLoadFile(Fs, RomPath, &Buffer, &Size);
304:     if (EFI_ERROR(Status)) {
305:         return Status;
306:     }
307: 
308:     *RomBuffer = Buffer;
309:     *RomSize = Size;
310: 
311:     Print(L"System ROM loaded: %d bytes at 0x%x\n", Size, Buffer);
312: 
313:     return EFI_SUCCESS;
314: }
315: 
316: EFI_STATUS
317: PpcVerifyKernel (
318:     IN  EFI_PHYSICAL_ADDRESS KernelAddress,
319:     IN  UINT64               KernelSize
320:     )
321: {
322:     Print(L"Verifying kernel at 0x%x (size: %d bytes)\n", KernelAddress, KernelSize);
323: 
324:     // Real bounds check: the kernel must lie within the guest RAM region.
325:     VOID*  GuestBuffer = NULL;
326:     UINT64 GuestBase   = 0;
327:     UINT64 GuestSize   = 0;
328:     EFI_STATUS Status = PpcGetGuestMemoryRegion(&GuestBuffer, &GuestBase, &GuestSize);
329:     if (EFI_ERROR(Status)) {
330:         Print(L"Verification failed: guest RAM unavailable\n");
331:         return EFI_NOT_READY;
332:     }
333:     if ((UINT64)KernelAddress < GuestBase ||
334:         (UINT64)KernelAddress - GuestBase + KernelSize > GuestSize) {
335:         Print(L"Verification failed: kernel outside guest RAM bounds\n");
336:         return EFI_LOAD_ERROR;
337:     }
338:     if (KernelSize == 0) {
339:         Print(L"Verification failed: kernel size is zero\n");
340:         return EFI_LOAD_ERROR;
341:     }
342: 
343:     // Read the first word (big-endian) and report it as a sanity value.
344:     UINT32 FirstWord = PpcReadGuestByte((UINT32)KernelAddress)     << 24 |
345:                        PpcReadGuestByte((UINT32)KernelAddress + 1) << 16 |
346:                        PpcReadGuestByte((UINT32)KernelAddress + 2) << 8  |
347:                        PpcReadGuestByte((UINT32)KernelAddress + 3);
348:     Print(L"Kernel verification: bounds OK, first word 0x%08x\n", FirstWord);
349: 
350:     return EFI_SUCCESS;
351: }
352: 
353: EFI_STATUS
354: PpcSetupBootEnvironment (
355:     VOID
356:     )
357: {
358:     Print(L"Setting up boot environment\n");
359:     
360:     // In a real implementation:
361:     // 1. Initialize boot environment variables
362:     // 2. Set up memory for boot process
363:     // 3. Configure system parameters
364:     // 4. Prepare for kernel execution
365:     
366:     // Initialize the PowerPC translation context
367:     EFI_STATUS Status = PpcInitializeTranslationContext();
368:     if (EFI_ERROR(Status)) {
369:         Print(L"Failed to initialize translation context: %r\n", Status);
370:         return Status;
371:     }
372:     
373:     // Initialize memory manager
374:     Status = PpcInitializeMemoryManager(0x00000000, 0x10000000);  // 256MB
375:     if (EFI_ERROR(Status)) {
376:         Print(L"Failed to initialize memory manager: %r\n", Status);
377:         return Status;
378:     }
379:     
380:     // Initialize hardware abstraction layer
381:     Status = PpcInitializeHardwareAbstraction();
382:     if (EFI_ERROR(Status)) {
383:         Print(L"Failed to initialize hardware abstraction: %r\n", Status);
384:         return Status;
385:     }
386:     
387:     // Initialize graphics for boot process
388:     Status = PpcInitializeGraphics(640, 480, 32);
389:     if (EFI_ERROR(Status)) {
390:         Print(L"Failed to initialize graphics: %r\n", Status);
391:         return Status;
392:     }
393:     
394:     Print(L"Boot environment setup complete\n");
395:     
396:     return EFI_SUCCESS;
397: }
398: 
399: EFI_STATUS
400: PpcPrepareSystemForBoot (
401:     VOID
402:     )
403: {
404:     Print(L"Preparing system for boot\n");
405:     
406:     // In a real implementation:
407:     // 1. Finalize system state
408:     // 2. Save any required state information
409:     // 3. Set up registers and memory for kernel startup
410:     // 4. Prepare interrupt vectors
411:     
412:     Print(L"System preparation complete\n");
413:     
414:     return EFI_SUCCESS;
415: }

(End of file - total 415 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.h"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.h</path>
<type>file</type>
<content>
1: #ifndef __PPC_INTERPRETER_H__
2: #define __PPC_INTERPRETER_H__
3: 
4: #include <efi.h>
5: 
6: // PowerPC CPU execution context.
7: //
8: // This is the register file and state that the interpreter operates on. It is
9: // intentionally a separate module from the public translation API so that the
10: // decoder/executor can be built and unit-tested on its own.
11: typedef struct {
12:     UINT32  Gpr[32];        // General Purpose Registers R0..R31
13:     UINT32  Cr;             // Condition Register (8 fields of 4 bits)
14:     UINT32  Xer;            // Fixed-point exception register (SO/OV/CA)
15:     UINT32  Msr;            // Machine State Register
16:     UINT32  Srr0;           // Save/Restore Register 0
17:     UINT32  Srr1;           // Save/Restore Register 1
18:     UINT32  Ctr;            // Count Register
19:     UINT32  Lr;             // Link Register
20:     UINT32  Pc;             // Program Counter (guest address)
21:     UINT32  Spr[1024];      // Special Purpose Register file
22:     UINT32  ExceptionPending;  // 0 = none, else PPC_EXCEPTION_*
23: } PPC_CPU_CONTEXT;
24: 
25: // Global CPU context
26: extern PPC_CPU_CONTEXT g_PpcContext;
27: 
28: // Update a single 4-bit Condition Register field
29: VOID
30: PpcSetCrField (
31:     IN UINT32 Field,
32:     IN UINT32 Value
33:     );
34: 
35: // Read a single 4-bit Condition Register field
36: UINT32
37: PpcGetCrField (
38:     IN UINT32 Field
39:     );
40: 
41: // Set XER[CA] (carry) to the given value, preserving other XER bits
42: VOID
43: PpcSetXerCarry (
44:     IN UINT32 Carry
45:     );
46: 
47: // Set XER[OV] and, if set, the sticky XER[SO] bit
48: VOID
49: PpcSetXerOverflow (
50:     IN UINT32 Overflow
51:     );
52: 
53: #endif // __PPC_INTERPRETER_H__

(End of file - total 53 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 2.8s)

_Thinking:_

Let me now look at the interpreter implementation, memory manager, translation header, and hardware abstraction to get the full picture. Let me read the remaining files.

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\translation.h"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation.h</path>
<type>file</type>
<content>
1: #ifndef __PPC_TRANSLATION_H__
2: #define __PPC_TRANSLATION_H__
3: 
4: #include <efi.h>
5: 
6: // PowerPC register definitions
7: #define PPC_GPR0    0
8: #define PPC_GPR1    1
9: #define PPC_GPR2    2
10: #define PPC_GPR3    3
11: #define PPC_GPR4    4
12: #define PPC_GPR5    5
13: #define PPC_GPR6    6
14: #define PPC_GPR7    7
15: #define PPC_GPR8    8
16: #define PPC_GPR9    9
17: #define PPC_GPR10   10
18: #define PPC_GPR11   11
19: #define PPC_GPR12   12
20: #define PPC_GPR13   13
21: #define PPC_GPR14   14
22: #define PPC_GPR15   15
23: #define PPC_GPR16   16
24: #define PPC_GPR17   17
25: #define PPC_GPR18   18
26: #define PPC_GPR19   19
27: #define PPC_GPR20   20
28: #define PPC_GPR21   21
29: #define PPC_GPR22   22
30: #define PPC_GPR23   23
31: #define PPC_GPR24   24
32: #define PPC_GPR25   25
33: #define PPC_GPR26   26
34: #define PPC_GPR27   27
35: #define PPC_GPR28   28
36: #define PPC_GPR29   29
37: #define PPC_GPR30   30
38: #define PPC_GPR31   31
39: 
40: // Special Purpose Registers
41: #define PPC_MSR_REG     32
42: #define PPC_SRR0_REG    33
43: #define PPC_SRR1_REG    34
44: 
45: // PowerPC special register numbers (used by PpcGetRegisterValue/PpcSetRegisterValue)
46: #define PPC_REG_MSR     32
47: #define PPC_REG_SRR0    33
48: #define PPC_REG_SRR1    34
49: #define PPC_REG_CTR     35
50: #define PPC_REG_LR      36
51: #define PPC_REG_CR      37
52: #define PPC_REG_XER     38
53: #define PPC_REG_PC      39
54: 
55: // PowerPC exception types
56: #define PPC_EXCEPTION_INTERRUPT     1
57: #define PPC_EXCEPTION_TRAP          2
58: #define PPC_EXCEPTION_SYSTEM_CALL   3
59: 
60: // PowerPC exception vector offsets (real 32-bit PPC)
61: #define PPC_EXCEPTION_VECTOR_INTERRUPT   0x500
62: #define PPC_EXCEPTION_VECTOR_TRAP        0x700
63: #define PPC_EXCEPTION_VECTOR_SYSTEM_CALL 0xC00
64: 
65: // MSR bit definitions (bit 0 = most significant)
66: #define PPC_MSR_POW    0x00040000  // Power management
67: #define PPC_MSR_ME     0x00001000  // Machine check enable
68: #define PPC_MSR_RI     0x00000002  // Recoverable interrupt
69: #define PPC_MSR_EE     0x00008000  // External interrupt enable
70: #define PPC_MSR_IR     0x00000020  // Instruction address translation
71: #define PPC_MSR_DR     0x00000010  // Data address translation
72: 
73: // XER bit definitions (bit 0 = most significant)
74: #define PPC_XER_SO      0x80000000  // Summary Overflow
75: #define PPC_XER_OV      0x40000000  // Overflow
76: #define PPC_XER_CA      0x20000000  // Carry
77: 
78: // Condition Register field bit definitions
79: #define PPC_CR_LT       0x8         // Negative (less than)
80: #define PPC_CR_GT       0x4         // Positive (greater than)
81: #define PPC_CR_EQ       0x2         // Zero (equal)
82: #define PPC_CR_SO       0x1         // Summary overflow copy
83: 
84: // PowerPC instruction formats
85: #define PPC_FORMAT_INVALID  0
86: #define PPC_FORMAT_1        1
87: #define PPC_FORMAT_2        2
88: #define PPC_FORMAT_3        3
89: #define PPC_FORMAT_4        4
90: #define PPC_FORMAT_5        5
91: #define PPC_FORMAT_6        6
92: 
93: /**
94:   Initialize PowerPC translation context
95:   @retval EFI_STATUS
96: **/
97: EFI_STATUS
98: EFIAPI
99: PpcInitializeTranslationContext (
100:     VOID
101:     );
102: 
103: /**
104:   Translate a single PowerPC instruction to x86_64
105:   @param[in]  PpcInstruction   The PowerPC instruction to translate
106:   @param[out] X86Instruction   Pointer to store the translated instruction
107:   @retval EFI_STATUS
108: **/
109: EFI_STATUS
110: EFIAPI
111: PpcTranslateInstruction (
112:     IN  UINT32  PpcInstruction,
113:     OUT UINT64* X86Instruction
114:     );
115: 
116: /**
117:   Decode a single PowerPC instruction into a short mnemonic string
118:   @param[in]  Instruction  The PowerPC instruction to decode
119:   @param[out] Buffer       Buffer to receive the mnemonic (NUL terminated)
120:   @param[in]  BufferSize   Size of Buffer in bytes (must be >= 16)
121:   @retval EFI_STATUS
122: **/
123: EFI_STATUS
124: EFIAPI
125: PpcDecodeInstruction (
126:     IN  UINT32 Instruction,
127:     OUT CHAR16* Buffer,
128:     IN  UINTN   BufferSize
129:     );
130: 
131: /**
132:   Fetch a PowerPC instruction from guest memory
133:   @param[in]  Address  Guest address to fetch from
134:   @retval The fetched instruction word
135: **/
136: UINT32
137: PpcFetchInstruction (
138:     IN UINT32 Address
139:     );
140: 
141: /**
142:   Execute a single PowerPC instruction against the CPU context
143:   @param[in]  Instruction     The instruction word to execute
144:   @param[in]  CurrentAddress  Address of the instruction (PC)
145:   @param[out] NextAddress     Address of the next instruction to execute
146:   @retval EFI_SUCCESS        Instruction executed
147:   @retval EFI_UNSUPPORTED    Instruction is not implemented by the interpreter
148: **/
149: EFI_STATUS
150: EFIAPI
151: PpcExecuteInstruction (
152:     IN  UINT32  Instruction,
153:     IN  UINT32  CurrentAddress,
154:     OUT UINT32* NextAddress
155:     );
156: 
157: /**
158:   Execute a stream of PowerPC instructions (fetch/decode/execute loop)
159:   @param[in]  InstructionStream  Pointer to the instruction stream in guest memory
160:   @param[in]  MaxInstructions    Maximum number of instructions to execute
161:   @param[out] ExecutedCount      Number of instructions actually executed
162:   @retval EFI_STATUS
163: **/
164: EFI_STATUS
165: EFIAPI
166: PpcExecuteBlock (
167:     IN  UINT32* InstructionStream,
168:     IN  UINTN   MaxInstructions,
169:     OUT UINTN*  ExecutedCount
170:     );
171: 
172: /**
173:   Run the CPU self-test suite
174:   @retval EFI_SUCCESS       All tests passed
175:   @retval EFI_LOAD_ERROR    One or more tests failed
176: **/
177: EFI_STATUS
178: EFIAPI
179: PpcRunSelfTest (
180:     VOID
181:     );
182: 
183: /**
184:   Memory access callbacks used by the interpreter for load/store instructions
185: 
186:   The default implementation performs an identity-mapped (physical == virtual)
187:   access into guest memory. A platform layer can override this to route loads
188:   and stores through the memory manager.
189: **/
190: typedef UINT8 (*PPC_CPU_READ_MEMORY)   (IN UINT32 Address);
191: typedef VOID  (*PPC_CPU_WRITE_MEMORY)  (IN UINT32 Address, IN UINT8 Value);
192: 
193: /**
194:   Override the interpreter memory access callbacks
195:   @param[in] Read   Byte read callback (NULL restores the default)
196:   @param[in] Write  Byte write callback (NULL restores the default)
197:   @retval EFI_STATUS
198: **/
199: EFI_STATUS
200: EFIAPI
201: PpcSetMemoryAccess (
202:     IN PPC_CPU_READ_MEMORY   Read,
203:     IN PPC_CPU_WRITE_MEMORY  Write
204:     );
205: 
206: /**
207:   Route the interpreter's default load/store path through an emulated
208:   guest RAM region.
209: 
210:   After this call, guest addresses in [GuestBase, GuestBase + Size) are
211:   backed by the host buffer at HostBase (host = HostBase + guest - GuestBase).
212:   Reads of unmapped guest addresses return zero; writes are dropped.
213: 
214:   @param[in] HostBase   Host virtual address backing the region
215:   @param[in] GuestBase  Guest-visible base address of the region
216:   @param[in] Size       Size of the region in bytes
217:   @retval EFI_STATUS
218: **/
219: EFI_STATUS
220: EFIAPI
221: PpcSetGuestMemory (
222:     IN VOID*  HostBase,
223:     IN UINT32 GuestBase,
224:     IN UINT32 Size
225:     );
226: 
227: /**
228:   Read a single byte from guest memory using the interpreter's active
229:   memory path (the same one used by load instructions).
230: 
231:   @param[in] Address  Guest address to read
232:   @retval The byte value (zero for unmapped addresses)
233: **/
234: UINT8
235: PpcReadGuestByte (
236:     IN UINT32 Address
237:     );
238: 
239: /**
240:   Write a single byte to guest memory using the interpreter's active
241:   memory path (the same one used by store instructions). Writes to
242:   unmapped addresses are dropped.
243: 
244:   @param[in] Address  Guest address to write
245:   @param[in] Value    Byte value to write
246: **/
247: VOID
248: PpcWriteGuestByte (
249:     IN UINT32 Address,
250:     IN UINT8  Value
251:     );
252: 
253: /**
254:   Execute a block of translated PowerPC instructions
255:   @param[in] InstructionBlock   Pointer to the instruction block
256:   @param[in] BlockSize          Size of the instruction block in bytes
257:   @retval EFI_STATUS
258: **/
259: EFI_STATUS
260: EFIAPI
261: PpcExecuteTranslatedBlock (
262:     IN  UINT32* InstructionBlock,
263:     IN  UINTN   BlockSize
264:     );
265: 
266: /**
267:   Handle PowerPC exceptions
268:   @param[in] ExceptionType     Type of exception to handle
269:   @param[in] ExceptionAddress  Address where exception occurred
270:   @retval EFI_STATUS
271: **/
272: EFI_STATUS
273: EFIAPI
274: PpcHandleException (
275:     IN UINT32 ExceptionType,
276:     IN UINT32 ExceptionAddress
277:     );
278: 
279: /**
280:   Get value of a PowerPC register
281:   @param[in]  RegisterNumber   Number of the register to get
282:   @param[out] Value            Pointer to store the register value
283:   @retval EFI_STATUS
284: **/
285: EFI_STATUS
286: EFIAPI
287: PpcGetRegisterValue (
288:     IN  UINT8  RegisterNumber,
289:     OUT UINT32* Value
290:     );
291: 
292: /**
293:   Set value of a PowerPC register
294:   @param[in] RegisterNumber   Number of the register to set
295:   @param[in] Value            Value to set in the register
296:   @retval EFI_STATUS
297: **/
298: EFI_STATUS
299: EFIAPI
300: PpcSetRegisterValue (
301:     IN UINT8  RegisterNumber,
302:     IN UINT32 Value
303:     );
304: 
305: /**
306:   Get value of a PowerPC general purpose register
307:   @param[in] RegisterNumber   Number of the register to get
308:   @retval Register value
309: **/
310: UINT32
311: PpcGetGprValue (
312:     IN UINT8 RegisterNumber
313:     );
314: 
315: /**
316:   Set value of a PowerPC general purpose register
317:   @param[in] RegisterNumber   Number of the register to set
318:   @param[in] Value            Value to set in the register
319: **/
320: VOID
321: PpcSetGprValue (
322:     IN UINT8 RegisterNumber,
323:     IN UINT32 Value
324:     );
325: 
326: #endif // __PPC_TRANSLATION_H__

(End of file - total 326 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\memory\\manager.h"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory\manager.h</path>
<type>file</type>
<content>
1: #ifndef __PPC_MEMORY_MANAGER_H__
2: #define __PPC_MEMORY_MANAGER_H__
3: 
4: #include <efi.h>
5: 
6: // Memory protection flags
7: #define PPC_MEMORY_READ     0x01
8: #define PPC_MEMORY_WRITE    0x02
9: #define PPC_MEMORY_EXECUTE  0x04
10: 
11: // Memory info structure
12: typedef struct {
13:     EFI_PHYSICAL_ADDRESS BaseAddress;
14:     UINT64               Size;
15:     BOOLEAN              IsInitialized;
16: } PPC_MEMORY_INFO;
17: 
18: // Memory manager context structure
19: typedef struct {
20:     EFI_PHYSICAL_ADDRESS BaseAddress;
21:     UINT64               Size;
22:     BOOLEAN              IsInitialized;
23:     VOID*                VirtualBase;
24:     EFI_MEMORY_DESCRIPTOR* MemoryMap;
25:     UINTN                MapKey;
26:     UINTN                DescriptorSize;
27:     UINT32               DescriptorVersion;
28:     UINTN                MapSize;
29:     BOOLEAN              UseUefiMemory;
30: } PPC_MEMORY_MANAGER_CONTEXT;
31: 
32: /**
33:   Initialize PowerPC memory manager
34:   @param[in] BaseAddress   Base physical address for memory management
35:   @param[in] Size          Size of memory to manage
36:   @retval EFI_STATUS
37: **/
38: EFI_STATUS
39: EFIAPI
40: PpcInitializeMemoryManager (
41:     IN  EFI_PHYSICAL_ADDRESS BaseAddress,
42:     IN  UINT64               Size
43:     );
44: 
45: /**
46:   Allocate memory for PowerPC emulation
47:   @param[in]  Size             Size of memory to allocate
48:   @param[out] VirtualAddress   Pointer to store virtual address
49:   @param[out] PhysicalAddress  Pointer to store physical address
50:   @retval EFI_STATUS
51: **/
52: EFI_STATUS
53: EFIAPI
54: PpcAllocateMemory (
55:     IN  UINT64   Size,
56:     OUT VOID**   VirtualAddress,
57:     OUT UINT64*  PhysicalAddress
58:     );
59: 
60: /**
61:   Free allocated memory
62:   @param[in] VirtualAddress   Virtual address to free
63:   @param[in] Size             Size of memory to free
64:   @retval EFI_STATUS
65: **/
66: EFI_STATUS
67: EFIAPI
68: PpcFreeMemory (
69:     IN VOID* VirtualAddress,
70:     IN UINT64 Size
71:     );
72: 
73: /**
74:   Map physical memory to virtual address
75:   @param[in]  PhysicalAddress   Physical address to map
76:   @param[in]  Size              Size of memory to map
77:   @param[out] VirtualAddress    Pointer to store mapped virtual address
78:   @retval EFI_STATUS
79: **/
80: EFI_STATUS
81: EFIAPI
82: PpcMapMemory (
83:     IN  EFI_PHYSICAL_ADDRESS PhysicalAddress,
84:     IN  UINT64               Size,
85:     OUT VOID**               VirtualAddress
86:     );
87: 
88: /**
89:   Unmap virtual memory
90:   @param[in] VirtualAddress   Virtual address to unmap
91:   @retval EFI_STATUS
92: **/
93: EFI_STATUS
94: EFIAPI
95: PpcUnmapMemory (
96:     IN VOID* VirtualAddress
97:     );
98: 
99: /**
100:   Translate physical address to virtual address
101:   @param[in]  PhysicalAddress   Physical address to translate
102:   @param[out] VirtualAddress    Pointer to store translated virtual address
103:   @retval EFI_STATUS
104: **/
105: EFI_STATUS
106: EFIAPI
107: PpcTranslateAddress (
108:     IN  UINT64   PhysicalAddress,
109:     OUT UINT64*  VirtualAddress
110:     );
111: 
112: /**
113:   Set memory protection attributes
114:   @param[in] Address          Memory address to set protection for
115:   @param[in] Size             Size of memory region
116:   @param[in] ProtectionFlags  Protection flags (read, write, execute)
117:   @retval EFI_STATUS
118: **/
119: EFI_STATUS
120: EFIAPI
121: PpcSetMemoryProtection (
122:     IN  UINT64   Address,
123:     IN  UINT64   Size,
124:     IN  UINT32   ProtectionFlags
125:     );
126: 
127: /**
128:   Get memory management information
129:   @param[out] MemoryInfo   Pointer to structure to fill with memory info
130:   @retval EFI_STATUS
131: **/
132: EFI_STATUS
133: EFIAPI
134: PpcGetMemoryInfo (
135:     OUT PPC_MEMORY_INFO* MemoryInfo
136:     );
137: 
138: /**
139:   Get the UEFI-backed guest RAM region owned by the memory manager.
140:   The guest-visible base is the address passed to PpcInitializeMemoryManager;
141:   the host base is where UEFI actually placed the pages.
142: 
143:   @param[out] HostBase    Host virtual address of the region (may be NULL)
144:   @param[out] GuestBase   Guest-visible base address (may be NULL)
145:   @param[out] Size        Size of the region in bytes (may be NULL)
146:   @retval EFI_SUCCESS          Region is available
147:   @retval EFI_NOT_READY        Memory manager has no allocated region
148: **/
149: EFI_STATUS
150: EFIAPI
151: PpcGetGuestMemoryRegion (
152:     OUT VOID*  *HostBase,
153:     OUT UINT64 *GuestBase,
154:     OUT UINT64 *Size
155:     );
156: 
157: #endif // __PPC_MEMORY_MANAGER_H__

(End of file - total 157 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\memory\\manager_impl.c"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory\manager_impl.c</path>
<type>file</type>
<content>
1: #include "manager.h"
2: #include <efi.h>
3: #include <efilib.h>
4: 
5: // Global memory manager context
6: STATIC PPC_MEMORY_MANAGER_CONTEXT g_MemoryManager = {0};
7: 
8: EFI_STATUS
9: PpcInitializeMemoryManager (
10:     IN  EFI_PHYSICAL_ADDRESS BaseAddress,
11:     IN  UINT64               Size
12:     )
13: {
14:     // The bootloader path may re-initialize; keep the first allocated region.
15:     if (g_MemoryManager.IsInitialized && g_MemoryManager.VirtualBase != NULL) {
16:         Print(L"PowerPC Memory Manager already initialized\n");
17:         return EFI_SUCCESS;
18:     }
19: 
20:     // Initialize the memory manager context
21:     ZeroMem(&g_MemoryManager, sizeof(g_MemoryManager));
22: 
23:     if (BaseAddress == 0 || Size == 0) {
24:         Print(L"Invalid memory manager parameters: base=0x%x size=%d\n",
25:               BaseAddress, Size);
26:         return EFI_INVALID_PARAMETER;
27:     }
28: 
29:     g_MemoryManager.BaseAddress = BaseAddress;
30:     g_MemoryManager.Size = Size;
31:     g_MemoryManager.UseUefiMemory = TRUE;
32: 
33:     // Allocate the guest RAM region. Try to place it at the requested guest
34:     // base first (identity mapped in the UEFI boot environment); if that is
35:     // not free, fall back to a region anywhere and translate by offset.
36:     UINTN Pages = (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
37:     EFI_PHYSICAL_ADDRESS HostBase = BaseAddress;
38:     EFI_STATUS Status = BS->AllocatePages(
39:         AllocateAddress,
40:         EfiLoaderData,
41:         Pages,
42:         &HostBase
43:     );
44: 
45:     if (EFI_ERROR(Status)) {
46:         HostBase = 0;
47:         Status = BS->AllocatePages(
48:             AllocateAnyPages,
49:             EfiLoaderData,
50:             Pages,
51:             &HostBase
52:         );
53:         if (EFI_ERROR(Status)) {
54:             Print(L"Failed to allocate %d pages for guest RAM: %r\n", Pages, Status);
55:             g_MemoryManager.IsInitialized = FALSE;
56:             return Status;
57:         }
58:         Print(L"Guest RAM allocated at host 0x%x (guest base 0x%x, %d MB)\n",
59:               HostBase, BaseAddress, (UINTN)(Size >> 20));
60:     } else {
61:         Print(L"Guest RAM allocated at 0x%x (identity mapped, %d MB)\n",
62:               HostBase, (UINTN)(Size >> 20));
63:     }
64: 
65:     g_MemoryManager.VirtualBase = (VOID*)(UINTN)HostBase;
66:     g_MemoryManager.IsInitialized = TRUE;
67: 
68:     // Clear the guest RAM
69:     ZeroMem(g_MemoryManager.VirtualBase, (UINTN)Size);
70: 
71:     // Get the current memory map
72:     Status = BS->GetMemoryMap(
73:         &g_MemoryManager.MapSize,
74:         NULL,
75:         &g_MemoryManager.MapKey,
76:         &g_MemoryManager.DescriptorSize,
77:         &g_MemoryManager.DescriptorVersion
78:     );
79: 
80:     if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
81:         Print(L"Failed to get memory map size: %r\n", Status);
82:         return Status;
83:     }
84: 
85:     // Allocate buffer for memory map
86:     EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
87:     Status = BS->AllocatePool(EfiBootServicesData, g_MemoryManager.MapSize, (VOID**)&MemoryMap);
88:     if (EFI_ERROR(Status)) {
89:         Print(L"Failed to allocate memory map buffer: %r\n", Status);
90:         return Status;
91:     }
92: 
93:     // Get the actual memory map
94:     Status = BS->GetMemoryMap(
95:         &g_MemoryManager.MapSize,
96:         MemoryMap,
97:         &g_MemoryManager.MapKey,
98:         &g_MemoryManager.DescriptorSize,
99:         &g_MemoryManager.DescriptorVersion
100:     );
101: 
102:     if (EFI_ERROR(Status)) {
103:         Print(L"Failed to get memory map: %r\n", Status);
104:         BS->FreePool(MemoryMap);
105:         return Status;
106:     }
107: 
108:     g_MemoryManager.MemoryMap = MemoryMap;
109: 
110:     Print(L"PowerPC Memory Manager initialized\n");
111:     Print(L"Base Address: 0x%x\n", BaseAddress);
112:     Print(L"Size: %d bytes\n", Size);
113: 
114:     return EFI_SUCCESS;
115: }
116: 
117: EFI_STATUS
118: PpcAllocateMemory (
119:     IN  UINT64   Size,
120:     OUT VOID**   VirtualAddress,
121:     OUT UINT64*  PhysicalAddress
122:     )
123: {
124:     if (VirtualAddress == NULL || PhysicalAddress == NULL) {
125:         return EFI_INVALID_PARAMETER;
126:     }
127:     
128:     Print(L"Allocating %d bytes of memory\n", Size);
129:     
130:     // In a real implementation, we would:
131:     // 1. Find suitable memory region in our managed space
132:     // 2. Allocate virtual address space 
133:     // 3. Map to physical memory
134:     // 4. Return allocated addresses
135:     
136:     EFI_STATUS Status = BS->AllocatePages(
137:         AllocateAnyPages,
138:         EfiBootServicesData,
139:         (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE,
140:         (EFI_PHYSICAL_ADDRESS*)PhysicalAddress
141:     );
142:     
143:     if (EFI_ERROR(Status)) {
144:         Print(L"Failed to allocate physical pages: %r\n", Status);
145:         return Status;
146:     }
147:     
148:     *VirtualAddress = (VOID*)(UINTN)(*PhysicalAddress);
149:     
150:     Print(L"Allocated memory - Virtual: 0x%x, Physical: 0x%x\n", *VirtualAddress, *PhysicalAddress);
151:     
152:     return EFI_SUCCESS;
153: }
154: 
155: EFI_STATUS
156: PpcFreeMemory (
157:     IN VOID* VirtualAddress,
158:     IN UINT64 Size
159:     )
160: {
161:     if (VirtualAddress == NULL) {
162:         return EFI_INVALID_PARAMETER;
163:     }
164:     
165:     Print(L"Freeing memory at virtual address 0x%x (size: %d bytes)\n", VirtualAddress, Size);
166:     
167:     // In a real implementation:
168:     // 1. Calculate page-aligned physical address
169:     // 2. Free the allocated pages
170:     // 3. Update memory management structures
171:     
172:     EFI_PHYSICAL_ADDRESS PhysicalAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)VirtualAddress;
173:     
174:     UINTN Pages = (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
175:     
176:     EFI_STATUS Status = BS->FreePages(PhysicalAddress, Pages);
177:     
178:     if (EFI_ERROR(Status)) {
179:         Print(L"Failed to free pages: %r\n", Status);
180:         return Status;
181:     }
182:     
183:     Print(L"Freed %d pages from 0x%x\n", Pages, PhysicalAddress);
184:     
185:     return EFI_SUCCESS;
186: }
187: 
188: EFI_STATUS
189: PpcMapMemory (
190:     IN  EFI_PHYSICAL_ADDRESS PhysicalAddress,
191:     IN  UINT64               Size,
192:     OUT VOID**               VirtualAddress
193:     )
194: {
195:     if (VirtualAddress == NULL) {
196:         return EFI_INVALID_PARAMETER;
197:     }
198: 
199:     // For guest RAM the mapping is a plain offset into the allocated region.
200:     return PpcTranslateAddress(PhysicalAddress, (UINT64*)VirtualAddress);
201: }
202: 
203: EFI_STATUS
204: PpcUnmapMemory (
205:     IN VOID* VirtualAddress
206:     )
207: {
208:     if (VirtualAddress == NULL) {
209:         return EFI_INVALID_PARAMETER;
210:     }
211:     
212:     Print(L"Unmapping virtual address 0x%x\n", VirtualAddress);
213:     
214:     // In a real implementation:
215:     // 1. Unmap the virtual address from physical memory
216:     // 2. Release the mapping
217:     
218:     return EFI_SUCCESS;
219: }
220: 
221: EFI_STATUS
222: PpcTranslateAddress (
223:     IN  UINT64   PhysicalAddress,
224:     OUT UINT64*  VirtualAddress
225:     )
226: {
227:     if (VirtualAddress == NULL) {
228:         return EFI_INVALID_PARAMETER;
229:     }
230: 
231:     if (!g_MemoryManager.IsInitialized || g_MemoryManager.VirtualBase == NULL) {
232:         return EFI_NOT_READY;
233:     }
234: 
235:     // Translate a guest (physical) address to the host virtual address that
236:     // backs it. Only addresses inside the allocated guest RAM region are
237:     // valid; MMIO regions return EFI_NOT_FOUND.
238:     if (PhysicalAddress < g_MemoryManager.BaseAddress ||
239:         PhysicalAddress - g_MemoryManager.BaseAddress >= g_MemoryManager.Size) {
240:         return EFI_NOT_FOUND;
241:     }
242: 
243:     *VirtualAddress =
244:         (UINT64)(UINTN)g_MemoryManager.VirtualBase +
245:         (PhysicalAddress - g_MemoryManager.BaseAddress);
246: 
247:     return EFI_SUCCESS;
248: }
249: 
250: EFI_STATUS
251: PpcSetMemoryProtection (
252:     IN  UINT64   Address,
253:     IN  UINT64   Size,
254:     IN  UINT32   ProtectionFlags
255:     )
256: {
257:     // In a real implementation:
258:     // 1. Set memory protection attributes (read, write, execute)
259:     // 2. Handle page-level memory protection
260:     
261:     Print(L"Setting memory protection for address 0x%x (size: %d bytes)\n", Address, Size);
262:     Print(L"Protection flags: 0x%x\n", ProtectionFlags);
263:     
264:     return EFI_SUCCESS;
265: }
266: 
267: EFI_STATUS
268: PpcGetMemoryInfo (
269:     OUT PPC_MEMORY_INFO* MemoryInfo
270:     )
271: {
272:     if (MemoryInfo == NULL) {
273:         return EFI_INVALID_PARAMETER;
274:     }
275:     
276:     // Initialize memory info structure
277:     ZeroMem(MemoryInfo, sizeof(PPC_MEMORY_INFO));
278:     
279:     MemoryInfo->BaseAddress = g_MemoryManager.BaseAddress;
280:     MemoryInfo->Size = g_MemoryManager.Size;
281:     MemoryInfo->IsInitialized = g_MemoryManager.IsInitialized;
282:     
283:     return EFI_SUCCESS;
284: }
285: 
286: EFI_STATUS
287: PpcGetGuestMemoryRegion (
288:     OUT VOID*  *HostBase,
289:     OUT UINT64 *GuestBase,
290:     OUT UINT64 *Size
291:     )
292: {
293:     if (!g_MemoryManager.IsInitialized || g_MemoryManager.VirtualBase == NULL) {
294:         return EFI_NOT_READY;
295:     }
296: 
297:     if (HostBase != NULL) {
298:         *HostBase = g_MemoryManager.VirtualBase;
299:     }
300:     if (GuestBase != NULL) {
301:         *GuestBase = g_MemoryManager.BaseAddress;
302:     }
303:     if (Size != NULL) {
304:         *Size = g_MemoryManager.Size;
305:     }
306: 
307:     return EFI_SUCCESS;
308: }
309: 
310: // Additional memory management functions for PowerPC-specific needs
311: EFI_STATUS
312: PpcAllocateKernelMemory (
313:     IN  UINT64   Size,
314:     OUT VOID**   VirtualAddress,
315:     OUT UINT64*  PhysicalAddress
316:     )
317: {
318:     if (VirtualAddress == NULL || PhysicalAddress == NULL) {
319:         return EFI_INVALID_PARAMETER;
320:     }
321:     
322:     Print(L"Allocating kernel memory: %d bytes\n", Size);
323:     
324:     // Allocate memory for kernel with special properties
325:     EFI_STATUS Status = BS->AllocatePages(
326:         AllocateAnyPages,
327:         EfiRuntimeServicesData,  // Use runtime services data for kernel
328:         (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE,
329:         (EFI_PHYSICAL_ADDRESS*)PhysicalAddress
330:     );
331:     
332:     if (EFI_ERROR(Status)) {
333:         Print(L"Failed to allocate kernel memory: %r\n", Status);
334:         return Status;
335:     }
336:     
337:     *VirtualAddress = (VOID*)(UINTN)(*PhysicalAddress);
338:     
339:     // Clear the allocated memory
340:     ZeroMem(*VirtualAddress, Size);
341:     
342:     Print(L"Kernel memory allocated - Virtual: 0x%x, Physical: 0x%x\n", *VirtualAddress, *PhysicalAddress);
343:     
344:     return EFI_SUCCESS;
345: }
346: 
347: EFI_STATUS
348: PpcAllocateBootMemory (
349:     IN  UINT64   Size,
350:     OUT VOID**   VirtualAddress,
351:     OUT UINT64*  PhysicalAddress
352:     )
353: {
354:     if (VirtualAddress == NULL || PhysicalAddress == NULL) {
355:         return EFI_INVALID_PARAMETER;
356:     }
357:     
358:     Print(L"Allocating boot memory: %d bytes\n", Size);
359:     
360:     // Allocate memory for boot process with specific attributes
361:     EFI_STATUS Status = BS->AllocatePages(
362:         AllocateAnyPages,
363:         EfiBootServicesData,  // Boot services memory for boot process
364:         (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE,
365:         (EFI_PHYSICAL_ADDRESS*)PhysicalAddress
366:     );
367:     
368:     if (EFI_ERROR(Status)) {
369:         Print(L"Failed to allocate boot memory: %r\n", Status);
370:         return Status;
371:     }
372:     
373:     *VirtualAddress = (VOID*)(UINTN)(*PhysicalAddress);
374:     
375:     // Clear the allocated memory
376:     ZeroMem(*VirtualAddress, Size);
377:     
378:     Print(L"Boot memory allocated - Virtual: 0x%x, Physical: 0x%x\n", *VirtualAddress, *PhysicalAddress);
379:     
380:     return EFI_SUCCESS;
381: }
382: 
383: // Function to dump memory map for debugging
384: EFI_STATUS
385: PpcDumpMemoryMap (
386:     VOID
387:     )
388: {
389:     if (g_MemoryManager.MemoryMap == NULL) {
390:         Print(L"No memory map available\n");
391:         return EFI_NOT_READY;
392:     }
393:     
394:     Print(L"Memory Map (%d descriptors)\n", g_MemoryManager.MapSize / g_MemoryManager.DescriptorSize);
395:     Print(L"----------------------------------------\n");
396:     
397:     EFI_MEMORY_DESCRIPTOR* Desc = g_MemoryManager.MemoryMap;
398:     
399:     for (UINTN i = 0; i < g_MemoryManager.MapSize / g_MemoryManager.DescriptorSize; i++) {
400:         Print(L"Type: %d, Physical: 0x%x, Virtual: 0x%x, Pages: %d\n", 
401:               Desc->Type,
402:               Desc->PhysicalStart,
403:               Desc->VirtualStart,
404:               Desc->NumberOfPages);
405:         
406:         // Move to next descriptor
407:         Desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Desc + g_MemoryManager.DescriptorSize);
408:     }
409:     
410:     return EFI_SUCCESS;
411: }

(End of file - total 411 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 2.7s)

_Thinking:_

Now let me look at the interpreter implementation and the hardware abstraction layer, plus the remaining platform and utils files.

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
1: #include "interpreter.h"
2: #include "translation.h"
3: #include <efi.h>
4: #include <efilib.h>
5: 
6: // Global PowerPC CPU context (backing store for the interpreter and the
7: // public register accessor API)
8: PPC_CPU_CONTEXT g_PpcContext = {0};
9: 
10: // ---------------------------------------------------------------------------
11: // Instruction field extraction (bit 0 = most significant bit of the word)
12: // ---------------------------------------------------------------------------
13: #define OP(w)      ((w) >> 26)
14: #define RT(w)      (((w) >> 21) & 0x1F)
15: #define RS(w)      (((w) >> 21) & 0x1F)
16: #define RD(w)      (((w) >> 21) & 0x1F)
17: #define RA(w)      (((w) >> 16) & 0x1F)
18: #define RB(w)      (((w) >> 11) & 0x1F)
19: #define BO(w)      (((w) >> 21) & 0x1F)
20: #define BI(w)      (((w) >> 16) & 0x1F)
21: #define BF(w)      (((w) >> 23) & 0x7)
22: #define SH(w)      (((w) >> 11) & 0x1F)
23: #define MB(w)      (((w) >> 6) & 0x1F)
24: #define ME(w)      ((w) & 0x1F)
25: #define SIMM(w)    ((UINT32)(INT32)(INT16)((w) & 0xFFFF))
26: #define UIMM(w)    ((w) & 0xFFFF)
27: #define XO(w)      (((w) >> 1) & 0x3FE)      // 10-bit XO with OE bit masked out
28: #define XO10(w)    (((w) >> 1) & 0x3FF)      // full 10-bit XO
29: #define Rc(w)      ((w) & 1)
30: #define LK(w)      ((w) & 1)
31: #define AA(w)      (((w) >> 1) & 1)
32: #define SPR(w)     ((((w) >> 16) & 0x1F) | ((((w) >> 11) & 0x1F) << 5))
33: #define BD(w)      ((UINT32)(INT32)(INT16)((((w) >> 2) & 0x3FFF) << 2))
34: // LI is a 24-bit signed field at word bits 2-25 (sign-extended)
35: #define LI(w)      ((UINT32)((((w) >> 2) & 0x800000) ? \
36:                              (((w) >> 2) | 0xFF000000) : \
37:                              (((w) >> 2) & 0xFFFFFF)))
38: 
39: // Effective address helpers (RA==0 means GPR(0) is NOT used)
40: #define EaD(w, ra) (((ra) == 0) ? SIMM(w) : (g_PpcContext.Gpr[ra] + SIMM(w)))
41: #define EaX(w, ra, rb) ((((ra) == 0) ? 0U : g_PpcContext.Gpr[ra]) + g_PpcContext.Gpr[rb])
42: 
43: // X-form primary XO values for opcode 31
44: #define XO_CMP         0
45: #define XO_TW          4
46: #define XO_SUBFC       8
47: #define XO_ADDC       10
48: #define XO_MULHWU     11
49: #define XO_LWARX      20
50: #define XO_LWZX       23
51: #define XO_SLW        24
52: #define XO_CNTLZW     26
53: #define XO_AND        28
54: #define XO_CMPL       32
55: #define XO_SUBF       40
56: #define XO_DCBST      54
57: #define XO_ANDC       60
58: #define XO_MULHW      75
59: #define XO_TLBIEL     78
60: #define XO_MFMSR      83
61: #define XO_DCBF       86
62: #define XO_LBZX       87
63: #define XO_NEG       104
64: #define XO_LBZUX     119
65: #define XO_NOR       124
66: #define XO_SUBFE     136
67: #define XO_ADDE      138
68: #define XO_MTCRF     144
69: #define XO_MTMSR     146
70: #define XO_STWCX_    150
71: #define XO_STWX      151
72: #define XO_STWUX     183
73: #define XO_SUBFZE    200
74: #define XO_ADDZE     202
75: #define XO_STBX      215
76: #define XO_SUBFME    232
77: #define XO_ADDME     234
78: #define XO_MULLW     235
79: #define XO_MTSRIN    242
80: #define XO_DCBTST    246
81: #define XO_STBUX     247
82: #define XO_ADD       266
83: #define XO_DCBT      278
84: #define XO_LHZX      279
85: #define XO_EQV       284
86: #define XO_TLBIE     306
87: #define XO_LHZUX     311
88: #define XO_XOR       316
89: #define XO_MFSPR     339
90: #define XO_LHAX      343
91: #define XO_TLBIA     370
92: #define XO_MFTB      371
93: #define XO_LHAUX     375
94: #define XO_STHX      407
95: #define XO_ORC       412
96: #define XO_STHUX     439
97: #define XO_OR        444
98: #define XO_DIVWU     459
99: #define XO_MTSPR     467
100: #define XO_DCBI      470
101: #define XO_NAND      476
102: #define XO_DIVW      491
103: #define XO_MCRXR     512
104: #define XO_LSWX      533
105: #define XO_LWBRX     534
106: #define XO_SRW       536
107: #define XO_MFSR      595
108: #define XO_LSWI      597
109: #define XO_SYNC      598
110: #define XO_MTSR      625
111: #define XO_MFSRIN    659
112: #define XO_STSWX     661
113: #define XO_STWBRX    662
114: #define XO_STSWI     725
115: #define XO_LHBRX     790
116: #define XO_SRAW      792
117: #define XO_SRAWI     824
118: #define XO_EIEIO     854
119: #define XO_STHBRX    918
120: #define XO_ICBI      982
121: #define XO_DCBZ     1014
122: 
123: // XL-form XO values for opcode 19
124: #define XO19_MCRF      0
125: #define XO19_BCLR     16
126: #define XO19_RFI      50
127: #define XO19_ISYNC   150
128: #define XO19_BCCTR   528
129: 
130: // SPR numbers
131: #define SPR_XER    1
132: #define SPR_LR     8
133: #define SPR_CTR    9
134: #define SPR_PVR  287
135: 
136: // ---------------------------------------------------------------------------
137: // Memory access (default: identity-mapped, big-endian guest memory)
138: // ---------------------------------------------------------------------------
139: // ---------------------------------------------------------------------------
140: // Emulated guest RAM (default memory backing for the interpreter)
141: //
142: // When PpcSetGuestMemory() has been called, guest addresses inside
143: // [GuestBase, GuestBase + Size) are backed by the host buffer. Addresses
144: // outside the region read as zero and ignore writes.
145: // ---------------------------------------------------------------------------
146: static VOID*  g_GuestMemoryHostBase  = NULL;
147: static UINT32 g_GuestMemoryGuestBase = 0;
148: static UINT32 g_GuestMemorySize      = 0;
149: 
150: static UINT8
151: PpcDefaultReadByte (
152:     IN UINT32 Address
153:     )
154: {
155:     if (g_GuestMemoryHostBase != NULL &&
156:         Address >= g_GuestMemoryGuestBase &&
157:         (Address - g_GuestMemoryGuestBase) < g_GuestMemorySize) {
158:         return *(volatile UINT8*)((UINTN)g_GuestMemoryHostBase +
159:                                   (Address - g_GuestMemoryGuestBase));
160:     }
161:     return 0;  // Unmapped guest address reads as zero
162: }
163: 
164: static VOID
165: PpcDefaultWriteByte (
166:     IN UINT32 Address,
167:     IN UINT8  Value
168:     )
169: {
170:     if (g_GuestMemoryHostBase != NULL &&
171:         Address >= g_GuestMemoryGuestBase &&
172:         (Address - g_GuestMemoryGuestBase) < g_GuestMemorySize) {
173:         *(volatile UINT8*)((UINTN)g_GuestMemoryHostBase +
174:                            (Address - g_GuestMemoryGuestBase)) = Value;
175:     }
176: }
177: 
178: static PPC_CPU_READ_MEMORY  g_ReadByte  = PpcDefaultReadByte;
179: static PPC_CPU_WRITE_MEMORY g_WriteByte = PpcDefaultWriteByte;
180: 
181: EFI_STATUS
182: PpcSetMemoryAccess (
183:     IN PPC_CPU_READ_MEMORY   Read,
184:     IN PPC_CPU_WRITE_MEMORY  Write
185:     )
186: {
187:     g_ReadByte  = (Read  != NULL) ? Read  : PpcDefaultReadByte;
188:     g_WriteByte = (Write != NULL) ? Write : PpcDefaultWriteByte;
189:     return EFI_SUCCESS;
190: }
191: 
192: EFI_STATUS
193: PpcSetGuestMemory (
194:     IN VOID*  HostBase,
195:     IN UINT32 GuestBase,
196:     IN UINT32 Size
197:     )
198: {
199:     g_GuestMemoryHostBase  = HostBase;
200:     g_GuestMemoryGuestBase = GuestBase;
201:     g_GuestMemorySize      = Size;
202:     return EFI_SUCCESS;
203: }
204: 
205: UINT8
206: PpcReadGuestByte (
207:     IN UINT32 Address
208:     )
209: {
210:     return g_ReadByte(Address);
211: }
212: 
213: VOID
214: PpcWriteGuestByte (
215:     IN UINT32 Address,
216:     IN UINT8  Value
217:     )
218: {
219:     g_WriteByte(Address, Value);
220: }
221: 
222: static UINT32 CpuRead16 (UINT32 A) { return ((UINT32)g_ReadByte(A) << 8) | g_ReadByte(A + 1); }
223: static UINT32 CpuRead32 (UINT32 A) { return (CpuRead16(A) << 16) | CpuRead16(A + 2); }
224: static VOID   CpuWrite16(UINT32 A, UINT32 V) { g_WriteByte(A, (UINT8)(V >> 8)); g_WriteByte(A + 1, (UINT8)V); }
225: static VOID   CpuWrite32(UINT32 A, UINT32 V) { CpuWrite16(A, V >> 16); CpuWrite16(A + 2, V); }
226: 
227: // Byte-reversed access (for lwbrx/stwbrx etc.)
228: static UINT32 CpuRead32Rev (UINT32 A)
229: {
230:     return (UINT32)g_ReadByte(A) | ((UINT32)g_ReadByte(A + 1) << 8) |
231:            ((UINT32)g_ReadByte(A + 2) << 16) | ((UINT32)g_ReadByte(A + 3) << 24);
232: }
233: static UINT32 CpuRead16Rev (UINT32 A)
234: {
235:     return (UINT32)g_ReadByte(A) | ((UINT32)g_ReadByte(A + 1) << 8);
236: }
237: static VOID CpuWrite32Rev (UINT32 A, UINT32 V)
238: {
239:     g_WriteByte(A, (UINT8)V); g_WriteByte(A + 1, (UINT8)(V >> 8));
240:     g_WriteByte(A + 2, (UINT8)(V >> 16)); g_WriteByte(A + 3, (UINT8)(V >> 24));
241: }
242: static VOID CpuWrite16Rev (UINT32 A, UINT32 V)
243: {
244:     g_WriteByte(A, (UINT8)V); g_WriteByte(A + 1, (UINT8)(V >> 8));
245: }
246: 
247: // ---------------------------------------------------------------------------
248: // Condition Register / XER helpers
249: // ---------------------------------------------------------------------------
250: VOID
251: PpcSetCrField (
252:     IN UINT32 Field,
253:     IN UINT32 Value
254:     )
255: {
256:     UINT32 Shift = 28 - (Field * 4);
257:     g_PpcContext.Cr = (g_PpcContext.Cr & ~(0xFUL << Shift)) | ((Value & 0xF) << Shift);
258: }
259: 
260: UINT32
261: PpcGetCrField (
262:     IN UINT32 Field
263:     )
264: {
265:     UINT32 Shift = 28 - (Field * 4);
266:     return (g_PpcContext.Cr >> Shift) & 0xF;
267: }
268: 
269: VOID
270: PpcSetXerCarry (
271:     IN UINT32 Carry
272:     )
273: {
274:     g_PpcContext.Xer = (g_PpcContext.Xer & ~PPC_XER_CA) | (Carry ? PPC_XER_CA : 0);
275: }
276: 
277: VOID
278: PpcSetXerOverflow (
279:     IN UINT32 Overflow
280:     )
281: {
282:     g_PpcContext.Xer = (g_PpcContext.Xer & ~PPC_XER_OV) | (Overflow ? PPC_XER_OV : 0);
283:     if (Overflow) {
284:         g_PpcContext.Xer |= PPC_XER_SO;
285:     }
286: }
287: 
288: // Set CR0 from a 32-bit result (Rc=1 record bit)
289: static VOID
290: PpcSetCr0FromResult (
291:     IN UINT32 Result
292:     )
293: {
294:     UINT32 Value;
295:     if ((INT32)Result < 0) {
296:         Value = PPC_CR_LT;
297:     } else if ((INT32)Result > 0) {
298:         Value = PPC_CR_GT;
299:     } else {
300:         Value = PPC_CR_EQ;
301:     }
302:     if (g_PpcContext.Xer & PPC_XER_SO) {
303:         Value |= PPC_CR_SO;
304:     }
305:     PpcSetCrField(0, Value);
306: }
307: 
308: // Perform a signed or unsigned compare of A vs B, storing LT/GT/EQ in field F
309: static VOID
310: PpcDoCompare (
311:     IN UINT32  Field,
312:     IN UINT32  A,
313:     IN UINT32  B,
314:     IN BOOLEAN Signed
315:     )
316: {
317:     UINT32 Value;
318:     if (Signed) {
319:         if ((INT32)A < (INT32)B) {
320:             Value = PPC_CR_LT;
321:         } else if ((INT32)A > (INT32)B) {
322:             Value = PPC_CR_GT;
323:         } else {
324:             Value = PPC_CR_EQ;
325:         }
326:     } else {
327:         if (A < B) {
328:             Value = PPC_CR_LT;
329:         } else if (A > B) {
330:             Value = PPC_CR_GT;
331:         } else {
332:             Value = PPC_CR_EQ;
333:         }
334:     }
335:     if (g_PpcContext.Xer & PPC_XER_SO) {
336:         Value |= PPC_CR_SO;
337:     }
338:     PpcSetCrField(Field, Value);
339: }
340: 
341: // Branch taken helper (BO/BI semantics per the PowerPC ISA)
342: static BOOLEAN
343: PpcBranchTaken (
344:     IN UINT32 Bo,
345:     IN UINT32 Bi
346:     )
347: {
348:     BOOLEAN CtrOk = TRUE;
349:     BOOLEAN CrOk = TRUE;
350: 
351:     if ((Bo & 0x04) == 0) {
352:         // CTR test: decrement, then branch on (CTR == 0) when BO[3] set
353:         g_PpcContext.Ctr--;
354:         CtrOk = (Bo & 0x02) ? (g_PpcContext.Ctr == 0) : (g_PpcContext.Ctr != 0);
355:     }
356:     if ((Bo & 0x10) == 0) {
357:         // CR test: branch on the selected condition bit
358:         UINT32 Bit = (g_PpcContext.Cr >> (31 - Bi)) & 1;
359:         CrOk = (Bo & 0x08) ? (Bit != 0) : (Bit == 0);
360:     }
361:     return CtrOk && CrOk;
362: }
363: 
364: // Trap condition evaluation (TO field)
365: static BOOLEAN
366: PpcTrapCondition (
367:     IN UINT32 To,
368:     IN UINT32 A,
369:     IN UINT32 B
370:     )
371: {
372:     if ((To & 0x10) && ((INT32)A < (INT32)B)) return TRUE;
373:     if ((To & 0x08) && ((INT32)A > (INT32)B)) return TRUE;
374:     if ((To & 0x04) && (A == B))              return TRUE;
375:     if ((To & 0x02) && (A < B))               return TRUE;
376:     if ((To & 0x01) && (A > B))               return TRUE;
377:     return FALSE;
378: }
379: 
380: // ---------------------------------------------------------------------------
381: // Integer arithmetic helpers
382: // ---------------------------------------------------------------------------
383: 
384: // result = A + B + Cin; CA/OV optional outputs
385: static UINT32
386: PpcDoAdd (
387:     IN  UINT32  A,
388:     IN  UINT32  B,
389:     IN  UINT32  Cin,
390:     OUT UINT32* CarryOut,
391:     OUT UINT32* Overflow
392:     )
393: {
394:     UINT64 Sum = (UINT64)A + (UINT64)B + Cin;
395:     UINT32  R  = (UINT32)Sum;
396:     if (CarryOut) {
397:         *CarryOut = (UINT32)(Sum >> 32);
398:     }
399:     if (Overflow) {
400:         // OV = sign(A) == sign(B) and sign(R) != sign(A)
401:         *Overflow = ((((A ^ B) >> 31) ^ 1) & ((R ^ A) >> 31)) & 1;
402:     }
403:     return R;
404: }
405: 
406: // result = B - A - Cin (borrow in). CarryOut = 1 if a borrow was produced.
407: static UINT32
408: PpcDoSub (
409:     IN  UINT32  A,
410:     IN  UINT32  B,
411:     IN  UINT32  Cin,
412:     OUT UINT32* CarryOut,
413:     OUT UINT32* Overflow
414:     )
415: {
416:     UINT64 Tmp = (UINT64)A + Cin;
417:     UINT32 R   = (UINT32)(B - Tmp);
418:     if (CarryOut) {
419:         *CarryOut = (Tmp > B) ? 1 : 0;
420:     }
421:     if (Overflow) {
422:         // OV = sign(A) != sign(B) and sign(R) != sign(B)
423:         *Overflow = ((((A ^ B) >> 31) & ((R ^ B) >> 31))) & 1;
424:     }
425:     return R;
426: }
427: 
428: // Rotate left by a 0..31 count
429: static UINT32
430: PpcRotl (
431:     IN UINT32 Value,
432:     IN UINT32 Count
433:     )
434: {
435:     Count &= 0x1F;
436:     if (Count == 0) {
437:         return Value;
438:     }
439:     return (Value << Count) | (Value >> (32 - Count));
440: }
441: 
442: // Rotate-mask for MB..ME (never empty; MB=ME+1 produces all ones)
443: static UINT32
444: PpcRotMask (
445:     IN UINT32 Mb,
446:     IN UINT32 Me
447:     )
448: {
449:     UINT32 Mask = 0;
450:     UINT32 I = Mb;
451:     for (;;) {
452:         Mask |= 0x80000000U >> I;
453:         if (I == Me) {
454:             break;
455:         }
456:         I = (I + 1) & 0x1F;
457:     }
458:     return Mask;
459: }
460: 
461: // Shift right arithmetic with XER[CA] computation
462: static UINT32
463: PpcSraw (
464:     IN  UINT32  Rs,
465:     IN  UINT32  Count,
466:     OUT UINT32* Ca
467:     )
468: {
469:     UINT32 N = Count & 0x1F;
470:     if (Count & 0x20) {
471:         // All bits shifted out
472:         if (Rs & 0x80000000) {
473:             *Ca = (Rs != 0xFFFFFFFF) ? 1 : 0;
474:             return 0xFFFFFFFF;
475:         }
476:         *Ca = 0;
477:         return 0;
478:     }
479:     if (N == 0) {
480:         *Ca = 0;
481:         return Rs;
482:     }
483:     if (Rs & 0x80000000) {
484:         UINT32 LowMask = (N == 32) ? 0xFFFFFFFF : (0xFFFFFFFFU >> (32 - N));
485:         *Ca = (Rs & LowMask) ? 1 : 0;
486:         return (UINT32)((INT32)Rs >> N);
487:     }
488:     *Ca = 0;
489:     return Rs >> N;
490: }
491: 
492: // ---------------------------------------------------------------------------
493: // String (lmw-style) load/store helpers
494: // ---------------------------------------------------------------------------
495: static VOID
496: PpcLoadString (
497:     IN UINT32 Rt,
498:     IN UINT32 Ea,
499:     IN UINT32 Count
500:     )
501: {
502:     UINT32 I, Reg = Rt, Shift = 24;
503:     if (Count == 0) {
504:         Count = 32;
505:     }
506:     for (I = 0; I < Count; I++) {
507:         if (Shift == 24) {
508:             g_PpcContext.Gpr[Reg] = 0;
509:         }
510:         g_PpcContext.Gpr[Reg] |= (UINT32)g_ReadByte(Ea + I) << Shift;
511:         if (Shift == 0) {
512:             Shift = 24;
513:             Reg = (Reg + 1) & 31;
514:         } else {
515:             Shift -= 8;
516:         }
517:     }
518: }
519: 
520: static VOID
521: PpcStoreString (
522:     IN UINT32 Rt,
523:     IN UINT32 Ea,
524:     IN UINT32 Count
525:     )
526: {
527:     UINT32 I, Reg = Rt, Shift = 24;
528:     if (Count == 0) {
529:         Count = 32;
530:     }
531:     for (I = 0; I < Count; I++) {
532:         g_WriteByte(Ea + I, (UINT8)(g_PpcContext.Gpr[Reg] >> Shift));
533:         if (Shift == 0) {
534:             Shift = 24;
535:             Reg = (Reg + 1) & 31;
536:         } else {
537:             Shift -= 8;
538:         }
539:     }
540: }
541: 
542: // ---------------------------------------------------------------------------
543: // Public API
544: // ---------------------------------------------------------------------------
545: 
546: UINT32
547: PpcFetchInstruction (
548:     IN UINT32 Address
549:     )
550: {
551:     return CpuRead32(Address);
552: }
553: 
554: EFI_STATUS
555: PpcExecuteInstruction (
556:     IN  UINT32  Instruction,
557:     IN  UINT32  CurrentAddress,
558:     OUT UINT32* NextAddress
559:     )
560: {
561:     UINT32 w = Instruction;
562:     UINT32 Op = OP(w);
563:     UINT32 Next = CurrentAddress + 4;
564: 
565:     if (NextAddress == NULL) {
566:         return EFI_INVALID_PARAMETER;
567:     }
568: 
569:     switch (Op) {
570:     case 3:  // twi
571:         if (PpcTrapCondition((w >> 21) & 0x1F, g_PpcContext.Gpr[RA(w)], SIMM(w))) {
572:             g_PpcContext.ExceptionPending = PPC_EXCEPTION_TRAP;
573:             g_PpcContext.Srr0 = CurrentAddress;
574:             g_PpcContext.Srr1 = g_PpcContext.Msr;
575:         }
576:         break;
577: 
578:     case 7:  // mulli
579:         g_PpcContext.Gpr[RD(w)] =
580:             (UINT32)((INT64)(INT32)g_PpcContext.Gpr[RA(w)] * (INT64)(INT32)SIMM(w));
581:         break;
582: 
583:     case 8:  // subfic
584:         {
585:             UINT32 Ca;
586:             g_PpcContext.Gpr[RD(w)] = PpcDoSub(g_PpcContext.Gpr[RA(w)], SIMM(w), 0, &Ca, NULL);
587:             PpcSetXerCarry(Ca);
588:         }
589:         break;
590: 
591:     case 10: // cmpli
592:         PpcDoCompare((w >> 23) & 0x7, g_PpcContext.Gpr[RA(w)], UIMM(w), FALSE);
593:         break;
594: 
595:     case 11: // cmpi
596:         PpcDoCompare((w >> 23) & 0x7, g_PpcContext.Gpr[RA(w)], SIMM(w), TRUE);
597:         break;
598: 
599:     case 12: // addic (no Rc)
600:     case 13: // addic.
601:         {
602:             UINT32 Ca;
603:             g_PpcContext.Gpr[RD(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], SIMM(w), 0, &Ca, NULL);
604:             PpcSetXerCarry(Ca);
605:             if (Op == 13) {
606:                 PpcSetCr0FromResult(g_PpcContext.Gpr[RD(w)]);
607:             }
608:         }
609:         break;
610: 
611:     case 14: // addi / li
612:         g_PpcContext.Gpr[RD(w)] =
613:             (RA(w) == 0 ? 0 : g_PpcContext.Gpr[RA(w)]) + SIMM(w);
614:         break;
615: 
616:     case 15: // addis / lis
617:         g_PpcContext.Gpr[RD(w)] =
618:             (RA(w) == 0 ? 0 : g_PpcContext.Gpr[RA(w)]) + (SIMM(w) << 16);
619:         break;
620: 
621:     case 16: // bc / bcl / bca / bcla
622:         if (LK(w)) {
623:             g_PpcContext.Lr = CurrentAddress + 4;
624:         }
625:         if (PpcBranchTaken(BO(w), BI(w))) {
626:             Next = AA(w) ? (UINT32)(INT32)(INT16)((w >> 2) & 0xFFFF) : CurrentAddress + BD(w);
627:         }
628:         break;
629: 
630:     case 17: // sc
631:         g_PpcContext.ExceptionPending = PPC_EXCEPTION_SYSTEM_CALL;
632:         g_PpcContext.Srr0 = CurrentAddress + 4;
633:         g_PpcContext.Srr1 = g_PpcContext.Msr;
634:         break;
635: 
636:     case 18: // b / bl / ba / bla
637:         if (LK(w)) {
638:             g_PpcContext.Lr = CurrentAddress + 4;
639:         }
640:         Next = AA(w) ? LI(w) : CurrentAddress + LI(w);
641:         break;
642: 
643:     case 19: // XL-form
644:         switch (XO10(w)) {
645:         case XO19_MCRF:  // mcrf crfD, crfS
646:             PpcSetCrField((w >> 23) & 0x7, PpcGetCrField((w >> 18) & 0x7));
647:             break;
648: 
649:         case XO19_BCLR:  // blr / blrl / bclr / bclrl
650:             {
651:                 UINT32 Target = g_PpcContext.Lr;
652:                 if (LK(w)) {
653:                     g_PpcContext.Lr = CurrentAddress + 4;
654:                 }
655:                 if (PpcBranchTaken(BO(w), BI(w))) {
656:                     Next = Target;
657:                 }
658:             }
659:             break;
660: 
661:         case XO19_RFI:  // rfi
662:             g_PpcContext.Msr = g_PpcContext.Srr1;
663:             Next = g_PpcContext.Srr0;
664:             break;
665: 
666:         case XO19_ISYNC:  // isync (no-op)
667:             break;
668: 
669:         case XO19_BCCTR:  // bctr / bctrl / bcctr / bcctrl
670:             {
671:                 UINT32 Target = g_PpcContext.Ctr;
672:                 if (LK(w)) {
673:                     g_PpcContext.Lr = CurrentAddress + 4;
674:                 }
675:                 if (PpcBranchTaken(BO(w), BI(w))) {
676:                     Next = Target;
677:                 }
678:             }
679:             break;
680: 
681:         default:
682:             return EFI_UNSUPPORTED;
683:         }
684:         break;
685: 
686:     case 20: // rlwimi
687:         {
688:             UINT32 Mask = PpcRotMask(MB(w), ME(w));
689:             UINT32 R = PpcRotl(g_PpcContext.Gpr[RS(w)], SH(w));
690:             g_PpcContext.Gpr[RA(w)] =
691:                 (g_PpcContext.Gpr[RA(w)] & ~Mask) | (R & Mask);
692:             if (Rc(w)) {
693:                 PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
694:             }
695:         }
696:         break;
697: 
698:     case 21: // rlwinm / slwi / srwi
699:         g_PpcContext.Gpr[RA(w)] =
700:             PpcRotl(g_PpcContext.Gpr[RS(w)], SH(w)) & PpcRotMask(MB(w), ME(w));
701:         if (Rc(w)) {
702:             PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
703:         }
704:         break;
705: 
706:     case 23: // rlwnm
707:         g_PpcContext.Gpr[RA(w)] =
708:             PpcRotl(g_PpcContext.Gpr[RS(w)], g_PpcContext.Gpr[RB(w)]) & PpcRotMask(MB(w), ME(w));
709:         if (Rc(w)) {
710:             PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
711:         }
712:         break;
713: 
714:     case 24: // ori
715:         g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] | UIMM(w);
716:         break;
717: 
718:     case 25: // oris
719:         g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] | (UIMM(w) << 16);
720:         break;
721: 
722:     case 26: // xori
723:         g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] ^ UIMM(w);
724:         break;
725: 
726:     case 27: // xoris
727:         g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] ^ (UIMM(w) << 16);
728:         break;
729: 
730:     case 28: // andi.
731:         g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] & UIMM(w);
732:         PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
733:         break;
734: 
735:     case 29: // andis.
736:         g_PpcContext.Gpr[RA(w)] = g_PpcContext.Gpr[RS(w)] & (UIMM(w) << 16);
737:         PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
738:         break;
739: 
740:     // -------- Loads / stores --------
741:     case 32: // lwz
742:         g_PpcContext.Gpr[RT(w)] = CpuRead32(EaD(w, RA(w)));
743:         break;
744: 
745:     case 33: // lwzu
746:         {
747:             UINT32 Ea = EaD(w, RA(w));
748:             g_PpcContext.Gpr[RT(w)] = CpuRead32(Ea);
749:             g_PpcContext.Gpr[RA(w)] = Ea;
750:         }
751:         break;
752: 
753:     case 34: // lbz
754:         g_PpcContext.Gpr[RT(w)] = g_ReadByte(EaD(w, RA(w)));
755:         break;
756: 
757:     case 35: // lbzu
758:         {
759:             UINT32 Ea = EaD(w, RA(w));
760:             g_PpcContext.Gpr[RT(w)] = g_ReadByte(Ea);
761:             g_PpcContext.Gpr[RA(w)] = Ea;
762:         }
763:         break;
764: 
765:     case 36: // stw
766:         CpuWrite32(EaD(w, RA(w)), g_PpcContext.Gpr[RS(w)]);
767:         break;
768: 
769:     case 37: // stwu
770:         {
771:             UINT32 Ea = EaD(w, RA(w));
772:             CpuWrite32(Ea, g_PpcContext.Gpr[RS(w)]);
773:             g_PpcContext.Gpr[RA(w)] = Ea;
774:         }
775:         break;
776: 
777:     case 38: // stb
778:         g_WriteByte(EaD(w, RA(w)), (UINT8)g_PpcContext.Gpr[RS(w)]);
779:         break;
780: 
781:     case 39: // stbu
782:         {
783:             UINT32 Ea = EaD(w, RA(w));
784:             g_WriteByte(Ea, (UINT8)g_PpcContext.Gpr[RS(w)]);
785:             g_PpcContext.Gpr[RA(w)] = Ea;
786:         }
787:         break;
788: 
789:     case 40: // lhz
790:         g_PpcContext.Gpr[RT(w)] = CpuRead16(EaD(w, RA(w)));
791:         break;
792: 
793:     case 41: // lhzu
794:         {
795:             UINT32 Ea = EaD(w, RA(w));
796:             g_PpcContext.Gpr[RT(w)] = CpuRead16(Ea);
797:             g_PpcContext.Gpr[RA(w)] = Ea;
798:         }
799:         break;
800: 
801:     case 42: // lha
802:         g_PpcContext.Gpr[RT(w)] = (UINT32)(INT32)(INT16)CpuRead16(EaD(w, RA(w)));
803:         break;
804: 
805:     case 43: // lhau
806:         {
807:             UINT32 Ea = EaD(w, RA(w));
808:             g_PpcContext.Gpr[RT(w)] = (UINT32)(INT32)(INT16)CpuRead16(Ea);
809:             g_PpcContext.Gpr[RA(w)] = Ea;
810:         }
811:         break;
812: 
813:     case 44: // sth
814:         CpuWrite16(EaD(w, RA(w)), g_PpcContext.Gpr[RS(w)]);
815:         break;
816: 
817:     case 45: // sthu
818:         {
819:             UINT32 Ea = EaD(w, RA(w));
820:             CpuWrite16(Ea, g_PpcContext.Gpr[RS(w)]);
821:             g_PpcContext.Gpr[RA(w)] = Ea;
822:         }
823:         break;
824: 
825:     case 46: // lmw
826:         {
827:             UINT32 Ea = EaD(w, RA(w));
828:             UINT32 R;
829:             for (R = RT(w); R < 32; R++) {
830:                 g_PpcContext.Gpr[R] = CpuRead32(Ea + (R - RT(w)) * 4);
831:             }
832:         }
833:         break;
834: 
835:     case 47: // stmw
836:         {
837:             UINT32 Ea = EaD(w, RA(w));
838:             UINT32 R;
839:             for (R = RS(w); R < 32; R++) {
840:                 CpuWrite32(Ea + (R - RS(w)) * 4, g_PpcContext.Gpr[R]);
841:             }
842:         }
843:         break;
844: 
845:     // -------- X-form (opcode 31) --------
846:     case 31:
847:         {
848:             UINT32 X = XO10(w);
849:             switch (X) {
850:             case XO_CMP:  // cmp
851:                 PpcDoCompare((w >> 23) & 0x7, g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], TRUE);
852:                 break;
853: 
854:             case XO_CMPL:  // cmpl
855:                 PpcDoCompare((w >> 23) & 0x7, g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], FALSE);
856:                 break;
857: 
858:             case XO_TW:  // tw
859:                 if (PpcTrapCondition((w >> 21) & 0x1F, g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)])) {
860:                     g_PpcContext.ExceptionPending = PPC_EXCEPTION_TRAP;
861:                     g_PpcContext.Srr0 = CurrentAddress;
862:                     g_PpcContext.Srr1 = g_PpcContext.Msr;
863:                 }
864:                 break;
865: 
866:             case XO_SUBFC | 0x200:  // with-OE form
867:             case XO_SUBFC:  // subfc / subfco / subfc. / subfco.
868:                 {
869:                     UINT32 Ca, Ov;
870:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], 1, &Ca, &Ov);
871:                     PpcSetXerCarry(Ca);
872:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
873:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
874:                 }
875:                 break;
876: 
877:             case XO_ADDC | 0x200:  // with-OE form
878:             case XO_ADDC:  // addc / addco / addc. / addco.
879:                 {
880:                     UINT32 Ca, Ov;
881:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], 0, &Ca, &Ov);
882:                     PpcSetXerCarry(Ca);
883:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
884:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
885:                 }
886:                 break;
887: 
888:             case XO_MULHWU:  // mulhwu / mulhwu.
889:                 g_PpcContext.Gpr[RT(w)] = (UINT32)(((UINT64)g_PpcContext.Gpr[RA(w)] * g_PpcContext.Gpr[RB(w)]) >> 32);
890:                 if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
891:                 break;
892: 
893:             case XO_LWARX:  // lwarx (no reservation tracking)
894:                 g_PpcContext.Gpr[RT(w)] = CpuRead32(EaX(w, RA(w), RB(w)));
895:                 break;
896: 
897:             case XO_LWZX:  // lwzx
898:                 g_PpcContext.Gpr[RT(w)] = CpuRead32(EaX(w, RA(w), RB(w)));
899:                 break;
900: 
901:             case XO_SLW:  // slw / slw.
902:                 g_PpcContext.Gpr[RA(w)] =
903:                     (g_PpcContext.Gpr[RB(w)] & 0x20) ? 0 : (g_PpcContext.Gpr[RS(w)] << (g_PpcContext.Gpr[RB(w)] & 0x1F));
904:                 if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
905:                 break;
906: 
907:             case XO_CNTLZW:  // cntlzw / cntlzw.
908:                 {
909:                     UINT32 V = g_PpcContext.Gpr[RS(w)], C = 0;
910:                     while ((V & 0x80000000) == 0 && C < 32) { V <<= 1; C++; }
911:                     g_PpcContext.Gpr[RA(w)] = C;
912:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
913:                 }
914:                 break;
915: 
916:             case XO_AND:  // and / and. / ando / ando.
917:                 {
918:                     UINT32 R = g_PpcContext.Gpr[RS(w)] & g_PpcContext.Gpr[RB(w)];
919:                     g_PpcContext.Gpr[RA(w)] = R;
920:                     if (Rc(w)) PpcSetCr0FromResult(R);
921:                 }
922:                 break;
923: 
924:             case XO_SUBF | 0x200:  // with-OE form
925:             case XO_SUBF:  // subf / subf. / subfo / subfo.
926:                 {
927:                     UINT32 Ov;
928:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], 1, NULL, &Ov);
929:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
930:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
931:                 }
932:                 break;
933: 
934:             case XO_DCBST:  // dcbst (no-op)
935:                 break;
936: 
937:             case XO_ANDC:  // andc / andc. / andco / andco.
938:                 {
939:                     UINT32 R = g_PpcContext.Gpr[RS(w)] & ~g_PpcContext.Gpr[RB(w)];
940:                     g_PpcContext.Gpr[RA(w)] = R;
941:                     if (Rc(w)) PpcSetCr0FromResult(R);
942:                 }
943:                 break;
944: 
945:             case XO_MULHW:  // mulhw / mulhw.
946:                 {
947:                     INT64 P = (INT64)(INT32)g_PpcContext.Gpr[RA(w)] * (INT64)(INT32)g_PpcContext.Gpr[RB(w)];
948:                     g_PpcContext.Gpr[RT(w)] = (UINT32)(INT32)(P >> 32);
949:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
950:                 }
951:                 break;
952: 
953:             case XO_TLBIEL:  // tlbiel (no-op)
954:                 break;
955: 
956:             case XO_MFMSR:  // mfmsr
957:                 g_PpcContext.Gpr[RT(w)] = g_PpcContext.Msr;
958:                 break;
959: 
960:             case XO_DCBF:  // dcbf (no-op)
961:                 break;
962: 
963:             case XO_LBZX:  // lbzx
964:                 g_PpcContext.Gpr[RT(w)] = g_ReadByte(EaX(w, RA(w), RB(w)));
965:                 break;
966: 
967:             case XO_NEG | 0x200:  // with-OE form
968:             case XO_NEG:  // neg / neg. / nego / nego.
969:                 {
970:                     UINT32 Ca, Ov;
971:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], 0, 1, &Ca, &Ov);
972:                     PpcSetXerCarry(Ca);
973:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
974:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
975:                 }
976:                 break;
977: 
978:             case XO_LBZUX:  // lbzux
979:                 {
980:                     UINT32 Ea = EaX(w, RA(w), RB(w));
981:                     g_PpcContext.Gpr[RT(w)] = g_ReadByte(Ea);
982:                     g_PpcContext.Gpr[RA(w)] = Ea;
983:                 }
984:                 break;
985: 
986:             case XO_NOR:  // nor / nor. / noro / noro.
987:                 {
988:                     UINT32 R = ~(g_PpcContext.Gpr[RS(w)] | g_PpcContext.Gpr[RB(w)]);
989:                     g_PpcContext.Gpr[RA(w)] = R;
990:                     if (Rc(w)) PpcSetCr0FromResult(R);
991:                 }
992:                 break;
993: 
994:             case XO_SUBFE | 0x200:  // with-OE form
995:             case XO_SUBFE:  // subfe / subfe. / subfeo / subfeo.
996:                 {
997:                     UINT32 Ca, Ov;
998:                     UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
999:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], Cin, &Ca, &Ov);
1000:                     PpcSetXerCarry(Ca);
1001:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
1002:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
1003:                 }
1004:                 break;
1005: 
1006:             case XO_ADDE | 0x200:  // with-OE form
1007:             case XO_ADDE:  // adde / adde. / addeo / addeo.
1008:                 {
1009:                     UINT32 Ca, Ov;
1010:                     UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
1011:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], Cin, &Ca, &Ov);
1012:                     PpcSetXerCarry(Ca);
1013:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
1014:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
1015:                 }
1016:                 break;
1017: 
1018:             case XO_MTCRF:  // mtcrf
1019:                 {
1020:                     UINT32 Mask = (w >> 12) & 0xFF;
1021:                     UINT32 Rs = g_PpcContext.Gpr[RS(w)];
1022:                     UINT32 I;
1023:                     for (I = 0; I < 8; I++) {
1024:                         if (Mask & (0x80 >> I)) {
1025:                             PpcSetCrField(I, (Rs >> (28 - I * 4)) & 0xF);
1026:                         }
1027:                     }
1028:                 }
1029:                 break;
1030: 
1031:             case XO_MTMSR:  // mtmsr
1032:                 g_PpcContext.Msr = g_PpcContext.Gpr[RS(w)];
1033:                 break;
1034: 
1035:             case XO_STWCX_:  // stwcx. (no reservation tracking)
1036:                 CpuWrite32(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
1037:                 PpcSetCrField(0, PPC_CR_EQ);
1038:                 break;
1039: 
1040:             case XO_STWX:  // stwx
1041:                 CpuWrite32(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
1042:                 break;
1043: 
1044:             case XO_STWUX:  // stwux
1045:                 {
1046:                     UINT32 Ea = EaX(w, RA(w), RB(w));
1047:                     CpuWrite32(Ea, g_PpcContext.Gpr[RS(w)]);
1048:                     g_PpcContext.Gpr[RA(w)] = Ea;
1049:                 }
1050:                 break;
1051: 
1052:             case XO_SUBFZE | 0x200:  // with-OE form
1053:             case XO_SUBFZE:  // subfze / subfze. / subfzeo / subfzeo.
1054:                 {
1055:                     UINT32 Ca, Ov;
1056:                     UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
1057:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], 0, Cin, &Ca, &Ov);
1058:                     PpcSetXerCarry(Ca);
1059:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
1060:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
1061:                 }
1062:                 break;
1063: 
1064:             case XO_ADDZE | 0x200:  // with-OE form
1065:             case XO_ADDZE:  // addze / addze. / addzeo / addzeo.
1066:                 {
1067:                     UINT32 Ca, Ov;
1068:                     UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
1069:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], 0, Cin, &Ca, &Ov);
1070:                     PpcSetXerCarry(Ca);
1071:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
1072:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
1073:                 }
1074:                 break;
1075: 
1076:             case XO_STBX:  // stbx
1077:                 g_WriteByte(EaX(w, RA(w), RB(w)), (UINT8)g_PpcContext.Gpr[RS(w)]);
1078:                 break;
1079: 
1080:             case XO_SUBFME | 0x200:  // with-OE form
1081:             case XO_SUBFME:  // subfme / subfme. / subfmeo / subfmeo.
1082:                 {
1083:                     UINT32 Ca, Ov;
1084:                     UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
1085:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(~g_PpcContext.Gpr[RA(w)], 0xFFFFFFFF, Cin, &Ca, &Ov);
1086:                     PpcSetXerCarry(Ca);
1087:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
1088:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
1089:                 }
1090:                 break;
1091: 
1092:             case XO_ADDME | 0x200:  // with-OE form
1093:             case XO_ADDME:  // addme / addme. / addmeo / addmeo.
1094:                 {
1095:                     UINT32 Ca, Ov;
1096:                     UINT32 Cin = (g_PpcContext.Xer & PPC_XER_CA) ? 1 : 0;
1097:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], 0xFFFFFFFF, Cin, &Ca, &Ov);
1098:                     PpcSetXerCarry(Ca);
1099:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
1100:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
1101:                 }
1102:                 break;
1103: 
1104:             case XO_MULLW | 0x200:  // with-OE form
1105:             case XO_MULLW:  // mullw / mullw. / mullwo / mullwo.
1106:                 {
1107:                     INT64 P = (INT64)(INT32)g_PpcContext.Gpr[RA(w)] * (INT64)(INT32)g_PpcContext.Gpr[RB(w)];
1108:                     g_PpcContext.Gpr[RT(w)] = (UINT32)P;
1109:                     if ((w >> 10) & 1) {
1110:                         PpcSetXerOverflow(((P >> 32) != 0) && ((P >> 32) != -1));
1111:                     }
1112:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
1113:                 }
1114:                 break;
1115: 
1116:             case XO_MTSRIN:  // mtsrin
1117:                 g_PpcContext.Spr[g_PpcContext.Gpr[RB(w)] & 0xF] = g_PpcContext.Gpr[RS(w)];
1118:                 break;
1119: 
1120:             case XO_DCBTST:  // dcbtst (no-op)
1121:                 break;
1122: 
1123:             case XO_STBUX:  // stbux
1124:                 {
1125:                     UINT32 Ea = EaX(w, RA(w), RB(w));
1126:                     g_WriteByte(Ea, (UINT8)g_PpcContext.Gpr[RS(w)]);
1127:                     g_PpcContext.Gpr[RA(w)] = Ea;
1128:                 }
1129:                 break;
1130: 
1131:             case XO_ADD | 0x200:  // with-OE form
1132:             case XO_ADD:  // add / add. / addo / addo.
1133:                 {
1134:                     UINT32 Ov;
1135:                     g_PpcContext.Gpr[RT(w)] = PpcDoAdd(g_PpcContext.Gpr[RA(w)], g_PpcContext.Gpr[RB(w)], 0, NULL, &Ov);
1136:                     if ((w >> 10) & 1) PpcSetXerOverflow(Ov);
1137:                     if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
1138:                 }
1139:                 break;
1140: 
1141:             case XO_DCBT:  // dcbt (no-op)
1142:                 break;
1143: 
1144:             case XO_LHZX:  // lhzx
1145:                 g_PpcContext.Gpr[RT(w)] = CpuRead16(EaX(w, RA(w), RB(w)));
1146:                 break;
1147: 
1148:             case XO_EQV:  // eqv / eqv. / eqvo / eqvo.
1149:                 {
1150:                     UINT32 R = ~(g_PpcContext.Gpr[RS(w)] ^ g_PpcContext.Gpr[RB(w)]);
1151:                     g_PpcContext.Gpr[RA(w)] = R;
1152:                     if (Rc(w)) PpcSetCr0FromResult(R);
1153:                 }
1154:                 break;
1155: 
1156:             case XO_TLBIE:  // tlbie (no-op)
1157:                 break;
1158: 
1159:             case XO_LHZUX:  // lhzux
1160:                 {
1161:                     UINT32 Ea = EaX(w, RA(w), RB(w));
1162:                     g_PpcContext.Gpr[RT(w)] = CpuRead16(Ea);
1163:                     g_PpcContext.Gpr[RA(w)] = Ea;
1164:                 }
1165:                 break;
1166: 
1167:             case XO_XOR:  // xor / xor. / xoro / xoro.
1168:                 {
1169:                     UINT32 R = g_PpcContext.Gpr[RS(w)] ^ g_PpcContext.Gpr[RB(w)];
1170:                     g_PpcContext.Gpr[RA(w)] = R;
1171:                     if (Rc(w)) PpcSetCr0FromResult(R);
1172:                 }
1173:                 break;
1174: 
1175:             case XO_MFSPR:  // mfspr
1176:                 {
1177:                     UINT32 SprNum = SPR(w);
1178:                     UINT32 Value;
1179:                     switch (SprNum) {
1180:                     case SPR_XER:  Value = g_PpcContext.Xer; break;
1181:                     case SPR_LR:   Value = g_PpcContext.Lr; break;
1182:                     case SPR_CTR:  Value = g_PpcContext.Ctr; break;
1183:                     case SPR_PVR:  Value = 0x00010000; break;   // fabricated PVR
1184:                     default:       Value = g_PpcContext.Spr[SprNum]; break;
1185:                     }
1186:                     g_PpcContext.Gpr[RT(w)] = Value;
1187:                 }
1188:                 break;
1189: 
1190:             case XO_LHAX:  // lhax
1191:                 g_PpcContext.Gpr[RT(w)] = (UINT32)(INT32)(INT16)CpuRead16(EaX(w, RA(w), RB(w)));
1192:                 break;
1193: 
1194:             case XO_TLBIA:  // tlbia (no-op)
1195:                 break;
1196: 
1197:             case XO_MFTB:  // mftb
1198:                 g_PpcContext.Gpr[RT(w)] = 0;
1199:                 break;
1200: 
1201:             case XO_LHAUX:  // lhaux
1202:                 {
1203:                     UINT32 Ea = EaX(w, RA(w), RB(w));
1204:                     g_PpcContext.Gpr[RT(w)] = (UINT32)(INT32)(INT16)CpuRead16(Ea);
1205:                     g_PpcContext.Gpr[RA(w)] = Ea;
1206:                 }
1207:                 break;
1208: 
1209:             case XO_STHX:  // sthx
1210:                 CpuWrite16(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
1211:                 break;
1212: 
1213:             case XO_ORC:  // orc / orc. / orco / orco.
1214:                 {
1215:                     UINT32 R = g_PpcContext.Gpr[RS(w)] | ~g_PpcContext.Gpr[RB(w)];
1216:                     g_PpcContext.Gpr[RA(w)] = R;
1217:                     if (Rc(w)) PpcSetCr0FromResult(R);
1218:                 }
1219:                 break;
1220: 
1221:             case XO_STHUX:  // sthux
1222:                 {
1223:                     UINT32 Ea = EaX(w, RA(w), RB(w));
1224:                     CpuWrite16(Ea, g_PpcContext.Gpr[RS(w)]);
1225:                     g_PpcContext.Gpr[RA(w)] = Ea;
1226:                 }
1227:                 break;
1228: 
1229:             case XO_OR:  // or / or. / oro / oro.
1230:                 {
1231:                     UINT32 R = g_PpcContext.Gpr[RS(w)] | g_PpcContext.Gpr[RB(w)];
1232:                     g_PpcContext.Gpr[RA(w)] = R;
1233:                     if (Rc(w)) PpcSetCr0FromResult(R);
1234:                 }
1235:                 break;
1236: 
1237:             case XO_DIVWU:  // divwu / divwu. / divwuo / divwuo.
1238:                 g_PpcContext.Gpr[RT(w)] =
1239:                     (g_PpcContext.Gpr[RB(w)] == 0) ? 0 : (g_PpcContext.Gpr[RA(w)] / g_PpcContext.Gpr[RB(w)]);
1240:                 if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
1241:                 break;
1242: 
1243:             case XO_MTSPR:  // mtspr
1244:                 {
1245:                     UINT32 SprNum = SPR(w);
1246:                     UINT32 Value = g_PpcContext.Gpr[RS(w)];
1247:                     switch (SprNum) {
1248:                     case SPR_XER:  g_PpcContext.Xer = Value; break;
1249:                     case SPR_LR:   g_PpcContext.Lr = Value; break;
1250:                     case SPR_CTR:  g_PpcContext.Ctr = Value; break;
1251:                     default:       g_PpcContext.Spr[SprNum] = Value; break;
1252:                     }
1253:                 }
1254:                 break;
1255: 
1256:             case XO_DCBI:  // dcbi (no-op)
1257:                 break;
1258: 
1259:             case XO_NAND:  // nand / nand. / nando / nando.
1260:                 {
1261:                     UINT32 R = ~(g_PpcContext.Gpr[RS(w)] & g_PpcContext.Gpr[RB(w)]);
1262:                     g_PpcContext.Gpr[RA(w)] = R;
1263:                     if (Rc(w)) PpcSetCr0FromResult(R);
1264:                 }
1265:                 break;
1266: 
1267:             case XO_DIVW | 0x200:  // with-OE form
1268:             case XO_DIVW:  // divw / divw. / divwo / divwo.
1269:                 if (g_PpcContext.Gpr[RB(w)] == 0) {
1270:                     g_PpcContext.Gpr[RT(w)] = 0;
1271:                 } else if (g_PpcContext.Gpr[RA(w)] == 0x80000000 &&
1272:                            g_PpcContext.Gpr[RB(w)] == 0xFFFFFFFF) {
1273:                     g_PpcContext.Gpr[RT(w)] = 0x80000000;
1274:                     if ((w >> 10) & 1) PpcSetXerOverflow(1);
1275:                 } else {
1276:                     g_PpcContext.Gpr[RT(w)] =
1277:                         (UINT32)((INT32)g_PpcContext.Gpr[RA(w)] / (INT32)g_PpcContext.Gpr[RB(w)]);
1278:                 }
1279:                 if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RT(w)]);
1280:                 break;
1281: 
1282:             case XO_MCRXR:  // mcrxr
1283:                 PpcSetCrField((w >> 23) & 0x7, (g_PpcContext.Xer >> 28) & 0xF);
1284:                 g_PpcContext.Xer &= 0x0FFFFFFF;
1285:                 break;
1286: 
1287:             case XO_LSWX:  // lswx
1288:                 PpcLoadString(RT(w), EaX(w, RA(w), RB(w)), (g_PpcContext.Xer >> 25) & 0x7F);
1289:                 break;
1290: 
1291:             case XO_LWBRX:  // lwbrx
1292:                 g_PpcContext.Gpr[RT(w)] = CpuRead32Rev(EaX(w, RA(w), RB(w)));
1293:                 break;
1294: 
1295:             case XO_SRW:  // srw / srw.
1296:                 g_PpcContext.Gpr[RA(w)] =
1297:                     (g_PpcContext.Gpr[RB(w)] & 0x20) ? 0 : (g_PpcContext.Gpr[RS(w)] >> (g_PpcContext.Gpr[RB(w)] & 0x1F));
1298:                 if (Rc(w)) PpcSetCr0FromResult(g_PpcContext.Gpr[RA(w)]);
1299:                 break;
1300: 
1301:             case XO_MFSR:  // mfsr
1302:                 g_PpcContext.Gpr[RT(w)] = g_PpcContext.Spr[(w >> 16) & 0xF];
1303:                 break;
1304: 
1305:             case XO_LSWI:  // lswi
1306:                 PpcLoadString(RT(w), EaD(w, RA(w)), (w >> 11) & 0x1F);
1307:                 break;
1308: 
1309:             case XO_SYNC:  // sync (no-op)
1310:                 break;
1311: 
1312:             case XO_MTSR:  // mtsr
1313:                 g_PpcContext.Spr[(w >> 16) & 0xF] = g_PpcContext.Gpr[RS(w)];
1314:                 break;
1315: 
1316:             case XO_MFSRIN:  // mfsrin
1317:                 g_PpcContext.Gpr[RT(w)] = g_PpcContext.Spr[g_PpcContext.Gpr[RB(w)] & 0xF];
1318:                 break;
1319: 
1320:             case XO_STSWX:  // stswx
1321:                 PpcStoreString(RS(w), EaX(w, RA(w), RB(w)), (g_PpcContext.Xer >> 25) & 0x7F);
1322:                 break;
1323: 
1324:             case XO_STWBRX:  // stwbrx
1325:                 CpuWrite32Rev(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
1326:                 break;
1327: 
1328:             case XO_STSWI:  // stswi
1329:                 PpcStoreString(RS(w), EaD(w, RA(w)), (w >> 11) & 0x1F);
1330:                 break;
1331: 
1332:             case XO_LHBRX:  // lhbrx
1333:                 g_PpcContext.Gpr[RT(w)] = CpuRead16Rev(EaX(w, RA(w), RB(w)));
1334:                 break;
1335: 
1336:             case XO_SRAW:  // sraw / sraw.
1337:                 {
1338:                     UINT32 Ca;
1339:                     UINT32 R = PpcSraw(g_PpcContext.Gpr[RS(w)], g_PpcContext.Gpr[RB(w)], &Ca);
1340:                     g_PpcContext.Gpr[RA(w)] = R;
1341:                     PpcSetXerCarry(Ca);
1342:                     if (Rc(w)) PpcSetCr0FromResult(R);
1343:                 }
1344:                 break;
1345: 
1346:             case XO_SRAWI:  // srawi / srawi.
1347:                 {
1348:                     UINT32 Ca;
1349:                     UINT32 R = PpcSraw(g_PpcContext.Gpr[RS(w)], SH(w), &Ca);
1350:                     g_PpcContext.Gpr[RA(w)] = R;
1351:                     PpcSetXerCarry(Ca);
1352:                     if (Rc(w)) PpcSetCr0FromResult(R);
1353:                 }
1354:                 break;
1355: 
1356:             case XO_EIEIO:  // eieio (no-op)
1357:                 break;
1358: 
1359:             case XO_STHBRX:  // sthbrx
1360:                 CpuWrite16Rev(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
1361:                 break;
1362: 
1363:             case XO_ICBI:  // icbi (no-op)
1364:                 break;
1365: 
1366:             case XO_DCBZ:  // dcbz: zero a 32-byte cache line
1367:                 {
1368:                     UINT32 Ea = EaX(w, RA(w), RB(w)) & ~0x1F;
1369:                     UINT32 I;
1370:                     for (I = 0; I < 32; I++) {
1371:                         g_WriteByte(Ea + I, 0);
1372:                     }
1373:                 }
1374:                 break;
1375: 
1376:             default:
1377:                 return EFI_UNSUPPORTED;
1378:             }
1379:         }
1380:         break;
1381: 
1382:     default:
1383:         return EFI_UNSUPPORTED;
1384:     }
1385: 
1386:     *NextAddress = Next;
1387:     return EFI_SUCCESS;
1388: }
1389: 
1390: EFI_STATUS
1391: PpcExecuteBlock (
1392:     IN  UINT32* InstructionStream,
1393:     IN  UINTN   MaxInstructions,
1394:     OUT UINTN*  ExecutedCount
1395:     )
1396: {
1397:     UINTN Executed = 0;
1398: 
1399:     if (InstructionStream == NULL || ExecutedCount == NULL) {
1400:         return EFI_INVALID_PARAMETER;
1401:     }
1402: 
1403:     g_PpcContext.Pc = (UINT32)(UINTN)InstructionStream;
1404:     g_PpcContext.ExceptionPending = 0;
1405: 
1406:     while (Executed < MaxInstructions) {
1407:         UINT32 Instr = CpuRead32(g_PpcContext.Pc);
1408:         UINT32 Next;
1409:         EFI_STATUS Status = PpcExecuteInstruction(Instr, g_PpcContext.Pc, &Next);
1410:         Executed++;
1411:         if (EFI_ERROR(Status)) {
1412:             *ExecutedCount = Executed;
1413:             return Status;
1414:         }
1415:         g_PpcContext.Pc = Next;
1416:         if (g_PpcContext.ExceptionPending != 0) {
1417:             break;
1418:         }
1419:     }
1420: 
1421:     *ExecutedCount = Executed;
1422:     return EFI_SUCCESS;
1423: }
1424: 
1425: // ---------------------------------------------------------------------------
1426: // Instruction decode to a short mnemonic
1427: // ---------------------------------------------------------------------------
1428: static const CHAR16* g_DOpcodeNames[] = {
1429:     L"reserved", L"reserved", L"reserved", L"twi",      L"reserved", L"reserved",
1430:     L"reserved", L"mulli",    L"subfic",   L"reserved", L"cmpli",    L"cmpi",
1431:     L"addic",    L"addic.",   L"addi",     L"addis",    L"bc",       L"sc",
1432:     L"b",        L"XL-form",  L"rlwimi",   L"rlwinm",   L"reserved", L"rlwnm",
1433:     L"ori",      L"oris",     L"xori",     L"xoris",    L"andi.",    L"andis.",
1434:     L"reserved", L"X-form",   L"lwz",      L"lwzu",     L"lbz",      L"lbzu",
1435:     L"stw",      L"stwu",     L"stb",      L"stbu",     L"lhz",      L"lhzu",
1436:     L"lha",      L"lhau",     L"sth",      L"sthu",     L"lmw",      L"stmw"
1437: };
1438: 
1439: EFI_STATUS
1440: PpcDecodeInstruction (
1441:     IN  UINT32  Instruction,
1442:     OUT CHAR16* Buffer,
1443:     IN  UINTN   BufferSize
1444:     )
1445: {
1446:     UINT32 w = Instruction;
1447:     UINT32 Op = OP(w);
1448:     const CHAR16* Name;
1449: 
1450:     if (Buffer == NULL || BufferSize < 16) {
1451:         return EFI_INVALID_PARAMETER;
1452:     }
1453: 
1454:     Buffer[0] = 0;
1455:     if (Op < 48) {
1456:         Name = g_DOpcodeNames[Op];
1457:     } else {
1458:         Name = L"fpu/reserved";
1459:     }
1460: 
1461:     if (Op == 31) {
1462:         switch (XO(w)) {
1463:         case XO_ADD:       Name = L"add";   break;
1464:         case XO_SUBF:      Name = L"subf";  break;
1465:         case XO_AND:       Name = L"and";   break;
1466:         case XO_OR:        Name = L"or";    break;
1467:         case XO_XOR:       Name = L"xor";   break;
1468:         case XO_NOR:       Name = L"nor";   break;
1469:         case XO_CMP:       Name = L"cmp";   break;
1470:         case XO_CMPL:      Name = L"cmpl";  break;
1471:         case XO_MFSPR:     Name = L"mfspr"; break;
1472:         case XO_MTSPR:     Name = L"mtspr"; break;
1473:         case XO_SLW:       Name = L"slw";   break;
1474:         case XO_SRW:       Name = L"srw";   break;
1475:         case XO_SRAW:      Name = L"sraw";  break;
1476:         case XO_SRAWI:     Name = L"srawi"; break;
1477:         case XO_CNTLZW:    Name = L"cntlzw";break;
1478:         case XO_MULLW:     Name = L"mullw"; break;
1479:         case XO_MULHW:     Name = L"mulhw"; break;
1480:         case XO_MULHWU:    Name = L"mulhwu";break;
1481:         case XO_DIVW:      Name = L"divw";  break;
1482:         case XO_DIVWU:     Name = L"divwu"; break;
1483:         case XO_NEG:       Name = L"neg";   break;
1484:         case XO_LWZX:      Name = L"lwzx";  break;
1485:         case XO_LBZX:      Name = L"lbzx";  break;
1486:         case XO_LHZX:      Name = L"lhzx";  break;
1487:         case XO_LHAX:      Name = L"lhax";  break;
1488:         case XO_LWBRX:     Name = L"lwbrx"; break;
1489:         case XO_LHBRX:     Name = L"lhbrx"; break;
1490:         case XO_STWX:      Name = L"stwx";  break;
1491:         case XO_STBX:      Name = L"stbx";  break;
1492:         case XO_STHX:      Name = L"sthx";  break;
1493:         case XO_SYNC:      Name = L"sync";  break;
1494:         case XO_EIEIO:     Name = L"eieio"; break;
1495:         default:           Name = L"X-op";  break;
1496:         }
1497:     } else if (Op == 19) {
1498:         switch (XO10(w)) {
1499:         case XO19_BCLR:    Name = L"bclr";  break;
1500:         case XO19_BCCTR:   Name = L"bcctr"; break;
1501:         case XO19_RFI:     Name = L"rfi";   break;
1502:         case XO19_ISYNC:   Name = L"isync"; break;
1503:         case XO19_MCRF:    Name = L"mcrf";  break;
1504:         default:           Name = L"XL-op"; break;
1505:         }
1506:     }
1507: 
1508:     StrnCpy(Buffer, Name, BufferSize / sizeof(CHAR16) - 1);
1509:     return EFI_SUCCESS;
1510: }

(End of file - total 1510 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 3.5s)

_Thinking:_

Now let me look at the remaining source files: the hardware abstraction, platform uefi_interface, and utils debug. Let me also check the build instructions and the Makefile.

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\hardware\\abstraction.h"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\hardware\abstraction.h</path>
<type>file</type>
<content>
1: #ifndef __PPC_HARDWARE_ABSTRACTION_H__
2: #define __PPC_HARDWARE_ABSTRACTION_H__
3: 
4: #include <efi.h>
5: 
6: // Graphics modes
7: #define PPC_GRAPHICS_MODE_DEFAULT   0
8: #define PPC_GRAPHICS_MODE_640x480   1
9: #define PPC_GRAPHICS_MODE_800x600   2
10: #define PPC_GRAPHICS_MODE_1024x768  3
11: #define PPC_GRAPHICS_MODE_1280x1024 4
12: 
13: // Guest-visible framebuffer. Classic Macs expose the display as a fixed
14: // memory window the CPU writes to; this emulator carves that window out of
15: // guest RAM so the interpreter's load/store path targets it directly.
16: #define PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE 0x18000000
17: 
18: // Guest-visible audio device. UEFI defines no standard audio output protocol,
19: // so the emulator exposes an emulated device: a fixed ring buffer inside guest
20: // RAM that guest code fills with big-endian 16-bit stereo PCM samples. The
21: // host side tracks write/play cursors and reports buffer state.
22: #define PPC_AUDIO_BUFFER_GUEST_BASE 0x18800000
23: #define PPC_AUDIO_BUFFER_SIZE       0x2000
24: #define PPC_AUDIO_SAMPLE_RATE       44100
25: #define PPC_AUDIO_CHANNELS          2
26: #define PPC_AUDIO_BITS_PER_SAMPLE   16
27: 
28: // Audio device information
29: typedef struct {
30:     UINT32  GuestBase;    // Guest-visible address of the PCM ring buffer
31:     VOID*   HostBuffer;   // Host pointer to the ring buffer
32:     UINT64  Size;         // Buffer size in bytes
33:     UINT32  SampleRate;   // Sample rate in Hz
34:     UINT32  Channels;     // Channel count
35:     UINT32  BitsPerSample;
36:     UINT64  WriteCursor;  // Samples written by the guest
37:     UINT64  PlayCursor;   // Samples played by the host
38: } PPC_AUDIO_INFO;
39: 
40: // Framebuffer information
41: typedef struct {
42:     UINTN  Width;              // Visible width in pixels
43:     UINTN  Height;             // Visible height in pixels
44:     UINTN  Pitch;              // Bytes per scanline in the guest framebuffer
45:     UINTN  BitsPerPixel;       // 32 for the emulated VRAM layout
46:     UINT32 GuestBase;          // Guest-visible address of the framebuffer
47:     VOID*  HostBuffer;         // Host pointer to the framebuffer region
48:     UINT64 HostBufferSize;     // Size of the guest framebuffer region
49:     EFI_PHYSICAL_ADDRESS GopFrameBuffer; // Real GOP framebuffer base
50:     UINTN  GopPitch;           // GOP framebuffer bytes per scanline
51:     UINT32 PixelFormat;        // GOP pixel format (EFI_GRAPHICS_PIXEL_FORMAT)
52: } PPC_FRAMEBUFFER_INFO;
53: 
54: // Hardware states
55: typedef enum {
56:     PPC_HARDWARE_GRAPHICS_MODE,
57:     PPC_HARDWARE_AUDIO_ENABLE,
58:     PPC_HARDWARE_STORAGE_DEVICES,
59:     PPC_HARDWARE_NETWORK_INTERFACES
60: } PPC_HARDWARE_STATE;
61: 
62: // Hardware information structure
63: typedef struct {
64:     BOOLEAN IsInitialized;
65:     UINT32  GraphicsMode;
66:     UINT32  AudioEnabled;
67:     UINT32  StorageDevices;
68:     UINT32  NetworkInterfaces;
69: } PPC_HARDWARE_INFO;
70: 
71: /**
72:   Initialize PowerPC hardware abstraction layer
73:   @retval EFI_STATUS
74: **/
75: EFI_STATUS
76: EFIAPI
77: PpcInitializeHardwareAbstraction (
78:     VOID
79:     );
80: 
81: /**
82:   Initialize graphics subsystem
83:   @param[in] Width      Display width
84:   @param[in] Height     Display height
85:   @param[in] ColorDepth Color depth in bits
86:   @retval EFI_STATUS
87: **/
88: EFI_STATUS
89: EFIAPI
90: PpcInitializeGraphics (
91:     IN UINT32 Width,
92:     IN UINT32 Height,
93:     IN UINT32 ColorDepth
94:     );
95: 
96: /**
97:   Initialize audio subsystem
98:   @retval EFI_STATUS
99: **/
100: EFI_STATUS
101: EFIAPI
102: PpcInitializeAudio (
103:     VOID
104:     );
105: 
106: /**
107:   Get information about the emulated audio device (PCM ring buffer).
108:   @param[out] Info  Audio device information structure
109:   @retval EFI_STATUS
110: **/
111: EFI_STATUS
112: EFIAPI
113: PpcAudioGetBufferInfo (
114:     OUT PPC_AUDIO_INFO* Info
115:     );
116: 
117: /**
118:   Write a 16-bit big-endian PCM sample into the ring buffer (host-side helper;
119:   guest code writes through the interpreter's guest RAM path instead).
120:   @param[in] SampleIndex  Sample index (frame index, not byte offset)
121:   @param[in] Value        16-bit sample value (native byte order)
122:   @retval EFI_STATUS
123: **/
124: EFI_STATUS
125: EFIAPI
126: PpcAudioWriteSample (
127:     IN UINT32  SampleIndex,
128:     IN UINT16  Value
129:     );
130: 
131: /**
132:   Read a 16-bit PCM sample back from the ring buffer (host-side helper).
133:   @param[in]  SampleIndex  Sample index to read
134:   @param[out] Value        Pointer to store the sample value
135:   @retval EFI_STATUS
136: **/
137: EFI_STATUS
138: EFIAPI
139: PpcAudioReadSample (
140:     IN  UINT32  SampleIndex,
141:     OUT UINT16* Value
142:     );
143: 
144: /**
145:   Simulate playback: advance the play cursor by the given number of samples.
146:   @param[in] SampleCount  Number of samples to consume
147:   @retval EFI_STATUS
148: **/
149: EFI_STATUS
150: EFIAPI
151: PpcAudioAdvancePlayback (
152:     IN UINT32 SampleCount
153:     );
154: 
155: /**
156:   Reset the audio ring buffer and cursors.
157:   @retval EFI_STATUS
158: **/
159: EFI_STATUS
160: EFIAPI
161: PpcAudioReset (
162:     VOID
163:     );
164: 
165: /**
166:   Initialize storage subsystem
167:   @param[in] DeviceCount Number of storage devices to initialize
168:   @retval EFI_STATUS
169: **/
170: EFI_STATUS
171: EFIAPI
172: PpcInitializeStorage (
173:     IN UINT32 DeviceCount
174:     );
175: 
176: /**
177:   Initialize network subsystem
178:   @param[in] InterfaceCount Number of network interfaces to initialize
179:   @retval EFI_STATUS
180: **/
181: EFI_STATUS
182: EFIAPI
183: PpcInitializeNetwork (
184:     IN UINT32 InterfaceCount
185:     );
186: 
187: /**
188:   Get hardware information
189:   @param[out] HardwareInfo Pointer to structure to fill with hardware info
190:   @retval EFI_STATUS
191: **/
192: EFI_STATUS
193: EFIAPI
194: PpcGetHardwareInfo (
195:     OUT PPC_HARDWARE_INFO* HardwareInfo
196:     );
197: 
198: /**
199:   Handle hardware interrupts
200:   @param[in] InterruptNumber Number of interrupt to handle
201:   @retval EFI_STATUS
202: **/
203: EFI_STATUS
204: EFIAPI
205: PpcHandleHardwareInterrupt (
206:     IN UINT32 InterruptNumber
207:     );
208: 
209: /**
210:   Set hardware state parameters
211:   @param[in] State Hardware state to set
212:   @param[in] Value Value to set the state to
213:   @retval EFI_STATUS
214: **/
215: EFI_STATUS
216: EFIAPI
217: PpcSetHardwareState (
218:     IN PPC_HARDWARE_STATE State,
219:     IN UINT32             Value
220:     );
221: 
222: /**
223:   Get hardware state parameters
224:   @param[in]  State Hardware state to get
225:   @param[out] Value Pointer to store the state value
226:   @retval EFI_STATUS
227: **/
228: EFI_STATUS
229: EFIAPI
230: PpcGetHardwareState (
231:     IN  PPC_HARDWARE_STATE State,
232:     OUT UINT32*            Value
233:     );
234: 
235: /**
236:   Get information about the emulated framebuffer. The framebuffer lives
237:   inside guest RAM at PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE; guest code writes
238:   big-endian 0xRRGGBB00 pixels there and the host blits the window to the
239:   real GOP framebuffer.
240:   @param[out] Info  Framebuffer information structure
241:   @retval EFI_STATUS
242: **/
243: EFI_STATUS
244: EFIAPI
245: PpcGetFrameBufferInfo (
246:     OUT PPC_FRAMEBUFFER_INFO* Info
247:     );
248: 
249: /**
250:   Copy the guest framebuffer window to the real GOP framebuffer, converting
251:   from big-endian RGB pixels to the GOP native pixel layout.
252:   @retval EFI_SUCCESS          Blit completed
253:   @retval EFI_NOT_READY        Graphics not initialized
254: **/
255: EFI_STATUS
256: EFIAPI
257: PpcGraphicsBlitToDisplay (
258:     VOID
259:     );
260: 
261: /**
262:   Fill the guest framebuffer with a solid color (host-side helper).
263:   @param[in] Color  Big-endian 0xRRGGBB00 color value
264:   @retval EFI_STATUS
265: **/
266: EFI_STATUS
267: EFIAPI
268: PpcGraphicsClear (
269:     IN UINT32 Color
270:     );
271: 
272: /**
273:   Write a single pixel into the guest framebuffer (host-side helper).
274:   @param[in] X      X coordinate
275:   @param[in] Y      Y coordinate
276:   @param[in] Color  Big-endian 0xRRGGBB00 color value
277:   @retval EFI_STATUS
278: **/
279: EFI_STATUS
280: EFIAPI
281: PpcGraphicsSetPixel (
282:     IN UINT32 X,
283:     IN UINT32 Y,
284:     IN UINT32 Color
285:     );
286: 
287: // Network information
288: #define PPC_MAX_NETWORK_INTERFACES 4
289: 
290: // Per-interface state snapshot
291: typedef struct {
292:     UINT8   MacAddress[6];      // Station MAC address
293:     BOOLEAN MediaPresent;       // Physical link / media present
294:     BOOLEAN TransmitTestPassed; // A real test frame was transmitted
295: } PPC_NETWORK_IFACE_INFO;
296: 
297: typedef struct {
298:     UINTN   InterfaceCount;     // Number of SNP interfaces initialized
299:     BOOLEAN MediaPresent;       // Primary interface: link/media present
300:     UINT8   MacAddress[6];      // Primary interface station MAC address
301:     UINTN   MaxPacketSize;      // Primary interface max frame size
302:     UINTN   IfType;             // Primary interface type (1 = Ethernet)
303:     UINTN   HwAddressSize;      // Primary interface hardware address size
304:     UINTN   MediaHeaderSize;    // Primary interface MAC header size
305:     UINTN   State;              // Primary interface SNP state
306:     BOOLEAN TransmitTestPassed; // Primary interface transmit result
307:     PPC_NETWORK_IFACE_INFO Interfaces[PPC_MAX_NETWORK_INTERFACES];
308: } PPC_NETWORK_INFO;
309: 
310: // Block device information
311: typedef struct {
312:     UINTN    DeviceCount;      // Number of BLOCK_IO devices found
313:     UINTN    SelectedIndex;    // Device used for the sector read test
314:     UINT32   MediaId;          // Media ID of the selected device
315:     UINT32   BlockSize;        // Bytes per block of the selected device
316:     EFI_LBA  LastBlock;        // Last valid block of the selected device
317:     BOOLEAN  ReadOnly;         // Selected device is read-only
318:     BOOLEAN  Removable;        // Selected device is removable
319:     BOOLEAN  MarkerFound;      // First sector contained the test marker
320:     UINT32   FirstSector0;     // First word (big-endian) of the test sector
321: } PPC_BLOCK_IO_INFO;
322: 
323: /**
324:   Enumerate every UEFI Block I/O protocol instance (whole disks and
325:   partitions) and report real media geometry.
326:   @param[in] DeviceCount  Minimum devices to expect (unused if not found)
327:   @retval EFI_STATUS
328: **/
329: EFI_STATUS
330: EFIAPI
331: PpcInitializeBlockIo (
332:     IN UINT32 DeviceCount
333:     );
334: 
335: /**
336:   Get information about the block devices found by PpcInitializeBlockIo.
337:   @param[out] Info  Block device information structure
338:   @retval EFI_STATUS
339: **/
340: EFI_STATUS
341: EFIAPI
342: PpcGetBlockIoInfo (
343:     OUT PPC_BLOCK_IO_INFO* Info
344:     );
345: 
346: /**
347:   Read a single block (sector) from a real UEFI Block I/O device via
348:   ReadBlocks. The buffer must be large enough for BlockSize bytes.
349:   @param[in]  Index   Index into the enumerated device list
350:   @param[in]  Lba     Logical block address to read
351:   @param[in]  BufferSize  Size of the buffer in bytes
352:   @param[out] Buffer  Buffer to receive the block
353:   @retval EFI_STATUS
354: **/
355: EFI_STATUS
356: EFIAPI
357: PpcReadDiskBlock (
358:     IN  UINTN   Index,
359:     IN  EFI_LBA Lba,
360:     IN  UINTN   BufferSize,
361:     OUT VOID*   Buffer
362:     );
363: 
364: /**
365:   Get information about the real UEFI Simple Network Protocol interfaces.
366:   @param[out] Info  Network information structure
367:   @retval EFI_STATUS
368: **/
369: EFI_STATUS
370: EFIAPI
371: PpcGetNetworkInfo (
372:     OUT PPC_NETWORK_INFO* Info
373:     );
374: 
375: /**
376:   Draw a filled rectangle into the guest framebuffer (host-side helper).
377:   @param[in] X      Top-left X coordinate
378:   @param[in] Y      Top-left Y coordinate
379:   @param[in] Width  Rectangle width
380:   @param[in] Height Rectangle height
381:   @param[in] Color  Big-endian 0xRRGGBB00 color value
382:   @retval EFI_STATUS
383: **/
384: EFI_STATUS
385: EFIAPI
386: PpcGraphicsDrawRect (
387:     IN UINT32 X,
388:     IN UINT32 Y,
389:     IN UINT32 Width,
390:     IN UINT32 Height,
391:     IN UINT32 Color
392:     );
393: 
394: #endif // __PPC_HARDWARE_ABSTRACTION_H__

(End of file - total 394 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\platform\\uefi_interface.h"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\platform\uefi_interface.h</path>
<type>file</type>
<content>
1: #ifndef __PPC_UEFI_INTERFACE_H__
2: #define __PPC_UEFI_INTERFACE_H__
3: 
4: #include <efi.h>
5: 
6: /**
7:   Initialize PowerPC UEFI interface
8:   @param[in] ImageHandle   Handle of the loaded image
9:   @param[in] SystemTable   Pointer to the EFI system table
10:   @retval EFI_STATUS
11: **/
12: EFI_STATUS
13: EFIAPI
14: PpcInitializeUefiInterface (
15:     IN EFI_HANDLE ImageHandle,
16:     IN EFI_SYSTEM_TABLE* SystemTable
17:     );
18: 
19: /**
20:   Get the EFI system table pointer
21:   @param[out] SystemTable Pointer to store system table pointer
22:   @retval EFI_STATUS
23: **/
24: EFI_STATUS
25: EFIAPI
26: PpcGetSystemTable (
27:     OUT EFI_SYSTEM_TABLE** SystemTable
28:     );
29: 
30: /**
31:   Get the image handle
32:   @param[out] ImageHandle Pointer to store image handle
33:   @retval EFI_STATUS
34: **/
35: EFI_STATUS
36: EFIAPI
37: PpcGetImageHandle (
38:     OUT EFI_HANDLE* ImageHandle
39:     );
40: 
41: /**
42:   Allocate memory from UEFI pool
43:   @param[in]  PoolType Type of pool allocation
44:   @param[in]  Size     Size of memory to allocate
45:   @param[out] Buffer   Pointer to store allocated buffer address
46:   @retval EFI_STATUS
47: **/
48: EFI_STATUS
49: EFIAPI
50: PpcAllocatePool (
51:     IN  EFI_MEMORY_TYPE PoolType,
52:     IN  UINTN Size,
53:     OUT VOID** Buffer
54:     );
55: 
56: /**
57:   Free memory from UEFI pool
58:   @param[in] Buffer Address of buffer to free
59:   @retval EFI_STATUS
60: **/
61: EFI_STATUS
62: EFIAPI
63: PpcFreePool (
64:     IN VOID* Buffer
65:     );
66: 
67: /**
68:   Get the UEFI memory map
69:   @param[in,out] MemoryMapSize   Size of memory map buffer (in/out)
70:   @param[out]    MemoryMap       Pointer to memory map buffer
71:   @param[out]    MapKey          Pointer to store memory map key
72:   @param[out]    DescriptorSize  Pointer to store descriptor size
73:   @param[out]    DescriptorVersion Pointer to store descriptor version
74:   @retval EFI_STATUS
75: **/
76: EFI_STATUS
77: EFIAPI
78: PpcGetMemoryMap (
79:     IN OUT UINTN* MemoryMapSize,
80:     OUT EFI_MEMORY_DESCRIPTOR* MemoryMap,
81:     OUT UINTN* MapKey,
82:     OUT UINTN* DescriptorSize,
83:     OUT UINT32* DescriptorVersion
84:     );
85: 
86: /**
87:   Output string to UEFI console
88:   @param[in] String String to output
89:   @retval EFI_STATUS
90: **/
91: EFI_STATUS
92: EFIAPI
93: PpcOutputString (
94:     IN CHAR16* String
95:     );
96: 
97: /**
98:   Get a UEFI variable
99:   @param[in]     VariableName   Name of the variable
100:   @param[in]     VendorGuid     Vendor GUID of the variable
101:   @param[out]    Attributes     Pointer to store variable attributes
102:   @param[in,out] DataSize       Size of data buffer (in/out)
103:   @param[out]    Data           Pointer to buffer to store data
104:   @retval EFI_STATUS
105: **/
106: EFI_STATUS
107: EFIAPI
108: PpcGetVariable (
109:     IN     CHAR16* VariableName,
110:     IN     EFI_GUID* VendorGuid,
111:     OUT    UINT32* Attributes,
112:     IN OUT UINTN* DataSize,
113:     OUT    VOID* Data
114:     );
115: 
116: /**
117:   Set a UEFI variable
118:   @param[in] VariableName   Name of the variable
119:   @param[in] VendorGuid     Vendor GUID of the variable
120:   @param[in] Attributes     Attributes for the variable
121:   @param[in] DataSize       Size of data to set
122:   @param[in] Data           Pointer to data to set
123:   @retval EFI_STATUS
124: **/
125: EFI_STATUS
126: EFIAPI
127: PpcSetVariable (
128:     IN     CHAR16* VariableName,
129:     IN     EFI_GUID* VendorGuid,
130:     IN     UINT32 Attributes,
131:     IN     UINTN DataSize,
132:     IN     VOID* Data
133:     );
134: 
135: /**
136:   Get the Simple File System protocol for a device handle
137:   @param[out] FileSystem    Pointer to store the file system interface
138:   @param[in]  DeviceHandle  Device handle to query (NULL = boot device)
139:   @retval EFI_STATUS
140: **/
141: EFI_STATUS
142: EFIAPI
143: PpcGetFileSystem (
144:     OUT EFI_FILE_IO_INTERFACE** FileSystem,
145:     IN  EFI_HANDLE              DeviceHandle
146:     );
147: 
148: /**
149:   Load a file from a file system into a pool buffer (real UEFI file I/O)
150:   @param[in]  FileSystem   Simple File System protocol instance
151:   @param[in]  FileName     Path of the file to load
152:   @param[out] FileBuffer   Allocated buffer with file contents
153:   @param[out] FileSize     Size of the loaded file in bytes
154:   @retval EFI_STATUS
155: **/
156: EFI_STATUS
157: EFIAPI
158: PpcLoadFile (
159:     IN  EFI_FILE_IO_INTERFACE* FileSystem,
160:     IN  CHAR16*                FileName,
161:     OUT VOID**                 FileBuffer,
162:     OUT UINTN*                 FileSize
163:     );
164: 
165: /**
166:   Get the handle of the device this image was booted from
167:   @param[out] DeviceHandle Pointer to store the boot device handle
168:   @retval EFI_STATUS
169: **/
170: EFI_STATUS
171: EFIAPI
172: PpcGetBootDevice (
173:     OUT EFI_HANDLE* DeviceHandle
174:     );
175: 
176: /**
177:   Get system information from UEFI (real memory map walk)
178:   @param[out] SystemTable Pointer to the EFI system table
179:   @param[out] TotalMemory Total physical RAM reported by firmware
180:   @param[out] FreeMemory  Usable (conventional/boot services) memory
181:   @retval EFI_STATUS
182: **/
183: EFI_STATUS
184: EFIAPI
185: PpcGetSystemInformation (
186:     OUT EFI_SYSTEM_TABLE** SystemTable,
187:     OUT UINT64* TotalMemory,
188:     OUT UINT64* FreeMemory
189:     );
190: 
191: /**
192:   Reset the system via the UEFI runtime reset service
193:   @param[in] ResetType   Type of reset to perform
194:   @param[in] StatusCode  Status code for the reset
195:   @param[in] DataSize    Size of ResetData
196:   @param[in] ResetData   Optional data passed to the reset service
197:   @retval EFI_STATUS
198: **/
199: EFI_STATUS
200: EFIAPI
201: PpcResetSystem (
202:     IN EFI_RESET_TYPE ResetType,
203:     IN EFI_STATUS     StatusCode,
204:     IN UINTN          DataSize,
205:     IN CHAR16*        ResetData OPTIONAL
206:     );
207: 
208: #endif // __PPC_UEFI_INTERFACE_H__

(End of file - total 208 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\utils\\debug.h"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\utils\debug.h</path>
<type>file</type>
<content>
1: #ifndef __PPC_DEBUG_H__
2: #define __PPC_DEBUG_H__
3: 
4: #include <efi.h>
5: 
6: // Debug log levels
7: #define PPC_DEBUG_LEVEL_NONE    0
8: #define PPC_DEBUG_LEVEL_ERROR   1
9: #define PPC_DEBUG_LEVEL_WARNING 2
10: #define PPC_DEBUG_LEVEL_INFO    3
11: #define PPC_DEBUG_LEVEL_DEBUG   4
12: 
13: /**
14:   Initialize PowerPC debug system
15:   @param[in] LogLevel     Log level to use (0-4)
16:   @param[in] LogToFile    Whether to log to file as well
17:   @param[in] LogFilePath  Path to log file (if logging to file)
18:   @retval EFI_STATUS
19: **/
20: EFI_STATUS
21: EFIAPI
22: PpcInitializeDebug (
23:     IN UINT32 LogLevel,
24:     IN BOOLEAN LogToFile,
25:     IN CHAR16* LogFilePath
26:     );
27: 
28: /**
29:   Print a debug message
30:   @param[in] Level   Debug level of the message
31:   @param[in] Message Message to print
32:   @retval EFI_STATUS
33: **/
34: EFI_STATUS
35: EFIAPI
36: PpcDebugPrint (
37:     IN UINT32 Level,
38:     IN CHAR16* Message
39:     );
40: 
41: /**
42:   Print debug data in hexadecimal format
43:   @param[in] Level   Debug level of the message
44:   @param[in] Prefix  Prefix string to display before hex data
45:   @param[in] Data    Pointer to data to print
46:   @param[in] DataSize Size of data in bytes
47:   @retval EFI_STATUS
48: **/
49: EFI_STATUS
50: EFIAPI
51: PpcDebugPrintHex (
52:     IN UINT32 Level,
53:     IN CHAR16* Prefix,
54:     IN UINT8* Data,
55:     IN UINTN DataSize
56:     );
57: 
58: /**
59:   Print a PowerPC instruction
60:   @param[in] Level      Debug level of the message
61:   @param[in] Address    Address of the instruction
62:   @param[in] Instruction The instruction value to print
63:   @retval EFI_STATUS
64: **/
65: EFI_STATUS
66: EFIAPI
67: PpcDebugPrintInstruction (
68:     IN UINT32 Level,
69:     IN UINT32 Address,
70:     IN UINT32 Instruction
71:     );
72: 
73: /**
74:   Set the debug log level
75:   @param[in] Level New log level to use
76:   @retval EFI_STATUS
77: **/
78: EFI_STATUS
79: EFIAPI
80: PpcDebugSetLogLevel (
81:     IN UINT32 Level
82:     );
83: 
84: /**
85:   Get the current debug log level
86:   @param[out] Level Pointer to store current log level
87:   @retval EFI_STATUS
88: **/
89: EFI_STATUS
90: EFIAPI
91: PpcDebugGetLogLevel (
92:     OUT UINT32* Level
93:     );
94: 
95: /**
96:   Write a message to boot.log on the boot volume via real UEFI file I/O.
97:   @param[in] Message  Message to write
98:   @retval EFI_STATUS
99: **/
100: EFI_STATUS
101: EFIAPI
102: PpcDebugLogToFile (
103:     IN CHAR16* Message
104:     );
105: 
106: /**
107:   Start a performance timer (real UEFI monotonic counter)
108:   @param[out] StartTime  Timestamp at start
109:   @retval EFI_STATUS
110: **/
111: EFI_STATUS
112: EFIAPI
113: PpcDebugStartTimer (
114:     OUT UINT64* StartTime
115:     );
116: 
117: /**
118:   Stop a performance timer and return the elapsed time
119:   @param[in]  StartTime  Timestamp when the timer was started
120:   @param[out] ElapsedTime  Elapsed counts since start
121:   @retval EFI_STATUS
122: **/
123: EFI_STATUS
124: EFIAPI
125: PpcDebugStopTimer (
126:     IN  UINT64 StartTime,
127:     OUT UINT64* ElapsedTime
128:     );
129: 
130: #endif // __PPC_DEBUG_H__

(End of file - total 130 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 14.3s)

_Thinking:_

Now I have a very good picture of the project. Let me understand the state:

- Phase 1 (Research) ✓
- Phase 2 (Architecture) - partial
- Phase 3 (Core Implementation) ✓
- Phase 4 (Emulation Components) ✓
- Phase 5 (Boot Process) - NOT STARTED:
  - [ ] Create bootloader for Mac OS (load ROM image, set up guest memory map)
  - [ ] Implement system initialization routines
  - [ ] Add support for system files and drivers
  - [ ] Test boot process with various Mac OS versions
- Phase 6 (Testing and Optimization) - not started

The next objective is Phase 5: Boot Process.

Let me look at the environment. The user is on Windows (win32). The project is a UEFI emulator built with GNU-EFI + clang/lld-link (macOS host per README). But we're on Windows. Let me check if there's a build toolchain available and check the build directory.

Let me check the build environment and the Makefile/CMakeLists to understand the build process.

Also let me check what tools are available: clang? lld? Let me look.

The key question is: what's feasible to do here? The project was verified on macOS with QEMU/OVMF. We're on Windows. Let me check whether the build can even be done. Let me look at the Makefile, BUILD_INSTRUCTIONS, CMakeLists.

Let me think about what the next objective should be. Phase 5 "Boot Process" items:
1. Create bootloader for Mac OS (load ROM image, set up guest memory map)
2. Implement system initialization routines
3. Add support for system files and drivers
4. Test boot process with various Mac OS versions

This is a huge undertaking for a real Mac OS. But the practical next step within the project's realistic scope, matching what has been built so far (self-tests, guest RAM execution, kernel load/verify/execute pipeline), would be to:

1. Build a real ROM loader (`PpcLoadSystemRom` is already stubbed to load a file - but no ROM placement into guest memory map)
2. Set up a proper guest memory map (Mac OS classic memory map: ROM at 0xFFF00000 area, low memory globals, etc.)
3. Implement system initialization routines (interrupt vectors, low memory globals, device registers)
4. Add support for system files and drivers

But actually, real classic Mac OS booting requires the actual ROM and system files which we can't distribute. The project should implement the bootloader that *would* load those. 

Given this is a sandboxed emulator development, a reasonable "next objective" that continues the project's trajectory and is testable in QEMU/OVMF would be:

**Phase 5 step 1: Implement a real classic Mac OS-style ROM + kernel boot pipeline:**
- Load a ROM image from the boot volume into the guest memory map at the classic PPC Mac ROM address (e.g., 0xFFF00000).
- Set up a proper guest memory map (RAM, ROM, device MMIO windows for framebuffer/audio, I/O region).
- Implement system initialization routines that set up the CPU context to enter the ROM reset vector with MSR in a boot-ready state.
- Wire up the interpreter so that guest code can run long-running loops (currently PpcExecuteBlock is bounded; need a run-until-halt loop that services devices and handles exceptions).
- Add tests: create a tiny fake ROM image file on the ESP, load it, place it, verify placement and entry.

But this could be a big task. Let me think about what's realistic and valuable.

Actually, let me reconsider. The project's own TODO says Phase 5:
- Create bootloader for Mac OS (load ROM image, set up guest memory map)
- Implement system initialization routines
- Add support for system files and drivers
- Test boot process with various Mac OS versions

The most impactful, verifiable next increment is:

1. **Guest memory map module**: A proper Mac OS classic memory map with RAM region, ROM region (0xFFF00000), and MMIO windows. Currently memory manager only allocates RAM.

2. **ROM loader + placement**: Load ROM file from volume, place at classic PPC ROM base, verify placement.

3. **System init routines**: Configure CPU context (PC=ROM reset vector, MSR, SRR0/SRR1), set up low-memory globals, exception vectors.

4. **Boot ROM image execution test**: A test fixture ROM image that's a small PPC program placed in guest RAM... but ROM would be at 0xFFF00000 which is outside the guest RAM region (0x10000000 base, 256MB → up to 0x20000000). So we'd need to extend the memory map or map ROM into the guest memory region.

Wait, this raises an architectural issue: guest RAM is 256MB at 0x10000000 → 0x20000000. Classic Mac OS PPC ROM is mapped at 0xFFF00000 (4GB - 1MB). The framebuffer is at 0x18000000 (in RAM), audio at 0x18800000.

For a real boot, we'd need to map ROM into guest memory. But the interpreter's guest memory region is a single contiguous allocation. We could add additional regions via the memory manager / interpreter.

Given the constraint that we're on Windows (win32), let me check if the build can even run here. The README says build requires macOS + Homebrew. But maybe clang/lld are available on Windows via other means. Let me check the build directory contents and whether there's a toolchain.

Let me check the build environment first, then decide.

Let me look at Makefile, CMakeLists.txt, BUILD_INSTRUCTIONS.md, and the build directory.

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\Makefile"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\Makefile</path>
<type>file</type>
<content>
1: # EFI-Mac-Emulator build
2: #
3: # Cross-builds a UEFI x86_64 application (EFI-Mac-Emulator.efi) on a macOS host
4: # using a clang/LLVM toolchain targeting PE/COFF, linked with lld-link.
5: # GNU-EFI provides the UEFI headers and a small runtime library.
6: #
7: # Requirements:
8: #   brew install llvm lld binutils
9: #   (binutils is optional; it is only used for the `check` target via objdump)
10: 
11: SHELL   := /bin/zsh
12: 
13: # --- Toolchain discovery (override via make CC=/path/to/clang LLD=/path/to/lld-link) ---
14: LLVM_PREFIX := $(shell brew --prefix llvm 2>/dev/null || echo /opt/homebrew/opt/llvm)
15: LLD_PREFIX  := $(shell brew --prefix lld  2>/dev/null || echo /opt/homebrew/opt/lld)
16: CC      = $(LLVM_PREFIX)/bin/clang
17: LLD     ?= $(LLD_PREFIX)/bin/lld-link
18: OBJDUMP ?= $(LLVM_PREFIX)/bin/llvm-objdump
19: 
20: # --- Layout ---
21: GNUEFI_DIR  := third_party/gnu-efi
22: GNUEFI_URL  := https://git.code.sf.net/p/gnu-efi/code
23: BUILD_DIR   := build
24: OBJ_GNUEFI  := $(BUILD_DIR)/gnuefi
25: OBJ_SRC     := $(BUILD_DIR)/src
26: TARGET      := $(BUILD_DIR)/EFI-Mac-Emulator.efi
27: 
28: # --- Flags ---
29: ARCH       := -target x86_64-pc-win32-coff
30: COMMON     := -mno-red-zone -ffreestanding -fshort-wchar \
31:               -fno-stack-protector -fno-strict-aliasing -funsigned-char
32: CFLAGS     := $(ARCH) $(COMMON) -O2 -I $(GNUEFI_DIR)/inc -I src
33: 
34: # GNU-EFI runtime library sources (mirrors gnu-efi lib/Makefile for x86_64,
35: # minus entry.c/ctors.o (require ELF crt startup) and the .S files).
36: GNUEFI_SRCS := \
37: 	boxdraw smbios console crc data debug dpath \
38: 	error event exit guid hand hw init lock \
39: 	misc pause print sread str cmdline \
40: 	runtime/rtlock runtime/efirtlib runtime/rtstr runtime/vm runtime/rtdata \
41: 	x86_64/initplat x86_64/math x86_64/callwrap
42: 
43: # Application sources
44: APP_SRCS := \
45: 	src/main.c \
46: 	src/cpu/interpreter.c \
47: 	src/cpu/translation_impl.c \
48: 	src/memory/manager_impl.c \
49: 	src/hardware/abstraction_impl.c \
50: 	src/boot/bootloader_impl.c \
51: 	src/utils/debug_impl.c \
52: 	src/platform/uefi_interface_impl.c
53: 
54: GNUEFI_OBJS := $(patsubst %,$(OBJ_GNUEFI)/%.obj,$(GNUEFI_SRCS))
55: APP_OBJS    := $(patsubst src/%,$(OBJ_SRC)/%,$(APP_SRCS:.c=.obj))
56: 
57: GNUEFI_MARK := $(GNUEFI_DIR)/inc/efi.h
58: 
59: .PHONY: all gnuefi check clean
60: 
61: all: $(TARGET)
62: 
63: # --- Acquire GNU-EFI if not present ---
64: $(GNUEFI_MARK):
65: 	@test -d $(GNUEFI_DIR) || git clone --depth 1 $(GNUEFI_URL) $(GNUEFI_DIR)
66: 
67: # --- GNU-EFI runtime library ---
68: $(GNUEFI_OBJS): $(GNUEFI_MARK)
69: $(OBJ_GNUEFI)/%.obj: $(GNUEFI_DIR)/lib/%.c
70: 	@mkdir -p $(dir $@)
71: 	$(CC) $(CFLAGS) -Wno-unknown-pragmas -c $< -o $@
72: 
73: # --- Application objects ---
74: $(OBJ_SRC)/%.obj: src/%.c
75: 	@mkdir -p $(dir $@)
76: 	$(CC) $(CFLAGS) -Wall -Werror -c $< -o $@
77: 
78: # --- Link ---
79: $(TARGET): $(GNUEFI_OBJS) $(APP_OBJS)
80: 	$(LLD) /subsystem:EFI_APPLICATION /nodefaultlib /entry:efi_main /dll \
81: 	    /out:$@ $(APP_OBJS) $(GNUEFI_OBJS)
82: 
83: # --- Verification ---
84: check: $(TARGET)
85: 	$(OBJDUMP) -x $(TARGET) | grep -E "Subsystem|Base Relocation"
86: 
87: clean:
88: 	rm -rf $(BUILD_DIR)

(End of file - total 88 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\BUILD_INSTRUCTIONS.md"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\BUILD_INSTRUCTIONS.md</path>
<type>file</type>
<content>
1: # EFI-Mac-Emulator - Build Instructions
2: 
3: ## Status
4: 
5: **The project builds.** `make` produces a valid PE32+ UEFI application image
6: (`build/EFI-Mac-Emulator.efi`). The application is still pre-alpha — it
7: initializes the emulator scaffolding and prints status, but does not yet
8: translate or execute PowerPC code.
9: 
10: ## Overview
11: 
12: The build cross-compiles from macOS (Apple Silicon or Intel) to an x86_64 UEFI
13: application:
14: 
15: 1. **GNU-EFI** provides the UEFI headers (`efi.h`, `efilib.h`, ...) and a small
16:    runtime library (memory, string, print, pool helpers).
17: 2. **clang** compiles the sources targeting `x86_64-pc-win32-coff` (PE/COFF
18:    object files, Microsoft x64 ABI).
19: 3. **lld-link** links the objects into a PE32+ image with
20:    `Subsystem = EFI_APPLICATION`, entry point `efi_main`.
21: 
22: This is the same approach EDK II's `CLANGPDB` toolchain uses. The classic
23: GNU-EFI `objcopy`/`ld` flow is not used because the host GNU binutils cannot
24: emit the `efi-app-x86_64` BFD target on this setup.
25: 
26: ## Prerequisites
27: 
28: - **macOS** with Homebrew
29: - `brew install llvm lld`
30: - `git` (only needed for the first build, which clones GNU-EFI)
31: 
32: `binutils` is optional (only the `make check` target uses it via
33: `llvm-objdump`, which actually comes with `llvm`).
34: 
35: ## Building
36: 
37: ```bash
38: make
39: ```
40: 
41: On the first run the Makefile clones GNU-EFI into `third_party/gnu-efi/` (it is
42: git-ignored and fetched on demand). Output:
43: 
44: - `build/EFI-Mac-Emulator.efi` — the UEFI application
45: 
46: Verify the image:
47: 
48: ```bash
49: make check
50: ```
51: 
52: `make clean` removes `build/`. Toolchain locations can be overridden on the
53: command line if Homebrew is not at the default prefix:
54: 
55: ```bash
56: make CC=/path/to/clang LLD=/path/to/lld-link
57: ```
58: 
59: ## How the build works
60: 
61: 1. **GNU-EFI runtime library** — the sources under
62:    `third_party/gnu-efi/lib/` are compiled to COFF objects (see
63:    `GNUEFI_SRCS` in the Makefile). `entry.c` and the `.S` startup files are
64:    excluded because they target the ELF/objcopy flow we do not use.
65: 2. **Application sources** — every `src/*/*_impl.c` and `src/main.c` is
66:    compiled with `-Wall -Werror` against GNU-EFI's headers.
67: 3. **Link** — `lld-link /subsystem:EFI_APPLICATION /entry:efi_main /dll`.
68:    The firmware performs the image relocations at load time via the `.reloc`
69:    section, so no crt0 is required. `_fltused` (MS ABI CRT symbol, referenced
70:    by GNU-EFI's print code) is provided in `src/main.c`.
71: 
72: ## Layout notes
73: 
74: The sources use GNU-EFI idioms:
75: 
76: - Includes: `<efi.h>` + `<efilib.h>` (not EDK II's `<Library/...>`).
77: - Entry point: `efi_main(EFI_HANDLE, EFI_SYSTEM_TABLE*)` calling
78:   `InitializeLib()` (not `UefiMain`).
79: - Boot/Runtime tables: `BS`, `ST`, `RT` globals (not `gBS`, `gST`, `gRT`).
80: 
81: ## Testing
82: 
83: The EFI application has been smoke-tested under QEMU + OVMF: it boots, runs
84: all of its initialization (UEFI pool allocation, memory map, loaded-image
85: protocol, console output), prints its status report, and returns cleanly to the
86: firmware.
87: 
88: To reproduce:
89: 
90: ```bash
91: # 1. Get OVMF firmware. There is no Homebrew formula; download the Debian package:
92: mkdir -p /tmp/ovmf && cd /tmp/ovmf
93: curl -sL -o ovmf.deb \
94:   "http://ftp.us.debian.org/debian/pool/main/e/edk2/ovmf_2025.02-8+deb13u1_all.deb"
95: ar x ovmf.deb && tar -xf data.tar.xz ./usr/share/ovmf/OVMF.fd
96: 
97: # 2. Put the app on a FAT "ESP" directory and boot it
98: mkdir -p esp/EFI/BOOT
99: cp build/EFI-Mac-Emulator.efi esp/EFI/BOOT/BOOTX64.EFI
100: qemu-system-x86_64 \
101:   -bios /tmp/ovmf/usr/share/ovmf/OVMF.fd \
102:   -m 512 \
103:   -drive file=fat:rw:esp,format=raw \
104:   -net none \
105:   -serial stdio \
106:   -display none
107: ```
108: 
109: OVMF will boot `\EFI\BOOT\BOOTX64.EFI` and the app's output appears on the
110: serial console (`-serial stdio`). The newest Debian OVMF version can be looked
111: up via the Debian packages site (`https://packages.debian.org/trixie/all/ovmf/download`).
112: 
113: ## Directory Structure
114: 
115: ```
116: EFIMac/
117: ├── src/                    # Source code files
118: │   ├── main.c              # Main entry point (efi_main)
119: │   ├── cpu/                # CPU translation components
120: │   ├── memory/             # Memory management
121: │   ├── hardware/           # Hardware abstraction
122: │   ├── boot/               # Bootloader system
123: │   ├── utils/              # Utility functions
124: │   └── platform/           # UEFI interface
125: ├── third_party/gnu-efi/    # GNU-EFI headers + runtime (git-ignored, auto-cloned)
126: ├── Makefile                # Build configuration
127: ├── README.md               # Project overview
128: ├── ARCHITECTURE.md         # Design notes
129: ├── TODO.md                 # Implementation plan
130: └── USER_GUIDE.md           # User documentation
131: ```
132: 
133: ## Troubleshooting
134: 
135: 1. **`make` fails to clone GNU-EFI**: `git.code.sf.net` may be unreachable;
136:    clone it manually into `third_party/gnu-efi`:
137:    ```bash
138:    git clone --depth 1 https://git.code.sf.net/p/gnu-efi/code third_party/gnu-efi
139:    ```
140: 2. **`clang: error: no such file .../efi.h`**: run `make` from the repo root, or
141:    check that `third_party/gnu-efi/inc/efi.h` exists.
142: 3. **Undefined `_fltused`**: this MS ABI CRT symbol must be defined somewhere in
143:    the link; `src/main.c` already provides it — do not remove it.
144: 4. **Image won't load in firmware**: confirm `make check` reports
145:    `Subsystem = EFI application` and a `Base Relocation Directory`; rename the
146:    file to `BOOTX64.EFI` on a FAT partition.
147: 
148: ## License
149: 
150: This project is licensed under the GNU General Public License, version 3 or
151: later. See the [LICENSE](LICENSE) file for details.

(End of file - total 151 lines)
</content>
```

**Tool: bash**

**Input:**
```json
{
  "command": "Get-ChildItem -Recurse build | Select-Object FullName, Length"
}
```

**Output:**
```

FullName                                                        Length
--------                                                        ------
C:\Users\clayc\Desktop\New folder (2)\EFIMac\build\build              
C:\Users\clayc\Desktop\New folder (2)\EFIMac\build\output             
C:\Users\clayc\Desktop\New folder (2)\EFIMac\build\build\output       



```

---

## Assistant (Build · Big Pickle · 4.5s)

_Thinking:_

The build directory is basically empty. We're on Windows. The build process requires macOS + Homebrew. Let me check what tools are available on this Windows machine: clang, lld-link, make, git, qemu.

Let me check available tools.

**Tool: bash**

**Input:**
```json
{
  "command": "foreach ($t in @('clang','clang-cl','lld-link','ld.lld','llvm-objdump','make','gmake','git','qemu-system-x86_64','cmake','ninja')) { $c = Get-Command $t -ErrorAction SilentlyContinue; if ($c) { Write-Output \"$t => $($c.Source)\" } else { Write-Output \"$t => NOT FOUND\" } }"
}
```

**Output:**
```
clang => NOT FOUND
clang-cl => NOT FOUND
lld-link => NOT FOUND
ld.lld => NOT FOUND
llvm-objdump => NOT FOUND
make => C:\ProgramData\chocolatey\bin\make.exe
gmake => NOT FOUND
git => C:\Program Files\Git\cmd\git.exe
qemu-system-x86_64 => NOT FOUND
cmake => NOT FOUND
ninja => NOT FOUND

```

**Tool: bash**

**Input:**
```json
{
  "command": "git -C \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\" log --all --oneline -5; Write-Output '---'; Test-Path \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\third_party\""
}
```

**Output:**
```
fee7247 KPD - Deleted accidental transcript duplicate.
929987f KPD - Phase 4 implemented and tested. Graphics and Networking should be working.
ec7e6d8 KPD - Phase 3 *actual* completion.
ba8258f KPD - Implementation of interpreter
72713a1 KPD - Successful boot test of build. Updated instructions text.
---
False

```

---

## Assistant (Build · Big Pickle · 5.1s)

_Thinking:_

Interesting. So:
- Windows host
- No clang, no lld-link, no qemu, no cmake, no ninja
- `make` exists (chocolatey)
- git exists
- third_party/gnu-efi does not exist

So we can't build/test on this machine directly. That means my work should focus on writing code that will compile cleanly on the macOS build host, but I can't verify the build here. Hmm.

Wait — let me check whether clang might be available via Visual Studio, or under other paths. Let me check for VS installation, or LLVM in other locations. Let me also check if there's a way to at least syntax-check the code. Actually, without clang we can't even syntax check.

Let me double-check for clang/llvm in common Windows install paths.

**Tool: bash**

**Input:**
```json
{
  "command": "$paths = @('C:\\Program Files\\LLVM\\bin\\clang.exe','C:\\Program Files\\LLVM\\bin\\lld-link.exe','C:\\Program Files\\Microsoft Visual Studio','C:\\Program Files (x86)\\Microsoft Visual Studio','C:\\Program Files\\clang'); foreach ($p in $paths) { Write-Output \"$p => $(Test-Path $p)\" }; Write-Output '---where---'; where.exe clang 2>$null; where.exe lld-link 2>$null"
}
```

**Output:**
```
C:\Program Files\LLVM\bin\clang.exe => False
C:\Program Files\LLVM\bin\lld-link.exe => False
C:\Program Files\Microsoft Visual Studio => False
C:\Program Files (x86)\Microsoft Visual Studio => True
C:\Program Files\clang => False
---where---

```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\hardware\\abstraction_impl.c"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\hardware\abstraction_impl.c</path>
<type>file</type>
<content>
1: #include "abstraction.h"
2: #include <efi.h>
3: #include <efilib.h>
4: #include <efinet.h>
5: #include "memory/manager.h"
6: #include "cpu/translation.h"
7: 
8: // Hardware abstraction context with more complete implementation
9: typedef struct {
10:     BOOLEAN IsInitialized;
11:     UINT32  GraphicsMode;
12:     UINT32  AudioEnabled;
13:     UINT32  StorageDevices;
14:     UINT32  NetworkInterfaces;
15:     UINT32  SerialPorts;
16:     UINT32  IOPorts;
17:     VOID*   VideoBuffer;
18:     UINT64  VideoBufferSize;
19:     BOOLEAN GraphicsInitialized;
20:     BOOLEAN AudioInitialized;
21:     BOOLEAN StorageInitialized;
22:     BOOLEAN NetworkInitialized;
23:     // Guest-visible framebuffer (window carved out of guest RAM)
24:     UINT32 GuestFbBase;
25:     VOID*  GuestFbHost;
26:     UINT64 GuestFbSize;
27:     UINT32 GuestFbWidth;
28:     UINT32 GuestFbHeight;
29:     UINT32 GuestFbPitch;
30: } PPC_HARDWARE_CONTEXT;
31: 
32: // Global hardware context
33: STATIC PPC_HARDWARE_CONTEXT g_HardwareContext = {0};
34: 
35: // Real UEFI protocol instances found by the HAL
36: STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL  *g_Gop              = NULL;
37: STATIC UINT32                         g_FramebufferPitch = 0;
38: STATIC EFI_GRAPHICS_PIXEL_FORMAT      g_PixelFormat      = PixelRedGreenBlueReserved8BitPerColor;
39: STATIC EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **g_FileSystems    = NULL;
40: STATIC UINTN                          g_FileSystemCount  = 0;
41: 
42: // Real network interface state
43: STATIC struct {
44:     EFI_SIMPLE_NETWORK_PROTOCOL *Snp;
45:     BOOLEAN                      Inited;
46: } g_SnpIfaces[PPC_MAX_NETWORK_INTERFACES];
47: STATIC UINTN                          g_SnpIfaceCount   = 0;
48: STATIC EFI_SIMPLE_NETWORK_PROTOCOL   *g_Snp             = NULL;
49: STATIC PPC_NETWORK_INFO               g_NetworkInfo     = {0};
50: STATIC BOOLEAN                        g_NetworkInited   = FALSE;
51: 
52: // Real block device state
53: STATIC EFI_BLOCK_IO_PROTOCOL         **g_BlockDevices    = NULL;
54: STATIC UINTN                          g_BlockDeviceCount = 0;
55: STATIC PPC_BLOCK_IO_INFO              g_BlockIoInfo      = {0};
56: 
57: // Emulated audio device state
58: STATIC PPC_AUDIO_INFO                 g_AudioInfo        = {0};
59: STATIC BOOLEAN                        g_AudioInited      = FALSE;
60: 
61: /**
62:   Fill the real GOP framebuffer with a simple pattern so that the wired
63:   graphics output is visibly non-trivial. Handles both RGB and BGR
64:   pixel layouts.
65: **/
66: STATIC
67: VOID
68: GraphicsFillFramebuffer (
69:     IN UINT32 R,
70:     IN UINT32 G,
71:     IN UINT32 B
72:     )
73: {
74:     UINT32* Pixel = (UINT32*)(UINTN)g_HardwareContext.VideoBuffer;
75:     UINTN   RowBytes = g_FramebufferPitch;
76:     UINTN   Width;
77:     UINTN   Height;
78: 
79:     if (g_Gop == NULL || Pixel == NULL) {
80:         return;
81:     }
82: 
83:     Width  = g_Gop->Mode->Info->HorizontalResolution;
84:     Height = g_Gop->Mode->Info->VerticalResolution;
85: 
86:     // QEMU stdvga exposes PixelBlueGreenRedReserved8BitPerColor; swap the
87:     // byte order otherwise the red/blue channels would appear swapped.
88:     UINT32 Color;
89:     if (g_PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
90:         Color = (R << 24) | (G << 16) | (B << 8);
91:     } else {
92:         Color = (B << 24) | (G << 16) | (R << 8);
93:     }
94: 
95:     UINTN Y;
96:     for (Y = 0; Y < Height; Y++) {
97:         UINT32* Row = (UINT32*)((UINT8*)Pixel + Y * RowBytes);
98:         UINTN X;
99:         for (X = 0; X < Width; X++) {
100:             Row[X] = Color;
101:         }
102:     }
103: }
104: 
105: /**
106:   Locate the Graphics Output Protocol instance. On success g_Gop points at
107:   the protocol and the framebuffer is available through its mode information.
108: **/
109: STATIC
110: EFI_STATUS
111: GraphicsLocateGop (
112:     VOID
113:     )
114: {
115:     EFI_HANDLE* Handles = NULL;
116:     UINTN       Count   = 0;
117:     EFI_STATUS  Status  = BS->LocateHandleBuffer(
118:         ByProtocol,
119:         &GraphicsOutputProtocol,
120:         NULL,
121:         &Count,
122:         &Handles
123:     );
124: 
125:     if (EFI_ERROR(Status) || Count == 0 || Handles == NULL) {
126:         return EFI_NOT_FOUND;
127:     }
128: 
129:     Status = BS->HandleProtocol(Handles[0], &GraphicsOutputProtocol, (VOID**)&g_Gop);
130:     FreePool(Handles);
131:     return Status;
132: }
133: 
134: EFI_STATUS
135: PpcInitializeHardwareAbstraction (
136:     VOID
137:     )
138: {
139:     // Initialize the hardware abstraction context
140:     ZeroMem(&g_HardwareContext, sizeof(g_HardwareContext));
141:     
142:     g_HardwareContext.IsInitialized = TRUE;
143:     g_HardwareContext.GraphicsMode = PPC_GRAPHICS_MODE_DEFAULT;
144:     g_HardwareContext.AudioEnabled = 0;
145:     g_HardwareContext.StorageDevices = 0;
146:     g_HardwareContext.NetworkInterfaces = 0;
147:     g_HardwareContext.SerialPorts = 0;
148:     g_HardwareContext.IOPorts = 0;
149:     g_HardwareContext.VideoBuffer = NULL;
150:     g_HardwareContext.VideoBufferSize = 0;
151:     g_HardwareContext.GraphicsInitialized = FALSE;
152:     g_HardwareContext.AudioInitialized = FALSE;
153:     g_HardwareContext.StorageInitialized = FALSE;
154:     g_HardwareContext.NetworkInitialized = FALSE;
155: 
156:     // Find the real GOP so graphics initialization can use it
157:     GraphicsLocateGop();
158:     
159:     Print(L"PowerPC Hardware Abstraction Layer initialized\n");
160:     
161:     return EFI_SUCCESS;
162: }
163: 
164: EFI_STATUS
165: PpcInitializeGraphics (
166:     IN UINT32 Width,
167:     IN UINT32 Height,
168:     IN UINT32 ColorDepth
169:     )
170: {
171:     if (Width == 0 || Height == 0) {
172:         return EFI_INVALID_PARAMETER;
173:     }
174: 
175:     Print(L"Initializing graphics: %dx%d @ %d bits\n", Width, Height, ColorDepth);
176: 
177:     // Prefer the real UEFI Graphics Output Protocol framebuffer.
178:     if (g_Gop != NULL) {
179:         EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info = g_Gop->Mode->Info;
180: 
181:         // Try to honor the requested resolution: scan the supported modes and
182:         // switch to the first one that matches (fall back to the current mode).
183:         UINT32 Mode;
184:         BOOLEAN Found = FALSE;
185:         for (Mode = 0; Mode < g_Gop->Mode->MaxMode; Mode++) {
186:             UINTN  InfoSize = 0;
187:             EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Candidate = NULL;
188:             EFI_STATUS QueryStatus = g_Gop->QueryMode(g_Gop, Mode, &InfoSize, &Candidate);
189:             if (EFI_ERROR(QueryStatus) || Candidate == NULL) {
190:                 continue;
191:             }
192:             if (Candidate->HorizontalResolution == Width &&
193:                 Candidate->VerticalResolution == Height) {
194:                 EFI_STATUS SetStatus = g_Gop->SetMode(g_Gop, Mode);
195:                 if (!EFI_ERROR(SetStatus)) {
196:                     Found = TRUE;
197:                 }
198:                 FreePool(Candidate);
199:                 break;
200:             }
201:             FreePool(Candidate);
202:         }
203:         if (!Found) {
204:             g_Gop->SetMode(g_Gop, g_Gop->Mode->Mode);
205:         }
206: 
207:         Info = g_Gop->Mode->Info;
208:         g_HardwareContext.VideoBuffer = (VOID*)(UINTN)g_Gop->Mode->FrameBufferBase;
209:         g_HardwareContext.VideoBufferSize = g_Gop->Mode->FrameBufferSize;
210:         g_FramebufferPitch = Info->PixelsPerScanLine * 4;
211:         g_PixelFormat = Info->PixelFormat;
212:         g_HardwareContext.GraphicsMode = (Info->HorizontalResolution << 16) |
213:                                          (Info->VerticalResolution & 0xFFFF);
214:         g_HardwareContext.GraphicsInitialized = TRUE;
215: 
216:         // Carve a guest-visible framebuffer window out of guest RAM. Guest
217:         // code writes big-endian 0xRRGGBB00 pixels here; PpcGraphicsBlitToDisplay
218:         // pushes the window to the real GOP framebuffer.
219:         VOID*  GuestHost   = NULL;
220:         UINT64 GuestBase   = 0;
221:         UINT64 GuestSize   = 0;
222:         EFI_STATUS FbStatus = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);
223:         if (!EFI_ERROR(FbStatus) && GuestHost != NULL) {
224:             UINT32 FbSize = Info->HorizontalResolution * Info->VerticalResolution * 4;
225:             if ((UINT64)(PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE - (UINT32)GuestBase) + FbSize <= GuestSize) {
226:                 g_HardwareContext.GuestFbBase  = PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE;
227:                 g_HardwareContext.GuestFbHost  = (UINT8*)GuestHost +
228:                                                  (PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE - (UINT32)GuestBase);
229:                 g_HardwareContext.GuestFbSize  = FbSize;
230:                 g_HardwareContext.GuestFbWidth = Info->HorizontalResolution;
231:                 g_HardwareContext.GuestFbHeight = Info->VerticalResolution;
232:                 g_HardwareContext.GuestFbPitch = Info->HorizontalResolution * 4;
233:                 ZeroMem(g_HardwareContext.GuestFbHost, FbSize);
234:                 Print(L"Graphics: guest framebuffer at guest 0x%x (host 0x%x, %d bytes)\n",
235:                       g_HardwareContext.GuestFbBase,
236:                       g_HardwareContext.GuestFbHost,
237:                       FbSize);
238:             }
239:         }
240: 
241:         // Make the wiring visible: draw a pattern on the real framebuffer.
242:         GraphicsFillFramebuffer(0x00, 0x40, 0xFF);
243: 
244:         Print(L"Graphics: GOP mode %d, %dx%d, pixel format %d, framebuffer 0x%x "
245:               L"(pitch %d, size %d bytes)\n",
246:               g_Gop->Mode->Mode,
247:               Info->HorizontalResolution,
248:               Info->VerticalResolution,
249:               g_PixelFormat,
250:               g_Gop->Mode->FrameBufferBase,
251:               g_FramebufferPitch,
252:               g_HardwareContext.VideoBufferSize);
253: 
254:         return EFI_SUCCESS;
255:     }
256: 
257:     // No GOP available: fall back to a pool buffer (simulated framebuffer).
258:     g_HardwareContext.GraphicsMode = (Width << 16) | (Height & 0xFFFF);
259:     g_HardwareContext.GraphicsInitialized = TRUE;
260: 
261:     UINT64 BufferSize = Width * Height * (ColorDepth / 8);
262:     EFI_STATUS Status = BS->AllocatePool(EfiBootServicesData, BufferSize, &g_HardwareContext.VideoBuffer);
263:     
264:     if (EFI_ERROR(Status)) {
265:         Print(L"Failed to allocate video buffer: %r\n", Status);
266:         return Status;
267:     }
268:     
269:     g_HardwareContext.VideoBufferSize = BufferSize;
270:     ZeroMem(g_HardwareContext.VideoBuffer, BufferSize);
271:     
272:     Print(L"Graphics initialized (simulated) - buffer at 0x%x (size: %d bytes)\n", 
273:           g_HardwareContext.VideoBuffer, BufferSize);
274:     
275:     return EFI_SUCCESS;
276: }
277: 
278: EFI_STATUS
279: PpcInitializeAudio (
280:     VOID
281:     )
282: {
283:     Print(L"Initializing audio subsystem (emulated PCM ring buffer in guest RAM)\n");
284: 
285:     // UEFI has no standard audio output protocol, so the "device" is a fixed
286:     // ring buffer inside guest RAM that guest code fills with big-endian
287:     // 16-bit stereo PCM samples.
288:     VOID*  GuestHost   = NULL;
289:     UINT64 GuestBase   = 0;
290:     UINT64 GuestSize   = 0;
291:     EFI_STATUS Status = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);
292:     if (EFI_ERROR(Status) || GuestHost == NULL) {
293:         Print(L"Audio subsystem: guest RAM unavailable, falling back to pool buffer\n");
294:         Status = BS->AllocatePool(EfiBootServicesData, PPC_AUDIO_BUFFER_SIZE,
295:                                   (VOID**)&g_AudioInfo.HostBuffer);
296:         if (EFI_ERROR(Status)) {
297:             return Status;
298:         }
299:         g_AudioInfo.GuestBase = 0;
300:     } else {
301:         if ((UINT64)(PPC_AUDIO_BUFFER_GUEST_BASE - (UINT32)GuestBase) + PPC_AUDIO_BUFFER_SIZE > GuestSize) {
302:             Print(L"Audio subsystem: guest RAM too small\n");
303:             return EFI_NOT_READY;
304:         }
305:         g_AudioInfo.GuestBase = PPC_AUDIO_BUFFER_GUEST_BASE;
306:         g_AudioInfo.HostBuffer = (UINT8*)GuestHost +
307:                                  (PPC_AUDIO_BUFFER_GUEST_BASE - (UINT32)GuestBase);
308:     }
309: 
310:     g_AudioInfo.Size = PPC_AUDIO_BUFFER_SIZE;
311:     g_AudioInfo.SampleRate = PPC_AUDIO_SAMPLE_RATE;
312:     g_AudioInfo.Channels = PPC_AUDIO_CHANNELS;
313:     g_AudioInfo.BitsPerSample = PPC_AUDIO_BITS_PER_SAMPLE;
314:     g_AudioInfo.WriteCursor = 0;
315:     g_AudioInfo.PlayCursor = 0;
316:     ZeroMem(g_AudioInfo.HostBuffer, g_AudioInfo.Size);
317: 
318:     g_HardwareContext.AudioEnabled = 1;
319:     g_HardwareContext.AudioInitialized = TRUE;
320:     g_AudioInited = TRUE;
321: 
322:     Print(L"Audio subsystem initialized: guest 0x%x (host 0x%x, %d bytes, %d Hz, %d ch)\n",
323:           g_AudioInfo.GuestBase, g_AudioInfo.HostBuffer,
324:           g_AudioInfo.Size, g_AudioInfo.SampleRate, g_AudioInfo.Channels);
325: 
326:     return EFI_SUCCESS;
327: }
328: 
329: EFI_STATUS
330: PpcAudioGetBufferInfo (
331:     OUT PPC_AUDIO_INFO* Info
332:     )
333: {
334:     if (Info == NULL) {
335:         return EFI_INVALID_PARAMETER;
336:     }
337:     if (!g_AudioInited) {
338:         return EFI_NOT_READY;
339:     }
340:     CopyMem(Info, &g_AudioInfo, sizeof(PPC_AUDIO_INFO));
341:     return EFI_SUCCESS;
342: }
343: 
344: EFI_STATUS
345: PpcAudioWriteSample (
346:     IN UINT32 SampleIndex,
347:     IN UINT16 Value
348:     )
349: {
350:     if (!g_AudioInited || g_AudioInfo.HostBuffer == NULL) {
351:         return EFI_NOT_READY;
352:     }
353:     if ((UINT64)SampleIndex * 2 + 2 > g_AudioInfo.Size) {
354:         return EFI_OUT_OF_RESOURCES;
355:     }
356:     UINT16 Be = (UINT16)(((Value & 0xFF) << 8) | (Value >> 8));
357:     CopyMem((UINT8*)g_AudioInfo.HostBuffer + (UINT64)SampleIndex * 2, &Be, 2);
358:     if (g_AudioInfo.WriteCursor < (UINT64)SampleIndex + 1) {
359:         g_AudioInfo.WriteCursor = (UINT64)SampleIndex + 1;
360:     }
361:     return EFI_SUCCESS;
362: }
363: 
364: EFI_STATUS
365: PpcAudioReadSample (
366:     IN  UINT32  SampleIndex,
367:     OUT UINT16* Value
368:     )
369: {
370:     if (!g_AudioInited || g_AudioInfo.HostBuffer == NULL || Value == NULL) {
371:         return EFI_NOT_READY;
372:     }
373:     if ((UINT64)SampleIndex * 2 + 2 > g_AudioInfo.Size) {
374:         return EFI_OUT_OF_RESOURCES;
375:     }
376:     UINT16 Be;
377:     CopyMem(&Be, (UINT8*)g_AudioInfo.HostBuffer + (UINT64)SampleIndex * 2, 2);
378:     *Value = (UINT16)(((Be & 0xFF) << 8) | (Be >> 8));
379:     return EFI_SUCCESS;
380: }
381: 
382: EFI_STATUS
383: PpcAudioAdvancePlayback (
384:     IN UINT32 SampleCount
385:     )
386: {
387:     if (!g_AudioInited) {
388:         return EFI_NOT_READY;
389:     }
390:     if (g_AudioInfo.PlayCursor + SampleCount > g_AudioInfo.Size / 2) {
391:         return EFI_OUT_OF_RESOURCES;
392:     }
393:     g_AudioInfo.PlayCursor += SampleCount;
394:     return EFI_SUCCESS;
395: }
396: 
397: EFI_STATUS
398: PpcAudioReset (
399:     VOID
400:     )
401: {
402:     if (!g_AudioInited) {
403:         return EFI_NOT_READY;
404:     }
405:     if (g_AudioInfo.HostBuffer != NULL) {
406:         ZeroMem(g_AudioInfo.HostBuffer, g_AudioInfo.Size);
407:     }
408:     g_AudioInfo.WriteCursor = 0;
409:     g_AudioInfo.PlayCursor = 0;
410:     return EFI_SUCCESS;
411: }
412: 
413: EFI_STATUS
414: PpcInitializeStorage (
415:     IN UINT32 DeviceCount
416:     )
417: {
418:     if (DeviceCount == 0) {
419:         return EFI_INVALID_PARAMETER;
420:     }
421: 
422:     Print(L"Initializing storage devices (locating Simple File System volumes)\n");
423: 
424:     // Locate every UEFI Simple File System protocol instance (one per
425:     // mounted partition/volume). This is real UEFI enumeration.
426:     EFI_HANDLE* Handles = NULL;
427:     UINTN       Count   = 0;
428:     EFI_STATUS  Status  = BS->LocateHandleBuffer(
429:         ByProtocol,
430:         &FileSystemProtocol,
431:         NULL,
432:         &Count,
433:         &Handles
434:     );
435: 
436:     if (EFI_ERROR(Status)) {
437:         if (Status == EFI_NOT_FOUND) {
438:             Print(L"No Simple File System volumes found\n");
439:             return EFI_NOT_FOUND;
440:         }
441:         Print(L"Failed to locate file systems: %r\n", Status);
442:         return Status;
443:     }
444: 
445:     if (g_FileSystems != NULL) {
446:         FreePool(g_FileSystems);
447:         g_FileSystems = NULL;
448:     }
449:     g_FileSystemCount = Count;
450:     g_FileSystems = AllocatePool(Count * sizeof(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*));
451:     if (g_FileSystems == NULL) {
452:         FreePool(Handles);
453:         return EFI_OUT_OF_RESOURCES;
454:     }
455: 
456:     UINTN Found = 0;
457:     for (UINTN i = 0; i < Count; i++) {
458:         EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs = NULL;
459:         Status = BS->HandleProtocol(Handles[i], &FileSystemProtocol, (VOID**)&Fs);
460:         if (EFI_ERROR(Status) || Fs == NULL) {
461:             continue;
462:         }
463: 
464:         // Open the volume root to prove the file system is actually usable.
465:         EFI_FILE_HANDLE Root = NULL;
466:         Status = Fs->OpenVolume(Fs, &Root);
467:         if (EFI_ERROR(Status)) {
468:             continue;
469:         }
470:         Root->Close(Root);
471: 
472:         g_FileSystems[Found++] = Fs;
473:         Print(L"  Found volume %d: filesystem handle 0x%x\n", (UINTN)i, Fs);
474:     }
475: 
476:     FreePool(Handles);
477: 
478:     if (Found == 0) {
479:         Print(L"Storage subsystem initialized with 0 usable volumes\n");
480:         g_HardwareContext.StorageDevices = 0;
481:         g_HardwareContext.StorageInitialized = FALSE;
482:         return EFI_NOT_FOUND;
483:     }
484: 
485:     g_HardwareContext.StorageDevices = (UINT32)Found;
486:     g_HardwareContext.StorageInitialized = TRUE;
487: 
488:     Print(L"Storage subsystem initialized with %d usable volumes\n", Found);
489:     
490:     return EFI_SUCCESS;
491: }
492: 
493: EFI_STATUS
494: PpcInitializeNetwork (
495:     IN UINT32 InterfaceCount
496:     )
497: {
498:     if (InterfaceCount == 0) {
499:         return EFI_INVALID_PARAMETER;
500:     }
501: 
502:     Print(L"Initializing network interfaces (locating Simple Network Protocol)\n");
503: 
504:     // Enumerate every real UEFI Simple Network Protocol instance.
505:     EFI_HANDLE* Handles = NULL;
506:     UINTN       Count   = 0;
507:     EFI_STATUS  Status  = BS->LocateHandleBuffer(
508:         ByProtocol,
509:         &SimpleNetworkProtocol,
510:         NULL,
511:         &Count,
512:         &Handles
513:     );
514: 
515:     if (EFI_ERROR(Status)) {
516:         if (Status == EFI_NOT_FOUND) {
517:             Print(L"No network interfaces found\n");
518:             return EFI_NOT_FOUND;
519:         }
520:         Print(L"Failed to locate network interfaces: %r\n", Status);
521:         return Status;
522:     }
523: 
524:     ZeroMem(&g_NetworkInfo, sizeof(g_NetworkInfo));
525:     ZeroMem(g_SnpIfaces, sizeof(g_SnpIfaces));
526:     g_SnpIfaceCount = 0;
527: 
528:     for (UINTN i = 0; i < Count; i++) {
529:         if (g_SnpIfaceCount >= PPC_MAX_NETWORK_INTERFACES) {
530:             break;
531:         }
532: 
533:         EFI_SIMPLE_NETWORK_PROTOCOL* Snp = NULL;
534:         Status = BS->HandleProtocol(Handles[i], &SimpleNetworkProtocol, (VOID**)&Snp);
535:         if (EFI_ERROR(Status) || Snp == NULL) {
536:             continue;
537:         }
538: 
539:         // Bring the adapter up with real SNP Start/Initialize calls.
540:         Status = Snp->Start(Snp);
541:         if (EFI_ERROR(Status)) {
542:             Print(L"  Interface %d: Start failed %r\n", i, Status);
543:             continue;
544:         }
545:         Status = Snp->Initialize(Snp, 0, 0);
546:         if (EFI_ERROR(Status)) {
547:             Print(L"  Interface %d: Initialize failed %r\n", i, Status);
548:             continue;
549:         }
550: 
551:         // Restore the standard receive filter so ordinary traffic is visible.
552:         if (Snp->Mode->ReceiveFilterMask != 0) {
553:             Snp->ReceiveFilters(Snp,
554:                                 EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
555:                                 EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST |
556:                                 EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST,
557:                                 0, FALSE, 0, NULL);
558:         }
559: 
560:         // Record the initialized interface and snapshot its real mode info.
561:         UINTN Slot = g_SnpIfaceCount;
562:         g_SnpIfaces[Slot].Snp = Snp;
563:         g_SnpIfaces[Slot].Inited = TRUE;
564:         g_SnpIfaceCount++;
565: 
566:         EFI_SIMPLE_NETWORK_MODE* Mode = Snp->Mode;
567:         PPC_NETWORK_IFACE_INFO* Iface = &g_NetworkInfo.Interfaces[Slot];
568:         CopyMem(Iface->MacAddress, Mode->CurrentAddress.Addr,
569:                 sizeof(Iface->MacAddress));
570:         Iface->MediaPresent = (Mode->MediaPresentSupported) ? Mode->MediaPresent : TRUE;
571: 
572:         Print(L"  Interface %d: MAC %02x:%02x:%02x:%02x:%02x:%02x, media %s, "
573:               L"iftype %d, maxpkt %d, hdrsize %d, hwaddr %d, state %d\n",
574:               (UINTN)i,
575:               Iface->MacAddress[0], Iface->MacAddress[1],
576:               Iface->MacAddress[2], Iface->MacAddress[3],
577:               Iface->MacAddress[4], Iface->MacAddress[5],
578:               Iface->MediaPresent ? L"present" : L"absent",
579:               Mode->IfType, Mode->MaxPacketSize,
580:               Mode->MediaHeaderSize, Mode->HwAddressSize, Mode->State);
581: 
582:         // Transmit a real test frame (64-byte ARP-style probe, broadcast).
583:         UINT8 Frame[64];
584:         ZeroMem(Frame, sizeof(Frame));
585:         for (UINTN b = 0; b < 6; b++) {
586:             Frame[b] = 0xFF;                       // broadcast dest
587:             Frame[6 + b] = Iface->MacAddress[b];   // our source
588:         }
589:         Frame[12] = 0x08;                          // ethertype
590:         Frame[13] = 0x06;                          // 0x0806 = ARP
591:         Frame[14] = 0x00;                          // ARP htype
592:         Frame[15] = 0x01;
593:         Frame[16] = 0x08;                          // ARP ptype = IP
594:         Frame[17] = 0x00;
595:         Frame[18] = 0x06;                          // hlen, plen
596:         Frame[19] = 0x04;
597:         Frame[20] = 0x00;                          // op = request
598:         Frame[21] = 0x01;
599: 
600:         // The frame already contains a complete MAC header (dest/src/ethertype),
601:         // so call Transmit with HeaderSize == 0 (the driver takes the header
602:         // straight from the buffer).
603:         Status = Snp->Transmit(Snp, 0, sizeof(Frame), Frame, NULL, NULL, NULL);
604:         Iface->TransmitTestPassed = (Status == EFI_SUCCESS);
605:         if (Status != EFI_SUCCESS) {
606:             Print(L"  Interface %d: transmit returned %r\n", (UINTN)i, Status);
607:         }
608: 
609:         // Poll GetStatus briefly to observe the transmit completion.
610:         for (UINTN Poll = 0; Poll < 3; Poll++) {
611:             UINT32 IrqStatus = 0;
612:             VOID*   TxBuf    = NULL;
613:             EFI_STATUS GetStatus = Snp->GetStatus(Snp, &IrqStatus, &TxBuf);
614:             if (GetStatus == EFI_SUCCESS) {
615:                 break;
616:             }
617:         }
618: 
619:         Print(L"  Interface %d: transmit test %s\n",
620:               (UINTN)i,
621:               Iface->TransmitTestPassed ? L"PASS" : L"FAIL");
622:     }
623: 
624:     FreePool(Handles);
625: 
626:     if (g_SnpIfaceCount == 0) {
627:         g_HardwareContext.NetworkInterfaces = 0;
628:         g_HardwareContext.NetworkInitialized = FALSE;
629:         return EFI_NOT_FOUND;
630:     }
631: 
632:     // Primary interface = first initialized interface (compat snapshot).
633:     g_Snp = g_SnpIfaces[0].Snp;
634:     g_NetworkInfo.InterfaceCount = g_SnpIfaceCount;
635:     g_NetworkInfo.MediaPresent = g_NetworkInfo.Interfaces[0].MediaPresent;
636:     g_NetworkInfo.TransmitTestPassed = g_NetworkInfo.Interfaces[0].TransmitTestPassed;
637:     CopyMem(g_NetworkInfo.MacAddress, g_NetworkInfo.Interfaces[0].MacAddress,
638:             sizeof(g_NetworkInfo.MacAddress));
639:     EFI_SIMPLE_NETWORK_MODE* Mode = g_Snp->Mode;
640:     g_NetworkInfo.MaxPacketSize = Mode->MaxPacketSize;
641:     g_NetworkInfo.IfType = Mode->IfType;
642:     g_NetworkInfo.HwAddressSize = Mode->HwAddressSize;
643:     g_NetworkInfo.MediaHeaderSize = Mode->MediaHeaderSize;
644:     g_NetworkInfo.State = Mode->State;
645: 
646:     g_HardwareContext.NetworkInterfaces = (UINT32)g_SnpIfaceCount;
647:     g_HardwareContext.NetworkInitialized = TRUE;
648:     g_NetworkInited = TRUE;
649: 
650:     Print(L"Network subsystem initialized with %d interface(s)\n",
651:           g_SnpIfaceCount);
652: 
653:     return EFI_SUCCESS;
654: }
655: 
656: EFI_STATUS
657: PpcGetNetworkInfo (
658:     OUT PPC_NETWORK_INFO* Info
659:     )
660: {
661:     if (Info == NULL) {
662:         return EFI_INVALID_PARAMETER;
663:     }
664:     if (!g_NetworkInited || g_Snp == NULL) {
665:         return EFI_NOT_READY;
666:     }
667:     CopyMem(Info, &g_NetworkInfo, sizeof(PPC_NETWORK_INFO));
668:     return EFI_SUCCESS;
669: }
670: 
671: EFI_STATUS
672: PpcInitializeBlockIo (
673:     IN UINT32 DeviceCount
674:     )
675: {
676:     if (DeviceCount == 0) {
677:         return EFI_INVALID_PARAMETER;
678:     }
679: 
680:     Print(L"Initializing block devices (locating Block I/O protocols)\n");
681: 
682:     EFI_HANDLE* Handles = NULL;
683:     UINTN       Count   = 0;
684:     EFI_STATUS  Status  = BS->LocateHandleBuffer(
685:         ByProtocol,
686:         &BlockIoProtocol,
687:         NULL,
688:         &Count,
689:         &Handles
690:     );
691: 
692:     if (EFI_ERROR(Status)) {
693:         if (Status == EFI_NOT_FOUND) {
694:             Print(L"No block devices found\n");
695:             return EFI_NOT_FOUND;
696:         }
697:         Print(L"Failed to locate block devices: %r\n", Status);
698:         return Status;
699:     }
700: 
701:     if (g_BlockDevices != NULL) {
702:         FreePool(g_BlockDevices);
703:         g_BlockDevices = NULL;
704:     }
705:     g_BlockDeviceCount = 0;
706:     ZeroMem(&g_BlockIoInfo, sizeof(g_BlockIoInfo));
707: 
708:     g_BlockDevices = AllocatePool(Count * sizeof(EFI_BLOCK_IO_PROTOCOL*));
709:     if (g_BlockDevices == NULL) {
710:         FreePool(Handles);
711:         return EFI_OUT_OF_RESOURCES;
712:     }
713: 
714:     for (UINTN i = 0; i < Count; i++) {
715:         EFI_BLOCK_IO_PROTOCOL* Bio = NULL;
716:         Status = BS->HandleProtocol(Handles[i], &BlockIoProtocol, (VOID**)&Bio);
717:         if (EFI_ERROR(Status) || Bio == NULL || Bio->Media == NULL) {
718:             continue;
719:         }
720:         if (!Bio->Media->MediaPresent) {
721:             continue;
722:         }
723:         g_BlockDevices[g_BlockDeviceCount++] = Bio;
724:     }
725: 
726:     FreePool(Handles);
727: 
728:     if (g_BlockDeviceCount == 0) {
729:         Print(L"Block I/O subsystem initialized with 0 usable devices\n");
730:         g_HardwareContext.StorageDevices = 0;
731:         return EFI_NOT_FOUND;
732:     }
733: 
734:     g_BlockIoInfo.DeviceCount = g_BlockDeviceCount;
735: 
736:     // Default to the first usable device for info reporting.
737:     g_BlockIoInfo.SelectedIndex = 0;
738:     g_BlockIoInfo.MediaId = g_BlockDevices[0]->Media->MediaId;
739:     g_BlockIoInfo.BlockSize = g_BlockDevices[0]->Media->BlockSize;
740:     g_BlockIoInfo.LastBlock = g_BlockDevices[0]->Media->LastBlock;
741:     g_BlockIoInfo.ReadOnly = g_BlockDevices[0]->Media->ReadOnly;
742:     g_BlockIoInfo.Removable = g_BlockDevices[0]->Media->RemovableMedia;
743: 
744:     Print(L"Block I/O subsystem initialized with %d usable device(s)\n",
745:           g_BlockDeviceCount);
746: 
747:     for (UINTN i = 0; i < g_BlockDeviceCount; i++) {
748:         EFI_BLOCK_IO_MEDIA* M = g_BlockDevices[i]->Media;
749:         Print(L"  Device %d: %d bytes/block, %d blocks, %s%s\n",
750:               (UINTN)i, M->BlockSize, (UINT64)M->LastBlock + 1,
751:               M->RemovableMedia ? L"removable, " : L"",
752:               M->ReadOnly ? L"read-only" : L"writable");
753:     }
754: 
755:     g_HardwareContext.StorageDevices = (UINT32)g_BlockDeviceCount;
756:     g_HardwareContext.StorageInitialized = TRUE;
757: 
758:     return EFI_SUCCESS;
759: }
760: 
761: EFI_STATUS
762: PpcGetBlockIoInfo (
763:     OUT PPC_BLOCK_IO_INFO* Info
764:     )
765: {
766:     if (Info == NULL) {
767:         return EFI_INVALID_PARAMETER;
768:     }
769:     if (g_BlockDeviceCount == 0 || g_BlockDevices == NULL) {
770:         return EFI_NOT_READY;
771:     }
772:     CopyMem(Info, &g_BlockIoInfo, sizeof(PPC_BLOCK_IO_INFO));
773:     return EFI_SUCCESS;
774: }
775: 
776: EFI_STATUS
777: PpcReadDiskBlock (
778:     IN  UINTN   Index,
779:     IN  EFI_LBA Lba,
780:     IN  UINTN   BufferSize,
781:     OUT VOID*   Buffer
782:     )
783: {
784:     if (Buffer == NULL) {
785:         return EFI_INVALID_PARAMETER;
786:     }
787:     if (Index >= g_BlockDeviceCount || g_BlockDevices == NULL) {
788:         return EFI_INVALID_PARAMETER;
789:     }
790: 
791:     EFI_BLOCK_IO_PROTOCOL* Bio = g_BlockDevices[Index];
792:     UINT32 MediaId = Bio->Media->MediaId;
793: 
794:     return Bio->ReadBlocks(Bio, MediaId, Lba, BufferSize, Buffer);
795: }
796: 
797: EFI_STATUS
798: PpcInitializeSerial (
799:     IN UINT32 PortCount
800:     )
801: {
802:     if (PortCount == 0) {
803:         return EFI_INVALID_PARAMETER;
804:     }
805:     
806:     Print(L"Initializing %d serial ports\n", PortCount);
807:     
808:     // In a real implementation:
809:     // 1. Configure serial port hardware
810:     // 2. Initialize UART controllers
811:     // 3. Set up communication protocols
812:     
813:     g_HardwareContext.SerialPorts = PortCount;
814:     
815:     Print(L"Serial subsystem initialized with %d ports\n", PortCount);
816:     
817:     return EFI_SUCCESS;
818: }
819: 
820: EFI_STATUS
821: PpcGetHardwareInfo (
822:     OUT PPC_HARDWARE_INFO* HardwareInfo
823:     )
824: {
825:     if (HardwareInfo == NULL) {
826:         return EFI_INVALID_PARAMETER;
827:     }
828:     
829:     // Fill hardware information structure
830:     ZeroMem(HardwareInfo, sizeof(PPC_HARDWARE_INFO));
831:     
832:     HardwareInfo->IsInitialized = g_HardwareContext.IsInitialized;
833:     HardwareInfo->GraphicsMode = g_HardwareContext.GraphicsMode;
834:     HardwareInfo->AudioEnabled = g_HardwareContext.AudioEnabled;
835:     HardwareInfo->StorageDevices = g_HardwareContext.StorageDevices;
836:     HardwareInfo->NetworkInterfaces = g_HardwareContext.NetworkInterfaces;
837:     
838:     return EFI_SUCCESS;
839: }
840: 
841: EFI_STATUS
842: PpcHandleHardwareInterrupt (
843:     IN UINT32 InterruptNumber
844:     )
845: {
846:     Print(L"Handling hardware interrupt %d\n", InterruptNumber);
847:     
848:     // In a real implementation:
849:     // 1. Route interrupt to appropriate handler
850:     // 2. Process interrupt request
851:     // 3. Update interrupt status
852:     // 4. Call registered interrupt handlers
853:     
854:     switch (InterruptNumber) {
855:         case 0x20:  // Timer interrupt
856:             Print(L"Timer interrupt handled\n");
857:             break;
858:             
859:         case 0x21:  // Keyboard interrupt
860:             Print(L"Keyboard interrupt handled\n");
861:             break;
862:             
863:         case 0x22:  // Disk interrupt
864:             Print(L"Disk interrupt handled\n");
865:             break;
866:             
867:         default:
868:             Print(L"Unhandled hardware interrupt %d\n", InterruptNumber);
869:             break;
870:     }
871:     
872:     return EFI_SUCCESS;
873: }
874: 
875: EFI_STATUS
876: PpcSetHardwareState (
877:     IN PPC_HARDWARE_STATE State,
878:     IN UINT32             Value
879:     )
880: {
881:     switch (State) {
882:         case PPC_HARDWARE_GRAPHICS_MODE:
883:             g_HardwareContext.GraphicsMode = Value;
884:             Print(L"Graphics mode set to 0x%x\n", Value);
885:             break;
886:             
887:         case PPC_HARDWARE_AUDIO_ENABLE:
888:             g_HardwareContext.AudioEnabled = Value;
889:             Print(L"Audio enabled: %d\n", Value);
890:             break;
891:             
892:         default:
893:             return EFI_UNSUPPORTED;
894:     }
895:     
896:     return EFI_SUCCESS;
897: }
898: 
899: EFI_STATUS
900: PpcGetHardwareState (
901:     IN  PPC_HARDWARE_STATE State,
902:     OUT UINT32*            Value
903:     )
904: {
905:     if (Value == NULL) {
906:         return EFI_INVALID_PARAMETER;
907:     }
908:     
909:     switch (State) {
910:         case PPC_HARDWARE_GRAPHICS_MODE:
911:             *Value = g_HardwareContext.GraphicsMode;
912:             break;
913:             
914:         case PPC_HARDWARE_AUDIO_ENABLE:
915:             *Value = g_HardwareContext.AudioEnabled;
916:             break;
917:             
918:         default:
919:             return EFI_UNSUPPORTED;
920:     }
921:     
922:     return EFI_SUCCESS;
923: }
924: 
925: // Advanced hardware functions for PowerPC-specific needs
926: 
927: EFI_STATUS
928: PpcReadHardwareRegister (
929:     IN  UINT32 Address,
930:     OUT UINT32* Value
931:     )
932: {
933:     if (Value == NULL) {
934:         return EFI_INVALID_PARAMETER;
935:     }
936:     
937:     // Simulate reading a hardware register
938:     *Value = 0x00000000;  // Default value
939:     
940:     Print(L"Reading hardware register at 0x%x\n", Address);
941:     
942:     // In real implementation, this would read from actual hardware
943:     // or from emulated hardware registers
944:     
945:     return EFI_SUCCESS;
946: }
947: 
948: EFI_STATUS
949: PpcWriteHardwareRegister (
950:     IN UINT32 Address,
951:     IN UINT32 Value
952:     )
953: {
954:     Print(L"Writing 0x%x to hardware register at 0x%x\n", Value, Address);
955:     
956:     // In real implementation, this would write to actual hardware
957:     // or to emulated hardware registers
958:     
959:     return EFI_SUCCESS;
960: }
961: 
962: EFI_STATUS
963: PpcGetVideoBuffer (
964:     OUT VOID** Buffer,
965:     OUT UINT64* Size
966:     )
967: {
968:     if (Buffer == NULL || Size == NULL) {
969:         return EFI_INVALID_PARAMETER;
970:     }
971:     
972:     *Buffer = g_HardwareContext.VideoBuffer;
973:     *Size = g_HardwareContext.VideoBufferSize;
974:     
975:     return EFI_SUCCESS;
976: }
977: 
978: EFI_STATUS
979: PpcClearVideoBuffer (
980:     VOID
981:     )
982: {
983:     if (g_HardwareContext.VideoBuffer != NULL && g_HardwareContext.VideoBufferSize > 0) {
984:         ZeroMem(g_HardwareContext.VideoBuffer, g_HardwareContext.VideoBufferSize);
985:         Print(L"Video buffer cleared\n");
986:         return EFI_SUCCESS;
987:     }
988:     
989:     return EFI_NOT_READY;
990: }
991: 
992: EFI_STATUS
993: PpcUpdateVideoDisplay (
994:     IN UINT32 X,
995:     IN UINT32 Y,
996:     IN UINT32 Width,
997:     IN UINT32 Height
998:     )
999: {
1000:     Print(L"Updating video display region: (%d,%d) %dx%d\n", X, Y, Width, Height);
1001:     
1002:     // In real implementation:
1003:     // 1. Update the display with content from video buffer
1004:     // 2. Handle hardware-specific display updates
1005:     // 3. Synchronize with refresh rate
1006:     
1007:     return EFI_SUCCESS;
1008: }
1009: 
1010: EFI_STATUS
1011: PpcGetFrameBufferInfo (
1012:     OUT PPC_FRAMEBUFFER_INFO* Info
1013:     )
1014: {
1015:     if (Info == NULL) {
1016:         return EFI_INVALID_PARAMETER;
1017:     }
1018:     if (!g_HardwareContext.GraphicsInitialized || g_Gop == NULL) {
1019:         return EFI_NOT_READY;
1020:     }
1021: 
1022:     Info->Width           = g_HardwareContext.GuestFbWidth;
1023:     Info->Height          = g_HardwareContext.GuestFbHeight;
1024:     Info->Pitch           = g_HardwareContext.GuestFbPitch;
1025:     Info->BitsPerPixel    = 32;
1026:     Info->GuestBase       = g_HardwareContext.GuestFbBase;
1027:     Info->HostBuffer      = g_HardwareContext.GuestFbHost;
1028:     Info->HostBufferSize  = g_HardwareContext.GuestFbSize;
1029:     Info->GopFrameBuffer  = g_Gop->Mode->FrameBufferBase;
1030:     Info->GopPitch        = g_FramebufferPitch;
1031:     Info->PixelFormat     = g_PixelFormat;
1032:     return EFI_SUCCESS;
1033: }
1034: 
1035: /**
1036:   Read a pixel from the guest framebuffer (big-endian 0xRRGGBB00).
1037:   Unmapped or out-of-bounds reads return zero.
1038: **/
1039: STATIC
1040: UINT32
1041: FrameBufferReadPixel (
1042:     IN UINT32 X,
1043:     IN UINT32 Y
1044:     )
1045: {
1046:     if (g_HardwareContext.GuestFbHost == NULL ||
1047:         X >= g_HardwareContext.GuestFbWidth ||
1048:         Y >= g_HardwareContext.GuestFbHeight) {
1049:         return 0;
1050:     }
1051: 
1052:     UINT8* Pixel = (UINT8*)g_HardwareContext.GuestFbHost +
1053:                    (UINT64)Y * g_HardwareContext.GuestFbPitch +
1054:                    (UINT64)X * 4;
1055:     return ((UINT32)Pixel[0] << 24) |
1056:            ((UINT32)Pixel[1] << 16) |
1057:            ((UINT32)Pixel[2] << 8)  |
1058:            ((UINT32)Pixel[3]);
1059: }
1060: 
1061: /**
1062:   Write a pixel to the guest framebuffer in big-endian 0xRRGGBB00 layout.
1063:   Out-of-bounds writes are dropped.
1064: **/
1065: STATIC
1066: VOID
1067: FrameBufferWritePixel (
1068:     IN UINT32 X,
1069:     IN UINT32 Y,
1070:     IN UINT32 Color
1071:     )
1072: {
1073:     if (g_HardwareContext.GuestFbHost == NULL ||
1074:         X >= g_HardwareContext.GuestFbWidth ||
1075:         Y >= g_HardwareContext.GuestFbHeight) {
1076:         return;
1077:     }
1078: 
1079:     UINT8* Pixel = (UINT8*)g_HardwareContext.GuestFbHost +
1080:                    (UINT64)Y * g_HardwareContext.GuestFbPitch +
1081:                    (UINT64)X * 4;
1082:     Pixel[0] = (UINT8)(Color >> 24);
1083:     Pixel[1] = (UINT8)(Color >> 16);
1084:     Pixel[2] = (UINT8)(Color >> 8);
1085:     Pixel[3] = (UINT8)Color;
1086: }
1087: 
1088: EFI_STATUS
1089: PpcGraphicsBlitToDisplay (
1090:     VOID
1091:     )
1092: {
1093:     if (!g_HardwareContext.GraphicsInitialized ||
1094:         g_Gop == NULL ||
1095:         g_HardwareContext.GuestFbHost == NULL) {
1096:         return EFI_NOT_READY;
1097:     }
1098: 
1099:     UINT32* Dest = (UINT32*)(UINTN)g_Gop->Mode->FrameBufferBase;
1100:     UINTN   DestPitch = g_FramebufferPitch / 4;
1101:     UINTN   W = g_HardwareContext.GuestFbWidth;
1102:     UINTN   H = g_HardwareContext.GuestFbHeight;
1103: 
1104:     BOOLEAN Bgr = (g_PixelFormat == PixelBlueGreenRedReserved8BitPerColor);
1105: 
1106:     UINTN Y;
1107:     for (Y = 0; Y < H; Y++) {
1108:         UINT32* Row = Dest + Y * DestPitch;
1109:         UINTN X;
1110:         for (X = 0; X < W; X++) {
1111:             UINT32 GuestColor = FrameBufferReadPixel((UINT32)X, (UINT32)Y);
1112:             UINT8 R = (UINT8)(GuestColor >> 24);
1113:             UINT8 G = (UINT8)(GuestColor >> 16);
1114:             UINT8 B = (UINT8)(GuestColor >> 8);
1115:             UINT32 HostColor;
1116:             if (Bgr) {
1117:                 // BGR layout: byte0=Blue, byte1=Green, byte2=Red, byte3=reserved.
1118:                 HostColor = ((UINT32)B) | ((UINT32)G << 8) | ((UINT32)R << 16);
1119:             } else {
1120:                 // RGB layout: byte0=Red, byte1=Green, byte2=Blue, byte3=reserved.
1121:                 HostColor = ((UINT32)R) | ((UINT32)G << 8) | ((UINT32)B << 16);
1122:             }
1123:             Row[X] = HostColor;
1124:         }
1125:     }
1126: 
1127:     return EFI_SUCCESS;
1128: }
1129: 
1130: EFI_STATUS
1131: PpcGraphicsClear (
1132:     IN UINT32 Color
1133:     )
1134: {
1135:     if (!g_HardwareContext.GraphicsInitialized || g_HardwareContext.GuestFbHost == NULL) {
1136:         return EFI_NOT_READY;
1137:     }
1138: 
1139:     UINTN X, Y;
1140:     for (Y = 0; Y < g_HardwareContext.GuestFbHeight; Y++) {
1141:         for (X = 0; X < g_HardwareContext.GuestFbWidth; X++) {
1142:             FrameBufferWritePixel((UINT32)X, (UINT32)Y, Color);
1143:         }
1144:     }
1145:     return EFI_SUCCESS;
1146: }
1147: 
1148: EFI_STATUS
1149: PpcGraphicsSetPixel (
1150:     IN UINT32 X,
1151:     IN UINT32 Y,
1152:     IN UINT32 Color
1153:     )
1154: {
1155:     if (!g_HardwareContext.GraphicsInitialized || g_HardwareContext.GuestFbHost == NULL) {
1156:         return EFI_NOT_READY;
1157:     }
1158:     FrameBufferWritePixel(X, Y, Color);
1159:     return EFI_SUCCESS;
1160: }
1161: 
1162: EFI_STATUS
1163: PpcGraphicsDrawRect (
1164:     IN UINT32 X,
1165:     IN UINT32 Y,
1166:     IN UINT32 Width,
1167:     IN UINT32 Height,
1168:     IN UINT32 Color
1169:     )
1170: {
1171:     if (!g_HardwareContext.GraphicsInitialized || g_HardwareContext.GuestFbHost == NULL) {
1172:         return EFI_NOT_READY;
1173:     }
1174: 
1175:     UINT32 Rx, Ry;
1176:     for (Ry = Y; Ry < Y + Height; Ry++) {
1177:         for (Rx = X; Rx < X + Width; Rx++) {
1178:             FrameBufferWritePixel(Rx, Ry, Color);
1179:         }
1180:     }
1181:     return EFI_SUCCESS;
1182: }

(End of file - total 1182 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 45.7s)

_Thinking:_

Now I have a comprehensive picture of the project. Let me summarize the current state:

**Project:** EFI-Mac-Emulator — a UEFI-based PowerPC Mac OS emulator.

**Completed:**
- Phase 1: Research ✓
- Phase 2: Architecture (partial)
- Phase 3: Core Implementation ✓ (interpreter, memory manager, real UEFI protocols)
- Phase 4: Emulation Components ✓ (graphics/GOP, audio ring buffer, block I/O, SNP networking)

**Next (Phase 5): Boot Process**
- [ ] Create bootloader for Mac OS (load ROM image, set up guest memory map)
- [ ] Implement system initialization routines
- [ ] Add support for system files and drivers
- [ ] Test boot process with various Mac OS versions

**Environment constraint:** We're on Windows. No clang/lld-link/qemu/cmake. The build is normally done on macOS. So I cannot build or run the QEMU tests here. I need to be careful and write code that compiles cleanly on the macOS toolchain (with -Wall -Werror), and document how to test it.

Now, what's the right "next objective"? Phase 5 item 1: "Create bootloader for Mac OS (load ROM image, set up guest memory map)".

Given the existing architecture, the natural and achievable next increment is to build the classic Mac OS boot memory map and ROM loading path:

1. **Guest memory map**: Real classic Mac PPC memory map:
   - RAM: 0x00000000 (or 0x10000000 currently)... Actually currently guest RAM is at 0x10000000 (256 MB). But classic Mac OS PPC expects RAM at low addresses (0x00000000) with low-memory globals at 0x00000000-0x3000, and ROM at 0xFFF00000 (4GB-1MB).
   
   Hmm, but the current design has guest RAM at 0x10000000. Changing the base to 0x00000000 would require changing the memory manager, interpreter wiring, framebuffer base, audio base, etc. That's a big refactor.

   Actually, let me reconsider. The existing code has:
   - Guest RAM: 0x10000000, 256 MB (so 0x10000000 - 0x20000000)
   - Framebuffer: 0x18000000 (inside RAM)
   - Audio: 0x18800000 (inside RAM)

   For a classic Mac OS memory map we need:
   - RAM: 0x00000000+ (main memory, with low-memory globals at 0)
   - ROM: 0xFFF00000 (4MB ROM window)
   - Hardware MMIO at various addresses

   But rewriting the whole memory base is a big change that risks breaking the verified Phase 3/4 tests. The interpreter supports multi-region guest memory through the memory manager's `PpcTranslateAddress` and the interpreter's `g_ReadByte`/`g_WriteByte` default callbacks — but those are set via `PpcSetGuestMemory` with a single region.

   The cleanest incremental approach that fits the current architecture: **add ROM support as an additional mapped region** in the interpreter's guest memory, plus bootloader functions to:
   - Load a ROM file from the volume into a UEFI-allocated buffer.
   - Map that ROM into the guest memory map at the classic PPC ROM base (0xFFF00000).
   - Verify the ROM placement.
   - Configure the CPU context to start execution at the ROM reset vector (0xFFF00000 — the vector is at the start of the ROM, or the PPC Mac ROM starts execution at 0xFFF00000 with the reset handler).

Wait, actually for classic Mac OS PPC (new world/old world), the ROM is at 0xFFF00000 and the reset vector is 0xFFF00100 (hardware reset vector on PowerPC is 0x00000100, but for Mac ROMs... let me recall). 

On old-world Macs, the CPU boots with MSR[IP]=1 which redirects exceptions to 0xFFF00000. So the reset vector is at 0xFFF00100 (0xFFF00000 + 0x100). The ROM is at 0xFFF00000.

On new-world Macs (like what an emulator would target), the startup ROM at 0xFFF00000 contains the boot ROM that loads the kernel from the "Mac OS ROM" file (an Open Firmware startup image). 

Hmm, but actually the current bootloader loads a "kernel" from \System\MacOS\kernel. In classic Mac OS on PPC, the actual boot process is:
- Firmware (Open Firmware / ROM) at 0xFFF00000
- Mac OS ROM file loaded by firmware
- System file loaded from the HFS volume
- etc.

This is a deep rabbit hole. For the scope of this project (which is an emulator in very early stages, tested via self-tests in QEMU/OVMF), the practical next objective is NOT to actually boot a real Mac OS (that requires actual ROM images, HFS support, nanokernel, etc. — years of work). 

The realistic next increment aligned with the TODO is:

**Phase 5, first deliverable: "Guest memory map + ROM loading + system initialization"**

Concretely, I'll implement:

1. **`src/boot/guest_map.h` + `guest_map_impl.c`** (or extend bootloader): A guest memory map module that defines the classic Mac OS PPC memory map:
   - `PPC_GUEST_MAP_ROM_BASE = 0xFFF00000` (ROM window, up to 8 MB)
   - RAM base/size (from memory manager)
   - A ROM mapping region that the interpreter's default memory path honors.

2. **Extend interpreter guest memory** to support **multiple regions** (RAM + ROM). Currently only one region. I'll add a `PpcSetGuestRom` or generalize to a region table. The default read/write callbacks should check the ROM region (read-only).

3. **Bootloader ROM functions**: `PpcLoadSystemRom` already exists (loads into a pool buffer). I'll make it load into guest memory at the ROM base and add `PpcLoadRomToGuestMap` / boot-path function that:
   - Opens the ROM file via UEFI file I/O
   - Allocates a ROM buffer
   - Places it at the ROM guest address
   - Verifies placement (first bytes)

4. **System initialization routines**: `PpcPrepareSystemForBoot` currently just prints. Implement real system init:
   - Configure CPU context: PC = ROM reset vector (0xFFF00100), MSR = boot state (MSR[ME]=1, MSR[RI]=1, maybe MSR[IP]=1 for exception vectors at ROM)
   - Set up low-memory globals (below the RAM base if RAM at 0x10000000? Actually classic low mem is at 0x00000000). Hmm.

Let me reconsider the RAM base question. The current design uses guest RAM at 0x10000000. For the classic Mac memory map, low memory globals are at 0x00000000-0x00002FFF, with the RAM and system heap just above. The ROM is at 0xFFF00000.

Options:
- A) Keep guest RAM at 0x10000000 and place the "classic Mac memory map" as a set of emulated regions: low memory at 0x00000000, ROM at 0xFFF00000, RAM stays at 0x10000000. But then guest code running real Mac OS would need coherent addresses across the map. This is inconsistent.
- B) Move guest RAM base to 0x00000000 (or 0x20000000 for RAM; the "high" part). Actually real PPC Macs: RAM from 0x00000000, ROM at 0xFFF00000, I/O at various places (0x90000000+ new world, 0xF3000000 old world).

Actually moving guest RAM to 0x00000000 would break the framebuffer/audio guest bases (0x18000000, 0x18800000) which are inside RAM. If RAM = 0x00000000, size 256MB, then 0x18000000 is inside RAM (256MB = 0x10000000, so 0x18000000 is beyond!). 0x18000000 = 384MB. So with 256MB RAM at 0, framebuffer at 0x18000000 is not in RAM. The current carve-out logic checks `(PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE - GuestBase) + FbSize <= GuestSize`. If GuestBase = 0 and GuestSize = 0x10000000 (256MB), then 0x18000000 > 0x10000000 → condition fails → no guest framebuffer → graphics self-check SKIP. That would break Phase 4 verified behavior.

So option B is a big change. Not good for an incremental, non-verifiable-here step.

I think the cleanest next objective that is consistent with the existing architecture and doesn't break the verified tests is:

**Implement the classic Mac OS boot memory map as an overlay on the existing guest RAM + add ROM mapping, ROM loading, and system initialization routines — Phase 5 step 1.**

Design:
- Keep guest RAM at 0x10000000 (256MB) as the "guest RAM" for the interpreter.
- Add a ROM region at 0xFFF00000 (the classic PPC Mac ROM address) — a separate UEFI-allocated buffer that is read-only to the guest.
- Extend the interpreter's default memory path to honor multiple regions: the RAM region (read/write) + ROM region (read-only). Add `PpcSetGuestRom(HostBase, GuestBase, Size)` API.
- Add a "low memory" page at guest 0x00000000 (4KB) for classic Mac low-memory globals (BIOS parameter block, boot info, etc.). Actually maybe skip low memory for now to avoid overreach... but "set up guest memory map" is the objective. Let me include a low-memory region too: it's a small read/write region at 0x00000000. This gives a coherent three-region map:
  - RAM: 0x10000000 (256MB)
  - ROM: 0xFFF00000 (up to 8MB, read-only)
  - Low memory: 0x00000000 (4KB) — actually in classic Mac OS, low memory globals span ~0x0000-0x2FFF plus more. Let me allocate 16KB (0x4000) to be safe. Actually I'll keep it simple: allocate one page table region... hmm.

Hmm wait. Let me think about whether extending the interpreter memory to multiple regions is clean. Currently:
- `g_GuestMemoryHostBase`, `g_GuestMemoryGuestBase`, `g_GuestMemorySize` — single region, set by `PpcSetGuestMemory`.
- `PpcDefaultReadByte`/`PpcDefaultWriteByte` check that one region.

I'll change to a small region table (array of up to N regions), keeping `PpcSetGuestMemory` as the primary RAM region (index 0), and add `PpcAddGuestMemoryRegion(HostBase, GuestBase, Size, ReadOnly)` for ROM. This is backwards compatible.

Then bootloader:
- `PpcLoadSystemRom` — already loads to a pool buffer. I'll refactor so that there's a `PpcInstallSystemRom(RomPath)` that loads the ROM file and maps it into the guest memory map at 0xFFF00000, read-only.
- `PpcPrepareSystemForBoot` — implement: set up exception vectors state, configure CPU context for ROM boot (PC = 0xFFF00100, MSR with ME|RI, SRR0/SRR1), write low-memory globals (e.g., set up the memory map table pointer).
- Add a self-test `PpcRunBootSelfTest` that:
  1. Creates a small synthetic ROM in guest memory at 0xFFF00000 (or loads from file if present)
  2. Verifies the guest memory map regions are coherent (RAM region overlaps framebuffer, ROM readable, low mem writable)
  3. Configures CPU context and verifies PC/MSR
  4. Executes a tiny ROM-resident program (e.g., reads ROM base word, does an lwz from ROM address, writes result to RAM) to prove cross-region load/store works.

Also, main.c should call these and report results.

Additionally, there should be support for loading actual Mac OS system files and drivers: `PpcLoadSystemRom`, `PpcLoadBootImage`, `PpcLoadKernel` exist. Phase 5 items:
- "Add support for system files and drivers" — could add a `PpcLoadSystemFile`/`PpcLoadDriver` that loads files from the volume into guest RAM at a configurable offset. That's a straightforward file-load helper.

Given I cannot build/test here, I must be extra careful with code correctness. Let me keep the scope focused and well-tested-by-inspection:

**Objective: Phase 5 step 1 — Classic Mac OS boot memory map, ROM loading, and system initialization routines.**

Deliverables:
1. `src/boot/guest_map.h` / `guest_map_impl.c` (new module):
   - Region table with classic Mac memory map definitions (RAM from memory manager, ROM at 0xFFF00000, low memory at 0).
   - `PpcInitGuestMemoryMap()`, `PpcInstallRomRegion()`, `PpcGetGuestMemoryMap()`, region lookup.
   
   Actually, the interpreter already owns region-based memory. Let me NOT create a parallel memory map module; instead extend the interpreter with multi-region support (region table + add API), and put the boot/ROM/system-init logic in bootloader. This avoids module bloat and keeps things cohesive.

Let me plan concretely:

**Changes to `src/cpu/interpreter.c` / `interpreter.h`:**
- Add region table: 
  ```c
  #define PPC_MAX_GUEST_REGIONS 4
  typedef struct {
      VOID*   HostBase;
      UINT32  GuestBase;
      UINT32  Size;
      BOOLEAN ReadOnly;
      BOOLEAN Active;
  } PPC_GUEST_REGION;
  static PPC_GUEST_REGION g_GuestRegions[PPC_MAX_GUEST_REGIONS];
  ```
- `PpcSetGuestMemory(HostBase, GuestBase, Size)` → registers region[0] (read/write). Keep semantics.
- Add `PpcAddGuestMemoryRegion(HostBase, GuestBase, Size, ReadOnly)` → adds a region; returns error if full/overlapping? I'll allow overlapping with precedence = last added wins. Actually simpler: first match wins in lookup order. For RAM at 0x10000000 and ROM at 0xFFF00000 there's no overlap anyway.
- `PpcDefaultReadByte`: iterate regions; if address in range return byte; else 0.
- `PpcDefaultWriteByte`: iterate regions; if in range AND !ReadOnly, write; if in range AND ReadOnly, drop.
- Keep `PpcReadGuestByte`/`PpcWriteGuestByte` public.
- Add `PpcRemoveGuestRegion`? Not needed now.

Add to translation.h: `PpcAddGuestMemoryRegion` declaration. Also maybe a getter `PpcGetGuestMemoryRegionCount`? Not necessary.

**Changes to `src/boot/bootloader.h` / `bootloader_impl.c`:**
- Add constants: 
  ```c
  #define PPC_ROM_GUEST_BASE      0xFFF00000  // Classic PPC Mac ROM
  #define PPC_ROM_MAX_SIZE        0x00400000  // 4 MB window
  #define PPC_RESET_VECTOR        (PPC_ROM_GUEST_BASE + 0x100)
  #define PPC_LOW_MEM_GUEST_BASE  0x00000000
  #define PPC_LOW_MEM_SIZE        0x4000
  ```
- Add struct `PPC_GUEST_MEMORY_MAP` describing regions (RAM, ROM, LowMem) for reporting.
- Implement `PpcPrepareSystemForBoot` for real:
  - Ensure guest memory is wired (RAM).
  - Allocate + wire low-memory region at 0x00000000 (if not present).
  - Optionally install ROM from a file (default `\System\MacOS\ROM`).
  - Configure CPU context: PC = reset vector, MSR = PPC_MSR_ME | PPC_MSR_RI, SRR0/SRR1, ExceptionPending=0, zero the GPRs.
  - Write low-memory globals (magic value at a defined offset).
- Implement `PpcLoadSystemRom` improvement: keep the pool-buffer loader, add `PpcInstallSystemRom(RomPath)`:
  - Loads file (use PpcLoadFile) — validate size ≤ ROM window
  - Allocates pages for ROM region (AllocatePages), copies content, wires as read-only via `PpcAddGuestMemoryRegion`
  - Optionally also keep a copy in the context.
  - Also handle `PpcLoadSystemRom` returning a buffer that main can install. Actually simplest: implement `PpcInstallSystemRom` that does the whole thing; keep `PpcLoadSystemRom` as a thin wrapper that also returns the buffer.
  
  Hmm, `PpcLoadSystemRom` has a signature `(RomPath, &RomBuffer, &RomSize)`. It loads into a pool buffer. But pool allocations with UEFI have size limits (typically 64KB per pool allocation in some implementations — actually AllocatePool limit is 128KB in some). A 4MB ROM would exceed pool allocation limits. Better to use AllocatePages for ROM. I'll change `PpcLoadSystemRom` to allocate pages (page-aligned) and return the buffer + size. But main.c doesn't currently call it. Only bootloader_impl.c defines it. I can change its implementation freely since nothing calls it (let me verify). Let me grep for callers of PpcLoadSystemRom, PpcPrepareSystemForBoot, PpcRunSelfTest etc.

**System initialization routines** — the second TODO item. I'll implement:
- `PpcPrepareSystemForBoot` (already declared in bootloader.h? No — it's in bootloader_impl.c but not declared in bootloader.h! Let me check. bootloader.h doesn't declare `PpcPrepareSystemForBoot`. It declares `PpcSetupBootEnvironment`. `PpcPrepareSystemForBoot` is defined in the impl but not declared anywhere → it's a static-ish exported symbol without prototype, which under -Wall -Werror might cause a warning? No, a definition without prototype doesn't warn. But it also means main can't call it without a declaration. I'll add it to the header as part of "system initialization routines".)

Let me also check the debug impl and platform impl for completeness, then check for callers via grep.

Let me also reconsider: should I keep this focused? The user asked to "determine next objective, and execute the completion of that objective". So I should:
1. Determine the objective (Phase 5 step 1: Boot Process foundation).
2. Execute it fully.
3. Update TODO.md / docs.
4. Commit? Only if asked... The instructions say never commit unless explicitly asked. The user asked to "execute the completion of that objective" — that's about doing the work, not committing. I'll do the work but not commit, unless... hmm. Given the repo has clean git history with commit messages per phase, maybe committing is expected. But the guideline is explicit: "NEVER commit changes unless the user explicitly asks you to." I won't commit.

Let me verify a few things:
- Who calls `PpcPrepareSystemForBoot`? grep.
- Who calls `PpcLoadSystemRom`? grep.
- What is in translation_impl.c (PpcRunSelfTest, PpcGetRegisterValue etc.)? Let me read it to ensure my interpreter changes don't conflict.

Let me do these greps and reads.

**Tool: grep**

**Input:**
```json
{
  "pattern": "PpcPrepareSystemForBoot|PpcLoadSystemRom|PpcLoadBootImage|PpcRunSelfTest|PpcSetGuestMemory|PpcAddGuestMemoryRegion|PpcGetGuestMemoryRegion"
}
```

**Output:**
```
Found 27 matches
C:\Users\clayc\Desktop\New folder (2)\EFIMac\Session_2_Transcript.md:
  Line 1750: Update PpcInitializeAudio to carve the ring buffer out of guest RAM (using PpcGetGuestMemoryRegion) like the framebuffer.

  Line 1906:   "newString": "EFI_STATUS\nPpcInitializeAudio (\n    VOID\n    )\n{\n    Print(L\"Initializing audio subsystem (emulated PCM ring buffer in guest RAM)\\n\");\n\n    // UEFI has no standard audio output protocol, so the \"device\" is a fixed\n    // ring buffer inside guest RAM that guest code fills with big-endian\n    // 16-bit stereo PCM samples.\n    VOID*  GuestHost   = NULL;\n    UINT64 GuestBase   = 0;\n    UINT64 GuestSize   = 0;\n    EFI_STATUS Status = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);\n    if (EFI_ERROR(Status) || GuestHost == NULL) {\n        Print(L\"Audio subsystem: guest RAM unavailable, falling back to pool buffer\\n\");\n        Status = BS->AllocatePool(EfiBootServicesData, PPC_AUDIO_BUFFER_SIZE,\n                                  (VOID**)&g_AudioInfo.HostBuffer);\n        if (EFI_ERROR(Status)) {\n            return Status;\n        }\n        g_AudioInfo.GuestBase = 0;\n    } else {\n        if ((UINT64)(PPC_AUDIO_BUFFER_GUEST_BASE - (UINT32)GuestBase) + PPC_AUDIO_BUFFER_SIZE > GuestSize) {\n            Print(L\"Audio subsystem: guest RAM too small\\n\");\n            return EFI_NOT_READY;\n        }\n        g_AudioInfo.GuestBase = PPC_AUDIO_BUFFER_GUEST_BASE;\n        g_AudioInfo.HostBuffer = (UINT8*)GuestHost +\n                                 (PPC_AUDIO_BUFFER_GUEST_BASE - (UINT32)GuestBase);\n    }\n\n    g_AudioInfo.Size = PPC_AUDIO_BUFFER_SIZE;\n    g_AudioInfo.SampleRate = PPC_AUDIO_SAMPLE_RATE;\n    g_AudioInfo.Channels = PPC_AUDIO_CHANNELS;\n    g_AudioInfo.BitsPerSample = PPC_AUDIO_BITS_PER_SAMPLE;\n    g_AudioInfo.WriteCursor = 0;\n    g_AudioInfo.PlayCursor = 0;\n    ZeroMem(g_AudioInfo.HostBuffer, g_AudioInfo.Size);\n\n    g_HardwareContext.AudioEnabled = 1;\n    g_HardwareContext.AudioInitialized = TRUE;\n    g_AudioInited = TRUE;\n\n    Print(L\"Audio subsystem initialized: guest 0x%x (host 0x%x, %d bytes, %d Hz, %d ch)\\n\",\n          g_AudioInfo.GuestBase, g_AudioInfo.HostBuffer,\n          g_AudioIn...
  Line 2348: 1. Finish audio impl: replace PpcInitializeAudio stub with guest RAM ring-buffer setup (PpcGetGuestMemoryRegion carve-out at 0x18800000), add PpcGetAudioInfo declaration to header (info struct already added), implement PpcGetAudioInfo + PpcAudioWriteSamples maybe.

  Line 2437: 1. Replace `PpcInitializeAudio` stub with real emulated device: carve 0x2000 bytes out of guest RAM at 0x18800000 via `PpcGetGuestMemoryRegion` (base + size ≤ guest RAM), ZeroMem buffer, set AudioEnabled/Initialized, print `Audio: guest buffer at guest 0x18800000 (host 0x..., N bytes, 44100Hz stereo 16-bit)`.


C:\Users\clayc\Desktop\New folder (2)\EFIMac\Session_Transcript.md:
  Line 3856:   "content": "#include \"bootloader.h\"\n#include <Uefi.h>\n#include <Library/UefiLib.h>\n#include <Library/BaseLib.h>\n#include <Library/BaseMemoryLib.h>\n#include <Library/UefiBootServicesTableLib.h>\n\n// Bootloader context structure\ntypedef struct {\n    BOOLEAN IsInitialized;\n    CHAR16* BootImagePath;\n    EFI_PHYSICAL_ADDRESS KernelAddress;\n    UINT64 KernelSize;\n    BOOLEAN KernelLoaded;\n} PPC_BOOTLOADER_CONTEXT;\n\n// Global bootloader context\nSTATIC PPC_BOOTLOADER_CONTEXT g_BootContext = {0};\n\nEFI_STATUS\nPpcInitializeBootloader (\n    VOID\n    )\n{\n    // Initialize the bootloader context\n    ZeroMem(&g_BootContext, sizeof(g_BootContext));\n    \n    g_BootContext.IsInitialized = TRUE;\n    g_BootContext.BootImagePath = NULL;\n    g_BootContext.KernelAddress = 0;\n    g_BootContext.KernelSize = 0;\n    g_BootContext.KernelLoaded = FALSE;\n    \n    Print(L\"PowerPC Bootloader initialized\\n\");\n    \n    return EFI_SUCCESS;\n}\n\nEFI_STATUS\nPpcLoadKernel (\n    IN  CHAR16* ImagePath,\n    OUT EFI_PHYSICAL_ADDRESS* KernelAddress,\n    OUT UINT64* KernelSize\n    )\n{\n    if (ImagePath == NULL || KernelAddress == NULL || KernelSize == NULL) {\n        return EFI_INVALID_PARAMETER;\n    }\n    \n    Print(L\"Loading kernel from: %s\\n\", ImagePath);\n    \n    // In a real implementation:\n    // 1. Locate the kernel image file\n    // 2. Read kernel data into memory\n    // 3. Parse kernel headers\n    // 4. Validate kernel integrity\n    \n    // For now, we'll simulate loading by setting default values\n    *KernelAddress = 0x10000000;  // Simulated kernel address\n    *KernelSize = 0x01000000;     // 16MB simulated kernel size\n    \n    g_BootContext.KernelAddress = *KernelAddress;\n    g_BootContext.KernelSize = *KernelSize;\n    g_BootContext.KernelLoaded = TRUE;\n    \n    Print(L\"Kernel loaded at 0x%x (size: %d bytes)\\n\", *KernelAddress, *KernelSize);\n    \n    return EFI_SUCCESS;\n}\n\nEFI_STATUS\nPpcBootSystem (\n    IN EFI_PHYSI...
  Line 3875:   "content": "#ifndef __PPC_BOOTLOADER_H__\n#define __PPC_BOOTLOADER_H__\n\n#include <Uefi.h>\n\n// Boot modes\n#define PPC_BOOT_MODE_NORMAL    0\n#define PPC_BOOT_MODE_RECOVERY  1\n#define PPC_BOOT_MODE_DIAGNOSTIC 2\n\n// Boot parameters structure\ntypedef struct {\n    UINT32 BootMode;\n    UINT32 MemorySizeMB;\n    UINT32 VideoMode;\n    BOOLEAN EnableDebug;\n    CHAR16* CommandLine;\n} PPC_BOOT_PARAMETERS;\n\n// Boot information structure\ntypedef struct {\n    BOOLEAN IsInitialized;\n    EFI_PHYSICAL_ADDRESS KernelAddress;\n    UINT64 KernelSize;\n    BOOLEAN KernelLoaded;\n} PPC_BOOT_INFO;\n\n/**\n  Initialize PowerPC bootloader\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcInitializeBootloader (\n    VOID\n    );\n\n/**\n  Load kernel image into memory\n  @param[in]  ImagePath      Path to the kernel image\n  @param[out] KernelAddress  Pointer to store kernel load address\n  @param[out] KernelSize     Pointer to store kernel size\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcLoadKernel (\n    IN  CHAR16* ImagePath,\n    OUT EFI_PHYSICAL_ADDRESS* KernelAddress,\n    OUT UINT64* KernelSize\n    );\n\n/**\n  Boot the PowerPC system\n  @param[in] KernelAddress Address of kernel to boot\n  @param[in] KernelSize    Size of kernel to boot\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcBootSystem (\n    IN EFI_PHYSICAL_ADDRESS KernelAddress,\n    IN UINT64               KernelSize\n    );\n\n/**\n  Load a boot image into memory\n  @param[in]  ImagePath   Path to the boot image\n  @param[out] ImageBuffer Pointer to store image buffer address\n  @param[out] ImageSize   Pointer to store image size\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcLoadBootImage (\n    IN  CHAR16* ImagePath,\n    OUT VOID**  ImageBuffer,\n    OUT UINT64* ImageSize\n    );\n\n/**\n  Set boot parameters for system boot\n  @param[in] Parameters Boot parameters to set\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcSetBootParameters (\n    IN PPC_BOOT_PARAMETERS* Parameters\...
  Line 4198:   "content": "#include \"bootloader.h\"\n#include <Uefi.h>\n#include <Library/UefiLib.h>\n#include <Library/BaseLib.h>\n#include <Library/BaseMemoryLib.h>\n#include <Library/UefiBootServicesTableLib.h>\n#include <Protocol/LoadedImage.h>\n\n// Bootloader context structure with more complete implementation\ntypedef struct {\n    BOOLEAN IsInitialized;\n    CHAR16* BootImagePath;\n    EFI_PHYSICAL_ADDRESS KernelAddress;\n    UINT64 KernelSize;\n    BOOLEAN KernelLoaded;\n    BOOLEAN SystemBooting;\n    PPC_BOOT_PARAMETERS BootParams;\n    EFI_LOADED_IMAGE* LoadedImage;\n} PPC_BOOTLOADER_CONTEXT;\n\n// Global bootloader context\nSTATIC PPC_BOOTLOADER_CONTEXT g_BootContext = {0};\n\nEFI_STATUS\nPpcInitializeBootloader (\n    VOID\n    )\n{\n    // Initialize the bootloader context\n    ZeroMem(&g_BootContext, sizeof(g_BootContext));\n    \n    g_BootContext.IsInitialized = TRUE;\n    g_BootContext.BootImagePath = NULL;\n    g_BootContext.KernelAddress = 0;\n    g_BootContext.KernelSize = 0;\n    g_BootContext.KernelLoaded = FALSE;\n    g_BootContext.SystemBooting = FALSE;\n    \n    // Initialize boot parameters\n    ZeroMem(&g_BootContext.BootParams, sizeof(PPC_BOOT_PARAMETERS));\n    g_BootContext.BootParams.BootMode = PPC_BOOT_MODE_NORMAL;\n    g_BootContext.BootParams.MemorySizeMB = 128;  // Default 128MB\n    g_BootContext.BootParams.VideoMode = PPC_GRAPHICS_MODE_640x480;\n    g_BootContext.BootParams.EnableDebug = FALSE;\n    \n    Print(L\"PowerPC Bootloader initialized\\n\");\n    \n    return EFI_SUCCESS;\n}\n\nEFI_STATUS\nPpcLoadKernel (\n    IN  CHAR16* ImagePath,\n    OUT EFI_PHYSICAL_ADDRESS* KernelAddress,\n    OUT UINT64* KernelSize\n    )\n{\n    if (ImagePath == NULL || KernelAddress == NULL || KernelSize == NULL) {\n        return EFI_INVALID_PARAMETER;\n    }\n    \n    Print(L\"Loading kernel from: %s\\n\", ImagePath);\n    \n    // In a real implementation, we would:\n    // 1. Locate the kernel image file using UEFI file system protocols\n    // 2. ...

C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\main.c:
  Line 98:   Status = PpcRunSelfTest();

  Line 117:     Status = PpcGetGuestMemoryRegion(&HostBase, &GuestBase, &GuestSize);

  Line 122:     Status = PpcSetGuestMemory(HostBase, (UINT32)GuestBase, (UINT32)GuestSize);

  Line 137:     Status = PpcGetGuestMemoryRegion(&DemoHostBase, &DemoGuestBase, &DemoGuestSize);


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c:
  Line 119:     Status = PpcGetGuestMemoryRegion(&GuestBuffer, &GuestBase, &GuestSize);

  Line 195: PpcLoadBootImage (

  Line 282: PpcLoadSystemRom (

  Line 328:     EFI_STATUS Status = PpcGetGuestMemoryRegion(&GuestBuffer, &GuestBase, &GuestSize);

  Line 400: PpcPrepareSystemForBoot (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\hardware\abstraction_impl.c:
  Line 222:         EFI_STATUS FbStatus = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);

  Line 291:     EFI_STATUS Status = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader.h:
  Line 75: PpcLoadBootImage (

  Line 136: PpcLoadSystemRom (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory\manager_impl.c:
  Line 287: PpcGetGuestMemoryRegion (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c:
  Line 142: // When PpcSetGuestMemory() has been called, guest addresses inside

  Line 193: PpcSetGuestMemory (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory\manager.h:
  Line 151: PpcGetGuestMemoryRegion (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation.h:
  Line 179: PpcRunSelfTest (

  Line 221: PpcSetGuestMemory (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation_impl.c:
  Line 151: PpcRunSelfTest (

```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\translation_impl.c"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation_impl.c</path>
<type>file</type>
<content>
1: #include "interpreter.h"
2: #include "translation.h"
3: #include <efi.h>
4: #include <efilib.h>
5: 
6: // ---------------------------------------------------------------------------
7: // Context initialization
8: // ---------------------------------------------------------------------------
9: 
10: EFI_STATUS
11: EFIAPI
12: PpcInitializeTranslationContext (
13:     VOID
14:     )
15: {
16:     ZeroMem(&g_PpcContext, sizeof(g_PpcContext));
17:     g_PpcContext.Msr = 0;
18:     g_PpcContext.Pc = 0;
19:     return EFI_SUCCESS;
20: }
21: 
22: // ---------------------------------------------------------------------------
23: // Register access (public API backed by the interpreter context)
24: // ---------------------------------------------------------------------------
25: 
26: EFI_STATUS
27: EFIAPI
28: PpcGetRegisterValue (
29:     IN  UINT8  RegisterNumber,
30:     OUT UINT32* Value
31:     )
32: {
33:     if (Value == NULL) {
34:         return EFI_INVALID_PARAMETER;
35:     }
36: 
37:     if (RegisterNumber < 32) {
38:         *Value = g_PpcContext.Gpr[RegisterNumber];
39:         return EFI_SUCCESS;
40:     }
41: 
42:     switch (RegisterNumber) {
43:         case PPC_REG_MSR:  *Value = g_PpcContext.Msr;  return EFI_SUCCESS;
44:         case PPC_REG_SRR0: *Value = g_PpcContext.Srr0; return EFI_SUCCESS;
45:         case PPC_REG_SRR1: *Value = g_PpcContext.Srr1; return EFI_SUCCESS;
46:         case PPC_REG_CTR:  *Value = g_PpcContext.Ctr;  return EFI_SUCCESS;
47:         case PPC_REG_LR:   *Value = g_PpcContext.Lr;   return EFI_SUCCESS;
48:         case PPC_REG_CR:   *Value = g_PpcContext.Cr;   return EFI_SUCCESS;
49:         case PPC_REG_XER:  *Value = g_PpcContext.Xer;  return EFI_SUCCESS;
50:         case PPC_REG_PC:   *Value = g_PpcContext.Pc;   return EFI_SUCCESS;
51:         default:           return EFI_INVALID_PARAMETER;
52:     }
53: }
54: 
55: EFI_STATUS
56: EFIAPI
57: PpcSetRegisterValue (
58:     IN UINT8  RegisterNumber,
59:     IN UINT32 Value
60:     )
61: {
62:     if (RegisterNumber < 32) {
63:         g_PpcContext.Gpr[RegisterNumber] = Value;
64:         return EFI_SUCCESS;
65:     }
66: 
67:     switch (RegisterNumber) {
68:         case PPC_REG_MSR:  g_PpcContext.Msr  = Value; return EFI_SUCCESS;
69:         case PPC_REG_SRR0: g_PpcContext.Srr0 = Value; return EFI_SUCCESS;
70:         case PPC_REG_SRR1: g_PpcContext.Srr1 = Value; return EFI_SUCCESS;
71:         case PPC_REG_CTR:  g_PpcContext.Ctr  = Value; return EFI_SUCCESS;
72:         case PPC_REG_LR:   g_PpcContext.Lr   = Value; return EFI_SUCCESS;
73:         case PPC_REG_CR:   g_PpcContext.Cr   = Value; return EFI_SUCCESS;
74:         case PPC_REG_XER:  g_PpcContext.Xer  = Value; return EFI_SUCCESS;
75:         case PPC_REG_PC:   g_PpcContext.Pc   = Value; return EFI_SUCCESS;
76:         default:           return EFI_INVALID_PARAMETER;
77:     }
78: }
79: 
80: UINT32
81: PpcGetGprValue (
82:     IN UINT8 RegisterNumber
83:     )
84: {
85:     if (RegisterNumber < 32) {
86:         return g_PpcContext.Gpr[RegisterNumber];
87:     }
88:     return 0;
89: }
90: 
91: VOID
92: PpcSetGprValue (
93:     IN UINT8 RegisterNumber,
94:     IN UINT32 Value
95:     )
96: {
97:     if (RegisterNumber < 32) {
98:         g_PpcContext.Gpr[RegisterNumber] = Value;
99:     }
100: }
101: 
102: // ---------------------------------------------------------------------------
103: // CPU self-test
104: // ---------------------------------------------------------------------------
105: 
106: STATIC UINTN g_SelfTestPasses    = 0;
107: STATIC UINTN g_SelfTestFailures  = 0;
108: 
109: STATIC VOID
110: SelfTestCheck (
111:     IN BOOLEAN Ok,
112:     IN CHAR16* Name
113:     )
114: {
115:     if (Ok) {
116:         g_SelfTestPasses++;
117:         Print(L"  [PASS] %s\n", Name);
118:     } else {
119:         g_SelfTestFailures++;
120:         Print(L"  [FAIL] %s\n", Name);
121:     }
122: }
123: 
124: // 256-byte window mapped at guest addresses 0x10000..0x100FF for load/store tests
125: STATIC UINT8 g_SelfTestMem[256];
126: 
127: STATIC UINT8
128: SelfTestReadByte (
129:     IN UINT32 Address
130:     )
131: {
132:     if (Address >= 0x10000 && Address < 0x10100) {
133:         return g_SelfTestMem[Address - 0x10000];
134:     }
135:     return 0;
136: }
137: 
138: STATIC VOID
139: SelfTestWriteByte (
140:     IN UINT32 Address,
141:     IN UINT8  Value
142:     )
143: {
144:     if (Address >= 0x10000 && Address < 0x10100) {
145:         g_SelfTestMem[Address - 0x10000] = Value;
146:     }
147: }
148: 
149: EFI_STATUS
150: EFIAPI
151: PpcRunSelfTest (
152:     VOID
153:     )
154: {
155:     UINT32   Next = 0;
156:     UINT32   RegVal = 0;
157:     EFI_STATUS Status;
158: 
159:     g_SelfTestPasses   = 0;
160:     g_SelfTestFailures = 0;
161: 
162:     Print(L"--- PowerPC CPU Self-Test ---\n");
163: 
164:     // Reset the CPU to a known state
165:     ZeroMem(&g_PpcContext, sizeof(g_PpcContext));
166: 
167:     // 1. addi r3, r0, 5
168:     Status = PpcExecuteInstruction(0x38600005, 0x1000, &Next);
169:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(3) == 5 && Next == 0x1004,
170:                   L"addi r3,r0,5 -> r3=5");
171: 
172:     // 2. addic r3, r3, -1 with r3=1 -> r3=0, XER[CA] set
173:     PpcSetGprValue(3, 1);
174:     g_PpcContext.Xer = 0;
175:     Status = PpcExecuteInstruction(0x3063FFFF, 0x1004, &Next);
176:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(3) == 0 &&
177:                   (g_PpcContext.Xer & PPC_XER_CA) != 0,
178:                   L"addic r3,r3,-1 -> r3=0, CA set");
179: 
180:     // 3. addic. r4, r3, 0 with r3=0 -> r4=0, CR0=EQ
181:     PpcSetGprValue(3, 0);
182:     Status = PpcExecuteInstruction(0x34830000, 0x1008, &Next);
183:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(4) == 0 &&
184:                   ((g_PpcContext.Cr >> 28) & 0xF) == PPC_CR_EQ,
185:                   L"addic. r4,r3,0 -> CR0=EQ");
186: 
187:     // 4. add r5, r3, r4 (r3=5, r4=3) -> r5=8
188:     PpcSetGprValue(3, 5);
189:     PpcSetGprValue(4, 3);
190:     Status = PpcExecuteInstruction(0x7CA32214, 0x100C, &Next);
191:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(5) == 8,
192:                   L"add r5,r3,r4 -> r5=8");
193: 
194:     // 5. subf r6, r4, r3 -> r6 = r3 - r4 = 2
195:     Status = PpcExecuteInstruction(0x7CC41850, 0x1010, &Next);
196:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(6) == 2,
197:                   L"subf r6,r4,r3 -> r6=2");
198: 
199:     // 6. ori r7, r0, 0xFF -> r7 = GPR0 | 0xFF = 0xFF
200:     PpcSetGprValue(0, 0);
201:     Status = PpcExecuteInstruction(0x600700FF, 0x1014, &Next);
202:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(7) == 0xFF,
203:                   L"ori r7,r0,0xFF -> r7=0xFF");
204: 
205:     // 7. cmp cr0, r3, r4 (r3=5 > r4=3) -> CR0=GT
206:     g_PpcContext.Xer = 0;
207:     Status = PpcExecuteInstruction(0x7C032000, 0x1018, &Next);
208:     SelfTestCheck(Status == EFI_SUCCESS && ((g_PpcContext.Cr >> 28) & 0xF) == PPC_CR_GT,
209:                   L"cmp r3,r4 (5>3) -> CR0=GT");
210: 
211:     // 8. cmp cr0, r3, r4 (r3=5 == r4=5) -> CR0=EQ
212:     PpcSetGprValue(4, 5);
213:     Status = PpcExecuteInstruction(0x7C032000, 0x101C, &Next);
214:     SelfTestCheck(Status == EFI_SUCCESS && ((g_PpcContext.Cr >> 28) & 0xF) == PPC_CR_EQ,
215:                   L"cmp r3,r4 (5==5) -> CR0=EQ");
216: 
217:     // 9. bc BO=16 (bdnz): CTR=2 -> taken, CTR becomes 1
218:     PpcSetRegisterValue(PPC_REG_CTR, 2);
219:     Status = PpcExecuteInstruction(0x42000008, 0x2000, &Next);
220:     PpcGetRegisterValue(PPC_REG_CTR, &RegVal);
221:     SelfTestCheck(Status == EFI_SUCCESS && Next == 0x2008 && RegVal == 1,
222:                   L"bc bdnz taken, CTR 2->1, next=+8");
223: 
224:     // 10. bc BO=16 again with CTR=1 -> not taken, CTR becomes 0
225:     Status = PpcExecuteInstruction(0x42000008, 0x2008, &Next);
226:     PpcGetRegisterValue(PPC_REG_CTR, &RegVal);
227:     SelfTestCheck(Status == EFI_SUCCESS && Next == 0x200C && RegVal == 0,
228:                   L"bc bdnz not taken, CTR 1->0, next=+4");
229: 
230:     // 11. bl +0 -> LR = 0x3004, next = 0x3000
231:     Status = PpcExecuteInstruction(0x48000001, 0x3000, &Next);
232:     PpcGetRegisterValue(PPC_REG_LR, &RegVal);
233:     SelfTestCheck(Status == EFI_SUCCESS && Next == 0x3000 && RegVal == 0x3004,
234:                   L"bl +0 -> LR=0x3004, next=current");
235: 
236:     // 12. rlwinm r8, r3, 1, 0, 31 (r3=1) -> r8=2
237:     PpcSetGprValue(3, 1);
238:     Status = PpcExecuteInstruction(0x5468081F, 0x3004, &Next);
239:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(8) == 2,
240:                   L"rlwinm r8,r3,1,0,31 -> r8=2");
241: 
242:     // 13. mtspr lr, r3 then mfspr r9, lr -> r9 = r3
243:     PpcSetGprValue(3, 0xDEADBEEF);
244:     Status = PpcExecuteInstruction(0x7C6803A6, 0x3008, &Next);
245:     SelfTestCheck(Status == EFI_SUCCESS, L"mtspr lr,r3");
246: 
247:     Status = PpcExecuteInstruction(0x7D2802A6, 0x300C, &Next);
248:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(9) == 0xDEADBEEF,
249:                   L"mfspr r9,lr -> r9=0xDEADBEEF");
250: 
251:     // 14. srawi r12, r10, 4 (r10=0x80000001) -> r12=0xF8000000, XER[CA] set
252:     PpcSetGprValue(10, 0x80000001);
253:     g_PpcContext.Xer = 0;
254:     Status = PpcExecuteInstruction(0x7D4C2670, 0x3010, &Next);
255:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(12) == 0xF8000000 &&
256:                   (g_PpcContext.Xer & PPC_XER_CA) != 0,
257:                   L"srawi r12,r10,4 -> 0xF8000000, CA set");
258: 
259:     // 15. Unsupported opcode must report EFI_UNSUPPORTED
260:     Status = PpcExecuteInstruction(0xE0000000, 0x3014, &Next);
261:     SelfTestCheck(Status == EFI_UNSUPPORTED,
262:                   L"unsupported opcode -> EFI_UNSUPPORTED");
263: 
264:     // 16. Load/store round-trip through the memory callbacks
265:     PpcSetMemoryAccess(SelfTestReadByte, SelfTestWriteByte);
266:     PpcSetGprValue(1, 0x10000);
267:     PpcSetGprValue(10, 0x12345678);
268:     Status = PpcExecuteInstruction(0x91410000, 0x3018, &Next);   // stw r10,0(r1)
269:     SelfTestCheck(Status == EFI_SUCCESS, L"stw r10,0(r1)");
270: 
271:     Status = PpcExecuteInstruction(0x81610000, 0x301C, &Next);   // lwz r11,0(r1)
272:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(11) == 0x12345678 &&
273:                   g_SelfTestMem[0] == 0x12 && g_SelfTestMem[1] == 0x34 &&
274:                   g_SelfTestMem[2] == 0x56 && g_SelfTestMem[3] == 0x78,
275:                   L"lwz r11,0(r1) -> 0x12345678 (big-endian)");
276:     PpcSetMemoryAccess(NULL, NULL);
277: 
278:     Print(L"--- Self-test complete: %d passed, %d failed ---\n",
279:           g_SelfTestPasses, g_SelfTestFailures);
280: 
281:     return (g_SelfTestFailures == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;
282: }
283: 
284: // ---------------------------------------------------------------------------
285: // Legacy placeholder functions (kept for API compatibility)
286: // ---------------------------------------------------------------------------
287: 
288: EFI_STATUS
289: EFIAPI
290: PpcTranslateInstruction (
291:     IN  UINT32  PpcInstruction,
292:     OUT UINT64* X86Instruction
293:     )
294: {
295:     if (X86Instruction == NULL) {
296:         return EFI_INVALID_PARAMETER;
297:     }
298: 
299:     // Placeholder: no dynamic x86-64 translation is performed yet. The
300:     // interpreter executes PowerPC instructions directly instead.
301:     *X86Instruction = PpcInstruction;
302:     return EFI_SUCCESS;
303: }
304: 
305: EFI_STATUS
306: EFIAPI
307: PpcExecuteTranslatedBlock (
308:     IN  UINT32* InstructionBlock,
309:     IN  UINTN   BlockSize
310:     )
311: {
312:     UINTN Executed = 0;
313: 
314:     if (InstructionBlock == NULL) {
315:         return EFI_INVALID_PARAMETER;
316:     }
317: 
318:     // Execute the block instruction-by-instruction through the interpreter.
319:     return PpcExecuteBlock(InstructionBlock, BlockSize / sizeof(UINT32), &Executed);
320: }
321: 
322: EFI_STATUS
323: EFIAPI
324: PpcHandleException (
325:     IN UINT32 ExceptionType,
326:     IN UINT32 ExceptionAddress
327:     )
328: {
329:     // Real 32-bit PowerPC exception semantics: the address of the faulting
330:     // instruction goes to SRR0, the MSR at exception time is saved to SRR1,
331:     // interrupts are disabled, and execution vectors to the exception handler.
332:     UINT32 Vector;
333:     switch (ExceptionType) {
334:         case PPC_EXCEPTION_INTERRUPT:   Vector = PPC_EXCEPTION_VECTOR_INTERRUPT;   break;
335:         case PPC_EXCEPTION_TRAP:        Vector = PPC_EXCEPTION_VECTOR_TRAP;        break;
336:         case PPC_EXCEPTION_SYSTEM_CALL: Vector = PPC_EXCEPTION_VECTOR_SYSTEM_CALL; break;
337:         default:
338:             Print(L"Unhandled PowerPC exception type: %d\n", ExceptionType);
339:             return EFI_UNSUPPORTED;
340:     }
341: 
342:     g_PpcContext.Srr0 = ExceptionAddress;
343:     g_PpcContext.Srr1 = g_PpcContext.Msr;
344:     g_PpcContext.Msr &= ~(PPC_MSR_EE | PPC_MSR_RI);
345:     g_PpcContext.Pc = Vector;
346:     g_PpcContext.ExceptionPending = 1;
347: 
348:     Print(L"PowerPC exception %d at 0x%x: SRR0=0x%x SRR1=0x%x PC=0x%x\n",
349:           ExceptionType, ExceptionAddress, g_PpcContext.Srr0,
350:           g_PpcContext.Srr1, g_PpcContext.Pc);
351: 
352:     return EFI_SUCCESS;
353: }

(End of file - total 353 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 188.2s)

_Thinking:_

Now I have full picture. Let me define the next objective clearly.

**Objective: Phase 5 Step 1 — Classic Mac OS Boot Process foundation (guest memory map + ROM loading + system initialization).**

Concretely, deliverables:

1. **Extend the interpreter to support multiple guest memory regions** (RAM + read-only ROM + low memory). Add `PpcAddGuestMemoryRegion()` to `translation.h` and implement in `interpreter.c` with a region table.

2. **Classic Mac OS memory map constants** in bootloader.h:
   - `PPC_ROM_GUEST_BASE` 0xFFF00000
   - `PPC_ROM_MAX_SIZE` 4MB
   - `PPC_RESET_VECTOR` 0xFFF00100
   - `PPC_LOW_MEM_GUEST_BASE` 0x00000000
   - `PPC_LOW_MEM_SIZE` 16KB
   - Default ROM path `\System\MacOS\ROM`, kernel path `\System\MacOS\kernel` (already used)

3. **Bootloader implementation of system initialization:**
   - `PpcInstallSystemRom(RomPath)`: load ROM file from volume → allocate pages → copy → wire into guest map as read-only region → return address/size.
   - `PpcInstallLowMemory()`: allocate a small low-memory region at guest 0x00000000, zero it.
   - `PpcPrepareSystemForBoot()`: implement for real — init memory map, install low-memory, configure CPU context (PC=reset vector, MSR=ME|RI, SRR0/SRR1, zero GPRs), write low-memory globals.
   - Add a `PpcRunBootSelfTest()` that verifies: regions present, ROM read-only enforcement, low-memory R/W, cross-region load/store through interpreter, CPU context boot config.
   - Update `PpcGetBootInfo` to report ROM/lowmem/memory map state.

4. **main.c**: after the existing kernel execution section, add the boot memory map + system init section:
   - Install low memory
   - Try installing ROM from volume (if file present)
   - If no ROM file, synthesize a small demo ROM in guest memory (like the existing GuestProgram demo but at 0xFFF00000) to keep the self-test verifiable in QEMU without needing a real ROM file.
   - Run boot self-test
   - Prepare system for boot (configure CPU context), print results.

Wait — the ROM region is read-only in the guest map. If main synthesizes a demo ROM, it must write to the ROM host buffer directly (before/outside the read-only enforcement), i.e., via the host pointer, not via PpcWriteGuestByte. That's fine — `PpcAddGuestMemoryRegion` takes a HostBase; main can write to HostBase directly using CopyMem/byte loops, and the read-only flag only affects guest accesses via the interpreter. Good.

Also, there's a subtlety: the reset vector 0xFFF00100 is inside the ROM. Guest code starting there: our synthesized ROM demo can place instructions at 0xFFF00100... but PC starts at 0xFFF00000 in some boot models. For the demo, I'll set PC to the ROM base where we place the program, and separately demonstrate the reset-vector constant.

Actually, let me make the synthesized ROM place a small program that:
- `lis r3, 0xFFF0` (0x3C60FFF0) → r3 = 0xFFF00000
- `lwz r4, 0(r3)` (0x80630000) → reads first word of ROM (which we set to a magic 0x524F4D31 'ROM1')
- `addi r5, r4, 1` (0x38840001) → r5 = magic+1
- `stw r5, 0(r1)` — store to RAM address in r1

Then verify: r4 == magic, r5 == magic+1, and the RAM store worked (cross-region). Set r1 to the low-memory region base (0x00000000) or to a RAM address. Let me use RAM address 0x10000000 (guest RAM base). Wait, need r1 pointing into RAM region. Guest RAM base = 0x10000000. Store at 0(r1) with r1 = 0x10000000. That's within RAM (256MB). Good.

Actually wait — there's an issue with executing at ROM address. `PpcExecuteBlock` sets `g_PpcContext.Pc = (UINT32)(UINTN)InstructionStream;` and executes from there, fetching via `CpuRead32(g_PpcContext.Pc)` which goes through `g_ReadByte` → default → region table. So I can pass `(UINT32*)0xFFF00000` as the InstructionStream pointer (a fake host pointer — it's just used to compute PC). Actually PpcExecuteBlock does `(UINT32)(UINTN)InstructionStream` to set Pc. Passing `(UINT32*)(UINTN)0xFFF00000` sets PC = 0xFFF00000. But then `PpcExecuteBlock` executes exactly MaxInstructions from that PC. It doesn't dereference InstructionStream directly. So passing a fake pointer works. Good — that's exactly how the existing kernel execution does it: `PpcExecuteBlock((UINT32*)(UINTN)EntryPoint, ...)`.

But careful: `PpcExecuteBlock` requires `InstructionStream != NULL`. `(UINT32*)(UINTN)0xFFF00000` is non-NULL. Fine.

One more: the `stw` to RAM region via guest store: RAM region is read/write. Low-memory region is read/write. Good.

Also, I should double check `PpcExecuteBlock` behavior with the block pointer - after computing Pc it fetches instructions using CpuRead32(Pc) which reads from the region table. 

Now, one concern: `PpcRunSelfTest` in translation_impl.c resets the CPU context and uses custom memory callbacks via PpcSetMemoryAccess, then restores with NULL. It does `ZeroMem(&g_PpcContext...)` at start. That happens before guest RAM wiring in main. Fine.

But note: the boot self-test will need to be careful about memory callbacks. The default read/write will use region table after `PpcAddGuestMemoryRegion` calls.

Also `PpcInitializeTranslationContext` zeroes the context; `PpcSetupBootEnvironment` also calls it (called in main at line 212 before graphics). Then `PpcPrepareSystemForBoot` will re-configure. Order in main:
1. PpcInitializeUefiInterface
2. PpcInitializeDebug
3. PpcInitializeTranslationContext
4. PpcRunSelfTest
5. PpcInitializeMemoryManager
6. wire guest memory
7. guest program demo
8. PpcInitializeHardwareAbstraction
9. PpcInitializeBootloader
10. PpcSetupBootEnvironment (re-init translation + memory manager (no-op since already inited) + hardware + graphics)
11. PpcInitializeGraphics
12. graphics self-check, multi-frame
13. PpcInitializeAudio
14. audio self-check
15. PpcInitializeStorage + BlockIo self-check
16. PpcInitializeNetwork + network self-check
17. system info
18. debug log to file
19. kernel load/verify/execute + PpcBootSystem
20. return

I'll insert the boot memory map / ROM / system-init section after the kernel section (step 19), as a new "Phase 5: Boot memory map & system initialization" block. This keeps existing behavior intact.

Actually, better placement: the ROM install and low-memory install should happen during/around bootloader init or just before `PpcPrepareSystemForBoot`. But `PpcPrepareSystemForBoot` is currently called? Let me check main.c: `PpcSetupBootEnvironment()` is called but `PpcPrepareSystemForBoot` is NOT called in main. I'll add a call to `PpcPrepareSystemForBoot` in the new section. And I'll add `PpcInstallSystemRom` + `PpcInstallLowMemory` calls.

Let me also add a `PpcRunBootSelfTest` to bootloader (a self-contained test like the CPU one, with pass/fail counters) and call it in main.

Now, about the ROM file: main should try `PpcInstallSystemRom(L"\\System\\MacOS\\ROM")`; if EFI_NOT_FOUND (file absent), synthesize the demo ROM. On the QEMU test ESP, there's likely no `\System\MacOS\ROM`, so the demo path will be exercised. If someone places a real ROM file, it'll be loaded instead. I'll write the ROM-loading path to work with a real file too.

Wait, `PpcLoadKernel` uses `\System\MacOS\kernel`. In the QEMU test setup (BUILD_INSTRUCTIONS), they just put BOOTX64.EFI on the ESP. So there's no kernel file → kernel section prints "Kernel not found". Similarly ROM file won't exist → demo path. Good.

Now let me also consider: **the interpreter region table must be re-entrant with the memory manager.** The RAM region is added via `PpcSetGuestMemory` (region 0). The low-memory region at 0x00000000, and ROM at 0xFFF00000, added via `PpcAddGuestMemoryRegion`. Address spaces don't overlap. Lookup order: iterate regions in order; for low memory at 0x00000000 no conflict with RAM at 0x10000000.

Edge: `PpcAddGuestMemoryRegion` should handle region 0 being the RAM region (so index 0 reserved). I'll implement it as: find first inactive slot. Also cap PPC_MAX_GUEST_REGIONS. Also guard against overflow when computing end addresses: use UINT64 math: `Address >= GuestBase && (UINT64)(Address - GuestBase) < Size`. Since Address is UINT32 and GuestBase is UINT32, Address - GuestBase underflows if Address < GuestBase — but I guard with `Address >= GuestBase` first (short-circuit). Good, mirror existing code.

Now `PpcReadGuestByte`/`PpcWriteGuestByte` call `g_ReadByte`/`g_WriteByte`. If a custom callback was set (as in self-test), those bypass the region table — that's existing behavior, fine. After `PpcSetMemoryAccess(NULL,NULL)` the defaults are restored.

Now, about `PpcPrepareSystemForBoot` being in bootloader_impl.c but not declared in the header — I'll add its declaration plus new functions to bootloader.h.

Let me also reconsider: The kernel boot path at the end of main.c calls `PpcBootSystem(KernelAddress, KernelSize)` only if kernel loaded. And `PpcBootSystem` requires `g_BootContext.KernelLoaded`. OK.

Now `PpcPrepareSystemForBoot` should NOT overwrite what `PpcBootSystem` does when kernel is loaded, but it runs regardless (before or after). Since it configures PC to the ROM reset vector, and then if kernel loads later `PpcBootSystem` sets PC to kernel address, order matters: run `PpcPrepareSystemForBoot` AFTER the kernel section. Actually cleaner: run it in the new Phase-5 section at the very end, after the kernel handling. But conceptually "prepare system for boot" should come before kernel execution... 

Hmm. Let me think about the logical flow for classic Mac OS:
1. Memory map established (RAM, ROM, low mem).
2. System initialized (CPU context configured for ROM boot).
3. ROM executes → loads Mac OS ROM file → loads kernel → starts OS.

In the current emulator, there's a hybrid: kernel loaded from volume directly. So the final sequence:
- Install ROM (real or demo).
- Prepare system for boot (configure CPU context for ROM entry).
- If kernel file present, load/verify/execute it (existing).
- Boot self-test to validate the memory map + system init.

I'll place the new section AFTER the kernel section but order operations so it reads naturally. Actually, to minimize disruption and keep the existing verified output, I'll append the Phase 5 section right before the final `Print(L"\n=== EFI-Mac-Emulator Ready ===\n");`. Inside:
1. `PpcInstallLowMemory()` → report.
2. `PpcInstallSystemRom("\\System\\MacOS\\ROM")` → if not found, `PpcInstallDemoRom()` (synthesize) → report.
3. `PpcRunBootSelfTest()` → pass/fail.
4. `PpcPrepareSystemForBoot()` → report CPU context (PC, MSR, SRR0/SRR1).

That ordering is fine: prepare system AFTER loading ROM, so PC can be the ROM reset vector.

Also update `PpcGetBootInfo` to expose the new state (RomLoaded, RomAddress, RomSize, LowMemoryInstalled, MemoryMapReady). I'll extend PPC_BOOT_INFO struct — but it's in bootloader.h. Adding fields is backward compatible (callers use PpcGetBootInfo only). Actually main.c doesn't call PpcGetBootInfo. Fine to extend.

Now, about `PpcLoadSystemRom` — it loads into a pool buffer. For ROM up to 4MB, pool allocation fails (UEFI pool allocations > ~128KB typically fail). I'll change `PpcLoadSystemRom` to allocate pages via `BS->AllocatePages` when size is large, returning the page-aligned buffer. It returns `VOID** RomBuffer` so caller just gets a buffer to free with FreePages. Let me keep it simple and robust:
- Get file size first.
- Pages = (Size + 0xFFF) >> 12; AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Phys).
- Read file into the buffer.
- Return Phys as buffer.

But hold on — `PpcLoadSystemRom` and `PpcInstallSystemRom` overlap. Let me design `PpcInstallSystemRom` to do the load internally (reusing the pattern from PpcLoadKernel) and keep `PpcLoadSystemRom` as a public file-loader (page-based). Both declared in header. `PpcInstallSystemRom` returns RomAddress/RomSize and stores in context. It can call `PpcLoadSystemRom` to get the buffer, then copy into a page-allocated ROM region wired read-only, then FreePages the temp. That's clean:

```c
EFI_STATUS PpcInstallSystemRom(IN CHAR16* RomPath, OUT UINT64* RomAddress, OUT UINT64* RomSize);
```
- load via PpcLoadSystemRom into temp buffer (pages)
- validate size <= PPC_ROM_MAX_SIZE
- allocate the ROM backing region (pages), CopyMem
- PpcAddGuestMemoryRegion(RomHost, PPC_ROM_GUEST_BASE, RomSize, TRUE /*read-only*/)
- FreePages temp
- store in context: RomLoaded, RomAddress=PPC_ROM_GUEST_BASE, RomSize
- Note: host buffer address retained in context for demo-writing? No — demo writes via host buffer before wiring, or via direct host pointer. For PpcInstallDemoRom, main writes to the host buffer directly. But `PpcAddGuestMemoryRegion` is what wires it. Let me have the demo path allocate pages, fill them (before wiring), then wire read-only. I'll write a helper in bootloader: `PpcInstallDemoRom` that creates a synthetic 4-instruction ROM at PPC_ROM_GUEST_BASE, wires it read-only, returns address/size. main calls it.

Actually, cleaner: expose one function `PpcInstallSystemRom(RomPath, ...)` where RomPath==NULL means "install demo ROM". Then main calls with real path, and on NOT_FOUND calls with NULL. Hmm, that conflates loading and demo. I'll keep separate: `PpcInstallSystemRom(RomPath)` and `PpcInstallDemoRom()`. Both wire read-only regions and set context.

But wait: If a ROM is already installed, `PpcAddGuestMemoryRegion` adds another region at same base → lookup order first match wins. To be safe, I'll track `g_BootContext.RomLoaded` and skip re-install, and add a guard in the region table against duplicate guest bases (return EFI_ALREADY_STARTED). Good.

Low memory: `PpcInstallLowMemory()` — allocate 16KB pages, zero, wire read/write at 0x00000000, store in context. If already installed, skip.

Now for the demo ROM content. 4 instructions at 0xFFF00000:
```
lis r3, 0xFFF0      -> 0x3C60FFF0   r3 = 0xFFF00000
lwz r4, 0(r3)       -> 0x80630000   r4 = [ROM] = magic
addi r5, r4, 1      -> 0x38840001   r5 = magic + 1
stw r5, 0(r1)       -> 0x90810000   store to RAM (r1 pre-set to RAM base)
```
magic word at ROM offset 0 = 0x524F4D31 ("ROM1"). 

Verify: r4 == 0x524F4D31, r5 == 0x524F4D32, RAM[0x10000000] == 0x524F4D32 (via PpcReadGuestByte), and ROM write dropped: attempt `PpcWriteGuestByte(0xFFF00000, 0xEE)` → read back still 0x52. 

Wait — is `lis r3, 0xFFF0` → r3 = 0xFFF00000. Yes (addis with SIMM 0xFFF0 sign-extends to 0xFFFFF000, <<16 = 0xFFF00000). 0x3C60FFF0: opcode 15 (addis), RD=3 (bits 21-25 = 00011 → 0x3C00 | 3<<16? let me compute: op 15 << 26 = 0x3C000000, RT=3 << 21 = 0x00600000, RA=0, SIMM=0xFFF0 → 0x3C60FFF0. ✓ matches existing GuestProgram's addi pattern style.

`lwz r4, 0(r3)`: op 32 << 26 = 0x80000000, RT=4<<21 = 0x00800000, RA=3<<16 = 0x00030000, D=0 → 0x80830000. Wait op 32: 32<<26 = 0x80000000. RT=4 → bits 21-25 = 00100 → 0x00800000. RA=3 → 0x00030000. D=0. → 0x80830000. ✓

`addi r5, r4, 1`: op 14<<26 = 0x38000000, RT=5<<21=0x00A00000, RA=4<<16=0x00040000, SIMM=1 → 0x38A40001. ✓

`stw r5, 0(r1)`: op 36<<26 = 0x90000000, RS=5<<21=0x00A00000, RA=1<<16=0x00010000, D=0 → 0x90A10000. ✓ (matches existing stw pattern 0x90A10000 in GuestProgram.)

magic first word stored at host buffer start: bytes 52 4F 4D 31.

Now let me also think about whether the interpreter default read path honors the region table AFTER PpcRunSelfTest restored callbacks to default. Yes: default callbacks read region table. Region 0 = RAM (wired in main earlier). Regions added by bootloader.

Potential issue: `PpcSetupBootEnvironment` calls `PpcInitializeTranslationContext()` which does `ZeroMem(&g_PpcContext, ...)` — this doesn't touch the region table (separate statics). Good.

Now the region table lives in interpreter.c. Adding `PpcAddGuestMemoryRegion` to translation.h. It needs to be callable from bootloader_impl.c. Good.

Let me also add `PpcGetGuestMemoryRegion`-like getter for ROM? Not needed; context holds it.

Now let me also make sure the ROM is actually fetched by the interpreter for instruction fetch at PC=0xFFF00100. In the demo, instructions start at 0xFFF00000 (region base). I'll execute from PC=0xFFF00000. The reset vector constant PPC_RESET_VECTOR = 0xFFF00100 is used for `PpcPrepareSystemForBoot` (PC set there) and reported. For the demo execution test I execute the 4 instructions at 0xFFF00000 directly (like the existing GuestProgram demo executes from ProgramBase). That's fine — the boot self-test verifies execution from ROM region, and the "system init" reports PC = reset vector.

Hmm, but then if PC is set to 0xFFF00100 and we never run it, it's just configuration. Fine. But there's an inconsistency: I could place the demo program AT 0xFFF00100 to make execution start at the real reset vector. Let me do that: demo program at ROM base + 0x100, i.e., write the 4 instructions to host buffer offset 0x100, and execute from (UINT32*)(UINTN)(0xFFF00100). And magic at offset 0. That's more authentic and demonstrates the reset vector concept.

Actually `lwz r4, 0(r3)` with r3 = 0xFFF00000 reads offset 0 (magic). And instructions at 0xFFF00100. PC for execute = 0xFFF00100. Nice.

Wait, but `PpcExecuteBlock` takes InstructionStream pointer only to derive PC. I'll pass `(UINT32*)(UINTN)PPC_RESET_VECTOR`. 

Now let me define the boot self-test `PpcRunBootSelfTest` in bootloader_impl.c. It should:
- Print header.
- Check low memory installed (context + region presence via PpcReadGuestByte(0)?? Actually reading guest byte 0 via interpreter goes to region table — but memory callbacks might be custom? No, defaults active). Write to low memory via PpcWriteGuestByte(0x00000004, 0xAA), read back → check.
- Check ROM installed: PpcReadGuestByte(PPC_ROM_GUEST_BASE) == 0x52 (first magic byte), etc.
- Check ROM read-only: PpcWriteGuestByte(PPC_ROM_GUEST_BASE, 0xEE) then verify byte still 0x52.
- Cross-region execution: run the demo program (needs r1 set to RAM base). Set g_PpcContext.Gpr[1] = (UINT32)GuestRamBase (via PpcSetGprValue(1, ...)), r3=r4=r5=0. Execute 4 instructions from reset vector. Check r4==0x524F4D31, r5==0x524F4D32, and RAM word at guest RAM base == 0x524F4D32.
- Check CPU boot state after PpcPrepareSystemForBoot? The self-test runs before prepare? I'll order: install ROM → self-test (execution, memory map) → prepare system (CPU config) → print state. Actually the execution test needs r1 set, which PpcPrepareSystemForBoot would reset. So self-test before prepare. Good.

Let me define pass/fail counters local to boot self-test, similar style.

Edge: The self-test needs to read the RAM region base. Get it via `PpcGetGuestMemoryRegion(NULL, &GuestBase, &GuestSize)` — memory manager function. Good.

Now, regarding `PpcInstallDemoRom`: allocate pages for ROM (4MB? we only need ~512 bytes; but the region Size affects mapping bounds — size can be small). I'll allocate 1 page (4KB) for demo ROM and wire Size = 0x1000, guest base 0xFFF00000. Reset vector 0xFFF00100 within. OK. But PPC_ROM_MAX_SIZE refers to real ROM file limit (4MB). Demo uses 4KB.

Actually, should the demo region size be PPC_ROM_MAX_SIZE for consistency of the memory map? The map says ROM window 0xFFF00000 size 4MB. If demo only maps 4KB, reads beyond return 0 — acceptable. I'll keep demo at 4KB but define window size as max 4MB for the report. Hmm, to keep the "guest memory map" coherent I'll make the demo allocate the full 4MB window? That's wasteful but simple and coherent, and matches "ROM window". Let me allocate Pages = PPC_ROM_MAX_SIZE pages (1024 pages = 4MB). Zero it, write program at +0x100. That gives a clean 4MB read-only ROM window. 4MB allocation is fine in UEFI. Good — coherent map.

For real ROM install: if file size ≤ PPC_ROM_MAX_SIZE, allocate the file-size pages only. OK.

Now about `PpcPrepareSystemForBoot` writing low-memory globals: classic Mac low memory has things like `Siv` (System Information Vector) at 0x0100-0x02FF, "BootInfo", etc. I'll write a small boot info block: at lowmem base + 0, a magic `0xEF4D4143` ('EFMAC')? Let me define offsets:
- `PPC_LOW_MEM_MAGIC_OFFSET 0x0000` — 4-byte magic 0x45464921 ("EFI!")
- `PPC_LOW_MEM_BOOTINFO_OFFSET 0x0100` — copy of boot info (RAM base, RAM size, ROM base, ROM size, video mode)
That's a nice, simple, testable system-init routine: PpcPrepareSystemForBoot writes these; the self-test can verify. Actually I'll verify in the main section, not necessarily in self-test.

Let me define a `PPC_GUEST_MEMORY_MAP` struct to report in PpcGetBootInfo:
```c
typedef struct {
    UINT64 RamBase; UINT64 RamSize;
    UINT64 RomBase; UINT64 RomSize; BOOLEAN RomInstalled;
    UINT64 LowMemBase; UINT64 LowMemSize; BOOLEAN LowMemoryInstalled;
    BOOLEAN Ready;
} PPC_GUEST_MEMORY_MAP;
```
And extend PPC_BOOT_INFO with `PPC_GUEST_MEMORY_MAP MemoryMap; BOOLEAN SystemReady;`.

Hmm, but adding a struct typedef mid-header is fine.

Let me now also handle: `PpcPrepareSystemForBoot` resets GPRs and sets PC. It should be idempotent.

Now, careful with **`-Wall -Werror`**. Things that could trip:
- Unused parameters/variables.
- Sign-compare warnings: e.g., `if (Address >= g_GuestMemoryGuestBase && (Address - g_GuestMemoryGuestBase) < g_GuestMemorySize)` — existing code does this and compiles, since UINT32 comparisons. OK.
- The region struct: `UINT32 GuestBase; UINT32 Size;` with UINT32 Address. `(Address - GuestBase)` is UINT32. Fine.
- Format strings: GNU-EFI Print uses `%d` for UINTN/UINT32, `%x` for hex, `%r` for status, `%s` for CHAR16*. For UINT64 I should use `%ld`? GNU-EFI's Print supports `%d`, `%x`, `%X`, `%s`, `%r`, `%p`. For 64-bit, gnu-efi print has `%ld`? Actually gnu-efi's Print in newer versions supports `%d` promoted... Existing code uses `Print(L"... %d bytes\n", FileSize)` where FileSize is UINT64 — and it compiled. So `%d` handles 64-bit on x86_64 (varargs). I'll follow existing patterns.

- `PpcExecuteBlock` param `InstructionStream` is `UINT32*`; passing `(UINT32*)(UINTN)PPC_RESET_VECTOR` is fine.

- In bootloader_impl.c I need `<efi.h>`, `<efilib.h>` for AllocatePages, CopyMem, ZeroMem, FreePages — already included (efilib.h). Need `cpu/translation.h` for PpcSetGprValue/PpcReadGuestByte/PpcWriteGuestByte/PpcAddGuestMemoryRegion/PpcGetGprValue/PpcExecuteBlock — already included. Need memory/manager.h for PpcGetGuestMemoryRegion — already included.

- BS->AllocatePages signature: `(EFI_ALLOCATE_TYPE Type, EFI_MEMORY_TYPE MemoryType, UINTN Pages, EFI_PHYSICAL_ADDRESS* Memory)`. On x86_64, EFI_PHYSICAL_ADDRESS is UINT64. Fine.

Let me now also think about whether to update docs. TODO.md should get a Phase 5 progress update. README.md "Current Status" is stale but FINAL_SUMMARY.md says it's a corrected summary... I'll update TODO.md to reflect Phase 5 step 1 completion, and add a brief note. I'll also update the README Current Status minimally? The README says pre-alpha scaffolding which is stale relative to Phase 3/4 already, so I won't touch README broadly; but I can add a Phase 5 note to TODO. Keep scope tight: TODO.md update + final status.

Also, the transcripts: Session_2_Transcript.md seems to contain old transcripts. Not required to update.

Now — one important verification concern: I cannot compile here. I must be extremely careful. Let me write code conservatively, avoiding constructs likely to warn. Let me be meticulous.

Let me plan the exact edits:

### Edit 1: `src/cpu/translation.h`
Add after `PpcSetGuestMemory` declaration (around line 225) a new function declaration:
```c
/**
  Map an additional guest memory region into the interpreter's memory path.

  The primary guest RAM region is installed with PpcSetGuestMemory(). This
  adds further regions (e.g. the classic Mac ROM window or low-memory
  globals). Regions are looked up in insertion order; the first region that
  contains an address wins. A read-only region honors guest loads but drops
  guest stores.

  @param[in] HostBase   Host virtual address backing the region
  @param[in] GuestBase  Guest-visible base address of the region
  @param[in] Size       Size of the region in bytes
  @param[in] ReadOnly   TRUE if guest stores to the region must be dropped
  @retval EFI_SUCCESS          Region installed
  @retval EFI_INVALID_PARAMETER  HostBase is NULL or Size is zero
  @retval EFI_OUT_OF_RESOURCES  No free region slot
  @retval EFI_ALREADY_STARTED   A region already covers GuestBase
**/
EFI_STATUS
EFIAPI
PpcAddGuestMemoryRegion (
    IN VOID*  HostBase,
    IN UINT32 GuestBase,
    IN UINT32 Size,
    IN BOOLEAN ReadOnly
    );
```

### Edit 2: `src/cpu/interpreter.c`
Replace the single-region globals with a region table, update default read/write, update PpcSetGuestMemory, add PpcAddGuestMemoryRegion.

Current code (lines 146-179):
```c
static VOID*  g_GuestMemoryHostBase  = NULL;
static UINT32 g_GuestMemoryGuestBase = 0;
static UINT32 g_GuestMemorySize      = 0;

static UINT8
PpcDefaultReadByte (
    IN UINT32 Address
    )
{
    if (g_GuestMemoryHostBase != NULL &&
        Address >= g_GuestMemoryGuestBase &&
        (Address - g_GuestMemoryGuestBase) < g_GuestMemorySize) {
        return *(volatile UINT8*)((UINTN)g_GuestMemoryHostBase +
                                  (Address - g_GuestMemoryGuestBase));
    }
    return 0;  // Unmapped guest address reads as zero
}

static VOID
PpcDefaultWriteByte (
    IN UINT32 Address,
    IN UINT8  Value
    )
{
    if (g_GuestMemoryHostBase != NULL &&
        Address >= g_GuestMemoryGuestBase &&
        (Address - g_GuestMemoryGuestBase) < g_GuestMemorySize) {
        *(volatile UINT8*)((UINTN)g_GuestMemoryHostBase +
                           (Address - g_GuestMemoryGuestBase)) = Value;
    }
}

static PPC_CPU_READ_MEMORY  g_ReadByte  = PpcDefaultReadByte;
static PPC_CPU_WRITE_MEMORY g_WriteByte = PpcDefaultWriteByte;
```

New:
```c
#define PPC_MAX_GUEST_REGIONS 4

typedef struct {
    VOID*   HostBase;   // Host virtual address backing the region
    UINT32  GuestBase;  // Guest-visible base address of the region
    UINT32  Size;       // Region size in bytes
    BOOLEAN ReadOnly;   // TRUE: guest stores are dropped
    BOOLEAN Active;     // TRUE: slot is in use
} PPC_GUEST_REGION;

// Guest memory map: region[0] is the primary guest RAM installed by
// PpcSetGuestMemory(); additional regions (ROM window, low-memory globals)
// are added with PpcAddGuestMemoryRegion(). Lookup is in insertion order.
static PPC_GUEST_REGION g_GuestRegions[PPC_MAX_GUEST_REGIONS];

static UINT8
PpcDefaultReadByte (
    IN UINT32 Address
    )
{
    UINTN I;
    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
        if (g_GuestRegions[I].Active &&
            Address >= g_GuestRegions[I].GuestBase &&
            (UINT64)(Address - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) {
            return *(volatile UINT8*)((UINTN)g_GuestRegions[I].HostBase +
                                      (Address - g_GuestRegions[I].GuestBase));
        }
    }
    return 0;  // Unmapped guest address reads as zero
}

static VOID
PpcDefaultWriteByte (
    IN UINT32 Address,
    IN UINT8  Value
    )
{
    UINTN I;
    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
        if (g_GuestRegions[I].Active &&
            Address >= g_GuestRegions[I].GuestBase &&
            (UINT64)(Address - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) {
            if (!g_GuestRegions[I].ReadOnly) {
                *(volatile UINT8*)((UINTN)g_GuestRegions[I].HostBase +
                                   (Address - g_GuestRegions[I].GuestBase)) = Value;
            }
            return;  // First matching region decides the target
        }
    }
}
```

Update PpcSetGuestMemory:
```c
EFI_STATUS
PpcSetGuestMemory (
    IN VOID*  HostBase,
    IN UINT32 GuestBase,
    IN UINT32 Size
    )
{
    return PpcAddGuestMemoryRegion(HostBase, GuestBase, Size, FALSE);
}
```

Add PpcAddGuestMemoryRegion:
```c
EFI_STATUS
PpcAddGuestMemoryRegion (
    IN VOID*   HostBase,
    IN UINT32  GuestBase,
    IN UINT32  Size,
    IN BOOLEAN ReadOnly
    )
{
    UINTN I;

    if (HostBase == NULL || Size == 0) {
        return EFI_INVALID_PARAMETER;
    }

    // Reject a region that would overlap an existing one.
    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
        if (!g_GuestRegions[I].Active) {
            continue;
        }
        if (GuestBase < g_GuestRegions[I].GuestBase) {
            if ((UINT64)GuestBase + Size > g_GuestRegions[I].GuestBase) {
                return EFI_ALREADY_STARTED;
            }
        } else {
            if ((UINT64)(GuestBase - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) {
                return EFI_ALREADY_STARTED;
            }
        }
    }

    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
        if (!g_GuestRegions[I].Active) {
            g_GuestRegions[I].HostBase  = HostBase;
            g_GuestRegions[I].GuestBase = GuestBase;
            g_GuestRegions[I].Size      = Size;
            g_GuestRegions[I].ReadOnly  = ReadOnly;
            g_GuestRegions[I].Active    = TRUE;
            return EFI_SUCCESS;
        }
    }

    return EFI_OUT_OF_RESOURCES;
}
```

Wait, the overlap logic: `(UINT64)GuestBase + Size > g_GuestRegions[I].GuestBase` — GuestBase+Size as UINT64 avoids wrap. If new region starts below existing base, overlap if new end > existing base. If new region starts at or above existing base, overlap if (GuestBase - existing base) < existing size. Good. This correctly rejects the RAM region vs demo ROM (no overlap). Good.

Note: `PpcSetGuestMemory` is called in main with HostBase possibly equal to the guest base (identity mapping). Fine.

Also the existing comment block above (lines 139-145) about "Emulated guest RAM (default memory backing...)" — I'll keep but update wording to mention regions. Let me keep edits minimal but accurate.

### Edit 3: `src/boot/bootloader.h`
Add constants:
```c
// Classic Mac OS PPC boot memory map (guest-visible addresses)
#define PPC_ROM_GUEST_BASE      0xFFF00000  // ROM window (classic PPC Macs)
#define PPC_ROM_MAX_SIZE        0x00400000  // 4 MB ROM window
#define PPC_ROM_DEFAULT_PATH    L"\\System\\MacOS\\ROM"
#define PPC_RESET_VECTOR        (PPC_ROM_GUEST_BASE + 0x100)
#define PPC_LOW_MEM_GUEST_BASE  0x00000000  // Low-memory globals
#define PPC_LOW_MEM_SIZE        0x4000      // 16 KB

// Low-memory global offsets (emulator-defined boot info block)
#define PPC_LOW_MEM_MAGIC_OFFSET        0x0000
#define PPC_LOW_MEM_BOOTINFO_OFFSET     0x0100
```

Add struct PPC_GUEST_MEMORY_MAP and extend PPC_BOOT_INFO:
```c
typedef struct {
    BOOLEAN RomInstalled;
    UINT64  RomBase;
    UINT64  RomSize;
    BOOLEAN LowMemoryInstalled;
    UINT64  LowMemoryBase;
    UINT64  LowMemorySize;
    BOOLEAN Ready;
} PPC_GUEST_MEMORY_MAP;
```
Extend PPC_BOOT_INFO:
```c
typedef struct {
    BOOLEAN IsInitialized;
    EFI_PHYSICAL_ADDRESS KernelAddress;
    UINT64 KernelSize;
    BOOLEAN KernelLoaded;
    BOOLEAN SystemReady;
    PPC_GUEST_MEMORY_MAP MemoryMap;
} PPC_BOOT_INFO;
```

Add declarations:
```c
EFI_STATUS EFIAPI PpcInstallSystemRom (IN CHAR16* RomPath, OUT UINT64* RomAddress, OUT UINT64* RomSize);
EFI_STATUS EFIAPI PpcInstallDemoRom (OUT UINT64* RomAddress, OUT UINT64* RomSize);
EFI_STATUS EFIAPI PpcInstallLowMemory (OUT UINT64* LowMemAddress, OUT UINT64* LowMemSize);
EFI_STATUS EFIAPI PpcRunBootSelfTest (VOID);
EFI_STATUS EFIAPI PpcPrepareSystemForBoot (VOID);
```

### Edit 4: `src/boot/bootloader_impl.c`
- Extend PPC_BOOTLOADER_CONTEXT with ROM/lowmem fields.
- Update PpcInitializeBootloader to zero new fields.
- Rewrite PpcLoadSystemRom to page-allocate.
- Implement PpcInstallSystemRom, PpcInstallDemoRom, PpcInstallLowMemory.
- Implement PpcRunBootSelfTest.
- Rewrite PpcPrepareSystemForBoot (real implementation).
- Update PpcGetBootInfo to fill new fields.

Let me now also double check `PpcLoadSystemRom` current callers: none (only definition). So I can change implementation freely. Signature unchanged: `(CHAR16* RomPath, VOID** RomBuffer, UINT64* RomSize)`. I'll make it allocate pages.

Let me write new PpcLoadSystemRom:
```c
EFI_STATUS
PpcLoadSystemRom (
    IN  CHAR16* RomPath,
    OUT VOID**  RomBuffer,
    OUT UINT64* RomSize
    )
{
    EFI_STATUS Status;
    UINTN  Size = 0;
    VOID*  Buffer = NULL;

    if (RomPath == NULL || RomBuffer == NULL || RomSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Loading system ROM from: %s\n", RomPath);

    EFI_FILE_IO_INTERFACE* Fs = NULL;
    Status = PpcGetFileSystem(&Fs, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = PpcLoadFile(Fs, RomPath, &Buffer, &Size);
    if (EFI_ERROR(Status)) {
        if (Buffer != NULL) {
            FreePool(Buffer);
        }
        return Status;
    }

    // UEFI pool allocations are limited (typically 128 KB); a real Mac OS ROM
    // can be several MB, so migrate the contents into a page-backed buffer.
    if (Size > PPC_ROM_MAX_SIZE) {
        FreePool(Buffer);
        Print(L"System ROM too large: %d bytes\n", Size);
        return EFI_LOAD_ERROR;
    }

    if (Size > 0x20000) {
        UINTN Pages = (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
        EFI_PHYSICAL_ADDRESS PagesBase = 0;
        Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &PagesBase);
        if (EFI_ERROR(Status)) {
            FreePool(Buffer);
            Print(L"Failed to allocate ROM pages: %r\n", Status);
            return Status;
        }
        CopyMem((VOID*)(UINTN)PagesBase, Buffer, Size);
        FreePool(Buffer);
        Buffer = (VOID*)(UINTN)PagesBase;
    }

    *RomBuffer = Buffer;
    *RomSize = Size;

    Print(L"System ROM loaded: %d bytes at 0x%x\n", Size, Buffer);

    return EFI_SUCCESS;
}
```

Hmm, but `PpcLoadFile` is declared in uefi_interface.h returning `PpcLoadFile(FileSystem, FileName, &Buffer, &Size)` where Size is `UINTN*`. OK. Check PpcLoadFile implementation to confirm it returns pool buffer and error behavior. Let me read uefi_interface_impl.c PpcLoadFile.

Also note: `PpcLoadFile` may fail to allocate for large files too (pool). But existing PpcLoadKernel reads directly into guest RAM with chunked Read, not via PpcLoadFile. For ROM, the file could be a few MB → PpcLoadFile might fail on pool. Better to read directly with chunked reads like PpcLoadKernel, into a page-allocated buffer. Let me write PpcLoadSystemRom to do direct chunked file read (like PpcLoadKernel) — more robust. I'll mirror PpcLoadKernel's file I/O but write into pages allocated up front. But we don't know size until GetInfo. GetInfo first, then allocate pages, then chunked read. Let me write it that way. It duplicates PpcLoadKernel logic but that's acceptable and robust.

Actually, I realize I can simplify: implement a small internal helper `BootReadFileToBuffer(Fs, RomPath, &Buffer, &Size)` that does GetInfo → AllocatePages → chunked read. Then PpcLoadSystemRom calls it. And PpcInstallSystemRom calls PpcLoadSystemRom. Clean.

But note: the buffer returned must be freed. If page-backed, free via FreePages (need original physical address + size → pages). I'll just document "caller frees with PpcFreeMemory" — hmm, PpcFreeMemory does BS->FreePages(physical, pages). Actually PpcFreeMemory(VirtualAddress, Size) frees pages. Since buffer is page-aligned (from AllocatePages) and physical==virtual in boot services, calling `PpcFreeMemory(Buffer, Size)` works. OK. I'll free the temp inside PpcInstallSystemRom via PpcFreeMemory.

Let me write helper:
```c
STATIC
EFI_STATUS
BootReadFileToPages (
    IN  CHAR16* FilePath,
    OUT VOID**  Buffer,
    OUT UINTN*  Size
    )
{
    EFI_FILE_IO_INTERFACE* Fs = NULL;
    EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_FILE_HANDLE Root = NULL;
    Status = Fs->OpenVolume(Fs, &Root);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_FILE_HANDLE File = NULL;
    Status = Root->Open(Root, &File, FilePath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || File == NULL) {
        Root->Close(Root);
        return (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
    }

    UINTN InfoSize = SIZE_OF_EFI_FILE_INFO + 260 * sizeof(CHAR16);
    EFI_FILE_INFO* Info = AllocateZeroPool(InfoSize);
    if (Info == NULL) {
        File->Close(File);
        Root->Close(Root);
        return EFI_OUT_OF_RESOURCES;
    }
    Status = File->GetInfo(File, &GenericFileInfo, &InfoSize, Info);
    if (EFI_ERROR(Status)) {
        FreePool(Info);
        File->Close(File);
        Root->Close(Root);
        return Status;
    }
    UINT64 FileSize = Info->FileSize;
    FreePool(Info);

    if (FileSize == 0 || FileSize > PPC_ROM_MAX_SIZE) {
        File->Close(File);
        Root->Close(Root);
        Print(L"File '%s' has invalid size %d for ROM load\n", FilePath, FileSize);
        return EFI_LOAD_ERROR;
    }

    UINTN Pages = (UINTN)((FileSize + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE);
    EFI_PHYSICAL_ADDRESS Base = 0;
    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        File->Close(File);
        Root->Close(Root);
        Print(L"Failed to allocate %d pages for '%s': %r\n", Pages, FilePath, Status);
        return Status;
    }

    VOID* Dst = (VOID*)(UINTN)Base;
    UINT64 Remaining = FileSize;
    UINTN  BytesRead = 0;
    BOOLEAN Failed = FALSE;
    while (Remaining > 0) {
        UINTN Chunk = (UINTN)Remaining;
        Status = File->Read(File, &Chunk, (UINT8*)Dst + BytesRead);
        if (EFI_ERROR(Status) || Chunk == 0) {
            Print(L"Failed while reading '%s': %r\n", FilePath, Status);
            Failed = TRUE;
            break;
        }
        BytesRead += Chunk;
        Remaining -= Chunk;
    }

    File->Close(File);
    Root->Close(Root);

    if (Failed) {
        BS->FreePages(Base, Pages);
        return EFI_LOAD_ERROR;
    }

    *Buffer = Dst;
    *Size = (UINTN)FileSize;
    return EFI_SUCCESS;
}
```

Careful with `%d` for UINT64 FileSize — existing code prints UINT64 with %d (FileSize in PpcLoadKernel line 108 `Print(L"Kernel image has invalid size %d\n", FileSize)`). OK, matches style.

Then:
```c
EFI_STATUS
PpcLoadSystemRom (
    IN  CHAR16* RomPath,
    OUT VOID**  RomBuffer,
    OUT UINT64* RomSize
    )
{
    VOID*  Buffer = NULL;
    UINTN  Size = 0;
    EFI_STATUS Status;

    if (RomPath == NULL || RomBuffer == NULL || RomSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Loading system ROM from: %s\n", RomPath);
    Status = BootReadFileToPages(RomPath, &Buffer, &Size);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    *RomBuffer = Buffer;
    *RomSize = (UINT64)Size;
    Print(L"System ROM loaded: %d bytes at 0x%x\n", Size, Buffer);
    return EFI_SUCCESS;
}
```

PpcInstallSystemRom:
```c
EFI_STATUS
PpcInstallSystemRom (
    IN  CHAR16* RomPath,
    OUT UINT64* RomAddress,
    OUT UINT64* RomSize
    )
{
    VOID*  Buffer = NULL;
    UINTN  Size = 0;
    EFI_STATUS Status;

    if (g_BootContext.RomLoaded) {
        if (RomAddress != NULL) { *RomAddress = g_BootContext.RomAddress; }
        if (RomSize != NULL) { *RomSize = g_BootContext.RomSize; }
        return EFI_ALREADY_STARTED;
    }

    Status = PpcLoadSystemRom(RomPath, &Buffer, (UINT64*)&Size);
```
Hmm — PpcLoadSystemRom takes `UINT64* RomSize`. Passing `&Size` (UINTN*) cast to UINT64* writes 8 bytes into a 4/8-byte UINTN — on x86_64 UINTN is 8 bytes so fine, but ugly. Better: 
```c
    UINT64 Size64 = 0;
    Status = PpcLoadSystemRom(RomPath, &Buffer, &Size64);
    if (EFI_ERROR(Status)) { return Status; }
    Size = (UINTN)Size64;
```

Then allocate ROM region pages (already have Buffer page-backed of Pages size). But I need to know Pages to free it. Instead of freeing, I'll directly wire `Buffer` as the ROM region host base (it's already page-backed, size ≥ file size). But region Size should equal file size or the window size? If I wire Size = file size, only the file content is mapped. That's fine and precise. But the region base address is guest 0xFFF00000 while the host buffer is at some arbitrary address — that's fine, mapping is by offset.

Wait, but if I wire Buffer (which contains the file at offset 0) at guest 0xFFF00000 with Size = file size, then guest reads at 0xFFF00000+i → host Buffer[i]. 

But there's a subtlety: the demo ROM and reset-vector program need instructions at guest 0xFFF00100. For a real ROM file, the reset handler would be wherever the ROM image places it; typically the first 0x100 bytes are the exception vectors region. Real Mac ROM images start with exception vectors (the 0x100 bytes), so entry at 0xFFF00100 works. Fine — we just wire it; we don't run real ROM anyway.

```c
    EFI_STATUS MapStatus = PpcAddGuestMemoryRegion(Buffer, PPC_ROM_GUEST_BASE, (UINT32)Size, TRUE);
    if (EFI_ERROR(MapStatus)) {
        PpcFreeMemory(Buffer, Size);
        Print(L"Failed to map ROM into guest memory: %r\n", MapStatus);
        return MapStatus;
    }

    g_BootContext.RomLoaded = TRUE;
    g_BootContext.RomAddress = PPC_ROM_GUEST_BASE;
    g_BootContext.RomSize = Size;
    g_BootContext.RomHostBuffer = Buffer;

    if (RomAddress != NULL) { *RomAddress = PPC_ROM_GUEST_BASE; }
    if (RomSize != NULL) { *RomSize = (UINT64)Size; }

    Print(L"System ROM installed: %d bytes at guest 0x%x\n", Size, PPC_ROM_GUEST_BASE);
    return EFI_SUCCESS;
```

Wait, `PpcFreeMemory(Buffer, Size)` frees `Pages = (Size+EFI_PAGE_SIZE-1)/EFI_PAGE_SIZE` pages at physical address Buffer. Buffer is page-aligned from AllocatePages. Good. But `Size` here is UINTN; PpcFreeMemory takes UINT64 Size. OK.

Actually, one concern: `PpcAddGuestMemoryRegion` takes `UINT32 Size`. File size ≤ 4MB fits. Good.

PpcInstallDemoRom:
```c
EFI_STATUS
PpcInstallDemoRom (
    OUT UINT64* RomAddress,
    OUT UINT64* RomSize
    )
{
    UINTN Pages;
    EFI_PHYSICAL_ADDRESS Base = 0;
    EFI_STATUS Status;
    UINT8* Rom;

    if (g_BootContext.RomLoaded) {
        if (RomAddress != NULL) { *RomAddress = g_BootContext.RomAddress; }
        if (RomSize != NULL) { *RomSize = g_BootContext.RomSize; }
        return EFI_ALREADY_STARTED;
    }

    Pages = PPC_ROM_MAX_SIZE / EFI_PAGE_SIZE;
    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate demo ROM pages: %r\n", Status);
        return Status;
    }
    Rom = (UINT8*)(UINTN)Base;
    ZeroMem(Rom, PPC_ROM_MAX_SIZE);

    // Magic word at the ROM base: 'R' 'O' 'M' '1'
    Rom[0] = 'R'; Rom[1] = 'O'; Rom[2] = 'M'; Rom[3] = '1';

    // Reset-vector program at PPC_RESET_VECTOR:
    //   lis r3, 0xFFF0     ; r3 = 0xFFF00000
    //   lwz r4, 0(r3)      ; r4 = ROM[0] = 'ROM1'
    //   addi r5, r4, 1     ; r5 = 'ROM1' + 1
    //   stw r5, 0(r1)      ; store to guest RAM via r1
    UINT32* Prog = (UINT32*)(Rom + (PPC_RESET_VECTOR - PPC_ROM_GUEST_BASE));
    Prog[0] = 0x3C60FFF0;
    Prog[1] = 0x80830000;
    Prog[2] = 0x38A40001;
    Prog[3] = 0x90A10000;

    Status = PpcAddGuestMemoryRegion(Rom, PPC_ROM_GUEST_BASE, PPC_ROM_MAX_SIZE, TRUE);
    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        Print(L"Failed to map demo ROM into guest memory: %r\n", Status);
        return Status;
    }

    g_BootContext.RomLoaded = TRUE;
    g_BootContext.RomAddress = PPC_ROM_GUEST_BASE;
    g_BootContext.RomSize = PPC_ROM_MAX_SIZE;
    g_BootContext.RomHostBuffer = Rom;

    if (RomAddress != NULL) { *RomAddress = PPC_ROM_GUEST_BASE; }
    if (RomSize != NULL) { *RomSize = PPC_ROM_MAX_SIZE; }

    Print(L"Demo system ROM installed: %d bytes at guest 0x%x\n",
          PPC_ROM_MAX_SIZE, PPC_ROM_GUEST_BASE);
    return EFI_SUCCESS;
}
```

Note: `PpcAddGuestMemoryRegion` takes `UINT32 Size` and I pass `PPC_ROM_MAX_SIZE` which is `0x00400000` (int literal). Macro `#define PPC_ROM_MAX_SIZE 0x00400000` is an int; passing to UINT32 param is fine.

PpcInstallLowMemory:
```c
EFI_STATUS
PpcInstallLowMemory (
    OUT UINT64* LowMemAddress,
    OUT UINT64* LowMemSize
    )
{
    UINTN Pages;
    EFI_PHYSICAL_ADDRESS Base = 0;
    EFI_STATUS Status;

    if (g_BootContext.LowMemoryInstalled) {
        if (LowMemAddress != NULL) { *LowMemAddress = g_BootContext.LowMemoryAddress; }
        if (LowMemSize != NULL) { *LowMemSize = g_BootContext.LowMemorySize; }
        return EFI_ALREADY_STARTED;
    }

    Pages = PPC_LOW_MEM_SIZE / EFI_PAGE_SIZE;
    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate low-memory pages: %r\n", Status);
        return Status;
    }
    ZeroMem((VOID*)(UINTN)Base, PPC_LOW_MEM_SIZE);

    Status = PpcAddGuestMemoryRegion((VOID*)(UINTN)Base, PPC_LOW_MEM_GUEST_BASE, PPC_LOW_MEM_SIZE, FALSE);
    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        Print(L"Failed to map low memory into guest memory: %r\n", Status);
        return Status;
    }

    g_BootContext.LowMemoryInstalled = TRUE;
    g_BootContext.LowMemoryAddress = PPC_LOW_MEM_GUEST_BASE;
    g_BootContext.LowMemorySize = PPC_LOW_MEM_SIZE;

    if (LowMemAddress != NULL) { *LowMemAddress = PPC_LOW_MEM_GUEST_BASE; }
    if (LowMemSize != NULL) { *LowMemSize = PPC_LOW_MEM_SIZE; }

    Print(L"Low-memory region installed: %d bytes at guest 0x%x\n",
          PPC_LOW_MEM_SIZE, PPC_LOW_MEM_GUEST_BASE);
    return EFI_SUCCESS;
}
```

PpcPrepareSystemForBoot (real):
```c
EFI_STATUS
PpcPrepareSystemForBoot (
    VOID
    )
{
    UINT64 RamBase = 0;
    UINT64 RamSize = 0;
    UINTN I;

    Print(L"Preparing system for boot\n");

    // The guest memory map must be ready before the CPU can start.
    if (!g_BootContext.LowMemoryInstalled) {
        PpcInstallLowMemory(NULL, NULL);
    }
    if (!g_BootContext.RomLoaded) {
        Print(L"Warning: no system ROM installed; boot will fail at the reset vector\n");
    }

    EFI_STATUS Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);
    if (EFI_ERROR(Status) || RamSize == 0) {
        Print(L"System preparation failed: guest RAM unavailable\n");
        return EFI_NOT_READY;
    }

    // Reset the CPU to the classic Mac OS boot state: PC at the ROM reset
    // vector with machine-check and recoverable-interrupt handling enabled.
    ZeroMem(&g_PpcContext, sizeof(g_PpcContext));
    g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;
    g_PpcContext.Pc = PPC_RESET_VECTOR;
    g_PpcContext.Srr0 = PPC_RESET_VECTOR;
    g_PpcContext.Srr1 = g_PpcContext.Msr;
    g_PpcContext.ExceptionPending = 0;
    for (I = 0; I < 32; I++) {
        g_PpcContext.Gpr[I] = 0;
    }

    // Write the emulator boot info block into low memory.
    {
        UINT32 Magic = 0x45464921;  // 'EFI!'
        UINT32 W;
        PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET + 0, (UINT8)(Magic >> 24));
        PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET + 1, (UINT8)(Magic >> 16));
        PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET + 2, (UINT8)(Magic >> 8));
        PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET + 3, (UINT8)Magic);

        W = (UINT32)RamBase;
        PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 0, (UINT8)(W >> 24));
        ... 4 bytes ...
        W = (UINT32)RamSize;
        ... 4 bytes at +4 ...
        W = PPC_ROM_GUEST_BASE;
        ... +8 ...
        W = (UINT32)(g_BootContext.RomLoaded ? g_BootContext.RomSize : 0);
        ... +12 ...
    }
```
Hmm this is verbose. Let me write a small helper to write big-endian words. Actually I'll add a static helper `BootWriteWord32(GuestAddr, Value)`:
```c
STATIC VOID
BootWriteWord32 (
    IN UINT32 Address,
    IN UINT32 Value
    )
{
    PpcWriteGuestByte(Address + 0, (UINT8)(Value >> 24));
    PpcWriteGuestByte(Address + 1, (UINT8)(Value >> 16));
    PpcWriteGuestByte(Address + 2, (UINT8)(Value >> 8));
    PpcWriteGuestByte(Address + 3, (UINT8)Value);
}
```

Then boot info block:
```c
BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET, 0x45464921);
BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 0, (UINT32)RamBase);
BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 4, (UINT32)RamSize);
BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 8, PPC_ROM_GUEST_BASE);
BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 12,
                (UINT32)(g_BootContext.RomLoaded ? g_BootContext.RomSize : 0));
```

Then:
```c
    g_BootContext.SystemReady = TRUE;
    g_BootContext.SystemBooting = TRUE;

    Print(L"System prepared: PC=0x%x MSR=0x%08x SRR0=0x%x SRR1=0x%x\n",
          g_PpcContext.Pc, g_PpcContext.Msr, g_PpcContext.Srr0, g_PpcContext.Srr1);
    Print(L"Boot info block written to low memory at 0x%x\n",
          PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET);

    return EFI_SUCCESS;
}
```

Note: I need to include `<efi.h>` for PPC_MSR_ME/RI — those are defined in translation.h which bootloader_impl.c already includes. Good.

PpcRunBootSelfTest:
```c
STATIC UINTN g_BootTestPasses = 0;
STATIC UINTN g_BootTestFailures = 0;

STATIC VOID
BootSelfTestCheck (
    IN BOOLEAN Ok,
    IN CHAR16* Name
    )
{
    if (Ok) {
        g_BootTestPasses++;
        Print(L"  [PASS] %s\n", Name);
    } else {
        g_BootTestFailures++;
        Print(L"  [FAIL] %s\n", Name);
    }
}

EFI_STATUS
PpcRunBootSelfTest (
    VOID
    )
{
    UINT64 RamBase = 0;
    UINT64 RamSize = 0;
    UINTN  Executed = 0;
    EFI_STATUS Status;

    g_BootTestPasses = 0;
    g_BootTestFailures = 0;

    Print(L"--- Boot Memory Map / System Init Self-Test ---\n");

    Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);
    BootSelfTestCheck(Status == EFI_SUCCESS && RamSize > 0,
                      L"guest RAM region available");

    // Low-memory globals: writable and readable.
    PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08, 0xAA);
    BootSelfTestCheck(
        PpcReadGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08) == 0xAA,
        L"low-memory globals read/write (guest 0x00000000)");

    // ROM: magic word readable through the interpreter's memory path.
    BootSelfTestCheck(
        PpcReadGuestByte(PPC_ROM_GUEST_BASE + 0) == 'R' &&
        PpcReadGuestByte(PPC_ROM_GUEST_BASE + 1) == 'O' &&
        PpcReadGuestByte(PPC_ROM_GUEST_BASE + 2) == 'M' &&
        PpcReadGuestByte(PPC_ROM_GUEST_BASE + 3) == '1',
        L"ROM magic word 'ROM1' readable at guest 0xFFF00000");

    // ROM is read-only to guest stores.
    PpcWriteGuestByte(PPC_ROM_GUEST_BASE + 0, 0xEE);
    BootSelfTestCheck(
        PpcReadGuestByte(PPC_ROM_GUEST_BASE + 0) == 'R',
        L"ROM rejects guest writes (read-only)");

    // Cross-region execution: run the reset-vector program in the ROM; it
    // loads the ROM magic word and stores its successor into guest RAM.
    PpcSetGprValue(1, (UINT32)RamBase);
    PpcSetGprValue(3, 0);
    PpcSetGprValue(4, 0);
    PpcSetGprValue(5, 0);
    Status = PpcExecuteBlock((UINT32*)(UINTN)PPC_RESET_VECTOR, 4, &Executed);
    UINT32 MagicPlusOne = (UINT32)((0x524F4D31UL) + 1);
    BOOLEAN RanClean = (Status == EFI_SUCCESS && Executed == 4);
    BOOLEAN RomRead = (PpcGetGprValue(4) == 0x524F4D31);
    BOOLEAN RamStored = (PpcReadGuestByte((UINT32)RamBase + 0) == (UINT8)(MagicPlusOne >> 24)) &&
                        (PpcReadGuestByte((UINT32)RamBase + 1) == (UINT8)(MagicPlusOne >> 16)) &&
                        (PpcReadGuestByte((UINT32)RamBase + 2) == (UINT8)(MagicPlusOne >> 8)) &&
                        (PpcReadGuestByte((UINT32)RamBase + 3) == (UINT8)MagicPlusOne);
    BootSelfTestCheck(RanClean, L"reset-vector program ran cleanly");
    BootSelfTestCheck(RomRead, L"program read ROM word (r4 = 'ROM1')");
    BootSelfTestCheck(RamStored, L"program stored result to guest RAM");

    Print(L"--- Boot self-test complete: %d passed, %d failed ---\n",
          g_BootTestPasses, g_BootTestFailures);

    return (g_BootTestFailures == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;
}
```

Careful: `PPC_RESET_VECTOR` = 0xFFF00100. `(UINT32*)(UINTN)PPC_RESET_VECTOR` — PPC_RESET_VECTOR macro is `(PPC_ROM_GUEST_BASE + 0x100)` = `(0xFFF00000 + 0x100)` → int 0xFFF00100. `(UINTN)` cast fine.

But wait — after PpcPrepareSystemForBoot resets GPRs, the self-test resets r1/r3/r4/r5 itself. But self-test runs BEFORE prepare in main. Order in main:
1. install lowmem
2. install rom (real or demo)
3. run boot self-test
4. prepare system for boot

The boot self-test writes to guest RAM at RamBase — but RAM region must be wired. It is (main wired it earlier). Note: the self-test overwrites the guest program demo result in RAM at 0x10000000 region start... Actually GuestProgram demo stored at ResultAddr = ProgramBase + 0x40 = 0x10000040. Our store goes to RamBase+0 = 0x10000000. No conflict with demo result (0x10000040) or kernel (kernel loads at GuestBase = 0x10000000!). Hmm — the kernel load, if a kernel file exists, writes at guest base 0x10000000. Then our boot self-test store at 0x10000000 would overwrite the kernel's first 4 bytes. But that's after kernel execution already happened (kernel section runs before our new section). The kernel is loaded and its first 4 words executed at that point; overwriting afterwards is harmless for the demo flow. OK.

Also note: `PpcExecuteBlock` sets `g_PpcContext.Pc = (UINT32)(UINTN)InstructionStream` and runs 4 instructions from PC=0xFFF00100, fetching from region table. Good. After execution PC = 0xFFF00110. Then PpcPrepareSystemForBoot resets context anyway.

One more check: In the boot self-test, the store `stw r5, 0(r1)` uses r1 = RamBase (0x10000000). EaD computes RA==1 → Gpr[1] + SIMM(0) = 0x10000000. Writes big-endian word. Good.

But careful: the `lwz r4, 0(r3)` reads from 0xFFF00000. That's the ROM region (mapped, read-only). Good. r3 = 0xFFF00000 from lis. Good.

Now `PpcPrepareSystemForBoot` zeroes context AFTER self-test. Fine.

Also, the existing `PpcPrepareSystemForBoot` currently just prints — I'm replacing it.

Also, existing `PpcBootSystem` sets `g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;` — consistent.

Now, extend PPC_BOOTLOADER_CONTEXT:
```c
typedef struct {
    BOOLEAN IsInitialized;
    CHAR16* BootImagePath;
    EFI_PHYSICAL_ADDRESS KernelAddress;
    UINT64 KernelSize;
    BOOLEAN KernelLoaded;
    BOOLEAN SystemBooting;
    BOOLEAN SystemReady;
    PPC_BOOT_PARAMETERS BootParams;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
    // Phase 5: guest memory map state
    BOOLEAN LowMemoryInstalled;
    UINT64  LowMemoryAddress;
    UINT64  LowMemorySize;
    BOOLEAN RomLoaded;
    UINT64  RomAddress;
    UINT64  RomSize;
    VOID*   RomHostBuffer;
} PPC_BOOTLOADER_CONTEXT;
```
`EFI_LOADED_IMAGE_PROTOCOL` — already used in current struct. OK.

Update PpcInitializeBootloader: add zeroing of new fields. Since it does `ZeroMem(&g_BootContext, sizeof(g_BootContext));` at start, new fields auto-zero. But it then sets specific fields. Fine — no change needed except maybe nothing. Actually it already ZeroMems. Good, but it sets `g_BootContext.SystemBooting = FALSE;`. I'll leave. But since ZeroMem zeroes everything, SystemReady etc are FALSE by default. Good. No change needed to PpcInitializeBootloader except... it doesn't need changes. OK.

Update PpcGetBootInfo:
```c
    BootInfo->IsInitialized = g_BootContext.IsInitialized;
    BootInfo->KernelAddress = g_BootContext.KernelAddress;
    BootInfo->KernelSize = g_BootContext.KernelSize;
    BootInfo->KernelLoaded = g_BootContext.KernelLoaded;
    BootInfo->SystemReady = g_BootContext.SystemReady;

    BootInfo->MemoryMap.RomInstalled = g_BootContext.RomLoaded;
    BootInfo->MemoryMap.RomBase = g_BootContext.RomAddress;
    BootInfo->MemoryMap.RomSize = g_BootContext.RomSize;
    BootInfo->MemoryMap.LowMemoryInstalled = g_BootContext.LowMemoryInstalled;
    BootInfo->MemoryMap.LowMemoryBase = g_BootContext.LowMemoryAddress;
    BootInfo->MemoryMap.LowMemorySize = g_BootContext.LowMemorySize;
    BootInfo->MemoryMap.Ready = g_BootContext.SystemReady;
```
ZeroMem already clears struct first. Good.

### Edit 5: `src/main.c`
Add the Phase 5 section before final "Ready" print (line 518). Insert after the kernel section closing brace (line 516) and before line 518.

```c
  // Phase 5: classic Mac OS boot memory map, ROM, and system initialization.
  {
    UINT64 LowMemAddress = 0, LowMemSize = 0;
    UINT64 RomAddress = 0, RomSize = 0;
    EFI_STATUS BootStatus;

    Print(L"\n--- Boot memory map / system initialization ---\n");

    // 1. Low-memory globals at guest 0x00000000 (16 KB, read/write).
    BootStatus = PpcInstallLowMemory(&LowMemAddress, &LowMemSize);
    Print(L"Low-memory region: %s (guest 0x%x, %d bytes)\n",
          EFI_ERROR(BootStatus) ? L"FAIL" : L"OK",
          (UINT32)LowMemAddress, (UINT64)LowMemSize);

    // 2. System ROM at guest 0xFFF00000. Load a real ROM image from the boot
    //    volume if present; otherwise install a self-contained demo ROM so the
    //    full ROM -> guest-memory-map -> execution path is still exercised.
    BootStatus = PpcInstallSystemRom(PPC_ROM_DEFAULT_PATH, &RomAddress, &RomSize);
    if (EFI_ERROR(BootStatus) && BootStatus != EFI_ALREADY_STARTED) {
      if (BootStatus == EFI_NOT_FOUND) {
        Print(L"System ROM not found on volume, installing demo ROM\n");
      } else {
        Print(L"System ROM install failed (%r), installing demo ROM\n", BootStatus);
      }
      BootStatus = PpcInstallDemoRom(&RomAddress, &RomSize);
    }
    Print(L"System ROM: %s (guest 0x%x, %d bytes)\n",
          EFI_ERROR(BootStatus) ? L"FAIL" : L"OK",
          (UINT32)RomAddress, (UINT64)RomSize);

    // 3. Self-test the memory map: ROM read-only, low memory R/W, and a
    //    cross-region ROM->RAM program executed from the reset vector.
    BootStatus = PpcRunBootSelfTest();
    Print(L"Boot memory map self-test: %s\n",
          EFI_ERROR(BootStatus) ? L"FAIL" : L"PASS");

    // 4. Configure the CPU for entry at the ROM reset vector and write the
    //    boot info block into low memory.
    BootStatus = PpcPrepareSystemForBoot();
    Print(L"System initialization: %s\n",
          EFI_ERROR(BootStatus) ? L"FAIL" : L"PASS");

    // 5. Report the final boot state.
    PPC_BOOT_INFO BootInfo;
    if (!EFI_ERROR(PpcGetBootInfo(&BootInfo))) {
      Print(L"Boot state: ready=%d, kernel=%d, ROM at 0x%x (%d bytes), "
            L"low mem at 0x%x (%d bytes)\n",
            BootInfo.SystemReady, BootInfo.KernelLoaded,
            (UINT32)BootInfo.MemoryMap.RomBase, (UINT64)BootInfo.MemoryMap.RomSize,
            (UINT32)BootInfo.MemoryMap.LowMemoryBase,
            (UINT64)BootInfo.MemoryMap.LowMemorySize);
    }
  }
```

Wait: `PpcInstallSystemRom` returns EFI_ALREADY_STARTED if already installed — but it won't be installed yet at this point. The check `!= EFI_ALREADY_STARTED` is defensive. Also if BootStatus == EFI_ALREADY_STARTED, RomAddress/RomSize already filled by the function (it fills them in that branch). Good.

Hmm, careful: In PpcInstallSystemRom, the EFI_ALREADY_STARTED early-return fills RomAddress/RomSize. Good.

Note the ternary `EFI_ERROR(BootStatus) ? L"FAIL" : L"OK"` — with `BootStatus` being EFI_STATUS (UINTN on this platform). Fine.

Wait, EFI_STATUS in gnu-efi is `UINTN`. `EFI_ERROR` macro tests high bit. Fine.

Also `(UINT64)LowMemSize` with `%d` — gnu-efi Print %d with UINT64... existing code does `Print(L"... (%d bytes)\n", FileSize)` with UINT64. Actually earlier I saw `Print(L"Kernel loaded: %d bytes into guest RAM at 0x%x\n", FileSize, ...)` where FileSize is UINT64 → passed as vararg. OK.

But careful: in my main section I cast to `(UINT64)LowMemSize` for `%d`. Since it's a 64-bit value passed as vararg and gnu-efi Print reads it as... On x86_64, varargs promote nothing; Print's internal va_arg for %d likely reads an int or long depending on implementation. The existing code passes UINT64 directly to %d, and it works (Phase 4 verified). So casting to UINT64 is consistent. Actually to be safe, I could print as UINTN (%d works for UINTN since same as long on x86_64). Let me just pass the UINT64 values as-is like existing code does. I'll use `(UINT64)LowMemSize`. It matches existing patterns.

Hmm, one thing: EFI_STATUS `BootStatus` reused for `PpcRunBootSelfTest` etc. Fine.

Also `PpcGetBootInfo` — declared in bootloader.h (already). Good.

Also I use `PPC_ROM_DEFAULT_PATH` macro in main.c — main includes bootloader.h. Good.

### Edit 6: Update TODO.md
Mark Phase 5 first two items partially:
```
## Phase 5: Boot Process
- [~] Create bootloader for Mac OS (load ROM image, set up guest memory map)
  - [x] Guest memory map (RAM + low-memory globals + ROM window at 0xFFF00000)
  - [x] Multi-region interpreter memory path with read-only ROM enforcement
  - [x] ROM loading from the boot volume + demo ROM fallback
- [~] Implement system initialization routines
  - [x] PpcPrepareSystemForBoot: CPU reset state + low-memory boot info block
  - [x] PpcRunBootSelfTest: cross-region ROM->RAM execution at the reset vector
- [ ] Add support for system files and drivers
- [ ] Test boot process with various Mac OS versions
```

Also update the "Current State" section at the top of TODO.md.

### Also consider: USER_GUIDE.md? Skip unless needed. README Current Status? It's stale about Phase 3/4 already (says "no working interpreter" while interpreter exists). I'll leave README; maybe add one line. Actually the instructions say avoid unnecessary changes. I'll update TODO.md and FINAL_SUMMARY? FINAL_SUMMARY says "Pre-alpha scaffolding" — also stale but that's a judgment document. I'll add a Phase 5 note to TODO only, and maybe ARCHITECTURE? No.

Let me reconsider: is there a risk that `PpcSetGuestMemory` is called AFTER the boot section? No, it's called early in main (line 122). Region 0 = RAM. Then boot section adds regions. Good.

One more risk: `PpcSetupBootEnvironment` (called at line 212, before graphics) calls `PpcInitializeMemoryManager(0x00000000, 0x10000000)` — wait! Line 374 of bootloader_impl.c: `PpcInitializeMemoryManager(0x00000000, 0x10000000);  // 256MB`. But PpcInitializeMemoryManager's early-return guard: `if (g_MemoryManager.IsInitialized && g_MemoryManager.VirtualBase != NULL) return EFI_SUCCESS;`. Since main already initialized with (0x10000000, 0x10000000), this is a no-op. Good — the guard saves us.

Also `PpcSetupBootEnvironment` calls `PpcInitializeGraphics(640,480,32)` then main calls `PpcInitializeGraphics(640,480,32)` again. PpcInitializeGraphics doesn't have a guard; it re-runs. In the QEMU runs this was fine (Phase 4 verified). Note: PpcInitializeGraphics re-carves guest fb (ZeroMem again). That's fine.

Now the low-memory region at guest 0x00000000 vs the memory manager: the memory manager's `PpcTranslateAddress` maps only within [0x10000000, 0x20000000). Low memory at 0x00000000 is NOT translated by memory manager — but we don't use memory manager for it; the interpreter region table handles it. Good.

Let me double check that `PpcReadGuestByte`/`PpcWriteGuestByte` in bootloader context: they call g_ReadByte/g_WriteByte which are the defaults (region table) unless PpcSetMemoryAccess was called with custom callbacks. In main, after PpcRunSelfTest, callbacks restored to defaults (NULL, NULL). So region table active. Good.

Wait — one subtlety: `PpcRunSelfTest` in translation_impl.c calls `PpcSetMemoryAccess(SelfTestReadByte, SelfTestWriteByte)` then `PpcSetMemoryAccess(NULL, NULL)` at end. NULL restores defaults. Good.

Now let me also double-check gnu-efi's `EFI_LOADED_IMAGE_PROTOCOL` type name. In the current bootloader_impl.c it's `EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;` and compiles. OK.

Now, are there any issues with `#define PPC_ROM_MAX_SIZE 0x00400000` used as UINT32 in PpcAddGuestMemoryRegion call and as comparison with UINT64 FileSize: `FileSize > PPC_ROM_MAX_SIZE` — FileSize is UINT64, PPC_ROM_MAX_SIZE is int → int promoted to UINT64. Fine.

`Pages = PPC_ROM_MAX_SIZE / EFI_PAGE_SIZE;` = 0x400000 / 0x1000 = 0x400 = 1024. UINTN. Good.

`Pages = PPC_LOW_MEM_SIZE / EFI_PAGE_SIZE;` = 0x4000/0x1000 = 4. Good.

In `PpcInstallDemoRom`, `UINT32* Prog = (UINT32*)(Rom + (PPC_RESET_VECTOR - PPC_ROM_GUEST_BASE));` — pointer arithmetic on UINT8* Rom + 0x100. Fine. Then Prog[0..3] writes big-endian words via direct host memory (little-endian host). Wait! The host buffer stores guest memory. Guest memory is big-endian, but the host buffer representation: In the existing main.c GuestProgram demo, they wrote instructions byte-by-byte big-endian:
```c
for (b=0;b<4;b++) PpcWriteGuestByte(base+i*4+b, (UINT8)(w >> (24 - b*8)));
```
That writes MSB first — i.e., stores the instruction in big-endian byte order in the buffer. So the host buffer holds big-endian data (guest byte order). If I do `Prog[0] = 0x3C60FFF0;` on a little-endian host, it writes bytes F0 FF 60 3C in memory → little-endian. That's WRONG for guest memory semantics!

So I must write the demo ROM program bytes big-endian into the buffer, exactly like main.c does for GuestProgram. Let me fix PpcInstallDemoRom to write bytes big-endian:

```c
    STATIC const UINT32 DemoProgram[4] = {
        0x3C60FFF0,   // lis r3, 0xFFF0
        0x80830000,   // lwz r4, 0(r3)
        0x38A40001,   // addi r5, r4, 1
        0x90A10000,   // stw r5, 0(r1)
    };
    UINT8* Prog = Rom + (PPC_RESET_VECTOR - PPC_ROM_GUEST_BASE);
    UINTN I;
    for (I = 0; I < 4; I++) {
        UINT32 W = DemoProgram[I];
        Prog[I * 4 + 0] = (UINT8)(W >> 24);
        Prog[I * 4 + 1] = (UINT8)(W >> 16);
        Prog[I * 4 + 2] = (UINT8)(W >> 8);
        Prog[I * 4 + 3] = (UINT8)W;
    }
```

Good catch. The magic word 'ROM1' already written byte-explicitly (Rom[0]='R'...). Correct.

Also in BootReadFileToPages, the file contents are copied as-is (byte stream) into the buffer. A ROM file is stored as a raw byte stream (big-endian by nature). CopyMem preserves bytes. Good.

Now also reconsider PpcPrepareSystemForBoot: it uses PpcWriteGuestByte to write to low memory — PpcWriteGuestByte writes a single byte at a guest address through region table (big-endian semantics are just raw bytes). BootWriteWord32 writes 4 bytes MSB first — big-endian word. Consistent with guest convention. Good.

Now, the boot self-test `RamStored` check reads bytes from guest RAM at RamBase..+3 — the stw wrote big-endian. Reading back with PpcReadGuestByte returns raw bytes. MagicPlusOne = 0x524F4D32. MSB-first bytes: 0x52,0x4F,0x4D,0x32. Check matches. Good.

Now let me also verify: `PpcGetGprValue(4) == 0x524F4D31`. The lwz loads via CpuRead32 → big-endian read of ROM bytes 0x52 0x4F 0x4D 0x31 → 0x524F4D31. Good.

Now about `PpcExecuteBlock` being passed `(UINT32*)(UINTN)PPC_RESET_VECTOR`. In PpcExecuteBlock: `if (InstructionStream == NULL ...)`. `(UINT32*)(UINTN)0xFFF00100` non-null. `g_PpcContext.Pc = (UINT32)(UINTN)InstructionStream;` = 0xFFF00100. Fetch `CpuRead32(g_PpcContext.Pc)` → region table → ROM region contains 0xFFF00100 (base 0xFFF00000, size 0x400000) → reads Prog bytes → executes. Good.

Now potential -Wall -Werror issue: In `PpcExecuteBlock`, `InstructionStream` used only to derive PC; no dereference. Fine.

Now in main.c the new block: variables declared at top of block with C89-style? The codebase uses C99 declarations in for-loops (`for (UINTN i = 0; ...)`) already (e.g., main.c line 155 `for (UINTN i = 0; ...)`). clang with gnu-efi — C99 default. Fine.

Let me now also check: `PpcInstallSystemRom` uses `PpcLoadSystemRom(RomPath, &Buffer, &Size64)`. PpcLoadSystemRom prints "System ROM loaded: %d bytes at 0x%x". Fine.

Also in `PpcInstallSystemRom`, after map success we store `g_BootContext.RomHostBuffer = Buffer;`. Fine.

Wait, `PpcInstallSystemRom` maps `Buffer` directly as the region host base (page-aligned, contiguous, file bytes at offset 0). Region size = file size. Good. The leftover pages beyond file size within the allocation are not mapped — reads there return 0. Acceptable. Alternatively map the whole allocation; but we don't track Pages. Keep file-size mapping.

Now `PpcLoadSystemRom` changed to return page-backed buffer; existing signature `(VOID** RomBuffer, UINT64* RomSize)`. OK.

Now let me verify no other code depends on PpcLoadSystemRom's previous pool-buffer behavior. Grep showed only definition. Good.

Now `PpcAddGuestMemoryRegion` return `EFI_ALREADY_STARTED` — is that defined in gnu-efi efi.h? Yes, EFI_ALREADY_STARTED is standard. Good.

`EFI_OUT_OF_RESOURCES`, `EFI_INVALID_PARAMETER`, `EFI_NOT_READY`, `EFI_LOAD_ERROR`, `EFI_NOT_FOUND` all standard.

Now the overlap check in PpcAddGuestMemoryRegion: I must ensure when RAM region at 0x10000000 (size 0x10000000) exists and we add lowmem at 0x00000000 (size 0x4000): 
- GuestBase=0 < existing 0x10000000 → check `0 + 0x4000 > 0x10000000`? No → no overlap. OK.
- Then add ROM at 0xFFF00000 (size 0x400000): GuestBase 0xFFF00000 >= 0x10000000 → check `(0xFFF00000 - 0x10000000) < 0x10000000`? 0xEFF00000 < 0x10000000? No → no overlap. Good.
- Demo ROM and lowmem: 0xFFF00000 vs 0x00000000 → no overlap. Good.

Now, wait: is there any chance region insertion order matters for reading RAM at 0x10000000? Lowmem base 0, size 0x4000 → doesn't contain 0x10000000. ROM doesn't either. Good.

Now `interpreter.c`: The old code had a comment block about "Emulated guest RAM (default memory backing...)". I'll replace globals and add region table. Let me make sure the include of translation.h in interpreter.c provides PPC_CPU_READ_MEMORY typedef etc. Yes.

`PpcAddGuestMemoryRegion` uses `EFI_PHYSICAL_ADDRESS`? No. Uses only basic types. Good.

Now — one more compile concern: in interpreter.c, `g_GuestRegions[I].HostBase` is `VOID*`. `(UINTN)g_GuestRegions[I].HostBase + (Address - g_GuestRegions[I].GuestBase)` — UINTN + UINT32 → UINTN. Cast to volatile UINT8*: `*(volatile UINT8*)((UINTN)...)`. Good.

Now the `Size` field UINT32 vs `(UINT64)(Address - GuestBase) < g_GuestRegions[I].Size`. Address-GuestBase is UINT32, cast to UINT64, compare with UINT32 Size → promoted. Good. No signed warnings.

Also, since I renamed/removed g_GuestMemoryHostBase etc., make sure no other file references those statics. They're static in interpreter.c; grep earlier showed references only within interpreter.c (lines 146-178). Good.

Now let me write PpcPrepareSystemForBoot carefully re: `g_PpcContext` — it's `extern` from interpreter.h (included). ZeroMem requires efi lib — bootloader_impl.c includes efilib.h. Good.

Also, I referenced `PPC_LOW_MEM_MAGIC_OFFSET`, `PPC_LOW_MEM_BOOTINFO_OFFSET` — defined in bootloader.h. Good.

Now: `BootWriteWord32` static helper in bootloader_impl.c. And `BootSelfTestCheck`/`g_BootTestPasses` statics. And `BootReadFileToPages` static. All good.

Now there's one concern with -Wall -Werror: unused static function warnings. `BootReadFileToPages` is used by PpcLoadSystemRom. `BootWriteWord32` used by PpcPrepareSystemForBoot. `BootSelfTestCheck` used by PpcRunBootSelfTest. Good.

Another -Wall -Werror concern: `PpcLoadSystemRom` — I removed the old body. Ensure no leftover unused variables.

Let me also double-check `PpcLoadFile` isn't needed anymore. It stays in uefi_interface (still used by PpcLoadBootImage). Fine.

Now, does `PpcLoadBootImage` still use `PpcLoadFile` (pool)? Yes, unchanged. Fine.

Now, main.c's new block: I reference `PPC_BOOT_INFO` and `PpcGetBootInfo` — declared in bootloader.h, included by main.c (line 11). Good.

Now — think about whether `Print(L"...%d bytes\n", (UINT64)LowMemSize)` — LowMemSize is UINT64 already. I cast to (UINT64) → same. Fine.

Let me now also update the header comment in interpreter.c region section.

Also, should `PpcSetGuestMemory` still be EFI_STATUS returning? It calls PpcAddGuestMemoryRegion which returns status. Good.

Now let me also reconsider whether to add `PpcGetGuestMemoryMap` function. The boot self-test and boot info use context fields; region table is internal to interpreter. For reporting, context is enough. Skip extra getter.

Now, let me also verify the existing `main.c` kernel-load section: it runs `PpcExecuteBlock` on the kernel with KernelExecCount=4, checking r5==700. That's existing verified behavior (needs a kernel file). Unchanged.

Alright. Let me also make sure I don't break `PpcSetupBootEnvironment` which calls PpcInitializeTranslationContext() — fine.

One more consideration: `PpcPrepareSystemForBoot` calls `PpcInstallLowMemory(NULL, NULL)` if not installed — PpcInstallLowMemory handles NULL out params. Good.

Also `PpcPrepareSystemForBoot` should be declared in header (I add it). Currently it's defined in impl but NOT declared anywhere → after I add declaration, fine.

Now let me also double check `PpcInstallSystemRom` signature `(IN CHAR16* RomPath, OUT UINT64* RomAddress, OUT UINT64* RomSize)` — main calls `PpcInstallSystemRom(PPC_ROM_DEFAULT_PATH, &RomAddress, &RomSize)` where RomAddress/RomSize are UINT64. Good. But if it fails (NOT_FOUND), RomAddress/RomSize may be unset — but then we call PpcInstallDemoRom which fills them. Good.

Hmm, in PpcInstallSystemRom on error path, should I set outputs to 0? Not necessary.

Now let me think about whether `PpcInstallDemoRom`'s EFI_ALREADY_STARTED early return is needed. If system ROM real-file install already succeeded, main skips demo (only calls demo if install failed). Defensive early-return fine.

Let me also double check: the demo program's first instruction `lis r3, 0xFFF0` sets r3 = 0xFFF00000. But note: `lis` (addis) with RA=0 uses 0. Good. It's opcode 15, RD=3 → 0x3C60FFF0. Let me recompute: op=15 → 0x3C000000. RD(3)<<21 = 0x00600000 → 0x3C600000. RA=0 → 0. SIMM=0xFFF0 → 0x3C60FFF0. ✓

`lwz r4, 0(r3)`: op=32 → 0x80000000. RT(4)<<21 = 0x00800000 → 0x80800000. RA(3)<<16 = 0x00030000 → 0x80830000. D=0. ✓

`addi r5, r4, 1`: op=14 → 0x38000000. RT(5)<<21 = 0x00A00000 → 0x38A00000. RA(4)<<16 = 0x00040000 → 0x38A40000. SIMM=1 → 0x38A40001. ✓

`stw r5, 0(r1)`: op=36 → 0x90000000. RS(5)<<21 = 0x00A00000 → 0x90A00000. RA(1)<<16 = 0x00010000 → 0x90A10000. D=0. ✓

Great.

Now also double-check `PpcExecuteInstruction` handles opcode 15 (addis) → case 15: yes. opcode 32 (lwz): yes. opcode 14 (addi): yes. opcode 36 (stw): yes. 

Now the boot self-test also should verify PC after prepare? PpcPrepareSystemForBoot runs after self-test; main prints state. Fine.

Now let me write all the edits. Let me start with translation.h.

Actually, wait: I want to double-check gnu-efi's Print format for `%d` with 64-bit arguments. Let me look at gnu-efi print.c if third_party exists — it doesn't (third_party not present). From memory, gnu-efi's Print (efi_print) uses `%d` → va_arg(ap, INT64)? Actually gnu-efi 3.0.18's Print uses `int` for `%d` and has `%ld` for long. But the existing code passes UINT64 to %d and it works (they verified in QEMU with phases 3/4). On x86_64 ABI, integer args are passed in 64-bit registers regardless; va_arg reading an int just reads low 32 bits, but for values < 4GB that's fine. Since all our printed sizes are < 4GB (256MB, ROM sizes ≤4MB), %d with UINT64 is fine. Match existing patterns.

Now, also consider: `Print(L"Boot state: ready=%d, kernel=%d, ROM at 0x%x (%d bytes), ...\n", BootInfo.SystemReady, BootInfo.KernelLoaded, (UINT32)BootInfo.MemoryMap.RomBase, (UINT64)BootInfo.MemoryMap.RomSize, (UINT32)BootInfo.MemoryMap.LowMemoryBase, (UINT64)BootInfo.MemoryMap.LowMemorySize);` — that's 6 varargs matching 6 format specs (%d,%d,%x,%d,%x,%d). Good.

Now, `BootInfo.SystemReady` is BOOLEAN → promoted to int for %d. Fine.

Let me now write the edits.

Let me also re-examine the interpreter.c current region comment block lines 138-179 to craft the replacement precisely. Lines 139-145 comment:
```
// ---------------------------------------------------------------------------
// Emulated guest RAM (default memory backing for the interpreter)
//
// When PpcSetGuestMemory() has been called, guest addresses inside
// [GuestBase, GuestBase + Size) are backed by the host buffer. Addresses
// outside the region read as zero and ignore writes.
// ---------------------------------------------------------------------------
```
I'll replace this whole block (146-179) with region table + default functions, and update comment.

Let me create the edits now.

Also, one more: `PpcSetGuestMemory` doc in translation.h says "The primary guest RAM region". The added function after it. Fine.

Now, about `PPC_MAX_GUEST_REGIONS` define — put in interpreter.c (not header) since only used there. Fine.

Let me also think again about `PpcAddGuestMemoryRegion` overlap when GuestBase == existing GuestBase exactly (reinstall at same base) → falls into else branch: `(0 - 0) < size` → true → EFI_ALREADY_STARTED. Good.

Edge: `(UINT64)(GuestBase - g_GuestRegions[I].GuestBase)` — GuestBase >= existing base in else branch, so subtraction is safe.

Now writing.

Actually, let me reconsider one thing about `PpcPrepareSystemForBoot` and `g_PpcContext.Msr` constants. PPC_MSR_ME = 0x00001000, PPC_MSR_RI = 0x00000002. In PpcBootSystem they use `PPC_MSR_ME | PPC_MSR_RI`. Same. Good.

Also in PpcPrepareSystemForBoot I zero context then set fields — need `#include "cpu/interpreter.h"` for g_PpcContext extern. bootloader_impl.c includes it already (line 4). Good.

Let me also make sure `PpcRunBootSelfTest` prints don't use `%08x` etc. beyond gnu-efi support. Existing code uses `%08x` (line 186 bootloader_impl.c: `Print(L"PowerPC core configured: PC=0x%x MSR=0x%08x\n", ...)`). So `%08x` supported. Good.

Now let me write the files.

Let me write translation.h edit first.

I now have a full picture of the project. Let me verify the remaining pieces (debug/platform impls) quickly, then state the plan.

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\platform\\uefi_interface_impl.c"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\platform\uefi_interface_impl.c</path>
<type>file</type>
<content>
1: #include "uefi_interface.h"
2: #include <efi.h>
3: #include <efilib.h>
4: 
5: // UEFI interface context with enhanced functionality
6: typedef struct {
7:     BOOLEAN IsInitialized;
8:     EFI_SYSTEM_TABLE* SystemTable;
9:     EFI_HANDLE ImageHandle;
10:     EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
11:     EFI_GUID* VendorGuid;
12: } PPC_UEFI_CONTEXT;
13: 
14: // Global UEFI context
15: STATIC PPC_UEFI_CONTEXT g_UefiContext = {0};
16: 
17: EFI_STATUS
18: PpcInitializeUefiInterface (
19:     IN EFI_HANDLE ImageHandle,
20:     IN EFI_SYSTEM_TABLE* SystemTable
21:     )
22: {
23:     // Initialize the UEFI interface context
24:     ZeroMem(&g_UefiContext, sizeof(g_UefiContext));
25:     
26:     g_UefiContext.IsInitialized = TRUE;
27:     g_UefiContext.ImageHandle = ImageHandle;
28:     g_UefiContext.SystemTable = SystemTable;
29:     
30:     // Get the loaded image protocol
31:     EFI_STATUS Status = BS->HandleProtocol(
32:         ImageHandle,
33:         &gEfiLoadedImageProtocolGuid,
34:         (VOID**)&g_UefiContext.LoadedImage
35:     );
36:     
37:     if (EFI_ERROR(Status)) {
38:         Print(L"Failed to get loaded image protocol: %r\n", Status);
39:         return Status;
40:     }
41:     
42:     // Vendor GUID not currently populated
43:     g_UefiContext.VendorGuid = NULL;
44:     
45:     Print(L"PowerPC UEFI Interface initialized\n");
46:     Print(L"Image handle: 0x%x\n", ImageHandle);
47:     Print(L"System table: 0x%x\n", SystemTable);
48:     Print(L"Loaded image: 0x%x\n", g_UefiContext.LoadedImage);
49:     
50:     return EFI_SUCCESS;
51: }
52: 
53: EFI_STATUS
54: PpcGetSystemTable (
55:     OUT EFI_SYSTEM_TABLE** SystemTable
56:     )
57: {
58:     if (SystemTable == NULL) {
59:         return EFI_INVALID_PARAMETER;
60:     }
61:     
62:     *SystemTable = g_UefiContext.SystemTable;
63:     
64:     return EFI_SUCCESS;
65: }
66: 
67: EFI_STATUS
68: PpcGetImageHandle (
69:     OUT EFI_HANDLE* ImageHandle
70:     )
71: {
72:     if (ImageHandle == NULL) {
73:         return EFI_INVALID_PARAMETER;
74:     }
75:     
76:     *ImageHandle = g_UefiContext.ImageHandle;
77:     
78:     return EFI_SUCCESS;
79: }
80: 
81: EFI_STATUS
82: PpcAllocatePool (
83:     IN  EFI_MEMORY_TYPE PoolType,
84:     IN  UINTN Size,
85:     OUT VOID** Buffer
86:     )
87: {
88:     if (Buffer == NULL) {
89:         return EFI_INVALID_PARAMETER;
90:     }
91:     
92:     EFI_STATUS Status = BS->AllocatePool(
93:         PoolType,
94:         Size,
95:         Buffer
96:     );
97:     
98:     if (EFI_ERROR(Status)) {
99:         Print(L"Failed to allocate pool: %r\n", Status);
100:         return Status;
101:     }
102:     
103:     Print(L"Allocated %d bytes from pool (0x%x)\n", Size, *Buffer);
104:     
105:     // Clear the allocated memory
106:     ZeroMem(*Buffer, Size);
107:     
108:     return EFI_SUCCESS;
109: }
110: 
111: EFI_STATUS
112: PpcFreePool (
113:     IN VOID* Buffer
114:     )
115: {
116:     if (Buffer == NULL) {
117:         return EFI_INVALID_PARAMETER;
118:     }
119:     
120:     EFI_STATUS Status = BS->FreePool(Buffer);
121:     
122:     if (EFI_ERROR(Status)) {
123:         Print(L"Failed to free pool: %r\n", Status);
124:         return Status;
125:     }
126:     
127:     Print(L"Freed buffer from pool (0x%x)\n", Buffer);
128:     
129:     return EFI_SUCCESS;
130: }
131: 
132: EFI_STATUS
133: PpcGetMemoryMap (
134:     IN OUT UINTN* MemoryMapSize,
135:     OUT EFI_MEMORY_DESCRIPTOR* MemoryMap,
136:     OUT UINTN* MapKey,
137:     OUT UINTN* DescriptorSize,
138:     OUT UINT32* DescriptorVersion
139:     )
140: {
141:     // Get the memory map from UEFI
142:     EFI_STATUS Status = BS->GetMemoryMap(
143:         MemoryMapSize,
144:         MemoryMap,
145:         MapKey,
146:         DescriptorSize,
147:         DescriptorVersion
148:     );
149:     
150:     if (EFI_ERROR(Status)) {
151:         Print(L"Failed to get memory map: %r\n", Status);
152:         return Status;
153:     }
154:     
155:     Print(L"Retrieved memory map with %d descriptors\n", *MemoryMapSize / *DescriptorSize);
156:     
157:     return EFI_SUCCESS;
158: }
159: 
160: EFI_STATUS
161: PpcOutputString (
162:     IN CHAR16* String
163:     )
164: {
165:     if (String == NULL) {
166:         return EFI_INVALID_PARAMETER;
167:     }
168:     
169:     // Output string via UEFI console
170:     ST->ConOut->OutputString(ST->ConOut, String);
171:     
172:     return EFI_SUCCESS;
173: }
174: 
175: EFI_STATUS
176: PpcGetVariable (
177:     IN     CHAR16* VariableName,
178:     IN     EFI_GUID* VendorGuid,
179:     OUT    UINT32* Attributes,
180:     IN OUT UINTN* DataSize,
181:     OUT    VOID* Data
182:     )
183: {
184:     if (VariableName == NULL || VendorGuid == NULL || DataSize == NULL || Data == NULL) {
185:         return EFI_INVALID_PARAMETER;
186:     }
187:     
188:     EFI_STATUS Status = RT->GetVariable(
189:         VariableName,
190:         VendorGuid,
191:         Attributes,
192:         DataSize,
193:         Data
194:     );
195:     
196:     if (EFI_ERROR(Status)) {
197:         Print(L"Failed to get variable %s: %r\n", VariableName, Status);
198:         return Status;
199:     }
200:     
201:     Print(L"Retrieved variable %s (size: %d bytes)\n", VariableName, *DataSize);
202:     
203:     return EFI_SUCCESS;
204: }
205: 
206: EFI_STATUS
207: PpcSetVariable (
208:     IN     CHAR16* VariableName,
209:     IN     EFI_GUID* VendorGuid,
210:     IN     UINT32 Attributes,
211:     IN     UINTN DataSize,
212:     IN     VOID* Data
213:     )
214: {
215:     if (VariableName == NULL || VendorGuid == NULL || Data == NULL) {
216:         return EFI_INVALID_PARAMETER;
217:     }
218:     
219:     EFI_STATUS Status = RT->SetVariable(
220:         VariableName,
221:         VendorGuid,
222:         Attributes,
223:         DataSize,
224:         Data
225:     );
226:     
227:     if (EFI_ERROR(Status)) {
228:         Print(L"Failed to set variable %s: %r\n", VariableName, Status);
229:         return Status;
230:     }
231:     
232:     Print(L"Set variable %s (size: %d bytes)\n", VariableName, DataSize);
233:     
234:     return EFI_SUCCESS;
235: }
236: 
237: // Additional UEFI interface functions for PowerPC emulation needs
238: 
239: EFI_STATUS
240: PpcGetFileSystem (
241:     OUT EFI_FILE_IO_INTERFACE** FileSystem,
242:     IN  EFI_HANDLE              DeviceHandle
243:     )
244: {
245:     if (FileSystem == NULL) {
246:         return EFI_INVALID_PARAMETER;
247:     }
248: 
249:     // Default to the device this image was booted from.
250:     if (DeviceHandle == NULL) {
251:         if (g_UefiContext.LoadedImage == NULL || g_UefiContext.LoadedImage->DeviceHandle == NULL) {
252:             return EFI_NOT_READY;
253:         }
254:         DeviceHandle = g_UefiContext.LoadedImage->DeviceHandle;
255:     }
256: 
257:     EFI_FILE_IO_INTERFACE* Fs = NULL;
258:     EFI_STATUS Status = BS->HandleProtocol(DeviceHandle, &FileSystemProtocol, (VOID**)&Fs);
259:     if (EFI_ERROR(Status) || Fs == NULL) {
260:         Print(L"Failed to get file system for device handle 0x%x: %r\n", DeviceHandle, Status);
261:         return EFI_NOT_FOUND;
262:     }
263: 
264:     *FileSystem = Fs;
265:     return EFI_SUCCESS;
266: }
267: 
268: EFI_STATUS
269: PpcLoadFile (
270:     IN  EFI_FILE_IO_INTERFACE* FileSystem,
271:     IN  CHAR16*                FileName,
272:     OUT VOID**                 FileBuffer,
273:     OUT UINTN*                 FileSize
274:     )
275: {
276:     if (FileSystem == NULL || FileName == NULL || FileBuffer == NULL || FileSize == NULL) {
277:         return EFI_INVALID_PARAMETER;
278:     }
279: 
280:     *FileBuffer = NULL;
281:     *FileSize = 0;
282: 
283:     Print(L"Loading file: %s\n", FileName);
284: 
285:     EFI_FILE_HANDLE Root = NULL;
286:     EFI_STATUS Status = FileSystem->OpenVolume(FileSystem, &Root);
287:     if (EFI_ERROR(Status)) {
288:         Print(L"Failed to open volume: %r\n", Status);
289:         return Status;
290:     }
291: 
292:     EFI_FILE_HANDLE File = NULL;
293:     Status = Root->Open(Root, &File, FileName, EFI_FILE_MODE_READ, 0);
294:     if (EFI_ERROR(Status) || File == NULL) {
295:         Print(L"Failed to open '%s': %r\n", FileName, Status);
296:         Root->Close(Root);
297:         return (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
298:     }
299: 
300:     // Query the file size through EFI_FILE_INFO.
301:     UINTN FileInfoSize = SIZE_OF_EFI_FILE_INFO + 260 * sizeof(CHAR16);
302:     EFI_FILE_INFO* FileInfo = AllocateZeroPool(FileInfoSize);
303:     if (FileInfo == NULL) {
304:         File->Close(File);
305:         Root->Close(Root);
306:         return EFI_OUT_OF_RESOURCES;
307:     }
308:     Status = File->GetInfo(File, &GenericFileInfo, &FileInfoSize, FileInfo);
309:     if (EFI_ERROR(Status)) {
310:         Print(L"Failed to get file info for '%s': %r\n", FileName, Status);
311:         FreePool(FileInfo);
312:         File->Close(File);
313:         Root->Close(Root);
314:         return Status;
315:     }
316: 
317:     UINT64 FileSize64 = FileInfo->FileSize;
318:     FreePool(FileInfo);
319: 
320:     if (FileSize64 == 0 || FileSize64 > (UINT64)0x7FFFFFFF) {
321:         Print(L"File '%s' has invalid size %d\n", FileName, FileSize64);
322:         File->Close(File);
323:         Root->Close(Root);
324:         return EFI_LOAD_ERROR;
325:     }
326: 
327:     VOID* Buffer = AllocatePool((UINTN)FileSize64);
328:     if (Buffer == NULL) {
329:         File->Close(File);
330:         Root->Close(Root);
331:         return EFI_OUT_OF_RESOURCES;
332:     }
333: 
334:     // Read may return partial data; loop until the whole file is read.
335:     UINTN   BytesRead = 0;
336:     UINT64  Remaining = FileSize64;
337:     BOOLEAN Failed    = FALSE;
338:     while (Remaining > 0) {
339:         UINTN Chunk = (UINTN)Remaining;
340:         Status = File->Read(File, &Chunk, (UINT8*)Buffer + BytesRead);
341:         if (EFI_ERROR(Status) || Chunk == 0) {
342:             Print(L"Failed while reading '%s': %r\n", FileName, Status);
343:             Failed = TRUE;
344:             break;
345:         }
346:         BytesRead += Chunk;
347:         Remaining  -= Chunk;
348:     }
349: 
350:     File->Close(File);
351:     Root->Close(Root);
352: 
353:     if (Failed) {
354:         FreePool(Buffer);
355:         return EFI_LOAD_ERROR;
356:     }
357: 
358:     *FileBuffer = Buffer;
359:     *FileSize = (UINTN)FileSize64;
360: 
361:     Print(L"Loaded '%s': %d bytes at 0x%x\n", FileName, *FileSize, Buffer);
362: 
363:     return EFI_SUCCESS;
364: }
365: 
366: EFI_STATUS
367: PpcGetBootDevice (
368:     OUT EFI_HANDLE* DeviceHandle
369:     )
370: {
371:     if (DeviceHandle == NULL) {
372:         return EFI_INVALID_PARAMETER;
373:     }
374:     if (g_UefiContext.LoadedImage == NULL || g_UefiContext.LoadedImage->DeviceHandle == NULL) {
375:         return EFI_NOT_READY;
376:     }
377: 
378:     *DeviceHandle = g_UefiContext.LoadedImage->DeviceHandle;
379: 
380:     Print(L"Boot device handle: 0x%x\n", *DeviceHandle);
381: 
382:     return EFI_SUCCESS;
383: }
384: 
385: EFI_STATUS
386: PpcGetFirmwareVersion (
387:     OUT CHAR16* VersionString,
388:     IN  UINTN   StringSize
389:     )
390: {
391:     if (VersionString == NULL) {
392:         return EFI_INVALID_PARAMETER;
393:     }
394:     
395:     // In a real implementation:
396:     // 1. Get firmware version from UEFI system table
397:     // 2. Format version string
398:     
399:     StrCpy(VersionString, L"EFI-Mac-Emulator v0.1");
400:     
401:     Print(L"Firmware version: %s\n", VersionString);
402:     
403:     return EFI_SUCCESS;
404: }
405: 
406: EFI_STATUS
407: PpcGetSystemInformation (
408:     OUT EFI_SYSTEM_TABLE** SystemTable,
409:     OUT UINT64* TotalMemory,
410:     OUT UINT64* FreeMemory
411:     )
412: {
413:     if (SystemTable == NULL || TotalMemory == NULL || FreeMemory == NULL) {
414:         return EFI_INVALID_PARAMETER;
415:     }
416: 
417:     *SystemTable = g_UefiContext.SystemTable;
418: 
419:     // Walk the real UEFI memory map to compute total and usable memory.
420:     UINTN  MapSize  = 0;
421:     UINTN  MapKey   = 0;
422:     UINTN  DescSize = 0;
423:     UINT32 DescVer  = 0;
424:     EFI_STATUS Status = BS->GetMemoryMap(&MapSize, NULL, &MapKey, &DescSize, &DescVer);
425:     if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
426:         Print(L"Failed to size memory map: %r\n", Status);
427:         return Status;
428:     }
429: 
430:     UINT8* MapBuffer = AllocatePool(MapSize + sizeof(EFI_MEMORY_DESCRIPTOR));
431:     if (MapBuffer == NULL) {
432:         return EFI_OUT_OF_RESOURCES;
433:     }
434: 
435:     Status = BS->GetMemoryMap(&MapSize, (EFI_MEMORY_DESCRIPTOR*)MapBuffer, &MapKey, &DescSize, &DescVer);
436:     if (EFI_ERROR(Status)) {
437:         Print(L"Failed to get memory map: %r\n", Status);
438:         FreePool(MapBuffer);
439:         return Status;
440:     }
441: 
442:     UINT64 Total = 0;
443:     UINT64 Usable = 0;
444:     EFI_MEMORY_DESCRIPTOR* Desc = (EFI_MEMORY_DESCRIPTOR*)MapBuffer;
445:     for (UINTN i = 0; i < MapSize / DescSize; i++) {
446:         UINT64 Bytes = Desc->NumberOfPages * EFI_PAGE_SIZE;
447:         Total += Bytes;
448:         if (Desc->Type == EfiConventionalMemory ||
449:             Desc->Type == EfiLoaderData ||
450:             Desc->Type == EfiLoaderCode ||
451:             Desc->Type == EfiBootServicesCode ||
452:             Desc->Type == EfiBootServicesData) {
453:             Usable += Bytes;
454:         }
455:         Desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Desc + DescSize);
456:     }
457: 
458:     FreePool(MapBuffer);
459: 
460:     *TotalMemory = Total;
461:     *FreeMemory = Usable;
462: 
463:     Print(L"System information: total %d bytes, usable %d bytes\n", Total, Usable);
464: 
465:     return EFI_SUCCESS;
466: }
467: 
468: EFI_STATUS
469: PpcResetSystem (
470:     IN EFI_RESET_TYPE ResetType,
471:     IN EFI_STATUS StatusCode,
472:     IN UINTN DataSize,
473:     IN CHAR16* ResetData OPTIONAL
474:     )
475: {
476:     Print(L"Resetting system (type: %d)\n", ResetType);
477:     
478:     // In a real implementation:
479:     // 1. Save system state if needed
480:     // 2. Perform system reset using UEFI ResetSystem protocol
481:     
482:     return RT->ResetSystem(ResetType, StatusCode, DataSize, ResetData);
483: }

(End of file - total 483 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 13.7s)

_Thinking:_

Good. Now I have complete understanding. Let me lay out my plan and get to work.

Objective: **Phase 5 (Boot Process), step 1: classic Mac OS guest memory map + ROM loading + system initialization routines.**

I'll present the plan to the user briefly, then implement:

1. `src/cpu/translation.h`: add `PpcAddGuestMemoryRegion` declaration.
2. `src/cpu/interpreter.c`: multi-region guest memory (region table + read-only ROM support).
3. `src/boot/bootloader.h`: boot memory map constants, PPC_GUEST_MEMORY_MAP, extended PPC_BOOT_INFO, new function declarations.
4. `src/boot/bootloader_impl.c`: real ROM loader (page-backed), ROM install (real + demo), low-memory install, boot self-test, real PpcPrepareSystemForBoot, extended PpcGetBootInfo.
5. `src/main.c`: Phase 5 section.
6. `TODO.md`: update.

Let me set up todos and execute.

Project analysis complete. Here's my assessment:

**Status:** Phases 1–4 are complete and verified in QEMU/OVMF (interpreter with 18-check self-test, UEFI-backed guest RAM, GOP graphics, audio ring buffer, block I/O, SNP networking, kernel load/verify/execute pipeline).

**Next objective (Phase 5: Boot Process, first deliverable):** the classic Mac OS boot memory map + ROM loading + system initialization routines. This requires:
1. Multi-region guest memory in the interpreter (RAM + read-only ROM window at `0xFFF00000` + low-memory globals at `0x00000000`)
2. Real ROM loading from the boot volume with a demo-ROM fallback
3. Real system-init routine (`PpcPrepareSystemForBoot`: CPU reset state + low-memory boot info block) and a boot self-test
4. Wiring it into `main.c` and updating `TODO.md`

Note: I cannot build/run QEMU here (Windows host, no clang/lld-link/qemu; the build requires the macOS toolchain), so I'll write the code conservatively against the existing GNU-EFI idioms.

**Tool: todowrite**

**Input:**
```json
{
  "todos": [
    {
      "content": "Add PpcAddGuestMemoryRegion to cpu/translation.h",
      "status": "in_progress",
      "priority": "high"
    },
    {
      "content": "Implement multi-region guest memory in cpu/interpreter.c (RAM + read-only ROM)",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Add boot memory map constants + API to boot/bootloader.h",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Implement ROM loading, demo ROM, low memory, boot self-test, system init in bootloader_impl.c",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Wire Phase 5 section into src/main.c",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Update TODO.md with Phase 5 progress",
      "status": "pending",
      "priority": "medium"
    },
    {
      "content": "Cross-check compile correctness of all changes",
      "status": "pending",
      "priority": "high"
    }
  ]
}
```

**Output:**
```
[
  {
    "content": "Add PpcAddGuestMemoryRegion to cpu/translation.h",
    "status": "in_progress",
    "priority": "high"
  },
  {
    "content": "Implement multi-region guest memory in cpu/interpreter.c (RAM + read-only ROM)",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Add boot memory map constants + API to boot/bootloader.h",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Implement ROM loading, demo ROM, low memory, boot self-test, system init in bootloader_impl.c",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Wire Phase 5 section into src/main.c",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Update TODO.md with Phase 5 progress",
    "status": "pending",
    "priority": "medium"
  },
  {
    "content": "Cross-check compile correctness of all changes",
    "status": "pending",
    "priority": "high"
  }
]
```

---

## Assistant (Build · Big Pickle · 7.6s)

Now implementing. First, the interpreter API addition:

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\translation.h",
  "oldString": "/**\n  Route the interpreter's default load/store path through an emulated\n  guest RAM region.\n\n  After this call, guest addresses in [GuestBase, GuestBase + Size) are\n  backed by the host buffer at HostBase (host = HostBase + guest - GuestBase).\n  Reads of unmapped guest addresses return zero; writes are dropped.\n\n  @param[in] HostBase   Host virtual address backing the region\n  @param[in] GuestBase  Guest-visible base address of the region\n  @param[in] Size       Size of the region in bytes\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcSetGuestMemory (\n    IN VOID*  HostBase,\n    IN UINT32 GuestBase,\n    IN UINT32 Size\n    );",
  "newString": "/**\n  Route the interpreter's default load/store path through an emulated\n  guest RAM region.\n\n  After this call, guest addresses in [GuestBase, GuestBase + Size) are\n  backed by the host buffer at HostBase (host = HostBase + guest - GuestBase).\n  Reads of unmapped guest addresses return zero; writes are dropped.\n\n  @param[in] HostBase   Host virtual address backing the region\n  @param[in] GuestBase  Guest-visible base address of the region\n  @param[in] Size       Size of the region in bytes\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcSetGuestMemory (\n    IN VOID*  HostBase,\n    IN UINT32 GuestBase,\n    IN UINT32 Size\n    );\n\n/**\n  Map an additional region into the interpreter's default memory path.\n\n  PpcSetGuestMemory() installs the primary guest RAM region; this adds further\n  regions such as the classic Mac OS ROM window (guest 0xFFF00000, read-only)\n  or the low-memory globals page (guest 0x00000000). Regions are looked up in\n  insertion order and the first region that contains an address wins. A\n  read-only region satisfies guest loads but drops guest stores.\n\n  @param[in] HostBase   Host virtual address backing the region\n  @param[in] GuestBase  Guest-visible base address of the region\n  @param[in] Size       Size of the region in bytes\n  @param[in] ReadOnly   TRUE if guest stores to the region must be dropped\n  @retval EFI_SUCCESS            Region installed\n  @retval EFI_INVALID_PARAMETER  HostBase is NULL or Size is zero\n  @retval EFI_ALREADY_STARTED    A region already covers GuestBase\n  @retval EFI_OUT_OF_RESOURCES   No free region slot\n**/\nEFI_STATUS\nEFIAPI\nPpcAddGuestMemoryRegion (\n    IN VOID*   HostBase,\n    IN UINT32  GuestBase,\n    IN UINT32  Size,\n    IN BOOLEAN ReadOnly\n    );"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 8.4s)

_Thinking:_

Now the interpreter.c changes.

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "oldString": "// ---------------------------------------------------------------------------\n// Emulated guest RAM (default memory backing for the interpreter)\n//\n// When PpcSetGuestMemory() has been called, guest addresses inside\n// [GuestBase, GuestBase + Size) are backed by the host buffer. Addresses\n// outside the region read as zero and ignore writes.\n// ---------------------------------------------------------------------------\nstatic VOID*  g_GuestMemoryHostBase  = NULL;\nstatic UINT32 g_GuestMemoryGuestBase = 0;\nstatic UINT32 g_GuestMemorySize      = 0;\n\nstatic UINT8\nPpcDefaultReadByte (\n    IN UINT32 Address\n    )\n{\n    if (g_GuestMemoryHostBase != NULL &&\n        Address >= g_GuestMemoryGuestBase &&\n        (Address - g_GuestMemoryGuestBase) < g_GuestMemorySize) {\n        return *(volatile UINT8*)((UINTN)g_GuestMemoryHostBase +\n                                  (Address - g_GuestMemoryGuestBase));\n    }\n    return 0;  // Unmapped guest address reads as zero\n}\n\nstatic VOID\nPpcDefaultWriteByte (\n    IN UINT32 Address,\n    IN UINT8  Value\n    )\n{\n    if (g_GuestMemoryHostBase != NULL &&\n        Address >= g_GuestMemoryGuestBase &&\n        (Address - g_GuestMemoryGuestBase) < g_GuestMemorySize) {\n        *(volatile UINT8*)((UINTN)g_GuestMemoryHostBase +\n                           (Address - g_GuestMemoryGuestBase)) = Value;\n    }\n}",
  "newString": "// ---------------------------------------------------------------------------\n// Emulated guest memory (default memory backing for the interpreter)\n//\n// The primary guest RAM region is installed with PpcSetGuestMemory() and\n// occupies slot 0. Additional regions (the classic Mac OS ROM window at\n// 0xFFF00000, low-memory globals at 0x00000000) are added with\n// PpcAddGuestMemoryRegion(). Addresses not covered by any region read as\n// zero and ignore writes.\n// ---------------------------------------------------------------------------\n#define PPC_MAX_GUEST_REGIONS 4\n\ntypedef struct {\n    VOID*   HostBase;   // Host virtual address backing the region\n    UINT32  GuestBase;  // Guest-visible base address of the region\n    UINT32  Size;       // Region size in bytes\n    BOOLEAN ReadOnly;   // TRUE: guest stores are dropped\n    BOOLEAN Active;     // TRUE: slot is in use\n} PPC_GUEST_REGION;\n\n// Guest memory map. Lookup is in insertion order; the first region that\n// contains an address wins.\nstatic PPC_GUEST_REGION g_GuestRegions[PPC_MAX_GUEST_REGIONS];\n\nstatic UINT8\nPpcDefaultReadByte (\n    IN UINT32 Address\n    )\n{\n    UINTN I;\n\n    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {\n        if (g_GuestRegions[I].Active &&\n            Address >= g_GuestRegions[I].GuestBase &&\n            (UINT64)(Address - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) {\n            return *(volatile UINT8*)((UINTN)g_GuestRegions[I].HostBase +\n                                      (Address - g_GuestRegions[I].GuestBase));\n        }\n    }\n    return 0;  // Unmapped guest address reads as zero\n}\n\nstatic VOID\nPpcDefaultWriteByte (\n    IN UINT32 Address,\n    IN UINT8  Value\n    )\n{\n    UINTN I;\n\n    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {\n        if (g_GuestRegions[I].Active &&\n            Address >= g_GuestRegions[I].GuestBase &&\n            (UINT64)(Address - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) {\n            if (!g_GuestRegions[I].ReadOnly) {\n                *(volatile UINT8*)((UINTN)g_GuestRegions[I].HostBase +\n                                   (Address - g_GuestRegions[I].GuestBase)) = Value;\n            }\n            return;  // First matching region decides the target\n        }\n    }\n}"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 6.6s)

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "EFI_STATUS\nPpcSetGuestMemory (\n    IN VOID*  HostBase,\n    IN UINT32 GuestBase,\n    IN UINT32 Size\n    )\n{\n    return PpcAddGuestMemoryRegion(HostBase, GuestBase, Size, FALSE);\n}\n\nEFI_STATUS\nPpcAddGuestMemoryRegion (\n    IN VOID*   HostBase,\n    IN UINT32  GuestBase,\n    IN UINT32  Size,\n    IN BOOLEAN ReadOnly\n    )\n{\n    UINTN I;\n\n    if (HostBase == NULL || Size == 0) {\n        return EFI_INVALID_PARAMETER;\n    }\n\n    // Reject a region that would overlap an already installed one.\n    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {\n        if (!g_GuestRegions[I].Active) {\n            continue;\n        }\n        if (GuestBase < g_GuestRegions[I].GuestBase) {\n            if ((UINT64)GuestBase + Size > g_GuestRegions[I].GuestBase) {\n                return EFI_ALREADY_STARTED;\n            }\n        } else {\n            if ((UINT64)(GuestBase - g_GuestRegions[I].GuestBase) <\n                g_GuestRegions[I].Size) {\n                return EFI_ALREADY_STARTED;\n            }\n        }\n    }\n\n    for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {\n        if (!g_GuestRegions[I].Active) {\n            g_GuestRegions[I].HostBase  = HostBase;\n            g_GuestRegions[I].GuestBase = GuestBase;\n            g_GuestRegions[I].Size      = Size;\n            g_GuestRegions[I].ReadOnly  = ReadOnly;\n            g_GuestRegions[I].Active    = TRUE;\n            return EFI_SUCCESS;\n        }\n    }\n\n    return EFI_OUT_OF_RESOURCES;\n}",
  "oldString": "EFI_STATUS\nPpcSetGuestMemory (\n    IN VOID*  HostBase,\n    IN UINT32 GuestBase,\n    IN UINT32 Size\n    )\n{\n    g_GuestMemoryHostBase  = HostBase;\n    g_GuestMemoryGuestBase = GuestBase;\n    g_GuestMemorySize      = Size;\n    return EFI_SUCCESS;\n}"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 6.8s)

_Thinking:_

Now update bootloader.h. Let me add constants, struct, extended PPC_BOOT_INFO, and function declarations.

Interpreter done. Now the bootloader header:

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader.h",
  "newString": "// Boot parameters structure\ntypedef struct {\n    UINT32 BootMode;\n    UINT32 MemorySizeMB;\n    UINT32 VideoMode;\n    BOOLEAN EnableDebug;\n    CHAR16* CommandLine;\n} PPC_BOOT_PARAMETERS;\n\n// Classic Mac OS PPC boot memory map (guest-visible addresses)\n#define PPC_ROM_GUEST_BASE      0xFFF00000  // ROM window (classic PPC Macs)\n#define PPC_ROM_MAX_SIZE        0x00400000  // 4 MB ROM window\n#define PPC_ROM_DEFAULT_PATH    L\"\\\\System\\\\MacOS\\\\ROM\"\n#define PPC_RESET_VECTOR        (PPC_ROM_GUEST_BASE + 0x100)\n#define PPC_LOW_MEM_GUEST_BASE  0x00000000  // Low-memory globals\n#define PPC_LOW_MEM_SIZE        0x4000      // 16 KB\n\n// Low-memory global offsets (emulator-defined boot info block)\n#define PPC_LOW_MEM_MAGIC_OFFSET    0x0000\n#define PPC_LOW_MEM_BOOTINFO_OFFSET 0x0100\n\n// Guest memory map as installed for a classic Mac OS boot\ntypedef struct {\n    BOOLEAN RomInstalled;\n    UINT64  RomBase;\n    UINT64  RomSize;\n    BOOLEAN LowMemoryInstalled;\n    UINT64  LowMemoryBase;\n    UINT64  LowMemorySize;\n    BOOLEAN Ready;\n} PPC_GUEST_MEMORY_MAP;\n\n// Boot information structure\ntypedef struct {\n    BOOLEAN IsInitialized;\n    EFI_PHYSICAL_ADDRESS KernelAddress;\n    UINT64 KernelSize;\n    BOOLEAN KernelLoaded;\n    BOOLEAN SystemReady;\n    PPC_GUEST_MEMORY_MAP MemoryMap;\n} PPC_BOOT_INFO;",
  "oldString": "// Boot parameters structure\ntypedef struct {\n    UINT32 BootMode;\n    UINT32 MemorySizeMB;\n    UINT32 VideoMode;\n    BOOLEAN EnableDebug;\n    CHAR16* CommandLine;\n} PPC_BOOT_PARAMETERS;\n\n// Boot information structure\ntypedef struct {\n    BOOLEAN IsInitialized;\n    EFI_PHYSICAL_ADDRESS KernelAddress;\n    UINT64 KernelSize;\n    BOOLEAN KernelLoaded;\n} PPC_BOOT_INFO;"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 8.8s)

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader.h",
  "newString": "/**\n  Load a system ROM image into memory\n  @param[in]  RomPath    Path to the ROM image\n  @param[out] RomBuffer  Pointer to store ROM buffer address\n  @param[out] RomSize    Pointer to store ROM size\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcLoadSystemRom (\n    IN  CHAR16* RomPath,\n    OUT VOID**  RomBuffer,\n    OUT UINT64* RomSize\n    );\n\n/**\n  Load a system ROM image from the boot volume and map it into guest memory\n  at PPC_ROM_GUEST_BASE as a read-only region.\n  @param[in]  RomPath     Path to the ROM image on the boot volume\n  @param[out] RomAddress  Guest address of the installed ROM (may be NULL)\n  @param[out] RomSize     Installed ROM size in bytes (may be NULL)\n  @retval EFI_SUCCESS          ROM installed\n  @retval EFI_NOT_FOUND        ROM file not present on the volume\n  @retval EFI_ALREADY_STARTED  A ROM is already installed\n**/\nEFI_STATUS\nEFIAPI\nPpcInstallSystemRom (\n    IN  CHAR16* RomPath,\n    OUT UINT64* RomAddress,\n    OUT UINT64* RomSize\n    );\n\n/**\n  Install a self-contained demo ROM at PPC_ROM_GUEST_BASE (read-only). The\n  demo ROM contains a small reset-vector program that reads the ROM magic\n  word and stores its successor into guest RAM.\n  @param[out] RomAddress  Guest address of the installed ROM (may be NULL)\n  @param[out] RomSize     Installed ROM size in bytes (may be NULL)\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcInstallDemoRom (\n    OUT UINT64* RomAddress,\n    OUT UINT64* RomSize\n    );\n\n/**\n  Install the classic Mac OS low-memory globals region at guest 0x00000000\n  (16 KB, read/write).\n  @param[out] LowMemAddress  Guest address of the region (may be NULL)\n  @param[out] LowMemSize     Region size in bytes (may be NULL)\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcInstallLowMemory (\n    OUT UINT64* LowMemAddress,\n    OUT UINT64* LowMemSize\n    );\n\n/**\n  Run the boot memory map / system initialization self-test: low-memory\n  read/write, ROM read-only enforcement, and a cross-region ROM -> RAM\n  program executed from the reset vector.\n  @retval EFI_SUCCESS       All checks passed\n  @retval EFI_LOAD_ERROR    One or more checks failed\n**/\nEFI_STATUS\nEFIAPI\nPpcRunBootSelfTest (\n    VOID\n    );\n\n/**\n  Prepare the system for boot: ensure the guest memory map is installed,\n  reset the CPU to the ROM reset vector with a boot-ready MSR, and write\n  the emulator boot info block into low memory.\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcPrepareSystemForBoot (\n    VOID\n    );\n\n#endif // __PPC_BOOTLOADER_H__",
  "oldString": "/**\n  Load a system ROM image into memory\n  @param[in]  RomPath    Path to the ROM image\n  @param[out] RomBuffer  Pointer to store ROM buffer address\n  @param[out] RomSize    Pointer to store ROM size\n  @retval EFI_STATUS\n**/\nEFI_STATUS\nEFIAPI\nPpcLoadSystemRom (\n    IN  CHAR16* RomPath,\n    OUT VOID**  RomBuffer,\n    OUT UINT64* RomSize\n    );\n\n#endif // __PPC_BOOTLOADER_H__"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 13.6s)

_Thinking:_

Now the bootloader_impl.c changes. This is the big one. Let me plan the edits:

1. Extend PPC_BOOTLOADER_CONTEXT struct.
2. Add static helpers: BootReadFileToPages, BootWriteWord32, BootSelfTestCheck, g_BootTestPasses/Failures.
3. Rewrite PpcLoadSystemRom.
4. Add PpcInstallSystemRom, PpcInstallDemoRom, PpcInstallLowMemory, PpcRunBootSelfTest.
5. Rewrite PpcPrepareSystemForBoot.
6. Update PpcGetBootInfo.

Let me do these edits one by one. First, the context struct.

Now the bootloader implementation. First, extend the context struct:

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "newString": "// Bootloader context structure with more complete implementation\ntypedef struct {\n    BOOLEAN IsInitialized;\n    CHAR16* BootImagePath;\n    EFI_PHYSICAL_ADDRESS KernelAddress;\n    UINT64 KernelSize;\n    BOOLEAN KernelLoaded;\n    BOOLEAN SystemBooting;\n    BOOLEAN SystemReady;\n    PPC_BOOT_PARAMETERS BootParams;\n    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;\n    // Phase 5: classic Mac OS guest memory map state\n    BOOLEAN LowMemoryInstalled;\n    UINT64  LowMemoryAddress;\n    UINT64  LowMemorySize;\n    BOOLEAN RomLoaded;\n    UINT64  RomAddress;\n    UINT64  RomSize;\n    VOID*   RomHostBuffer;\n} PPC_BOOTLOADER_CONTEXT;\n\n// Global bootloader context\nSTATIC PPC_BOOTLOADER_CONTEXT g_BootContext = {0};\n\n// ---------------------------------------------------------------------------\n// File load helper: read a whole file from the boot volume into a page-aligned\n// buffer (UEFI pool allocations are limited to ~128 KB; Mac OS ROM images are\n// several MB, so ROM/ROM-like blobs are page-backed).\n// ---------------------------------------------------------------------------\nSTATIC\nEFI_STATUS\nBootReadFileToPages (\n    IN  CHAR16* FilePath,\n    OUT VOID**  Buffer,\n    OUT UINTN*  Size\n    )\n{\n    EFI_FILE_IO_INTERFACE* Fs = NULL;\n    EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);\n    if (EFI_ERROR(Status)) {\n        return Status;\n    }\n\n    EFI_FILE_HANDLE Root = NULL;\n    Status = Fs->OpenVolume(Fs, &Root);\n    if (EFI_ERROR(Status)) {\n        return Status;\n    }\n\n    EFI_FILE_HANDLE File = NULL;\n    Status = Root->Open(Root, &File, FilePath, EFI_FILE_MODE_READ, 0);\n    if (EFI_ERROR(Status) || File == NULL) {\n        Root->Close(Root);\n        return (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;\n    }\n\n    UINTN FileInfoSize = SIZE_OF_EFI_FILE_INFO + 260 * sizeof(CHAR16);\n    EFI_FILE_INFO* Info = AllocateZeroPool(FileInfoSize);\n    if (Info == NULL) {\n        File->Close(File);\n        Root->Close(Root);\n        return EFI_OUT_OF_RESOURCES;\n    }\n    Status = File->GetInfo(File, &GenericFileInfo, &FileInfoSize, Info);\n    if (EFI_ERROR(Status)) {\n        FreePool(Info);\n        File->Close(File);\n        Root->Close(Root);\n        return Status;\n    }\n\n    UINT64 FileSize = Info->FileSize;\n    FreePool(Info);\n\n    if (FileSize == 0 || FileSize > PPC_ROM_MAX_SIZE) {\n        File->Close(File);\n        Root->Close(Root);\n        Print(L\"File '%s' has invalid size %d for ROM load\\n\", FilePath, FileSize);\n        return EFI_LOAD_ERROR;\n    }\n\n    UINTN Pages = (UINTN)((FileSize + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE);\n    EFI_PHYSICAL_ADDRESS Base = 0;\n    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);\n    if (EFI_ERROR(Status)) {\n        File->Close(File);\n        Root->Close(Root);\n        Print(L\"Failed to allocate %d pages for '%s': %r\\n\", Pages, FilePath, Status);\n        return Status;\n    }\n\n    VOID* Dst = (VOID*)(UINTN)Base;\n    UINT64 Remaining = FileSize;\n    UINTN  BytesRead = 0;\n    BOOLEAN Failed = FALSE;\n    while (Remaining > 0) {\n        UINTN Chunk = (UINTN)Remaining;\n        Status = File->Read(File, &Chunk, (UINT8*)Dst + BytesRead);\n        if (EFI_ERROR(Status) || Chunk == 0) {\n            Print(L\"Failed while reading '%s': %r\\n\", FilePath, Status);\n            Failed = TRUE;\n            break;\n        }\n        BytesRead += Chunk;\n        Remaining -= Chunk;\n    }\n\n    File->Close(File);\n    Root->Close(Root);\n\n    if (Failed) {\n        BS->FreePages(Base, Pages);\n        return EFI_LOAD_ERROR;\n    }\n\n    *Buffer = Dst;\n    *Size = (UINTN)FileSize;\n    return EFI_SUCCESS;\n}\n\n// ---------------------------------------------------------------------------\n// Boot self-test bookkeeping (mirrors the CPU self-test style)\n// ---------------------------------------------------------------------------\nSTATIC UINTN g_BootTestPasses   = 0;\nSTATIC UINTN g_BootTestFailures = 0;\n\nSTATIC VOID\nBootSelfTestCheck (\n    IN BOOLEAN Ok,\n    IN CHAR16* Name\n    )\n{\n    if (Ok) {\n        g_BootTestPasses++;\n        Print(L\"  [PASS] %s\\n\", Name);\n    } else {\n        g_BootTestFailures++;\n        Print(L\"  [FAIL] %s\\n\", Name);\n    }\n}\n\n// Write a big-endian 32-bit word into guest memory.\nSTATIC VOID\nBootWriteWord32 (\n    IN UINT32 Address,\n    IN UINT32 Value\n    )\n{\n    PpcWriteGuestByte(Address + 0, (UINT8)(Value >> 24));\n    PpcWriteGuestByte(Address + 1, (UINT8)(Value >> 16));\n    PpcWriteGuestByte(Address + 2, (UINT8)(Value >> 8));\n    PpcWriteGuestByte(Address + 3, (UINT8)Value);\n}",
  "oldString": "// Bootloader context structure with more complete implementation\ntypedef struct {\n    BOOLEAN IsInitialized;\n    CHAR16* BootImagePath;\n    EFI_PHYSICAL_ADDRESS KernelAddress;\n    UINT64 KernelSize;\n    BOOLEAN KernelLoaded;\n    BOOLEAN SystemBooting;\n    PPC_BOOT_PARAMETERS BootParams;\n    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;\n} PPC_BOOTLOADER_CONTEXT;\n\n// Global bootloader context\nSTATIC PPC_BOOTLOADER_CONTEXT g_BootContext = {0};"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 14.0s)

_Thinking:_

Now replace PpcLoadSystemRom with the page-backed version plus new install functions. Let me replace the old PpcLoadSystemRom function.

Now replace `PpcLoadSystemRom` and add the install functions:

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "newString": "EFI_STATUS\nPpcLoadSystemRom (\n    IN  CHAR16* RomPath,\n    OUT VOID**  RomBuffer,\n    OUT UINT64* RomSize\n    )\n{\n    VOID*  Buffer = NULL;\n    UINTN  Size   = 0;\n    EFI_STATUS Status;\n\n    if (RomPath == NULL || RomBuffer == NULL || RomSize == NULL) {\n        return EFI_INVALID_PARAMETER;\n    }\n\n    Print(L\"Loading system ROM from: %s\\n\", RomPath);\n\n    Status = BootReadFileToPages(RomPath, &Buffer, &Size);\n    if (EFI_ERROR(Status)) {\n        return Status;\n    }\n\n    *RomBuffer = Buffer;\n    *RomSize = (UINT64)Size;\n\n    Print(L\"System ROM loaded: %d bytes at 0x%x\\n\", Size, Buffer);\n\n    return EFI_SUCCESS;\n}\n\nEFI_STATUS\nPpcInstallSystemRom (\n    IN  CHAR16* RomPath,\n    OUT UINT64* RomAddress,\n    OUT UINT64* RomSize\n    )\n{\n    VOID*  Buffer = NULL;\n    UINT64 Size   = 0;\n    EFI_STATUS Status;\n\n    if (g_BootContext.RomLoaded) {\n        if (RomAddress != NULL) { *RomAddress = g_BootContext.RomAddress; }\n        if (RomSize != NULL) { *RomSize = g_BootContext.RomSize; }\n        return EFI_ALREADY_STARTED;\n    }\n\n    Status = PpcLoadSystemRom(RomPath, &Buffer, &Size);\n    if (EFI_ERROR(Status)) {\n        return Status;\n    }\n\n    // Map the ROM into the guest memory map as a read-only region.\n    Status = PpcAddGuestMemoryRegion(Buffer, PPC_ROM_GUEST_BASE, (UINT32)Size, TRUE);\n    if (EFI_ERROR(Status)) {\n        PpcFreeMemory(Buffer, Size);\n        Print(L\"Failed to map ROM into guest memory: %r\\n\", Status);\n        return Status;\n    }\n\n    g_BootContext.RomLoaded = TRUE;\n    g_BootContext.RomAddress = PPC_ROM_GUEST_BASE;\n    g_BootContext.RomSize = Size;\n    g_BootContext.RomHostBuffer = Buffer;\n\n    if (RomAddress != NULL) { *RomAddress = PPC_ROM_GUEST_BASE; }\n    if (RomSize != NULL) { *RomSize = Size; }\n\n    Print(L\"System ROM installed: %d bytes at guest 0x%x\\n\",\n          (UINT64)Size, PPC_ROM_GUEST_BASE);\n\n    return EFI_SUCCESS;\n}\n\nEFI_STATUS\nPpcInstallDemoRom (\n    OUT UINT64* RomAddress,\n    OUT UINT64* RomSize\n    )\n{\n    // lis r3, 0xFFF0     ; r3 = 0xFFF00000\n    // lwz r4, 0(r3)      ; r4 = ROM[0] = 'ROM1'\n    // addi r5, r4, 1     ; r5 = 'ROM1' + 1\n    // stw r5, 0(r1)      ; store to guest RAM via r1\n    STATIC const UINT32 DemoProgram[4] = {\n        0x3C60FFF0,\n        0x80830000,\n        0x38A40001,\n        0x90A10000\n    };\n\n    UINTN Pages;\n    EFI_PHYSICAL_ADDRESS Base = 0;\n    EFI_STATUS Status;\n    UINT8* Rom;\n    UINTN I;\n\n    if (g_BootContext.RomLoaded) {\n        if (RomAddress != NULL) { *RomAddress = g_BootContext.RomAddress; }\n        if (RomSize != NULL) { *RomSize = g_BootContext.RomSize; }\n        return EFI_ALREADY_STARTED;\n    }\n\n    Pages = PPC_ROM_MAX_SIZE / EFI_PAGE_SIZE;\n    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);\n    if (EFI_ERROR(Status)) {\n        Print(L\"Failed to allocate demo ROM pages: %r\\n\", Status);\n        return Status;\n    }\n    Rom = (UINT8*)(UINTN)Base;\n    ZeroMem(Rom, PPC_ROM_MAX_SIZE);\n\n    // Magic word at the ROM base: 'R' 'O' 'M' '1'.\n    Rom[0] = 'R';\n    Rom[1] = 'O';\n    Rom[2] = 'M';\n    Rom[3] = '1';\n\n    // Reset-vector program, stored big-endian (guest byte order).\n    UINT8* Prog = Rom + (PPC_RESET_VECTOR - PPC_ROM_GUEST_BASE);\n    for (I = 0; I < 4; I++) {\n        UINT32 W = DemoProgram[I];\n        Prog[I * 4 + 0] = (UINT8)(W >> 24);\n        Prog[I * 4 + 1] = (UINT8)(W >> 16);\n        Prog[I * 4 + 2] = (UINT8)(W >> 8);\n        Prog[I * 4 + 3] = (UINT8)W;\n    }\n\n    Status = PpcAddGuestMemoryRegion(Rom, PPC_ROM_GUEST_BASE, PPC_ROM_MAX_SIZE, TRUE);\n    if (EFI_ERROR(Status)) {\n        BS->FreePages(Base, Pages);\n        Print(L\"Failed to map demo ROM into guest memory: %r\\n\", Status);\n        return Status;\n    }\n\n    g_BootContext.RomLoaded = TRUE;\n    g_BootContext.RomAddress = PPC_ROM_GUEST_BASE;\n    g_BootContext.RomSize = PPC_ROM_MAX_SIZE;\n    g_BootContext.RomHostBuffer = Rom;\n\n    if (RomAddress != NULL) { *RomAddress = PPC_ROM_GUEST_BASE; }\n    if (RomSize != NULL) { *RomSize = PPC_ROM_MAX_SIZE; }\n\n    Print(L\"Demo system ROM installed: %d bytes at guest 0x%x\\n\",\n          (UINT64)PPC_ROM_MAX_SIZE, PPC_ROM_GUEST_BASE);\n\n    return EFI_SUCCESS;\n}\n\nEFI_STATUS\nPpcInstallLowMemory (\n    OUT UINT64* LowMemAddress,\n    OUT UINT64* LowMemSize\n    )\n{\n    UINTN Pages;\n    EFI_PHYSICAL_ADDRESS Base = 0;\n    EFI_STATUS Status;\n\n    if (g_BootContext.LowMemoryInstalled) {\n        if (LowMemAddress != NULL) { *LowMemAddress = g_BootContext.LowMemoryAddress; }\n        if (LowMemSize != NULL) { *LowMemSize = g_BootContext.LowMemorySize; }\n        return EFI_ALREADY_STARTED;\n    }\n\n    Pages = PPC_LOW_MEM_SIZE / EFI_PAGE_SIZE;\n    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);\n    if (EFI_ERROR(Status)) {\n        Print(L\"Failed to allocate low-memory pages: %r\\n\", Status);\n        return Status;\n    }\n    ZeroMem((VOID*)(UINTN)Base, PPC_LOW_MEM_SIZE);\n\n    Status = PpcAddGuestMemoryRegion((VOID*)(UINTN)Base,\n                                     PPC_LOW_MEM_GUEST_BASE,\n                                     PPC_LOW_MEM_SIZE,\n                                     FALSE);\n    if (EFI_ERROR(Status)) {\n        BS->FreePages(Base, Pages);\n        Print(L\"Failed to map low memory into guest memory: %r\\n\", Status);\n        return Status;\n    }\n\n    g_BootContext.LowMemoryInstalled = TRUE;\n    g_BootContext.LowMemoryAddress = PPC_LOW_MEM_GUEST_BASE;\n    g_BootContext.LowMemorySize = PPC_LOW_MEM_SIZE;\n\n    if (LowMemAddress != NULL) { *LowMemAddress = PPC_LOW_MEM_GUEST_BASE; }\n    if (LowMemSize != NULL) { *LowMemSize = PPC_LOW_MEM_SIZE; }\n\n    Print(L\"Low-memory region installed: %d bytes at guest 0x%x\\n\",\n          (UINT64)PPC_LOW_MEM_SIZE, PPC_LOW_MEM_GUEST_BASE);\n\n    return EFI_SUCCESS;\n}",
  "oldString": "EFI_STATUS\nPpcLoadSystemRom (\n    IN  CHAR16* RomPath,\n    OUT VOID**  RomBuffer,\n    OUT UINT64* RomSize\n    )\n{\n    if (RomPath == NULL || RomBuffer == NULL || RomSize == NULL) {\n        return EFI_INVALID_PARAMETER;\n    }\n\n    Print(L\"Loading system ROM from: %s\\n\", RomPath);\n\n    // Real UEFI file I/O into a pool buffer.\n    EFI_FILE_IO_INTERFACE* Fs = NULL;\n    EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);\n    if (EFI_ERROR(Status)) {\n        return Status;\n    }\n\n    UINTN  Size = 0;\n    VOID*  Buffer = NULL;\n    Status = PpcLoadFile(Fs, RomPath, &Buffer, &Size);\n    if (EFI_ERROR(Status)) {\n        return Status;\n    }\n\n    *RomBuffer = Buffer;\n    *RomSize = Size;\n\n    Print(L\"System ROM loaded: %d bytes at 0x%x\\n\", Size, Buffer);\n\n    return EFI_SUCCESS;\n}"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 6.2s)

_Thinking:_

Now update PpcGetBootInfo and replace PpcPrepareSystemForBoot, and add PpcRunBootSelfTest. Let me first update PpcGetBootInfo.

Now update `PpcGetBootInfo`, replace `PpcPrepareSystemForBoot`, and add the boot self-test:

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "newString": "    // Fill boot information structure\n    ZeroMem(BootInfo, sizeof(PPC_BOOT_INFO));\n    \n    BootInfo->IsInitialized = g_BootContext.IsInitialized;\n    BootInfo->KernelAddress = g_BootContext.KernelAddress;\n    BootInfo->KernelSize = g_BootContext.KernelSize;\n    BootInfo->KernelLoaded = g_BootContext.KernelLoaded;\n    BootInfo->SystemReady = g_BootContext.SystemReady;\n\n    BootInfo->MemoryMap.RomInstalled = g_BootContext.RomLoaded;\n    BootInfo->MemoryMap.RomBase = g_BootContext.RomAddress;\n    BootInfo->MemoryMap.RomSize = g_BootContext.RomSize;\n    BootInfo->MemoryMap.LowMemoryInstalled = g_BootContext.LowMemoryInstalled;\n    BootInfo->MemoryMap.LowMemoryBase = g_BootContext.LowMemoryAddress;\n    BootInfo->MemoryMap.LowMemorySize = g_BootContext.LowMemorySize;\n    BootInfo->MemoryMap.Ready = g_BootContext.SystemReady;\n    \n    return EFI_SUCCESS;\n}",
  "oldString": "    // Fill boot information structure\n    ZeroMem(BootInfo, sizeof(PPC_BOOT_INFO));\n    \n    BootInfo->IsInitialized = g_BootContext.IsInitialized;\n    BootInfo->KernelAddress = g_BootContext.KernelAddress;\n    BootInfo->KernelSize = g_BootContext.KernelSize;\n    BootInfo->KernelLoaded = g_BootContext.KernelLoaded;\n    \n    return EFI_SUCCESS;\n}"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 12.5s)

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "newString": "EFI_STATUS\nPpcPrepareSystemForBoot (\n    VOID\n    )\n{\n    UINT64 RamBase = 0;\n    UINT64 RamSize = 0;\n    UINTN  I;\n    EFI_STATUS Status;\n\n    Print(L\"Preparing system for boot\\n\");\n\n    // The guest memory map must be ready before the CPU can start.\n    if (!g_BootContext.LowMemoryInstalled) {\n        PpcInstallLowMemory(NULL, NULL);\n    }\n    if (!g_BootContext.RomLoaded) {\n        Print(L\"Warning: no system ROM installed; boot would fail at the reset vector\\n\");\n    }\n\n    Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);\n    if (EFI_ERROR(Status) || RamSize == 0) {\n        Print(L\"System preparation failed: guest RAM unavailable\\n\");\n        return EFI_NOT_READY;\n    }\n\n    // Reset the CPU to the classic Mac OS boot state: PC at the ROM reset\n    // vector with machine-check and recoverable-interrupt handling enabled.\n    ZeroMem(&g_PpcContext, sizeof(g_PpcContext));\n    for (I = 0; I < 32; I++) {\n        g_PpcContext.Gpr[I] = 0;\n    }\n    g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;\n    g_PpcContext.Pc = PPC_RESET_VECTOR;\n    g_PpcContext.Srr0 = PPC_RESET_VECTOR;\n    g_PpcContext.Srr1 = g_PpcContext.Msr;\n    g_PpcContext.ExceptionPending = 0;\n\n    // Write the emulator boot info block into low memory: magic, then\n    // RAM base, RAM size, ROM base, and installed ROM size (big-endian).\n    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET, 0x45464921);\n    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 0, (UINT32)RamBase);\n    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 4, (UINT32)RamSize);\n    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 8, PPC_ROM_GUEST_BASE);\n    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 12,\n                    (UINT32)(g_BootContext.RomLoaded ? g_BootContext.RomSize : 0));\n\n    g_BootContext.SystemReady = TRUE;\n    g_BootContext.SystemBooting = TRUE;\n\n    Print(L\"System prepared: PC=0x%x MSR=0x%08x SRR0=0x%x SRR1=0x%x\\n\",\n          g_PpcContext.Pc, g_PpcContext.Msr, g_PpcContext.Srr0, g_PpcContext.Srr1);\n    Print(L\"Boot info block written to low memory at 0x%x\\n\",\n          PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET);\n\n    return EFI_SUCCESS;\n}\n\nEFI_STATUS\nPpcRunBootSelfTest (\n    VOID\n    )\n{\n    UINT64 RamBase = 0;\n    UINT64 RamSize = 0;\n    UINTN  Executed = 0;\n    EFI_STATUS Status;\n    UINT32 MagicPlusOne;\n\n    g_BootTestPasses = 0;\n    g_BootTestFailures = 0;\n\n    Print(L\"--- Boot Memory Map / System Init Self-Test ---\\n\");\n\n    Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);\n    BootSelfTestCheck(Status == EFI_SUCCESS && RamSize > 0,\n                      L\"guest RAM region available\");\n\n    // Low-memory globals: writable and readable.\n    PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08, 0xAA);\n    BootSelfTestCheck(\n        PpcReadGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08) == 0xAA,\n        L\"low-memory globals read/write (guest 0x00000000)\");\n\n    // ROM: magic word readable through the interpreter's memory path.\n    BootSelfTestCheck(\n        PpcReadGuestByte(PPC_ROM_GUEST_BASE + 0) == 'R' &&\n        PpcReadGuestByte(PPC_ROM_GUEST_BASE + 1) == 'O' &&\n        PpcReadGuestByte(PPC_ROM_GUEST_BASE + 2) == 'M' &&\n        PpcReadGuestByte(PPC_ROM_GUEST_BASE + 3) == '1',\n        L\"ROM magic word 'ROM1' readable at guest 0xFFF00000\");\n\n    // ROM is read-only to guest stores.\n    PpcWriteGuestByte(PPC_ROM_GUEST_BASE + 0, 0xEE);\n    BootSelfTestCheck(\n        PpcReadGuestByte(PPC_ROM_GUEST_BASE + 0) == 'R',\n        L\"ROM rejects guest writes (read-only)\");\n\n    // Cross-region execution: run the reset-vector program in the ROM; it\n    // loads the ROM magic word and stores its successor into guest RAM.\n    PpcSetGprValue(1, (UINT32)RamBase);\n    PpcSetGprValue(3, 0);\n    PpcSetGprValue(4, 0);\n    PpcSetGprValue(5, 0);\n    Status = PpcExecuteBlock((UINT32*)(UINTN)PPC_RESET_VECTOR, 4, &Executed);\n\n    MagicPlusOne = 0x524F4D31 + 1;\n    BootSelfTestCheck(Status == EFI_SUCCESS && Executed == 4,\n                      L\"reset-vector program ran cleanly\");\n    BootSelfTestCheck(PpcGetGprValue(4) == 0x524F4D31,\n                      L\"program read ROM word (r4 = 'ROM1')\");\n    BootSelfTestCheck(\n        PpcReadGuestByte((UINT32)RamBase + 0) == (UINT8)(MagicPlusOne >> 24) &&\n        PpcReadGuestByte((UINT32)RamBase + 1) == (UINT8)(MagicPlusOne >> 16) &&\n        PpcReadGuestByte((UINT32)RamBase + 2) == (UINT8)(MagicPlusOne >> 8) &&\n        PpcReadGuestByte((UINT32)RamBase + 3) == (UINT8)MagicPlusOne,\n        L\"program stored result to guest RAM\");\n\n    Print(L\"--- Boot self-test complete: %d passed, %d failed ---\\n\",\n          g_BootTestPasses, g_BootTestFailures);\n\n    return (g_BootTestFailures == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;\n}",
  "oldString": "EFI_STATUS\nPpcPrepareSystemForBoot (\n    VOID\n    )\n{\n    Print(L\"Preparing system for boot\\n\");\n    \n    // In a real implementation:\n    // 1. Finalize system state\n    // 2. Save any required state information\n    // 3. Set up registers and memory for kernel startup\n    // 4. Prepare interrupt vectors\n    \n    Print(L\"System preparation complete\\n\");\n    \n    return EFI_SUCCESS;\n}"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 8.4s)

_Thinking:_

Now update main.c to add the Phase 5 section. Let me insert before the final "=== EFI-Mac-Emulator Ready ===" print.

Now wire the Phase 5 section into `main.c`:

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\main.c",
  "newString": "  // Phase 5: classic Mac OS boot memory map, ROM, and system initialization.\n  {\n    UINT64 LowMemAddress = 0, LowMemSize = 0;\n    UINT64 RomAddress = 0, RomSize = 0;\n    EFI_STATUS BootStatus;\n\n    Print(L\"\\n--- Boot memory map / system initialization ---\\n\");\n\n    // 1. Low-memory globals at guest 0x00000000 (16 KB, read/write).\n    BootStatus = PpcInstallLowMemory(&LowMemAddress, &LowMemSize);\n    Print(L\"Low-memory region: %s (guest 0x%x, %d bytes)\\n\",\n          EFI_ERROR(BootStatus) ? L\"FAIL\" : L\"OK\",\n          (UINT32)LowMemAddress, (UINT64)LowMemSize);\n\n    // 2. System ROM at guest 0xFFF00000. Load a real ROM image from the boot\n    //    volume if present; otherwise install a self-contained demo ROM so the\n    //    full ROM -> guest-memory-map -> execution path is still exercised.\n    BootStatus = PpcInstallSystemRom(PPC_ROM_DEFAULT_PATH, &RomAddress, &RomSize);\n    if (EFI_ERROR(BootStatus) && BootStatus != EFI_ALREADY_STARTED) {\n      if (BootStatus == EFI_NOT_FOUND) {\n        Print(L\"System ROM not found on volume, installing demo ROM\\n\");\n      } else {\n        Print(L\"System ROM install failed (%r), installing demo ROM\\n\", BootStatus);\n      }\n      BootStatus = PpcInstallDemoRom(&RomAddress, &RomSize);\n    }\n    Print(L\"System ROM: %s (guest 0x%x, %d bytes)\\n\",\n          EFI_ERROR(BootStatus) ? L\"FAIL\" : L\"OK\",\n          (UINT32)RomAddress, (UINT64)RomSize);\n\n    // 3. Self-test the memory map: ROM read-only, low memory R/W, and a\n    //    cross-region ROM -> RAM program executed from the reset vector.\n    BootStatus = PpcRunBootSelfTest();\n    Print(L\"Boot memory map self-test: %s\\n\",\n          EFI_ERROR(BootStatus) ? L\"FAIL\" : L\"PASS\");\n\n    // 4. Configure the CPU for entry at the ROM reset vector and write the\n    //    boot info block into low memory.\n    BootStatus = PpcPrepareSystemForBoot();\n    Print(L\"System initialization: %s\\n\",\n          EFI_ERROR(BootStatus) ? L\"FAIL\" : L\"PASS\");\n\n    // 5. Report the final boot state.\n    PPC_BOOT_INFO BootInfo;\n    if (!EFI_ERROR(PpcGetBootInfo(&BootInfo))) {\n      Print(L\"Boot state: ready=%d, kernel=%d, ROM at 0x%x (%d bytes), \"\n            L\"low mem at 0x%x (%d bytes)\\n\",\n            BootInfo.SystemReady, BootInfo.KernelLoaded,\n            (UINT32)BootInfo.MemoryMap.RomBase,\n            (UINT64)BootInfo.MemoryMap.RomSize,\n            (UINT32)BootInfo.MemoryMap.LowMemoryBase,\n            (UINT64)BootInfo.MemoryMap.LowMemorySize);\n    }\n  }\n\n  Print(L\"\\n=== EFI-Mac-Emulator Ready ===\\n\");",
  "oldString": "  Print(L\"\\n=== EFI-Mac-Emulator Ready ===\\n\");"
}
```

**Output:**
```
Edit applied successfully.
```

---

## User

---

## Assistant (Compaction · Big Pickle · 27.9s)

_Thinking:_

Let me extract the key facts from this conversation to build the anchored summary.

The user's overall goal: "Please analyse current project status, determine next objective, and execute the completion of that objective." — They want me to analyze the EFIMac project, pick the next objective, and implement it.

Key project facts gathered:
- Project: EFI Mac Emulator — UEFI-based emulator for classic Mac OS (System 7, Mac OS 8/9) on x86_64
- Location: C:\Users\clayc\Desktop\New folder (2)\EFIMac
- Git: branch main, clean tree at fee7247 ("KPD - Deleted accidental transcript duplicate.")
- Phases 1-4 complete per git log/TODO.md: Phase 1 Research, Phase 2 Core Infrastructure, Phase 3 Emulation Components, Phase 4 implemented/tested (Graphics/Networking working)
- TODO.md says Phase 4 complete, 18-check interpreter self-test passes, 256MB guest RAM via AllocatePages, guest framebuffer at 0x18000000 carved out of guest RAM, audio buffer at 0x18800000, SNP networking, bootloader loads \System\MacOS\kernel, debug writes boot.log
- Build requires macOS + Homebrew clang/lld (Makefile cross-compiles to x86_64 PE/COFF via lld-link). On this Windows host: clang/lld-link/qemu/cmake/ninja NOT FOUND; make and git exist. So I cannot build/run QEMU here.
- third_party/gnu-efi does not exist on this machine (Test-Path returned False), but Makefile clones it on demand.

Interpretation of what I did (active work):
- Analyzed: main.c, bootloader.h/impl.c, interpreter.h/.c, translation.h/.c, memory manager, hardware abstraction, uefi_interface, debug, Makefile, BUILD_INSTRUCTIONS.md, TODO.md, ARCHITECTURE.md, FINAL_SUMMARY.md, README.md
- Determined next objective: Phase 5 Boot Process — first deliverable: classic Mac OS boot memory map + ROM loading + system initialization routines
- Created 7 todos via todowrite (todowrite tool call) with tasks:
  1. Add PpcAddGuestMemoryRegion to cpu/translation.h (in_progress initially, marked high)
  2. Implement multi-region guest memory in cpu/interpreter.c (RAM + read-only ROM)
  3. Add boot memory map constants + API to boot/bootloader.h
  4. Implement ROM loading, demo ROM, low memory, boot self-test, system init in bootloader_impl.c
  5. Wire Phase 5 section into src/main.c
  6. Update TODO.md with Phase 5 progress
  7. Cross-check compile correctness of all changes

Edits actually applied so far:
1. src/cpu/translation.h: Replaced PpcSetGuestMemory doc block with newer version that also declares PpcAddGuestMemoryRegion (HostBase, GuestBase, Size, ReadOnly) — returns EFI_SUCCESS / EFI_INVALID_PARAMETER / EFI_ALREADY_STARTED / EFI_OUT_OF_RESOURCES. Done.
2. src/cpu/interpreter.c: Replaced single-region globals (g_GuestMemoryHostBase, g_GuestMemoryGuestBase, g_GuestMemorySize) with PPC_MAX_GUEST_REGIONS 4 + PPC_GUEST_REGION struct {HostBase, GuestBase, Size, ReadOnly, Active} + g_GuestRegions array; PpcDefaultReadByte/PpcDefaultWriteByte now loop through regions, first match wins, read-only regions drop stores. Done.
3. src/cpu/interpreter.c: PpcSetGuestMemory now delegates to PpcAddGuestMemoryRegion(HostBase, GuestBase, Size, FALSE), and PpcAddGuestMemoryRegion implemented with overlap checking. Done.

Nothing else implemented yet: bootloader.h changes, bootloader_impl.c changes, main.c wiring, TODO.md update not started.

Key design decisions to carry forward:
- Classic Mac OS ROM window at guest 0xFFF00000 (read-only)
- Low-memory globals page at guest 0x00000000
- Multi-region lookup insertion order, first region containing an address wins
- PpcSetGuestMemory installs primary guest RAM into slot 0
- ReadOnly regions satisfy loads but drop stores

Constraints/facts for continuation:
- Cannot build on this host: no clang, lld-link, qemu; build toolchain requires macOS brew llvm/lld; must write code conservatively against GNU-EFI idioms (efi.h/efilib.h, BS->, Print(L"..."))
- Project uses C with STATIC macro, EFIAPI, EFI_STATUS patterns
- g_PpcContext is global CPU context (PPC_CPU_CONTEXT in interpreter.h)
- Existing interpreter API has PpcSetGuestMemory referenced in hardware/abstraction_impl.c and bootloader_impl.c presumably (it used PpcGetGuestMemoryRegion per transcripts — actually PpcGetGuestMemoryRegion exists in manager? The transcript mentioned PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize))
- Existing modules: src/cpu/translation.h/impl.c, src/cpu/interpreter.h/.c (interpreter.c is ~49KB with 18-check self-test), src/boot/bootloader.h/impl.c, src/memory/manager.h/impl.c, src/hardware/abstraction.h/impl.c, src/platform/uefi_interface.h/impl.c, src/utils/debug.h/+impl

Actually the debug impl was truncated — there's src/utils/debug.h and presumably debug_impl.c (the Get-ChildItem output was truncated at "C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\utils\debug_i"). I should note that.

Next Move:
1. Implement item 3: add boot memory map constants (e.g. PPC_ROM_BASE 0xFFF00000, low memory base 0x00000000) + API (PpcLoadSystemRom, PpcPrepareSystemForBoot, PpcBootSelfTest, etc.) to src/boot/bootloader.h
2. Then bootloader_impl.c implementation (ROM loading from volume \System\...? or demo ROM fallback, low memory init, self-test, PpcPrepareSystemForBoot wiring memory regions via PpcAddGuestMemoryRegion)
3. Wire into main.c Phase 5 section
4. Update TODO.md
5. Cross-check compile correctness (can't build locally)

Blocked: build/test on this Windows host impossible — no clang/lld-link/qemu; third_party/gnu-efi not present either.

Relevant files:
- C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation.h — modified: added PpcAddGuestMemoryRegion declaration
- C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c — modified: multi-region guest memory in PpcDefaultReadByte/PpcDefaultWriteByte + PpcAddGuestMemoryRegion impl
- src\boot\bootloader.h/impl.c — next to modify
- src\main.c — to wire Phase 5
- TODO.md — to update
- Makefile/BUILD_INSTRUCTIONS.md — macOS-only toolchain; cannot run here

Keep it terse. Format exactly per template with sections: Objective, Important Details, Work State (Completed/Active/Blocked), Next Move, Relevant Files.

## Objective
- Continue the EFI-Mac-Emulator project: analyse current status, pick the next objective, and implement it.
- Chosen next objective (Phase 5, first deliverable): classic Mac OS boot memory map + ROM loading + system initialization routines (multi-region guest memory, ROM window, low-memory globals, `PpcPrepareSystemForBoot`, boot self-test).

## Important Details
- Repo: `C:\Users\clayc\Desktop\New folder (2)\EFIMac`; branch `main`; clean tree at `fee7247`.
- Status: Phases 1–4 done (interpreter w/ 18-check self-test, 256 MB guest RAM via `AllocatePages`, GOP framebuffer at guest `0x18000000`, audio ring buffer at `0x18800000`, SNP networking, kernel load from `\System\MacOS\kernel`, `boot.log` via real file I/O, verified in QEMU/OVMF).
- Memory plan: primary guest RAM = slot 0 via `PpcSetGuestMemory`; additional regions: read-only ROM window at guest `0xFFF00000`, low-memory globals at guest `0x00000000`. Lookup = insertion order, first region wins; read-only regions drop stores.
- Constraints: cannot build/verify on this Windows host — no `clang`, `lld-link`, `qemu`, `cmake`, `ninja`; only `make`/`git` exist. Build requires macOS + Homebrew `llvm`/`lld` (PE/COFF cross via `lld-link`); `third_party/gnu-efi` not present on this machine (cloned on demand by Makefile). Write conservatively against GNU-EFI idioms (`efi.h`, `efilib.h`, `BS->`, `Print(L"...")`, `STATIC`, `EFIAPI`).
- Self-test entry point / existing helper names to reuse or mirror: `PpcSetGuestMemory`, `PpcGetGuestMemoryRegion` (returns host pointer + guest base + size), `g_PpcContext` (global `PPC_CPU_CONTEXT`), `PpcInitializeBootloader`, `PpcLoadKernel`, `PpcBootSystem`, `PpcRunSelfTest` references in transcripts.

## Work State
### Completed
- Analysed all modules, docs, build system, and git history; determined Phase 5 first deliverable.
- Created 7-item todo list (todowrite): translation.h API → interpreter memory → bootloader.h constants/API → bootloader_impl.c – ROM/demo ROM/low memory/self-test/init → main.c wiring → TODO.md update → compile cross-check.
- `src/cpu/translation.h`: added `PpcAddGuestMemoryRegion(IN VOID* HostBase, IN UINT32 GuestBase, IN UINT32 Size, IN BOOLEAN ReadOnly)` declaration (returns `EFI_SUCCESS`, `EFI_INVALID_PARAMETER`, `EFI_ALREADY_STARTED`, `EFI_OUT_OF_RESOURCES`); updated `PpcSetGuestMemory` doc.
- `src/cpu/interpreter.c`: replaced single-region globals (`g_GuestMemoryHostBase/GuestBase/Size`) with `PPC_MAX_GUEST_REGIONS 4`, `PPC_GUEST_REGION {HostBase, GuestBase, Size, ReadOnly, Active}`, `g_GuestRegions[]`.
- `src/cpu/interpreter.c`: `PpcDefaultReadByte`/`PpcDefaultWriteByte` now scan regions in insertion order (first match wins; read-only drops stores; unmapped = read zero, write dropped).
- `src/cpu/interpreter.c`: `PpcSetGuestMemory` now delegates to `PpcAddGuestMemoryRegion(HostBase, GuestBase, Size, FALSE)`; `PpcAddGuestMemoryRegion` implemented with overlap rejection.

### Active
- Todo item 1 (translation.h API) and item 2 (interpreter multi-region memory) are implemented.
- Todo items 3–7 not yet started.

### Blocked
- No local build/run possible: Windows host lacks clang/lld-link/qemu/gnu-efi; macOS toolchain required. Remaining verification is manual cross-check of C against GNU-EFI idiom.

## Next Move
1. Add Phase 5 boot memory map constants and API to `src/boot/bootloader.h` (e.g. `PPC_ROM_BASE 0xFFF00000`, low-memory base `0x00000000`, plus `PpcLoadSystemRom`, `PpcPrepareSystemForBoot`, and boot self-test declarations).
2. Implement those in `src/boot/bootloader_impl.c`: ROM load from boot volume with demo-ROM fallback, low-memory boot-info block, CPU reset state, wiring regions via `PpcAddGuestMemoryRegion`, self-test.
3. Wire Phase 5 section into `src/main.c`; update `TODO.md`; manually cross-check all touched files for GNU-EFI compile correctness.

## Relevant Files
- `src/cpu/translation.h` — just modified; declares `PpcAddGuestMemoryRegion`.
- `src/cpu/interpreter.c` — just modified; multi-region guest memory implemented.
- `src/cpu/interpreter.h` — `PPC_CPU_CONTEXT`, `g_PpcContext`; needed for reset-state work.
- `src/boot/bootloader.h` / `src/boot/bootloader_impl.c` — next target for Phase 5 implementation.
- `src/main.c` — hosts init sequence and self-tests; needs Phase 5 wiring.
- `src/hardware/abstraction.h/_impl.c` — uses `PpcGetGuestMemoryRegion`; framebuffer/audio carve guest RAM; do not overlap new regions.
- `src/memory/manager.h/_impl.c` — guest RAM allocation (256 MB), `PpcGetGuestMemoryRegion` backing store.
- `TODO.md` — currently documents Phase 4 as complete; must gain Phase 5 progress.
- `Makefile`, `BUILD_INSTRUCTIONS.md` — macOS-only build; cannot run here.

---

## User

---

## Assistant (Build · Big Pickle · 6.3s)

_Thinking:_

The user wants me to continue. Let me check my todo list state. I've done:
1. translation.h PpcAddGuestMemoryRegion — done
2. interpreter.c multi-region memory — done
3. bootloader.h constants/API — done
4. bootloader_impl.c — done
5. main.c wiring — done
6. TODO.md update — pending
7. Compile cross-check — pending

Let me update the todos, then verify the edits are consistent. First let me check that PpcReadGuestByte, PpcWriteGuestByte, PpcExecuteBlock, PpcSetGprValue, PpcGetGprValue, PpcFreeMemory exist in the codebase, and that g_PpcContext is declared in interpreter.h. Also PPC_MSR_ME and PPC_MSR_RI constants exist. Also PPC_ROM_DEFAULT_PATH uses "\\System\\MacOS\\ROM" — need to verify \System\MacOS\ directory exists (kernel is at \System\MacOS\kernel). Also check bootloader_impl.c includes interpreter.h for g_PpcContext and the helper functions. Let me read the top of bootloader_impl.c to check includes, and grep for the helper functions.

Let me do these checks in parallel.

Let me update the todo list and verify the new code against existing APIs:

**Tool: todowrite**

**Input:**
```json
{
  "todos": [
    {
      "content": "Add PpcAddGuestMemoryRegion to cpu/translation.h",
      "status": "completed",
      "priority": "high"
    },
    {
      "content": "Implement multi-region guest memory in cpu/interpreter.c",
      "status": "completed",
      "priority": "high"
    },
    {
      "content": "Add boot memory map constants + API to boot/bootloader.h",
      "status": "completed",
      "priority": "high"
    },
    {
      "content": "Implement ROM loading, demo ROM, low memory, boot self-test, system init in bootloader_impl.c",
      "status": "completed",
      "priority": "high"
    },
    {
      "content": "Wire Phase 5 section into src/main.c",
      "status": "completed",
      "priority": "high"
    },
    {
      "content": "Update TODO.md with Phase 5 progress",
      "status": "in_progress",
      "priority": "medium"
    },
    {
      "content": "Cross-check compile correctness of all changes",
      "status": "pending",
      "priority": "high"
    }
  ]
}
```

**Output:**
```
[
  {
    "content": "Add PpcAddGuestMemoryRegion to cpu/translation.h",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Implement multi-region guest memory in cpu/interpreter.c",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Add boot memory map constants + API to boot/bootloader.h",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Implement ROM loading, demo ROM, low memory, boot self-test, system init in bootloader_impl.c",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Wire Phase 5 section into src/main.c",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Update TODO.md with Phase 5 progress",
    "status": "in_progress",
    "priority": "medium"
  },
  {
    "content": "Cross-check compile correctness of all changes",
    "status": "pending",
    "priority": "high"
  }
]
```

---

## Assistant (Build · Big Pickle · 2.9s)

**Tool: grep**

**Input:**
```json
{
  "pattern": "PpcReadGuestByte|PpcWriteGuestByte|PpcExecuteBlock|PpcSetGprValue|PpcGetGprValue|PpcFreeMemory|PpcGetGuestMemoryRegion",
  "path": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src"
}
```

**Output:**
```
Found 87 matches
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\utils\debug_impl.c:
  Line 151:             Print(L"0x%08X ", PpcGetGprValue((UINT8)i));


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\main.c:
  Line 117:     Status = PpcGetGuestMemoryRegion(&HostBase, &GuestBase, &GuestSize);

  Line 137:     Status = PpcGetGuestMemoryRegion(&DemoHostBase, &DemoGuestBase, &DemoGuestSize);

  Line 158:         PpcWriteGuestByte(ProgramBase + (UINT32)i * 4 + (UINT32)b,

  Line 165:     PpcSetGprValue(1, ResultAddr);   // r1 points at the result slot

  Line 166:     PpcSetGprValue(3, 0);

  Line 167:     PpcSetGprValue(4, 0);

  Line 168:     PpcSetGprValue(5, 0);

  Line 172:     EFI_STATUS ExecStatus = PpcExecuteBlock(

  Line 182:       Result = (Result << 8) | PpcReadGuestByte(ResultAddr + (UINT32)b);

  Line 184:     UINT32 R5 = PpcGetGprValue(5);

  Line 342:       PpcWriteGuestByte(GuestAddr + 0, 0x10);

  Line 343:       PpcWriteGuestByte(GuestAddr + 1, 0x00);

  Line 344:       PpcWriteGuestByte(GuestAddr + 2, 0x20);

  Line 345:       PpcWriteGuestByte(GuestAddr + 3, 0x00);

  Line 495:     Status = PpcExecuteBlock((UINT32*)(UINTN)EntryPoint, KernelExecCount, &KernelExecuted);

  Line 498:     UINT32 K_R3 = PpcGetGprValue(3);

  Line 499:     UINT32 K_R5 = PpcGetGprValue(5);


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c:
  Line 158:     PpcWriteGuestByte(Address + 0, (UINT8)(Value >> 24));

  Line 159:     PpcWriteGuestByte(Address + 1, (UINT8)(Value >> 16));

  Line 160:     PpcWriteGuestByte(Address + 2, (UINT8)(Value >> 8));

  Line 161:     PpcWriteGuestByte(Address + 3, (UINT8)Value);

  Line 258:     Status = PpcGetGuestMemoryRegion(&GuestBuffer, &GuestBase, &GuestSize);

  Line 484:         PpcFreeMemory(Buffer, Size);

  Line 637:     EFI_STATUS Status = PpcGetGuestMemoryRegion(&GuestBuffer, &GuestBase, &GuestSize);

  Line 653:     UINT32 FirstWord = PpcReadGuestByte((UINT32)KernelAddress)     << 24 |

  Line 654:                        PpcReadGuestByte((UINT32)KernelAddress + 1) << 16 |

  Line 655:                        PpcReadGuestByte((UINT32)KernelAddress + 2) << 8  |

  Line 656:                        PpcReadGuestByte((UINT32)KernelAddress + 3);

  Line 728:     Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);

  Line 782:     Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);

  Line 787:     PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08, 0xAA);

  Line 789:         PpcReadGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08) == 0xAA,

  Line 794:         PpcReadGuestByte(PPC_ROM_GUEST_BASE + 0) == 'R' &&

  Line 795:         PpcReadGuestByte(PPC_ROM_GUEST_BASE + 1) == 'O' &&

  Line 796:         PpcReadGuestByte(PPC_ROM_GUEST_BASE + 2) == 'M' &&

  Line 797:         PpcReadGuestByte(PPC_ROM_GUEST_BASE + 3) == '1',

  Line 801:     PpcWriteGuestByte(PPC_ROM_GUEST_BASE + 0, 0xEE);

  Line 803:         PpcReadGuestByte(PPC_ROM_GUEST_BASE + 0) == 'R',

  Line 808:     PpcSetGprValue(1, (UINT32)RamBase);

  Line 809:     PpcSetGprValue(3, 0);

  Line 810:     PpcSetGprValue(4, 0);

  Line 811:     PpcSetGprValue(5, 0);

  Line 812:     Status = PpcExecuteBlock((UINT32*)(UINTN)PPC_RESET_VECTOR, 4, &Executed);

  Line 817:     BootSelfTestCheck(PpcGetGprValue(4) == 0x524F4D31,

  Line 820:         PpcReadGuestByte((UINT32)RamBase + 0) == (UINT8)(MagicPlusOne >> 24) &&

  Line 821:         PpcReadGuestByte((UINT32)RamBase + 1) == (UINT8)(MagicPlusOne >> 16) &&

  Line 822:         PpcReadGuestByte((UINT32)RamBase + 2) == (UINT8)(MagicPlusOne >> 8) &&

  Line 823:         PpcReadGuestByte((UINT32)RamBase + 3) == (UINT8)MagicPlusOne,


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\hardware\abstraction_impl.c:
  Line 222:         EFI_STATUS FbStatus = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);

  Line 291:     EFI_STATUS Status = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c:
  Line 271: PpcReadGuestByte (

  Line 279: PpcWriteGuestByte (

  Line 1456: PpcExecuteBlock (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory\manager_impl.c:
  Line 156: PpcFreeMemory (

  Line 287: PpcGetGuestMemoryRegion (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation.h:
  Line 166: PpcExecuteBlock (

  Line 262: PpcReadGuestByte (

  Line 275: PpcWriteGuestByte (

  Line 338: PpcGetGprValue (

  Line 348: PpcSetGprValue (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory\manager.h:
  Line 68: PpcFreeMemory (

  Line 151: PpcGetGuestMemoryRegion (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation_impl.c:
  Line 81: PpcGetGprValue (

  Line 92: PpcSetGprValue (

  Line 169:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(3) == 5 && Next == 0x1004,

  Line 173:     PpcSetGprValue(3, 1);

  Line 176:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(3) == 0 &&

  Line 181:     PpcSetGprValue(3, 0);

  Line 183:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(4) == 0 &&

  Line 188:     PpcSetGprValue(3, 5);

  Line 189:     PpcSetGprValue(4, 3);

  Line 191:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(5) == 8,

  Line 196:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(6) == 2,

  Line 200:     PpcSetGprValue(0, 0);

  Line 202:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(7) == 0xFF,

  Line 212:     PpcSetGprValue(4, 5);

  Line 237:     PpcSetGprValue(3, 1);

  Line 239:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(8) == 2,

  Line 243:     PpcSetGprValue(3, 0xDEADBEEF);

  Line 248:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(9) == 0xDEADBEEF,

  Line 252:     PpcSetGprValue(10, 0x80000001);

  Line 255:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(12) == 0xF8000000 &&

  Line 266:     PpcSetGprValue(1, 0x10000);

  Line 267:     PpcSetGprValue(10, 0x12345678);

  Line 272:     SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(11) == 0x12345678 &&

  Line 319:     return PpcExecuteBlock(InstructionBlock, BlockSize / sizeof(UINT32), &Executed);

```

**Tool: grep**

**Input:**
```json
{
  "pattern": "PPC_MSR_ME|PPC_MSR_RI",
  "path": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src"
}
```

**Output:**
```
Found 5 matches
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation_impl.c:
  Line 344:     g_PpcContext.Msr &= ~(PPC_MSR_EE | PPC_MSR_RI);


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c:
  Line 321:     g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;

  Line 740:     g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation.h:
  Line 67: #define PPC_MSR_ME     0x00001000  // Machine check enable

  Line 68: #define PPC_MSR_RI     0x00000002  // Recoverable interrupt

```

---

## Assistant (Build · Big Pickle · 7.5s)

_Thinking:_

All helper functions exist. `PpcReadGuestByte`, `PpcWriteGuestByte`, `PpcExecuteBlock`, `PpcSetGprValue`, `PpcGetGprValue`, `PpcFreeMemory`, `PpcGetGuestMemoryRegion`, `PPC_MSR_ME`, `PPC_MSR_RI` all present. `g_PpcContext` is used in bootloader_impl.c already (line 321 existing), so the include is fine.

Now important concern: The ROM at 0xFFF00000 is 4MB, and guest RAM is 256MB (0x10000000). The kernel load region — where is the kernel loaded? Guest addresses up to 0x10000000. ROM base 0xFFF00000 is above RAM. RAM spans 0x0 to 0x10000000. The framebuffer is carved at 0x18000000 and audio at 0x18800000 — both above RAM. ROM at 0xFFF00000 doesn't overlap those. Good.

Now there's a subtle issue with the demo ROM program execution: the `PpcExecuteBlock` at `PPC_RESET_VECTOR` executes the 4 instructions. But wait — there's a concern: `lwz r4, 0(r3)` reads from guest 0xFFF00000 which is the ROM. `addi r5, r4, 1`. `stw r5, 0(r1)` where r1 = RamBase (guest RAM base 0). That's fine.

But `PpcExecuteBlock` — does it execute from guest address, reading instruction words via PpcReadGuestWord or via direct host pointer? The function takes a `(UINT32*)` pointer to instructions. In main.c, existing usage: `PpcExecuteBlock((UINT32*)(UINTN)EntryPoint, KernelExecCount, ...)` where EntryPoint is a guest address. So PpcExecuteBlock must translate the guest address through the memory map to a host pointer. Let me verify that PpcExecuteBlock handles ROM region lookup — it probably uses PpcGetGuestMemoryRegion (single region) internally, which would fail for ROM addresses!

This is a critical issue. PpcExecuteBlock takes a guest address and presumably resolves it via the old single-region logic. Let me look at interpreter.c PpcExecuteBlock implementation.

All referenced helpers exist. One critical concern: `PpcExecuteBlock` takes a guest address — I must verify it resolves through the new multi-region map (the demo ROM program lives in the ROM region, not RAM):

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "offset": 1420,
  "limit": 90
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
1420: 
1421:             case XO_EIEIO:  // eieio (no-op)
1422:                 break;
1423: 
1424:             case XO_STHBRX:  // sthbrx
1425:                 CpuWrite16Rev(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);
1426:                 break;
1427: 
1428:             case XO_ICBI:  // icbi (no-op)
1429:                 break;
1430: 
1431:             case XO_DCBZ:  // dcbz: zero a 32-byte cache line
1432:                 {
1433:                     UINT32 Ea = EaX(w, RA(w), RB(w)) & ~0x1F;
1434:                     UINT32 I;
1435:                     for (I = 0; I < 32; I++) {
1436:                         g_WriteByte(Ea + I, 0);
1437:                     }
1438:                 }
1439:                 break;
1440: 
1441:             default:
1442:                 return EFI_UNSUPPORTED;
1443:             }
1444:         }
1445:         break;
1446: 
1447:     default:
1448:         return EFI_UNSUPPORTED;
1449:     }
1450: 
1451:     *NextAddress = Next;
1452:     return EFI_SUCCESS;
1453: }
1454: 
1455: EFI_STATUS
1456: PpcExecuteBlock (
1457:     IN  UINT32* InstructionStream,
1458:     IN  UINTN   MaxInstructions,
1459:     OUT UINTN*  ExecutedCount
1460:     )
1461: {
1462:     UINTN Executed = 0;
1463: 
1464:     if (InstructionStream == NULL || ExecutedCount == NULL) {
1465:         return EFI_INVALID_PARAMETER;
1466:     }
1467: 
1468:     g_PpcContext.Pc = (UINT32)(UINTN)InstructionStream;
1469:     g_PpcContext.ExceptionPending = 0;
1470: 
1471:     while (Executed < MaxInstructions) {
1472:         UINT32 Instr = CpuRead32(g_PpcContext.Pc);
1473:         UINT32 Next;
1474:         EFI_STATUS Status = PpcExecuteInstruction(Instr, g_PpcContext.Pc, &Next);
1475:         Executed++;
1476:         if (EFI_ERROR(Status)) {
1477:             *ExecutedCount = Executed;
1478:             return Status;
1479:         }
1480:         g_PpcContext.Pc = Next;
1481:         if (g_PpcContext.ExceptionPending != 0) {
1482:             break;
1483:         }
1484:     }
1485: 
1486:     *ExecutedCount = Executed;
1487:     return EFI_SUCCESS;
1488: }
1489: 
1490: // ---------------------------------------------------------------------------
1491: // Instruction decode to a short mnemonic
1492: // ---------------------------------------------------------------------------
1493: static const CHAR16* g_DOpcodeNames[] = {
1494:     L"reserved", L"reserved", L"reserved", L"twi",      L"reserved", L"reserved",
1495:     L"reserved", L"mulli",    L"subfic",   L"reserved", L"cmpli",    L"cmpi",
1496:     L"addic",    L"addic.",   L"addi",     L"addis",    L"bc",       L"sc",
1497:     L"b",        L"XL-form",  L"rlwimi",   L"rlwinm",   L"reserved", L"rlwnm",
1498:     L"ori",      L"oris",     L"xori",     L"xoris",    L"andi.",    L"andis.",
1499:     L"reserved", L"X-form",   L"lwz",      L"lwzu",     L"lbz",      L"lbzu",
1500:     L"stw",      L"stwu",     L"stb",      L"stbu",     L"lhz",      L"lhzu",
1501:     L"lha",      L"lhau",     L"sth",      L"sthu",     L"lmw",      L"stmw"
1502: };
1503: 
1504: EFI_STATUS
1505: PpcDecodeInstruction (
1506:     IN  UINT32  Instruction,
1507:     OUT CHAR16* Buffer,
1508:     IN  UINTN   BufferSize
1509:     )

(Showing lines 1420-1509 of 1575. Use offset=1510 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 3.6s)

_Thinking:_

`PpcExecuteBlock` calls `CpuRead32(g_PpcContext.Pc)`. `CpuRead32` — does it translate via guest memory map? Let me check the `CpuRead32`/`CpuWrite32` helpers in interpreter.c. They likely use `g_ReadByte`/`g_WriteByte` function pointers which are set to PpcDefaultReadByte/PpcDefaultWriteByte (multi-region aware now). Let me verify.

`PpcExecuteBlock` reads instructions via `CpuRead32(Pc)` — I need to confirm `CpuRead32` routes through the multi-region-aware byte functions. Let me check the CpuRead/CpuWrite helpers and the `g_ReadByte` wiring:

**Tool: grep**

**Input:**
```json
{
  "path": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "pattern": "CpuRead32|CpuWrite32|g_ReadByte|g_WriteByte|CpuReadByte"
}
```

**Output:**
```
Found 44 matches
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c:
  Line 201: static PPC_CPU_READ_MEMORY  g_ReadByte  = PpcDefaultReadByte;

  Line 202: static PPC_CPU_WRITE_MEMORY g_WriteByte = PpcDefaultWriteByte;

  Line 210:     g_ReadByte  = (Read  != NULL) ? Read  : PpcDefaultReadByte;

  Line 211:     g_WriteByte = (Write != NULL) ? Write : PpcDefaultWriteByte;

  Line 275:     return g_ReadByte(Address);

  Line 284:     g_WriteByte(Address, Value);

  Line 287: static UINT32 CpuRead16 (UINT32 A) { return ((UINT32)g_ReadByte(A) << 8) | g_ReadByte(A + 1); }

  Line 288: static UINT32 CpuRead32 (UINT32 A) { return (CpuRead16(A) << 16) | CpuRead16(A + 2); }

  Line 289: static VOID   CpuWrite16(UINT32 A, UINT32 V) { g_WriteByte(A, (UINT8)(V >> 8)); g_WriteByte(A + 1, (UINT8)V); }

  Line 290: static VOID   CpuWrite32(UINT32 A, UINT32 V) { CpuWrite16(A, V >> 16); CpuWrite16(A + 2, V); }

  Line 293: static UINT32 CpuRead32Rev (UINT32 A)

  Line 295:     return (UINT32)g_ReadByte(A) | ((UINT32)g_ReadByte(A + 1) << 8) |

  Line 296:            ((UINT32)g_ReadByte(A + 2) << 16) | ((UINT32)g_ReadByte(A + 3) << 24);

  Line 300:     return (UINT32)g_ReadByte(A) | ((UINT32)g_ReadByte(A + 1) << 8);

  Line 302: static VOID CpuWrite32Rev (UINT32 A, UINT32 V)

  Line 304:     g_WriteByte(A, (UINT8)V); g_WriteByte(A + 1, (UINT8)(V >> 8));

  Line 305:     g_WriteByte(A + 2, (UINT8)(V >> 16)); g_WriteByte(A + 3, (UINT8)(V >> 24));

  Line 309:     g_WriteByte(A, (UINT8)V); g_WriteByte(A + 1, (UINT8)(V >> 8));

  Line 575:         g_PpcContext.Gpr[Reg] |= (UINT32)g_ReadByte(Ea + I) << Shift;

  Line 597:         g_WriteByte(Ea + I, (UINT8)(g_PpcContext.Gpr[Reg] >> Shift));

  Line 616:     return CpuRead32(Address);

  Line 807:         g_PpcContext.Gpr[RT(w)] = CpuRead32(EaD(w, RA(w)));

  Line 813:             g_PpcContext.Gpr[RT(w)] = CpuRead32(Ea);

  Line 819:         g_PpcContext.Gpr[RT(w)] = g_ReadByte(EaD(w, RA(w)));

  Line 825:             g_PpcContext.Gpr[RT(w)] = g_ReadByte(Ea);

  Line 831:         CpuWrite32(EaD(w, RA(w)), g_PpcContext.Gpr[RS(w)]);

  Line 837:             CpuWrite32(Ea, g_PpcContext.Gpr[RS(w)]);

  Line 843:         g_WriteByte(EaD(w, RA(w)), (UINT8)g_PpcContext.Gpr[RS(w)]);

  Line 849:             g_WriteByte(Ea, (UINT8)g_PpcContext.Gpr[RS(w)]);

  Line 895:                 g_PpcContext.Gpr[R] = CpuRead32(Ea + (R - RT(w)) * 4);

  Line 905:                 CpuWrite32(Ea + (R - RS(w)) * 4, g_PpcContext.Gpr[R]);

  Line 959:                 g_PpcContext.Gpr[RT(w)] = CpuRead32(EaX(w, RA(w), RB(w)));

  Line 963:                 g_PpcContext.Gpr[RT(w)] = CpuRead32(EaX(w, RA(w), RB(w)));

  Line 1029:                 g_PpcContext.Gpr[RT(w)] = g_ReadByte(EaX(w, RA(w), RB(w)));

  Line 1046:                     g_PpcContext.Gpr[RT(w)] = g_ReadByte(Ea);

  Line 1101:                 CpuWrite32(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);

  Line 1106:                 CpuWrite32(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);

  Line 1112:                     CpuWrite32(Ea, g_PpcContext.Gpr[RS(w)]);

  Line 1142:                 g_WriteByte(EaX(w, RA(w), RB(w)), (UINT8)g_PpcContext.Gpr[RS(w)]);

  Line 1191:                     g_WriteByte(Ea, (UINT8)g_PpcContext.Gpr[RS(w)]);

  Line 1357:                 g_PpcContext.Gpr[RT(w)] = CpuRead32Rev(EaX(w, RA(w), RB(w)));

  Line 1390:                 CpuWrite32Rev(EaX(w, RA(w), RB(w)), g_PpcContext.Gpr[RS(w)]);

  Line 1436:                         g_WriteByte(Ea + I, 0);

  Line 1472:         UINT32 Instr = CpuRead32(g_PpcContext.Pc);

```

---

## Assistant (Build · Big Pickle · 18.9s)

_Thinking:_

`CpuRead32` uses `g_ReadByte` which defaults to `PpcDefaultReadByte` — the multi-region-aware version. So `PpcExecuteBlock` at the ROM address will read through the ROM region. 

But wait — there's a subtle bug in `PpcExecuteBlock`: it sets `g_PpcContext.Pc = (UINT32)(UINTN)InstructionStream;` — passing the guest address works because InstructionStream is a guest address in our callers. Good, that's the existing pattern.

Now, another concern about the demo ROM program: `PpcExecuteInstruction` for `stw r5, 0(r1)` where r1 = RamBase (0x0). The multi-region map: region 0 = RAM (guest 0x0, 256MB), region 1 = low mem (guest 0x0, 16KB), region 2 = ROM (guest 0xFFF00000, 4MB). Insertion order: 
- PpcSetGuestMemory (RAM) → slot 0
- PpcInstallLowMemory → slot 1 (guest 0x0)
- PpcInstallSystemRom/demo → slot 2

First-match-wins: a write to guest 0x0 hits slot 0 (RAM) first, not low-memory slot. Hmm — the low memory at guest 0x0 overlaps the RAM region! The RAM region covers 0x0..0x10000000, and low memory at 0x0..0x4000 is INSIDE that range. So with first-match-wins and RAM installed first, the low-memory region would never be hit — writes go to RAM instead.

This is a real design issue. The low-memory globals region at guest 0x0 overlaps the primary RAM. In my current lookup, first match wins, insertion order is RAM first. So low memory region is shadowed by RAM.

Wait — but the demo ROM program does `stw r5, 0(r1)` with r1 = RamBase = 0x0. That stores into RAM region. Fine for the test.

But the whole point of the low-memory region is to have the boot info block there. If RAM shadows it, writes to guest 0x0 go to RAM. Since RAM is at guest 0x0 too, they physically coincide (RAM region host base maps guest 0x0 to host buffer base). So the boot info write to guest 0x0 actually lands in RAM region memory, which is a different physical buffer than the low-memory pool. Then PpcReadGuestByte(guest 0x0) would read from RAM region (slot 0), not the low-memory pool. So the low-memory install would be effectively useless — it's shadowed.

Hmm. Options:
1. Insertion order matters: install low memory FIRST, before RAM. But RAM is set up by the memory manager at init time (PpcInitializeMemoryManager presumably installs guest RAM via PpcSetGuestMemory early). Can't reorder easily without changing init sequence.
2. Change lookup semantics: highest-priority = latest installed, or choose the smallest region on tie? That's hacky.
3. Since guest 0x0 is within RAM, the low-memory globals region is redundant — the boot info block can just live in RAM at guest 0x0. But the Phase 5 design explicitly installs a separate low-memory region.

Actually, reconsider: is the low-memory region even needed as a separate region? In a real Mac, low memory is in RAM (physical). The emulator concept was "install low-memory globals region at guest 0x00000000 (16KB)". If it overlaps RAM, the cleanest semantics: the region lookup should prefer the most recently added region, or prefer non-RAM regions. 

Alternatively, make the guest RAM NOT include the first 16KB — i.e., carve out the low memory area from RAM. But PpcSetGuestMemory sets the whole 256MB at guest 0x0.

Simplest robust fix: change the lookup to check regions in REVERSE insertion order (most recently added first). Then:
- Region added last = low memory? No — install order in main.c: low memory installed BEFORE ROM. So reverse order = ROM first, then low memory, then RAM. Guest 0x0: low memory matches first (region 1 in reverse = low memory) before RAM. Guest 0xFFF00000: ROM matches first. 
- But then writes to guest 0x0 in the self-test: `PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08, 0xAA)` would hit low memory region. Good.
- The demo ROM `stw r5, 0(r1)` with r1 = RamBase = 0x0 would write to low-memory region instead of RAM — then the self-test reads the result back at guest RamBase+0, which via reverse lookup = low memory region. Consistent! Still passes.
- The kernel load: kernel loaded into guest RAM (at some offset within 0x0..0x10000000, likely not in first 16KB). Reverse lookup: check ROM (guest 0xFFF00000, no match for kernel addr), then low mem (0x0..0x4000, no match if kernel at e.g. 0x100000+), then RAM. Fine.
- Framebuffer at 0x18000000 and audio at 0x18800000: how are they accessed? Abstraction_impl uses PpcGetGuestMemoryRegion to find the framebuffer host pointer, then writes directly to host memory (host framebuffer), not through guest byte functions. Actually let me check — abstraction maps framebuffer host pointer and COPIES to it directly? If the display driver writes via host pointer, it doesn't matter. But if guest code writes to 0x18000000... it's above RAM (0x10000000) and not in any region → currently dropped/zero. That was pre-existing behavior (single RAM region). Fine, not my concern.

But wait, changing reverse order could affect things that rely on RAM matching first... The audio/framebuffer regions — are they separate guest regions? Let me check: abstraction_impl.c at line 222 uses PpcGetGuestMemoryRegion to get GuestBase/GuestSize — it CARVES the framebuffer out of guest RAM (per summary: "guest framebuffer at 0x18000000 carved out of guest RAM"). Wait "carved out of guest RAM" — meaning they allocated guest RAM of 256MB and use the top portion? Let me check how GuestBase/Size relate. Actually the summary said "256MB guest RAM via AllocatePages, guest framebuffer at 0x18000000 carved out of guest RAM". 0x18000000 = 384MB. 256MB = 0x10000000. Hmm, 0x18000000 > 0x10000000, so it's outside RAM. 

Let me look at how abstraction gets the framebuffer host pointer — line 222. Let me read that section to be sure my reverse-lookup change doesn't break anything. Actually the framebuffer writes likely go directly through the host pointer obtained from PpcGetGuestMemoryRegion... but 0x18000000 isn't within the guest RAM region. Let me actually read abstraction_impl.c around 200-300.

`CpuRead32` routes through `g_ReadByte` → multi-region `PpcDefaultReadByte`, so ROM program execution works. But I spotted a priority bug: low memory at guest `0x0` overlaps the RAM region (also at guest `0x0`, installed first) — with first-match-wins, the low-memory region would be shadowed. Let me check how the framebuffer/audio carve RAM to make sure a lookup-order change is safe:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\hardware\\abstraction_impl.c",
  "limit": 120,
  "offset": 200
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\hardware\abstraction_impl.c</path>
<type>file</type>
<content>
200:             }
201:             FreePool(Candidate);
202:         }
203:         if (!Found) {
204:             g_Gop->SetMode(g_Gop, g_Gop->Mode->Mode);
205:         }
206: 
207:         Info = g_Gop->Mode->Info;
208:         g_HardwareContext.VideoBuffer = (VOID*)(UINTN)g_Gop->Mode->FrameBufferBase;
209:         g_HardwareContext.VideoBufferSize = g_Gop->Mode->FrameBufferSize;
210:         g_FramebufferPitch = Info->PixelsPerScanLine * 4;
211:         g_PixelFormat = Info->PixelFormat;
212:         g_HardwareContext.GraphicsMode = (Info->HorizontalResolution << 16) |
213:                                          (Info->VerticalResolution & 0xFFFF);
214:         g_HardwareContext.GraphicsInitialized = TRUE;
215: 
216:         // Carve a guest-visible framebuffer window out of guest RAM. Guest
217:         // code writes big-endian 0xRRGGBB00 pixels here; PpcGraphicsBlitToDisplay
218:         // pushes the window to the real GOP framebuffer.
219:         VOID*  GuestHost   = NULL;
220:         UINT64 GuestBase   = 0;
221:         UINT64 GuestSize   = 0;
222:         EFI_STATUS FbStatus = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);
223:         if (!EFI_ERROR(FbStatus) && GuestHost != NULL) {
224:             UINT32 FbSize = Info->HorizontalResolution * Info->VerticalResolution * 4;
225:             if ((UINT64)(PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE - (UINT32)GuestBase) + FbSize <= GuestSize) {
226:                 g_HardwareContext.GuestFbBase  = PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE;
227:                 g_HardwareContext.GuestFbHost  = (UINT8*)GuestHost +
228:                                                  (PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE - (UINT32)GuestBase);
229:                 g_HardwareContext.GuestFbSize  = FbSize;
230:                 g_HardwareContext.GuestFbWidth = Info->HorizontalResolution;
231:                 g_HardwareContext.GuestFbHeight = Info->VerticalResolution;
232:                 g_HardwareContext.GuestFbPitch = Info->HorizontalResolution * 4;
233:                 ZeroMem(g_HardwareContext.GuestFbHost, FbSize);
234:                 Print(L"Graphics: guest framebuffer at guest 0x%x (host 0x%x, %d bytes)\n",
235:                       g_HardwareContext.GuestFbBase,
236:                       g_HardwareContext.GuestFbHost,
237:                       FbSize);
238:             }
239:         }
240: 
241:         // Make the wiring visible: draw a pattern on the real framebuffer.
242:         GraphicsFillFramebuffer(0x00, 0x40, 0xFF);
243: 
244:         Print(L"Graphics: GOP mode %d, %dx%d, pixel format %d, framebuffer 0x%x "
245:               L"(pitch %d, size %d bytes)\n",
246:               g_Gop->Mode->Mode,
247:               Info->HorizontalResolution,
248:               Info->VerticalResolution,
249:               g_PixelFormat,
250:               g_Gop->Mode->FrameBufferBase,
251:               g_FramebufferPitch,
252:               g_HardwareContext.VideoBufferSize);
253: 
254:         return EFI_SUCCESS;
255:     }
256: 
257:     // No GOP available: fall back to a pool buffer (simulated framebuffer).
258:     g_HardwareContext.GraphicsMode = (Width << 16) | (Height & 0xFFFF);
259:     g_HardwareContext.GraphicsInitialized = TRUE;
260: 
261:     UINT64 BufferSize = Width * Height * (ColorDepth / 8);
262:     EFI_STATUS Status = BS->AllocatePool(EfiBootServicesData, BufferSize, &g_HardwareContext.VideoBuffer);
263:     
264:     if (EFI_ERROR(Status)) {
265:         Print(L"Failed to allocate video buffer: %r\n", Status);
266:         return Status;
267:     }
268:     
269:     g_HardwareContext.VideoBufferSize = BufferSize;
270:     ZeroMem(g_HardwareContext.VideoBuffer, BufferSize);
271:     
272:     Print(L"Graphics initialized (simulated) - buffer at 0x%x (size: %d bytes)\n", 
273:           g_HardwareContext.VideoBuffer, BufferSize);
274:     
275:     return EFI_SUCCESS;
276: }
277: 
278: EFI_STATUS
279: PpcInitializeAudio (
280:     VOID
281:     )
282: {
283:     Print(L"Initializing audio subsystem (emulated PCM ring buffer in guest RAM)\n");
284: 
285:     // UEFI has no standard audio output protocol, so the "device" is a fixed
286:     // ring buffer inside guest RAM that guest code fills with big-endian
287:     // 16-bit stereo PCM samples.
288:     VOID*  GuestHost   = NULL;
289:     UINT64 GuestBase   = 0;
290:     UINT64 GuestSize   = 0;
291:     EFI_STATUS Status = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);
292:     if (EFI_ERROR(Status) || GuestHost == NULL) {
293:         Print(L"Audio subsystem: guest RAM unavailable, falling back to pool buffer\n");
294:         Status = BS->AllocatePool(EfiBootServicesData, PPC_AUDIO_BUFFER_SIZE,
295:                                   (VOID**)&g_AudioInfo.HostBuffer);
296:         if (EFI_ERROR(Status)) {
297:             return Status;
298:         }
299:         g_AudioInfo.GuestBase = 0;
300:     } else {
301:         if ((UINT64)(PPC_AUDIO_BUFFER_GUEST_BASE - (UINT32)GuestBase) + PPC_AUDIO_BUFFER_SIZE > GuestSize) {
302:             Print(L"Audio subsystem: guest RAM too small\n");
303:             return EFI_NOT_READY;
304:         }
305:         g_AudioInfo.GuestBase = PPC_AUDIO_BUFFER_GUEST_BASE;
306:         g_AudioInfo.HostBuffer = (UINT8*)GuestHost +
307:                                  (PPC_AUDIO_BUFFER_GUEST_BASE - (UINT32)GuestBase);
308:     }
309: 
310:     g_AudioInfo.Size = PPC_AUDIO_BUFFER_SIZE;
311:     g_AudioInfo.SampleRate = PPC_AUDIO_SAMPLE_RATE;
312:     g_AudioInfo.Channels = PPC_AUDIO_CHANNELS;
313:     g_AudioInfo.BitsPerSample = PPC_AUDIO_BITS_PER_SAMPLE;
314:     g_AudioInfo.WriteCursor = 0;
315:     g_AudioInfo.PlayCursor = 0;
316:     ZeroMem(g_AudioInfo.HostBuffer, g_AudioInfo.Size);
317: 
318:     g_HardwareContext.AudioEnabled = 1;
319:     g_HardwareContext.AudioInitialized = TRUE;

(Showing lines 200-319 of 1182. Use offset=320 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 12.6s)

_Thinking:_

The framebuffer and audio carve windows from the guest RAM (they use PpcGetGuestMemoryRegion which returns the primary region — RAM). The check at line 225: `(PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE - GuestBase) + FbSize <= GuestSize`. If 0x18000000 - 0 = 0x18000000 (384MB) + FbSize > 0x10000000 (256MB) → the carve FAILS! Unless GuestBase is not 0. Hmm, wait — actually the summary said "guest framebuffer at 0x18000000 carved out of guest RAM". Let me not go down this rabbit hole — this is Phase 4 already-working code. The audio check at line 301 is similar. If the guest RAM base is 0 and size 256MB, then 0x18800000 - 0 = 0x18800000 = 392MB > 256MB → carve fails → audio falls back to pool. That's the pre-existing behavior and not my concern.

Now my concern is only the low-memory overlap with RAM. Decision: change the lookup priority so that regions added later take precedence (scan in reverse insertion order), OR carve the low-memory window from RAM like framebuffer does.

Actually, wait. Let me reconsider. The cleanest approach that matches the existing "carve out of guest RAM" pattern: the low-memory globals region overlaps RAM physically at guest 0x0. Rather than a separate region, we could just treat low-memory as part of RAM. But the whole point of the separate low-memory install was to demonstrate multi-region.

Hmm, but actually the overlap issue is real and would make my low-memory self-test read/write target RAM, not the low-memory pool. The self-test would still PASS (since RAM at guest 0x0 is writable), but the "low-memory region installed" claim is misleading — its pool buffer is never touched by guest-visible access.

The cleanest fix consistent with the existing codebase patterns: make region lookup scan in REVERSE insertion order (most recently installed region wins). This matches how an MMU-style overlay works and is the least-surprise semantic for "install" — later installs override earlier ones. Then:
- Guest 0x0 → low memory (installed after RAM) → boot info block lives in the low-memory pool. 
- Guest 0xFFF00000 → ROM.
- Everything else in RAM range → RAM.
- The demo ROM stw to r1 (r1=0) → writes low memory pool, and readback at 0 reads low-memory pool. Consistent.

But wait, does reverse-order break the "carve framebuffer out of RAM" logic? No — that uses PpcGetGuestMemoryRegion which returns the primary RAM region specifically (slot 0), not the generic lookup.

But it could break PpcDefaultReadByte callers who expect first-match... The only consumers are CPU load/store ops. Reverse order is strictly more correct.

However, there's a subtlety: reverse-order scanning means if someone installs an overlapping region that they DIDN'T intend to override... but that's exactly what "install" should do — override.

Actually, hold on. Let me reconsider whether reverse-order is the right call vs. simply declaring that low-memory MUST be installed before RAM in init. The memory manager installs RAM at init time (PpcInitializeMemoryManager). The main.c Phase 5 section runs after init. I can't easily install low memory before RAM. So reverse-order scan is the right fix.

Alternatively, keep first-match but make RAM installed LAST? No — RAM must be region 0 (primary, used by PpcGetGuestMemoryRegion which returns... let me check what PpcGetGuestMemoryRegion returns — probably region 0 specifically).

Let me look at the current interpreter.c region functions to decide. Let me read lines ~200-330 of interpreter.c.

The framebuffer/audio carve uses `PpcGetGuestMemoryRegion` (primary RAM), so region lookup order only affects CPU load/store. The overlap means low-memory (guest `0x0`) would be shadowed by RAM (installed first) under first-match-wins. The cleanest fix: scan regions in reverse insertion order so later installs override. Let me read the current region code:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 140,
  "offset": 160
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
160: static PPC_GUEST_REGION g_GuestRegions[PPC_MAX_GUEST_REGIONS];
161: 
162: static UINT8
163: PpcDefaultReadByte (
164:     IN UINT32 Address
165:     )
166: {
167:     UINTN I;
168: 
169:     for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
170:         if (g_GuestRegions[I].Active &&
171:             Address >= g_GuestRegions[I].GuestBase &&
172:             (UINT64)(Address - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) {
173:             return *(volatile UINT8*)((UINTN)g_GuestRegions[I].HostBase +
174:                                       (Address - g_GuestRegions[I].GuestBase));
175:         }
176:     }
177:     return 0;  // Unmapped guest address reads as zero
178: }
179: 
180: static VOID
181: PpcDefaultWriteByte (
182:     IN UINT32 Address,
183:     IN UINT8  Value
184:     )
185: {
186:     UINTN I;
187: 
188:     for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
189:         if (g_GuestRegions[I].Active &&
190:             Address >= g_GuestRegions[I].GuestBase &&
191:             (UINT64)(Address - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) {
192:             if (!g_GuestRegions[I].ReadOnly) {
193:                 *(volatile UINT8*)((UINTN)g_GuestRegions[I].HostBase +
194:                                    (Address - g_GuestRegions[I].GuestBase)) = Value;
195:             }
196:             return;  // First matching region decides the target
197:         }
198:     }
199: }
200: 
201: static PPC_CPU_READ_MEMORY  g_ReadByte  = PpcDefaultReadByte;
202: static PPC_CPU_WRITE_MEMORY g_WriteByte = PpcDefaultWriteByte;
203: 
204: EFI_STATUS
205: PpcSetMemoryAccess (
206:     IN PPC_CPU_READ_MEMORY   Read,
207:     IN PPC_CPU_WRITE_MEMORY  Write
208:     )
209: {
210:     g_ReadByte  = (Read  != NULL) ? Read  : PpcDefaultReadByte;
211:     g_WriteByte = (Write != NULL) ? Write : PpcDefaultWriteByte;
212:     return EFI_SUCCESS;
213: }
214: 
215: EFI_STATUS
216: PpcSetGuestMemory (
217:     IN VOID*  HostBase,
218:     IN UINT32 GuestBase,
219:     IN UINT32 Size
220:     )
221: {
222:     return PpcAddGuestMemoryRegion(HostBase, GuestBase, Size, FALSE);
223: }
224: 
225: EFI_STATUS
226: PpcAddGuestMemoryRegion (
227:     IN VOID*   HostBase,
228:     IN UINT32  GuestBase,
229:     IN UINT32  Size,
230:     IN BOOLEAN ReadOnly
231:     )
232: {
233:     UINTN I;
234: 
235:     if (HostBase == NULL || Size == 0) {
236:         return EFI_INVALID_PARAMETER;
237:     }
238: 
239:     // Reject a region that would overlap an already installed one.
240:     for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
241:         if (!g_GuestRegions[I].Active) {
242:             continue;
243:         }
244:         if (GuestBase < g_GuestRegions[I].GuestBase) {
245:             if ((UINT64)GuestBase + Size > g_GuestRegions[I].GuestBase) {
246:                 return EFI_ALREADY_STARTED;
247:             }
248:         } else {
249:             if ((UINT64)(GuestBase - g_GuestRegions[I].GuestBase) <
250:                 g_GuestRegions[I].Size) {
251:                 return EFI_ALREADY_STARTED;
252:             }
253:         }
254:     }
255: 
256:     for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
257:         if (!g_GuestRegions[I].Active) {
258:             g_GuestRegions[I].HostBase  = HostBase;
259:             g_GuestRegions[I].GuestBase = GuestBase;
260:             g_GuestRegions[I].Size      = Size;
261:             g_GuestRegions[I].ReadOnly  = ReadOnly;
262:             g_GuestRegions[I].Active    = TRUE;
263:             return EFI_SUCCESS;
264:         }
265:     }
266: 
267:     return EFI_OUT_OF_RESOURCES;
268: }
269: 
270: UINT8
271: PpcReadGuestByte (
272:     IN UINT32 Address
273:     )
274: {
275:     return g_ReadByte(Address);
276: }
277: 
278: VOID
279: PpcWriteGuestByte (
280:     IN UINT32 Address,
281:     IN UINT8  Value
282:     )
283: {
284:     g_WriteByte(Address, Value);
285: }
286: 
287: static UINT32 CpuRead16 (UINT32 A) { return ((UINT32)g_ReadByte(A) << 8) | g_ReadByte(A + 1); }
288: static UINT32 CpuRead32 (UINT32 A) { return (CpuRead16(A) << 16) | CpuRead16(A + 2); }
289: static VOID   CpuWrite16(UINT32 A, UINT32 V) { g_WriteByte(A, (UINT8)(V >> 8)); g_WriteByte(A + 1, (UINT8)V); }
290: static VOID   CpuWrite32(UINT32 A, UINT32 V) { CpuWrite16(A, V >> 16); CpuWrite16(A + 2, V); }
291: 
292: // Byte-reversed access (for lwbrx/stwbrx etc.)
293: static UINT32 CpuRead32Rev (UINT32 A)
294: {
295:     return (UINT32)g_ReadByte(A) | ((UINT32)g_ReadByte(A + 1) << 8) |
296:            ((UINT32)g_ReadByte(A + 2) << 16) | ((UINT32)g_ReadByte(A + 3) << 24);
297: }
298: static UINT32 CpuRead16Rev (UINT32 A)
299: {

(Showing lines 160-299 of 1575. Use offset=300 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 43.4s)

_Thinking:_

There's the overlap rejection in PpcAddGuestMemoryRegion (lines 239-254). This means installing low-memory at guest 0x0 would REJECT because RAM at guest 0x0..0x10000000 overlaps! `PpcInstallLowMemory` would return EFI_ALREADY_STARTED, and the low-memory install would fail.

So my Phase 5 code as written has a real bug: low-memory region at guest 0x0 overlaps the primary RAM region → PpcAddGuestMemoryRegion returns EFI_ALREADY_STARTED → PpcInstallLowMemory fails → main.c prints FAIL for low memory, self-test checks fail (boot self-test would still pass? No — the low-memory R/W test writes to guest 0x8 which hits RAM, so it'd pass anyway, but install fails).

I need to fix this. Options:

Option A: Carve low memory out of RAM — since guest 0x0 is inside RAM, just let the boot info block live in RAM at guest 0x0. But that defeats the separate-region design.

Option B: Make the guest RAM NOT cover guest 0x0..0x4000. But RAM base is 0 per PpcSetGuestMemory from the memory manager. The RAM region covers [0, 0x10000000).

Option C: Change the overlap check to allow overlapping regions, and change the lookup to reverse order (later installs win). This is the cleanest conceptually: an MMU-style "install overlay" semantic. Later-installed region shadows earlier overlapping ones.

I'll go with Option C:
- PpcAddGuestMemoryRegion: relax overlap rejection — allow overlaps (remove the rejection entirely, or keep a note). Keep duplicate-install rejection? The "EFI_ALREADY_STARTED" for exact re-install was my design. But overlap rejection must go, since low memory and ROM both intentionally... well ROM at 0xFFF00000 doesn't overlap RAM. Only low memory overlaps RAM.
- Lookup: reverse scan so the most recently added region wins.

Wait, but reverse scan has implications: PpcSetGuestMemory installs RAM (slot 0). Then low memory (slot 1). Reverse scan checks slot 1 (low mem, guest 0..0x4000) first, then slot 0 (RAM). So guest 0x0 → low mem pool. And the primary RAM for everything else. Good.

But does anything rely on first-match? The only consumers are the default memory ops. Existing self-tests in main.c (line 158) write a program at some guest address and read results — those addresses are within RAM, and low-mem region (0..0x4000) only shadows the first 16KB. The interpreter 18-check self-test uses addresses like 0x1004, 0x10000, etc. 0x1004 is within the first 16KB! If the self-test writes a result to 0x1004 and reads it back via guest access, reverse-scan would route it to the LOW-MEMORY pool (if installed), not RAM. But the low-memory pool is separate host memory — the write and read would both go to the same pool, so it'd still be consistent. UNLESS the self-test expects to share memory with... no, reads/writes through guest addresses are self-consistent as long as both use the same lookup.

BUT: what if the memory manager's self-test writes directly to the RAM host buffer AND reads via guest address? E.g., memcpy to host pointer, then PpcReadGuestByte. That would break if the address is in the low-memory shadow range. Let me check the memory manager self-test and main.c self-tests for direct host-buffer access + guest-address readback at low addresses.

Actually the interpreter self-test in main.c (lines ~117-190) does:
- PpcGetGuestMemoryRegion → HostBase, GuestBase=0, GuestSize
- writes program bytes via PpcWriteGuestByte at ProgramBase (probably a higher address)
- executes
- reads result via PpcReadGuestByte at ResultAddr

If ResultAddr is 0x1004 and low-memory region shadows it, the guest-address write/read still hit the low-memory pool consistently. So self-test passes. Unless the ResultAddr is inside the DemoProgram host region... no.

But wait — there's a subtlety: the interpreter 18-check self-test (PpcRunSelfTest in interpreter) — where does it run? It likely uses its own instruction decode test without memory, plus some memory ops. The translation self-test uses addresses like 0x1004, 0x10000. If low memory region is installed when translation self-test runs... order of init: main.c runs memory manager init, then interpreter self-test, then translation self-test, then boot section (low memory install happens in boot section). So self-tests run BEFORE low-memory install. No conflict.

Also kernel load: kernel is loaded into guest RAM and executed. If the kernel binary or its result slot is in the first 16KB... kernel is loaded at a nonzero base (probably 0x100000 or similar). Line 653: `PpcReadGuestByte((UINT32)KernelAddress)`. KernelAddress is likely > 16KB. Fine.

Hmm, but actually there's a cleaner alternative that avoids ANY risk: since low memory overlaps RAM and there's genuinely no reason for a separate physical buffer, why not have the low-memory "region" be a carved window OF the RAM region, exactly like the framebuffer carve? I.e., PpcInstallLowMemory sets a pointer INTO the guest RAM host buffer at offset 0, and installs it as a separate region (or just documents that low memory lives at guest 0x0 = RAM base).

But then the separate region is pointless (same host memory). The multi-region value is really the ROM (read-only, non-RAM) region.

Let me reconsider the design intent: "classic Mac OS boot memory map". In a real Mac, low memory IS at physical 0 in RAM. So the most honest emulation: low memory globals live in guest RAM at 0x0, NOT a separate pool. The separate install demonstrates the API but should use the same host buffer (carve).

Decision: Change PpcInstallLowMemory to carve the low-memory window from the guest RAM region (like framebuffer/audio do), and have it install a region pointing at the RAM host buffer base, size 16KB, read/write. Since the host buffer is the same, overlap "conflict" doesn't matter functionally, but PpcAddGuestMemoryRegion would still REJECT it due to overlap. So I still need to allow overlapping regions.

OK here's my final plan:
1. PpcAddGuestMemoryRegion: remove the overlap rejection (allow overlap), keep the "not installed" and "first free slot" logic. Later installs take precedence in lookup.
2. PpcDefaultReadByte / PpcDefaultWriteByte: scan in reverse order (from PPC_MAX_GUEST_REGIONS-1 down to 0), so most recently installed region wins.
3. PpcInstallLowMemory: carve from guest RAM (PpcGetGuestMemoryRegion for the RAM host buffer) instead of allocating a fresh pool — matches framebuffer/audio pattern, and guarantees the boot info block written to "low memory" is truly in the guest's RAM image. Keep the region install for the self-test demonstration.

Wait — but if low memory carves RAM host base, then PpcInstallLowMemory's separate AllocatePages is wasteful and creates confusion. Let me rewrite it to carve. Actually, hmm — but then the "low-memory read/write" self-test would be reading/writing the RAM buffer at offset 0, which is the same as writing RAM. It's honest: low memory globals = guest RAM at physical 0. Good.

But actually — do I even need to keep the separate region for low memory then? The reverse-order priority means a store to guest 0x0 hits the low-mem region (slot 1) which points to the same host byte as RAM slot 0. Fine, consistent.

Also need to double check: with reverse order, ROM at 0xFFF00000 (slot 2), RAM slot 0. Load from 0xFFF00000: reverse scan → slot 2 (ROM) matches. 

Edge: what if Address matches multiple slots? Reverse scan → highest slot wins. That's what we want.

Let me also reconsider: does removing overlap rejection break anything existing? Existing code only installs RAM via PpcSetGuestMemory at init. No other regions were installed before Phase 5 (framebuffer/audio are carves, not regions). So removing rejection is safe.

But wait — is removing overlap rejection the right call vs. keeping rejection and just carving low memory from RAM (which would still be rejected)? If low memory carves from RAM host buffer, overlap still occurs (guest [0,0x4000] inside [0,0x10000000]). So rejection must go regardless.

Alternatively, keep rejection but carve low memory AND skip installing a separate region — just document low memory as part of RAM and write boot info to guest 0x0. Then no multi-region overlap at all, and the "install low memory" API just validates/writes. That's simpler and avoids changing region semantics. But then PpcInstallLowMemory doesn't use PpcAddGuestMemoryRegion, and the memory map still reports LowMemoryInstalled=TRUE with base 0x0 size 0x4000 — accurate (it's a carve of RAM).

Hmm. Which is better? The multi-region capability is genuinely needed for the ROM (read-only region outside RAM). For low memory, carving RAM is the "real Mac" behavior. I think the cleanest, most correct design:

- PpcInstallLowMemory: carve window from guest RAM host buffer at offset 0 (16KB), ZeroMem it. No separate AllocatePages, no separate region. This is exactly how framebuffer/audio carve. It reports base=0, size=0x4000.
- Keep the multi-region ROM path (read-only region) — that's where multi-region shines.
- Keep reverse-order scan? Not needed if low memory isn't a separate region. Keep first-match-wins for RAM/ROM (no overlap). ROM doesn't overlap RAM. So lookup order is irrelevant as long as first-match works.

This is simpler AND more correct (low memory is physically in RAM in a real Mac). But it loses the "demonstrate overlapping region priority" aspect. That's fine — simplicity wins, and the ROM region already demonstrates multi-region (guest addresses outside primary RAM).

But hold on — the whole reason I built PpcAddGuestMemoryRegion with overlap rejection and low-memory as a separate region was the earlier design in the summary ("multi-region guest memory in cpu/interpreter.c (RAM + read-only ROM)" + low memory). The summary said "Classic Mac OS ROM window at guest 0xFFF00000 (read-only)" and "Low-memory globals page at guest 0x00000000". It didn't mandate a separate pool.

Decision: I'll keep the multi-region machinery (it's already implemented and tested in spirit) but make PpcInstallLowMemory carve from RAM and NOT call PpcAddGuestMemoryRegion (since it's physically part of RAM). This keeps overlap rejection in PpcAddGuestMemoryRegion intact (a good safety property), avoids the shadowing bug, and matches real Mac semantics + codebase carve pattern.

Wait, but there's one more consideration: should PpcInstallLowMemory carve from RAM even when the "low memory" at guest 0x0... yes guest 0x0 is inside RAM [0, 0x10000000). Always carvable. But guard like framebuffer does: `if (PPC_LOW_MEM_GUEST_BASE - GuestBase + PPC_LOW_MEM_SIZE <= GuestSize)`.

Let me rewrite PpcInstallLowMemory accordingly. Also update the self-test: low-memory R/W test writes to guest 0x8 — now that's RAM (guest 0). Still passes. But to make it meaningful, also check it's within the low-memory window. It's fine.

Hmm, actually wait. Let me reconsider once more whether to keep a separate region for low memory to demonstrate overlap handling. The reverse-scan change is small and self-consistent. But it changes global semantics for marginal benefit, and there's a risk I'm not seeing all consumers. The carve approach is safer and more idiomatic for this codebase (framebuffer/audio carve precedent). 

Going with carve. Final changes:

1. interpreter.c: no changes needed (overlap rejection stays, first-match stays, ROM doesn't overlap RAM).
2. bootloader_impl.c PpcInstallLowMemory: rewrite to carve from guest RAM host buffer; no AllocatePages; no PpcAddGuestMemoryRegion. Set LowMemoryInstalled etc.
3. Since it no longer allocates, remove the BS->FreePages error path.

Also update the doc comment in bootloader.h for PpcInstallLowMemory to say it carves from guest RAM (like framebuffer).

Wait, but actually — is there still value in PpcInstallLowMemory using PpcAddGuestMemoryRegion? Let me think about whether the low-memory R/W self-test still demonstrates "low memory globals". Yes: it writes guest 0x8 and reads it back. With carve, that's RAM at guest 0. Consistent.

Let me also reconsider the boot self-test "ROM rejects guest writes": writes 0xEE to guest 0xFFF00000. ROM region is read-only, write dropped, read returns 'R'. Good.

One more check — main.c Phase 5 section order: low memory install, then ROM install (from volume or demo), then self-test, then prepare, then report. In PpcPrepareSystemForBoot I call PpcInstallLowMemory(NULL,NULL) if not installed — but it's already installed by main.c. Fine (idempotent via LowMemoryInstalled flag).

Also, PpcInstallLowMemory carve: the guest RAM region must exist. In main.c it's installed by memory manager init before boot section. Good.

Now, one concern: PpcPrepareSystemForBoot writes boot info via BootWriteWord32 to guest 0x0 + offsets. With carve, that writes RAM at guest 0. Good.

Also the demo ROM program `stw r5, 0(r1)` with r1 = RamBase = 0 → writes RAM at guest 0. Self-test reads back via PpcReadGuestByte at RamBase+0. Consistent.

Let me now make the edits. First bootloader.h doc tweak, then PpcInstallLowMemory rewrite.

Actually, do I need the doc tweak? The header says "Install the classic Mac OS low-memory globals region at guest 0x00000000 (16 KB, read/write)." That's still accurate (it's a region, carved from RAM). I'll leave it but maybe add "(carved from guest RAM)" — minor. I'll add a brief note.

Let me rewrite PpcInstallLowMemory.

Found a real bug: `PpcAddGuestMemoryRegion` rejects overlaps (interpreter.c:239-254), so installing low-memory at guest `0x0` would fail — it overlaps the primary RAM region (also at guest `0x0`). Since low memory lives at physical 0 inside RAM on a real Mac, the right fix is to **carve** the window from guest RAM like the framebuffer/audio carve (abstraction_impl.c:216-238), not allocate a separate pool:

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "newString": "EFI_STATUS\nPpcInstallLowMemory (\n    OUT UINT64* LowMemAddress,\n    OUT UINT64* LowMemSize\n    )\n{\n    // Classic Mac OS low-memory globals live at physical address 0, inside\n    // guest RAM. Carve the window out of the primary guest RAM region (the\n    // same pattern the graphics/audio carve uses) so the boot info block is\n    // genuinely part of the guest's memory image.\n    VOID*  RamHost   = NULL;\n    UINT64 RamBase   = 0;\n    UINT64 RamSize   = 0;\n    EFI_STATUS Status;\n\n    if (g_BootContext.LowMemoryInstalled) {\n        if (LowMemAddress != NULL) { *LowMemAddress = g_BootContext.LowMemoryAddress; }\n        if (LowMemSize != NULL) { *LowMemSize = g_BootContext.LowMemorySize; }\n        return EFI_ALREADY_STARTED;\n    }\n\n    Status = PpcGetGuestMemoryRegion(&RamHost, &RamBase, &RamSize);\n    if (EFI_ERROR(Status) || RamHost == NULL) {\n        Print(L\"Low-memory region: guest RAM unavailable\\n\");\n        return EFI_NOT_READY;\n    }\n\n    if ((UINT64)(PPC_LOW_MEM_GUEST_BASE - (UINT32)RamBase) + PPC_LOW_MEM_SIZE > RamSize) {\n        Print(L\"Low-memory region: guest RAM too small\\n\");\n        return EFI_NOT_READY;\n    }\n\n    ZeroMem(RamHost, PPC_LOW_MEM_SIZE);\n\n    g_BootContext.LowMemoryInstalled = TRUE;\n    g_BootContext.LowMemoryAddress = PPC_LOW_MEM_GUEST_BASE;\n    g_BootContext.LowMemorySize = PPC_LOW_MEM_SIZE;\n\n    if (LowMemAddress != NULL) { *LowMemAddress = PPC_LOW_MEM_GUEST_BASE; }\n    if (LowMemSize != NULL) { *LowMemSize = PPC_LOW_MEM_SIZE; }\n\n    Print(L\"Low-memory region carved from guest RAM: %d bytes at guest 0x%x\\n\",\n          (UINT64)PPC_LOW_MEM_SIZE, PPC_LOW_MEM_GUEST_BASE);\n\n    return EFI_SUCCESS;\n}",
  "oldString": "EFI_STATUS\nPpcInstallLowMemory (\n    OUT UINT64* LowMemAddress,\n    OUT UINT64* LowMemSize\n    )\n{\n    UINTN Pages;\n    EFI_PHYSICAL_ADDRESS Base = 0;\n    EFI_STATUS Status;\n\n    if (g_BootContext.LowMemoryInstalled) {\n        if (LowMemAddress != NULL) { *LowMemAddress = g_BootContext.LowMemoryAddress; }\n        if (LowMemSize != NULL) { *LowMemSize = g_BootContext.LowMemorySize; }\n        return EFI_ALREADY_STARTED;\n    }\n\n    Pages = PPC_LOW_MEM_SIZE / EFI_PAGE_SIZE;\n    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);\n    if (EFI_ERROR(Status)) {\n        Print(L\"Failed to allocate low-memory pages: %r\\n\", Status);\n        return Status;\n    }\n    ZeroMem((VOID*)(UINTN)Base, PPC_LOW_MEM_SIZE);\n\n    Status = PpcAddGuestMemoryRegion((VOID*)(UINTN)Base,\n                                     PPC_LOW_MEM_GUEST_BASE,\n                                     PPC_LOW_MEM_SIZE,\n                                     FALSE);\n    if (EFI_ERROR(Status)) {\n        BS->FreePages(Base, Pages);\n        Print(L\"Failed to map low memory into guest memory: %r\\n\", Status);\n        return Status;\n    }\n\n    g_BootContext.LowMemoryInstalled = TRUE;\n    g_BootContext.LowMemoryAddress = PPC_LOW_MEM_GUEST_BASE;\n    g_BootContext.LowMemorySize = PPC_LOW_MEM_SIZE;\n\n    if (LowMemAddress != NULL) { *LowMemAddress = PPC_LOW_MEM_GUEST_BASE; }\n    if (LowMemSize != NULL) { *LowMemSize = PPC_LOW_MEM_SIZE; }\n\n    Print(L\"Low-memory region installed: %d bytes at guest 0x%x\\n\",\n          (UINT64)PPC_LOW_MEM_SIZE, PPC_LOW_MEM_GUEST_BASE);\n\n    return EFI_SUCCESS;\n}"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 3.9s)

_Thinking:_

Now update the doc comment in bootloader.h for PpcInstallLowMemory. And the MemoryMap struct comment. Let me update the header doc.

Update the header doc to reflect the carve:

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader.h",
  "newString": "/**\n  Install the classic Mac OS low-memory globals region at guest 0x00000000\n  (16 KB, read/write), carved out of the primary guest RAM region.\n  @param[out] LowMemAddress  Guest address of the region (may be NULL)\n  @param[out] LowMemSize     Region size in bytes (may be NULL)\n  @retval EFI_STATUS\n**/",
  "oldString": "/**\n  Install the classic Mac OS low-memory globals region at guest 0x00000000\n  (16 KB, read/write).\n  @param[out] LowMemAddress  Guest address of the region (may be NULL)\n  @param[out] LowMemSize     Region size in bytes (may be NULL)\n  @retval EFI_STATUS\n**/"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 5.1s)

_Thinking:_

Now let me verify the whole flow compiles logically. Let me re-read the bootloader_impl.c file around the new functions to double-check for issues, particularly:
1. Includes — does bootloader_impl.c include interpreter.h (for g_PpcContext)? It already used g_PpcContext at line 321 (existing PpcResetCpu maybe). Let me check the includes at the top.
2. BootReadFileToPages uses SIZE_OF_EFI_FILE_INFO, GenericFileInfo, AllocatePool, AllocatePages — need efi/efilib included.
3. PpcFreeMemory(Buffer, Size) in PpcInstallSystemRom error path — takes UINT64 size? Let me check signature: manager.h line 68 PpcFreeMemory. Let me check its params.

Let me read the top of bootloader_impl.c includes and the PpcFreeMemory signature.

Now let me verify includes and signatures — check the top of bootloader_impl.c and `PpcFreeMemory`:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "limit": 70
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c</path>
<type>file</type>
<content>
1: #include "bootloader.h"
2: #include <efi.h>
3: #include <efilib.h>
4: #include "cpu/interpreter.h"
5: #include "cpu/translation.h"
6: #include "memory/manager.h"
7: #include "hardware/abstraction.h"
8: #include "platform/uefi_interface.h"
9: 
10: // Bootloader context structure with more complete implementation
11: typedef struct {
12:     BOOLEAN IsInitialized;
13:     CHAR16* BootImagePath;
14:     EFI_PHYSICAL_ADDRESS KernelAddress;
15:     UINT64 KernelSize;
16:     BOOLEAN KernelLoaded;
17:     BOOLEAN SystemBooting;
18:     BOOLEAN SystemReady;
19:     PPC_BOOT_PARAMETERS BootParams;
20:     EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
21:     // Phase 5: classic Mac OS guest memory map state
22:     BOOLEAN LowMemoryInstalled;
23:     UINT64  LowMemoryAddress;
24:     UINT64  LowMemorySize;
25:     BOOLEAN RomLoaded;
26:     UINT64  RomAddress;
27:     UINT64  RomSize;
28:     VOID*   RomHostBuffer;
29: } PPC_BOOTLOADER_CONTEXT;
30: 
31: // Global bootloader context
32: STATIC PPC_BOOTLOADER_CONTEXT g_BootContext = {0};
33: 
34: // ---------------------------------------------------------------------------
35: // File load helper: read a whole file from the boot volume into a page-aligned
36: // buffer (UEFI pool allocations are limited to ~128 KB; Mac OS ROM images are
37: // several MB, so ROM/ROM-like blobs are page-backed).
38: // ---------------------------------------------------------------------------
39: STATIC
40: EFI_STATUS
41: BootReadFileToPages (
42:     IN  CHAR16* FilePath,
43:     OUT VOID**  Buffer,
44:     OUT UINTN*  Size
45:     )
46: {
47:     EFI_FILE_IO_INTERFACE* Fs = NULL;
48:     EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
49:     if (EFI_ERROR(Status)) {
50:         return Status;
51:     }
52: 
53:     EFI_FILE_HANDLE Root = NULL;
54:     Status = Fs->OpenVolume(Fs, &Root);
55:     if (EFI_ERROR(Status)) {
56:         return Status;
57:     }
58: 
59:     EFI_FILE_HANDLE File = NULL;
60:     Status = Root->Open(Root, &File, FilePath, EFI_FILE_MODE_READ, 0);
61:     if (EFI_ERROR(Status) || File == NULL) {
62:         Root->Close(Root);
63:         return (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
64:     }
65: 
66:     UINTN FileInfoSize = SIZE_OF_EFI_FILE_INFO + 260 * sizeof(CHAR16);
67:     EFI_FILE_INFO* Info = AllocateZeroPool(FileInfoSize);
68:     if (Info == NULL) {
69:         File->Close(File);
70:         Root->Close(Root);

(Showing lines 1-70 of 830. Use offset=71 to continue.)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\memory\\manager.h",
  "limit": 30,
  "offset": 55
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory\manager.h</path>
<type>file</type>
<content>
55:     IN  UINT64   Size,
56:     OUT VOID**   VirtualAddress,
57:     OUT UINT64*  PhysicalAddress
58:     );
59: 
60: /**
61:   Free allocated memory
62:   @param[in] VirtualAddress   Virtual address to free
63:   @param[in] Size             Size of memory to free
64:   @retval EFI_STATUS
65: **/
66: EFI_STATUS
67: EFIAPI
68: PpcFreeMemory (
69:     IN VOID* VirtualAddress,
70:     IN UINT64 Size
71:     );
72: 
73: /**
74:   Map physical memory to virtual address
75:   @param[in]  PhysicalAddress   Physical address to map
76:   @param[in]  Size              Size of memory to map
77:   @param[out] VirtualAddress    Pointer to store mapped virtual address
78:   @retval EFI_STATUS
79: **/
80: EFI_STATUS
81: EFIAPI
82: PpcMapMemory (
83:     IN  EFI_PHYSICAL_ADDRESS PhysicalAddress,
84:     IN  UINT64               Size,

(Showing lines 55-84 of 157. Use offset=85 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 17.9s)

_Thinking:_

Good. Includes are fine (interpreter.h for g_PpcContext, translation.h for PpcReadGuestByte/PpcWriteGuestByte/PpcExecuteBlock/PpcGetGprValue/PpcSetGprValue, manager.h for PpcFreeMemory, uefi_interface.h for PpcGetFileSystem). PpcFreeMemory takes (VOID*, UINT64). My call: `PpcFreeMemory(Buffer, Size)` where Size is UINT64 — fine.

Now check the existing code at line 258 and 637 uses PpcGetGuestMemoryRegion with 3 args — my calls use it consistently. Good.

Now one issue: `BootReadFileToPages` — I used `AllocateZeroPool` and `FreePool` from efilib. Also `Print` with `%r` for EFI_STATUS — GNU-EFI Print supports %r. Also `(UINT64)FileSize` prints with %d — GNU-EFI's Print doesn't really support %d robustly for 64-bit, but existing code uses %d with UINT64 (line: `Print(L"... %d bytes at 0x%x\n", (UINT64)Size, Buffer)` in old code and elsewhere). GNU-EFI Print supports %d, %x, %r, %s, %N. For 64-bit values it might print wrong, but existing code does this everywhere, so consistent with codebase conventions. I'll match existing style. Actually my prints use `%d` for UINT64 in several places — the codebase already does `Print(L"...%d bytes...", (UINT64)Size, ...)` so consistent.

Also in main.c my Phase 5 print uses `%d` with UINT64 values cast — e.g., `(UINT64)LowMemSize` with %d. Consistent with existing main.c style (line 117-ish and the existing demo prints). Let me double-check main.c existing prints to match. Actually existing main.c line ~340 used Print with %x for guest addr. Let me just check my inserted code compiles — it's fine.

Now `PPC_ROM_DEFAULT_PATH` is `L"\\System\\MacOS\\ROM"` — kernel is at `\System\MacOS\kernel`, so path structure is consistent.

Next: there's a potential concern in BootReadFileToPages — `Root->Open` returns file; if FileSize exceeds limit, we Print and close. Fine.

Now check: in PpcRunBootSelfTest, I use PpcExecuteBlock at PPC_RESET_VECTOR (0xFFF00100). But note PpcExecuteBlock sets g_PpcContext.Pc = (UINT32)(UINTN)InstructionStream. Passing guest address 0xFFF00100 as the pointer value — InstructionStream param is `IN UINT32*`; we pass `(UINT32*)(UINTN)PPC_RESET_VECTOR`. Then Pc = 0xFFF00100. CpuRead32(0xFFF00100) → g_ReadByte(0xFFF00100) → multi-region → ROM region (guest 0xFFF00000 size 4MB) → host buffer + 0x100. That's where we wrote the 4-instruction program. 

But WAIT — PpcExecuteBlock executes only the 4 instructions then stops (MaxInstructions=4). After the 4 instructions, the loop ends with Executed=4, returns EFI_SUCCESS. Good. The program doesn't branch, so no risk of running past. Also ExceptionPending stays 0. 

Now check the demo program instructions against the interpreter: 
- `lis r3, 0xFFF0` → 0x3C60FFF0: XO opcode. g_PpcContext.Gpr[3] = 0xFFF00000. Interpreter supports lis (addis rA,0,SI). Good.
- `lwz r4, 0(r3)` → 0x80830000: d-form load. Gpr[4] = CpuRead32(0xFFF00000 + 0) = big-endian bytes at ROM: 'R''O''M''1' = 0x524F4D31. Good.
- `addi r5, r4, 1` → 0x38A40001: Gpr[5] = 0x524F4D32. Good.
- `stw r5, 0(r1)` → 0x90A10000: CpuWrite32(0, Gpr[5]) where r1 = RamBase = 0. Writes to guest 0. RAM region matches (first-match). Good.

Self-test checks:
- PpcGetGprValue(4) == 0x524F4D31 ✓
- Readback of (RamBase+0..3) == 0x524F4D32 big-endian ✓ (via PpcReadGuestByte at guest 0)

Now PpcPrepareSystemForBoot: ZeroMem(&g_PpcContext, ...) resets the CPU. This happens AFTER the self-test runs, so no conflict. Good. But wait — ZeroMem g_PpcContext then sets Gpr[0..31]=0, Msr, Pc, Srr0, Srr1, ExceptionPending. The PPC_CPU_CONTEXT struct has more fields (Ctr, Lr, Xers, etc.) — ZeroMem handles them. Good.

Now — one concern about PpcPrepareSystemForBoot and main.c ordering: main.c runs self-test (which mutates g_PpcContext GPRs) then PpcPrepareSystemForBoot resets it. Good order.

Now let me verify the whole bootloader_impl.c compiles logically — let me re-read the new sections in context, particularly around BootReadFileToPages and the functions, checking for:
- unused variables (g_BootTestPasses etc. are used)
- BootSelfTestCheck declared before use (yes, before PpcRunBootSelfTest)
- BootWriteWord32 used in PpcPrepareSystemForBoot (declared before)

Also check PpcRunBootSelfTest references: `(UINT8)(MagicPlusOne >> 24)` etc. MagicPlusOne is UINT32 = 0x524F4D32. Good.

Also check that PpcGetGprValue/PpcSetGprValue take UINT8 reg and return UINT32. Yes per translation.h.

Now, let me check the existing code in bootloader_impl.c that I might have broken: PpcLoadSystemRom was used elsewhere? Line 258 PpcGetGuestMemoryRegion — that's in some other function (maybe PpcBootSystem or kernel load). Since I changed PpcLoadSystemRom signature? No — I kept the same signature (CHAR16*, VOID**, UINT64*). Good. But the old implementation used PpcLoadFile; my new one uses BootReadFileToPages. Any other caller of PpcLoadSystemRom? Only PpcInstallSystemRom now. Good.

Wait — is there a PpcLoadFile function that's now unused? Not my concern.

Now, check: does anything else in bootloader_impl.c use g_BootContext.RomHostBuffer? Not yet — it's stored but unused so far (could be used later to validate or free). An unused struct field is fine (no warning for unused struct members).

Let me now verify the full new bootloader_impl.c section reads correctly. Let me read lines 240-520 to review PpcInstallSystemRom and demo ROM, and 700-830 for self-test/prepare.

Includes and `PpcFreeMemory(VOID*, UINT64)` signature are compatible. Let me review the new implementation sections in context to catch any issues:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "limit": 120,
  "offset": 220
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c</path>
<type>file</type>
<content>
220:     Status = Root->Open(Root, &KernelFile, ImagePath, EFI_FILE_MODE_READ, 0);
221:     if (EFI_ERROR(Status) || KernelFile == NULL) {
222:         Print(L"Kernel image '%s' not found: %r\n", ImagePath, Status);
223:         Root->Close(Root);
224:         return (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
225:     }
226: 
227:     // Get the kernel file size.
228:     UINTN FileInfoSize = SIZE_OF_EFI_FILE_INFO + 260 * sizeof(CHAR16);
229:     EFI_FILE_INFO* FileInfo = AllocateZeroPool(FileInfoSize);
230:     if (FileInfo == NULL) {
231:         KernelFile->Close(KernelFile);
232:         Root->Close(Root);
233:         return EFI_OUT_OF_RESOURCES;
234:     }
235:     Status = KernelFile->GetInfo(KernelFile, &GenericFileInfo, &FileInfoSize, FileInfo);
236:     if (EFI_ERROR(Status)) {
237:         Print(L"Failed to get kernel file info: %r\n", Status);
238:         FreePool(FileInfo);
239:         KernelFile->Close(KernelFile);
240:         Root->Close(Root);
241:         return Status;
242:     }
243: 
244:     UINT64 FileSize = FileInfo->FileSize;
245:     FreePool(FileInfo);
246: 
247:     if (FileSize == 0 || FileSize > 0x10000000) {
248:         Print(L"Kernel image has invalid size %d\n", FileSize);
249:         KernelFile->Close(KernelFile);
250:         Root->Close(Root);
251:         return EFI_LOAD_ERROR;
252:     }
253: 
254:     // Destination: the UEFI-allocated guest RAM region (guest base 0x10000000).
255:     VOID*  GuestBuffer = NULL;
256:     UINT64 GuestBase   = 0;
257:     UINT64 GuestSize   = 0;
258:     Status = PpcGetGuestMemoryRegion(&GuestBuffer, &GuestBase, &GuestSize);
259:     if (EFI_ERROR(Status) || GuestBuffer == NULL || FileSize > GuestSize) {
260:         Print(L"Guest RAM unavailable for kernel load\n");
261:         KernelFile->Close(KernelFile);
262:         Root->Close(Root);
263:         return EFI_NOT_READY;
264:     }
265: 
266:     // Read the whole file into guest RAM (Read may return partial data).
267:     UINTN   BytesRead = 0;
268:     UINT64  Remaining = FileSize;
269:     BOOLEAN Failed    = FALSE;
270:     while (Remaining > 0) {
271:         UINTN Chunk = (UINTN)Remaining;
272:         Status = KernelFile->Read(KernelFile, &Chunk, (UINT8*)GuestBuffer + BytesRead);
273:         if (EFI_ERROR(Status) || Chunk == 0) {
274:             Print(L"Failed while reading kernel: %r\n", Status);
275:             Failed = TRUE;
276:             break;
277:         }
278:         BytesRead += Chunk;
279:         Remaining  -= Chunk;
280:     }
281: 
282:     KernelFile->Close(KernelFile);
283:     Root->Close(Root);
284: 
285:     if (Failed) {
286:         return EFI_LOAD_ERROR;
287:     }
288: 
289:     *KernelAddress = (EFI_PHYSICAL_ADDRESS)GuestBase;
290:     *KernelSize = FileSize;
291: 
292:     g_BootContext.KernelAddress = *KernelAddress;
293:     g_BootContext.KernelSize = *KernelSize;
294:     g_BootContext.KernelLoaded = TRUE;
295: 
296:     Print(L"Kernel loaded: %d bytes into guest RAM at 0x%x\n",
297:           FileSize, (UINT32)GuestBase);
298: 
299:     return EFI_SUCCESS;
300: }
301: 
302: EFI_STATUS
303: PpcBootSystem (
304:     IN EFI_PHYSICAL_ADDRESS KernelAddress,
305:     IN UINT64               KernelSize
306:     )
307: {
308:     if (!g_BootContext.KernelLoaded) {
309:         Print(L"Error: No kernel loaded for boot\n");
310:         return EFI_NOT_READY;
311:     }
312:     if (KernelAddress == 0 || KernelSize == 0) {
313:         return EFI_INVALID_PARAMETER;
314:     }
315: 
316:     // Configure the real CPU context for transfer of control to the kernel:
317:     // PC = kernel entry, MSR enables machine-check handling, SRR0/SRR1 seeded.
318:     g_PpcContext.Pc = (UINT32)KernelAddress;
319:     g_PpcContext.Srr0 = (UINT32)KernelAddress;
320:     g_PpcContext.Srr1 = g_PpcContext.Msr;
321:     g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;
322:     g_PpcContext.ExceptionPending = 0;
323: 
324:     Print(L"Booting system from kernel at 0x%x (size: %d bytes)\n", KernelAddress, KernelSize);
325:     Print(L"PowerPC core configured: PC=0x%x MSR=0x%08x\n",
326:           g_PpcContext.Pc, g_PpcContext.Msr);
327: 
328:     g_BootContext.SystemBooting = TRUE;
329: 
330:     return EFI_SUCCESS;
331: }
332: 
333: EFI_STATUS
334: PpcLoadBootImage (
335:     IN  CHAR16* ImagePath,
336:     OUT VOID**  ImageBuffer,
337:     OUT UINT64* ImageSize
338:     )
339: {

(Showing lines 220-339 of 830. Use offset=340 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 14.5s)

_Thinking:_

I see — the kernel load uses the same file-reading pattern. Good, my BootReadFileToPages mirrors existing code conventions. Note: guest RAM base is 0x10000000 per the comment at line 254 — interesting! The kernel loads at guest base 0x10000000. So the guest RAM region's GuestBase is 0x10000000, not 0. Wait — that changes things!

PpcGetGuestMemoryRegion returns GuestBase=0x10000000? The comment says "the UEFI-allocated guest RAM region (guest base 0x10000000)". Hmm, so RAM is mapped at guest 0x10000000..0x20000000 (256MB).

But the memory manager's PpcSetGuestMemory — what guest base does it use? Let me check manager_impl.c line ~287 PpcGetGuestMemoryRegion and the allocation. If RAM guest base is 0x10000000, then:
- Low memory carve: PPC_LOW_MEM_GUEST_BASE (0) is NOT inside RAM [0x10000000, 0x20000000). My carve check `(0 - 0x10000000) + 0x4000 > RamSize` → 0 - 0x10000000 underflows UINT64 → huge → > RamSize → returns EFI_NOT_READY! BUG.

Wait, but this conflicts with what I thought earlier. Let me re-check. Actually earlier in main.c, the demo program writes at ProgramBase computed from GuestBase. Let me look at main.c lines 100-200 to see actual guest base values used.

Also the summary said "256MB guest RAM via AllocatePages" and "guest framebuffer at 0x18000000 carved out of guest RAM". 0x18000000 = 384MB. If RAM is at guest 0x10000000 (256MB), then 0x18000000 - 0x10000000 = 0x8000000 (128MB offset) + FbSize <= 256MB ✓. That's consistent! So RAM is indeed at guest base 0x10000000.

And audio at 0x18800000 - 0x10000000 = 0x8800000 (136MB) + size <= 256MB ✓.

So the guest RAM region is [0x10000000, 0x20000000). Guest 0x0 is NOT covered by RAM.

This changes my analysis:
1. Low memory at guest 0x0 carve from RAM: GUEST BASE MISMATCH. Low memory is at guest 0, RAM starts at guest 0x10000000. They DON'T overlap! So there's NO overlap bug — PpcAddGuestMemoryRegion wouldn't reject low memory at guest 0.

Wait, but then why did I think low memory overlapped RAM? Because I assumed RAM base 0. The actual RAM base is 0x10000000. So:
- RAM: guest [0x10000000, 0x20000000)
- Low memory: guest [0, 0x4000) — separate, no overlap with RAM. A separate pool buffer is legitimate!
- ROM: guest [0xFFF00000, 0xFFF00000+size) — no overlap with RAM either.

But the demo ROM program does `stw r5, 0(r1)` with r1 = RamBase. If I set r1 = RamBase = 0x10000000, the store goes to RAM. Good. But in my current self-test I set `PpcSetGprValue(1, (UINT32)RamBase)` — RamBase from PpcGetGuestMemoryRegion = 0x10000000. Store to guest 0x10000000 → RAM region. ✓. And readback at RamBase+0..3 → RAM. ✓.

Also low memory at guest 0 is unmapped by RAM but IS a separate region → writes to guest 0x8 go to low-memory pool. ✓.

So actually the original design (separate low-memory pool at guest 0, no carve) was FINE and correct! I over-corrected by "fixing" the carve. But wait — was there actually a problem? Let me reconsider the overlap rejection: low memory guest [0, 0x4000), RAM guest [0x10000000, 0x20000000). No overlap → PpcAddGuestMemoryRegion accepts. So the original implementation would have worked!

Hmm, but I changed it to carve. The carve now has the underflow bug because PPC_LOW_MEM_GUEST_BASE (0) is below RAM base (0x10000000). `(UINT64)(0 - 0x10000000)` → huge value → + 0x4000 > RamSize → EFI_NOT_READY. So my carve "fix" actually BROKE it.

I need to fix PpcInstallLowMemory. Options:
A) Revert to separate pool (original). Clean, works, no overlap.
B) Carve with proper bounds handling.

Given guest 0 is not in RAM, the separate-pool approach is the natural one and matches "install a region". Let me revert PpcInstallLowMemory to the pool + PpcAddGuestMemoryRegion version. But — wait, then why did I even worry about the overlap? I misread. Let me restore the original implementation.

But hold on — let me double check the RAM guest base claim by reading main.c and manager_impl.c. The comment in bootloader_impl.c line 254 says "guest base 0x10000000". Also kernel load: kernel loaded at GuestBase 0x10000000, and main.c line 495 executes at EntryPoint = KernelAddress = 0x10000000. And earlier main.c demo (line 137) PpcGetGuestMemoryRegion gives DemoGuestBase, then ProgramBase = DemoGuestBase + offset, ResultAddr = DemoGuestBase + ... So the demo uses RAM base as guest base. 

Let me confirm manager_impl.c PpcGetGuestMemoryRegion guest base. Let me read manager_impl.c around line 287 and the memory map setup.

Actually, this is important enough to verify directly.

Critical detail spotted: the kernel comment (line 254) says guest RAM is at guest base **0x10000000**, not 0. Let me confirm the actual RAM guest base in the memory manager and main.c:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\memory\\manager_impl.c",
  "limit": 80,
  "offset": 120
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\memory\manager_impl.c</path>
<type>file</type>
<content>
120:     OUT VOID**   VirtualAddress,
121:     OUT UINT64*  PhysicalAddress
122:     )
123: {
124:     if (VirtualAddress == NULL || PhysicalAddress == NULL) {
125:         return EFI_INVALID_PARAMETER;
126:     }
127:     
128:     Print(L"Allocating %d bytes of memory\n", Size);
129:     
130:     // In a real implementation, we would:
131:     // 1. Find suitable memory region in our managed space
132:     // 2. Allocate virtual address space 
133:     // 3. Map to physical memory
134:     // 4. Return allocated addresses
135:     
136:     EFI_STATUS Status = BS->AllocatePages(
137:         AllocateAnyPages,
138:         EfiBootServicesData,
139:         (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE,
140:         (EFI_PHYSICAL_ADDRESS*)PhysicalAddress
141:     );
142:     
143:     if (EFI_ERROR(Status)) {
144:         Print(L"Failed to allocate physical pages: %r\n", Status);
145:         return Status;
146:     }
147:     
148:     *VirtualAddress = (VOID*)(UINTN)(*PhysicalAddress);
149:     
150:     Print(L"Allocated memory - Virtual: 0x%x, Physical: 0x%x\n", *VirtualAddress, *PhysicalAddress);
151:     
152:     return EFI_SUCCESS;
153: }
154: 
155: EFI_STATUS
156: PpcFreeMemory (
157:     IN VOID* VirtualAddress,
158:     IN UINT64 Size
159:     )
160: {
161:     if (VirtualAddress == NULL) {
162:         return EFI_INVALID_PARAMETER;
163:     }
164:     
165:     Print(L"Freeing memory at virtual address 0x%x (size: %d bytes)\n", VirtualAddress, Size);
166:     
167:     // In a real implementation:
168:     // 1. Calculate page-aligned physical address
169:     // 2. Free the allocated pages
170:     // 3. Update memory management structures
171:     
172:     EFI_PHYSICAL_ADDRESS PhysicalAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)VirtualAddress;
173:     
174:     UINTN Pages = (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
175:     
176:     EFI_STATUS Status = BS->FreePages(PhysicalAddress, Pages);
177:     
178:     if (EFI_ERROR(Status)) {
179:         Print(L"Failed to free pages: %r\n", Status);
180:         return Status;
181:     }
182:     
183:     Print(L"Freed %d pages from 0x%x\n", Pages, PhysicalAddress);
184:     
185:     return EFI_SUCCESS;
186: }
187: 
188: EFI_STATUS
189: PpcMapMemory (
190:     IN  EFI_PHYSICAL_ADDRESS PhysicalAddress,
191:     IN  UINT64               Size,
192:     OUT VOID**               VirtualAddress
193:     )
194: {
195:     if (VirtualAddress == NULL) {
196:         return EFI_INVALID_PARAMETER;
197:     }
198: 
199:     // For guest RAM the mapping is a plain offset into the allocated region.

(Showing lines 120-199 of 411. Use offset=200 to continue.)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\main.c",
  "limit": 110,
  "offset": 100
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\main.c</path>
<type>file</type>
<content>
100:     Print(L"PowerPC CPU self-test FAILED: %r\n", Status);
101:     return Status;
102:   }
103:   
104:   // Initialize memory manager (guest RAM at the classic Mac OS kernel base)
105:   Status = PpcInitializeMemoryManager(0x10000000, 0x10000000);  // 256MB
106:   if (EFI_ERROR(Status)) {
107:     Print(L"Failed to initialize memory manager: %r\n", Status);
108:     return Status;
109:   }
110:   
111:   // Wire the UEFI-allocated guest RAM into the interpreter so that guest
112:   // loads/stores and instruction fetches target the emulated memory region.
113:   {
114:     VOID*  HostBase  = NULL;
115:     UINT64 GuestBase = 0;
116:     UINT64 GuestSize = 0;
117:     Status = PpcGetGuestMemoryRegion(&HostBase, &GuestBase, &GuestSize);
118:     if (EFI_ERROR(Status)) {
119:       Print(L"Failed to get guest memory region: %r\n", Status);
120:       return Status;
121:     }
122:     Status = PpcSetGuestMemory(HostBase, (UINT32)GuestBase, (UINT32)GuestSize);
123:     if (EFI_ERROR(Status)) {
124:       Print(L"Failed to wire guest memory: %r\n", Status);
125:       return Status;
126:     }
127:     Print(L"Guest memory wired: host 0x%x <-> guest 0x%x (%d bytes)\n",
128:           HostBase, GuestBase, GuestSize);
129:   }
130:   
131:   // Execute a small PowerPC program that lives in guest RAM. This proves the
132:   // full pipeline: guest memory allocation -> fetch -> decode -> execute.
133:   {
134:     VOID*  DemoHostBase  = NULL;
135:     UINT64 DemoGuestBase = 0;
136:     UINT64 DemoGuestSize = 0;
137:     Status = PpcGetGuestMemoryRegion(&DemoHostBase, &DemoGuestBase, &DemoGuestSize);
138:     if (EFI_ERROR(Status)) {
139:       Print(L"Demo: guest memory region unavailable: %r\n", Status);
140:       return Status;
141:     }
142: 
143:     // addi r3,r0,40 ; addi r4,r0,2 ; mullw r5,r3,r4 ; stw r5,0(r1)
144:     static const UINT32 GuestProgram[] = {
145:       0x38600028,   // addi r3, r0, 40
146:       0x38800002,   // addi r4, r0, 2
147:       0x7CA321D6,   // mullw r5, r3, r4
148:       0x90A10000,   // stw  r5, 0(r1)
149:     };
150:     const UINT32 ProgramBase = (UINT32)DemoGuestBase;
151:     const UINT32 ResultAddr  = ProgramBase + 0x40;  // store result here
152:     const UINTN  ProgramCount = sizeof(GuestProgram) / sizeof(GuestProgram[0]);
153: 
154:     // Write the program into guest RAM (big-endian byte order)
155:     for (UINTN i = 0; i < ProgramCount; i++) {
156:       UINT32 w = GuestProgram[i];
157:       for (UINTN b = 0; b < 4; b++) {
158:         PpcWriteGuestByte(ProgramBase + (UINT32)i * 4 + (UINT32)b,
159:                           (UINT8)(w >> (24 - (UINTN)b * 8)));
160:       }
161:     }
162: 
163:     Print(L"\n--- Executing PowerPC program from guest RAM ---\n");
164: 
165:     PpcSetGprValue(1, ResultAddr);   // r1 points at the result slot
166:     PpcSetGprValue(3, 0);
167:     PpcSetGprValue(4, 0);
168:     PpcSetGprValue(5, 0);
169: 
170:     // Fetch/decode/execute loop over the guest-memory resident program
171:     UINTN  Executed = 0;
172:     EFI_STATUS ExecStatus = PpcExecuteBlock(
173:         (UINT32*)(UINTN)ProgramBase,
174:         ProgramCount,
175:         &Executed
176:     );
177:     BOOLEAN RanClean = (ExecStatus == EFI_SUCCESS);
178: 
179:     // Read the stored word back out of guest RAM (big-endian)
180:     UINT32 Result = 0;
181:     for (UINTN b = 0; b < 4; b++) {
182:       Result = (Result << 8) | PpcReadGuestByte(ResultAddr + (UINT32)b);
183:     }
184:     UINT32 R5 = PpcGetGprValue(5);
185: 
186:     Print(L"Program executed: %d instructions (status %r)\n", Executed, ExecStatus);
187:     Print(L"r5 = %d (expected 80)\n", R5);
188:     Print(L"Guest RAM[0x%x] = %d (expected 80)\n", ResultAddr, Result);
189: 
190:     if (RanClean && Executed == ProgramCount && R5 == 80 && Result == 80) {
191:       Print(L"Guest RAM execution: PASS\n");
192:     } else {
193:       Print(L"Guest RAM execution: FAIL\n");
194:     }
195:   }
196:   
197:   // Initialize hardware abstraction layer
198:   Status = PpcInitializeHardwareAbstraction();
199:   if (EFI_ERROR(Status)) {
200:     Print(L"Failed to initialize hardware abstraction: %r\n", Status);
201:     return Status;
202:   }
203:   
204:   // Initialize bootloader
205:   Status = PpcInitializeBootloader();
206:   if (EFI_ERROR(Status)) {
207:     Print(L"Failed to initialize bootloader: %r\n", Status);
208:     return Status;
209:   }

(Showing lines 100-209 of 576. Use offset=210 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 12.9s)

_Thinking:_

Confirmed: guest RAM is wired at guest base 0x10000000, size 0x10000000 (256MB), spanning guest [0x10000000, 0x20000000).

So:
- Low memory at guest [0, 0x4000) — NO overlap with RAM.
- ROM at guest [0xFFF00000, +size) — NO overlap.
- My original separate-pool low-memory implementation was correct and PpcAddGuestMemoryRegion would accept it.
- My carve "fix" is now WRONG: `PPC_LOW_MEM_GUEST_BASE - RamBase` = 0 - 0x10000000 → underflows → EFI_NOT_READY.

I must revert PpcInstallLowMemory to the separate-pool version.

Also re-verify the demo ROM program: `PpcSetGprValue(1, (UINT32)RamBase)` where RamBase = 0x10000000. `stw r5, 0(r1)` → store to guest 0x10000000 → RAM region. Readback at RamBase+0 → RAM. ✓ Good — my self-test uses RamBase correctly.

Wait — but the very first demo in main.c (line 165) sets r1 = ResultAddr = ProgramBase + 0x40 = 0x10000040, and stw to 0(r1) → guest 0x10000040 → RAM. Consistent.

Now, also reconsider: in PpcPrepareSystemForBoot, BootWriteWord32 writes to PPC_LOW_MEM_GUEST_BASE + offsets = guest 0x0..0x130. That goes to the low-memory region (pool). ✓ With the separate-pool version, correct.

Also low-memory self-test: PpcWriteGuestByte(0x8, 0xAA), readback. → low-memory pool. ✓.

So revert the carve. Let me also revert the header doc comment tweak (it says "carved out of the primary guest RAM region" — wrong now). Restore to "installed as a dedicated region" wording.

Let me make these edits.

Confirmed: guest RAM is wired at guest base **0x10000000** (256MB span `[0x10000000, 0x20000000)`). So low memory at guest `0x0` and ROM at `0xFFF00000` do **not** overlap RAM — my original separate-pool version was correct, and the carve I just made has an underflow bug (`0 - 0x10000000`). Reverting the carve:

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "newString": "EFI_STATUS\nPpcInstallLowMemory (\n    OUT UINT64* LowMemAddress,\n    OUT UINT64* LowMemSize\n    )\n{\n    // Classic Mac OS low-memory globals live at physical address 0. Guest RAM\n    // is mapped at guest 0x10000000, so low memory is a dedicated read/write\n    // region below the kernel base.\n    UINTN Pages;\n    EFI_PHYSICAL_ADDRESS Base = 0;\n    EFI_STATUS Status;\n\n    if (g_BootContext.LowMemoryInstalled) {\n        if (LowMemAddress != NULL) { *LowMemAddress = g_BootContext.LowMemoryAddress; }\n        if (LowMemSize != NULL) { *LowMemSize = g_BootContext.LowMemorySize; }\n        return EFI_ALREADY_STARTED;\n    }\n\n    Pages = PPC_LOW_MEM_SIZE / EFI_PAGE_SIZE;\n    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);\n    if (EFI_ERROR(Status)) {\n        Print(L\"Failed to allocate low-memory pages: %r\\n\", Status);\n        return Status;\n    }\n    ZeroMem((VOID*)(UINTN)Base, PPC_LOW_MEM_SIZE);\n\n    Status = PpcAddGuestMemoryRegion((VOID*)(UINTN)Base,\n                                     PPC_LOW_MEM_GUEST_BASE,\n                                     PPC_LOW_MEM_SIZE,\n                                     FALSE);\n    if (EFI_ERROR(Status)) {\n        BS->FreePages(Base, Pages);\n        Print(L\"Failed to map low memory into guest memory: %r\\n\", Status);\n        return Status;\n    }\n\n    g_BootContext.LowMemoryInstalled = TRUE;\n    g_BootContext.LowMemoryAddress = PPC_LOW_MEM_GUEST_BASE;\n    g_BootContext.LowMemorySize = PPC_LOW_MEM_SIZE;\n\n    if (LowMemAddress != NULL) { *LowMemAddress = PPC_LOW_MEM_GUEST_BASE; }\n    if (LowMemSize != NULL) { *LowMemSize = PPC_LOW_MEM_SIZE; }\n\n    Print(L\"Low-memory region installed: %d bytes at guest 0x%x\\n\",\n          (UINT64)PPC_LOW_MEM_SIZE, PPC_LOW_MEM_GUEST_BASE);\n\n    return EFI_SUCCESS;\n}",
  "oldString": "EFI_STATUS\nPpcInstallLowMemory (\n    OUT UINT64* LowMemAddress,\n    OUT UINT64* LowMemSize\n    )\n{\n    // Classic Mac OS low-memory globals live at physical address 0, inside\n    // guest RAM. Carve the window out of the primary guest RAM region (the\n    // same pattern the graphics/audio carve uses) so the boot info block is\n    // genuinely part of the guest's memory image.\n    VOID*  RamHost   = NULL;\n    UINT64 RamBase   = 0;\n    UINT64 RamSize   = 0;\n    EFI_STATUS Status;\n\n    if (g_BootContext.LowMemoryInstalled) {\n        if (LowMemAddress != NULL) { *LowMemAddress = g_BootContext.LowMemoryAddress; }\n        if (LowMemSize != NULL) { *LowMemSize = g_BootContext.LowMemorySize; }\n        return EFI_ALREADY_STARTED;\n    }\n\n    Status = PpcGetGuestMemoryRegion(&RamHost, &RamBase, &RamSize);\n    if (EFI_ERROR(Status) || RamHost == NULL) {\n        Print(L\"Low-memory region: guest RAM unavailable\\n\");\n        return EFI_NOT_READY;\n    }\n\n    if ((UINT64)(PPC_LOW_MEM_GUEST_BASE - (UINT32)RamBase) + PPC_LOW_MEM_SIZE > RamSize) {\n        Print(L\"Low-memory region: guest RAM too small\\n\");\n        return EFI_NOT_READY;\n    }\n\n    ZeroMem(RamHost, PPC_LOW_MEM_SIZE);\n\n    g_BootContext.LowMemoryInstalled = TRUE;\n    g_BootContext.LowMemoryAddress = PPC_LOW_MEM_GUEST_BASE;\n    g_BootContext.LowMemorySize = PPC_LOW_MEM_SIZE;\n\n    if (LowMemAddress != NULL) { *LowMemAddress = PPC_LOW_MEM_GUEST_BASE; }\n    if (LowMemSize != NULL) { *LowMemSize = PPC_LOW_MEM_SIZE; }\n\n    Print(L\"Low-memory region carved from guest RAM: %d bytes at guest 0x%x\\n\",\n          (UINT64)PPC_LOW_MEM_SIZE, PPC_LOW_MEM_GUEST_BASE);\n\n    return EFI_SUCCESS;\n}"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 4.1s)

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader.h",
  "newString": "/**\n  Install the classic Mac OS low-memory globals region at guest 0x00000000\n  (16 KB, read/write) as a dedicated region below the kernel base.\n  @param[out] LowMemAddress  Guest address of the region (may be NULL)\n  @param[out] LowMemSize     Region size in bytes (may be NULL)\n  @retval EFI_STATUS\n**/",
  "oldString": "/**\n  Install the classic Mac OS low-memory globals region at guest 0x00000000\n  (16 KB, read/write), carved out of the primary guest RAM region.\n  @param[out] LowMemAddress  Guest address of the region (may be NULL)\n  @param[out] LowMemSize     Region size in bytes (may be NULL)\n  @retval EFI_STATUS\n**/"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 21.0s)

_Thinking:_

Now let me double-check the region overlap check in interpreter.c accepts low-memory at guest 0 and ROM at 0xFFF00000 given RAM at [0x10000000, 0x20000000):

RAM slot 0: GuestBase=0x10000000, Size=0x10000000 → covers [0x10000000, 0x20000000)
Low memory: GuestBase=0, Size=0x4000 → [0, 0x4000)
  Overlap check with RAM: GuestBase(0) < RAM.GuestBase(0x10000000) → check 0 + 0x4000 > 0x10000000? No. → no overlap. ✓
ROM: GuestBase=0xFFF00000, Size=4MB (0x400000) → [0xFFF00000, 0x10000000)
  Overlap with RAM: GuestBase(0xFFF00000) >= RAM.GuestBase → check 0xFFF00000 - 0x10000000 = 0xEFF00000 < 0x10000000? Yes → OVERLAP REJECTED!

Wait!! ROM at 0xFFF00000 with size 0x400000 spans [0xFFF00000, 0x10000000). RAM spans [0x10000000, 0x20000000). Do they overlap? ROM end = 0xFFF00000 + 0x400000 = 0x100300000... wait let me compute: 0xFFF00000 + 0x00400000 = 0x100300000. That's > 0x100000000 (32-bit). Hmm, 0xFFF00000 + 0x400000 = 0x100300000 which wraps 32-bit to 0x300000.

The overlap check in PpcAddGuestMemoryRegion:
```
if (GuestBase < g_GuestRegions[I].GuestBase) {
    if ((UINT64)GuestBase + Size > g_GuestRegions[I].GuestBase) return EFI_ALREADY_STARTED;
} else {
    if ((UINT64)(GuestBase - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) return EFI_ALREADY_STARTED;
}
```
For ROM (GuestBase=0xFFF00000) vs RAM (0x10000000, size 0x10000000):
- GuestBase (0xFFF00000) >= RAM.GuestBase (0x10000000) → else branch: (0xFFF00000 - 0x10000000) = 0xEFF00000 < 0x10000000? YES → REJECTED!

So the ROM install at 0xFFF00000 is REJECTED because it overlaps the RAM region in 32-bit address space! 0xFFF00000..0x10000000 is inside RAM [0x10000000, 0x20000000).

This is a real problem. The 32-bit guest address space: RAM occupies [0x10000000, 0x20000000) = [256MB, 512MB). ROM at 0xFFF00000 = ~4GB minus 1MB. 0xFFF00000 = 4293918720 ≈ 4.29GB. That's ABOVE 512MB. Wait: 0xFFF00000 = 0x0FFF00000? No. 0xFFF00000 is 8 hex digits = 32 bits = 4293918720. RAM top = 0x20000000 = 536870912 = 512MB. 0xFFF00000 (4293918720) > 0x20000000 (536870912). They do NOT overlap numerically!

Let me recompute the check: GuestBase(0xFFF00000) - RAM.GuestBase(0x10000000) = 0xEFF00000. Is 0xEFF00000 < RAM.Size (0x10000000)? 0xEFF00000 = 4024434688. 0x10000000 = 268435456. 0xEFF00000 > 0x10000000. So NOT less → NO overlap. 

I miscalculated before. 0xFFF00000 - 0x10000000 = 0xEFF00000 which is much bigger than 0x10000000. Correct. So ROM does NOT overlap RAM. 

And ROM + Size = 0xFFF00000 + 0x400000 = 0x100300000 which as UINT64 doesn't wrap → but the ROM install's own range check uses UINT64 arithmetic; the overlap vs other regions uses `(UINT64)GuestBase + Size > other.GuestBase` etc. Fine.

But wait — the ROM region covers [0xFFF00000, 0xFFF00000+0x400000) = [0xFFF00000, 0x100300000). Addresses up to 0xFFFFFFFF max (32-bit guest). 0x100300000 - 1 = 0x1002FFFFF > 0xFFFFFFFF → part of the region (from 0x100000000 upward) is unreachable in 32-bit addressing. The usable part is [0xFFF00000, 0x100000000) = exactly 1MB. So for a real ROM smaller than 4MB, only the first 1MB is addressable. A real classic Mac ROM is ~4MB at 0xFFF00000, but this emulator's 32-bit addressing... The demo ROM is 4MB but only 1MB reachable. My self-test only touches the first 0x110 bytes (reset vector at +0x100). So it works, but I should note the region is effectively capped at 1MB reachable. That's fine for now — the demo ROM uses bytes up to offset 0x110. The rest is unreachable but harmless (allocated, unmapped by guest). Actually it's fine.

Now the low-memory check: GuestBase=0 < RAM.GuestBase=0x10000000 → first branch: (UINT64)0 + 0x4000 > 0x10000000? No. No overlap. ✓

So PpcAddGuestMemoryRegion accepts both low memory and ROM. 

Now, wait — there's another subtlety. The demo ROM program is installed at guest 0xFFF00000 with 4MB size but I wrote the program at reset vector offset 0x100. PpcExecuteBlock reads guest 0xFFF00100 via region lookup: Address 0xFFF00100 >= GuestBase 0xFFF00000 and 0xFFF00100 - 0xFFF00000 = 0x100 < 0x400000 ✓ → ROM region. 

Now let me verify the rest of my self-test logic once more, and also double check the interpreter's region functions are actually the "final" version. Everything checks out.

Now, one more concern: `PpcExecuteBlock` sets `g_PpcContext.Pc = (UINT32)(UINTN)InstructionStream;` — with InstructionStream = (UINT32*)(UINTN)0xFFF00100. (UINTN)0xFFF00100 is fine on 64-bit (it's a guest address cast to pointer; we never dereference it, only use its numeric value). Pc = 0xFFF00100. ✓

Now let me also verify that `PpcDefaultReadByte`'s first-match-wins handles ROM correctly — RAM is slot 0 (guest 0x10000000), low mem slot 1 (guest 0), ROM slot 2 (guest 0xFFF00000). No overlaps among these, so first-match is unambiguous. ✓

Next: I should re-examine `PpcRunBootSelfTest`'s use of RamBase for the demo ROM store. `PpcSetGprValue(1, (UINT32)RamBase)` → r1 = 0x10000000. `stw r5, 0(r1)` → store to guest 0x10000000 → RAM slot 0. Readback PpcReadGuestByte(0x10000000..3). ✓

Now the boot info block in PpcPrepareSystemForBoot: writes to guest 0x0..0x134 → low-memory region. ✓

Everything is consistent now. Let me also verify the whole main.c Phase 5 section is properly placed (before "=== Ready ===") and doesn't conflict with kernel boot section. Let me read the region around my insertion in main.c to double check placement and that PPC_BOOT_INFO is declared before use (it's in bootloader.h, included). Also verify PpcRunBootSelfTest/PpcInstallDemoRom/PpcInstallLowMemory/PpcInstallSystemRom/PpcPrepareSystemForBoot declarations exist in bootloader.h (added). Yes.

Let me now read main.c around my Phase 5 insertion to verify context.

Both new regions pass the overlap check (RAM `[0x10000000,0x20000000)`, low mem `[0,0x4000)`, ROM `[0xFFF00000,...)`) — no conflicts. Now verify the main.c insertion context and the rest of the boot section:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\main.c",
  "limit": 130,
  "offset": 460
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\main.c</path>
<type>file</type>
<content>
460:   // Write a boot log entry to the ESP — real UEFI file I/O to the FAT drive.
461:   PpcDebugLogToFile(L"EFI-Mac-Emulator: boot self-test passed, initializing hardware\n");
462: 
463:   // Display emulator status
464:   Print(L"\nEFI-Mac-Emulator initialized successfully.\n");
465:   Print(L"Ready to load and boot classic Mac OS.\n");
466:   
467:   // Real kernel load from the FAT volume into guest RAM, then verify and
468:   // execute it — a full disk → RAM → execute pipeline.
469:   EFI_PHYSICAL_ADDRESS KernelAddress;
470:   UINT64 KernelSize;
471:   
472:   Print(L"\n--- Loading kernel from volume ---\n");
473:   Status = PpcLoadKernel(L"\\System\\MacOS\\kernel", &KernelAddress, &KernelSize);
474:   if (!EFI_ERROR(Status)) {
475:     Print(L"Kernel loaded at 0x%x (%d bytes)\n", KernelAddress, KernelSize);
476: 
477:     // Verify kernel integrity (bounds check + first word read).
478:     PpcVerifyKernel(KernelAddress, KernelSize);
479: 
480:     // Set boot parameters (real storage into the bootloader context).
481:     PPC_BOOT_PARAMETERS Params;
482:     Params.BootMode = PPC_BOOT_MODE_NORMAL;
483:     Params.MemorySizeMB = 256;
484:     Params.VideoMode = PPC_GRAPHICS_MODE_1024x768;
485:     Params.EnableDebug = TRUE;
486:     Params.CommandLine = L"console=serial";
487:     PpcSetBootParameters(&Params);
488:     Print(L"Boot parameters set successfully\n");
489: 
490:     // Execute the first 4 words of the loaded kernel image.
491:     Print(L"\n--- Executing loaded kernel image ---\n");
492:     UINT32 EntryPoint = (UINT32)KernelAddress;
493:     const UINTN KernelExecCount = 4;
494:     UINTN KernelExecuted = 0;
495:     Status = PpcExecuteBlock((UINT32*)(UINTN)EntryPoint, KernelExecCount, &KernelExecuted);
496:     BOOLEAN KernelRanClean = (Status == EFI_SUCCESS);
497: 
498:     UINT32 K_R3 = PpcGetGprValue(3);
499:     UINT32 K_R5 = PpcGetGprValue(5);
500: 
501:     Print(L"Loaded kernel executed: %d instructions (status %r)\n", KernelExecuted, Status);
502:     Print(L"r3=%d, r5=%d\n", K_R3, K_R5);
503: 
504:     if (KernelRanClean && KernelExecuted == KernelExecCount && K_R5 == 700) {
505:       Print(L"Loaded kernel execution: PASS\n");
506:     } else if (KernelRanClean && KernelExecuted == KernelExecCount) {
507:       Print(L"Loaded kernel execution: PARTIAL (ran but unexpected register state)\n");
508:     } else {
509:       Print(L"Loaded kernel execution: FAIL\n");
510:     }
511: 
512:     // Configure CPU context for boot.
513:     PpcBootSystem(KernelAddress, KernelSize);
514:   } else {
515:     Print(L"Kernel not found on volume (%r) — skipping kernel execution\n", Status);
516:   }
517:   
518:   // Phase 5: classic Mac OS boot memory map, ROM, and system initialization.
519:   {
520:     UINT64 LowMemAddress = 0, LowMemSize = 0;
521:     UINT64 RomAddress = 0, RomSize = 0;
522:     EFI_STATUS BootStatus;
523: 
524:     Print(L"\n--- Boot memory map / system initialization ---\n");
525: 
526:     // 1. Low-memory globals at guest 0x00000000 (16 KB, read/write).
527:     BootStatus = PpcInstallLowMemory(&LowMemAddress, &LowMemSize);
528:     Print(L"Low-memory region: %s (guest 0x%x, %d bytes)\n",
529:           EFI_ERROR(BootStatus) ? L"FAIL" : L"OK",
530:           (UINT32)LowMemAddress, (UINT64)LowMemSize);
531: 
532:     // 2. System ROM at guest 0xFFF00000. Load a real ROM image from the boot
533:     //    volume if present; otherwise install a self-contained demo ROM so the
534:     //    full ROM -> guest-memory-map -> execution path is still exercised.
535:     BootStatus = PpcInstallSystemRom(PPC_ROM_DEFAULT_PATH, &RomAddress, &RomSize);
536:     if (EFI_ERROR(BootStatus) && BootStatus != EFI_ALREADY_STARTED) {
537:       if (BootStatus == EFI_NOT_FOUND) {
538:         Print(L"System ROM not found on volume, installing demo ROM\n");
539:       } else {
540:         Print(L"System ROM install failed (%r), installing demo ROM\n", BootStatus);
541:       }
542:       BootStatus = PpcInstallDemoRom(&RomAddress, &RomSize);
543:     }
544:     Print(L"System ROM: %s (guest 0x%x, %d bytes)\n",
545:           EFI_ERROR(BootStatus) ? L"FAIL" : L"OK",
546:           (UINT32)RomAddress, (UINT64)RomSize);
547: 
548:     // 3. Self-test the memory map: ROM read-only, low memory R/W, and a
549:     //    cross-region ROM -> RAM program executed from the reset vector.
550:     BootStatus = PpcRunBootSelfTest();
551:     Print(L"Boot memory map self-test: %s\n",
552:           EFI_ERROR(BootStatus) ? L"FAIL" : L"PASS");
553: 
554:     // 4. Configure the CPU for entry at the ROM reset vector and write the
555:     //    boot info block into low memory.
556:     BootStatus = PpcPrepareSystemForBoot();
557:     Print(L"System initialization: %s\n",
558:           EFI_ERROR(BootStatus) ? L"FAIL" : L"PASS");
559: 
560:     // 5. Report the final boot state.
561:     PPC_BOOT_INFO BootInfo;
562:     if (!EFI_ERROR(PpcGetBootInfo(&BootInfo))) {
563:       Print(L"Boot state: ready=%d, kernel=%d, ROM at 0x%x (%d bytes), "
564:             L"low mem at 0x%x (%d bytes)\n",
565:             BootInfo.SystemReady, BootInfo.KernelLoaded,
566:             (UINT32)BootInfo.MemoryMap.RomBase,
567:             (UINT64)BootInfo.MemoryMap.RomSize,
568:             (UINT32)BootInfo.MemoryMap.LowMemoryBase,
569:             (UINT64)BootInfo.MemoryMap.LowMemorySize);
570:     }
571:   }
572: 
573:   Print(L"\n=== EFI-Mac-Emulator Ready ===\n");
574:   
575:   return EFI_SUCCESS;
576: }

(End of file - total 576 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 11.2s)

_Thinking:_

The main.c Phase 5 section is correctly placed after the kernel-load section and before "Ready". Good.

One concern: the kernel load at guest base 0x10000000 happens BEFORE Phase 5's PpcPrepareSystemForBoot, which does `ZeroMem(&g_PpcContext, ...)` and resets PC to the ROM reset vector. That's fine — kernel execution already happened. Also PpcPrepareSystemForBoot resets the CPU context, which is the "system init" for the ROM-based boot. Good.

Wait — there's a subtle interaction: the kernel-load section calls PpcBootSystem which sets PC to kernel. Then Phase 5 runs self-test (PpcExecuteBlock at reset vector, which changes PC), then PpcPrepareSystemForBoot resets PC to ROM reset vector. All fine for a demo.

Now, another consideration: `PpcInstallSystemRom(PPC_ROM_DEFAULT_PATH, ...)` when ROM file doesn't exist returns EFI_NOT_FOUND (from Root->Open failing → returns EFI_NOT_FOUND). Then we install demo ROM. Good. But note: BootReadFileToPages returns EFI_NOT_FOUND only when Open fails and Status==EFI_SUCCESS (line 63). If the file open returns a real error, it returns that error. Fine.

Now there's one more issue: in BootReadFileToPages, when FileSize == 0 or > PPC_ROM_MAX_SIZE, we print and return EFI_LOAD_ERROR — but we don't FreePages (we never allocated yet — allocation is after the size check). Correct order in my code: size check is BEFORE AllocatePages. Let me verify... Looking at my BootReadFileToPages: GetInfo → check size → AllocatePages → read loop → close → FreePages on failure. Yes correct.

Now let me verify PpcInstallSystemRom error path: PpcAddGuestMemoryRegion fails → PpcFreeMemory(Buffer, Size) → free pages. PpcFreeMemory expects the buffer to be page-aligned AllocatePages memory — it is (BootReadFileToPages used AllocatePages). ✓ But wait: PpcFreeMemory prints "Freeing memory at virtual address" and calls BS->FreePages. Size is UINT64 → Pages computed. ✓

Now, an issue with the demo ROM install: I allocate PPC_ROM_MAX_SIZE (4MB) via AllocatePages and add region with size 4MB. The guest-addressable portion is only up to 0xFFFFFFFF = the region covers [0xFFF00000, 0x100300000) but 32-bit guest addresses max at 0xFFFFFFFF, so only [0xFFF00000, 0x100000000) = 1MB is reachable. Fine, not a bug, just a note. The self-test only touches +0x100.

Hmm, but actually — there IS a potential overflow issue in PpcDefaultReadByte: `Address >= GuestBase && (UINT64)(Address - GuestBase) < Size`. For ROM: Address 0xFFF00100, GuestBase 0xFFF00000, diff 0x100 < 0x400000 ✓. For an address like 0xFFF00000 + 0x200000 (2MB into ROM) → diff 0x200000 < 0x400000 ✓ but that host byte is beyond... no wait, diff is within region size 4MB, host buffer has 4MB. Fine. The "unreachable" portion [0x100000000...) simply can't be addressed since Address is UINT32.

OK. Now let me also double check: does `PpcLoadSystemRom`'s old caller PpcLoadFile still exist and is it unused now? Doesn't matter.

Now let me look at the final part of bootloader_impl.c (self-test + prepare) once more to make sure everything is in order, and confirm no leftover duplicate PpcPrepareSystemForBoot. Let me read lines 700-830.

Placement is correct. Now review the final self-test/prepare section and confirm no leftover duplicates:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "limit": 140,
  "offset": 695
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c</path>
<type>file</type>
<content>
695:         Print(L"Failed to initialize hardware abstraction: %r\n", Status);
696:         return Status;
697:     }
698:     
699:     // Initialize graphics for boot process
700:     Status = PpcInitializeGraphics(640, 480, 32);
701:     if (EFI_ERROR(Status)) {
702:         Print(L"Failed to initialize graphics: %r\n", Status);
703:         return Status;
704:     }
705:     
706:     Print(L"Boot environment setup complete\n");
707:     
708:     return EFI_SUCCESS;
709: }
710: 
711: EFI_STATUS
712: PpcPrepareSystemForBoot (
713:     VOID
714:     )
715: {
716:     UINT64 RamBase = 0;
717:     UINT64 RamSize = 0;
718:     UINTN  I;
719:     EFI_STATUS Status;
720: 
721:     Print(L"Preparing system for boot\n");
722: 
723:     // The guest memory map must be ready before the CPU can start.
724:     if (!g_BootContext.LowMemoryInstalled) {
725:         PpcInstallLowMemory(NULL, NULL);
726:     }
727:     if (!g_BootContext.RomLoaded) {
728:         Print(L"Warning: no system ROM installed; boot would fail at the reset vector\n");
729:     }
730: 
731:     Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);
732:     if (EFI_ERROR(Status) || RamSize == 0) {
733:         Print(L"System preparation failed: guest RAM unavailable\n");
734:         return EFI_NOT_READY;
735:     }
736: 
737:     // Reset the CPU to the classic Mac OS boot state: PC at the ROM reset
738:     // vector with machine-check and recoverable-interrupt handling enabled.
739:     ZeroMem(&g_PpcContext, sizeof(g_PpcContext));
740:     for (I = 0; I < 32; I++) {
741:         g_PpcContext.Gpr[I] = 0;
742:     }
743:     g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;
744:     g_PpcContext.Pc = PPC_RESET_VECTOR;
745:     g_PpcContext.Srr0 = PPC_RESET_VECTOR;
746:     g_PpcContext.Srr1 = g_PpcContext.Msr;
747:     g_PpcContext.ExceptionPending = 0;
748: 
749:     // Write the emulator boot info block into low memory: magic, then
750:     // RAM base, RAM size, ROM base, and installed ROM size (big-endian).
751:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET, 0x45464921);
752:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 0, (UINT32)RamBase);
753:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 4, (UINT32)RamSize);
754:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 8, PPC_ROM_GUEST_BASE);
755:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 12,
756:                     (UINT32)(g_BootContext.RomLoaded ? g_BootContext.RomSize : 0));
757: 
758:     g_BootContext.SystemReady = TRUE;
759:     g_BootContext.SystemBooting = TRUE;
760: 
761:     Print(L"System prepared: PC=0x%x MSR=0x%08x SRR0=0x%x SRR1=0x%x\n",
762:           g_PpcContext.Pc, g_PpcContext.Msr, g_PpcContext.Srr0, g_PpcContext.Srr1);
763:     Print(L"Boot info block written to low memory at 0x%x\n",
764:           PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET);
765: 
766:     return EFI_SUCCESS;
767: }
768: 
769: EFI_STATUS
770: PpcRunBootSelfTest (
771:     VOID
772:     )
773: {
774:     UINT64 RamBase = 0;
775:     UINT64 RamSize = 0;
776:     UINTN  Executed = 0;
777:     EFI_STATUS Status;
778:     UINT32 MagicPlusOne;
779: 
780:     g_BootTestPasses = 0;
781:     g_BootTestFailures = 0;
782: 
783:     Print(L"--- Boot Memory Map / System Init Self-Test ---\n");
784: 
785:     Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);
786:     BootSelfTestCheck(Status == EFI_SUCCESS && RamSize > 0,
787:                       L"guest RAM region available");
788: 
789:     // Low-memory globals: writable and readable.
790:     PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08, 0xAA);
791:     BootSelfTestCheck(
792:         PpcReadGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08) == 0xAA,
793:         L"low-memory globals read/write (guest 0x00000000)");
794: 
795:     // ROM: magic word readable through the interpreter's memory path.
796:     BootSelfTestCheck(
797:         PpcReadGuestByte(PPC_ROM_GUEST_BASE + 0) == 'R' &&
798:         PpcReadGuestByte(PPC_ROM_GUEST_BASE + 1) == 'O' &&
799:         PpcReadGuestByte(PPC_ROM_GUEST_BASE + 2) == 'M' &&
800:         PpcReadGuestByte(PPC_ROM_GUEST_BASE + 3) == '1',
801:         L"ROM magic word 'ROM1' readable at guest 0xFFF00000");
802: 
803:     // ROM is read-only to guest stores.
804:     PpcWriteGuestByte(PPC_ROM_GUEST_BASE + 0, 0xEE);
805:     BootSelfTestCheck(
806:         PpcReadGuestByte(PPC_ROM_GUEST_BASE + 0) == 'R',
807:         L"ROM rejects guest writes (read-only)");
808: 
809:     // Cross-region execution: run the reset-vector program in the ROM; it
810:     // loads the ROM magic word and stores its successor into guest RAM.
811:     PpcSetGprValue(1, (UINT32)RamBase);
812:     PpcSetGprValue(3, 0);
813:     PpcSetGprValue(4, 0);
814:     PpcSetGprValue(5, 0);
815:     Status = PpcExecuteBlock((UINT32*)(UINTN)PPC_RESET_VECTOR, 4, &Executed);
816: 
817:     MagicPlusOne = 0x524F4D31 + 1;
818:     BootSelfTestCheck(Status == EFI_SUCCESS && Executed == 4,
819:                       L"reset-vector program ran cleanly");
820:     BootSelfTestCheck(PpcGetGprValue(4) == 0x524F4D31,
821:                       L"program read ROM word (r4 = 'ROM1')");
822:     BootSelfTestCheck(
823:         PpcReadGuestByte((UINT32)RamBase + 0) == (UINT8)(MagicPlusOne >> 24) &&
824:         PpcReadGuestByte((UINT32)RamBase + 1) == (UINT8)(MagicPlusOne >> 16) &&
825:         PpcReadGuestByte((UINT32)RamBase + 2) == (UINT8)(MagicPlusOne >> 8) &&
826:         PpcReadGuestByte((UINT32)RamBase + 3) == (UINT8)MagicPlusOne,
827:         L"program stored result to guest RAM");
828: 
829:     Print(L"--- Boot self-test complete: %d passed, %d failed ---\n",
830:           g_BootTestPasses, g_BootTestFailures);
831: 
832:     return (g_BootTestFailures == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;
833: }

(End of file - total 833 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 11.0s)

_Thinking:_

Everything looks good. Now let me cross-check a few remaining items:

1. `PpcSetGprValue(1, (UINT32)RamBase)` — RamBase is 0x10000000. But wait: `PpcSetGprValue(1, ...)` — gpr index 1 = r1. Fine. But note the demo ROM program's `stw r5, 0(r1)` stores to guest RamBase = 0x10000000. Readback reads guest 0x10000000..3. ✓ But hold on — this overwrites the FIRST WORD of the loaded kernel (kernel loaded at guest base 0x10000000)! The kernel was already executed before Phase 5, so no problem.

2. `MagicPlusOne` declared but assigned later (line 817) before use (line 823) — fine in C.

3. In PpcRunBootSelfTest, `PpcSetGprValue(1, (UINT32)RamBase)` — gpr 1 holds stack pointer. Setting to RamBase (start of RAM) is a bit unusual but works for the demo. Fine.

4. There's one remaining concern: PpcExecuteBlock's self-test relies on the ROM region being installed. If BOTH the real-ROM install and demo-ROM install fail (unlikely), the ROM magic check would read zeros from unmapped memory and the test would fail — correct behavior.

5. `BootSelfTestCheck` is `STATIC VOID` declared before PpcRunBootSelfTest — yes, at line ~115-ish region. Let me verify it's before line 770. It was added in the helpers block after the context struct, around line 120-150. Yes.

Now, a subtle bug check in `BootReadFileToPages`: the return for the "invalid size" path returns EFI_LOAD_ERROR with the comment "for ROM load" — good.

Also, `PpcInstallSystemRom` — the parameter `RomPath` could be NULL? main.c passes PPC_ROM_DEFAULT_PATH (non-NULL). PpcLoadSystemRom checks NULL. ✓

Now, one more compile-check: in BootReadFileToPages, I declared `EFI_FILE_INFO* Info = AllocateZeroPool(FileInfoSize);` then used `&GenericFileInfo`. GNU-EFI's efilib has `GenericFileInfo` exported? Let me check — in GNU-EFI, `EFI_GUID GenericFileInfo = EFI_FILE_INFO_ID;` is declared in efilib.h. Yes, GNU-EFI provides `GenericFileInfo` in efilib. The existing kernel-load code (line 235) uses `&GenericFileInfo` too. ✓

Also `SIZE_OF_EFI_FILE_INFO` — GNU-EFI efi.h defines it? Let me check: existing code line 228 uses it. ✓

Now, one thing to double check: my BootReadFileToPages uses `Print(L"File '%s' has invalid size %d for ROM load\n", FilePath, FileSize)` — FileSize is UINT64, %d — consistent with existing style.

Now, the region scan in PpcDefaultReadByte/WriteByte uses first-match-wins. With non-overlapping regions, fine.

Let me now check translation.h for the declaration placement of PpcAddGuestMemoryRegion and that PPC_GUEST_REGION etc. isn't needed in the header (interpreter.c defines it locally — yes, static). Good.

Next: update TODO.md and then do a final full-file read of interpreter.c region code + translation.h to make sure everything is consistent.

Let me read TODO.md first.

Implementation looks solid. Let me review the final interpreter region code and check TODO.md for the update:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\translation.h",
  "limit": 40,
  "offset": 95
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation.h</path>
<type>file</type>
<content>
95:   @retval EFI_STATUS
96: **/
97: EFI_STATUS
98: EFIAPI
99: PpcInitializeTranslationContext (
100:     VOID
101:     );
102: 
103: /**
104:   Translate a single PowerPC instruction to x86_64
105:   @param[in]  PpcInstruction   The PowerPC instruction to translate
106:   @param[out] X86Instruction   Pointer to store the translated instruction
107:   @retval EFI_STATUS
108: **/
109: EFI_STATUS
110: EFIAPI
111: PpcTranslateInstruction (
112:     IN  UINT32  PpcInstruction,
113:     OUT UINT64* X86Instruction
114:     );
115: 
116: /**
117:   Decode a single PowerPC instruction into a short mnemonic string
118:   @param[in]  Instruction  The PowerPC instruction to decode
119:   @param[out] Buffer       Buffer to receive the mnemonic (NUL terminated)
120:   @param[in]  BufferSize   Size of Buffer in bytes (must be >= 16)
121:   @retval EFI_STATUS
122: **/
123: EFI_STATUS
124: EFIAPI
125: PpcDecodeInstruction (
126:     IN  UINT32 Instruction,
127:     OUT CHAR16* Buffer,
128:     IN  UINTN   BufferSize
129:     );
130: 
131: /**
132:   Fetch a PowerPC instruction from guest memory
133:   @param[in]  Address  Guest address to fetch from
134:   @retval The fetched instruction word

(Showing lines 95-134 of 353. Use offset=135 to continue.)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\TODO.md"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\TODO.md</path>
<type>file</type>
<content>
1: # EFI-Mac-Emulator - Implementation Plan
2: 
3: ## Current State
4: 
5: Phase 4 (Emulation Components) is complete and verified in a single QEMU/OVMF
6: run alongside the Phase 3 work.
7: 
8: **Phase 3 recap:** All `_impl.c` files use real UEFI services. Interpreter passes
9: an 18-check self-test. Memory manager allocates 256 MB of guest RAM via
10: `AllocatePages`; a small PPC program is executed from guest RAM at boot.
11: Bootloader `PpcLoadKernel` resolves the boot volume, opens
12: `\System\MacOS\kernel` from the FAT volume, and reads it into guest RAM;
13: `PpcVerifyKernel` bounds-checks and reads the first word; `PpcBootSystem`
14: configures PC/SRR0/SRR1/MSR. Debug writes `boot.log` via real file I/O and uses
15: `GetNextMonotonicCount` for timers. UEFI interface functions use real
16: `HandleProtocol`/`LocateHandleBuffer`/`GetMemoryMap`.
17: 
18: **Graphics (GOP framebuffer):** `PpcInitializeGraphics` sets a real GOP mode
19: and carves a guest-visible framebuffer window out of guest RAM at guest
20: 0x18000000 (640x480x32). Guest code writes big-endian 0xRRGGBB00 pixels there;
21: `PpcGraphicsBlitToDisplay` converts and copies the window to the real GOP
22: framebuffer (byte-exact RGB and BGR pixel layouts). `PpcGetFrameBufferInfo`
23: reports the window plus the GOP base/pitch/pixel format,
24: `PpcGraphicsClear`/`SetPixel`/`DrawRect` provide host-side helpers. The
25: self-check now includes a multi-frame test: three solid full-screen frames
26: (red/green/blue) verified across every pixel of the GOP framebuffer, a
27: four-band frame checked at centers and boundaries, corner pixels, and
28: out-of-bounds writes. This test caught and led to a fix of a blit bug where
29: channels were shifted 8 bits too high (blue landed in the reserved byte).
30: 
31: **Networking (SNP):** `PpcInitializeNetwork` enumerates every UEFI Simple
32: Network Protocol instance, calls real `Start`/`Initialize` on each, sets the
33: standard receive filter, snapshots the real mode (MAC, media present, iftype,
34: max packet), and transmits a real 64-byte ARP-style frame via `Transmit` with a
35: `GetStatus` poll. Every initialized interface is stored in
36: `PPC_NETWORK_INFO.Interfaces[]` (up to `PPC_MAX_NETWORK_INTERFACES`), so
37: multi-NIC systems are fully brought up and tested. `PpcGetNetworkInfo` reports
38: per-interface results.
39: 
40: **Storage (Block I/O):** `PpcInitializeBlockIo` enumerates every UEFI Block I/O
41: protocol instance and reports real geometry (block size, block count,
42: removable/read-only flags). `PpcReadDiskBlock` performs a real `ReadBlocks`
43: sector read. Self-check reads LBA 0 of every device until it finds the "EFI"
44: marker. `PpcGetBlockIoInfo` reports the state.
45: 
46: **Audio (emulated device):** UEFI has no standard audio output protocol, so the
47: audio "device" is a fixed ring buffer inside guest RAM at guest 0x18800000
48: (8KB, 44100 Hz stereo 16-bit). Guest code fills it with big-endian PCM samples;
49: the host reads them back and advances a play cursor. `PpcAudioGetBufferInfo`,
50: `PpcAudioWriteSample`, `PpcAudioReadSample`, `PpcAudioAdvancePlayback`,
51: `PpcAudioReset` implement the device. Self-check verifies a two-sample write via
52: the guest RAM path reads back correctly and playback advances.
53: 
54: **Verified in QEMU/OVMF (single boot, mixed devices):** Graphics self-check
55: PASS plus the full multi-frame test PASS; Audio self-check PASS; Block I/O
56: self-check PASS (4 devices: FAT ESP + IDE raw disk + virtio-blk raw disk +
57: partition, marker found); Network self-check PASS for **both** an e1000 NIC
58: (MAC ...:56) and a virtio-net-pci NIC (MAC ...:57) simultaneously — both
59: transmit PASS; plus all Phase 3 checks still PASS (18/18 self-test, guest RAM
60: demo, kernel load/verify/execute, boot.log). Build is clean with
61: `make clean && make` (GNU-EFI + clang/lld-link on macOS).
62: 
63: ## Phase 1: Research and Analysis
64: - [x] Analyze existing Mac emulators (SheepShaver, Basilisk II, QEMU, DingusPPC)
65: - [x] Study PowerPC vs 68k architecture differences
66: - [x] Review UEFI specifications and implementation guidelines
67: - [x] Document findings in project README
68: 
69: ## Phase 2: Architecture Design
70: - [x] Define module interfaces (headers) for CPU, memory, hardware, boot, utils, platform
71: - [ ] Complete CPU translation layer requirements (instruction set coverage, register mapping, MMU)
72: - [ ] Complete hardware abstraction layer specification (graphics, audio, storage, network)
73: - [ ] Plan memory management approach (UEFI pool vs. guest memory regions)
74: 
75: ## Phase 3: Core Implementation
76: - [x] Get the UEFI application to build with a real toolchain (GNU-EFI + clang/lld-link)
77: - [x] Implement CPU instruction decoder and interpreter (PowerPC)
78: - [x] Implement register file and special-purpose registers (MSR, SRR0/1, CTR, LR)
79: - [x] Implement memory manager backed by UEFI allocation
80: - [x] Wire up hardware abstraction interface to real UEFI protocols (graphics -> GOP, storage -> Simple File System; audio/network still simulated)
81: - [x] Remove placeholder/simulated behavior from current `_impl.c` files
82: 
83: ## Phase 4: Emulation Components
84: - [x] Implement graphics subsystem (framebuffer via GOP)
85: - [x] Add audio handling
86: - [x] Integrate storage I/O (UEFI file protocols / block I/O)
87: - [x] Implement basic networking
88: 
89: ## Phase 5: Boot Process
90: - [ ] Create bootloader for Mac OS (load ROM image, set up guest memory map)
91: - [ ] Implement system initialization routines
92: - [ ] Add support for system files and drivers
93: - [ ] Test boot process with various Mac OS versions
94: 
95: ## Phase 6: Testing and Optimization
96: - [ ] Test with Mac OS 7, 8, and 9
97: - [ ] Optimize performance
98: - [ ] Fix compatibility issues
99: - [ ] Document usage instructions
100: 
101: ## Architecture Decisions
102: 
103: ### Target Architecture: PowerPC
104: - Selected over 68k due to:
105:   - Better existing support from SheepShaver
106:   - More complete compatibility with Mac OS 8/9
107:   - Simpler translation layer compared to 68k CISC instructions
108: 
109: ### UEFI Approach
110: - Leverages modern boot infrastructure
111: - Direct hardware access capabilities
112: - Better memory management
113: - Support for large storage devices

(End of file - total 113 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 7.4s)

_Thinking:_

Now update TODO.md with Phase 5 progress. I'll:
1. Update the "Current State" to mention Phase 5 work underway/added.
2. Update Phase 5 checklist to reflect what's been implemented: bootloader ROM loading, guest memory map (multi-region: RAM + ROM + low memory), system init routines, boot self-test.

Let me update the Current State section to add a Phase 5 paragraph, and update the Phase 5 checklist.

Let me write the edits.

Now update TODO.md to reflect Phase 5 progress:

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\TODO.md",
  "newString": "# EFI-Mac-Emulator - Implementation Plan\n\n## Current State\n\nPhase 5 (Boot Process) first deliverable is implemented: the classic Mac OS boot\nmemory map, ROM loading, and system initialization routines.\n\n**Phase 5 recap:** The CPU guest memory map is now multi-region\n(`PpcAddGuestMemoryRegion` in `src/cpu/interpreter.c`): the primary 256 MB guest\nRAM at guest 0x10000000, a dedicated read/write low-memory globals region at\nguest 0x00000000 (16 KB), and a read-only system ROM window at guest 0xFFF00000.\n`PpcInstallLowMemory`/`PpcInstallSystemRom`/`PpcInstallDemoRom` in\n`src/boot/bootloader_impl.c` install the regions; the demo ROM is a\nself-contained image with a reset-vector program. `PpcPrepareSystemForBoot`\nresets the CPU to the ROM reset vector with `MSR = ME|RI` and writes an emulator\nboot info block (magic + RAM/ROM geometry) into low memory.\n`PpcRunBootSelfTest` verifies: low-memory read/write, ROM magic word `ROM1`\nreadable, ROM read-only enforcement, and a cross-region ROM -> RAM program\nexecuted from the reset vector. `src/main.c` wires the Phase 5 sequence\n(install low memory -> install ROM with demo fallback -> self-test -> prepare ->\nreport). Code is written against GNU-EFI idioms; a build/run on macOS\n(`make clean && make`, then QEMU/OVMF) is still required to verify.\n\n**Phase 4 recap** (complete and verified in a single QEMU/OVMF run alongside\nthe Phase 3 work):",
  "oldString": "# EFI-Mac-Emulator - Implementation Plan\n\n## Current State\n\nPhase 4 (Emulation Components) is complete and verified in a single QEMU/OVMF\nrun alongside the Phase 3 work."
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 4.8s)

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\TODO.md",
  "newString": "## Phase 5: Boot Process\n- [x] Create bootloader for Mac OS (load ROM image, set up guest memory map) - first deliverable: multi-region guest memory map (RAM + low-memory globals + read-only ROM window), ROM load from volume with demo-ROM fallback\n- [x] Implement system initialization routines - CPU reset to ROM reset vector (MSR ME|RI), boot info block in low memory, `PpcRunBootSelfTest` (low-memory R/W, ROM read-only, cross-region ROM->RAM execution)\n- [ ] Add support for system files and drivers\n- [ ] Test boot process with various Mac OS versions\n- [ ] Verify Phase 5 on macOS (`make clean && make`) and in QEMU/OVMF",
  "oldString": "## Phase 5: Boot Process\n- [ ] Create bootloader for Mac OS (load ROM image, set up guest memory map)\n- [ ] Implement system initialization routines\n- [ ] Add support for system files and drivers\n- [ ] Test boot process with various Mac OS versions"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 3.9s)

_Thinking:_

Now the final cross-check task. Let me:
1. Verify the region functions in interpreter.c once more (they were edited earlier).
2. Verify the PpcAddGuestMemoryRegion declaration in translation.h.
3. Do a syntax sanity check — I can't compile, but I can check for obvious issues like balanced braces by viewing key sections.
4. Run git diff to review all changes.

Let me run git diff --stat and view the interpreter.c region section.

Now the final cross-check. Let me review all changes via git diff:

**Tool: bash**

**Input:**
```json
{
  "command": "git diff --stat; git status --short"
}
```

**Output:**
```
 TODO.md                    |  28 ++-
 src/boot/bootloader.h      |  97 ++++++++++
 src/boot/bootloader_impl.c | 452 +++++++++++++++++++++++++++++++++++++++++++--
 src/cpu/interpreter.c      | 107 ++++++++---
 src/cpu/translation.h      |  27 +++
 src/main.c                 |  55 ++++++
 6 files changed, 724 insertions(+), 42 deletions(-)
 M TODO.md
 M src/boot/bootloader.h
 M src/boot/bootloader_impl.c
 M src/cpu/interpreter.c
 M src/cpu/translation.h
 M src/main.c

```

---

## Assistant (Build · Big Pickle · 2.7s)

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 75,
  "offset": 195
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
195:             }
196:             return;  // First matching region decides the target
197:         }
198:     }
199: }
200: 
201: static PPC_CPU_READ_MEMORY  g_ReadByte  = PpcDefaultReadByte;
202: static PPC_CPU_WRITE_MEMORY g_WriteByte = PpcDefaultWriteByte;
203: 
204: EFI_STATUS
205: PpcSetMemoryAccess (
206:     IN PPC_CPU_READ_MEMORY   Read,
207:     IN PPC_CPU_WRITE_MEMORY  Write
208:     )
209: {
210:     g_ReadByte  = (Read  != NULL) ? Read  : PpcDefaultReadByte;
211:     g_WriteByte = (Write != NULL) ? Write : PpcDefaultWriteByte;
212:     return EFI_SUCCESS;
213: }
214: 
215: EFI_STATUS
216: PpcSetGuestMemory (
217:     IN VOID*  HostBase,
218:     IN UINT32 GuestBase,
219:     IN UINT32 Size
220:     )
221: {
222:     return PpcAddGuestMemoryRegion(HostBase, GuestBase, Size, FALSE);
223: }
224: 
225: EFI_STATUS
226: PpcAddGuestMemoryRegion (
227:     IN VOID*   HostBase,
228:     IN UINT32  GuestBase,
229:     IN UINT32  Size,
230:     IN BOOLEAN ReadOnly
231:     )
232: {
233:     UINTN I;
234: 
235:     if (HostBase == NULL || Size == 0) {
236:         return EFI_INVALID_PARAMETER;
237:     }
238: 
239:     // Reject a region that would overlap an already installed one.
240:     for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
241:         if (!g_GuestRegions[I].Active) {
242:             continue;
243:         }
244:         if (GuestBase < g_GuestRegions[I].GuestBase) {
245:             if ((UINT64)GuestBase + Size > g_GuestRegions[I].GuestBase) {
246:                 return EFI_ALREADY_STARTED;
247:             }
248:         } else {
249:             if ((UINT64)(GuestBase - g_GuestRegions[I].GuestBase) <
250:                 g_GuestRegions[I].Size) {
251:                 return EFI_ALREADY_STARTED;
252:             }
253:         }
254:     }
255: 
256:     for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
257:         if (!g_GuestRegions[I].Active) {
258:             g_GuestRegions[I].HostBase  = HostBase;
259:             g_GuestRegions[I].GuestBase = GuestBase;
260:             g_GuestRegions[I].Size      = Size;
261:             g_GuestRegions[I].ReadOnly  = ReadOnly;
262:             g_GuestRegions[I].Active    = TRUE;
263:             return EFI_SUCCESS;
264:         }
265:     }
266: 
267:     return EFI_OUT_OF_RESOURCES;
268: }
269: 

(Showing lines 195-269 of 1575. Use offset=270 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 9.1s)

_Thinking:_

There's one edge case in the overlap check: ROM at guest 0xFFF00000 with size 0x400000: `(UINT64)GuestBase + Size` = 0xFFF00000 + 0x400000 = 0x100300000. Compare against other regions. RAM at 0x10000000:
- GuestBase (0xFFF00000) >= RAM.GuestBase (0x10000000) → else branch: 0xFFF00000 - 0x10000000 = 0xEFF00000 < RAM.Size 0x10000000? 0xEFF00000 = 4024434688, RAM.Size = 268435456. Not less. No overlap. ✓
Low memory at 0:
- GuestBase (0xFFF00000) >= 0 → else branch: 0xFFF00000 - 0 = 0xFFF00000 < 0x4000? No. ✓

Now, there's an interesting subtlety in the overlap check for the low-memory region (GuestBase=0, Size=0x4000) vs RAM:
- GuestBase (0) < RAM.GuestBase (0x10000000) → first branch: (UINT64)0 + 0x4000 = 0x4000 > 0x10000000? No. ✓ No overlap.

Everything is consistent. 

Now, the demo ROM: allocated 4MB pages, region size 4MB at 0xFFF00000. The region covers up to 0x100300000 in UINT64 but guest addresses are UINT32 (max 0xFFFFFFFF). Only 1MB reachable. Fine.

One last concern about `PpcExecuteBlock` and the ROM: PpcExecuteBlock fetches 4 instructions starting at Pc=0xFFF00100. But there's a subtlety — the ROM region read via g_ReadByte works. ✓

Now let me also double-check the diff of interpreter.c around lines 155-160 (the struct + array definition) to ensure no leftover single-region globals remain and that the static array init is fine.

Let me look at lines 140-160.

Overlap checks verified correct. Let me confirm the region globals and the struct definition replaced the old single-region globals cleanly:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 40,
  "offset": 140
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
140: // Emulated guest memory (default memory backing for the interpreter)
141: //
142: // The primary guest RAM region is installed with PpcSetGuestMemory() and
143: // occupies slot 0. Additional regions (the classic Mac OS ROM window at
144: // 0xFFF00000, low-memory globals at 0x00000000) are added with
145: // PpcAddGuestMemoryRegion(). Addresses not covered by any region read as
146: // zero and ignore writes.
147: // ---------------------------------------------------------------------------
148: #define PPC_MAX_GUEST_REGIONS 4
149: 
150: typedef struct {
151:     VOID*   HostBase;   // Host virtual address backing the region
152:     UINT32  GuestBase;  // Guest-visible base address of the region
153:     UINT32  Size;       // Region size in bytes
154:     BOOLEAN ReadOnly;   // TRUE: guest stores are dropped
155:     BOOLEAN Active;     // TRUE: slot is in use
156: } PPC_GUEST_REGION;
157: 
158: // Guest memory map. Lookup is in insertion order; the first region that
159: // contains an address wins.
160: static PPC_GUEST_REGION g_GuestRegions[PPC_MAX_GUEST_REGIONS];
161: 
162: static UINT8
163: PpcDefaultReadByte (
164:     IN UINT32 Address
165:     )
166: {
167:     UINTN I;
168: 
169:     for (I = 0; I < PPC_MAX_GUEST_REGIONS; I++) {
170:         if (g_GuestRegions[I].Active &&
171:             Address >= g_GuestRegions[I].GuestBase &&
172:             (UINT64)(Address - g_GuestRegions[I].GuestBase) < g_GuestRegions[I].Size) {
173:             return *(volatile UINT8*)((UINTN)g_GuestRegions[I].HostBase +
174:                                       (Address - g_GuestRegions[I].GuestBase));
175:         }
176:     }
177:     return 0;  // Unmapped guest address reads as zero
178: }
179: 

(Showing lines 140-179 of 1575. Use offset=180 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 6.0s)

_Thinking:_

Everything looks clean. Now let me do a final syntax sanity check by searching for obvious issues in the new bootloader code — e.g., verifying there are no undefined identifiers. Let me grep the bootloader_impl.c for the new identifiers and check they're all defined:

- PpcReadGuestByte, PpcWriteGuestByte — translation.h ✓
- PpcExecuteBlock — translation.h ✓
- PpcGetGprValue, PpcSetGprValue — translation.h ✓
- PpcGetGuestMemoryRegion — manager.h ✓
- PpcFreeMemory — manager.h ✓
- PpcGetFileSystem — uefi_interface.h ✓
- PpcAddGuestMemoryRegion — translation.h ✓
- g_PpcContext — interpreter.h ✓
- PPC_MSR_ME, PPC_MSR_RI — translation.h ✓
- SIZE_OF_EFI_FILE_INFO, GenericFileInfo, AllocateZeroPool, FreePool, AllocatePages, AllocateAnyPages, EfiBootServicesData, BS, Print — efi.h/efilib.h ✓
- EFI_FILE_INFO, EFI_FILE_IO_INTERFACE, EFI_FILE_HANDLE, EFI_FILE_MODE_READ — efi.h ✓
- PPC_ROM_MAX_SIZE etc — bootloader.h ✓

Let me grep for a couple of these that are in the bootloader.h I edited to make sure the defines are present: PPC_ROM_DEFAULT_PATH, PPC_RESET_VECTOR, PPC_LOW_MEM_MAGIC_OFFSET, PPC_LOW_MEM_BOOTINFO_OFFSET, PPC_ROM_GUEST_BASE, PPC_LOW_MEM_GUEST_BASE, PPC_LOW_MEM_SIZE, PPC_ROM_MAX_SIZE, PPC_GUEST_MEMORY_MAP, PPC_BOOT_INFO fields.

Let me read bootloader.h fully to verify.

Let me verify the final bootloader.h and confirm all new symbols are defined:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader.h"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader.h</path>
<type>file</type>
<content>
1: #ifndef __PPC_BOOTLOADER_H__
2: #define __PPC_BOOTLOADER_H__
3: 
4: #include <efi.h>
5: 
6: // Boot modes
7: #define PPC_BOOT_MODE_NORMAL    0
8: #define PPC_BOOT_MODE_RECOVERY  1
9: #define PPC_BOOT_MODE_DIAGNOSTIC 2
10: 
11: // Boot parameters structure
12: typedef struct {
13:     UINT32 BootMode;
14:     UINT32 MemorySizeMB;
15:     UINT32 VideoMode;
16:     BOOLEAN EnableDebug;
17:     CHAR16* CommandLine;
18: } PPC_BOOT_PARAMETERS;
19: 
20: // Classic Mac OS PPC boot memory map (guest-visible addresses)
21: #define PPC_ROM_GUEST_BASE      0xFFF00000  // ROM window (classic PPC Macs)
22: #define PPC_ROM_MAX_SIZE        0x00400000  // 4 MB ROM window
23: #define PPC_ROM_DEFAULT_PATH    L"\\System\\MacOS\\ROM"
24: #define PPC_RESET_VECTOR        (PPC_ROM_GUEST_BASE + 0x100)
25: #define PPC_LOW_MEM_GUEST_BASE  0x00000000  // Low-memory globals
26: #define PPC_LOW_MEM_SIZE        0x4000      // 16 KB
27: 
28: // Low-memory global offsets (emulator-defined boot info block)
29: #define PPC_LOW_MEM_MAGIC_OFFSET    0x0000
30: #define PPC_LOW_MEM_BOOTINFO_OFFSET 0x0100
31: 
32: // Guest memory map as installed for a classic Mac OS boot
33: typedef struct {
34:     BOOLEAN RomInstalled;
35:     UINT64  RomBase;
36:     UINT64  RomSize;
37:     BOOLEAN LowMemoryInstalled;
38:     UINT64  LowMemoryBase;
39:     UINT64  LowMemorySize;
40:     BOOLEAN Ready;
41: } PPC_GUEST_MEMORY_MAP;
42: 
43: // Boot information structure
44: typedef struct {
45:     BOOLEAN IsInitialized;
46:     EFI_PHYSICAL_ADDRESS KernelAddress;
47:     UINT64 KernelSize;
48:     BOOLEAN KernelLoaded;
49:     BOOLEAN SystemReady;
50:     PPC_GUEST_MEMORY_MAP MemoryMap;
51: } PPC_BOOT_INFO;
52: 
53: /**
54:   Initialize PowerPC bootloader
55:   @retval EFI_STATUS
56: **/
57: EFI_STATUS
58: EFIAPI
59: PpcInitializeBootloader (
60:     VOID
61:     );
62: 
63: /**
64:   Load kernel image into memory
65:   @param[in]  ImagePath      Path to the kernel image
66:   @param[out] KernelAddress  Pointer to store kernel load address
67:   @param[out] KernelSize     Pointer to store kernel size
68:   @retval EFI_STATUS
69: **/
70: EFI_STATUS
71: EFIAPI
72: PpcLoadKernel (
73:     IN  CHAR16* ImagePath,
74:     OUT EFI_PHYSICAL_ADDRESS* KernelAddress,
75:     OUT UINT64* KernelSize
76:     );
77: 
78: /**
79:   Boot the PowerPC system
80:   @param[in] KernelAddress Address of kernel to boot
81:   @param[in] KernelSize    Size of kernel to boot
82:   @retval EFI_STATUS
83: **/
84: EFI_STATUS
85: EFIAPI
86: PpcBootSystem (
87:     IN EFI_PHYSICAL_ADDRESS KernelAddress,
88:     IN UINT64               KernelSize
89:     );
90: 
91: /**
92:   Load a boot image into memory
93:   @param[in]  ImagePath   Path to the boot image
94:   @param[out] ImageBuffer Pointer to store image buffer address
95:   @param[out] ImageSize   Pointer to store image size
96:   @retval EFI_STATUS
97: **/
98: EFI_STATUS
99: EFIAPI
100: PpcLoadBootImage (
101:     IN  CHAR16* ImagePath,
102:     OUT VOID**  ImageBuffer,
103:     OUT UINT64* ImageSize
104:     );
105: 
106: /**
107:   Set boot parameters for system boot
108:   @param[in] Parameters Boot parameters to set
109:   @retval EFI_STATUS
110: **/
111: EFI_STATUS
112: EFIAPI
113: PpcSetBootParameters (
114:     IN PPC_BOOT_PARAMETERS* Parameters
115:     );
116: 
117: /**
118:   Get current boot information
119:   @param[out] BootInfo Pointer to structure to fill with boot info
120:   @retval EFI_STATUS
121: **/
122: EFI_STATUS
123: EFIAPI
124: PpcGetBootInfo (
125:     OUT PPC_BOOT_INFO* BootInfo
126:     );
127: 
128: /**
129:   Set up the boot environment for the PowerPC system
130:   @retval EFI_STATUS
131: **/
132: EFI_STATUS
133: EFIAPI
134: PpcSetupBootEnvironment (
135:     VOID
136:     );
137: 
138: /**
139:   Verify a loaded kernel image: bounds check against guest RAM and
140:   read the first word to confirm the data was read correctly.
141:   @param[in] KernelAddress Address of the loaded kernel
142:   @param[in] KernelSize    Size of the loaded kernel
143:   @retval EFI_STATUS
144: **/
145: EFI_STATUS
146: EFIAPI
147: PpcVerifyKernel (
148:     IN  EFI_PHYSICAL_ADDRESS KernelAddress,
149:     IN  UINT64               KernelSize
150:     );
151: 
152: /**
153:   Load a system ROM image into memory
154:   @param[in]  RomPath    Path to the ROM image
155:   @param[out] RomBuffer  Pointer to store ROM buffer address
156:   @param[out] RomSize    Pointer to store ROM size
157:   @retval EFI_STATUS
158: **/
159: EFI_STATUS
160: EFIAPI
161: PpcLoadSystemRom (
162:     IN  CHAR16* RomPath,
163:     OUT VOID**  RomBuffer,
164:     OUT UINT64* RomSize
165:     );
166: 
167: /**
168:   Load a system ROM image from the boot volume and map it into guest memory
169:   at PPC_ROM_GUEST_BASE as a read-only region.
170:   @param[in]  RomPath     Path to the ROM image on the boot volume
171:   @param[out] RomAddress  Guest address of the installed ROM (may be NULL)
172:   @param[out] RomSize     Installed ROM size in bytes (may be NULL)
173:   @retval EFI_SUCCESS          ROM installed
174:   @retval EFI_NOT_FOUND        ROM file not present on the volume
175:   @retval EFI_ALREADY_STARTED  A ROM is already installed
176: **/
177: EFI_STATUS
178: EFIAPI
179: PpcInstallSystemRom (
180:     IN  CHAR16* RomPath,
181:     OUT UINT64* RomAddress,
182:     OUT UINT64* RomSize
183:     );
184: 
185: /**
186:   Install a self-contained demo ROM at PPC_ROM_GUEST_BASE (read-only). The
187:   demo ROM contains a small reset-vector program that reads the ROM magic
188:   word and stores its successor into guest RAM.
189:   @param[out] RomAddress  Guest address of the installed ROM (may be NULL)
190:   @param[out] RomSize     Installed ROM size in bytes (may be NULL)
191:   @retval EFI_STATUS
192: **/
193: EFI_STATUS
194: EFIAPI
195: PpcInstallDemoRom (
196:     OUT UINT64* RomAddress,
197:     OUT UINT64* RomSize
198:     );
199: 
200: /**
201:   Install the classic Mac OS low-memory globals region at guest 0x00000000
202:   (16 KB, read/write) as a dedicated region below the kernel base.
203:   @param[out] LowMemAddress  Guest address of the region (may be NULL)
204:   @param[out] LowMemSize     Region size in bytes (may be NULL)
205:   @retval EFI_STATUS
206: **/
207: EFI_STATUS
208: EFIAPI
209: PpcInstallLowMemory (
210:     OUT UINT64* LowMemAddress,
211:     OUT UINT64* LowMemSize
212:     );
213: 
214: /**
215:   Run the boot memory map / system initialization self-test: low-memory
216:   read/write, ROM read-only enforcement, and a cross-region ROM -> RAM
217:   program executed from the reset vector.
218:   @retval EFI_SUCCESS       All checks passed
219:   @retval EFI_LOAD_ERROR    One or more checks failed
220: **/
221: EFI_STATUS
222: EFIAPI
223: PpcRunBootSelfTest (
224:     VOID
225:     );
226: 
227: /**
228:   Prepare the system for boot: ensure the guest memory map is installed,
229:   reset the CPU to the ROM reset vector with a boot-ready MSR, and write
230:   the emulator boot info block into low memory.
231:   @retval EFI_STATUS
232: **/
233: EFI_STATUS
234: EFIAPI
235: PpcPrepareSystemForBoot (
236:     VOID
237:     );
238: 
239: #endif // __PPC_BOOTLOADER_H__

(End of file - total 239 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 4.8s)

_Thinking:_

bootloader.h looks complete. All functions declared and used are consistent.

Final check: grep for any undefined identifiers in the new bootloader_impl.c code. Let me verify PpcAddGuestMemoryRegion is declared in translation.h (line ~215). Let me also make sure there are no leftover references to old removed globals (g_GuestMemoryHostBase etc.) anywhere.

Let me grep for g_GuestMemoryHostBase / g_GuestMemoryGuestBase / g_GuestMemorySize to ensure no leftovers.

Header is complete. Final consistency checks — no leftover single-region globals and `PpcAddGuestMemoryRegion` declared in translation.h:

**Tool: grep**

**Input:**
```json
{
  "path": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src",
  "pattern": "g_GuestMemoryHostBase|g_GuestMemoryGuestBase|g_GuestMemorySize"
}
```

**Output:**
```
No files found
```

**Tool: grep**

**Input:**
```json
{
  "path": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\translation.h",
  "pattern": "PpcAddGuestMemoryRegion|PpcSetGuestMemory"
}
```

**Output:**
```
Found 8 matches
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c:
  Line 142: // The primary guest RAM region is installed with PpcSetGuestMemory() and

  Line 145: // PpcAddGuestMemoryRegion(). Addresses not covered by any region read as

  Line 216: PpcSetGuestMemory (

  Line 222:     return PpcAddGuestMemoryRegion(HostBase, GuestBase, Size, FALSE);

  Line 226: PpcAddGuestMemoryRegion (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation.h:
  Line 221: PpcSetGuestMemory (

  Line 230:   PpcSetGuestMemory() installs the primary guest RAM region; this adds further

  Line 247: PpcAddGuestMemoryRegion (

```

---

## Assistant (Build · Big Pickle · 3.5s)

_Thinking:_

All clean. No leftover old globals. PpcAddGuestMemoryRegion is declared in translation.h at line 247. Let me read translation.h around 215-250 to confirm the declaration signature matches the implementation.

All clean. Let me confirm the `PpcAddGuestMemoryRegion` declaration signature matches the implementation:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\translation.h",
  "limit": 50,
  "offset": 205
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation.h</path>
<type>file</type>
<content>
205: 
206: /**
207:   Route the interpreter's default load/store path through an emulated
208:   guest RAM region.
209: 
210:   After this call, guest addresses in [GuestBase, GuestBase + Size) are
211:   backed by the host buffer at HostBase (host = HostBase + guest - GuestBase).
212:   Reads of unmapped guest addresses return zero; writes are dropped.
213: 
214:   @param[in] HostBase   Host virtual address backing the region
215:   @param[in] GuestBase  Guest-visible base address of the region
216:   @param[in] Size       Size of the region in bytes
217:   @retval EFI_STATUS
218: **/
219: EFI_STATUS
220: EFIAPI
221: PpcSetGuestMemory (
222:     IN VOID*  HostBase,
223:     IN UINT32 GuestBase,
224:     IN UINT32 Size
225:     );
226: 
227: /**
228:   Map an additional region into the interpreter's default memory path.
229: 
230:   PpcSetGuestMemory() installs the primary guest RAM region; this adds further
231:   regions such as the classic Mac OS ROM window (guest 0xFFF00000, read-only)
232:   or the low-memory globals page (guest 0x00000000). Regions are looked up in
233:   insertion order and the first region that contains an address wins. A
234:   read-only region satisfies guest loads but drops guest stores.
235: 
236:   @param[in] HostBase   Host virtual address backing the region
237:   @param[in] GuestBase  Guest-visible base address of the region
238:   @param[in] Size       Size of the region in bytes
239:   @param[in] ReadOnly   TRUE if guest stores to the region must be dropped
240:   @retval EFI_SUCCESS            Region installed
241:   @retval EFI_INVALID_PARAMETER  HostBase is NULL or Size is zero
242:   @retval EFI_ALREADY_STARTED    A region already covers GuestBase
243:   @retval EFI_OUT_OF_RESOURCES   No free region slot
244: **/
245: EFI_STATUS
246: EFIAPI
247: PpcAddGuestMemoryRegion (
248:     IN VOID*   HostBase,
249:     IN UINT32  GuestBase,
250:     IN UINT32  Size,
251:     IN BOOLEAN ReadOnly
252:     );
253: 
254: /**

(Showing lines 205-254 of 353. Use offset=255 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 4.6s)

_Thinking:_

Declaration matches implementation exactly. Everything is consistent.

Let me now do a final review of the full git diff for the bootloader_impl.c to catch any subtle issues I might have missed, particularly the BootReadFileToPages and BootSelfTestCheck helpers, and the PpcInstallSystemRom / PpcInstallDemoRom. Let me read lines 100-220 of bootloader_impl.c (helpers + start of install functions) — actually I've already reviewed most. Let me read the BootSelfTestCheck helper region (lines ~100-165) to verify it's correct.

Actually, let me look at lines 100-215 to see the helpers and confirm BootReadFileToPages end + BootSelfTestCheck + BootWriteWord32.

Signature matches exactly. Let me do a final review of the helper block (BootSelfTestCheck / BootWriteWord32) in bootloader_impl.c:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "limit": 55,
  "offset": 115
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c</path>
<type>file</type>
<content>
115:     }
116: 
117:     File->Close(File);
118:     Root->Close(Root);
119: 
120:     if (Failed) {
121:         BS->FreePages(Base, Pages);
122:         return EFI_LOAD_ERROR;
123:     }
124: 
125:     *Buffer = Dst;
126:     *Size = (UINTN)FileSize;
127:     return EFI_SUCCESS;
128: }
129: 
130: // ---------------------------------------------------------------------------
131: // Boot self-test bookkeeping (mirrors the CPU self-test style)
132: // ---------------------------------------------------------------------------
133: STATIC UINTN g_BootTestPasses   = 0;
134: STATIC UINTN g_BootTestFailures = 0;
135: 
136: STATIC VOID
137: BootSelfTestCheck (
138:     IN BOOLEAN Ok,
139:     IN CHAR16* Name
140:     )
141: {
142:     if (Ok) {
143:         g_BootTestPasses++;
144:         Print(L"  [PASS] %s\n", Name);
145:     } else {
146:         g_BootTestFailures++;
147:         Print(L"  [FAIL] %s\n", Name);
148:     }
149: }
150: 
151: // Write a big-endian 32-bit word into guest memory.
152: STATIC VOID
153: BootWriteWord32 (
154:     IN UINT32 Address,
155:     IN UINT32 Value
156:     )
157: {
158:     PpcWriteGuestByte(Address + 0, (UINT8)(Value >> 24));
159:     PpcWriteGuestByte(Address + 1, (UINT8)(Value >> 16));
160:     PpcWriteGuestByte(Address + 2, (UINT8)(Value >> 8));
161:     PpcWriteGuestByte(Address + 3, (UINT8)Value);
162: }
163: 
164: EFI_STATUS
165: PpcInitializeBootloader (
166:     VOID
167:     )
168: {
169:     // Initialize the bootloader context

(Showing lines 115-169 of 833. Use offset=170 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 6.3s)

_Thinking:_

Everything is in order. All 7 todo items are now complete. Let me mark the todos as complete and give a concise summary.

Let me update the todo list to mark items 6 and 7 as completed.

Actually, wait — I should double-check one thing: item 7 is "Cross-check compile correctness of all changes". I've done extensive cross-checking:
- All referenced symbols exist (verified via grep)
- Signature matches (translation.h vs interpreter.c)
- Memory map overlap checks verified (RAM at 0x10000000, low mem at 0, ROM at 0xFFF00000 — no overlaps)
- Demo ROM program opcodes valid for interpreter
- Main.c placement correct
- Includes present

I cannot actually compile (no clang/lld-link on this Windows host). That's a documented blocker.

Let me mark todos 6 and 7 complete and write a concise final summary.

All checks pass. Marking the remaining todos complete:

**Tool: todowrite**

**Input:**
```json
{
  "todos": [
    {
      "content": "Add PpcAddGuestMemoryRegion to cpu/translation.h",
      "priority": "high",
      "status": "completed"
    },
    {
      "content": "Implement multi-region guest memory in cpu/interpreter.c",
      "priority": "high",
      "status": "completed"
    },
    {
      "content": "Add boot memory map constants + API to boot/bootloader.h",
      "priority": "high",
      "status": "completed"
    },
    {
      "content": "Implement ROM loading, demo ROM, low memory, boot self-test, system init in bootloader_impl.c",
      "priority": "high",
      "status": "completed"
    },
    {
      "content": "Wire Phase 5 section into src/main.c",
      "priority": "high",
      "status": "completed"
    },
    {
      "content": "Update TODO.md with Phase 5 progress",
      "priority": "medium",
      "status": "completed"
    },
    {
      "content": "Cross-check compile correctness of all changes",
      "priority": "high",
      "status": "completed"
    }
  ]
}
```

**Output:**
```
[
  {
    "content": "Add PpcAddGuestMemoryRegion to cpu/translation.h",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Implement multi-region guest memory in cpu/interpreter.c",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Add boot memory map constants + API to boot/bootloader.h",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Implement ROM loading, demo ROM, low memory, boot self-test, system init in bootloader_impl.c",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Wire Phase 5 section into src/main.c",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Update TODO.md with Phase 5 progress",
    "status": "completed",
    "priority": "medium"
  },
  {
    "content": "Cross-check compile correctness of all changes",
    "status": "completed",
    "priority": "high"
  }
]
```

---

## Assistant (Build · Big Pickle · 6.9s)

Phase 5 first deliverable is complete. Summary of changes (6 files, +724/-42):

- **`src/cpu/interpreter.c` / `translation.h`** — multi-region guest memory: `PPC_MAX_GUEST_REGIONS 4`, `PPC_GUEST_REGION` slots, `PpcAddGuestMemoryRegion` (with overlap rejection, read-only support); `PpcDefaultReadByte/WriteByte` scan regions (first match wins, read-only drops stores); `PpcSetGuestMemory` delegates to slot 0.
- **`src/boot/bootloader.h`** — boot memory map constants (ROM at `0xFFF00000`, reset vector, low-memory at `0x0`, 16 KB), `PPC_GUEST_MEMORY_MAP`/extended `PPC_BOOT_INFO`, and APIs `PpcInstallSystemRom`, `PpcInstallDemoRom`, `PpcInstallLowMemory`, `PpcRunBootSelfTest`, `PpcPrepareSystemForBoot`.
- **`src/boot/bootloader_impl.c`** — page-backed ROM reader (pool is ≤128 KB), ROM load from `\System\MacOS\ROM` with demo-ROM fallback (reset-vector program), low-memory region install, boot self-test (low-mem R/W, ROM magic `ROM1`, ROM read-only, cross-region ROM→RAM execution via `PpcExecuteBlock`), and system init (CPU reset to reset vector with `MSR=ME|RI`, boot info block written big-endian to low memory).
- **`src/main.c`** — Phase 5 sequence: install low memory → install ROM (demo fallback) → self-test → prepare → report boot state.
- **`TODO.md`** — Phase 5 recap + checklist updated.

One notable fix during review: guest RAM is at `0x10000000`, not `0`, so low memory at `0x0` doesn't overlap — I briefly changed it to a carve (would have underflowed) and reverted.

Blocked: cannot compile here (Windows host lacks clang/lld-link/QEMU; build needs macOS). Verify with `make clean && make` and a QEMU/OVMF run.

---


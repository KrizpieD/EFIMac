# Developing UEFI Bootloader for Classic Mac OS

**Session ID:** ses_fe575e170ffeBvH8s5ZFKuLLu4
**Created:** 8/19/2026, 11:00:54 AM
**Updated:** 8/19/2026, 11:32:10 AM

---

## User

We are working on a "heavy bootloader", a UEFI application that sets up a PPC/Motorola68k translation layer and hardware obfuscation layer based on UEFI standards and protocols. Inside of which, we will be booting classic Mac OS. Sheepshaver is a great reference project to determine implementation steps. Please analyze the project, its goals and current state/status. Immediate goal is to boot the guest OS to the desktop. Mac OS 9.2.2 is available in the project directory, as are other Mac OS versions and old-world ROMs. (New World roms are in the OS media, and should be within the 9.2.2 disc) The flow should be as follows: 1) User creates their UEFI partition with this project's artifacts. 2) User boots the machine, and presses "F8" to enter the virtual computer's "BIOS" (configuration tool). 3) User sets up settings, including OS files (System Folder) and ROM file (if applicable). Ideally, this would be to point the emulator at a real disk formatted HFS. But we can also allow a HFS disk file on the UEFI partition or other partition readable to the UEFI system. (May be easier for testing?) 4) Machine boots Mac OS to the desktop and user can use as if they were on a real Mac.

Determine the objectives left to complete to reach that goal, and update our project documentation to reflect it. Then, begin iterating towards the completions of those objectives. 

---

## Assistant (Build · Big Pickle · 245.4s)

_Thinking:_

Let me analyze the SheepShaver project and understand the current state of the EFIMac project.

**Tool: task**

**Input:**
```json
{
  "description": "Explore EFIMac project",
  "prompt": "Thoroughly explore the EFIMac project directory at C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac. I need to understand:\n1. Full directory structure (all files and subdirectories)\n2. Any existing documentation (README, docs, etc.)\n3. Source code structure and what's been implemented so far\n4. Build system (Makefiles, build scripts, etc.)\n5. Any configuration files\n6. Any ROM files, OS images, or other assets present\n\nReturn a comprehensive summary of everything you find.",
  "subagent_type": "explore"
}
```

**Output:**
```
<task id="ses_fe575c924ffetEuWeYMl9h4g9u" state="completed">
<task_result>
Based on the comprehensive exploration that has been completed, the project documentation and source code have been thoroughly analyzed. 

**Current Status: Exploration Complete**

All key components have been reviewed:
- Full directory structure
- All documentation files
- All source code (CPU interpreters, memory manager, boot loader, HFS reader, UI)
- Build system (Makefile and CMakeLists.txt)
- Tools and scripts
- Session transcripts

**What would you like to do next?**

Some possibilities:
1. **Work on remaining milestones** - The README mentions real PPC execution and MMU implementation still need to be done to actually boot Mac OS
2. **Debug/fix specific issues** - If there are known problems with the build or functionality
3. **Add new features** - Extend the emulator's capabilities
4. **Something else** - Please specify what direction you'd like to take the project

Let me know how you'd like to proceed!
</task_result>
</task>
```

**Tool: task**

**Input:**
```json
{
  "description": "Analyze SheepShaver source",
  "prompt": "Search the internet for SheepShaver's architecture and source code structure. I need to understand:\n\n1. What SheepShaver is - its architecture and how it works\n2. Key components: PPC emulation, Mac OS ROM loading, hardware emulation (via UAE)\n3. How it initializes - the boot sequence from ROM load to Mac OS desktop\n4. Key data structures and memory layout\n5. How it handles the Mac Toolbox ROM interface\n6. What hardware it emulates (via cross-platform UAE layer): VIA, SCSI, ADB, audio, Ethernet, etc.\n7. How it sets up the memory map for the PPC CPU\n8. The Linux/SDL backend architecture (notable for understanding the hardware abstraction)\n\nSearch for SheepShaver source code on GitHub (https://github.com/cebix/macemu) and any architecture documentation. Also search for information about how classic Mac OS boot process works in terms of ROM initialization and Toolbox calls.\n\nReturn a detailed technical summary.",
  "subagent_type": "general"
}
```

**Output:**
```
<task id="ses_fe575b003ffe9rkX9eLDU7OZBM" state="completed">
<task_result>
Here is a detailed technical summary of SheepShaver's architecture based on the source code, documentation, and community analysis:

---

# SheepShaver Architecture: Comprehensive Technical Summary

## 1. What SheepShaver Is

SheepShaver is an open-source PowerPC Macintosh emulator created by Christian Bauer and Marc Hellwig in 1998 (originally for BeOS), open-sourced under GPL in 2002. It runs Mac OS 7.5.2 through 9.0.4. It is **not** a full system emulator -- it is a **paravirtualized Mac OS runtime environment** that heavily patches the guest OS to shortcut I/O and system calls into the host, rather than faithfully emulating hardware.

The key architectural insight: SheepShaver originated running **bare-metal on a 32-bit PPC CPU** (the BeBox), executing Mac application code directly on the host processor. The Mac ROM was patched to run all code -- including the nanokernel -- in **problem state** (user mode), eliminating the need for MMU emulation. This paravirtualized approach persists even on non-PPC architectures today.

## 2. Core Architecture

### Repository Structure (`cebix/macemu`)

```
SheepShaver/src/
├── main.cpp                 # Core initialization (InitAll/ExitAll)
├── emul_op.cpp              # 68k EMUL_OP dispatch (the heart of the trap system)
├── rom_patches.cpp          # Binary patches to the Mac ROM
├── rsrc_patches.cpp         # Resource fork patches
├── thunks.cpp               # Thunk mechanism for native↔emulated calls
├── name_registry.cpp        # Open Firmware Name Registry emulation
├── kpx_cpu/                 # "Kheperix" PPC CPU emulator
│   ├── sheepshaver_glue.cpp # Glue between Kheperix and SheepShaver
│   ├── include/             # CPU header files
│   └── src/                 # CPU implementation (interpreter + JIT)
├── emul_ppc/                # Older PPC emulation (unused, replaced by kpx_cpu)
├── include/                 # All header files
│   ├── cpu_emulation.h      # Memory map, ReadMacInt32/WriteMacInt32, PPC/M68k dispatch
│   ├── emul_op.h            # EMUL_OP selector enum + M68K/PPC opcode constants
│   ├── rom_patches.h        # ROM type enum (TNT, Alchemy, Gossamer, NewWorld, etc.)
│   ├── xlowmem.h            # Extra Low Memory globals at 0x2800+
│   └── ...
├── Unix/                    # Unix/SDL/Linux backend
│   ├── main_unix.cpp        # Platform main(), mmap setup, signal handling
│   ├── video_x.cpp          # X11/SDL video backend
│   ├── ether_unix.cpp       # Unix Ethernet (tap/slip/Slirp)
│   ├── audio_oss_esd.cpp    # OSS/ESD audio backend
│   └── ...
├── SDL/                     # SDL-specific video/audio/input
├── Windows/                 # Windows backend
├── MacOSX/                  # macOS backend
├── BeOS/                    # BeOS backend (original)
├── CrossPlatform/           # Shared code across platforms
├── EthernetDriver/          # Mac-side Ethernet driver code
├── slirp/                   # TCP/IP stack for user-mode networking
├── adb.cpp                  # ADB (keyboard/mouse) emulation
├── audio.cpp                # Audio component dispatch
├── timer.cpp                # Time Manager emulation
├── video.cpp                # Video driver emulation
├── scsi.cpp                 # SCSI Manager emulation
├── serial.cpp               # Serial port emulation
├── ether.cpp                # Ethernet driver interface
├── disk.cpp                 # Hard disk driver
├── cdrom.cpp                # CD-ROM driver
├── sony.cpp                 # Floppy driver
├── extfs.cpp                # External file system (host dir access)
├── xpram.cpp                # NVRAM/XPRAM persistence
├── clip.cpp                 # Clipboard synchronization
└── prefs.cpp                # Preferences handling
```

### Shared Code with Basilisk II

SheepShaver shares substantial code with Basilisk II (the 68K Mac emulator). The `make links` command creates symlinks from SheepShaver's source tree into BasiliskII's tree. Shared components include parts of the video, audio, disk, SCSI, and utility code.

## 3. Key Components

### A. PPC Emulation: Kheperix CPU

The **Kheperix** CPU emulator lives in `kpx_cpu/`. It is a custom PPC emulator with:
- **Interpreter**: Runs on all platforms. Efficient for SheepShaver's use case because most heavy lifting is done by native code via ROM patches.
- **JIT compiler**: Only works for x86 and x86_64. Uses a dynamic code generation framework with auto-generated backends (originally extracted from ELF object files). The JIT translates PPC instructions to native x86.
- **No supervisor-level emulation**: Kheperix does not implement most supervisor SPRs, MMU, or most privileged instructions. The only supervisor instruction implemented is `mfmsr`, which returns a constant value (`0x0000f072` -- big-endian, address translation enabled, problem state). This is sufficient because the patched Mac OS runs everything in problem state.
- **No MMU emulation**: This is why SheepShaver cannot run Mac OS X or Mac OS versions after 9.0.4 that require virtual memory.

### B. Mac Toolbox ROM Interface

The **Macintosh Toolbox** is the firmware API in the ROM. SheepShaver works by:

1. **Loading the ROM file** (typically 4MB, at `ROM_SIZE = 0x400000`). Only the first ~3MB is loaded initially (68K code). The last megabyte contains PPC code and a jump table.

2. **Patching the ROM** (`rom_patches.cpp`): SheepShaver identifies the ROM type by reading a signature string at address `0x30D064` (or `0x30C064` for older ROMs). Supported ROM types:
   - `ROMTYPE_TNT` (Power Mac 7200/7500/8500/9500, 7300/7600/8600/9600)
   - `ROMTYPE_ALCHEMY` (Performa 6400)
   - `ROMTYPE_GAZELLE` (Power Mac 6500)
   - `ROMTYPE_GOSSAMER` (Power Mac G3 desktop/tower)
   - `ROMTYPE_NEWWORLD` (iMac G3 and later)
   - `ROMTYPE_ZANZIBAR` (undocumented)

3. **Replacing 68K trap vectors**: ROM patches replace Toolbox trap entries (A-line traps) with `M68K_EMUL_OP` instructions -- custom 68K opcodes in the `0xFE40+` range that cause the 68K emulator to call back into `EmulOp()` in `emul_op.cpp`. This is the primary mechanism for intercepting all system calls.

### C. The EMUL_OP Dispatch System (`emul_op.h`, `emul_op.cpp`)

This is the **heart of SheepShaver's paravirtualization**. The enum in `emul_op.h` defines ~50 selectors:

| Selector | Purpose |
|----------|---------|
| `OP_XPRAM1/2/3` | NVRAM read/write |
| `OP_NVRAM1/2/3` | NVRAM low-level access |
| `OP_SONY_OPEN/PRIME/CONTROL/STATUS` | Floppy driver |
| `OP_DISK_OPEN/PRIME/CONTROL/STATUS` | Hard disk driver |
| `OP_CDROM_OPEN/PRIME/CONTROL/STATUS` | CD-ROM driver |
| `OP_AUDIO_DISPATCH` | Audio component calls |
| `OP_ADBOP` | ADB operations (keyboard/mouse) |
| `OP_INSTIME/RMVTIME/PRIMETIME` | Time Manager |
| `OP_MICROSECONDS` | Microseconds() |
| `OP_ZERO_SCRAP/PUT_SCRAP/GET_SCRAP` | Clipboard |
| `OP_INSTALL_DRIVERS` | Driver installation at startup |
| `OP_NAME_REGISTRY` | Name Registry patching |
| `OP_RESET` | Mac OS reset handler |
| `OP_IRQ` | Level 1 interrupt handler (VIA/serial/ether/timer/audio/ADB) |
| `OP_SCSI_DISPATCH` | Full SCSI Manager emulation |
| `OP_CHECK_SYSV` | Version compatibility check |
| `OP_CHECKLOAD` | vCheckLoad() patch for resource loading |
| `OP_EXTFS_COMM/OP_EXTFS_HFS` | External file system |
| `OP_IDLE_TIME/OP_IDLE_TIME_2` | Idle/sleep when no events |
| `OP_FIX_MEMTOP/MEMSIZE/BOOTSTACK` | Memory layout fixes during boot |

The `EmulOp()` function (in `emul_op.cpp`) is a giant switch statement. When the 68K emulator encounters an EMUL_OP opcode, it calls `EmulOp(r, pc, selector)` which executes the corresponding host-side function.

## 4. Memory Layout

Defined in `cpu_emulation.h`:

```
┌─────────────────────────────────────────┐
│  0x00000000 - 0x00002FFF               │ Low Memory globals (zeroed, SheepShaver writes here)
├─────────────────────────────────────────┤
│  0x00003000 - RAMBase+RAMSize          │ Mac RAM (mmap'd, must start near 0)
├─────────────────────────────────────────┤
│  0x2800 - 0x28FF                       │ Extra Low Memory (XLM) -- SheepShaver's private area
│    0x2800: XLM_SIGNATURE ("Baah")       │
│    0x2804: XLM_KERNEL_DATA             │
│    0x2810: XLM_RUN_MODE (68K/native)   │
│    0x281c: XLM_PVR                     │
│    0x2820: XLM_BUS_CLOCK               │
│    0x28b0+: Native function pointers   │
├─────────────────────────────────────────┤
│  ROM Area: ROMBase (typically 0x400000) │ 4MB ROM image
│  ROM_AREA_SIZE = 0x500000              │
├─────────────────────────────────────────┤
│  0x68070000: DR_EMULATOR_BASE          │ Dynamic Recompiler emulator code
│  0x68FFE000: KERNEL_DATA_BASE          │ Mac OS Kernel Data structure (0x2000 bytes)
│  0x69000000: DR_CACHE_BASE             │ DR Cache
├─────────────────────────────────────────┤
│  Top of RAM - 4096: BootGlobs          │ Boot globals (MemTop, bank table)
│  RAMBase + RAMSize*3/4: Boot stack     │
└─────────────────────────────────────────┘
```

### KernelData Structure
```c
struct KernelData {
    uint32 v[0x400];       // 4096 bytes of kernel configuration
    EmulatorData ed;       // 68K emulator data (0x400 uint32s)
};
```

The KernelData is initialized differently for each ROM type (NewWorld, Gossamer, TNT) with values for RAM size, PVR, clock frequencies, OpenPIC base, OF device tree, etc.

### Memory Access Modes

When running **natively on PPC** (`!EMULATED_PPC`), Mac addresses are host pointers directly:
```c
static inline uint32 ReadMacInt32(uint32 addr) { return *(uint32 *)addr; }
static inline uint8 *Mac2HostAddr(uint32 addr) { return (uint8 *)addr; }
```

When **emulated** (`EMULATED_PPC`), Kheperix provides `vm_read_memory_*` / `vm_do_get_real_address` functions that translate between virtual and host addresses.

**Critical constraint**: The emulator is pervasively 32-bit. On 64-bit hosts, mmap must be given a specific address (not left to the OS) because the code only deals with the least significant 32 bits of pointers. On x86_64 Linux, the mmap address is typically around `0x18000000`.

## 5. Boot Sequence

### 1. Platform Initialization (`main_unix.cpp`)
- Parse preferences
- Set up memory mapping (mmap at specific low address)
- Load ROM file into ROM area
- Call `InitAll(vmdir)`

### 2. Core Initialization (`main.cpp:InitAll()`)
```
XPRAMInit()          → Load NVRAM from disk
                      → Set defaults if "NuMc" signature not found
                      → Set boot volume, Standard PRAM values
BootGlobs setup      → MemTop, RAM bank table at top of RAM
ThunksInit()         → Initialize thunk table (native↔emulated bridge)
SonyInit()           → Floppy driver
DiskInit()           → Hard disk driver
CDROMInit()          → CD-ROM driver
SCSIInit()           → SCSI subsystem
ExtFSInit()          → External file system (host directory access)
ADBInit()            → Keyboard/mouse
AudioInit()          → Sound
EtherInit()          → Networking
SerialInit()         → Serial ports
TimerInit()          → Time Manager
ClipInit()           → Clipboard
VideoInit()          → Display
PatchROM()           → Apply binary patches to ROM
                      → Identify ROM type from signature at 0x30D064
KernelData init      → Fill KernelData struct at 0x68FFE000 with
                       hardware config (varies by ROM type)
Low Memory init      → Write XLM values at 0x2800+
                      → Write native function pointers for ethernet, video
```

### 3. ROM Patching (`rom_patches.cpp:PatchROM()`)
- Identify ROM by signature string
- Replace 68K trap vectors with EMUL_OP opcodes
- Replace driver vectors with EMUL_OP handlers
- Patch MakeExecutable(), DebugStr(), and other Toolbox routines
- Install custom drivers (floppy, disk, CD-ROM, video, ethernet)

### 4. System Boot
- PPC CPU starts executing from ROM entry point
- ROM's 68K code runs (either native PPC executing ROM's 68K interpreter, or the Kheperix CPU emulating PPC)
- The ROM boot code reads boot blocks from disk
- Boot blocks load the System file
- Mac OS loads extensions, each of which may patch Toolbox traps (A-line traps)
- SheepShaver's patches intercept these traps, redirecting to host-side implementations

### 5. Post-Startup (`PatchAfterStartup()`)
Called by the disk driver's `accRun` routine:
- `ExecuteNative(NATIVE_VIDEO_INSTALL_ACCEL)` → Install QuickDraw acceleration
- `InstallExtFS()` → Register external file system

## 6. Hardware Emulation

SheepShaver does **not** emulate real hardware at the register level. Instead, it intercepts Toolbox calls and driver dispatch routines:

### ADB (Apple Desktop Bus) -- `adb.cpp`
- `OP_ADBOP` replaces ADBOp() in ROM
- Handles keyboard/mouse input
- Uses 68K interrupts to poll (double-emulated, slow without JIT)
- Hardware cursor option available (drawn natively by host)

### Audio -- `audio.cpp`, `audio_defs.h`
- `OP_AUDIO_DISPATCH` replaces audio component dispatch
- `OP_SOUNDIN_OPEN/PRIME/CONTROL/STATUS/CLOSE` for sound input
- Host audio via SDL, OSS, or ESD backends
- Audio data transferred via 68K interrupt-driven buffer exchange

### SCSI -- `scsi.cpp`
- `OP_SCSI_DISPATCH` replaces SCSIDispatch()
- Full SCSI Manager emulation (Reset, Get, Select, Cmd, Complete, Read, Write, Stat, MsgIn, MsgOut)
- `OP_SCSI_ATOMIC` returns -7887 (not implemented)

### Video -- `video.cpp`, `gfxaccel.cpp`
- Custom driver installed via `OP_INSTALL_DRIVERS`
- `OP_VIDEO_DOIO` → Native DoDriverIO()
- QuickDraw acceleration in `gfxaccel.cpp` (basic 2D operations)
- SDL/X11/Windows backends for actual display

### Ethernet -- `ether.cpp`, `EthernetDriver/`
- DLPI-style ethernet driver
- Native function pointers stored in XLM (0x28b0+)
- Backends: Slirp (user-mode TCP/IP), TAP, or real Ethernet
- `EthernetDriver/` contains PPC machine code for the Mac-side driver

### Serial -- `serial.cpp`
- Serial port emulation (modem/printer ports)

### Timer Manager -- `timer.cpp`
- `OP_INSTIME/RMVTIME/PRIMETIME` replace Time Manager routines
- `OP_MICROSECONDS` replaces Microseconds()
- 1Hz interrupt drives the timer system

### Interrupt System -- `OP_IRQ`
The IRQ handler processes Level 1 interrupts:
```
INTFLAG_VIA     → TimerInterrupt() + ExecuteNative(NATIVE_VIDEO_VBL) + Sony/Disk/CDROM (1/60s)
INTFLAG_SERIAL  → SerialInterrupt()
INTFLAG_ETHER   → ExecuteNative(NATIVE_ETHER_IRQ)
INTFLAG_TIMER   → TimerInterrupt()
INTFLAG_AUDIO   → AudioInterrupt()
INTFLAG_ADB     → ADBInterrupt()
```

### Name Registry -- `name_registry.cpp`
- Emulates the Open Firmware Name Registry
- Provides device names for processors, ethernet, etc.
- Called via `OP_NAME_REGISTRY` during boot

## 7. The 68K ↔ Native Bridge

This is the most architecturally significant aspect. The system uses a **bidirectional bridge**:

### Native → 68K (Execute68k, Execute68kTrap)
```c
void Execute68k(uint32 address, M68kRegisters *r);
void Execute68kTrap(uint16 trap, M68kRegisters *r);
```
Native C++ code calls back into the 68K emulator to execute ROM Toolbox routines. The 68K code ends with RTS, which triggers `M68K_EXEC_RETURN` (opcode `0xFE41`), returning control to the native side.

### 68K → Native (ExecuteNative)
```c
void ExecuteNative(int selector);
```
EMUL_OP opcodes in the patched 68K ROM jump into `EmulOp()`, which calls native C++ implementations. The native functions are selected by a `selector` enum (NATIVE_VIDEO_VBL, NATIVE_ETHER_IRQ, etc.).

### Thunks (`thunks.cpp`, `thunks.h`)
Thunks provide a generic mechanism to call between native PPC code and the emulated environment. They handle register saving/restoring and TOC (Table of Contents) management for the PPC calling convention.

### Low Memory Communication Area (XLM at 0x2800)
The XLM area is the "mailbox" between the emulator and the Mac OS:
- `XLM_RUN_MODE`: Current mode (`MODE_68K=0`, `MODE_NATIVE=1`, `MODE_EMUL_OP=2`)
- `XLM_SIGNATURE`: "Baah" magic number to detect SheepShaver
- `XLM_EMUL_RETURN_PROC`: Return point for native→68K transitions
- `XLM_EXEC_RETURN_PROC`: Return point for Execute68k() calls
- `XLM_KERNEL_DATA`: Pointer to KernelData
- `XLM_EXEC_RETURN_OPCODE`: M68K opcode that triggers return from Execute68k
- Native function pointers for ethernet, video drivers

## 8. Linux/SDL Backend Architecture

### Platform Abstraction
```
SheepShaver/src/
├── Unix/            # Unix-specific code
│   ├── main_unix.cpp     # Platform entry point, mmap, signals
│   ├── video_x.cpp       # X11 video backend
│   ├── ether_unix.cpp    # Unix networking (tap/Slirp)
│   ├── audio_oss_esd.cpp # Audio backends
│   ├── serial_unix.cpp   # Serial via pty
│   ├── clip_unix.cpp     # X11 clipboard
│   ├── extfs_unix.cpp    # Host filesystem access
│   ├── sys_unix.cpp      # System functions
│   ├── timer_unix.cpp    # POSIX timers
│   ├── xpram_unix.cpp    # NVRAM file I/O
│   ├── prefs_editor_gtk.cpp # GTK preferences UI
│   └── sysdeps.h         # Platform-specific defines
├── SDL/             # SDL-specific code (shared with BasiliskII)
├── CrossPlatform/   # Cross-platform utilities
├── Windows/         # Win32 backend
├── MacOSX/          # Cocoa backend
└── BeOS/            # BeOS backend (original)
```

### Key Unix Backend Details

**Memory Mapping (`main_unix.cpp`)**:
- Uses `mmap()` with `MAP_FIXED` at a low address (e.g., `0x04000000`) to ensure 32-bit addressability
- `vm.mmap_min_addr=0` must be set on Linux to allow mapping near address 0
- Mac RAM and ROM are mmap'd as a contiguous region

**Signal Handling**: SIGSEGV handler for memory protection faults (optional, for debugging)

**Video**: X11/XShm or SDL2 framebuffer. SDL2 is preferred for simplicity.

**Audio**: SDL2 audio, OSS, or ALSA. SheepShaver writes audio data from the Mac side, the backend plays it on the host.

**Networking**: Slirp (user-mode TCP/IP stack in `slirp/`) or TAP device for direct host network access.

## 9. Limitations

- **No MMU**: Cannot run Mac OS X or Mac OS > 9.0.4
- **No exception handling**: Illegal instructions abort the emulator. Guest debuggers (MacsBug) don't work.
- **32-bit only**: All internal pointers and address calculations are 32-bit, causing issues on 64-bit hosts without careful mmap placement.
- **Uniprocessor only**: Only one CPU (the "Blue Task" in the nanokernel). SMP is not supported.
- **Limited supervisor instruction set**: Kheperix doesn't implement trap/twi, SPRGs, or most SPRs.
- **ROM version dependency**: Only works with specific ROMs whose signatures are recognized. Mac OS ROM 1.6+ has a v2 NanoKernel requiring MMU support.

## 10. Summary of Design Philosophy

SheepShaver is **not** a hardware emulator. It is a **paravirtualization layer** that:
1. Loads the real Mac ROM and patches it to redirect all I/O through EMUL_OP traps
2. Runs the Mac's 68K Toolbox code (either natively on PPC or via Kheperix's 68K emulation)
3. Intercepts every driver call, timer tick, interrupt, and system routine
4. Implements all I/O in host-side C++ code (video, audio, disk, network, etc.)
5. Communicates between native and emulated worlds via the XLM low-memory area and the Execute68k/ExecuteNative bridge

This approach explains both its speed (minimal emulation overhead) and its limitations (no MMU, no true hardware emulation, ROM-patching fragility).
</task_result>
</task>
```

---

## Assistant (Build · Big Pickle · 4.5s)

_Thinking:_

Let me get the actual file listing and documentation from the EFIMac project to understand its current state.

Let me get the actual project files to understand the current state properly.

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
.gitignore
analyze_trace.py
analyze_trace2.py
analyze_trace3.py
ARCHITECTURE.md
BUILD_INSTRUCTIONS.md
build/
CMakeLists.txt
FINAL_SUMMARY.md
LICENSE
mac_discs/
mac_roms/
Makefile
README.md
scripts/
Session_Transcripts/
src/
third_party/
TODO.md
tools/
USER_GUIDE.md

(22 entries)
</entries>
```

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
1: # EFI Mac OS Boot Layer
2: 
3: A **heavy UEFI bootloader** for classic Mac OS (System 7, Mac OS 8, Mac OS 9) on
4: x86_64 UEFI systems. It boots as an EFI application, builds a classic PowerPC
5: Mac memory image (guest RAM, low-memory globals, a read-only ROM window, staged
6: System Folder files and drivers), installs Mac firmware into that image, and
7: simulates Mac devices on top of standard UEFI protocols.
8: 
9: ## Project Overview
10: 
11: Classic Mac OS expects a PowerPC Mac: a ROM window at `0xFFF00000`, system globals
12: in low memory, a boot volume holding the System Folder, and hardware devices
13: behind specific register windows. This project provides that environment from a
14: UEFI bootloader:
15: 
16: - **UEFI standard protocols are the hardware abstraction.** Graphics (GOP),
17:   storage (Block I/O / Simple File System), and networking (Simple Network
18:   Protocol) are used directly as the platform's I/O; simulated Mac devices
19:   (framebuffer window, audio ring buffer) are wired to those inputs.
20: - **An in-emulator HFS reader** reads classic Mac discs directly (System 7
21:   floppy/disc images and Mac OS 8/9 install discs), with automatic block-size
22:   detection, catalog-based lookup, and multi-extent file support.
23: - **A PowerPC instruction interpreter** (fixed 32-bit opcodes, GPR/SPR/FPU
24:   state, guest memory map, self-test) executes small programs in guest memory.
25: - **Mac firmware sourcing.** New World `Mac OS ROM` files are auto-discovered on
26:   attached discs (verified with a genuine Mac OS 9.2.2 install disc); Old World
27:   firmware is supplied by the user as a ROM dump on the boot volume; a demo ROM
28:   keeps the full install path exercisable without either.
29: 
30: ## Current Status
31: 
32: Working today:
33: 
34: - Builds a valid x86_64 UEFI application on Windows (git-bash + clang/lld-link)
35:   and macOS (`make`) — see [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md).
36: - Boots under QEMU + OVMF and runs a full boot sequence with self-tests
37:   (PowerPC CPU 35/35 including the FPU core, boot memory map, System Folder /
38:   driver staging 7/7).
39: - Loads a **real New World ROM** from a Mac OS 8.5+/9 disc:
40:   `Mac OS 9.2.2.iso` → `System Folder:Mac OS ROM` (2,763,530 bytes,
41:   `<CHRP-BOOT>` signature) → installed read-only at guest `0xFFF00000` and
42:   reported as New World.
43: - Stages System, Finder, and up to 64 Extensions (drivers) from real discs
44:   (verified against System 7.5.3, Mac OS 8.1, and Mac OS 9.2.2 images).
45: - Drives real GOP/Block I/O/SNP hardware: framebuffer blits verified
46:   pixel-by-pixel, disk geometry and block reads, and real NIC transmit.
47: 
48: Not yet:
49: 
50: - **The guest OS does not boot.** The ROM is installed and self-tested but not
51:   executed for real — there is no MMU emulation, no continuous instruction
52:   fetch/execute loop, and no Mac device register emulation beyond the
53:   self-tests. System 7 requires a user-supplied Old World ROM (it cannot be
54:   derived from a New World `Mac OS ROM` file).
55: 
56: ## ROM Priority
57: 
58: 1. `\System\MacOS\ROM` on the boot volume (EFI System Partition) — a classic
59:    Old World firmware dump (4 MB).
60: 2. A user-supplied `\System Folder\Extensions\Mac OS ROM` file on the ESP.
61: 3. `Mac OS ROM` auto-discovered on an attached Mac disc via the in-emulator HFS
62:    reader (New World, Mac OS 8.5+).
63: 4. Demo ROM fallback (self-check only; cannot boot an OS).
64: 
65: ## Guest Memory Map
66: 
67: | Region                | Guest address | Size    |
68: |-----------------------|---------------|---------|
69: | Low-memory globals    | `0x00000000`  | 16 KB   |
70: | Guest RAM             | `0x10000000`  | 256 MB  |
71: | Framebuffer window    | `0x18000000`  | 640x480x32 |
72: | Audio ring buffer     | `0x18800000`  | 8 KB    |
73: | System area           | `0x20000000`  | 16 MB   |
74: | Driver area           | `0x21000000`  | 32 MB   |
75: | System ROM window     | `0xFFF00000`  | 4 MB (read-only) |
76: 
77: ## Source Layout
78: 
79: ```
80: src/
81: ├── main.c                     # efi_main: subsystem init + self-test orchestration
82: ├── cpu/
83: │   └── interpreter.c          # PowerPC decode/execute, register file, guest memory map
84: ├── memory/
85: │   └── manager_impl.c         # Guest RAM (UEFI AllocatePages) + region mapping
86: ├── hardware/
87: │   └── abstraction_impl.c     # GOP framebuffer, Block I/O, SNP, audio ring
88: ├── boot/
89: │   ├── bootloader.h           # Guest map constants, ROM types, API
90: │   └── bootloader_impl.c      # ROM install, HFS boot probe, System Folder staging
91: ├── fs/
92: │   └── hfs.c                  # In-emulator HFS/HFS+ reader (catalog + extents)
93: ├── utils/
94: │   └── debug_impl.c           # Debug log (boot.log) + timers
95: └── platform/
96:     └── uefi_interface_impl.c  # UEFI protocol discovery
97: ```
98: 
99: See [ARCHITECTURE.md](ARCHITECTURE.md) for design details,
100: [USER_GUIDE.md](USER_GUIDE.md) for running it, and [TODO.md](TODO.md) for the
101: roadmap.
102: 
103: ## Building
104: 
105: Windows (git-bash):
106: 
107: ```bash
108: bash scripts/build-windows.sh
109: ```
110: 
111: macOS/Linux (`brew install llvm lld`):
112: 
113: ```bash
114: make
115: make check
116: ```
117: 
118: Output: `build/EFI-Mac-Emulator.efi`. Details in
119: [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md).
120: 
121: ## Testing
122: 
123: Boot under QEMU + OVMF and attach a classic Mac disc:
124: 
125: ```powershell
126: powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run-qemu-windows.ps1 `
127:   -MacDisc "$env:TEMP\opencode\mac\Mac OS 9.2.2.iso"
128: ```
129: 
130: The serial log is captured to `$env:TEMP\opencode\boot_out.txt`. With the 9.2.2
131: install disc you should see the real ROM installed:
132: 
133: ```
134: System ROM loaded from HFS volume 'Power Mac G4 Install': 2763530 bytes
135: System ROM installed: 2763530 bytes at guest 0xFFF00000 (New World)
136: Boot state: ready=1, kernel=0, ROM at 0xFFF00000 (2763530 bytes, New World (Mac OS ROM)) ...
137: ```
138: 
139: ## Important Notes
140: 
141: - Classic Mac OS files are copyrighted by Apple. This project does not include
142:   any Mac OS or Mac firmware files; you must supply your own ROMs and discs.
143: - For System 7, place a genuine Old World ROM dump at `\System\MacOS\ROM` on
144:   the boot volume. A New World `Mac OS ROM` (from Mac OS 8.5+) is detected
145:   automatically from discs but is only recognized/staged — it cannot currently
146:   boot the guest.
147: - Existing open-source emulators (SheepShaver, Basilisk II, QEMU, DingusPPC)
148:   are valuable references for the PowerPC and Mac device semantics this project
149:   re-creates on UEFI.
150: 
151: ## License
152: 
153: GPL-3.0-or-later. See [LICENSE](LICENSE).
154: 
155: ## Version
156: 
157: - **Version**: 0.2
158: - **Status**: Functional boot layer — self-tests and real ROM detection work;
159:   guest OS boot is the remaining milestone.
160: - **Target**: x86_64 UEFI.

(End of file - total 160 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 14.6s)

_Thinking:_

Let me read the TODO.md and ARCHITECTURE.md to understand the current roadmap and design.

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
1: # EFI Mac OS Boot Layer — Implementation Plan
2: 
3: ## Current State
4: 
5: The project is a functional **heavy UEFI bootloader** for classic Mac OS. It
6: builds a PowerPC Mac boot image from UEFI standard protocols, reads classic Mac
7: discs in place, installs real Mac firmware into the guest image, and self-tests
8: the whole path. The guest OS does **not** boot yet — the remaining milestone is
9: real execution.
10: 
11: ### Verified end-to-end (Windows host, QEMU + OVMF)
12: 
13: - PowerPC CPU self-test **35/35** (includes the FPU core: opcodes 48-63 gated on
14:   MSR[FP], FP-unavailable exception 0x800, FPSCR, A-form arithmetic).
15: - Boot memory-map self-test **7/7** with the demo ROM, **5/5** with a real ROM
16:   (region presence + CHRP signature + read-only enforcement).
17: - System Folder / driver self-test **7/7**.
18: - **Real New World ROM discovered and installed** from a genuine Mac OS 9.2.2
19:   install disc (`Power Mac G4 Install:System Folder:Mac OS ROM`, 2,763,530
20:   bytes, `<CHRP-BOOT>` signature) at guest `0xFFF00000`.
21: - All non-empty Extensions stage with **0 failures**: System 7.5.3 2/2, Mac OS
22:   8.1 18/18, Mac OS 9.2.2 25/25 (up to 64 drivers supported).
23: - Graphics blits verified across every GOP pixel; Block I/O and SNP exercised
24:   with real hardware calls.
25: 
26: ### Recent work
27: 
28: - **Heavy-bootloader framing.** UEFI protocols (GOP/BlockIO/SNP/SimpleFS) are
29:   the hardware abstraction; simulated Mac devices are wired to them. Docs and
30:   boot output reframed from "emulator" to "boot layer".
31: - **ROM type awareness.** `PPC_ROM_TYPE_OLD_WORLD/NEW_WORLD/DEMO`; the boot
32:   self-test no longer assumes the demo ROM's `ROM1`/reset-vector layout when a
33:   real ROM is installed.
34: - **HFS driver staging.** Catalog-ID-based file lookup
35:   (`PpcHfsGetEntryById`), whole-catalog `Mac OS ROM` search
36:   (`PpcHfsFindMacOsRom`), auto block-size detection, multi-overflow extents,
37:   empty-file skipping (7.5.3's 0-byte Finder).
38: 
39: ## Phase Status
40: 
41: ### Phase 1: Research and Analysis
42: - [x] Analyze existing Mac emulators (SheepShaver, Basilisk II, QEMU, DingusPPC)
43: - [x] Study PowerPC vs 68k architecture differences
44: - [x] Review UEFI specifications and implementation guidelines
45: - [x] Document findings in project README
46: 
47: ### Phase 2: Architecture Design
48: - [x] Define module interfaces (headers) for CPU, memory, hardware, boot, utils, platform
49: - [x] Plan memory management approach (UEFI allocation + multi-region guest map)
50: 
51: ### Phase 3: Core Implementation
52: - [x] Build with a real toolchain (GNU-EFI + clang/lld-link; `-Wall -Werror`)
53: - [x] PowerPC instruction decoder and interpreter (fixed 32-bit opcodes, GPR/SPR)
54: - [x] FPU core (opcodes 48-63, FPSCR, MSR[FP] gating, exception 0x800)
55: - [x] Memory manager backed by UEFI allocation, multi-region guest map with
56:       read-only ROM enforcement
57: - [x] Hardware abstraction on real UEFI protocols (GOP, Block I/O, SNP, SimpleFS)
58: 
59: ### Phase 4: Emulation Components
60: - [x] Graphics: GOP mode + guest framebuffer window, verified blits
61: - [x] Audio: guest RAM ring buffer device (no UEFI audio standard)
62: - [x] Storage: Block I/O enumeration + real sector reads
63: - [x] Networking: SNP start/initialize/transmit
64: 
65: ### Phase 5: Boot Process
66: - [x] Multi-region guest memory map (RAM + low-memory globals + read-only ROM)
67: - [x] ROM load with priority: ESP path → ESP Mac OS ROM → HFS Mac OS ROM →
68:       demo fallback; Old World/New World identification
69: - [x] System initialization: CPU reset to reset vector, boot-info block,
70:       `PpcRunBootSelfTest`
71: - [x] System Folder scan + System/Finder staging + Extensions driver registry
72:       (64 drivers) with HFS catalog-ID lookup
73: - [x] Verify Phase 5 build and boot under QEMU/OVMF on Windows
74: - [ ] Boot a real guest OS (the headline remaining item)
75: 
76: ### Phase 6: Real Boot — Native 68K Interpreter
77: The active bottleneck is the post-nanokernel handoff to the 68K DR emulator.
78: The ROM's built-in PPC DR emulator crashes because internal structures are not
79: fully set up after ~4 sessions of attempts. The chosen approach is a **native
80: 68K interpreter in C** (Path 2: replace the ROM's PPC DR emulator entirely).
81: 
82: #### 6.1 68K CPU Context & Core Infrastructure
83: - [ ] Create `src/cpu/m68k.h` — 68K CPU context struct (D0-D7, A0-A7, PC, SR/CCR,
84:       supervisor/user mode), public API declarations
85: - [ ] Create `src/cpu/m68k.c` — 68K decode/execute engine
86: - [ ] Define M68K_CPU_CONTEXT struct: data registers (D0-D7, 32-bit), address
87:       registers (A0-A7, 32-bit), PC (32-bit), SR (16-bit), supervisor/user tracking
88: - [ ] Memory access layer: reuse existing CpuRead8/16/32 and CpuWrite8/16/32
89:       (both are big-endian, no byte-swap needed)
90: - [ ] Effective Address (EA) computation: all 9 68k addressing modes
91:       (Dn, An, (An), (An)+, -(An), d16(An), d16(An,Dn), xxx.W, xxx.L)
92: - [ ] SR/CCR helpers: X, N, Z, V, C flags; pack/unpack SR; flag computation
93:       from ALU results
94: 
95: #### 6.2 68K Instruction Decode & Execute (Minimum Set for System 7 Boot)
96: - [ ] Data movement: MOVE.B/W/L, MOVEA.W/L, MOVEQ, LEA, PEA, LINK/UNLK,
97:       MOVEM, EXG
98: - [ ] Arithmetic: ADD/SUB (byte/word/long), ADDI/SUBI, ADDQ/SUBQ, ADDA/SUBA,
99:       CMP/CMPA/CMPI, NEG, EXT
100: - [ ] Logic: AND/OR/EOR (byte/word/long), ANDI/ORI/EORI, NOT
101: - [ ] Shift/Rotate: ASL/ASR, LSL/LSR, ROL/ROR (register and immediate)
102: - [ ] Branch: BRA, BSR, Bcc (BEQ/BNE/BLT/BGT/BLE/BGE/BCS/BCC), DBcc
103: - [ ] Bit: BTST/BSET/BCLR/BCHG (register and memory)
104: - [ ] System: RESET, NOP, RTE, MOVE USP, STOP, TRAP, Illegal instruction
105: - [ ] Special: SWAP, TAS, TST, CMPi (CMPI/CMPi)
106: - [ ] Unknown opcodes: print-and-skip stub (don't crash)
107: 
108: #### 6.3 Integration with PPC Interpreter
109: - [ ] Global 68K context struct (like g_PpcContext for PPC)
110: - [ ] Trampoline entry: `M68kExecuteFromPPC()` called when PPC code enters
111:       the 68K opcode table; reads 68k opcode from guest memory at 68k PC,
112:       decodes/executes, updates 68k PC/registers/flags, returns to PPC loop
113: - [ ] Opcode table patching: in PpcPatchNewWorldRom() (or post-patch hook),
114:       overwrite each regular opcode table entry (ROM+0x380000) with a branch
115:       to the C trampoline
116: - [ ] Exception handling: 68k exceptions dispatched through 68k vector table
117:       (0x0000-0x03FF); integrates with existing PPC exception handling
118: - [ ] Remove/keep existing hooks (MOVE.SR, RESET, 4E7B, MOVEQ) as appropriate
119: 
120: #### 6.4 Boot Integration
121: - [ ] Initial 68k context setup at emulator-start (0x40B6F900):
122:       PC from reset vector (0x0004), SSP from 0x0000, SR=0x2700
123: - [ ] Trap #1 dispatch: basic Mac OS system call mechanism (stub most traps
124:       initially, implement the most critical ones incrementally)
125: - [ ] Progressive enhancement: get first few hundred 68k instructions executing
126:       after handoff, then add opcodes as crash points are discovered
127: 
128: #### 6.5 Verification
129: - [ ] 68k self-test suite: test individual opcodes, flag computation,
130:       addressing modes, exception dispatch
131: - [ ] Incremental testing: nanokernel to handoff, then single-step 68k
132:       interpreter to verify each opcode as encountered
133: 
134: #### Files to Create/Modify
135: - **Create**: `src/cpu/m68k.h` (68k context struct, public API)
136: - **Create**: `src/cpu/m68k.c` (68k decode/execute engine)
137: - **Modify**: `src/cpu/interpreter.c` (add integration hooks, call m68k functions)
138: - **Modify**: `src/boot/bootloader.h` (add m68k-related constants if needed)
139: - **Build system**: ensure m68k.c is compiled and linked
140: 
141: ## Architecture Decisions
142: 
143: ### Heavy bootloader, not an application emulator
144: UEFI standard protocols are the hardware abstraction; guest-visible Mac devices
145: are thin simulated windows wired to GOP/BlockIO/SNP. This keeps the host-side
146: code small and lets the guest own the boot process.
147: 
148: ### Target architecture: PowerPC
149: - Better fit for Mac OS 8/9 and for a user-supplied Old World ROM.
150: - References: SheepShaver, Basilisk II, QEMU, DingusPPC.
151: 
152: ### In-emulator HFS reader
153: The bootloader must read Mac discs without a host filesystem; catalog-ID lookup
154: avoids name/path separator ambiguity and survives all three test-disc layouts.

(End of file - total 154 lines)
</content>
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
1: # EFI Mac OS Boot Layer — Architecture
2: 
3: This document describes how the project actually works today: a heavy UEFI
4: bootloader that stages a classic PowerPC Mac boot environment from UEFI standard
5: protocols, plus the design decisions behind it.
6: 
7: ## Overview
8: 
9: Classic Mac OS (System 7 through Mac OS 9) boots from firmware that owns a
10: PowerPC Mac: a read-only ROM window at `0xFFF00000`, low-memory system globals
11: at `0x0`, a boot volume containing the System Folder, and hardware devices.
12: This project supplies that firmware-side environment as an EFI application:
13: 
14: 1. **UEFI is the hardware.** GOP is the display, Block I/O / Simple File System
15:    is storage, Simple Network Protocol is the network, and UEFI pool/allocation
16:    services are memory. Simulated Mac devices (framebuffer, audio ring) are
17:    buffers inside guest RAM that the bootloader copies to/from the real UEFI
18:    devices.
19: 2. **A classic Mac boot volume is read in place.** An in-emulator HFS reader
20:    parses the attached disc directly (no host mount required), so the bootloader
21:    works on raw floppy/disc images as QEMU would see them.
22: 3. **Firmware is installed into the guest image.** A ROM is loaded (boot-volume
23:    path, ESP file, or HFS `Mac OS ROM` discovery), mapped read-only into the
24:    guest map, and identified as Old World / New World / demo.
25: 4. **A PowerPC interpreter executes guest code.** Fixed 32-bit opcodes are
26:    decoded and interpreted against guest memory (with a multi-region map and
27:    read-only ROM enforcement), including a full FPU core and exception support.
28: 
29: ## Boot Flow
30: 
31: ```
32: efi_main (src/main.c)
33:   PpcInitializeUefiInterface      # LoadedImage, Boot Services, console
34:   PpcInitializeDebug              # boot.log + monotonic timer
35:   PpcInitializeTranslationContext # PPC register file, MSR/SRR0/1/CTR/LR
36:   PpcRunSelfTest                  # 35 checks incl. FPU core
37:   PpcInitializeMemoryManager      # 256 MB guest RAM @ 0x10000000
38:   PpcSetGuestMemory               # wire UEFI pages into interpreter
39:   [RAM-resident PPC program demo] # addi/mullw/stw through the memory path
40:   PpcInitializeHardwareAbstraction# GOP, Block I/O, SNP, audio ring
41:   PpcInitializeBootloader
42:   PpcSetupBootEnvironment
43:   PpcInitializeGraphics           # GOP mode + guest framebuffer window
44:   [Graphics self-checks]          # full-screen frames verified on the GOP buffer
45:   PpcInstallLowMemory             # 16 KB globals @ 0x0
46:   PpcInstallSystemRom             # \System\MacOS\ROM -> HFS "Mac OS ROM" -> demo
47:   PpcRunBootSelfTest              # region map, read-only ROM, reset vector
48:   PpcPrepareSystemForBoot         # PC = reset vector, MSR = ME|RI, boot info block
49:   PpcLocateSystemFolder / PpcLoadSystemFiles / PpcScanExtensionsDirectory /
50:     PpcLoadDrivers                # stage System, Finder, Mac OS ROM, Extensions
51:   PpcRunSystemFilesSelfTest       # staged bytes read back via interpreter
52:   PpcGetBootInfo -> status report
53: ```
54: 
55: ## ROM Sourcing and Types
56: 
57: Priority order (implemented in `PpcInstallSystemRom` /
58: `PpcLoadSystemRom` / `BootLoadHfsRomToPages`):
59: 
60: 1. `\System\MacOS\ROM` on the boot volume — an Old World firmware dump
61:    (System 7 needs one of these).
62: 2. `\System Folder\Extensions\Mac OS ROM` on the ESP.
63: 3. `Mac OS ROM` found anywhere on an attached Mac disc via
64:    `PpcHfsFindMacOsRom` (whole-catalog search, largest non-empty match). This is
65:    how a real Mac OS 9.2.2 install disc yields its 2,763,530-byte New World ROM
66:    from `Power Mac G4 Install:System Folder:Mac OS ROM`.
67: 4. `PpcInstallDemoRom` — a 4 MB self-contained image with a reset-vector
68:    program, used to keep the full install + self-test path alive without
69:    firmware.
70: 
71: `BootIdentifyRomType` classifies by signature: a leading `<CHRP-BOOT>\r` means
72: New World (PPC, Mac OS 8.5+), otherwise Old World, and the guest boot-info block
73: records the type. The boot self-test adapts to the ROM: the `ROM1` magic and
74: reset-vector execution checks run only for the demo ROM, while a real ROM is
75: verified for region presence (and the CHRP signature when New World) plus
76: read-only enforcement.
77: 
78: ## Guest Memory Map
79: 
80: Managed by `PpcAddGuestMemoryRegion` (multi-region map in the interpreter,
81: read-only flag per region):
82: 
83: | Region              | Guest address | Size       | Access |
84: |---------------------|---------------|------------|--------|
85: | Low-memory globals  | `0x00000000`  | 16 KB      | R/W    |
86: | Guest RAM           | `0x10000000`  | 256 MB     | R/W    |
87: | Framebuffer window  | `0x18000000`  | 640x480x32 | R/W    |
88: | Audio ring buffer   | `0x18800000`  | 8 KB       | R/W    |
89: | System area         | `0x20000000`  | 16 MB      | R/W    |
90: | Driver area         | `0x21000000`  | 32 MB      | R/W    |
91: | System ROM          | `0xFFF00000`  | 4 MB       | R (ROM) |
92: 
93: The bootloader-defined boot-info block in low memory (magic `"EFI!"` at `0x0`,
94: then RAM base/size, ROM base/size, ROM type) is entirely host-defined — it is
95: not a real Mac OS ROM globals table.
96: 
97: ## In-Emulator HFS Reader
98: 
99: `src/fs/hfs.c` parses classic HFS volumes without the host mounting them:
100: 
101: - **Block-size auto-detection** so raw floppy images (512 B blocks), CD ISO
102:   images (2048 B blocks), and HFS-with-2-KB-cluster layouts all work.
103: - **Catalog-based lookup** (`PpcHfsGetEntryById`) that resolves files by their
104:   catalog FlNum/DirID, so names containing `/` or `:` are handled correctly and
105:   are not ambiguous with path separators.
106: - **Extent handling** with multi-overflow-extent support for large files.
107: - Used by the System Folder probe (`PpcHfsProbeBootFiles`), the whole-catalog
108:   `Mac OS ROM` search, and driver enumeration (`BootEnumerateExtensionsHfs`).
109: 
110: The reader has been exercised against System 7.5.3 (raw HFS image), Mac OS 8.1
111: (ISO with non-zero block base), and Mac OS 9.2.2 (ISO with multi-overflow
112: extents).
113: 
114: ## System Folder and Driver Staging
115: 
116: - `PpcLocateSystemFolder` finds the System Folder on the boot volume (ESP FAT or
117:   attached HFS disc) and detects System, Finder, Extensions, and the Mac OS ROM.
118: - `PpcLoadSystemFiles` stages System and Finder (empty files are skipped; System
119:   7.5.3's `Finder` is a genuine 0-byte stub) into the system area at
120:   `0x20000000`.
121: - `PpcScanExtensionsDirectory` / `PpcLoadDrivers` enumerate and stage up to 64
122:   Extensions (drivers) into the driver area at `0x21000000`; empty data forks
123:   are skipped. Every non-empty extension stages with 0 failures on the test
124:   discs (7.5.3: 2/2, 8.1: 18/18, 9.2.2: 25/25).
125: 
126: ## Device Simulation on UEFI
127: 
128: - **Graphics:** `PpcInitializeGraphics` selects a GOP mode and carves a
129:   640x480x32 guest framebuffer window. Guest code writes big-endian `0xRRGGBB00`
130:   pixels there; `PpcGraphicsBlitToDisplay` converts and copies to the real GOP
131:   framebuffer (byte-exact for RGB and BGR layouts). Verified with full-screen
132:   solid frames checked pixel-by-pixel on the GOP buffer, band boundaries,
133:   corners, and out-of-bounds write rejection.
134: - **Storage:** `PpcInitializeBlockIo` enumerates every Block I/O handle and
135:   reports real geometry; `PpcReadDiskBlock` issues real `ReadBlocks` calls. The
136:   HFS reader is layered on top of this.
137: - **Network:** `PpcInitializeNetwork` starts and initializes every Simple
138:   Network Protocol interface, snapshots real mode (MAC, media state), and
139:   transmits a real frame via `Transmit`/`GetStatus`.
140: - **Audio:** no UEFI audio standard exists, so the device is a fixed ring buffer
141:   in guest RAM; the host reads PCM samples back and advances a play cursor.
142: 
143: ## PowerPC Interpreter
144: 
145: `src/cpu/interpreter.c` decodes and executes fixed 32-bit big-endian PowerPC
146: opcodes with a register file (32 GPRs, CR, CTR, LR, MSR, SRR0/1, FP registers +
147: FPSCR), big-endian guest memory access, FPU core (opcodes 48-63, gated on
148: MSR[FP] with the FP-unavailable exception at `0x800`), and exception dispatch
149: (program `0x700`, FP `0x800`). Execution today is block-at-a-time
150: (`PpcExecuteBlock`): small hand-checked programs run from guest RAM and the
151: demo ROM's reset vector. There is no MMU, no timer/interrupt injection, and no
152: continuous fetch-execute loop — the ROM window is never executed for real.
153: 
154: ## Build and Run
155: 
156: See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for the clang/lld-link
157: GNU-EFI cross-build (Windows git-bash script or macOS `make`) and
158: [USER_GUIDE.md](USER_GUIDE.md) for the QEMU/OVMF boot and disc attachment.
159: 
160: ## Open Work
161: 
162: See [TODO.md](TODO.md). The short list: continuous guest execution, MMU and
163: exception delivery to real firmware, Mac device register emulation in the guest
164: map, Old World ROM boot testing with System 7, and New World ROM execution.

(End of file - total 164 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 6.0s)

_Thinking:_

Now I have a good understanding of the project. Let me look at the source code to understand what's been implemented so far.

Now let me examine the source code to understand what exists:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src</path>
<type>directory</type>
<entries>
boot/
cpu/
fs/
hardware/
main.c
memory/
platform/
ui/
utils/

(9 entries)
</entries>
```

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
8: #include "cpu/interpreter.h"
9: #include "cpu/translation.h"
10: #include "cpu/m68k.h"
11: #include "memory/manager.h"
12: #include "hardware/abstraction.h"
13: #include "boot/bootloader.h"
14: #include "fs/hfs.h"
15: #include "utils/debug.h"
16: #include "platform/uefi_interface.h"
17: #include "ui/ui.h"
18: 
19: // True if the GOP pixel at (X,Y) holds exactly the R/G/B channels of the
20: // given guest color (big-endian 0xRRGGBB00), placed according to the GOP
21: // pixel format (byte-exact, so channel-position bugs are caught).
22: STATIC
23: BOOLEAN
24: GopPixelMatches (
25:   IN UINT8* GopBase,
26:   IN UINTN  GopPitch,
27:   IN UINTN  X,
28:   IN UINTN  Y,
29:   IN UINT32 GuestColor,
30:   IN UINT32 PixelFormat
31:   )
32: {
33:   UINT8 R = (UINT8)(GuestColor >> 24);
34:   UINT8 G = (UINT8)(GuestColor >> 16);
35:   UINT8 B = (UINT8)(GuestColor >> 8);
36:   UINT8* P = GopBase + Y * GopPitch + X * 4;
37:   if (PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
38:     return P[0] == B && P[1] == G && P[2] == R;
39:   }
40:   return P[0] == R && P[1] == G && P[2] == B;
41: }
42: 
43: // True if every visible pixel of the GOP framebuffer matches the guest color.
44: STATIC
45: BOOLEAN
46: GopFrameIsSolid (
47:   IN UINT8* GopBase,
48:   IN UINTN  GopPitch,
49:   IN UINTN  W,
50:   IN UINTN  H,
51:   IN UINT32 GuestColor,
52:   IN UINT32 PixelFormat
53:   )
54: {
55:   for (UINTN Y = 0; Y < H; Y++) {
56:     for (UINTN X = 0; X < W; X++) {
57:       if (!GopPixelMatches(GopBase, GopPitch, X, Y, GuestColor, PixelFormat)) {
58:         return FALSE;
59:       }
60:     }
61:   }
62:   return TRUE;
63: }
64: 
65: // Short label for an installed ROM type.
66: STATIC
67: CHAR16*
68: BootRomTypeName (
69:   IN UINT32 RomType
70:   )
71: {
72:   switch (RomType) {
73:   case PPC_ROM_TYPE_NEW_WORLD: return L"New World (Mac OS ROM)";
74:   case PPC_ROM_TYPE_OLD_WORLD: return L"Old World (classic firmware dump)";
75:   case PPC_ROM_TYPE_DEMO:      return L"demo (no Mac firmware)";
76:   default:                     return L"unknown";
77:   }
78: }
79: 
80: EFI_STATUS
81: efi_main (
82:   IN EFI_HANDLE        ImageHandle,
83:   IN EFI_SYSTEM_TABLE  *SystemTable
84:   )
85: {
86:   EFI_STATUS Status;
87:   
88:   // Initialize the GNU-EFI library
89:   InitializeLib(ImageHandle, SystemTable);
90:   
91:   // Print welcome message
92:   Print(L"EFI Mac OS Boot Layer v0.2\n");
93:   Print(L"Heavy bootloader for classic Mac OS (System 7, Mac OS 8/9) via UEFI\n");
94:   Print(L"Initializing PowerPC environment...\n");
95:   
96:   // Initialize UEFI interface
97:   Status = PpcInitializeUefiInterface(ImageHandle, SystemTable);
98:   if (EFI_ERROR(Status)) {
99:     Print(L"Failed to initialize UEFI interface: %r\n", Status);
100:     return Status;
101:   }
102: 
103:   // Load the saved configuration, run the boot gate (ASCII Macintosh splash
104:   // with a 5-second countdown) and, when F8 is pressed, the setup menu. The
105:   // resulting configuration drives the boot below.
106:   PPC_CONFIG BootConfig;
107:   PpcConfigLoad(&BootConfig);
108:   if (PpcBootGateWait(&BootConfig)) {
109:     Status = PpcShowConfigMenu(&BootConfig);
110:     if (EFI_ERROR(Status)) {
111:       Print(L"Configuration menu failed: %r\n", Status);
112:     }
113:   }
114: 
115:   // Honor the configured boot volume (auto-detection when not pinned).
116:   PpcHfsSetDeviceIndex(BootConfig.BootDeviceIndex);
117:   
118:   // Initialize debug system (level follows the setup configuration)
119:   Status = PpcInitializeDebug(BootConfig.DebugEnabled ? PPC_DEBUG_LEVEL_DEBUG
120:                                                       : PPC_DEBUG_LEVEL_ERROR,
121:                               FALSE, NULL);
122:   if (EFI_ERROR(Status)) {
123:     Print(L"Failed to initialize debug system: %r\n", Status);
124:     return Status;
125:   }
126:   
127:   // Initialize PowerPC translation context
128:   Status = PpcInitializeTranslationContext();
129:   if (EFI_ERROR(Status)) {
130:     Print(L"Failed to initialize translation context: %r\n", Status);
131:     return Status;
132:   }
133:   
134:   // Run the CPU self-test suite
135:   Status = PpcRunSelfTest();
136:   if (EFI_ERROR(Status)) {
137:     Print(L"PowerPC CPU self-test FAILED: %r\n", Status);
138:     return Status;
139:   }
140: 
141:   // Initialize the 68K interpreter
142:   M68kInitialize();
143: 
144:   // Run the 68K CPU self-test suite
145:   Status = M68kRunSelfTest();
146:   if (EFI_ERROR(Status)) {
147:     Print(L"68K CPU self-test FAILED: %r\n", Status);
148:     return Status;
149:   }
150:   
151:   // Initialize memory manager (guest RAM at the classic Mac OS kernel base,
152:   // sized from the setup configuration; 256 MB default).
153:   {
154:     UINTN GuestRamMB = (UINTN)BootConfig.MemorySizeMB;
155:     if (GuestRamMB < 128 || GuestRamMB > 2048) {
156:       GuestRamMB = 256;
157:     }
158:     Status = PpcInitializeMemoryManager(0x10000000, GuestRamMB * 0x100000);
159:   }
160:   if (EFI_ERROR(Status)) {
161:     Print(L"Failed to initialize memory manager: %r\n", Status);
162:     return Status;
163:   }
164:   
165:   // Wire the UEFI-allocated guest RAM into the interpreter so that guest
166:   // loads/stores and instruction fetches target the emulated memory region.
167:   {
168:     VOID*  HostBase  = NULL;
169:     UINT64 GuestBase = 0;
170:     UINT64 GuestSize = 0;
171:     Status = PpcGetGuestMemoryRegion(&HostBase, &GuestBase, &GuestSize);
172:     if (EFI_ERROR(Status)) {
173:       Print(L"Failed to get guest memory region: %r\n", Status);
174:       return Status;
175:     }
176:     Status = PpcSetGuestMemory(HostBase, (UINT32)GuestBase, (UINT32)GuestSize);
177:     if (EFI_ERROR(Status)) {
178:       Print(L"Failed to wire guest memory: %r\n", Status);
179:       return Status;
180:     }
181:     Print(L"Guest memory wired: host 0x%x <-> guest 0x%x (%d bytes)\n",
182:           HostBase, GuestBase, GuestSize);
183:   }
184:   
185:   // Execute a small PowerPC program that lives in guest RAM. This proves the
186:   // full pipeline: guest memory allocation -> fetch -> decode -> execute.
187:   {
188:     VOID*  DemoHostBase  = NULL;
189:     UINT64 DemoGuestBase = 0;
190:     UINT64 DemoGuestSize = 0;
191:     Status = PpcGetGuestMemoryRegion(&DemoHostBase, &DemoGuestBase, &DemoGuestSize);
192:     if (EFI_ERROR(Status)) {
193:       Print(L"Demo: guest memory region unavailable: %r\n", Status);
194:       return Status;
195:     }
196: 
197:     // addi r3,r0,40 ; addi r4,r0,2 ; mullw r5,r3,r4 ; stw r5,0(r1)
198:     static const UINT32 GuestProgram[] = {
199:       0x38600028,   // addi r3, r0, 40
200:       0x38800002,   // addi r4, r0, 2
201:       0x7CA321D6,   // mullw r5, r3, r4
202:       0x90A10000,   // stw  r5, 0(r1)
203:     };
204:     const UINT32 ProgramBase = (UINT32)DemoGuestBase;
205:     const UINT32 ResultAddr  = ProgramBase + 0x40;  // store result here
206:     const UINTN  ProgramCount = sizeof(GuestProgram) / sizeof(GuestProgram[0]);
207: 
208:     // Write the program into guest RAM (big-endian byte order)
209:     for (UINTN i = 0; i < ProgramCount; i++) {
210:       UINT32 w = GuestProgram[i];
211:       for (UINTN b = 0; b < 4; b++) {
212:         PpcWriteGuestByte(ProgramBase + (UINT32)i * 4 + (UINT32)b,
213:                           (UINT8)(w >> (24 - (UINTN)b * 8)));
214:       }
215:     }
216: 
217:     Print(L"\n--- Executing PowerPC program from guest RAM ---\n");
218: 
219:     PpcSetGprValue(1, ResultAddr);   // r1 points at the result slot
220:     PpcSetGprValue(3, 0);
221:     PpcSetGprValue(4, 0);
222:     PpcSetGprValue(5, 0);
223: 
224:     // Fetch/decode/execute loop over the guest-memory resident program
225:     UINTN  Executed = 0;
226:     EFI_STATUS ExecStatus = PpcExecuteBlock(
227:         (UINT32*)(UINTN)ProgramBase,
228:         ProgramCount,
229:         &Executed
230:     );
231:     BOOLEAN RanClean = (ExecStatus == EFI_SUCCESS);
232: 
233:     // Read the stored word back out of guest RAM (big-endian)
234:     UINT32 Result = 0;
235:     for (UINTN b = 0; b < 4; b++) {
236:       Result = (Result << 8) | PpcReadGuestByte(ResultAddr + (UINT32)b);
237:     }
238:     UINT32 R5 = PpcGetGprValue(5);
239: 
240:     Print(L"Program executed: %d instructions (status %r)\n", Executed, ExecStatus);
241:     Print(L"r5 = %d (expected 80)\n", R5);
242:     Print(L"Guest RAM[0x%x] = %d (expected 80)\n", ResultAddr, Result);
243: 
244:     if (RanClean && Executed == ProgramCount && R5 == 80 && Result == 80) {
245:       Print(L"Guest RAM execution: PASS\n");
246:     } else {
247:       Print(L"Guest RAM execution: FAIL\n");
248:     }
249:   }
250:   
251:   // Initialize hardware abstraction layer
252:   Status = PpcInitializeHardwareAbstraction();
253:   if (EFI_ERROR(Status)) {
254:     Print(L"Failed to initialize hardware abstraction: %r\n", Status);
255:     return Status;
256:   }
257:   
258:   // Initialize bootloader
259:   Status = PpcInitializeBootloader();
260:   if (EFI_ERROR(Status)) {
261:     Print(L"Failed to initialize bootloader: %r\n", Status);
262:     return Status;
263:   }
264:   
265:   // Setup boot environment
266:   Status = PpcSetupBootEnvironment();
267:   if (EFI_ERROR(Status)) {
268:     Print(L"Failed to setup boot environment: %r\n", Status);
269:     return Status;
270:   }
271:   
272:   // Initialize graphics for the emulator at the configured resolution.
273:   {
274:     UINT32 VideoWidth  = 640;
275:     UINT32 VideoHeight = 480;
276:     PpcVideoModeResolution(BootConfig.VideoMode, &VideoWidth, &VideoHeight);
277:     Status = PpcInitializeGraphics(VideoWidth, VideoHeight, 32);
278:   }
279:   if (EFI_ERROR(Status)) {
280:     Print(L"Failed to initialize graphics: %r\n", Status);
281:     return Status;
282:   }
283:   
284:   // Graphics self-check: draw into the guest framebuffer, blit to the real
285:   // GOP display, and read pixels back from both buffers to verify the path.
286:   {
287:     PPC_FRAMEBUFFER_INFO FbInfo;
288:     Status = PpcGetFrameBufferInfo(&FbInfo);
289:     if (!EFI_ERROR(Status) && FbInfo.HostBuffer != NULL) {
290:       PpcGraphicsClear(0x10101000);                          // dark grey background
291:       PpcGraphicsDrawRect(0, 0, 640, 120, 0xCC000000);       // red band
292:       PpcGraphicsDrawRect(0, 120, 640, 120, 0x00CC0000);     // green band
293:       PpcGraphicsDrawRect(0, 240, 640, 120, 0x0000CC00);     // blue band
294:       PpcGraphicsDrawRect(0, 360, 640, 120, 0xCCCC0000);     // yellow band
295:       Status = PpcGraphicsBlitToDisplay();
296: 
297:       UINT32 P0 = (UINT32)((UINT8*)FbInfo.HostBuffer)[0] << 24 |
298:                   (UINT32)((UINT8*)FbInfo.HostBuffer)[1] << 16 |
299:                   (UINT32)((UINT8*)FbInfo.HostBuffer)[2] << 8;
300:       UINT8* GopP = (UINT8*)(UINTN)FbInfo.GopFrameBuffer;
301:       UINT32 P1 = ((UINT32)GopP[2] << 16) | ((UINT32)GopP[1] << 8) | (UINT32)GopP[0];
302: 
303:       if (!EFI_ERROR(Status) && (P0 >> 24) == 0xCC && (P0 & 0xFF0000) == 0) {
304:         Print(L"Graphics self-check: PASS (guest fb 0x%08x, GOP fb 0x%08x)\n", P0, P1);
305:       } else {
306:         Print(L"Graphics self-check: FAIL (guest fb 0x%08x, GOP fb 0x%08x)\n", P0, P1);
307:       }
308:     } else {
309:       Print(L"Graphics self-check: SKIP (no framebuffer)\n");
310:     }
311:   }
312: 
313:   // Multi-frame graphics test: full-screen frames written into the guest
314:   // framebuffer are blitted through the real GOP path and verified across
315:   // the entire display, plus band boundaries, corners, and out-of-bounds.
316:   {
317:     PPC_FRAMEBUFFER_INFO FbInfo;
318:     Status = PpcGetFrameBufferInfo(&FbInfo);
319:     if (!EFI_ERROR(Status) && FbInfo.HostBuffer != NULL && FbInfo.GopPitch != 0) {
320:       UINTN  W   = FbInfo.Width;
321:       UINTN  H   = FbInfo.Height;
322:       UINT8* Gop = (UINT8*)(UINTN)FbInfo.GopFrameBuffer;
323:       BOOLEAN MultiOk = TRUE;
324: 
325:       // Frames 1-3: solid red, green, blue across the full visible area.
326:       UINT32 Solids[3] = { 0xCC000000, 0x00CC0000, 0x0000CC00 };
327:       for (UINTN f = 0; f < 3; f++) {
328:         PpcGraphicsClear(Solids[f]);
329:         PpcGraphicsBlitToDisplay();
330:         if (!GopFrameIsSolid(Gop, FbInfo.GopPitch, W, H, Solids[f],
331:                              (UINT32)FbInfo.PixelFormat)) {
332:           MultiOk = FALSE;
333:           Print(L"Multi-frame: solid frame %d FAIL\n", f + 1);
334:         }
335:       }
336:       if (MultiOk) {
337:         Print(L"Multi-frame: 3 solid frames full-coverage PASS\n");
338:       }
339: 
340:       // Frame 4: vertical color bands; verify band centers and boundaries.
341:       PpcGraphicsClear(0x10101000);
342:       PpcGraphicsDrawRect(0, 0, W / 4, H, 0xCC000000);      // red
343:       PpcGraphicsDrawRect(W / 4, 0, W / 4, H, 0x00CC0000);  // green
344:       PpcGraphicsDrawRect(W / 2, 0, W / 4, H, 0x0000CC00);  // blue
345:       PpcGraphicsDrawRect(3 * W / 4, 0, W / 4, H, 0xCCCC0000); // yellow
346:       PpcGraphicsBlitToDisplay();
347: 
348:       BOOLEAN BandsOk =
349:         GopPixelMatches(Gop, FbInfo.GopPitch, W / 8, H / 2, 0xCC000000,
350:                         (UINT32)FbInfo.PixelFormat) &&
351:         GopPixelMatches(Gop, FbInfo.GopPitch, 3 * W / 8, H / 2, 0x00CC0000,
352:                         (UINT32)FbInfo.PixelFormat) &&
353:         GopPixelMatches(Gop, FbInfo.GopPitch, 5 * W / 8, H / 2, 0x0000CC00,
354:                         (UINT32)FbInfo.PixelFormat) &&
355:         GopPixelMatches(Gop, FbInfo.GopPitch, 7 * W / 8, H / 2, 0xCCCC0000,
356:                         (UINT32)FbInfo.PixelFormat) &&
357:         GopPixelMatches(Gop, FbInfo.GopPitch, W / 2 - 1, H / 2, 0x00CC0000,
358:                         (UINT32)FbInfo.PixelFormat) &&
359:         GopPixelMatches(Gop, FbInfo.GopPitch, W / 2, H / 2, 0x0000CC00,
360:                         (UINT32)FbInfo.PixelFormat);
361:       Print(L"Multi-frame: band frame %s\n", BandsOk ? L"PASS" : L"FAIL");
362:       MultiOk = MultiOk && BandsOk;
363: 
364:       // Corner and out-of-bounds handling.
365:       BOOLEAN CornersOk =
366:         GopPixelMatches(Gop, FbInfo.GopPitch, 0, 0, 0xCC000000,
367:                         (UINT32)FbInfo.PixelFormat) &&
368:         GopPixelMatches(Gop, FbInfo.GopPitch, W - 1, H - 1, 0xCCCC0000,
369:                         (UINT32)FbInfo.PixelFormat);
370:       PpcGraphicsSetPixel(W + 100, H + 100, 0xFF000000);   // OOB write is dropped
371:       PpcGraphicsBlitToDisplay();
372:       BOOLEAN OobOk =
373:         GopPixelMatches(Gop, FbInfo.GopPitch, W - 1, H - 1, 0xCCCC0000,
374:                         (UINT32)FbInfo.PixelFormat);
375:       Print(L"Multi-frame: corners %s, OOB dropped %s\n",
376:             CornersOk ? L"PASS" : L"FAIL", OobOk ? L"PASS" : L"FAIL");
377:       MultiOk = MultiOk && CornersOk && OobOk;
378: 
379:       Print(L"Multi-frame graphics self-check: %s\n",
380:             MultiOk ? L"PASS" : L"FAIL");
381:     } else {
382:       Print(L"Multi-frame graphics self-check: SKIP (no framebuffer)\n");
383:     }
384:   }
385:   
386:   // Initialize audio subsystem (disabled in the setup menu when off)
387:   if (BootConfig.AudioEnabled) {
388:     Status = PpcInitializeAudio();
389:     if (EFI_ERROR(Status)) {
390:       Print(L"Failed to initialize audio: %r\n", Status);
391:       return Status;
392:     }
393:   } else {
394:     Print(L"Audio device: DISABLED (setup)\n");
395:   }
396: 
397:   // Audio self-check: the guest writes big-endian PCM samples into the
398:   // ring buffer, the host reads them back, then advances playback.
399:   if (BootConfig.AudioEnabled) {
400:     {
401:       PPC_AUDIO_INFO AudioInfo;
402:       Status = PpcAudioGetBufferInfo(&AudioInfo);
403:       if (!EFI_ERROR(Status) && AudioInfo.HostBuffer != NULL) {
404:         // Guest-side write path: 16-bit big-endian 0x1000 = 4096 sample.
405:         UINT32 GuestAddr = AudioInfo.GuestBase;
406:         PpcWriteGuestByte(GuestAddr + 0, 0x10);
407:         PpcWriteGuestByte(GuestAddr + 1, 0x00);
408:         PpcWriteGuestByte(GuestAddr + 2, 0x20);
409:         PpcWriteGuestByte(GuestAddr + 3, 0x00);
410: 
411:         UINT16 S0 = 0, S1 = 0;
412:         BOOLEAN ReadOk = (PpcAudioReadSample(0, &S0) == EFI_SUCCESS) &&
413:                          (PpcAudioReadSample(1, &S1) == EFI_SUCCESS);
414:         Status = PpcAudioAdvancePlayback(2);
415:         PPC_AUDIO_INFO After;
416:         PpcAudioGetBufferInfo(&After);
417: 
418:         if (ReadOk && S0 == 0x1000 && S1 == 0x2000 && !EFI_ERROR(Status) &&
419:             After.PlayCursor == 2) {
420:           Print(L"Audio self-check: PASS (samples 0x%04x/0x%04x, played %d)\n",
421:                 S0, S1, (UINT32)After.PlayCursor);
422:         } else {
423:           Print(L"Audio self-check: FAIL (samples 0x%04x/0x%04x, played %d)\n",
424:                 S0, S1, (UINT32)(After.PlayCursor));
425:         }
426:       } else {
427:         Print(L"Audio self-check: SKIP (no buffer)\n");
428:       }
429:     }
430:   }
431:   
432:   // Initialize storage subsystem
433:   Status = PpcInitializeStorage(1);
434:   if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
435:     Print(L"Failed to initialize storage: %r\n", Status);
436:     return Status;
437:   }
438:   
439:   // Block I/O self-check: enumerate real block devices and read a sector.
440:   Status = PpcInitializeBlockIo(1);
441:   if (!EFI_ERROR(Status)) {
442:     PPC_BLOCK_IO_INFO BioInfo;
443:     Status = PpcGetBlockIoInfo(&BioInfo);
444:     if (!EFI_ERROR(Status)) {
445:       BOOLEAN FoundMarker = FALSE;
446:       for (UINTN i = 0; i < BioInfo.DeviceCount; i++) {
447:         UINT8 Sector[512];
448:         if (!EFI_ERROR(PpcReadDiskBlock(i, 0, 512, Sector)) &&
449:             Sector[0] == 'E' && Sector[1] == 'F' && Sector[2] == 'I') {
450:           Print(L"Block I/O self-check: PASS (device %d, LBA 0 = \"%c%c%c...\", 512-byte sector)\n",
451:                 (UINTN)i, Sector[0], Sector[1], Sector[2]);
452:           FoundMarker = TRUE;
453:           break;
454:         }
455:       }
456:       if (!FoundMarker) {
457:         Print(L"Block I/O self-check: devices enumerated, marker not found\n");
458:       }
459:     }
460:   } else {
461:     Print(L"Block I/O self-check: SKIP (%r)\n", Status);
462:   }
463: 
464:   // HFS self-test: mount the attached Mac OS disc (raw HFS/HFS+ block device)
465:   // and verify catalog parsing + System file readback through Block I/O.
466:   PpcHfsRunSelfTest();
467:   
468:   // Initialize network subsystem (disabled in the setup menu when off)
469:   if (BootConfig.NetworkEnabled) {
470:     Status = PpcInitializeNetwork(1);
471:     if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
472:       Print(L"Failed to initialize network: %r\n", Status);
473:       return Status;
474:     }
475:   } else {
476:     Print(L"Network: DISABLED (setup)\n");
477:   }
478: 
479:   // Network self-check: report every real SNP interface and its transmit test.
480:   if (BootConfig.NetworkEnabled) {
481:     {
482:       PPC_NETWORK_INFO NetInfo;
483:       Status = PpcGetNetworkInfo(&NetInfo);
484:       if (!EFI_ERROR(Status)) {
485:         BOOLEAN AllPassed = NetInfo.InterfaceCount > 0;
486:         for (UINTN i = 0; i < NetInfo.InterfaceCount; i++) {
487:           Print(L"Network self-check: interface %d MAC %02x:%02x:%02x:%02x:%02x:%02x, "
488:                 L"media %s, transmit %s\n",
489:                 i,
490:                 NetInfo.Interfaces[i].MacAddress[0],
491:                 NetInfo.Interfaces[i].MacAddress[1],
492:                 NetInfo.Interfaces[i].MacAddress[2],
493:                 NetInfo.Interfaces[i].MacAddress[3],
494:                 NetInfo.Interfaces[i].MacAddress[4],
495:                 NetInfo.Interfaces[i].MacAddress[5],
496:                 NetInfo.Interfaces[i].MediaPresent ? L"present" : L"absent",
497:                 NetInfo.Interfaces[i].TransmitTestPassed ? L"PASS" : L"FAIL");
498:           if (!NetInfo.Interfaces[i].MediaPresent ||
499:               !NetInfo.Interfaces[i].TransmitTestPassed) {
500:             AllPassed = FALSE;
501:           }
502:         }
503:         Print(L"Network self-check: %s (%d interface(s))\n",
504:               AllPassed ? L"PASS" : L"FAIL", NetInfo.InterfaceCount);
505:       } else {
506:         Print(L"Network self-check: SKIP (no SNP interface)\n");
507:       }
508:     }
509:   }
510:   
511:   // Display system information
512:   Print(L"\n=== EFI-Mac-Emulator System Information ===\n");
513:   Print(L"UEFI Version: %d.%d\n", SystemTable->FirmwareRevision >> 16, SystemTable->FirmwareRevision & 0xFFFF);
514:   Print(L"System Table: 0x%x\n", SystemTable);
515:   Print(L"Image Handle: 0x%x\n", ImageHandle);
516:   
517:   // Get memory info
518:   PPC_MEMORY_INFO MemoryInfo;
519:   Status = PpcGetMemoryInfo(&MemoryInfo);
520:   if (!EFI_ERROR(Status)) {
521:     Print(L"Memory Base: 0x%x\n", MemoryInfo.BaseAddress);
522:     Print(L"Memory Size: %d bytes\n", MemoryInfo.Size);
523:   }
524:   
525:   // Get hardware info
526:   PPC_HARDWARE_INFO HardwareInfo;
527:   Status = PpcGetHardwareInfo(&HardwareInfo);
528:   if (!EFI_ERROR(Status)) {
529:     Print(L"Graphics Mode: 0x%x\n", HardwareInfo.GraphicsMode);
530:     Print(L"Audio Enabled: %d\n", HardwareInfo.AudioEnabled);
531:     Print(L"Storage Devices: %d\n", HardwareInfo.StorageDevices);
532:     Print(L"Network Interfaces: %d\n", HardwareInfo.NetworkInterfaces);
533:   }
534:   
535:   // Write a boot log entry to the ESP — real UEFI file I/O to the FAT drive.
536:   PpcDebugLogToFile(L"EFI-Mac-Emulator: boot self-test passed, initializing hardware\n");
537: 
538:   // Display emulator status
539:   Print(L"\nEFI-Mac-Emulator initialized successfully.\n");
540:   Print(L"Ready to load and boot classic Mac OS.\n");
541:   
542:   // Real kernel load from the FAT volume into guest RAM, then verify and
543:   // execute it — a full disk → RAM → execute pipeline.
544:   EFI_PHYSICAL_ADDRESS KernelAddress;
545:   UINT64 KernelSize;
546:   
547:   Print(L"\n--- Loading kernel from volume ---\n");
548:   Status = PpcLoadKernel(L"\\System\\MacOS\\kernel", &KernelAddress, &KernelSize);
549:   if (!EFI_ERROR(Status)) {
550:     Print(L"Kernel loaded at 0x%x (%d bytes)\n", KernelAddress, KernelSize);
551: 
552:     // Verify kernel integrity (bounds check + first word read).
553:     PpcVerifyKernel(KernelAddress, KernelSize);
554: 
555:     // Set boot parameters (from the saved/edited configuration).
556:     PPC_BOOT_PARAMETERS Params;
557:     Params.BootMode = BootConfig.BootMode;
558:     Params.MemorySizeMB = BootConfig.MemorySizeMB;
559:     Params.VideoMode = BootConfig.VideoMode;
560:     Params.EnableDebug = BootConfig.DebugEnabled;
561:     Params.CommandLine = L"console=serial";
562:     PpcSetBootParameters(&Params);
563:     Print(L"Boot parameters set successfully\n");
564: 
565:     // Execute the first 4 words of the loaded kernel image.
566:     Print(L"\n--- Executing loaded kernel image ---\n");
567:     UINT32 EntryPoint = (UINT32)KernelAddress;
568:     const UINTN KernelExecCount = 4;
569:     UINTN KernelExecuted = 0;
570:     Status = PpcExecuteBlock((UINT32*)(UINTN)EntryPoint, KernelExecCount, &KernelExecuted);
571:     BOOLEAN KernelRanClean = (Status == EFI_SUCCESS);
572: 
573:     UINT32 K_R3 = PpcGetGprValue(3);
574:     UINT32 K_R5 = PpcGetGprValue(5);
575: 
576:     Print(L"Loaded kernel executed: %d instructions (status %r)\n", KernelExecuted, Status);
577:     Print(L"r3=%d, r5=%d\n", K_R3, K_R5);
578: 
579:     if (KernelRanClean && KernelExecuted == KernelExecCount && K_R5 == 700) {
580:       Print(L"Loaded kernel execution: PASS\n");
581:     } else if (KernelRanClean && KernelExecuted == KernelExecCount) {
582:       Print(L"Loaded kernel execution: PARTIAL (ran but unexpected register state)\n");
583:     } else {
584:       Print(L"Loaded kernel execution: FAIL\n");
585:     }
586: 
587:     // Configure CPU context for boot.
588:     PpcBootSystem(KernelAddress, KernelSize);
589:   } else {
590:     Print(L"Kernel not found on volume (%r) — skipping kernel execution\n", Status);
591:   }
592:   
593:   // Phase 5: classic Mac OS boot memory map, ROM, and system initialization.
594:   {
595:     UINT64 LowMemAddress = 0, LowMemSize = 0;
596:     UINT64 RomAddress = 0, RomSize = 0;
597:     EFI_STATUS BootStatus;
598: 
599:     Print(L"\n--- Boot memory map / system initialization ---\n");
600: 
601:     // 1. Low-memory globals at guest 0x00000000 (16 KB, read/write).
602:     BootStatus = PpcInstallLowMemory(&LowMemAddress, &LowMemSize);
603:     Print(L"Low-memory region: %s (guest 0x%x, %d bytes)\n",
604:           EFI_ERROR(BootStatus) ? L"FAIL" : L"OK",
605:           (UINT32)LowMemAddress, (UINT64)LowMemSize);
606: 
607:     // 2. System ROM at guest 0xFFF00000. Load a real ROM image from the boot
608:     //    volume if present (the classic Mac OS "Mac OS ROM" file in the
609:     //    System Folder Extensions is the fallback); otherwise install a
610:     //    self-contained demo ROM so the full ROM -> guest-memory-map ->
611:     //    execution path is still exercised.
612:     BootStatus = PpcInstallSystemRom(PPC_ROM_DEFAULT_PATH, &RomAddress, &RomSize);
613:     if (EFI_ERROR(BootStatus) && BootStatus != EFI_ALREADY_STARTED) {
614:       if (BootStatus == EFI_NOT_FOUND) {
615:         Print(L"System ROM not found at '%s', trying Mac OS ROM file\n",
616:               PPC_ROM_DEFAULT_PATH);
617:         BootStatus = PpcInstallSystemRom(PPC_SYSTEM_FOLDER_ROM_PATH, &RomAddress, &RomSize);
618:       }
619:       if (EFI_ERROR(BootStatus) && BootStatus != EFI_ALREADY_STARTED) {
620:         if (BootStatus == EFI_NOT_FOUND) {
621:           Print(L"Mac OS ROM file not found, installing demo ROM\n");
622:         } else {
623:           Print(L"System ROM install failed (%r), installing demo ROM\n", BootStatus);
624:         }
625:         BootStatus = PpcInstallDemoRom(&RomAddress, &RomSize);
626:       }
627:     }
628:     Print(L"System ROM: %s (guest 0x%x, %d bytes)\n",
629:           EFI_ERROR(BootStatus) ? L"FAIL" : L"OK",
630:           (UINT32)RomAddress, (UINT64)RomSize);
631: 
632:     // 3. Self-test the memory map: ROM read-only, low memory R/W, and a
633:     //    cross-region ROM -> RAM program executed from the reset vector.
634:     BootStatus = PpcRunBootSelfTest();
635:     Print(L"Boot memory map self-test: %s\n",
636:           EFI_ERROR(BootStatus) ? L"FAIL" : L"PASS");
637: 
638:     // 4. Configure the CPU for entry at the ROM reset vector and write the
639:     //    boot info block into low memory.
640:     BootStatus = PpcPrepareSystemForBoot();
641:     Print(L"System initialization: %s\n",
642:           EFI_ERROR(BootStatus) ? L"FAIL" : L"PASS");
643: 
644:     // 4b. Continuous execution of a real installed ROM. New World Macs skip
645:     //     Open Firmware and enter the nanokernel boot routine directly
646:     //     (ROM base + 0x310000, the SheepShaver entry); Old World ROMs and the
647:     //     demo start at the CPU reset vector. Runs real firmware code through
648:     //     the interpreter until the first unimplemented opcode stops the guest
649:     //     (the current milestone).
650:     {
651:       PPC_BOOT_INFO RunInfo;
652:       if (!EFI_ERROR(PpcGetBootInfo(&RunInfo)) &&
653:           RunInfo.MemoryMap.RomInstalled &&
654:           RunInfo.MemoryMap.RomType != PPC_ROM_TYPE_DEMO &&
655:           RunInfo.MemoryMap.RomType != PPC_ROM_TYPE_UNKNOWN) {
656:         UINTN Executed = 0;
657:         EFI_STATUS RunStatus;
658: 
659:         if (RunInfo.MemoryMap.RomType == PPC_ROM_TYPE_NEW_WORLD) {
660:           g_PpcContext.Pc = (UINT32)RunInfo.MemoryMap.RomBase + PPC_NANOKERNEL_BOOT_OFFSET;
661:           // r3 = nanokernel code base: the NK entry's old-world replacement
662:           // path rfis to r3 + 0x40 (InitReplacement), so r3 must point at the
663:           // nanokernel image itself (ROM base + 0x310000).
664:           g_PpcContext.Gpr[3] = (UINT32)RunInfo.MemoryMap.RomBase + PPC_NANOKERNEL_BOOT_OFFSET;
665:           // r4 = nanokernel boot-workspace base (the boot entry's zero-loop,
666:           // stack r1 = r4+0xA000, guard-fill [r4, r4+0x1000), and the NK pool
667:           // FreeNext = r4+0x3008 / FreeList = r4+0x9BB0 all derive from it).
668:           // ISOLATION EXPERIMENT: keep r4 = 0 (the value the committed milestone
669:           // d98bc92 used) so the boot reaches the 0x13F handoff again; the r4 =
670:           // 0x10000 workspace move regressed the nanokernel area setup (all
671:           // CreateArea calls come back "skipped" and the boot task terminates in
672:           // the area-merge gap check at 0x40B1F67C).
673:           g_PpcContext.Gpr[4] = 0x00000000;
674:           // r5 = nanokernel output-device base. The NK boot printer polls
675:           // [base+2] bit 2 until set and writes output characters to [base+6].
676:           // Point it at a spare region of low memory pre-marked "ready" so the
677:           // poll completes; characters land harmlessly in RAM (no display yet).
678:           g_PpcContext.Gpr[5] = 0x00020000;
679:           PpcWriteGuestByte(0x00020000 + 2, 0x04);
680:           Print(L"  Outdev seed: [0x20002]=0x%02x (read-back)\n",
681:                 PpcReadGuestByte(0x00020000 + 2));
682:           // The NK InitReplacement reads a caller structure through SPRG4
683:           // (mfspr r11,sprg4) and copies [r11+4 .. r11+0x1000] onto its own
684:           // stack at [r1+4 .. r1+0x1000]. Version magic at [r11+0xFE4]:
685:           // 0x101 -> replaced (classic protocol), >= 0x200 -> NOT replaced.
686:           // Provide a structure in low RAM (0x30000, outside the NK's
687:           // guard-fill ranges) so the NK takes the replaced path.
688:           g_PpcContext.Spr[272] = 0x00030000;
689:           {
690:             UINT32 B = 0x00030000;
691:             PpcWriteGuestByte(B + 0xFE4, 0x01);
692:             PpcWriteGuestByte(B + 0xFE5, 0x01);
693:             PpcWriteGuestByte(B + 0x340, 0x00);
694:             PpcWriteGuestByte(B + 0x5B4, 0x00);
695:             PpcWriteGuestByte(B + 0x684 + 0, (UINT8)(B >> 24));
696:             PpcWriteGuestByte(B + 0x684 + 1, (UINT8)(B >> 16));
697:             PpcWriteGuestByte(B + 0x684 + 2, (UINT8)(B >> 8));
698:             PpcWriteGuestByte(B + 0x684 + 3, (UINT8)(B));
699:             PpcWriteGuestByte(B + 0x66C + 0, (UINT8)(B >> 24));
700:             PpcWriteGuestByte(B + 0x66C + 1, (UINT8)(B >> 16));
701:             PpcWriteGuestByte(B + 0x66C + 2, (UINT8)(B >> 8));
702:             PpcWriteGuestByte(B + 0x66C + 3, (UINT8)(B));
703:             PpcWriteGuestByte(B + 0x5E8 + 0, (UINT8)(B >> 24));
704:             PpcWriteGuestByte(B + 0x5E8 + 1, (UINT8)(B >> 16));
705:             PpcWriteGuestByte(B + 0x5E8 + 2, (UINT8)(B >> 8));
706:             PpcWriteGuestByte(B + 0x5E8 + 3, (UINT8)(B));
707:             // VMMaxVirtualPages/VMLogicalPages: the NK prints these from
708:             // caller[0x6B4]/[0x6B8] (copied to [r1+0x6B4]/[r1+0x6B8]). They are
709:             // 0 in the degenerate build -> zero virtual memory -> the PMDT gets
710:             // no RAM descriptors and the walk panics on duplicate [0,0xFFF].
711:             // Seed the page count for 256 MB of RAM (0x10000 pages @ 4 KB).
712:             PpcWriteGuestByte(B + 0x6B4 + 0, 0x00);
713:             PpcWriteGuestByte(B + 0x6B4 + 1, 0x00);
714:             PpcWriteGuestByte(B + 0x6B4 + 2, 0x01);
715:             PpcWriteGuestByte(B + 0x6B4 + 3, 0x00);
716:             PpcWriteGuestByte(B + 0x6B8 + 0, 0x00);
717:             PpcWriteGuestByte(B + 0x6B8 + 1, 0x00);
718:             PpcWriteGuestByte(B + 0x6B8 + 2, 0x01);
719:             PpcWriteGuestByte(B + 0x6B8 + 3, 0x00);
720:           }
721:           Print(L"  Seeded SPRG4 caller structure at 0x30000: version [0x30FE4]=0x0101\n");
722:           Print(L"\n--- Executing system ROM from nanokernel boot entry (0x%08x) ---\n",
723:                 g_PpcContext.Pc);
724:           {
725:             UINTN D;
726:             UINT32 DAddr[14] = {
727:               0x40B10000, 0x40B1000C, 0x40B10040, 0x40B10190,
728:               0x40B26440, 0x40B26B44, 0x40B28A74, 0x40B28A88,
729:               0x40B28BF0, 0x40B28C04, 0x40B32640, 0x40B32874,
730:               0x40B32888, 0x40B328F0
731:             };
732:             for (D = 0; D < 14; D++) {
733:               UINT32 W = ((UINT32)PpcReadGuestByte(DAddr[D]) << 24) |
734:                          ((UINT32)PpcReadGuestByte(DAddr[D] + 1) << 16) |
735:                          ((UINT32)PpcReadGuestByte(DAddr[D] + 2) << 8) |
736:                          ((UINT32)PpcReadGuestByte(DAddr[D] + 3));
737:               Print(L"  ROMDUMP[0x%08x] = 0x%08x\n", DAddr[D], W);
738:             }
739:           }
740:           {
741:             // The NK copies the SPRG4 caller structure [0x30004..0x31000] onto
742:             // its own stack at [r1+4..r1+0x1000] with r1=0xA000, and the boot
743:             // tail (NKInit.s: old_world_rfi_to_userspace_boot -> ReturnCode)
744:             // calls the emulator via `lwz r4,0x648(r1); ...; blrl`, i.e. it
745:             // blrls to KDP.LA_EmulatorKernelTrapTable [0xA648]. The seed must
746:             // therefore land in the caller structure, not at absolute 0x648,
747:             // so the copy delivers it to [0xA648].
748:             //
749:             // The real machine uses LA_EmulatorCode + KernelTrapTableOffset =
750:             // 0x68060000 + 0xE8C0 = 0x6806E8C0: the 68K emulator's kernel-trap
751:             // table (`twui r31,n`). PpcPatchNewWorldRom redirects
752:             // LA_EmulatorCode into the ROM window (ROM + 0x360000) and
753:             // overwrites that trap table with absolute branches to the
754:             // emulator-start routines (b 0x36f900..0x36fd00), so the boot
755:             // tail's blrl jumps straight into the 68K emulator without raising
756:             // a trap. Seeding 0x40B10000 (the old behaviour) re-entered the NK
757:             // boot entry instead, which rfi'd to r3+0x40 = 0x13F (guard-fill
758:             // -> crash).
759:             UINT32 ReturnTarget = PPC_EMULATOR_TRAP_TABLE;
760:             PpcWriteGuestByte(0x30000 + 0x648 + 0, (UINT8)(ReturnTarget >> 24));
761:             PpcWriteGuestByte(0x30000 + 0x648 + 1, (UINT8)(ReturnTarget >> 16));
762:             PpcWriteGuestByte(0x30000 + 0x648 + 2, (UINT8)(ReturnTarget >> 8));
763:             PpcWriteGuestByte(0x30000 + 0x648 + 3, (UINT8)(ReturnTarget));
764:             Print(L"  Seeded NK emulator-entry slot [0x30648] = 0x%08x "
765:                   L"(emulator kernel trap table)\n", ReturnTarget);
766:             // NOTE: do NOT pre-queue a nanodebugger "go" ('g' CR) here. The NK
767:             // polls the SCC during normal boot and drops into the nanodebugger
768:             // on any character, so an early 'g' derails boot. The interpreter's
769:             // AUTORESUME probe feeds 'g' only when the guest is actually idling
770:             // at the nanodebugger prompt (PC=0x40B2751C).
771:           }
772:           // The NK entry tests MSR bit 0x10 (rlwinm r0,r0,0,0x1b,0x1b at
773:           // 0x40B10014) and takes the cold/BAT path (beql 0x40B104A8) when
774:           // the bit is clear. PpcPrepareSystemForBoot left MSR = ME|RI
775:           // (0x1002), so bit 0x10 is clear. With it set, the fall-through
776:           // rfis to r3+0x40 and executes "crset cr5eq" (0x40B10040), so
777:           // CR5.EQ=1 at the 0x40B123A4 gate and the free-list walk is
778:           // skipped. PPC_MSR_DR is the emulator's label for bit 0x10.
779:           g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI | PPC_MSR_DR;
780:         } else {
781:           g_PpcContext.Pc = PPC_RESET_VECTOR;
782:           Print(L"\n--- Executing system ROM from reset vector ---\n");
783:         }
784:         RunStatus = PpcRunGuest(PPC_GUEST_STEP_BUDGET, TRUE, &Executed);
785:         Print(L"Guest execution stopped after %d instructions at PC=0x%08x: %r\n",
786:               Executed, g_PpcContext.Pc, RunStatus);
787:       }
788:     }
789: 
790:     // 5. Report the final boot state.
791:     PPC_BOOT_INFO BootInfo;
792:     if (!EFI_ERROR(PpcGetBootInfo(&BootInfo))) {
793:       Print(L"Boot state: ready=%d, kernel=%d, ROM at 0x%x (%d bytes, %s), "
794:             L"low mem at 0x%x (%d bytes)\n",
795:             BootInfo.SystemReady, BootInfo.KernelLoaded,
796:             (UINT32)BootInfo.MemoryMap.RomBase,
797:             (UINT64)BootInfo.MemoryMap.RomSize,
798:             BootRomTypeName(BootInfo.MemoryMap.RomType),
799:             (UINT32)BootInfo.MemoryMap.LowMemoryBase,
800:             (UINT64)BootInfo.MemoryMap.LowMemorySize);
801:       if (!BootInfo.MemoryMap.RomInstalled) {
802:         Print(L"NOTE: no system ROM is installed; the demo ROM will not boot a real OS.\n");
803:       } else if (BootInfo.MemoryMap.RomType == PPC_ROM_TYPE_DEMO) {
804:         Print(L"NOTE: running the demo ROM. For a real boot, place a ROM image on the\n"
805:               L"  boot volume at \\System\\MacOS\\ROM (Old World dump) or\n"
806:               L"  \\System Folder\\Extensions\\Mac OS ROM (New World file), or attach a\n"
807:               L"  Mac OS 8.5+ disc that contains the 'Mac OS ROM' file.\n");
808:       }
809:     }
810:   }
811: 
812:   // Phase 5: classic Mac OS system files and drivers (System Folder support).
813:   {
814:     PPC_SYSTEM_FOLDER_INFO SysInfo;
815:     EFI_STATUS SysStatus;
816: 
817:     Print(L"\n--- System files and drivers ---\n");
818: 
819:     // 1. Scan the boot volume for the System Folder and its components.
820:     ZeroMem(&SysInfo, sizeof(SysInfo));
821:     SysStatus = PpcLocateSystemFolder(&SysInfo);
822:     if (EFI_ERROR(SysStatus) || !SysInfo.Found) {
823:       Print(L"System Folder not found on volume (scan: %r)\n", SysStatus);
824:     } else {
825:       Print(L"System Folder found: %s\n", SysInfo.Path);
826:       Print(L"  System=%d, Finder=%d, Extensions=%d, Mac OS ROM=%d\n",
827:             SysInfo.SystemPresent, SysInfo.FinderPresent,
828:             SysInfo.ExtensionsPresent, SysInfo.MacOsRomPresent);
829: 
830:       // 2. Stage the System file, Finder, and Mac OS ROM into guest memory.
831:       SysStatus = PpcLoadSystemFiles();
832:       Print(L"System files staged: %s\n",
833:             EFI_ERROR(SysStatus) ? L"FAIL" : L"OK");
834: 
835:       // 3. Enumerate Extensions and stage the drivers.
836:       SysStatus = PpcScanExtensionsDirectory();
837:       Print(L"Extensions scanned: %s\n",
838:             EFI_ERROR(SysStatus) ? L"FAIL" : L"OK");
839:       SysStatus = PpcLoadDrivers();
840:       Print(L"Drivers staged: %s\n",
841:             EFI_ERROR(SysStatus) ? L"FAIL" : L"OK");
842:     }
843: 
844:     // 4. Self-test: staged files read back through the interpreter memory path.
845:     SysStatus = PpcRunSystemFilesSelfTest();
846:     Print(L"System files self-test: %s\n",
847:           EFI_ERROR(SysStatus) ? L"FAIL" : L"PASS");
848: 
849:     // 5. Report the final staging state.
850:     if (!EFI_ERROR(PpcGetSystemFolderInfo(&SysInfo))) {
851:       Print(L"System files: %d staged, %d drivers registered (%d staged), "
852:             L"%d bytes total\n",
853:             SysInfo.LoadedFileCount, SysInfo.DriverCount,
854:             SysInfo.LoadedDriverCount, SysInfo.TotalStagedBytes);
855:       if (SysInfo.SystemAreaBase != 0) {
856:         Print(L"Staging areas: system 0x%x, drivers 0x%x\n",
857:               (UINT32)SysInfo.SystemAreaBase, (UINT32)SysInfo.DriverAreaBase);
858:       }
859:     }
860:   }
861: 
862:   Print(L"\n=== EFI Mac OS Boot Layer ready ===\n");
863:   
864:   return EFI_SUCCESS;
865: }

(End of file - total 865 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu</path>
<type>directory</type>
<entries>
interpreter.c
interpreter.h
m68k.c
m68k.h
translation_impl.c
translation.h

(6 entries)
</entries>
```

---

## Assistant (Build · Big Pickle · 5.9s)

_Thinking:_

Now I have a good understanding of the project. Let me read the key source files to understand what's implemented:

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
22:     UINT32  TimeBaseL;      // Time base lower (TBL)
23:     UINT32  TimeBaseH;      // Time base upper (TBU)
24:     UINT32  DecrementerNegative;  // 1 while DEC (SPR 22) is negative
25:     UINT32  DecrementerWritten;   // 1 once the guest has armed DEC via mtspr
26:     UINT64  Fpr[32];        // Floating-point registers (IEEE-754 double bit patterns)
27:     UINT32  Fpscr;          // Floating-point status/control register (classic 32-bit layout)
28:     UINT8   Vr[32][16];     // AltiVec vector registers VR0..VR31 (big-endian byte order)
29:     UINT32  Vscr;           // Vector status/control register
30:     UINT32  ExceptionPending;  // 0 = none, else PPC_EXCEPTION_*
31: } PPC_CPU_CONTEXT;
32: 
33: // Global CPU context
34: extern PPC_CPU_CONTEXT g_PpcContext;
35: 
36: // Update a single 4-bit Condition Register field
37: VOID
38: PpcSetCrField (
39:     IN UINT32 Field,
40:     IN UINT32 Value
41:     );
42: 
43: // Read a single 4-bit Condition Register field
44: UINT32
45: PpcGetCrField (
46:     IN UINT32 Field
47:     );
48: 
49: // Set XER[CA] (carry) to the given value, preserving other XER bits
50: VOID
51: PpcSetXerCarry (
52:     IN UINT32 Carry
53:     );
54: 
55: // Set XER[OV] and, if set, the sticky XER[SO] bit
56: VOID
57: PpcSetXerOverflow (
58:     IN UINT32 Overflow
59:     );
60: 
61: // Queue a byte on the NK's emulated SCC receive side
62: VOID
63: PpcSccPutChar (
64:     IN UINT8 Char
65:     );
66: 
67: #endif // __PPC_INTERPRETER_H__

(End of file - total 67 lines)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\m68k.h"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\m68k.h</path>
<type>file</type>
<content>
1: #ifndef __M68K_INTERPRETER_H__
2: #define __M68K_INTERPRETER_H__
3: 
4: #include <efi.h>
5: 
6: // ---------------------------------------------------------------------------
7: // Motorola 68000 CPU context
8: //
9: // The 68K registers as the New World ROM's DR emulator expects them. The
10: // SheepShaver DR emulator maps 68K registers to PPC registers: D0-D7 = r8-r15,
11: // A0-A6 = r16-r22, A7 (USP) = r1. Our native interpreter keeps them in a
12: // dedicated context struct that lives alongside the PPC context.
13: // ---------------------------------------------------------------------------
14: typedef struct {
15:     // Data registers D0-D7 (32-bit, accessible as byte/halfword/word)
16:     UINT32  D[8];
17: 
18:     // Address registers A0-A7 (32-bit, A7 is the USP; SSP tracked separately)
19:     UINT32  A[8];
20: 
21:     // Supervisor stack pointer (A7 in supervisor mode)
22:     UINT32  SSP;
23: 
24:     // Program counter (32-bit, but 68000 uses only lower 24 bits)
25:     UINT32  PC;
26: 
27:     // Status register (16-bit):
28:     //   bits 15-8: system byte (T1/T0/S/I2/I0)
29:     //   bits  7-0: CCR (X/N/Z/V/C/0/0/0)
30:     UINT16  SR;
31: 
32:     // Current instruction word (for debugging/tracing)
33:     UINT32  CurrentOpcode;
34: 
35:     // 1 = supervisor mode, 0 = user mode
36:     BOOLEAN Supervisor;
37: 
38:     // Set to TRUE when the interpreter encounters an unimplemented opcode
39:     // or an exception; PpcRunGuest can check this to stop.
40:     BOOLEAN Halted;
41: } M68K_CPU_CONTEXT;
42: 
43: // Global 68K CPU context
44: extern M68K_CPU_CONTEXT g_M68kContext;
45: 
46: // ---------------------------------------------------------------------------
47: // CCR flag bit positions (in the low byte of SR)
48: // ---------------------------------------------------------------------------
49: #define M68K_CCR_C  0x01    // Carry
50: #define M68K_CCR_V  0x02    // Overflow
51: #define M68K_CCR_Z  0x04    // Zero
52: #define M68K_CCR_N  0x08    // Negative
53: #define M68K_CCR_X  0x10    // Extend
54: 
55: // SR system byte bits
56: #define M68K_SR_T1   0x8000  // Trace mode 1
57: #define M68K_SR_T0   0x4000  // Trace mode 0
58: #define M68K_SR_S    0x2000  // Supervisor (1) / User (0)
59: #define M68K_SR_I2   0x1000  // Interrupt mask bit 2
60: #define M68K_SR_I1   0x0800  // Interrupt mask bit 1
61: #define M68K_SR_I0   0x0400  // Interrupt mask bit 0
62: 
63: // ---------------------------------------------------------------------------
64: // 68K exception vector numbers
65: // ---------------------------------------------------------------------------
66: #define M68K_VEC_RESET_SSP     0x00   // Initial SSP
67: #define M68K_VEC_RESET_PC      0x04   // Initial PC
68: #define M68K_VEC_BUS_ERROR     0x08
69: #define M68K_VEC_ADDRESS_ERROR 0x0C
70: #define M68K_VEC_ILLEGAL_INSTR 0x10
71: #define M68K_VEC_ZERO_DIVIDE   0x14
72: #define M68K_VEC_CHK           0x18
73: #define M68K_VEC_TRAPV         0x1C
74: #define M68K_VEC_PRIV_VIOLATION 0x20
75: #define M68K_VEC_TRACE         0x24
76: #define M68K_VEC_LINE_1010     0x28
77: #define M68K_VEC_LINE_1111     0x2C
78: #define M68K_VEC_FORMAT_ERROR  0x38
79: #define M68K_VEC_UNINITIALIZED 0x3C
80: #define M68K_VEC_SPURIOUS      0x60
81: #define M68K_VEC_LEVEL1        0x64
82: #define M68K_VEC_LEVEL2        0x68
83: #define M68K_VEC_LEVEL3        0x6C
84: #define M68K_VEC_LEVEL4        0x70
85: #define M68K_VEC_LEVEL5        0x74
86: #define M68K_VEC_LEVEL6        0x78
87: #define M68K_VEC_LEVEL7        0x7C
88: #define M68K_VEC_TRAP0         0x80
89: #define M68K_VEC_TRAP1         0x84  // Mac OS system call (TRAP #1)
90: #define M68K_VEC_TRAP(n)       (0x80 + (n) * 4)
91: 
92: // Mac OS A-line trap: TRAP #A (line F = 0xA). These are the Toolbox calls.
93: #define M68K_VEC_ALINE         0xB0
94: 
95: // ---------------------------------------------------------------------------
96: // Public API
97: // ---------------------------------------------------------------------------
98: 
99: // Initialize the 68K CPU context to reset state (reads reset vector from
100: // guest memory at 0x0000/0x0004).
101: VOID
102: M68kInitialize (
103:     VOID
104:     );
105: 
106: // Reset the 68K CPU: reload PC and SSP from the vector table at 0x0000,
107: // set SR to supervisor mode with interrupts masked (0x2700).
108: VOID
109: M68kReset (
110:     VOID
111:     );
112: 
113: // Execute a single 68K instruction from the current PC. Returns the number
114: // of cycles consumed (approximate). Sets g_M68kContext.Halted on fatal error.
115: // The PC is advanced past the instruction.
116: UINT32
117: M68kExecuteInstruction (
118:     VOID
119:     );
120: 
121: // Execute multiple 68K instructions (up to MaxInstructions). Returns the
122: // number of instructions actually executed. Stops early if Halted is set.
123: UINTN
124: M68kExecuteBlock (
125:     IN UINTN MaxInstructions
126:     );
127: 
128: // Called from the PPC interpreter when the PPC code enters the 68K opcode
129: // table (the trampoline). This reads the 68K opcode from guest memory at
130: // the 68K PC, decodes and executes it, then returns to the PPC DR emulator
131: // loop. Returns EFI_SUCCESS on success, or an error on fatal exception.
132: EFI_STATUS
133: M68kExecuteFromPPC (
134:     VOID
135:     );
136: 
137: // Patch the 68K opcode table (ROM+0x380000) so that regular 68K opcode
138: // entries branch to the C trampoline instead of the PPC DR emulator code.
139: // EMUL_OP entries (0xFE40+) are left as-is (they use the mulli marker
140: // interception already in the PPC interpreter).
141: VOID
142: M68kPatchOpcodeTable (
143:     VOID
144:     );
145: 
146: // Read a byte from the 68K address space (uses the PPC guest memory path).
147: UINT8
148: M68kReadByte (
149:     IN UINT32 Address
150:     );
151: 
152: // Read a word (16-bit, big-endian) from the 68K address space.
153: UINT16
154: M68kReadWord (
155:     IN UINT32 Address
156:     );
157: 
158: // Read a long (32-bit, big-endian) from the 68K address space.
159: UINT32
160: M68kReadLong (
161:     IN UINT32 Address
162:     );
163: 
164: // Write a byte to the 68K address space.
165: VOID
166: M68kWriteByte (
167:     IN UINT32 Address,
168:     IN UINT8  Value
169:     );
170: 
171: // Write a word (16-bit, big-endian) to the 68K address space.
172: VOID
173: M68kWriteWord (
174:     IN UINT32 Address,
175:     IN UINT16 Value
176:     );
177: 
178: // Write a long (32-bit, big-endian) to the 68K address space.
179: VOID
180: M68kWriteLong (
181:     IN UINT32 Address,
182:     IN UINT32 Value
183:     );
184: 
185: // Read a word from the 68K address space without advancing PC (for
186: // prefetch/peek operations).
187: UINT16
188: M68kFetchWord (
189:     IN UINT32 Address
190:     );
191: 
192: // Read a long from the 68K address space without advancing PC.
193: UINT32
194: M68kFetchLong (
195:     IN UINT32 Address
196:     );
197: 
198: // Push a word onto the 68K supervisor stack.
199: VOID
200: M68kPushWord (
201:     IN UINT16 Value
202:     );
203: 
204: // Push a long onto the 68K supervisor stack.
205: VOID
206: M68kPushLong (
207:     IN UINT32 Value
208:     );
209: 
210: // Pop a word from the 68K supervisor stack.
211: UINT16
212: M68kPopWord (
213:     VOID
214:     );
215: 
216: // Pop a long from the 68K supervisor stack.
217: UINT32
218: M68kPopLong (
219:     VOID
220:     );
221: 
222: // Set a CCR flag bit.
223: VOID
224: M68kSetFlag (
225:     IN UINT8 Flag
226:     );
227: 
228: // Clear a CCR flag bit.
229: VOID
230: M68kClearFlag (
231:     IN UINT8 Flag
232:     );
233: 
234: // Test a CCR flag bit (returns TRUE if set).
235: BOOLEAN
236: M68kTestFlag (
237:     IN UINT8 Flag
238:     );
239: 
240: // Set X, N, Z, V, C flags based on a result value.
241: // ResultSize: 1 = byte, 2 = word, 4 = long
242: VOID
243: M68kSetFlagsFromResult (
244:     IN UINT32 Result,
245:     IN UINT8  ResultSize
246:     );
247: 
248: // Set CCR from a subtraction result (result, src, dst) for proper
249: // carry/overflow computation.
250: VOID
251: M68kSetFlagsFromSub (
252:     IN UINT32 Result,
253:     IN UINT32 Src,
254:     IN UINT32 Dst,
255:     IN UINT8  Size
256:     );
257: 
258: // Set CCR from an addition result (result, src, dst).
259: VOID
260: M68kSetFlagsFromAdd (
261:     IN UINT32 Result,
262:     IN UINT32 Src,
263:     IN UINT32 Dst,
264:     IN UINT8  Size
265:     );
266: 
267: // Get the current SR (status register, 16-bit).
268: UINT16
269: M68kGetSR (
270:     VOID
271:     );
272: 
273: // Set the SR (status register, 16-bit). Handles supervisor/user stack
274: // switching if the S bit changes.
275: VOID
276: M68kSetSR (
277:     IN UINT16 Value
278:     );
279: 
280: // Get the CCR (condition code register, low 8 bits of SR).
281: UINT8
282: M68kGetCCR (
283:     VOID
284:     );
285: 
286: // Set the CCR (condition code register, low 8 bits of SR).
287: VOID
288: M68kSetCCR (
289:     IN UINT8 Value
290:     );
291: 
292: // Raise a 68K exception. Pushes the current PC and SR onto the
293: // supervisor stack, then vectors through the exception table.
294: VOID
295: M68kRaiseException (
296:     IN UINT8 VectorNumber
297:     );
298: 
299: // Decode a single 68K instruction to a mnemonic string.
300: VOID
301: M68kDecodeInstruction (
302:     IN  UINT16 Opcode,
303:     OUT CHAR16* Buffer,
304:     IN  UINTN   BufferSize
305:     );
306: 
307: // Synchronize 68K CPU context from PPC interpreter registers.
308: // Maps: PPC r8-r15 -> D0-D7, r16-r22 -> A0-A6, r1 -> A7, r24 -> PC, r25 -> SR
309: VOID
310: M68kSyncFromPPC (
311:     VOID
312:     );
313: 
314: // Synchronize PPC interpreter registers from 68K CPU context (reverse of above).
315: VOID
316: M68kSyncToPPC (
317:     VOID
318:     );
319: 
320: // Run the 68K self-test suite.
321: EFI_STATUS
322: M68kRunSelfTest (
323:     VOID
324:     );
325: 
326: #endif // __M68K_INTERPRETER_H__

(End of file - total 326 lines)
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
20: // Classic Mac OS PPC boot memory map (guest-visible addresses)
21: #define PPC_ROM_GUEST_BASE      0xFFF00000  // ROM window (classic PPC Macs)
22: #define PPC_ROM_MAX_SIZE        0x00400000  // 4 MB ROM window
23: #define PPC_ROM_DEFAULT_PATH    L"\\System\\MacOS\\ROM"
24: #define PPC_RESET_VECTOR        (PPC_ROM_GUEST_BASE + 0x100)
25: #define PPC_NANOKERNEL_BOOT_OFFSET 0x310000  // New World nanokernel boot entry (SheepShaver)
26: // The 68K emulator's kernel-trap table, as patched by PpcPatchNewWorldRom:
27: // LA_EmulatorCode is redirected from the baked 0x68060000 (RAM) to the ROM
28: // window at ROM base + 0x360000, so LA_EmulatorCode + KernelTrapTableOffset
29: // (0xE8C0) = ROM base + 0x36E8C0 = 0x40B6E8C0. The nanokernel's boot tail
30: // blrls to this address; the original `twui r31,0` dispatch table there is
31: // patched to `b 0x36f900` (the emulator-start routine), so no trap fires.
32: #define PPC_NEW_WORLD_ROM_LA_EMULCODE_BASE (PPC_NEW_WORLD_ROM_GUEST_BASE + 0x360000)
33: #define PPC_NEW_WORLD_ROM_KERNEL_TRAP_TABLE_OFFSET 0xE8C0
34: #define PPC_EMULATOR_TRAP_TABLE (PPC_NEW_WORLD_ROM_LA_EMULCODE_BASE + PPC_NEW_WORLD_ROM_KERNEL_TRAP_TABLE_OFFSET)
35: 
36: // SheepShaver-faithful 68K emulator activation (New World ROM only).
37: // PpcPatchNewWorldRom writes:
38: //  - the ROM boot structure (ROM + 0x30d000): LA_InfoRecord/LA_KernelData/
39: //    LA_EmulatorData = 0x68ffe000/0x68fff000, LA_DispatchTable = ROM + 0x380000,
40: //    LA_EmulatorCode = ROM + 0x360000, physical RAM base = 0, 68K reset vector
41: //    = ROM + 0x2a;
42: //  - the kernel-trap table (ROM + 0x36E8C0, the `twui r31,n` table) as
43: //    absolute branches to the emulator-entry routines at ROM + 0x36F900..0x36FD00;
44: //  - those entry routines (SheepShaver's emulator-start/MixedMode/Reset/FC1E/
45: //    FE0A/FE0F fragments, 27 words each), with the emulator-start fragment's
46: //    final `blr` redirected to the injected 68K DR-emulator entry routine
47: //    (ROM + 0x36f700, SheepShaver's execute_68k contract: full 68K context +
48: //    first opcode dispatch) and the ed.v[0x814] dispatch helper (ROM + 0x36f7c0);
49: //  - the 68K opcode dispatch table (ROM + 0x380000, one 8-byte entry per 16-bit
50: //    opcode): slots for the EMUL_OP extended opcodes (0xFE40..0xFE40+OP_MAX+2)
51: //    at ROM + 0x3FF200 become `POWERPC_EMUL_OP | n` markers ("mulli r0,r0,n")
52: //    followed by `b 0x366084` (re-enter the DR emulator loop). The interpreter
53: //    intercepts those markers (see PpcEmulatorDispatchOp in interpreter.c).
54: #define PPC_NEW_WORLD_ROM_BOOT_STRUCT_OFFSET     0x30D000
55: #define PPC_NEW_WORLD_ROM_TRAP_TABLE_OFFSET      0x36E8C0
56: #define PPC_NEW_WORLD_ROM_EMUL_START_OFFSET      0x36F900
57: #define PPC_NEW_WORLD_ROM_DISPATCH_TABLE_OFFSET  0x380000
58: #define PPC_NEW_WORLD_ROM_EMUL_OP_ENTRY_OFFSET   0x3FF200  // (0xFE40 << 3)
59: #define PPC_NEW_WORLD_ROM_EMUL_OP_END_OFFSET     0x3FF400
60: #define PPC_EMUL_OP_MARKER                       0x18000000  // mulli r0,r0,n (n = marker)
61: #define PPC_EMUL_OP_DISPATCH_GUEST_BASE (PPC_NEW_WORLD_ROM_GUEST_BASE + PPC_NEW_WORLD_ROM_EMUL_OP_ENTRY_OFFSET)
62: #define PPC_EMUL_OP_DISPATCH_GUEST_END  (PPC_NEW_WORLD_ROM_GUEST_BASE + PPC_NEW_WORLD_ROM_EMUL_OP_END_OFFSET)
63: 
64: // XLM ("eXtra Low Memory") globals written by the bootloader and consumed by the
65: // 68K emulator entry routines (SheepShaver's xlowmem.h layout). They live above
66: // the 0x0-0x2000 low-memory region the nanokernel zeroes during boot.
67: #define PPC_XLM_SIGNATURE_OFFSET   0x2800  // 'Baah'
68: #define PPC_XLM_KERNEL_DATA_OFFSET 0x2804  // r1 the emulator switches to
69: #define PPC_XLM_TOC_OFFSET         0x2808
70: #define PPC_XLM_SHEEP_OBJ_OFFSET   0x280C
71: #define PPC_XLM_RUN_MODE_OFFSET    0x2810  // MODE_68K=0, MODE_NATIVE=1, MODE_EMUL_OP=2
72: #define PPC_XLM_68K_R25_OFFSET     0x2814  // native 68K PC slot
73: #define PPC_XLM_IRQ_NEST_OFFSET    0x2818
74: #define PPC_XLM_PVR_OFFSET         0x281C
75: #define PPC_XLM_BUS_CLOCK_OFFSET   0x2820
76: 
77: // OP_* selectors for the EMUL_OP markers (SheepShaver's emul_op.h order). The
78: // marker n in `mulli r0,r0,n` maps to EmulOp(selector) = n - 3 for n >= 3.
79: enum {
80:     PPC_OP_BREAK = 0,
81:     PPC_OP_XPRAM1, PPC_OP_XPRAM2, PPC_OP_XPRAM3,
82:     PPC_OP_NVRAM1, PPC_OP_NVRAM2, PPC_OP_NVRAM3,
83:     PPC_OP_FIX_MEMTOP, PPC_OP_FIX_MEMSIZE, PPC_OP_FIX_BOOTSTACK,
84:     PPC_OP_SONY_OPEN, PPC_OP_SONY_PRIME, PPC_OP_SONY_CONTROL, PPC_OP_SONY_STATUS,
85:     PPC_OP_DISK_OPEN, PPC_OP_DISK_PRIME, PPC_OP_DISK_CONTROL, PPC_OP_DISK_STATUS,
86:     PPC_OP_CDROM_OPEN, PPC_OP_CDROM_PRIME, PPC_OP_CDROM_CONTROL, PPC_OP_CDROM_STATUS,
87:     PPC_OP_AUDIO_DISPATCH,
88:     PPC_OP_SOUNDIN_OPEN, PPC_OP_SOUNDIN_PRIME, PPC_OP_SOUNDIN_CONTROL,
89:     PPC_OP_SOUNDIN_STATUS, PPC_OP_SOUNDIN_CLOSE,
90:     PPC_OP_ADBOP,
91:     PPC_OP_INSTIME, PPC_OP_RMVTIME, PPC_OP_PRIMETIME, PPC_OP_MICROSECONDS,
92:     PPC_OP_ZERO_SCRAP, PPC_OP_PUT_SCRAP, PPC_OP_GET_SCRAP,
93:     PPC_OP_DEBUG_STR, PPC_OP_INSTALL_DRIVERS, PPC_OP_NAME_REGISTRY,
94:     PPC_OP_RESET, PPC_OP_IRQ,
95:     PPC_OP_SCSI_DISPATCH, PPC_OP_SCSI_ATOMIC,
96:     PPC_OP_CHECK_SYSV, PPC_OP_NTRB_17_PATCH, PPC_OP_NTRB_17_PATCH2,
97:     PPC_OP_NTRB_17_PATCH3, PPC_OP_NTRB_17_PATCH4, PPC_OP_CHECKLOAD,
98:     PPC_OP_EXTFS_COMM, PPC_OP_EXTFS_HFS,
99:     PPC_OP_IDLE_TIME, PPC_OP_IDLE_TIME_2,
100:     PPC_OP_MAX
101: };
102: #define PPC_GUEST_STEP_BUDGET   150000000   // Continuous-run instruction budget
103: #define PPC_LOW_MEM_GUEST_BASE  0x00000000  // Low-memory globals
104: #define PPC_LOW_MEM_SIZE        0x00040000  // 256 KB (covers the nanokernel's fixed stack/context at 0xA000-0x1A000)
105: 
106: // Classic Mac OS PPC kernel/system area. The nanokernel hard-codes its kernel
107: // stack/heap in the 0x68F0xxxx range (e.g. addis/ori 0x68F168F1 at the boot
108: // entry and in the first-task setup), which is the standard 0x68000000 system
109: // area on every New World Mac. The emulator has no MMU/BAT translation, so the
110: // logical area is backed directly with guest RAM.
111: #define PPC_NK_SYSTEM_AREA_GUEST_BASE 0x68000000
112: #define PPC_NK_SYSTEM_AREA_SIZE       0x08000000  // 128 MB
113: 
114: // New World "Mac OS ROM" images are 4 MB but boot from offset 0x310000. A 4 MB
115: // window at PPC_ROM_GUEST_BASE (the top of the 32-bit space) can only address
116: // offsets < 1 MB, so New World ROMs are mapped lower, at SheepShaver's
117: // ROM_BASE (0x40800000); ROM base + 0x310000 = 0x40B10000 is the boot entry.
118: #define PPC_NEW_WORLD_ROM_GUEST_BASE 0x40800000
119: 
120: // Which kind of system ROM is installed. Old World is a classic PowerPC
121: // firmware dump (System 7 through early Mac OS 8); New World is the
122: // "Mac OS ROM" file (Mac OS 8.5 and later, CHRP-style <CHRP-BOOT> header);
123: // DEMO is the self-contained fallback image with no Mac firmware.
124: #define PPC_ROM_TYPE_UNKNOWN    0
125: #define PPC_ROM_TYPE_OLD_WORLD  1
126: #define PPC_ROM_TYPE_NEW_WORLD  2
127: #define PPC_ROM_TYPE_DEMO       3
128: 
129: // Low-memory global offsets (emulator-defined boot info block)
130: #define PPC_LOW_MEM_MAGIC_OFFSET    0x0000
131: #define PPC_LOW_MEM_BOOTINFO_OFFSET 0x0100
132: 
133: // System Folder layout on the boot volume (classic Mac OS)
134: #define PPC_SYSTEM_FOLDER_PATH      L"\\System Folder"
135: #define PPC_SYSTEM_FILE_PATH        L"\\System Folder\\System"
136: #define PPC_FINDER_FILE_PATH        L"\\System Folder\\Finder"
137: #define PPC_EXTENSIONS_DIR_PATH     L"\\System Folder\\Extensions"
138: #define PPC_SYSTEM_FOLDER_ROM_PATH  L"\\System Folder\\Extensions\\Mac OS ROM"
139: 
140: // HFS volume paths (Mac-style ':' separators) used when the System Folder is
141: // read from an attached Mac OS disc through the in-emulator HFS reader instead
142: // of the FAT boot volume.
143: #define PPC_HFS_SYSTEM_FOLDER_PATH  L"System Folder"
144: #define PPC_HFS_SYSTEM_FILE_PATH    L"System Folder:System"
145: #define PPC_HFS_FINDER_FILE_PATH    L"System Folder:Finder"
146: #define PPC_HFS_ROM_FILE_PATH       L"System Folder:Extensions:Mac OS ROM"
147: 
148: // Guest staging areas for system files and drivers
149: #define PPC_SYSTEM_AREA_GUEST_BASE  0x20000000  // System + Finder + Mac OS ROM
150: #define PPC_SYSTEM_AREA_SIZE        0x01000000  // 16 MB
151: #define PPC_DRIVER_AREA_GUEST_BASE  0x21000000  // Extensions (drivers)
152: #define PPC_DRIVER_AREA_SIZE        0x02000000  // 32 MB
153: 
154: // Limits for the system file / driver registry
155: #define PPC_SYSTEM_FOLDER_PATH_MAX  256
156: #define PPC_SYSTEM_FILE_NAME_MAX    64
157: #define PPC_SYSTEM_FILE_PATH_MAX    260
158: #define PPC_MAX_SYSTEM_FILES        6
159: #define PPC_MAX_DRIVERS             64
160: 
161: // Types of classic Mac OS system files
162: typedef enum {
163:     PPC_SYSTEM_FILE_TYPE_UNKNOWN = 0,
164:     PPC_SYSTEM_FILE_TYPE_SYSTEM,   // System file
165:     PPC_SYSTEM_FILE_TYPE_FINDER,   // Finder
166:     PPC_SYSTEM_FILE_TYPE_ROM,      // Mac OS ROM file
167:     PPC_SYSTEM_FILE_TYPE_DRIVER    // extension in the Extensions folder
168: } PPC_SYSTEM_FILE_TYPE;
169: 
170: // A staged system file or driver
171: typedef struct {
172:     PPC_SYSTEM_FILE_TYPE Type;
173:     BOOLEAN Loaded;
174:     CHAR16  Name[PPC_SYSTEM_FILE_NAME_MAX];
175:     CHAR16  Path[PPC_SYSTEM_FILE_PATH_MAX];
176:     UINT64  FileSize;      // size on disk
177:     UINT64  GuestAddress;  // guest address where staged (0 if not loaded)
178:     UINT64  StagedSize;    // bytes staged into guest memory
179:     UINT32  HfsId;         // catalog ID when sourced from the HFS reader (0 otherwise)
180: } PPC_SYSTEM_FILE;
181: 
182: // Aggregate report of the System Folder scan / staging results
183: typedef struct {
184:     BOOLEAN Found;
185:     CHAR16  Path[PPC_SYSTEM_FOLDER_PATH_MAX];
186:     BOOLEAN SystemPresent;
187:     BOOLEAN FinderPresent;
188:     BOOLEAN ExtensionsPresent;
189:     BOOLEAN MacOsRomPresent;
190:     UINTN   FileCount;
191:     UINTN   LoadedFileCount;
192:     UINTN   DriverCount;
193:     UINTN   LoadedDriverCount;
194:     UINT64  TotalStagedBytes;
195:     UINT64  SystemAreaBase;
196:     UINT64  DriverAreaBase;
197: } PPC_SYSTEM_FOLDER_INFO;
198: 
199: // Guest memory map as installed for a classic Mac OS boot
200: typedef struct {
201:     BOOLEAN RomInstalled;
202:     UINT64  RomBase;
203:     UINT64  RomSize;
204:     UINT32  RomType;           // PPC_ROM_TYPE_* (OLD_WORLD / NEW_WORLD / DEMO)
205:     BOOLEAN LowMemoryInstalled;
206:     UINT64  LowMemoryBase;
207:     UINT64  LowMemorySize;
208:     BOOLEAN Ready;
209: } PPC_GUEST_MEMORY_MAP;
210: 
211: // Boot information structure
212: typedef struct {
213:     BOOLEAN IsInitialized;
214:     EFI_PHYSICAL_ADDRESS KernelAddress;
215:     UINT64 KernelSize;
216:     BOOLEAN KernelLoaded;
217:     BOOLEAN SystemReady;
218:     PPC_GUEST_MEMORY_MAP MemoryMap;
219:     PPC_SYSTEM_FOLDER_INFO SystemFolder;
220: } PPC_BOOT_INFO;
221: 
222: /**
223:   Initialize PowerPC bootloader
224:   @retval EFI_STATUS
225: **/
226: EFI_STATUS
227: EFIAPI
228: PpcInitializeBootloader (
229:     VOID
230:     );
231: 
232: /**
233:   Load kernel image into memory
234:   @param[in]  ImagePath      Path to the kernel image
235:   @param[out] KernelAddress  Pointer to store kernel load address
236:   @param[out] KernelSize     Pointer to store kernel size
237:   @retval EFI_STATUS
238: **/
239: EFI_STATUS
240: EFIAPI
241: PpcLoadKernel (
242:     IN  CHAR16* ImagePath,
243:     OUT EFI_PHYSICAL_ADDRESS* KernelAddress,
244:     OUT UINT64* KernelSize
245:     );
246: 
247: /**
248:   Boot the PowerPC system
249:   @param[in] KernelAddress Address of kernel to boot
250:   @param[in] KernelSize    Size of kernel to boot
251:   @retval EFI_STATUS
252: **/
253: EFI_STATUS
254: EFIAPI
255: PpcBootSystem (
256:     IN EFI_PHYSICAL_ADDRESS KernelAddress,
257:     IN UINT64               KernelSize
258:     );
259: 
260: /**
261:   Load a boot image into memory
262:   @param[in]  ImagePath   Path to the boot image
263:   @param[out] ImageBuffer Pointer to store image buffer address
264:   @param[out] ImageSize   Pointer to store image size
265:   @retval EFI_STATUS
266: **/
267: EFI_STATUS
268: EFIAPI
269: PpcLoadBootImage (
270:     IN  CHAR16* ImagePath,
271:     OUT VOID**  ImageBuffer,
272:     OUT UINT64* ImageSize
273:     );
274: 
275: /**
276:   Set boot parameters for system boot
277:   @param[in] Parameters Boot parameters to set
278:   @retval EFI_STATUS
279: **/
280: EFI_STATUS
281: EFIAPI
282: PpcSetBootParameters (
283:     IN PPC_BOOT_PARAMETERS* Parameters
284:     );
285: 
286: /**
287:   Get current boot information
288:   @param[out] BootInfo Pointer to structure to fill with boot info
289:   @retval EFI_STATUS
290: **/
291: EFI_STATUS
292: EFIAPI
293: PpcGetBootInfo (
294:     OUT PPC_BOOT_INFO* BootInfo
295:     );
296: 
297: /**
298:   Set up the boot environment for the PowerPC system
299:   @retval EFI_STATUS
300: **/
301: EFI_STATUS
302: EFIAPI
303: PpcSetupBootEnvironment (
304:     VOID
305:     );
306: 
307: /**
308:   Verify a loaded kernel image: bounds check against guest RAM and
309:   read the first word to confirm the data was read correctly.
310:   @param[in] KernelAddress Address of the loaded kernel
311:   @param[in] KernelSize    Size of the loaded kernel
312:   @retval EFI_STATUS
313: **/
314: EFI_STATUS
315: EFIAPI
316: PpcVerifyKernel (
317:     IN  EFI_PHYSICAL_ADDRESS KernelAddress,
318:     IN  UINT64               KernelSize
319:     );
320: 
321: /**
322:   Load a system ROM image into memory
323:   @param[in]  RomPath    Path to the ROM image
324:   @param[out] RomBuffer  Pointer to store ROM buffer address
325:   @param[out] RomSize    Pointer to store ROM size
326:   @retval EFI_STATUS
327: **/
328: EFI_STATUS
329: EFIAPI
330: PpcLoadSystemRom (
331:     IN  CHAR16* RomPath,
332:     OUT VOID**  RomBuffer,
333:     OUT UINT64* RomSize
334:     );
335: 
336: /**
337:   Load a system ROM image from the boot volume and map it into guest memory
338:   at PPC_ROM_GUEST_BASE as a read-only region.
339:   @param[in]  RomPath     Path to the ROM image on the boot volume
340:   @param[out] RomAddress  Guest address of the installed ROM (may be NULL)
341:   @param[out] RomSize     Installed ROM size in bytes (may be NULL)
342:   @retval EFI_SUCCESS          ROM installed
343:   @retval EFI_NOT_FOUND        ROM file not present on the volume
344:   @retval EFI_ALREADY_STARTED  A ROM is already installed
345: **/
346: EFI_STATUS
347: EFIAPI
348: PpcInstallSystemRom (
349:     IN  CHAR16* RomPath,
350:     OUT UINT64* RomAddress,
351:     OUT UINT64* RomSize
352:     );
353: 
354: /**
355:   Install a self-contained demo ROM at PPC_ROM_GUEST_BASE (read-only). The
356:   demo ROM contains a small reset-vector program that reads the ROM magic
357:   word and stores its successor into guest RAM.
358:   @param[out] RomAddress  Guest address of the installed ROM (may be NULL)
359:   @param[out] RomSize     Installed ROM size in bytes (may be NULL)
360:   @retval EFI_STATUS
361: **/
362: EFI_STATUS
363: EFIAPI
364: PpcInstallDemoRom (
365:     OUT UINT64* RomAddress,
366:     OUT UINT64* RomSize
367:     );
368: 
369: /**
370:   Install the classic Mac OS low-memory globals region at guest 0x00000000
371:   (16 KB, read/write) as a dedicated region below the kernel base.
372:   @param[out] LowMemAddress  Guest address of the region (may be NULL)
373:   @param[out] LowMemSize     Region size in bytes (may be NULL)
374:   @retval EFI_STATUS
375: **/
376: EFI_STATUS
377: EFIAPI
378: PpcInstallLowMemory (
379:     OUT UINT64* LowMemAddress,
380:     OUT UINT64* LowMemSize
381:     );
382: 
383: /**
384:   Install the classic Mac OS kernel/system area region at guest 0x68000000
385:   (128 MB, read/write). The nanokernel keeps its kernel stack and heap in
386:   this range (fixed 0x68F0xxxx addresses) on all New World Macs; back it
387:   directly since the emulator does not implement segment/BAT translation.
388:   @retval EFI_ALREADY_STARTED  Region already installed
389:   @retval EFI_STATUS
390: **/
391: EFI_STATUS
392: EFIAPI
393: PpcInstallNkSystemArea (
394:     VOID
395:     );
396: 
397: /**
398:   Run the boot memory map / system initialization self-test: low-memory
399:   read/write, ROM read-only enforcement, and a cross-region ROM -> RAM
400:   program executed from the reset vector.
401:   @retval EFI_SUCCESS       All checks passed
402:   @retval EFI_LOAD_ERROR    One or more checks failed
403: **/
404: EFI_STATUS
405: EFIAPI
406: PpcRunBootSelfTest (
407:     VOID
408:     );
409: 
410: /**
411:   Prepare the system for boot: ensure the guest memory map is installed,
412:   reset the CPU to the ROM reset vector with a boot-ready MSR, and write
413:   the emulator boot info block into low memory.
414:   @retval EFI_STATUS
415: **/
416: EFI_STATUS
417: EFIAPI
418: PpcPrepareSystemForBoot (
419:     VOID
420:     );
421: 
422: /**
423:   Scan the boot volume for a classic Mac OS System Folder and record the
424:   presence of System, Finder, Extensions, and Mac OS ROM.
425:   @param[out] Info  Folder scan report (may be NULL)
426:   @retval EFI_STATUS
427: **/
428: EFI_STATUS
429: EFIAPI
430: PpcLocateSystemFolder (
431:     OUT PPC_SYSTEM_FOLDER_INFO* Info
432:     );
433: 
434: /**
435:   Stage the System file, Finder, and Mac OS ROM file from the System Folder
436:   into the guest system staging area.
437:   @retval EFI_SUCCESS       Files staged
438:   @retval EFI_NOT_FOUND     No System Folder / no stageable files
439:   @retval EFI_ALREADY_STARTED  Files already staged
440: **/
441: EFI_STATUS
442: EFIAPI
443: PpcLoadSystemFiles (
444:     VOID
445:     );
446: 
447: /**
448:   Enumerate the Extensions folder and register each file as a driver.
449:   @retval EFI_SUCCESS
450:   @retval EFI_NOT_FOUND     No Extensions folder
451: **/
452: EFI_STATUS
453: EFIAPI
454: PpcScanExtensionsDirectory (
455:     VOID
456:     );
457: 
458: /**
459:   Stage every registered driver into the guest driver staging area.
460:   @retval EFI_SUCCESS       At least one driver staged
461:   @retval EFI_NOT_FOUND     No drivers registered
462: **/
463: EFI_STATUS
464: EFIAPI
465: PpcLoadDrivers (
466:     VOID
467:     );
468: 
469: /**
470:   Get the current System Folder scan / staging report.
471:   @param[out] Info  Report structure to fill
472:   @retval EFI_STATUS
473: **/
474: EFI_STATUS
475: EFIAPI
476: PpcGetSystemFolderInfo (
477:     OUT PPC_SYSTEM_FOLDER_INFO* Info
478:     );
479: 
480: /**
481:   Get a single staged system file entry.
482:   @param[in]  Index  Entry index
483:   @param[out] File   Entry structure to fill
484:   @retval EFI_STATUS
485: **/
486: EFI_STATUS
487: EFIAPI
488: PpcGetSystemFile (
489:     IN  UINTN Index,
490:     OUT PPC_SYSTEM_FILE* File
491:     );
492: 
493: /**
494:   Get a single registered driver entry.
495:   @param[in]  Index  Driver index
496:   @param[out] Driver Driver structure to fill
497:   @retval EFI_STATUS
498: **/
499: EFI_STATUS
500: EFIAPI
501: PpcGetDriver (
502:     IN  UINTN Index,
503:     OUT PPC_SYSTEM_FILE* Driver
504:     );
505: 
506: /**
507:   Run the system files & drivers self-test: staged files read back through the
508:   interpreter memory path, low-memory boot info intact, registry consistent.
509:   @retval EFI_SUCCESS       All checks passed
510:   @retval EFI_LOAD_ERROR    One or more checks failed
511: **/
512: EFI_STATUS
513: EFIAPI
514: PpcRunSystemFilesSelfTest (
515:     VOID
516:     );
517: 
518: #endif // __PPC_BOOTLOADER_H__

(End of file - total 518 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 9.0s)

_Thinking:_

Now I have a thorough understanding of the project. Let me also check the interpreter.c to understand the PPC execution loop and how the 68K integration works, and look at the UI code for the config menu:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 150
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
1: #include "interpreter.h"
2: #include "translation.h"
3: #include "m68k.h"
4: #include "boot/bootloader.h"
5: #include <efi.h>
6: #include <efilib.h>
7: 
8: // Global PowerPC CPU context (backing store for the interpreter and the
9: // public register accessor API)
10: PPC_CPU_CONTEXT g_PpcContext = {0};
11: 
12: // ---------------------------------------------------------------------------
13: // Instruction field extraction (bit 0 = most significant bit of the word)
14: // ---------------------------------------------------------------------------
15: #define OP(w)      ((w) >> 26)
16: #define RT(w)      (((w) >> 21) & 0x1F)
17: #define RS(w)      (((w) >> 21) & 0x1F)
18: #define RD(w)      (((w) >> 21) & 0x1F)
19: #define RA(w)      (((w) >> 16) & 0x1F)
20: #define RB(w)      (((w) >> 11) & 0x1F)
21: #define BO(w)      (((w) >> 21) & 0x1F)
22: #define BI(w)      (((w) >> 16) & 0x1F)
23: #define BF(w)      (((w) >> 23) & 0x7)
24: #define SH(w)      (((w) >> 11) & 0x1F)
25: #define MB(w)      (((w) >> 6) & 0x1F)
26: #define ME(w)      (((w) >> 1) & 0x1F)
27: #define SIMM(w)    ((UINT32)(INT32)(INT16)((w) & 0xFFFF))
28: #define UIMM(w)    ((w) & 0xFFFF)
29: #define XO(w)      (((w) >> 1) & 0x3FE)      // 10-bit XO with OE bit masked out
30: #define XO10(w)    (((w) >> 1) & 0x3FF)      // full 10-bit XO
31: #define Rc(w)      ((w) & 1)
32: #define LK(w)      ((w) & 1)
33: #define AA(w)      (((w) >> 1) & 1)
34: #define SPR(w)     ((((w) >> 16) & 0x1F) | ((((w) >> 11) & 0x1F) << 5))
35: // BD is a 14-bit signed field at word bits 16-29; the byte displacement is
36: // sign_extend(BD) << 2.
37: #define BD(w)      ((UINT32)(INT32)(INT16)((((w) >> 2) & 0x3FFF) << 2))
38: // LI is a 24-bit signed field at word bits 6-29; the byte displacement is
39: // sign_extend(LI) << 2.
40: #define LI(w)      ((UINT32)(INT32)(((((w) >> 2) & 0x800000) ? \
41:                      (((w) >> 2) | 0xFF000000) : (((w) >> 2) & 0xFFFFFF)) << 2))
42: 
43: // Floating-point fields (opcodes 48-63). FRT/FRA/FRB occupy the same word
44: // positions as their fixed-point counterparts; FRC is the third source of the
45: // A-form fused multiply-add instructions.
46: #define FRT(w)     (((w) >> 21) & 0x1F)
47: #define FRA(w)     (((w) >> 16) & 0x1F)
48: #define FRB(w)     (((w) >> 11) & 0x1F)
49: #define FRC(w)     (((w) >> 6) & 0x1F)
50: 
51: // AltiVec vector fields (opcode 4). VD/VA/VB occupy the same positions as
52: // RT/RA/RB. The 11-bit vector sub-opcode spans bits 0-10: a 5-bit XO in the
53: // FRC position (bits 6-10), the V bit at 5, and a 5-bit extension in bits 0-4.
54: // For the VA-form ops (vperm/vsel/vmaddfp/vsldoi/...) the FRC field holds the
55: // third source register or shift instead of part of the opcode.
56: #define VD(w)      RT(w)
57: #define VA(w)      RA(w)
58: #define VB(w)      RB(w)
59: #define VC(w)      FRC(w)
60: #define VX5(w)     FRC(w)      // 5-bit vector sub-opcode (bits 6-10)
61: #define VV(w)      ((w >> 5) & 1)
62: #define VTAIL(w)   (w & 0x1F)
63: #define VS(w)      RT(w)       // vector target/source (mfvscr/mtvscr)
64: #define UIM(w)     VA(w)       // unsigned immediate (convert / splat ops)
65: 
66: // Vector register byte access (guest big-endian). Index 0 is the most
67: // significant byte of the 16-byte vector.
68: #define VBYTE(r, i)      (g_PpcContext.Vr[r][i])
69: #define VWD(r, i)        (((UINT32)VBYTE(r, (i) * 4) << 24) | \
70:                           ((UINT32)VBYTE(r, (i) * 4 + 1) << 16) | \
71:                           ((UINT32)VBYTE(r, (i) * 4 + 2) << 8) | \
72:                           (UINT32)VBYTE(r, (i) * 4 + 3))
73: #define VWD_SET(r, i, v) do { \
74:     VBYTE(r, (i) * 4)     = (UINT8)((v) >> 24); \
75:     VBYTE(r, (i) * 4 + 1) = (UINT8)((v) >> 16); \
76:     VBYTE(r, (i) * 4 + 2) = (UINT8)((v) >> 8);  \
77:     VBYTE(r, (i) * 4 + 3) = (UINT8)(v);         \
78: } while (0)
79: #define VHW(r, i)        (((UINT32)VBYTE(r, (i) * 2) << 8) | VBYTE(r, (i) * 2 + 1))
80: #define VHW_SET(r, i, v) do { \
81:     VBYTE(r, (i) * 2) = (UINT8)((v) >> 8); \
82:     VBYTE(r, (i) * 2 + 1) = (UINT8)(v);    \
83: } while (0)
84: 
85: // Effective address helpers (RA==0 means GPR(0) is NOT used)
86: #define EaD(w, ra) (((ra) == 0) ? SIMM(w) : (g_PpcContext.Gpr[ra] + SIMM(w)))
87: #define EaX(w, ra, rb) ((((ra) == 0) ? 0U : g_PpcContext.Gpr[ra]) + g_PpcContext.Gpr[rb])
88: 
89: // X-form primary XO values for opcode 31
90: #define XO_CMP         0
91: #define XO_TW          4
92: #define XO_SUBFC       8
93: #define XO_ADDC       10
94: #define XO_MULHWU     11
95: #define XO_MFCR       19
96: #define XO_LWARX      20
97: #define XO_LWZX       23
98: #define XO_LWZUX      55
99: #define XO_SLW        24
100: #define XO_CNTLZW     26
101: #define XO_AND        28
102: #define XO_CMPL       32
103: #define XO_SUBF       40
104: #define XO_DCBST      54
105: #define XO_ANDC       60
106: #define XO_MULHW      75
107: #define XO_TLBIEL     78
108: #define XO_MFMSR      83
109: #define XO_DCBF       86
110: #define XO_LBZX       87
111: #define XO_NEG       104
112: #define XO_LBZUX     119
113: #define XO_NOR       124
114: #define XO_SUBFE     136
115: #define XO_ADDE      138
116: #define XO_MTCRF     144
117: #define XO_MTMSR     146
118: #define XO_STWCX_    150
119: #define XO_STWX      151
120: #define XO_STWUX     183
121: #define XO_SUBFZE    200
122: #define XO_ADDZE     202
123: #define XO_STBX      215
124: #define XO_SUBFME    232
125: #define XO_ADDME     234
126: #define XO_MULLW     235
127: #define XO_MTSRIN    242
128: #define XO_DCBTST    246
129: #define XO_STBUX     247
130: #define XO_ADD       266
131: #define XO_DCBT      278
132: #define XO_LHZX      279
133: #define XO_EQV       284
134: #define XO_TLBIE     306
135: #define XO_LHZUX     311
136: #define XO_XOR       316
137: #define XO_MFSPR     339
138: #define XO_LHAX      343
139: #define XO_TLBIA     370
140: #define XO_MFTB      371
141: #define XO_LHAUX     375
142: #define XO_STHX      407
143: #define XO_ORC       412
144: #define XO_STHUX     439
145: #define XO_OR        444
146: #define XO_DIVWU     459
147: #define XO_MTSPR     467
148: #define XO_DCBI      470
149: #define XO_NAND      476
150: #define XO_DIVW      491

(Showing lines 1-150 of 5348. Use offset=151 to continue.)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\ui"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\ui</path>
<type>directory</type>
<entries>
ui_impl.c
ui.h

(2 entries)
</entries>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\m68k.c",
  "limit": 150
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\m68k.c</path>
<type>file</type>
<content>
1: // ---------------------------------------------------------------------------
2: // Native Motorola 68000 interpreter for EFIMac
3: //
4: // This replaces the ROM's PPC-based DR emulator with a native C interpreter.
5: // The PPC interpreter hooks the opcode table (ROM+0x380000) so that regular
6: // 68K opcodes route through a trampoline into this code. EMUL_OP entries
7: // (0xFE40+) continue to use the existing mulli-marker interception.
8: // ---------------------------------------------------------------------------
9: 
10: #include "m68k.h"
11: #include "interpreter.h"
12: #include "translation.h"
13: #include "boot/bootloader.h"
14: #include <efi.h>
15: #include <efilib.h>
16: #include <lib.h>
17: 
18: // ---------------------------------------------------------------------------
19: // Trace file logging
20: // ---------------------------------------------------------------------------
21: STATIC CHAR16 g_TraceBuf[8192];
22: STATIC UINTN  g_TraceLen = 0;
23: 
24: STATIC
25: VOID
26: M68kTraceFlush (
27:     VOID
28:     )
29: {
30:     if (g_TraceLen == 0) return;
31: 
32:     EFI_LOADED_IMAGE_PROTOCOL* LoadedImage = NULL;
33:     EFI_STATUS Status = BS->HandleProtocol(LibImageHandle, &LoadedImageProtocol, (VOID**)&LoadedImage);
34:     if (EFI_ERROR(Status) || LoadedImage == NULL || LoadedImage->DeviceHandle == NULL) return;
35: 
36:     EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs = NULL;
37:     Status = BS->HandleProtocol(LoadedImage->DeviceHandle, &FileSystemProtocol, (VOID**)&Fs);
38:     if (EFI_ERROR(Status) || Fs == NULL) return;
39: 
40:     EFI_FILE_HANDLE Root = NULL;
41:     Status = Fs->OpenVolume(Fs, &Root);
42:     if (EFI_ERROR(Status)) return;
43: 
44:     EFI_FILE_HANDLE F = NULL;
45:     Status = Root->Open(Root, &F, L"trace68k.log",
46:                         EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
47:     if (EFI_ERROR(Status) || F == NULL) { Root->Close(Root); return; }
48: 
49:     F->SetPosition(F, (UINT64)-1);
50:     UINTN Len = g_TraceLen * sizeof(CHAR16);
51:     F->Write(F, &Len, g_TraceBuf);
52:     F->Close(F);
53:     Root->Close(Root);
54:     g_TraceLen = 0;
55: }
56: 
57: STATIC
58: VOID
59: M68kTrace (
60:     IN CHAR16* Line
61:     )
62: {
63:     UINTN Len = StrLen(Line);
64:     if (g_TraceLen + Len + 1 > sizeof(g_TraceBuf) / sizeof(CHAR16)) {
65:         M68kTraceFlush();
66:     }
67:     CopyMem(g_TraceBuf + g_TraceLen, Line, (Len + 1) * sizeof(CHAR16));
68:     g_TraceLen += Len;
69:     if (g_TraceLen > 7900) M68kTraceFlush();
70: }
71: 
72: // Helper: write a formatted trace line using simple hex formatting
73: STATIC
74: VOID
75: M68kTraceLine (
76:     IN UINT32 Step,
77:     IN UINT32 PC,
78:     IN UINT16 Op,
79:     IN UINT32 SP,
80:     IN UINT16 SR,
81:     IN UINT32 A6Val,
82:     IN UINT32 D0Val
83:     )
84: {
85:     CHAR16 Buf[120];
86:     // Manual format: step, PC, Op, SP, SR, A6, D0
87:     // Use Print to format into Buf (Print goes through ConOut but we write to Buf via UnicodeSPrint)
88:     UnicodeSPrint(Buf, sizeof(Buf),
89:         L"%u:PC=%08X:%04X SP=%08X SR=%04X A6=%08X D0=%08X\n",
90:         Step, PC, Op, SP, SR, A6Val, D0Val);
91:     M68kTrace(Buf);
92: }
93: 
94: // ---------------------------------------------------------------------------
95: // Global 68K CPU context
96: // ---------------------------------------------------------------------------
97: M68K_CPU_CONTEXT g_M68kContext = {0};
98: 
99: // ---------------------------------------------------------------------------
100: // Detect addresses that belong to the DR emulator's domain (PPC opcode table,
101: // emulator code area).  Jumps into these regions must be intercepted and
102: // redirected to A6 (the callback return address).
103: // ---------------------------------------------------------------------------
104: STATIC
105: BOOLEAN
106: M68kIsDrEmulatorAddress (
107:     IN UINT32 Addr
108:     )
109: {
110:     if (Addr >= 0x30500000 && Addr < 0x30600000) return TRUE;
111:     if (Addr >= 0x40B60000 && Addr < 0x40C00000) return TRUE;
112:     return FALSE;
113: }
114: 
115: // ---------------------------------------------------------------------------
116: // Memory access (big-endian, same as PPC guest)
117: // ---------------------------------------------------------------------------
118: 
119: UINT8
120: M68kReadByte (
121:     IN UINT32 Address
122:     )
123: {
124:     return PpcReadGuestByte (Address);
125: }
126: 
127: UINT16
128: M68kReadWord (
129:     IN UINT32 Address
130:     )
131: {
132:     return ((UINT16)PpcReadGuestByte (Address) << 8) |
133:            (UINT16)PpcReadGuestByte (Address + 1);
134: }
135: 
136: UINT32
137: M68kReadLong (
138:     IN UINT32 Address
139:     )
140: {
141:     return ((UINT32)M68kReadWord (Address) << 16) |
142:            (UINT32)M68kReadWord (Address + 2);
143: }
144: 
145: VOID
146: M68kWriteByte (
147:     IN UINT32 Address,
148:     IN UINT8  Value
149:     )
150: {

(Showing lines 1-150 of 2900. Use offset=151 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 6.8s)

_Thinking:_

Now I have a good understanding of the project. Let me read more of the interpreter to understand PpcRunGuest and the execution loop:

**Tool: grep**

**Input:**
```json
{
  "pattern": "PpcRunGuest",
  "path": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src"
}
```

**Output:**
```
Found 4 matches
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\main.c:
  Line 784:         RunStatus = PpcRunGuest(PPC_GUEST_STEP_BUDGET, TRUE, &Executed);


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c:
  Line 4138: PpcRunGuest (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\translation.h:
  Line 248: PpcRunGuest (


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\m68k.h:
  Line 39:     // or an exception; PpcRunGuest can check this to stop.

```

**Tool: grep**

**Input:**
```json
{
  "pattern": "PpcPatchNewWorldRom",
  "path": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src"
}
```

**Output:**
```
Found 6 matches
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c:
  Line 1885: PpcPatchNewWorldRom (

  Line 2054:         Status = PpcPatchNewWorldRom();


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader.h:
  Line 26: // The 68K emulator's kernel-trap table, as patched by PpcPatchNewWorldRom:

  Line 37: // PpcPatchNewWorldRom writes:


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c:
  Line 4504:             // globals PpcPatchNewWorldRom wrote. Restore them at the exact


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\main.c:
  Line 751:             // table (`twui r31,n`). PpcPatchNewWorldRom redirects

```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\ui\\ui.h"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\ui\ui.h</path>
<type>file</type>
<content>
1: #ifndef __PPC_UI_H__
2: #define __PPC_UI_H__
3: 
4: #include <efi.h>
5: 
6: // Length of the boot gate countdown (seconds). Pressing F8 inside this
7: // window opens the setup menu.
8: #define PPC_BOOT_GATE_TIMEOUT_SECONDS  5u
9: 
10: // Vendor GUID under which the saved configuration lives in NVRAM.
11: #define PPC_CONFIG_VARIABLE_GUID \
12:   { 0x6F9AC2B8, 0x5E8A, 0x4C29, { 0x9E, 0x2F, 0x2F, 0x16, 0x5C, 0x49, 0x0D, 0x53 } }
13: 
14: #define PPC_CONFIG_SIGNATURE          0x4546494D  // "EFIM"
15: #define PPC_CONFIG_VERSION            1u
16: 
17: // BootDeviceIndex value meaning "auto-detect the first HFS volume".
18: #define PPC_CONFIG_AUTO_BOOT_DEVICE   0xFFFFFFFFu
19: 
20: // Persistent boot configuration. Kept deliberately flat and fixed-size so
21: // it round-trips through a single UEFI NVRAM variable with a checksum.
22: typedef struct {
23:     UINT32  Signature;          // PPC_CONFIG_SIGNATURE
24:     UINT32  Version;            // PPC_CONFIG_VERSION
25:     UINT32  BootMode;           // PPC_BOOT_MODE_*
26:     UINT32  MemorySizeMB;       // guest RAM size in MB
27:     UINT32  VideoMode;          // PPC_GRAPHICS_MODE_*
28:     UINT32  BootDeviceIndex;    // block device index (PPC_CONFIG_AUTO_BOOT_DEVICE = auto)
29:     BOOLEAN AudioEnabled;       // emulated audio device
30:     BOOLEAN NetworkEnabled;     // emulated network interfaces
31:     BOOLEAN DebugEnabled;       // verbose debug output
32:     UINT8   Reserved[9];        // padding so the checksum stays word aligned
33:     UINT32  Checksum;           // simple sum over the preceding bytes
34: } PPC_CONFIG;
35: 
36: /**
37:   Fill a configuration structure with the factory defaults.
38:   @param[out] Config  Structure to initialize
39:   @retval EFI_SUCCESS
40:   @retval EFI_INVALID_PARAMETER  Config is NULL
41: **/
42: EFI_STATUS
43: EFIAPI
44: PpcConfigSetDefaults (
45:     OUT PPC_CONFIG* Config
46:     );
47: 
48: /**
49:   Load the saved configuration from NVRAM. On first boot or on corruption
50:   the structure is filled with defaults.
51:   @param[out] Config  Structure to receive the loaded configuration
52:   @retval EFI_SUCCESS       Configuration loaded
53:   @retval EFI_LOAD_ERROR    Stored configuration was corrupt (defaults used)
54:   @retval EFI_NOT_FOUND     No configuration stored yet (defaults used)
55:   @retval EFI_INVALID_PARAMETER  Config is NULL
56: **/
57: EFI_STATUS
58: EFIAPI
59: PpcConfigLoad (
60:     OUT PPC_CONFIG* Config
61:     );
62: 
63: /**
64:   Persist a configuration structure to NVRAM.
65:   @param[in] Config  Configuration to store
66:   @retval EFI_STATUS
67: **/
68: EFI_STATUS
69: EFIAPI
70: PpcConfigSave (
71:     IN  const PPC_CONFIG* Config
72:     );
73: 
74: /**
75:   Translate a PPC_GRAPHICS_MODE_* value into pixel dimensions.
76:   @param[in]  Mode   PPC_GRAPHICS_MODE_* constant
77:   @param[out] Width  Horizontal resolution
78:   @param[out] Height Vertical resolution
79:   @retval EFI_SUCCESS     Resolution returned
80:   @retval EFI_NOT_FOUND   Unknown mode
81: **/
82: EFI_STATUS
83: EFIAPI
84: PpcVideoModeResolution (
85:     IN  UINT32 Mode,
86:     OUT UINT32* Width,
87:     OUT UINT32* Height
88:     );
89: 
90: /**
91:   Show the ASCII "Macintosh" splash and wait up to
92:   PPC_BOOT_GATE_TIMEOUT_SECONDS for F8. Clears and repaints the text
93:   console, so call it before any other screen output.
94:   @param[in] Config  Current configuration (shown as a summary)
95:   @retval TRUE   F8 was pressed; the setup menu should be shown
96:   @retval FALSE  Countdown expired; boot with the current configuration
97: **/
98: BOOLEAN
99: EFIAPI
100: PpcBootGateWait (
101:     IN  const PPC_CONFIG* Config
102:     );
103: 
104: /**
105:   Interactive configuration menu. Arrow keys move and change values,
106:   Enter activates an action row, Esc exits without saving, S saves.
107:   @param[in,out] Config  Configuration to edit
108:   @retval EFI_SUCCESS
109:   @retval EFI_UNSUPPORTED  No text input device available
110: **/
111: EFI_STATUS
112: EFIAPI
113: PpcShowConfigMenu (
114:     IN OUT PPC_CONFIG* Config
115:     );
116: 
117: #endif // __PPC_UI_H__

(End of file - total 117 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 4.8s)

_Thinking:_

Now I have a thorough understanding of the project. Let me now plan what needs to be done. Let me read the PpcRunGuest function and PpcPatchNewWorldRom to understand the current execution flow:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "offset": 4100,
  "limit": 300
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
4100:     OUT UINTN*  ExecutedCount
4101:     )
4102: {
4103:     UINTN Executed = 0;
4104: 
4105:     if (InstructionStream == NULL || ExecutedCount == NULL) {
4106:         return EFI_INVALID_PARAMETER;
4107:     }
4108: 
4109:     g_PpcContext.Pc = (UINT32)(UINTN)InstructionStream;
4110:     g_PpcContext.ExceptionPending = 0;
4111: 
4112:     while (Executed < MaxInstructions) {
4113:         UINT32 Instr = CpuRead32(g_PpcContext.Pc);
4114:         UINT32 Next;
4115:         EFI_STATUS Status = PpcExecuteInstruction(Instr, g_PpcContext.Pc, &Next);
4116:         Executed++;
4117:         if (EFI_ERROR(Status)) {
4118:             *ExecutedCount = Executed;
4119:             return Status;
4120:         }
4121:         g_PpcContext.Pc = Next;
4122:         if (g_PpcContext.ExceptionPending != 0) {
4123:             break;
4124:         }
4125:     }
4126: 
4127:     *ExecutedCount = Executed;
4128:     return EFI_SUCCESS;
4129: }
4130: 
4131: // Continuous guest execution harness. Runs up to MaxInstructions of real
4132: // guest code from the current PC, delivering pending exceptions through the
4133: // CPU vector mechanism so interrupt/syscall handlers run like on hardware.
4134: // Stops with the reported status on an unimplemented opcode (EFI_UNSUPPORTED)
4135: // or a memory/execution error; the guest PC is left at the stopping point.
4136: EFI_STATUS
4137: EFIAPI
4138: PpcRunGuest (
4139:     IN  UINT32  MaxInstructions,
4140:     IN  BOOLEAN LogUnsupported,
4141:     OUT UINTN*  ExecutedCount
4142:     )
4143: {
4144:     UINTN Executed = 0;
4145:     UINTN TailStart = 0;
4146:     UINTN TailCount = 0;
4147:     static UINT32 TailPc[4096];
4148:     static UINT32 TailInst[4096];
4149:     static UINT32 TailNext[4096];
4150:     static UINT32 TailR28[4096];
4151:     static UINT32 TailR8[4096];
4152:     static UINT32 TailR17[4096];
4153:     static UINT32 TailLr[4096];
4154:     static UINT32 TailR24[4096];
4155:     static UINT32 TailR27[4096];
4156:     static UINT32 TailR7[4096];
4157:     static UINT32 TailR5[4096];
4158:     static UINT32 TailR15[4096];
4159:     static UINT32 TailR16[4096];
4160:     static UINT32 TailCr[4096];
4161:     static UINT32 PcsDumped = 0;
4162:     static UINT32 TraceDumped = 0;
4163:     static UINT32 StoreProbed = 0;
4164:     static UINT32 RamProbed = 0;
4165:     static UINT32 AllocTraced = 0;
4166:     static UINT32 FlushProbed = 0;
4167:     static UINT32 HelperDumped = 0;
4168:     static UINT32 SccPollTraced = 0;
4169:     static UINT32 HelperStep = 0;
4170:     static UINT32 TermEntries = 0;
4171:     static UINT32 AutoResumed = 0;
4172:     static UINT32 PmdWalked = 0;
4173:     static UINT32 PmdEntry = 0;
4174:     static UINT32 PmdArrDump = 0;
4175:     static UINT32 MergeTraced = 0;
4176:     static UINT32 PmdFixed = 0;
4177:     static UINT32 BootTailProbed = 0;
4178:     static UINT32 TailProbed = 0;
4179:     static UINT32 EmulStartProbed = 0;
4180:     static UINT32 EmulTrapProbed = 0;
4181:     static UINT32 CallTblProbed = 0;
4182:     static UINT32 ScSiteCount = 0;
4183:     static UINT32 SyscallDispatchProbed = 0;
4184:     static UINT32 TrapProbed = 0;
4185:     static UINT32 EcbProbed = 0;
4186:     static UINT32 SchedProbes = 0;
4187:     static UINT32 EmulWindowProbed = 0;
4188:     static UINT32 InjectedEntryProbed = 0;
4189:     static UINT32 WalkProbed = 0;
4190: 
4191:     if (ExecutedCount == NULL) {
4192:         return EFI_INVALID_PARAMETER;
4193:     }
4194: 
4195:     g_PpcContext.ExceptionPending = 0;
4196: 
4197:     while (Executed < MaxInstructions) {
4198:         UINT32 Instr;
4199:         UINT32 Current;
4200:         UINT32 Next;
4201:         EFI_STATUS Status;
4202: 
4203:         Instr = CpuRead32(g_PpcContext.Pc);
4204:         Current = g_PpcContext.Pc;
4205:         // ---- 68K DR-emulator software-function hooks ----
4206:         // The ROM dispatches certain 68K opcodes through "software function"
4207:         // pointers stored in ed.v (offsets 0x800..0x834 of the emulator data
4208:         // block at 0xB000). The ROM never seeds these slots, so the dispatch
4209:         // machinery falls into the tail at 0x40B6CA60 and executes `bctrl`
4210:         // with CTR == ed.v[0x80C] == 0, branching to address 0. The missing
4211:         // functions are emulated here in C and the context is handed back to
4212:         // the ROM's common dispatch (0x40B67C60).
4213:         UINT32 Hooked = 0;
4214:         // ---- Native 68K dispatch-loop hook ----
4215:         // When the PPC DR-emulator enters its common dispatch at 0x40B67C60,
4216:         // intercept and execute the 68K instruction natively via the C
4217:         // interpreter, completely replacing the PPC-based opcode table.
4218:         if (Current == 0x40B67C60) {
4219:             Status = M68kExecuteFromPPC ();
4220:             g_PpcContext.Gpr[27] = 0;
4221:             g_PpcContext.Gpr[29] = 0x40B80000;
4222:             Next = 0x40B67C60;
4223:             Hooked = 1;
4224:         }
4225:         if (Current == 0x40B6CA84 && Instr == 0x4E800421) {
4226:             // Tail's `bctrl` (software fn ed.v[0x80C]). 68K MOVE #<imm>,SR
4227:             // (0x46FC) routes here via entry[0x46FC] -> 0x40B6C570 bnsl cr2
4228:             // -> 0x40B6CA68. r3/r24 = imm address (PC+2), r27 = SR value.
4229:             if (CpuRead32(0x0000B80C) == 0 && CpuRead16(g_PpcContext.Gpr[3] - 2) == 0x46FC) {
4230:                 UINT16 Sr = CpuRead16(g_PpcContext.Gpr[3]);
4231:                 g_PpcContext.Gpr[24] = g_PpcContext.Gpr[3] + 2;
4232:                 g_PpcContext.Gpr[25] = Sr >> 8;
4233:                 g_PpcContext.Gpr[26] = 0;
4234:                 g_PpcContext.Gpr[27] = 0;
4235:                 g_PpcContext.Gpr[29] = 0x40B80000;
4236:                 g_PpcContext.Xer = 0;
4237:                 g_PpcContext.Cr &= ~0x0F00000F;             // CCR==0: clear cr1, cr7
4238:                 g_PpcContext.Cr = (g_PpcContext.Cr & ~0x00F00000) | 0x00100000;  // cr2 = SO (supervisor)
4239:                 Next = 0x40B67C60;
4240:                 Hooked = 1;
4241:                 Print(L"  MOVE-SR-HOOK 46FC SR=0x%04x r24=0x%08x CR=0x%08x -> 0x40b67c60\n",
4242:                       Sr, g_PpcContext.Gpr[24], g_PpcContext.Cr);
4243:             }
4244:         }
4245:         if (Current == 0x40BA7380) {
4246:             // entry[0x4E70] = 68K RESET (software fn ed.v[0x828]): reset the
4247:             // external devices. Treated as a no-op; continue at opcode+2
4248:             // (r24 already points there from the common dispatch).
4249:             if (CpuRead32(0x0000B828) == 0 && CpuRead16(g_PpcContext.Gpr[24] - 2) == 0x4E70) {
4250:                 g_PpcContext.Gpr[27] = 0;
4251:                 g_PpcContext.Gpr[29] = 0x40B80000;
4252:                 Next = 0x40B67C60;
4253:                 Hooked = 1;
4254:                 Print(L"  RESET-HOOK 4E70 r24=0x%08x CR=0x%08x -> 0x40b67c60\n",
4255:                       g_PpcContext.Gpr[24], g_PpcContext.Cr);
4256:             }
4257:         }
4258:         if (Current == 0x40BA73D8 && Instr == 0x80BF087C) {
4259:             // entry[0x4E7B] = 68K escape (software fn ed.v[0x87C]): the
4260:             // dispatch has already consumed the 2-byte parameter word into
4261:             // r27 and advanced r24 past the opcode (r24 = param address).
4262:             // Treated as a no-op; advance r24 past the parameter and resume
4263:             // the DR loop at the next 68K opcode.
4264:             if (CpuRead32(0x0000B87C) == 0 && CpuRead16(g_PpcContext.Gpr[24] - 2) == 0x4E7B) {
4265:                 UINT32 Resume = g_PpcContext.Gpr[24] + 2;
4266:                 UINT16 Param = (UINT16)g_PpcContext.Gpr[27];
4267:                 g_PpcContext.Gpr[24] = Resume;
4268:                 g_PpcContext.Gpr[27] = 0;
4269:                 g_PpcContext.Gpr[29] = 0x40B80000;
4270:                 Next = 0x40B67C60;
4271:                 Hooked = 1;
4272:                 Print(L"  4E7B-HOOK param=0x%04x resume=0x%08x CR=0x%08x -> 0x40b67c60\n",
4273:                       Param, Resume, g_PpcContext.Cr);
4274:             }
4275:         }
4276:         if (Current == 0x40BBF8D0 && Instr == 0x80BF0800) {
4277:             // entry[0x7F1A] = 68K MOVEQ #imm,Dn (software fn ed.v[0x800],
4278:             // the r6=0x10 trampoline at 0x40B6D750). The ROM implements the
4279:             // MOVEQ table natively as `addic. rD,r0,signext(imm8)` thunks but
4280:             // routes this one entry through the unseeded ed.v[0x800] slot.
4281:             // Emulate the same addic. the native thunk would have executed,
4282:             // leave r24 pointing at the next 68K opcode, and resume the DR
4283:             // loop. (r24 is already past the 1-word MOVEQ at this point.)
4284:             if (CpuRead32(0x0000B800) == 0) {
4285:                 UINT16 Op = CpuRead16(g_PpcContext.Gpr[24] - 2);
4286:                 if ((Op & 0xF000) == 0x7000) {
4287:                     UINT32 Ca;
4288:                     INT32 Imm = (INT32)(INT8)(Op & 0xFF);
4289:                     UINT32 Rd = 8 + ((Op >> 9) & 7);   // r8..r15 = d0..d7
4290:                     g_PpcContext.Gpr[Rd] = PpcDoAdd(0, Imm, 0, &Ca, NULL);
4291:                     PpcSetXerCarry(Ca);
4292:                     PpcSetCr0FromResult(g_PpcContext.Gpr[Rd]);
4293:                     g_PpcContext.Gpr[27] = 0;
4294:                     g_PpcContext.Gpr[29] = 0x40B80000;
4295:                     Next = 0x40B67C60;
4296:                     Hooked = 1;
4297:                     Print(L"  MOVEQ-HOOK op=0x%04x imm=%d d%u=0x%08x r24=0x%08x CR=0x%08x -> 0x40b67c60\n",
4298:                           Op, Imm, (Op >> 9) & 7, g_PpcContext.Gpr[Rd],
4299:                           g_PpcContext.Gpr[24], g_PpcContext.Cr);
4300:                 }
4301:             }
4302:         }
4303:         if (Hooked) {
4304:             Status = EFI_SUCCESS;
4305:         } else {
4306:             Status = PpcExecuteInstruction(Instr, Current, &Next);
4307:         }
4308:         Executed++;
4309:         if (Current == 0x40B126CC || Current == 0x40B107FC || Current == 0x40B10098) {
4310:             Print(L"  PROBE@0x%08x r1=0x%08x r3=0x%08x [r1+648]=0x%08x [0x648]=0x%08x [0xA648]=0x%08x [0xAFE4]=0x%04x [r1+5A0]=0x%08x [r1+5A4]=0x%08x [r1-964]=0x%08x [r1-20]=0x%08x\n",
4311:                   Current, g_PpcContext.Gpr[1], g_PpcContext.Gpr[3],
4312:                   CpuRead32(g_PpcContext.Gpr[1] + 0x648),
4313:                   CpuRead32(0x00000648), CpuRead32(0x0000A648),
4314:                   CpuRead16(0x0000AFE4),
4315:                   CpuRead32(g_PpcContext.Gpr[1] + 0x5A0),
4316:                   CpuRead32(g_PpcContext.Gpr[1] + 0x5A4),
4317:                   CpuRead32(g_PpcContext.Gpr[1] - 0x964),
4318:                   CpuRead32(g_PpcContext.Gpr[1] - 0x20));
4319:         }
4320:         if (EcbProbed == 0 && Current == 0x40B10834) {
4321:             UINT32 R3 = g_PpcContext.Gpr[3];
4322:             EcbProbed = 1;
4323:             Print(L"  ECB@0x%08x r1=0x%08x r3=0x%08x r8=0x%08x r11=0x%08x r12=0x%08x "
4324:                   L"[r3+78]=0x%08x [r3+84]=0x%08x [r3+A4]=0x%08x [r3+AC]=0x%08x "
4325:                   L"[r1+654]=0x%08x [r1+658]=0x%08x\n",
4326:                   Current, g_PpcContext.Gpr[1], R3, g_PpcContext.Gpr[8],
4327:                   g_PpcContext.Gpr[11], g_PpcContext.Gpr[12],
4328:                   CpuRead32(R3 + 0x78), CpuRead32(R3 + 0x84),
4329:                   CpuRead32(R3 + 0xA4), CpuRead32(R3 + 0xAC),
4330:                   CpuRead32(g_PpcContext.Gpr[1] + 0x654),
4331:                   CpuRead32(g_PpcContext.Gpr[1] + 0x658));
4332:         }
4333:         // The NK boot tail's `blrl` at 0x40B126F0 calls
4334:         // KDP.LA_EmulatorKernelTrapTable ([r1+0x648]) = 0x6806E8C0
4335:         // (the 68K emulator's kernel-trap table, `twui r31,0`). This is the
4336:         // exact moment of the 68K handoff: dump the trap-entry protocol state
4337:         // the interpreter must reproduce for IntProgram.
4338:         if (BootTailProbed == 0 && Current == 0x40B126F0) {
4339:             UINT32 K = g_PpcContext.Gpr[1];
4340:             BootTailProbed = 1;
4341:             Print(L"  BOOTTAIL@0x%08x r1=0x%08x r3=0x%08x r4=0x%08x MSR=0x%08x SRR0=0x%08x SRR1=0x%08x\n",
4342:                   Current, K, g_PpcContext.Gpr[3], g_PpcContext.Gpr[4],
4343:                   g_PpcContext.Msr, g_PpcContext.Srr0, g_PpcContext.Srr1);
4344:             Print(L"  BOOTTAIL SPRG0=0x%08x SPRG1=0x%08x SPRG2=0x%08x SPRG3=0x%08x SPRG4=0x%08x\n",
4345:                   g_PpcContext.Spr[272], g_PpcContext.Spr[273],
4346:                   g_PpcContext.Spr[274], g_PpcContext.Spr[275],
4347:                   g_PpcContext.Spr[276]);
4348:             Print(L"  BOOTTAIL KDP.PA_ConfigInfo[r1+630]=0x%08x PA_EmulatorData[r1+634]=0x%08x "
4349:                   L"PA_CurAS[r1-1C]=0x%08x PA_PSA[r1-18]=0x%08x PA_KDP[r1-4]=0x%08x\n",
4350:                   CpuRead32(K + 0x630), CpuRead32(K + 0x634),
4351:                   CpuRead32(K - 0x1C), CpuRead32(K - 0x18), CpuRead32(K - 0x04));
4352:             Print(L"  BOOTTAIL KDP.LA_EmulatorKernelTrapTable[r1+648]=0x%08x PA_ECB[r1+658]=0x%08x "
4353:                   L"LA_ECB[r1+654]=0x%08x NanoKernelCallTable[0][r1+5F0]=0x%08x\n",
4354:                   CpuRead32(K + 0x648), CpuRead32(K + 0x658),
4355:                   CpuRead32(K + 0x654), CpuRead32(K + 0x5F0));
4356:             Print(L"  BOOTTAIL KDP.ECB[r1+65C]=0x%08x flags[r1+660]=0x%08x CallTable[1]=0x%08x "
4357:                   L"[2]=0x%08x [3]=0x%08x [4]=0x%08x\n",
4358:                   CpuRead32(K + 0x65C), CpuRead32(K + 0x660),
4359:                   CpuRead32(K + 0x5F4), CpuRead32(K + 0x5F8),
4360:                   CpuRead32(K + 0x5FC), CpuRead32(K + 0x600));
4361:             Print(L"  BOOTTAIL trap-table word[0]=0x%08x word[1]=0x%08x "
4362:                   L"XLM.SIG=0x%08x XLM.KDP=0x%08x XLM.IRQNEST=0x%08x\n",
4363:                   CpuRead32(PPC_EMULATOR_TRAP_TABLE),
4364:                   CpuRead32(PPC_EMULATOR_TRAP_TABLE + 4),
4365:                   CpuRead32(PPC_XLM_SIGNATURE_OFFSET),
4366:                   CpuRead32(PPC_XLM_KERNEL_DATA_OFFSET),
4367:                   CpuRead32(PPC_XLM_IRQ_NEST_OFFSET));
4368:         }
4369:         // The boot tail's `blrl` lands here: the patched trap table entry
4370:         // (b 0x36f900). Dump the handoff state once at emulator start.
4371:         if (EmulStartProbed == 0 && Current == 0x40B6F900) {
4372:             UINT32 K = g_PpcContext.Gpr[1];
4373:             EmulStartProbed = 1;
4374:             Print(L"  EMUSTART@0x%08x r1=0x%08x r3=0x%08x r6=0x%08x r7=0x%08x "
4375:                   L"r8=0x%08x r10=0x%08x r11=0x%08x r12=0x%08x LR=0x%08x MSR=0x%08x\n",
4376:                   Current, K, g_PpcContext.Gpr[3], g_PpcContext.Gpr[6],
4377:                   g_PpcContext.Gpr[7], g_PpcContext.Gpr[8], g_PpcContext.Gpr[10],
4378:                   g_PpcContext.Gpr[11], g_PpcContext.Gpr[12],
4379:                   g_PpcContext.Lr, g_PpcContext.Msr);
4380:             Print(L"  EMUSTART KDP.CallTable[0][r1+5F0]=0x%08x ECB[r1+65C]=0x%08x "
4381:                   L"flags[r1+660]=0x%08x XLM.RUNMODE=0x%08x\n",
4382:                   CpuRead32(K + 0x5F0), CpuRead32(K + 0x65C),
4383:                   CpuRead32(K + 0x660), CpuRead32(PPC_XLM_RUN_MODE_OFFSET));
4384:             Print(L"  EMUSTART runtime mem@calltable[0] (0x%08x): %08x %08x %08x %08x\n",
4385:                   CpuRead32(K + 0x5F0), CpuRead32(CpuRead32(K + 0x5F0)),
4386:                   CpuRead32(CpuRead32(K + 0x5F0) + 4),
4387:                   CpuRead32(CpuRead32(K + 0x5F0) + 8),
4388:                   CpuRead32(CpuRead32(K + 0x5F0) + 0xC));
4389:             Print(L"  EMUSTART emulstart w[26]@0x40b6f968=0x%08x w[20]@0x40b6f950=0x%08x "
4390:                   L"entry[0]@0x40b6f700=0x%08x entry[1]=0x%08x helper[0]@0x40b6f7c0=0x%08x\n",
4391:                   CpuRead32(0x40B6F968), CpuRead32(0x40B6F950),
4392:                   CpuRead32(0x40B6F700), CpuRead32(0x40B6F704),
4393:                   CpuRead32(0x40B6F7C0));
4394:         }
4395:         // The injected 68K DR-emulator entry (RomWriteEmulatorEntryRoutine).
4396:         if (InjectedEntryProbed == 0 && Current == 0x40B6F700) {
4397:             InjectedEntryProbed = 1;
4398:             Print(L"  INJENTRY@0x40b6f700 r1=0x%08x r8=0x%08x r24=0x%08x r27=0x%08x "
4399:                   L"r29=0x%08x r30=0x%08x r31=0x%08x LR=0x%08x\n",

(Showing lines 4100-4399 of 5348. Use offset=4400 to continue.)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\boot\\bootloader_impl.c",
  "offset": 1850,
  "limit": 250
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\boot\bootloader_impl.c</path>
<type>file</type>
<content>
1850:     }
1851: }
1852: 
1853: // Install the ed.v[0x814] dispatch helper at ROM + 0x36f7c0. The DR emulator's
1854: // state machine (0x40b6d114) calls it through the 68K-mode glue via blrl
1855: // (CTR = dispatch entry, LR = return into the state machine). It sets cr2.GE
1856: // so the glue's `bgelr cr2` returns into the state machine, then bctr's to the
1857: // dispatch entry.
1858: STATIC VOID
1859: RomWriteEmulatorDispatchHelper (
1860:     IN UINT8*  Rom,
1861:     IN UINT32  Offset
1862:     )
1863: {
1864:     static const UINT32 Words[3] = {
1865:         0x3c000060,  // lis r0,0x0060         cr2.GT|EQ
1866:         0x7c004120,  // mtcrf 0x04,r0         cr2 = GE
1867:         0x4e800420   // bctr
1868:     };
1869:     UINT32 I;
1870:     for (I = 0; I < sizeof(Words) / sizeof(Words[0]); I++) {
1871:         RomPatchWriteWord32(Rom, Offset + I * 4, Words[I]);
1872:     }
1873: }
1874: 
1875: // SheepShaver-faithful activation of the New World ROM's built-in 68K DR
1876: // emulator. The ROM's ConfigInfo (ROM + 0x30d000) bakes LA_EmulatorCode =
1877: // 0x68060000 / LA_DispatchTable = 0x68080000 (logical RAM addresses the real
1878: // hardware maps to the emulator image); this redirects them into the ROM
1879: // window (ROM + 0x360000 / ROM + 0x380000) so the nanokernel's boot tail
1880: // executes the emulator in place. The `twui r31,n` kernel-trap table (ROM +
1881: // 0x36e8c0) is then rewritten into absolute branches to the emulator-entry
1882: // routines, and the EMUL_OP dispatch markers are installed in the opcode
1883: // table. Must run after PpcInstallLowMemory (writes XLM globals at 0x2800).
1884: STATIC EFI_STATUS
1885: PpcPatchNewWorldRom (
1886:     VOID
1887:     )
1888: {
1889:     UINT8* Rom     = (UINT8*)g_BootContext.RomHostBuffer;
1890:     UINT32 RomBase = (UINT32)g_BootContext.RomAddress;
1891:     UINT32 Struct  = PPC_NEW_WORLD_ROM_BOOT_STRUCT_OFFSET;
1892:     UINT32 TrapBase = 0;
1893:     UINT32 I;
1894: 
1895:     if (Rom == NULL ||
1896:         g_BootContext.RomSize < PPC_NEW_WORLD_ROM_EMUL_OP_END_OFFSET + 8) {
1897:         Print(L"ROM patch skipped: ROM not loaded or too small\n");
1898:         return EFI_UNSUPPORTED;
1899:     }
1900: 
1901:     // ConfigInfo LA fields: keep LA_KernelData/LA_EmulatorData (the 0x68ffxxxx
1902:     // system-area backing), repoint the emulator image at the in-place ROM
1903:     // copy, clear the physical RAM base (baked 0xffffffff), and set the 68K
1904:     // reset vector to the ROM entry (ROM + 0x2a).
1905:     RomPatchWriteWord32(Rom, Struct + 0x9C, 0x68FFE000);  // LA_InfoRecord
1906:     RomPatchWriteWord32(Rom, Struct + 0xA0, 0x68FFE000);  // LA_KernelData
1907:     RomPatchWriteWord32(Rom, Struct + 0xA4, 0x68FFF000);  // LA_EmulatorData
1908:     RomPatchWriteWord32(Rom, Struct + 0xA8,
1909:                         RomBase + PPC_NEW_WORLD_ROM_DISPATCH_TABLE_OFFSET);
1910:     RomPatchWriteWord32(Rom, Struct + 0xAC,
1911:                         RomBase + PPC_NEW_WORLD_ROM_LA_EMULCODE_BASE - PPC_NEW_WORLD_ROM_GUEST_BASE);
1912:     RomPatchWriteWord32(Rom, Struct + 0x360, 0x00000000); // physical RAM base
1913:     RomPatchWriteWord32(Rom, Struct + 0xFD8, RomBase + 0x2A); // 68K reset vector
1914: 
1915:     // Locate the `twui r31,0..2` kernel-trap table (SheepShaver's
1916:     // find_rom_data range; verified at ROM + 0x36e8c0 in the standard image).
1917:     {
1918:         static const UINT8 TwiPattern[12] =
1919:             {0x0F,0xFF,0x00,0x00, 0x0F,0xFF,0x00,0x01, 0x0F,0xFF,0x00,0x02};
1920:         UINT32 Off;
1921:         for (Off = 0x36E600; Off + sizeof(TwiPattern) <= 0x36EA00; Off += 4) {
1922:             BOOLEAN Match = TRUE;
1923:             UINTN B;
1924:             for (B = 0; B < sizeof(TwiPattern); B++) {
1925:                 if (Rom[Off + B] != TwiPattern[B]) { Match = FALSE; break; }
1926:             }
1927:             if (Match) { TrapBase = Off; break; }
1928:         }
1929:         if (TrapBase == 0) {
1930:             Print(L"ROM patch failed: twi trap-table pattern not found\n");
1931:             return EFI_NOT_FOUND;
1932:         }
1933:     }
1934: 
1935:     // Rewrite the 16-word trap table as branches to the entry routines:
1936:     // trap 0 -> emulator start, 1 -> Mixed Mode, 2 -> Reset/FC1E, 3 -> FE0A,
1937:     // 4 -> (interrupt, ILLEGAL), 5 -> FE0F, 6..15 -> ILLEGAL.
1938:     {
1939:         static const UINT32 TrapEntries[16] = {
1940:             0x36F900, 0x36FA00, 0x36FB00, 0x36FC00,
1941:             0x00000000, 0x36FD00, 0x00000000, 0x00000000,
1942:             0x00000000, 0x00000000, 0x00000000, 0x00000000,
1943:             0x00000000, 0x00000000, 0x00000000, 0x00000000
1944:         };
1945:         for (I = 0; I < 16; I++) {
1946:             UINT32 Target = TrapEntries[I];
1947:             UINT32 Word = (Target == 0)
1948:                               ? 0x00000000
1949:                               : (0x48000000 + (Target - (TrapBase + I * 4)));
1950:             RomPatchWriteWord32(Rom, TrapBase + I * 4, Word);
1951:         }
1952:     }
1953: 
1954:     // Install the five 27-word entry routines. The emulator-start fragment
1955:     // (trap 0) branches to the injected 68K DR-emulator entry instead of the
1956:     // plain `blr` so the boot-tail handoff starts the DR emulator directly.
1957:     RomWriteEmulStartRoutine(Rom, 0x36F900, 0x814105F0, 0x4BFFFD98);
1958:     RomWriteEmulStartRoutine(Rom, 0x36FA00, 0x814105F4, 0x4E800020);
1959:     RomWriteEmulStartRoutine(Rom, 0x36FB00, 0x814105F8, 0x4E800020);
1960:     RomWriteEmulStartRoutine(Rom, 0x36FC00, 0x814105FC, 0x4E800020);
1961:     RomWriteEmulStartRoutine(Rom, 0x36FD00, 0x81410604, 0x4E800020);
1962: 
1963:     // The 68K DR-emulator entry + ed.v[0x814] dispatch helper (free NOP region
1964:     // at ROM + 0x36f700..0x36f8fc).
1965:     RomWriteEmulatorEntryRoutine(Rom, 0x36F700);
1966:     RomWriteEmulatorDispatchHelper(Rom, 0x36F7C0);
1967: 
1968:     // The ROM's control-flow dispatch glue bakes `rlwimi r29,r24,0x14,0xb,0xb`
1969:     // (0x531DA2D6) into every branch/jmp path: it copies the low bit of the
1970:     // 68K PC into bit 20 (0x100000) of the dispatch address. For valid 68K
1971:     // code (always word-aligned) that bit is 0, which zeroes bit 20 of the
1972:     // opcode-table base. The dispatch table lives at 0x40b80000 (bit 20 = 1),
1973:     // so every control-flow re-dispatch would land 0x100000 off (in the "kckc"
1974:     // data region) and the boot walks garbage. On real hardware the table base
1975:     // has bit 20 = 0 and the rlwimi is a harmless no-op; with our base it must
1976:     // be neutralised. NOP every occurrence in the emulator image.
1977:     {
1978:         UINT32 Count = 0;
1979:         for (I = 0x360000; I + 4 <= 0x380000; I += 4) {
1980:             if (Rom[I] == 0x53 && Rom[I + 1] == 0x1D &&
1981:                 Rom[I + 2] == 0xA2 && Rom[I + 3] == 0xD6) {
1982:                 RomPatchWriteWord32(Rom, I, 0x60000000);
1983:                 Count++;
1984:             }
1985:         }
1986:         Print(L"68K emulator: neutralised %u rlwimi dispatch-bit-20 words\n", Count);
1987:     }
1988: 
1989:     // Overwrite the opcode-table slots for the EMUL_OP extended opcodes
1990:     // (0xFE40..0xFE40+OP_MAX+2) with POWERPC_EMUL_OP markers ("addi r0,r0,n")
1991:     // followed by `b 0x366084` (re-enter the DR emulator loop). The
1992:     // interpreter intercepts the markers (PpcEmulatorDispatchOp).
1993:     {
1994:         UINT32 Entry = PPC_NEW_WORLD_ROM_EMUL_OP_ENTRY_OFFSET;
1995:         RomPatchWriteWord32(Rom, Entry +  0, PPC_EMUL_OP_MARKER | 0);   // EMUL_RETURN
1996:         RomPatchWriteWord32(Rom, Entry +  4, 0x4BF66E80);
1997:         RomPatchWriteWord32(Rom, Entry +  8, PPC_EMUL_OP_MARKER | 1);   // EXEC_RETURN
1998:         RomPatchWriteWord32(Rom, Entry + 12, 0x4BF66E78);
1999:         RomPatchWriteWord32(Rom, Entry + 16, PPC_EMUL_OP_MARKER | 2);   // EXEC_NATIVE
2000:         RomPatchWriteWord32(Rom, Entry + 20, 0x4BF66E70);
2001:         for (I = 0; I < PPC_OP_MAX; I++) {
2002:             RomPatchWriteWord32(Rom, Entry + 24 + I * 8,
2003:                                 PPC_EMUL_OP_MARKER | (I + 3));
2004:             RomPatchWriteWord32(Rom, Entry + 28 + I * 8, 0x4BF66E68 - I * 8);
2005:         }
2006:     }
2007: 
2008:     // XLM ("eXtra Low Memory") globals the entry routines read; they sit above
2009:     // the 0x0-0x1800 low-memory area the nanokernel zeroes during its boot.
2010:     BootWriteWord32(PPC_XLM_SIGNATURE_OFFSET,   0x42616168);       // 'Baah'
2011:     BootWriteWord32(PPC_XLM_KERNEL_DATA_OFFSET, 0x0000A000);       // NK KDP
2012:     BootWriteWord32(PPC_XLM_TOC_OFFSET,         0x00000000);
2013:     BootWriteWord32(PPC_XLM_SHEEP_OBJ_OFFSET,   0x00000000);
2014:     BootWriteWord32(PPC_XLM_RUN_MODE_OFFSET,    0x00000000);       // MODE_68K
2015:     BootWriteWord32(PPC_XLM_68K_R25_OFFSET,     0x00000000);
2016:     BootWriteWord32(PPC_XLM_IRQ_NEST_OFFSET,    0x00000000);
2017:     BootWriteWord32(PPC_XLM_PVR_OFFSET,         0x00000000);
2018:     BootWriteWord32(PPC_XLM_BUS_CLOCK_OFFSET,   50000000);
2019: 
2020:     Print(L"68K emulator patched: LA_EmulatorCode 0x%08x LA_DispatchTable 0x%08x "
2021:           L"trap table at ROM+0x%x -> emulator start 0x%08x\n",
2022:           RomBase + 0x360000, RomBase + 0x380000, TrapBase,
2023:           RomBase + 0x36F900);
2024:     return EFI_SUCCESS;
2025: }
2026: 
2027: EFI_STATUS
2028: PpcPrepareSystemForBoot (
2029:     VOID
2030:     )
2031: {
2032:     UINT64 RamBase = 0;
2033:     UINT64 RamSize = 0;
2034:     UINTN  I;
2035:     EFI_STATUS Status;
2036: 
2037:     Print(L"Preparing system for boot\n");
2038: 
2039:     // The guest memory map must be ready before the CPU can start.
2040:     if (!g_BootContext.LowMemoryInstalled) {
2041:         PpcInstallLowMemory(NULL, NULL);
2042:     }
2043:     // The nanokernel keeps its kernel stack/heap in the 0x68000000 system area
2044:     // (fixed 0x68F0xxxx logical addresses); back it so those accesses land in
2045:     // RAM instead of reading as zero.
2046:     PpcInstallNkSystemArea();
2047:     if (!g_BootContext.RomLoaded) {
2048:         Print(L"Warning: no system ROM installed; boot would fail at the reset vector\n");
2049:     } else if (g_BootContext.RomType == PPC_ROM_TYPE_NEW_WORLD) {
2050:         // Patch the ROM's built-in 68K emulator in place (SheepShaver
2051:         // style): the ROM window is writable, so the ConfigInfo LA fields,
2052:         // the twi kernel-trap table, the emulator-entry routines and the
2053:         // EMUL_OP dispatch markers are all installed directly in the ROM.
2054:         Status = PpcPatchNewWorldRom();
2055:         if (EFI_ERROR(Status)) {
2056:             Print(L"Warning: 68K emulator ROM patch failed: %r\n", Status);
2057:         }
2058:     }
2059: 
2060:     Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);
2061:     if (EFI_ERROR(Status) || RamSize == 0) {
2062:         Print(L"System preparation failed: guest RAM unavailable\n");
2063:         return EFI_NOT_READY;
2064:     }
2065: 
2066:     // Reset the CPU to the classic Mac OS boot state: PC at the ROM reset
2067:     // vector with machine-check and recoverable-interrupt handling enabled.
2068:     ZeroMem(&g_PpcContext, sizeof(g_PpcContext));
2069:     for (I = 0; I < 32; I++) {
2070:         g_PpcContext.Gpr[I] = 0;
2071:     }
2072:     g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;
2073:     g_PpcContext.Pc = PPC_RESET_VECTOR;
2074:     g_PpcContext.Srr0 = PPC_RESET_VECTOR;
2075:     g_PpcContext.Srr1 = g_PpcContext.Msr;
2076:     g_PpcContext.ExceptionPending = 0;
2077: 
2078:     // Write the emulator boot info block into low memory: magic, then
2079:     // RAM base, RAM size, ROM base, installed ROM size, and ROM type
2080:     // (big-endian).
2081:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET, 0x45464921);
2082:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 0, (UINT32)RamBase);
2083:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 4, (UINT32)RamSize);
2084:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 8,
2085:                     (UINT32)g_BootContext.RomAddress);
2086:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 12,
2087:                     (UINT32)(g_BootContext.RomLoaded ? g_BootContext.RomSize : 0));
2088:     BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 16,
2089:                     g_BootContext.RomType);
2090: 
2091:     g_BootContext.SystemReady = TRUE;
2092:     g_BootContext.SystemBooting = TRUE;
2093: 
2094:     Print(L"System prepared: PC=0x%x MSR=0x%08x SRR0=0x%x SRR1=0x%x\n",
2095:           g_PpcContext.Pc, g_PpcContext.Msr, g_PpcContext.Srr0, g_PpcContext.Srr1);
2096:     Print(L"Boot info block written to low memory at 0x%x\n",
2097:           PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET);
2098: 
2099:     return EFI_SUCCESS;

(Showing lines 1850-2099 of 2612. Use offset=2100 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 5.9s)

_Thinking:_

Now I have a thorough understanding of the project. Let me also check the build status and look at the mac_roms and mac_discs directories to understand what assets are available:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\mac_roms"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\mac_roms</path>
<type>directory</type>
<entries>
Old_World_Mac_Roms.zip

(1 entries)
</entries>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\mac_discs"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\mac_discs</path>
<type>directory</type>
<entries>
Apple Mac OS 9.2.2 [PowerMac G4].7z
MacOS 8 (Version 8.1)(691-1912-A)(Apple Computer, Inc.)(1998).iso
System7_5_3.img

(3 entries)
</entries>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\USER_GUIDE.md"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\USER_GUIDE.md</path>
<type>file</type>
<content>
1: # EFI Mac OS Boot Layer — User Guide
2: 
3: ## Status
4: 
5: The boot layer is functional as a UEFI application: it builds, boots under
6: QEMU/OVMF, runs its self-tests, stages System Folder files and drivers from real
7: classic Mac discs, and detects and installs a genuine New World `Mac OS ROM`
8: from a Mac OS 8.5+/9 disc. **It does not yet boot a Mac OS guest** — there is no
9: MMU or continuous execution, so the installed ROM is not run for real. This
10: guide covers what works today and how to exercise it.
11: 
12: ## What You Need
13: 
14: - **Host**: x86_64 with QEMU (and OVMF firmware). Windows and macOS builds are
15:   supported; see [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md).
16: - **A classic Mac disc image** for testing: a System 7 floppy/disc image
17:   (raw HFS, e.g. `System7_5_3.img`), or a Mac OS 8/9 install ISO
18:   (e.g. `Mac OS 9.2.2.iso`).
19: - **Firmware** (optional but recommended): a genuine Old World ROM dump for
20:   System 7. A New World `Mac OS ROM` is auto-discovered from Mac OS 8.5+ discs.
21: 
22: ## Build
23: 
24: Windows (git-bash):
25: 
26: ```bash
27: bash scripts/build-windows.sh
28: ```
29: 
30: macOS/Linux (`brew install llvm lld`):
31: 
32: ```bash
33: make
34: make check
35: ```
36: 
37: Output: `build/EFI-Mac-Emulator.efi`.
38: 
39: ## Run Under QEMU
40: 
41: A helper script builds an OVMF ESP and captures the serial log:
42: 
43: ```powershell
44: powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run-qemu-windows.ps1
45: ```
46: 
47: Attach a Mac disc:
48: 
49: ```powershell
50: powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run-qemu-windows.ps1 `
51:   -MacDisc "$env:TEMP\opencode\mac\Mac OS 9.2.2.iso"
52: ```
53: 
54: The serial log lands at `$env:TEMP\opencode\boot_out.txt`. Check the result:
55: 
56: ```powershell
57: Select-String -Path "$env:TEMP\opencode\boot_out.txt" -Pattern "self-test complete|Boot state"
58: ```
59: 
60: What to expect:
61: 
62: - PowerPC CPU self-test 35/35 (includes the FPU core).
63: - With a Mac OS 8.5+ disc attached: the real New World ROM is found and
64:   installed (`System ROM loaded from HFS volume 'Power Mac G4 Install':
65:   2763530 bytes` → `System ROM installed: ... (New World)`), and the boot
66:   memory-map self-test passes 5/5.
67: - Without such a disc: the demo ROM is installed and the boot self-test passes
68:   7/7.
69: - System Folder staging: System, Finder, and up to 64 Extensions are staged from
70:   the disc; the system-files self-test passes 7/7.
71: - The app reports `Boot state: ready=1 ...` and returns cleanly to the firmware.
72: 
73: ## Providing a ROM
74: 
75: The ROM loader uses this priority:
76: 
77: 1. `\System\MacOS\ROM` on the EFI System Partition — an **Old World** firmware
78:    dump. Required for System 7.
79: 2. `\System Folder\Extensions\Mac OS ROM` on the ESP.
80: 3. `Mac OS ROM` auto-discovered on an attached disc (New World, Mac OS 8.5+).
81: 4. Demo ROM fallback (self-check only).
82: 
83: To test Old World path: copy a real firmware dump to
84: `esp\System\MacOS\ROM` in the ESP directory used by the run script, then boot.
85: The log will print `System ROM installed: ... (Old World)`.
86: 
87: ## What Works Today
88: 
89: - Full UEFI initialization: GOP framebuffer (blits verified pixel-by-pixel),
90:   Block I/O (real sector reads), Simple Network Protocol (real frame transmit),
91:   audio ring buffer self-checks.
92: - In-emulator HFS reading of System 7 / Mac OS 8 / Mac OS 9 disc images:
93:   catalog lookup, auto block-size, multi-extent files.
94: - Real New World ROM discovery + install at `0xFFF00000` (read-only) with CHRP
95:   signature verification.
96: - System Folder / driver staging into guest staging areas with read-back
97:   verification.
98: 
99: ## Limitations
100: 
101: - The guest OS does not boot: the ROM is installed but not executed (no MMU, no
102:   continuous fetch/execute, no Mac device register emulation).
103: - System 7 requires a user-supplied Old World ROM; a New World `Mac OS ROM`
104:   cannot serve System 7.
105: - HFS filename bytes are MacRoman/Latin-1; names with high-bit characters print
106:   as console garbage (cosmetic).
107: - Classic Mac OS files are copyrighted by Apple; none are included in this
108:   repository.
109: 
110: ## Troubleshooting
111: 
112: 1. **No serial log / app not running**: confirm OVMF is present and the image is
113:    named `EFI\BOOT\BOOTX64.EFI` on a FAT partition (the run script does this).
114: 2. **Demo ROM installed unexpectedly**: the disc either has no `Mac OS ROM` (a
115:    pre-8.5 disc) or it was not detected. Check the log lines around `System ROM
116:    not found` / `Mac OS ROM file not found`.
117: 3. **System 7 disc staged nothing**: System 7.5.3's `Finder` is a genuine
118:    0-byte stub and is skipped; System and the two real Extensions still stage.
119: 
120: ## License
121: 
122: GPL-3.0-or-later. See [LICENSE](LICENSE).

(End of file - total 122 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 5.4s)

_Thinking:_

Now I have a very thorough understanding of the project. Let me read the rest of the PpcRunGuest to see how the execution loop currently works with the native 68K interpreter hook:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 300,
  "offset": 4400
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
4400:                   g_PpcContext.Gpr[1], g_PpcContext.Gpr[8],
4401:                   g_PpcContext.Gpr[24], g_PpcContext.Gpr[27],
4402:                   g_PpcContext.Gpr[29], g_PpcContext.Gpr[30],
4403:                   g_PpcContext.Gpr[31], g_PpcContext.Lr);
4404:         }
4405:         // First step into the ROM data region where the failed boot walks.
4406:         if (WalkProbed == 0 && Current >= 0x40AFC000 && Current < 0x40B00000) {
4407:             WalkProbed = 1;
4408:             Print(L"  WALK@0x%08x r1=0x%08x r8=0x%08x r23=0x%08x r24=0x%08x r25=0x%08x "
4409:                   L"r27=0x%08x r28=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x "
4410:                   L"LR=0x%08x CR=0x%08x SRR0=0x%08x MSR=0x%08x CTR=0x%08x\n",
4411:                   Current, g_PpcContext.Gpr[1], g_PpcContext.Gpr[8],
4412:                   g_PpcContext.Gpr[23], g_PpcContext.Gpr[24], g_PpcContext.Gpr[25],
4413:                   g_PpcContext.Gpr[27], g_PpcContext.Gpr[28], g_PpcContext.Gpr[29],
4414:                   g_PpcContext.Gpr[30], g_PpcContext.Gpr[31],
4415:                   g_PpcContext.Lr, g_PpcContext.Cr, g_PpcContext.Srr0,
4416:                   g_PpcContext.Msr, g_PpcContext.Ctr);
4417:             Print(L"  WALK prev w[0x%08x-4]=0x%08x w[+4]=0x%08x [r24]=0x%08x "
4418:                   L"[0x4080002a]=0x%04x [0x4080002c]=0x%04x [0xb814]=0x%08x "
4419:                   L"[0xb074]=0x%08x\n",
4420:                   Current, CpuRead32(Current - 4), CpuRead32(Current + 4),
4421:                   CpuRead32(g_PpcContext.Gpr[24]),
4422:                   CpuRead16(0x4080002A), CpuRead16(0x4080002C),
4423:                   CpuRead32(0xB814), CpuRead32(0xB074));
4424:         }
4425:         // Arrival at the NK call-table[0] target after the emulator-start
4426:         // routine's blr. Dump the runtime code once to see whether the NK
4427:         // installed its own glue here (the ROM file has zeros in this region).
4428:         if (EmulWindowProbed == 0 && Current >= 0x40B6E800 && Current < 0x40B6FF00) {
4429:             EmulWindowProbed = 1;
4430:             Print(L"  EMULWIN@0x%08x r1=0x%08x r3=0x%08x r4=0x%08x LR=0x%08x MSR=0x%08x\n",
4431:                   Current, g_PpcContext.Gpr[1], g_PpcContext.Gpr[3],
4432:                   g_PpcContext.Gpr[4], g_PpcContext.Lr, g_PpcContext.Msr);
4433:             Print(L"  EMULWIN w[0x%08x]=0x%08x w[0x%08x]=0x%08x w[0x%08x]=0x%08x\n",
4434:                   Current, CpuRead32(Current), Current + 4, CpuRead32(Current + 4),
4435:                   Current + 8, CpuRead32(Current + 8));
4436:         }
4437:         if (CallTblProbed == 0 && Current == 0x40B13BF8) {
4438:             UINT32 A;
4439:             CallTblProbed = 1;
4440:             Print(L"  CALLTBL@0x%08x (blr from emulator start) r1=0x%08x r10=0x%08x "
4441:                   L"r11=0x%08x LR=0x%08x MSR=0x%08x\n",
4442:                   Current, g_PpcContext.Gpr[1], g_PpcContext.Gpr[10],
4443:                   g_PpcContext.Gpr[11], g_PpcContext.Lr, g_PpcContext.Msr);
4444:             for (A = Current; A < Current + 64; A += 16) {
4445:                 Print(L"  CALLTBL[0x%08x] %08x %08x %08x %08x\n",
4446:                       A, CpuRead32(A), CpuRead32(A + 4),
4447:                       CpuRead32(A + 8), CpuRead32(A + 0xC));
4448:             }
4449:         }
4450:         // The NK syscall site at the tail of the task/event loop: r0 carries
4451:         // the syscall number, r3/r4 the args, and the handler's rfi must
4452:         // return to sc+4 (0x40B24FDC) so the `cmpwi r3,0` result check runs.
4453:         if (Current == 0x40B24FD8 && ScSiteCount < 4) {
4454:             ScSiteCount++;
4455:             Print(L"  SCSITE[%u] r0=0x%08x r3=0x%08x r4=0x%08x r31=0x%08x "
4456:                   L"LR=0x%08x SRR0=0x%08x MSR=0x%08x DEC=0x%08x TBL=0x%08x TBU=0x%08x\n",
4457:                   ScSiteCount, g_PpcContext.Gpr[0], g_PpcContext.Gpr[3],
4458:                   g_PpcContext.Gpr[4], g_PpcContext.Gpr[31],
4459:                   g_PpcContext.Lr, g_PpcContext.Srr0, g_PpcContext.Msr,
4460:                   g_PpcContext.Spr[22], g_PpcContext.TimeBaseL,
4461:                   g_PpcContext.TimeBaseH);
4462:         }
4463:         if (ScSiteCount >= 1 && Current == 0x40B24FDC) {
4464:             Print(L"  SCSEVRET r3=0x%08x r0=0x%08x r31=0x%08x LR=0x%08x\n",
4465:                   g_PpcContext.Gpr[3], g_PpcContext.Gpr[0],
4466:                   g_PpcContext.Gpr[31], g_PpcContext.Lr);
4467:         }
4468:         // NK syscall dispatch: r15 = syscall number (restored from
4469:         // [ECB+0x104]), table base loaded via `lis r16,imm; ori r16,r16,imm`
4470:         // at 0x40B1AEF4/0x40B1AEF8 -- the NK relocates these (patches the
4471:         // `lis` high half), so read the live table base from the instructions.
4472:         if (SyscallDispatchProbed < 8 && Current == 0x40B1AED0) {
4473:             UINT32 Lis = CpuRead32(0x40B1AEF4);
4474:             UINT32 Ori = CpuRead32(0x40B1AEF8);
4475:             UINT32 Base = ((Lis & 0xFFFF) << 16) | (Ori & 0xFFFF);
4476:             UINT32 N = g_PpcContext.Gpr[15];
4477:             UINT32 Entry = CpuRead32(Base + (N & 0xFF) * 4);
4478:             SyscallDispatchProbed++;
4479:             Print(L"  SYSDISP[%u] n=0x%x (r15) lis=0x%08x ori=0x%08x "
4480:                   L"tblbase=0x%08x entry=0x%08x target=0x%08x r3=0x%08x r4=0x%08x r14=0x%08x\n",
4481:                   SyscallDispatchProbed, N, Lis, Ori, Base, Entry,
4482:                   Base + (N & 0xFF) * 4 + Entry,
4483:                   g_PpcContext.Gpr[3], g_PpcContext.Gpr[4], g_PpcContext.Gpr[14]);
4484:             Print(L"  SYSDISP tbl[0..20]=");
4485:             for (N = 0; N < 20; N++) {
4486:                 Print(L"%08x ", CpuRead32(Base + N * 4));
4487:             }
4488:             Print(L"\n");
4489:             Print(L"  SYSDISP KDP-0x338=[0x9CC8]=0x%08x [r22+0x38]=0x%08x "
4490:                   L"[r22+0x44]=0x%08x KDP+0x65C=0x%08x KDP+0x5F0=0x%08x "
4491:                   L"[r22+0x4C]=0x%08x\n",
4492:                   CpuRead32(0x9CC8), CpuRead32(CpuRead32(0x9CC8) + 0x38),
4493:                   CpuRead32(CpuRead32(0x9CC8) + 0x44),
4494:                   CpuRead32(0xA65C), CpuRead32(0xA5F0),
4495:                   CpuRead32(CpuRead32(0x9CC8) + 0x4C));
4496:         }
4497:         if (EmulTrapProbed == 0 && Current == PPC_EMULATOR_TRAP_TABLE) {
4498:             EmulTrapProbed = 1;
4499:             Print(L"  EMUTRAP@0x%08x (patched: b 0x36f900) r1=0x%08x r4=0x%08x "
4500:                   L"LR=0x%08x MSR=0x%08x\n",
4501:                   Current, g_PpcContext.Gpr[1], g_PpcContext.Gpr[4],
4502:                   g_PpcContext.Lr, g_PpcContext.Msr);
4503:             // The nanokernel zeroed low memory during its boot, wiping the XLM
4504:             // globals PpcPatchNewWorldRom wrote. Restore them at the exact
4505:             // moment of the 68K handoff: the emulator-start routine reads
4506:             // XLM_IRQ_NEST [0x2818] and XLM_KERNEL_DATA [0x2804] as its first
4507:             // instructions (after this instruction has already executed).
4508:             CpuWrite32(PPC_XLM_SIGNATURE_OFFSET,   0x42616168);  // 'Baah'
4509:             CpuWrite32(PPC_XLM_KERNEL_DATA_OFFSET, 0x0000A000);  // NK KDP
4510:             CpuWrite32(PPC_XLM_TOC_OFFSET,         0x00000000);
4511:             CpuWrite32(PPC_XLM_SHEEP_OBJ_OFFSET,   0x00000000);
4512:             CpuWrite32(PPC_XLM_RUN_MODE_OFFSET,    0x00000000);  // MODE_68K
4513:             CpuWrite32(PPC_XLM_68K_R25_OFFSET,     0x00000000);
4514:             CpuWrite32(PPC_XLM_IRQ_NEST_OFFSET,    0x00000000);
4515:             CpuWrite32(PPC_XLM_PVR_OFFSET,         0x00000000);
4516:             CpuWrite32(PPC_XLM_BUS_CLOCK_OFFSET,   50000000);
4517:             Print(L"  EMUTRAP XLM restored: [2800]=0x%08x [2804]=0x%08x "
4518:                   L"[2818]=0x%08x\n",
4519:                   CpuRead32(PPC_XLM_SIGNATURE_OFFSET),
4520:                   CpuRead32(PPC_XLM_KERNEL_DATA_OFFSET),
4521:                   CpuRead32(PPC_XLM_IRQ_NEST_OFFSET));
4522:             // Seed the 68K DR-emulator context: the emulator-start routine
4523:             // saves the interrupted 68K context to [ECB+0x13C..] and reads
4524:             // KDP.ECB; the injected entry routine and the DR emulator read
4525:             // ed.v[0x74]/[0x78] (opcode table / emulator base) and call the
4526:             // ed.v[0x814] dispatch helper. ECB+0x1CC holds the interrupt
4527:             // pending bits (& 7 == 7 per the state machine).
4528:             CpuWrite32(0x0000A634, 0x0000B000);  // KDP.PA_EmulatorData
4529:             CpuWrite32(0x0000A65C, 0x0000B100);  // KDP.ECB
4530:             CpuWrite32(0x0000B074, 0x40B80000);  // ed.v[0x74] opcode table
4531:             CpuWrite32(0x0000B078, 0x40B60000);  // ed.v[0x78] emulator base
4532:             CpuWrite32(0x0000B814, 0x40B6F7C0);  // ed.v[0x814] dispatch helper
4533:             CpuWrite32(0x0000B2CC, 0x00000007);  // ECB+0x1CC interrupt pending
4534:             Print(L"  EMUTRAP DR context seeded: [A634]=0x%08x [A65C]=0x%08x "
4535:                   L"[B074]=0x%08x [B078]=0x%08x [B814]=0x%08x [B2CC]=0x%08x\n",
4536:                   CpuRead32(0xA634), CpuRead32(0xA65C), CpuRead32(0xB074),
4537:                   CpuRead32(0xB078), CpuRead32(0xB814), CpuRead32(0xB2CC));
4538:         }
4539:         if (StoreProbed == 0 && (Current == 0x40B11B64 || Current == 0x40B11B48)) {
4540:             UINT32 P = g_PpcContext.Gpr[1];
4541:             UINT32 T;
4542:             StoreProbed = 1;
4543:             Print(L"  STOREPROBE@0x%08x (before) r1=0x%08x r8=0x%08x r9=0x%08x r16=0x%08x r28=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x\n",
4544:                   Current, P, g_PpcContext.Gpr[8], g_PpcContext.Gpr[9],
4545:                   g_PpcContext.Gpr[16], g_PpcContext.Gpr[28], g_PpcContext.Gpr[29],
4546:                   g_PpcContext.Gpr[30], g_PpcContext.Gpr[31]);
4547:             Print(L"  STOREPROBE PA_CurAS[r1-1C]=0x%08x PA_PSA[r1-18]=0x%08x PA_KDP[r1-4]=0x%08x\n",
4548:                   CpuRead32(P - 0x1C), CpuRead32(P - 0x18), CpuRead32(P - 0x04));
4549:             Print(L"  STOREPROBE PA_ConfigInfo[r1+648]=0x%08x [r1+64C]=0x%08x\n",
4550:                   CpuRead32(P + 0x648), CpuRead32(P + 0x64C));
4551:             Print(L"  STOREPROBE FreePool[r1-AB0]=0x%08x FirstSeg[r1-AA0]=0x%08x FirstSegLogi[r1-A9C]=0x%08x\n",
4552:                   CpuRead32(P - 0xAB0), CpuRead32(P - 0xAA0), CpuRead32(P - 0xA9C));
4553:             Print(L"  STOREPROBE mem@0x8C40:\n");
4554:             for (T = 0x8C40; T < 0x8D40; T += 16) {
4555:                 Print(L"    0x%08x: %08x %08x %08x %08x\n",
4556:                       T, CpuRead32(T), CpuRead32(T + 4), CpuRead32(T + 8), CpuRead32(T + 0xC));
4557:             }
4558:         }
4559:         if (RamProbed == 0 && Current == 0x40B1243C) {
4560:             UINT32 P = g_PpcContext.Gpr[1];
4561:             UINT32 T;
4562:             RamProbed = 1;
4563:             Print(L"  RAMPROBE@0x%08x r1=0x%08x r17=0x%08x r18=0x%08x r19=0x%08x r21=0x%08x r22=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x\n",
4564:                   Current, P, g_PpcContext.Gpr[17], g_PpcContext.Gpr[18],
4565:                   g_PpcContext.Gpr[19], g_PpcContext.Gpr[21], g_PpcContext.Gpr[22],
4566:                   g_PpcContext.Gpr[29], g_PpcContext.Gpr[30], g_PpcContext.Gpr[31]);
4567:             Print(L"  RAMPROBE loc[1704]=0x%08x loc[1708]=0x%08x loc[1716]=0x%08x loc[1592]=0x%08x loc[1596]=0x%08x loc[-32]=0x%08x abs[6A8]=0x%08x abs[6AC]=0x%08x\n",
4568:                   CpuRead32(P + 0x6A8), CpuRead32(P + 0x6AC), CpuRead32(P + 0x6B4),
4569:                   CpuRead32(P + 0x638), CpuRead32(P + 0x63C), CpuRead32(P - 0x20),
4570:                   CpuRead32(0x000006A8), CpuRead32(0x000006AC));
4571:             Print(L"  RAMPROBE memmap@r1+120:\n");
4572:             for (T = P + 0x78; T < P + 0x178; T += 16) {
4573:                 Print(L"    0x%08x: %08x %08x %08x %08x\n",
4574:                       T, CpuRead32(T), CpuRead32(T + 4), CpuRead32(T + 8), CpuRead32(T + 0xC));
4575:             }
4576:         }
4577:         if (AllocTraced < 60 && Current == 0x40B22828) {
4578:             UINT32 R1 = g_PpcContext.Gpr[1];
4579:             Print(L"  ALLOCENTRY[%d] size=0x%08x r9=0x%08x LR=0x%08x FreeNext=0x%08x FreePageCnt=0x%08x FreeList=0x%08x\n",
4580:                   AllocTraced, g_PpcContext.Gpr[8], g_PpcContext.Gpr[9],
4581:                   g_PpcContext.Lr, CpuRead32(R1 - 0xAB0 + 8),
4582:                   CpuRead32(R1 - 0x430), CpuRead32(R1 - 0x448));
4583:         }
4584:         if (AllocTraced < 200 && Current == 0x40B228D8) {
4585:             UINT32 R1 = g_PpcContext.Gpr[1];
4586:             Print(L"  ALLOCWALK[%d] block=0x%08x blocksize=0x%08x req=0x%08x sig=0x%08x FreeNext=0x%08x\n",
4587:                   AllocTraced, g_PpcContext.Gpr[15], CpuRead32(g_PpcContext.Gpr[15]),
4588:                   g_PpcContext.Gpr[8], CpuRead32(g_PpcContext.Gpr[15] + 4),
4589:                   CpuRead32(R1 - 0xAB0 + 8));
4590:         }
4591:         if (AllocTraced < 1 && Current >= 0x40B22820 && Current <= 0x40B228E4) {
4592:             Print(L"  ALLOCSTEP[%d] PC=0x%08x next=0x%08x r8=0x%08x r15=0x%08x r16=0x%08x r17=0x%08x r18=0x%08x CR=0x%08x\n",
4593:                   AllocTraced, Current, Next, g_PpcContext.Gpr[8], g_PpcContext.Gpr[15],
4594:                   g_PpcContext.Gpr[16], g_PpcContext.Gpr[17], g_PpcContext.Gpr[18],
4595:                   g_PpcContext.Cr);
4596:         }
4597:         if (AllocTraced < 120 && Current == 0x40B229D4) {
4598:             UINT32 R1 = g_PpcContext.Gpr[1];
4599:             UINT32 R = g_PpcContext.Gpr[8];
4600:             Print(L"  ALLOCRET[%d] ret=0x%08x LR=0x%08x FreeHead=0x%08x sig=0x%08x offnext=0x%08x\n",
4601:                   AllocTraced, R, g_PpcContext.Lr, CpuRead32(R1 - 0xAB0 + 8),
4602:                   CpuRead32(R - 4), CpuRead32(R - 8));
4603:             AllocTraced++;
4604:         }
4605:         // PMDT chunk-pointer array: the walk reads the PMDT base for each 256MB
4606:         // chunk via 'lwzu r25, 8(r27)' (r27 = r1 + 0x78), so the 32-bit pointers
4607:         // live at [r1+0x80 + 8k]. Dump them once to see how many chunks are
4608:         // populated and whether adjacent slots alias the same table.
4609:         if (PmdArrDump == 0 && Current == 0x40B1F404) {
4610:             UINT32 R1 = g_PpcContext.Gpr[1];
4611:             UINT32 K;
4612:             PmdArrDump = 1;
4613:             Print(L"  PMDTARR r1=0x%08x r26=0x%08x pointers [r1+0x80+8k]:\n",
4614:                   R1, g_PpcContext.Gpr[26]);
4615:             for (K = 0; K < 16; K++) {
4616:                 Print(L"    k=%2d @0x%08x: 0x%08x\n", K, R1 + 0x80 + 8 * K,
4617:                       CpuRead32(R1 + 0x80 + 8 * K));
4618:             }
4619:         }
4620:         // PMDT RAM injection (one-shot): the DR=1 boot path skips the NK's PMDT
4621:         // builder, so the table only has the [0xFFF7,9] top-of-block-0
4622:         // reservation followed by 63 zero entries. The walk dispatches on
4623:         // flags&0xE00: 0 = area create, 0xC00 = special area, any other value
4624:         // with page=0 && count=0xFFFF = chunk terminator (r26 += 256MB).
4625:         // Rewrite chunk 0's table as reservation + [0,0xFFF6) RAM + a real
4626:         // terminator, and point chunks 1..15 at the terminator entry so each
4627:         // 256MB chunk walks cleanly and the walk ends when r26 wraps.
4628:         if (PmdFixed == 0 && Current == 0x40B1F418) {
4629:             UINT32 Base = g_PpcContext.Gpr[25];
4630:             UINT32 R1  = g_PpcContext.Gpr[1];
4631:             UINT32 K;
4632:             PmdFixed = 1;
4633:             // chunk 0 table: r25 was already loaded from [r1+0x78] (original
4634:             // pointer array); rewrite it explicitly for self-consistency.
4635:             CpuWrite32(R1 + 0x78, Base);
4636:             // chunks 1..15: point at the terminator-only entry (no areas).
4637:             for (K = 1; K < 16; K++) {
4638:                 CpuWrite32(R1 + 0x80 + 8 * K, Base + 16);
4639:             }
4640:             // entry 0 [0xFFF7,9] already holds the top-of-block-0 reservation.
4641:             // entry 1: RAM [0, 0xFFF7000).
4642:             CpuWrite16(Base + 8, 0x0000);
4643:             CpuWrite16(Base + 10, 0xFFF6);
4644:             CpuWrite32(Base + 12, 0x00000000);
4645:             // entry 2: chunk terminator, flags&0xE00 = 0x400 (not 0, not 0xC00).
4646:             CpuWrite16(Base + 16, 0x0000);
4647:             CpuWrite16(Base + 18, 0xFFFF);
4648:             CpuWrite32(Base + 20, 0x00000400);
4649:             // entries 3..63: unreachable (chunk advances at entry 2), but keep
4650:             // them as terminators so a stray entry can never walk an area.
4651:             for (K = 24; K < 64 * 8; K += 8) {
4652:                 CpuWrite16(Base + K, 0x0000);
4653:                 CpuWrite16(Base + K + 2, 0xFFFF);
4654:                 CpuWrite32(Base + K + 4, 0x00000400);
4655:             }
4656:             Print(L"  PMDTINJECT base=0x%08x chunk0=[0xFFF7,9]+[0,0xFFF6]+TERM chunks1..15=TERM\n",
4657:                   Base);
4658:         }
4659:         // PMDT table dump: 0x40B1F418 ('lwz r17, 4(r25)') is the top of the
4660:         // per-chunk entry scan; r25 holds the current 8-byte entry base. Dump 64
4661:         // entries from the first entry read to see the table the walk is scanning.
4662:         if (PmdWalked < 1 && Current == 0x40B1F418) {
4663:             UINT32 Base = g_PpcContext.Gpr[25];
4664:             UINT32 I;
4665:             PmdWalked++;
4666:             Print(L"  PMDTDUMP r25=0x%08x r26=0x%08x r27=0x%08x r1=0x%08x 64 entries:\n",
4667:                   Base, g_PpcContext.Gpr[26], g_PpcContext.Gpr[27],
4668:                   g_PpcContext.Gpr[1]);
4669:             for (I = 0; I < 64; I++) {
4670:                 UINT32 E = Base + I * 8;
4671:                 Print(L"    PMDT[%2d] @0x%08x page=0x%04x count=0x%04x type=0x%08x\n",
4672:                       I, E, CpuRead16(E), CpuRead16(E + 2), CpuRead32(E + 4));
4673:             }
4674:         }
4675:         // PMDT per-entry read: at 0x40B1F428 (after andi. r17,r8,0xE00) r25 is the
4676:         // entry base, r15=page, r16=count, r17=type&0xE00, r8=type, r26=chunk base.
4677:         if (PmdEntry < 40 && Current == 0x40B1F428) {
4678:             PmdEntry++;
4679:             Print(L"  PMDENTRY[%d] base=0x%08x page=0x%04x count=0x%04x type=0x%08x r8=0x%08x r26=0x%08x r27=0x%08x\n",
4680:                   PmdEntry, g_PpcContext.Gpr[25], g_PpcContext.Gpr[15],
4681:                   g_PpcContext.Gpr[16], g_PpcContext.Gpr[17],
4682:                   g_PpcContext.Gpr[8], g_PpcContext.Gpr[26],
4683:                   g_PpcContext.Gpr[27]);
4684:         }
4685:         // Merge path: 0x40B1F668 is reached via the beq at 0x40B1F614 when
4686:         // [new+0x24] == [existing+0x24]. r24 = existing area, r31 = new area.
4687:         // Log the fields that the guard at 0x40B1F67C compares: the 0x28 fields
4688:         // are never written by the creation code, so they should be pool garbage.
4689:         if (MergeTraced < 8 && Current == 0x40B1F668) {
4690:             UINT32 Ex = g_PpcContext.Gpr[24];
4691:             UINT32 Nw = g_PpcContext.Gpr[31];
4692:             MergeTraced++;
4693:             Print(L"  MERGE[%d] existing=0x%08x new=0x%08x [ex+0x24]=0x%08x [ex+0x28]=0x%08x [ex+0x2C]=0x%08x [new+0x24]=0x%08x [new+0x28]=0x%08x [new+0x2C]=0x%08x r25=0x%08x r26=0x%08x r15=0x%08x r16=0x%08x r9=0x%08x\n",
4694:                   MergeTraced, Ex, Nw,
4695:                   CpuRead32(Ex + 0x24), CpuRead32(Ex + 0x28), CpuRead32(Ex + 0x2C),
4696:                   CpuRead32(Nw + 0x24), CpuRead32(Nw + 0x28), CpuRead32(Nw + 0x2C),
4697:                   g_PpcContext.Gpr[25], g_PpcContext.Gpr[26],
4698:                   g_PpcContext.Gpr[15], g_PpcContext.Gpr[16], g_PpcContext.Gpr[9]);
4699:         }

(Showing lines 4400-4699 of 5348. Use offset=4700 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 17.1s)

_Thinking:_

Now I have a comprehensive understanding of the project. Let me also look at what the UI implementation does to understand the config menu flow:

Now I have a thorough understanding of the project. Let me look at the rest of the PpcRunGuest to understand the current execution flow and where it stops:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 300,
  "offset": 4700
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
4700:         // Banner CR/LF flush-tail diagnostics. The guest spins at the SCC
4701:         // Tx-empty poll (PC=0x40B26500, LBZ 2(r28) / ANDI. bit 2) because the
4702:         // SCC base register r28 is 0, so the poll reads guest 0x2 instead of
4703:         // the SCC at 0x20002. Log r28 around the flush helper call (bl at
4704:         // 0x40B264D8 to 0x40B28A98) to see whether the helper zeroes r28 or
4705:         // whether the SCC base was never loaded (candidate: PSA NoIdeaR23 at
4706:         // [KDP-0x900], the SCC base `prints` reads via `lwz r28,-0x900(r1)`).
4707:         if (FlushProbed < 4 && (Current == 0x40B264D8 || Current == 0x40B264DC)) {
4708:             UINT32 Ewa = g_PpcContext.Spr[272];
4709:             UINT32 Kdp = CpuRead32(Ewa - 4);
4710:             Print(L"  FLUSHPROBE[%d] @0x%08x r28=0x%08x CR=0x%08x CTR=0x%08x LR=0x%08x\n",
4711:                   FlushProbed, Current, g_PpcContext.Gpr[28], g_PpcContext.Cr,
4712:                   g_PpcContext.Ctr, g_PpcContext.Lr);
4713:             if (Current == 0x40B264D8) {
4714:                 Print(L"  FLUSHPROBE KDP=0x%08x NoIdeaR23[KDP-0x900]=0x%08x [KDP+0xedc]=0x%08x [KDP+0x648]=0x%08x [KDP+0x64c]=0x%08x\n",
4715:                       Kdp, CpuRead32(Kdp - 0x900), CpuRead32(Kdp + 0xedc),
4716:                       CpuRead32(Kdp + 0x648), CpuRead32(Kdp + 0x64c));
4717:             }
4718:             FlushProbed++;
4719:         }
4720:         // Dump the flush helper body once so we can see how it sets r28/CR.
4721:         if (HelperDumped == 0 && Current == 0x40B28A98) {
4722:             UINT32 A;
4723:             HelperDumped = 1;
4724:             Print(L"  FLUSHHELPER dump 0x40B28A74..0x40B28C00:\n");
4725:             for (A = 0x40B28A74; A < 0x40B28C00; A += 16) {
4726:                 Print(L"    0x%08x: %08x %08x %08x %08x\n",
4727:                       A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
4728:             }
4729:         }
4730:         // Log the SCC Tx-empty poll iterations: r29 is the value the LBZ at
4731:         // 0x40B264F8 just read from [r28+2], which must be the SCC status reg
4732:         // (0x20002). If addr != 0x20002 the poll will spin forever.
4733:         if (SccPollTraced < 30 && Current == 0x40B264FC) {
4734:             Print(L"  SCCPOLL[%d] after lbz r29,2(r28): r28=0x%08x addr=0x%08x value=0x%02x\n",
4735:                   SccPollTraced, g_PpcContext.Gpr[28], g_PpcContext.Gpr[28] + 2,
4736:                   g_PpcContext.Gpr[29]);
4737:             SccPollTraced++;
4738:         }
4739:         // Step through the flush helper (0x40B28A98..0x40B28C04) one instruction
4740:         // at a time, printing state BEFORE each instruction executes. State shown
4741:         // at PC=X is therefore the result of the instruction at PC-4.
4742:         if (HelperStep < 45 && Current >= 0x40B28A98 && Current <= 0x40B28C04) {
4743:             UINT32 R1 = g_PpcContext.Gpr[1];
4744:             Print(L"  HELPER[%d] PC=0x%08x r1=0x%08x r14=0x%08x r15=0x%08x r16=0x%08x r26=0x%08x CR=0x%08x CR0=%x CR7=%x LR=0x%08x next=0x%08x [r1-3F0]=0x%08x [r1-3EC]=0x%08x [r1+EDC]=0x%08x\n",
4745:                   HelperStep, Current, R1, g_PpcContext.Gpr[14], g_PpcContext.Gpr[15],
4746:                   g_PpcContext.Gpr[16], g_PpcContext.Gpr[26], g_PpcContext.Cr,
4747:                   (g_PpcContext.Cr >> 28) & 0xF, g_PpcContext.Cr & 0xF,
4748:                   g_PpcContext.Lr, Next, CpuRead32(R1 - 0x3F0), CpuRead32(R1 - 0x3EC),
4749:                   CpuRead32(R1 + 0xEDC));
4750:             HelperStep++;
4751:         }
4752:         TailInst[TailStart] = Instr;
4753:         TailPc[TailStart] = Current;
4754:         TailNext[TailStart] = Next;
4755:         TailR28[TailStart] = g_PpcContext.Gpr[28];
4756:         TailR8[TailStart] = g_PpcContext.Gpr[8];
4757:         TailR17[TailStart] = g_PpcContext.Gpr[17];
4758:         TailLr[TailStart] = g_PpcContext.Lr;
4759:     TailR24[TailStart] = g_PpcContext.Gpr[24];
4760:     TailR27[TailStart] = g_PpcContext.Gpr[27];
4761:     TailR7[TailStart] = g_PpcContext.Gpr[7];
4762:     TailR5[TailStart] = g_PpcContext.Gpr[5];
4763:     TailR15[TailStart] = g_PpcContext.Gpr[15];
4764:     TailR16[TailStart] = g_PpcContext.Gpr[16];
4765:     TailCr[TailStart] = g_PpcContext.Cr;
4766:         TailStart = (TailStart + 1) % 4096;
4767:         if (TailCount < 4096) TailCount++;
4768:         if (TailProbed == 0 && (Current == 0x40B6CA68 || Current == 0x40B6CA78 || Current == 0x40B6CA84 || Current == 0x40B6CA88)) {
4769:             TailProbed = 1;
4770:             Print(L"  MOVE-SR-TAIL PC=0x%08x r3=0x%08x r24=0x%08x r27=0x%08x r25=0x%08x r28=0x%08x r31=0x%08x CR=0x%08x CR0=%x CR2=%x CR5=%x CR7=%x\n",
4771:                   Current, g_PpcContext.Gpr[3], g_PpcContext.Gpr[24], g_PpcContext.Gpr[27],
4772:                   g_PpcContext.Gpr[25], g_PpcContext.Gpr[28], g_PpcContext.Gpr[31],
4773:                   g_PpcContext.Cr, (g_PpcContext.Cr >> 28) & 0xF, (g_PpcContext.Cr >> 20) & 0xF,
4774:                   (g_PpcContext.Cr >> 8) & 0xF, g_PpcContext.Cr & 0xF);
4775:             Print(L"  ED dump: [0x20]=0x%08x [0x3E]=0x%08x [0xB072C]=0x%08x [0xB80C]=0x%08x [0xB818]=0x%08x [0xB074]=0x%08x [0xB078]=0x%08x [0xB814]=0x%08x [0xB2CC]=0x%08x\n",
4776:                   CpuRead32(0x20), CpuRead32(0x3E), CpuRead32(0xB72C), CpuRead32(0xB80C),
4777:                   CpuRead32(0xB818), CpuRead32(0xB074), CpuRead32(0xB078), CpuRead32(0xB814),
4778:                   CpuRead32(0xB2CC));
4779:         }
4780:         if ((Executed % 250000) == 0) {
4781:             Print(L"  PROGRESS[%d] PC=0x%08x LR=0x%08x r1=0x%08x r8=0x%08x r28=0x%08x SPRG4=0x%08x "
4782:                   L"MSR=0x%08x DEC=0x%08x TBL=0x%08x NEG=%u\n",
4783:                   Executed, Current, g_PpcContext.Lr, g_PpcContext.Gpr[1],
4784:                   g_PpcContext.Gpr[8], g_PpcContext.Gpr[28], g_PpcContext.Spr[272],
4785:                   g_PpcContext.Msr, g_PpcContext.Spr[22], g_PpcContext.TimeBaseL,
4786:                   g_PpcContext.DecrementerNegative);
4787:         }
4788:         if (SchedProbes < 4 && Current == 0x40B22F18) {
4789:             UINT32 K = g_PpcContext.Gpr[1];
4790:             UINT32 Ecc = CpuRead32(K + 0x658);
4791:             UINT32 Cur = CpuRead32(K - 0x254);
4792:             SchedProbes++;
4793:             Print(L"  SCHED[%u] PC=0x%08x r8=0x%08x r9=0x%08x TBL=0x%08x DEC=0x%08x NEG=%u\n",
4794:                   SchedProbes, Current, g_PpcContext.Gpr[8], g_PpcContext.Gpr[9],
4795:                   g_PpcContext.TimeBaseL, g_PpcContext.Spr[22],
4796:                   g_PpcContext.DecrementerNegative);
4797:             Print(L"  SCHED   KDP-0x309(flg)=%u KDP-0x2E8/4(dead)=0x%08x:0x%08x "
4798:                   L"curTask=0x%08x st16=0x%02x\n",
4799:                   PpcReadGuestByte(K - 0x309), CpuRead32(K - 0x2E8), CpuRead32(K - 0x2E4),
4800:                   Cur, (Cur ? PpcReadGuestByte(Cur + 0x16) : 0));
4801:             Print(L"  SCHED   ECB=0x%08x ECB+CC=0x%08x KDP+5A0=0x%08x KDP+F2C=0x%08x "
4802:                   L"KDP+E8C=0x%08x\n",
4803:                   Ecc, CpuRead32(Ecc + 0xCC), CpuRead32(K + 0x5A0),
4804:                   CpuRead32(K + 0xF2C), CpuRead32(K + 0xE8C));
4805:         }
4806:         if (PcsDumped == 0 && (Current == 0x40B2751C || Current == 0x40B27530 || Current == 0x40B27540)) {
4807:             UINT32 Ewa = g_PpcContext.Spr[272];
4808:             UINT32 Kdp = CpuRead32(Ewa - 4);
4809:             PcsDumped = 1;
4810:             Print(L"  PANICDUMP EWA=0x%08x KDP=0x%08x [EWA-4]=0x%08x\n", Ewa, Kdp, CpuRead32(Ewa - 4));
4811:             Print(L"  PANICDUMP saved r0-r11: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
4812:                   CpuRead32(Kdp+0x700), CpuRead32(Kdp+0x704), CpuRead32(Kdp+0x708),
4813:                   CpuRead32(Kdp+0x70c), CpuRead32(Kdp+0x710), CpuRead32(Kdp+0x714),
4814:                   CpuRead32(Kdp+0x718), CpuRead32(Kdp+0x71c), CpuRead32(Kdp+0x720),
4815:                   CpuRead32(Kdp+0x724), CpuRead32(Kdp+0x728), CpuRead32(Kdp+0x72c));
4816:             Print(L"  PANICDUMP saved r12-r23: %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
4817:                   CpuRead32(Kdp+0x730), CpuRead32(Kdp+0x734), CpuRead32(Kdp+0x738),
4818:                   CpuRead32(Kdp+0x73c), CpuRead32(Kdp+0x740), CpuRead32(Kdp+0x744),
4819:                   CpuRead32(Kdp+0x748), CpuRead32(Kdp+0x74c), CpuRead32(Kdp+0x750),
4820:                   CpuRead32(Kdp+0x754), CpuRead32(Kdp+0x758), CpuRead32(Kdp+0x75c));
4821:             Print(L"  PANICDUMP saved r24-r31: %08x %08x %08x %08x %08x %08x %08x %08x\n",
4822:                   CpuRead32(Kdp+0x760), CpuRead32(Kdp+0x764), CpuRead32(Kdp+0x768),
4823:                   CpuRead32(Kdp+0x76c), CpuRead32(Kdp+0x770), CpuRead32(Kdp+0x774),
4824:                   CpuRead32(Kdp+0x778), CpuRead32(Kdp+0x77c));
4825:             Print(L"  PANICDUMP CR=0x%08x XER=0x%08x CTR=0x%08x LR=0x%08x PVR=0x%08x DSISR=0x%08x DAR=0x%08x\n",
4826:                   CpuRead32(Kdp+0x780), CpuRead32(Kdp+0x788), CpuRead32(Kdp+0x790),
4827:                   CpuRead32(Kdp+0x78c), CpuRead32(Kdp+0x794), CpuRead32(Kdp+0x798),
4828:                   CpuRead32(Kdp+0x79c));
4829:             Print(L"  PANICDUMP TBU=0x%08x TBL=0x%08x DEC=0x%08x SDR1=0x%08x SRR0=0x%08x SRR1=0x%08x MSR=0x%08x\n",
4830:                   CpuRead32(Kdp+0x7a0), CpuRead32(Kdp+0x7a4), CpuRead32(Kdp+0x7a8),
4831:                   CpuRead32(Kdp+0x7b0), CpuRead32(Kdp+0x7b4), CpuRead32(Kdp+0x7b8),
4832:                   CpuRead32(Kdp+0x7bc));
4833:             Print(L"  PANICDUMP TerminationCaller[KDP+0x904]=0x%08x [KDP+0x900]=0x%08x [KDP+0x908]=0x%08x\n",
4834:                   CpuRead32(Kdp+0x904), CpuRead32(Kdp+0x900), CpuRead32(Kdp+0x908));
4835:             Print(L"  PANICDUMP NoIdeaR23[KDP-0x900]=0x%08x OldKDP[KDP+0x5a0]=0x%08x [KDP+0x5a4]=0x%08x [KDP+0x648]=0x%08x [KDP+0x64c]=0x%08x\n",
4836:                   CpuRead32(Kdp-0x900), CpuRead32(Kdp+0x5a0), CpuRead32(Kdp+0x5a4),
4837:                   CpuRead32(Kdp+0x648), CpuRead32(Kdp+0x64c));
4838:             Print(L"  PANICDUMP pool FreePool[KDP-0xAB0]=0x%08x FirstSeg[KDP-0xAA0]=0x%08x FirstSegLogi[KDP-0xA9C]=0x%08x\n",
4839:                   CpuRead32(Kdp-0xAB0), CpuRead32(Kdp-0xAA0), CpuRead32(Kdp-0xA9C));
4840:             Print(L"  PANICPOOL FreePool LLL @0x9548:\n");
4841:             {
4842:                 UINT32 A;
4843:                 for (A = 0x9548; A < 0x9568; A += 16) {
4844:                     Print(L"    0x%08x: %08x %08x %08x %08x\n",
4845:                           A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
4846:                 }
4847:             }
4848:             Print(L"  PANICPOOL first segment begin @0x2FE0:\n");
4849:             {
4850:                 UINT32 A;
4851:                 for (A = 0x2FE0; A < 0x3050; A += 16) {
4852:                     Print(L"    0x%08x: %08x %08x %08x %08x\n",
4853:                           A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
4854:                 }
4855:             }
4856:             Print(L"  PANICPOOL first segment end @0x9FC0..0xA010:\n");
4857:             {
4858:                 UINT32 A;
4859:                 for (A = 0x9FC0; A < 0xA010; A += 16) {
4860:                     Print(L"    0x%08x: %08x %08x %08x %08x\n",
4861:                           A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
4862:                 }
4863:             }
4864:             Print(L"  PANICPOOL cgrp block @0x8C40..0x8CB8:\n");
4865:             {
4866:                 UINT32 A;
4867:                 for (A = 0x8C40; A < 0x8CB8; A += 16) {
4868:                     Print(L"    0x%08x: %08x %08x %08x %08x\n",
4869:                           A, CpuRead32(A), CpuRead32(A + 4), CpuRead32(A + 8), CpuRead32(A + 0xC));
4870:                 }
4871:             }
4872:             Print(L"  PANICDUMP mem@0x8C00..0x8D00:\n");
4873:             {
4874:                 UINT32 T;
4875:                 for (T = 0x8C00; T < 0x8D00; T += 16) {
4876:                     Print(L"    0x%08x: %08x %08x %08x %08x\n",
4877:                           T, CpuRead32(T), CpuRead32(T + 4), CpuRead32(T + 8), CpuRead32(T + 0xC));
4878:                 }
4879:             }
4880:             Print(L"  PANICROM (NKCreateAddressSpaceSub region):\n");
4881:             {
4882:                 UINT32 A;
4883:                 for (A = 0x40B1F000; A < 0x40B1FC00; A += 16) {
4884:                     Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
4885:                           A, CpuRead32(A), CpuRead32(A + 4),
4886:                           CpuRead32(A + 8), CpuRead32(A + 12));
4887:                 }
4888:             }
4889:             Print(L"  PANICROM (InitPool region 0x40B10F00):\n");
4890:             {
4891:                 UINT32 A;
4892:                 for (A = 0x40B10F00; A < 0x40B11200; A += 16) {
4893:                     Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
4894:                           A, CpuRead32(A), CpuRead32(A + 4),
4895:                           CpuRead32(A + 8), CpuRead32(A + 12));
4896:                 }
4897:             }
4898:             Print(L"  PANICROM (PoolAllocClear/InitPool region 0x40B22600):\n");
4899:             {
4900:                 UINT32 A;
4901:                 for (A = 0x40B22600; A < 0x40B22A00; A += 16) {
4902:                     Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
4903:                           A, CpuRead32(A), CpuRead32(A + 4),
4904:                           CpuRead32(A + 8), CpuRead32(A + 12));
4905:                 }
4906:             }
4907:             Print(L"  PANICROM (system-AS creation 0x40B11B00):\n");
4908:             {
4909:                 UINT32 A;
4910:                 for (A = 0x40B11B00; A < 0x40B11E60; A += 16) {
4911:                     Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
4912:                           A, CpuRead32(A), CpuRead32(A + 4),
4913:                           CpuRead32(A + 8), CpuRead32(A + 12));
4914:                 }
4915:             }
4916:             Print(L"  PANICDUMP live r1=0x%08x r8=0x%08x r28=0x%08x r29=0x%08x r30=0x%08x r31=0x%08x LR=0x%08x\n",
4917:                   g_PpcContext.Gpr[1], g_PpcContext.Gpr[8], g_PpcContext.Gpr[28],
4918:                   g_PpcContext.Gpr[29], g_PpcContext.Gpr[30], g_PpcContext.Gpr[31],
4919:                   g_PpcContext.Lr);
4920:             Print(L"  PANICROM (message + dead-loop region 0x40B10600):\n");
4921:             {
4922:                 UINT32 A;
4923:                 for (A = 0x40B10600; A < 0x40B10900; A += 16) {
4924:                     Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
4925:                           A, CpuRead32(A), CpuRead32(A + 4),
4926:                           CpuRead32(A + 8), CpuRead32(A + 12));
4927:                 }
4928:             }
4929:             Print(L"  PANICROM (panic handler region 0x40B26300):\n");
4930:             {
4931:                 UINT32 A;
4932:                 for (A = 0x40B26300; A < 0x40B27600; A += 16) {
4933:                     Print(L"  ROM[0x%08x] %08x %08x %08x %08x\n",
4934:                           A, CpuRead32(A), CpuRead32(A + 4),
4935:                           CpuRead32(A + 8), CpuRead32(A + 12));
4936:                 }
4937:             }
4938:         }
4939:         // Log EVERY nanodebugger (Termination) entry with its caller so we can
4940:         // see each fatal check the guest hits as boot progresses. r29 is loaded
4941:         // from LR (the caller's return address) at 0x40B272F8; [KDP+0x904] holds
4942:         // the same value once stored. The 'g' handler's optional context
4943:         // re-save (0x40B27A90 -> Termination) shows up here as caller 0x40B27A94.
4944:         if (TermEntries < 60 && Current == 0x40B272F8) {
4945:             UINT32 Ewa = g_PpcContext.Spr[272];
4946:             UINT32 Kdp = CpuRead32(Ewa - 4);
4947:             UINT32 Caller = g_PpcContext.Gpr[29];
4948:             TermEntries++;
4949:             Print(L"  TERMENTRY[%d] PC=0x%08x caller=0x%08x%s r1=0x%08x r8=0x%08x r9=0x%08x r31=0x%08x KDP=0x%08x EWA=0x%08x\n",
4950:                   TermEntries, Current, Caller,
4951:                   (Caller == 0x40B27A94) ? L" (g re-save)" : L"",
4952:                   g_PpcContext.Gpr[1], g_PpcContext.Gpr[8], g_PpcContext.Gpr[9],
4953:                   g_PpcContext.Gpr[31], Kdp, Ewa);
4954:         }
4955:         // Auto-answer the nanodebugger wait loop: when the guest is spinning
4956:         // (PC=0x40B2751C) with an empty SCC Rx FIFO, queue the same
4957:         // 'g' CR 'g' CR sequence the host pre-queues for the first entry so the
4958:         // boot continues past each subsequent fatal check. Cap it so a
4959:         // pathological re-panic loop cannot flood the log forever.
4960:         if (AutoResumed < 25 && Current == 0x40B2751C &&
4961:             g_SccRxFifoHead == g_SccRxFifoTail) {
4962:             AutoResumed++;
4963:             Print(L"  AUTORESUME[%d] queued 'g' CR 'g' CR at PC=0x%08x r1=0x%08x LR=0x%08x\n",
4964:                   AutoResumed, Current, g_PpcContext.Gpr[1], g_PpcContext.Lr);
4965:             PpcSccPutChar('g');
4966:             PpcSccPutChar(0x0D);
4967:             PpcSccPutChar('g');
4968:             PpcSccPutChar(0x0D);
4969:         }
4970:         if (TraceDumped == 0 && (Current == 0x40B272E0 || Current == 0x40B272E8 || Current == 0x40B272EC)) {
4971:             UINTN I;
4972:             UINTN N = (TailCount < 1500) ? TailCount : 1500;
4973:             CHAR16 Mn[16];
4974:             TraceDumped = 1;
4975:             Print(L"--- last %d instructions before panic entry ---\n", N);
4976:             for (I = 0; I < N; I++) {
4977:                 UINTN Idx = (TailStart + TailCount - 1 - I) % 4096;
4978:                 PpcDecodeInstruction(TailInst[Idx], Mn, sizeof(Mn));
4979:                 Print(L"  PRE[-%d] PC=0x%08x 0x%08x %s -> 0x%08x r28=0x%08x r8=0x%08x r17=0x%08x\n",
4980:                       (UINTN)I + 1, TailPc[Idx], TailInst[Idx], Mn, TailNext[Idx],
4981:                       TailR28[Idx], TailR8[Idx], TailR17[Idx]);
4982:             }
4983:             Print(L"  PRE[0] PC=0x%08x LR=0x%08x r1=0x%08x r8=0x%08x r9=0x%08x r17=0x%08x r28=0x%08x\n",
4984:                   Current, g_PpcContext.Lr, g_PpcContext.Gpr[1], g_PpcContext.Gpr[8],
4985:                   g_PpcContext.Gpr[9], g_PpcContext.Gpr[17], g_PpcContext.Gpr[28]);
4986:         }
4987:         if (Executed <= 200) {
4988:             CHAR16 Mn[16];
4989:             PpcDecodeInstruction(Instr, Mn, sizeof(Mn));
4990:             Print(L"  TRACE[%d] PC=0x%08x 0x%08x %s -> next 0x%08x\n",
4991:                   Executed, Current, Instr, Mn, Next);
4992:         }
4993: 
4994:         if (EFI_ERROR(Status)) {
4995:             if (LogUnsupported) {
4996:                 UINTN I;
4997:                 CHAR16 Mn[16];
4998:                 Print(L"--- last %d instructions before stop ---\n", TailCount);
4999:                 for (I = 0; I < TailCount; I++) {

(Showing lines 4700-4999 of 5348. Use offset=5000 to continue.)
</content>
```

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\ui\\ui_impl.c",
  "limit": 100
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\ui\ui_impl.c</path>
<type>file</type>
<content>
1: /* EFI-Mac setup front-end.
2:  *
3:  * Configuration persistence (a single NVRAM variable), the boot gate
4:  * (5-second countdown with the classic Macintosh face, F8 opens setup)
5:  * and the interactive configuration menu. All output goes through the
6:  * EFI text console, so it renders on the GOP display and on the serial
7:  * console alike.
8:  */
9: 
10: #include <efi.h>
11: #include <efilib.h>
12: 
13: #include "ui/ui.h"
14: #include "hardware/abstraction.h"
15: #include "boot/bootloader.h"
16: #include "platform/uefi_interface.h"
17: 
18: // NVRAM variable that stores the saved configuration.
19: STATIC EFI_GUID g_UiConfigGuid = PPC_CONFIG_VARIABLE_GUID;
20: 
21: // The UEFI scan code for Enter (gnu-efi's eficon.h stops at SCAN_ESC).
22: #define UI_SCAN_ENTER   0x000D
23: 
24: // ASCII "Macintosh" face shown during the boot countdown: the classic
25: // compact-Mac silhouette with a smiling screen and a floppy drive slot.
26: STATIC CONST CHAR16* UiMacFace[] = {
27:     L"              .----------------------------------.",
28:     L"             /                                    \\",
29:     L"            |   .----------------------------.     |",
30:     L"            |  (                              )    |",
31:     L"            |   |    .------------------.    |     |",
32:     L"            |   |   |    (o)    (o)     |    |     |",
33:     L"            |   |   |       ----        |    |     |",
34:     L"            |   |   |     \\_____/       |    |     |",
35:     L"            |   |    '------------------'    |     |",
36:     L"            |   |    .------------------.    |     |",
37:     L"            |   |   |  |  |  |  |  |   |     |     |",
38:     L"            |   |    '------------------'    |     |",
39:     L"            |  (                              )    |",
40:     L"            |   '----------------------------'     |",
41:     L"             \\                                    /",
42:     L"              '----------------------------------'"
43: };
44: #define UI_MAC_FACE_LINES  (sizeof(UiMacFace) / sizeof(UiMacFace[0]))
45: 
46: // ---------------------------- console helpers ----------------------------
47: 
48: STATIC
49: VOID
50: UiSetAttr (
51:     IN UINTN Attr
52:     )
53: {
54:     if (ST != NULL && ST->ConOut != NULL) {
55:         ST->ConOut->SetAttribute(ST->ConOut, Attr);
56:     }
57: }
58: 
59: STATIC
60: VOID
61: UiConsoleSize (
62:     OUT UINTN* Columns,
63:     OUT UINTN* Rows
64:     )
65: {
66:     UINTN C = 80;
67:     UINTN R = 25;
68:     if (ST != NULL && ST->ConOut != NULL && ST->ConOut->Mode != NULL) {
69:         ST->ConOut->QueryMode(ST->ConOut, ST->ConOut->Mode->Mode, &C, &R);
70:     }
71:     if (Columns != NULL) {
72:         *Columns = C;
73:     }
74:     if (Rows != NULL) {
75:         *Rows = R;
76:     }
77: }
78: 
79: STATIC
80: VOID
81: UiDrawCentered (
82:     IN UINTN         Row,
83:     IN CONST CHAR16* Text
84:     )
85: {
86:     UINTN Columns = 80;
87:     UINTN Rows    = 25;
88:     UiConsoleSize(&Columns, &Rows);
89:     UINTN Len = StrLen(Text);
90:     UINTN Col = (Len >= Columns) ? 0 : ((Columns - Len) / 2);
91:     if (Row < Rows) {
92:         PrintAt(Col, Row, L"%s", Text);
93:     }
94: }
95: 
96: STATIC
97: VOID
98: UiDrawCenteredPrint (
99:     IN UINTN         Row,
100:     IN CONST CHAR16* Fmt,

(Showing lines 1-100 of 794. Use offset=101 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 6.5s)

_Thinking:_

Now I have a comprehensive understanding of the project. Let me also check if there's a build that works to verify:

Now I have a comprehensive understanding of the project. Let me verify the build works, then create the plan:

**Tool: bash**

**Input:**
```json
{
  "command": "ls build/ 2>/dev/null || echo \"No build directory\"",
  "workdir": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac"
}
```

**Output:**
```
[31;1mOut-File: [31;1mCould not find a part of the path 'C:\dev\null'.[0m
No build directory

```

---

## Assistant (Build · Big Pickle · 11.4s)

_Thinking:_

The build directory doesn't exist yet. Let me check the build instructions more carefully to understand how to build:

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
5: **The project builds.** `make` (or `scripts/build-windows.sh`) produces a valid
6: PE32+ UEFI application image (`build/EFI-Mac-Emulator.efi`). The application is
7: a heavy bootloader for classic Mac OS: it initializes the UEFI environment, runs
8: a 35-check PowerPC CPU self-test (including the FPU core), sets up the guest
9: memory map (RAM, low-memory globals, system ROM, staging areas), executes a
10: PowerPC program from guest RAM, initializes graphics/audio/storage/network, runs
11: the boot memory-map self-test (7/7 with the demo ROM, 5/5 with a real ROM) and
12: the System Folder / driver staging self-test (7/7), installs a real New World
13: ROM from a Mac OS 8.5+/9 disc when attached, then reports ready. The full boot
14: sequence has been verified under QEMU + OVMF on a Windows host (CPU 35/35, boot
15: 5/5 with the 9.2.2 disc, system files 7/7).
16: 
17: ## Overview
18: 
19: The build cross-compiles from macOS (Apple Silicon or Intel) to an x86_64 UEFI
20: application:
21: 
22: 1. **GNU-EFI** provides the UEFI headers (`efi.h`, `efilib.h`, ...) and a small
23:    runtime library (memory, string, print, pool helpers).
24: 2. **clang** compiles the sources targeting `x86_64-pc-win32-coff` (PE/COFF
25:    object files, Microsoft x64 ABI).
26: 3. **lld-link** links the objects into a PE32+ image with
27:    `Subsystem = EFI_APPLICATION`, entry point `efi_main`.
28: 
29: This is the same approach EDK II's `CLANGPDB` toolchain uses. The classic
30: GNU-EFI `objcopy`/`ld` flow is not used because the host GNU binutils cannot
31: emit the `efi-app-x86_64` BFD target on this setup.
32: 
33: ## Prerequisites
34: 
35: - **macOS** with Homebrew
36: - `brew install llvm lld`
37: - `git` (only needed for the first build, which clones GNU-EFI)
38: 
39: `binutils` is optional (only the `make check` target uses it via
40: `llvm-objdump`, which actually comes with `llvm`).
41: 
42: ### Windows
43: 
44: The same clang/lld-link flow also builds and boots on Windows (chocolatey):
45: 
46: ```powershell
47: choco install llvm qemu
48: ```
49: 
50: - **llvm** (22.1.x, installed to `C:\Program Files\LLVM\bin`)
51: - **qemu** (installed to `C:\Program Files\qemu`)
52: - **git-bash** (installed to `C:\Program Files\Git`) to run `make`
53: 
54: Build and boot scripts are in `scripts/` (see "Building on Windows").
55: 
56: ## Building
57: 
58: ```bash
59: make
60: ```
61: 
62: On the first run the Makefile clones GNU-EFI into `third_party/gnu-efi/` (it is
63: git-ignored and fetched on demand). Output:
64: 
65: - `build/EFI-Mac-Emulator.efi` — the UEFI application
66: 
67: Verify the image:
68: 
69: ```bash
70: make check
71: ```
72: 
73: `make clean` removes `build/`. Toolchain locations can be overridden on the
74: command line if Homebrew is not at the default prefix:
75: 
76: ```bash
77: make CC=/path/to/clang LLD=/path/to/lld-link
78: ```
79: 
80: ## How the build works
81: 
82: 1. **GNU-EFI runtime library** — the sources under
83:    `third_party/gnu-efi/lib/` are compiled to COFF objects (see
84:    `GNUEFI_SRCS` in the Makefile). `entry.c` and the `.S` startup files are
85:    excluded because they target the ELF/objcopy flow we do not use.
86: 2. **Application sources** — every `src/*/*_impl.c` and `src/main.c` is
87:    compiled with `-Wall -Werror` against GNU-EFI's headers.
88: 3. **Link** — `lld-link /subsystem:EFI_APPLICATION /entry:efi_main /dll`.
89:    The firmware performs the image relocations at load time via the `.reloc`
90:    section, so no crt0 is required. `_fltused` (MS ABI CRT symbol, referenced
91:    by GNU-EFI's print code) is provided in `src/main.c`.
92: 
93: ## Layout notes
94: 
95: The sources use GNU-EFI idioms:
96: 
97: - Includes: `<efi.h>` + `<efilib.h>` (not EDK II's `<Library/...>`).
98: - Entry point: `efi_main(EFI_HANDLE, EFI_SYSTEM_TABLE*)` calling
99:   `InitializeLib()` (not `UefiMain`).
100: - Boot/Runtime tables: `BS`, `ST`, `RT` globals (not `gBS`, `gST`, `gRT`).
101: 
102: ## Testing
103: 
104: The EFI application has been smoke-tested under QEMU + OVMF: it boots, runs
105: all of its initialization (UEFI pool allocation, memory map, loaded-image
106: protocol, console output), prints its status report, and returns cleanly to the
107: firmware.
108: 
109: To reproduce:
110: 
111: ```bash
112: # 1. Get OVMF firmware. There is no Homebrew formula; download the Debian package:
113: mkdir -p /tmp/ovmf && cd /tmp/ovmf
114: curl -sL -o ovmf.deb \
115:   "http://ftp.us.debian.org/debian/pool/main/e/edk2/ovmf_2025.02-8+deb13u1_all.deb"
116: ar x ovmf.deb && tar -xf data.tar.xz ./usr/share/ovmf/OVMF.fd
117: 
118: # 2. Put the app on a FAT "ESP" directory and boot it
119: mkdir -p esp/EFI/BOOT
120: cp build/EFI-Mac-Emulator.efi esp/EFI/BOOT/BOOTX64.EFI
121: qemu-system-x86_64 \
122:   -bios /tmp/ovmf/usr/share/ovmf/OVMF.fd \
123:   -m 512 \
124:   -drive file=fat:rw:esp,format=raw \
125:   -net none \
126:   -serial stdio \
127:   -display none
128: ```
129: 
130: OVMF will boot `\EFI\BOOT\BOOTX64.EFI` and the app's output appears on the
131: serial console (`-serial stdio`). The newest Debian OVMF version can be looked
132: up via the Debian packages site (`https://packages.debian.org/trixie/all/ovmf/download`).
133: 
134: ### Windows (chocolatey LLVM + QEMU)
135: 
136: ```powershell
137: # 1. Build (git-bash), then copy the image and unpack OVMF once:
138: bash scripts/build-windows.sh
139: 
140: # 2. OVMF: download the Debian ovmf package and extract OVMF_CODE_4M.fd /
141: #    OVMF_VARS_4M.fd. Use Windows tar, not git-bash tar, for the .deb/.tar.xz:
142: #    (paths below match scripts/run-qemu-windows.ps1 defaults)
143: #    C:\Users\...\AppData\Local\Temp\opencode\ovmf\usr\share\OVMF\OVMF_CODE_4M.fd
144: 
145: # 3. Boot under QEMU (PowerShell); captures serial output to boot_out.txt:
146: powershell -ExecutionPolicy Bypass -File scripts/run-qemu-windows.ps1
147: 
148: # 4. Check the self-test results:
149: Select-String -Path "$env:TEMP\opencode\boot_out.txt" -Pattern "self-test complete"
150: ```
151: 
152: Expect CPU self-test 35/35 and the system-files self-test 7/7; the boot
153: self-test is 7/7 with no Mac disc (demo ROM) or 5/5 with a Mac OS 8.5+/9 disc
154: attached (real New World ROM), then a clean handoff to the OVMF UI.
155: 
156: ## Directory Structure
157: 
158: ```
159: EFIMac/
160: ├── src/                    # Source code files
161: │   ├── main.c              # Main entry point (efi_main)
162: │   ├── cpu/                # CPU translation components
163: │   ├── memory/             # Memory management
164: │   ├── hardware/           # Hardware abstraction
165: │   ├── boot/               # Bootloader system
166: │   ├── utils/              # Utility functions
167: │   └── platform/           # UEFI interface
168: ├── third_party/gnu-efi/    # GNU-EFI headers + runtime (git-ignored, auto-cloned)
169: ├── Makefile                # Build configuration
170: ├── README.md               # Project overview
171: ├── ARCHITECTURE.md         # Design notes
172: ├── TODO.md                 # Implementation plan
173: └── USER_GUIDE.md           # User documentation
174: ```
175: 
176: ## Troubleshooting
177: 
178: 1. **`make` fails to clone GNU-EFI**: `git.code.sf.net` may be unreachable;
179:    clone it manually into `third_party/gnu-efi`:
180:    ```bash
181:    git clone --depth 1 https://git.code.sf.net/p/gnu-efi/code third_party/gnu-efi
182:    ```
183: 2. **`clang: error: no such file .../efi.h`**: run `make` from the repo root, or
184:    check that `third_party/gnu-efi/inc/efi.h` exists.
185: 3. **Undefined `_fltused`**: this MS ABI CRT symbol must be defined somewhere in
186:    the link; `src/main.c` already provides it — do not remove it.
187: 4. **Image won't load in firmware**: confirm `make check` reports
188:    `Subsystem = EFI application` and a `Base Relocation Directory`; rename the
189:    file to `BOOTX64.EFI` on a FAT partition.
190: 
191: ## License
192: 
193: This project is licensed under the GNU General Public License, version 3 or
194: later. See the [LICENSE](LICENSE) file for details.

(End of file - total 194 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 136.4s)

_Thinking:_

Now I have a thorough understanding of the project. Let me formulate a comprehensive plan based on my analysis.

## Summary of Current State

**What exists:**
1. A UEFI application that boots on x86_64 UEFI systems
2. PowerPC interpreter (full, including FPU and AltiVec)
3. 68K interpreter (native C implementation with opcode table patching)
4. In-emulator HFS reader that can read classic Mac discs
5. System Folder and driver staging (System, Finder, Extensions, Mac OS ROM)
6. Real New World ROM discovery and installation from Mac OS 9.2.2 disc
7. ROM patching (PpcPatchNewWorldRom) - configures the ROM's built-in 68K DR emulator
8. Memory management (guest RAM, low-memory globals, ROM window)
9. Hardware abstraction (GOP framebuffer, Block I/O, SNP, audio ring buffer)
10. Boot gate with ASCII Mac face, F8 to enter config menu
11. Config persistence in NVRAM**What's working:**
- Self-tests pass (PPC 35/35, boot memory map, system files, graphics)
- ROM is installed and configured with all the SheepShaver-style patches
- The nanokernel boot sequence executes and reaches the 68K handoff point
- The 68K DR emulator entry point is patched and the interpreter hooks it at 0x40B67C60
- Several68K opcodes are hooked (MOVE SR, RESET, 4E7B, MOVEQ, native 68K dispatch)

**What's NOT working (the core problem):**
The boot sequence crashes/fails during the nanokernel → 68K transition. The key issues visible from the code:

1. **The nanokernel panics** - The PANICDUMP output shows the NK terminates with error checks. The auto-resume loop feeds 'g' to skip past nanodebugger entries, but the boot never progresses past the 68K emulator initialization.

2. **The 68K interpreter is being invoked** but:
   - Only a few opcodes are hooked (MOVE SR, RESET, 4E7B, MOVEQ)
   - The main opcode dispatch at 0x40B67C60 is intercepted and calls M68kExecuteFromPPC()
   - But the boot tail calls the 68K emulator which is supposed to:
     - Set up the 68K CPU context
     - Begin executing 68K code from the 68K reset vector (ROM + 0x2A)
     - Handle Mac Toolbox traps

3. **Missing components for boot:**
   - No continuous execution loop that stays in 68K mode - The EMUL_OP handlers (PPC_OP_*) are defined but not implemented
   - No timer/interrupt delivery mechanism
   - No VIA (Versatile Interface Adapter) emulation
   - The SCC (serial) driver poll loop hangs because r28 = 0 (SCC base not loaded)

**SheepShaver's approach for reference:**
SheepShaver is fundamentally different from what EFIMac is trying to do:
- SheepShaver runs on a PPC host (or emulates PPC) and patches the 68K ROM to redirect ALL I/O through EMUL_OP traps
- It patches A-line trap vectors to EMUL_OP instructions
- Every driver call is intercepted and handled in host-side C++ code
- There's no hardware emulation at all - it's pure paravirtualization

**The fundamental challenge:**
The current approach tries to:
1. Run the PPC nanokernel (which initializes the Mac OS kernel infrastructure)
2. The nanokernel then hands off to the 68K DR emulator
3. The 68K DR emulator executes the Mac OS ROM's 68K code
4. The 68K code expects real hardware (VIA, SCSI, etc.)

The nanokernel itself is failing during initialization because:
- The PMDT (Page Map Descriptor Table) setup fails (visible in the PANICDUMP)
- The area creation/merge checks fail
- The SCC poll hangs because the SCC base address isn't properly seeded

**What needs to happen to reach the desktop:**

Looking at this more carefully, I think the approach needs to be reconsidered. The SheepShaver approach is fundamentally different from trying to run the nanokernel. SheepShaver completely bypasses the nanokernel and directly patches the 68K ROM.The most practical path forward for EFIMac would be:

### Option A: Fix the nanokernel path (current approach)
- Fix the PMDT walk by properly seeding the page tables
- Fix the SCC base address so the NK boot printer works
- Fix the area creation/merge issues
- Then fix the 68K interpreter to handle all opcodes the ROM needs
- Then add EMUL_OP handlers for the hardware abstraction

### Option B: SheepShaver-style approach- Skip the nanokernel entirely
- Patch the 68K ROM's trap vectors directly (A-line traps → EMUL_OP)
- Run68K code directly through the native interpreter
- Implement all EMUL_OP handlers for device emulation- This is what SheepShaver does and it worksOption B is more reliable and is what the project should converge on, because:
- The nanokernel path requires getting ALL the hardware initialization right (MMU, BAT, page tables, etc.)
- SheepShaver proved that you can boot Mac OS by patching just the 68K trap table
- The project already has the 68K interpreter and the EMUL_OP framework

However, looking at the code more carefully, the project IS already pursuing a hybrid approach:
- The ROM is patched with PpcPatchNewWorldRom (SheepShaver-style)
- The 68K opcode table is patched
- The EMUL_OP markers are installed
- The native 68K interpreter hooks the main dispatch at 0x40B67C60

The issue is that the nanokernel boot sequence is failing before it can reach the 68K code execution properly. The nanokernel needs to:
1. Initialize memory management2. Create the kernel address space
3. Initialize the scheduler
4. Start the first task (the Mac OS Finder shell)

And this is where it's crashing.

Let me think about what specific objectives need to be completed to reach the desktop...

## Objectives to reach boot-to-desktop

### Phase A: Fix the nanokernel boot (get past the current crash)
1. Fix the SCC base address seeding (r28 = 0 causes infinite poll)
2. Fix the PMDT walk (properly seed page tables for the memory regions)
3. Fix area creation/merge (ensure the NK area manager doesn't fail)
4. Get the NK to successfully initialize and hand off to the 68K emulator

### Phase B: Fix the 68K interpreter integration1. Make M68kExecuteFromPPC() the main execution path (not just a single-step)
2. Implement all68K opcodes the ROM needs3. Implement the 68K exception/interrupt mechanism
4. Seed the 68K reset vector properly (PC = ROM + 0x2A, SSP from vector table)

### Phase C: EMUL_OP handlers (hardware abstraction)
1. Implement PPC_OP_ADBOP (keyboard/mouse)
2. Implement PPC_OP_DISK_OPEN/PRIME/CONTROL/STATUS (boot volume access)
3. Implement PPC_OP_AUDIO_DISPATCH (audio)
4. Implement PPC_OP_SCSI_DISPATCH (SCSI manager)
5. Implement PPC_OP_INSTIME/RMVTIME/PRIMETIME (timer)
6. Implement PPC_OP_MICROSECONDS
7. Implement PPC_OP_IRQ (interrupt delivery)
8. Implement PPC_OP_RESET
9. Implement PPC_OP_INSTALL_DRIVERS
10. Implement PPC_OP_NAME_REGISTRY
11. Implement PPC_OP_SONY_OPEN/PRIME/CONTROL/STATUS (floppy)
12. Implement PPC_OP_CDROM_OPEN/PRIME/CONTROL/STATUS

### Phase D: Mac device register emulation
1. VIA (Versatile Interface Adapter) - timers, interrupts, ADB
2. SCC (Serial Communications Controller) - for the NK boot printer
3. SCSI controller registers
4. Ethernet controller

### Phase E: System integration
1. Mac OS Toolbox trap dispatch (A-line traps → EMUL_OP)
2. Proper boot sequence: ROM → 68K boot code → System file → Finder → Desktop
3. Boot volume mounting4. Extension loading
5. Time Manager integration

### Phase F: Display and input
1. Framebuffer driver integration2. Cursor support
3. Keyboard/mouse input through ADB
4. Sound output

Actually, wait. Let me reconsider. The current code at the `0x40B67C60` hook IS the 68K execution path. When the PPC DR emulator enters its common dispatch, the hook calls M68kExecuteFromPPC() which reads the 68K opcode from guest memory and executes it. But the problem is that this only handles one instruction at a time and returns to the PPC loop.The fundamental issue is that the current approach relies on the PPC-based DR emulator to drive the main loop, and the native 68K interpreter is called per-instruction from within that PPC loop. But the PPC DR emulator itself has issues (the nanokernel crashes during initialization).

Let me re-read the code more carefully to understand the exact point of failure...

Looking at the probes in the code:
- EMUTRAP at PPC_EMULATOR_TRAP_TABLE - XLM is restored, DR context is seeded
- EMUSTART at 0x40B6F900 - the emulator-start routine is entered
- INJENTRY at 0x40B6F700 - the injected 68K DR-emulator entry
- Then the code enters the loop at 0x40B67C60 where M68kExecuteFromPPC is called

But then it hits PANICDUMP entries and TERMINAL entries. The nanokernel is panicking.

The PANICDUMP shows:
- The NK terminated after hitting an assertion check
- The area manager's merge/guard check at 0x40B1F67C is the crash point
- Pool management issues (FreePool, FirstSeg)

And the auto-resume mechanism feeds 'g' to the nanodebugger to continue past each panic, but this is a degenerate approach - the NK panics, we send 'g' to continue, it panics again, etc.

I think the real issue is that the nanokernel is failing because of the PMDT (Page Map Descriptor Table) setup. The DR=1 boot path (data relocation enabled) requires the NK to walk the PMDT to understand memory layout, and the PMDT isn't properly seeded.

The fix at `PmdFixed` tries to inject a proper PMDT but the fundamental problem may be that the memory layout doesn't match what the NK expects.

Actually, I think I need to step back and think about this differently.

SheepShaver's approach is:
1. Load the New World ROM
2. Patch the 68K trap table to redirect to EMUL_OP handlers
3. Run the ROM's boot code (which initializes the nanokernel)
4. The nanokernel initializes the 68K emulator
5. The 68K emulator runs the Toolbox code
6. Each Toolbox call is intercepted by EMUL_OP and handled in host code

The key insight is that SheepShaver doesn't try to run the nanokernel itself - it lets the ROM's nanokernel run on the PPC (or PPC emulator), and only intercepts the 68K calls.

In EFIMac's case, the PPC interpreter IS running the nanokernel code, and it gets to the point where the nanokernel tries to initialize the 68K emulator. That's where the EMULATOR_TRAP_TABLE entry is patched. But the NK crashes before reaching that point because of PMDT/memory setup issues.

Wait, actually looking again at the code flow:
1. The NK boots from ROM base + 0x310000
2. It initializes memory, creates areas, sets up the PMDT
3. It reaches the boot tail at 0x40B126F0 which calls blrl to the emulator4. The blrl lands at PPC_EMULATOR_TRAP_TABLE (patched to b 0x36F900 = emulator start)
5. The emulator-start routine at 0x40B6F900 enters the DR emulator
6. The DR emulator dispatch at 0x40B67C60 is hooked to call M68kExecuteFromPPC()

So the nanokernel IS successfully reaching the 68K handoff. The PANICDUMP entries are from the nanokernel's internal error handling, but the auto-resume mechanism pushes past them. The real question is: what happens AFTER the 68K execution starts?

Looking at the `TermEntries` counter - it goes up to 60 entries. Each time the NK panics, auto-resume sends 'g' to the nanodebugger. This means the NK is in a loop of: initialize → crash → nanodebugger → 'g' → reinitialize → crash → ...

The fundamental issue is that the NK's initialization is incomplete/broken. The 68K execution that starts at 0x40B67C60 is called from within the DR emulator loop, but the DR emulator is supposed to be running the Mac OS 68K boot code (which starts at the 68K reset vector). The NK hasn't finished setting up the environment for the 68K code to run successfully.

OK, let me think about what's the most pragmatic approach here.

I think the most important objective is:

**Make the 68K interpreter the primary execution engine, and bypass the NK's problematic initialization.**

Here's what I think the path forward is:

1. **Don't let the NK's broken initialization block the boot.** Instead of trying to fix every NK crash, take control of the boot flow after the ROM is installed.

2. **Start68K execution directly from the ROM's 68K reset vector.** The New World ROM has a 68K reset vector at ROM + 0x2A. We can set up the 68K CPU context (PC = ROM + 0x2A, SSP from the vector table, SR = 0x2700) and start executing 68K code directly through the native interpreter.

3. **Handle the Mac Toolbox traps through EMUL_OP handlers.** As the 68K code runs, it will hit Toolbox traps (A-line calls) which are patched to EMUL_OP entries. The interpreter handles these by calling the host-side EMUL_OP handlers.

4. **Implement the critical EMUL_OP handlers:**
   - PPcOp_IRQ for timer/interrupt delivery
   - PPC_OP_INSTALL_DRIVERS for extension loading
   - PPC_OP_DISK_* for boot volume
   - PPC_OP_ADBOP for input
   - PPC_OP_NAME_REGISTRY for device discovery

But wait, this is basically what the code is already trying to do! The problem is that the NK is supposed to set up the 68K emulator environment (KernelData, EmulatorData, opcode table, dispatch table, etc.) and if the NK crashes, these aren't set up properly.

However, looking at PpcPatchNewWorldRom, it already sets up everything the DR emulator needs:
- LA_InfoRecord, LA_KernelData, LA_EmulatorData
- LA_DispatchTable (opcode table)
- LA_EmulatorCode (emulator image)
- The trap table is patched with branches to emulator entry routines
- The EMUL_OP markers are installed
- XLM globals are written

And the emulator-start code at 0x40B6F900, the injected entry at 0x40B6F700, and the dispatch helper at 0x40B6F7C0 are all installed.

So the question is: does the NK's initialization actually complete enough for the 68K boot to start?

Looking at the trace more carefully, the PANICDUMP entries happen DURING the NK's initialization, and the auto-resume pushes past them. But the NK might be in a degenerate state after that.

OK, I think I've analyzed this enough. Let me create a plan based on what needs to be done. The key realization is:

The project has all the infrastructure in place but needs to:
1. Fix the NK boot path OR bypass it
2. Complete the 68K interpreter
3. Implement EMUL_OP handlers
4. Add Mac device register emulation

Given the complexity, the most practical approach is:

**A. Fix the nanokernel boot (current active approach):**
- The NK gets to the 68K handoff but crashes during PMDT/area setup
- The PMDT injection at PmdFixed and the area fixes need to be debugged/fixed
- The SCC base needs to be seeded- Once the NK successfully initializes, the 68K emulator will start

**B. Alternatively, bypass the NK entirely:**
- Start the 68K interpreter at the ROM's 68K reset vector directly
- Implement the full68K instruction set
- Implement all EMUL_OP handlers
- This is more work but avoids the NK initialization issues

I think the most practical approach is to continue with fixing the NK boot while also improving the 68K interpreter. Let me create a comprehensive TODO list and then start implementing.

Actually, let me reconsider the whole architecture. Looking at the code more carefully:

The current execution flow in PpcRunGuest is:
1. PPC code runs (the nanokernel)
2. At0x40B67C60, it's intercepted and M68kExecuteFromPPC() is called
3. This executes ONE 68K instruction and returns to the PPC loop at 0x40B67C60
4. This creates a tight loop: PPC reads instruction at 0x40B67C60, intercepts, runs 68K, returns to 0x40B67C60

But this is wrong! The PPC code at 0x40B67C60 is the common dispatch entry of the DR emulator. The DR emulator is supposed to:
1. Read the 68K opcode from the opcode table (at r29 + opcode*8)
2. Execute the 68K instruction
3. Loop back to read the next opcode

So the hook at 0x40B67C60 is actually intercepting the DR emulator's main loop. Each time through the loop, one 68K instruction is executed natively. This is correct!

But the issue is that the DR emulator's initialization (which happens before reaching 0x40B67C60) might not complete properly due to the NK crashes.

Let me look at the flow more carefully:
1. NK boots, sets up memory, reaches 0x40B126F0 (boot tail)
2. Boot tail calls blrl to PPC_EMULATOR_TRAP_TABLE (0x40B6E8C0)
3. This jumps to 0x40B6F900 (emulator-start)
4. Emulator-start reads XLM, loads KDP, enters DR emulator
5. DR emulator initializes: sets up opcode table, dispatch table6. DR emulator's main loop at 0x40B67C60 reads and executes 68K opcodes

The question is: does step 4-5 complete successfully? Looking at the auto-resume count (up to 25) and term entries (up to 60), it seems like the boot keeps crashing and recovering.

I think the most impactful thing to do right now is:

1. **Clean up the execution flow** - Remove the massive debug probe infrastructure from PpcRunGuest and replace it with a proper execution loop
2. **Fix the remaining68K interpreter gaps** - Make sure all opcodes the ROM needs are implemented
3. **Implement the core EMUL_OP handlers** - Timer, interrupts, disk, ADB
4. **Add Mac device register emulation** - VIA registersLet me create the TODO list now and start implementing.

Wait, I should also check what the m68k.c file looks like more fully, and the scripts for building.

Let me check the build scripts to verify the build environment:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\scripts"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\scripts</path>
<type>directory</type>
<entries>
build-windows.sh
run-qemu-windows.ps1

(2 entries)
</entries>
```

---

## Assistant (Build · Big Pickle · 9.1s)

_Thinking:_

Let me read the build script and the Makefile to understand how to build:

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\scripts\\build-windows.sh"
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\scripts\build-windows.sh</path>
<type>file</type>
<content>
1: #!/usr/bin/env bash
2: # Build EFI-Mac-Emulator on Windows with the chocolatey LLVM toolchain.
3: # Run from git-bash. Usage: bash scripts/build-windows.sh
4: set -euo pipefail
5: 
6: LLVM_BIN="/c/Program Files/LLVM/bin"
7: export PATH="$LLVM_BIN:$PATH"
8: 
9: cd "$(dirname "$0")/.." || exit 1
10: 
11: make -j8
12: make check
13: 
14: echo "Built: build/EFI-Mac-Emulator.efi"

(End of file - total 14 lines)
</content>
```

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
3: # Cross-builds a UEFI x86_64 application (EFI-Mac-Emulator.efi) using a
4: # clang/LLVM toolchain targeting PE/COFF, linked with lld-link. GNU-EFI
5: # provides the UEFI headers and a small runtime library.
6: #
7: # Requirements:
8: #   macOS:  brew install llvm lld  (llvm-objdump comes with llvm)
9: #   Windows: chocolatey install llvm (or add LLVM\bin to PATH), GNU make,
10: #            git-bash on PATH so `make` finds /bin/sh
11: #   Linux:  apt install clang lld
12: #
13: # Host detection: on macOS the Homebrew prefix is used if available; other
14: # hosts resolve clang/lld-link from PATH. Override via
15: # make CC=/path/to/clang LLD=/path/to/lld-link
16: 
17: SHELL   := /bin/sh
18: 
19: # --- Toolchain discovery (override via make CC=/path/to/clang LLD=/path/to/lld-link) ---
20: UNAME_S := $(shell uname -s)
21: ifeq ($(UNAME_S),Darwin)
22: LLVM_PREFIX := $(shell brew --prefix llvm 2>/dev/null || echo /opt/homebrew/opt/llvm)
23: LLD_PREFIX  := $(shell brew --prefix lld  2>/dev/null || echo /opt/homebrew/opt/lld)
24: CC      = $(LLVM_PREFIX)/bin/clang
25: LLD     ?= $(LLD_PREFIX)/bin/lld-link
26: OBJDUMP = $(LLVM_PREFIX)/bin/llvm-objdump
27: else
28: # Plain "?=" would let a host environment CC (e.g. git-bash's CC=cc) leak in;
29: # use ":=" so the PATH-resolved clang wins unless overridden on the command line.
30: CC      := clang
31: LLD     := lld-link
32: OBJDUMP := llvm-objdump
33: endif
34: 
35: # --- Layout ---
36: GNUEFI_DIR  := third_party/gnu-efi
37: GNUEFI_URL  := https://git.code.sf.net/p/gnu-efi/code
38: BUILD_DIR   := build
39: OBJ_GNUEFI  := $(BUILD_DIR)/gnuefi
40: OBJ_SRC     := $(BUILD_DIR)/src
41: TARGET      := $(BUILD_DIR)/EFI-Mac-Emulator.efi
42: 
43: # --- Flags ---
44: ARCH       := -target x86_64-pc-win32-coff
45: COMMON     := -mno-red-zone -ffreestanding -fshort-wchar \
46:               -fno-stack-protector -fno-strict-aliasing -funsigned-char \
47:               -fno-math-errno
48: CFLAGS     := $(ARCH) $(COMMON) -O2 -I $(GNUEFI_DIR)/inc -I src
49: 
50: # GNU-EFI runtime library sources (mirrors gnu-efi lib/Makefile for x86_64,
51: # minus entry.c/ctors.o (require ELF crt startup) and the .S files).
52: GNUEFI_SRCS := \
53: 	boxdraw smbios console crc data debug dpath \
54: 	error event exit guid hand hw init lock \
55: 	misc pause print sread str cmdline \
56: 	runtime/rtlock runtime/efirtlib runtime/rtstr runtime/vm runtime/rtdata \
57: 	x86_64/initplat x86_64/math x86_64/callwrap
58: 
59: # Application sources
60: APP_SRCS := \
61: 	src/main.c \
62: 	src/ui/ui_impl.c \
63: 	src/cpu/interpreter.c \
64: 	src/cpu/m68k.c \
65: 	src/cpu/translation_impl.c \
66: 	src/memory/manager_impl.c \
67: 	src/hardware/abstraction_impl.c \
68: 	src/boot/bootloader_impl.c \
69: 	src/fs/hfs.c \
70: 	src/utils/debug_impl.c \
71: 	src/platform/uefi_interface_impl.c
72: 
73: GNUEFI_OBJS := $(patsubst %,$(OBJ_GNUEFI)/%.obj,$(GNUEFI_SRCS))
74: APP_OBJS    := $(patsubst src/%,$(OBJ_SRC)/%,$(APP_SRCS:.c=.obj))
75: 
76: GNUEFI_MARK := $(GNUEFI_DIR)/inc/efi.h
77: 
78: .PHONY: all gnuefi check clean
79: 
80: all: $(TARGET)
81: 
82: # --- Acquire GNU-EFI if not present ---
83: $(GNUEFI_MARK):
84: 	@test -d $(GNUEFI_DIR) || git clone --depth 1 $(GNUEFI_URL) $(GNUEFI_DIR)
85: 
86: # --- GNU-EFI runtime library ---
87: $(GNUEFI_OBJS): $(GNUEFI_MARK)
88: $(OBJ_GNUEFI)/%.obj: $(GNUEFI_DIR)/lib/%.c
89: 	@mkdir -p $(dir $@)
90: 	$(CC) $(CFLAGS) -Wno-unknown-pragmas -c $< -o $@
91: 
92: # --- Application objects ---
93: $(OBJ_SRC)/%.obj: src/%.c
94: 	@mkdir -p $(dir $@)
95: 	$(CC) $(CFLAGS) -Wall -Werror -c $< -o $@
96: 
97: # --- Link ---
98: $(TARGET): $(GNUEFI_OBJS) $(APP_OBJS)
99: 	$(LLD) /subsystem:EFI_APPLICATION /nodefaultlib /entry:efi_main /dll \
100: 	    /out:$@ $(APP_OBJS) $(GNUEFI_OBJS)
101: 
102: # --- Verification ---
103: check: $(TARGET)
104: 	$(OBJDUMP) -x $(TARGET) | grep -E "Subsystem|Base Relocation"
105: 
106: clean:
107: 	rm -rf $(BUILD_DIR)

(End of file - total 107 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 88.6s)

_Thinking:_

Now I have a comprehensive understanding of the project. Let me create the updated TODO and documentation, then start working on the most impactful tasks.

Based on my thorough analysis, here's what I understand:

## Current State Summary

The project is at version 0.2 and has:
1. **Working PPC interpreter** with full instruction set, FPU, AltiVec
2. **Working 68K interpreter** with basic instruction set 
3. **Working UEFI integration** - GOP, Block I/O, SNP, HFS reader
4. **Working System Folder staging** from real Mac OS discs
5. **Working ROM detection and installation** (New World from Mac OS 9.2.2 disc)
6. **Working nanokernel boot** that reaches the 68K handoff point
7. **Working 68K dispatch** via the hooked DR emulator loop at 0x40B67C60

## What's Broken

The nanokernel boots but repeatedly crashes during PMDT (Page Map Descriptor Table) initialization. The NK's area manager fails on merge/guard checks. The auto-resume mechanism sends 'g' to the nanodebugger to push past each crash, but this means the NK never fully initializes.

The core issues:
1. **PMDT walk fails** - The page table structure isn't properly configured for the flat memory layout
2. **Area creation/merge fails** - The NK's memory area manager hits assertion checks
3. **SCC base address is 0** - The boot printer can't output4. **Memory layout mismatch** - The NK expects real Mac hardware memory mapping, but we're giving it a flat guest RAM## Key Insight: SheepShaver's Approach vs. Current Approach

SheepShaver bypasses the nanokernel entirely on the 68K side:
1. It runs on bare PPC (or emulated PPC)2. It patches the ROM's 68K trap vectors directly
3. The ROM's 68K code runs the Toolbox traps
4. Each trap is intercepted and handled in host C++ code
5. No MMU/BAT setup needed because everything runs in problem state

The EFIMac project is trying to:
1. Run the PPC nanokernel (which tries to set up real hardware)
2. The NK crashes because the hardware isn't real
3. We keep patching more and more NK internals to work around crashes

The pragmatic path forward is a hybrid:
- Let the NK boot (with proper patches to avoid crashes)
- Get it to the 68K handoff
- The 68K interpreter takes over and runs the Toolbox code
- EMUL_OP handlers provide the hardware abstraction

OR: Skip the NK entirely and boot directly from the 68K reset vector (like SheepShaver does on non-PPC hosts). This would mean:
1. Set up the 68K CPU context directly (SSP from ROM, PC from ROM+4)
2. Execute the ROM's 68K boot code
3. The 68K code expects the Toolbox trap table to be set up (which PpcPatchNewWorldRom does)
4. EMUL_OP handlers intercept all Toolbox calls

The second approach is actually more viable because:
- It avoids the NK entirely (which requires proper memory management, MMU, etc.)
- It's what SheepShaver does on non-PPC hosts (x86, ARM)
- The project already has the 68K interpreter and EMUL_OP frameworkHowever, there's a chicken-and-egg problem: The New World ROM's boot code is PPC, not 68K. The 68K reset vector only applies to the 68K portion. In SheepShaver:
- On PPC hosts: The PPC code runs the nanokernel, patches the 68K trap table, then the 68K emulator starts
- On non-PPC hosts: The PPC interpreter runs the nanokernel, same thing

So the NK IS needed even in SheepShaver - but SheepShaver's PPC emulator (Kheperix) can run the NK because it properly emulates supervisor mode, BAT registers, etc.

The fundamental problem with EFIMac is that the PPC interpreter doesn't emulate supervisor-level features:
- No BAT (Block Address Translation) registers
- No segment registers (SR/SDR1)
- No MMU translation
- No MMU exceptions- The NK uses supervisor features during initialization

So the NK crashes because:
1. It tries to use BAT registers (via mtspr/mfspr) → no effect (just stored in SPR array)
2. It tries to set up page tables → no MMU to walk them
3. It expects memory at certain physical addresses that don't match our flat layout

The question is: can we patch/fix the NK enough to work without MMU/BAT support?

Looking at what SheepShaver does:
- Kheperix returns a constant MSR value (0x0000f072 - problem state, no translation)
- The NK is patched to run everything in problem state
- No MMU is needed because all code runs in problem state with direct memory access

This is the KEY insight! SheepShaver patches the ROM so that:
1. The NK runs in problem state (no supervisor mode)
2. The 68K emulator runs in problem state
3. No MMU/BAT is needed
4. All memory access is flatSo the fix for EFIMac is to properly patch the ROM the way SheepShaver does:
- Patch the MSR to disable address translation (DR=0, IR=0)
- Patch the NK's memory setup to use flat memory- Patch the area manager to not do BAT/PMDT setup
- Make the68K emulator entry point work correctly

This is what PpcPatchNewWorldRom is trying to do, but it's not complete enough. The NK still crashes because it tries to use features that aren't emulated.

Let me now look at what specific patches SheepShaver applies to the ROM...

Actually, looking at the current code more carefully, the issue is visible in the main.c:

```c
g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI | PPC_MSR_DR;
```

MSR bit 0x10 (PPC_MSR_DR = Data Relocation) is being SET! This means the NK is trying to use data address translation (BAT/segment registers). But our interpreter doesn't implement address translation - CpuRead32/CpuWrite32 just access the flat guest memory map directly. So when the NK accesses memory through the data relocation mechanism, it gets the wrong addresses.

SheepShaver sets DR=0 (no data relocation) because it patches the NK to not use address translation. This is the correct approach.

So the fix involves:
1. Setting MSR = ME | RI (no DR, no IR) - don't enable address translation
2. Patching the NK code paths that assume address translation is enabled
3. Ensuring all memory access goes through the flat memory map

But wait, the code also sets up SPRG4 and seeds various caller structures... This is all SheepShaver-style patching. Let me think about what's still missing.

The PMDT walk failure is the most critical issue. The NK's memory manager tries to:
1. Create a PMDT (Page Map Descriptor Table) that maps physical addresses2. The PMDT walk at 0x40B1F418 reads entries and dispatches on flags&0xE00
3. The PmdFixed injection at 0x40B1F418 tries to fix the table but it's not working correctly

The root cause: The NK expects to map physical memory starting at address 0, but our guest RAM starts at 0x10000000. The NK creates areas that map [0, RAMSize) but the physical addresses in our flat map don't match.

Looking at SheepShaver's KernelData setup, it sets:
- physical RAM base = 0 (at ConfigInfo+0x360)
- PVR and other hardware parameters
- The NK then maps [0, RAMSize) which works because SheepShaver's mmap'd memory starts at 0

In EFIMac, the guest RAM is at 0x10000000. But the NK expects physical RAM at 0. This mismatch causes the PMDT to map the wrong addresses.

The fix: Either:
1. Map guest RAM at address 0 (like SheepShaver) - but this conflicts with low-memory globals
2. Patch the NK's PMDT setup to account for the offset
3. Use the DR=0 path that doesn't need PMDT

Looking at the code, the PmdFixed injection tries option 2, but it's incomplete. The PMDT entry for chunk 0 maps [0, 0xFFF6) which should cover256MB-4KB, but the actual guest RAM is at 0x10000000.

I think the most robust fix is option 3: ensure DR=0 (no data relocation) and patch the NK to not do PMDT/area setup. But this requires extensive ROM patching.

Alternatively, we could move the guest RAM to address 0 and put low-memory globals in a different location. But this would require significant changes to the memory manager.

Actually, wait. Let me re-read the code more carefully. In the NK's PMDT injection:

```c
// entry 1: RAM [0, 0xFFF7000).
CpuWrite16(Base + 8, 0x0000);  // page = 0
CpuWrite16(Base + 10, 0xFFF6); // count = 0xFFF6
CpuWrite32(Base + 12, 0x00000000); // type = 0 (area create)
```

This maps pages 0 through 0xFFF6 (about 255.9 MB) as RAM. But the guest RAM starts at 0x10000000 (256 MB). So the PMDT is mapping the wrong region!

The fix would be to map [0x1000, 0x1FFF] instead (which corresponds to physical addresses 0x10000000-0x1FFFFFFF for 256MB).

Actually no, the PMDT entries are in terms of page numbers (4KB pages). So:
- Page 0 = physical address 0x00000000
- Page 0x1000 = physical address 0x00100000 (1MB)
- Page 0x10000 = physical address 0x10000000 (256MB)

So for guest RAM at 0x10000000:
- Start page = 0x10000 (0x10000000 / 0x1000)
- End page = 0x10000 + (RAMSize / 0x1000) - 1

For 256MB RAM:
- End page = 0x10000 + 0x10000 - 1 = 0x1FFFF

But the PmdFixed code maps [0, 0xFFF6] which is wrong!

OK so the fix is to update PmdFixed to map the correct physical range. Let me also think about the DR=0 vs DR=1 path...

Actually, I think the simplest and most robust approach is:

1. **Set MSR without DR/IR** - Don't enable address translation
2. **Patch the NK to not use PMDT** - Bypass the memory management setup
3. **Use SheepShaver's approach** - Run everything in flat memory space

This is what SheepShaver does. The key patches are:
- The NK is patched so that all code runs in problem state (no supervisor mode)
- Memory management is bypassed (no MMU, no BAT)
- All memory access is flat (physical = logical)
- The 68K emulator handles all I/O through trap dispatch

The PpcPatchNewWorldRom function already does many of these patches but doesn't go far enough. We need to:
1. Not set MSR[DR] (keep it at 0)
2. Patch the NK's CreateArea calls to succeed without PMDT
3. Ensure the NK's scheduler can start the Mac OS boot task

Let me think about what specific changes to make...

Actually, I realize I'm getting too deep into the weeds. Let me step back and create a comprehensive plan. The user asked me to:
1. Analyze the SheepShaver project ✓ (done)
2. Determine objectives to boot to desktop
3. Update project documentation
4. Start iterating towards completion

Let me create the TODO and then start with the most impactful changes.

The most critical path to desktop boot is:

1. **Fix the nanokernel boot** - Fix the PMDT/area issues so the NK completes initialization
2. **Fix the 68K interpreter to handle more opcodes** - The ROM needs many more opcodes than currently implemented
3. **Implement core EMUL_OP handlers** - Timer, interrupts, disk, keyboard
4. **Add Mac device register emulation** - VIA, SCC registers
5. **Implement the boot sequence** - ROM → 68K boot → System Folder → Finder → Desktop

Let me now create the updated TODO.md and start working on the first objective.

Now I have a comprehensive understanding of the project. Let me create the plan and update the documentation.

**Tool: todowrite**

**Input:**
```json
{
  "todos": [
    {
      "content": "Analyze SheepShaver architecture and map EFIMac gaps",
      "status": "completed",
      "priority": "high"
    },
    {
      "content": "Update TODO.md with comprehensive roadmap to desktop boot",
      "status": "in_progress",
      "priority": "high"
    },
    {
      "content": "Fix nanokernel boot: PMDT mapping, MSR[DR], area creation",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Expand 68K interpreter with missing opcodes for ROM execution",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Implement core EMUL_OP handlers (timer, disk, ADB, interrupts)",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Add Mac VIA/SCC device register emulation in guest memory",
      "status": "pending",
      "priority": "medium"
    },
    {
      "content": "Implement continuous68K execution loop (beyond single-step PPC hook)",
      "status": "pending",
      "priority": "medium"
    },
    {
      "content": "Implement Toolbox trap dispatch (A-line traps → EMUL_OP)",
      "status": "pending",
      "priority": "medium"
    },
    {
      "content": "Add boot volume mounting and Finder launch sequence",
      "status": "pending",
      "priority": "medium"
    },
    {
      "content": "Wire framebuffer, keyboard, mouse to UEFI protocols",
      "status": "pending",
      "priority": "low"
    }
  ]
}
```

**Output:**
```
[
  {
    "content": "Analyze SheepShaver architecture and map EFIMac gaps",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Update TODO.md with comprehensive roadmap to desktop boot",
    "status": "in_progress",
    "priority": "high"
  },
  {
    "content": "Fix nanokernel boot: PMDT mapping, MSR[DR], area creation",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Expand 68K interpreter with missing opcodes for ROM execution",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Implement core EMUL_OP handlers (timer, disk, ADB, interrupts)",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Add Mac VIA/SCC device register emulation in guest memory",
    "status": "pending",
    "priority": "medium"
  },
  {
    "content": "Implement continuous68K execution loop (beyond single-step PPC hook)",
    "status": "pending",
    "priority": "medium"
  },
  {
    "content": "Implement Toolbox trap dispatch (A-line traps → EMUL_OP)",
    "status": "pending",
    "priority": "medium"
  },
  {
    "content": "Add boot volume mounting and Finder launch sequence",
    "status": "pending",
    "priority": "medium"
  },
  {
    "content": "Wire framebuffer, keyboard, mouse to UEFI protocols",
    "status": "pending",
    "priority": "low"
  }
]
```

---

## Assistant (Build · Big Pickle · 8.0s)

_Thinking:_

Now let me read the remaining parts of the m68k.c and the full PpcRunGuest to understand the current 68K implementation before I update the TODO.

Let me check what 68K opcodes are currently implemented:

**Tool: bash**

**Input:**
```json
{
  "command": "wc -l src/cpu/m68k.c",
  "workdir": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac"
}
```

**Output:**
```
[31;1mwc: [31;1mThe term 'wc' is not recognized as a name of a cmdlet, function, script file, or executable program.[0m
[31;1m[31;1mCheck the spelling of the name, or if a path was included, verify that the path is correct and try again.[0m

```

**Tool: grep**

**Input:**
```json
{
  "path": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\m68k.c",
  "pattern": "case 0x"
}
```

**Output:**
```
Found 100 matches (more matches available)
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\m68k.c:
  Line 1296:     case 0x0: TakeBranch = TRUE;  break;  // BRA (always)

  Line 1297:     case 0x1: TakeBranch = TRUE;  break;  // BSR (always)

  Line 1298:     case 0x2: TakeBranch = !M68kTestFlag (M68K_CCR_C) && !M68kTestFlag (M68K_CCR_Z); break;  // BHI

  Line 1299:     case 0x3: TakeBranch = M68kTestFlag (M68K_CCR_C) || M68kTestFlag (M68K_CCR_Z); break;    // BLS

  Line 1300:     case 0x4: TakeBranch = !M68kTestFlag (M68K_CCR_C); break;                                 // BCC/BHS

  Line 1301:     case 0x5: TakeBranch = M68kTestFlag (M68K_CCR_C); break;                                  // BCS/BLO

  Line 1302:     case 0x6: TakeBranch = !M68kTestFlag (M68K_CCR_Z); break;                                 // BNE

  Line 1303:     case 0x7: TakeBranch = M68kTestFlag (M68K_CCR_Z); break;                                  // BEQ

  Line 1304:     case 0x8: TakeBranch = !M68kTestFlag (M68K_CCR_V); break;                                 // BVC

  Line 1305:     case 0x9: TakeBranch = M68kTestFlag (M68K_CCR_V); break;                                  // BVS

  Line 1306:     case 0xA: TakeBranch = !(M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;  // BPL

  Line 1307:     case 0xB: TakeBranch = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;   // BMI

  Line 1308:     case 0xC: TakeBranch = (M68kTestFlag (M68K_CCR_N) == M68kTestFlag (M68K_CCR_V)) && !M68kTestFlag (M68K_CCR_Z); break; // BGT

  Line 1309:     case 0xD: TakeBranch = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)) || M68kTestFlag (M68K_CCR_Z); break;  // BLE

  Line 1310:     case 0xE: TakeBranch = !(M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;  // BGE

  Line 1311:     case 0xF: TakeBranch = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;   // BLT

  Line 1374:     case 0x0: ConditionMet = FALSE; break;  // DBRA (always false)

  Line 1375:     case 0x1: ConditionMet = TRUE;  break;  // DBSR (always true)

  Line 1376:     case 0x2: ConditionMet = !M68kTestFlag (M68K_CCR_C) && !M68kTestFlag (M68K_CCR_Z); break;

  Line 1377:     case 0x3: ConditionMet = M68kTestFlag (M68K_CCR_C) || M68kTestFlag (M68K_CCR_Z); break;

  Line 1378:     case 0x4: ConditionMet = !M68kTestFlag (M68K_CCR_C); break;

  Line 1379:     case 0x5: ConditionMet = M68kTestFlag (M68K_CCR_C); break;

  Line 1380:     case 0x6: ConditionMet = !M68kTestFlag (M68K_CCR_Z); break;

  Line 1381:     case 0x7: ConditionMet = M68kTestFlag (M68K_CCR_Z); break;

  Line 1382:     case 0x8: ConditionMet = !M68kTestFlag (M68K_CCR_V); break;

  Line 1383:     case 0x9: ConditionMet = M68kTestFlag (M68K_CCR_V); break;

  Line 1384:     case 0xA: ConditionMet = !M68kTestFlag (M68K_CCR_N); break;

  Line 1385:     case 0xB: ConditionMet = M68kTestFlag (M68K_CCR_N); break;

  Line 1386:     case 0xC: ConditionMet = (M68kTestFlag (M68K_CCR_N) == M68kTestFlag (M68K_CCR_V)) && !M68kTestFlag (M68K_CCR_Z); break;

  Line 1387:     case 0xD: ConditionMet = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)) || M68kTestFlag (M68K_CCR_Z); break;

  Line 1388:     case 0xE: ConditionMet = !(M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;

  Line 1389:     case 0xF: ConditionMet = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;

  Line 1939:     case 0x0: {

  Line 1956:         case 0x0: // ORI  (special: 0x003C = ORI to CCR, 0x007C = ORI to SR)

  Line 1965:         case 0x1: // Reserved (no valid immediate op; mode!=1 caught above)

  Line 1968:         case 0x2: // ANDI  (special: 0x023C = ANDI to CCR, 0x027C = ANDI to SR)

  Line 1977:         case 0x3: // Reserved

  Line 1980:         case 0x4: // SUBI

  Line 1983:         case 0x5: // Reserved

  Line 1986:         case 0x6: // ADDI

  Line 1989:         case 0x7: // Reserved

  Line 1992:         case 0x8: {

  Line 2051:         case 0x9: // Reserved

  Line 2054:         case 0xA: // EORI  (special: 0x0A3C = EORI to CCR, 0x0A7C = EORI to SR)

  Line 2063:         case 0xB: // Reserved

  Line 2066:         case 0xC: // CMPI

  Line 2069:         case 0xD: // Reserved

  Line 2072:         case 0xE: // Reserved

  Line 2075:         case 0xF: // Reserved

  Line 2082:     case 0x1: M68kExecuteMove (Opcode); break;  // MOVE.B

  Line 2083:     case 0x2: M68kExecuteMove (Opcode); break;  // MOVE.L

  Line 2084:     case 0x3: M68kExecuteMove (Opcode); break;  // MOVE.W

  Line 2086:     case 0x4: {

  Line 2216:     case 0x5: {

  Line 2227:             case 0x0: Set = TRUE; break;   // ST

  Line 2228:             case 0x1: Set = FALSE; break;  // SF

  Line 2229:             case 0x2: Set = !M68kTestFlag (M68K_CCR_C) && !M68kTestFlag (M68K_CCR_Z); break;

  Line 2230:             case 0x3: Set = M68kTestFlag (M68K_CCR_C) || M68kTestFlag (M68K_CCR_Z); break;

  Line 2231:             case 0x4: Set = !M68kTestFlag (M68K_CCR_C); break;

  Line 2232:             case 0x5: Set = M68kTestFlag (M68K_CCR_C); break;

  Line 2233:             case 0x6: Set = !M68kTestFlag (M68K_CCR_Z); break;

  Line 2234:             case 0x7: Set = M68kTestFlag (M68K_CCR_Z); break;

  Line 2235:             case 0x8: Set = !M68kTestFlag (M68K_CCR_V); break;

  Line 2236:             case 0x9: Set = M68kTestFlag (M68K_CCR_V); break;

  Line 2237:             case 0xA: Set = !M68kTestFlag (M68K_CCR_N); break;

  Line 2238:             case 0xB: Set = M68kTestFlag (M68K_CCR_N); break;

  Line 2239:             case 0xC: Set = (M68kTestFlag (M68K_CCR_N) == M68kTestFlag (M68K_CCR_V)) && !M68kTestFlag (M68K_CCR_Z); break;

  Line 2240:             case 0xD: Set = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)) || M68kTestFlag (M68K_CCR_Z); break;

  Line 2241:             case 0xE: Set = !(M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;

  Line 2242:             case 0xF: Set = (M68kTestFlag (M68K_CCR_N) != M68kTestFlag (M68K_CCR_V)); break;

  Line 2253:     case 0x6: {

  Line 2259:     case 0x7: {

  Line 2265:     case 0x8: {

  Line 2325:     case 0x9: {

  Line 2340:     case 0xA: {

  Line 2347:     case 0xB: {

  Line 2370:     case 0xC: {

  Line 2408:     case 0xD: {

  Line 2423:     case 0xE: {

  Line 2429:     case 0xF: {


C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c:
  Line 1155:         case 0x00:  // vmhaddshs vD, vA, vB, vC: sat((vA*vB + vC) >> 1)

  Line 1163:         case 0x01:  // vmhraddshs: sat((vA*vB + vC + 0x4000) >> 15)

  Line 1171:         case 0x02:  // vmladduhm: sat16(vA*vB + vC), unsigned halfwords

  Line 1178:         case 0x04:  // vmsumubm

  Line 1189:         case 0x05:  // vmsummbm

  Line 1200:         case 0x06:  // vmsumuhm

  Line 1211:         case 0x07:  // vmsumuhs

  Line 1224:         case 0x08:  // vmsumshm

  Line 1235:         case 0x09:  // vmsumshs

  Line 1248:         case 0x0A:  // vsel vD, vA, vB, vC: (vC & vA) | (~vC & vB)

  Line 1255:         case 0x0B:  // vperm vD, vA, vB, vC

  Line 1263:         case 0x0C:  // vsldoi vD, vA, vB, SHB

  Line 1277:         case 0x0D:  // vpermxor vD, vA, vB, vC: (vA ^ vB) permuted by vC

  Line 1284:         case 0x0E:  // vmaddfp vD, vA, vC, vB: vD = vA*vC + vB

  Line 1290:         case 0x0F:  // vnmsubfp vD, vA, vC, vB: vD = -(vA*vC - vB)

  Line 1296:         case 0x1B:  // vpermr vD, vA, vB, vC (reverse of vperm: bit-0x10 selects vA)

  Line 1304:         case 0x1C:  // vaddeuqm vD, vA, vB, vC (carry in/out in vC[127])

  Line 1305:         case 0x1D:  // vaddecuq

  Line 1306:         case 0x1E:  // vsubeuqm

  Line 1307:         case 0x1F:  // vsubecuq


(Results truncated. Consider using a more specific path or pattern.)
```

---

## Assistant (Build · Big Pickle · 121.1s)

_Thinking:_

Now I have a thorough understanding. Let me write the updated TODO.md and then begin implementation. Let me read the final part of the interpreter to understand the execution flow.

Now I have a thorough understanding of the entire project. Let me update the TODO.md with a comprehensive roadmap, then start implementing the critical fixes.

**Tool: write**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\TODO.md",
  "content": "# EFI Mac OS Boot Layer — Implementation Plan\n\n## Current State\n\nThe project is a functional **heavy UEFI bootloader** for classic Mac OS. It\nbuilds a PowerPC Mac boot image from UEFI standard protocols, reads classic Mac\ndiscs in place, installs real Mac firmware into the guest image, and self-tests\nthe whole path. The **New World ROM boots through the nanokernel** and hands off\nto the 68K DR emulator, where execution enters the native 68K interpreter. The\nguest OS does **not** reach the desktop yet — the remaining work is completing\nthe 68K interpreter, implementing EMUL_OP device handlers, and wiring Mac\nhardware register emulation to UEFI protocols.\n\n### Verified end-to-end (Windows host, QEMU + OVMF)\n\n- PowerPC CPU self-test **35/35** (includes the FPU core: opcodes 48-63 gated on\n  MSR[FP], FP-unavailable exception 0x800, FPSCR, A-form arithmetic).\n- 68K CPU self-test passes (instruction decode, addressing modes, flag compute).\n- Boot memory-map self-test **7/7** with the demo ROM, **5/5** with a real ROM\n  (region presence + CHRP signature + read-only enforcement).\n- System Folder / driver self-test **7/7**.\n- **Real New World ROM discovered and installed** from a genuine Mac OS 9.2.2\n  install disc (`Power Mac G4 Install:System Folder:Mac OS ROM`, 2,763,530\n  bytes, `<CHRP-BOOT>` signature) at guest `0x40800000`.\n- All non-empty Extensions stage with **0 failures**: System 7.5.3 2/2, Mac OS\n  8.1 18/18, Mac OS 9.2.2 25/25 (up to 64 drivers supported).\n- Graphics blits verified across every GOP pixel; Block I/O and SNP exercised\n  with real hardware calls.\n- **New World ROM nanokernel boots** through the full boot sequence: NK\n  initialization, memory setup, PMDT walk, and 68K DR-emulator handoff via the\n  patched trap table.\n- **68K DR emulator entry** reached at `0x40B6F900`; the native 68K interpreter\n  hooks the common dispatch at `0x40B67C60` and executes 68K instructions.\n- The NK nanodebugger fires on assertion failures during area creation and PMDT\n  setup; auto-resume sends 'g' to continue past each check point.\n\n### Recent work\n\n- **Heavy-bootloader framing.** UEFI protocols (GOP/BlockIO/SNP/SimpleFS) are\n  the hardware abstraction; simulated Mac devices are wired to them. Docs and\n  boot output reframed from \"emulator\" to \"boot layer\".\n- **ROM type awareness.** `PPC_ROM_TYPE_OLD_WORLD/NEW_WORLD/DEMO`; the boot\n  self-test no longer assumes the demo ROM's `ROM1`/reset-vector layout when a\n  real ROM is installed.\n- **HFS driver staging.** Catalog-ID-based file lookup\n  (`PpcHfsGetEntryById`), whole-catalog `Mac OS ROM` search\n  (`PpcHfsFindMacOsRom`), auto block-size detection, multi-overflow extents,\n  empty-file skipping (7.5.3's 0-byte Finder).\n- **Native 68K interpreter** (`src/cpu/m68k.c`, ~2900 lines): all data-movement\n  opcodes, arithmetic, logic, shifts, branches (Bcc/DBcc), bit ops, system\n  calls (TRAP/RTE/MOVE SR), effective-address computation, CCR flag compute.\n- **68K ↔ PPC context synchronization** via `M68kSyncFromPPC` / `M68kSyncToPPC`\n  (D0-D7 = PPC r8-r15, A0-A6 = r16-r22, A7 = r1, PC = r24, SR = r25).\n- **New World ROM patching** (`PpcPatchNewWorldRom`): ConfigInfo LA fields\n  redirected, twi kernel-trap table rewritten, 5 emulator-entry routines\n  installed, EMUL_OP dispatch markers installed, rlwimi dispatch-bit-20\n  neutralised, XLM globals seeded.\n- **68K DR-emulator hook** at `0x40B67C60`: the PPC interpreter intercepts the\n  common dispatch and calls `M68kExecuteFromPPC()` for native 68K execution.\n- **PPC-level 68K opcode hooks**: MOVE SR (0x46FC), RESET (0x4E70), escape\n  (0x4E7B), MOVEQ #imm,Dn (0x7F1A) routed through the interpreter for\n  opcodes not yet in the opcode table.\n\n## SheepShaver Architecture Reference\n\nSheepShaver is a **paravirtualizer**, not a hardware emulator. Key design:\n\n1. **Patches the 68K ROM** so all code runs in problem state (no supervisor mode,\n   no MMU). Only `mfmsr` is implemented, returning `0x0000f072` (ME|RI|FP|PR).\n2. **Replaces Toolbox trap vectors** (A-line traps) with `EMUL_OP` instructions —\n   custom 68K opcodes that dispatch to host-side C++ handlers.\n3. **Intercepts all driver calls**: disk, SCSI, audio, ADB (keyboard/mouse),\n   timer, serial, ethernet, video — all via EMUL_OP dispatch.\n4. **Two-way bridge**: `Execute68k()` calls 68K code from native;\n   `ExecuteNative()` calls native code from 68K via EMUL_OP.\n5. **KernelData** at `0x68FFE000`: hardware config (PVR, clock, OpenPIC, OF\n   device tree) filled per-ROM type.\n6. **XLM** (\"eXtra Low Memory\") at `0x2800`: communication mailbox between the\n   emulator and Mac OS (`XLM_RUN_MODE`, `XLM_SIGNATURE`, native fn ptrs).\n7. **EMUL_OP selectors** (see `emul_op.h`): ~50 selectors covering XPRAM,\n   NVRAM, Sony, Disk, CDROM, Audio, ADB, Timer, Clipboard, SCSI, ExtFS, etc.\n\n## Gap Analysis: SheepShaver vs. EFIMac\n\n| Component | SheepShaver | EFIMac Status | Gap |\n|-----------|-------------|---------------|-----|\n| PPC interpreter | Kheperix (interp + JIT) | Full interpreter (5300+ lines) | Complete |\n| 68K interpreter | Built into DR emulator | Native C interpreter (2900 lines) | Needs more opcodes |\n| ROM patching | 68K trap table + EMUL_OP markers | ConfigInfo + trap table + entry routines | Mostly complete |\n| EMUL_OP dispatch | Full (50+ selectors) | Markers in ROM, hook in PPC interp | **Not implemented** |\n| VIA emulation | Not needed (all via EMUL_OP) | Not implemented | **Must build** |\n| Timer/interrupt | EMUL_OP_INSTIME/RMVTIME/IRQ | Not implemented | **Must build** |\n| Disk driver | EMUL_OP_DISK_* | Not implemented | **Must build** |\n| ADB (input) | EMUL_OP_ADBOP | Not implemented | **Must build** |\n| Audio | EMUL_OP_AUDIO_DISPATCH | Not implemented | **Must build** |\n| SCSI | EMUL_OP_SCSI_DISPATCH | Not implemented | **Must build** |\n| Video | Custom driver + QuickDraw accel | Framebuffer blit only | **Must build driver** |\n| Ethernet | EMUL_OP + Slirp | SNP frame transmit | Needs integration |\n| Name Registry | EMUL_OP_NAME_REGISTRY | Not implemented | **Must build** |\n| Memory (BAT/MMU) | None (flat, problem state) | Flat memory, no MMU | Match SheepShaver |\n| KernelData | ROM-type-specific init | Not seeded | **Must seed** |\n| XLM globals | Written at 0x2800 | Written by PpcPatchNewWorldRom | Complete |\n\n## Implementation Roadmap: Boot to Desktop\n\n### Phase A: Fix Nanokernel Boot (critical path)\n\nThe NK currently boots, hits assertion failures during PMDT/area setup, and the\nauto-resume pushes past each crash. The NK needs to complete initialization\ncleanly so the 68K emulator handoff is in a valid state.\n\n#### A.1 Fix MSR[DR] — disable data relocation\n- **File**: `src/main.c` (line ~779)\n- Currently sets `MSR |= PPC_MSR_DR` (bit 0x10). This enables data address\n  relocation, but the interpreter has no BAT/segment translation — all memory\n  access is flat. DR=1 causes the NK's PMDT walk to fail because it expects\n  translated addresses.\n- **Fix**: Remove `PPC_MSR_DR` from the MSR. The NK should boot with DR=0\n  (flat memory), matching SheepShaver's approach.\n\n#### A.2 Fix PMDT mapping for guest RAM at 0x10000000\n- **File**: `src/cpu/interpreter.c` (PmdFixed injection ~line 4628)\n- The PMDT injection maps pages [0, 0xFFF6) as RAM, but guest RAM starts at\n  `0x10000000` (page `0x10000`). The NK's area manager maps wrong physical\n  addresses.\n- **Fix**: Map pages `[0x10000, 0x10000 + RAMSize/4K)` as the RAM region in the\n  PMDT. Also ensure low-memory globals (page 0) and the ROM window are\n  represented in the PMDT.\n\n#### A.3 Fix SCC base address for NK boot printer\n- **File**: `src/main.c` and `src/cpu/interpreter.c`\n- The NK's boot printer polls `[r28+2]` bit 2 for SCC Tx-empty. r28 is 0 because\n  `NoIdeaR23 [KDP-0x900]` is never seeded. The SCC on a Power Mac G4 is at\n  `0x80013020` (CHRP SCC base).\n- **Fix**: Seed the SCC base in the KernelData/ECB area. Alternatively, seed a\n  \"virtual SCC\" at `0x20002` (the address the boot printer is using) with\n  the Tx-ready bit pre-set.\n\n#### A.4 Fix area creation / merge assertions\n- **File**: `src/cpu/interpreter.c` (PmdFixed, MergeTraced ~lines 4628-4699)\n- The NK panics at the area-merge guard check (0x40B1F67C) because area structs\n  have uninitialized fields at offsets +0x24/+0x28/+0x2C.\n- **Fix**: Pre-seed the NK's area structures so the merge/guard checks pass.\n  Alternatively, NOP the problematic assertion in the ROM.\n\n#### A.5 Seed KernelData for New World G4\n- **File**: `src/boot/bootloader_impl.c` (PpcPatchNewWorldRom)\n- The ROM's ConfigInfo at `ROM+0x30D000` needs `LA_InfoRecord` (0x68FFE000),\n  `LA_KernelData` (0x68FFE000), `LA_EmulatorData` (0x68FFF000),\n  `physical RAM base` (0), and `68K reset vector` (ROM+0x2A). These are already\n  seeded. However, the KernelData structure itself at `0x68FFE000` needs\n  hardware fields: PVR, bus clock, OpenPIC base, OF device tree pointers.\n- **Fix**: Fill the KernelData struct with G4/PowerMac values matching the ROM.\n\n### Phase B: Complete 68K Interpreter\n\nThe native 68K interpreter handles basic opcodes but needs expansion for the\nROM's Toolbox code to execute.\n\n#### B.1 Additional opcodes (priority order for boot)\n- [ ] Bit manipulation: BTST/BSET/BCLR/BCHG (register and memory)\n- [ ] Shift/Rotate: ASL/ASR, LSL/LSR, ROL/ROR (register and immediate)\n- [ ] Multiply/Divide: MULS.W, MULU.W, DIVS.W, DIVU.W\n- [ ] EXT.W, EXT.L (sign extension)\n- [ ] EXG (exchange registers)\n- [ ] SWAP (byte-swap halves of Dn)\n- [ ] PEA (push effective address)\n- [ ] JMP, JSR (jump/subroutine — needed for Toolbox calls)\n- [ ] RTS, RTR (return from subroutine/trap)\n- [ ] Line A (1010) / Line F (1111) emulation: intercept as trap vectors\n- [ ] MOVEM with register lists\n- [ ] NEGX, NEG (with extend)\n- [ ] ABCD, SBCD, NBCD (BCD arithmetic)\n- [ ] TAS (test and set — needed for ADB/mutex)\n\n#### B.2 Exception dispatch\n- [ ] 68K exception vector table at `0x0000-0x03FF` — read vector addresses,\n  push PC+SR to supervisor stack, dispatch\n- [ ] Interrupt exception (level 1-7): VBL, SCC, SCSI, slot\n- [ ] Trap #1 (Mac OS system call): `_Trap` dispatch through the trap table\n- [ ] Line 1010 / Line 1111: A-line / F-line traps (Toolbox)\n- [ ] Illegal instruction exception\n\n#### B.3 Memory access layer\n- [ ] Ensure all 68K memory access goes through the PPC guest memory path\n  (already done via `M68kReadByte/WriteByte` → `PpcReadGuestByte`)\n- [ ] Add memory-mapped I/O dispatch: detect VIA (0x5000xxx), SCC, SCSI\n  register windows and route to device emulation\n\n### Phase C: EMUL_OP Device Handlers\n\nThese implement the hardware abstraction layer, replacing real Mac devices with\nhost-side UEFI protocol calls.\n\n#### C.1 EMUL_OP framework\n- [ ] `PpcEmulatorDispatchOp()` in interpreter.c: already intercepts `mulli r0,r0,n`\n  markers. Route to `M68kEmulOpDispatch(selector)` in `src/cpu/m68k.c`.\n- [ ] `M68kEmulOpDispatch()`: switch on selector, read 68K register state from\n  context, call handler, return to DR emulator loop.\n\n#### C.2 Timer system (highest priority — drives everything)\n- [ ] `PPC_OP_INSTIME / RMVTIME / PRIMETIME`: replace `InsTime/RmvTime/PrimeTime`\n  Toolbox calls with host-side timer management.\n- [ ] `PPC_OP_MICROSECONDS`: return monotonic microsecond count from UEFI\n  `QueryPerformanceCounter`.\n- [ ] 1Hz periodic interrupt: fire VBL (vertical blank) at 60Hz via a timer that\n  sets the VIA interrupt flag.\n\n#### C.3 Interrupt system\n- [ ] `PPC_OP_IRQ`: the Level 1 interrupt handler. Process:\n  - `INTFLAG_VIA` → timer tick, VBL, Sony/Disk/CDROM polling\n  - `INTFLAG_SERIAL` → SCC interrupt\n  - `INTFLAG_ETHER` → Ethernet interrupt\n  - `INTFLAG_TIMER` → decrementer/1Hz timer\n  - `INTFLAG_AUDIO` → audio buffer completion\n  - `INTFLAG_ADB` → ADB polling (keyboard/mouse)\n\n#### C.4 Disk driver\n- [ ] `PPC_OP_DISK_OPEN`: open the boot volume (identify HFS partition via\n  Block I/O, store partition info).\n- [ ] `PPC_OP_DISK_PRIME`: read/write blocks via `PpcReadDiskBlock` →\n  UEFI Block I/O `ReadBlocks`/`WriteBlocks`.\n- [ ] `PPC_OP_DISK_CONTROL`: ioctl (drive status, geometry, eject).\n- [ ] `PPC_OP_DISK_STATUS`: return drive ready flag.\n\n#### C.5 ADB (keyboard/mouse)\n- [ ] `PPC_OP_ADBOP`: ADB manager operations:\n  - Poll keyboard: translate UEFI `ReadKeyStroke` to ADB key codes\n  - Poll mouse: translate UEFI pointer protocol to ADB mouse data\n  - Register device handlers\n\n#### C.6 Video driver\n- [ ] `PPC_OP_INSTALL_DRIVERS`: install the Mac video driver at driver area.\n  The driver's `DoDriverIO` handler maps to:\n  - `Open`: set video mode (resolution, bit depth)\n  - `Prime`: initial framebuffer setup\n  - `Control`: mode switch, palette, vbank\n  - `Status`: current mode info\n- [ ] `PPC_OP_VIDEO_DOIO`: dispatch to GOP framebuffer operations. Convert\n  big-endian Mac pixels to GOP pixel format on blit.\n\n#### C.7 Audio\n- [ ] `PPC_OP_AUDIO_DISPATCH`: audio component dispatch. Map to ring buffer\n  in guest RAM; host reads PCM samples and plays through UEFI (no standard)\n  or serial debug.\n\n#### C.8 SCSI\n- [ ] `PPC_OP_SCSI_DISPATCH`: SCSI Manager emulation. Route reads/writes\n  to Block I/O for the boot volume and attached discs.\n\n#### C.9 Name Registry\n- [ ] `PPC_OP_NAME_REGISTRY`: emulate the Open Firmware Name Registry.\n  Provide device tree nodes for: `/cpus/cpu@0`, `/mac-io`, `/nvram`,\n  `/scsi`, `/ethernet`, `/display`.\n\n#### C.10 Other EMUL_OP selectors\n- [ ] `PPC_OP_SONY_OPEN/PRIME/CONTROL/STATUS`: floppy driver (stub or\n  map to HFS image)\n- [ ] `PPC_OP_CDROM_OPEN/PRIME/CONTROL/STATUS`: CD-ROM driver\n- [ ] `PPC_OP_SOUNDIN_*`: sound input (stub)\n- [ ] `PPC_OP_DEBUG_STR`: `_DebugStr` — print to serial\n- [ ] `PPC_OP_RESET`: Mac OS reset handler\n- [ ] `PPC_OP_CHECK_SYSV`: version compatibility check\n- [ ] `PPC_OP_CHECKLOAD`: resource loading hook\n- [ ] `PPC_OP_EXTFS_COMM/HFS`: external file system\n- [ ] `PPC_OP_IDLE_TIME`: idle/sleep when no events\n- [ ] `PPC_OP_ZERO_SCRAP / PUT_SCRAP / GET_SCRAP`: clipboard\n\n### Phase D: Mac Hardware Register Emulation\n\nMac Toolbox code and drivers read/write hardware registers directly. These must\nbe backed in guest memory with emulated behavior.\n\n#### D.1 VIA (Versatile Interface Adapter) — 0x50000000\n- [ ] VRA/VRB (timer A/B counters): decrement at 60Hz, set IRQ on expiry\n- [ ] IFR (interrupt flag register): aggregate all device interrupt sources\n- [ ] IER (interrupt enable register): per-bit enable mask\n- [ ] SR (shift register): ADB data transfer\n- [ ] DIRA/DIRB (data direction): configure input/output\n- [ ] PA/PB (port data): bit-level device control\n\n#### D.2 SCC (Serial Communications Controller) — 0x80013020\n- [ ] RR0 (receive status): data available, FIFO depth\n- [ ] RR3 (interrupt pending): which channel has pending IRQ\n- [ ] WR0 (command): reset, send (SCC boot printer)\n- [ ] WR7 (misc): enable/disable\n\n#### D.3 SCSI (NCR 53C96) — 0x80010000\n- [ ] DMACNT/SCPDMA: DMA transfer control\n- [ ] SCMD/SCISR: command/interrupt status\n- [ ] SCFIFO: data FIFO\n\n#### D.4 Slot Management — 0x50Fxxxxx\n- [ ] S-slot ROM: auto-inject device directory entries for video, SCSI,\n  ethernet (matching SheepShaver's `SlotManager`)\n\n### Phase E: Boot Sequence Integration\n\n#### E.1 Continuous 68K execution loop\n- [ ] Replace the per-instruction PPC hook with a dedicated 68K execution\n  mode: when the NK hands off to the DR emulator, enter `M68kExecuteBlock()`\n  as the primary loop. Return to PPC only when a supervisor-level event\n  (interrupt, exception) requires it.\n- [ ] Trigger timer interrupts via the VIA at 60Hz (VBL) to drive the Mac\n  OS event loop.\n\n#### E.2 Toolbox trap dispatch\n- [ ] A-line traps (Line 1010): read trap word, dispatch through the\n  Toolbox trap table in low memory (0x0). For `_Gate`-style traps, follow\n  the dispatch chain.\n- [ ] Trap #1: Mac OS system call mechanism. The trap word encodes the\n  selector; dispatch through the trap table.\n\n#### E.3 Boot sequence\n1. PPC nanokernel completes initialization\n2. NK hands off to DR emulator (trap table → emulator-start)\n3. 68K interpreter starts at 68K reset vector (ROM + 0x2A)\n4. 68K boot code initializes Toolbox, Memory Manager, Device Manager\n5. Toolbox loads the System file, Finder\n6. Finder draws the desktop\n7. User interacts via ADB (keyboard/mouse → UEFI → ADB codes → Finder)\n\n### Phase F: User Experience\n\n#### F.1 Configuration menu enhancements\n- [ ] ROM file browser: navigate ESP/HFS volumes to select ROM\n- [ ] System Folder browser: select boot volume\n- [ ] Display resolution selector\n- [ ] Memory size (128 MB – 2 GB)\n\n#### F.2 Real disk boot\n- [ ] Detect and boot from HFS-formatted physical disk (UEFI Block I/O)\n- [ ] Detect and boot from HFS disk image file on FAT partition\n- [ ] Both paths use the in-emulator HFS reader\n\n## Architecture Decisions\n\n### Heavy bootloader, not an application emulator\nUEFI standard protocols are the hardware abstraction; guest-visible Mac devices\nare thin simulated windows wired to GOP/BlockIO/SNP. This keeps the host-side\ncode small and lets the guest own the boot process.\n\n### Target architecture: PowerPC\nBetter fit for Mac OS 8/9 and for a user-supplied Old World ROM.\nReferences: SheepShaver, Basilisk II, QEMU, DingusPPC.\n\n### In-emulator HFS reader\nThe bootloader must read Mac discs without a host filesystem; catalog-ID lookup\navoids name/path separator ambiguity and survives all three test-disc layouts.\n\n### SheepShaver-style paravirtualization\nThe ROM is patched so all code runs in flat memory (no MMU, no BAT). Device I/O\nis intercepted through EMUL_OP trap dispatch. This avoids the need for hardware\nregister emulation at the register level — instead, Toolbox calls are redirected\nto host-side C implementations backed by UEFI protocols.\n"
}
```

**Output:**
```
Wrote file successfully.
```

---

## Assistant (Build · Big Pickle · 237.2s)

_Thinking:_

Now let me start implementing the most critical fix first: fixing the MSR[DR] issue and the PMDT mapping. Let me read the relevant code sections to understand the current state.

Now let me start implementing the critical fixes. First, the MSR[DR] fix and PMDT mapping:

**Tool: todowrite**

**Input:**
```json
{
  "todos": [
    {
      "content": "Analyze SheepShaver architecture and map EFIMac gaps",
      "priority": "high",
      "status": "completed"
    },
    {
      "content": "Update TODO.md with comprehensive roadmap to desktop boot",
      "priority": "high",
      "status": "completed"
    },
    {
      "content": "Fix nanokernel boot: PMDT mapping, MSR[DR], area creation",
      "priority": "high",
      "status": "in_progress"
    },
    {
      "content": "Expand 68K interpreter with missing opcodes for ROM execution",
      "priority": "high",
      "status": "pending"
    },
    {
      "content": "Implement core EMUL_OP handlers (timer, disk, ADB, interrupts)",
      "priority": "high",
      "status": "pending"
    },
    {
      "content": "Add Mac VIA/SCC device register emulation in guest memory",
      "priority": "medium",
      "status": "pending"
    },
    {
      "content": "Implement continuous68K execution loop (beyond single-step PPC hook)",
      "priority": "medium",
      "status": "pending"
    },
    {
      "content": "Implement Toolbox trap dispatch (A-line traps → EMUL_OP)",
      "priority": "medium",
      "status": "pending"
    },
    {
      "content": "Add boot volume mounting and Finder launch sequence",
      "priority": "medium",
      "status": "pending"
    },
    {
      "content": "Wire framebuffer, keyboard, mouse to UEFI protocols",
      "priority": "low",
      "status": "pending"
    }
  ]
}
```

**Output:**
```
[
  {
    "content": "Analyze SheepShaver architecture and map EFIMac gaps",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Update TODO.md with comprehensive roadmap to desktop boot",
    "status": "completed",
    "priority": "high"
  },
  {
    "content": "Fix nanokernel boot: PMDT mapping, MSR[DR], area creation",
    "status": "in_progress",
    "priority": "high"
  },
  {
    "content": "Expand 68K interpreter with missing opcodes for ROM execution",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Implement core EMUL_OP handlers (timer, disk, ADB, interrupts)",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Add Mac VIA/SCC device register emulation in guest memory",
    "status": "pending",
    "priority": "medium"
  },
  {
    "content": "Implement continuous68K execution loop (beyond single-step PPC hook)",
    "status": "pending",
    "priority": "medium"
  },
  {
    "content": "Implement Toolbox trap dispatch (A-line traps → EMUL_OP)",
    "status": "pending",
    "priority": "medium"
  },
  {
    "content": "Add boot volume mounting and Finder launch sequence",
    "status": "pending",
    "priority": "medium"
  },
  {
    "content": "Wire framebuffer, keyboard, mouse to UEFI protocols",
    "status": "pending",
    "priority": "low"
  }
]
```

---

## Assistant (Build · Big Pickle · 301.9s)

---

## User

great, thank you! now you can proceed 

---

## Assistant (Build · Big Pickle · 439.4s)

---


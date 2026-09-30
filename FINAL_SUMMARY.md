# EFIMac — Project Status Summary

**Terminology:** EFIMac is a **heavy UEFI bootloader** for classic Mac OS on
x86_64, not an emulator. It stages a PowerPC Mac boot environment from UEFI
standard protocols and then executes the installed firmware (and the OS it
boots) for real on a PowerPC interpreter.

## What Exists Today

- **PowerPC interpreter with a continuous fetch-execute loop**
  (`src/cpu/interpreter.c`): fixed 32-bit big-endian opcodes, GPR/SPR/FPU
  state, multi-region guest memory map with read-only ROM enforcement, a
  timebase/decrementer scheduler tick, and exception delivery through the
  firmware's own `VecTbl` (SPRG3). Includes binary instrumentation for boot
  chasing (milestone logs, capped probes, a 4096-entry PC/register tail ring).
- **UEFI hardware layer** (`src/platform`, `src/hardware`): GOP framebuffer,
  Block I/O / Simple File System, Simple Network Protocol, audio ring buffer;
  an HFS/HFS+ reader (`src/fs/hfs.c`) reads classic discs in place.
- **Firmware sourcing** (`src/boot`): priority-ordered ROM install — ESP Old
  World ROM path, ESP `Mac OS ROM`, HFS-discovered New World ROM (verified from
  a real Mac OS 9.2.2 install disc), demo ROM fallback.
- **System Folder / driver staging**: System, Finder, and up to 64 Extensions
  staged with 0 failures on System 7.5.3 / Mac OS 8.1 / Mac OS 9.2.2 discs.

## Boot State Reached (2026-09)

With a Mac OS 9.2.2 install disc, the `Mac OS ROM` (2,763,530 bytes) is
discovered, installed read-only, and executed continuously:

- NK init text ("Hello from the replacement multitasking NanoKernel"),
  coherence groups, address spaces, BAT setup, ready queues, blue task,
  timeslicing, idle task.
- "converting PMDTs to areas" — VMM zone carving via the free-list walker.
- Task creation with PPC/68K interleave (`RETTASK`, `DRYIELD` round-trips).
- Fix B lands: NK scheduler idle-frame queue-head mirror removes the SBIDLE
  deadlock; the scheduler loops cleanly.
- The **DR console read loop is live** at `0x40B2751C`; the SCC device is
  emulated at `0x20000` (status `0x20002`, data `0x20006`) and the boot console
  drains queued Rx during the banner flush.

Self-tests: PowerPC CPU 35/35 (incl. FPU), boot memory map, System Folder /
driver staging 7/7. Boots under QEMU + OVMF on Windows and macOS.

## Current Firewall (2026-09-30): SCC EXT wake dispatch

The boot stalls at the second-NK-init idle sell: the idle task gives up via
`sc` (`r0=0x2E`) at `0x40B24F04` and the scheduler never wakes it, because the
machine-level IRQ routing the guest needs (`[KDP-0x824]` interrupt-controller
base) is 0 at this phase — a bare `0x500` dispatch would no-op. Instead, the
emulator now fabricates the external interrupt **directly through the guest's
own level-9 dispatch**:

- **Raise site** `0x40B24F04` (idle give-up, EE off in the sell phase): the
  emulator injects an EXT before the `sc`, and the guest's D0 handler
  (`0x40B14880` → KCALLSAVE `0x40B13D40` → ISR `0x40B148E0`) runs for real.
- **Mirror tables** are seeded so the ISR dispatches: tabbase `0x6C00`
  (A-table `0x6C00`, P-table `0x6D00`, B/stack-table `0x6D20`, `B[0]=0xA000`),
  IC mirrors at `[IC+0x20]=3` (events) / `[IC+0x24]=1` / `[IC+0x38]=2` /
  `[IC+0x44]=0x3FFFFFFF` / `[IC+0x4C]=0x75C0`, and `[KDP-0x338]=0x81C0`.
- **Dispatch target** `P[0] = Next` = `0x40B24F08` — the preempted task's
  sell-glue continuation. Resumed GPRs stay live in the vCPU (PPC exceptions
  don't auto-save); the dispatch only rewrites r1/r2 from the stack table.
- **SRR1/MSR is forced at the dispatch rfi** (`0x40B14A04`, `SRR0==0x40B24F08`):
  the ISR's SRR1 source (`lwz r19,0xF69C(r1)` at `0x40B1499C`; displacement is
  signed `r1-0x964`) reads a slot that is **not** `[0x969C]` (`[0x969C]=0xD032`
  there, yet the load yields `0x1032`/`0x4BF620C4` — r1 at that PC is not
  `0xA000`), so on-memory seeding was discarded in favor of a direct forced
  `SRR1=0x9002` (EE on) on every fabricated dispatch.

Verified across 60/180/300 s soaks: the dispatch is a clean, repeatable,
non-preempting task switch — zero spinlocks, no GUEST STOP, no crash. The
give-up then re-sells (`VECDISP 0xC00`/`SCSEVRET r3=0` loop, DEC decrementing
~`0x1E0`/give-up). The 4 queued `'g' CR 'g' CR` bytes are consumed by the
console task **before** the raise (AUTORESUME x4, SCCPOLL x3).

**Remaining wall (unresolved):** resuming the idle give-up does *not* advance
boot. The resumed code (`0x40B24F04+`) is the idle task's NK-panic banner
("idle task René Alan Jim Alex Derrick…"); its wake path is a 1 s delay then
NKPANIC (`0x40B272EC`). The installed give-up stub (`li r3,0; rfi` at `0xBDFC`)
always returns r3=0, so the guest times out and re-sells forever. The missing
wake is not raw SCC input — it appears to be a different event (a DEC/1 s
timeout, the warm/CSR branch at `0x40B126E8`, or an unemulated KDP service).

Candidate next directions:
1. Trace the give-up's wake-event mask and the `0x40B126E8` warm/CSR branch to
   identify the exact event the "Resuming" tail blocks on, then deliver it.
2. Pivot the dispatch target to the console/shield task's real SCC dequeue
   (`0x40B26548` / `0x40B263E0`) with its saved GPR context restored.
3. Emulate real give-up wake semantics in the `0x2E` stub (block, check the
   pending-event mask, return woken/nonzero when an event is queued).

Larger roadmap afterwards: the machine-level interrupt controller + SCC Rx IRQ
model (reference: DingusPPC `grandcentral` / SheepShaver IRQ model), then the
UEFI <-> PPC interface layer, then MMU/translation, then validation across
New World 9.2.2 / Mac OS 8.1 / Old World.

## Source Map

```
src/
├── main.c                     # efi_main: subsystem init + guest handoff
├── cpu/interpreter.c          # PPC execute loop, register file, guest map,
│   │                          #   SCC, timebase/DEC, instrumentation
├── cpu/m68k.c                 # Legacy 68K layer (runtime-only fallback)
├── cpu/translation.{h,c}      # Exception delivery + shared helpers
├── memory/manager_impl.c      # Guest RAM + region mapping
├── hardware/abstraction_impl.c# GOP, Block I/O, SNP, audio ring
├── boot/bootloader_impl.c     # ROM install, HFS probe, System Folder staging
├── fs/hfs.c                   # HFS/HFS+ reader
├── utils/debug_impl.c         # Logging + timers
└── platform/uefi_interface_impl.c  # UEFI protocol discovery
```

See README.md, ARCHITECTURE.md, TODO.md, BUILD_INSTRUCTIONS.md, USER_GUIDE.md.
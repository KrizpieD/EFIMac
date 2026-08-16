# Session 9 — ECB/68K-context work (state preserved after environment wipe)

Date: 2026-08-15. Shell (bash tool) broken; temp dir `C:\Users\clayc\AppData\Local\Temp\opencode` was wiped mid-session (lost `boot_out.txt`, `macosrom_flat.bin`, `ss_src`). User chose: restart the session to restore the shell, then continue ECB setup.

## Objective (unchanged)
Boot System 7.5.3 via the New World ROM's built-in 68K DR emulator. Current decision: implement ECB + 68K emulator context setup (SheepShaver-glue style) so the emulator-start/thunk/trap-0xFF path resumes into the DR emulator instead of the NK idle loop.

## Confirmed facts (this session, from cebix/macemu SheepShaver source)
- Our `Common[27]` emulator-start thunk (bootloader_impl.c 0x36F900) is a verbatim copy of SheepShaver's rom_patches.cpp 0x36F900 routine. Correct as-is.
- `lwz r6,0x65c(r1)` in the thunk => `[KDP+0x65C]` = pointer to the 68K register-save block (ECB); the routine stores r7-r13 at `[ECB+0x13C, 0x144, 0x14C, 0x154, 0x15C, 0x164, 0x16C]`. The DR emulator reads its 68K regs from this block.
- `[KDP+0x658]` = Mac (logical) address pointer used by host interrupt path: `cpu_emulation.cpp` sigusr2_handler does `WriteMacInt32([KDP+0x658] + 0xDC, [KDP+0x658] + 0xDC | [KDP+0x674])` (MODE_NATIVE). Also writes `WriteMacInt16([KDP+0x67C], 1)`. `[KDP+0x674]` = CR bitmask, `[KDP+0x67C]` = pending-byte addr.
- Five NKCallTable slots read by the trampolines: `[KDP+0x5F0, 0x5F4, 0x5F8, 0x5FC, 0x604]` (trap 0 emulator-start, Mixed Mode, Reset/FC1E, FE0A, FE0F). Trap 4 (interrupt) is ILLEGAL in both SheepShaver and our patch.
- `[KDP+0x634]` = pointer to Emulator Data, `[KDP+0x1184]` = pointer to emulator init routine, `[KDP+0x119C]` = pointer to opcode table (used by SheepShaver's "Jump to 68k emulator" patch: `lwz r3,0x634(r1); lwz r4,0x119c(r1); lwz r0,0x1184(r1); mtctr r0; bctr`).
- IMPORTANT: SheepShaver's rom_patches.cpp NEVER writes [KDP+0x658]/[KDP+0x65C]/[KDP+0x634]. The nanokernel ROM boot populates them on real hardware. Our ROM boot never runs the NK emulator-entry setup (0x40B107F0-0x40B10840, 0x40B10834 probe never fired), which is why [KDP+0x658]=0, [KDP+0x65C]=0 and the guest idles at 0x40B24F48.
- KernelData struct (BasiliskII/src/include/main.h): KDP = word array view; M68kRegisters = { uint32 d[8]; uint32 a[8]; uint16 sr; }.

## Guest state recap
- Boot passes the handoff (DEC gating fixed last session); guest spins in NK idle task 0x40B24F48-0x40B25000: register-rotation delay chain, r31=0x989680 countdown, `li r3,0xc; li r4,1; li r0,0x2e; sc` syscall (returns -0x7266: dispatch table 0xACB8 all zeros), `twui r31,5` (traps at 0 when r31 hits 0 -> 0x700 -> 0x40B14850).
- Flow: BOOTTAIL 0x40B126F0 (r3=0xFF, r4=0x40B6E8C0) -> EMULWIN 0x40B6E8C0 -> EMUSTART thunk 0x40B6F900 -> [KDP+0x5F0]=0x40B13BF8 (NKCallTable[0]) -> r3=0xFF>1 -> 0x40B14A38 "resume 68K" (restores regs from SRR0-relative block, `mtlr r8=[SRR0-0x2B8]; blr`) -> empty context -> falls to idle 0x40B24F04.
- Original trap table 0x36E8C0 = `0x0FFF00nn` words (PPC twui / 68K ori.b markers); we replace with `b` -> runtime-installed thunks at 0x36F900+ (ROM file has NOPs there; bootloader RomWriteEmulStartRoutine, bootloader_impl.c ~1744-1778, calls 1866-1870).

## Environment / restore steps (after session restart)
1. Verify shell works (`bash -c "echo ok"`).
2. Regenerate `C:\Users\clayc\AppData\Local\Temp\opencode\macosrom_flat.bin` from `mac_roms\Old_World_Mac_Roms.zip` (extract 4MB New World ROM, prep script used previously).
3. Re-run a QEMU boot to regenerate `boot_out.txt` (probes already in interpreter.c).
4. Re-disassemble 0x40B107F0-0x40B10840 (NK emulator-entry/ECB setup) and 0x40B14A38 + 0x40B13BF8 (resume/restore path) with capstone from the flat ROM to pin the exact context-block layout.
5. Implement ECB + context setup (see plan below); rebuild with scripts/build-windows.sh; re-run run-qemu-windows.ps1; iterate.
Commands: build: `& "C:\Program Files\Git\bin\bash.exe" -lc "bash scripts/build-windows.sh" 2>&1 | Select-Object -Last 1`. Run: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run-qemu-windows.ps1 -MacDisc "...\mac_discs\System7_5_3.img" -Seconds 25 2>&1 | Select-Object -Last 1`; log at `C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt`.

## Plan for ECB + 68K context setup
1. Allocate a persistent ECB buffer in low memory (safe region; NK KDP is at 0xA000, low-mem globals to 0x1800; candidate: an area the NK doesn't touch, confirm against disassembly).
2. Seed KDP fields: `[KDP+0x654]=PA_ECB`, `[KDP+0x658]=LA_ECB`, `[KDP+0x65C]=LA_ECB`, and set `[ECB+0x1CC]&7==7` so the 0x900 handler dispatches to the 68K.
3. Initialize the 68K context block restored by 0x40B14A38 so its final `blr` (to `[SRR0-0x2B8]`) enters the DR emulator loop; 68K PC = 0x40B6E8C0 (`ori.b #0xFF,D0` reset marker) so the first EMUL_OP/PpcEmulOp fires. Confirm exact block offsets from disassembly before writing.
4. Verify whether the syscall 0x2E table at 0xACB8 also needs populating for the idle loop to move on.
Note: SheepShaver never seeds these fields itself - the ROM boot does. Prefer replicating the ROM's own setup (0x40B107F0) rather than inventing new semantics.

## Relevant files
- src/boot/bootloader_impl.c: RomWriteEmulStartRoutine (~1744-1778), trap rewrite (1846-1870), ConfigInfo LA patches incl. 68K reset vector (1812-1824), XLM globals (1891-1901).
- src/main.c: KDP seeding via SPRG4 caller struct (0x30648 seed, ~729-753); MSR=ME|RI|DR (768).
- src/cpu/interpreter.c: DEC gating (2987/3498/3915), probe suite (4166+), PpcEmulOp/EMUL_OP, PPC_TIMEBASE_SCALE=50.
- src/cpu/translation_impl.c: PpcHandleException VECDISP (~576).
- SheepShaver ref (fetched this session, tool-output): SheepShaver/src/rom_patches.cpp, cpu_emulation.cpp (sigusr2_handler), BasiliskII/src/include/main.h (M68kRegisters).

# SUBSYSTEM_CONTRACTS — Milestone exit criteria

Each contract is a one-slice boundary. A task may modify at most one subsystem and
must name one gate ID (`C0`..`C4`, or active sub-gate `C2A`/`C2B`) before editing.
Gate commands use `scripts/bringup-gate.ps1`; log assertions are authoritative.

## C0 — Bootloader staging and self-tests

**Preconditions**
- Build: `& "C:\Program Files\Git\bin\bash.exe" -lc 'cd /c/Users/clayc/Desktop/EFIMac && bash scripts/build-windows.sh 2>&1 | tail -8'`.
- OVMF under `$env:TEMP\opencode\ovmf\usr\share\OVMF\`; ESP writable; stage
  `%TEMP%\opencode\mac_disc\Mac_OS_9.2.2.iso`; remove `mac_disc\C__`.
- Run helper: `scripts/run-qemu-windows.ps1` with `-NoReboot -StallSeconds 0`.

**Postconditions / Pass-Fail Gate**
```powershell
& .\scripts\bringup-gate.ps1 -Contract C0 -Seconds 45 `
  -Require @('System ROM installed:','Boot state: ready=1','self-test complete') `
  -Forbid @('[FAIL]','GUEST STOP','budget stop')
```
Pass requires: `System file resolved`, `PASS System file data read back`,
`System ROM installed: 4194304 bytes`, `Boot state: ready=1`, all self-test
blocks with `0 failed`. No `[FAIL]`, `GUEST STOP`, or `budget stop`.

**Explicit Non-Goals**
- Guest OS boot, ROM execution correctness, Toolbox, graphics, full disk I/O.

## C1 — Nanokernel handoff

**Preconditions**
- C0 passes. Entry invariants from `STATE_LEDGER.md` VI-B1..VI-B4:
  `PC=r3=0x40B10000`, `r4=0`, `r5=0x20000`, `SPRG4=0x30000`, `MSR=0x1012`,
  `[0x30648]=0x40B6E8C0`.
- Selector-2 trap slot `0x40B6E8C8` must still use the return-to-LR bypass with
  `MSR=Srr1` (VI-N0..VI-N4).

**Postconditions / Pass-Fail Gate**
```powershell
& .\scripts\bringup-gate.ps1 -Contract C1 -Seconds 45 `
  -Require @('Hello from the replacement multitasking NanoKernel','Nanokernel replace','TRAP2 entry') `
  -Forbid @('CPU MALFUNCT','GUEST STOP','budget stop')
```
Pass requires: NK banner, `Nanokernel replace`, at least one `TRAP2 entry`, and
no `MALFUNCT`/crash. External SRR1 cause bit `0x00010000`; trap cause
`0x00020000` (trace assertion when diagnostics are enabled).

**Explicit Non-Goals**
- Full task dispatch model, PMDT completion, desktop, graphics, real KCall 2.

## C2 — 68K park / dispatch / debugger unlock

**Preconditions**
- C1 passes. `AUTORESUME` cap 3 and `PARKWAKE` cap 8 active (VI-S1, VI-X0).
- SCC stub contract VI-S0 holds: `0x20002` bit0=Rx-ready, bit2=Tx-empty;
  `0x20006`/`0x20008` share one FIFO.
- 68K park word `0x408047AE = 0x60FE` remains the known park; do not NOP it.

**Postconditions / Pass-Fail Gate**
```powershell
& .\scripts\bringup-gate.ps1 -Contract C2 -Seconds 45 `
  -Require @('DSPDUMP[1]','Nanokernel replace','AUTORESUME[1]','VECDISP[3]') `
  -Forbid @('CPU MALFUNCT','GUEST STOP','budget stop')
```
Pass requires: one `DSPDUMP[1]`, `Nanokernel replace`, `AUTORESUME[1]`, and
external handler `0x40B14880` reached (`VECDISP[3]` or equivalent). A complete
`BGATE-DSP v1` and `BGATE-SCC v1` dump must be present before declaring the
dispatch model understood.

**Explicit Non-Goals**
- Ready-queue implementation, second full PMDT pass, `Priming`, Finder output.

### C2A — Current read-only map/contract lock — LOCKED 2026-11-23

**Preconditions**
- No code edits. Read-only RE of `0x40B148E0`, `0x40B1490C`, `dsp[9]=0x6D00`,
  `stk[0]=0xA000`, and `Session_28_Transcript.md` §12.
- No QEMU run longer than 45 s was used; the cold boot had not reached
  `DSPDUMP[1]` by 45 s, so the lock is based on static ROM disassembly plus the
  existing live dump.

**Locked dispatch invariant**
- `r23 = SPRG0 = KDP = 0x0000A000`.
- Index selection at `0x40B148F8`: `r20 = (r8==0 ? r3 : [KDP-0x238]) << 2`.
  The live dump has `r8!=0`, `[KDP-0x238]=9`, `r20=0x24`; `r3=0` is expected.
- Table base: `r22=[KDP-0x338]=0x000081C0`, `dsp=[r22+0x3C]`, `stk=[r22+0x40]`.
- Guards: `[r22+0x38] != 0`; `r20 < ([r22+0x44]<<2)`.
- Resume: `ent=dsp[r20]`, `SRR0=[ent+0]`, `r2=[ent+4]`,
  `r1=stk[[KDP-0x116]<<2]`, `r3=[KDP-0x116]`,
  `SRR1=[KDP-0x10]|0x8000`.
- Live index-9 snapshot: `dsp[9]=0x00006D00`, `ent+0=0x40B11A90`, `stk[9]=0`.
- `rfi` site is `0x40B14A04` for `r8!=0`, `0x40B14A1C` for `r8==0`.
- `0xB000+I*0x20` is DR emulator gd/PC-relative data, not the NK ready queue.

**Postconditions / Pass-Fail Gate**
- H-D1, H-D2, H-D3 are RESOLVED in `STATE_LEDGER.md` with regression checks.
- C2B below contains the exact index register, table base, and expected `SRR0`.

**Explicit Non-Goals**
- Implementing the bypass/model, changing AUTORESUME/PARKWAKE caps, new probes.

### C2B — Scoped implementation gate (LOCKED)

**Status:** PASS. Final clean-build 240 s gate observed `Nanokernel replace` at
t+24s, `DSPDUMP[1]` at t+72s, `Priming the system free list` at t+95s, with no
forbidden markers. Locked change retains the guest-store `PUTS` probe and its
priming-entry reset; resolved trace instrumentation was removed.

**Preconditions**
- C2A is LOCKED. Modify exactly one of: dispatcher table seed for index `9`,
  resume PC selection at `0x40B149B0..0x40B149BC`, or SCC-ready shim. One
  hypothesis only (`H-C1`..`H-C9`, or `H-C36` after its `STATE_LEDGER.md` entry).
- For `H-C36`, the single scoped change was the SCC control register-pointer/RR1
  model in `src\cpu\interpreter.c`; it was falsified and reverted.
- For `H-C37`, the single scoped change was a bounded `SCCDATA` probe in the
  `0x20006/0x20008` data-read branch; it resolved the consumer and was reverted.
- For `H-C38`, the single scoped change was a bounded `POSTPRINT` PC trace in
  `src\cpu\interpreter.c`; it resolved the non-returning helper and is retained
  only as the H-C39 verification probe.
- For `H-C39`, the scoped SCC control-pointer/RR1 model was falsified and fully
  reverted; `POSTPRINT` remained identical.
- For `H-C40`, the bounded `HELPER` PC trace showed early calls returning
  normally but exhausted its cap before the failing menu-print call.
- For `H-C41`, the gated `HELPER` trace localized the stall to the nested
  `bl 0x40B272E0` at menu-helper PC `0x40B265C4`.
- For `H-C42`, the extended trace falsified the lock hypothesis and identified
  recursive debugger entry through the zero `-0xaf0(r1)` reentrancy flag.
- For `H-C43`, seeding `[r1-0xAF0]` was falsified: that word is the
  `0x40B12700` lock, not a separate flag; fully reverted.
- For `H-C44`, the bounded write watch showed the expected acquire/release
  writers only, but filled its cap before the failing menu call.
- For `H-C45`, the armed trace proved `reent=0`, `cr1eq=1` at `0x40B265BC`.
- For `H-C46`, clearing CR1 EQ at `0x40B265BC` made the menu print helper return
  to `0x40B27574` but the cap-8 fix exhausted and recursion resumed.
- For `H-C47`, cap 256 let execution reach parser entry `0x40B2758C`, but the
  path loops with the SCC FIFO unchanged.
- For `H-C48`, the parser-caller data-read probe showed zero data reads, so
  `GetChar` exits before touching the data register.
- For `H-C49`, zero status polls proved `GetChar` exits on `r28==0` at
  `0x40B268A8` before touching the SCC stub.
- For `H-C50`, the LR-gated base restore never fired and is reverted.
- For `H-C51`, the base slot is valid (`0x9700=0x20000`); the earlier zero-base
  inference was wrong because LR changes at `0x40B268C8` before polling.
- For `H-C52`, parser GetChar consumes both `g` and CR but no command dispatch
  follows; the post-read parser path needs tracing.
- For `H-C53`, the expanded trace filled before input consumption and was
  inconclusive.
- For `H-C54`, CR-armed selected-PC tracing still missed the GetChar return path.
- For `H-C55`, the CR return path re-enters the debugger because it uses LR
  `0x40B268C8`, which H-C46 did not cover.
- For `H-C56`, parser dispatch is reached but the command buffer stays zeroed.
- For `H-C57`, the broad write watch filled before the parser stores.
- For `H-C58`, parser stores CR then NUL but never the typed character.
- For `H-C59`, CR0 EQ is incorrectly clear at the CR branch.
- For `H-C61`, compare and CR are correct; the missing `g` store needs tracing.
- For `H-C62`, the parser's own GetChar never received `0x67`, only CR.
- For `H-C63`, parser feeds reach the SCC data read but not the command buffer.
- For `H-C64`, the guest command line is empty at dispatch despite SCC reads.
- For `H-C65`, the buffer now contains `g\0` but no resume follows.
- For `H-C66`, the handler returns the nonzero command char `0x67`.
- For `H-C67`, the `g` handler is reached but does not emit `Resuming at`.
- For `H-C68`, the resume/print path calls the shared helper with new LRs.
- For `H-C69`, the extension was ineffective and reverted.
- For `H-C70`, printf at `0x40B27A4C` never returns.
- For `H-C71`, no shared-helper recursion occurs in the printf path.
- For `H-C72`, early hex-print callers filled the trace cap.
- For `H-C73`, the resume LR arms the hex-print trace correctly.
- For `H-C74`, the resume helper returns to the menu loop instead of `0x40B27A50`.
- For `H-C75`, the epilogue recursion uses LR `0x40B26848`.
- For `H-C76`, the resume path completes but resumes into the debugger helper.
- For `H-C77`, saved SRR0 is `0x40B11A90` while the resume slot is corrupted.
- For `H-C78`, the resume slot is repaired but a nested debugger entry loops.
- For `H-C79`, skipping the nested debugger entry zeroed context and caused GUEST STOP.
- For `H-C80`, LR `0x40B27A94` does not reach the shared epilogue.
- For `H-C81`, the nested entry saves context then diverts into the menu.
- For `H-C82`, direct return still loses kernel context and is reverted.
- For `H-C83`, the resume frame at `r1` is not the frame used; restore
  dereferences `[r1-4]`.
- For `H-C84`, the dereferenced restore frame is the same zeroed frame.
- For `H-C85`, `LR==0x40B27578` is not a debugger entry.
- For `H-C86`, `0x40B272E0` is debugger-internal, not the kernel save entry.
- For `H-C87`, early SPRG1 frames are zeroed and the cap hides kernel entry.
- For `H-C88`, kernel entry context is valid but later clobbered.
- For `H-C89`, the restore frame is repaired but the resume tail still loops.
- For `H-C90`, memory-slot repair is insufficient; the guest tail rewrites it.
- For `H-C91`, direct branch fails on unrepaired segment-register state.
- For `H-C92`, SR restore is insufficient; MSR IR/DR re-enable breaks fetch.
- For `H-C93`, MSR masking does not repair the fetch.
- For `H-C94`, the suspect ROM word is already wrong at fetch time.
- For `H-C95`, a guest store at `0x40B11AB4` corrupts executable ROM.
- For `H-C96`, ROM write protection stops the fatal opcode corruption.
- For `H-C97`, the resumed kernel path exits the debugger cleanly.
- For `H-C98`, the resumed kernel loops in the copy loop.
- For `H-C99`, the resumed kernel exits the copy loop and reaches the
  `NKCreateAddressSpaceSub` debugger-log call.
- For `H-C100`, late kernel debugger calls are distinguishable by LR.
- For `H-C101`, resumes return to the latest kernel debugger caller.
- For `H-C102`, the first debugger stop is the priming log call.
- For `H-C103`, the first resume reaches the priming log caller.
- For `H-C104`, the priming string is readable.
- For `H-C105`, the initial print loop runs but reads wrong byte values.
- For `H-C106`, the load instruction and source byte are correct.
- For `H-C107`, the load trace filled before the priming print.
- For `H-C108`, the priming byte load is correct; `r29` is clobbered by the
  guest string-append helper.
- For `H-C109`, the guest store is sampled, but the probe cap fills early.
- For `H-C110`, the guest-emitted priming string is captured; C2B 120 s and
  240 s gates PASS.
- For `H-C111`, the single scoped change removes resolved trace instrumentation;
  all other subsystems unchanged.
- No changes to `AUTORESUME`, ROM patch set, or 68K engine in the same task.
  For H-C11 only, the scoped change is the `ParkWakeCount` ceiling in
  `src\cpu\interpreter.c`; no other `PARKWAKE` behavior may change.
- Expected locked snapshot: `r20=0x24`, `r22=0x000081C0`, `dsp=0x00006C00`,
  `ent=dsp[9]=0x00006D00`, expected `SRR0=[ent+0]=0x40B11A90`.
- Any bypass must either preserve that `SRR0` and repair the surrounding task
  state, or replace it with a named continuation recorded in `STATE_LEDGER.md`.
  Do not hard-code a different PC without an invariant.

**Postconditions / Pass-Fail Gate**
```powershell
& .\scripts\bringup-gate.ps1 -Contract C2B -Seconds 240 `
  -StopPattern @('Priming the system free list') `
  -Require @('DSPDUMP[1]','Nanokernel replace','Priming the system free list') `
  -Forbid @('CPU MALFUNCT','GUEST STOP','budget stop','NULLBRANCH[0]: 1')
```
Pass requires `Priming the system free list` observed at least once, no
`MALFUNCT`, no `GUEST STOP`, no `NULLBRANCH[0]: 1`. The 45 s and 120 s budgets
are insufficient: baseline first reaches `DSPDUMP[1]` at 86-103 s, and the
post-resume init path advances slowly after that. This is a targeted validation
of the H-C9 budget hypothesis with a stop on the target marker, not a blind soak.

**Explicit Non-Goals**
- Full desktop, graphics, broad diagnostic cleanup, multi-minute blind soaks.

## C3 — Toolbox / Glue vertical slice

**Active hypothesis:** none (`H-C132` RESOLVED). `H-C132` set CR bit 9 at
`0x40B123A4` only after a kernel-target resume repair, taking
`beq cr5,0x40B1255C` and producing 3 `Converting PMDTs to areas` records
(2 after `Priming`) plus 5 `CreateArea` records in the named C3 240 s gate,
with no unclassified `UNBOUNDCALL`. Bounded diagnostics were removed after
PASS; remaining state corrections are the PMDT counter mask at `0x40B12244`
and CR5 EQ repair at `0x40B123A4`. Final diagnostic-free build re-verified C3
(4 PMDT records, 3 after `Priming`; no forbidden markers) and the C2B locked
regression (`t+160s Priming`). Post-C4 H-C140 regressions re-verified C3
(`records 7042`) and C2B (`records 6226`) with no forbidden markers.

**Preconditions**
- C2 or C2B passes; dispatcher/stack table semantics locked in `STATE_LEDGER.md`.
- At least one Toolbox/Glue import boundary identified from `UNBOUNDCALL <name>`
  or LINE-A/LINE-F stub traces.

**Postconditions / Pass-Fail Gate**
```powershell
& .\scripts\bringup-gate.ps1 -Contract C3 -Seconds 240 `
  -Require @('Priming the system free list','Converting PMDTs to areas','CreateArea') `
  -Forbid @('GUEST STOP','CPU MALFUNCT','budget stop','UNBOUNDCALL <unhandled>')
```
Pass requires the guest-emitted `Converting PMDTs to areas` marker at least
twice (counted from the log after the gate), at least one `CreateArea`, and no
unclassified `UNBOUNDCALL`. Every remaining `UNBOUNDCALL` must be entered in
`STATE_LEDGER.md` as a stub or a tracked contract.

**Explicit Non-Goals**
- Final Finder UI, audio, network, full Toolbox coverage.

## C4 — First sign-of-life output

**Status:** PASS via `H-C140` (immediate post-store blit). `H-C139` proved a
real guest PPC stub can write `0x18000000` (`FBWRITE[0..3]`), and `H-C140`
added an immediate blit after restoring guest state. The named C4 240 s gate
passed with all required markers and no forbidden markers. A 480 s
post-gate validation also passed (`records 7106`, same markers, no forbidden
markers). Active hypothesis: none. This is a bounded guest-executed framebuffer
store, not QD rendering; QD-driven drawing remains a separate future contract.

**Preconditions**
- C3 passes. Graphics/serial output path initialized; no known fatal markers.

**Postconditions / Pass-Fail Gate**
```powershell
& .\scripts\bringup-gate.ps1 -Contract C4 -Seconds 240 `
  -Require @('Priming the system free list','NatQdInstallScreen','PpcGraphicsBlitToDisplay','FBWRITE[0]') `
  -Forbid @('[FAIL]','GUEST STOP','budget stop')
```
Pass requires real post-`Priming` `NatQdInstallScreen`,
`PpcGraphicsBlitToDisplay`, and `FBWRITE[0]` evidence and stable serial output
through the gate. Wall-clock validation beyond 240 s is allowed only after this
gate passes.

**Explicit Non-Goals**
- Full Mac OS feature completeness, every Finder window, sound, or network.

## C5 — QD-driven drawing

**Status:** C5 CONTRACT PASS at H-C174 (final official 240 s gate). C4 is locked by H-C140 plus a 480 s validation, but its
framebuffer writes come from the bounded guest PPC stub at `0x0000FC00`.
H-C141 falsified probe un-gating alone: with the stub disabled, no QD render
probe, framebuffer write, or blit appeared. H-C142 traced the post-screen
`Pc24` windows and found no PATH2/native-handoff offer; H-C143 showed low
`r24` values are transient inside `PpcExecuteInstruction`; H-C144 latched
handoff and completed PEF loading but still produced no QD output. H-C145 was
inconclusive and reverted after two runs failed before handoff; C4 was
revalidated after the revert. H-C146 reproduced the isolated H-C144 latch and
PEF load deterministically, but QD output is still absent. H-C147 dumped a
post-handoff fork-window exit but its ring data was unreliable. H-C148 seeded
LR to `0x40BA7680` and reached program-fragment main without the debugger
poll loop, but still produced no QD output. H-C149 dumped the native-main
entry; H-C150 decoded the `0x0055EAEC..0x0055EB04` list walk and confirmed it
terminates through TOC `-0x210` (`0x4090A638`). H-C151 Phase A mapped that
thunk's first `CallUniversalProc` to zero UPP `[0x58C]` and identified
`0x4080D99C` as the flat-ROM 68K routine for that vector. H-C151 falsified
installing it during `PpcInstallLowMemory`; H-C152 moved the write into the
low-PC handoff block, made that CUP nonzero, and produced real guest/CUP68K
framebuffer writes plus one blit with `C4FBSTUB[0]` absent, but no QDRENDER
probe and no full-C5 pass. User decision: keep the strict `QDRENDER` gate.
H-C153 confirmed `[0xFB8]` is still zero at the second CUP and mapped the flat-ROM
thunks that consume it. H-C154 falsified the observed-spin candidate `0x4085DE20`; the corrected mapping
is low global `0xFB8 -> ABB8 -> flat-ROM trap-table routine 0x40800DB8`. Active
H-C155B and H-C156 reached seeded UPPs but produced no QDRENDER; speculative
`[0xFB8]`/`[0x11F8]` writes are being reverted to the H-C152 state. Active
H-C157B/C found all three QD probe PCs zero in the native-memory view both at handoff
and at the first CUP hook. User chose the PC-domain path. H-C158B found the QD PCs zero in the `CpuRead32` fetch domain too. H-C158C found zero executed PCs below `0x00040000`. H-C158D found zero PPC PCs in the program-fragment range either. H-C171 PASS: targeted and official 240 s C5 gates emitted `QDRENDER`,
`FBWRITE[0]`, and `PpcGraphicsBlitToDisplay` with no forbidden markers. H-C172
cleanup removed timing-visible `QDBRANCH`/`QDSTEER` prints and lost `QDRENDER`; active
H-C173 restores those two prints while keeping `QDCODE`/`LOWPCHIST`/`QDDISP` removed,
then revalidates the official C5 gate. H-C173 still failed: `QDBRANCH` fired with
`flag=1` and the expected rect, but `QDRENDER` stayed absent. Active H-C174 restores
`LOWPCHIST` to test whether low-PC trace timing is required between branch setup and
`0x1F640`. H-C174 PASS: targeted 195 s gate emitted `QDRENDER`, `FBWRITE[0]`, and
`PpcGraphicsBlitToDisplay` with no forbidden markers; `LOWPCHIST` is timing-critical
and must stay. Official 240 s H-C174 gate also PASS: `Priming` t+161 s,
`NatQdInstallScreen` t+162 s, `FBWRITE[0]`/`QDRENDER` t+174 s,
`PpcGraphicsBlitToDisplay` t+179 s, no forbidden markers. C5 is closed on that
build. The C5 postcondition remains: a post-C4
boot emits `QDRENDER` and `FBWRITE[0]` with `C4FBSTUB[0]` absent, then
`PpcGraphicsBlitToDisplay`.

**Preconditions**
- C4 gate and 480 s validation pass; C3/C2B regressions pass.
- The virtual QD screen remains installed at `0x18000000`.
- The C4 synthetic store is disabled or made ineligible before the C5 gate.

**Postconditions / Pass-Fail Gate**
```powershell
& .\scripts\bringup-gate.ps1 -Contract C5 -Seconds 240 `
  -Require @('Priming the system free list','NatQdInstallScreen','QDRENDER','FBWRITE[0]','PpcGraphicsBlitToDisplay') `
  -Forbid @('[FAIL]','GUEST STOP','budget stop','C4FBSTUB[0]')
```
Pass requires at least one QD render probe hit, a framebuffer write while the
C4 stub is absent, and a GOP blit, with no forbidden markers. Wall-clock
validation beyond 240 s is allowed only after this gate passes.

**Explicit Non-Goals**
- Full desktop/Finder completion, sound, network, and general Toolbox coverage.
- Treating guest-executed synthetic stores as QD rendering.

# Session 28 — DR handoff, checksum skip, park wake, PMDT conversion reached

## Starting state

Stable 70 s run with selector-2 trap returning to LR, but 68K boot stalled in
`0x4080064E..0x40800662` checksum/relocation loop with `A3=0xFFFFFFE0`.

## Experiments and results

### 1. A-register masking (reverted)

- Forced all r16..r23 through `&= 0x00FFFFFFu` in `PpcExecuteInstruction`.
- Unconditional version: `GUEST STOP` after 19 s, `DRBOOT-RESUME: 0`.
- Gated version (`r24` in ROM range): same early failure.
- Conclusion: DR uses A registers as 32-bit PPC working registers outside the
  68K park; global masking is invalid. Reverted.

### 2. ROM patch: NOP checksum helper call (kept as experiment)

- Guest `0x4080011A`: `61 00 05 08` = `bsr.w 0x40800624`.
- Patched to `4E 71 4E 71` (two NOPs) in `bootloader_impl.c`.
- 70 s: full run, `GUEST STOP: 0`, `DWIN: 42`, `DRBOOT-RESUME: 1`.
- Stall moved to `0x40807BD0` copy loop; after 120 s moved again to the known
  `0x408047AE` park.
- Conclusion: skipping that one checksum pass advances boot without fatal.

### 3. Park wake injection (kept as experiment)

- Native DR never wakes from the `BRA$` at `0x408047AE`.
- Added in `PpcExecuteInstruction`: when `r24 == 0x408047AE` and
  `Seq % 50000 == 0`, set `ExceptionPending = PPC_EXCEPTION_DECREMENTER`.
- First injection at `seq=750000`, `MSR=0x00025030`.
- Guest progressed to `GOPROBE: 2` but crashed at `PC=0x00010001`.

### 4. SRR0 crash root cause and fix (kept as experiment)

- Broadened `SRR0BAD` log threshold from `<0x00010000` to `<0x00020000`.
- Found `SRR0BAD[1] mtspr SRR0=0x00010001 from r18 @PC=0x40B149BC
  LR=0x40B6E8C0`.
- Disassembly at `0x40B149BC`: task switch loads `r18 = [r20]` where
  `r20 = [[r22+0x3C]+r20]`; `TASKDISP r22=0x000081C0`,
  `[r22+0x3C]=0x00006C00`, `r20=0x14`.
- Added one-shot remap `0x00010001 -> 0x40B12DF0` (the GOPROBE target) at the
  SRR0 write.
- 120 s run: full duration, `GUEST STOP: 0`, `DWIN: 48`, `DRBOOT-RESUME: 1`,
  `GOPROBE: 2`, `PASS: 145`, `FAIL: 0`.

### 5. New frontier / correction

- Log showed repeated `verting PMDTs to areas`, `CreateArea ... placed ...
  created`, and `Nanokernel replace`, then `*** CPU MALFUNCT` without fatal.
- Correction: PMDT entries 0–3 at `0xA000` are correct (`[0xFFF7,9]`,
  `[0,0x0FF0]`, terminators). The later `page=0x0000 count=0xA000
  type=0x20000000` lines are the PMDT pointer array at `0xA078+` being misread
  as 8-byte entries, not corrupt PMDT data.
- Metrics script still reports `converting PMDTs: 0` because the serial string
  is split (`verting PMDTs to areas`).

## Build/run commands

```powershell
Remove-Item -Force "C:\Users\clayc\Desktop\EFIMac\build\src\cpu\interpreter.obj","C:\Users\clayc\Desktop\EFIMac\build\src\boot\bootloader_impl.obj"
& "C:\Program Files\Git\bin\bash.exe" -lc 'cd /c/Users/clayc/Desktop/EFIMac && bash scripts/build-windows.sh'
Remove-Item -Recurse -Force "$env:TEMP\opencode\mac_disc\C__" -ErrorAction SilentlyContinue
& "C:\Users\clayc\Desktop\EFIMac\scripts\run-qemu-windows.ps1" -MacDisc "$env:TEMP\opencode\mac_disc\Mac_OS_9.2.2.iso" -Seconds 120 -StallSeconds 0 -NoReboot
python3 C:\Users\clayc\AppData\Local\Temp\opencode\metrics.py
```

### 6. One-shot park wake + context-aware SRR0 fix (implemented)

- `PARKWAKE` is now global one-shot: one DEC injection only; the repeated
  50k-instruction re-injection that caused 4× phase repeats was removed.
- Replaced hard-coded `SRR0FIX 0x00010001 -> 0x40B12DF0` with `SRR0CTX` at
  `0x40B149BC`:
  - Diagnostic showed `tcb=0x00000000`, `[tcb]=0x00010001`,
    `lr=0x40B6E8C0`, `target=[r1+0x904]=0x40B12DF0`.
  - Resume PC is now derived from `[r1+0x904]` (the GOPROBE target); low
    memory word 0 is repaired with that value, with `0x40B12DF0` fallback.
- 120 s verification: full duration, `GUEST STOP: 0`, one `PARKWAKE`, one
  `SRR0CTX`, `GOPROBE: 2`, one `Nanokernel replace`, no fatal.
- With one-shot only the guest parks again after the first wake; no PMDT
  conversion phase in that run.

### 7. Bounded park wakes / MALFUNCT root cause

- Raised park wake cap to 8; only 2 wakes actually occur in 180 s. Result is
  stable: 4 `PMDTs to areas`, 12 `CreateArea`, 4 `Nanokernel replace`, no
  `GUEST STOP`, no `Priming`.
- Disassembled DEC handler `0x40B13200`: `0x40B13AB0` is the `*** CPU
  MALFUNCT - Masked interrupt punched through` path; it is taken when SRR1[EE]
  (`0x8000`) is clear at exception entry. Observed park-time MSR is
  `0x00025030` / `0x00003032`, both EE-off.
- Experiments that did **not** help:
  - Setting `[KDP+0x5A0]` before injection: MALFUNCT persisted (CR0 from
    `rlwinm. r9,r11,0,0x10,0x10` tests SRR1[EE], not `[r1+0x5A0]`).
  - Forcing SRR1[EE] in `PpcHandleException`: MALFUNCT disappeared, but boot
    stalled at `0x40BB07F0` (`lhau r27,-2(r24)`) with no NanoKernel replace.
  - Injecting external interrupt instead of DEC: still reached MALFUNCT.
- Reverted all three experiments; current code is back to bounded DEC wake
  injection plus the context-derived SRR0 repair. `VECDISP` now also prints
  SRR1 for future EE tracing.

### 8. EE-loss root cause found

- Added `PpcRecordMsrWrite()` instrumentation to `rfi`, `mtmsr`,
  `PpcSetRegisterValue(PPC_REG_MSR)`, and exception entry in
  `src/cpu/translation_impl.c`.
- The last EE-clearing write before park is consistently:
  `eeclr[3] k3 @0x40B6E8C8 0x0002D032->0x00025030`.
- `0x40B6E8C8` is the selector-2 `twi r31,2` trap. In the trap-2 special case
  in `PpcHandleException` we mask MSR (`&= ~(PPC_MSR_EE|PPC_MSR_RI)`) and then
  return directly to LR without an `rfi`, so the pre-trap EE is never restored.
- Restoring `g_PpcContext.Msr = g_PpcContext.Srr1` in that special case makes
  park-time MSR `0x0002D032` (EE on) and removes `MALFUNCT`, but boot then
  stalls in the 68K emulation dispatch loop at `0x40B67EC0..0x40B67ED4`
  (`r24` cycling `0x408047AE`/`0x408047B0`), with no NanoKernel replace.
- Conclusion: the EE-off `MALFUNCT` path is currently load-bearing for progress
  (it re-enters the boot process); the technically correct EE restore needs a
  separate fix to the 68K emulator loop. Reverted the EE restore to keep the
  best progressing state. Kept the EE-write probe and broadened park PC check.

### 9. EE-correct path exploration

- Restored `g_PpcContext.Msr = g_PpcContext.Srr1` in the trap-2 special case;
  park MSR is now `0x0002D032` (EE on).
- Disassembled the loop: guest 68K PC `0x408047AE` holds `0x60FE` (`BRA.S -2`,
  a self-loop). The PPC-side dispatch loop is `0x40B67EC0` (`mtctr r23`),
  `0x40B67EC4` (`rlwimi`), `0x40B67EC8` (`mtlr`), `0x40B67ECC`
  (`lhau r27,2(r24)`), with backup path `0x40BB07F0` (`lhau r27,-2(r24)`).
- Patching the 68K `BRA.S` to `NOP` made execution run off into garbage
  (`r24=0xEFC1xxxx`, PPC PC `0x40B80004` self-branch); reverted.
- Switching the park wake from DEC to external interrupt (`PPC_EXCEPTION_INTERRUPT`)
  with EE preserved gives one clean `Nanokernel replace` + PMDT conversion and
  **no** `MALFUNCT`, but still no `Priming`. Guest then settles into a new loop
  around `0x40B26570`/`0x40B273xx` with `r24=0x40B27534`.
- Broadening the wake trigger to `r24 == 0x40B27534` produced 8 EXT wakes but
  no further phase progression.

### 10. New stall loop is an SCC-polled context-save path

- Added `IDLEPOLL` diagnostic at `0x40B268D4`: `ch=0x00020000`, `ch[2]`
  alternates `0x05` (Rx-ready) / `0x04` (Tx-empty only), `ch[6]` returns `0x67`
  (`g`), and `[F3000050]` mirrors SCC Rx pending. The loop is polling the SCC,
  not a decrementer.
- `0x40B26880` is the SCC poll helper; callers include `0x40B27530` inside the
  context-save routine `0x40B272F0..0x40B27540`. `r24=0x40B27534` is the LR
  saved by that call, not a 68K PC.
- The Nanokernel debugger at `0x40B27574` prints `NanoKernel debugger`, and its
  command table includes `g [address] -- Go resume`. However the stall path
  never reaches the command dispatcher at `0x40B27624`.
- Raising the AUTORESUME cap from 3 to 8 and then 32 to feed `g` CR at the new
  sites consumed 32 bytes but produced no further progress; reverted to cap 3.
- Conclusion: the EE-correct path is stuck in an NK context-save/task-switch
  loop that consumes SCC input but never resumes the boot task. More input is
  not the fix; proper NK task-switch/interrupt state emulation is needed.

### 11. First task-shadow experiment and external-interrupt SRR1 fix

- User chose to attempt full NK task-switch/interrupt state emulation rather
  than restore the `MALFUNCT` shortcut.
- Reverse-engineered the dispatcher/context structures:
  - Context node offsets: `+0x04` stack, `+0x104` handler resume, `+0x10C` r1,
    `+0x114..+0x16C` r2-r13, `+0x174..+0x1FC` r14-r31, `+0x648` LR,
    `+0x658` next context.
  - `0x40B12BFC` begins the context restore path; `0x40B14940..0x40B14A1C` is
    the exception-return dispatch path via `r22+0x3C` / `r22+0x40`.
  - `0x40B14880` external handler calls `0x40B13D40` (`InterruptSave`), then
    tests SRR1 and may branch to `0x40B13AB0`.
- Added a temporary shadow-task model (`NK_TASK_SHADOW`) with capture/write
  helpers, attempted to arm a boot-process context at `0x0000B000` on the
  `Nanokernel replaced` print, and tried to queue it via `[r1+0x658]` at park
  wake. This reached boot-process resume `0x40B11FFC` but still ended in the
  `0x40B2751C` SCC poll; feeding 16 `g` CRs did not help.
- Diagnosed a real emulation gap: external exceptions did not set the
  exception-specific SRR1 cause bit. Added
  `g_PpcContext.Srr1 |= 0x00010000u` for `PPC_EXCEPTION_INTERRUPT` in
  `src\cpu\translation_impl.c` (trap already set `0x00020000u`). `PANICDUMP` was
  confirmed to be our own stall diagnostic, not a guest panic string.
- Forcing MSR[EE] at injection created `NULLBRANCH[0]: 1`; reverted. The
  ineffective task-shadow experiment and all of its guest-memory writes were
  removed; the AUTORESUME cap was restored to 3.
- Final 180s baseline with only the SRR1 cause-bit fix: stable, `GUEST STOP: 0`,
  `NULLBRANCH[0]: 0`, `PASS: 145`, `FAIL: 0`, but still no `Priming`, no second
  PMDT conversion, no graphics.

### 12. Dispatcher dump and SCC/debugger entry mapping

- Added a one-shot `DSPDUMP` at `0x40B1490C` to dump the live dispatcher state
  from `r22`, the dispatch table and pseudo-ECB area. Key values:
  - `r22=0x000081C0` (loaded from `[KDP-0x338]`, KDP `0xA000`)
  - `dsp=[r22+0x3C]=0x00006C00`, `stk=[r22+0x40]=0x00006D20`
  - `r20=0x24` (byte index 9), `dsp[9]=0x00006D00`, `stk[9]=0`
  - `ent=dsp[9]=0x00006D00`, `ent+0=0x40B11A90`, `ent+0x20=0x0000A000`
  - dumped `taskidx=0x000080E0`, `stkidx=0x00020380`, `srr1src=0x00A00000`
- Confirmed that `0x40B148E0..0x40B14940` computes `r20 = r3*4`, loads
  `dsp=[r22+0x3C]`, `stk=[r22+0x40]`, saves the current context, then sets
  `SRR0 = [dsp[r20]]` before `rfi`. The `VECDISP` `SRR0` value is the faulting
  PC at exception entry, not the post-`rfi` resume.
- Reverse-engineered the debugger entry:
  - `0x40B23600` polls SCC via `0x40B26880`; if a character is present,
    `0x40B2360C bl 0x40B272E8`.
  - `0x40B272E0` sets `cr1eq`; entry via `0x40B272E8` clears it. Cleared
    `cr1eq` forces the `0x40B2751C..0x40B2753C` SCC-wait inside the full
    context save before the command loop at `0x40B27574`.
  - `0x40B27540` sets `[r1+0xEDC] |= 2` and branches to the command path.
- Controlled experiments:
  - `AUTORESUME` cap 0 (no `g` CR feeds): no `Nanokernel replace`, guest stuck
    at `0x40BB07F0`; confirms the console feed is needed to unlock the first
    debugger/context-save path.
  - Targeted feeds at `0x40B2751C` (CR only, then `g` CR): the save-wait
    consumes the byte, but no `CMD[...]` or command-loop parser hit follows;
    the guest re-enters context save. SCC status shows Rx-ready bit clear on
    the later CR-only feed, so `0x40B26880` returns `-1`.
  - Long log shows repeated kernel init messages (`Init ready queue`,
    `Starting timeslicing`, `Adding idle task`) without reaching `Priming`.
- Conclusion: the blocker is not simply "more input"; the guest repeatedly
  re-enters the NK debugger/context-save path with inconsistent task/SPRG/KDP
  state. Reverted the targeted AUTORESUME and `DBGTRACE` experiments; kept
  `DSPDUMP` for the task-model work. Clean rebuild passes.

## Next move

- Continue proper NK task-switch emulation: map the ready queues and dispatch
  tables used by `0x40B148E0` / `0x40B14940` before adding any more task-shadow
  hooks. Start by dumping `dsp[9]=0x6D00` and the structures it references,
  and identify why the boot task keeps re-entering debugger save.
- Ultimately remove temporary diagnostics (`PPCSAMPLE`, `RELOPROBE`,
  `A6PROBE`, `TRAP2 entry`, extensive `SRR0BAD`, `PpcRecordMsrWrite`,
  `DSPDUMP`).

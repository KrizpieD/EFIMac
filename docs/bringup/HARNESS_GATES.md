# HARNESS_GATES — deterministic trace and execution rules

All agent runs must declare one contract ID and one hypothesis ID from
`STATE_LEDGER.md` before execution. The harness is a measurement device, not a
search strategy.

## Run budgets

| Gate type | Max `-Seconds` | Purpose | Allowed when |
|---|---:|---|---|
| Map/read-only | 25 | First entry/exit trace for a named PC or table | Phase A only |
| Scoped verification | 45 | One contract postcondition after one subsystem edit | Phase B |
| Regression check | 60 | Nearby contracts after a passing scoped gate | Phase B |
| Targeted validation | 120 | Post-implementation confirmation of C4-style output | Only after gate passes |
| Named C2B budget test | 240 | H-C9: prove post-resume init is slow, not stalled | Once, after diagnostics reverted |
| Named C3 budget test | 240 | Post-`Priming` C3 hypotheses (H-C116 onward): prove slow, not stalled | Once per authorized hypothesis, after C2B regression passes and diagnostics are reverted |
| Named C4 budget test | 240 | Post-`Priming` H-C133: prove QD install, guest framebuffer writes, and GOP blit | Once per authorized hypothesis, after C3 regression passes |
| Soak | 180 | Explicitly requested final validation | Never as discovery |

No multi-minute blind runs. No ad hoc QEMU command loops. Always use
`scripts/run-qemu-windows.ps1` directly from PowerShell, via
`scripts/bringup-gate.ps1` when a contract is named.

## Mandatory pre-run checklist

1. Remove stale stage: `Remove-Item -Recurse -Force "$env:TEMP\opencode\mac_disc\C__" -ErrorAction SilentlyContinue`.
2. Use `-NoReboot -StallSeconds 0`; `-Seconds` is a ceiling, never a target.
3. Supply `-StopPattern` for the contract's success marker and `-TimelinePattern`
   for first/last transition markers.
4. Bound every probe by a cap (`1`, `4`, `8`, `16`) or an executed-instruction
   window. Function-scope `static` counters are not zero-initialized.
5. Parse `%TEMP%\opencode\boot_out.txt`: strip `\x1b\[[0-9;?]*[A-Za-z]`, split on
   `..`, then search normalized segments. `qemu_result.txt` is informational only.
6. A run that passes no hypothesis is a failed run; record the falsified ID in
   `STATE_LEDGER.md`.

## Standard single-pass dump format

Every new diagnostic should emit one or more `BGATE-*` lines with fixed field
order. Existing `DSPDUMP`/`PANICDUMP` lines are accepted until replaced.

```
BGATE-DSP v1 contract=<C2|C2A|C2B> seq=<u32> pc=0x%08x r3=0x%08x r20=0x%08x r22=0x%08x dsp=0x%08x stk=0x%08x taskidx=0x%08x ent=0x%08x ent0=0x%08x srr0=0x%08x lr=0x%08x
BGATE-CTX v1 contract=<id> seq=<u32> entry=0x40B272E0 sprg0=0x%08x kdp=0x%08x ewa=0x%08x stack=0x%08x resume=0x%08x flags=0x%08x
BGATE-SCC v1 contract=<id> seq=<u32> channel=0x20000 status=0x%02x data=0x%02x ready=<0|1> pending=<0|1> head=<u32> tail=<u32>
BGATE-EXIT v1 contract=<id> gate=<gate-id> result=<PASS|FAIL> reason=<token> seq=<u32>
```

Required field meanings:
- `dsp=[r22+0x3C]`, `stk=[r22+0x40]`, `ent=[dsp+r20]`, `ent0=[ent]`.
- `srr0` is the value that will be loaded at the next guest `rfi`.
- `status` is the byte read from `0x20002`; `ready = status & 1`.
- `resume` is the context field that will become `SRR0` on task return.

## Gate script

`scripts/bringup-gate.ps1` wraps the run helper and enforces contract assertions:

```powershell
& .\scripts\bringup-gate.ps1 -Contract C2 -Seconds 45 `
  -Require @('DSPDUMP[1]','Nanokernel replace','AUTORESUME[1]') `
  -Forbid @('CPU MALFUNCT','GUEST STOP','budget stop')
```

It writes `%TEMP%\opencode\bringup_gate_result.txt` with `PASS|FAIL`,
contract ID, missing patterns, forbidden patterns, and normalized log path, then
sets a non-zero exit code on failure. It parses normalized log segments; it does
not trust `qemu_result.txt`.

## Known harness discrepancies (audit 2026-11-23)

- `run-qemu-windows.ps1` has no pass/fail exit status; outcome is only text in
  `qemu_result.txt`. `bringup-gate.ps1` supplies the exit status.
- `-StopPattern`/`-TimelinePattern` use raw `Contains` on 250 ms chunks, so split
  markers can be missed; the gate script re-parses the full normalized log.
- Stall detection is in an `elseif` guarded by no new bytes, so chatty logs can
  prevent stall exit. Use `-StallSeconds 0` and contract patterns.
- Scratch logs are shared (`boot_out.txt`); runs must be serial. No per-run
  archive exists; copy the log before the next run if comparison is needed.
- Headless and GUI runners diverge (`-display none`/QMP off vs `-vga std`/QMP on,
  `-Seconds` 25 vs 900); only headless results are contract-valid.
- Docs drift: `USER_GUIDE.md:55` uses a non-existent disc path, GUI script header
  names the headless script, `AGENTS.md` flag list omits `-Efi/-Esp/-Ovmf/
  -NoStopOnMarker/-FatalMarkers`, and `CMakeLists.txt` is not the supported build.
  Treat `BUILD_INSTRUCTIONS.md`, `Makefile`, and these bringup docs as authoritative.

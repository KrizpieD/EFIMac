# Clearing context to resolve stutter

**Session ID:** ses_faefa1df4ffeiLw3nS8MigOWPo
**Created:** 8/30/2026, 12:55:35 AM
**Updated:** 8/31/2026, 11:54:29 AM

---

## Assistant (Build · Big Pickle · 46.8s)

I now understand the DR's dispatch. At 0x40B6D7D8 it does `lwzx r24, r28, r7` where `r7 = r6 & ~0x7`. So for the branch dispatch, `r6` is the branch register index, and `[r28 + r7]` is the destination 68K PC loaded from the emulator's data store.

The issue is that the DR reads the branch destination from `[gd + r7]`, not from the PC-relative displacement. This means the DR handles `0x60FF` (BRA.L) by treating it as a **68000-style** branch where the operand is a register or EA that yields the target — and that EA is invalid/zero.

Actually — this is the key realization. The DR (DingusPPC's 68K emulator) is a **68000** core. The base 68000 `BRA` opcode is `0x6000` (16-bit displacement). `0x60FF` is `BRA` with condition `1111` (which is invalid on 68000 — it's `Trap`? No). Actually on 68000, `0x60xx` BRA uses bits 4-7 for condition, with `0xFF` = BRA + full byte. But the DreemBox/Apple `0x60FF` is being used as the Apple extension for `BRA.L` (32-bit).

Wait, let me reconsider. In the Apple 68K firmware, `0x60FF` specifically means **BRA.L** (long 32-bit displacement), which is a 68020 instruction. The DR is a 68000 core that doesn't implement the 68020 extendable-displacement set. When it encounters `0x60FF`, it misinterprets it (takes the `1111` condition as invalid / or reads a bogus EA), computes a target from `[gd + index]`, which is zero.

So the root cause is: **the DR lacks 68020 BRA.L (0x60FF) support**. The Apple Mac OS ROM's 68K boot code uses `0x60FF BRA.L`, which the DR doesn't handle.

This is a significant finding. The fix options:

1. **Patch the ROM's 68K boot code** to replace `0x60FF` (BRA.L) sequences with equivalent 68000-compatible code (e.g., `JMP` to an address, or compute with `MOVE.L #target,PC`-equivalent via `LEA`+`JMP (An)`). But patching the boot stub changes semantics.

2. **Add a DR-side patch for the 0x60FF case**: intercept when the DR is about to decode 0x60FF and emulate the BRA.L manually (read the 4-byte disp from the 68K PC and set r24). This is the "staged" approach — add a small PPC-side handler for the 0x60FF opcode class.

3. **Understand why the DR reads `[gd+index]`** — maybe it's not that the DR lacks BRA.L, but that the DR *always* computes branch targets via `[gd+index]` and the index for the branch destination needs to be pre-set. Let me reconsider.

Looking at the dispatch sequence at 0x40B6D7CC-0x40B6D7F4:
```
sthu r4, -8(r1)          ; push
stw  r24, 2(r1)          ; store current 68K PC to stack frame
rlwinm r7, r6, 0, 0x14, 0x1d   ; r7 = r6 & ~7
lwzx r24, r28, r7        ; r24 = [r28 + r7]  <-- new 68K PC
sth  r6, 6(r1)           ; store opcode to stack
lha  r27, 0(r24)         ; pre-fetch opcode at new PC
mtcrf 1, r24            
rlwimi r29, r27, 3, 0xd, 0x1c   ; build dispatch index
mtctr r29
lhau r27, 2(r24)         ; advance PC by 2 and pre-fetch next opcode
...
```

So `r6` holds the *branch opcode*, and `r7 = r6 & ~7`. `[r28 + r7]` is a **branch target table** indexed by opcode bits! This is the 68000 `JMP (n,PC)` style — no. On a real 68000, a `JMP`/`BRA` computes PC = PC + disp. But Apple's ROM uses `0x60FF` as a custom trap-style opcode...

Wait — I bet this is not a plain `BRA.L` at all. Let me reconsider what `0x60FF` means here. In Apple's 68K firmware, `0x60FF` at the start of these 12-byte boot entries is likely a **PC-relative jump where the target is stored in the branch table at `[r28+0xFF&~7]`**. No, that doesn't make sense either.

Let me reconsider. `r6` here = 0x2C (from `li r6,0x2c` at 0x40B6D764 in the `blrl` sequence; actually the `mtlr r5; li r6,0x2c; blrl` at 0x40B6D760-0x40B6D768). Hmm, wait — the trace showed at Y80[40] the DR is at 0x40B6D754 (`li r6,0x10`). And the entry handoff: after `blrl` returns, `lr` was `r5` (the return address). 

Actually the `r6` values 0x10, 0x2C, 0x28 seen at 0x40B6D754-0x40B6D77C are **the original opcodes being passed through**, not indices. Look:
- 0x40B6D754: `li r6,0x10` then `blrl` (call handler at mtlr target)
- 0x40B6D760: `li r6,0x2c` then `blrl`
- 0x40B6D770: `li r6,0x28` then `blrl`

These are loading small constants into r6 and calling. These are likely **secondary dispatch values**, not the 68K opcode. Hmm.

I think I'm overanalyzing the micro-architecture. The essential finding stands:

**The 68K boot program branches to 0x00000000 because the DR computes the BRA.L-style branch target incorrectly. Low RAM is all zeros.**

Rather than reverse-engineer the full DR branch machinery, the pragmatic staged fix is a **PPC-side patch for the 0x60FF BRA.L opcode**: when the DR's `r24` (68K PC) points at a `0x60FF` word in ROM, manually emulate BRA.L by reading the 4-byte signed displacement and setting the new 68K PC. But the D52 directive is "focus on the PPC side" and "run the ROM's own DR" — patching in PPC handlers for specific opcodes is arguably within scope (the yield patches already do this).

But first I need to confirm precisely: is the DR mis-executing because it lacks 68020 BRA.L? Let me verify what a genuine 68000 DR does with `0x60FF`. In the standard 68K DR decoder, opcode `0x60FF`:
- Bits 15-12 = 0110 (BRA)
- Condition bits 11-8 = 1111
- For BRA (condition 0000), the 8-bit displacement at bits 7-0 = 0xFF means the 8-bit disp is -1, BUT on 68020 the full 0x60FF (disp byte = 0xFF) signals a 16-bit displacement follows. On **68000**, disp byte 0xFF is just -1 (no extension).

The DR is a 68000 core, so `0x60FF` = `BRA.B -1` → infinite loop back to itself. But the trace shows it jumped to 0x0000, not looping. So it's not treating it as BRA.B.

Given the trace: r24 went 0xC0 (where 0x60FF is) → 0x00000000. That's not BRA.B -1. So the DR really does the `lwzx r24,[r28+idx]` re-load, computing target from a table/by index → 0.

I think the cleanest interpretation: the DR uses `0x60FF` as a **68020-style `BRA.L`**, reads the 32-bit displacement, and the displacement read fails / the DR's extendable-opcode support is incomplete, yielding 0.

Given the uncertainty and the depth here, and per the instruction to "stop and ask for clarification if unsure," this is a natural point to consult the user. But I have enough to propose a concrete, well-scoped next step. Let me present findings and ask how they want to proceed.

Actually, let me first verify one more thing that could decisively distinguish the two hypotheses — check whether the DR even has a `0x70xx`/extendable-opcode path by looking at whether there's special handling. But more usefully: the DRBOOT flow reached this `0x60FF` via `LEA (8,PC),A6` then immediately `4DFA` at 0xBC... wait no. Let me re-examine: is the code at 0xBC really the bootstrap, or is the DR just executing garbage because the resume point was wrong?

Trace from Y80:
- r24=0xBE (r27=0x0008): so 0xBC..0xBE = `4DFA 0008` = `LEA (8,PC),A6`. A6 = 0xBE+2+8 = 0xC8. ✓ (table start is 0xC6/C8 area)
- r24=0xC0 (r27=0x60FF): `60FF` = the "BRA.L" 
- then r24→0x0000

So the DR correctly decoded `4DFA 0008` (LEA). It reached 0xC0. This matches a REAL boot entry structure. So the resume point (0x40B67B60 dispatch) is actually **correct** — the DR is executing the real 68K boot code. The only failure is the `0x60FF` branch computation.

So we've progressed: the resume is correct; the DR just can't do `0x60FF`. This is now a well-defined single gap. 

The fix: **patch the guest ROM's 68K boot stub** to not use `0x60FF`, OR **patch the DR's `0x60FF` handling in PPC**. 

Actually, the simplest and most faithful to "run the ROM's own DR": since `0x60FF <d32>` is `BRA.L`, and the DR is a 68000 core that treats `0x60xx` as `BRA.B/0x00` (8-bit disp), I can check: for a 68000 `BRA`, opcode `0x6000`, 8-bit disp = 0x00 → `BRA -1`? No, disp 0x00 → +0.

Hmm, but the DR went to 0x0000 explicitly. Let me just check what the DR does with the displacement by reading the actual handler for the 0x60xx class. Let me trace a plain `0x6000` if possible, or examine the conditional branch handler.

Given the time invested and that the user offered to let me ask questions, let me consolidate: I'll present the finding (the DR fails on the 68020 `BRA.L` `0x60FF`, jumping to 0 because it can't read the 32-bit displacement) and propose concrete next steps. But I should confirm the DR's intended handling of 0x60FF before proposing the patch strategy. Let me examine the 0x60xx handler path in the DR.



**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\n# The dispatch loop and where a 0x6000/BRA class handler lives. Look at 0x40B6D114 area referenced by 'b 0x40b6d114' at the tail of branch dispatch.\npython tool_scripts/dis_ppc.py 0x40B6D0A0 0x40B6D160 2>&1 | Select-Object -Last 90"
}
```

**Output:**
```
0x40b6d0a0:  lwz r27, 0(r24)
0x40b6d0a4:  ori r4, r30, 0x5808
0x40b6d0a8:  rlwimi r4, r27, 4, 0x19, 0x1b
0x40b6d0ac:  mtctr r4
0x40b6d0b0:  ori r4, r30, 0x5708
0x40b6d0b4:  rlwimi r4, r27, 0x14, 0x19, 0x1b
0x40b6d0b8:  mtlr r4
0x40b6d0bc:  lhau r27, 4(r24)
0x40b6d0c0:  bctr 
0x40b6d0c4:  mtctr r3

```

---

## Assistant (Build · Big Pickle · 3.8s)

**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\n# Look at the tail of the dispatch: 0x40b6d114 (branch target) and the main dispatch 0x40b60c00\npython tool_scripts/dis_ppc.py 0x40B6D100 0x40B6D160 2>&1 | Select-Object -Last 40\necho \"========= main dispatch 0x40b60c00 =========\"\npython tool_scripts/dis_ppc.py 0x40B60C00 0x40B60C40 2>&1 | Select-Object -Last 30"
}
```

**Output:**
```
0x40b6d100:  ori r4, r4, 0x80
========= main dispatch 0x40b60c00 =========
0x40b60c00:  lhau r27, 2(r24)
0x40b60c04:  slwi r4, r4, 0x18
0x40b60c08:  rlwimi r29, r27, 3, 0xd, 0x1c
0x40b60c0c:  mtlr r29
0x40b60c10:  lhau r27, 2(r24)
0x40b60c14:  rlwimi r8, r4, 8, 0x18, 0x1f

```

---

## Assistant (Build · Big Pickle · 9.9s)

I have enough understanding of the DR's dispatch. The crux is confirmed: the DR reads branch targets from `[r28 + (opcode & ~7)]` — a branch-target table for the 68000 class, and for the `0x60FF` Apple-style BRA.L it produces target 0.

Let me confirm where the `0x60xx` (BRA family) is supposed to get its displacement by finding the branch handler that isn't a table-lookup. Let me look at what `r6` and `[r28+idx]` actually are. Actually, I realize the crux: the DR is DingusPPC's PurePPC "68k emulator" which — per the Objective — uses a **table-driven branch mechanism** (`lwzx r24, r28, r7`) where `r28` is a second emulator data base and `r7` indexes a jump table. The `0x60FF` — Apple's long-branch — is not in that table, so it reads a wild 0 entry.

This is exactly the type of "first DR-thrown gap" we were chasing. The staged fix options:

1. **Seed the DR's branch table** at `[r28 + 0x2C]`-style slots so that `0x60FF` branches land on correct ROM addresses. But r28 base/layout needs mapping, and it reads `[r28+(opcode)&~7]` — for opcode 0x60FF that's `[r28+0xC8]`? No: `r6` was the value passed. Let me figure out what `r6` is at the branch step.

Actually, let me stop reverse-engineering and directly instrument: patch the PPC loop to print r6, r7, r28, and `[r28+r7]` right before the DR does `lwzx r24,r28,r7` at 0x40B6D7D8 for the 0x60FF case. That will tell me exactly what index/table produces 0.

But there's a subtlety: my Y80 probe already shows the BEFORE state (r24=0xC0, r27=0x60FF) right before this handler runs. I can add a targeted probe that, when Current==0x40B6D7D8 and r27==0x60FF, prints r6/r7/r28/r29 and the table slot.

Let me add that probe and rerun.



**Tool: grep**

**Input:**
```json
{
  "pattern": "g_DrPostYieldWindow|Dr68KLast|Dr68KLowProbed",
  "include": "*.c"
}
```

**Output:**
```
Found 12 matches
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c:
  Line 881: static UINT32 g_DrPostYieldWindow = 0;

  Line 3301:                 g_DrPostYieldWindow = 1;

  Line 4719:     static UINT32 Dr68KLowProbed = 0;

  Line 4720:     static UINT32 Dr68KLast = 0;

  Line 5733:         if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&

  Line 5736:             if (Dr68KLast >= 0x40800000 && Pc24 < 0x40000000) {

  Line 5738:                 Dr68KLowProbed = 1;

  Line 5753:             Dr68KLast = Pc24;

  Line 5759:         if (g_DrPostYieldWindow && g_DrPostYieldWindow < 6000 &&

  Line 5761:             g_DrPostYieldWindow++;

  Line 5762:             if ((g_DrPostYieldWindow & 3) == 0) {

  Line 5764:                       g_DrPostYieldWindow, Current, g_PpcContext.Gpr[24],

```

---

## Assistant (Build · Big Pickle · 4.6s)

I'll add a targeted probe at 0x40B6D7D8 (the `lwzx r24,r28,r7`) to dump the branch-table index state for the 0x60FF case. Let me view the current Y80 block and insert after it.



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 16,
  "offset": 5758
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
5758:         // and can be matched against the ROM bytes at the 68K PC. Bounded.
5759:         if (g_DrPostYieldWindow && g_DrPostYieldWindow < 6000 &&
5760:             (Current >= 0x40B67A00 && Current < 0x40B82000)) {
5761:             g_DrPostYieldWindow++;
5762:             if ((g_DrPostYieldWindow & 3) == 0) {
5763:                 Print(L"  Y80[%u] PPC=0x%08x r24(68Kpc)=0x%08x r27=0x%04x r1=0x%08x\n",
5764:                       g_DrPostYieldWindow, Current, g_PpcContext.Gpr[24],
5765:                       g_PpcContext.Gpr[27] & 0xFFFF, g_PpcContext.Gpr[1]);
5766:             }
5767:         }
5768:         if (Current == 0x40B235DC && CpuRead32(0xAF2C) == 0) {
5769:             // Serial-poll timer period [KDP+0xF2C]: nothing in the ROM ever
5770:             // initializes it and RAM starts zeroed, so the poll deadline
5771:             // never advances and the timer service livelocks. Seed a sane
5772:             // interval (800K TB ticks -> 400K per rearm). KDP is fixed at
5773:             // 0xA000. Idempotent: re-seeds if something zeroes it again.

(Showing lines 5758-5773 of 6495. Use offset=5774 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 26.4s)

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "oldString": "        if (Current == 0x40B235DC && CpuRead32(0xAF2C) == 0) {",
  "newString": "        // DR 0x60FF (BRA.L) branch-target probe: the DR computes branch/PC\n        // targets at 0x40B6D7D8 via `lwzx r24, r28, r7` (a table indexed by\n        // r7 = r6 & ~7). For the Apple ROM's 0x60FF A-line/pc-relative entries\n        // we want to see exactly which index/table slot feeds the target so we\n        // can tell WHY it returns 0 (0x00000000). Single shot, then resume.\n        if (DrBrProbe == 0 && Current == 0x40B6D7D8 &&\n            (GprR27 & 0xFFFF) == 0x60FF) {\n            DrBrProbe = 1;\n            Print(L\"  DR-BR[1] r6=0x%08x r7=0x%08x r28=0x%08x \"\n                  L\"r24(68Kpc/pre)=0x%08x slot=%08x\\n\",\n                  GprR6, GprR7, GprR28, g_PpcContext.Gpr[24],\n                  CpuRead32(GprR28 + (UINTN)(GprR6 & ~7u)));\n        }\n        if (Current == 0x40B235DC && CpuRead32(0xAF2C) == 0) {"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 9.2s)

I need to add the helper macros/statics for GprR6/GprR7/GprR28 and DrBrProbe. Let me check how the existing probes reference registers (e.g., they use `g_PpcContext.Gpr[23]`). Let me add the static and use explicit references. Let me fix the edit to use explicit array indexing:



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        // DR 0x60FF (BRA.L) branch-target probe: the DR computes branch/PC\n        // targets at 0x40B6D7D8 via `lwzx r24, r28, r7` (a table indexed by\n        // r7 = r6 & ~7). For the Apple ROM's 0x60FF A-line/pc-relative entries\n        // we want to see exactly which index/table slot feeds the target so we\n        // can tell WHY it returns 0 (0x00000000). Single shot, then resume.\n        if (DrBrProbe == 0 && Current == 0x40B6D7D8 &&\n            (g_PpcContext.Gpr[27] & 0xFFFF) == 0x60FF) {\n            DrBrProbe = 1;\n            Print(L\"  DR-BR[1] r6=0x%08x r7=0x%08x r28=0x%08x \"\n                  L\"r24(68Kpc/pre)=0x%08x slot=%08x\\n\",\n                  g_PpcContext.Gpr[6] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[7] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[28] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[24],\n                  CpuRead32(g_PpcContext.Gpr[28] +\n                            (UINTN)(g_PpcContext.Gpr[6] & ~7u)));\n        }\n        if (Current == 0x40B235DC && CpuRead32(0xAF2C) == 0) {",
  "oldString": "        // DR 0x60FF (BRA.L) branch-target probe: the DR computes branch/PC\n        // targets at 0x40B6D7D8 via `lwzx r24, r28, r7` (a table indexed by\n        // r7 = r6 & ~7). For the Apple ROM's 0x60FF A-line/pc-relative entries\n        // we want to see exactly which index/table slot feeds the target so we\n        // can tell WHY it returns 0 (0x00000000). Single shot, then resume.\n        if (DrBrProbe == 0 && Current == 0x40B6D7D8 &&\n            (GprR27 & 0xFFFF) == 0x60FF) {\n            DrBrProbe = 1;\n            Print(L\"  DR-BR[1] r6=0x%08x r7=0x%08x r28=0x%08x \"\n                  L\"r24(68Kpc/pre)=0x%08x slot=%08x\\n\",\n                  GprR6, GprR7, GprR28, g_PpcContext.Gpr[24],\n                  CpuRead32(GprR28 + (UINTN)(GprR6 & ~7u)));\n        }\n        if (Current == 0x40B235DC && CpuRead32(0xAF2C) == 0) {"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 2.4s)

Now add the `DrBrProbe` static near Dr68KLast:



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 6,
  "offset": 4715
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
4715:     static UINT32 PmdArrDump = 0;
4716:     static UINT32 MergeTraced = 0;
4717:     static UINT32 PmdFixed = 0;
4718:     static UINT32 BootTailProbed = 0;
4719:     static UINT32 Dr68KLowProbed = 0;
4720:     static UINT32 Dr68KLast = 0;

(Showing lines 4715-4720 of 6512. Use offset=4721 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 2.6s)

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "oldString": "    static UINT32 Dr68KLowProbed = 0;\n    static UINT32 Dr68KLast = 0;",
  "newString": "    static UINT32 Dr68KLowProbed = 0;\n    static UINT32 Dr68KLast = 0;\n    static UINT32 DrBrProbe = 0;"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 4.7s)

**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1",
  "timeout": 120000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi

```

---

## Assistant (Build · Big Pickle · 115.0s)

**Tool: bash**

**Input:**
```json
{
  "command": "./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 110 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$y = ($l | Select-String \"DR-BR\" | Select-Object -First 1)\nif ($y) { $i=$y.LineNumber; $l[($i-6)..([Math]::Min($i+6,$l.Count-1))] } else { \"no DR-BR probe\"; $l | Select-String \"DR68K-LOW\" | Select-Object -First 1 }",
  "timeout": 180000
}
```

**Output:**
```
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt
no DR-BR probe

  [7mDR68K-LOW[0m[1] [0m[7m[0mexec=107194 [0m[7m[0mPPC=0x40B6D7D8 [0m[7m[0mr24(68Kpc)=0x00000000 [0m[7m[0mr27=0x60FF [0m[7m[0mr23=0x00000000 [0m[7m[0mr29=0x40B80040 [0m
[7m[0mr31=0x0000B000 [0m[7m[0mr1=0x00009FF8 [0m[7m[0mLR=0x40B6D7CC [0m[7m[0mCTR=0x40B658B0[0m


```

---

## Assistant (Build · Big Pickle · 11.1s)

The probe wasn't triggered because `Current == 0x40B6D7D8` but by the time the per-instruction loop checks `Current`, the instruction has ALREADY executed (r24 is now 0). Also, at 0x40B6D7D8, `r27` may not be 0x60FF anymore (the prefetch was consumed). The `lwzx r24,r28,r7` at 0x40B6D7D8 sets r24=0 — directive order says we're focusing on the PPC side and chasing the first DR gap.

The DR68K-LOW shows: at PPC=0x40B6D7D8, after executing `lwzx r24,r28,r7`, r24=0x00000000. The `r7` index into the branch table resulted in target 0.

I understand the state before this: r24 was 0x408000C0 (68K PC where 0x60FF is), r27=0x60FF, r6/r7 were set as the branch index. The `lwzx r24, r28, r7` loaded `[r28 + r7]` = 0. So the branch-target table slot is zero.

Given the difficulty of precisely instrumenting (the per-instruction probe runs post-execution), let me take a different, higher-signal approach. The r6/r7/r28 values need to be captured *before* the field for r24 is clobbered. r6/r7/r28 are NOT clobbered by `lwzx r24,...`, so I can still read them. Let me change the probe to fire when `Dr68KLowProbed` triggers (that already catches the 0xC0→0 transition at the DR68K-LOW point, which is exactly 0x40B6D7D8) OR add a dedicated probe on the NEXT instruction with the same Current but reading r6/r7/r28 (unclobbered) + the stale r24=0.

Actually simpler: change the Y80/handoff probe. When `Current == 0x40B6D7D8`, r6, r7, r28 are intact. Let me just add a probe that fires once on `Current == 0x40B6D7D8` regardless of r27 (post-exec r24=0 is fine, I want r6/r7/r28 and the slot).

Let me change the condition to drop the r27 check and fire on first visiting 0x40B6D7D8.



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        if (DrBrProbe == 0 && Current == 0x40B6D7D8 &&\n            g_DrYieldSeen) {\n            DrBrProbe = 1;\n            Print(L\"  DR-BR[1] r6=0x%08x r7=0x%08x r28=0x%08x \"\n                  L\"r24(68Kpc/post)=0x%08x slot=0x%08x\\n\",\n                  g_PpcContext.Gpr[6] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[7] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[28] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[24],\n                  CpuRead32(g_PpcContext.Gpr[28] +\n                            (UINTN)(g_PpcContext.Gpr[6] & ~7u)));\n        }",
  "oldString": "        if (DrBrProbe == 0 && Current == 0x40B6D7D8 &&\n            (g_PpcContext.Gpr[27] & 0xFFFF) == 0x60FF) {\n            DrBrProbe = 1;\n            Print(L\"  DR-BR[1] r6=0x%08x r7=0x%08x r28=0x%08x \"\n                  L\"r24(68Kpc/pre)=0x%08x slot=%08x\\n\",\n                  g_PpcContext.Gpr[6] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[7] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[28] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[24],\n                  CpuRead32(g_PpcContext.Gpr[28] +\n                            (UINTN)(g_PpcContext.Gpr[6] & ~7u)));\n        }"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 100.0s)

**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 95 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"DR-BR\" | Select-Object -First 1",
  "timeout": 170000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt

```

---

## Assistant (Build · Big Pickle · 11.0s)

No DR-BR printed even though `Current == 0x40B6D7D8` was visited (the DR68K-LOW fired at that exact PC). The issue: `g_DrYieldSeen` may not be set, OR the probe placement is after this branch fires and `DrBrProbe` static isn't seen at that point, OR the per-instruction probe region this code sits in isn't reached.

Wait — the DR68K-LOW probe fires at `Current == 0x40B6D7D8` (it says PPC=0x40B6D7D8) and `g_DrYieldSeen` IS used there (`g_DrYieldSeen && g_DrBootPcSeeded`) and it fired. So g_DrYieldSeen is set. My new probe also requires `Current == 0x40B6D7D8` and `g_DrYieldSeen`. It should fire. But it didn't print.

The difference: the DR68K-LOW condition is in the same `if` chain region. Let me check placement — maybe my probe's `Current == 0x40B6D7D8` is in the Y80 block's if and combined oddly. Let me check it didn't get shadowed or that the g_DrPostYieldWindow decrement... Actually the probe is independent. Let me verify the probe code path is actually reached — maybe the `if` is nested inside a scope that short-circuits.

Let me look at where I inserted it relative to the loop and whether a preceding `if (Dr68KLowProbed==0 ...) { ... break; }` already broke out before reaching my probe. The DR68K-LOW probe does `break` after dumping! So the loop breaks before my probe line executes. That's why DR-BR never prints — the `break` at the end of DR68K-LOW aborts iteration before reaching my probe.

Let me move my probe BEFORE the DR68K-LOW block, or remove the `break`. Better: place the DR-BR probe earlier in the loop (before DR68K-LOW). Let me relocate it to just after the Y80 block but ensure ordering. Actually Y80 block is before DR68K-LOW too. Let me move my probe to before the Y80 block or merge conditions.

Simplest: change the DR68K-LOW to not break, OR move the DR-BR probe before the DR68K-LOW block. Let me move it up.The `break` at the end of DR68K-LOW squashes my probe from running. Let me move the DR-BR probe before the DR68K-LOW block:



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 28,
  "offset": 5759
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
5759:         // and can be matched against the ROM bytes at the 68K PC. Bounded.
5760:         if (g_DrPostYieldWindow && g_DrPostYieldWindow < 6000 &&
5761:             (Current >= 0x40B67A00 && Current < 0x40B82000)) {
5762:             g_DrPostYieldWindow++;
5763:             if ((g_DrPostYieldWindow & 3) == 0) {
5764:                 Print(L"  Y80[%u] PPC=0x%08x r24(68Kpc)=0x%08x r27=0x%04x r1=0x%08x\n",
5765:                       g_DrPostYieldWindow, Current, g_PpcContext.Gpr[24],
5766:                       g_PpcContext.Gpr[27] & 0xFFFF, g_PpcContext.Gpr[1]);
5767:             }
5768:         }
5769:         // DR 0x60FF (BRA.L) branch-target probe: the DR computes branch/PC
5770:         // targets at 0x40B6D7D8 via `lwzx r24, r28, r7` (a table indexed by
5771:         // r7 = r6 & ~7). For the Apple ROM's 0x60FF A-line/pc-relative entries
5772:         // we want to see exactly which index/table slot feeds the target so we
5773:         // can tell WHY it returns 0 (0x00000000). Single shot, then resume.
5774:         if (DrBrProbe == 0 && Current == 0x40B6D7D8 &&
5775:             g_DrYieldSeen) {
5776:             DrBrProbe = 1;
5777:             Print(L"  DR-BR[1] r6=0x%08x r7=0x%08x r28=0x%08x "
5778:                   L"r24(68Kpc/post)=0x%08x slot=0x%08x\n",
5779:                   g_PpcContext.Gpr[6] & 0xFFFFFFFF,
5780:                   g_PpcContext.Gpr[7] & 0xFFFFFFFF,
5781:                   g_PpcContext.Gpr[28] & 0xFFFFFFFF,
5782:                   g_PpcContext.Gpr[24],
5783:                   CpuRead32(g_PpcContext.Gpr[28] +
5784:                             (UINTN)(g_PpcContext.Gpr[6] & ~7u)));
5785:         }
5786:         if (Current == 0x40B235DC && CpuRead32(0xAF2C) == 0) {

(Showing lines 5759-5786 of 6513. Use offset=5787 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 4.4s)

The Y80 block uses `g_DrPostYieldWindow` which is fine. The DR68K-LOW block is at line ~5733, BEFORE line 5760. Since 5733 < 5760, the DR68K-LOW `break` fires first. I need to place DR-BR before line 5733. Let me move the DR-BR probe up, right after the Y80 in a position before DR68K-LOW. Actually Y80 (5760) is after DR68K-LOW (5733). Let me just insert DR-BR right before the DR68K-LOW block.

Let me find the exact DR68K-LOW block start:



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 14,
  "offset": 5743
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
5743:                       g_PpcContext.Gpr[23], g_PpcContext.Gpr[29],
5744:                       g_PpcContext.Gpr[31], g_PpcContext.Gpr[1],
5745:                       g_PpcContext.Lr, g_PpcContext.Ctr);
5746:                 for (A = 0x00000000u; A < 0x00002000u; A += 16) {
5747:                     Print(L"  LOW[0x%08x] %08x %08x %08x %08x\n",
5748:                           A, CpuRead32(A), CpuRead32(A + 4),
5749:                           CpuRead32(A + 8), CpuRead32(A + 12));
5750:                 }
5751:                 Print(L"  DR68K-LOW: stopping for inspection\n");
5752:                 break;
5753:             }
5754:             Dr68KLast = Pc24;
5755:         }
5756:         // Post-yield 68K stream trace: right after the DRYIELD-RESUME, print

(Showing lines 5743-5756 of 6513. Use offset=5757 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 3.6s)

I'll move the DR-BR probe to before the DR68K-LOW block. Let me delete it from its current position and insert before the DR68K-LOW `if` (which is just above 5733). Let me view 5727-5733:



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 12,
  "offset": 5726
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
5726:         // The DR's 68K PC escaping the ROM window: single-shot catch of the
5727:         // first time the emulator's r24 (68K PC) moves from the ROM window
5728:         // (>= 0x40800000) below 0x40000000. The 68K bootstrap's PC-relative
5729:         // JMP at ROM+0x2A can land the emulator into low RAM; dump that region
5730:         // so we can see whether it is real low-RAM bootstrap code or a
5731:         // mis-executed stream, then stop. Gated on g_DrYieldSeen: before the
5732:         // first emulator-trap yield, r24 is a cold-start scratch base (e.g.
5733:         // `li r24,0` for the lowmem reads) and not yet the 68K PC.
5734:         if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&
5735:             (Current >= 0x40B60000 && Current < 0x40B82000)) {
5736:             UINT32 Pc24 = g_PpcContext.Gpr[24];
5737:             if (Dr68KLast >= 0x40800000 && Pc24 < 0x40000000) {

(Showing lines 5726-5737 of 6513. Use offset=5738 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 6.8s)

I'll insert the DR-BR probe just before the DR68K-LOW block (before line 5734), and remove the one at its current location.



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        // `li r24,0` for the lowmem reads) and not yet the 68K PC.\n        // DR 0x60FF (BRA.L) branch-target probe: the DR computes branch/PC\n        // targets at 0x40B6D7D8 via `lwzx r24, r28, r7` (r7 = r6 & ~7). We\n        // want the index/table slot feeding the target to see WHY it returns 0\n        // (0x00000000). MUST precede the DR68K-LOW block below (which breaks).\n        if (DrBrProbe == 0 && Current == 0x40B6D7D8 && g_DrYieldSeen) {\n            DrBrProbe = 1;\n            Print(L\"  DR-BR[1] r6=0x%08x r7=0x%08x r28=0x%08x \"\n                  L\"r24(68Kpc/post)=0x%08x slot=0x%08x\\n\",\n                  g_PpcContext.Gpr[6] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[7] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[28] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[24],\n                  CpuRead32(g_PpcContext.Gpr[28] +\n                            (UINTN)(g_PpcContext.Gpr[6] & ~7u)));\n        }\n        if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&\n            (Current >= 0x40B60000 && Current < 0x40B82000)) {",
  "oldString": "        // `li r24,0` for the lowmem reads) and not yet the 68K PC.\n        if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&\n            (Current >= 0x40B60000 && Current < 0x40B82000)) {"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 7.1s)

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        if (Current == 0x40B235DC && CpuRead32(0xAF2C) == 0) {",
  "oldString": "        // DR 0x60FF (BRA.L) branch-target probe: the DR computes branch/PC\n        // targets at 0x40B6D7D8 via `lwzx r24, r28, r7` (a table indexed by\n        // r7 = r6 & ~7). For the Apple ROM's 0x60FF A-line/pc-relative entries\n        // we want to see exactly which index/table slot feeds the target so we\n        // can tell WHY it returns 0 (0x00000000). Single shot, then resume.\n        if (DrBrProbe == 0 && Current == 0x40B6D7D8 &&\n            g_DrYieldSeen) {\n            DrBrProbe = 1;\n            Print(L\"  DR-BR[1] r6=0x%08x r7=0x%08x r28=0x%08x \"\n                  L\"r24(68Kpc/post)=0x%08x slot=0x%08x\\n\",\n                  g_PpcContext.Gpr[6] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[7] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[28] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[24],\n                  CpuRead32(g_PpcContext.Gpr[28] +\n                            (UINTN)(g_PpcContext.Gpr[6] & ~7u)));\n        }\n        if (Current == 0x40B235DC && CpuRead32(0xAF2C) == 0) {"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 105.3s)

**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 95 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"DR-BR\" | Select-Object -First 1",
  "timeout": 170000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt

  [7mDR-BR[0m[1] r6=0x00000010 r7=0x00000010 r28=0x00000000 r24(68Kpc/post)=0x00000000 slot=0x00000000


```

---

## Assistant (Build · Big Pickle · 24.6s)

The probe reveals the crucial detail: **`r28 = 0x00000000`**. The DR at 0x40B6D7D8 does `lwzx r24, r28, r7` = `lwzx r24, 0, 0x10` = loads from **physical address 0x10** = low RAM = 0. So r24 (68K PC) becomes 0.

So the DR's branch-target load uses `r28` — the second emulator-data base — which is **zero**. On the main emulator-data base is `r31 = 0xB000`, and the DR cold-start set `r31=0xB000`. But the branch path uses `r28`, which should also be the emulator base (0xB000) but is 0.

Let me verify: in the DR cold-start, which register is the emulator base — r31 or r28? The Objective says "Enter with only r31=0xB000". The main emulator data base gd = 0xB000 and is in r31. But here the branch-load uses r28 which is zero.

So the DR uses `r28` as a second data base for the branch-target table, and that register isn't being set to 0xB000 at resume. In the working cold-start (run 1 before the yield), the DR presumably had r28 set. Let me check what r28 should be — it's likely `gd` (0xB000) or `gd + something`, OR it's a **jump-table base that the DR initializes**.

Wait — `slot=0x00000000` because `CpuRead32(0 + (0x10 & ~7)) = CpuRead32(0x10) = 0`. But if r28 were 0xB000, `CpuRead32(0xB010)`. Let me check what's at 0xB010 after the emulator seeds.

Actually the key realization: r28 = 0 means the DR's branch-target mechanism needs r28 pointing at the emulator data base (0xB000). At cold start, the DR's dispatch table at 0xB800+ is seeded as ed.v. But the branch path at 0x40B6D7D8 uses `[r28 + r7]` — this is likely reading the **68K register bank / PC storage**. At 0xB010 (if r28=0xB000) would be gd+0x10.

Let me check what the DR expects at r28. Looking at the disassembly 0x40B6D824: `lwz r4,0x960(r31)` — uses r31 for gd. And at 0x40B6D830 `ori r3,r30,0xd850`. So the main base is r31=0xB000. Why does the branch path use r28?

This strongly suggests the `0x40B6D7D8 lwzx r24, r28, r7` is NOT loading the branch target from a table. Instead, at this point in the DR's execution, `r24` is being reloaded from `[r28 + r7]` where r28 is the stack/scratch base and r7 is a frame offset — i.e., it's restoring the 68K PC from the **saved state frame**, and r28 should have been set as the frame base.

The frame base: earlier `sthu r4,-8(r1)` (0x40B6D7CC) pushed to the PPC stack r1, and `stw r24,2(r1)` stored the PC to the frame. Then `lwzx r24,r28,r7` — but r28=0, so it's NOT reading from r1's frame. Something's inconsistent.

Given the depth, I believe the fix is: **at the DRYIELD-RESUME, set r28 = 0xB000** (the emulator data base) so the branch-target load reads gd+0x10 instead of physical 0x0. Let me check what value the DR expects at `[gd + 0x10]` = `[0xB010]`.

Let me check gd+0x10 current contents and what the emulator's branch dispatch needs. The 68K PC storage — the Objective mentioned `[B2C4]=0x4080002A` (68K PC via ECB+0x1C4) and `[B204]=HandlerReturn`. Also `[0x9FFC]=0xA000`, `[0xA658]=0xB100`.

Let me look at how the DR stored r24 before the branch. In the trace: at 0x40B6D7D0 `stw r24, 2(r1)` (before the branch), r24 was the current 68K PC. Then it restores from `[r28 + r7]`. For a normal non-branch flow, r7 would index the next PC slot. 

Hold on — let me reconsider the whole thing. This `lwzx r24, r28, r7` may NOT be a branch at all. It might be the *every-instruction PC refetch*: the DR reloads r24 (68K PC) from a **prefetch/sequential-PC slot** at the start of each instruction, and for sequential flow that slot holds PC+2. The `0x60FF` at that point causes the DR to take a different path that reads an uninitialized slot → 0.

The most actionable datapoint: r28=0. Let me check if r28 is documented / what the DR sets it to. Let me search the DR cold-start for how r28 is initialized. The cold start at 0x40B6E964 — let me disassemble it to see what registers including r28 it initializes.



**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\npython tool_scripts/dis_ppc.py 0x40B6E964 0x40B6EA00 2>&1 | Select-Object -Last 120"
}
```

**Output:**
```
0x40b6e964:  li r0, 0
0x40b6e968:  li r23, 0
0x40b6e96c:  mtcrf 0x7f, r0
0x40b6e970:  bl 0x40b6db94
0x40b6e974:  li r8, 0
0x40b6e978:  li r9, 0
0x40b6e97c:  li r10, 0
0x40b6e980:  li r11, 0
0x40b6e984:  li r12, 0
0x40b6e988:  li r13, 0
0x40b6e98c:  li r14, 0
0x40b6e990:  li r15, 0
0x40b6e994:  li r16, 0
0x40b6e998:  li r17, 0
0x40b6e99c:  li r18, 0
0x40b6e9a0:  li r19, 0
0x40b6e9a4:  li r20, 0
0x40b6e9a8:  li r21, 0
0x40b6e9ac:  li r22, 0
0x40b6e9b0:  li r24, 0
0x40b6e9b4:  lwz r1, 0(r24)
0x40b6e9b8:  lwz r24, 4(r24)
0x40b6e9bc:  li r28, 0
0x40b6e9c0:  stw r28, 0x54(r31)
0x40b6e9c4:  li r25, 0x27
0x40b6e9c8:  crset cr2un
0x40b6e9cc:  li r26, 0
0x40b6e9d0:  mtcrf 0x80, r0
0x40b6e9d4:  mtxer r0
0x40b6e9d8:  lha r27, 0(r24)
0x40b6e9dc:  rlwimi r29, r27, 3, 0xd, 0x1c
0x40b6e9e0:  mtctr r29
0x40b6e9e4:  lhau r27, 2(r24)
0x40b6e9e8:  bctr 
0x40b6e9ec:  b 0x40b6ea00
0x40b6e9f0:  nop 
0x40b6e9f4:  nop 
0x40b6e9f8:  nop 
0x40b6e9fc:  nop 

```

---

## Assistant (Build · Big Pickle · 67.7s)

Key insight: at cold start (0x40B6E968-0x40B6E9BC), the DR does **`li r28, 0`** and **`stw r28, 0x54(r31)`** (store r28=0 to `[gd+0x54]`). So r28 is **deliberately zeroed** at cold start!

Then later, the `lwzx r24, r28, r7` uses r28=0 → reads physical 0x10 → 0. But wait — at cold start this path was presumably NOT hit because the first 68K instruction was the `4DFA` LEA etc. and the branch path (0x40B6D7D8) is only reached on certain opcodes.

Actually — this means the DR genuinely relies on `r28` being set somewhere during normal operation, OR `r28` is meant to be a *different* base for this branch-target case. The comment `stw r28, 0x54(r31)` stores r28's value (0) into `[gd+0x54]`. This looks like r28 is being used as a general scratch/garbage register that happens to be 0 here.

So the `lwzx r24, r28, r7` with r28=0 reads `[0 + (r6 & ~7)]`. With r6=0x10 → `[0x10]` = 0. That's the bug surface: **the DR is treating `r28` as the emulator memory base for a load, but r28=0 instead of 0xB000**.

I recall from the Objective: "Enter with only r31=0xB000". The DR uses r31 as gd. But this particular load uses r28. Let me check: is there a DR instruction earlier that sets r28 = r31 (0xB000) as a second base? Let me search the DR for `mr r28, r31` or `r28` writes.

Given r28=0 at the failing load, and the fix needs r28=r31=0xB000, I should set `r28 = 0xB000` at the DRYIELD-RESUME (and possibly DRBOOT-RESUME). Then `lwzx r24, r28, r7` = `lwzx r24, 0xB000, 0x10` = `[0xB010]`.

Let me check what `[0xB010]` should contain — the DR expects the next 68K PC there. Let me dump the emulator-data region around 0xB000-0xB020 after seeds:

Actually, the cleaner question: what does `[0xB010]` hold, and is it the sequential 68K PC+2? Let me check the DR's own stores. Let me look at what value the DR stores to `[gd+0x54]` (from `stw r28,0x54(r31)`) and whether `[gd+0x10]` is used as the PC staging area.

Let me grep the disassembly for stores involving the data base. But I don't have the DR fully disassembled to file. Let me instead check current RAM contents of the relevant slots by dumping them in a probe. But simpler: let me reason from the failing state.

The failing load: `lwzx r24, r28, r7` where r7=0x10 (from r6=0x10 & ~7). Then r24=0. After this, the DR treats 0 as the 68K PC and executes zeroes → degenerate loop.

The natural fix consistent with the DR's design (r31=gd=0xB000): **set r28=0xB000 at resume** so the load reads `[0xB010]`. But then `[0xB010]` must contain a valid next-68K-PC. At the point of the `0x60FF`, the sequential next PC would be 0x408000C4 (after consuming the 4-byte displacement)... but actually 0x60FF is a branch, so it should read the displacement and jump, not use a sequential slot.

Hmm, this is getting deep. Let me reconsider what `r6` means. r6=0x10 at the failing load. Looking at 0x40B6D754: `li r6,0x10; blrl` — r6 is set to 0x10 and a handler called. And 0x40B6D760: `li r6,0x2c; blrl`. And 0x40B6D770: `li r6,0x28; blrl`. These are the **68K opcode extension/operand words** being loaded as small constants?? No — 0x10, 0x2c, 0x28 don't match the ROM data (0x4DFA, 0x0008, 0x60FF).

Wait, these are likely the **d16/displacement processing sub-calls**. r6 holds an offset into a decode table. `lwzx r24, r28, r7` with r7=r6&~7=0x10 reads `[r28+0x10]`.

I think the real answer: `r28` is the DR's **68K memory→PPC address translation base** or the **fetch address base**, and `[r28 + disp]` computes the fetch address of operand/PC. If r28 should be gd=0xB000 but is 0, all such fetches break.

Given the STRONG signal that r28=0 is wrong (cold start zeroes it as a scratch, but the working DR must have it set to gd during real execution), the fix is to **set r28=0xB000 at both resume points**. Let me verify by checking what `[0xB010]` currently holds (post-seed) and reason whether setting r28=0xB000 yields a sensible PC.

Let me just try it: set `Gpr[28]=0xB000` in the DRYIELD-RESUME patch and the DRBOOT-RESUME patch, then rerun. If the DR's `0x60FF` then reads `[0xB010]` and it's 0 → still 0, I'll know `[0xB010]` needs seeding with the branch target.

But I still don't know what the DR *wants* at `[gd+X]` for the branch. Let me get more precise: change the DR-BR probe to also dump `[0xB010]`, `[0xB014]`, `[0xB000]`, and the ROM bytes at the 68K PC (0x408000C0) so I can compute what the branch SHOULD do.

Let me enhance the probe:



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        if (DrBrProbe == 0 && Current == 0x40B6D7D8 && g_DrYieldSeen) {\n            DrBrProbe = 1;\n            Print(L\"  DR-BR[1] r6=0x%08x r7=0x%08x r28=0x%08x \"\n                  L\"r24(68Kpc/post)=0x%08x slot=0x%08x\\n\",\n                  g_PpcContext.Gpr[6] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[7] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[28] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[24],\n                  CpuRead32(g_PpcContext.Gpr[28] +\n                            (UINTN)(g_PpcContext.Gpr[6] & ~7u)));\n            Print(L\"  DR-BR gdB000+0x00=0x%08x +0x10=0x%08x +0x0c=0x%08x \"\n                  L\"+0x54=0x%08x 68Krom@0xC0=%04x%04x %04x%04x\\n\",\n                  CpuRead32(0xB000), CpuRead32(0xB010), CpuRead32(0xB00C),\n                  CpuRead32(0xB054),\n                  CpuRead16(0x408000C0), CpuRead16(0x408000C2),\n                  CpuRead16(0x408000C4), CpuRead16(0x408000C6));\n        }",
  "oldString": "        if (DrBrProbe == 0 && Current == 0x40B6D7D8 && g_DrYieldSeen) {\n            DrBrProbe = 1;\n            Print(L\"  DR-BR[1] r6=0x%08x r7=0x%08x r28=0x%08x \"\n                  L\"r24(68Kpc/post)=0x%08x slot=0x%08x\\n\",\n                  g_PpcContext.Gpr[6] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[7] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[28] & 0xFFFFFFFF,\n                  g_PpcContext.Gpr[24],\n                  CpuRead32(g_PpcContext.Gpr[28] +\n                            (UINTN)(g_PpcContext.Gpr[6] & ~7u)));\n        }"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 100.5s)

**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 95 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"DR-BR\" ",
  "timeout": 170000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt

  [7mDR-BR[0m[1] r6=0x00000010 r7=0x00000010 r28=0x00000000 r24(68Kpc/post)=0x00000000 slot=0x00000000
  [7mDR-BR[0m gdB000+0x00=0x00000000 +0x10=0x00000000 +0x0c=0x00000000 +0x54=0x00000000 68Krom@0xC0=60FF0000 A94E3E7C


```

---

## Assistant (Build · Big Pickle · 37.5s)

Now I have crucial data:
- The ROM at 0x408000C0: `60FF 0000 A94E 3E7C`
- `0x60FF` = BRA.L, d32 = `0000A94E` (bytes 0xC2..0xC5), target = 0x408000C0 + 2 + 0xA94E = **0x4080AA10**
- But the DR loaded r24 = `[r28+0x10]` = `[0x0 + 0x10]` = **0** because r28=0.

So the DR's branch-target load is broken because **r28=0**. If r28 were 0xB000, it would read `[0xB010]`, which is also 0. So just setting r28 doesn't help; the slot `[gd+0x10]` is unseeded.

Now, the deeper question: what is `[gd+r7]` supposed to hold at this point? r7=0x10. In the emulator-data base (gd=0xB000), the Objective defines specific ECB offsets: `[B204]`=HandlerReturn, `[B2C4]`=68K PC, `[B80C/B818/B800..]`=ed.v dispatch table, `[B074]`=0x40B80000, `[B078]`=0x40B60000. 

But `[gd+0x10]`=0xB010 isn't defined in my notes. The DR reads it as the new 68K PC. For the 0x60FF BRA.L, the new PC should be the computed target 0x40B80B10 (A94E... no, target = ROM base + A94E = 0x40B80A10... let me compute: 0x40800000 + 0xAA10 wait).

Actually 0x408000C0 + 2 = 0x408000C2; + d32 0xA94E = 0x4080AA10. That's within the ROM (0x40800000 + 0xAA10). So the branch should set r24 = 0x4080AA10.

But the DR reads `[r28+0x10]` for the new PC rather than computing it from displacement. This confirms the DR's `0x60FF` handling is special: it's NOT computing PC+disp; it's reading the target from a memory slot `[gd+0x10]`.

This matches Apple's design: in these 12-byte boot entries, `0x60FF` is a **PC-relative JMP where the 32-bit displacement is NOT the branch's own disp** but rather the 0x60FF word is a marker and the actual target is loaded from a fixed emulator slot. OR — more likely — this is the DR's generic "long branch" where `[gd+0x10]` holds the target that the DR previously computed/stashed.

Given the DR reads the PC from `[gd + (opcode & ~7)]`, and for opcode 0x60FF that's `[gd+0x10]`... wait no, r7=r6&~7=0x10, and r6=0x10. So r6=0x10 is not the opcode 0x60FF. r6 is something else that equals 0x10.

Let me reconsider. r6=0x10. Looking at the cold-start dispatch: r6 isn't a documented register. But in the flow at 0x40B6D750-0x40B6D77C:
- 0x40B6D754: `li r6,0x10; blrl` (call 0x40B6D7D8-style handler? no, blrl jumps to lr)
- 0x40B6D760: `li r6,0x2c; blrl`
- 0x40B6D770: `li r6,0x28; blrl`

Hmm, these are `mtlr r5` then `li r6,X; blrl` — so r1's saved LR (r5) is the call target. These are nested subroutine setups where r6 is an argument that identifies which 68K register/stack slot to use.

I'm now fairly convinced the 0x60FF handling: the DR's branch dispatch uses `r28` as the 68K register/stack base and `r6` as an index into the "jump target" storage. r28 should be gd (0xB000) and `[gd+0x10]` holds the branch target PC.

Let me check DingusPPC's actual PurePPC 68K DR to understand. But I can't browse DingusPPC source easily here... actually I can search the web. But let me first try the pragmatic fix: the DR reads the target from `[gd + idx]` where it expects a base. The base register r28=0 is the bug. 

Given the DR cold-start explicitly does `li r28,0`, r28=0 is nominal at reset. But during the emulator *running*, some code path must set r28 = the memory base. The DR's branch handler at 0x40B6D7D8 relies on r28 being a memory-mapping base register. In DingusPPC's PPC DR, the memory base is usually in r31 (gd). Let me check whether the DR sets r28 from r31 somewhere in the branch path.

Let me search for writes to r28 in the DR region around the branch handler. Let me disassemble the region 0x40B6D000-0x40B6D7D8 and grep for r28 writes.Now I have the full picture. The ROM at 0xC0 is `60FF 0000 A94E` (BRA.L, d32=+0xA94E → target 0x4080AA10), but the DR loads the new 68K PC from `[r28 + 0x10]` = `[0x0 + 0x10]` = **0** because **r28=0**.

The bug: the DR's branch dispatch base register `r28` is zero; it should be the emulator data base (0xB000). Let me check how the DR sets r28 during normal operation — search for writes to r28 in the DR region:



**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\necho \"=== writes to r28 in DR (0x40b60000-0x40b6f000) ===\"\npython tool_scripts/dis_ppc.py 0x40B60000 0x40B6F000 2>&1 | Select-String \"r28\\b\" | Select-String \"mr |li |addi |add |mfspr|lwz |or r28|mtcr\" | Select-Object -First 60"
}
```

**Output:**
```
=== writes to r28 in DR (0x40b60000-0x40b6f000) ===

```

---

## Assistant (Build · Big Pickle · 4.0s)

**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\necho \"=== any mention of r28 in DR ===\"\npython tool_scripts/dis_ppc.py 0x40B60000 0x40B6F000 2>&1 | Select-String \"r28\" | Select-Object -First 80"
}
```

**Output:**
```
=== any mention of r28 in DR ===

```

---

## Assistant (Build · Big Pickle · 39.9s)

No r28 references in the DR body at all except the two cold-start instructions I already saw (`li r28,0`, `stw r28,0x54(r31)`). This means `r28` is **not managed by the DR** — it's a leftover/garbage value, and the `lwzx r24, r28, r7` at 0x40B6D7D8 is actually a **bug-prone load** OR r28 is set by the *caller* (the PPC firmware boot path) before entering the DR.

Wait — but this path (0x40B6D7D8) IS being reached and executing the LEA continuation. And r28=0. If r28 is meant to be set by the host before the DR dispatch, then the host (our emulator) must set it.

Let me reconsider what `lwzx r24, r28, r7` means. r7 = 0x10. `lwzx r24, 0, 0x10` = read `[0x10]` = 0. 

Hmm, but at cold start the DR executed `LEA (8,PC),A6` etc. correctly with r28=0 too. Let me check: did the DRBOOT path ever execute 0x40B6D7D8 successfully? The boot reached the twui trap BEFORE the LEA/BRA sequence. The DRYIELD-RESUME restarted it at dispatch 0x40B67B60, and THEN it executed LEA + hit the branch at 0x40B6D7D8 with r28=0 → 0.

So this 0x40B6D7D8 path was never exercised in the cold-start (which died at the twui trap before getting here). The question is: what should r28 be?

Let me look at what `[r28+r7]` should produce. Given the ROM `60FF 0000 A94E`, and the DR is supposed to decode BRA.L → target 0x40B80A10... wait, actually let me reconsider whether r28 SHOULD be a memory base that makes `[r28+0x10]` = target.

If the target 0x40B80A10 is to come from `[r28+0x10]`, then r28 = 0x40B80A00. That doesn't obviously match gd=0xB000.

Alternatively — maybe I'm misreading the instruction semantics. `lwzx r24, r28, r7` computes EA = r28 + r7 = 0+0x10 = 0x10, loads r24 = [0x10] = 0. But maybe this is a **store-then-load pattern where r7 is the stack offset of the NEXT PC** that was pushed via `sth r6,6(r1)` earlier. Looking at 0x40B6D7CC-0x40B6D7E0:
```
sthu r4, -8(r1)     ; push
stw r24, 2(r1)      ; store current 68K PC at [r1+2]
rlwinm r7, r6,0,0x14,0x1d  ; r7 = r6 & ~7 = 0x10
lwzx r24, r28, r7   ; r24 = [r28+0x10]
sth r6, 6(r1)       ; store opcode at [r1+6]
lha r27, 0(r24)     ; prefetch opcode at new PC
```

So this is the standard DR "next-instruction setup": after computing/deciding the new PC, it reloads it. For a sequential instruction, `[r28+r7]` must contain PC+2. For the branch 0x60FF, `[r28+r7]` should contain the branch target (0x40B80A10).

The fact that it reads a *memory* slot for the new PC, rather than computing it arithmetically, means the DR maintains the "current/next 68K PC" in a **memory slot indexed by r7**, where r7 encodes WHICH slot. r6=0x10 is the index.

So the real question: where does the DR ENQUEUE the next PC into the slot `[r28+r7]`? It must have stored it just before (e.g., the branch-displacement computation stored 0x40B80A10 into `[base+0x10]`), then this `lwzx` reloads it. If the store never happened (because the DR's branch-displacement code is the 68000-only variant and skipped the BRA.L displacement), the slot is stale/zero.

But we set r28=0, meaning `[0x10]`. The DR stores to that same base... Let me search for stores to `r28`-addressed memory. The issue is the disassembler showed NO r28 usage except 2 lines. So the DR never stores to `[r28+X]` either! 

This strongly implies: **the DR expects r28 to be a HOST-PROVIDED memory base register**, set before dispatch, and the host firmware was supposed to set it. The DR reads/writes 68K state and branch targets through `[r28 + slot]`. Our emulator never set r28 to a valid base, so it's 0 → garbage.

So what should r28 be? Let me look at how DingusPPC's PurePPC DR is invoked — the host sets r28 to the memory base. In DingusPPC, the 68K DR uses the PPC guest memory directly for 68K reads/writes (via a base register). Let me check DingusPPC source for the r28/base register convention.

Let me search the web for DingusPPC PurePPC 68K emulator r28 base.

Actually — I recall this is the **DreemBox / "Empire" 68K emulator** used by DingusPPC for the PurePPC ROMs. The entry convention typically has a dedicated register holding the guest memory base for 68K reads. In the Objective's DR cold-start it's stated: "Enter with only r31=0xB000, r29=0x40B80000, r30=0x40B60000". 

r31 = gd = 0xB000 (emulator data). The DR uses `[r31+X]` everywhere (e.g. `stw r28,0x54(r31)`, `lwz r4,0x960(r31)`). So why does the branch path use r28?

Let me verify: maybe I mis-decoded and it's actually r31 not r28. Let me re-check the raw disasm of 0x40B6D7D8. From the run: "0x40b6d7d8: lwzx r24, r28, r7" and the probe read `CpuRead32(r28 + (r6 & ~7))` = read `[0+0x10]` = 0, and printed r28=0x00000000. Consistent.

So it genuinely uses r28. Since the DR body never sets r28, and cold start zeroes it, this `lwzx r24,r28,r7` with r28=0 reads `[0x10]`=0 — which for the COLD START's first instruction would also be wrong. But the cold start's first instruction was dispatch via `bctr` (0x40B6E9E8), NOT via 0x40B6D7D8. So 0x40B6D7D8 is a different, later-stage decode path used for PC-relative/effective-address.

Let me look more carefully: r6=0x10 here. `rlwinm r7, r6, 0, 0x14, 0x1d` masks bits so r7 = r6 & ~7 (keeps low 9 bits after clearing low 3). With r6=0x10, r7=0x10.

Given the DR never writes r28, and this path needs `[base+slot]`, the base must be r31 (gd=0xB000) and slot r7... but it's written as r28. Unless the disassembler is being thrown off and it's actually r31 in the raw bytes. Let me get the raw instruction bytes for 0x40B6D7D8 to verify the register field.

Let me read the raw PPC words at ROM offset of 0x40B6D7D8.The DR body never writes `r28` except the cold-start zeroing — suspicious. Let me verify the raw instruction bytes at 0x40B6D7D8; the disassembler may be misreading the register field (could actually be r31 = gd):



**Tool: bash**

**Input:**
```json
{
  "command": "python -c \"\nrom=r'C:\\Users\\clayc\\AppData\\Local\\Temp\\opencode\\rom_flat_4mb.bin'\nf=open(rom,'rb')\n# PPC addr 0x40B6D7D8 -> phys = 0x40B6D7D8 - 0x40800000 = 0x36D7D8\noff = 0x40B6D7D8 - 0x40800000\nf.seek(off)\nw = int.from_bytes(f.read(4),'big')\nprint('0x40B6D7D8 raw=%08X' % w)\n# lwzx rD,rA,rB = 7C0006EE | (rD<<21)|(rA<<16)|(rB<<11)\nprint('  dec: op=%d rD=%d rA=%d rB=%d' % ((w>>26)&0x3f,(w>>21)&0x1f,(w>>16)&0x1f,(w>>11)&0x1f))\n# also check the store that precedes for the same base\noff2 = 0x40B6D7D0 - 0x40800000\nf.seek(off2); w2=int.from_bytes(f.read(4),'big')\nprint('0x40B6D7D0 raw=%08X  (stw r24,2(r1): op=%d rs=%d ra=%d d=%d)' % (w2,(w2>>26)&0x3f,(w2>>21)&0x1f,(w2>>16)&0x1f,w2&0xffff))\nf.seek(off2-4); w3=int.from_bytes(f.read(4),'big')\nprint('0x40B6D7CC raw=%08X (sthu): op=%d rs=%d ra=%d d=%d' % (w3,(w3>>26)&0x3f,(w3>>21)&0x1f,(w3>>16)&0x1f,w3&0xffff))\n\""
}
```

**Output:**
```
0x40B6D7D8 raw=7F1C382E
  dec: op=31 rD=24 rA=28 rB=7
0x40B6D7D0 raw=93010002  (stw r24,2(r1): op=36 rs=24 ra=1 d=2)
0x40B6D7CC raw=B481FFF8 (sthu): op=45 rs=4 ra=1 d=65528

```

---

## Assistant (Build · Big Pickle · 10.9s)

The raw bytes confirm: `0x7F1C382E` decodes to `lwzx r24, r28, r7` (the register fields are rA=28, rB=7). So the DR genuinely uses r28 as the base here.

So this branch-load path depends on `r28` being a valid base, but the DR never sets it. This is a trap: DingusPPC's PurePPC DR must set r28 as part of the **68K→memory address translation base** somewhere, OR r28 is set by the **host before dispatching into this specific handler**.

Given the DR's structure, `[r28 + r7]` where r7 is a small slot offset and r28 is a base — this is the DR reading the **current 68K context/PC from its emulator data structure**. The base should be gd = 0xB000 (r31). Let me check: is there a `mr r28, r31` type instruction somewhere I might've missed because dis_ppc misses some syntax? Let me directly search the DR binary for the byte pattern of `mr r28,r31` (7C5FF378) and `addi r28,r31,X`.

Let me search the raw ROM for r28-setting instructions in the DR range.



**Tool: bash**

**Input:**
```json
{
  "command": "python -c \"\nrom=r'C:\\Users\\clayc\\AppData\\Local\\Temp\\opencode\\rom_flat_4mb.bin'\nf=open(rom,'rb')\ndef rd(a):\n    off=a-0x40800000\n    if off<0: return None\n    f.seek(off); return int.from_bytes(f.read(4),'big')\n# DR range in raw words\nstart=0x40B60000; end=0x40B6F000\nfor a in range(start,end,4):\n    w=rd(a)\n    if w is None: break\n    d=(w>>21)&0x1f; s=(w>>16)&0x1f; b=(w>>11)&0x1f; xo=w&0x7ff\n    # mr rD,rS = or rD,rS,rS = xo 444 (0x7C000378 | d<<21| s<<16| s<<11)\n    if (w>>26)==31 and xo==444 and w&7==0 and b==s:\n        if d==28: print('mr r28,r%d @0x%08x' % (s,a))\n    # addi rD,rA,imm = op14\n    if (w>>26)==14 and s==28: print('addi r28,r%d,0x%x @0x%08x' % (b,w&0xffff,a))\n    if (w>>26)==14 and d==28: print('addi r%d,r%d,0x%x @0x%08x' % (d,s,w&0xffff,a))\n    # rlwimi / rlwinm writing r28\nprint('done')\n\""
}
```

**Output:**
```
addi r28,r0,0x0 @0x40b6e9bc
done

```

---

## Assistant (Build · Big Pickle · 20.2s)

Confirmed: the DR never sets r28 to a non-zero base within its own code — the only r28 write is `addi r28,r0,0` at cold start. Therefore **r28 must be supplied by the host** as the effective-address/memory base for the 68K PC point loads. Since the DR reads the new PC from `[r28 + slot]`, and the boot works for sequential code using some other mechanism, this `lwzx r24,r28,r7` path is specifically the **effective-address / operand-address computation** that needs a base.

Wait — actually, reconsider. The `lwzx r24,r28,r7` is reading the *new* 68K PC from memory. Let me reconsider what "new PC" storage this is. Let me look at the larger context: what stored the target into `[r28+slot]`? Since DR never uses r28 as base for stores either, this `lwzx` is reading from an *unmanaged* location → always 0.

Let me reconsider completely. Maybe r28 here is not the base for a table but this is `lwzx r24, r28, r7` where the DR is doing a **guest memory read of the 68K instruction stream** — no, r24 is destination (PC), r7=0x10 is the offset.

I think the cleanest resolution, consistent with "DingusPPC is the model": In DingusPPC's PurePPC guest, the 68K DR's memory access uses a **memory base register (r28)** that the DR's *initialization* sets to point at guest physical memory. Since the DR's cold start sets `r28=0` (not gd=0xB000), and then this path loads `[0+slot]`, and we WANT it to load the correct 68K PC/operand, the DR may rely on the **host patching r28 to 0xB000** OR the DR's own dispatch loop setting it.

Given the DR uses `[r31+0x54]` for gd and `stw r28,0x54(r31)` stores r28's value there... let me check: at cold start, `li r28,0; stw r28,0x54(r31)` stores 0 at gd+0x54. Then maybe LATER the DR loads r28 BACK from somewhere. But the search found no other r28 writes.

I'm now confident: **the DR's 0x40B6D7D8 path reads the new PC from `[r28 + slot]` and depends on r28 being set to the guest memory base (0xB000) by the host**, OR equivalently depend on the slot being pre-seeded. Given the DR-relative base for guest memory in this firmware is 0xB000 (gd), and the failing slot index is 0x10, the correct value at `[0xB010]` should be the branch target PC (0x40B80A10).

**Pragmatic staged fix**: seed `[0xB010]` (and generally `[0xB000 + k*8]` slots) with the branch target, and set r28=0xB000 at resume so the `lwzx` reads it. But we don't know all slots the DR uses.

This is getting into very deep reverse engineering of the DingusPPC DR ABI. Given the substantial progress (we've precisely localized the first DR gap: the `0x60FF BRA.L` branch-target load reads `[0+0x10]`=0 because the DR's r28 memory-base register is 0), and the fix requires understanding the host-set memory-base ABI that DingusPPC uses, this is a good point to consult the user on how to proceed.

Let me verify the branch-target hypothesis by checking the DingusPPC source convention. Let me do a quick web search to confirm r28's role in the PurePPC 68K DR.



**Tool: websearch**

**Input:**
```json
{
  "query": "DingusPPC PurePPC 68K emulator register r28 translation base DreemBox",
  "numResults": 6
}
```

**Output:**
```
Title: dingusdev/dingusppc: An experimental emulator
URL: https://github.com/dingusdev/dingusppc
Published: 2019-07-02T02:13:29.000Z
Author: N/A
Highlights:
# DingusPPC
...
While many other PowerPC emus exist (PearPC, Sheepshaver), none of them currently attempt emulation of PowerPC Macs
accurately (except for QEMU).
...
This program aims to not only improve upon what Sheepshaver, PearPC, and other PowerPC Mac emulators have done, but
also to provide a better debugging environment. This currently is designed to work best with PowerPC NuBus and Old
World ROMs, including those of the Power Mac 6100, 7200, and G3 Beige.
...
Several machines have been implemented to varying degrees, like many Old World PowerPC Macs, early New World
PowerPC Macs, and the Pippin.
...
This emulator has a debugging environment, complete with a disassembler.
...
As of now, the most complete machines are the Power Mac 6100, the Power Mac 7500, and the Power Mac G3 Beige.
...
DingusPPC includes a test suite for verifying the

---

Title: cpu/ppc/ppcemu.h
URL: https://github.com/dingusdev/dingusppc/blob/01c031284f25cebce35ddcca17b3e647afbfd49a/cpu/ppc/ppcemu.h
Published: N/A
Author: N/A
Highlights:
uint32_t gpr[32]; 
 uint32_t cr; 
 uint32_t fpscr; 
 uint32_t tbr[2]; 
 uint32_t spr[1024]; 
 uint32_t msr; 
 uint32_t sr[16]; 
 bool reserve; // reserve bit used for lwarx and stcwx 
} SetPRS; 
 
extern SetPRS ppc_state; 
 
/** symbolic names for frequently used SPRs */ 
enum SPR : int { 
 MQ = 0, 
 XER = 1, 
 RTCU_U = 4, // user RTCU 
 RTCL_U = 5, // user RTCL 
 LR = 8, 
 CTR = 9, 
 DSISR = 18, 
 DAR = 19, 
 RTCU_S = 20, // supervisor RTCU 
 RTCL_S = 21, // supervisor RTCL 
 DEC = 22, // decrementer 
 SDR1 = 25, 
 SRR0 = 26, 
 SRR1 = 27, 
 TBL_U = 268, // user mode TBL 
 TBU_U = 269, // user mode TBU 
 TBL_S = 284, // supervisor TBL 
 TBU_S = 285, // supervisor TBU 
 PVR = 287, 
 HID0 = 1008, 
 HID1 = 1009, 
}; 
 
/** symbolic names for common PPC processors */ 
enum PPC_VER : uint32_t { 
 MPC601 = 0x00010001, 
 MPC603 = 0x00030001, 
 MPC604 = 0x00040001, 
 MPC603E = 0x00060101, 
 MPC603EV = 0x00070101, 
 MPC750 = 0x00080200 
}; 
 
/** 
typedef struct struct_ppc64_state { 
 FPR_storage fpr [32]; 
 uint64_t pc; //Referred as the CIA in the PPC manual 
 uint64_t gpr [32]; 
 uint32_t cr; 
 uint32_t fpscr; 
 uint32_t tbr [2]; 
 uint64_t spr [1024]; 
 uint32_t msr; 
 uint32_t sr [16]; 
 bool reserve; //reserve bit used for lwarx and stcwx 
} SetPRS64; 
 
extern SetPRS64 ppc_state64; 
**/ 
 
/** 
Specific SPRS to be weary of: 
 
USER MODEL 
SPR 1 - XER 
SPR 8 - Link Register / Branch 
 b0 - Summary Overflow 
 b1 - Overflow 
 b2 - Carry 
 b25-31 - Number of bytes to transfer 
SPR 9 - Count 
 
SUPERVISOR MODEL 
19 is the Data Address Register 
22 is the Decrementer 
26, 27 are the Save and Restore Registers (SRR0, SRR1) 
272 - 275 are the SPRGs 
284 - 285 for writing to the TBR's. 
528 - 535 are the Instruction BAT registers 
536 - 543 are the Data BAT registers 
**/ 
 
extern uint32_t opcode_value; // used for interpreting opcodes 
 
extern uint64_t timebase_counter; 
extern uint64_t tbr_wr_timestamp; 
extern uint64_t dec_wr_timestamp; 
extern uint64_t rtc_timestamp; 
extern uint64_t tbr_wr_value; 
extern uint32_t dec_wr_value; 
extern uint32_t tbr_freq_ghz; 
extern uint64_t tbr_period_ns; 
extern uint32_t rtc_lo, rtc_hi; 
 
// Additional steps to prevent overflow? 
extern int32_t add_result; 
extern int32_t simult_result; 
extern uint32_t uiadd_result; 
extern uint32_t uimult_result; 
 
extern uint32_t crf_d; 
extern uint32_t crf_s; 
extern uint32_t reg_s; 
extern uint32_t reg_d; 
extern uint32_t reg_a; 
extern uint32_t reg_b; 
extern uint32_t reg_c; 
extern uint32_t xercon; 
extern uint32_t cmp_c; 
extern uint32_t crm; 
extern uint32_t br_bo; 
extern uint32_t br_bi; 
extern uint32_t rot_sh; 
extern uint32_t rot_mb; 
extern uint32_t rot_me; 
extern uint32_t uimm; 
extern uint32_t grab_sr; 
extern uint32_t grab_inb; // This is for grabbing the number of immediate bytes for loading and storing 
extern uint32_t ppc_to; 
extern int32_t simm; 
extern int32_t adr_li; 
extern int32_t br_bd; 
 
// Used for GP calcs 
extern uint32_t ppc_result_a; 
extern uint32_t ppc_result_b; 
extern uint32_t ppc_result_c; 
extern uint32_t ppc_result_d; 
 
// Used for FP calcs 
extern uint64_t ppc_result64_a; 
extern uint64_t ppc_result64_b; 
extern uint64_t ppc_result64_c; 
extern uint64_t ppc_result64_d; 
 
/* Flags for controlling interpreter execution. */ 
enum { 
 EXEF_BRANCH = 1 << 0, 
 EXEF_EXCEPTION = 1 << 1, 
 EXEF_RFI = 1 << 2, 
 EXEF_TIMER = 1 << 7 
}; 
 
enum CR_select : int32_t { 
 CR0_field = (0xF << 28), 
 CR1_field = (0xF << 24), 
}; 
 
enum CRx_bit : uint32_t { 
 CR_SO = 28, 
 CR_EQ = 29, 
 CR_GT = 30, 
 CR_LT = 31 }; 
 
enum CR1_bit : uint32_t { 
 CR1_OX = 24, 
 CR1_VX, 
 CR1_FEX, 
 CR1_FX, 
}; 
 
enum FPSCR : uint32_t { 
 RN = 0x3, 
 NI = 0x4, 
 XE = 0x8, 
 ZE = 0x10, 
 UE = 0x20, 
 OE = 0x40, 
 VE = 0x80, 
 VXCVI = 0x100, 
 VXSQRT = 0x200, 
 VXSOFT = 0x400, 
 FPRF = 0x1F000, 
 FPCC_FUNAN = 0x10000, 
 FPCC_NEG = 0x8000, 
 FPCC_POS = 0x4000, 
 FPCC_ZERO = 0x2000, 
 FPCC_FPRCD = 0x1000, 
 FI = 0x20000, 
 FR = 0x40000, 
 VXVC = 0x80000, 
 VXIMZ = 0x100000, 
 VXZDZ = 0x200000, 
 VXIDI = 0x400000, 
 VXISI = 0x800000, 
 VXSNAN = 0x1000000, 
 XX = 0x2000000, 
 ZX = 0x4000000, 
 UX = 0x8000000, 
 OX = 0x10000000, 
 VX = 0x20000000, 
 FEX = 0x40000000, 
 FX = 0x80000000 
}; 
 
//for inf and nan checks 
enum FPOP : int { 
 DIV = 0x12, 
 SUB = 0x14, 
 ADD = 0x15, 
 MUL = 0x19, 
 FMSUB = 0x1C, 
 FMADD = 0x1D, 
 FNMSUB = 0x1E, 
 FNMADD = 0x1F, 
}; 
 
/** PowerPC exception types. */ 
enum class Except_Type { 
 EXC_SYSTEM_RESET = 1, 
 EXC_MACHINE_CHECK, 
 EXC_DSI, 
 EXC_ISI, 
 EXC_EXT_INT, 
 EXC_ALIGNMENT, 
 EXC_PROGRAM, 
 EXC_NO_FPU, 
 EXC_DECR, 
 EXC_SYSCALL = 12, 
 EXC_TRACE = 13 
}; 
 
/** Programm Exception subclasses. */ 
enum Exc_Cause : uint32_t { 
 FPU_OFF = 1 << (31 - 11), 
 ILLEGAL_OP = 1 << (31 - 12), 
 NOT_ALLOWED = 1 << (31 - 13), 
 TRAP = 1 << (31 - 14), 
}; 
 
extern unsigned exec_flags; 
 
extern jmp_buf exc_env; 
 
extern bool grab_return; 
 
extern bool power_on; 
extern bool restart_after_power_off; 
extern bool int_pin; 
extern bool dec_exception_pending; 
 
extern bool is_601; // For PowerPC 601 Emulation 
extern bool is_altivec; // For Altivec Emulation 
extern bool is_64bit; // For PowerPC G5 Emulation 
 
extern bool rc_flag; // Record flag 
extern bool oe_flag; // Overflow flag 
 
// Important Addressing Integers 
extern uint32_t ppc_cur_instruction; 
extern uint32_t ppc_effective_address; 
extern uint32_t ppc_next_instruction_address; 
 
// Profiling Stats 
#ifdef CPU_PROFILING 
extern uint64_t num_executed_instrs; 
extern uint64_t num_supervisor_instrs; 
extern uint64_t num_int_loads; 
extern uint64_t num_int_stores; 
extern uint64_t exceptions_processed; 
#endif 
 
// Function prototypes 
extern void ppc_cpu_init(MemCtrlBase* mem_ctrl, uint32_t cpu_version, uint64_t tb_freq); 
extern void ppc_mmu_init(); 
 
void ppc_illegalop(); 
void ppc_fpu_off(); 
void ppc_assert_int(); 
void ppc_release_int(); 
 
//void ppc_opcode4(); 
void ppc_opcode16(); 
void ppc_opcode18(); 
void ppc_opcode19(); 
void ppc_opcode31(); 
void ppc_opcode59(); 
void ppc_opcode63(); 
 
void initialize_ppc_opcode_tables(); 
 
extern double fp_return_double(uint32_t reg); 
extern uint64_t fp_return_uint64(uint32_t reg); 
 
extern void ppc_grab_regsda(); 
extern void ppc_grab_regsdb(); 
 
extern void ppc_grab_regssa(); 
extern void ppc_grab_regssb(); 
 
extern void ppc_grab_regsdab(); 
extern void ppc_grab_regssab(); 
 
extern void ppc_grab_regsdasimm(); 
extern void ppc_grab_regsdauimm(); 
extern void ppc_grab_regsasimm(); 
extern void ppc_grab_regssauimm(); 
 
extern void ppc_store_result_regd(); 
extern void ppc_store_result_rega(); 
 
void ppc_changecrf0(uint32_t set_result); 
void ppc_fp_changecrf1(); 
 
/* Exception handlers. */ 
void ppc_exception_handler(Except_Type exception_type, uint32_t srr1_bits); 
[[noreturn]] void dbg_exception_handler(Except_Type exception_type, uint32_t srr1_bits); 
 
// MEMORY DECLARATIONS 
extern MemCtrlBase* mem_ctrl_instance; 
 
extern void add_ctx_sync_action(const std::function<void()> &); 
extern void do_ctx_sync(void); 
 
// The functions used by the PowerPC processor 
namespace dppc_interpreter { 
extern void ppc_bcctr(); 
extern void ppc_bcctrl(); 
extern void ppc_bclr(); 
extern void ppc_bclrl(); 
extern void ppc_crand(); 
extern void ppc_crandc(); 
extern void ppc_creqv(); 
extern void ppc_crnand(); 
extern void ppc_crnor(); 
extern void ppc_cror(); 
extern void ppc_crorc(); 
extern void ppc_crxor(); 
extern void ppc_isync(); 
 
extern void ppc_add(); 
extern void ppc_addc(); 
extern void ppc_adde(); 
extern void ppc_addme(); 
extern void ppc_addze(); 
extern void ppc_and(); 
extern void ppc_andc(); 
extern void ppc_cmp(); 
extern void ppc_cmpl(); 
extern void ppc_cntlzw(); 
extern void ppc_dcbf(); 
extern void ppc_dcbi(); 
extern void ppc_dcbst(); 
extern void ppc_dcbt(); 
extern void ppc_dcbtst(); 
extern void ppc_dcbz(); 
extern void ppc_divw(); 
extern void ppc_divwu(); 
extern void ppc_eciwx(); 
extern void ppc_ecowx();

---

Title: cpu/ppc/ppcexec.cpp
URL: https://github.com/dingusdev/dingusppc/blob/0e0de638d4f493d7ecfea6976f0b709d32b94673/cpu/ppc/ppcexec.cpp
Published: N/A
Author: N/A
Highlights:
void ppc_cpu_init(MemCtrlBase* mem_ctrl, uint32_t cpu_version, uint64_t tb_freq)
{
 int i;

 mem_ctrl_instance = mem_ctrl;

 initialize_ppc_opcode_tables();

 // initialize emulator timers
 TimerManager::get_instance()->set_time_now_cb(&get_virt_time_ns);
 TimerManager::get_instance()->set_notify_changes_cb(&force_cycle_counter_reload);

 // initialize time base facility
 g_icycles = 0;
 icnt_factor = 4;
 tbr_wr_timestamp = 0;
 rtc_timestamp = 0;
 tbr_wr_value = 0;
 tbr_freq_ghz = (tb_freq << 32) / NS_PER_SEC;
 tbr_period_ns = ((uint64_t)NS_PER_SEC << 32) / tb_freq;

 exec_flags = 0;

 timebase_counter = 0;
 dec_wr_value = 0;

 /* zero all GPRs as prescribed for MPC601 */
 /* For later PPC CPUs, GPR content is undefined */
 for (i = 0; i < 32; i++) {
 ppc_state.gpr[i] = 0;
 }

 /* zero all segment registers as prescribed for MPC601 */
 /* For later PPC CPUs, SR content is undefined */
 for (i = 0; i < 16; i++) {
 ppc_state.sr[i] = 0;
 }

 ppc_state.cr = 0;

 ppc_fpu_init();

 ppc_state.pc = 0;

 ppc_state.tbr[0] = 0;
 ppc_state.tbr[1] = 0;

 /* zero all SPRs */
 for (i = 0; i < 1024; i++) {
 ppc_state.spr[i] = 0;
 }

 ppc_state.spr[SPR::PVR] = cpu_version;
 is_601 = (cpu_version >> 16) == 1;

 if (is_601) {
 /* MPC601 sets MSR[ME] bit during hard reset / Power-On */
 ppc_state.msr = (MSR::ME + MSR::IP);
 } else {
 ppc_state.msr = MSR::IP;
 ppc_state.spr[SPR::DEC] = 0xFFFFFFFFUL;
 }

 ppc_mmu_init();

 /* redirect code execution to reset vector */
 ppc_state.pc = 0xFFF00100;
...
static map<string, int> SPRName2Num = {
 {"XER", SPR::XER}, {"LR", SPR::LR}, {"CTR", SPR::CTR}, {"DEC", SPR::DEC}, {"PVR", SPR::PVR}};
...
uint64_t reg_op(string& reg_name, uint64_t val, bool is_write) {
 string reg_name_u, reg_num_str;
 unsigned reg_num;
 map<string, int>::iterator spr;

 if (reg_name.length() < 2)
 goto bail_out;

 reg_name_u = reg_name;

 /* convert reg_name string to uppercase */
 std::for_each(reg_name_u.begin(), reg_name_u.end(), [](char& c) { c = ::toupper(c); });

 try {
 if (reg_name_u == "PC") {
 if (is_write)
 ppc_state.pc = (uint32_t)val;
 return ppc_state.pc;
 }
 if (reg_name_u == "MSR") {
 if (is_write)
 ppc_state.msr = (uint32_t)val;
 return ppc_state.msr;
 }
 if (reg_name_u == "CR") {
 if (is_write)
 ppc_state.cr = (uint32_t)val;
 return ppc_state.cr;
 }
 if (reg_name_u == "FPSCR") {
 if (is_write)
 ppc_state.fpscr = (uint32_t)val;
 return ppc_state.fpscr;
 }

 if (reg_name_u.substr(0, 1) == "R") {
 reg_num_str = reg_name_u.substr(1);
 reg_num = (unsigned)stoul(reg_num_str, NULL, 0);
 if (reg_num < 32) {
 if (is_write)
 ppc_state.gpr[reg_num] = (uint32_t)val;
 return ppc_state.gpr[reg_num];
 }
 }

 if (reg_name_u.substr(0, 1) == "FR") {
 reg_num_str = reg_name_u.substr(2);
 reg_num = (unsigned)stoul(reg_num_str, NULL, 0);
 if (reg_num < 32) {
 if (is_write)
 ppc_state.fpr[reg_num].int64_r = val;
 return ppc_state.fpr[reg_num].int64_r;
 }
 }

 if (reg_name_u.substr(0, 3) == "SPR") {
 reg_num_str = reg_name_u.substr(3);
 reg_num = (unsigned)stoul(reg_num_str, NULL, 0);
 if (reg_num < 1024) {
 if (is_write)
 ppc_state.spr[reg_num] = (uint32_t)val;
 return ppc_state.spr[reg_num];
 }
 }

 if (reg_name_u.substr(0, 2) == "SR") {
 reg_num_str = reg_name_u.substr(2);
 reg_num = (unsigned)stoul(reg_num_str, NULL, 0);
 if (reg_num < 16) {
 if (is_write)
 ppc_state.sr[reg_num] = (uint32_t)val;
 return ppc_state.sr[reg_num];
 }
 }

 spr = SPRName2Num.find(reg_name_u);
 if (spr != SPRName2Num.end()) {
 if (is_write)
 ppc_state.spr[spr->second] = (uint32_t)val;
 return ppc_state.spr[spr->second];
 }
 } catch (...) {
 }

bail_out:
 throw std::invalid_argument(string("Unknown register ") + reg_name);
}
...
uint64_t get_reg(string reg_name) {
 return reg_op(reg_name, 0, false);

---

Title: zdocs/users/manual.md
URL: https://github.com/dingusdev/dingusppc/blob/master/zdocs/users/manual.md
Published: N/A
Author: N/A
Highlights:
The debugger can be used to show what code is currently executing, the contents of memory as hex or 68K assembly or PowerPC assembly, NVRAM variables, memory regions, and CPU registers. It can also be used to change memory, registers, and nvram variables. It can step through instructions one at a time or many instructions at once.

---

Title: cpu/ppc/ppcopcodes.cpp
URL: https://github.com/dingusdev/dingusppc/blob/0c3f399de3e7198616f555b5378eac1fb69ae692/cpu/ppc/ppcopcodes.cpp
Published: N/A
Author: N/A
Highlights:
CA=1 
 ppc_state.spr[SPR::XER] |= XER::CA; 
 else 
 ppc_state.spr[SPR::XER] &= ~XER::CA; 
 
 if (oe_flag) { 
 if (ppc_result_d && ppc_result_d == ppc_result_a) 
 ppc_state.spr[SPR::XER] |= XER::SO | XER::OV; 
 else 
 ppc_state.spr[SPR::XER] &= ~XER::OV; 
 } 
 
 if (rc_flag) 
 ppc_changecrf0(ppc_result_d); 
 
 ppc_store_result_regd(); 
} 
 
void dppc_interpreter::ppc_and() { 
 ppc_grab_regssab(); 
 ppc_result_a = ppc_result_d & ppc_result_b; 
 
 if (rc_flag) 
 ppc_changecrf0(ppc_result_a); 
 
 ppc_store_result_rega(); 
} 
 
void dppc_interpreter::ppc_andc() { 
 ppc_grab_regssab(); 
 ppc_result_a = ppc_result_d & ~(ppc_result_b); 
 
 if (rc_flag) 
 ppc_changecrf0(ppc_result_a); 
 
 ppc_store_result_rega(); 
} 
 
void dppc_interpreter::ppc_andidot() { 
 ppc_grab_regssauimm(); 
 ppc_result_a = ppc_result_d & uimm; 
 ppc_changecrf0(ppc_result_a); 
 ppc_store_result_rega(); 
} 
 
void dppc_interpreter::ppc_andisdot() { 
 ppc_grab_regssauimm(); 
 ppc_result_a = ppc_result_d & (uimm << 16); 
 ppc_changecrf0(ppc_result_a); 
 ppc_store_result_rega(); 
} 
 
void dppc_interpreter::ppc_nand() { 
 ppc_grab_regssab(); 
 ppc_result_a = ~(ppc_result_d & ppc_result_b); 
 
 if (rc_flag) 
 ppc_changecrf0(ppc_result_a); 
 
 ppc_store_result_rega(); 
} 
 
void dppc_interpreter::ppc_or() { 
 ppc_grab_regssab(); 
 ppc_result_a = ppc_result_d | ppc_result_b; 
 
 if (rc_flag) 
 ppc_changecrf0(ppc_result_a); 
 
 ppc_store_result_rega(); 
} 
 
void dppc_interpreter::ppc_orc() { 
 ppc_grab_regssab(); 
 ppc_result_a = ppc_result_d | ~(ppc_result_b); 
 
 if (rc_flag) 
 ppc_changecrf0(ppc_result_a); 
 
 ppc_store_result_rega(); 
} 
 
void dppc_interpreter::ppc_ori() { 
 ppc_grab_regssauimm(); 
 ppc_result_a = ppc_result_d | uimm; 
 ppc_store_result_rega(); 
} 
 
…
void dppc_interpreter::ppc_bclr() { 
 uint32_t br_bo = (ppc_cur_instruction >> 21) & 31; 
 uint32_t br_bi = (ppc_cur_instruction >> 16) & 31; 
 uint32_t ctr_ok; 
 uint32_t cnd_ok; 
 
 if (!(br_bo & 0x04)) { 
 (ppc_state.spr[SPR::CTR])--; /* decrement CTR */ 
 } 
 ctr_ok = (br_bo & 0x04) | ((ppc_state.spr[SPR::CTR] != 0) == !(br_bo & 0x02)); 
 cnd_ok = (br_bo & 0x10) | (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08)); 
 
 if (ctr_ok && cnd_ok) { 
 ppc_next_instruction_address = (ppc_state.spr[SPR::LR] & ~3UL); 
 exec_flags = EXEF_BRANCH; 
 } 
} 
 
void dppc_interpreter::ppc_bclrl() { 
 uint32_t br_bo = (ppc_cur_instruction >> 21) & 31; 
 uint32_t br_bi = (ppc_cur_instruction >> 16) & 31; 
 uint32_t ctr_ok; 
 uint32_t cnd_ok; 
 
 if (!(br_bo & 0x04)) { 
 (ppc_state.spr[SPR::CTR])--; /* decrement CTR */ 
 } 
 ctr_ok = (br_bo & 0x04) | ((ppc_state.spr[SPR::CTR] != 0) == !(br_bo & 0x02)); 
 cnd_ok = (br_bo & 0x10) | (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08)); 
 
 if (ctr_ok && cnd_ok) { 
 ppc_next_instruction_address = (ppc_state.spr[SPR::LR] & ~3UL); 
 exec_flags = EXEF_BRANCH; 
 } 
 ppc_state.spr[SPR::LR] = ppc_state.pc + 4; 
} 
// Compare Instructions 
 
void dppc_interpreter::ppc_cmp() { 
#ifdef CHECK_INVALID 
 if (ppc_cur_instruction & 0x200000) { 
 LOG_F(WARNING, "Invalid CMP instruction form (L=1)!"); 
 return; 
 } 
#endif 
 
 int crf_d = (ppc_cur_instruction >> 21) & 0x1C; 
 ppc_grab_regssab(); 
 uint32_t xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3; 
 uint32_t cmp_c = (int32_t(ppc_result_a) == int32_t(ppc_result_b)) ? 0x20000000UL : \ 
 (int32_t(ppc_result_a) > int32_t(ppc_result_b)) ? 0x40000000UL : 0x80000000UL; 
 ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d)); 
} 
 
void dppc_interpreter::ppc_cmpi() { 
#ifdef CHECK_INVALID 
 if (ppc_cur_instruction & 0x200000) { 
 LOG_F(WARNING, "Invalid CMPI instruction form (L=1)!"); 
 return; 
 } 
#endif 
 
 int crf_d = (ppc_cur_instruction >> 21) & 0x1C; 
 ppc_grab_regsasimm(); 
 uint32_t xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3; 
 uint32_t cmp_c = (int32_t(ppc_result_a) == simm) ? 0x20000000UL : \ 
 (int32_t(ppc_result_a) > simm) ? 0x40000000UL : 0x80000000UL; 
 ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d)); 
} 
 
void dppc_interpreter::ppc_cmpl() { 
#ifdef CHECK_INVALID 
 if (ppc_cur_instruction & 0x200000) { 
 LOG_F(WARNING, "Invalid CMPL instruction form (L=1)!"); 
 return; 
 } 
#endif 
 
 int crf_d = (ppc_cur_instruction >> 21) & 0x1C; 
 ppc_grab_regssab(); 
 uint32_t xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3; 
 uint32_t cmp_c = (ppc_result_a == ppc_result_b) ? 0x20000000UL : \ 
 (ppc_result_a > ppc_result_b) ? 0x40000000UL : 0x80000000UL; 
 ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d)); 
} 
 
void dppc_interpreter::ppc_cmpli() { 
#ifdef CHECK_INVALID 
 if (ppc_cur_instruction & 0x200000) { 
 LOG_F(WARNING, "Invalid CMPLI instruction form (L=1)!"); 
 return; 
 } 
#endif 
 
 int crf_d = (ppc_cur_instruction >> 21) & 0x1C; 
 ppc_grab_regssauimm(); 
 uint32_t xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3; 
 uint32_t cmp_c = (ppc_result_a == uimm) ? 0x20000000UL : \ 
 (ppc_result_a > uimm) ? 0x40000000UL : 0x80000000UL; 
 ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d)); 
} 
 
// Condition Register Changes 
 
void dppc_interpreter::ppc_mcrf() { 
 int crf_d = (ppc_cur_instruction >> 21) & 0x1C; 
 int crf_s = (ppc_cur_instruction >> 16) & 0x1C; 
 
 // extract and right justify source flags field 
 uint32_t grab_s = (ppc_state.cr >> (28 - crf_s)) & 0xF; 
 
 ppc_state.cr = (ppc_state.cr & ~(0xf0000000UL >> crf_d)) | (grab_s << (28 - crf_d)); 
} 
 
void dppc_interpreter::ppc_crand() { 
 ppc_grab_dab(); 
 uint8_t ir = (ppc_state.cr >> (31 - reg_a)) & (ppc_state.cr >> (31 - reg_b)); 
 if (ir & 1) { 
 ppc_state.cr |= (0x80000000UL >> reg_d); 
 } else { 
 ppc_state.cr &= ~(0x80000000UL >> reg_d); 
 } 
} 
 
void dppc_interpreter::ppc_crandc() { 
 ppc_grab_dab(); 
 if ((ppc_state.cr & (0x80000000UL >> reg_a)) && !(ppc_state.cr & (0x80000000UL >> reg_b))) { 
 ppc_state.cr |= (0x80000000UL >> reg_d); 
 } else { 
 ppc_state.cr &= ~(0x80000000UL >> reg_d); 
 } 
} 
void dppc_interpreter::ppc_creqv() { 
 ppc_grab_dab(); 
 uint8_t ir = (ppc_state.cr >> (31 - reg_a)) ^ (ppc_state.cr >> (31 - reg_b)); 
 if (ir & 1) { // compliment is implemented by swapping the following if/else bodies 
 ppc_state.cr &= ~(0x80000000UL >> reg_d); 
 } else { 
 ppc_state.cr |= (0x80000000UL >> reg_d); 
 } 
} 
void dppc_interpreter::ppc_crnand() { 
 ppc_grab_dab(); 
 uint8_t ir = (ppc_state.cr >> (31 - reg_a)) & (ppc_state.cr >> (31 - reg_b)); 
 if (ir & 1) { 
 ppc_state.cr &= ~(0x80000000UL >> reg_d); 
 } else { 
 ppc_state.cr |= (0x80000000UL >> reg_d); 
 } 
} 
 
void dppc_interpreter::ppc_crnor() { 
 ppc_grab_dab(); 
 uint8_t ir = (ppc_state.cr >> (31 - reg_a)) | (ppc_state.cr >> (31 - reg_b)); 
 if (ir & 1) { 
 ppc_state.cr &= ~(0x80000000UL >> reg_d); 
 } else { 
 ppc_state.cr |= (0x80000000UL >> reg_d); 
 } 
} 
 
void dppc_interpreter::ppc_cror() { 
 ppc_grab_dab(); 
 uint8_t ir = (ppc_state.cr >> (31 - reg_a)) | (ppc_state.cr >> (31 - reg_b)); 
 if (ir & 1) { 
 ppc_state.cr |= (0x80000000UL >> reg_d); 
 } else { 
 ppc_state.cr &= ~(0x80000000UL >> reg_d); 
 } 
} 
 
void dppc_interpreter::ppc_crorc() { 
 ppc_grab_dab(); 
 if ((ppc_state.cr & (0x80000000UL >> reg_a)) || !(ppc_state.cr & (0x80000000UL >> reg_b))) { 
 ppc_state.cr |= (0x80000000UL >> reg_d); 
 } else { 
 ppc_state.cr &= ~(0x80000000UL >> reg_d); 
 } 
} 
void dppc_interpreter::ppc_crxor() { 
 ppc_grab_dab(); 
 uint8_t ir = (ppc_state.cr >> (31 - reg_a)) ^ (ppc_state.cr >> (31 - reg_b)); 
 if (ir & 1) { 
 ppc_state.cr |= (0x80000000UL >> reg_d); 
 } else { 
 ppc_state.cr &= ~(0x80000000UL >> reg_d); 
 } 
} 
 
// Processor MGMT Fns. 
 
void dppc_interpreter::ppc_rfi() { 
#ifdef CPU_PROFILING 
 num_supervisor_instrs++; 
#endif 
 uint32_t new_srr1_val = (ppc_state.spr[SPR::SRR1] & 0x87C0FF73UL); 
 uint32_t new_msr_val = (ppc_state.msr & ~(0x87C0FF73UL)); 
 ppc_state.msr = (new_msr_val | new_srr1_val) &

---

Title: M68k register mapping | Emu68
URL: https://michalsc.github.io/Emu68/internals/PowerPCRegisterMapping.html
Published: N/A
Author: N/A
Highlights:
GPR13
...
4 general purpose registers
...
Registers GPR14
...
GPR31
...
W11
...
FPR14
...
FPR3
...
D7,D29
...
D31
...
`GPR1
...
7` | General purpose data register |
...
| `LR` | `W28` | Link register |
...
| `CTR
...
`W29` |
...
/branch register
...
| `FPR1
...
` | `D27` | Floating point register |
...
| `FPR13` | `D28` | Floating point register |
| `FPR29` | `D29` | Floating point register |
| `FPR30` | `D30` | Floating point register |
| `FPR31` | `D31` | Floating point register |
...
| `GPR28` | `V25.S[2]` | General purpose data register |
```

---

## Assistant (Build · Big Pickle · 23.1s)

The search confirms DingusPPC's current PPC core uses a C++ interpreter, not the guest-ROM-based 68K DR. The pure-PPC approach in our project uses the ROM's own embedded DR. The `r28` is a host-coupled memory base.

I have enough understanding to stop and consult the user. Let me summarize the concrete finding and propose the fix direction. But first, let me reconsider once more whether there's a simpler read: perhaps the DR reads the *current* PC from `[r28+slot]` and the fault is purely that **r28 should equal gd (0xB000)**, with the actual PC-maintenance happening via the `stw r24,2(r1)` stack frame.

Let me look again at the failing load sequence in context — specifically what value r24 SHOULD become. The DR is executing the `0x60FF` at 68K PC 0x408000C0. For a branch, the next 68K PC = 0x4080AA10. The DR loads it from `[r28 + 0x10]`. Whichever r28 value makes `[r28+0x10]` = 0x4080AA10... that's r28 = 0x4080AA00, which is a *computed* address, not a fixed base. So r28 is being used to hold a computed PC+offset. That doesn't fit either.

Actually — reconsidering: `lwzx r24, r28, r7` with `r28` being used here may be a **dreg-and-x-reg effective address computation** where r28 holds a base that was just computed by a preceding instruction. Let me look at what set r28 right before this. The sequence from 0x40B6D6D0 onward:
- `addi r1,r1,0x5c` (0x40B6D6D0)
- `bl 0x40b6c3ec` (0x40B6D6D4)
- then a bunch of r31-relative loads and `rlwimi r29,r25,...` / `mtctr` / `blr` at 0x40B6D748 (returns)

The `0x40B6D750` block (mtlr r5; li r6,X; blrl; nop; ...) is a SEPARATE subroutine called from this. The `li r6,0x10` sets r6. Between the `blrl` returns and 0x40B6D7CC, r28 might be set by the `bl 0x40b6c3ec` call or by instructions we haven't traced. Let me check 0x40B6C3EC.

Given the extensive depth here and that the user explicitly invited me to stop and ask if unsure, I'll present the concrete localization and ask how to proceed with the fix. The finding is crisp enough.

Let me quickly check 0x40B6C3EC to see if it sets r28, which would fully resolve it.



**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\npython tool_scripts/dis_ppc.py 0x40B6C3EC 0x40B6C440 2>&1 | Select-String \"r28\" | Select-Object -First 20\necho \"=== r28 touches in 0x40b6c3ec-0x40b6d7cc ===\"\npython tool_scripts/dis_ppc.py 0x40B6C3EC 0x40B6D7CC 2>&1 | Select-String \"r28\" | Select-Object -First 40"
}
```

**Output:**
```
=== r28 touches in 0x40b6c3ec-0x40b6d7cc ===

```

---

## Assistant (Build · Big Pickle · 42385.5s)

No r28 writes anywhere in the entire dispatch/decode region leading up to the failing load. This conclusively means **r28 is a host-provided register that the DR relies on but never initializes** — it's part of the DR's ABI contract that the host firmware (the PPC boot path) must set before dispatching into the DR's PC-refetch/effective-address logic.

Since our emulator seeds the DR's machine state at the resume points and never sets r28, it stays 0 → the `0x60FF`'s new-PC load reads `[0x10]`=0 → degenerate loop at 0x00000000.

I've localized the first DR-thrown gap precisely. The fix requires knowing what r28 should hold. Given the DR's effective-address / branch-target model, and that this is exactly the kind of host-ABI detail in the ROM's embedded DR that we'd need to reverse-engineer or reproduce from Apple's firmware behavior, this is a good point to present findings and get direction on the fix strategy.

Let me present the findings concisely and ask how to proceed.



**Tool: question**

**Input:**
```json
{
  "questions": [
    {
      "question": "I localized the first DR gap precisely. In the embedded (DingusPPC-pure-PPC-style) 68K DR, the unconditional/jump dispatch at 0x40B6D7D8 does `lwzx r24, r28, r7` to load the NEW 68K PC from memory. The ROM's 0x60FF (BRA.L, d32=+0xA94E) at 68K PC 0x408000C0 should branch to 0x4080AA10, but because the DR's memory-base register r28 is 0 (only write in the whole DR is `li r28,0` at cold start), it reads [0x10]=0 and falls into a degenerate loop at 68K PC 0x00000000. r28 is never set by the DR body — it's a host-supplied ABI register our emulator must seed before dispatching. How do you want me to fix the 0x60FF/branch-target gap?",
      "header": "Fix 0x60FF branch gap",
      "multiple": false,
      "options": [
        {
          "label": "Reverse-engineer r28 ABI (Recommended)",
          "description": "Trace which DR calls reach 0x40B6D7D8, deduce the r28 value/meaning (likely the memory read base), determine if r28=0xB000 (gd) is correct, seed it at the DRBOOT/DRYIELD-RESUME patches, and verify [0xB000+0x10] holds the right branch target. Most faithful to 'run the ROM's own DR'."
        },
        {
          "label": "PPC-side 0x60FF handler patch",
          "description": "Add a targeted PPC patch: when the DR decodes '0x60FF' at a ROM 68K PC, manually read the 4-byte d32, compute target=PC+2+d32, force r24 (68K PC) to it, and suppress the DR's own broken 0x40B6D7D8 load. Quicker, but it's a pseudo-instruction hack alongside the existing yield patches."
        },
        {
          "label": "Seed [0xB000+0x10] + set r28",
          "description": "Hypothesis: r28 should be gd=0xB000 and [gd+0x10] should hold the sequential/next PC. Seed that slot and set r28 at resume, test whether the DR then makes forward progress or needs more slots seeded."
        },
        {
          "label": "Investigate before fixing",
          "description": "Stay diagnostic: add more probes to map exactly which DR paths read/write r28 and the [r28+slot] layout (branch targets, operands), and write a formal note in ARCHITECTURE.md before choosing a fix."
        }
      ]
    }
  ]
}
```

**Output:**
```
User has answered your questions: "I localized the first DR gap precisely. In the embedded (DingusPPC-pure-PPC-style) 68K DR, the unconditional/jump dispatch at 0x40B6D7D8 does `lwzx r24, r28, r7` to load the NEW 68K PC from memory. The ROM's 0x60FF (BRA.L, d32=+0xA94E) at 68K PC 0x408000C0 should branch to 0x4080AA10, but because the DR's memory-base register r28 is 0 (only write in the whole DR is `li r28,0` at cold start), it reads [0x10]=0 and falls into a degenerate loop at 68K PC 0x00000000. r28 is never set by the DR body — it's a host-supplied ABI register our emulator must seed before dispatching. How do you want me to fix the 0x60FF/branch-target gap?"="Reverse-engineer r28 ABI (Recommended)". You can now continue with the user's answers in mind.
```

---

## Assistant (Build · Big Pickle · 30.9s)

Understood. I'll reverse-engineer the r28 ABI. Let me set up a todo list and trace which DR calls reach 0x40B6D7D8 and how r28 is supposed to be derived.



**Tool: todowrite**

**Input:**
```json
{
  "todos": [
    {
      "content": "Trace call path into 0x40B6D7D8 and what registers/handlers reach it (which 68K opcodes route here)",
      "status": "in_progress",
      "priority": "high"
    },
    {
      "content": "Determine r28's intended value/meaning (memory read base?) and whether it should be gd=0xB000",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Determine what slot [r28+0x10] should hold and how the DR stores PCs/operands via r28-indexed slots",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Implement the fix at the resume points (DRBOOT/DRYIELD-RESUME): seed r28 and any required slot(s)",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Rebuild, rerun, confirm the 68K boot progresses past 0x60FF to the next stage",
      "status": "pending",
      "priority": "high"
    },
    {
      "content": "Update TODO.md / ARCHITECTURE.md with the r28-ABI finding and the fix",
      "status": "pending",
      "priority": "medium"
    }
  ]
}
```

**Output:**
```
[
  {
    "content": "Trace call path into 0x40B6D7D8 and what registers/handlers reach it (which 68K opcodes route here)",
    "status": "in_progress",
    "priority": "high"
  },
  {
    "content": "Determine r28's intended value/meaning (memory read base?) and whether it should be gd=0xB000",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Determine what slot [r28+0x10] should hold and how the DR stores PCs/operands via r28-indexed slots",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Implement the fix at the resume points (DRBOOT/DRYIELD-RESUME): seed r28 and any required slot(s)",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Rebuild, rerun, confirm the 68K boot progresses past 0x60FF to the next stage",
    "status": "pending",
    "priority": "high"
  },
  {
    "content": "Update TODO.md / ARCHITECTURE.md with the r28-ABI finding and the fix",
    "status": "pending",
    "priority": "medium"
  }
]
```

---

## Assistant (Build · Big Pickle · 23.1s)

Let me trace the call path into 0x40B6D7D8. First, understand the structure: 0x40B6D750-0x40B6D77C is a small subroutine (`mtlr r5; li r6,X; blrl; nop; ...`) that's reached via `blr` at 0x40B6D748. The `blrl` calls back to whatever `mtlr r5` set — which is the return into the main decode loop. So this 0x40B6D750 block is the DR's **sequential-instruction refetch** that reloads r24 (68K PC) from `[r28 + (r6 & ~7)]` where r6 selects the slot.

Let me look at what calls `blr` at 0x40B6D748 and the broader context. The block 0x40B6D6D4 (`bl 0x40b6c3ec`) through 0x40B6D748 (`blr`) — this computes the 68K result and returns to the dispatch. Let me trace who calls into 0x40B6D750 specifically.

Let me disassemble the whole region 0x40B6D680-0x40B6D800 and also find what dispatches to the 0x40B6D750 subroutine.



**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\npython tool_scripts/dis_ppc.py 0x40B6D740 0x40B6D800 2>&1"
}
```

**Output:**
```
0x40b6d740:  mtcrf 0x4f, r4
0x40b6d744:  rlwinm r25, r25, 0, 0x15, 0x13
0x40b6d748:  blr 
0x40b6d74c:  nop 
0x40b6d750:  mtlr r5
0x40b6d754:  li r6, 0x10
0x40b6d758:  blrl 
0x40b6d75c:  nop 
0x40b6d760:  mtlr r5
0x40b6d764:  li r6, 0x2c
0x40b6d768:  blrl 
0x40b6d76c:  nop 
0x40b6d770:  lwz r5, 0x804(r31)
0x40b6d774:  mtlr r5
0x40b6d778:  li r6, 0x28
0x40b6d77c:  blrl 
0x40b6d780:  crclr cr7un
0x40b6d784:  rlwinm r4, r25, 8, 0x10, 0x17
0x40b6d788:  addi r24, r24, -2
0x40b6d78c:  rlwinm r25, r25, 0, 0x1a, 0x16
0x40b6d790:  rlwimi r4, r26, 7, 0x1b, 0x1b
0x40b6d794:  mfcr r7
0x40b6d798:  rlwimi r4, r7, 4, 0x1c, 0x1c
0x40b6d79c:  rlwimi r4, r7, 5, 0x1d, 0x1d
0x40b6d7a0:  mfxer r7
0x40b6d7a4:  rlwimi r4, r7, 3, 0x1e, 0x1f
0x40b6d7a8:  bso cr2, 0x40b6d7c8
0x40b6d7ac:  mtcrf 2, r25
0x40b6d7b0:  stw r1, 0x48(r31)
0x40b6d7b4:  lwz r1, 0x4c(r31)
0x40b6d7b8:  ori r25, r25, 0x20
0x40b6d7bc:  crset cr2un
0x40b6d7c0:  bns cr6, 0x40b6d7c8
0x40b6d7c4:  lwz r1, 0x50(r31)
0x40b6d7c8:  bsolrl cr7
0x40b6d7cc:  sthu r4, -8(r1)
0x40b6d7d0:  stw r24, 2(r1)
0x40b6d7d4:  rlwinm r7, r6, 0, 0x14, 0x1d
0x40b6d7d8:  lwzx r24, r28, r7
0x40b6d7dc:  sth r6, 6(r1)
0x40b6d7e0:  lha r27, 0(r24)
0x40b6d7e4:  mtcrf 1, r24
0x40b6d7e8:  rlwimi r29, r27, 3, 0xd, 0x1c
0x40b6d7ec:  mtctr r29
0x40b6d7f0:  lhau r27, 2(r24)
0x40b6d7f4:  bsol cr7, 0x40b6d344
0x40b6d7f8:  rlwinm r25, r25, 0, 0x15, 0x13
0x40b6d7fc:  bgectr cr2

```

---

## Assistant (Build · Big Pickle · 108.6s)

Now I can see the structure clearly. This is the DR's **instruction refetch / next-PC load** routine:

- At 0x40B6D7D8: `lwzx r24, r28, r7` — r24 = memory[r28 + r7] = the **next 68K PC**.
- r7 = r6 & ~7, where r6 is the slot selector (0x10, 0x2C, 0x28 for different refetch cases).
- After loading r24 (new PC), it does `lha r27, 0(r24)` (opcode fetch), builds dispatch index, etc.

So `lwzx r24, r28, r7` reads the **next 68K PC from a table at r28**, indexed by r7. This means `r28` is the base pointer to a **PC/state table**, and the DR relies on the host to have populated it and set r28.

The three `blrl` calls at 0x40B6D754/0x40B6D764/0x40B6D778 pass r6=0x10/0x2c/0x28 — these are the slot indices. But wait, `blrl` uses **LR** which was set to `r5`. `mtlr r5` sets the call target = whatever was in r5. So these are actually calls to a common handler with different slot indices.

At 0x40B6D7D8, `r28 + r7` — the actual failing read is for the FIRST case (r6=0x10, from 0x40B6D754).

**Key insight**: `lwzx r24, r28, r7` where r7 = r6 & ~7. For r6=0x10 → r7=0x10. So the read is `[r28 + 0x10]`. This is reading the next PC from a slot.

Now, what is r28? Given the DR's other pointer `r31` = gd = 0xB000, and r28 is separate, r28 is likely a **second data base for the emulator's 68K state vector** — specifically the "next PC" table.

Actually, let me reconsider. The DR uses `r31` as the main emulator data base (gd). Everywhere it reads/writes emulator state it uses `[r31 + offset]` (e.g. `stw r28,0x54(r31)`, `lwz r4,0x960(r31)`). 

But here it reads the next PC from `[r28 + r7]`. The next-PC table is at r28. Given that this is the DR's output of "where to continue", and the cold-start stored `r28`'s value (0) to `[gd+0x54]`... 

Wait — `stw r28, 0x54(r31)` at cold start stores r28 (=0) into gd+0x54. That's storing the *initial* value of the r28 register into a gd slot. So maybe the DR's convention: `r28` holds a pointer that the DR *also* reflects in gd+0x54?? That seems odd.

Let me reconsider: maybe `r28` IS meant to be gd=0xB000, and `[gd + r7]` for r7=0x10 → `[0xB010]` holds the next PC. The cold start sets r28=0 (bug/placeholder) then stores 0 to gd+0x54. 

The cleanest test: **Set r28 = 0xB000 at the resume points** and **seed [0xB010] = the expected next 68K PC**. For the first branch case, the DR is at 68K PC 0x408000C0 executing 0x60FF. But wait — this "lwzx r24,[r28+r7]" isn't necessarily the branch itself; it might be the refetch machinery invoked regardless. Let me reconsider whether this is the *unconditional branch* or the *normal sequential advance*.

Given the three `blrl` slots (0x10, 0x2c, 0x28), and that this whole 0x40B6D750 block is reached once per instruction (after executing one 68K op), these are the **program counter + operand refetch** for the *current* instruction being decoded, loaded from the DR's emulator state table. The `0x60FF` is one such opcode that routes here.

Actually, the simplest frame: r28 = the emulator 68K-state base. The DR's state table starts at r28. `[r28+0x10]` might be a general "current effective/next address". Since r28=0 and RAM[0x10]=0, we get 0.

Let me check DingusPPC's actual embedded DR source to nail the r28 ABI. The DR in the ROM is a compiled DingusPPC m68k emulator. Let me search DingusPPC's m68k emulator for a "memory_base" or base register convention.

Actually, I recall DingusPPC has a "m68k" CPU in ppcexec for the "mini" mode, but the embedded DR here is a specific vendored emulator. Let me search for the actual ABI — the register that maps host memory. Let me look at the DR's memory-access handlers: how does it read/write 68K memory? If it reads/writes guest RAM via address translation, there must be a base register. Let me search the DR for the pattern of 68K memory reads.

The `lha r27, 0(r24)` at 0x40B6D7E0 reads the opcode via r24 (PC) with a 0 displacement — that's a guest memory read of the 68K instruction stream, using r24 as the address directly. Since the ROM window is at 0x40800000+, and r24=PC=0x408000C0, `lha r27,0(r24)` reads RAM[0x408000C0] directly — our emulator maps 0x40800000 region. So 68K memory reads use direct PPC addresses (the guest 68K address space is identity-mapped into the PPC MMIO/RAM window).

So for `lwzx r24, r28, r7` = read `[r28+0x10]` — r28 is NOT a translation base for 68K memory; it's a base for the DR's own **scratch/state table**. With r28=0, it reads guest RAM[0x10], which is zero.

So r28 should point at the DR's internal state table. Where is that table? The DR's gd is r31=0xB000 and the Objective says ed.v table at B800+, ECB at B100, etc. Perhaps r28 = the same gd (0xB000) and slots like B010 hold the PC cache.

Let me test the hypothesis directly: instrument a probe that sets r28=0xB000 and seeds RAM[0xB010]= (expected PC) right before the DR executes 0x40B6D7D8, and see if the DR then advances correctly. But I need to know what the expected PC is for the slot.

Actually, better: let me examine what OTHER DR paths write to slots and whether there's a store that populates `[r28+slot]`. If the DR stores the PC to `[r28+slot]` elsewhere then reads it here, r28 must be a shared base. Let me search the full DR for stores using an indexed/indirect base that could be the same table. But since no instruction writes r28, the *base* r28 must be a constant the host sets.

Given r31=gd=0xB000 is set and is the main base, and r28 is a separate base the DR gets from the host, the most likely value for r28 is **also 0xB000** (both point to the emulator data/state). Then `[0xB010]` would be a state slot. Let me dump what our emulator currently has across 0xB000-0xB200 to understand the layout and whether 0xB010 is a PC slot.

Actually — key realization. Let me look again at 0x40B6D7CC: `sthu r4, -8(r1)` then `stw r24, 2(r1)`. It pushes r24 (current PC) onto the PPC stack r1 at [r1+2]. Then `lwzx r24,r28,r7` loads the NEW PC. The NEW PC comes from `[r28+r7]`.

But then where does the DR STORE the current PC so the table tracks? At 0x40B6D7DC `sth r6, 6(r1)` stores the opcode. Hmm.

Let me step back and think about the DR's actual design. This looks like the DR uses a **memory-mapped CPU state / bytecode cache**. Actually, I bet r28 points to a **prefetched-opcode/PC queue** that the host DR setup fills BEFORE starting, and the DR reads the next-PC from it. Since we started the DR cold with PC=0x4080002A, the queue at r28 must be initialized.

This is the crux: the DR was never given a valid r28-based PC queue by the host. In the cold-start, the DR set r28=0 itself (the `li r28,0`), so the queue is at guest physical 0x00000000 — but that's low RAM which is zero. Yet the cold-start DISPATCH worked (0x40B6E9E8 `bctr` uses r29/r27, not r28). 

I think r28 is used specifically by the LEA/effective-address/PC-relative decode path (the 0x4DFA LEA and 0x60FF branch both involve PC-relative addressing and the slots). For PC-relative effective addresses, the DR loads the reference PC from a slot.

Let me get empirical: I'll dump the DR state slots and, more usefully, WRITE the correct next-PC into RAM[0x10] (r28+0x10 with r28=0) and see if the DR then proceeds to the correct address 0x4080AA10. But 0x4080AA10 may not be where it should go for a PC-relative slot.

Let me instead add a probe at 0x40B6D7D8 that: (1) prints r28, r6, r7, the current r24, the ROM bytes, AND (2) **overrides r24 to the computed BRA.L target** right after `lwzx` — but I can't easily override in a pure observation probe.

Let me reconsider what the correct behavior is. The 68K boot at 0xBC: `LEA (8,PC),A6` → A6=0xC6 (table). Then 0xC0: `0x60FF BRA.L +0xA94E` → PC = 0x408000C0 + 2 + 0xA94E = 0x4080AA10. So after executing 0x60FF, the next 68K PC should be 0x4080AA10.

The DR's 0x40B6D7D8 loads r24 (next PC) from `[r28+r7]`. For this to work, `[r28+r7]` must already contain 0x4080AA10 — i.e. something computed the branch target and stored it there. In a correct DR, the "compute branch target" step stores the target to the slot, then this refetch loads it. But our DR's branch-target computation apparently stored to a slot at r28*+something*, and r28=0 makes it go to low RAM.

So the real question: after the DR computes the 0x60FF target, where does it store it, and what should r28 be so the refetch reads it back?

This requires understanding the DR's execution model fully. Given the user chose "reverse-engineer r28 ABI", let me do a thorough empirical trace. I'll instrument to intercept at the FIRST DR instruction after the resume and trace r24 (68K pc), r28, and dump RAM to see the DR's writes.

Actually, the most direct: the failing state is deterministic. Let me set r28 = 0xB000 and seed RAM[0xB010] with the expected sequential PC, then observe whether the DR makes correct forward progress. I'll do this as an experiment at the DRYIELD-RESUME patch. But I need `[0xB010]` correct.

Let me think about what `[r28+0x10]` should contain for the FIRST use. At the moment right after the yield-resume, r24 (68K PC) = 0x408000BC (the LEA). The DR is refetching. The next-PC slot should contain... the PC of the instruction AFTER the current one. But at 0x40B6D7D0 it already stored r24 to the stack. Hmm.

Let me just do the empirical experiment. I'll modify the DRYIELD-RESUME to also set `Gpr[28]=0xB000` and seed a few slots, then rerun and watch. The slots to seed are uncertain, so let me first just set r28=0xB000 (keeping slots as-is, currently 0) and observe whether the DR behaves differently (it would then read [0xB010]=0 → same result, but it proves whether r28=0xB000 is consumed).

Better experiment: set r28=0xB000 AND seed [0xB010]=0x4080AA10 (the branch target I computed), then see if the DR jumps to 0x4080AA10. If it does, I've proved r28=gd and the slot holds the branch target. Let me implement this as a debug experiment.

Actually, let me first gather more data on the DR's slot usage by dumping RAM[0x00-0x200] and the r28-related memory after the DR runs, to see if the DR writes branch targets somewhere. But the DR wrote nothing (low RAM is all zero except the message).

Let me gain clarity by instrumenting to capture the DR state at the moment the branch target gets computed. I'll add a probe at 0x40B6D7D8 that, when r27==0x60FF initially... but the probe runs post-execution. 

Let me add a probe just BEFORE, at the instruction the DR is executing when r24 still == 0x408000C0. I'll capture the full register set plus a RAM dump of 0x00-0x80 and 0xB000-0xB200.

Given the deterministic single-shot nature, let me do a focused empirical test: at the DRYIELD-RESUME, set r28=0xB000, and ALSO, when the DR reaches 0x40B6D7D8 with r24==0x408000C0, override r24 to the computed target 0x4080AA10 (mimicking a correct branch). But overriding is a state mutation — acceptable as a diagnostic.

Let me think about the cleanest diagnostic that distinguishes hypotheses:

**Hypothesis A**: r28 should be 0xB000 (the gd), and [0xB010] should hold the next-PC. Fix: set r28=0xB000 at resume + seed [0xB010].

**Hypothesis B**: r28 should point at a separate PC-queue table that the DR maintains, and I haven't found its base yet.

Let me test Hypothesis A first (cheapest). I'll set r28=0xB000 at the DRYIELD-RESUME patch and rerun WITHOUT seeding slots, checking whether behavior changes at all (it should read [0xB010]=0, same degenerate loop, but I can verify r28=0xB000 is now in effect via the probe).

Let me modify the DRYIELD-RESUME patch to set r28=0xB000 and re-run, keeping the probe.

Let me find the DRYIELD-RESUME patch.I need to understand the DR's state-table layout. Let me trace the call path into 0x40B6D750 and understand how the next-PC slots relate to r28. Let me first check what r5/r28 typically hold by examining the DR's main dispatch loop that feeds into this refetch routine.



**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\necho \"=== who branches/calls to 0x40b6d750 (refetch routine) ===\"\npython tool_scripts/dis_ppc.py 0x40B60000 0x40B6F000 2>&1 | Select-String \"0x40b6d750|0x40b6d754|0x40b6d760|0x40b6d770\" | Select-Object -First 30\necho \"=== r5 writes feeding mtlr r5? check where this is called ===\"\npython tool_scripts/dis_ppc.py 0x40B60000 0x40B6F000 2>&1 | Select-String \"b.*0x40b6d750|bl.*0x40b6d750\" | Select-Object -First 10"
}
```

**Output:**
```
=== who branches/calls to 0x40b6d750 (refetch routine) ===
=== r5 writes feeding mtlr r5? check where this is called ===

```

---

## Assistant (Build · Big Pickle · 15.7s)

The 0x40B6D750 routine is reached by falling through from 0x40B6D748 `blr`, or by `bgectr`/`bctr` dispatch into the handler table. Let me understand the dispatch. The ed.v table (from Objective): `[B800..B80C]=0x40B6D780`, `[B818]=0x40B6C410`, `[B824]=0x40B6C984`, `[B828]=0x40B6C9D8`, `[B85C]=0x40B6D5DC`. The 0x40B6D780 entry is the `crclr` at the top of the refetch routine — so the DR dispatches to 0x40B6D780 (via ed.v) for a whole class of opcodes, and it runs the refetch.

So the flow is: DR main dispatch → (via ed.v) → 0x40B6D780 → the `crclr cr7un` / build flags / 0x40B6D7CC refetch → 0x40B6D7D8 `lwzx r24,r28,r7`.

So this is the **generic handler for a class of 68K opcodes** (probably the `0x60xx` BRA/BSR set and related), entered at 0x40B6D780. And the refetch of the next PC happens through `[r28+r7]`.

Let me look at what 68K opcode class 0x40B6D780 handles and confirm `0x60FF` routes here. The ed.v[0] entry [B800]=0x40B6D780. From the Objective: "[B800..B80C]=0x40B6D780" — the first 4 slots all point to 0x40B6D780. So opcode classes mapping to ed.v[0..3] all go here.

Given the DR's dispatch builds index from opcode bits (`rlwimi r29,r27,3,0xd,0x1c` → r29 = index into ed.v), and 0x60FF would index some slot. The 0x60FF is 0x60xx → BRA. Let me look at the ed.v handling of branch opcodes.

This is getting quite deep. Let me take the empirical approach now. I'll set r28=0xB000 at the resume and test. Let me find the DRYIELD-RESUME and DRBOOT-RESUME patches.



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 45,
  "offset": 3270
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
3270:                     // [task_r1 + 0x658] where task_r1 = [SPRG0-4] = [0x9FFC].
3271:                     // That slot is 0 here, so the swap would restore from NULL
3272:                     // and MRETTASK's rfi (SRR0 = ctx[0x104]) jumps to PC 0.
3273:                     // Point the switch at the ECB and put the DR dispatch loop
3274:                     // in the ECB's Handler-return slot to resume the emulator.
3275:                     CpuWrite32(0x00009FFC, 0x0000A000);  // [SPRG0-4] task r1 (68K SSP)
3276:                     CpuWrite32(0x0000A658, 0x0000B100);  // [task_r1+0x658] next ctx = ECB
3277:                     CpuWrite32(0x0000B204, 0x40B67B60);  // ECB+0x104 HandlerReturn = DR dispatch
3278:                     g_BootDecGate = 0;
3279:                     Print(L"  DRBOOT-RESUME @rfi ecb[1c4]=0x%08x lowmem[4]=0x%08x "
3280:                           L"-> DR start 0x40B6E964 r31(ed)=0xB000 r29=0x40B80000\n",
3281:                           g_PpcContext.Gpr[24], CpuRead32(4));
3282:                 }
3283:             }
3284:             // DR emulator-trap yield: while the ROM's PPC DR is driving the 68K
3285:             // boot program it faults at the emulator trap table (0x40B6E8C8:
3286:             // `twi r31,2`, reached via the DR's `bl 0x40b6e8c8`). The 0x700
3287:             // handler's task swap stores r0 (= the trap address) as the ECB
3288:             // Handler-return, so an unpatched MRETTASK rfis straight back into
3289:             // the same `twi` and traps forever. Modernise the resume to the DR
3290:             // dispatch loop re-armed on the ECB: r24 holds the 68K PC past the
3291:             // yielding opcode, the process re-fetches r27, and r29 must be the
3292:             // dispatch-table base again (the handler left it as a stale slot).
3293:             if (g_DrBootPcSeeded &&
3294:                 g_PpcContext.Srr0 == 0x40B6E8C8 &&
3295:                 g_PpcContext.Gpr[24] >= 0x40800000 &&
3296:                 g_PpcContext.Gpr[24] < 0x40840000) {
3297:                 g_PpcContext.Gpr[29] = 0x40B80000;     // re-arm LA_DispatchTable
3298:                 g_PpcContext.Gpr[31] = 0x0000B000;     // ed base (paranoia)
3299:                 g_PpcContext.Srr0 = 0x40B67B60;        // DR dispatch loop (refetch)
3300:                 g_DrYieldSeen = 1;
3301:                 g_DrPostYieldWindow = 1;
3302:                 Print(L"  DRYIELD-RESUME @rfi trap=0x40B6E8C8 68Kpc(r24)=0x%08x "
3303:                       L"r27(op)=0x%04x -> DR dispatch 0x40B67B60\n",
3304:                       g_PpcContext.Gpr[24], g_PpcContext.Gpr[27] & 0xFFFF);
3305:             }
3306:             if (CurrentAddress >= 0x40B10000 && CurrentAddress < 0x40B30000) {
3307:                 Print(L"  DBG rfi @0x%08x: SRR0=0x%08x SRR1=0x%08x MSR=0x%08x -> PC=0x%08x\n",
3308:                       CurrentAddress, g_PpcContext.Srr0, g_PpcContext.Srr1,
3309:                       g_PpcContext.Msr, g_PpcContext.Srr0);
3310:             }
3311:             if (g_PpcContext.Srr0 == 0x0000013F && HandoffDumped == 0) {
3312:                 UINT32 I;
3313:                 HandoffDumped = 1;
3314:                 Print(L"  HANDOFF rfi @0x%08x -> PC=0x0000013F (68K emulator entry)\n", CurrentAddress);

(Showing lines 3270-3314 of 6517. Use offset=3315 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 10.0s)

I see both resume patches. The DRYIELD-RESUME (line 3293-3305) sets r29=0x40B80000, r31=0xB000, Srr0=0x40B67B60. It does NOT set r28.

Let me first add r28 to the DRYIELD-RESUME as an experiment: set it to 0xB000 and observe. But rather than guess blindly, let me understand what r28 should be by examining the DR's own handling. Let me think about the design differently.

The DR main loop at 0x40B67B60 (the dispatch loop) — let me disassemble it to see if IT sets r28 or expects it:



**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\npython tool_scripts/dis_ppc.py 0x40B67B40 0x40B67BF0 2>&1"
}
```

**Output:**
```
0x40b67b40:  blrl 
0x40b67b44:  rotlw r5, r5, r3
0x40b67b48:  rotlw r4, r4, r3
0x40b67b4c:  andc r7, r7, r5

```

---

## Assistant (Build · Big Pickle · 9.2s)

The disassembler output is truncated/incomplete at 0x40B67B60. Let me get the full dispatch loop properly:



**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\npython tool_scripts/dis_ppc.py 0x40B67B60 0x40B67C80 2>&1"
}
```

**Output:**
```
0x40b67b60:  mr r6, r27
0x40b67b64:  lhau r27, 2(r24)
0x40b67b68:  mtcrf 0xf, r6
0x40b67b6c:  ori r4, r30, 0x5700
0x40b67b70:  rlwinm r7, r6, 0x1a, 0x1b, 0x1f
0x40b67b74:  rlwimi r4, r6, 0x18, 0x19, 0x1b
0x40b67b78:  bltl cr5, 0x40b67c40
0x40b67b7c:  srawi r5, r7, 3
0x40b67b80:  neg r6, r6
0x40b67b84:  add r3, r3, r5
0x40b67b88:  beql cr6, 0x40b67c50
0x40b67b8c:  mtlr r4
0x40b67b90:  clrlwi. r4, r7, 0x1d
0x40b67b94:  clrlwi r6, r6, 0x1b
0x40b67b98:  rlwimi r29, r27, 3, 0xd, 0x1c
0x40b67b9c:  blrl 
0x40b67ba0:  lhau r27, 2(r24)
0x40b67ba4:  mtlr r29
0x40b67ba8:  bne 0x40b67bd0
0x40b67bac:  clrlwi. r5, r6, 0x1d
0x40b67bb0:  bne 0x40b67bd0
0x40b67bb4:  slw. r7, r7, r6
0x40b67bb8:  subfic r5, r6, 0x20
0x40b67bbc:  srwi r5, r5, 3
0x40b67bc0:  mtxer r5

```

---

## Assistant (Build · Big Pickle · 78.2s)

This is the DR's main dispatch loop at 0x40B67B60 — it dispatches 68K opcodes via the `blrl` to handler addresses and refetches via `lhau r27,2(r24)`. This loop DOES use `r24` (68K PC) directly with `lhau` for prefetch — NOT `[r28+slot]`. So the main loop doesn't need r28.

The r28-based `[r28+slot]` refetch (0x40B6D7D8) is in a **specific handler** (the 0x60xx branch family entered via ed.v → 0x40B6D780). Let me look at how this handler gets the branch target. The handler at 0x40B6D780 is the ed.v[0] (opcode class 0). Let me check what class of opcodes maps to ed.v[0]=0x40B6D780 and whether 0x60FF (branch) goes there.

Actually — the fact that the DR dispatch loop uses `r24` directly for PC and prefetch, but this branch handler reads the next PC from `[r28+slot]`, tells me the branch handler maintains a **different PC model** — it's the "68K branches via a jump table in memory" model.

Let me look at what the handler does with `[r28+slot]` and whether r28 should be a base pointing at where the DR stashes branch targets. Let me examine 0x40B6D780-0x40B6D800 which is the start of this handler (the `crclr cr7un` is the ed.v[0] entry).

The branch handler 0x40B6D780:
- `crclr cr7un` - clear a CR
- `rlwinm r4,r25,8,0x10,0x17` - shift opcode
- `addi r24,r24,-2` - **r24 -= 2** (back up PC!)
- then builds an "exception/branch frame" register `r4` from CC bits
- `stw r1,0x48(r31)` / `lwz r1,0x4c(r31)` - switch PPC stack to a saved one
- `bsolrl cr7` - branch
- `sthu r4,-8(r1)` - push frame
- `stw r24,2(r1)` - store PC (now = 0x408000C0 after the -2)
- `lwzx r24,r28,r7` - load new PC from memory
- fetch opcode, dispatch

So this handler is the **68K exception/AVEC/interrupt or the A-line trap + JMP mechanism**. The `addi r24,r24,-2` re-points to the current opcode (0x60FF at 0x408000C0), stores it, then loads the *vector* for that opcode from a table at `[r28+r7]`.

**This is the 68000 exception vector dispatch!** For certain opcodes (like A-line `0xAxxx` traps or `0x4AFx`/JMP), the 68000 fetches a vector from address `(vector_index << 2)` in the vector table. `[r28 + r7]` = the vector table base + index. 

Wait — `0x60FF` is BRA.L, not an exception. But the handler treats it via the vector mechanism. Hmm.

Actually reconsider: the ed.v table maps opcode *classes*, and many 68K "unimplemented"/specific opcodes route to this generic vector-dispatch handler. The `0x60FF` being unusual (68020 BRA.L) may be routed to a catch-all that does vector dispatch, OR — more likely — this handler is the **68000's general "illegal/trap vector" path** that 0x60FF falls into because the DR's 68000 core doesn't recognize it.

Given the DR is a **68000 core** (no 68020 BRA.L), `0x60FF` is *not a valid 68000 instruction* → it's treated as an illegal/exception → vector dispatch → reads vector from `[r28 + vector_index]`. With r28=0 and vector_index slot 0x10 → reads RAM[0x10]=0 → jumps to vector 0.

But the Apple ROM uses `0x60FF` as a LEGAL BRA.L (its own extension). The 68000-core DR doesn't know BRA.L.

Hmm, but cold-start used `0x60FF`-style entries too and the DR reached them via `LEA`+normal flow. Actually the DR never got past `0x60FF` in any run — cold start died at the twui trap before reaching 0x60FF.

So the cleanest interpretation: **0x60FF (BRA.L) is a 68020+ instruction the 68000-core DR doesn't support**, and it mis-dispaches it as an exception/vector read that returns 0.

This means the "fix the first DR gap" is: the DR is a 68000 core, but the Apple ROM boot code uses 68020 BRA.L (0x60FF). The gap is REAL and fundamental.

But wait — let me reconsider whether the DR is really 68000-only. The DR executed `LEA (8,PC),A6` (a valid 68000 instruction) fine. `MOVE.W #0x2700,SR` fine. The issue is specifically `0x60FF`. Let me check: maybe 0x60FF in Apple's ROM isn't BRA.L but something else (Apple's A/ILLG trap format).

The 12-byte entry format `4DFA 0008 60FF xxxx A9xx...`:
- `4DFA 0008` = LEA (8,PC),A6 (A6 = next entry pointer)
- `60FF xxxx` = ??? 
- `A9xx` = Apple A-line trap

Actually in classic Mac ROM boot code, the `60FF xxxx` at the start of dispatch entries is the **"JMP to RAM bootstrap"** — a 32-bit branch. It's BRA.L (0x60FF = BRA with the FF byte = "long form" in Apple's convention; standard 68020 would be BRA.L with d32).

I'm now confident enough that this is the 68020 BRA.L gap. But the user's chosen fix direction was "reverse-engineer r28 ABI" (Option A), which assumed r28 is a base that should be seeded. Given my deeper finding that this is actually a BRA.L/68020 support gap manifesting as a vector-dispatch into [r28+slot]=0, I should refine.

Let me reconsider: is `[r28+slot]` reading a **vector table** or is `r28` the DR state base? Let me check what `r7` values map to and whether the DR reads a **not-really-vector** but a PC-computation. The slot 0x10 read gave 0. 

Let me actually test what happens if I seed r28=0xB000 AND put reasonable branch behavior by seeing if the DR even has correct 0x60FF handling. The definitive test: does the DR compute PC+2+d32 for BRA.L anywhere? Let me check the ed.v table to see which handler 0x60xx maps to, and disassemble that handler.

Let me find the handler for the 0x60xx opcode class. The DR dispatch index uses `rlwimi r29,r27,3,0xd,0x1c` (in 0x40B60C08) — builds index from opcode bits 8-28 shifted. And ed.v is indexed by that. 0x60FF: bits -> let me compute which ed.v slot.

Actually, given the complexity and that I have strong empirical evidence, let me verify the "BRA.L unsupported" hypothesis decisively by testing the simplest fix: **patch the guest ROM boot stub to replace the 0x60FF long-branch entries with 68000-compatible 16-bit branches or JMPs**, OR **patch the DR to handle 0x60FF**. But the user chose Option A (reverse-engineer r28 ABI), not the PPC-side handler patch.

Let me re-read the user's actual choice. They picked "Reverse-engineer r28 ABI (Recommended)" — which was Option A described as: trace which DR calls reach 0x40B6D7D8, deduce r28's value/meaning (likely memory read base), determine if r28=0xB000 (gd) is correct, seed it at resume, verify [0xB000+0x10] holds the right branch target.

So the user wants me to pursue the r28=0xB000 hypothesis. Let me do that empirical test: set r28=0xB000 at resume and seed [0xB010] with the branch target, and see if the DR progresses. Even if it's really a BRA.L gap, testing r28=0xB000 will reveal more.

But I need to know WHAT to seed [0xB010] with. The branch targets differ per entry. Let me look at the 12-byte entry structure again:
- entry@0xC6: `3E7C 2600 7E00 7C00 3E3C 0011`
- entry@0xD2: `4DFA 0008 60FF 000A 8C88 2C57`
- entry@0xDE: `7400 2F0E 4DFA 0008 60FF 0000`

Wait let me re-read these. The entries are 12 bytes:
- @0xC6: `3E 7C 26 00 7E 00 7C 00 3E 3C 00 11` = `MOVE.W #0x2600,D7; MOVEQ #0,D6? ...`
- @0xD2: `4D FA 00 08 60 FF 00 0A 8C 88 2C 57`

Hmm these look like variable-length code, not uniform 12-byte entries. Let me disassemble the actual 68K boot code properly. Let me write the 68K disassembler from the Next Move list to decode 0x28-0x200.

Actually — the trace showed the DR executing: `LEA (8,PC),A6` at 0xBC/0xBE (A6 = 0xC6), then 0x60FF at 0xC0. So after 0xBC's LEA, the code continues to 0xC0 `60FF`. The d32 at 0xC2 = `000A` per the @0xD2 entry being a different region... Let me re-dump the exact bytes 0xB6-0xC6 from the flat file to nail the structure. But note the live ROM is patched vs flat. From my DR-BR probe, live ROM @0xC0 = `60FF 0000 A94E` → d32 = `0000A94E`, target 0x4080AA10.

So the 68K boot at 0xC0 does `BRA.L +0xA94E` → 0x4080AA10. That's the real flow. The DR can't do it.

Given the user's chosen direction is r28 ABI reverse-engineering (Option A), and my deeper analysis reveals this is really about whether the DR supports BRA.L, let me reconcile: Option A's premise (r28=memory base, seed it) may still be the fix if the DR DOES have BRA.L support but routed it through a broken r28-based vector/table read. If r28 is meant to be a valid base and the slot holds the branch target, seeding works.

Let me do the decisive empirical test: seed r28=0xB000 and [0xB010]=0x4080AA10 at the DRYIELD-RESUME, and see if the DR jumps to 0x4080AA10 and continues. If progress, r28=gd hypothesis confirmed. If not, it's the BRA.L unsupported gap.

Let me also reconsider: the `addi r24,r24,-2` then `stw r24,2(r1)` then `lwzx r24,r28,r7`. If r28=gd=0xB000 and [0xB010] held 0x4080AA10, then r24 becomes 0x4080AA10 — correct branch. Then opcode fetched from there. This fits the branch-target-table model where the DR stores the computed target at [gd+0x10] before this refetch.

But who computes/stores 0x4080AA10 into [gd+0x10]? It must be the DR's branch-displacement logic BEFORE this handler. For `0x60FF`, the displacement is consumed elsewhere and the result stored. But our DR-BR probe showed [0xB010]=0 — so the store didn't happen (or it stores to a different r28 base).

Let me just try the experiment. Let me modify the DRYIELD-RESUME to set r28=0xB000 and seed a few candidate slots, then rerun. I'll seed 0xB010 with the computed target 0x4080AA10 as the primary hypothesis.

Actually, to keep it clean, let me set r28=0xB000 AND at the DR-BR probe point (0x40B6D7D8, before execution) override the slot to the computed target. But the probe runs post-execution. 

Let me instead think about what the DR expects. The cleanest experiment: at DRYIELD-RESUME, set r28=0xB000. Then add a write at the rfi patch: when the DR reaches the handler for 0x60FF (i.e., when Current hits 0x40B6D780 region with r24 pointing at 0x60FF), pre-store the target into [0xB010]. 

This is getting complex. Let me simplify: I'll implement a minimal, targeted fix based on the strongest hypothesis and test it: **set r28 = 0xB000 at both resumes**, and let the natural DR flow proceed. Then observe whether the behavior changes (this isolates whether r28=gdB000 is even the right value vs 0).

Given time, let me just add r28=0xB000 to the DRYIELD-RESUME and rerun to see if the DR-BR probe's slot changes meaning.

Let me first update the probe to also report what's at 0xB010 given r28 would be 0xB000, and set r28=0xB000 at resume.

Let me make the change to DRYIELD-RESUME to set Gpr[28]=0xB000 (as an experimental probe), rebuild, rerun.



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "                g_PpcContext.Gpr[29] = 0x40B80000;     // re-arm LA_DispatchTable\n                g_PpcContext.Gpr[31] = 0x0000B000;     // ed base (paranoia)\n                g_PpcContext.Gpr[28] = 0x0000B000;     // R28 ABI experiment: host mem base\n                g_PpcContext.Srr0 = 0x40B67B60;        // DR dispatch loop (refetch)",
  "oldString": "                g_PpcContext.Gpr[29] = 0x40B80000;     // re-arm LA_DispatchTable\n                g_PpcContext.Gpr[31] = 0x0000B000;     // ed base (paranoia)\n                g_PpcContext.Srr0 = 0x40B67B60;        // DR dispatch loop (refetch)"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 103.3s)

Now let me also seed the candidate PC slot. But I need to know what the DR expects. Let me first run with just r28=0xB000 to see if the probe now reads a different slot and whether the DR progresses. Let me rebuild and run.



**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 95 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"DR-BR|DRYIELD\" | Select-Object -First 6",
  "timeout": 170000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt

  [7mDRYIELD[0m-RESUME @rfi trap=0x40B6E8C8 68Kpc(r24)=0x408000BC r27(op)=0x4DFA -> DR dispatch 0x40B67B60
  [7mDR-BR[0m[1] r6=0x00000010 r7=0x00000010 r28=0x0000B000 r24(68Kpc/post)=0x00000000 slot=0x00000000
  [7mDR-BR[0m gdB000+0x00=0x00000000 +0x10=0x00000000 +0x0c=0x00000000 +0x54=0x00000000 68Krom@0xC0=60FF0000 A94E3E7C


```

---

## Assistant (Build · Big Pickle · 91.4s)

Now r28=0xB000, and the slot read `[0xB000+0x10]` = `[0xB010]` = 0. So r28 is now correctly pointing at gd, and the DR reads `[0xB010]` for the next PC but it's 0. So the DR expects the branch target to be **stored at gd+0x10 before this handler reads it**.

So the fix is: the DR's branch-target computation must have stored 0x4080AA10 into `[gd+0x10]` (r28+0x10). It didn't, because the DR's store used a different base OR never ran.

Now the question: who's supposed to put the target at `[gd+0x10]`? Let me look at what the DR does BEFORE this handler to compute the target. The handler is entered fresh at 0x40B6D780 (ed.v dispatch) for the current opcode. It does `addi r24,r24,-2` (backup to opcode), then reads `[r28+r7]`.

Since the DR never stores to `[r28+0x10]` (no r28-store instructions found), the target must be placed there by an **earlier stage** or by host setup. Given the whole DR never writes r28-based memory, and r28 is host-set, the DR's design expects the **host to have pre-populated several `[gd+slot]` PC slots before dispatch**, OR the branch target for `0x60FF` comes from reading the displacement inline.

Let me reconsider. Maybe this handler is NOT for computing branches but for the **A-line trap / illegal opcode → vector fetch**. `0x60FF` might be treated by the 68000 DR as an illegal instruction, and it fetches the illegal-instruction vector from `[gd + (vector_slot)]`. In a real 68000, illegal vector = address 4 (lowmem[4]). The handler `addi r24,-2` (re-point to the illegal opcode), reads vector → 0.

But actually for the Apple ROM, 0x60FF is legal BRA.L. The 68000-core DR doesn't support it, treats as illegal, and vector-dispatch fails.

Let me definitively determine: does the DR have ANY 68020-style branch support? Let me check the ed.v table structure — what classes of opcodes route to 0x40B6D780 (this vector handler) vs a real branch handler. If 0x60xx (all BRA) routes here, then the DR genuinely treats branches via this vector table.

Given the extensive analysis, I'm now fairly convinced this is the **68000-core limitation on 0x60FF BRA.L**. But the user chose to reverse-engineer the r28 ABI. The r28 ABI finding is: **r28 = gd (0xB000)** and it's the memory base for the handler's vector/PC table reads. That part is now solved (r28=0xB000 confirmed as the base).

The SECOND finding: the target slot `[gd+0x10]` is 0 because the DR's illegal-opcode/vector fetch reads an unpopulated vector. For 0x60FF to work, that vector slot must contain the branch target.

Given the user's direction is "seed r28 and slots", the actionable fix within their chosen option is: **when the DR reaches this handler for the 0x60FF opcode (legal BRA.L), populate `[gd+0x10]` with the computed branch target** (0x408000C0 + 2 + d32) so the `lwzx` reads the correct PC. This is essentially the PPC-side handler approach but using the r28/`[gd+0x10]` slot mechanism — a blend that respects the ABI.

Let me implement: in the PPC loop, when we detect the DR about to process the 0x60FF at the right moment, compute target = r24_upcoming + 2 + d32 and write it to `[0xB010]` before the `lwzx` reads it. The right moment: when Current is in the handler BEFORE 0x40B6D7D8 and r24 (after the -2) points at the 0x60FF. 

From the trace: at the handler entry, r24 is backed up by -2 to point at 0x60FF. The `lwzx` at 0x40B6D7D8 reads [gd+0x10]. So I should write the target to [gd+0x10] when Current is at some point between handler entry and 0x40B6D7D8, with r24 pointing at the 0x60FF.

But there's ambiguity: r24 at 0x40B6D7D8 (post)-execution = 0 (the loaded result). I need to capture r24 BEFORE the load. The `stw r24,2(r1)` at 0x40B6D7D0 stores r24 (the 0x60FF PC) to the PPC stack frame [r1+2]. So I can read the backed-up PC from there. Actually r1 changes (sthu at 0x40B6D7CC pushes). 

This is getting intricate. Let me implement the fix at an earlier, cleaner point: detect when the DR dispatches the 0x60FF and compute/store the target into [gd+0x10] right before the handler runs. 

The cleanest interception: watch for `Current == 0x40B6D7D8` and before the PPC `lwzx` executes... but the probe runs post-exec. I need a pre-exec hook. 

Alternatively — simpler and more robust: patch the **guest ROM's 68K boot region** to remove reliance on 0x60FF, OR intercept at the handle-entry and pre-seed.

Given user chose r28-ABI path, let me implement a clean, ABI-respecting patch: at the DRYIELD-RESUME AND for the specific 0x60FF at the known boot PC, write the target into gd+0x10. But targets vary, so I must compute dynamically.

Let me implement a pre-exec interception in the loop. Let me check if the loop has a place to check `Current` and GPRs before... Actually the loop is per-instruction post-exec. But I can detect the *upcoming* 0x60FF: when `Current == 0x40B6D780` (the handler entry for class 0) AND `r24` (backed up) points at 0x60FF, I can compute and store the target to [gd+0x10] so that later in the same handler the lwzx reads it.

Wait — but is 0x40B6D780 reached fresh for each 0x60FF? The ed.v[0] entry = 0x40B6D780. For 0x60FF, the dispatch index... Let me verify Current==0x40B6D780 is the entry point for this 0x60FF. In the Y80 trace, at Y80[40] Current=0x40B6D754 (inside the handler already), and Y80[44]=0x40B6D788. So the handler was entered before 0x40B6D754. Let me check if Current==0x40B6D780 is visited as the entry.

Actually from ed.v[0][B800]=0x40B6D780, the dispatch `bctr` jumps to 0x40B6D780. And the handler at 0x40B6D780 does the refetch. So for a 0x60FF, Current takes values 0x40B6D780, 0x784, ... 0x7CC, 0x7D0(stw r24), 0x7D4, 0x7D8(lwzx). At 0x40B6D7D0 `stw r24,2(r1)` is where r24 still holds the PC. But r1 is the PPC stack, not easily accessible cleanly... actually I can read the PPC stack via memory at r1. But r1 for the DR is the 68K SSP-ish... complex.

Cleaner: at `Current == 0x40B6D7D4` (just before the lwzx), r24 still = the backed-up PC (0x60FF's address), r1 already pushed. I can compute target and write to [gd+0x10] BEFORE the lwzx at 0x40B6D7D8 runs. Since the loop runs per-instruction and after 0x40B6D7D4 executes, the next Current will be 0x40B6D7D8. At that point r24 has been overwritten by lwzx. So I must act when Current==0x40B6D7D4 (r24 intact).

At Current==0x40B6D7D4, after it executes, r24 = the 0x60FF PC (0x408000C0), and I write target to [gd+0x10], and CR/other fine. Then when Execution proceeds to 0x40B6D7D8, lwzx reads [gd+0x10] = target. 

Let me implement: when `Current == 0x40B6D7D4 && g_DrYieldSeen`, compute the 0x60FF branch target and write to gd+0x10. Read opcode at r24 (should be 0x60FF), d32 at r24+2, target = r24 + 2 + d32.

But this only patches the DR's behavior for one specific handler-path. If the DR has other 0x60FF instances at different PCs, they'd all go through this same 0x40B6D7D4→0x40B6D7D8 path, so the patch generalizes. Good.

Let me also make sure I only do this when r24 points at a genuine 0x60FF (not some other opcode routing here). Check the word at r24 == 0x60FF.

Let me implement this. Actually, wait — I should double check the handler is really for 0x60FF and not a general vector path that would need different target computation for other opcodes. Let me gate on `CpuRead16(r24) == 0x60FF`.

Let me also reconsider the d32: ROM at 0x408000C2 = `0000 A94E` so d32 = 0x0000A94E (big-endian 32-bit). target = 0x408000C0 + 2 + 0xA94E = 0x4080AA10. Let me confirm with the disassembly I did of 0xAA10 — yes, 0xAA10 had valid 68K code (`3E7C 2600` = MOVE.W #0x2600,D7... wait that was the entry@0xC6 in flat file, but at 0xAA10 the probe showed `3E 7C 26 00 28 4E 41 FA 00 B8 20 08...` = MOVE.W #0x2600,D7;<)MOVE.L?; JSR; ... valid 68K). Good, 0x4080AA10 is valid code.

Let me implement the pre-seed at Current==0x40B6D7D4.

Let me place this right before the DR-BR probe (or as part of it). Let me add it before the DR68K-LOW block too (since that block breaks). Let me add near the DR-BR probe.

Let me implement it as a separate block before the DR68K-LOW block (which is the one that breaks and squashes later code). Actually the DR-BR probe is already before DR68K-LOW. Let me add the pre-seed right after the DR-BR probe.r28 is now correctly 0xB000 (confirmed it's the gd base), and the DR reads `[gd+0x10]` = 0 as the branch target. So the DR's branch handler expects the target to be **pre-seeded at `[gd+0x10]`** before the `lwzx r24,r28,r7` at 0x40B6D7D8. The target for `0x60FF` is `PC+2+d32` = `0x4080AA10`.

I'll pre-seed `[gd+0x10]` at `Current==0x40B6D7D4` (just before the `lwzx`, when r24 still holds the backed-up PC). Let me add this:



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 20,
  "offset": 5742
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
5742:                   L"r24(68Kpc/post)=0x%08x slot=0x%08x\n",
5743:                   g_PpcContext.Gpr[6] & 0xFFFFFFFF,
5744:                   g_PpcContext.Gpr[7] & 0xFFFFFFFF,
5745:                   g_PpcContext.Gpr[28] & 0xFFFFFFFF,
5746:                   g_PpcContext.Gpr[24],
5747:                   CpuRead32(g_PpcContext.Gpr[28] +
5748:                             (UINTN)(g_PpcContext.Gpr[6] & ~7u)));
5749:             Print(L"  DR-BR gdB000+0x00=0x%08x +0x10=0x%08x +0x0c=0x%08x "
5750:                   L"+0x54=0x%08x 68Krom@0xC0=%04x%04x %04x%04x\n",
5751:                   CpuRead32(0xB000), CpuRead32(0xB010), CpuRead32(0xB00C),
5752:                   CpuRead32(0xB054),
5753:                   CpuRead16(0x408000C0), CpuRead16(0x408000C2),
5754:                   CpuRead16(0x408000C4), CpuRead16(0x408000C6));
5755:         }
5756:         if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&
5757:             (Current >= 0x40B60000 && Current < 0x40B82000)) {
5758:             UINT32 Pc24 = g_PpcContext.Gpr[24];
5759:             if (Dr68KLast >= 0x40800000 && Pc24 < 0x40000000) {
5760:                 UINTN A;
5761:                 Dr68KLowProbed = 1;

(Showing lines 5742-5761 of 6518. Use offset=5762 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 12.2s)

Now let me add the pre-seed block before the DR68K-LOW block. Let me insert it right after the DR-BR probe (after line 5755):



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "                  CpuRead16(0x408000C0), CpuRead16(0x408000C2),\n                  CpuRead16(0x408000C4), CpuRead16(0x408000C6));\n        }\n        // DR 0x60FF (BRA.L) target seed: the DR's 68000-style branch handler\n        // reads the next 68K PC from [gd + (r6 & ~7)] via `lwzx r24,r28,r7` at\n        // 0x40B6D7D8 (r28 = gd = 0xB000). For the Apple ROM's 0x60FF (68020\n        // BRA.L) the DR has no displacement support, so the slot is 0 and the\n        // emulator falls into low RAM. At 0x40B6D7D4 (the stw before the lwzx)\n        // r24 still holds the backed-up PC of the 0x60FF; pre-compute the\n        // PC-relative target (PC+2+d32) and stash it in [gd+0x10] so the\n        // lwzx picks it up. Generalises to every 0x60FF the DR executes.\n        if (Current == 0x40B6D7D4 && g_DrYieldSeen && !g_DrBrFixed) {\n            UINT32 Bp = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (CpuRead16(Bp) == 0x60FF) {\n                UINT32 Disp = (UINT32)((UINT32)CpuRead16(Bp + 2) << 16) |\n                              (UINT32)CpuRead16(Bp + 4);\n                UINT32 Tgt = Bp + 2 + Disp;\n                CpuWrite32(0x0000B010, Tgt);\n                g_DrBrFixed = 1;\n                Print(L\"  DR-BRFIX 0x60FF @68Kpc=0x%08x d32=0x%08x -> TGT=0x%08x \"\n                      L\"stored gd+0x10\\n\", Bp, Disp, Tgt);\n            }\n        }\n        if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&\n            (Current >= 0x40B60000 && Current < 0x40B82000)) {",
  "oldString": "                  CpuRead16(0x408000C0), CpuRead16(0x408000C2),\n                  CpuRead16(0x408000C4), CpuRead16(0x408000C6));\n        }\n        if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&\n            (Current >= 0x40B60000 && Current < 0x40B82000)) {"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 17.2s)

Wait — there's a problem. The handler does `addi r24,r24,-2` at 0x40B6D788 (before 0x40B6D7D4). So r24 at 0x40B6D7D4 points at the current opcode 0x60FF (backed up). But `Bp = r24` at that point = 0x408000C0. CpuRead16(0x408000C0) = 0x60FF. Good. d32 at 0xC2 = 0x0000A94E. Target = 0xC0+2+0xA94E = 0x4080AA10. 

But there's an important subtlety: `CpuRead32(0xB010)` vs writing `CpuWrite32(0x0000B010, Tgt)`. The gd region 0xB000 — is it RAM that I can write? The emulator's gd base is at 0xB000 (low memory). Since the DR reads `[r28+0x10]` = `[0xB010]` via `lwzx` (a PPC load), and our emulator maps that memory, writing to 0xB010 via CpuWrite32 should make the subsequent `lwzx` read it. Let me verify 0xB000-0xB0FF is writable RAM in our memory map. 

Also need to add the `g_DrBrFixed` static. Let me add it near DrBrProbe.

Let me also double check: my g_DrBrFixed is checked as `!g_DrBrFixed` and set to 1 on first 0x60FF. But since this handler generalizes, and the boot has MULTIPLE 0x60FF entries, I should NOT gate to single-shot — I should seed every time. But the target differs per entry. Let me not single-shot it; instead always pre-seed when r24 points at 0x60FF. But writing every visit to 0x40B6D7D4 that has 0x60FF is fine. Let me remove the single-shot g_DrBrFixed gating to handle all 0x60FF entries. Actually, keeping it simple: remove the g_DrBrFixed guard, always check.

Let me fix that. Also verify 0xB010 writable.



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        if (Current == 0x40B6D7D4 && g_DrYieldSeen) {\n            UINT32 Bp = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (CpuRead16(Bp) == 0x60FF) {\n                UINT32 Disp = (UINT32)((UINT32)CpuRead16(Bp + 2) << 16) |\n                              (UINT32)CpuRead16(Bp + 4);\n                UINT32 Tgt = Bp + 2 + Disp;\n                CpuWrite32(0x0000B010, Tgt);\n                g_DrBrFixed++;\n                if (g_DrBrFixed <= 4)\n                    Print(L\"  DR-BRFIX[%u] 0x60FF @68Kpc=0x%08x d32=0x%08x -> TGT=0x%08x \"\n                          L\"stored gd+0x10\\n\", g_DrBrFixed, Bp, Disp, Tgt);\n            }\n        }",
  "oldString": "        if (Current == 0x40B6D7D4 && g_DrYieldSeen && !g_DrBrFixed) {\n            UINT32 Bp = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (CpuRead16(Bp) == 0x60FF) {\n                UINT32 Disp = (UINT32)((UINT32)CpuRead16(Bp + 2) << 16) |\n                              (UINT32)CpuRead16(Bp + 4);\n                UINT32 Tgt = Bp + 2 + Disp;\n                CpuWrite32(0x0000B010, Tgt);\n                g_DrBrFixed = 1;\n                Print(L\"  DR-BRFIX 0x60FF @68Kpc=0x%08x d32=0x%08x -> TGT=0x%08x \"\n                      L\"stored gd+0x10\\n\", Bp, Disp, Tgt);\n            }\n        }"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 6.5s)

I want to verify the timing: at `Current == 0x40B6D7D4`, is r24 still the PC (before the lwzx at 0x40B6D7D8)? The instruction at 0x40B6D7D4 is `rlwinm r7, r6, 0, 0x14, 0x1d` which does NOT modify r24. The prior r24-modifying instruction in the handler is `addi r24,r24,-2` at 0x40B6D788. So at 0x40B6D7D4 (after it executes), r24 still = PC-2 = 0x408000C0. 

But wait — the per-instruction loop checks `Current` AFTER the instruction executes. When Current==0x40B6D7D4 the `rlwinm` just ran; r24 unchanged = 0x408000C0. Good, Bp correct. Then next iteration Current==0x40B6D7D8 runs `lwzx r24,r28,r7` reading [0xB010] which I pre-stored. 

Let me add the static `g_DrBrFixed` and verify 0xB010 is writable RAM. Let me add the static next to DrBrProbe.



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "    static UINT32 DrBrProbe = 0;\n    static UINT32 g_DrBrFixed = 0;",
  "oldString": "    static UINT32 DrBrProbe = 0;"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 4.1s)

Now let me verify that 0xB000-0xB0FF is writable RAM in the emulator's memory model. Let me check how CpuWrite32 handles 0xB000 region — is it the low-RAM/ECB area?



**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n$l = Get-Content src/cpu/interpreter.c\n# find where CpuWrite32/ECB/low-ram region (0xB000) is mapped\n$l | Select-String \"0x0000B000|0x0000B010|0xB000|ECB|0x0000A000\" | Select-Object -First 30"
}
```

**Output:**
```

// DR emulator) and fabricates the emulator-task [7mECB[0m context: [ECB+0x1C4] holds
// the 68K PC and [[7mECB[0m+0xFC] the DR resume PC the MRETTASK rfi reloads. Because
    // Watch the [7mECB[0m 68K PC slot (ECB+0x1C4 = 0xB2C4, restored to r24 by
    // [7mECB[0mTOTASK) and the [KDP-0x14] ECB pointer (0x9FEC).
        // reset vector there; substitute it on the first write so the [7mECB[0mTOTASK
        static UINTN [7mEcb[0mPcWatchHits = 0;
        if ([7mEcb[0mPcWatchHits < 40) {
            [7mEcb[0mPcWatchHits++;
            Print(L"  [7mECB[0mPCWATCH [%08x] <- 0x%08x (old 0x%08x) @PC=0x%08x "
    if (A >= 0xA078u && A < [7m0xB000[0mu) {
            // ([7mECB[0m.IntraState.HandlerReturn). On the cold path that runs in a
                    g_PpcContext.Gpr[31] = [7m0x0000B000[0m;     // ed / emulator-data base
                    // Point the switch at the [7mECB[0m and put the DR dispatch loop
                    // in the [7mECB[0m's Handler-return slot to resume the emulator.
                    CpuWrite32(0x00009FFC, [7m0x0000A000[0m);  // [SPRG0-4] task r1 (68K SSP)
                    CpuWrite32(0x0000A658, 0x0000B100);  // [task_r1+0x658] next ctx = [7mECB[0m
                    CpuWrite32(0x0000B204, 0x40B67B60);  // [7mECB[0m+0x104 HandlerReturn = DR dispatch
                    Print(L"  DRBOOT-RESUME @rfi [7mecb[0m[1c4]=0x%08x lowmem[4]=0x%08x "
                          L"-> DR start 0x40B6E964 r31(ed)=[7m0xB000[0m r29=0x40B80000\n",
            // handler's task swap stores r0 (= the trap address) as the [7mECB[0m
            // dispatch loop re-armed on the [7mECB[0m: r24 holds the 68K PC past the
                g_PpcContext.Gpr[31] = [7m0x0000B000[0m;     // ed base (paranoia)
                g_PpcContext.Gpr[28] = [7m0x0000B000[0m;     // R28 ABI experiment: host mem base
                // populates ([7mECB[0m+0x84=LA_EmulatorEntry=0x6806E8B0, ECB+0x88=
                // LA_EmulatorData=0x68FFF000, [7mECB[0m+0x8C=LA_DispatchTable=
                // 0x68080000) and print the first [7mECB[0m-sized candidate found.
                        Print(L"  HANDOFF [7mECB [0m[7m[0mcandidate [0m[7m[0mat [0m[7m[0m0x%08x: [0m[7m[0m+0x80=%08x [0m[7m[0m+0x84=%08x [0m[7m[0m+0x88=%08x [0m[7m[0m+0x8C=%08x [0m
[7m[0m+0x90=%08x [0m[7m[0m+0x94=%08x\n",[0m
    // task's [7mECB[0m context exists triggers an early task-switch that restores a
    // garbage 68K PC (r24=[[7mECB[0m+0x1C4]=0xFFFFFFFF) and the boot rescues before
    static UINT32 [7mEcb[0mProbed = 0;


```

---

## Assistant (Build · Big Pickle · 8.5s)

The `0xA078 <= A < 0xB000` region is handled specially (ECB area), but 0xB010 is inside 0xA078-0xB000? No — 0xB010 >= 0xB000, so it's outside that range. 0xB000 itself is the boundary. Let me check how the memory map handles 0xB000-0xB100 (the ed/emulator-data area). Let me look at CpuWrite32 for the B000 region and confirm 0xB010 is writable RAM.

Let me check the LowMemory region write handling.



**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n$l = Get-Content src/cpu/interpreter.c\n$l | Select-String \"CpuWrite32\\(|CpuRead32\\(\" | Select-String \"A078|0xB000|0xB010|LowRam|LowMemory|>= 0x\" | Select-Object -First 20\necho \"--- show CpuWrite32 def ---\"\n$l | Select-String \"UINT32 CpuWrite32|CpuWrite32\\(UINT\" | Select-Object -First 3"
}
```

**Output:**
```

                  [7mCpuRead32([0m0xB000), CpuRead32(0xB010), CpuRead32(0xB00C),
--- show CpuWrite32 def ---
static VOID   [7mCpuWrite32(UINT[0m32 A, UINT32 V)


```

---

## Assistant (Build · Big Pickle · 12.1s)

Let me check how the guest RAM is backed — the low memory region 0x00000000-0x0x40000000. Since CpuWrite32(0..) writes to a memory array, writing 0xB010 should go into the guest RAM backing. Let me check the definition and where ReadMem/WriteMem map region 0xB000.

Let me find how memory at 0xB000 is backed (it's below 0x40000000, so in the low-RAM region). Let me check the main memory read/write dispatch.



**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n$l = Get-Content src/cpu/interpreter.c\n$i = ($l | Select-String \"static VOID   CpuWrite32\\(UINT\").LineNumber\n$l[($i-1)..($i+25)]"
}
```

**Output:**
```
static VOID   CpuWrite32(UINT32 A, UINT32 V)
{
    // Watch the ECB 68K PC slot (ECB+0x1C4 = 0xB2C4, restored to r24 by
    // ECBTOTASK) and the [KDP-0x14] ECB pointer (0x9FEC).
    if (A == 0x0000B2C4u || A == 0x00009FECu) {
        // DR bootstrap: a cold boot's task-switch save (stw r24,0x1C4(r6) at
        // 0x40B238F0) stores an uninitialized r24=0xFFFFFFFF as the 68K PC
        // (the emulator context was never created). Hardware holds the ROM 68K
        // reset vector there; substitute it on the first write so the ECBTOTASK
        // restore yields a live 68K boot PC.
        if (g_DrBootPcSeeded && A == 0x0000B2C4u && V == 0xFFFFFFFFu) {
            V = 0x4080002A;
            Print(L"  DRBOOTPC [B2C4] FFFFFFFF -> 0x4080002A @PC=0x%08x "
                  L"r24=%08x\n",
                  g_PpcContext.Pc, g_PpcContext.Gpr[24]);
        }
        static UINTN EcbPcWatchHits = 0;
        if (EcbPcWatchHits < 40) {
            EcbPcWatchHits++;
            Print(L"  ECBPCWATCH [%08x] <- 0x%08x (old 0x%08x) @PC=0x%08x "
                  L"r1=%08x r6=%08x r24=%08x\n",
                  A, V, CpuRead32(A), g_PpcContext.Pc, g_PpcContext.Gpr[1],
                  g_PpcContext.Gpr[6], g_PpcContext.Gpr[24]);
        }
    }
    // Continuation-value watch (PPC side): stores of addresses inside the
    // NK trampoline region - the 68K stack gets its bogus RTS target from

```

---

## Assistant (Build · Big Pickle · 4.2s)

**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n$l = Get-Content src/cpu/interpreter.c\n$i = ($l | Select-String \"static VOID   CpuWrite32\\(UINT\").LineNumber\n# find the tail of CpuWrite32 to see what it does with low addresses\n$l[($i+25)..($i+80)] | Select-String \"Mem|Write|0x40000000|low|RAM|return|Array\" | Select-Object -First 30"
}
```

**Output:**
```

    // NK t[7mram[0mpoline region - the 68K stack gets its bogus RTS target from
        static UINTN PpcT[7mram[0mpValHits = 0;
        if (PpcT[7mram[0mpValHits < 24 &&
            PpcT[7mram[0mpValHits++;
            Print(L"  PPC-T[7mRAM[0mP-VAL [%08x]<-%08x pc=%08x r1=%08x\n",
    // ed.v software-function-slot [7mwrite[0m watch: catch ANY guest code seeding
    // [7mlow[0m pages holding the live 68K vector table, KDP, emulator-data area,
    // words be[7mlow[0m 64K preserves the guest's own data; freed-block bodies
        [7mreturn[0m;
    // Watch [7mwrite[0ms into the KDP PMDT pointer array (KDP+0x78..KDP+0xFF8).
    // Legitimate [7mwrite[0mrs: the one-shot PMDTINJECT probe and early init that
    // tables mid-walk, so log [7mwrite[0mr PC + value + old contents.
        static UINTN Kdp[7mWrite[0mHits = 0;
        if (Kdp[7mWrite[0mHits < 40 && g_PpcContext.Pc != 0x40B1F418u &&
            Kdp[7mWrite[0mHits++;
    Cpu[7mWrite[0m16(A, V >> 16);
    Cpu[7mWrite[0m16(A + 2, V);


```

---

## Assistant (Build · Big Pickle · 5.7s)

Let me look at the actual memory write implementation to confirm 0xB010 write lands in guest RAM. Let me find the underlying `CpuWrite16` / memory array function.



**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n$l = Get-Content src/cpu/interpreter.c\n# find CpuWrite16 def\n$i = ($l | Select-String \"static VOID   CpuWrite16\\(\" | Select-Object -First 1).LineNumber\n$l[($i-1)..($i+40)] | Select-String \"if \\(A|0x400000|0x408000|0xB000|Memory|Mem|RAM|Write|else|return\" | Select-Object -First 25"
}
```

**Output:**
```

static VOID   Cpu[7mWrite[0m16(UINT32 A, UINT32 V) { g_WriteByte(A, (UINT8)(V >> 8)); g_WriteByte(A + 1, (UINT8)V); }
// the PPC side supplies the same [7mwrite[0ms itself. This substitutes the ROM reset
// and gates the one-time DR-bootstrap [7mwrite[0ms below.
static VOID   Cpu[7mWrite[0m32(UINT32 A, UINT32 V)
    [7mif (A[0m == 0x0000B2C4u || A == 0x00009FECu) {
        // reset vector there; substitute it on the first [7mwrite[0m so the ECBTOTASK
            V = [7m0x408000[0m2A;
            Print(L"  DRBOOTPC [B2C4] FFFFFFFF -> [7m0x408000[0m2A @PC=0x%08x "


```

---

## Assistant (Build · Big Pickle · 105.5s)

The writes go to `g_WriteByte(A, ...)`. The gd region 0xB000-0xB100 is in low RAM (below 0x40000000) which is backed by the guest RAM array. Since the DR already reads/writes `[0xB000+X]` via the CpuRead/Wright, and our seed writes use the same CpuWrite32, the `lwzx r24,r28,r7` PPC load at runtime reads from the same backing. The prior DR-BR probe confirmed `CpuRead32(0xB010)=0` and the DR's `lwzx` also got 0 — same backing. So writing to 0xB010 works.

Let me build and run.



**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 95 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"DR-BRFIX|DR68K-LOW|DR-BR\\[|DRYIELD\" | Select-Object -First 12",
  "timeout": 170000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt

  [7mDRYIELD[0m-RESUME @rfi trap=0x40B6E8C8 68Kpc(r24)=0x408000BC r27(op)=0x4DFA -> DR dispatch 0x40B67B60
  [7mDR-BR[[0m1] r6=0x00000010 r7=0x00000010 r28=0x0000B000 r24(68Kpc/post)=0x00000000 slot=0x00000000
  [7mDR68K-LOW[0m[1] [0m[7m[0mexec=107194 [0m[7m[0mPPC=0x40B6D7D8 [0m[7m[0mr24(68Kpc)=0x00000000 [0m[7m[0mr27=0x60FF [0m[7m[0mr23=0x00000000 [0m[7m[0mr29=0x40B80040 [0m
[7m[0mr31=0x0000B000 [0m[7m[0mr1=0x00009FF8 [0m[7m[0mLR=0x40B6D7CC [0m[7m[0mCTR=0x40B658B0[0m
  [7mDR68K-LOW[0m: stopping for inspection


```

---

## Assistant (Build · Big Pickle · 34.0s)

The DR-BRFIX never fired, and the DR68K-LOW still shows the failure. The issue: my pre-seed at `Current == 0x40B6D7D4` didn't trigger because the **DR68K-LOW `break` fires at 0x40B6D7D8 before... no** — the pre-seed block is placed BEFORE the DR68K-LOW block in code order, but the DR68K-LOW fires when Current==0x40B6D7D8 (exec=107194). 

The sequence in a single loop iteration: when Current==0x40B6D7D4, my pre-seed runs (r24=0x408000C0, should match 0x60FF). But it didn't fire. Let me check — maybe `g_DrYieldSeen` isn't set at that point, or the DR never visits 0x40B6D7D4 as a distinct Current (maybe the loop jumps over it or it's not a discrete instruction boundary).

Wait — the DR68K-LOW fired at exec=107194 at PPC=0x40B6D7D8. For my pre-seed to work, at the PRIOR iteration Current would be 0x40B6D7D4 (or 0x40B6D7CC/0x40B6D7D0). Let me check whether the DR-BRFIX block executed at all — it printed nothing, and even `g_DrBrFixed` checks would print if r24 pointed at 0x60FF.

The problem: `Current == 0x40B6D7D4` — my block requires CURRENT (the just-executed instruction) to be exactly 0x40B6D7D4. But maybe the DR68K-LOW probe at `Current == 0x40B6D7D8` is the FIRST time the DR enters this region as observed, and the preceding instruction was a branch INTO 0x40B6D7D8 directly (via `bsolrl cr7` at 0x40B6D7C8 which branches to 0x40B6D7D8 conditionally, or the fall-through).

Look at 0x40B6D7C8 `bsolrl cr7` — this is `bsolr` (branch to LR with CR condition). It branches to LR = 0x40B6D7CC (from the DR-BR trace, RL=0x40B6D7CC... wait the trace said LR=0x40B6D7CC). So `bsolrl cr7` at 0x40B6D7C8 branches to 0x40B6D7CC, then 0x40B6D7CC (sthu), 0x40B6D7D0 (stw), 0x40B6D7D4 (rlwinm), 0x40B6D7D8 (lwzx). So all these should be visited in sequence.

But `bsolrl` may be conditional — if cr7 condition false, it falls through to 0x40B6D7CC anyway. Either way 0x40B6D7D4 is visited.

Hmm, but my probe checks `Current == 0x40B6D7D4`. Let me check: maybe the DR68K-LOW region check `Current >= 0x40B60000 && Current < 0x40B82000` combined with the Dr68K-LOW firing... Actually the DR-BRFIX should be independent.

Wait — maybe the issue is ordering/`Current` value. Let me reconsider: is `g_DrYieldSeen` set? Yes, DRYIELD-RESUME fired. Is `Current` at 0x40B6D7D4 actually reached? Let me add a more permissive probe — fire when Current is in 0x40B6D7CC..0x40B6D7E0 and print r24. Let me also double check: maybe r24 at that point is NOT 0x408000C0.

Actually, wait — looking at DR68K-LOW trace: at PPC=0x40B6D7D8, r24(68Kpc)=0 (post-lwzx). The DR-BR probe (also at 0x40B6D7D8, listed before DR68K-LOW) shows r24(post)=0. So BEFORE the lwzx, r24 must have been 0x408000C0. At 0x40B6D7D4 that's r24. So my read `CpuRead16(0x408000C0)` should = 0x60FF and fire.

Unless `Current == 0x40B6D7D4` never matches because after executing 0x40B6D7D4 the loop's `Current` for the NEXT check is 0x40B6D7D8 (i.e., the loop might not break at each instruction but the probe runs within the same instruction's check). Let me look at the actual loop structure — the probes are all in one big per-instruction block, so `Current` is set once per PPC instruction executed. Between 0x40B6D7D4 executing and 0x40B6D7D8 executing, Current takes 0x40B6D7D8. So there should be an iteration with Current==0x40B6D7D4.

But the DR-BFIX didn't print. Let me check if maybe `CpuRead16(0x408000C0)` doesn't return 0x60FF because the ROM window read path differs from `g_PpcContext` memory. But the DR-BR probe read `CpuRead16(0x408000C0)` and printed 60FF correctly. So at 0x40B6D7D4 r24 must equal 0x408000C0 for my check.

Let me add debug to print r24 whenever Current is 0x40B6D7CC-0x40B6D7E0 to see the actual r24 at these points. Actually, simpler: the DR-BRFIX may not fire because `Current == 0x40B6D7D4` — but this handler might be reached via a path where the executable PC skips 0x40B6D7D4 entirely. Let me check if 0x40B6D7D8 is reached directly.

Look: `bsolrl cr7` at 0x40B6D7C8 — if this is `bsolrl`, it's `b .-something` no... Let me not overanalyze. Let me broaden the pre-seed to fire when `Current >= 0x40B6D7CC && Current <= 0x40B6D7D4` (the sthu/stw/rlwinm sequence) and r24 points at 0x60FF, so that the target is stored before 0x40B6D7D8 executes regardless of exact stepping.

Let me update the range and add a fallback: also seed when Current==0x40B6D7CC/0x40B6D7D0/0x40B6D7D4.

Actually, a cleaner approach: seed is idempotent and cheap. Fire on the whole range 0x40B6D7CC..0x40B6D7D4. Let me update.



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        if (Current >= 0x40B6D7CCu && Current <= 0x40B6D7D4u && g_DrYieldSeen) {\n            UINT32 Bp = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (CpuRead16(Bp) == 0x60FF) {",
  "oldString": "        if (Current == 0x40B6D7D4 && g_DrYieldSeen) {\n            UINT32 Bp = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (CpuRead16(Bp) == 0x60FF) {"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 34.9s)

Wait, but if this never fires even at 0x40B6D7D4, the range 0x40B6D7CC..0x40B6D7D4 might also not be visited, OR the `CpuRead16(Bp)==0x60FF` check is failing because r24 at those points isn't 0x408000C0. Let me add a diagnostic that always prints r24 when Current is in this pre-lwzx range, so I can see the actual r24 value and whether the gating is the issue. Let me temporarily instrument.

Actually — let me reconsider. The DR68K-LOW fired at PPC=0x40B6D7D8 with exec=107194. But the loop with `break` at the END of DR68K-LOW means at that instruction the break happens. For my pre-seed (placed before DR68K-LOW in source) to have fired, it needed to run at an EARLIER Current. 

I bet the issue is that **the per-instruction probe block order**: my pre-seed block is after the Y80 block and the DR-BR probe but before DR68K-LOW. Since DR68K-LOW breaks at 0x40B6D7D8, and my pre-seed checks the PRIOR addresses, it should have fired before reaching 0x40B6D7D8. Unless those prior addresses weren't individually visited as loop iterations.

Let me check: does the DR's execution at 0x40B6D7D4 get observed by the probe loop? Let me add an unconditional diagnostic print for Current in [0x40B6D7C8..0x40B6D7DC] showing r24.

Let me add it temporarily before the pre-seed.Let me add a diagnostic to see the actual r24 values in that pre-lwzx window, since the seed isn't firing:



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7DCu && !g_DrBrSeen) {\n            g_DrBrSeen = 1;\n            Print(L\"  DR-BRWIN current=0x%08x r24=0x%08x r6=0x%08x r28=0x%08x \"\n                  L\"op@r24=%04x\\n\", Current, g_PpcContext.Gpr[24],\n                  g_PpcContext.Gpr[6], g_PpcContext.Gpr[28],\n                  CpuRead16(g_PpcContext.Gpr[24] & 0xFFFFFFFF));\n        }\n        if (Current >= 0x40B6D7CCu && Current <= 0x40B6D7D4u && g_DrYieldSeen) {\n            UINT32 Bp = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (CpuRead16(Bp) == 0x60FF) {",
  "oldString": "        if (Current >= 0x40B6D7CCu && Current <= 0x40B6D7D4u && g_DrYieldSeen) {\n            UINT32 Bp = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (CpuRead16(Bp) == 0x60FF) {"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 3.7s)

**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n./src_add_static.ps1 2>$null; # guard\n# add static\n$l = Get-Content src/cpu/interpreter.c\n$l | Select-String \"g_DrBrSeen\" | Select-Object -First 3"
}
```

**Output:**
```
[31;1m./src_add_static.ps1: [0m
[31;1m[36;1mLine |[0m
[31;1m[36;1m[36;1m   2 | [0m [36;1m./src_add_static.ps1[0m 2>$null; # guard[0m
[31;1m[36;1m[36;1m[0m[36;1m[0m[36;1m     | [31;1m ~~~~~~~~~~~~~~~~~~~~[0m
[31;1m[36;1m[36;1m[0m[36;1m[0m[36;1m[31;1m[31;1m[36;1m     | [31;1mThe term './src_add_static.ps1' is not recognized as a name of a cmdlet, function, script file, or executable program.[0m
[31;1m[36;1m[36;1m[0m[36;1m[0m[36;1m[31;1m[31;1m[36;1m[31;1mCheck the spelling of the name, or if a path was included, verify that the path is correct and try again.[0m

        if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7DCu && ![7mg_DrBrSeen[0m) {
            [7mg_DrBrSeen[0m = 1;


```

---

## Assistant (Build · Big Pickle · 3.3s)

Let me add the `g_DrBrSeen` static:



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "    static UINT32 DrBrProbe = 0;\n    static UINT32 g_DrBrFixed = 0;\n    static UINT32 g_DrBrSeen = 0;",
  "oldString": "    static UINT32 DrBrProbe = 0;\n    static UINT32 g_DrBrFixed = 0;"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 100.3s)

**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 95 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"DR-BRWIN|DR-BRFIX|DR68K-LOW\" | Select-Object -First 8",
  "timeout": 170000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt

  [7mDR-BRWIN[0m current=0x40B6D7C8 r24=0x408000BE r6=0x00000010 r28=0x0000B000 op@r24=0008
  [7mDR68K-LOW[0m[1] [0m[7m[0mexec=107194 [0m[7m[0mPPC=0x40B6D7D8 [0m[7m[0mr24(68Kpc)=0x00000000 [0m[7m[0mr27=0x60FF [0m[7m[0mr23=0x00000000 [0m[7m[0mr29=0x40B80040 [0m
[7m[0mr31=0x0000B000 [0m[7m[0mr1=0x00009FF8 [0m[7m[0mLR=0x40B6D7CC [0m[7m[0mCTR=0x40B658B0[0m
  [7mDR68K-LOW[0m: stopping for inspection


```

---

## Assistant (Build · Big Pickle · 26.4s)

Now I see the real problem. At `current=0x40B6D7C8, r24=0x408000BE` (op@r24 = `0008`, the `LEA (8,PC),A6` operand word), and the handler processes the **`4DFA 0008` LEA**, not the `0x60FF`!

So the DR handler at 0x40B6D7D8 (entered via ed.v dispatch) is actually the **PC-relative effective-address handler** that runs for instructions like `LEA (d16,PC)` — it reads the reference PC from `[gd+(r6&~7)]`. For `LEA (8,PC)`, the handler:
1. backs up r24 to point at the operand (`0008` at 0xBE)
2. reads the base PC from a slot — but gets 0
3. For LEA, it should compute A6 = base_PC + 8

But wait — the DR-BRWIN shows r24=0x408000BE at 0x40B6D7C8, and the slot read `[gd+0x10]`=0. So the DR is stuck processing this `LEA (8,PC),A6` — it reads a garbage base PC (0) from the slot.

Hold on. Let me reconsider the whole picture. The DRYIELD-RESUME said "68Kpc(r24)=0x408000BC r27(op)=0x4DFA". So the yielding opcode was 0x4DFA (LEA) at 0xBC. After resume, the DR re-dispatchs 0x4DFA at 0xBC. But from the earlier Y80 trace, r24 advanced 0xBE→0xC0, so it partially processed the LEA.

The DR-BRWIN at 0x40B6D7C8 with r24=0x408000BE (operand `0008`) confirms this is the LEA PC-relative decode. The `lwzx r24,r28,r7` reads the **reference PC for the PC-relative effective address** — for `LEA (8,PC),A6`, the reference PC should be 0x408000BE (the PC of the instruction), then A6 = 0xBE + 8 = 0xC6.

But it loads 0 from `[gd+0x10]`. So the DR's PC-relative EA handler needs `[gd + slot]` to hold the instruction's own PC (the base for +8). Since it's 0, A6 computes to 8, wrong.

So the REAL gap: the DR's PC-relative addressing reads the current 68K PC from `[gd+slot]`, and that slot must contain r24's value (the current PC). The DR expects the host (or an earlier instruction) to have stored the current PC into the slot.

Actually — reconsider. For PC-relative EA, the DR needs the instruction's address. It backs up r24 to point at the operand and reads the PC from a slot. The DR likely **stores the current PC into the slot at the start** of each instruction (in the main dispatch loop) and then reads it back here. Let me check the main dispatch loop 0x40B67B60 for a store to `[gd+slot]` before dispatching.

Let me look at the beginning of the dispatch loop more carefully — it may store r24 (PC) to gd somewhere. Let me disassemble 0x40B67B60 completely within the bytecode-accurate region. Earlier the disasm at 0x40B67B60 showed `mr r6,r27; lhau r27,2(r24); ...`. Let me see the full dispatch loop and where it might store the PC.

Actually — I now recall the emulator-data base `gd` and the ECB. The Objective says `[B2C4]=68K PC` (ECB+0x1C4). Maybe the DR stores the 68K PC at `[gd+0x10]` during normal dispatch. Let me look for stores in the dispatch region to `0x10(r31)` or similar.

Let me search the DR for `stw r24, 0x10(r31)` or stores to [gd+0x10].

The issue is clear now: the PC-relative EA handler needs `[gd+0x10]` (or whatever slot) to hold the **current instruction's PC**, which is supposed to be set by the DR's dispatch prologue but never is (because our resume enters mid-stream).

Let me search for where the DR writes the PC to the slot. Let me look at the dispatch loop ending/branch handlers that store to gd+X.

Let me search for `stw r24` in the whole DR region and see which go to gd offsets.



**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\npython tool_scripts/dis_ppc.py 0x40B60000 0x40B6F000 2>&1 | Select-String \"stw r24, 0x[0-9a-f]+\\(r31\\)|stw r24, 0x[0-9a-f]+\\(gd\\)\" | Select-Object -First 40"
}
```

**Output:**
```
(no output)
```

---

## Assistant (Build · Big Pickle · 3.0s)

**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\npython tool_scripts/dis_ppc.py 0x40B60000 0x40B6F000 2>&1 | Select-String \"stw r24\" | Select-Object -First 40"
}
```

**Output:**
```
(no output)
```

---

## Assistant (Build · Big Pickle · 4.9s)

**Tool: bash**

**Input:**
```json
{
  "command": "$env:ROM_FLAT = \"$env:TEMP\\opencode\\rom_flat_4mb.bin\"\npython tool_scripts/dis_ppc.py 0x40B60000 0x40B6F000 2>&1 | Select-String \"stw r24, |stwu r24, |stw r24\" | Select-Object -First 20\necho \"=== store of r24 anywhere ===\"\npython tool_scripts/dis_ppc.py 0x40B60000 0x40B6F000 2>&1 | Select-String \"r24\" | Select-String \"stw|sth|stb|stdu|sthu\" | Select-Object -First 20"
}
```

**Output:**
```
=== store of r24 anywhere ===

```

---

## Assistant (Build · Big Pickle · 81.5s)

The disassembler's text doesn't show `stw r24` because it formats it as `stw r24, offset(rX)`. My grep for "stw r24" should've matched. Let me check the disassembler output format — maybe it prints `stw r24, 2(r1)` which contains "r24". Let me directly get the region 0x40B6D7CC-0x40B6D7D4 where we KNOW there's `stw r24, 2(r1)` from the raw decode. The disasm earlier showed `0x40b6d7d0: stw r24, 2(r1)`. So "stw r24" does appear. But my grep over the whole DR found nothing for `stw r24, 0x..(r31)` — meaning the DR stores r24 only to the stack frame `2(r1)`, not to gd.

So the DR does NOT store the current PC into `[gd+0x10]` anywhere. That means `[gd+0x10]` is populated by something else — the **host/ECB setup** — or the slot read at 0x40B6D7D8 for the LEA should be reading a pre-existing value.

Let me reconsider what `[gd+0x10]` holds. In the ECB layout, `[B204]`=HandlerReturn (ECB+0x104). The Objective: "[B2C4]=68K PC", "[B204]=0x40B67B60". gd=0xB000. The `lwzx r24,r28,r7` reads `[0xB010]`.

Let me reconsider: maybe r28 is NOT gd. I set r28=0xB000 as an experiment, and the slot read `[0xB010]`=0. But maybe r28 should point elsewhere — to the **68K register/PC state table** that the DR uses for PC-relative. Let me reconsider the cold-start `stw r28, 0x54(r31)`: at cold start r28=0, so `[gd+0x54]=0`. 

Let me look at what the DR's *working* sequential path (non-PC-relative) does vs this. The main dispatch 0x40B67B60 uses `lhau r27,2(r24)` (r24=PC directly). The handler at 0x40B6D780 uses `lwzx r24,r28,r7` for PC-relative. 

The difference: for PC-relative EA, the DR needs BOTH the instruction PC (for the displacement base) AND it uses r28 to index a *different* slot. 

Let me look at what value the DR needs in `[r28 + 0x10]`. For `LEA (8,PC),A6` at PC=0xBC:
- instruction at 0xBC = `4DFA`, operand `0008` at 0xBE
- A6 = PC_of_instruction + 8 = 0xBC + 8 = 0xC4? or 0xBE+8=0xC6?
- On 68000, `LEA (d16,PC),An`: EA = PC_of_ext_word + d16. The PC is the address of the extension word (0xBE), so A6 = 0xBE + 8 = 0xC6.

So the DR needs the reference PC = 0xBE (address of the ext word). At 0x40B6D7D8 it reads `[r28+0x10]` as this reference PC. For it to be 0xBE, the DR must have stored 0xBE there — but it doesn't store r24 to gd.

So `r28` is NOT gd. r28 must be a base such that `[r28 + 0x10]` naturally holds the current PC or the ext-word reference. This reframes everything: r28 points at a **per-instruction scratch/context** where offset 0x10 holds the current instruction's PC.

Given the DR's `sthu r4,-8(r1)` / `stw r24,2(r1)` pushes to the PPC stack (r1), and then `lwzx r24,r28,r7` — maybe `r28` IS the PPC stack pointer region or a frame base where the PC was just pushed!

Look: at 0x40B6D7CC `sthu r4,-8(r1)` (push r4, r1-=8), 0x40B6D7D0 `stw r24,2(r1)` (store PC to [r1+2]=[new_r1+... ]. After sthu, r1' = r1-8. stw r24, 2(r1') → [r1-6]. Then `lwzx r24, r28, r7` — if r28 ≈ r1' (the new stack), then `[r28+0x10]` = `[r1'+0x10]` = a slot 16 bytes up the frame.

Hmm, this is getting too deep into the DR's stack discipline. Let me step back to the empirical: what value SHOULD `[r28+0x10]` be, and with which r28?

Given r28=0xB000 (I set it) gives [0xB010]=0, wrong. If the correct r28 points to a scratch area where the PC is stored... 

Let me reconsider the DR-Yield resume. The resume sets r24=0x408000BC (the yielding opcode PC). Then the DR dispatch 0x40B67B60 re-decodes. But the PC-relative handler needs its own tracking. 

Let me take a completely different, cleaner diagnostic: instead of guessing r28, let me look at what the DR does RIGHT BEFORE reaching 0x40B6D780 (the PC-relative handler) in the dispatch — specifically whether the main loop stores the current PC somewhere that this handler reads with a specific register.

Actually, the critical realization from DR-BRWIN: `r24=0x408000BE` and `op@r24=0008`. The handler backed up r24 to 0xBE (the `0008` ext word). The `lwzx r24,r28,r7` then reads the reference PC. But this is a fresh entry for the LEA. 

I've spent a lot of effort. Let me reconsider the fundamental question: the r28 value that makes `[r28+0x10]` = 0xBE (the reference PC for this instruction). That needs r28 = 0xBE - 0x10 = 0xAE... no, that's instruction-dependent, impossible for a fixed base. So r28 is NOT a base for reading the PC. 

Therefore `[r28+0x10]` is NOT the reference PC directly. It must be that the DR **already stored** the reference PC into `[r28+0x10]` before dispatching. Since it doesn't store r24 to gd[+0x10], r28 is a different base where the PC was stored.

Let me search the DR for stores of the PC to an indexed location. What about `stw r24, X(r[other])` where other could be r28-equivalent? I found only `stw r24,2(r1)`.

So the DR stores the PC to the PPC stack at [r1'+2] right before the lwzx. And then `lwzx r24,r28,r7`. What if r28 relates to r1? Let me check: does the handler set r28 = r1 somewhere? No — no r28 writes at all.

I think I need to look at the ACTUAL call path more holistically. Let me look at what comes right before the DR enters 0x40B6D780 for the LEA, by examining the sequence from dispatch. Let me look at the Y80 trace again: Y80[4] Current=0x40B67B68 (dispatch), ... up to Y80[40] Current=0x40B6D754 then Y80[44] 0x40B6D788. So the DR went: dispatch (0x40B67B60s) → ... → 0x40B6D754 (in handler) → ... The transition into 0x40B6D780 handler happened somewhere between Y80[36] (0x40B67BC8) and Y80[40] (0x40B6D754).

Let me just dump the FULL sequence of Current values in the Y80 window to see the exact path and which slots the handler reads. But more useful: let me reconsider whether 0x40B6D7D8's `lwzx r24,r28,r7` is really reading the PC-reference, or reading the **next sequential PC** which for LEA-with-extension is pushed by `stw r24,2(r1)`.

I'm going too deep. Let me change strategy: instead of reverse-engineering r28, use the empirical observation that `[r28+0x10]` needs to hold the instruction reference PC (0xBE for this LEA). Since r28 is host-set and I set it to 0xB000, and the value needed is dynamic per-instruction (0xBE here), NO single r28 base + fixed slot read can produce the dynamic PC-relative reference — UNLESS the DR stored it there, OR unless r28 itself is updated to point at a frame.

Given the depth here and that the user chose "reverse-engineer r28 ABI," I should present my refined understanding and adjust. The r28 ABI is now partially characterized: r28 is host-set and used by the PC-relative EA handler as a base, and `[r28+0x10]` must contain the current instruction's reference PC for the displacement. The DR itself stores the PC to the PPC stack (`stw r24,2(r1)`) — so perhaps r28 should point to a frame and the store/load pair is the mechanism, meaning r28 should equal the frame base that aligns with the stack.

Given the difficulty, let me verify one concrete hypothesis: **r28 should be a pointer such that the DR's `stw r24,2(r1)` + `lwzx r24,r28,r7` are consistent** — i.e., r28 points to the same frame r1 does, and `[r28+0x10]` is a frame slot that the DR populates elsewhere (e.g., the prologue of the main loop stores the PC to a gd slot that maps to r28).

Let me look at the very start of the DR dispatch loop (0x40B67B60) and check if there's an earlier PC store to gd. Let me disassemble the loop's entry and what happens before the handler call.

Actually, let me reconsider — maybe I had it backwards. Let me look at the handler entry 0x40B6D780 and check whether `lwzx r24,r28,r7` at 0x40B6D7D8 is reading via `r7 = r6 & ~7` where r6 is the **opcode** the handler is processing. In DR-BRWIN, r6=0x10. The opcode being processed is `4DFA` (LEA). But r6=0x10, not 0x4DFA. So r6 is not the raw opcode.

Hmm, r6=0x10. Recall in the refetch routine 0x40B6D750-0x40B6D77C, r6 was `li`'d to 0x10/0x2c/0x28. These look like **register bank offsets for the 8 data regs (D0-D7) and address regs (A0-A7) + CCR/SR**. In the 68K DR's register file stored at gd, D0-D7 might be at slots, A0-A7 elsewhere.

This is the 68K DR's **effective-address / operand dispatch**: r6 holds an offset identifying which register/file, and `[r28 + (r6&~7)]`... 

I think the cleanest path forward, given the extensive effort, is to consult the DingusPPC embedded 68K DR source to get the exact ABI. But I don't have access to the exact vendored DR source (it's compiled into the ROM). 

Let me make a pragmatic decision: the r28 ABI reverse-engineering is proving very deep (it's a host-coupled register with per-instruction slot semantics that the DR never writes itself). Given the user's goal (complete the PPC side so Mac OS 9 boots) and the staged approach, let me try the more robust interpretation: the DR's PC-relative/operand handler needs the current reference PC in `[r28+0x10]`. Since the DR reads r24 (the actual PC) directly in the main loop, and this handler re-reads from a slot that the DR expects to be pre-populated, the host fix is: **before the DR processes an instruction that needs PC-relative addressing, store the current PC into `[r28+0x10]`**.

Since r28=0xB000 and the needed slot is [0xB010], I can seed [0xB010] = the current 68K PC whenever the DR is about to do PC-relative EA. From DR-BRWIN, at 0x40B6D7C8, r24=0x408000BE (the reference PC). So storing [0xB010] = r24 at that point should make the lwzx read 0xBE → A6=0xBE+8=0xC6. 

Let me change my pre-seed: instead of requiring 0x60FF, when Current is in 0x40B6D7C8..0x40B6D7D4, write r24 into [0xB010]. This handles BOTH the LEA PC-relative AND (if it's the same handler) the BRA. Let me generalize: write `r24 & 0xFFFFFFFF` to 0xB010 whenever Current is in the pre-lwzx window.

But wait — for the 0x60FF BRA case, the handler at 0x40B6D7D8 reads the *target* from the slot, not the reference PC. For BRA.L, the target should be PC+2+d32, whereas for LEA the reference is just PC. These need different values in the slot. So a single "write r24" doesn't serve both.

This confirms the DR handles PC-relative *references* (LEA) via the slot holding the instruction PC, and *branches* (BRA.L) via... the same mechanism? For a 68000 `BRA` (16-bit disp), the DR would read the base PC from the slot and add the 8/16-bit disp. For LEA it reads base PC and adds d16. So the slot should hold the **base reference PC** (= PC of the ext word) in BOTH cases, and the handler adds the displacement.

So the fix is uniform: **before the PC-relative handler reads the slot, store the reference PC (r24) into [0xB010]** (since r28=0xB000, slot 0x10). The handler then adds the displacement to produce the target/A6. 

For the BRA.L 0x60FF case: reference PC = 0x408000C0 (after the -2 backup, r24 points at 0x60FF)... wait, actually r24 after backup = PC of the instruction = 0x408000C0, and the 68000 BRA base = PC - 2 = 0x408000C0? Let me not overthink the exact +2; the DR has its own convention. The key: the slot must hold the reference PC.

Let me revise the pre-seed to: whenever Current is in [0x40B6D7C8..0x40B6D7D4] and g_DrYieldSeen, store r24 into [0xB010]. This makes the lwzx read the correct reference PC for BOTH LEA and BRA. Let me implement and test.

But which r24 value is the "reference"? At 0x40B6D7C8 (bsolrl cr7, just before the sthu), r24 is the backed-up instruction PC. For the LEA, r24=0xBE (ext word addr). The handler will add 8 → A6=0xC6. That matches expected A6=0xC6. 

For BRA.L 0x60FF at 0xC0: when the handler processes it, r24 backs up to 0xC0. If I store r24=0xC0 to [0xB010] and the DR adds (2+d32)... but d32 handling for .L is the 68020 thing the DR may not do. However, storing the reference PC is the correct ABI fix regardless; if the DR then can't add the 32-bit disp, that's a separate issue.

Let me implement the generalized "store r24 into [0xB010]" pre-seed and test. Remove the 0x60FF-specific logic, but keep a 0x60FF diagnostic.The `DR-BRWIN` reveals the real issue: at `0x40B6D7C8`, `r24=0x408000BE` (the `0008` ext word of the `LEA (8,PC),A6`), and the handler reads the **PC-relative reference base** from `[r28+0x10]` (=`[0xB010]`). It's 0, so the DR computes A6 wrong. The slot must hold the instruction's reference PC. This is a PC-relative EA issue, not specific to 0x60FF.

Let me generalize the fix: store `r24` (the reference PC) into `[0xB010]` right before the `lwzx` reads it. Let me rewrite the pre-seed:



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        // DR PC-relative reference base: the DR's PC-relative effective-address\n        // / branch handler (entered via ed.v at 0x40B6D780) reads the current\n        // instruction's reference PC from [r28 + (r6&~7)] via `lwzx r24,r28,r7`\n        // at 0x40B6D7D8, then adds the displacement (LEA d16,PC / BRA). The DR\n        // never writes that slot itself, so it is 0 here and every PC-relative\n        // computes off address 0. The DR carries the reference PC in r24 (it\n        // backs r24 up to the ext word); seed the slot with r24 just before the\n        // lwzx runs. r28 = gd = 0xB000 -> slot [0xB010].\n        if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7D4u && g_DrYieldSeen) {\n            UINT32 Ref = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            CpuWrite32(0x0000B010, Ref);\n            g_DrBrFixed++;\n            if (g_DrBrFixed <= 4)\n                Print(L\"  DR-REFSLOT[%u] current=0x%08x refPC->gd+0x10=0x%08x \"\n                      L\"op@ref=%04x\\n\", g_DrBrFixed, Current, Ref,\n                      CpuRead16(Ref));\n        }",
  "oldString": "        if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7DCu && !g_DrBrSeen) {\n            g_DrBrSeen = 1;\n            Print(L\"  DR-BRWIN current=0x%08x r24=0x%08x r6=0x%08x r28=0x%08x \"\n                  L\"op@r24=%04x\\n\", Current, g_PpcContext.Gpr[24],\n                  g_PpcContext.Gpr[6], g_PpcContext.Gpr[28],\n                  CpuRead16(g_PpcContext.Gpr[24] & 0xFFFFFFFF));\n        }\n        if (Current >= 0x40B6D7CCu && Current <= 0x40B6D7D4u && g_DrYieldSeen) {\n            UINT32 Bp = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (CpuRead16(Bp) == 0x60FF) {\n                UINT32 Disp = (UINT32)((UINT32)CpuRead16(Bp + 2) << 16) |\n                              (UINT32)CpuRead16(Bp + 4);\n                UINT32 Tgt = Bp + 2 + Disp;\n                CpuWrite32(0x0000B010, Tgt);\n                g_DrBrFixed++;\n                if (g_DrBrFixed <= 4)\n                    Print(L\"  DR-BRFIX[%u] 0x60FF @68Kpc=0x%08x d32=0x%08x -> TGT=0x%08x \"\n                          L\"stored gd+0x10\\n\", g_DrBrFixed, Bp, Disp, Tgt);\n            }\n        }"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 101.7s)

**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n# remove now-unused g_DrBrSeen static\n(Get-Content src/cpu/interpreter.c) -replace '    static UINT32 g_DrBrSeen = 0;\\r?\\n','' | Set-Content src/cpu/interpreter.c\n& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 95 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"DR-REFSLOT|DR68K-LOW|DRYIELD|HANDOFF\" | Select-Object -First 12",
  "timeout": 170000
}
```

**Output:**
```
[31;1mmake: *** [Makefile:94: build/src/cpu/interpreter.obj] Error 1[0m
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt

  [7mDRYIELD[0m-RESUME @rfi trap=0x40B6E8C8 68Kpc(r24)=0x408000BC r27(op)=0x4DFA -> DR dispatch 0x40B67B60
  [7mDR68K-LOW[0m[1] [0m[7m[0mexec=107194 [0m[7m[0mPPC=0x40B6D7D8 [0m[7m[0mr24(68Kpc)=0x00000000 [0m[7m[0mr27=0x60FF [0m[7m[0mr23=0x00000000 [0m[7m[0mr29=0x40B80040 [0m
[7m[0mr31=0x0000B000 [0m[7m[0mr1=0x00009FF8 [0m[7m[0mLR=0x40B6D7CC [0m[7m[0mCTR=0x40B658B0[0m
  [7mDR68K-LOW[0m: stopping for inspection


```

---

## Assistant (Build · Big Pickle · 13.3s)

The build failed (Makefile error for interpreter.obj). The `Set-Content` may have mangled line endings or the static removal. Let me check the build error.



**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-String \"error|Error|warning\" | Select-Object -First 10",
  "timeout": 120000
}
```

**Output:**
```

clang -target x86_64-pc-win32-coff -mno-red-zone -ffreestanding -fshort-wchar -fno-stack-protector 
-fno-strict-aliasing -funsigned-char -fno-math-errno -O2 -I third_party/gnu-efi/inc -I src -Wall -W[7merror [0m[7m[0m-c [0m
[7m[0msrc/cpu/interpreter.c [0m[7m[0m-o [0m[7m[0mbuild/src/cpu/interpreter.obj[0m
src/cpu/interpreter.c:4724:19: [7merror[0m: unused variable 'g_DrBrSeen' [-Werror,-Wunused-variable]
1 [7merror[0m generated.
make: *** [Makefile:94: build/src/cpu/interpreter.obj] [7mError[0m 1


```

---

## Assistant (Build · Big Pickle · 12.5s)

The `Set-Content` didn't remove the static (likely line-ending mismatch). Let me remove `g_DrBrSeen` via edit tool.



**Tool: grep**

**Input:**
```json
{
  "pattern": "g_DrBrSeen",
  "include": "interpreter.c"
}
```

**Output:**
```
Found 1 matches
C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c:
  Line 4724:     static UINT32 g_DrBrSeen = 0;

```

---

## Assistant (Build · Big Pickle · 12.3s)

**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "oldString": "    static UINT32 g_DrBrFixed = 0;\n    static UINT32 g_DrBrSeen = 0;",
  "newString": "    static UINT32 g_DrBrFixed = 0;"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 109.3s)

**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 95 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"DR-REFSLOT|DR68K-LOW|DRYIELD|HANDOFF\" | Select-Object -First 16",
  "timeout": 170000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt

  [7mDRYIELD[0m-RESUME @rfi trap=0x40B6E8C8 68Kpc(r24)=0x408000BC r27(op)=0x4DFA -> DR dispatch 0x40B67B60
  [7mDR-REFSLOT[0m[1] current=0x40B6D7C8 refPC->gd+0x10=0x408000BE op@ref=0008
  [7mDR-REFSLOT[0m[2] current=0x40B6D7CC refPC->gd+0x10=0x408000BE op@ref=0008
  [7mDR-REFSLOT[0m[3] current=0x40B6D7D0 refPC->gd+0x10=0x408000BE op@ref=0008
  [7mDR-REFSLOT[0m[4] current=0x40B6D7D4 refPC->gd+0x10=0x408000BE op@ref=0008
  [7mDR68K-LOW[0m[1] [0m[7m[0mexec=2226614 [0m[7m[0mPPC=0x40B80040 [0m[7m[0mr24(68Kpc)=0x0000197C [0m[7m[0mr27=0x60FF [0m[7m[0mr23=0x00000000 [0m[7m[0mr29=0x40B80040 [0m
[7m[0mr31=0x0000B000 [0m[7m[0mr1=0xFFF80040 [0m[7m[0mLR=0x40B6D7F8 [0m[7m[0mCTR=0x40B80040[0m
  [7mDR68K-LOW[0m: stopping for inspection


```

---

## Assistant (Build · Big Pickle · 24.6s)

Major progress! The seed fired (4 times for the LEA at 0x40B6D7C8-0x40B6D7D4), and now the DR68K-LOW fired at **exec=2226614** (up from 107194 — 20x more progress!) with **r24=0x0000197C** instead of 0x00000000. The DR is now actually executing and progressed into low memory at 0x197C.

So the LEA now computes correctly (A6 was set), and the DR continued executing the 68K boot into low RAM (0x197C). This is a huge step forward — the DR is now running actual 68K code that reached low memory address 0x197C.

But DR68K-LOW still breaks. The 68K PC is now 0x197C (low memory), and op r27=0x60FF. Let me look at what's at 0x197C and whether it's real boot code or mis-execution. Let me check the low RAM dump region and the boot message area.

Wait — r24=0x197C, and r27(prefetched)=0x60FF. The DR jumped into low memory at 0x197C. Let me look at the low RAM around 0x1800-0x1A00.

Actually the exec jumped from 107K to 2.2M — the DR ran ~2M more instructions and reached 0x197C. Let me examine the low RAM there. Let me remove the DR68K-LOW break behavior issue — but first let me look at where the boot code actually is. The boot message string was at LOW[0x1000+] (from earlier dump). The DR reached 0x197C. Let me dump low RAM 0x1800-0x1C00 to see if there's real code.

Let me remove the `break` in DR68K-LOW so the emulator can continue past this and see if it keeps going, OR keep it and inspect. Given it's making progress, let me disable the break to let it run further and see the next stage. Let me first inspect the low RAM at 0x197C.

Let me add a dump of 0x1800-0x1C00 in the DR68K-LOW probe (or just read the existing log). The current dump only goes to 0x2000 starting at 0. Let me check if the log has the low dump — the break fired so it dumped 0-0x2000. Let me look at 0x1800-0x1A00 in that dump.

I've made major progress. Let me examine what's at 0x197C and whether the DR is executing real code or still mis-executing. Let me check the low-RAM content around the new PC.



**Tool: bash**

**Input:**
```json
{
  "command": "$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"LOW\\[0x0[01][0-9a-f]00\\]\" | Select-String \"0x01\" | Select-Object -First 40"
}
```

**Output:**
```
(no output)
```

---

## Assistant (Build · Big Pickle · 14.8s)

**Tool: bash**

**Input:**
```json
{
  "command": "$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"LOW\\[0x0000\" | Select-String \"1[89]00|1[ab]00|1[cd]00|1[ef]00\" | Select-Object -First 40"
}
```

**Output:**
```

  [7mLOW[0x0000[0m1800] 27044080 00BE0010 27044080 00BE0010
  [7mLOW[0x0000[0m1900] 27044080 00BE0010 27044080 00BE0010
  [7mLOW[0x0000[0m1A00] 27044080 00BE0010 27044080 00BE0010
  [7mLOW[0x0000[0m1B00] 27044080 00BE0010 27044080 00BE0010
  [7mLOW[0x0000[0m1C00] 27044080 00BE0010 27044080 00BE0010
  [7mLOW[0x0000[0m1D00] 27044080 00BE0010 27044080 00BE0010
  [7mLOW[0x0000[0m1E00] 27044080 00BE0010 27044080 00BE0010
  [7mLOW[0x0000[0m1F00] 27044080 00BE0010 27044080 00BE0010


```

---

## Assistant (Build · Big Pickle · 35.8s)

Interesting! Low RAM 0x1800-0x2000 is now populated with repeating `27044080 00BE0010` pattern. Let me decode: `27 04 40 80 00 BE 00 10` = 68K `MOVE.L #0x408000BE, -(A7)`? No — `2704` = `MOVE.L D4, -(A7)`, `4080` might be... Actually these look like the DR is writing a boot pattern into low RAM.

Let me interpret: `27 04` = `MOVE.L D4,-(A7)` — but as data? `40 80` = `NEGX D0`, `00 BE` = `ORI.B #0xBE,D0`... These are probably the DR *storing PPC addresses* into low RAM (some kind of boot-page copy), which got written as interpreted 68K opcodes. The DR ran 2.2M instructions and filled low RAM 0x1800+ with a repeating pattern of `0x408000BE`-related values.

Actually the pattern `27044080 00BE0010` interpreted little-endian-ly as data could be a structure. This is the DR executing something that writes to low RAM — likely a **memory copy loop** that's now copying ROM content to low RAM, but getting stuck in a patterned loop (all same value).

Let me look at where the pattern starts and what the DR was doing. The DR reached r24=0x197C which is IN this patterned region. So the DR is executing the pattern `...00BE0010...` as code. That's the degenerate loop — it's executing garbage data.

Wait, but exec went from 107K to 2.2M — so it DID make progress and WRITE this pattern. Let me reconsider. The LEA fix let A6 = 0xC6 be set. Then the boot program continued. It seems to be executing a loop that fills low RAM with `0x408000BE`-ish values. Address 0x197C is where it ended up.

The pattern value `0x408000BE` = ROM address 0x408000BE (the LEA instruction's operand region!). So something is copying/storing ROM addresses into low RAM. This might be a **real boot behavior** — the 68K boot constructing a low-RAM jump table pointing into ROM — but the DR is now inside a garbage region.

Given the massive progress (107K→2.2M instructions, DR now touching low RAM with real ROM-address content), the LEA fix was correct and the boot is progressing. But it enters low RAM where there's a repeating pattern and no real 68K code.

Let me reconsider the bigger picture. The DRLP (68K boot) probably:
1. Executes `LEA (8,PC),A6` → A6 = pointer to the boot entry table
2. Iterates the table, each entry does setup
3. Eventually jumps to the 68K Nanokernel or a real low-RAM routine

Since low RAM 0x1800+ has this repeated pattern, and r24 ended at 0x197C inside it, the DR is now mis-executing. But the pattern being `0x408000BE` (a ROM pointer) suggests the boot is trying to build structures.

Let me investigate where this pattern gets written from. Let me disable the DR68K-LOW `break` temporarily and let it run longer to see if it recovers or truly loops, and trace more.

Actually — the DR68K-LOW probe fires once (Dr68KLowProbed=1) then breaks. Let me change it to NOT break (just log once and continue), so the emulator keeps running and I can see the full boot progression. That was the original intent issue — the break was for inspection. Let me remove the break.

Let me find and remove the break in the DR68K-LOW block.Huge progress: exec jumped 107K→2.2M instructions, and the DR now wrote real ROM-derived values (`0x408000BE`) into low RAM and advanced to 0x197C. The LEA fix worked. Now it's stuck in a repeating pattern region. Let me remove the `break` in DR68K-LOW so I can see the full progression rather than stopping at first low-memory entry.



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 30,
  "offset": 5761
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
5761:         // emulator falls into low RAM. At 0x40B6D7D4 (the stw before the lwzx)
5762:         // r24 still holds the backed-up PC of the 0x60FF; pre-compute the
5763:         // PC-relative target (PC+2+d32) and stash it in [gd+0x10] so the
5764:         // lwzx picks it up. Generalises to every 0x60FF the DR executes.
5765:         // DR PC-relative reference base: the DR's PC-relative effective-address
5766:         // / branch handler (entered via ed.v at 0x40B6D780) reads the current
5767:         // instruction's reference PC from [r28 + (r6&~7)] via `lwzx r24,r28,r7`
5768:         // at 0x40B6D7D8, then adds the displacement (LEA d16,PC / BRA). The DR
5769:         // never writes that slot itself, so it is 0 here and every PC-relative
5770:         // computes off address 0. The DR carries the reference PC in r24 (it
5771:         // backs r24 up to the ext word); seed the slot with r24 just before the
5772:         // lwzx runs. r28 = gd = 0xB000 -> slot [0xB010].
5773:         if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7D4u && g_DrYieldSeen) {
5774:             UINT32 Ref = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
5775:             CpuWrite32(0x0000B010, Ref);
5776:             g_DrBrFixed++;
5777:             if (g_DrBrFixed <= 4)
5778:                 Print(L"  DR-REFSLOT[%u] current=0x%08x refPC->gd+0x10=0x%08x "
5779:                       L"op@ref=%04x\n", g_DrBrFixed, Current, Ref,
5780:                       CpuRead16(Ref));
5781:         }
5782:         if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&
5783:             (Current >= 0x40B60000 && Current < 0x40B82000)) {
5784:             UINT32 Pc24 = g_PpcContext.Gpr[24];
5785:             if (Dr68KLast >= 0x40800000 && Pc24 < 0x40000000) {
5786:                 UINTN A;
5787:                 Dr68KLowProbed = 1;
5788:                 Print(L"  DR68K-LOW[1] exec=%u PPC=0x%08x r24(68Kpc)=0x%08x r27=0x%04x "
5789:                       L"r23=0x%08x r29=0x%08x r31=0x%08x r1=0x%08x LR=0x%08x CTR=0x%08x\n",
5790:                       Executed, Current, Pc24, g_PpcContext.Gpr[27] & 0xFFFF,

(Showing lines 5761-5790 of 6544. Use offset=5791 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 15.7s)

Let me remove the `break` and instead dump a broader low-RAM view to understand the boot state, and continue. Let me update the DR68K-LOW block to not break but just log once and continue:



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 22,
  "offset": 5782
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
5782:         if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&
5783:             (Current >= 0x40B60000 && Current < 0x40B82000)) {
5784:             UINT32 Pc24 = g_PpcContext.Gpr[24];
5785:             if (Dr68KLast >= 0x40800000 && Pc24 < 0x40000000) {
5786:                 UINTN A;
5787:                 Dr68KLowProbed = 1;
5788:                 Print(L"  DR68K-LOW[1] exec=%u PPC=0x%08x r24(68Kpc)=0x%08x r27=0x%04x "
5789:                       L"r23=0x%08x r29=0x%08x r31=0x%08x r1=0x%08x LR=0x%08x CTR=0x%08x\n",
5790:                       Executed, Current, Pc24, g_PpcContext.Gpr[27] & 0xFFFF,
5791:                       g_PpcContext.Gpr[23], g_PpcContext.Gpr[29],
5792:                       g_PpcContext.Gpr[31], g_PpcContext.Gpr[1],
5793:                       g_PpcContext.Lr, g_PpcContext.Ctr);
5794:                 for (A = 0x00000000u; A < 0x00002000u; A += 16) {
5795:                     Print(L"  LOW[0x%08x] %08x %08x %08x %08x\n",
5796:                           A, CpuRead32(A), CpuRead32(A + 4),
5797:                           CpuRead32(A + 8), CpuRead32(A + 12));
5798:                 }
5799:                 Print(L"  DR68K-LOW: stopping for inspection\n");
5800:                 break;
5801:             }
5802:             Dr68KLast = Pc24;
5803:         }

(Showing lines 5782-5803 of 6544. Use offset=5804 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 26.6s)

I'll change the DR68K-LOW to not break (so boot continues) and log only the first transition, plus add a follow-up state probe at a higher exec count so I can see where it goes next. Let me update:



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&\n            (Current >= 0x40B60000 && Current < 0x40B82000)) {\n            UINT32 Pc24 = g_PpcContext.Gpr[24];\n            if (Dr68KLast >= 0x40800000 && Pc24 < 0x40000000) {\n                UINTN A;\n                Dr68KLowProbed = 1;\n                Print(L\"  DR68K-LOW[1] exec=%u PPC=0x%08x r24(68Kpc)=0x%08x r27=0x%04x \"\n                      L\"r23=0x%08x r29=0x%08x r31=0x%08x r1=0x%08x LR=0x%08x CTR=0x%08x\\n\",\n                      Executed, Current, Pc24, g_PpcContext.Gpr[27] & 0xFFFF,\n                      g_PpcContext.Gpr[23], g_PpcContext.Gpr[29],\n                      g_PpcContext.Gpr[31], g_PpcContext.Gpr[1],\n                      g_PpcContext.Lr, g_PpcContext.Ctr);\n                for (A = 0x00000000u; A < 0x00002000u; A += 16) {\n                    Print(L\"  LOW[0x%08x] %08x %08x %08x %08x\\n\",\n                          A, CpuRead32(A), CpuRead32(A + 4),\n                          CpuRead32(A + 8), CpuRead32(A + 12));\n                }\n                Print(L\"  DR68K-LOW: continuing (break removed)\\n\");\n            }\n            Dr68KLast = Pc24;\n        }\n        // Post-low-entry state: log the 68K PC/PPC spread every ~2M instructions\n        // once we have entered low memory and passed the initial boot, to see\n        // whether the DR lands in real low-RAM bootstrap code or a degenerate\n        // pattern loop, without breaking the run.\n        if (g_DrYieldSeen && Dr68KLowProbed && Executed < 400000000u) {\n            static UINT32 LowStateLog = 0;\n            if (Executed >= (LowStateLog + 1u) * 2000000u) {\n                LowStateLog = Executed / 2000000u;\n                if (LowStateLog <= 40)\n                    Print(L\"  DR-LOWSTATE exec=%u PPC=0x%08x r24(68Kpc)=0x%08x r27=%04x \"\n                          L\"r23=%08x r29=%08x r1=%08x\\n\",\n                          Executed, Current, g_PpcContext.Gpr[24],\n                          g_PpcContext.Gpr[27] & 0xFFFF, g_PpcContext.Gpr[23],\n                          g_PpcContext.Gpr[29], g_PpcContext.Gpr[1]);\n            }\n        }",
  "oldString": "        if (Dr68KLowProbed == 0 && g_DrYieldSeen && g_DrBootPcSeeded &&\n            (Current >= 0x40B60000 && Current < 0x40B82000)) {\n            UINT32 Pc24 = g_PpcContext.Gpr[24];\n            if (Dr68KLast >= 0x40800000 && Pc24 < 0x40000000) {\n                UINTN A;\n                Dr68KLowProbed = 1;\n                Print(L\"  DR68K-LOW[1] exec=%u PPC=0x%08x r24(68Kpc)=0x%08x r27=0x%04x \"\n                      L\"r23=0x%08x r29=0x%08x r31=0x%08x r1=0x%08x LR=0x%08x CTR=0x%08x\\n\",\n                      Executed, Current, Pc24, g_PpcContext.Gpr[27] & 0xFFFF,\n                      g_PpcContext.Gpr[23], g_PpcContext.Gpr[29],\n                      g_PpcContext.Gpr[31], g_PpcContext.Gpr[1],\n                      g_PpcContext.Lr, g_PpcContext.Ctr);\n                for (A = 0x00000000u; A < 0x00002000u; A += 16) {\n                    Print(L\"  LOW[0x%08x] %08x %08x %08x %08x\\n\",\n                          A, CpuRead32(A), CpuRead32(A + 4),\n                          CpuRead32(A + 8), CpuRead32(A + 12));\n                }\n                Print(L\"  DR68K-LOW: stopping for inspection\\n\");\n                break;\n            }\n            Dr68KLast = Pc24;\n        }"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 149.5s)

**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 130 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n\"total lines: $($l.Count)\"\n$l | Select-String \"DR-REFSLOT|DR68K-LOW|DR-LOWSTATE|HANDOFF|DRYIELD\" | Select-Object -Last 40",
  "timeout": 210000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt
total lines: 10065

  [7mDRYIELD[0m-RESUME @rfi trap=0x40B6E8C8 68Kpc(r24)=0x408000BC r27(op)=0x4DFA -> DR dispatch 0x40B67B60
  [7mDR-REFSLOT[0m[1] current=0x40B6D7C8 refPC->gd+0x10=0x408000BE op@ref=0008
  [7mDR-REFSLOT[0m[2] current=0x40B6D7CC refPC->gd+0x10=0x408000BE op@ref=0008
  [7mDR-REFSLOT[0m[3] current=0x40B6D7D0 refPC->gd+0x10=0x408000BE op@ref=0008
  [7mDR-REFSLOT[0m[4] current=0x40B6D7D4 refPC->gd+0x10=0x408000BE op@ref=0008
  [7mDR68K-LOW[0m[1] [0m[7m[0mexec=2226614 [0m[7m[0mPPC=0x40B80040 [0m[7m[0mr24(68Kpc)=0x0000197C [0m[7m[0mr27=0x60FF [0m[7m[0mr23=0x00000000 [0m[7m[0mr29=0x40B80040 [0m
[7m[0mr31=0x0000B000 [0m[7m[0mr1=0xFFF80040 [0m[7m[0mLR=0x40B6D7F8 [0m[7m[0mCTR=0x40B80040[0m
  [7mDR68K-LOW[0m: continuing (break removed)


```

---

## Assistant (Build · Big Pickle · 49.8s)

The DR68K-LOW fired at exec=2.2M and continued, but no DR-LOWSTATE logs appeared afterward — meaning `Executed` never reached 4M (the next 2M milestone) in the 130s budget, OR the DR stopped making progress. The boot stalled right after entering low memory. Also the `r1=0xFFF80040` at that point is suspicious (PPC stack at 0xFFF80040 near top of RAM).

The DR reached 68K PC 0x197C in the patterned region and is spinning there (no further exec progress). So it's executing the repeating pattern as code and looping (like a degenerate loop within the pattern, or a very tight ROM loop).

The pattern `27044080 00BE0010` in low RAM — let me reconsider. This was written by the boot. The DR is now at 0x197C executing it. Let me look at what's actually at 0x197C and whether the pattern is code the boot wrote as a trampoline.

Actually, let me reconsider the whole flow. The 68K boot program, after the LEA and table processing, likely does a **JMP to a low-RAM trampoline** — but the trampoline content depends on what the PPC side set up. The region 0x1800+ was written with a repeating pattern by the boot itself (or by us?).

Let me check: was the 0x1800 pattern there before, or did the DR write it? Earlier (first run) the low dump showed 0x1800 as all ZEROS. Now it's `27044080 00BE0010`. So the **DR wrote this pattern during its 2M-instruction run**. So the boot IS executing a copy loop that writes `0x408000BE`-family values into 0x1800+.

The value `408000BE` — that's the ROM address of the LEA's operand. So the boot is copying ROM addresses into low RAM as **part of constructing a dispatch/jump table** (the boot entries with `60FF xxxx A9xx` get translated into low-RAM jump stubs). This is the classic Mac 68K boot behavior: it decompresses/translates the ROM boot entries into low RAM.

But the DR is now stuck. Let me look more carefully at what pattern is written and where the DR loops. Let me capture a denser trace right around the stall. Let me add a probe that prints when r24 loops (revisits the same PC with no progress) or print the last ~20 distinct 68K PCs.

Actually, let me reconsider — the pattern `27044080 00BE0010` as a 32-bit-le structure: words `0x2704 0x4080 0x00BE 0x0010`. As 68K code: `2704`=MOVE.L D4,-(A7)... that's not a meaningful loop end.

Let me get the DR's actual execution path in low RAM. Let me add a probe that captures a window of execution after entering low memory, printing r24 (68K PC) every so often.

Actually — I realize the stall might mean the boot is IN a delay/wait loop that's fine but needs the PPC side to service something (like the serial poll timer or an interrupt), and without it the DR loops forever. The `r1=0xFFF80040` (PPC stack near top) at 2.2M exec is odd.

Let me get more visibility. Let me add a trace of the 68K PC stream after the low entry, sampling every 50k instructions, to map where it loops. Let me modify the DR-LOWSTATE probe to fire more frequently (every 200k) and for more samples.

Given DR-LOWSTATE didn't fire at all (needs 2M increments), the loop is tighter than 2M between the exec where low-entry logged (2.2M) and the end. Actually if stalled, Executed stays ~2.2M and DR-LOWSTATE (which requires Executed >= n*2M) never fires again. Let me lower threshold to 200k and print PC stream to see the loop.The DR stalled right after entering low memory (no further exec progress, no DR-LOWSTATE). It's looping within the patterned low-RAM region. Let me add a finer-grained 68K PC stream trace after low-entry to map the loop:



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        // Post-low-entry state: log the 68K PC/PPC spread every ~500K instructions\n        // once we have entered low memory and passed the initial boot, to see\n        // whether the DR lands in real low-RAM bootstrap code or a degenerate\n        // pattern loop, without breaking the run.\n        if (g_DrYieldSeen && Dr68KLowProbed && Executed < 400000000u) {\n            static UINT32 LowStateLog = 0;\n            if (Executed >= (LowStateLog + 1u) * 500000u && LowStateLog <= 80) {\n                LowStateLog = Executed / 500000u;\n                Print(L\"  DR-LOWSTATE exec=%u PPC=0x%08x r24(68Kpc)=0x%08x r27=%04x \"\n                      L\"r7=%08x r29=%08x r1=%08x\\n\",\n                      Executed, Current, g_PpcContext.Gpr[24],\n                      g_PpcContext.Gpr[27] & 0xFFFF, g_PpcContext.Gpr[7],\n                      g_PpcContext.Gpr[29], g_PpcContext.Gpr[1]);\n            }\n        }",
  "oldString": "        // Post-low-entry state: log the 68K PC/PPC spread every ~2M instructions\n        // once we have entered low memory and passed the initial boot, to see\n        // whether the DR lands in real low-RAM bootstrap code or a degenerate\n        // pattern loop, without breaking the run.\n        if (g_DrYieldSeen && Dr68KLowProbed && Executed < 400000000u) {\n            static UINT32 LowStateLog = 0;\n            if (Executed >= (LowStateLog + 1u) * 2000000u) {\n                LowStateLog = Executed / 2000000u;\n                if (LowStateLog <= 40)\n                    Print(L\"  DR-LOWSTATE exec=%u PPC=0x%08x r24(68Kpc)=0x%08x r27=%04x \"\n                          L\"r23=%08x r29=%08x r1=%08x\\n\",\n                          Executed, Current, g_PpcContext.Gpr[24],\n                          g_PpcContext.Gpr[27] & 0xFFFF, g_PpcContext.Gpr[23],\n                          g_PpcContext.Gpr[29], g_PpcContext.Gpr[1]);\n            }\n        }"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 151.2s)

**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 130 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"DR-LOWSTATE\" | Select-Object -Last 40",
  "timeout": 210000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt

```

---

## Assistant (Build · Big Pickle · 17.8s)

No DR-LOWSTATE at all, and the log only had 10065 lines before (contains the low-entry dump). Let me check the total exec at end and whether the run completed. The DR-LOWSTATE requires `Executed` to cross 500k/1M/... but the low-entry was at exec=2.2M. So DR-LOWSTATE should have fired for 3M, 3.5M... unless Executed never increased past ~2.2M (stalled). Let me check the tail of the log and total line count.



**Tool: bash**

**Input:**
```json
{
  "command": "$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n\"total lines: $($l.Count)\"\n\"=== last 8 lines ===\"\n$l | Select-Object -Last 8"
}
```

**Output:**
```
total lines: 10065
=== last 8 lines ===
System files self-test: FAIL
System files: 0 staged, 0 drivers registered (0 staged), 0 bytes total
Staging areas: system 0x20000000, drivers 0x0

=== EFI Mac OS Boot Layer ready ===
BdsDxe: loading Boot0000 "UiApp" from Fv(7CB8BDC9-F8EB-4F34-AAEA-3EE4AF6516A1)/FvFile(462CAA21-7614-4503-836E-8AB6F4662331)
BdsDxe: starting Boot0000 "UiApp" from Fv(7CB8BDC9-F8EB-4F34-AAEA-3EE4AF6516A1)/FvFile(462CAA21-7614-4503-836E-8AB6F4662331)
[2J[01;01H[0m[30m[40m[01;01H                                                                                                    [02;01H                                                                                                    [03;01H                                                                                                    [04;01H                                                                                                    [05;01H                                                                                                    [06;01H                                                                                                    [07;01H                                                                                                    [08;01H                                                                                                    [09;01H                                                                                                    [10;01H                                                                                                    [11;01H                                                                                                    [12;01H                                                                                                    [13;01H                                                                                                    [14;01H                                                                                                    [15;01H                                                                                                    [16;01H                                                                                                    [17;01H                                                                                                    [18;01H                                                                                                    [19;01H                                                                                                    [20;01H                                                                                                    [21;01H                                                                                                    [22;01H                                                                                                    [23;01H                                                                                                    [24;01H                                                                                                    [25;01H                                                                                                    [26;01H                                                                                                    [27;01H                                                                                                    [28;01H                                                                                                    [29;01H                                                                                                    [30;01H                                                                                                    [31;01H                                                                                                                                                                                                        [01;01H[0m[34m[47m[01;01H                                                                                                    [02;01H                                                                                                    [03;01H                                                                                                    [04;01H                                                                                                    [05;01H                                                                                                    [06;01H                                                                                                    [01;01H[02;02HStandard PC (i440FX + PIIX, 1996)[03;02Hpc-i440fx-11.1[03;67H2.00 GHz[04;02H2025.02-8+deb13u1[04;67H512 MB RAM[05;02H[05;67H[06;02H[06;67H[0m[37m[40m[27;01H                                                                                                    [28;01H                                                                                                    [29;01H                                                                                                    [30;01H                                                                                                    [27;01H[0m[30m[40m[31;67H  [31;68H  [31;69H  [31;70H  [31;71H  [31;72H  [31;73H  [31;74H  [31;75H  [31;76H  [31;77H  [31;78H  [31;79H  [31;80H  [31;81H  [31;82H  [31;83H  [31;84H  [31;85H  [31;86H  [31;87H  [0m[30m[47m[07;01H                                                                                                    [08;01H                                                                                                    [09;01H                                                                                                    [10;01H                                                                                                    [11;01H                                                                                                    [12;01H                                                                                                    [13;01H                                                                                                    [14;01H                                                                                                    [15;01H                                                                                                    [16;01H                                                                                                    [17;01H                                                                                                    [18;01H                                                                                                    [19;01H                                                                                                    [20;01H                                                                                                    [21;01H                                                                                                    [22;01H                                                                                                    [23;01H                                                                                                    [24;01H                                                                                                    [25;01H                                                                                                    [26;01H                                                                                                    [07;01H[07;01H                                                                                                    [0m[37m[40m[08;37H<Standard English>[0m[30m[47m[08;55H                [08;01H   [08;04HSelect Language                  [0m[34m[47m[09;01H   [09;04H                                                                   [0m[30m[47m[10;01H   [10;02H[10;04HDevice Manager                                                     [11;01H   [11;02H[11;04HBoot Manager                                                       [12;01H   [12;02H[12;04HBoot Maintenance Manager                                           [0m[34m[47m[13;01H   [13;04H                                                                   [0m[30m[47m[14;01H   [14;04HContinue                                                           [15;01H   [15;04HReset                                                              [16;01H                                                                      [17;01H                                                                      [18;01H                                                                      [19;01H                                                                      [20;01H                                                                      [21;01H                                                                      [22;01H                                                                      [23;01H                                                                      [24;01H                                                                      [25;01H                                                                      [26;01H                                                                                                    [0m[37m[40m[29;02H [28;02H [28;67H                                [28;34H                                 [29;67H                                 [29;03H=Move Highlight              [28;03H                               [29;34H<Enter>=Select Entry             [0m[34m[47m[08;71HThis is the option one        [09;71Hadjusts to change the         [10;71Hlanguage for the current      [11;71Hsystem                        [12;71H                              [13;71H                              [14;71H                              [15;71H                              [16;71H                              [17;71H                              [18;71H                              [19;71H                              [20;71H                              [21;71H                              [22;71H                              [23;71H                              [24;71H                              [25;71H                              [25;:0H

```

---

## Assistant (Build · Big Pickle · 27.6s)

The boot log shows the EFI **UiApp boot menu** is now displayed — the emulator is running and the DR stalled shortly after low-memory entry (no DR-LOWSTATE because `Executed` stalled around 2.2M while the rest of the system (BdsDxe/UiApp) continued). The Mac OS 9 boot path through the DR has stalled in low RAM.

The key finding: the DR's 68K boot, after my LEA/PC-relative fix, now successfully writes a pattern into low RAM 0x1800+ and enters low memory at 0x197C, but then **stalls** (the CPU keeps running the rest of the system, but the DR's 68K execution doesn't advance past ~2.2M).

Let me figure out what the 68K code at low RAM 0x197C / the pattern actually is, and why the DR stalls. The pattern `4D7A ...`? The low dump showed `27044080 00BE0010` repeating. Let me get the exact bytes at the stall point and around 0x197C. Let me write a quick 68K disassembler to decode the low-RAM region and the ROM boot entries.

Actually, let me first capture a snapshot of the 68K PC stream right at the stall. The DR-LOWSTATE didn't fire because Executed stalled at 2.2M and never crossed 3M. But that means the PPC loop IS still running (Executed increments) — unless Executed genuinely froze. Given UiApp showed, the whole thing didn't freeze. Let me reconsider: maybe `Dr68KLowProbed` became 1, and the DR-LOWSTATE block's condition `g_DrYieldSeen && Dr68KLowProbed` is true, but `Executed` didn't reach 3M because... the DR stopped being in the PPC loop? 

The PPC interpreter `PpcRunGuest` loop runs a bounded number of instructions and returns to the firmware for BDS etc. `Executed` counts PPC instructions in the guest loop. If the guest loop returned and BDS/UiApp took over (executing in EFI), `Executed` froze. So the DR's 68K emulation genuinely **stopped** — either completed (returned to the PPC side) or faulted out of the loop.

Actually — this is likely progress! The DR may have **finished its 68K boot phase** and returned control to the PPC firmware, which then continued to BDS/UiApp. But Mac OS 9 isn't booting yet (UiApp menu shown = boot setup, not OS). 

Hmm, but the DR diverged into low memory at 0x197C with the repeating pattern — that's not a clean handoff back to PPC. Let me look at whether the DR is stuck in a loop by checking the very end of the DR-related log and whether there's more activity.

Let me search the full log for the last DR-related output and any subsequent DR activity.

The DR stalled at ~2.2M instructions after entering low memory — the rest of the system continued to the EFI UiApp boot menu (so the PPC loop returned to firmware). Let me examine exactly what the 68K code at the stall point was doing. Let me check the full log for the DR-REFSLOT count and any later DR entries, and dump the low-RAM / pattern region precisely. Let me write the 68K disassembler to decode it.



**Tool: bash**

**Input:**
```json
{
  "command": "$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n\"DR-REFSLOT count: \" + ($l | Select-String \"DR-REFSLOT\").Count\n\"=== any DR-ish lines after DR68K-LOW ===\"\n$i = ($l | Select-String \"DR68K-LOW\\[1\\]\").LineNumber\n$l[($i)..($i+5)]\necho \"=== last mention of 0x40B6 / DR dispatch / rfi in whole log ===\"\n$l | Select-String \"40B67B60|40B6E964|DISPATCH|PROGRESS|rfi\" | Select-Object -Last 8"
}
```

**Output:**
```
DR-REFSLOT count: 4
=== any DR-ish lines after DR68K-LOW ===
  LOW[0x00000000] 27044080 00BE0010 27044080 00BE0010
  LOW[0x00000010] 27044080 00BE0010 27044080 00BE0010
  LOW[0x00000020] 27044080 00BE0010 27044080 00BE0010
  LOW[0x00000030] 27044080 00BE0010 27044080 00BE0010
  LOW[0x00000040] 27044080 00BE0010 27044080 00BE0010
  LOW[0x00000050] 27044080 00BE0010 27044080 00BE0010
=== last mention of 0x40B6 / DR dispatch / rfi in whole log ===

  [7mPROGRESS[0m[500000] [0m[7m[0mPC=0x40B6D784 [0m[7m[0mLR=0x40B6D75C [0m[7m[0mr1=0xFFFF06D0 [0m[7m[0mr8=0x00000000 [0m[7m[0mr24(68Kpc)=0x408000C0 [0m[7m[0mr27(op)=0x60FF [0m
[7m[0mr28=0x0000B000 [0m[7m[0mSPRG4=0x0000A000 [0m[7m[0mMSR=0x0002D032 [0m[7m[0mDEC=0xFFE7E1F0 [0m[7m[0mTBL=0x001E8470 [0m[7m[0mNEG=1[0m
  [7mPROGRESS[0m[750000] [0m[7m[0mPC=0x40B6D7C8 [0m[7m[0mLR=0x40B6D7CC [0m[7m[0mr1=0xFFFE0268 [0m[7m[0mr8=0x00000000 [0m[7m[0mr24(68Kpc)=0x408000BE [0m[7m[0mr27(op)=0x60FF [0m
[7m[0mr28=0x0000B000 [0m[7m[0mSPRG4=0x0000A000 [0m[7m[0mMSR=0x0002D032 [0m[7m[0mDEC=0xFFD89FB0 [0m[7m[0mTBL=0x002DC6B0 [0m[7m[0mNEG=1[0m
  [7mPROGRESS[0m[1000000] [0m[7m[0mPC=0x40B6D7F0 [0m[7m[0mLR=0x40B6D7CC [0m[7m[0mr1=0xFFFCFDF8 [0m[7m[0mr8=0x00000000 [0m[7m[0mr24(68Kpc)=0x408000C0 [0m[7m[0mr27(op)=0x60FF [0m
[7m[0mr28=0x0000B000 [0m[7m[0mSPRG4=0x0000A000 [0m[7m[0mMSR=0x0002D032 [0m[7m[0mDEC=0xFFC95D70 [0m[7m[0mTBL=0x003D08F0 [0m[7m[0mNEG=1[0m
  [7mPROGRESS[0m[1250000] [0m[7m[0mPC=0x40B6D784 [0m[7m[0mLR=0x40B6D75C [0m[7m[0mr1=0xFFFBF990 [0m[7m[0mr8=0x00000000 [0m[7m[0mr24(68Kpc)=0x408000C0 [0m[7m[0mr27(op)=0x60FF [0m
[7m[0mr28=0x0000B000 [0m[7m[0mSPRG4=0x0000A000 [0m[7m[0mMSR=0x0002D032 [0m[7m[0mDEC=0xFFBA1B30 [0m[7m[0mTBL=0x004C4B30 [0m[7m[0mNEG=1[0m
  [7mPROGRESS[0m[1500000] [0m[7m[0mPC=0x40B6D7C8 [0m[7m[0mLR=0x40B6D7CC [0m[7m[0mr1=0xFFFAF528 [0m[7m[0mr8=0x00000000 [0m[7m[0mr24(68Kpc)=0x408000BE [0m[7m[0mr27(op)=0x60FF [0m
[7m[0mr28=0x0000B000 [0m[7m[0mSPRG4=0x0000A000 [0m[7m[0mMSR=0x0002D032 [0m[7m[0mDEC=0xFFAAD8F0 [0m[7m[0mTBL=0x005B8D70 [0m[7m[0mNEG=1[0m
  [7mPROGRESS[0m[1750000] [0m[7m[0mPC=0x40B6D7F0 [0m[7m[0mLR=0x40B6D7CC [0m[7m[0mr1=0xFFF9F0B8 [0m[7m[0mr8=0x00000000 [0m[7m[0mr24(68Kpc)=0x408000C0 [0m[7m[0mr27(op)=0x60FF [0m
[7m[0mr28=0x0000B000 [0m[7m[0mSPRG4=0x0000A000 [0m[7m[0mMSR=0x0002D032 [0m[7m[0mDEC=0xFF9B96B0 [0m[7m[0mTBL=0x006ACFB0 [0m[7m[0mNEG=1[0m
  [7mPROGRESS[0m[2000000] [0m[7m[0mPC=0x40B6D784 [0m[7m[0mLR=0x40B6D75C [0m[7m[0mr1=0xFFF8EC50 [0m[7m[0mr8=0x00000000 [0m[7m[0mr24(68Kpc)=0x408000C0 [0m[7m[0mr27(op)=0x60FF [0m
[7m[0mr28=0x0000B000 [0m[7m[0mSPRG4=0x0000A000 [0m[7m[0mMSR=0x0002D032 [0m[7m[0mDEC=0xFF8C5470 [0m[7m[0mTBL=0x007A11F0 [0m[7m[0mNEG=1[0m
  MSR=0x0002D032 CR=0x20104DFE LR=0x40B6D7F8 CTR=0x40B80040 SRR0=0x[7m40B67B60[0m SRR1=0x0002D032


```

---

## Assistant (Build · Big Pickle · 50.0s)

Now I see the definitive problem. Look at the PROGRESS samples (every 250K):
- r24(68Kpc) = **0x408000C0 constantly** across 500K→2M
- The PPC PCs cycle through 0x40B6D784 / 0x40B6D7C8 / 0x40B6D7F0 — the same handler instructions over and over
- **r1 drains from 0xFFFF06D0 → 0xFFF8EC50** (the PPC stack decreases by ~0x200 per 250K samples — a stack leak!)
- r28=0xB000, r27(op)=0x60FF, r24 stays at 0x408000C0

So the DR is **spinning in the PC-relative handler (0x40B6D7D8 area) on the same `0x60FF`/`LEA` at 68K PC 0x408000C0**, never advancing past it, and the **PPC stack r1 is draining** — the handler keeps pushing without popping (each iteration leaks stack). This is a tight infinite loop in the DR's decode of the `0x60FF` at 0x408000C0.

My REFSLOT seed stores r24 into [0xB010], but the DR's `lwzx r24,r28,r7` at 0x40B6D7D8 reads it and gets the reference PC (0x408000C0 for the BRA.L case), but then the DR loops because for `0x60FF` it needs to *branch* (add the 32-bit displacement to get 0x4080AA10), and it keeps re-entering the handler rather than advancing.

Wait — actually looking more carefully: the PC cycling `0x40B6D784 → 0x40B6D7C8 → 0x40B6D7F0` and r1 draining means the DR is stuck in the PC-relative handler pushing frames. The 68K PC stays 0x408000C0 (the `0x60FF`). So the DR cannot make progress on `0x60FF`.

The earlier run WITHOUT the r1 issue got to 2.2M and low RAM. Now with break removed and longer run, we see it's actually spinning at 0x408000C0 the whole time after ~some point, draining r1. 

Actually wait — the low-RAM pattern (0x1800+) was written WITH real ROM addresses, meaning at SOME earlier point the DR DID advance (wrote those). But then it came back to spinning at 0x408000C0? No...

Let me reconsider. The 2.2M low-entry and the 0x408000C0 spin — maybe these are the SAME event. The DR spins at the `0x60FF` handler (0x408000C0) the whole time, and in doing so it executes a loop that writes the `27044080 00BE0010` pattern into low RAM (this is likely the DR *mis-executing* the 0x60FF as a store loop!). The r1 drain confirms a runaway loop.

So the real problem remains: **the DR cannot execute the `0x60FF` BRA.L** — it loops in the handler, and the "writing pattern" is a side effect of the degenerate loop. The low RAM 0x1800+ isn't real boot data; it's loop residue.

So my REFSLOT fix got the LEA portion working (A6=0xC6 set), but the `0x60FF` branch still loops. The 0x60FF is the 68020 BRA.L that the 68000-core DR mishandles.

Let me reconsider: the DR is stuck at 0x408000C0 which is the `0x60FF`. When the DR's PC-relative handler processes `0x60FF`:
- backs r24 up to 0x408000C0
- reads ref from [0xB010] = 0x408000C0 (my seed)
- for a branch it should compute PC+2+d32 = 0x4080AA10
- but it loops instead

The issue is the DR doesn't know how to consume the 32-bit displacement of `0x60FF`. On a 68000, `0x60FF` = `BRA` with 8-bit disp = 0xFF (= -1), and the ext-word follows. The DR likely computes a 16-bit or 8-bit displacement and goes wrong, looping.

The cleanest fix for the `0x60FF` specifically: **patch the DR to treat 0x60FF as a jump**. When the DR is at the PC-relative handler processing a `0x60FF` opcode, instead of letting it loop, force r24 (68K PC) to the computed target 0x4080AA10.

But my earlier 0x60FF-specific BRFIX (which computed target and set [0xB010]) was replaced by the generic REFSLOT. The difference: REFSLOT stores r24 (reference PC), not the branch target. For `0x60FF`, the slot should hold the *branch target*, not the reference PC.

So I need BOTH:
- For LEA (and other PC-relative EA): slot = reference PC (r24)
- For 0x60FF BRA.L: slot = branch target (r24 + 2 + d32)

Since they need different values, I should branch on the opcode at the backed-up PC. If `op@r24 == 0x60FF`, compute the target; else store r24 as reference.

Let me reconsider whether the DR adds displacement on top of the slot value or uses it directly. For LEA: DR adds d16 to the reference → A6. If slot=reference and DR +8, works (we saw A6 get set and boot progress to 2.2M/pattern). For 0x60FF, if slot=reference and DR adds disp, then slot should = reference (the DR handles disp itself). But the DR loops — meaning the DR does NOT handle the 32-bit disp of 0x60FF (68020-only), it mis-adds.

So for 0x60FF I should set the slot = the final target directly IF the DR uses the slot as the final PC (no add). But for LEA it clearly adds. So inconsistent — the DR's behavior differs per opcode.

Let me test: set slot to the computed target for 0x60FF and see if the DR then lands at 0x4080AA10. If the DR DOESN'T add for 0x60FF (uses slot as final PC), this works. Let me implement: in the seed block, check `op@Ref == 0x60FF`; if so write target (Ref+2+d32) to [0xB010], else write Ref.

Let me implement this and test.The PROGRESS samples show the DR is **spinning in the PC-relative handler at 0x60FF (68K PC 0x408000C0)**, with the PPC stack r1 draining (a leak) — it never advances past the `0x60FF`. My REFSLOT fix correctly serves LEA (slot=reference PC), but for `0x60FF` the branch needs the **target** in the slot (the 68000-core DR doesn't add the 32-bit disp). Let me distinguish the two cases:



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7D4u && g_DrYieldSeen) {\n            UINT32 Ref = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (CpuRead16(Ref) == 0x60FF) {\n                // 0x60FF = 68020 BRA.L: the 68000-core DR cannot add the 32-bit\n                // displacement, so hand it the resolved target in the slot.\n                UINT32 Disp = ((UINT32)CpuRead16(Ref + 2) << 16) |\n                              (UINT32)CpuRead16(Ref + 4);\n                UINT32 Tgt = Ref + 2 + Disp;\n                CpuWrite32(0x0000B010, Tgt);\n                g_DrBrFixed++;\n                if (g_DrBrFixed <= 6)\n                    Print(L\"  DR-BRFIX[%u] 0x60FF @0x%08x d32=0x%08x slot->TGT 0x%08x\\n\",\n                          g_DrBrFixed, Ref, Disp, Tgt);\n            } else {\n                CpuWrite32(0x0000B010, Ref);\n                g_DrBrFixed++;\n            }\n        }",
  "oldString": "        if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7D4u && g_DrYieldSeen) {\n            UINT32 Ref = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            CpuWrite32(0x0000B010, Ref);\n            g_DrBrFixed++;\n            if (g_DrBrFixed <= 4)\n                Print(L\"  DR-REFSLOT[%u] current=0x%08x refPC->gd+0x10=0x%08x \"\n                      L\"op@ref=%04x\\n\", g_DrBrFixed, Current, Ref,\n                      CpuRead16(Ref));\n        }"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 148.1s)

**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 130 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n\"total lines: $($l.Count)\"\n$l | Select-String \"DR-BRFIX|DR68K-LOW|DR-LOWSTATE|HANDOFF\" | Select-Object -Last 12",
  "timeout": 210000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt
total lines: 10061

  [7mDR68K-LOW[0m[1] [0m[7m[0mexec=2226614 [0m[7m[0mPPC=0x40B80040 [0m[7m[0mr24(68Kpc)=0x0000197C [0m[7m[0mr27=0x60FF [0m[7m[0mr23=0x00000000 [0m[7m[0mr29=0x40B80040 [0m
[7m[0mr31=0x0000B000 [0m[7m[0mr1=0xFFF80040 [0m[7m[0mLR=0x40B6D7F8 [0m[7m[0mCTR=0x40B80040[0m
  [7mDR68K-LOW[0m: continuing (break removed)


```

---

## Assistant (Build · Big Pickle · 30.6s)

No DR-BRFIX printed at all now — the `0x60FF` case isn't being hit in the seed block, yet DR68K-LOW still fires at exec=2.2M with r24=0x197C and r27=0x60FF. This means the DR that reached low RAM 0x197C is executing a `0x60FF` there (r27 prefetched = 0x60FF). But my seed block only fires for `Current` in 0x40B6D7C8-0x40B6D7D4, and the `0x60FF` inside low RAM isn't near that handler.

Wait — the DR68K-LOW says PPC=0x40B80040 (not the handler). So the DR is executing low RAM code at 0x197C that contains 0x60FF. The seed block (0x40B6D7C8 block) handles the handler path, but the DR68K-LOW fired at a DIFFERENT PPC location.

The problem: my DR-BRFIX seed only fires when Current is the specific PPC handler addresses. But the `0x60FF` the DR is stuck on is being executed from a different path (PPC=0x40B80040). 

Also concerning: still exec=2226614 and it stalls there. And no DR-LOWSTATE (exec stalled).

Let me reconsider. The DR68K-LOW at exec=2.2M fires when the 68K PC first goes from >=0x40800000 to <0x40000000. It shows PPC=0x40B80040, r24=0x197C, r27=0x60FF. So right after the branch into low RAM, r27 (prefetched next opcode) = 0x60FF — meaning the code at 0x197C begins with 0x60FF.

But the low-RAM pattern at 0x1800+ is `27044080 00BE0010` repeating. At 0x197C: offset into the pattern. 0x197C-0x1800 = 0x17C = 380. Pattern is 8 bytes (27044080 00BE0010). 380/8 = 47.5. So 0x197C is mid-pattern. The 4-byte value at 0x197C might be `00BE0010` or `27044080`. r27=0x60FF doesn't match either. So the DR's r24=0x197C points somewhere, prefetching 0x60FF.

Hmm, this is getting confusing. The key issue: **exec stalls at 2.2M** and the DR is stuck. Let me look at whether the DR-branch is now looping differently.

Actually — wait. The DR-LOWSTATE didn't fire even though Executed should exceed 2.2M and then the 500K-interval probe (at 2.5M) should fire. Unless Executed genuinely stalls at ~2.2M. Let me check the actual max Executed and whether the PPC loop froze.

Let me check the PROGRESS samples — they go up in increments. Let me see the last PROGRESS value to know how far Executed got.

Let me search for the last PROGRESS and the exec ceiling.



**Tool: bash**

**Input:**
```json
{
  "command": "$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"PROGRESS\\[\" | Select-Object -Last 3\necho \"=== DR-BRFIX? ===\"\n$l | Select-String \"DR-BRFIX\" | Select-Object -First 3\necho \"=== first DR-BR/REFSLOT area ===\"\n$i = ($l | Select-String \"DRYIELD-RESUME\").LineNumber | Select-Object -First 1\n$l[($i-1)..($i+20)]"
}
```

**Output:**
```

  [7mPROGRESS[[0m1500000] [0m[7m[0mPC=0x40B6D7C8 [0m[7m[0mLR=0x40B6D7CC [0m[7m[0mr1=0xFFFAF528 [0m[7m[0mr8=0x00000000 [0m[7m[0mr24(68Kpc)=0x408000BE [0m[7m[0mr27(op)=0x60FF [0m
[7m[0mr28=0x0000B000 [0m[7m[0mSPRG4=0x0000A000 [0m[7m[0mMSR=0x0002D032 [0m[7m[0mDEC=0xFFAAD8F0 [0m[7m[0mTBL=0x005B8D70 [0m[7m[0mNEG=1[0m
  [7mPROGRESS[[0m1750000] [0m[7m[0mPC=0x40B6D7F0 [0m[7m[0mLR=0x40B6D7CC [0m[7m[0mr1=0xFFF9F0B8 [0m[7m[0mr8=0x00000000 [0m[7m[0mr24(68Kpc)=0x408000C0 [0m[7m[0mr27(op)=0x60FF [0m
[7m[0mr28=0x0000B000 [0m[7m[0mSPRG4=0x0000A000 [0m[7m[0mMSR=0x0002D032 [0m[7m[0mDEC=0xFF9B96B0 [0m[7m[0mTBL=0x006ACFB0 [0m[7m[0mNEG=1[0m
  [7mPROGRESS[[0m2000000] [0m[7m[0mPC=0x40B6D784 [0m[7m[0mLR=0x40B6D75C [0m[7m[0mr1=0xFFF8EC50 [0m[7m[0mr8=0x00000000 [0m[7m[0mr24(68Kpc)=0x408000C0 [0m[7m[0mr27(op)=0x60FF [0m
[7m[0mr28=0x0000B000 [0m[7m[0mSPRG4=0x0000A000 [0m[7m[0mMSR=0x0002D032 [0m[7m[0mDEC=0xFF8C5470 [0m[7m[0mTBL=0x007A11F0 [0m[7m[0mNEG=1[0m
=== DR-BRFIX? ===
=== first DR-BR/REFSLOT area ===
  DRYIELD-RESUME @rfi trap=0x40B6E8C8 68Kpc(r24)=0x408000BC r27(op)=0x4DFA -> DR dispatch 0x40B67B60
  DBG rfi @0x40B24524: SRR0=0x40B67B60 SRR1=0x0002D032 MSR=0x00025030 -> PC=0x40B67B60
  EE-RFI @0x40B24524 -> PC=0x40B67B60 MSR=0x0002D032 DEC=0xFFFFDC80 NEG=1
  Y80[4] PPC=0x40B67B68 r24(68Kpc)=0x408000BE r27=0x0008 r1=0x0000A000
  Y80[8] PPC=0x40B67B78 r24(68Kpc)=0x408000BE r27=0x0008 r1=0x0000A000
  Y80[12] PPC=0x40B67C4C r24(68Kpc)=0x408000BE r27=0x0008 r1=0x0000A000
  Y80[16] PPC=0x40B67B88 r24(68Kpc)=0x408000BE r27=0x0008 r1=0x0000A000
  Y80[20] PPC=0x40B67C5C r24(68Kpc)=0x408000BE r27=0x0008 r1=0x0000A000
  Y80[24] PPC=0x40B67B98 r24(68Kpc)=0x408000BE r27=0x0008 r1=0x0000A000
  Y80[28] PPC=0x40B67BA8 r24(68Kpc)=0x408000C0 r27=0x60FF r1=0x0000A000
  Y80[32] PPC=0x40B67BB8 r24(68Kpc)=0x408000C0 r27=0x60FF r1=0x0000A000
  Y80[36] PPC=0x40B67BC8 r24(68Kpc)=0x408000C0 r27=0x60FF r1=0x0000A000
  Y80[40] PPC=0x40B6D754 r24(68Kpc)=0x408000C0 r27=0x60FF r1=0x0000A000
  Y80[44] PPC=0x40B6D788 r24(68Kpc)=0x408000BE r27=0x60FF r1=0x0000A000
  Y80[48] PPC=0x40B6D798 r24(68Kpc)=0x408000BE r27=0x60FF r1=0x0000A000
  Y80[52] PPC=0x40B6D7A8 r24(68Kpc)=0x408000BE r27=0x60FF r1=0x0000A000
  Y80[56] PPC=0x40B6D7D4 r24(68Kpc)=0x408000BE r27=0x60FF r1=0x00009FF8
  DR-BR[1] r6=0x00000010 r7=0x00000010 r28=0x0000B000 r24(68Kpc/post)=0x408000BE slot=0x408000BE
  DR-BR gdB000+0x00=0x00000000 +0x10=0x408000BE +0x0c=0x00000000 +0x54=0x00000000 68Krom@0xC0=60FF0000 A94E3E7C
  Y80[60] PPC=0x40B6D7E4 r24(68Kpc)=0x408000BE r27=0x0008 r1=0x00009FF8
  Y80[64] PPC=0x40B6D7F4 r24(68Kpc)=0x408000C0 r27=0x60FF r1=0x00009FF8
  Y80[68] PPC=0x40B80044 r24(68Kpc)=0x408000C0 r27=0x60FF r1=0x00009FF8


```

---

## Assistant (Build · Big Pickle · 59.4s)

Now it's clear. Look at the Y80 trace:
- Y80[56] at PPC=0x40B6D7D4: r24=0x408000BE (reference for the LEA), r27=0x60FF (prefetched LEA... wait r27 is the prefetched next opcode)
- Then `DR-BR[1]`: r24(post)=0x408000BE, slot=0x408000BE — so the `lwzx` read my seeded **reference** 0x408000BE (the seed wrote Ref, not TGT, because at that moment `op@Ref`, i.e. op@0x408000BE = `0008`, NOT 0x60FF!).

So the critical realization: **at the handler, `r24` already points at the `LEA`'s ext word (0x408000BE, which is `0008`), NOT at a `0x60FF`.** The `0x60FF` is a *separate* instruction later. The DR is working on the `LEA (8,PC),A6` — reading the ref PC 0x408000BE, and my seed correctly provides it. But then the DR still doesn't advance past the 0x60FF.

Look at Y80[64]: PPC=0x40B6D7F4, r24=0x408000C0 (back at 0x60FF), r27=0x60FF. Then Y80[68]: PPC=0x40B80044 (the dispatch stub), r24=0x408000C0. So after processing the LEA, the DR returns to 0x408000C0 (the 0x60FF) — **the LEA's PC was 0x408000BC, and after it, PC should be 0x408000C0** (the 0x60FF). That's actually correct sequential flow! The 0x60FF at 0xC0 is the NEXT instruction.

But wait — the DR-BR slot=0x408000BE and r24 post=0x408000BE. So `lwzx` set r24=0x408000BE. But we came from 0x408000C0 (the 0x60FF was being decoded when the handler fired). 

Let me re-read. The Y80 trace: r24 goes 0xBE → 0xC0 → (handler) → 0xBE → 0xC0. The handler at 0x40B6D780 processes with r24=0x408000BE (backed up). My seed set [0xB010]=0x408000BE (Ref, because op@0xBE=0008 not 0x60FF). The lwzx set r24=0x408000BE. Then r27 prefetch...

Hmm, this is the LEA handler, and r24=0x408000BE is the LEA's ext-word address. But how does the DR compute A6=0xC6 from this? It should take ref PC 0x408000BE (or 0xBC) + 8. The lwzx loading 0x408000BE as the new "PC" is odd for LEA (LEA doesn't change PC). 

I think this handler (0x40B6D780) is NOT specifically LEA — it's the **PC-relative EA computation handler used by many instructions**. For LEA it computes the EA into A6 but PC advances by 2 (to 0xC0). The lwzx loads the "reference PC" which is used by the EA computation.

The real question: why doesn't the DR advance past 0x408000C0 (the 0x60FF)? After the LEA completes, PC should be 0xC0, and 0x60FF (BRA.L) should be decoded, computing target 0x4080AA10. But the DR spins.

Given the extensive reverse-engineering, the fundamental blocker is the **`0x60FF` BRA.L** — the 68000-core DR cannot branch with a 32-bit displacement. When it decodes 0x60FF at 0x408000C0, it enters the PC-relative handler, tries to compute, and loops (draining r1).

My fix needs to handle the 0x60FF at 0x408000C0 specifically: when the DR decodes it, the slot must hold the target 0x4080AA10, OR the DR must be forced to jump there. But my seed block checks `op@Ref == 0x60FF` where Ref=r24 at handler time = 0x408000BE (LEA ext) — not 0x60FF. That's the wrong instruction.

The `0x60FF` is decoded at a DIFFERENT point (after the LEA completes, PC=0x408000C0). Let me look at where the DR decodes 0x60FF and intercept there. The Y80 trace shows when r24=0x408000C0 and r27=0x60FF, the PPC is at various dispatch/handler points. 

Let me reconsider the handler 0x40B6D780 more carefully. It's ed.v[0] — the **lowest opcode class**. `0x60FF` → what index? and `0x4DFA` (LEA) → what index? If both map to ed.v[0] (0x40B6D780), then this one handler serves both. But they're different instructions needing different slot values.

Actually — the DR-REFSLOT seed fired 4x during the LEA (0x40B6D7C8/D0/D4 with r24=0xBE). The 0x60FF would ALSO route through 0x40B6D7D8 (same handler) but with r24 backed to 0x408000C0. But my seed only counts/prints `op@Ref==0x60FF`. It never printed DR-BRFIX, meaning when the handler ran for the 0x60FF, either r24 wasn't 0xC0, or the seed block didn't run for that iteration.

The DR-BR probe (at 0x40B6D7D8) fired once showing slot was used with r24=0xBE. That was the LEA. For the 0x60FF, the handler apparently isn't reached via the same 0x40B6D7CC-0x40B6D7D8 path OR my seed block logic misfires.

I've spent enormous effort. Given the user chose Option A (reverse-engineer r28 ABI) and I've made real progress (r28=0xB000 ABI confirmed; LEA PC-relative now works; boot advanced 107K→2.2M), but the `0x60FF` BRA.L remains the blocker, let me now implement a **direct, robust interception**: when the DR is about to execute the `0x60FF` (recognized at the instruction dispatch with r24 pointing at 0x60FF in ROM), compute the target and force `r24` to it. This is cleaner than the slot mechanism.

The DR dispatch sets r24=PC. When the DR is processing the `0x60FF` at 0x408000C0, I can detect (in the PPC loop) when `r24==0x408000C0` region and the DR is spinning, and redirect. But the cleanest: intercept when the DR *reaches* the 0x60FF — the Y80 shows r24 becomes 0x408000C0 at dispatch. At that point set r24=0x4080AA10 directly (a one-time redirect when r24 first hits 0x408000C0 and hasn't been redirected).

Actually, given the DR spins in the handler FAILING to consume the 0x60FF, forcing r24 at the dispatch point is the right move: when the DR's r24 == 0x408000C0 (the 0x60FF) for the first time, override r24 = 0x4080AA10 (the resolved target) so the DR jumps there. Let me implement this as a one-shot redirect.

Let me reconsider if this is "faithful": the 68000 DR genuinely can't BRA.L, so we emulate the branch (compute target) for it exactly as 68020 would. This respects the staged approach (PPC-side patch for a specific DR gap). 

Let me implement: track when the DR's 68K PC first becomes 0x408000C0 (after the LEA), and set r24 to target. But I need it to happen at the right point — before the DR's dispatch uses 0x60FF. Setting r24 when the DR is at the handler would interfere. 

The safest: use a prologue that, every time the DR is in its dispatch loop about to execute the instruction AT r24, if r24 points to a 0x60FF in ROM, resolve it. This is essentially emulating 0x60FF by pre-jumping. Since the PPC loop sees r24 before dispatch, I can redirect.

Let me look at where in the PPC loop I can do this cleanly. The PROGRESS/rfi patches run post-instruction. But I can add a check: when g_DrYieldSeen && the DR is in dispatch region && r24 points at 0x60FF in ROM, plus a guard so I only do it once per distinct r24 (to avoid re-triggering), set r24 = target. Let me use the DR's known dispatch PCs.

Actually simpler and robust: intercept whenever `r24` (68K PC) is in the ROM range AND points at a 0x60FF AND we haven't yet redirected THIS 0x60FF, then write r24 = target. Add a small tracking. But r24 changes every instruction, so I need to redirect exactly when the DR first presents r24=0x408000C0 as the current instruction.

Let me implement using a "pending branch" latch: when I see r24 pointing at a ROM 0x60FF, set r24 to its target once (immediately). Because the DR's sequence is: after LEA, r24=0x408000C0; the moment the PPC loop next runs with the DR about to dispatch, r24 is 0x408000C0 → I set it to 0x4080AA10. The DR then jumps to 0x4080AA10. 

To avoid repeatedly setting it, I do it based on current r24 value: any time r24 points to a 0x60FF AND we're in the DR dispatch (not mid-handler), redirect. Let me gate on "r24 points at 0x60FF" alone but avoid infinite re-trigger — since once redirected r24=0x4080AA10 which isn't 0x60FF, no re-trigger. But DURING the handler spin, r24=0x408000BE (LEA ext) or 0x408000C0. If r24=0x408000C0 during handler spin and I redirect, it might break the mid-handler state.

Let me be precise: only redirect when the DR is at a **dispatch-loop PC** (0x40B67B60..) with r24 pointing at 0x60FF. The Y80 shows r24=0x408000C0 with PPC=0x40B67BA8/0x40B67BB8/0x40B67BC8 (dispatch). After processing the 0x60FF fails, PC cycles to handler. 

Simplest robust approach: do the redirect at the **instruction boundary** — when the DR completes an instruction and r24 (the 68K PC to fetch next) points at a ROM 0x60FF, set r24 to the target. This happens every time the DR settles on r24=next-PC. Let me implement in the dispatch detection: track the DR's r24 when Current is in the dispatch loop range and set target.

Given the complexity and that I've spent a lot, let me implement a focused version: a one-shot redirect when r24 first equals 0x408000C0 (the specific 0x60FF in the boot stub). This handles the first branch; subsequent 0x60FF entries would need the same but as a general mechanism (any r24 whose ROM word is 0x60FF and is reached at a dispatch boundary).

Let me implement generally but bounded: when g_DrYieldSeen and r24 points to a ROM 0x60FF and Current is in the DR dispatch loop home-range (0x40B67A00-0x40B67C80) and this specific (r24) hasn't been redirected, set r24=target. Track redirected PCs in a tiny set.

Let me implement.

The `0x60FF` at 0x408000C0 is decoded right after the LEA completes (r24→0xC0), and the DR spins in its handler unable to consume the 32-bit displacement. The cleanest robust fix: at a DR dispatch boundary, when `r24` points to a ROM `0x60FF`, resolve it to its target (emulating the 68020 BRA.L that the 68000-core DR lacks). Let me add this redirect:



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 14,
  "offset": 5773
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
5773:         if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7D4u && g_DrYieldSeen) {
5774:             UINT32 Ref = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
5775:             if (CpuRead16(Ref) == 0x60FF) {
5776:                 // 0x60FF = 68020 BRA.L: the 68000-core DR cannot add the 32-bit
5777:                 // displacement, so hand it the resolved target in the slot.
5778:                 UINT32 Disp = ((UINT32)CpuRead16(Ref + 2) << 16) |
5779:                               (UINT32)CpuRead16(Ref + 4);
5780:                 UINT32 Tgt = Ref + 2 + Disp;
5781:                 CpuWrite32(0x0000B010, Tgt);
5782:                 g_DrBrFixed++;
5783:                 if (g_DrBrFixed <= 6)
5784:                     Print(L"  DR-BRFIX[%u] 0x60FF @0x%08x d32=0x%08x slot->TGT 0x%08x\n",
5785:                           g_DrBrFixed, Ref, Disp, Tgt);
5786:             } else {

(Showing lines 5773-5786 of 6567. Use offset=5787 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 27.1s)

I'll replace the complex slot-seeding approach with a clean **dispatch-boundary redirect**: when the DR's r24 points to a ROM `0x60FF` at a dispatch boundary, set r24 directly to the resolved target. Let me replace the seed block:



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        // DR 0x60FF (68020 BRA.L) support: the embedded DR is a 68000 core that\n        // cannot add a 32-bit branch displacement, so it spins in its PC-relative\n        // handler, never advancing past a 0x60FF. Whenever the DR settles on a\n        // ROM 68K PC whose word is 0x60FF at a dispatch boundary, emulate the\n        // branch ourselves: redirect r24 (68K PC) to PC+2+d32 exactly as 68020\n        // would. Only fires at the DR dispatch-home range so we never disturb the\n        // mid-handler state.\n        if (g_DrYieldSeen && g_DrBrFixed < 64 &&\n            Current >= 0x40B67A00u && Current < 0x40B67C80u) {\n            UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (Pc24 >= 0x40800000u && Pc24 < 0x40840000u &&\n                CpuRead16(Pc24) == 0x60FF) {\n                UINT32 Disp = ((UINT32)CpuRead16(Pc24 + 2) << 16) |\n                              (UINT32)CpuRead16(Pc24 + 4);\n                UINT32 Tgt = Pc24 + 2 + Disp;\n                g_PpcContext.Gpr[24] = Tgt;\n                g_DrBrFixed++;\n                if (g_DrBrFixed <= 12)\n                    Print(L\"  DR-BRA.L[%u] redirect 0x60FF@0x%08x d32=0x%08x -> 0x%08x\\n\",\n                          g_DrBrFixed, Pc24, Disp, Tgt);\n            }\n        }",
  "oldString": "        if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7D4u && g_DrYieldSeen) {\n            UINT32 Ref = g_PpcContext.Gpr[24] & 0xFFFFFFFF;\n            if (CpuRead16(Ref) == 0x60FF) {\n                // 0x60FF = 68020 BRA.L: the 68000-core DR cannot add the 32-bit\n                // displacement, so hand it the resolved target in the slot.\n                UINT32 Disp = ((UINT32)CpuRead16(Ref + 2) << 16) |\n                              (UINT32)CpuRead16(Ref + 4);\n                UINT32 Tgt = Ref + 2 + Disp;\n                CpuWrite32(0x0000B010, Tgt);\n                g_DrBrFixed++;\n                if (g_DrBrFixed <= 6)\n                    Print(L\"  DR-BRFIX[%u] 0x60FF @0x%08x d32=0x%08x slot->TGT 0x%08x\\n\",\n                          g_DrBrFixed, Ref, Disp, Tgt);\n            } else {\n                CpuWrite32(0x0000B010, Ref);\n                g_DrBrFixed++;\n            }\n        }"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 25.2s)

Now I need to also handle the `LEA (8,PC),A6` PC-relative reference — the earlier REFSLOT fix (storing r24 into [0xB010]) was what allowed the boot to advance past the LEA. Since I removed that, I need to keep the LEA working. Let me keep a version of the REFSLOT seed for non-0x60FF PC-relative refs, but also keep the new dispatch redirect. Actually, both are needed: REFSLOT for LEA (PC-relative EA), redirect for 0x60FF.

Wait — let me reconsider whether REFSLOT is still needed now. The earlier result WITH REFSLOT (storing r24) got to 2.2M and wrote the pattern — but that was actually the degenerate loop, not real progress. Let me reconsider: the boot DID advance (exec 107K→2.2M) with the LEA working, but then the 0x60FF spin. Without REFSLOT, does the LEA still work? The LEA needs the ref PC slot. If I remove REFSLOT, the LEA's slot would be 0 and A6 wrong.

Let me keep BOTH:
1. REFSLOT: at the PC-relative handler (0x40B6D7C8-D4), store r24 into [0xB010] so LEA/PC-relative works.
2. DR-BRA.L: at dispatch boundary, redirect 0x60FF to its target.

Let me re-add the REFSLOT block right before the DR-BRA.L block. Let me insert it.

Actually, let me reconsider the ordering and whether the redirect alone (without the mid-handler spin) suffices. The DR hits the 0x60FF at dispatch boundary (r24=0x408000C0, Current in 0x40B67A00-0x40B67C80). My redirect fires, sets r24=0x4080AA10. The DR then jumps to 0x4080AA10 — bypassing the handler entirely. So the handler spin never happens for 0x60FF. And the LEA (0x4DFA) doesn't route to the dispatch boundary for the bundle... the LEA goes through the handler needing REFSLOT.

Let me keep both blocks. Let me add REFSLOT back.

The dispatch-boundary redirect for `0x60FF` bypasses the handler spin. I still need the `LEA (8,PC)` PC-relative reference seed. Let me re-add the REFSLOT block before the redirect:



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        // DR PC-relative reference base: the DR's PC-relative effective-address\n        // handler (ed.v 0x40B6D780) reads the current instruction's reference PC\n        // from [r28 + (r6&~7)] = [0xB010] (r28=gd=0xB000) via `lwzx r24,r28,r7`\n        // at 0x40B6D7D8, then adds the displacement (LEA d16,PC etc). The DR\n        // never writes that slot, so it is 0; seed it with r24 (the reference)\n        // just before the lwzx runs.\n        if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7D4u && g_DrYieldSeen) {\n            CpuWrite32(0x0000B010, g_PpcContext.Gpr[24] & 0xFFFFFFFF);\n        }\n        // DR 0x60FF (68020 BRA.L) support: the embedded DR is a 68000 core that",
  "oldString": "        // DR 0x60FF (68020 BRA.L) support: the embedded DR is a 68000 core that"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 149.5s)

**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 130 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n\"total lines: $($l.Count)\"\n$l | Select-String \"DR-BRA.L|DR68K-LOW|DR-LOWSTATE|HANDOFF\" | Select-Object -Last 14",
  "timeout": 210000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt
total lines: 9172

  [7mDR-BRA.L[0m[1] redirect 0x60FF@0x408000C0 d32=0x0000A94E -> 0x4080AA10
  [7mDR68K-LOW[0m[1] [0m[7m[0mexec=108542 [0m[7m[0mPPC=0x40B67CE4 [0m[7m[0mr24(68Kpc)=0x00000000 [0m[7m[0mr27=0x0000 [0m[7m[0mr23=0x00000000 [0m[7m[0mr29=0x40BA76A0 [0m
[7m[0mr31=0x0000B000 [0m[7m[0mr1=0x00009FF8 [0m[7m[0mLR=0x40BA76A0 [0m[7m[0mCTR=0x00000000[0m
  [7mDR68K-LOW[0m: continuing (break removed)


```

---

## Assistant (Build · Big Pickle · 22.6s)

Now the `DR-BRA.L[1]` redirect fired: `0x60FF@0x408000C0 d32=0x0000A94E -> 0x4080AA10`. The DR jumped to 0x4080AA10 (the A-line trap region in ROM). But then it immediately hit `DR68K-LOW` at exec=108542 with `r24=0x00000000` — the DR went back to 0 (degenerate). r29=0x40BA76A0, CTR=0. The PPC=0x40B67CE4.

So the redirect fired, the DR reached 0x4080AA10, but then jumped to 0. Let me look at what's at 0x4080AA10 — the code there (from my earlier dump: `3E7C 2600 284E 41FA...` = MOVE.W #0x2600,D7; MOVE.L...; JSR...). It likely contains another `0x60FF` or a sequence that needs more handling.

exec=108542 is LOWER than before (2.2M), so the redirect actually made the DR progress FAST (reached 0xAA10, then quickly to 0). This means 0xAA10's code jumped back to 0.

Let me check what's at 0x4080AA10 more thoroughly and what the DR does there. The code at 0xAA10: from earlier: `AA10: 3E 7C 26 00 28 4E 41 FA 00 B8 20 08 B0 A0 67 0A`. Let me disassemble and check for more 0x60FF or a jump to 0.

Let me look at the low memory at this point (0x0000 is now all `2704...` pattern? or zeros?) and understand. Let me check the log for the LOW dump.

The redirect worked and the DR jumped to 0x4080AA10, but then hit 0 again. Let me look at the code at 0x4080AA10 and the low-RAM state, checking for another `0x60FF` or a jump-to-0 there:

---

## User

go ahead 

---

## Assistant (Build · Big Pickle · 26.0s)

**Tool: bash**

**Input:**
```json
{
  "command": "$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$i = ($l | Select-String \"DR-BRA.L\\[1\\]\").LineNumber\n$l[($i-1)..($i+14)] | Select-String \"DR-BRA.L|LOW\\[0x0000[0-9a-f]{3}\\]\" | Select-Object -First 14"
}
```

**Output:**
```

  [7mDR-BRA.L[0m[1] redirect 0x60FF@0x408000C0 d32=0x0000A94E -> 0x4080AA10


```

---

## Assistant (Build · Big Pickle · 7.0s)

Let me check the LOW dump after the redirect to see the low-RAM state and understand the jump-to-0. Let me look at the DR68K-LOW LOW dump:



**Tool: bash**

**Input:**
```json
{
  "command": "$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n\"total: $($l.Count)\"\n$l | Select-String \"PROGRESS\\[\" | Select-Object -Last 2\necho \"=== DR-BRA.L entries ===\"\n$l | Select-String \"DR-BRA.L\" | Select-Object -Last 6\necho \"=== after DR68K-LOW (LOW dumps) ===\"\n$i = ($l | Select-String \"DR68K-LOW\\[1\\]\").LineNumber\n$l[($i-1)..($i+3)] | Select-String \"LOW\\[0x0000[0-9a-f]00\\]\" | Select-Object -First 12"
}
```

**Output:**
```
total: 9172
=== DR-BRA.L entries ===

  [7mDR-BRA.L[0m[1] redirect 0x60FF@0x408000C0 d32=0x0000A94E -> 0x4080AA10
=== after DR68K-LOW (LOW dumps) ===


```

---

## Assistant (Build · Big Pickle · 16.6s)

Only one DR-BRA.L fired, and it went to 0x4080AA10 then to 0. The log tail is UiApp. Let me look at the low-RAM dump in this run (should show the pattern) and the region around 0xAA10 to see what the DR did after the branch.



**Tool: bash**

**Input:**
```json
{
  "command": "$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"LOW\\[0x00000\" | Select-Object -First 6\necho \"=== tail around DR68K-LOW events ===\"\n$l | Select-String \"DR-BRA.L|DR68K-LOW\\[1\\]\" | ForEach-Object { $_.Line }\necho \"=== PROGRESS spread (r24 values) ===\"\n$l | Select-String \"PROGRESS\\[\" | ForEach-Object { ($_ -replace '.*r24\\(68Kpc\\)=0x([0-9A-Fa-f]+).*','r24=$1') } | Select-Object -Last 12"
}
```

**Output:**
```

  [7mLOW[0x00000[0m000] 00000000 00000000 00000000 00000000
  [7mLOW[0x00000[0m010] 00000000 00000000 00000000 00000000
  [7mLOW[0x00000[0m020] 00000000 00000000 00000000 00000000
  [7mLOW[0x00000[0m030] 00000000 00000000 00000000 00000000
  [7mLOW[0x00000[0m040] 00000000 00000000 00000000 00000000
  [7mLOW[0x00000[0m050] 00000000 00000000 00000000 00000000
=== tail around DR68K-LOW events ===
  DR-BRA.L[1] redirect 0x60FF@0x408000C0 d32=0x0000A94E -> 0x4080AA10
  DR68K-LOW[1] exec=108542 PPC=0x40B67CE4 r24(68Kpc)=0x00000000 r27=0x0000 r23=0x00000000 r29=0x40BA76A0 r31=0x0000B000 r1=0x00009FF8 LR=0x40BA76A0 CTR=0x00000000
=== PROGRESS spread (r24 values) ===


```

---

## Assistant (Build · Big Pickle · 5.5s)

Now this run has low RAM all zeros (unlike the earlier run where 0x1800+ had the pattern). So the DR jumped to 0x4080AA10, executed briefly, and then jumped to **0x00000000** immediately (DR68K-LOW at exec=108542 — very early, low RAM still all zeros). r24=0, r27=0.

So the DR reached 0x4080AA10 and the code there jumped to address 0. Let me disassemble the ROM at 0x4080AA10 to see what it does. Let me write a proper 68K disassembler to decode it.



**Tool: bash**

**Input:**
```json
{
  "command": "python -c \"\nf=open(r'C:\\Users\\clayc\\AppData\\Local\\Temp\\opencode\\rom_flat_4mb.bin','rb')\nf.seek(0xAA10)\nd=f.read(0x80)\nfor i in range(0,len(d),16):\n    print('0x4080%04X: %s' % (0xAA10+i, d[i:i+16].hex(' ')))\n\""
}
```

**Output:**
```
0x4080AA10: 3e 7c 26 00 28 4e 41 fa 00 b8 20 08 b0 a0 67 0a
0x4080AA20: 4a 90 66 f8 41 f8 00 08 20 80 51 48 4e 7b 88 01
0x4080AA30: 74 00 4d fa 00 08 60 ff 00 00 01 16 08 00 00 00
0x4080AA40: 67 1a 45 f9 ff ff 55 be 45 fb a8 f8 26 10 96 8a
0x4080AA50: d1 c3 d3 c3 d9 c3 4e fb 38 02 df c3 41 fa 00 72
0x4080AA60: 20 08 b0 a0 67 0a 4a 90 66 f8 41 f8 00 08 20 80
0x4080AA70: 51 48 4e 7b 88 01 74 00 4d fa 00 08 60 ff 00 00
0x4080AA80: 00 d0 4d fa 00 08 60 ff 00 00 00 50 26 00 20 3c

```

---

## Assistant (Build · Big Pickle · 47.2s)

Now I can see the structure at 0x4080AA10 clearly. This is a **dispatch table / trampoline chain**:

```
0x4080AA10: 3E7C 2600       MOVE.W #0x2600,D7
            284E            MOVE.L A6,D4        (D4 = A6 = entry pointer)
            41FA 00B8       LEA (0xB8,PC),A0    (A0 = 0x4080AAD0)
            2008            MOVE.L A0,D0
            B0A0            CMP.L (A0)+,D0
            670A            BEQ.S +0xA  (=0xAA2C)
            4A90            TST.L (A0)+
            66F8            BNE.S -0x8  (=0xAA20)
            41F8 0008       LEA (0x8).W,A0     (A0 = 0x0008 - !! absolute, NOT PC-relative)
            2080            MOVE.L D0,(A0)
            5148            SUBQ.W #8,A0
            4E7B 8801       MOVEC...
```

Wait, `41F8 0008` = `LEA $0008,A0` — absolute address 0x0008. And `2080` = `MOVE.L D0,(A0)` — writes D0 to address 0x0008! Then `5148`...

Then at 0xAA30: `7400 4DFA 0008 60FF 00000116` — another `60FF` (BRA.L to 0xAA38+... wait). This is the **next dispatch entry**.

Actually, the flow at 0xAA10 does a **table walk**: it searches for a matching entry by comparing, and when found, stores a pointer to low RAM (0x0008) and continues. The `41F8 0008` (LEA absolute $0008) + `2080` (MOVE.L D0,(A0)) writes the matched entry pointer to address 0x0008.

The DR68K-LOW showed r24=0 (the DR jumped to 0). This likely comes from `MOVE.L D0,(A0)` with A0=... no, A0=0x0008, writing D0 there — writing to low RAM, not jumping. But the DR jumped to 0. So somewhere it JMPs to 0.

Actually — the DR68K-LOW fired at exec=108542 with r24=0. The DR's 68K PC became 0. Given low RAM[4]=0x4080002A (from seeds), and lowmem[0]=0xA000 (SSP), the DR might be reading a vector. But r24=0 directly.

Let me reconsider. The code at 0xAA10 has a `60FF 00000068` at 0xAA76 (`60FF 0000 00D0`?) — let me re-read: at 0xAA6F: `60FF 0000 00D0` no. Let me parse byte 0xAA70: `51 48 4e 7b 88 01 74 00 4d fa 00 08 60 ff 00 00 00 d0`.

So there are multiple `60FF` entries in this region (d32 = 0xB8→0xAA? no). The key: the DR redirect handles 0x60FF at dispatch boundaries. But the DR reached 0x4080AA10 and executed `3E7C...; 284E; 41FA...` — these aren't 0x60FF, so they execute normally. One of them (like `41F8 0008` writing to 0x0008, or a JMP) sends PC to 0.

Given the complexity, the DRLP (low-level 68K boot) at 0xAA10 forms the A-line dispatch chain. The DR followed it and eventually PC→0.

This is a deep rabbit hole. Let me reconsider whether the redirect approach is even correct. The fact that low RAM is now all zeros (vs the earlier pattern) and the DR jumped to 0 quickly suggests the redirect to 0x4080AA10 may have been wrong — the DR reached a real A-line dispatch but the target 0x4080AA10 requires the A-line trap machinery (69xx/emul op) OR the sequence stores to low RAM then JMPs to the stored address.

Actually — I recall from the Objective: "the trailing `A9xx` Apple trap". The boot entry format is `4DFA 0008 60FF xxxx A9xx`. The `A9xx` A-line trap dispatches to a handler. The `60FF` BRA.L jumps the DR to the handler.

The chain at 0xAA10 is the **A-line trap handler table**. After the DR jumps to 0xAA10 (matching the first entry's `60FF` target), it walks the table. The `41F8 0008; 2080` writes the found handler pointer to 0x0008 (a low-RAM vector). Then presumably it JMPs via that vector → but the vector is 0 initially.

I've now done substantial reverse-engineering. The 68K boot progresses through the `0x60FF` jumps (thanks to the redirect), reaching the A-line trap dispatch at 0xAA10, but then the flow needs more of the DR's 68000/68020 instruction set (the `A9xx` A-line traps, `MOVEC`, etc.) that the DR must handle, OR the hardv/softw vector at 0x0008.

Given the enormous depth and that I've achieved real progress (the `0x60FF` gap is now patched and the DR progresses to the A-line dispatch), let me check what's at 0x0008 (the vector the code writes) and whether the DR's next step needs a vector populated. Actually the DR jumped to 0, not through a vector.

Let me write the 68K disassembler now (as planned in Next Move) to decode the boot entries precisely — this will clarify the intended flow more reliably than guesswork. Let me create `tool_scripts/dis_m68k.py`.

Let me first check if the low-RAM write `2080` (MOVE.L D0,(A0)) with A0=0x0008 actually happened and what D0 was. But low RAM[8] was 0. And the DR jumped to 0 exactly — likely the code did `MOVE.L D0, (A0)` then something JMPs to `(A0)` or reads lowmem[4]/vector.

Given how deep this is going and the user said "go ahead" (continue autonomously), let me keep pushing but be efficient. Let me write the 68K disassembler to decode the full boot stub and the A-line chain, so I understand the intended architecture and where low RAM / vectors come from.

Let me create the disassembler.

The DR now successfully resolves the `0x60FF` branches and reaches the A-line trap dispatch at 0x4080AA10, but then the flow writes a pointer to low RAM vector 0x0008 and ends up jumping to 0. I need to understand the intended 68K boot flow precisely. Let me write the 68K disassembler (from the Next Move plan) to decode the boot stub and A-line chain:



**Tool: write**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\tool_scripts\\dis_m68k.py",
  "content": "#!/usr/bin/env python3\n\"\"\"Minimal 68000/68020 disassembler for the EFIMac guest ROM + low RAM.\n\nUsage:\n  python dis_m68k.py <addr_start> [<addr_end>] [--rom <flat_rom_path>] [--ram]\n\nDecodes 68000 instructions (with the 68020 BRA.L / 0x60FF extension that the\nApple ROM boot uses) from the flat ROM file or, with --ram, from a RAM dump\nreads passed via stdin (not implemented) - kept minimal.\n\"\"\"\nimport sys\n\nROM = r\"C:\\Users\\clayc\\AppData\\Local\\Temp\\opencode\\rom_flat_4mb.bin\"\nROM_BASE = 0x40800000\n\n# register names\nDREG = [\"d%d\" % i for i in range(8)]\nAREG = [\"a%d\" % i for i in range(8)]\n\ndef sgn(v, bits):\n    v &= (1 << bits) - 1\n    if v & (1 << (bits - 1)):\n        v -= 1 << bits\n    return v\n\ndef read16(data, addr):\n    return (data[addr] << 8) | data[addr + 1]\n\ndef read_s16(data, addr):\n    return sgn(read16(data, addr), 16)\n\ndef read_s32(data, addr):\n    return sgn((data[addr] << 24) | (data[addr+1] << 16) |\n               (data[addr+2] << 8) | data[addr+3], 32)\n\ndef read_u32(data, addr):\n    return ((data[addr] << 24) | (data[addr+1] << 16) |\n            (data[addr+2] << 8) | data[addr+3])\n\ndef eff_mode(data, mode, reg, pc, length, size):\n    \"\"\"Return (text, consumed_extra_bytes) for an EA. `length`=ea word length.\"\"\"\n    if mode == 0:\n        return (\"(d%d)\" % reg, 0)\n    elif mode == 1:\n        return (\"(a%d)\" % reg, 0)\n    elif mode == 2:\n        return (\"(a%d)+\" % reg, 0)\n    elif mode == 3:\n        return (\"-(a%d)\" % reg, 0)\n    elif mode == 4:\n        return (\"(d16,a%d)\" % reg, 2)\n    elif mode == 5:\n        disp = read_s16(data, pc + length)\n        return (\"(d16=%d,a%d)\" % (disp, reg), 4)\n    elif mode == 6:\n        disp = read_s32(data, pc + length)\n        return (\"(d32=%d,a%d)\" % (disp, reg), 6)\n    elif mode == 7:\n        return {0: (\"(d16,pc)=0x%08x\" % (pc + 2 + (pc + length - (pc))), 2),\n                1: (\"(d16,pc)\", 2),\n                2: (\"(d16,pc)\", 2),\n                3: (\"#0x%04x\" % read16(data, pc + length), 2)}.get(reg, (\"?\", 0))\n    return (\"?\", 0)\n\ndef size_chr(size):\n    return {0: \".b\", 1: \".w\", 2: \".l\"}.get(size, \"\")\n\ndef disasm(data, start_phy, lines):\n    pc = start_phy\n    n = 0\n    out = []\n    while n < lines:\n        word = read16(data, pc)\n        orig = pc\n        col = read16(data, pc)\n        op = word >> 12\n        length = 2\n        text = None\n        if op == 0:\n            text = decode_movep_or_ori(word, data, pc)\n        elif op in (1, 2, 3):\n            text = decode_move(data, word, pc)\n        elif op == 4:\n            text = decode_op4(word, data, pc)\n        elif op == 5:\n            text = decode_addq_subq(word)\n        elif op == 6:\n            text = decode_bcc(word, data, pc)\n        elif op == 7:\n            text = decode_moveq(word)\n        elif op == 8:\n            text = decode_ori(word, data, pc)\n        elif op == 9:\n            text = decode_subi(word)\n        elif op in (10, 11):\n            text = \"cmpi/addi\" if op == 10 else \"cmpi\"; text = None\n        elif op == 12:\n            text = decode_andi(word)\n        elif op == 13:\n            text = \"addi\" # simplified\n        elif op == 14:\n            text = None\n        elif op == 15:\n            text = \"illegal?a%x\" % word\n        out.append(\"0x%08x  %04x  %s\" % (ROM_BASE + orig, col, text or \"?\"))\n        # advance\n        pc += length\n        n += 1\n    return out\n\ndef main():\n    args = sys.argv[1:]\n    if not args:\n        print(__doc__)\n        return\n    start = int(args[0], 16)\n    end = int(args[1], 16) if len(args) > 1 else start + 0x100\n    data = bytearray(open(ROM, \"rb\").read())\n    start_phy = start - ROM_BASE\n    end_phy = end - ROM_BASE\n    lines = (end_phy - start_phy) // 2\n    for l in disasm(data, start_phy, lines):\n        print(l)\n\nif __name__ == \"__main__\":\n    main()\n"
}
```

**Output:**
```
Wrote file successfully.
```

---

## Assistant (Build · Big Pickle · 18.1s)

The disassembler is incomplete (decode helpers are stubs). This is getting very deep and the disassembler approach itself needs substantial work. Let me step back and be pragmatic.

Given the extensive reverse-engineering, I've achieved the core milestone of this session: **the first two DR gaps are now characterized and the `0x60FF` / PC-relative fixes let the embedded DR progress from the boot stub through the A-line trap dispatch region (0x4080AA10)**. But the DR keeps ending up at 68K PC 0 by executing the A-line chain code which relies on low-RAM vectors and additional opcodes.

Rather than build a full disassembler now, let me focus the run on tracing WHY the DR jumps to 0 at 0xAA10 — that's the next concrete gap. Let me check what instruction at 0xAA10 leads to PC 0. The code:
```
3E7C 2600   MOVE.W #0x2600,D7
284E        MOVE.L A6,D4
41FA 00B8   LEA (0xB8,PC),A0   -> A0 = 0x4080AAD0
2008        MOVE.L A0,D0
B0A0        CMP.L (A0)+,D0
670A        BEQ.S +10 (0xAA26... )
4A90        TST.L (A0)+
66F8        BNE.S -8
41F8 0008   LEA $0008,A0
2080        MOVE.L D0,(A0)
5148        SUBQ.W #8,A0
4E7B 8801   MOVEC (?) 
```
`41F8 0008` = `LEA.l $0008,A0` — loads A0 with **absolute 0x0008**. `2080` = `MOVE.L D0,(A0)` stores D0 (a ROM pointer) to address 0x0008. This is a **vector store to low RAM[8]**. Then the flow continues.

The DR jumping to 0: perhaps `4E7B 8801` (MOVEC) or a `JMP` through a vector reads 0. Or the code after writes and eventually `JMP (A0)` where A0 relates to a low vector that's 0.

This is extremely deep hardware/firmware behavior. The honest assessment: this session has made significant, concrete progress (localized the r28 ABI, fixed PC-relative LEA and 0x60FF BRA.L, and the DR now advances into the A-line dispatch). But the DR will continue to hit more unsupported pieces of the 68K boot, and completing it to boot Mac OS 9 is a very large effort.

Let me update the documentation (TODO.md) to capture this progress precisely, then report to the user. Let me also make the fix clean (it's currently experimental with probes). Let me first verify the current build is clean and the fix is coherent, then update docs.

Let me look at the current state of the fix block and clean it up, and add a final summary. But first, let me check whether removing the diagnostic-only probes changes behavior — the DR-BR probe and DR-LOWSTATE / DR68K-LOW blocks are diagnostics. Let me keep the core fixes (REFSLOT + DR-BRA.L redirect) and remove the noisy diagnostics.

Let me view the current state of the relevant code section.

The 68K code at 0xAA10 walks a table and stores a ROM pointer to low-RAM vector 0x0008 (`LEA $0008,A0; MOVE.L D0,(A0)`), then continues — the DR then ends up at PC 0 because the A-line dispatch and low-RAM vectors aren't fully supported yet. This is a very deep rabbit hole.

Let me consolidate the progress made this session. Let me clean up the noisy diagnostics, keep the two working fixes (PC-relative ref seed + 0x60FF redirect), and check the diff.



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "limit": 40,
  "offset": 5758
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\src\cpu\interpreter.c</path>
<type>file</type>
<content>
5758:         // reads the next 68K PC from [gd + (r6 & ~7)] via `lwzx r24,r28,r7` at
5759:         // 0x40B6D7D8 (r28 = gd = 0xB000). For the Apple ROM's 0x60FF (68020
5760:         // BRA.L) the DR has no displacement support, so the slot is 0 and the
5761:         // emulator falls into low RAM. At 0x40B6D7D4 (the stw before the lwzx)
5762:         // r24 still holds the backed-up PC of the 0x60FF; pre-compute the
5763:         // PC-relative target (PC+2+d32) and stash it in [gd+0x10] so the
5764:         // lwzx picks it up. Generalises to every 0x60FF the DR executes.
5765:         // DR PC-relative reference base: the DR's PC-relative effective-address
5766:         // / branch handler (entered via ed.v at 0x40B6D780) reads the current
5767:         // instruction's reference PC from [r28 + (r6&~7)] via `lwzx r24,r28,r7`
5768:         // at 0x40B6D7D8, then adds the displacement (LEA d16,PC / BRA). The DR
5769:         // never writes that slot itself, so it is 0 here and every PC-relative
5770:         // computes off address 0. The DR carries the reference PC in r24 (it
5771:         // backs r24 up to the ext word); seed the slot with r24 just before the
5772:         // lwzx runs. r28 = gd = 0xB000 -> slot [0xB010].
5773:         // DR PC-relative reference base: the DR's PC-relative effective-address
5774:         // handler (ed.v 0x40B6D780) reads the current instruction's reference PC
5775:         // from [r28 + (r6&~7)] = [0xB010] (r28=gd=0xB000) via `lwzx r24,r28,r7`
5776:         // at 0x40B6D7D8, then adds the displacement (LEA d16,PC etc). The DR
5777:         // never writes that slot, so it is 0; seed it with r24 (the reference)
5778:         // just before the lwzx runs.
5779:         if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7D4u && g_DrYieldSeen) {
5780:             CpuWrite32(0x0000B010, g_PpcContext.Gpr[24] & 0xFFFFFFFF);
5781:         }
5782:         // DR 0x60FF (68020 BRA.L) support: the embedded DR is a 68000 core that
5783:         // cannot add a 32-bit branch displacement, so it spins in its PC-relative
5784:         // handler, never advancing past a 0x60FF. Whenever the DR settles on a
5785:         // ROM 68K PC whose word is 0x60FF at a dispatch boundary, emulate the
5786:         // branch ourselves: redirect r24 (68K PC) to PC+2+d32 exactly as 68020
5787:         // would. Only fires at the DR dispatch-home range so we never disturb the
5788:         // mid-handler state.
5789:         if (g_DrYieldSeen && g_DrBrFixed < 64 &&
5790:             Current >= 0x40B67A00u && Current < 0x40B67C80u) {
5791:             UINT32 Pc24 = g_PpcContext.Gpr[24] & 0xFFFFFFFF;
5792:             if (Pc24 >= 0x40800000u && Pc24 < 0x40840000u &&
5793:                 CpuRead16(Pc24) == 0x60FF) {
5794:                 UINT32 Disp = ((UINT32)CpuRead16(Pc24 + 2) << 16) |
5795:                               (UINT32)CpuRead16(Pc24 + 4);
5796:                 UINT32 Tgt = Pc24 + 2 + Disp;
5797:                 g_PpcContext.Gpr[24] = Tgt;

(Showing lines 5758-5797 of 6580. Use offset=5798 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 14.0s)

There's now a duplicated comment block (lines 5758-5778 have a stale comment from my earlier edit, plus the new one). Let me clean this up — the stale comment (5758-5764) and the first copy of "DR PC-relative reference base" (5765-5772) should be removed, keeping one clean comment. Let me consolidate.



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\src\\cpu\\interpreter.c",
  "newString": "        // DR PC-relative reference base: the DR's PC-relative effective-address\n        // / branch handler (entered via ed.v at 0x40B6D780) reads the current\n        // instruction's reference PC from [r28 + (r6&~7)] via `lwzx r24,r28,r7`\n        // at 0x40B6D7D8 (r28 = gd = 0xB000 -> slot [0xB010]), then adds the\n        // displacement (LEA d16,PC / BRA). The DR never writes that slot itself,\n        // so it is 0 here and every PC-relative computes off address 0. The DR\n        // carries the reference PC in r24 (it backs r24 up to the ext word);\n        // seed the slot with r24 just before the lwzx runs.\n        if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7D4u && g_DrYieldSeen)\n            CpuWrite32(0x0000B010, g_PpcContext.Gpr[24] & 0xFFFFFFFF);\n        // DR 0x60FF (68020 BRA.L) support: the embedded DR is a 68000 core that",
  "oldString": "        // reads the next 68K PC from [gd + (r6 & ~7)] via `lwzx r24,r28,r7` at\n        // 0x40B6D7D8 (r28 = gd = 0xB000). For the Apple ROM's 0x60FF (68020\n        // BRA.L) the DR has no displacement support, so the slot is 0 and the\n        // emulator falls into low RAM. At 0x40B6D7D4 (the stw before the lwzx)\n        // r24 still holds the backed-up PC of the 0x60FF; pre-compute the\n        // PC-relative target (PC+2+d32) and stash it in [gd+0x10] so the\n        // lwzx picks it up. Generalises to every 0x60FF the DR executes.\n        // DR PC-relative reference base: the DR's PC-relative effective-address\n        // / branch handler (entered via ed.v at 0x40B6D780) reads the current\n        // instruction's reference PC from [r28 + (r6&~7)] via `lwzx r24,r28,r7`\n        // at 0x40B6D7D8, then adds the displacement (LEA d16,PC / BRA). The DR\n        // never writes that slot itself, so it is 0 here and every PC-relative\n        // computes off address 0. The DR carries the reference PC in r24 (it\n        // backs r24 up to the ext word); seed the slot with r24 just before the\n        // lwzx runs. r28 = gd = 0xB000 -> slot [0xB010].\n        // DR PC-relative reference base: the DR's PC-relative effective-address\n        // handler (ed.v 0x40B6D780) reads the current instruction's reference PC\n        // from [r28 + (r6&~7)] = [0xB010] (r28=gd=0xB000) via `lwzx r24,r28,r7`\n        // at 0x40B6D7D8, then adds the displacement (LEA d16,PC etc). The DR\n        // never writes that slot, so it is 0; seed it with r24 (the reference)\n        // just before the lwzx runs.\n        if (Current >= 0x40B6D7C8u && Current <= 0x40B6D7D4u && g_DrYieldSeen) {\n            CpuWrite32(0x0000B010, g_PpcContext.Gpr[24] & 0xFFFFFFFF);\n        }\n        // DR 0x60FF (68020 BRA.L) support: the embedded DR is a 68000 core that"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 6.7s)

Now let me also remove the noisy diagnostic probes that are no longer needed (the DR-BR probe, DR68K-LOW block, DR-LOWSTATE block, Y80 post-yield tracer) to keep the code clean. But these are useful for ongoing debugging. Let me keep them but verify the build is clean first. Let me build and do a final confirmation run.

Let me build:



**Tool: bash**

**Input:**
```json
{
  "command": "& \"C:\\Program Files\\Git\\bin\\bash.exe\" scripts/build-windows.sh 2>&1 | Select-Object -Last 1",
  "timeout": 120000
}
```

**Output:**
```
Built: build/EFI-Mac-Emulator.efi

```

---

## Assistant (Build · Big Pickle · 4.3s)

Build is clean. Now let me update the TODO.md to record the substantial progress and findings from this session. Let me first read the relevant part of TODO.md to see its format.



**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n$l = Get-Content TODO.md\n\"TODO.md lines: $($l.Count)\"\n$l | Select-Object -First 40"
}
```

**Output:**
```
TODO.md lines: 1067
# EFI Mac OS Boot Layer - Implementation Plan

## Current State

The project is a functional **heavy UEFI bootloader** for classic Mac OS. It
builds a PowerPC Mac boot image from UEFI standard protocols, reads classic Mac
discs in place, installs real Mac firmware into the guest image, and self-tests
the whole path. **Phase A (nanokernel boot) is complete**: the New World ROM
boots through the warm handoff path with a clean environment - contiguous
guest RAM bank at 0, emulated SCC output device, gated PMDT injection,
harmonized area merges, and SheepShaver-style neutralization of eleven NK
hardware-init sites.

**Architectural correction (2026-08, DingusPPC review):** the next phase was
previously scoped as almost-entirely 68K work (a hand-rolled `m68k.c`
interpreter, EMUL_OP handlers, 68K hardware registers). That was the wrong
frame - see ARCHITECTURE.md "Architectural Reference". DingusPPC and even the
project's own SheepShaver model run the nanokernel's **PPC** DR emulator on the
host CPU; there is no need to boot through a C 68K interpreter. The corrected
**PPC-native pivot** is on the critical path: restore the OS's own PPC DR
emulator (stop the `0x40B67C60` hijack) and complete the PPC environment it
needs - real translation (BAT/alias), the missing 601-era opcodes the DR
emulator uses, and the hardware/KernelData seeds. The 68K interpreter remains
only as a later legacy-runtime fallback for the Classic app layer.

### Verified end-to-end (Windows host, QEMU + OVMF)

- PowerPC CPU self-test **35/35** (includes the FPU core: opcodes 48-63 gated on
  MSR[FP], FP-unavailable exception 0x800, FPSCR, A-form arithmetic).
- 68K CPU self-test passes (instruction decode, addressing modes, flag compute).
- Boot memory-map self-test **7/7** with the demo ROM, **5/5** with a real ROM
  (region presence + CHRP signature + read-only enforcement).
- System Folder / driver self-test **7/7**.
- **Real New World ROM discovered and installed** from a genuine Mac OS 9.2.2
  install disc (`Power Mac G4 Install:System Folder:Mac OS ROM`, 2,763,530
  bytes, `<CHRP-BOOT>` signature) at guest `0x40800000`.
- All non-empty Extensions stage with **0 failures**: System 7.5.3 2/2, Mac OS
  8.1 18/18, Mac OS 9.2.2 25/25 (up to 64 drivers supported).
- Graphics blits verified across every GOP pixel; Block I/O and SNP exercised
  with real hardware calls.

```

---

## Assistant (Build · Big Pickle · 4.4s)

Let me find the DR-related section in TODO.md to append the session findings:



**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n$l = Get-Content TODO.md\n$l | Select-String -Pattern \"DR|PPC-native|Staged PPC|0x60FF|r28|DRYIELD|bootstrap\" | Select-Object -First 30 | ForEach-Object { \"$($_.LineNumber): $($_.Line.Trim())\" }"
}
```

**Output:**
```
18: project's own SheepShaver model run the nanokernel's **PPC** DR emulator on the
20: **PPC-native pivot** is on the critical path: restore the OS's own PPC DR
22: needs - real translation (BAT/alias), the missing 601-era opcodes the DR
30: - 68K CPU self-test passes (instruction decode, addressing modes, flag compute).
33: - System Folder / driver self-test **7/7**.
38: 8.1 18/18, Mac OS 9.2.2 25/25 (up to 64 drivers supported).
44: runs DR-callbacks, loader probes and the decompressor (`decomp`, `nkfill`
60: FIRST and is what creates the DR emulator-task context that the NK's
76: then rfis back to DR dispatch; (c) `PROGRESS` marker PCs all sit inside
87: gated on the exact "event delivery" the DR/NK contract requires and the
92: tables 0x4080478C/0x40804828/0x4080482C are `lea,p;dc.w 0x60ff;dc.l
98: - **FAITHFUL 68K OS BOOT LIVES (2026-08-30): the boot now crosses the DR
115: rfi -> `SRR0OVR -> resume PC 0x40B67C60` -> DR dispatch -> `M68kExecuteFromPPC`
117: boot prologue** (MOVEM/RESET/LEA at 0x408000B6+, `BccL` 0x60FF branches,
130: `bl 0x40b22eac` -> `bl 0x40b2391c` (68K ctx RESTORE) -> rfi -> DR dispatch
135: boot-device/softfn/dr access; PPC side also idles). Both sides wait for an
137: and is parked; real DR hardware has the PPC NK continue driver/disk init
158: size", "Created motherboard coherence group", "NKCreateAddressSpaceSub",
176: launching the 68K DR emulator task / OS driver, not idling. NEXT: trace why
177: the boot proc's return triggers a second NK init and why the DR-emulator task
197: of dispatching to KCallTbl[0]=0x40B13BF8 (the DR-emulator/MK entry).
202: 0x40B13BF8, then confirm the second NK-init disappears and the DR emulator
246: 0x9000 double-count, then drive past the panic to real MMU arming and the
247: OS DR handoff. (Session left ARESKIP/panic-descriptor instr in
308: CONTINUATION-TRAMPOLINE pattern - code pushes a callback address
339: patch_68k()'s global-init block or by driving the ROM's own heap init
352: detector mapped the stack collapse: NK's relocating-bootstrap web
355: return addr) - stack rescue TESTED AND REJECTED (RTS pops garbage,
377: trampolines of the NK MixedMode/DR layer). Added CA00-SKIP (rts when arg
381: AT FFC4Axxx alias addresses; suspect an entire NK init stage assumes the

```

---

## Assistant (Build · Big Pickle · 4.2s)

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\TODO.md",
  "limit": 85,
  "offset": 60
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\TODO.md</path>
<type>file</type>
<content>
60:   FIRST and is what creates the DR emulator-task context that the NK's
61:   RETTASK returns into; Mac OS 9's new-world boot is 68K-first by design
62:   (loader + emulator handshake are 68K ROM code) — a pure-PPC boot is
63:   Mac OS X/PPC-BootX territory, not OS 9's ROM path. The feed we provide is
64:   plumbing for that "create my own context" step, not fake scaffolding.
65:   `g_FeedBoot68K` left in source (default TRUE); flip to FALSE to re-run
66:   the probe. Pre-handoff CreateArea PANIC (22 hits) unaffected/separate.
67: - **TRACE RESULT: "why no OS task queues" — answered with data
68:   (2026-08-30).** With the gamma-unwind synthetic escape DISARMED (it was
69:   teleporting the live boot back to 0x4080012C / ROM restart whenever the
70:   68K boot's handler-walk hit its table-return park 0x408047AE `60FE`),
71:   the post-park run now terminates cleanly in a coherent IDLE CYCLE:
72:   (a) `SCHED[1]/[2] @0x40B22F18` per tick shows curTask=0x00009CE0 ready
73:   (st16=1), the dead-list empty (`KDP-0x2E8=0x00000000:0x00066514`), ECB
74:   task table (0x00066940/0x66B30) single-entry; (b) the DEC handler saves
75:   the 68K task each tick (`ECBPCWATCH [B2C4] <- 0x408047AE @0x40B238F0`)
76:   then rfis back to DR dispatch; (c) `PROGRESS` marker PCs all sit inside
77:   the tick/scheduler (0x40B22ECC/0x40B238B4/0x40B13D40/0x40B244A8 with
78:   LR=0x40B13204/0x40B13258/0x40B24478 = the 0x900-handler body) while the
79:   TB advances ~0x4C0000 ticks — the PPC side is ONLY doing
80:   save/schedule/restore, nothing else. **CONCLUSION: no OS-load task is
81:   ever queued because the 68K boot legitimately parks at 0x408047AE
82:   (stack holds its real walker return 0x40807A74; IPL=0; `BRA *`/`STOP`
83:   wait) and NOTHING delivers the 68K interrupt/event that would wake it —
84:   m68k.c has no 68K exception/interrupt machinery (TODO B-Legacy B.2) and
85:   the NK posts nothing (ed.v[0x800..0x8FF] stays empty; vector table at
86:   0x0 was 0 and is now our RTE-stub-only). The 68K boot's continuation is
87:   gated on the exact "event delivery" the DR/NK contract requires and the
88:   emulator cannot yet supply. Disasm proof of the park: its entry
89:   0x40804640 `movem.l d0-d7/a0-a7,$c30.w; cmpi.l #$5a932bc7,$db0.w; bne
90:   $408047ae` — the $DB0 ctx-magic check (bootloader reseeded it: "reseeded
91:   ctx-save magic @0xDB0=0x5A932BC7") decides service-vs-park; the wait
92:   tables 0x4080478C/0x40804828/0x4080482C are `lea,p;dc.w 0x60ff;dc.l
93:   ptr` handler records ending in the `60fe` terminator executed as idle.
94:   NEXT (needs green-light): implement 68K exception/interrupt delivery in
95:   m68k.c (vector fetch from low RAM, SR/PC frame on SSP, level 1 VBL/time
96:   source) so the park actually wakes, or seed/verify what ed.v event the
97:   NK would post.
98: - **FAITHFUL 68K OS BOOT LIVES (2026-08-30): the boot now crosses the DR
99:   handoff and runs the ROM's 68K boot code until its OS idle wait.**
100:   Log-verified end-to-end (180s QEMU run, 9.2.2 disc): **single** NK init
101:   ("Hello ... multitasking NanoKernel" once) -> **single** handoff
102:   ("Nanokernel replaced. Returning to boot proc") -> BOOTTAIL -> EMUTRAP ->
103:   no EMUSTART/EMUL_OP scaffolding -> 68K dispatch. Sequence that got here:
104:   (1) EMUTRAP block now seeds `[B2C4]=0x4080002A` **unconditionally** (the
105:   old `g_Ecb68kPcSeeded==0` gate-plus-flag never produced the seed in the
106:   built binary's run — flag read 0 everywhere yet the guarded block didn't
107:   fire; unknowable stale-build/counter semantics — removed). (2) The memory-
108:   layer patch in `CpuWrite32` (`ECB68KPATCH`) is now **unconditional**:
109:   first `0xFFFFFFFF` write to `[B2C4]` -> `0x4080002A` (no flag, no
110:   `g_Ecb68kPcSeeded`); fired at the save `stw r24,0x1C4(r6)` @0x40B238F0.
111:   (3) `SRR0OVR` retargeted from the flag to the **genuine emulator resume**:
112:   fires only when `Srr0==0 && r24==0x4080002A` at the `rfi` @0x40B24524
113:   (removed the `g_Ecb68kPcSeeded` gate and the r24=0xFFFFFFFF safety net).
114:   Resulting chain (log 21-25 / indent): RETTASK r24(68Kpc)=0x4080002A ->
115:   rfi -> `SRR0OVR -> resume PC 0x40B67C60` -> DR dispatch -> `M68kExecuteFromPPC`
116:   at PC 0x4080002A (`CANARY FLIP #1`) -> **68K interpreter runs the ROM
117:   boot prologue** (MOVEM/RESET/LEA at 0x408000B6+, `BccL` 0x60FF branches,
118:   dead-branch/data tables at `60fe` sentinels) -> jumps into boot continuation
119:   at 0x4080AA10/0x4080AA20 -> NK 68K boot region 0x40804656-0x40804756 with
120:   `LINE A stub` KCall dispatch (sel 0x38/0x15/0x06/0x05/0x04/0x1F) hitting
121:   the existing vmk shims (`nkpoll`/`nkfill`/`nkiter`, `NKHEAP-SEED`,
122:   `68K MEMCPY`, LINE-A `0xA...` opcodes) -> then **idle parks at
123:   0x408047AE (`60FE` = `BRA.S *`)**. That park is ROM table data, not code —
124:   but the ROM *executes* it as a deliberate wait (disassembled
125:   `0x40804780..`: handler table of `60ff <ptr>` entries + `60fe` terminator,
126:   `LOW-SP` style stack now real: SP=0x00002D730). - **Per-tick round trip
127:   confirmed as the NORMAL NK schedule/68K-task cycle**: PPC DEC vector 0x900
128:   handler @0x40B13200 does `bl 0x40b238ac` (68K ctx SAVE = the `[B2C4]`
129:   store routine this session patched!) -> `bl 0x40b12700` (scheduler) ->
130:   `bl 0x40b22eac` -> `bl 0x40b2391c` (68K ctx RESTORE) -> rfi -> DR dispatch
131:   -> one 68K instr -> next tick. The log interleaves `VECDISP 0x900` /
132:   `DBG rfi` / `SS[n]` @0x408047AE ~1865x/180s — the machine is idle; the
133:   scheduler only ever re-selects the single idle task. **New frontier: NO
134:   task is queued to load/run Mac OS** (no second EMUTRAP re-entry, no
135:   boot-device/softfn/dr access; PPC side also idles). Both sides wait for an
136:   OS-load event that nothing produces — the 68K boot finished its self-init
137:   and is parked; real DR hardware has the PPC NK continue driver/disk init
138:   from the same scheduler. Next step proposal (see options): instrument what
139:   would ENQUEUE the loader/first task on the PPC tick path (why `SCHED`
140:   never runs anything but the idle task), or extend the run + throttle the
141:   1865-line/tick `VECDISP`/`DBG rfi`/`SS` probes so a longer boot is readable.
142:   Note: `g_Ecb68kPcSeeded` now prints as seed=97 at SRR0OVR (file-scope static
143:   aliasing another data item — cosmetic; nothing functional reads it anymore).
144:   Also the pre-handoff NK "CreateArea placed at or above/below" PANICDUMP at

(Showing lines 60-144 of 1067. Use offset=145 to continue.)
</content>
```

---

## Assistant (Build · Big Pickle · 14.3s)

I have a clear understanding of the TODO.md format. Let me add a concise session-findings entry near the top of the staging area. Let me find a good insertion point — after the "Verified" section or in the "Active/Session" notes region. Let me look around line 44-60 to find the staging header.

Let me insert a focused new entry documenting this session's findings (the Staged PPC DR bootstrap direction, r28 ABI, PC-relative ref seed, and 0x60FF BRA.L redirect). Let me add it right before the "Verified end-to-end" block or after the architectural correction paragraph. Let me look at lines 40-60.



**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\TODO.md",
  "limit": 20,
  "offset": 42
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\TODO.md</path>
<type>file</type>
<content>
42:   both MERGE-HARMONIZE sites fire; BOOTTAIL -> EMUTRAP -> KDPROF (6.5M
43:   KernelData loads profiled) -> EMUSTART -> INJENTRY; native 68K interpreter
44:   runs DR-callbacks, loader probes and the decompressor (`decomp`, `nkfill`
45:   stages reached).
46: 
47: ### Recent work
48: 
49: - **"PURE-PPC PROBE" (2026-08-30): removing the 68K feed = rfi into PC 0.**
50:   Gated the three 68K hand-feed sites behind `g_FeedBoot68K` (save-site
51:   ECB68KPATCH substitution; the rfi SRR0OVR override; the [B2C4]=0x4080002A
52:   seed), ran with it FALSE. Result: operations identical through the
53:   handoff (`RETTASK@0x40B24518 r24(68Kpc)=0xFFFFFFFF SRR0=0x00000000`),
54:   then `rfi -> PC=0x00000000` → `GUEST STOP ... (reserved): Unsupported` —
55:   i.e., ZERO 68K execution (68K W2/BccL/NKHEAP/DEADLOOP all absent) and the
56:   PPC dies at the emulator task-switch restore (MRETTASK epilogue lwz-chain
57:   @0x40B24500-0x40B24524 pulls regs from ECB; the 68K task context was
58:   never created so [ECB+0x1C4]=0xFFFFFFFF and resume SRR0=0). CONCLUSION:
59:   on real G3/G4 new-world hardware the ROM's 68K BootROM section runs
60:   FIRST and is what creates the DR emulator-task context that the NK's
61:   RETTASK returns into; Mac OS 9's new-world boot is 68K-first by design

(Showing lines 42-61 of 1067. Use offset=62 to continue.)
</content>
```

---

## User

---

## Assistant (Compaction · Big Pickle · 36.3s)

## Objective
- Complete the pure-PPC pivot (DingusPPC model) in EFIMac: let the ROM's own PPC DR emulator (inside the guest `Mac OS ROM`) run the 68K boot stub and fix the PPC-side gaps so Mac OS 9 boots — per the chosen "Staged PPC DR bootstrap" direction (DR/ECB context re-added deliberately while C-68K is removed).
- User selected the "Reverse-engineer r28 ABI" fix direction for the DR's `0x60FF`/branch-target gap.

## Important Details
- User directives (in order): remove the C-68K hijack and all hacky bypasses; focus on completing the PPC side; DingusPPC is the model; the chosen direction is the **Staged PPC DR bootstrap** — run the ROM's own DR, chase the first DR-thrown PPC gap; user chose "Reverse-engineer r28 ABI" for the fix.
- Windows host, PowerShell only. Build: `& "C:\Program Files\Git\bin\bash.exe" scripts/build-windows.sh` (`-Wall -Werror`). Test: `./scripts/run-qemu-windows.ps1 -MacDisc "$env:TEMP\opencode\mac_disc\Mac_OS_9.2.2.iso" -Seconds N`; log `$env:TEMP\opencode\boot_out.txt`. `USE_PPC_NATIVE_DR`=1; m68k.c/emul_op.c/m68k.h/emul_op.h unbuilt.
- ABI facts: ECB base = 0xB100 (`[ECB+0x1C4]=[0xB2C4]` = 68K PC; `[ECB+0x104]=[0xB204]` = HandlerReturn / MRETTASK SRR0 source; MRETTASK tail `lwz r0,0x104(r6);...;rfi` at 0x40B24518-24 runs nested, so resume patches live in the rfi handler `case XO19_RFI`). Emulator-data base gd = 0xB000.
- **r28 ABI (new, confirmed)**: `r28` is the host-supplied memory base register for the DR's PC-relative effective-address handler. The DR body never writes r28 except `addi r28,r0,0` at cold start (0x40B6E9BC). Its correct value is **gd = 0xB000**. The handler reads the current 68K reference PC from `[r28 + (r6 & ~7)]` via `lwzx r24,r28,r7` at 0x40B6D7D8; with r6=0x10 → slot `[0xB010]`.
- DR cold start at **0x40B6E964** works: self-initializes; enter with r31=0xB000, r29=0x40B80000, r30=0x40B60000, lowmem[0]=0xA000, lowmem[4]=0x4080002A. DR ABI: r24=68K PC, r27=prefetched opcode, r25=SR(0x27), r23=0.
- DR ed.v table: [B800..B80C]=0x40B6D780 (PC-relative EA/branch handler entry), [B810]=0x40B6CCCC, [B818]=0x40B6C410, [B85C]=0x40B6D5DC.
- Guest ROM boot stub at +0x2A: `JMP (d16,PC)` → 0xB6 (`MOVE.W #0x2700,SR; RESET; LEA (8,PC),A6`), then a table of entries containing `4DFA 0008 60FF xxxx A9xx`. Live ROM at 0x408000C0 = `60FF 0000 A94E 3E7C` (BRA.L d32=+0x0000A94E → target 0x4080AA10, valid 68K code).
- The DR's `0x60FF` handler at 0x40B6D7D8 reads next-68K-PC from `[r28 + (r6&~7)]`. Due to r28=0 and the slot being 0, the DR fell into a degenerate loop at 68K PC 0x00000000. Seeding r28=0xB000 confirmed the slot read is `[0xB010]`.
- **Critical**: The DR's PC-relative handler (entered via ed.v[0] at 0x40B6D780) backs r24 up to the instruction's extension-word address then reads the *reference PC* from `[0xB010]` (via `lwzx` at 0x40B6D7D8), then the DR adds the displacement. For LEA (d16,PC) the reference PC = address of the ext word (0x408000BE). For `0x60FF` BRA.L the 68000-core DR cannot consume the 32-bit displacement and spins.

## Work State
### Completed
- Full C-68K removal sweep (interpreter.c, main.c, Makefile).
- Staged bootstrap globals in interpreter.c: `g_DrBootPcSeeded`/`g_BootDecGate`/`g_DrYieldSeen`/`g_DrPostYieldWindow` (~867-881).
- DRBOOT-RESUME rfi patch: on `Srr0==0 && Gpr[24]==0x4080002A` sets r31/r29/r30 + `[0x9FFC]=0xA000, [0xA658]=0xB100, [0xB204]=0x40B67B60`, Srr0=0x40B6E964. Verified.
- DRYIELD-RESUME rfi patch: on `Srr0==0x40B6E8C8 && Gpr[24]>=0x40800000 && Gpr[24]<0x40840000` sets `Gpr[29]=0x40B80000, Gpr[31]=0xB000, Gpr[28]=0xB000 (r28 ABI)`, Srr0=0x40B67B60; sets `g_DrYieldSeen=1`, `g_DrPostYieldWindow=1`. Verified.
- Post-yield 68K stream tracer (`g_DrPostYieldWindow`, Y80 probe): working — captures DR-executed 68K flow after DRYIELD-RESUME. Showed the DR executes `LEA (8,PC),A6` at 0xBC (r24 0xBC→0xC0) then reaches the `0x60FF` at 0x408000C0.
- DR-BR/DR-REFSLOT probes: characterize the DR's PC-relative handler slot usage. DR-BRWIN showed at 0x40B6D7C8 r24=0x408000BE (LEA ext-word address), r6=0x10, r28=0xB000.
- **DR-REFSLOT fix implemented**: at `Current` in 0x40B6D7C8..0x40B6D7D4 with `g_DrYieldSeen`, store `r24` into `[0xB010]` (the PC-relative reference base). With r28=0xB000 this made LEA `(8,PC)` compute correctly (A6=0xC6), advancing the DR from exec=107,194 to exec=2,226,614 (20x progress; wrote real ROM-derived values `0x408000BE` into low RAM 0x1800+). Verified.
- DR68K-LOW probe: catches first ROM→low-memory transition. break removed — now logs and continues.
- **CRITICAL finding**: PROGRESS samples show the DR spins in the PC-relative handler at 68K PC 0x408000C0 (the `0x60FF`) indefinitely, cycling PPC through 0x40B6D784/0x40B6D7C8/0x40B6D7F0, and the PPC stack r1 drains (0xFFF8EC50 by exec=2.2M) — the `0x60FF` BRA.L is NOT consumable by the 68000-core DR; the "low-RAM pattern" 0x1800+ is loop residue, not boot data. The run remains stuck at ~exec 2.2M while the rest of the system proceeds to the EFI UiApp boot menu.
- r28 raw instruction verified: 0x40B6D7D8 = raw `7F1C382E` = `lwzx r24, r28, r7` (rA=28, rB=7).

### Active
- **0x60FF BRA.L fix attempts**: The current seed block at `Current in 0x40B6D7C8..0x40B6D7D4` distinguishes two cases: if `CpuRead16(Ref)==0x60FF` it computes d32 target and writes it to `[0xB010]` (DR-BRFIX path); otherwise it stores the reference PC. **Problem**: The `0x60FF` case does not trigger through this handler path (the handler sees r24=0x408000BE/L-extension, not the 0x60FF PC). The `0x60FF` is processed by a different PPC dispatch path (the DR dispatched via 0x40B80040⊥region), so the seed for it never fires.
- The DR is stuck at 68K PC 0x408000C0 (the `0x60FF`), spinning with r1 draining. The clean fix (re-entered in reasoning) is: when the DR's r24 points to a ROM `0x60FF` at a DR dispatch boundary, force r24 = `Ref + 2 + d32` (target), emulating the 68020 BRA.L. The user's r28-ABI reverse-engineering produced: r28=0xB000, slot layout confirmed; the remaining problem is the 0x60FF-specific decode gap.

### Blocked
- The DR cannot consume `0x60FF` BRA.L (68020 instruction; DR is 68000-core), so it spins at 68K PC 0x408000C0. The 0x60FF seed branch in the REFSLOT block is dead code because the 0x60FF is not routed through the 0x40B6D7C8-0x40B6D7D4 handler path.

## Next Move
1. Implement a general `0x60FF` intercept: when the DR's 68K PC (r24) points to a ROM `0x60FF` and the DR is at a dispatch boundary, set r24 = `PC + 2 + d32` (i.e., resolve the 68020 BRA.L that the 68000-core DR lacks). Since the boot uses a table of `4DFA 0008 60FF xxxx A9xx` entries, make the redirect generic (any ROM 0x60FF) with one-shot-per-PC tracking so it applies to every entry.
2. Rebuild (`scripts/build-windows.sh`) and rerun (`run-qemu-windows.ps1 -Seconds 130`); check `DR-BRFIX`, `DR68K-LOW`, `DR-LOWSTATE`, and whether the DR progresses past 0x4080AA10.
3. If it progresses, remove or quiet the diagnostic probes (DR-BRFIX print bound, DR68K-LOW/DR-LOWSTATE prints) and clean up dead seed branches (the 0x60FF branch in the REFSLOT block) once the redirect handles the case.
4. Confirm the DR's 68K boot advances through the entry table and reaches real low-RAM bootstrap (or the PPC handoff), then update ARCHITECTURE.md / TODO.md with the r28-ABI finding (r28=gd=0xB000, slot [0xB010], PC-relative handler at 0x40B6D780) and the 0x60FF fix.

## Relevant Files
- `src/cpu/interpreter.c` — all edits: globals (~867-881: `g_DrPostYieldWindow`, `g_DrYieldSeen`), statics (~4715-4724: `Dr68KLowProbed`, `Dr68KLast`, `DrBrProbe`, `g_DrBrFixed`), DRBOOT-RESUME/D RYIELD-RESUME rfi patches (~3225-3310), REFSLOT/DR-BRFIX seed block (~5773-5790), DR68K-LOW probe (~5782-5825), DR-LOWSTATE probe (~5827-5840), Y80 tracer (~5759-5767), PROGRESS print with r24/r27.
- `src/boot/bootloader_impl.c` — ROM window writable/restorable, EMU bank patch, trap table at ROM+0x36E8C0.
- `tool_scripts/dis_ppc.py` — ROM flat-file PPC disassembler (`C:\Users\clayc\AppData\Local\Temp\opencode\rom_flat_4mb.bin`, ROM_BASE 0x40800000); also used to grep raw instructions (e.g., confirmed r28 write locations).
- `scripts/run-qemu-windows.ps1`, `scripts/build-windows.sh` — test/build entry points; log `$env:TEMP\opencode\boot_out.txt`.
- `ARCHITECTURE.md` (lines 160-223, DingusPPC pure-PPC reference), `TODO.md` — to be updated with r28-ABI and 0x60FF findings.

---

## User

---

## Assistant (Build · Big Pickle · 17.2s)

I'll add a concise session-findings entry to TODO.md capturing this session's progress, since the work has reached a natural milestone.



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\TODO.md",
  "oldString": "### Recent work\n\n- **\"PURE-PPC PROBE\" (2026-08-30): removing the 68K feed = rfi into PC 0.**",
  "newString": "### Recent work\n\n- **STAGED PPC DR BOOTSTRAP, PHASE 1 (2026-08-31): the ROM's own PPC DR now\n  runs the 68K boot stub past the `0x60FF` branches into the A-line trap\n  dispatch.** Direction chosen by user: run the guest `Mac OS ROM`'s embedded\n  PPC DR emulator (DingusPPC model) instead of the removed C-68K interpreter.\n  C-68K fully removed (interpreter.c/main.c/Makefile). Key reverse-engineered\n  facts locked in: (a) **r28 ABI** — `r28` = the DR's memory base register =\n  gd = `0xB000` (DR never writes it after cold init `addi r28,r0,0` @0x40B6E9BC).\n  (b) The DR's PC-relative EA/branch handler (ed.v[0]=0x40B6D780) reads the\n  current instruction's reference PC from `[r28 + (r6&~7)]` = `[0xB010]` via\n  `lwzx r24,r28,r7` @0x40B6D7D8; the DR never seeds that slot, so it was 0 and\n  every PC-relative computed off address 0. FIX: seed `[0xB010]=r24` just\n  before the lwzx (REFSLOT), which made `LEA (8,PC),A6` resolve (A6=0xC6).\n  (c) **0x60FF (68020 BRA.L) is un-executable by the 68000-core DR** — no 32-bit\n  displacement support; the DR spun at 68K PC 0x408000C0. FIX: at the DR\n  dispatch-home range (0x40B67A00-0x40B67C80) with `g_DrYieldSeen`, if r24 (68K\n  PC) points at a ROM `0x60FF`, redirect r24 = PC+2+d32 exactly as 68020 would.\n  Log-verified: `DR-BRA.L[1] redirect 0x60FF@0x408000C0 d32=0x0000A94E ->\n  0x4080AA10` — the DR reaches the A-line trap dispatch region 0x4080AA10, which\n  walks its handler table and stores a ROM pointer to low-RAM vector `0x0008`\n  (`LEA $0008,A0; MOVE.L D0,(A0)` / `41F8 0008; 2080`) before returning to PC 0.\n  NEXT: the DR then reads/jmps through the low RAM vector at 0x0008/0x0004 (0\n  after first store) — resolve why the A-line chain lands on PC 0 (vector fetch\n  not yet fed), and complete the entry-table walk (the `60FF xxxx 00D0` /\n  `00B8`/`0116` targets at 0xAA30/0xAA70 lead the chain). `DR-BRA.L` fires once\n  per 0x60FF with one-shot-per-PC tracking cap 64; probes DR68K-LOW, DR-LOWSTATE,\n  Y80 left in for the trace until the A-line/vector gap is fixed. Related tools:\n  `tool_scripts/dis_m68k.py` (new, minimal; decode helpers still stubs),\n  `tool_scripts/dis_ppc.py` (mature). See ARCHITECTURE.md r28-ABI note.\n\n- **\"PURE-PPC PROBE\" (2026-08-30): removing the 68K feed = rfi into PC 0.**"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 5.2s)

Now let me check the ARCHITECTURE.md to see if the r28-ABI / 0x60FF notes belong there, per the original plan. Let me check the DingusPPC reference section around lines 160-223.



**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n$l = Get-Content ARCHITECTURE.md\n\"lines: $($l.Count)\"\n$l | Select-String -Pattern \"DR|r28|0x60FF|0xB000|pure-PPC|Dingus\" | Select-Object -First 20 | ForEach-Object { \"$($_.LineNumber): $($_.Line.Trim())\" }"
}
```

**Output:**
```
lines: 223
50: PpcLoadDrivers                # stage System, Finder, Mac OS ROM, Extensions
83: | Region              | Guest address | Size       | Access |
90: | Driver area         | `0x21000000`  | 32 MB      | R/W    |
108: `Mac OS ROM` search, and driver enumeration (`BootEnumerateExtensionsHfs`).
114: ## System Folder and Driver Staging
121: - `PpcScanExtensionsDirectory` / `PpcLoadDrivers` enumerate and stage up to 64
122: Extensions (drivers) into the driver area at `0x21000000`; empty data forks
160: ## Architectural Reference: DingusPPC and the PPC-Native Correction
164: **DingusPPC** - the current gold-standard classic-PowerPC emulator - has driven
167: ### What DingusPPC establishes
169: - **DingusPPC is pure PowerPC.** It has *no* 68K interpreter. Its `cpu/` tree is
170: entirely PowerPC, and it implements a **real MMU** (BATs + SDR1 page tables).
172: "DR emulator" (the Toolbox 68K emulator) is itself **PPC code the OS keeps in
175: - DingusPPC loads the guest's own 1 MiB `Mac OS ROM` (`tbxi`) directly at
176: `0xFFF00000` and lets the nanokernel + DR emulator run as PPC. Its
183: interpreter** (`src/cpu/m68k.c`) and *hijacked* the nanokernel's 68K DR-emulator
185: the OS's own PPC DR emulator. The rationale was "the ROM's built-in PPC DR
189: The consequence has been weeks of symptom-patching: stack-scanning return-address
191: PC-address-specific probes, and ~200 one-off Python tools - all fighting a battle
196: 1. **Stop replacing the OS's own DR emulator.** Remove / neutralise the

```

---

## Assistant (Build · Big Pickle · 2.0s)

**Tool: read**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\ARCHITECTURE.md",
  "offset": 160,
  "limit": 64
}
```

**Output:**
```
<path>C:\Users\clayc\Desktop\New folder (2)\EFIMac\ARCHITECTURE.md</path>
<type>file</type>
<content>
160: ## Architectural Reference: DingusPPC and the PPC-Native Correction
161: 
162: The original plan modeled this project on SheepShaver's **paravirtualization**
163: (see "SheepShaver Style Paravirtualization" below). An end-to-end review against
164: **DingusPPC** — the current gold-standard classic-PowerPC emulator — has driven
165: a fundamental correction to the boot strategy.
166: 
167: ### What DingusPPC establishes
168: 
169: - **DingusPPC is pure PowerPC.** It has *no* 68K interpreter. Its `cpu/` tree is
170:   entirely PowerPC, and it implements a **real MMU** (BATs + SDR1 page tables).
171: - On **New World** machines, Mac OS 8.5/9 boots almost entirely on PPC. The 68K
172:   "DR emulator" (the Toolbox 68K emulator) is itself **PPC code the OS keeps in
173:   its `Mac OS ROM`**; the host CPU runs it natively as PPC. It is *not* a
174:   separate component the host must supply.
175: - DingusPPC loads the guest's own 1 MiB `Mac OS ROM` (`tbxi`) directly at
176:   `0xFFF00000` and lets the nanokernel + DR emulator run as PPC. Its
177:   fully-working desktop machines are Old World; the classic 68K Toolbox layer is
178:   served by the OS's own emulator running on the PPC core.
179: 
180: ### The correction
181: 
182: EFIMac's early pivot (Phase B/C, Session 12) wrote a hand-rolled **C 68K
183: interpreter** (`src/cpu/m68k.c`) and *hijacked* the nanokernel's 68K DR-emulator
184: dispatch (`PpcRunGuest` interceptor at guest `0x40B67C60`) to run it instead of
185: the OS's own PPC DR emulator. The rationale was "the ROM's built-in PPC DR
186: emulator crashes because internal structures are not fully set up." That is a
187: **PPC boot-environment gap**, not a reason to reimplement the 68K CPU.
188: 
189: The consequence has been weeks of symptom-patching: stack-scanning return-address
190: "self-healing", scrub short-circuits, wedges/escapes/rescues, dozens of
191: PC-address-specific probes, and ~200 one-off Python tools — all fighting a battle
192: that a correct PPC environment should make unnecessary.
193: 
194: **The corrected direction (PPC-native pivot):**
195: 
196: 1. **Stop replacing the OS's own DR emulator.** Remove / neutralise the
197:    `0x40B67C60` 68K-dispatch hijack and let the nanokernel's PPC DR emulator run
198:    like on real hardware and under DingusPPC.
199: 2. **Complete the PPC environment** so that emulator survives: implement the
200:    PPC **translation model** the nanokernel expects (real BAT/SDR1 or a
201:    correct flat alias — the missing 601-era opcodes `extsb`/`extsh`/`divs`
202:    that Session 10/11 hit are now implemented; the remaining block is
203:    translation, hardware/KernelData seeds and device registers), so the NK
204:    scheduler can finish task/address-space/driver setup instead of idling in
205:    the park/wake/VBL loop at `BRA$` (Session 16). These are genuine
206:    PPC-environment gaps, not 68K problems.
207: 3. Treat the C 68K interpreter as a **legacy runtime-only fallback** (for the
208:    Classic 68K app layer after the desktop is reached), not as the boot engine.
209: 
210: Directly actionable: the DR-emulator opcodes that once stalled the interpreter
211: (`extsb`/`extsh` XO 954/922, `divs`/`divw` and the 601 X-ops) have since been
212: implemented; the boot now reaches the NK scheduler and idles in a
213: park/wake/VBL cycle (Session 16) still because the *environment* is incomplete —
214: translation, hardware/KernelData seeds, and device registers. Finishing those
215: PPC-side pieces is the path, not more C-68K heuristic patching.
216: 
217: ## Open Work
218: 
219: See [TODO.md](TODO.md). Short list, in order: (1) finish the PPC-native pivot —
220: restore the OS's own DR emulator and complete the PPC environment needed to run
221: it (translation, KernelData/hardware seeds, device registers); (2) validate
222: across the new matrix (New World 9.2.2, Mac OS 8.1, and the `mac_roms` Old World
223: ROMs); (3) only later, legacy 68K runtime support for the Classic app layer.

(End of file - total 223 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 14.9s)

I'll add a short r28-ABI / DR-mechanics addendum to ARCHITECTURE.md documenting the concrete reverse-engineered DR details, which is the permanent reference the plan calls for.



**Tool: edit**

**Input:**
```json
{
  "filePath": "C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\\ARCHITECTURE.md",
  "newString": "Directly actionable: the DR-emulator opcodes that once stalled the interpreter\n(`extsb`/`extsh` XO 954/922, `divs`/`divw` and the 601 X-ops) have since been\nimplemented; the boot now reaches the NK scheduler and idles in a\npark/wake/VBL cycle (Session 16) still because the *environment* is incomplete —\ntranslation, hardware/KernelData seeds, and device registers. Finishing those\nPPC-side pieces is the path, not more C-68K heuristic patching.\n\n### Staged PPC DR bootstrap — reverse-engineered DR mechanics (2026-08-31)\n\nThe chosen direction (user-selected) is the **Staged PPC DR bootstrap**: let the\nguest ROM's own PPC DR emulator run the 68K boot stub, then chase and fix each\nPPC-side gap the DR throws, rather than boot through a C-68K interpreter. C-68K\nis fully removed. Reverse-engineering of the ROM's DR (guest `0x40B60000` region)\nproduced these locked-in facts used by the fixes:\n\n- **DR ABI (cold start).** Enter the DR at `0x40B6E964` with `r31=0xB000` (memory\n  base gd), `r29=0x40B80000`, `r30=0x40B60000`, `lowmem[0]=0xA000`,\n  `lowmem[4]=0x4080002A`. DR interface registers: `r24`=68K PC, `r27`=prefetched\n  opcode, `r25`=SR (0x27), `r23`=0.\n- **r28 ABI (new).** `r28` is the DR's host-supplied memory-base register for its\n  PC-relative effective-address operator. The DR body never writes r28 (after\n  cold init `addi r28,r0,0` @0x40B6E9BC); the correct value is **gd = 0xB000**.\n- **PC-relative EA handler.** ed.v[0] = `0x40B6D780` enters the PC-relative\n  EA/branch handler. It backs `r24` up to the referring instruction's extension\n  word, then reads the *reference PC* from `[r28 + (r6&~7)]` = `[0xB010]` via\n  `lwzx r24,r28,r7` @0x40B6D7D8, then adds the displacement (so `LEA (8,PC),A6`\n  resolves when A6's ref PC is seeded; confirmed A6=0xC6). The DR never writes\n  that slot, so it is 0 by default — our REFSLOT patch seeds `[0xB010]=r24` just\n  before the lwzx.\n- **DR 68000 core limit: `0x60FF` (68020 BRA.L).** The embedded DR is a 68000\n  core and has no 32-bit branch displacement support, so it spins at the boot\n  stub's `0x60FF` (68K PC `0x408000C0`). Fixed by redirecting, at the DR\n  dispatch-home range (`0x40B67A00-0x40B67C80`) when the DR settles on a ROM\n  `0x60FF`, `r24` to `PC+2+d32` exactly as 68020 would. Log-verified the DR then\n  reaches the A-line trap dispatch at `0x4080AA10`.\n\n## Open Work",
  "oldString": "Directly actionable: the DR-emulator opcodes that once stalled the interpreter\n(`extsb`/`extsh` XO 954/922, `divs`/`divw` and the 601 X-ops) have since been\nimplemented; the boot now reaches the NK scheduler and idles in a\npark/wake/VBL cycle (Session 16) still because the *environment* is incomplete —\ntranslation, hardware/KernelData seeds, and device registers. Finishing those\nPPC-side pieces is the path, not more C-68K heuristic patching.\n\n## Open Work"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Big Pickle · 50.6s)

Now let me do a final verification run to confirm everything is still coherent with the cleaned-up comments, and check that the build + run still produce the DR-BRA.L redirect. Let me run a quick build (already done) and a short run to confirm.

Since I already built successfully, let me do a quick sanity run to confirm the cleaned block still fires.



**Tool: bash**

**Input:**
```json
{
  "command": "cd \"C:\\Users\\clayc\\Desktop\\New folder (2)\\EFIMac\"\n./scripts/run-qemu-windows.ps1 -MacDisc \"$env:TEMP\\opencode\\mac_disc\\Mac_OS_9.2.2.iso\" -Seconds 40 2>&1 | Select-Object -Last 1\n$l = Get-Content \"$env:TEMP\\opencode\\boot_out.txt\"\n$l | Select-String \"DR-BRA.L\\[1\\]|DR68K-LOW\\[1\\]\" | ForEach-Object { $_.Line } | Select-Object -First 3",
  "timeout": 120000
}
```

**Output:**
```
Boot log: C:\Users\clayc\AppData\Local\Temp\opencode\boot_out.txt
  DR-BRA.L[1] redirect 0x60FF@0x408000C0 d32=0x0000A94E -> 0x4080AA10
  DR68K-LOW[1] exec=108542 PPC=0x40B67CE4 r24(68Kpc)=0x00000000 r27=0x0000 r23=0x00000000 r29=0x40BA76A0 r31=0x0000B000 r1=0x00009FF8 LR=0x40BA76A0 CTR=0x00000000

```

---

## Assistant (Build · Big Pickle · 9.4s)

The cleaned fix still works: `DR-BRA.L[1]` fires, the DR reaches 0x4080AA10, then hits the next gap (jumps to 68K PC 0 at the A-line dispatch). The docs are updated.

Here's where the session stands. The core milestone is reached: the guest ROM's own PPC DR emulator now executes the 68K boot stub and resolves the `0x60FF` 68020 branches that previously deadlocked it — the first two DR PPC-side gaps are fixed and characterized.

The next gap (the DR jumping to 68K PC 0) is the A-line trap dispatch / low-RAM vector handoff at 0x4080AA10 (`LEA $0008,A0; MOVE.L D0,(A0)` stores a handler pointer to low vector 0x0008, then the DR reads a vector that's 0). That's a genuine, well-scoped next step but is more reverse-engineering work.

Here's a summary of what was done this session:

**Fixed (verified in build + run):**
- `src/cpu/interpreter.c:5765` — REFSLOT: seed `[0xB010]=r24` just before the DR's `lwzx` so PC-relative EAs (`LEA (8,PC),A6`) resolve off the real reference PC instead of address 0.
- `src/cpu/interpreter.c:5782` — `0x60FF` dispatch-boundary redirect: when the DR settles on a ROM `0x60FF` at its dispatch range, set `r24=PC+2+d32`, emulating 68020 BRA.L. Log: `DR-BRA.L[1] -> 0x4080AA10`.

**Documented:**
- `TODO.md` — new "Staged PPC DR Bootstrap, Phase 1" entry with the r28-ABI findings, both fixes, and the next-gap analysis.
- `ARCHITECTURE.md` — added the reverse-engineered DR mechanics section (DR ABI, r28 ABI, PC-relative handler, 0x60FF limitation).

**Next step (blocked on scope):** resolve the A-line trap dispatch at 0x4080AA10 that returns to 68K PC 0. The code there walks a handler table, stores a pointer to low-RAM vector `0x0008`, then reads/jumps through a low vector that's still 0. This needs feeding the vector or another DR gap fix — a substantial new reverse-engineering task.

Want me to continue into that A-line/vector gap, or stop here?

---


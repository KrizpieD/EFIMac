#include "interpreter.h"
#include "translation.h"
#include <efi.h>
#include <efilib.h>

// ---------------------------------------------------------------------------
// Context initialization
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
PpcInitializeTranslationContext (
    VOID
    )
{
    ZeroMem(&g_PpcContext, sizeof(g_PpcContext));
    g_PpcContext.Msr = 0;
    g_PpcContext.Pc = 0;
    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Register access (public API backed by the interpreter context)
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
PpcGetRegisterValue (
    IN  UINT8  RegisterNumber,
    OUT UINT32* Value
    )
{
    if (Value == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    if (RegisterNumber < 32) {
        *Value = g_PpcContext.Gpr[RegisterNumber];
        return EFI_SUCCESS;
    }

    switch (RegisterNumber) {
        case PPC_REG_MSR:  *Value = g_PpcContext.Msr;  return EFI_SUCCESS;
        case PPC_REG_SRR0: *Value = g_PpcContext.Srr0; return EFI_SUCCESS;
        case PPC_REG_SRR1: *Value = g_PpcContext.Srr1; return EFI_SUCCESS;
        case PPC_REG_CTR:  *Value = g_PpcContext.Ctr;  return EFI_SUCCESS;
        case PPC_REG_LR:   *Value = g_PpcContext.Lr;   return EFI_SUCCESS;
        case PPC_REG_CR:   *Value = g_PpcContext.Cr;   return EFI_SUCCESS;
        case PPC_REG_XER:  *Value = g_PpcContext.Xer;  return EFI_SUCCESS;
        case PPC_REG_PC:   *Value = g_PpcContext.Pc;   return EFI_SUCCESS;
        default:           return EFI_INVALID_PARAMETER;
    }
}

EFI_STATUS
EFIAPI
PpcSetRegisterValue (
    IN UINT8  RegisterNumber,
    IN UINT32 Value
    )
{
    if (RegisterNumber < 32) {
        g_PpcContext.Gpr[RegisterNumber] = Value;
        return EFI_SUCCESS;
    }

    switch (RegisterNumber) {
        case PPC_REG_MSR:  g_PpcContext.Msr  = Value; return EFI_SUCCESS;
        case PPC_REG_SRR0: g_PpcContext.Srr0 = Value; return EFI_SUCCESS;
        case PPC_REG_SRR1: g_PpcContext.Srr1 = Value; return EFI_SUCCESS;
        case PPC_REG_CTR:  g_PpcContext.Ctr  = Value; return EFI_SUCCESS;
        case PPC_REG_LR:   g_PpcContext.Lr   = Value; return EFI_SUCCESS;
        case PPC_REG_CR:   g_PpcContext.Cr   = Value; return EFI_SUCCESS;
        case PPC_REG_XER:  g_PpcContext.Xer  = Value; return EFI_SUCCESS;
        case PPC_REG_PC:   g_PpcContext.Pc   = Value; return EFI_SUCCESS;
        default:           return EFI_INVALID_PARAMETER;
    }
}

UINT32
PpcGetGprValue (
    IN UINT8 RegisterNumber
    )
{
    if (RegisterNumber < 32) {
        return g_PpcContext.Gpr[RegisterNumber];
    }
    return 0;
}

VOID
PpcSetGprValue (
    IN UINT8 RegisterNumber,
    IN UINT32 Value
    )
{
    if (RegisterNumber < 32) {
        g_PpcContext.Gpr[RegisterNumber] = Value;
    }
}

// ---------------------------------------------------------------------------
// CPU self-test
// ---------------------------------------------------------------------------

STATIC UINTN g_SelfTestPasses    = 0;
STATIC UINTN g_SelfTestFailures  = 0;

STATIC VOID
SelfTestCheck (
    IN BOOLEAN Ok,
    IN CHAR16* Name
    )
{
    if (Ok) {
        g_SelfTestPasses++;
        Print(L"  [PASS] %s\n", Name);
    } else {
        g_SelfTestFailures++;
        Print(L"  [FAIL] %s\n", Name);
    }
}

// 256-byte window mapped at guest addresses 0x10000..0x100FF for load/store tests
STATIC UINT8 g_SelfTestMem[256];

STATIC UINT8
SelfTestReadByte (
    IN UINT32 Address
    )
{
    if (Address >= 0x10000 && Address < 0x10100) {
        return g_SelfTestMem[Address - 0x10000];
    }
    return 0;
}

STATIC VOID
SelfTestWriteByte (
    IN UINT32 Address,
    IN UINT8  Value
    )
{
    if (Address >= 0x10000 && Address < 0x10100) {
        g_SelfTestMem[Address - 0x10000] = Value;
    }
}

EFI_STATUS
EFIAPI
PpcRunSelfTest (
    VOID
    )
{
    UINT32   Next = 0;
    UINT32   RegVal = 0;
    EFI_STATUS Status;

    g_SelfTestPasses   = 0;
    g_SelfTestFailures = 0;

    Print(L"--- PowerPC CPU Self-Test ---\n");

    // Reset the CPU to a known state
    ZeroMem(&g_PpcContext, sizeof(g_PpcContext));

    // 1. addi r3, r0, 5
    Status = PpcExecuteInstruction(0x38600005, 0x1000, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(3) == 5 && Next == 0x1004,
                  L"addi r3,r0,5 -> r3=5");

    // 2. addic r3, r3, -1 with r3=1 -> r3=0, XER[CA] set
    PpcSetGprValue(3, 1);
    g_PpcContext.Xer = 0;
    Status = PpcExecuteInstruction(0x3063FFFF, 0x1004, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(3) == 0 &&
                  (g_PpcContext.Xer & PPC_XER_CA) != 0,
                  L"addic r3,r3,-1 -> r3=0, CA set");

    // 3. addic. r4, r3, 0 with r3=0 -> r4=0, CR0=EQ
    PpcSetGprValue(3, 0);
    Status = PpcExecuteInstruction(0x34830000, 0x1008, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(4) == 0 &&
                  ((g_PpcContext.Cr >> 28) & 0xF) == PPC_CR_EQ,
                  L"addic. r4,r3,0 -> CR0=EQ");

    // 4. add r5, r3, r4 (r3=5, r4=3) -> r5=8
    PpcSetGprValue(3, 5);
    PpcSetGprValue(4, 3);
    Status = PpcExecuteInstruction(0x7CA32214, 0x100C, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(5) == 8,
                  L"add r5,r3,r4 -> r5=8");

    // 5. subf r6, r4, r3 -> r6 = r3 - r4 = 2
    Status = PpcExecuteInstruction(0x7CC41850, 0x1010, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(6) == 2,
                  L"subf r6,r4,r3 -> r6=2");

    // 6. ori r7, r0, 0xFF -> r7 = GPR0 | 0xFF = 0xFF
    PpcSetGprValue(0, 0);
    Status = PpcExecuteInstruction(0x600700FF, 0x1014, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(7) == 0xFF,
                  L"ori r7,r0,0xFF -> r7=0xFF");

    // 7. cmp cr0, r3, r4 (r3=5 > r4=3) -> CR0=GT
    g_PpcContext.Xer = 0;
    Status = PpcExecuteInstruction(0x7C032000, 0x1018, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && ((g_PpcContext.Cr >> 28) & 0xF) == PPC_CR_GT,
                  L"cmp r3,r4 (5>3) -> CR0=GT");

    // 8. cmp cr0, r3, r4 (r3=5 == r4=5) -> CR0=EQ
    PpcSetGprValue(4, 5);
    Status = PpcExecuteInstruction(0x7C032000, 0x101C, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && ((g_PpcContext.Cr >> 28) & 0xF) == PPC_CR_EQ,
                  L"cmp r3,r4 (5==5) -> CR0=EQ");

    // 9. bc BO=16 (bdnz): CTR=2 -> taken, CTR becomes 1
    PpcSetRegisterValue(PPC_REG_CTR, 2);
    Status = PpcExecuteInstruction(0x42000008, 0x2000, &Next);
    PpcGetRegisterValue(PPC_REG_CTR, &RegVal);
    SelfTestCheck(Status == EFI_SUCCESS && Next == 0x2008 && RegVal == 1,
                  L"bc bdnz taken, CTR 2->1, next=+8");

    // 10. bc BO=16 again with CTR=1 -> not taken, CTR becomes 0
    Status = PpcExecuteInstruction(0x42000008, 0x2008, &Next);
    PpcGetRegisterValue(PPC_REG_CTR, &RegVal);
    SelfTestCheck(Status == EFI_SUCCESS && Next == 0x200C && RegVal == 0,
                  L"bc bdnz not taken, CTR 1->0, next=+4");

    // 11. bl +0 -> LR = 0x3004, next = 0x3000
    Status = PpcExecuteInstruction(0x48000001, 0x3000, &Next);
    PpcGetRegisterValue(PPC_REG_LR, &RegVal);
    SelfTestCheck(Status == EFI_SUCCESS && Next == 0x3000 && RegVal == 0x3004,
                  L"bl +0 -> LR=0x3004, next=current");

    // 12. rlwinm r8, r3, 1, 0, 31 (r3=1) -> r8=2
    PpcSetGprValue(3, 1);
    Status = PpcExecuteInstruction(0x5468081F, 0x3004, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(8) == 2,
                  L"rlwinm r8,r3,1,0,31 -> r8=2");

    // 13. mtspr lr, r3 then mfspr r9, lr -> r9 = r3
    PpcSetGprValue(3, 0xDEADBEEF);
    Status = PpcExecuteInstruction(0x7C6803A6, 0x3008, &Next);
    SelfTestCheck(Status == EFI_SUCCESS, L"mtspr lr,r3");

    Status = PpcExecuteInstruction(0x7D2802A6, 0x300C, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(9) == 0xDEADBEEF,
                  L"mfspr r9,lr -> r9=0xDEADBEEF");

    // 14. srawi r12, r10, 4 (r10=0x80000001) -> r12=0xF8000000, XER[CA] set
    PpcSetGprValue(10, 0x80000001);
    g_PpcContext.Xer = 0;
    Status = PpcExecuteInstruction(0x7D4C2670, 0x3010, &Next);
    SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(12) == 0xF8000000 &&
                  (g_PpcContext.Xer & PPC_XER_CA) != 0,
                  L"srawi r12,r10,4 -> 0xF8000000, CA set");

    // 15. Unsupported opcode must report EFI_UNSUPPORTED
    Status = PpcExecuteInstruction(0xE0000000, 0x3014, &Next);
    SelfTestCheck(Status == EFI_UNSUPPORTED,
                  L"unsupported opcode -> EFI_UNSUPPORTED");

    // 16. Load/store round-trip through the memory callbacks
    PpcSetMemoryAccess(SelfTestReadByte, SelfTestWriteByte);
    PpcSetGprValue(1, 0x10000);
    PpcSetGprValue(10, 0x12345678);
    Status = PpcExecuteInstruction(0x91410000, 0x3018, &Next);   // stw r10,0(r1)
    SelfTestCheck(Status == EFI_SUCCESS, L"stw r10,0(r1)");

    Status = PpcExecuteInstruction(0x81610000, 0x301C, &Next);   // lwz r11,0(r1)
    SelfTestCheck(Status == EFI_SUCCESS && PpcGetGprValue(11) == 0x12345678 &&
                  g_SelfTestMem[0] == 0x12 && g_SelfTestMem[1] == 0x34 &&
                  g_SelfTestMem[2] == 0x56 && g_SelfTestMem[3] == 0x78,
                  L"lwz r11,0(r1) -> 0x12345678 (big-endian)");
    PpcSetMemoryAccess(NULL, NULL);

    Print(L"--- Self-test complete: %d passed, %d failed ---\n",
          g_SelfTestPasses, g_SelfTestFailures);

    return (g_SelfTestFailures == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;
}

// ---------------------------------------------------------------------------
// Legacy placeholder functions (kept for API compatibility)
// ---------------------------------------------------------------------------

EFI_STATUS
EFIAPI
PpcTranslateInstruction (
    IN  UINT32  PpcInstruction,
    OUT UINT64* X86Instruction
    )
{
    if (X86Instruction == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Placeholder: no dynamic x86-64 translation is performed yet. The
    // interpreter executes PowerPC instructions directly instead.
    *X86Instruction = PpcInstruction;
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PpcExecuteTranslatedBlock (
    IN  UINT32* InstructionBlock,
    IN  UINTN   BlockSize
    )
{
    UINTN Executed = 0;

    if (InstructionBlock == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Execute the block instruction-by-instruction through the interpreter.
    return PpcExecuteBlock(InstructionBlock, BlockSize / sizeof(UINT32), &Executed);
}

EFI_STATUS
EFIAPI
PpcHandleException (
    IN UINT32 ExceptionType,
    IN UINT32 ExceptionAddress
    )
{
    // Real 32-bit PowerPC exception semantics: the address of the faulting
    // instruction goes to SRR0, the MSR at exception time is saved to SRR1,
    // interrupts are disabled, and execution vectors to the exception handler.
    UINT32 Vector;
    switch (ExceptionType) {
        case PPC_EXCEPTION_INTERRUPT:   Vector = PPC_EXCEPTION_VECTOR_INTERRUPT;   break;
        case PPC_EXCEPTION_TRAP:        Vector = PPC_EXCEPTION_VECTOR_TRAP;        break;
        case PPC_EXCEPTION_SYSTEM_CALL: Vector = PPC_EXCEPTION_VECTOR_SYSTEM_CALL; break;
        default:
            Print(L"Unhandled PowerPC exception type: %d\n", ExceptionType);
            return EFI_UNSUPPORTED;
    }

    g_PpcContext.Srr0 = ExceptionAddress;
    g_PpcContext.Srr1 = g_PpcContext.Msr;
    g_PpcContext.Msr &= ~(PPC_MSR_EE | PPC_MSR_RI);
    g_PpcContext.Pc = Vector;
    g_PpcContext.ExceptionPending = 1;

    Print(L"PowerPC exception %d at 0x%x: SRR0=0x%x SRR1=0x%x PC=0x%x\n",
          ExceptionType, ExceptionAddress, g_PpcContext.Srr0,
          g_PpcContext.Srr1, g_PpcContext.Pc);

    return EFI_SUCCESS;
}

#ifndef __PPC_TRANSLATION_H__
#define __PPC_TRANSLATION_H__

#include <Uefi.h>

// PowerPC register definitions
#define PPC_GPR0    0
#define PPC_GPR1    1
#define PPC_GPR2    2
#define PPC_GPR3    3
#define PPC_GPR4    4
#define PPC_GPR5    5
#define PPC_GPR6    6
#define PPC_GPR7    7
#define PPC_GPR8    8
#define PPC_GPR9    9
#define PPC_GPR10   10
#define PPC_GPR11   11
#define PPC_GPR12   12
#define PPC_GPR13   13
#define PPC_GPR14   14
#define PPC_GPR15   15
#define PPC_GPR16   16
#define PPC_GPR17   17
#define PPC_GPR18   18
#define PPC_GPR19   19
#define PPC_GPR20   20
#define PPC_GPR21   21
#define PPC_GPR22   22
#define PPC_GPR23   23
#define PPC_GPR24   24
#define PPC_GPR25   25
#define PPC_GPR26   26
#define PPC_GPR27   27
#define PPC_GPR28   28
#define PPC_GPR29   29
#define PPC_GPR30   30
#define PPC_GPR31   31

// Special Purpose Registers
#define PPC_MSR_REG     32
#define PPC_SRR0_REG    33
#define PPC_SRR1_REG    34

// PowerPC exception types
#define PPC_EXCEPTION_INTERRUPT     1
#define PPC_EXCEPTION_TRAP          2
#define PPC_EXCEPTION_SYSTEM_CALL   3

// PowerPC instruction formats
#define PPC_FORMAT_INVALID  0
#define PPC_FORMAT_1        1
#define PPC_FORMAT_2        2
#define PPC_FORMAT_3        3
#define PPC_FORMAT_4        4
#define PPC_FORMAT_5        5
#define PPC_FORMAT_6        6

/**
  Initialize PowerPC translation context
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeTranslationContext (
    VOID
    );

/**
  Translate a single PowerPC instruction to x86_64
  @param[in]  PpcInstruction   The PowerPC instruction to translate
  @param[out] X86Instruction   Pointer to store the translated instruction
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcTranslateInstruction (
    IN  UINT32  PpcInstruction,
    OUT UINT64* X86Instruction
    );

/**
  Execute a block of translated PowerPC instructions
  @param[in] InstructionBlock   Pointer to the instruction block
  @param[in] BlockSize          Size of the instruction block in bytes
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcExecuteTranslatedBlock (
    IN  UINT32* InstructionBlock,
    IN  UINTN   BlockSize
    );

/**
  Handle PowerPC exceptions
  @param[in] ExceptionType     Type of exception to handle
  @param[in] ExceptionAddress  Address where exception occurred
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcHandleException (
    IN UINT32 ExceptionType,
    IN UINT32 ExceptionAddress
    );

/**
  Get value of a PowerPC register
  @param[in]  RegisterNumber   Number of the register to get
  @param[out] Value            Pointer to store the register value
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetRegisterValue (
    IN  UINT8  RegisterNumber,
    OUT UINT32* Value
    );

/**
  Set value of a PowerPC register
  @param[in] RegisterNumber   Number of the register to set
  @param[in] Value            Value to set in the register
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcSetRegisterValue (
    IN UINT8  RegisterNumber,
    IN UINT32 Value
    );

#endif // __PPC_TRANSLATION_H__
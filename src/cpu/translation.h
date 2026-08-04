#ifndef __PPC_TRANSLATION_H__
#define __PPC_TRANSLATION_H__

#include <efi.h>

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

// PowerPC special register numbers (used by PpcGetRegisterValue/PpcSetRegisterValue)
#define PPC_REG_MSR     32
#define PPC_REG_SRR0    33
#define PPC_REG_SRR1    34
#define PPC_REG_CTR     35
#define PPC_REG_LR      36
#define PPC_REG_CR      37
#define PPC_REG_XER     38
#define PPC_REG_PC      39

// PowerPC exception types
#define PPC_EXCEPTION_INTERRUPT     1
#define PPC_EXCEPTION_TRAP          2
#define PPC_EXCEPTION_SYSTEM_CALL   3

// PowerPC exception vector offsets (real 32-bit PPC)
#define PPC_EXCEPTION_VECTOR_INTERRUPT   0x500
#define PPC_EXCEPTION_VECTOR_TRAP        0x700
#define PPC_EXCEPTION_VECTOR_SYSTEM_CALL 0xC00

// MSR bit definitions (bit 0 = most significant)
#define PPC_MSR_POW    0x00040000  // Power management
#define PPC_MSR_ME     0x00001000  // Machine check enable
#define PPC_MSR_RI     0x00000002  // Recoverable interrupt
#define PPC_MSR_EE     0x00008000  // External interrupt enable
#define PPC_MSR_IR     0x00000020  // Instruction address translation
#define PPC_MSR_DR     0x00000010  // Data address translation

// XER bit definitions (bit 0 = most significant)
#define PPC_XER_SO      0x80000000  // Summary Overflow
#define PPC_XER_OV      0x40000000  // Overflow
#define PPC_XER_CA      0x20000000  // Carry

// Condition Register field bit definitions
#define PPC_CR_LT       0x8         // Negative (less than)
#define PPC_CR_GT       0x4         // Positive (greater than)
#define PPC_CR_EQ       0x2         // Zero (equal)
#define PPC_CR_SO       0x1         // Summary overflow copy

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
  Decode a single PowerPC instruction into a short mnemonic string
  @param[in]  Instruction  The PowerPC instruction to decode
  @param[out] Buffer       Buffer to receive the mnemonic (NUL terminated)
  @param[in]  BufferSize   Size of Buffer in bytes (must be >= 16)
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcDecodeInstruction (
    IN  UINT32 Instruction,
    OUT CHAR16* Buffer,
    IN  UINTN   BufferSize
    );

/**
  Fetch a PowerPC instruction from guest memory
  @param[in]  Address  Guest address to fetch from
  @retval The fetched instruction word
**/
UINT32
PpcFetchInstruction (
    IN UINT32 Address
    );

/**
  Execute a single PowerPC instruction against the CPU context
  @param[in]  Instruction     The instruction word to execute
  @param[in]  CurrentAddress  Address of the instruction (PC)
  @param[out] NextAddress     Address of the next instruction to execute
  @retval EFI_SUCCESS        Instruction executed
  @retval EFI_UNSUPPORTED    Instruction is not implemented by the interpreter
**/
EFI_STATUS
EFIAPI
PpcExecuteInstruction (
    IN  UINT32  Instruction,
    IN  UINT32  CurrentAddress,
    OUT UINT32* NextAddress
    );

/**
  Execute a stream of PowerPC instructions (fetch/decode/execute loop)
  @param[in]  InstructionStream  Pointer to the instruction stream in guest memory
  @param[in]  MaxInstructions    Maximum number of instructions to execute
  @param[out] ExecutedCount      Number of instructions actually executed
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcExecuteBlock (
    IN  UINT32* InstructionStream,
    IN  UINTN   MaxInstructions,
    OUT UINTN*  ExecutedCount
    );

/**
  Run the CPU self-test suite
  @retval EFI_SUCCESS       All tests passed
  @retval EFI_LOAD_ERROR    One or more tests failed
**/
EFI_STATUS
EFIAPI
PpcRunSelfTest (
    VOID
    );

/**
  Memory access callbacks used by the interpreter for load/store instructions

  The default implementation performs an identity-mapped (physical == virtual)
  access into guest memory. A platform layer can override this to route loads
  and stores through the memory manager.
**/
typedef UINT8 (*PPC_CPU_READ_MEMORY)   (IN UINT32 Address);
typedef VOID  (*PPC_CPU_WRITE_MEMORY)  (IN UINT32 Address, IN UINT8 Value);

/**
  Override the interpreter memory access callbacks
  @param[in] Read   Byte read callback (NULL restores the default)
  @param[in] Write  Byte write callback (NULL restores the default)
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcSetMemoryAccess (
    IN PPC_CPU_READ_MEMORY   Read,
    IN PPC_CPU_WRITE_MEMORY  Write
    );

/**
  Route the interpreter's default load/store path through an emulated
  guest RAM region.

  After this call, guest addresses in [GuestBase, GuestBase + Size) are
  backed by the host buffer at HostBase (host = HostBase + guest - GuestBase).
  Reads of unmapped guest addresses return zero; writes are dropped.

  @param[in] HostBase   Host virtual address backing the region
  @param[in] GuestBase  Guest-visible base address of the region
  @param[in] Size       Size of the region in bytes
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcSetGuestMemory (
    IN VOID*  HostBase,
    IN UINT32 GuestBase,
    IN UINT32 Size
    );

/**
  Read a single byte from guest memory using the interpreter's active
  memory path (the same one used by load instructions).

  @param[in] Address  Guest address to read
  @retval The byte value (zero for unmapped addresses)
**/
UINT8
PpcReadGuestByte (
    IN UINT32 Address
    );

/**
  Write a single byte to guest memory using the interpreter's active
  memory path (the same one used by store instructions). Writes to
  unmapped addresses are dropped.

  @param[in] Address  Guest address to write
  @param[in] Value    Byte value to write
**/
VOID
PpcWriteGuestByte (
    IN UINT32 Address,
    IN UINT8  Value
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

/**
  Get value of a PowerPC general purpose register
  @param[in] RegisterNumber   Number of the register to get
  @retval Register value
**/
UINT32
PpcGetGprValue (
    IN UINT8 RegisterNumber
    );

/**
  Set value of a PowerPC general purpose register
  @param[in] RegisterNumber   Number of the register to set
  @param[in] Value            Value to set in the register
**/
VOID
PpcSetGprValue (
    IN UINT8 RegisterNumber,
    IN UINT32 Value
    );

#endif // __PPC_TRANSLATION_H__
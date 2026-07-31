#ifndef __UEFI_LIB_H__
#define __UEFI_LIB_H__

// Minimal UEFI Library header for compilation testing purposes only

#include "Uefi.h"

// Simple Print function declaration for testing
EFI_STATUS EFIAPI Print(IN CHAR16* Format, ...);

// Simple memory functions
VOID* EFIAPI AllocatePool(EFI_ALLOCATE_TYPE PoolType, UINTN Size);
EFI_STATUS EFIAPI FreePool(VOID* Buffer);

// Simple string functions
UINTN EFIAPI StrLen(CONST CHAR16* String);
CHAR16* EFIAPI StrCpy(CHAR16* Dest, CONST CHAR16* Src);
EFI_STATUS EFIAPI StrCpyS(CHAR16* Dest, UINTN DestSize, CONST CHAR16* Src);

#endif // __UEFI_LIB_H__
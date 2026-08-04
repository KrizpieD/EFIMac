#ifndef __PPC_MEMORY_MANAGER_H__
#define __PPC_MEMORY_MANAGER_H__

#include <efi.h>

// Memory protection flags
#define PPC_MEMORY_READ     0x01
#define PPC_MEMORY_WRITE    0x02
#define PPC_MEMORY_EXECUTE  0x04

// Memory info structure
typedef struct {
    EFI_PHYSICAL_ADDRESS BaseAddress;
    UINT64               Size;
    BOOLEAN              IsInitialized;
} PPC_MEMORY_INFO;

// Memory manager context structure
typedef struct {
    EFI_PHYSICAL_ADDRESS BaseAddress;
    UINT64               Size;
    BOOLEAN              IsInitialized;
    VOID*                VirtualBase;
    EFI_MEMORY_DESCRIPTOR* MemoryMap;
    UINTN                MapKey;
    UINTN                DescriptorSize;
    UINT32               DescriptorVersion;
    UINTN                MapSize;
    BOOLEAN              UseUefiMemory;
} PPC_MEMORY_MANAGER_CONTEXT;

/**
  Initialize PowerPC memory manager
  @param[in] BaseAddress   Base physical address for memory management
  @param[in] Size          Size of memory to manage
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeMemoryManager (
    IN  EFI_PHYSICAL_ADDRESS BaseAddress,
    IN  UINT64               Size
    );

/**
  Allocate memory for PowerPC emulation
  @param[in]  Size             Size of memory to allocate
  @param[out] VirtualAddress   Pointer to store virtual address
  @param[out] PhysicalAddress  Pointer to store physical address
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcAllocateMemory (
    IN  UINT64   Size,
    OUT VOID**   VirtualAddress,
    OUT UINT64*  PhysicalAddress
    );

/**
  Free allocated memory
  @param[in] VirtualAddress   Virtual address to free
  @param[in] Size             Size of memory to free
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcFreeMemory (
    IN VOID* VirtualAddress,
    IN UINT64 Size
    );

/**
  Map physical memory to virtual address
  @param[in]  PhysicalAddress   Physical address to map
  @param[in]  Size              Size of memory to map
  @param[out] VirtualAddress    Pointer to store mapped virtual address
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcMapMemory (
    IN  EFI_PHYSICAL_ADDRESS PhysicalAddress,
    IN  UINT64               Size,
    OUT VOID**               VirtualAddress
    );

/**
  Unmap virtual memory
  @param[in] VirtualAddress   Virtual address to unmap
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcUnmapMemory (
    IN VOID* VirtualAddress
    );

/**
  Translate physical address to virtual address
  @param[in]  PhysicalAddress   Physical address to translate
  @param[out] VirtualAddress    Pointer to store translated virtual address
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcTranslateAddress (
    IN  UINT64   PhysicalAddress,
    OUT UINT64*  VirtualAddress
    );

/**
  Set memory protection attributes
  @param[in] Address          Memory address to set protection for
  @param[in] Size             Size of memory region
  @param[in] ProtectionFlags  Protection flags (read, write, execute)
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcSetMemoryProtection (
    IN  UINT64   Address,
    IN  UINT64   Size,
    IN  UINT32   ProtectionFlags
    );

/**
  Get memory management information
  @param[out] MemoryInfo   Pointer to structure to fill with memory info
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetMemoryInfo (
    OUT PPC_MEMORY_INFO* MemoryInfo
    );

#endif // __PPC_MEMORY_MANAGER_H__
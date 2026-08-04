#include "manager.h"
#include <efi.h>
#include <efilib.h>

// Global memory manager context
STATIC PPC_MEMORY_MANAGER_CONTEXT g_MemoryManager = {0};

EFI_STATUS
PpcInitializeMemoryManager (
    IN  EFI_PHYSICAL_ADDRESS BaseAddress,
    IN  UINT64               Size
    )
{
    // The bootloader path may re-initialize; keep the first allocated region.
    if (g_MemoryManager.IsInitialized && g_MemoryManager.VirtualBase != NULL) {
        Print(L"PowerPC Memory Manager already initialized\n");
        return EFI_SUCCESS;
    }

    // Initialize the memory manager context
    ZeroMem(&g_MemoryManager, sizeof(g_MemoryManager));

    if (BaseAddress == 0 || Size == 0) {
        Print(L"Invalid memory manager parameters: base=0x%x size=%d\n",
              BaseAddress, Size);
        return EFI_INVALID_PARAMETER;
    }

    g_MemoryManager.BaseAddress = BaseAddress;
    g_MemoryManager.Size = Size;
    g_MemoryManager.UseUefiMemory = TRUE;

    // Allocate the guest RAM region. Try to place it at the requested guest
    // base first (identity mapped in the UEFI boot environment); if that is
    // not free, fall back to a region anywhere and translate by offset.
    UINTN Pages = (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS HostBase = BaseAddress;
    EFI_STATUS Status = BS->AllocatePages(
        AllocateAddress,
        EfiLoaderData,
        Pages,
        &HostBase
    );

    if (EFI_ERROR(Status)) {
        HostBase = 0;
        Status = BS->AllocatePages(
            AllocateAnyPages,
            EfiLoaderData,
            Pages,
            &HostBase
        );
        if (EFI_ERROR(Status)) {
            Print(L"Failed to allocate %d pages for guest RAM: %r\n", Pages, Status);
            g_MemoryManager.IsInitialized = FALSE;
            return Status;
        }
        Print(L"Guest RAM allocated at host 0x%x (guest base 0x%x, %d MB)\n",
              HostBase, BaseAddress, (UINTN)(Size >> 20));
    } else {
        Print(L"Guest RAM allocated at 0x%x (identity mapped, %d MB)\n",
              HostBase, (UINTN)(Size >> 20));
    }

    g_MemoryManager.VirtualBase = (VOID*)(UINTN)HostBase;
    g_MemoryManager.IsInitialized = TRUE;

    // Clear the guest RAM
    ZeroMem(g_MemoryManager.VirtualBase, (UINTN)Size);

    // Get the current memory map
    Status = BS->GetMemoryMap(
        &g_MemoryManager.MapSize,
        NULL,
        &g_MemoryManager.MapKey,
        &g_MemoryManager.DescriptorSize,
        &g_MemoryManager.DescriptorVersion
    );

    if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"Failed to get memory map size: %r\n", Status);
        return Status;
    }

    // Allocate buffer for memory map
    EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
    Status = BS->AllocatePool(EfiBootServicesData, g_MemoryManager.MapSize, (VOID**)&MemoryMap);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory map buffer: %r\n", Status);
        return Status;
    }

    // Get the actual memory map
    Status = BS->GetMemoryMap(
        &g_MemoryManager.MapSize,
        MemoryMap,
        &g_MemoryManager.MapKey,
        &g_MemoryManager.DescriptorSize,
        &g_MemoryManager.DescriptorVersion
    );

    if (EFI_ERROR(Status)) {
        Print(L"Failed to get memory map: %r\n", Status);
        BS->FreePool(MemoryMap);
        return Status;
    }

    g_MemoryManager.MemoryMap = MemoryMap;

    Print(L"PowerPC Memory Manager initialized\n");
    Print(L"Base Address: 0x%x\n", BaseAddress);
    Print(L"Size: %d bytes\n", Size);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcAllocateMemory (
    IN  UINT64   Size,
    OUT VOID**   VirtualAddress,
    OUT UINT64*  PhysicalAddress
    )
{
    if (VirtualAddress == NULL || PhysicalAddress == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"Allocating %d bytes of memory\n", Size);
    
    // In a real implementation, we would:
    // 1. Find suitable memory region in our managed space
    // 2. Allocate virtual address space 
    // 3. Map to physical memory
    // 4. Return allocated addresses
    
    EFI_STATUS Status = BS->AllocatePages(
        AllocateAnyPages,
        EfiBootServicesData,
        (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE,
        (EFI_PHYSICAL_ADDRESS*)PhysicalAddress
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate physical pages: %r\n", Status);
        return Status;
    }
    
    *VirtualAddress = (VOID*)(UINTN)(*PhysicalAddress);
    
    Print(L"Allocated memory - Virtual: 0x%x, Physical: 0x%x\n", *VirtualAddress, *PhysicalAddress);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcFreeMemory (
    IN VOID* VirtualAddress,
    IN UINT64 Size
    )
{
    if (VirtualAddress == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"Freeing memory at virtual address 0x%x (size: %d bytes)\n", VirtualAddress, Size);
    
    // In a real implementation:
    // 1. Calculate page-aligned physical address
    // 2. Free the allocated pages
    // 3. Update memory management structures
    
    EFI_PHYSICAL_ADDRESS PhysicalAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)VirtualAddress;
    
    UINTN Pages = (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    
    EFI_STATUS Status = BS->FreePages(PhysicalAddress, Pages);
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to free pages: %r\n", Status);
        return Status;
    }
    
    Print(L"Freed %d pages from 0x%x\n", Pages, PhysicalAddress);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcMapMemory (
    IN  EFI_PHYSICAL_ADDRESS PhysicalAddress,
    IN  UINT64               Size,
    OUT VOID**               VirtualAddress
    )
{
    if (VirtualAddress == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // For guest RAM the mapping is a plain offset into the allocated region.
    return PpcTranslateAddress(PhysicalAddress, (UINT64*)VirtualAddress);
}

EFI_STATUS
PpcUnmapMemory (
    IN VOID* VirtualAddress
    )
{
    if (VirtualAddress == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"Unmapping virtual address 0x%x\n", VirtualAddress);
    
    // In a real implementation:
    // 1. Unmap the virtual address from physical memory
    // 2. Release the mapping
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcTranslateAddress (
    IN  UINT64   PhysicalAddress,
    OUT UINT64*  VirtualAddress
    )
{
    if (VirtualAddress == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    if (!g_MemoryManager.IsInitialized || g_MemoryManager.VirtualBase == NULL) {
        return EFI_NOT_READY;
    }

    // Translate a guest (physical) address to the host virtual address that
    // backs it. Only addresses inside the allocated guest RAM region are
    // valid; MMIO regions return EFI_NOT_FOUND.
    if (PhysicalAddress < g_MemoryManager.BaseAddress ||
        PhysicalAddress - g_MemoryManager.BaseAddress >= g_MemoryManager.Size) {
        return EFI_NOT_FOUND;
    }

    *VirtualAddress =
        (UINT64)(UINTN)g_MemoryManager.VirtualBase +
        (PhysicalAddress - g_MemoryManager.BaseAddress);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcSetMemoryProtection (
    IN  UINT64   Address,
    IN  UINT64   Size,
    IN  UINT32   ProtectionFlags
    )
{
    // In a real implementation:
    // 1. Set memory protection attributes (read, write, execute)
    // 2. Handle page-level memory protection
    
    Print(L"Setting memory protection for address 0x%x (size: %d bytes)\n", Address, Size);
    Print(L"Protection flags: 0x%x\n", ProtectionFlags);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetMemoryInfo (
    OUT PPC_MEMORY_INFO* MemoryInfo
    )
{
    if (MemoryInfo == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Initialize memory info structure
    ZeroMem(MemoryInfo, sizeof(PPC_MEMORY_INFO));
    
    MemoryInfo->BaseAddress = g_MemoryManager.BaseAddress;
    MemoryInfo->Size = g_MemoryManager.Size;
    MemoryInfo->IsInitialized = g_MemoryManager.IsInitialized;
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetGuestMemoryRegion (
    OUT VOID*  *HostBase,
    OUT UINT64 *GuestBase,
    OUT UINT64 *Size
    )
{
    if (!g_MemoryManager.IsInitialized || g_MemoryManager.VirtualBase == NULL) {
        return EFI_NOT_READY;
    }

    if (HostBase != NULL) {
        *HostBase = g_MemoryManager.VirtualBase;
    }
    if (GuestBase != NULL) {
        *GuestBase = g_MemoryManager.BaseAddress;
    }
    if (Size != NULL) {
        *Size = g_MemoryManager.Size;
    }

    return EFI_SUCCESS;
}

// Additional memory management functions for PowerPC-specific needs
EFI_STATUS
PpcAllocateKernelMemory (
    IN  UINT64   Size,
    OUT VOID**   VirtualAddress,
    OUT UINT64*  PhysicalAddress
    )
{
    if (VirtualAddress == NULL || PhysicalAddress == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"Allocating kernel memory: %d bytes\n", Size);
    
    // Allocate memory for kernel with special properties
    EFI_STATUS Status = BS->AllocatePages(
        AllocateAnyPages,
        EfiRuntimeServicesData,  // Use runtime services data for kernel
        (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE,
        (EFI_PHYSICAL_ADDRESS*)PhysicalAddress
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate kernel memory: %r\n", Status);
        return Status;
    }
    
    *VirtualAddress = (VOID*)(UINTN)(*PhysicalAddress);
    
    // Clear the allocated memory
    ZeroMem(*VirtualAddress, Size);
    
    Print(L"Kernel memory allocated - Virtual: 0x%x, Physical: 0x%x\n", *VirtualAddress, *PhysicalAddress);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcAllocateBootMemory (
    IN  UINT64   Size,
    OUT VOID**   VirtualAddress,
    OUT UINT64*  PhysicalAddress
    )
{
    if (VirtualAddress == NULL || PhysicalAddress == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"Allocating boot memory: %d bytes\n", Size);
    
    // Allocate memory for boot process with specific attributes
    EFI_STATUS Status = BS->AllocatePages(
        AllocateAnyPages,
        EfiBootServicesData,  // Boot services memory for boot process
        (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE,
        (EFI_PHYSICAL_ADDRESS*)PhysicalAddress
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate boot memory: %r\n", Status);
        return Status;
    }
    
    *VirtualAddress = (VOID*)(UINTN)(*PhysicalAddress);
    
    // Clear the allocated memory
    ZeroMem(*VirtualAddress, Size);
    
    Print(L"Boot memory allocated - Virtual: 0x%x, Physical: 0x%x\n", *VirtualAddress, *PhysicalAddress);
    
    return EFI_SUCCESS;
}

// Function to dump memory map for debugging
EFI_STATUS
PpcDumpMemoryMap (
    VOID
    )
{
    if (g_MemoryManager.MemoryMap == NULL) {
        Print(L"No memory map available\n");
        return EFI_NOT_READY;
    }
    
    Print(L"Memory Map (%d descriptors)\n", g_MemoryManager.MapSize / g_MemoryManager.DescriptorSize);
    Print(L"----------------------------------------\n");
    
    EFI_MEMORY_DESCRIPTOR* Desc = g_MemoryManager.MemoryMap;
    
    for (UINTN i = 0; i < g_MemoryManager.MapSize / g_MemoryManager.DescriptorSize; i++) {
        Print(L"Type: %d, Physical: 0x%x, Virtual: 0x%x, Pages: %d\n", 
              Desc->Type,
              Desc->PhysicalStart,
              Desc->VirtualStart,
              Desc->NumberOfPages);
        
        // Move to next descriptor
        Desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Desc + g_MemoryManager.DescriptorSize);
    }
    
    return EFI_SUCCESS;
}
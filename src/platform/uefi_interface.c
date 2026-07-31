#include "uefi_interface.h"
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>

// UEFI interface context
typedef struct {
    BOOLEAN IsInitialized;
    EFI_SYSTEM_TABLE* SystemTable;
    EFI_HANDLE ImageHandle;
    EFI_LOADED_IMAGE* LoadedImage;
} PPC_UEFI_CONTEXT;

// Global UEFI context
STATIC PPC_UEFI_CONTEXT g_UefiContext = {0};

EFI_STATUS
PpcInitializeUefiInterface (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE* SystemTable
    )
{
    // Initialize the UEFI interface context
    ZeroMem(&g_UefiContext, sizeof(g_UefiContext));
    
    g_UefiContext.IsInitialized = TRUE;
    g_UefiContext.ImageHandle = ImageHandle;
    g_UefiContext.SystemTable = SystemTable;
    
    // Get the loaded image protocol
    EFI_STATUS Status = g_BS->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&g_UefiContext.LoadedImage
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get loaded image protocol: %r\n", Status);
        return Status;
    }
    
    Print(L"PowerPC UEFI Interface initialized\n");
    Print(L"Image handle: 0x%x\n", ImageHandle);
    Print(L"System table: 0x%x\n", SystemTable);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetSystemTable (
    OUT EFI_SYSTEM_TABLE** SystemTable
    )
{
    if (SystemTable == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    *SystemTable = g_UefiContext.SystemTable;
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetImageHandle (
    OUT EFI_HANDLE* ImageHandle
    )
{
    if (ImageHandle == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    *ImageHandle = g_UefiContext.ImageHandle;
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcAllocatePool (
    IN  EFI_ALLOCATE_TYPE PoolType,
    IN  UINTN Size,
    OUT VOID** Buffer
    )
{
    if (Buffer == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    EFI_STATUS Status = g_BS->AllocatePool(
        PoolType,
        Size,
        Buffer
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate pool: %r\n", Status);
        return Status;
    }
    
    Print(L"Allocated %d bytes from pool\n", Size);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcFreePool (
    IN VOID* Buffer
    )
{
    if (Buffer == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    EFI_STATUS Status = g_BS->FreePool(Buffer);
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to free pool: %r\n", Status);
        return Status;
    }
    
    Print(L"Freed buffer from pool\n");
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetMemoryMap (
    IN OUT UINTN* MemoryMapSize,
    OUT EFI_MEMORY_DESCRIPTOR* MemoryMap,
    OUT UINTN* MapKey,
    OUT UINTN* DescriptorSize,
    OUT UINT32* DescriptorVersion
    )
{
    // Get the memory map from UEFI
    EFI_STATUS Status = g_BS->GetMemoryMap(
        MemoryMapSize,
        MemoryMap,
        MapKey,
        DescriptorSize,
        DescriptorVersion
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get memory map: %r\n", Status);
        return Status;
    }
    
    Print(L"Retrieved memory map with %d descriptors\n", *MemoryMapSize / *DescriptorSize);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcOutputString (
    IN CHAR16* String
    )
{
    if (String == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Output string via UEFI console
    g_ST->ConOut->OutputString(g_ST->ConOut, String);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetVariable (
    IN     CHAR16* VariableName,
    IN     EFI_GUID* VendorGuid,
    OUT    UINT32* Attributes,
    IN OUT UINTN* DataSize,
    OUT    VOID* Data
    )
{
    if (VariableName == NULL || VendorGuid == NULL || DataSize == NULL || Data == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    EFI_STATUS Status = g_BS->GetVariable(
        VariableName,
        VendorGuid,
        Attributes,
        DataSize,
        Data
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get variable %s: %r\n", VariableName, Status);
        return Status;
    }
    
    Print(L"Retrieved variable %s\n", VariableName);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcSetVariable (
    IN     CHAR16* VariableName,
    IN     EFI_GUID* VendorGuid,
    IN     UINT32 Attributes,
    IN     UINTN DataSize,
    IN     VOID* Data
    )
{
    if (VariableName == NULL || VendorGuid == NULL || Data == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    EFI_STATUS Status = g_BS->SetVariable(
        VariableName,
        VendorGuid,
        Attributes,
        DataSize,
        Data
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to set variable %s: %r\n", VariableName, Status);
        return Status;
    }
    
    Print(L"Set variable %s\n", VariableName);
    
    return EFI_SUCCESS;
}
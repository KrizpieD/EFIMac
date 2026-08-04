#ifndef __PPC_UEFI_INTERFACE_H__
#define __PPC_UEFI_INTERFACE_H__

#include <efi.h>

/**
  Initialize PowerPC UEFI interface
  @param[in] ImageHandle   Handle of the loaded image
  @param[in] SystemTable   Pointer to the EFI system table
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeUefiInterface (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE* SystemTable
    );

/**
  Get the EFI system table pointer
  @param[out] SystemTable Pointer to store system table pointer
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetSystemTable (
    OUT EFI_SYSTEM_TABLE** SystemTable
    );

/**
  Get the image handle
  @param[out] ImageHandle Pointer to store image handle
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetImageHandle (
    OUT EFI_HANDLE* ImageHandle
    );

/**
  Allocate memory from UEFI pool
  @param[in]  PoolType Type of pool allocation
  @param[in]  Size     Size of memory to allocate
  @param[out] Buffer   Pointer to store allocated buffer address
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcAllocatePool (
    IN  EFI_MEMORY_TYPE PoolType,
    IN  UINTN Size,
    OUT VOID** Buffer
    );

/**
  Free memory from UEFI pool
  @param[in] Buffer Address of buffer to free
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcFreePool (
    IN VOID* Buffer
    );

/**
  Get the UEFI memory map
  @param[in,out] MemoryMapSize   Size of memory map buffer (in/out)
  @param[out]    MemoryMap       Pointer to memory map buffer
  @param[out]    MapKey          Pointer to store memory map key
  @param[out]    DescriptorSize  Pointer to store descriptor size
  @param[out]    DescriptorVersion Pointer to store descriptor version
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetMemoryMap (
    IN OUT UINTN* MemoryMapSize,
    OUT EFI_MEMORY_DESCRIPTOR* MemoryMap,
    OUT UINTN* MapKey,
    OUT UINTN* DescriptorSize,
    OUT UINT32* DescriptorVersion
    );

/**
  Output string to UEFI console
  @param[in] String String to output
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcOutputString (
    IN CHAR16* String
    );

/**
  Get a UEFI variable
  @param[in]     VariableName   Name of the variable
  @param[in]     VendorGuid     Vendor GUID of the variable
  @param[out]    Attributes     Pointer to store variable attributes
  @param[in,out] DataSize       Size of data buffer (in/out)
  @param[out]    Data           Pointer to buffer to store data
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetVariable (
    IN     CHAR16* VariableName,
    IN     EFI_GUID* VendorGuid,
    OUT    UINT32* Attributes,
    IN OUT UINTN* DataSize,
    OUT    VOID* Data
    );

/**
  Set a UEFI variable
  @param[in] VariableName   Name of the variable
  @param[in] VendorGuid     Vendor GUID of the variable
  @param[in] Attributes     Attributes for the variable
  @param[in] DataSize       Size of data to set
  @param[in] Data           Pointer to data to set
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcSetVariable (
    IN     CHAR16* VariableName,
    IN     EFI_GUID* VendorGuid,
    IN     UINT32 Attributes,
    IN     UINTN DataSize,
    IN     VOID* Data
    );

/**
  Get the Simple File System protocol for a device handle
  @param[out] FileSystem    Pointer to store the file system interface
  @param[in]  DeviceHandle  Device handle to query (NULL = boot device)
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetFileSystem (
    OUT EFI_FILE_IO_INTERFACE** FileSystem,
    IN  EFI_HANDLE              DeviceHandle
    );

/**
  Load a file from a file system into a pool buffer (real UEFI file I/O)
  @param[in]  FileSystem   Simple File System protocol instance
  @param[in]  FileName     Path of the file to load
  @param[out] FileBuffer   Allocated buffer with file contents
  @param[out] FileSize     Size of the loaded file in bytes
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcLoadFile (
    IN  EFI_FILE_IO_INTERFACE* FileSystem,
    IN  CHAR16*                FileName,
    OUT VOID**                 FileBuffer,
    OUT UINTN*                 FileSize
    );

/**
  Get the handle of the device this image was booted from
  @param[out] DeviceHandle Pointer to store the boot device handle
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetBootDevice (
    OUT EFI_HANDLE* DeviceHandle
    );

/**
  Get system information from UEFI (real memory map walk)
  @param[out] SystemTable Pointer to the EFI system table
  @param[out] TotalMemory Total physical RAM reported by firmware
  @param[out] FreeMemory  Usable (conventional/boot services) memory
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetSystemInformation (
    OUT EFI_SYSTEM_TABLE** SystemTable,
    OUT UINT64* TotalMemory,
    OUT UINT64* FreeMemory
    );

/**
  Reset the system via the UEFI runtime reset service
  @param[in] ResetType   Type of reset to perform
  @param[in] StatusCode  Status code for the reset
  @param[in] DataSize    Size of ResetData
  @param[in] ResetData   Optional data passed to the reset service
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcResetSystem (
    IN EFI_RESET_TYPE ResetType,
    IN EFI_STATUS     StatusCode,
    IN UINTN          DataSize,
    IN CHAR16*        ResetData OPTIONAL
    );

#endif // __PPC_UEFI_INTERFACE_H__
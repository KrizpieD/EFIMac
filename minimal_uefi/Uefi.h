#ifndef __UEFI_H__
#define __UEFI_H__

// Minimal UEFI header file for compilation testing purposes only
// This is NOT a complete UEFI implementation - it's just enough to compile our code

#include <stdint.h>
#include <stddef.h>

// Basic types (simplified)
typedef uint8_t BOOLEAN;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;
typedef char CHAR8;
typedef wchar_t CHAR16;

// EFI_STATUS definition
typedef enum {
    EFI_SUCCESS = 0,
    EFI_LOAD_ERROR = 1,
    EFI_INVALID_PARAMETER = 2,
    EFI_UNSUPPORTED = 3,
    EFI_BAD_BUFFER_SIZE = 4,
    EFI_BUFFER_TOO_SMALL = 5,
    EFI_NOT_READY = 6,
    EFI_DEVICE_ERROR = 7,
    EFI_WRITE_PROTECTED = 8,
    EFI_OUT_OF_RESOURCES = 9,
    EFI_VOLUME_CORRUPTED = 10,
    EFI_VOLUME_FULL = 11,
    EFI_NO_MEDIA = 12,
    EFI_MEDIA_CHANGED = 13,
    EFI_NOT_FOUND = 14,
    EFI_ACCESS_DENIED = 15,
    EFI_NO_RESPONSE = 16,
    EFI_NO_MAPPING = 17,
    EFI_TIMEOUT = 18,
    EFI_NOT_STARTED = 19,
    EFI_ALREADY_STARTED = 20,
    EFI_ABORTED = 21,
    EFI_ICMP_ERROR = 22,
    EFI_TFTP_ERROR = 23,
    EFI_PROTOCOL_ERROR = 24,
    EFI_INCOMPATIBLE_VERSION = 25,
    EFI_SECURITY_VIOLATION = 26,
    EFI_CRC_ERROR = 27,
    EFI_END_OF_MEDIA = 28,
    EFI_END_OF_FILE = 1000,
    EFI_WARN_UNKNOWN_GLYPH = 1,
    EFI_WARN_DELETE_FAILURE = 2,
    EFI_WARN_WRITE_FAILURE = 3,
    EFI_WARN_BUFFER_TOO_SMALL = 4
} EFI_STATUS;

// Basic EFI structures
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8 Data4[8];
} EFI_GUID;

// Simple EFI_HANDLE definition for testing purposes
typedef void* EFI_HANDLE;

// EFI_SYSTEM_TABLE structure (minimal)
typedef struct {
    // This is a minimal stub - real implementation would be much more complex
    UINT64 FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void* ConIn;
    EFI_HANDLE ConsoleOutHandle;
    void* ConOut;
    EFI_HANDLE StandardErrorHandle;
    void* StdErr;
    void* RuntimeServices;
    void* BootServices;
    UINTN NumberOfTableEntries;
    void* ConfigurationTable;
} EFI_SYSTEM_TABLE;

// EFI_BOOT_SERVICES structure (minimal)
typedef struct {
    // Minimal stub for testing
    EFI_STATUS (*AllocatePool)(UINT32 PoolType, UINTN Size, void** Buffer);
    EFI_STATUS (*FreePool)(void* Buffer);
    EFI_STATUS (*GetMemoryMap)(UINTN* MemoryMapSize, void* MemoryMap, UINTN* MapKey, UINTN* DescriptorSize, UINT32* DescriptorVersion);
} EFI_BOOT_SERVICES;

// EFI_RUNTIME_SERVICES structure (minimal)
typedef struct {
    // Minimal stub for testing
} EFI_RUNTIME_SERVICES;

// EFI_LOADED_IMAGE_PROTOCOL structure (minimal)
typedef struct {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE* SystemTable;
    EFI_HANDLE DeviceHandle;
    void* FilePath;
    void* Reserved;
    UINT32 LoadOptionsSize;
    void* LoadOptions;
    void* ImageBase;
    UINT64 ImageSize;
    UINT32 ImageCodeType;
    UINT32 ImageDataType;
    void* Unload;
} EFI_LOADED_IMAGE;

// Protocol GUIDs (minimal)
extern const EFI_GUID gEfiLoadedImageProtocolGuid;

// Function prototypes for basic UEFI functions
EFI_STATUS EFIAPI EfiInitializeLib(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable);
EFI_STATUS EFIAPI EfiLibInstallProtocolInterfaces(EFI_HANDLE* Handle, ...);

// Constants
#define EFI_PAGE_SIZE 4096
#define EFI_PAGE_MASK 0xFFF

// Memory types for allocation
typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

// Memory descriptor structure (simplified)
typedef struct {
    UINT32 Type;
    UINT64 PhysicalStart;
    UINT64 VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

// EFI_ALLOCATE_TYPE definition
typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

// EFI_RESET_TYPE definition
typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown,
    EfiResetPlatformSpecific
} EFI_RESET_TYPE;

#endif // __UEFI_H__
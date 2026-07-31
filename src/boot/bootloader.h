#ifndef __PPC_BOOTLOADER_H__
#define __PPC_BOOTLOADER_H__

#include <Uefi.h>

// Boot modes
#define PPC_BOOT_MODE_NORMAL    0
#define PPC_BOOT_MODE_RECOVERY  1
#define PPC_BOOT_MODE_DIAGNOSTIC 2

// Boot parameters structure
typedef struct {
    UINT32 BootMode;
    UINT32 MemorySizeMB;
    UINT32 VideoMode;
    BOOLEAN EnableDebug;
    CHAR16* CommandLine;
} PPC_BOOT_PARAMETERS;

// Boot information structure
typedef struct {
    BOOLEAN IsInitialized;
    EFI_PHYSICAL_ADDRESS KernelAddress;
    UINT64 KernelSize;
    BOOLEAN KernelLoaded;
} PPC_BOOT_INFO;

/**
  Initialize PowerPC bootloader
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeBootloader (
    VOID
    );

/**
  Load kernel image into memory
  @param[in]  ImagePath      Path to the kernel image
  @param[out] KernelAddress  Pointer to store kernel load address
  @param[out] KernelSize     Pointer to store kernel size
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcLoadKernel (
    IN  CHAR16* ImagePath,
    OUT EFI_PHYSICAL_ADDRESS* KernelAddress,
    OUT UINT64* KernelSize
    );

/**
  Boot the PowerPC system
  @param[in] KernelAddress Address of kernel to boot
  @param[in] KernelSize    Size of kernel to boot
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcBootSystem (
    IN EFI_PHYSICAL_ADDRESS KernelAddress,
    IN UINT64               KernelSize
    );

/**
  Load a boot image into memory
  @param[in]  ImagePath   Path to the boot image
  @param[out] ImageBuffer Pointer to store image buffer address
  @param[out] ImageSize   Pointer to store image size
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcLoadBootImage (
    IN  CHAR16* ImagePath,
    OUT VOID**  ImageBuffer,
    OUT UINT64* ImageSize
    );

/**
  Set boot parameters for system boot
  @param[in] Parameters Boot parameters to set
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcSetBootParameters (
    IN PPC_BOOT_PARAMETERS* Parameters
    );

/**
  Get current boot information
  @param[out] BootInfo Pointer to structure to fill with boot info
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetBootInfo (
    OUT PPC_BOOT_INFO* BootInfo
    );

#endif // __PPC_BOOTLOADER_H__
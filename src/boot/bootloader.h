#ifndef __PPC_BOOTLOADER_H__
#define __PPC_BOOTLOADER_H__

#include <efi.h>

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

// Classic Mac OS PPC boot memory map (guest-visible addresses)
#define PPC_ROM_GUEST_BASE      0xFFF00000  // ROM window (classic PPC Macs)
#define PPC_ROM_MAX_SIZE        0x00400000  // 4 MB ROM window
#define PPC_ROM_DEFAULT_PATH    L"\\System\\MacOS\\ROM"
#define PPC_RESET_VECTOR        (PPC_ROM_GUEST_BASE + 0x100)
#define PPC_LOW_MEM_GUEST_BASE  0x00000000  // Low-memory globals
#define PPC_LOW_MEM_SIZE        0x4000      // 16 KB

// Low-memory global offsets (emulator-defined boot info block)
#define PPC_LOW_MEM_MAGIC_OFFSET    0x0000
#define PPC_LOW_MEM_BOOTINFO_OFFSET 0x0100

// System Folder layout on the boot volume (classic Mac OS)
#define PPC_SYSTEM_FOLDER_PATH      L"\\System Folder"
#define PPC_SYSTEM_FILE_PATH        L"\\System Folder\\System"
#define PPC_FINDER_FILE_PATH        L"\\System Folder\\Finder"
#define PPC_EXTENSIONS_DIR_PATH     L"\\System Folder\\Extensions"
#define PPC_SYSTEM_FOLDER_ROM_PATH  L"\\System Folder\\Extensions\\Mac OS ROM"

// HFS volume paths (Mac-style ':' separators) used when the System Folder is
// read from an attached Mac OS disc through the in-emulator HFS reader instead
// of the FAT boot volume.
#define PPC_HFS_SYSTEM_FOLDER_PATH  L"System Folder"
#define PPC_HFS_SYSTEM_FILE_PATH    L"System Folder:System"
#define PPC_HFS_FINDER_FILE_PATH    L"System Folder:Finder"
#define PPC_HFS_ROM_FILE_PATH       L"System Folder:Extensions:Mac OS ROM"

// Guest staging areas for system files and drivers
#define PPC_SYSTEM_AREA_GUEST_BASE  0x20000000  // System + Finder + Mac OS ROM
#define PPC_SYSTEM_AREA_SIZE        0x01000000  // 16 MB
#define PPC_DRIVER_AREA_GUEST_BASE  0x21000000  // Extensions (drivers)
#define PPC_DRIVER_AREA_SIZE        0x00800000  // 8 MB

// Limits for the system file / driver registry
#define PPC_SYSTEM_FOLDER_PATH_MAX  256
#define PPC_SYSTEM_FILE_NAME_MAX    64
#define PPC_SYSTEM_FILE_PATH_MAX    260
#define PPC_MAX_SYSTEM_FILES        6
#define PPC_MAX_DRIVERS             24

// Types of classic Mac OS system files
typedef enum {
    PPC_SYSTEM_FILE_TYPE_UNKNOWN = 0,
    PPC_SYSTEM_FILE_TYPE_SYSTEM,   // System file
    PPC_SYSTEM_FILE_TYPE_FINDER,   // Finder
    PPC_SYSTEM_FILE_TYPE_ROM,      // Mac OS ROM file
    PPC_SYSTEM_FILE_TYPE_DRIVER    // extension in the Extensions folder
} PPC_SYSTEM_FILE_TYPE;

// A staged system file or driver
typedef struct {
    PPC_SYSTEM_FILE_TYPE Type;
    BOOLEAN Loaded;
    CHAR16  Name[PPC_SYSTEM_FILE_NAME_MAX];
    CHAR16  Path[PPC_SYSTEM_FILE_PATH_MAX];
    UINT64  FileSize;      // size on disk
    UINT64  GuestAddress;  // guest address where staged (0 if not loaded)
    UINT64  StagedSize;    // bytes staged into guest memory
} PPC_SYSTEM_FILE;

// Aggregate report of the System Folder scan / staging results
typedef struct {
    BOOLEAN Found;
    CHAR16  Path[PPC_SYSTEM_FOLDER_PATH_MAX];
    BOOLEAN SystemPresent;
    BOOLEAN FinderPresent;
    BOOLEAN ExtensionsPresent;
    BOOLEAN MacOsRomPresent;
    UINTN   FileCount;
    UINTN   LoadedFileCount;
    UINTN   DriverCount;
    UINTN   LoadedDriverCount;
    UINT64  TotalStagedBytes;
    UINT64  SystemAreaBase;
    UINT64  DriverAreaBase;
} PPC_SYSTEM_FOLDER_INFO;

// Guest memory map as installed for a classic Mac OS boot
typedef struct {
    BOOLEAN RomInstalled;
    UINT64  RomBase;
    UINT64  RomSize;
    BOOLEAN LowMemoryInstalled;
    UINT64  LowMemoryBase;
    UINT64  LowMemorySize;
    BOOLEAN Ready;
} PPC_GUEST_MEMORY_MAP;

// Boot information structure
typedef struct {
    BOOLEAN IsInitialized;
    EFI_PHYSICAL_ADDRESS KernelAddress;
    UINT64 KernelSize;
    BOOLEAN KernelLoaded;
    BOOLEAN SystemReady;
    PPC_GUEST_MEMORY_MAP MemoryMap;
    PPC_SYSTEM_FOLDER_INFO SystemFolder;
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

/**
  Set up the boot environment for the PowerPC system
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcSetupBootEnvironment (
    VOID
    );

/**
  Verify a loaded kernel image: bounds check against guest RAM and
  read the first word to confirm the data was read correctly.
  @param[in] KernelAddress Address of the loaded kernel
  @param[in] KernelSize    Size of the loaded kernel
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcVerifyKernel (
    IN  EFI_PHYSICAL_ADDRESS KernelAddress,
    IN  UINT64               KernelSize
    );

/**
  Load a system ROM image into memory
  @param[in]  RomPath    Path to the ROM image
  @param[out] RomBuffer  Pointer to store ROM buffer address
  @param[out] RomSize    Pointer to store ROM size
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcLoadSystemRom (
    IN  CHAR16* RomPath,
    OUT VOID**  RomBuffer,
    OUT UINT64* RomSize
    );

/**
  Load a system ROM image from the boot volume and map it into guest memory
  at PPC_ROM_GUEST_BASE as a read-only region.
  @param[in]  RomPath     Path to the ROM image on the boot volume
  @param[out] RomAddress  Guest address of the installed ROM (may be NULL)
  @param[out] RomSize     Installed ROM size in bytes (may be NULL)
  @retval EFI_SUCCESS          ROM installed
  @retval EFI_NOT_FOUND        ROM file not present on the volume
  @retval EFI_ALREADY_STARTED  A ROM is already installed
**/
EFI_STATUS
EFIAPI
PpcInstallSystemRom (
    IN  CHAR16* RomPath,
    OUT UINT64* RomAddress,
    OUT UINT64* RomSize
    );

/**
  Install a self-contained demo ROM at PPC_ROM_GUEST_BASE (read-only). The
  demo ROM contains a small reset-vector program that reads the ROM magic
  word and stores its successor into guest RAM.
  @param[out] RomAddress  Guest address of the installed ROM (may be NULL)
  @param[out] RomSize     Installed ROM size in bytes (may be NULL)
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInstallDemoRom (
    OUT UINT64* RomAddress,
    OUT UINT64* RomSize
    );

/**
  Install the classic Mac OS low-memory globals region at guest 0x00000000
  (16 KB, read/write) as a dedicated region below the kernel base.
  @param[out] LowMemAddress  Guest address of the region (may be NULL)
  @param[out] LowMemSize     Region size in bytes (may be NULL)
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInstallLowMemory (
    OUT UINT64* LowMemAddress,
    OUT UINT64* LowMemSize
    );

/**
  Run the boot memory map / system initialization self-test: low-memory
  read/write, ROM read-only enforcement, and a cross-region ROM -> RAM
  program executed from the reset vector.
  @retval EFI_SUCCESS       All checks passed
  @retval EFI_LOAD_ERROR    One or more checks failed
**/
EFI_STATUS
EFIAPI
PpcRunBootSelfTest (
    VOID
    );

/**
  Prepare the system for boot: ensure the guest memory map is installed,
  reset the CPU to the ROM reset vector with a boot-ready MSR, and write
  the emulator boot info block into low memory.
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcPrepareSystemForBoot (
    VOID
    );

/**
  Scan the boot volume for a classic Mac OS System Folder and record the
  presence of System, Finder, Extensions, and Mac OS ROM.
  @param[out] Info  Folder scan report (may be NULL)
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcLocateSystemFolder (
    OUT PPC_SYSTEM_FOLDER_INFO* Info
    );

/**
  Stage the System file, Finder, and Mac OS ROM file from the System Folder
  into the guest system staging area.
  @retval EFI_SUCCESS       Files staged
  @retval EFI_NOT_FOUND     No System Folder / no stageable files
  @retval EFI_ALREADY_STARTED  Files already staged
**/
EFI_STATUS
EFIAPI
PpcLoadSystemFiles (
    VOID
    );

/**
  Enumerate the Extensions folder and register each file as a driver.
  @retval EFI_SUCCESS
  @retval EFI_NOT_FOUND     No Extensions folder
**/
EFI_STATUS
EFIAPI
PpcScanExtensionsDirectory (
    VOID
    );

/**
  Stage every registered driver into the guest driver staging area.
  @retval EFI_SUCCESS       At least one driver staged
  @retval EFI_NOT_FOUND     No drivers registered
**/
EFI_STATUS
EFIAPI
PpcLoadDrivers (
    VOID
    );

/**
  Get the current System Folder scan / staging report.
  @param[out] Info  Report structure to fill
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetSystemFolderInfo (
    OUT PPC_SYSTEM_FOLDER_INFO* Info
    );

/**
  Get a single staged system file entry.
  @param[in]  Index  Entry index
  @param[out] File   Entry structure to fill
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetSystemFile (
    IN  UINTN Index,
    OUT PPC_SYSTEM_FILE* File
    );

/**
  Get a single registered driver entry.
  @param[in]  Index  Driver index
  @param[out] Driver Driver structure to fill
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetDriver (
    IN  UINTN Index,
    OUT PPC_SYSTEM_FILE* Driver
    );

/**
  Run the system files & drivers self-test: staged files read back through the
  interpreter memory path, low-memory boot info intact, registry consistent.
  @retval EFI_SUCCESS       All checks passed
  @retval EFI_LOAD_ERROR    One or more checks failed
**/
EFI_STATUS
EFIAPI
PpcRunSystemFilesSelfTest (
    VOID
    );

#endif // __PPC_BOOTLOADER_H__
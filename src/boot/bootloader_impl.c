#include "bootloader.h"
#include <efi.h>
#include <efilib.h>
#include "cpu/interpreter.h"
#include "cpu/translation.h"
#include "memory/manager.h"
#include "hardware/abstraction.h"
#include "platform/uefi_interface.h"

// Bootloader context structure with more complete implementation
typedef struct {
    BOOLEAN IsInitialized;
    CHAR16* BootImagePath;
    EFI_PHYSICAL_ADDRESS KernelAddress;
    UINT64 KernelSize;
    BOOLEAN KernelLoaded;
    BOOLEAN SystemBooting;
    PPC_BOOT_PARAMETERS BootParams;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
} PPC_BOOTLOADER_CONTEXT;

// Global bootloader context
STATIC PPC_BOOTLOADER_CONTEXT g_BootContext = {0};

EFI_STATUS
PpcInitializeBootloader (
    VOID
    )
{
    // Initialize the bootloader context
    ZeroMem(&g_BootContext, sizeof(g_BootContext));
    
    g_BootContext.IsInitialized = TRUE;
    g_BootContext.BootImagePath = NULL;
    g_BootContext.KernelAddress = 0;
    g_BootContext.KernelSize = 0;
    g_BootContext.KernelLoaded = FALSE;
    g_BootContext.SystemBooting = FALSE;
    
    // Initialize boot parameters
    ZeroMem(&g_BootContext.BootParams, sizeof(PPC_BOOT_PARAMETERS));
    g_BootContext.BootParams.BootMode = PPC_BOOT_MODE_NORMAL;
    g_BootContext.BootParams.MemorySizeMB = 128;  // Default 128MB
    g_BootContext.BootParams.VideoMode = PPC_GRAPHICS_MODE_640x480;
    g_BootContext.BootParams.EnableDebug = FALSE;
    
    Print(L"PowerPC Bootloader initialized\n");
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcLoadKernel (
    IN  CHAR16* ImagePath,
    OUT EFI_PHYSICAL_ADDRESS* KernelAddress,
    OUT UINT64* KernelSize
    )
{
    if (ImagePath == NULL || KernelAddress == NULL || KernelSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Loading kernel from: %s\n", ImagePath);

    // Real UEFI file I/O: resolve the file system of the boot device.
    EFI_FILE_IO_INTERFACE* Fs = NULL;
    EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get boot file system: %r\n", Status);
        return Status;
    }

    EFI_FILE_HANDLE Root = NULL;
    Status = Fs->OpenVolume(Fs, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open boot volume: %r\n", Status);
        return Status;
    }

    EFI_FILE_HANDLE KernelFile = NULL;
    Status = Root->Open(Root, &KernelFile, ImagePath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || KernelFile == NULL) {
        Print(L"Kernel image '%s' not found: %r\n", ImagePath, Status);
        Root->Close(Root);
        return (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
    }

    // Get the kernel file size.
    UINTN FileInfoSize = SIZE_OF_EFI_FILE_INFO + 260 * sizeof(CHAR16);
    EFI_FILE_INFO* FileInfo = AllocateZeroPool(FileInfoSize);
    if (FileInfo == NULL) {
        KernelFile->Close(KernelFile);
        Root->Close(Root);
        return EFI_OUT_OF_RESOURCES;
    }
    Status = KernelFile->GetInfo(KernelFile, &GenericFileInfo, &FileInfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get kernel file info: %r\n", Status);
        FreePool(FileInfo);
        KernelFile->Close(KernelFile);
        Root->Close(Root);
        return Status;
    }

    UINT64 FileSize = FileInfo->FileSize;
    FreePool(FileInfo);

    if (FileSize == 0 || FileSize > 0x10000000) {
        Print(L"Kernel image has invalid size %d\n", FileSize);
        KernelFile->Close(KernelFile);
        Root->Close(Root);
        return EFI_LOAD_ERROR;
    }

    // Destination: the UEFI-allocated guest RAM region (guest base 0x10000000).
    VOID*  GuestBuffer = NULL;
    UINT64 GuestBase   = 0;
    UINT64 GuestSize   = 0;
    Status = PpcGetGuestMemoryRegion(&GuestBuffer, &GuestBase, &GuestSize);
    if (EFI_ERROR(Status) || GuestBuffer == NULL || FileSize > GuestSize) {
        Print(L"Guest RAM unavailable for kernel load\n");
        KernelFile->Close(KernelFile);
        Root->Close(Root);
        return EFI_NOT_READY;
    }

    // Read the whole file into guest RAM (Read may return partial data).
    UINTN   BytesRead = 0;
    UINT64  Remaining = FileSize;
    BOOLEAN Failed    = FALSE;
    while (Remaining > 0) {
        UINTN Chunk = (UINTN)Remaining;
        Status = KernelFile->Read(KernelFile, &Chunk, (UINT8*)GuestBuffer + BytesRead);
        if (EFI_ERROR(Status) || Chunk == 0) {
            Print(L"Failed while reading kernel: %r\n", Status);
            Failed = TRUE;
            break;
        }
        BytesRead += Chunk;
        Remaining  -= Chunk;
    }

    KernelFile->Close(KernelFile);
    Root->Close(Root);

    if (Failed) {
        return EFI_LOAD_ERROR;
    }

    *KernelAddress = (EFI_PHYSICAL_ADDRESS)GuestBase;
    *KernelSize = FileSize;

    g_BootContext.KernelAddress = *KernelAddress;
    g_BootContext.KernelSize = *KernelSize;
    g_BootContext.KernelLoaded = TRUE;

    Print(L"Kernel loaded: %d bytes into guest RAM at 0x%x\n",
          FileSize, (UINT32)GuestBase);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcBootSystem (
    IN EFI_PHYSICAL_ADDRESS KernelAddress,
    IN UINT64               KernelSize
    )
{
    if (!g_BootContext.KernelLoaded) {
        Print(L"Error: No kernel loaded for boot\n");
        return EFI_NOT_READY;
    }
    if (KernelAddress == 0 || KernelSize == 0) {
        return EFI_INVALID_PARAMETER;
    }

    // Configure the real CPU context for transfer of control to the kernel:
    // PC = kernel entry, MSR enables machine-check handling, SRR0/SRR1 seeded.
    g_PpcContext.Pc = (UINT32)KernelAddress;
    g_PpcContext.Srr0 = (UINT32)KernelAddress;
    g_PpcContext.Srr1 = g_PpcContext.Msr;
    g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;
    g_PpcContext.ExceptionPending = 0;

    Print(L"Booting system from kernel at 0x%x (size: %d bytes)\n", KernelAddress, KernelSize);
    Print(L"PowerPC core configured: PC=0x%x MSR=0x%08x\n",
          g_PpcContext.Pc, g_PpcContext.Msr);

    g_BootContext.SystemBooting = TRUE;

    return EFI_SUCCESS;
}

EFI_STATUS
PpcLoadBootImage (
    IN  CHAR16* ImagePath,
    OUT VOID**  ImageBuffer,
    OUT UINT64* ImageSize
    )
{
    if (ImagePath == NULL || ImageBuffer == NULL || ImageSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Real UEFI file I/O into a pool buffer.
    EFI_FILE_IO_INTERFACE* Fs = NULL;
    EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    UINTN  Size = 0;
    VOID*  Buffer = NULL;
    Status = PpcLoadFile(Fs, ImagePath, &Buffer, &Size);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    *ImageBuffer = Buffer;
    *ImageSize = Size;

    Print(L"Boot image loaded: %d bytes at 0x%x\n", Size, Buffer);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcSetBootParameters (
    IN PPC_BOOT_PARAMETERS* Parameters
    )
{
    if (Parameters == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Validate boot parameters
    if (Parameters->MemorySizeMB == 0 || Parameters->MemorySizeMB > 4096) {
        Print(L"Invalid memory size: %d MB\n", Parameters->MemorySizeMB);
        return EFI_INVALID_PARAMETER;
    }
    
    // In a real implementation:
    // 1. Validate boot parameters
    // 2. Store parameters for system boot
    // 3. Set up boot environment
    
    Print(L"Setting boot parameters\n");
    Print(L"Boot mode: %d\n", Parameters->BootMode);
    Print(L"Memory size: %d MB\n", Parameters->MemorySizeMB);
    Print(L"Video mode: %d\n", Parameters->VideoMode);
    Print(L"Debug enabled: %s\n", Parameters->EnableDebug ? L"YES" : L"NO");
    
    // Copy the parameters
    g_BootContext.BootParams = *Parameters;
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetBootInfo (
    OUT PPC_BOOT_INFO* BootInfo
    )
{
    if (BootInfo == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Fill boot information structure
    ZeroMem(BootInfo, sizeof(PPC_BOOT_INFO));
    
    BootInfo->IsInitialized = g_BootContext.IsInitialized;
    BootInfo->KernelAddress = g_BootContext.KernelAddress;
    BootInfo->KernelSize = g_BootContext.KernelSize;
    BootInfo->KernelLoaded = g_BootContext.KernelLoaded;
    
    return EFI_SUCCESS;
}

// Additional bootloader functions for PowerPC-specific boot requirements

EFI_STATUS
PpcLoadSystemRom (
    IN  CHAR16* RomPath,
    OUT VOID**  RomBuffer,
    OUT UINT64* RomSize
    )
{
    if (RomPath == NULL || RomBuffer == NULL || RomSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Loading system ROM from: %s\n", RomPath);

    // Real UEFI file I/O into a pool buffer.
    EFI_FILE_IO_INTERFACE* Fs = NULL;
    EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    UINTN  Size = 0;
    VOID*  Buffer = NULL;
    Status = PpcLoadFile(Fs, RomPath, &Buffer, &Size);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    *RomBuffer = Buffer;
    *RomSize = Size;

    Print(L"System ROM loaded: %d bytes at 0x%x\n", Size, Buffer);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcVerifyKernel (
    IN  EFI_PHYSICAL_ADDRESS KernelAddress,
    IN  UINT64               KernelSize
    )
{
    Print(L"Verifying kernel at 0x%x (size: %d bytes)\n", KernelAddress, KernelSize);

    // Real bounds check: the kernel must lie within the guest RAM region.
    VOID*  GuestBuffer = NULL;
    UINT64 GuestBase   = 0;
    UINT64 GuestSize   = 0;
    EFI_STATUS Status = PpcGetGuestMemoryRegion(&GuestBuffer, &GuestBase, &GuestSize);
    if (EFI_ERROR(Status)) {
        Print(L"Verification failed: guest RAM unavailable\n");
        return EFI_NOT_READY;
    }
    if ((UINT64)KernelAddress < GuestBase ||
        (UINT64)KernelAddress - GuestBase + KernelSize > GuestSize) {
        Print(L"Verification failed: kernel outside guest RAM bounds\n");
        return EFI_LOAD_ERROR;
    }
    if (KernelSize == 0) {
        Print(L"Verification failed: kernel size is zero\n");
        return EFI_LOAD_ERROR;
    }

    // Read the first word (big-endian) and report it as a sanity value.
    UINT32 FirstWord = PpcReadGuestByte((UINT32)KernelAddress)     << 24 |
                       PpcReadGuestByte((UINT32)KernelAddress + 1) << 16 |
                       PpcReadGuestByte((UINT32)KernelAddress + 2) << 8  |
                       PpcReadGuestByte((UINT32)KernelAddress + 3);
    Print(L"Kernel verification: bounds OK, first word 0x%08x\n", FirstWord);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcSetupBootEnvironment (
    VOID
    )
{
    Print(L"Setting up boot environment\n");
    
    // In a real implementation:
    // 1. Initialize boot environment variables
    // 2. Set up memory for boot process
    // 3. Configure system parameters
    // 4. Prepare for kernel execution
    
    // Initialize the PowerPC translation context
    EFI_STATUS Status = PpcInitializeTranslationContext();
    if (EFI_ERROR(Status)) {
        Print(L"Failed to initialize translation context: %r\n", Status);
        return Status;
    }
    
    // Initialize memory manager
    Status = PpcInitializeMemoryManager(0x00000000, 0x10000000);  // 256MB
    if (EFI_ERROR(Status)) {
        Print(L"Failed to initialize memory manager: %r\n", Status);
        return Status;
    }
    
    // Initialize hardware abstraction layer
    Status = PpcInitializeHardwareAbstraction();
    if (EFI_ERROR(Status)) {
        Print(L"Failed to initialize hardware abstraction: %r\n", Status);
        return Status;
    }
    
    // Initialize graphics for boot process
    Status = PpcInitializeGraphics(640, 480, 32);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to initialize graphics: %r\n", Status);
        return Status;
    }
    
    Print(L"Boot environment setup complete\n");
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcPrepareSystemForBoot (
    VOID
    )
{
    Print(L"Preparing system for boot\n");
    
    // In a real implementation:
    // 1. Finalize system state
    // 2. Save any required state information
    // 3. Set up registers and memory for kernel startup
    // 4. Prepare interrupt vectors
    
    Print(L"System preparation complete\n");
    
    return EFI_SUCCESS;
}
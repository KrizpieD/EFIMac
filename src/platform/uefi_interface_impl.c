#include "uefi_interface.h"
#include <efi.h>
#include <efilib.h>

// UEFI interface context with enhanced functionality
typedef struct {
    BOOLEAN IsInitialized;
    EFI_SYSTEM_TABLE* SystemTable;
    EFI_HANDLE ImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
    EFI_GUID* VendorGuid;
} PPC_UEFI_CONTEXT;

// Global UEFI context
static PPC_UEFI_CONTEXT g_UefiContext = {0};

// ---------------------------------------------------------------------------
// Fast serial console
//
// Diagnostic volume is the dominant cost of a boot soak. Routing it through
// ST->ConOut->OutputString (OVMF's serial SimpleTextOut instance) measured ~412
// bytes/s, which stretches every guest timer by orders of magnitude: the
// timebase advances per *instruction* (PPC_TIMEBASE_SCALE), so a console that
// blocks inside Print starves the guest clock. A 346 KB run took 840 s.
//
// We therefore emit straight to the PC's 16550 COM1 (0x3F8), which is where
// the harness's -serial stdio captures. The full SimpleTextOutput protocol is
// still forwarded to OVMF so the real UI (ClearScreen/SetAttribute/QueryMode)
// keeps working unchanged; only OutputString is intercepted.
// ---------------------------------------------------------------------------
#define PPC_UART_BASE      0x3F8
#define PPC_UART_LSR       (PPC_UART_BASE + 5)
#define PPC_UART_LSR_THRE  0x20        // transmit holding register empty

static BOOLEAN g_FastConsole = FALSE;
static SIMPLE_TEXT_OUTPUT_INTERFACE* g_OrigConOut = NULL;
static SIMPLE_TEXT_OUTPUT_INTERFACE g_FastConOut = {0};

static inline void PpcOutByte (UINT16 Port, UINT8 Value) {
    __asm__ __volatile__ ("outb %0, %1" :: "a"(Value), "Nd"(Port));
}

static inline UINT8 PpcInByte (UINT16 Port) {
    UINT8 Result;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(Result) : "Nd"(Port));
    return Result;
}

static VOID
PpcSerialPutChar (
    IN CHAR16 Ch
    )
{
    UINT8 Byte;
    UINTN ThreSpin;
    // Only the ASCII range reaches the serial log; anything else (and every
    // non-printable control char except the usual whitespace) becomes '.'.
    Byte = (Ch < 0x20 || Ch > 0x7E) ? (UINT8)'.' : (UINT8)Ch;
    // Bounded THRE wait: never wedge the emulator permanently on the virtual
    // UART (transport stalls have wedged whole soaks). Exceedance just drops
    // the character and the log resumes at the next one.
    for (ThreSpin = 0;
         (PpcInByte(PPC_UART_LSR) & PPC_UART_LSR_THRE) == 0 && ThreSpin < 200000000u;
         ThreSpin++) { }
    if (Byte == (UINT8)'\n') {
        PpcOutByte(PPC_UART_BASE, (UINT8)'\r');
        for (ThreSpin = 0;
             (PpcInByte(PPC_UART_LSR) & PPC_UART_LSR_THRE) == 0 && ThreSpin < 200000000u;
             ThreSpin++) { }
    }
    PpcOutByte(PPC_UART_BASE, Byte);
}

static EFI_STATUS EFIAPI
PpcFastConOutReset (
    IN SIMPLE_TEXT_OUTPUT_INTERFACE* This,
    IN BOOLEAN Extended
    )
{
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
PpcFastConOutOutputString (
    IN SIMPLE_TEXT_OUTPUT_INTERFACE* This,
    IN CHAR16* String
    )
{
    if (String == NULL) {
        return EFI_SUCCESS;
    }
    while (*String != 0) {
        PpcSerialPutChar(*String);
        String++;
    }
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
PpcFastConOutTestString (
    IN SIMPLE_TEXT_OUTPUT_INTERFACE* This,
    IN CHAR16* String
    )
{
    CHAR16* P;
    if (String == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    for (P = String; *P != 0; P++) {
        // Only ASCII printable bytes plus normal whitespace can survive the
        // 16550 path; reject the rest.
        if ((*P < 0x20 && *P != L'\n' && *P != L'\r' && *P != L'\t') || *P > 0x7E) {
            return EFI_UNSUPPORTED;
        }
    }
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
PpcFastConOutQueryMode (
    IN SIMPLE_TEXT_OUTPUT_INTERFACE* This,
    IN UINTN Mode,
    OUT UINTN* Columns,
    OUT UINTN* Rows
    )
{
    *Columns = 80; *Rows = 25;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
PpcFastConOutSetMode (
    IN SIMPLE_TEXT_OUTPUT_INTERFACE* This,
    IN UINTN Mode
    )
{
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
PpcFastConOutSetAttribute (
    IN SIMPLE_TEXT_OUTPUT_INTERFACE* This,
    IN UINTN Attribute
    )
{
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
PpcFastConOutClearScreen (
    IN SIMPLE_TEXT_OUTPUT_INTERFACE* This
    )
{
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
PpcFastConOutSetCursorPosition (
    IN SIMPLE_TEXT_OUTPUT_INTERFACE* This,
    IN UINTN Column,
    IN UINTN Row
    )
{
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
PpcFastConOutEnableCursor (
    IN SIMPLE_TEXT_OUTPUT_INTERFACE* This,
    IN BOOLEAN Visible
    )
{
    return EFI_SUCCESS;
}

static VOID
PpcInstallFastConsole (
    VOID
    )
{
    if (g_FastConsole || ST == NULL || ST->ConOut == NULL) {
        return;
    }
    g_OrigConOut = ST->ConOut;
    g_FastConOut.Reset               = PpcFastConOutReset;
    g_FastConOut.OutputString        = PpcFastConOutOutputString;
    g_FastConOut.TestString          = PpcFastConOutTestString;
    g_FastConOut.QueryMode           = PpcFastConOutQueryMode;
    g_FastConOut.SetMode             = PpcFastConOutSetMode;
    g_FastConOut.SetAttribute        = PpcFastConOutSetAttribute;
    g_FastConOut.ClearScreen         = PpcFastConOutClearScreen;
    g_FastConOut.SetCursorPosition   = PpcFastConOutSetCursorPosition;
    g_FastConOut.EnableCursor        = PpcFastConOutEnableCursor;
    g_FastConOut.Mode                = g_OrigConOut->Mode;
    ST->ConOut = &g_FastConOut;
    g_FastConsole = TRUE;
}

EFI_STATUS
PpcInitializeUefiInterface (
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE* SystemTable
    )
{
    // Initialize the UEFI interface context
    ZeroMem(&g_UefiContext, sizeof(g_UefiContext));

    // Route all console output straight to the 16550 COM1 port before the
    // first Print; this is what makes soaks fast.
    PpcInstallFastConsole();

    g_UefiContext.IsInitialized = TRUE;
    g_UefiContext.ImageHandle = ImageHandle;
    g_UefiContext.SystemTable = SystemTable;
    
    // Get the loaded image protocol
    EFI_STATUS Status = BS->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&g_UefiContext.LoadedImage
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get loaded image protocol: %r\n", Status);
        return Status;
    }
    
    // Vendor GUID not currently populated
    g_UefiContext.VendorGuid = NULL;
    
    Print(L"PowerPC UEFI Interface initialized\n");
    Print(L"Image handle: 0x%x\n", ImageHandle);
    Print(L"System table: 0x%x\n", SystemTable);
    Print(L"Loaded image: 0x%x\n", g_UefiContext.LoadedImage);
    
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
    IN  EFI_MEMORY_TYPE PoolType,
    IN  UINTN Size,
    OUT VOID** Buffer
    )
{
    if (Buffer == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    EFI_STATUS Status = BS->AllocatePool(
        PoolType,
        Size,
        Buffer
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate pool: %r\n", Status);
        return Status;
    }
    
    Print(L"Allocated %d bytes from pool (0x%x)\n", Size, *Buffer);
    
    // Clear the allocated memory
    ZeroMem(*Buffer, Size);
    
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
    
    EFI_STATUS Status = BS->FreePool(Buffer);
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to free pool: %r\n", Status);
        return Status;
    }
    
    Print(L"Freed buffer from pool (0x%x)\n", Buffer);
    
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
    EFI_STATUS Status = BS->GetMemoryMap(
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
    ST->ConOut->OutputString(ST->ConOut, String);
    
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
    
    EFI_STATUS Status = RT->GetVariable(
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
    
    Print(L"Retrieved variable %s (size: %d bytes)\n", VariableName, *DataSize);
    
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
    
    EFI_STATUS Status = RT->SetVariable(
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
    
    Print(L"Set variable %s (size: %d bytes)\n", VariableName, DataSize);
    
    return EFI_SUCCESS;
}

// Additional UEFI interface functions for PowerPC emulation needs

EFI_STATUS
PpcGetFileSystem (
    OUT EFI_FILE_IO_INTERFACE** FileSystem,
    IN  EFI_HANDLE              DeviceHandle
    )
{
    if (FileSystem == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Default to the device this image was booted from.
    if (DeviceHandle == NULL) {
        if (g_UefiContext.LoadedImage == NULL || g_UefiContext.LoadedImage->DeviceHandle == NULL) {
            return EFI_NOT_READY;
        }
        DeviceHandle = g_UefiContext.LoadedImage->DeviceHandle;
    }

    EFI_FILE_IO_INTERFACE* Fs = NULL;
    EFI_STATUS Status = BS->HandleProtocol(DeviceHandle, &FileSystemProtocol, (VOID**)&Fs);
    if (EFI_ERROR(Status) || Fs == NULL) {
        Print(L"Failed to get file system for device handle 0x%x: %r\n", DeviceHandle, Status);
        return EFI_NOT_FOUND;
    }

    *FileSystem = Fs;
    return EFI_SUCCESS;
}

EFI_STATUS
PpcLoadFile (
    IN  EFI_FILE_IO_INTERFACE* FileSystem,
    IN  CHAR16*                FileName,
    OUT VOID**                 FileBuffer,
    OUT UINTN*                 FileSize
    )
{
    if (FileSystem == NULL || FileName == NULL || FileBuffer == NULL || FileSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    *FileBuffer = NULL;
    *FileSize = 0;

    Print(L"Loading file: %s\n", FileName);

    EFI_FILE_HANDLE Root = NULL;
    EFI_STATUS Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open volume: %r\n", Status);
        return Status;
    }

    EFI_FILE_HANDLE File = NULL;
    Status = Root->Open(Root, &File, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || File == NULL) {
        Print(L"Failed to open '%s': %r\n", FileName, Status);
        Root->Close(Root);
        return (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
    }

    // Query the file size through EFI_FILE_INFO.
    UINTN FileInfoSize = SIZE_OF_EFI_FILE_INFO + 260 * sizeof(CHAR16);
    EFI_FILE_INFO* FileInfo = AllocateZeroPool(FileInfoSize);
    if (FileInfo == NULL) {
        File->Close(File);
        Root->Close(Root);
        return EFI_OUT_OF_RESOURCES;
    }
    Status = File->GetInfo(File, &GenericFileInfo, &FileInfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get file info for '%s': %r\n", FileName, Status);
        FreePool(FileInfo);
        File->Close(File);
        Root->Close(Root);
        return Status;
    }

    UINT64 FileSize64 = FileInfo->FileSize;
    FreePool(FileInfo);

    if (FileSize64 == 0 || FileSize64 > (UINT64)0x7FFFFFFF) {
        Print(L"File '%s' has invalid size %d\n", FileName, FileSize64);
        File->Close(File);
        Root->Close(Root);
        return EFI_LOAD_ERROR;
    }

    VOID* Buffer = AllocatePool((UINTN)FileSize64);
    if (Buffer == NULL) {
        File->Close(File);
        Root->Close(Root);
        return EFI_OUT_OF_RESOURCES;
    }

    // Read may return partial data; loop until the whole file is read.
    UINTN   BytesRead = 0;
    UINT64  Remaining = FileSize64;
    BOOLEAN Failed    = FALSE;
    while (Remaining > 0) {
        UINTN Chunk = (UINTN)Remaining;
        Status = File->Read(File, &Chunk, (UINT8*)Buffer + BytesRead);
        if (EFI_ERROR(Status) || Chunk == 0) {
            Print(L"Failed while reading '%s': %r\n", FileName, Status);
            Failed = TRUE;
            break;
        }
        BytesRead += Chunk;
        Remaining  -= Chunk;
    }

    File->Close(File);
    Root->Close(Root);

    if (Failed) {
        FreePool(Buffer);
        return EFI_LOAD_ERROR;
    }

    *FileBuffer = Buffer;
    *FileSize = (UINTN)FileSize64;

    Print(L"Loaded '%s': %d bytes at 0x%x\n", FileName, *FileSize, Buffer);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetBootDevice (
    OUT EFI_HANDLE* DeviceHandle
    )
{
    if (DeviceHandle == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (g_UefiContext.LoadedImage == NULL || g_UefiContext.LoadedImage->DeviceHandle == NULL) {
        return EFI_NOT_READY;
    }

    *DeviceHandle = g_UefiContext.LoadedImage->DeviceHandle;

    Print(L"Boot device handle: 0x%x\n", *DeviceHandle);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetFirmwareVersion (
    OUT CHAR16* VersionString,
    IN  UINTN   StringSize
    )
{
    if (VersionString == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // In a real implementation:
    // 1. Get firmware version from UEFI system table
    // 2. Format version string
    
    StrCpy(VersionString, L"EFI-Mac-Emulator v0.1");
    
    Print(L"Firmware version: %s\n", VersionString);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetSystemInformation (
    OUT EFI_SYSTEM_TABLE** SystemTable,
    OUT UINT64* TotalMemory,
    OUT UINT64* FreeMemory
    )
{
    if (SystemTable == NULL || TotalMemory == NULL || FreeMemory == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    *SystemTable = g_UefiContext.SystemTable;

    // Walk the real UEFI memory map to compute total and usable memory.
    UINTN  MapSize  = 0;
    UINTN  MapKey   = 0;
    UINTN  DescSize = 0;
    UINT32 DescVer  = 0;
    EFI_STATUS Status = BS->GetMemoryMap(&MapSize, NULL, &MapKey, &DescSize, &DescVer);
    if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"Failed to size memory map: %r\n", Status);
        return Status;
    }

    UINT8* MapBuffer = AllocatePool(MapSize + sizeof(EFI_MEMORY_DESCRIPTOR));
    if (MapBuffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = BS->GetMemoryMap(&MapSize, (EFI_MEMORY_DESCRIPTOR*)MapBuffer, &MapKey, &DescSize, &DescVer);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get memory map: %r\n", Status);
        FreePool(MapBuffer);
        return Status;
    }

    UINT64 Total = 0;
    UINT64 Usable = 0;
    EFI_MEMORY_DESCRIPTOR* Desc = (EFI_MEMORY_DESCRIPTOR*)MapBuffer;
    for (UINTN i = 0; i < MapSize / DescSize; i++) {
        UINT64 Bytes = Desc->NumberOfPages * EFI_PAGE_SIZE;
        Total += Bytes;
        if (Desc->Type == EfiConventionalMemory ||
            Desc->Type == EfiLoaderData ||
            Desc->Type == EfiLoaderCode ||
            Desc->Type == EfiBootServicesCode ||
            Desc->Type == EfiBootServicesData) {
            Usable += Bytes;
        }
        Desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Desc + DescSize);
    }

    FreePool(MapBuffer);

    *TotalMemory = Total;
    *FreeMemory = Usable;

    Print(L"System information: total %d bytes, usable %d bytes\n", Total, Usable);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcResetSystem (
    IN EFI_RESET_TYPE ResetType,
    IN EFI_STATUS StatusCode,
    IN UINTN DataSize,
    IN CHAR16* ResetData OPTIONAL
    )
{
    Print(L"Resetting system (type: %d)\n", ResetType);
    
    // In a real implementation:
    // 1. Save system state if needed
    // 2. Perform system reset using UEFI ResetSystem protocol
    
    return RT->ResetSystem(ResetType, StatusCode, DataSize, ResetData);
}
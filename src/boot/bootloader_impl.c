#include "bootloader.h"
#include <efi.h>
#include <efilib.h>
#include "cpu/interpreter.h"
#include "cpu/translation.h"
#include "memory/manager.h"
#include "hardware/abstraction.h"
#include "fs/hfs.h"
#include "platform/uefi_interface.h"

// Bootloader context structure with more complete implementation
typedef struct {
    BOOLEAN IsInitialized;
    CHAR16* BootImagePath;
    EFI_PHYSICAL_ADDRESS KernelAddress;
    UINT64 KernelSize;
    BOOLEAN KernelLoaded;
    BOOLEAN SystemBooting;
    BOOLEAN SystemReady;
    PPC_BOOT_PARAMETERS BootParams;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
    // Phase 5: classic Mac OS guest memory map state
    BOOLEAN LowMemoryInstalled;
    UINT64  LowMemoryAddress;
    UINT64  LowMemorySize;
    BOOLEAN NkSystemAreaInstalled;
    BOOLEAN RomLoaded;
    UINT64  RomAddress;
    UINT64  RomSize;
    VOID*   RomHostBuffer;
    UINT32  RomType;
    BOOLEAN RomDecoded;     // TRUE when a CHRP file was expanded to the flat image
    // Phase 5: system files and drivers (classic Mac OS System Folder)
    BOOLEAN SystemFolderScanned;
    BOOLEAN SystemFolderFound;
    BOOLEAN SystemFolderFromHfs;
    CHAR16  SystemFolderPath[PPC_SYSTEM_FOLDER_PATH_MAX];
    BOOLEAN SystemPresent;
    BOOLEAN FinderPresent;
    BOOLEAN ExtensionsPresent;
    BOOLEAN MacOsRomPresent;
    UINTN   SystemFileCount;
    UINTN   LoadedSystemFileCount;
    UINTN   DriverCount;
    UINTN   LoadedDriverCount;
    UINT64  TotalStagedBytes;
    BOOLEAN SystemAreaInstalled;
    UINT64  SystemAreaCursor;
    VOID*   SystemAreaHost;
    BOOLEAN DriverAreaInstalled;
    UINT64  DriverAreaCursor;
    VOID*   DriverAreaHost;
    PPC_SYSTEM_FILE SystemFiles[PPC_MAX_SYSTEM_FILES];
    VOID*   SystemFileHosts[PPC_MAX_SYSTEM_FILES];
    PPC_SYSTEM_FILE Drivers[PPC_MAX_DRIVERS];
    VOID*   DriverHosts[PPC_MAX_DRIVERS];
} PPC_BOOTLOADER_CONTEXT;

// Global bootloader context
STATIC PPC_BOOTLOADER_CONTEXT g_BootContext = {0};

// ---------------------------------------------------------------------------
// File load helper: read a whole file from the boot volume into a page-aligned
// buffer (UEFI pool allocations are limited to ~128 KB; Mac OS ROM images are
// several MB, so ROM/ROM-like blobs are page-backed).
// ---------------------------------------------------------------------------
STATIC
EFI_STATUS
BootOpenFile (
    IN  CHAR16* FilePath,
    OUT EFI_FILE_HANDLE* Root,
    OUT EFI_FILE_HANDLE* File,
    OUT UINT64* FileSize
    )
{
    EFI_FILE_IO_INTERFACE* Fs = NULL;
    EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_FILE_HANDLE RootHandle = NULL;
    Status = Fs->OpenVolume(Fs, &RootHandle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_FILE_HANDLE FileHandle = NULL;
    Status = RootHandle->Open(RootHandle, &FileHandle, FilePath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || FileHandle == NULL) {
        RootHandle->Close(RootHandle);
        return (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
    }

    UINTN FileInfoSize = SIZE_OF_EFI_FILE_INFO + 260 * sizeof(CHAR16);
    EFI_FILE_INFO* Info = AllocateZeroPool(FileInfoSize);
    if (Info == NULL) {
        FileHandle->Close(FileHandle);
        RootHandle->Close(RootHandle);
        return EFI_OUT_OF_RESOURCES;
    }
    Status = FileHandle->GetInfo(FileHandle, &GenericFileInfo, &FileInfoSize, Info);
    if (EFI_ERROR(Status)) {
        FreePool(Info);
        FileHandle->Close(FileHandle);
        RootHandle->Close(RootHandle);
        return Status;
    }

    *Root = RootHandle;
    *File = FileHandle;
    *FileSize = Info->FileSize;
    FreePool(Info);
    return EFI_SUCCESS;
}

STATIC
EFI_STATUS
BootReadFileInto (
    IN  EFI_FILE_HANDLE File,
    IN  CHAR16*         FilePath,
    IN  VOID*           Dst,
    IN  UINTN           DstSize,
    OUT UINTN*          BytesRead
    )
{
    UINT64 Remaining = DstSize;
    UINTN  Offset    = 0;
    EFI_STATUS Status;

    while (Remaining > 0) {
        UINTN Chunk = (UINTN)Remaining;
        Status = File->Read(File, &Chunk, (UINT8*)Dst + Offset);
        if (EFI_ERROR(Status)) {
            Print(L"Failed while reading '%s': %r\n", FilePath, Status);
            return EFI_LOAD_ERROR;
        }
        if (Chunk == 0) {
            break;  // clean end of file
        }
        Offset += Chunk;
        Remaining -= Chunk;
    }

    if (BytesRead != NULL) {
        *BytesRead = Offset;
    }
    return EFI_SUCCESS;
}

// Read a whole file from the boot volume into a page-aligned buffer (UEFI
// pool allocations are limited to ~128 KB; ROM and system blobs are several MB,
// so they are page-backed).
STATIC
EFI_STATUS
BootReadFileToPages (
    IN  CHAR16* FilePath,
    IN  UINTN   MaxSize,
    OUT VOID**  Buffer,
    OUT UINTN*  Size
    )
{
    EFI_FILE_HANDLE Root = NULL;
    EFI_FILE_HANDLE File = NULL;
    UINT64 FileSize = 0;
    EFI_STATUS Status = BootOpenFile(FilePath, &Root, &File, &FileSize);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (FileSize == 0 || FileSize > MaxSize) {
        File->Close(File);
        Root->Close(Root);
        Print(L"File '%s' has invalid size %d (max %d)\n", FilePath, (UINT64)FileSize, (UINTN)MaxSize);
        return EFI_LOAD_ERROR;
    }

    UINTN Pages = (UINTN)((FileSize + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE);
    EFI_PHYSICAL_ADDRESS Base = 0;
    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        File->Close(File);
        Root->Close(Root);
        Print(L"Failed to allocate %d pages for '%s': %r\n", Pages, FilePath, Status);
        return Status;
    }

    Status = BootReadFileInto(File, FilePath, (VOID*)(UINTN)Base, (UINTN)FileSize, NULL);
    File->Close(File);
    Root->Close(Root);

    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        return Status;
    }

    *Buffer = (VOID*)(UINTN)Base;
    *Size = (UINTN)FileSize;
    return EFI_SUCCESS;
}

// Check whether a file exists on the boot volume and get its size.
STATIC
EFI_STATUS
BootFileExists (
    IN  CHAR16* FilePath,
    OUT BOOLEAN* Exists,
    OUT UINT64*  FileSize
    )
{
    EFI_FILE_HANDLE Root = NULL;
    EFI_FILE_HANDLE File = NULL;
    UINT64 Size = 0;
    EFI_STATUS Status = BootOpenFile(FilePath, &Root, &File, &Size);
    if (Status == EFI_NOT_FOUND) {
        if (Exists != NULL) { *Exists = FALSE; }
        if (FileSize != NULL) { *FileSize = 0; }
        return EFI_SUCCESS;
    }
    if (EFI_ERROR(Status)) {
        return Status;
    }
    File->Close(File);
    Root->Close(Root);

    if (Exists != NULL) { *Exists = (Size > 0); }
    if (FileSize != NULL) { *FileSize = Size; }
    return EFI_SUCCESS;
}

// Check whether a directory exists on the boot volume.
STATIC
EFI_STATUS
BootDirectoryExists (
    IN  CHAR16* DirPath,
    OUT BOOLEAN* Exists
    )
{
    EFI_FILE_IO_INTERFACE* Fs = NULL;
    EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_FILE_HANDLE Root = NULL;
    Status = Fs->OpenVolume(Fs, &Root);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_FILE_HANDLE Dir = NULL;
    Status = Root->Open(Root, &Dir, DirPath, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || Dir == NULL) {
        Root->Close(Root);
        if (Exists != NULL) { *Exists = FALSE; }
        if (Status == EFI_NOT_FOUND) {
            return EFI_SUCCESS;
        }
        return (Status == EFI_SUCCESS) ? EFI_SUCCESS : Status;
    }

    Dir->Close(Dir);
    Root->Close(Root);
    if (Exists != NULL) { *Exists = TRUE; }
    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Boot self-test bookkeeping (mirrors the CPU self-test style)
// ---------------------------------------------------------------------------
STATIC UINTN g_BootTestPasses   = 0;
STATIC UINTN g_BootTestFailures = 0;

STATIC VOID
BootSelfTestCheck (
    IN BOOLEAN Ok,
    IN CHAR16* Name
    )
{
    if (Ok) {
        g_BootTestPasses++;
        Print(L"  [PASS] %s\n", Name);
    } else {
        g_BootTestFailures++;
        Print(L"  [FAIL] %s\n", Name);
    }
}

// Write a big-endian 32-bit word into guest memory.
STATIC VOID
BootWriteWord32 (
    IN UINT32 Address,
    IN UINT32 Value
    )
{
    PpcWriteGuestByte(Address + 0, (UINT8)(Value >> 24));
    PpcWriteGuestByte(Address + 1, (UINT8)(Value >> 16));
    PpcWriteGuestByte(Address + 2, (UINT8)(Value >> 8));
    PpcWriteGuestByte(Address + 3, (UINT8)Value);
}

// NUL-terminated bounded string copy (avoids GNU-EFI StrnCpy padding pitfalls).
STATIC VOID
BootCopyString (
    OUT CHAR16* Dst,
    IN  CHAR16* Src,
    IN  UINTN   MaxChars
    )
{
    UINTN I;
    for (I = 0; I + 1 < MaxChars && Src[I] != 0; I++) {
        Dst[I] = Src[I];
    }
    Dst[I] = 0;
}

// Case-insensitive CHAR16 comparison (ASCII; no GNU-EFI Stricmp dependency).
STATIC INTN
BootStriCmp (
    IN CHAR16* A,
    IN CHAR16* B
    )
{
    while (*A != 0 && *B != 0) {
        CHAR16 Ca = *A;
        CHAR16 Cb = *B;
        if (Ca >= L'a' && Ca <= L'z') { Ca -= (L'a' - L'A'); }
        if (Cb >= L'a' && Cb <= L'z') { Cb -= (L'a' - L'A'); }
        if (Ca != Cb) {
            return (Ca < Cb) ? -1 : 1;
        }
        A++;
        B++;
    }
    if (*A == *B) {
        return 0;
    }
    return (*A == 0) ? -1 : 1;
}

// Build "DirPath\FileName" into OutPath (bounded).
STATIC VOID
BootBuildPath (
    IN  CHAR16* DirPath,
    IN  CHAR16* FileName,
    OUT CHAR16* OutPath,
    IN  UINTN   MaxChars
    )
{
    UINTN I = 0;
    while (I + 1 < MaxChars && DirPath[I] != 0) {
        OutPath[I] = DirPath[I];
        I++;
    }
    if (I + 1 < MaxChars) {
        OutPath[I++] = L'\\';
    }
    {
        UINTN J = 0;
        while (I + 1 < MaxChars && FileName[J] != 0) {
            OutPath[I++] = FileName[J++];
        }
    }
    OutPath[I] = 0;
}

// Allocate and map the guest staging area for System/Finder/Mac OS ROM.
STATIC EFI_STATUS
BootEnsureSystemArea (
    VOID
    )
{
    if (g_BootContext.SystemAreaInstalled) {
        return EFI_SUCCESS;
    }
    UINTN Pages = PPC_SYSTEM_AREA_SIZE / EFI_PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS Base = 0;
    EFI_STATUS Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate system staging area: %r\n", Status);
        return Status;
    }
    ZeroMem((VOID*)(UINTN)Base, PPC_SYSTEM_AREA_SIZE);

    Status = PpcAddGuestMemoryRegion((VOID*)(UINTN)Base,
                                     PPC_SYSTEM_AREA_GUEST_BASE,
                                     PPC_SYSTEM_AREA_SIZE,
                                     FALSE);
    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        Print(L"Failed to map system staging area: %r\n", Status);
        return Status;
    }

    g_BootContext.SystemAreaInstalled = TRUE;
    g_BootContext.SystemAreaHost = (VOID*)(UINTN)Base;
    g_BootContext.SystemAreaCursor = PPC_SYSTEM_AREA_GUEST_BASE;
    Print(L"System staging area installed: guest 0x%x (%d MB, read/write)\n",
          PPC_SYSTEM_AREA_GUEST_BASE, PPC_SYSTEM_AREA_SIZE / (1024 * 1024));
    return EFI_SUCCESS;
}

// Allocate and map the guest staging area for drivers (extensions).
STATIC EFI_STATUS
BootEnsureDriverArea (
    VOID
    )
{
    if (g_BootContext.DriverAreaInstalled) {
        return EFI_SUCCESS;
    }
    UINTN Pages = PPC_DRIVER_AREA_SIZE / EFI_PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS Base = 0;
    EFI_STATUS Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate driver staging area: %r\n", Status);
        return Status;
    }
    ZeroMem((VOID*)(UINTN)Base, PPC_DRIVER_AREA_SIZE);

    Status = PpcAddGuestMemoryRegion((VOID*)(UINTN)Base,
                                     PPC_DRIVER_AREA_GUEST_BASE,
                                     PPC_DRIVER_AREA_SIZE,
                                     FALSE);
    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        Print(L"Failed to map driver staging area: %r\n", Status);
        return Status;
    }

    g_BootContext.DriverAreaInstalled = TRUE;
    g_BootContext.DriverAreaHost = (VOID*)(UINTN)Base;
    g_BootContext.DriverAreaCursor = PPC_DRIVER_AREA_GUEST_BASE;
    Print(L"Driver staging area installed: guest 0x%x (%d MB, read/write)\n",
          PPC_DRIVER_AREA_GUEST_BASE, PPC_DRIVER_AREA_SIZE / (1024 * 1024));
    return EFI_SUCCESS;
}

STATIC VOID
BootExtractFileName (
    IN  CHAR16* Path,
    OUT CHAR16* Name,
    IN  UINTN   MaxChars
    );

// Stage a single file from the boot volume into a guest staging area.
STATIC EFI_STATUS
BootStageFile (
    IN  CHAR16* FilePath,
    IN  PPC_SYSTEM_FILE_TYPE Type,
    IN  UINT64  AreaGuestBase,
    IN  UINTN   AreaSize,
    IN  VOID*   AreaHost,
    IN  UINT64* Cursor,
    OUT PPC_SYSTEM_FILE* OutFile,
    OUT VOID**  OutHost
    )
{
    EFI_FILE_HANDLE Root = NULL;
    EFI_FILE_HANDLE File = NULL;
    UINT64 FileSize = 0;
    EFI_STATUS Status = BootOpenFile(FilePath, &Root, &File, &FileSize);
    if (EFI_ERROR(Status)) {
        return Status;  // EFI_NOT_FOUND, etc.
    }
    if (FileSize == 0) {
        File->Close(File);
        Root->Close(Root);
        return EFI_NOT_FOUND;
    }

    UINTN Aligned = ((UINTN)FileSize + 0xF) & ~0xF;
    if ((UINT64)(*Cursor - AreaGuestBase) + Aligned > AreaSize) {
        File->Close(File);
        Root->Close(Root);
        Print(L"Staging area full for '%s'\n", FilePath);
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN Offset = (UINTN)(*Cursor - AreaGuestBase);
    UINTN BytesRead = 0;
    Status = BootReadFileInto(File, FilePath, (UINT8*)AreaHost + Offset, Aligned, &BytesRead);
    File->Close(File);
    Root->Close(Root);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    (VOID)BytesRead;

    OutFile->Type = Type;
    OutFile->Loaded = TRUE;
    OutFile->FileSize = FileSize;
    OutFile->GuestAddress = *Cursor;
    OutFile->StagedSize = Aligned;
    BootCopyString(OutFile->Path, FilePath, PPC_SYSTEM_FILE_PATH_MAX);
    BootExtractFileName(FilePath, OutFile->Name, PPC_SYSTEM_FILE_NAME_MAX);

    if (OutHost != NULL) {
        *OutHost = (UINT8*)AreaHost + Offset;
    }
    *Cursor += Aligned;
    g_BootContext.TotalStagedBytes += FileSize;

    return EFI_SUCCESS;
}

// Stage a single file from the mounted HFS volume into a guest staging area.
// Mirrors BootStageFile but reads the data fork through the in-emulator HFS
// reader (PpcHfsReadFile) instead of the FAT boot volume.
STATIC EFI_STATUS
BootStageHfsFile (
    IN  PPC_HFS_ENTRY*     Entry,
    IN  CHAR16*            ReportPath,
    IN  PPC_SYSTEM_FILE_TYPE Type,
    IN  UINT64             AreaGuestBase,
    IN  UINTN              AreaSize,
    IN  VOID*              AreaHost,
    IN  UINT64*            Cursor,
    OUT PPC_SYSTEM_FILE*   OutFile,
    OUT VOID**             OutHost
    )
{
    if (Entry == NULL || Entry->IsDirectory || Entry->Size == 0) {
        return EFI_NOT_FOUND;
    }

    UINTN FileSize = (UINTN)Entry->Size;
    UINTN Aligned = (FileSize + 0xF) & ~0xF;
    if ((UINT64)(*Cursor - AreaGuestBase) + Aligned > AreaSize) {
        Print(L"Staging area full for '%s'\n", ReportPath);
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN Offset = (UINTN)(*Cursor - AreaGuestBase);
    UINTN Got = FileSize;
    EFI_STATUS Status = PpcHfsReadFile(Entry, (UINT8*)AreaHost + Offset, &Got);
    if (EFI_ERROR(Status) || Got != FileSize) {
        return EFI_ERROR(Status) ? Status : EFI_LOAD_ERROR;
    }
    ZeroMem((UINT8*)AreaHost + Offset + FileSize, Aligned - FileSize);

    OutFile->Type = Type;
    OutFile->Loaded = TRUE;
    OutFile->FileSize = FileSize;
    OutFile->GuestAddress = *Cursor;
    OutFile->StagedSize = Aligned;
    BootCopyString(OutFile->Path, ReportPath, PPC_SYSTEM_FILE_PATH_MAX);
    BootCopyString(OutFile->Name, Entry->Name, PPC_SYSTEM_FILE_NAME_MAX);

    if (OutHost != NULL) {
        *OutHost = (UINT8*)AreaHost + Offset;
    }
    *Cursor += Aligned;
    g_BootContext.TotalStagedBytes += FileSize;

    return EFI_SUCCESS;
}

// Enumerate the Extensions folder and register every file as a driver.
STATIC EFI_STATUS
BootEnumerateExtensions (
    VOID
    )
{
    EFI_FILE_IO_INTERFACE* Fs = NULL;
    EFI_STATUS Status = PpcGetFileSystem(&Fs, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_FILE_HANDLE Root = NULL;
    Status = Fs->OpenVolume(Fs, &Root);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_FILE_HANDLE Dir = NULL;
    Status = Root->Open(Root, &Dir, PPC_EXTENSIONS_DIR_PATH, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status) || Dir == NULL) {
        Root->Close(Root);
        return (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
    }

    UINTN  BufSize = SIZE_OF_EFI_FILE_INFO + 260 * sizeof(CHAR16);
    UINT8* Buf = AllocateZeroPool(BufSize);
    if (Buf == NULL) {
        Dir->Close(Dir);
        Root->Close(Root);
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN Count = 0;
    for (;;) {
        UINTN ReadSize = BufSize;
        Status = Dir->Read(Dir, &ReadSize, Buf);
        if (Status == EFI_BUFFER_TOO_SMALL) {
            UINTN NewSize = BufSize * 2;
            UINT8* NewBuf = AllocateZeroPool(NewSize);
            if (NewBuf == NULL) {
                FreePool(Buf);
                Dir->Close(Dir);
                Root->Close(Root);
                return EFI_OUT_OF_RESOURCES;
            }
            FreePool(Buf);
            Buf = NewBuf;
            BufSize = NewSize;
            continue;
        }
        if (EFI_ERROR(Status)) {
            break;
        }
        if (ReadSize == 0) {
            break;  // end of directory
        }

        EFI_FILE_INFO* Info = (EFI_FILE_INFO*)Buf;
        if (Info->Attribute & EFI_FILE_DIRECTORY) {
            continue;
        }
        if (BootStriCmp(Info->FileName, L"Mac OS ROM") == 0) {
            continue;  // handled by the ROM loader
        }
        if (BootStriCmp(Info->FileName, L".") == 0 || BootStriCmp(Info->FileName, L"..") == 0) {
            continue;
        }

        if (Count >= PPC_MAX_DRIVERS) {
            break;
        }
        PPC_SYSTEM_FILE* D = &g_BootContext.Drivers[Count];
        ZeroMem(D, sizeof(PPC_SYSTEM_FILE));
        D->Type = PPC_SYSTEM_FILE_TYPE_DRIVER;
        D->Loaded = FALSE;
        BootCopyString(D->Name, Info->FileName, PPC_SYSTEM_FILE_NAME_MAX);
        BootBuildPath(PPC_EXTENSIONS_DIR_PATH, Info->FileName, D->Path, PPC_SYSTEM_FILE_PATH_MAX);
        D->FileSize = Info->FileSize;
        Count++;
    }

    FreePool(Buf);
    Dir->Close(Dir);
    Root->Close(Root);

    g_BootContext.DriverCount = Count;
    Print(L"Extensions scanned: %d driver(s) found\n", Count);
    return EFI_SUCCESS;
}

// Enumerate the Extensions folder on the attached Mac OS disc (in-emulator
// HFS/HFS+ reader) and register every file as a driver. Mirrors
// BootEnumerateExtensions, which reads the FAT boot volume instead.
STATIC EFI_STATUS
BootEnumerateExtensionsHfs (
    VOID
    )
{
    PPC_HFS_ENTRY ExtDir;
    EFI_STATUS Status = PpcHfsOpenPath(PPC_HFS_SYSTEM_FOLDER_PATH L":Extensions", &ExtDir);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    if (!ExtDir.IsDirectory) {
        return EFI_NOT_FOUND;
    }

    PPC_HFS_ENTRY* Children = AllocatePool(PPC_MAX_DRIVERS * sizeof(PPC_HFS_ENTRY));
    if (Children == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    UINTN Count = PPC_MAX_DRIVERS;
    Status = PpcHfsListChildren(ExtDir.Id, Children, &Count);
    if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
        FreePool(Children);
        return Status;
    }

    UINTN Registered = 0;
    for (UINTN I = 0; I < Count; I++) {
        if (Children[I].IsDirectory) {
            continue;
        }
        if (BootStriCmp(Children[I].Name, L"Mac OS ROM") == 0) {
            continue;   // handled by the ROM loader
        }
        if (Registered >= PPC_MAX_DRIVERS) {
            break;
        }
        PPC_SYSTEM_FILE* D = &g_BootContext.Drivers[Registered];
        ZeroMem(D, sizeof(PPC_SYSTEM_FILE));
        D->Type = PPC_SYSTEM_FILE_TYPE_DRIVER;
        D->Loaded = FALSE;
        D->FileSize = Children[I].Size;
        D->HfsId = Children[I].Id;
        BootCopyString(D->Name, Children[I].Name, PPC_SYSTEM_FILE_NAME_MAX);
        BootCopyString(D->Path, PPC_HFS_SYSTEM_FOLDER_PATH L":Extensions:",
                       PPC_SYSTEM_FILE_PATH_MAX);
        UINTN Off = 0;
        while (Off + 1 < PPC_SYSTEM_FILE_PATH_MAX && D->Path[Off] != 0) { Off++; }
        for (UINTN K = 0; Off + 1 < PPC_SYSTEM_FILE_PATH_MAX && Children[I].Name[K] != 0; K++) {
            D->Path[Off++] = Children[I].Name[K];
        }
        D->Path[Off] = 0;
        Registered++;
    }

    FreePool(Children);
    g_BootContext.DriverCount = Registered;
    Print(L"Extensions scanned (HFS): %d driver(s) found\n", Registered);
    return EFI_SUCCESS;
}

// Extract the trailing file name component from a path.
STATIC VOID
BootExtractFileName (
    IN  CHAR16* Path,
    OUT CHAR16* Name,
    IN  UINTN   MaxChars
    )
{
    UINTN Len = 0;
    UINTN LastSep = 0;
    while (Path[Len] != 0) {
        if (Path[Len] == L'\\') {
            LastSep = Len + 1;
        }
        Len++;
    }
    BootCopyString(Name, Path + LastSep, MaxChars);
}

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
    BootInfo->SystemReady = g_BootContext.SystemReady;

    BootInfo->MemoryMap.RomInstalled = g_BootContext.RomLoaded;
    BootInfo->MemoryMap.RomBase = g_BootContext.RomAddress;
    BootInfo->MemoryMap.RomSize = g_BootContext.RomSize;
    BootInfo->MemoryMap.RomType = g_BootContext.RomType;
    BootInfo->MemoryMap.LowMemoryInstalled = g_BootContext.LowMemoryInstalled;
    BootInfo->MemoryMap.LowMemoryBase = g_BootContext.LowMemoryAddress;
    BootInfo->MemoryMap.LowMemorySize = g_BootContext.LowMemorySize;
    BootInfo->MemoryMap.Ready = g_BootContext.SystemReady;

    PpcGetSystemFolderInfo(&BootInfo->SystemFolder);
    
    return EFI_SUCCESS;
}

// Additional bootloader functions for PowerPC-specific boot requirements

// Distinguish the installed ROM image. New World "Mac OS ROM" files (Mac OS
// 8.5+) are CHRP-style and begin with the <CHRP-BOOT> marker; classic Old
// World PowerPC firmware dumps (System 7 through early Mac OS 8) do not, so
// any other non-empty image the user supplies is treated as Old World.
STATIC UINT32
BootIdentifyRomType (
    IN const UINT8* Rom,
    IN UINTN        Size
    )
{
    STATIC const UINT8 ChrpBoot[11] = { '<', 'C', 'H', 'R', 'P', '-',
                                        'B', 'O', 'O', 'T', '>' };
    if (Rom == NULL || Size == 0) {
        return PPC_ROM_TYPE_UNKNOWN;
    }
    if (Size >= sizeof(ChrpBoot) && CompareMem(Rom, ChrpBoot, sizeof(ChrpBoot)) == 0) {
        return PPC_ROM_TYPE_NEW_WORLD;
    }
    return PPC_ROM_TYPE_OLD_WORLD;
}

// ---------------------------------------------------------------------------
// New World "Mac OS ROM" image decode
// ---------------------------------------------------------------------------
// The New World "Mac OS ROM" file (Mac OS 8.5+) is a CHRP boot image. Its
// Forth text descriptor declares where the compressed payload lives:
//
//     h# 01BFC0 constant parcels-offset
//     h# 259C8C constant parcels-size
//
// The payload starts with the fourcc 'prcl' followed by a chain of parcels.
// Each parcel header is big-endian 32-bit words: next-offset, type, then
// type-specific data. The chain is walked starting at parcel offset 0x14. The
// parcel of type 'rom ' holds the actual firmware image as LZSS data, which
// expands to the flat 4 MB ROM window the CPU boots from. (Same layout as
// SheepShaver rom_patches.cpp DecodeROM/decode_parcels/decode_lzss.)

STATIC UINT32
BootReadBe32 (
    IN const UINT8* P
    )
{
    return ((UINT32)P[0] << 24) | ((UINT32)P[1] << 16) |
           ((UINT32)P[2] << 8)  | ((UINT32)P[3]);
}

// Read the hexadecimal constant that immediately precedes the token
// "constant <Name>" inside the CHRP descriptor text, e.g. the "01BFC0" in
// "h# 01BFC0 constant parcels-offset". Returns TRUE and sets *Value on match.
STATIC BOOLEAN
BootFindChrpHexConstant (
    IN const UINT8* Data,
    IN UINTN        Size,
    IN const CHAR8* Name,
    OUT UINT32*     Value
    )
{
    STATIC const CHAR8 Token[] = "constant ";
    UINTN NameLen = 0;
    UINTN I;

    if (Value == NULL) {
        return FALSE;
    }
    while (Name[NameLen] != 0) {
        NameLen++;
    }

    for (I = 0; I < Size; I++) {
        UINT8 Nibbles[16];
        UINTN Count = 0;
        UINTN J = I;
        UINT32 V = 0;

        if (Data[I] != Token[0]) {
            continue;
        }
        if (I + sizeof(Token) - 1 + NameLen > Size) {
            break;
        }
        if (CompareMem(Data + I, Token, sizeof(Token) - 1) != 0) {
            continue;
        }
        if (CompareMem(Data + I + sizeof(Token) - 1, Name, NameLen) != 0) {
            continue;
        }

        // Walk backwards from the 'c' of "constant": first over any
        // whitespace, then over the hex digits of the declared value
        // (e.g. the "01BFC0" in "h# 01BFC0 constant parcels-offset").
        while (J > 0) {
            UINT8 Sep = Data[J - 1];
            if (Sep == ' ' || Sep == '\t' || Sep == '\r' || Sep == '\n') {
                J--;
            } else {
                break;
            }
        }
        while (J > 0 && Count < 16) {
            UINT8 C = Data[J - 1];
            UINT8 D;
            if (C >= '0' && C <= '9')        { D = (UINT8)(C - '0'); }
            else if (C >= 'a' && C <= 'f')   { D = (UINT8)(C - 'a' + 10); }
            else if (C >= 'A' && C <= 'F')   { D = (UINT8)(C - 'A' + 10); }
            else                             { break; }
            Nibbles[Count++] = D;
            J--;
        }
        if (Count == 0) {
            continue;
        }
        // Nibbles were collected right-to-left; assemble forward.
        while (Count > 0) {
            V = (V << 4) | Nibbles[--Count];
        }
        *Value = V;
        return TRUE;
    }

    return FALSE;
}

// LZSS decompression (the algorithm used by Apple's compressed ROM parcels).
// The 4 KB dictionary is held at file scope: on the stack it would trigger an
// x86_64 __chkstk stack probe, which the freestanding image does not provide.
// The decoder is not reentrant in this single-threaded boot path.
static UINT8 g_LzssDict[0x1000];

STATIC VOID
BootLzssDecode (
    IN const UINT8* Src,
    IN UINTN        Size,
    OUT UINT8*      Dest
    )
{
    UINT8* Dict = g_LzssDict;
    INTN  RunMask = 0;
    INTN  Remaining = (INTN)Size;
    UINTN DictIdx = 0xFEE;

    SetMem(Dict, sizeof(g_LzssDict), 0);
    while (Remaining >= 0) {
        if (RunMask < 0x100) {
            if (--Remaining < 0) break;
            RunMask = *Src++ | 0xFF00;
        }
        if (RunMask & 1) {
            // Verbatim byte
            if (--Remaining < 0) break;
            UINT8 C = *Src++;
            Dict[DictIdx & 0xFFF] = C;
            *Dest++ = C;
            DictIdx = (DictIdx + 1) & 0xFFF;
        } else {
            // Copy run from the 4 KB dictionary
            if (--Remaining < 0) break;
            UINT8 Idx = *Src++;
            if (--Remaining < 0) break;
            UINT8 Cnt = *Src++;
            UINTN Start = (UINTN)(Idx | ((UINTN)(Cnt << 4) & 0xF00));
            UINTN N = (UINTN)(Cnt & 0x0F) + 3;
            while (N--) {
                UINT8 C = Dict[Start & 0xFFF];
                Dict[DictIdx & 0xFFF] = C;
                *Dest++ = C;
                Start = (Start + 1) & 0xFFF;
                DictIdx = (DictIdx + 1) & 0xFFF;
            }
        }
        RunMask >>= 1;
    }
}

// Walk the 'prcl' parcel chain and expand the 'rom ' parcel (LZSS) into the
// flat ROM image buffer Dest.
STATIC EFI_STATUS
BootDecodeParcels (
    IN const UINT8* Parcels,
    IN UINTN        ParcelsSize,
    OUT UINT8*      Dest
    )
{
    UINT32 Offset = 0x14;

    if (ParcelsSize < 0x14 + 16) {
        return EFI_LOAD_ERROR;
    }
    if (BootReadBe32(Parcels) != 0x7072636CU /* 'prcl' */) {
        return EFI_LOAD_ERROR;
    }

    while (Offset != 0) {
        UINT32 Next = 0;
        UINT32 Type = 0;
        UINT32 LzssOffset = 0;
        UINTN  ParcelBase = (UINTN)Offset;

        if (ParcelBase + 12 > ParcelsSize) {
            return EFI_LOAD_ERROR;
        }
        Next = BootReadBe32(Parcels + ParcelBase + 0);
        Type = BootReadBe32(Parcels + ParcelBase + 4);
        LzssOffset = BootReadBe32(Parcels + ParcelBase + 8);

        if (Type == 0x726F6D20U /* 'rom ' */) {
            UINT32 LzssSize;
            if (ParcelBase + LzssOffset > ParcelsSize || Next > ParcelsSize) {
                return EFI_LOAD_ERROR;
            }
            if (Next < ParcelBase + LzssOffset) {
                return EFI_LOAD_ERROR;
            }
            LzssSize = Next - (UINT32)(ParcelBase + LzssOffset);
            BootLzssDecode(Parcels + ParcelBase + LzssOffset, LzssSize, Dest);
            return EFI_SUCCESS;
        }

        if (Next == 0 || Next <= ParcelBase || Next > ParcelsSize) {
            return EFI_LOAD_ERROR;
        }
        Offset = Next;
    }

    return EFI_LOAD_ERROR;
}

// Try to expand a New World <CHRP-BOOT> ROM file into the flat 4 MB ROM
// window image. On success *Out holds a freshly allocated page-aligned buffer
// and *OutSize is the mapped size; the caller owns it and should free the
// original compressed file buffer. Returns FALSE if the image is not a CHRP
// file or cannot be decoded.
STATIC BOOLEAN
BootDecodeChrpRom (
    IN  const UINT8* Buffer,
    IN  UINTN        Size,
    OUT VOID**       Out,
    OUT UINTN*       OutSize
    )
{
    STATIC const UINT8 ChrpBoot[11] = { '<', 'C', 'H', 'R', 'P', '-',
                                        'B', 'O', 'O', 'T', '>' };
    UINT32 ParcelsOffset = 0;
    UINT32 ParcelsSize = 0;
    UINTN  Pages;
    UINT8* Rom;
    EFI_PHYSICAL_ADDRESS Base = 0;
    EFI_STATUS Status;

    if (Out == NULL || OutSize == NULL) {
        return FALSE;
    }
    if (Buffer == NULL || Size < sizeof(ChrpBoot)) {
        return FALSE;
    }
    if (CompareMem(Buffer, ChrpBoot, sizeof(ChrpBoot)) != 0) {
        return FALSE;
    }

    if (!BootFindChrpHexConstant(Buffer, Size, "parcels-offset", &ParcelsOffset)) {
        return FALSE;
    }
    if (!BootFindChrpHexConstant(Buffer, Size, "parcels-size", &ParcelsSize)) {
        return FALSE;
    }
    if (ParcelsOffset == 0 || (UINTN)ParcelsOffset + ParcelsSize > Size) {
        return FALSE;
    }
    if (BootReadBe32(Buffer + ParcelsOffset) != 0x7072636CU /* 'prcl' */) {
        return FALSE;
    }

    // The flat image is 4 MB; the compressor may emit a few bytes past the
    // window, so hold a small slack beyond the mapped size.
    Pages = (PPC_ROM_MAX_SIZE + 0x10000) / EFI_PAGE_SIZE;
    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate decoded ROM pages: %r\n", Status);
        return FALSE;
    }
    Rom = (UINT8*)(UINTN)Base;
    ZeroMem(Rom, (PPC_ROM_MAX_SIZE + 0x10000));

    Status = BootDecodeParcels(Buffer + ParcelsOffset, ParcelsSize, Rom);
    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        Print(L"Failed to decode New World ROM parcels: %r\n", Status);
        return FALSE;
    }

    Print(L"New World ROM decompressed: %d bytes of parcels to flat image\n",
          (UINT64)ParcelsSize);

    *Out = Rom;
    *OutSize = PPC_ROM_MAX_SIZE;
    return TRUE;
}

EFI_STATUS
PpcLoadSystemRom (
    IN  CHAR16* RomPath,
    OUT VOID**  RomBuffer,
    OUT UINT64* RomSize
    )
{
    VOID*  Buffer = NULL;
    UINTN  Size   = 0;
    EFI_STATUS Status;

    if (RomPath == NULL || RomBuffer == NULL || RomSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Loading system ROM from: %s\n", RomPath);

    Status = BootReadFileToPages(RomPath, PPC_ROM_MAX_SIZE, &Buffer, &Size);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    *RomBuffer = Buffer;
    *RomSize = (UINT64)Size;

    Print(L"System ROM loaded: %d bytes at 0x%x\n", Size, Buffer);

    return EFI_SUCCESS;
}

// Load the "Mac OS ROM" file from the attached Mac OS disc (in-emulator
// HFS/HFS+ reader) into a page-aligned buffer. The file ships inside a
// System Folder and, on install discs, inside an install-image System Folder
// (e.g. "Power Mac G4 Install:System Folder:Mac OS ROM"), so it is located by
// a whole-catalog search rather than a fixed path. Used as a fallback when
// the boot volume has no ROM file.
STATIC EFI_STATUS
BootLoadHfsRomToPages (
    OUT VOID**  Buffer,
    OUT UINTN*  Size
    )
{
    PPC_HFS_VOLUME_INFO HfsInfo;
    EFI_STATUS Status = PpcHfsGetVolumeInfo(&HfsInfo);
    if (EFI_ERROR(Status)) {
        Status = PpcHfsMount(NULL);
        if (EFI_ERROR(Status)) {
            return Status;
        }
        Status = PpcHfsGetVolumeInfo(&HfsInfo);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    PPC_HFS_ENTRY RomEntry;
    Status = PpcHfsFindMacOsRom(&RomEntry);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    if (RomEntry.IsDirectory || RomEntry.Size == 0) {
        return EFI_NOT_FOUND;
    }
    if (RomEntry.Size > PPC_ROM_MAX_SIZE) {
        Print(L"HFS Mac OS ROM too large: %d bytes\n", (UINT64)RomEntry.Size);
        return EFI_LOAD_ERROR;
    }

    UINTN FileSize = (UINTN)RomEntry.Size;
    UINTN Pages = (FileSize + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS Base = 0;
    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    UINTN Got = FileSize;
    Status = PpcHfsReadFile(&RomEntry, (VOID*)(UINTN)Base, &Got);
    if (EFI_ERROR(Status) || Got != FileSize) {
        BS->FreePages(Base, Pages);
        return EFI_ERROR(Status) ? Status : EFI_LOAD_ERROR;
    }

    *Buffer = (VOID*)(UINTN)Base;
    *Size = FileSize;
    Print(L"System ROM loaded from HFS volume '%s': %d bytes\n",
          HfsInfo.VolumeName, (UINT64)FileSize);
    return EFI_SUCCESS;
}

EFI_STATUS
PpcInstallSystemRom (
    IN  CHAR16* RomPath,
    OUT UINT64* RomAddress,
    OUT UINT64* RomSize
    )
{
    VOID*  Buffer = NULL;
    UINT64 Size   = 0;
    EFI_STATUS Status;

    if (g_BootContext.RomLoaded) {
        if (RomAddress != NULL) { *RomAddress = g_BootContext.RomAddress; }
        if (RomSize != NULL) { *RomSize = g_BootContext.RomSize; }
        return EFI_ALREADY_STARTED;
    }

    Status = PpcLoadSystemRom(RomPath, &Buffer, &Size);
    if (EFI_ERROR(Status)) {
        // No ROM file on the boot volume: try the "Mac OS ROM" file on the
        // attached Mac OS disc through the in-emulator HFS reader.
        if (Status == EFI_NOT_FOUND) {
            VOID*  HfsRom = NULL;
            UINTN  HfsRomSize = 0;
            Status = BootLoadHfsRomToPages(&HfsRom, &HfsRomSize);
            if (!EFI_ERROR(Status)) {
                Buffer = HfsRom;
                Size = HfsRomSize;
            }
        }
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    // Identify the ROM type from the file as loaded: a New World "Mac OS ROM"
    // file is a compressed CHRP image, so its type must be captured before the
    // parcels are expanded (the flat image no longer starts with <CHRP-BOOT>).
    g_BootContext.RomType = BootIdentifyRomType((const UINT8*)Buffer, (UINTN)Size);
    g_BootContext.RomDecoded = FALSE;

    // New World "Mac OS ROM" files are compressed CHRP images (LZSS parcels).
    // Expand them into the flat 4 MB ROM window so the reset/boot region
    // contains real firmware code instead of descriptor text.
    {
        VOID*  Decoded = NULL;
        UINTN  DecodedSize = 0;
        if (BootDecodeChrpRom((const UINT8*)Buffer, (UINTN)Size, &Decoded, &DecodedSize)) {
            PpcFreeMemory(Buffer, Size);
            Buffer = Decoded;
            Size = DecodedSize;
            g_BootContext.RomDecoded = TRUE;
        }
    }

    // Map the ROM into the guest memory map. New World images boot from ROM
    // base + 0x310000, which overflows the 32-bit space at PPC_ROM_GUEST_BASE,
    // so they are mapped at the lower SheepShaver base. The region must be
    // writable: the nanokernel is told this 4 MB window is a RAM bank (the
    // SheepShaver memory model), and builds its HTAB, kernel data page (KDP),
    // EWA, stack and IRP in the top of it (HTABORG 0x40BF0000, KDP 0x40BEE000).
    // A read-only mapping silently drops those stores and the NK panics.
    UINT32 GuestBase = (g_BootContext.RomType == PPC_ROM_TYPE_NEW_WORLD)
                           ? PPC_NEW_WORLD_ROM_GUEST_BASE
                           : PPC_ROM_GUEST_BASE;
    Status = PpcAddGuestMemoryRegion(Buffer, GuestBase, (UINT32)Size,
                                     g_BootContext.RomType != PPC_ROM_TYPE_NEW_WORLD);
    if (EFI_ERROR(Status)) {
        PpcFreeMemory(Buffer, Size);
        Print(L"Failed to map ROM into guest memory: %r\n", Status);
        return Status;
    }

    g_BootContext.RomLoaded = TRUE;
    g_BootContext.RomAddress = GuestBase;
    g_BootContext.RomSize = Size;
    g_BootContext.RomHostBuffer = Buffer;

    if (RomAddress != NULL) { *RomAddress = GuestBase; }
    if (RomSize != NULL) { *RomSize = Size; }

    Print(L"System ROM installed: %d bytes at guest 0x%x (%s)\n",
          (UINT64)Size, GuestBase,
          g_BootContext.RomType == PPC_ROM_TYPE_NEW_WORLD ? L"New World" :
          g_BootContext.RomType == PPC_ROM_TYPE_OLD_WORLD ? L"Old World" :
          L"unknown type");

    return EFI_SUCCESS;
}

EFI_STATUS
PpcInstallDemoRom (
    OUT UINT64* RomAddress,
    OUT UINT64* RomSize
    )
{
    // lis r3, 0xFFF0     ; r3 = 0xFFF00000
    // lwz r4, 0(r3)      ; r4 = ROM[0] = 'ROM1'
    // addi r5, r4, 1     ; r5 = 'ROM1' + 1
    // stw r5, 0(r1)      ; store to guest RAM via r1
    STATIC const UINT32 DemoProgram[4] = {
        0x3C60FFF0,
        0x80830000,
        0x38A40001,
        0x90A10000
    };

    UINTN Pages;
    EFI_PHYSICAL_ADDRESS Base = 0;
    EFI_STATUS Status;
    UINT8* Rom;
    UINTN I;

    if (g_BootContext.RomLoaded) {
        if (RomAddress != NULL) { *RomAddress = g_BootContext.RomAddress; }
        if (RomSize != NULL) { *RomSize = g_BootContext.RomSize; }
        return EFI_ALREADY_STARTED;
    }

    Pages = PPC_ROM_MAX_SIZE / EFI_PAGE_SIZE;
    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate demo ROM pages: %r\n", Status);
        return Status;
    }
    Rom = (UINT8*)(UINTN)Base;
    ZeroMem(Rom, PPC_ROM_MAX_SIZE);

    // Magic word at the ROM base: 'R' 'O' 'M' '1'.
    Rom[0] = 'R';
    Rom[1] = 'O';
    Rom[2] = 'M';
    Rom[3] = '1';

    // Reset-vector program, stored big-endian (guest byte order).
    UINT8* Prog = Rom + (PPC_RESET_VECTOR - PPC_ROM_GUEST_BASE);
    for (I = 0; I < 4; I++) {
        UINT32 W = DemoProgram[I];
        Prog[I * 4 + 0] = (UINT8)(W >> 24);
        Prog[I * 4 + 1] = (UINT8)(W >> 16);
        Prog[I * 4 + 2] = (UINT8)(W >> 8);
        Prog[I * 4 + 3] = (UINT8)W;
    }

    Status = PpcAddGuestMemoryRegion(Rom, PPC_ROM_GUEST_BASE, PPC_ROM_MAX_SIZE, TRUE);
    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        Print(L"Failed to map demo ROM into guest memory: %r\n", Status);
        return Status;
    }

    g_BootContext.RomLoaded = TRUE;
    g_BootContext.RomAddress = PPC_ROM_GUEST_BASE;
    g_BootContext.RomSize = PPC_ROM_MAX_SIZE;
    g_BootContext.RomHostBuffer = Rom;
    g_BootContext.RomType = PPC_ROM_TYPE_DEMO;

    if (RomAddress != NULL) { *RomAddress = PPC_ROM_GUEST_BASE; }
    if (RomSize != NULL) { *RomSize = PPC_ROM_MAX_SIZE; }

    Print(L"Demo system ROM installed: %d bytes at guest 0x%x\n",
          (UINT64)PPC_ROM_MAX_SIZE, PPC_ROM_GUEST_BASE);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcInstallLowMemory (
    OUT UINT64* LowMemAddress,
    OUT UINT64* LowMemSize
    )
{
    // Classic Mac OS low-memory globals live at physical address 0. Guest RAM
    // is mapped at guest 0x10000000, so low memory is a dedicated read/write
    // region below the kernel base.
    UINTN Pages;
    EFI_PHYSICAL_ADDRESS Base = 0;
    EFI_STATUS Status;

    if (g_BootContext.LowMemoryInstalled) {
        if (LowMemAddress != NULL) { *LowMemAddress = g_BootContext.LowMemoryAddress; }
        if (LowMemSize != NULL) { *LowMemSize = g_BootContext.LowMemorySize; }
        return EFI_ALREADY_STARTED;
    }

    Pages = PPC_LOW_MEM_SIZE / EFI_PAGE_SIZE;
    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate low-memory pages: %r\n", Status);
        return Status;
    }
    ZeroMem((VOID*)(UINTN)Base, PPC_LOW_MEM_SIZE);

    Status = PpcAddGuestMemoryRegion((VOID*)(UINTN)Base,
                                     PPC_LOW_MEM_GUEST_BASE,
                                     PPC_LOW_MEM_SIZE,
                                     FALSE);
    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        Print(L"Failed to map low memory into guest memory: %r\n", Status);
        return Status;
    }

    g_BootContext.LowMemoryInstalled = TRUE;
    g_BootContext.LowMemoryAddress = PPC_LOW_MEM_GUEST_BASE;
    g_BootContext.LowMemorySize = PPC_LOW_MEM_SIZE;

    if (LowMemAddress != NULL) { *LowMemAddress = PPC_LOW_MEM_GUEST_BASE; }
    if (LowMemSize != NULL) { *LowMemSize = PPC_LOW_MEM_SIZE; }

    Print(L"Low-memory region installed: %d bytes at guest 0x%x\n",
          (UINT64)PPC_LOW_MEM_SIZE, PPC_LOW_MEM_GUEST_BASE);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcInstallNkSystemArea (
    VOID
    )
{
    UINTN Pages;
    EFI_PHYSICAL_ADDRESS Base = 0;
    EFI_STATUS Status;

    if (g_BootContext.NkSystemAreaInstalled) {
        return EFI_ALREADY_STARTED;
    }

    Pages = PPC_NK_SYSTEM_AREA_SIZE / EFI_PAGE_SIZE;
    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate nanokernel system-area pages: %r\n", Status);
        return Status;
    }
    ZeroMem((VOID*)(UINTN)Base, PPC_NK_SYSTEM_AREA_SIZE);

    Status = PpcAddGuestMemoryRegion((VOID*)(UINTN)Base,
                                     PPC_NK_SYSTEM_AREA_GUEST_BASE,
                                     PPC_NK_SYSTEM_AREA_SIZE,
                                     FALSE);
    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        Print(L"Failed to map nanokernel system area into guest memory: %r\n", Status);
        return Status;
    }

    g_BootContext.NkSystemAreaInstalled = TRUE;

    Print(L"Nanokernel system area installed: %d bytes at guest 0x%x\n",
          (UINT64)PPC_NK_SYSTEM_AREA_SIZE, PPC_NK_SYSTEM_AREA_GUEST_BASE);

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

// Write a big-endian 32-bit word into the ROM host buffer (the same backing
// the interpreter's CpuRead16 reads, so the patch is immediately visible).
STATIC VOID
RomPatchWriteWord32 (
    IN UINT8*  Rom,
    IN UINT32  Offset,
    IN UINT32  Value
    )
{
    Rom[Offset + 0] = (UINT8)(Value >> 24);
    Rom[Offset + 1] = (UINT8)(Value >> 16);
    Rom[Offset + 2] = (UINT8)(Value >> 8);
    Rom[Offset + 3] = (UINT8)Value;
}

// Install one 27-word 68K emulator-entry routine (SheepShaver's
// emulator-start/MixedMode/Reset/FC1E/FE0A/FE0F fragments). The routines are
// identical except for the `lwz r10,<offset>(r1)` word that picks the
// NanoKernelCallTable slot to blr to once the 68K context is saved, and the
// final word: the emulator-start fragment (trap 0, ROM + 0x36f900) branches
// to the injected 68K DR-emulator entry (RomWriteEmulatorEntryRoutine), the
// others keep the plain `blr`.
STATIC VOID
RomWriteEmulStartRoutine (
    IN UINT8*  Rom,
    IN UINT32  Offset,
    IN UINT32  LwzR10,
    IN UINT32  FinalBranch
    )
{
    static const UINT32 Common[27] = {
        0x7c2903a6,  // mtctr r1
        0x80202818,  // lwz   r1,0x2818(0)   XLM_IRQ_NEST
        0x38210001,  // addi  r1,r1,1
        0x90202818,  // stw   r1,0x2818(0)   XLM_IRQ_NEST
        0x80202804,  // lwz   r1,0x2804(0)   XLM_KERNEL_DATA
        0x90c10018,  // stw   r6,0x18(r1)
        0x7cc902a6,  // mfctr r6
        0x90c10004,  // stw   r6,0x04(r1)
        0x80c1065c,  // lwz   r6,0x65c(r1)   KDP.ECB
        0x90e6013c,  // stw   r7,0x13c(r6)
        0x91060144,  // stw   r8,0x144(r6)
        0x9126014c,  // stw   r9,0x14c(r6)
        0x91460154,  // stw   r10,0x154(r6)
        0x9166015c,  // stw   r11,0x15c(r6)
        0x91860164,  // stw   r12,0x164(r6)
        0x91a6016c,  // stw   r13,0x16c(r6)
        0x7da00026,  // mfcr  r13
        0x80e10660,  // lwz   r7,0x660(r1)   KDP.flags
        0x7d8802a6,  // mflr  r12
        0x50e74001,  // rlwimi. r7,r7,8,0x80000000
        0x00000000,  // lwz   r10,<offset>(r1)  NanoKernelCallTable entry (variant)
        0x7d4803a6,  // mtlr  r10
        0x7d8a6378,  // mr    r10,r12
        0x3d600002,  // lis   r11,0x0002
        0x616bf072,  // ori   r11,r11,0xf072   MSR bits
        0x50e7deb4,  // rlwimi r7,r7,27,0x00000020
        0x4e800020   // blr
    };
    UINT32 I;
    for (I = 0; I < 27; I++) {
        UINT32 Word = (I == 20) ? LwzR10 : (I == 26) ? FinalBranch : Common[I];
        RomPatchWriteWord32(Rom, Offset + I * 4, Word);
    }
}

// Install the injected 68K DR-emulator entry (SheepShaver's execute_68k
// contract) at ROM + 0x36f700. It builds the full 68K context -- d0..d7 =
// r8..r15, a0..a6 = r16..r22, a7 = r1 = 0x2600, r23 = 0, r24 = 68K PC
// (0x4080002a), r25 = SR MSB (0x27), r26 = 0, r28 = 0 (VBR), r29 = opcode
// table (0x40b80000), r30 = emulator base (0x40b60000), r31 = KDP + 0x1000,
// XER = 0, cr1 = 0 (the glue's `bgtctr cr1` must not fire), cr2 = SO -- then
// performs the first dispatch exactly like SheepShaver's loop: fetch the
// opcode at [r24] (unsigned), index the opcode table (rlwimi), prefetch the
// next word with lhau (r24 = PC + 2), and bctr straight into the dispatch
// entry. The DR loop at 0x40b66080 is NOT used: its double-lhau leaves r24 =
// PC + 4, which breaks PC-relative handlers (e.g. jmp 0x4efa computes
// extension-word address + displacement and would land two bytes past).
STATIC VOID
RomWriteEmulatorEntryRoutine (
    IN UINT8*  Rom,
    IN UINT32  Offset
    )
{
    static const UINT32 Words[37] = {
        0x7c3f0b78,  // mr r31,r1
        0x3bff1000,  // addi r31,r31,0x1000     r31 = KDP + 0x1000 = ed
        0x3fa040b8,  // lis r29,0x40b8          r29 = opcode dispatch table
        0x3fc040b6,  // lis r30,0x40b6          r30 = emulator base
        0x3f004080,  // lis r24,0x4080          r24 = 68K PC
        0x6318002a,  // ori r24,r24,0x2a        = 0x4080002a
        0x38000000,  // li r0,0
        0x7c0103a6,  // mtxer r0                XER = 0
        0x60102600,  // ori r1,r0,0x2600        a7 = 0x2600
        0x39000000,  // li r8,0                 d0..d7
        0x39200000,  // li r9,0
        0x39400000,  // li r10,0
        0x39600000,  // li r11,0
        0x39800000,  // li r12,0
        0x39a00000,  // li r13,0
        0x39c00000,  // li r14,0
        0x39e00000,  // li r15,0
        0x3a000000,  // li r16,0               a0..a6
        0x3a200000,  // li r17,0
        0x3a400000,  // li r18,0
        0x3a600000,  // li r19,0
        0x3a800000,  // li r20,0
        0x3aa00000,  // li r21,0
        0x3ac00000,  // li r22,0
        0x3ae00000,  // li r23,0
        0x3b200027,  // li r25,0x27            SR = 0x27 (MSB)
        0x3b400000,  // li r26,0
        0x3b800000,  // li r28,0              VBR = 0
        0x3ce00200,  // lis r7,0x0200
        0x7ce04120,  // mtcrf 0x04,r7         cr2 = SO (SheepShaver entry)
        0x38e00000,  // li r7,0
        0x7ce02120,  // mtcrf 0x02,r7         cr1 = 0 (glue bgtctr guard)
        0xa3780000,  // lhz r27,0(r24)        r27 = opcode (unsigned)
        0x537d1b78,  // rlwimi r29,r27,3,0xd,0x1c  r29 = table + opcode*8
        0xaf780002,  // lhau r27,2(r24)       r27 = next word, r24 = PC + 2
        0x7fa903a6,  // mtctr r29
        0x4e800420   // bctr                  -> 0x40ba77d0 (jmp dispatch)
    };
    UINT32 I;
    for (I = 0; I < sizeof(Words) / sizeof(Words[0]); I++) {
        RomPatchWriteWord32(Rom, Offset + I * 4, Words[I]);
    }
}

// Install the ed.v[0x814] dispatch helper at ROM + 0x36f7c0. The DR emulator's
// state machine (0x40b6d114) calls it through the 68K-mode glue via blrl
// (CTR = dispatch entry, LR = return into the state machine). It sets cr2.GE
// so the glue's `bgelr cr2` returns into the state machine, then bctr's to the
// dispatch entry.
STATIC VOID
RomWriteEmulatorDispatchHelper (
    IN UINT8*  Rom,
    IN UINT32  Offset
    )
{
    static const UINT32 Words[3] = {
        0x3c000060,  // lis r0,0x0060         cr2.GT|EQ
        0x7c004120,  // mtcrf 0x04,r0         cr2 = GE
        0x4e800420   // bctr
    };
    UINT32 I;
    for (I = 0; I < sizeof(Words) / sizeof(Words[0]); I++) {
        RomPatchWriteWord32(Rom, Offset + I * 4, Words[I]);
    }
}

// SheepShaver-faithful activation of the New World ROM's built-in 68K DR
// emulator. The ROM's ConfigInfo (ROM + 0x30d000) bakes LA_EmulatorCode =
// 0x68060000 / LA_DispatchTable = 0x68080000 (logical RAM addresses the real
// hardware maps to the emulator image); this redirects them into the ROM
// window (ROM + 0x360000 / ROM + 0x380000) so the nanokernel's boot tail
// executes the emulator in place. The `twui r31,n` kernel-trap table (ROM +
// 0x36e8c0) is then rewritten into absolute branches to the emulator-entry
// routines, and the EMUL_OP dispatch markers are installed in the opcode
// table. Must run after PpcInstallLowMemory (writes XLM globals at 0x2800).
STATIC EFI_STATUS
PpcPatchNewWorldRom (
    VOID
    )
{
    UINT8* Rom     = (UINT8*)g_BootContext.RomHostBuffer;
    UINT32 RomBase = (UINT32)g_BootContext.RomAddress;
    UINT32 Struct  = PPC_NEW_WORLD_ROM_BOOT_STRUCT_OFFSET;
    UINT32 TrapBase = 0;
    UINT32 I;

    if (Rom == NULL ||
        g_BootContext.RomSize < PPC_NEW_WORLD_ROM_EMUL_OP_END_OFFSET + 8) {
        Print(L"ROM patch skipped: ROM not loaded or too small\n");
        return EFI_UNSUPPORTED;
    }

    // ConfigInfo LA fields: keep LA_KernelData/LA_EmulatorData (the 0x68ffxxxx
    // system-area backing), repoint the emulator image at the in-place ROM
    // copy, clear the physical RAM base (baked 0xffffffff), and set the 68K
    // reset vector to the ROM entry (ROM + 0x2a).
    RomPatchWriteWord32(Rom, Struct + 0x9C, 0x68FFE000);  // LA_InfoRecord
    RomPatchWriteWord32(Rom, Struct + 0xA0, 0x68FFE000);  // LA_KernelData
    RomPatchWriteWord32(Rom, Struct + 0xA4, 0x68FFF000);  // LA_EmulatorData
    RomPatchWriteWord32(Rom, Struct + 0xA8,
                        RomBase + PPC_NEW_WORLD_ROM_DISPATCH_TABLE_OFFSET);
    RomPatchWriteWord32(Rom, Struct + 0xAC,
                        RomBase + PPC_NEW_WORLD_ROM_LA_EMULCODE_BASE - PPC_NEW_WORLD_ROM_GUEST_BASE);
    RomPatchWriteWord32(Rom, Struct + 0x360, 0x00000000); // physical RAM base
    RomPatchWriteWord32(Rom, Struct + 0xFD8, RomBase + 0x2A); // 68K reset vector

    // Locate the `twui r31,0..2` kernel-trap table (SheepShaver's
    // find_rom_data range; verified at ROM + 0x36e8c0 in the standard image).
    {
        static const UINT8 TwiPattern[12] =
            {0x0F,0xFF,0x00,0x00, 0x0F,0xFF,0x00,0x01, 0x0F,0xFF,0x00,0x02};
        UINT32 Off;
        for (Off = 0x36E600; Off + sizeof(TwiPattern) <= 0x36EA00; Off += 4) {
            BOOLEAN Match = TRUE;
            UINTN B;
            for (B = 0; B < sizeof(TwiPattern); B++) {
                if (Rom[Off + B] != TwiPattern[B]) { Match = FALSE; break; }
            }
            if (Match) { TrapBase = Off; break; }
        }
        if (TrapBase == 0) {
            Print(L"ROM patch failed: twi trap-table pattern not found\n");
            return EFI_NOT_FOUND;
        }
    }

    // Rewrite the 16-word trap table as branches to the entry routines:
    // trap 0 -> emulator start, 1 -> Mixed Mode, 2 -> Reset/FC1E, 3 -> FE0A,
    // 4 -> (interrupt, ILLEGAL), 5 -> FE0F, 6..15 -> ILLEGAL.
    {
        static const UINT32 TrapEntries[16] = {
            0x36F900, 0x36FA00, 0x36FB00, 0x36FC00,
            0x00000000, 0x36FD00, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000
        };
        for (I = 0; I < 16; I++) {
            UINT32 Target = TrapEntries[I];
            UINT32 Word = (Target == 0)
                              ? 0x00000000
                              : (0x48000000 + (Target - (TrapBase + I * 4)));
            RomPatchWriteWord32(Rom, TrapBase + I * 4, Word);
        }
    }

    // Install the five 27-word entry routines. The emulator-start fragment
    // (trap 0) branches to the injected 68K DR-emulator entry instead of the
    // plain `blr` so the boot-tail handoff starts the DR emulator directly.
    RomWriteEmulStartRoutine(Rom, 0x36F900, 0x814105F0, 0x4BFFFD98);
    RomWriteEmulStartRoutine(Rom, 0x36FA00, 0x814105F4, 0x4E800020);
    RomWriteEmulStartRoutine(Rom, 0x36FB00, 0x814105F8, 0x4E800020);
    RomWriteEmulStartRoutine(Rom, 0x36FC00, 0x814105FC, 0x4E800020);
    RomWriteEmulStartRoutine(Rom, 0x36FD00, 0x81410604, 0x4E800020);

    // The 68K DR-emulator entry + ed.v[0x814] dispatch helper (free NOP region
    // at ROM + 0x36f700..0x36f8fc).
    RomWriteEmulatorEntryRoutine(Rom, 0x36F700);
    RomWriteEmulatorDispatchHelper(Rom, 0x36F7C0);

    // The ROM's control-flow dispatch glue bakes `rlwimi r29,r24,0x14,0xb,0xb`
    // (0x531DA2D6) into every branch/jmp path: it copies the low bit of the
    // 68K PC into bit 20 (0x100000) of the dispatch address. For valid 68K
    // code (always word-aligned) that bit is 0, which zeroes bit 20 of the
    // opcode-table base. The dispatch table lives at 0x40b80000 (bit 20 = 1),
    // so every control-flow re-dispatch would land 0x100000 off (in the "kckc"
    // data region) and the boot walks garbage. On real hardware the table base
    // has bit 20 = 0 and the rlwimi is a harmless no-op; with our base it must
    // be neutralised. NOP every occurrence in the emulator image.
    {
        UINT32 Count = 0;
        for (I = 0x360000; I + 4 <= 0x380000; I += 4) {
            if (Rom[I] == 0x53 && Rom[I + 1] == 0x1D &&
                Rom[I + 2] == 0xA2 && Rom[I + 3] == 0xD6) {
                RomPatchWriteWord32(Rom, I, 0x60000000);
                Count++;
            }
        }
        Print(L"68K emulator: neutralised %u rlwimi dispatch-bit-20 words\n", Count);
    }

    // Overwrite the opcode-table slots for the EMUL_OP extended opcodes
    // (0xFE40..0xFE40+OP_MAX+2) with POWERPC_EMUL_OP markers ("addi r0,r0,n")
    // followed by `b 0x366084` (re-enter the DR emulator loop). The
    // interpreter intercepts the markers (PpcEmulatorDispatchOp).
    {
        UINT32 Entry = PPC_NEW_WORLD_ROM_EMUL_OP_ENTRY_OFFSET;
        RomPatchWriteWord32(Rom, Entry +  0, PPC_EMUL_OP_MARKER | 0);   // EMUL_RETURN
        RomPatchWriteWord32(Rom, Entry +  4, 0x4BF66E80);
        RomPatchWriteWord32(Rom, Entry +  8, PPC_EMUL_OP_MARKER | 1);   // EXEC_RETURN
        RomPatchWriteWord32(Rom, Entry + 12, 0x4BF66E78);
        RomPatchWriteWord32(Rom, Entry + 16, PPC_EMUL_OP_MARKER | 2);   // EXEC_NATIVE
        RomPatchWriteWord32(Rom, Entry + 20, 0x4BF66E70);
        for (I = 0; I < PPC_OP_MAX; I++) {
            RomPatchWriteWord32(Rom, Entry + 24 + I * 8,
                                PPC_EMUL_OP_MARKER | (I + 3));
            RomPatchWriteWord32(Rom, Entry + 28 + I * 8, 0x4BF66E68 - I * 8);
        }
    }

    // XLM ("eXtra Low Memory") globals the entry routines read; they sit above
    // the 0x0-0x1800 low-memory area the nanokernel zeroes during its boot.
    BootWriteWord32(PPC_XLM_SIGNATURE_OFFSET,   0x42616168);       // 'Baah'
    BootWriteWord32(PPC_XLM_KERNEL_DATA_OFFSET, 0x0000A000);       // NK KDP
    BootWriteWord32(PPC_XLM_TOC_OFFSET,         0x00000000);
    BootWriteWord32(PPC_XLM_SHEEP_OBJ_OFFSET,   0x00000000);
    BootWriteWord32(PPC_XLM_RUN_MODE_OFFSET,    0x00000000);       // MODE_68K
    BootWriteWord32(PPC_XLM_68K_R25_OFFSET,     0x00000000);
    BootWriteWord32(PPC_XLM_IRQ_NEST_OFFSET,    0x00000000);
    BootWriteWord32(PPC_XLM_PVR_OFFSET,         0x00000000);
    BootWriteWord32(PPC_XLM_BUS_CLOCK_OFFSET,   50000000);

    Print(L"68K emulator patched: LA_EmulatorCode 0x%08x LA_DispatchTable 0x%08x "
          L"trap table at ROM+0x%x -> emulator start 0x%08x\n",
          RomBase + 0x360000, RomBase + 0x380000, TrapBase,
          RomBase + 0x36F900);
    return EFI_SUCCESS;
}

EFI_STATUS
PpcPrepareSystemForBoot (
    VOID
    )
{
    UINT64 RamBase = 0;
    UINT64 RamSize = 0;
    UINTN  I;
    EFI_STATUS Status;

    Print(L"Preparing system for boot\n");

    // The guest memory map must be ready before the CPU can start.
    if (!g_BootContext.LowMemoryInstalled) {
        PpcInstallLowMemory(NULL, NULL);
    }
    // The nanokernel keeps its kernel stack/heap in the 0x68000000 system area
    // (fixed 0x68F0xxxx logical addresses); back it so those accesses land in
    // RAM instead of reading as zero.
    PpcInstallNkSystemArea();
    if (!g_BootContext.RomLoaded) {
        Print(L"Warning: no system ROM installed; boot would fail at the reset vector\n");
    } else if (g_BootContext.RomType == PPC_ROM_TYPE_NEW_WORLD) {
        // Patch the ROM's built-in 68K emulator in place (SheepShaver
        // style): the ROM window is writable, so the ConfigInfo LA fields,
        // the twi kernel-trap table, the emulator-entry routines and the
        // EMUL_OP dispatch markers are all installed directly in the ROM.
        Status = PpcPatchNewWorldRom();
        if (EFI_ERROR(Status)) {
            Print(L"Warning: 68K emulator ROM patch failed: %r\n", Status);
        }
    }

    Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);
    if (EFI_ERROR(Status) || RamSize == 0) {
        Print(L"System preparation failed: guest RAM unavailable\n");
        return EFI_NOT_READY;
    }

    // Reset the CPU to the classic Mac OS boot state: PC at the ROM reset
    // vector with machine-check and recoverable-interrupt handling enabled.
    ZeroMem(&g_PpcContext, sizeof(g_PpcContext));
    for (I = 0; I < 32; I++) {
        g_PpcContext.Gpr[I] = 0;
    }
    g_PpcContext.Msr = PPC_MSR_ME | PPC_MSR_RI;
    g_PpcContext.Pc = PPC_RESET_VECTOR;
    g_PpcContext.Srr0 = PPC_RESET_VECTOR;
    g_PpcContext.Srr1 = g_PpcContext.Msr;
    g_PpcContext.ExceptionPending = 0;

    // Write the emulator boot info block into low memory: magic, then
    // RAM base, RAM size, ROM base, installed ROM size, and ROM type
    // (big-endian).
    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET, 0x45464921);
    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 0, (UINT32)RamBase);
    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 4, (UINT32)RamSize);
    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 8,
                    (UINT32)g_BootContext.RomAddress);
    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 12,
                    (UINT32)(g_BootContext.RomLoaded ? g_BootContext.RomSize : 0));
    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET + 16,
                    g_BootContext.RomType);

    g_BootContext.SystemReady = TRUE;
    g_BootContext.SystemBooting = TRUE;

    Print(L"System prepared: PC=0x%x MSR=0x%08x SRR0=0x%x SRR1=0x%x\n",
          g_PpcContext.Pc, g_PpcContext.Msr, g_PpcContext.Srr0, g_PpcContext.Srr1);
    Print(L"Boot info block written to low memory at 0x%x\n",
          PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcRunBootSelfTest (
    VOID
    )
{
    UINT64 RamBase = 0;
    UINT64 RamSize = 0;
    UINTN  Executed = 0;
    EFI_STATUS Status;
    UINT32 MagicPlusOne;

    g_BootTestPasses = 0;
    g_BootTestFailures = 0;

    Print(L"--- Boot Memory Map / System Init Self-Test ---\n");

    Status = PpcGetGuestMemoryRegion(NULL, &RamBase, &RamSize);
    BootSelfTestCheck(Status == EFI_SUCCESS && RamSize > 0,
                      L"guest RAM region available");

    // Low-memory globals: writable and readable.
    PpcWriteGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08, 0xAA);
    BootSelfTestCheck(
        PpcReadGuestByte(PPC_LOW_MEM_GUEST_BASE + 0x08) == 0xAA,
        L"low-memory globals read/write (guest 0x00000000)");

    // ROM mapping: readable through the interpreter's memory path, and
    // read-only to guest stores. A real firmware dump must not be executed,
    // so the reset-vector execution checks only run against the demo ROM.
    UINT32 RomBase = (UINT32)g_BootContext.RomAddress;
    if (g_BootContext.RomType == PPC_ROM_TYPE_DEMO) {
        // Demo ROM: magic word readable at the ROM base.
        BootSelfTestCheck(
            PpcReadGuestByte(RomBase + 0) == 'R' &&
            PpcReadGuestByte(RomBase + 1) == 'O' &&
            PpcReadGuestByte(RomBase + 2) == 'M' &&
            PpcReadGuestByte(RomBase + 3) == '1',
            L"ROM magic word 'ROM1' readable at the ROM base");
    } else {
        // Real ROM: region present and its base is readable (no fixed magic).
        BootSelfTestCheck(
            g_BootContext.RomLoaded && g_BootContext.RomSize > 0,
            L"system ROM region present in guest memory");
        if (g_BootContext.RomType == PPC_ROM_TYPE_NEW_WORLD) {
            if (g_BootContext.RomDecoded) {
                // The flat image no longer carries the <CHRP-BOOT> text header;
                // verify the nanokernel boot entry (ROM base + 0x310000, the
                // SheepShaver entry point) contains executable code instead.
                UINT32 EntryWord = 0;
                for (UINTN I = 0; I < 4; I++) {
                    EntryWord = (EntryWord << 8) |
                                PpcReadGuestByte(RomBase +
                                                 PPC_NANOKERNEL_BOOT_OFFSET + (UINT32)I);
                }
                BootSelfTestCheck(EntryWord != 0, L"New World ROM nanokernel boot entry present");
            } else {
                static const UINT8 ChrpSig[12] = { '<', 'C', 'H', 'R', 'P', '-',
                                                   'B', 'O', 'O', 'T', '>', '\r' };
                BOOLEAN Chrp = TRUE;
                for (UINTN I = 0; I < 12; I++) {
                    if (PpcReadGuestByte(RomBase + (UINT32)I) != ChrpSig[I]) {
                        Chrp = FALSE;
                        break;
                    }
                }
                BootSelfTestCheck(Chrp, L"New World ROM '<CHRP-BOOT>' signature present");
            }
        }
    }

    // ROM is read-only to guest stores (applies to demo and real ROMs).
    {
        UINT8 B0 = PpcReadGuestByte(RomBase + 0);
        PpcWriteGuestByte(RomBase + 0, (UINT8)(B0 ^ 0xFF));
        BootSelfTestCheck(
            PpcReadGuestByte(RomBase + 0) == B0,
            L"ROM rejects guest writes (read-only)");
    }

    // Cross-region execution (demo ROM only): run the reset-vector program in
    // the ROM; it loads the ROM magic word and stores its successor into
    // guest RAM.
    if (g_BootContext.RomType == PPC_ROM_TYPE_DEMO) {
        PpcSetGprValue(1, (UINT32)RamBase);
        PpcSetGprValue(3, 0);
        PpcSetGprValue(4, 0);
        PpcSetGprValue(5, 0);
        Status = PpcExecuteBlock((UINT32*)(UINTN)PPC_RESET_VECTOR, 4, &Executed);

        MagicPlusOne = 0x524F4D31 + 1;
        BootSelfTestCheck(Status == EFI_SUCCESS && Executed == 4,
                          L"reset-vector program ran cleanly");
        BootSelfTestCheck(PpcGetGprValue(4) == 0x524F4D31,
                          L"program read ROM word (r4 = 'ROM1')");
        BootSelfTestCheck(
            PpcReadGuestByte((UINT32)RamBase + 0) == (UINT8)(MagicPlusOne >> 24) &&
            PpcReadGuestByte((UINT32)RamBase + 1) == (UINT8)(MagicPlusOne >> 16) &&
            PpcReadGuestByte((UINT32)RamBase + 2) == (UINT8)(MagicPlusOne >> 8) &&
            PpcReadGuestByte((UINT32)RamBase + 3) == (UINT8)MagicPlusOne,
            L"program stored result to guest RAM");
    }

    Print(L"--- Boot self-test complete: %d passed, %d failed ---\n",
          g_BootTestPasses, g_BootTestFailures);

    return (g_BootTestFailures == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;
}

// ---------------------------------------------------------------------------
// System files & drivers (classic Mac OS System Folder support)
// ---------------------------------------------------------------------------

STATIC VOID
BootFillSystemFolderInfo (
    OUT PPC_SYSTEM_FOLDER_INFO* Info
    )
{
    ZeroMem(Info, sizeof(PPC_SYSTEM_FOLDER_INFO));
    Info->Found = g_BootContext.SystemFolderFound;
    BootCopyString(Info->Path, g_BootContext.SystemFolderPath, PPC_SYSTEM_FOLDER_PATH_MAX);
    Info->SystemPresent = g_BootContext.SystemPresent;
    Info->FinderPresent = g_BootContext.FinderPresent;
    Info->ExtensionsPresent = g_BootContext.ExtensionsPresent;
    Info->MacOsRomPresent = g_BootContext.MacOsRomPresent;
    Info->FileCount = g_BootContext.SystemFileCount;
    Info->LoadedFileCount = g_BootContext.LoadedSystemFileCount;
    Info->DriverCount = g_BootContext.DriverCount;
    Info->LoadedDriverCount = g_BootContext.LoadedDriverCount;
    Info->TotalStagedBytes = g_BootContext.TotalStagedBytes;
    Info->SystemAreaBase = g_BootContext.SystemAreaInstalled ? PPC_SYSTEM_AREA_GUEST_BASE : 0;
    Info->DriverAreaBase = g_BootContext.DriverAreaInstalled ? PPC_DRIVER_AREA_GUEST_BASE : 0;
}

// Fall back to the attached Mac OS disc when the boot volume has no System
// Folder: mount the disc's HFS/HFS+ volume (via the in-emulator reader) and
// record the presence of System / Finder / Extensions / Mac OS ROM.
STATIC VOID
BootLocateSystemFolderHfs (
    VOID
    )
{
    PPC_HFS_VOLUME_INFO HfsInfo;
    EFI_STATUS Status = PpcHfsGetVolumeInfo(&HfsInfo);
    if (EFI_ERROR(Status)) {
        Status = PpcHfsMount(NULL);
        if (EFI_ERROR(Status)) {
            return;
        }
    }

    BOOLEAN Folder = FALSE;
    BOOLEAN Sys = FALSE;
    BOOLEAN Finder = FALSE;
    BOOLEAN Rom = FALSE;
    UINT32  FolderId = 0;
    Status = PpcHfsProbeBootFiles(&Folder, &Sys, &Finder, &Rom, &FolderId);
    if (EFI_ERROR(Status) || !Folder || !Sys) {
        return;
    }

    BOOLEAN Extensions = FALSE;
    PPC_HFS_ENTRY Ext;
    if (!EFI_ERROR(PpcHfsOpenPath(PPC_HFS_SYSTEM_FOLDER_PATH L":Extensions", &Ext)) &&
        Ext.IsDirectory) {
        Extensions = TRUE;
    }

    g_BootContext.SystemFolderFound = TRUE;
    g_BootContext.SystemFolderFromHfs = TRUE;
    g_BootContext.SystemPresent = Sys;
    g_BootContext.FinderPresent = Finder;
    g_BootContext.ExtensionsPresent = Extensions;
    g_BootContext.MacOsRomPresent = Rom;
    BootCopyString(g_BootContext.SystemFolderPath, L":System Folder",
                   PPC_SYSTEM_FOLDER_PATH_MAX);
    Print(L"System Folder found on HFS volume '%s': System=%d Finder=%d "
          L"Extensions=%d MacOSROM=%d (DirID %d)\n",
          HfsInfo.VolumeName, Sys, Finder, Extensions, Rom, FolderId);
}

EFI_STATUS
PpcLocateSystemFolder (
    OUT PPC_SYSTEM_FOLDER_INFO* Info
    )
{
    if (!g_BootContext.SystemFolderScanned) {
        BOOLEAN Exists = FALSE;
        EFI_STATUS Status = BootDirectoryExists(PPC_SYSTEM_FOLDER_PATH, &Exists);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        g_BootContext.SystemFolderFound = Exists;
        BootCopyString(g_BootContext.SystemFolderPath,
                       g_BootContext.SystemFolderFound ? PPC_SYSTEM_FOLDER_PATH : L"",
                       PPC_SYSTEM_FOLDER_PATH_MAX);
        if (Exists) {
            BootFileExists(PPC_SYSTEM_FILE_PATH, &g_BootContext.SystemPresent, NULL);
            BootFileExists(PPC_FINDER_FILE_PATH, &g_BootContext.FinderPresent, NULL);
            BootDirectoryExists(PPC_EXTENSIONS_DIR_PATH, &g_BootContext.ExtensionsPresent);
            BootFileExists(PPC_SYSTEM_FOLDER_ROM_PATH, &g_BootContext.MacOsRomPresent, NULL);
        }
        g_BootContext.SystemFolderScanned = TRUE;
        Print(L"System Folder scan: found=%d System=%d Finder=%d Extensions=%d MacOSROM=%d\n",
              g_BootContext.SystemFolderFound,
              g_BootContext.SystemPresent,
              g_BootContext.FinderPresent,
              g_BootContext.ExtensionsPresent,
              g_BootContext.MacOsRomPresent);

        // The boot volume (FAT ESP) has no System Folder: try the attached Mac
        // OS disc through the in-emulator HFS/HFS+ reader.
        if (!g_BootContext.SystemFolderFound) {
            BootLocateSystemFolderHfs();
        }
    }

    if (Info != NULL) {
        BootFillSystemFolderInfo(Info);
    }
    return EFI_SUCCESS;
}

EFI_STATUS
PpcLoadSystemFiles (
    VOID
    )
{
    PPC_SYSTEM_FILE* F;
    VOID* Host = NULL;
    EFI_STATUS Status;

    if (!g_BootContext.SystemFolderFound) {
        return EFI_NOT_FOUND;
    }
    if (g_BootContext.SystemFileCount > 0) {
        return EFI_ALREADY_STARTED;
    }

    Status = BootEnsureSystemArea();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // System Folder on the attached Mac OS disc: stage System and Finder
    // through the in-emulator HFS reader. The system ROM is installed
    // separately by PpcInstallSystemRom (ESP -> HFS "Mac OS ROM" -> demo).
    if (g_BootContext.SystemFolderFromHfs) {
        struct {
            CHAR16*         Path;
            CHAR16*         Report;
            PPC_SYSTEM_FILE_TYPE Type;
        } BootFiles[2] = {
            { PPC_HFS_SYSTEM_FILE_PATH, L":System Folder:System", PPC_SYSTEM_FILE_TYPE_SYSTEM },
            { PPC_HFS_FINDER_FILE_PATH, L":System Folder:Finder", PPC_SYSTEM_FILE_TYPE_FINDER },
        };
        for (UINTN I = 0; I < 2; I++) {
            BOOLEAN Present;
            switch (BootFiles[I].Type) {
            case PPC_SYSTEM_FILE_TYPE_SYSTEM: Present = g_BootContext.SystemPresent; break;
            default:                          Present = g_BootContext.FinderPresent; break;
            }
            if (!Present) {
                continue;
            }
            PPC_HFS_ENTRY E;
            Status = PpcHfsOpenPath(BootFiles[I].Path, &E);
            if (EFI_ERROR(Status) || E.IsDirectory) {
                Print(L"Failed to resolve HFS '%s': %r\n", BootFiles[I].Path, Status);
                continue;
            }
            if (E.Size == 0) {
                Print(L"  Skipped %s (empty data fork)\n", BootFiles[I].Report);
                continue;
            }
            F = &g_BootContext.SystemFiles[g_BootContext.SystemFileCount];
            Status = BootStageHfsFile(&E, BootFiles[I].Report, BootFiles[I].Type,
                                      PPC_SYSTEM_AREA_GUEST_BASE, PPC_SYSTEM_AREA_SIZE,
                                      g_BootContext.SystemAreaHost,
                                      &g_BootContext.SystemAreaCursor, F, &Host);
            if (!EFI_ERROR(Status)) {
                g_BootContext.SystemFileHosts[g_BootContext.SystemFileCount] = Host;
                g_BootContext.SystemFileCount++;
                g_BootContext.LoadedSystemFileCount++;
                Print(L"Staged %s: '%s' -> guest 0x%x (%d bytes)\n",
                      F->Type == PPC_SYSTEM_FILE_TYPE_SYSTEM ? L"System file" :
                      F->Type == PPC_SYSTEM_FILE_TYPE_FINDER ? L"Finder" : L"Mac OS ROM file",
                      F->Name, (UINT64)F->GuestAddress, (UINT64)F->FileSize);
            } else {
                Print(L"Failed to stage %s: %r\n", BootFiles[I].Report, Status);
            }
        }
        return (g_BootContext.LoadedSystemFileCount > 0) ? EFI_SUCCESS : EFI_NOT_FOUND;
    }

    if (g_BootContext.SystemPresent) {
        F = &g_BootContext.SystemFiles[g_BootContext.SystemFileCount];
        Status = BootStageFile(PPC_SYSTEM_FILE_PATH, PPC_SYSTEM_FILE_TYPE_SYSTEM,
                               PPC_SYSTEM_AREA_GUEST_BASE, PPC_SYSTEM_AREA_SIZE,
                               g_BootContext.SystemAreaHost, &g_BootContext.SystemAreaCursor,
                               F, &Host);
        if (!EFI_ERROR(Status)) {
            g_BootContext.SystemFileHosts[g_BootContext.SystemFileCount] = Host;
            g_BootContext.SystemFileCount++;
            g_BootContext.LoadedSystemFileCount++;
            Print(L"Staged System file: %s -> guest 0x%x (%d bytes)\n",
                  F->Name, (UINT64)F->GuestAddress, (UINT64)F->FileSize);
        } else {
            Print(L"Failed to stage System file: %r\n", Status);
        }
    }

    if (g_BootContext.FinderPresent) {
        F = &g_BootContext.SystemFiles[g_BootContext.SystemFileCount];
        Status = BootStageFile(PPC_FINDER_FILE_PATH, PPC_SYSTEM_FILE_TYPE_FINDER,
                               PPC_SYSTEM_AREA_GUEST_BASE, PPC_SYSTEM_AREA_SIZE,
                               g_BootContext.SystemAreaHost, &g_BootContext.SystemAreaCursor,
                               F, &Host);
        if (!EFI_ERROR(Status)) {
            g_BootContext.SystemFileHosts[g_BootContext.SystemFileCount] = Host;
            g_BootContext.SystemFileCount++;
            g_BootContext.LoadedSystemFileCount++;
            Print(L"Staged Finder: %s -> guest 0x%x (%d bytes)\n",
                  F->Name, (UINT64)F->GuestAddress, (UINT64)F->FileSize);
        } else {
            Print(L"Failed to stage Finder: %r\n", Status);
        }
    }

    // The system ROM (Old World dump or New World "Mac OS ROM") is installed
    // separately by PpcInstallSystemRom, not staged into the system area.

    return (g_BootContext.LoadedSystemFileCount > 0) ? EFI_SUCCESS : EFI_NOT_FOUND;
}

EFI_STATUS
PpcScanExtensionsDirectory (
    VOID
    )
{
    if (!g_BootContext.SystemFolderFound) {
        return EFI_NOT_FOUND;
    }
    if (g_BootContext.SystemFolderFromHfs) {
        return BootEnumerateExtensionsHfs();
    }
    return BootEnumerateExtensions();
}

EFI_STATUS
PpcLoadDrivers (
    VOID
    )
{
    UINTN I;
    UINTN Loaded = 0;
    EFI_STATUS Status;

    if (g_BootContext.DriverCount == 0) {
        return EFI_NOT_FOUND;
    }
    if (g_BootContext.LoadedDriverCount > 0) {
        return EFI_ALREADY_STARTED;
    }

    Status = BootEnsureDriverArea();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    for (I = 0; I < g_BootContext.DriverCount; I++) {
        PPC_SYSTEM_FILE* D = &g_BootContext.Drivers[I];
        VOID* Host = NULL;
        if (D->FileSize == 0) {
            Print(L"  Skipped driver '%s' (empty data fork)\n", D->Name);
            continue;
        }
        if (g_BootContext.SystemFolderFromHfs) {
            PPC_HFS_ENTRY E;
            // Resolve by catalog ID: the entry came from PpcHfsListChildren, and
            // re-opening the path would mis-split names containing '/' or ':'.
            Status = (D->HfsId != 0) ? PpcHfsGetEntryById(D->HfsId, &E)
                                     : PpcHfsOpenPath(D->Path, &E);
            if (EFI_ERROR(Status) || E.IsDirectory) {
                Print(L"  Failed to resolve driver '%s': %r\n", D->Name, Status);
                continue;
            }
            Status = BootStageHfsFile(&E, D->Path, PPC_SYSTEM_FILE_TYPE_DRIVER,
                                      PPC_DRIVER_AREA_GUEST_BASE, PPC_DRIVER_AREA_SIZE,
                                      g_BootContext.DriverAreaHost,
                                      &g_BootContext.DriverAreaCursor, D, &Host);
        } else {
            Status = BootStageFile(D->Path, PPC_SYSTEM_FILE_TYPE_DRIVER,
                                   PPC_DRIVER_AREA_GUEST_BASE, PPC_DRIVER_AREA_SIZE,
                                   g_BootContext.DriverAreaHost, &g_BootContext.DriverAreaCursor,
                                   D, &Host);
        }
        if (!EFI_ERROR(Status)) {
            g_BootContext.DriverHosts[I] = Host;
            Loaded++;
            Print(L"  Staged driver: %s -> guest 0x%x (%d bytes)\n",
                  D->Name, (UINT64)D->GuestAddress, (UINT64)D->FileSize);
        } else {
            Print(L"  Failed to stage driver '%s': %r\n", D->Name, Status);
        }
    }

    g_BootContext.LoadedDriverCount = Loaded;
    return (Loaded > 0) ? EFI_SUCCESS : EFI_NOT_FOUND;
}

EFI_STATUS
PpcGetSystemFolderInfo (
    OUT PPC_SYSTEM_FOLDER_INFO* Info
    )
{
    if (Info == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    BootFillSystemFolderInfo(Info);
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetSystemFile (
    IN  UINTN Index,
    OUT PPC_SYSTEM_FILE* File
    )
{
    if (File == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (Index >= g_BootContext.SystemFileCount) {
        return EFI_NOT_FOUND;
    }
    *File = g_BootContext.SystemFiles[Index];
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetDriver (
    IN  UINTN Index,
    OUT PPC_SYSTEM_FILE* Driver
    )
{
    if (Driver == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (Index >= g_BootContext.DriverCount) {
        return EFI_NOT_FOUND;
    }
    *Driver = g_BootContext.Drivers[Index];
    return EFI_SUCCESS;
}

EFI_STATUS
PpcRunSystemFilesSelfTest (
    VOID
    )
{
    UINTN I;
    UINTN DriverMismatch = 0;

    g_BootTestPasses = 0;
    g_BootTestFailures = 0;

    Print(L"--- System Files & Drivers Self-Test ---\n");

    BootSelfTestCheck(g_BootContext.SystemFolderScanned, L"system folder scan ran");
    BootSelfTestCheck(g_BootContext.LoadedSystemFileCount <= g_BootContext.SystemFileCount,
                      L"system file count consistent");
    BootSelfTestCheck(g_BootContext.LoadedDriverCount <= g_BootContext.DriverCount,
                      L"driver count consistent");

    for (I = 0; I < g_BootContext.SystemFileCount; I++) {
        PPC_SYSTEM_FILE* F = &g_BootContext.SystemFiles[I];
        if (!F->Loaded) {
            continue;
        }
        UINT8 First = PpcReadGuestByte((UINT32)F->GuestAddress);
        UINT8 Expect = ((UINT8*)g_BootContext.SystemFileHosts[I])[0];
        BootSelfTestCheck(First == Expect, F->Name);
    }

    for (I = 0; I < g_BootContext.DriverCount; I++) {
        PPC_SYSTEM_FILE* D = &g_BootContext.Drivers[I];
        if (!D->Loaded) {
            continue;
        }
        UINT8 First = PpcReadGuestByte((UINT32)D->GuestAddress);
        UINT8 Expect = ((UINT8*)g_BootContext.DriverHosts[I])[0];
        if (First != Expect) {
            DriverMismatch++;
        }
    }
    BootSelfTestCheck(DriverMismatch == 0, L"staged drivers read back correctly");

    if (g_BootContext.SystemReady) {
        BootSelfTestCheck(
            PpcReadGuestByte(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET + 0) == 0x45 &&
            PpcReadGuestByte(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET + 1) == 0x46 &&
            PpcReadGuestByte(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET + 2) == 0x49 &&
            PpcReadGuestByte(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET + 3) == 0x21,
            L"low-memory boot info intact after staging");
    }

    Print(L"--- System files self-test complete: %d passed, %d failed ---\n",
          g_BootTestPasses, g_BootTestFailures);

    return (g_BootTestFailures == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;
}
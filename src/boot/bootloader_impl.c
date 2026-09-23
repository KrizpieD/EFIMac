#include "bootloader.h"
#include <efi.h>
#include <efilib.h>
#include "cpu/interpreter.h"
#include "cpu/translation.h"
#include "cpu/emul_op.h"
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
    BOOLEAN OsRuntimeStaged;    // 68K System data fork staged for DR handoff
} PPC_BOOTLOADER_CONTEXT;

// Global bootloader context
static PPC_BOOTLOADER_CONTEXT g_BootContext = {0};

// ---------------------------------------------------------------------------
// File load helper: read a whole file from the boot volume into a page-aligned
// buffer (UEFI pool allocations are limited to ~128 KB; Mac OS ROM images are
// several MB, so ROM/ROM-like blobs are page-backed).
// ---------------------------------------------------------------------------
static
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

static
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
static
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
static
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
static
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
static UINTN g_BootTestPasses   = 0;
static UINTN g_BootTestFailures = 0;

static VOID
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
static VOID
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
static VOID
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
static INTN
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
static VOID
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
static EFI_STATUS
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
static EFI_STATUS
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

static VOID
BootExtractFileName (
    IN  CHAR16* Path,
    OUT CHAR16* Name,
    IN  UINTN   MaxChars
    );

// Stage a single file from the boot volume into a guest staging area.
static EFI_STATUS
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
static EFI_STATUS
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
static EFI_STATUS
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
static EFI_STATUS
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
static VOID
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
static UINT32
BootIdentifyRomType (
    IN const UINT8* Rom,
    IN UINTN        Size
    )
{
    static const UINT8 ChrpBoot[11] = { '<', 'C', 'H', 'R', 'P', '-',
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

static UINT32
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
static BOOLEAN
BootFindChrpHexConstant (
    IN const UINT8* Data,
    IN UINTN        Size,
    IN const CHAR8* Name,
    OUT UINT32*     Value
    )
{
    static const CHAR8 Token[] = "constant ";
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

static VOID
BootLzssDecode (
    IN const UINT8* Src,
    IN UINTN        Size,
    OUT UINT8*      Dest,
    IN UINTN        DestSize
    )
{
    UINT8* Dict = g_LzssDict;
    INTN  RunMask = 0;
    INTN  Remaining = (INTN)Size;
    UINTN DictIdx = 0xFEE;
    UINTN Written = 0;

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
            if (Written < DestSize) { Dest[Written] = C; Written++; }
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
                if (Written < DestSize) { Dest[Written] = C; Written++; }
                Start = (Start + 1) & 0xFFF;
                DictIdx = (DictIdx + 1) & 0xFFF;
            }
        }
        RunMask >>= 1;
    }
}

// Walk the 'prcl' parcel chain and expand the 'rom ' parcel (LZSS) into the
// flat ROM image buffer Dest.
static EFI_STATUS
BootDecodeParcels (
    IN const UINT8* Parcels,
    IN UINTN        ParcelsSize,
    OUT UINT8*      Dest,
    IN UINTN        DestSize
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
            BootLzssDecode(Parcels + ParcelBase + LzssOffset, LzssSize, Dest,
                           DestSize);
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
static BOOLEAN
BootDecodeChrpRom (
    IN  const UINT8* Buffer,
    IN  UINTN        Size,
    OUT VOID**       Out,
    OUT UINTN*       OutSize
    )
{
    static const UINT8 ChrpBoot[11] = { '<', 'C', 'H', 'R', 'P', '-',
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

    Print(L"RAWPARM parcels-offset=0x%X parcels-size=0x%X file-size=0x%X\n",
          ParcelsOffset, ParcelsSize, (UINT32)Size);
    Print(L"RAWHEAD %02X%02X%02X%02X-%02X%02X%02X%02X-%02X%02X%02X%02X-%02X%02X%02X%02X\n",
           Buffer[0], Buffer[1], Buffer[2], Buffer[3],
           Buffer[4], Buffer[5], Buffer[6], Buffer[7],
           Buffer[8], Buffer[9], Buffer[10], Buffer[11],
           Buffer[12], Buffer[13], Buffer[14], Buffer[15]);
    Print(L"RAWLZSS %02X%02X%02X%02X-%02X%02X%02X%02X-%02X%02X%02X%02X-%02X%02X%02X%02X\n",
           Buffer[0x340C4], Buffer[0x340C5], Buffer[0x340C6], Buffer[0x340C7],
           Buffer[0x340C8], Buffer[0x340C9], Buffer[0x340CA], Buffer[0x340CB],
           Buffer[0x340CC], Buffer[0x340CD], Buffer[0x340CE], Buffer[0x340CF],
           Buffer[0x340D0], Buffer[0x340D1], Buffer[0x340D2], Buffer[0x340D3]);

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

    Status = BootDecodeParcels(Buffer + ParcelsOffset, ParcelsSize, Rom,
                               (PPC_ROM_MAX_SIZE + 0x10000));
    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        Print(L"Failed to decode New World ROM parcels: %r\n", Status);
        return FALSE;
    }

    Print(L"New World ROM decompressed: %d bytes of parcels to flat image\n",
          (UINT64)ParcelsSize);

    {
        INTN i;
        Print(L"FLATBOOT [0000AF60]");
        for (i = 0; i < 32; i++) {
            Print(L"%08X", BootReadBe32(Rom + 0xAF60 + (UINTN)i * 4));
            if ((i & 3) == 3) Print(L"\n");
            else if ((i & 3) != 0 || i == 0) Print(L" ");
        }
        Print(L"FLATDESC [0000E180]");
        for (i = 0; i < 96; i++) {
            Print(L"%08X", BootReadBe32(Rom + 0xE180 + (UINTN)i * 4));
            if ((i & 3) == 3) Print(L"\n");
            else if ((i & 3) != 0 || i == 0) Print(L" ");
        }
    }

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
static EFI_STATUS
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

    // Classic Mac ROM top-of-space alias: a 4 MB image appears at
    // 0xFFC00000..0xFFFFFFFF on real hardware. New World dispatch tables
    // hold 0xFFC4xxxx pointers that must resolve here. Same host buffer
    // => both views stay consistent.
    //
    // SEEDING (session 5): mirror the image across ALL FOUR 4 MB windows
    // of the 0xFF half (FF0/FF4/FF8/FFC). Unrelocated table entries in the
    // image encode ROM offsets as 0xFFxxxxxx words; Apple's loader rebases
    // them, but any entry we missed then reads as an address in FF0-FFB
    // and faults. With every window mapped, such entries resolve directly
    // (VA & 0x3FFFFF == offset within window), exactly like classic 24-bit
    // ROM mirroring. The FFC window keeps its existing linear semantics.
    if (g_BootContext.RomType == PPC_ROM_TYPE_NEW_WORLD && Size == 0x00400000u) {
        EFI_STATUS AliasStatus =
            PpcAddGuestMemoryRegion(Buffer, 0xFFC00000u, (UINT32)Size,
                                    FALSE);
        Print(L"ROM alias installed: %d bytes at guest 0xFFC00000 (%r)\n",
              (UINT64)Size, AliasStatus);
        {
            static const UINT32 ExtraWindows[3] =
                { 0xFF000000u, 0xFF400000u, 0xFF800000u };
            UINTN W;
            for (W = 0; W < 3; W++) {
                // Read-only: stray frame allocations from broken-stack eras
                // must not corrupt the shared ROM buffer.
                EFI_STATUS S2 = PpcAddGuestMemoryRegion(
                    Buffer, ExtraWindows[W], (UINT32)Size, TRUE);
                Print(L"ROM alias window %08x (%r)\n", ExtraWindows[W], S2);
            }
        }

        // SEEDING: trampoline soft-landing page. NK continuation jumps
        // land at 0x7F40xx (page-stable, offset-varies) when the builder
        // stage never ran. Fill 0x7F4000-0x7FFFFF with RTS instructions:
        // a wild entry unwinds up the stack one frame per pop until it
        // reaches a genuine caller, keeping the boot alive instead of
        // executing zeros into the guard halt.
        {
            UINT32 A;
            for (A = 0x7F4000u; A + 1 < 0x800000u; A += 2) {
                PpcWriteGuestByte(A,     0x4E);
                PpcWriteGuestByte(A + 1, 0x75);
            }
            Print(L"  trampoline soft-landing: RTS sled @7F4000-7FFFFF\n");
        }

        // SheepShaver-equivalent boot-structure patches
        // (rom_patches.cpp: patch_nanokernel_boot):
        //  - Copy the last 1 MB of ROM (PPC emulator + tables) to a
        //    writable bank right after the image, then point
        //    LA_EmulatorCode / LA_DispatchTable there. The DR emulator's
        //    opcode/dispatch tables MUST live in modifiable RAM; the
        //    Apple flow has the nanokernel copy them to 0x68060000 /
        //    0x68080000, which nothing in our environment performs --
        //    leaving those tables zeroed and every handler lookup
        //    returning 0 (observed as an endless resolver(d0=0) loop in
        //    the TRAP $A247 patch-scanner).
        //  - Force Physical RAM base to 0 (NewWorld ROM ships -1).
        //  - Point the 68k reset vector at ROMBase+0x2a.
        {
            UINTN BankPages = 0x00100000 / EFI_PAGE_SIZE;
            EFI_PHYSICAL_ADDRESS BankBase = 0;
            EFI_STATUS BankStatus =
                BS->AllocatePages(AllocateAnyPages, EfiBootServicesData,
                                  BankPages, &BankBase);
            if (!EFI_ERROR(BankStatus)) {
                UINT8* Bank = (UINT8*)(UINTN)BankBase;
                ZeroMem(Bank, 0x00100000);
                CopyMem(Bank, (UINT8*)Buffer + (Size - 0x00100000u),
                        0x00100000);
                BankStatus = PpcAddGuestMemoryRegion(
                    Bank, GuestBase + Size, 0x00100000u, FALSE);
                if (!EFI_ERROR(BankStatus)) {
                    // Boot structure at ROM+0x30d000 (big-endian image):
                    // store byte-swapped values.
                    UINT32* Boot = (UINT32*)((UINT8*)Buffer + 0x30D000u);
                    Boot[0x09C >> 2] = __builtin_bswap32(0x68FFE000u); // LA_InfoRecord (keep Apple KData)
                    Boot[0x0A0 >> 2] = __builtin_bswap32(0x68FFE000u); // LA_KernelData
                    Boot[0x0A4 >> 2] = __builtin_bswap32(0x68FFF000u); // LA_EmulatorData
                    Boot[0x0A8 >> 2] = __builtin_bswap32(GuestBase + Size + 0x80000u); // LA_DispatchTable -> writable bank
                    Boot[0x0AC >> 2] = __builtin_bswap32(GuestBase + Size + 0x60000u); // LA_EmulatorCode  -> writable bank
                    Boot[0x360 >> 2] = 0;                              // PhysRAMBase = 0
                    Boot[0xFD8 >> 2] = __builtin_bswap32(0x40800000u + 0x2Au); // reset vec
                    Print(L"EMU bank installed: guest 0x%08x, "
                          L"LA_EmulatorCode/Dispatch -> bank\n",
                          (UINT32)(GuestBase + Size));
                } else {
                    BS->FreePages(BankBase, BankPages);
                    Print(L"EMU bank map failed: %r\n", BankStatus);
                }
            }
        }

        // -- 68K RAM-Rom window (New World) --------------------------------
        // On real hardware the ROM's decompressor writes the 68K RAM-Rom
        // image into top-of-space RAM (68K code fetches at 0x8103xxxx), then
        // the 68K bootstrap resumes at 0x810303B0.  Nothing owns that window
        // here, so those writes were silently dropped and the 68K fetch read
        // zeros -> DR dispatched into empty space -> stub blr through LR=0 ->
        // GUEST STOP at PC 0.  Install a WRITABLE zeroed region so the ROM's
        // own decompressor can populate it.  16 MB is far larger than the
        // decompressed RAM-Rom and covers both plausible bases (0x81000000 /
        // 0x81030000).
        {
            UINTN RrPages = 0x01000000 / EFI_PAGE_SIZE;
            EFI_PHYSICAL_ADDRESS RrBase = 0;
            EFI_STATUS RrStatus =
                BS->AllocatePages(AllocateAnyPages, EfiBootServicesData,
                                  RrPages, &RrBase);
            if (!EFI_ERROR(RrStatus)) {
                UINT8* Rr = (UINT8*)(UINTN)RrBase;
                ZeroMem(Rr, 0x01000000);
                RrStatus = PpcAddGuestMemoryRegion(
                    Rr, 0x81000000u, 0x01000000u, FALSE);
                if (EFI_ERROR(RrStatus)) {
                    BS->FreePages(RrBase, RrPages);
                    Print(L"68K RAM-Rom window map failed: %r\n", RrStatus);
                } else {
                    Print(L"68K RAM-Rom window: guest 0x81000000 +16MB\n");
                }
            }
            // "-- 68K secondary native-workspace window ------------------------
            // The OS's 68K boot references additional high-68K workspace at
            // 0x52xxxxxx-0x7C8xxxxx (native transcode tables / context) that a
            // real Mac has in RAM.  Install second writable mapping of the same
            // staged System source so the DRAME's table walks find content
            // instead of zeros.
            {
                UINTN NsPages = 0x01000000 / EFI_PAGE_SIZE;
                EFI_PHYSICAL_ADDRESS NsBase = 0;
                EFI_STATUS NsStatus =
                    BS->AllocatePages(AllocateAnyPages, EfiBootServicesData,
                                      NsPages, &NsBase);
                if (!EFI_ERROR(NsStatus)) {
                    UINT8* Ns = (UINT8*)(UINTN)NsBase;
                    ZeroMem(Ns, 0x01000000);
                    NsStatus = PpcAddGuestMemoryRegion(
                        Ns, 0x52000000u, 0x01000000u, FALSE);
                    if (EFI_ERROR(NsStatus)) {
                        BS->FreePages(NsBase, NsPages);
                        Print(L"68K workspace window map failed: %r\n", NsStatus);
                    } else {
                        Print(L"68K workspace window: guest 0x52000000 +16MB\n");
                    }
                }
            }
            // "-- 68K system-global alias window ------------------------------
            // The 68K boot builds and calls its native-emulator bridge at
            // 0x80BD0000-0x80BDFFFF (the OS copies glue there, then JSRs into
            // it to invoke PPC-side routines).  Nothing owns that address in
            // a flat map, so writes were dropped and the JSR executed zeros.
            // Install a third writable zeroed region to hold that page.
            {
                UINTN SgPages = 0x01000000 / EFI_PAGE_SIZE;
                EFI_PHYSICAL_ADDRESS SgBase = 0;
                EFI_STATUS SgStatus =
                    BS->AllocatePages(AllocateAnyPages, EfiBootServicesData,
                                      SgPages, &SgBase);
                if (!EFI_ERROR(SgStatus)) {
                    UINT8* Sg = (UINT8*)(UINTN)SgBase;
                    ZeroMem(Sg, 0x01000000);
                    SgStatus = PpcAddGuestMemoryRegion(
                        Sg, 0x80000000u, 0x01000000u, FALSE);
                    if (EFI_ERROR(SgStatus)) {
                        BS->FreePages(SgBase, SgPages);
                        Print(L"68K sys-globals window map failed: %r\n", SgStatus);
                    } else {
                        UINTN SgI;
                        Print(L"68K sys-globals window: guest 0x80000000 +16MB\n");
                        // Seed the native-bridge page with a benign 68K
                        // RTS stub so a 68K bootstrap JSR into 0x80BDxxxx
                        // returns instead of executing zeros (data-march).
                        for (SgI = 0; SgI < 0x30000 / 2; SgI++) {
                            PpcWriteGuestByte(0x80BD0000u + (UINT32)SgI*2,     0x4E);
                            PpcWriteGuestByte(0x80BD0000u + (UINT32)SgI*2 + 1, 0x75);
                        }
                        Print(L"68K sys-globals stub seeded @0x80BD0000..0x80BF0000\n");
                        if (PpcReadGuestByte(0x80BD0006u) != 0x4E ||
                            PpcReadGuestByte(0x80BD0007u) != 0x75) {
                            Print(L"68K sys-globals readback FAILED "
                                  L"(0x%02x 0x%02x) - writes dropped?\n",
                                  PpcReadGuestByte(0x80BD0006u),
                                  PpcReadGuestByte(0x80BD0007u));
                        } else {
                            Print(L"68K sys-globals readback OK: 0x4E75\n");
                        }
                    }
                }
            }
            // "-- DRAME lazy-translation cache windows -------------------
            // The ROM's PPC 68K emulator dispatches each guest op through
            // per-opcode cells it builds lazily in low RAM pages around
            // 0x0170xxxx (first-level) and 0x0284xxxx (second-level).  On
            // real hardware those live inside the low-RAM bank; our 16 MB
            // boot bank stops at 0x01000000, so both cell windows were
            // unmapped: reads returned zeros, writes were dropped, and the
            // lazy cells could never be populated.  Install a writable
            // zeroed region covering 0x01000000-0x04000000 so the DRAME's
            // JIT writes (and the host's trampolines) actually land.
            {
                UINTN DrPages = 0x03000000 / EFI_PAGE_SIZE;
                EFI_PHYSICAL_ADDRESS DrBase = 0;
                EFI_STATUS DrStatus =
                    BS->AllocatePages(AllocateAnyPages, EfiBootServicesData,
                                      DrPages, &DrBase);
                if (!EFI_ERROR(DrStatus)) {
                    UINT8* Dr = (UINT8*)(UINTN)DrBase;
                    ZeroMem(Dr, 0x03000000);
                    DrStatus = PpcAddGuestMemoryRegion(
                        Dr, 0x01000000u, 0x03000000u, FALSE);
                    if (EFI_ERROR(DrStatus)) {
                        BS->FreePages(DrBase, DrPages);
                        Print(L"DRAME cache window map failed: %r\n", DrStatus);
                    } else {
                        Print(L"DRAME cache window: guest 0x01000000 +48MB\n");
                    }
                }
            }

            {
                // Mirror the DRAME's ROM cell-template shapes into the lazy-cell
                // banks the machine jumps into, so a start/secondary cell
                // contains real translated PPC (the operand-class walker shapes
                // at 0x40B67C60..) instead of zeros.  The whole span is copied
                // contiguously so its short relative branches (b at +0x1c,
                // bgtctr/bgelr via CR/CTR/LR) stay correct.  Longer-range exits
                // (b +0x5494 to the hub) land past the copy, so each descent
                // escape (`bgelr cr2` = 0x4CA80020 and `b hub` = 0x48005494)
                // is repointed at a local 4-word shim at 0x032C1000 that sets
                // LR to the ROM cell-processor continuation (0x40B6D7CC, the
                // "next 68K op" entry) and blr's there.  Op 0x0058 (ORI #imm)
                // is exactly the 2-byte-immediate walker shape that starts here.
                UINT32 SrcBase = PPC_NEW_WORLD_ROM_GUEST_BASE + 0x367C60u;
                UINT32 DstBase[] = { 0x017080A2u, 0x02847F05u, 0x032C0000u };
                UINT32 Span = 0x200u;
                const UINT32 ShimBase = 0x032C1000u;
                UINT32 C, D, I;
                for (C = 0; C < sizeof(DstBase)/sizeof(DstBase[0]); C++) {
                    for (I = 0; I < Span; I += 4) {
                        PpcWriteGuestByte(DstBase[C] + I,     PpcReadGuestByte(SrcBase + I));
                        PpcWriteGuestByte(DstBase[C] + I + 1, PpcReadGuestByte(SrcBase + I + 1));
                        PpcWriteGuestByte(DstBase[C] + I + 2, PpcReadGuestByte(SrcBase + I + 2));
                        PpcWriteGuestByte(DstBase[C] + I + 3, PpcReadGuestByte(SrcBase + I + 3));
                    }
                    // Repoint every descent escape to the shim.
                    for (I = 0; I < Span; I += 4) {
                        UINT32 Word = ((UINT32)PpcReadGuestByte(DstBase[C] + I) << 24) |
                                      ((UINT32)PpcReadGuestByte(DstBase[C] + I + 1) << 16) |
                                      ((UINT32)PpcReadGuestByte(DstBase[C] + I + 2) << 8) |
                                      ((UINT32)PpcReadGuestByte(DstBase[C] + I + 3));
                        if (Word == 0x4CA80020u || Word == 0x48005494u) {
                            INT64 Dist = (INT64)(ShimBase - (DstBase[C] + I));
                            UINT32 B = 0x48000000u | (UINT32)((Dist >> 2) & 0x03FFFFFFu);
                            PpcWriteGuestByte(DstBase[C] + I,     (UINT8)(B >> 24));
                            PpcWriteGuestByte(DstBase[C] + I + 1, (UINT8)(B >> 16));
                            PpcWriteGuestByte(DstBase[C] + I + 2, (UINT8)(B >> 8));
                            PpcWriteGuestByte(DstBase[C] + I + 3, (UINT8)B);
                        }
                    }
                }
                // The shim: set LR to the machine's cell-processor continuation
                // (0x40B6D7CC, the next-68K-op entry) then blr there.
                {
                    const UINT32 Sh[] = { 0x3D8040B6u, 0x618CD7CCu,
                                          0x7D8C03A6u, 0x4E800020u };
                    for (D = 0; D < sizeof(Sh)/sizeof(Sh[0]); D++) {
                        PpcWriteGuestByte(ShimBase + D * 4,     (UINT8)(Sh[D] >> 24));
                        PpcWriteGuestByte(ShimBase + D * 4 + 1, (UINT8)(Sh[D] >> 16));
                        PpcWriteGuestByte(ShimBase + D * 4 + 2, (UINT8)(Sh[D] >> 8));
                        PpcWriteGuestByte(ShimBase + D * 4 + 3, (UINT8)Sh[D]);
                    }
                }
                Print(L"DRAME cells seeded from ROM+0x367C60 (%d words each), "
                      L"descent->shim 0x%08X\n", Span / 4, ShimBase);
                for (D = 0; D < sizeof(DstBase)/sizeof(DstBase[0]); D++) {
                    UINT32 A = DstBase[D];
                    Print(L"  seed@0x%08X: %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x\n",
                          A,
                          PpcReadGuestByte(A),     PpcReadGuestByte(A + 1),
                          PpcReadGuestByte(A + 2), PpcReadGuestByte(A + 3),
                          PpcReadGuestByte(A + 4), PpcReadGuestByte(A + 5),
                          PpcReadGuestByte(A + 6), PpcReadGuestByte(A + 7),
                          PpcReadGuestByte(A + 8), PpcReadGuestByte(A + 9),
                          PpcReadGuestByte(A + 10), PpcReadGuestByte(A + 11));
                }
            }
        }
    }

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
    static const UINT32 DemoProgram[4] = {
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
    // PHASE A: contiguous boot RAM bank at guest physical 0.
    //
    // Classic Mac OS firmware maps system RAM contiguously from physical
    // address 0, and the nanokernel places its boot workspace, kernel data,
    // EWA, stacks and lock structures there (observed live: SPRG4 workspace
    // relocated to 0x37E000, recursive-spinlock header at 0xAE000). The old
    // map backed only [0,256 KB) plus a hand-carved stack hole at
    // [0x600000,0x800000), leaving every NK structure above 256 KB reading
    // as zero-filled void -- the direct cause of the cold-boot spinlock.
    // Back the whole window with one writable region instead; low-memory
    // globals are simply its first bytes.
    UINTN Pages;
    EFI_PHYSICAL_ADDRESS Base = 0;
    EFI_STATUS Status;

    if (g_BootContext.LowMemoryInstalled) {
        if (LowMemAddress != NULL) { *LowMemAddress = g_BootContext.LowMemoryAddress; }
        if (LowMemSize != NULL) { *LowMemSize = g_BootContext.LowMemorySize; }
        return EFI_ALREADY_STARTED;
    }

    Pages = PPC_BOOT_RAM_BANK_SIZE / EFI_PAGE_SIZE;
    Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate boot RAM bank pages: %r\n", Status);
        return Status;
    }
    ZeroMem((VOID*)(UINTN)Base, PPC_BOOT_RAM_BANK_SIZE);

    Status = PpcAddGuestMemoryRegion((VOID*)(UINTN)Base,
                                     PPC_BOOT_RAM_BANK_GUEST_BASE,
                                     PPC_BOOT_RAM_BANK_SIZE,
                                     FALSE);
    if (EFI_ERROR(Status)) {
        BS->FreePages(Base, Pages);
        Print(L"Failed to map boot RAM bank into guest memory: %r\n", Status);
        return Status;
    }

    g_BootContext.LowMemoryInstalled = TRUE;
    g_BootContext.LowMemoryAddress = PPC_BOOT_RAM_BANK_GUEST_BASE;
    g_BootContext.LowMemorySize = PPC_BOOT_RAM_BANK_SIZE;

    if (LowMemAddress != NULL) { *LowMemAddress = PPC_BOOT_RAM_BANK_GUEST_BASE; }
    if (LowMemSize != NULL) { *LowMemSize = PPC_BOOT_RAM_BANK_SIZE; }

    Print(L"Boot RAM bank installed: %d MB at guest 0x%x (contiguous, "
          L"covers NK workspace/stacks/locks)\n",
          (UINT32)(PPC_BOOT_RAM_BANK_SIZE >> 20), PPC_BOOT_RAM_BANK_GUEST_BASE);

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
static VOID
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

// Read a big-endian 32-bit word from the ROM host buffer.
static UINT32
RomPatchReadWord32 (
    IN UINT8*  Rom,
    IN UINT32  Offset
    )
{
    return ((UINT32)Rom[Offset + 0] << 24) |
           ((UINT32)Rom[Offset + 1] << 16) |
           ((UINT32)Rom[Offset + 2] << 8)  |
            (UINT32)Rom[Offset + 3];
}

// Relocate the ROM's 68K jump tables (SheepShaver rom_patches.cpp
// "Relocate jump tables ($2000..)"). The New World image stores table
// entries as 0xFFxxxxxx words encoding a ROM-relative offset; Apple's own
// boot loader rebases them to absolute addresses before the nanokernel
// uses them. We ship no such loader, so every entry must be rewritten here:
//   while entry looks like 0xFFxxxxxx -> entry = (entry & 0x3FFFFF) + RomBase
//   skip zero padding between blocks
//   stop when the next block header does not match.
// Header pattern (LEA 14(A5),A1 ; MOVE.L A0,(A1) ; RTS) marks each table;
// entries begin 16 bytes after the header start.
static VOID
RomRelocateJumpTables (
    IN UINT8*  Rom,
    IN UINT32  Size,
    IN UINT32  RomBase
    )
{
    static const UINT8 JumpTabHdr[10] =
        {0x41,0xFA,0x00,0x0E, 0x21,0xC8,0x20,0x10, 0x4E,0x75};
    UINT32 Off;
    UINTN TotalFixed = 0;
    UINTN Tables = 0;

    for (Off = 0; Off + 20 <= Size; Off += 2) {
        UINTN K;
        for (K = 0; K < sizeof(JumpTabHdr); K++) {
            if (Rom[Off + K] != JumpTabHdr[K]) break;
        }
        if (K != sizeof(JumpTabHdr)) {
            continue;
        }
        {
            UINT32 Lp = Off + 16;
            UINTN Fixed = 0;
            Tables++;
            for (;;) {
                // Rebase contiguous run of 0xFFxxxxxx entries.
                while (Lp + 4 <= Size &&
                       (RomPatchReadWord32(Rom, Lp) & 0xFF000000u)
                           == 0xFF000000u) {
                    RomPatchWriteWord32(
                        Rom, Lp,
                        (RomPatchReadWord32(Rom, Lp) & 0x003FFFFFu) + RomBase);
                    Fixed++;
                    Lp += 4;
                }
                // Skip zero padding between blocks.
                while (Lp + 4 <= Size && RomPatchReadWord32(Rom, Lp) == 0) {
                    Lp += 4;
                }
                // Continue only if the next block reuses the same header.
                if (Lp + 4 > Size ||
                    RomPatchReadWord32(Rom, Lp) != 0x41FA000Eu) {
                    break;
                }
                Lp += 4;
            }
            TotalFixed += Fixed;
            Print(L"  jump-table relocated @ROM+0x%08x (%d entries)\n",
                  Off, (UINT32)Fixed);
        }
    }
    Print(L"  jump-table relocation: %d tables, %d entries rebased to "
          L"%08x\n", (UINT32)Tables, (UINT32)TotalFixed, RomBase);
}

// ---------------------------------------------------------------------------
// FAITHFUL HANDOFF (DingusPPC-faithful, 2026-08): the four SheepShaver handoff
// builders below (RomWriteEmulStartRoutine, RomWriteEmulatorEntryRoutine,
// RomWriteEmulatorDispatchHelper, RomWriteEmulatorClassHelper) are retired.
#if 0 // FAITHFUL handoff builders retired
// Install one 27-word 68K emulator-entry routine (SheepShaver's
// emulator-start/MixedMode/Reset/FC1E/FE0A/FE0F fragments). The routines are
// identical except for the `lwz r10,<offset>(r1)` word that picks the
// NanoKernelCallTable slot to blr to once the 68K context is saved, and the
// final word: the emulator-start fragment (trap 0, ROM + 0x36f900) branches
// to the injected 68K DR-emulator entry (RomWriteEmulatorEntryRoutine), the
// others keep the plain `blr`.
static VOID
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
// r8..r15, a0..a6 = r16..r22, a7 = r1 = 0x2600, r23 = 0, r24 = 68K PC - 2
// (0x40800028: the ROM's own loop pre-fetches +2), r25 = SR MSB (0x27),
// r26 = 0, r28 = 0 (VBR), r29 = opcode table (0x40b80000), r30 = emulator
// base (0x40b60000), r31 = KDP + 0x1000, XER = 0 -- then jumps into the
// ROM's own DR-loop body at 0x40b66080. Handlers depend on invariants only
// that loop establishes (r24 pre-advanced past the opcode, r27 = prefetched
// next word, LR = current dispatch entry, cr1/cr2 fields steering the
// shared tail's bgtctr/bgelr three-way jump), so the first dispatch must go
// through the ROM's own fetch/rlwimi/mtlr/bgelr sequence rather than a
// hand-rolled bctr.
static VOID
RomWriteEmulatorEntryRoutine (
    IN UINT8*  Rom,
    IN UINT32  Offset
    )
{
    static const UINT32 Words[32] = {
        0x7c3f0b78,  // mr r31,r1
        0x3bff1000,  // addi r31,r31,0x1000     r31 = KDP + 0x1000 = ed
        0x3fa040b8,  // lis r29,0x40b8          r29 = opcode dispatch table
        0x3fc040b6,  // lis r30,0x40b6          r30 = emulator base
        0x3f004080,  // lis r24,0x4080          r24 = 68K PC - 2
        0x63180028,  // ori r24,r24,0x28        = 0x40800028
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
        0x7ce04120,  // mtcrf 0x04,r7         dispatch cond field (LT clear)
        0x38e00000,  // li r7,0
        0x7ce02120   // mtcrf 0x02,r7         cr1 = 0 (shared-tail guard)
    };
    UINT32 I;
    for (I = 0; I < sizeof(Words) / sizeof(Words[0]); I++) {
        RomPatchWriteWord32(Rom, Offset + I * 4, Words[I]);
    }
    {
        // Final branch to the ROM's own DR-loop body. Offset is a ROM file
        // offset; the guest runs the ROM copy at 0x40800000.
        UINT32 Pc = 0x40800000u + Offset +
                    (UINT32)(sizeof(Words) / sizeof(Words[0])) * 4;
        INT32 Delta = (INT32)(0x40B66080u - (Pc + 4));
        RomPatchWriteWord32(
            Rom,
            Offset + sizeof(Words) / sizeof(Words[0]) * 4,
            0x48000000u | ((UINT32)Delta & 0x03FFFFFFu));
    }
}

// Install the ed.v[0x814] dispatch helper at ROM + 0x36f7c0. The DR emulator's
// state machine (0x40b6d114) calls it through the 68K-mode glue via blrl
// (CTR = dispatch entry, LR = return into the state machine). It sets cr2.GE
// so the glue's `bgelr cr2` returns into the state machine, then bctr's to the
// dispatch entry.
static VOID
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

// Install the ed.v[0x818] opcode-class helper at ROM + 0x36f7d0. The DR
// emulator's shared dispatch tail enters it with two different calling
// conventions: the lhz-class site (0x40b6c530 -> 0x40b6ca44/0x40b6ca48 bctr,
// or 0x40b6c534 bsoctrl) tail-jumps via CTR with the handler address already
// built in r29 -- continue to it; the rlwinm-class site (0x40b6c63c ->
// 0x40b6c648 bctrl) calls it as a function and afterwards merges r5 into r29
// (rlwimi r29,r5,3), so a plain blr -- leaving r5 as the next ext word loaded
// at 0x40b6c644 -- reproduces the threaded flow exactly.
static VOID
RomWriteEmulatorClassHelper (
    IN UINT8*  Rom,
    IN UINT32  Offset
    )
{
    static const UINT32 Words[8] = {
        0x7d2042a6,  // mflr r9
        0x3d4040b6,  // lis r10,0x40b6
        0x614ac64c,  // ori r10,r10,0xc64c    LR == bctrl return site?
        0x7c095000,  // cmpw r9,r10
        0x40820008,  // bne +8
        0x4e800020,  // blr                   call: return, keep r5
        0x7fa903a6,  // mtctr r29             jump: go to handler in r29
        0x4e800420   // bctr
    };
    UINT32 I;
    for (I = 0; I < sizeof(Words) / sizeof(Words[0]); I++) {
        RomPatchWriteWord32(Rom, Offset + I * 4, Words[I]);
    }
}
#endif // FAITHFUL handoff builders retired

// PHASE A.5: KernelData hardware-field provisioning.
//
// The nanokernel owns the KernelData page (LA_KernelData = 0x68FFE000) and
// initializes nearly all of it during its own boot; the emulator's job is
// only the hardware-dependent inputs the ROM cannot discover by itself.
// Those are delivered through two channels that already exist:
//   - ConfigInfo physical RAM base (boot struct + 0x360 = 0, patched above);
//   - XLM PVR / bus-clock globals (0x281C / 0x2820), SheepShaver's
//     sanctioned channel for emulator-provided CPU identity.
// Any further field-fill must be evidence-based: blind writes into this
// page corrupt live nanokernel data structures. The interpreter's KDPROF
// profiler records every load the NK performs from the page during boot
// and dumps the consumed offsets at the 68K handoff; seeds for those
// offsets belong here once identified. This function validates the page
// is reachable and snapshots its initial contents so the profile output
// can be compared against the pre-boot state.
static VOID
BootSeedKernelDataHardware (
    VOID
    )
{
    UINT8 B0 = PpcReadGuestByte(0x68FFE000);
    UINT8 B1 = PpcReadGuestByte(0x68FFE001);
    UINT8 B2 = PpcReadGuestByte(0x68FFE002);
    UINT8 B3 = PpcReadGuestByte(0x68FFE003);
    if ((B0 | B1 | B2 | B3) == 0 && PpcReadGuestByte(0x68FFEFF0) == 0 &&
        PpcReadGuestByte(0x68FFEFF1) == 0 &&
        PpcReadGuestByte(0x68FFEFF2) == 0 &&
        PpcReadGuestByte(0x68FFEFF3) == 0) {
        // Both ends of the page read as zero. That is the expected fresh
        // state (the page lives in the zeroed NK system area), but confirm
        // the region is writable so later runtime seeds will stick.
        BootWriteWord32(0x68FFEFF4, 0xA5A5A5A5);
        if (PpcReadGuestByte(0x68FFEFF4) != 0xA5 ||
            PpcReadGuestByte(0x68FFEFF5) != 0xA5 ||
            PpcReadGuestByte(0x68FFEFF6) != 0xA5 ||
            PpcReadGuestByte(0x68FFEFF7) != 0xA5) {
            Print(L"KernelData page NOT writable: A.5 runtime seeds will fail\n");
            return;
        }
        BootWriteWord32(0x68FFEFF4, 0);
    }
    Print(L"KernelData page OK (LA_KernelData 0x68FFE000, head %02x%02x%02x%02x). "
          L"PVR/bus-clock delivered via XLM [281C]/[2820]; "
          L"field-fill awaits KDPROF offsets\n",
          B0, B1, B2, B3);
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

// ---------------------------------------------------------------------------
// PHASE A (SheepShaver-faithful nanokernel cold-boot neutralization).
//
// With MSR[DR] clear (A.1) the NK runs its real cold path: SR/BAT/SDR setup,
// page-table clear, PMDT ("RAM descriptor") build, BAT/SR loads, performance-
// monitor SPR probes and PVR-dependent CPU tables. The interpreter has no
// MMU, so every hardware-initialization stage is neutralized exactly the way
// SheepShaver's patch_nanokernel_boot() does: pattern-matched sites in the
// 0x310000..0x320000 NK image are NOPed or redirected to emulator-provided
// values (XLM). Pattern addresses below were verified against the Mac OS
// 9.2.2 "Mac OS ROM" image; a missed match logs a warning and leaves that
// stage intact rather than aborting the whole patch set.
//
// Returns the number of patches applied.
// ---------------------------------------------------------------------------
#define POWERPC_NOP  0x60000000u
#define POWERPC_BLR  0x4E800020u

// Peek a big-endian word from the host-side ROM image without modifying it.
static UINT32
RomPeekWord32 (
    IN UINT8* Rom,
    IN UINT32 Offset
    )
{
    return ((UINT32)Rom[Offset] << 24) |
           ((UINT32)Rom[Offset + 1] << 16) |
           ((UINT32)Rom[Offset + 2] << 8) |
           (UINT32)Rom[Offset + 3];
}

// Find a byte pattern inside [Lo, Hi) of the flat ROM image; 0 if absent.
static UINT32
RomFindBytes (
    IN UINT8*       Rom,
    IN const UINT8* Pat,
    IN UINTN        Len,
    IN UINT32       Lo,
    IN UINT32       Hi
    )
{
    UINT32 Off;
    for (Off = Lo; Off + Len <= Hi; Off++) {
        if (CompareMem(Rom + Off, Pat, Len) == 0) {
            return Off;
        }
    }
    return 0;
}

static UINTN
BootPatchNkBootSequence (
    IN UINT8* Rom
    )
{
    UINTN Applied = 0;
    UINT32 Base;
    static const UINT8 PatPvr1[]  = {0x7d,0x9f,0x42,0xa6};
    static const UINT8 PatSprg3[] = {0x39,0x21,0x03,0x60,0x7d,0x33,0x43,0xa6,
                                     0x39,0x01,0x04,0x20};
    static const UINT8 PatPvr2[]  = {0x7e,0xff,0x42,0xa6,0x56,0xf7,0x84,0x3e};
    static const UINT8 PatPvr4[]  = {0x7d,0x3f,0x42,0xa6,0x55,0x29,0x84,0x3e};
    static const UINT8 PatPmck[]  = {0x7e,0x58,0xeb,0xa6,0x7e,0x53,0x90,0xf8,
                                     0x7e,0x78,0xea,0xa6};

    // Don't read PVR (#1): mfspr r12,PVR -> lwz r12,XLM_PVR.
    Base = RomFindBytes(Rom, PatPvr1, sizeof(PatPvr1), 0x3103B0, 0x3108B0);
    if (Base != 0) {
        RomPatchWriteWord32(Rom, Base, 0x81800000u | PPC_XLM_PVR_OFFSET);
        Applied++;
        Print(L"  NKPATCH pvr1 @0x%x -> lwz r12,XLM_PVR\n", Base);
    } else {
        Print(L"  NKPATCH pvr1: pattern NOT found\n");
    }

    // Don't set SPRG3 (second site): NOP the mtsprg.
    Base = RomFindBytes(Rom, PatSprg3, sizeof(PatSprg3), 0x310000, 0x314000);
    if (Base != 0) {
        RomPatchWriteWord32(Rom, Base + 4, POWERPC_NOP);
        Applied++;
        Print(L"  NKPATCH sprg3-2nd @0x%x\n", Base + 4);
    } else {
        Print(L"  NKPATCH sprg3-2nd: pattern NOT found\n");
    }

    // Don't read PVR (#2): up to two occurrences; mfspr r23,PVR ->
    // lwz r23,XLM_PVR (the following rlwinm only masks version bits).
    Base = RomFindBytes(Rom, PatPvr2, sizeof(PatPvr2), 0x310000, 0x320000);
    while (Base != 0) {
        RomPatchWriteWord32(Rom, Base, 0x82E00000u | PPC_XLM_PVR_OFFSET);
        Applied++;
        Print(L"  NKPATCH pvr2 @0x%x\n", Base);
        Base = RomFindBytes(Rom, PatPvr2, sizeof(PatPvr2), Base + 4, 0x320000);
    }

    // Don't read PVR (#4): mfspr r9,PVR -> lwz r9,XLM_PVR.
    Base = RomFindBytes(Rom, PatPvr4, sizeof(PatPvr4), 0x310000, 0x320000);
    if (Base != 0) {
        RomPatchWriteWord32(Rom, Base, 0x81200000u | PPC_XLM_PVR_OFFSET);
        Applied++;
        Print(L"  NKPATCH pvr4 @0x%x\n", Base);
    }

    // ---------------------------------------------------------------------
    // FAITHFUL MMU BOOT (DingusPPC-faithful, 2026-08): the four MMU-related
    // neutralizations that SheepShaver's patch_nanokernel_boot() applies here
    // (SDR1 read, page-table clear/tlbie, PMDT/RAM-descriptor builder, and
    // the final SR-load helper) are REMOVED so the nanokernel arms its own
    // real translation (SDR1 + page table + BATs + MSR[IR]/[DR]) and the
    // interpreter's full PPC MMU engages. Only the CPU-identity/feature
    // probes (PVR, SPRG3, PM SPRs) remain neutralized — those burn answers
    // the interpreter must feed and are unrelated to address translation.
    // ---------------------------------------------------------------------
    // (Faithful path: the NK's MMU arming sites below are left untouched.)

    // Don't check performance monitor: NOP every mtspr/mfspr pair for the
    // PM SPRs (952 mmcr0 .. 959 sda) inside the probe block.
    Base = RomFindBytes(Rom, PatPmck, sizeof(PatPmck), 0x310000, 0x320000);
    if (Base != 0) {
        static const UINT32 SprList[8] = {952,953,954,955,956,957,958,959};
        UINTN K;
        for (K = 0; K < 8; K++) {
            UINT32 Spr = SprList[K];
            UINT32 Mt = 0x7E4003A6u | ((Spr & 0x1F) << 16) | ((Spr & 0x3E0) << 6);
            UINT32 Mf = 0x7E6002A6u | ((Spr & 0x1F) << 16) | ((Spr & 0x3E0) << 6);
            UINTN Of;
            for (Of = 0; Of < 64; Of++) {
                if (RomPeekWord32(Rom, Base + (UINT32)Of * 4) == Mt &&
                    RomPeekWord32(Rom, Base + (UINT32)Of * 4 + 8) == Mf) {
                    RomPatchWriteWord32(Rom, Base + (UINT32)Of * 4,     POWERPC_NOP);
                    RomPatchWriteWord32(Rom, Base + (UINT32)Of * 4 + 8, POWERPC_NOP);
                }
            }
        }
        Applied++;
        Print(L"  NKPATCH perf-monitor SPR pairs @0x%x\n", Base);
    } else {
        Print(L"  NKPATCH perf-monitor: pattern NOT found\n");
    }

    return Applied;
}

static EFI_STATUS
PpcPatchNewWorldRom (
    VOID
    )
{
    UINT8* Rom     = (UINT8*)g_BootContext.RomHostBuffer;
    UINT32 RomBase = (UINT32)g_BootContext.RomAddress;
    UINT32 Struct  = PPC_NEW_WORLD_ROM_BOOT_STRUCT_OFFSET;
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

    // Relocate the ROM's 68K jump tables (see RomRelocateJumpTables): the
    // image ships entries as 0xFFxxxxxx ROM-relative words that Apple's
    // loader rebases; without it, NK thunk dispatchers read unmapped
    // 0xFFxxxxxx addresses and propagate sentinel garbage into every
    // downstream MixedMode call.
    RomRelocateJumpTables(Rom, g_BootContext.RomSize, RomBase);

    // 68K boot-driver relocation gate. The driver at guest 0x4080AA36 calls
    // DR service 0x116 (a6 = return 0x4080AA3C) and, when D0 bit0 is SET,
    // relocates its image pointer and `jmp (pc,d3.l)`s into the low-RAM mirror
    // at 0xAA5A (guest 0x4080AA56-0x4080AA5A). Our boot never stages that
    // low-RAM copy, so the 68K marches through zeros. Turn the gate's
    // `btst.b #0,d0 / beq.s` (0x67 0x1A) into an unconditional `bra.s`
    // (0x60 0x1A) so the driver always continues in ROM at 0x4080AA5C.
    if (g_BootContext.RomSize >= 0xAA42 &&
        Rom[0xAA40] == 0x67 && Rom[0xAA41] == 0x1A) {
        Rom[0xAA40] = 0x60;   // beq.s +0x1A -> bra.s +0x1A
        Print(L"  68K patch: skip low-RAM relocation (0xAA40 beq->bra)\n");
    }

    // 68K boot HWInfo gate. On real hardware the Open Firmware trampoline
    // builds the IRP's HWInfo record and signs it with 'Hnfo' before the
    // nanokernel runs (powermac-rom InfoRecords.a: NKHWInfo.Signature at
    // IRP+0xF70). The 68K startup validates it at guest 0x4080AFBE
    // (cmpli.l #'Hnfo',D0 after a DR-emulator service call); when the
    // compare fails, bne.s diverts to an info-table scan that ends parked
    // in an idle loop at 0x4080ABE6 because no signed record exists.
    // We have no trampoline, so turn the failure branch into a NOP: flow
    // then always reaches cmp.w d0,d0 at 0x4080AFD6 which forces Z=1
    // ("HWInfo present") and continues through the healthy path.
    // NOTE: an earlier patch here rewrote 0xAFC4 `bne.s +0x0E` as `60 00`,
    // intending a NOP. On the 68K, a branch displacement byte of 0x00 is an
    // ESCAPE meaning "16-bit displacement follows", so `60 00` became
    // bra.w +$3030 into the middle of a data table at 0x4080DFF6 whose
    // trailing RTS popped a null return address. The 'HnoF' info block is
    // now fabricated at point-of-use (see m68k.c), so the original gate
    // logic works and the patch must NOT be applied.

    // Locate the `twui r31,0..2` kernel-trap table (SheepShaver's
    // find_rom_data range; verified at ROM + 0x36e8c0 in the standard image).
#if 0 // FAITHFUL: trap-table redirect retired; TrapBase no longer needed.
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
#endif
    // ---------------------------------------------------------------------
    // FAITHFUL HANDOFF (DingusPPC-faithful, 2026-08): the SheepShaver handoff
    // scaffolding below is REMOVED so the ROM keeps its ORIGINAL content and
    // the OS reaches its natural DR handoff via the real `twi r31,k` kernel-
    // trap / exception path (the injected trap-table redirect, entry routines,
    // fake 68K DR-context builders, and EMUL_OP markers are all retired). The
    // clean ROM holds `twi r31,0..15` constants at 0x36E8C0 and NOPs at
    // 0x36F700/0x36F900; those sites must not be overwritten.
    // ---------------------------------------------------------------------
#if 0 // FAITHFUL: trap-table redirect + injected entry routines retired
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

    // The 68K DR-emulator entry + ed.v[0x814]/ed.v[0x818] dispatch helpers
    // (free NOP region at ROM + 0x36f700..0x36f8fc).
    RomWriteEmulatorEntryRoutine(Rom, 0x36F700);
    RomWriteEmulatorDispatchHelper(Rom, 0x36F7C0);
    RomWriteEmulatorClassHelper(Rom, 0x36F7D0);
#endif // FAITHFUL handoff retired

    // The ROM's control-flow dispatch glue bakes a family of
    // `rlwimi r29,...` words that force bit 20 (0x100000) of the
    // dispatch-table base to 0: they copy the low bit of the 68K PC (held
    // in r24/r1/r3/...) into bit 20 of the dispatch address. For valid 68K
    // code (always word-aligned) that bit is 0, which zeroes bit 20 of the
    // opcode-table base. The dispatch table lives at 0x40b80000 (bit 20 = 1),
    // so every control-flow re-dispatch would land 0x100000 off (in the "kckc"
    // data region) and the boot walks garbage. On real hardware the table base
    // has bit 20 = 0 and the rlwimi is a harmless no-op; with our base every
    // rlwimi whose bitmask covers bit 20 must be neutralised. The encoding
    // family varies (SH/MB/ME span, RS register, mask width: 0x531DA2D6 /
    // 0x501DA2D6 / 0x537DA2D6 / 0x509D1B78 / 0x50DD1B78 ...), so decode the
    // instruction rather than matching a byte pattern.
    {
        UINT32 Count = 0;
        for (I = 0x360000; I + 4 <= 0x380000; I += 4) {
            const UINT32 W = ((UINT32)Rom[I] << 24) |
                             ((UINT32)Rom[I + 1] << 16) |
                             ((UINT32)Rom[I + 2] << 8) | Rom[I + 3];
            const UINT32 MB = (W >> 6) & 0x1F;
            const UINT32 ME = (W >> 1) & 0x1F;
            const int CoversBit20 =
                (MB <= ME) ? (MB <= 11 && 11 <= ME) : (11 >= MB || 11 <= ME);
            if (((W >> 26) & 0x3F) == 20 &&   // rlwimi
                ((W >> 16) & 0x1F) == 29 &&   // RA = r29 (table base)
                CoversBit20) {
                RomPatchWriteWord32(Rom, I, 0x60000000);
                Count++;
            }
        }
        Print(L"68K emulator: neutralised %u rlwimi dispatch-bit-20 words\n", Count);
    }

// EMUL_OP marker slots (additive; the SheepShaver trap-table redirect and
    // entry routines stay retired -- only the dispatch-table slots for the
    // EMUL_OP extended opcodes are armed). The DR's 68K opcode table maps
    // (0xFE40+selector) to an 8-byte slot at ROM + 0x3FF200; write
    // `PPC_EMUL_OP_MARKER | (selector+3)` ("mulli r0,r0,n") so a boot device
    // open dispatches to the host device layer (the interpreter's EMUL_OP
    // intercept) and resumes the DR loop at 0x366084. The slots are cold
    // until a ROM site executes a 0xFE4x opcode.
    {
        UINT32 Entry = PPC_NEW_WORLD_ROM_EMUL_OP_ENTRY_OFFSET;
        for (I = 0; I < PPC_OP_MAX; I++) {
            RomPatchWriteWord32(Rom, Entry + I * 8, PPC_EMUL_OP_MARKER | (I + 3));
        }
        Print(L"EMUL_OP marker slots armed: %u opcodes at ROM+0x%X\n",
              PPC_OP_MAX, Entry);
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
    BootWriteWord32(PPC_XLM_PVR_OFFSET,         0x000C0000);       // PowerPC 7400 (G4)
    BootWriteWord32(PPC_XLM_BUS_CLOCK_OFFSET,   100000000);      // 100 MHz bus clock

    // NK context-save signature: the ROM's task-context save routine at
    // 0x40804640 movem's all registers to the 0xC30 block and gates on a
    // magic longword at 0xDB0 before proceeding; without it the boot parks
    // in the bra-self deadloop at 0x408047AE. Nothing in the paths we run
    // writes it, so seed the same magic the ROM data table carries.
    BootWriteWord32(0x00000DB0, 0x5A932BC7);

    // PHASE A: neutralize the NK cold-boot hardware init (SR/BAT/SDR, page
    // table clear, PMDT builder, perf-monitor probes; PVR reads answered
    // from XLM) so initialization completes cleanly with flat memory.
    {
        UINTN NkPatched = BootPatchNkBootSequence(Rom);
        Print(L"NK boot sequence patched: %u sites\n", (UINT32)NkPatched);
    }

    // PHASE A.5: validate the KernelData page and snapshot its state.
    BootSeedKernelDataHardware();

    Print(L"68K emulator (FAITHFUL): LA_EmulatorCode 0x%08x LA_DispatchTable 0x%08x "
          L"trap table kept at ROM+0x36E8C0 (natural twi path)\n",
          RomBase + 0x360000, RomBase + 0x380000);

    // Boot-proc warm-reboot KCall function id. After the nanokernel replaces
    // itself it "returns to the boot proc"; the boot-proc tail at ROM+0x3126E8
    // then issues `li r3,0xff / mtlr r4 / blrl` into the twi kernel-trap table.
    // The trap dispatches to the 0x700 KCall handler (KCallTbl[0] at 0x40B13BF8)
    // which branches on r3: r3==0 is the DR-emulator scheduler entry
    // (0x40B13C38 -> 0x40B12CB0), while r3=0xff resolves to the soft-interrupt
    // path (0x40B12AB4) that returns to 0x40B126F4, re-entering the boot lock
    // and re-running NK init -- the spurious warm-reboot loop. On real hardware
    // the cold-launch path (InitEmulator, Init.s) enters the trap table with
    // r3==0, so the emulator is launched through the r3==0 branch. Reproduce
    // that faithful state: devirtuate the tail's own function id 0xff -> 0 so
    // the KCall dispatches to the DR-emulator scheduler instead of returning.
    {
        UINT32 BootTailOff = 0x3126E8; // ROM+0x3126E8 == guest 0x40B126E8
        UINT32 TailWord = (UINT32)Rom[BootTailOff] << 24 |
                          (UINT32)Rom[BootTailOff + 1] << 16 |
                          (UINT32)Rom[BootTailOff + 2] << 8 |
                          (UINT32)Rom[BootTailOff + 3];
        if ((TailWord & 0xFFFF0000u) == 0x38600000u) { // li r3,<simm16>
            UINT32 NewWord = (TailWord & 0xFFFF0000u) | 0x0000;
            RomPatchWriteWord32(Rom, BootTailOff, NewWord);
            Print(L"68K emulator: boot-proc tail KCall id 0xff->0 at ROM+0x3126E8 "
                  L"(li r3,0) to take DR-emulator scheduler r3==0 path\n");
        }
    }

    return EFI_SUCCESS;
}

// Stage the classic Mac OS 68K System data fork into guest RAM so the DR
// handoff (which jumps to low-RAM 0x0) can relocate the real 68K boot stub
// there. Reads the file through the in-emulator HFS reader into a host buffer,
// then bulk-copies it into the mapped nanokernel system area (guest
// 0x68000000-0x70000000) at PPC_OS_RUNTIME_GUEST_BASE, and records that guest
// base in the emulator boot-info block (offset +20) for the interpreter to find.
EFI_STATUS
EFIAPI
PpcStageOsRuntime (
    VOID
    )
{
    if (g_BootContext.OsRuntimeStaged) {
        return EFI_SUCCESS;
    }

    PPC_HFS_VOLUME_INFO HfsInfo;
    EFI_STATUS Status = PpcHfsGetVolumeInfo(&HfsInfo);
    if (EFI_ERROR(Status)) {
        Status = PpcHfsMount(NULL);
        if (EFI_ERROR(Status)) {
            return EFI_NOT_FOUND;
        }
    }

    PPC_HFS_ENTRY Sys;
    Status = PpcHfsOpenPath(PPC_HFS_SYSTEM_FILE_PATH, &Sys);
    if (EFI_ERROR(Status) || Sys.IsDirectory || Sys.Size == 0) {
        Print(L"OS runtime: 'System' not found on volume (%r)\n", Status);
        return EFI_NOT_FOUND;
    }

    UINTN FileSize = (UINTN)Sys.Size;
    if (FileSize > PPC_OS_RUNTIME_MAX_SIZE) {
        Print(L"OS runtime: System too large: %d bytes\n", (UINT64)FileSize);
        return EFI_LOAD_ERROR;
    }
    if (!g_BootContext.NkSystemAreaInstalled) {
        PpcInstallNkSystemArea();
    }

    VOID* Host = NULL;
    Status = BS->AllocatePool(EfiBootServicesData, FileSize, &Host);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    UINTN Got = FileSize;
    Status = PpcHfsReadFile(&Sys, Host, &Got);
    if (EFI_ERROR(Status) || Got != FileSize) {
        Print(L"OS runtime: failed to read System data fork: %r (got %d/%d)\n",
              Status, (UINT64)Got, (UINT64)FileSize);
        BS->FreePool(Host);
        return EFI_ERROR(Status) ? Status : EFI_LOAD_ERROR;
    }

    // Bulk copy host -> guest (big-endian byte order is preserved by a raw
    // byte copy; classic 68K data is stored as bytes in guest RAM).
    for (UINTN I = 0; I < FileSize; I++) {
        PpcWriteGuestByte(PPC_OS_RUNTIME_GUEST_BASE + (UINT32)I,
                          ((UINT8*)Host)[I]);
    }
    BS->FreePool(Host);

    // Record the staged guest base in the emulator boot-info block (+20) so
    // the interpreter's DR-handoff can relocate the 68K boot stub to low RAM.
    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET +
                        PPC_LOW_MEM_BOOTINFO_OSRUNTIME_OFFSET,
                    PPC_OS_RUNTIME_GUEST_BASE);

    g_BootContext.OsRuntimeStaged = TRUE;
    Print(L"OS runtime staged: System -> guest 0x%x (%d bytes)\n",
          PPC_OS_RUNTIME_GUEST_BASE, (UINT64)FileSize);
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

    // Stage the classic Mac OS 68K System data fork so the DR handoff can
    // relocate its boot stub into low RAM. Best-effort: failure just logs.
    PpcStageOsRuntime();

    g_BootContext.SystemReady = TRUE;
    g_BootContext.SystemBooting = TRUE;

    Print(L"System prepared: PC=0x%x MSR=0x%08x SRR0=0x%x SRR1=0x%x\n",
          g_PpcContext.Pc, g_PpcContext.Msr, g_PpcContext.Srr0, g_PpcContext.Srr1);
    Print(L"Boot info block written to low memory at 0x%x\n",
          PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_BOOTINFO_OFFSET);

    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Disk block-media self-test: exercise the EMUL_OP disk PRIME handler the way
// the guest's own HFS driver would -- build an IOParam7 buffer in guest
// memory, arm the DR register file (A0 = PB, D0 = read), dispatch
// PPC_OP_DISK_PRIME, and confirm the raw block served back is the boot
// volume's Master Directory Block (HFS 'BD' / HFS+ 'H+' signature). This
// locks in the "PRIME routed into the in-emulator HFS" contract so a later
// guest self-mount reads real filesystem bytes, not zeros.
// ---------------------------------------------------------------------------
#define PPC_PB_SCRATCH_GUEST_BASE (PPC_LOW_MEM_GUEST_BASE + 0x2000)
#define PPC_PB_SCRATCH_BUF_OFFSET (0x0004)

// Local big-endian word write used only to fabricate the guest IOParam for
// the disk media self-test (a host-side write, not a guest store).
static VOID
EmulWpTmp (
    IN UINT32 Addr,
    IN UINT32 Value
    )
{
    PpcWriteGuestByte(Addr + 0, (UINT8)(Value >> 24));
    PpcWriteGuestByte(Addr + 1, (UINT8)(Value >> 16));
    PpcWriteGuestByte(Addr + 2, (UINT8)(Value >> 8));
    PpcWriteGuestByte(Addr + 3, (UINT8)Value);
}

static VOID
PpcRunDiskMediaSelfTest (
    VOID
    )
{
    // Only meaningful once Block I/O media is present and the HFS reader can
    // mount the boot volume (the block device the PRIME path reads).
    PPC_HFS_VOLUME_INFO Vol;
    if (EFI_ERROR(PpcHfsGetVolumeInfo(&Vol)) || !Vol.Mounted) {
        Print(L"--- Disk media self-test: skipped (no mounted HFS volume) ---\n");
        return;
    }

    // IOParam7 at scratch base: ioRefNum +14, ioBuffer +22, ioReqCount +26,
    // ioPosOffset +36. Read 512 bytes from media offset 0 (the MDB for a
    // raw-at-0 volume; absolute offset 0 regardless of embedding).
    UINT32 Pb = PPC_PB_SCRATCH_GUEST_BASE;
    UINT32 Buf = Pb + PPC_PB_SCRATCH_BUF_OFFSET;

    EmulWpTmp(Pb + 14, 1);                    // ioRefNum = boot drive
    EmulWpTmp(Pb + 22, Buf);                  // ioBuffer
    EmulWpTmp(Pb + 26, 512);                  // ioReqCount
    EmulWpTmp(Pb + 30, 0);                    // ioActCount (filled by PRIME)
    EmulWpTmp(Pb + 36, (UINT32)Vol.VolumeBase); // ioPosOffset = volume base (MDB)

    PpcSetGprValue(16, Pb);                   // A0 = PB (r16)
    PpcSetGprValue(8, 3);                     // D0 = 3 (read) (r8)
    EmulOpDispatch(PPC_OP_DISK_PRIME);

    UINT32 Err = PpcGetGprValue(8) & 0xFFFFFFFF;
    UINT32 Act = 0xFFFFFFFF;
    {
        // ioActCount = EmulRl(Pb + 30)
        Act = ((UINT32)PpcReadGuestByte(Pb + 30) << 24) |
              ((UINT32)PpcReadGuestByte(Pb + 31) << 16) |
              ((UINT32)PpcReadGuestByte(Pb + 32) << 8) |
              (UINT32)PpcReadGuestByte(Pb + 33);
    }
    BootSelfTestCheck(Err == 0, L"disk PRIME (guest PBRead boot drive) returns noErr");
    BootSelfTestCheck(Act == 512, L"disk PRIME ioActCount = ioReqCount (512)");

    // Best-effort: locate the HFS MDB signature ('BD'/'H+') served through the
    // PRIME path. Classic Mac install discs put Apple driver blocks ahead of
    // the MDB, and CD discs may embed the HFS overlay well into the media, so
    // this is informational rather than a pass/fail assertion -- the hard
    // PRIME contract (noErr + ioActCount) is already asserted above.
    BOOLEAN FoundMdb = FALSE;
    UINT32 MdbAt = 0;
    for (UINTN Off = 0; Off < 0x80000u && !FoundMdb; Off += 512) {
        EmulWpTmp(Pb + 36, (UINT32)Vol.VolumeBase + (UINT32)Off); // ioPosOffset
        PpcSetGprValue(16, Pb);                                   // A0 = PB
        PpcSetGprValue(8, 3);                                     // D0 = PBRead
        EmulOpDispatch(PPC_OP_DISK_PRIME);
        if ((PpcGetGprValue(8) & 0xFFFFFFFF) != 0) {
            break;                              // read error: stop scanning
        }
        UINT8 S0 = PpcReadGuestByte(Buf + 0);
        UINT8 S1 = PpcReadGuestByte(Buf + 1);
        if ((S0 == 0x42 && S1 == 0x44) || (S0 == 0x48 && S1 == 0x2B)) {
            FoundMdb = TRUE;
            MdbAt = (UINT32)Vol.VolumeBase + (UINT32)Off;
        }
    }
    Print(L"  [info] disk PRIME MDB '%s' %s (volume base 0x%x, device %d); "
          L"PRIME returns noErr + ioActCount=%u\n",
          FoundMdb ? L"BD/H+" : L"(not found)",
          FoundMdb ? L"served at" : L"in scan window",
          (UINT32)Vol.VolumeBase, (UINTN)Vol.DeviceIndex, (UINT32)Act);
    if (FoundMdb) {
        Print(L"  [info]   -> media byte 0x%x\n", MdbAt);
    }

    // Restore a clean DR register file (A0/D0 no longer describe a PB).
    PpcSetGprValue(16, 0);
    PpcSetGprValue(8, 0);
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

    // ROM window access enforcement. Old World and demo images are mapped
    // read-only, so guest stores must be rejected. The New World window is
    // deliberately writable -- the nanokernel builds its HTAB, kernel data
    // page, EWA and IRP inside it (see PpcLoadSystemRom) -- so the test
    // instead verifies the write lands and is restored.
    {
        UINT8 B0 = PpcReadGuestByte(RomBase + 0);
        PpcWriteGuestByte(RomBase + 0, (UINT8)(B0 ^ 0xFF));
        BOOLEAN Changed = PpcReadGuestByte(RomBase + 0) == (UINT8)(B0 ^ 0xFF);
        PpcWriteGuestByte(RomBase + 0, B0);
        if (g_BootContext.RomType == PPC_ROM_TYPE_NEW_WORLD) {
            BootSelfTestCheck(
                Changed && PpcReadGuestByte(RomBase + 0) == B0,
                L"New World ROM window writable + restorable (HTAB/KDP backing)");
        } else {
            BootSelfTestCheck(
                !Changed,
                L"ROM rejects guest writes (read-only)");
        }
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

    // Exercise the EMUL_OP disk PRIME block-media path (guest PBRead through
    // the in-emulator HFS volume) so the routed PRIME is validated.
    PpcRunDiskMediaSelfTest();

    Print(L"--- Boot self-test complete: %d passed, %d failed ---\n",
          g_BootTestPasses, g_BootTestFailures);

    return (g_BootTestFailures == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;
}

// ---------------------------------------------------------------------------
// System files & drivers (classic Mac OS System Folder support)
// ---------------------------------------------------------------------------

static VOID
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
static VOID
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
        } else if (!g_BootContext.SystemPresent || !g_BootContext.FinderPresent) {
            // The boot volume's System Folder is incomplete (holds e.g. only
            // Extensions / Mac OS ROM staged on the ESP): System and Finder
            // live on the attached Mac OS disc, so merge them in from HFS.
            Print(L"System Folder on boot volume incomplete (System=%d Finder=%d): "
                  L"merging from Mac OS disc\n",
                  g_BootContext.SystemPresent, g_BootContext.FinderPresent);
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

    // The guest (nanokernel/DR) wipes the first 8 KB of low RAM during boot,
    // so the 'EFI!' low-memory marker written by PpcPrepareSystemForBoot is
    // gone by the time we stage files. Re-arm it here so the post-staging
    // self-test really validates the staging paths (which must never touch
    // low memory) rather than reflecting the guest's deliberate wipe.
    BootWriteWord32(PPC_LOW_MEM_GUEST_BASE + PPC_LOW_MEM_MAGIC_OFFSET, 0x45464921u);

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
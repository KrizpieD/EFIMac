#ifndef __PPC_HFS_H__
#define __PPC_HFS_H__

#include <efi.h>

// Classic Mac OS filesystem reader (HFS and HFS+), the in-emulator port of
// tools/hfs_read.py. Volumes are located on the raw UEFI block devices
// enumerated by the hardware abstraction layer (the Mac OS install/system
// disc attached as a -drive under QEMU). Detection matches the verified
// Python reader: a raw volume at offset 0 wins, otherwise Apple Partition Map
// partitions of type Apple_HFS are tried, otherwise the largest plausible MDB
// found on a 2048-byte boundary is chosen.

#define PPC_HFS_NAME_MAX        31      // HFS_NAMELEN
#define PPC_HFS_MAX_FILES       8192    // catalog file entries
#define PPC_HFS_MAX_DIRS        4096    // catalog folder entries
#define PPC_HFS_MAX_EXTENTS     8       // hfsplus_extent_rec / max extent pairs
#define PPC_HFS_MAX_OVF         512     // extents overflow records (data fork)

// Catalog record types (hfs_common.h)
#define PPC_HFS_CDR_DIR         0x01    // folder
#define PPC_HFS_CDR_FIL         0x02    // file
#define PPC_HFS_CDR_THD         0x03    // folder thread
#define PPC_HFS_CDR_FTH         0x04    // file thread

// Volume formats
typedef enum {
    PPC_HFS_KIND_NONE    = 0,
    PPC_HFS_KIND_CLASSIC,               // "BD" (HFS)
    PPC_HFS_KIND_PLUS                   // "H+" (HFS+)
} PPC_HFS_KIND;

// A single catalog entry (file or directory) resolved from the B-tree.
typedef struct {
    BOOLEAN IsDirectory;
    UINT32  Id;                         // FlNum (file) or DirID (folder)
    UINT32  ParentId;                   // ParID (catalog key)
    CHAR16  Name[PPC_HFS_NAME_MAX + 1]; // MacRoman/Latin-1 or UTF-16BE decoded
    UINT64  Size;                       // logical data-fork size
    UINT32  ExtentCount;                // number of (block,count) pairs
    UINT32  Extents[PPC_HFS_MAX_EXTENTS * 2]; // Block,Count,Block,Count...
} PPC_HFS_ENTRY;

// Mounted volume geometry and catalog totals.
typedef struct {
    BOOLEAN      Mounted;
    UINTN        DeviceIndex;           // block device that holds the volume
    UINTN        MediaBlockSize;        // media sector size in bytes
    UINT64       DeviceBytes;           // total media bytes
    PPC_HFS_KIND Kind;
    UINTN        VolumeBase;            // byte offset of the volume MDB
    UINTN        VolumeBlockSize;       // allocation block size (HFS) / block size (HFS+)
    UINTN        TotalBytes;            // computed volume size
    CHAR16       VolumeName[PPC_HFS_NAME_MAX + 1];
    UINTN        FileCount;             // catalog file entries
    UINTN        DirCount;              // catalog folder entries
} PPC_HFS_VOLUME_INFO;

/**
  Scan the enumerated block devices for an HFS or HFS+ volume and build its
  catalog B-tree in memory. Re-mounting replaces any previous volume.
  @param[out] Info  Volume geometry (may be NULL)
  @retval EFI_SUCCESS         Volume mounted
  @retval EFI_NOT_FOUND       No HFS/HFS+ volume found on any device
  @retval EFI_NOT_READY       Block I/O not available
**/
EFI_STATUS
EFIAPI
PpcHfsMount (
    OUT PPC_HFS_VOLUME_INFO* Info
    );

/**
  Get the mounted volume geometry.
  @param[out] Info  Volume geometry structure to fill
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcHfsGetVolumeInfo (
    OUT PPC_HFS_VOLUME_INFO* Info
    );

/**
  Probe the mounted volume for a bootable classic Mac OS System Folder.
  @param[out] SystemFolderPresent  TRUE if a "System Folder" exists at the root
  @param[out] SystemPresent        TRUE if "System Folder:System" exists
  @param[out] FinderPresent        TRUE if "System Folder:Finder" exists
  @param[out] MacOsRomPresent      TRUE if "System Folder:Extensions:Mac OS ROM" exists
  @param[out] SystemFolderId       DirID of the System Folder (0 if absent)
  @retval EFI_SUCCESS              Volume mounted and probe completed
  @retval EFI_NOT_READY            No volume mounted
**/
EFI_STATUS
EFIAPI
PpcHfsProbeBootFiles (
    OUT BOOLEAN* SystemFolderPresent,
    OUT BOOLEAN* SystemPresent,
    OUT BOOLEAN* FinderPresent,
    OUT BOOLEAN* MacOsRomPresent,
    OUT UINT32*  SystemFolderId
    );

/**
  List the immediate children (files and directories) of a folder.
  @param[in]  DirId    Folder DirID (2 = root)
  @param[out] Entries  Buffer to receive the child entries
  @param[in,out] Count Capacity on entry; number of children written on exit.
                       If the capacity is smaller than the child count, only
                       the first Count children are written and
                       EFI_BUFFER_TOO_SMALL is returned.
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcHfsListChildren (
    IN  UINT32        DirId,
    OUT PPC_HFS_ENTRY* Entries,
    IN  OUT UINTN*    Count
    );

/**
  Resolve a Mac path (e.g. "System Folder:System" or "System Folder\\Finder")
  into a catalog entry. Case-insensitive; both ":" and "\" separate segments.
  @param[in]  Path   Path to resolve
  @param[out] Entry  Resolved entry (file or directory)
  @retval EFI_SUCCESS          Entry resolved
  @retval EFI_NOT_FOUND        Path not found
  @retval EFI_NOT_READY        No volume mounted
**/
EFI_STATUS
EFIAPI
PpcHfsOpenPath (
    IN  CHAR16*      Path,
    OUT PPC_HFS_ENTRY* Entry
    );

/**
  Read the data fork of a file into a buffer.
  @param[in]  Entry    File entry (IsDirectory must be FALSE)
  @param[out] Buffer   Destination buffer
  @param[in,out] Size  Capacity on entry; bytes written on exit
  @retval EFI_SUCCESS          File read
  @retval EFI_BUFFER_TOO_SMALL Buffer too small for the full file (partial read)
  @retval EFI_INVALID_PARAMETER  Entry is a directory or Buffer is NULL
**/
EFI_STATUS
EFIAPI
PpcHfsReadFile (
    IN  PPC_HFS_ENTRY* Entry,
    OUT VOID*          Buffer,
    IN  OUT UINTN*     Size
    );

/**
  Mount a volume (if any) and verify the reader against it: catalog parse,
  root listing, and (when a System Folder is present) System file readback.
  @retval EFI_SUCCESS       Mount + core checks passed
  @retval EFI_LOAD_ERROR    Mount failed or core checks failed
  @retval EFI_NOT_FOUND     No HFS volume present to test
**/
EFI_STATUS
EFIAPI
PpcHfsRunSelfTest (
    VOID
    );

#endif // __PPC_HFS_H__

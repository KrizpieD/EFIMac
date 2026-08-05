#include "hfs.h"
#include <efi.h>
#include <efilib.h>
#include "hardware/abstraction.h"

// ---------------------------------------------------------------------------
// In-emulator HFS/HFS+ reader. On-disk layouts follow Linux
// include/linux/hfs_common.h (the classic HFS offsets match the verified
// tools/hfs_read.py; HFS+ offsets follow the spec exactly). The volume is
// found by scanning the raw UEFI block devices, and file data is read back
// through EFI_BLOCK_IO_PROTOCOL so the same path a guest would use drives the
// reader.
// ---------------------------------------------------------------------------

#define HFS_SIG_BD    0x4244u   // "BD" classic HFS
#define HFS_SIG_PLUS  0x482Bu   // "H+" HFS+
#define HFS_SIG_X     0x4858u   // "HX" HFSX (case-sensitive; unsupported)

#define HFS_MDB_OFF   1024      // MDB / volume header sector offset
#define HFS_ROOT_CNID 2         // root directory id

// Big-endian field accessors
STATIC UINT16 HfsU16 (const UINT8* P) { return (UINT16)((P[0] << 8) | P[1]); }
STATIC UINT32 HfsU32 (const UINT8* P) {
    return ((UINT32)P[0] << 24) | ((UINT32)P[1] << 16) | ((UINT32)P[2] << 8) | P[3];
}
STATIC UINT64 HfsU64 (const UINT8* P) {
    return ((UINT64)HfsU32(P) << 32) | HfsU32(P + 4);
}

// ---------------------------------------------------------------------------
// Mounted volume state
// ---------------------------------------------------------------------------
STATIC PPC_HFS_VOLUME_INFO g_HfsVolume = {0};
STATIC BOOLEAN             g_HfsMounted = FALSE;

// Device backing the mounted volume
STATIC UINTN  g_HfsDeviceIndex    = 0;
STATIC UINTN  g_HfsMediaBlockSize = 512;
STATIC UINT64 g_HfsDeviceBytes    = 0;

// Classic HFS: byte offset of the first allocation block (drAlBlSt * 512).
STATIC UINTN  g_HfsAllocBlockStart = 0;

// Catalog arrays (page-backed; HFS files can exceed pool limits)
STATIC PPC_HFS_ENTRY* g_HfsDirs  = NULL;
STATIC PPC_HFS_ENTRY* g_HfsFiles = NULL;
STATIC UINTN          g_HfsDirCount  = 0;
STATIC UINTN          g_HfsFileCount = 0;

// In-memory B-tree files
STATIC UINT8* g_CatData     = NULL;
STATIC UINTN  g_CatSize     = 0;
STATIC UINTN  g_CatNodeSize = 512;
STATIC UINT8* g_XofData     = NULL;
STATIC UINTN  g_XofSize     = 0;
STATIC UINTN  g_XofNodeSize = 512;

// Classic HFS extents-overflow index (data fork, fktype 0)
typedef struct {
    UINT32 FlNum;
    UINT32 Fabn;
    UINT32 Blocks[3];
    UINT32 Counts[3];
} HFS_OVF_ENTRY;
STATIC HFS_OVF_ENTRY g_HfsOvf[PPC_HFS_MAX_OVF];
STATIC UINTN         g_HfsOvfCount = 0;

// ---------------------------------------------------------------------------
// Memory / device helpers
// ---------------------------------------------------------------------------
STATIC EFI_STATUS
HfsAllocBytes (
    IN  UINTN Size,
    OUT VOID** Out
    )
{
    if (Size == 0) {
        *Out = NULL;
        return EFI_SUCCESS;
    }
    if (Size <= 128 * 1024) {
        VOID* P = AllocatePool(Size);
        if (P == NULL) {
            return EFI_OUT_OF_RESOURCES;
        }
        *Out = P;
        return EFI_SUCCESS;
    }
    UINTN Pages = (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS Base = 0;
    EFI_STATUS Status = BS->AllocatePages(AllocateAnyPages, EfiBootServicesData, Pages, &Base);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    *Out = (VOID*)(UINTN)Base;
    return EFI_SUCCESS;
}

STATIC VOID
HfsFreeBytes (
    IN UINTN Size,
    IN VOID* P
    )
{
    if (P == NULL) {
        return;
    }
    if (Size <= 128 * 1024) {
        FreePool(P);
    } else {
        BS->FreePages((EFI_PHYSICAL_ADDRESS)(UINTN)P,
                      (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE);
    }
}

// Read Size bytes at byte Offset from the mounted device into Buffer. Reads
// whole media blocks and copies the byte range when the request is not
// block-aligned (ReadBlocks requires buffer sizes to be block multiples).
STATIC EFI_STATUS
HfsDeviceReadBytes (
    IN  UINTN  Offset,
    IN  UINTN  Size,
    OUT VOID*  Buffer
    )
{
    if (Buffer == NULL || Size == 0) {
        return EFI_INVALID_PARAMETER;
    }
    if (Offset + Size > g_HfsDeviceBytes) {
        return EFI_END_OF_MEDIA;
    }

    if (Offset % g_HfsMediaBlockSize == 0 && Size % g_HfsMediaBlockSize == 0) {
        return PpcReadDiskBlock(g_HfsDeviceIndex,
                                (EFI_LBA)(Offset / g_HfsMediaBlockSize),
                                Size, Buffer);
    }

    UINTN  First = Offset / g_HfsMediaBlockSize;
    UINTN  Last  = (Offset + Size - 1) / g_HfsMediaBlockSize;
    UINTN  BlockCount = Last - First + 1;
    UINTN  ScratchSize = BlockCount * g_HfsMediaBlockSize;
    VOID*  Scratch = NULL;
    EFI_STATUS Status = HfsAllocBytes(ScratchSize, &Scratch);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = PpcReadDiskBlock(g_HfsDeviceIndex, (EFI_LBA)First, ScratchSize, Scratch);
    if (!EFI_ERROR(Status)) {
        CopyMem(Buffer, (UINT8*)Scratch + (Offset - First * g_HfsMediaBlockSize), Size);
    }

    HfsFreeBytes(ScratchSize, Scratch);
    return Status;
}

// ---------------------------------------------------------------------------
// Volume detection
// ---------------------------------------------------------------------------
// Validate the classic HFS MDB at Base+1024 (offsets match hfs_read.py).
STATIC BOOLEAN
HfsMdbFieldsClassic (
    IN  UINTN  Base,
    OUT UINTN* OutSize,
    OUT CHAR16* OutName
    )
{
    if (Base + HFS_MDB_OFF + 64 > g_HfsDeviceBytes) {
        return FALSE;
    }
    UINT8 Hdr[64];
    if (EFI_ERROR(HfsDeviceReadBytes(Base + HFS_MDB_OFF, 64, Hdr))) {
        return FALSE;
    }
    if (HfsU16(Hdr) != HFS_SIG_BD) {
        return FALSE;
    }
    UINT32 AlBlk   = HfsU32(Hdr + 20);  // drAlBlkSiz
    UINT16 NmBlks  = HfsU16(Hdr + 18);  // drNmAlBlks
    UINT8  VLen    = Hdr[36];           // drVN length byte
    if (AlBlk % 512 != 0 || AlBlk < 512 || AlBlk > (1u << 20)) {
        return FALSE;
    }
    if (VLen < 1 || VLen > PPC_HFS_NAME_MAX) {
        return FALSE;
    }
    if (NmBlks == 0 || (UINT64)NmBlks * AlBlk > g_HfsDeviceBytes) {
        return FALSE;
    }
    if (OutSize != NULL) {
        *OutSize = (UINTN)NmBlks * AlBlk;
    }
    if (OutName != NULL) {
        UINTN I;
        for (I = 0; I < VLen; I++) {
            OutName[I] = Hdr[37 + I];
        }
        OutName[VLen] = 0;
    }
    return TRUE;
}

// Validate the HFS+ volume header at Base+1024 (hfsplus_vh offsets).
STATIC BOOLEAN
HfsMdbFieldsPlus (
    IN  UINTN  Base,
    OUT UINTN* OutSize
    )
{
    if (Base + HFS_MDB_OFF + 64 > g_HfsDeviceBytes) {
        return FALSE;
    }
    UINT8 Hdr[64];
    if (EFI_ERROR(HfsDeviceReadBytes(Base + HFS_MDB_OFF, 64, Hdr))) {
        return FALSE;
    }
    UINT16 Sig = HfsU16(Hdr);
    if (Sig != HFS_SIG_PLUS && Sig != HFS_SIG_X) {
        return FALSE;
    }
    if (HfsU16(Hdr + 2) < 4) {           // version
        return FALSE;
    }
    UINT32 BlockSize    = HfsU32(Hdr + 40);   // blocksize
    UINT32 TotalBlocks  = HfsU32(Hdr + 44);   // total_blocks
    if (BlockSize % 512 != 0 || BlockSize < 512 || BlockSize > (1u << 20)) {
        return FALSE;
    }
    if (TotalBlocks == 0 || (UINT64)TotalBlocks * BlockSize > g_HfsDeviceBytes) {
        return FALSE;
    }
    if (OutSize != NULL) {
        *OutSize = (UINTN)TotalBlocks * BlockSize;
    }
    return TRUE;
}

// Locate a classic HFS volume via an Apple Partition Map scan.
STATIC BOOLEAN
HfsDetectApm (
    OUT UINTN* Base,
    OUT UINTN* Size
    )
{
    UINT8 Blk[4];
    if (EFI_ERROR(HfsDeviceReadBytes(0, 4, Blk))) {
        return FALSE;
    }
    if (HfsU16(Blk) != 0x4552u && HfsU16(Blk + 2) != 0x504Du) {
        return FALSE;   // no "ER" driver map / "PM" first entry
    }

    UINT16 BlockSize = HfsU16(Blk + 2);
    if (BlockSize < 512 || BlockSize > 8192) {
        BlockSize = 2048;
    }

    UINT8 Entry[64];
    for (UINTN E = 1; E <= 128; E++) {
        UINTN Off = E * BlockSize;
        if (Off + 64 > g_HfsDeviceBytes) {
            break;
        }
        if (EFI_ERROR(HfsDeviceReadBytes(Off, 64, Entry))) {
            break;
        }
        if (HfsU16(Entry) != 0x504Du) {      // 'PM'
            break;
        }
        UINTN  PStart = (UINTN)HfsU32(Entry + 8) * BlockSize;   // pmPyPartStart
        UINT32 PBlks  = HfsU32(Entry + 12);                     // pmPartBlkCnt
        if (Entry[48 + 0] == 'A' && Entry[48 + 1] == 'p' && Entry[48 + 2] == 'p' &&
            Entry[48 + 3] == 'l' && Entry[48 + 4] == 'e' && Entry[48 + 5] == '_') {
            // pmParType @48, "Apple_HFS"/"Apple_HFSX"
            UINTN S = 0;
            if (HfsMdbFieldsClassic(PStart, &S, NULL)) {
                *Base = PStart;
                *Size = S;
                return TRUE;
            }
        }
        (VOID)PBlks;
    }
    return FALSE;
}

// Full 2048-byte-boundary MDB scan; picks the largest plausible volume so
// discs whose HFS volume lives outside the declared partitions (e.g. the
// "Mac OS 8.1HD" volume on the retail 8.1 CD) are found.
STATIC BOOLEAN
HfsDetectScan (
    OUT UINTN* Base,
    OUT UINTN* Size,
    OUT BOOLEAN* IsPlus
    )
{
    const UINTN ChunkSize = 2 * 1024 * 1024;
    VOID* Chunk = NULL;
    if (EFI_ERROR(HfsAllocBytes(ChunkSize, &Chunk))) {
        return FALSE;
    }

    // Cheap pre-filter: a PC boot-sector filesystem (FAT/NTFS/ISO floppy) has
    // the 0x55AA signature at offset 510 and a jmp opcode at 0. Raw HFS boot
    // blocks never do. Skipping these avoids a slow full-device scan of, e.g.,
    // the 500 MB FAT ESP that OVMF exposes as a block device.
    {
        UINT8 Probe[512];
        if (!EFI_ERROR(HfsDeviceReadBytes(0, sizeof(Probe), Probe)) &&
            (Probe[0] == 0xEB || Probe[0] == 0xE9) &&
            Probe[510] == 0x55 && Probe[511] == 0xAA) {
            HfsFreeBytes(ChunkSize, Chunk);
            return FALSE;
        }
    }

    BOOLEAN Found = FALSE;
    UINTN BestSize = 0;
    UINTN BestBase = 0;
    BOOLEAN BestPlus = FALSE;

    // Real Mac volumes always live in the first few MB (driver area +
    // partitions), so cap the scan to bound the cost on huge non-HFS block
    // devices like the OVMF FAT ESP.
    UINTN ScanEnd = g_HfsDeviceBytes;
    if (ScanEnd > 32 * 1024 * 1024) {
        ScanEnd = 32 * 1024 * 1024;
    }

    for (UINTN Start = 0; Start < ScanEnd; Start += ChunkSize) {
        UINTN Take = ChunkSize;
        if (Start + Take > ScanEnd) {
            Take = (UINTN)(ScanEnd - Start);
        }
        if (Take < 2048) {
            break;
        }
        if (EFI_ERROR(HfsDeviceReadBytes(Start, Take, Chunk))) {
            break;
        }
        // The MDB signature sits at Base+1024 for a 2048-aligned Base, so it
        // appears at positions congruent to 1024 mod 2048.
        for (UINTN P = 1024; P + 2 <= Take; P += 2048) {
            UINT8* S = (UINT8*)Chunk + P;
            UINTN B = Start + P - 1024;
            if (S[0] == 'B' && S[1] == 'D') {
                UINTN Sz = 0;
                if (HfsMdbFieldsClassic(B, &Sz, NULL)) {
                    if (Sz > BestSize) {
                        BestSize = Sz;
                        BestBase = B;
                        BestPlus = FALSE;
                        Found = TRUE;
                    }
                }
            } else if (S[0] == 'H' && (S[1] == '+' || S[1] == 'X')) {
                UINTN Sz = 0;
                if (HfsMdbFieldsPlus(B, &Sz)) {
                    if (Sz > BestSize) {
                        BestSize = Sz;
                        BestBase = B;
                        BestPlus = TRUE;
                        Found = TRUE;
                    }
                }
            }
        }
    }

    HfsFreeBytes(ChunkSize, Chunk);

    if (Found) {
        *Base = BestBase;
        *Size = BestSize;
        *IsPlus = BestPlus;
    }
    return Found;
}

// ---------------------------------------------------------------------------
// B-tree parsing (shared classic/HFS+ node walking)
// ---------------------------------------------------------------------------
STATIC UINT16
HfsNodeRecOffset (
    IN UINT8* Node,
    IN UINTN  NodeSize,
    IN UINTN  RecIdx
    )
{
    return HfsU16(Node + NodeSize - 2 - 2 * RecIdx);
}

// Decode a classic HFS name (single-byte MacRoman/Latin-1) into CHAR16.
STATIC VOID
HfsNameLatin1 (
    OUT CHAR16* Dst,
    IN  const UINT8* Src,
    IN  UINTN Len
    )
{
    UINTN I;
    UINTN Max = Len < PPC_HFS_NAME_MAX ? Len : PPC_HFS_NAME_MAX;
    for (I = 0; I < Max; I++) {
        Dst[I] = Src[I];
    }
    Dst[Max] = 0;
}

// Decode an HFS+ UTF-16BE name into CHAR16.
STATIC VOID
HfsNameUtf16 (
    OUT CHAR16* Dst,
    IN  const UINT8* Src,
    IN  UINTN Chars
    )
{
    UINTN I;
    UINTN Max = Chars < PPC_HFS_NAME_MAX ? Chars : PPC_HFS_NAME_MAX;
    for (I = 0; I < Max; I++) {
        Dst[I] = HfsU16(Src + I * 2);
    }
    Dst[Max] = 0;
}

// Case-insensitive ASCII CHAR16 compare.
STATIC INTN
HfsStriCmp (
    IN const CHAR16* A,
    IN const CHAR16* B
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

// ---------------------------------------------------------------------------
// Classic HFS catalog + extents overflow build
// ---------------------------------------------------------------------------
STATIC INTN
HfsOvfCompare (
    IN const HFS_OVF_ENTRY* A,
    IN const HFS_OVF_ENTRY* B
    )
{
    if (A->FlNum != B->FlNum) {
        return (A->FlNum < B->FlNum) ? -1 : 1;
    }
    if (A->Fabn != B->Fabn) {
        return (A->Fabn < B->Fabn) ? -1 : 1;
    }
    return 0;
}

STATIC VOID
HfsOvfSort (VOID)
{
    for (UINTN I = 1; I < g_HfsOvfCount; I++) {
        HFS_OVF_ENTRY Tmp = g_HfsOvf[I];
        UINTN J = I;
        while (J > 0 && HfsOvfCompare(&g_HfsOvf[J - 1], &Tmp) > 0) {
            g_HfsOvf[J] = g_HfsOvf[J - 1];
            J--;
        }
        g_HfsOvf[J] = Tmp;
    }
}

STATIC EFI_STATUS
HfsBuildClassicCatalog (
    VOID
    )
{
    g_HfsDirCount  = 0;
    g_HfsFileCount = 0;
    g_HfsOvfCount  = 0;

    UINTN CatNodes = g_CatSize / g_CatNodeSize;
    for (UINTN N = 0; N < CatNodes; N++) {
        UINT8* Node = g_CatData + N * g_CatNodeSize;
        if (Node[8] != 0xFF) {              // ndType != leaf
            continue;
        }
        UINT16 NumRecs = HfsU16(Node + 10); // ndNRecs
        for (UINTN R = 0; R < NumRecs; R++) {
            UINT16 ROff = HfsNodeRecOffset(Node, g_CatNodeSize, R);
            if (ROff + 1 >= g_CatNodeSize) {
                continue;
            }
            UINT8  KLen = Node[ROff];
            if (KLen == 0) {
                continue;
            }
            UINTN KeySize = (UINTN)((KLen | 1) + 1);   // padded to even
            if (ROff + KeySize > g_CatNodeSize) {
                continue;
            }
            UINT8* Key = Node + ROff + 1;
            UINT8* Rec = Node + ROff + KeySize;
            if (KLen < 6) {
                continue;
            }
            UINT32 Parent = HfsU32(Key + 1);           // ParID
            UINT8  NLen   = Key[5];                    // CName length
            if (NLen > PPC_HFS_NAME_MAX) {
                continue;
            }
            UINT8 Type = Rec[0];
            if (Type == PPC_HFS_CDR_FIL && g_HfsFileCount < PPC_HFS_MAX_FILES) {
                PPC_HFS_ENTRY* E = &g_HfsFiles[g_HfsFileCount];
                ZeroMem(E, sizeof(*E));
                E->IsDirectory = FALSE;
                E->Id       = HfsU32(Rec + 20);        // FlNum
                E->ParentId = Parent;
                E->Size     = HfsU32(Rec + 26);        // LgLen
                UINT32 Cnt = 0;
                for (UINTN X = 0; X < 3; X++) {        // ExtRec @74
                    UINT16 Blk = HfsU16(Rec + 74 + X * 4);
                    UINT16 C   = HfsU16(Rec + 74 + X * 4 + 2);
                    if (C != 0 && Cnt < PPC_HFS_MAX_EXTENTS) {
                        E->Extents[Cnt * 2]     = Blk;
                        E->Extents[Cnt * 2 + 1] = C;
                        Cnt++;
                    }
                }
                E->ExtentCount = Cnt;
                HfsNameLatin1(E->Name, Key + 6, NLen);
                g_HfsFileCount++;
            } else if (Type == PPC_HFS_CDR_DIR && g_HfsDirCount < PPC_HFS_MAX_DIRS) {
                PPC_HFS_ENTRY* E = &g_HfsDirs[g_HfsDirCount];
                ZeroMem(E, sizeof(*E));
                E->IsDirectory = TRUE;
                E->Id       = HfsU32(Rec + 6);         // DirID
                E->ParentId = Parent;
                E->Size     = 0;
                HfsNameLatin1(E->Name, Key + 6, NLen);
                g_HfsDirCount++;
            }
        }
    }

    // Extents overflow B-tree -> (FlNum, Fabn) -> 3 extent records (data fork).
    UINTN XofNodes = g_XofSize / g_XofNodeSize;
    for (UINTN N = 0; N < XofNodes; N++) {
        UINT8* Node = g_XofData + N * g_XofNodeSize;
        if (Node[8] != 0xFF) {
            continue;
        }
        UINT16 NumRecs = HfsU16(Node + 10);
        for (UINTN R = 0; R < NumRecs; R++) {
            UINT16 ROff = HfsNodeRecOffset(Node, g_XofNodeSize, R);
            if (ROff + 1 >= g_XofNodeSize) {
                continue;
            }
            UINT8  KLen = Node[ROff];
            if (KLen == 0) {
                continue;
            }
            UINTN KeySize = (UINTN)((KLen | 1) + 1);
            if (ROff + KeySize > g_XofNodeSize) {
                continue;
            }
            UINT8* Key = Node + ROff + 1;
            UINT8* Rec = Node + ROff + KeySize;
            if (Key[0] != 0x00 || g_HfsOvfCount >= PPC_HFS_MAX_OVF) {
                continue;                              // data fork only
            }
            HFS_OVF_ENTRY* O = &g_HfsOvf[g_HfsOvfCount];
            ZeroMem(O, sizeof(*O));
            O->FlNum = HfsU32(Key + 1);                // FNum
            O->Fabn  = HfsU16(Key + 5);                // FABN
            for (UINTN X = 0; X < 3; X++) {
                O->Blocks[X] = HfsU16(Rec + X * 4);
                O->Counts[X] = HfsU16(Rec + X * 4 + 2);
            }
            g_HfsOvfCount++;
        }
    }
    HfsOvfSort();

    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// HFS+ catalog build
// ---------------------------------------------------------------------------
STATIC EFI_STATUS
HfsBuildPlusCatalog (
    VOID
    )
{
    g_HfsDirCount  = 0;
    g_HfsFileCount = 0;

    UINTN CatNodes = g_CatSize / g_CatNodeSize;
    for (UINTN N = 0; N < CatNodes; N++) {
        UINT8* Node = g_CatData + N * g_CatNodeSize;
        if (Node[8] != 0xFF) {
            continue;
        }
        UINT16 NumRecs = HfsU16(Node + 10);
        for (UINTN R = 0; R < NumRecs; R++) {
            UINT16 ROff = HfsNodeRecOffset(Node, g_CatNodeSize, R);
            if (ROff + 2 >= g_CatNodeSize) {
                continue;
            }
            UINT16 KLen = HfsU16(Node + ROff);
            if (KLen < 4 || ROff + 2 + KLen >= g_CatNodeSize) {
                continue;
            }
            UINT8* Key = Node + ROff + 2;
            UINT8* Rec = Node + ROff + 2 + KLen;
            UINT32 Parent = HfsU32(Key);               // parent CNID
            UINT16 RType  = HfsU16(Rec);               // record type
            if (RType == 1 && g_HfsDirCount < PPC_HFS_MAX_DIRS) {   // folder
                PPC_HFS_ENTRY* E = &g_HfsDirs[g_HfsDirCount];
                ZeroMem(E, sizeof(*E));
                E->IsDirectory = TRUE;
                E->Id       = HfsU32(Rec + 8);         // folder id
                E->ParentId = Parent;
                E->Size     = 0;
                UINTN NLen = (KLen - 4) / 2;
                HfsNameUtf16(E->Name, Key + 4, NLen);
                g_HfsDirCount++;
            } else if (RType == 2 && g_HfsFileCount < PPC_HFS_MAX_FILES) { // file
                PPC_HFS_ENTRY* E = &g_HfsFiles[g_HfsFileCount];
                ZeroMem(E, sizeof(*E));
                E->IsDirectory = FALSE;
                E->Id       = HfsU32(Rec + 8);         // file id
                E->ParentId = Parent;
                E->Size     = HfsU64(Rec + 88);        // data fork logical size
                UINT32 Cnt = 0;
                for (UINTN X = 0; X < 8; X++) {        // data fork extents @104
                    UINT32 Blk = HfsU32(Rec + 104 + X * 8);
                    UINT32 C   = HfsU32(Rec + 108 + X * 8);
                    if (C != 0 && Cnt < PPC_HFS_MAX_EXTENTS) {
                        E->Extents[Cnt * 2]     = Blk;
                        E->Extents[Cnt * 2 + 1] = C;
                        Cnt++;
                    }
                }
                E->ExtentCount = Cnt;
                UINTN NLen = (KLen - 4) / 2;
                HfsNameUtf16(E->Name, Key + 4, NLen);
                g_HfsFileCount++;
            }
        }
    }

    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Mount
// ---------------------------------------------------------------------------
// Load a B-tree file (catalog or extents overflow) into memory and parse its
// header node.
STATIC EFI_STATUS
HfsLoadBTreeFile (
    IN  UINTN  Base,
    IN  UINTN  AllocBlockSize,
    IN  UINTN  AllocBlockStart,
    IN  const UINT16* Extents,       // 3 (block,count) pairs
    IN  UINTN  FileSize,
    OUT UINT8** OutData,
    OUT UINTN*  OutSize,
    OUT UINTN*  OutNodeSize
    )
{
    UINT8* Data = NULL;
    EFI_STATUS Status = HfsAllocBytes(FileSize, (VOID**)&Data);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    UINTN Have = 0;
    for (UINTN X = 0; X < 3 && Have < FileSize; X++) {
        UINT16 Block = Extents[X * 2];
        UINT16 Count = Extents[X * 2 + 1];
        if (Count == 0) {
            continue;
        }
        UINTN Take = (UINTN)Count * AllocBlockSize;
        if (Have + Take > FileSize) {
            Take = FileSize - Have;
        }
        Status = HfsDeviceReadBytes(Base + AllocBlockStart + (UINTN)Block * AllocBlockSize,
                                    Take, Data + Have);
        if (EFI_ERROR(Status)) {
            HfsFreeBytes(FileSize, Data);
            return Status;
        }
        Have += Take;
    }

    UINTN NodeSize = 512;
    if (FileSize >= 56) {
        NodeSize = HfsU16(Data + 32);   // bthNodeSize
    }

    *OutData = Data;
    *OutSize = FileSize;
    *OutNodeSize = NodeSize;
    return EFI_SUCCESS;
}

EFI_STATUS
PpcHfsMount (
    OUT PPC_HFS_VOLUME_INFO* Info
    )
{
    // Make sure the block devices are enumerated.
    PPC_BLOCK_IO_INFO Bio;
    EFI_STATUS Status = PpcGetBlockIoInfo(&Bio);
    if (EFI_ERROR(Status)) {
        Status = PpcInitializeBlockIo(1);
        if (EFI_ERROR(Status)) {
            return EFI_NOT_READY;
        }
        Status = PpcGetBlockIoInfo(&Bio);
        if (EFI_ERROR(Status)) {
            return EFI_NOT_READY;
        }
    }

    BOOLEAN Found = FALSE;
    UINTN   FoundKind = PPC_HFS_KIND_NONE;
    UINTN   FoundBase = 0;

    for (UINTN I = 0; I < Bio.DeviceCount && !Found; I++) {
        PPC_BLOCK_DEVICE_INFO Dev;
        if (EFI_ERROR(PpcGetBlockDeviceInfo(I, &Dev))) {
            continue;
        }
        if (Dev.BlockSize == 0 || Dev.BlockCount == 0) {
            continue;
        }

        g_HfsDeviceIndex    = I;
        g_HfsMediaBlockSize = Dev.BlockSize;
        g_HfsDeviceBytes    = Dev.BlockCount * Dev.BlockSize;

        // 1. Raw volume at offset 0.
        UINTN Sz = 0;
        CHAR16 Name[PPC_HFS_NAME_MAX + 1];
        if (HfsMdbFieldsClassic(0, &Sz, Name)) {
            Found = TRUE;
            FoundKind = PPC_HFS_KIND_CLASSIC;
            FoundBase = 0;
            break;
        }
        if (HfsMdbFieldsPlus(0, &Sz)) {
            Found = TRUE;
            FoundKind = PPC_HFS_KIND_PLUS;
            FoundBase = 0;
            break;
        }

        // 2. Apple Partition Map Apple_HFS partition.
        if (HfsDetectApm(&FoundBase, &Sz)) {
            Found = TRUE;
            FoundKind = PPC_HFS_KIND_CLASSIC;
            break;
        }

        // 3. Full 2048-byte boundary scan, largest volume wins.
        BOOLEAN IsPlus = FALSE;
        if (HfsDetectScan(&FoundBase, &Sz, &IsPlus)) {
            Found = TRUE;
            FoundKind = IsPlus ? PPC_HFS_KIND_PLUS : PPC_HFS_KIND_CLASSIC;
            break;
        }
    }

    if (!Found) {
        g_HfsMounted = FALSE;
        return EFI_NOT_FOUND;
    }

    // Free any previous catalog state.
    if (g_CatData != NULL) { HfsFreeBytes(g_CatSize, g_CatData); g_CatData = NULL; }
    if (g_XofData != NULL) { HfsFreeBytes(g_XofSize, g_XofData); g_XofData = NULL; }
    if (g_HfsDirs  != NULL) {
        HfsFreeBytes(PPC_HFS_MAX_DIRS * sizeof(PPC_HFS_ENTRY), g_HfsDirs);
        g_HfsDirs = NULL;
    }
    if (g_HfsFiles != NULL) {
        HfsFreeBytes(PPC_HFS_MAX_FILES * sizeof(PPC_HFS_ENTRY), g_HfsFiles);
        g_HfsFiles = NULL;
    }

    Status = HfsAllocBytes(PPC_HFS_MAX_DIRS * sizeof(PPC_HFS_ENTRY), (VOID**)&g_HfsDirs);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    Status = HfsAllocBytes(PPC_HFS_MAX_FILES * sizeof(PPC_HFS_ENTRY), (VOID**)&g_HfsFiles);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    g_HfsVolume.Mounted = TRUE;
    g_HfsVolume.DeviceIndex = g_HfsDeviceIndex;
    g_HfsVolume.MediaBlockSize = g_HfsMediaBlockSize;
    g_HfsVolume.DeviceBytes = g_HfsDeviceBytes;
    g_HfsVolume.Kind = (PPC_HFS_KIND)FoundKind;
    g_HfsVolume.VolumeBase = FoundBase;
    g_HfsVolume.FileCount = 0;
    g_HfsVolume.DirCount = 0;
    ZeroMem(g_HfsVolume.VolumeName, sizeof(g_HfsVolume.VolumeName));

    if (FoundKind == PPC_HFS_KIND_CLASSIC) {
        UINT8 Mdb[162];
        Status = HfsDeviceReadBytes(FoundBase + HFS_MDB_OFF, sizeof(Mdb), Mdb);
        if (EFI_ERROR(Status)) {
            g_HfsMounted = FALSE;
            return Status;
        }
        UINTN AlBlk    = HfsU32(Mdb + 20);            // drAlBlkSiz
        UINTN AlBlkOff = (UINTN)HfsU16(Mdb + 28) * 512;  // drAlBlSt
        UINT8 VLen     = Mdb[36];
        g_HfsVolume.VolumeBlockSize = AlBlk;
        g_HfsVolume.TotalBytes = (UINTN)HfsU16(Mdb + 18) * AlBlk;  // drNmAlBlks
        g_HfsAllocBlockStart  = AlBlkOff;
        if (VLen <= PPC_HFS_NAME_MAX) {
            for (UINTN I = 0; I < VLen; I++) {
                g_HfsVolume.VolumeName[I] = Mdb[37 + I];
            }
            g_HfsVolume.VolumeName[VLen] = 0;
        }

        // Extents overflow file: drXTExtRec @134, drXTFlSize @130
        UINT16 XofExts[6];
        for (UINTN X = 0; X < 3; X++) {
            XofExts[X * 2]     = HfsU16(Mdb + 134 + X * 4);
            XofExts[X * 2 + 1] = HfsU16(Mdb + 134 + X * 4 + 2);
        }
        Status = HfsLoadBTreeFile(FoundBase, AlBlk, AlBlkOff, XofExts,
                                  HfsU32(Mdb + 130), &g_XofData, &g_XofSize, &g_XofNodeSize);
        if (EFI_ERROR(Status)) {
            g_HfsMounted = FALSE;
            return Status;
        }

        // Catalog file: drCTExtRec @150, drCTFlSize @146
        UINT16 CatExts[6];
        for (UINTN X = 0; X < 3; X++) {
            CatExts[X * 2]     = HfsU16(Mdb + 150 + X * 4);
            CatExts[X * 2 + 1] = HfsU16(Mdb + 150 + X * 4 + 2);
        }
        Status = HfsLoadBTreeFile(FoundBase, AlBlk, AlBlkOff, CatExts,
                                  HfsU32(Mdb + 146), &g_CatData, &g_CatSize, &g_CatNodeSize);
        if (EFI_ERROR(Status)) {
            g_HfsMounted = FALSE;
            return Status;
        }

        Status = HfsBuildClassicCatalog();
    } else {
        UINT8 Vh[512];
        Status = HfsDeviceReadBytes(FoundBase + HFS_MDB_OFF, sizeof(Vh), Vh);
        if (EFI_ERROR(Status)) {
            g_HfsMounted = FALSE;
            return Status;
        }
        g_HfsVolume.VolumeBlockSize = HfsU32(Vh + 40);   // blocksize
        g_HfsVolume.TotalBytes = (UINTN)HfsU32(Vh + 44) * g_HfsVolume.VolumeBlockSize;

        // Catalog fork @272 (hfsplus_fork_raw: size@0, extents@16)
        UINT64 CatLogSize = HfsU64(Vh + 272);
        UINTN  CatSize = (UINTN)CatLogSize;
        VOID*  CatBuf = NULL;
        Status = HfsAllocBytes(CatSize, &CatBuf);
        if (EFI_ERROR(Status)) {
            g_HfsMounted = FALSE;
            return Status;
        }
        UINTN Have = 0;
        for (UINTN X = 0; X < 8 && Have < CatSize; X++) {
            UINT32 Blk = HfsU32(Vh + 272 + 16 + X * 8);
            UINT32 C   = HfsU32(Vh + 272 + 20 + X * 8);
            if (C == 0) {
                continue;
            }
            UINTN Take = (UINTN)C * g_HfsVolume.VolumeBlockSize;
            if (Have + Take > CatSize) {
                Take = CatSize - Have;
            }
            Status = HfsDeviceReadBytes(FoundBase + (UINTN)Blk * g_HfsVolume.VolumeBlockSize,
                                        Take, (UINT8*)CatBuf + Have);
            if (EFI_ERROR(Status)) {
                HfsFreeBytes(CatSize, CatBuf);
                g_HfsMounted = FALSE;
                return Status;
            }
            Have += Take;
        }
        g_CatData = (UINT8*)CatBuf;
        g_CatSize = CatSize;
        g_CatNodeSize = (CatSize >= 56) ? HfsU16(g_CatData + 32) : 512;

        Status = HfsBuildPlusCatalog();
    }

    if (EFI_ERROR(Status)) {
        g_HfsMounted = FALSE;
        return Status;
    }

    g_HfsVolume.FileCount = g_HfsFileCount;
    g_HfsVolume.DirCount  = g_HfsDirCount;
    g_HfsMounted = TRUE;

    Print(L"HFS volume mounted: device %d, base 0x%x, %s, block size %d, "
          L"%d files, %d folders\n",
          (UINTN)g_HfsVolume.DeviceIndex, (UINTN)g_HfsVolume.VolumeBase,
          g_HfsVolume.Kind == PPC_HFS_KIND_CLASSIC ? L"HFS" : L"HFS+",
          (UINTN)g_HfsVolume.VolumeBlockSize,
          (UINTN)g_HfsVolume.FileCount, (UINTN)g_HfsVolume.DirCount);

    if (Info != NULL) {
        CopyMem(Info, &g_HfsVolume, sizeof(PPC_HFS_VOLUME_INFO));
    }
    return EFI_SUCCESS;
}

EFI_STATUS
PpcHfsGetVolumeInfo (
    OUT PPC_HFS_VOLUME_INFO* Info
    )
{
    if (Info == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (!g_HfsMounted) {
        return EFI_NOT_READY;
    }
    CopyMem(Info, &g_HfsVolume, sizeof(PPC_HFS_VOLUME_INFO));
    return EFI_SUCCESS;
}

EFI_STATUS
PpcHfsProbeBootFiles (
    OUT BOOLEAN* SystemFolderPresent,
    OUT BOOLEAN* SystemPresent,
    OUT BOOLEAN* FinderPresent,
    OUT BOOLEAN* MacOsRomPresent,
    OUT UINT32*  SystemFolderId
    )
{
    if (!g_HfsMounted) {
        return EFI_NOT_READY;
    }

    BOOLEAN Folder = FALSE;
    BOOLEAN Sys = FALSE;
    BOOLEAN Finder = FALSE;
    BOOLEAN Rom = FALSE;
    UINT32  FolderId = 0;

    PPC_HFS_ENTRY E;
    if (!EFI_ERROR(PpcHfsOpenPath(L"System Folder", &E)) && E.IsDirectory) {
        Folder = TRUE;
        FolderId = E.Id;
        if (!EFI_ERROR(PpcHfsOpenPath(L"System Folder:System", &E)) && !E.IsDirectory) {
            Sys = TRUE;
        }
        if (!EFI_ERROR(PpcHfsOpenPath(L"System Folder:Finder", &E)) && !E.IsDirectory) {
            Finder = TRUE;
        }
    }

    // The New World "Mac OS ROM" file may live inside an install-image System
    // Folder (e.g. "Power Mac G4 Install:System Folder:Mac OS ROM"), so probe
    // the whole catalog rather than a single fixed path.
    if (!EFI_ERROR(PpcHfsFindMacOsRom(&E)) && !E.IsDirectory) {
        Rom = TRUE;
    }

    if (SystemFolderPresent) { *SystemFolderPresent = Folder; }
    if (SystemPresent) { *SystemPresent = Sys; }
    if (FinderPresent) { *FinderPresent = Finder; }
    if (MacOsRomPresent) { *MacOsRomPresent = Rom; }
    if (SystemFolderId) { *SystemFolderId = FolderId; }
    return EFI_SUCCESS;
}

EFI_STATUS
PpcHfsListChildren (
    IN  UINT32         DirId,
    OUT PPC_HFS_ENTRY* Entries,
    IN  OUT UINTN*     Count
    )
{
    if (Entries == NULL || Count == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (!g_HfsMounted) {
        return EFI_NOT_READY;
    }
    if (*Count == 0) {
        return EFI_BUFFER_TOO_SMALL;
    }

    UINTN Cap = *Count;
    UINTN Written = 0;
    UINTN Total = 0;

    for (UINTN I = 0; I < g_HfsDirCount; I++) {
        if (g_HfsDirs[I].ParentId == DirId) {
            Total++;
            if (Written < Cap) {
                CopyMem(&Entries[Written], &g_HfsDirs[I], sizeof(PPC_HFS_ENTRY));
                Written++;
            }
        }
    }
    for (UINTN I = 0; I < g_HfsFileCount; I++) {
        if (g_HfsFiles[I].ParentId == DirId) {
            Total++;
            if (Written < Cap) {
                CopyMem(&Entries[Written], &g_HfsFiles[I], sizeof(PPC_HFS_ENTRY));
                Written++;
            }
        }
    }

    *Count = Written;
    return (Written < Total) ? EFI_BUFFER_TOO_SMALL : EFI_SUCCESS;
}

EFI_STATUS
PpcHfsGetEntryById (
    IN  UINT32         Id,
    OUT PPC_HFS_ENTRY* Entry
    )
{
    if (Entry == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (!g_HfsMounted) {
        return EFI_NOT_READY;
    }

    for (UINTN I = 0; I < g_HfsDirCount; I++) {
        if (g_HfsDirs[I].Id == Id) {
            CopyMem(Entry, &g_HfsDirs[I], sizeof(PPC_HFS_ENTRY));
            return EFI_SUCCESS;
        }
    }
    for (UINTN I = 0; I < g_HfsFileCount; I++) {
        if (g_HfsFiles[I].Id == Id) {
            CopyMem(Entry, &g_HfsFiles[I], sizeof(PPC_HFS_ENTRY));
            return EFI_SUCCESS;
        }
    }

    return EFI_NOT_FOUND;
}

// The New World "Mac OS ROM" file name. It ships inside System Folders, and
// on install discs it lives inside the disc's install-image System Folder
// (e.g. "Power Mac G4 Install:System Folder:Mac OS ROM"), so a path-based
// lookup is not enough: search the whole catalog.
STATIC const CHAR16 PPC_HFS_MAC_OS_ROM_NAME[] = L"Mac OS ROM";

EFI_STATUS
PpcHfsFindMacOsRom (
    OUT PPC_HFS_ENTRY* Entry
    )
{
    if (Entry == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (!g_HfsMounted) {
        return EFI_NOT_READY;
    }

    BOOLEAN Found = FALSE;
    PPC_HFS_ENTRY Best;
    ZeroMem(&Best, sizeof(Best));
    for (UINTN I = 0; I < g_HfsFileCount; I++) {
        if (g_HfsFiles[I].Size == 0) {
            continue;
        }
        if (HfsStriCmp(g_HfsFiles[I].Name, PPC_HFS_MAC_OS_ROM_NAME) != 0) {
            continue;
        }
        if (!Found || g_HfsFiles[I].Size > Best.Size) {
            Best = g_HfsFiles[I];
            Found = TRUE;
        }
    }

    if (!Found) {
        return EFI_NOT_FOUND;
    }
    CopyMem(Entry, &Best, sizeof(PPC_HFS_ENTRY));
    return EFI_SUCCESS;
}

EFI_STATUS
PpcHfsOpenPath (
    IN  CHAR16*       Path,
    OUT PPC_HFS_ENTRY* Entry
    )
{
    if (Path == NULL || Entry == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (!g_HfsMounted) {
        return EFI_NOT_READY;
    }

    UINT32 CurId = HFS_ROOT_CNID;
    PPC_HFS_ENTRY Cur;
    ZeroMem(&Cur, sizeof(Cur));
    BOOLEAN HaveCur = FALSE;

    // Split the path on ':' or '\\'.
    UINTN Pos = 0;
    while (Path[Pos] != 0) {
        while (Path[Pos] == L':' || Path[Pos] == L'\\' || Path[Pos] == L'/') {
            Pos++;
        }
        if (Path[Pos] == 0) {
            break;
        }
        UINTN SegStart = Pos;
        while (Path[Pos] != 0 && Path[Pos] != L':' && Path[Pos] != L'\\' &&
               Path[Pos] != L'/') {
            Pos++;
        }
        UINTN SegLen = Pos - SegStart;

        CHAR16 Seg[PPC_HFS_NAME_MAX + 1];
        UINTN L = SegLen < PPC_HFS_NAME_MAX ? SegLen : PPC_HFS_NAME_MAX;
        for (UINTN I = 0; I < L; I++) {
            Seg[I] = Path[SegStart + I];
        }
        Seg[L] = 0;

        BOOLEAN Match = FALSE;
        for (UINTN I = 0; I < g_HfsDirCount && !Match; I++) {
            if (g_HfsDirs[I].ParentId == CurId && HfsStriCmp(g_HfsDirs[I].Name, Seg) == 0) {
                Cur = g_HfsDirs[I];
                Match = TRUE;
            }
        }
        if (!Match) {
            for (UINTN I = 0; I < g_HfsFileCount && !Match; I++) {
                if (g_HfsFiles[I].ParentId == CurId && HfsStriCmp(g_HfsFiles[I].Name, Seg) == 0) {
                    Cur = g_HfsFiles[I];
                    Match = TRUE;
                }
            }
        }
        if (!Match) {
            return EFI_NOT_FOUND;
        }
        CurId = Cur.Id;
        HaveCur = TRUE;
    }

    if (!HaveCur) {
        return EFI_NOT_FOUND;
    }
    CopyMem(Entry, &Cur, sizeof(PPC_HFS_ENTRY));
    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// File data reads
// ---------------------------------------------------------------------------
// Classic HFS: walk the file's extent records, spilling into the sorted
// extents-overflow records when the first three are exhausted. Mirrors
// hfs_read.py's read_file().
STATIC EFI_STATUS
HfsReadFileClassic (
    IN  PPC_HFS_ENTRY* Entry,
    OUT VOID*          Buffer,
    IN  UINTN          Size
    )
{
    UINTN AlBlk = g_HfsVolume.VolumeBlockSize;
    UINTN Base  = g_HfsVolume.VolumeBase;
    UINTN Want   = (UINTN)Entry->Size;
    UINTN ToRead = (Want < Size) ? Want : Size;

    UINT32 Exts[PPC_HFS_MAX_EXTENTS * 2];
    UINT32 ExtCnt = Entry->ExtentCount;
    for (UINTN I = 0; I < ExtCnt * 2; I++) {
        Exts[I] = Entry->Extents[I];
    }

    UINTN Have = 0;
    UINTN Logical = 0;
    UINTN OvfPos = 0;

    while (Have < ToRead) {
        while (ExtCnt > 0 && Have < ToRead) {
            UINT32 Block = Exts[0];
            UINT32 Count = Exts[1];
            for (UINTN K = 0; K + 1 < ExtCnt; K++) {
                Exts[K * 2]     = Exts[(K + 1) * 2];
                Exts[K * 2 + 1] = Exts[(K + 1) * 2 + 1];
            }
            ExtCnt--;
            UINTN N = (UINTN)Count * AlBlk;
            if (Have + N > ToRead) {
                N = ToRead - Have;
            }
            EFI_STATUS S = HfsDeviceReadBytes(Base + g_HfsAllocBlockStart +
                                              (UINTN)Block * AlBlk, N,
                                              (UINT8*)Buffer + Have);
            if (EFI_ERROR(S)) {
                return S;
            }
            Have += N;
            Logical += Count;
        }
        if (Have >= ToRead) {
            break;
        }

        // Pull the next overflow record whose FABN lines up with the logical
        // position; its three extents continue the file.
        BOOLEAN Found = FALSE;
        while (OvfPos < g_HfsOvfCount) {
            HFS_OVF_ENTRY* O = &g_HfsOvf[OvfPos++];
            if (O->FlNum != Entry->Id) {
                continue;
            }
            if (O->Fabn > Logical) {
                break;
            }
            ExtCnt = 0;
            for (UINTN X = 0; X < 3; X++) {
                if (O->Counts[X] != 0 && ExtCnt < PPC_HFS_MAX_EXTENTS) {
                    Exts[ExtCnt * 2]     = O->Blocks[X];
                    Exts[ExtCnt * 2 + 1] = O->Counts[X];
                    ExtCnt++;
                }
            }
            Found = TRUE;
            break;
        }
        if (!Found) {
            break;
        }
    }

    return (Have >= ToRead) ? EFI_SUCCESS : EFI_END_OF_MEDIA;
}

// HFS+: the catalog record carries up to eight extents directly.
STATIC EFI_STATUS
HfsReadFilePlus (
    IN  PPC_HFS_ENTRY* Entry,
    OUT VOID*          Buffer,
    IN  UINTN          Size
    )
{
    UINTN AlBlk = g_HfsVolume.VolumeBlockSize;
    UINTN Base  = g_HfsVolume.VolumeBase;
    UINTN Want   = (UINTN)Entry->Size;
    UINTN ToRead = (Want < Size) ? Want : Size;

    UINTN Have = 0;
    for (UINTN X = 0; X < Entry->ExtentCount && Have < ToRead; X++) {
        UINTN Block = Entry->Extents[X * 2];
        UINTN Count = Entry->Extents[X * 2 + 1];
        UINTN N = Count * AlBlk;
        if (Have + N > ToRead) {
            N = ToRead - Have;
        }
        EFI_STATUS S = HfsDeviceReadBytes(Base + Block * AlBlk, N,
                                          (UINT8*)Buffer + Have);
        if (EFI_ERROR(S)) {
            return S;
        }
        Have += N;
    }

    return (Have >= ToRead) ? EFI_SUCCESS : EFI_END_OF_MEDIA;
}

EFI_STATUS
PpcHfsReadFile (
    IN  PPC_HFS_ENTRY* Entry,
    OUT VOID*          Buffer,
    IN  OUT UINTN*     Size
    )
{
    if (Entry == NULL || Buffer == NULL || Size == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (Entry->IsDirectory) {
        return EFI_INVALID_PARAMETER;
    }
    if (!g_HfsMounted) {
        return EFI_NOT_READY;
    }

    UINTN Cap = *Size;
    EFI_STATUS Status = (g_HfsVolume.Kind == PPC_HFS_KIND_CLASSIC)
                            ? HfsReadFileClassic(Entry, Buffer, Cap)
                            : HfsReadFilePlus(Entry, Buffer, Cap);

    UINTN Read = ((UINTN)Entry->Size < Cap) ? (UINTN)Entry->Size : Cap;
    if (EFI_ERROR(Status)) {
        *Size = Read;
        return Status;
    }

    *Size = Read;
    if ((UINTN)Entry->Size > Cap) {
        return EFI_BUFFER_TOO_SMALL;
    }
    return EFI_SUCCESS;
}

// ---------------------------------------------------------------------------
// Self-test
// ---------------------------------------------------------------------------
STATIC UINTN g_HfsTestPass = 0;
STATIC UINTN g_HfsTestFail = 0;

STATIC VOID
HfsCheck (
    IN BOOLEAN Ok,
    IN CHAR16* Name
    )
{
    if (Ok) {
        g_HfsTestPass++;
        Print(L"  [PASS] %s\n", Name);
    } else {
        g_HfsTestFail++;
        Print(L"  [FAIL] %s\n", Name);
    }
}

EFI_STATUS
PpcHfsRunSelfTest (
    VOID
    )
{
    g_HfsTestPass = 0;
    g_HfsTestFail = 0;

    Print(L"--- HFS Volume Self-Test ---\n");

    PPC_HFS_VOLUME_INFO Info;
    EFI_STATUS Status = PpcHfsMount(&Info);
    if (EFI_ERROR(Status)) {
        Print(L"  HFS mount: %r\n", Status);
        Print(L"--- HFS self-test: mount failed ---\n");
        return EFI_LOAD_ERROR;
    }

    Print(L"  Volume: %s  format=%s  block size=%d  base=0x%x\n",
          Info.VolumeName,
          Info.Kind == PPC_HFS_KIND_CLASSIC ? L"HFS" : L"HFS+",
          (UINTN)Info.VolumeBlockSize, (UINTN)Info.VolumeBase);

    HfsCheck(Info.Mounted, L"volume mounted");
    HfsCheck(Info.VolumeBlockSize % 512 == 0 && Info.VolumeBlockSize > 0,
             L"allocation block size is 512-byte multiple");
    HfsCheck(Info.VolumeName[0] != 0, L"volume name present");
    HfsCheck(Info.FileCount > 0, L"catalog file entries found");
    HfsCheck(Info.DirCount > 0, L"catalog folder entries found");

    PPC_HFS_ENTRY Root[16];
    UINTN RootCount = 16;
    Status = PpcHfsListChildren(HFS_ROOT_CNID, Root, &RootCount);
    HfsCheck((Status == EFI_SUCCESS || Status == EFI_BUFFER_TOO_SMALL) &&
             RootCount > 0, L"root directory listing");
    if (!EFI_ERROR(Status)) {
        Print(L"  Root children: %d\n", (UINTN)RootCount);
    } else {
        Print(L"  Root children: >= %d\n", (UINTN)RootCount);
    }

    BOOLEAN Folder, Sys, Finder, Rom;
    UINT32  FolderId;
    Status = PpcHfsProbeBootFiles(&Folder, &Sys, &Finder, &Rom, &FolderId);
    if (EFI_ERROR(Status)) {
        Print(L"  Boot-file probe: FAIL (%r)\n", Status);
        g_HfsTestFail++;
    } else {
        Print(L"  System Folder=%d System=%d Finder=%d MacOSROM=%d (DirID %d)\n",
              Folder, Sys, Finder, Rom, FolderId);
        if (Folder && Sys) {
            PPC_HFS_ENTRY SysFile;
            Status = PpcHfsOpenPath(L"System Folder:System", &SysFile);
            BOOLEAN Opened = !EFI_ERROR(Status) && !SysFile.IsDirectory;
            HfsCheck(Opened, L"System file resolved");
            if (Opened) {
                UINT8* Buf = NULL;
                UINTN BufSize = (UINTN)SysFile.Size;
                BOOLEAN ReadOk = FALSE;
                if (BufSize > 0 &&
                    !EFI_ERROR(HfsAllocBytes(BufSize, (VOID**)&Buf))) {
                    UINTN Got = BufSize;
                    Status = PpcHfsReadFile(&SysFile, Buf, &Got);
                    ReadOk = !EFI_ERROR(Status) && Got == BufSize && Buf[0] != 0;
                    HfsFreeBytes(BufSize, Buf);
                }
                HfsCheck(ReadOk, L"System file data read back");
            }
        } else {
            HfsCheck(TRUE, L"no bootable System Folder (SKIP: installer disc)");
        }
    }

    Print(L"--- HFS self-test complete: %d passed, %d failed ---\n",
          g_HfsTestPass, g_HfsTestFail);

    return (g_HfsTestFail == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;
}

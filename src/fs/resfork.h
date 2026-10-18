#ifndef __PPC_RESFORK_H__
#define __PPC_RESFORK_H__

#include <efi.h>

// Classic Mac OS resource-fork reader (the Resource Manager's back end).
//
// The bootloader reads a file's resource fork (an HFS resource fork is a
// container of the resource map plus resource data) into a host buffer and
// hands it here with PpcResForkLoad. The interpreter then answers the guest's
// Resource Manager traps (CurResFile/UseResFile/GetResource/Get1Resource/
// GetIndString/...) out of this one loaded fork.
//
// Resource-map layout (offsets are big-endian, fork-relative):
//   0   16 bytes  fork header: dataOff, mapOff, dataLen, mapLen
//   mapOff+16     reserved (4)
//   mapOff+20     2 bytes  file reference number
//   22            2 bytes  file attributes
//   24            2 bytes  offset to the type list
//   26            2 bytes  offset to the name list
//   mapOff+tlOff  2 bytes  (type count - 1)          [count-prefixed map]
//   mapOff+tlOff+2         8-byte type entries: type(4), numRes-1(2), refOff(2)
//   ref list (per type) at mapOff + typeListOff + refOff:
//     12-byte entries: id (signed 2), nameOff (2), attrs (1), dataOff (3)
//   name list at mapOff + nameListOffset: Pascal strings
//   resource data at dataOff + dataOff: 4-byte length prefix then the bytes
//
// Some maps omit the 2-byte count prefix and terminate the type list with a
// zero type; both forms are accepted.

#define PPC_RES_NAME_MAX 255

// A resolved resource. Data points into the loaded fork buffer (host memory);
// the caller must copy it before the fork is unloaded.
typedef struct {
    INT16        Id;
    UINT8        Attributes;
    UINT32       Size;
    const UINT8* Data;
    UINT8        NameLen;
    CHAR8        Name[PPC_RES_NAME_MAX + 1];
} PPC_RESOURCE;

/**
  Install a resource fork read from a file. The buffer must stay valid for as
  long as the fork is used (the bootloader keeps it in pool memory, which
  persists because the guest never calls ExitBootServices).
  @param[in] Data  Resource fork bytes
  @param[in] Size  Resource fork length
**/
VOID
PpcResForkLoad (
    IN const UINT8* Data,
    IN UINTN        Size
    );

/** Forget the loaded fork. */
VOID
PpcResForkUnload (
    VOID
    );

/** @retval TRUE if a fork is loaded and its header validates. */
BOOLEAN
PpcResForkReady (
    VOID
    );

/** Current resource-file reference number (CurResFile). */
UINT32
PpcResForkCurRef (
    VOID
    );

/** Select the current resource-file reference (UseResFile). */
VOID
PpcResForkUseRef (
    IN UINT32 Ref
    );

/** Number of resources of a type (0 if the type is absent). */
UINT32
PpcResForkCountType (
    IN UINT32 Type
    );

/**
  Look up a resource by 4CC type and signed ID.
  @param[in]  Type  Resource type (big-endian 4CC as a UINT32)
  @param[in]  Id    Resource ID
  @param[out] Out   Filled in on success
  @retval TRUE  found
  @retval FALSE not found (or no fork loaded)
**/
BOOLEAN
PpcResForkGet (
    IN  UINT32      Type,
    IN  INT16       Id,
    OUT PPC_RESOURCE* Out
    );

/**
  Look up the Index-th (0-based) resource of a type.
  @retval TRUE  found
  @retval FALSE index out of range (or no fork loaded)
**/
BOOLEAN
PpcResForkGetIndex (
    IN  UINT32      Type,
    IN  UINT16      Index,
    OUT PPC_RESOURCE* Out
    );

/**
  Verify the reader against the currently installed fork: walk every type and
  reference, checking bounds. Interpretation of results is left to the caller.
  @retval EFI_SUCCESS    Fork valid (or no fork installed)
  @retval EFI_LOAD_ERROR A reference pointed outside the fork
**/
EFI_STATUS
PpcResForkValidate (
    VOID
    );

#endif // __PPC_RESFORK_H__

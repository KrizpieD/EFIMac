#include "resfork.h"
#include <efilib.h>

// ---------------------------------------------------------------------------
// Classic resource-fork reader (see resfork.h for the on-disk layout).
// ---------------------------------------------------------------------------

static const UINT8* g_Fork;
static UINTN        g_Size;
static BOOLEAN      g_Ready;
static UINT32       g_DataOff;
static UINT32       g_MapOff;
static UINT32       g_DataLen;
static UINT32       g_MapLen;
static UINT16       g_TypeListOff;
static UINT16       g_NameListOff;
static UINT16       g_TypeCount;
static BOOLEAN      g_CountMode;
static UINT32       g_CurRef;

static UINT16
Rf16 (
    IN const UINT8* P
    )
{
    return (UINT16)(((UINT16)P[0] << 8) | (UINT16)P[1]);
}

static UINT32
Rf32 (
    IN const UINT8* P
    )
{
    return ((UINT32)P[0] << 24) | ((UINT32)P[1] << 16) |
           ((UINT32)P[2] << 8)  | (UINT32)P[3];
}

static BOOLEAN
RfInRange (
    IN UINTN Off,
    IN UINTN Len
    )
{
    return (Off <= g_Size) && (Len <= g_Size - Off);
}

static const UINT8*
RfTypeEntry (
    IN UINT16 Index
    )
{
    UINTN Base = (UINTN)g_MapOff + (UINTN)g_TypeListOff +
                 (g_CountMode ? 2u : 0u);
    return g_Fork + Base + (UINTN)Index * 8u;
}

static INT32
RfFindType (
    IN UINT32 Type
    )
{
    UINT16 I;
    for (I = 0; I < g_TypeCount; I++) {
        if (Rf32 (RfTypeEntry (I)) == Type) {
            return (INT32)I;
        }
    }
    return -1;
}

VOID
PpcResForkUnload (
    VOID
    )
{
    g_Fork        = NULL;
    g_Size        = 0;
    g_Ready       = FALSE;
    g_DataOff     = 0;
    g_MapOff      = 0;
    g_DataLen     = 0;
    g_MapLen      = 0;
    g_TypeListOff = 0;
    g_NameListOff = 0;
    g_TypeCount   = 0;
    g_CountMode   = FALSE;
    g_CurRef      = 0;
}

VOID
PpcResForkLoad (
    IN const UINT8* Data,
    IN UINTN        Size
    )
{
    UINTN   TlBase;
    UINT16  Candidate;
    BOOLEAN FirstLooksType;

    PpcResForkUnload ();
    if (Data == NULL || Size < 16u) {
        return;
    }

    g_DataOff = Rf32 (Data + 0);
    g_MapOff  = Rf32 (Data + 4);
    g_DataLen = Rf32 (Data + 8);
    g_MapLen  = Rf32 (Data + 12);

    /* The map header begins with a 16-byte copy of the fork header; the map
       proper (offsets, type list, names) must live inside the fork. */
    if (g_MapOff < 16u || g_MapOff + 28u > Size ||
        (UINTN)g_MapOff + (UINTN)g_MapLen > Size ||
        (UINTN)g_DataOff + (UINTN)g_DataLen > Size) {
        return;
    }

    g_Fork    = Data;
    g_Size    = Size;
    g_CurRef  = 0;

    g_TypeListOff = Rf16 (Data + g_MapOff + 24u);
    g_NameListOff = Rf16 (Data + g_MapOff + 26u);

    TlBase = (UINTN)g_MapOff + (UINTN)g_TypeListOff;
    if (!RfInRange (TlBase, 8u)) {
        PpcResForkUnload ();
        return;
    }

    /* Two spellings are seen in the wild: a 4CC-terminated type list, and a
       list prefixed by a 2-byte (count-1). Pick whichever the bytes support. */
    FirstLooksType = ((Data[TlBase + 0] >= 0x20u && Data[TlBase + 0] < 0x7Fu) &&
                      (Data[TlBase + 1] >= 0x20u && Data[TlBase + 1] < 0x7Fu) &&
                      (Data[TlBase + 2] >= 0x20u && Data[TlBase + 2] < 0x7Fu) &&
                      (Data[TlBase + 3] >= 0x20u && Data[TlBase + 3] < 0x7Fu));

    if (FirstLooksType) {
        g_CountMode = FALSE;
        g_TypeCount = 0;
        while (g_TypeCount < 4096u) {
            const UINT8* E = g_Fork + TlBase + (UINTN)g_TypeCount * 8u;
            if (!RfInRange ((UINTN)(E - g_Fork), 8u)) { break; }
            if (Rf32 (E) == 0u) { break; }
            g_TypeCount++;
        }
    } else {
        Candidate   = (UINT16)(Rf16 (Data + TlBase) + 1u);
        g_CountMode = TRUE;
        g_TypeCount = Candidate;
        if (g_TypeCount == 0u ||
            (UINTN)g_TypeListOff + 2u + (UINTN)g_TypeCount * 8u >
                (UINTN)g_NameListOff) {
            PpcResForkUnload ();
            return;
        }
    }

    g_Ready = TRUE;
}

BOOLEAN
PpcResForkReady (
    VOID
    )
{
    return g_Ready;
}

UINT32
PpcResForkCurRef (
    VOID
    )
{
    return g_CurRef;
}

VOID
PpcResForkUseRef (
    IN UINT32 Ref
    )
{
    g_CurRef = Ref & 0xFFFFu;
}

UINT32
PpcResForkCountType (
    IN UINT32 Type
    )
{
    INT32 Ti;
    if (!g_Ready) {
        return 0;
    }
    Ti = RfFindType (Type);
    if (Ti < 0) {
        return 0;
    }
    return (UINT32)Rf16 (RfTypeEntry ((UINT16)Ti) + 4) + 1u;
}

BOOLEAN
PpcResForkGetIndex (
    IN  UINT32       Type,
    IN  UINT16       Index,
    OUT PPC_RESOURCE* Out
    )
{
    INT32        Ti;
    const UINT8* E;
    UINT16       Num;
    UINT16       RefOff;
    UINTN        RefEntry;
    const UINT8* R;
    UINT16       NameOff;
    UINT32       DataOff;
    UINT32       ResLen;
    UINTN        DataStart;

    if (!g_Ready || Out == NULL) {
        return FALSE;
    }

    Ti = RfFindType (Type);
    if (Ti < 0) {
        return FALSE;
    }
    E   = RfTypeEntry ((UINT16)Ti);
    Num = (UINT16)(Rf16 (E + 4) + 1u);
    if (Index >= Num) {
        return FALSE;
    }
    RefOff = Rf16 (E + 6);

    /* Reference lists are measured from the type-list base. */
    RefEntry = (UINTN)g_MapOff + (UINTN)g_TypeListOff + (UINTN)RefOff +
               (UINTN)Index * 12u;
    if (!RfInRange (RefEntry, 12u)) {
        return FALSE;
    }
    R = g_Fork + RefEntry;

    Out->Id         = (INT16)Rf16 (R + 0);
    NameOff         = Rf16 (R + 2);
    Out->Attributes = R[4];
    DataOff         = ((UINT32)R[5] << 16) | ((UINT32)R[6] << 8) | (UINT32)R[7];

    DataStart = (UINTN)g_DataOff + (UINTN)DataOff;
    if (!RfInRange (DataStart, 4u)) {
        return FALSE;
    }
    ResLen = Rf32 (g_Fork + DataStart);
    if (!RfInRange (DataStart + 4u, ResLen)) {
        return FALSE;
    }
    Out->Size = ResLen;
    Out->Data = g_Fork + DataStart + 4u;

    Out->NameLen = 0;
    Out->Name[0] = 0;
    if (NameOff != 0xFFFFu) {
        UINTN Nl = (UINTN)g_MapOff + (UINTN)g_NameListOff + (UINTN)NameOff;
        UINT8 L;
        if (RfInRange (Nl, 1u)) {
            L = g_Fork[Nl];
            if (RfInRange (Nl + 1u, L)) {
                UINT8 I;
                UINT8 Cap = (L > PPC_RES_NAME_MAX) ? (UINT8)PPC_RES_NAME_MAX : L;
                for (I = 0; I < Cap; I++) {
                    Out->Name[I] = (CHAR8)g_Fork[Nl + 1u + I];
                }
                Out->Name[Cap] = 0;
                Out->NameLen   = Cap;
            }
        }
    }
    return TRUE;
}

BOOLEAN
PpcResForkGet (
    IN  UINT32       Type,
    IN  INT16        Id,
    OUT PPC_RESOURCE* Out
    )
{
    UINT32 Count;
    UINT16 I;

    if (!g_Ready) {
        return FALSE;
    }
    Count = PpcResForkCountType (Type);
    for (I = 0; I < Count; I++) {
        if (PpcResForkGetIndex (Type, I, Out)) {
            if (Out->Id == Id) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

EFI_STATUS
PpcResForkValidate (
    VOID
    )
{
    UINT16 Ti;
    UINT32 Bad = 0;
    UINT32 Total = 0;

    if (!g_Ready) {
        return EFI_SUCCESS;
    }
    for (Ti = 0; Ti < g_TypeCount; Ti++) {
        UINT32 Num = PpcResForkCountType (Rf32 (RfTypeEntry (Ti)));
        UINT16 I;
        for (I = 0; I < Num; I++) {
            PPC_RESOURCE Res;
            Total++;
            if (!PpcResForkGetIndex (Rf32 (RfTypeEntry (Ti)), I, &Res)) {
                Bad++;
            }
        }
    }
    Print (L"RESFORK: validated %u resources across %u types (%u bad)\n",
           Total, (UINT32)g_TypeCount, Bad);
    return (Bad == 0) ? EFI_SUCCESS : EFI_LOAD_ERROR;
}

/* EFI-Mac setup front-end.
 *
 * Configuration persistence (a single NVRAM variable), the boot gate
 * (5-second countdown with the classic Macintosh face, F8 opens setup)
 * and the interactive configuration menu. All output goes through the
 * EFI text console, so it renders on the GOP display and on the serial
 * console alike.
 */

#include <efi.h>
#include <efilib.h>

#include "ui/ui.h"
#include "hardware/abstraction.h"
#include "boot/bootloader.h"
#include "platform/uefi_interface.h"

// NVRAM variable that stores the saved configuration.
STATIC EFI_GUID g_UiConfigGuid = PPC_CONFIG_VARIABLE_GUID;

// The UEFI scan code for Enter (gnu-efi's eficon.h stops at SCAN_ESC).
#define UI_SCAN_ENTER   0x000D

// ASCII "Macintosh" face shown during the boot countdown: the classic
// compact-Mac silhouette with a smiling screen and a floppy drive slot.
STATIC CONST CHAR16* UiMacFace[] = {
    L"              .----------------------------------.",
    L"             /                                    \\",
    L"            |   .----------------------------.     |",
    L"            |  (                              )    |",
    L"            |   |    .------------------.    |     |",
    L"            |   |   |    (o)    (o)     |    |     |",
    L"            |   |   |       ----        |    |     |",
    L"            |   |   |     \\_____/       |    |     |",
    L"            |   |    '------------------'    |     |",
    L"            |   |    .------------------.    |     |",
    L"            |   |   |  |  |  |  |  |   |     |     |",
    L"            |   |    '------------------'    |     |",
    L"            |  (                              )    |",
    L"            |   '----------------------------'     |",
    L"             \\                                    /",
    L"              '----------------------------------'"
};
#define UI_MAC_FACE_LINES  (sizeof(UiMacFace) / sizeof(UiMacFace[0]))

// ---------------------------- console helpers ----------------------------

STATIC
VOID
UiSetAttr (
    IN UINTN Attr
    )
{
    if (ST != NULL && ST->ConOut != NULL) {
        ST->ConOut->SetAttribute(ST->ConOut, Attr);
    }
}

STATIC
VOID
UiConsoleSize (
    OUT UINTN* Columns,
    OUT UINTN* Rows
    )
{
    UINTN C = 80;
    UINTN R = 25;
    if (ST != NULL && ST->ConOut != NULL && ST->ConOut->Mode != NULL) {
        ST->ConOut->QueryMode(ST->ConOut, ST->ConOut->Mode->Mode, &C, &R);
    }
    if (Columns != NULL) {
        *Columns = C;
    }
    if (Rows != NULL) {
        *Rows = R;
    }
}

STATIC
VOID
UiDrawCentered (
    IN UINTN         Row,
    IN CONST CHAR16* Text
    )
{
    UINTN Columns = 80;
    UINTN Rows    = 25;
    UiConsoleSize(&Columns, &Rows);
    UINTN Len = StrLen(Text);
    UINTN Col = (Len >= Columns) ? 0 : ((Columns - Len) / 2);
    if (Row < Rows) {
        PrintAt(Col, Row, L"%s", Text);
    }
}

STATIC
VOID
UiDrawCenteredPrint (
    IN UINTN         Row,
    IN CONST CHAR16* Fmt,
    ...
    )
{
    CHAR16 Buf[96];
    va_list Args;
    va_start(Args, Fmt);
    UnicodeVSPrint(Buf, sizeof(Buf), Fmt, Args);
    va_end(Args);
    UiDrawCentered(Row, Buf);
}

STATIC
CONST CHAR16*
UiVideoResolution (
    IN UINT32 Mode
    )
{
    switch (Mode) {
    case PPC_GRAPHICS_MODE_640x480:   return L"640x480";
    case PPC_GRAPHICS_MODE_800x600:   return L"800x600";
    case PPC_GRAPHICS_MODE_1024x768:  return L"1024x768";
    case PPC_GRAPHICS_MODE_1280x1024: return L"1280x1024";
    default:                          return L"default";
    }
}

// -------------------------- configuration storage --------------------------

STATIC
UINT32
UiConfigChecksum (
    IN const PPC_CONFIG* Config
    )
{
    const UINT8* Bytes = (const UINT8*)Config;
    UINT32 Sum = 0;
    UINTN  I;
    for (I = 0; I < sizeof(PPC_CONFIG) - sizeof(Config->Checksum); I++) {
        Sum += Bytes[I];
    }
    return Sum;
}

EFI_STATUS
EFIAPI
PpcConfigSetDefaults (
    OUT PPC_CONFIG* Config
    )
{
    if (Config == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    ZeroMem(Config, sizeof(*Config));
    Config->Signature       = PPC_CONFIG_SIGNATURE;
    Config->Version         = PPC_CONFIG_VERSION;
    Config->BootMode        = PPC_BOOT_MODE_NORMAL;
    Config->MemorySizeMB    = 256;
    Config->VideoMode       = PPC_GRAPHICS_MODE_1024x768;
    Config->BootDeviceIndex = PPC_CONFIG_AUTO_BOOT_DEVICE;
    Config->AudioEnabled    = TRUE;
    Config->NetworkEnabled  = TRUE;
    Config->DebugEnabled    = TRUE;
    Config->Checksum        = UiConfigChecksum(Config);
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PpcConfigLoad (
    OUT PPC_CONFIG* Config
    )
{
    if (Config == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    PpcConfigSetDefaults(Config);

    UINTN      DataSize = sizeof(*Config);
    UINT32     Attributes = 0;
    EFI_STATUS Status = PpcGetVariable(
        L"EFIMacCfg", &g_UiConfigGuid, &Attributes, &DataSize, Config);
    if (EFI_ERROR(Status)) {
        PpcConfigSetDefaults(Config);
        return Status;
    }

    if (DataSize != sizeof(*Config) ||
        Config->Signature != PPC_CONFIG_SIGNATURE ||
        Config->Version   != PPC_CONFIG_VERSION ||
        Config->Checksum  != UiConfigChecksum(Config)) {
        Print(L"[setup] Saved configuration is invalid; using defaults\n");
        PpcConfigSetDefaults(Config);
        return EFI_LOAD_ERROR;
    }

    Print(L"[setup] Loaded saved configuration "
          L"(RAM %d MB, video %s, boot device %d, debug %s)\n",
          (UINTN)Config->MemorySizeMB,
          UiVideoResolution(Config->VideoMode),
          (UINTN)Config->BootDeviceIndex,
          Config->DebugEnabled ? L"on" : L"off");
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PpcConfigSave (
    IN const PPC_CONFIG* Config
    )
{
    if (Config == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    PPC_CONFIG Copy = *Config;
    Copy.Checksum = UiConfigChecksum(&Copy);

    UINT32 Attributes =
        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
    return PpcSetVariable(
        L"EFIMacCfg", &g_UiConfigGuid, Attributes, sizeof(Copy), &Copy);
}

EFI_STATUS
EFIAPI
PpcVideoModeResolution (
    IN  UINT32 Mode,
    OUT UINT32* Width,
    OUT UINT32* Height
    )
{
    if (Width == NULL || Height == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    switch (Mode) {
    case PPC_GRAPHICS_MODE_640x480:   *Width = 640;  *Height = 480;  return EFI_SUCCESS;
    case PPC_GRAPHICS_MODE_800x600:   *Width = 800;  *Height = 600;  return EFI_SUCCESS;
    case PPC_GRAPHICS_MODE_1024x768:  *Width = 1024; *Height = 768;  return EFI_SUCCESS;
    case PPC_GRAPHICS_MODE_1280x1024: *Width = 1280; *Height = 1024; return EFI_SUCCESS;
    default:
        *Width = 0;
        *Height = 0;
        return EFI_NOT_FOUND;
    }
}

// --------------------------- boot gate + countdown --------------------------

BOOLEAN
EFIAPI
PpcBootGateWait (
    IN const PPC_CONFIG* Config
    )
{
    SIMPLE_INPUT_INTERFACE* ConIn = (ST != NULL) ? ST->ConIn : NULL;
    UINTN Columns = 80;
    UINTN Rows    = 25;
    UiConsoleSize(&Columns, &Rows);

    UiSetAttr(EFI_TEXT_ATTR(EFI_WHITE, EFI_BACKGROUND_BLACK));
    if (ST != NULL && ST->ConOut != NULL) {
        ST->ConOut->ClearScreen(ST->ConOut);
    }

    // Title and hint.
    UiSetAttr(EFI_TEXT_ATTR(EFI_LIGHTCYAN, EFI_BACKGROUND_BLACK));
    UiDrawCentered(1, L"EFI-Mac  -  PowerPC Mac OS Boot Loader");
    UiSetAttr(EFI_TEXT_ATTR(EFI_WHITE, EFI_BACKGROUND_BLACK));
    UiDrawCenteredPrint(2, L"Press F8 within %d seconds to enter setup",
                   (UINTN)PPC_BOOT_GATE_TIMEOUT_SECONDS);

    // The classic Macintosh face.
    UINTN FaceRow = 4;
    for (UINTN I = 0; I < UI_MAC_FACE_LINES; I++) {
        UiDrawCentered(FaceRow + I, UiMacFace[I]);
    }

    // Configuration summary under the face.
    UINTN SummaryRow = FaceRow + (UINTN)UI_MAC_FACE_LINES + 1;
    if (SummaryRow < Rows) {
        CHAR16 Summary[96];
        UnicodeSPrint(Summary, sizeof(Summary),
                      L"RAM %d MB   Video %s   Boot device %s",
                      (UINTN)(Config != NULL ? Config->MemorySizeMB : 256),
                      UiVideoResolution(Config != NULL ? Config->VideoMode
                                                       : PPC_GRAPHICS_MODE_1024x768),
                      (Config == NULL ||
                       Config->BootDeviceIndex == PPC_CONFIG_AUTO_BOOT_DEVICE)
                          ? L"Auto"
                          : L"Selected");
        UiSetAttr(EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BACKGROUND_BLACK));
        UiDrawCentered(SummaryRow, Summary);
    }

    // Countdown row (guard against short consoles).
    UINTN CountdownRow = SummaryRow + 1;
    if (CountdownRow >= Rows) {
        CountdownRow = (Rows > 0) ? Rows - 1 : 0;
    }

    // One-second periodic timer drives the countdown.
    EFI_EVENT TimerEvent = NULL;
    EFI_STATUS Status = BS->CreateEvent(EVT_TIMER, TPL_APPLICATION, NULL, NULL, &TimerEvent);
    if (EFI_ERROR(Status)) {
        return FALSE;
    }
    Status = BS->SetTimer(TimerEvent, TimerPeriodic, 10000000ULL);
    if (EFI_ERROR(Status)) {
        BS->CloseEvent(TimerEvent);
        return FALSE;
    }

    BOOLEAN EnterMenu  = FALSE;
    UINTN   SecondsLeft = PPC_BOOT_GATE_TIMEOUT_SECONDS;

    while (SecondsLeft > 0 && !EnterMenu) {
        UiSetAttr(EFI_TEXT_ATTR(EFI_LIGHTGREEN, EFI_BACKGROUND_BLACK));
        UiDrawCenteredPrint(CountdownRow,
                            L"Booting in %d seconds...   (F8 = setup)",
                            (UINTN)SecondsLeft);

        EFI_EVENT Events[2];
        UINTN EventCount = 0;
        Events[EventCount++] = TimerEvent;
        if (ConIn != NULL) {
            Events[EventCount++] = ConIn->WaitForKey;
        }

        UINTN Index = 0;
        BS->WaitForEvent(EventCount, Events, &Index);

        // Drain every queued key; F8 at any point arms the setup menu.
        if (ConIn != NULL) {
            EFI_INPUT_KEY Key;
            while (ConIn->ReadKeyStroke(ConIn, &Key) == EFI_SUCCESS) {
                if (Key.ScanCode == SCAN_F8 ||
                    Key.UnicodeChar == L'f' || Key.UnicodeChar == L'F') {
                    EnterMenu = TRUE;
                }
            }
        }

        // One timer tick per second (WaitForEvent re-arms the periodic
        // timer, so CheckEvent reports the tick that just fired).
        if (BS->CheckEvent(TimerEvent) == EFI_SUCCESS && SecondsLeft > 0) {
            SecondsLeft--;
        }
    }

    BS->CloseEvent(TimerEvent);

    if (EnterMenu) {
        UiSetAttr(EFI_TEXT_ATTR(EFI_YELLOW, EFI_BACKGROUND_BLACK));
        Print(L"\n\nOpening setup...\n");
    }
    return EnterMenu;
}

// -------------------------------- setup menu --------------------------------

typedef enum {
    UI_ROW_BOOT_DEVICE,
    UI_ROW_MEMORY,
    UI_ROW_VIDEO,
    UI_ROW_AUDIO,
    UI_ROW_NETWORK,
    UI_ROW_DEBUG,
    UI_ROW_BOOT_MODE,
    UI_ROW_SEPARATOR,
    UI_ROW_SAVE_EXIT,
    UI_ROW_DEFAULTS,
    UI_ROW_EXIT,
    UI_ROW_COUNT
} UI_ROW;

STATIC CONST CHAR16* UiRowLabels[UI_ROW_COUNT] = {
    L"Boot device",
    L"Memory size",
    L"Video mode",
    L"Audio device",
    L"Network interfaces",
    L"Debug output",
    L"Boot mode",
    L"",
    L"Save configuration and exit",
    L"Reset to defaults",
    L"Exit without saving"
};

// ------------------------------ row helpers --------------------------------

STATIC
UINTN
UiBootDeviceCount (
    VOID
    )
{
    PPC_BLOCK_IO_INFO Bio;
    if (EFI_ERROR(PpcGetBlockIoInfo(&Bio))) {
        return 1;   // only "Auto"
    }
    return 1 + Bio.DeviceCount;   // "Auto" + one choice per device
}

STATIC
UINTN
UiBootDeviceChoice (
    IN const PPC_CONFIG* Config
    )
{
    if (Config->BootDeviceIndex == PPC_CONFIG_AUTO_BOOT_DEVICE) {
        return 0;
    }
    return 1 + Config->BootDeviceIndex;
}

STATIC
VOID
UiBootDeviceSelect (
    IN OUT PPC_CONFIG* Config,
    IN UINTN           Choice
    )
{
    Config->BootDeviceIndex = (Choice == 0)
        ? PPC_CONFIG_AUTO_BOOT_DEVICE
        : (Choice - 1);
}

STATIC
VOID
UiBootDeviceLabel (
    IN UINTN   Choice,
    OUT CHAR16* Buf,
    IN UINTN   BufSize
    )
{
    if (Choice == 0) {
        UnicodeSPrint(Buf, BufSize, L"Auto (first HFS volume)");
        return;
    }

    UINTN Index = Choice - 1;
    PPC_BLOCK_DEVICE_INFO Dev;
    if (EFI_ERROR(PpcGetBlockDeviceInfo(Index, &Dev)) ||
        Dev.BlockSize == 0 || Dev.BlockCount == 0) {
        UnicodeSPrint(Buf, BufSize, L"Device %d (not present)", (UINTN)Index);
        return;
    }

    UnicodeSPrint(Buf, BufSize, L"Device %d (%s, %d MB)",
                  (UINTN)Index,
                  Dev.Removable ? L"CD-ROM" : L"Hard disk",
                  (UINTN)(Dev.BlockCount * Dev.BlockSize / 1024 / 1024));
}

STATIC
UINTN
UiRowChoiceCount (
    IN UI_ROW Row
    )
{
    switch (Row) {
    case UI_ROW_BOOT_DEVICE: return UiBootDeviceCount();
    case UI_ROW_MEMORY:      return 3;   // 256 / 512 / 1024 MB
    case UI_ROW_VIDEO:       return 4;
    case UI_ROW_AUDIO:       return 2;
    case UI_ROW_NETWORK:     return 2;
    case UI_ROW_DEBUG:       return 2;
    case UI_ROW_BOOT_MODE:   return 3;
    default:                 return 1;
    }
}

STATIC
UINTN
UiRowChoice (
    IN UI_ROW         Row,
    IN const PPC_CONFIG* Config
    )
{
    switch (Row) {
    case UI_ROW_BOOT_DEVICE:
        return UiBootDeviceChoice(Config);
    case UI_ROW_MEMORY: {
        static const UINT32 Sizes[3] = {256, 512, 1024};
        for (UINTN I = 0; I < 3; I++) {
            if (Config->MemorySizeMB == Sizes[I]) {
                return I;
            }
        }
        return 0;
    }
    case UI_ROW_VIDEO: {
        static const UINT32 Modes[4] = {
            PPC_GRAPHICS_MODE_640x480, PPC_GRAPHICS_MODE_800x600,
            PPC_GRAPHICS_MODE_1024x768, PPC_GRAPHICS_MODE_1280x1024
        };
        for (UINTN I = 0; I < 4; I++) {
            if (Config->VideoMode == Modes[I]) {
                return I;
            }
        }
        return 2;   // 1024x768 default
    }
    case UI_ROW_AUDIO:
        return Config->AudioEnabled ? 1 : 0;
    case UI_ROW_NETWORK:
        return Config->NetworkEnabled ? 1 : 0;
    case UI_ROW_DEBUG:
        return Config->DebugEnabled ? 1 : 0;
    case UI_ROW_BOOT_MODE:
        return (Config->BootMode < PPC_BOOT_MODE_DIAGNOSTIC)
            ? (UINTN)Config->BootMode : 0;
    default:
        return 0;
    }
}

STATIC
VOID
UiRowCycle (
    IN OUT PPC_CONFIG* Config,
    IN UI_ROW          Row,
    IN INTN            Direction
    )
{
    UINTN Count = UiRowChoiceCount(Row);
    UINTN Cur   = UiRowChoice(Row, Config);
    UINTN Next  = (UINTN)(((INTN)Cur + Direction + (INTN)Count) % (INTN)Count);

    switch (Row) {
    case UI_ROW_BOOT_DEVICE:
        UiBootDeviceSelect(Config, Next);
        break;
    case UI_ROW_MEMORY: {
        static const UINT32 Sizes[3] = {256, 512, 1024};
        Config->MemorySizeMB = Sizes[Next];
        break;
    }
    case UI_ROW_VIDEO: {
        static const UINT32 Modes[4] = {
            PPC_GRAPHICS_MODE_640x480, PPC_GRAPHICS_MODE_800x600,
            PPC_GRAPHICS_MODE_1024x768, PPC_GRAPHICS_MODE_1280x1024
        };
        Config->VideoMode = Modes[Next];
        break;
    }
    case UI_ROW_AUDIO:
        Config->AudioEnabled = (Next != 0);
        break;
    case UI_ROW_NETWORK:
        Config->NetworkEnabled = (Next != 0);
        break;
    case UI_ROW_DEBUG:
        Config->DebugEnabled = (Next != 0);
        break;
    case UI_ROW_BOOT_MODE:
        Config->BootMode = (Next == 0) ? PPC_BOOT_MODE_NORMAL
                         : (Next == 1) ? PPC_BOOT_MODE_RECOVERY
                         : PPC_BOOT_MODE_DIAGNOSTIC;
        break;
    default:
        break;
    }
}

STATIC
VOID
UiRowValue (
    IN UI_ROW            Row,
    IN const PPC_CONFIG* Config,
    OUT CHAR16*          Buf,
    IN UINTN             BufSize
    )
{
    UINTN Choice = UiRowChoice(Row, Config);
    switch (Row) {
    case UI_ROW_BOOT_DEVICE:
        UiBootDeviceLabel(Choice, Buf, BufSize);
        break;
    case UI_ROW_MEMORY:
        UnicodeSPrint(Buf, BufSize, L"%d MB", (UINTN)Config->MemorySizeMB);
        break;
    case UI_ROW_VIDEO:
        UnicodeSPrint(Buf, BufSize, L"%s", UiVideoResolution(Config->VideoMode));
        break;
    case UI_ROW_AUDIO:
        UnicodeSPrint(Buf, BufSize, L"%s",
                      Config->AudioEnabled ? L"Enabled" : L"Disabled");
        break;
    case UI_ROW_NETWORK:
        UnicodeSPrint(Buf, BufSize, L"%s",
                      Config->NetworkEnabled ? L"Enabled" : L"Disabled");
        break;
    case UI_ROW_DEBUG:
        UnicodeSPrint(Buf, BufSize, L"%s",
                      Config->DebugEnabled ? L"Enabled" : L"Disabled");
        break;
    case UI_ROW_BOOT_MODE:
        UnicodeSPrint(Buf, BufSize, L"%s",
                      Choice == 0 ? L"Normal"
                    : Choice == 1 ? L"Recovery"
                    : L"Diagnostic");
        break;
    default:
        Buf[0] = 0;
        break;
    }
}

// ------------------------------ menu drawing --------------------------------

STATIC
VOID
UiMenuDrawRow (
    IN UINTN         Row,
    IN CONST CHAR16* Label,
    IN CONST CHAR16* Value,
    IN BOOLEAN       Selected,
    IN UINTN         Columns
    )
{
    UINTN MaxLen = (Columns > 60) ? 60 : ((Columns > 0) ? Columns - 1 : 0);
    CHAR16 Line[80];

    UnicodeSPrint(Line, sizeof(Line), L"  %s", Label);
    UINTN Len = StrLen(Line);
    if (Len > 30) {
        Len = 30;
    }
    while (Len < 31) {
        Line[Len++] = L' ';
    }
    Line[Len] = 0;

    UnicodeSPrint(Line + Len, (80 - Len) * sizeof(CHAR16), L"%s", Value);
    Len = StrLen(Line);
    if (Len > MaxLen) {
        Len = MaxLen;
    }
    while (Len < MaxLen) {
        Line[Len++] = L' ';
    }
    Line[MaxLen] = 0;

    UiSetAttr(Selected ? EFI_TEXT_ATTR(EFI_BLACK, EFI_LIGHTCYAN)
                       : EFI_TEXT_ATTR(EFI_WHITE, EFI_BACKGROUND_BLACK));
    PrintAt(0, Row, L"%s", Line);
}

EFI_STATUS
EFIAPI
PpcShowConfigMenu (
    IN OUT PPC_CONFIG* Config
    )
{
    if (Config == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Make sure the block device list exists so the Boot device menu can
    // enumerate every attached disc.
    {
        PPC_BLOCK_IO_INFO Bio;
        if (EFI_ERROR(PpcGetBlockIoInfo(&Bio))) {
            PpcInitializeBlockIo(1);
        }
    }

    SIMPLE_INPUT_INTERFACE* ConIn = (ST != NULL) ? ST->ConIn : NULL;
    if (ConIn == NULL) {
        Print(L"[setup] No text input device; skipping configuration menu\n");
        return EFI_UNSUPPORTED;
    }

    UINTN Columns = 80;
    UINTN Rows    = 25;
    UiConsoleSize(&Columns, &Rows);

    UINTN    Selected   = UI_ROW_BOOT_DEVICE;
    BOOLEAN  Exit       = FALSE;
    BOOLEAN  Saved      = FALSE;
    EFI_STATUS SaveStatus = EFI_SUCCESS;

    UiSetAttr(EFI_TEXT_ATTR(EFI_WHITE, EFI_BACKGROUND_BLACK));
    if (ST != NULL && ST->ConOut != NULL) {
        ST->ConOut->ClearScreen(ST->ConOut);
    }

    while (!Exit) {
        // Title and hint.
        UiSetAttr(EFI_TEXT_ATTR(EFI_YELLOW, EFI_BACKGROUND_BLACK));
        UiDrawCentered(0, L"EFI-Mac Setup  -  PowerPC Mac OS Boot Configuration");
        UiSetAttr(EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BACKGROUND_BLACK));
        UiDrawCentered(1, L"Arrows move / change value   Enter selects   Esc exits");

        // Menu rows.
        for (UINTN R = 0; R < UI_ROW_COUNT; R++) {
            UINTN RowY = 3 + R;
            if (RowY >= Rows) {
                break;
            }
            if (R == UI_ROW_SEPARATOR) {
                UiSetAttr(EFI_TEXT_ATTR(EFI_DARKGRAY, EFI_BACKGROUND_BLACK));
                PrintAt(0, RowY, L"  -------------------------------------------");
                continue;
            }
            CHAR16 Value[64];
            UiRowValue((UI_ROW)R, Config, Value, sizeof(Value));
            UiMenuDrawRow(RowY, UiRowLabels[R], Value, (R == Selected), Columns);
        }

        // Status line.
        UiSetAttr(EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BACKGROUND_BLACK));
        if (Rows > 0) {
            PrintAt(0, Rows - 1,
                    L"S = save and exit    D = reset to defaults    Esc = exit");
        }

        // Wait for a key.
        EFI_INPUT_KEY Key;
        while (ConIn->ReadKeyStroke(ConIn, &Key) != EFI_SUCCESS) {
            UINTN Index = 0;
            BS->WaitForEvent(1, &ConIn->WaitForKey, &Index);
        }

        if (Key.ScanCode == SCAN_UP) {
            Selected = (Selected == 0) ? (UI_ROW_COUNT - 1) : (Selected - 1);
        } else if (Key.ScanCode == SCAN_DOWN) {
            Selected = (Selected + 1) % UI_ROW_COUNT;
        } else if (Key.ScanCode == SCAN_LEFT) {
            UiRowCycle(Config, (UI_ROW)Selected, -1);
        } else if (Key.ScanCode == SCAN_RIGHT) {
            UiRowCycle(Config, (UI_ROW)Selected, +1);
        } else if (Key.ScanCode == SCAN_ESC) {
            Exit = TRUE;
        } else if (Key.ScanCode == UI_SCAN_ENTER ||
                   Key.UnicodeChar == 0x0D || Key.UnicodeChar == 0x0A) {
            switch ((UI_ROW)Selected) {
            case UI_ROW_SAVE_EXIT:
                SaveStatus = PpcConfigSave(Config);
                Saved = !EFI_ERROR(SaveStatus);
                Exit = TRUE;
                break;
            case UI_ROW_DEFAULTS:
                PpcConfigSetDefaults(Config);
                break;
            case UI_ROW_EXIT:
                Exit = TRUE;
                break;
            default:
                UiRowCycle(Config, (UI_ROW)Selected, +1);
                break;
            }
        } else {
            switch (Key.UnicodeChar) {
            case L's':
            case L'S':
                SaveStatus = PpcConfigSave(Config);
                Saved = !EFI_ERROR(SaveStatus);
                Exit = TRUE;
                break;
            case L'd':
            case L'D':
                PpcConfigSetDefaults(Config);
                break;
            case L'e':
            case L'E':
            case L'x':
            case L'X':
                Exit = TRUE;
                break;
            default:
                break;
            }
        }
    }

    // Back to the normal boot output.
    UiSetAttr(EFI_TEXT_ATTR(EFI_WHITE, EFI_BACKGROUND_BLACK));
    if (ST != NULL && ST->ConOut != NULL) {
        ST->ConOut->ClearScreen(ST->ConOut);
    }

    if (Saved) {
        Print(L"[setup] Configuration saved.\n");
    } else if (EFI_ERROR(SaveStatus)) {
        Print(L"[setup] Failed to save configuration: %r\n", SaveStatus);
    }
    Print(L"[setup] Booting with the selected configuration...\n");

    return EFI_SUCCESS;
}

#ifndef __PPC_UI_H__
#define __PPC_UI_H__

#include <efi.h>

// Length of the boot gate countdown (seconds). Pressing F8 inside this
// window opens the setup menu.
#define PPC_BOOT_GATE_TIMEOUT_SECONDS  5u

// Vendor GUID under which the saved configuration lives in NVRAM.
#define PPC_CONFIG_VARIABLE_GUID \
  { 0x6F9AC2B8, 0x5E8A, 0x4C29, { 0x9E, 0x2F, 0x2F, 0x16, 0x5C, 0x49, 0x0D, 0x53 } }

#define PPC_CONFIG_SIGNATURE          0x4546494D  // "EFIM"
#define PPC_CONFIG_VERSION            1u

// BootDeviceIndex value meaning "auto-detect the first HFS volume".
#define PPC_CONFIG_AUTO_BOOT_DEVICE   0xFFFFFFFFu

// Persistent boot configuration. Kept deliberately flat and fixed-size so
// it round-trips through a single UEFI NVRAM variable with a checksum.
typedef struct {
    UINT32  Signature;          // PPC_CONFIG_SIGNATURE
    UINT32  Version;            // PPC_CONFIG_VERSION
    UINT32  BootMode;           // PPC_BOOT_MODE_*
    UINT32  MemorySizeMB;       // guest RAM size in MB
    UINT32  VideoMode;          // PPC_GRAPHICS_MODE_*
    UINT32  BootDeviceIndex;    // block device index (PPC_CONFIG_AUTO_BOOT_DEVICE = auto)
    BOOLEAN AudioEnabled;       // emulated audio device
    BOOLEAN NetworkEnabled;     // emulated network interfaces
    BOOLEAN DebugEnabled;       // verbose debug output
    UINT8   Reserved[9];        // padding so the checksum stays word aligned
    UINT32  Checksum;           // simple sum over the preceding bytes
} PPC_CONFIG;

/**
  Fill a configuration structure with the factory defaults.
  @param[out] Config  Structure to initialize
  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER  Config is NULL
**/
EFI_STATUS
EFIAPI
PpcConfigSetDefaults (
    OUT PPC_CONFIG* Config
    );

/**
  Load the saved configuration from NVRAM. On first boot or on corruption
  the structure is filled with defaults.
  @param[out] Config  Structure to receive the loaded configuration
  @retval EFI_SUCCESS       Configuration loaded
  @retval EFI_LOAD_ERROR    Stored configuration was corrupt (defaults used)
  @retval EFI_NOT_FOUND     No configuration stored yet (defaults used)
  @retval EFI_INVALID_PARAMETER  Config is NULL
**/
EFI_STATUS
EFIAPI
PpcConfigLoad (
    OUT PPC_CONFIG* Config
    );

/**
  Persist a configuration structure to NVRAM.
  @param[in] Config  Configuration to store
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcConfigSave (
    IN  const PPC_CONFIG* Config
    );

/**
  Translate a PPC_GRAPHICS_MODE_* value into pixel dimensions.
  @param[in]  Mode   PPC_GRAPHICS_MODE_* constant
  @param[out] Width  Horizontal resolution
  @param[out] Height Vertical resolution
  @retval EFI_SUCCESS     Resolution returned
  @retval EFI_NOT_FOUND   Unknown mode
**/
EFI_STATUS
EFIAPI
PpcVideoModeResolution (
    IN  UINT32 Mode,
    OUT UINT32* Width,
    OUT UINT32* Height
    );

/**
  Show the ASCII "Macintosh" splash and wait up to
  PPC_BOOT_GATE_TIMEOUT_SECONDS for F8. Clears and repaints the text
  console, so call it before any other screen output.
  @param[in] Config  Current configuration (shown as a summary)
  @retval TRUE   F8 was pressed; the setup menu should be shown
  @retval FALSE  Countdown expired; boot with the current configuration
**/
BOOLEAN
EFIAPI
PpcBootGateWait (
    IN  const PPC_CONFIG* Config
    );

/**
  Interactive configuration menu. Arrow keys move and change values,
  Enter activates an action row, Esc exits without saving, S saves.
  @param[in,out] Config  Configuration to edit
  @retval EFI_SUCCESS
  @retval EFI_UNSUPPORTED  No text input device available
**/
EFI_STATUS
EFIAPI
PpcShowConfigMenu (
    IN OUT PPC_CONFIG* Config
    );

#endif // __PPC_UI_H__

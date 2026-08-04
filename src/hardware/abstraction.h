#ifndef __PPC_HARDWARE_ABSTRACTION_H__
#define __PPC_HARDWARE_ABSTRACTION_H__

#include <efi.h>

// Graphics modes
#define PPC_GRAPHICS_MODE_DEFAULT   0
#define PPC_GRAPHICS_MODE_640x480   1
#define PPC_GRAPHICS_MODE_800x600   2
#define PPC_GRAPHICS_MODE_1024x768  3
#define PPC_GRAPHICS_MODE_1280x1024 4

// Hardware states
typedef enum {
    PPC_HARDWARE_GRAPHICS_MODE,
    PPC_HARDWARE_AUDIO_ENABLE,
    PPC_HARDWARE_STORAGE_DEVICES,
    PPC_HARDWARE_NETWORK_INTERFACES
} PPC_HARDWARE_STATE;

// Hardware information structure
typedef struct {
    BOOLEAN IsInitialized;
    UINT32  GraphicsMode;
    UINT32  AudioEnabled;
    UINT32  StorageDevices;
    UINT32  NetworkInterfaces;
} PPC_HARDWARE_INFO;

/**
  Initialize PowerPC hardware abstraction layer
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeHardwareAbstraction (
    VOID
    );

/**
  Initialize graphics subsystem
  @param[in] Width      Display width
  @param[in] Height     Display height
  @param[in] ColorDepth Color depth in bits
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeGraphics (
    IN UINT32 Width,
    IN UINT32 Height,
    IN UINT32 ColorDepth
    );

/**
  Initialize audio subsystem
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeAudio (
    VOID
    );

/**
  Initialize storage subsystem
  @param[in] DeviceCount Number of storage devices to initialize
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeStorage (
    IN UINT32 DeviceCount
    );

/**
  Initialize network subsystem
  @param[in] InterfaceCount Number of network interfaces to initialize
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeNetwork (
    IN UINT32 InterfaceCount
    );

/**
  Get hardware information
  @param[out] HardwareInfo Pointer to structure to fill with hardware info
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetHardwareInfo (
    OUT PPC_HARDWARE_INFO* HardwareInfo
    );

/**
  Handle hardware interrupts
  @param[in] InterruptNumber Number of interrupt to handle
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcHandleHardwareInterrupt (
    IN UINT32 InterruptNumber
    );

/**
  Set hardware state parameters
  @param[in] State Hardware state to set
  @param[in] Value Value to set the state to
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcSetHardwareState (
    IN PPC_HARDWARE_STATE State,
    IN UINT32             Value
    );

/**
  Get hardware state parameters
  @param[in]  State Hardware state to get
  @param[out] Value Pointer to store the state value
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetHardwareState (
    IN  PPC_HARDWARE_STATE State,
    OUT UINT32*            Value
    );

#endif // __PPC_HARDWARE_ABSTRACTION_H__
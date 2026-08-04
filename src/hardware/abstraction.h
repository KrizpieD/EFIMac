#ifndef __PPC_HARDWARE_ABSTRACTION_H__
#define __PPC_HARDWARE_ABSTRACTION_H__

#include <efi.h>

// Graphics modes
#define PPC_GRAPHICS_MODE_DEFAULT   0
#define PPC_GRAPHICS_MODE_640x480   1
#define PPC_GRAPHICS_MODE_800x600   2
#define PPC_GRAPHICS_MODE_1024x768  3
#define PPC_GRAPHICS_MODE_1280x1024 4

// Guest-visible framebuffer. Classic Macs expose the display as a fixed
// memory window the CPU writes to; this emulator carves that window out of
// guest RAM so the interpreter's load/store path targets it directly.
#define PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE 0x18000000

// Guest-visible audio device. UEFI defines no standard audio output protocol,
// so the emulator exposes an emulated device: a fixed ring buffer inside guest
// RAM that guest code fills with big-endian 16-bit stereo PCM samples. The
// host side tracks write/play cursors and reports buffer state.
#define PPC_AUDIO_BUFFER_GUEST_BASE 0x18800000
#define PPC_AUDIO_BUFFER_SIZE       0x2000
#define PPC_AUDIO_SAMPLE_RATE       44100
#define PPC_AUDIO_CHANNELS          2
#define PPC_AUDIO_BITS_PER_SAMPLE   16

// Audio device information
typedef struct {
    UINT32  GuestBase;    // Guest-visible address of the PCM ring buffer
    VOID*   HostBuffer;   // Host pointer to the ring buffer
    UINT64  Size;         // Buffer size in bytes
    UINT32  SampleRate;   // Sample rate in Hz
    UINT32  Channels;     // Channel count
    UINT32  BitsPerSample;
    UINT64  WriteCursor;  // Samples written by the guest
    UINT64  PlayCursor;   // Samples played by the host
} PPC_AUDIO_INFO;

// Framebuffer information
typedef struct {
    UINTN  Width;              // Visible width in pixels
    UINTN  Height;             // Visible height in pixels
    UINTN  Pitch;              // Bytes per scanline in the guest framebuffer
    UINTN  BitsPerPixel;       // 32 for the emulated VRAM layout
    UINT32 GuestBase;          // Guest-visible address of the framebuffer
    VOID*  HostBuffer;         // Host pointer to the framebuffer region
    UINT64 HostBufferSize;     // Size of the guest framebuffer region
    EFI_PHYSICAL_ADDRESS GopFrameBuffer; // Real GOP framebuffer base
    UINTN  GopPitch;           // GOP framebuffer bytes per scanline
    UINT32 PixelFormat;        // GOP pixel format (EFI_GRAPHICS_PIXEL_FORMAT)
} PPC_FRAMEBUFFER_INFO;

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
  Get information about the emulated audio device (PCM ring buffer).
  @param[out] Info  Audio device information structure
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcAudioGetBufferInfo (
    OUT PPC_AUDIO_INFO* Info
    );

/**
  Write a 16-bit big-endian PCM sample into the ring buffer (host-side helper;
  guest code writes through the interpreter's guest RAM path instead).
  @param[in] SampleIndex  Sample index (frame index, not byte offset)
  @param[in] Value        16-bit sample value (native byte order)
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcAudioWriteSample (
    IN UINT32  SampleIndex,
    IN UINT16  Value
    );

/**
  Read a 16-bit PCM sample back from the ring buffer (host-side helper).
  @param[in]  SampleIndex  Sample index to read
  @param[out] Value        Pointer to store the sample value
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcAudioReadSample (
    IN  UINT32  SampleIndex,
    OUT UINT16* Value
    );

/**
  Simulate playback: advance the play cursor by the given number of samples.
  @param[in] SampleCount  Number of samples to consume
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcAudioAdvancePlayback (
    IN UINT32 SampleCount
    );

/**
  Reset the audio ring buffer and cursors.
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcAudioReset (
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

/**
  Get information about the emulated framebuffer. The framebuffer lives
  inside guest RAM at PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE; guest code writes
  big-endian 0xRRGGBB00 pixels there and the host blits the window to the
  real GOP framebuffer.
  @param[out] Info  Framebuffer information structure
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetFrameBufferInfo (
    OUT PPC_FRAMEBUFFER_INFO* Info
    );

/**
  Copy the guest framebuffer window to the real GOP framebuffer, converting
  from big-endian RGB pixels to the GOP native pixel layout.
  @retval EFI_SUCCESS          Blit completed
  @retval EFI_NOT_READY        Graphics not initialized
**/
EFI_STATUS
EFIAPI
PpcGraphicsBlitToDisplay (
    VOID
    );

/**
  Fill the guest framebuffer with a solid color (host-side helper).
  @param[in] Color  Big-endian 0xRRGGBB00 color value
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGraphicsClear (
    IN UINT32 Color
    );

/**
  Write a single pixel into the guest framebuffer (host-side helper).
  @param[in] X      X coordinate
  @param[in] Y      Y coordinate
  @param[in] Color  Big-endian 0xRRGGBB00 color value
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGraphicsSetPixel (
    IN UINT32 X,
    IN UINT32 Y,
    IN UINT32 Color
    );

// Network information
#define PPC_MAX_NETWORK_INTERFACES 4

// Per-interface state snapshot
typedef struct {
    UINT8   MacAddress[6];      // Station MAC address
    BOOLEAN MediaPresent;       // Physical link / media present
    BOOLEAN TransmitTestPassed; // A real test frame was transmitted
} PPC_NETWORK_IFACE_INFO;

typedef struct {
    UINTN   InterfaceCount;     // Number of SNP interfaces initialized
    BOOLEAN MediaPresent;       // Primary interface: link/media present
    UINT8   MacAddress[6];      // Primary interface station MAC address
    UINTN   MaxPacketSize;      // Primary interface max frame size
    UINTN   IfType;             // Primary interface type (1 = Ethernet)
    UINTN   HwAddressSize;      // Primary interface hardware address size
    UINTN   MediaHeaderSize;    // Primary interface MAC header size
    UINTN   State;              // Primary interface SNP state
    BOOLEAN TransmitTestPassed; // Primary interface transmit result
    PPC_NETWORK_IFACE_INFO Interfaces[PPC_MAX_NETWORK_INTERFACES];
} PPC_NETWORK_INFO;

// Block device information
typedef struct {
    UINTN    DeviceCount;      // Number of BLOCK_IO devices found
    UINTN    SelectedIndex;    // Device used for the sector read test
    UINT32   MediaId;          // Media ID of the selected device
    UINT32   BlockSize;        // Bytes per block of the selected device
    EFI_LBA  LastBlock;        // Last valid block of the selected device
    BOOLEAN  ReadOnly;         // Selected device is read-only
    BOOLEAN  Removable;        // Selected device is removable
    BOOLEAN  MarkerFound;      // First sector contained the test marker
    UINT32   FirstSector0;     // First word (big-endian) of the test sector
} PPC_BLOCK_IO_INFO;

/**
  Enumerate every UEFI Block I/O protocol instance (whole disks and
  partitions) and report real media geometry.
  @param[in] DeviceCount  Minimum devices to expect (unused if not found)
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeBlockIo (
    IN UINT32 DeviceCount
    );

/**
  Get information about the block devices found by PpcInitializeBlockIo.
  @param[out] Info  Block device information structure
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetBlockIoInfo (
    OUT PPC_BLOCK_IO_INFO* Info
    );

/**
  Read a single block (sector) from a real UEFI Block I/O device via
  ReadBlocks. The buffer must be large enough for BlockSize bytes.
  @param[in]  Index   Index into the enumerated device list
  @param[in]  Lba     Logical block address to read
  @param[in]  BufferSize  Size of the buffer in bytes
  @param[out] Buffer  Buffer to receive the block
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcReadDiskBlock (
    IN  UINTN   Index,
    IN  EFI_LBA Lba,
    IN  UINTN   BufferSize,
    OUT VOID*   Buffer
    );

/**
  Get information about the real UEFI Simple Network Protocol interfaces.
  @param[out] Info  Network information structure
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGetNetworkInfo (
    OUT PPC_NETWORK_INFO* Info
    );

/**
  Draw a filled rectangle into the guest framebuffer (host-side helper).
  @param[in] X      Top-left X coordinate
  @param[in] Y      Top-left Y coordinate
  @param[in] Width  Rectangle width
  @param[in] Height Rectangle height
  @param[in] Color  Big-endian 0xRRGGBB00 color value
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcGraphicsDrawRect (
    IN UINT32 X,
    IN UINT32 Y,
    IN UINT32 Width,
    IN UINT32 Height,
    IN UINT32 Color
    );

#endif // __PPC_HARDWARE_ABSTRACTION_H__
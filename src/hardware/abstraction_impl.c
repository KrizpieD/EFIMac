#include "abstraction.h"
#include <efi.h>
#include <efilib.h>

// Hardware abstraction context with more complete implementation
typedef struct {
    BOOLEAN IsInitialized;
    UINT32  GraphicsMode;
    UINT32  AudioEnabled;
    UINT32  StorageDevices;
    UINT32  NetworkInterfaces;
    UINT32  SerialPorts;
    UINT32  IOPorts;
    VOID*   VideoBuffer;
    UINT64  VideoBufferSize;
    BOOLEAN GraphicsInitialized;
    BOOLEAN AudioInitialized;
    BOOLEAN StorageInitialized;
    BOOLEAN NetworkInitialized;
} PPC_HARDWARE_CONTEXT;

// Global hardware context
STATIC PPC_HARDWARE_CONTEXT g_HardwareContext = {0};

// Real UEFI protocol instances found by the HAL
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL  *g_Gop              = NULL;
STATIC UINT32                         g_FramebufferPitch = 0;
STATIC EFI_GRAPHICS_PIXEL_FORMAT      g_PixelFormat      = PixelRedGreenBlueReserved8BitPerColor;
STATIC EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **g_FileSystems    = NULL;
STATIC UINTN                          g_FileSystemCount  = 0;

/**
  Fill the real GOP framebuffer with a simple pattern so that the wired
  graphics output is visibly non-trivial. Handles both RGB and BGR
  pixel layouts.
**/
STATIC
VOID
GraphicsFillFramebuffer (
    IN UINT32 R,
    IN UINT32 G,
    IN UINT32 B
    )
{
    UINT32* Pixel = (UINT32*)(UINTN)g_HardwareContext.VideoBuffer;
    UINTN   RowBytes = g_FramebufferPitch;
    UINTN   Width;
    UINTN   Height;

    if (g_Gop == NULL || Pixel == NULL) {
        return;
    }

    Width  = g_Gop->Mode->Info->HorizontalResolution;
    Height = g_Gop->Mode->Info->VerticalResolution;

    // QEMU stdvga exposes PixelBlueGreenRedReserved8BitPerColor; swap the
    // byte order otherwise the red/blue channels would appear swapped.
    UINT32 Color;
    if (g_PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        Color = (R << 24) | (G << 16) | (B << 8);
    } else {
        Color = (B << 24) | (G << 16) | (R << 8);
    }

    UINTN Y;
    for (Y = 0; Y < Height; Y++) {
        UINT32* Row = (UINT32*)((UINT8*)Pixel + Y * RowBytes);
        UINTN X;
        for (X = 0; X < Width; X++) {
            Row[X] = Color;
        }
    }
}

/**
  Locate the Graphics Output Protocol instance. On success g_Gop points at
  the protocol and the framebuffer is available through its mode information.
**/
STATIC
EFI_STATUS
GraphicsLocateGop (
    VOID
    )
{
    EFI_HANDLE* Handles = NULL;
    UINTN       Count   = 0;
    EFI_STATUS  Status  = BS->LocateHandleBuffer(
        ByProtocol,
        &GraphicsOutputProtocol,
        NULL,
        &Count,
        &Handles
    );

    if (EFI_ERROR(Status) || Count == 0 || Handles == NULL) {
        return EFI_NOT_FOUND;
    }

    Status = BS->HandleProtocol(Handles[0], &GraphicsOutputProtocol, (VOID**)&g_Gop);
    FreePool(Handles);
    return Status;
}

EFI_STATUS
PpcInitializeHardwareAbstraction (
    VOID
    )
{
    // Initialize the hardware abstraction context
    ZeroMem(&g_HardwareContext, sizeof(g_HardwareContext));
    
    g_HardwareContext.IsInitialized = TRUE;
    g_HardwareContext.GraphicsMode = PPC_GRAPHICS_MODE_DEFAULT;
    g_HardwareContext.AudioEnabled = 0;
    g_HardwareContext.StorageDevices = 0;
    g_HardwareContext.NetworkInterfaces = 0;
    g_HardwareContext.SerialPorts = 0;
    g_HardwareContext.IOPorts = 0;
    g_HardwareContext.VideoBuffer = NULL;
    g_HardwareContext.VideoBufferSize = 0;
    g_HardwareContext.GraphicsInitialized = FALSE;
    g_HardwareContext.AudioInitialized = FALSE;
    g_HardwareContext.StorageInitialized = FALSE;
    g_HardwareContext.NetworkInitialized = FALSE;

    // Find the real GOP so graphics initialization can use it
    GraphicsLocateGop();
    
    Print(L"PowerPC Hardware Abstraction Layer initialized\n");
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcInitializeGraphics (
    IN UINT32 Width,
    IN UINT32 Height,
    IN UINT32 ColorDepth
    )
{
    if (Width == 0 || Height == 0) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Initializing graphics: %dx%d @ %d bits\n", Width, Height, ColorDepth);

    // Prefer the real UEFI Graphics Output Protocol framebuffer.
    if (g_Gop != NULL) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info = g_Gop->Mode->Info;

        // Try to honor the requested resolution: scan the supported modes and
        // switch to the first one that matches (fall back to the current mode).
        UINT32 Mode;
        BOOLEAN Found = FALSE;
        for (Mode = 0; Mode < g_Gop->Mode->MaxMode; Mode++) {
            UINTN  InfoSize = 0;
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Candidate = NULL;
            EFI_STATUS QueryStatus = g_Gop->QueryMode(g_Gop, Mode, &InfoSize, &Candidate);
            if (EFI_ERROR(QueryStatus) || Candidate == NULL) {
                continue;
            }
            if (Candidate->HorizontalResolution == Width &&
                Candidate->VerticalResolution == Height) {
                EFI_STATUS SetStatus = g_Gop->SetMode(g_Gop, Mode);
                if (!EFI_ERROR(SetStatus)) {
                    Found = TRUE;
                }
                FreePool(Candidate);
                break;
            }
            FreePool(Candidate);
        }
        if (!Found) {
            g_Gop->SetMode(g_Gop, g_Gop->Mode->Mode);
        }

        Info = g_Gop->Mode->Info;
        g_HardwareContext.VideoBuffer = (VOID*)(UINTN)g_Gop->Mode->FrameBufferBase;
        g_HardwareContext.VideoBufferSize = g_Gop->Mode->FrameBufferSize;
        g_FramebufferPitch = Info->PixelsPerScanLine * 4;
        g_PixelFormat = Info->PixelFormat;
        g_HardwareContext.GraphicsMode = (Info->HorizontalResolution << 16) |
                                         (Info->VerticalResolution & 0xFFFF);
        g_HardwareContext.GraphicsInitialized = TRUE;

        // Make the wiring visible: draw a pattern on the real framebuffer.
        GraphicsFillFramebuffer(0x00, 0x40, 0xFF);

        Print(L"Graphics: GOP mode %d, %dx%d, pixel format %d, framebuffer 0x%x "
              L"(pitch %d, size %d bytes)\n",
              g_Gop->Mode->Mode,
              Info->HorizontalResolution,
              Info->VerticalResolution,
              g_PixelFormat,
              g_Gop->Mode->FrameBufferBase,
              g_FramebufferPitch,
              g_HardwareContext.VideoBufferSize);

        return EFI_SUCCESS;
    }

    // No GOP available: fall back to a pool buffer (simulated framebuffer).
    g_HardwareContext.GraphicsMode = (Width << 16) | (Height & 0xFFFF);
    g_HardwareContext.GraphicsInitialized = TRUE;

    UINT64 BufferSize = Width * Height * (ColorDepth / 8);
    EFI_STATUS Status = BS->AllocatePool(EfiBootServicesData, BufferSize, &g_HardwareContext.VideoBuffer);
    
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate video buffer: %r\n", Status);
        return Status;
    }
    
    g_HardwareContext.VideoBufferSize = BufferSize;
    ZeroMem(g_HardwareContext.VideoBuffer, BufferSize);
    
    Print(L"Graphics initialized (simulated) - buffer at 0x%x (size: %d bytes)\n", 
          g_HardwareContext.VideoBuffer, BufferSize);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcInitializeAudio (
    VOID
    )
{
    Print(L"Initializing audio subsystem\n");
    
    // In a real implementation:
    // 1. Set up audio drivers
    // 2. Configure audio hardware
    // 3. Allocate audio buffers
    // 4. Initialize audio interface
    
    g_HardwareContext.AudioEnabled = 1;
    g_HardwareContext.AudioInitialized = TRUE;
    
    Print(L"Audio subsystem initialized\n");
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcInitializeStorage (
    IN UINT32 DeviceCount
    )
{
    if (DeviceCount == 0) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Initializing storage devices (locating Simple File System volumes)\n");

    // Locate every UEFI Simple File System protocol instance (one per
    // mounted partition/volume). This is real UEFI enumeration.
    EFI_HANDLE* Handles = NULL;
    UINTN       Count   = 0;
    EFI_STATUS  Status  = BS->LocateHandleBuffer(
        ByProtocol,
        &FileSystemProtocol,
        NULL,
        &Count,
        &Handles
    );

    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            Print(L"No Simple File System volumes found\n");
            return EFI_NOT_FOUND;
        }
        Print(L"Failed to locate file systems: %r\n", Status);
        return Status;
    }

    if (g_FileSystems != NULL) {
        FreePool(g_FileSystems);
        g_FileSystems = NULL;
    }
    g_FileSystemCount = Count;
    g_FileSystems = AllocatePool(Count * sizeof(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*));
    if (g_FileSystems == NULL) {
        FreePool(Handles);
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN Found = 0;
    for (UINTN i = 0; i < Count; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs = NULL;
        Status = BS->HandleProtocol(Handles[i], &FileSystemProtocol, (VOID**)&Fs);
        if (EFI_ERROR(Status) || Fs == NULL) {
            continue;
        }

        // Open the volume root to prove the file system is actually usable.
        EFI_FILE_HANDLE Root = NULL;
        Status = Fs->OpenVolume(Fs, &Root);
        if (EFI_ERROR(Status)) {
            continue;
        }
        Root->Close(Root);

        g_FileSystems[Found++] = Fs;
        Print(L"  Found volume %d: filesystem handle 0x%x\n", (UINTN)i, Fs);
    }

    FreePool(Handles);

    if (Found == 0) {
        Print(L"Storage subsystem initialized with 0 usable volumes\n");
        g_HardwareContext.StorageDevices = 0;
        g_HardwareContext.StorageInitialized = FALSE;
        return EFI_NOT_FOUND;
    }

    g_HardwareContext.StorageDevices = (UINT32)Found;
    g_HardwareContext.StorageInitialized = TRUE;

    Print(L"Storage subsystem initialized with %d usable volumes\n", Found);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcInitializeNetwork (
    IN UINT32 InterfaceCount
    )
{
    if (InterfaceCount == 0) {
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"Initializing %d network interfaces\n", InterfaceCount);
    
    // In a real implementation:
    // 1. Set up network drivers
    // 2. Configure network interfaces
    // 3. Initialize protocol stacks
    // 4. Set up network communication
    
    g_HardwareContext.NetworkInterfaces = InterfaceCount;
    g_HardwareContext.NetworkInitialized = TRUE;
    
    Print(L"Network subsystem initialized with %d interfaces\n", InterfaceCount);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcInitializeSerial (
    IN UINT32 PortCount
    )
{
    if (PortCount == 0) {
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"Initializing %d serial ports\n", PortCount);
    
    // In a real implementation:
    // 1. Configure serial port hardware
    // 2. Initialize UART controllers
    // 3. Set up communication protocols
    
    g_HardwareContext.SerialPorts = PortCount;
    
    Print(L"Serial subsystem initialized with %d ports\n", PortCount);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetHardwareInfo (
    OUT PPC_HARDWARE_INFO* HardwareInfo
    )
{
    if (HardwareInfo == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Fill hardware information structure
    ZeroMem(HardwareInfo, sizeof(PPC_HARDWARE_INFO));
    
    HardwareInfo->IsInitialized = g_HardwareContext.IsInitialized;
    HardwareInfo->GraphicsMode = g_HardwareContext.GraphicsMode;
    HardwareInfo->AudioEnabled = g_HardwareContext.AudioEnabled;
    HardwareInfo->StorageDevices = g_HardwareContext.StorageDevices;
    HardwareInfo->NetworkInterfaces = g_HardwareContext.NetworkInterfaces;
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcHandleHardwareInterrupt (
    IN UINT32 InterruptNumber
    )
{
    Print(L"Handling hardware interrupt %d\n", InterruptNumber);
    
    // In a real implementation:
    // 1. Route interrupt to appropriate handler
    // 2. Process interrupt request
    // 3. Update interrupt status
    // 4. Call registered interrupt handlers
    
    switch (InterruptNumber) {
        case 0x20:  // Timer interrupt
            Print(L"Timer interrupt handled\n");
            break;
            
        case 0x21:  // Keyboard interrupt
            Print(L"Keyboard interrupt handled\n");
            break;
            
        case 0x22:  // Disk interrupt
            Print(L"Disk interrupt handled\n");
            break;
            
        default:
            Print(L"Unhandled hardware interrupt %d\n", InterruptNumber);
            break;
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcSetHardwareState (
    IN PPC_HARDWARE_STATE State,
    IN UINT32             Value
    )
{
    switch (State) {
        case PPC_HARDWARE_GRAPHICS_MODE:
            g_HardwareContext.GraphicsMode = Value;
            Print(L"Graphics mode set to 0x%x\n", Value);
            break;
            
        case PPC_HARDWARE_AUDIO_ENABLE:
            g_HardwareContext.AudioEnabled = Value;
            Print(L"Audio enabled: %d\n", Value);
            break;
            
        default:
            return EFI_UNSUPPORTED;
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetHardwareState (
    IN  PPC_HARDWARE_STATE State,
    OUT UINT32*            Value
    )
{
    if (Value == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    switch (State) {
        case PPC_HARDWARE_GRAPHICS_MODE:
            *Value = g_HardwareContext.GraphicsMode;
            break;
            
        case PPC_HARDWARE_AUDIO_ENABLE:
            *Value = g_HardwareContext.AudioEnabled;
            break;
            
        default:
            return EFI_UNSUPPORTED;
    }
    
    return EFI_SUCCESS;
}

// Advanced hardware functions for PowerPC-specific needs

EFI_STATUS
PpcReadHardwareRegister (
    IN  UINT32 Address,
    OUT UINT32* Value
    )
{
    if (Value == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Simulate reading a hardware register
    *Value = 0x00000000;  // Default value
    
    Print(L"Reading hardware register at 0x%x\n", Address);
    
    // In real implementation, this would read from actual hardware
    // or from emulated hardware registers
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcWriteHardwareRegister (
    IN UINT32 Address,
    IN UINT32 Value
    )
{
    Print(L"Writing 0x%x to hardware register at 0x%x\n", Value, Address);
    
    // In real implementation, this would write to actual hardware
    // or to emulated hardware registers
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetVideoBuffer (
    OUT VOID** Buffer,
    OUT UINT64* Size
    )
{
    if (Buffer == NULL || Size == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    *Buffer = g_HardwareContext.VideoBuffer;
    *Size = g_HardwareContext.VideoBufferSize;
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcClearVideoBuffer (
    VOID
    )
{
    if (g_HardwareContext.VideoBuffer != NULL && g_HardwareContext.VideoBufferSize > 0) {
        ZeroMem(g_HardwareContext.VideoBuffer, g_HardwareContext.VideoBufferSize);
        Print(L"Video buffer cleared\n");
        return EFI_SUCCESS;
    }
    
    return EFI_NOT_READY;
}

EFI_STATUS
PpcUpdateVideoDisplay (
    IN UINT32 X,
    IN UINT32 Y,
    IN UINT32 Width,
    IN UINT32 Height
    )
{
    Print(L"Updating video display region: (%d,%d) %dx%d\n", X, Y, Width, Height);
    
    // In real implementation:
    // 1. Update the display with content from video buffer
    // 2. Handle hardware-specific display updates
    // 3. Synchronize with refresh rate
    
    return EFI_SUCCESS;
}
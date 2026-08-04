#include "abstraction.h"
#include <efi.h>
#include <efilib.h>
#include <efinet.h>
#include "memory/manager.h"
#include "cpu/translation.h"

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
    // Guest-visible framebuffer (window carved out of guest RAM)
    UINT32 GuestFbBase;
    VOID*  GuestFbHost;
    UINT64 GuestFbSize;
    UINT32 GuestFbWidth;
    UINT32 GuestFbHeight;
    UINT32 GuestFbPitch;
} PPC_HARDWARE_CONTEXT;

// Global hardware context
STATIC PPC_HARDWARE_CONTEXT g_HardwareContext = {0};

// Real UEFI protocol instances found by the HAL
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL  *g_Gop              = NULL;
STATIC UINT32                         g_FramebufferPitch = 0;
STATIC EFI_GRAPHICS_PIXEL_FORMAT      g_PixelFormat      = PixelRedGreenBlueReserved8BitPerColor;
STATIC EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **g_FileSystems    = NULL;
STATIC UINTN                          g_FileSystemCount  = 0;

// Real network interface state
STATIC struct {
    EFI_SIMPLE_NETWORK_PROTOCOL *Snp;
    BOOLEAN                      Inited;
} g_SnpIfaces[PPC_MAX_NETWORK_INTERFACES];
STATIC UINTN                          g_SnpIfaceCount   = 0;
STATIC EFI_SIMPLE_NETWORK_PROTOCOL   *g_Snp             = NULL;
STATIC PPC_NETWORK_INFO               g_NetworkInfo     = {0};
STATIC BOOLEAN                        g_NetworkInited   = FALSE;

// Real block device state
STATIC EFI_BLOCK_IO_PROTOCOL         **g_BlockDevices    = NULL;
STATIC UINTN                          g_BlockDeviceCount = 0;
STATIC PPC_BLOCK_IO_INFO              g_BlockIoInfo      = {0};

// Emulated audio device state
STATIC PPC_AUDIO_INFO                 g_AudioInfo        = {0};
STATIC BOOLEAN                        g_AudioInited      = FALSE;

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

        // Carve a guest-visible framebuffer window out of guest RAM. Guest
        // code writes big-endian 0xRRGGBB00 pixels here; PpcGraphicsBlitToDisplay
        // pushes the window to the real GOP framebuffer.
        VOID*  GuestHost   = NULL;
        UINT64 GuestBase   = 0;
        UINT64 GuestSize   = 0;
        EFI_STATUS FbStatus = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);
        if (!EFI_ERROR(FbStatus) && GuestHost != NULL) {
            UINT32 FbSize = Info->HorizontalResolution * Info->VerticalResolution * 4;
            if ((UINT64)(PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE - (UINT32)GuestBase) + FbSize <= GuestSize) {
                g_HardwareContext.GuestFbBase  = PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE;
                g_HardwareContext.GuestFbHost  = (UINT8*)GuestHost +
                                                 (PPC_GRAPHICS_FRAMEBUFFER_GUEST_BASE - (UINT32)GuestBase);
                g_HardwareContext.GuestFbSize  = FbSize;
                g_HardwareContext.GuestFbWidth = Info->HorizontalResolution;
                g_HardwareContext.GuestFbHeight = Info->VerticalResolution;
                g_HardwareContext.GuestFbPitch = Info->HorizontalResolution * 4;
                ZeroMem(g_HardwareContext.GuestFbHost, FbSize);
                Print(L"Graphics: guest framebuffer at guest 0x%x (host 0x%x, %d bytes)\n",
                      g_HardwareContext.GuestFbBase,
                      g_HardwareContext.GuestFbHost,
                      FbSize);
            }
        }

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
    Print(L"Initializing audio subsystem (emulated PCM ring buffer in guest RAM)\n");

    // UEFI has no standard audio output protocol, so the "device" is a fixed
    // ring buffer inside guest RAM that guest code fills with big-endian
    // 16-bit stereo PCM samples.
    VOID*  GuestHost   = NULL;
    UINT64 GuestBase   = 0;
    UINT64 GuestSize   = 0;
    EFI_STATUS Status = PpcGetGuestMemoryRegion(&GuestHost, &GuestBase, &GuestSize);
    if (EFI_ERROR(Status) || GuestHost == NULL) {
        Print(L"Audio subsystem: guest RAM unavailable, falling back to pool buffer\n");
        Status = BS->AllocatePool(EfiBootServicesData, PPC_AUDIO_BUFFER_SIZE,
                                  (VOID**)&g_AudioInfo.HostBuffer);
        if (EFI_ERROR(Status)) {
            return Status;
        }
        g_AudioInfo.GuestBase = 0;
    } else {
        if ((UINT64)(PPC_AUDIO_BUFFER_GUEST_BASE - (UINT32)GuestBase) + PPC_AUDIO_BUFFER_SIZE > GuestSize) {
            Print(L"Audio subsystem: guest RAM too small\n");
            return EFI_NOT_READY;
        }
        g_AudioInfo.GuestBase = PPC_AUDIO_BUFFER_GUEST_BASE;
        g_AudioInfo.HostBuffer = (UINT8*)GuestHost +
                                 (PPC_AUDIO_BUFFER_GUEST_BASE - (UINT32)GuestBase);
    }

    g_AudioInfo.Size = PPC_AUDIO_BUFFER_SIZE;
    g_AudioInfo.SampleRate = PPC_AUDIO_SAMPLE_RATE;
    g_AudioInfo.Channels = PPC_AUDIO_CHANNELS;
    g_AudioInfo.BitsPerSample = PPC_AUDIO_BITS_PER_SAMPLE;
    g_AudioInfo.WriteCursor = 0;
    g_AudioInfo.PlayCursor = 0;
    ZeroMem(g_AudioInfo.HostBuffer, g_AudioInfo.Size);

    g_HardwareContext.AudioEnabled = 1;
    g_HardwareContext.AudioInitialized = TRUE;
    g_AudioInited = TRUE;

    Print(L"Audio subsystem initialized: guest 0x%x (host 0x%x, %d bytes, %d Hz, %d ch)\n",
          g_AudioInfo.GuestBase, g_AudioInfo.HostBuffer,
          g_AudioInfo.Size, g_AudioInfo.SampleRate, g_AudioInfo.Channels);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcAudioGetBufferInfo (
    OUT PPC_AUDIO_INFO* Info
    )
{
    if (Info == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (!g_AudioInited) {
        return EFI_NOT_READY;
    }
    CopyMem(Info, &g_AudioInfo, sizeof(PPC_AUDIO_INFO));
    return EFI_SUCCESS;
}

EFI_STATUS
PpcAudioWriteSample (
    IN UINT32 SampleIndex,
    IN UINT16 Value
    )
{
    if (!g_AudioInited || g_AudioInfo.HostBuffer == NULL) {
        return EFI_NOT_READY;
    }
    if ((UINT64)SampleIndex * 2 + 2 > g_AudioInfo.Size) {
        return EFI_OUT_OF_RESOURCES;
    }
    UINT16 Be = (UINT16)(((Value & 0xFF) << 8) | (Value >> 8));
    CopyMem((UINT8*)g_AudioInfo.HostBuffer + (UINT64)SampleIndex * 2, &Be, 2);
    if (g_AudioInfo.WriteCursor < (UINT64)SampleIndex + 1) {
        g_AudioInfo.WriteCursor = (UINT64)SampleIndex + 1;
    }
    return EFI_SUCCESS;
}

EFI_STATUS
PpcAudioReadSample (
    IN  UINT32  SampleIndex,
    OUT UINT16* Value
    )
{
    if (!g_AudioInited || g_AudioInfo.HostBuffer == NULL || Value == NULL) {
        return EFI_NOT_READY;
    }
    if ((UINT64)SampleIndex * 2 + 2 > g_AudioInfo.Size) {
        return EFI_OUT_OF_RESOURCES;
    }
    UINT16 Be;
    CopyMem(&Be, (UINT8*)g_AudioInfo.HostBuffer + (UINT64)SampleIndex * 2, 2);
    *Value = (UINT16)(((Be & 0xFF) << 8) | (Be >> 8));
    return EFI_SUCCESS;
}

EFI_STATUS
PpcAudioAdvancePlayback (
    IN UINT32 SampleCount
    )
{
    if (!g_AudioInited) {
        return EFI_NOT_READY;
    }
    if (g_AudioInfo.PlayCursor + SampleCount > g_AudioInfo.Size / 2) {
        return EFI_OUT_OF_RESOURCES;
    }
    g_AudioInfo.PlayCursor += SampleCount;
    return EFI_SUCCESS;
}

EFI_STATUS
PpcAudioReset (
    VOID
    )
{
    if (!g_AudioInited) {
        return EFI_NOT_READY;
    }
    if (g_AudioInfo.HostBuffer != NULL) {
        ZeroMem(g_AudioInfo.HostBuffer, g_AudioInfo.Size);
    }
    g_AudioInfo.WriteCursor = 0;
    g_AudioInfo.PlayCursor = 0;
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

    Print(L"Initializing network interfaces (locating Simple Network Protocol)\n");

    // Enumerate every real UEFI Simple Network Protocol instance.
    EFI_HANDLE* Handles = NULL;
    UINTN       Count   = 0;
    EFI_STATUS  Status  = BS->LocateHandleBuffer(
        ByProtocol,
        &SimpleNetworkProtocol,
        NULL,
        &Count,
        &Handles
    );

    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            Print(L"No network interfaces found\n");
            return EFI_NOT_FOUND;
        }
        Print(L"Failed to locate network interfaces: %r\n", Status);
        return Status;
    }

    ZeroMem(&g_NetworkInfo, sizeof(g_NetworkInfo));
    ZeroMem(g_SnpIfaces, sizeof(g_SnpIfaces));
    g_SnpIfaceCount = 0;

    for (UINTN i = 0; i < Count; i++) {
        if (g_SnpIfaceCount >= PPC_MAX_NETWORK_INTERFACES) {
            break;
        }

        EFI_SIMPLE_NETWORK_PROTOCOL* Snp = NULL;
        Status = BS->HandleProtocol(Handles[i], &SimpleNetworkProtocol, (VOID**)&Snp);
        if (EFI_ERROR(Status) || Snp == NULL) {
            continue;
        }

        // Bring the adapter up with real SNP Start/Initialize calls.
        Status = Snp->Start(Snp);
        if (EFI_ERROR(Status)) {
            Print(L"  Interface %d: Start failed %r\n", i, Status);
            continue;
        }
        Status = Snp->Initialize(Snp, 0, 0);
        if (EFI_ERROR(Status)) {
            Print(L"  Interface %d: Initialize failed %r\n", i, Status);
            continue;
        }

        // Restore the standard receive filter so ordinary traffic is visible.
        if (Snp->Mode->ReceiveFilterMask != 0) {
            Snp->ReceiveFilters(Snp,
                                EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
                                EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST |
                                EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST,
                                0, FALSE, 0, NULL);
        }

        // Record the initialized interface and snapshot its real mode info.
        UINTN Slot = g_SnpIfaceCount;
        g_SnpIfaces[Slot].Snp = Snp;
        g_SnpIfaces[Slot].Inited = TRUE;
        g_SnpIfaceCount++;

        EFI_SIMPLE_NETWORK_MODE* Mode = Snp->Mode;
        PPC_NETWORK_IFACE_INFO* Iface = &g_NetworkInfo.Interfaces[Slot];
        CopyMem(Iface->MacAddress, Mode->CurrentAddress.Addr,
                sizeof(Iface->MacAddress));
        Iface->MediaPresent = (Mode->MediaPresentSupported) ? Mode->MediaPresent : TRUE;

        Print(L"  Interface %d: MAC %02x:%02x:%02x:%02x:%02x:%02x, media %s, "
              L"iftype %d, maxpkt %d, hdrsize %d, hwaddr %d, state %d\n",
              (UINTN)i,
              Iface->MacAddress[0], Iface->MacAddress[1],
              Iface->MacAddress[2], Iface->MacAddress[3],
              Iface->MacAddress[4], Iface->MacAddress[5],
              Iface->MediaPresent ? L"present" : L"absent",
              Mode->IfType, Mode->MaxPacketSize,
              Mode->MediaHeaderSize, Mode->HwAddressSize, Mode->State);

        // Transmit a real test frame (64-byte ARP-style probe, broadcast).
        UINT8 Frame[64];
        ZeroMem(Frame, sizeof(Frame));
        for (UINTN b = 0; b < 6; b++) {
            Frame[b] = 0xFF;                       // broadcast dest
            Frame[6 + b] = Iface->MacAddress[b];   // our source
        }
        Frame[12] = 0x08;                          // ethertype
        Frame[13] = 0x06;                          // 0x0806 = ARP
        Frame[14] = 0x00;                          // ARP htype
        Frame[15] = 0x01;
        Frame[16] = 0x08;                          // ARP ptype = IP
        Frame[17] = 0x00;
        Frame[18] = 0x06;                          // hlen, plen
        Frame[19] = 0x04;
        Frame[20] = 0x00;                          // op = request
        Frame[21] = 0x01;

        // The frame already contains a complete MAC header (dest/src/ethertype),
        // so call Transmit with HeaderSize == 0 (the driver takes the header
        // straight from the buffer).
        Status = Snp->Transmit(Snp, 0, sizeof(Frame), Frame, NULL, NULL, NULL);
        Iface->TransmitTestPassed = (Status == EFI_SUCCESS);
        if (Status != EFI_SUCCESS) {
            Print(L"  Interface %d: transmit returned %r\n", (UINTN)i, Status);
        }

        // Poll GetStatus briefly to observe the transmit completion.
        for (UINTN Poll = 0; Poll < 3; Poll++) {
            UINT32 IrqStatus = 0;
            VOID*   TxBuf    = NULL;
            EFI_STATUS GetStatus = Snp->GetStatus(Snp, &IrqStatus, &TxBuf);
            if (GetStatus == EFI_SUCCESS) {
                break;
            }
        }

        Print(L"  Interface %d: transmit test %s\n",
              (UINTN)i,
              Iface->TransmitTestPassed ? L"PASS" : L"FAIL");
    }

    FreePool(Handles);

    if (g_SnpIfaceCount == 0) {
        g_HardwareContext.NetworkInterfaces = 0;
        g_HardwareContext.NetworkInitialized = FALSE;
        return EFI_NOT_FOUND;
    }

    // Primary interface = first initialized interface (compat snapshot).
    g_Snp = g_SnpIfaces[0].Snp;
    g_NetworkInfo.InterfaceCount = g_SnpIfaceCount;
    g_NetworkInfo.MediaPresent = g_NetworkInfo.Interfaces[0].MediaPresent;
    g_NetworkInfo.TransmitTestPassed = g_NetworkInfo.Interfaces[0].TransmitTestPassed;
    CopyMem(g_NetworkInfo.MacAddress, g_NetworkInfo.Interfaces[0].MacAddress,
            sizeof(g_NetworkInfo.MacAddress));
    EFI_SIMPLE_NETWORK_MODE* Mode = g_Snp->Mode;
    g_NetworkInfo.MaxPacketSize = Mode->MaxPacketSize;
    g_NetworkInfo.IfType = Mode->IfType;
    g_NetworkInfo.HwAddressSize = Mode->HwAddressSize;
    g_NetworkInfo.MediaHeaderSize = Mode->MediaHeaderSize;
    g_NetworkInfo.State = Mode->State;

    g_HardwareContext.NetworkInterfaces = (UINT32)g_SnpIfaceCount;
    g_HardwareContext.NetworkInitialized = TRUE;
    g_NetworkInited = TRUE;

    Print(L"Network subsystem initialized with %d interface(s)\n",
          g_SnpIfaceCount);

    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetNetworkInfo (
    OUT PPC_NETWORK_INFO* Info
    )
{
    if (Info == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (!g_NetworkInited || g_Snp == NULL) {
        return EFI_NOT_READY;
    }
    CopyMem(Info, &g_NetworkInfo, sizeof(PPC_NETWORK_INFO));
    return EFI_SUCCESS;
}

EFI_STATUS
PpcInitializeBlockIo (
    IN UINT32 DeviceCount
    )
{
    if (DeviceCount == 0) {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Initializing block devices (locating Block I/O protocols)\n");

    EFI_HANDLE* Handles = NULL;
    UINTN       Count   = 0;
    EFI_STATUS  Status  = BS->LocateHandleBuffer(
        ByProtocol,
        &BlockIoProtocol,
        NULL,
        &Count,
        &Handles
    );

    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            Print(L"No block devices found\n");
            return EFI_NOT_FOUND;
        }
        Print(L"Failed to locate block devices: %r\n", Status);
        return Status;
    }

    if (g_BlockDevices != NULL) {
        FreePool(g_BlockDevices);
        g_BlockDevices = NULL;
    }
    g_BlockDeviceCount = 0;
    ZeroMem(&g_BlockIoInfo, sizeof(g_BlockIoInfo));

    g_BlockDevices = AllocatePool(Count * sizeof(EFI_BLOCK_IO_PROTOCOL*));
    if (g_BlockDevices == NULL) {
        FreePool(Handles);
        return EFI_OUT_OF_RESOURCES;
    }

    for (UINTN i = 0; i < Count; i++) {
        EFI_BLOCK_IO_PROTOCOL* Bio = NULL;
        Status = BS->HandleProtocol(Handles[i], &BlockIoProtocol, (VOID**)&Bio);
        if (EFI_ERROR(Status) || Bio == NULL || Bio->Media == NULL) {
            continue;
        }
        if (!Bio->Media->MediaPresent) {
            continue;
        }
        g_BlockDevices[g_BlockDeviceCount++] = Bio;
    }

    FreePool(Handles);

    if (g_BlockDeviceCount == 0) {
        Print(L"Block I/O subsystem initialized with 0 usable devices\n");
        g_HardwareContext.StorageDevices = 0;
        return EFI_NOT_FOUND;
    }

    g_BlockIoInfo.DeviceCount = g_BlockDeviceCount;

    // Default to the first usable device for info reporting.
    g_BlockIoInfo.SelectedIndex = 0;
    g_BlockIoInfo.MediaId = g_BlockDevices[0]->Media->MediaId;
    g_BlockIoInfo.BlockSize = g_BlockDevices[0]->Media->BlockSize;
    g_BlockIoInfo.LastBlock = g_BlockDevices[0]->Media->LastBlock;
    g_BlockIoInfo.ReadOnly = g_BlockDevices[0]->Media->ReadOnly;
    g_BlockIoInfo.Removable = g_BlockDevices[0]->Media->RemovableMedia;

    Print(L"Block I/O subsystem initialized with %d usable device(s)\n",
          g_BlockDeviceCount);

    for (UINTN i = 0; i < g_BlockDeviceCount; i++) {
        EFI_BLOCK_IO_MEDIA* M = g_BlockDevices[i]->Media;
        Print(L"  Device %d: %d bytes/block, %d blocks, %s%s\n",
              (UINTN)i, M->BlockSize, (UINT64)M->LastBlock + 1,
              M->RemovableMedia ? L"removable, " : L"",
              M->ReadOnly ? L"read-only" : L"writable");
    }

    g_HardwareContext.StorageDevices = (UINT32)g_BlockDeviceCount;
    g_HardwareContext.StorageInitialized = TRUE;

    return EFI_SUCCESS;
}

EFI_STATUS
PpcGetBlockIoInfo (
    OUT PPC_BLOCK_IO_INFO* Info
    )
{
    if (Info == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (g_BlockDeviceCount == 0 || g_BlockDevices == NULL) {
        return EFI_NOT_READY;
    }
    CopyMem(Info, &g_BlockIoInfo, sizeof(PPC_BLOCK_IO_INFO));
    return EFI_SUCCESS;
}

EFI_STATUS
PpcReadDiskBlock (
    IN  UINTN   Index,
    IN  EFI_LBA Lba,
    IN  UINTN   BufferSize,
    OUT VOID*   Buffer
    )
{
    if (Buffer == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (Index >= g_BlockDeviceCount || g_BlockDevices == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    EFI_BLOCK_IO_PROTOCOL* Bio = g_BlockDevices[Index];
    UINT32 MediaId = Bio->Media->MediaId;

    return Bio->ReadBlocks(Bio, MediaId, Lba, BufferSize, Buffer);
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

EFI_STATUS
PpcGetFrameBufferInfo (
    OUT PPC_FRAMEBUFFER_INFO* Info
    )
{
    if (Info == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    if (!g_HardwareContext.GraphicsInitialized || g_Gop == NULL) {
        return EFI_NOT_READY;
    }

    Info->Width           = g_HardwareContext.GuestFbWidth;
    Info->Height          = g_HardwareContext.GuestFbHeight;
    Info->Pitch           = g_HardwareContext.GuestFbPitch;
    Info->BitsPerPixel    = 32;
    Info->GuestBase       = g_HardwareContext.GuestFbBase;
    Info->HostBuffer      = g_HardwareContext.GuestFbHost;
    Info->HostBufferSize  = g_HardwareContext.GuestFbSize;
    Info->GopFrameBuffer  = g_Gop->Mode->FrameBufferBase;
    Info->GopPitch        = g_FramebufferPitch;
    Info->PixelFormat     = g_PixelFormat;
    return EFI_SUCCESS;
}

/**
  Read a pixel from the guest framebuffer (big-endian 0xRRGGBB00).
  Unmapped or out-of-bounds reads return zero.
**/
STATIC
UINT32
FrameBufferReadPixel (
    IN UINT32 X,
    IN UINT32 Y
    )
{
    if (g_HardwareContext.GuestFbHost == NULL ||
        X >= g_HardwareContext.GuestFbWidth ||
        Y >= g_HardwareContext.GuestFbHeight) {
        return 0;
    }

    UINT8* Pixel = (UINT8*)g_HardwareContext.GuestFbHost +
                   (UINT64)Y * g_HardwareContext.GuestFbPitch +
                   (UINT64)X * 4;
    return ((UINT32)Pixel[0] << 24) |
           ((UINT32)Pixel[1] << 16) |
           ((UINT32)Pixel[2] << 8)  |
           ((UINT32)Pixel[3]);
}

/**
  Write a pixel to the guest framebuffer in big-endian 0xRRGGBB00 layout.
  Out-of-bounds writes are dropped.
**/
STATIC
VOID
FrameBufferWritePixel (
    IN UINT32 X,
    IN UINT32 Y,
    IN UINT32 Color
    )
{
    if (g_HardwareContext.GuestFbHost == NULL ||
        X >= g_HardwareContext.GuestFbWidth ||
        Y >= g_HardwareContext.GuestFbHeight) {
        return;
    }

    UINT8* Pixel = (UINT8*)g_HardwareContext.GuestFbHost +
                   (UINT64)Y * g_HardwareContext.GuestFbPitch +
                   (UINT64)X * 4;
    Pixel[0] = (UINT8)(Color >> 24);
    Pixel[1] = (UINT8)(Color >> 16);
    Pixel[2] = (UINT8)(Color >> 8);
    Pixel[3] = (UINT8)Color;
}

EFI_STATUS
PpcGraphicsBlitToDisplay (
    VOID
    )
{
    if (!g_HardwareContext.GraphicsInitialized ||
        g_Gop == NULL ||
        g_HardwareContext.GuestFbHost == NULL) {
        return EFI_NOT_READY;
    }

    UINT32* Dest = (UINT32*)(UINTN)g_Gop->Mode->FrameBufferBase;
    UINTN   DestPitch = g_FramebufferPitch / 4;
    UINTN   W = g_HardwareContext.GuestFbWidth;
    UINTN   H = g_HardwareContext.GuestFbHeight;

    BOOLEAN Bgr = (g_PixelFormat == PixelBlueGreenRedReserved8BitPerColor);

    UINTN Y;
    for (Y = 0; Y < H; Y++) {
        UINT32* Row = Dest + Y * DestPitch;
        UINTN X;
        for (X = 0; X < W; X++) {
            UINT32 GuestColor = FrameBufferReadPixel((UINT32)X, (UINT32)Y);
            UINT8 R = (UINT8)(GuestColor >> 24);
            UINT8 G = (UINT8)(GuestColor >> 16);
            UINT8 B = (UINT8)(GuestColor >> 8);
            UINT32 HostColor;
            if (Bgr) {
                // BGR layout: byte0=Blue, byte1=Green, byte2=Red, byte3=reserved.
                HostColor = ((UINT32)B) | ((UINT32)G << 8) | ((UINT32)R << 16);
            } else {
                // RGB layout: byte0=Red, byte1=Green, byte2=Blue, byte3=reserved.
                HostColor = ((UINT32)R) | ((UINT32)G << 8) | ((UINT32)B << 16);
            }
            Row[X] = HostColor;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS
PpcGraphicsClear (
    IN UINT32 Color
    )
{
    if (!g_HardwareContext.GraphicsInitialized || g_HardwareContext.GuestFbHost == NULL) {
        return EFI_NOT_READY;
    }

    UINTN X, Y;
    for (Y = 0; Y < g_HardwareContext.GuestFbHeight; Y++) {
        for (X = 0; X < g_HardwareContext.GuestFbWidth; X++) {
            FrameBufferWritePixel((UINT32)X, (UINT32)Y, Color);
        }
    }
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGraphicsSetPixel (
    IN UINT32 X,
    IN UINT32 Y,
    IN UINT32 Color
    )
{
    if (!g_HardwareContext.GraphicsInitialized || g_HardwareContext.GuestFbHost == NULL) {
        return EFI_NOT_READY;
    }
    FrameBufferWritePixel(X, Y, Color);
    return EFI_SUCCESS;
}

EFI_STATUS
PpcGraphicsDrawRect (
    IN UINT32 X,
    IN UINT32 Y,
    IN UINT32 Width,
    IN UINT32 Height,
    IN UINT32 Color
    )
{
    if (!g_HardwareContext.GraphicsInitialized || g_HardwareContext.GuestFbHost == NULL) {
        return EFI_NOT_READY;
    }

    UINT32 Rx, Ry;
    for (Ry = Y; Ry < Y + Height; Ry++) {
        for (Rx = X; Rx < X + Width; Rx++) {
            FrameBufferWritePixel(Rx, Ry, Color);
        }
    }
    return EFI_SUCCESS;
}
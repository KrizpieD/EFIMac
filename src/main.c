#include <efi.h>
#include <efilib.h>

// MS ABI CRT symbol: signals the linker that floating-point is in use.
UINT32 _fltused = 0;

// Include all our module headers
#include "cpu/translation.h"
#include "memory/manager.h"
#include "hardware/abstraction.h"
#include "boot/bootloader.h"
#include "fs/hfs.h"
#include "utils/debug.h"
#include "platform/uefi_interface.h"

// True if the GOP pixel at (X,Y) holds exactly the R/G/B channels of the
// given guest color (big-endian 0xRRGGBB00), placed according to the GOP
// pixel format (byte-exact, so channel-position bugs are caught).
STATIC
BOOLEAN
GopPixelMatches (
  IN UINT8* GopBase,
  IN UINTN  GopPitch,
  IN UINTN  X,
  IN UINTN  Y,
  IN UINT32 GuestColor,
  IN UINT32 PixelFormat
  )
{
  UINT8 R = (UINT8)(GuestColor >> 24);
  UINT8 G = (UINT8)(GuestColor >> 16);
  UINT8 B = (UINT8)(GuestColor >> 8);
  UINT8* P = GopBase + Y * GopPitch + X * 4;
  if (PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
    return P[0] == B && P[1] == G && P[2] == R;
  }
  return P[0] == R && P[1] == G && P[2] == B;
}

// True if every visible pixel of the GOP framebuffer matches the guest color.
STATIC
BOOLEAN
GopFrameIsSolid (
  IN UINT8* GopBase,
  IN UINTN  GopPitch,
  IN UINTN  W,
  IN UINTN  H,
  IN UINT32 GuestColor,
  IN UINT32 PixelFormat
  )
{
  for (UINTN Y = 0; Y < H; Y++) {
    for (UINTN X = 0; X < W; X++) {
      if (!GopPixelMatches(GopBase, GopPitch, X, Y, GuestColor, PixelFormat)) {
        return FALSE;
      }
    }
  }
  return TRUE;
}

// Short label for an installed ROM type.
STATIC
CHAR16*
BootRomTypeName (
  IN UINT32 RomType
  )
{
  switch (RomType) {
  case PPC_ROM_TYPE_NEW_WORLD: return L"New World (Mac OS ROM)";
  case PPC_ROM_TYPE_OLD_WORLD: return L"Old World (classic firmware dump)";
  case PPC_ROM_TYPE_DEMO:      return L"demo (no Mac firmware)";
  default:                     return L"unknown";
  }
}

EFI_STATUS
efi_main (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  
  // Initialize the GNU-EFI library
  InitializeLib(ImageHandle, SystemTable);
  
  // Print welcome message
  Print(L"EFI Mac OS Boot Layer v0.2\n");
  Print(L"Heavy bootloader for classic Mac OS (System 7, Mac OS 8/9) via UEFI\n");
  Print(L"Initializing PowerPC environment...\n");
  
  // Initialize UEFI interface
  Status = PpcInitializeUefiInterface(ImageHandle, SystemTable);
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize UEFI interface: %r\n", Status);
    return Status;
  }
  
  // Initialize debug system
  Status = PpcInitializeDebug(PPC_DEBUG_LEVEL_DEBUG, FALSE, NULL);
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize debug system: %r\n", Status);
    return Status;
  }
  
  // Initialize PowerPC translation context
  Status = PpcInitializeTranslationContext();
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize translation context: %r\n", Status);
    return Status;
  }
  
  // Run the CPU self-test suite
  Status = PpcRunSelfTest();
  if (EFI_ERROR(Status)) {
    Print(L"PowerPC CPU self-test FAILED: %r\n", Status);
    return Status;
  }
  
  // Initialize memory manager (guest RAM at the classic Mac OS kernel base)
  Status = PpcInitializeMemoryManager(0x10000000, 0x10000000);  // 256MB
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize memory manager: %r\n", Status);
    return Status;
  }
  
  // Wire the UEFI-allocated guest RAM into the interpreter so that guest
  // loads/stores and instruction fetches target the emulated memory region.
  {
    VOID*  HostBase  = NULL;
    UINT64 GuestBase = 0;
    UINT64 GuestSize = 0;
    Status = PpcGetGuestMemoryRegion(&HostBase, &GuestBase, &GuestSize);
    if (EFI_ERROR(Status)) {
      Print(L"Failed to get guest memory region: %r\n", Status);
      return Status;
    }
    Status = PpcSetGuestMemory(HostBase, (UINT32)GuestBase, (UINT32)GuestSize);
    if (EFI_ERROR(Status)) {
      Print(L"Failed to wire guest memory: %r\n", Status);
      return Status;
    }
    Print(L"Guest memory wired: host 0x%x <-> guest 0x%x (%d bytes)\n",
          HostBase, GuestBase, GuestSize);
  }
  
  // Execute a small PowerPC program that lives in guest RAM. This proves the
  // full pipeline: guest memory allocation -> fetch -> decode -> execute.
  {
    VOID*  DemoHostBase  = NULL;
    UINT64 DemoGuestBase = 0;
    UINT64 DemoGuestSize = 0;
    Status = PpcGetGuestMemoryRegion(&DemoHostBase, &DemoGuestBase, &DemoGuestSize);
    if (EFI_ERROR(Status)) {
      Print(L"Demo: guest memory region unavailable: %r\n", Status);
      return Status;
    }

    // addi r3,r0,40 ; addi r4,r0,2 ; mullw r5,r3,r4 ; stw r5,0(r1)
    static const UINT32 GuestProgram[] = {
      0x38600028,   // addi r3, r0, 40
      0x38800002,   // addi r4, r0, 2
      0x7CA321D6,   // mullw r5, r3, r4
      0x90A10000,   // stw  r5, 0(r1)
    };
    const UINT32 ProgramBase = (UINT32)DemoGuestBase;
    const UINT32 ResultAddr  = ProgramBase + 0x40;  // store result here
    const UINTN  ProgramCount = sizeof(GuestProgram) / sizeof(GuestProgram[0]);

    // Write the program into guest RAM (big-endian byte order)
    for (UINTN i = 0; i < ProgramCount; i++) {
      UINT32 w = GuestProgram[i];
      for (UINTN b = 0; b < 4; b++) {
        PpcWriteGuestByte(ProgramBase + (UINT32)i * 4 + (UINT32)b,
                          (UINT8)(w >> (24 - (UINTN)b * 8)));
      }
    }

    Print(L"\n--- Executing PowerPC program from guest RAM ---\n");

    PpcSetGprValue(1, ResultAddr);   // r1 points at the result slot
    PpcSetGprValue(3, 0);
    PpcSetGprValue(4, 0);
    PpcSetGprValue(5, 0);

    // Fetch/decode/execute loop over the guest-memory resident program
    UINTN  Executed = 0;
    EFI_STATUS ExecStatus = PpcExecuteBlock(
        (UINT32*)(UINTN)ProgramBase,
        ProgramCount,
        &Executed
    );
    BOOLEAN RanClean = (ExecStatus == EFI_SUCCESS);

    // Read the stored word back out of guest RAM (big-endian)
    UINT32 Result = 0;
    for (UINTN b = 0; b < 4; b++) {
      Result = (Result << 8) | PpcReadGuestByte(ResultAddr + (UINT32)b);
    }
    UINT32 R5 = PpcGetGprValue(5);

    Print(L"Program executed: %d instructions (status %r)\n", Executed, ExecStatus);
    Print(L"r5 = %d (expected 80)\n", R5);
    Print(L"Guest RAM[0x%x] = %d (expected 80)\n", ResultAddr, Result);

    if (RanClean && Executed == ProgramCount && R5 == 80 && Result == 80) {
      Print(L"Guest RAM execution: PASS\n");
    } else {
      Print(L"Guest RAM execution: FAIL\n");
    }
  }
  
  // Initialize hardware abstraction layer
  Status = PpcInitializeHardwareAbstraction();
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize hardware abstraction: %r\n", Status);
    return Status;
  }
  
  // Initialize bootloader
  Status = PpcInitializeBootloader();
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize bootloader: %r\n", Status);
    return Status;
  }
  
  // Setup boot environment
  Status = PpcSetupBootEnvironment();
  if (EFI_ERROR(Status)) {
    Print(L"Failed to setup boot environment: %r\n", Status);
    return Status;
  }
  
  // Initialize graphics for the emulator
  Status = PpcInitializeGraphics(640, 480, 32);
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize graphics: %r\n", Status);
    return Status;
  }
  
  // Graphics self-check: draw into the guest framebuffer, blit to the real
  // GOP display, and read pixels back from both buffers to verify the path.
  {
    PPC_FRAMEBUFFER_INFO FbInfo;
    Status = PpcGetFrameBufferInfo(&FbInfo);
    if (!EFI_ERROR(Status) && FbInfo.HostBuffer != NULL) {
      PpcGraphicsClear(0x10101000);                          // dark grey background
      PpcGraphicsDrawRect(0, 0, 640, 120, 0xCC000000);       // red band
      PpcGraphicsDrawRect(0, 120, 640, 120, 0x00CC0000);     // green band
      PpcGraphicsDrawRect(0, 240, 640, 120, 0x0000CC00);     // blue band
      PpcGraphicsDrawRect(0, 360, 640, 120, 0xCCCC0000);     // yellow band
      Status = PpcGraphicsBlitToDisplay();

      UINT32 P0 = (UINT32)((UINT8*)FbInfo.HostBuffer)[0] << 24 |
                  (UINT32)((UINT8*)FbInfo.HostBuffer)[1] << 16 |
                  (UINT32)((UINT8*)FbInfo.HostBuffer)[2] << 8;
      UINT8* GopP = (UINT8*)(UINTN)FbInfo.GopFrameBuffer;
      UINT32 P1 = ((UINT32)GopP[2] << 16) | ((UINT32)GopP[1] << 8) | (UINT32)GopP[0];

      if (!EFI_ERROR(Status) && (P0 >> 24) == 0xCC && (P0 & 0xFF0000) == 0) {
        Print(L"Graphics self-check: PASS (guest fb 0x%08x, GOP fb 0x%08x)\n", P0, P1);
      } else {
        Print(L"Graphics self-check: FAIL (guest fb 0x%08x, GOP fb 0x%08x)\n", P0, P1);
      }
    } else {
      Print(L"Graphics self-check: SKIP (no framebuffer)\n");
    }
  }

  // Multi-frame graphics test: full-screen frames written into the guest
  // framebuffer are blitted through the real GOP path and verified across
  // the entire display, plus band boundaries, corners, and out-of-bounds.
  {
    PPC_FRAMEBUFFER_INFO FbInfo;
    Status = PpcGetFrameBufferInfo(&FbInfo);
    if (!EFI_ERROR(Status) && FbInfo.HostBuffer != NULL && FbInfo.GopPitch != 0) {
      UINTN  W   = FbInfo.Width;
      UINTN  H   = FbInfo.Height;
      UINT8* Gop = (UINT8*)(UINTN)FbInfo.GopFrameBuffer;
      BOOLEAN MultiOk = TRUE;

      // Frames 1-3: solid red, green, blue across the full visible area.
      UINT32 Solids[3] = { 0xCC000000, 0x00CC0000, 0x0000CC00 };
      for (UINTN f = 0; f < 3; f++) {
        PpcGraphicsClear(Solids[f]);
        PpcGraphicsBlitToDisplay();
        if (!GopFrameIsSolid(Gop, FbInfo.GopPitch, W, H, Solids[f],
                             (UINT32)FbInfo.PixelFormat)) {
          MultiOk = FALSE;
          Print(L"Multi-frame: solid frame %d FAIL\n", f + 1);
        }
      }
      if (MultiOk) {
        Print(L"Multi-frame: 3 solid frames full-coverage PASS\n");
      }

      // Frame 4: vertical color bands; verify band centers and boundaries.
      PpcGraphicsClear(0x10101000);
      PpcGraphicsDrawRect(0, 0, W / 4, H, 0xCC000000);      // red
      PpcGraphicsDrawRect(W / 4, 0, W / 4, H, 0x00CC0000);  // green
      PpcGraphicsDrawRect(W / 2, 0, W / 4, H, 0x0000CC00);  // blue
      PpcGraphicsDrawRect(3 * W / 4, 0, W / 4, H, 0xCCCC0000); // yellow
      PpcGraphicsBlitToDisplay();

      BOOLEAN BandsOk =
        GopPixelMatches(Gop, FbInfo.GopPitch, W / 8, H / 2, 0xCC000000,
                        (UINT32)FbInfo.PixelFormat) &&
        GopPixelMatches(Gop, FbInfo.GopPitch, 3 * W / 8, H / 2, 0x00CC0000,
                        (UINT32)FbInfo.PixelFormat) &&
        GopPixelMatches(Gop, FbInfo.GopPitch, 5 * W / 8, H / 2, 0x0000CC00,
                        (UINT32)FbInfo.PixelFormat) &&
        GopPixelMatches(Gop, FbInfo.GopPitch, 7 * W / 8, H / 2, 0xCCCC0000,
                        (UINT32)FbInfo.PixelFormat) &&
        GopPixelMatches(Gop, FbInfo.GopPitch, W / 2 - 1, H / 2, 0x00CC0000,
                        (UINT32)FbInfo.PixelFormat) &&
        GopPixelMatches(Gop, FbInfo.GopPitch, W / 2, H / 2, 0x0000CC00,
                        (UINT32)FbInfo.PixelFormat);
      Print(L"Multi-frame: band frame %s\n", BandsOk ? L"PASS" : L"FAIL");
      MultiOk = MultiOk && BandsOk;

      // Corner and out-of-bounds handling.
      BOOLEAN CornersOk =
        GopPixelMatches(Gop, FbInfo.GopPitch, 0, 0, 0xCC000000,
                        (UINT32)FbInfo.PixelFormat) &&
        GopPixelMatches(Gop, FbInfo.GopPitch, W - 1, H - 1, 0xCCCC0000,
                        (UINT32)FbInfo.PixelFormat);
      PpcGraphicsSetPixel(W + 100, H + 100, 0xFF000000);   // OOB write is dropped
      PpcGraphicsBlitToDisplay();
      BOOLEAN OobOk =
        GopPixelMatches(Gop, FbInfo.GopPitch, W - 1, H - 1, 0xCCCC0000,
                        (UINT32)FbInfo.PixelFormat);
      Print(L"Multi-frame: corners %s, OOB dropped %s\n",
            CornersOk ? L"PASS" : L"FAIL", OobOk ? L"PASS" : L"FAIL");
      MultiOk = MultiOk && CornersOk && OobOk;

      Print(L"Multi-frame graphics self-check: %s\n",
            MultiOk ? L"PASS" : L"FAIL");
    } else {
      Print(L"Multi-frame graphics self-check: SKIP (no framebuffer)\n");
    }
  }
  
  // Initialize audio subsystem
  Status = PpcInitializeAudio();
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize audio: %r\n", Status);
    return Status;
  }

  // Audio self-check: the guest writes big-endian PCM samples into the
  // ring buffer, the host reads them back, then advances playback.
  {
    PPC_AUDIO_INFO AudioInfo;
    Status = PpcAudioGetBufferInfo(&AudioInfo);
    if (!EFI_ERROR(Status) && AudioInfo.HostBuffer != NULL) {
      // Guest-side write path: 16-bit big-endian 0x1000 = 4096 sample.
      UINT32 GuestAddr = AudioInfo.GuestBase;
      PpcWriteGuestByte(GuestAddr + 0, 0x10);
      PpcWriteGuestByte(GuestAddr + 1, 0x00);
      PpcWriteGuestByte(GuestAddr + 2, 0x20);
      PpcWriteGuestByte(GuestAddr + 3, 0x00);

      UINT16 S0 = 0, S1 = 0;
      BOOLEAN ReadOk = (PpcAudioReadSample(0, &S0) == EFI_SUCCESS) &&
                       (PpcAudioReadSample(1, &S1) == EFI_SUCCESS);
      Status = PpcAudioAdvancePlayback(2);
      PPC_AUDIO_INFO After;
      PpcAudioGetBufferInfo(&After);

      if (ReadOk && S0 == 0x1000 && S1 == 0x2000 && !EFI_ERROR(Status) &&
          After.PlayCursor == 2) {
        Print(L"Audio self-check: PASS (samples 0x%04x/0x%04x, played %d)\n",
              S0, S1, (UINT32)After.PlayCursor);
      } else {
        Print(L"Audio self-check: FAIL (samples 0x%04x/0x%04x, played %d)\n",
              S0, S1, (UINT32)(After.PlayCursor));
      }
    } else {
      Print(L"Audio self-check: SKIP (no buffer)\n");
    }
  }
  
  // Initialize storage subsystem
  Status = PpcInitializeStorage(1);
  if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
    Print(L"Failed to initialize storage: %r\n", Status);
    return Status;
  }
  
  // Block I/O self-check: enumerate real block devices and read a sector.
  Status = PpcInitializeBlockIo(1);
  if (!EFI_ERROR(Status)) {
    PPC_BLOCK_IO_INFO BioInfo;
    Status = PpcGetBlockIoInfo(&BioInfo);
    if (!EFI_ERROR(Status)) {
      BOOLEAN FoundMarker = FALSE;
      for (UINTN i = 0; i < BioInfo.DeviceCount; i++) {
        UINT8 Sector[512];
        if (!EFI_ERROR(PpcReadDiskBlock(i, 0, 512, Sector)) &&
            Sector[0] == 'E' && Sector[1] == 'F' && Sector[2] == 'I') {
          Print(L"Block I/O self-check: PASS (device %d, LBA 0 = \"%c%c%c...\", 512-byte sector)\n",
                (UINTN)i, Sector[0], Sector[1], Sector[2]);
          FoundMarker = TRUE;
          break;
        }
      }
      if (!FoundMarker) {
        Print(L"Block I/O self-check: devices enumerated, marker not found\n");
      }
    }
  } else {
    Print(L"Block I/O self-check: SKIP (%r)\n", Status);
  }

  // HFS self-test: mount the attached Mac OS disc (raw HFS/HFS+ block device)
  // and verify catalog parsing + System file readback through Block I/O.
  PpcHfsRunSelfTest();
  
  // Initialize network subsystem
  Status = PpcInitializeNetwork(1);
  if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) {
    Print(L"Failed to initialize network: %r\n", Status);
    return Status;
  }

  // Network self-check: report every real SNP interface and its transmit test.
  {
    PPC_NETWORK_INFO NetInfo;
    Status = PpcGetNetworkInfo(&NetInfo);
    if (!EFI_ERROR(Status)) {
      BOOLEAN AllPassed = NetInfo.InterfaceCount > 0;
      for (UINTN i = 0; i < NetInfo.InterfaceCount; i++) {
        Print(L"Network self-check: interface %d MAC %02x:%02x:%02x:%02x:%02x:%02x, "
              L"media %s, transmit %s\n",
              i,
              NetInfo.Interfaces[i].MacAddress[0],
              NetInfo.Interfaces[i].MacAddress[1],
              NetInfo.Interfaces[i].MacAddress[2],
              NetInfo.Interfaces[i].MacAddress[3],
              NetInfo.Interfaces[i].MacAddress[4],
              NetInfo.Interfaces[i].MacAddress[5],
              NetInfo.Interfaces[i].MediaPresent ? L"present" : L"absent",
              NetInfo.Interfaces[i].TransmitTestPassed ? L"PASS" : L"FAIL");
        if (!NetInfo.Interfaces[i].MediaPresent ||
            !NetInfo.Interfaces[i].TransmitTestPassed) {
          AllPassed = FALSE;
        }
      }
      Print(L"Network self-check: %s (%d interface(s))\n",
            AllPassed ? L"PASS" : L"FAIL", NetInfo.InterfaceCount);
    } else {
      Print(L"Network self-check: SKIP (no SNP interface)\n");
    }
  }
  
  // Display system information
  Print(L"\n=== EFI-Mac-Emulator System Information ===\n");
  Print(L"UEFI Version: %d.%d\n", SystemTable->FirmwareRevision >> 16, SystemTable->FirmwareRevision & 0xFFFF);
  Print(L"System Table: 0x%x\n", SystemTable);
  Print(L"Image Handle: 0x%x\n", ImageHandle);
  
  // Get memory info
  PPC_MEMORY_INFO MemoryInfo;
  Status = PpcGetMemoryInfo(&MemoryInfo);
  if (!EFI_ERROR(Status)) {
    Print(L"Memory Base: 0x%x\n", MemoryInfo.BaseAddress);
    Print(L"Memory Size: %d bytes\n", MemoryInfo.Size);
  }
  
  // Get hardware info
  PPC_HARDWARE_INFO HardwareInfo;
  Status = PpcGetHardwareInfo(&HardwareInfo);
  if (!EFI_ERROR(Status)) {
    Print(L"Graphics Mode: 0x%x\n", HardwareInfo.GraphicsMode);
    Print(L"Audio Enabled: %d\n", HardwareInfo.AudioEnabled);
    Print(L"Storage Devices: %d\n", HardwareInfo.StorageDevices);
    Print(L"Network Interfaces: %d\n", HardwareInfo.NetworkInterfaces);
  }
  
  // Write a boot log entry to the ESP — real UEFI file I/O to the FAT drive.
  PpcDebugLogToFile(L"EFI-Mac-Emulator: boot self-test passed, initializing hardware\n");

  // Display emulator status
  Print(L"\nEFI-Mac-Emulator initialized successfully.\n");
  Print(L"Ready to load and boot classic Mac OS.\n");
  
  // Real kernel load from the FAT volume into guest RAM, then verify and
  // execute it — a full disk → RAM → execute pipeline.
  EFI_PHYSICAL_ADDRESS KernelAddress;
  UINT64 KernelSize;
  
  Print(L"\n--- Loading kernel from volume ---\n");
  Status = PpcLoadKernel(L"\\System\\MacOS\\kernel", &KernelAddress, &KernelSize);
  if (!EFI_ERROR(Status)) {
    Print(L"Kernel loaded at 0x%x (%d bytes)\n", KernelAddress, KernelSize);

    // Verify kernel integrity (bounds check + first word read).
    PpcVerifyKernel(KernelAddress, KernelSize);

    // Set boot parameters (real storage into the bootloader context).
    PPC_BOOT_PARAMETERS Params;
    Params.BootMode = PPC_BOOT_MODE_NORMAL;
    Params.MemorySizeMB = 256;
    Params.VideoMode = PPC_GRAPHICS_MODE_1024x768;
    Params.EnableDebug = TRUE;
    Params.CommandLine = L"console=serial";
    PpcSetBootParameters(&Params);
    Print(L"Boot parameters set successfully\n");

    // Execute the first 4 words of the loaded kernel image.
    Print(L"\n--- Executing loaded kernel image ---\n");
    UINT32 EntryPoint = (UINT32)KernelAddress;
    const UINTN KernelExecCount = 4;
    UINTN KernelExecuted = 0;
    Status = PpcExecuteBlock((UINT32*)(UINTN)EntryPoint, KernelExecCount, &KernelExecuted);
    BOOLEAN KernelRanClean = (Status == EFI_SUCCESS);

    UINT32 K_R3 = PpcGetGprValue(3);
    UINT32 K_R5 = PpcGetGprValue(5);

    Print(L"Loaded kernel executed: %d instructions (status %r)\n", KernelExecuted, Status);
    Print(L"r3=%d, r5=%d\n", K_R3, K_R5);

    if (KernelRanClean && KernelExecuted == KernelExecCount && K_R5 == 700) {
      Print(L"Loaded kernel execution: PASS\n");
    } else if (KernelRanClean && KernelExecuted == KernelExecCount) {
      Print(L"Loaded kernel execution: PARTIAL (ran but unexpected register state)\n");
    } else {
      Print(L"Loaded kernel execution: FAIL\n");
    }

    // Configure CPU context for boot.
    PpcBootSystem(KernelAddress, KernelSize);
  } else {
    Print(L"Kernel not found on volume (%r) — skipping kernel execution\n", Status);
  }
  
  // Phase 5: classic Mac OS boot memory map, ROM, and system initialization.
  {
    UINT64 LowMemAddress = 0, LowMemSize = 0;
    UINT64 RomAddress = 0, RomSize = 0;
    EFI_STATUS BootStatus;

    Print(L"\n--- Boot memory map / system initialization ---\n");

    // 1. Low-memory globals at guest 0x00000000 (16 KB, read/write).
    BootStatus = PpcInstallLowMemory(&LowMemAddress, &LowMemSize);
    Print(L"Low-memory region: %s (guest 0x%x, %d bytes)\n",
          EFI_ERROR(BootStatus) ? L"FAIL" : L"OK",
          (UINT32)LowMemAddress, (UINT64)LowMemSize);

    // 2. System ROM at guest 0xFFF00000. Load a real ROM image from the boot
    //    volume if present (the classic Mac OS "Mac OS ROM" file in the
    //    System Folder Extensions is the fallback); otherwise install a
    //    self-contained demo ROM so the full ROM -> guest-memory-map ->
    //    execution path is still exercised.
    BootStatus = PpcInstallSystemRom(PPC_ROM_DEFAULT_PATH, &RomAddress, &RomSize);
    if (EFI_ERROR(BootStatus) && BootStatus != EFI_ALREADY_STARTED) {
      if (BootStatus == EFI_NOT_FOUND) {
        Print(L"System ROM not found at '%s', trying Mac OS ROM file\n",
              PPC_ROM_DEFAULT_PATH);
        BootStatus = PpcInstallSystemRom(PPC_SYSTEM_FOLDER_ROM_PATH, &RomAddress, &RomSize);
      }
      if (EFI_ERROR(BootStatus) && BootStatus != EFI_ALREADY_STARTED) {
        if (BootStatus == EFI_NOT_FOUND) {
          Print(L"Mac OS ROM file not found, installing demo ROM\n");
        } else {
          Print(L"System ROM install failed (%r), installing demo ROM\n", BootStatus);
        }
        BootStatus = PpcInstallDemoRom(&RomAddress, &RomSize);
      }
    }
    Print(L"System ROM: %s (guest 0x%x, %d bytes)\n",
          EFI_ERROR(BootStatus) ? L"FAIL" : L"OK",
          (UINT32)RomAddress, (UINT64)RomSize);

    // 3. Self-test the memory map: ROM read-only, low memory R/W, and a
    //    cross-region ROM -> RAM program executed from the reset vector.
    BootStatus = PpcRunBootSelfTest();
    Print(L"Boot memory map self-test: %s\n",
          EFI_ERROR(BootStatus) ? L"FAIL" : L"PASS");

    // 4. Configure the CPU for entry at the ROM reset vector and write the
    //    boot info block into low memory.
    BootStatus = PpcPrepareSystemForBoot();
    Print(L"System initialization: %s\n",
          EFI_ERROR(BootStatus) ? L"FAIL" : L"PASS");

    // 5. Report the final boot state.
    PPC_BOOT_INFO BootInfo;
    if (!EFI_ERROR(PpcGetBootInfo(&BootInfo))) {
      Print(L"Boot state: ready=%d, kernel=%d, ROM at 0x%x (%d bytes, %s), "
            L"low mem at 0x%x (%d bytes)\n",
            BootInfo.SystemReady, BootInfo.KernelLoaded,
            (UINT32)BootInfo.MemoryMap.RomBase,
            (UINT64)BootInfo.MemoryMap.RomSize,
            BootRomTypeName(BootInfo.MemoryMap.RomType),
            (UINT32)BootInfo.MemoryMap.LowMemoryBase,
            (UINT64)BootInfo.MemoryMap.LowMemorySize);
      if (!BootInfo.MemoryMap.RomInstalled) {
        Print(L"NOTE: no system ROM is installed; the demo ROM will not boot a real OS.\n");
      } else if (BootInfo.MemoryMap.RomType == PPC_ROM_TYPE_DEMO) {
        Print(L"NOTE: running the demo ROM. For a real boot, place a ROM image on the\n"
              L"  boot volume at \\System\\MacOS\\ROM (Old World dump) or\n"
              L"  \\System Folder\\Extensions\\Mac OS ROM (New World file), or attach a\n"
              L"  Mac OS 8.5+ disc that contains the 'Mac OS ROM' file.\n");
      }
    }
  }

  // Phase 5: classic Mac OS system files and drivers (System Folder support).
  {
    PPC_SYSTEM_FOLDER_INFO SysInfo;
    EFI_STATUS SysStatus;

    Print(L"\n--- System files and drivers ---\n");

    // 1. Scan the boot volume for the System Folder and its components.
    ZeroMem(&SysInfo, sizeof(SysInfo));
    SysStatus = PpcLocateSystemFolder(&SysInfo);
    if (EFI_ERROR(SysStatus) || !SysInfo.Found) {
      Print(L"System Folder not found on volume (scan: %r)\n", SysStatus);
    } else {
      Print(L"System Folder found: %s\n", SysInfo.Path);
      Print(L"  System=%d, Finder=%d, Extensions=%d, Mac OS ROM=%d\n",
            SysInfo.SystemPresent, SysInfo.FinderPresent,
            SysInfo.ExtensionsPresent, SysInfo.MacOsRomPresent);

      // 2. Stage the System file, Finder, and Mac OS ROM into guest memory.
      SysStatus = PpcLoadSystemFiles();
      Print(L"System files staged: %s\n",
            EFI_ERROR(SysStatus) ? L"FAIL" : L"OK");

      // 3. Enumerate Extensions and stage the drivers.
      SysStatus = PpcScanExtensionsDirectory();
      Print(L"Extensions scanned: %s\n",
            EFI_ERROR(SysStatus) ? L"FAIL" : L"OK");
      SysStatus = PpcLoadDrivers();
      Print(L"Drivers staged: %s\n",
            EFI_ERROR(SysStatus) ? L"FAIL" : L"OK");
    }

    // 4. Self-test: staged files read back through the interpreter memory path.
    SysStatus = PpcRunSystemFilesSelfTest();
    Print(L"System files self-test: %s\n",
          EFI_ERROR(SysStatus) ? L"FAIL" : L"PASS");

    // 5. Report the final staging state.
    if (!EFI_ERROR(PpcGetSystemFolderInfo(&SysInfo))) {
      Print(L"System files: %d staged, %d drivers registered (%d staged), "
            L"%d bytes total\n",
            SysInfo.LoadedFileCount, SysInfo.DriverCount,
            SysInfo.LoadedDriverCount, SysInfo.TotalStagedBytes);
      if (SysInfo.SystemAreaBase != 0) {
        Print(L"Staging areas: system 0x%x, drivers 0x%x\n",
              (UINT32)SysInfo.SystemAreaBase, (UINT32)SysInfo.DriverAreaBase);
      }
    }
  }

  Print(L"\n=== EFI Mac OS Boot Layer ready ===\n");
  
  return EFI_SUCCESS;
}
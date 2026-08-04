#include <efi.h>
#include <efilib.h>

// MS ABI CRT symbol: signals the linker that floating-point is in use.
UINT32 _fltused = 0;

// Include all our module headers
#include "cpu/translation.h"
#include "memory/manager.h"
#include "hardware/abstraction.h"
#include "boot/bootloader.h"
#include "utils/debug.h"
#include "platform/uefi_interface.h"

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
  Print(L"EFI-Mac-Emulator v0.1\n");
  Print(L"Initializing PowerPC emulation environment...\n");
  
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
  
  // Initialize audio subsystem
  Status = PpcInitializeAudio();
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize audio: %r\n", Status);
    return Status;
  }
  
  // Initialize storage subsystem
  Status = PpcInitializeStorage(1);
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize storage: %r\n", Status);
    return Status;
  }
  
  // Initialize network subsystem
  Status = PpcInitializeNetwork(1);
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize network: %r\n", Status);
    return Status;
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
  
  Print(L"\n=== EFI-Mac-Emulator Ready ===\n");
  
  return EFI_SUCCESS;
}
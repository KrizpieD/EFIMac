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
  
  // Initialize memory manager
  Status = PpcInitializeMemoryManager(0x00000000, 0x10000000);  // 256MB
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize memory manager: %r\n", Status);
    return Status;
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
  
  // Display emulator status
  Print(L"\nEFI-Mac-Emulator initialized successfully.\n");
  Print(L"Ready to load and boot classic Mac OS.\n");
  
  // In a real implementation, we would now:
  // 1. Load the Mac OS kernel
  // 2. Set up the PowerPC environment
  // 3. Begin execution of the emulated system
  
  // For demonstration purposes, let's simulate loading a kernel
  EFI_PHYSICAL_ADDRESS KernelAddress;
  UINT64 KernelSize;
  
  Print(L"\n--- Simulating kernel load ---\n");
  Status = PpcLoadKernel(L"\\System\\MacOS\\kernel", &KernelAddress, &KernelSize);
  if (!EFI_ERROR(Status)) {
    Print(L"Kernel loaded successfully at 0x%x\n", KernelAddress);
    
    // Set some boot parameters
    PPC_BOOT_PARAMETERS Params;
    Params.BootMode = PPC_BOOT_MODE_NORMAL;
    Params.MemorySizeMB = 256;
    Params.VideoMode = PPC_GRAPHICS_MODE_1024x768;
    Params.EnableDebug = TRUE;
    Params.CommandLine = L"console=serial";
    
    Status = PpcSetBootParameters(&Params);
    if (!EFI_ERROR(Status)) {
      Print(L"Boot parameters set successfully\n");
    }
  }
  
  Print(L"\n=== EFI-Mac-Emulator Ready ===\n");
  Print(L"To boot Mac OS, call PpcBootSystem() with kernel address\n");
  
  // In a real implementation, this would be the end of initialization
  // and we'd wait for a command to begin booting
  
  return EFI_SUCCESS;
}
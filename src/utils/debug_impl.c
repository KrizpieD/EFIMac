#include "debug.h"
#include <efi.h>
#include <efilib.h>
#include "cpu/translation.h"

// Debug context structure with enhanced functionality
typedef struct {
    BOOLEAN IsInitialized;
    UINT32  LogLevel;
    BOOLEAN LogToFile;
    CHAR16* LogFilePath;
    EFI_FILE_HANDLE LogFile;
} PPC_DEBUG_CONTEXT;

// Global debug context
STATIC PPC_DEBUG_CONTEXT g_DebugContext = {0};

EFI_STATUS
PpcInitializeDebug (
    IN UINT32 LogLevel,
    IN BOOLEAN LogToFile,
    IN CHAR16* LogFilePath
    )
{
    // Initialize the debug context
    ZeroMem(&g_DebugContext, sizeof(g_DebugContext));
    
    g_DebugContext.IsInitialized = TRUE;
    g_DebugContext.LogLevel = LogLevel;
    g_DebugContext.LogToFile = LogToFile;
    g_DebugContext.LogFilePath = LogFilePath;
    g_DebugContext.LogFile = NULL;
    
    Print(L"PowerPC Debug System initialized\n");
    Print(L"Log level: %d\n", LogLevel);
    Print(L"Log to file: %s\n", LogToFile ? L"YES" : L"NO");
    
    if (LogFilePath != NULL) {
        Print(L"Log file path: %s\n", LogFilePath);
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcDebugPrint (
    IN UINT32 Level,
    IN CHAR16* Message
    )
{
    if (Message == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Only print if log level allows it
    if (Level <= g_DebugContext.LogLevel) {
        Print(L"[DEBUG] %s\n", Message);
        
        // Optionally log to file as well
        if (g_DebugContext.LogToFile && g_DebugContext.LogFilePath != NULL) {
            // In a real implementation, we would write to the log file
            // This is a placeholder - actual file I/O would use UEFI file protocols
        }
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcDebugPrintHex (
    IN UINT32 Level,
    IN CHAR16* Prefix,
    IN UINT8* Data,
    IN UINTN DataSize
    )
{
    if (Data == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Only print if log level allows it
    if (Level <= g_DebugContext.LogLevel) {
        Print(L"[DEBUG] %s: ", Prefix);
        
        for (UINTN i = 0; i < DataSize && i < 32; i++) {
            Print(L"%02X ", Data[i]);
        }
        Print(L"\n");
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcDebugPrintInstruction (
    IN UINT32 Level,
    IN UINT32 Address,
    IN UINT32 Instruction
    )
{
    // Only print if log level allows it
    if (Level <= g_DebugContext.LogLevel) {
        Print(L"[DEBUG] 0x%x: 0x%08X\n", Address, Instruction);
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcDebugSetLogLevel (
    IN UINT32 Level
    )
{
    g_DebugContext.LogLevel = Level;
    Print(L"Debug log level set to %d\n", Level);
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcDebugGetLogLevel (
    OUT UINT32* Level
    )
{
    if (Level == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    *Level = g_DebugContext.LogLevel;
    
    return EFI_SUCCESS;
}

// Enhanced debugging functions

EFI_STATUS
PpcDebugPrintGprState (
    VOID
    )
{
    Print(L"General Purpose Register State:\n");
    
    for (UINTN i = 0; i < 32; i++) {
        if (i % 4 == 0) {
            Print(L"r%d-r%d: ", i, i+3);
        }
        
        // Just print a few registers to avoid too much output
        if (i < 8) {
            Print(L"0x%08X ", PpcGetGprValue((UINT8)i));
        } else {
            Print(L"      ");
        }
        
        if ((i+1) % 4 == 0) {
            Print(L"\n");
        }
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcDebugPrintMsrState (
    VOID
    )
{
    UINT32 MsrValue;
    EFI_STATUS Status = PpcGetRegisterValue(PPC_REG_MSR, &MsrValue);
    
    if (!EFI_ERROR(Status)) {
        Print(L"Machine State Register (MSR): 0x%08X\n", MsrValue);
    }
    
    return Status;
}

EFI_STATUS
PpcDebugPrintContext (
    VOID
    )
{
    Print(L"=== PowerPC Context Information ===\n");
    
    // Print general information
    PpcDebugPrintGprState();
    PpcDebugPrintMsrState();
    
    Print(L"===================================\n");
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcDebugLogToFile (
    IN CHAR16* Message
    )
{
    if (Message == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // In a real implementation, this would write to the log file
    // This is a placeholder for actual file I/O operations
    
    Print(L"[LOG] Writing to log file: %s\n", Message);
    
    return EFI_SUCCESS;
}

// Function to dump memory contents for debugging
EFI_STATUS
PpcDebugDumpMemory (
    IN VOID* Address,
    IN UINTN Size
    )
{
    if (Address == NULL || Size == 0) {
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"Memory dump at 0x%x (size: %d bytes):\n", Address, Size);
    
    UINT8* Data = (UINT8*)Address;
    UINTN BytesPerLine = 16;
    
    for (UINTN i = 0; i < Size; i += BytesPerLine) {
        Print(L"0x%08X: ", (UINTN)Data + i);
        
        // Print hex values
        for (UINTN j = 0; j < BytesPerLine && (i + j) < Size; j++) {
            Print(L"%02X ", Data[i + j]);
        }
        
        // Print ASCII representation
        Print(L" |");
        for (UINTN j = 0; j < BytesPerLine && (i + j) < Size; j++) {
            CHAR8 c = Data[i + j];
            if (c >= 32 && c <= 126) {
                Print(L"%c", c);
            } else {
                Print(L".");
            }
        }
        Print(L"|\n");
        
        // Limit output to prevent overwhelming console
        if (i > 512) {  // Just show first 512 bytes for large dumps
            Print(L"... (truncated)\n");
            break;
        }
    }
    
    return EFI_SUCCESS;
}

// Performance monitoring functions
EFI_STATUS
PpcDebugStartTimer (
    OUT UINT64* StartTime
    )
{
    if (StartTime == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // In a real implementation, this would use UEFI timer services
    *StartTime = 0;  // Placeholder
    
    Print(L"Performance timer started\n");
    
    return EFI_SUCCESS;
}

EFI_STATUS
PpcDebugStopTimer (
    IN  UINT64 StartTime,
    OUT UINT64* ElapsedTime
    )
{
    if (ElapsedTime == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // In a real implementation, this would calculate elapsed time using UEFI timer services
    *ElapsedTime = 0;  // Placeholder
    
    Print(L"Performance timer stopped. Elapsed: %d cycles\n", *ElapsedTime);
    
    return EFI_SUCCESS;
}
#include "debug.h"
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>

// Debug context structure
typedef struct {
    BOOLEAN IsInitialized;
    UINT32  LogLevel;
    BOOLEAN LogToFile;
    CHAR16* LogFilePath;
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
    
    Print(L"PowerPC Debug System initialized\n");
    Print(L"Log level: %d\n", LogLevel);
    Print(L"Log to file: %s\n", LogToFile ? L"YES" : L"NO");
    
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
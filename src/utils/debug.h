#ifndef __PPC_DEBUG_H__
#define __PPC_DEBUG_H__

#include <efi.h>

// Debug log levels
#define PPC_DEBUG_LEVEL_NONE    0
#define PPC_DEBUG_LEVEL_ERROR   1
#define PPC_DEBUG_LEVEL_WARNING 2
#define PPC_DEBUG_LEVEL_INFO    3
#define PPC_DEBUG_LEVEL_DEBUG   4

/**
  Initialize PowerPC debug system
  @param[in] LogLevel     Log level to use (0-4)
  @param[in] LogToFile    Whether to log to file as well
  @param[in] LogFilePath  Path to log file (if logging to file)
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcInitializeDebug (
    IN UINT32 LogLevel,
    IN BOOLEAN LogToFile,
    IN CHAR16* LogFilePath
    );

/**
  Print a debug message
  @param[in] Level   Debug level of the message
  @param[in] Message Message to print
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcDebugPrint (
    IN UINT32 Level,
    IN CHAR16* Message
    );

/**
  Print debug data in hexadecimal format
  @param[in] Level   Debug level of the message
  @param[in] Prefix  Prefix string to display before hex data
  @param[in] Data    Pointer to data to print
  @param[in] DataSize Size of data in bytes
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcDebugPrintHex (
    IN UINT32 Level,
    IN CHAR16* Prefix,
    IN UINT8* Data,
    IN UINTN DataSize
    );

/**
  Print a PowerPC instruction
  @param[in] Level      Debug level of the message
  @param[in] Address    Address of the instruction
  @param[in] Instruction The instruction value to print
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcDebugPrintInstruction (
    IN UINT32 Level,
    IN UINT32 Address,
    IN UINT32 Instruction
    );

/**
  Set the debug log level
  @param[in] Level New log level to use
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcDebugSetLogLevel (
    IN UINT32 Level
    );

/**
  Get the current debug log level
  @param[out] Level Pointer to store current log level
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcDebugGetLogLevel (
    OUT UINT32* Level
    );

/**
  Write a message to boot.log on the boot volume via real UEFI file I/O.
  @param[in] Message  Message to write
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcDebugLogToFile (
    IN CHAR16* Message
    );

/**
  Start a performance timer (real UEFI monotonic counter)
  @param[out] StartTime  Timestamp at start
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcDebugStartTimer (
    OUT UINT64* StartTime
    );

/**
  Stop a performance timer and return the elapsed time
  @param[in]  StartTime  Timestamp when the timer was started
  @param[out] ElapsedTime  Elapsed counts since start
  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
PpcDebugStopTimer (
    IN  UINT64 StartTime,
    OUT UINT64* ElapsedTime
    );

#endif // __PPC_DEBUG_H__
# EFI-Mac-Emulator Build Script
# This is a simplified build script for Windows/MinGW environment

Write-Host "EFI-Mac-Emulator Build Script"
Write-Host "================================"

# Check if we have the required tools
$gccAvailable = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gccAvailable) {
    Write-Host "Error: GCC compiler not found. Please install MinGW-w64 or ensure GCC is in PATH."
    exit 1
}

# Define source files
$sources = @(
    "src/main.c",
    "src/cpu/translation_impl.c",
    "src/memory/manager_impl.c", 
    "src/hardware/abstraction_impl.c",
    "src/boot/bootloader_impl.c",
    "src/utils/debug_impl.c",
    "src/platform/uefi_interface_impl.c"
)

# Define include directories
$includes = @(
    "-Isrc",
    "-Isrc/cpu", 
    "-Isrc/memory",
    "-Isrc/hardware",
    "-Isrc/boot",
    "-Isrc/utils",
    "-Isrc/platform"
)

# Check if build directory exists, create if not
if (!(Test-Path "build")) {
    New-Item -ItemType Directory -Name "build"
}

# Change to build directory
Set-Location "build"

# Create the output directory if it doesn't exist
if (!(Test-Path "output")) {
    New-Item -ItemType Directory -Name "output"
}

# Define output file
$outputFile = "EFI-Mac-Emulator.efi"

Write-Host "Compiling EFI-Mac-Emulator..."

# Compile with GCC (simplified approach)
try {
    $compileCmd = "gcc -m64 -Wall -Wextra -std=c11 -DUEFI -O2 -o `"$outputFile`" "
    
    # Add sources
    foreach ($src in $sources) {
        if (Test-Path "../$src") {
            $compileCmd += "../$src "
        } else {
            Write-Host "Warning: Source file not found: ../$src"
        }
    }
    
    # Add includes
    foreach ($inc in $includes) {
        $compileCmd += "$inc "
    }
    
    Write-Host "Running command:"
    Write-Host $compileCmd
    
    # Execute compilation
    Invoke-Expression $compileCmd
    
    if (Test-Path $outputFile) {
        Write-Host "SUCCESS: EFI-Mac-Emulator.efi built successfully!"
        Write-Host "Output file location: $(Get-Location)\$outputFile"
    } else {
        Write-Host "ERROR: Build failed - output file not created"
        exit 1
    }
} catch {
    Write-Host "ERROR during compilation: $($_.Exception.Message)"
    exit 1
}

Write-Host "Build process completed."
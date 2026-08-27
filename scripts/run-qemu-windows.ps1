# Boot the built EFI-Mac-Emulator.efi under QEMU + OVMF on Windows.
# Usage (PowerShell):
#   .\scripts\run-qemu-windows.ps1                      # no Mac disc attached
#   .\scripts\run-qemu-windows.ps1 -MacDisc mac_discs\System7_5_3.img
#   .\scripts\run-qemu-windows.ps1 -MacDisc mac_discs\System7_5_3.img `
#       -OldWorldRom "mac_roms\1997-11 - 79D68D63 - Power Mac G3 desktop.ROM"
#   .\scripts\run-qemu-windows.ps1 -MacDisc mac_discs\MacOS 8 (...8.1...).iso
#
# Validation matrix targets (see TODO.md "Validation Matrix"):
#   Mac OS 9.2.2  -MacDisc mac_discs\Apple Mac OS 9.2.2 [PowerMac G4].7z  (New World)
#   Mac OS 8.1    -MacDisc mac_discs\MacOS 8 (...8.1...).iso             (New World)
#   System 7.5.3  -MacDisc mac_discs\System7_5_3.img  + -OldWorldRom      (Old World)
#   Old World ROM -OldWorldRom mac_roms\...Power Mac 7100 (newer).ROM     (Old World)
# To attach an Old World ROM dump, pass -OldWorldRom; it is staged onto the ESP
# at \System\MacOS\ROM (the bootlayer's first-priority ROM source for Old World).
#
# Prereqs: chocolatey llvm + qemu; OVMF_CODE_4M.fd / OVMF_VARS_4M.fd unpacked
# from the Debian ovmf package into $env:TEMP\opencode\ovmf (see BUILD_INSTRUCTIONS.md).
param(
    [string]$Efi   = "$PSScriptRoot\..\build\EFI-Mac-Emulator.efi",
    [string]$Esp   = "$env:TEMP\opencode\esp",
    [string]$Ovmf  = "$env:TEMP\opencode\ovmf",
    [string]$MacDisc = "",
    [string]$OldWorldRom = "",
    [int]$Seconds  = 25
)

$ErrorActionPreference = "Stop"
$env:Path = "C:\Program Files\LLVM\bin;$env:Path"

$Efi   = (Resolve-Path $Efi).Path
$Ovmf  = (Resolve-Path $Ovmf).Path
New-Item -ItemType Directory -Force -Path $Esp | Out-Null

$BootOut   = Join-Path $env:TEMP "opencode\boot_out.txt"
$BootOutErr = "$BootOut.err"

# Stage the EFI image as the default boot target.
Copy-Item -Force $Efi (Join-Path $Esp "EFI\BOOT\BOOTX64.EFI")

# Stage an Old World ROM dump at \System\MacOS\ROM (first-priority ROM source).
if ($OldWorldRom -ne "") {
    $OldWorldRom = (Resolve-Path $OldWorldRom).Path
    New-Item -ItemType Directory -Force -Path (Join-Path $Esp "System\MacOS") | Out-Null
    Copy-Item -Force $OldWorldRom (Join-Path $Esp "System\MacOS\ROM")
    Write-Output "Staged Old World ROM: $OldWorldRom -> esp\System\MacOS\ROM"
}

# OVMF: code is read-only; vars is a writable copy of OVMF_VARS_4M.fd.
$Vars = Join-Path $Ovmf "vars.fd"
if (-not (Test-Path $Vars)) {
    Copy-Item (Join-Path $Ovmf "usr\share\OVMF\OVMF_VARS_4M.fd") $Vars
}

$Args = @(
    "-drive", "if=pflash,format=raw,readonly=on,file=$(Join-Path $Ovmf 'usr\share\OVMF\OVMF_CODE_4M.fd')",
    "-drive", "if=pflash,format=raw,file=$Vars",
    "-m", "512",
    "-drive", "file=fat:rw:$Esp,format=raw"
)
if ($MacDisc -ne "") {
    # Stage the disc into a space-free path (Start-Process splits arguments on
    # spaces, so paths under "New folder (2)" would otherwise break QEMU).
    $MacDisc = (Resolve-Path $MacDisc).Path
    $StageDir = Join-Path $env:TEMP "opencode\mac_disc"
    New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
    $StageName = [regex]::Replace((Split-Path $MacDisc -Leaf), '[^A-Za-z0-9._-]', '_')
    $Stage = Join-Path $StageDir $StageName
    if (-not (Test-Path $Stage) -or (Get-Item $Stage).Length -ne (Get-Item $MacDisc).Length) {
        Copy-Item -Force $MacDisc $Stage
    }
    $Args += @("-drive", "file=$Stage,format=raw,if=none,id=mac0",
               "-device", "ide-hd,drive=mac0")
}
$Args += @("-net", "none", "-serial", "stdio", "-display", "none", "-monitor", "none")

$p = Start-Process -FilePath "C:\Program Files\qemu\qemu-system-x86_64.exe" `
    -ArgumentList $Args `
    -NoNewWindow `
    -RedirectStandardOutput $BootOut `
    -RedirectStandardError $BootOutErr `
    -PassThru

Start-Sleep -Seconds $Seconds
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

Write-Output "Boot log: $BootOut"

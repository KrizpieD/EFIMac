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
    [switch]$NoReboot,
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

$QArgs = @(
    "-drive", "if=pflash,format=raw,readonly=on,file=$(Join-Path $Ovmf 'usr\share\OVMF\OVMF_CODE_4M.fd')",
    "-drive", "if=pflash,format=raw,file=$Vars",
    "-m", "1024",
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
    $QArgs += @("-drive", "file=$Stage,format=raw,if=none,id=mac0",
               "-device", "ide-hd,drive=mac0")
}
$QArgs += @("-net", "none", "-serial", "stdio", "-display", "none", "-monitor", "none")
if ($NoReboot) {
    # Exit instead of rebooting: a guest-initiated reset makes QEMU quit (with
    # the OS's pending-console output intact) instead of silently reloading.
    $QArgs += @("-no-reboot")
    Write-Output "no-reboot: guest reset requests will exit QEMU"
}

# QMP monitor so we can observe RESET events (with reason) from the guest.
$QmpPort = 14444
$Qdbg = Join-Path $env:TEMP "opencode\qemu_debug.log"
Remove-Item $Qdbg -ErrorAction SilentlyContinue
$QArgs += @("-qmp", "tcp:127.0.0.1:$QmpPort,server=on,wait=off",
           "-D", $Qdbg, "-d", "guest_errors,unimp")

$p = Start-Process -FilePath "C:\Program Files\qemu\qemu-system-x86_64.exe" `
    -ArgumentList $QArgs `
    -NoNewWindow `
    -RedirectStandardOutput $BootOut `
    -RedirectStandardError $BootOutErr `
    -PassThru

# Consume QMP events to catch guest-issued resets with their reason.
$qmpLog = Join-Path $env:TEMP "opencode\qmp_events.txt"
Remove-Item $qmpLog -ErrorAction SilentlyContinue
$qmpClient = $null
$stream = $null
$resetReasons = New-Object System.Collections.Generic.List[string]
$iterations = $Seconds * 4
$elapsed = 0
for ($i = 0; $i -lt $iterations; $i++) {
    if ($p.HasExited) {
        # Double-check: a spurious HasExited at startup (Process handle quirk)
        # would otherwise truncate the whole run. Give QEMU a moment and re-test
        # before declaring an early exit.
        Start-Sleep -Milliseconds 300
        $p.Refresh()
        if (-not $p.HasExited) { continue }
        break
    }
    if ($null -eq $qmpClient -and $elapsed -ge 3) {
        try {
            $qmpClient = New-Object System.Net.Sockets.TcpClient
            $qmpClient.Connect("127.0.0.1", $QmpPort)
            $stream = $qmpClient.GetStream()
            Start-Sleep -Milliseconds 300
            $greet = New-Object byte[] 4096
            $gn = $stream.Read($greet, 0, $greet.Length)
            if ($gn -gt 0) { Add-Content -Path $qmpLog -Value ([System.Text.Encoding]::ASCII.GetString($greet, 0, $gn)) }
            $cap = [System.Text.Encoding]::ASCII.GetBytes("{""execute"":""qmp_capabilities""}`n")
            $stream.Write($cap, 0, $cap.Length)
            $stream.Flush()
            Start-Sleep -Milliseconds 300
        } catch { $qmpClient = $null }
    }
    if ($null -ne $stream) {
        try {
            if ($stream.DataAvailable) {
                $buf = New-Object byte[] 65536
                $n = $stream.Read($buf, 0, $buf.Length)
                if ($n -gt 0) {
                    $text = [System.Text.Encoding]::ASCII.GetString($buf, 0, $n)
                    Add-Content -Path $qmpLog -Value $text
                    foreach ($m in [regex]::Matches($text, '"reason":"([^"]+)"')) {
                        $resetReasons.Add($m.Groups[1].Value)
                    }
                }
            }
        } catch {
            try { $stream.Dispose() } catch { }
            $stream = $null
        }
    }
    $elapsed = [math]::Floor($i / 4)
    Start-Sleep -Milliseconds 250
}
if ($qmpClient) { $qmpClient.Close() }
$resetReasonStr = $resetReasons -join ','
if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    $p.WaitForExit(5000) | Out-Null
    Write-Output "QEMU ran the full $Seconds s (no early exit)"
    Set-Content -Path (Join-Path $env:TEMP "opencode\qemu_result.txt") `
                -Value "full seconds=$Seconds resetReasons=[$resetReasonStr]"
} else {
    # If detection was wrong and QEMU is somehow still running, reclaim it so
    # we never orphan a guest. Kills are no-ops on an already-exited process.
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    $ecStr = "unknown"
    try {
        Add-Type -ErrorAction SilentlyContinue @"
using System;
using System.Runtime.InteropServices;
public static class WProc {
  [DllImport("kernel32.dll")] public static extern bool GetExitCodeProcess(IntPtr h, out uint c);
}
"@
        $x = [uint32]0xDEADBEEF
        if ($p.Handle -and [WProc]::GetExitCodeProcess($p.Handle, [ref]$x)) {
            $ecStr = "0x{0:X8} ({0})" -f $x
        } else {
            $p.WaitForExit(5000) | Out-Null
            $ec = $p.ExitCode
            if ($null -ne $ec) { $ecStr = "0x{0:X8} ({0})" -f $ec }
        }
    } catch { }
    Write-Output "QEMU exited early with code $ecStr"
    Set-Content -Path (Join-Path $env:TEMP "opencode\qemu_result.txt") `
                -Value "early seconds=$elapsed exit=$ecStr resetReasons=[$resetReasonStr]"
}

Write-Output "Boot log: $BootOut"

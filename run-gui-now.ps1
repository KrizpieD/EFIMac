# Run EFIMac with GUI for visual inspection
$ErrorActionPreference = "Stop"
$env:Path = "C:\Program Files\LLVM\bin;C:\Program Files\qemu;" + $env:Path

# Build
& "C:\Program Files\Git\bin\bash.exe" -lc "cd /c/Users/clayc/Desktop/EFIMac && bash scripts/build-windows.sh" | Select-Object -Last 2

# Setup
$Esp = "$env:TEMP\opencode\esp"
$Ovmf = "$env:TEMP\opencode\ovmf"
$OvmfCode = "$Ovmf\OVMF_CODE_4M.fd"
if (-not (Test-Path $OvmfCode)) { $OvmfCode = "$Ovmf\usr\share\OVMF\OVMF_CODE_4M.fd" }
$OvmfVars = "$Ovmf\OVMF_VARS_4M.fd"
if (-not (Test-Path $OvmfVars)) { $OvmfVars = "$Ovmf\vars.fd" }
$MacDisc = "$env:TEMP\opencode\mac_disc\Mac_OS_9.2.2.iso"
Remove-Item -Recurse -Force "$env:TEMP\opencode\mac_disc\C__" -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Esp | Out-Null
New-Item -ItemType Directory -Force -Path "$Esp\EFI\BOOT" | Out-Null
Copy-Item -Force "C:\Users\clayc\Desktop\EFIMac\build\EFI-Mac-Emulator.efi" "$Esp\EFI\BOOT\BOOTX64.EFI"

# Stage disc
$Stage = $null
if (Test-Path $MacDisc) {
  $StageDir = "$env:TEMP\opencode\mac_disc"
  $StageName = [regex]::Replace((Split-Path $MacDisc -Leaf), "[^A-Za-z0-9._-]", "_")
  $Stage = Join-Path $StageDir $StageName
  if (-not (Test-Path $Stage)) { Copy-Item $MacDisc $Stage }
}

# QEMU args with GUI (match original style)
$QArgs = @(
  "-machine", "q35",
  "-m", "1024",
  "-drive", "if=pflash,format=raw,readonly=on,file=$OvmfCode",
  "-drive", "if=pflash,format=raw,file=$OvmfVars",
  "-drive", "file=fat:rw:$Esp,format=raw",
  "-net", "none",
  "-serial", "stdio",
  "-vga", "std",
  "-monitor", "none"
)
if ($Stage -ne $null -and (Test-Path $Stage)) {
  $QArgs += @("-drive", "file=$Stage,format=raw,if=none,id=mac0", "-device", "ide-hd,drive=mac0")
}

& "C:\Program Files\qemu\qemu-system-x86_64.exe" @QArgs

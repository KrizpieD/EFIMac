param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('C0', 'C1', 'C2', 'C2A', 'C2B', 'C3', 'C4')]
    [string]$Contract,
    [int]$Seconds = 45,
    [string[]]$Require = @(),
    [string[]]$Forbid = @(),
    [string[]]$StopPattern = @(),
    [string]$MacDisc = "$env:TEMP\opencode\mac_disc\Mac_OS_9.2.2.iso",
    [string]$Esp = "$env:TEMP\opencode\esp",
    [string]$Ovmf = "$env:TEMP\opencode\ovmf"
)

$ErrorActionPreference = 'Stop'
$work = Join-Path $env:TEMP 'opencode'
New-Item -ItemType Directory -Force -Path $work | Out-Null
$stageDir = Join-Path $env:TEMP 'opencode\mac_disc'
Remove-Item -LiteralPath (Join-Path $stageDir 'C__') -Recurse -Force -ErrorAction SilentlyContinue

$bootOut = Join-Path $work 'boot_out.txt'
$gateLog = Join-Path $work ("bringup_gate_{0}.log" -f $Contract)
$resultPath = Join-Path $work 'bringup_gate_result.txt'
Remove-Item -LiteralPath $gateLog -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $resultPath -ErrorAction SilentlyContinue

$helper = Join-Path $PSScriptRoot 'run-qemu-windows.ps1'
$runArgs = @{
    MacDisc = $MacDisc
    Esp = $Esp
    Ovmf = $Ovmf
    Seconds = $Seconds
    StallSeconds = 0
    NoReboot = $true
    StopOnPattern = @($StopPattern)
    TimelinePattern = @($Require)
}

& $helper @runArgs | ForEach-Object { Write-Output $_ }

if (-not (Test-Path -LiteralPath $bootOut)) {
    $lines = @('result=FAIL', "contract=$Contract", 'reason=missing boot log')
    Set-Content -LiteralPath $resultPath -Value $lines
    $lines | ForEach-Object { Write-Output $_ }
    exit 1
}

Copy-Item -LiteralPath $bootOut -Destination $gateLog -Force
$raw = [IO.File]::ReadAllText($gateLog)
$clean = [regex]::Replace($raw, "`e\[[0-9;?]*[A-Za-z]", '')
$normalized = $clean.Replace('..', '')
$segments = @($clean -split '\.\.')

$defaultForbid = @('[FAIL]', 'GUEST STOP', 'budget stop', 'CPU MALFUNCT', 'NULLBRANCH[0]: 1')
$forbidden = @()
foreach ($pattern in (@($defaultForbid) + @($Forbid))) {
    if (-not ($forbidden -contains $pattern)) { $forbidden += $pattern }
}

$missing = @()
foreach ($pattern in $Require) {
    $hit = $normalized.Contains($pattern)
    if (-not $hit) {
        foreach ($segment in $segments) {
            if ($segment.Contains($pattern)) { $hit = $true; break }
        }
    }
    if (-not $hit) { $missing += $pattern }
}

$found = @()
foreach ($pattern in $forbidden) {
    if ($normalized.Contains($pattern)) { $found += $pattern }
}

$result = if ($missing.Count -eq 0 -and $found.Count -eq 0) { 'PASS' } else { 'FAIL' }
$lines = @(
    "result=$result",
    "contract=$Contract",
    "seconds=$Seconds",
    "log=$gateLog",
    "missing=$($missing -join '|')",
    "forbidden=$($found -join '|')"
)
Set-Content -LiteralPath $resultPath -Value $lines
$lines | ForEach-Object { Write-Output $_ }
if ($result -ne 'PASS') { exit 1 }

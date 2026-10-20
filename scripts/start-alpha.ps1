# Distributed beside the executable as Start-Freedom-Unbound.ps1.
[CmdletBinding()]
param(
    [string]$DataDirectory = '',
    [string]$LogDirectory = '',
    [switch]$SmokeTest
)

$ErrorActionPreference = 'Stop'
$executable = Join-Path $PSScriptRoot 'FreedomUniteNative.exe'
if (-not (Test-Path -LiteralPath $executable)) {
    throw 'Extract the complete package before starting Freedom Unbound.'
}
# A child PowerShell process owns these overrides; the user's environment is unchanged.
Get-ChildItem Env: | Where-Object { $_.Name -match '^(MHFU_|PSPRECOMP_)' } |
    ForEach-Object { Remove-Item -LiteralPath ('Env:' + $_.Name) }
$env:MHFU_HOME_SCREEN = '1'
$env:MHFU_GPU_DECODE = '0'
$env:PSPRECOMP_NO_INTERPRETER = '1'
if ($SmokeTest -and (-not $DataDirectory -or -not $LogDirectory)) {
    throw 'SmokeTest requires explicit isolated DataDirectory and LogDirectory paths.'
}
if ($DataDirectory) { $env:MHFU_DATA_DIR = [IO.Path]::GetFullPath($DataDirectory) }
$logRoot = if ($LogDirectory) { [IO.Path]::GetFullPath($LogDirectory) } else {
    Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'FreedomUnbound/logs'
}
$run = Join-Path $logRoot ((Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $PID)
New-Item -ItemType Directory -Path $run -Force | Out-Null
# Keep only the newest 10 session folders so logs do not pile up.
Get-ChildItem -LiteralPath $logRoot -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '^\d{8}-\d{6}-\d{3}-\d+$' } |
    Sort-Object Name -Descending | Select-Object -Skip 10 |
    ForEach-Object { Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction SilentlyContinue }
if ($SmokeTest) {
    $env:MHFU_INPUT_SCRIPT = '60:shot launcher;120:quit'
    $env:MHFU_SCREENSHOT_DIR = Join-Path $run 'captures'
    New-Item -ItemType Directory -Path $env:MHFU_SCREENSHOT_DIR -Force | Out-Null
}
Write-Host "Freedom Unbound alpha. Logs: $run"
if (-not $SmokeTest -and -not ('FreedomUnboundForeground' -as [type])) {
    # Compile before spawning, so granting activation cannot lag behind startup.
    Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class FreedomUnboundForeground { [DllImport("user32.dll")] public static extern bool AllowSetForegroundWindow(uint processId); }'
}
$clock = [Diagnostics.Stopwatch]::StartNew()
$process = Start-Process -FilePath $executable -WorkingDirectory $PSScriptRoot -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput (Join-Path $run 'stdout.log') -RedirectStandardError (Join-Path $run 'stderr.log')
$process.Handle | Out-Null
if (-not $SmokeTest) {
    # Pass the user's launch activation through the hidden logging helper.
    [FreedomUnboundForeground]::AllowSetForegroundWindow([uint32]$process.Id) | Out-Null
}
$process.WaitForExit()
$clock.Stop()
$reason = Select-String -LiteralPath (Join-Path $run 'stdout.log') -Pattern '^Runtime stopped:' |
    Select-Object -Last 1
$sha = [Security.Cryptography.SHA256]::Create()
$stream = [IO.File]::OpenRead($executable)
try {
    $executableHash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '')
} finally {
    $stream.Dispose()
    $sha.Dispose()
}
[ordered]@{
    exit_code = $process.ExitCode
    elapsed_seconds = $clock.Elapsed.TotalSeconds
    reason = if ($reason) { $reason.Line } else { $null }
    executable_sha256 = $executableHash
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'session.json') -Encoding UTF8
Write-Host "Logs saved: $run"
# The runtime reports a normal game-window close as 4; launcher Quit returns 0.
if ($process.ExitCode -notin @(0,4)) {
    Write-Host "Game stopped with exit code $($process.ExitCode). Include this session's logs in your report."
    exit 1
}
exit 0

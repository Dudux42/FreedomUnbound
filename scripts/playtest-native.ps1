param([switch]$Launcher)
$playtestRoot = Join-Path (Split-Path $PSScriptRoot) 'build/windows'
$ErrorActionPreference = 'Stop'
$executable = Join-Path $playtestRoot 'bin/FreedomUniteNative.exe'
$dataDirectory = Join-Path $playtestRoot 'game'
if (-not (Test-Path -LiteralPath $executable) -or -not (Test-Path -LiteralPath (Join-Path $dataDirectory 'EBOOT.ELF'))) {
    throw 'The isolated build and game installation must be prepared first.'
}
$lock = $null
try {
    $lock = [IO.File]::Open((Join-Path $playtestRoot '.manual-playtest.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
    $logs = Join-Path $playtestRoot ('investigation/manual-native-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item -ItemType Directory -Path (Join-Path $logs 'overlays') -Force | Out-Null
    $env:MHFU_DATA_DIR = $dataDirectory
    $env:MHFU_STRICT_HLE = '1'
    $env:PSPRECOMP_NO_INTERPRETER = '1'
    $env:MHFU_PERF = 'overlay+log'
    $env:MHFU_GPU_DECODE = '0'
    $env:MHFU_TRACE_RENDER = '1'
    $env:MHFU_TRACE_VERTEX_DECODE = '1'
    $env:MHFU_TRACE_TIMING = '1'
    $env:MHFU_TRACE_LOAD = '1'
    $env:MHFU_TRACE_AUDIO = '1'
    $env:MHFU_DUMP_OVERLAYS = Join-Path $logs 'overlays'
    $env:MHFU_WINDOW_TITLE = 'Freedom Unite - Native playtest'
    $env:MHFU_HOME_SCREEN = if ($Launcher) { '1' } else { '0' }
    foreach ($name in @('MHFU_NATIVE_60_EXPERIMENT','MHFU_INPUT_SCRIPT','MHFU_INPUT_LIVE','MHFU_UNTHROTTLED','MHFU_SCREENSHOT_DIR','MHFU_INTERNAL_SCALE','MHFU_AUTO_CONFIRM','MHFU_NO_RENDER','MHFU_NO_AUDIO','MHFU_DISABLE_OVERLAY_SLOTS','PSPRECOMP_MAX_DISPATCHES')) {
        Remove-Item -LiteralPath ('Env:' + $name) -ErrorAction SilentlyContinue
    }
    Write-Host 'Native-only playtest. Select Test from the isolated save.'
    foreach ($name in @('MHFU_CHECK_GPU_DECODE','MHFU_CHECK_DECODE','MHFU_NO_SIMD_SKINNING','MHFU_NO_FAST_DECODE','MHFU_NO_SPARSE_DECODE','MHFU_PERF_ALTERNATE')) {
        Remove-Item -LiteralPath ('Env:' + $name) -ErrorAction SilentlyContinue
    }
    Write-Host ('Logs: ' + $logs)
    Write-Host 'Timing measurements enabled; gameplay remains at 30 FPS.'
    Write-Host 'In the hunt, stand still briefly, attack several times, and gather. Then close the game.'
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $launchOptions = @{FilePath=$executable; WorkingDirectory=(Split-Path $executable); PassThru=$true; RedirectStandardOutput=(Join-Path $logs 'stdout.log'); RedirectStandardError=(Join-Path $logs 'stderr.log')}
    if (-not $Launcher) { $launchOptions.ArgumentList = '--play' }
    $process = Start-Process @launchOptions
    # Keep the process handle open so Windows PowerShell retains its exit code.
    $process.Handle | Out-Null
    $process.WaitForExit()
    Write-Host ('Game exit code: ' + $process.ExitCode)
    $reason = Select-String -LiteralPath (Join-Path $logs 'stdout.log') -Pattern '^Runtime stopped:' | Select-Object -Last 1
    if ($reason) { Write-Host $reason.Line }
    [ordered]@{
        exit_code = $process.ExitCode
        exit_hex = if ($null -ne $process.ExitCode) { '0x{0:X8}' -f ([long]$process.ExitCode -band 0xFFFFFFFFL) } else { $null }
        elapsed_seconds = $timer.Elapsed.TotalSeconds
        reason = if ($reason) { $reason.Line } else { $null }
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $logs 'exit.json') -Encoding UTF8
    Write-Host ('Logs preserved: ' + $logs)
} finally {
    if ($lock) { $lock.Dispose() }
}

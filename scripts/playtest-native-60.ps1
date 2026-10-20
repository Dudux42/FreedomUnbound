# User-operated timing research; each session starts from a fresh copied save.
[CmdletBinding()]
param([switch]$Reference, [switch]$PrepareOnly)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $workspace 'build/windows'
$package = Get-ChildItem -LiteralPath (Join-Path $buildRoot 'packages') -Directory -Filter 'FreedomUnite-Native60Research-*' | Sort-Object Name -Descending | Select-Object -First 1
if (-not $package) { throw 'Build and package the native timing research version first.' }
$metadata = Get-Content -LiteralPath (Join-Path $package.FullName 'build-info.json') -Raw | ConvertFrom-Json
if (-not $metadata.native_60_research_hooks) { throw 'This package has no native timing research hooks.' }
$executable = Join-Path $package.FullName 'FreedomUniteNative.exe'
if ((Get-FileHash -LiteralPath $executable).Hash -ne $metadata.executable_sha256) { throw 'Research executable hash mismatch.' }
foreach ($file in (Get-Content -LiteralPath (Join-Path $package.FullName 'build-files.json') -Raw | ConvertFrom-Json)) {
    if ((Get-FileHash -LiteralPath (Join-Path $package.FullName $file.file)).Hash -ne $file.sha256) { throw "Research package hash mismatch: $($file.file)" }
}
$sourceData = Join-Path $buildRoot 'game'
$run = Join-Path $buildRoot ('investigation/native-60fps/manual-research-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$data = Join-Path $run 'game'
$lock = $null
$oldEnvironment = @{}
$environmentChanged = $false
try {
    $lock = [IO.File]::Open((Join-Path $buildRoot '.manual-playtest.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
    New-Item -ItemType Directory -Path $data -ErrorAction Stop | Out-Null
    foreach ($name in @('EBOOT.ELF','settings.ini','ms0','pipeline_cache.bin','pipeline_keys.bin')) {
        $source = Join-Path $sourceData $name
        if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $data -Recurse }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $data 'EBOOT.ELF'))) { throw 'Install the supported USA game first.' }
    $settingsPath = Join-Path $data 'settings.ini'
    $settings = Get-Content -LiteralPath $settingsPath -Raw
    $settings = $settings -replace '(?m)^video.frame_rate=.*$', 'video.frame_rate=30'
    $settings = $settings -replace '(?m)^video.fast_loading=.*$', 'video.fast_loading=0'
    $settings = $settings -replace '(?m)^video.unthrottled=.*$', 'video.unthrottled=0'
    $settings = $settings -replace '(?m)^video.internal_scale=.*$', 'video.internal_scale=1'
    $settings = $settings -replace '(?m)^audio.volume=.*$', 'audio.volume=20'
    Set-Content -LiteralPath $settingsPath -Value $settings -Encoding utf8
    $mode = if ($Reference) { '30 FPS reference' } else { 'Native 60 FPS research' }
    [ordered]@{mode=$mode; package=$package.FullName; data=$data; original_data=$sourceData} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'session.json')
    Write-Host "$mode - separate copied save"
    if (-not $Reference) {
        Write-Host 'Native 60 FPS remains experimental: gameplay validation failed on October 10.'
        Write-Host 'Known issues: climbing/falling, gathering/carving, HUD timing and visible attacks missing damage.'
        Write-Host 'Subsequent research builds are diagnostic candidates until those cases pass comparison.'
    }
    Write-Host 'Current diagnostic focus: repeat the same climb, ledge jump/drop and gathering point in Reference and Native60; carve once if convenient.'
    Write-Host 'A full hunt is unnecessary for this trace. Action-state and movement observations are collected automatically.'
    Write-Host 'Combat validation remains pending: visible swings missing contact/damage are not claimed fixed.'
    Write-Host 'Report faster or slower actions, odd collision/damage, animation freezes, audio gaps and frame drops.'
    Write-Host 'Keep renderer frame rate at 30: native updates are enabled separately; this avoids interpolation.'
    Write-Host 'Start at 1x internal resolution for the timing comparison; raise it afterward for a separate performance check.'
    Write-Host 'The regular launcher and original save are unaffected. Progress here stays in this research copy.'
    Write-Host "Logs and copied save: $run"
    if ($PrepareOnly) { return }
    $environmentChanged = $true
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(MHFU_|PSPRECOMP_)' } | ForEach-Object {
        $oldEnvironment[$_.Name] = $_.Value
        Remove-Item -LiteralPath ('Env:' + $_.Name)
    }
    $env:MHFU_DATA_DIR = $data
    $env:MHFU_NATIVE_60_EXPERIMENT = if ($Reference) { '0' } else { '1' }
    $env:MHFU_TRACE_TIMING = '1'
    $env:MHFU_TIMING_PLAYER_STATE = '1'
    $env:MHFU_TRACE_AUDIO = '1'
    $env:MHFU_PERF = 'overlay+log'
    $env:MHFU_GPU_DECODE = '0'
    $env:MHFU_STRICT_HLE = '1'
    $env:PSPRECOMP_NO_INTERPRETER = '1'
    $env:MHFU_WINDOW_TITLE = "Freedom Unbound - $mode (copied save)"
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $executable -ArgumentList '--play' -WorkingDirectory $package.FullName -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'stdout.log') -RedirectStandardError (Join-Path $run 'stderr.log')
    $process.Handle | Out-Null
    $process.WaitForExit()
    [ordered]@{exit_code=$process.ExitCode; elapsed_seconds=$timer.Elapsed.TotalSeconds} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'exit.json')
    Write-Host "Exit code: $($process.ExitCode). Research logs: $run"
} finally {
    if ($environmentChanged) {
        Get-ChildItem Env: | Where-Object { $_.Name -match '^(MHFU_|PSPRECOMP_)' } | ForEach-Object { Remove-Item -LiteralPath ('Env:' + $_.Name) }
        foreach ($name in $oldEnvironment.Keys) { Set-Item -LiteralPath ('Env:' + $name) -Value $oldEnvironment[$name] }
    }
    if ($lock) { $lock.Dispose() }
}

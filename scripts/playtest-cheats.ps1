# Persistent isolated progression copy for the owner's cheat acceptance run.
[CmdletBinding()]
param([switch]$PrepareOnly)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $workspace 'build/windows'
$package = Get-ChildItem -LiteralPath (Join-Path $buildRoot 'packages') -Directory |
    Sort-Object LastWriteTime -Descending | Where-Object {
        $infoPath = Join-Path $_.FullName 'build-info.json'
        if (-not (Test-Path -LiteralPath $infoPath)) { return $false }
        $info = Get-Content -LiteralPath $infoPath -Raw | ConvertFrom-Json
        return $info.PSObject.Properties['native_cheats_hook_version'] -and $info.native_cheats_hook_version -ge 6 -and
            -not $info.native_60_research_hooks -and -not $info.action_timing_diagnostics
    } | Select-Object -First 1
if (-not $package) { throw 'Build, test and package the cheat implementation first.' }
foreach ($entry in (Get-Content -LiteralPath (Join-Path $package.FullName 'build-files.json') -Raw | ConvertFrom-Json)) {
    if ((Get-FileHash -LiteralPath (Join-Path $package.FullName $entry.file)).Hash -ne $entry.sha256) {
        throw "Package hash mismatch: $($entry.file)"
    }
}
$testRoot = Join-Path $buildRoot 'cheats-playtest'
$data = Join-Path $testRoot 'game'
$oldEnvironment = @{}
$environmentChanged = $false
$lock = $null
try {
    $lock = [IO.File]::Open((Join-Path $buildRoot '.manual-playtest.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
    if (-not (Test-Path -LiteralPath $data)) {
        $sourceData = Join-Path $buildRoot 'investigation/alpha-owner-playtest/game'
        if (-not (Test-Path -LiteralPath (Join-Path $sourceData 'EBOOT.ELF'))) { $sourceData = Join-Path $buildRoot 'game' }
        New-Item -ItemType Directory -Path $data | Out-Null
        foreach ($name in @('EBOOT.ELF','settings.ini','ms0')) {
            $source = Join-Path $sourceData $name
            if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $data -Recurse }
        }
        if (-not (Test-Path -LiteralPath (Join-Path $data 'EBOOT.ELF'))) { throw 'Supported USA installation missing.' }
        $settingsPath = Join-Path $data 'settings.ini'
        $settings = Get-Content -LiteralPath $settingsPath -Raw
        $settings = $settings -replace '(?m)^video.frame_rate=.*$', 'video.frame_rate=30'
        $settings = $settings -replace '(?m)^video.internal_scale=.*$', 'video.internal_scale=1'
        $settings = $settings -replace '(?m)^audio.volume=.*$', 'audio.volume=20'
        Set-Content -LiteralPath $settingsPath -Value $settings -Encoding utf8
        "health=1`nstamina=1`none_hit=1" | Set-Content -LiteralPath (Join-Path $data 'cheats.ini') -Encoding ascii
        [ordered]@{source_data=$sourceData; copied_at=(Get-Date -Format o)} | ConvertTo-Json |
            Set-Content -LiteralPath (Join-Path $testRoot 'copy-record.json') -Encoding utf8
    }
    [ordered]@{package=$package.FullName; data=$data} | ConvertTo-Json |
        Set-Content -LiteralPath (Join-Path $testRoot 'current-package.json') -Encoding utf8
    Write-Host "Cheat progression save: $data"
    Write-Host 'This same copy is retained between runs. Cheats are under Cheats in the launcher and Esc menu.'
    Write-Host 'Check damage immunity, sustained sprint/rolls, one damaging hit, carving/rewards, then toggles off and save/reload.'
    if ($PrepareOnly) { return }
    $environmentChanged = $true
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(MHFU_|PSPRECOMP_)' } | ForEach-Object {
        $oldEnvironment[$_.Name] = $_.Value
        Remove-Item -LiteralPath ('Env:' + $_.Name)
    }
    $logs = Join-Path $testRoot ('logs/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $logs -Force | Out-Null
    $env:MHFU_DATA_DIR = $data
    $env:MHFU_HOME_SCREEN = '1'
    $env:MHFU_GPU_DECODE = '0'
    $env:MHFU_PERF = 'overlay+log'
    $env:PSPRECOMP_NO_INTERPRETER = '1'
    $process = Start-Process -FilePath (Join-Path $package.FullName 'FreedomUniteNative.exe') -WorkingDirectory $package.FullName -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logs 'stdout.log') -RedirectStandardError (Join-Path $logs 'stderr.log')
    $process.Handle | Out-Null
    $process.WaitForExit()
    [ordered]@{exit_code=$process.ExitCode; package=$package.FullName; data=$data} | ConvertTo-Json |
        Set-Content -LiteralPath (Join-Path $logs 'session.json') -Encoding utf8
} finally {
    if ($environmentChanged) {
        Get-ChildItem Env: | Where-Object { $_.Name -match '^(MHFU_|PSPRECOMP_)' } | ForEach-Object { Remove-Item -LiteralPath ('Env:' + $_.Name) }
        foreach ($name in $oldEnvironment.Keys) { Set-Item -LiteralPath ('Env:' + $name) -Value $oldEnvironment[$name] }
    }
    if ($lock) { $lock.Dispose() }
}

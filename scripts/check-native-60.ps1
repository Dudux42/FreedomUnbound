# Bounded comparison on copied data; never run a hunt or save through this script.
[CmdletBinding()]
param(
    [string]$Executable = 'build/windows/bin/FreedomUniteNative.exe',
    [string]$InputSteps = '180:shot cadence;480:quit',
    [ValidateSet('baseline','native60')][string[]]$Modes = @('baseline','native60'),
    [ValidateRange(1,55)][int]$Seconds = 30,
    [switch]$VillageWalk,
    [switch]$MenuRepeat,
    [switch]$WatchMovement
)
$ErrorActionPreference = 'Stop'
if ($VillageWalk -and $MenuRepeat) { throw 'Choose one comparison scenario.' }
$workspace = Split-Path -Parent $PSScriptRoot
$sourceData = Join-Path $workspace 'build/windows/investigation/native-60fps/test-game'
$evidence = Join-Path $workspace ('build/windows/investigation/native-60fps/native-comparison-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$binary = [IO.Path]::GetFullPath((Join-Path $workspace $Executable))
if (-not (Test-Path -LiteralPath $binary) -or -not (Test-Path -LiteralPath (Join-Path $sourceData 'EBOOT.ELF'))) {
    throw 'Build the research executable and prepare copied timing test data first.'
}
$savedEnvironment = @{}
Get-ChildItem Env: | Where-Object { $_.Name -match '^(MHFU_|PSPRECOMP_)' } | ForEach-Object {
    $savedEnvironment[$_.Name] = $_.Value
    Remove-Item -LiteralPath ('Env:' + $_.Name)
}
try {
    foreach ($mode in $Modes) {
        $run = Join-Path $evidence $mode
        $data = Join-Path $run 'game'
        $captures = Join-Path $run 'captures'
        New-Item -ItemType Directory -Path $data,$captures -Force | Out-Null
        Get-ChildItem -LiteralPath $sourceData | Copy-Item -Destination $data -Recurse
        $settingsPath = Join-Path $data 'settings.ini'
        $settings = Get-Content -LiteralPath $settingsPath -Raw
        $settings = $settings -replace '(?m)^video.frame_rate=.*$', 'video.frame_rate=30'
        $settings = $settings -replace '(?m)^video.fast_loading=.*$', 'video.fast_loading=0'
        $settings = $settings -replace '(?m)^video.internal_scale=.*$', 'video.internal_scale=1'
        $settings = $settings -replace '(?m)^audio.volume=.*$', 'audio.volume=20'
        Set-Content -LiteralPath $settingsPath -Value $settings -Encoding utf8
        $saveHashes = @(Get-ChildItem -LiteralPath (Join-Path $data 'ms0') -Recurse -File | Get-FileHash)
        $env:MHFU_DATA_DIR = $data
        $env:MHFU_NATIVE_60_EXPERIMENT = if ($mode -eq 'native60') { '1' } else { '0' }
        $env:MHFU_TRACE_TIMING = '1'
        $env:MHFU_PERF = 'overlay+log'
        $env:MHFU_GPU_DECODE = '0'
        $env:MHFU_STRICT_HLE = '1'
        $env:PSPRECOMP_NO_INTERPRETER = '1'
        $env:MHFU_HOME_SCREEN = '0'
        $env:MHFU_WINDOW_TITLE = "Freedom Unbound - $mode timing research"
        $env:MHFU_INPUT_SCRIPT = if ($MenuRepeat) {
            if ($mode -eq 'native60') { '600:shot repeat-start;640:key Down 240;920:key Up 2;960:shot repeat-stop;1000:quit' }
            else { '300:shot repeat-start;320:key Down 120;460:key Up 2;480:shot repeat-stop;500:quit' }
        } else { $InputSteps }
        $env:MHFU_SCREENSHOT_DIR = $captures
        $env:MHFU_SCREENSHOT_EVERY = '0'
        if ($WatchMovement) {
            # Measured player actor + 0x200, the translation vector used by
            # the shared actor update. Watch stores without changing values.
            $env:PSPRECOMP_WATCH_WRITE = '0x090B3780'
            $env:PSPRECOMP_WATCH_WRITE_SIZE = '12'
        }
        $liveInput = Join-Path $run 'village-input.txt'
        if ($VillageWalk) {
            Set-Content -LiteralPath $liveInput -Value ''
            $env:MHFU_INPUT_LIVE = $liveInput
        }
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $binary -ArgumentList '--play' -WorkingDirectory (Split-Path $binary) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'stdout.log') -RedirectStandardError (Join-Path $run 'stderr.log')
        $process.Handle | Out-Null
        $walkStarted = $null
        $walkReadyAt = $null
        $walkStopped = $false
        $villageLoaded = $false
        if ($VillageWalk) {
            # Wait for the measured player idle sample, rather than sending
            # movement while a slower reference is still loading. Each live
            # input line starts its own frame count when the game reads it.
            while ($timer.Elapsed.TotalSeconds -lt $Seconds -and -not $process.WaitForExit(100)) {
                if ($null -eq $walkStarted) {
                    $recent = Get-Content -LiteralPath (Join-Path $run 'stdout.log') -Tail 100
                    if (-not $villageLoaded) {
                        $villageLoaded = [bool](Select-String -LiteralPath (Join-Path $run 'stdout.log') -SimpleMatch '[overlay] installed lobby_task' -Quiet)
                    }
                    if ($villageLoaded -and $null -eq $walkReadyAt -and $recent -match '\[fu-action\] point=0x088d5e30 object=0x090b3580 .*id=1 ') {
                        $walkReadyAt = $timer.Elapsed.TotalSeconds
                    }
                    # Allow the village's entry transition to finish before
                    # using the same keyboard path verified in the manual run.
                    if ($null -ne $walkReadyAt -and $timer.Elapsed.TotalSeconds - $walkReadyAt -ge 2.0) {
                        $walkStarted = $timer.Elapsed.TotalSeconds
                        # The binary live-input reader keeps carriage returns;
                        # write explicit LF so quit and capture names parse.
                        $walkFrames = if ($mode -eq 'native60') { 120 } else { 60 }
                        [IO.File]::AppendAllText($liveInput, "0:shot walk-start`n0:key W $walkFrames`n")
                    }
                } elseif (-not $walkStopped -and $timer.Elapsed.TotalSeconds - $walkStarted -ge 2.0) {
                    [IO.File]::AppendAllText($liveInput, "10:shot walk-stop`n30:quit`n")
                    $walkStopped = $true
                }
            }
            $completed = $process.HasExited
        } else {
            $completed = $process.WaitForExit($Seconds * 1000)
        }
        if (-not $completed) { $process.Kill(); $process.WaitForExit() }
        $unchanged = $true
        foreach ($save in $saveHashes) {
            if ((Get-FileHash -LiteralPath $save.Path).Hash -ne $save.Hash) { $unchanged = $false }
        }
        [ordered]@{completed=$completed; exit_code=$process.ExitCode; elapsed_seconds=$timer.Elapsed.TotalSeconds; save_files_unchanged=$unchanged; village_walk_started_at=$walkStarted; village_walk_stopped=$walkStopped} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'result.json')
        if (-not $completed -or -not $unchanged -or $process.ExitCode -ne 4 -or ($VillageWalk -and -not $walkStopped)) {
            throw "Timing run failed: $mode; inspect $run"
        }
    }
    Write-Host "Timing comparison: $evidence"
} finally {
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(MHFU_|PSPRECOMP_)' } | ForEach-Object { Remove-Item -LiteralPath ('Env:' + $_.Name) }
    foreach ($name in $savedEnvironment.Keys) { Set-Item -LiteralPath ('Env:' + $name) -Value $savedEnvironment[$name] }
}

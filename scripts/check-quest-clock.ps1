# Bounded quest-clock observation on copied data. Does not save or attack.
[CmdletBinding()]
param(
    [ValidateSet('baseline','native60')][string]$Mode = 'baseline',
    [string]$Executable = 'build/windows/bin/FreedomUniteNative.exe',
    [string]$DataSource = 'build/windows/game',
    [ValidateRange(2,8)][int]$ObservationSeconds = 2,
    [string]$ObservationSteps = '',
    [string]$NavigationSteps = '60:key Return;200:key Return;300:key Return;420:key Return;600:key Return;730:key Return;830:key Return;950:key Space;1050:key Space;1150:key Space;1350:key F;1500:key F;1650:key Space;1850:key Space;2300:shot village',
    [switch]$Explore,
    [switch]$ElderRoute,
    [switch]$BaselineNavigation,
    [switch]$PlayerMotion
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$run = Join-Path $workspace ('build/windows/investigation/native-60fps/quest-clock-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + $Mode)
$data = Join-Path $run 'game'
$captures = Join-Path $run 'captures'
$live = Join-Path $run 'input.txt'
$savedEnvironment = @{}
$process = $null
try {
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(MHFU_|PSPRECOMP_)' } | ForEach-Object {
        $savedEnvironment[$_.Name] = $_.Value
        Remove-Item -LiteralPath ('Env:' + $_.Name)
    }
    New-Item -ItemType Directory -Path $data,$captures -Force | Out-Null
    foreach ($name in @('EBOOT.ELF','settings.ini','ms0','pipeline_cache.bin','pipeline_keys.bin')) {
        $source = Join-Path (Join-Path $workspace $DataSource) $name
        if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $data -Recurse }
    }
    $settingsPath = Join-Path $data 'settings.ini'
    $settings = Get-Content -LiteralPath $settingsPath -Raw
    foreach ($entry in @(@('video.frame_rate','30'),@('video.fast_loading','0'),@('video.unthrottled','0'),@('video.internal_scale','1'),@('video.window_scale','1'),@('video.fullscreen','0'),@('video.present_mode','immediate'),@('audio.volume','20'))) {
        $settings = $settings -replace ('(?m)^' + [regex]::Escape($entry[0]) + '=.*$'), ($entry[0] + '=' + $entry[1])
    }
    Set-Content -LiteralPath $settingsPath -Value $settings -Encoding utf8
    $saveHashes = @(Get-ChildItem -LiteralPath (Join-Path $data 'ms0') -Recurse -File | Get-FileHash)
    [IO.File]::WriteAllText($live, '')
    $env:MHFU_DATA_DIR = $data
    $env:MHFU_NATIVE_60_EXPERIMENT = if ($Mode -eq 'native60') { '1' } else { '0' }
    $env:MHFU_TRACE_TIMING = '1'
    if ($PlayerMotion) { $env:MHFU_TIMING_PLAYER_STATE = '1' }
    $env:MHFU_TIMING_QUEST_MESSAGES_ONLY = '1'
    $env:MHFU_TIMING_NAVIGATION = '1'
    if ($BaselineNavigation -and $Mode -eq 'native60') { $env:MHFU_NATIVE60_BASELINE_NAVIGATION = '1' }
    $env:MHFU_PERF = 'overlay+log'
    $env:MHFU_GPU_DECODE = '0'
    $env:MHFU_STRICT_HLE = '1'
    $env:PSPRECOMP_NO_INTERPRETER = '1'
    $env:MHFU_HOME_SCREEN = '0'
    $env:MHFU_INPUT_SCRIPT = $NavigationSteps
    $env:MHFU_INPUT_LIVE = $live
    $env:MHFU_SCREENSHOT_DIR = $captures
    $env:MHFU_SCREENSHOT_EVERY = '0'
    $env:MHFU_WINDOW_TITLE = "Freedom Unbound - $Mode quest-clock diagnostics"
    Write-Host "Quest-clock run: $run"
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $binary = [IO.Path]::GetFullPath((Join-Path $workspace $Executable))
    $process = Start-Process -FilePath $binary -ArgumentList '--play' -WorkingDirectory (Split-Path $binary) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $run 'stdout.log') -RedirectStandardError (Join-Path $run 'stderr.log')
    $process.Handle | Out-Null
    $clockStarted = $false
    $elderStarted = $false
    # Finish startup input before the live walking route. Late confirm presses
    # otherwise interact with nearby NPCs while walking past them.
    $lastStartupAction = @($NavigationSteps -split ';' | Where-Object { $_ -notmatch '^\d+:shot ' })[-1]
    if ($lastStartupAction -notmatch '^(\d+):(.+)$') { throw 'Missing startup input action' }
    $startupMarker = "[script] frame $($Matches[1]): $($Matches[2])"
    while ($timer.Elapsed.TotalSeconds -lt 58 -and -not $process.WaitForExit(100)) {
        if ($ElderRoute -and -not $elderStarted) {
            $log = Join-Path $run 'stdout.log'
            $recent = Get-Content -LiteralPath $log -Tail 100
            if ((Select-String -LiteralPath $log -SimpleMatch $startupMarker -Quiet) -and
                (Select-String -LiteralPath $log -SimpleMatch '[overlay] installed lobby_task' -Quiet) -and
                ($recent -match '\[fu-action\] point=0x088d5e30 object=0x090b3580 .*id=1 ')) {
                $elderStarted = $true
                $factor = if ($Mode -eq 'native60' -and -not $BaselineNavigation) { 2 } else { 1 }
                # Screen-verified Chief route. The right turn at the fence
                # reaches its boundary quickly; avoid waiting against it.
                $route = @(@(0,'shot village-start'),@(0,'axis leftx 1'),@(150,'axis leftx 0'),@(150,'axis lefty -1'),@(650,'axis lefty 0'),@(670,'axis leftx 1'),@(730,'axis leftx 0'),@(750,'axis lefty -1'),@(900,'axis lefty 0'),@(920,'axis leftx 1'),@(1000,'axis leftx 0'),@(1020,'axis lefty 1'),@(1065,'axis lefty 0'),@(1080,'axis leftx 1'),@(1105,'axis leftx 0'),@(1115,'key Space'),@(1135,'shot chief-talk'))
                if ($Explore) {
                    foreach ($frame in @(300,450,640)) { $route += ,@($frame,"shot north-$frame") }
                }
                # Separate presses reveal dialogue, choose one-star rank and
                # accept the first (no-fee) gathering quest. No save is issued.
                foreach ($page in 0..9) {
                    $route += ,@((1150 + $page * 20),'key Space')
                    if ($Explore) { $route += ,@((1166 + $page * 20),"shot quest-page-$page") }
                }
                $route += @(@(1360,'shot quest-accepted'),@(1380,'axis leftx -1'),@(1460,'axis leftx 0'),@(1460,'axis lefty -1'),@(1590,'axis lefty 0'),@(1600,'key E'),@(1650,'shot departure'))
                # Capturing the large window stalls navigation. Keep route
                # captures for exploration; accepted measurements retain the
                # quest-entry and final captures below.
                if (-not $Explore) { $route = @($route | Where-Object { $_[1] -notlike 'shot *' }) }
                $lines = ($route | ForEach-Object { "$($_[0] * $factor):$($_[1])" }) -join "`n"
                [IO.File]::AppendAllText($live, "$lines`n")
            }
        }
        if (-not $clockStarted -and (Select-String -LiteralPath (Join-Path $run 'stdout.log') -SimpleMatch '[fu-navigation] normal pacing restored' -Quiet)) {
            $clockStarted = $true
            $frames = $ObservationSeconds * $(if ($Mode -eq 'native60') { 60 } else { 30 })
            $steps = if ($ObservationSteps) { ($ObservationSteps -replace ';', "`n") + "`n" } else { "${frames}:shot clock-stop`n$($frames + 15):quit`n" }
            [IO.File]::AppendAllText($live, "0:shot clock-start`n$steps")
        }
    }
    $completed = $process.HasExited
    if (-not $completed) { $process.Kill(); $process.WaitForExit() }
    $unchanged = $true
    foreach ($save in $saveHashes) { if ((Get-FileHash -LiteralPath $save.Path).Hash -ne $save.Hash) { $unchanged = $false } }
    if (@(Get-ChildItem -LiteralPath (Join-Path $data 'ms0') -Recurse -File).Count -ne $saveHashes.Count) { $unchanged = $false }
    $samples = @(Select-String -LiteralPath (Join-Path $run 'stdout.log') -Pattern '\[fu-quest-clock\].*pause=0 state=2 .*paced=1' | ForEach-Object {
        if ($_.Line -match 'object=(0x[0-9a-f]+) remaining=(\d+) .*calls=(\d+) virtual_us=(\d+) real_us=(\d+)') {
            [pscustomobject]@{object=$Matches[1]; remaining=[long]$Matches[2]; calls=[long]$Matches[3]; virtual_us=[long]$Matches[4]; real_us=[long]$Matches[5]}
        }
    })
    $measurements = @($samples | Group-Object object | ForEach-Object {
        $first = $_.Group[0]; $last = $_.Group[-1]
        $guestDelta = $last.virtual_us - $first.virtual_us
        $realDelta = $last.real_us - $first.real_us
        if ($_.Count -ge 3 -and $guestDelta -gt 0 -and $realDelta -gt 0 -and $first.remaining -gt $last.remaining) {
            # Report the full observation window, including quest entry and
            # shutdown. Separately expose interior samples, excluding those
            # endpoints consistently in both modes (not selected by rate).
            $interior = $null
            if ($_.Count -ge 5) {
                $start = $_.Group[1]; $end = $_.Group[-2]
                $guest = $end.virtual_us - $start.virtual_us
                $real = $end.real_us - $start.real_us
                if ($guest -gt 0 -and $real -gt 0) {
                    $interior = [ordered]@{samples=$_.Count-2; countdown_ticks=$start.remaining-$end.remaining;
                        update_calls=$end.calls-$start.calls; guest_seconds=$guest/1000000.0; real_seconds=$real/1000000.0;
                        ticks_per_guest_second=($start.remaining-$end.remaining)*1000000.0/$guest;
                        ticks_per_real_second=($start.remaining-$end.remaining)*1000000.0/$real}
                }
            }
            [ordered]@{object=$_.Name; samples=$_.Count; countdown_ticks=$first.remaining-$last.remaining;
                update_calls=$last.calls-$first.calls; guest_seconds=$guestDelta/1000000.0; real_seconds=$realDelta/1000000.0;
                ticks_per_guest_second=($first.remaining-$last.remaining)*1000000.0/$guestDelta;
                ticks_per_real_second=($first.remaining-$last.remaining)*1000000.0/$realDelta; interior=$interior}
        }
    })
    $nativeActive = Select-String -LiteralPath (Join-Path $run 'stdout.log') -SimpleMatch '[fu-native60] RESEARCH ONLY:' -Quiet
    if ($BaselineNavigation -and $Mode -eq 'native60') {
        $nativeActive = $nativeActive -and (Select-String -LiteralPath (Join-Path $run 'stdout.log') -SimpleMatch '[fu-native60] DIAGNOSTIC ONLY: native hooks activated at active quest countdown' -Quiet)
    }
    $messageSamples = @(Select-String -LiteralPath (Join-Path $run 'stdout.log') -Pattern '\[fu-message\]' | ForEach-Object {
        if ($_.Line -match 'object=(0x[0-9a-f]+) slot=\d+ active=(\d+) kind=(\d+) phase=(\d+) substate=(\d+) remaining=(\d+) .*pause=(\d+) calls=(\d+) virtual_us=(\d+) real_us=(\d+) paced=1') {
            [pscustomobject]@{object=$Matches[1]; active=[int]$Matches[2]; kind=[int]$Matches[3]; phase=[int]$Matches[4]; substate=[int]$Matches[5]; remaining=[int]$Matches[6]; pause=[int]$Matches[7]; calls=[long]$Matches[8]; virtual_us=[long]$Matches[9]; real_us=[long]$Matches[10]}
        }
    })
    # Each hold is paired with its own subsequent slide-out transition, so a
    # reused slot cannot produce a duration spanning separate announcements.
    $messageHolds = @()
    $pendingHolds = @{}
    foreach ($sample in $messageSamples) {
        if ($sample.active -eq 1 -and $sample.phase -eq 1 -and $sample.substate -eq 1) {
            if (-not $pendingHolds.ContainsKey($sample.object)) {
                $pendingHolds[$sample.object] = @{start=$sample; pause_start=$null; paused_calls=0L; paused_guest=0L; paused_real=0L; paused_samples=0; pause_counter_changed=$false}
            }
            $hold = $pendingHolds[$sample.object]
            if ($sample.pause -ne 0) {
                $hold.paused_samples++
                if ($null -eq $hold.pause_start) { $hold.pause_start = $sample }
                elseif ($sample.remaining -ne $hold.pause_start.remaining) { $hold.pause_counter_changed = $true }
            } elseif ($null -ne $hold.pause_start) {
                $hold.paused_calls += $sample.calls - $hold.pause_start.calls
                $hold.paused_guest += $sample.virtual_us - $hold.pause_start.virtual_us
                $hold.paused_real += $sample.real_us - $hold.pause_start.real_us
                $hold.pause_start = $null
            }
        } elseif ($pendingHolds.ContainsKey($sample.object)) {
            $hold = $pendingHolds[$sample.object]
            $start = $hold.start
            if ($sample.active -eq 1 -and $sample.phase -eq 1 -and $sample.substate -eq 2 -and $sample.kind -eq $start.kind) {
                $messageHolds += [ordered]@{object=$sample.object; kind=$sample.kind; countdown_ticks=$start.remaining-$sample.remaining;
                    root_update_calls=$sample.calls-$start.calls; update_calls=$sample.calls-$start.calls-$hold.paused_calls;
                    guest_seconds=($sample.virtual_us-$start.virtual_us)/1000000.0;
                    real_seconds=($sample.real_us-$start.real_us)/1000000.0;
                    paused_samples=$hold.paused_samples; pause_counter_changed=$hold.pause_counter_changed;
                    paused_guest_seconds=$hold.paused_guest/1000000.0; paused_real_seconds=$hold.paused_real/1000000.0;
                    active_guest_seconds=($sample.virtual_us-$start.virtual_us-$hold.paused_guest)/1000000.0;
                    active_real_seconds=($sample.real_us-$start.real_us-$hold.paused_real)/1000000.0}
            }
            $pendingHolds.Remove($sample.object)
        }
    }
    $observed = $clockStarted -and $measurements.Count -gt 0
    $modeMatches = $nativeActive -eq ($Mode -eq 'native60')
    [ordered]@{mode=$Mode; baseline_navigation=[bool]$BaselineNavigation; native_hooks_active=$nativeActive; completed=$completed; exit_code=$process.ExitCode; elapsed_seconds=$timer.Elapsed.TotalSeconds; save_files_unchanged=$unchanged; clock_observed=$observed; paced_samples=$samples.Count; measurements=$measurements; message_holds=$messageHolds; data=$data} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $run 'result.json')
    if (-not $unchanged -or (-not $Explore -and (-not $modeMatches -or -not $observed -or -not $completed -or $process.ExitCode -ne 4))) { throw "Quest-clock run incomplete: inspect $run" }
} finally {
    if ($null -ne $process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(MHFU_|PSPRECOMP_)' } | ForEach-Object { Remove-Item -LiteralPath ('Env:' + $_.Name) }
    foreach ($name in $savedEnvironment.Keys) { Set-Item -LiteralPath ('Env:' + $name) -Value $savedEnvironment[$name] }
}

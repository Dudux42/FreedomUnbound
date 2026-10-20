# Windows release workflow. See BUILDING.md for prerequisites and stages.
[CmdletBinding()]
param(
    [ValidateSet('Patch','Configure','Bootstrap','Install','Generate','Overlays','Build','Test','Package','All')]
    [string]$Step = 'Build',
    [string]$BuildDirectory = 'build/windows',
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$Glslang = '',
    [string]$DownloadCache = '',
    [string]$Iso = '',
    [string[]]$OverlayDump = @(),
    [ValidateRange(1,32)][int]$Jobs = 2,
    [switch]$ActionTimingDiagnostics,
    [switch]$Native60Experiment,
    [switch]$AlphaCandidate,
    [switch]$TestData,
    [ValidatePattern('^\d+\.\d+\.\d+-alpha(\.\d+)?$')]
    [string]$ReleaseVersion = '',
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]*$')]
    [string]$PackageFolderName = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($ReleaseVersion -and -not $AlphaCandidate) {
    throw 'ReleaseVersion requires AlphaCandidate so research builds cannot be labeled as alpha releases.'
}
$workspace = Split-Path -Parent $PSScriptRoot
$framework = Join-Path $workspace 'portablekit'
# ULUS10391 BOOT.BIN: seven constructors embedded in .data, followed at the
# exclusive end by their pointer table. See INVESTIGATION.md.
$extraCodeRanges = @('0x089B93D8-0x089BAB70')
$build = [IO.Path]::GetFullPath((Join-Path $workspace $BuildDirectory))
$allowed = [IO.Path]::GetFullPath((Join-Path $workspace 'build')) + [IO.Path]::DirectorySeparatorChar
if (-not $build.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'BuildDirectory must be a subdirectory of build/; prototype directories are protected.'
}
New-Item -ItemType Directory -Path $build -Force | Out-Null
# All build directories share generated/, so serialize the workflow per checkout.
$buildLock = [IO.File]::Open((Join-Path $workspace 'build/.workflow.lock'), 'OpenOrCreate', 'ReadWrite', 'None')

function Invoke-Tool([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

function Check-Framework {
    $patch = Join-Path $workspace 'patches/portablekit-local.patch'
    if (-not (Test-Path -LiteralPath (Join-Path $framework '.git'))) {
        throw 'Initialize the submodule with git submodule update --init --recursive.'
    }
    $revision = & git -C $framework rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read PortableKit revision.' }
    if ($revision -ne '8b0c3fedd1d908dc2d171f8257c5c3d9af8886aa') {
        throw 'This patch targets PortableKit 8b0c3fedd1d908dc2d171f8257c5c3d9af8886aa; do not update the submodule implicitly.'
    }
    & git -C $framework apply --reverse --check $patch 2>$null
    if ($LASTEXITCODE -eq 0) { Write-Host 'PortableKit patch is already present.'; return }
    if ($Step -notin @('Patch','All')) { throw 'Run this script with -Step Patch first.' }
    $changes = @(& git -C $framework status --porcelain)
    if ($LASTEXITCODE -ne 0 -or $changes.Count -ne 0) {
        throw 'PortableKit has local changes that do not match the patch. Resolve them before applying it.'
    }
    Invoke-Tool git @('-C', $framework, 'apply', '--check', $patch)
    Invoke-Tool git @('-C', $framework, 'apply', $patch)
    Invoke-Tool git @('-C', $framework, 'apply', '--reverse', '--check', $patch)
}

function Import-CompilerEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 C++ Build Tools, or run from its x64 developer shell.' }
    $installation = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installation) { throw 'Visual Studio 2022 x64 C++ tools were not found.' }
    $developer = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
    # cmd is used only to obtain the compiler environment, never for file operations.
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $env:ComSpec
    $start.Arguments = '/d /s /c ""' + $developer + '" -no_logo -arch=x64 -host_arch=x64 >nul && set"'
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $process = [Diagnostics.Process]::Start($start)
    $environment = $process.StandardOutput.ReadToEnd() -split '\r?\n'
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    $process.Dispose()
    if ($exitCode -ne 0) { throw 'Cannot initialize the MSVC environment.' }
    foreach ($line in $environment) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
}

function Configure-Build([bool]$WithoutCorpus) {
    Import-CompilerEnvironment
    if (-not $VcpkgRoot -or -not (Test-Path -LiteralPath (Join-Path $VcpkgRoot 'scripts/buildsystems/vcpkg.cmake'))) {
        throw 'Set VCPKG_ROOT or pass -VcpkgRoot. See BUILDING.md for required packages.'
    }
    $shaderTool = $Glslang
    if (-not $shaderTool) {
        $command = Get-Command glslangValidator.exe -ErrorAction SilentlyContinue
        if ($command) { $shaderTool = $command.Source }
        else { $shaderTool = Join-Path $VcpkgRoot 'installed/x64-windows/tools/glslang/glslangValidator.exe' }
    }
    if (-not (Test-Path -LiteralPath $shaderTool -PathType Leaf)) { throw 'Pass -Glslang with the path to glslangValidator.exe.' }
    $corpus = if ($WithoutCorpus) { 'ON' } else { 'OFF' }
    $options = @('-S', $workspace, '-B', $build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake",
        '-DVCPKG_TARGET_TRIPLET=x64-windows', "-DPORTABLEKIT_GLSLANG=$shaderTool",
        '-DPORTABLEKIT_RELEASE=ON', '-DPORTABLEKIT_RENDERER=ON', '-DMHFU_REQUIRE_RENDERER=ON',
        '-DPORTABLEKIT_FFMPEG=bundled', '-DPSPRECOMP_BUILD_TESTS=ON',
        '-DPSPRECOMP_GENERATED_OPT_LEVEL=2', "-DPSPRECOMP_GENERATED_JOBS=$Jobs",
        '-DPSPRECOMP_MSVC_MP_JOBS=1', "-DMHFU_NO_CORPUS=$corpus",
        "-DMHFU_ACTION_TIMING_DIAGNOSTICS=$($ActionTimingDiagnostics.IsPresent)",
        "-DMHFU_NATIVE_60_EXPERIMENT=$($Native60Experiment.IsPresent)", "-DMHFU_TEST_DATA=$($TestData.IsPresent)")
    if ($ReleaseVersion) { $options += "-DMHFU_VERSION=$ReleaseVersion" }
    if ($DownloadCache) { $options += "-DPORTABLEKIT_FFMPEG_DOWNLOAD_DIR=$DownloadCache" }
    Invoke-Tool cmake $options
}

function Install-Game {
    if (-not $Iso -or -not (Test-Path -LiteralPath $Iso -PathType Leaf)) { throw 'Install needs -Iso pointing to your supported USA disc image.' }
    $oldData = $env:MHFU_DATA_DIR
    try {
        $env:MHFU_DATA_DIR = Join-Path $build 'game'
        Invoke-Tool (Join-Path $build 'bin/FreedomUniteNative.exe') @('--install', (Resolve-Path -LiteralPath $Iso).Path, '--in-place')
    } finally { $env:MHFU_DATA_DIR = $oldData }
}

function Generate-Code {
    $executable = Join-Path $build 'game/EBOOT.ELF'
    if (-not (Test-Path -LiteralPath $executable)) { throw 'Run Bootstrap and Install before Generate.' }
    $profile = Get-Content -LiteralPath (Join-Path $workspace 'host/freedom_unite_profile.cpp') -Raw
    if ($profile -notmatch '\.executable_sha256\s*=\s*"([0-9a-f]{64})"') { throw 'The profile has no executable hash.' }
    if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne $Matches[1]) { throw 'Executable hash does not match the supported profile.' }
    $arguments = @($executable, '--auto', (Join-Path $workspace 'generated'))
    foreach ($range in $extraCodeRanges) { $arguments += @('--code', $range) }
    Invoke-Tool (Join-Path $build 'portablekit/psp_recomp.exe') $arguments
    [ordered]@{
        executable_sha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
        extra_code_ranges=$extraCodeRanges
        registry_sha256=(Get-FileHash -LiteralPath (Join-Path $workspace 'generated/generated_registry.cpp') -Algorithm SHA256).Hash
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $workspace 'generated/mhfu-codegen.json') -Encoding utf8
}

function Generate-Overlays {
    if ($OverlayDump.Count -eq 0) { throw 'Overlays needs -OverlayDump with one or more local MWo3 dumps.' }
    $executable = Join-Path $build 'game/EBOOT.ELF'
    $profile = Get-Content -LiteralPath (Join-Path $workspace 'host/freedom_unite_profile.cpp') -Raw
    if ($profile -notmatch '\.executable_sha256\s*=\s*"([0-9a-f]{64})"' -or
        -not (Test-Path -LiteralPath $executable) -or
        (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne $Matches[1]) {
        throw 'Overlays requires the supported executable in this build installation.'
    }
    # Validate every input before generating any corpus.
    $dumps = @($OverlayDump | ForEach-Object {
        $path = (Resolve-Path -LiteralPath $_).Path
        $bytes = [IO.File]::ReadAllBytes($path)
        if ($bytes.Length -lt 64 -or [Text.Encoding]::ASCII.GetString($bytes,0,4) -ne 'MWo3') {
            throw "Not a MWo3 overlay: $path"
        }
        $base = [BitConverter]::ToUInt32($bytes,8)
        $imageSize = 64L + [BitConverter]::ToUInt32($bytes,12) + [BitConverter]::ToUInt32($bytes,16)
        if ($imageSize -ne $bytes.Length -or $base % 4 -ne 0 -or
            $base -lt 0x08800000 -or ([long]$base + $imageSize) -gt 0x0A000000) {
            throw "Overlay size or address is invalid: $path"
        }
        [ordered]@{path=$path; base=('0x{0:X8}' -f $base)}
    })
    foreach ($dump in $dumps) {
        Invoke-Tool python @((Join-Path $framework 'tools/add_overlay.py'), '--no-build',
            '--profile-dir', $workspace, $build, $dump.path, $dump.base)
    }
}

function Get-OverlayLibraries {
    $directory = Join-Path $workspace 'overlays'
    if (-not (Test-Path -LiteralPath $directory)) { return }
    foreach ($corpus in Get-ChildItem -LiteralPath $directory -Directory | Sort-Object Name) {
        if (-not (Test-Path -LiteralPath (Join-Path $corpus.FullName 'meta.txt'))) { continue }
        $file = 'overlays/' + $corpus.Name + '.dll'
        $library = Join-Path $build ('bin/' + $file)
        if (-not (Test-Path -LiteralPath $library)) { throw "Missing overlay library: $file" }
        [ordered]@{file=$file; sha256=(Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash}
    }
}

function Build-Game {
    if (-not (Test-Path -LiteralPath (Join-Path $workspace 'generated/generated_registry.cpp'))) {
        throw 'Generate the game corpus first; Build must not silently produce an interpreter-only application.'
    }
    $profile = Get-Content -LiteralPath (Join-Path $workspace 'host/freedom_unite_profile.cpp') -Raw
    if ($profile -notmatch '\.executable_sha256\s*=\s*"([0-9a-f]{64})"') { throw 'The profile has no executable hash.' }
    $expectedHash = $Matches[1]
    $registry = Get-Content -LiteralPath (Join-Path $workspace 'generated/generated_registry.cpp') -Raw
    if ($registry -notmatch 'generated_corpus_source_sha256\(\)\s*\{\s*return "([0-9a-f]{64})"' -or $Matches[1] -ne $expectedHash) {
        throw 'Generated corpus does not match the supported executable. Run Generate first.'
    }
    $generationRecord = Join-Path $workspace 'generated/mhfu-codegen.json'
    if (-not (Test-Path -LiteralPath $generationRecord)) { throw 'Run Generate to include the embedded startup code.' }
    $generation = Get-Content -LiteralPath $generationRecord -Raw | ConvertFrom-Json
    if ($generation.executable_sha256 -ne $expectedHash -or
        ($generation.extra_code_ranges -join ',') -ne ($extraCodeRanges -join ',') -or
        $generation.registry_sha256 -ne (Get-FileHash -LiteralPath (Join-Path $workspace 'generated/generated_registry.cpp') -Algorithm SHA256).Hash) {
        throw 'Generated corpus metadata is stale. Run Generate before Build.'
    }
    Configure-Build $false
    Invoke-Tool cmake @('--build', $build, '--parallel', "$Jobs")
    $sourceFiles = @('CMakeLists.txt','host/app_icon.rc.in','patches/portablekit-local.patch','assets/launcher/modern-landscape.png','assets/branding/freedom-unbound-logo-v2.png','assets/branding/freedom-unbound-icon.ico')
    $sourceFiles += @(Get-ChildItem -LiteralPath (Join-Path $workspace 'host'),(Join-Path $workspace 'scripts'),(Join-Path $workspace 'tests') -File -Recurse |
        Where-Object { $_.Extension -in @('.cpp','.hpp','.ps1','.py') } |
        ForEach-Object { $_.FullName.Substring($workspace.Length + 1).Replace('\','/') })
    $sourceFiles = @($sourceFiles | Sort-Object -Unique)
    $sourceHashes = @($sourceFiles | ForEach-Object {
        [ordered]@{file=$_; sha256=(Get-FileHash -LiteralPath (Join-Path $workspace $_) -Algorithm SHA256).Hash}
    })
    $state = [ordered]@{
        created_utc=[DateTime]::UtcNow.ToString('o')
        parent_revision=(& git -C $workspace rev-parse HEAD)
        portablekit_revision=(& git -C $framework rev-parse HEAD)
        corpus_source_sha256=$expectedHash
        extra_code_ranges=$extraCodeRanges
        overlay_libraries=@(Get-OverlayLibraries)
        generated_registry_sha256=(Get-FileHash -LiteralPath (Join-Path $workspace 'generated/generated_registry.cpp') -Algorithm SHA256).Hash
        executable_sha256=(Get-FileHash -LiteralPath (Join-Path $build 'bin/FreedomUniteNative.exe') -Algorithm SHA256).Hash
        launcher_sha256=(Get-FileHash -LiteralPath (Join-Path $build 'bin/FreedomUnbound.exe') -Algorithm SHA256).Hash
        source_files=$sourceHashes
        action_timing_diagnostics=$ActionTimingDiagnostics.IsPresent
        native_game_frame_rate=30
        native_cheats_hook_version=7
        native_60_research_hooks=$Native60Experiment.IsPresent
        configuration='Windows x64; Ninja; Release; renderer ON; bundled FFmpeg; generated optimization 2; interpreter fallback available'
        msvc_tools_version=$env:VCToolsVersion
    }
    $state | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $build 'build-info.json') -Encoding utf8
}

function Package-Game {
    $cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Raw
    if ($cache -notmatch '(?m)^MHFU_NO_CORPUS:BOOL=OFF\r?$' -or -not (Test-Path -LiteralPath (Join-Path $build 'build-info.json'))) {
        throw 'Package requires the completed native Build stage, not a bootstrap installer.'
    }
    $bin = Join-Path $build 'bin'
    $buildInfo = Get-Content -LiteralPath (Join-Path $build 'build-info.json') -Raw | ConvertFrom-Json
    if ($AlphaCandidate -and (
        ($buildInfo.PSObject.Properties['native_60_research_hooks'] -and $buildInfo.native_60_research_hooks) -or
        ($buildInfo.PSObject.Properties['action_timing_diagnostics'] -and $buildInfo.action_timing_diagnostics))) {
        throw 'Alpha candidates require a standard Build without native 60 FPS hooks or action timing diagnostics.'
    }
    if ((Get-FileHash -LiteralPath (Join-Path $bin 'FreedomUniteNative.exe') -Algorithm SHA256).Hash -ne $buildInfo.executable_sha256) {
        throw 'The executable no longer matches build-info.json. Run Build before Package.'
    }
    if ($AlphaCandidate -and (
        -not $buildInfo.PSObject.Properties['launcher_sha256'] -or
        -not (Test-Path -LiteralPath (Join-Path $bin 'FreedomUnbound.exe')) -or
        (Get-FileHash -LiteralPath (Join-Path $bin 'FreedomUnbound.exe') -Algorithm SHA256).Hash -ne $buildInfo.launcher_sha256)) {
        throw 'The dedicated launcher is missing or changed. Run Build before alpha packaging.'
    }
    foreach ($source in $buildInfo.source_files) {
        if (-not (Test-Path -LiteralPath (Join-Path $workspace $source.file)) -or
            (Get-FileHash -LiteralPath (Join-Path $workspace $source.file) -Algorithm SHA256).Hash -ne $source.sha256) {
            throw 'Project sources changed after Build. Rebuild before packaging.'
        }
    }
    $overlays = @(Get-OverlayLibraries)
    if ($buildInfo.PSObject.Properties.Name -notcontains 'overlay_libraries') {
        throw 'Build metadata predates overlay packaging. Run Build before Package.'
    }
    if ((($overlays | ForEach-Object { $_.file }) -join ',') -ne
        (($buildInfo.overlay_libraries | ForEach-Object { $_.file }) -join ',')) {
        throw 'Overlay library set changed. Run Build before Package.'
    }
    foreach ($overlay in $buildInfo.overlay_libraries) {
        if ((Get-FileHash -LiteralPath (Join-Path $bin $overlay.file) -Algorithm SHA256).Hash -ne $overlay.sha256) {
            throw 'An overlay library changed. Run Build before Package.'
        }
    }
    $files = @('FreedomUniteNative.exe','SDL3.dll','vulkan-1.dll','avcodec-61.dll','avutil-59.dll','swresample-5.dll','FFmpeg-LICENSE.txt','FFmpeg-SOURCE.txt')
    if ($AlphaCandidate) { $files += 'FreedomUnbound.exe' }
    foreach ($file in $files) { if (-not (Test-Path -LiteralPath (Join-Path $bin $file))) { throw "Package needs $file; finish Build first." } }
    $dependencyNotices = @()
    if ($AlphaCandidate) {
        $installedMatch = [regex]::Match($cache, '(?m)^VCPKG_INSTALLED_DIR:PATH=(.+)\r?$')
        $tripletMatch = [regex]::Match($cache, '(?m)^VCPKG_TARGET_TRIPLET:STRING=(.+)\r?$')
        if (-not $installedMatch.Success -or -not $tripletMatch.Success) {
            throw 'Alpha packaging needs vcpkg dependency paths to include library license notices.'
        }
        $share = Join-Path $installedMatch.Groups[1].Value.Trim() ($tripletMatch.Groups[1].Value.Trim() + '/share')
        foreach ($library in @('sdl3','vulkan-loader')) {
            $notice = Join-Path $share ($library + '/copyright')
            if (-not (Test-Path -LiteralPath $notice)) { throw "Missing dependency notice: $library" }
            $dependencyNotices += @{source=$notice; destination=($library + '-LICENSE.txt')}
        }
        foreach ($item in @(
            @{source='third_party/imgui/LICENSE.txt'; destination='DearImGui-LICENSE.txt'},
            @{source='third_party/tiny_aes/UNLICENSE'; destination='tiny-AES-UNLICENSE.txt'},
            @{source='third_party/xxhash/LICENSE'; destination='xxHash-LICENSE.txt'})) {
            $notice = Join-Path $framework $item.source
            if (-not (Test-Path -LiteralPath $notice)) { throw "Missing compiled dependency notice: $($item.source)" }
            $dependencyNotices += @{source=$notice; destination=$item.destination}
        }
    }
    if ($AlphaCandidate) { $dependencyNotices += @{source=(Join-Path $workspace 'third_party/miniz/LICENSE'); destination='miniz-LICENSE.txt'} }
    $packagePrefix = if ($buildInfo.PSObject.Properties['native_60_research_hooks'] -and $buildInfo.native_60_research_hooks) { 'FreedomUnite-Native60Research-' } else { 'FreedomUnite-' }
    if ($AlphaCandidate) { $packagePrefix = 'FreedomUnbound-AlphaCandidate-' }
    $packageName = if ($ReleaseVersion) { 'FreedomUnbound-' + $ReleaseVersion + '-windows-x64' } else { $packagePrefix + (Get-Date -Format 'yyyyMMdd-HHmmss') }
    if ($PackageFolderName) { $packageName = $PackageFolderName }
    $package = Join-Path $build ('packages/' + $packageName)
    if (Test-Path -LiteralPath $package) { throw 'Package already exists; refusing to overwrite it.' }
    New-Item -ItemType Directory -Path $package -Force | Out-Null
    foreach ($file in $files) { Copy-Item -LiteralPath (Join-Path $bin $file) -Destination $package }
    if ($overlays.Count -gt 0) {
        New-Item -ItemType Directory -Path (Join-Path $package 'overlays') | Out-Null
        foreach ($overlay in $overlays) { Copy-Item -LiteralPath (Join-Path $bin $overlay.file) -Destination (Join-Path $package $overlay.file) }
    }
    Copy-Item -LiteralPath (Join-Path $framework 'LICENSE') -Destination (Join-Path $package 'PortableKit-LICENSE.txt')
    foreach ($notice in $dependencyNotices) {
        Copy-Item -LiteralPath $notice.source -Destination (Join-Path $package $notice.destination)
    }
    if ($AlphaCandidate) {
        foreach ($headerName in @('stb_image','stb_image_write','stb_truetype')) {
            $header = Get-Content -LiteralPath (Join-Path $framework ('third_party/' + $headerName + '.h')) -Raw
            $begin = $header.LastIndexOf('ALTERNATIVE A - MIT License')
            $end = if ($begin -ge 0) { $header.IndexOf('*/', $begin) } else { -1 }
            if ($end -lt 0) { throw "Cannot locate the original license notice in $headerName" }
            $header.Substring($begin, $end - $begin).Trim() | Set-Content -LiteralPath (Join-Path $package ($headerName + '-LICENSE.txt')) -Encoding utf8
        }
    }
    Copy-Item -LiteralPath (Join-Path $build 'build-info.json') -Destination $package
    Copy-Item -LiteralPath (Join-Path $workspace 'CHEATS.md') -Destination $package
    Copy-Item -LiteralPath (Join-Path $workspace 'MODDING.md') -Destination $package
    foreach ($guide in @('MOD_CATALOG.md','INGAME_MOD_MANAGER.md','LAN_MULTIPLAYER.md','DISCLAIMERS.md')) {
        Copy-Item -LiteralPath (Join-Path $workspace $guide) -Destination $package
    }
    Copy-Item -LiteralPath (Join-Path $workspace 'WHATSNEW.md') -Destination $package
    Copy-Item -LiteralPath (Join-Path $workspace 'TEST_BUILD_CHANGES.md') -Destination $package
    # Bundled launcher/host translations: <program folder>\lang\<language>\host.txt (host/fu_host_text.cpp).
    $langSource = Join-Path $workspace 'lang'
    if (Test-Path -LiteralPath $langSource -PathType Container) { Copy-Item -LiteralPath $langSource -Destination (Join-Path $package 'lang') -Recurse }
    if ($AlphaCandidate) {
        Copy-Item -LiteralPath (Join-Path $workspace 'scripts/start-alpha.ps1') -Destination (Join-Path $package 'Start-Freedom-Unbound.ps1')
        Copy-Item -LiteralPath (Join-Path $workspace 'ALPHA_PLAYTEST.md') -Destination $package
        if ($ReleaseVersion) {
            Copy-Item -LiteralPath (Join-Path $workspace 'RELEASE_NOTES.md') -Destination $package
        }
        @'
@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Freedom-Unbound.ps1"
if errorlevel 1 pause
endlocal
'@ | Set-Content -LiteralPath (Join-Path $package 'Start-Freedom-Unbound.cmd') -Encoding ascii
    }
    @'
Freedom Unbound Windows build

Keep this folder together: the executable requires the adjacent DLLs.
First launch opens setup; supply your own supported USA ISO (ULUS10391).
Requirements: Windows x64, Vulkan graphics support, AVX-capable CPU and the
Microsoft Visual C++ x64 runtime (this build uses MSVC's shared runtime).
Esc or L3+R3 opens the in-game menu. Use its controls page for bindings.
The tested gameplay paths run with interpreter fallback disabled.
The preserved earlier prototype's gameplay loop was verified by its owner;
full campaign coverage and multiplayer have not been verified.
Native gameplay is 30 FPS. Higher renderer rates are interpolation.
Read the bundled tester guide for acceptance coverage and known issues.

Alpha 0.1.5 adds the Fill aspect ratio, the F7 Equipment Wishlist panel while
its mod is enabled and translated Saves screen messages. F9 opens the in-game
mod manager; F10 toggles an installed texture pack and does nothing without one.
LAN Host/Join is available before boot.
Read RELEASE_NOTES.md, MOD_CATALOG.md, INGAME_MOD_MANAGER.md and LAN_MULTIPLAYER.md.
Mod packs and the standalone save editor are separate, not bundled.
Read DISCLAIMERS.md for AI-use disclosure and legal/IP information.
'@ | Set-Content -LiteralPath (Join-Path $package 'README.txt') -Encoding utf8
    Get-Content -LiteralPath (Join-Path $workspace 'DISCLAIMERS.md') -Raw | Add-Content -LiteralPath (Join-Path $package 'README.txt') -Encoding utf8
    if ($AlphaCandidate) {
        @'

ALPHA: start FreedomUnbound.exe. The .cmd shortcut remains available as a fallback.
Read ALPHA_PLAYTEST.md for setup, coverage and reporting instructions.
Session logs: %LOCALAPPDATA%\FreedomUnbound\logs.
Native 60 FPS hooks and action timing diagnostics are compiled out.
See the release notes or ALPHA_RELEASE.md for recorded acceptance coverage.
'@ | Add-Content -LiteralPath (Join-Path $package 'README.txt') -Encoding utf8
        if ($ReleaseVersion) {
            "`r`nRelease version: $ReleaseVersion`r`n60 FPS interpolation is experimental; the house background can distort.`r`nUse 30 FPS for stable reference gameplay. Native 60 FPS is not included.`r`n" | Add-Content -LiteralPath (Join-Path $package 'README.txt') -Encoding utf8
        }
    }
    if ($buildInfo.PSObject.Properties['native_60_research_hooks'] -and $buildInfo.native_60_research_hooks) {
        @'

RESEARCH BUILD: compiled with optional native 60 FPS main-image hooks.
They are disabled by default and additionally require MHFU_NATIVE_60_EXPERIMENT=1.
This mode is incomplete: movement, monsters, physics, overlays, UI and cutscene
timing are not validated. Use copied data only. This is not a supported 60 FPS release.
'@ | Add-Content -LiteralPath (Join-Path $package 'README.txt') -Encoding utf8
    }
    $manifest = @(Get-ChildItem -LiteralPath $package -File -Recurse | ForEach-Object {
        [ordered]@{file=$_.FullName.Substring($package.Length + 1).Replace('\','/'); bytes=$_.Length; sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'build-files.json') -Encoding utf8
    Write-Host "Package: $package"
}

function Run-Tests {
    $testTemp = Join-Path $build 'test-temp'
    New-Item -ItemType Directory -Path $testTemp -Force | Out-Null
    $oldTemp = $env:TEMP
    $oldTmp = $env:TMP
    # Playtest switches must not change interpreter/settings unit-test behavior.
    $testEnvironment = @{}
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(MHFU_|PSPRECOMP_)' } | ForEach-Object {
        $testEnvironment[$_.Name] = $_.Value
        Remove-Item -LiteralPath ('Env:' + $_.Name)
    }
    try {
        $env:TEMP = $testTemp
        $env:TMP = $testTemp
        Invoke-Tool ctest @('--test-dir', $build, '--output-on-failure', '--timeout', '60')
    } finally {
        $env:TEMP = $oldTemp
        $env:TMP = $oldTmp
        foreach ($name in $testEnvironment.Keys) {
            Set-Item -LiteralPath ('Env:' + $name) -Value $testEnvironment[$name]
        }
    }
}

try {
    Push-Location $workspace
    try {
        Check-Framework
        switch ($Step) {
            'Patch' { }
            'Configure' { Configure-Build $false }
            'Bootstrap' { Configure-Build $true; Invoke-Tool cmake @('--build', $build, '--target', 'FreedomUniteNative', 'psp_recomp', '--parallel', "$Jobs") }
            'Install' { Install-Game }
            'Generate' { Generate-Code }
            'Overlays' { Generate-Overlays; Build-Game }
            'Build' { Build-Game }
            'Test' { Run-Tests }
            'Package' { Package-Game }
            'All' {
                Configure-Build $true
                Invoke-Tool cmake @('--build', $build, '--target', 'FreedomUniteNative', 'psp_recomp', '--parallel', "$Jobs")
                Install-Game
                Generate-Code
                if ($OverlayDump.Count -gt 0) { Generate-Overlays }
                Build-Game
                Run-Tests
                Package-Game
            }
        }
    } finally { Pop-Location }
} finally { $buildLock.Dispose() }

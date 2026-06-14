<#
.SYNOPSIS
Generates CMake build files for Windows Visual Studio and Unix/macOS generators.

.DESCRIPTION
This script configures out-of-source CMake build directories for:
- Visual Studio 2022
- Visual Studio 2026 (when supported by installed CMake)
- Linux Unix Makefiles
- macOS Xcode
- Ninja (when installed and Target includes unix/all)

It auto-detects VCPKG_ROOT and wires in the vcpkg toolchain file unless
-DisableVcpkg is specified.

.PARAMETER SourceDir
Path to the CMake source directory containing CMakeLists.txt.
Default: .

.PARAMETER BuildRoot
Base directory where generator-specific build folders are created.
Default: build

.PARAMETER Target
Controls which platform group to generate.
- all: Generate all configurations relevant to current host OS.
- windows: Generate Visual Studio configurations (Windows hosts only).
- unix: Generate Linux/macOS/Ninja configurations (Linux/macOS hosts for native generators; Ninja on any host if available).

.PARAMETER SkipVs2026
Skips attempting the Visual Studio 2026 generator.

.PARAMETER SkipVs2022
Skips generating the Visual Studio 2022 generator.

.PARAMETER DisableVcpkg
Disables automatic vcpkg toolchain integration from VCPKG_ROOT.

.PARAMETER WindowsSdkVersion
Optional explicit Windows SDK version passed to CMake as:
-DCMAKE_SYSTEM_VERSION=<value>
Useful to avoid SDK auto-discovery issues in restricted environments.

.PARAMETER ExtraCMakeArgs
Additional arguments forwarded directly to each CMake generation call.

.PARAMETER NoFresh
Disables automatic use of CMake --fresh when reconfiguring existing build
directories.

.EXAMPLE
./generate-build-files.ps1
Generates all applicable configurations for the current host using vcpkg when available.

.EXAMPLE
./generate-build-files.ps1 -Target windows -SkipVs2026
Generates only Visual Studio 2022 files on Windows.

.EXAMPLE
./generate-build-files.ps1 -Target windows -SkipVs2022
Generates only Visual Studio 2026 files on Windows.

.EXAMPLE
./generate-build-files.ps1 -Target windows -WindowsSdkVersion 10.0.26100.0
Generates Windows configs and forces a specific SDK version.

.EXAMPLE
./generate-build-files.ps1 -Target unix -ExtraCMakeArgs @("-DCMAKE_BUILD_TYPE=Release")
Generates Unix/macOS/Ninja configs and forwards additional CMake options.

.EXAMPLE
./generate-build-files.ps1 -Install
Generates, builds Release, and installs to ./dist.

.EXAMPLE
./generate-build-files.ps1 -Install -InstallPrefix C:\MyApp\ddsviewer -InstallConfig Debug
Generates, builds Debug, and installs to a custom prefix.

.NOTES
Requires CMake in PATH.
Visual Studio 2026 generation requires a CMake version that supports
the "Visual Studio 18 2026" generator.
#>
param(
    [string]$SourceDir = ".",
    [string]$BuildRoot = "build",
    [ValidateSet("all", "windows", "unix")]
    [string]$Target = "all",
    [switch]$SkipVs2022,
    [switch]$SkipVs2026,
    [switch]$DisableVcpkg,
    [switch]$NoFresh,
    [string]$WindowsSdkVersion,
    [string[]]$ExtraCMakeArgs = @(),
    [switch]$Install,
    [string]$InstallPrefix = "dist",
    [string]$InstallConfig = "Release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:GeneratedDirs = [System.Collections.Generic.List[hashtable]]::new()

function Invoke-CMakeGenerate {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Generator,
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,
        [string]$Architecture,
        [string[]]$AdditionalArgs = @(),
        [bool]$MultiConfig = $false
    )

    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

    $script:GeneratedDirs.Add(@{ Path = $BuildDir; MultiConfig = $MultiConfig })

    $args = @(
        "-S", $SourceDir,
        "-B", $BuildDir,
        "-G", $Generator
    )

    # Single-config generators need build type baked in at configure time
    if ($Install -and -not $MultiConfig) {
        $args += "-DCMAKE_BUILD_TYPE=$InstallConfig"
    }

    if ((-not $NoFresh) -and (Test-Path -Path (Join-Path $BuildDir "CMakeCache.txt") -PathType Leaf)) {
        $args += "--fresh"
    }

    if ($Architecture) {
        $args += @("-A", $Architecture)
    }

    if ($AdditionalArgs.Count -gt 0) {
        $args += $AdditionalArgs
    }

    if ($ExtraCMakeArgs.Count -gt 0) {
        $args += $ExtraCMakeArgs
    }

    Write-Host ""
    Write-Host "Generating: $Generator -> $BuildDir"
    & cmake @args
    if ($LASTEXITCODE -ne 0) {
        throw "CMake generation failed for generator '$Generator' (exit code $LASTEXITCODE)."
    }
}

$sourceFullPath = Resolve-Path -Path $SourceDir -ErrorAction Stop
if (-not (Test-Path -Path (Join-Path $sourceFullPath "CMakeLists.txt") -PathType Leaf)) {
    $fallbackSourceDir = ".worktrees/implement"
    $fallbackSourcePath = Join-Path (Get-Location) $fallbackSourceDir
    if (Test-Path -Path (Join-Path $fallbackSourcePath "CMakeLists.txt") -PathType Leaf) {
        Write-Warning "No CMakeLists.txt found at '$sourceFullPath'. Falling back to '$fallbackSourceDir'."
        $SourceDir = $fallbackSourceDir
        $sourceFullPath = Resolve-Path -Path $SourceDir -ErrorAction Stop
    }
    else {
        throw "No CMakeLists.txt found in source directory: $sourceFullPath"
    }
}

$buildRootFullPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $BuildRoot))
New-Item -ItemType Directory -Path $buildRootFullPath -Force | Out-Null

$runningOnWindows = $PSVersionTable.PSVersion.Major -lt 6 -or $IsWindows
$runningOnMacOS = $PSVersionTable.PSVersion.Major -ge 6 -and $IsMacOS
$runningOnLinux = $PSVersionTable.PSVersion.Major -ge 6 -and $IsLinux

if ($Target -in @("all", "windows") -and $runningOnWindows -and $SkipVs2022 -and $SkipVs2026) {
    throw "Both -SkipVs2022 and -SkipVs2026 were specified; no Visual Studio generators remain."
}

$vcpkgArgs = @()
if (-not $DisableVcpkg -and $env:VCPKG_ROOT) {
    $vcpkgRoot = [System.IO.Path]::GetFullPath($env:VCPKG_ROOT)
    $vcpkgToolchain = Join-Path $vcpkgRoot "scripts/buildsystems/vcpkg.cmake"
    $vcpkgExecutableName = if ($runningOnWindows) { "vcpkg.exe" } else { "vcpkg" }
    $vcpkgExe = Join-Path $vcpkgRoot $vcpkgExecutableName

    if (Test-Path -Path $vcpkgToolchain -PathType Leaf) {
        $vcpkgArgs += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain"
        $vcpkgArgs += "-DVCPKG_MANIFEST_MODE=ON"
        $vcpkgArgs += "-DVCPKG_MANIFEST_INSTALL=ON"
        $vcpkgArgs += "-DVCPKG_MANIFEST_DIR=$sourceFullPath"

        if (Test-Path -Path $vcpkgExe -PathType Leaf) {
            $vcpkgArgs += "-DVCPKG_EXECUTABLE=$vcpkgExe"
        }
        else {
            Write-Warning "vcpkg executable not found at '$vcpkgExe'. Dependency auto-install may fail."
        }

        if ($runningOnWindows) {
            $vcpkgArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows"
        }
        elseif ($runningOnLinux) {
            $vcpkgArgs += "-DVCPKG_TARGET_TRIPLET=x64-linux"
        }
        elseif ($runningOnMacOS) {
            $vcpkgArgs += "-DVCPKG_TARGET_TRIPLET=x64-osx"
        }
    }
}

$windowsArgs = @()
if ($WindowsSdkVersion) {
    $windowsArgs += "-DCMAKE_SYSTEM_VERSION=$WindowsSdkVersion"
}

if ($Target -in @("all", "windows") -and $runningOnWindows) {
    if (-not $SkipVs2022) {
        Invoke-CMakeGenerate `
            -Generator "Visual Studio 17 2022" `
            -BuildDir (Join-Path $buildRootFullPath "vs2022") `
            -Architecture "x64" `
            -AdditionalArgs ($vcpkgArgs + $windowsArgs) `
            -MultiConfig $true
    }

    if (-not $SkipVs2026) {
        try {
            # Visual Studio 2026 is expected to map to the next CMake VS generator.
            Invoke-CMakeGenerate `
                -Generator "Visual Studio 18 2026" `
                -BuildDir (Join-Path $buildRootFullPath "vs2026") `
                -Architecture "x64" `
                -AdditionalArgs ($vcpkgArgs + $windowsArgs) `
                -MultiConfig $true
        }
        catch {
            Write-Warning "VS 2026 generator is unavailable in this CMake version. Install a newer CMake or run with -SkipVs2026."
            Write-Warning $_
        }
    }
}

if ($Target -in @("all", "unix") -and ($runningOnLinux -or $runningOnMacOS)) {
    if ($runningOnLinux) {
        Invoke-CMakeGenerate `
            -Generator "Unix Makefiles" `
            -BuildDir (Join-Path $buildRootFullPath "linux-makefiles") `
            -AdditionalArgs $vcpkgArgs `
            -MultiConfig $false
    }

    if ($runningOnMacOS) {
        Invoke-CMakeGenerate `
            -Generator "Xcode" `
            -BuildDir (Join-Path $buildRootFullPath "macos-xcode") `
            -AdditionalArgs $vcpkgArgs `
            -MultiConfig $true
    }
}

if ($Target -in @("all", "unix")) {
    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        Invoke-CMakeGenerate `
            -Generator "Ninja" `
            -BuildDir (Join-Path $buildRootFullPath "ninja") `
            -AdditionalArgs $vcpkgArgs `
            -MultiConfig $false
    }
}

Write-Host ""
Write-Host "CMake generation complete."

if ($Install) {
    $installPrefixFull = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $InstallPrefix))
    Write-Host ""
    Write-Host "Installing to: $installPrefixFull  (config: $InstallConfig)"

    foreach ($entry in $script:GeneratedDirs) {
        $buildDir = $entry.Path
        $multiConfig = $entry.MultiConfig

        Write-Host ""
        Write-Host "Building: $buildDir"
        $buildArgs = @("--build", $buildDir)
        if ($multiConfig) { $buildArgs += @("--config", $InstallConfig) }
        & cmake @buildArgs
        if ($LASTEXITCODE -ne 0) { throw "Build failed for '$buildDir'." }

        Write-Host "Installing: $buildDir -> $installPrefixFull"
        $installArgs = @("--install", $buildDir, "--prefix", $installPrefixFull)
        if ($multiConfig) { $installArgs += @("--config", $InstallConfig) }
        & cmake @installArgs
        if ($LASTEXITCODE -ne 0) { throw "Install failed for '$buildDir'." }
    }

    Write-Host ""
    Write-Host "Install complete: $installPrefixFull"
}

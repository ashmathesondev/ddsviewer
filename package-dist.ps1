<#
.SYNOPSIS
Builds a current Release dist folder and packages it into a versioned zip file.

.DESCRIPTION
Configures and builds the Release CMake preset, installs it into the dist
directory, then creates dist-<version>.zip from the installed files. By default,
the version comes from the nearest reachable git tag matching vMAJOR.MINOR.PATCH,
which is the same source used by the CMake-generated application version.

.PARAMETER DistDir
Directory where the Release build is installed before packaging.
Default: dist

.PARAMETER OutputDir
Directory where the zip file is written.
Default: .

.PARAMETER ConfigurePreset
CMake configure preset used to refresh the Release build directory.
Default: release

.PARAMETER BuildPreset
CMake build preset used to build the Release artifacts.
Default: release

.PARAMETER BuildDir
CMake build directory installed into DistDir.
Default: cmake-build-release

.PARAMETER InstallConfig
CMake install configuration.
Default: Release

.PARAMETER Version
Optional explicit version override. Must match vMAJOR.MINOR.PATCH.

.PARAMETER NoBuild
Skips configure, build, and install, and zips the existing DistDir contents.

.PARAMETER NoClean
Skips cleaning DistDir before installing the new Release build.

.EXAMPLE
./package-dist.ps1
Builds and installs Release to ./dist, then creates ./dist-v0.2.0.zip when
v0.2.0 is the current git tag.

.EXAMPLE
./package-dist.ps1 -DistDir dist -OutputDir releases
Builds and installs Release to ./dist, then creates releases/dist-<version>.zip.
#>
param(
    [string]$DistDir = "dist",
    [string]$OutputDir = ".",
    [string]$ConfigurePreset = "release",
    [string]$BuildPreset = "release",
    [string]$BuildDir = "cmake-build-release",
    [string]$InstallConfig = "Release",
    [string]$Version,
    [switch]$NoBuild,
    [switch]$NoClean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-ReleaseVersion {
    if ($Version) {
        return $Version
    }

    $tag = (& git describe --tags --match "v[0-9]*.[0-9]*.[0-9]*" --abbrev=0 2>$null)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($tag)) {
        throw "No reachable semver git tag found. Create a tag like 'v0.2.0' before packaging."
    }

    return $tag.Trim()
}

function Invoke-CheckedCMake {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [Parameter(Mandatory = $true)]
        [string]$FailureMessage
    )

    & cmake @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FailureMessage (exit code $LASTEXITCODE)."
    }
}

function Clear-DistDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $sourceRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
    $distRoot = [System.IO.Path]::GetFullPath($Path)
    $comparison = [System.StringComparison]::OrdinalIgnoreCase
    $sourcePrefix = $sourceRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar

    if (-not $distRoot.StartsWith($sourcePrefix, $comparison)) {
        throw "Refusing to clean dist directory outside the repository: $distRoot"
    }

    if ((Test-Path -LiteralPath $distRoot -PathType Container)) {
        Get-ChildItem -LiteralPath $distRoot -Force | Remove-Item -Recurse -Force
    }
}

function Update-DistDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$InstallPath
    )

    Write-Host ""
    Write-Host "Configuring Release build: $ConfigurePreset"
    Invoke-CheckedCMake `
        -Arguments @("--preset", $ConfigurePreset) `
        -FailureMessage "CMake configure failed for preset '$ConfigurePreset'"

    Write-Host ""
    Write-Host "Building Release artifacts: $BuildPreset"
    Invoke-CheckedCMake `
        -Arguments @("--build", "--preset", $BuildPreset) `
        -FailureMessage "CMake build failed for preset '$BuildPreset'"

    if (-not $NoClean) {
        Write-Host ""
        Write-Host "Cleaning dist directory: $InstallPath"
        Clear-DistDirectory -Path $InstallPath
    }

    New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null

    Write-Host ""
    Write-Host "Installing Release build to: $InstallPath"
    Invoke-CheckedCMake `
        -Arguments @("--install", $BuildDir, "--prefix", $InstallPath, "--config", $InstallConfig) `
        -FailureMessage "CMake install failed for build directory '$BuildDir'"
}

$releaseVersion = Get-ReleaseVersion
if ($releaseVersion -notmatch "^v\d+\.\d+\.\d+$") {
    throw "Version '$releaseVersion' is not valid. Expected format: vMAJOR.MINOR.PATCH, for example v0.2.0."
}

$distFullPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $DistDir))

if (-not $NoBuild) {
    Update-DistDirectory -InstallPath $distFullPath
}

$distPath = Resolve-Path -Path $distFullPath -ErrorAction Stop
$distFullPath = $distPath.ProviderPath
$exePath = Join-Path $distFullPath "ddsviewer.exe"
if (-not (Test-Path -Path $exePath -PathType Leaf)) {
    throw "Expected executable not found: $exePath."
}

$distItems = Get-ChildItem -Path $distFullPath -Force
if ($distItems.Count -eq 0) {
    throw "Dist directory is empty: $distFullPath"
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$outputFullPath = [System.IO.Path]::GetFullPath((Join-Path $OutputDir "dist-$releaseVersion.zip"))

if (Test-Path -Path $outputFullPath -PathType Leaf) {
    Remove-Item -LiteralPath $outputFullPath -Force
}

Compress-Archive -Path (Join-Path $distFullPath "*") -DestinationPath $outputFullPath -Force

Write-Host "Packaged $distFullPath"
Write-Host "Version: $releaseVersion"
Write-Host "Output:  $outputFullPath"

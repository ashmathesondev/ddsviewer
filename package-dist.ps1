<#
.SYNOPSIS
Packages the dist folder into a versioned zip file.

.DESCRIPTION
Creates dist-<version>.zip from the contents of the dist directory. By default,
the version comes from the nearest reachable git tag matching vMAJOR.MINOR.PATCH,
which is the same source used by the CMake-generated application version.

.PARAMETER DistDir
Directory containing the installed release files.
Default: dist

.PARAMETER OutputDir
Directory where the zip file is written.
Default: .

.PARAMETER Version
Optional explicit version override. Must match vMAJOR.MINOR.PATCH.

.EXAMPLE
./package-dist.ps1
Creates ./dist-v0.2.0.zip from ./dist when v0.2.0 is the current git tag.

.EXAMPLE
./package-dist.ps1 -DistDir dist -OutputDir releases
Creates releases/dist-<version>.zip.
#>
param(
    [string]$DistDir = "dist",
    [string]$OutputDir = ".",
    [string]$Version
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

$releaseVersion = Get-ReleaseVersion
if ($releaseVersion -notmatch "^v\d+\.\d+\.\d+$") {
    throw "Version '$releaseVersion' is not valid. Expected format: vMAJOR.MINOR.PATCH, for example v0.2.0."
}

$distPath = Resolve-Path -Path $DistDir -ErrorAction Stop
$distFullPath = $distPath.ProviderPath
$exePath = Join-Path $distFullPath "ddsviewer.exe"
if (-not (Test-Path -Path $exePath -PathType Leaf)) {
    throw "Expected executable not found: $exePath. Run './generate-build-files.ps1 -Install' first."
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

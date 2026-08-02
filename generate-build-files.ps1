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
        [string]$Toolset,
        [string]$VCTargetsPath,
        [string]$VsInstallPath,
        [string]$MsvcToolsetVersion,
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

    if ($Toolset) {
        $args += @("-T", $Toolset)
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

    if ($VCTargetsPath) {
        Write-VisualStudioDirectoryBuildProps -BuildDir $BuildDir -VCTargetsPath $VCTargetsPath
        Update-VisualStudioProjectImports `
            -BuildDir $BuildDir `
            -VCTargetsPath $VCTargetsPath `
            -VsInstallPath $VsInstallPath `
            -MsvcToolsetVersion $MsvcToolsetVersion
    }
}

function Get-CMakeGeneratorNames {
    $capabilitiesJson = & cmake -E capabilities
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to query CMake capabilities (exit code $LASTEXITCODE)."
    }

    $capabilities = $capabilitiesJson | ConvertFrom-Json
    return @($capabilities.generators | ForEach-Object { $_.name })
}

function Get-VisualStudioInstances {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -Path $vswhere -PathType Leaf)) {
        return @()
    }

    $instancesJson = & $vswhere -all -products * -prerelease -format json
    if ($LASTEXITCODE -ne 0 -or -not $instancesJson) {
        return @()
    }

    return @($instancesJson | ConvertFrom-Json)
}

function Find-VisualStudioInstance {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Instances,
        [Parameter(Mandatory = $true)]
        [string]$MajorVersion
    )

    $matchingInstances = @($Instances |
        Where-Object {
            $_.catalog.productLine -eq "Dev$MajorVersion" -or
            $_.catalog.productLineVersion -eq $MajorVersion -or
            $_.installationVersion -like "$MajorVersion.*"
        })

    $completeInstance = $matchingInstances |
        Where-Object { $_.isComplete -and $_.isLaunchable } |
        Sort-Object { [version]$_.installationVersion } -Descending |
        Select-Object -First 1

    if ($completeInstance) {
        return $completeInstance
    }

    return $matchingInstances |
        Where-Object { Get-LatestMsvcToolsetVersion -VsInstallPath $_.installationPath } |
        Sort-Object { [version]$_.installationVersion } -Descending |
        Select-Object -First 1
}

function Get-CMakeGeneratorInstance {
    param(
        [Parameter(Mandatory = $true)]
        [object]$VisualStudioInstance
    )

    return $VisualStudioInstance.installationPath
}

function Get-LatestMsvcToolsetVersion {
    param(
        [Parameter(Mandatory = $true)]
        [string]$VsInstallPath,
        [string]$VersionPrefix
    )

    $msvcRoot = Join-Path $VsInstallPath "VC\Tools\MSVC"
    if (-not (Test-Path -Path $msvcRoot -PathType Container)) {
        return $null
    }

    $toolsets = @(Get-ChildItem -Path $msvcRoot -Directory |
        Where-Object {
            $_.Name -match '^\d+\.\d+\.\d+$' -and
            ((-not $VersionPrefix) -or $_.Name.StartsWith($VersionPrefix))
        } |
        Sort-Object { [version]$_.Name } -Descending)

    if ($toolsets.Count -eq 0) {
        return $null
    }

    return $toolsets[0].Name
}

function Get-VisualStudioVcTargetsPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$VsInstallPath,
        [Parameter(Mandatory = $true)]
        [string]$MajorVersion
    )

    $targetsPath = Join-Path $VsInstallPath "MSBuild\Microsoft\VC\v$($MajorVersion)0"
    if (Test-Path -Path (Join-Path $targetsPath "Microsoft.Cpp.targets") -PathType Leaf) {
        return $targetsPath
    }

    return $null
}

function Get-VisualStudioGlobalArgs {
    param(
        [Parameter(Mandatory = $true)]
        [string]$VsInstallPath,
        [Parameter(Mandatory = $true)]
        [string]$MsvcToolsetVersion
    )

    $vsInstallDir = $VsInstallPath.Replace('\', '/')
    $vcInstallDir = (Join-Path $VsInstallPath "VC").Replace('\', '/')
    $vcToolsInstallDir = (Join-Path $VsInstallPath "VC\Tools\MSVC\$MsvcToolsetVersion").Replace('\', '/')

    return "-DCMAKE_VS_GLOBALS=VSInstallDir=$vsInstallDir/;VCInstallDir=$vcInstallDir/;VCToolsInstallDir=$vcToolsInstallDir/;VCToolsVersion=$MsvcToolsetVersion"
}

function Write-VisualStudioDirectoryBuildProps {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,
        [Parameter(Mandatory = $true)]
        [string]$VCTargetsPath
    )

    $propsPath = Join-Path $BuildDir "Directory.Build.props"
    $escapedVCTargetsPath = [System.Security.SecurityElement]::Escape($VCTargetsPath)
    $propsContent = @"
<?xml version="1.0" encoding="utf-8"?>
<Project>
  <PropertyGroup>
    <VCTargetsPath>$escapedVCTargetsPath\</VCTargetsPath>
    <CurrentVCTargetsPath>$escapedVCTargetsPath\</CurrentVCTargetsPath>
  </PropertyGroup>
</Project>
"@

    Set-Content -Path $propsPath -Value $propsContent -Encoding UTF8
    Write-Host "Pinned VCTargetsPath for IDE builds: $VCTargetsPath"
}

function Update-VisualStudioProjectImports {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,
        [Parameter(Mandatory = $true)]
        [string]$VCTargetsPath,
        [string]$VsInstallPath,
        [string]$MsvcToolsetVersion
    )

    $escapedVCTargetsPath = [System.Security.SecurityElement]::Escape($VCTargetsPath.Replace('\', '/'))
    $escapedVsInstallPath = if ($VsInstallPath) { [System.Security.SecurityElement]::Escape($VsInstallPath.Replace('\', '/')) } else { $null }
    $escapedVcInstallPath = if ($VsInstallPath) { [System.Security.SecurityElement]::Escape((Join-Path $VsInstallPath "VC").Replace('\', '/')) } else { $null }
    $escapedMsvcInstallPath = if ($VsInstallPath -and $MsvcToolsetVersion) { [System.Security.SecurityElement]::Escape((Join-Path $VsInstallPath "VC\Tools\MSVC\$MsvcToolsetVersion").Replace('\', '/')) } else { $null }
    $projectFiles = @(Get-ChildItem -Path $BuildDir -Recurse -Filter "*.vcxproj" -File)

    foreach ($projectFile in $projectFiles) {
        $content = Get-Content -Path $projectFile.FullName -Raw
        $content = $content.Replace('Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props"', "Project=""$escapedVCTargetsPath/Microsoft.Cpp.Default.props""")
        $content = $content.Replace('Project="$(VCTargetsPath)\Microsoft.Cpp.props"', "Project=""$escapedVCTargetsPath/Microsoft.Cpp.props""")
        $content = $content.Replace('Project="$(VCTargetsPath)\Microsoft.Cpp.targets"', "Project=""$escapedVCTargetsPath/Microsoft.Cpp.targets""")

        if ($escapedVsInstallPath -and $escapedMsvcInstallPath -and $content -notmatch '<VCToolsInstallDir>') {
            $toolsetProperties = @"
    <VSInstallDir>$escapedVsInstallPath/</VSInstallDir>
    <VCInstallDir>$escapedVcInstallPath/</VCInstallDir>
    <VCToolsInstallDir>$escapedMsvcInstallPath/</VCToolsInstallDir>
    <VCToolsVersion>$MsvcToolsetVersion</VCToolsVersion>
"@
            $content = $content.Replace('    <VCProjectUpgraderObjectName>NoUpgrade</VCProjectUpgraderObjectName>', "    <VCProjectUpgraderObjectName>NoUpgrade</VCProjectUpgraderObjectName>`r`n$toolsetProperties")
        }

        Set-Content -Path $projectFile.FullName -Value $content -Encoding UTF8
    }

    Write-Host "Pinned Visual Studio project imports for IDE builds: $VCTargetsPath"
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
$cmakeGenerators = @(Get-CMakeGeneratorNames)
$visualStudioInstances = if ($runningOnWindows) { @(Get-VisualStudioInstances) } else { @() }

if ($Target -in @("all", "windows") -and $runningOnWindows -and $SkipVs2022 -and $SkipVs2026) {
    throw "Both -SkipVs2022 and -SkipVs2026 were specified; no Visual Studio generators remain."
}

$vcpkgArgs = @()
if ($DisableVcpkg) {
    $vcpkgArgs += "-DDDSVIEWER_DISABLE_AUTO_VCPKG=ON"
}
elseif ($env:VCPKG_ROOT) {
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
        $vs2022Instance = Find-VisualStudioInstance -Instances $visualStudioInstances -MajorVersion "17"

        if (-not ("Visual Studio 17 2022" -in $cmakeGenerators)) {
            Write-Warning "CMake does not support the Visual Studio 17 2022 generator. Skipping VS 2022."
        }
        elseif (-not $vs2022Instance) {
            Write-Warning "Visual Studio 2022 is not installed. Skipping VS 2022."
        }
        else {
            $vs2022MsvcVersion = Get-LatestMsvcToolsetVersion -VsInstallPath $vs2022Instance.installationPath
            $vs2022Toolset = if ($vs2022MsvcVersion) { "v143,version=$vs2022MsvcVersion" } else { "v143" }
            $vs2022VcTargetsPath = Get-VisualStudioVcTargetsPath -VsInstallPath $vs2022Instance.installationPath -MajorVersion "17"
            $vs2022Args = $vcpkgArgs + $windowsArgs + "-DCMAKE_GENERATOR_INSTANCE=$(Get-CMakeGeneratorInstance -VisualStudioInstance $vs2022Instance)"
            if ($vs2022MsvcVersion) {
                $vs2022Args += Get-VisualStudioGlobalArgs -VsInstallPath $vs2022Instance.installationPath -MsvcToolsetVersion $vs2022MsvcVersion
            }

            Write-Host "Using Visual Studio 2022: $($vs2022Instance.installationPath)"
            if (-not $vs2022Instance.isComplete -or -not $vs2022Instance.isLaunchable) {
                Write-Warning "Visual Studio 2022 is reported incomplete by Visual Studio Installer; attempting generation because an MSVC toolset was found."
            }
            if ($vs2022MsvcVersion) {
                Write-Host "Using MSVC toolset: $vs2022MsvcVersion"
            }

            Invoke-CMakeGenerate `
                -Generator "Visual Studio 17 2022" `
                -BuildDir (Join-Path $buildRootFullPath "vs2022") `
                -Architecture "x64" `
                -Toolset $vs2022Toolset `
                -VCTargetsPath $vs2022VcTargetsPath `
                -VsInstallPath $vs2022Instance.installationPath `
                -MsvcToolsetVersion $vs2022MsvcVersion `
                -AdditionalArgs $vs2022Args `
                -MultiConfig $true
        }
    }

    if (-not $SkipVs2026) {
        $vs2026Instance = Find-VisualStudioInstance -Instances $visualStudioInstances -MajorVersion "18"

        if (-not ("Visual Studio 18 2026" -in $cmakeGenerators)) {
            Write-Warning "CMake does not support the Visual Studio 18 2026 generator. Skipping VS 2026."
        }
        elseif (-not $vs2026Instance) {
            Write-Warning "Visual Studio 2026 is not installed. Skipping VS 2026."
        }
        else {
            $vs2026MsvcVersion = Get-LatestMsvcToolsetVersion -VsInstallPath $vs2026Instance.installationPath -VersionPrefix "14.4"
            $vs2026PlatformToolset = "v143"
            if (-not $vs2026MsvcVersion) {
                $vs2026MsvcVersion = Get-LatestMsvcToolsetVersion -VsInstallPath $vs2026Instance.installationPath
                $vs2026PlatformToolset = "v145"
            }
            $vs2026Toolset = if ($vs2026MsvcVersion) { "$vs2026PlatformToolset,version=$vs2026MsvcVersion" } else { $vs2026PlatformToolset }
            $vs2026VcTargetsPath = Get-VisualStudioVcTargetsPath -VsInstallPath $vs2026Instance.installationPath -MajorVersion "18"
            $vs2026Args = $vcpkgArgs + $windowsArgs + "-DCMAKE_GENERATOR_INSTANCE=$(Get-CMakeGeneratorInstance -VisualStudioInstance $vs2026Instance)"
            if ($vs2026MsvcVersion) {
                $vs2026Args += Get-VisualStudioGlobalArgs -VsInstallPath $vs2026Instance.installationPath -MsvcToolsetVersion $vs2026MsvcVersion
            }

            Write-Host "Using Visual Studio 2026: $($vs2026Instance.installationPath)"
            if (-not $vs2026Instance.isComplete -or -not $vs2026Instance.isLaunchable) {
                Write-Warning "Visual Studio 2026 is reported incomplete by Visual Studio Installer; attempting generation because an MSVC toolset was found."
            }
            if ($vs2026MsvcVersion) {
                Write-Host "Using MSVC toolset: $vs2026MsvcVersion"
            }

            Invoke-CMakeGenerate `
                -Generator "Visual Studio 18 2026" `
                -BuildDir (Join-Path $buildRootFullPath "vs2026") `
                -Architecture "x64" `
                -Toolset $vs2026Toolset `
                -VCTargetsPath $vs2026VcTargetsPath `
                -VsInstallPath $vs2026Instance.installationPath `
                -MsvcToolsetVersion $vs2026MsvcVersion `
                -AdditionalArgs $vs2026Args `
                -MultiConfig $true
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

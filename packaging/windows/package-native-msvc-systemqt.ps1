[CmdletBinding()]
param(
    [string]$InstallTree = "C:\tmp\kdevelop-native-msvc-install",
    [string]$QtDir = "C:\Qt\6.11.0\msvc2022_64",
    [string]$CraftRoot = "C:\CraftRoot",
    [string]$OutputRoot = "D:\tmp\kdevelop-native-installer",
    [string]$InstallerName = "KDevelop-CK803-Setup.exe",
    [switch]$SkipWindeployQt,
    [switch]$SkipLauncherBuild,
    [switch]$SkipNsis,
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)

function Require-Path([string]$Path, [string]$Description) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description not found: $Path"
    }
}

function Import-VisualStudioEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    Require-Path $vswhere "vswhere.exe"

    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        throw "Visual Studio C++ tools were not found."
    }

    $devCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
    Require-Path $devCmd "VsDevCmd.bat"

    $envLines = cmd /c "`"$devCmd`" -arch=amd64 -host_arch=amd64 >nul && set"
    foreach ($line in $envLines) {
        $idx = $line.IndexOf("=")
        if ($idx -gt 0) {
            [Environment]::SetEnvironmentVariable($line.Substring(0, $idx), $line.Substring($idx + 1), "Process")
        }
    }
}

function Remove-DirectoryInside([string]$Directory, [string]$ExpectedParent) {
    $fullDirectory = [IO.Path]::GetFullPath($Directory)
    $fullParent = [IO.Path]::GetFullPath($ExpectedParent)
    if (-not $fullDirectory.StartsWith($fullParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove path outside output root: $fullDirectory"
    }
    if (Test-Path -LiteralPath $fullDirectory) {
        Remove-Item -LiteralPath $fullDirectory -Recurse -Force
    }
}

function Copy-CraftRuntimeDlls([string]$SourceBin, [string]$DestBin) {
    $skip = @(
        "Qt6*.dll",
        "KF6*.dll",
        "*d.dll"
    )

    Get-ChildItem -LiteralPath $SourceBin -Filter "*.dll" | ForEach-Object {
        $name = $_.Name
        foreach ($pattern in $skip) {
            if ($name -like $pattern) {
                return
            }
        }

        $target = Join-Path $DestBin $name
        if (-not (Test-Path -LiteralPath $target)) {
            Copy-Item -LiteralPath $_.FullName -Destination $target
        }
    }
}

function Find-MakeNsis {
    $cmd = Get-Command makensis.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $candidates = @(
        "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
        "${env:ProgramFiles(x86)}\NSIS\Bin\makensis.exe",
        "$env:ProgramFiles\NSIS\makensis.exe",
        "$env:ProgramFiles\NSIS\Bin\makensis.exe"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }

    return $null
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$cp210xZip = Join-Path $repoRoot "CP210x_Windows_Drivers.zip"
$launcherSource = Join-Path $PSScriptRoot "kdevelop-launcher.cpp"
$nsisScript = Join-Path $PSScriptRoot "kdevelop-installer.nsi"
$qtBin = Join-Path $QtDir "bin"
$windeployqt = Join-Path $qtBin "windeployqt.exe"
$craftBin = Join-Path $CraftRoot "bin"

Require-Path $InstallTree "KDevelop install tree"
Require-Path (Join-Path $InstallTree "bin\kdevelop.exe") "kdevelop.exe"
Require-Path $QtDir "Qt directory"
Require-Path $qtBin "Qt bin directory"
Require-Path $windeployqt "windeployqt.exe"
Require-Path $craftBin "Craft bin directory"
Require-Path $cp210xZip "CP210x driver zip"
Require-Path $launcherSource "Launcher source"
Require-Path $nsisScript "NSIS script"

if ($ValidateOnly) {
    Write-Host "Validation passed."
    return
}

$stagingRoot = Join-Path $OutputRoot "staging"
$appPayload = Join-Path $stagingRoot "app"
$driverPayload = Join-Path $stagingRoot "drivers\CP210x"
$artifactDir = Join-Path $OutputRoot "artifacts"
$installerPath = Join-Path $artifactDir $InstallerName

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
Remove-DirectoryInside $stagingRoot $OutputRoot
New-Item -ItemType Directory -Force -Path $appPayload, $driverPayload, $artifactDir | Out-Null

Write-Host "Copying KDevelop install tree..."
Copy-Item -Path (Join-Path $InstallTree "*") -Destination $appPayload -Recurse -Force
Get-ChildItem -LiteralPath $appPayload -Recurse -File | ForEach-Object {
    if ($_.IsReadOnly) {
        $_.IsReadOnly = $false
    }
}

if (-not $SkipWindeployQt) {
    Write-Host "Deploying Qt runtime with windeployqt..."
    & $windeployqt `
        --release `
        --compiler-runtime `
        --force `
        --ignore-library-errors `
        --skip-plugin-types "position,qmltooling,generic" `
        --add-plugin-types "sqldrivers" `
        --dir (Join-Path $appPayload "bin") `
        (Join-Path $appPayload "bin\kdevelop.exe")
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE"
    }
}

Write-Host "Copying non-Qt Craft runtime DLLs..."
Copy-CraftRuntimeDlls $craftBin (Join-Path $appPayload "bin")

Write-Host "Expanding CP210x driver package..."
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($cp210xZip, $driverPayload)

if (-not $SkipLauncherBuild) {
    Write-Host "Building launcher..."
    Import-VisualStudioEnvironment
    $launcherExe = Join-Path $appPayload "KDevelop.exe"
    $launcherObj = Join-Path $artifactDir "kdevelop-launcher.obj"
    & cl.exe /nologo /std:c++17 /O2 /DUNICODE /D_UNICODE /EHsc /Fo:$launcherObj /Fe:$launcherExe $launcherSource /link /SUBSYSTEM:WINDOWS shell32.lib user32.lib
    if ($LASTEXITCODE -ne 0) {
        throw "Launcher build failed with exit code $LASTEXITCODE"
    }
} else {
    Write-Warning "Skipping launcher build. The installer shortcut expects app\KDevelop.exe."
}

if (-not $SkipNsis) {
    $makensis = Find-MakeNsis
    if (-not $makensis) {
        throw "makensis.exe was not found. Install NSIS 3.x or rerun with -SkipNsis to prepare staging only."
    }

    Write-Host "Building NSIS installer..."
    & $makensis "/DAPP_SOURCE=$stagingRoot" "/DOUTFILE=$installerPath" $nsisScript
    if ($LASTEXITCODE -ne 0) {
        throw "makensis failed with exit code $LASTEXITCODE"
    }
}

Write-Host "Staging directory: $stagingRoot"
if (Test-Path -LiteralPath $installerPath) {
    Write-Host "Installer: $installerPath"
}

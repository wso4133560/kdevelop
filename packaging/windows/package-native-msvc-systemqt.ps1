[CmdletBinding()]
param(
    [string]$InstallTree = "C:\tmp\kdevelop-native-msvc-install",
    [string]$QtDir = "C:\Qt\6.11.0\msvc2022_64",
    [string]$CraftRoot = "C:\CraftRoot",
    [string]$RiscvToolkitDir = "",
    [string]$OutputRoot = "D:\tmp\kdevelop-native-installer",
    [string]$InstallerName = "RRISE-Setup.exe",
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
        "Qt6*.dll"
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

function New-IcoFromSvg([string]$SvgPath, [string]$IcoPath, [string]$QtDir, [string]$BuildDir) {
    Import-VisualStudioEnvironment

    $sourcePath = Join-Path $BuildDir "svg-to-ico.cpp"
    $toolPath = Join-Path $BuildDir "svg-to-ico.exe"
    $source = @'
#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QStringList>
#include <QSvgRenderer>
#include <QVector>

static bool isNearWhite(QRgb pixel)
{
    return qAlpha(pixel) > 0 && qRed(pixel) >= 245 && qGreen(pixel) >= 245 && qBlue(pixel) >= 245;
}

static void makeWhiteTransparent(QImage &image)
{
    image = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (isNearWhite(line[x])) {
                line[x] = qRgba(255, 255, 255, 0);
            }
        }
    }
}

static QRect opaqueBounds(const QImage &image)
{
    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;

    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) > 8) {
                left = qMin(left, x);
                top = qMin(top, y);
                right = qMax(right, x);
                bottom = qMax(bottom, y);
            }
        }
    }

    if (right < left || bottom < top) {
        return image.rect();
    }
    return QRect(left, top, right - left + 1, bottom - top + 1);
}

static QImage renderCroppedSource(QSvgRenderer &renderer)
{
    QSizeF viewSize = renderer.viewBoxF().size();
    if (viewSize.width() <= 0 || viewSize.height() <= 0) {
        viewSize = QSizeF(2048, 2048);
    }

    const int maxSide = 2048;
    const qreal scale = maxSide / qMax(viewSize.width(), viewSize.height());
    const QSize rasterSize(qMax(1, int(viewSize.width() * scale)), qMax(1, int(viewSize.height() * scale)));

    QImage image(rasterSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&painter);
    painter.end();

    makeWhiteTransparent(image);
    return image.copy(opaqueBounds(image));
}

static QImage makeIconImage(const QImage &source, int size)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    const int padding = qMax(0, size / 32);
    const QSize targetBox(qMax(1, size - padding * 2), qMax(1, size - padding * 2));
    QSize scaled = source.size();
    scaled.scale(targetBox, Qt::KeepAspectRatio);

    const QRect target((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled.width(), scaled.height());

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target, source);
    painter.end();

    return image;
}

static void writeIconEntry(QDataStream &out, int size, const QByteArray &png, quint32 offset)
{
    out << quint8(size == 256 ? 0 : size);
    out << quint8(size == 256 ? 0 : size);
    out << quint8(0);
    out << quint8(0);
    out << quint16(1);
    out << quint16(32);
    out << quint32(png.size());
    out << offset;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() != 3) {
        return 2;
    }

    QSvgRenderer renderer(args.at(1));
    if (!renderer.isValid()) {
        return 3;
    }

    const QVector<int> sizes = {256, 128, 64, 48, 32, 16};
    QVector<QByteArray> images;
    const QImage source = renderCroppedSource(renderer);
    for (const int size : sizes) {
        QImage image = makeIconImage(source, size);

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        images.append(png);
    }

    QFile output(args.at(2));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return 4;
    }

    QDataStream out(&output);
    out.setByteOrder(QDataStream::LittleEndian);
    out << quint16(0) << quint16(1) << quint16(images.size());

    quint32 offset = 6 + images.size() * 16;
    for (int i = 0; i < images.size(); ++i) {
        writeIconEntry(out, sizes[i], images[i], offset);
        offset += images[i].size();
    }
    for (const QByteArray &png : images) {
        output.write(png);
    }

    return 0;
}
'@
    Set-Content -LiteralPath $sourcePath -Value $source -Encoding UTF8

    & cl.exe `
        /nologo `
        /std:c++17 `
        /Zc:__cplusplus `
        /permissive- `
        /utf-8 `
        /O2 `
        /EHsc `
        "/I$QtDir\include" `
        "/I$QtDir\include\QtCore" `
        "/I$QtDir\include\QtGui" `
        "/I$QtDir\include\QtSvg" `
        "/Fo$(Join-Path $BuildDir "svg-to-ico.obj")" `
        /Fe:$toolPath `
        $sourcePath `
        /link `
        "/LIBPATH:$QtDir\lib" `
        Qt6Core.lib `
        Qt6Gui.lib `
        Qt6Svg.lib
    if ($LASTEXITCODE -ne 0) {
        throw "svg-to-ico build failed with exit code $LASTEXITCODE"
    }

    $oldPath = $env:PATH
    try {
        $env:PATH = "$QtDir\bin;$oldPath"
        & $toolPath $SvgPath $IcoPath
        if ($LASTEXITCODE -ne 0) {
            throw "svg-to-ico failed with exit code $LASTEXITCODE"
        }
    } finally {
        $env:PATH = $oldPath
    }
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$cp210xZip = Join-Path $repoRoot "CP210x_Windows_Drivers.zip"
$logoSvgCandidates = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot "pics") -Filter "*LOGO-*.svg" -File)
if ($logoSvgCandidates.Count -ne 1) {
    throw "Expected exactly one RRISE logo SVG matching pics\*LOGO-*.svg, found $($logoSvgCandidates.Count)."
}
$logoSvg = $logoSvgCandidates[0].FullName
$logoSvgName = $logoSvgCandidates[0].Name
$launcherSource = Join-Path $PSScriptRoot "kdevelop-launcher.cpp"
$nsisScript = Join-Path $PSScriptRoot "kdevelop-installer.nsi"
$qtBin = Join-Path $QtDir "bin"
$windeployqt = Join-Path $qtBin "windeployqt.exe"
$craftBin = Join-Path $CraftRoot "bin"
if (-not $RiscvToolkitDir) {
    $RiscvToolkitDir = Join-Path ([IO.Path]::GetFullPath((Join-Path $repoRoot ".."))) "riscv_toolkit"
}

Require-Path $InstallTree "KDevelop install tree"
Require-Path (Join-Path $InstallTree "bin\kdevelop.exe") "kdevelop.exe"
Require-Path $QtDir "Qt directory"
Require-Path $qtBin "Qt bin directory"
Require-Path $windeployqt "windeployqt.exe"
Require-Path $craftBin "Craft bin directory"
Require-Path $RiscvToolkitDir "RISC-V toolkit directory"
Require-Path $cp210xZip "CP210x driver zip"
Require-Path $logoSvg "RRISE logo"
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

Write-Host "Copying RISC-V toolkit..."
$toolkitPayload = Join-Path $appPayload "riscv_toolkit"
New-Item -ItemType Directory -Force -Path $toolkitPayload | Out-Null
Copy-Item -Path (Join-Path $RiscvToolkitDir "*") -Destination $toolkitPayload -Recurse -Force
Get-ChildItem -LiteralPath $toolkitPayload -Recurse -File | ForEach-Object {
    if ($_.IsReadOnly) {
        $_.IsReadOnly = $false
    }
}

Write-Host "Copying RRISE logo..."
$appPics = Join-Path $appPayload "pics"
New-Item -ItemType Directory -Force -Path $appPics | Out-Null
Copy-Item -LiteralPath $logoSvg -Destination (Join-Path $appPics $logoSvgName) -Force
New-IcoFromSvg $logoSvg (Join-Path $appPics "rrise-logo.ico") $QtDir $artifactDir

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

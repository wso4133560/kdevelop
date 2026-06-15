[CmdletBinding()]
param(
    [string]$InstallTree = "C:\tmp\kdevelop-native-msvc-install",
    [string]$QtDir = "C:\Qt\6.11.0\msvc2022_64",
    [string]$CraftRoot = "C:\CraftRoot",
    [string]$RiscvToolkitDir = "",
    [string]$ThuCompilerDir = "",
    [string]$ThuCompilerMingwRuntimeDir = "",
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

function Copy-SystemQtKf6Overrides([string]$SourceBin, [string]$DestBin) {
    if (-not (Test-Path -LiteralPath $SourceBin)) {
        Write-Warning "System Qt KF6 override directory not found: $SourceBin"
        return
    }

    $overrideDlls = @(
        "KF6WidgetsAddons.dll",
        "KF6SonnetCore.dll",
        "KF6SonnetUi.dll",
        "KF6SyntaxHighlighting.dll",
        "KF6TextEditor.dll",
        "KF6BreezeIcons.dll",
        "KF6IconThemes.dll",
        "KF6ColorScheme.dll",
        "KF6XmlGui.dll"
    )

    foreach ($dll in $overrideDlls) {
        $source = Join-Path $SourceBin $dll
        if (-not (Test-Path -LiteralPath $source)) {
            throw "Required system Qt KF6 override DLL not found: $source"
        }

        Copy-Item -LiteralPath $source -Destination (Join-Path $DestBin $dll) -Force
    }
}

function Copy-QtRuntimeDlls([string]$QtBin, [string]$DestBin) {
    $dlls = @(
        "Qt6TextToSpeech.dll",
        "Qt6Multimedia.dll",
        "Qt6MultimediaWidgets.dll"
    )

    foreach ($dll in $dlls) {
        $source = Join-Path $QtBin $dll
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $DestBin $dll) -Force
        } else {
            Write-Warning "Optional Qt runtime DLL not found: $source"
        }
    }
}

function Copy-QtPluginType([string]$QtDir, [string]$DestBin, [string]$PluginType) {
    $sourceDir = Join-Path $QtDir "plugins\$PluginType"
    if (-not (Test-Path -LiteralPath $sourceDir)) {
        Write-Warning "Optional Qt plugin directory not found: $sourceDir"
        return
    }

    $destDir = Join-Path $DestBin $PluginType
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null

    Get-ChildItem -LiteralPath $sourceDir -Filter "*.dll" -File | Where-Object {
        $_.BaseName -notlike "*d"
    } | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $destDir $_.Name) -Force
    }
}

function Write-RrisePackageInfo([string]$AppRoot, [string]$RepoRoot) {
    $infoDir = Join-Path $AppRoot "bin\data\rrise"
    New-Item -ItemType Directory -Force -Path $infoDir | Out-Null

    $gitCommit = ""
    try {
        $gitCommit = (& git -C $RepoRoot rev-parse --short=12 HEAD 2>$null).Trim()
        if ($LASTEXITCODE -ne 0) {
            $gitCommit = ""
        }
    } catch {
        $gitCommit = ""
    }

    $packageTime = Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz"
    $packageTimeUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    $content = @(
        "[Package]",
        "Time=$packageTime",
        "TimeUtc=$packageTimeUtc",
        "GitCommit=$gitCommit"
    )
    Set-Content -LiteralPath (Join-Path $infoDir "package-info.ini") -Value $content -Encoding ASCII
}

function Update-ThuCompilerScripts([string]$ThuCompilerRoot) {
    $compileBat = Join-Path $ThuCompilerRoot "compile.bat"
    Require-Path $compileBat "THU compiler compile.bat"

    $content = Get-Content -LiteralPath $compileBat -Raw
    $replacement = @'
set "compiler_path=%~dp0"
for %%I in ("%compiler_path%.") do set "compiler_path=%%~fI"
'@
    $content = [regex]::Replace($content, 'set "compiler_path=[^"]*"', $replacement, 1)
    $localSourceReplacement = @'
            set "local_source=__rrise_thu_%%~na%%~xa"
            copy /y "%%a" "!local_source!" > nul
            if errorlevel 1 (
                echo ERROR: failed to copy "%%a" to working directory!
                exit /b 1
            )
            call "%MIDDLE%" "!local_source!" "!bitcode_file!"
            set "middle_exit=!errorlevel!"
            if exist "!local_source!" del /f /q "!local_source!"
            if !middle_exit! neq 0 exit /b !middle_exit!
'@
    $content = [regex]::Replace(
        $content,
        'call "%MIDDLE%" "%%a" "!bitcode_file!"',
        $localSourceReplacement,
        1
    )
    $texConvertorReplacement = @'
            if exist "ConfigPack.h" del /f /q "ConfigPack.h"
            if exist "ConfigPack_sw.txt" del /f /q "ConfigPack_sw.txt"
            set "tex_convertor_log=!file_name_without_ext!_tex_convertor.log"
            "%TEX_CONVERTOR%" -no_run -hw  Schedule.tex > "!tex_convertor_log!" 2>&1
            if errorlevel 1 exit /b !errorlevel!
            if not exist "ConfigPack.h" ping -n 2 127.0.0.1 > nul
            if not exist "ConfigPack.h" (
                echo WARN: "ConfigPack.h" is not generated, retrying tex convertor...
                "%TEX_CONVERTOR%" -no_run -hw  Schedule.tex >> "!tex_convertor_log!" 2>&1
                if errorlevel 1 exit /b !errorlevel!
                if not exist "ConfigPack.h" ping -n 2 127.0.0.1 > nul
            )
'@
    $content = [regex]::Replace(
        $content,
        '"%TEX_CONVERTOR%" -no_run -hw\s+Schedule\.tex',
        $texConvertorReplacement
    )
    $configPackReplacement = @'
if exist "ConfigPack.h" (
                ren ConfigPack.h  "!result_config!"
            ) else (
                echo ERROR: "ConfigPack.h" is not generated!
                exit /b 1
            )
'@
    $content = [regex]::Replace(
        $content,
        'if exist "ConfigPack\.h" \(\s*ren ConfigPack\.h\s+"!result_config!"\s*\)',
        $configPackReplacement
    )
    Set-Content -LiteralPath $compileBat -Value $content -NoNewline -Encoding ASCII
}

function Test-ThuCompilerMingwRuntimeDir([string]$RuntimeDir) {
    if ([string]::IsNullOrWhiteSpace($RuntimeDir) -or -not (Test-Path -LiteralPath $RuntimeDir)) {
        return $false
    }

    $required = @("libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll")
    foreach ($dll in $required) {
        if (-not (Test-Path -LiteralPath (Join-Path $RuntimeDir $dll))) {
            return $false
        }
    }
    return $true
}

function Test-ThuCompilerTexConvertorRuntimeDir([string]$ThuCompilerRoot, [string]$RuntimeDir) {
    if (-not (Test-ThuCompilerMingwRuntimeDir $RuntimeDir)) {
        return $false
    }

    $texConvertorExe = Join-Path $ThuCompilerRoot "tex-convertor\GReP-Simulator.exe"
    if (-not (Test-Path -LiteralPath $texConvertorExe)) {
        Write-Warning "THU compiler runtime probe cannot find GReP-Simulator.exe: $texConvertorExe"
        return $false
    }

    $probeDir = Join-Path $env:TEMP ("rrise-thu-tex-runtime-probe-{0}-{1}" -f $PID, ([Guid]::NewGuid().ToString("N")))
    New-Item -ItemType Directory -Force -Path $probeDir | Out-Null
    try {
        Copy-Item -LiteralPath $texConvertorExe -Destination (Join-Path $probeDir "GReP-Simulator.exe") -Force
        foreach ($dll in @("libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll")) {
            Copy-Item -LiteralPath (Join-Path $RuntimeDir $dll) -Destination (Join-Path $probeDir $dll) -Force
        }

        Push-Location $probeDir
        try {
            $probeLog = Join-Path $probeDir "probe.log"
            & cmd.exe /d /s /c '"GReP-Simulator.exe" -no_run -hw "__rrise_missing_probe__.tex" > "probe.log" 2>&1'
            $exitCode = $LASTEXITCODE
        } finally {
            Pop-Location
        }

        if ($exitCode -eq -1073741511 -or $exitCode -eq -1073741515) {
            Write-Warning "THU compiler runtime probe failed for $RuntimeDir with Windows loader exit code $exitCode."
            return $false
        }

        return $true
    } catch {
        Write-Warning "THU compiler runtime probe failed for $RuntimeDir`: $($_.Exception.Message)"
        return $false
    } finally {
        Remove-Item -LiteralPath $probeDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Resolve-ThuCompilerMingwRuntimeDir([string]$ThuCompilerRoot, [string]$ExplicitRuntimeDir) {
    if (-not [string]::IsNullOrWhiteSpace($ExplicitRuntimeDir)) {
        $full = [IO.Path]::GetFullPath($ExplicitRuntimeDir)
        if (Test-ThuCompilerTexConvertorRuntimeDir $ThuCompilerRoot $full) {
            return $full
        }
        throw "THU compiler MinGW runtime directory is missing required DLLs or is incompatible with GReP-Simulator.exe: $full"
    }

    $thuParent = [IO.Path]::GetFullPath((Join-Path $ThuCompilerRoot ".."))
    $thuGrandParent = [IO.Path]::GetFullPath((Join-Path $ThuCompilerRoot "..\.."))
    $knownReleaseRuntime = "docs-for-thu-compiler-ReleaseV0.1\docs-for-thu-compiler-ReleaseV0.1\x86_64-8.1.0-release-posix-seh-rt_v6-rev0\mingw64\bin"
    $candidates = @(
        (Join-Path $ThuCompilerRoot "tex-convertor"),
        (Join-Path $ThuCompilerRoot "mingw64\bin"),
        (Join-Path $ThuCompilerRoot "runtime\mingw64\bin"),
        (Join-Path "C:\code\compiler" $knownReleaseRuntime),
        (Join-Path "C:\code" $knownReleaseRuntime),
        (Join-Path $thuParent $knownReleaseRuntime),
        (Join-Path $thuGrandParent $knownReleaseRuntime)
    )

    foreach ($candidate in $candidates) {
        $full = [IO.Path]::GetFullPath($candidate)
        if (Test-ThuCompilerTexConvertorRuntimeDir $ThuCompilerRoot $full) {
            return $full
        } elseif (Test-ThuCompilerMingwRuntimeDir $full) {
            Write-Warning "Skipping incompatible THU compiler MinGW runtime: $full"
        }
    }

    $whereOutput = & where.exe libstdc++-6.dll 2>$null
    foreach ($libstdcpp in $whereOutput) {
        $candidate = Split-Path -Parent $libstdcpp
        if (Test-ThuCompilerTexConvertorRuntimeDir $ThuCompilerRoot $candidate) {
            return [IO.Path]::GetFullPath($candidate)
        } elseif (Test-ThuCompilerMingwRuntimeDir $candidate) {
            Write-Warning "Skipping incompatible THU compiler MinGW runtime: $candidate"
        }
    }

    throw "Unable to find THU compiler MinGW runtime DLLs compatible with GReP-Simulator.exe. Pass -ThuCompilerMingwRuntimeDir with a directory containing compatible libstdc++-6.dll, libgcc_s_seh-1.dll and libwinpthread-1.dll."
}

function Copy-ThuCompilerMingwRuntimeDlls([string]$ThuCompilerRoot, [string]$RuntimeDir) {
    $texConvertorDir = Join-Path $ThuCompilerRoot "tex-convertor"
    Require-Path $texConvertorDir "THU compiler tex-convertor directory"

    foreach ($dll in @("libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll")) {
        Copy-Item -LiteralPath (Join-Path $RuntimeDir $dll) -Destination (Join-Path $texConvertorDir $dll) -Force
    }
}

function Install-RiscvToolchainAliases([string]$ToolkitRoot) {
    $toolchainBin = Join-Path $ToolkitRoot "compiler\CKV2ElfMinilib_V3.10.29\bin"
    Require-Path $toolchainBin "RISC-V toolkit compiler bin directory"

    $tools = @(
        "addr2line",
        "ar",
        "as",
        "c++",
        "c++filt",
        "cpp",
        "elfedit",
        "g++",
        "gcc",
        "gcc-6.3.0",
        "gcc-ar",
        "gcc-nm",
        "gcc-ranlib",
        "gcov",
        "gcov-tool",
        "gdb",
        "gprof",
        "ld",
        "ld.bfd",
        "nm",
        "objcopy",
        "objdump",
        "ranlib",
        "readelf",
        "size",
        "strings",
        "strip"
    )

    foreach ($tool in $tools) {
        $source = Join-Path $toolchainBin ("csky-elfabiv2-{0}.exe" -f $tool)
        $dest = Join-Path $toolchainBin ("riscv-elfabiv2-{0}.exe" -f $tool)
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination $dest -Force
        }
    }
}

function Copy-HicolorIconThemeIndex([string]$SourceBin, [string]$PayloadRoot) {
    $source = Join-Path $SourceBin "data\icons\hicolor\index.theme"
    Require-Path $source "hicolor icon theme index"

    $destDir = Join-Path $PayloadRoot "bin\data\icons\hicolor"
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    Copy-Item -LiteralPath $source -Destination (Join-Path $destDir "index.theme") -Force
}

function Copy-Kf6ZhCnTranslations([string]$CraftRoot, [string]$PayloadRoot) {
    $sourceDir = Join-Path $CraftRoot "bin\data\locale\zh_CN\LC_MESSAGES"
    if (-not (Test-Path -LiteralPath $sourceDir)) {
        Write-Warning "KF6 zh_CN translation directory not found: $sourceDir"
        return
    }

    $destDir = Join-Path $PayloadRoot "bin\data\locale\zh_CN\LC_MESSAGES"
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null

    $catalogs = @(
        "kcolorscheme6.mo",
        "kconfigwidgets6.mo",
        "kiconthemes6.mo",
        "kio6.mo",
        "kparts6.mo",
        "ktexteditor6.mo",
        "kxmlgui6.mo"
    )

    foreach ($catalog in $catalogs) {
        $source = Join-Path $sourceDir $catalog
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $destDir $catalog) -Force
        } else {
            Write-Warning "Optional KF6 zh_CN translation not found: $source"
        }
    }
}

function Remove-RriseDisabledPlugins([string]$PayloadRoot) {
    $plugins = @(
        "lib\plugins\kdevplatform\66\kdevcraft.dll"
    )

    foreach ($plugin in $plugins) {
        $path = Join-Path $PayloadRoot $plugin
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Force
        }
    }
}

function Remove-RriseStaleTemplateArchives([string]$PayloadRoot) {
    $archives = @(
        "bin\data\kdevappwizard\templates\riscv_ifft_layout.tar.bz2"
    )

    foreach ($archive in $archives) {
        $path = Join-Path $PayloadRoot $archive
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Force
        }
    }
}

function Install-RriseTemplateDescriptions([string]$PayloadRoot) {
    $templateRoot = Join-Path $PayloadRoot "bin\data\kdevappwizard"
    $archive = Join-Path $templateRoot "templates\riscv_layout.tar.bz2"
    Require-Path $archive "RISC-V template archive"

    $descriptionDir = Join-Path $templateRoot "template_descriptions"
    New-Item -ItemType Directory -Force -Path $descriptionDir | Out-Null

    $tempDir = Join-Path $OutputRoot "template-description-extract"
    Remove-DirectoryInside $tempDir $OutputRoot
    New-Item -ItemType Directory -Force -Path $tempDir | Out-Null

    tar -xjf $archive -C $tempDir riscv_layout.kdevtemplate
    Copy-Item -LiteralPath (Join-Path $tempDir "riscv_layout.kdevtemplate") `
        -Destination (Join-Path $descriptionDir "riscv_layout.kdevtemplate") -Force
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
$systemQtKf6Bin = "C:\tmp\systemqt-kf6\bin"
if (-not $RiscvToolkitDir) {
    $RiscvToolkitDir = Join-Path ([IO.Path]::GetFullPath((Join-Path $repoRoot ".."))) "riscv_toolkit"
}
if (-not $ThuCompilerDir) {
    $ThuCompilerDir = Join-Path ([IO.Path]::GetFullPath((Join-Path $repoRoot ".."))) "thu-compiler"
}

Require-Path $InstallTree "KDevelop install tree"
Require-Path (Join-Path $InstallTree "bin\kdevelop.exe") "kdevelop.exe"
Require-Path $QtDir "Qt directory"
Require-Path $qtBin "Qt bin directory"
Require-Path $windeployqt "windeployqt.exe"
Require-Path $craftBin "Craft bin directory"
Require-Path $RiscvToolkitDir "RISC-V toolkit directory"
Require-Path $ThuCompilerDir "THU compiler directory"
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

Write-Host "Removing RRISE-disabled KDevelop plugins..."
Remove-RriseDisabledPlugins $appPayload
Remove-RriseStaleTemplateArchives $appPayload
Install-RriseTemplateDescriptions $appPayload
Write-RrisePackageInfo $appPayload $repoRoot

Write-Host "Copying RISC-V toolkit..."
$toolkitPayload = Join-Path $appPayload "riscv_toolkit"
New-Item -ItemType Directory -Force -Path $toolkitPayload | Out-Null
Copy-Item -Path (Join-Path $RiscvToolkitDir "*") -Destination $toolkitPayload -Recurse -Force
Get-ChildItem -LiteralPath $toolkitPayload -Recurse -File | ForEach-Object {
    if ($_.IsReadOnly) {
        $_.IsReadOnly = $false
    }
}
Install-RiscvToolchainAliases $toolkitPayload

Write-Host "Copying THU compiler..."
$thuCompilerPayload = Join-Path $appPayload "thu-compiler"
New-Item -ItemType Directory -Force -Path $thuCompilerPayload | Out-Null
Copy-Item -Path (Join-Path $ThuCompilerDir "*") -Destination $thuCompilerPayload -Recurse -Force
Get-ChildItem -LiteralPath $thuCompilerPayload -Recurse -File | ForEach-Object {
    if ($_.IsReadOnly) {
        $_.IsReadOnly = $false
    }
}
$thuCompilerRuntimeDir = Resolve-ThuCompilerMingwRuntimeDir $ThuCompilerDir $ThuCompilerMingwRuntimeDir
Write-Host "Copying THU compiler MinGW runtime from $thuCompilerRuntimeDir..."
Copy-ThuCompilerMingwRuntimeDlls $thuCompilerPayload $thuCompilerRuntimeDir
Update-ThuCompilerScripts $thuCompilerPayload

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

Write-Host "Copying extra Qt runtime modules..."
Copy-QtRuntimeDlls $qtBin (Join-Path $appPayload "bin")
Copy-QtPluginType $QtDir (Join-Path $appPayload "bin") "texttospeech"
Copy-QtPluginType $QtDir (Join-Path $appPayload "bin") "multimedia"

Write-Host "Copying non-Qt Craft runtime DLLs..."
Copy-CraftRuntimeDlls $craftBin (Join-Path $appPayload "bin")
Write-Host "Copying hicolor icon theme index..."
Copy-HicolorIconThemeIndex $craftBin $appPayload
Write-Host "Copying KF6 zh_CN translations..."
Copy-Kf6ZhCnTranslations $CraftRoot $appPayload
Write-Host "Overriding selected KF6 runtime DLLs with system Qt builds..."
Copy-SystemQtKf6Overrides $systemQtKf6Bin (Join-Path $appPayload "bin")

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

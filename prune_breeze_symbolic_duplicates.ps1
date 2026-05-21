$ErrorActionPreference = "Stop"

$root = "C:\tmp\src-kf6-systemqt\breeze-icons-6.26.0\icons"
$removed = 0

Get-ChildItem -Path $root -Recurse -Filter *.svg | Where-Object { $_.Name -notlike "*-symbolic.svg" } | ForEach-Object {
    $symbolic = Join-Path $_.DirectoryName ($_.BaseName + "-symbolic.svg")
    if (Test-Path -LiteralPath $symbolic) {
        Remove-Item -LiteralPath $_.FullName -Force
        Write-Output $_.FullName
        $removed++
    }
}

Write-Output "RemovedCount=$removed"

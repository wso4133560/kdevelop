$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

$buildScript = "C:\code\kdevelop\build_breeze_icons_systemqt.bat"
$maxAttempts = 40
$logPath = "C:\tmp\breeze-icons-systemqt-loop.log"

for ($attempt = 1; $attempt -le $maxAttempts; $attempt++) {
    Write-Host "Attempt $attempt"
    Remove-Item -LiteralPath $logPath -Force -ErrorAction SilentlyContinue
    $proc = Start-Process -FilePath "cmd.exe" -ArgumentList "/c `"$buildScript`" > `"$logPath`" 2>&1" -Wait -PassThru -WindowStyle Hidden
    $text = Get-Content -LiteralPath $logPath -Raw
    if ($proc.ExitCode -eq 0) {
        Write-Host "Breeze Icons rebuild succeeded."
        exit 0
    }

    $match = [regex]::Match($text, 'Fatal:\s+file "([^"]+)" is a duplicate of file "([^"]+)"')
    if (-not $match.Success) {
        $text | Write-Output
        throw "Breeze Icons rebuild failed without a recognizable duplicate-file pattern."
    }

    $duplicatePath = $match.Groups[1].Value
    Write-Host "Removing duplicate source file: $duplicatePath"
    Remove-Item -LiteralPath $duplicatePath -Force
}

throw "Exceeded $maxAttempts attempts while resolving Breeze Icons duplicate files."

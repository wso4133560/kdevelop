@echo off
setlocal enabledelayedexpansion

for /f %%%%I in ('powershell.exe -NoProfile -Command "[DateTime]::UtcNow.Ticks"') do set "BUILD_START_TICKS=%%%%I"

set "TARGET_NAME=%~1"
if "%TARGET_NAME%"=="" set "TARGET_NAME=%{APPNAMELC}"

set "SCRIPT_DIR=%~dp0"
for %%%%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%%%~fI"

call "%SCRIPT_DIR%resolve_toolkit_root.cmd"
if errorlevel 1 exit /b 1

call "%RRISE_TOOLKIT_ROOT%\scripts\env_ck803_build.cmd"
if errorlevel 1 exit /b 1

set "MAKEFILE=%PROJECT_DIR%\%{APPNAMELC}.mk"
if exist "%MAKEFILE%" goto makefile_found
echo ERROR: makefile not found: %MAKEFILE%
exit /b 1

:makefile_found

if exist "%PROJECT_DIR%\Obj" rmdir /s /q "%PROJECT_DIR%\Obj"
if exist "%PROJECT_DIR%\Lst" rmdir /s /q "%PROJECT_DIR%\Lst"
mkdir "%PROJECT_DIR%\Obj" >nul 2>nul
mkdir "%PROJECT_DIR%\Lst" >nul 2>nul

pushd "%PROJECT_DIR%"
if not defined CK803_MAKE_EXE set "CK803_MAKE_EXE=make-old.exe"
"%CK803_MAKE_EXE%" -f "%{APPNAMELC}.mk"
set "BUILD_RC=%ERRORLEVEL%"
popd

if exist "%PROJECT_DIR%\Obj\%{APPNAMELC}.elf" goto elf_found
echo ERROR: missing ELF output: %PROJECT_DIR%\Obj\%{APPNAMELC}.elf
exit /b 1

:elf_found

if exist "%PROJECT_DIR%\Lst\%{APPNAMELC}.asm" goto asm_found
echo ERROR: missing ASM output: %PROJECT_DIR%\Lst\%{APPNAMELC}.asm
exit /b 1

:asm_found

if not "%BUILD_RC%"=="0" (
    echo WARNING: make returned %BUILD_RC%, but core outputs exist.
)

for %%%%I in ("%PROJECT_DIR%\Obj\%{APPNAMELC}.elf") do set "ELF_SIZE_BYTES=%%%%~zI"
powershell.exe -NoProfile -Command "$bytes = [int64]$env:ELF_SIZE_BYTES; if ($bytes -ge 1MB) { Write-Host ('ELF file size: {0:N2} MB' -f ($bytes / 1MB)) } elseif ($bytes -ge 1KB) { Write-Host ('ELF file size: {0:N2} KB' -f ($bytes / 1KB)) } else { Write-Host ('ELF file size: {0} bytes' -f $bytes) }"
powershell.exe -NoProfile -Command "$completed = Get-Date; $elapsed = [TimeSpan]::FromTicks([DateTime]::UtcNow.Ticks - [int64]$env:BUILD_START_TICKS); Write-Host ('Build completed at {0:yyyy-MM-dd HH:mm:ss}, elapsed {1:hh\:mm\:ss\.fff}' -f $completed, $elapsed)"
echo SUCCESS: built %TARGET_NAME%
exit /b 0

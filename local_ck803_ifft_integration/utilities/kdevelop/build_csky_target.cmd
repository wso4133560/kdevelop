@echo off
setlocal enabledelayedexpansion

set "TARGET_NAME=%~1"
if "%TARGET_NAME%"=="" set "TARGET_NAME=ifftt"

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"

call "%APPDATA%\C-Sky\ck803_toolkit_extract\scripts\env_ck803_build.cmd"
if errorlevel 1 exit /b 1

set "MAKEFILE=%PROJECT_DIR%\%TARGET_NAME%.mk"
set "RT_ENTRY_TEMPLATE=%PROJECT_DIR%\__rt_entry.S.template"
if not exist "%RT_ENTRY_TEMPLATE%" set "RT_ENTRY_TEMPLATE=%APPDATA%\C-Sky\ck803_toolkit_extract\verify\ifft_project\__rt_entry.S.template"

if not exist "%MAKEFILE%" (
    echo ERROR: makefile not found: %MAKEFILE%
    exit /b 1
)

set "PROJECT_NAME="
for /f "tokens=2 delims=:=" %%A in ('findstr /b /c:"ProjectName" "%MAKEFILE%"') do set "PROJECT_NAME=%%A"
set "PROJECT_NAME=%PROJECT_NAME: =%"

if "%PROJECT_NAME%"=="" (
    echo ERROR: failed to parse ProjectName from %MAKEFILE%
    exit /b 1
)

set "MSYS_PROJECT=%PROJECT_DIR:\=/%"
set "MSYS_PROJECT=/%MSYS_PROJECT:C:=c%"

echo TARGET_NAME=%TARGET_NAME%
echo PROJECT_NAME=%PROJECT_NAME%
echo PROJECT_DIR=%PROJECT_DIR%

echo [1/3] cleaning target...
if exist "%PROJECT_DIR%\Obj" rmdir /s /q "%PROJECT_DIR%\Obj"
if exist "%PROJECT_DIR%\Lst\%PROJECT_NAME%.asm" del /f /q "%PROJECT_DIR%\Lst\%PROJECT_NAME%.asm"
if exist "%PROJECT_DIR%\Obj\%PROJECT_NAME%.ihex" del /f /q "%PROJECT_DIR%\Obj\%PROJECT_NAME%.ihex"
if not exist "%PROJECT_DIR%\Obj" mkdir "%PROJECT_DIR%\Obj"
if not exist "%PROJECT_DIR%\Lst" mkdir "%PROJECT_DIR%\Lst"
if exist "%RT_ENTRY_TEMPLATE%" copy /y "%RT_ENTRY_TEMPLATE%" "%PROJECT_DIR%\Obj\__rt_entry.S" >nul

echo [2/3] building target...
pushd "%PROJECT_DIR%"
sh -lc "cd '%MSYS_PROJECT%' && make-old -f '%TARGET_NAME%.mk'"
set "BUILD_RC=%ERRORLEVEL%"
popd

echo [3/3] verifying outputs...
if not exist "%PROJECT_DIR%\Obj\%PROJECT_NAME%.elf" (
    echo ERROR: missing ELF output: %PROJECT_DIR%\Obj\%PROJECT_NAME%.elf
    exit /b 1
)

if not exist "%PROJECT_DIR%\Lst\%PROJECT_NAME%.asm" (
    echo ERROR: missing ASM output: %PROJECT_DIR%\Lst\%PROJECT_NAME%.asm
    exit /b 1
)

if not "%BUILD_RC%"=="0" (
    echo WARNING: make returned %BUILD_RC%, but core outputs exist.
    echo WARNING: likely a post-build batch step reported non-zero.
)

echo SUCCESS: %TARGET_NAME% core build passed
echo ELF: %PROJECT_DIR%\Obj\%PROJECT_NAME%.elf
echo ASM: %PROJECT_DIR%\Lst\%PROJECT_NAME%.asm
exit /b 0

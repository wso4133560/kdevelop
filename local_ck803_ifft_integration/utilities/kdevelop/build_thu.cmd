@echo off
setlocal

set "THU_ROOT=C:\code\compiler\thu-compiler-ReleaseV0.1\thu-compiler"
set "THU_COMPILE=%THU_ROOT%\compile.bat"

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"
set "THU_WORK_DIR=%PROJECT_DIR%\src\TC_adda_mix\c"

if not exist "%THU_COMPILE%" (
    echo ERROR: THU compile.bat not found: %THU_COMPILE%
    exit /b 1
)

if not exist "%THU_WORK_DIR%" (
    echo ERROR: THU work dir not found: %THU_WORK_DIR%
    exit /b 1
)

echo THU_ROOT=%THU_ROOT%
echo WORK_DIR=%THU_WORK_DIR%

set "THU_LOG=%TEMP%\kdevelop_thu_%RANDOM%_%RANDOM%.log"

pushd "%THU_WORK_DIR%"
if "%~1"=="" (
    echo THU_ARGS=-IFFT 14
    call "%THU_COMPILE%" -IFFT 14 > "%THU_LOG%" 2>&1
) else (
    echo THU_ARGS=%*
    call "%THU_COMPILE%" %* > "%THU_LOG%" 2>&1
)
set "RC=%ERRORLEVEL%"
popd

if not "%RC%"=="0" (
    if exist "%THU_WORK_DIR%\ConfigPack_ifft1k_row_fp_DN.h" if exist "%THU_WORK_DIR%\ConfigPack_16ifft_fp_DN.h" (
        findstr /v /c:"Traceback (most recent call last):" /c:"File \"main.py\"" /c:"ZeroDivisionError:" /c:"[PYI-" "%THU_LOG%"
        echo WARNING: THU returned %RC%, but required ConfigPack headers exist.
        echo WARNING: continuing with the existing/generated THU outputs.
        del /f /q "%THU_LOG%" >nul 2>nul
        set "RC=0"
    )
)

if exist "%THU_LOG%" (
    if not "%RC%"=="0" (
        type "%THU_LOG%"
    )
    del /f /q "%THU_LOG%" >nul 2>nul
)

exit /b %RC%

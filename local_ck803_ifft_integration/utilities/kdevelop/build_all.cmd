@echo off
setlocal

set "TARGET_NAME=%~1"
if "%TARGET_NAME%"=="" set "TARGET_NAME=ifftt"

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"
set "THU_WORK_DIR=%PROJECT_DIR%\src\TC_adda_mix\c"

echo [1/2] build THU outputs...
if "%~2"=="" (
    call "%SCRIPT_DIR%build_thu.cmd"
) else (
    shift
    call "%SCRIPT_DIR%build_thu.cmd" %*
)
if errorlevel 1 exit /b 1

echo [2/2] build C-Sky target...
call "%SCRIPT_DIR%build_csky_target.cmd" "%TARGET_NAME%"
exit /b %ERRORLEVEL%

@echo off
setlocal

set "TARGET_NAME=%~1"
if "%TARGET_NAME%"=="" set "TARGET_NAME=ifftt"

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"

set "PROJECT_NAME="
for /f "tokens=2 delims=:=" %%A in ('findstr /b /c:"ProjectName" "%PROJECT_DIR%\%TARGET_NAME%.mk"') do set "PROJECT_NAME=%%A"
set "PROJECT_NAME=%PROJECT_NAME: =%"
if "%PROJECT_NAME%"=="" set "PROJECT_NAME=%TARGET_NAME%"

if exist "%PROJECT_DIR%\Obj" rmdir /s /q "%PROJECT_DIR%\Obj"
if exist "%PROJECT_DIR%\Lst\%PROJECT_NAME%.asm" del /f /q "%PROJECT_DIR%\Lst\%PROJECT_NAME%.asm"
if exist "%PROJECT_DIR%\Obj\%PROJECT_NAME%.ihex" del /f /q "%PROJECT_DIR%\Obj\%PROJECT_NAME%.ihex"

echo SUCCESS: cleaned %TARGET_NAME% outputs
exit /b 0

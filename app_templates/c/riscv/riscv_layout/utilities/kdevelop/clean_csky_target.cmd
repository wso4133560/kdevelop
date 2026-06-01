@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"

if exist "%PROJECT_DIR%\Obj" rmdir /s /q "%PROJECT_DIR%\Obj"
if exist "%PROJECT_DIR%\Lst" rmdir /s /q "%PROJECT_DIR%\Lst"

echo SUCCESS: cleaned generated outputs
exit /b 0

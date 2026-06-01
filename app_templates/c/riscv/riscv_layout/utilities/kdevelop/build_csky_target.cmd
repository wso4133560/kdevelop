@echo off
setlocal enabledelayedexpansion

set "TARGET_NAME=%~1"
if "%TARGET_NAME%"=="" set "TARGET_NAME=%{APPNAMELC}"

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"

call "%SCRIPT_DIR%resolve_toolkit_root.cmd"
if errorlevel 1 exit /b 1

call "%RRISE_TOOLKIT_ROOT%\scripts\env_ck803_build.cmd"
if errorlevel 1 exit /b 1

set "MAKEFILE=%PROJECT_DIR%\%{APPNAMELC}.mk"
if not exist "%MAKEFILE%" (
    echo ERROR: makefile not found: %MAKEFILE%
    exit /b 1
)

call :ToMsysPath "%PROJECT_DIR%" MSYS_PROJECT
call :ToMsysPath "%RRISE_TOOLKIT_ROOT%" MSYS_TOOLKIT

if exist "%PROJECT_DIR%\Obj" rmdir /s /q "%PROJECT_DIR%\Obj"
if exist "%PROJECT_DIR%\Lst" rmdir /s /q "%PROJECT_DIR%\Lst"
mkdir "%PROJECT_DIR%\Obj" >nul 2>nul
mkdir "%PROJECT_DIR%\Lst" >nul 2>nul

pushd "%PROJECT_DIR%"
sh -lc "cd '%MSYS_PROJECT%' && RRISE_TOOLKIT_ROOT='%MSYS_TOOLKIT%' make-old -f '%{APPNAMELC}.mk'"
set "BUILD_RC=%ERRORLEVEL%"
popd

if not exist "%PROJECT_DIR%\Obj\%{APPNAMELC}.elf" (
    echo ERROR: missing ELF output: %PROJECT_DIR%\Obj\%{APPNAMELC}.elf
    exit /b 1
)

if not exist "%PROJECT_DIR%\Lst\%{APPNAMELC}.asm" (
    echo ERROR: missing ASM output: %PROJECT_DIR%\Lst\%{APPNAMELC}.asm
    exit /b 1
)

if not "%BUILD_RC%"=="0" (
    echo WARNING: make returned %BUILD_RC%, but core outputs exist.
)

echo SUCCESS: built %TARGET_NAME%
exit /b 0

:ToMsysPath
setlocal
set "WIN_PATH=%~1"
set "DRIVE=%WIN_PATH:~0,1%"
set "REST=%WIN_PATH:~2%"
set "REST=%REST:\=/%"
for %%L in (a b c d e f g h i j k l m n o p q r s t u v w x y z) do if /I "%%L"=="%DRIVE%" set "DRIVE=%%L"
endlocal & set "%~2=/%DRIVE%%REST%" & exit /b 0

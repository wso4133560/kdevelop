@echo off
setlocal enabledelayedexpansion

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

echo SUCCESS: built %TARGET_NAME%
exit /b 0

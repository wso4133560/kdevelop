@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"

call "%SCRIPT_DIR%resolve_toolkit_root.cmd"
if errorlevel 1 exit /b 1

set "GDB_EXE=%RRISE_TOOLKIT_ROOT%\compiler\CKV2ElfMinilib_V3.10.29\bin\csky-elfabiv2-gdb.exe"
if not exist "%GDB_EXE%" (
    echo ERROR: gdb not found: %GDB_EXE%
    exit /b 1
)

pushd "%PROJECT_DIR%"
"%GDB_EXE%" -x "utilities\\kdevelop\\gdb_ck803_config.gdb" -x "utilities\\kdevelop\\gdb_ck803_run.gdb"
set "RC=%ERRORLEVEL%"
popd

exit /b %RC%

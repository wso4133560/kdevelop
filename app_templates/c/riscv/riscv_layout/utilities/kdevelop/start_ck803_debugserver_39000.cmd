@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%resolve_toolkit_root.cmd"
if errorlevel 1 exit /b 1

set "DEBUGSERVER_EXE=%RRISE_TOOLKIT_ROOT%\debugger\T-HeadDebugServer_V5.16.6\bin\DebugServerConsole.exe"
if not exist "%DEBUGSERVER_EXE%" (
    echo ERROR: DebugServerConsole.exe not found: %DEBUGSERVER_EXE%
    exit /b 1
)

pushd "%RRISE_TOOLKIT_ROOT%\debugger\T-HeadDebugServer_V5.16.6\bin"
"%DEBUGSERVER_EXE%" -port 39000 -setclk 12000k -nrstdelay 100 -rstwait 50 -ide
set "RC=%ERRORLEVEL%"
popd

exit /b %RC%

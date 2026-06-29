@echo off
setlocal

if not "%RRISE_TOOLKIT_ROOT%"=="" goto have_root

if not exist "%ProgramFiles%\RRISE\riscv_toolkit\scripts\env_ck803_build.cmd" goto check_x86_root
set "RRISE_TOOLKIT_ROOT=%ProgramFiles%\RRISE\riscv_toolkit"
goto have_root

:check_x86_root
if not exist "%ProgramFiles(x86)%\RRISE\riscv_toolkit\scripts\env_ck803_build.cmd" goto have_root
set "RRISE_TOOLKIT_ROOT=%ProgramFiles(x86)%\RRISE\riscv_toolkit"

:have_root
if not "%RRISE_TOOLKIT_ROOT%"=="" goto root_set
echo ERROR: RRISE toolkit root is not set.
echo ERROR: Set RRISE_TOOLKIT_ROOT or install RRISE to the default Program Files location.
exit /b 1

:root_set
if exist "%RRISE_TOOLKIT_ROOT%\scripts\env_ck803_build.cmd" goto root_ok
echo ERROR: toolkit script not found: %RRISE_TOOLKIT_ROOT%\scripts\env_ck803_build.cmd
exit /b 1

:root_ok
endlocal & set "RRISE_TOOLKIT_ROOT=%RRISE_TOOLKIT_ROOT%" & exit /b 0

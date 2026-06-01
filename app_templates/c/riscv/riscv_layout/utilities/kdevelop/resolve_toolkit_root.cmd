@echo off
setlocal

if not "%RRISE_TOOLKIT_ROOT%"=="" goto have_root

if exist "%ProgramFiles%\RRISE\riscv_toolkit\scripts\env_ck803_build.cmd" (
    set "RRISE_TOOLKIT_ROOT=%ProgramFiles%\RRISE\riscv_toolkit"
) else if exist "%ProgramFiles(x86)%\RRISE\riscv_toolkit\scripts\env_ck803_build.cmd" (
    set "RRISE_TOOLKIT_ROOT=%ProgramFiles(x86)%\RRISE\riscv_toolkit"
)

:have_root
if "%RRISE_TOOLKIT_ROOT%"=="" (
    echo ERROR: RRISE toolkit root is not set.
    echo ERROR: Set RRISE_TOOLKIT_ROOT or install RRISE to the default Program Files location.
    exit /b 1
)

if not exist "%RRISE_TOOLKIT_ROOT%\scripts\env_ck803_build.cmd" (
    echo ERROR: toolkit script not found: %RRISE_TOOLKIT_ROOT%\scripts\env_ck803_build.cmd
    exit /b 1
)

endlocal & set "RRISE_TOOLKIT_ROOT=%RRISE_TOOLKIT_ROOT%" & exit /b 0

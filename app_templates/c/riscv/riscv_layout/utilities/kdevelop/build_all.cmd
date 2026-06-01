@echo off
setlocal

call "%~dp0build_csky_target.cmd" "%{APPNAMELC}"
exit /b %ERRORLEVEL%

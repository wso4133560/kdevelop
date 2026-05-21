@echo off
setlocal

set "KDEV_BIN=C:\tmp\kdevelop-native-msvc-install\bin"
set "QT_BIN=C:\Qt\6.11.0\msvc2022_64\bin"
set "KDE_DEPS_BIN=C:\CraftRoot\bin"
set "LOCALAPPDATA=C:\tmp\kdevelop-clean-localappdata"

set "PATH=%KDEV_BIN%;%QT_BIN%;%KDE_DEPS_BIN%;%PATH%"
set "QT_PLUGIN_PATH=C:\tmp\kdevelop-native-msvc-install\lib\plugins;C:\CraftRoot\plugins;%QT_BIN%\plugins"
if not exist "%LOCALAPPDATA%" mkdir "%LOCALAPPDATA%"
"%KDEV_BIN%\kdevelop.exe" "C:\code\ifft\ifft"
echo kdevelop exit code: %ERRORLEVEL%

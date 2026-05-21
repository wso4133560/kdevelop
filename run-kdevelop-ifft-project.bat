@echo off
setlocal

set "KDEV_BIN=C:\tmp\kdevelop-native-msvc-install\bin"
set "QT_BIN=C:\Qt\6.11.0\msvc2022_64\bin"
set "KDE_DEPS_BIN=C:\CraftRoot\bin"

set "PATH=%KDEV_BIN%;%QT_BIN%;%KDE_DEPS_BIN%;%PATH%"
set "QT_PLUGIN_PATH=C:\tmp\kdevelop-native-msvc-install\lib\plugins;C:\CraftRoot\plugins;%QT_BIN%\plugins"
set "DONT_CLEAR_DUCHAIN_DIR=1"
powershell -NoProfile -ExecutionPolicy Bypass -Command "if (-not (Get-NetTCPConnection -LocalPort 39000 -State Listen -ErrorAction SilentlyContinue)) { Start-Process -FilePath 'cmd.exe' -ArgumentList '/c','\"C:\Users\tanglin\AppData\Roaming\C-Sky\ck803_toolkit_extract\scripts\start_ifft_debugserver_39000.cmd\"' -WindowStyle Hidden }"
start "" "%KDEV_BIN%\kdevelop.exe" -s "{11111111-2222-4333-8444-555555555555}"

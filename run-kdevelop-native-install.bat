@echo off
setlocal

set "KDEV_ROOT=C:\tmp\kdevelop-native-msvc-install"
set "KDEV_BIN=%KDEV_ROOT%\bin"
set "QT_BIN=C:\Qt\6.11.0\msvc2022_64\bin"
set "KDE_DEPS_BIN=C:\CraftRoot\bin"

set "PATH=%KDEV_BIN%;%QT_BIN%;%KDE_DEPS_BIN%;%PATH%"
set "KDEDIRS=%KDEV_ROOT%"
set "XDG_DATA_DIRS=%KDEV_BIN%\data"
set "XDG_CONFIG_DIRS=%KDEV_ROOT%\etc\xdg"
set "QT_PLUGIN_PATH=%KDEV_ROOT%\lib\plugins"
set "QML2_IMPORT_PATH=%KDEV_ROOT%\lib\qml"
set "QT_QUICK_CONTROLS_STYLE_PATH=%KDEV_ROOT%\lib\qml\QtQuick\Controls.2"

start "" "%KDEV_BIN%\kdevelop.exe"

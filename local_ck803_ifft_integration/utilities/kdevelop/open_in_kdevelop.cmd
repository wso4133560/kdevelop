@echo off
setlocal

set "PROJECT_FILE=C:\code\ifft\ifft\ifft.kdev4"
set "PROJECT_DIR=C:\code\ifft\ifft"
set "KDEV_ROOT=C:\tmp\kdevelop-native-msvc-install"
set "KDEV_BIN=%KDEV_ROOT%\bin"
set "QT_BIN=C:\Qt\6.11.0\msvc2022_64\bin"
set "KDE_DEPS_BIN=C:\CraftRoot\bin"

if not exist "%PROJECT_FILE%" (
    echo ERROR: project file not found: %PROJECT_FILE%
    exit /b 1
)

if not exist "%PROJECT_DIR%" (
    echo ERROR: project dir not found: %PROJECT_DIR%
    exit /b 1
)

if not exist "%KDEV_BIN%\kdevelop.exe" (
    echo ERROR: kdevelop.exe not found: %KDEV_BIN%\kdevelop.exe
    exit /b 1
)

set "PATH=%KDEV_BIN%;%QT_BIN%;%KDE_DEPS_BIN%;%PATH%"
set "KDEDIRS=%KDEV_ROOT%"
set "XDG_DATA_DIRS=%KDEV_BIN%\data"
set "XDG_CONFIG_DIRS=%KDEV_ROOT%\etc\xdg"
set "QT_PLUGIN_PATH=%KDEV_ROOT%\lib\plugins"
set "QML2_IMPORT_PATH=%KDEV_ROOT%\lib\qml"
set "QT_QUICK_CONTROLS_STYLE_PATH=%KDEV_ROOT%\lib\qml\QtQuick\Controls.2"

start "" "%KDEV_BIN%\kdevelop.exe" "%PROJECT_DIR%"
exit /b 0

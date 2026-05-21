@echo off
setlocal

set "KDEV_BIN=C:\tmp\kdevelop-native-msvc-systemqt\bin"
set "QT_BIN=C:\Qt\6.11.0\msvc2022_64\bin"
set "KDE_DEPS_BIN=C:\CraftRoot\bin"

set "PATH=%KDEV_BIN%;%QT_BIN%;%KDE_DEPS_BIN%;%PATH%"
start "" "%KDEV_BIN%\kdevelop.exe" %*

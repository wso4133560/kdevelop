@echo off
setlocal

set "PATH=C:\tmp\systemqt-kf6\bin;C:\Qt\6.11.0\msvc2022_64\bin;C:\CraftRoot\bin;%PATH%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build C:\tmp\build-sonnet-systemqt --parallel 16 || exit /b 1
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --install C:\tmp\build-sonnet-systemqt || exit /b 1
copy /Y C:\tmp\systemqt-kf6\bin\KF6SonnetCore.dll C:\tmp\kdevelop-native-msvc-systemqt\bin\ >NUL || exit /b 1
copy /Y C:\tmp\systemqt-kf6\bin\KF6SonnetUi.dll C:\tmp\kdevelop-native-msvc-systemqt\bin\ >NUL || exit /b 1

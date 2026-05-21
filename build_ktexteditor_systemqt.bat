@echo off
setlocal

set "PATH=C:\tmp\systemqt-kf6\bin;C:\Qt\6.11.0\msvc2022_64\bin;C:\CraftRoot\bin;%PATH%"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

"C:\Qt\Tools\CMake_64\bin\cmake.exe" -S C:\tmp\src-kf6-systemqt\ktexteditor-6.26.0 -B C:\tmp\build-ktexteditor-systemqt -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=C:\tmp\systemqt-kf6 -DCMAKE_PREFIX_PATH="C:\tmp\systemqt-kf6;C:\Qt\6.11.0\msvc2022_64;C:\CraftRoot" || exit /b 1
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build C:\tmp\build-ktexteditor-systemqt --parallel 16 || exit /b 1
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --install C:\tmp\build-ktexteditor-systemqt || exit /b 1
copy /Y C:\tmp\systemqt-kf6\bin\KF6TextEditor.dll C:\tmp\kdevelop-native-msvc-systemqt\bin\ >NUL || exit /b 1

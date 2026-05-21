@echo off
setlocal

set "SRC=C:\tmp\systemqt-kf6\bin"
set "DST=C:\tmp\kdevelop-native-msvc-install\bin"

copy /Y "%SRC%\KF6WidgetsAddons.dll" "%DST%\" >NUL || exit /b 1
copy /Y "%SRC%\KF6SonnetCore.dll" "%DST%\" >NUL || exit /b 1
copy /Y "%SRC%\KF6SonnetUi.dll" "%DST%\" >NUL || exit /b 1
copy /Y "%SRC%\KF6SyntaxHighlighting.dll" "%DST%\" >NUL || exit /b 1
copy /Y "%SRC%\KF6TextEditor.dll" "%DST%\" >NUL || exit /b 1
copy /Y "%SRC%\KF6BreezeIcons.dll" "%DST%\" >NUL || exit /b 1
copy /Y "%SRC%\KF6IconThemes.dll" "%DST%\" >NUL || exit /b 1
copy /Y "%SRC%\KF6ColorScheme.dll" "%DST%\" >NUL || exit /b 1
copy /Y "%SRC%\KF6XmlGui.dll" "%DST%\" >NUL || exit /b 1

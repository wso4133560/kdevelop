Unicode true

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

!ifndef APP_SOURCE
  !error "APP_SOURCE must point to the prepared staging directory."
!endif

!ifndef OUTFILE
  !define OUTFILE "KDevelop-CK803-Setup.exe"
!endif

Name "KDevelop CK803"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\KDevelop CK803"
InstallDirRegKey HKLM "Software\KDE e.V.\KDevelop CK803" "Install_Dir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${APP_SOURCE}\drivers\CP210x\SLAB_License_Agreement_VCP_Windows.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

Section "KDevelop application" SEC_APP
  SectionIn RO

  SetOutPath "$INSTDIR"
  File /r "${APP_SOURCE}\app\*.*"

  CreateDirectory "$SMPROGRAMS\KDevelop CK803"
  CreateShortcut "$SMPROGRAMS\KDevelop CK803\KDevelop.lnk" "$INSTDIR\KDevelop.exe" "" "$INSTDIR\bin\kdevelop.exe" 0
  CreateShortcut "$DESKTOP\KDevelop CK803.lnk" "$INSTDIR\KDevelop.exe" "" "$INSTDIR\bin\kdevelop.exe" 0

  WriteRegStr HKLM "Software\KDE e.V.\KDevelop CK803" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KDevelop CK803" "DisplayName" "KDevelop CK803"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KDevelop CK803" "DisplayIcon" "$INSTDIR\bin\kdevelop.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KDevelop CK803" "Publisher" "KDE e.V."
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KDevelop CK803" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section /o "CP210x USB-to-UART driver" SEC_CP210X
  SetOutPath "$INSTDIR\drivers\CP210x"
  File /r "${APP_SOURCE}\drivers\CP210x\*.*"

  ${If} ${RunningX64}
    ExecWait '"$INSTDIR\drivers\CP210x\CP210xVCPInstaller_x64.exe"'
  ${Else}
    ExecWait '"$INSTDIR\drivers\CP210x\CP210xVCPInstaller_x86.exe"'
  ${EndIf}
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\KDevelop CK803.lnk"
  Delete "$SMPROGRAMS\KDevelop CK803\KDevelop.lnk"
  RMDir "$SMPROGRAMS\KDevelop CK803"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\KDevelop CK803"
  DeleteRegKey HKLM "Software\KDE e.V.\KDevelop CK803"

  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR"
SectionEnd

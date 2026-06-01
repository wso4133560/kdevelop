Unicode true

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

!ifndef APP_SOURCE
  !error "APP_SOURCE must point to the prepared staging directory."
!endif

!ifndef OUTFILE
  !define OUTFILE "RRISE-Setup.exe"
!endif

Name "RRISE"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\RRISE"
InstallDirRegKey HKLM "Software\KDE e.V.\RRISE" "Install_Dir"
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

  CreateDirectory "$SMPROGRAMS\RRISE"
  CreateShortcut "$SMPROGRAMS\RRISE\RRISE.lnk" "$INSTDIR\KDevelop.exe" "" "$INSTDIR\bin\kdevelop.exe" 0
  CreateShortcut "$DESKTOP\RRISE.lnk" "$INSTDIR\KDevelop.exe" "" "$INSTDIR\bin\kdevelop.exe" 0

  WriteRegStr HKLM "Software\KDE e.V.\RRISE" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "DisplayName" "RRISE"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "DisplayIcon" "$INSTDIR\bin\kdevelop.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "Publisher" "KDE e.V."
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "UninstallString" "$INSTDIR\Uninstall.exe"
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
  Delete "$DESKTOP\RRISE.lnk"
  Delete "$SMPROGRAMS\RRISE\RRISE.lnk"
  RMDir "$SMPROGRAMS\RRISE"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE"
  DeleteRegKey HKLM "Software\KDE e.V.\RRISE"

  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR"
SectionEnd

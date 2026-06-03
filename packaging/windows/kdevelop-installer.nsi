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
!define MUI_ICON "${APP_SOURCE}\app\pics\rrise-logo.ico"
!define MUI_UNICON "${APP_SOURCE}\app\pics\rrise-logo.ico"

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

Function KillDebugServerConsole
  DetailPrint "Stopping DebugServerConsole.exe if it is running..."
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /IM DebugServerConsole.exe /T'
FunctionEnd

Function .onInit
  Call KillDebugServerConsole
FunctionEnd

Section "KDevelop application" SEC_APP
  SectionIn RO

  Call KillDebugServerConsole
  Delete "$INSTDIR\pics\logo.png"
  Delete "$INSTDIR\pics\logo.ico"
  Delete "$INSTDIR\lib\plugins\kdevplatform\66\kdevcraft.dll"
  Delete "$INSTDIR\bin\data\kdevappwizard\templates\riscv_ifft_layout.tar.bz2"
  Delete "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_ifft_layout.kdevtemplate"
  SetOutPath "$INSTDIR"
  File /r "${APP_SOURCE}\app\*.*"

  CreateDirectory "$LOCALAPPDATA\kdevappwizard\template_descriptions"
  CopyFiles /SILENT "$INSTDIR\bin\data\kdevappwizard\template_descriptions\riscv_layout.kdevtemplate" "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_layout.kdevtemplate"

  CreateDirectory "$SMPROGRAMS\RRISE"
  CreateShortcut "$SMPROGRAMS\RRISE\RRISE.lnk" "$INSTDIR\KDevelop.exe" "" "$INSTDIR\pics\rrise-logo.ico" 0
  CreateShortcut "$DESKTOP\RRISE.lnk" "$INSTDIR\KDevelop.exe" "" "$INSTDIR\pics\rrise-logo.ico" 0

  WriteRegStr HKLM "Software\KDE e.V.\RRISE" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "DisplayName" "RRISE"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "DisplayIcon" "$INSTDIR\pics\rrise-logo.ico"
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

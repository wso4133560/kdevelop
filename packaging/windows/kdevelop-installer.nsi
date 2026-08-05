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
!define MUI_CUSTOMFUNCTION_ABORT HandleInstallAbort
!define MUI_ICON "${APP_SOURCE}\app\pics\rrise-logo.ico"
!define MUI_UNICON "${APP_SOURCE}\app\pics\rrise-logo.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${APP_SOURCE}\licenses\RRISE-LICENSE-zh_CN.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

Var RollbackDir
Var RollbackPrepared
Var RollbackPerformed
Var RollbackFailed
Var InstallCommitted
Var HadExistingInstall
Var HadDesktopShortcut
Var HadStartMenuShortcut
Var HadCurrentTemplate
Var HadLegacyTemplate
Var HadInstallRegistry
Var PreviousInstallDir
Var HadUninstallRegistry
Var PreviousDisplayName
Var PreviousDisplayIcon
Var PreviousPublisher
Var PreviousUninstallString
Var MoveSourceDir
Var MoveDestinationDir
Var MoveContentsFailed
Var MoveFindHandle
Var MoveEntry
Var DirectoryHasEntries

Function KillDebugServerConsole
  DetailPrint "Stopping DebugServerConsole.exe if it is running..."
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /IM DebugServerConsole.exe /T'
FunctionEnd

Function CheckRRISEClosed
  nsExec::ExecToStack '"$SYSDIR\cmd.exe" /C tasklist /FI "IMAGENAME eq KDevelop.exe" /NH | findstr /I /C:"KDevelop.exe" >NUL'
  Pop $0
  Pop $1
  ${If} $0 == 0
    MessageBox MB_ICONEXCLAMATION|MB_OK "RRISE 正在运行。请先关闭 RRISE 后再继续安装。"
    Abort
  ${EndIf}
FunctionEnd

Function MoveDirectoryContents
  StrCpy $MoveContentsFailed 0
  CreateDirectory "$MoveDestinationDir"
  ClearErrors
  FindFirst $MoveFindHandle $MoveEntry "$MoveSourceDir\*.*"
  ${If} ${Errors}
    Return
  ${EndIf}

  move_directory_contents_loop:
    StrCmp $MoveEntry "" move_directory_contents_done
    StrCmp $MoveEntry "." move_directory_contents_next
    StrCmp $MoveEntry ".." move_directory_contents_next
    ClearErrors
    Rename "$MoveSourceDir\$MoveEntry" "$MoveDestinationDir\$MoveEntry"
    ${If} ${Errors}
      StrCpy $MoveContentsFailed 1
    ${EndIf}
  move_directory_contents_next:
    ClearErrors
    FindNext $MoveFindHandle $MoveEntry
    ${IfNot} ${Errors}
      Goto move_directory_contents_loop
    ${EndIf}
  move_directory_contents_done:
  FindClose $MoveFindHandle
FunctionEnd

Function CheckDirectoryHasEntries
  StrCpy $DirectoryHasEntries 0
  ClearErrors
  FindFirst $MoveFindHandle $MoveEntry "$INSTDIR\*.*"
  ${If} ${Errors}
    Return
  ${EndIf}

  check_directory_entries_loop:
    StrCmp $MoveEntry "" check_directory_entries_done
    StrCmp $MoveEntry "." check_directory_entries_next
    StrCmp $MoveEntry ".." check_directory_entries_next
    StrCpy $DirectoryHasEntries 1
    Goto check_directory_entries_done
  check_directory_entries_next:
    ClearErrors
    FindNext $MoveFindHandle $MoveEntry
    ${IfNot} ${Errors}
      Goto check_directory_entries_loop
    ${EndIf}
  check_directory_entries_done:
  FindClose $MoveFindHandle
FunctionEnd

Function PrepareInstallRollback
  StrCpy $RollbackDir "$INSTDIR.rrise-rollback"
  StrCpy $RollbackPrepared 0
  StrCpy $RollbackPerformed 0
  StrCpy $RollbackFailed 0
  StrCpy $InstallCommitted 0
  StrCpy $HadExistingInstall 0
  StrCpy $HadDesktopShortcut 0
  StrCpy $HadStartMenuShortcut 0
  StrCpy $HadCurrentTemplate 0
  StrCpy $HadLegacyTemplate 0
  StrCpy $HadInstallRegistry 0
  StrCpy $HadUninstallRegistry 0

  ; Remove an empty rollback directory left by an interrupted cleanup.
  RMDir "$RollbackDir"
  ${If} ${FileExists} "$RollbackDir\*.*"
    ${If} ${FileExists} "$INSTDIR\*.*"
      MessageBox MB_ICONSTOP|MB_OK "检测到上次安装遗留的回退目录：$RollbackDir。本次安装已停止，以防覆盖可恢复的数据。"
      Abort
    ${EndIf}

    DetailPrint "Restoring a previous interrupted installation..."
    ClearErrors
    Rename "$RollbackDir" "$INSTDIR"
    ${If} ${Errors}
      MessageBox MB_ICONSTOP|MB_OK "无法恢复上次安装的数据：$RollbackDir。本次安装已停止。"
      Abort
    ${EndIf}
  ${EndIf}

  ; A failed uninstall can leave only the debug server files behind. That is
  ; not a usable installation and must not be treated as a rollback source.
  ${If} ${FileExists} "$INSTDIR\*.*"
  ${AndIfNot} ${FileExists} "$INSTDIR\KDevelop.exe"
  ${AndIfNot} ${FileExists} "$INSTDIR\bin\kdevelop.exe"
  ${AndIfNot} ${FileExists} "$INSTDIR\Uninstall.exe"
    DetailPrint "Removing an incomplete RRISE installation directory..."
    SetOutPath "$TEMP"
    RMDir /r "$INSTDIR"
    Call CheckDirectoryHasEntries
    ${If} $DirectoryHasEntries == 1
      MessageBox MB_ICONSTOP|MB_OK "检测到不完整的 RRISE 安装残留，但无法自动清理。请确认调试服务已关闭，并以管理员权限重新运行安装程序。"
      Abort
    ${EndIf}
    RMDir "$INSTDIR"
  ${EndIf}

  InitPluginsDir
  ${If} ${FileExists} "$DESKTOP\RRISE.lnk"
    StrCpy $HadDesktopShortcut 1
    CopyFiles /SILENT "$DESKTOP\RRISE.lnk" "$PLUGINSDIR\previous-desktop.lnk"
    ${IfNot} ${FileExists} "$PLUGINSDIR\previous-desktop.lnk"
      MessageBox MB_ICONSTOP|MB_OK "无法创建桌面快捷方式的回退副本。本次安装已停止。"
      Abort
    ${EndIf}
  ${EndIf}
  ${If} ${FileExists} "$SMPROGRAMS\RRISE\RRISE.lnk"
    StrCpy $HadStartMenuShortcut 1
    CopyFiles /SILENT "$SMPROGRAMS\RRISE\RRISE.lnk" "$PLUGINSDIR\previous-start-menu.lnk"
    ${IfNot} ${FileExists} "$PLUGINSDIR\previous-start-menu.lnk"
      MessageBox MB_ICONSTOP|MB_OK "无法创建开始菜单快捷方式的回退副本。本次安装已停止。"
      Abort
    ${EndIf}
  ${EndIf}
  ${If} ${FileExists} "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_layout.kdevtemplate"
    StrCpy $HadCurrentTemplate 1
    CopyFiles /SILENT "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_layout.kdevtemplate" "$PLUGINSDIR\previous-riscv-layout.kdevtemplate"
    ${IfNot} ${FileExists} "$PLUGINSDIR\previous-riscv-layout.kdevtemplate"
      MessageBox MB_ICONSTOP|MB_OK "无法创建工程模板配置的回退副本。本次安装已停止。"
      Abort
    ${EndIf}
  ${EndIf}
  ${If} ${FileExists} "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_ifft_layout.kdevtemplate"
    StrCpy $HadLegacyTemplate 1
    CopyFiles /SILENT "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_ifft_layout.kdevtemplate" "$PLUGINSDIR\previous-riscv-ifft-layout.kdevtemplate"
    ${IfNot} ${FileExists} "$PLUGINSDIR\previous-riscv-ifft-layout.kdevtemplate"
      MessageBox MB_ICONSTOP|MB_OK "无法创建旧工程模板配置的回退副本。本次安装已停止。"
      Abort
    ${EndIf}
  ${EndIf}

  ClearErrors
  ReadRegStr $PreviousInstallDir HKLM "Software\KDE e.V.\RRISE" "Install_Dir"
  ${IfNot} ${Errors}
    StrCpy $HadInstallRegistry 1
  ${EndIf}

  ClearErrors
  ReadRegStr $PreviousDisplayName HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "DisplayName"
  ${IfNot} ${Errors}
    StrCpy $HadUninstallRegistry 1
    ReadRegStr $PreviousDisplayIcon HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "DisplayIcon"
    ReadRegStr $PreviousPublisher HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "Publisher"
    ReadRegStr $PreviousUninstallString HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "UninstallString"
  ${EndIf}

  ${If} ${FileExists} "$INSTDIR\*.*"
    DetailPrint "Saving the current RRISE installation for rollback..."
    CreateDirectory "$RollbackDir"
    StrCpy $MoveSourceDir "$INSTDIR"
    StrCpy $MoveDestinationDir "$RollbackDir"
    Call MoveDirectoryContents
    ${If} $MoveContentsFailed == 1
      StrCpy $MoveSourceDir "$RollbackDir"
      StrCpy $MoveDestinationDir "$INSTDIR"
      Call MoveDirectoryContents
      RMDir "$RollbackDir"
      MessageBox MB_ICONSTOP|MB_OK "无法创建当前 RRISE 安装的回退点。请关闭所有正在使用 RRISE 文件的程序，并确认安装程序已获得管理员权限后重试。"
      Abort
    ${EndIf}
    StrCpy $HadExistingInstall 1
  ${EndIf}

  StrCpy $RollbackPrepared 1
  DetailPrint "Installation rollback point created."
FunctionEnd

Function RollbackInstallation
  ${If} $RollbackPrepared != 1
    Return
  ${EndIf}
  ${If} $InstallCommitted == 1
    Return
  ${EndIf}

  StrCpy $RollbackPerformed 1
  StrCpy $RollbackFailed 0
  DetailPrint "Rolling back the incomplete RRISE installation..."
  Call KillDebugServerConsole

  Delete "$DESKTOP\RRISE.lnk"
  ${If} $HadDesktopShortcut == 1
    CopyFiles /SILENT "$PLUGINSDIR\previous-desktop.lnk" "$DESKTOP\RRISE.lnk"
    ${IfNot} ${FileExists} "$DESKTOP\RRISE.lnk"
      StrCpy $RollbackFailed 1
    ${EndIf}
  ${EndIf}

  Delete "$SMPROGRAMS\RRISE\RRISE.lnk"
  ${If} $HadStartMenuShortcut == 1
    CreateDirectory "$SMPROGRAMS\RRISE"
    CopyFiles /SILENT "$PLUGINSDIR\previous-start-menu.lnk" "$SMPROGRAMS\RRISE\RRISE.lnk"
    ${IfNot} ${FileExists} "$SMPROGRAMS\RRISE\RRISE.lnk"
      StrCpy $RollbackFailed 1
    ${EndIf}
  ${Else}
    RMDir "$SMPROGRAMS\RRISE"
  ${EndIf}

  Delete "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_layout.kdevtemplate"
  Delete "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_ifft_layout.kdevtemplate"
  ${If} $HadCurrentTemplate == 1
    CreateDirectory "$LOCALAPPDATA\kdevappwizard\template_descriptions"
    CopyFiles /SILENT "$PLUGINSDIR\previous-riscv-layout.kdevtemplate" "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_layout.kdevtemplate"
    ${IfNot} ${FileExists} "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_layout.kdevtemplate"
      StrCpy $RollbackFailed 1
    ${EndIf}
  ${EndIf}
  ${If} $HadLegacyTemplate == 1
    CreateDirectory "$LOCALAPPDATA\kdevappwizard\template_descriptions"
    CopyFiles /SILENT "$PLUGINSDIR\previous-riscv-ifft-layout.kdevtemplate" "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_ifft_layout.kdevtemplate"
    ${IfNot} ${FileExists} "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_ifft_layout.kdevtemplate"
      StrCpy $RollbackFailed 1
    ${EndIf}
  ${EndIf}

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE"
  DeleteRegKey HKLM "Software\KDE e.V.\RRISE"
  ${If} $HadInstallRegistry == 1
    WriteRegStr HKLM "Software\KDE e.V.\RRISE" "Install_Dir" "$PreviousInstallDir"
  ${EndIf}
  ${If} $HadUninstallRegistry == 1
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "DisplayName" "$PreviousDisplayName"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "DisplayIcon" "$PreviousDisplayIcon"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "Publisher" "$PreviousPublisher"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE" "UninstallString" "$PreviousUninstallString"
  ${EndIf}

  SetOutPath "$TEMP"
  RMDir /r "$INSTDIR"
  ${If} $HadExistingInstall == 1
    CreateDirectory "$INSTDIR"
    StrCpy $MoveSourceDir "$RollbackDir"
    StrCpy $MoveDestinationDir "$INSTDIR"
    Call MoveDirectoryContents
    ${If} $MoveContentsFailed == 1
      StrCpy $RollbackFailed 1
    ${Else}
      RMDir "$RollbackDir"
    ${EndIf}
  ${Else}
    Call CheckDirectoryHasEntries
    ${If} $DirectoryHasEntries == 1
      StrCpy $RollbackFailed 1
    ${EndIf}
  ${EndIf}

  ${If} $RollbackFailed == 0
    StrCpy $RollbackPrepared 0
    DetailPrint "Installation rollback completed."
  ${Else}
    DetailPrint "Installation rollback failed. Preserved backup: $RollbackDir"
  ${EndIf}
FunctionEnd

Function HandleInstallAbort
  StrCpy $RollbackPerformed 0
  Call RollbackInstallation
  ${If} $RollbackPerformed == 1
    ${If} $RollbackFailed == 0
      MessageBox MB_ICONINFORMATION|MB_OK "安装已取消，安装前的 RRISE 状态已经恢复。"
    ${Else}
      MessageBox MB_ICONSTOP|MB_OK "安装已取消，但自动回退未能完成。原安装备份保留在：$RollbackDir"
    ${EndIf}
  ${EndIf}
FunctionEnd

Function .onInstFailed
  StrCpy $RollbackPerformed 0
  Call RollbackInstallation
  ${If} $RollbackPerformed == 1
    ${If} $RollbackFailed == 0
      MessageBox MB_ICONEXCLAMATION|MB_OK "安装失败，安装前的 RRISE 状态已经恢复。"
    ${Else}
      MessageBox MB_ICONSTOP|MB_OK "安装失败且自动回退未能完成。原安装备份保留在：$RollbackDir"
    ${EndIf}
  ${EndIf}
FunctionEnd

Function .onInstSuccess
  StrCpy $InstallCommitted 1
  ${If} $RollbackPrepared == 1
    DetailPrint "Removing the completed installation rollback point..."
    RMDir /r "$RollbackDir"
    ${If} ${FileExists} "$RollbackDir\*.*"
      MessageBox MB_ICONEXCLAMATION|MB_OK "RRISE 已安装成功，但旧版本备份未能自动删除：$RollbackDir。确认新版本正常后可手动删除该目录。"
    ${EndIf}
    StrCpy $RollbackPrepared 0
  ${EndIf}
FunctionEnd

Function un.KillDebugServerConsole
  DetailPrint "Stopping DebugServerConsole.exe if it is running..."
  nsExec::ExecToLog '"$SYSDIR\taskkill.exe" /F /IM DebugServerConsole.exe /T'
FunctionEnd

Function InstallCkLinkDrivers
  ${IfNot} ${FileExists} "$INSTDIR\riscv_toolkit\debugger\T-HeadDebugServer_V5.16.6\drivers\csky-cklink.inf"
    MessageBox MB_ICONEXCLAMATION|MB_OK "未找到 CK-Link 调试驱动文件。调试功能可能无法识别板卡。"
    Return
  ${EndIf}

  DetailPrint "Installing CK-Link debug drivers..."
  ${If} ${RunningX64}
    ${DisableX64FSRedirection}
  ${EndIf}
  nsExec::ExecToStack '"$SYSDIR\pnputil.exe" /add-driver "$INSTDIR\riscv_toolkit\debugger\T-HeadDebugServer_V5.16.6\drivers\*.inf" /subdirs /install'
  Pop $0
  Pop $1
  ${If} ${RunningX64}
    ${EnableX64FSRedirection}
  ${EndIf}
  DetailPrint "$1"
  ${If} $0 != 0
    MessageBox MB_ICONEXCLAMATION|MB_OK "CK-Link 调试驱动安装失败，退出码：$0$\r$\n$1$\r$\n请确认安装程序以管理员权限运行，或手动安装：$INSTDIR\riscv_toolkit\debugger\T-HeadDebugServer_V5.16.6\drivers\csky-cklink.inf"
  ${EndIf}
FunctionEnd

Function un.CheckRRISEClosed
  nsExec::ExecToStack '"$SYSDIR\cmd.exe" /C tasklist /FI "IMAGENAME eq KDevelop.exe" /NH | findstr /I /C:"KDevelop.exe" >NUL'
  Pop $0
  Pop $1
  ${If} $0 == 0
    MessageBox MB_ICONEXCLAMATION|MB_OK "RRISE 正在运行。请先关闭 RRISE 后再卸载。"
    Abort
  ${EndIf}
FunctionEnd

Function .onInit
  StrCpy $RollbackPrepared 0
  StrCpy $InstallCommitted 0
FunctionEnd

Function un.onInit
  Call un.CheckRRISEClosed
FunctionEnd

Section "RRISE 主程序" SEC_APP
  SectionIn RO

  Call CheckRRISEClosed
  Call KillDebugServerConsole
  Call PrepareInstallRollback
  Delete "$INSTDIR\pics\logo.png"
  Delete "$INSTDIR\pics\logo.ico"
  Delete "$INSTDIR\lib\plugins\kdevplatform\66\kdevcraft.dll"
  Delete "$INSTDIR\bin\data\kdevappwizard\templates\riscv_ifft_layout.tar.bz2"
  Delete "$LOCALAPPDATA\kdevappwizard\template_descriptions\riscv_ifft_layout.kdevtemplate"
  SetOutPath "$INSTDIR"
  File /r "${APP_SOURCE}\app\*.*"
  SetOutPath "$INSTDIR\licenses"
  File /r "${APP_SOURCE}\licenses\*.*"
  SetOutPath "$INSTDIR"
  Call InstallCkLinkDrivers

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
  Call un.KillDebugServerConsole

  Delete "$DESKTOP\RRISE.lnk"
  Delete "$SMPROGRAMS\RRISE\RRISE.lnk"
  RMDir "$SMPROGRAMS\RRISE"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RRISE"
  DeleteRegKey HKLM "Software\KDE e.V.\RRISE"

  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR"
SectionEnd

; Abyssfire Windows installer (NSIS 3.08+, Unicode, MUI2). Built on Linux by unreal/CI/scripts/windows-installers.sh:
;
;   makensis -INPUTCHARSET UTF8 -DVERSION=1.2.3 -DVERSION_QUAD=1.2.3.45 -DSOURCE_DIR=<staged Windows dir>
;            -DLISTS_DIR=<gen_file_lists.py output> -DASSETS_DIR=<installer-assets> -DOUTFILE=<setup.exe>
;            -DMSI_UPGRADE_CODE=<UpgradeCode of abyssfire.wxs> [-DSIZE_KB=n] [-DSIGN_CMD="<signs %1 in place>"]
;            abyssfire.nsi
;
; Per-machine install into "Program Files\Abyssfire" (admin), Start-menu folder with the game and its uninstaller,
; optional desktop shortcut, Add/Remove Programs entry, in-place upgrade (the previous version is uninstalled
; silently first), UE prerequisites (VC++ runtime / DirectX) when the staged build carries UEPrereqSetup_x64.exe.
; Saves live in %LOCALAPPDATA%\Abyssfire and are never touched. Silent: setup.exe /S [/D=C:\Games\Abyssfire]
; (/D also wins over the folder of a previous install). Refuses to run over an MSI install of the game (same folder
; and Start menu; the MSI refuses a setup.exe install in turn): uninstall that one first.
Unicode true
ManifestDPIAware true
ManifestSupportedOS all
RequestExecutionLevel admin
SetCompressor /SOLID lzma
SetCompressorDictSize 64

!ifndef VERSION
  !error "pass -DVERSION=x.y.z"
!endif
!ifndef VERSION_QUAD
  !error "pass -DVERSION_QUAD=x.y.z.b"
!endif
!ifndef SOURCE_DIR
  !error "pass -DSOURCE_DIR=<staged Win64 build>"
!endif
!ifndef LISTS_DIR
  !error "pass -DLISTS_DIR=<gen_file_lists.py output>"
!endif
!ifndef ASSETS_DIR
  !error "pass -DASSETS_DIR=<installer-assets>"
!endif
!ifndef OUTFILE
  !define OUTFILE "Abyssfire-${VERSION}-win64-setup.exe"
!endif
!ifndef SIZE_KB
  !define SIZE_KB 0
!endif
!ifndef MSI_UPGRADE_CODE
  !error "pass -DMSI_UPGRADE_CODE={...} (the UpgradeCode of abyssfire.wxs)"
!endif

!define PRODUCT "Abyssfire"
!define PUBLISHER "feuvan"
!define EXE "Abyssfire.exe"
; setup.exe's own key: the MSI uses Software\feuvan\Abyssfire values as component key paths
!define REG_KEY "Software\feuvan\Abyssfire\Setup"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Abyssfire"
!define URL "https://github.com/feuvan/abyssfire"
!define PREREQ "Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe"

!if /FileExists "${SOURCE_DIR}/${EXE}"
!else
  !error "${SOURCE_DIR}/${EXE} not found: SOURCE_DIR must be the staged Windows build root"
!endif

; Authenticode: sign the uninstaller before it is embedded, and the finished installer (osslsigncode on Linux).
!ifdef SIGN_CMD
  !uninstfinalize '${SIGN_CMD} "%1"' = 0
  !finalize '${SIGN_CMD} "%1"' = 0
!endif

Name "$(AppName)"
Caption "$(AppName) ${VERSION}"
BrandingText "${PRODUCT} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\${PRODUCT}"
ShowInstDetails show
ShowUninstDetails show

VIProductVersion "${VERSION_QUAD}"
VIFileVersion "${VERSION_QUAD}"
VIAddVersionKey /LANG=0 "ProductName" "${PRODUCT}"
VIAddVersionKey /LANG=0 "CompanyName" "${PUBLISHER}"
VIAddVersionKey /LANG=0 "FileDescription" "${PRODUCT} Setup"
VIAddVersionKey /LANG=0 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=0 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=0 "LegalCopyright" "(c) ${PUBLISHER}"

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

!define MUI_ICON "${ASSETS_DIR}/Abyssfire.ico"
!define MUI_UNICON "${ASSETS_DIR}/Abyssfire.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${ASSETS_DIR}/nsis-welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${ASSETS_DIR}/nsis-welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${ASSETS_DIR}/nsis-header.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "${ASSETS_DIR}/nsis-header.bmp"
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_NODESC
; Launch through Explorer: the installer runs elevated, a direct Exec would give the game the admin token (and, with
; over-the-shoulder UAC, the admin's %LOCALAPPDATA% for its saves).
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchGame
!define MUI_FINISHPAGE_RUN_TEXT "$(RunGame)"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; The first language is the fallback; NSIS picks the one matching the Windows UI language.
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

LangString AppName ${LANG_SIMPCHINESE} "渊火 Abyssfire"
LangString AppName ${LANG_ENGLISH} "Abyssfire"
LangString SecGame ${LANG_SIMPCHINESE} "游戏本体"
LangString SecGame ${LANG_ENGLISH} "Game files"
LangString SecDesktop ${LANG_SIMPCHINESE} "桌面快捷方式"
LangString SecDesktop ${LANG_ENGLISH} "Desktop shortcut"
LangString SecPrereq ${LANG_SIMPCHINESE} "运行库 (VC++ / DirectX)"
LangString SecPrereq ${LANG_ENGLISH} "Runtime prerequisites (VC++ / DirectX)"
LangString RunGame ${LANG_SIMPCHINESE} "启动渊火"
LangString RunGame ${LANG_ENGLISH} "Launch Abyssfire"
LangString UninstallLink ${LANG_SIMPCHINESE} "卸载渊火"
LangString UninstallLink ${LANG_ENGLISH} "Uninstall Abyssfire"
LangString Need64 ${LANG_SIMPCHINESE} "渊火需要 64 位 Windows 10 或更高版本。"
LangString Need64 ${LANG_ENGLISH} "Abyssfire requires 64-bit Windows 10 or later."
LangString RemovingOld ${LANG_SIMPCHINESE} "正在移除旧版本……"
LangString RemovingOld ${LANG_ENGLISH} "Removing the previous version..."
LangString MsiInstalled ${LANG_SIMPCHINESE} "渊火已经通过 .msi 安装包安装。请先在“设置 > 应用”中卸载它，再运行本安装程序。"
LangString MsiInstalled ${LANG_ENGLISH} "Abyssfire is already installed from the .msi package. Uninstall it first (Settings > Apps), then run this setup again."

Var OldDir

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "$(Need64)" /SD IDOK
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext all
  ; an MSI install (same folder, Start menu and files) is not ours to overwrite: find it by its UpgradeCode
  System::Call 'msi::MsiEnumRelatedProductsW(w "${MSI_UPGRADE_CODE}", i 0, i 0, w .r1) i .r0'
  ${If} $0 == 0
    MessageBox MB_ICONSTOP "$(MsiInstalled)" /SD IDOK
    Abort
  ${EndIf}
  ; reuse the previous install location (InstallDirRegKey would read the 32-bit registry view), unless /D= chose one
  ReadRegStr $OldDir HKLM "${REG_KEY}" "InstallDir"
  ${If} $OldDir != ""
  ${AndIf} ${FileExists} "$OldDir\uninstall.exe"
    ${If} $INSTDIR == "$PROGRAMFILES64\${PRODUCT}"
      StrCpy $INSTDIR $OldDir
    ${EndIf}
  ${Else}
    StrCpy $OldDir ""
  ${EndIf}
FunctionEnd

Function LaunchGame
  Exec '"$WINDIR\explorer.exe" "$INSTDIR\${EXE}"'
FunctionEnd

Section "!$(SecGame)" SecGame
  SectionIn RO
  ${If} $OldDir != ""
    DetailPrint "$(RemovingOld)"
    ; _?= keeps the old uninstaller from copying itself to %TEMP%, so ExecWait really waits for it
    ExecWait '"$OldDir\uninstall.exe" /S _?=$OldDir'
    Delete "$OldDir\uninstall.exe"
  ${EndIf}

  !include "${LISTS_DIR}/install-files.nsh"

  SetOutPath "$INSTDIR"
  File "/oname=Abyssfire.ico" "${ASSETS_DIR}/Abyssfire.ico"
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\${PRODUCT}"
  CreateShortcut "$SMPROGRAMS\${PRODUCT}\${PRODUCT}.lnk" "$INSTDIR\${EXE}" "" "$INSTDIR\Abyssfire.ico" 0
  CreateShortcut "$SMPROGRAMS\${PRODUCT}\$(UninstallLink).lnk" "$INSTDIR\uninstall.exe"

  WriteRegStr HKLM "${REG_KEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${REG_KEY}" "Version" "${VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "$(AppName)"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "${UNINST_KEY}" "URLInfoAbout" "${URL}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\Abyssfire.ico"
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" ${SIZE_KB}
SectionEnd

Section "$(SecDesktop)" SecDesktop
  CreateShortcut "$DESKTOP\${PRODUCT}.lnk" "$INSTDIR\${EXE}" "" "$INSTDIR\Abyssfire.ico" 0
SectionEnd

!if /FileExists "${SOURCE_DIR}/Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe"
Section "$(SecPrereq)" SecPrereq
  ; exit code 3010 = installed, reboot pending; the game still starts
  ExecWait '"$INSTDIR\${PREREQ}" /quiet /norestart'
SectionEnd
!endif

Function un.onInit
  SetRegView 64
  SetShellVarContext all
FunctionEnd

Section "Uninstall"
  Delete "$DESKTOP\${PRODUCT}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT}\${PRODUCT}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT}\$(UninstallLink).lnk"
  Delete "$SMPROGRAMS\${PRODUCT}\*.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT}"

  !include "${LISTS_DIR}/uninstall-files.nsh"
  Delete "$INSTDIR\Abyssfire.ico"
  Delete "$INSTDIR\uninstall.exe"
  ; only removed when empty (never RMDir /r on a user-chosen folder)
  RMDir "$INSTDIR"

  DeleteRegKey HKLM "${UNINST_KEY}"
  DeleteRegKey HKLM "${REG_KEY}"
  DeleteRegKey /ifempty HKLM "Software\feuvan\Abyssfire"
  DeleteRegKey /ifempty HKLM "Software\feuvan"
SectionEnd

; NereusSDR NSIS installer script
; Builds: NereusSDR-vX.Y.Z-Windows-x64-setup.exe
; Invoked by .github/workflows/release.yml build-windows job.

!include "MUI2.nsh"
!include "LogicLib.nsh"

;--- Variables passed in via /D on the makensis command line ---
; NSDR_VERSION   — full version string, e.g. 0.2.0
; NSDR_DEPLOYDIR — path to the windeployqt'd deploy/ directory
; NSDR_OUTFILE   — full output path for the .exe

!ifndef NSDR_VERSION
  !define NSDR_VERSION "0.0.0"
!endif
!ifndef NSDR_DEPLOYDIR
  !define NSDR_DEPLOYDIR "deploy"
!endif
!ifndef NSDR_OUTFILE
  !define NSDR_OUTFILE "NereusSDR-setup.exe"
!endif
!ifndef NSDR_VERSION_NUMERIC
  !define NSDR_VERSION_NUMERIC "0.0.0"
!endif

Name "NereusSDR ${NSDR_VERSION}"
OutFile "${NSDR_OUTFILE}"
Unicode True
InstallDir "$PROGRAMFILES64\NereusSDR"
InstallDirRegKey HKLM "Software\NereusSDR" "InstallDir"
RequestExecutionLevel admin

VIProductVersion "${NSDR_VERSION_NUMERIC}.0"
VIAddVersionKey "ProductName" "NereusSDR"
VIAddVersionKey "FileDescription" "NereusSDR — cross-platform OpenHPSDR SDR console"
VIAddVersionKey "FileVersion" "${NSDR_VERSION}"
VIAddVersionKey "ProductVersion" "${NSDR_VERSION}"
VIAddVersionKey "LegalCopyright" "© JJ Boyd ~KG4VCF"

;--- Modern UI pages ---
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "NereusSDR (required)" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${NSDR_DEPLOYDIR}\*.*"

  WriteRegStr HKLM "Software\NereusSDR" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\NereusSDR" "Version" "${NSDR_VERSION}"

  ; Add/Remove Programs entry
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
              "DisplayName" "NereusSDR ${NSDR_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
              "DisplayVersion" "${NSDR_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
              "Publisher" "JJ Boyd ~KG4VCF"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
              "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
              "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
              "DisplayIcon" "$INSTDIR\NereusSDR.exe"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
              "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR" \
              "NoRepair" 1

  ; Windows Firewall inbound UDP allow for NereusSDR.exe.
  ;
  ; OpenHPSDR Protocol 2 radios (ANAN, Hermes, HL2, Saturn) stream
  ; DDC I/Q data from UDP source ports 1035-1041 to the client PC. Those
  ; packets are "unsolicited inbound UDP" from Windows Defender Firewall's
  ; stateful-inspection point of view — the PC never sends to those source
  ; ports so the firewall has no return-flow mapping and silently drops
  ; every DDC packet. Result: the spectrum/waterfall stays empty even
  ; though the radio is connected and streaming correctly. Control
  ; packets (high-priority status on 1025, mic samples on 1026) flow fine
  ; because the PC sends CmdRx/CmdTx to those radio ports, which opens
  ; the return flow. Pre-v0.1.2 installers had no firewall rule and
  ; users had to either allow manually or disable the firewall; this
  ; rule is what Thetis, PowerSDR, SparkSDR, and every other OpenHPSDR
  ; client installer creates at install time.
  ;
  ; netsh errors are non-fatal — log and continue so the installer still
  ; succeeds on systems where the firewall service is disabled or a
  ; policy blocks the change. The user can always allow manually.
  DetailPrint "Adding Windows Firewall rule: NereusSDR (inbound UDP)"
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="NereusSDR (inbound UDP)"'
  Pop $0
  nsExec::ExecToLog 'netsh advfirewall firewall add rule name="NereusSDR (inbound UDP)" dir=in action=allow program="$INSTDIR\NereusSDR.exe" protocol=UDP profile=any description="Allow inbound UDP from OpenHPSDR radios (DDC IQ streams, control)"'
  Pop $0
  ${If} $0 != 0
    DetailPrint "netsh returned $0 — firewall rule may not be active. You can allow NereusSDR manually via Windows Security → Firewall & network protection."
  ${EndIf}

  WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Start Menu shortcut" SecStartMenu
  CreateDirectory "$SMPROGRAMS\NereusSDR"
  CreateShortcut "$SMPROGRAMS\NereusSDR\NereusSDR.lnk" "$INSTDIR\NereusSDR.exe"
  CreateShortcut "$SMPROGRAMS\NereusSDR\Uninstall NereusSDR.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\NereusSDR.lnk" "$INSTDIR\NereusSDR.exe"
SectionEnd

;--- Uninstaller ---
Section "Uninstall"
  Delete "$DESKTOP\NereusSDR.lnk"
  Delete "$SMPROGRAMS\NereusSDR\NereusSDR.lnk"
  Delete "$SMPROGRAMS\NereusSDR\Uninstall NereusSDR.lnk"
  RMDir  "$SMPROGRAMS\NereusSDR"

  ; Remove the firewall rule we created at install time. Ignore the
  ; return code — if the rule is already gone (user removed it manually,
  ; or a previous uninstall already cleaned up), that's fine.
  DetailPrint "Removing Windows Firewall rule: NereusSDR (inbound UDP)"
  nsExec::ExecToLog 'netsh advfirewall firewall delete rule name="NereusSDR (inbound UDP)"'
  Pop $0

  RMDir /r "$INSTDIR"

  DeleteRegKey HKLM "Software\NereusSDR"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\NereusSDR"
SectionEnd

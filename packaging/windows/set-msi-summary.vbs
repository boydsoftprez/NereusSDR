' Set the NereusSDR MSI Summary Information (Explorer Details tab).
'
' WiX writes "Installation Database" as the SummaryInformation Title and has
' no Package attribute for it, so the installer build fixes it after Light.
' Each property is written in its own open/persist cycle: updating more than
' one property on the same SummaryInfo object makes the MSI API reject the
' second one with "Property,Pid".

Option Explicit

Dim msi, version

If WScript.Arguments.Count < 1 Then
    WScript.Echo "usage: set-msi-summary.vbs <msi> [version]"
    WScript.Quit 2
End If

msi = WScript.Arguments(0)
If WScript.Arguments.Count >= 2 Then
    version = WScript.Arguments(1)
Else
    version = ""
End If

SetSummary msi, 2, "NereusSDR"
SetSummary msi, 3, "Software Defined Radio"
If version = "" Then
    SetSummary msi, 6, "Installs NereusSDR"
Else
    SetSummary msi, 6, "Installs NereusSDR " & version
End If

Sub SetSummary(path, id, value)
    Dim installer, db, si
    Set installer = CreateObject("WindowsInstaller.Installer")
    Set db = installer.OpenDatabase(path, 1)   ' msiOpenDatabaseModeTransacted
    Set si = db.SummaryInformation(1)          ' 1 = update
    si.Property(id) = value
    si.Persist
    db.Commit
End Sub

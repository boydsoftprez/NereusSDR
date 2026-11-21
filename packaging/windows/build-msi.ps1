# Build the NereusSDR Windows deploy tree and an MSI installer from a
# Visual Studio Release or Debug build.
#
#   powershell -ExecutionPolicy Bypass -File packaging\windows\build-msi.ps1
#
# Outputs:
#   bin\<Config>\...                              runnable deploy tree
#   bin\Installers\NereusSDR-<version>-Windows-x64.msi
#
# Requires: Qt 6 (windeployqt), WiX Toolset v3.14, Visual Studio (dumpbin).
# DFNR is optional: if deepfilter.dll and its model are absent, the installer
# simply ships without the DFNR runtime (the app was built with DFNR disabled).

[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [string]$Version = "2026.11.19",
    # MSI ProductVersion must have major/minor < 256; the year-based release
    # version is shown through the DisplayVersion property instead.
    [string]$MsiVersion = "1.11.2",
    [string]$DeployDir = "",
    [string]$OutFile = "",
    # The name the desktop/Start-menu shortcuts carry (the revision date).
    # Empty: the Release version, as before. Metadata keeps $Version.
    [string]$ShortcutName = "",
    # The revision the artifact is named for (folder date, e.g. 2026.11.21).
    # Empty: the release version. Names the file and the shortcuts; the
    # installer metadata stays $Version.
    [string]$Revision = "",
    [string]$QtBin = "C:/Qt/6.12.0/msvc2022_64/bin",
    [string]$WixBin = "C:/Program Files (x86)/WiX Toolset v3.14/bin",
    [string]$OpenSslBin = "C:/Program Files/OpenSSL-Win64/bin",
    [string]$MsvcBin = "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64",
    # Stage bin\<Config> only; used for the Debug tree, which is not packaged.
    [switch]$SkipMsi
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $BuildDir) { $BuildDir = Join-Path $root "build-vs" }
if (-not $DeployDir) { $DeployDir = Join-Path $root "bin\$Config" }
if (-not $Revision) {
    # Follow the revision: take the date from the dated tree folder
    # (NereusSDR_2026-11-21 -> 2026-11-21); otherwise the product version.
    if ($root -match '(20\d{2})-(\d{2})-(\d{2})') { $Revision = "$($Matches[1]).$($Matches[2]).$($Matches[3])" }
    else { $Revision = $Version }
}
if ($Revision -match '^(20\d{2})-(\d{2})-(\d{2})$') { $Revision = "$($Matches[1]).$($Matches[2]).$($Matches[3])" }
if (-not $ShortcutName) { $ShortcutName = $Revision }
if (-not $OutFile) {
    $OutFile = Join-Path $root "bin\Installers\NereusSDR-$Revision-Windows-x64.msi"
}

# The dated tree keeps a visible helper set next to src\; solution builds write
# their payloads there. Fall back to the in-source helpers for upstream layouts.
$helpersRoot = Join-Path (Split-Path -Parent $root) "helpers"
if (-not (Test-Path $helpersRoot)) { $helpersRoot = Join-Path $root "third_party\helpers" }

function Require-Path([string]$Path, [string]$What) {
    if (-not (Test-Path $Path)) { throw "$What not found: $Path" }
}

function Invoke-Tool([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        throw "$([IO.Path]::GetFileName($Exe)) failed with exit code $code"
    }
}

Require-Path $BuildDir "Build directory"
Require-Path $QtBin "Qt bin directory"
Require-Path $WixBin "WiX bin directory"
Require-Path (Join-Path $MsvcBin "dumpbin.exe") "dumpbin.exe"

$configDir = Join-Path $BuildDir $Config
Require-Path $configDir "Build output directory"

$windeployqt = Join-Path $QtBin "windeployqt.exe"
Require-Path $windeployqt "windeployqt.exe"

$heat = Join-Path $WixBin "heat.exe"
$candle = Join-Path $WixBin "candle.exe"
$light = Join-Path $WixBin "light.exe"
foreach ($tool in @($heat, $candle, $light)) { Require-Path $tool "WiX tool" }

if (Test-Path $DeployDir) { Remove-Item $DeployDir -Recurse -Force }
New-Item -ItemType Directory -Path $DeployDir | Out-Null

# --- main binaries ---------------------------------------------------------
$mainFiles = @(
    @{ From = (Join-Path $configDir "NereusSDR.exe");             To = "NereusSDR.exe" },
    @{ From = (Join-Path $configDir "NereusSDRLib.dll");          To = "NereusSDRLib.dll" },
    @{ From = (Join-Path $configDir "NereusCore.dll");            To = "NereusCore.dll" },
    @{ From = (Join-Path $configDir "nereusd.exe");               To = "nereusd.exe" },
    @{ From = (Join-Path $configDir "nereus-audio-capture.exe");  To = "nereus-audio-capture.exe" }
)
foreach ($f in $mainFiles) {
    Require-Path $f.From "Required build output"
    Copy-Item $f.From (Join-Path $DeployDir $f.To) -Force
}

$rade = Join-Path $BuildDir "third_party\rade\src\$Config\rade.dll"
Require-Path $rade "rade.dll"
Copy-Item $rade (Join-Path $DeployDir "rade.dll") -Force

# --- bundled helper applications -----------------------------------------
# WaveNode USB is installed with Nereus but runs independently: Nereus never
# starts or stops it. The Palstar HF-Auto Controller and its MMIO frontend are
# bundled and started as before.
$oldWave = Join-Path $DeployDir "WaveNode"
if (Test-Path $oldWave) { Remove-Item $oldWave -Recurse -Force }
$oldWaveUsb = Join-Path $DeployDir "WaveNodeUsb"
if (Test-Path $oldWaveUsb) { Remove-Item $oldWaveUsb -Recurse -Force }
foreach ($helper in @("WaveNodeUsb", "HF-Auto Controller", "HF-Auto MMIO", "SDR Scripts")) {
    $source = Join-Path $helpersRoot $helper
    if (Test-Path $source) {
        $target = Join-Path $DeployDir $helper
        if (Test-Path $target) { Remove-Item $target -Recurse -Force }
        Copy-Item $source $target -Recurse -Force
    }
}
# The helper's development log, self-test report and symbols are not part of
# the installed payload.
$waveUsb = Join-Path $DeployDir "WaveNodeUsb"
if (Test-Path $waveUsb) {
    Get-ChildItem $waveUsb -File | Where-Object {
        $_.Extension -in @('.pdb', '.log') -or $_.Name -like '*-selftest.txt'
    } | Remove-Item -Force
}
# Thetis .dat envelope bridge (built by the thetis-dat-bridge target).
$datBridge = Join-Path $BuildDir "thetis-dat-bridge.exe"
if (Test-Path $datBridge) {
    Copy-Item $datBridge (Join-Path $DeployDir "thetis-dat-bridge.exe") -Force
}
# Ready-made Thetis container files (one .dat per meter layout).
$containerFiles = Join-Path $root "container-files"
if (Test-Path $containerFiles) {
    $containerDest = Join-Path $DeployDir "Container Files"
    New-Item -ItemType Directory -Path $containerDest -Force | Out-Null
    Copy-Item (Join-Path $containerFiles "*") $containerDest -Force
}

# Debug/RelWithDebInfo: keep the PDBs next to the binaries so the deploy tree
# is actually debuggable. Release builds do not need them.
if ($Config -ne "Release") {
    foreach ($name in @("NereusSDR", "NereusSDRLib", "NereusCore", "nereusd", "nereus-audio-capture")) {
        $pdb = Join-Path $configDir "$name.pdb"
        if (Test-Path $pdb) { Copy-Item $pdb (Join-Path $DeployDir "$name.pdb") -Force }
    }
    $radePdb = Join-Path $BuildDir "third_party\rade\src\$Config\rade.pdb"
    if (Test-Path $radePdb) { Copy-Item $radePdb (Join-Path $DeployDir "rade.pdb") -Force }
}

foreach ($dll in @("libfftw3-3.dll", "libfftw3f-3.dll")) {
    $p = Join-Path $root "third_party\fftw3\bin\$dll"
    if (Test-Path $p) { Copy-Item $p (Join-Path $DeployDir $dll) -Force }
}

foreach ($dll in @("libcrypto-3-x64.dll", "libssl-3-x64.dll")) {
    $p = Join-Path $OpenSslBin $dll
    if (Test-Path $p) { Copy-Item $p (Join-Path $DeployDir $dll) -Force }
}

# --- Visual C++ runtime ----------------------------------------------------
# The app, Qt and the static WaveNode helper need the MSVC runtime. Copy the
# x64 CRT beside the executables so an installed machine does not need the
# VC++ redistributable installed separately.
$vcRedistRoot = Join-Path $env:ProgramFiles "Microsoft Visual Studio\18\Community\VC\Redist\MSVC"
if (Test-Path $vcRedistRoot) {
    $crt = Get-ChildItem $vcRedistRoot -Recurse -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\Microsoft\.VC' -and $_.FullName -notmatch '\\spectre\\|\\onecore\\' } |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($crt) {
        Copy-Item (Join-Path $crt.FullName "*.dll") $DeployDir -Force
    } else {
        Write-Warning "VC++ x64 CRT folder not found under $vcRedistRoot"
    }
} else {
    Write-Warning "Visual C++ Redist folder not found; the target machine needs the VC++ redistributable."
}

$deepfilter = Join-Path $root "third_party\deepfilter\lib\windows-x86_64\deepfilter.dll"
if (Test-Path $deepfilter) {
    Copy-Item $deepfilter (Join-Path $DeployDir "deepfilter.dll") -Force
}

# --- Qt runtime ------------------------------------------------------------
$deployExe = Join-Path $DeployDir "NereusSDR.exe"
# Qt uses its Schannel TLS backend on Windows by default; the app's own
# libcrypto/libssl DLLs are copied separately for Core certificate generation.
$qtArgs = @("--no-translations", "--no-system-d3d-compiler")
if ($Config -eq "Debug") { $qtArgs += "--debug" } else { $qtArgs += "--release" }
$qtArgs += @($deployExe,
    (Join-Path $DeployDir "NereusSDRLib.dll"),
    (Join-Path $DeployDir "NereusCore.dll"),
    (Join-Path $DeployDir "nereus-audio-capture.exe"),
    (Join-Path $DeployDir "nereusd.exe"))
Invoke-Tool $windeployqt $qtArgs

# --- DirectX shader compiler ------------------------------------------------
# windeployqt only copies dxcompiler.dll/dxil.dll when it finds them on PATH;
# a plain PowerShell session often does not, so add the newest Windows SDK copy
# and keep the deploy tree/installer self-contained.
if (-not (Test-Path (Join-Path $DeployDir "dxcompiler.dll"))) {
    $dxc = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\dxcompiler.dll" -ErrorAction SilentlyContinue |
           Sort-Object { [version]($_.Directory.Parent.Name) } -Descending | Select-Object -First 1
    if ($dxc) {
        Copy-Item $dxc.FullName (Join-Path $DeployDir "dxcompiler.dll") -Force
        $dxil = Join-Path $dxc.DirectoryName "dxil.dll"
        if (Test-Path $dxil) { Copy-Item $dxil (Join-Path $DeployDir "dxil.dll") -Force }
        Write-Host "DX compiler: $($dxc.Directory.Parent.Name)"
    }
}

# --- licenses and models ---------------------------------------------------
$licenseDir = Join-Path $DeployDir "licenses"
New-Item -ItemType Directory -Path $licenseDir -Force | Out-Null
$jreLegal = Join-Path $helpersRoot "SDR Scripts\jre\legal"
if (Test-Path $jreLegal) {
    Copy-Item $jreLegal (Join-Path $licenseDir "jre") -Recurse -Force
    $jreNotice = Join-Path $helpersRoot "SDR Scripts\jre\NOTICE"
    if (Test-Path $jreNotice) { Copy-Item $jreNotice (Join-Path $licenseDir "jre\NOTICE") -Force }
}
Copy-Item (Join-Path $root "packaging\third-party-licenses\*") $licenseDir -Recurse -Force
Copy-Item (Join-Path $root "LICENSE") $licenseDir -Force
Copy-Item (Join-Path $root "LICENSE-DUAL-LICENSING") $licenseDir -Force

$rnnoiseLicense = Join-Path $root "third_party\rnnoise\LICENSE"
if (Test-Path $rnnoiseLicense) {
    New-Item -ItemType Directory -Path (Join-Path $licenseDir "rnnoise") -Force | Out-Null
    Copy-Item $rnnoiseLicense (Join-Path $licenseDir "rnnoise\LICENSE") -Force
}

$icenseRtf = @'
{\rtf1\ansi\deff0{\fonttbl{\f0 Segoe UI;}}\f0\fs20
NereusSDR is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.\par
\par
NereusSDR is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.\par
\par
The full licence is installed as licenses\LICENSE. Third-party notices are in
licenses\ and the source offer is in licenses\SOURCE-OFFER.txt.\par
}
'@
[IO.File]::WriteAllText((Join-Path $licenseDir "NereusSDR-License.rtf"), $icenseRtf, (New-Object Text.UTF8Encoding($false)))

$rnnoiseModels = Join-Path $root "third_party\rnnoise\models"
if (Test-Path $rnnoiseModels) {
    $modelDir = Join-Path $DeployDir "models\rnnoise"
    New-Item -ItemType Directory -Path $modelDir -Force | Out-Null
    Copy-Item (Join-Path $rnnoiseModels "*.bin") $modelDir -Force -ErrorAction SilentlyContinue
}
$dfModel = Join-Path $root "third_party\deepfilter\models\DeepFilterNet3_onnx.tar.gz"
if (Test-Path $dfModel) {
    $dfModelDir = Join-Path $DeployDir "models\dfnet3"
    New-Item -ItemType Directory -Path $dfModelDir -Force | Out-Null
    Copy-Item $dfModel $dfModelDir -Force
}

# --- dependency check ------------------------------------------------------
$dumpbin = Join-Path $MsvcBin "dumpbin.exe"
$available = @{}
foreach ($dir in @($BuildDir, $QtBin, $OpenSslBin)) {
    if (Test-Path $dir) {
        Get-ChildItem $dir -Recurse -File -Filter "*.dll" -ErrorAction SilentlyContinue |
            ForEach-Object { $available[$_.Name.ToLower()] = $_.FullName }
    }
}

$present = @{}
Get-ChildItem $DeployDir -Recurse -File |
    ForEach-Object { $present[$_.Name.ToLower()] = $true }

$missing = New-Object System.Collections.Generic.List[string]
Get-ChildItem $DeployDir -Recurse -File |
    Where-Object { $_.Extension -in ".exe", ".dll" } |
    ForEach-Object {
        $bin = $_
        & $dumpbin /nologo /dependents $bin.FullName 2>$null | ForEach-Object {
            if ($_ -match '^\s+(\S+\.dll)\s*$') {
                $dll = $Matches[1]
                $lower = $dll.ToLower()
                if (-not $present.ContainsKey($lower) -and
                    -not $lower.StartsWith("api-ms-") -and
                    -not $lower.StartsWith("ext-ms-") -and
                    -not (Test-Path (Join-Path $env:SystemRoot "System32\$dll"))) {
                    if ($available.ContainsKey($lower)) {
                        Copy-Item $available[$lower] (Join-Path $DeployDir $dll) -Force
                        $present[$lower] = $true
                        Write-Host "Copied runtime dependency $dll"
                    } else {
                        $missing.Add("$($bin.Name) -> $dll") | Out-Null
                    }
                }
            }
        }
    }
if ($missing.Count -gt 0) {
    $missing | ForEach-Object { Write-Host "Missing dependency: $_" }
    throw "Unresolved DLL imports; the deploy tree would not start"
}

if ($SkipMsi) {
    Write-Host ""
    Write-Host "Deploy: $DeployDir"
    return
}

# --- first-run settings seed ----------------------------------------------
# Ship the maintained workspace/layout as a first-run default. AppSettings
# copies it only when no settings file exists, so installed users keep theirs.
$seedSource = Join-Path $PSScriptRoot "seed\NereusSDR.settings"
if (Test-Path $seedSource) {
    $seedDir = Join-Path $DeployDir "defaults"
    New-Item -ItemType Directory -Path $seedDir -Force | Out-Null
    Copy-Item $seedSource (Join-Path $seedDir "NereusSDR.settings") -Force
    Write-Host "First-run settings seed: $seedSource"
}

# --- MSI -------------------------------------------------------------------
$wixWork = Join-Path $BuildDir "wix"
if (Test-Path $wixWork) { Remove-Item $wixWork -Recurse -Force }
New-Item -ItemType Directory -Path $wixWork | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $OutFile) -Force | Out-Null

$harvest = Join-Path $wixWork "NereusFiles.wxs"
Invoke-Tool $heat @(
    "dir", $DeployDir,
    "-cg", "NereusFiles",
    "-dr", "INSTALLFOLDER",
    "-gg", "-g1", "-srd", "-sfrag", "-sreg",
    "-var", "var.DeployDir",
    "-out", $harvest
)

$mainWxs = Join-Path $PSScriptRoot "NereusSDR.wxs"
$mainObj = Join-Path $wixWork "NereusSDR.wixobj"
$filesObj = Join-Path $wixWork "NereusFiles.wixobj"
Invoke-Tool $candle @("-nologo", "-arch", "x64",
    "-ext", "WixFirewallExtension",
    "-dDeployDir=$DeployDir", "-dVersion=$Version", "-dMsiVersion=$MsiVersion", "-dShortcutName=$ShortcutName",
    "-out", $mainObj, $mainWxs)
Invoke-Tool $candle @("-nologo", "-arch", "x64",
    "-ext", "WixFirewallExtension",
    "-dDeployDir=$DeployDir", "-dVersion=$Version", "-dMsiVersion=$MsiVersion", "-dShortcutName=$ShortcutName",
    "-out", $filesObj, $harvest)
Invoke-Tool $light @("-nologo", "-cultures:en-us", "-sice:ICE61",
    "-ext", "WixUIExtension", "-ext", "WixFirewallExtension",
    "-out", $OutFile, $mainObj, $filesObj)

# WiX's SummaryInformation Title is always "Installation Database"; set the
# product title shown in Explorer's Details tab after linking.
$summaryScript = Join-Path $PSScriptRoot "set-msi-summary.vbs"
Invoke-Tool (Join-Path $env:SystemRoot "System32\cscript.exe") @(
    "//nologo", $summaryScript, $OutFile, $Version)

Require-Path $OutFile "MSI output"
Write-Host ""
Write-Host "Deploy: $DeployDir"
Write-Host "MSI:    $OutFile"

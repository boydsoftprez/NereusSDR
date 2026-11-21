# Fetch the Java runtime used by the bundled Denkovi relay tool.
#
# The optional start/exit scripts run DenkoviRelayCommandLineTool.jar. To keep
# an installed NereusSDR self-sufficient, the installer ships a Java runtime
# beside it. This script downloads the Eclipse Temurin JRE (GPLv2 + Classpath
# Exception) into third_party\helpers\SDR Scripts\jre.
#
# Run once per fresh tree:
#   powershell -ExecutionPolicy Bypass -File setup-denkovi-jre.ps1
#
# Future builds copy the existing folder; the script exits immediately when
# the runtime is already there.

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$destination = Join-Path $root "third_party\helpers\SDR Scripts\jre"
$java = Join-Path $destination "bin\java.exe"

if (Test-Path $java) {
    Write-Host "Temurin JRE already present in $destination"
    exit 0
}

$url = "https://api.adoptium.net/v3/binary/latest/17/ga/windows/x64/jre/hotspot/normal/eclipse?project=jdk"
$zip = Join-Path $env:TEMP "temurin-jre.zip"
$extract = Join-Path $env:TEMP "temurin-jre-extract"

Write-Host "Downloading the Eclipse Temurin JRE..."
$ProgressPreference = "SilentlyContinue"
Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing

if (Test-Path $extract) { Remove-Item $extract -Recurse -Force }
Expand-Archive -Path $zip -DestinationPath $extract -Force
$inner = Get-ChildItem $extract -Directory | Select-Object -First 1
if (-not $inner) { throw "Unexpected JRE archive layout." }

if (Test-Path $destination) { Remove-Item $destination -Recurse -Force }
New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
Move-Item $inner.FullName $destination

Remove-Item $zip -Force -ErrorAction SilentlyContinue
Remove-Item $extract -Recurse -Force -ErrorAction SilentlyContinue

if (-not (Test-Path $java)) { throw "The JRE did not install at $destination." }
Write-Host "Installed the Temurin JRE in $destination"
Write-Host "The deploy tree and MSI will include it in SDR Scripts\jre."

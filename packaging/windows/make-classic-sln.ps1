# Generate a classic Visual Studio .sln from the .slnx that CMake's VS 18
# generator writes.  Both live in the same build directory and reference the
# same .vcxproj files, so either can be opened.
#
#   powershell -ExecutionPolicy Bypass -File packaging\windows\make-classic-sln.ps1
#
# The MSI project is included in the solution but is not part of the default
# Build (build it explicitly as NereusSDR-MSI).

[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [string]$SolutionName = "NereusSDR"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $BuildDir) { $BuildDir = Join-Path $root "build-vs" }

$slnx = Join-Path $BuildDir "$SolutionName.slnx"
$sln = Join-Path $BuildDir "$SolutionName.sln"
if (-not (Test-Path $slnx)) { throw "Solution not found: $slnx" }

[xml]$xml = Get-Content $slnx -Raw
$projects = @($xml.Solution.Project)
if ($projects.Count -eq 0) { throw "No projects in $slnx" }

$configs = @()
foreach ($bt in @($xml.Solution.Configurations.BuildType)) {
    foreach ($pf in @($xml.Solution.Configurations.Platform)) {
        $configs += "$($bt.Name)|$($pf.Name)"
    }
}
if ($configs.Count -eq 0) {
    $configs = @("Debug|x64", "Release|x64", "MinSizeRel|x64", "RelWithDebInfo|x64")
}

# Projects that are IDE-only or explicit tools, not part of a normal Build.
$noBuild = @("INSTALL.vcxproj", "NereusCore_other_files.vcxproj", "$SolutionName-MSI.vcxproj")

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("Microsoft Visual Studio Solution File, Format Version 12.00")
$lines.Add("# Visual Studio Version 18")
$lines.Add("VisualStudioVersion = 18.0.0.0")
$lines.Add("MinimumVisualStudioVersion = 10.0.40219.1")

foreach ($p in $projects) {
    $path = $p.Path -replace '/', '\'
    $name = [IO.Path]::GetFileNameWithoutExtension($path)
    $guid = "{" + $p.Id.ToUpperInvariant() + "}"
    $type = "{" + $p.Type.ToUpperInvariant() + "}"
    $lines.Add("Project(`"$type`") = `"$name`", `"$path`", `"$guid`"")
    $lines.Add("EndProject")
}

$lines.Add("Global")
$lines.Add("`tGlobalSection(SolutionConfigurationPlatforms) = preSolution")
foreach ($cfg in $configs) { $lines.Add("`t`t$cfg = $cfg") }
$lines.Add("`tEndGlobalSection")
$lines.Add("`tGlobalSection(ProjectConfigurationPlatforms) = postSolution")
foreach ($p in $projects) {
    $guid = "{" + $p.Id.ToUpperInvariant() + "}"
    $buildDefault = $noBuild -notcontains $p.Path
    $buildChildren = @($p.Build)
    foreach ($cfg in $configs) {
        $buildIt = $buildDefault
        $cfgName = $cfg.Split("|")[0]
        foreach ($b in $buildChildren) {
            if ($b.Project -eq "false") {
                $sel = $b.Solution
                if ($null -eq $sel -or $sel -eq "*|*" -or $sel -eq $cfg -or $sel -eq "$cfgName|*") {
                    $buildIt = $false
                }
            }
        }
        $lines.Add("`t`t$guid.$cfg.ActiveCfg = $cfg")
        if ($buildIt) { $lines.Add("`t`t$guid.$cfg.Build.0 = $cfg") }
    }
}
$lines.Add("`tEndGlobalSection")
$lines.Add("`tGlobalSection(SolutionProperties) = preSolution")
$lines.Add("`t`tHideSolutionNode = FALSE")
$lines.Add("`tEndGlobalSection")
$lines.Add("EndGlobal")

[IO.File]::WriteAllLines($sln, $lines, (New-Object Text.UTF8Encoding($false)))
Write-Host "Wrote $sln"

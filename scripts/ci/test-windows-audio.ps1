# no-port-check: NereusSDR-original Windows CI runtime staging.
# 2026-10-31 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
# 2026-11-16 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code: find
# the OpenSSL DLLs by pattern, since MSYS2 moved from OpenSSL 3 to 4.
# Git Bash adds Git's UCRT C++ runtime ahead of Qt's MinGW runtime. Keep
# test dependencies explicit instead of inheriting unrelated runner DLLs.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build = Join-Path $root 'build'
$runtime = Join-Path $root 'windows-audio-test-runtime'
if (Test-Path -LiteralPath $runtime) { throw "Unexpected existing runtime: $runtime" }
foreach ($variable in @('QT_ROOT_DIR', 'IQTA_TOOLS', 'OPENSSL_ROOT_DIR')) {
    if (![Environment]::GetEnvironmentVariable($variable)) { throw "Missing provider root: $variable" }
}
New-Item -ItemType Directory -Path $runtime | Out-Null
# The OpenSSL DLL names carry its major version (libcrypto-3-x64.dll for 3.x,
# libcrypto-4-x64.dll for 4.x), and MSYS2 moves to new majors on its own
# schedule. Take the single one that is installed, whatever its major.
function Find-OpenSslDll([string]$stem) {
    $bin = Join-Path $env:OPENSSL_ROOT_DIR 'bin'
    $found = @(Get-ChildItem -LiteralPath $bin -File -Filter "$stem-*-x64.dll" |
        Where-Object { $_.Name -match "^$stem-\d+-x64\.dll$" })
    if ($found.Count -ne 1) {
        throw "Expected one $stem-<major>-x64.dll in ${bin}, found $($found.Count): $($found.Name -join ', ')"
    }
    return $found[0].Name
}
$groups = @(
    @{ root=(Join-Path $env:QT_ROOT_DIR 'bin'); names=@(
        'Qt6Core.dll','Qt6Gui.dll','Qt6Multimedia.dll','Qt6Network.dll',
        'Qt6SerialPort.dll','Qt6Svg.dll','Qt6Test.dll','Qt6WebSockets.dll','Qt6Widgets.dll') },
    @{ root=(Join-Path $env:IQTA_TOOLS 'mingw1310_64/bin'); names=@(
        'libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll') },
    # Copy only OpenSSL, preserving Qt's matching compiler runtime. Do not
    # prepend MSYS2/bin: its newer C++ runtime is incompatible with Qt here.
    @{ root=(Join-Path $env:OPENSSL_ROOT_DIR 'bin'); names=@(
        (Find-OpenSslDll 'libcrypto'),(Find-OpenSslDll 'libssl')) },
    @{ root=(Join-Path $root 'third_party/fftw3/bin'); names=@('libfftw3-3.dll','libfftw3f-3.dll') }
)
$manifest = foreach ($group in $groups) {
    foreach ($name in $group.names) {
        $source = Join-Path $group.root $name
        $destination = Join-Path $runtime $name
        if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing runtime DLL: $source" }
        $digest = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        Copy-Item -LiteralPath $source -Destination $destination
        if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $digest) {
            throw "Runtime copy mismatch: $name"
        }
        [ordered]@{ name=$name; source=$source; destination=$destination; sha256=$digest }
    }
}
if (@($manifest).Count -ne 16) { throw 'Expected exactly sixteen controlled runtime DLLs' }
$env:PATH = "$runtime;$env:PATH"
[ordered]@{ path=$env:PATH; providers=@($manifest) } | ConvertTo-Json -Depth 5 |
    Tee-Object -FilePath (Join-Path $build 'windows-audio-runtime-manifest.json')
$selection = '^(tst_wasapi_notifier_lifetime(_native)?|tst_wasapi_policy|tst_audio_device_catalog|tst_station_foreign_lock|tst_station_handover)$'
& ctest --version
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ctest --test-dir $build -N -V -R $selection |
    Tee-Object -FilePath (Join-Path $build 'windows-audio-selected.txt')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ctest --test-dir $build -V --interactive-debug-mode 0 -R $selection `
    --output-on-failure --no-tests=error --output-junit (Join-Path $build 'windows-audio-tests.xml')
exit $LASTEXITCODE

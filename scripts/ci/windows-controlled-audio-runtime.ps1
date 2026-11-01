# no-port-check: NereusSDR-original preserved-binary runtime diagnostic.
# 2026-10-31 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
# No configure/build, product installation, station access or native-child bypass.
[CmdletBinding()]
param(
    [string]$Root = $env:GITHUB_WORKSPACE,
    [string]$RuntimeDirectory = '',
    [string]$EvidenceDirectory = '',
    [string]$AuditHelper = '',
    [switch]$StageOnly
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$Root = [IO.Path]::GetFullPath($Root)
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $Root 'controlled-audio-runtime' }
if (!$EvidenceDirectory) { $EvidenceDirectory = Join-Path $Root 'runtime-evidence/controlled' }
if (!$AuditHelper) { $AuditHelper = Join-Path $Root 'scripts/ci/windows-audio-runtime-replay.py' }
$RuntimeDirectory = [IO.Path]::GetFullPath($RuntimeDirectory)
$EvidenceDirectory = [IO.Path]::GetFullPath($EvidenceDirectory)
$rootPrefix = $Root.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
foreach ($directory in @($RuntimeDirectory, $EvidenceDirectory)) {
    if (!$directory.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Diagnostic output must stay inside its owned workspace: $directory"
    }
    foreach ($protected in @('build', 'third_party', '.git')) {
        $protectedPath = Join-Path $Root $protected
        if ($directory.Equals($protectedPath, [StringComparison]::OrdinalIgnoreCase) -or
            $directory.StartsWith($protectedPath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Do not modify preserved artifact/source directories: $directory"
        }
    }
}
if (Test-Path $RuntimeDirectory) { throw "Refusing to reuse an existing runtime folder: $RuntimeDirectory" }
if (Test-Path $EvidenceDirectory) { throw "Refusing to overwrite prior evidence: $EvidenceDirectory" }
New-Item -ItemType Directory $RuntimeDirectory, $EvidenceDirectory | Out-Null
$status = [ordered]@{ state = 'started'; artifactRun = 41258145196; auditExit = $null; testExit = $null; error = $null }
$previousPath = $env:PATH
function Require-Hash([string]$Path, [string]$Expected) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing required file: $Path" }
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Expected) { throw "Unexpected bytes for $Path : $actual (expected $Expected)" }
    return $actual
}
try {
    foreach ($variable in @('QT_ROOT_DIR', 'IQTA_TOOLS', 'OPENSSL_ROOT_DIR')) {
        if (![Environment]::GetEnvironmentVariable($variable)) { throw "Required recorded provider root absent: $variable" }
    }
    $qtBin = Join-Path $env:QT_ROOT_DIR 'bin'
    $gccBin = Join-Path $env:IQTA_TOOLS 'mingw1310_64/bin'
    $sslBin = Join-Path $env:OPENSSL_ROOT_DIR 'bin'
    $compiler = Join-Path $gccBin 'c++.exe'
    $compilerHash = Require-Hash $compiler 'cd65c087ad69a877e5a105c607cc910abfabcc52ba7e9448572f36567b51698c'
    $compilerVersion = (& $compiler -dumpfullversion | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $compilerVersion -ne '13.1.0') { throw 'Recorded GCC 13.1.0 compiler identity not available' }
    $qtVersion = (Get-Item (Join-Path $qtBin 'Qt6Core.dll')).VersionInfo.FileVersion
    if ($qtVersion -notmatch '^6\.8\.3([. ]|$)') { throw "Unexpected Qt version: $qtVersion" }
    $opensslHeader = Join-Path $env:OPENSSL_ROOT_DIR 'include/openssl/opensslv.h'
    if ((Get-Content -LiteralPath $opensslHeader -Raw) -notmatch 'OPENSSL_VERSION_STR\s+"3\.6\.5"') { throw 'Recorded MSYS2 OpenSSL 3.6.5 header not available' }
    # These hashes are actual provider bytes retained in replay artifact13065488845,
    # not basename/version guesses. Copy only this explicit list, never MSYS2/bin/*.
    $providers = @(
        @{ name='Qt6Core.dll'; root=$qtBin; sha='06d84a2e85bda1a651c65f7d159b63bee2458a8b50274da52b70c9e3e9188e97' },
        @{ name='Qt6Gui.dll'; root=$qtBin; sha='ef458a379a4b588fd6042a3bcc7d0b9631352745600ae933a7faec4c851b16bb' },
        @{ name='Qt6Multimedia.dll'; root=$qtBin; sha='29c4d08d77be58831a1e975d45cec67354bee1c9c79aeaeca9ee819dabdf8a10' },
        @{ name='Qt6Network.dll'; root=$qtBin; sha='3158e8979ce542c84a61257cfe03983144241ab4d11d8099c08db7ecf5cf8b83' },
        @{ name='Qt6SerialPort.dll'; root=$qtBin; sha='806c75d115d1b502b39be0ba05527c9c91777adec684dee8efd313a988d60988' },
        @{ name='Qt6Svg.dll'; root=$qtBin; sha='593ed06864d4b1c31394aeb429b2b799c1671c77e1646c6fb56e12e3f767d290' },
        @{ name='Qt6Test.dll'; root=$qtBin; sha='bb51aa62555f9dabb99d60b4068ad6de5c4c135e89567dba77cf95bf54554dac' },
        @{ name='Qt6WebSockets.dll'; root=$qtBin; sha='ddc8bab3c5420d0b782d0c741e60dde18deb3de9bd3a67c50264c8b077ad2fa5' },
        @{ name='Qt6Widgets.dll'; root=$qtBin; sha='9076b021a060dc8c0ae5a50f3dbf9c23018d7729fe102f7a75adbdf863fa2d0d' },
        @{ name='libgcc_s_seh-1.dll'; root=$gccBin; sha='5e27589147caa6d7a30f4cc058a14c455dc62233901ffff9a1797ede425e9f86' },
        @{ name='libstdc++-6.dll'; root=$gccBin; sha='8013488c5528bad7966ca07f3ea2e7a9b743cacb258fe76b46a326f821cc83b0' },
        @{ name='libwinpthread-1.dll'; root=$gccBin; sha='c7c7dced65fff71c7bb61f80c771c562f533d26d72722d9d6091129a3b03d5ce' },
        @{ name='libcrypto-3-x64.dll'; root=$sslBin; sha='26c5c4804d14049dc50e7d1a7ba5fd479c1bcee3ae5c3942a7f069f5159a4745' },
        @{ name='libssl-3-x64.dll'; root=$sslBin; sha='a3ddb63a654e05844cf59d463c9557eed7ae675d59fd2c47f3415f5e1fc067f5' },
        @{ name='libfftw3-3.dll'; root=(Join-Path $Root 'third_party/fftw3/bin'); sha='001a835d2f25ac0661580d67db5622157c28481c06a7f3ef45ad939839bf54a1' },
        @{ name='libfftw3f-3.dll'; root=(Join-Path $Root 'third_party/fftw3/bin'); sha='42ca18fff35dd12890e04478bc990005b3969cb744f6843976bd436ccd7f0a4c' }
    )
    $productFiles = @{
        'build/libNereusCore.dll'='4fab90090f83c3948402765cb59e055dc55d34c67c94130928e2a31aa4122e62';
        'build/libNereusSDRLib.dll'='be4c9da9e7cc9187829f7910bc43caa79f0a312ce79cae99e365f0438fd13c65';
        'build/librade.dll'='0ae6ae9ccfa786f260afa60aa069f6d35d6b95072d2c5e1aae44bd473c89aba7';
        'build/tests/tst_wasapi_policy.exe'='bf600b086d66b1f8526905d5c177ea2467fa1782690f014a6ba01e0635649390';
        'build/tests/tst_wasapi_notifier_lifetime.exe'='f22eb380104d65687fdea2900cfecba23ef60543e8dd15ec779bbfc6a94cf8a1';
        'build/tests/tst_audio_device_catalog.exe'='5c1eacdce0160e39c074d4696b90b037e8c8eebb5a562dde08ee2a4e33e5b621'
    }
    $products = @()
    if (!$StageOnly) {
        $products = foreach ($entry in $productFiles.GetEnumerator()) {
            $path = Join-Path $Root $entry.Key
            [ordered]@{ path=$path; sha256=(Require-Hash $path $entry.Value) }
        }
    }
    $copied = foreach ($provider in $providers) {
        $source = Join-Path $provider.root $provider.name
        $digest = Require-Hash $source $provider.sha
        $destination = Join-Path $RuntimeDirectory $provider.name
        Copy-Item -LiteralPath $source -Destination $destination
        $null = Require-Hash $destination $provider.sha
        [ordered]@{ name=$provider.name; source=$source; destination=$destination; sha256=$digest }
    }
    $env:PATH = "$RuntimeDirectory;$previousPath"
    [ordered]@{
        providerEvidenceArtifact=13065488845; compiler=$compiler; compilerSha256=$compilerHash;
        compilerVersion=$compilerVersion; qtVersion=$qtVersion; opensslVersion='3.6.5';
        previousPath=$previousPath; controlledPath=$env:PATH; providers=@($copied); products=@($products)
    } | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $EvidenceDirectory 'controlled-manifest.json') -Encoding utf8
    if ($StageOnly) {
        # Reusable CI staging portion: source/destination byte verification,
        # version checks and manifest only; no fixed product hashes or probes.
        $status.state = 'staged'
    }
    else {
    & python $AuditHelper audit --root $Root --out $EvidenceDirectory 2>&1 |
        Tee-Object -FilePath (Join-Path $EvidenceDirectory 'audit-console.txt')
    $status.auditExit = $LASTEXITCODE
    if ($status.auditExit -ne 0) { throw "Controlled provider import audit failed: $($status.auditExit)" }
    $audit = Get-Content (Join-Path $EvidenceDirectory 'pe-runtime-audit.json') -Raw | ConvertFrom-Json -AsHashtable
    # Prove that every staged provider is the first selected normal-search
    # candidate; ambient MySQL/Git/MSYS2 files must not fill one of these names.
    foreach ($provider in $providers) {
        $candidates = $audit['normal_search_candidates'][$provider.name.ToLowerInvariant()]
        if (!$candidates -or
            !([IO.Path]::GetFullPath($candidates[0])).Equals(
                (Join-Path $RuntimeDirectory $provider.name), [StringComparison]::OrdinalIgnoreCase)) {
            throw "Controlled provider was not selected first: $($provider.name)"
        }
        $edges = @($audit.matched_imports | Where-Object dll -IEQ $provider.name)
        if (!$edges.Count) { throw "No verified import requests for staged provider: $($provider.name)" }
        foreach ($edge in $edges) {
            if ($edge.provider_sha256 -ne $provider.sha -or
                !([IO.Path]::GetFullPath($edge.provider)).Equals(
                    (Join-Path $RuntimeDirectory $provider.name), [StringComparison]::OrdinalIgnoreCase)) {
                throw "Unexpected selected provider for $($provider.name): $($edge.provider)"
            }
        }
    }
    $selection = '^(tst_wasapi_notifier_lifetime(_native)?|tst_wasapi_policy|tst_audio_device_catalog)$'
    $listing = & ctest --test-dir (Join-Path $Root 'build') --show-only=json-v1 -R $selection | Out-String
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the preserved CTest registrations' }
    $listing | Set-Content (Join-Path $EvidenceDirectory 'selected.json') -Encoding utf8
    $testNames = @(($listing | ConvertFrom-Json).tests | ForEach-Object name | Sort-Object)
    $expectedNames = @('tst_audio_device_catalog','tst_wasapi_notifier_lifetime','tst_wasapi_notifier_lifetime_native','tst_wasapi_policy')
    if (($testNames -join ';') -ne ($expectedNames -join ';')) { throw 'Controlled replay did not select exactly the four approved registrations' }
    & ctest --test-dir (Join-Path $Root 'build') -N -V -R $selection |
        Set-Content (Join-Path $EvidenceDirectory 'selected.txt') -Encoding utf8
    if ($LASTEXITCODE -ne 0) { throw 'Cannot capture selected CTest environments' }
    & ctest --test-dir (Join-Path $Root 'build') -V --interactive-debug-mode 0 -R $selection `
        --output-on-failure --no-tests=error --output-junit (Join-Path $EvidenceDirectory 'tests.xml') 2>&1 |
        Tee-Object -FilePath (Join-Path $EvidenceDirectory 'test-console.txt')
    $status.testExit = $LASTEXITCODE
    if ($status.testExit -ne 0) { throw "Controlled parent CTest replay failed: $($status.testExit)" }
    $status.state = 'passed'
    }
}
catch {
    $status.state = 'failed'
    $status.error = $_.Exception.Message
    throw
}
finally {
    # StageOnly leaves the controlled PATH in this PowerShell process so a
    # caller can run CTest in the same CI step. It does not change GITHUB_PATH
    # or another step/process. A failed stage restores the prior environment.
    if (!$StageOnly -or $status.state -ne 'staged') { $env:PATH = $previousPath }
    $status | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $EvidenceDirectory 'status.json') -Encoding utf8
}

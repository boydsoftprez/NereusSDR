# Windows Visual Studio + MSI

CMake generates:

```
build-vs\NereusSDR.slnx   Visual Studio 2026 solution
build-vs\NereusSDR.sln    classic solution, generated from the .slnx
```

If CMake is reconfigured, regenerate the classic solution with:

```powershell
powershell -ExecutionPolicy Bypass -File packaging\windows\make-classic-sln.ps1
```

Open either solution, select **Release | x64**, and build. A full solution
Rebuild has been verified with zero errors in Release and RelWithDebInfo:

```powershell
msbuild build-vs\NereusSDR.sln /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m
msbuild build-vs\NereusSDR.sln /t:Rebuild /p:Configuration=RelWithDebInfo /p:Platform=x64 /m
``` The main application targets are
`NereusSDR`, `NereusSDRLib`, `NereusCore`, `nereusd`, `nereus-audio-capture` and
the vendored `rade` DLL.

## MSI target

The solution also contains a custom target:

```
NereusSDR-MSI
```

Build that target in **Release** (or run the command below) to produce:

```
bin\Release\...                                      runnable deploy tree
bin\Installers\NereusSDR-2026.11.0-Windows-x64.msi   installer
```

Command line equivalent:

```powershell
cmake --build build-vs --config Release --target NereusSDR-MSI
```

The target runs `packaging\windows\build-msi.ps1`, which:

1. stages the app, Core, helper, `rade.dll`, FFTW and OpenSSL DLLs in
   `bin\Release`, plus the bundled station helpers, start/exit scripts,
   Thetis container files and the `.dat` bridge;
2. runs `windeployqt` for the Qt DLLs and plugins;
3. copies the third-party licences and rnnoise models;
4. checks every shipped EXE/DLL with `dumpbin` for unresolved imports;
5. harvests the tree with WiX `heat`, then links the MSI with WiX
   `candle`/`light`.

The MSI installs to `Program Files\NereusSDR`, adds Start Menu and Desktop
shortcuts, writes the Apps & features entry, and adds the OpenHPSDR inbound-UDP
firewall rule.

## Bundled helpers, scripts and container files

The deploy tree and MSI carry these folders beside `NereusSDR.exe`:

```
WaveNode\               Wavenode.exe (static Release) + WavenodeDirect.ini
HF-Auto MMIO\           HFAuto_MMIO.exe + .NET 8 runtime (the frontend Nereus starts)
HF-Auto Controller\     HFAutoController.exe, config, .NET dependencies, licence
SDR Scripts\            nereus-start.bat, nereus-stop.bat, Denkovi\ (jar + libs), jre\
Container Files\        Main_Set.dat, Palstar_HF-Auto.dat, WaveNode-LATEST.dat, ...
thetis-dat-bridge.exe   .NET envelope bridge for Thetis container .dat files
```

The HF-Auto helper entry launches **HF-Auto MMIO**, not the controller directly.
The MMIO frontend seeds/supervises the original controller as its serial engine;
both run minimized. Nereus feeds Tuner Follow and the TX frequency to the
frontend on `127.0.0.1:12052` and reads the `QxG4.*` JSON from `12050`. The
frontend owns CAT `13013` for the controller, so Nereus does not bind it.

The deploy also copies the Visual C++ x64 runtime beside the executables and
the Eclipse Temurin JRE under `SDR Scripts\jre`, so a target machine does not
need the VC++ redistributable or Java installed separately. A seeded per-user
script set skips the 130 MB JRE and points `jre-home.txt` at the install copy.

On first use, when a helper or script is **enabled**, the desktop app seeds its
per-user copy into `%LOCALAPPDATA%\NereusSDR\helpers\`. Nothing is copied for a
feature the user never selects. Existing files are never overwritten, so an
edited relay script survives an update. WaveNode and HF-Auto are checkable
entries in **Containers → Helpers** (with a status-bar segment); each opens its
own container imported from the matching `Container Files` `.dat`, and
**Reset helper containers** there or on the **Setup → General → Options** page
removes old or combined containers and re-imports the Thetis layouts. The same
Setup page selects the start/exit scripts and the auto-connect delay, and
offers **Remove installed files** for either helper or the scripts.

`thetis-dat-bridge.exe` is built from
`tools\thetis-dat-bridge\ThetisDatBridge.cs` with the .NET Framework C#
compiler (`csc.exe`) as a POST_BUILD step of `NereusSDR`. Without `csc.exe`
the app still builds and uses native `.nscontainer` files; `.dat` import and
export are unavailable.

The optional Denkovi relay scripts need a Java runtime. `setup-denkovi-jre.ps1`
is a one-time prerequisite that fetches the Eclipse Temurin JRE into
`third_party\helpers\SDR Scripts\jre`; the deploy and MSI then ship it, so the
installed machine needs no separate Java installation. The script exits
immediately when the runtime is already present.

## Debug tree

A full MSVC **Debug** link is not possible for this tree: automatic DLL exports
plus the Debug object set exceed LINK's 65,535-member limit (the project's own
CMake comments record this; `/Zc:inline` only helps optimized configurations).
Use **RelWithDebInfo** when you need PDBs and a debuggable binary:

```powershell
cmake --build build-vs --config RelWithDebInfo --target NereusSDR
powershell -ExecutionPolicy Bypass -File packaging\windows\build-msi.ps1 `
    -Config RelWithDebInfo -DeployDir bin\Debug -SkipMsi
```

That stages the RelWithDebInfo binaries and their PDBs in `bin\Debug`, with the
Release Qt runtime, so the folder has the same runnable layout as
`bin\Release`.

## Requirements

- Visual Studio 2026 with the C++ workload
- Qt 6.12 `msvc2022_64`
- WiX Toolset v3.14
- OpenSSL 3 (`C:\Program Files\OpenSSL-Win64`)
- The first CMake configure downloads FFTW 3.3.5

## DFNR / DeepFilterNet3

`setup-deepfilter.ps1` is a **one-time prerequisite**, not part of a normal
build. Once these exist:

```
third_party\deepfilter\lib\windows-x86_64\deepfilter.dll
third_party\deepfilter\models\DeepFilterNet3_onnx.tar.gz
```

CMake enables DFNR automatically, compiles it into `NereusCore`, and the MSI
target packages the DLL and model. Future builds and rebuilds do not rerun the
setup. Rerun it only for a fresh tree/machine or if those files are deleted;
without them CMake falls back to “DFNR disabled” and still builds.

## Visual Studio generator notes

Two vendored-build fixes make the multi-config generator work:

- `third_party/rade/cmake/BuildOpus.cmake` resolves `opus.lib` under the
  per-configuration directory (`Release\`, `Debug\`).
- `third_party/libspecbleach/CMakeLists.txt` invokes the MSVC librarian by its
  full CMake path instead of relying on `lib.exe` being on `PATH`.

`/bigobj` is enabled for C++ so the large generated moc units build in Debug.

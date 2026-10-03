# NereusSDR

Cross-platform C++20 / Qt6 SDR console for every OpenHPSDR Protocol 1 and 2
radio (ANAN line, Hermes Lite 2): multiple slices and panadapters, and a
remote Core with desktop and phone clients. The client does ALL signal
processing; the radio is an ADC/DAC with network transport.

## Building on OpenHPSDR

| Need | Reference |
| --- | --- |
| Radio behavior, DSP, protocol handling | Thetis, mi0bot-Thetis (HL2), WDSP |
| Another client's approach (C/GTK, Linux, macOS, Pi) | piHPSDR, deskHPSDR |
| Hardware facts (DDCs, ADCs, clocks) | docs/protocols/, TAPR and Anvelina gateware (facts only) |
| Qt6 structure, UI patterns | AetherSDR |
| Digital voice, PSK Reporter | freedv-gui |

**Study, then choose.** Read how the references do it and summarize it in the
plan or PR. Then **port** when it fits as written (faithfully, under the rules
below), or **design** for NereusSDR's slices, panadapters, remote Core and
clients, using the reference as the basis and saying in the PR what changed.

Never guess facts: protocol layouts, enum values, hardware limits and WDSP
signatures come from a source, with a cite. Can't find it? Stop and ask. A
bug an upstream shares is fixed in our copy, with a note on why ours differs.

**Before any port, read [docs/attribution/HOW-TO-PORT.md](docs/attribution/HOW-TO-PORT.md).**

### License rules for ported code (non-negotiable)

In the same commit that introduces a port, the NereusSDR file header gets the
upstream file's header **byte-for-byte**: every `Copyright (C)` line, the GPL
permission block, the Samphire dual-licence statement if present, plus a
"Modification history (NereusSDR)" block (date, human author, AI tooling).
Headers differ per upstream file and are not interchangeable; a file porting
from several gets each one under `// --- From [filename] ---`. New ports also
add a row to `docs/attribution/THETIS-PROVENANCE.md` (or
`FREEDV-GUI-PROVENANCE.md`). Missing notices are a GPL compliance bug that
blocks the PR.

### Inline comment preservation (SHIP-BLOCKING)

Every `//` comment inside ported logic is copied verbatim, above all developer
tags (`//MW0LGE`, `//-W2PA`, `//[2.10.3.13]MW0LGE`, `//DH1KLM`, `//MI0BOT`, ...),
behavioral notes and TODO/FIXME. If restructuring moves the line, put the
comment on the nearest equivalent line with
`[original inline comment from file:line]`.
`scripts/verify-inline-tag-preservation.py` enforces this in pre-commit and CI.

### Cites, constants, WDSP

* In ported code, every block and constant carries `// From Thetis file:line [v2.10.3.15]`
  (current pin: v2.10.3.15 / `3759d09`; `[@shortsha]` between releases; get it once per session with
  `git -C ../Thetis describe --tags`).
* Keep constants and magic numbers exactly (`0.98f` stays `0.98f`) as named
  `constexpr` with the cite.
* WDSP calls must match the name, parameter order and types in
  `third_party/wdsp/`; when porting a Thetis feature, ranges, defaults and
  scaling come from its callsite.
* Hardware facts (DDC count, board byte, clocks) may cite the gateware at
  `../TAPR-OpenHPSDR-Firmware/` (`e7c6584`) or `../n1gp-Anvelina_PROIII/`
  (`8e86a61`), both pinned; never pull. Cite facts only; ask before porting
  Verilog logic. Details in HOW-TO-PORT.md.
* piHPSDR (`../pihpsdr/`, pinned `4aa95c5`) and deskHPSDR (`../deskhpsdr/`, pinned `f3d857c`) are references like Thetis: facts cite them, and code ported from them keeps their GPL headers and attribution (HOW-TO-PORT.md).

## Agent boundaries

May fix autonomously: bugs with a clear root cause, OpenHPSDR protocol
compliance, build/CI breakage.

Must NOT change without the maintainer: visual design, UX behavior, architecture
(threads, signal routing, dependencies), feature scope, user-facing defaults,
DSP parameters. When in doubt, implement and flag
the design decision in the PR.

Also: never propose Wine/CrossOver; flag anything touching the core RX path
(I/Q → WDSP → audio); ask for pcaps when protocol behavior is unclear; use
OpenHPSDR specs, not SmartSDR.

## C++ style

Full conventions in [CONTRIBUTING.md](CONTRIBUTING.md). Non-negotiables:

* No `goto`, no raw `new`/`delete` (unique_ptr or Qt parent), no `#define`
  constants (`constexpr`), braces on all control flow, `auto` only when the
  type is obvious.
* Naming: `PascalCase` classes, `camelCase` methods, `kPascalCase` constants,
  `m_camelCase` members.
* Platform guards `Q_OS_WIN` / `Q_OS_MAC` / `Q_OS_LINUX`, never `_WIN32` /
  `__APPLE__`.
* Errors: `qCWarning(lcCategory)`, no exceptions.
* Cross-thread DSP parameters are `std::atomic`; never hold a mutex in the
  audio callback.
* Don't remove code you didn't add.

## Versioning

Product releases use CalVer `YYYY.M.counter`: full year, unpadded month,
counter starting at zero and increasing within that month. Tags are
`vYYYY.M.counter`, with `-rcN` for release candidates; candidates do not
consume a final-release counter. `CMakeLists.txt` owns the product version.
Use `scripts/release-version.py next`, `last`, and `check` rather than
calculating from tag sort order. The iOS client uses independent `ios-v`
tags/counters and does not invoke the desktop release workflow.

Product version, settings schema, driver bundle and station wire protocol
versions are independent. Keep the PGXL discovery beacon at `0.5.2`; never
substitute the product calendar version into that protocol field.

## Settings

**`AppSettings`, never `QSettings`** (`src/core/AppSettings.h`, XML at
`~/.config/NereusSDR/NereusSDR.settings`). PascalCase keys; booleans are the
strings `"True"` / `"False"`.

* Radio-authoritative, never persisted: antenna selection.
* Saved and sent to the radio on connect, as Thetis does: ADC attenuation and
  preamp (console.cs:2174-2179), per-band TX power (console.cs:3089-3093,
  4903-4910, 17528-17542). Connecting never keys.
* Per-MAC under `hardware/<mac>/...`: sample rate, active RX count.
* Client-authoritative, persisted: VFO, mode, filter, DSP settings, layout, UI
  and display preferences.

GUI↔model sync: model setters emit, the connection sends; guard echo loops with
`m_updatingFromModel` or `QSignalBlocker` (AetherSDR pattern).

## Architecture

`src/core/` (protocol, audio, DSP), `src/models/` (RadioModel, SliceModel,
PanadapterModel), `src/gui/` (MainWindow, SpectrumWidget, applets). Threads:
main (GUI + all models), connection (UDP), audio (WDSP + output), spectrum
(FFT). Cross-thread traffic is auto-queued signals only. Details and data flow:
[docs/architecture/overview.md](docs/architecture/overview.md).

**Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.**
`tst_core_has_no_gui_includes` enforces it; extract an interface instead (as
`ISpectrumSink` did).

## Build and test

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
./build/NereusSDR
```

The build also produces the headless `nereusd`, installed only with
`--component nereusd`; install notes in [README.md](README.md).
Dependencies: [README.md](README.md) "Building from Source". **Read
[docs/development/fast-test-loop.md](docs/development/fast-test-loop.md)
before running tests**; build single tests, never the whole suite by default.
First launch generates FFTW wisdom (~15 min), cached in `~/.config/NereusSDR/`.

## Where things live

* Upstreams, cloned as siblings of the repo root:
  `../Thetis/` (github.com/ramdor/Thetis), `../mi0bot-Thetis/` (authoritative
  for HL2), `../AetherSDR/` (github.com/ten9876/AetherSDR), `../freedv-gui/`
  (github.com/drowe67/freedv-gui; RADE steps, FreeDV + PSK Reporter),
  `../pihpsdr/` (github.com/dl1ycf/pihpsdr), `../deskhpsdr/`
  (github.com/dl1bz/deskhpsdr), `../n1gp-Anvelina_PROIII/`
  (github.com/n1gp/Anvelina_PROIII, pinned), `../TAPR-OpenHPSDR-Firmware/`
  (github.com/TAPR/OpenHPSDR-Firmware, pinned).
* Vendored: `third_party/wdsp/` (WDSP 2.10, TAPR b02d5bac), `third_party/rade/` (radae_nopy
  b289102, BSD-2), `third_party/r8brain/` (MIT resampler), `third_party/fftw3/`
  (Windows DLL).
* Version: `CMakeLists.txt`. Phase status, release history, plan index:
  [docs/development/project-status.md](docs/development/project-status.md),
  [CHANGELOG.md](CHANGELOG.md), [docs/MASTER-PLAN.md](docs/MASTER-PLAN.md).
* Design docs and plans: `docs/architecture/`. Protocols: `docs/protocols/`.

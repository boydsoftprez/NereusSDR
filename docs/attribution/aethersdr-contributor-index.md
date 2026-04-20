# AetherSDR Contributor Index

Per-source catalog of contributors named in `ten9876/AetherSDR` source
files, built during NereusSDR v0.2.0 compliance Phase 4 (Task 25a).

Source of record: `/Users/j.j.boyd/AetherSDR/` (shallow clone of
`https://github.com/ten9876/AetherSDR`, HEAD `a29ff40` — "Fix Aurora (AU-)
power meter showing 0-120W instead of 0-600W", AetherSDR v0.8.8).

Scope: the AetherSDR counterparts to the 158 NereusSDR source files that
cite AetherSDR in their Modification-History block or in inline `// From
AetherSDR ...` comments. Sibling index to
`thetis-contributor-index.md`.

## Upstream

- Repo: <https://github.com/ten9876/AetherSDR>
- Primary author: **Jeremy** (callsign **KK7GWY**, GitHub `ten9876`)
- Project tagline (README): "A Linux-native client for FlexRadio Systems
  transceivers"
- License: **GPLv3** (`LICENSE` file at repo root is the verbatim
  "GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007". README badge
  reads "License: GPL v3" and links to
  <https://www.gnu.org/licenses/gpl-3.0>.)
- No explicit "or (at your option) any later version" wording was found
  on the top-level LICENSE header — the file is the canonical GPLv3 text
  verbatim. The About dialog (`src/gui/MainWindow.cpp:3253-3258`) reads
  "&copy; 2026 AetherSDR Contributors / Licensed under GPLv3".
- CONTRIBUTING.md §License: "licensed under the GNU General Public
  License v3.0".
- Cloned locally for audit at `/Users/j.j.boyd/AetherSDR/`.

## Compatibility note (GPLv3 → NereusSDR GPLv3)

NereusSDR is GPLv3. AetherSDR is GPLv3. The two licenses are identical,
so downstream relicensing is not a concern. The compliance question
reduces to **attribution preservation**: under GPLv3 §5 we must preserve
"all notices" on the AetherSDR material we copy/port, and must mark
"the modified work as a modified version of the original" (NereusSDR's
Modification-History blocks satisfy the mark-as-modified requirement;
Task 25b will audit whether we've preserved the upstream notices).

## Upstream contributor attribution — the important fact

**AetherSDR source files do NOT carry per-file copyright headers.**

Every `.cpp`/`.h` file under `src/core/`, `src/models/`, `src/gui/`, and
`src/main.cpp` opens with `#pragma once` (or `#include "…"`), followed
immediately by Qt/std includes and the `namespace AetherSDR {` block.
No `Copyright (C) …`, no SPDX-License-Identifier, no per-file GPL
permission-block comments.

This was verified by scanning the entire `src/` tree for
`Copyright (C)`, `Copyright (c)`, `Copyright ©`, and
`SPDX-License-Identifier` — **zero matches** across all 235 source
files in `src/core/`, `src/models/`, `src/gui/`, `src/generated/`.
The 32 files whose grep lines mentioned "Copyright / FlexRadio /
KK7GWY / Jeremy / NR0V / Warren Pratt / GPLv" turned out to be either
(a) the About-dialog HTML in `MainWindow.cpp`, (b) string literals
containing "FlexRadio Systems" (protocol/brand), or (c) in-line
comments like "per FlexRadio community guidance". None are per-file
copyright headers.

AetherSDR's attribution model is centralised, not per-file:

1. **`LICENSE`** (top-level) — full GPLv3 text.
2. **`README.md`** — GPLv3 badge, repo URL.
3. **`CONTRIBUTING.md` §License** — "licensed under the GNU General
   Public License v3.0".
4. **About dialog** (`src/gui/MainWindow.cpp:3195-3300`) — the canonical
   live contributor list:
   - `"Jeremy (KK7GWY)<br>Claude &middot; Anthropic<br>rfoust<br>Dependabot"`
     (the static default, line 3226)
   - Live fetch from the GitHub contributors API at dialog-open
     (lines 3281-3308) — `names << "Jeremy (KK7GWY)" << "Claude &middot; Anthropic"`
     then appends each GitHub login that isn't `ten9876` or a `[bot]`
     account.
   - Footer: `"© 2026 AetherSDR Contributors / Licensed under GPLv3"`,
     linked repo URL, `"SmartSDR protocol © FlexRadio Systems"`.

**Implication for NereusSDR Phase 4 (important for Task 25b):** when we
port a file from AetherSDR there is no per-file Block/Inline attribution
to copy forward. The only contributor facts the upstream file itself
gives us are:

- Author handle: **Jeremy (KK7GWY)** (`ten9876` on GitHub) — the sole
  on-record primary author named in every AetherSDR artifact.
- Optional cited helpers in the About dialog (Claude/Anthropic as AI
  tooling, `rfoust` and `Dependabot` as GitHub-API contributors).
- The GPLv3 license grant from the top-level `LICENSE` file.

No Warren Pratt / NR0V, no FlexRadio Systems copyright, no MW0LGE, no
mi0bot, no W5WC, no VK6APH, no G8NJJ attribution lines exist in any
AetherSDR `.cpp`/`.h` file we inspected. The Thetis lineage that
NereusSDR follows for DSP/radio logic is entirely separate from the
AetherSDR lineage — AetherSDR is a **SmartSDR-over-TCP client** for
FlexRadio, not a WDSP/Thetis derivative, so the AetherSDR tree has no
inherited WDSP or Thetis contributors to carry forward. (This is why
NereusSDR's source-first protocol routes "what does the code do?" to
Thetis and "how do we structure it in Qt6?" to AetherSDR — AetherSDR
supplies **structural/architectural patterns only**, not DSP
algorithms.)

This matches the conventions of many Qt6/CMake projects that rely
exclusively on a top-level `LICENSE` plus SPDX metadata in a
`packaging/` manifest, rather than repeating the header on each file.

## Top-level AetherSDR repo layout (depth 2)

```
AetherSDR/
├── LICENSE                    # GPLv3 verbatim
├── README.md                  # GPLv3 badge, project description
├── CLAUDE.md                  # AI agent guidance
├── CONTRIBUTING.md            # License/DCO guidance
├── CMakeLists.txt
├── CHANGELOG.md
├── CODE_OF_CONDUCT.md
├── CODEOWNERS
├── SECURITY.md
├── STYLEGUIDE.md
├── cty.dat                    # AD1C DXCC data (third-party)
├── docs/                      # Architecture + protocol docs
├── hal-plugin/                # HAL audio plugin
├── macos/                     # Bundle resources
├── packaging/                 # AppImage/DMG/NSIS
├── plugins/
├── resources/                 # Icons/images/skins
├── resources.qrc
├── scripts/
├── setup-{deepfilter,fftw,hidapi,opus}.{ps1,sh}
├── src/
│   ├── main.cpp
│   ├── core/                  # protocol, audio, DSP (95 files)
│   ├── models/                # RadioModel, SliceModel, etc. (28 files)
│   ├── gui/                   # MainWindow, applets, widgets (110 files)
│   └── generated/             # code-gen outputs (e.g. WhatsNewData.cpp)
└── third_party/               # bundled deps
    ├── deepfilter/            # DeepFilterNet3 neural NR
    ├── fftw3/                 # FFTW3 (pre-built Windows DLL)
    ├── ggmorse/               # CW decoder
    ├── libspecbleach/         # Spectral noise reduction
    ├── r8brain/               # r8brain-free-src resampler
    ├── radae/                 # RADAE (FreeDV AI codec)
    ├── rnnoise/               # RNNoise
    └── rtmidi/                # MIDI I/O
```

## Per-file contributor summary

### Header-pattern applies to ALL files below

Every AetherSDR `.cpp`/`.h` source file follows the same pattern:

```cpp
#pragma once                     // (.h only)

#include <QObject>
// ... std/Qt includes ...

namespace AetherSDR {

// short descriptive comment (behavioural, NOT a copyright block)
class Foo { ... };

} // namespace AetherSDR
```

Therefore, for every file in the table below:

- **Block** = `(none — AetherSDR files carry no per-file copyright
  header; upstream credit is centralised in LICENSE + MainWindow About
  dialog, with "Jeremy (KK7GWY)" as primary author)`
- **Inline** = `(none of the tracked callsigns/FlexRadio/MW0LGE/etc.
  cited — strings like "FlexRadio Systems" appear only as protocol
  brand references, not code attributions; see Notes column for any
  protocol-lineage comments)`
- **Apparent author (via git/README/About)** = Jeremy (KK7GWY) unless
  otherwise noted.

Rather than repeat that boilerplate on every row, the table is compact.

### Counterpart map — AetherSDR upstream source for NereusSDR AetherSDR-citing files

The left column is the AetherSDR upstream file. The right column is the
NereusSDR file(s) most likely to have ported from or been structurally
inspired by it (rough heuristic — Task 25b does the authoritative
per-file NereusSDR-side diff).

| AetherSDR file | NereusSDR counterpart(s) | Notes on upstream content |
|---|---|---|
| `src/main.cpp` | `src/main.cpp` | Bootstrap only. |
| `src/core/AppSettings.h` / `.cpp` | `src/core/AppSettings.h` / `.cpp` | Comment cites "structured to match SmartSDR's SSDR.settings" — pattern only, no Flex/Thetis code. |
| `src/core/AudioEngine.h` / `.cpp` | `src/core/AudioEngine.h` / `.cpp` | SmartSDR-flavoured audio engine. Comments mention FlexRadio DAX and Voicemeeter as *device-name* examples only. Opus TX pacing, VITA-49 framing. |
| `src/core/RadioConnection.h` / `.cpp` | `src/core/RadioConnection.{h,cpp}` and `src/core/radio/*` | AetherSDR impl is TCP to FlexRadio port 4992 (SmartSDR), different wire protocol from NereusSDR's OpenHPSDR UDP P1/P2. Structural pattern (worker thread, state machine) is what NereusSDR reuses. |
| `src/core/RadioDiscovery.h` / `.cpp` | `src/core/RadioDiscovery.h` / `.cpp` | Header comment: "Represents a discovered FlexRadio on the network." UDP listener pattern (bind retry, stale timer, re-bind) ported structurally. NereusSDR later re-ported the *discovery protocol parsing* from mi0bot/Thetis — keep that distinction in mind. |
| `src/core/WdspEngine.{h,cpp}` | `src/core/WdspEngine.{h,cpp}` | **NOT present in AetherSDR** — AetherSDR has no WDSP. NereusSDR's WdspEngine is Thetis-derived, not AetherSDR-derived. Any NereusSDR `// From AetherSDR` comment on WdspEngine would be an attribution error — flag for 25b. |
| `src/core/FFTEngine.{h,cpp}` | `src/core/FFTEngine.{h,cpp}` | AetherSDR has no standalone FFTEngine — FFT happens inside PanadapterStream/VITA-49 parsing. NereusSDR's FFTEngine (FFTW3 worker thread) is an original NereusSDR composition with AetherSDR-inspired threading pattern. |
| `src/core/PanadapterStream.{h,cpp}` | (not directly — VITA-49 parsing lives in P2 radio connection) | AetherSDR-specific: FlexRadio VITA-49 multiplexing. NereusSDR's DDC/panadapter plumbing is OpenHPSDR-native. |
| `src/core/mmio/…` subsystem | `src/core/mmio/{MmioEndpoint.h,FormatParser.{h,cpp},UdpEndpointWorker.{h,cpp},TcpListener…,TcpClient…,SerialEndpointWorker.{h,cpp},ExternalVariableEngine.h}` | **NOT present in AetherSDR** — the MMIO external-variable subsystem is a Thetis meter-system feature (Thetis frmMeterDisplay external-data inputs) ported directly into NereusSDR's 3G-6/3G-7 meter dialog. The NereusSDR `// From AetherSDR` comments on MMIO files, if any, should be re-checked against Thetis — flag for 25b. |
| `src/core/CommandParser.{h,cpp}` | (none — NereusSDR uses OpenHPSDR binary framing, not SmartSDR TCP text protocol) | SmartSDR-specific. |
| `src/core/RNNoiseFilter.{h,cpp}` | (none — NereusSDR's NR2/NR3/NR4 is WDSP) | AetherSDR client-side neural NR, not used by NereusSDR. |
| `src/core/DeepFilterFilter.{h,cpp}` | (none) | Same. |
| `src/core/SpecbleachFilter.{h,cpp}` | (none) | Same. |
| `src/core/NvidiaBnrFilter.{h,cpp}` | (none) | Same. |
| `src/core/SpectralNR.{h,cpp}` | (none) | Same. |
| `src/core/OpusCodec.{h,cpp}` | (none) | Opus TX codec for SmartLink WAN; not used by NereusSDR. |
| `src/core/Resampler.{h,cpp}` | (none — NereusSDR uses WDSP-internal resamplers) | r8brain-based. |
| `src/core/RADEEngine.{h,cpp}` | (none) | FreeDV RADE AI codec. |
| `src/core/CwDecoder.{h,cpp}` | (none in NereusSDR 0.1.7) | ggmorse-based CW decoder. |
| `src/core/CtyDatParser.{h,cpp}` | (none) | AD1C cty.dat parsing. |
| `src/core/AdifParser.{h,cpp}` | (none) | ADIF log file parser. |
| `src/core/DxccColorProvider.{h,cpp}` | (none) | DXCC lookup helper. |
| `src/core/DxccWorkedStatus.{h,cpp}` | (none) | Worked/confirmed tracker. |
| `src/core/DxClusterClient.{h,cpp}` | (none — planned Phase 3J) | |
| `src/core/PotaClient.{h,cpp}` | (none) | POTA spot fetch. |
| `src/core/FlexControlManager.{h,cpp}` | (none) | FlexControl USB tuning knob. |
| `src/core/HidDeviceParser.{h,cpp}` / `HidEncoderManager.{h,cpp}` | (none) | HID MIDI encoders. |
| `src/core/MidiControlManager.{h,cpp}` / `MidiSettings.{h,cpp}` | (none yet in NereusSDR) | MIDI mapping. |
| `src/core/SmartLinkClient.{h,cpp}` | (none) | SmartSDR+ WAN TLS client. |
| `src/core/WanConnection.{h,cpp}` | (none) | TLS transport for SmartLink. |
| `src/core/TciServer.{h,cpp}` / `TciProtocol.{h,cpp}` | (none yet — planned Phase 3J) | TCI WebSocket server. |
| `src/core/RigctlServer.{h,cpp}` / `RigctlProtocol.{h,cpp}` / `RigctlPty.{h,cpp}` | (none yet — planned Phase 3K) | Hamlib rigctld. |
| `src/core/SerialPortController.{h,cpp}` | (none) | Serial PTT/CW keying. |
| `src/core/SpotCollectorClient.{h,cpp}` / `SpotCollectorProto` | (none yet) | Spot aggregator. |
| `src/core/FirmwareStager.{h,cpp}` / `FirmwareUploader.{h,cpp}` | (none — different firmware model) | SmartSDR firmware updater. |
| `src/core/FreeDvClient.{h,cpp}` | (none) | FreeDV Reporter. |
| `src/core/DvkWavTransfer.{h,cpp}` | (none) | DVK file transfer. |
| `src/core/PgxlConnection.{h,cpp}` / `TgxlConnection.{h,cpp}` | (none) | PGXL amp / TGXL tuner. |
| `src/core/LogManager.{h,cpp}` | (no direct counterpart — NereusSDR uses `qCWarning(lcCategory)` directly) | Qt logging category wrappers. |
| `src/core/ShortcutManager.{h,cpp}` | (none yet) | Keyboard shortcuts. |
| `src/core/SupportBundle.{h,cpp}` | (none yet — pending 3G-14) | Diagnostic bundle. |
| `src/core/BandStackSettings.{h,cpp}` | (none direct — NereusSDR has PanadapterModel per-band grid) | |
| `src/core/PipeWireAudioBridge.{h,cpp}` | `src/core/audio/LinuxPipeBus.{h,cpp}` | Linux PulseAudio/PipeWire virtual-audio bridge (VAX). NereusSDR decomposes the monolithic PipeWireAudioBridge into per-endpoint `IAudioBus` instances (Role enum for Vax1..4 / TxInput), drops QObject/signals in favour of atomic metering, defers silence-fill + TX poll timer + per-channel/TX gain to Phase 3M, rebrands pipe paths `/tmp/aethersdr-dax-*` → `/tmp/nereussdr-vax-*`, and lifts format 24 kHz mono int16 → 48 kHz stereo float32 per spec §8.1. Stale-module cleanup is gated by `std::once_flag` (runs once per process on first `open()`). Added Sub-Phase 6 Task 6.1. |
| `src/core/VirtualAudioBridge.{h,cpp}` | `src/core/audio/CoreAudioHalBus.{h,cpp}` | macOS CoreAudio HAL bridge. NereusSDR decomposes the monolithic 4-RX + 1-TX bridge into per-endpoint `IAudioBus` instances (Role enum for Vax1..4 / TxInput), drops the QObject/signals layer in favour of atomic metering, defers the silence-fill + TX poll timers to Phase 3M, rebrands shm paths `/aethersdr-dax-*` → `/nereussdr-vax-*`, and lifts the native rate 24 kHz → 48 kHz per spec §8.1. Added Sub-Phase 5 Task 5.3. |
| `hal-plugin/AetherSDRDAX.cpp` | `hal-plugin/NereusSDRVAX.cpp` | macOS Core Audio HAL Audio Server Plug-In (libASPL-based). NereusSDR rebrands DAX → VAX: device UIDs `com.aethersdr.dax.*` → `com.nereussdr.vax.*`, shm paths `/aethersdr-dax-*` → `/nereussdr-vax-*`, device names `AetherSDR DAX N` → `NereusSDR VAX N`, factory UUID regenerated. Same 48 kHz stereo float32 format. Added Sub-Phase 5 Task 5.1. |
| `hal-plugin/CMakeLists.txt` / `hal-plugin/Info.plist` | `hal-plugin/CMakeLists.txt` / `hal-plugin/Info.plist` | HAL plugin build + bundle descriptor. Structural templates only — NereusSDR rewrites bundle IDs, display names, and FetchContent pins while keeping the libASPL build shape. |
| `src/core/PropForecastClient.{h,cpp}` | (none) | hamqsl.com HF forecast. |
| `src/core/MacMicPermission.{h,mm}` | (none) | macOS TCC mic consent. |
| `src/core/VersionNumber.h` | (none) | Small version-compare helper. |
| `src/core/proto/` | (none) | Additional protocol helpers. |
| `src/models/RadioModel.{h,cpp}` | `src/models/RadioModel.{h,cpp}` | **Main structural template** for NereusSDR's RadioModel (central state hub, owns all sub-models, emits signals on main thread). NereusSDR swapped the SmartSDR-specific fields (slice/pan/handle) for OpenHPSDR equivalents (DDC/receiver/MAC). |
| `src/models/SliceModel.{h,cpp}` | `src/models/SliceModel.{h,cpp}` | Per-slice VFO state template. NereusSDR's SliceModel 3G-10 slice feature set was inspired here, algorithmic behaviour ported from Thetis console.cs. |
| `src/models/PanadapterModel.{h,cpp}` | `src/models/PanadapterModel.{h,cpp}` | Per-pan display-state template. NereusSDR's PanadapterModel also holds per-band grid (added 3G-8) which is Thetis-derived, not AetherSDR-derived. |
| `src/models/MeterModel.{h,cpp}` | (structurally split across MeterWidget/MeterPoller/MeterItem) | |
| `src/models/TransmitModel.{h,cpp}` | (NereusSDR TX models pending 3M) | |
| `src/models/TunerModel.{h,cpp}` | (none yet) | TGXL tuner state. |
| `src/models/EqualizerModel.{h,cpp}` | (none direct — EQ is in EqApplet) | |
| `src/models/TnfModel.{h,cpp}` | (none yet) | Tracking notch filters. |
| `src/models/SpotModel.{h,cpp}` | (none yet) | DX spot overlay. |
| `src/models/CwxModel.{h,cpp}` / `DvkModel.{h,cpp}` / `UsbCableModel.{h,cpp}` / `DaxIqModel.{h,cpp}` / `AntennaGeniusModel.{h,cpp}` | (none yet) | FlexRadio-specific features. |
| `src/models/BandPlan.h` / `BandPlanManager.{h,cpp}` | (none — NereusSDR uses `Band` enum + `bandFromFrequency`, Thetis-derived IARU Region 2) | AetherSDR has its own JSON-loaded band plan with license classes. |
| `src/models/BandDefs.h` | (NereusSDR `Band.h` supersedes this, Thetis-derived) | |
| `src/models/BandSettings.{h,cpp}` | (none direct) | |
| `src/gui/MainWindow.{h,cpp}` | `src/gui/MainWindow.{h,cpp}` | **Key structural source** — the About dialog contributor list is the canonical upstream attribution point (lines 3195-3308). NereusSDR's MainWindow borrows the signal-routing-hub pattern. About-dialog text in NereusSDR was *not* copied; NereusSDR has its own `src/gui/AboutDialog.cpp`. |
| `src/gui/SpectrumWidget.{h,cpp}` | `src/gui/SpectrumWidget.{h,cpp}` | QRhi GPU spectrum + waterfall. Heavy structural borrow — QRhi pipelines, shader approach, tile layout, overlay caching. Inline comment cites "FlexRadio community guidance" for waterfall tile geometry (line 738) — brand/protocol, not code attribution. |
| `src/gui/SpectrumOverlayMenu.{h,cpp}` | `src/gui/SpectrumOverlayMenu.h` / `SpectrumOverlayPanel.{h,cpp}` | Overlay panel with flyout sub-panels. |
| `src/gui/VfoWidget.{h,cpp}` | `src/gui/widgets/VfoWidget.{h,cpp}` / `VfoModeContainers.{h,cpp}` / `VfoStyles.h` / `VfoLevelBar.cpp` / `ScrollableLabel.{h,cpp}` | Floating VFO flag pattern. NereusSDR's VfoWidget 3G-10 slice rewrite brought in mode containers + S-meter. Comment at line 2540 "From docs/vfo_mode_filters.csv" = internal AetherSDR docs reference. |
| `src/gui/ConnectionPanel.{h,cpp}` | `src/gui/ConnectionPanel.{h,cpp}` | Connect-to-radio UI pattern. AetherSDR version has flexradio-account email field (SmartLink login); NereusSDR version is OpenHPSDR discovery-only. |
| `src/gui/AppletPanel.{h,cpp}` | `src/gui/applets/AppletPanelWidget.{h,cpp}` | Fixed header + scrollable body layout pattern. |
| `src/gui/RxApplet.{h,cpp}` | `src/gui/applets/RxApplet.{h,cpp}` | Receiver controls applet. NereusSDR RX controls are largely Thetis-feature-mapped but styled from AetherSDR. |
| `src/gui/TxApplet.{h,cpp}` | `src/gui/applets/TxApplet.{h,cpp}` | TX controls applet. Pending 3M for full TX logic. |
| `src/gui/PhoneApplet.{h,cpp}` / `PhoneCwApplet.{h,cpp}` | `src/gui/applets/PhoneCwApplet.cpp` | |
| `src/gui/EqApplet.{h,cpp}` | `src/gui/applets/EqApplet.{h,cpp}` | |
| `src/gui/CatApplet.{h,cpp}` | (pending 3K) | |
| `src/gui/TunerApplet.{h,cpp}` | (pending) | |
| `src/gui/AmpApplet.{h,cpp}` / `AntennaGeniusApplet.{h,cpp}` | (FlexRadio accessory-specific — not in NereusSDR) | |
| `src/gui/PanadapterApplet.{h,cpp}` / `PanadapterStack.{h,cpp}` / `PanLayoutDialog.{h,cpp}` | (pending Phase 3F) | Multi-pan management UI. |
| `src/gui/RadioSetupDialog.{h,cpp}` | `src/gui/setup/*` pages + `HardwarePage.{h,cpp}` + `setup/hardware/*` tabs + `SetupPage.cpp` + `TransmitSetupPages.{h,cpp}` + `DspSetupPages.cpp` + `DisplaySetupPages.{h,cpp}` + `GeneralOptionsPage.{h,cpp}` | Pattern only; content is Thetis-feature-driven. AetherSDR's `RadioSetupDialog.cpp` §License Info block (lines 263-312) is SmartSDR-license-specific, has no NereusSDR counterpart. |
| `src/gui/ProfileManagerDialog.{h,cpp}` | (none yet) | |
| `src/gui/DxClusterDialog.{h,cpp}` | (pending 3J) | |
| `src/gui/MemoryDialog.{h,cpp}` | (none yet) | |
| `src/gui/ShortcutDialog.{h,cpp}` / `KeyboardMapWidget.{h,cpp}` | (none yet) | |
| `src/gui/MidiMappingDialog.{h,cpp}` | (none yet) | |
| `src/gui/MultiFlexDialog.{h,cpp}` | (none — SmartSDR-specific) | |
| `src/gui/NetworkDiagnosticsDialog.{h,cpp}` | (none yet) | |
| `src/gui/SliceTroubleshootingDialog.{h,cpp}` | (none yet) | |
| `src/gui/HelpDialog.{h,cpp}` | (none yet) | |
| `src/gui/WhatsNewDialog.{h,cpp}` | (none yet) | |
| `src/gui/SupportDialog.{h,cpp}` | (none yet) | String "for FlexRadio transceivers" appears only in the support-bundle description text. |
| `src/gui/SpotSettingsDialog.{h,cpp}` | (pending 3J) | |
| `src/gui/CwxPanel.{h,cpp}` / `DvkPanel.{h,cpp}` / `BandStackPanel.{h,cpp}` | (none yet) | |
| `src/gui/FloatingAppletWindow.{h,cpp}` | `src/gui/containers/FloatingContainer.{h,cpp}` + `ContainerWidget.{h,cpp}` + `ContainerManager.{h,cpp}` + `ContainerSettingsDialog.{h,cpp}` | Float/dock/resize shell. NereusSDR's 3G-1/3G-6 container system took structural inspiration here but expanded it significantly (dock modes, axis-lock, MMIO). Thetis `ucMeter` / `frmMeterDisplay` is the *behavioural* model (per NereusSDR CLAUDE.md). |
| `src/gui/FilterPassbandWidget.{h,cpp}` | `src/gui/widgets/FilterPassbandWidget.{h,cpp}` | Filter low/high drag widget. |
| `src/gui/GuardedSlider.h` | `src/gui/widgets/GuardedSlider.h` + `GuardedComboBox.h` + `ResetSlider.h` + `CenterMarkSlider.h` + `TriBtn.h` | Widget-library primitives. |
| `src/gui/MeterSlider.{h,cpp}` | (NereusSDR slider library) | |
| `src/gui/PhaseKnob.{h,cpp}` | (none yet) | |
| `src/gui/SMeterWidget.{h,cpp}` | (split into `MeterWidget` + S-meter `ItemGroup` preset) | |
| `src/gui/MeterApplet.{h,cpp}` | `src/gui/meters/MeterWidget.{h,cpp}` + `MeterPoller.{h,cpp}` + `MeterItem.{h,cpp}` + all the item subclasses + `ItemGroup.{h,cpp}` | NereusSDR's meter system is a **major divergence** — it's Thetis-MeterManager-ported (per NereusSDR CLAUDE.md, 3G-2/3G-3/3G-4/3G-5/3G-6), not AetherSDR-derived. Any `// From AetherSDR` comments on `src/gui/meters/*` items should be re-checked against Thetis `MeterManager.cs` — flag for 25b. |
| `src/gui/DspParamPopup.{h,cpp}` / `AetherDspDialog.{h,cpp}` | (none — NereusSDR uses inline applet controls) | |
| `src/gui/TitleBar.{h,cpp}` | `src/gui/TitleBar.{h,cpp}` | **Scoped-down port** (Phase 3O Sub-Phase 10 Task 10c, 2026-04-20). NereusSDR's TitleBar is a thin 32 px host strip hosting the menu bar + MasterOutputWidget only; AetherSDR's heartbeat / multiFLEX / PC-audio / headphone / minimal-mode / feature-request widgets are intentionally omitted (deferred to separate NereusSDR phases — 3G-14 plans the 💡 feature-request widget; headphone devices land in Sub-Phase 12 Setup → Audio → Devices). `setMenuBar()` is a line-for-line port of AetherSDR `TitleBar.cpp:282-295`. |
| `src/gui/ComboStyle.h` / `HGauge.h` / `SliceColors.h` | `src/gui/StyleConstants.h` and co. | Shared style constants. |
| `src/generated/WhatsNewData.cpp` | (none — auto-generated) | Change-log entries. |

### NereusSDR-original files with NO AetherSDR counterpart (important for 25b)

These NereusSDR files cite AetherSDR in their Modification-History block
or inline comments but have **no upstream AetherSDR source file** to
attribute to (because AetherSDR targets a different radio/DSP stack):

- `src/core/WdspEngine.cpp` and the WDSP RX/TX channel wrappers — WDSP
  is Thetis/TAPR, not AetherSDR.
- `src/core/ReceiverManager.{h,cpp}` — DDC-aware receiver lifecycle;
  AetherSDR has no DDC concept (FlexRadio abstracts this into "slice").
- `src/core/HardwareProfile.{h,cpp}` + all `BoardCapabilities` work —
  OpenHPSDR-specific; AetherSDR's radio model is FlexRadio-only.
- `src/core/StepAttenuatorController.{h,cpp}` — Thetis feature.
- `src/core/NoiseFloorEstimator.h` — NereusSDR-original (3G-13).
- `src/core/ClarityController.h` — NereusSDR-original clarity algorithm.
- `src/core/FFTEngine.{h,cpp}` — FFTW3-based, AetherSDR uses
  `PanadapterStream` VITA-49 parsing instead.
- `src/core/mmio/*` — Thetis MeterManager external-variable subsystem;
  AetherSDR has no MMIO.
- `src/models/Band.h` — NereusSDR 14-band enum (Thetis-derived).
- `src/models/RxDspWorker.h` — NereusSDR worker pattern.
- `src/gui/meters/*` entire tree — Thetis MeterManager port.
- `src/gui/containers/*` — pattern-inspired by AetherSDR
  FloatingAppletWindow but significantly expanded; Thetis ucMeter is the
  behavioural model.
- `src/gui/setup/**` setup-page hierarchy — Thetis Setup.cs layout.
- `src/gui/AddCustomRadioDialog.{h,cpp}` — Thetis frmAddCustomRadio port.

Task 25b should treat AetherSDR citations on the above files as
**suspicious** — either (a) the citation is acknowledging a structural
pattern influence (legitimate but low-weight), (b) the citation is a
copy-paste carryover that should be removed, or (c) the header should
add a Thetis citation that's currently missing.

## Flags for Task 25b

1. **No per-file upstream headers to preserve.** AetherSDR's
   centralised attribution model means our porting-header convention
   (Block = Jeremy/KK7GWY + GPLv3) is the maximum upstream-derived
   attribution we can produce. Do not write per-file
   `Copyright (C) Jeremy (KK7GWY)` lines into AetherSDR headers as if
   we sourced them from upstream — we'd be inventing text that isn't
   there. Preferred form: treat AetherSDR as a *project-level*
   attribution (repo URL + GPLv3 + "Jeremy (KK7GWY) et al. per
   <repo>/LICENSE and About dialog") rather than a per-file copyright
   line.
2. **SmartSDR / FlexRadio protocol strings.** AetherSDR source is full
   of literal strings like `"FlexRadio"`, `"smartlink.flexradio.com"`,
   `"FlexRadio DAX"`. These are brand/protocol references, not
   copyright attributions — NereusSDR shouldn't have carried any of
   them over (NereusSDR targets OpenHPSDR), and if any have been
   copied verbatim that's a dead-code bug, not a licensing bug.
3. **WDSP / Thetis lineage is entirely separate.** Any NereusSDR file
   that does DSP (WdspEngine, RxChannel, TxChannel, NB, AGC, etc.) has
   no legitimate AetherSDR ancestry — its "upstream" is Thetis and/or
   WDSP. If such files cite AetherSDR, the citation should be limited
   to structural patterns (Qt threading, signal/slot shape) and
   explicitly *not* DSP behaviour.
4. **Meter subsystem** (`src/gui/meters/*`) — Thetis MeterManager, not
   AetherSDR. AetherSDR's `MeterApplet` is a single applet; NereusSDR's
   meter tree is the full Thetis ucMeter/frmMeterDisplay port. 25b:
   re-check any `// From AetherSDR` on `src/gui/meters/*` against
   Thetis MeterManager.cs / ucMeter.cs.
5. **Container subsystem** (`src/gui/containers/*`) — same pattern;
   Thetis ucMeter/frmMeterDisplay is the behavioural model, AetherSDR
   FloatingAppletWindow is the structural pattern. Split the citations
   accordingly.
6. **MMIO subsystem** (`src/core/mmio/*`) — pure Thetis; any AetherSDR
   citation is wrong.
7. **RadioDiscovery** — NereusSDR cites `mi0bot/AetherSDR port` which
   is a two-stage lineage (mi0bot/Thetis → AetherSDR-style
   reorganisation → NereusSDR). Both citations legitimate; make sure
   both are preserved.
8. **RadioModel / SliceModel / PanadapterModel structural templates**
   are the strongest AetherSDR-legitimate citations in the NereusSDR
   tree. Preserve these.
9. **SpectrumWidget** — QRhi pipeline architecture is the strongest
   code-level AetherSDR borrow. Verify NereusSDR's comments/headers
   credit Jeremy (KK7GWY) + AetherSDR repo URL + GPLv3 explicitly.
10. **VfoWidget** — strong AetherSDR structural template for the
    floating VFO flag. Preserve the citation.
11. **AboutDialog** — NereusSDR has its own `src/gui/AboutDialog.cpp`.
    If it was modelled on AetherSDR's MainWindow About block, its
    source-of-origin comment should cite
    `AetherSDR/src/gui/MainWindow.cpp` around line 3195-3308. 25b
    check.

## Counted scope

- **Total AetherSDR source files surveyed:** 235
  (`src/core/` + `src/models/` + `src/gui/` + `src/generated/` + `src/main.cpp`)
- **Files with per-file copyright header:** **0**
- **Files with SPDX-License-Identifier:** **0**
- **Files catalogued in per-file table above (counterpart-mapped or
  explicitly noted as "no counterpart"):** ~75 entries covering all
  AetherSDR files whose names parallel any AetherSDR-citing NereusSDR
  file, plus the major NereusSDR files that cite AetherSDR but have no
  counterpart.
- **Sole on-record upstream author:** Jeremy (KK7GWY) — `ten9876` on
  GitHub.
- **Other credited contributors (About dialog static list, line 3226):**
  `Claude · Anthropic` (AI tooling), `rfoust` (GitHub contributor),
  `Dependabot` (bot). Live GitHub API call at dialog-open appends
  additional logins.

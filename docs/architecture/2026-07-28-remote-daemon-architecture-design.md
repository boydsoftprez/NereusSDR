# Remote Daemon Architecture (`nereusd`): Design

**Status:** Design, revised after adversarial review. Not yet planned or implemented.
**Date:** 2026-07-28 (rev 2)
**Author:** J.J. Boyd (KG4VCF), with AI-assisted drafting via Anthropic Claude Code

> **All NereusSDR line numbers are taken against `origin/main` as of
> 2026-08-02.** Phase 3F landed on `main` via PR #312
> (`feature/nereussdr-multipan`), carrying PR #293 and PR #291 with it, so the
> baseline this design was written against is now simply `main` and the
> merge-order precondition in section 2.8 is discharged. Cites were re-derived
> by symbol search after the merge. Thetis cites are against `v2.10.3.15`
> @ `3759d096` and are unaffected by our branch state.

---

## 1. Goal

Run a headless NereusSDR daemon on a small Linux host (Raspberry Pi class)
that owns the radio and performs all baseband DSP, and let the existing
NereusSDR GUI connect to it from anywhere as a remote console.

The daemon does everything NereusSDR does today except draw. It connects to
the radio over Ethernet using Protocol 1 or Protocol 2, runs the full WDSP
receive and transmit chains for every slice, computes the spectrum for every
subscribed pan, and streams audio, spectrum, meters, and state to a remote
client. The client renders, accepts operator input, and sends microphone
audio back.

**Non-goal:** this is not screen-sharing, and it is not raw-IQ streaming.
Both are rejected in §3.

---

## 2. Findings that shaped this design

Recorded because they are not obvious, and because several were discovered by
reading source rather than by reasoning.

### 2.1 Our core is nearly GUI-free, but the spectrum pipeline is not

`CORE_SOURCES` (118 entries) plus `MODEL_SOURCES` (15) contain exactly **one**
`#include "gui/..."`: `gui/SpectrumWidget.h` at `src/models/RadioModel.cpp:299`,
the non-owning view hook `MainWindow` installs.

That single-include figure is real but misleading on its own. The entire
**spectrum production pipeline** lives in GUI code (§4.2 items 2 through 6),
and `SpectrumDetector` is not GUI-free despite its DSP-only content. The
honest summary is: a build-system change plus a small number of interface
extractions and one service relocation, not a rewrite, but materially more
than one extraction.

CMake is the right shape to start from: `NereusSDRObjs` is an OBJECT library
and `NereusSDR` is a thin executable adding only `src/main.cpp`.

### 2.2 TCI has no spectrum stream, and what that costs

`TCIServer.cs` at `v2.10.3.15` is 9,342 lines and contains **zero**
occurrences of spectrum, waterfall, fft, or panadapter. `TCIStreamType`
(`TCIServer.cs:343-350 [v2.10.3.15]`) is exactly five values: IQ, RX_AUDIO,
TX_AUDIO, TX_CHRONO, LINEOUT.

**How every current TCI client draws a panadapter anyway.** TCI carries raw
IQ. `TCIStreamType.IQ_STREAM` is type 0, with `iq_start`, `iq_stop`, and
`iq_samplerate` commands. `PublishIQSamples` (`TCIServer.cs:5837-5839
[v2.10.3.15]`) emits **unconditionally FLOAT32** complex IQ, and the rate is
clamped to 48 kHz through 384 kHz (`TCIServer.cs:5771-5772 [v2.10.3.15]`:
`// iq can only go up to that`). Clients subscribe and compute their own FFT.

Derived cost: **3.07 Mbit/s at the 48 kHz floor, 12.29 at 192 kHz, 24.58 at
384 kHz.** ON7OFF quote "roughly 2 Mbit/s" for the same stream, which is below
what the server will emit at any setting; the derived figure is worse than
their marketing figure, which strengthens rather than weakens §3's argument.

This is worth stating explicitly, because "TCI has no spectrum stream" and
"TCI clients display panadapters" both being true is otherwise confusing. A
panadapter can be drawn from IQ; it just costs multiple Mbit/s.

**The commercial ON7OFF "TCI Remote Compactor" is the market evidence.** Their
Android client runs direct on a LAN, subscribing to the IQ stream and running
a Blackman-Harris FFT on the phone, which their own product page states is
unusable on cellular. Via the Compactor, a Windows middlebox on the shack LAN
consumes that IQ stream and ships under 100 kbit/s onward.

*Attribution note:* the sub-100 kbit/s figure, the Opus usage, and the
Blackman-Harris FFT are the vendor's own published claims. That the Compactor
performs the FFT and reduction itself is our **inference** from TCI carrying
no spectrum stream, not something the vendor states. The inference is sound
but it is an inference, and §2.4's "verified rather than inferred" standard
does not extend to it.

Note: the Compactor product page lists NereusSDR by name as a supported TCI
server, alongside Thetis, Zeus, and AetherSDR. Our TCI implementation is
already a third-party integration target.

### 2.3 Thetis has the spectrum reducer, switched off, with nowhere to send it

Three layers are easy to conflate, so they are separated explicitly:

| Layer | State in released `v2.10.3.15` |
| --- | --- |
| Spectrum **producer** (`clsSpectrumProcessor.cs`) | In source, compiled, **never instantiated** |
| TCI **wire protocol** | No spectrum stream type (§2.2) |
| MW0LGE's **unreleased** build | Both present and wired; known only from screenshots (§2.5) |

`clsSpectrumProcessor.cs` (1,353 lines) allocates a dedicated WDSP analyzer
per endpoint via `cmaster.AllocAnalyzer`, runs `SpecHPSDRDLL.GetPixels()` on a
worker thread at a configured frame rate, and returns `float[pixels]` of dBm
with a `dataIndex` frame counter. Per-source, with independent pixels, frame
rate, FFT size, sample rate, zoom, and pan.

Constants (`clsSpectrumProcessor.cs:59-69 [v2.10.3.15]`), all eleven:

| Constant | Value |
| --- | --- |
| `DefaultPixels` | 1024 |
| `DefaultFrameRate` | 15 |
| `DefaultFftSize` | 4096 |
| `DefaultMaxFftSize` | 262144 |
| `DefaultSampleRate` | 192000 |
| `DefaultWindowType` | 4 |
| `DefaultDetectorTypePan` | 0 |
| `DefaultAverageMode` | 0 |
| `DefaultAverageTau` | 0.12 |
| `EmptyPixelValue` | -200.0f |
| **`DataIndexWrap`** | **1000000000** |

`DataIndexWrap` matters: the counter wraps to **1**, not 0, so any
frame-index comparison must be wrap-aware (§9.4).

**The class has never executed in any shipping Thetis.** Every call site in
`console.cs` is commented out (`console.cs:1125-1131 [v2.10.3.15]`), the field
at `console.cs:1200` has no live assignment, and both landed already-commented
in commit `903df762` (2026-03-15), two and a half weeks before archival.

**Risk consequence:** we may port the endpoint model from released GPL source,
but it must not be described as proven upstream code. The reduction
mathematics we actually reuse, `SpectrumDetector` and `SpectrumAvenger`, are
verbatim WDSP `analyzer.c` ports that run in our shipping application, so the
risk sits in endpoint plumbing rather than in the maths.

**The call-site parameters have moved on `origin/master`.** At `v2.10.3.15`
the block reads `AddReceiver(0, 1024, 15, 8192)` and `AddReceiver(1, 128, 15,
8192)`. On `origin/master` (`console.cs:1124-1128`) RX1 is
`AddReceiver(0, 720, 30, 262144)` and the RX2 lines are **double**-commented,
leaving one endpoint. The newer values are more informative (30 fps matches
the §2.5 screenshot, 262144 is `DefaultMaxFftSize`), but the two-pan
configuration this document previously cited as evidence for per-viewport
variation is no longer live upstream. That argument still holds on its own
merits (§9.5) and no longer rests on an upstream configuration.

### 2.4 Thetis was un-archived on 2026-07-02 to build remote operation

From the current upstream `ReadMe.md`, quoting Richie MW0LGE:

> Development is continuing for remote op access, with a full permission and
> state system, display codecs, opus support, remote web client (MW0LGE),
> remote windows client (OE3IDE). TCI will support WS and WSS connections.
> This is being implemented natively in Thetis and will not require 3rd party
> solutions.

ETA is stated as "when it is done, perhaps a few months from now."

**None of it is public, verified by absence rather than inferred.** The whole
`Project Files/Source` tree searched for the vocabulary MW0LGE's own test
client exposes:

| Term | Files containing it |
| --- | --- |
| keyframe / Keyframe / KeyFrame | 0 |
| adaptive block / AdaptiveBlock | 0 |
| quantiz / Quantiz | 0 |
| frames per line / FramesPerLine | 0 |
| blockDelta / BlockDelta | 0 |

Zero in every case. No TCI display stream either: `TCIServer.cs` has 12
occurrences of "display", of which **ten** are calibration-offset fields
passed through `sendCalibration`, one is a code comment, and one is a
`ClickTuneDisplay` CTUN flag. None is a stream. No Opus anywhere
(`packages.config` at `origin/master` has no Opus package; the only
`opus_types.h` is vendored inside rnnoise). No compression of any kind;
`encodeSamples` is pure format conversion. No TLS: the WebSocket layer is
hand-rolled over a raw `TcpListener` (`TCIServer.cs:1876 [v2.10.3.15]`). No
permission or state system.

Expressed as a pipeline, what exists stops abruptly:

```
WDSP analyzer → GetPixels() → float[1024] dBm → [codec] → [display stream] → [WSS] → client
   PRESENT        PRESENT         PRESENT         ABSENT       ABSENT          ABSENT
```

**Upstream movement, stated accurately.** `origin/master` is five commits
ahead of `v2.10.3.15`: four `ReadMe.md`-only edits, plus `8071b543` ("n1mm
fix") which despite its title touches 10 files **including
`clsSpectrumProcessor.cs` (15 lines) and `console.cs` (76)**. So there has
been movement, confined to the reducer class and its disabled test harness.
`TCIServer.cs` is genuinely untouched. **No wire format, no codec, and no
transport has appeared publicly.**

### 2.5 Measured targets, which are per panadapter

Two screenshots of MW0LGE's in-development clients:

*Thetis Remote Web Client v0.1.132:* RX 94.4 kbit/s total, RTT 27 ms, 30.0 fps
spectrum, "Audio worklet 181 ms resample 1.000213x".

*Test TCI Audio Spectrum Client:* Display in 104.00 kbit/s, Audio in 39.55
kbit/s, total in 144.18 kbit/s, **out 0.24 kbit/s**. Settings: 1024 pixels,
30 fps, FFT 65536, Hann window, Detector Peak, Averaging Log Recursive, Avg
120 ms, 384 kHz display span.

ON7OFF independently claim "under 100 kbps".

**Three caveats that must travel with these numbers.**

**They are per panadapter, and RX-only.** "Out 0.24 kbit/s" means no
microphone was active. Any multi-pan or TX budget must scale from here, not
adopt it (§9.5, §10.3).

**They are a best-case codec configuration.** 104 kbit/s was measured at FFT
65536 over 384 kHz at 30 fps, which is 80.5% inter-frame overlap. Delta-coding
efficiency is driven by overlap, and our own default wide view runs at 0%
overlap (§9.2).

**They do not sum.** 104.00 + 39.55 = 143.55 against a reported total of
144.18, leaving 0.63 kbit/s attributed to neither stream. Presumably control
traffic, but it is unexplained.

**Header overhead is significant.** The Opus target is 24,000 bit/s but
measured audio is 39.55 kbit/s. At 40 ms frames that is 25 frames/sec, and a
64-byte TCI binary header costs 25 x 64 x 8 = **12.8 kbit/s**, roughly a third
of the audio budget. This motivates §7.2's framing choice and §7.3.

### 2.6 Codec parameters observed in MW0LGE's test client

**These come from a screenshot of a UI, not from source.** We cannot port from
them; they inform parameter choice only, and the display codec is
NereusSDR-original (§9.2). Everything §9.2 and §9.5 derive from them inherits
this unverifiability.

*Display codec:* Bits 8, Min dBm -135, Max dBm -30, Delta on, Block delta on,
Adaptive blocks on, Keyframe 120, Accuracy "1 Normal", Frames per line 2.

*Opus:* application `audio`, signal `music`, bandwidth `mediumband`, bitrate
24000, VBR and constrained VBR on, complexity 10, FEC off, DTX off, frame
40 ms. See §9.3: this triple cannot all take effect.

### 2.7 What we already have

| Piece | Location | Note |
| --- | --- | --- |
| WDSP `detector()` verbatim port, bin to pixel, 5 modes | `src/gui/spectrum/SpectrumDetector.{h,cpp}` | **Not GUI-free**: includes `gui/SpectrumWidget.h` for its enum |
| WDSP `avenger()` verbatim port | `src/gui/spectrum/SpectrumAvenger.{h,cpp}` | Genuinely clean, `<QVector>` only |
| FFT engine, FFTW3, 7 windows, `kMaxFftSize` 262144 | `src/core/FFTEngine.{h,cpp}` | Emits **linear power**, not dBm |
| Overlap-based frame advance | `FFTEngine.cpp:621` | `advance = sampleRate / fps`, clamped to `fftSize` |
| Pan-to-stream **topology** (no frames flow through it) | `src/core/FFTRouter.h` | See §9.1; used as a synchronous oracle |
| Slice-to-stream many-to-one binding | `src/core/SliceStreamAllocator.h` | Why slice, stream, FFT and pan are four numbers |
| Per-slice audio summing | `src/core/MasterMixer.{h,cpp}` | |
| Single-TX invariant, MOX-drop handoff, per-MAC persistence | `src/core/TxSliceArbiter.{h,cpp}` | See §7.1: it does **not** wait for confirmation |
| Wideband transform and accumulator | `src/core/WidebandFftEngine`, `WidebandFrameAccumulator` | §9.6 |
| Performance instrumentation | `src/core/PerfMonitor` | Reuse for §17 item 2 |
| Memory pressure and locking | `src/core/MemoryPressure`, `MemoryLock` | |
| Route probing | `src/core/RouteProbe` | §10.6, §10.7 |
| Realtime audio scheduling | `src/core/RealtimeAudioPriority` | §4.3 |
| Waterfall cadence | `src/core/WaterfallTicker` | §9.2 frames-per-line |
| Qt WebSocket server, configurable bind | `src/core/TciServer.cpp:1267` | `NonSecureMode` only; no TLS anywhere in tree |
| 3-priority send queue | `src/core/TciSendQueue.h` | Urgent / Binary / Control |
| Opus, static, `OPUS_DRED` + `OPUS_OSCE` | `third_party/rade/cmake/BuildOpus.cmake` | |
| r8brain polyphase resampler | `third_party/r8brain/` | |

`PanadapterStack` is deliberately **not** in this table: it is a QWidget and
therefore client-side only.

### 2.8 Baseline: Phase 3F assumed landed, as a precondition not an assumption

**Baseline: PR #293** (`feature/phase3f-sub-epic-a-foundation`), multi-pan plus
multi-slice, 8 sub-epics, **264 commits** (`git rev-list --count
origin/main..origin/feature/phase3f-sub-epic-a-foundation`; GitHub's PR commit
count caps at 100 and reports a wrong number), +50,091 / -1,520 across 258
files.

**Merge state, verified.** PR #293, PR #291 and PR #306 are **all `state OPEN`,
`mergedAt null`**. #291's head branch (`feature/rfkit-rf2ks-applet`) is **not**
an ancestor of the 3F branch: `git merge-base --is-ancestor` returns false and
it carries 34 commits the baseline lacks, including the mic native-rate
capture fixes §8.2 depends on. An earlier revision of this document asserted
#291 was "already merged into #293", taken from its PR body. That was false.

**Merge-order precondition:** #291 must land into #293 (or both into `main`)
before R4 begins. This document's baseline is `feature/phase3f-sub-epic-a-foundation`
as it stands, without them.

What 3F changes for this design:

- **`FFTRouter` owns topology, not dataflow.** §9.1 attaches to
  `FFTEngine::fftReadyLinear` and uses the router as an oracle.
- **`TxSliceArbiter` already owns TX arbitration**, but not the way this
  document previously claimed (§7.1).
- **`SliceStreamAllocator` makes slice, stream, FFT and pan four different
  numbers** (`SliceStreamAllocator.h:29`). Every bandwidth and endpoint
  argument in §9 depends on this.
- **Multi-pan is a first-class case**, which makes the shared-FFT question in
  §9.1 live and the budget in §9.5 mandatory.
- **AppSettings migrates v5 to v6**, additive with no renames and lazy key
  population (`AppSettings.cpp:1119`), so `SettingsProxy` needs no rename
  map. But `ensureSettingsAtVersion` has exactly **one** call site in the tree,
  `src/main.cpp:280`, so a daemon-first install would run unmigrated (§4.2).
- **Per-slice DSP routing and per-slice audio both landed in Sub-Epic I.** N
  slices are demodulated concurrently and summed by `MasterMixer` into one
  global output (`RxDspWorker.cpp` per-slice loop at `:317`, `rxBlockReady` at
  `:491`; `AudioEngine.cpp:294` preregisters `maxSlices`). An earlier revision
  claimed this was deferred to a "Phase 3F-1", a label that appears **nowhere**
  in the repository. That was false, and it mis-sized §8, §9.3 and §9.5. 3F's
  actual audio non-goal is per-slice **output device** routing
  (`2026-05-26-phase3f-multi-pan-multi-slice-design.md:40`).
- **Wideband extended pan** adds a structurally different second spectrum
  source (§9.6).

Also open and assumed landed: **PR #291** (NB live-rate fix, TX I/Q telemetry,
mic native-rate path), affecting §8.2, and **PR #306** (ANAN-G2E disconnect
wedge fix), affecting §13.

**Consequence for phasing:** R1 is planned against the 3F branch and must not
begin until #293 merges, or must be rebased onto it. The build split touches
CMake target composition, which #293's 258 changed files would conflict with
badly.

### 2.9 Unrelated gap noted in passing

Thetis `v2.10.3.15` has 17 `_ex` TCI commands; our dispatch implements 6.
Missing: `rx_step_att_ex`, `rx_step_att_enabled_ex`, `rx_preamp_att_ex`,
`rx_nb_enable_ex`, `agc_auto_ex`, `vfo_sync_ex`, `vfo_swap_ex`,
`fm_deviation_ex`, `tx_filter_band_ex`, `tx_frequency_ex`, `run_cat_ex`.
Pre-existing TCI parity gap, out of scope, tracked separately.

---

## 3. The split point: daemon demodulates

**Decision: option C. The daemon performs all DSP and streams decoded audio.
The client is a control surface and a renderer.**

| | Cut | Daemon owns | Verdict |
| --- | --- | --- | --- |
| **A** | Raw IQ | Transport only | Rejected: bandwidth. This is `rtl_tcp` |
| **B** | Narrow IQ per slice | Radio, channelization | Rejected: 400 to 800 kbit/s per slice against roughly 100 for decoded audio; needs a daemon-side down-converter that does not exist; network jitter becomes on-air IQ underruns |
| **C** | **Decoded audio** | **Radio, DSP, FFT, audio** | **Chosen** |
| **D** | View-model only | Radio, DSP, and the models, with the client holding no model objects | Rejected: the client's GUI binds directly to `SliceModel` and friends, so replacing them with a thin view-model rewrites every binding. **Note this rejects relocating the models, not mirroring them.** §6.1 mirrors the models precisely so the bindings keep working |
| **E** | Pixels | Everything | Rejected: this is VNC |

Option B was seriously considered, since it would let the client keep running
its own WDSP chain and need no protocol coverage for DSP controls at all. It
was rejected once measured numbers arrived, and §2.2's corrected IQ figures
(3.07 to 24.58 Mbit/s) make the margin wider than previously stated.

### 3.1 Accepted costs

**Codec-over-codec on digital modes.** The daemon demodulates and Opus
encodes. Weak-signal decoding from Opus audio is degraded, and RADE is a
neural vocoder whose output would pass through a second speech codec.
Mitigations: a selectable lossless or high-rate PCM mode, and running WSJT-X
class decoders on the daemon host where the audio is clean.

**Microphone audio is compressed before the TX chain.** Mitigation: a
high-rate or lossless mic uplink when the link allows.

**DSP controls incur one round trip.** Acceptable for toggles, slightly
rubbery on a filter-width drag.

**Per-slice AF gain, mute and pan are daemon-side and pre-mix**, and therefore
also cost a round trip. They are WDSP parameters applied upstream of the mix:
`RxChannel::setAfGain` calls `SetRXAPanelGain1` (`RxChannel.cpp:1397`),
`setMuted` calls `SetRXAPanelRun` (`:1396`), `setPan` calls `SetRXAPanelPan`
(`:1438`), and mute is additionally gated pre-mix in `AudioEngine::rxBlockReady`
before `MasterMixer::accumulate`. An earlier revision claimed these stay
client-side; that is impossible when the daemon encodes a mixed master,
because the samples are already summed, and it would additionally break the
VAX inverse-scale (`AudioEngine.cpp:1025-1037` divides by `afGain` precisely
because the gain is applied inside WDSP), silently miscalibrating a co-located
WSJT-X.

Only the client's own **master** output trim and mute are local. They are a
separate control, are not persisted as station state, and are never written
back into `SliceModel::afGain`. A client-side master mute must additionally
signal the daemon so it can stop encoding, which is a §9.5 bandwidth lever.

**Zoom and pan must remain locally instant**, which constrains §9.4's crop
model. See §9.4.

**PureSignal stays daemon-side.** It requires sample-correlated feedback IQ
and is not exposed for remote real-time operation beyond enable, disable, and
status.

---

## 4. Process and build architecture

### 4.1 Target split

| Target | Kind | Contents | Qt modules |
| --- | --- | --- | --- |
| `NereusCore` | **SHARED** | `CORE_SOURCES` (118) + `MODEL_SOURCES` (15) | Core, **Gui**, Network, WebSockets, Multimedia, **SerialPort**, ZLIB |
| `NereusGui` | **SHARED** | `GUI_SOURCES` (244) + resources, links `NereusCore` | adds Widgets, Svg, GuiPrivate |
| `NereusSDR` | exe | `src/main.cpp` + `NereusGui` | full |
| `nereusd` | exe | `src/server_main.cpp` + `NereusCore` + remote sources | no Widgets, no Svg, no QRhi |

**Shared, not OBJECT, and this is a link-time split rather than a subsystem
split.** `origin/main` carries `2026-07-25-test-execution-speed-phase1-design.md`,
which builds `NereusSDRObjs` as a single shared library and says "do not split
it into subsystem libraries", on measured evidence (rebuilding all tests after
touching one core file: 31.4 minutes as OBJECT against 29.5 seconds as SHARED,
and `build/tests` falling from 12 GB to 1.6 GB).

That decision is an input here, not a constraint, and the two are compatible
anyway. Its measured win comes from shared linkage, which this design keeps.
What it rejects is splitting by *subsystem*, which this is not: the cut here is
the single boundary the daemon actually needs, between code that references a
widget symbol and code that does not. Two shared libraries preserve the
rebuild and disk win while giving `nereusd` a target it can link without Qt
Widgets.

Two consequences to settle during R1 rather than assume: per-library IPO
(`NEREUSSDR_ENABLE_LTO`) must be re-evaluated against two shared targets, since
the phase-1 doc already flags IPO as a risk on one; and symbol visibility needs
an export macro, which an OBJECT library did not require.

*Note: that phase-1 document exists on `main` and not on the 3F baseline, which
is one reason §2.8's precondition is three-way.*

**`Qt6::Gui` and `Qt6::SerialPort` are required by `NereusCore` and must be
stated, not inherited.** Seven core and model entries need `<QColor>` or
`<QDesktopServices>` (`DxccColorProvider.h:45`, `FreeDVStation.h:40`,
`PureSignal.h:56`, `BandPlan.h:17`, `BandPlanManager.cpp:20`,
`SpotTableModel.cpp:35`, `SupportBundle.cpp:16`), and
`src/core/mmio/SerialEndpointWorker` uses `QSerialPort`. `Qt6::Gui` is not
`Qt6::Widgets`; the daemon still links no widget toolkit.

**`NereusSDRObjs_LTO` exists on the baseline** (6 references in
`CMakeLists.txt`, absent from `main`): a second full OBJECT library over the
same sources. Whether it survives the move to shared linkage is part of the
IPO question above.

The daemon build drops Qt Widgets, Qt Svg, the QRhi shader pipeline, and all
244 GUI source entries.

### 4.2 Prerequisites for the split

Nine, not three. An earlier revision listed only items 1, 3, and 7, which
would have produced a daemon that connects to a radio and emits no spectrum
at all.

1. **Extract the view-hook interface.** `src/models/RadioModel.cpp:299`
   includes `gui/SpectrumWidget.h`. `RadioModel` holds **two** non-owning view
   hooks (`m_spectrumWidget` and `m_fftEngine`), both wired exactly once at
   `MainWindow.cpp:3103-3104`. Extract a minimal abstract sink into
   `src/core/`.
2. **Extract the `SpectrumDetector` enum** out of `gui/SpectrumWidget.h` into a
   standalone core header. `SpectrumDetector.h:62` includes
   `gui/SpectrumWidget.h` solely for it, and that header drags in `<QWidget>`,
   `<QRhiWidget>` and `<rhi/qrhi.h>`.
3. **Relocate `SpectrumDetector` and `SpectrumAvenger`** to
   `src/core/spectrum/`. WDSP license headers travel verbatim; PROVENANCE row
   paths updated. `SpectrumAvenger` is already clean; `SpectrumDetector` is
   only clean after item 2.
4. **Extract the FFT engine pool owner.** `MainWindow::createFftEngineForStream`
   (`MainWindow.cpp:1430`) creates per-stream engines, reads four
   **global** AppSettings keys (`DisplaySpectrumFps`, `DisplayFftSize`,
   `DisplayFftWindow`, `DisplayHzPerBinTarget`), and parks every engine on one
   shared `m_fftThread` (`MainWindow.h:597-606`). This is core work sitting in
   a QWidget.
5. **Extract the topology builder.** `MainWindow::rebuildFftRouting`
   (`MainWindow.cpp:2092`) iterates `m_panStack->allApplets()`, a QWidget, so
   without `PanadapterStack` the router stays empty. The daemon needs an
   equivalent driven by remote subscriptions.
6. **Extract the crop-and-reduce stage.** It currently lives inside a
   QRhiWidget (`SpectrumWidget::updateSpectrumLinear`, `SpectrumWidget.cpp:2679+`),
   `visibleBinRange()` is private, and the pixel count is derived from widget
   geometry (`const int displayWidth = qMax(width() - effectiveStripW(), 800);`,
   where `effectiveStripW()` is itself a visibility query). The extracted
   service takes an **explicit pixel count** and an explicit
   `(centreHz, spanHz)` window. This is a refactor, not a relocation.
7. **Migration entry point.** `ensureSettingsAtVersion` has one call site,
   `src/main.cpp:280`. Either `src/server_main.cpp` calls it too, or the call
   moves into a shared core init used by both binaries.
8. **Preserve the `NereusSDRObjs` target name** as an INTERFACE aggregate of
   `NereusCore` + `NereusGui`. `tests/CMakeLists.txt` has **561**
   `nereus_add_test` call sites routed through one helper that does
   `target_link_libraries(${name} PRIVATE NereusSDRObjs Qt6::Test)` and
   `REUSE_FROM NereusSDRObjs`. Removing the name means rewriting the helper and
   its label-derivation logic.
9. Build-and-fix pass for any remaining Widgets dependency.

### 4.3 Optional audio, and realtime scheduling on a headless host

Qt Multimedia stays linked so a daemon on a shack machine can monitor locally,
but a headless host with no sound device must start cleanly. Audio output is
optional at runtime and its absence is not an error.

`src/core/RealtimeAudioPriority` already exists and should be reused; a
headless daemon has different RT-scheduling constraints from a desktop app and
this is where they are expressed.

### 4.5 Hardware floor, and what it forces

**Floor: Raspberry Pi 4 Model B Rev 1.5, 8 GB, quad Cortex-A72. Target:
Raspberry Pi 5, quad Cortex-A76.** The floor is roughly 2.5x to 3x slower
than the target, which is wide enough that "it runs on a Pi 5" says almost
nothing about whether it runs at all.

Three consequences the design has to absorb rather than discover at bench:

**Capacity is a runtime property, not a SKU property.** §7.0's capability
descriptor currently advertises `maxSlices` and `userDdcCount` straight from
`BoardCapabilities`, which describes what the *radio* supports. On the floor
the *daemon* may not sustain that. The descriptor therefore advertises
**effective** limits: sustainable slice count, maximum aggregate spectrum
pixel-rate, and whether the display codec is available at all. The client
gates its UI on the effective values, never the board values.

**A stated degradation ladder, applied in this order**, so the daemon sheds
load predictably instead of collapsing unpredictably:

1. Reduce spectrum frame rate, then pixel count, per §9.5's budget.
2. Increase frames-per-line so the waterfall advances less often.
3. Disable spectrum endpoints for pans the client has not marked visible.
4. Refuse additional slices beyond the sustainable count.
5. Never degrade audio, and never drop TX. Audio starvation is audible and TX
   loss is a safety event (§12); spectrum degradation is cosmetic.

`PerfMonitor` drives the ladder, and every step is reported to the client so
the operator can see why the waterfall slowed rather than assuming a fault.

**The shared FFT thread is the first thing to break.** `MainWindow.h:597-606`
already flags `m_fftThread` carrying every pooled engine as unresolved at
5 streams and 1536 kHz, with per-engine threads noted as a follow-up needing
sign-off. On four A72 cores that is far more likely to bind than on a Pi 5,
and the daemon has to resolve it rather than inherit the open question.

**One useful consequence.** The CM4 inside the Saturn board is also a
Cortex-A72. Making the Pi 4 the floor therefore makes the on-board daemon in
§16 a CPU-budget question we will have already answered, rather than a
separate unknown.

### 4.5a Measured floor capacity (2026-08-02)

Measured on the floor hardware: Raspberry Pi 4 Model B Rev 1.5, quad
Cortex-A72, governor pinned to `performance` at 1.8 GHz, Debian 13 aarch64,
FFTW 3.3.10, Opus 1.5.2, `-O3 -march=native`. The harness mirrors
`FFTEngine::processFrame`: Hann window multiply, complex forward FFT, then
linear power, which is what `fftReadyLinear` emits.

**Per-stream spectrum cost, one core:**

| FFT size | us/frame | max fps, 1 core | % core at 30 fps | 5 streams at 30 fps |
| --- | --- | --- | --- | --- |
| 4,096 | 59 | 16,971 | 0.2% | 0.9% |
| 16,384 | 312 | 3,208 | 0.9% | 4.7% |
| 65,536 | 2,336 | 428 | 7.0% | 35.0% |
| 262,144 | 11,772 | 85 | 35.3% | **176.6%** |

**Thread scaling at 65,536**, which is the load-bearing measurement:

| Threads | Worst us/frame | fps per thread | Aggregate fps |
| --- | --- | --- | --- |
| 1 | 2,352 | 425 | 425 |
| 2 | 3,631 | 275 | 551 |
| 4 | 6,993 | 143 | 572 |

**Opus encode** at the §9.3 settings (48 kHz mono, 40 ms, complexity 10,
24 kbit/s VBR + constrained, `audio`/`music`): 427 us per frame, **1.07% of one
core continuous**, 5.33% for five slices, producing 23.8 kbit/s against the
24,000 target. Thermals reached 68 C with `get_throttled` at `0x0`.

**Four conclusions, three of which change the design.**

**1. FFT size dominates everything; stream count is secondary.** Going from
4,096 to 262,144 costs 200x. Five streams at 4,096 cost less than one tenth of
what a single stream at 262,144 costs. The expensive axis is zoom depth, not
slice count.

**2. The single shared `m_fftThread` has a measured breaking point.** Five
streams at 30 fps need 35% of one thread at 65,536 and **176% of one thread at
262,144**, which one thread cannot deliver. It holds comfortably to 65,536, is
marginal around 131,072, and fails at 262,144. §4.5's prediction that this
binds first is confirmed with a number attached.

**3. Adding threads does not rescue it, and this is the finding that matters
most.** Four threads yield only **1.35x** the aggregate FFT throughput of one
(572 fps against 425), because a 65,536-point complex transform moves 512 kB in
and 512 kB out against a 1 MB shared L2, so the workload is memory-bandwidth
bound rather than core bound. The "one thread per engine" follow-up flagged at
`MainWindow.h:597-606` therefore **does not fix the 262,144 case**: single
thread manages 85 fps, aggregate is roughly 115 fps even granting the same
1.35x (and the 4x larger working set makes that optimistic), while five streams
at 30 fps need 150 fps. **Five deep-zoomed pans at maximum FFT size is not
achievable on the floor hardware at any threading arrangement.**

**4. Opus and the display codec are rounding errors.** At 1.07% of a core per
slice, audio encoding is not a constraint on this hardware and needs no
mitigation.

**Consequences.**

The §9.1 two-shared-FFT tier moves from a deferred optimisation to the
**recommended default**, because the measurement makes its value concrete: one
wide-and-fast engine at 4,096 (0.2%) plus one narrow-and-fine at 65,536 (7.0%)
serves a mixed multi-pan layout for **7.2% of one core**, against 176% for five
independent engines at maximum size. That is a 24x difference for a
visually equivalent result.

The daemon must **cap effective FFT size from measured capacity**, not from
`kMaxFftSize`, and advertise the cap through §7.0's effective-limits
descriptor. §9.1's physics table stands as physics; what changes is that the
deepest rows are not simultaneously available across many pans on the floor.

**Still unmeasured, and the remaining gap: WDSP RX per slice.** The spike
covered FFT and Opus, not the demodulation chain, because that needs WDSP built
on the target. Until it is measured, the per-slice DSP budget is unknown and
these numbers describe the spectrum and audio-encode paths only.

### 4.4 Packaging, and the two-writer problem

**One installer, two binaries, daemon off by default.** The desktop package
ships both. Out of the box the GUI runs in its existing in-process direct mode
and the user never learns the daemon exists. Enabling a home server is a Setup
toggle. The Pi package ships `nereusd` and a systemd unit only.

**The daemon uses a distinct settings store.** With the toggle enabled both
binaries would otherwise call `AppSettings::instance()`, resolving to one
per-user file whose save path is a whole-file read-modify-write with `.bak`
rotation, and there is no cross-process locking primitive anywhere in the tree
(`git grep -l "QLockFile\|QSharedMemory\|QLocalServer" -- src` returns
nothing). Two writers race and can lose settings, the exact failure class a
v0.5.1 fix already addressed once. `AppSettings` already supports a named
profile subdirectory (`.../NereusSDR/profiles/<name>/`), so a reserved daemon
profile name is the answer.

**Enabling the toggle must first tear down the GUI's in-process radio
connection.** The two binaries must never hold the radio concurrently.

---

## 5. Phasing and the always-client-server end state

**v1 adds the daemon and remote client mode. The existing in-process local
path is left untouched.**

The end state is that the GUI always talks to a daemon, including locally over
loopback. That is architecturally correct and removes dual-mode entirely. It is
not the first step, because `RadioModel` owns `WdspEngine` in-process, so
switching wholesale requires every control mirrored before the application
works at all.

Running the GUI against `localhost` works from day one, so the "sit at the Pi"
case is covered by v1 regardless.

**Standing rule that keeps the end state reachable:**

> No feature may be implemented in a way that only works in-process. Every new
> DSP or radio control goes through the model `Q_PROPERTY` layer that
> `StateMirror` reflects, and **every new runtime collection needs a lifecycle
> event, not only new properties** (§6.1).

---

## 6. State synchronization

The naive form needs a wire command, a daemon handler, a client branch, and a
connect-time sync path per settable parameter. For NereusSDR that is
realistically 300 to 500 parameters (`MicProfileManager` alone bundles **93**
keys, `MicProfileManager.h:207`). Rejected.

Three generic mechanisms are required, not two.

### 6.1 `StateMirror`: properties, identity, and lifecycle

Reflects `Q_PROPERTY` declarations over `QMetaObject`: enumerate, subscribe to
`NOTIFY`, transmit on change, apply on the far side.

Coverage on the baseline, **139 declarations** (`grep -c 'Q_PROPERTY('` per
file against the 3F branch):

| Model | Q_PROPERTY | No WRITE accessor | Instances at runtime |
| --- | --- | --- | --- |
| `SliceModel` | 98 | 4 | x `maxSlices` (up to 5) |
| `TransmitModel` | 15 | 3 | x1 |
| `TunerModel` | 13 | **13** | x1 |
| `RadioModel` | 5 | 4 | x1 |
| `MeterModel` | 4 | **4** | x1 |
| `PanadapterModel` | 4 | 0 | **excluded, client-only (§9.4)** |

**The live mirrored surface is roughly 500 property instances on a 5-slice SKU,
not 139.** `SliceModel` is a runtime collection.

**Three classes of property, and the generic mechanism handles only one of
them unaided:**

**Bidirectional.** Operator-settable state. The mechanism as described works.

**Read-only telemetry: 28 of the 139 have no WRITE accessor**, so
`QMetaProperty::write()` returns false and a blind apply silently no-ops. This
is not a corner: it is **all 13 of `TunerModel`** (the entire ATU surface),
**all 4 of `MeterModel`** (`forwardPower`, `swr`, `alc`, `sMeter`), 4 on
`RadioModel` including `connected`, 4 on `SliceModel`, and 3 on
`TransmitModel`. Each needs either a mirror-side setter or a per-model
`applyMirroredValue(name, variant)` hook. This also settles §8.1's open meter
question: `MeterModel`'s four are in this set.

**Derived, and dangerous: writable but daemon-authoritative.** Phase 3F added
`SliceModel` properties that are writable yet owned by `RadioModel` and
`SliceStreamAllocator`, not by the operator: `chainIndex`, `ddcIndex`,
`streamIndex`, `shiftOffsetHz`, `panKey`, `sampleRateHz`, plus
`widebandExtensionRequested` and `psPaused`. `SliceModel`'s own header says so
of `sampleRateHz`: writing the property directly "moves the display only, and
the next bind or rate change overwrites it"; the verb is
`RadioModel::requestSliceSampleRate`.

**A blind apply-on-the-far-side would desynchronise the allocator**, and echo
suppression does not help because these are not echoes. Every mirrored property
therefore carries a **direction classification**: derived properties are
transmitted outbound and never applied inbound, and the client must use a
command verb instead (`requestSliceSampleRate`, `bindSliceToStream`,
`addSliceOnPan`, `requestTxHandoffToSlice`). §14 asserts that an inbound write
to a derived property is rejected rather than applied.

**Properties alone are insufficient**, and this is a structural gap rather than
a documentation one:

- `SliceModel` has **no** `sliceIndex` Q_PROPERTY. Nothing in the mirrored set
  identifies which slice a tuple belongs to.
- `RadioModel::slices()` is a plain getter (`RadioModel.h:446`), not a
  property, so the far side never learns the collection changed.
- `sliceAdded(int)` / `sliceRemoved(int)` (`RadioModel.h:2184-2185`) are
  ordinary signals, not any property's `NOTIFY`, so a property-enumerating
  mirror cannot see them.

**Required additions, all R2 prerequisites:**

1. **A stable wire object-id scheme**: type tag plus persistent id, explicitly
   **not** list position. Note ids are **reused**: `RadioModel` mints
   lowest-free, so an id freed by a removal is handed out again, and the wire
   protocol must tolerate that.
2. **Object create and destroy messages** for the slice and pan collections,
   driven by `sliceAdded` / `sliceRemoved` / `activeSliceChanged` and the pan
   equivalents, plus the `Q_INVOKABLE` creation verbs (`addSliceOnPan`,
   `removeSlice`, `requestTxHandoffToSlice`) as explicit wire commands.
3. **A connect-time snapshot** with a defined message shape, an ordering
   guarantee relative to change deltas, and an explicit **snapshot-complete
   marker**. §12.2's TX gate and §13's resync both depend on this and neither
   can be implemented without it.
4. **Add `Q_PROPERTY(int sliceIndex READ sliceIndex CONSTANT)` to `SliceModel`**,
   or carry the handle out of band.

Echo suppression uses the existing `m_updatingFromModel` / `QSignalBlocker`
pattern.

### 6.1a Collection and record streams (the third mechanism)

`SpotModel`, `SpotTableModel`, `FreeDVStationModel` and `RxDecodeModel` each
declare **zero** `Q_PROPERTY`s. They are append-and-expire record streams, not
property bags, and the property mirror cannot carry them at all. The fault log
is the same shape.

A third mechanism is required: an append or upsert or expire record stream
keyed by record id, with a bounded backlog on connect. Until it exists, §6.4's
daemon-side collector placement cannot be implemented.

### 6.2 `SettingsProxy`: allowlist, snapshot, write-through

`AppSettings` has **593 `AppSettings::instance()` call sites across 88 files**
on the baseline.

**A naive proxy is not implementable.** `AppSettings::value()` is synchronous
and returns a value; proxying every call makes each one a network round trip on
the calling thread, and building the 47-page Setup dialog would perform
hundreds of sequential round trips on the GUI thread.

**Design:**

- **Key-prefix allowlist.** Station keys proxied, operator keys local,
  `audio/*` explicitly local. A blanket forward would ship the client's
  machine-local audio configuration into the daemon's store:
  `AudioDeviceConfig.cpp` persists ten fields under `audio/<prefix>/`
  including `DriverApi`, `DeviceName`, `SampleRate`, `BufferSamples` and
  `ExclusiveMode`.
- **Bulk snapshot at session start**, so reads stay synchronous and local out
  of a cache. This needs a new bulk-snapshot entry point on `AppSettings`.
- **Write-through**, with defined offline behaviour.
- **Both scopes.** The proxy must handle global and per-MAC keys (§6.3).

The 47 Setup pages keep working unchanged **because the cache makes reads
local**, not because a singleton is inherently remotable.

### 6.3 The settings boundary, by key prefix

Concepts are ambiguous; key prefixes are not.

**Station, proxied to the daemon:** `hardware/<mac>/*` (radio info, sample
rate, active RX count, TX-bound slice, DDC routing overrides, PA and
calibration profiles, antenna and Alex state, step attenuator, accessory
config).

**Operating state, daemon-authoritative and mirrored live:** `Slice<N>/*` and
`Slice<N>/Band<X>/*`. Note these are persisted under **global** keys with no
`hardware/<mac>/` component, while the TX binding beside them **is** per-MAC
(`TxSliceArbiter.cpp`: `hardware/%1/TxBoundSliceIndex`). **The store therefore
cannot be partitioned by scope prefix alone**, which an earlier revision
assumed.

**Operator, local to the client:** window geometry, `PanLayoutId`,
`PanSplitter*Sizes`, applet visibility, skins, colour schemes, waterfall
palette, grid colours, fonts, and all of `audio/*`.

**Three unrelated things are called "sample rate"** and must be disambiguated
by key wherever the word appears:

| Key | Meaning | Owner |
| --- | --- | --- |
| `hardware/<mac>/radioInfo/sampleRate` | Radio DDC rate | Station |
| `audio/<prefix>/SampleRate` | Local audio device rate | Operator, machine-local |
| `Slice<N>/Band<X>/SampleRate` | Stream window width | Operating state |

**The display-settings line runs through one Setup page**, and splitting it is
larger than it looks. See §17 item 3.

### 6.4 Spot client placement

Split by nature. DX cluster, RBN, POTA and PSK Reporter are station-level: they
should keep collecting with the GUI closed, and PSK Reporter should report the
station's location. WSJT-X UDP is local to wherever WSJT-X runs. Both
placements configurable; these are the defaults.

**Blocked on §6.1a**: these are all record streams.

---

## 7. Session and wire

### 7.0 Connect sequence, versioning, and capabilities

An earlier revision contained none of this. Grepping it for "version" returned
one hit, and the match was the substring inside "conversion".

**Connect sequence:** TLS establish → protocol hello carrying a semantic
version from both ends → authentication → capability exchange → state snapshot
→ snapshot-complete marker → TX gate opens (§12.2).

**Version policy: refuse on major mismatch, negotiate down on minor.** Both
ends advertise `major.minor`. Differing major means an incompatible wire
contract, so the connection is refused with a message naming both versions
rather than failing obscurely. Equal major with differing minor negotiates down
to the lower, and each side gates optional behaviour on the agreed value. A
desktop GUI at R4 pointed at a Pi still running R2 is the expected case, not an
error case, and it must degrade to R2 behaviour rather than refuse.

Major is incremented only for a breaking change to framing, identity, or the
snapshot contract. Everything else is a minor bump plus a capability bit.

**Capability descriptor**, advertised by the daemon: SKU, `maxSlices`,
`userDdcCount`, available modes, PureSignal present, wideband present, TX
permitted, supported audio codecs, supported display-codec versions, max
pixels, max frame rate, AppSettings schema version. The client's UI gates on
these.

Every §15 phase states which capability bits it adds.

### 7.1 Session model

**Single operator, one session at a time.** Authentication is a generated
pre-shared token, never user-chosen, rate-limited on failure. **The token
distribution mechanism must be specified before R2**: daemon console output on
first run, plus the desktop Setup toggle, alongside the TLS fingerprint
(§10.5).

Every connection carries a session identity and a role from day one, even
though the role is always owner.

**A second authenticated connection preempts the existing session.** The token
is the authority, and the realistic sequence is the same operator reconnecting
after a link drop: hotel Wi-Fi fails, §12.1 fires, §13 has the daemon retain
radio state, and the operator reconnects from a phone. If the stale session
survives until its heartbeat deadline, or a second connection is refused, the
operator is locked out of their own transmitter for an undefined interval,
which is the opposite of the control-operator requirement §12.1 invokes. The
displaced session is told why, and MOX drops across the transition.

**TX arbitration already exists and must not be reinvented, but not as
previously described.** `TxSliceArbiter` owns the single-TX invariant across
up to `maxSlices` slices, persists the bound slice per-MAC, and re-establishes
the invariant after every slice-list mutation via `syncToSliceList()`.

Two corrections to an earlier revision:

**It does not wait for MOX confirmation.** `requestHandoff` calls
`m_mox->setMox(false)` and flips the `isTxSlice` flags on the next statements,
with an in-code comment noting that if `setMox` ever becomes async, a
`QEventLoop` wait would be needed. This is sound in-process because `setMox`
is synchronous on the `moxChanged` side. **It does not hold across a network
link**, where MOX state round-trips. Remote handoff must add an explicit
unkey-confirmed gate before the flag flip, and **R4 owns that gate**.
(`TxSliceArbiter.h:46` carries a stale comment claiming a confirmation wait
and contradicts its own `.cpp`; fix separately.)

**Remote requests must not call `requestHandoff(int)`, which is positional.**
`TxSliceArbiter.h:59` is explicit that the index is a list position, not a
`SliceModel::sliceIndex()`, and that resolving it as an id picks the wrong
slice once a mid-list removal makes ids and positions diverge (`removeSlice`
does not renumber survivors). Remote TX routes through
`RadioModel::requestTxHandoffToSlice(int sliceId)`, the id-based entry point.

**General rule: every wire message naming a slice carries
`SliceModel::sliceIndex()`, never a list position**, and the daemon resolves
via `RadioModel::sliceById`.

### 7.2 Framing

**Split by payload: use a standard where one genuinely exists for that
payload, and do not pretend one exists where it does not.**

| Stream | Framing | Path |
| --- | --- | --- |
| Audio, both directions | **RTP, Opus per RFC 7587** | Own stream, unreliable, **never coalesced** |
| Spectrum | Compact native | Coalesced (§7.3) |
| Meters, state, control | Compact native | Coalesced, reliable |
| Context and metadata | Control channel | Reliable |

**Audio leaves the mux entirely.** Opus-in-RTP is specified, so the stream is
decodable by anything speaking RTP. RTP's timestamp is already sample-count at
a defined clock rate, which is what drift correction needs (§8.3). RTCP
receiver reports carry loss, jitter, and round-trip time as standard, which is
the link-quality panel we would otherwise build worse ourselves. And keeping
audio out of the coalesced datagram is what makes §10.4's loss-concealment
argument true: coalescing audio with a spectrum frame means one loss destroys
both.

**Spectrum gets native framing** because no envelope helps: whatever standard
we wrapped it in, the payload would still be ours, so the envelope buys only
stream identity, sequence, and timing, which fit in roughly twelve bytes.

**Why not VITA-49 for spectrum**, given FlexRadio uses it for the same job.
Three of its ideas are adopted:

- **A stream identifier** per source.
- **Context carried separately from data**, so a retune does not glitch frames
  already in flight. We carry context on the control channel rather than as
  VITA context packets, which is also what FlexRadio does.
- **A sample-count timestamp** rather than wall-clock.

The standard itself is not adopted. Its two real payoffs are wire dissection
by standard tools and ecosystem interoperability, and **both are neutralized
here**: the stream runs inside TLS or DTLS to a single client, so a capture
shows ciphertext, and there is no standard VITA-49 payload for reduced
spectrum bins anyway. VITA-49 has no Opus payload either, which is one reason
audio goes on RTP.

Header size decides nothing: at 30 fps a 28-byte VITA-49 header costs 6.7
kbit/s against 2.9 for a 12-byte native envelope, a 3.8 kbit/s difference
against roughly 100, under four percent.

**The existing 64-byte `TciBinaryFrame` header is not reused for the remote
path.** **Forty** of its sixty-four bytes are reserved zeros (offsets 12-19
and 32-63, `TciBinaryFrame.h:101-107`), it carries no timestamp and no context
concept, and §7.3 needs compact sub-frame headers regardless. It remains
unchanged for the existing TCI server, a separate surface.

The `TciSendQueue` three-priority drain **is** reused for the coalesced path;
it is orthogonal to header layout.

**The framing must be transport-agnostic.** The transport changes across the
ladder in §10 and must not be visible above the framing layer.

**Deliberately left open, and reversible:** a header carrying these semantics
maps mechanically onto VITA-49 later should SmartSDR-family interoperability
become a goal. The PGXL work already left a VITA-49-style discovery beacon
(`FlexRadioDiscoveryBroadcaster.cpp:272`) and a minimal SmartSDR API server.
Product decision, not a technical blocker.

### 7.3 Coalescing, and the reliability class of each envelope

**There are two coalesced envelopes, not one, because reliability is a
property of the envelope and the payloads disagree about what they need.** An
earlier revision left this unstated while §7.2, §7.3 and §10.4 each implied a
different answer, and it determines the codec: a delta chain on an unreliable
channel needs the loss detection in §9.4, while one on a reliable channel does
not.

| Envelope | Class | Carries |
| --- | --- | --- |
| Control | Reliable, ordered | State deltas, lifecycle events, snapshot, capability, keyframe requests, commands |
| Media | Unreliable | Spectrum frames, meter updates |
| (Audio) | Unreliable, own RTP stream | Not coalesced at all (§7.2) |

Meters ride the unreliable envelope because they are periodic telemetry at
10 fps; a dropped one is superseded 100 ms later. State deltas must not, because
a lost delta desynchronises the mirror with no way to detect it.

**Consequence for §9.4:** spectrum is on the unreliable envelope, so the
encoder sequence number and keyframe-on-gap rule are mandatory, not optional.

What remains still justifies coalescing:

Per session per tick: up to N spectrum frames (one per subscribed pan, §9.5)
plus meter updates at **10 fps** (`MeterPoller.h:168`, "Default 100ms (10 fps)
from Thetis MeterManager.cs") plus state deltas. Each sub-frame otherwise pays
its own envelope plus transport framing.

**One message per tick carrying a multiplex header and N sub-frames.**
`TciSendQueue` already drains on a 5 ms timer.

**Recompute the justification against actual envelope sizes** (roughly 12
bytes native, not the discarded 64-byte TCI header) once the spectrum term is
known, and note that the spectrum term multiplies with pan count. If the
recomputed figure does not support "required", restate coalescing as an
optimisation rather than a requirement.

---

## 8. Data flow

### 8.1 Receive

The RX path fans out on **three independent axes**, and slice count, stream
count, FFT count and pan count are four different numbers
(`SliceStreamAllocator.h:29`).

```
Radio → P1/P2RadioConnection → ReceiverManager → per-stream I/Q (0..userDdcCount-1)

  for each stream s:
    → FFTEngine[s]  (linear power + windowEnb + dbmOffset)
        → N x SpectrumEndpoint (one per subscribed pan; own pixels, fps, crop)
            → DisplayCodec → coalesced mux
    → RxDspWorker → RxChannel[slice] for every slice bound to s
        → AudioEngine::rxBlockReady(sliceId, ...) → MasterMixer
            → [one master encoder OR N per-slice encoders: OPEN, §16]
                → Opus → RTP (not coalesced)
    → per-slice meters (RxChannel::getMeter) + global TX meters → coalesced mux

  model property changes + collection lifecycle → StateMirror → coalesced mux
```

**Meters need their own decision.** `MeterPoller` is GUI-resident
(`src/gui/meters/MeterPoller.cpp` includes `MeterWidget.h`, `MeterItem.h` and
`gui/SMeterWidget.h`) so it does not compile into a Widgets-free core. And
`MeterModel` is a global with four scalars, while per-slice S-meter bypasses it
entirely via `MeterPoller::sliceSmeterUpdated(int sliceIndex, double dbm)`
consumed by a per-flag lambda in `MainWindow`, so routing meters through
`MeterModel` loses N-1 of the N S-meters. Either extract the polling loop to
core (leaving the `MeterWidget` push GUI-side) or have the daemon poll
`RxChannel` and `TxChannel` directly. Per-slice S-meter has no model backing
today.

### 8.2 Transmit

```
client: mic → PcMicSource → OpusAudioCodec → RTP
daemon: RTP → Opus decode → TX jitter buffer (§8.3) → TxMicRouter
        → TxWorkerThread → TxChannel (full TXA chain) → RadioConnection
```

The complete TX processing chain (EQ, Leveler, ALC, CFC, CPDR, CESSB, Phase
Rotator, DEXP/VOX) runs on the daemon at the radio's clock, unchanged. This
path depends on PR #291's mic native-rate capture fixes (§2.8).

### 8.3 Audio clock discipline, both directions

Daemon and client clocks are independent and will drift.

**Receive.** Jitter buffer around 180 ms, adaptive, plus continuous drift
correction by resampling at a ratio near unity driven by buffer depth.
`third_party/r8brain` is the tool. MW0LGE's client shows 1.000213x in steady
state.

**Transmit.** A TX-side jitter buffer is required and was previously
unspecified despite appearing in §8.2's diagram. **Its depth must be strictly
less than the §12.3 uplink-starvation deadline**, and both numbers stated
together, or the buffer masks the condition the watchdog exists to catch.

### 8.4 Reconfiguration events

An earlier revision defined none of this; grepping it for "mode change" and
"band change" returned zero lines. Three routine operator actions have
undefined remote behaviour, each breaking a different part of the design.

For each of **sample-rate change, mode change, band change, slice add/remove,
and stream retune**, this design must state: what the daemon sends, whether
spectrum endpoints are torn down or reconfigured, whether a keyframe is forced
(§9.2), whether audio restarts or reconfigures in place, and whether TX is
inhibited during the transition.

Specific hazards:

- **Sample-rate change** is a station-wide 12-step live-apply sequence
  (`RadioModel::setSampleRateLive`, `RadioModel.h:1263`) that retunes every RX
  channel. It is **not** a per-endpoint client-set parameter and is removed
  from §9.4's client-owned list.
- **Mode change into RADE swaps the entire DSP channel object**
  (`RxChannel` → `RadeChannel`) and inserts a 48-to-16 kHz resampler in the TX
  path. §9.3's "bandwidth tracks the mode" is therefore a mid-stream codec
  reconfiguration needing explicit signalling.
- **Band change** triggers tune-memory recall, Alex routing, PGXL band
  notification, and per-band grid and display-setting reload, the last of
  which straddles §6.3's station/operator line.

---

## 9. Spectrum

### 9.1 One FFT per stream, N viewport reducers

**Decision: one `FFTEngine` per stream at full size, with a per-subscription
`SpectrumEndpoint` that crops to that viewport's bin range and reduces to its
pixel count.**

The unit owning an `FFTEngine` is a **stream** (`0..userDdcCount-1`), not a
receiver or a hardware DDC number. Keying on the DDC number was a live defect
fixed during 3F (`MainWindow.cpp:2638-2643`).

**`FFTRouter` owns topology only; no frames flow through it.** An earlier
revision named `FFTRouter::fftFrameForPan` as the insertion point. That signal
has **zero emitters** and `onFftFrame` **zero callers**; a `SpectrumEndpoint`
connected to it would receive nothing, forever. `FFTRouter.h:18-22` says as
much itself. The live path is `MainWindow::dispatchFftFrameToPans`
(`MainWindow.cpp:2188`), which deliberately uses the router as a synchronous
oracle rather than a signal hop, for the reason stated at `:2025-2028`
("routing through its own signal would add a queued hop on the render path for
no gain").

**The remote spectrum path therefore attaches to
`FFTEngine::fftReadyLinear(int, const QVector<float>& binsLinear, double
windowEnb, double dbmOffset)` (`FFTEngine.h:221`) and resolves fan-out through
`FFTRouter::pansForReceiver(streamIndex)`**, mirroring the live dispatcher.

**`SpectrumEndpoint` consumes linear power bins plus `windowEnb` and
`dbmOffset`, not dBm**, and performs its own dB conversion via the detector and
avenger stage. `FFTRouter`'s `binsDbm` parameter name is misleading and should
be renamed `binsLinear` in R3.

**The shared-FFT cost, which 3F makes live rather than hypothetical:** pans
sharing a stream share its FFT size. If one pan is zoomed deep, forcing a large
FFT, the long time window smears the waterfall on another pan watching wide off
the same stream. An earlier revision claimed this was nil at single-operator
scope on the assumption that one session meant one viewport. That is wrong: a
single operator can produce the collision with no guest session anywhere. The
mitigation is **two shared FFTs per stream**, one wide-and-fast and one
narrow-and-fine spun up past a zoom threshold, and it is §17 item 4, blocking
R3 rather than deferred indefinitely.

**Physics**, at 192 kHz and 1024 pixels (arithmetic verified in every row):

| Zoomed to | FFT size needed | Hz/bin | Window spans |
| --- | --- | --- | --- |
| 192 kHz (full) | 1,024 | 188 | 5 ms |
| 48 kHz | 4,096 | 47 | 21 ms |
| 12 kHz | 16,384 | 12 | 85 ms |
| 3 kHz (SSB) | 65,536 | 2.9 | 341 ms |
| 500 Hz (CW) | 524,288 | 0.37 | 2.7 s |

**Below a threshold the endpoint interpolates rather than reduces.** With
`kMaxFftSize` 262144, the crossover for a 1024-pixel request is a **750 Hz
span at 192 kHz and 1500 Hz at 384 kHz**, which covers ordinary CW and
narrow-digital zoom on any 384 kHz DDC. At the 500 Hz row clamped to
`kMaxFftSize`, only 683 bins exist against 1024 pixels, so a third of that
endpoint's bandwidth is server-side interpolation the client could produce
itself. The branch is real: `SpectrumDetector.cpp` gates the reduce path on
`pixPerBin <= 1.0` and interpolates in the else branch.

**Rule:** `SpectrumEndpoint` clamps the requested pixel count to the available
bin count and **reports the clamp**, so the client stretches locally.

Frame rate is decoupled from FFT size by overlap (`FFTEngine.cpp:621`).

### 9.2 Display codec

**NereusSDR-original code.** There is no upstream source to port (§2.4), so
there is no attribution question and equally no source to check against. The
parameter names in §2.6 come from a screenshot and inform parameter choice
only.

**Each pan endpoint produces two reduced planes, not one.** Trace and waterfall
have independent detector and averaging settings (`SpectrumWidget.h:371-388`
declares `spectrumDetector()`, `spectrumAveraging()`, `waterfallDetector()`,
`waterfallAveraging()` as four independent values; the second reduction pass is
at `SpectrumWidget.cpp:2758`). The waterfall row therefore **cannot** be
derived client-side from the trace array without losing its independent
detector and averaging. Both planes are transmitted unless dropping one is made
an explicit, recorded divergence from local rendering.

Design:

1. Quantize `float` dBm to 8 bits across a configurable `[minDbm, maxDbm]`
   window. At -135 to -30 that is 105 dB over 256 levels, 0.41 dB per step.
2. Temporal delta against the previous frame.
3. Block-wise coding with adaptive block size.
4. Periodic keyframe, plus a keyframe on demand (see §9.4's desync rule).
5. Decouple waterfall advance from trace rate ("frames per line"); reuse
   `WaterfallTicker` for the cadence.
6. **A rate-control lever.** §2.6 records "Accuracy: 1 Normal" from the
   screenshot but the design previously dropped it. A dead-zone or
   coarser-delta quantizer on the delta domain is the mechanism that makes a
   flat budget holdable across configurations. §14's bounded-dB-error assertion
   is its acceptance test.

**State the budget at the worst case, not the best.** Bits-per-pixel cost is a
function of **FFT overlap**, not of the codec: frame-to-frame delta entropy
depends on how much input two consecutive frames share. §2.5's measurement was
taken at 80.5% overlap, while `FFTEngine`'s default `fftSizeBaseline` is 4096
and auto-zoom sets `desired = baseline * (sampleRate/bwHz)`, which equals
baseline at full span, so **our default wide view runs at 0% overlap**, the
most expensive point in the space. §9.1's table names FFT 1024 for the
full-span row, worse still.

The codec must therefore be measured at FFT 4096 full-span, both planes, and
the budget stated there.

### 9.3 Audio codec

Opus, carried as RTP per RFC 7587 (§7.2), which also yields RTCP receiver
reports for the link-quality figures.

**The §2.6 parameter triple cannot all take effect.** With `signal=music` the
encoder uses the music-mode threshold and at 24 kbit/s selects CELT-only, and
CELT does not support MEDIUMBAND, so libopus promotes it to WIDEBAND. The
"mediumband" setting is inert in exactly this configuration and the encoder is
already delivering 8 kHz audio bandwidth.

**Consequence:** any plan to track the receive filter width and mode must be
expressed in bandwidths libopus honours in CELT mode (NARROWBAND, WIDEBAND,
SUPERWIDEBAND, FULLBAND), and mode changes are a mid-stream reconfiguration
event (§8.4).

*This analysis was not independently verified against the vendored
`opus_encoder.c` in this checkout; confirm before implementing.*

A lossless or high-rate PCM mode is required for digital-mode work and for
microphone uplink on capable links (§3.1).

### 9.4 Endpoint model, identity, and staleness

**Endpoints are keyed by a session-scoped endpoint id minted by the client at
subscribe time.** The daemon's subscription is `(endpointId → streamIndex)`,
and teardown is an **explicit unsubscribe**, never an inference from
visibility or layout.

**A pan is a client concept. An endpoint is the wire concept. They are not
the same object and must not be conflated.** An earlier revision said pan ids
"stay client-local", which is wrong in two ways: `SliceModel::panKey` is a
`Q_PROPERTY`, so a property-enumerating mirror ships pan ids by construction,
and `RadioModel::connectToRadio` itself hardcodes `setPanKey("pan-0")`
daemon-side. Pan ids therefore cross the wire whether we design for it or not.

The resolution, which also settles what R1's "pan orchestration" means:

**The daemon does not own pan objects.** There is no core-side pan model to
own: `RadioModel::addPanadapter()` has no production caller, every consumer
guards on `isEmpty()`, and `removePanadapter(int)` is positional. Rather than
build one for the daemon's benefit, the daemon owns **streams and endpoints**,
which is what it actually needs, and treats `panKey` as an opaque
client-assigned label it stores and echoes back so the client can correlate.
§15 R1's deliverable is therefore **slice and endpoint orchestration**, not
pan orchestration.

**`PanadapterModel`'s four values are client-owned, and the mirror must not
fight the endpoint config over them.** `centerFrequency`, `bandwidth`,
`dBmFloor` and `dBmCeiling` are exactly §9.4a's crop window and quantisation
window. An earlier revision made them simultaneously daemon-mirrored and
client-requested, with no precedence, so a band change firing the per-band grid
restore would push new floor and ceiling values through `StateMirror` while the
endpoint config asserted its own, and the display range would oscillate.

`PanadapterModel` is therefore **excluded from the mirrored set** and lives
client-side only. The per-band grid restore that drives it becomes a
client-side reaction to the mirrored `bandChanged`, not a daemon push. This
also removes the "x pan count" row from §6.1's instance table.

**Per-endpoint configuration, client-requested:** pixel count, frame rate,
frames-per-line, crop window, detector mode, averaging mode and tau, and the
min/max dBm quantisation window. See §9.4a for what is **not** client-owned.

**Latest-value-wins at the producer, never a queue.** The producer keeps the
most recent frame double-buffered with a monotonic frame index. An index that
jumped means frames were skipped, and for a display that is correct behaviour.
**The counter must be wrap-aware**: `DataIndexWrap` is 1000000000 and wraps to
**1**, not 0.

**But the wire needs a separate sequence, and this is a correctness issue.**
The producer rule applies **before** encoding. On the wire, a delta chain
requires the exact previous frame, and §10.4 puts spectrum on an unreliable
channel where drops are routine. Applying "gaps are correct behaviour" to the
decoded wire stream would disarm the only signal that detects a broken chain,
corrupting trace and waterfall until the next keyframe.

**Rule:** carry an explicit **encoder frame sequence number** distinct from the
producer frame index. A gap in the encoder sequence is loss and **must** trigger
a keyframe request on the reliable control channel. State the maximum tolerated
corruption window, and either shorten the keyframe interval or reference deltas
against the last acknowledged frame.

**Pixel count is a bandwidth control, decoupled from widget width.** The client
requests a count and stretches. Note the shipping reducer currently does the
opposite (`SpectrumWidget::updateSpectrumLinear` derives it from
`width() - effectiveStripW()`), which is why §4.2 item 6 is a refactor.

**Zoom and pan are applied server-side before reduction, with a transmit
margin.** A strict crop would destroy the instant local zoom that ships today:
`SpectrumWidget::wheelEvent` assigns `m_bandwidthHz` and repaints immediately
out of the full bin set it already holds, emitting `bandwidthChangeRequested`
asynchronously, and CLAUDE.md states that contract explicitly. The endpoint
therefore transmits a configurable margin around the visible span (1.5x
default) so the client crops locally during a gesture, and the daemon
re-centres on gesture end. Zoom and pan join §3.1's locally-instant list, and
§14 gains a gesture-latency criterion.

**Frame rate is a target, not a guarantee.** The client measures and displays
the delivered rate.

### 9.4a Parameter ownership

Three sections previously assigned FFT size to three different owners. One
table, referenced by §6.3, §9.1 and §9.4:

| Parameter | Owner | Notes |
| --- | --- | --- |
| FFT size, window function | Per **stream**, daemon-owned | Shared by every pan on the stream; a client sends a **resolution target** (Hz/bin) which the daemon may satisfy or refuse |
| Stream sample rate | Radio, station-wide | `setSampleRateLive`; never per viewport (§8.4) |
| Pixel count, frame rate, frames-per-line | Per **endpoint**, client-requested | §9.5 bandwidth levers |
| Crop window (zoom, pan) | Per **endpoint**, client-requested | Plus transmit margin |
| Detector mode, averaging mode and tau | Per **endpoint, per plane** | Trace and waterfall independently (§9.2) |
| Min/max dBm quantisation window | Per endpoint, client-requested | Display-codec input |

Note that per-stream FFT configuration is itself new work: these are four
**global** AppSettings keys today, applied identically to every engine
(`MainWindow.cpp:1430`).

### 9.5 Multi-pan budget

**§2.5's measured targets are per panadapter and RX-only.** Scaling them
requires three different quantities that an earlier revision conflated:

| Quantity | Meaning | Cap |
| --- | --- | --- |
| `userDdcCount` | Concurrent streams, therefore concurrent FFTs and the ceiling on distinct spectrum sources | Per SKU |
| `maxSlices` | Audio slices | Per SKU |
| Docked pan count | Layout templates | **4** (`"1"`, `"2v"`, `"2h"`, `"12h"`, `"2x2"` in `PanadapterStack.cpp`), plus floating windows |

There is **no five-pan template**; five pans requires floating windows. And
slices bind to streams many-to-one (`SliceStreamAllocator.h:29`), so five
slices commonly share fewer streams, fewer FFTs, and fewer pans.

Per-SKU values must be read from `BoardCapabilities.cpp` rather than
reproduced here, because an earlier revision's table contained two errors:
**SaturnMKII was missing** (`BoardCapabilities.cpp:1010` sets `maxSlices = 5`)
and **"RedPitaya P1 = 4" has no supporting capability row** (the only RedPitaya
rows map into the OrionMKII block at `maxSlices = 5`, `:565`, `:583-584`).
Note also that `maxSlices` and `userDdcCount` differ on at least one SKU:
HermesC10/G2E is `maxSlices = 5` (`:669`) with `userDdcCount = 4` (`:684`).

**A session-wide budget is required, not per-pan defaults.** The client
receives a total display budget and allocates across its endpoints. The levers
are per-endpoint (§9.4): a four-pan layout might run the focused pan at 1024
by 30 and the rest at 256 by 10.

**Endpoints not being viewed must be disabled, not reduced**, via an explicit
enable/disable message. **Do not implement this as a widget-visibility test:**
layout changes and float/dock cycles hide and show every pan, and a naive
`isVisible()` gate would flap.

### 9.6 Wideband extended pan

Phase 3F Sub-Epic F adds a structurally different second spectrum source:
`WidebandFftEngine` is a different transform (16384-point real-to-complex
against complex I/Q), a different bin count (8192), a different span (0 to
adcRate/2), and a different bin width, with a deliberate refresh-rate mismatch
against the DDC path.

**Decision: out of scope for v1, and explicitly so.** Separate data path
needing its own endpoint and codec tuning; its visual rendering is itself
deferred upstream to Sub-Epic F polish (T7 through T10, per
`SpectrumWidget.h:994-998`); and it is a zoomed-way-out overview, the least
latency-sensitive view.

**Two things v1 must still do.** Not preclude it: wideband becomes an
additional stream identity with its own endpoint configuration, which §9.4
accommodates. And **give it a thread**: `RadioModel.cpp:7477-7481` hops the
wideband FFT to the **main thread** to stay out of the network hot path, and
`nereusd` has no main thread in that sense, so R1 must provide one even though
streaming is out of scope.

A remote client zoomed past the DDC span in v1 shows the DDC span and stops,
rather than silently misrendering.

---

## 10. Transport

### 10.1 Governing principle

> **No component of the normal operating path may depend on infrastructure the
> operator does not control. Infrastructure we run may make things easier; it
> may never be what makes them possible.**

A resilience requirement, not a cost one. If the standard path runs through
hosted infrastructure, an outage takes every station offline at once. It also
fixes incentives: if relay were the default, direct connectivity would be an
optimisation nobody exercises and its edge cases would rot.

This rules out any dependency on a third-party VPN client for the network layer
to function, a required license or activation check, and any mandatory cloud
settings sync.

### 10.2 Direct first, relay last

| Tier | Path | Works when |
| --- | --- | --- |
| 1 | IPv6 host to host | Both ends have IPv6. No traversal machinery beyond the pinhole the outbound probe creates |
| 2 | IPv4 host to host | Same LAN |
| 3 | IPv4 server-reflexive, hole-punched | Both NATs use endpoint-independent mapping |
| 4 | Relay | Everything else |

This is ICE candidate ordering, and **IPv6 preference is not special-cased**:
it falls out of IPv6 being a host candidate with high local preference, and
host candidates outranking anything requiring NAT.

**Structural luck:** the carriers that impose CGNAT (T-Mobile, Starlink) do so
because they exhausted IPv4, which is the same reason they hand out IPv6.

Tier 3 fails only against address-and-port-dependent (symmetric) mapping. Most
carrier NAT is endpoint-independent because symmetric behaviour breaks games
and VoIP.

### 10.3 The rendezvous

Three distinct jobs; conflating them caused confusion during design:

| Job | Traffic | Public option exists? |
| --- | --- | --- |
| STUN reflection | ~2 packets per session | Yes, but we self-host |
| Signaling / introduction | ~2 KB per session | **No, and never will** |
| TURN relay | See sizing below | No |

Signaling is application-specific by nature: it must know which daemon belongs
to which operator and authenticate both ends.

STUN cannot be skipped as a function, because the address the signaling server
observes is the TCP mapping while media flows over a different UDP socket with
a different NAT mapping. But it need not be separate: `nereus-rendezvous`
answers STUN on a UDP port. One binary, one host, three roles.

**Relay sizing, keyed to session shape rather than one constant.** An earlier
revision used 144 kbit/s, which is §2.5's single-pan, RX-only, download-only
figure. Relay is billed **per direction**:

| Session shape | Down | Up |
| --- | --- | --- |
| Single pan, RX only | ~145 kbit/s | negligible |
| Four pans | ~440 kbit/s | negligible |
| Plus microphone (R4 onward) | as above | ~40 kbit/s |

**An outage stops new sessions from starting. It does not drop established
ones.** Once ICE selects a pair, media flows peer to peer and the rendezvous
holds only idle control connections. Preserve this deliberately.

Three further measures reduce the dependency: **cached peer state** (try a
station's last known candidates before contacting the rendezvous), **direct
address entry as a first-class option**, and **multiple rendezvous endpoints**
in an ordered client-side list.

### 10.4 Transport implementation

An ICE stack with DTLS and data channels. `libdatachannel` is the leading
candidate: ICE via libjuice, DTLS, SCTP data channels, the same stack browsers
use, and it works with any standard STUN and TURN server.

**Unreliable data channels are a material benefit beyond traversal**, provided
audio is not coalesced into them (§7.2). Control on a reliable ordered channel,
spectrum on an unreliable one, audio on its own RTP stream.

**Open item, gated before the dependency lands:** `libdatachannel` is MPL-2.0
and libjuice is LGPL. Both are very probably compatible with GPLv2-or-later,
but this project's compliance bar requires verification, not probability. If it
does not clear, the fallback is native: STUN binding requests and hole punching
are modest, and Qt supplies `QDtls` and `QDtlsClientVerifier`. The cost is a
hand-rolled candidate and check implementation with a likely lower traversal
success rate.

### 10.5 Encryption, and which phase owns it

**TLS lands in R2, not R5.** An earlier revision made TLS mandatory but
attached it to components that do not exist until R5, while R2 through R4 run
on a plain transport explicitly sanctioned "over the internet". Followed
literally that ships internet-reachable unencrypted TX keying for three phases.

**The supporting claim was also overstated.** `git grep -n
'NonSecureMode\|SecureMode\|QSslConfiguration\|setSslConfiguration' -- src`
returns exactly one line: `src/core/TciServer.cpp:1267`, `NonSecureMode`. There
is no `QSslConfiguration`, no certificate handling, and no `wss` code path
anywhere in the tree. "Close to free" omitted the actual work.

**Certificate model.** A headless Pi has no domain name, so the daemon
generates a self-signed certificate on first run and the client pins its
fingerprint, displayed at pairing time alongside the token (§7.1).

If R2 must ship without TLS, its stated reachability is restricted to loopback
and LAN only, and R4 must not land on it.

### 10.6 IPv6 correctness

- **Bind dual-stack.** `QHostAddress::Any` is dual-stack in Qt.
  `QHostAddress::LocalHost` is IPv4 `127.0.0.1` only, the current default.
- **Accept bracketed literals** (`wss://[2001:db8::1]:50001`) everywhere:
  connection dialog, settings, logs, reconnect. This is where IPv6 support most
  commonly breaks silently.
- **Happy Eyeballs (RFC 8305) is staggered and IPv6-preferring, not parallel.**
  Start the IPv6 attempt first and stagger IPv4 behind it by the Connection
  Attempt Delay (250 ms default); wait up to the Resolution Delay (roughly
  50 ms) for a pending AAAA before falling back. Implementing it as "try both
  at once" loses the IPv6-first bias §10.2 relies on.
- **Client-finds-daemon discovery must be IPv6-capable** from the start.
  (`RadioDiscovery`'s IPv4 broadcast is radio-side and unaffected.)

### 10.7 Reachability diagnostics

The daemon reports its own situation. **This requires STUN, not just a local
interface check.** A purely local check reports "RFC 1918 behind a router where
forwarding would work" for the standard carrier-NAT topology (customer router
hands out RFC 1918 on the LAN, carrier NAT upstream), sending the operator to
configure a port forward that cannot function. That is precisely the population
§14 names as the target case.

Three outcomes, distinguished by comparing the local address against the
server-reflexive address:

| Local | Reflexive | Verdict |
| --- | --- | --- |
| RFC 1918 | equals the router's WAN, publicly routable | Port forwarding will work |
| RFC 1918 | in `100.64.0.0/10` (RFC 6598) or otherwise not routable | Carrier NAT upstream; forwarding cannot work |
| Global IPv6 present | n/a | Direct connection available |

`src/core/RouteProbe` already exists and should be reused.

---

## 11. Relationship to upstream Thetis

We build natively now and treat alignment as a later transport implementation.
§2.4 establishes by verified absence that the wire format we would otherwise
port from does not exist publicly and has no committed date.

**Not a schedule risk.** The only public movement since 2026-03-15 is inside
the disabled reducer class and its test harness (§2.4). No wire format, no
codec, no transport has appeared.

**Attribution decision, which must be made once and stated, because the
pre-commit chain and CI both enforce it** (`scripts/verify-thetis-headers.py`,
`scripts/check-new-ports.py`):

- **The endpoint model is treated as design shape, not ported code.** §9.4's
  requirements are specified in NereusSDR terms (endpoint ids, wire sequence
  numbers, clamp reporting) and do not reproduce `clsSpectrumProcessor`'s
  structure. The upstream identifiers named in this document (`dataIndex`,
  `SetReceiverEnabled`, `DataIndexWrap`) are cited as **evidence for a design
  argument**, not as an API to reproduce. No new PROVENANCE row is required.
- **`SpectrumDetector` and `SpectrumAvenger` are existing WDSP ports** whose
  headers travel verbatim on relocation, with PROVENANCE paths updated
  (§4.2 item 3).
- **The display codec, session layer, state mirror, and transport are
  NereusSDR-original.** There is no upstream source for the codec in any form.

This decision is a §15 R3 deliverable so the implementer is not resolving a
compliance question mid-task.

Our existing TCI server is unaffected and continues to serve third-party
applications as it does today.

---

## 12. Safety

### 12.1 TX watchdog (hard requirement)

**If the link drops while transmitting, the daemon keys down immediately.**

Not solely an engineering concern: remote operation requires the control
operator to be able to shut down the transmitter, and a stuck carrier because a
hotel Wi-Fi dropped is precisely the scenario the rules exist for.

Liveness source changes as the transport ladder is built:

- **R2 through R4:** an application-level heartbeat on the control channel,
  with a deadline in the low hundreds of milliseconds while MOX is asserted.
- **R5 onward:** ICE consent freshness (RFC 7675) supplements it. Consent
  freshness declares a dead path in roughly 30 seconds, which is **far too slow
  to replace** the keyed-state heartbeat; it does not supersede it.

The daemon drops MOX through `MoxController` on whichever signal fires first,
the same path `TxSliceArbiter` uses.

**Reconnect timers must be cancellable** (§13).

Requires a test, and the test is not optional.

ON7OFF's product implements the same protection independently, which is
reasonable evidence the requirement is correct.

### 12.2 Further TX protections

- **PTT timeout.** Configurable maximum continuous transmission.
- **TX disabled until the session is fully established**, meaning specifically
  until §7.0's snapshot-complete marker has been received, so a half-connected
  client cannot key an unknown frequency.
- **Existing protections stay daemon-side and authoritative**:
  `TxInterlockPolicy`, PA over-drive safety, SWR gating, out-of-band checks. A
  remote client cannot override them.
- **Remote TX handoff needs an unkey-confirmed gate** (§7.1), because
  `TxSliceArbiter` does not provide one.

### 12.3 Uplink starvation while keyed

§12.1 covers link **loss**. It does not cover a live link whose microphone
uplink starves (congestion, Wi-Fi stall, client CPU spike) while transmitting.
§13's underrun handling is a **playback** remedy and does not apply.

Required:

- **A mic-starvation deadline shorter than the §12.1 link-loss deadline**, and
  a TX jitter buffer depth strictly less than it (§8.3), with all three numbers
  stated together.
- **A per-mode action**, because the right answer differs: silence in SSB is
  benign, while in AM, FM and RADE it is an unmodulated or garbage carrier on
  the air.
- A §14 bench row for starvation while keyed.

---

## 13. Error handling and reconnect

**Link loss.** The client retains last-known state, indicates staleness rather
than showing stale values as live, and reconnects with exponential backoff.

**The reconnect timer must be cancellable, which the obvious in-tree reference
is not.** `PgxlConnection` and `TgxlConnection` arm retry with the **static**
`QTimer::singleShot`, so a pending reconnect cannot be cancelled; both declare
an `m_reconnectTimer` member referenced zero times in either `.cpp`. The
correct pattern is an owned single-shot `QTimer` plus an `onReconnectTimeout`
slot with latched host and port. Cancellability matters more here than there:
ICE restart and operator-initiated disconnect both need it, and this subsystem
gates a transmitter.

**Reconnect.** Attempt cached peer candidates first (§10.3), then the
rendezvous. Request a display-codec keyframe on resume. Re-sync via §6.1's
snapshot with its completion marker, rather than assuming continuity.

**ICE restart.** A network change triggers gathering and checking again over
existing rendezvous connections without operator action.

**Receive underrun.** Jitter buffer refills, drift resampler corrects.
Underruns counted and surfaced, matching the "Audio err" and "Display err"
counters upstream exposes.

**Transmit starvation.** See §12.3. This is a distinct condition with a
distinct remedy.

**Daemon-side client loss.** Drop MOX immediately (§12.1), stop encoding,
retain radio state and connection. The radio does not disconnect because the
operator's laptop closed.

**No audio device on the daemon.** Not an error (§4.3).

---

## 14. Testing

**Unit.** Display codec round-trip with a bounded dB error assertion, measured
at FFT 4096 full-span (§9.2) and **under simulated packet loss**, asserting
recovery within N frames. Opus round-trip. Frame mux and demux. Coalescing
under queue pressure. Reachability classification including the carrier-NAT
case (§10.7).

**State mirror**, three separate cases rather than one, because every
collapsed-to-one defect in this design only appears above N=1:

1. Property reflection across the full `Q_PROPERTY` set.
2. **Slice lifecycle**: add, remove-from-middle, re-add with **id reuse**, and
   TX handoff, asserting id-not-position resolution throughout.
3. **Pan and endpoint lifecycle**: layout change, float and dock, reconnect,
   asserting no orphaned endpoints and no leaked subscriptions.

**Integration.** Daemon and client in one process over an in-memory transport,
including a case at `maxSlices` with a 2x2 layout.

**Loopback.** Separate processes on one machine with a real radio attached.
**This exercises the entire remote path without any Pi hardware and should be
the main development loop.**

**Network.** Daemon on a Pi, client on a laptop: LAN, then internet, then the
target case of **daemon behind one carrier CGNAT and client behind a different
carrier CGNAT**.

**Bench matrix** at `docs/architecture/2026-07-28-remote-daemon-verification/`
covering each transport tier, the TX watchdog, **uplink starvation while
keyed**, PTT timeout, reconnect, ICE restart, gesture latency for zoom and pan
(§9.4), bandwidth against §9.5's per-shape budgets, and audio drift over a long
session.

---

## 15. Phasing

Each phase is independently useful and independently verifiable, and each gets
its own implementation plan document. This design is the umbrella.

**R1: build split and daemon skeleton.** The three-way CMake split, **all nine
prerequisites in §4.2** (including the FFT pool owner, the topology builder and
the crop-and-reduce extraction, which are build-and-target surgery and must not
be deferred to R3), `nereusd` with config file and systemd unit, a thread for
the wideband FFT (§9.6), and the daemon's slice and **endpoint**
orchestration (not pan orchestration; §9.4 places pans client-side).

**R1 must resolve slice creation.** Every `addSliceOnPan` call site is
GUI-resident (five in `MainWindow.cpp`; the definition is `Q_INVOKABLE` on
`RadioModel`) and `connectToRadio` uses the no-argument `addSlice` overload, so
a headless daemon otherwise gets Slice A and nothing else. R1 either takes its
slice and pan set from config or states explicitly that it is single-slice.

*Verified by:* headless daemon on a Pi receiving from an ANAN, with N slices.

**R2: state mirror, settings, transport, and TLS.** `StateMirror` with object
identity and lifecycle (§6.1), the record-stream mechanism (§6.1a),
`SettingsProxy` with allowlist and snapshot (§6.2), §7.0's handshake and
capability exchange, token distribution, **TLS**, and a plain WebSocket
transport with no ICE and no codecs. GUI remote mode against `localhost`.

*Verified by:* full operator control from a GUI on the same host, over `wss`.

**R3: codecs and spectrum endpoints.** `SpectrumEndpoint` per subscription,
`DisplayCodec` with keyframe-on-loss, `OpusAudioCodec` on RTP, coalescing,
jitter buffer, drift resampler, the §9.1 two-FFT decision, and the §11
attribution decision.

*Verified by:* §9.5 budgets met over a LAN at four pans, audio stable over
hours.

**R4: transmit and its safety harness.** Mic uplink, daemon-side TX chain
wiring, the unkey-confirmed handoff gate (§7.1), TX watchdog, uplink-starvation
handling (§12.3), PTT timeout, TX-disabled-until-snapshot-complete. These ship
together. **Requires #291 merged** (§2.8).

*Verified by:* on-air TX from a remote client, plus a watchdog test that
physically severs the link mid-transmission, plus a starvation test.

**R5: transport ladder.** `nereus-rendezvous` (STUN, signaling, TURN),
candidate gathering and checks, hole punching, relay. Cached peer state, direct
address entry, multiple rendezvous endpoints, §10.6 IPv6 work.

*Verified by:* the CGNAT-to-CGNAT case, and an established session surviving a
rendezvous outage.

**R6: operations.** Reachability diagnostics (§10.7), connection UX, packaging,
documentation.

---

## 16. Deferred and open decisions

**Per-slice audio encoding: an open decision, not a deferral. Must be decided
before R3.** Does the daemon encode the `MasterMixer` master as one Opus stream,
in which case every per-slice gain, mute and pan control stays daemon-side and
pre-mix (§3.1)? Or N per-slice streams, letting the client mix, at N times the
audio budget in §9.5 and §10.3? An earlier revision recorded this as "deferred
upstream", which was factually wrong (§2.8).

**Saturn on-board daemon.** The ANAN-G2 and G2E Saturn board pairs the FPGA
with a Raspberry Pi CM4 over PCIe, so the daemon could run inside the radio.
`RadioConnection` is an abstract base with pure virtuals, so this is a third
subclass and nothing above it changes. Deferred because the CM4 is a
Cortex-A72 at 1.5 GHz against a Pi 5's Cortex-A76 at 2.4 GHz and the budget
needs measuring; G8NJJ's `saturn` repository is a new upstream requiring its
own provenance table; and it means installing our software onto a radio that
shipped with working firmware.

**Guest sessions.** Roles and identity exist from day one (§7.1); only owner is
implemented.

**Wideband over the link** (§9.6).

**Collapsing local mode onto the daemon** (§5).

**Web client.** §10.4's transport choice keeps it viable; nothing here assumes
it.

---

## 17. Open items

1. **`libdatachannel` licensing and current state** (§10.4). Verify before the
   dependency is accepted. Fallback documented.
2. **CPU headroom on target hardware.** **The hardware floor is a Raspberry
   Pi 4 Model B Rev 1.5 (8 GB), quad Cortex-A72. A Pi 5 (quad Cortex-A76) is
   the preferred target, not the minimum.** That is roughly a 2.5x to 3x
   spread between floor and target, so a configuration that is comfortable on
   a Pi 5 may not run at all on the floor, and the spike must be run on the
   Pi 4 first. See §4.5 for what the floor implies for the design.
   Scope the spike as **5 streams at the
   SKU's top rate with FFT at `kMaxFftSize` on the single shared `m_fftThread`**
   (`MainWindow.h:597-606` flags thread saturation as unresolved: "splitting to
   one thread per engine is a follow-up needing maintainer sign-off"), plus
   WDSP RX per slice, plus Opus per slice, plus display codec per endpoint,
   plus the wideband FFT which runs on the main thread. It must answer both
   thread-architecture questions. **No CPU measurement exists anywhere in the
   repository, on any branch, for any platform**, so the design currently has
   no evidence either way. RADE and DeepFilterNet3 are additional risks, not
   the only ones. Reuse `PerfMonitor`, `MemoryPressure` and
   `RealtimeAudioPriority` as instrumentation. Schedule in R1.
3. **Display setup page split** (§6.3). A design pass, not a mechanical edit.
   The Display pages drive a single renderer and a single FFT engine through
   two non-owning hooks wired once at `MainWindow.cpp:3103-3104` and never
   re-pointed, so roughly 80 setup call sites act on pan-0 and stream-0 only.
   Splitting for remote operation must also answer "which pan does this control
   act on", a multi-pan question. **The page already misbehaves under
   multi-pan, so the remote split cannot preserve existing behaviour verbatim.**
4. **Two shared FFTs per stream, or one** (§9.1). Blocking R3.
5. **Session-wide display budget allocation** (§9.5): client-allocated,
   daemon-capped, or both. Needs a real four-pan layout on a real link.
6. **Whether audio rides WebRTC media tracks or data channels** (§7.2, §10.4).
   Media tracks make RTP native and, with a full WebRTC stack, would supply the
   jitter buffer and packet-loss concealment §8.3 otherwise requires.
   `libdatachannel` carries media as RTP passthrough **without** that audio
   pipeline, so it does not remove that work; full `libwebrtc` would, at a much
   heavier dependency cost. Decide alongside item 1.
7. **Verify the §9.3 Opus mode analysis** against the vendored
   `opus_encoder.c` in this checkout before implementing.

---

## 18. Decisions, for the record

| Decision | Choice |
| --- | --- |
| Baseline | Phase 3F (PR #293) as a **three-way merge-order precondition**: #291 into #293, #293 rebased onto `main`, then R1. Line cites re-derived after the rebase |
| Build target kind | Two **shared** libraries. A link-time split, not the subsystem split `main`'s phase-1 doc rejects |
| Pan ownership | Daemon owns streams and endpoints; pans are client concepts. `PanadapterModel` is excluded from the mirror |
| Property directions | Three classes: bidirectional, read-only telemetry needing a shadow-apply path, and derived properties never applied inbound |
| Version mismatch | Refuse on major, negotiate down on minor |
| Envelope reliability | Two coalesced envelopes: reliable for control and state, unreliable for spectrum and meters |
| DSP split point | Option C: daemon demodulates; client is control surface and renderer |
| Upstream alignment | Build natively now; TCI WS/WSS becomes a later transport |
| Spectrum source | `FFTEngine::fftReadyLinear` (linear power); `FFTRouter` is a topology oracle carrying no frames |
| Spectrum architecture | One FFT per **stream**, per-endpoint crop and reduce, with transmit margin |
| Spectrum delivery | Latest-value-wins at the producer; **separate wrap-aware wire sequence** with keyframe-on-loss |
| Pixel count | Bandwidth control, decoupled from widget width, clamped to available bins with the clamp reported |
| TX arbitration | Route through `RadioModel::requestTxHandoffToSlice` (id-based), plus a new unkey-confirmed gate for remote |
| Session model | Single operator; a second authenticated connection preempts |
| State sync | Three mechanisms: property mirror, settings proxy, record streams |
| Object identity | Persistent ids, never list positions; ids are reused |
| Architectural cut | Models mirrored on both sides (option D rejects relocating them, not mirroring them) |
| Daemon requirement | Optional; local direct path untouched in v1; distinct settings store |
| Packaging | One installer, two binaries, daemon off by default |
| Settings boundary | By key prefix, three scopes, and the store is not partitionable by prefix alone |
| Audio framing | RTP with Opus per RFC 7587, never coalesced; RTCP for link statistics |
| Spectrum framing | Compact native, VITA-49 semantics, standard not adopted |
| Multi-pan | First-class from v1, session-wide bandwidth budget |
| Wideband pan | Out of scope for v1, not precluded, but needs a thread in R1 |
| Transport default | Direct first, relay as fallback |
| TLS | Lands in R2, not R5 |
| Hosted infrastructure | May make things easier, never possible |
| Third-party VPN dependency | Rejected |
| TX watchdog | Hard requirement, ships with TX; uplink starvation is a separate condition |

# Tunable Notch Filter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task by task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship an operator-placed tunable notch filter: click a carrier on the panadapter, a narrow slot is cut out of the received audio, and it stays cut across retunes, band changes, reconnects and restarts.

**Architecture:** A single global `NotchModel` on `RadioModel` holds the canonical notch list in absolute RF Hz. `RadioModel` mirrors it into every live `RxChannel`'s WDSP notch database, each channel also receiving its hosting DDC stream's centre frequency so WDSP can map absolute notches into that channel's baseband. `SpectrumWidget` renders markers and raises interaction signals per pan; the model is shared, the pixel mapping is per-pan.

**Tech Stack:** C++20, Qt6 (Widgets, Test), WDSP (vendored TAPR v1.29, `nbp.c` NBP notch API), AppSettings XML persistence, CMake + Ninja, Qt Test.

**Design authority:** [`docs/architecture/2026-07-28-tunable-notch-filter-design.md`](2026-07-28-tunable-notch-filter-design.md). Adversarial review that shaped it: [`2026-07-28-tnf-spec-adversarial-review.md`](2026-07-28-tnf-spec-adversarial-review.md). Where this plan and the spec disagree, the spec wins; report the conflict rather than silently choosing.

---

## Global Constraints

Every task's requirements implicitly include this section.

**Upstream stamps.** Thetis `v2.10.3.15` (`3759d096`) at `/Users/j.j.boyd/Thetis`. mi0bot-Thetis `@c26a8a4`. AetherSDR `@c6481cbf` at `/Users/j.j.boyd/AetherSDR`. WDSP vendored at `third_party/wdsp/src/`. Cite ported logic inline as `// From Thetis <file>:<line> [v2.10.3.15]` or `// From AetherSDR <file>:<line> [@c6481cbf]`.

**Source first.** Never invent a signature, constant, line number or API. Open the upstream file, read it, port it. If you cannot find the source, stop and ask.

**Attribution is ship-blocking.** `NotchModel.{h,cpp}` is a new attribution event and needs the AetherSDR block plus **both** Thetis headers (`radio.cs` and `console.cs`) byte for byte, `// --- From <file> ---` separated, plus PROVENANCE rows, in the commit that introduces it. See design §10.1. Author tags inside ported ranges must survive verbatim, including `//NOTCH MW0LGE` (`console.cs:33283`); see design §10.3. `SpectrumOverlayPanel.{h,cpp}` must receive **no** Thetis cite (design §10.2).

**Style.** Classes `PascalCase`, methods and variables `camelCase`, constants `kPascalCase`, members `m_camelCase`. Braces on all control flow. No raw `new`/`delete`. No `goto`. No `QSettings`, use `AppSettings`. Log with `qCWarning`, never throw. Platform guards use `Q_OS_*`.

**Commits.** GPG-signed. No `Co-Authored-By: Claude` trailer. No em-dash characters in commit messages, comments, or documentation.

**Build and test.**

```bash
cmake --build build --target tst_notch_model_guards -j$(sysctl -n hw.ncpu) && ctest --test-dir build -R tst_notch_model_guards --output-on-failure
```

Build only the target you need. A full suite build costs roughly 32 minutes; see `docs/development/fast-test-loop.md`.

---

## Normative cross-task corrections

These came out of a reconcile pass over the ten independently authored tasks. **They override the task bodies below.** Apply each when you reach the task named.

### Hard errors

1. **`tst_notch_hit_test` must be registered exactly once.** Tasks 6 and 7 both register it. Two `add_executable()` calls with one target name is a CMake configure failure. **Task 6 creates and registers the file; Task 7 appends test slots only and registers nothing.**
2. **Persistence is never triggered as written.** Task 3 produces `restoreFromSettings()` and `saveToSettings()`; no task calls either. **Task 4 constructs `NotchModel` and calls `restoreFromSettings()` in the `RadioModel` constructor. Task 3 makes every mutator save-on-mutate** (matching how `SliceModel` persists), and says so in its Produces.
3. **`RxChannel::m_notchAutoIncrease` defaults to `{true}`**, not `{false}`. WDSP creates nbp0 with `1, // auto-increase notch width` (`RXA.c:105`). A `{false}` carry plus Task 4's unconditional reconcile silently disables it on every channel and fails bench row 8. **Task 2 owns this member and both notch carries, written outside `#ifdef HAVE_WDSP`. Task 4 declares neither.**

### Duplicated declarations, single owner

4. `NotchModel::visualEnabled` / `setVisualEnabled` / `visualEnabledChanged` and the `NotchVisualEnabled` key: **Task 3 owns.** Task 10 consumes; it keeps only `SpectrumWidget::setVisualNotchEnabled` and the `chkVisualNotch` binding.
5. The five `SpectrumWidget` notch signals plus `m_selectedNotchId` / `m_hoveredNotchId`: **Task 6 owns** (its colour logic reads both). Task 7 consumes, and produces only `NotchGrab`, `m_notchGrab`, the drag anchors and the hit-test seams.
6. Bounds constants: **`NotchModel` owns** `kMaxNotchWidthHz = 10000.0`, `kMinNotchCentreHz = 100000.0`, `kMaxNotchCentreHz = 61440000.0`. Task 9 keeps only `kMnfWidthMinHz` and `kMnfWidthStepHz`; Task 7 uses `NotchModel::kMaxNotchWidthHz`. `SpectrumWidget.cpp:125` already includes `models/BandPlanManager.h`, so the gui-to-models include is established.
7. Width clamping lives **only** in `NotchModel::setWidth`. Task 7's wheel handler is a bare `setWidth(id, current + delta * step)` with no second clamp.
8. `notchSpecRect()` (Task 7) is the **single** geometry source; Task 6's two paint sites call it, so hit boxes cannot drift from drawn markers.

### Naming

9. Task 6 uses `NotchModel::kDefaultNotchWidthHz` and `kNarrowNotchWidthHz` (Task 3's names). Task 3 does not rename.
10. **Unit boundary, declare it and honour it:** `NotchMarker::freqMhz` is MHz. Everything else, `NotchModel::centerHz`, all five `notch*Requested` signals, `setNotchMinWidthHz`, and the dent maths, is **Hz**. `MainWindow::refreshPanNotchMarkers` is the only conversion site.
11. Task 4 cites `RXA.c:87` (master run) and `RXA.c:105` (autoincr), not `:86` / `:103`.

### Missing wiring to add

12. **`RxChannel::minNotchWidthHz()` must reach `SpectrumWidget::setNotchMinWidthHz()`.** Without it the widget keeps a hardcoded 100.0 forever, which feeds Task 7's edge-drag clamp and Task 10's dent span. **Task 10 owns this wiring**, alongside a `minNotchWidthChanged(double)` signal driven from `setFilterSizeSamples` / `setSampleRate` so bench row 7 can pass.
13. **Task 8 registers a test.** Suggest `tst_tnf_ui_wiring`, covering the accelerator and the menu two-way sync.
14. **Task 8 names its `+TNF` click slot** and either reuses `MainWindow::onNotchCreateRequested(double, bool)` (Task 6) or declares a distinct member.
15. **Task 8 states explicitly that it deletes the disabled `MNF` stub at `SpectrumOverlayPanel.cpp:273-278`**, keeping `+TNF` at index 1. Task 10 consumes that deletion.
16. Either wire `NotchModel::notchAddRejected` to a status message (Task 8) or drop it along with `notchNearFreq()` / `notchesInBandwidth()` / `notchSurrounding()`. As written a `+TNF` press inside the 10 Hz dedupe window is silently ignored with no operator feedback.

### Sequencing

17. Tasks 1, 2, 4 and 9 each add a forward declaration plus a `friend class ::TestX;` line to the same two `NEREUS_BUILD_TESTS` blocks in `src/core/WdspEngine.h:100-124` and `:674-695`. Expect textual conflicts. Land them in task order and rebase rather than merging.

---

## Open decisions for the maintainer

These are user-visible defaults and attribution scope, which CLAUDE.md places outside autonomous change. **Resolve before Task 3 lands.**

| # | Decision | Options |
| --- | --- | --- |
| D-a | `NotchModel::globalEnabled` default | **RESOLVED (JJ, 2026-07-29): `false`.** Matches Thetis (`chkTNF` ships unchecked) and WDSP (`create_notchdb` master run `0`, `RXA.c:87`). Task 3 sets the model default `false`; Task 6 sets `m_notchGlobalEnabled{false}` in the widget to match; Task 9's checkbox reflects it. Note the operator consequence: the first notch placed does nothing until the master switch is on, so Task 8's status-bar indicator must make the off state visible. |
| D-b | `NotchModel::autoIncrease` default | **RESOLVED: `true`.** Confirmed independently in both upstreams: WDSP creates nbp0 with `1, // auto-increase notch width` (`RXA.c:105`), and Thetis ships `chkMNFAutoIncrease.Checked = true` (`setup.designer.cs:44197`). Note the P/Invoke takes a bool (`dsp.cs:737`) while the C takes an int; our wrapper takes `bool` and converts at the boundary. |
| D-c | `NotchModel` attribution scope | **RESOLVED: keep three files, move the cite.** `NotchModel` carries the AetherSDR block plus `radio.cs` and `console.cs` only. The visual-notch default cite moves to `DspSetupPages.cpp`, which is already `setup.cs`-registered, so no fourth verbatim header is needed. The default itself is verified: `chkVisualNotch` has no `Checked` assignment anywhere (`setup.designer.cs:44169-44179`; only `AutoSize`, `Name`, `Text` and the handler hookup), so Windows Forms leaves it unchecked and `false` is correct. |
| D-d | `adminBusy` semantics | **RESOLVED: keep the flag, Task 9's reading is correct.** Thetis clears `AddActive` / `EditActive` before it writes (`setup.cs:17941-17958`), so the flag guards the panadapter path during the Settings-page edit window and never blocks the page's own commit. Design §9 and §5.4 are both accurate; the apparent contradiction is resolved by the clear-before-write ordering, which the task must reproduce. |
| D-e | Right-click on empty pan | **RESOLVED (JJ, 2026-07-29): add it to the existing popup.** Do not convert `SpectrumOverlayMenu` to a `QMenu`. It is a `QVBoxLayout` with section headers (`SpectrumOverlayMenu.cpp:19`, `:57` "Waterfall", `:110` "Spectrum"), so add a third section header "Notch" and an "Add notch here" button beneath it, styled from the same stylesheet block (`:37`, `:53`). Add a `notchAddRequested(double freqHz)` signal to `SpectrumOverlayMenu.h:54-62` and have `SpectrumWidget` pass the frequency under the cursor at popup time. **Task 7 owns this**, reversing its decision to drop the gesture. Note `SpectrumOverlayMenu` is AetherSDR-registered, not Thetis-registered, so the button carries no Thetis cite (same constraint as `SpectrumOverlayPanel`, Global Constraints). |
| D-f | TNF toggle accelerator | **RESOLVED: Ctrl+Shift+N**, as Task 8 proposed and verified unclaimed. Consistent with the existing Ctrl+Shift+S (Spot Hub) and Ctrl+Shift+R (FreeDV Reporter) chords. |

---

## Shared interface contract

Read this before starting any task. Every signature below was read out of the tree, not recalled.

# TNF Shared Interface Contract

Every signature, guard pattern and convention below was read out of the tree at
`/Users/j.j.boyd/NereusSDR/.claude/worktrees/tdd-test-performance-2b2c10`.
Line numbers are as of this reading. Quote blocks are verbatim.

---

## 1. `RxChannel` (`src/core/RxChannel.h` 967 lines / `.cpp` 2301 lines)

### 1.1 Class declaration head (`RxChannel.h:218-249`, after ~215 lines of licence headers)

```cpp
#include "NbFamily.h"
#include "WdspTypes.h"
#include "dsp/ChannelConfig.h"
#include "dsp/RxChannelState.h"

#ifdef HAVE_DFNR
#include "DeepFilterFilter.h"
#endif

#ifdef HAVE_MNR
#include "MacNRFilter.h"
#endif

#include <QObject>

#include <atomic>
#include <cstring>
#include <memory>

namespace NereusSDR {

class WdspEngine;  // forward declaration for rebuild()

// Per-receiver WDSP channel wrapper.
// ...
class RxChannel : public QObject {
    Q_OBJECT

    Q_PROPERTY(NereusSDR::DSPMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(NereusSDR::AGCMode agcMode READ agcMode WRITE setAgcMode NOTIFY agcModeChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

public:
    explicit RxChannel(int channelId, int bufferSize, int sampleRate,
                       QObject* parent = nullptr);
    ~RxChannel() override;

    int channelId() const { return m_channelId; }
    int bufferSize() const { return m_bufferSize; }
    int sampleRate() const { return m_sampleRate; }
```

Note: only three `Q_PROPERTY` declarations exist. The NR/AGC/notch surface is
plain C++ getters+setters, NOT Q_PROPERTY. Follow that. Do not add
`Q_PROPERTY` for notch state on `RxChannel`.

### 1.2 `setShiftFrequency` end to end, VERBATIM (`RxChannel.cpp:1464-1488`)

```cpp
// ---------------------------------------------------------------------------
// Frequency shift (pan offset from VFO)
// ---------------------------------------------------------------------------

void RxChannel::setShiftFrequency(double offsetHz)
{
    if (offsetHz == m_shiftOffsetHz) {
        return;
    }

    m_shiftOffsetHz = offsetHz;

#ifdef HAVE_WDSP
    if (std::abs(offsetHz) < 0.5) {
        // No offset ,  disable shift for efficiency
        SetRXAShiftRun(m_channelId, 0);
    } else {
        // From Thetis radio.cs:1417-1418 ,  both calls use the same sign
        SetRXAShiftFreq(m_channelId, offsetHz);
        RXANBPSetShiftFrequency(m_channelId, offsetHz);
        SetRXAShiftRun(m_channelId, 1);
    }
#else
    Q_UNUSED(offsetHz);
#endif
}
```

Declaration side (`RxChannel.h:615-617`):

```cpp
    // --- Frequency shift (for pan offset from VFO) ---

    void setShiftFrequency(double offsetHz);
```

Carry member (`RxChannel.h:936-938`):

```cpp
    // Shift offset carry (mirrors what was last passed to setShiftFrequency)
    double m_shiftOffsetHz{0.0};
```

Spec §4.3 requires this function be changed so `SetRXAShiftFreq` +
`RXANBPSetShiftFrequency` fire on BOTH branches and only `SetRXAShiftRun`
stays gated on magnitude, and the stale cite `radio.cs:1417-1418` becomes
`radio.cs:1419-1420 [v2.10.3.15]`.

### 1.3 NR setters, VERBATIM (`RxChannel.cpp:875-920`) ,  the canonical setter shape

```cpp
void RxChannel::setAnrTaps(int taps)
{
    m_nr1Tuning.taps = taps;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:681-698 [v2.10.3.13]
    SetRXAANRTaps(m_channelId, taps);
#endif
}

void RxChannel::setAnrGain(double gain)
{
    m_nr1Tuning.gain = gain;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:8545 [v2.10.3.13] ,  caller has already applied ×1e-6.
    // Passes raw WDSP-domain value directly to SetRXAANRGain.
    SetRXAANRGain(m_channelId, gain);
#endif
}

void RxChannel::setAnrPosition(NrPosition p)
{
    m_nr1Tuning.position = p;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:8723 [v2.10.3.13]
    SetRXAANRPosition(m_channelId, static_cast<int>(p));
#endif
}
```

Struct-level setter that fans out to several WDSP calls
(`RxChannel.cpp:864-873`):

```cpp
void RxChannel::setAnrTuning(const Nr1Tuning& t)
{
    m_nr1Tuning = t;
#ifdef HAVE_WDSP
    // From Thetis radio.cs:681-698 [v2.10.3.13] ,  SetNRVals() calls
    // WDSP.SetRXAANRVals(id, taps, delay, gain, leak) with already-scaled values.
    SetRXAANRVals(m_channelId, t.taps, t.delay, t.gain, t.leakage);
    SetRXAANRPosition(m_channelId, static_cast<int>(t.position));
#endif
}
```

### 1.4 The `HAVE_WDSP` guard pattern (three variants in use, all in this file)

1. **Carry then guard** (dominant, use for notch setters):
   member write happens OUTSIDE the guard; WDSP calls inside.
   `#endif` with no `#else` when the parameter is consumed by the member write.
2. **Guard with `#else Q_UNUSED(x);`** when nothing outside the guard uses the
   argument (see `setBinauralEnabled`, `setShiftFrequency`).
3. **Guard with `#else` returning a stub value** for getters
   (e.g. `RxChannel.cpp:464-468`, `473-482`, `487-499`).

Spec §4.6 makes this load-bearing for TNF: `setNotchTuneFrequency` must set its
carry member outside `#ifdef HAVE_WDSP`, and expose `notchTuneFrequencyHz()`.

`RxChannel.cpp` includes at `:248-275`:

```cpp
#include "RxChannel.h"
#include "AppSettings.h"
#include "LogCategories.h"
#include "NbFamily.h"
#include "SampleRateCatalog.h"  // bufferSizeForRate() ,  for setSampleRate()
#include "WdspEngine.h"
#include "wdsp_api.h"
```

`wdsp_api.h` is included unconditionally; it self-guards internally.

### 1.5 Member naming and atomics

- Members are `m_camelCase`, brace-initialised with the default inline.
- `std::atomic<T>` is used ONLY for state the audio thread or the meter timer
  reads while the main thread writes. Verbatim examples (`RxChannel.h:850-887`):

```cpp
    std::atomic<double> m_afGain{1.0};
    std::atomic<bool> m_binauralEnabled{false};
    std::atomic<bool> m_active{false};
    // threshold default: From Thetis console.cs:45977 ,  agc_thresh_point = -20
    std::atomic<int> m_agcThreshold{-20};
    // ----- NR state (Sub-epic C-1) -----
    // From Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13]
    std::atomic<NrSlot> m_activeNr{NrSlot::Off};
```

- Non-atomic plain members are used for state ONLY the main thread touches,
  with the reason stated in comment (`RxChannel.h:880-883`):

```cpp
    // Full tuning state per stage.  Written by the main thread under no lock
    // (struct setters copy by value; WDSP setters are the authoritative state
    // for the audio thread).
    Nr1Tuning m_nr1Tuning;
```

Notch carry state falls in the second category: WDSP owns the authoritative
notch DB; the C++ carry is main-thread only. Use plain members, not atomics,
and say so in a comment. Atomic accessors use explicit ordering where they were
added late: `m_activeNr.load(std::memory_order_acquire)`.

- Enum-class parameters are converted at the WDSP boundary with
  `static_cast<int>(x)`; booleans with `x ? 1 : 0`.
- Class-scope struct definitions (`Nr1Tuning` etc.) live in the public section
  with per-field defaults + a Thetis cite per field. Same shape for a `Notch`
  struct if one lands on `RxChannel`.

---

## 2. `src/core/wdsp_api.h` (1407 lines)

### 2.1 File structure

- `#pragma once` at `:239`, after a modification-history block (`:20-238`) and
  the four verbatim upstream licence headers.
- `#ifdef HAVE_WDSP` at `:255`, `extern "C" {` at `:257`, `} // extern "C"` at
  `:1343`, `#endif // HAVE_WDSP` at `:1345`.
- `#ifndef HAVE_WDSP` block `:1370-1407` holds stub-build constants only.
- **Every new declaration must be added inside the `extern "C"` block.**
- **Every batch of new declarations must also get a dated entry appended to the
  modification-history block** at the top, naming the author, the task, the
  upstream file + line range and the version stamp. That is an established,
  unbroken convention: 20+ entries, e.g.

```
//   2026-04-27 ,  SetTXAPanelGain1 added by J.J. Boyd (KG4VCF) during 3M-1b
//                 Task D.6 ,  TxChannel mic-mute path via setMicPreamp.
//                 Signature matches wdsp/patchpanel.c:209 [v2.10.3.13].
//                 AI-assisted transformation via Anthropic Claude Code.
```

### 2.2 Grouping and comment style

Two banner widths are in use, both live:

```cpp
// ---------------------------------------------------------------------------
// Frequency shift (shift.h)
// ---------------------------------------------------------------------------
```

```cpp
// =====================================================================
// NR1 ,  Adaptive Noise Reduction (WDSP anr.c, Warren Pratt NR0V)
// From Thetis wdsp/anr.h:47-52 [v2.10.3.13]. Channel = WDSP channel id.
// =====================================================================
```

The `=====` form is used for newer, subsystem-scoped blocks and carries the
upstream header cite on the banner. Use that form for the notch block. Blank
line between declarations inside a `=====` group; no blank line inside a tight
`-----` group.

### 2.3 The existing RXANBP declarations, VERBATIM

`wdsp_api.h:328-341`:

```cpp
// ---------------------------------------------------------------------------
// Bandpass filter (bandpass.h / bandpass.c)
// ---------------------------------------------------------------------------

void SetRXABandpassFreqs(int channel, double f_low, double f_high);

// CRITICAL: SetRXABandpassFreqs only updates bp1, which only runs when
// AMD/SNBA/EMNR/ANF/ANR is enabled. For plain SSB the active filter is
// nbp0, controlled by RXANBPSetFreqs. Thetis always calls BOTH together
// (rxa.cs:110-111, radio.cs:603-604). Call both in setFilterFreqs.
void RXANBPSetFreqs(int channel, double flow, double fhigh);

void SetRXABandpassNC(int channel, int nc);
```

`wdsp_api.h:346-357`:

```cpp
// ---------------------------------------------------------------------------
// Frequency shift (shift.h)
// ---------------------------------------------------------------------------

void SetRXAShiftRun(int channel, int run);

void SetRXAShiftFreq(int channel, double fshift);

// ---------------------------------------------------------------------------
// Notch bandpass shift (nbp.h) ,  From Thetis radio.cs:1418
// ---------------------------------------------------------------------------

void RXANBPSetShiftFrequency(int channel, double shift);
```

**No other RXANBP notch function is declared today.** `grep RXANBP src/`
returns only `wdsp_api.h`. The nine functions from spec §6.1 are all present
and `PORT`-exported in the vendored tree; I verified each line number in
`third_party/wdsp/src/nbp.c`:

| Function | nbp.c line | Verified |
| --- | --- | --- |
| `RXANBPAddNotch` | 362 | yes |
| `RXANBPGetNotch` | 393 | yes |
| `RXANBPDeleteNotch` | 418 | yes |
| `RXANBPEditNotch` | 444 | yes |
| `RXANBPGetNumNotches` | 465 | yes |
| `RXANBPSetTuneFrequency` | 475 | yes |
| `RXANBPSetNotchesRun` | 499 | yes |
| `RXANBPGetMinNotchWidth` | 594 | yes |
| `RXANBPSetAutoIncrease` | 604 | yes |

**Trap worth stating**: `third_party/wdsp/src/nbp.h` declares only
`RXANBPSetFreqs` / `SetNC` / `SetMP` (lines 96 / 98 / 100). The notch entry
points exist in `nbp.c` with the `PORT` macro but are NOT in the vendored
header, which is exactly why `wdsp_api.h` has to declare them itself. That is
the pre-existing pattern (`RXANBPSetShiftFrequency` is in the same situation).

---

## 3. `RadioModel` (`src/models/RadioModel.h` 3277 lines / `.cpp` 13144 lines)

### 3.1 Sub-model ownership and exposure (the pattern `NotchModel` must copy)

Forward declaration at `RadioModel.h:182`: `class SpotModel;`

Accessor, `RadioModel.h:768`:

```cpp
    SpotModel*            spotModel()           const { return m_spotModel.get(); }
```

Member, `RadioModel.h:3066`:

```cpp
    std::unique_ptr<SpotModel>            m_spotModel;
```

Construction, `RadioModel.cpp:1595`:

```cpp
    m_spotModel           = std::make_unique<SpotModel>(this);
```

The sub-model itself (`SpotModel.h:76-80`):

```cpp
class SpotModel : public QObject {
    Q_OBJECT
public:
    explicit SpotModel(QObject* parent = nullptr);
    ~SpotModel() override = default;
```

`NotchModel` gets the identical treatment: forward decl in `RadioModel.h`,
`std::unique_ptr<NotchModel> m_notchModel;` in the private members, a
`NotchModel* notchModel() const { return m_notchModel.get(); }` accessor in the
public accessor block, `std::make_unique<NotchModel>(this)` in the ctor.

**Build registration**: `CMakeLists.txt` lists model sources EXPLICITLY, no
glob. `src/models/NotchModel.cpp` must be added to `set(MODEL_SOURCES ...)`
at `CMakeLists.txt:656-673`.

### 3.2 Slice accessors

`RadioModel.h:374`:

```cpp
    QList<SliceModel*> slices() const { return m_slices; }
```

`RadioModel.h:383` (doc block at `:376-382` warns ids are NOT list positions):

```cpp
    SliceModel* sliceById(int sliceId) const;
```

Impl `RadioModel.cpp:3735-3743`:

```cpp
SliceModel* RadioModel::sliceById(int sliceId) const
{
    for (SliceModel* s : m_slices) {
        if (s && s->sliceIndex() == sliceId) {
            return s;
        }
    }
    return nullptr;
}
```

`RadioModel.h:412`, impl `RadioModel.cpp:2909-2916`:

```cpp
    int maxSlices() const;
```
```cpp
int RadioModel::maxSlices() const
{
    if (!isConnected()) {
        return 1;
    }
    const int n = boardCapabilities().maxSlices;
    return n > 0 ? n : 1;
}
```

Invariant to rely on: **WDSP RX channel id == `slice->sliceIndex()`**
(stated at `RadioModel.cpp:3110`, `WdspEngine.h:194-208`). Fan-out therefore
looks like:

```cpp
for (SliceModel* s : std::as_const(m_slices)) {
    if (RxChannel* ch = m_wdspEngine->rxChannel(s->sliceIndex())) { /* ... */ }
}
```

`std::as_const` on the loop is the house style in this file.

### 3.3 `bindSliceToStream` (declared `RadioModel.h:2417`, defined `RadioModel.cpp:3572`)

```cpp
    bool bindSliceToStream(SliceModel* slice, double frequencyHz);
```

Head (`RadioModel.cpp:3572-3605`):

```cpp
bool RadioModel::bindSliceToStream(SliceModel* slice, double frequencyHz)
{
    if (!slice) { return false; }
    if (m_streamAllocator.streamCount() <= 0) { return false; }

    const int previousStream = slice->streamIndex();
    const bool ddcPinned =
        m_receiverManager && m_receiverManager->ddcFrequencyLocked();
    const bool soleOccupant =
        previousStream >= 0 && slicesOnStream(previousStream).size() == 1;

    const auto placement =
        (previousStream < 0)
            ? m_streamAllocator.placeSlice(frequencyHz)
            : m_streamAllocator.retuneSlice(previousStream, soleOccupant,
                                            ddcPinned, frequencyHz);

    using Outcome = NereusSDR::SliceStreamAllocator::Outcome;
```

The DDC centre push for `NewStream` / `RetunedStream` (`RadioModel.cpp:3646-3657`):

```cpp
        if (m_receiverManager) {
            m_receiverManager->setReceiverFrequency(
                placement.streamIndex,
                static_cast<quint64>(placement.newStreamCentreHz));
        }
        emit streamCentreChanged(
            placement.streamIndex, placement.newStreamCentreHz,
            m_streamAllocator.streamSampleRateHz(placement.streamIndex));
```

**The insertion point named by spec §4.2** (`RadioModel.cpp:3663-3679`):

```cpp
    slice->setStreamIndex(placement.streamIndex);
    slice->setShiftOffsetHz(placement.shiftOffsetHz);
    // ...
    // Push the offset into WDSP. RxChannel::setShiftFrequency is the Thetis
    // RXOsc port (radio.cs:1409-1420 [v2.10.3.15]): SetRXAShiftFreq +
    // RXANBPSetShiftFrequency.
    if (m_wdspEngine) {
        if (RxChannel* ch = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            ch->setShiftFrequency(placement.shiftOffsetHz);
        }
    }
```

Tail (`RadioModel.cpp:3705-3714`): `activateSliceChannel(slice);` then
`requestDdcAssignment(); return true;`.

### 3.4 `openRxChannelPool` (declared `RadioModel.h:447`, defined `RadioModel.cpp:3033`)

```cpp
    void openRxChannelPool(int poolSize, int inputBufferSize,
                           int inputSampleRateHz);
```

Body core (`RadioModel.cpp:3040-3068`):

```cpp
    const int requested = poolSize > 0 ? poolSize : 1;
    const int clamped = std::min(requested, WdspEngine::kMaxSliceChannels);
    if (clamped < requested) {
        qCWarning(lcDsp) << "RX channel pool request" << requested /* ... */;
    }

    for (int ch = WdspEngine::kFirstSliceChannelId; ch < clamped; ++ch) {
        if (!m_wdspEngine->rxChannel(ch)) {
            m_wdspEngine->createRxChannel(ch, inputBufferSize, 4096,
                                          inputSampleRateHz, 48000, 48000);
        }
    }
    // ...
    activateBoundSliceChannels();
}
```

### 3.5 `activateSliceChannel` (declared `RadioModel.h:466`, defined `RadioModel.cpp:3111`)

VERBATIM (`RadioModel.cpp:3111-3147`):

```cpp
void RadioModel::activateSliceChannel(SliceModel* slice)
{
    if (!m_wdspEngine || !slice || slice->streamIndex() < 0) {
        return;
    }

    // Sub-Epic I invariant: WDSP RX channel id == slice index.
    RxChannel* ch = m_wdspEngine->rxChannel(slice->sliceIndex());
    if (!ch || ch->isActive()) {
        // Already live. Leave it alone: connectToRadio's WDSP-init lambda
        // gives Slice A's channel the full state push (NR, SNB, APF, squelch,
        // audio panel, the lot) and then activates it, and re-running the
        // subset below on top of that would be a downgrade dressed as a
        // refresh.
        return;
    }

    ch->setMode(slice->dspMode());
    ch->setFilterFreqs(slice->filterLow(), slice->filterHigh());
    ch->setAgcMode(slice->agcMode());
    ch->setAgcTop(slice->rfGain());
    ch->setAfGain(slice->afGain() / 100.0);
    // The offset the allocator resolved for this slice. bindSliceToStream
    // pushes it too, but a reconnect re-opens the channel underneath an
    // already-bound slice, so it has to be re-seeded here as well.
    ch->setShiftFrequency(slice->shiftOffsetHz());

    ch->setActive(true);
}
```

Note the early return on `ch->isActive()` ,  spec §4.2 flags this as why
`activateSliceChannel` is dead as a notch hook, and §6.3 flags that
`connectToRadio` activates channel 0 at `RadioModel.cpp:5365` BEFORE
`openRxChannelPool` at `:5377`.

Siblings: `activateBoundSliceChannels()` (`.h:457` / `.cpp:3104`),
`deactivateSliceChannel(int sliceId)` (`.h:471` / `.cpp:3153`),
`bindUnboundSlices()` (`.h:478` / `.cpp:3163`).

### 3.6 `wireSliceSignals` (declared `RadioModel.h:2244`, defined `RadioModel.cpp:8218`)

```cpp
    void wireSliceSignals(SliceModel* slice);
```

Test seam already present (`RadioModel.h:1191`):

```cpp
    void wireSliceSignalsForTest() { wireSliceSignals(m_activeSlice); }
```

**Connect shape used throughout** (`RadioModel.cpp:8933-8946`):

```cpp
    connect(slice, &SliceModel::mutedChanged, this, [this, slice](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setMuted(v);
        }
        scheduleSettingsSave();
    });
```

`scheduleSettingsSave()` is declared `RadioModel.h:2308` (private); the
500 ms-debounced coalescer. `flushPendingSettingsSave()` is the public
force-run used on quit paths.

**The connect-time seed, spec §4.5** (`RadioModel.cpp:9202-9215`):

```cpp
    // Send initial frequency to radio (after connection init completes).
    // XIT offset applied here too so on-connect TX NCO matches the stored
    // XIT state without needing a separate update trigger.
    QTimer::singleShot(100, this, [this, slice]() {
        if (m_connection && m_connection->isConnected()) {
            int rxIdx = slice->streamIndex();
            quint64 freqHz = static_cast<quint64>(slice->frequency());
            if (rxIdx >= 0) {
                m_receiverManager->setReceiverFrequency(rxIdx, freqHz);
            }
            // Seed the transmit frequency from the TX-bound slice, which on
            // connect is usually but not necessarily this one.
            pushTxFrequencyFromTxSlice();
        }
    });
```

`freqHz` is at `:9207`, `setReceiverFrequency` at `:9209`. Matches the spec.

### 3.7 The `updateShiftFrequency` lambda, VERBATIM (`RadioModel.cpp:8956-8988`)

```cpp
    // RIT + DIG offset → WDSP shift frequency
    //
    // RIT (Receive Incremental Tuning): client-side demodulation offset that
    // does NOT retune the hardware VFO.
    // From Thetis console.cs ,  RIT adjusts receive demodulation without moving
    // the hardware DDC center.
    //
    // DIG offset: per-mode click-tune demodulation offset for DIGL/DIGU.
    // From Thetis console.cs:14637 (DIGUClickTuneOffset) and :14672
    // (DIGLClickTuneOffset). Both are int offsets in Hz; Thetis uses per-mode
    // filter re-centering internally, but NereusSDR implements DIG offset as
    // an additive shift on the same setShiftFrequency path as RIT.
    //
    // Combined: shift = ritOffset + digOffset (where digOffset is mode-gated).
    // For 3G-10 (single RX, no CTUN), the shift = these two terms only.
    auto updateShiftFrequency = [this, slice]() {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (!rxCh) { return; }
        double offset = slice->ritEnabled()
                        ? static_cast<double>(slice->ritHz())
                        : 0.0;
        // DIG offset per mode ,  Thetis console.cs:14637,14672
        if (slice->dspMode() == DSPMode::DIGL) {
            offset += static_cast<double>(slice->diglOffsetHz());
        } else if (slice->dspMode() == DSPMode::DIGU) {
            offset += static_cast<double>(slice->diguOffsetHz());
        }
        rxCh->setShiftFrequency(offset);
    };
    connect(slice, &SliceModel::ritEnabledChanged,  this, updateShiftFrequency);
    connect(slice, &SliceModel::ritHzChanged,        this, updateShiftFrequency);
    connect(slice, &SliceModel::diglOffsetHzChanged, this, updateShiftFrequency);
    connect(slice, &SliceModel::diguOffsetHzChanged, this, updateShiftFrequency);
    connect(slice, &SliceModel::dspModeChanged,      this, updateShiftFrequency);
```

`rxCh->setShiftFrequency(offset);` is at `:8982`. This is the writer spec §4.4
says must compose `placement.shiftOffsetHz + rit + dig`, mirrored at
`:3676`. Note the lambda does NOT null-check `m_wdspEngine` (only `rxCh`);
`RadioModel::wdspEngine()` is constructed in the ctor so it is non-null for
the object's lifetime.

### 3.8 Signals / test seams needed by the TNF tests

`RadioModel.h:1947`:
```cpp
    void streamCentreChanged(int streamIndex, double centreHz, int sampleRateHz);
```

`RadioModel.h:2483-2485`:
```cpp
    double streamCentreHzForTest(int streamIndex) const {
        return m_streamAllocator.streamCentreHz(streamIndex);
    }
```

Other useful existing seams: `configureStreamPool(int userDdcCount, int maxSlices, int defaultRateHz)` (`.h:429`), `streamPoolSize()`, `activeStreamCount()`, `injectConnectionForTest(RadioConnection*)`, `wdspEngine()`, `addSlice()`.

---

## 4. `SliceStreamAllocator` (`src/core/SliceStreamAllocator.h` / `.cpp`)

### 4.1 `Placement` struct + `Outcome`, VERBATIM (`SliceStreamAllocator.h:37-51`)

```cpp
    enum class Outcome {
        JoinedExisting,   ///< Fits an active stream's window; set shift only.
        NewStream,        ///< Claimed a free DDC, centred on the slice.
        RetunedStream,    ///< Sole occupant; moved its stream's centre instead.
        Rejected          ///< No stream fits and none free. `reason` explains.
    };

    struct Placement {
        Outcome outcome{Outcome::Rejected};
        int     streamIndex{-1};
        double  shiftOffsetHz{0.0};     ///< slice freq minus stream centre
        double  newStreamCentreHz{0.0}; ///< set for NewStream / RetunedStream
        QString reason;                 ///< human-readable, for Rejected
    };
```

### 4.2 Public API (`SliceStreamAllocator.h:53-98`)

```cpp
    void configure(int userDdcCount, int maxSlices);
    void activateStream(int streamIndex, double centreHz, int sampleRateHz);
    void deactivateStream(int streamIndex);
    Placement placeSlice(double frequencyHz) const;
    Placement retuneSlice(int currentStream,
                          bool soleOccupant,
                          bool ddcPinned,
                          double frequencyHz) const;

    int  streamCount() const { return m_streams.size(); }
    int  activeStreamCount() const;
    bool isStreamActive(int streamIndex) const;
    double streamCentreHz(int streamIndex) const;
    int  streamSampleRateHz(int streamIndex) const;
    void setDefaultSampleRateHz(int rateHz) { m_defaultRateHz = rateHz; }
```

`streamCentreHz` impl (`SliceStreamAllocator.cpp:159-163`):

```cpp
double SliceStreamAllocator::streamCentreHz(int streamIndex) const
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return 0.0; }
    return m_streams.at(streamIndex).centreHz;
}
```

### 4.3 Outcome semantics ,  exactly what each branch sets

`placeSlice` (`SliceStreamAllocator.cpp:60-95`):

| Branch | outcome | streamIndex | shiftOffsetHz | newStreamCentreHz |
| --- | --- | --- | --- | --- |
| window covers freq | `JoinedExisting` | matching stream | `frequencyHz - centreHz` | 0.0 (unset) |
| free DDC exists | `NewStream` | `firstFreeStream()` | **0.0** | `frequencyHz` |
| none | `Rejected` | -1 | 0.0 | 0.0 (`reason` set) |

`retuneSlice` (`SliceStreamAllocator.cpp:97-142`), in evaluation order:

1. `haveStream && soleOccupant && (!ddcPinned || !inWindow)` →
   `RetunedStream`, `streamIndex = currentStream`, **`shiftOffsetHz = 0.0`**,
   `newStreamCentreHz = frequencyHz`.
2. `inWindow` → `JoinedExisting`, `shiftOffsetHz = frequencyHz - centreHz`.
3. otherwise → falls through to `placeSlice(frequencyHz)`.

`windowContains` (`SliceStreamAllocator.cpp:36-48`), strict on both sides:

```cpp
bool SliceStreamAllocator::windowContains(const Stream& s,
                                          double frequencyHz) const
{
    if (!s.active || s.sampleRateHz <= 0) { return false; }
    const double halfWindow = static_cast<double>(s.sampleRateHz) / 2.0;
    const double offset     = frequencyHz - s.centreHz;
    // Strict both sides. From Thetis console.cs:31920 [v2.10.3.15]:
    //   if (rx2_osc > -sample_rate_rx1 / 2 && rx2_osc < sample_rate_rx1 / 2)
    // MW0LGE [2.7.0.9] only when RX'ing. Fixes issue where multirx would be
    // outside sample area after a tx  [original inline comment from
    // console.cs:31913]
    return offset > -halfWindow && offset < halfWindow;
}
```

**Consequence for spec §4.1/§4.2 that authors must internalise:** in the
dominant non-CTUN sole-occupant case the outcome is `RetunedStream` with
`shiftOffsetHz == 0.0`, which is exactly why `setShiftFrequency`'s
value-equality early return makes it a dead hook for the notch tune frequency.
The absolute quantity to push is the stream centre, reachable as
`m_streamAllocator.streamCentreHz(placement.streamIndex)` or, at bind time,
`placement.newStreamCentreHz` for New/Retuned outcomes.

---

## 5. `AppSettings` (`src/core/AppSettings.h`)

### 5.1 Exact API (`AppSettings.h:103-145`)

```cpp
class AppSettings {
public:
    static AppSettings& instance();
    explicit AppSettings(const QString& filePath);   // tests only
    ~AppSettings() = default;
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    void load();
    void save();

    QVariant value(const QString& key, const QVariant& defaultValue = {}) const;
    void setValue(const QString& key, const QVariant& val);
    void remove(const QString& key);
    bool contains(const QString& key) const;
    QStringList allKeys() const;
    void clear();                       // test isolation; does NOT save()

    QVariant stationValue(const QString& key, const QVariant& defaultValue = {}) const;
    void setStationValue(const QString& key, const QVariant& val);
    QString stationName() const;
    void setStationName(const QString& name);
    QString filePath() const { return m_filePath; }
```

Never `QSettings`. Access is always `auto& s = AppSettings::instance();`.

### 5.2 True/False convention

Booleans persist as the literal strings `"True"` / `"False"`. The canonical
helper is file-local in `SliceModel.cpp:1594`:

```cpp
// Boolean → AppSettings canonical string.
QString boolStr(bool v) { return v ? QStringLiteral("True") : QStringLiteral("False"); }
```

Read side is a string compare, both forms in use in-tree:

```cpp
setAutoAgcEnabled(s.value(bp + QStringLiteral("AgcAutoEnabled")).toString() == QLatin1String("True"));
```
```cpp
bool on = s.value("MyFeatureEnabled", "False").toString() == "True";
```

### 5.3 Real per-model save/restore pair (`SliceModel.cpp`)

Key-prefix helpers, file-local anonymous namespace (`SliceModel.cpp:1560-1592`):

```cpp
namespace {

// Build the per-band prefix string, e.g. "Slice0/Band20m/".
QString bandPrefix(int sliceIndex, Band band)
{
    return QStringLiteral("Slice%1/Band%2/")
               .arg(sliceIndex)
               .arg(bandKeyName(band));
}

// Build the session-state prefix string, e.g. "Slice0/".
QString slicePrefix(int sliceIndex)
{
    return QStringLiteral("Slice%1/").arg(sliceIndex);
}
```

Save (`SliceModel.cpp:1612-1665`, excerpt):

```cpp
void SliceModel::saveToSettings(Band band)
{
    auto& s = AppSettings::instance();
    const QString bp = bandPrefix(m_sliceIndex, band);
    const QString sp = slicePrefix(m_sliceIndex);

    s.setValue(bp + QStringLiteral("Frequency"),    m_frequency);
    s.setValue(bp + QStringLiteral("AgcThreshold"), m_agcThreshold);
    // ...
    s.setValue(bp + QStringLiteral("DspMode"),      static_cast<int>(m_dspMode));
    s.setValue(bp + QStringLiteral("DiversityFineNullEnabled"),
               boolStr(m_diversityFineNullEnabled));

    // ── Session state (band-agnostic) ─────────────────────────────────────────
    s.setValue(sp + QStringLiteral("NrActive"),        static_cast<int>(m_activeNr));
    s.setValue(sp + QStringLiteral("Nr1Taps"),         m_nr1Taps);
    s.setValue(sp + QStringLiteral("Nr2AeFilter"),     boolStr(m_nr2AeFilter));
```

Restore (`SliceModel.cpp:1730-1760`, excerpt). **Every key is `contains`-guarded
so a missing key leaves the in-memory default untouched, and each restore goes
through the public setter so signals fire:**

```cpp
void SliceModel::restoreFromSettings(Band band)
{
    auto& s = AppSettings::instance();
    const QString bp = bandPrefix(m_sliceIndex, band);
    const QString sp = slicePrefix(m_sliceIndex);

    // ── Per-band DSP state ────────────────────────────────────────────────────
    // Each key: if absent, leave the current SliceModel default unchanged.

    if (s.contains(bp + QStringLiteral("Frequency"))) {
        setFrequency(s.value(bp + QStringLiteral("Frequency")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("AgcThreshold"))) {
        setAgcThreshold(s.value(bp + QStringLiteral("AgcThreshold")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcAutoEnabled"))) {
        setAutoAgcEnabled(s.value(bp + QStringLiteral("AgcAutoEnabled")).toString() == QLatin1String("True"));
    }
```

Declarations (`SliceModel.h:794-796`), plus the documented contract that
`saveToSettings` does NOT call `AppSettings::save()`:

```cpp
    void saveToSettings(NereusSDR::Band band);
    void restoreFromSettings(NereusSDR::Band band);
    static void migrateLegacyKeys();
```

Key names are PascalCase with `/`-delimited namespace segments. Spec §5.5 /
§9 name `NotchAutoIncrease` as one such key.

---

## 6. `SpectrumWidget` (`src/gui/SpectrumWidget.h` / `.cpp`)

### 6.1 Spot-overlay push API + ForTest convention, VERBATIM (`SpectrumWidget.h:1010-1122`)

The whole block is re-opened under a fresh `public:` for a MOC reason worth
copying if the notch API lands next to it:

```cpp
public:
    // ── Spot overlay (Phase 3J-2 Task E1) ─────────────────────────────────
    // Public structs + setters re-declared under a fresh `public:` access
    // specifier so MOC doesn't try to interpret the nested struct as a
    // slot declaration (the enclosing block above is `public slots:`).
    //
    // From AetherSDR src/gui/SpectrumWidget.h:283-294 [@0cd4559]
    struct SpotMarker {
        int    index{-1};
        QString callsign;
        double freqMhz{0.0};
        QString color;       // #AARRGGBB or empty for default
        QString mode;
        QColor  dxccColor;   // DXCC-aware color from DxccColorProvider (#330)
        QString source;
        QString spotterCallsign;
        QString comment;
        qint64  timestampMs{0};
    };

    // From AetherSDR src/gui/SpectrumWidget.h:297-300 [@0cd4559]
    struct SpotCluster {
        QRect rect;
        QVector<SpotMarker> spots;
    };

    // From AetherSDR src/gui/SpectrumWidget.h:635-639 [@0cd4559]
    struct SpotHitRect {
        QRect  rect;
        double freqMhz{0.0};
        int    markerIndex{-1};
    };

    void setSpotMarkers(const QVector<SpotMarker>& markers);
    void setHoverSpotIndexExternal(int idx) { /* ... */ }
    void setSpotSourceVisible(const QString& source, bool visible) { /* ... */ }
    bool isSpotSourceVisible(const QString& source) const { /* ... */ }
    void setShowSpots(bool on) { m_showSpots = on; update(); }
    bool showSpots() const { return m_showSpots; }
    void setSpotFontSize(int px) { m_spotFontSize = px; update(); }
    void setSpotMaxLevels(int n) { m_spotMaxLevels = n; update(); }
    void setSpotStartPct(int pct) { m_spotStartPct = pct; update(); }
    void setSpotOverrideColors(bool on) { m_spotOverrideColors = on; update(); }
    void setSpotOverrideBg(bool on) { m_spotOverrideBg = on; update(); }
    void setSpotColor(const QColor& c) { m_spotColor = c; update(); }
    void setSpotBgColor(const QColor& c) { m_spotBgColor = c; update(); }
    void setSpotBgOpacity(int pct) { m_spotBgOpacity = pct; update(); }

    void loadSpotDisplaySettings();

    // Test seams (Phase 3J-2 Task E1). Public read-only views into the
    // private state drawSpotMarkers() rebuilds each frame; the test
    // suite asserts contract by inspecting these vectors after a
    // synthetic render pass.
    const QVector<SpotMarker>&   spotMarkersForTest()   const { return m_spotMarkers; }
    const QVector<SpotHitRect>&  spotClickRectsForTest() const { return m_spotClickRects; }
    const QVector<SpotCluster>&  spotClustersForTest()  const { return m_spotClusters; }
    void  drawSpotMarkersForTest(QPainter& p, const QRect& specRect) {
        drawSpotMarkers(p, specRect);
    }

    int    spotFontSizeForTest()        const { return m_spotFontSize; }
    bool   spotOverrideColorsForTest()  const { return m_spotOverrideColors; }
    QColor spotColorForTest()           const { return m_spotColor; }
```

**ForTest convention, stated:** suffix `ForTest`, public, `const` and
returning a `const&` for containers; a non-const `drawXForTest(QPainter&, QRect)`
forwarder exists so a test can drive the private draw routine and then inspect
the rebuilt hit rects without a real paint cycle. Mirror this exactly for
notches (`notchMarkersForTest()`, `notchHitRectsForTest()`,
`drawNotchMarkersForTest(...)`).

Push impl (`SpectrumWidget.cpp:5163-5171`) ,  note it does NOT call
`markOverlayDirty()`, only `update()`, and it short-circuits on a
visual-equality helper:

```cpp
void SpectrumWidget::setSpotMarkers(const QVector<SpotMarker>& markers)
{
    const bool visualChange = !spotMarkersVisuallyEqual(m_spotMarkers, markers);
    m_spotMarkers = markers;
    if (!visualChange) {
        return;
    }
    update();
}
```

Private draw declaration (`SpectrumWidget.h:1321`):

```cpp
    void drawSpotMarkers(QPainter& p, const QRect& specRect);
```

Called from two places, both gated on `if (m_showSpots)`:
- CPU `paintEvent` path, `SpectrumWidget.cpp:3043-3045`, between
  `drawWaterfall` and `drawVfoMarker`.
- GPU static-overlay rebuild, `SpectrumWidget.cpp:7143-7145`, same relative
  ordering. The in-tree comment there records the upstream ordering as
  "drawSpotMarkers between drawTnfMarkers and drawSliceMarkers"
  (AetherSDR `SpectrumWidget.cpp:3787 [@0cd4559]`), which is where notch
  markers belong: **before** `drawSpotMarkers`.

### 6.2 `markOverlayDirty` + overlay-cache members

`SpectrumWidget.h:2059-2068`:

```cpp
    // Invalidate the GPU-path cached overlay texture so grid, labels,
    // dBm scale, waterfall filter/zero-line/timestamp overlays, and
    // other QPainter-drawn chrome re-render on next frame. Safe no-op
    // when the GPU path is disabled.
    void markOverlayDirty() {
#ifdef NEREUS_GPU_SPECTRUM
        m_overlayStaticDirty = true;
#endif
        update();
    }
```

It is the LAST member of the class, private, immediately before the closing
brace. Cache members, all inside `#ifdef NEREUS_GPU_SPECTRUM`
(`SpectrumWidget.h:1984-2018`):

```cpp
    // ---- Overlay GPU resources ----
    QRhiGraphicsPipeline*       m_ovPipeline{nullptr};
    QRhiShaderResourceBindings* m_ovSrb{nullptr};
    QRhiBuffer*                 m_ovVbo{nullptr};
    QRhiTexture*                m_ovGpuTex{nullptr};
    QRhiSampler*                m_ovSampler{nullptr};
    QImage m_overlayStatic;
    bool   m_overlayStaticDirty{true};
    bool   m_overlayNeedsUpload{true};

    // 2026-05-26 KG4VCF dual-layer overlay split.
    // ... static = chrome that changes on operator interaction;
    //     dynamic = per-frame peak hold / blobs / noise floor ...
    QRhiShaderResourceBindings* m_ovDynSrb{nullptr};
    QRhiTexture*                m_ovDynGpuTex{nullptr};
    QImage m_overlayDynamic;
    bool   m_overlayDynamicDirty{true};
    bool   m_overlayDynamicNeedsUpload{true};
    qint64 m_overlayDynamicDirtyMs{0};
```

**Notch markers are chrome, not per-frame data: they go in the STATIC layer,
and every notch mutator must call `markOverlayDirty()`** (spots get away with
bare `update()` only because they refresh on their own cadence; a notch drag
must invalidate the cached texture or the marker will not move on the GPU path).

### 6.3 `mousePressEvent` structure (`SpectrumWidget.cpp:5887-6194`)

Order of checks, top to bottom. New notch hit tests must be inserted with full
awareness of this order (spec §7.3 governs which wins):

1. `:5890-5895` disconnected guard: swallow all left clicks, emit
   `disconnectedClickRequest()`, return.
2. `:5897-5903` geometry locals:
   ```cpp
    int w = width();
    int h = height();
    int specH = specHFromHeight(h, m_spectrumFrac, kFreqScaleH + kDividerH);
    int dividerY = specH;
    QRect specRect(0, 0, w - effectiveStripW(), specH);
    int mx = static_cast<int>(event->position().x());
    int my = static_cast<int>(event->position().y());
   ```
3. `:5906-5914` double-click on off-screen indicator → `recenterOnVfo()`.
4. `:5916-6014` `Qt::RightButton`: first the spot-label hit test (walk
   `m_spotClickRects`, build a `QMenu`, `menu.exec(event->globalPosition().toPoint())`,
   `event->accept(); return;`), then the fallback `SpectrumOverlayMenu`.
5. `:6013-6016` non-left buttons fall through to `QWidget::mousePressEvent`.
6. `:6021-6046` left-button spot label hit test, then cluster-badge hit test.
7. `:6048-6089` dBm scale strip (right edge) ,  arrow row, then drag-pan.
8. `:6092-6098` divider bar, `kDividerGrab = 6`.
9. `:6101-6121` frequency scale bar (LIVE button first, then bandwidth drag).
10. `:6123-6146` filter-edge grab, `kFilterGrab` tolerance:
    ```cpp
    double loHz = m_vfoHz + m_filterLowHz;
    double hiHz = m_vfoHz + m_filterHighHz;
    int xLo = hzToX(loHz, specRect);
    int xHi = hzToX(hiHz, specRect);
    if (xLo > xHi) { std::swap(xLo, xHi); }
    bool loHit = std::abs(mx - xLo) <= kFilterGrab;
    bool hiHit = std::abs(mx - xHi) <= kFilterGrab;
    ```
11. `:6148-6156` inside passband → `m_draggingVfo = true`.
12. `:6158-6183` time-scale strip / LIVE button.
13. `:6185-6194` fallback pan drag: `m_draggingPan = true; m_panDragStartX = mx;
    m_panDragStartCenter = m_centerHz; setCursor(Qt::ClosedHandCursor);` then
    `QWidget::mousePressEvent(event);`.

Every accepted branch either `return`s bare or does `event->accept(); return;`.
Drag state is a set of `m_draggingX` bools cleared en masse in
`mouseReleaseEvent` (`:6568-6574`):

```cpp
        m_draggingDbm = false;
        m_draggingFilter = FilterEdge::None;
        m_draggingVfo = false;
        m_draggingDivider = false;
        m_draggingPan = false;
        m_draggingBandwidth = false;
        setCursor(Qt::CrossCursor);
```

Click-vs-drag discrimination lives in `mouseReleaseEvent` (`:6518-6521`), a
**4 px Manhattan threshold on x** ported from AetherSDR `SpectrumWidget.cpp:1427-1457`:

```cpp
        if (m_draggingPan) {
            int dx = std::abs(static_cast<int>(event->position().x()) - m_panDragStartX);
            if (dx <= 4) {
                // ... xToHz + snap to m_stepHz, then emit frequencyClicked(hz)
```

`mouseMoveEvent` (`:6196-...`) opens with
`m_mousePos = event->pos(); m_mouseInWidget = true;`, recomputes the same
geometry locals, then dispatches on the drag flags in the same priority order,
each branch ending `return;`. `hzPerPx` inside a drag is computed as
`m_bandwidthHz / specRect.width()`.

### 6.4 Frequency <-> pixel helpers, VERBATIM

Declared private (`SpectrumWidget.h:1347-1351`):

```cpp
    // ---- Coordinate helpers ----
    int    hzToX(double hz, const QRect& r) const;
    double xToHz(int x, const QRect& r) const;
    int    dbmToY(float dbm, const QRect& r) const;
    float  dbmToYf(float dbm, const QRect& r) const; // sub-pixel variant for antialiased trace
```

Defined (`SpectrumWidget.cpp:4025-4036`):

```cpp
int SpectrumWidget::hzToX(double hz, const QRect& r) const
{
    double lowHz = m_centerHz - m_bandwidthHz / 2.0;
    double frac = (hz - lowHz) / m_bandwidthHz;
    return r.left() + static_cast<int>(frac * r.width());
}

double SpectrumWidget::xToHz(int x, const QRect& r) const
{
    double frac = static_cast<double>(x - r.left()) / r.width();
    return (m_centerHz - m_bandwidthHz / 2.0) + frac * m_bandwidthHz;
}
```

Both take the spectrum rect explicitly; callers build
`QRect specRect(0, 0, w - effectiveStripW(), specH)`. Neither is clamped, so a
hit test must range-check itself.

### 6.5 Signals available to a notch UI (`SpectrumWidget.h`)

```cpp
:1130    void spectrumFrameRendered();
:1134    void disconnectedClickRequest();
:1137    void frequencyClicked(double hz);
:1143    void spotTriggered(int spotIndex);
:1149    void spotRemoveRequested(int spotIndex);
:1155    void spotHoverIndexChanged(int spotIndex);
:1158    void filterEdgeDragged(int lowHz, int highHz);
:1160    void centerChanged(double centerHz);
:1162    void bandwidthChangeRequested(double newBandwidthHz);
:1169    void widebandExtensionStateChanged(bool extensionRequested);
:1177    void ddcRetuneRequested(double frequencyHz);
:1183    void dbmRangeChangeRequested(float minDbm, float maxDbm);
:1186    void ctunEnabledChanged(bool enabled);
```

New notch signals follow the same shape: plain values, Hz as `double`, index as
`int`.

---

## 7. Test conventions

### 7.1 A complete small test, VERBATIM (`tests/tst_slice_model_phase3f_properties.cpp`, first 51 lines + tail)

```cpp
// =================================================================
// tests/tst_slice_model_phase3f_properties.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Tasks 4-11: verify SliceModel gains 7 new
// Q_PROPERTYs per
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §3.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceModelPhase3FProperties : public QObject {
    Q_OBJECT

private slots:
    // ── Task 4: sliceLetter ──────────────────────────────────────────────
    void slice_letter_default_is_A()
    {
        SliceModel slice;
        QCOMPARE(slice.sliceLetter(), QChar('A'));
    }

    void slice_letter_setter_round_trips()
    {
        SliceModel slice;
        slice.setSliceLetter(QChar('C'));
        QCOMPARE(slice.sliceLetter(), QChar('C'));
    }

    void slice_letter_setter_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::sliceLetterChanged);
        slice.setSliceLetter(QChar('B'));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toChar(), QChar('B'));
    }

    void slice_letter_setter_idempotent()
    {
        SliceModel slice;
        slice.setSliceLetter(QChar('B'));
        QSignalSpy spy(&slice, &SliceModel::sliceLetterChanged);
        slice.setSliceLetter(QChar('B'));  // same value
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestSliceModelPhase3FProperties)
#include "tst_slice_model_phase3f_properties.moc"
```

Hard requirements visible here:
- `// no-port-check:` marker on line 4 for NereusSDR-original tests (this is
  what keeps `scripts/check-new-ports.py` quiet).
- Header banner naming the file and the design-doc section.
- Includes are **repo-root-relative without `src/`**: `"models/SliceModel.h"`,
  `"core/RxChannel.h"`, `"gui/SpectrumWidget.h"`. This is also what drives the
  automatic ctest label derivation (`tests/CMakeLists.txt:65-89` matches
  `"(\\.\\./)*(src/)?core/"` etc.), so include paths are load-bearing.
- `using namespace NereusSDR;` after includes.
- Test class at **global scope** (required for the `friend class ::TestX;`
  seam), named `TestPascalCase`, `: public QObject`, `Q_OBJECT`,
  `private slots:`.
- Slot names are `snake_case_sentences`.
- `QTEST_MAIN(...)` then `#include "<file>.moc"`.
- Four-test shape per property: default / round-trip / emits / idempotent.

### 7.2 CMake registration (`tests/CMakeLists.txt`)

Registration is a single call preceded by a comment block that names the phase,
what is verified, and the source authority:

```cmake
# Phase 3F Sub-Epic A Tasks 4-6: SliceModel new Q_PROPERTYs
# Verifies three new Q_PROPERTYs added for multi-panadapter/multi-slice support:
#   Task 4: sliceLetter (QChar, default 'A') - per-slice letter ID for badge color
#   Task 5: chainIndex  (int,   default 0)   - which Alex chain (ADC) hosts slice DDC
#   Task 6: ddcIndex    (int,   default -1)  - codec-assigned DDC index; -1=unassigned
# Source: NereusSDR-original (no Thetis upstream).
# Design: docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §3.
nereus_add_test(tst_slice_model_phase3f_properties)
```

What `nereus_add_test(name)` does (`tests/CMakeLists.txt:114-203`):

```cmake
    add_executable(${name} EXCLUDE_FROM_ALL ${name}.cpp $<TARGET_OBJECTS:nereus_test_sandbox> ${ARGN})
    target_link_libraries(${name} PRIVATE NereusSDRObjs Qt6::Test)
    set_property(GLOBAL APPEND PROPERTY NEREUS_ALL_TESTS ${name})
    if(NEREUS_USE_PCH)
        target_precompile_headers(${name} REUSE_FROM NereusSDRObjs)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    _nereus_derive_test_labels(_test_labels "${name}.cpp")
    set_tests_properties(${name} PROPERTIES
        LABELS "${_test_labels}"
        TIMEOUT 120)
```

Extra source files go as `ARGN`: `nereus_add_test(tst_foo Fake.cpp)`.
Per-test defines are added after the call, e.g.
`target_compile_definitions(tst_p1_wire_format PRIVATE NEREUS_BUILD_TESTS)`.
Tests are `EXCLUDE_FROM_ALL`; aggregates are `all_tests` and `tests_<label>`.

### 7.3 How a test gets a `RadioModel`

Plain stack construction, no fixture, no MainWindow
(`tests/tst_stream_pool_binding.cpp:70-88`):

```cpp
class TestStreamPoolBinding : public QObject {
    Q_OBJECT
private slots:
    void pool_sizes_to_the_sku()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        QCOMPARE(model.streamPoolSize(), 5);
    }

    void first_slice_activates_stream_zero()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int idx = model.addSlice();
        SliceModel* s = model.slices().at(idx);

        QCOMPARE(s->streamIndex(), 0);
        QCOMPARE(s->shiftOffsetHz(), 0.0);
    }
```

`RadioModel model;` on the stack; `configureStreamPool` then `addSlice()`. Note
`addSlice()` returns a slice **id**, and both `slices().at(id)` and
`sliceById(id)` appear in this file; prefer `sliceById` for new code.

**Caller-side value assertion seam** (spec §4.6 option 1) at
`tst_stream_pool_binding.cpp:576-584`:

```cpp
        // The invariant that actually matters: whatever the VFO reads, the
        // DSP must be demodulating it. Reconstruct the demodulated frequency
        // from the binding the way WDSP does -- stream centre plus shift --
        // and require it to equal the frequency on the flag.
        QCOMPARE(slice->streamIndex(), boundStream);
        const double demodulatedHz =
            model.streamCentreHzForTest(slice->streamIndex())
            + slice->shiftOffsetHz();
        QCOMPARE(slice->frequency(), demodulatedHz);
```

### 7.4 How a test gets a live `WdspEngine` (the friend seam)

Two edits are required, both in `src/core/WdspEngine.h`:

1. Global-scope forward declaration inside the existing
   `#ifdef NEREUS_BUILD_TESTS` block near the top (`WdspEngine.h:105-124`):

```cpp
// wrapper ,  friend declarations need the fully-qualified name.
class TestWdspEngineTxChannel;
// ...
// Phase 3F Sub-Epic I closeout, defect H1: the per-stream drain-geometry
// test primes the engine so createRxChannel can seed real RX channels.
class TestStreamPoolBinding;
// Phase 3F: the channel-id map test primes the engine so it can watch
// which ids openRxChannelPool actually opens.
class TestWdspChannelIdMap;
#endif
```

2. The friend line inside the class (`WdspEngine.h:674-694`):

```cpp
#ifdef NEREUS_BUILD_TESTS
    // Test-only friend: lets unit tests bypass async wisdom load by setting
    // m_initialized = true directly so they can exercise createTxChannel /
    // createRxChannel without a running event loop or a real WDSP wisdom
    // file.  Production builds (without NEREUS_BUILD_TESTS) never see this.
    friend class ::TestWdspEngineTxChannel;
    friend class ::TstWdspEngineDexpInit;
    friend class ::TstPsFeedbackChannel;
    friend class ::TestSliceModelRadeSwap;
    friend class ::TestRadeApplet;
    friend class ::TestStreamPoolBinding;
    friend class ::TestWdspChannelIdMap;
#endif
};
```

Use at `tst_stream_pool_binding.cpp:997-1017`:

```cpp
    void single_slice_leaves_only_slice_as_channel_live()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        QVERIFY(model.sliceById(a)->streamIndex() >= 0);

        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        QVERIFY(engine->rxChannel(a) != nullptr);
        QVERIFY(engine->rxChannel(a)->isActive());
        for (int ch = 1; ch < 5; ++ch) {
            QVERIFY2(!engine->rxChannel(ch)->isActive(),
                     qPrintable(QStringLiteral("unbound pool channel %1 is running")
                                    .arg(ch)));
        }
    }
```

The standalone-engine variant (`tst_ps_feedback_channel.cpp:70-78`):

```cpp
    void hasUniqueChannelId() {
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
                                        // bypasses async wisdom path
        engine.openPsFeedbackChannelForTesting();
        TxChannel* tx = engine.createTxChannel(kTxChannelIdForTest);
```

Relevant `WdspEngine` API for a real opened channel (`WdspEngine.h:208-238`):

```cpp
    static constexpr int kMaxSliceChannels = 5;
    static constexpr int kFirstSliceChannelId = 0;
    static constexpr int kTxChannelId = kMaxSliceChannels;
    static constexpr int kPsFeedbackChannelId = kTxChannelId + 1;   // :426

    RxChannel* createRxChannel(int channelId,
                               int inputBufferSize = 238,
                               int dspBufferSize = 4096,
                               int inputSampleRate = 48000,
                               int dspSampleRate = 48000,
                               int outputSampleRate = 48000);
    void destroyRxChannel(int channelId);
    RxChannel* rxChannel(int channelId) const;
```

**Hazard restated from spec §4.6**: test binaries compile with live WDSP
(`HAVE_WDSP` is `PUBLIC` on `NereusSDRObjs`, `CMakeLists.txt:1124`), and
`rxa` is sized `MAX_CHANNELS = 32`, so the existing `kTestChannel = 99`
pattern used by the RxChannel tests is an out-of-bounds read for any WDSP call
that dereferences before comparing. `RXANBPSetTuneFrequency` dereferences at
`nbp.c:479` (`if (tunefreq != a->tunefreq)`), so the "pass the same value so
the C++ early return fires" hatch does not protect it. Use one of the two
seams above, not `kTestChannel = 99`.

---

## 8. Settings pages

### 8.1 The stub to fill in, VERBATIM (`src/gui/setup/DspSetupPages.cpp:2103-2128`)

```cpp
// ══════════════════════════════════════════════════════════════════════════════
// MnfSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// From Thetis setup.cs ,  tabDSP / tabPageMNF controls:
//   chkMNFAutoIncrease, comboMNFWindow, lstNotches (display)
//
MnfSetupPage::MnfSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("MNF", model, parent)
{
    // ── Manual Notch ──────────────────────────────────────────────────────────
    QGroupBox* mnfGrp = addSection("Manual Notch");
    QVBoxLayout* mnfLay = qobject_cast<QVBoxLayout*>(mnfGrp->layout());

    auto* autoIncrease = new QPushButton("Enable");
    addLabeledToggle(mnfLay, "Auto-Increase", autoIncrease);

    auto* windowCombo = new QComboBox;
    windowCombo->addItems({"Blackman-Harris", "Hann", "Flat-Top"});
    addLabeledCombo(mnfLay, "Window", windowCombo);

    auto* notchList = new QLabel("(no notches)");
    addLabeledLabel(mnfLay, "Notch List", notchList);

    disableGroup(mnfGrp);
}
```

Header declaration (`src/gui/setup/DspSetupPages.h:245-252`):

```cpp
// ── MNF ──────────────────────────────────────────────────────────────────────

class MnfSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit MnfSetupPage(RadioModel* model, QWidget* parent = nullptr);
};
```

The NYI guard it must lose (`DspSetupPages.cpp:93-99`):

```cpp
// ─────────────────────────────────────────────────────────────────────────────
// Helper: disable every child widget inside a group box (NYI guard).
// ─────────────────────────────────────────────────────────────────────────────
static void disableGroup(QGroupBox* grp)
{
    grp->setEnabled(false);
}
```

### 8.2 Its `registerPage` call, VERBATIM (`src/gui/SetupDialog.cpp:609`)

```cpp
    registerPage(dsp, "MNF", [this] { return new MnfSetupPage(m_model); });
```

Context (`SetupDialog.cpp:586-621`): `QTreeWidgetItem* dsp = addCategory("DSP");`
then one `registerPage` per leaf, then `tick("DSP");`. The variant used when
the page has signals to wire up:

```cpp
    registerPage(dsp, "CFC", [this]() -> QWidget* {
        auto* cfcPage = new CfcSetupPage(m_model);
        connect(cfcPage, &CfcSetupPage::openCfcDialogRequested,
                this,    &SetupDialog::cfcDialogRequested);
        return cfcPage;
    });
```

`registerPage` itself (`SetupDialog.h:129-130`, `SetupDialog.cpp:307-315`) is
lazy: it records a factory, and `realizePage` builds it on first navigation.
**Do not do work in the page ctor that must run at app start.**

```cpp
QTreeWidgetItem* SetupDialog::registerPage(QTreeWidgetItem* parent,
                                          const QString& label,
                                          std::function<QWidget*()> factory)
{
    auto* item = new QTreeWidgetItem(parent, QStringList{label});
    item->setData(0, Qt::UserRole, static_cast<int>(m_pages.size()));
    m_pages.push_back(PageEntry{label, std::move(factory), nullptr, -1});
    return item;
}
```

### 8.3 `SetupPage` base API (`src/gui/SetupPage.h:26-93`)

```cpp
class SetupPage : public QWidget {
    Q_OBJECT
public:
    explicit SetupPage(const QString& title, RadioModel* model, QWidget* parent = nullptr);
    explicit SetupPage(const QString& title, QWidget* parent = nullptr);
    virtual ~SetupPage() = default;

    QString pageTitle() const { return m_title; }
    virtual void syncFromModel();
    static void markNyi(QWidget* widget, const QString& phase);

    QGroupBox* addSection(const QString& title);

    QPushButton* addLabeledToggle(const QString& label);
    QComboBox*   addLabeledCombo(const QString& label, const QStringList& items);
    QSlider*     addLabeledSlider(const QString& label, int minimum, int maximum, int value);
    QSpinBox*    addLabeledSpinner(const QString& label, int minimum, int maximum, int value);
    QPushButton* addLabeledButton(const QString& label, const QString& buttonText);
    QLabel*      addLabeledLabel(const QString& label, const QString& value);
    QLineEdit*   addLabeledEdit(const QString& label, const QString& placeholder = {});

protected:
    QVBoxLayout* contentLayout() { return m_contentLayout; }
    RadioModel*  model()         { return m_model; }

    // ── Low-level helpers (for subclasses that build complex layouts) ─────────
    QHBoxLayout* addLabeledCombo(QLayout* parent, const QString& label, QComboBox* combo);
    QHBoxLayout* addLabeledSlider(QLayout* parent, const QString& label, QSlider* slider,
                                   QLabel* valueLabel = nullptr);
    QHBoxLayout* addLabeledToggle(QLayout* parent, const QString& label, QPushButton* toggle);
    QHBoxLayout* addLabeledSpinner(QLayout* parent, const QString& label, QSpinBox* spinner);
    QHBoxLayout* addLabeledEdit(QLayout* parent, const QString& label, QLineEdit* edit);
    QHBoxLayout* addLabeledLabel(QLayout* parent, const QString& label, QLabel* value);
};
```

A table (spec §9 requires one) has no helper: add it directly to
`qobject_cast<QVBoxLayout*>(grp->layout())` or to `contentLayout()`.

### 8.4 A fully-implemented page for the shape: `CfcSetupPage` (`DspSetupPages.cpp:1869-2101`)

Opening + null-model guard + a group with two-way model binding, VERBATIM
(`:1869-1960`):

```cpp
CfcSetupPage::CfcSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("CFC", model, parent)
{
    if (!model) {
        // No model ,  show disabled placeholder (consistent with AgcAlcSetupPage).
        QGroupBox* grp = addSection("Phase Rotator");
        disableGroup(grp);
        return;
    }

    TransmitModel& tx = model->transmitModel();

    // ══════════════════════════════════════════════════════════════════════════
    // ── Group 1: Phase Rotator ────────────────────────────────────────────────
    // ══════════════════════════════════════════════════════════════════════════
    //
    // From Thetis setup.Designer.cs:46162-46280 [v2.10.3.13] ,  grpPhRot.
    // Four controls: chkPHROTEnable / udPhRotFreq / udPHROTStages /
    // chkPHROTReverse.  Tooltip strings copied verbatim from designer file.
    //
    QGroupBox* phRotGrp = addSection("Phase Rotator");
    QVBoxLayout* phRotLay = qobject_cast<QVBoxLayout*>(phRotGrp->layout());

    m_phRotEnableChk = new QCheckBox("Enable");
    m_phRotEnableChk->setObjectName(QStringLiteral("chkPHROTEnable"));
    m_phRotEnableChk->setChecked(tx.phaseRotatorEnabled());
    // From Thetis setup.Designer.cs:46281 [v2.10.3.13] ,  chkPHROTEnable tooltip.
    m_phRotEnableChk->setToolTip(QStringLiteral("Turn the phase rotator on or off"));
    phRotLay->addWidget(m_phRotEnableChk);

    m_phRotFreqSpin = new QSpinBox;
    m_phRotFreqSpin->setObjectName(QStringLiteral("udPhRotFreq"));
    m_phRotFreqSpin->setRange(TransmitModel::kPhaseRotatorFreqHzMin,
                              TransmitModel::kPhaseRotatorFreqHzMax);
    m_phRotFreqSpin->setSuffix(" Hz");
    m_phRotFreqSpin->setValue(tx.phaseRotatorFreqHz());
    // From Thetis setup.Designer.cs:46264 [v2.10.3.13] ,  udPhRotFreq tooltip.
    m_phRotFreqSpin->setToolTip(
        QStringLiteral("Set rotation frequency in Hz. (default 338)"));
    addLabeledSpinner(phRotLay, "FREQ", m_phRotFreqSpin);

    // ── Wiring: PhRot widgets ↔ TransmitModel ────────────────────────────────
    connect(m_phRotEnableChk, &QCheckBox::toggled,
            &tx, &TransmitModel::setPhaseRotatorEnabled);
    connect(&tx, &TransmitModel::phaseRotatorEnabledChanged,
            m_phRotEnableChk, [this](bool on) {
        QSignalBlocker b(m_phRotEnableChk);
        m_phRotEnableChk->setChecked(on);
    });

    connect(m_phRotFreqSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            &tx, &TransmitModel::setPhaseRotatorFreqHz);
    connect(&tx, &TransmitModel::phaseRotatorFreqHzChanged,
            m_phRotFreqSpin, [this](int hz) {
        QSignalBlocker b(m_phRotFreqSpin);
        m_phRotFreqSpin->setValue(hz);
    });
```

The rules this encodes, all mandatory for the MNF page:
- `if (!model) { addSection(...); disableGroup(grp); return; }` guard first.
- Widgets are raw `new` with **Qt parent ownership via layout insertion** (the
  "no raw new" rule is satisfied by Qt ownership; this is the house exception
  and every page does it).
- `setObjectName(QStringLiteral("<thetisControlName>"))` on every control, using
  the literal Thetis designer name. For MNF that means `chkMNFAutoIncrease`,
  `chkMNFActive`, `udMNFFreq`, `udMNFWidth`, `udMNFNotch`, `chkVisualNotch`,
  `btnMNFAdd` / `Edit` / `Delete` / `Enter` / `Cancel`, per spec §9
  (`setup.designer.cs:44145-44159`).
- Ranges come from named `static constexpr` model constants
  (`TransmitModel::kPhaseRotatorFreqHzMin`), not literals.
- Initial value read from the model in the ctor.
- Tooltips verbatim from Thetis, with the cite in a `//` comment on the line
  ABOVE, never inside the user-visible string.
- Two-way binding is a pair of `connect`s; the model-to-widget direction always
  wraps in `QSignalBlocker b(m_widget);` to break the echo loop.
- `QOverload<int>::of(&QSpinBox::valueChanged)` for the spin box overload.
- Page-owned widget pointers are `m_camelCase{nullptr}` members declared in
  `DspSetupPages.h` grouped by group box with `// ── Group ──` comments.

File includes for `DspSetupPages.cpp` (`:64-87`) already cover
`core/AppSettings.h`, `core/wdsp_api.h`, `core/WdspEngine.h`,
`models/RadioModel.h`, `models/SliceModel.h`, `QCheckBox`, `QComboBox`,
`QSignalBlocker`, `QSpinBox`, `QTabWidget`, `QVBoxLayout`. A table adds
`QTableWidget` / `QTableView`.

### 8.5 The third stub to retire (`src/gui/SpectrumOverlayPanel.cpp:272-278`)

```cpp
    // Button 9: MNF (NYI)
    {
        auto* btn = makeDisabledBtn("MNF", this);
        btn->setToolTip("Manual notch filter (NYI)");
        m_menuBtns.append(btn);  // index 7
    }
```

Spec §9 says replace this rather than shipping both, and that the overlay
button reads `+TNF`, not `+MNF`.

---

## 9. Build and test commands (`docs/development/fast-test-loop.md`)

Configure (from `CLAUDE.md`; tests are opt-in and OFF by default,
`CMakeLists.txt:1450`):

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON
```

Build the app:

```bash
cmake --build build
```

Build and run ONE test (the everyday TDD loop):

```bash
cmake --build build --target tst_slice_auto_agc && ctest --test-dir build -R '^tst_slice_auto_agc$' --output-on-failure
```

Run one subsystem. **Always build the matching `tests_<label>` target first** , 
test executables are `EXCLUDE_FROM_ALL`, so a bare `ctest -L core` either
reports "Not Run" on a clean tree or silently runs stale binaries and returns a
false green:

```bash
cmake --build build --target tests_core && ctest --test-dir build -L core
```

Targets:

```bash
cmake --build build --target tst_slice_auto_agc   # one test
cmake --build build --target tests_core           # one subsystem
cmake --build build --target all_tests            # the lot (~32 min cold)
```

Labels are derived automatically from each test's own `#include` lines
(`core` / `models` / `gui` / `unclassified`); there is one `tests_<label>`
target per label. Every test carries `TIMEOUT 120`.

Suite facts: 513 registered tests, ~38 CPU-s to link each, ~32 min to build
all, ~5 min to run cold. Labels narrow the run, not the dependency: 85 tests
carry no `core` label but still statically link all of `NereusSDRObjs`.

macOS: exempt the terminal under **System Settings → Privacy & Security →
Developer Tools** or first-run malware scanning adds ~5 min to a cold suite.

ccache (auto-wired when on `PATH`) needs one non-default setting because of the
shared PCH:

```bash
ccache --set-config sloppiness=pch_defines,time_macros
```

Test writing: **never `QTest::qWait` for a state change**; use
`QTRY_VERIFY_WITH_TIMEOUT(cond, ms)` plus a `QSignalSpy` count, and add a
narrow test-only seam rather than sleeping.

Settings sandbox: `tests/TestSandboxInit.cpp` runs a file-scope static ctor
before `main()` calling `QStandardPaths::setTestModeEnabled(true)`. The real
settings file is `~/Library/Preferences/NereusSDR/NereusSDR.settings` on macOS
and `~/.config/NereusSDR/NereusSDR.settings` on Linux.

---

## 10. Cross-cutting conventions

- Logging categories (`src/core/LogCategories.h:12-23`): `lcDiscovery`,
  `lcConnection`, `lcProtocol`, `lcReceiver`, `lcAudio`, `lcDsp`, `lcSpectrum`,
  `lcContainer`, `lcMeter`, `lcMmio`, `lcTci`, `lcSpots`. Notch work uses
  `lcDsp` (DSP/model side) and `lcSpectrum` (widget side). Log with
  `qCWarning(lcDsp) << ...`; never throw.
- Inline cite grammar in force this session:
  `// From Thetis <file>:<line> [v2.10.3.15]` and
  `// From AetherSDR <file>:<line> [@c6481cbf]`. Both bracket forms appear
  in-tree; older files carry `[v2.10.3.13]`, do not "fix" those.
- No source cites inside user-visible strings (tooltips, status bar, button
  captions). The cite goes in a `//` comment above the string.
- `no-port-check:` on line 3-4 of a NereusSDR-original file's banner is what
  keeps `scripts/check-new-ports.py` from flagging it
  (`SliceStreamAllocator.h:5`, every `tst_*.cpp`).
- New files with ported logic need the verbatim upstream header AND a
  `docs/attribution/THETIS-PROVENANCE.md` row in the same commit. Spec §10.1
  declares `src/models/NotchModel.{h,cpp}` a new attribution event over three
  upstream files.

---

## 11. Gaps / things the spec leaves for the plan author

Flagging these rather than inventing answers:

1. **`Notch` struct location and shape are not fully specified.** Spec §6.2
   uses `const QList<Notch>&` in `RxChannel::syncNotches` and §5.1 puts the
   data on `NotchModel`, but does not say whether `Notch` is nested in
   `NotchModel` or free in the `NereusSDR` namespace, nor its field names /
   types. The plan author must pick and state it; the house precedent
   (`RxChannel::Nr1Tuning`, `SpectrumWidget::SpotMarker`) is a nested public
   struct with brace-initialised defaults owned by the class that produces it,
   which would put `Notch` in `NotchModel` and have `RxChannel` include
   `models/NotchModel.h`. That is a `core` -> `models` include direction that
   `RxChannel.h` does not currently have (it includes only `core/` and
   `core/dsp/` headers), so it needs a deliberate decision.

2. **`SpectrumWidget` has no existing notch draw / hit-test member**, and spec
   §8.1/§8.2 describe the push API and rendering but I did not read those
   sections in full for this contract. Whatever they specify must land as
   `drawNotchMarkers(QPainter&, const QRect&)` private + a `ForTest` forwarder,
   inserted before `drawSpotMarkers` in BOTH the CPU path
   (`SpectrumWidget.cpp:3043`) and the GPU static-overlay rebuild
   (`SpectrumWidget.cpp:7143`). Missing the second call site is a silent
   GPU-only regression, since `NEREUS_GPU_SPECTRUM` is the shipping path.

3. **`RXANBPGetMinNotchWidth` has no obvious refresh trigger in-tree.** It
   varies with `nc` and sample rate (spec §9). `RxChannel` already tracks
   `m_filterSize` / `m_dspBlockSize` and has `setFilterSizeSamples` /
   `setSampleRate`, but no signal fires on those today that a Settings page
   could subscribe to for a live readout. The plan author needs to either poll
   it on page show or add a `minNotchWidthChanged(double)` signal.


---

# Tasks

### Task 1: The gating fix: RXANBPSetTuneFrequency and three sibling defects

**Files:**
- Create: `tests/tst_notch_tune_frequency.cpp`
- Modify: `tests/CMakeLists.txt:1621` (register the new test after `nereus_add_test(tst_stream_pool_binding)`)
- Modify: `src/core/wdsp_api.h:130-136` (modification-history entry) and `src/core/wdsp_api.h:353-357` (new `extern "C"` declaration)
- Modify: `src/core/RxChannel.h:615-617` (accessors) and `src/core/RxChannel.h:936-938` (carry members)
- Modify: `src/core/RxChannel.cpp:1464-1489` (`setShiftFrequency` restructure + new `setNotchTuneFrequency`)
- Modify: `src/core/WdspEngine.h:121-124` (test-class forward decl) and `src/core/WdspEngine.h:691-694` (friend line)
- Modify: `src/models/RadioModel.h:2256-2258` (private helpers) and `src/models/RadioModel.h:2483-2485` (ForTest seam)
- Modify: `src/models/RadioModel.cpp:3140-3145` (`activateSliceChannel`), `:3671-3679` (`bindSliceToStream`), `:8955-8988` (`updateShiftFrequency` lambda), `:9201-9215` (connect-time seed), plus two new methods
- Test: `tests/tst_notch_tune_frequency.cpp`

**Interfaces:**
- Consumes: nothing. This is Task 1 of 11 and it is the premise of the rest.
- Produces:
  - `void RXANBPSetTuneFrequency(int channel, double tunefreq);` (`src/core/wdsp_api.h`, inside `extern "C"`, guarded by `HAVE_WDSP`)
  - `void RxChannel::setNotchTuneFrequency(double absoluteHz);`
  - `double RxChannel::notchTuneFrequencyHz() const;`
  - `double RxChannel::shiftOffsetHz() const;`
  - `double RxChannel::notchShiftHz() const;`
  - `double RadioModel::composedShiftHz(const SliceModel* slice) const;` (private)
  - `void RadioModel::seedConnectFrequency(SliceModel* slice);` (private)
  - `void RadioModel::seedConnectFrequencyForTest(SliceModel* slice);` (public test seam)
  - `friend class ::TestNotchTuneFrequency;` on `WdspEngine`

> **Plan comment on a spec divergence.** §4.4 says there are "**two** writers" of the shift
> (`RadioModel.cpp:3676` and `:8982`). There are **three**: `activateSliceChannel`
> (`RadioModel.cpp:3144`) pushes `slice->shiftOffsetHz()` too, and it runs at the **tail** of
> `bindSliceToStream` (`:3705`) on a first bind, i.e. immediately **after** the `:3676` push,
> so leaving it alone would discard the composed sum on every first bind. All three writers
> therefore go through `composedShiftHz`. Verified by reading `RadioModel.cpp:3111-3147`:
> the `if (!ch || ch->isActive()) { return; }` early return only makes it dead for an
> *already-live* channel, not for the first bind.

---

- [ ] **Step 1: Write the failing test (first three slots: the RxChannel carries, §4.6)**

Create `tests/tst_notch_tune_frequency.cpp`:

```cpp
// =================================================================
// tests/tst_notch_tune_frequency.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Tunable Notch Filter, Task 1. Covers
// docs/architecture/2026-07-28-tunable-notch-filter-design.md §4 in full:
//   §4.1/§4.2  the notch database's tune frequency is the hosting DDC
//              stream's CENTRE, pushed from bindSliceToStream.
//   §4.3       the shift reaches the notch database on the way back down
//              to zero, and keeps its sign.
//   §4.4       the stream offset and RIT/DIG compose instead of clobbering
//              each other, asserted in both writer orders.
//   §4.5       the connect-time seed commands the stream centre, not the
//              slice frequency.
//
// The invariant every slot below is really asserting, from §4.1:
//     tunefreq + shift == the slice's demodulated RF
//                      == stream centre + slice offset + RIT + DIG
// asserted in BOTH halves, never the sum alone: a sum-only assertion
// passes under the double-count bug whenever the shift happens to be zero.
//
// WDSP is live in test binaries (CMakeLists.txt:1124 sets HAVE_WDSP PUBLIC)
// and RXANBPSetTuneFrequency dereferences rxa[channel].ndb.p before it
// compares (third_party/wdsp/src/nbp.c:477-479), so the usual
// kTestChannel = 99 never-opened-channel hatch is an out-of-bounds read
// here, not a no-op. Every slot uses a really opened channel through the
// NEREUS_BUILD_TESTS friend seam, the pattern at
// tests/tst_ps_feedback_channel.cpp:72,78 and
// tests/tst_stream_pool_binding.cpp:997-1017.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/P1RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Stream geometry shared by every slot. 192 kHz gives a +/- 96 kHz window
// (SliceStreamAllocator::windowContains), so 14.200 and 14.210 MHz share a
// stream and 14.200 / 14.400 MHz do not.
constexpr int    kRateHz       = 192000;
constexpr double kSliceAFreqHz = 14200000.0;
constexpr double kSliceBFreqHz = 14210000.0;
constexpr double kFarFreqHz    = 14400000.0;

// Detaches a stack-injected RadioConnection on scope exit, however the
// scope is left. From tests/tst_stream_pool_binding.cpp:60-63: QCOMPARE
// returns from the enclosing slot on failure, so a trailing
// injectConnectionForTest(nullptr) is skipped on exactly the run where it
// matters most.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

} // namespace

class TestNotchTuneFrequency : public QObject {
    Q_OBJECT

private slots:
    // ── §4.6: the RxChannel carries ──────────────────────────────────────

    void notch_tune_frequency_defaults_to_zero()
    {
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        // The construction default, and the whole defect: nothing in the
        // tree pushed this value, so every channel's notchdb.tunefreq sat
        // here and calc_nbp_lightweight mapped notches from the wrong RF
        // origin (third_party/wdsp/src/nbp.c:192).
        QCOMPARE(ch->notchTuneFrequencyHz(), 0.0);
    }

    void notch_tune_frequency_carry_round_trips()
    {
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        ch->setNotchTuneFrequency(kSliceAFreqHz);
        QCOMPARE(ch->notchTuneFrequencyHz(), kSliceAFreqHz);
    }

    void shift_push_keeps_its_sign()
    {
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        // A slice 10 kHz ABOVE its stream centre pushes +10000, not -10000.
        // Thetis's -value at radio.cs:1419-1420 [v2.10.3.15] is not a
        // divergence: rx_osc is already the negated quantity upstream
        // (console.cs:31916-31922), so Thetis's -rx_osc equals the offsetHz
        // handed in here, which equals frequencyHz - centreHz at
        // SliceStreamAllocator.cpp:70. Locked here so it never gets
        // "corrected" into an inversion of every shifted slice.
        ch->setShiftFrequency(10000.0);
        QCOMPARE(ch->shiftOffsetHz(), 10000.0);
    }
};

QTEST_MAIN(TestNotchTuneFrequency)
#include "tst_notch_tune_frequency.moc"
```

- [ ] **Step 2: Register the test**

In `tests/CMakeLists.txt`, immediately after `nereus_add_test(tst_stream_pool_binding)` (line 1621):

```cmake
# ── Tunable Notch Filter Task 1: the notch DB's tune frequency ─────────────
# WDSP maps absolute-Hz notch centres into a channel's passband with
# offset = tunefreq + shift (third_party/wdsp/src/nbp.c:192). tunefreq had
# never been pushed by anything in this tree, so every channel's
# notchdb.tunefreq sat at its construction default and the whole notch
# feature was unimplementable on top of it. Pins all four halves of design
# doc §4: the pushed value is the hosting STREAM's centre and not the slice
# frequency (§4.1/§4.2, asserted for both the sole-owner and JoinedExisting
# outcomes); the shift reaches the notch database on the way back to zero
# and keeps its sign (§4.3); the stream offset and RIT/DIG compose instead
# of clobbering each other in either writer order (§4.4); and the
# connect-time DDC seed commands the stream centre (§4.5).
# Source: Thetis console.cs:31940-31941 + radio.cs:1419-1420 [v2.10.3.15].
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §4.
nereus_add_test(tst_notch_tune_frequency)
```

- [ ] **Step 3: Open the WdspEngine friend seam for the new test class**

In `src/core/WdspEngine.h`, append to the `#ifdef NEREUS_BUILD_TESTS` forward-declaration block (after `class TestWdspChannelIdMap;`, line 124):

```cpp
// TNF Task 1: the notch tune-frequency test primes the engine so
// createRxChannel opens real RX channels. RXANBPSetTuneFrequency
// dereferences rxa[channel].ndb.p before it compares (nbp.c:477-479), so
// the kTestChannel = 99 never-opened-channel hatch is unavailable here.
class TestNotchTuneFrequency;
```

and to the friend block inside the class (after `friend class ::TestWdspChannelIdMap;`, line 694):

```cpp
    // TNF Task 1: same friendship for the notch tune-frequency test, which
    // needs really opened RX channels rather than an unopened slot.
    friend class ::TestNotchTuneFrequency;
```

- [ ] **Step 4: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_tune_frequency && ctest --test-dir build -R '^tst_notch_tune_frequency$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'notchTuneFrequencyHz' in 'NereusSDR::RxChannel'`
(and the same for `setNotchTuneFrequency` and `shiftOffsetHz`).

- [ ] **Step 5: Declare `RXANBPSetTuneFrequency` in `wdsp_api.h`**

Append to the modification-history block, immediately before its
`// =================================================================`
terminator at `src/core/wdsp_api.h:136`:

```cpp
//   2026-07-29 ,  RXANBPSetTuneFrequency declaration added by J.J. Boyd
//                 (KG4VCF) during the Tunable Notch Filter epic, Task 1
//                 (docs/architecture/2026-07-28-tunable-notch-filter-
//                 design.md §4). Nothing in this tree had ever called it,
//                 so every RXA channel's notchdb.tunefreq sat at its
//                 construction default and calc_nbp_lightweight mapped
//                 notch centres from the wrong RF origin
//                 (offset = b->tunefreq + b->shift, wdsp/nbp.c:192).
//                 Signature matches Thetis Console/dsp.cs:718-719
//                 [v2.10.3.15] P/Invoke decl and wdsp/nbp.c:475.
//                 AI-assisted transformation via Anthropic Claude Code.
```

Then insert the declaration inside the `extern "C"` block, after
`void RXANBPSetShiftFrequency(int channel, double shift);` at `:357`:

```cpp
// =====================================================================
// Notch bandpass tune frequency (nbp.c) ,  the RF origin the per-channel
// notch database maps its absolute-Hz notch centres from.
// calc_nbp_lightweight computes offset = tunefreq + shift
// (third_party/wdsp/src/nbp.c:192), so this is the hosting DDC stream's
// CENTRE, NOT the slice frequency: RXANBPSetShiftFrequency above already
// carries the slice's displacement from that centre.
// From Thetis console.cs:31940-31941 [v2.10.3.15] (RX1 pushes the same
// tunefreq to both subrx ids, RX1DDSFreq being CentreFrequency at
// console.cs:31932) and console.cs:32926 [v2.10.3.15] (RX2).
// Internally idempotent: nbp.c:479 guards on if (tunefreq != a->tunefreq).
// =====================================================================

void RXANBPSetTuneFrequency(int channel, double tunefreq);
```

- [ ] **Step 6: Add the `RxChannel` accessors and carries**

In `src/core/RxChannel.h`, replace the block at `:615-617`:

```cpp
    // --- Frequency shift (for pan offset from VFO) ---

    void setShiftFrequency(double offsetHz);

    // The offset last handed to setShiftFrequency, in Hz. Carried because
    // WDSP exposes no getter for shift.freq, and the design-doc §4.1
    // invariant (notch tune frequency + shift == the slice's demodulated
    // RF) has to be assertable from the caller side.
    double shiftOffsetHz() const { return m_shiftOffsetHz; }

    // --- Notch bandpass tune frequency (TNF §4) ---

    // The RF origin the per-channel notch database maps its absolute-Hz
    // notch centres from: the hosting DDC stream's CENTRE, not the slice
    // frequency. WDSP sums it with the shift above
    // (offset = tunefreq + shift, third_party/wdsp/src/nbp.c:192) and
    // setShiftFrequency already carries the slice's displacement from that
    // centre, so driving this from the slice frequency would compute
    // 2*sliceFreq - streamCentre.
    // From Thetis console.cs:31940-31941 [v2.10.3.15].
    void setNotchTuneFrequency(double absoluteHz);
    double notchTuneFrequencyHz() const { return m_notchTuneFrequencyHz; }
```

and replace the carry member at `:936-938`:

```cpp
    // Shift offset carry (mirrors what was last passed to setShiftFrequency)
    double m_shiftOffsetHz{0.0};

    // Notch tune-frequency carry (mirrors what was last passed to
    // RXANBPSetTuneFrequency). Plain member, not atomic: WDSP owns the
    // authoritative notch database and the audio thread never reads this,
    // so it is main-thread-only state.
    double m_notchTuneFrequencyHz{0.0};
```

In `src/core/RxChannel.cpp`, append after `setShiftFrequency` (currently ends
at `:1489`):

```cpp
// ---------------------------------------------------------------------------
// Notch bandpass tune frequency (TNF §4)
// ---------------------------------------------------------------------------

void RxChannel::setNotchTuneFrequency(double absoluteHz)
{
    // Carry set outside the WDSP guard, mirroring setShiftFrequency, so a
    // stub build and the unit tests still see the quantity the caller
    // resolved.
    m_notchTuneFrequencyHz = absoluteHz;

#ifdef HAVE_WDSP
    // From Thetis console.cs:31940-31941 [v2.10.3.15] ,  pushed on every
    // retune, unconditionally, and the SAME value goes to every subrx
    // sharing the stream. RXANBPSetTuneFrequency is internally idempotent
    // (nbp.c:479, if (tunefreq != a->tunefreq)), so an unconditional push
    // costs nothing.
    RXANBPSetTuneFrequency(m_channelId, absoluteHz);
#endif
}
```

- [ ] **Step 7: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_tune_frequency && ctest --test-dir build -R '^tst_notch_tune_frequency$' --output-on-failure`

Expected: PASS (3 slots).

- [ ] **Step 8: Commit**

```bash
git add tests/tst_notch_tune_frequency.cpp tests/CMakeLists.txt src/core/WdspEngine.h src/core/wdsp_api.h src/core/RxChannel.h src/core/RxChannel.cpp
git commit -m "feat(dsp): declare RXANBPSetTuneFrequency and carry it on RxChannel"
```

---

- [ ] **Step 9: Write the failing test for §4.3 (the shift must reach the notch DB on the way back to zero)**

Add to `tests/tst_notch_tune_frequency.cpp`, after `shift_push_keeps_its_sign()`:

```cpp
    void shift_reaches_the_notch_database_on_the_way_back_to_zero()
    {
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* ch = engine.createRxChannel(0, bufferSizeForRate(kRateHz),
                                               4096, kRateHz, 48000, 48000);
        QVERIFY(ch != nullptr);

        ch->setShiftFrequency(10000.0);
        QCOMPARE(ch->notchShiftHz(), 10000.0);

        // §4.3. RXANBPSetShiftFrequency is the sole writer of
        // NOTCHDB->shift (third_party/wdsp/src/nbp.c:487-496) and
        // calc_nbp_lightweight consumes that field with no reference to any
        // run flag (nbp.c:192), so the old near-zero branch -- which called
        // SetRXAShiftRun(channel, 0) and nothing else -- left the notch
        // database holding a stale shift on every RIT-off, DIG-exit, band
        // jump and CTUN-off. Thetis has no such branch: radio.cs:1419-1420
        // [v2.10.3.15] pushes both setters on every RXOsc change including a
        // change to zero, and SetRXAShiftRun appears nowhere in its Console
        // tree. notchShiftHz() is written next to the WDSP call it mirrors,
        // so it goes stale here if the gate ever comes back.
        ch->setShiftFrequency(0.0);
        QCOMPARE(ch->shiftOffsetHz(), 0.0);
        QCOMPARE(ch->notchShiftHz(), 0.0);
    }
```

- [ ] **Step 10: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_tune_frequency && ctest --test-dir build -R '^tst_notch_tune_frequency$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'notchShiftHz' in 'NereusSDR::RxChannel'`.

- [ ] **Step 11: Restructure `setShiftFrequency` so only the run flag stays gated**

In `src/core/RxChannel.h`, add the accessor immediately after
`double notchTuneFrequencyHz() const { ... }`:

```cpp
    // The shift value last handed to RXANBPSetShiftFrequency. Deliberately
    // distinct from shiftOffsetHz(): it is written next to that call, so it
    // is what tells a caller (and the test suite) whether the push actually
    // happened. NOTCHDB->shift is write-only from the host side
    // (third_party/wdsp/src/nbp.c:487-496 is its sole writer) and
    // calc_nbp_lightweight reads it with no reference to any run flag
    // (nbp.c:192), so a shift that stops being pushed fails silently.
    double notchShiftHz() const { return m_notchShiftHz; }
```

and the carry member immediately after `m_notchTuneFrequencyHz`:

```cpp
    // Notch shift carry (mirrors what was last passed to
    // RXANBPSetShiftFrequency). Main-thread-only, same reasoning as
    // m_notchTuneFrequencyHz. Do NOT move this write away from the WDSP
    // call it mirrors in setShiftFrequency; that co-location is the point.
    double m_notchShiftHz{0.0};
```

In `src/core/RxChannel.cpp`, replace `setShiftFrequency` (`:1468-1489`) in full:

```cpp
void RxChannel::setShiftFrequency(double offsetHz)
{
    if (offsetHz == m_shiftOffsetHz) {
        return;
    }

    m_shiftOffsetHz = offsetHz;

#ifdef HAVE_WDSP
    // From Thetis radio.cs:1419-1420 [v2.10.3.15] ,  both calls use the same
    // sign, and both fire on EVERY RXOsc change, including a change back to
    // zero. Thetis has no run gate at all: SetRXAShiftRun appears nowhere in
    // its Console tree, so the gate below is NereusSDR-original and now
    // covers only the run flag.
    //
    // The two frequency pushes used to sit inside the else of an
    // if (std::abs(offsetHz) < 0.5) branch, so returning to zero skipped
    // them. SetRXAShiftRun writes rxa[channel].shift.p->run (shift.c:113-116)
    // and never touches NOTCHDB->shift, RXANBPSetShiftFrequency is that
    // field's sole writer (nbp.c:487-496), and calc_nbp_lightweight consumes
    // it unconditionally (nbp.c:192). The stored shift therefore went stale
    // on every RIT-off, DIGU/DIGL exit, band jump and CTUN-off, and every
    // notch would have been mapped off its carrier.
    //
    // No sign change. Thetis's -value is not a divergence: rx_osc is already
    // the negated quantity upstream (console.cs:31916-31922,
    // rx2_osc = RXOsc - diff), so Thetis's -rx_osc equals the offsetHz handed
    // in here, which equals frequencyHz - centreHz at
    // SliceStreamAllocator.cpp:70.
    SetRXAShiftFreq(m_channelId, offsetHz);
    RXANBPSetShiftFrequency(m_channelId, offsetHz);
    // Written here, next to the call it mirrors, and not up beside
    // m_shiftOffsetHz: notchShiftHz() exists to say whether the push above
    // really happened.
    m_notchShiftHz = offsetHz;
    SetRXAShiftRun(m_channelId, std::abs(offsetHz) < 0.5 ? 0 : 1);
#else
    m_notchShiftHz = offsetHz;
#endif
}
```

- [ ] **Step 12: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_tune_frequency && ctest --test-dir build -R '^tst_notch_tune_frequency$' --output-on-failure`

Expected: PASS (4 slots).

- [ ] **Step 13: Commit**

```bash
git add tests/tst_notch_tune_frequency.cpp src/core/RxChannel.h src/core/RxChannel.cpp
git commit -m "fix(dsp): push the notch shift on every offset change, not only non-zero ones"
```

---

- [ ] **Step 14: Write the failing tests for §4.1 and §4.2 (the pushed value is the stream centre)**

Add to `tests/tst_notch_tune_frequency.cpp`, after the §4.3 slot:

```cpp
    // ── §4.1/§4.2: bindSliceToStream pushes the hosting stream's centre ───

    void bind_pushes_the_hosting_streams_centre_sole_owner()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(/*userDdcCount*/ 2, /*maxSlices*/ 2, kRateHz);
        // Pool BEFORE the slices. bindSliceToStream is the push site (§4.2)
        // and it can only reach a channel that already exists; the real
        // connect ordering (openRxChannelPool after the binds) is closed by
        // syncNotchesToAllChannels in the fan-out task, per design doc §6.3.
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);

        // Sole occupant: the allocator retunes the stream under the slice
        // (SliceStreamAllocator.cpp:128 sets shiftOffsetHz = 0.0), so the
        // centre and the slice frequency coincide. This half of §4.1 cannot
        // catch the double-count bug on its own; the next slot can.
        QCOMPARE(model.streamCentreHzForTest(0), kSliceAFreqHz);
        QCOMPARE(ch->notchTuneFrequencyHz(), kSliceAFreqHz);
        QCOMPARE(ch->shiftOffsetHz(), 0.0);
    }

    void bind_pushes_the_hosting_streams_centre_joined_existing()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        const int b = model.addSlice();
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(kSliceBFreqHz);

        // Two slices, one stream, 10 kHz apart inside a 192 kHz window.
        // This is Thetis's own topology: console.cs:31922 gives the subrx
        // its own shift while console.cs:31940-31941 [v2.10.3.15] push the
        // IDENTICAL tunefreq to id(0,0) and id(0,1).
        QCOMPARE(sliceB->streamIndex(), 0);
        QCOMPARE(sliceB->shiftOffsetHz(), 10000.0);

        RxChannel* chB = engine->rxChannel(b);
        QVERIFY(chB != nullptr);

        // Half (a) of the §4.1 invariant: tunefreq is the STREAM centre.
        // Driving it from the slice frequency computes
        // 2*sliceFreq - streamCentre, which the sum assertion below would
        // not catch on its own.
        QCOMPARE(chB->notchTuneFrequencyHz(), kSliceAFreqHz);
        QCOMPARE(chB->shiftOffsetHz(), 10000.0);

        // Half (b): the two terms WDSP adds (nbp.c:192) land on the RF this
        // slice is demodulating.
        QCOMPARE(chB->notchTuneFrequencyHz() + chB->shiftOffsetHz(),
                 sliceB->effectiveRxFrequency());
    }

    void a_sole_occupant_retune_carries_the_tune_frequency_to_the_new_centre()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        SliceModel* sliceA = model.sliceById(a);
        sliceA->setFrequency(kSliceAFreqHz);

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);

        // 200 kHz away is outside the 192 kHz window, so the sole occupant
        // drags its own DDC rather than claiming a second one.
        sliceA->setFrequency(kFarFreqHz);

        QCOMPARE(sliceA->streamIndex(), 0);
        QCOMPARE(model.streamCentreHzForTest(0), kFarFreqHz);
        QCOMPARE(ch->notchTuneFrequencyHz(), kFarFreqHz);
        QCOMPARE(ch->shiftOffsetHz(), 0.0);
    }

    void panning_a_shifted_slice_onto_the_centre_clears_the_notch_shift()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        const int b = model.addSlice();
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(kSliceBFreqHz);

        RxChannel* chB = engine->rxChannel(b);
        QVERIFY(chB != nullptr);
        QCOMPARE(chB->notchShiftHz(), 10000.0);

        // Pan out and back (§4.3 at the model level). Slice A still holds
        // the stream, so B is not the sole occupant and this is a
        // JoinedExisting placement with a zero shift, not a retune.
        sliceB->setFrequency(kSliceAFreqHz);

        QCOMPARE(chB->shiftOffsetHz(), 0.0);
        QCOMPARE(chB->notchShiftHz(), 0.0);
        QCOMPARE(chB->notchTuneFrequencyHz(), kSliceAFreqHz);
        QCOMPARE(chB->notchTuneFrequencyHz() + chB->shiftOffsetHz(),
                 sliceB->effectiveRxFrequency());
    }
```

- [ ] **Step 15: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_tune_frequency && ctest --test-dir build -R '^tst_notch_tune_frequency$' --output-on-failure`

Expected: FAIL at runtime in `bind_pushes_the_hosting_streams_centre_sole_owner` with
`Compared values are not the same
   Actual   (ch->notchTuneFrequencyHz()): 0
   Expected (kSliceAFreqHz)              : 1.42e+07`
(nothing pushes the tune frequency yet, so every channel still carries the construction default).

- [ ] **Step 16: Push the stream centre from `bindSliceToStream`**

In `src/models/RadioModel.cpp`, replace the block at `:3671-3679`:

```cpp
    // Push the offset into WDSP. RxChannel::setShiftFrequency is the Thetis
    // RXOsc port (radio.cs:1409-1420 [v2.10.3.15]): SetRXAShiftFreq +
    // RXANBPSetShiftFrequency.
    //
    // The notch database's tune frequency goes with it, unconditionally.
    // From Thetis console.cs:31940-31941 [v2.10.3.15], where RX1DDSFreq is
    // CentreFrequency (console.cs:31932) and the SAME value is pushed to
    // both subrx ids on the stream. WDSP sums the two terms
    // (offset = b->tunefreq + b->shift, third_party/wdsp/src/nbp.c:192), so
    // the stream centre is exactly what makes tunefreq + shift land on the
    // slice's demodulated RF. The slice frequency would compute
    // 2*sliceFreq - streamCentre.
    //
    // Sourced from the allocator, never from placement.newStreamCentreHz:
    // that field is left at 0.0 on the JoinedExisting path
    // (SliceStreamAllocator.cpp:66-73), which is the normal case, and
    // activateStream has already run above so the allocator is
    // authoritative. RXANBPSetTuneFrequency is internally idempotent
    // (nbp.c:479), so pushing on every bind costs nothing.
    if (m_wdspEngine) {
        if (RxChannel* ch = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            ch->setShiftFrequency(placement.shiftOffsetHz);
            ch->setNotchTuneFrequency(
                m_streamAllocator.streamCentreHz(placement.streamIndex));
        }
    }
```

- [ ] **Step 17: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_tune_frequency && ctest --test-dir build -R '^tst_notch_tune_frequency$' --output-on-failure`

Expected: PASS (8 slots).

- [ ] **Step 18: Commit**

```bash
git add tests/tst_notch_tune_frequency.cpp src/models/RadioModel.cpp
git commit -m "fix(dsp): push the hosting stream centre as the notch tune frequency"
```

---

- [ ] **Step 19: Write the failing tests for §4.4 (the two writers must compose, not clobber)**

Add to `tests/tst_notch_tune_frequency.cpp`, after the §4.1/§4.2 slots:

```cpp
    // ── §4.4: the stream offset and RIT/DIG compose ──────────────────────
    //
    // Both writer orders, because fixing only one leaves the mirror bug:
    // RadioModel.cpp:3676 pushed the placement offset with no RIT term and
    // the wireSliceSignals lambda pushed RIT with no placement term, so
    // whichever fired last won and threw the other away.
    //
    // wireSliceSignals early-returns on !m_connection (RadioModel.cpp:8228),
    // so the RIT lambda only exists once a connection is injected. It is
    // never opened: isConnected() stays false, which also keeps the
    // connect-time seed's singleShot a no-op.

    void rit_adds_to_the_stream_shift_instead_of_replacing_it()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        const int b = model.addSlice();
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(kSliceBFreqHz);

        RxChannel* chB = engine->rxChannel(b);
        QVERIFY(chB != nullptr);
        QCOMPARE(chB->shiftOffsetHz(), 10000.0);

        sliceB->setRitHz(500);
        sliceB->setRitEnabled(true);

        // 10 kHz of stream offset PLUS 500 Hz of RIT, not 500 Hz alone.
        QCOMPARE(chB->shiftOffsetHz(), 10500.0);
        QCOMPARE(chB->notchTuneFrequencyHz(), kSliceAFreqHz);
        QCOMPARE(chB->notchTuneFrequencyHz() + chB->shiftOffsetHz(),
                 sliceB->effectiveRxFrequency());
    }

    void a_retune_while_rit_is_on_keeps_the_rit_term()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        const int b = model.addSlice();
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(kSliceBFreqHz);
        sliceB->setRitHz(500);
        sliceB->setRitEnabled(true);

        RxChannel* chB = engine->rxChannel(b);
        QVERIFY(chB != nullptr);
        QCOMPARE(chB->shiftOffsetHz(), 10500.0);

        // The mirror case: the retune writer must not drop the RIT term.
        sliceB->setFrequency(14205000.0);

        QCOMPARE(sliceB->shiftOffsetHz(), 5000.0);
        QCOMPARE(chB->shiftOffsetHz(), 5500.0);
        QCOMPARE(chB->notchTuneFrequencyHz(), kSliceAFreqHz);
        QCOMPARE(chB->notchTuneFrequencyHz() + chB->shiftOffsetHz(),
                 sliceB->effectiveRxFrequency());
    }
```

- [ ] **Step 20: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_tune_frequency && ctest --test-dir build -R '^tst_notch_tune_frequency$' --output-on-failure`

Expected: FAIL at runtime in `rit_adds_to_the_stream_shift_instead_of_replacing_it` with
`Compared values are not the same
   Actual   (chB->shiftOffsetHz()): 500
   Expected (10500.0)              : 10500`

- [ ] **Step 21: Compose the shift once and push the same sum from all three writers**

In `src/models/RadioModel.h`, add the private declaration immediately before
`void pushTxFrequencyFromTxSlice();` (line 2258):

```cpp
    // The total WDSP shift for a slice: the allocator's offset from its
    // hosting stream's centre, plus RIT, plus the per-mode DIG click-tune
    // offset. Three sites push the shift -- bindSliceToStream,
    // activateSliceChannel and the RIT/DIG lambda in wireSliceSignals -- and
    // they used to disagree about which terms belonged in it, so each
    // clobbered the others'. Toggling RIT on a shifted slice threw away the
    // stream offset; retuning with RIT on threw away the RIT.
    // See docs/architecture/2026-07-28-tunable-notch-filter-design.md §4.4.
    double composedShiftHz(const SliceModel* slice) const;
```

In `src/models/RadioModel.cpp`, add the definition immediately above
`void RadioModel::pushTxFrequencyFromTxSlice()` (line 8199):

```cpp
double RadioModel::composedShiftHz(const SliceModel* slice) const
{
    if (!slice) {
        return 0.0;
    }

    // The stream term: "slice freq minus stream centre"
    // (SliceStreamAllocator.h:48), committed to the slice by
    // bindSliceToStream before any of the three writers run.
    double offset = slice->shiftOffsetHz();

    // RIT (Receive Incremental Tuning): client-side demodulation offset that
    // does NOT retune the hardware VFO.
    // From Thetis console.cs ,  RIT adjusts receive demodulation without
    // moving the hardware DDC center.
    if (slice->ritEnabled()) {
        offset += static_cast<double>(slice->ritHz());
    }

    // DIG offset per mode ,  Thetis console.cs:14637 (DIGUClickTuneOffset)
    // and :14672 (DIGLClickTuneOffset).
    if (slice->dspMode() == DSPMode::DIGL) {
        offset += static_cast<double>(slice->diglOffsetHz());
    } else if (slice->dspMode() == DSPMode::DIGU) {
        offset += static_cast<double>(slice->diguOffsetHz());
    }

    return offset;
}
```

Writer 1, `bindSliceToStream` (the `ch->setShiftFrequency(...)` line added in
Step 16):

```cpp
            ch->setShiftFrequency(composedShiftHz(slice));
```

Writer 2, `activateSliceChannel` (`src/models/RadioModel.cpp:3140-3144`):

```cpp
    // The offset the allocator resolved for this slice, plus RIT and DIG.
    // bindSliceToStream pushes it too, but a reconnect re-opens the channel
    // underneath an already-bound slice, so it has to be re-seeded here as
    // well. Composed, not bare: this call runs at the TAIL of
    // bindSliceToStream on a first bind, so a plain slice->shiftOffsetHz()
    // here would silently discard the RIT term that call had just pushed.
    ch->setShiftFrequency(composedShiftHz(slice));
```

Writer 3, the `updateShiftFrequency` lambda in `wireSliceSignals`
(`src/models/RadioModel.cpp:8955-8983`), replacing the comment block and the
lambda body:

```cpp
    // RIT + DIG offset → WDSP shift frequency
    //
    // RIT (Receive Incremental Tuning): client-side demodulation offset that
    // does NOT retune the hardware VFO.
    // From Thetis console.cs ,  RIT adjusts receive demodulation without moving
    // the hardware DDC center.
    //
    // DIG offset: per-mode click-tune demodulation offset for DIGL/DIGU.
    // From Thetis console.cs:14637 (DIGUClickTuneOffset) and :14672
    // (DIGLClickTuneOffset). Both are int offsets in Hz; Thetis uses per-mode
    // filter re-centering internally, but NereusSDR implements DIG offset as
    // an additive shift on the same setShiftFrequency path as RIT.
    //
    // Post-3F these are NOT the only two terms. The slice also sits at an
    // offset from its hosting stream's centre, and this lambda used to push
    // RIT + DIG alone, so toggling RIT on a shifted slice clobbered the
    // stream offset and moved the demodulator off frequency. composedShiftHz
    // is the single sum every writer pushes (design doc §4.4).
    auto updateShiftFrequency = [this, slice]() {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (!rxCh) { return; }
        rxCh->setShiftFrequency(composedShiftHz(slice));
    };
```

- [ ] **Step 22: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_tune_frequency && ctest --test-dir build -R '^tst_notch_tune_frequency$' --output-on-failure`

Expected: PASS (10 slots).

- [ ] **Step 23: Commit**

```bash
git add tests/tst_notch_tune_frequency.cpp src/models/RadioModel.h src/models/RadioModel.cpp
git commit -m "fix(dsp): compose the stream offset with RIT and DIG on every shift writer"
```

---

- [ ] **Step 24: Write the failing test for §4.5 (the connect-time seed)**

Add to `tests/tst_notch_tune_frequency.cpp`, after the §4.4 slots:

```cpp
    // ── §4.5: the connect-time DDC seed ──────────────────────────────────

    void the_connect_seed_commands_the_stream_centre_not_the_slice_frequency()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(kSliceAFreqHz);

        const int b = model.addSlice();
        SliceModel* sliceB = model.sliceById(b);
        sliceB->setFrequency(kSliceBFreqHz);
        QCOMPARE(sliceB->streamIndex(), 0);

        // ReceiverManager drops a frequency for a receiver it never created
        // (ReceiverManager.cpp:226-228), so give stream 0 one.
        ReceiverManager* rm = model.receiverManager();
        QVERIFY(rm != nullptr);
        QCOMPARE(rm->createReceiver(), 0);

        QSignalSpy spy(rm, &ReceiverManager::receiverFrequencyChanged);
        model.seedConnectFrequencyForTest(sliceB);

        // The seed used to hand ReceiverManager slice->frequency()
        // (RadioModel.cpp:9206-9209). For a slice on the JoinedExisting path
        // that is the stream centre plus its own offset, so the DDC came up
        // 10 kHz off and every notch mapped off it came up wrong with it.
        // The allocator is authoritative, matching the bind-time push at
        // RadioModel.cpp:3649-3651.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 0);
        QCOMPARE(spy.first().at(1).toULongLong(),
                 static_cast<quint64>(kSliceAFreqHz));
    }
```

- [ ] **Step 25: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_tune_frequency && ctest --test-dir build -R '^tst_notch_tune_frequency$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'seedConnectFrequencyForTest' in 'NereusSDR::RadioModel'`.
(A verbatim extraction of the current lambda body would then still fail at
runtime with `Actual: 14210000, Expected: 14200000`.)

- [ ] **Step 26: Seed the stream centre, not the slice frequency**

In `src/models/RadioModel.h`, add the private declaration immediately after
`double composedShiftHz(const SliceModel* slice) const;`:

```cpp
    // The connect-time DDC seed, factored out of the wireSliceSignals
    // singleShot so it can be driven without a live connection. Commands the
    // centre of whichever stream hosts `slice`, then re-seeds the TX NCO.
    // See design doc §4.5.
    void seedConnectFrequency(SliceModel* slice);
```

and the test seam immediately after `streamCentreHzForTest` (line 2485,
still inside the public block that precedes `private:`):

```cpp
    /// TNF Task 1 test seam (§4.5): runs the connect-time DDC seed without a
    /// live connection, so a test can assert the quantity it commands.
    void seedConnectFrequencyForTest(SliceModel* slice) {
        seedConnectFrequency(slice);
    }
```

In `src/models/RadioModel.cpp`, add the definition immediately above
`double RadioModel::composedShiftHz(...)`:

```cpp
void RadioModel::seedConnectFrequency(SliceModel* slice)
{
    if (!slice) {
        return;
    }

    // The hosting STREAM's centre, not slice->frequency(). A slice that
    // joined an existing stream sits at a non-zero offset inside its window
    // (SliceStreamAllocator.h:48) and the two quantities differ, so seeding
    // from the slice frequency dragged the DDC off by the stream delta. Same
    // wrong-quantity mistake the notch tune frequency corrects, and the same
    // fix: the allocator owns the centre, exactly as at
    // RadioModel.cpp:3649-3651.
    const int streamIndex = slice->streamIndex();
    if (streamIndex >= 0 && m_receiverManager) {
        const double centreHz = m_streamAllocator.streamCentreHz(streamIndex);
        m_receiverManager->setReceiverFrequency(
            streamIndex, static_cast<quint64>(centreHz));
    }

    // Seed the transmit frequency from the TX-bound slice, which on
    // connect is usually but not necessarily this one.
    pushTxFrequencyFromTxSlice();
}
```

and replace the seed lambda at `src/models/RadioModel.cpp:9201-9215`:

```cpp
    // Send initial frequency to radio (after connection init completes).
    // XIT offset applied here too so on-connect TX NCO matches the stored
    // XIT state without needing a separate update trigger.
    QTimer::singleShot(100, this, [this, slice]() {
        if (m_connection && m_connection->isConnected()) {
            seedConnectFrequency(slice);
        }
    });
```

- [ ] **Step 27: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_tune_frequency && ctest --test-dir build -R '^tst_notch_tune_frequency$' --output-on-failure`

Expected: PASS (11 slots).

- [ ] **Step 28: Commit**

```bash
git add tests/tst_notch_tune_frequency.cpp src/models/RadioModel.h src/models/RadioModel.cpp
git commit -m "fix(dsp): seed the connect-time DDC from the stream centre"
```

---

- [ ] **Step 29: Re-run the two neighbouring suites that share this code path**

`bindSliceToStream`, `activateSliceChannel` and `openRxChannelPool` are all
touched, and `tst_stream_pool_binding` asserts the sibling shift push at
`:576-584`.

Run: `cmake --build build --target tst_stream_pool_binding tst_wdsp_channel_id_map tst_slice_stream_allocator && ctest --test-dir build -R '^(tst_stream_pool_binding|tst_wdsp_channel_id_map|tst_slice_stream_allocator)$' --output-on-failure`

Expected: PASS (3 tests, 0 failures). No commit; this is a regression gate on
the four commits above.

---

### Task 2: wdsp_api.h declarations and RxChannel notch wrappers

**Files:**
- Create: `src/core/dsp/Notch.h`
- Create: `tests/tst_rxchannel_notch_wrappers.cpp`
- Modify: `docs/attribution/THETIS-PROVENANCE.md:413` (append one row to the column-2 "Independently implemented" table, after the `src/core/dsp/TxChannelState.h` row)
- Modify: `src/core/wdsp_api.h:130-136` (append one modification-history entry before the closing `// ====` at :136)
- Modify: `src/core/wdsp_api.h:357` (insert the notch declaration block after Task 1's `RXANBPSetTuneFrequency` declaration, which Task 1 places directly under `void RXANBPSetShiftFrequency(int channel, double shift);`)
- Modify: `src/core/RxChannel.h:200-201` (add `#include "dsp/Notch.h"`), `:211` (add `#include <QList>`), `:617` region (public notch API, after Task 1's `setNotchTuneFrequency` / `notchTuneFrequencyHz` declarations), `:936-938` region (private carries, next to `m_shiftOffsetHz`)
- Modify: `src/core/RxChannel.cpp:1488` region (new notch section after Task 1's `RxChannel::setNotchTuneFrequency` definition, before the `// Channel state` banner at :1490)
- Modify: `src/core/WdspEngine.h:123` (test-only forward declaration) and `:694` (friend declaration)
- Modify: `tests/CMakeLists.txt:5732` (append `nereus_add_test` after `tst_diversity_dialog_persistence`)
- Test: `tests/tst_rxchannel_notch_wrappers.cpp`

**Interfaces:**

- Consumes (from Task 1):
  - `void RXANBPSetTuneFrequency(int channel, double tunefreq);` declared in `src/core/wdsp_api.h` inside the `extern "C"` block
  - `void RxChannel::setNotchTuneFrequency(double absoluteHz);`
  - `double RxChannel::notchTuneFrequencyHz() const;`
  - the rewritten `void RxChannel::setShiftFrequency(double offsetHz);` (both branches push `SetRXAShiftFreq` + `RXANBPSetShiftFrequency`)

- Produces (relied on by Tasks 3, 4, 5, 6, 7, 8, 9, 10):
  - `struct NereusSDR::Notch { int id{0}; double centerHz{0.0}; double widthHz{200.0}; bool active{true}; };` in `src/core/dsp/Notch.h`
  - `int  RXANBPAddNotch(int channel, int notch, double fcenter, double fwidth, int active);`
  - `int  RXANBPGetNotch(int channel, int notch, double* fcenter, double* fwidth, int* active);`
  - `int  RXANBPEditNotch(int channel, int notch, double fcenter, double fwidth, int active);`
  - `int  RXANBPDeleteNotch(int channel, int notch);`
  - `void RXANBPGetNumNotches(int channel, int* nnotches);`
  - `void RXANBPSetNotchesRun(int channel, int run);`
  - `void RXANBPGetMinNotchWidth(int channel, double* minwidth);`
  - `void RXANBPSetAutoIncrease(int channel, int autoincr);`
  - `static constexpr int RxChannel::kMaxNotches = 1024;`
  - `bool   RxChannel::addNotch(int index, const Notch& n);`
  - `bool   RxChannel::editNotch(int index, const Notch& n);`
  - `bool   RxChannel::deleteNotch(int index);`
  - `int    RxChannel::notchCount() const;`
  - `void   RxChannel::syncNotches(const QList<Notch>& notches);`
  - `void   RxChannel::setNotchesRun(bool run);`
  - `bool   RxChannel::notchesRun() const;`
  - `void   RxChannel::setNotchAutoIncrease(bool on);`
  - `bool   RxChannel::notchAutoIncrease() const;`
  - `double RxChannel::minNotchWidthHz() const;`
  - test-only friend seam: `friend class ::TestRxChannelNotchWrappers;` on `WdspEngine`

---

## Cycle 1: the `Notch` value type

Six steps rather than five: the CMake registration is its own action because a test that is not registered cannot be built, so it has to land before the "watch it fail" run.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_rxchannel_notch_wrappers.cpp`:

```cpp
// no-port-check: NereusSDR-original test infrastructure.  Thetis and WDSP
// file names appear in comments to document what each wrapper forwards to;
// no upstream logic is ported into this file.
// =================================================================
// tests/tst_rxchannel_notch_wrappers.cpp  (NereusSDR)
// =================================================================
//
// TNF build-order step 2: the Notch value type plus the RxChannel manual
// notch wrappers that carry it into the per-channel WDSP notch database.
//
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         §5.1 (Notch), §6.1 (wdsp_api.h declarations), §6.2 (RxChannel).
// =================================================================
#include <QtTest/QtTest>

#include "core/dsp/Notch.h"

using namespace NereusSDR;

class TestRxChannelNotchWrappers : public QObject {
    Q_OBJECT

private slots:
    // ── §5.1: the Notch value type ───────────────────────────────────────

    void notch_defaults_to_panadapter_width()
    {
        Notch n;
        QCOMPARE(n.widthHz, 200.0);
    }

    void notch_defaults_to_active()
    {
        Notch n;
        QVERIFY(n.active);
    }

    void notch_defaults_to_unset_id_and_centre()
    {
        Notch n;
        QCOMPARE(n.id, 0);
        QCOMPARE(n.centerHz, 0.0);
    }
};

QTEST_MAIN(TestRxChannelNotchWrappers)
#include "tst_rxchannel_notch_wrappers.moc"
```

- [ ] **Step 2: Register the test in `tests/CMakeLists.txt`**

Insert after line 5732 (`nereus_add_test(tst_diversity_dialog_persistence)`), before the `# ----` aggregate-target banner at line 5734:

```cmake
# TNF build-order step 2: RxChannel manual-notch wrappers (Notch value type,
# add / edit / delete / sync / count, notches-run, auto-increase, min width).
# Opens ONE real WDSP RX channel through the WdspEngine NEREUS_BUILD_TESTS
# friend seam, because every RXANBP* entry point dereferences rxa[channel].ndb
# and the usual kTestChannel = 99 convention would read out of bounds
# (comm.h:110 sizes rxa at MAX_CHANNELS = 32).
# Source: NereusSDR-original test infrastructure.
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §6.2, §11.1.
nereus_add_test(tst_rxchannel_notch_wrappers)
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: FAIL at compile time with `fatal error: 'core/dsp/Notch.h' file not found`

- [ ] **Step 4: Create the `Notch` value type**

Create `src/core/dsp/Notch.h`:

```cpp
// no-port-check: NereusSDR-original struct; the doc comment below names Thetis
// source files only to explain the field-set and default-value provenance, not
// to port code.
#pragma once

namespace NereusSDR {

/// One manual notch, in absolute RF Hz.
///
/// Handed between NotchModel (the owner), RxChannel (the WDSP fan-out) and
/// SpectrumWidget (the marker layer). It lives in core/ rather than models/ so
/// RxChannel can take it by const reference without core acquiring a dependency
/// on models; same placement rationale as dsp/ChannelConfig.h.
///
/// **Field-set provenance.** centerHz / widthHz / active correspond to the three
/// fields of Thetis's MNotch class in radio.cs (FCenter / FWidth / Active). None
/// of MNotch's logic is carried over: its Parse / ToString round-trip exists to
/// fit Thetis's key-value database and NereusSDR persists flat AppSettings keys
/// instead, and its CompareTo has no NereusSDR caller. `id` has no Thetis
/// counterpart at all; Thetis identifies a notch by its position in MNotchDB,
/// which is why every Thetis mutation loses the operator's selection and has to
/// recover it by searching for matching field values.
///
/// **Default width.** 200 Hz is the width Thetis gives a notch created from the
/// panadapter. The authoritative named constant lives on NotchModel; the
/// initialiser here mirrors it so a default-constructed Notch is usable.
///
/// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §5.1.
struct Notch {
    int    id{0};           ///< stable, monotonic; UI hit-test and drag key
    double centerHz{0.0};   ///< absolute RF Hz
    double widthHz{200.0};  ///< Hz
    bool   active{true};    ///< per-notch bypass
};

}  // namespace NereusSDR
```

Append one row to the column-2 exclusion table in `docs/attribution/THETIS-PROVENANCE.md`, directly after the `src/core/dsp/TxChannelState.h` row at line 413:

```markdown
| Manual notch value type ,  pure NereusSDR infrastructure | src/core/dsp/Notch.h | NereusSDR-original 4-field POD (id / centerHz / widthHz / active) passed between NotchModel, RxChannel and SpectrumWidget. Field set and default width derived from Thetis radio.cs class MNotch and the panadapter notch width in console.cs, but no code ported: MNotch's only logic (Parse / ToString / CompareTo) is deliberately dropped, see docs/architecture/2026-07-28-tunable-notch-filter-design.md §5.1 and §5.5. no-port-check tag in file. |
```

- [ ] **Step 5: Run the test, watch it pass**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: PASS (3 slots)

- [ ] **Step 6: Commit**

```bash
git add src/core/dsp/Notch.h tests/tst_rxchannel_notch_wrappers.cpp tests/CMakeLists.txt docs/attribution/THETIS-PROVENANCE.md
git commit -S -m "feat(dsp): add Notch value type for the manual notch filter"
```

---

## Cycle 2: the eight `wdsp_api.h` declarations, `notchCount()` and `kMaxNotches`

- [ ] **Step 1: Extend the test with a real opened channel**

Replace the include block and add the fixture helpers plus two slots in `tests/tst_rxchannel_notch_wrappers.cpp`.

Includes (replace the single `#include "core/dsp/Notch.h"` line):

```cpp
#include <QtTest/QtTest>

#include <QList>

#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/dsp/Notch.h"
#include "core/wdsp_api.h"

using namespace NereusSDR;

namespace {

// A real WDSP channel id, not the usual kTestChannel = 99 sentinel.  Every
// RXANBP* entry point dereferences rxa[channel].ndb before it range-checks
// anything (nbp.c:367, :397, :424, :450, :469, :479), and rxa is sized
// MAX_CHANNELS = 32 (comm.h:110), so 99 is an out-of-bounds read rather than
// a harmless miss.  Design doc §11.1.
constexpr int kNotchTestChannel = 0;

// Geometry the fixture opens the channel with.  Both values are load-bearing
// for the min-notch-width expectation in the last cycle: WDSP derives the
// filter's coefficient count as max(2048, dsp_size) (RXA.c:96) and reads the
// rate straight off the channel (RXA.c:102).
constexpr int kDspBufferSize   = 4096;
constexpr int kDspSampleRateHz = 48000;

// Closes the opened WDSP channel however the scope is left.  QCOMPARE /
// QVERIFY return from the enclosing slot on failure, so a trailing
// destroyRxChannel() would be skipped on exactly the run where a leaked
// rxa[0] slot would poison every later slot.
struct ChannelCloser {
    WdspEngine* engine{nullptr};
    ~ChannelCloser() {
        if (engine) { engine->destroyRxChannel(kNotchTestChannel); }
    }
};

#ifdef HAVE_WDSP
// One notch read straight out of the WDSP database, bypassing the wrapper
// under test.  nbp.c:393 returns -1 and writes fcenter = -1.0 past the end.
struct RawNotch {
    double centerHz{0.0};
    double widthHz{0.0};
    int    active{-1};
    int    rval{-1};
};

RawNotch readRawNotch(int channelId, int index)
{
    RawNotch n;
    n.rval = RXANBPGetNotch(channelId, index, &n.centerHz, &n.widthHz, &n.active);
    return n;
}
#endif

} // namespace
```

Add the fixture helper and the two new slots to the class (helper first, as a plain private member function so QtTest does not treat it as a test):

```cpp
class TestRxChannelNotchWrappers : public QObject {
    Q_OBJECT

private:
    // Primes the engine past its async wisdom load (the NEREUS_BUILD_TESTS
    // friend seam on WdspEngine) and opens one real RX channel, so
    // rxa[kNotchTestChannel].ndb exists.  Same pattern as
    // tests/tst_ps_feedback_channel.cpp:72,78.
    RxChannel* openNotchChannel(WdspEngine& engine)
    {
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        return engine.createRxChannel(kNotchTestChannel,
                                      /*inputBufferSize*/ 238,
                                      /*dspBufferSize*/ kDspBufferSize,
                                      /*inputSampleRate*/ kDspSampleRateHz,
                                      /*dspSampleRate*/ kDspSampleRateHz,
                                      /*outputSampleRate*/ kDspSampleRateHz);
    }

private slots:
    // ... the three Cycle 1 slots stay exactly as they are ...

    // ── §6.2: notch capacity and count readback ──────────────────────────

    void max_notches_matches_the_wdsp_database_size()
    {
        // create_notchdb is called with maxnotches = 1024 for every RXA
        // channel (RXA.c:88); RXANBPAddNotch refuses past it (nbp.c:368).
        QCOMPARE(RxChannel::kMaxNotches, 1024);
    }

    void fresh_channel_reports_zero_notches()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QCOMPARE(ch->notchCount(), 0);
#endif
    }
};
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: FAIL at compile time with `error: 'm_initialized' is a private member of 'NereusSDR::WdspEngine'`, `error: no member named 'kMaxNotches' in 'NereusSDR::RxChannel'`, `error: no member named 'notchCount' in 'NereusSDR::RxChannel'`, and `error: use of undeclared identifier 'RXANBPGetNotch'`

- [ ] **Step 3: Declare the eight WDSP entry points, open the friend seam, add `notchCount()` and `kMaxNotches`**

`src/core/wdsp_api.h` ,  append a modification-history entry immediately before the closing `// ====` at line 136:

```cpp
//   2026-07-29 ,  RXANBPAddNotch, RXANBPGetNotch, RXANBPEditNotch,
//                 RXANBPDeleteNotch, RXANBPGetNumNotches,
//                 RXANBPSetNotchesRun, RXANBPGetMinNotchWidth,
//                 RXANBPSetAutoIncrease declarations added by J.J. Boyd
//                 (KG4VCF) during the Tunable Notch Filter epic, build-order
//                 step 2 ,  the RxChannel manual-notch wrappers.  Signatures
//                 match wdsp/nbp.c:362, 393, 444, 418, 465, 499, 594, 604
//                 [v2.10.3.15]; managed-side P/Invoke decls are
//                 Console/dsp.cs:703-737 [v2.10.3.15].  None of the eight are
//                 declared in the vendored third_party/wdsp/src/nbp.h, which
//                 exports only RXANBPSetFreqs / SetNC / SetMP (nbp.h:96-100) , 
//                 same situation as the existing RXANBPSetShiftFrequency decl.
//                 AI-assisted transformation via Anthropic Claude Code.
```

`src/core/wdsp_api.h` ,  insert this block inside the `extern "C" { ... }` region, immediately after Task 1's `RXANBPSetTuneFrequency` declaration (which sits under `void RXANBPSetShiftFrequency(int channel, double shift);` at line 357):

```cpp
// =====================================================================
// Manual notch filter ,  per-channel notch database (WDSP nbp.c)
// From Thetis Project Files/Source/Console/dsp.cs:703-737 [v2.10.3.15] , 
// P/Invoke declarations.  Thetis marshals `active` / `run` / `autoincr` as
// C# bool, which DllImport marshals as a 4-byte Win32 BOOL; the C signatures
// below take int, which is the same wire value.
//
// The vendored third_party/wdsp/src/nbp.h declares only RXANBPSetFreqs (:96),
// RXANBPSetNC (:98) and RXANBPSetMP (:100).  Every entry point below is
// PORT-exported from nbp.c but absent from that header, which is why it has
// to be declared here.  RXANBPSetNC is deliberately NOT declared: the route
// to it already exists transitively through RXASetNC (RXA.c:1043).
// =====================================================================

// nbp.c:362 ,  INSERT at position `notch`.  Returns -1 and mutates nothing
// when notch > nn or the database is already full (RXA.c:88 sizes it at
// 1024).  Callers must surface the -1; the recovery is a full resync.
int RXANBPAddNotch(int channel, int notch, double fcenter, double fwidth,
                   int active);

// nbp.c:393 ,  readback.  Returns -1 and writes fcenter = -1.0, fwidth = 0.0,
// active = -1 when notch >= nn.
int RXANBPGetNotch(int channel, int notch, double* fcenter, double* fwidth,
                   int* active);

// nbp.c:444 ,  overwrite in place.  Returns -1 when notch >= nn.
int RXANBPEditNotch(int channel, int notch, double fcenter, double fwidth,
                    int active);

// nbp.c:418 ,  erase and shift the remaining entries down one slot.  Returns
// -1 when notch >= nn.
int RXANBPDeleteNotch(int channel, int notch);

// nbp.c:465
void RXANBPGetNumNotches(int channel, int* nnotches);

// nbp.c:499 ,  the ONLY writer of notchdb.master_run.  RXA.c:86-88 builds
// every database with master_run = 0 and nbp0 with its notch-run flag 0, and
// both calc_nbp_lightweight (nbp.c:190) and calc_nbp_impulse (nbp.c:223)
// bypass the database entirely when fnfrun is 0.  A channel that never gets
// this call is notch-inert, not merely notch-empty.
void RXANBPSetNotchesRun(int channel, int run);

// nbp.c:594 ,  narrowest notch the current filter can realise.  Varies with
// the filter's coefficient count and sample rate (min_notch_width,
// nbp.c:82-95), so it is not a constant.
void RXANBPGetMinNotchWidth(int channel, double* minwidth);

// nbp.c:604
void RXANBPSetAutoIncrease(int channel, int autoincr);
```

`src/core/WdspEngine.h` ,  add the global-scope forward declaration after line 123 (`class TestWdspChannelIdMap;`), inside the existing `#ifdef NEREUS_BUILD_TESTS` block:

```cpp
// Tunable Notch Filter step 2: the notch-wrapper test opens one real RX
// channel so the RXANBP* entry points have an rxa[].ndb to dereference.
class TestRxChannelNotchWrappers;
```

`src/core/WdspEngine.h` ,  add the friend after line 694 (`friend class ::TestWdspChannelIdMap;`):

```cpp
    // Tunable Notch Filter step 2: same friendship for the notch-wrapper
    // test, which primes m_initialized so createRxChannel opens a real
    // WDSP channel with a real notch database.
    friend class ::TestRxChannelNotchWrappers;
```

`src/core/RxChannel.h` ,  add the include after line 200 (`#include "dsp/ChannelConfig.h"`):

```cpp
#include "dsp/Notch.h"
```

and after line 211 (`#include <QObject>`):

```cpp
#include <QList>
```

`src/core/RxChannel.h` ,  add the public notch API immediately after Task 1's `notchTuneFrequencyHz()` declaration in the frequency-shift block:

```cpp
    // --- Manual notch filter (TNF) ---
    //
    // WDSP owns the authoritative per-channel notch database; RxChannel is a
    // thin forwarder.  List position IS the WDSP notch index, so every caller
    // must keep its own ordering in lockstep (design doc §5.2).
    //
    // WDSP builds each RXA channel's database with room for 1024 notches
    // (third_party/wdsp/src/RXA.c:88).  RXANBPAddNotch returns -1 and mutates
    // nothing once nn reaches that (nbp.c:368).
    static constexpr int kMaxNotches = 1024;

    /// Number of notches currently installed on this channel.
    int notchCount() const;
```

`src/core/RxChannel.cpp` ,  add a new section immediately after Task 1's `RxChannel::setNotchTuneFrequency` definition, before the `// Channel state` banner:

```cpp
// ---------------------------------------------------------------------------
// Manual notch filter (TNF) ,  per-channel WDSP notch database
// ---------------------------------------------------------------------------

int RxChannel::notchCount() const
{
#ifdef HAVE_WDSP
    // From Thetis console.cs:40265 [v2.10.3.15] ,  AddNotch reads the count
    // back out of WDSP before it picks an insert index.
    // WDSP: third_party/wdsp/src/nbp.c:465
    int n = 0;
    RXANBPGetNumNotches(m_channelId, &n);
    return n;
#else
    return 0;
#endif
}
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: PASS (5 slots)

- [ ] **Step 5: Commit**

```bash
git add src/core/wdsp_api.h src/core/WdspEngine.h src/core/RxChannel.h src/core/RxChannel.cpp tests/tst_rxchannel_notch_wrappers.cpp
git commit -S -m "feat(wdsp): declare the eight RXANBP notch entry points and read the notch count"
```

---

## Cycle 3: `addNotch`

- [ ] **Step 1: Write the failing test**

Add two slots to `TestRxChannelNotchWrappers`:

```cpp
    // ── §6.2: add ────────────────────────────────────────────────────────

    void add_notch_appends_in_list_order()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 14074000.0, 200.0, true}));
        QVERIFY(ch->addNotch(1, Notch{2, 14100000.0, 100.0, true}));
        QVERIFY(ch->addNotch(2, Notch{3, 14200000.0, 500.0, false}));

        QCOMPARE(ch->notchCount(), 3);

        const RawNotch first = readRawNotch(kNotchTestChannel, 0);
        QCOMPARE(first.rval, 0);
        QCOMPARE(first.centerHz, 14074000.0);
        QCOMPARE(first.widthHz, 200.0);
        QCOMPARE(first.active, 1);

        const RawNotch third = readRawNotch(kNotchTestChannel, 2);
        QCOMPARE(third.rval, 0);
        QCOMPARE(third.centerHz, 14200000.0);
        QCOMPARE(third.widthHz, 500.0);
        QCOMPARE(third.active, 0);
#endif
    }

    void add_notch_past_the_end_is_rejected_without_mutating()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 7040000.0, 200.0, true}));

        // nbp.c:368 guards on `notch <= b->nn`, so index 1 is legal (append)
        // and index 2 is not.  The -1 must reach the caller, not be swallowed.
        QVERIFY(!ch->addNotch(2, Notch{2, 7050000.0, 200.0, true}));
        QCOMPARE(ch->notchCount(), 1);
#endif
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: FAIL at compile time with `error: no member named 'addNotch' in 'NereusSDR::RxChannel'`

- [ ] **Step 3: Implement `addNotch`**

`src/core/RxChannel.h` ,  add after the `notchCount()` declaration:

```cpp
    /// Insert `n` at WDSP notch index `index`.  Returns false when WDSP
    /// refuses (index past the end, or kMaxNotches reached), in which case
    /// nothing was mutated and the caller should resync.
    bool addNotch(int index, const Notch& n);
```

`src/core/RxChannel.cpp` ,  add above `notchCount()`:

```cpp
bool RxChannel::addNotch(int index, const Notch& n)
{
#ifdef HAVE_WDSP
    // From Thetis console.cs:40271-40273 [v2.10.3.15] ,  AddNotch pushes the
    // same (index, centre, width, active) tuple to every RX channel.  Centre
    // and width are absolute Hz on the wire (console.cs:40271 passes fFreqHZ
    // straight through).
    // WDSP: third_party/wdsp/src/nbp.c:362 ,  an INSERT guarded by
    // `notch <= b->nn && b->nn < b->maxnotches`; returns -1 with no mutation
    // otherwise.
    const int rval = RXANBPAddNotch(m_channelId, index, n.centerHz, n.widthHz,
                                    n.active ? 1 : 0);
    if (rval < 0) {
        qCWarning(lcDsp) << "RxChannel" << m_channelId
                          << "RXANBPAddNotch rejected index" << index
                          << "centreHz" << n.centerHz
                          << "existing" << notchCount();
        return false;
    }
    return true;
#else
    Q_UNUSED(index);
    Q_UNUSED(n);
    return false;
#endif
}
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: PASS (7 slots)

- [ ] **Step 5: Commit**

```bash
git add src/core/RxChannel.h src/core/RxChannel.cpp tests/tst_rxchannel_notch_wrappers.cpp
git commit -S -m "feat(dsp): add RxChannel::addNotch and surface the WDSP rejection"
```

---

## Cycle 4: `editNotch` and `deleteNotch`

- [ ] **Step 1: Write the failing test**

Add four slots to `TestRxChannelNotchWrappers`:

```cpp
    // ── §6.2: edit ───────────────────────────────────────────────────────

    void edit_notch_rewrites_only_that_index()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 14074000.0, 200.0, true}));
        QVERIFY(ch->addNotch(1, Notch{2, 14100000.0, 200.0, true}));

        QVERIFY(ch->editNotch(1, Notch{2, 14101234.0, 350.0, false}));

        const RawNotch edited = readRawNotch(kNotchTestChannel, 1);
        QCOMPARE(edited.centerHz, 14101234.0);
        QCOMPARE(edited.widthHz, 350.0);
        QCOMPARE(edited.active, 0);

        const RawNotch untouched = readRawNotch(kNotchTestChannel, 0);
        QCOMPARE(untouched.centerHz, 14074000.0);
        QCOMPARE(untouched.widthHz, 200.0);
        QCOMPARE(untouched.active, 1);
        QCOMPARE(ch->notchCount(), 2);
#endif
    }

    void edit_notch_past_the_end_is_rejected()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 3573000.0, 200.0, true}));
        QVERIFY(!ch->editNotch(1, Notch{2, 3574000.0, 200.0, true}));
        QCOMPARE(ch->notchCount(), 1);
#endif
    }

    // ── §6.2 / §5.2: delete keeps position == WDSP index ─────────────────

    void delete_notch_shifts_later_notches_down()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 14074000.0, 200.0, true}));
        QVERIFY(ch->addNotch(1, Notch{2, 14100000.0, 200.0, true}));
        QVERIFY(ch->addNotch(2, Notch{3, 14200000.0, 200.0, true}));

        // Delete from the middle: WDSP shifts its array down (nbp.c:426-434),
        // so index 1 must now be what used to be index 2.
        QVERIFY(ch->deleteNotch(1));

        QCOMPARE(ch->notchCount(), 2);
        QCOMPARE(readRawNotch(kNotchTestChannel, 0).centerHz, 14074000.0);
        QCOMPARE(readRawNotch(kNotchTestChannel, 1).centerHz, 14200000.0);
        QCOMPARE(readRawNotch(kNotchTestChannel, 2).rval, -1);
#endif
    }

    void delete_notch_past_the_end_is_rejected()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(!ch->deleteNotch(0));
        QCOMPARE(ch->notchCount(), 0);
#endif
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: FAIL at compile time with `error: no member named 'editNotch' in 'NereusSDR::RxChannel'` and `error: no member named 'deleteNotch' in 'NereusSDR::RxChannel'`

- [ ] **Step 3: Implement `editNotch` and `deleteNotch`**

`src/core/RxChannel.h` ,  add after the `addNotch` declaration:

```cpp
    /// Overwrite the notch at WDSP index `index`.  Returns false when the
    /// index is past the end, in which case nothing was mutated.
    bool editNotch(int index, const Notch& n);

    /// Erase the notch at WDSP index `index`.  WDSP shifts the remaining
    /// entries down one slot, so callers must do the same to keep list
    /// position == WDSP index.  Returns false when the index is past the end.
    bool deleteNotch(int index);
```

`src/core/RxChannel.cpp` ,  add after `addNotch`:

```cpp
bool RxChannel::editNotch(int index, const Notch& n)
{
#ifdef HAVE_WDSP
    // From Thetis console.cs:40028-40030 [v2.10.3.15] (ChangeNotchBW) and
    // console.cs:40100-40102 [v2.10.3.15] (ChangeNotchCentreFrequency).  Both
    // Thetis edit paths read the current tuple back, change one member and
    // push the whole tuple; NereusSDR's caller already holds the whole tuple,
    // so the readback is unnecessary.
    // WDSP: third_party/wdsp/src/nbp.c:444 ,  returns -1 when notch >= nn.
    //
    // Not cheap: RXANBPEditNotch runs UpdateNBPFilters (nbp.c:345-359), which
    // designs nbp0 AND recalc_bpsnba_filter (snb.c:814-828).  That is one
    // filter pair per edit, versus 2N for a full syncNotches, which is why
    // live edits take this path.
    const int rval = RXANBPEditNotch(m_channelId, index, n.centerHz, n.widthHz,
                                     n.active ? 1 : 0);
    if (rval < 0) {
        qCWarning(lcDsp) << "RxChannel" << m_channelId
                          << "RXANBPEditNotch rejected index" << index
                          << "of" << notchCount();
        return false;
    }
    return true;
#else
    Q_UNUSED(index);
    Q_UNUSED(n);
    return false;
#endif
}

bool RxChannel::deleteNotch(int index)
{
#ifdef HAVE_WDSP
    // From Thetis console.cs:40207-40209 [v2.10.3.15] ,  removeNotch.
    // WDSP: third_party/wdsp/src/nbp.c:418 ,  erases and shifts the array
    // down, so the caller's list must shift the same way (design doc §5.2).
    const int rval = RXANBPDeleteNotch(m_channelId, index);
    if (rval < 0) {
        qCWarning(lcDsp) << "RxChannel" << m_channelId
                          << "RXANBPDeleteNotch rejected index" << index
                          << "of" << notchCount();
        return false;
    }
    return true;
#else
    Q_UNUSED(index);
    return false;
#endif
}
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: PASS (11 slots)

- [ ] **Step 5: Commit**

```bash
git add src/core/RxChannel.h src/core/RxChannel.cpp tests/tst_rxchannel_notch_wrappers.cpp
git commit -S -m "feat(dsp): add RxChannel::editNotch and RxChannel::deleteNotch"
```

---

## Cycle 5: `syncNotches`

- [ ] **Step 1: Write the failing test**

Add two slots to `TestRxChannelNotchWrappers`:

```cpp
    // ── §6.2: full rebuild ───────────────────────────────────────────────

    void sync_notches_replaces_the_whole_set_in_list_order()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        // Stale set the channel is already carrying.
        QVERIFY(ch->addNotch(0, Notch{9, 1810000.0, 200.0, true}));
        QVERIFY(ch->addNotch(1, Notch{8, 1820000.0, 200.0, true}));

        const QList<Notch> wanted = {
            Notch{1, 7040000.0,  200.0, true},
            Notch{2, 7074000.0,  100.0, false},
            Notch{3, 7100000.0, 1000.0, true},
        };
        ch->syncNotches(wanted);

        QCOMPARE(ch->notchCount(), wanted.size());
        for (int i = 0; i < wanted.size(); ++i) {
            const RawNotch got = readRawNotch(kNotchTestChannel, i);
            QCOMPARE(got.rval, 0);
            QCOMPARE(got.centerHz, wanted.at(i).centerHz);
            QCOMPARE(got.widthHz, wanted.at(i).widthHz);
            QCOMPARE(got.active, wanted.at(i).active ? 1 : 0);
        }
#endif
    }

    void sync_notches_with_an_empty_list_clears_the_channel()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        QVERIFY(ch->addNotch(0, Notch{1, 10120000.0, 200.0, true}));
        QVERIFY(ch->addNotch(1, Notch{2, 10130000.0, 200.0, true}));

        // NotchModel::clear() lands here via notchesReset(); a sync that did
        // not erase would leave the channel notching while the model shows
        // nothing (design doc §5.3, clear() contract).
        ch->syncNotches({});

        QCOMPARE(ch->notchCount(), 0);
        QCOMPARE(readRawNotch(kNotchTestChannel, 0).rval, -1);
#endif
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: FAIL at compile time with `error: no member named 'syncNotches' in 'NereusSDR::RxChannel'`

- [ ] **Step 3: Implement `syncNotches`**

`src/core/RxChannel.h` ,  add after the `deleteNotch` declaration:

```cpp
    /// Replace this channel's entire notch set with `notches`, in list order,
    /// so list position == WDSP notch index.  Used on channel activation and
    /// after NotchModel::restoreFromSettings; live edits use the incremental
    /// calls above because this one designs 2N filter pairs.
    void syncNotches(const QList<Notch>& notches);
```

`src/core/RxChannel.cpp` ,  add after `deleteNotch`:

```cpp
void RxChannel::syncNotches(const QList<Notch>& notches)
{
#ifdef HAVE_WDSP
    // Drop whatever the channel is currently carrying.  Always erase index 0:
    // RXANBPDeleteNotch shifts the array down (nbp.c:426-434), so repeatedly
    // removing the head walks the whole database without index arithmetic.
    for (int remaining = notchCount(); remaining > 0; --remaining) {
        RXANBPDeleteNotch(m_channelId, 0);
    }

    // From Thetis setup.cs:18002-18004 [v2.10.3.15] , 
    // RestoreNotchesFromDatabase: one RXANBPAddNotch per stored notch with
    // the loop counter as the index, which is what makes list position and
    // WDSP index the same thing (design doc §5.2).
    // sets max limits, and selects first notch if one exists MW0LGE
    //   [original inline comment from setup.cs:18007]
    for (int i = 0; i < notches.size(); ++i) {
        const Notch& n = notches.at(i);
        if (RXANBPAddNotch(m_channelId, i, n.centerHz, n.widthHz,
                            n.active ? 1 : 0) < 0) {
            qCWarning(lcDsp) << "RxChannel" << m_channelId
                              << "notch sync truncated at index" << i
                              << "of" << notches.size();
            return;
        }
    }
#else
    Q_UNUSED(notches);
#endif
}
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: PASS (13 slots)

- [ ] **Step 5: Commit**

```bash
git add src/core/RxChannel.h src/core/RxChannel.cpp tests/tst_rxchannel_notch_wrappers.cpp
git commit -S -m "feat(dsp): add RxChannel::syncNotches full notch-set rebuild"
```

---

## Cycle 6: `setNotchesRun`, `setNotchAutoIncrease`, `minNotchWidthHz`

- [ ] **Step 1: Write the failing test**

Add four slots to `TestRxChannelNotchWrappers`:

```cpp
    // ── §6.3: run flag and auto-increase carries ─────────────────────────

    void notches_run_defaults_off_and_round_trips()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        // create_notchdb is called with master_run = 0 (RXA.c:87), so a
        // freshly opened channel is notch-inert until told otherwise.
        QVERIFY(!ch->notchesRun());

        ch->setNotchesRun(true);
        QVERIFY(ch->notchesRun());

        ch->setNotchesRun(false);
        QVERIFY(!ch->notchesRun());
#endif
    }

    void auto_increase_defaults_to_the_wdsp_construction_value()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        // create_nbp is called with autoincr = 1 (RXA.c:105).
        QVERIFY(ch->notchAutoIncrease());
#endif
    }

    void auto_increase_round_trips()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        ch->setNotchAutoIncrease(false);
        QVERIFY(!ch->notchAutoIncrease());

        ch->setNotchAutoIncrease(true);
        QVERIFY(ch->notchAutoIncrease());
#endif
    }

    // ── §9: minimum realisable notch width ───────────────────────────────

    void min_notch_width_matches_the_wdsp_formula()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires a live WDSP build: the notch database is rxa[].ndb.");
#else
        WdspEngine engine;
        RxChannel* ch = openNotchChannel(engine);
        ChannelCloser closer{&engine};
        QVERIFY(ch != nullptr);

        // min_notch_width (nbp.c:82-95) for wintype 0 is
        //   1600.0 / (nc / 256) * (rate / 48000)
        // create_nbp gives this channel wintype 0 (RXA.c:103),
        // nc = max(2048, dsp_size) = 4096 (RXA.c:96) and rate = dsp_rate
        // = 48000 (RXA.c:102), so 1600.0 / 16 * 1 = 100.0 Hz.
        QCOMPARE(ch->minNotchWidthHz(), 100.0);
#endif
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: FAIL at compile time with `error: no member named 'notchesRun' in 'NereusSDR::RxChannel'`, `error: no member named 'notchAutoIncrease' in 'NereusSDR::RxChannel'`, and `error: no member named 'minNotchWidthHz' in 'NereusSDR::RxChannel'`

- [ ] **Step 3: Implement the run flag, auto-increase and min-width readback**

`src/core/RxChannel.h` ,  add after the `syncNotches` declaration:

```cpp
    /// Master notch enable for THIS channel.  WDSP builds every notch
    /// database inert (RXA.c:87), so a channel that never gets this call is
    /// notch-inert rather than merely notch-empty.
    void setNotchesRun(bool run);
    bool notchesRun() const { return m_notchesRun; }

    /// Let WDSP widen a notch that is narrower than the filter can realise,
    /// instead of dropping it.
    void setNotchAutoIncrease(bool on);
    bool notchAutoIncrease() const { return m_notchAutoIncrease; }

    /// Narrowest notch the current filter can realise, in Hz.  Varies with
    /// the filter's coefficient count and the channel's DSP rate, so it must
    /// be re-read after either changes.
    double minNotchWidthHz() const;
```

`src/core/RxChannel.h` ,  add the carries next to `m_shiftOffsetHz` in the private section:

```cpp
    // Manual notch carries.  Main-thread only, no atomics: WDSP owns the
    // authoritative per-channel notch state and there is no WDSP getter for
    // either flag, so these mirror the last value pushed purely so callers
    // and tests can read back what a channel was told.
    // Defaults mirror WDSP's construction values so the carry is not a lie
    // about a freshly opened channel: create_notchdb master_run = 0
    // (third_party/wdsp/src/RXA.c:87), create_nbp autoincr = 1 (RXA.c:105).
    bool m_notchesRun{false};
    bool m_notchAutoIncrease{true};
```

`src/core/RxChannel.cpp` ,  add after `syncNotches`:

```cpp
void RxChannel::setNotchesRun(bool run)
{
    m_notchesRun = run;
#ifdef HAVE_WDSP
    // From Thetis console.cs:40000-40002 [v2.10.3.15] ,  the TNFActive setter
    // fans the same flag to all three fixed channel ids.
    // WDSP: third_party/wdsp/src/nbp.c:499 ,  the only writer of
    // notchdb.master_run; it also drives nbp0.fnfrun and re-runs
    // RXAbpsnbaCheck / RXAbpsnbaSet, so it is not a cheap toggle.
    RXANBPSetNotchesRun(m_channelId, run ? 1 : 0);
#endif
}

void RxChannel::setNotchAutoIncrease(bool on)
{
    m_notchAutoIncrease = on;
#ifdef HAVE_WDSP
    // From Thetis setup.cs:17928-17930 [v2.10.3.15] , 
    // chkMNFAutoIncrease_CheckedChanged.
    // WDSP: third_party/wdsp/src/nbp.c:604 ,  touches both nbp0 and bpsnba.
    RXANBPSetAutoIncrease(m_channelId, on ? 1 : 0);
#endif
}

double RxChannel::minNotchWidthHz() const
{
#ifdef HAVE_WDSP
    // From Thetis console.cs:48804 [v2.10.3.15] ,  the per-RX minimum notch
    // width readback that feeds Thetis's _minimum_rx_notch_width map.
    // WDSP: third_party/wdsp/src/nbp.c:594 -> min_notch_width (nbp.c:82-95),
    // which scales with the filter's coefficient count and sample rate.
    double minWidth = 0.0;
    RXANBPGetMinNotchWidth(m_channelId, &minWidth);
    return minWidth;
#else
    return 0.0;
#endif
}
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_rxchannel_notch_wrappers && ctest --test-dir build -R '^tst_rxchannel_notch_wrappers$' --output-on-failure`

Expected: PASS (17 slots)

- [ ] **Step 5: Commit**

```bash
git add src/core/RxChannel.h src/core/RxChannel.cpp tests/tst_rxchannel_notch_wrappers.cpp
git commit -S -m "feat(dsp): wire the notch run flag, auto-increase and min-width readback"
```

---

### Task 3: NotchModel: guards, clear() contract, persistence, pure helpers

**Files:**
- Create: `src/models/NotchModel.h`
- Create: `src/models/NotchModel.cpp`
- Modify: `CMakeLists.txt:657-673` (add `src/models/NotchModel.cpp` to `MODEL_SOURCES`)
- Modify: `docs/attribution/THETIS-PROVENANCE.md:315-316` (insert two rows, alphabetical, between `BandDefaults.cpp` and `PanadapterModel.cpp`)
- Modify: `docs/attribution/aethersdr-reconciliation.md:88-89` (insert two rows in the Models table)
- Modify: `tests/CMakeLists.txt:5732` (append five `nereus_add_test` registrations)
- Test: `tests/tst_notch_model_guards.cpp`
- Test: `tests/tst_notch_model_index_invariant.cpp`
- Test: `tests/tst_notch_spatial_helpers.cpp`
- Test: `tests/tst_notch_sideband_shift.cpp`
- Test: `tests/tst_notch_persistence.cpp`

**Interfaces:**

- Consumes (from Task 2):
  - `struct NereusSDR::Notch { int id{0}; double centerHz{0.0}; double widthHz{200.0}; bool active{true}; };` declared in `src/core/dsp/Notch.h`
  - `void RxChannel::syncNotches(const QList<Notch>& notches);` (not called here; establishes that `Notch` is reachable from `core/`)

- Produces (relied on by Tasks 4 through 10):
  - `class NereusSDR::NotchModel : public QObject` in `src/models/NotchModel.h`
  - `explicit NotchModel(QObject* parent = nullptr);`
  - `const QList<Notch>& notches() const;`
  - `const Notch* notchById(int id) const;`
  - `int indexOfId(int id) const;`
  - `bool globalEnabled() const;` / `bool autoIncrease() const;` / `bool visualEnabled() const;` / `bool adminBusy() const;`
  - `bool notchNearFreq(double hz, int deltaHz) const;`
  - `QList<Notch> notchesInBandwidth(double centreHz, int lowHz, int highHz) const;`
  - `const Notch* notchSurrounding(double centreHz, int lowHz, int highHz, double hz, int padWidthHz = 0) const;`
  - `static int notchSidebandShift(int filterLowHz, int filterHighHz);`
  - `int addNotch(double centerHz, double widthHz = kDefaultNotchWidthHz);`
  - `bool setCenter(int id, double centerHz);` / `bool setWidth(int id, double widthHz);` / `bool setActive(int id, bool active);` / `bool removeNotch(int id);`
  - `void setGlobalEnabled(bool on);` / `void setAutoIncrease(bool on);` / `void setVisualEnabled(bool on);` / `void setAdminBusy(bool busy);` / `void clear();`
  - `void saveToSettings() const;` / `void restoreFromSettings();`
  - signals `notchAdded(int)`, `notchChanged(int)`, `notchRemoved(int, int)`, `globalEnabledChanged(bool)`, `autoIncreaseChanged(bool)`, `visualEnabledChanged(bool)`, `notchAddRejected(const QString&)`, `notchesReset()`
  - constants `kNotchDedupeWindowHz`, `kDefaultNotchWidthHz`, `kNarrowNotchWidthHz`, `kMaxNotchWidthHz`, `kMinNotchCentreHz`, `kMaxNotchCentreHz`

> **Plan note on the header.** `NotchModel.h` lands complete in Cycle 1 because §5.3 is the published contract Tasks 4 through 10 consume and later tasks must not be blocked on header churn. The `.cpp` fills in across the six cycles below, so every cycle after the first fails at **link** time with a named undefined symbol before its implementation lands. That is a real, specific, reproducible red.

---

- [ ] **Step 1: Write the failing test for the add-path guards**

Create `tests/tst_notch_model_guards.cpp`:

```cpp
// =================================================================
// tests/tst_notch_model_guards.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF §5.4: NotchModel's ported guards. Add path in this file's first
// half, edit path in the second (added by Cycle 2).
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         §5.4 (guard table), §1.2 (CW pitch correction NOT ported).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/NotchModel.h"

using namespace NereusSDR;

class TestNotchModelGuards : public QObject {
    Q_OBJECT

private slots:
    // ── Whole-Hz rounding on add (Thetis console.cs:40230) ───────────────
    void add_rounds_centre_to_whole_hz()
    {
        NotchModel m;
        const int id = m.addNotch(14074123.4);
        QVERIFY(id > 0);
        QCOMPARE(m.notchById(id)->centerHz, 14074123.0);
    }

    void add_rounds_midpoint_to_even()
    {
        // C# Math.Round(double) is MidpointRounding.ToEven. Both midpoints
        // below therefore land on the even neighbour 14074124.
        NotchModel a;
        const int idA = a.addNotch(14074123.5);
        QVERIFY(idA > 0);
        QCOMPARE(a.notchById(idA)->centerHz, 14074124.0);

        NotchModel b;
        const int idB = b.addNotch(14074124.5);
        QVERIFY(idB > 0);
        QCOMPARE(b.notchById(idB)->centerHz, 14074124.0);
    }

    // ── Width defaults (Thetis console.cs:40268-40269) ───────────────────
    void add_default_width_is_200hz()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QCOMPARE(m.notchById(id)->widthHz, NotchModel::kDefaultNotchWidthHz);
        QCOMPARE(NotchModel::kDefaultNotchWidthHz, 200.0);
    }

    void add_narrow_width_is_100hz()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0, NotchModel::kNarrowNotchWidthHz);
        QCOMPARE(m.notchById(id)->widthHz, 100.0);
        QCOMPARE(NotchModel::kNarrowNotchWidthHz, 100.0);
    }

    void add_marks_new_notch_active()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QVERIFY(m.notchById(id)->active);
    }

    void add_emits_notchAdded_with_the_new_id()
    {
        NotchModel m;
        QSignalSpy spy(&m, &NotchModel::notchAdded);
        const int id = m.addNotch(14074000.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), id);
    }

    // ── 10 Hz dedupe, exact boundary (Thetis console.cs:40260 via
    //    radio.cs:4267 ,  strict `<`, so exactly 10 Hz away is allowed) ────
    void dedupe_window_is_ten_hz()
    {
        QCOMPARE(NotchModel::kNotchDedupeWindowHz, 10);
    }

    void add_rejects_notch_within_ten_hz()
    {
        NotchModel m;
        QVERIFY(m.addNotch(14074000.0) > 0);
        QCOMPARE(m.addNotch(14074009.0), -1);
        QCOMPARE(m.addNotch(14073991.0), -1);
        QCOMPARE(m.notches().size(), 1);
    }

    void add_allows_notch_exactly_ten_hz_away()
    {
        NotchModel m;
        QVERIFY(m.addNotch(14074000.0) > 0);
        QVERIFY(m.addNotch(14074010.0) > 0);
        QCOMPARE(m.notches().size(), 2);

        NotchModel below;
        QVERIFY(below.addNotch(14074000.0) > 0);
        QVERIFY(below.addNotch(14073990.0) > 0);
        QCOMPARE(below.notches().size(), 2);
    }

    void add_rejection_emits_reason()
    {
        NotchModel m;
        QVERIFY(m.addNotch(14074000.0) > 0);
        QSignalSpy spy(&m, &NotchModel::notchAddRejected);
        QCOMPARE(m.addNotch(14074002.0), -1);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!spy.first().first().toString().isEmpty());
    }

    // ── Centre constrained to the radio tuning range
    //    (Thetis console.cs:40257; bounds per design §5.4) ────────────────
    void add_rejects_centre_below_minimum()
    {
        NotchModel m;
        QCOMPARE(m.addNotch(NotchModel::kMinNotchCentreHz - 1.0), -1);
        QVERIFY(m.notches().isEmpty());
    }

    void add_accepts_centre_exactly_at_minimum()
    {
        NotchModel m;
        QVERIFY(m.addNotch(NotchModel::kMinNotchCentreHz) > 0);
    }

    void add_rejects_centre_above_maximum()
    {
        NotchModel m;
        QCOMPARE(m.addNotch(NotchModel::kMaxNotchCentreHz + 1.0), -1);
        QVERIFY(m.notches().isEmpty());
    }

    void add_accepts_centre_exactly_at_maximum()
    {
        NotchModel m;
        QVERIFY(m.addNotch(NotchModel::kMaxNotchCentreHz) > 0);
    }

    void constrain_bounds_match_the_vfo_clamp()
    {
        QCOMPARE(NotchModel::kMinNotchCentreHz, 100000.0);
        QCOMPARE(NotchModel::kMaxNotchCentreHz, 61440000.0);
    }

    // ── NotchAdminBusy (Thetis console.cs:40224) ─────────────────────────
    void add_rejected_while_admin_busy()
    {
        NotchModel m;
        m.setAdminBusy(true);
        QVERIFY(m.adminBusy());
        QCOMPARE(m.addNotch(14074000.0), -1);
        QVERIFY(m.notches().isEmpty());
    }

    void add_resumes_when_admin_busy_clears()
    {
        NotchModel m;
        m.setAdminBusy(true);
        QCOMPARE(m.addNotch(14074000.0), -1);
        m.setAdminBusy(false);
        QVERIFY(m.addNotch(14074000.0) > 0);
    }

    // ── §1.2: the Thetis CW-pitch correction is deliberately NOT ported.
    //    NotchModel takes no mode and no rx argument, so the CWU and CWL
    //    calls are literally the same call and both store F exactly.  A
    //    ported GetDSPcwPitchShiftToZero would show up here as F +/- pitch.
    void cw_upper_notch_stores_at_exact_frequency()
    {
        NotchModel m;
        const double f = 7025000.0;
        const int id = m.addNotch(f);
        QCOMPARE(m.notchById(id)->centerHz, f);
    }

    void cw_lower_notch_stores_at_exact_frequency()
    {
        NotchModel m;
        const double f = 7025000.0;
        const int id = m.addNotch(f);
        QCOMPARE(m.notchById(id)->centerHz, f);
    }

    // ── ids are stable and monotonic (AetherSDR TnfEntry::id addition) ───
    void ids_are_monotonic_and_distinct()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        const int c = m.addNotch(14076000.0);
        QVERIFY(a < b);
        QVERIFY(b < c);
        QCOMPARE(m.notchById(a)->id, a);
        QCOMPARE(m.notchById(c)->id, c);
    }

    void notchById_returns_nullptr_for_unknown_id()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QCOMPARE(m.notchById(9999), nullptr);
    }
};

QTEST_MAIN(TestNotchModelGuards)
#include "tst_notch_model_guards.moc"
```

Register it in `tests/CMakeLists.txt`, immediately after the `tst_diversity_dialog_persistence` block that ends at line 5732:

```cmake
# TNF §5.4: NotchModel ported guards. 10 Hz dedupe (exact boundary, strict
# `<` per radio.cs:4267), 200/100 Hz width defaults, whole-Hz midpoint-to-even
# rounding, min/max centre constrain, NotchAdminBusy rejection, wheel-resize
# edge clamp, and the §1.2 CW case (a notch added at F stores at exactly F
# because the Thetis CW-pitch correction is deliberately not ported).
# Source: Thetis console.cs:13221; 33299-33321; 40007-40047; 40050-40120;
#         40222-40280 [v2.10.3.15]; radio.cs:4261-4272 [v2.10.3.15].
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §5.4.
nereus_add_test(tst_notch_model_guards)
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON && cmake --build build --target tst_notch_model_guards`

Expected: FAIL at compile with `fatal error: 'models/NotchModel.h' file not found`.

- [ ] **Step 3: Create `NotchModel.h` (complete §5.3 contract) and the add path in `NotchModel.cpp`**

Create `src/models/NotchModel.h`:

```cpp
// =================================================================
// src/models/NotchModel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/radio.cs (MNotchDB spatial helpers
//     GetFirstNotchThatMatches / NotchNearFreq / NotchesInBW /
//     NotchThatSurroundsFrequencyInBW ,  lines 4246-4325; MNotch value
//     type ,  lines 4328-4360), original licence from Thetis source is
//     included below
//   Project Files/Source/Console/console.cs (NotchAdminBusy guards,
//     ChangeNotchBW, ChangeNotchCentreFrequency, changeNotchActive,
//     removeNotch, AddNotch, notchSidebandShift, notchMouseWheel
//     clamps, _max_filter_width, max_freq ,  lines 13221; 15552;
//     33299-33321; 40007-40047; 40050-40120; 40123-40156; 40198-40219;
//     40222-40280; 40281-40307), original licence from Thetis source is
//     included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-07-29 ,  TNF (tunable notch filter) build order step 3.
//                 Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.  Thetis's index-is-identity MNotchDB gains
//                 a stable monotonic `id` (the AetherSDR TnfModel
//                 addition), which removes the upstream
//                 GetFirstNotchThatMatches selection-recovery dance.
//                 The Thetis CW-pitch correction (console.cs:40228) and
//                 the XVTR min/max override (console.cs:40051-40077) are
//                 deliberately NOT ported; see design §1.2 and §5.4.
// =================================================================
//
// Source attribution (AetherSDR ,  GPL-3.0-or-later):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       ,  per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   The stable-id notch store shape is a port of AetherSDR
//   src/models/TnfModel.{h,cpp} [@c6481cbf].  AetherSDR is licensed
//   under the GNU General Public License v3 or later.  NereusSDR is
//   also GPLv3.  Attribution follows GPLv3 §5 requirements.
//
// =================================================================

// --- From radio.cs ---

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// --- From console.cs ---

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#pragma once

#include "core/dsp/Notch.h"

#include <QList>
#include <QObject>
#include <QString>

namespace NereusSDR {

// Canonical store for operator-placed notches (TNF).  Global, slice-agnostic:
// Thetis fans one notch set to three fixed WDSP channel ids
// (console.cs:40271-40273 [v2.10.3.15]); NereusSDR fans it to every live
// RxChannel.  The list is an ORDERED QList and its position IS the WDSP notch
// index; every mutation keeps the two in lockstep (design §5.2).
class NotchModel : public QObject {
    Q_OBJECT

public:
    explicit NotchModel(QObject* parent = nullptr);
    ~NotchModel() override = default;

    // ── Ported guard constants (design §5.4) ─────────────────────────────

    // From Thetis console.cs:40260 [v2.10.3.15]: "if there is a notch within
    // 10hz ignore"
    static constexpr int kNotchDedupeWindowHz = 10;

    // From Thetis console.cs:40268 [v2.10.3.15] ,  original value 200
    static constexpr double kDefaultNotchWidthHz = 200.0;

    // From Thetis console.cs:40269 [v2.10.3.15] ,  original value 100, taken
    // when the Shift key is down at add time.
    static constexpr double kNarrowNotchWidthHz = 100.0;

    // From Thetis console.cs:13221 [v2.10.3.15] ,  _max_filter_width = 10000
    static constexpr double kMaxNotchWidthHz = 10000.0;

    // Thetis constrains the notch centre to min_freq..max_freq
    // (console.cs:40257 and :40077 [v2.10.3.15], where max_freq = 61.44 at
    // console.cs:15552 [v2.10.3.15]).  NereusSDR has no radio frequency-range
    // capability field yet (design §5.4), so the notch constrain reuses the
    // bounds VfoWidget.cpp:698 already clamps operator frequency entry to.
    static constexpr double kMinNotchCentreHz = 100000.0;
    static constexpr double kMaxNotchCentreHz = 61440000.0;

    // ── Queries ──────────────────────────────────────────────────────────
    const QList<Notch>& notches() const { return m_notches; }
    const Notch*        notchById(int id) const;
    int                 indexOfId(int id) const;

    bool globalEnabled() const { return m_globalEnabled; }
    bool autoIncrease()  const { return m_autoIncrease; }
    bool visualEnabled() const { return m_visualEnabled; }
    bool adminBusy()     const { return m_adminBusy; }

    // ── Thetis-ported spatial helpers [v2.10.3.15] ───────────────────────
    //   NotchNearFreq                     radio.cs:4261-4272
    //   NotchesInBW                       radio.cs:4276-4293
    //   NotchThatSurroundsFrequencyInBW   radio.cs:4297-4325
    bool         notchNearFreq(double hz, int deltaHz) const;
    QList<Notch> notchesInBandwidth(double centreHz, int lowHz, int highHz) const;
    const Notch* notchSurrounding(double centreHz, int lowHz, int highHz,
                                  double hz, int padWidthHz = 0) const;

    // Pure helper for the +TNF button.  Static because a global,
    // slice-agnostic NotchModel cannot reach per-slice filter edges; the
    // caller supplies the active slice's edges.  Upstream reads them off the
    // DSP object (console.cs:40289-40295 [v2.10.3.15]).
    static int notchSidebandShift(int filterLowHz, int filterHighHz);

    // ── Mutations ────────────────────────────────────────────────────────
    int  addNotch(double centerHz, double widthHz = kDefaultNotchWidthHz);
    bool setCenter(int id, double centerHz);
    bool setWidth(int id, double widthHz);
    bool setActive(int id, bool active);
    bool removeNotch(int id);

    void setGlobalEnabled(bool on);
    void setAutoIncrease(bool on);
    void setVisualEnabled(bool on);

    // Settings-page edit lock.  From Thetis SetupForm.NotchAdminBusy
    // (console.cs:40009 [v2.10.3.15]) ,  while the MNF page is mid-edit every
    // panadapter-side mutation is a no-op, so a drag cannot reorder the list
    // underneath the table's index mapping.
    void setAdminBusy(bool busy);

    void clear();

    // ── Persistence (AppSettings, global scope; design §5.5) ─────────────
    void saveToSettings() const;
    void restoreFromSettings();

signals:
    void notchAdded(int id);
    void notchChanged(int id);
    // formerIndex is the positional (WDSP) index the entry occupied; it is
    // gone from the list by the time the signal lands, and the fan-out needs
    // it for RXANBPDeleteNotch.
    void notchRemoved(int id, int formerIndex);
    void globalEnabledChanged(bool on);
    void autoIncreaseChanged(bool on);
    void visualEnabledChanged(bool on);
    void notchAddRejected(const QString& reason);
    // Whole-list replacement.  clear() and restoreFromSettings() emit this;
    // RadioModel handles it as syncNotches({}) / syncNotches(notches()) on
    // every channel (design §5.3 clear() contract).
    void notchesReset();

private:
    // Main-thread-only state.  WDSP owns the authoritative notch database
    // (From WDSP RXA.c:85-88); this list is the operator-facing source of
    // truth and is never read from the audio thread, so no atomics.
    QList<Notch> m_notches;
    int  m_nextId{1};

    // Defaults to on, matching AetherSDR src/models/TnfModel.h:52
    // [@c6481cbf].  On a first run the list is empty so the flag is
    // behaviourally inert; on every later run it comes from AppSettings.
    bool m_globalEnabled{true};

    // WDSP creates nbp0 with autoincr = 1 (From WDSP RXA.c:105), so the §9
    // control starts ON, not OFF.
    bool m_autoIncrease{true};

    // Thetis chkVisualNotch carries no designer Checked assignment
    // (setup.designer.cs:44167-44179 [v2.10.3.15]), so WinForms leaves it
    // unchecked.
    bool m_visualEnabled{false};

    bool m_adminBusy{false};
};

}  // namespace NereusSDR
```

Create `src/models/NotchModel.cpp` by copying the verified attribution block byte-for-byte off the header, then appending the implementation:

```bash
awk '/^#pragma once/{exit} {print}' src/models/NotchModel.h \
  | sed 's|src/models/NotchModel\.h  (NereusSDR)|src/models/NotchModel.cpp  (NereusSDR)|' \
  > src/models/NotchModel.cpp
```

Then append to `src/models/NotchModel.cpp`:

```cpp
#include "NotchModel.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"

#include <QLatin1String>
#include <QVariant>

#include <cmath>

namespace NereusSDR {

namespace {

// Boolean → AppSettings canonical string.  Mirrors SliceModel.cpp:1594.
QString boolStr(bool v)
{
    return v ? QStringLiteral("True") : QStringLiteral("False");
}

bool boolFrom(const QVariant& v)
{
    return v.toString() == QLatin1String("True");
}

}  // namespace

NotchModel::NotchModel(QObject* parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

int NotchModel::indexOfId(int id) const
{
    for (int i = 0; i < m_notches.size(); ++i) {
        if (m_notches.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

const Notch* NotchModel::notchById(int id) const
{
    const int index = indexOfId(id);
    if (index < 0) {
        return nullptr;
    }
    return &m_notches.at(index);
}

//MW0LGE check if notch close by
// From Thetis radio.cs:4261-4272 [v2.10.3.15] ,  MNotchDB.NotchNearFreq.
// Strict `<` at radio.cs:4267, so a notch exactly deltaHz away does NOT
// block an add.
bool NotchModel::notchNearFreq(double hz, int deltaHz) const
{
    for (const Notch& n : m_notches) {
        if (std::abs(hz - n.centerHz) < deltaHz) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Mutations
// ---------------------------------------------------------------------------

// From Thetis console.cs:40222-40280 [v2.10.3.15] ,  AddNotch(fFreqHZ, sourceRX).
// Guard order is upstream's: admin-busy, round, constrain, dedupe.
// Deliberately NOT ported: console.cs:40228 (GetDSPcwPitchShiftToZero) and the
// XVTR min/max override at console.cs:40232-40254.  See design §1.2 and §5.4.
int NotchModel::addNotch(double centerHz, double widthHz)
{
    // From Thetis console.cs:40224 [v2.10.3.15]
    if (m_adminBusy) { // dont add if using add/edit on the setup form
        emit notchAddRejected(
            QStringLiteral("The MNF settings page is mid-edit"));
        return -1;
    }

    // From Thetis console.cs:40230 [v2.10.3.15].  C# Math.Round(double) is
    // MidpointRounding.ToEven, which is what std::nearbyint does under the
    // default FE_TONEAREST rounding mode.
    centerHz = std::nearbyint(centerHz); //[2.10.3.7]MW0LGE moved from below

    //constrain
    // From Thetis console.cs:40256-40257 [v2.10.3.15]
    if (centerHz < kMinNotchCentreHz || centerHz > kMaxNotchCentreHz) {
        emit notchAddRejected(
            QStringLiteral("Frequency is outside the radio tuning range"));
        return -1;
    }

    // From Thetis console.cs:40259-40260 [v2.10.3.15]
    // if there is a notch within 10hz ignore
    if (notchNearFreq(centerHz, kNotchDedupeWindowHz)) {
        emit notchAddRejected(
            QStringLiteral("A notch already exists within 10 Hz"));
        return -1;
    }

    // Design §5.2: append at position n, so the list position IS the WDSP
    // notch index the fan-out will pass to RXANBPAddNotch.
    Notch n;
    n.id       = m_nextId++;
    n.centerHz = centerHz;
    n.widthHz  = widthHz;
    n.active   = true;
    m_notches.append(n);

    emit notchAdded(n.id);
    return n.id;
}

void NotchModel::setAdminBusy(bool busy)
{
    m_adminBusy = busy;
}

}  // namespace NereusSDR
```

Add the source to `CMakeLists.txt`, after line 667 (`src/models/FilterPresetStore.cpp`):

```cmake
    src/models/NotchModel.cpp
```

Insert two rows into `docs/attribution/THETIS-PROVENANCE.md` between line 315 (`src/models/BandDefaults.cpp`) and line 316 (`src/models/PanadapterModel.cpp`):

```
| src/models/NotchModel.h | Project Files/Source/Console/radio.cs; Project Files/Source/Console/console.cs | 4246-4325; 4328-4360; 13221; 15552; 33299-33321; 40007-40047; 40050-40120; 40123-40156; 40198-40219; 40222-40280; 40281-40307 | port | multi-source | TNF canonical notch store. MNotchDB spatial helpers (NotchNearFreq / NotchesInBW / NotchThatSurroundsFrequencyInBW) + MNotch value type from radio.cs; NotchAdminBusy guards, whole-Hz midpoint-to-even rounding, min/max centre constrain, 10 Hz dedupe, 200/100 Hz width defaults, wheel-resize edge clamp and notchSidebandShift from console.cs. Both source-file headers preserved verbatim with `// --- From <file> ---` separators (HOW-TO-PORT.md §multi-source). Thetis index-is-identity replaced by a stable monotonic id (AetherSDR TnfModel [@c6481cbf]), which retires the GetFirstNotchThatMatches selection-recovery dance. CW-pitch correction (console.cs:40228) and XVTR min/max override (console.cs:40051-40077) deliberately NOT ported per design §1.2 / §5.4 |
| src/models/NotchModel.cpp | Project Files/Source/Console/radio.cs; Project Files/Source/Console/console.cs | 4246-4325; 4328-4360; 13221; 15552; 33299-33321; 40007-40047; 40050-40120; 40123-40156; 40198-40219; 40222-40280; 40281-40307 | port | multi-source | implementation pairs with NotchModel.h |
```

Insert two rows into `docs/attribution/aethersdr-reconciliation.md` in the Models table, after line 88 (the `|---|---|---|---|` separator):

```
| `src/models/NotchModel.h` | `src/models/TnfModel.{h,cpp}` | Header attribution block names TnfModel [@c6481cbf]. `Notch::id` is the AetherSDR `TnfEntry::id` addition (`TnfModel.h:9`); `m_globalEnabled{true}` default matches `TnfModel.h:52`. | "Stable-id notch-store shape ported from AetherSDR `src/models/TnfModel.{h,cpp}` [@c6481cbf]. `TnfEntry::depthDb` and `::permanent` dropped (SmartSDR capabilities with no WDSP equivalent, design §1.2); notch geometry, guards and spatial helpers are Thetis, see Copyright block." |
| `src/models/NotchModel.cpp` | `src/models/TnfModel.cpp` | Same. | Same as NotchModel.h above. |
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_model_guards && ctest --test-dir build -R '^tst_notch_model_guards$' --output-on-failure`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/models/NotchModel.h src/models/NotchModel.cpp CMakeLists.txt \
        tests/tst_notch_model_guards.cpp tests/CMakeLists.txt \
        docs/attribution/THETIS-PROVENANCE.md \
        docs/attribution/aethersdr-reconciliation.md
git commit -m "feat(tnf): NotchModel add-path guards ported from Thetis AddNotch"
```

---

- [ ] **Step 6: Extend the guards test to the edit path**

Append these slots to `tests/tst_notch_model_guards.cpp`, inside the existing `private slots:` block, before the closing `};`:

```cpp
    // ── setCenter: constrain, admin-busy, whole-Hz rounding
    //    (Thetis console.cs:40077, :40079, :40081) ────────────────────────
    void setCenter_rounds_to_whole_hz()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QVERIFY(m.setCenter(id, 14074500.4));
        QCOMPARE(m.notchById(id)->centerHz, 14074500.0);
    }

    void setCenter_rejects_below_minimum()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QVERIFY(!m.setCenter(id, NotchModel::kMinNotchCentreHz - 1.0));
        QCOMPARE(m.notchById(id)->centerHz, 14074000.0);
    }

    void setCenter_rejects_above_maximum()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QVERIFY(!m.setCenter(id, NotchModel::kMaxNotchCentreHz + 1.0));
        QCOMPARE(m.notchById(id)->centerHz, 14074000.0);
    }

    void setCenter_rejected_while_admin_busy()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        m.setAdminBusy(true);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        QVERIFY(!m.setCenter(id, 14075000.0));
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m.notchById(id)->centerHz, 14074000.0);
    }

    void setCenter_emits_notchChanged_once_on_change()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        QVERIFY(m.setCenter(id, 14075000.0));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), id);
    }

    void setCenter_is_silent_when_value_unchanged()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        // Upstream returns true whenever the index resolves, and only fires
        // its change handler when the value actually moved
        // (console.cs:40109-40113 [v2.10.3.15]).
        QVERIFY(m.setCenter(id, 14074000.0));
        QCOMPARE(spy.count(), 0);
    }

    void setCenter_rejects_unknown_id()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QVERIFY(!m.setCenter(9999, 14075000.0));
    }

    // ── setWidth: wheel-resize clamps (Thetis console.cs:33313-33318) ────
    void setWidth_clamps_to_max_filter_width()
    {
        NotchModel m;
        const int id = m.addNotch(1000000.0);
        QVERIFY(m.setWidth(id, 20000.0));
        QCOMPARE(m.notchById(id)->widthHz, NotchModel::kMaxNotchWidthHz);
        QCOMPARE(NotchModel::kMaxNotchWidthHz, 10000.0);
    }

    void setWidth_clamps_negative_to_zero()
    {
        NotchModel m;
        const int id = m.addNotch(1000000.0);
        QVERIFY(m.setWidth(id, -50.0));
        QCOMPARE(m.notchById(id)->widthHz, 0.0);
    }

    void setWidth_rejects_when_upper_edge_leaves_the_range()
    {
        // Thetis rejects outright rather than clamping the width down:
        // "check to see if outside frequency limits" (console.cs:33316-33318).
        NotchModel m;
        const int id = m.addNotch(NotchModel::kMaxNotchCentreHz - 1000.0);
        QVERIFY(!m.setWidth(id, 5000.0));
        QCOMPARE(m.notchById(id)->widthHz, NotchModel::kDefaultNotchWidthHz);
    }

    void setWidth_accepts_when_upper_edge_stays_inside_the_range()
    {
        NotchModel m;
        const int id = m.addNotch(NotchModel::kMaxNotchCentreHz - 5000.0);
        QVERIFY(m.setWidth(id, 8000.0));
        QCOMPARE(m.notchById(id)->widthHz, 8000.0);
    }

    void setWidth_rejected_while_admin_busy()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        m.setAdminBusy(true);
        QVERIFY(!m.setWidth(id, 400.0));
        QCOMPARE(m.notchById(id)->widthHz, NotchModel::kDefaultNotchWidthHz);
    }

    void setWidth_emits_notchChanged_once_on_change()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        QVERIFY(m.setWidth(id, 400.0));
        QCOMPARE(spy.count(), 1);
    }

    // ── setActive (Thetis console.cs:40123-40156) ────────────────────────
    void setActive_toggles_and_emits()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        QVERIFY(m.setActive(id, false));
        QVERIFY(!m.notchById(id)->active);
        QCOMPARE(spy.count(), 1);
    }

    void setActive_is_silent_when_value_unchanged()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        QVERIFY(m.setActive(id, true));
        QCOMPARE(spy.count(), 0);
    }

    void setActive_rejected_while_admin_busy()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        m.setAdminBusy(true);
        QVERIFY(!m.setActive(id, false));
        QVERIFY(m.notchById(id)->active);
    }
```

- [ ] **Step 7: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_model_guards`

Expected: FAIL at link with `Undefined symbols ... NereusSDR::NotchModel::setCenter(int, double)`, `...::setWidth(int, double)`, `...::setActive(int, bool)`.

- [ ] **Step 8: Implement the three edit-path mutators**

Append to `src/models/NotchModel.cpp`, inside `namespace NereusSDR`, after `addNotch`:

```cpp
// From Thetis console.cs:40050-40120 [v2.10.3.15] , 
// ChangeNotchCentreFrequency(notch, newCentreFrequencyHz, sourceRX).
// Upstream orders the guards constrain-then-admin-busy-then-round; preserved.
// The XVTR override at console.cs:40054-40074 is deliberately NOT ported
// (design §5.4: NereusSDR has no transverter frequency lookup to route it
// through, and XVTR band clicks are rejected upstream of here anyway).
bool NotchModel::setCenter(int id, double centerHz)
{
    //constrain
    // From Thetis console.cs:40076-40077 [v2.10.3.15]
    if (centerHz < kMinNotchCentreHz || centerHz > kMaxNotchCentreHz) {
        return false;
    }

    // From Thetis console.cs:40079 [v2.10.3.15]
    if (m_adminBusy) { // cant change it if setup is adding/editing
        return false;
    }

    // From Thetis console.cs:40081 [v2.10.3.15]
    centerHz = std::nearbyint(centerHz);

    const int index = indexOfId(id);
    if (index < 0) {
        return false;
    }

    // Upstream returns true whenever the index resolved and fires its change
    // handler only when the value actually moved (console.cs:40109-40113).
    if (m_notches.at(index).centerHz == centerHz) {
        return true;
    }
    m_notches[index].centerHz = centerHz;
    emit notchChanged(id);
    return true;
}

// From Thetis console.cs:40007-40047 [v2.10.3.15] ,  ChangeNotchBW(notch,
// newWidth, notch_index), composed with the clamps its only interactive
// caller applies first (notchMouseWheel, console.cs:33313-33318).  Folding
// them in here is what lets the panadapter wheel handler be a bare
// setWidth(id, current + delta * step) call.
bool NotchModel::setWidth(int id, double widthHz)
{
    // From Thetis console.cs:40009 [v2.10.3.15]
    if (m_adminBusy) { // cant change it if setup is adding/editing
        return false;
    }

    const int index = indexOfId(id);
    if (index < 0) {
        return false;
    }

    // From Thetis console.cs:33313-33314 [v2.10.3.15]
    if (widthHz < 0.0) {
        widthHz = 0.0;
    }
    if (widthHz > kMaxNotchWidthHz) {
        widthHz = kMaxNotchWidthHz;
    }

    // check to see if outside frequency limits
    // From Thetis console.cs:33316-33318 [v2.10.3.15].  A width whose edges
    // would leave the range is rejected outright, not clamped down.  The
    // lower-edge arm is unreachable on this tree (kMinNotchCentreHz 100 kHz
    // minus half of kMaxNotchWidthHz is still 95 kHz), unlike Thetis where
    // min_freq is 0.0; ported verbatim rather than dropped.
    const double centreHz = m_notches.at(index).centerHz;
    if (centreHz - (widthHz / 2) < 0) {
        return false;
    }
    if (centreHz + (widthHz / 2) > kMaxNotchCentreHz) {
        return false;
    }

    if (m_notches.at(index).widthHz == widthHz) {
        return true;
    }
    m_notches[index].widthHz = widthHz;
    emit notchChanged(id);
    return true;
}

// From Thetis console.cs:40123-40156 [v2.10.3.15] , 
// changeNotchActive(notch, bActive).
bool NotchModel::setActive(int id, bool active)
{
    // From Thetis console.cs:40125 [v2.10.3.15]
    if (m_adminBusy) { // cant change it if setup is adding/editing
        return false;
    }

    const int index = indexOfId(id);
    if (index < 0) {
        return false;
    }

    if (m_notches.at(index).active == active) {
        return true;
    }
    m_notches[index].active = active;
    emit notchChanged(id);
    return true;
}
```

- [ ] **Step 9: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_model_guards && ctest --test-dir build -R '^tst_notch_model_guards$' --output-on-failure`

Expected: PASS

- [ ] **Step 10: Commit**

```bash
git add src/models/NotchModel.cpp tests/tst_notch_model_guards.cpp
git commit -m "feat(tnf): NotchModel edit-path guards (centre, width, active)"
```

---

- [ ] **Step 11: Write the failing index-invariant test**

Create `tests/tst_notch_model_index_invariant.cpp`:

```cpp
// =================================================================
// tests/tst_notch_model_index_invariant.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF §5.2: list position IS the WDSP notch index. Verified across add,
// edit and delete, including deleting from the middle, and verified that
// stable ids survive a mutation that shifts every later index down (the
// AetherSDR addition that retires Thetis's GetFirstNotchThatMatches
// selection-recovery dance).
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §5.2.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/NotchModel.h"

using namespace NereusSDR;

class TestNotchModelIndexInvariant : public QObject {
    Q_OBJECT

private slots:
    void add_appends_at_the_end()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        const int c = m.addNotch(14076000.0);

        QCOMPARE(m.notches().size(), 3);
        QCOMPARE(m.indexOfId(a), 0);
        QCOMPARE(m.indexOfId(b), 1);
        QCOMPARE(m.indexOfId(c), 2);
        QCOMPARE(m.notches().at(0).id, a);
        QCOMPARE(m.notches().at(2).id, c);
    }

    void add_out_of_frequency_order_still_appends_in_call_order()
    {
        // The list is index-ordered, not frequency-sorted: position must be
        // the WDSP index, and RXANBPAddNotch is an insert at position n.
        NotchModel m;
        const int a = m.addNotch(14076000.0);
        const int b = m.addNotch(14074000.0);
        QCOMPARE(m.indexOfId(a), 0);
        QCOMPARE(m.indexOfId(b), 1);
    }

    void delete_from_the_middle_shifts_later_indices_down()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        const int c = m.addNotch(14076000.0);

        QVERIFY(m.removeNotch(b));

        QCOMPARE(m.notches().size(), 2);
        QCOMPARE(m.indexOfId(a), 0);
        QCOMPARE(m.indexOfId(c), 1);
        QCOMPARE(m.indexOfId(b), -1);
        QCOMPARE(m.notchById(b), nullptr);
    }

    void delete_reports_the_former_index()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        m.addNotch(14076000.0);

        QSignalSpy spy(&m, &NotchModel::notchRemoved);
        QVERIFY(m.removeNotch(b));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), b);
        QCOMPARE(spy.first().at(1).toInt(), 1);
    }

    void ids_survive_a_middle_delete()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        const int c = m.addNotch(14076000.0);

        QVERIFY(m.removeNotch(b));

        // c kept its id even though its WDSP index moved from 2 to 1.
        QCOMPARE(m.notchById(c)->id, c);
        QCOMPARE(m.notchById(c)->centerHz, 14076000.0);
    }

    void edit_after_a_middle_delete_targets_the_shifted_index()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        const int c = m.addNotch(14076000.0);

        QVERIFY(m.removeNotch(b));
        QVERIFY(m.setCenter(c, 14077000.0));

        QCOMPARE(m.indexOfId(c), 1);
        QCOMPARE(m.notches().at(1).centerHz, 14077000.0);
        QCOMPARE(m.notches().at(0).centerHz, 14074000.0);
        QCOMPARE(m.indexOfId(a), 0);
    }

    void add_after_a_delete_appends_at_the_new_end()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        QVERIFY(m.removeNotch(a));

        const int c = m.addNotch(14076000.0);
        QCOMPARE(m.indexOfId(b), 0);
        QCOMPARE(m.indexOfId(c), 1);
        QVERIFY(c != a);
        QVERIFY(c != b);
    }

    void delete_rejects_unknown_id()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchRemoved);
        QVERIFY(!m.removeNotch(9999));
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m.notches().size(), 1);
    }

    void delete_rejected_while_admin_busy()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        m.setAdminBusy(true);
        QVERIFY(!m.removeNotch(a));
        QCOMPARE(m.notches().size(), 1);
    }

    void indexOfId_returns_minus_one_on_empty_model()
    {
        NotchModel m;
        QCOMPARE(m.indexOfId(1), -1);
    }
};

QTEST_MAIN(TestNotchModelIndexInvariant)
#include "tst_notch_model_index_invariant.moc"
```

Register it in `tests/CMakeLists.txt`, after the `tst_notch_model_guards` block:

```cmake
# TNF §5.2: the index invariant. List position tracks the WDSP notch index
# across add / edit / delete including deleting from the middle, and stable
# ids survive an index shift (the AetherSDR TnfEntry::id addition that
# retires Thetis's GetFirstNotchThatMatches selection recovery).
# Source: Thetis console.cs:40198-40219; 40262-40266 [v2.10.3.15].
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §5.2.
nereus_add_test(tst_notch_model_index_invariant)
```

- [ ] **Step 12: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_model_index_invariant`

Expected: FAIL at link with `Undefined symbols ... NereusSDR::NotchModel::removeNotch(int)`.

- [ ] **Step 13: Implement `removeNotch`**

Append to `src/models/NotchModel.cpp`, inside `namespace NereusSDR`, after `setActive`:

```cpp
// From Thetis console.cs:40198-40219 [v2.10.3.15] ,  removeNotch(notch).
// WDSP shifts its own notch array down inside RXANBPDeleteNotch, so erasing
// at the same position keeps the two in lockstep (design §5.2).
bool NotchModel::removeNotch(int id)
{
    // From Thetis console.cs:40200 [v2.10.3.15]
    if (m_adminBusy) { // cant remove it if setup is adding/editing
        return false;
    }

    const int index = indexOfId(id);
    if (index < 0) {
        return false;
    }

    m_notches.removeAt(index);
    emit notchRemoved(id, index);
    return true;
}
```

- [ ] **Step 14: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_model_index_invariant && ctest --test-dir build -R '^tst_notch_model_index_invariant$' --output-on-failure`

Expected: PASS

- [ ] **Step 15: Commit**

```bash
git add src/models/NotchModel.cpp tests/tst_notch_model_index_invariant.cpp tests/CMakeLists.txt
git commit -m "feat(tnf): NotchModel removeNotch keeps list position on the WDSP index"
```

---

- [ ] **Step 16: Write the failing spatial-helpers test**

Create `tests/tst_notch_spatial_helpers.cpp`:

```cpp
// =================================================================
// tests/tst_notch_spatial_helpers.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF §5.3: the MNotchDB spatial helpers ported from Thetis radio.cs.
// notchesInBandwidth uses INCLUSIVE edge overlap (radio.cs:4286); the
// notchSurrounding pad applies only when FWidth < padWidth * 2
// (radio.cs:4310), and the first match in list order wins.
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §5.3.
// =================================================================

#include <QtTest/QtTest>
#include "models/NotchModel.h"

using namespace NereusSDR;

class TestNotchSpatialHelpers : public QObject {
    Q_OBJECT

private slots:
    // ── notchNearFreq (radio.cs:4261-4272; strict `<` at :4267) ──────────
    void nearFreq_false_on_empty_model()
    {
        NotchModel m;
        QVERIFY(!m.notchNearFreq(14074000.0, 10));
    }

    void nearFreq_true_inside_the_window()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QVERIFY(m.notchNearFreq(14074009.0, 10));
        QVERIFY(m.notchNearFreq(14073991.0, 10));
        QVERIFY(m.notchNearFreq(14074000.0, 10));
    }

    void nearFreq_false_exactly_at_the_window_edge()
    {
        // Strict `<`: |delta| == deltaHz is NOT "near".
        NotchModel m;
        m.addNotch(14074000.0);
        QVERIFY(!m.notchNearFreq(14074010.0, 10));
        QVERIFY(!m.notchNearFreq(14073990.0, 10));
    }

    // ── notchesInBandwidth (radio.cs:4276-4293; inclusive at :4286) ──────
    void inBandwidth_returns_notch_fully_inside()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        const QList<Notch> l = m.notchesInBandwidth(14074000.0, -3000, 3000);
        QCOMPARE(l.size(), 1);
        QCOMPARE(l.first().id, id);
    }

    void inBandwidth_excludes_notch_outside()
    {
        NotchModel m;
        m.addNotch(14090000.0);
        QVERIFY(m.notchesInBandwidth(14074000.0, -3000, 3000).isEmpty());
    }

    void inBandwidth_includes_notch_whose_upper_edge_touches_the_low_bound()
    {
        // min = 14074000 - 3000 = 14071000.  A 200 Hz notch centred at
        // 14070900 has its upper edge at exactly 14071000, and radio.cs:4286
        // tests `>= min`, so it IS included.
        NotchModel m;
        const int id = m.addNotch(14070900.0);
        const QList<Notch> l = m.notchesInBandwidth(14074000.0, -3000, 3000);
        QCOMPARE(l.size(), 1);
        QCOMPARE(l.first().id, id);
    }

    void inBandwidth_includes_notch_whose_lower_edge_touches_the_high_bound()
    {
        // max = 14074000 + 3000 = 14077000.  A 200 Hz notch centred at
        // 14077100 has its lower edge at exactly 14077000 (`<= max`).
        NotchModel m;
        const int id = m.addNotch(14077100.0);
        const QList<Notch> l = m.notchesInBandwidth(14074000.0, -3000, 3000);
        QCOMPARE(l.size(), 1);
        QCOMPARE(l.first().id, id);
    }

    void inBandwidth_excludes_notch_one_hz_beyond_the_high_bound()
    {
        NotchModel m;
        m.addNotch(14077101.0);
        QVERIFY(m.notchesInBandwidth(14074000.0, -3000, 3000).isEmpty());
    }

    void inBandwidth_preserves_list_order()
    {
        NotchModel m;
        const int a = m.addNotch(14075000.0);
        const int b = m.addNotch(14073000.0);
        const QList<Notch> l = m.notchesInBandwidth(14074000.0, -3000, 3000);
        QCOMPARE(l.size(), 2);
        QCOMPARE(l.at(0).id, a);
        QCOMPARE(l.at(1).id, b);
    }

    // ── notchSurrounding (radio.cs:4297-4325) ────────────────────────────
    void surrounding_hits_inside_the_notch()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);   // 200 Hz wide
        const Notch* n = m.notchSurrounding(14074000.0, -3000, 3000, 14074050.0);
        QVERIFY(n != nullptr);
        QCOMPARE(n->id, id);
    }

    void surrounding_hits_exactly_on_the_notch_edges()
    {
        NotchModel m;
        m.addNotch(14074000.0);   // edges at 14073900 / 14074100
        QVERIFY(m.notchSurrounding(14074000.0, -3000, 3000, 14073900.0) != nullptr);
        QVERIFY(m.notchSurrounding(14074000.0, -3000, 3000, 14074100.0) != nullptr);
    }

    void surrounding_misses_just_outside_the_notch()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14074101.0), nullptr);
    }

    void surrounding_misses_when_the_notch_is_outside_the_bandwidth()
    {
        NotchModel m;
        m.addNotch(14090000.0);
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14090000.0), nullptr);
    }

    void surrounding_pad_widens_a_notch_narrower_than_twice_the_pad()
    {
        // width 200 < padWidth 150 * 2 = 300, so the pad applies: edges
        // become 14073750 / 14074250.
        NotchModel m;
        m.addNotch(14074000.0);
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14074200.0), nullptr);
        QVERIFY(m.notchSurrounding(14074000.0, -3000, 3000, 14074200.0, 150) != nullptr);
        QVERIFY(m.notchSurrounding(14074000.0, -3000, 3000, 14074250.0, 150) != nullptr);
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14074251.0, 150), nullptr);
    }

    void surrounding_pad_does_not_apply_to_a_wide_notch()
    {
        // width 400 is NOT < padWidth 150 * 2 = 300, so radio.cs:4310 leaves
        // the edges alone: 14073800 / 14074200.
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QVERIFY(m.setWidth(id, 400.0));
        QVERIFY(m.notchSurrounding(14074000.0, -3000, 3000, 14074200.0, 150) != nullptr);
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14074250.0, 150), nullptr);
    }

    void surrounding_returns_the_first_match_in_list_order()
    {
        // Two overlapping notches; the one added first must win.
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14074100.0);
        QVERIFY(a != b);
        const Notch* n = m.notchSurrounding(14074000.0, -3000, 3000, 14074050.0);
        QVERIFY(n != nullptr);
        QCOMPARE(n->id, a);
    }

    void surrounding_returns_nullptr_on_empty_model()
    {
        NotchModel m;
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14074000.0), nullptr);
    }
};

QTEST_MAIN(TestNotchSpatialHelpers)
#include "tst_notch_spatial_helpers.moc"
```

Register it in `tests/CMakeLists.txt`, after the index-invariant block:

```cmake
# TNF §5.3: MNotchDB spatial helpers ported from Thetis radio.cs.
# notchNearFreq strict `<` (radio.cs:4267), notchesInBandwidth inclusive
# edge overlap (radio.cs:4286), notchSurrounding with and without
# padWidthHz covering the pad-applies-only-when-FWidth<padWidth*2 branch
# (radio.cs:4310), first-found-in-list-order.
# Source: Thetis radio.cs:4261-4272; 4276-4293; 4297-4325 [v2.10.3.15].
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §5.3.
nereus_add_test(tst_notch_spatial_helpers)
```

- [ ] **Step 17: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_spatial_helpers`

Expected: FAIL at link with `Undefined symbols ... NereusSDR::NotchModel::notchesInBandwidth(double, int, int) const`, `...::notchSurrounding(double, int, int, double, int) const`.

- [ ] **Step 18: Implement the two remaining spatial helpers**

Append to `src/models/NotchModel.cpp`, inside `namespace NereusSDR`, after `notchNearFreq`:

```cpp
//MW0LGE return list of notches in given bandwidth
//notch is included if filter width is enough to be within the BW
// From Thetis radio.cs:4276-4293 [v2.10.3.15] ,  MNotchDB.NotchesInBW.
// Overlap test at radio.cs:4286 is inclusive on both sides.
QList<Notch> NotchModel::notchesInBandwidth(double centreHz,
                                            int lowHz, int highHz) const
{
    QList<Notch> l;
    const double min = centreHz + lowHz;
    const double max = centreHz + highHz;

    for (const Notch& n : m_notches) {
        if (((n.centerHz + n.widthHz / 2) >= min)
            && ((n.centerHz - n.widthHz / 2) <= max)) {
            l.append(n);
        }
    }

    return l;
}

//MW0LGE return first notch found that surrounds a given frequency in the given bandwidth
// From Thetis radio.cs:4297-4325 [v2.10.3.15] , 
// MNotchDB.NotchThatSurroundsFrequencyInBW.
// Upstream materialises NotchesInBW() and walks the copy.  NereusSDR folds
// the same predicate into one pass over m_notches so the returned pointer
// stays valid; iteration order is identical because NotchesInBW preserves
// list order.
const Notch* NotchModel::notchSurrounding(double centreHz, int lowHz,
                                          int highHz, double hz,
                                          int padWidthHz) const
{
    const double min = centreHz + lowHz;
    const double max = centreHz + highHz;

    for (int i = 0; i < m_notches.size(); ++i) {
        const Notch& n = m_notches.at(i);

        if (!(((n.centerHz + n.widthHz / 2) >= min)
              && ((n.centerHz - n.widthHz / 2) <= max))) {
            continue;
        }

        double dLf = n.centerHz - n.widthHz / 2;
        double dHf = n.centerHz + n.widthHz / 2;

        if (n.widthHz < (padWidthHz * 2)) {
            dLf -= padWidthHz;
            dHf += padWidthHz;
        }

        if (hz >= dLf && hz <= dHf) {
            return &m_notches.at(i);
        }
    }

    return nullptr;
}
```

- [ ] **Step 19: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_spatial_helpers && ctest --test-dir build -R '^tst_notch_spatial_helpers$' --output-on-failure`

Expected: PASS

- [ ] **Step 20: Commit**

```bash
git add src/models/NotchModel.cpp tests/tst_notch_spatial_helpers.cpp tests/CMakeLists.txt
git commit -m "feat(tnf): port MNotchDB spatial helpers onto NotchModel"
```

---

- [ ] **Step 21: Write the failing sideband-shift test**

Create `tests/tst_notch_sideband_shift.cpp`:

```cpp
// =================================================================
// tests/tst_notch_sideband_shift.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF: NotchModel::notchSidebandShift, ported from Thetis
// console.cs:40281-40307 notchSidebandShift(rx). Table-driven.
//
// The sign is the whole point. A dropped negation puts every LSB +TNF
// notch at VFO + 1500 instead of VFO - 1500 -- outside the passband,
// silent on LSB, and perfectly correct on USB, so it is invisible
// without this test.
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §5.3.
// =================================================================

#include <QtTest/QtTest>
#include "models/NotchModel.h"

using namespace NereusSDR;

class TestNotchSidebandShift : public QObject {
    Q_OBJECT

private slots:
    void shift_data()
    {
        QTest::addColumn<int>("filterLowHz");
        QTest::addColumn<int>("filterHighHz");
        QTest::addColumn<int>("expected");

        // Upper sideband: 200 + ((2800 - 200) / 2) = 200 + 1300 = 1500
        QTest::newRow("USB 200/2800")      << 200   << 2800  << 1500;
        // Lower sideband: -2800 + ((-200 - -2800) / 2) = -2800 + 1300 = -1500
        QTest::newRow("LSB -2800/-200")    << -2800 << -200  << -1500;
        // Symmetric filter (AM): middle == 0 hits the fallback at
        // console.cs:40294-40295 -> highHz / 2 = 3000 / 2 = 1500
        QTest::newRow("AM -3000/+3000")    << -3000 << 3000  << 1500;
        // Narrow CW-width symmetric filter, same fallback: 250 / 2 = 125
        QTest::newRow("symmetric -250/250") << -250 << 250   << 125;
        // Narrow USB: 300 + ((1500 - 300) / 2) = 300 + 600 = 900
        QTest::newRow("USB 300/1500")      << 300   << 1500  << 900;
        // Narrow LSB: -1500 + ((-300 - -1500) / 2) = -1500 + 600 = -900
        QTest::newRow("LSB -1500/-300")    << -1500 << -300  << -900;
        // Wide DIGU: 0 + ((3000 - 0) / 2) = 1500
        QTest::newRow("DIGU 0/3000")       << 0     << 3000  << 1500;
        // C# integer division truncates toward zero; C++ matches.
        // 100 + ((2801 - 100) / 2) = 100 + 1350 = 1450
        QTest::newRow("odd span 100/2801") << 100   << 2801  << 1450;
    }

    void shift()
    {
        QFETCH(int, filterLowHz);
        QFETCH(int, filterHighHz);
        QFETCH(int, expected);
        QCOMPARE(NotchModel::notchSidebandShift(filterLowHz, filterHighHz),
                 expected);
    }

    void lsb_shift_is_negative()
    {
        // Guards the sign specifically, independent of the table above.
        QVERIFY(NotchModel::notchSidebandShift(-2800, -200) < 0);
        QVERIFY(NotchModel::notchSidebandShift(200, 2800) > 0);
    }

    void shift_is_callable_without_an_instance()
    {
        // Static by design: a global, slice-agnostic NotchModel cannot reach
        // per-slice filter edges, so the caller supplies them.
        QCOMPARE(NotchModel::notchSidebandShift(200, 2800), 1500);
    }
};

QTEST_MAIN(TestNotchSidebandShift)
#include "tst_notch_sideband_shift.moc"
```

Register it in `tests/CMakeLists.txt`, after the spatial-helpers block:

```cmake
# TNF: NotchModel::notchSidebandShift, the pure helper behind the +TNF
# button. Table-driven. USB 200/2800 -> +1500; LSB -2800/-200 -> -1500;
# AM -3000/+3000 hits the middle==0 fallback -> 1500. A dropped sign is
# silent on LSB and correct on USB, so it needs its own test.
# Source: Thetis console.cs:40281-40307 [v2.10.3.15].
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §5.3.
nereus_add_test(tst_notch_sideband_shift)
```

- [ ] **Step 22: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_sideband_shift`

Expected: FAIL at link with `Undefined symbols ... NereusSDR::NotchModel::notchSidebandShift(int, int)`.

- [ ] **Step 23: Implement `notchSidebandShift`**

Append to `src/models/NotchModel.cpp`, inside `namespace NereusSDR`, after `notchSurrounding`:

```cpp
// used when adding a notch to shift it into the middle of the sideband
// From Thetis console.cs:40281-40307 [v2.10.3.15] ,  notchSidebandShift(rx).
// Upstream reads the edges off radio.GetDSPRX(...).RXFilterLow / RXFilterHigh
// per rx index; a global, slice-agnostic NotchModel cannot, so the caller
// supplies the active slice's edges (SliceModel::filterLow() / filterHigh()).
//
// The Thetis CW-pitch term is deliberately absent (design §1.2): upstream
// TNFAdd's +cw_pitch and AddNotch's -cw_pitch cancel, and NereusSDR keeps the
// CW pitch in the filter passband rather than on the DDC, so this shift alone
// is already correct.
int NotchModel::notchSidebandShift(int filterLowHz, int filterHighHz)
{
    int middle = filterLowHz + ((filterHighHz - filterLowHz) / 2);
    if (middle == 0) { // probably symetric filter such as AM
        middle = filterHighHz / 2;
    }
    return middle;
}
```

- [ ] **Step 24: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_sideband_shift && ctest --test-dir build -R '^tst_notch_sideband_shift$' --output-on-failure`

Expected: PASS

- [ ] **Step 25: Commit**

```bash
git add src/models/NotchModel.cpp tests/tst_notch_sideband_shift.cpp tests/CMakeLists.txt
git commit -m "feat(tnf): port notchSidebandShift for the +TNF add gesture"
```

---

- [ ] **Step 26: Write the failing persistence and clear()-contract test**

Create `tests/tst_notch_persistence.cpp`:

```cpp
// =================================================================
// tests/tst_notch_persistence.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF §5.5: AppSettings round-trip at global scope (NotchGlobalEnabled,
// NotchVisualEnabled, NotchAutoIncrease, NotchCount, Notch<i>Center /
// Width / Active), plus the §5.3 clear() contract -- clear() MUST emit
// notchesReset() because the RadioModel fan-out is purely signal-driven,
// so a silent clear would leave every channel's notch set installed.
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         §5.3 (clear() contract), §5.5 (keys), §8.3 (visual toggle),
//         §9 (auto-increase).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/NotchModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestNotchPersistence : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── Defaults (design §5.4 / §8.3 / §9) ───────────────────────────────
    void defaults_match_the_wdsp_and_upstream_state()
    {
        NotchModel m;
        QVERIFY(m.globalEnabled());
        // WDSP creates nbp0 with autoincr = 1 (RXA.c:105), so ON, not OFF.
        QVERIFY(m.autoIncrease());
        // Thetis chkVisualNotch has no designer Checked assignment.
        QVERIFY(!m.visualEnabled());
        QVERIFY(!m.adminBusy());
        QVERIFY(m.notches().isEmpty());
    }

    // ── Global flags ─────────────────────────────────────────────────────
    void setGlobalEnabled_emits_only_on_change()
    {
        NotchModel m;
        QSignalSpy spy(&m, &NotchModel::globalEnabledChanged);
        m.setGlobalEnabled(true);          // already true
        QCOMPARE(spy.count(), 0);
        m.setGlobalEnabled(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), false);
        m.setGlobalEnabled(false);
        QCOMPARE(spy.count(), 1);
    }

    void setAutoIncrease_emits_only_on_change()
    {
        NotchModel m;
        QSignalSpy spy(&m, &NotchModel::autoIncreaseChanged);
        m.setAutoIncrease(true);
        QCOMPARE(spy.count(), 0);
        m.setAutoIncrease(false);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!m.autoIncrease());
    }

    void setVisualEnabled_emits_only_on_change()
    {
        NotchModel m;
        QSignalSpy spy(&m, &NotchModel::visualEnabledChanged);
        m.setVisualEnabled(false);
        QCOMPARE(spy.count(), 0);
        m.setVisualEnabled(true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(m.visualEnabled());
    }

    // ── clear() contract (§5.3) ──────────────────────────────────────────
    void clear_empties_the_list_and_emits_notchesReset()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        m.addNotch(14075000.0);
        QSignalSpy spy(&m, &NotchModel::notchesReset);

        m.clear();

        QVERIFY(m.notches().isEmpty());
        QCOMPARE(spy.count(), 1);
    }

    void clear_emits_notchesReset_even_when_already_empty()
    {
        // The signal is the fan-out's reconcile trigger, not a change
        // notification: a channel reopened with a stale set still needs it.
        NotchModel m;
        QSignalSpy spy(&m, &NotchModel::notchesReset);
        m.clear();
        QCOMPARE(spy.count(), 1);
    }

    void clear_does_not_emit_per_notch_removal()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QSignalSpy removed(&m, &NotchModel::notchRemoved);
        m.clear();
        QCOMPARE(removed.count(), 0);
    }

    // ── Persistence round-trip (§5.5) ────────────────────────────────────
    void save_writes_the_documented_keys()
    {
        NotchModel m;
        const int id = m.addNotch(14074123.0);
        QVERIFY(m.setWidth(id, 400.0));
        QVERIFY(m.setActive(id, false));
        m.setGlobalEnabled(false);
        m.setVisualEnabled(true);
        m.setAutoIncrease(false);

        m.saveToSettings();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("NotchGlobalEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(QStringLiteral("NotchVisualEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(QStringLiteral("NotchAutoIncrease")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(QStringLiteral("NotchCount")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(s.value(QStringLiteral("Notch0Center")).toDouble(), 14074123.0);
        QCOMPARE(s.value(QStringLiteral("Notch0Width")).toDouble(), 400.0);
        QCOMPARE(s.value(QStringLiteral("Notch0Active")).toString(),
                 QStringLiteral("False"));
    }

    void save_does_not_lose_frequency_precision()
    {
        // QVariant::toString() on a raw double is how AppSettings stores
        // everything; the centre must survive it exactly.
        NotchModel m;
        m.addNotch(28074123.0);
        m.saveToSettings();
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Notch0Center"))
                     .toDouble(), 28074123.0);
    }

    void restore_round_trips_the_list_in_order()
    {
        NotchModel src;
        src.addNotch(14074000.0);
        const int b = src.addNotch(14075000.0);
        QVERIFY(src.setWidth(b, 600.0));
        QVERIFY(src.setActive(b, false));
        src.addNotch(14076000.0);
        src.saveToSettings();

        NotchModel dst;
        dst.restoreFromSettings();

        QCOMPARE(dst.notches().size(), 3);
        QCOMPARE(dst.notches().at(0).centerHz, 14074000.0);
        QCOMPARE(dst.notches().at(1).centerHz, 14075000.0);
        QCOMPARE(dst.notches().at(1).widthHz, 600.0);
        QCOMPARE(dst.notches().at(1).active, false);
        QCOMPARE(dst.notches().at(2).centerHz, 14076000.0);
        QVERIFY(dst.notches().at(0).active);
    }

    void restore_round_trips_the_three_global_flags()
    {
        NotchModel src;
        src.setGlobalEnabled(false);
        src.setVisualEnabled(true);
        src.setAutoIncrease(false);
        src.saveToSettings();

        NotchModel dst;
        dst.restoreFromSettings();

        QVERIFY(!dst.globalEnabled());
        QVERIFY(dst.visualEnabled());
        QVERIFY(!dst.autoIncrease());
    }

    void restore_mints_fresh_distinct_ids()
    {
        // ids are session-local hit-test keys and are deliberately not
        // persisted (design §5.1 / §5.5).
        NotchModel src;
        src.addNotch(14074000.0);
        src.addNotch(14075000.0);
        src.saveToSettings();

        NotchModel dst;
        dst.restoreFromSettings();

        const int idA = dst.notches().at(0).id;
        const int idB = dst.notches().at(1).id;
        QVERIFY(idA != idB);
        QCOMPARE(dst.indexOfId(idA), 0);
        QCOMPARE(dst.indexOfId(idB), 1);
        QCOMPARE(dst.notchById(idB)->centerHz, 14075000.0);
    }

    void restore_emits_notchesReset()
    {
        NotchModel src;
        src.addNotch(14074000.0);
        src.saveToSettings();

        NotchModel dst;
        QSignalSpy spy(&dst, &NotchModel::notchesReset);
        dst.restoreFromSettings();
        QCOMPARE(spy.count(), 1);
    }

    void restore_leaves_defaults_alone_when_nothing_is_persisted()
    {
        NotchModel m;
        m.restoreFromSettings();
        QVERIFY(m.globalEnabled());
        QVERIFY(m.autoIncrease());
        QVERIFY(!m.visualEnabled());
        QVERIFY(m.notches().isEmpty());
    }

    void save_prunes_keys_left_by_a_longer_previous_list()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        m.addNotch(14076000.0);
        m.saveToSettings();
        QVERIFY(AppSettings::instance().contains(QStringLiteral("Notch2Center")));

        QVERIFY(m.removeNotch(b));
        m.saveToSettings();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("NotchCount")).toString(),
                 QStringLiteral("2"));
        QVERIFY(!s.contains(QStringLiteral("Notch2Center")));
        QVERIFY(!s.contains(QStringLiteral("Notch2Width")));
        QVERIFY(!s.contains(QStringLiteral("Notch2Active")));

        NotchModel dst;
        dst.restoreFromSettings();
        QCOMPARE(dst.notches().size(), 2);
        QCOMPARE(dst.notches().at(1).centerHz, 14076000.0);
    }

    void restore_replaces_rather_than_appends()
    {
        NotchModel src;
        src.addNotch(14074000.0);
        src.saveToSettings();

        NotchModel dst;
        dst.addNotch(21074000.0);
        dst.restoreFromSettings();

        QCOMPARE(dst.notches().size(), 1);
        QCOMPARE(dst.notches().at(0).centerHz, 14074000.0);
    }

    void save_of_an_empty_list_round_trips_as_empty()
    {
        NotchModel src;
        src.addNotch(14074000.0);
        src.saveToSettings();
        src.clear();
        src.saveToSettings();

        NotchModel dst;
        dst.restoreFromSettings();
        QVERIFY(dst.notches().isEmpty());
    }
};

QTEST_MAIN(TestNotchPersistence)
#include "tst_notch_persistence.moc"
```

Register it in `tests/CMakeLists.txt`, after the sideband-shift block:

```cmake
# TNF §5.5: NotchModel AppSettings round-trip at global scope, plus the
# §5.3 clear() contract. Covers NotchGlobalEnabled / NotchVisualEnabled /
# NotchAutoIncrease / NotchCount / Notch<i>Center|Width|Active, key pruning
# on a shrink, whole-list-replacement semantics, and that clear() emits
# notchesReset() even when already empty (the fan-out's reconcile trigger).
# Source: NereusSDR-original persistence (Thetis packs these into a single
# mnotchdb string at console.cs:3034-3035, 4763-4764 [v2.10.3.15]; flat keys
# avoid its locale-sensitive double.Parse).
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §5.3, §5.5.
nereus_add_test(tst_notch_persistence)
```

- [ ] **Step 27: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_persistence`

Expected: FAIL at link with `Undefined symbols ... NereusSDR::NotchModel::setGlobalEnabled(bool)`, `...::setAutoIncrease(bool)`, `...::setVisualEnabled(bool)`, `...::clear()`, `...::saveToSettings() const`, `...::restoreFromSettings()`.

- [ ] **Step 28: Implement the global flags, `clear()` and persistence**

Append to `src/models/NotchModel.cpp`, inside `namespace NereusSDR`, after `notchSidebandShift`:

```cpp
// ---------------------------------------------------------------------------
// Global flags
// ---------------------------------------------------------------------------

// From Thetis console.cs:39987-40005 [v2.10.3.15] ,  TNFActive.  Upstream
// gates its change handler on `if (old_tnf != value)` at console.cs:40004;
// preserved so a repeated set does not re-push the run flag to every channel.
void NotchModel::setGlobalEnabled(bool on)
{
    if (m_globalEnabled == on) {
        return;
    }
    m_globalEnabled = on;
    emit globalEnabledChanged(on);
}

// Fanned out to RXANBPSetAutoIncrease.  Starts ON because WDSP creates nbp0
// with autoincr = 1 (From WDSP RXA.c:105).
void NotchModel::setAutoIncrease(bool on)
{
    if (m_autoIncrease == on) {
        return;
    }
    m_autoIncrease = on;
    emit autoIncreaseChanged(on);
}

// From Thetis setup.cs:24376-24379 [v2.10.3.15] ,  chkVisualNotch_CheckedChanged
// drives Display.ShowVisualNotch.  NereusSDR keeps the flag on the model and
// lets SpectrumWidget observe it.
void NotchModel::setVisualEnabled(bool on)
{
    if (m_visualEnabled == on) {
        return;
    }
    m_visualEnabled = on;
    emit visualEnabledChanged(on);
}

void NotchModel::clear()
{
    m_notches.clear();
    // Design §5.3 clear() contract: the RadioModel fan-out is purely
    // signal-driven, so a silent clear() would leave every channel's notch
    // set installed while the model showed none.  Emitted unconditionally,
    // including when the list was already empty, because the signal is the
    // reconcile trigger rather than a change notification.
    emit notchesReset();
}

// ---------------------------------------------------------------------------
// Persistence (AppSettings, global scope; design §5.5)
// ---------------------------------------------------------------------------

void NotchModel::saveToSettings() const
{
    auto& s = AppSettings::instance();

    // Prune the tail left by a previously longer list before writing the new
    // count, otherwise a shrink leaves orphan Notch<i>* entries behind and a
    // later grow would read stale values back.
    const int previousCount =
        s.value(QStringLiteral("NotchCount"), QStringLiteral("0"))
            .toString().toInt();
    for (int i = m_notches.size(); i < previousCount; ++i) {
        s.remove(QStringLiteral("Notch%1Center").arg(i));
        s.remove(QStringLiteral("Notch%1Width").arg(i));
        s.remove(QStringLiteral("Notch%1Active").arg(i));
    }

    s.setValue(QStringLiteral("NotchGlobalEnabled"), boolStr(m_globalEnabled));
    s.setValue(QStringLiteral("NotchVisualEnabled"), boolStr(m_visualEnabled));
    s.setValue(QStringLiteral("NotchAutoIncrease"),  boolStr(m_autoIncrease));
    s.setValue(QStringLiteral("NotchCount"),
               QString::number(m_notches.size()));

    for (int i = 0; i < m_notches.size(); ++i) {
        const Notch& n = m_notches.at(i);
        // Explicit fixed formatting: AppSettings stores QVariant::toString()
        // of whatever it is handed, and an 'f' with 6 decimals is lossless
        // for the whole-Hz centres this model produces.
        s.setValue(QStringLiteral("Notch%1Center").arg(i),
                   QString::number(n.centerHz, 'f', 6));
        s.setValue(QStringLiteral("Notch%1Width").arg(i),
                   QString::number(n.widthHz, 'f', 6));
        s.setValue(QStringLiteral("Notch%1Active").arg(i), boolStr(n.active));
    }
}

void NotchModel::restoreFromSettings()
{
    auto& s = AppSettings::instance();

    // Each key: if absent, leave the current default unchanged.  Restores go
    // through the public setters so observers see the change.
    if (s.contains(QStringLiteral("NotchGlobalEnabled"))) {
        setGlobalEnabled(boolFrom(s.value(QStringLiteral("NotchGlobalEnabled"))));
    }
    if (s.contains(QStringLiteral("NotchVisualEnabled"))) {
        setVisualEnabled(boolFrom(s.value(QStringLiteral("NotchVisualEnabled"))));
    }
    if (s.contains(QStringLiteral("NotchAutoIncrease"))) {
        setAutoIncrease(boolFrom(s.value(QStringLiteral("NotchAutoIncrease"))));
    }

    if (!s.contains(QStringLiteral("NotchCount"))) {
        return;
    }

    const int count = s.value(QStringLiteral("NotchCount")).toString().toInt();
    m_notches.clear();

    for (int i = 0; i < count; ++i) {
        const QString centerKey = QStringLiteral("Notch%1Center").arg(i);
        if (!s.contains(centerKey)) {
            qCWarning(lcDsp) << "NotchModel: missing" << centerKey
                             << "- stopping notch restore at index" << i;
            break;
        }

        // ids are session-local hit-test keys and are deliberately not
        // persisted (design §5.1); they are re-minted monotonically here.
        Notch n;
        n.id       = m_nextId++;
        n.centerHz = s.value(centerKey).toDouble();
        n.widthHz  = s.value(QStringLiteral("Notch%1Width").arg(i),
                             QString::number(kDefaultNotchWidthHz, 'f', 6))
                         .toDouble();
        n.active   = boolFrom(s.value(QStringLiteral("Notch%1Active").arg(i),
                                      QStringLiteral("True")));
        m_notches.append(n);
    }

    // Whole-list replacement (design §5.3): RadioModel reconciles every open
    // channel off this signal.
    emit notchesReset();
}
```

- [ ] **Step 29: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_persistence && ctest --test-dir build -R '^tst_notch_persistence$' --output-on-failure`

Expected: PASS

- [ ] **Step 30: Run all five TNF model tests together, then commit**

Run: `cmake --build build --target tst_notch_model_guards tst_notch_model_index_invariant tst_notch_spatial_helpers tst_notch_sideband_shift tst_notch_persistence && ctest --test-dir build -R '^tst_notch_' --output-on-failure`

Expected: PASS, 5 tests.

```bash
git add src/models/NotchModel.cpp tests/tst_notch_persistence.cpp tests/CMakeLists.txt
git commit -m "feat(tnf): NotchModel global flags, clear() reset contract and persistence"
```

---

### Task 4: RadioModel fan-out and syncNotchesToAllChannels

**Files:**
- Create: `tests/tst_notch_channel_sync.cpp`
- Modify: `src/models/RadioModel.h:131` (add `class RxChannel;` forward decl)
- Modify: `src/models/RadioModel.h:185` (add `class NotchModel;` forward decl)
- Modify: `src/models/RadioModel.h:457` (declare `syncNotchesToAllChannels()` after `activateBoundSliceChannels()`)
- Modify: `src/models/RadioModel.h:785` (add `notchModel()` accessor after `pskReporter()`)
- Modify: `src/models/RadioModel.h:2244` (private `wireNotchModel()` / `syncNotchesToChannel()` / `sliceRxChannels()` / `reconcileNotchCount()` after `wireSliceSignals`)
- Modify: `src/models/RadioModel.h:3078` (add `std::unique_ptr<NotchModel> m_notchModel;`)
- Modify: `src/models/RadioModel.cpp:315` (add `#include "models/NotchModel.h"`)
- Modify: `src/models/RadioModel.cpp:1580-1582` (construct + restore in ctor, before the spot-system block)
- Modify: `src/models/RadioModel.cpp:3068` (call `syncNotchesToAllChannels()` at the `openRxChannelPool` tail)
- Modify: `src/models/RadioModel.cpp:3144-3146` (notch reconcile in `activateSliceChannel` before `setActive(true)`)
- Modify: `src/models/RadioModel.cpp:3147` (insert the five new member definitions after `activateSliceChannel`)
- Modify: `src/core/WdspEngine.h:123` (forward-declare `class TestNotchChannelSync;`)
- Modify: `src/core/WdspEngine.h:694` (add `friend class ::TestNotchChannelSync;`)
- Modify: `src/core/RxChannel.h` (notch block introduced by Task 2: add `notchesRun()`, `notchAutoIncrease()`, `notchAt()` and the two carry members)
- Modify: `src/core/RxChannel.cpp` (notch block introduced by Task 2: carry writes in `setNotchesRun` / `setNotchAutoIncrease`, plus `notchAt`)
- Modify: `tests/CMakeLists.txt:5732` (register the new test)
- Test: `tests/tst_notch_channel_sync.cpp`

**Interfaces:**

- Consumes (Task 1, §4):
  - `void RxChannel::setNotchTuneFrequency(double absoluteHz);`
  - `double RxChannel::notchTuneFrequencyHz() const;`
  - `void RXANBPSetTuneFrequency(int channel, double tunefreq);` (`src/core/wdsp_api.h`)
- Consumes (Task 2, §6.1/§6.2):
  - `void RxChannel::syncNotches(const QList<Notch>& notches);`
  - `bool RxChannel::addNotch(int index, const Notch& n);`
  - `bool RxChannel::editNotch(int index, const Notch& n);`
  - `bool RxChannel::deleteNotch(int index);`
  - `int  RxChannel::notchCount() const;`
  - `void RxChannel::setNotchesRun(bool run);`
  - `void RxChannel::setNotchAutoIncrease(bool on);`
  - `double RxChannel::minNotchWidthHz() const;`
  - `int RXANBPGetNotch(int channel, int notch, double* fcenter, double* fwidth, int* active);` (`src/core/wdsp_api.h`)
  - `int RXANBPAddNotch(int, int, double, double, int);`, `int RXANBPEditNotch(int, int, double, double, int);`, `int RXANBPDeleteNotch(int, int);`, `void RXANBPGetNumNotches(int, int*);`, `void RXANBPSetNotchesRun(int, int);`, `void RXANBPSetAutoIncrease(int, int);`
- Consumes (Task 3, §5):
  - `struct NereusSDR::Notch { int id{0}; double centerHz{0.0}; double widthHz{200.0}; bool active{true}; };`, reachable by including `models/NotchModel.h`
  - `class NereusSDR::NotchModel : public QObject`, `explicit NotchModel(QObject* parent = nullptr);`
  - `const QList<Notch>& notches() const;` / `const Notch* notchById(int id) const;` / `int indexOfId(int id) const;`
  - `bool globalEnabled() const;` / `bool autoIncrease() const;`
  - `int addNotch(double centerHz, double widthHz = 200.0);` / `bool setWidth(int id, double widthHz);` / `bool removeNotch(int id);` / `void clear();`
  - `void setGlobalEnabled(bool on);` / `void setAutoIncrease(bool on);` / `void restoreFromSettings();`
  - signals `notchAdded(int id)`, `notchChanged(int id)`, `notchRemoved(int id, int formerIndex)`, `globalEnabledChanged(bool)`, `autoIncreaseChanged(bool)`, `notchesReset()`
  - `src/models/NotchModel.cpp` already appended to `MODEL_SOURCES` (`CMakeLists.txt:656-673`)
  - AppSettings keys `NotchGlobalEnabled` / `NotchCount` / `Notch<i>Center` / `Notch<i>Width` / `Notch<i>Active` (§5.5)
- Consumes (existing tree):
  - `SliceModel* RadioModel::sliceById(int sliceId) const;` / `QList<SliceModel*> RadioModel::slices() const;`
  - `WdspEngine* RadioModel::wdspEngine();` / `RxChannel* WdspEngine::rxChannel(int channelId) const;` / `void WdspEngine::destroyRxChannel(int channelId);`
  - `WdspEngine::kFirstSliceChannelId` / `WdspEngine::kMaxSliceChannels`
  - `double SliceStreamAllocator::streamCentreHz(int streamIndex) const;`
  - `void RadioModel::configureStreamPool(int, int, int);` / `int RadioModel::addSlice(const QString& = QString());` / `void RadioModel::openRxChannelPool(int, int, int);` / `void RadioModel::activateSliceChannel(SliceModel*);` / `double RadioModel::streamCentreHzForTest(int) const;`
  - `int bufferSizeForRate(int rateHz);` (`core/SampleRateCatalog.h`)
- Produces (Tasks 5, 8, 9 rely on these):
  - `NotchModel* RadioModel::notchModel() const;` (§8.1; needed by the TCI repoint, `+TNF`, the status-bar light and `MnfSetupPage`)
  - `void RadioModel::syncNotchesToAllChannels();`
  - `bool RxChannel::notchesRun() const;`
  - `bool RxChannel::notchAutoIncrease() const;`
  - `bool RxChannel::notchAt(int index, Notch& out) const;`
  - private: `void RadioModel::wireNotchModel();`, `void RadioModel::syncNotchesToChannel(RxChannel* ch, int channelId);`, `QVector<RxChannel*> RadioModel::sliceRxChannels() const;`, `void RadioModel::reconcileNotchCount(RxChannel* ch);`
  - `friend class ::TestNotchChannelSync;` on `WdspEngine`

---

#### Cycle A: `RadioModel` owns `NotchModel`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_notch_channel_sync.cpp`:

```cpp
// =================================================================
// tests/tst_notch_channel_sync.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF Task 4: RadioModel notch fan-out + syncNotchesToAllChannels().
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//   §6.3  fan-out and the openRxChannelPool-tail reconcile
//   §5.5  restore order (model populated before any channel exists)
//   §8.1  RadioModel::notchModel() accessor
//   §11   tst_notch_channel_sync
//
// Uses the WdspEngine NEREUS_BUILD_TESTS friend seam
// (src/core/WdspEngine.h:674-695) exactly as
// tests/tst_stream_pool_binding.cpp:997-1017 does: priming
// m_initialized lets openRxChannelPool run createRxChannel's real
// OpenChannel, so every RXANBP* wrapper here talks to a genuinely
// opened WDSP channel.  Design §11.1: an unopened channel is not
// merely inert, it is an out-of-bounds read on rxa[].
// =================================================================
#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestNotchChannelSync : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── §8.1: the accessor, alongside spotModel() ────────────────────────
    void radio_model_owns_one_notch_model()
    {
        RadioModel model;
        QVERIFY(model.notchModel() != nullptr);
        // A non-owning view onto a unique_ptr member, not a factory.
        QCOMPARE(model.notchModel(), model.notchModel());
    }

    // ── §5.5 / §8.1: restoreFromSettings() runs in the ctor, before any
    // channel exists, so the openRxChannelPool-tail reconcile always has
    // the full list to install.
    void notch_model_is_restored_at_construction()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("NotchGlobalEnabled"), QStringLiteral("True"));
        s.setValue(QStringLiteral("NotchCount"),         1);
        s.setValue(QStringLiteral("Notch0Center"),       14074000.0);
        s.setValue(QStringLiteral("Notch0Width"),        250.0);
        s.setValue(QStringLiteral("Notch0Active"),       QStringLiteral("True"));

        RadioModel model;
        const NotchModel* nm = model.notchModel();
        QVERIFY(nm != nullptr);
        QCOMPARE(nm->notches().size(), 1);
        QCOMPARE(nm->notches().at(0).centerHz, 14074000.0);
        QCOMPARE(nm->notches().at(0).widthHz,  250.0);
        QVERIFY(nm->notches().at(0).active);
        QVERIFY(nm->globalEnabled());
    }
};

QTEST_MAIN(TestNotchChannelSync)
#include "tst_notch_channel_sync.moc"
```

- [ ] **Step 2: Register the test and add the `WdspEngine` friend seam**

Append after `tests/CMakeLists.txt:5732` (`nereus_add_test(tst_diversity_dialog_persistence)`), before the `# Aggregate "all_tests" target` block:

```cmake
# ── TNF Task 4: RadioModel notch fan-out + syncNotchesToAllChannels() ──────
# Covers the design's §6.3 gap: connectToRadio activates Slice A's channel
# (RadioModel.cpp:5365) BEFORE it opens the pool (:5377), and
# activateSliceChannel early-returns on an already-active channel (:3119),
# so the primary receiver never passes through that hook.  Asserts Slice A
# gets the notch list, the master run flag, the auto-increase flag and the
# NBP tune frequency on connect and across a reconnect; that a slice added
# after a live notch add inherits the set; and that add / edit / remove /
# reset / master-enable / auto-increase all fan out to every live channel.
# Source: NereusSDR-original test infrastructure.  Behaviour ported from
# Thetis console.cs:40271-40273 [v2.10.3.15] (fixed three-id fan-out) and
# WDSP RXA.c:85-93 + nbp.c:190,223,499 (inert-by-default notch database).
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §6.3.
nereus_add_test(tst_notch_channel_sync)
```

Insert after `src/core/WdspEngine.h:123` (`class TestWdspChannelIdMap;`), inside the existing `#ifdef NEREUS_BUILD_TESTS` block:

```cpp
// TNF Task 4: the notch fan-out test primes the engine so openRxChannelPool
// opens real WDSP channels; the RXANBP* wrappers cannot be exercised on an
// unopened id (design §11.1 -- rxa[] is sized MAX_CHANNELS and every entry
// point dereferences before range-checking).
class TestNotchChannelSync;
```

Insert after `src/core/WdspEngine.h:694` (`friend class ::TestWdspChannelIdMap;`):

```cpp
    // TNF Task 4: same friendship for the notch channel-sync test.
    friend class ::TestNotchChannelSync;
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_channel_sync && ctest --test-dir build -R '^tst_notch_channel_sync$' --output-on-failure`

Expected: FAIL at compile time with `error: no member named 'notchModel' in 'NereusSDR::RadioModel'` at `tests/tst_notch_channel_sync.cpp` in `radio_model_owns_one_notch_model`.

- [ ] **Step 4: Give `RadioModel` the `NotchModel` member, accessor and ctor restore**

`src/models/RadioModel.h`, insert after line 131 (`class WdspEngine;`):

```cpp
class RxChannel;
```

`src/models/RadioModel.h`, insert after line 185 (`class RxDecodeModel;`):

```cpp
// TNF (design §5): the canonical notch store, owned by RadioModel alongside
// SpotModel. One list shared by every slice (design D1) because notch centres
// are absolute RF Hz, so a 20 m notch is inherently inert on a 40 m slice.
class NotchModel;
```

`src/models/RadioModel.h`, insert after line 785 (`PskReporterClient* pskReporter() ...`):

```cpp
    // ── TNF (design §8.1): the canonical notch store ────────────────────────
    //
    // Constructed in the RadioModel ctor and restored from AppSettings there,
    // before any WDSP channel exists, so the openRxChannelPool-tail reconcile
    // (§6.3) always has the full list to install. Non-owning pointer;
    // lifetime is RadioModel's. Consumed by the TCI rx_nf_enable repoint
    // (§6.4), the +TNF button and status-bar light (§7), and MnfSetupPage (§9).
    NotchModel* notchModel() const { return m_notchModel.get(); }
```

`src/models/RadioModel.h`, insert after line 3078 (`std::unique_ptr<PskReporterClient> m_pskReporter;`):

```cpp
    // TNF (design §5): notch store. Persisted globally rather than per-MAC
    // (design D3) because a notch tracks a QRM source at the operator's
    // location and band, not a property of the radio.
    std::unique_ptr<NotchModel> m_notchModel;
```

`src/models/RadioModel.cpp`, insert after line 315 (`#include "models/RxDecodeModel.h"`):

```cpp
// TNF (design §5, §6.3): the notch store RadioModel owns and fans out from.
#include "models/NotchModel.h"
```

`src/models/RadioModel.cpp`, insert after line 1580 (the closing `});` of the `Rf2ksConnection::disconnected` connect) and before the `// ── Phase 3J-2 H2: spot-system construction + wiring ──` banner at :1582:

```cpp
    // ── TNF (design §5, §5.5): notch store construction + restore ─────────────
    //
    // Constructed before anything that can open a WDSP channel, and restored
    // immediately, so §5.5's ordering holds: the model is fully populated by
    // the time openRxChannelPool's tail reconciles the pool (§6.3). On a cold
    // start no channel exists yet, which is exactly why the reconcile lives
    // there rather than at channel-activation time.
    m_notchModel = std::make_unique<NotchModel>(this);
    m_notchModel->restoreFromSettings();
```

- [ ] **Step 5: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_channel_sync && ctest --test-dir build -R '^tst_notch_channel_sync$' --output-on-failure`

Expected: PASS (2 slots).

- [ ] **Step 6: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp src/core/WdspEngine.h tests/tst_notch_channel_sync.cpp tests/CMakeLists.txt
git commit -m "feat(notch): RadioModel owns NotchModel and restores it at construction"
```

---

#### Cycle B: `RxChannel` notch readback seams

- [ ] **Step 7: Write the failing test**

Append this slot to `TestNotchChannelSync` in `tests/tst_notch_channel_sync.cpp`, after `notch_model_is_restored_at_construction()`:

```cpp
    // ── §6.2 + §11: the readbacks the fan-out tests assert through ───────
    //
    // notchesRun() / notchAutoIncrease() are C++ carries (the §4.6 pattern:
    // written outside #ifdef HAVE_WDSP), because WDSP exposes no getter for
    // NOTCHDB::master_run or NBP::autoincr. notchAt() is the real thing:
    // RXANBPGetNotch reads WDSP's own per-channel database back (nbp.c:393),
    // so it proves a push landed rather than echoing a carry.
    void rx_channel_reports_back_the_notch_state_it_was_handed()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14074500.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);

        ch->setNotchesRun(true);
        QVERIFY(ch->notchesRun());
        ch->setNotchesRun(false);
        QVERIFY(!ch->notchesRun());

        ch->setNotchAutoIncrease(true);
        QVERIFY(ch->notchAutoIncrease());
        ch->setNotchAutoIncrease(false);
        QVERIFY(!ch->notchAutoIncrease());

        Notch n;
        n.centerHz = 14074000.0;
        n.widthHz  = 250.0;
        n.active   = true;
        QVERIFY(ch->addNotch(0, n));
        QCOMPARE(ch->notchCount(), 1);

        Notch got;
        QVERIFY(ch->notchAt(0, got));
        QCOMPARE(got.centerHz, 14074000.0);
        QCOMPARE(got.widthHz,  250.0);
        QVERIFY(got.active);

        // Past the end: RXANBPGetNotch returns -1 and writes its sentinels
        // (nbp.c:406-411), so the wrapper must report failure, not garbage.
        QVERIFY(!ch->notchAt(1, got));
    }
```

- [ ] **Step 8: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_channel_sync && ctest --test-dir build -R '^tst_notch_channel_sync$' --output-on-failure`

Expected: FAIL at compile time with `error: no member named 'notchesRun' in 'NereusSDR::RxChannel'` at `tests/tst_notch_channel_sync.cpp`.

- [ ] **Step 9: Add the three readbacks to `RxChannel`**

`src/core/RxChannel.h`, in the public notch block introduced by Task 2, immediately after `double notchTuneFrequencyHz() const;`:

```cpp
    // Carried copies of the two per-channel notch flags. WDSP exposes no
    // getter for NOTCHDB::master_run (nbp.c:499 is a pure writer) or
    // NBP::autoincr (nbp.c:604), so the carry is the only readback, and it is
    // written outside #ifdef HAVE_WDSP exactly as setShiftFrequency writes
    // m_shiftOffsetHz (RxChannel.cpp:1468-1477).
    bool notchesRun() const { return m_notchesRun; }
    bool notchAutoIncrease() const { return m_notchAutoIncrease; }

    // Read one notch straight back out of WDSP's per-channel database.
    // From WDSP nbp.c:393 (RXANBPGetNotch): 0 on success, -1 with sentinel
    // outputs when `index` is past the end. `out.id` is left untouched --
    // WDSP's database is positional and carries no id (design D4).
    bool notchAt(int index, Notch& out) const;
```

`src/core/RxChannel.h`, in the private member block, immediately after `double m_notchTuneFrequencyHz{0.0};` (added by Task 1 alongside `m_shiftOffsetHz` at `:936-938`):

```cpp
    // Notch flag carries. Main thread only: nothing in the audio callback
    // reads them, so no atomics, for the same reason m_nr1Tuning is a plain
    // member -- WDSP holds the authoritative state for the audio thread.
    bool m_notchesRun{false};
    bool m_notchAutoIncrease{false};
```

`src/core/RxChannel.cpp`, in the notch block introduced by Task 2, replace the two flag setters with the carry-then-guard form and add `notchAt`:

```cpp
void RxChannel::setNotchesRun(bool run)
{
    m_notchesRun = run;
#ifdef HAVE_WDSP
    // From WDSP nbp.c:499 -- the only writer of NOTCHDB::master_run and
    // NBP::fnfrun. Both calc_nbp_lightweight (nbp.c:190) and calc_nbp_impulse
    // (nbp.c:223) bypass the notch database entirely while fnfrun is 0, and
    // the database is created inert (RXA.c:85-93), so a channel that never
    // sees this call is notch-inert, not merely empty.
    RXANBPSetNotchesRun(m_channelId, run ? 1 : 0);
#endif
}

void RxChannel::setNotchAutoIncrease(bool on)
{
    m_notchAutoIncrease = on;
#ifdef HAVE_WDSP
    // From WDSP nbp.c:604 -- widens a notch narrower than the achievable
    // minimum rather than silently under-delivering (nbp.c:122,
    // "if (autoincr && width[k] < minwidth)").
    RXANBPSetAutoIncrease(m_channelId, on ? 1 : 0);
#endif
}

bool RxChannel::notchAt(int index, Notch& out) const
{
#ifdef HAVE_WDSP
    double centerHz = 0.0;
    double widthHz  = 0.0;
    int    active   = 0;
    // From WDSP nbp.c:393 -- returns 0 on success; on -1 it writes
    // fcenter -1.0 / fwidth 0.0 / active -1, which must not reach the caller.
    if (RXANBPGetNotch(m_channelId, index, &centerHz, &widthHz, &active) != 0) {
        return false;
    }
    out.centerHz = centerHz;
    out.widthHz  = widthHz;
    out.active   = (active != 0);
    return true;
#else
    Q_UNUSED(index);
    Q_UNUSED(out);
    return false;
#endif
}
```

- [ ] **Step 10: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_channel_sync && ctest --test-dir build -R '^tst_notch_channel_sync$' --output-on-failure`

Expected: PASS (3 slots).

- [ ] **Step 11: Commit**

```bash
git add src/core/RxChannel.h src/core/RxChannel.cpp tests/tst_notch_channel_sync.cpp
git commit -m "feat(notch): RxChannel readbacks for run flag, auto-increase and stored notches"
```

---

#### Cycle C: `syncNotchesToAllChannels()` at the `openRxChannelPool` tail

- [ ] **Step 12: Write the failing tests**

Append these two slots to `TestNotchChannelSync`:

```cpp
    // ── §6.3: the whole point of the task ────────────────────────────────
    //
    // connectToRadio's WDSP-init lambda activates channel 0 at
    // RadioModel.cpp:5365, BEFORE it opens the pool at :5377, and
    // activateSliceChannel early-returns on an already-active channel
    // (:3119). Slice A therefore never passes through that hook, so the
    // reconcile has to live at the openRxChannelPool tail.
    void slice_a_gets_notches_run_autoincrease_and_tunefreq_on_connect()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        NotchModel* nm = model.notchModel();
        nm->setGlobalEnabled(true);
        nm->setAutoIncrease(true);
        QVERIFY(nm->addNotch(14074000.0, 200.0) >= 0);
        QVERIFY(nm->addNotch(14100000.0, 500.0) >= 0);

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14074500.0);
        QVERIFY(model.sliceById(a)->streamIndex() >= 0);

        // Everything above happened with zero WDSP channels open, so nothing
        // pushed anything. This call is the only writer.
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);
        QCOMPARE(ch->notchCount(), 2);
        QVERIFY(ch->notchesRun());
        QVERIFY(ch->notchAutoIncrease());

        // §4.1: tunefreq is the hosting stream's centre, not the slice
        // frequency. WDSP sums it with the shift (offset = tunefreq + shift,
        // nbp.c:192), so both halves are asserted, not just the sum.
        const int st = model.sliceById(a)->streamIndex();
        QCOMPARE(ch->notchTuneFrequencyHz(), model.streamCentreHzForTest(st));
        QCOMPARE(ch->notchTuneFrequencyHz() + model.sliceById(a)->shiftOffsetHz(),
                 model.sliceById(a)->frequency());

        // List order is the WDSP index (§5.2).
        Notch got;
        QVERIFY(ch->notchAt(0, got));
        QCOMPARE(got.centerHz, 14074000.0);
        QVERIFY(ch->notchAt(1, got));
        QCOMPARE(got.centerHz, 14100000.0);
    }

    // ── §6.3: reconnect reopens the hole ─────────────────────────────────
    //
    // teardownConnection calls WdspEngine::shutdown (RadioModel.cpp:10175),
    // which destroys every RX channel (WdspEngine.cpp:311-320); connectToRadio
    // then re-opens the pool. Simulated here by destroying the pool directly,
    // which is precisely the half of shutdown() that matters.
    void notches_come_back_on_slice_a_after_a_reconnect()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        NotchModel* nm = model.notchModel();
        nm->setGlobalEnabled(true);
        QVERIFY(nm->addNotch(7040000.0, 200.0) >= 0);

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(7040500.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);
        QCOMPARE(engine->rxChannel(a)->notchCount(), 1);

        for (int ch = 0; ch < WdspEngine::kMaxSliceChannels; ++ch) {
            engine->destroyRxChannel(ch);
        }
        QVERIFY(engine->rxChannel(a) == nullptr);

        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        RxChannel* ch = engine->rxChannel(a);
        QVERIFY(ch != nullptr);
        QCOMPARE(ch->notchCount(), 1);
        QVERIFY(ch->notchesRun());
        QCOMPARE(ch->notchTuneFrequencyHz(),
                 model.streamCentreHzForTest(model.sliceById(a)->streamIndex()));
    }
```

- [ ] **Step 13: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_channel_sync && ctest --test-dir build -R '^tst_notch_channel_sync$' --output-on-failure`

Expected: FAIL at runtime in `slice_a_gets_notches_run_autoincrease_and_tunefreq_on_connect` with `Compared values are not the same / Actual (ch->notchCount()): 0 / Expected (2)` (nothing reconciles the pool yet).

- [ ] **Step 14: Add the reconcile helpers and call them from `openRxChannelPool`**

`src/models/RadioModel.h`, insert after line 457 (`void activateBoundSliceChannels();`):

```cpp
    /// Reconcile every OPEN pool channel with the current notch set.
    ///
    /// Design §6.3. Runs at the tail of openRxChannelPool because
    /// activateSliceChannel is dead as a hook for Slice A: connectToRadio's
    /// WDSP-init lambda activates channel 0 (RadioModel.cpp:5365) before it
    /// opens the pool (:5377), and activateSliceChannel early-returns on an
    /// already-active channel (:3119). Covers reconnect for free, since
    /// teardownConnection destroys every channel (:10175).
    ///
    /// Reconciles channels that no slice is bound to yet as well: a notch DB
    /// is created inert (RXA.c:85-93) and this is where the run flag lands.
    void syncNotchesToAllChannels();
```

`src/models/RadioModel.h`, insert after line 2244 (`void wireSliceSignals(SliceModel* slice);`):

```cpp
    /// Push the full notch state at one channel: the list, the master run
    /// flag, the auto-increase flag and the NBP tune frequency. `channelId`
    /// is also the slice index (Sub-Epic I invariant), which is how the
    /// hosting stream's centre is resolved for the tune frequency (§4.1).
    void syncNotchesToChannel(RxChannel* ch, int channelId);

    /// Every WDSP RX channel that currently backs a slice. The fan-out
    /// target set for a live notch mutation (§6.3).
    QVector<RxChannel*> sliceRxChannels() const;

    /// Design §6.2: our list position IS the WDSP notch index, so a count
    /// divergence is a correctness bug. Detect and recover with a full
    /// resync rather than assert, which a release build compiles out.
    void reconcileNotchCount(RxChannel* ch);
```

`src/models/RadioModel.cpp`, replace line 3068 (`activateBoundSliceChannels();`) with:

```cpp
    activateBoundSliceChannels();

    // TNF design §6.3: reconcile the notch set, the master run flag, the
    // auto-increase flag and the NBP tune frequency across every channel this
    // pool just opened -- including channel 0, which the Slice A block in
    // connectToRadio already activated and which activateSliceChannel
    // therefore refuses to touch. Cheap when the notch list is empty
    // (syncNotches deletes nothing and adds nothing) and idempotent when it
    // is not (RXANBPSetTuneFrequency short-circuits at nbp.c:479).
    syncNotchesToAllChannels();
```

`src/models/RadioModel.cpp`, insert after line 3147 (the closing brace of `activateSliceChannel`):

```cpp
// ── TNF fan-out (design §6.3) ───────────────────────────────────────────────
//
// Thetis fans every notch mutation at three fixed WDSP ids -- WDSP.id(0, 0),
// WDSP.id(0, 1) and WDSP.id(2, 0) (console.cs:40271-40273 [v2.10.3.15]).
// NereusSDR's slice count is dynamic post-3F, so we walk slices() instead;
// the WDSP RX channel id is the slice index (Sub-Epic I invariant).
//
// One list serves every slice (design D1). Notch centres are absolute RF Hz,
// so a 20 m notch is inherently inert on a 40 m slice, which is what lets the
// same list go to every channel unfiltered.
QVector<RxChannel*> RadioModel::sliceRxChannels() const
{
    QVector<RxChannel*> out;
    if (!m_wdspEngine) {
        return out;
    }
    for (SliceModel* s : std::as_const(m_slices)) {
        if (!s) { continue; }
        if (RxChannel* ch = m_wdspEngine->rxChannel(s->sliceIndex())) {
            out.append(ch);
        }
    }
    return out;
}

void RadioModel::reconcileNotchCount(RxChannel* ch)
{
    if (!ch || !m_notchModel) {
        return;
    }
    // RXANBPGetNumNotches takes the channel's DSP critical section
    // (nbp.c:465-472). Negligible next to UpdateNBPFilters, which every
    // mutation already pays and which designs two filters, nbp0 plus
    // recalc_bpsnba_filter (nbp.c:345-359 -> snb.c:814-828).
    const int expected = m_notchModel->notches().size();
    const int actual   = ch->notchCount();
    if (actual == expected) {
        return;
    }
    qCWarning(lcDsp) << "Notch index divergence on RX channel"
                     << ch->channelId() << "- WDSP holds" << actual
                     << "notches, the model holds" << expected
                     << "- resyncing";
    ch->syncNotches(m_notchModel->notches());
}

void RadioModel::syncNotchesToChannel(RxChannel* ch, int channelId)
{
    if (!ch || !m_notchModel) {
        return;
    }

    ch->syncNotches(m_notchModel->notches());

    // Every channel's notch database is built inert: create_notchdb takes
    //   0,      // master run for all nbp's
    // and create_nbp takes
    //   0,      // run the notches
    // (third_party/wdsp/src/RXA.c:85-93), and both calc_nbp_lightweight
    // (nbp.c:190) and calc_nbp_impulse (nbp.c:223) bypass the database
    // entirely while fnfrun is 0. RXANBPSetNotchesRun is its only writer
    // (nbp.c:499), so a channel that misses this call is notch-inert rather
    // than merely empty.
    ch->setNotchesRun(m_notchModel->globalEnabled());

    // Easy to drop and silent when dropped: without it a sub-minimum notch is
    // never widened (nbp.c:122, "if (autoincr && width[k] < minwidth)") and
    // bench row 8 fails with no other symptom. Design §6.3 calls this out.
    ch->setNotchAutoIncrease(m_notchModel->autoIncrease());

    // Design §4.1: NOTCHDB::tunefreq is the hosting stream's CENTRE, not the
    // slice frequency. WDSP sums the two terms (offset = tunefreq + shift,
    // nbp.c:192) and we already feed shift as the slice's displacement from
    // its stream centre, so driving tunefreq from the slice frequency would
    // compute 2*sliceFreq - streamCentre. Thetis proves the intent: it gives
    // the sub-receiver its own shift (console.cs:31922 [v2.10.3.15]) while
    // pushing the identical tunefreq to both ids (:31940-31941).
    if (SliceModel* s = sliceById(channelId)) {
        if (s->streamIndex() >= 0) {
            ch->setNotchTuneFrequency(
                m_streamAllocator.streamCentreHz(s->streamIndex()));
        }
    }
}

void RadioModel::syncNotchesToAllChannels()
{
    if (!m_wdspEngine || !m_notchModel) {
        return;
    }
    for (int ch = WdspEngine::kFirstSliceChannelId;
         ch < WdspEngine::kMaxSliceChannels; ++ch) {
        // rxChannel returns nullptr for ids this pool did not open, so the
        // full sweep is safe even when the SKU's maxSlices is smaller.
        syncNotchesToChannel(m_wdspEngine->rxChannel(ch), ch);
    }
}
```

- [ ] **Step 15: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_channel_sync && ctest --test-dir build -R '^tst_notch_channel_sync$' --output-on-failure`

Expected: PASS (5 slots).

- [ ] **Step 16: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp tests/tst_notch_channel_sync.cpp
git commit -m "fix(notch): reconcile every open RX channel at the openRxChannelPool tail"
```

---

#### Cycle D: the `activateSliceChannel` hook for a later-added slice

- [ ] **Step 17: Write the failing test**

Append this slot to `TestNotchChannelSync`:

```cpp
    // ── §6.3: "keep the activateSliceChannel hook for the later-added-slice
    // case". The discriminating sequence is a notch added AFTER the pool
    // reconcile: the live fan-out walks slices(), and slice B does not exist
    // yet, so channel 1 is left open, bound to nothing and empty. Binding B
    // is the only remaining chance to seed it.
    void a_slice_added_after_a_live_notch_add_inherits_the_set()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14074500.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        NotchModel* nm = model.notchModel();
        nm->setGlobalEnabled(true);
        nm->setAutoIncrease(true);
        QVERIFY(nm->addNotch(14074000.0, 200.0) >= 0);

        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14076000.0);
        QVERIFY(model.sliceById(b)->streamIndex() >= 0);

        RxChannel* ch = engine->rxChannel(b);
        QVERIFY(ch != nullptr);
        QCOMPARE(ch->notchCount(), 1);
        QVERIFY(ch->notchesRun());
        QVERIFY(ch->notchAutoIncrease());
        QCOMPARE(ch->notchTuneFrequencyHz(),
                 model.streamCentreHzForTest(model.sliceById(b)->streamIndex()));
    }
```

- [ ] **Step 18: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_channel_sync && ctest --test-dir build -R '^tst_notch_channel_sync$' --output-on-failure`

Expected: FAIL at runtime in `a_slice_added_after_a_live_notch_add_inherits_the_set` with `Compared values are not the same / Actual (ch->notchCount()): 0 / Expected (1)`.

Note: the `nm->addNotch(...)` call above does not yet reach any channel either (Cycle E wires that), so this slot fails on slice B alone, which is exactly the hook under test.

- [ ] **Step 19: Add the notch reconcile to `activateSliceChannel`**

`src/models/RadioModel.cpp`, insert between line 3144 (`ch->setShiftFrequency(slice->shiftOffsetHz());`) and line 3146 (`ch->setActive(true);`):

```cpp
    // TNF design §6.3: the notch set, the master run flag, the auto-increase
    // flag and the NBP tune frequency. syncNotchesToAllChannels covers every
    // channel the pool opened, but a slice added afterwards binds to a channel
    // that has been sitting open and unreconciled since, and the signal
    // fan-out only walks slices that already exist. This is the hook for that
    // case. No-op on retune, because the early return above already fired.
    syncNotchesToChannel(ch, slice->sliceIndex());
```

- [ ] **Step 20: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_channel_sync && ctest --test-dir build -R '^tst_notch_channel_sync$' --output-on-failure`

Expected: PASS (6 slots).

- [ ] **Step 21: Commit**

```bash
git add src/models/RadioModel.cpp tests/tst_notch_channel_sync.cpp
git commit -m "fix(notch): seed a later-added slice's channel from activateSliceChannel"
```

---

#### Cycle E: live-mutation fan-out

- [ ] **Step 22: Write the failing tests**

Append these six slots to `TestNotchChannelSync`:

```cpp
    // ── §6.3 live fan-out: add ───────────────────────────────────────────
    void a_live_add_reaches_every_bound_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14074500.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14076000.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        QVERIFY(model.notchModel()->addNotch(14074000.0, 200.0) >= 0);

        QCOMPARE(engine->rxChannel(a)->notchCount(), 1);
        QCOMPARE(engine->rxChannel(b)->notchCount(), 1);

        Notch got;
        QVERIFY(engine->rxChannel(b)->notchAt(0, got));
        QCOMPARE(got.centerHz, 14074000.0);
        QCOMPARE(got.widthHz,  200.0);
        QVERIFY(got.active);
    }

    // ── §6.2: an edit is incremental (one UpdateNBPFilters), not a resync ─
    void a_live_width_edit_reaches_the_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14074500.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        const int id = model.notchModel()->addNotch(14074000.0, 200.0);
        QVERIFY(id >= 0);
        QVERIFY(model.notchModel()->setWidth(id, 400.0));

        RxChannel* ch = engine->rxChannel(a);
        QCOMPARE(ch->notchCount(), 1);
        Notch got;
        QVERIFY(ch->notchAt(0, got));
        QCOMPARE(got.widthHz,  400.0);
        QCOMPARE(got.centerHz, 14074000.0);
    }

    // ── §5.2 + §6.3: delete uses the FORMER index, and WDSP shifts its own
    // array down internally (nbp.c:418-441), so positions stay aligned.
    void a_live_remove_reaches_the_channel_and_keeps_the_order()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14074500.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        NotchModel* nm = model.notchModel();
        const int first  = nm->addNotch(14074000.0, 200.0);
        const int second = nm->addNotch(14100000.0, 500.0);
        QVERIFY(first >= 0);
        QVERIFY(second >= 0);
        QCOMPARE(engine->rxChannel(a)->notchCount(), 2);

        QVERIFY(nm->removeNotch(first));

        RxChannel* ch = engine->rxChannel(a);
        QCOMPARE(ch->notchCount(), 1);
        Notch got;
        QVERIFY(ch->notchAt(0, got));
        QCOMPARE(got.centerHz, 14100000.0);
        QCOMPARE(got.widthHz,  500.0);
    }

    // ── §5.3 clear() contract: a clear that emitted nothing would leave the
    // channels notched while the model showed none.
    void clearing_the_model_empties_every_bound_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14074500.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14076000.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        NotchModel* nm = model.notchModel();
        QVERIFY(nm->addNotch(14074000.0, 200.0) >= 0);
        QVERIFY(nm->addNotch(14100000.0, 500.0) >= 0);
        QCOMPARE(engine->rxChannel(a)->notchCount(), 2);

        nm->clear();

        QCOMPARE(engine->rxChannel(a)->notchCount(), 0);
        QCOMPARE(engine->rxChannel(b)->notchCount(), 0);
    }

    // ── §6.3: master TNF toggle reaches every channel ────────────────────
    void master_enable_flips_the_run_flag_on_every_bound_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14074500.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14076000.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        model.notchModel()->setGlobalEnabled(true);
        QVERIFY(engine->rxChannel(a)->notchesRun());
        QVERIFY(engine->rxChannel(b)->notchesRun());

        model.notchModel()->setGlobalEnabled(false);
        QVERIFY(!engine->rxChannel(a)->notchesRun());
        QVERIFY(!engine->rxChannel(b)->notchesRun());
    }

    // ── §6.3 + bench row 8: auto-increase is the one that goes missing ───
    void auto_increase_flips_on_every_bound_channel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14074500.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(14076000.0);
        model.openRxChannelPool(5, bufferSizeForRate(192000), 192000);

        model.notchModel()->setAutoIncrease(true);
        QVERIFY(engine->rxChannel(a)->notchAutoIncrease());
        QVERIFY(engine->rxChannel(b)->notchAutoIncrease());

        model.notchModel()->setAutoIncrease(false);
        QVERIFY(!engine->rxChannel(a)->notchAutoIncrease());
        QVERIFY(!engine->rxChannel(b)->notchAutoIncrease());
    }
```

- [ ] **Step 23: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_channel_sync && ctest --test-dir build -R '^tst_notch_channel_sync$' --output-on-failure`

Expected: FAIL at runtime in `a_live_add_reaches_every_bound_channel` with `Compared values are not the same / Actual (engine->rxChannel(a)->notchCount()): 0 / Expected (1)` (nothing listens to `NotchModel` yet).

- [ ] **Step 24: Wire `NotchModel`'s signals to the channel fan-out**

`src/models/RadioModel.h`, insert immediately above the `syncNotchesToChannel` declaration added in Step 14:

```cpp
    /// Connect NotchModel's mutation signals to the per-channel WDSP fan-out.
    /// Called once from the ctor; NotchModel outlives every connection.
    void wireNotchModel();
```

`src/models/RadioModel.cpp`, in the ctor block added in Step 4, insert between `m_notchModel = std::make_unique<NotchModel>(this);` and `m_notchModel->restoreFromSettings();`:

```cpp
    // Wired before the restore so a restore that replays its list as signals
    // is handled by the same path a live edit is. Harmless either way here:
    // no WDSP channel exists yet, so the fan-out has nothing to walk.
    wireNotchModel();
```

`src/models/RadioModel.cpp`, insert after the `syncNotchesToAllChannels()` definition added in Step 14:

```cpp
void RadioModel::wireNotchModel()
{
    NotchModel* nm = m_notchModel.get();
    if (!nm) {
        return;
    }

    connect(nm, &NotchModel::notchAdded, this, [this](int id) {
        const int index  = m_notchModel->indexOfId(id);
        const Notch* n   = m_notchModel->notchById(id);
        if (!n || index < 0) { return; }
        const QVector<RxChannel*> chans = sliceRxChannels();
        for (RxChannel* ch : chans) {
            // RXANBPAddNotch is an INSERT guarded by
            // "notch <= b->nn && b->nn < b->maxnotches", returning -1 with no
            // mutation at all (nbp.c:362-390). Design §6.2: surface it, and
            // recover with a full resync rather than an assert, which a
            // release build compiles out.
            if (!ch->addNotch(index, *n)) {
                ch->syncNotches(m_notchModel->notches());
            }
            reconcileNotchCount(ch);
        }
    });

    connect(nm, &NotchModel::notchChanged, this, [this](int id) {
        const int index  = m_notchModel->indexOfId(id);
        const Notch* n   = m_notchModel->notchById(id);
        if (!n || index < 0) { return; }
        const QVector<RxChannel*> chans = sliceRxChannels();
        for (RxChannel* ch : chans) {
            // Incremental, not a resync. RXANBPEditNotch runs UpdateNBPFilters
            // once (nbp.c:345-359), which designs nbp0 AND recalculates
            // bpsnba (snb.c:814-828); syncNotches would pay that 2N times
            // (nbp.c:384, :435, :456). Design §6.2.
            //
            // No throttling on drag, deliberately: Thetis pushes on every
            // mouse-move by named design (console.cs:49967, "//MW0LGE
            // [2.9.0.7] update on drag" [v2.10.3.15]) and does strictly more
            // per move than this (SaveNotchesToDatabase +
            // UpdateNotchDisplay, console.cs:40105-40106).
            if (!ch->editNotch(index, *n)) {
                ch->syncNotches(m_notchModel->notches());
            }
            reconcileNotchCount(ch);
        }
    });

    connect(nm, &NotchModel::notchRemoved, this, [this](int, int formerIndex) {
        if (formerIndex < 0) { return; }
        const QVector<RxChannel*> chans = sliceRxChannels();
        for (RxChannel* ch : chans) {
            // formerIndex, not indexOfId: the entry is gone from the model by
            // the time this lands. WDSP shifts its own array down internally
            // (nbp.c:418-441) and our list does the same, so positions stay
            // aligned (design §5.2).
            if (!ch->deleteNotch(formerIndex)) {
                ch->syncNotches(m_notchModel->notches());
            }
            reconcileNotchCount(ch);
        }
    });

    // Whole-list replacement, including NotchModel::clear(). Design §5.3: a
    // clear that emitted nothing would leave every channel's notch set
    // installed while the model showed none.
    connect(nm, &NotchModel::notchesReset, this, [this]() {
        const QVector<RxChannel*> chans = sliceRxChannels();
        for (RxChannel* ch : chans) {
            ch->syncNotches(m_notchModel->notches());
        }
    });

    // Master TNF toggle. Thetis's TNFActive is likewise global despite the
    // per-rx command shape (console.cs:39987-40005 [v2.10.3.15]).
    connect(nm, &NotchModel::globalEnabledChanged, this, [this](bool on) {
        const QVector<RxChannel*> chans = sliceRxChannels();
        for (RxChannel* ch : chans) {
            ch->setNotchesRun(on);
        }
    });

    connect(nm, &NotchModel::autoIncreaseChanged, this, [this](bool on) {
        const QVector<RxChannel*> chans = sliceRxChannels();
        for (RxChannel* ch : chans) {
            ch->setNotchAutoIncrease(on);
        }
    });
}
```

- [ ] **Step 25: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_channel_sync && ctest --test-dir build -R '^tst_notch_channel_sync$' --output-on-failure`

Expected: PASS (12 slots).

- [ ] **Step 26: Re-run the stream-pool suite to prove the pool tail did not regress**

Run: `cmake --build build --target tst_stream_pool_binding tst_wdsp_channel_id_map && ctest --test-dir build -R '^(tst_stream_pool_binding|tst_wdsp_channel_id_map)$' --output-on-failure`

Expected: PASS (both were already green; `openRxChannelPool` and `activateSliceChannel` both gained a call).

- [ ] **Step 27: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp tests/tst_notch_channel_sync.cpp
git commit -m "feat(notch): fan NotchModel mutations out to every live RX channel"
```

---

### Task 5: TCI repoint and both-index broadcast

**Files:**
- Create: `tests/tst_notch_tci_rx_nf_enable.cpp`
- Modify: `tests/CMakeLists.txt:5733` (register the new test, before the `all_tests` aggregate block at `:5734`)
- Modify: `src/models/RadioModel.h:1684-1695` (doc comment above `setRxNf` / `rxNf`)
- Modify: `src/models/RadioModel.h:2867-2872` (stub-array doc comment)
- Modify: `src/models/RadioModel.h:2904` (delete the dead `m_tciStubRxNf` array)
- Modify: `src/models/RadioModel.cpp:11412-11420` (`setRxNf` / `rxNf` bodies)
- Modify: `src/core/TciProtocol.cpp:2170-2177` (drop the handler's single-index push)
- Modify: `tests/data/tci/matrix.csv:36` (`rx_nf_enable_set_rx0` expected_notifications)
- Modify: `docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md` (regenerated, not hand-edited)
- Modify: `src/core/TciServer.h:122` (add `protocolForTest()` after `peekTxRingSize()`)
- Modify: `src/core/TciServer.h:427` (add `m_notchBroadcastWired` after `m_globalBroadcastsWired`)
- Modify: `src/core/TciServer.cpp:27` (add `#include "models/NotchModel.h"`)
- Modify: `src/core/TciServer.cpp:603` (append the one-shot wire to `hookSliceBroadcasts()`)
- Test: `tests/tst_notch_tci_rx_nf_enable.cpp`

**Interfaces:**
- Consumes (Task 3, `src/models/NotchModel.h`): `bool NotchModel::globalEnabled() const;` / `void NotchModel::setGlobalEnabled(bool on);` / `void NotchModel::globalEnabledChanged(bool on);` (signal, emitted only on change)
- Consumes (Task 4): `NotchModel* RadioModel::notchModel() const;` and the `#include "models/NotchModel.h"` already present in `src/models/RadioModel.cpp`
- Consumes (pre-existing): `void TciProtocol::enqueueLocalBroadcast(const QString& frame);` (`TciProtocol.h:113`), `bool TciProtocol::hasPendingNotification() const;` / `QString TciProtocol::takePendingNotification();` (`TciProtocol.h:81-82`), `void TciServer::hookSliceBroadcasts();` (`TciServer.h:268`)
- Produces: `TciProtocol* TciServer::protocolForTest() const;` (public test seam, returns the owned instance); `void RadioModel::setRxNf(int rx, bool on);` and `bool RadioModel::rxNf(int rx) const;` now backed by `NotchModel::globalEnabled` instead of `m_tciStubRxNf[]`; the wire-frame pair `rx_nf_enable:0,<bool>;` + `rx_nf_enable:1,<bool>;` queued on every `NotchModel::globalEnabledChanged`; private `bool TciServer::m_notchBroadcastWired{false};`

Plan comments on choices the spec left open:
- Spec §6.4 names no seam for observing the outbound queue. Adding `TciServer::protocolForTest()` follows the in-tree `ForTest` convention and the existing test-only members on this same class (`peekTxRingSize()` at `TciServer.h:122`, `injectAudioFrameForTest` at `:140`). It keeps the test free of a real socket and of the 5 ms drain timer, so nothing sleeps.
- Spec §6.4 says "one-shot" without naming the mechanism. `hookSliceBroadcasts()` runs from the constructor (`TciServer.cpp:467`) **and** from every `start()` (`TciServer.cpp:1191`), and `stop()`'s `QObject::disconnect(m_model, nullptr, this, nullptr)` (`TciServer.cpp:1220`) is rooted on `RadioModel`, so it cannot reach a `NotchModel`-rooted connection. A `bool m_notchBroadcastWired` that is set once and **never reset in `stop()`** is therefore the correct guard; resetting it (as `m_globalBroadcastsWired` does) would double-wire on restart.
- Spec §6.4 does not mention `tests/data/tci/matrix.csv:36`, whose `expected_notifications` cell pins the exact single-index push §6.4 orders dropped. `tst_tci_matrix_runner` asserts that cell (`QCOMPARE(notifs.join("|"), r.expectedNotifs)`), so the row must be updated in the same commit or the suite goes red.
- Spec does not say what becomes of the now-dead `m_tciStubRxNf[]` array or of out-of-range `rx`. The array is deleted (it has no other reader), and the shims keep Thetis's 0..1 receiver guard from `TCIServer.cs:3388` / `console.cs:52320 [v2.10.3.15]`.

---

- [ ] **Step 1: Write the failing test (round-trip half)**

Create `tests/tst_notch_tci_rx_nf_enable.cpp`:

```cpp
// =================================================================
// tests/tst_notch_tci_rx_nf_enable.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF §6.4: rx_nf_enable is repointed off the m_tciStubRxNf[] array and
// onto the real NotchModel master enable, the handler's single-index
// push is dropped, and the both-index broadcast Thetis drives from
// TNFChangedHandlers is wired in TciServer::hookSliceBroadcasts().
//
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §6.4.
// Upstream (all [v2.10.3.15]):
//   TCIServer.cs:3384-3400   handleRxNfEnable (query + set branches)
//   TCIServer.cs:1954-1958   sendRxNfEnable wire format
//   TCIServer.cs:1315-1320   NfChanged -> sendRxNfEnable(0) AND (1)
//   TCIServer.cs:6771        console.TNFChangedHandlers += OnTnfChanged
//   TCIServer.cs:7686-7697   OnTnfChanged -> NfChanged on every listener
//   console.cs:40004         gates the handler fire on old_tnf != value
//   console.cs:52317-52326   GetMNF -- global flag behind a per-rx shape
// =================================================================

#ifdef HAVE_WEBSOCKETS

#include <QtTest/QtTest>
#include <QStringList>

#include "core/TciProtocol.h"
#include "core/TciServer.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestNotchTciRxNfEnable : public QObject {
    Q_OBJECT

private:
    // Drain every frame the protocol has queued for broadcast.  TciServer's
    // 5 ms drain timer only ticks while the event loop runs between start()
    // and stop(); these tests read the queue directly so nothing waits.
    static QStringList drain(TciProtocol* p)
    {
        QStringList out;
        while (p->hasPendingNotification()) {
            out << p->takePendingNotification();
        }
        return out;
    }

private slots:
    // ── Round-trip against the real master enable ────────────────────────

    void set_on_rx0_flips_the_master_enable()
    {
        RadioModel m;
        TciProtocol p(&m);
        QVERIFY(m.notchModel());
        QVERIFY(!m.notchModel()->globalEnabled());

        p.handleCommand(QStringLiteral("rx_nf_enable:0,true;"));
        QVERIFY(m.notchModel()->globalEnabled());
    }

    void set_on_rx1_flips_the_same_master_enable()
    {
        RadioModel m;
        TciProtocol p(&m);
        QVERIFY(m.notchModel());

        // TCI addresses receivers as rx 0|1 but the notch enable is global,
        // exactly as Thetis GetMNF is: console.cs:52317-52326 [v2.10.3.15]
        // returns TNFActive for either index.
        p.handleCommand(QStringLiteral("rx_nf_enable:1,true;"));
        QVERIFY(m.notchModel()->globalEnabled());

        p.handleCommand(QStringLiteral("rx_nf_enable:1,false;"));
        QVERIFY(!m.notchModel()->globalEnabled());
    }

    void query_reports_the_master_enable_on_both_indices()
    {
        RadioModel m;
        TciProtocol p(&m);
        QVERIFY(m.notchModel());

        m.notchModel()->setGlobalEnabled(true);
        QCOMPARE(p.handleCommand(QStringLiteral("rx_nf_enable:0;")),
                 QStringLiteral("rx_nf_enable:0,true;"));
        QCOMPARE(p.handleCommand(QStringLiteral("rx_nf_enable:1;")),
                 QStringLiteral("rx_nf_enable:1,true;"));

        m.notchModel()->setGlobalEnabled(false);
        QCOMPARE(p.handleCommand(QStringLiteral("rx_nf_enable:0;")),
                 QStringLiteral("rx_nf_enable:0,false;"));
        QCOMPARE(p.handleCommand(QStringLiteral("rx_nf_enable:1;")),
                 QStringLiteral("rx_nf_enable:1,false;"));
    }

    void out_of_range_rx_index_leaves_the_master_enable_alone()
    {
        RadioModel m;
        TciProtocol p(&m);
        QVERIFY(m.notchModel());

        // handleRxNfEnable rejects rx outside 0..1 (TCIServer.cs:3388
        // [v2.10.3.15]); GetMNF rejects its 1-based equivalent
        // (console.cs:52320 [v2.10.3.15]).  The shim guards too, so a
        // caller that bypasses the handler cannot corrupt the flag.
        p.handleCommand(QStringLiteral("rx_nf_enable:2,true;"));
        QVERIFY(!m.notchModel()->globalEnabled());

        QMetaObject::invokeMethod(&m, "setRxNf", Qt::DirectConnection,
                                  Q_ARG(int, 7), Q_ARG(bool, true));
        QVERIFY(!m.notchModel()->globalEnabled());

        bool out = true;
        QMetaObject::invokeMethod(&m, "rxNf", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, out), Q_ARG(int, 7));
        QVERIFY(!out);
    }
};

QTEST_MAIN(TestNotchTciRxNfEnable)
#include "tst_notch_tci_rx_nf_enable.moc"

#else
// WebSockets not available -- TciServer.h is #ifdef HAVE_WEBSOCKETS.  The
// binary must still link so CTest doesn't report a missing executable.
int main() { return 0; }
#endif // HAVE_WEBSOCKETS
```

- [ ] **Step 2: Register the test, run it, watch it fail**

Insert immediately after `nereus_add_test(tst_diversity_dialog_persistence)` at `tests/CMakeLists.txt:5733`, before the `# Aggregate "all_tests" target` banner (the aggregate reads a GLOBAL property, so registrations must precede it):

```cmake
# TNF §6.4: rx_nf_enable repoint + both-index broadcast.
# Verifies the TCI master-notch surface against the real NotchModel:
#   - set on rx 0 or rx 1 writes the single global enable
#   - query on either index reports it
#   - the set handler queues NO notification of its own
#   - a UI-originated flip broadcasts BOTH indices, exactly once, and
#     survives a stop()/start() cycle without duplicating
# Source: Thetis TCIServer.cs:3384-3400 / 1315-1320 / 6771 / 7686-7697 and
#         console.cs:40004 / 52317-52326 [v2.10.3.15].
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §6.4.
# Gated on Qt6WebSockets_FOUND -- TciServer.h is #ifdef HAVE_WEBSOCKETS.
if(Qt6WebSockets_FOUND)
    nereus_add_test(tst_notch_tci_rx_nf_enable)
    target_link_libraries(tst_notch_tci_rx_nf_enable PRIVATE Qt6::WebSockets)
endif()
```

Run: `cmake --build build --target tst_notch_tci_rx_nf_enable && ctest --test-dir build -R '^tst_notch_tci_rx_nf_enable$' --output-on-failure`

Expected: FAIL. Compiles clean (both `notchModel()` and the shims already exist), then fails at runtime in `set_on_rx0_flips_the_master_enable` with `'m.notchModel()->globalEnabled()' returned FALSE` at the line after `handleCommand("rx_nf_enable:0,true;")`, because `RadioModel::setRxNf` still writes `m_tciStubRxNf[0]`. `query_reports_the_master_enable_on_both_indices` fails the same way with `Compared values are not the same / Actual: "rx_nf_enable:0,false;" / Expected: "rx_nf_enable:0,true;"`.

- [ ] **Step 3: Repoint `setRxNf` / `rxNf` at `NotchModel`**

`src/models/RadioModel.cpp:11412-11420`, replace both bodies:

```cpp
// TNF §6.4: rx_nf_enable is the master notch enable, not a per-rx stub.
// Thetis addresses it as rx 0|1 on the wire but the flag itself is global:
// GetMNF returns Console.TNFActive for either index
// (console.cs:52317-52326 [v2.10.3.15]) and the set branch writes that one
// property (TCIServer.cs:3397 [v2.10.3.15]).
//   // mnf enabled globally  [original inline comment from console.cs:52319]
void RadioModel::setRxNf(int rx, bool on)
{
    // From Thetis TCIServer.cs:3388 [v2.10.3.15] ,  "if (rx < 0 || rx > 1) return;"
    if (rx < 0 || rx > 1) { return; }
    if (m_notchModel) { m_notchModel->setGlobalEnabled(on); }
}
bool RadioModel::rxNf(int rx) const
{
    // From Thetis console.cs:52320 [v2.10.3.15] ,  "if (rx < 1 || rx > 2) return false;"
    // Thetis indexes receivers 1-based there; TCI hands us the 0-based index.
    if (rx < 0 || rx > 1) { return false; }
    return m_notchModel && m_notchModel->globalEnabled();
}
```

`src/models/RadioModel.h:1684-1695`, extend the block comment so the exception is visible at the declaration:

```cpp
    // ── Stub categories: SliceModel doesn't expose these as Q_PROPERTYs yet ─
    // Each stub stores the requested value in a small per-slice array so
    // round-trip (set then get) returns the operator's last value.  Real
    // wiring to WDSP comes when the underlying feature lands.
    Q_INVOKABLE void setRxBin(int rx, bool on);
    Q_INVOKABLE bool rxBin(int rx) const;
    Q_INVOKABLE void setRxApf(int rx, bool on);
    Q_INVOKABLE bool rxApf(int rx) const;
    // NOT a stub since TNF §6.4: these read and write the NotchModel master
    // enable.  Global despite the per-rx command shape, exactly as Thetis
    // GetMNF is (console.cs:52317-52326 [v2.10.3.15]).
    Q_INVOKABLE void setRxNf(int rx, bool on);
    Q_INVOKABLE bool rxNf(int rx) const;
    Q_INVOKABLE void setRxEnable(int rx, bool on);
    Q_INVOKABLE bool rxEnable(int rx) const;
```

`src/models/RadioModel.h:2867-2871`, drop `rxNf` from the stub roster:

```cpp
    // Per-slice stub state for DSP toggles SliceModel doesn't yet expose
    // as Q_PROPERTYs: rxBin / rxApf / rxEnable.  Sized to the max
    // RX count NereusSDR supports today (4 for the four-DDC SKUs); the
    // setter clamps the index so an out-of-range slice silently no-ops.
    // rxNf left this set in TNF §6.4: it is the global notch master enable
    // and now reads and writes NotchModel::globalEnabled.
    static constexpr int kTciStubSliceMax = 4;
```

`src/models/RadioModel.h:2904`, delete the now-unread array (this exact line, leaving the four siblings):

```cpp
    std::array<bool, kTciStubSliceMax> m_tciStubRxNf{};
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_tci_rx_nf_enable && ctest --test-dir build -R '^tst_notch_tci_rx_nf_enable$' --output-on-failure`

Expected: PASS (4 slots).

- [ ] **Step 5: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp \
        tests/tst_notch_tci_rx_nf_enable.cpp tests/CMakeLists.txt
git commit -m "fix(tci): repoint rx_nf_enable at the real notch master enable"
```

---

- [ ] **Step 1: Write the failing test for the dropped self-notification**

Append this slot to `tests/tst_notch_tci_rx_nf_enable.cpp`, after `out_of_range_rx_index_leaves_the_master_enable_alone()`:

```cpp
    // ── The set handler must not queue a frame of its own ────────────────

    void set_does_not_queue_its_own_single_index_notification()
    {
        RadioModel m;
        TciProtocol p(&m);
        QVERIFY(m.notchModel());

        p.handleCommand(QStringLiteral("rx_nf_enable:0,true;"));

        // Thetis's set branch sends nothing (TCIServer.cs:3394-3398
        // [v2.10.3.15]); the wire frames come from TNFChangedHandlers ->
        // OnTnfChanged -> NfChanged, which sends BOTH indices
        // (TCIServer.cs:1315-1320 [v2.10.3.15]).  A single-index push from
        // the handler is a wrong-arity duplicate of that pair.
        const QStringList queued = drain(&p);
        QVERIFY2(queued.isEmpty(),
                 qPrintable(QStringLiteral("handler queued: %1")
                                .arg(queued.join(QLatin1Char('|')))));
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_tci_rx_nf_enable && ctest --test-dir build -R '^tst_notch_tci_rx_nf_enable$' --output-on-failure`

Expected: FAIL in `set_does_not_queue_its_own_single_index_notification` with `handler queued: rx_nf_enable:0,true;` (the push at `TciProtocol.cpp:2174-2175` is still live).

- [ ] **Step 3: Drop the push and re-pin the matrix row**

`src/core/TciProtocol.cpp:2170-2177`, replace the set branch:

```cpp
    if (args.size() >= 2) {
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        QMetaObject::invokeMethod(m_radio, "setRxNf", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(bool, en));
        // No notification queued here.  From Thetis TCIServer.cs:3394-3398
        // [v2.10.3.15] the set branch sends nothing at all; the wire frames
        // come from TNFChangedHandlers -> OnTnfChanged (TCIServer.cs:6771,
        // :7686-7697) -> NfChanged, which sends BOTH indices
        // (TCIServer.cs:1315-1320 [v2.10.3.15]) because the flag is global.
        // TciServer::hookSliceBroadcasts carries that path here, so a
        // single-index push from this handler would be a wrong-arity
        // duplicate.  Thetis also gates the fire on change
        // (console.cs:40004 [v2.10.3.15]); NotchModel::globalEnabledChanged
        // gives us the same gate for free.
        return {};
    }
```

`tests/data/tci/matrix.csv:36`, blank the `expected_notifications` cell (fourth field) and restate the notes. Keep the cell text comma-free: `smartSplit` treats an unescaped comma as a field break and `loadMatrix` silently drops the overflow.

```
rx_nf_enable_set_rx0,rx_nf_enable:0\,true;,,,TCIServer.cs:4959 [v2.10.3.13],NF (Notch Filter) enable set writes the global master enable NotchModel::globalEnabled; handleRxNfEnable at TCIServer.cs:3249 [v2.10.3.13]; the handler queues no notification because Thetis's set branch sends nothing (TCIServer.cs:3394-3398 [v2.10.3.15]) and the both-index broadcast comes from TNFChangedHandlers -> NfChanged (TCIServer.cs:1315-1320 [v2.10.3.15]) which NereusSDR wires in TciServer::hookSliceBroadcasts; covered by tst_notch_tci_rx_nf_enable
```

Regenerate the mirrored table (the README carries a "do not edit by hand" banner):

```bash
python3 scripts/gen-tci-matrix-readme.py \
  > docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md
```

- [ ] **Step 4: Run both affected tests, watch them pass**

Run: `cmake --build build --target tst_notch_tci_rx_nf_enable --target tst_tci_matrix_runner && ctest --test-dir build -R '^(tst_notch_tci_rx_nf_enable|tst_tci_matrix_runner)$' --output-on-failure`

Expected: PASS, 2 tests (5 slots in `tst_notch_tci_rx_nf_enable`, `allRowsPass` in the matrix runner).

- [ ] **Step 5: Commit**

```bash
git add src/core/TciProtocol.cpp tests/data/tci/matrix.csv \
        docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md \
        tests/tst_notch_tci_rx_nf_enable.cpp
git commit -m "fix(tci): drop the redundant single-index rx_nf_enable push"
```

---

- [ ] **Step 1: Write the failing test for the both-index broadcast**

Append these three slots to `tests/tst_notch_tci_rx_nf_enable.cpp`, after `set_does_not_queue_its_own_single_index_notification()`:

```cpp
    // ── UI-originated flip broadcasts both indices ───────────────────────

    void ui_flip_broadcasts_both_rx_indices()
    {
        RadioModel m;
        TciServer  server(&m);   // the ctor runs hookSliceBroadcasts()
        TciProtocol* p = server.protocolForTest();
        QVERIFY(p);
        drain(p);                // discard anything queued during wireup

        m.notchModel()->setGlobalEnabled(true);

        // From Thetis TCIServer.cs:1315-1320 [v2.10.3.15] ,  NfChanged calls
        // sendRxNfEnable(0, newState) then sendRxNfEnable(1, newState).
        QCOMPARE(drain(p),
                 (QStringList{QStringLiteral("rx_nf_enable:0,true;"),
                              QStringLiteral("rx_nf_enable:1,true;")}));

        m.notchModel()->setGlobalEnabled(false);
        QCOMPARE(drain(p),
                 (QStringList{QStringLiteral("rx_nf_enable:0,false;"),
                              QStringLiteral("rx_nf_enable:1,false;")}));
    }

    void repeat_flip_to_the_same_value_broadcasts_nothing()
    {
        RadioModel m;
        TciServer  server(&m);
        TciProtocol* p = server.protocolForTest();
        QVERIFY(p);

        m.notchModel()->setGlobalEnabled(true);
        drain(p);

        // Thetis gates the handler fire on change:
        //   if (old_tnf != value) TNFChangedHandlers?.Invoke(old_tnf, value);
        // (console.cs:40004 [v2.10.3.15]).  NotchModel's change-guarded
        // signal is our equivalent gate.
        m.notchModel()->setGlobalEnabled(true);
        QCOMPARE(drain(p), QStringList());
    }

    void wire_survives_stop_start_without_duplicating()
    {
        RadioModel m;
        TciServer  server(&m);
        TciProtocol* p = server.protocolForTest();
        QVERIFY(p);

        // hookSliceBroadcasts runs from the ctor AND from every start().
        // stop()'s QObject::disconnect(m_model, nullptr, this, nullptr) is
        // rooted on RadioModel, so it cannot sever a NotchModel-rooted
        // connection: without a wire-once guard this would leave three
        // subscribers and emit three frame pairs per flip.
        QVERIFY(server.start(0));
        server.stop();
        QVERIFY(server.start(0));
        server.stop();   // also parks the 5 ms drain timer so the queue is ours
        drain(p);

        m.notchModel()->setGlobalEnabled(true);
        QCOMPARE(drain(p),
                 (QStringList{QStringLiteral("rx_nf_enable:0,true;"),
                              QStringLiteral("rx_nf_enable:1,true;")}));
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_tci_rx_nf_enable && ctest --test-dir build -R '^tst_notch_tci_rx_nf_enable$' --output-on-failure`

Expected: FAIL at compile time. `error: no member named 'protocolForTest' in 'NereusSDR::TciServer'` at the first `server.protocolForTest()` call, repeated for all three slots.

- [ ] **Step 3: Add the test seam**

`src/core/TciServer.h`, immediately after `peekTxRingSize()` at `:122`:

```cpp
    // Test-only: the shared TciProtocol instance, so a test can inspect the
    // outbound notification queue without binding a socket or spinning the
    // 5ms drain timer.  Used by tst_notch_tci_rx_nf_enable to assert the
    // both-index rx_nf_enable broadcast.  Never call from production code:
    // TciServer owns the lifetime and the pointer dies with this object.
    TciProtocol* protocolForTest() const { return m_protocol.get(); }
```

- [ ] **Step 4: Add the wire-once guard flag**

`src/core/TciServer.h`, immediately after `bool m_globalBroadcastsWired{false};` at `:427`:

```cpp
    // TNF §6.4: guard flag for the NotchModel master-enable broadcast wired
    // by hookSliceBroadcasts.  Deliberately NOT reset in stop(), unlike
    // m_globalBroadcastsWired above: stop()'s wholesale
    // QObject::disconnect(m_model, nullptr, this, nullptr) is rooted on
    // RadioModel, and NotchModel is a separate QObject, so that connection
    // survives a stop()/start() cycle intact.  hookSliceBroadcasts runs from
    // the constructor AND from every start(), so without this flag each
    // restart would add another subscriber and every flip would emit
    // duplicate rx_nf_enable frames.
    bool m_notchBroadcastWired{false};
```

- [ ] **Step 5: Wire the one-shot broadcast**

`src/core/TciServer.cpp:27`, add after the `models/SliceModel.h` include:

```cpp
#include "models/NotchModel.h"     // TNF §6.4: master notch enable broadcast.
```

`src/core/TciServer.cpp`, append inside `hookSliceBroadcasts()` after the `RadioModel::sliceAdded` connect (currently ending at `:603`) and before the closing brace at `:604`:

```cpp
    // ── Master notch enable (rx_nf_enable:, BOTH rx indices) ────────────────
    // Source: Thetis console.TNFChangedHandlers subscription at
    // TCIServer.cs:6771 [v2.10.3.15], routed to OnTnfChanged
    // (TCIServer.cs:7686-7697 [v2.10.3.15]) which calls NfChanged on every
    // listener; NfChanged sends sendRxNfEnable(0, ...) AND
    // sendRxNfEnable(1, ...) (TCIServer.cs:1315-1320 [v2.10.3.15]) because
    // the flag is global despite the per-rx command shape -- GetMNF returns
    // TNFActive for either index.
    //   // mnf enabled globally  [original inline comment from console.cs:52319]
    //
    // Wired HERE and not in wireSliceForBroadcast because NotchModel is
    // radio-global: wireSliceForBroadcast runs once per slice, so the same
    // connect placed there would emit N frame pairs per flip.
    if (!m_notchBroadcastWired) {
        if (NotchModel* notch = m_model->notchModel()) {
            connect(notch, &NotchModel::globalEnabledChanged, this,
                    [this](bool on) {
                        const QString boolStr = on ? QStringLiteral("true")
                                                   : QStringLiteral("false");
                        m_protocol->enqueueLocalBroadcast(
                            QStringLiteral("rx_nf_enable:0,%1;").arg(boolStr));
                        m_protocol->enqueueLocalBroadcast(
                            QStringLiteral("rx_nf_enable:1,%1;").arg(boolStr));
                    });
            m_notchBroadcastWired = true;
        }
    }
```

- [ ] **Step 6: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_tci_rx_nf_enable && ctest --test-dir build -R '^tst_notch_tci_rx_nf_enable$' --output-on-failure`

Expected: PASS (8 slots).

- [ ] **Step 7: Re-run the neighbouring TCI suites for regressions**

Run: `cmake --build build --target tst_tci_matrix_runner --target tst_tci_server_lifecycle --target tst_tci_radio_model_shims --target tst_tci_init_burst_live_state && ctest --test-dir build -R '^tst_tci_' --output-on-failure`

Expected: PASS. `tst_tci_init_burst_live_state` still asserts `rx_nf_enable:0,true;` alongside `rx_nf_enable:1,false;` and stays green because both init-burst suites bind `TestMockRadioModel`, whose per-index array is untouched by this task.

- [ ] **Step 8: Commit**

```bash
git add src/core/TciServer.h src/core/TciServer.cpp tests/tst_notch_tci_rx_nf_enable.cpp
git commit -m "feat(tci): broadcast the notch master enable on both rx indices"
```

---

### Task 6: SpectrumWidget notch marker rendering, pushed to every pan

Governing spec: §8.1 (push API), §8.2 (rendering), §12 step 6.

> **Plan comment (scope).** Spec §12 step 6 says "No new test executable; the hit
> test arrives with step 7." Step 7 budgets `tst_notch_hit_test`. This task
> therefore **creates** `tests/tst_notch_hit_test.cpp` and registers it once;
> Task 7 adds its hit-test slots to the same file. Net new executables across
> steps 6 + 7 stays at one, which is what step 6's line is protecting (each test
> binary costs ~38 CPU-s to link).
>
> **Plan comment (`Notch` placement).** The shared contract flags that the spec
> never says whether `Notch` is nested in `NotchModel` or free in `NereusSDR`.
> Task 6 does not need to know: the conversion loops here use `const auto&` over
> `notchModel()->notches()` and touch only `.id` / `.centerHz` / `.widthHz` /
> `.active`, so they compile under either choice Task 3 makes.
>
> **Plan comment (not tested here).** `MainWindow` is deliberately never
> constructed in this suite (see the banner of
> `tests/tst_mainwindow_tools_spot_hub.cpp`), so the `m_panStack` loop itself is
> covered by the §11.2 bench matrix. Cycle 5's test pins the part that can drift
> silently: the `NotchModel` → `NotchMarker` conversion and the every-pan push.

**Files:**
- Create: `tests/tst_notch_hit_test.cpp`
- Modify: `src/gui/SpectrumWidget.h:1120-1122` (public push API + test seams)
- Modify: `src/gui/SpectrumWidget.h:1155-1156` (five interaction signals)
- Modify: `src/gui/SpectrumWidget.h:1321-1322` (private `drawNotchMarkers` / `notchColor`)
- Modify: `src/gui/SpectrumWidget.h:1726-1727` (private notch state members)
- Modify: `src/gui/SpectrumWidget.cpp:5146-5148` (new notch overlay section)
- Modify: `src/gui/SpectrumWidget.cpp:3037-3038` (CPU `paintEvent` call site)
- Modify: `src/gui/SpectrumWidget.cpp:7138-7139` (GPU static-overlay call site)
- Modify: `src/gui/MainWindow.h:234-236` (fan-out + inbound slot declarations)
- Modify: `src/gui/MainWindow.cpp:308-309` (`models/NotchModel.h` include)
- Modify: `src/gui/MainWindow.cpp:1519-1520` (fan-out + inbound slot bodies)
- Modify: `src/gui/MainWindow.cpp:2168-2174` (countChanged hook + NotchModel connects)
- Modify: `src/gui/MainWindow.cpp:2182-2184` (initial arm before `applyLayout`)
- Modify: `tests/CMakeLists.txt:5733` (register `tst_notch_hit_test`)
- Test: `tests/tst_notch_hit_test.cpp`

**Interfaces:**

- Consumes (Task 3, `src/models/NotchModel.h`):
  - `class NotchModel : public QObject` with `explicit NotchModel(QObject* parent = nullptr);`
  - `const QList<Notch>& notches() const;` where each element exposes `int id`, `double centerHz`, `double widthHz`, `bool active`
  - `bool globalEnabled() const;`
  - `int  addNotch(double centerHz, double widthHz = 200.0);`
  - `bool setCenter(int id, double centerHz);`
  - `bool setWidth(int id, double widthHz);`
  - `bool setActive(int id, bool active);`
  - `bool removeNotch(int id);`
  - `void setGlobalEnabled(bool on);`
  - `static constexpr double kNotchDefaultWidthHz = 200.0;` (Thetis `console.cs:40268 [v2.10.3.15]`)
  - `static constexpr double kNotchNarrowWidthHz  = 100.0;` (Thetis `console.cs:40269 [v2.10.3.15]`)
  - signals `notchAdded(int)`, `notchChanged(int)`, `notchRemoved(int,int)`, `globalEnabledChanged(bool)`, `notchesReset()`
- Consumes (Task 4, `src/models/RadioModel.h`):
  - `NotchModel* notchModel() const;`
- Consumes (already in tree):
  - `QList<PanadapterApplet*> PanadapterStack::allApplets() const;` (`PanadapterStack.h:70`)
  - `SpectrumWidget* PanadapterStack::spectrum(const QString& panId) const;` (`PanadapterStack.h:74`)
  - `SpectrumWidget* PanadapterApplet::spectrumWidget() const;` (`PanadapterApplet.h:68`)
  - `QString PanadapterApplet::panId() const;`
  - `int SpectrumWidget::hzToX(double hz, const QRect& r) const;` (`SpectrumWidget.cpp:4025`)
  - `void SpectrumWidget::markOverlayDirty();` (`SpectrumWidget.h:2059`)
- Produces (Task 7 and Task 10 rely on these):
  - `struct SpectrumWidget::NotchMarker { int id{-1}; double freqMhz{0.0}; double widthHz{200.0}; bool active{true}; };`
  - `void SpectrumWidget::setNotchMarkers(const QVector<NotchMarker>& markers);`
  - `void SpectrumWidget::setNotchGlobalEnabled(bool on);`
  - `void SpectrumWidget::setNotchMinWidthHz(double hz);`
  - `const QVector<NotchMarker>& SpectrumWidget::notchMarkersForTest() const;`
  - `bool SpectrumWidget::notchGlobalEnabledForTest() const;`
  - `double SpectrumWidget::notchMinWidthHzForTest() const;`
  - `void SpectrumWidget::drawNotchMarkersForTest(QPainter& p, const QRect& specRect);`
  - `void SpectrumWidget::setSelectedNotchIdForTest(int id);`
  - `void SpectrumWidget::setHoveredNotchIdForTest(int id);`
  - `bool SpectrumWidget::overlayStaticDirtyForTest() const;`
  - `void SpectrumWidget::clearOverlayStaticDirtyForTest();`
  - signals `notchCreateRequested(double freqHz, bool narrow)`, `notchMoveRequested(int id, double newFreqHz)`, `notchWidthRequested(int id, double widthHz)`, `notchActiveRequested(int id, bool active)`, `notchRemoveRequested(int id)`
  - private `void SpectrumWidget::drawNotchMarkers(QPainter& p, const QRect& specRect);`
  - private `QColor SpectrumWidget::notchColor(const NotchMarker& n) const;`
  - private members `QVector<NotchMarker> m_notchMarkers; bool m_notchGlobalEnabled{true}; double m_notchMinWidthHz{100.0}; int m_selectedNotchId{-1}; int m_hoveredNotchId{-1};`
  - `void MainWindow::refreshPanNotchMarkers();`
  - `void MainWindow::wirePanNotchHandlers();`
  - `void MainWindow::onNotchCreateRequested(double freqHz, bool narrow);`
  - `void MainWindow::onNotchMoveRequested(int id, double newFreqHz);`
  - `void MainWindow::onNotchWidthRequested(int id, double widthHz);`
  - `void MainWindow::onNotchActiveRequested(int id, bool active);`
  - `void MainWindow::onNotchRemoveRequested(int id);`

---

#### Cycle 1: the push API

- [ ] **Step 1: Write the failing test**

Create `tests/tst_notch_hit_test.cpp`:

```cpp
// =================================================================
// tests/tst_notch_hit_test.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Tunable Notch Filter (TNF).
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         §8.1 (push API), §8.2 (rendering).
//
// Build order (design §12) puts the push API and the marker render in
// step 6 and the pixel hit test in step 7; both live in this one
// executable so the suite gains a single new binary, not two.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>

#include "gui/SpectrumWidget.h"
#include "gui/PanadapterStack.h"
#include "models/NotchModel.h"

using namespace NereusSDR;

namespace {

// Pan geometry shared by every render test.  8 kHz across 800 px is
// 10 Hz per pixel, so a 200 Hz notch is 20 px wide and both of its edge
// columns land on distinct, assertable pixels.
constexpr double kCentreHz    = 14'250'000.0;
constexpr double kBandwidthHz = 8'000.0;
constexpr int    kPanW        = 800;
constexpr int    kPanH        = 400;
constexpr int    kSpecH       = 200;

QRect specRect() { return QRect(0, 0, kPanW, kSpecH); }

// Reproduces SpectrumWidget::hzToX (src/gui/SpectrumWidget.cpp:4025-4030)
// term for term, in the same order and the same types, so expected pixel
// columns are exact rather than tolerance-bounded.
int expectX(double hz)
{
    const double lowHz = kCentreHz - kBandwidthHz / 2.0;
    const double frac  = (hz - lowHz) / kBandwidthHz;
    return 0 + static_cast<int>(frac * kPanW);
}

SpectrumWidget::NotchMarker makeNotch(int id, double freqHz, double widthHz,
                                      bool active = true)
{
    SpectrumWidget::NotchMarker n;
    n.id      = id;
    n.freqMhz = freqHz / 1.0e6;
    n.widthHz = widthHz;
    n.active  = active;
    return n;
}

QImage renderNotches(SpectrumWidget& sw)
{
    QImage img(specRect().size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, false);
    sw.drawNotchMarkersForTest(p, specRect());
    p.end();
    return img;
}

} // namespace

class TestNotchHitTest : public QObject {
    Q_OBJECT
private slots:
    // ── §8.1 push API ────────────────────────────────────────────────────
    void marker_list_is_empty_by_default()
    {
        SpectrumWidget sw;
        QCOMPARE(sw.notchMarkersForTest().size(), 0);
    }

    void marker_push_round_trips()
    {
        SpectrumWidget sw;
        sw.setNotchMarkers({makeNotch(7, kCentreHz, 200.0)});

        QCOMPARE(sw.notchMarkersForTest().size(), 1);
        QCOMPARE(sw.notchMarkersForTest().first().id, 7);
        QCOMPARE(sw.notchMarkersForTest().first().widthHz, 200.0);
        QCOMPARE(sw.notchMarkersForTest().first().active, true);
    }

    void global_enabled_defaults_true_and_round_trips()
    {
        SpectrumWidget sw;
        QCOMPARE(sw.notchGlobalEnabledForTest(), true);
        sw.setNotchGlobalEnabled(false);
        QCOMPARE(sw.notchGlobalEnabledForTest(), false);
    }

    // 100 Hz is what wintype-0 min_notch_width yields on this tree:
    // 1600 / (4096 / 256) * (48000 / 48000), third_party/wdsp/src/nbp.c:88.
    void min_notch_width_defaults_to_100_and_round_trips()
    {
        SpectrumWidget sw;
        QCOMPARE(sw.notchMinWidthHzForTest(), 100.0);
        sw.setNotchMinWidthHz(400.0);
        QCOMPARE(sw.notchMinWidthHzForTest(), 400.0);
    }

    // §8.2: notch chrome lives in the cached GPU static-overlay texture,
    // so every mutator must invalidate it.  A bare update() (which is all
    // the spot push does) leaves a dragged marker frozen on the shipping
    // path, where NEREUS_GPU_SPECTRUM is ON by default (CMakeLists.txt:420).
    void every_notch_mutator_invalidates_the_static_overlay()
    {
#ifdef NEREUS_GPU_SPECTRUM
        SpectrumWidget sw;

        sw.clearOverlayStaticDirtyForTest();
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0)});
        QVERIFY2(sw.overlayStaticDirtyForTest(),
                 "setNotchMarkers did not call markOverlayDirty()");

        sw.clearOverlayStaticDirtyForTest();
        sw.setNotchGlobalEnabled(false);
        QVERIFY2(sw.overlayStaticDirtyForTest(),
                 "setNotchGlobalEnabled did not call markOverlayDirty()");

        sw.clearOverlayStaticDirtyForTest();
        sw.setNotchMinWidthHz(400.0);
        QVERIFY2(sw.overlayStaticDirtyForTest(),
                 "setNotchMinWidthHz did not call markOverlayDirty()");
#else
        QSKIP("no cached overlay texture on the CPU-only spectrum path");
#endif
    }
};

QTEST_MAIN(TestNotchHitTest)
#include "tst_notch_hit_test.moc"
```

- [ ] **Step 2: Register the test in CMake**

Insert into `tests/CMakeLists.txt` at line 5733, immediately after
`nereus_add_test(tst_diversity_dialog_persistence)` and before the
`Aggregate "all_tests" target` banner:

```cmake

# TNF build order step 6 + 7: SpectrumWidget notch overlay.
# Step 6 pins the §8.1 push API (setNotchMarkers / setNotchGlobalEnabled /
# setNotchMinWidthHz + the ForTest read seams), the §8.2 render geometry
# (fill, hatch, 1 px edge lines, +/-5 px triangle grab handle), the four
# Thetis marker colours, and the every-pan push each SpectrumWidget
# converts into its own pixel space.  Step 7 adds the pixel hit test to
# the same executable via notchAtPixelForTest.
# Source: AetherSDR src/gui/SpectrumWidget.cpp:13436-13554 [@c6481cbf]
# (setTnfMarkers / setTnfGlobalEnabled / drawTnfMarkers) for geometry;
# Thetis display.cs:386-390, 8691-8722 [v2.10.3.15] for colours.
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §8.
nereus_add_test(tst_notch_hit_test)
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`error: no type named 'NotchMarker' in 'NereusSDR::SpectrumWidget'` and
`error: no member named 'setNotchMarkers' in 'NereusSDR::SpectrumWidget'`.

- [ ] **Step 4: Add the push API to SpectrumWidget**

In `src/gui/SpectrumWidget.h`, insert after line 1120
(`QColor spotBgColorForTest() const { return m_spotBgColor; }`) and before
the blank line preceding `signals:` at 1122:

```cpp

    // ── TNF / notch overlay (design §8.1) ─────────────────────────────────
    // Ported from AetherSDR's TnfMarker (src/gui/SpectrumWidget.h:575-581
    // [@c6481cbf]) with depthDb + permanent replaced by `active`: WDSP's
    // notch DB carries neither depth nor permanence, its add entry point
    // taking fcenter / fwidth / active only
    // (third_party/wdsp/src/nbp.c:362, RXANBPAddNotch).
    struct NotchMarker {
        int    id{-1};
        double freqMhz{0.0};
        double widthHz{200.0};
        bool   active{true};
    };

    // From AetherSDR src/gui/SpectrumWidget.cpp:13436-13440 [@c6481cbf].
    // markOverlayDirty(), not the bare update() the spot push uses: notch
    // chrome is cached in the GPU static-overlay texture, so a dragged
    // marker would otherwise not move on the shipping path.
    void setNotchMarkers(const QVector<NotchMarker>& markers);

    // From AetherSDR src/gui/SpectrumWidget.cpp:13497-13501 [@c6481cbf].
    // Master TNF flag.  Repaints every marker in the TNF-off colour rather
    // than hiding it (Thetis display.cs:8704-8707 [v2.10.3.15]).
    void setNotchGlobalEnabled(bool on);

    // WDSP's minimum notch width for the channel feeding this pan
    // (third_party/wdsp/src/nbp.c:594, RXANBPGetMinNotchWidth).  Pushed
    // rather than pulled because it varies with nc and sample rate; Thetis
    // caches it the same way (display.cs:1082 [v2.10.3.15]) and refreshes
    // it on filter-size change (console.cs:39052-39054 [v2.10.3.15],
    // UpdateMinimumNotchWidthRX).  Consumed by the visual-notch dent.
    void setNotchMinWidthHz(double hz);

    // Test seams, following the spotMarkersForTest convention above.
    const QVector<NotchMarker>& notchMarkersForTest() const { return m_notchMarkers; }
    bool   notchGlobalEnabledForTest() const { return m_notchGlobalEnabled; }
    double notchMinWidthHzForTest()    const { return m_notchMinWidthHz; }

    // Overlay-cache seam.  Returns false on a CPU-only build, where there
    // is no cached texture to invalidate.
    bool overlayStaticDirtyForTest() const {
#ifdef NEREUS_GPU_SPECTRUM
        return m_overlayStaticDirty;
#else
        return false;
#endif
    }
    void clearOverlayStaticDirtyForTest() {
#ifdef NEREUS_GPU_SPECTRUM
        m_overlayStaticDirty = false;
#endif
    }
```

In `src/gui/SpectrumWidget.h`, insert after line 1726
(`QHash<QString, bool> m_spotSourceVisible;`):

```cpp

    // ---- TNF / notch overlay state (design §8.1) ----
    // Main-thread only.  Both the paint path and the interaction layer run
    // there, so plain members rather than atomics; NotchModel is the
    // authoritative store and this is a render-side mirror of it.
    // Defaults mirror AetherSDR src/gui/SpectrumWidget.h:1608-1609 and
    // :1648 [@c6481cbf].
    QVector<NotchMarker> m_notchMarkers;
    bool   m_notchGlobalEnabled{true};
    // 100 Hz on this tree: nc = 4096 at a 48 kHz dsp rate through the
    // wintype-0 arm of min_notch_width (third_party/wdsp/src/nbp.c:88,
    // 1600.0 / (nc / 256) * (rate / 48000)).  Overwritten by
    // setNotchMinWidthHz once a channel is open.
    double m_notchMinWidthHz{100.0};
    // Written by the interaction layer (design §7.4); drive the Chartreuse
    // highlight and the hover popup respectively.
    int    m_selectedNotchId{-1};
    int    m_hoveredNotchId{-1};
```

In `src/gui/SpectrumWidget.cpp`, insert after line 5146 (the closing brace of
`txAudioToIq`) and before the `drawTxFilterOverlay()` banner at 5148:

```cpp

// ---------------------------------------------------------------------------
// TNF / notch overlay: setters, colour resolution, render (design §8.1, §8.2).
//
// Geometry is AetherSDR's drawTnfMarkers ported unchanged
// (src/gui/SpectrumWidget.cpp:13503-13554 [@c6481cbf]).  The only two
// divergences are the ones the missing depth axis forces: the hatch spacing
// is fixed instead of depth-derived (upstream :13535) and the handle height
// is fixed instead of 8 + depthDb * 2 (upstream :13545).
//
// Colours are Thetis's, not AetherSDR's: upstream encodes permanent versus
// temporary in green/yellow and we have no permanence, while Thetis encodes
// exactly the four states we do have (display.cs:386-390 [v2.10.3.15]).
// ---------------------------------------------------------------------------

// From Thetis display.cs:389 [v2.10.3.15] ,  notch_active_colour = Color.Yellow.
static constexpr QRgb kNotchActiveColour = qRgb(0xFF, 0xFF, 0x00);
// From Thetis display.cs:390 [v2.10.3.15] ,  notch_inactive_colour = Color.Gray.
// System.Drawing.Color.Gray is #808080 while Qt::gray is #A0A0A4, so the
// literal is spelled out rather than reaching for the Qt global colour.
static constexpr QRgb kNotchInactiveColour = qRgb(0x80, 0x80, 0x80);
// From Thetis display.cs:387 [v2.10.3.15] ,  notch_tnf_off_colour = Color.Olive.
static constexpr QRgb kNotchTnfOffColour = qRgb(0x80, 0x80, 0x00);
// From Thetis display.cs:386 [v2.10.3.15] ,  notch_highlight_color = Color.Chartreuse.
static constexpr QRgb kNotchHighlightColour = qRgb(0x7F, 0xFF, 0x00);

// From Thetis display.cs:400-408 [v2.10.3.15] ,  every notch fill brush is
// changeAlpha(colour, 92); changeAlpha is display.cs:2939-2942.
static constexpr int kNotchFillAlpha = 92;

// Fixed, replacing AetherSDR's depth-derived
// (depthDb <= 1) ? 12 : (depthDb == 2 ? 8 : 5) at
// src/gui/SpectrumWidget.cpp:13535 [@c6481cbf].
static constexpr int kNotchHatchSpacingPx = 8;

// Fixed, replacing AetherSDR's 8 + depthDb * 2 at
// src/gui/SpectrumWidget.cpp:13545 [@c6481cbf].
static constexpr int kNotchHandleHeightPx = 10;

// From AetherSDR src/gui/SpectrumWidget.cpp:13547-13548 [@c6481cbf] , 
// tri << QPoint(cx - 5, ...) << QPoint(cx + 5, ...).
static constexpr int kNotchHandleHalfWidthPx = 5;

// From AetherSDR src/gui/SpectrumWidget.cpp:13523 [@c6481cbf] , 
// std::max(2, ...): a sub-2-pixel notch stays grabbable.
static constexpr int kNotchMinHalfWidthPx = 2;

// From AetherSDR src/gui/SpectrumWidget.cpp:13551 [@c6481cbf] ,  the grab
// handle dims with the master flag as well as changing colour.
static constexpr int kNotchHandleAlphaOn  = 200;
static constexpr int kNotchHandleAlphaOff = 80;

// From AetherSDR src/gui/SpectrumWidget.cpp:13436-13440 [@c6481cbf]
void SpectrumWidget::setNotchMarkers(const QVector<NotchMarker>& markers)
{
    m_notchMarkers = markers;
    markOverlayDirty();
}

// From AetherSDR src/gui/SpectrumWidget.cpp:13497-13501 [@c6481cbf]
void SpectrumWidget::setNotchGlobalEnabled(bool on)
{
    m_notchGlobalEnabled = on;
    markOverlayDirty();
}

void SpectrumWidget::setNotchMinWidthHz(double hz)
{
    m_notchMinWidthHz = hz;
    markOverlayDirty();
}
```

- [ ] **Step 5: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp tests/tst_notch_hit_test.cpp tests/CMakeLists.txt
git commit -m "feat(spectrum): notch marker push API on SpectrumWidget"
```

---

#### Cycle 2: marker geometry

- [ ] **Step 1: Write the failing test**

Append these slots to `TestNotchHitTest` in `tests/tst_notch_hit_test.cpp`,
after `every_notch_mutator_invalidates_the_static_overlay()`:

```cpp
    // ── §8.2 render geometry ─────────────────────────────────────────────
    void empty_marker_list_paints_nothing()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({});

        const QImage img = renderNotches(sw);
        for (int x = 0; x < kPanW; x += 17) {
            QCOMPARE(img.pixelColor(x, kSpecH / 2), QColor(Qt::black));
        }
    }

    // AetherSDR src/gui/SpectrumWidget.cpp:13522-13525, :13538-13542
    // [@c6481cbf]: halfW comes from the UPPER edge only and is mirrored,
    // so left and right are symmetric about cx by construction.
    void edge_lines_land_on_both_notch_boundaries()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0)});

        const int cx    = expectX(kCentreHz);
        const int halfW = std::max(2, expectX(kCentreHz + 100.0) - cx);
        const int left  = cx - halfW;
        const int right = cx + halfW;
        QVERIFY2(halfW > kNotchHandleHalfWidthPxForTest,
                 "fixture notch must be wider than the grab handle");

        const QImage img = renderNotches(sw);
        const QColor yellow = QColor::fromRgb(qRgb(0xFF, 0xFF, 0x00));

        QCOMPARE(img.pixelColor(left,  kSpecH / 2), yellow);
        QCOMPARE(img.pixelColor(right, kSpecH / 2), yellow);
        QCOMPARE(img.pixelColor(left  - 2, kSpecH / 2), QColor(Qt::black));
        QCOMPARE(img.pixelColor(right + 2, kSpecH / 2), QColor(Qt::black));
    }

    // AetherSDR :13534 ,  fillRect spans specRect.height(), top to bottom.
    void fill_spans_the_full_spectrum_height()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0)});

        const int cx = expectX(kCentreHz);
        const QImage img = renderNotches(sw);

        QVERIFY(img.pixelColor(cx, kSpecH - 1) != QColor(Qt::black));
        QVERIFY(img.pixelColor(cx, kSpecH / 2) != QColor(Qt::black));
        QVERIFY(img.pixelColor(cx, 30)         != QColor(Qt::black));
    }

    // AetherSDR :13546-13549 ,  the handle is +/-5 px wide regardless of how
    // narrow the notch is, and it lives at the TOP of the spectrum only.
    void grab_handle_is_wider_than_a_narrow_notch_and_only_at_the_top()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        // 20 Hz across 10 Hz/px is 1 px of half-width, which the
        // std::max(2, ...) floor at AetherSDR :13523 lifts to 2.
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 20.0)});

        const int cx = expectX(kCentreHz);
        const QImage img = renderNotches(sw);

        // Body: exactly 2 px each side of centre.
        QVERIFY(img.pixelColor(cx - 2, kSpecH / 2) != QColor(Qt::black));
        QVERIFY(img.pixelColor(cx + 2, kSpecH / 2) != QColor(Qt::black));
        QCOMPARE(img.pixelColor(cx - 4, kSpecH / 2), QColor(Qt::black));

        // Handle: painted 4 px out at the top, gone 7 px out, and gone
        // again 40 px down.
        QVERIFY(img.pixelColor(cx - 4, 1)  != QColor(Qt::black));
        QVERIFY(img.pixelColor(cx + 4, 1)  != QColor(Qt::black));
        QCOMPARE(img.pixelColor(cx - 7, 1),  QColor(Qt::black));
        QCOMPARE(img.pixelColor(cx - 4, 40), QColor(Qt::black));
    }

    // AetherSDR :13527-13528 ,  "Skip if fully off-screen".
    void off_screen_markers_are_skipped()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz - 100'000.0, 200.0),
                            makeNotch(2, kCentreHz + 100'000.0, 200.0)});

        const QImage img = renderNotches(sw);
        for (int x = 0; x < kPanW; x += 13) {
            QCOMPARE(img.pixelColor(x, kSpecH / 2), QColor(Qt::black));
        }
    }
```

Add the fixture constant to the anonymous namespace in the same file, after
`constexpr int kSpecH = 200;`:

```cpp
// Mirrors kNotchHandleHalfWidthPx in SpectrumWidget.cpp (AetherSDR
// src/gui/SpectrumWidget.cpp:13547-13548 [@c6481cbf]); the implementation
// constant is file-local there, so the fixture restates it.
constexpr int kNotchHandleHalfWidthPxForTest = 5;
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'drawNotchMarkersForTest' in 'NereusSDR::SpectrumWidget'`.

- [ ] **Step 3: Implement drawNotchMarkers**

In `src/gui/SpectrumWidget.h`, add the forwarder to the public seam block
added in Cycle 1, immediately after `notchMinWidthHzForTest()`:

```cpp
    void drawNotchMarkersForTest(QPainter& p, const QRect& specRect) {
        drawNotchMarkers(p, specRect);
    }
```

In `src/gui/SpectrumWidget.h`, insert after line 1321
(`void drawSpotMarkers(QPainter& p, const QRect& specRect);`) and before the
`showSpotClusterPopup` declaration:

```cpp

    // ---- TNF / notch overlay (design §8.2) ----
    // Geometry ported unchanged from AetherSDR drawTnfMarkers
    // (src/gui/SpectrumWidget.cpp:13503-13554 [@c6481cbf]): translucent
    // fill over the full spectrum height, diagonal hatch clipped to the
    // notch rect, 1 px edge lines at both boundaries, downward triangle
    // grab handle at the top.  Colours come from Thetis instead
    // (display.cs:386-390, 8691-8722 [v2.10.3.15]).
    void drawNotchMarkers(QPainter& p, const QRect& specRect);
```

In `src/gui/SpectrumWidget.cpp`, append after `setNotchMinWidthHz` from
Cycle 1:

```cpp

// From AetherSDR src/gui/SpectrumWidget.cpp:13503-13554 [@c6481cbf]
void SpectrumWidget::drawNotchMarkers(QPainter& p, const QRect& specRect)
{
    if (m_notchMarkers.isEmpty()) {
        return;
    }

    // From AetherSDR src/gui/SpectrumWidget.cpp:13507-13519 [@c6481cbf] , 
    // the drawDepthHatch lambda, renamed because the depth argument is gone.
    const auto drawHatch = [&](const QRect& rect, const QColor& colour,
                               int left, int right, int spacing) {
        if (rect.isEmpty()) {
            return;
        }
        p.save();
        p.setClipRect(rect);
        p.setPen(QPen(colour, 1));
        const int height = rect.height();
        for (int x = left - height; x < right; x += spacing) {
            p.drawLine(x, rect.bottom(), x + height, rect.top());
        }
        p.restore();
    };

    for (const NotchMarker& n : m_notchMarkers) {
        // NereusSDR coordinate mapping: hzToX(double hz, QRect) takes Hz.
        // AetherSDR upstream uses mhzToX(freqMhz) at :13522-13523; multiply
        // by 1e6, exactly as drawSpotMarkers already does.
        const double centreHz = n.freqMhz * 1.0e6;
        const int cx    = hzToX(centreHz, specRect);
        const int halfW = std::max(kNotchMinHalfWidthPx,
                                   hzToX(centreHz + n.widthHz / 2.0, specRect) - cx);
        const int left  = cx - halfW;
        const int right = cx + halfW;

        // From AetherSDR src/gui/SpectrumWidget.cpp:13527-13528 [@c6481cbf]
        // Skip if fully off-screen
        if (right < 0 || left > width()) {
            continue;
        }

        const QColor base = notchColor(n);
        QColor fill(base);
        fill.setAlpha(kNotchFillAlpha);

        const QRect notchRect(left, specRect.top(), right - left, specRect.height());
        p.fillRect(notchRect, fill);
        drawHatch(notchRect, base, left, right, kNotchHatchSpacingPx);

        // From AetherSDR src/gui/SpectrumWidget.cpp:13538-13542 [@c6481cbf]
        // Edge lines
        p.setPen(QPen(base, 1, Qt::SolidLine));
        p.drawLine(left,  specRect.top(), left,  specRect.bottom());
        p.drawLine(right, specRect.top(), right, specRect.bottom());

        // From AetherSDR src/gui/SpectrumWidget.cpp:13544-13552 [@c6481cbf]
        // Center triangle (grab handle) at top of spectrum
        QPolygon tri;
        tri << QPoint(cx - kNotchHandleHalfWidthPx, specRect.top())
            << QPoint(cx + kNotchHandleHalfWidthPx, specRect.top())
            << QPoint(cx, specRect.top() + kNotchHandleHeightPx);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(base.red(), base.green(), base.blue(),
                          m_notchGlobalEnabled ? kNotchHandleAlphaOn
                                               : kNotchHandleAlphaOff));
        p.drawPolygon(tri);
    }

    p.setBrush(Qt::NoBrush);
    p.setPen(Qt::NoPen);
}
```

Also add a provisional `notchColor` so this cycle links; Cycle 3 replaces its
body with the full Thetis state machine. Append immediately above
`drawNotchMarkers`:

```cpp
QColor SpectrumWidget::notchColor(const NotchMarker& n) const
{
    Q_UNUSED(n);
    return QColor::fromRgb(kNotchActiveColour);
}
```

and declare it in `src/gui/SpectrumWidget.h` beside `drawNotchMarkers`:

```cpp
    QColor notchColor(const NotchMarker& n) const;
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp tests/tst_notch_hit_test.cpp
git commit -m "feat(spectrum): render TNF notch markers on the panadapter"
```

---

#### Cycle 3: the four Thetis marker colours

- [ ] **Step 1: Write the failing test**

Append to `TestNotchHitTest` in `tests/tst_notch_hit_test.cpp`:

```cpp
    // ── §8.2 colours, from Thetis display.cs:8691-8722 [v2.10.3.15] ──────
    void active_notch_is_yellow()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0, /*active*/ true)});

        const int cx   = expectX(kCentreHz);
        const int left = cx - std::max(2, expectX(kCentreHz + 100.0) - cx);
        QCOMPARE(renderNotches(sw).pixelColor(left, kSpecH / 2),
                 QColor::fromRgb(qRgb(0xFF, 0xFF, 0x00)));
    }

    void bypassed_notch_is_gray()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0, /*active*/ false)});

        const int cx   = expectX(kCentreHz);
        const int left = cx - std::max(2, expectX(kCentreHz + 100.0) - cx);
        QCOMPARE(renderNotches(sw).pixelColor(left, kSpecH / 2),
                 QColor::fromRgb(qRgb(0x80, 0x80, 0x80)));
    }

    void master_tnf_off_is_olive_even_for_an_active_notch()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(1, kCentreHz, 200.0, /*active*/ true)});
        sw.setNotchGlobalEnabled(false);

        const int cx   = expectX(kCentreHz);
        const int left = cx - std::max(2, expectX(kCentreHz + 100.0) - cx);
        QCOMPARE(renderNotches(sw).pixelColor(left, kSpecH / 2),
                 QColor::fromRgb(qRgb(0x80, 0x80, 0x00)));
    }

    // display.cs:8710-8722 "overide if highlighed" ,  the highlight wins over
    // every other state, master-off included.
    void selected_notch_is_chartreuse_and_overrides_master_off()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(4, kCentreHz, 200.0, /*active*/ false)});
        sw.setNotchGlobalEnabled(false);
        sw.setSelectedNotchIdForTest(4);

        const int cx   = expectX(kCentreHz);
        const int left = cx - std::max(2, expectX(kCentreHz + 100.0) - cx);
        QCOMPARE(renderNotches(sw).pixelColor(left, kSpecH / 2),
                 QColor::fromRgb(qRgb(0x7F, 0xFF, 0x00)));
    }

    void hovered_notch_is_chartreuse()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(9, kCentreHz, 200.0, /*active*/ true)});
        sw.setHoveredNotchIdForTest(9);

        const int cx   = expectX(kCentreHz);
        const int left = cx - std::max(2, expectX(kCentreHz + 100.0) - cx);
        QCOMPARE(renderNotches(sw).pixelColor(left, kSpecH / 2),
                 QColor::fromRgb(qRgb(0x7F, 0xFF, 0x00)));
    }

    // m_selectedNotchId / m_hoveredNotchId default to -1, and so does a
    // default-constructed NotchMarker.  They must not match each other.
    void unset_selection_does_not_highlight_an_unidentified_marker()
    {
        SpectrumWidget sw;
        sw.resize(kPanW, kPanH);
        sw.setFrequencyRange(kCentreHz, kBandwidthHz);
        sw.setNotchMarkers({makeNotch(-1, kCentreHz, 200.0, /*active*/ true)});

        const int cx   = expectX(kCentreHz);
        const int left = cx - std::max(2, expectX(kCentreHz + 100.0) - cx);
        QCOMPARE(renderNotches(sw).pixelColor(left, kSpecH / 2),
                 QColor::fromRgb(qRgb(0xFF, 0xFF, 0x00)));
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'setSelectedNotchIdForTest' in 'NereusSDR::SpectrumWidget'`.

- [ ] **Step 3: Implement the colour state machine**

In `src/gui/SpectrumWidget.h`, add to the public seam block after
`drawNotchMarkersForTest`:

```cpp
    // Selection and hover are written by the interaction layer (design
    // §7.4); until it lands these give the render tests a writer for the
    // Chartreuse highlight branch.  Same shape as
    // PanadapterStack::rootSplitterSetSizesForTest.
    void setSelectedNotchIdForTest(int id) { m_selectedNotchId = id; }
    void setHoveredNotchIdForTest(int id)  { m_hoveredNotchId = id; }
```

In `src/gui/SpectrumWidget.cpp`, replace the provisional `notchColor` body
from Cycle 2 with:

```cpp
// From Thetis display.cs:8691-8722 [v2.10.3.15] ,  handleNotches' brush
// selection, flattened to a colour because our pen and fill derive from one
// base (upstream keeps a Pen and a Brush per state and they never disagree).
QColor SpectrumWidget::notchColor(const NotchMarker& n) const
{
    // From Thetis display.cs:8710-8722 [v2.10.3.15]:
    //   //overide if highlighed  [original inline comment from display.cs:8710]
    // The highlight is applied AFTER the master-off branch upstream, so it
    // wins over every other state.  Guarded on a real id because both
    // selection members and a default-constructed marker share -1.
    if (n.id >= 0 && (n.id == m_selectedNotchId || n.id == m_hoveredNotchId)) {
        return QColor::fromRgb(kNotchHighlightColour);
    }

    // From Thetis display.cs:8704-8707 [v2.10.3.15] ,  master TNF off repaints
    // every marker olive rather than hiding it.
    if (!m_notchGlobalEnabled) {
        return QColor::fromRgb(kNotchTnfOffColour);
    }

    // From Thetis display.cs:8693-8702 [v2.10.3.15]
    return n.active ? QColor::fromRgb(kNotchActiveColour)
                    : QColor::fromRgb(kNotchInactiveColour);
}
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp tests/tst_notch_hit_test.cpp
git commit -m "feat(spectrum): Thetis notch marker colours for the four TNF states"
```

---

#### Cycle 4: interaction signals and per-pan pixel mapping

- [ ] **Step 1: Write the failing test**

Append to `TestNotchHitTest` in `tests/tst_notch_hit_test.cpp`:

```cpp
    // ── §8.1 inbound signals ─────────────────────────────────────────────
    // Emitters land with the interaction layer (design §12 step 7); this is
    // the signature gate MainWindow::wirePanNotchHandlers connects against.
    void notch_interaction_signals_exist_with_expected_signatures()
    {
        SpectrumWidget sw;
        QSignalSpy create(&sw, &SpectrumWidget::notchCreateRequested);
        QSignalSpy move(&sw,   &SpectrumWidget::notchMoveRequested);
        QSignalSpy width(&sw,  &SpectrumWidget::notchWidthRequested);
        QSignalSpy active(&sw, &SpectrumWidget::notchActiveRequested);
        QSignalSpy remove(&sw, &SpectrumWidget::notchRemoveRequested);

        QVERIFY(create.isValid());
        QVERIFY(move.isValid());
        QVERIFY(width.isValid());
        QVERIFY(active.isValid());
        QVERIFY(remove.isValid());
    }

    // §8.1: under D1 the notch list is global, so EVERY pan gets the same
    // push and each converts it into its own pixel space.  Deliberately NOT
    // the spot overlay's activeSpectrumWidget()-only shape
    // (MainWindow.cpp:2276), which leaves secondary pans blank.
    void two_pans_map_the_same_notch_to_their_own_pixel_space()
    {
        PanadapterStack stack;
        stack.applyLayout(QStringLiteral("2v"),
                          {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        QCOMPARE(stack.count(), 2);

        SpectrumWidget* a = stack.spectrum(QStringLiteral("pan-0"));
        SpectrumWidget* b = stack.spectrum(QStringLiteral("pan-1"));
        QVERIFY(a != nullptr);
        QVERIFY(b != nullptr);

        a->resize(kPanW, kPanH);
        b->resize(kPanW, kPanH);
        a->setFrequencyRange(kCentreHz, kBandwidthHz);
        // pan-1 is parked 2 kHz low, so the same absolute-RF notch has to
        // land 200 px to the right of where it lands on pan-0.
        b->setFrequencyRange(kCentreHz - 2'000.0, kBandwidthHz);

        const QVector<SpectrumWidget::NotchMarker> markers{
            makeNotch(1, kCentreHz, 200.0)};
        a->setNotchMarkers(markers);
        b->setNotchMarkers(markers);

        QCOMPARE(a->notchMarkersForTest().size(), 1);
        QCOMPARE(b->notchMarkersForTest().size(), 1);

        const QImage imgA = renderNotches(*a);
        const QImage imgB = renderNotches(*b);

        QVERIFY(imgA.pixelColor(400, kSpecH / 2) != QColor(Qt::black));
        QCOMPARE(imgA.pixelColor(600, kSpecH / 2), QColor(Qt::black));

        QVERIFY(imgB.pixelColor(600, kSpecH / 2) != QColor(Qt::black));
        QCOMPARE(imgB.pixelColor(400, kSpecH / 2), QColor(Qt::black));
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'notchCreateRequested' in 'NereusSDR::SpectrumWidget'`.

- [ ] **Step 3: Declare the signals and hook both paint paths**

In `src/gui/SpectrumWidget.h`, insert after line 1155
(`void spotHoverIndexChanged(int spotIndex);`) and before the
`// Emitted when user drags a filter edge` comment at 1157:

```cpp

    // ── TNF / notch overlay (design §8.1) ─────────────────────────────────
    // Wired per pan by MainWindow::wirePanNotchHandlers, so a marker drawn
    // on a pan acts through that pan's own frequency mapping while the
    // NotchModel the handlers mutate stays global (design D1).  `narrow` is
    // the Shift-held 100 Hz add (Thetis console.cs:40269 [v2.10.3.15]);
    // every frequency is absolute RF in Hz.  Emitters land with the
    // interaction layer (design §7.1 through §7.4).
    void notchCreateRequested(double freqHz, bool narrow);
    void notchMoveRequested(int id, double newFreqHz);
    void notchWidthRequested(int id, double widthHz);
    void notchActiveRequested(int id, bool active);
    void notchRemoveRequested(int id);
```

In `src/gui/SpectrumWidget.cpp`, insert before line 3038
(`// Phase 3J-2 Task E1: spot overlay between spectrum/waterfall and the`):

```cpp
    // TNF notch overlay immediately before the spots, matching upstream's
    // paint order: AetherSDR src/gui/SpectrumWidget.cpp:12903-12904
    // [@c6481cbf] draws drawTnfMarkers then drawSpotMarkers.  No visibility
    // gate: an empty marker list IS the off state, and the master TNF flag
    // recolours the markers rather than hiding them (Thetis
    // display.cs:8704-8707 [v2.10.3.15]).
    drawNotchMarkers(p, specRect);
```

In `src/gui/SpectrumWidget.cpp`, insert before line 7139
(`// Phase 3J-2 Task E1: spot overlay before VFO marker so labels`):

```cpp
            // TNF notch overlay, GPU static-overlay path.  Same relative
            // ordering as the CPU paintEvent above and as AetherSDR
            // src/gui/SpectrumWidget.cpp:12013-12016 [@c6481cbf], where
            // drawTnfMarkers likewise precedes drawSpotMarkers in the
            // frequency-plane painter.  Missing THIS call site while
            // having the CPU one is a silent GPU-only regression, since
            // NEREUS_GPU_SPECTRUM is the shipping path.
            drawNotchMarkers(p, specRect);
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp tests/tst_notch_hit_test.cpp
git commit -m "feat(spectrum): declare TNF interaction signals and paint notches on both paths"
```

---

#### Cycle 5: MainWindow fan-out to every pan

- [ ] **Step 1: Write the failing test**

Append to `TestNotchHitTest` in `tests/tst_notch_hit_test.cpp`:

```cpp
    // ── §8.1 fan-out ─────────────────────────────────────────────────────
    // MainWindow is deliberately never constructed in this suite (see the
    // banner of tests/tst_mainwindow_tools_spot_hub.cpp), so this pins the
    // half of MainWindow::refreshPanNotchMarkers that can drift silently:
    // the NotchModel -> NotchMarker conversion, and the fact that the same
    // vector reaches every pan.  The m_panStack loop itself is bench-covered
    // (design §11.2).
    //
    // `auto` in the loop is deliberate: it compiles whether Notch is nested
    // in NotchModel or lives at NereusSDR namespace scope.
    void notch_model_entries_convert_and_reach_every_pan()
    {
        NotchModel model;
        const int idA = model.addNotch(14'250'000.0, 200.0);
        const int idB = model.addNotch(14'251'000.0, 100.0);
        QVERIFY(idA >= 0);
        QVERIFY(idB >= 0);
        QVERIFY(model.setActive(idB, false));

        QVector<SpectrumWidget::NotchMarker> markers;
        markers.reserve(model.notches().size());
        for (const auto& n : model.notches()) {
            SpectrumWidget::NotchMarker m;
            m.id      = n.id;
            m.freqMhz = n.centerHz / 1.0e6;
            m.widthHz = n.widthHz;
            m.active  = n.active;
            markers.append(m);
        }
        QCOMPARE(markers.size(), 2);

        PanadapterStack stack;
        stack.applyLayout(QStringLiteral("2h"),
                          {QStringLiteral("pan-0"), QStringLiteral("pan-1")});

        model.setGlobalEnabled(false);
        for (const QString& panId : {QStringLiteral("pan-0"),
                                     QStringLiteral("pan-1")}) {
            SpectrumWidget* sw = stack.spectrum(panId);
            QVERIFY(sw != nullptr);
            sw->setNotchMarkers(markers);
            sw->setNotchGlobalEnabled(model.globalEnabled());
        }

        for (const QString& panId : {QStringLiteral("pan-0"),
                                     QStringLiteral("pan-1")}) {
            SpectrumWidget* sw = stack.spectrum(panId);
            QCOMPARE(sw->notchMarkersForTest().size(), 2);
            QCOMPARE(sw->notchMarkersForTest().at(0).id, idA);
            QCOMPARE(sw->notchMarkersForTest().at(0).freqMhz, 14.25);
            QCOMPARE(sw->notchMarkersForTest().at(0).widthHz, 200.0);
            QCOMPARE(sw->notchMarkersForTest().at(0).active, true);
            QCOMPARE(sw->notchMarkersForTest().at(1).id, idB);
            QCOMPARE(sw->notchMarkersForTest().at(1).widthHz, 100.0);
            QCOMPARE(sw->notchMarkersForTest().at(1).active, false);
            QCOMPARE(sw->notchGlobalEnabledForTest(), false);
        }
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`fatal error: 'models/NotchModel.h' file not found` if Task 3 has not landed,
otherwise PASS on the conversion and FAIL to link once the fan-out declarations
below are referenced. Land Task 3 and Task 4 first.

- [ ] **Step 3: Implement the MainWindow fan-out**

In `src/gui/MainWindow.cpp`, insert after line 308
(`#include "models/SpotModel.h"`):

```cpp
#include "models/NotchModel.h"
```

In `src/gui/MainWindow.h`, insert after line 234
(`void refreshMeterPollerSlices();`), inside the existing `private slots:`
block:

```cpp

    /// TNF: push the global notch list at EVERY pan (design §8.1).
    ///
    /// Under D1 the notch list is global, so each pan gets the same vector
    /// and converts it into its own pixel space.  Deliberately not the spot
    /// overlay's activeSpectrumWidget()-only shape (MainWindow.cpp:2276),
    /// which leaves every secondary pan blank.
    void refreshPanNotchMarkers();

    /// TNF: connect the five per-pan notch interaction signals on EVERY pan.
    ///
    /// Armed from PanadapterStack::countChanged, the same hook
    /// wirePanBadgeHandlers uses, and for the same reason: a pan created by
    /// a layout switch would otherwise never be wired.  Not folded into
    /// wireSpectrumForPan, which skips pan-0 by design
    /// (MainWindow.cpp:1731-1736).
    ///
    /// The handlers below are SLOTS, not lambdas, so Qt::UniqueConnection is
    /// actually honoured on the re-arm.
    void wirePanNotchHandlers();

    /// TNF inbound handlers.  Each mutates the single global NotchModel;
    /// the frequencies arrive already resolved in the emitting pan's own
    /// frequency mapping, so nothing here consults an active pan.
    void onNotchCreateRequested(double freqHz, bool narrow);
    void onNotchMoveRequested(int id, double newFreqHz);
    void onNotchWidthRequested(int id, double widthHz);
    void onNotchActiveRequested(int id, bool active);
    void onNotchRemoveRequested(int id);
```

In `src/gui/MainWindow.cpp`, insert after line 1519 (the closing brace of
`wirePanNotchStatusOverlayTriggers`'s sibling `wirePanStatusOverlayTriggers`)
and before the `// Phase 3F: badge-click fan-out.` comment at 1521:

```cpp

// TNF: notch-marker fan-out.  Fourth sibling of refreshPanWideBadges,
// refreshPanStatusOverlays and wirePanBadgeHandlers above, on the same hook
// and the same shape: ask the model once, push the answer at every pan.
//
// Every pan is refreshed on every pass because the notch list is global
// (design D1): a notch added from one pan is a notch on all of them.
void MainWindow::refreshPanNotchMarkers()
{
    if (!m_panStack || !m_radioModel) { return; }
    NotchModel* notches = m_radioModel->notchModel();
    if (!notches) { return; }

    QVector<SpectrumWidget::NotchMarker> markers;
    markers.reserve(notches->notches().size());
    // `auto` here: the element type is obvious from notches() and this stays
    // correct whichever scope NotchModel declares Notch in.
    for (const auto& n : notches->notches()) {
        SpectrumWidget::NotchMarker m;
        m.id      = n.id;
        m.freqMhz = n.centerHz / 1.0e6;
        m.widthHz = n.widthHz;
        m.active  = n.active;
        markers.append(m);
    }

    const bool globalOn = notches->globalEnabled();
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        SpectrumWidget* sw = applet->spectrumWidget();
        if (!sw) { continue; }
        sw->setNotchMarkers(markers);
        sw->setNotchGlobalEnabled(globalOn);
    }
}

void MainWindow::wirePanNotchHandlers()
{
    if (!m_panStack) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        SpectrumWidget* sw = applet->spectrumWidget();
        if (!sw) { continue; }
        connect(sw, &SpectrumWidget::notchCreateRequested,
                this, &MainWindow::onNotchCreateRequested,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchMoveRequested,
                this, &MainWindow::onNotchMoveRequested,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchWidthRequested,
                this, &MainWindow::onNotchWidthRequested,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchActiveRequested,
                this, &MainWindow::onNotchActiveRequested,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchRemoveRequested,
                this, &MainWindow::onNotchRemoveRequested,
                Qt::UniqueConnection);
    }
}

void MainWindow::onNotchCreateRequested(double freqHz, bool narrow)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    // Narrow is the Shift-held add.  Widths live on NotchModel because they
    // are Thetis constants (console.cs:40268-40269 [v2.10.3.15]).
    m_radioModel->notchModel()->addNotch(
        freqHz, narrow ? NotchModel::kNotchNarrowWidthHz
                       : NotchModel::kNotchDefaultWidthHz);
}

void MainWindow::onNotchMoveRequested(int id, double newFreqHz)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->setCenter(id, newFreqHz);
}

void MainWindow::onNotchWidthRequested(int id, double widthHz)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->setWidth(id, widthHz);
}

void MainWindow::onNotchActiveRequested(int id, bool active)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->setActive(id, active);
}

void MainWindow::onNotchRemoveRequested(int id)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->removeNotch(id);
}
```

In `src/gui/MainWindow.cpp`, extend the countChanged hook at lines 2168-2173 to:

```cpp
    connect(m_panStack, &PanadapterStack::countChanged, this, [this](int) {
        wirePanStatusOverlayTriggers();
        wirePanBadgeHandlers();
        wirePanNotchHandlers();
        ensureOverlayPanels();
        refreshPanStatusOverlays();
        refreshPanNotchMarkers();
    });

    // TNF: the notch list is global, so one connect per NotchModel signal
    // repaints every pan.  refreshPanNotchMarkers takes no arguments; Qt
    // drops the extra ones from notchAdded / notchChanged / notchRemoved /
    // globalEnabledChanged.
    if (NotchModel* notches = m_radioModel->notchModel()) {
        connect(notches, &NotchModel::notchAdded,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::notchChanged,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::notchRemoved,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::notchesReset,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::globalEnabledChanged,
                this, &MainWindow::refreshPanNotchMarkers);
    }
```

In `src/gui/MainWindow.cpp`, extend the initial arm at lines 2182-2183 to:

```cpp
    wirePanStatusOverlayTriggers();
    wirePanBadgeHandlers();
    wirePanNotchHandlers();
    // Seed pan-0 with whatever NotchModel::restoreFromSettings() already
    // loaded in the RadioModel constructor.  The layout restore below fires
    // countChanged and re-runs both of these for the pans it creates.
    refreshPanNotchMarkers();
```

- [ ] **Step 4: Run the test and the app build, watch them pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure && cmake --build build`

Expected: PASS, and the `NereusSDR` target links.

- [ ] **Step 5: Commit**

```bash
git add src/gui/MainWindow.h src/gui/MainWindow.cpp tests/tst_notch_hit_test.cpp
git commit -m "feat(gui): push notch markers to every panadapter"
```

---

### Task 7: SpectrumWidget interaction: add, drag, edge-drag, wheel, hover, menus

**Files:**
- Create: `tests/tst_notch_hit_test.cpp`
- Modify: `tests/CMakeLists.txt:4331` (append a `nereus_add_test` block after the `tst_spectrum_overlays` registration)
- Modify: `src/gui/SpectrumWidget.h:176` (add `class QMenu;` to the existing `QT_BEGIN_NAMESPACE` forward-decl block)
- Modify: `src/gui/SpectrumWidget.h:1120-1121` (public test seams, immediately after `spotBgColorForTest()`; after Task 6 this sits at the tail of the notch public block)
- Modify: `src/gui/SpectrumWidget.h:1199-1200` (five interaction signals, immediately before `protected:`)
- Modify: `src/gui/SpectrumWidget.h:1322-1323` (private notch helpers, immediately after `showSpotClusterPopup`)
- Modify: `src/gui/SpectrumWidget.h:1726-1727` (private notch interaction state, immediately after `m_spotSourceVisible`)
- Modify: `src/gui/SpectrumWidget.cpp:5885-5886` (new notch-interaction block between `specHFromHeight` and `mousePressEvent`)
- Modify: `src/gui/SpectrumWidget.cpp:5916-5917` (Ctrl add + notch context menu at the head of the `Qt::RightButton` branch)
- Modify: `src/gui/SpectrumWidget.cpp:6121-6123` (notch press latch, between the freq-scale bar block and the filter-edge block)
- Modify: `src/gui/SpectrumWidget.cpp:6239-6240` (notch drag branch, between the time-scale drag block and `m_draggingDbm`)
- Modify: `src/gui/SpectrumWidget.cpp:6452-6454` (notch hover, between the spot hover block and the dBm-strip cursor chain)
- Modify: `src/gui/SpectrumWidget.cpp:6567-6573` (clear `m_notchGrab` in the release button-clear block)
- Modify: `src/gui/SpectrumWidget.cpp:6585-6594` (`leaveEvent` clears notch hover state)
- Modify: `src/gui/SpectrumWidget.cpp:6596-6598` (notch wheel resize at the head of `wheelEvent`)
- Test: `tests/tst_notch_hit_test.cpp`

**Interfaces:**
- Consumes (from Task 6, `SpectrumWidget` marker rendering, §8.1/§8.2):
  - `struct SpectrumWidget::NotchMarker { int id; double freqMhz; double widthHz; bool active; };`
  - `void SpectrumWidget::setNotchMarkers(const QVector<NotchMarker>& markers);`
  - `void SpectrumWidget::setNotchGlobalEnabled(bool on);`
  - `void SpectrumWidget::setNotchMinWidthHz(double hz);`
  - `QVector<NotchMarker> m_notchMarkers;` (private member)
  - `void SpectrumWidget::drawNotchMarkers(QPainter& p, const QRect& specRect);` (private)
- Consumes (pre-existing, unchanged): `int SpectrumWidget::hzToX(double hz, const QRect& r) const;`, `double SpectrumWidget::xToHz(int x, const QRect& r) const;`, `int SpectrumWidget::effectiveStripW() const;`, `void SpectrumWidget::markOverlayDirty();`, `static int specHFromHeight(int widgetH, float spectrumFrac, int chromeH)` (file-local, `SpectrumWidget.cpp:5876`).
- Produces (Task 8 wires these per pan in `MainWindow`; Task 3's `NotchModel` is the sink):
  - `enum class SpectrumWidget::NotchGrab { None, Centre, LowEdge, HighEdge };`
  - `void SpectrumWidget::notchCreateRequested(double freqHz, bool narrow);` (signal)
  - `void SpectrumWidget::notchMoveRequested(int id, double newFreqHz);` (signal)
  - `void SpectrumWidget::notchWidthRequested(int id, double widthHz);` (signal)
  - `void SpectrumWidget::notchActiveRequested(int id, bool active);` (signal)
  - `void SpectrumWidget::notchRemoveRequested(int id);` (signal)
  - `int SpectrumWidget::notchAtPixelForTest(int x) const;`
  - `NotchGrab SpectrumWidget::notchGrabAtForTest(int id, int x, bool shiftHeld) const;`
  - `int SpectrumWidget::selectedNotchIdForTest() const;`
  - `int SpectrumWidget::hoveredNotchIdForTest() const;`
  - `void SpectrumWidget::buildNotchContextMenuForTest(int id, QMenu& menu);`

**Plan comments (decisions the implementer must not re-litigate):**

1. **`m_selectedNotchId` tracks hover, not "last clicked".** §7.4's inline comment says "last clicked", but the §7 gesture table says "Wheel **over notch**" and the cite it hangs the gate on (`console.cs:31141-31145`) reads `SelectedNotch`, which Thetis assigns from the *hover* hit test on every non-dragging move (`console.cs:49921`). Under a literal "last clicked" reading the wheel would keep resizing a notch after the cursor left it, stealing every scroll on the panadapter, which is the exact failure §7.4 exists to prevent. So: hover writes both ids when not dragging; a left press also writes `m_selectedNotchId` (so a press is self-contained); the press latches `m_notchGrab` and hover stops writing until release. `m_hoveredNotchId` and `m_selectedNotchId` therefore differ only mid-drag, which is the latch §7.3 asks for.
2. **Notch press precedence sits between the freq-scale bar and the filter edges.** Thetis runs its notch block first in `case MouseButtons.Left:` (`console.cs:48981`), ahead of every filter drag. The dBm strip, divider and freq-scale rows are NereusSDR chrome outside the spectrum plot, so they keep their existing precedence and the notch test is guarded on `my < specH`.
3. **Ctrl is `ControlModifier | MetaModifier`.** On macOS Qt swaps Control and Command, so the physical Ctrl key arrives as `Qt::MetaModifier`. `SpectrumWidget.cpp:6631` already accepts both for the zoom wheel; the add gesture matches that precedent so D2's gesture works on a Mac trackpad, which is the whole reason §7.1 picked it.
4. **Hover readout uses `QToolTip`, not a `QLabel` popup.** AetherSDR's `m_tnfHoverPopup` is a styled `QLabel`; the NereusSDR panadapter already routes hover detail through `QToolTip` (spot labels, `SpectrumWidget.cpp:6405`). Same operator-visible content (frequency + width), one popup mechanism instead of two.
5. **Right-click on empty pan is left alone.** §7's table has an "Add notch at X MHz" convenience item from AetherSDR's general-area `QMenu`. NereusSDR's empty-pan right-click opens `SpectrumOverlayMenu`, a custom popup widget rather than a `QMenu`, so that row cannot be added without changing what a shipped gesture does. Not implemented here; see spec_gaps. Ctrl + right-click (the Thetis-authoritative gesture) and the `+TNF` button in Task 8 both cover adding.
6. **`notchMarkerById` is claimed by this task.** If Task 6 already added it for the colour helpers, reuse it and drop the duplicate declaration.

---

- [ ] **Step 1: Write the failing test (hit test)**

Create `tests/tst_notch_hit_test.cpp`:

```cpp
// =================================================================
// tests/tst_notch_hit_test.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.  Cites for the ported
// interaction logic under test live in src/gui/SpectrumWidget.cpp.
//
// TNF design §7 (interaction model) / §11 tst_notch_hit_test.
// Pins the panadapter-side notch interaction:
//   * the pixel-space hit test (Thetis NotchThatSurroundsFrequencyInBW
//     semantics: first-found in list order, 1-pixel pad applied only
//     when the notch is narrower than twice the pad, off-screen reject);
//   * edge-vs-centre grab discrimination (8 px minimum on-screen width
//     before edge zones exist, +/- 4 px edge zone, side-of-centre
//     default, Shift as an explicit resize);
//   * hover selection, drag, wheel resize, Ctrl + right-click add and
//     the notch context menu.
//
// Upstream tag preserved for the hit-test rule:
//   //MW0LGE return first notch found that surrounds a given frequency
//   in the given bandwidth   [original inline comment from radio.cs:4296]
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPoint>
#include <QPointF>
#include <QSignalSpy>
#include <QVector>
#include <QWheelEvent>

#include <cmath>

#include "gui/SpectrumWidget.h"
#include "core/ConnectionState.h"

using namespace NereusSDR;

namespace {

// Deterministic geometry.  1000 px of spectrum across 128 kHz gives
// exactly 128 Hz per pixel, and the centre frequency lands on an exact
// binary fraction of the span, so every hand-computed pixel below is
// exact rather than "within a rounding error".
constexpr double kCentreHz    = 14200000.0;
constexpr double kBandwidthHz = 128000.0;
constexpr double kHzPerPx     = 128.0;       // kBandwidthHz / kWidgetW
constexpr int    kWidgetW     = 1000;
constexpr int    kWidgetH     = 400;
constexpr int    kCentreX     = 500;
// specHFromHeight(400, 0.40f, 28 + 4) == 147 on the GPU layout; any y
// well below that is inside the spectrum plot on both render paths.
constexpr int    kSpecY       = 50;

// Mirrors SpectrumWidget::hzToX (SpectrumWidget.cpp:4025-4030) exactly,
// truncating int cast included, so expected pixels cannot drift from the
// widget's own mapping.
int xForHz(double hz)
{
    const double lowHz = kCentreHz - kBandwidthHz / 2.0;
    return static_cast<int>((hz - lowHz) / kBandwidthHz * kWidgetW);
}

// Mirrors SpectrumWidget::xToHz (SpectrumWidget.cpp:4032-4036).
double hzForX(int x)
{
    const double lowHz = kCentreHz - kBandwidthHz / 2.0;
    return lowHz + (static_cast<double>(x) / kWidgetW) * kBandwidthHz;
}

SpectrumWidget::NotchMarker mk(int id, double centreHz, double widthHz,
                               bool active = true)
{
    SpectrumWidget::NotchMarker m;
    m.id      = id;
    m.freqMhz = centreHz / 1.0e6;
    m.widthHz = widthHz;
    m.active  = active;
    return m;
}

void configure(SpectrumWidget& w)
{
    w.resize(kWidgetW, kWidgetH);
    // dBm strip off -> effectiveStripW() == 0 -> spectrum rect is the
    // full widget width, so xForHz above is the widget's own mapping.
    w.setDbmScaleVisible(false);
    w.setFrequencyRange(kCentreHz, kBandwidthHz);
    // mousePressEvent swallows left clicks while not Connected.
    w.setConnectionState(ConnectionState::Connected);
}

} // namespace

class TestNotchHitTest : public QObject
{
    Q_OBJECT

private slots:

    // ── §7.3 hit test: Thetis notchSurrounding rule ──────────────────────

    void hit_test_returns_id_at_notch_centre()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(7, kCentreHz, 400.0)});

        QCOMPARE(w.notchAtPixelForTest(kCentreX), 7);
    }

    void hit_test_returns_minus_one_off_notch()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(7, kCentreHz, 400.0)});

        // 100 px away is 12800 Hz off centre, far outside a 400 Hz notch.
        QCOMPARE(w.notchAtPixelForTest(kCentreX + 100), -1);
    }

    void hit_test_returns_first_match_in_list_order()
    {
        SpectrumWidget w;
        configure(w);
        // Two notches covering the same pixel. Thetis returns the first
        // one found walking the list, not the nearest centre.
        w.setNotchMarkers({mk(3, kCentreHz, 2000.0),
                           mk(9, kCentreHz + 256.0, 2000.0)});

        QCOMPARE(w.notchAtPixelForTest(kCentreX), 3);
    }

    void hit_test_pads_sub_pixel_notch_by_one_pixel()
    {
        SpectrumWidget w;
        configure(w);
        // 10 Hz wide is 0.08 px: unhittable without the pad. 10 < 2*128,
        // so the pad applies and the reach becomes 5 + 128 = 133 Hz.
        w.setNotchMarkers({mk(4, kCentreHz, 10.0)});

        QCOMPARE(w.notchAtPixelForTest(kCentreX), 4);          // 0 Hz off
        QCOMPARE(w.notchAtPixelForTest(kCentreX + 1), 4);      // 128 Hz off
        QCOMPARE(w.notchAtPixelForTest(kCentreX - 1), 4);      // 128 Hz off
        QCOMPARE(w.notchAtPixelForTest(kCentreX + 2), -1);     // 256 Hz off
    }

    void hit_test_does_not_pad_a_notch_wider_than_two_pixels()
    {
        SpectrumWidget w;
        configure(w);
        // 400 >= 2*128, so no pad: reach is its own half width, 200 Hz.
        // With the pad it would have been 328 Hz and +2 px (256 Hz)
        // would hit. That asymmetry is the whole point of the branch.
        w.setNotchMarkers({mk(5, kCentreHz, 400.0)});

        QCOMPARE(w.notchAtPixelForTest(kCentreX + 1), 5);      // 128 Hz off
        QCOMPARE(w.notchAtPixelForTest(kCentreX + 2), -1);     // 256 Hz off
    }

    void hit_test_rejects_pixels_outside_the_spectrum_rect()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(7, kCentreHz, 400.0)});

        QCOMPARE(w.notchAtPixelForTest(-1), -1);
        QCOMPARE(w.notchAtPixelForTest(kWidgetW), -1);
    }
};

QTEST_MAIN(TestNotchHitTest)
#include "tst_notch_hit_test.moc"
```

- [ ] **Step 2: Register the test in CMake**

Append to `tests/CMakeLists.txt` after the `nereus_add_test(tst_spectrum_overlays)` block (`tests/CMakeLists.txt:4331`):

```cmake
# ── TNF design §7 / §11: panadapter notch interaction ────────────────────────
# tst_notch_hit_test pins the SpectrumWidget half of the tunable notch
# filter: the pixel-space hit test (Thetis
# MNotchDB.NotchThatSurroundsFrequencyInBW, radio.cs:4297-4325
# [v2.10.3.15] - first-found in list order, 1 px pad only when the notch
# is narrower than twice the pad), edge-vs-centre grab discrimination
# (console.cs:49037-49067 [v2.10.3.15] - 8 px minimum on-screen width,
# +/- 4 px edge zone, side-of-centre default, Shift resize), hover
# selection, centre/edge drag, wheel resize gated on selection
# (console.cs:31141-31145 + 33299-33321 [v2.10.3.15]), Ctrl +
# right-click add (console.cs:49614, 49629-49646 [v2.10.3.15]) and the
# notch context menu (AetherSDR SpectrumWidget.cpp:8517-8572 [@c6481cbf]).
# Uses QTEST_MAIN (QApplication required for SpectrumWidget); the widget
# is never shown, so no RHI context is needed.
nereus_add_test(tst_notch_hit_test)
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'notchAtPixelForTest' in 'NereusSDR::SpectrumWidget'`
(six occurrences, one per call site).

- [ ] **Step 4: Implement the hit test**

`src/gui/SpectrumWidget.h` ,  add to the public block immediately after `QColor spotBgColorForTest() const { return m_spotBgColor; }` (`:1120`):

```cpp
    // ── Notch (TNF) interaction test seams (design §7.3 / §11) ────────────
    // Public read-only view onto the private pixel-space hit test so
    // tst_notch_hit_test can pin the Thetis rule without a real mouse
    // event. Follows the spotMarkersForTest() convention above.
    int notchAtPixelForTest(int x) const;
```

`src/gui/SpectrumWidget.h` ,  add to the private section immediately after `void showSpotClusterPopup(const SpotCluster& cluster, const QPoint& globalPos);` (`:1322`):

```cpp
    // ---- Notch (TNF) interaction (design §7) ----
    // notchSpecRect: the spectrum plot rect the mouse handlers build, so
    //   the test seam and the handlers cannot drift apart.
    // notchAtPixel: pixel-space port of Thetis
    //   MNotchDB.NotchThatSurroundsFrequencyInBW.
    QRect notchSpecRect() const;
    const NotchMarker* notchMarkerById(int id) const;
    int   notchAtPixel(int x, const QRect& specRect) const;
```

`src/gui/SpectrumWidget.h` ,  add to the private members immediately after `QHash<QString, bool> m_spotSourceVisible;` (`:1726`):

```cpp
    // ---- Notch (TNF) interaction constants (design §7) ----
    // From Thetis console.cs:13221 [v2.10.3.15]:
    //   private int _max_filter_width = 10000;
    static constexpr double kNotchMaxWidthHz = 10000.0;
```

`src/gui/SpectrumWidget.cpp` ,  insert immediately after the closing brace of `specHFromHeight` (`:5885`) and before `mousePressEvent`:

```cpp
// ===========================================================================
// Notch (TNF) interaction ,  design §7
// ===========================================================================

QRect SpectrumWidget::notchSpecRect() const
{
    const int specH = specHFromHeight(height(), m_spectrumFrac,
                                      kFreqScaleH + kDividerH);
    return QRect(0, 0, width() - effectiveStripW(), specH);
}

const SpectrumWidget::NotchMarker* SpectrumWidget::notchMarkerById(int id) const
{
    for (const NotchMarker& n : m_notchMarkers) {
        if (n.id == id) {
            return &n;
        }
    }
    return nullptr;
}

// Pixel-space port of Thetis MNotchDB.NotchThatSurroundsFrequencyInBW:
// first-found in list order, with the pad applied only when the notch is
// narrower than twice the pad.
//
// From Thetis radio.cs:4297-4325 [v2.10.3.15]
// MW0LGE return first notch found that surrounds a given frequency in the given bandwidth
//   [original inline comment from radio.cs:4296]
// MW0LGE return list of notches in given bandwidth
//   [original inline comment from radio.cs:4274]
//
// Call site: console.cs:49921 [v2.10.3.15] passes HzInNPixels(1) as the
// pad ("we pad it with 1pixel worth of hz to make it selectable at low
// zoom", console.cs:49920) and widens the bandwidth window by
// _max_filter_width on both sides.
int SpectrumWidget::notchAtPixel(int x, const QRect& specRect) const
{
    if (m_notchMarkers.isEmpty() || specRect.width() <= 0) {
        return -1;
    }
    // Off-screen rejection: neither hzToX nor xToHz clamps, so a pixel
    // outside the plot maps to a frequency outside the displayed span.
    if (x < specRect.left() || x > specRect.right()) {
        return -1;
    }

    const double freqHz = xToHz(x, specRect);
    const double padHz  = m_bandwidthHz / static_cast<double>(specRect.width());
    const double windowLowHz  = m_centerHz - m_bandwidthHz / 2.0 - kNotchMaxWidthHz;
    const double windowHighHz = m_centerHz + m_bandwidthHz / 2.0 + kNotchMaxWidthHz;

    for (const NotchMarker& n : m_notchMarkers) {
        const double centreHz = n.freqMhz * 1.0e6;
        const double halfHz   = n.widthHz / 2.0;

        // NotchesInBW inclusive edge overlap. From Thetis radio.cs:4286
        // [v2.10.3.15].
        if (centreHz + halfHz < windowLowHz) {
            continue;
        }
        if (centreHz - halfHz > windowHighHz) {
            continue;
        }

        double lowHz  = centreHz - halfHz;
        double highHz = centreHz + halfHz;
        // From Thetis radio.cs:4310 [v2.10.3.15]:
        //   if (n.FWidth < (nPadWidth * 2))
        if (n.widthHz < padHz * 2.0) {
            lowHz  -= padHz;
            highHz += padHz;
        }
        if (freqHz >= lowHz && freqHz <= highHz) {
            return n.id;
        }
    }
    return -1;
}

int SpectrumWidget::notchAtPixelForTest(int x) const
{
    return notchAtPixel(x, notchSpecRect());
}
```

- [ ] **Step 5: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS (6 tests).

- [ ] **Step 6: Commit**

```bash
git add tests/tst_notch_hit_test.cpp tests/CMakeLists.txt src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
git commit -m "feat(tnf): pixel-space notch hit test on SpectrumWidget"
```

---

- [ ] **Step 7: Write the failing test (edge-vs-centre grab)**

Append to the `private slots:` block of `tests/tst_notch_hit_test.cpp`:

```cpp
    // ── §7.2 edge-vs-centre drag discrimination ──────────────────────────
    //
    // A 2000 Hz notch at 128 Hz/px is 15 px wide on screen (low edge at
    // x=492, high edge at x=507), comfortably past the 8 px gate.

    void grab_defaults_to_centre_in_the_notch_body()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});

        // x=500 is 8 px from the low edge and 7 px from the high edge:
        // outside both +/- 4 px zones, so the whole notch drags.
        QCOMPARE(w.notchGrabAtForTest(1, kCentreX, false),
                 SpectrumWidget::NotchGrab::Centre);
    }

    void grab_returns_low_edge_within_four_px_of_the_low_edge()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});
        const int lowX = xForHz(kCentreHz - 1000.0);   // 492

        QCOMPARE(w.notchGrabAtForTest(1, lowX, false),
                 SpectrumWidget::NotchGrab::LowEdge);
        QCOMPARE(w.notchGrabAtForTest(1, lowX + 3, false),
                 SpectrumWidget::NotchGrab::LowEdge);
        // 4 px is NOT near: Thetis tests Math.Abs(...) < 4.
        QCOMPARE(w.notchGrabAtForTest(1, lowX + 4, false),
                 SpectrumWidget::NotchGrab::Centre);
    }

    void grab_returns_high_edge_within_four_px_of_the_high_edge()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});
        const int highX = xForHz(kCentreHz + 1000.0);  // 507

        QCOMPARE(w.notchGrabAtForTest(1, highX, false),
                 SpectrumWidget::NotchGrab::HighEdge);
        QCOMPARE(w.notchGrabAtForTest(1, highX - 3, false),
                 SpectrumWidget::NotchGrab::HighEdge);
        QCOMPARE(w.notchGrabAtForTest(1, highX - 4, false),
                 SpectrumWidget::NotchGrab::Centre);
    }

    void grab_offers_no_edge_zone_below_eight_px_on_screen_width()
    {
        SpectrumWidget w;
        configure(w);
        // 400 Hz is 3 px wide on screen: nHpx - nLpx == 3, not > 8, so
        // Thetis never enters the edge-zone check at all.
        w.setNotchMarkers({mk(2, kCentreHz, 400.0)});
        const int highX = xForHz(kCentreHz + 200.0);

        QCOMPARE(w.notchGrabAtForTest(2, highX, false),
                 SpectrumWidget::NotchGrab::Centre);
    }

    void grab_with_shift_resizes_from_the_side_of_centre()
    {
        SpectrumWidget w;
        configure(w);
        // Same 3 px notch: the edge zones are still suppressed, but Shift
        // forces a resize and the side-of-centre default picks the edge.
        w.setNotchMarkers({mk(2, kCentreHz, 400.0)});

        QCOMPARE(w.notchGrabAtForTest(2, kCentreX + 1, true),
                 SpectrumWidget::NotchGrab::HighEdge);
        QCOMPARE(w.notchGrabAtForTest(2, kCentreX - 1, true),
                 SpectrumWidget::NotchGrab::LowEdge);
        // Exactly on centre counts as the high side (>=).
        QCOMPARE(w.notchGrabAtForTest(2, kCentreX, true),
                 SpectrumWidget::NotchGrab::HighEdge);
    }

    void grab_on_unknown_id_is_none()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});

        QCOMPARE(w.notchGrabAtForTest(99, kCentreX, false),
                 SpectrumWidget::NotchGrab::None);
    }
```

- [ ] **Step 8: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'notchGrabAtForTest' in 'NereusSDR::SpectrumWidget'` and
`error: no type named 'NotchGrab' in 'NereusSDR::SpectrumWidget'`.

- [ ] **Step 9: Implement the grab discrimination**

`src/gui/SpectrumWidget.h` ,  extend the public seam block added in Step 4 (before `int notchAtPixelForTest(int x) const;`):

```cpp
    // Which part of a notch a press at a given pixel would grab.
    // From Thetis console.cs:49037-49067 [v2.10.3.15]: the side-of-centre
    // default, an 8 px minimum on-screen width before edge zones exist at
    // all, a +/- 4 px edge zone, and Shift as an explicit alternative to
    // being near an edge.
    enum class NotchGrab { None, Centre, LowEdge, HighEdge };
```

and after `int notchAtPixelForTest(int x) const;`:

```cpp
    NotchGrab notchGrabAtForTest(int id, int x, bool shiftHeld) const;
```

`src/gui/SpectrumWidget.h` ,  extend the private helper block from Step 4:

```cpp
    NotchGrab notchGrabAt(int id, int x, bool shiftHeld,
                          const QRect& specRect) const;
```

`src/gui/SpectrumWidget.h` ,  extend the private constants block from Step 4:

```cpp
    // From Thetis console.cs:49039 [v2.10.3.15]: if (nHpx - nLpx > 8)
    static constexpr int kNotchEdgeZoneMinPx = 8;
    // From Thetis console.cs:49042 / :49047 [v2.10.3.15]:
    //   if (Math.Abs(e.X - nLpx) < 4) / else if (Math.Abs(e.X - nHpx) < 4)
    static constexpr int kNotchEdgeGrabPx = 4;
```

`src/gui/SpectrumWidget.cpp` ,  append to the notch block added in Step 4:

```cpp
// From Thetis console.cs:49037-49067 [v2.10.3.15]
// NOTCH MW0LGE  [original section marker from console.cs:48981]
SpectrumWidget::NotchGrab SpectrumWidget::notchGrabAt(
    int id, int x, bool shiftHeld, const QRect& specRect) const
{
    const NotchMarker* n = notchMarkerById(id);
    if (!n || specRect.width() <= 0) {
        return NotchGrab::None;
    }

    const double centreHz = n->freqMhz * 1.0e6;
    // upper and lower sides of the notch  [original comment, console.cs:49023]
    const int nLpx = hzToX(centreHz - n->widthHz / 2.0, specRect);
    const int nHpx = hzToX(centreHz + n->widthHz / 2.0, specRect);

    // default this based on which side of middle the mouse is
    // so that we get inuative feeling when using shift modifier to resize
    // ie we are not draggin an edge
    //   [original comments from console.cs:49034-49036]
    NotchGrab grab = (xToHz(x, specRect) >= centreHz) ? NotchGrab::HighEdge
                                                      : NotchGrab::LowEdge;

    bool nearEdge = false;
    if (nHpx - nLpx > kNotchEdgeZoneMinPx) {
        // ok, the edges are far enough appart in pixels to actually check
        // to see if we are over low or high side
        //   [original comment from console.cs:49041]
        if (std::abs(x - nLpx) < kNotchEdgeGrabPx) {
            grab = NotchGrab::LowEdge;
            nearEdge = true;
        } else if (std::abs(x - nHpx) < kNotchEdgeGrabPx) {
            grab = NotchGrab::HighEdge;
            nearEdge = true;
        }
    }

    // can also hold shift drag to resize the notch
    //   [original comment from console.cs:49056]
    return (nearEdge || shiftHeld) ? grab : NotchGrab::Centre;
}

SpectrumWidget::NotchGrab SpectrumWidget::notchGrabAtForTest(
    int id, int x, bool shiftHeld) const
{
    return notchGrabAt(id, x, shiftHeld, notchSpecRect());
}
```

- [ ] **Step 10: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS (12 tests).

- [ ] **Step 11: Commit**

```bash
git add tests/tst_notch_hit_test.cpp src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
git commit -m "feat(tnf): edge-vs-centre notch grab discrimination"
```

---

- [ ] **Step 12: Write the failing test (hover)**

Append the file-scope helper immediately after the `configure` helper in the anonymous namespace of `tests/tst_notch_hit_test.cpp`:

```cpp
// QTest::mouseMove with no button held only calls QCursor::setPos and
// never delivers an event to the widget (qtestmouse.h, Qt 6.11), so the
// hover tests synthesise the QMouseEvent directly.
void sendMouse(QWidget* w, QEvent::Type type, QPoint pos,
               Qt::MouseButton button, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)),
                   button, buttons, mods);
    QApplication::sendEvent(w, &me);
}
```

Append to the `private slots:` block:

```cpp
    // ── §7.4 hover drives selection (and therefore the wheel gate) ───────

    void hover_over_notch_sets_hovered_and_selected_ids()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});

        sendMouse(&w, QEvent::MouseMove, QPoint(kCentreX, kSpecY),
                  Qt::NoButton, Qt::NoButton);

        QCOMPARE(w.hoveredNotchIdForTest(), 1);
        QCOMPARE(w.selectedNotchIdForTest(), 1);
    }

    void hover_off_notch_clears_hovered_and_selected_ids()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});

        sendMouse(&w, QEvent::MouseMove, QPoint(kCentreX, kSpecY),
                  Qt::NoButton, Qt::NoButton);
        QCOMPARE(w.selectedNotchIdForTest(), 1);

        sendMouse(&w, QEvent::MouseMove, QPoint(kCentreX + 100, kSpecY),
                  Qt::NoButton, Qt::NoButton);

        QCOMPARE(w.hoveredNotchIdForTest(), -1);
        QCOMPARE(w.selectedNotchIdForTest(), -1);
    }

    void leave_event_clears_notch_hover_state()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});

        sendMouse(&w, QEvent::MouseMove, QPoint(kCentreX, kSpecY),
                  Qt::NoButton, Qt::NoButton);
        QCOMPARE(w.selectedNotchIdForTest(), 1);

        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&w, &leave);

        QCOMPARE(w.hoveredNotchIdForTest(), -1);
        QCOMPARE(w.selectedNotchIdForTest(), -1);
    }
```

- [ ] **Step 13: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'hoveredNotchIdForTest' in 'NereusSDR::SpectrumWidget'`
and `error: no member named 'selectedNotchIdForTest' in 'NereusSDR::SpectrumWidget'`.

- [ ] **Step 14: Implement hover selection**

`src/gui/SpectrumWidget.h` ,  extend the public seam block:

```cpp
    int selectedNotchIdForTest() const { return m_selectedNotchId; }
    int hoveredNotchIdForTest()  const { return m_hoveredNotchId; }
```

`src/gui/SpectrumWidget.h` ,  extend the private members block:

```cpp
    // ---- Notch (TNF) interaction state (design §7.4) ----
    // From Thetis console.cs:33284-33288 [v2.10.3.15] ,  the drag-state
    // block. m_selectedNotchId is Thetis's SelectedNotch: assigned from
    // the hover hit test on every non-dragging move (console.cs:49921),
    // latched while a drag is in flight, and the gate the wheel resize
    // depends on. m_hoveredNotchId drives the readout and the highlight.
    //NOTCH MW0LGE  [original section marker from console.cs:33283]
    int m_selectedNotchId{-1};
    int m_hoveredNotchId{-1};
```

`src/gui/SpectrumWidget.cpp` ,  insert in `mouseMoveEvent` immediately after the spot hover block's closing brace (`:6452`) and before the dBm-strip cursor chain (`:6454`):

```cpp
    // Notch hover: highlight, frequency/width readout, and the selection
    // the wheel resize is gated on. Thetis re-evaluates the selected
    // notch on every non-dragging move, and clears it when the cursor is
    // not over one, which is what keeps a plain scroll tuning the VFO.
    // From Thetis console.cs:49921 [v2.10.3.15]
    if (my < specH) {
        const int hoverNotch = notchAtPixel(mx, specRect);
        if (hoverNotch != m_hoveredNotchId) {
            m_hoveredNotchId = hoverNotch;
            if (hoverNotch < 0) {
                QToolTip::hideText();
            }
            markOverlayDirty();
        }
        if (hoverNotch != m_selectedNotchId) {
            m_selectedNotchId = hoverNotch;
            markOverlayDirty();
        }
        if (hoverNotch >= 0) {
            const NotchMarker* n = notchMarkerById(hoverNotch);
            if (n) {
                // AetherSDR renders this in a styled QLabel popup
                // (m_tnfHoverPopup [@c6481cbf]); NereusSDR routes the same
                // frequency + width readout through QToolTip, the path the
                // spot overlay above already uses.
                QToolTip::showText(event->globalPosition().toPoint(),
                    QString("<b>%1 MHz</b><br>Width: %2 Hz")
                        .arg(n->freqMhz, 0, 'f', 6)
                        .arg(qRound(n->widthHz)),
                    this);
            }
            setCursor(Qt::SizeHorCursor);
            markOverlayDirty();
            event->accept();
            return;
        }
    }
```

`src/gui/SpectrumWidget.cpp` ,  extend `leaveEvent` (`:6585-6594`), inserting before `QWidget::leaveEvent(event);`:

```cpp
    if (m_hoveredNotchId != -1 || m_selectedNotchId != -1) {
        m_hoveredNotchId = -1;
        m_selectedNotchId = -1;
        markOverlayDirty();
    }
```

- [ ] **Step 15: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS (15 tests).

- [ ] **Step 16: Commit**

```bash
git add tests/tst_notch_hit_test.cpp src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
git commit -m "feat(tnf): notch hover highlight and readout on the panadapter"
```

---

- [ ] **Step 17: Write the failing test (press + drag + release)**

Append to the `private slots:` block:

```cpp
    // ── §7.2 drag: whole notch, and width from either edge ───────────────

    void press_on_notch_latches_selection()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(kCentreX, kSpecY),
                  Qt::LeftButton, Qt::LeftButton);

        QCOMPARE(w.selectedNotchIdForTest(), 1);
    }

    void drag_body_emits_notch_move_requested()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchMoveRequested);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(kCentreX, kSpecY),
                  Qt::LeftButton, Qt::LeftButton);
        sendMouse(&w, QEvent::MouseMove, QPoint(kCentreX + 10, kSpecY),
                  Qt::NoButton, Qt::LeftButton);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        // 10 px right at 128 Hz/px = +1280 Hz.
        QVERIFY(std::abs(spy.at(0).at(1).toDouble()
                         - (kCentreHz + 10 * kHzPerPx)) < 1e-3);
    }

    void drag_high_edge_emits_double_the_pixel_delta_as_width()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);
        const int highX = xForHz(kCentreHz + 1000.0);   // 507

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(highX, kSpecY),
                  Qt::LeftButton, Qt::LeftButton);
        sendMouse(&w, QEvent::MouseMove, QPoint(highX + 10, kSpecY),
                  Qt::NoButton, Qt::LeftButton);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        // "we want double the diff, as we are doing 'both sides'":
        // 2000 + 2 * (10 * 128) = 4560 Hz.
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 4560.0) < 1e-3);
    }

    void drag_low_edge_grows_the_notch_when_dragged_left()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);
        const int lowX = xForHz(kCentreHz - 1000.0);    // 492

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(lowX, kSpecY),
                  Qt::LeftButton, Qt::LeftButton);
        sendMouse(&w, QEvent::MouseMove, QPoint(lowX - 10, kSpecY),
                  Qt::NoButton, Qt::LeftButton);

        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 4560.0) < 1e-3);
    }

    void release_clears_the_notch_grab()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 2000.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchMoveRequested);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(kCentreX, kSpecY),
                  Qt::LeftButton, Qt::LeftButton);
        sendMouse(&w, QEvent::MouseButtonRelease, QPoint(kCentreX, kSpecY),
                  Qt::LeftButton, Qt::NoButton);
        // A move after release is a hover, not a drag.
        sendMouse(&w, QEvent::MouseMove, QPoint(kCentreX + 10, kSpecY),
                  Qt::NoButton, Qt::NoButton);

        QCOMPARE(spy.count(), 0);
    }
```

- [ ] **Step 18: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'notchMoveRequested' in 'NereusSDR::SpectrumWidget'`
and `error: no member named 'notchWidthRequested' in 'NereusSDR::SpectrumWidget'`.

- [ ] **Step 19: Implement press, drag and release**

`src/gui/SpectrumWidget.h` ,  add to `signals:` immediately before `protected:` (`:1200`):

```cpp
    // ── Notch (TNF) interaction requests (design §8.1) ────────────────────
    // The widget owns pixel space only: every mutation is a request the
    // NotchModel validates (dedupe, rounding, min/max clamp, admin-busy).
    void notchMoveRequested(int id, double newFreqHz);
    void notchWidthRequested(int id, double widthHz);
```

`src/gui/SpectrumWidget.h` ,  extend the private notch state block:

```cpp
    // From Thetis console.cs:49059 / :49065 [v2.10.3.15]:
    //   drag_notch_start_data is the width for an edge drag and the
    //   centre frequency for a whole-notch drag.
    NotchGrab m_notchGrab{NotchGrab::None};
    int       m_notchDragStartX{0};
    double    m_notchDragStartData{0.0};
```

`src/gui/SpectrumWidget.cpp` ,  insert in `mousePressEvent` between the frequency-scale-bar block (`:6121`) and the filter-edge computation (`:6123`):

```cpp
    // 3b. Notch (TNF) grab. Thetis runs its notch block first inside
    // case MouseButtons.Left:, ahead of every filter drag, so a notch
    // marker sitting on a filter edge still drags as a notch. The dBm
    // strip / divider / freq-scale rows above are NereusSDR chrome
    // outside the spectrum plot and keep their existing precedence.
    // From Thetis console.cs:49037-49067 [v2.10.3.15]
    //NOTCH MW0LGE  [original section marker from console.cs:48981]
    if (my < specH) {
        const int hitNotch = notchAtPixel(mx, specRect);
        if (m_selectedNotchId != hitNotch) {
            m_selectedNotchId = hitNotch;
            markOverlayDirty();
        }
        if (hitNotch >= 0) {
            const bool shiftHeld =
                (event->modifiers() & Qt::ShiftModifier) != 0;
            m_notchGrab = notchGrabAt(hitNotch, mx, shiftHeld, specRect);
            const NotchMarker* n = notchMarkerById(hitNotch);
            m_notchDragStartX = mx;
            m_notchDragStartData = (m_notchGrab == NotchGrab::Centre)
                ? (n ? n->freqMhz * 1.0e6 : 0.0)
                : (n ? n->widthHz : 0.0);
            setCursor(Qt::SizeHorCursor);
            event->accept();
            return;
        }
    }
```

`src/gui/SpectrumWidget.cpp` ,  insert in `mouseMoveEvent` immediately after the `m_draggingTimeScale` block (`:6238`) and before `if (m_draggingDbm)`:

```cpp
    // Notch drag: whole-notch move, or width from a latched edge.
    // From Thetis console.cs:49928-49931 [v2.10.3.15] (centre) and
    // console.cs:49974-49987 [v2.10.3.15] (width).
    //MW0LGE [2.9.0.7] update on drag
    //   [original inline comment from console.cs:49967 ,  Thetis pushes on
    //   every mouse-move by named design; TNF design §6.2 says do not add
    //   throttling here.]
    if (m_notchGrab != NotchGrab::None && m_selectedNotchId >= 0
        && specRect.width() > 0) {
        const double hzPerPx = m_bandwidthHz / specRect.width();
        if (m_notchGrab == NotchGrab::Centre) {
            const double diff = (mx - m_notchDragStartX) * hzPerPx;
            emit notchMoveRequested(m_selectedNotchId,
                                    m_notchDragStartData + diff);
        } else {
            const double diff = (m_notchGrab == NotchGrab::HighEdge)
                ? (mx - m_notchDragStartX) * hzPerPx
                : (m_notchDragStartX - mx) * hzPerPx;
            // we want double the diff, as we are doing 'both sides'
            //   [original comment from console.cs:49984]
            double widthHz = m_notchDragStartData + diff * 2.0;
            if (widthHz < 0.0) { widthHz = 0.0; }
            if (widthHz > kNotchMaxWidthHz) { widthHz = kNotchMaxWidthHz; }
            emit notchWidthRequested(m_selectedNotchId, widthHz);
        }
        setCursor(Qt::SizeHorCursor);
        markOverlayDirty();
        event->accept();
        return;
    }
```

`src/gui/SpectrumWidget.cpp` ,  add to the left-button clear block in `mouseReleaseEvent`, immediately after `m_draggingBandwidth = false;` (`:6572`):

```cpp
        m_notchGrab = NotchGrab::None;
```

- [ ] **Step 20: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS (20 tests).

- [ ] **Step 21: Commit**

```bash
git add tests/tst_notch_hit_test.cpp src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
git commit -m "feat(tnf): drag a notch centre or edge on the panadapter"
```

---

- [ ] **Step 22: Write the failing test (wheel resize)**

Append the file-scope helper to the anonymous namespace, after `sendMouse`:

```cpp
void sendWheel(QWidget* w, QPoint pos, int angleDeltaY,
               Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QWheelEvent we(QPointF(pos), w->mapToGlobal(QPointF(pos)),
                   QPoint(0, 0), QPoint(0, angleDeltaY),
                   Qt::NoButton, mods, Qt::NoScrollPhase, false);
    QApplication::sendEvent(w, &we);
}
```

Append to the `private slots:` block:

```cpp
    // ── §7.4 wheel resize, gated on the selected notch ───────────────────

    void wheel_over_selected_notch_widens_by_ten_hz_per_detent()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 400.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);

        sendMouse(&w, QEvent::MouseMove, QPoint(kCentreX, kSpecY),
                  Qt::NoButton, Qt::NoButton);
        sendWheel(&w, QPoint(kCentreX, kSpecY), 120);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 410.0) < 1e-9);
    }

    void wheel_with_shift_steps_one_hz()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 400.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);

        sendMouse(&w, QEvent::MouseMove, QPoint(kCentreX, kSpecY),
                  Qt::NoButton, Qt::NoButton);
        sendWheel(&w, QPoint(kCentreX, kSpecY), -120, Qt::ShiftModifier);

        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 399.0) < 1e-9);
    }

    void wheel_clamps_width_to_the_thetis_maximum()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 9995.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);

        sendMouse(&w, QEvent::MouseMove, QPoint(kCentreX, kSpecY),
                  Qt::NoButton, Qt::NoButton);
        sendWheel(&w, QPoint(kCentreX, kSpecY), 120);

        QCOMPARE(spy.count(), 1);
        // _max_filter_width = 10000 (console.cs:13221).
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 10000.0) < 1e-9);
    }

    void wheel_without_a_selected_notch_does_not_resize()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 400.0)});
        QSignalSpy widthSpy(&w, &SpectrumWidget::notchWidthRequested);
        QSignalSpy tuneSpy(&w, &SpectrumWidget::frequencyClicked);

        // Hover away from the notch first: selection clears, so the wheel
        // must fall through to the VFO tune path.
        sendMouse(&w, QEvent::MouseMove, QPoint(kCentreX + 100, kSpecY),
                  Qt::NoButton, Qt::NoButton);
        sendWheel(&w, QPoint(kCentreX + 100, kSpecY), 120);

        QCOMPARE(widthSpy.count(), 0);
        QCOMPARE(tuneSpy.count(), 1);
    }
```

- [ ] **Step 23: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at runtime, four failures of the form
`FAIL!  : TestNotchHitTest::wheel_over_selected_notch_widens_by_ten_hz_per_detent() Compared values are not the same
   Actual   (spy.count()): 0
   Expected (1)          : 1`
(and `wheel_without_a_selected_notch_does_not_resize` failing on `tuneSpy.count()` being 2 once the others land, because the ungated wheel currently tunes on every scroll).

- [ ] **Step 24: Implement the wheel resize**

`src/gui/SpectrumWidget.h` ,  extend the private constants block:

```cpp
    // From Thetis console.cs:33305-33308 [v2.10.3.15]:
    //   else { tmp += wheelDelta * 10; }
    static constexpr double kNotchWheelStepHz = 10.0;
    // From Thetis console.cs:33305-33309 [v2.10.3.15]:
    //   if (Common.ShiftKeyDown) { tmp += wheelDelta; }
    static constexpr double kNotchWheelShiftStepHz = 1.0;
```

`src/gui/SpectrumWidget.cpp` ,  insert at the very top of `wheelEvent`, before the dBm-strip branch (`:6598`):

```cpp
    // MW0LGE before all, handle the notch size change
    //   [original inline comment from console.cs:31140]
    // From Thetis console.cs:31141-31145 [v2.10.3.15] ,  the wheel resizes
    // the selected notch and returns before any other wheel handling. The
    // gate is mandatory: without a selected notch a plain scroll over the
    // panadapter tunes the VFO, so an ungated resize would steal every
    // scroll (design §7.4).
    const int notchDelta = event->angleDelta().y();
    // From Thetis console.cs:31135-31137 [v2.10.3.15] ,  1 step per click.
    const int notchSteps = (notchDelta == 0) ? 0 : (notchDelta > 0 ? 1 : -1);
    if (m_selectedNotchId >= 0 && notchSteps != 0) {
        const NotchMarker* n = notchMarkerById(m_selectedNotchId);
        if (n) {
            // From Thetis console.cs:33299-33321 [v2.10.3.15]
            // notchMouseWheel.
            double tmp = n->widthHz;
            if (event->modifiers() & Qt::ShiftModifier) {
                tmp += notchSteps * kNotchWheelShiftStepHz;
            } else {
                tmp += notchSteps * kNotchWheelStepHz;
            }
            if (tmp < 0.0) { tmp = 0.0; }
            if (tmp > kNotchMaxWidthHz) { tmp = kNotchMaxWidthHz; }
            // The "both edges inside 0..max_freq" half of notchMouseWheel
            // (console.cs:33315-33318) is enforced model-side, where the
            // radio frequency limits live (design §5.4).
            emit notchWidthRequested(m_selectedNotchId, tmp);
        }
        event->accept();
        return;
    }
```

- [ ] **Step 25: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS (24 tests).

- [ ] **Step 26: Commit**

```bash
git add tests/tst_notch_hit_test.cpp src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
git commit -m "feat(tnf): wheel resize of the selected notch"
```

---

- [ ] **Step 27: Write the failing test (Ctrl + right-click add)**

Append to the `private slots:` block:

```cpp
    // ── §7.1 the add gesture is Thetis's: Ctrl + right-click ─────────────

    void ctrl_right_click_requests_a_notch_at_the_clicked_frequency()
    {
        SpectrumWidget w;
        configure(w);
        QSignalSpy spy(&w, &SpectrumWidget::notchCreateRequested);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, kSpecY),
                  Qt::RightButton, Qt::RightButton, Qt::ControlModifier);

        QCOMPARE(spy.count(), 1);
        QVERIFY(std::abs(spy.at(0).at(0).toDouble() - hzForX(600)) < 1e-3);
        QCOMPARE(spy.at(0).at(1).toBool(), false);
    }

    void ctrl_shift_right_click_requests_a_narrow_notch()
    {
        SpectrumWidget w;
        configure(w);
        QSignalSpy spy(&w, &SpectrumWidget::notchCreateRequested);

        sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, kSpecY),
                  Qt::RightButton, Qt::RightButton,
                  Qt::ControlModifier | Qt::ShiftModifier);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toBool(), true);
    }

    void plain_right_click_does_not_create_a_notch()
    {
        SpectrumWidget w;
        configure(w);
        QSignalSpy spy(&w, &SpectrumWidget::notchCreateRequested);

        // No markers pushed, so this lands on empty pan and falls through
        // to the existing overlay menu (a non-blocking popup widget).
        sendMouse(&w, QEvent::MouseButtonPress, QPoint(600, kSpecY),
                  Qt::RightButton, Qt::RightButton);

        QCOMPARE(spy.count(), 0);
    }
```

- [ ] **Step 28: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'notchCreateRequested' in 'NereusSDR::SpectrumWidget'`.

- [ ] **Step 29: Implement the add gesture**

`src/gui/SpectrumWidget.h` ,  extend the `signals:` block added in Step 19:

```cpp
    // Ctrl + right-click on the panadapter. `narrow` carries the Shift
    // modifier; the NotchModel maps it to the 100 Hz width and the plain
    // case to 200 Hz (console.cs:40268-40269 [v2.10.3.15]).
    void notchCreateRequested(double freqHz, bool narrow);
```

`src/gui/SpectrumWidget.cpp` ,  insert at the very top of the `if (event->button() == Qt::RightButton) {` branch (`:5917`), before the spot-label menu block:

```cpp
        // Ctrl + right-click adds a notch at the clicked frequency; Shift
        // makes it narrow. Checked before the spot menu and the overlay
        // menu, so plain right-click behaviour is unchanged when Ctrl is
        // not held (design §7.3).
        // From Thetis console.cs:49614, 49629-49646 [v2.10.3.15]:
        //   case MouseButtons.Right: -> if (Common.CtrlKeyDown) ->
        //   AddNotch(dFreq, rx).
        //
        // Both Control and Meta count: macOS swaps them, so the physical
        // Ctrl key arrives as Qt::MetaModifier. The zoom wheel below
        // (SpectrumWidget.cpp:6631) already accepts either.
        const bool notchCtrlHeld =
            (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) != 0;
        if (notchCtrlHeld && my < specH && mx <= specRect.right()) {
            const bool narrow = (event->modifiers() & Qt::ShiftModifier) != 0;
            emit notchCreateRequested(xToHz(mx, specRect), narrow);
            event->accept();
            return;
        }
```

- [ ] **Step 30: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS (27 tests).

- [ ] **Step 31: Commit**

```bash
git add tests/tst_notch_hit_test.cpp src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
git commit -m "feat(tnf): Ctrl plus right-click adds a notch at the cursor"
```

---

- [ ] **Step 32: Write the failing test (notch context menu)**

Append the file-scope helper to the anonymous namespace, after `sendWheel`:

```cpp
QAction* actionByText(const QList<QAction*>& actions, const QString& text)
{
    for (QAction* a : actions) {
        if (a->text() == text) {
            return a;
        }
    }
    return nullptr;
}
```

Append to the `private slots:` block:

```cpp
    // ── §7 right-click on a notch ────────────────────────────────────────
    //
    // Driven through the builder seam rather than a synthetic right-click,
    // because QMenu::exec() would block the test's event loop.

    void context_menu_header_shows_frequency_and_is_disabled()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 200.0)});

        QMenu menu;
        w.buildNotchContextMenuForTest(1, menu);

        QVERIFY(!menu.actions().isEmpty());
        QVERIFY(!menu.actions().first()->isEnabled());
        QVERIFY(menu.actions().first()->text().contains(
            QStringLiteral("14.200000")));
    }

    void context_menu_width_preset_emits_width_request()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 200.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchWidthRequested);

        QMenu menu;
        w.buildNotchContextMenuForTest(1, menu);

        QAction* widthAct = actionByText(menu.actions(),
                                         QStringLiteral("Width"));
        QVERIFY(widthAct != nullptr);
        QVERIFY(widthAct->menu() != nullptr);

        QAction* w500 = actionByText(widthAct->menu()->actions(),
                                     QStringLiteral("500 Hz"));
        QVERIFY(w500 != nullptr);
        w500->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        QVERIFY(std::abs(spy.at(0).at(1).toDouble() - 500.0) < 1e-9);

        // The preset matching the current width is checked.
        QAction* w200 = actionByText(widthAct->menu()->actions(),
                                     QStringLiteral("200 Hz"));
        QVERIFY(w200 != nullptr);
        QVERIFY(w200->isChecked());
    }

    void context_menu_bypass_emits_active_false()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 200.0, /*active*/ true)});
        QSignalSpy spy(&w, &SpectrumWidget::notchActiveRequested);

        QMenu menu;
        w.buildNotchContextMenuForTest(1, menu);
        QAction* bypass = actionByText(menu.actions(),
                                       QStringLiteral("Bypass Notch"));
        QVERIFY(bypass != nullptr);
        bypass->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
        QCOMPARE(spy.at(0).at(1).toBool(), false);
    }

    void context_menu_activate_shown_for_a_bypassed_notch()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 200.0, /*active*/ false)});
        QSignalSpy spy(&w, &SpectrumWidget::notchActiveRequested);

        QMenu menu;
        w.buildNotchContextMenuForTest(1, menu);
        QAction* activate = actionByText(menu.actions(),
                                         QStringLiteral("Activate Notch"));
        QVERIFY(activate != nullptr);
        activate->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toBool(), true);
    }

    void context_menu_remove_emits_remove_request()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers({mk(1, kCentreHz, 200.0)});
        QSignalSpy spy(&w, &SpectrumWidget::notchRemoveRequested);

        QMenu menu;
        w.buildNotchContextMenuForTest(1, menu);
        QAction* remove = actionByText(menu.actions(),
                                       QStringLiteral("Remove Notch"));
        QVERIFY(remove != nullptr);
        remove->trigger();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
    }
```

- [ ] **Step 33: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: FAIL at compile time with
`error: no member named 'buildNotchContextMenuForTest' in 'NereusSDR::SpectrumWidget'`,
`error: no member named 'notchActiveRequested' in 'NereusSDR::SpectrumWidget'` and
`error: no member named 'notchRemoveRequested' in 'NereusSDR::SpectrumWidget'`.

- [ ] **Step 34: Implement the notch context menu**

`src/gui/SpectrumWidget.h` ,  extend the `QT_BEGIN_NAMESPACE` forward-decl block (`:175-177`):

```cpp
QT_BEGIN_NAMESPACE
class QLabel;
class QMenu;
QT_END_NAMESPACE
```

`src/gui/SpectrumWidget.h` ,  extend the public seam block:

```cpp
    // Populate a caller-owned QMenu with the notch actions. Exists as a
    // seam because QMenu::exec() blocks, so the menu contents cannot be
    // asserted through a synthetic right-click.
    void buildNotchContextMenuForTest(int id, QMenu& menu) {
        buildNotchContextMenu(id, menu);
    }
```

`src/gui/SpectrumWidget.h` ,  extend the `signals:` block:

```cpp
    void notchActiveRequested(int id, bool active);
    void notchRemoveRequested(int id);
```

`src/gui/SpectrumWidget.h` ,  extend the private notch helper block:

```cpp
    void buildNotchContextMenu(int id, QMenu& menu);
```

`src/gui/SpectrumWidget.cpp` ,  append to the notch block:

```cpp
// Notch right-click menu. Contents from AetherSDR
// SpectrumWidget.cpp:8517-8572 [@c6481cbf], minus the Depth submenu and
// the Permanent toggle: WDSP's NBP has neither, so per design §1.2 the
// per-notch Active flag takes the Permanent slot.
void SpectrumWidget::buildNotchContextMenu(int id, QMenu& menu)
{
    const NotchMarker* n = notchMarkerById(id);
    if (!n) {
        return;
    }

    // Info header. AetherSDR renders it as a disabled QWidgetAction with a
    // two-line styled label (SpectrumWidget.cpp:8519-8546 [@c6481cbf]); a
    // disabled QAction carries the same text with no styling to maintain.
    QAction* info = menu.addAction(QString("%1 MHz    %2 Hz")
                                       .arg(n->freqMhz, 0, 'f', 6)
                                       .arg(qRound(n->widthHz)));
    info->setEnabled(false);
    menu.addSeparator();

    // From AetherSDR SpectrumWidget.cpp:8549-8558 [@c6481cbf]
    QMenu* widthMenu = menu.addMenu(QStringLiteral("Width"));
    for (int presetHz : {50, 100, 200, 500}) {
        QAction* a = widthMenu->addAction(
            QString("%1 Hz").arg(presetHz), this,
            [this, id, presetHz]() {
                emit notchWidthRequested(id, presetHz);
            });
        a->setCheckable(true);
        a->setChecked(qRound(n->widthHz) == presetHz);
    }

    menu.addSeparator();
    const bool active = n->active;
    menu.addAction(active ? QStringLiteral("Bypass Notch")
                          : QStringLiteral("Activate Notch"),
                   this, [this, id, active]() {
                       emit notchActiveRequested(id, !active);
                   });

    menu.addSeparator();
    // From AetherSDR SpectrumWidget.cpp:8548 [@c6481cbf] ("Remove TNF").
    menu.addAction(QStringLiteral("Remove Notch"), this,
                   [this, id]() { emit notchRemoveRequested(id); });
}
```

`src/gui/SpectrumWidget.cpp` ,  insert in the `Qt::RightButton` branch immediately after the Ctrl-add block added in Step 29, before the spot-label menu block:

```cpp
        // Right-click on a notch marker opens the notch menu. Thetis
        // suppresses every other right-click action while a notch is
        // highlighted: "if we have a notch highlighted, then all other
        // right click is ignored"  [original comment from
        // console.cs:49615, guarding the return at :49616 [v2.10.3.15]].
        if (my < specH && mx <= specRect.right()) {
            const int hitNotch = notchAtPixel(mx, specRect);
            if (hitNotch >= 0) {
                QMenu menu(this);
                buildNotchContextMenu(hitNotch, menu);
                menu.exec(event->globalPosition().toPoint());
                event->accept();
                return;
            }
        }
```

- [ ] **Step 35: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_hit_test && ctest --test-dir build -R '^tst_notch_hit_test$' --output-on-failure`

Expected: PASS (32 tests).

- [ ] **Step 36: Commit**

```bash
git add tests/tst_notch_hit_test.cpp src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
git commit -m "feat(tnf): notch right-click context menu"
```

---

### Task 8: +TNF button, status-bar indicator, menu action

**Files:**
- Create: `tests/tst_tnf_controls.cpp`
- Modify: `tests/CMakeLists.txt` (append `nereus_add_test(tst_tnf_controls)` before the `get_property(_all_tests ...)` tail block)
- Modify: `src/gui/SpectrumOverlayPanel.h:118-119` (signal gains `panId`), `src/gui/SpectrumOverlayPanel.h:162` (button-index comment)
- Modify: `src/gui/SpectrumOverlayPanel.cpp:223-229` (activate `+TNF`), `src/gui/SpectrumOverlayPanel.cpp:272-278` (delete the disabled `MNF` twin)
- Modify: `src/models/NotchModel.h` (add `tnfAddCenterHz` beside `notchSidebandShift` in the public API block), `src/models/NotchModel.cpp`
- Modify: `src/gui/MainWindow.h:183-184` (three public statics), `src/gui/MainWindow.h:662-663` (`m_tnfAction`), `src/gui/MainWindow.h:72-79` (add `<QKeySequence>`)
- Modify: `src/gui/MainWindow.cpp:1761-1762` (per-pan `addTnfClicked` handler), `src/gui/MainWindow.cpp:5192-5197` (DSP menu action), `src/gui/MainWindow.cpp:5918-5924` (status-bar light), `src/gui/MainWindow.cpp:7875-7876` (`eventFilter` branch), `src/gui/MainWindow.cpp:256-257` (add `models/NotchModel.h` include)
- Test: `tests/tst_tnf_controls.cpp`

**Interfaces:**
- Consumes (Task 3, `src/models/NotchModel.h`): `explicit NotchModel(QObject* parent = nullptr);` / `const QList<Notch>& notches() const;` / `bool globalEnabled() const;` / `void setGlobalEnabled(bool on);` / `int addNotch(double centerHz, double widthHz = 200.0);` / `static int notchSidebandShift(int filterLowHz, int filterHighHz);` / `void globalEnabledChanged(bool on);` / `void notchAdded(int id);` / `void notchRemoved(int id, int formerIndex);` / `void notchesReset();`
- Consumes (Task 4, `src/models/RadioModel.h`): `NotchModel* notchModel() const;`
- Consumes (existing): `double SliceModel::effectiveRxFrequency() const;` (`SliceModel.h:742`), `int SliceModel::filterLow() const;` (`:388`), `int SliceModel::filterHigh() const;` (`:391`), `SliceModel* MainWindow::sliceForPan(const QString& panId) const;` (`MainWindow.cpp:1784`), `void SpectrumOverlayPanel::setPanId(const QString& panId);` (`SpectrumOverlayPanel.h:69`), `constexpr auto NereusSDR::Style::kAccent` (`StyleConstants.h:44`)
- Produces: `void SpectrumOverlayPanel::addTnfClicked(const QString& panId);` (signal, arity changed from `()`), `static double NotchModel::tnfAddCenterHz(double effectiveRxFrequencyHz, int filterLowHz, int filterHighHz);`, `static QString MainWindow::tnfIndicatorStyleSheet(bool globalEnabled);`, `static QString MainWindow::tnfIndicatorTooltip(int notchCount, bool globalEnabled);`, `static QKeySequence MainWindow::tnfToggleShortcut();`, `QAction* MainWindow::m_tnfAction` (private)

---

- [ ] **Step 1: Write the failing test**

Create `tests/tst_tnf_controls.cpp`. Only the three overlay-panel slots are written now; Steps 7, 12 and 17 extend the same file.

```cpp
// =================================================================
// tests/tst_tnf_controls.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Nothing here
// is a port; the surfaces under test are fixed by
// docs/architecture/2026-07-28-tunable-notch-filter-design.md §7.5,
// §9 (overlay stub retirement) and §10.2 (shortcut scope).
//
// Build-order step 8: the three operator-facing TNF controls that live
// outside the panadapter itself.
//
//   1. The +TNF button on SpectrumOverlayPanel is live and carries the
//      id of the pan it is drawn on. The disabled "MNF" twin beside it
//      is gone -- §9: "Replace it rather than shipping both."
//   2. NotchModel::tnfAddCenterHz composes the centre that button asks
//      for: VFO + RIT, shifted into the sideband (§7.5).
//   3. The status-bar TNF light's colour and tooltip are pure functions
//      of (notch count, global enable), and the DSP-menu accelerator is
//      a named constant (§10.2: no shortcut-assignment subsystem
//      exists, so the chord is fixed rather than registered).
//
// MainWindow needs a full RadioModel (WDSP, audio, network) to
// construct, which is too heavyweight for a unit-test executable -- see
// the header of tst_mainwindow_status_bar_safety.cpp. The three
// MainWindow surfaces under test are therefore public statics, callable
// without an instance, and the menu two-way sync is mirrored in test
// scope the way tst_applet_visibility_menu_wiring.cpp mirrors its
// menus.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QKeySequence>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/StyleConstants.h"
#include "models/NotchModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestTnfControls : public QObject {
    Q_OBJECT

private:
    // SpectrumOverlayPanel parents its flyouts to parentWidget() (the host
    // SpectrumWidget in production, a bare QWidget here). Same harness as
    // tst_spectrum_overlay_panel.cpp.
    struct PanelHarness {
        QWidget host;
        SpectrumOverlayPanel* panel{nullptr};
        PanelHarness() {
            panel = new SpectrumOverlayPanel(&host);
        }
    };

    static QPushButton* buttonWithText(QWidget& host, const QString& text) {
        const QList<QPushButton*> btns = host.findChildren<QPushButton*>();
        for (QPushButton* b : btns) {
            if (b->text() == text) { return b; }
        }
        return nullptr;
    }

private slots:

    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── §9: the +TNF button is live ───────────────────────────────────────

    void tnf_add_button_is_live()
    {
        PanelHarness h;
        QPushButton* btn =
            h.panel->findChild<QPushButton*>(QStringLiteral("tnfAddButton"));
        QVERIFY(btn);
        QCOMPARE(btn->text(), QStringLiteral("+TNF"));
        QVERIFY(btn->isEnabled());
        QVERIFY2(!btn->toolTip().contains(QStringLiteral("NYI")),
                 qPrintable(btn->toolTip()));
    }

    // A control drawn on a pan acts on THAT pan, so the click has to name
    // it. Same contract as addRxClicked, which already carries m_panId.
    void tnf_add_button_reports_its_own_pan()
    {
        PanelHarness h;
        h.panel->setPanId(QStringLiteral("pan-2"));
        QPushButton* btn =
            h.panel->findChild<QPushButton*>(QStringLiteral("tnfAddButton"));
        QVERIFY(btn);

        QSignalSpy spy(h.panel, &SpectrumOverlayPanel::addTnfClicked);
        btn->click();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("pan-2"));
    }

    // §9: "There is a third stub to retire in the same work ... Replace it
    // rather than shipping both."
    void disabled_mnf_twin_is_gone()
    {
        PanelHarness h;
        QVERIFY2(buttonWithText(h.host, QStringLiteral("MNF")) == nullptr,
                 "the disabled MNF stub must be replaced by +TNF, not "
                 "shipped beside it");
    }
};

QTEST_MAIN(TestTnfControls)
#include "tst_tnf_controls.moc"
```

- [ ] **Step 2: Register the test**

Add to `tests/CMakeLists.txt`, immediately before the `get_property(_all_tests GLOBAL PROPERTY NEREUS_ALL_TESTS)` tail block:

```cmake
# ── TNF build-order step 8: +TNF button, status-bar light, menu action ───────
# Verifies the three operator-facing TNF controls that live outside the
# panadapter:
#   - SpectrumOverlayPanel's +TNF button is enabled and carries its pan id;
#     the disabled "MNF" twin is gone (design §9).
#   - NotchModel::tnfAddCenterHz composes VFO + RIT + sideband shift (§7.5).
#   - MainWindow's TNF indicator stylesheet / tooltip / accelerator statics
#     (§10.2 -- fixed chord, no shortcut-assignment subsystem exists).
# Source: NereusSDR-original test; surfaces fixed by
# docs/architecture/2026-07-28-tunable-notch-filter-design.md §7.5, §9, §10.2.
nereus_add_test(tst_tnf_controls)
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build --target tst_tnf_controls && ctest --test-dir build -R '^tst_tnf_controls$' --output-on-failure`

Expected: FAIL, 3 of 3 slots. `tnf_add_button_is_live` and `tnf_add_button_reports_its_own_pan` fail at `QVERIFY(btn)` with `'btn' returned FALSE` (the button has no `objectName` yet, so `findChild` returns null); `disabled_mnf_twin_is_gone` fails with `the disabled MNF stub must be replaced by +TNF, not shipped beside it`.

- [ ] **Step 4: Activate +TNF and delete the MNF stub**

`src/gui/SpectrumOverlayPanel.h` -- replace lines 118-119:

```cpp
    /// Add a notch on the pan this strip belongs to. Carries the pan id for
    /// the same reason addRxClicked does: a control rendered on a pan acts on
    /// THAT pan, never on whichever pan is implicitly "active".
    ///
    /// Pure signal. The notch centre is composed by NotchModel and the add is
    /// issued by MainWindow, so no upstream-DSP logic lands in this file.
    void addTnfClicked(const QString& panId);
```

`src/gui/SpectrumOverlayPanel.h` -- line 162 comment now counts seven strip buttons, not eight:

```cpp
    QVector<QPushButton*> m_menuBtns;   // indices 0-6 (buttons 2-8)
```

`src/gui/SpectrumOverlayPanel.cpp` -- replace lines 223-229:

```cpp
    // Button 3: +TNF
    {
        // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:293 [@c6481cbf] --
        // the "+TNF" entry in the strip's button table. Upstream's signal is
        // arg-less; ours carries the pan id, matching the +RX shape at
        // SpectrumOverlayMenu.cpp:313 [@c6481cbf] so a strip drawn on pan-2
        // never adds a notch on pan-0.
        auto* btn = makeMenuBtn("+TNF", this);
        btn->setObjectName(QStringLiteral("tnfAddButton"));
        btn->setToolTip("Add a notch filter at this panadapter's VFO");
        connect(btn, &QPushButton::clicked, this, [this]() {
            emit addTnfClicked(m_panId);
        });
        m_menuBtns.append(btn);  // index 1
    }
```

`src/gui/SpectrumOverlayPanel.cpp` -- delete lines 272-278 outright (the blank line 272 plus the whole `// Button 9: MNF (NYI)` block). `makeDisabledBtn` stays in use by the ATT button at index 6, so it does not become unused.

- [ ] **Step 5: Run the test, watch it pass**

Run: `cmake --build build --target tst_tnf_controls && ctest --test-dir build -R '^tst_tnf_controls$' --output-on-failure`

Expected: PASS (3 slots).

- [ ] **Step 6: Commit**

```bash
git add tests/tst_tnf_controls.cpp tests/CMakeLists.txt \
        src/gui/SpectrumOverlayPanel.h src/gui/SpectrumOverlayPanel.cpp
git commit -m "feat(tnf): activate the +TNF overlay button and retire the MNF stub"
```

---

- [ ] **Step 7: Extend the test for the +TNF centre composition**

Append these three slots to `TestTnfControls` in `tests/tst_tnf_controls.cpp`, after `disabled_mnf_twin_is_gone()`:

```cpp
    // ── §7.5: +TNF centre = VFO + RIT, shifted into the sideband ──────────

    // USB F5 default passband is 100..3000 Hz, so notchSidebandShift returns
    // 100 + (2900 / 2) = 1550 and the notch lands mid-sideband, not on the
    // suppressed carrier.
    void tnf_add_centre_shifts_into_the_usb_sideband()
    {
        const double centre = NotchModel::tnfAddCenterHz(14200000.0, 100, 3000);
        QCOMPARE(centre, 14200000.0 + 1550.0);
    }

    // LSB mirrors it: -3000..-100 puts the middle at -1550 Hz.
    void tnf_add_centre_shifts_into_the_lsb_sideband()
    {
        const double centre = NotchModel::tnfAddCenterHz(7100000.0, -3000, -100);
        QCOMPARE(centre, 7100000.0 - 1550.0);
    }

    // The RIT term is the deliberate §7.5 divergence. Thetis scales RITValue
    // by 1e-6 onto a Hz quantity, which makes 100 Hz of RIT move the notch by
    // 0.0001 Hz -- a unit bug, not a behavioural choice. §4.1 puts RIT inside
    // the WDSP shift, so the demodulated RF already carries it and a notch at
    // bare VFO would sit rit_hz off the signal. SliceModel::effectiveRxFrequency
    // is the existing VFO+RIT accessor, so the caller feeds that in.
    void tnf_add_centre_carries_rit_because_the_shift_does()
    {
        SliceModel slice;
        slice.setFrequency(14200000.0);
        slice.setFilterLow(100);
        slice.setFilterHigh(3000);
        slice.setRitEnabled(true);
        slice.setRitHz(250);

        const double centre = NotchModel::tnfAddCenterHz(
            slice.effectiveRxFrequency(), slice.filterLow(), slice.filterHigh());
        QCOMPARE(centre, 14200000.0 + 250.0 + 1550.0);
    }
```

- [ ] **Step 8: Run it and watch it fail**

Run: `cmake --build build --target tst_tnf_controls && ctest --test-dir build -R '^tst_tnf_controls$' --output-on-failure`

Expected: FAIL at compile. `tests/tst_tnf_controls.cpp: error: no member named 'tnfAddCenterHz' in 'NereusSDR::NotchModel'` (three occurrences). No test binary is produced.

- [ ] **Step 9: Add NotchModel::tnfAddCenterHz**

`src/models/NotchModel.h` -- declare directly under the existing `notchSidebandShift` declaration in the public API block:

```cpp
    // The centre +TNF asks for. Static and slice-agnostic for the same reason
    // notchSidebandShift is: a global NotchModel (D1) cannot reach per-slice
    // filter edges or per-slice RIT, so the caller supplies both.
    //
    // From Thetis console.cs:40317-40330 [v2.10.3.15] -- TNFAdd(rx):
    //     vfoHz = VFOAFreq * 1.0e6;
    //     if (RITOn) vfoHz += (double)RITValue * 1e-6;   // check for RIT
    //     vfoHz += notchSidebandShift(rx);               //MW0LGE_21k9rc4
    //     AddNotch(vfoHz, rx);
    //
    // Deliberate divergence, design §7.5: upstream's `* 1e-6` scales an
    // already-Hz RITValue onto an already-Hz VFO, making the RIT term inert
    // (100 Hz of RIT moves the notch by 0.0001 Hz). We port the evident
    // intent, place the notch where the operator is listening, and fix the
    // unit by taking the RIT-inclusive frequency as the argument.
    // Flagged for maintainer review: reverting to upstream's effective
    // behaviour means passing the bare VFO instead.
    static double tnfAddCenterHz(double effectiveRxFrequencyHz,
                                 int filterLowHz, int filterHighHz);
```

`src/models/NotchModel.cpp` -- define immediately after `notchSidebandShift`:

```cpp
double NotchModel::tnfAddCenterHz(double effectiveRxFrequencyHz,
                                  int filterLowHz, int filterHighHz)
{
    // From Thetis console.cs:40329 [v2.10.3.15]:
    //     vfoHz += notchSidebandShift(rx); //MW0LGE_21k9rc4
    return effectiveRxFrequencyHz
         + static_cast<double>(notchSidebandShift(filterLowHz, filterHighHz));
}
```

- [ ] **Step 10: Run the test, watch it pass**

Run: `cmake --build build --target tst_tnf_controls && ctest --test-dir build -R '^tst_tnf_controls$' --output-on-failure`

Expected: PASS (6 slots).

- [ ] **Step 11: Commit**

```bash
git add tests/tst_tnf_controls.cpp src/models/NotchModel.h src/models/NotchModel.cpp
git commit -m "feat(tnf): compose the +TNF notch centre from VFO, RIT and sideband shift"
```

---

- [ ] **Step 12: Extend the test for the status-bar indicator**

Append these four slots to `TestTnfControls`, after `tnf_add_centre_carries_rit_because_the_shift_does()`:

```cpp
    // ── §7: the status-bar TNF light follows the global flag ──────────────

    void tnf_indicator_lights_accent_when_enabled()
    {
        const QString on = MainWindow::tnfIndicatorStyleSheet(true);
        QVERIFY2(on.contains(QLatin1String(Style::kAccent)), qPrintable(on));
        QVERIFY2(on.contains(QStringLiteral("font-size: 11px")), qPrintable(on));
    }

    // #404858 is the literal the CWX / DVK / FDX labels beside it already use
    // in buildStatusBar(), so the four stay a matched set when TNF is off.
    void tnf_indicator_dims_to_its_siblings_when_disabled()
    {
        const QString off = MainWindow::tnfIndicatorStyleSheet(false);
        QVERIFY2(off.contains(QStringLiteral("#404858")), qPrintable(off));
        QVERIFY2(!off.contains(QLatin1String(Style::kAccent)), qPrintable(off));
        QVERIFY2(off.contains(QStringLiteral("font-size: 11px")), qPrintable(off));
    }

    void tnf_tooltip_reports_an_empty_notch_list()
    {
        const QString tip = MainWindow::tnfIndicatorTooltip(0, false);
        QVERIFY2(tip.contains(QStringLiteral("no notches")), qPrintable(tip));
        QVERIFY2(tip.contains(QStringLiteral("Click to toggle")), qPrintable(tip));
    }

    void tnf_tooltip_reports_count_and_state()
    {
        const QString many = MainWindow::tnfIndicatorTooltip(3, true);
        QVERIFY2(many.contains(QStringLiteral("3 notches")), qPrintable(many));
        QVERIFY2(many.contains(QStringLiteral("enabled")), qPrintable(many));

        const QString one = MainWindow::tnfIndicatorTooltip(1, false);
        QVERIFY2(one.contains(QStringLiteral("1 notch,")), qPrintable(one));
        QVERIFY2(one.contains(QStringLiteral("bypassed")), qPrintable(one));
    }
```

- [ ] **Step 13: Run it and watch it fail**

Run: `cmake --build build --target tst_tnf_controls && ctest --test-dir build -R '^tst_tnf_controls$' --output-on-failure`

Expected: FAIL at compile. `tests/tst_tnf_controls.cpp: error: no member named 'tnfIndicatorStyleSheet' in 'NereusSDR::MainWindow'` and `error: no member named 'tnfIndicatorTooltip' in 'NereusSDR::MainWindow'`.

- [ ] **Step 14: Wire the status-bar light**

`src/gui/MainWindow.h` -- add `#include <QKeySequence>` to the include block at lines 72-79 (after `#include <QActionGroup>`), and insert into the public section at line 184 (after `setVoltsAmpsVisible`):

```cpp
    // ── TNF status-bar light (design §7) ──────────────────────────────────
    // Public statics rather than file-local helpers: MainWindow needs a full
    // RadioModel (WDSP, audio, network) to construct, which no unit-test
    // executable can afford, so these are the only shape in which the
    // indicator's two pure behaviours can be tested. Both are called from
    // buildStatusBar().
    static QString tnfIndicatorStyleSheet(bool globalEnabled);
    static QString tnfIndicatorTooltip(int notchCount, bool globalEnabled);
```

`src/gui/MainWindow.cpp` -- add the model include beside the existing ones at line 257:

```cpp
#include "models/NotchModel.h"
```

`src/gui/MainWindow.cpp` -- define the two statics next to the other status-bar helpers, immediately above `void MainWindow::buildStatusBar()` (line 5797):

```cpp
QString MainWindow::tnfIndicatorStyleSheet(bool globalEnabled)
{
    // ON reads accent cyan, matching AetherSDR's status-bar TNF light
    // (MainWindow_Wiring.cpp:3308-3310 [@c6481cbf], #00b4d8 on / #404858 off).
    // OFF keeps the literal #404858 the CWX / DVK / FDX labels beside it
    // already use, so the four stay a matched set.
    const QString color = globalEnabled ? QString::fromLatin1(Style::kAccent)
                                        : QStringLiteral("#404858");
    return QStringLiteral(
               "QLabel { color: %1; font-weight: bold; font-size: 11px; }")
        .arg(color);
}

QString MainWindow::tnfIndicatorTooltip(int notchCount, bool globalEnabled)
{
    // Shape from AetherSDR buildTnfTooltip (MainWindowHelpers.cpp:233-247
    // [@c6481cbf]): name the feature, say how many notches exist, say the
    // click toggles them. Upstream renders an HTML table of every notch;
    // ours stays a single plain line because the Settings > DSP > MNF table
    // (design §9) is where the per-notch list lives.
    if (notchCount <= 0) {
        return QStringLiteral("Tunable Notch Filter: no notches. "
                              "Click to toggle all notches.");
    }
    return QStringLiteral("Tunable Notch Filter: %1 notch%2, %3. "
                          "Click to toggle all notches.")
        .arg(notchCount)
        .arg(notchCount == 1 ? QString() : QStringLiteral("es"),
             globalEnabled ? QStringLiteral("enabled")
                           : QStringLiteral("bypassed"));
}
```

`src/gui/MainWindow.cpp` -- replace lines 5918-5924 inside `buildStatusBar()`:

```cpp
    // TNF light. Click toggles every notch at once; colour and tooltip follow
    // NotchModel::globalEnabled.
    // From AetherSDR MainWindow.cpp:4422-4432 [@c6481cbf] (indicator label +
    // tooltip refresh on list changes) and MainWindow_Wiring.cpp:3306-3311
    // [@c6481cbf] (globalEnabledChanged drives the stylesheet).
    //
    // Not gated on isConnected the way AetherSDR's is: under design decision
    // D3 the notch list is persisted client-side operator state, not a mirror
    // of radio state, so it is meaningful before a radio is attached.
    m_tnfLabel = new QLabel(QStringLiteral("TNF"), barWidget);
    m_tnfLabel->setCursor(Qt::PointingHandCursor);
    m_tnfLabel->setProperty("isTnfToggle", true);
    m_tnfLabel->installEventFilter(this);
    hbox->addWidget(m_tnfLabel);

    if (NotchModel* notches = m_radioModel->notchModel()) {
        auto refreshTnfIndicator = [this, notches]() {
            if (!m_tnfLabel) { return; }
            const bool on = notches->globalEnabled();
            m_tnfLabel->setStyleSheet(tnfIndicatorStyleSheet(on));
            m_tnfLabel->setToolTip(tnfIndicatorTooltip(
                static_cast<int>(notches->notches().size()), on));
        };
        // Seed now; restoreFromSettings may not have run yet, in which case
        // the signal connections below carry the corrected state.
        refreshTnfIndicator();
        connect(notches, &NotchModel::globalEnabledChanged, this,
                [refreshTnfIndicator](bool) { refreshTnfIndicator(); });
        connect(notches, &NotchModel::notchAdded, this,
                [refreshTnfIndicator](int) { refreshTnfIndicator(); });
        connect(notches, &NotchModel::notchRemoved, this,
                [refreshTnfIndicator](int, int) { refreshTnfIndicator(); });
        connect(notches, &NotchModel::notchesReset, this, refreshTnfIndicator);
    }
```

`src/gui/MainWindow.cpp` -- add the click branch inside the existing `MouseButtonPress` block in `eventFilter`, between line 7875 (`}` closing the `isPanelToggle` branch) and line 7876 (`}` closing the `if (event->type() == ...)`):

```cpp
        // Status-bar TNF light: click toggles every notch at once.
        // From AetherSDR MainWindow_Shortcuts.cpp:612-614 [@c6481cbf], which
        // flips the model flag straight from the indicator's mouse press.
        if (label && label->property("isTnfToggle").toBool()) {
            NotchModel* notches =
                m_radioModel ? m_radioModel->notchModel() : nullptr;
            if (notches) {
                notches->setGlobalEnabled(!notches->globalEnabled());
            }
            return true;  // event consumed
        }
```

- [ ] **Step 15: Run the test, watch it pass**

Run: `cmake --build build --target tst_tnf_controls && ctest --test-dir build -R '^tst_tnf_controls$' --output-on-failure`

Expected: PASS (10 slots).

- [ ] **Step 16: Commit**

```bash
git add tests/tst_tnf_controls.cpp src/gui/MainWindow.h src/gui/MainWindow.cpp
git commit -m "feat(tnf): wire the status-bar TNF light to NotchModel"
```

---

- [ ] **Step 17: Extend the test for the menu action and its accelerator**

Append these two slots to `TestTnfControls`, after `tnf_tooltip_reports_count_and_state()`:

```cpp
    // ── §10.2: fixed accelerator, because there is nothing to register with
    //           (KeyboardSetupPages.cpp is a 100% NYI stub and no
    //           ShortcutManager exists anywhere in src/) ──────────────────

    void tnf_toggle_accelerator_is_fixed_and_unclaimed()
    {
        QCOMPARE(MainWindow::tnfToggleShortcut(),
                 QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));

        // Every accelerator MainWindow already hands out. A collision means
        // two menu items fight for the same chord and Qt fires neither
        // (QAction::ambiguous). Read out of MainWindow.cpp: 4705, 4729, 4744,
        // 4802, 4877, 4920, 5044, 5584, 5597, 5637, 5656, 5787.
        const QList<QKeySequence> taken = {
            QKeySequence(Qt::CTRL | Qt::Key_Comma),
            QKeySequence(Qt::CTRL | Qt::Key_Q),
            QKeySequence(Qt::CTRL | Qt::Key_K),
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K),
            QKeySequence(QStringLiteral("Ctrl+L")),
            QKeySequence(QStringLiteral("Ctrl+R")),
            QKeySequence(Qt::CTRL | Qt::Key_M),
            QKeySequence(QStringLiteral("Ctrl+Shift+S")),
            QKeySequence(QStringLiteral("Ctrl+Shift+R")),
            QKeySequence(QStringLiteral("Ctrl+Shift+D")),
            QKeySequence(Qt::CTRL | Qt::Key_X),
            QKeySequence(QStringLiteral("Ctrl+Shift+K")),
        };
        QVERIFY2(!taken.contains(MainWindow::tnfToggleShortcut()),
                 "TNF accelerator collides with one MainWindow already "
                 "registers");
    }

    // Mirrors the two connects buildMenuBar() makes between DSP > TNF and
    // NotchModel, the way tst_applet_visibility_menu_wiring.cpp mirrors its
    // menus: MainWindow cannot be instantiated in a unit test, but the model
    // and the QAction here are the production types. The bug this pins is the
    // echo loop -- without the QSignalBlocker, setChecked re-emits toggled,
    // which writes the model again.
    void tnf_menu_action_and_model_stay_in_sync_without_recursion()
    {
        NotchModel notches;
        QAction action;
        action.setCheckable(true);
        const bool initial = notches.globalEnabled();
        action.setChecked(initial);

        QObject::connect(&action, &QAction::toggled,
                         &notches, &NotchModel::setGlobalEnabled);
        QObject::connect(&notches, &NotchModel::globalEnabledChanged,
                         &action, [&action](bool on) {
            QSignalBlocker b(&action);
            action.setChecked(on);
        });

        QSignalSpy modelSpy(&notches, &NotchModel::globalEnabledChanged);
        QSignalSpy actionSpy(&action, &QAction::toggled);

        action.toggle();                          // operator picks the item
        QCOMPARE(notches.globalEnabled(), !initial);
        QCOMPARE(modelSpy.count(), 1);
        QCOMPARE(actionSpy.count(), 1);

        notches.setGlobalEnabled(initial);        // status-bar light, or TCI
        QCOMPARE(action.isChecked(), initial);
        QCOMPARE(modelSpy.count(), 2);
        QCOMPARE(actionSpy.count(), 1);           // blocker held the echo
    }
```

- [ ] **Step 18: Run it and watch it fail**

Run: `cmake --build build --target tst_tnf_controls && ctest --test-dir build -R '^tst_tnf_controls$' --output-on-failure`

Expected: FAIL at compile. `tests/tst_tnf_controls.cpp: error: no member named 'tnfToggleShortcut' in 'NereusSDR::MainWindow'` (two occurrences).

- [ ] **Step 19: Ship the menu action, the accelerator and the +TNF handler**

`src/gui/MainWindow.h` -- add beside the other DSP menu actions at line 663 (after `m_binAction`):

```cpp
    QAction*      m_tnfAction = nullptr;
```

`src/gui/MainWindow.h` -- add to the public TNF block created in Step 14:

```cpp
    // The DSP > TNF accelerator. Public + static so the collision test can
    // read it without an instance; design §10.2 fixes it in code because
    // NereusSDR has no shortcut-assignment subsystem to register with.
    static QKeySequence tnfToggleShortcut();
```

`src/gui/MainWindow.cpp` -- define beside the other two statics, above `buildStatusBar()`:

```cpp
QKeySequence MainWindow::tnfToggleShortcut()
{
    // Design §10.2: KeyboardSetupPages.cpp:32-70 is a 100% NYI stub, no
    // ShortcutManager or registerAction exists in src/, and every shipped
    // shortcut is a plain QAction::setShortcut here. AetherSDR registers
    // "tnf_toggle" with an empty default sequence
    // (MainWindow_Shortcuts.cpp:1093 [@c6481cbf]) precisely because it HAS a
    // manager to bind it later; we ship a fixed chord instead. Building the
    // assignment subsystem is a separate epic and explicitly out of scope.
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N);
}
```

`src/gui/MainWindow.cpp` -- replace lines 5192-5197 in `buildMenuBar()`:

```cpp
    // TNF: enable or bypass every notch at once. Global rather than per-slice
    // because the notch list itself is global (design decision D1), which is
    // also how Thetis models it: TNFActive is a single flag despite the
    // per-rx command shape (console.cs:52317-52326 [v2.10.3.15], GetMNF is
    // documented "mnf enabled globally").
    m_tnfAction = dspMenu->addAction(QStringLiteral("&TNF"));
    m_tnfAction->setCheckable(true);
    m_tnfAction->setShortcut(tnfToggleShortcut());
    m_tnfAction->setToolTip(
        QStringLiteral("Enable or bypass all tunable notch filters"));
    if (NotchModel* notches = m_radioModel->notchModel()) {
        m_tnfAction->setChecked(notches->globalEnabled());
        connect(m_tnfAction, &QAction::toggled,
                notches, &NotchModel::setGlobalEnabled);
        connect(notches, &NotchModel::globalEnabledChanged,
                m_tnfAction, [this](bool on) {
            QSignalBlocker b(m_tnfAction);
            m_tnfAction->setChecked(on);
        });
    }
```

`src/gui/MainWindow.cpp` -- insert in the per-pan overlay loop, after the `addRxClicked` connect that ends at line 1761:

```cpp
        // +TNF adds a notch on the slice THIS pan is showing, at the
        // frequency the operator is actually listening to.
        // From Thetis console.cs:40313-40331 [v2.10.3.15] -- TNFAdd(rx):
        // VFO, plus RIT, shifted into the sideband, then AddNotch. The
        // arithmetic lives in NotchModel::tnfAddCenterHz (design §7.5 and
        // §10.2); the admin-busy guard that upstream repeats at
        // console.cs:40315 is already enforced inside NotchModel::addNotch
        // (console.cs:40224), so the reject path is the model's.
        connect(panel, &SpectrumOverlayPanel::addTnfClicked, this,
                [this](const QString& id) {
            if (!m_radioModel) { return; }
            NotchModel* notches = m_radioModel->notchModel();
            SliceModel* slice = sliceForPan(id);
            if (!notches || !slice) { return; }
            notches->addNotch(NotchModel::tnfAddCenterHz(
                slice->effectiveRxFrequency(),
                slice->filterLow(), slice->filterHigh()));
        });
```

- [ ] **Step 20: Run the test, watch it pass**

Run: `cmake --build build --target tst_tnf_controls && ctest --test-dir build -R '^tst_tnf_controls$' --output-on-failure`

Expected: PASS (12 slots).

- [ ] **Step 21: Commit**

```bash
git add tests/tst_tnf_controls.cpp src/gui/MainWindow.h src/gui/MainWindow.cpp
git commit -m "feat(tnf): ship the DSP menu TNF toggle with a fixed accelerator"
```

---

### Task 9: Fill in the existing MnfSetupPage

**Files:**
- Modify: `src/gui/setup/DspSetupPages.h:245-251` (the `MnfSetupPage` class body) and `:90-93` (includes)
- Modify: `src/gui/setup/DspSetupPages.cpp:2103-2128` (the placeholder ctor block) and `:15-30` (includes)
- Modify: `src/core/WdspEngine.h:100-123` (test forward decls) and `:674-694` (friend list)
- Modify: `tests/CMakeLists.txt:2367` (append registration after `nereus_add_test(tst_cfc_setup_page)`)
- Test: `tests/tst_mnf_setup_page.cpp`

**Interfaces:**

- Consumes (from Task 3, `src/models/NotchModel.h`):
  - `const QList<Notch>& NotchModel::notches() const`
  - `const Notch* NotchModel::notchById(int id) const`
  - `bool NotchModel::autoIncrease() const`
  - `int NotchModel::addNotch(double centerHz, double widthHz = 200.0)`
  - `bool NotchModel::setCenter(int id, double centerHz)`
  - `bool NotchModel::setWidth(int id, double widthHz)`
  - `bool NotchModel::setActive(int id, bool active)`
  - `bool NotchModel::removeNotch(int id)`
  - `void NotchModel::setAutoIncrease(bool on)`
  - `void NotchModel::setAdminBusy(bool busy)` / `bool NotchModel::adminBusy() const`
  - signals `notchAdded(int)`, `notchChanged(int)`, `notchRemoved(int,int)`, `notchesReset()`, `autoIncreaseChanged(bool)`
  - `Notch` fields `id` / `centerHz` / `widthHz` / `active` (design §5.1)
- Consumes (from Task 2, `src/core/RxChannel.h`): `double RxChannel::minNotchWidthHz() const`
- Consumes (from Task 4, `src/models/RadioModel.h`): `NotchModel* RadioModel::notchModel() const`
- Consumes (existing): `SliceModel* RadioModel::activeSlice() const` (`RadioModel.h:385`), `WdspEngine* RadioModel::wdspEngine()` (`RadioModel.h:233`), `RxChannel* WdspEngine::rxChannel(int) const` (`WdspEngine.h:238`), `WdspEngine::kFirstSliceChannelId` (`WdspEngine.h:211`), `double SliceModel::frequency() const` (`SliceModel.h:376`), `int SliceModel::sliceIndex() const` (`SliceModel.h:453`), `QHBoxLayout* SetupPage::addLabeledLabel(QLayout*, const QString&, QLabel*)` (`SetupPage.h:82`)
- Produces:
  - `MnfSetupPage::MnfSetupPage(RadioModel* model, QWidget* parent = nullptr)` (signature unchanged; `SetupDialog.cpp:609` needs no edit)
  - `void MnfSetupPage::showEvent(QShowEvent* event) override`
  - private: `void rebuildTable()`, `void refreshRow(int notchId)`, `void commitRow(int notchId)`, `void beginAdminEdit()`, `void endAdminEdit()`, `void refreshMinNotchWidth()`
  - object names other code/tests can find: page-level `tblMNFNotches`, `btnMNFAdd`, `chkMNFAutoIncrease`, `lblMNFMinWidth`; per-row `udMNFFreq`, `udMNFWidth`, `chkMNFActive`, `btnMNFDelete`
  - `friend class ::TestMnfSetupPage;` on `WdspEngine` (Task 10 may reuse the seam)

---

#### Cycle A: auto-increase control

- [ ] **Step 1: Write the failing test**

```cpp
// =================================================================
// tests/tst_mnf_setup_page.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF design §9 (Settings → DSP → MNF), §5.3 (NotchModel::adminBusy),
// build order step 9:
//   docs/architecture/2026-07-28-tunable-notch-filter-design.md
//
// Coverage:
//   A. Auto-increase exists with the Thetis object name and binds both
//      directions without an echo loop.
//   B. The table carries one row per notch; Add seeds from the active slice.
//   C. Row edits (frequency, width, active) and Delete reach NotchModel.
//   D. An in-flight row edit holds adminBusy and blocks the panadapter path;
//      committing clears it first.
//   E. The minimum-notch-width readout follows the live RxChannel.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>

#include "core/AppSettings.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "gui/setup/DspSetupPages.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestMnfSetupPage : public QObject
{
    Q_OBJECT

private:
    // A RadioModel with a stream pool and one bound slice. Same fixture shape
    // as tests/tst_stream_pool_binding.cpp:76-88.
    static int seedSlice(RadioModel& model, double frequencyHz)
    {
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        if (slice) {
            slice->setFrequency(frequencyHz);
        }
        return id;
    }

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ── A. Auto-increase ─────────────────────────────────────────────────────

    void autoIncrease_controlExistsAndIsEnabled()
    {
        RadioModel model;
        MnfSetupPage page(&model);
        page.show();

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease"));
        QVERIFY(chk);
        // The placeholder ended in disableGroup(); the wired page must not.
        QVERIFY(chk->isEnabled());
    }

    void autoIncrease_mirrorsModelOnConstruction()
    {
        RadioModel model;
        model.notchModel()->setAutoIncrease(false);

        MnfSetupPage page(&model);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease"));
        QVERIFY(chk);
        QCOMPARE(chk->isChecked(), false);
    }

    void autoIncrease_toggleWritesModel()
    {
        RadioModel model;
        model.notchModel()->setAutoIncrease(false);

        MnfSetupPage page(&model);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease"));
        QVERIFY(chk);

        chk->setChecked(true);
        QCOMPARE(model.notchModel()->autoIncrease(), true);
    }

    void autoIncrease_modelChangeUpdatesCheckboxWithoutEcho()
    {
        RadioModel model;
        model.notchModel()->setAutoIncrease(false);

        MnfSetupPage page(&model);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease"));
        QVERIFY(chk);

        QSignalSpy spy(model.notchModel(), &NotchModel::autoIncreaseChanged);
        model.notchModel()->setAutoIncrease(true);

        QCOMPARE(chk->isChecked(), true);
        // One emit only: the QSignalBlocker in the model→widget direction
        // stops the checkbox echoing back into setAutoIncrease.
        QCOMPARE(spy.count(), 1);
    }

    void nullModel_doesNotCrash()
    {
        MnfSetupPage page(nullptr);
        page.show();
        QVERIFY(!page.findChild<QCheckBox*>(QStringLiteral("chkMNFAutoIncrease")));
    }
};

QTEST_MAIN(TestMnfSetupPage)
#include "tst_mnf_setup_page.moc"
```

- [ ] **Step 2: Register the test**

Append after `nereus_add_test(tst_cfc_setup_page)` (`tests/CMakeLists.txt:2367`):

```cmake
# ── TNF design §9 / build-order step 9: MnfSetupPage full implementation ─────
# Verifies Setup → DSP → MNF stops being a placeholder: the auto-increase
# checkbox, the notch table (one row per NotchModel entry, Thetis object names
# udMNFFreq / udMNFWidth / chkMNFActive / btnMNFDelete), the Add button seeding
# from the active slice's VFO, the adminBusy edit lock (Thetis
# SetupForm.NotchAdminBusy, console.cs:40009 [v2.10.3.15]) and the minimum
# notch width readout (RXANBPGetMinNotchWidth, nbp.c:594).
# Layout follows Thetis grpDSPMNF (setup.designer.cs:44145-44412 [v2.10.3.15]).
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md §9, §5.3.
nereus_add_test(tst_mnf_setup_page)
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build --target tst_mnf_setup_page && ctest --test-dir build -R '^tst_mnf_setup_page$' --output-on-failure`

Expected: FAIL. `autoIncrease_controlExistsAndIsEnabled` reports `FAIL!  : TestMnfSetupPage::autoIncrease_controlExistsAndIsEnabled() 'chk' returned FALSE.` The placeholder builds a `QPushButton` labelled "Enable" with no object name, so `findChild<QCheckBox*>("chkMNFAutoIncrease")` is null. The three binding tests fail the same way.

- [ ] **Step 4: Replace the placeholder ctor with the group box plus auto-increase**

`src/gui/setup/DspSetupPages.h` ,  replace lines 245-251:

```cpp
// ── MNF ──────────────────────────────────────────────────────────────────────

class MnfSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit MnfSetupPage(RadioModel* model, QWidget* parent = nullptr);

private:
    QCheckBox* m_autoIncreaseChk{nullptr};
};
```

`src/gui/setup/DspSetupPages.cpp` ,  add to the include block (`:15-30`, alphabetical):

```cpp
#include "models/NotchModel.h"
```

`src/gui/setup/DspSetupPages.cpp` ,  replace lines 2103-2128:

```cpp
// ══════════════════════════════════════════════════════════════════════════════
// MnfSetupPage
// ══════════════════════════════════════════════════════════════════════════════
//
// Setup → DSP → MNF. The tab keeps Thetis's name (setup.designer.cs:44141
// [v2.10.3.15], this.tpDSPMNF.Text = "MNF") and the group box keeps Thetis's
// caption (:44165, grpDSPMNF.Text = "Multi Notch Filter"); everything
// operator-facing outside Settings says TNF.
//
// grpDSPMNF's control set is at setup.designer.cs:44145-44159 [v2.10.3.15].
// This page keeps those object names and their verbatim tooltips but replaces
// the upstream one-notch-at-a-time shape (udMNFNotch index spinner plus
// Add / Edit / Enter / Cancel modal buttons) with a table that edits every
// notch in place. chkVisualNotch arrives with the visual-notch work.
//
// The prior placeholder carried a "Window" combo. No such control exists
// upstream (there is no comboMNFWindow anywhere in Thetis v2.10.3.15) and the
// bandpass window is out of scope, so it is dropped rather than wired.
MnfSetupPage::MnfSetupPage(RadioModel* model, QWidget* parent)
    : SetupPage("MNF", model, parent)
{
    // From Thetis setup.designer.cs:44165 [v2.10.3.15] ,  grpDSPMNF.Text.
    QGroupBox* mnfGrp = addSection(QStringLiteral("Multi Notch Filter"));
    QVBoxLayout* mnfLay = qobject_cast<QVBoxLayout*>(mnfGrp->layout());

    if (!model || !model->notchModel() || !mnfLay) {
        disableGroup(mnfGrp);
        return;
    }

    NotchModel* nm = model->notchModel();

    // ── Auto-increase ────────────────────────────────────────────────────────
    // From Thetis setup.designer.cs:44204 [v2.10.3.15] ,  chkMNFAutoIncrease.Text.
    m_autoIncreaseChk = new QCheckBox(
        QStringLiteral("Auto-Increase width (if needed) to achieve >100dB attenuation"),
        mnfGrp);
    m_autoIncreaseChk->setObjectName(QStringLiteral("chkMNFAutoIncrease"));
    m_autoIncreaseChk->setChecked(nm->autoIncrease());
    // From Thetis setup.designer.cs:44205 [v2.10.3.15] ,  chkMNFAutoIncrease tooltip.
    m_autoIncreaseChk->setToolTip(QStringLiteral(
        "The notch width will be increased if needed to ensure >100dB of attenuation"));
    mnfLay->addWidget(m_autoIncreaseChk);

    // Thetis fans the flag straight to three fixed channel ids from the Setup
    // form (setup.cs:17925-17932 [v2.10.3.15], chkMNFAutoIncrease_CheckedChanged
    // → WDSP.RXANBPSetAutoIncrease ×3). NereusSDR routes it through NotchModel
    // so RadioModel's fan-out reaches every open slice channel instead.
    connect(m_autoIncreaseChk, &QCheckBox::toggled, nm, &NotchModel::setAutoIncrease);
    connect(nm, &NotchModel::autoIncreaseChanged, m_autoIncreaseChk, [this](bool on) {
        QSignalBlocker b(m_autoIncreaseChk);
        m_autoIncreaseChk->setChecked(on);
    });
}
```

- [ ] **Step 5: Run the test, watch it pass**

Run: `cmake --build build --target tst_mnf_setup_page && ctest --test-dir build -R '^tst_mnf_setup_page$' --output-on-failure`

Expected: PASS (5 slots).

- [ ] **Step 6: Commit**

```bash
git add src/gui/setup/DspSetupPages.h src/gui/setup/DspSetupPages.cpp tests/tst_mnf_setup_page.cpp tests/CMakeLists.txt
git commit -m "feat(mnf): replace the MNF placeholder with a wired auto-increase control"
```

---

#### Cycle B: notch table and Add button

- [ ] **Step 7: Write the failing test**

Append these slots to `tests/tst_mnf_setup_page.cpp`, after `nullModel_doesNotCrash()`:

```cpp
    // ── B. Table and Add ─────────────────────────────────────────────────────

    void table_hasOneRowPerNotch()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);
        model.notchModel()->addNotch(7100000.0);

        MnfSetupPage page(&model);
        page.show();

        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->columnCount(), 4);
    }

    void table_rowEditorsCarryThetisNamesAndValues()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0, 200.0);

        MnfSetupPage page(&model);
        page.show();

        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 1);

        auto* freq   = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 0));
        auto* width  = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 1));
        auto* active = qobject_cast<QCheckBox*>(table->cellWidget(0, 2));
        auto* del    = qobject_cast<QPushButton*>(table->cellWidget(0, 3));

        QVERIFY(freq);
        QVERIFY(width);
        QVERIFY(active);
        QVERIFY(del);
        QCOMPARE(freq->objectName(),   QStringLiteral("udMNFFreq"));
        QCOMPARE(width->objectName(),  QStringLiteral("udMNFWidth"));
        QCOMPARE(active->objectName(), QStringLiteral("chkMNFActive"));
        QCOMPARE(del->objectName(),    QStringLiteral("btnMNFDelete"));

        QCOMPARE(freq->value(), 14200000.0);
        QCOMPARE(width->value(), 200.0);
        QCOMPARE(active->isChecked(), true);

        // From Thetis setup.designer.cs:44329-44338 [v2.10.3.15] , 
        // udMNFWidth.Minimum = 0, udMNFWidth.Maximum = 10000.
        QCOMPARE(width->minimum(), 0.0);
        QCOMPARE(width->maximum(), 10000.0);
    }

    void addButton_createsNotchAtVfoFrequency()
    {
        RadioModel model;
        seedSlice(model, 14074000.0);

        MnfSetupPage page(&model);
        page.show();

        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        auto* add   = page.findChild<QPushButton*>(QStringLiteral("btnMNFAdd"));
        QVERIFY(table);
        QVERIFY(add);
        QCOMPARE(table->rowCount(), 0);

        add->click();

        QCOMPARE(static_cast<int>(model.notchModel()->notches().size()), 1);
        QCOMPARE(model.notchModel()->notches().first().centerHz, 14074000.0);
        // Structural change reaches the table on the queued rebuild.
        QTRY_COMPARE(table->rowCount(), 1);
    }

    void addButton_isNoOpWithoutAnActiveSlice()
    {
        RadioModel model;
        MnfSetupPage page(&model);
        page.show();

        auto* add = page.findChild<QPushButton*>(QStringLiteral("btnMNFAdd"));
        QVERIFY(add);
        add->click();

        QCOMPARE(static_cast<int>(model.notchModel()->notches().size()), 0);
    }

    void table_followsNotchRemovedFromElsewhere()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();

        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 1);

        model.notchModel()->removeNotch(id);
        QTRY_COMPARE(table->rowCount(), 0);
    }
```

- [ ] **Step 8: Run it and watch it fail**

Run: `cmake --build build --target tst_mnf_setup_page && ctest --test-dir build -R '^tst_mnf_setup_page$' --output-on-failure`

Expected: FAIL. `table_hasOneRowPerNotch` reports `'table' returned FALSE` ,  there is no `tblMNFNotches` child yet. The other four new slots fail the same way on their `QVERIFY(table)` / `QVERIFY(add)`.

- [ ] **Step 9: Add the table, the Add button and the queued rebuild**

`src/gui/setup/DspSetupPages.h` ,  add the include (after `#include <QCheckBox>` at `:93`):

```cpp
#include <QList>
```

`src/gui/setup/DspSetupPages.h` ,  grow the `MnfSetupPage` body:

```cpp
class MnfSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit MnfSetupPage(RadioModel* model, QWidget* parent = nullptr);

private:
    // Full table rebuild. Wired to NotchModel's structural signals with
    // Qt::QueuedConnection so a rebuild never destroys the cell widget whose
    // signal is being emitted right now (the row Delete button above all).
    void rebuildTable();
    // Clear the Settings-side edit lock. NotchModel's mutators are shared with
    // the panadapter path and reject writes while adminBusy is set, so every
    // page-side write clears it first, exactly as Thetis's ENTER button does.
    void endAdminEdit();

    class QTableWidget* m_notchTable{nullptr};
    class QPushButton*  m_addBtn{nullptr};
    QCheckBox*          m_autoIncreaseChk{nullptr};

    // Table row → NotchModel notch id. The row lambdas capture the id, never
    // the row, so a reorder cannot mis-target a notch.
    QList<int> m_rowIds;
    bool m_rebuilding{false};
};
```

`src/gui/setup/DspSetupPages.cpp` ,  add to the include block:

```cpp
#include <QHeaderView>
#include <QTableWidget>
```

`src/gui/setup/DspSetupPages.cpp` ,  insert the range constants immediately above the `MnfSetupPage` ctor banner:

```cpp
// ── MNF editor ranges ─────────────────────────────────────────────────────────
// From Thetis setup.designer.cs:44334-44338 [v2.10.3.15] ,  udMNFWidth.Minimum.
static constexpr double kMnfWidthMinHz = 0.0;
// From Thetis setup.designer.cs:44329-44333 [v2.10.3.15] ,  udMNFWidth.Maximum,
// which matches console.cs:13221 [v2.10.3.15] _max_filter_width = 10000.
static constexpr double kMnfWidthMaxHz = 10000.0;
// From Thetis setup.designer.cs:44323-44327 [v2.10.3.15] ,  udMNFWidth.Increment.
static constexpr double kMnfWidthStepHz = 1.0;
// Thetis's udMNFFreq is in MHz over 0..1000000 (setup.designer.cs:44360-44369
// [v2.10.3.15]); this column is in Hz, so the editor takes the bounds
// NotchModel constrains to: the pair VfoWidget.cpp:698 clamps to, whose ceiling
// is Thetis max_freq = 61.44 (console.cs:15552 [v2.10.3.15]).
static constexpr double kMnfCentreMinHz = 100000.0;
static constexpr double kMnfCentreMaxHz = 61440000.0;
```

`src/gui/setup/DspSetupPages.cpp` ,  in the ctor, insert the table and Add button between the `NotchModel* nm = model->notchModel();` line and the auto-increase block:

```cpp
    // ── Notch table ──────────────────────────────────────────────────────────
    m_notchTable = new QTableWidget(0, 4, mnfGrp);
    m_notchTable->setObjectName(QStringLiteral("tblMNFNotches"));
    // Column captions follow Thetis lblMNFFreq / lblMNFWidth / chkMNFActive
    // (setup.designer.cs:44308, :44298, :44260 [v2.10.3.15]); the frequency
    // column is Hz here where upstream is MHz.
    m_notchTable->setHorizontalHeaderLabels({
        QStringLiteral("Center Frequency (Hz)"),
        QStringLiteral("Width (Hz)"),
        QStringLiteral("Active"),
        QString()
    });
    m_notchTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background: #131326; color: #c8d8e8; "
        "  gridline-color: #304050; border: 1px solid #304050; }"
        "QTableWidget::item { padding: 2px 4px; }"
        "QTableWidget::item:selected { background: #1a3050; }"
        "QHeaderView::section { background: #1a2030; color: #8aa8c0; "
        "  border: 1px solid #304050; padding: 4px; }"));
    m_notchTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_notchTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_notchTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_notchTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_notchTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_notchTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_notchTable->setColumnWidth(1, 110);
    m_notchTable->setColumnWidth(2, 60);
    m_notchTable->setColumnWidth(3, 80);
    m_notchTable->verticalHeader()->setVisible(false);
    m_notchTable->setMinimumHeight(160);
    mnfLay->addWidget(m_notchTable);

    // ── Add ──────────────────────────────────────────────────────────────────
    static const QString kMnfButtonStyle = QStringLiteral(
        "QPushButton { background: #203040; color: #c8d8e8; border: 1px solid #304050; "
        "  border-radius: 3px; padding: 4px 10px; }"
        "QPushButton:hover { background: #2a4060; }"
        "QPushButton:pressed { background: #1a2840; }");

    m_addBtn = new QPushButton(QStringLiteral("Add"), mnfGrp);
    m_addBtn->setObjectName(QStringLiteral("btnMNFAdd"));
    // From Thetis setup.designer.cs:44286 [v2.10.3.15] ,  btnMNFAdd tooltip.
    m_addBtn->setToolTip(QStringLiteral("Add a notch"));
    m_addBtn->setStyleSheet(kMnfButtonStyle);

    auto* addRow = new QHBoxLayout;
    addRow->setContentsMargins(0, 0, 0, 0);
    addRow->setSpacing(8);
    addRow->addWidget(m_addBtn);
    addRow->addStretch();
    mnfLay->addLayout(addRow);

    connect(m_addBtn, &QPushButton::clicked, this, [this] {
        RadioModel* rm = model();
        if (!rm || !rm->notchModel()) { return; }
        SliceModel* slice = rm->activeSlice();
        if (!slice) { return; }
        // Thetis splits this into two clicks: btnMNFAdd opens an empty row and
        // btnVFOFreq fills it from VFOA ("Enter the Frequency from VFOA",
        // setup.designer.cs:44190 [v2.10.3.15]). One gesture here, because the
        // table edits in place.
        endAdminEdit();
        rm->notchModel()->addNotch(slice->frequency());
    });
```

and append the table wiring plus the first rebuild at the end of the ctor, after the auto-increase connects:

```cpp
    // ── Wiring: notch list ↔ table ───────────────────────────────────────────
    // Structural changes go through a queued rebuild; a direct connection would
    // let removeNotch() delete the row Delete button from inside that button's
    // own clicked() emission.
    connect(nm, &NotchModel::notchAdded, this,
            [this](int) { rebuildTable(); }, Qt::QueuedConnection);
    connect(nm, &NotchModel::notchRemoved, this,
            [this](int, int) { rebuildTable(); }, Qt::QueuedConnection);
    connect(nm, &NotchModel::notchesReset, this,
            &MnfSetupPage::rebuildTable, Qt::QueuedConnection);

    rebuildTable();
```

New member functions, placed after the ctor:

```cpp
// ── MnfSetupPage::rebuildTable ────────────────────────────────────────────────

void MnfSetupPage::rebuildTable()
{
    RadioModel* rm = model();
    if (!m_notchTable || !rm || !rm->notchModel()) { return; }

    static const QString kMnfEditorStyle = QStringLiteral(
        "QDoubleSpinBox { background: #1a2030; color: #c8d8e8; "
        "  border: 1px solid #304050; border-radius: 2px; padding: 1px; }"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button "
        "  { background: #202838; width: 14px; }");
    static const QString kMnfRowButtonStyle = QStringLiteral(
        "QPushButton { background: #203040; color: #c8d8e8; border: 1px solid #304050; "
        "  border-radius: 2px; padding: 1px 6px; font-size: 11px; }"
        "QPushButton:hover { background: #2a4060; }");

    // setRowCount() destroys the outgoing cell widgets; a focused spin box
    // being destroyed emits editingFinished on its way out, so the guard has
    // to be up before the row count moves.
    m_rebuilding = true;

    // `auto` rather than the element type: NotchModel owns it and this loop
    // only reads the four fields the design fixes at §5.1.
    const auto& notches = rm->notchModel()->notches();
    const int count = static_cast<int>(notches.size());

    m_rowIds.clear();
    m_rowIds.reserve(count);
    m_notchTable->setRowCount(count);

    for (int row = 0; row < count; ++row) {
        const auto& n = notches.at(row);
        const int id = n.id;
        m_rowIds.append(id);

        // Col 0: centre frequency.
        auto* freqSpin = new QDoubleSpinBox(m_notchTable);
        freqSpin->setObjectName(QStringLiteral("udMNFFreq"));
        freqSpin->setDecimals(0);
        freqSpin->setRange(kMnfCentreMinHz, kMnfCentreMaxHz);
        freqSpin->setSingleStep(1.0);
        freqSpin->setValue(n.centerHz);
        freqSpin->setStyleSheet(kMnfEditorStyle);
        // From Thetis setup.designer.cs:44374 [v2.10.3.15] ,  udMNFFreq tooltip.
        freqSpin->setToolTip(QStringLiteral("Center frequency of the notch"));
        m_notchTable->setCellWidget(row, 0, freqSpin);

        // Col 1: width.
        auto* widthSpin = new QDoubleSpinBox(m_notchTable);
        widthSpin->setObjectName(QStringLiteral("udMNFWidth"));
        widthSpin->setDecimals(0);
        widthSpin->setRange(kMnfWidthMinHz, kMnfWidthMaxHz);
        widthSpin->setSingleStep(kMnfWidthStepHz);
        widthSpin->setValue(n.widthHz);
        widthSpin->setStyleSheet(kMnfEditorStyle);
        // From Thetis setup.designer.cs:44343 [v2.10.3.15] ,  udMNFWidth tooltip
        // (upstream spelling preserved verbatim).
        widthSpin->setToolTip(QStringLiteral("Bandwdith of the notch"));
        m_notchTable->setCellWidget(row, 1, widthSpin);

        // Col 2: active.
        auto* activeChk = new QCheckBox(m_notchTable);
        activeChk->setObjectName(QStringLiteral("chkMNFActive"));
        activeChk->setChecked(n.active);
        // From Thetis setup.designer.cs:44261 [v2.10.3.15] ,  chkMNFActive tooltip.
        activeChk->setToolTip(QStringLiteral("Checked if the notch is active"));
        m_notchTable->setCellWidget(row, 2, activeChk);

        // Col 3: delete.
        auto* delBtn = new QPushButton(QStringLiteral("Delete"), m_notchTable);
        delBtn->setObjectName(QStringLiteral("btnMNFDelete"));
        // From Thetis setup.designer.cs:44219 [v2.10.3.15] ,  btnMNFDelete tooltip.
        delBtn->setToolTip(QStringLiteral("Delete the current notch index"));
        delBtn->setStyleSheet(kMnfRowButtonStyle);
        connect(delBtn, &QPushButton::clicked, this, [this, id] {
            RadioModel* r = model();
            if (!r || !r->notchModel()) { return; }
            endAdminEdit();
            r->notchModel()->removeNotch(id);
        });
        m_notchTable->setCellWidget(row, 3, delBtn);

        m_notchTable->setRowHeight(row, 26);
    }

    m_rebuilding = false;
}

// ── MnfSetupPage::endAdminEdit ────────────────────────────────────────────────

void MnfSetupPage::endAdminEdit()
{
    RadioModel* rm = model();
    if (!rm || !rm->notchModel()) { return; }
    // Thetis clears the flag before it writes: btnMNFEnter_Click sets
    // `AddActive = false` / `EditActive = false` and only then calls WDSP
    // (setup.cs:17941-17943, :17957-17958 [v2.10.3.15]).
    rm->notchModel()->setAdminBusy(false);
}
```

- [ ] **Step 10: Run the test, watch it pass**

Run: `cmake --build build --target tst_mnf_setup_page && ctest --test-dir build -R '^tst_mnf_setup_page$' --output-on-failure`

Expected: PASS (10 slots).

- [ ] **Step 11: Commit**

```bash
git add src/gui/setup/DspSetupPages.h src/gui/setup/DspSetupPages.cpp tests/tst_mnf_setup_page.cpp
git commit -m "feat(mnf): add the MNF notch table and VFO-seeded Add button"
```

---

#### Cycle C: row edits commit, and the adminBusy edit lock

- [ ] **Step 12: Write the failing test**

Append these slots to `tests/tst_mnf_setup_page.cpp`:

```cpp
    // ── C. Row edits and the adminBusy lock ──────────────────────────────────

    void rowFrequencyEdit_commitsToModel()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0, 200.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* freq = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 0));
        QVERIFY(freq);

        freq->setValue(14200500.0);
        // QAbstractSpinBox::keyPressEvent emits editingFinished on Return.
        QTest::keyClick(freq, Qt::Key_Return);

        const auto* n = model.notchModel()->notchById(id);
        QVERIFY(n);
        QCOMPARE(n->centerHz, 14200500.0);
    }

    void rowWidthEdit_commitsToModel()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0, 200.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* width = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 1));
        QVERIFY(width);

        width->setValue(400.0);
        QTest::keyClick(width, Qt::Key_Return);

        const auto* n = model.notchModel()->notchById(id);
        QVERIFY(n);
        QCOMPARE(n->widthHz, 400.0);
    }

    void rowActiveCheckbox_commitsToModel()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* active = qobject_cast<QCheckBox*>(table->cellWidget(0, 2));
        QVERIFY(active);
        QCOMPARE(active->isChecked(), true);

        active->setChecked(false);

        const auto* n = model.notchModel()->notchById(id);
        QVERIFY(n);
        QCOMPARE(n->active, false);
    }

    void rowDeleteButton_removesNotch()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* del = qobject_cast<QPushButton*>(table->cellWidget(0, 3));
        QVERIFY(del);

        del->click();

        QCOMPARE(static_cast<int>(model.notchModel()->notches().size()), 0);
        QTRY_COMPARE(table->rowCount(), 0);
    }

    void rowValueChangedFromElsewhere_refreshesEditors()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0, 200.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* width = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 1));
        QVERIFY(width);

        // Panadapter-side wheel resize while the page is idle.
        QVERIFY(model.notchModel()->setWidth(id, 350.0));
        QCOMPARE(width->value(), 350.0);
    }

    void rowEdit_holdsAdminBusy()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* freq = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 0));
        QVERIFY(freq);
        QVERIFY(!model.notchModel()->adminBusy());

        freq->setValue(14200500.0);
        QVERIFY(model.notchModel()->adminBusy());
    }

    void rowCommit_clearsAdminBusyBeforeWriting()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* freq = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 0));
        QVERIFY(freq);

        freq->setValue(14200500.0);
        QTest::keyClick(freq, Qt::Key_Return);

        QVERIFY(!model.notchModel()->adminBusy());
        // Thetis's ENTER clears the flag first and only then writes
        // (setup.cs:17941-17950 [v2.10.3.15]); the write must land.
        QCOMPARE(model.notchModel()->notchById(id)->centerHz, 14200500.0);
    }

    void adminBusy_blocksThePanadapterPathDuringAnEdit()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);
        const int id = model.notchModel()->notches().first().id;

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);
        auto* freq = qobject_cast<QDoubleSpinBox*>(table->cellWidget(0, 0));
        QVERIFY(freq);

        freq->setValue(14200500.0);   // edit opens

        // From Thetis console.cs:40079 [v2.10.3.15]:
        //   "if (SetupForm.NotchAdminBusy) return false;"
        QCOMPARE(model.notchModel()->setCenter(id, 21000000.0), false);
        QCOMPARE(model.notchModel()->notchById(id)->centerHz, 14200000.0);
    }

    void rebuildingTheTable_doesNotOpenAnEdit()
    {
        RadioModel model;
        model.notchModel()->addNotch(14200000.0);

        MnfSetupPage page(&model);
        page.show();
        auto* table = page.findChild<QTableWidget*>(QStringLiteral("tblMNFNotches"));
        QVERIFY(table);

        // Seeding a second row re-runs rebuildTable(); the setValue() calls it
        // makes must not read as operator edits.
        model.notchModel()->addNotch(7100000.0);
        QTRY_COMPARE(table->rowCount(), 2);
        QVERIFY(!model.notchModel()->adminBusy());
    }
```

- [ ] **Step 13: Run it and watch it fail**

Run: `cmake --build build --target tst_mnf_setup_page && ctest --test-dir build -R '^tst_mnf_setup_page$' --output-on-failure`

Expected: FAIL. `rowFrequencyEdit_commitsToModel` reports `Compared floats are not the same (fuzzy compare) Actual (n->centerHz): 1.42e+07  Expected (14200500.0): 1.42005e+07` ,  the row spin boxes have no connects yet. `rowActiveCheckbox_commitsToModel`, `rowEdit_holdsAdminBusy` (`adminBusy()` still false) and `rowValueChangedFromElsewhere_refreshesEditors` fail the same way.

- [ ] **Step 14: Wire the row editors, the commit path and the edit lock**

`src/gui/setup/DspSetupPages.h` ,  add to `MnfSetupPage`'s private section, above `endAdminEdit()`:

```cpp
    // Value-only refresh of one row. Destroys nothing, so it is safe to run
    // synchronously from inside a cell widget's own signal.
    void refreshRow(int notchId);
    void commitRow(int notchId);
    // Open the Settings-side edit window. Thetis's SetupForm.NotchAdminBusy is
    // AddActive | EditActive (setup.cs:17728-17734 [v2.10.3.15]); an in-place
    // table edit is the same window, opening on the first value change.
    void beginAdminEdit();
```

`src/gui/setup/DspSetupPages.cpp` ,  in `rebuildTable()`, add the connects after each editor's `setToolTip` and before its `setCellWidget` (values are pushed before the connect, so construction cannot look like an edit):

```cpp
        connect(freqSpin, &QDoubleSpinBox::valueChanged, this,
                [this](double) { beginAdminEdit(); });
        connect(freqSpin, &QDoubleSpinBox::editingFinished, this,
                [this, id] { commitRow(id); });
```

```cpp
        connect(widthSpin, &QDoubleSpinBox::valueChanged, this,
                [this](double) { beginAdminEdit(); });
        connect(widthSpin, &QDoubleSpinBox::editingFinished, this,
                [this, id] { commitRow(id); });
```

```cpp
        connect(activeChk, &QCheckBox::toggled, this, [this, id](bool on) {
            if (m_rebuilding) { return; }
            RadioModel* r = model();
            if (!r || !r->notchModel()) { return; }
            endAdminEdit();
            r->notchModel()->setActive(id, on);
        });
```

`src/gui/setup/DspSetupPages.cpp` ,  in the ctor, add the value-change connection alongside the three structural ones:

```cpp
    // Value-only changes refresh the row in place. Direct, not queued: it
    // rewrites the existing editors instead of replacing them, so it is safe
    // even when it lands inside a spin box's own editingFinished.
    connect(nm, &NotchModel::notchChanged, this, &MnfSetupPage::refreshRow);
```

`src/gui/setup/DspSetupPages.cpp` ,  new member functions after `rebuildTable()`:

```cpp
// ── MnfSetupPage::refreshRow ──────────────────────────────────────────────────

void MnfSetupPage::refreshRow(int notchId)
{
    RadioModel* rm = model();
    if (!m_notchTable || !rm || !rm->notchModel()) { return; }

    const int row = m_rowIds.indexOf(notchId);
    if (row < 0) { return; }

    const auto* n = rm->notchModel()->notchById(notchId);
    if (!n) { return; }

    m_rebuilding = true;
    if (auto* freqSpin = qobject_cast<QDoubleSpinBox*>(m_notchTable->cellWidget(row, 0))) {
        QSignalBlocker b(freqSpin);
        freqSpin->setValue(n->centerHz);
    }
    if (auto* widthSpin = qobject_cast<QDoubleSpinBox*>(m_notchTable->cellWidget(row, 1))) {
        QSignalBlocker b(widthSpin);
        widthSpin->setValue(n->widthHz);
    }
    if (auto* activeChk = qobject_cast<QCheckBox*>(m_notchTable->cellWidget(row, 2))) {
        QSignalBlocker b(activeChk);
        activeChk->setChecked(n->active);
    }
    m_rebuilding = false;
}

// ── MnfSetupPage::commitRow ───────────────────────────────────────────────────

void MnfSetupPage::commitRow(int notchId)
{
    if (m_rebuilding) { return; }

    RadioModel* rm = model();
    if (!m_notchTable || !rm || !rm->notchModel()) { return; }

    const int row = m_rowIds.indexOf(notchId);
    if (row < 0) { return; }

    auto* freqSpin  = qobject_cast<QDoubleSpinBox*>(m_notchTable->cellWidget(row, 0));
    auto* widthSpin = qobject_cast<QDoubleSpinBox*>(m_notchTable->cellWidget(row, 1));
    if (!freqSpin || !widthSpin) { return; }

    // Lock down first, exactly as Thetis's ENTER does: btnMNFEnter_Click sets
    // AddActive / EditActive false and only then writes to WDSP
    // (setup.cs:17941-17943, :17957-17958 [v2.10.3.15]). NotchModel's mutators
    // are shared with the panadapter path and reject writes while adminBusy is
    // set (console.cs:40009, 40079 [v2.10.3.15]), so this ordering is required
    // for the page's own write to land at all.
    endAdminEdit();

    NotchModel* nm = rm->notchModel();
    nm->setCenter(notchId, freqSpin->value());
    nm->setWidth(notchId, widthSpin->value());
}

// ── MnfSetupPage::beginAdminEdit ──────────────────────────────────────────────

void MnfSetupPage::beginAdminEdit()
{
    if (m_rebuilding) { return; }
    RadioModel* rm = model();
    if (!rm || !rm->notchModel()) { return; }
    // Thetis raises the flag on btnMNFAdd / btnMNFEdit (setup.cs:17708-17726,
    // :17728-17734 [v2.10.3.15]) and every console-side notch mutator bails on
    // it (console.cs:40009, 40079, 40125, 40161, 40200, 40224, 40315
    // [v2.10.3.15]). An in-place table edit is the same window: it opens on the
    // first value change and closes when the row commits.
    rm->notchModel()->setAdminBusy(true);
}
```

- [ ] **Step 15: Run the test, watch it pass**

Run: `cmake --build build --target tst_mnf_setup_page && ctest --test-dir build -R '^tst_mnf_setup_page$' --output-on-failure`

Expected: PASS (19 slots).

- [ ] **Step 16: Commit**

```bash
git add src/gui/setup/DspSetupPages.h src/gui/setup/DspSetupPages.cpp tests/tst_mnf_setup_page.cpp
git commit -m "feat(mnf): commit MNF row edits under the NotchModel adminBusy lock"
```

---

#### Cycle D: minimum notch width readout

- [ ] **Step 17: Write the failing test**

Append these slots to `tests/tst_mnf_setup_page.cpp`:

```cpp
    // ── E. Minimum notch width ───────────────────────────────────────────────

    void minWidthLabel_showsPlaceholderWithoutAChannel()
    {
        RadioModel model;
        MnfSetupPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblMNFMinWidth"));
        QVERIFY(lbl);
        QCOMPARE(lbl->text(), QStringLiteral("--"));
    }

    void minWidthLabel_readsTheLiveChannel()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        QVERIFY(engine);
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        RxChannel* ch = engine->createRxChannel(WdspEngine::kFirstSliceChannelId,
                                                /*inputBufferSize*/ 238,
                                                /*dspBufferSize*/ 4096,
                                                /*inputSampleRate*/ 48000,
                                                /*dspSampleRate*/ 48000,
                                                /*outputSampleRate*/ 48000);
        QVERIFY(ch);

        MnfSetupPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblMNFMinWidth"));
        QVERIFY(lbl);
        QCOMPARE(lbl->text(),
                 QStringLiteral("%1 Hz").arg(ch->minNotchWidthHz(), 0, 'f', 1));
        QVERIFY(lbl->text() != QStringLiteral("--"));
    }

    void minWidthLabel_refreshesOnShow()
    {
        RadioModel model;
        MnfSetupPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblMNFMinWidth"));
        QVERIFY(lbl);

        // Nothing in the tree signals a change of RXANBPGetMinNotchWidth, so
        // the readout re-reads every time SetupDialog navigates to the page.
        lbl->setText(QStringLiteral("stale"));
        page.hide();
        page.show();
        QCOMPARE(lbl->text(), QStringLiteral("--"));
    }
```

- [ ] **Step 18: Run it and watch it fail**

Run: `cmake --build build --target tst_mnf_setup_page && ctest --test-dir build -R '^tst_mnf_setup_page$' --output-on-failure`

Expected: FAIL at compile time first: `error: 'm_initialized' is a private member of 'NereusSDR::WdspEngine'`. After the friend seam lands, the run fails with `'lbl' returned FALSE` in `minWidthLabel_showsPlaceholderWithoutAChannel` ,  there is no `lblMNFMinWidth` child yet.

- [ ] **Step 19: Add the friend seam, the readout and the show-time refresh**

`src/core/WdspEngine.h` ,  append inside the `#ifdef NEREUS_BUILD_TESTS` forward-declaration block (after `class TestWdspChannelIdMap;`, `:122`):

```cpp
// TNF step 9: the MNF Settings page test primes the engine so it can open one
// real RX channel and check the minimum-notch-width readout against it.
class TestMnfSetupPage;
```

`src/core/WdspEngine.h` ,  append inside the class's `#ifdef NEREUS_BUILD_TESTS` friend block (after `friend class ::TestWdspChannelIdMap;`, `:694`):

```cpp
    // TNF step 9: same friendship for the MnfSetupPage readout test.
    friend class ::TestMnfSetupPage;
```

`src/gui/setup/DspSetupPages.h` ,  add to `MnfSetupPage`:

```cpp
protected:
    void showEvent(QShowEvent* event) override;
```

and, in the private section:

```cpp
    void refreshMinNotchWidth();
```

plus the member, after `m_autoIncreaseChk`:

```cpp
    QLabel* m_minWidthLbl{nullptr};
```

`src/gui/setup/DspSetupPages.cpp` ,  add to the include block:

```cpp
#include "core/RxChannel.h"
#include <QShowEvent>
```

`src/gui/setup/DspSetupPages.cpp` ,  in the ctor, insert the readout between the Add-button block and the auto-increase block:

```cpp
    // ── Minimum notch width ──────────────────────────────────────────────────
    // Narrowest notch the current bandpass can realise: WDSP min_notch_width
    // (nbp.c:85-96), read through RXANBPGetMinNotchWidth (nbp.c:594). NereusSDR
    // -original control; Thetis pushes the same value out of
    // UpdateMinimumNotchWidthRX (console.cs:48787-48818 [v2.10.3.15]) to its
    // notch popup rather than to the Setup tab.
    m_minWidthLbl = new QLabel(QStringLiteral("--"), mnfGrp);
    m_minWidthLbl->setObjectName(QStringLiteral("lblMNFMinWidth"));
    m_minWidthLbl->setToolTip(QStringLiteral(
        "Narrowest notch the current bandpass filter can realise"));
    addLabeledLabel(mnfLay, QStringLiteral("Minimum Notch Width"), m_minWidthLbl);
```

and add the first read at the very end of the ctor, after `rebuildTable();`:

```cpp
    refreshMinNotchWidth();
```

`src/gui/setup/DspSetupPages.cpp` ,  new member functions after `beginAdminEdit()`:

```cpp
// ── MnfSetupPage::refreshMinNotchWidth ────────────────────────────────────────

void MnfSetupPage::refreshMinNotchWidth()
{
    if (!m_minWidthLbl) { return; }

    RadioModel* rm = model();
    WdspEngine* engine = rm ? rm->wdspEngine() : nullptr;
    if (!engine) {
        m_minWidthLbl->setText(QStringLiteral("--"));
        return;
    }

    // Thetis surfaces this per-RX (console.cs:48787-48818,
    // UpdateMinimumNotchWidthRX [v2.10.3.15]). NereusSDR's notch list is
    // global, so the readout follows the active slice's channel and falls back
    // to the first pooled channel before any slice exists.
    const int channelId = rm->activeSlice() ? rm->activeSlice()->sliceIndex()
                                            : WdspEngine::kFirstSliceChannelId;
    RxChannel* ch = engine->rxChannel(channelId);
    if (!ch) {
        m_minWidthLbl->setText(QStringLiteral("--"));
        return;
    }

    m_minWidthLbl->setText(
        QStringLiteral("%1 Hz").arg(ch->minNotchWidthHz(), 0, 'f', 1));
}

// ── MnfSetupPage::showEvent ───────────────────────────────────────────────────

void MnfSetupPage::showEvent(QShowEvent* event)
{
    SetupPage::showEvent(event);
    // min_notch_width varies with the bandpass coefficient count and the DSP
    // sample rate (nbp.c:85-96), both operator-changeable from the DSP Options
    // page, and nothing in the tree signals the change. Re-read on every visit.
    refreshMinNotchWidth();
    rebuildTable();
}
```

- [ ] **Step 20: Run the test, watch it pass**

Run: `cmake --build build --target tst_mnf_setup_page && ctest --test-dir build -R '^tst_mnf_setup_page$' --output-on-failure`

Expected: PASS (22 slots).

- [ ] **Step 21: Commit**

```bash
git add src/core/WdspEngine.h src/gui/setup/DspSetupPages.h src/gui/setup/DspSetupPages.cpp tests/tst_mnf_setup_page.cpp
git commit -m "feat(mnf): show the WDSP minimum notch width on the MNF page"
```

---

### Task 10: Visual notch: both planes, undented copy, MaxBin routing

**Files:**
- Modify: `src/models/NotchModel.h` (property block created by Task 3; add `visualEnabled` accessors + signal + member)
- Modify: `src/models/NotchModel.cpp` (add `setVisualEnabled`; add the `NotchVisualEnabled` line to `saveToSettings()` / `restoreFromSettings()`)
- Modify: `src/gui/SpectrumWidget.h:1099-1120` (test seams), `src/gui/SpectrumWidget.h:1347-1351` (private helpers), `src/gui/SpectrumWidget.h:1396-1400` (members)
- Modify: `src/gui/SpectrumWidget.cpp:1850-1871` (`processNoiseFloor`), `src/gui/SpectrumWidget.cpp:2733` (spectrum-plane dent), `src/gui/SpectrumWidget.cpp:2762` (waterfall-plane dent), `src/gui/SpectrumWidget.cpp:2909-2938` (`peakDbmInSlicePassband`)
- Modify: `src/gui/setup/DspSetupPages.h:247-251` (add `m_visualNotchChk`)
- Modify: `src/gui/setup/DspSetupPages.cpp:2110-2127` (`MnfSetupPage` ctor, filled in by Task 9; append `chkVisualNotch`)
- Modify: `src/gui/MainWindow.h:373-374` (declare `refreshPanVisualNotch`)
- Modify: `src/gui/MainWindow.cpp:1470-1478` (define it), `src/gui/MainWindow.cpp:2168-2173` (`countChanged` hook + initial push + model connect)
- Modify: `tests/CMakeLists.txt:5732-5733` (register the new test)
- Test: `tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp`

**Interfaces:**
- Consumes (Task 3, `NotchModel`): `explicit NotchModel(QObject* parent = nullptr);`, `void saveToSettings() const;`, `void restoreFromSettings();`, the file-local `QString boolStr(bool v)` helper in `NotchModel.cpp` (same shape as `SliceModel.cpp:1594`)
- Consumes (Task 4, `RadioModel`): `NotchModel* notchModel() const;`
- Consumes (Task 6, `SpectrumWidget`): `struct SpectrumWidget::NotchMarker { int id; double freqMhz; double widthHz; bool active; };`, `void setNotchMarkers(const QVector<NotchMarker>& markers);`, `void setNotchGlobalEnabled(bool on);`, `void setNotchMinWidthHz(double hz);`, members `QVector<NotchMarker> m_notchMarkers;`, `bool m_notchGlobalEnabled;`, `double m_notchMinWidthHz;`
- Consumes (Task 6, `MainWindow.cpp`): the existing `#include "models/NotchModel.h"` added for the per-pan marker fan-out
- Consumes (Task 9, `MnfSetupPage`): the filled-in ctor with `QGroupBox* mnfGrp` / `QVBoxLayout* mnfLay` in scope and the `disableGroup(mnfGrp)` NYI guard already removed
- Consumes (pre-existing): `void SpectrumWidget::setMoxOverlay(bool isTx);`, `int SpectrumWidget::hzToX(double hz, const QRect& r) const;`, `void SpectrumWidget::markOverlayDirty();`, `const QVector<float>& SpectrumWidget::renderedPixels() const;`, `const QVector<float>& SpectrumWidget::wfRenderedPixels() const;`, `double SpectrumWidget::peakDbmInSlicePassband() const;`, `QList<PanadapterApplet*> PanadapterStack::allApplets() const;`, `SpectrumWidget* PanadapterApplet::spectrumWidget() const;`
- Produces: `bool NotchModel::visualEnabled() const;`, `void NotchModel::setVisualEnabled(bool on);`, `void NotchModel::visualEnabledChanged(bool on);` (signal), AppSettings key `NotchVisualEnabled`; `void SpectrumWidget::setVisualNotchEnabled(bool on);`, `bool SpectrumWidget::visualNotchEnabled() const;`, `float SpectrumWidget::nfFftBinAverageForTest() const;`, `const QVector<float>& SpectrumWidget::activePeakHoldPeaksForTest() const;`, `const QVector<PeakBlob>& SpectrumWidget::peakBlobsForTest() const;`, `const QVector<float>& SpectrumWidget::undentedPixelsForTest() const;`, private `bool SpectrumWidget::visualNotchWillDent() const;` / `void SpectrumWidget::applyVisualNotchDent(QVector<float>& pixels) const;` / `const QVector<float>& SpectrumWidget::measurementPixels() const;`; `void MainWindow::refreshPanVisualNotch();`

---

## Cycle A: the `NotchVisualEnabled` model property

- [ ] **Step 1: Write the failing test**

Create `tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp` with the header, the fixture helpers, and the three model slots. Later cycles append slots to this same file.

```cpp
// =================================================================
// tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Upstream file
// and line references in the comments below are context for a reviewer;
// the ported logic itself lives in src/gui/SpectrumWidget.cpp and
// src/models/NotchModel.cpp, each of which carries its own verbatim
// upstream header and PROVENANCE row.
//
// TNF Task 10. Design:
//   docs/architecture/2026-07-28-tunable-notch-filter-design.md
//   section 8.3 ("Visual notch (trace dent)") and section 11.
//
// The invariant under test, asserted in BOTH directions:
//
//   * processNoiseFloor() and peakDbmInSlicePassband() read a pristine,
//     UNDENTED copy of the spectrum pixels. A display preference must
//     not silently move the noise-floor estimate or the analog S-Meter's
//     MaxBin reading. That is the NereusSDR-only hazard section 8.3
//     names: Thetis reads MaxBin from WDSP upstream of its display code,
//     so a Thetis visual notch structurally cannot move its meter, and
//     ours would.
//
//   * ActivePeakHoldTrace and PeakBlobDetector DO see the dent. That is
//     Thetis-faithful (its spectral peak hold and blob detector both
//     read the dented array, display.cs:5269 / :5280 / :5337), and it is
//     asserted positively so a later reviewer does not "fix" it into a
//     divergence.
//
// Fixture geometry, chosen so every pixel index below is exact:
//   pan centre 14.200000 MHz, span 800 Hz across 800 display pixels
//     => 1.0 Hz per display pixel,
//   tone + notch at 14.200150 MHz => display pixel 550,
//   4096 synthetic FFT bins => 5.12 bins per pixel, tone in bin 2816.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-07-29  J.J. Boyd / KG4VCF  TNF Task 10. Original test for
//                                    NereusSDR with AI-assisted
//                                    authoring via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QSignalSpy>
#include <QVector>

#include "core/AppSettings.h"
#include "gui/SpectrumWidget.h"
#include "gui/setup/DspSetupPages.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestNotchVisualDoesNotPerturbNoiseFloorOrMaxbin : public QObject
{
    Q_OBJECT

private:
    // ── Fixture geometry ─────────────────────────────────────────────────
    static constexpr double kPanCentreHz  = 14200000.0;
    static constexpr double kPanSpanHz    = 800.0;      // 1.0 Hz per pixel
    static constexpr double kToneMhz      = 14.200150;
    static constexpr int    kTonePixel    = 550;
    static constexpr int    kDisplayWidth = 800;        // headless fallback
    static constexpr int    kToneBin      = 2816;       // 4096 bins over 800 Hz

    // Synthetic linear-power FFT bins: flat floor plus one carrier, so the
    // tone is the frame's only strict local maximum and the blob detector
    // has exactly one thing to find.
    QVector<float> makeBins() const
    {
        constexpr int kFftSize = 4096;
        QVector<float> bins(kFftSize, 1e-12f);   // ~ -120 dBm floor
        bins[kToneBin] = 1e-3f;                  // ~ -30 dBm carrier
        return bins;
    }

    void configure(SpectrumWidget& w) const
    {
        w.setFrequencyRange(kPanCentreHz, kPanSpanHz);
        w.setDdcCenterFrequency(kPanCentreHz);
        w.setSampleRate(kPanSpanHz);
        w.setVfoFrequency(kPanCentreHz);
        // Passband straddles the carrier so peakDbmInSlicePassband has
        // something to find inside the 800 Hz window.
        w.setFilterOffset(50, 300);
        // Peak + no averaging keeps a single frame deterministic and puts
        // the carrier in exactly one display pixel.
        w.setSpectrumDetector(SpectrumDetector::Peak);
        w.setSpectrumAveraging(SpectrumAveraging::None);
        w.setWaterfallDetector(SpectrumDetector::Peak);
        w.setWaterfallAveraging(SpectrumAveraging::None);
        w.setNotchMinWidthHz(100.0);
        w.setNotchGlobalEnabled(true);
    }

    static QVector<SpectrumWidget::NotchMarker> oneNotch(double widthHz,
                                                         bool active = true)
    {
        SpectrumWidget::NotchMarker m;
        m.id      = 1;
        m.freqMhz = kToneMhz;
        m.widthHz = widthHz;
        m.active  = active;
        return QVector<SpectrumWidget::NotchMarker>{m};
    }

    void feed(SpectrumWidget& w, int frames) const
    {
        const QVector<float> bins = makeBins();
        for (int i = 0; i < frames; ++i) {
            // windowEnb 2.0 ~ Blackman-Harris-4; dbmOffset -10 arbitrary.
            w.updateSpectrumLinear(0, bins, 2.0, -10.0);
        }
    }

    // Width in pixels of the region where `dented` sits below `base`.
    static int dentedSpanPixels(const QVector<float>& base,
                                const QVector<float>& dented)
    {
        int lo = -1;
        int hi = -1;
        const int n = qMin(base.size(), dented.size());
        for (int i = 0; i < n; ++i) {
            if (base[i] - dented[i] > 0.01f) {
                if (lo < 0) { lo = i; }
                hi = i;
            }
        }
        return (lo < 0) ? 0 : (hi - lo + 1);
    }

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ── Cycle A: the model-side toggle ───────────────────────────────────

    void visual_enabled_defaults_off()
    {
        NotchModel notch;
        QCOMPARE(notch.visualEnabled(), false);
    }

    void visual_enabled_setter_emits_once_and_is_idempotent()
    {
        NotchModel notch;
        QSignalSpy spy(&notch, &NotchModel::visualEnabledChanged);
        notch.setVisualEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
        notch.setVisualEnabled(true);   // same value
        QCOMPARE(spy.count(), 1);
    }

    void visual_enabled_round_trips_through_app_settings()
    {
        {
            NotchModel notch;
            notch.setVisualEnabled(true);
            notch.saveToSettings();
        }
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("NotchVisualEnabled")).toString(),
                 QStringLiteral("True"));
        {
            NotchModel notch;
            notch.restoreFromSettings();
            QCOMPARE(notch.visualEnabled(), true);
        }
    }
};

QTEST_MAIN(TestNotchVisualDoesNotPerturbNoiseFloorOrMaxbin)
#include "tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.moc"
```

Register it. Insert into `tests/CMakeLists.txt` immediately after the `nereus_add_test(tst_diversity_dialog_persistence)` line (currently `:5732`), before the `Aggregate "all_tests" target` banner:

```cmake
# TNF Task 10 (visual notch): the section 8.3 measurement-routing gate.
# Asserts in both directions -- processNoiseFloor() and
# peakDbmInSlicePassband() read the pristine undented spectrum copy, while
# ActivePeakHoldTrace and PeakBlobDetector do see the dent (Thetis-faithful).
# Also covers the dent geometry (20 Hz fudge, WDSP minimum-width clamp),
# the second explicit waterfall-plane call, the MOX / master-TNF-off /
# per-notch-inactive suppression gates, and the NotchVisualEnabled
# round-trip through AppSettings.
# Source: NereusSDR-original test infrastructure.
# Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
#         section 8.3 + section 11.
nereus_add_test(tst_notch_visual_does_not_perturb_noise_floor_or_maxbin)
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: FAIL at compile time. `error: no member named 'visualEnabled' in 'NereusSDR::NotchModel'`, `no member named 'setVisualEnabled'`, and `no member named 'visualEnabledChanged'`.

- [ ] **Step 3: Add the `visualEnabled` property to `NotchModel`**

In `src/models/NotchModel.h`, next to the `globalEnabled()` / `autoIncrease()` accessors:

```cpp
    // Section 8.3 visual-notch (trace dent) toggle. It lives on the model
    // rather than on SpectrumWidget because section 5.5 keys it alongside
    // the rest of the notch state, and because a multi-pan layout has one
    // widget per pan but one notch list; MainWindow pushes this value at
    // every pan. Default off; see section 8.3 for the upstream default and
    // its cite (the panadapter-side member in SpectrumWidget carries it).
    bool visualEnabled() const { return m_visualEnabled; }
    void setVisualEnabled(bool on);
```

In the `signals:` block:

```cpp
    void visualEnabledChanged(bool on);
```

In the private members, beside `m_globalEnabled` / `m_autoIncrease`:

```cpp
    bool m_visualEnabled{false};
```

In `src/models/NotchModel.cpp`, beside `setGlobalEnabled`:

```cpp
// Mirrors setGlobalEnabled: value-equality guard, then one signal. The fan-out
// is MainWindow's (per-pan), not a WDSP write: the visual notch is a display
// approximation and touches no DSP state.
void NotchModel::setVisualEnabled(bool on)
{
    if (m_visualEnabled == on) { return; }
    m_visualEnabled = on;
    emit visualEnabledChanged(on);
}
```

In `NotchModel::saveToSettings()`, with the other flat keys:

```cpp
    s.setValue(QStringLiteral("NotchVisualEnabled"), boolStr(m_visualEnabled));
```

In `NotchModel::restoreFromSettings()`, contains-guarded like every sibling key so a missing key leaves the in-memory default alone, and routed through the public setter so the signal fires:

```cpp
    if (s.contains(QStringLiteral("NotchVisualEnabled"))) {
        setVisualEnabled(
            s.value(QStringLiteral("NotchVisualEnabled")).toString()
            == QLatin1String("True"));
    }
```

- [ ] **Step 4: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: PASS (3 slots).

- [ ] **Step 5: Commit**

```bash
git add src/models/NotchModel.h src/models/NotchModel.cpp \
        tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp \
        tests/CMakeLists.txt
git commit -m "feat(notch): NotchVisualEnabled toggle on NotchModel"
```

---

## Cycle B: the spectrum-plane dent

- [ ] **Step 6: Write the failing test**

Append four slots to `tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp`, after `visual_enabled_round_trips_through_app_settings()`:

```cpp
    // ── Cycle B: the spectrum-plane dent ─────────────────────────────────

    void widget_visual_notch_defaults_off()
    {
        SpectrumWidget w;
        QCOMPARE(w.visualNotchEnabled(), false);
    }

    void visual_notch_off_leaves_the_trace_undented()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean = base.renderedPixels();
        QCOMPARE(clean.size(), kDisplayWidth);

        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers(oneNotch(200.0));
        // Visual notch defaults off: a populated marker set on its own must
        // not touch the data the trace is drawn from.
        feed(w, 1);

        QCOMPARE(w.renderedPixels().size(), clean.size());
        QCOMPARE(dentedSpanPixels(clean, w.renderedPixels()), 0);
    }

    void visual_notch_dents_the_spectrum_trace_at_the_notch_centre()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean = base.renderedPixels();

        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers(oneNotch(200.0));
        w.setVisualNotchEnabled(true);
        feed(w, 1);

        const QVector<float>& dented = w.renderedPixels();
        // The left and right skirt loops both START at the centre pixel, so
        // it takes the full attenuation twice.
        QVERIFY(clean[kTonePixel] - dented[kTonePixel] > 150.0f);
        // Well inside the skirt.
        QVERIFY(clean[kTonePixel - 5] - dented[kTonePixel - 5] > 1.0f);
        // Well outside it (dent half-width is 110 px at 1.0 Hz per pixel).
        QCOMPARE(dented[kTonePixel - 200], clean[kTonePixel - 200]);

        // The pristine copy the measurement consumers read is NOT dented.
        QCOMPARE(w.undentedPixelsForTest().size(), clean.size());
        QCOMPARE(w.undentedPixelsForTest()[kTonePixel], clean[kTonePixel]);
    }

    void dent_span_carries_the_twenty_hz_fudge_factor()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean = base.renderedPixels();

        SpectrumWidget w;
        configure(w);                        // WDSP minimum pushed as 100 Hz
        w.setNotchMarkers(oneNotch(200.0));  // 200 > 100, so no clamp
        w.setVisualNotchEnabled(true);
        feed(w, 1);

        // 200 Hz + 20 Hz fudge = 220 Hz at 1.0 Hz per pixel. The skirt loops
        // cover [cX - wL + 1, cX + wR - 1] = 2 * 110 - 1 = 219 pixels.
        // Without the fudge it would be 199, so this band is decisive.
        const int span = dentedSpanPixels(clean, w.renderedPixels());
        QVERIFY2(span >= 217 && span <= 221,
                 qPrintable(QStringLiteral("dent span %1 px, expected ~219")
                                .arg(span)));
    }

    void dent_span_clamps_to_the_wdsp_minimum_notch_width()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean = base.renderedPixels();

        // 40 Hz notch, WDSP minimum 100 Hz -> dent is 100 + 20 = 120 Hz.
        SpectrumWidget clamped;
        configure(clamped);
        clamped.setNotchMarkers(oneNotch(40.0));
        clamped.setVisualNotchEnabled(true);
        feed(clamped, 1);
        const int clampedSpan =
            dentedSpanPixels(clean, clamped.renderedPixels());
        QVERIFY2(clampedSpan >= 117 && clampedSpan <= 121,
                 qPrintable(QStringLiteral("clamped span %1 px, expected ~119")
                                .arg(clampedSpan)));

        // Same notch with no WDSP minimum pushed -> dent is 40 + 20 = 60 Hz.
        SpectrumWidget unclamped;
        configure(unclamped);
        unclamped.setNotchMinWidthHz(0.0);
        unclamped.setNotchMarkers(oneNotch(40.0));
        unclamped.setVisualNotchEnabled(true);
        feed(unclamped, 1);
        const int unclampedSpan =
            dentedSpanPixels(clean, unclamped.renderedPixels());
        QVERIFY2(unclampedSpan >= 57 && unclampedSpan <= 61,
                 qPrintable(QStringLiteral("unclamped span %1 px, expected ~59")
                                .arg(unclampedSpan)));
    }
```

- [ ] **Step 7: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: FAIL at compile time. `error: no member named 'visualNotchEnabled' in 'NereusSDR::SpectrumWidget'`, `no member named 'setVisualNotchEnabled'`, `no member named 'undentedPixelsForTest'`.

- [ ] **Step 8: Port `modifyDataForNotches` and dent the spectrum plane**

In `src/gui/SpectrumWidget.h`, in the public block that holds the Task 6 notch push API:

```cpp
    // ── Visual notch (trace dent), design section 8.3 ────────────────────
    // Owner of the persisted state is NotchModel (key NotchVisualEnabled);
    // MainWindow::refreshPanVisualNotch pushes the same value at every pan.
    // From Thetis display.cs:1070 [v2.10.3.15]:
    //   private static bool m_bShowVisualNotch = false;
    void setVisualNotchEnabled(bool on);
    bool visualNotchEnabled() const { return m_visualNotchEnabled; }
```

Beside the existing spot test seams (`SpectrumWidget.h:1099-1120`):

```cpp
    // Visual-notch test seams (design section 8.3). Read-only views into the
    // state updateSpectrumLinear rebuilds each frame, so the section 11 test
    // can pin the measurement-routing contract without a paint cycle.
    float nfFftBinAverageForTest() const { return m_nfFftBinAverage; }
    const QVector<float>& undentedPixelsForTest() const {
        return measurementPixels();
    }
```

In the private helpers block beside `hzToX` (`SpectrumWidget.h:1347-1351`):

```cpp
    // ---- Visual notch (design section 8.3) ----
    /// True when this frame's pixels are to be dented: the toggle is on, we
    /// are not transmitting, the master TNF enable is on, and at least one
    /// marker exists. Mirrors the Thetis gate plus the _tnf_active half of
    /// its per-notch _Use flag.
    bool visualNotchWillDent() const;
    /// Subtract the notch skirts from `pixels` in place. Safe on either
    /// plane's array; the caller decides which.
    void applyVisualNotchDent(QVector<float>& pixels) const;
    /// The pristine (undented) spectrum pixels. Consumers that MEASURE
    /// rather than draw read this, never m_renderedPixels.
    const QVector<float>& measurementPixels() const;
```

In the private members beside `m_renderedPixels` (`SpectrumWidget.h:1396-1400`):

```cpp
    // ---- Visual notch state (design section 8.3) ----
    // From Thetis display.cs:1070 [v2.10.3.15]: m_bShowVisualNotch = false.
    bool m_visualNotchEnabled{false};

    // Pristine mirror of m_renderedPixels, populated ONLY on the frames that
    // actually dent so the default-off path costs nothing. Thetis keeps a
    // permanent second array instead (current_display_data_copy,
    // display.cs:5055 [v2.10.3.15]) because its analyzer hands one over for
    // free. measurementPixels() falls back to m_renderedPixels when this is
    // empty or stale-sized.
    QVector<float> m_undentedPixels;

    // From Thetis display.cs:4778 [v2.10.3.15]: float fAttenuation = 100f;
    static constexpr float kNotchDentAttenuationDb = 100.0f;

    // From Thetis display.cs:8680 [v2.10.3.15]:
    //   dNewWidth += 20; // fudge factor to align better with spectrum notch
    static constexpr double kNotchDentFudgeHz = 20.0;
```

In `src/gui/SpectrumWidget.cpp`, append after `peakDbmInSlicePassband()` (ends `:2938`):

```cpp
// ---------------------------------------------------------------------------
// Visual notch (trace dent) ,  design section 8.3
// ---------------------------------------------------------------------------
//
// Port of Thetis modifyDataForNotches (display.cs:4778-4816 [v2.10.3.15]) and
// the non-drawing arm of its handleNotches helper (display.cs:8677-8684
// [v2.10.3.15]).  Thetis works in Hz-from-the-VFO and divides every pixel
// index by its display decimation; our pixel array is absolute-RF and
// undecimated (the m_nDecimation == 1 case, see updateSpectrumLinear), so a
// pixel index here IS Thetis's xPos and hzToX is the whole coordinate map.
//
// Two terms of the upstream maths are deliberately NOT ported, per section 8.3:
//
//   * handleNotches' localRit / CTUN offset.  It compensates for Thetis's
//     VFO-label-anchored pixel maths combined with its RIT-driven DDS retune.
//     Our x axis is absolute RF, RIT never retunes the hardware, and WDSP
//     applies shift to the passband rather than to the notch, so adding it
//     would displace every dent by rit_hz.  Recorded so nobody re-adds it.
//   * cwSideToneShift, dropped entirely rather than threaded as a constant
//     zero: a notch added at F in CW stores at exactly F here.

void SpectrumWidget::setVisualNotchEnabled(bool on)
{
    if (m_visualNotchEnabled == on) { return; }
    m_visualNotchEnabled = on;
    markOverlayDirty();
}

// From Thetis display.cs:5235 [v2.10.3.15]:
//   if (bDoVisualNotch && m_bShowVisualNotch && !local_mox)
// plus the _tnf_active half of the per-notch _Use flag at display.cs:8686
// [v2.10.3.15].  The per-notch Active half is applied inside the loop below,
// exactly as upstream skips on !nc._Use.
bool SpectrumWidget::visualNotchWillDent() const
{
    return m_visualNotchEnabled
        && !m_moxOverlay
        && m_notchGlobalEnabled
        && !m_notchMarkers.isEmpty();
}

void SpectrumWidget::applyVisualNotchDent(QVector<float>& pixels) const
{
    const int n = pixels.size();
    if (n <= 0 || m_bandwidthHz <= 0.0) { return; }

    const float fAttenuation = kNotchDentAttenuationDb;

    // A rect the width of the array, so hzToX maps onto the same columns the
    // trace is drawn from regardless of the widget's live geometry.
    const QRect r(0, 0, n, 1);

    for (const NotchMarker& nc : m_notchMarkers) {
        if (!nc.active) { continue; } // skip inactive

        // From Thetis display.cs:8679-8680 [v2.10.3.15]:
        //   double dNewWidth = n.FWidth < min_notch_wdith ? min_notch_wdith : n.FWidth; // use the min width of filter from WDSP
        //   dNewWidth += 20; // fudge factor to align better with spectrum notch
        const double dNewWidth =
            ((nc.widthHz < m_notchMinWidthHz) ? m_notchMinWidthHz : nc.widthHz)
            + kNotchDentFudgeHz;

        const double centreHz = nc.freqMhz * 1e6;
        const int cX     = hzToX(centreHz, r);
        const int leftX  = hzToX(centreHz - dNewWidth / 2.0, r);
        const int rightX = hzToX(centreHz + dNewWidth / 2.0, r);

        // do left
        int wL = cX - leftX;
        wL = qMax(1, wL);
        for (int i = cX; i > cX - wL; --i) {
            if (i < 0 || i > n - 1) { continue; }
            const int x = cX - i;
            const float fTmp = 1.0f / static_cast<float>(std::pow(
                static_cast<double>(wL) / static_cast<double>(wL - x),
                1.5)); // pow2 quite sharp
            pixels[i] -= (fAttenuation * fTmp);
        }
        // do right
        int wR = rightX - cX;
        wR = qMax(1, wR);
        for (int i = cX; i < cX + wR; ++i) {
            if (i < 0 || i > n - 1) { continue; }
            const int x = i - cX;
            const float fTmp = 1.0f / static_cast<float>(std::pow(
                static_cast<double>(wR) / static_cast<double>(wR - x),
                1.5)); // pow2 quite sharp
            pixels[i] -= (fAttenuation * fTmp);
        }
    }
}

const QVector<float>& SpectrumWidget::measurementPixels() const
{
    return (m_undentedPixels.size() == m_renderedPixels.size())
               ? m_undentedPixels
               : m_renderedPixels;
}
```

In `updateSpectrumLinear`, immediately after the FFT-replan crossfade block closes (`SpectrumWidget.cpp:2733`) and before the legacy per-pixel peak hold:

```cpp
    // Visual notch, spectrum plane.  Thetis dents in place right after the
    // analyzer hands the frame over and before its per-pixel render loop
    // (display.cs:5235-5238 [v2.10.3.15]), keeping one pristine copy that
    // only the noise-floor accumulator reads (display.cs:5259 [v2.10.3.15]).
    // Peak hold, the blob / IMD detector and the max readout deliberately see
    // the dent (display.cs:5269, :5280, :5337 [v2.10.3.15]) and so are still
    // fed from m_renderedPixels below.
    if (visualNotchWillDent()) {
        m_undentedPixels = m_renderedPixels;
        applyVisualNotchDent(m_renderedPixels);
    } else if (!m_undentedPixels.isEmpty()) {
        m_undentedPixels.clear();
    }
```

- [ ] **Step 9: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: PASS (8 slots).

- [ ] **Step 10: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp \
        tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp
git commit -m "feat(spectrum): port modifyDataForNotches for the spectrum plane"
```

---

## Cycle C: the waterfall plane

- [ ] **Step 11: Write the failing test**

Append to the test file:

```cpp
    // ── Cycle C: the waterfall plane ─────────────────────────────────────

    void visual_notch_dents_the_waterfall_plane_too()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> cleanWf = base.wfRenderedPixels();
        QCOMPARE(cleanWf.size(), kDisplayWidth);

        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers(oneNotch(200.0));
        w.setVisualNotchEnabled(true);
        feed(w, 1);

        // NereusSDR keeps the waterfall pixels in their own array, so denting
        // the spectrum plane does not reach them: this is a second explicit
        // call, matching the second modifyDataForNotches call upstream.
        QCOMPARE(w.wfRenderedPixels().size(), cleanWf.size());
        QVERIFY(cleanWf[kTonePixel]
                    - w.wfRenderedPixels()[kTonePixel] > 150.0f);
        const int span = dentedSpanPixels(cleanWf, w.wfRenderedPixels());
        QVERIFY2(span >= 217 && span <= 221,
                 qPrintable(QStringLiteral("waterfall dent span %1 px, "
                                           "expected ~219").arg(span)));
    }
```

- [ ] **Step 12: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: FAIL. `visual_notch_dents_the_waterfall_plane_too()` fails at `QVERIFY(cleanWf[kTonePixel] - w.wfRenderedPixels()[kTonePixel] > 150.0f)` because the waterfall array is untouched.

- [ ] **Step 13: Add the second explicit dent call**

In `src/gui/SpectrumWidget.cpp`, immediately after the waterfall avenger's `apply(...)` call closes (`:2762`) and before the legacy per-pixel peak hold block:

```cpp
    // Visual notch, waterfall plane.  Thetis re-runs modifyDataForNotches on
    // the waterfall array after `data = current_waterfall_data`, under the
    // same MOX gate ,  display.cs:6579-6581 [v2.10.3.15].  A second explicit
    // call here because NereusSDR keeps the waterfall pixels in their own
    // array, so denting the spectrum plane above does not reach them.
    //
    // No undented waterfall copy: upstream needs one for its per-frame
    // waterfall minimum, and NereusSDR has no such tracker (m_wfLowThreshold
    // is a persisted user setting, and pushWaterfallRow is the only consumer
    // of m_wfRenderedPixels).
    if (visualNotchWillDent()) {
        applyVisualNotchDent(m_wfRenderedPixels);
    }
```

- [ ] **Step 14: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: PASS (9 slots).

- [ ] **Step 15: Commit**

```bash
git add src/gui/SpectrumWidget.cpp \
        tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp
git commit -m "feat(spectrum): dent the waterfall plane too, matching Thetis"
```

---

## Cycle D: measurement routing (noise floor, MaxBin, peak hold, blobs)

- [ ] **Step 16: Write the failing test**

Append to the test file:

```cpp
    // ── Cycle D: what reads the undented copy, and what does not ─────────

    void noise_floor_estimate_reads_the_undented_copy()
    {
        SpectrumWidget off;
        configure(off);
        off.setNotchMarkers(oneNotch(200.0));
        feed(off, 5);

        SpectrumWidget on;
        configure(on);
        on.setNotchMarkers(oneNotch(200.0));
        on.setVisualNotchEnabled(true);
        feed(on, 5);

        // Sanity: the dent really is on the trace for this frame, so the
        // comparison below is not vacuous.
        QVERIFY(off.renderedPixels()[kTonePixel]
                    - on.renderedPixels()[kTonePixel] > 150.0f);

        // A display preference must not move a measurement.
        QCOMPARE(on.nfFftBinAverageForTest(), off.nfFftBinAverageForTest());
    }

    void max_bin_passband_peak_reads_the_undented_copy()
    {
        SpectrumWidget off;
        configure(off);
        off.setNotchMarkers(oneNotch(200.0));
        feed(off, 1);

        SpectrumWidget on;
        configure(on);
        on.setNotchMarkers(oneNotch(200.0));
        on.setVisualNotchEnabled(true);
        feed(on, 1);

        const double offPeak = off.peakDbmInSlicePassband();
        const double onPeak  = on.peakDbmInSlicePassband();

        // Guard against the -400 sentinel making this pass for free.
        QVERIFY2(offPeak > -400.0,
                 "passband peak hit the sentinel; fixture geometry is wrong");
        // The analog S-Meter's MaxBin mode is fed from this. Notching a loud
        // carrier must not drop the needle.
        QCOMPARE(onPeak, offPeak);
    }

    void active_peak_hold_sees_the_dent()
    {
        SpectrumWidget off;
        configure(off);
        off.setActivePeakHoldEnabled(true);
        off.setNotchMarkers(oneNotch(200.0));
        feed(off, 1);

        SpectrumWidget on;
        configure(on);
        on.setActivePeakHoldEnabled(true);
        on.setNotchMarkers(oneNotch(200.0));
        on.setVisualNotchEnabled(true);
        feed(on, 1);

        QCOMPARE(on.activePeakHoldPeaksForTest().size(),
                 off.activePeakHoldPeaksForTest().size());
        QVERIFY(off.activePeakHoldPeaksForTest().size() > kTonePixel);

        // Thetis-faithful and deliberate: spectral peak hold reads the DENTED
        // array (display.cs:5337 [v2.10.3.15] feeds off `max`, not `max_copy`).
        // Do not "fix" this into a divergence.
        QVERIFY(off.activePeakHoldPeaksForTest()[kTonePixel]
                    - on.activePeakHoldPeaksForTest()[kTonePixel] > 150.0f);
    }

    void peak_blobs_see_the_dent()
    {
        SpectrumWidget off;
        configure(off);
        off.setPeakBlobsEnabled(true);
        off.setNotchMarkers(oneNotch(200.0));
        feed(off, 1);

        SpectrumWidget on;
        configure(on);
        on.setPeakBlobsEnabled(true);
        on.setNotchMarkers(oneNotch(200.0));
        on.setVisualNotchEnabled(true);
        feed(on, 1);

        int   offEnabled = 0;
        float offTop     = -400.0f;
        for (const PeakBlob& b : off.peakBlobsForTest()) {
            if (b.enabled) {
                ++offEnabled;
                offTop = qMax(offTop, b.max_dBm);
            }
        }
        QVERIFY2(offEnabled > 0, "undented frame produced no peak blob");

        float onTop = -400.0f;
        for (const PeakBlob& b : on.peakBlobsForTest()) {
            if (b.enabled) {
                onTop = qMax(onTop, b.max_dBm);
            }
        }

        // Also Thetis-faithful: the blob detector reads the dented array
        // (display.cs:5280 [v2.10.3.15]), so notching the only carrier in the
        // frame takes the top blob down with it.
        QVERIFY2(offTop - onTop > 50.0f,
                 qPrintable(QStringLiteral("top blob %1 dBm undented vs %2 dBm "
                                           "dented; blobs did not see the dent")
                                .arg(static_cast<double>(offTop))
                                .arg(static_cast<double>(onTop))));
    }
```

- [ ] **Step 17: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: FAIL at compile time first. `error: no member named 'activePeakHoldPeaksForTest'`, `no member named 'peakBlobsForTest'`. After those seams land, `noise_floor_estimate_reads_the_undented_copy()` and `max_bin_passband_peak_reads_the_undented_copy()` both fail their `QCOMPARE`, because both consumers still scan `m_renderedPixels`.

- [ ] **Step 18: Add the two seams and route the measurement consumers**

In `src/gui/SpectrumWidget.h`, beside the seams added in Step 8:

```cpp
    const QVector<float>& activePeakHoldPeaksForTest() const {
        return m_activePeakHold.peaks();
    }
    const QVector<PeakBlob>& peakBlobsForTest() const {
        return m_peakBlobs.blobs();
    }
```

In `src/gui/SpectrumWidget.cpp`, `processNoiseFloor()` (`:1850-1871`) reads the pristine copy. Replace the head of the function body:

```cpp
void SpectrumWidget::processNoiseFloor()
{
    // The noise floor is a MEASUREMENT, so it reads the undented pixels.
    // Upstream does the same: its accumulator takes max_copy from the
    // pristine array while everything else in that loop takes the dented
    // max ,  display.cs:5256-5259 [v2.10.3.15].
    const QVector<float>& src = measurementPixels();
    const int width = src.size();
    if (width <= 0) { return; }
```

and the accumulator loop:

```cpp
    for (int i = 0; i < width; ++i) {
        const float dB = src[i];
        if (dB < currentAverage) {
            averageSum += std::pow(10.0, static_cast<double>(dB) / 10.0);
            averageCount++;
        }
    }
```

In `peakDbmInSlicePassband()` (`:2909-2938`), replace the two `m_renderedPixels` reads with the pristine copy. Head:

```cpp
double SpectrumWidget::peakDbmInSlicePassband() const
{
    // Deliberate NereusSDR-specific divergence from the dent-in-place rule
    // (design section 8.3, decision recorded 2026-07-28): this feeds
    // WdspEngine's MaxBin detector and therefore the analog S-Meter. Thetis
    // reads MaxBin from WDSP upstream of its display code, so its visual
    // notch structurally cannot move its meter; ours would if this scanned
    // the dented array. A display preference must not change a measurement.
    const QVector<float>& src = measurementPixels();
    const int n = src.size();
    if (n < 2 || m_bandwidthHz <= 0.0) { return -400.0; }
```

and the scan:

```cpp
    float peak = -400.0f;
    for (int i = firstPx; i <= lastPx; ++i) {
        if (src[i] > peak) { peak = src[i]; }
    }
    return static_cast<double>(peak);
```

Also update the doc comment on `peakDbmInSlicePassband` in `SpectrumWidget.h:397-413`, replacing "computed from `m_renderedPixels`" with "computed from the undented spectrum pixels (`measurementPixels()`); see the visual-notch note in the definition", so the header does not contradict the body.

- [ ] **Step 19: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: PASS (13 slots).

- [ ] **Step 20: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp \
        tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp
git commit -m "fix(spectrum): route noise floor and MaxBin at the undented pixels"
```

---

## Cycle E: the suppression gates

- [ ] **Step 21: Write the failing test**

Append to the test file:

```cpp
    // ── Cycle E: suppression gates ───────────────────────────────────────

    void mox_suppresses_the_dent_on_both_planes()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean   = base.renderedPixels();
        const QVector<float> cleanWf = base.wfRenderedPixels();

        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers(oneNotch(200.0));
        w.setVisualNotchEnabled(true);
        // From Thetis display.cs:5235 [v2.10.3.15] ,  the visual notch is
        // gated on !local_mox, on both planes.
        w.setMoxOverlay(true);
        feed(w, 1);

        QCOMPARE(dentedSpanPixels(clean, w.renderedPixels()), 0);
        QCOMPARE(dentedSpanPixels(cleanWf, w.wfRenderedPixels()), 0);
    }

    void master_tnf_off_suppresses_the_dent()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean = base.renderedPixels();

        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers(oneNotch(200.0));
        w.setVisualNotchEnabled(true);
        w.setNotchGlobalEnabled(false);
        feed(w, 1);

        QCOMPARE(dentedSpanPixels(clean, w.renderedPixels()), 0);
    }

    void inactive_notch_is_not_dented()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean = base.renderedPixels();

        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers(oneNotch(200.0, /*active*/ false));
        w.setVisualNotchEnabled(true);
        feed(w, 1);

        QCOMPARE(dentedSpanPixels(clean, w.renderedPixels()), 0);
    }

    void turning_the_toggle_off_drops_the_stale_pristine_copy()
    {
        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers(oneNotch(200.0));
        w.setVisualNotchEnabled(true);
        feed(w, 1);
        QCOMPARE(w.undentedPixelsForTest().size(), kDisplayWidth);
        QVERIFY(w.undentedPixelsForTest()[kTonePixel]
                    - w.renderedPixels()[kTonePixel] > 150.0f);

        w.setVisualNotchEnabled(false);
        feed(w, 1);
        // The fallback returns the live array once the copy is dropped, so a
        // stale pristine frame can never outlive the toggle.
        QCOMPARE(w.undentedPixelsForTest()[kTonePixel],
                 w.renderedPixels()[kTonePixel]);
    }
```

- [ ] **Step 22: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: PASS for all four. `visualNotchWillDent()` already carries all three gates and `updateSpectrumLinear` already clears `m_undentedPixels`. If any of the four fails, the gate it names was dropped in Step 8 or Step 13; restore it before continuing.

- [ ] **Step 23: No implementation change; re-read the gate for drift**

Confirm `SpectrumWidget::visualNotchWillDent()` still reads exactly:

```cpp
    return m_visualNotchEnabled
        && !m_moxOverlay
        && m_notchGlobalEnabled
        && !m_notchMarkers.isEmpty();
```

and that the per-notch `if (!nc.active) { continue; } // skip inactive` is still the first statement of the loop body in `applyVisualNotchDent`. These four slots exist to pin that shape against later edits; the cycle is a characterisation cycle by design.

- [ ] **Step 24: Run the full spectrum-label subset for regressions**

Run: `cmake --build build --target tests_gui && ctest --test-dir build -L gui --output-on-failure`

Expected: PASS, including `tst_nf_aware_grid`, `tst_clarity_nf_grid_coexistence`, `tst_spectrum_pipeline_modes` and `tst_waterfall_defaults_changes` (section 11 requires the first two to still pass with the visual notch present; they are a general regression check, not a guard on the section 8.3 invariant).

- [ ] **Step 25: Commit**

```bash
git add tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp
git commit -m "test(notch): pin the MOX, master-off and per-notch dent gates"
```

---

## Cycle F: the operator-facing toggle

- [ ] **Step 26: Write the failing test**

Append to the test file:

```cpp
    // ── Cycle F: the Settings control ────────────────────────────────────

    void mnf_page_visual_notch_checkbox_binds_both_ways()
    {
        RadioModel model;
        MnfSetupPage page(&model);
        page.show();

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkVisualNotch"));
        QVERIFY2(chk, "MnfSetupPage is missing chkVisualNotch");
        QVERIFY(model.notchModel());
        QCOMPARE(chk->isChecked(), false);

        // Widget -> model.
        chk->setChecked(true);
        QCOMPARE(model.notchModel()->visualEnabled(), true);

        // Model -> widget, without an echo loop.
        QSignalSpy spy(model.notchModel(), &NotchModel::visualEnabledChanged);
        model.notchModel()->setVisualEnabled(false);
        QCOMPARE(chk->isChecked(), false);
        QCOMPARE(spy.count(), 1);
    }
```

- [ ] **Step 27: Run it and watch it fail**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: FAIL. `'MnfSetupPage is missing chkVisualNotch' returned FALSE` at the `QVERIFY2`.

- [ ] **Step 28: Add `chkVisualNotch` to the MNF page and the per-pan fan-out**

In `src/gui/setup/DspSetupPages.h`, give `MnfSetupPage` a private member block (the class currently has none):

```cpp
class MnfSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit MnfSetupPage(RadioModel* model, QWidget* parent = nullptr);

private:
    // ── Manual Notch group ───────────────────────────────────────────────
    QCheckBox* m_visualNotchChk{nullptr};
};
```

In `src/gui/setup/DspSetupPages.cpp`, at the tail of the `MnfSetupPage` ctor's Manual Notch group (`mnfLay` in scope), after the auto-increase and min-width controls Task 9 added:

```cpp
    // From Thetis setup.designer.cs:44167-44179 [v2.10.3.15] ,  chkVisualNotch,
    // the last control in grpDSPMNF. Caption and tooltip copied verbatim.
    NotchModel* notch = model->notchModel();
    if (notch) {
        m_visualNotchChk = new QCheckBox(
            "Visual approximation of notch (NOTE: this is not 100% "
            "representation of the active notch)");
        m_visualNotchChk->setObjectName(QStringLiteral("chkVisualNotch"));
        m_visualNotchChk->setWordWrap(true);
        // From Thetis setup.designer.cs:44177 [v2.10.3.15] ,  the tooltip.
        m_visualNotchChk->setToolTip(QStringLiteral(
            "This is a simple approximation and does not accurately "
            "represent the notch"));
        m_visualNotchChk->setChecked(notch->visualEnabled());
        mnfLay->addWidget(m_visualNotchChk);

        // From Thetis setup.cs:24376-24380 [v2.10.3.15]
        // chkVisualNotch_CheckedChanged sets Display.ShowVisualNotch AND
        // MiniSpec.ShowVisualNotch. NereusSDR has no mini-spectrum surface,
        // so only the panadapter half is ported.
        connect(m_visualNotchChk, &QCheckBox::toggled,
                notch, &NotchModel::setVisualEnabled);
        connect(notch, &NotchModel::visualEnabledChanged,
                m_visualNotchChk, [this](bool on) {
            QSignalBlocker b(m_visualNotchChk);
            m_visualNotchChk->setChecked(on);
        });
    }
```

Add `#include <QCheckBox>` to `src/gui/setup/DspSetupPages.h` if the header does not already carry it (the CFC page's members need it, so it normally does), and `#include "models/NotchModel.h"` to `src/gui/setup/DspSetupPages.cpp`.

In `src/gui/MainWindow.h`, immediately after `void refreshPanWideBadges();` (`:373`):

```cpp
    /// Design section 8.3: push the visual-notch (trace dent) toggle at every
    /// pan.
    ///
    /// Same shape as refreshPanWideBadges above, and for the same reason: the
    /// notch list is global but each pan converts it into its own pixel
    /// space, so the toggle has to reach pans created after startup too.
    /// Armed from PanadapterStack::countChanged.
    void refreshPanVisualNotch();
```

In `src/gui/MainWindow.cpp`, immediately after `refreshPanWideBadges()`'s closing brace (`:1478`):

```cpp
// Design section 8.3 visual-notch fan-out. See the declaration for why every
// pan is refreshed on every pass.
void MainWindow::refreshPanVisualNotch()
{
    if (!m_panStack || !m_radioModel) { return; }
    NotchModel* notch = m_radioModel->notchModel();
    if (!notch) { return; }
    const bool on = notch->visualEnabled();
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        if (SpectrumWidget* sw = applet->spectrumWidget()) {
            sw->setVisualNotchEnabled(on);
        }
    }
}
```

In the `PanadapterStack::countChanged` handler (`:2168-2173`), add the call so a pan created by a layout switch inherits the toggle:

```cpp
    connect(m_panStack, &PanadapterStack::countChanged, this, [this](int) {
        wirePanStatusOverlayTriggers();
        wirePanBadgeHandlers();
        ensureOverlayPanels();
        refreshPanStatusOverlays();
        refreshPanVisualNotch();
    });
```

And immediately after the `m_panStack->restoreSplitterState();` block (`:2202`), arm the model trigger plus the startup push:

```cpp
    // Design section 8.3: the visual-notch toggle is model state, so it has a
    // model trigger as well as the pan-count one above. Seeded once here for
    // the pans that exist at startup, since NotchModel::restoreFromSettings
    // runs in the RadioModel constructor, long before any pan does.
    if (NotchModel* notch = m_radioModel->notchModel()) {
        connect(notch, &NotchModel::visualEnabledChanged, this,
                [this](bool) { refreshPanVisualNotch(); });
    }
    refreshPanVisualNotch();
```

Per-pan fan-out has no unit-test harness in-tree: `tests/tst_mainwindow_tools_spot_hub.cpp:5-10` records the established practice of testing MainWindow logic without constructing MainWindow. It is covered by bench matrix rows 12 and 13 in `docs/architecture/2026-07-28-tnf-verification/README.md` (Task 11).

Finally, retire the third stub named in section 9: in `src/gui/SpectrumOverlayPanel.cpp:272-278`, Task 8 already replaced the disabled `"MNF"` button with the live `+TNF` button, so nothing further is owed here. Confirm that block no longer reads `makeDisabledBtn("MNF", this)` before committing.

- [ ] **Step 29: Run the test, watch it pass**

Run: `cmake --build build --target tst_notch_visual_does_not_perturb_noise_floor_or_maxbin && ctest --test-dir build -R '^tst_notch_visual_does_not_perturb_noise_floor_or_maxbin$' --output-on-failure`

Expected: PASS (18 slots).

- [ ] **Step 30: Commit**

```bash
git add src/gui/setup/DspSetupPages.h src/gui/setup/DspSetupPages.cpp \
        src/gui/MainWindow.h src/gui/MainWindow.cpp \
        tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp
git commit -m "feat(notch): chkVisualNotch on the MNF page plus per-pan fan-out"
```

---

# Appendix: reconcile pass, full output

# Cross-task contract check: tunable notch filter (tasks 1-10)

Verified against the tree where a claim was checkable (`third_party/wdsp/src/RXA.c`, `src/gui/SpectrumOverlayPanel.cpp`, `src/gui/SpectrumWidget.h/.cpp`, `tests/CMakeLists.txt`).

## 1. INTERFACE MISMATCHES

| # | Mismatch | Correction | Task to change |
|---|---|---|---|
| 1 | Task 6 consumes `NotchModel::kNotchDefaultWidthHz` / `kNotchNarrowWidthHz`. Task 3 produces `kDefaultNotchWidthHz` / `kNarrowNotchWidthHz`. | Task 6 uses `NotchModel::kDefaultNotchWidthHz` and `NotchModel::kNarrowNotchWidthHz`. Task 3's names are already referenced by its own default argument (`widthHz = kDefaultNotchWidthHz`), so Task 3 must not rename. | **6** |
| 2 | `RxChannel::m_notchAutoIncrease` default: Task 2 `{true}`, Task 4 `{false}`. | Keep `{true}`. Verified `third_party/wdsp/src/RXA.c:105` passes `1, // auto-increase notch width` into `create_nbp`. A `{false}` carry plus Task 4's unconditional reconcile would silently disable auto-increase on every channel (bench row 8). Task 4 deletes its declaration entirely (see 4.1). | **4** |
| 3 | Task 4 cites `RXA.c:103` for `create_nbp autoincr = 1`. Actual line is `:105` (`:103` is inside the same call but is not that argument). Task 4 also cites `RXA.c:86` for master run; the value line is `:87` (`:86` is the `create_notchdb(` call). Task 2 and Task 3 both cite `:87` / `:105` correctly. | Task 4 uses `RXA.c:87` and `RXA.c:105`. | **4** |
| 4 | Same executable `tst_notch_hit_test` declared with different labels: Task 6 says `gui` + `models`, Task 7 says `gui` + `core`. | Labels are auto-derived by `_nereus_derive_test_labels()` (`tests/CMakeLists.txt:186`), so both manual claims are advisory. The real defect is double registration; see 4.3. Drop the label claim from Task 7. | **7** |
| 5 | Task 8 states `m_menuBtns` is "now 7 entries, indices 0-6" without saying what was removed. Tree currently has 8 appends (`SpectrumOverlayPanel.cpp:220,228,236,244,255,263,270,277`), `+TNF` already at index 1 and the `MNF` disabled stub at index 7. | The arithmetic is correct only if Task 8 deletes the `MNF` stub at `SpectrumOverlayPanel.cpp:273-278` and keeps `+TNF` at index 1. Task 8 must say so explicitly, because Task 10 consumes that deletion. | **8** |
| 6 | Unit split: `SpectrumWidget::NotchMarker::freqMhz` is MHz (Task 6) while `NotchModel::centerHz`, all five `notch*Requested` signals, `setNotchMinWidthHz`, and Task 10's dent maths are Hz. | Not wrong, but undeclared as a conversion boundary. State in Task 6's contract that `MainWindow::refreshPanNotchMarkers` is the only Hz-to-MHz conversion site and that every signal out of `SpectrumWidget` is Hz. Tasks 7 and 10 both do `freqMhz * 1e6` arithmetic and will silently be off by 1e6 if either assumes otherwise. | **6** (declare), 7 and 10 (consume as declared) |

## 2. MISSING PRODUCERS

1. **Nobody calls `NotchModel::restoreFromSettings()` or `saveToSettings()`.** Task 3 produces both. Task 6 gap 8 *assumes* "`restoreFromSettings()` runs in the `RadioModel` constructor". Task 4 owns the ctor change (`std::unique_ptr<NotchModel> m_notchModel`) and its Produces list contains no restore call, and its gap 6 explicitly declines to add a save trigger on the assumption Task 3 wired save-on-mutate internally. Task 3's contract never says it does. Net result as written: notches never persist and never reload. Assign: Task 4 constructs and calls `restoreFromSettings()` in the ctor; Task 3 declares the save trigger (save-on-mutate inside `NotchModel`, or a named `RadioModel` hook). Task 10's visual-flag round-trip test depends on whichever is chosen.
2. **Nobody wires `RxChannel::minNotchWidthHz()` into `SpectrumWidget::setNotchMinWidthHz()`.** Task 6 produces the setter and defers the call site to "step 10"; Task 10 produces no such wiring; Task 9 reads `minNotchWidthHz()` only for the `lblMNFMinWidth` label. The widget therefore keeps its hardcoded `m_notchMinWidthHz{100.0}` forever, which feeds Task 7's edge-drag clamp and Task 10's `max(width, min)` dent span. Assign to Task 10 (it already touches both `SpectrumWidget` and `MainWindow`), or to Task 9 alongside the label refresh.
3. **Task 10 consumes the removal of `makeDisabledBtn("MNF", this)` from Task 8**, which Task 8 implies by a count but never declares. See 1.5.
4. **Task 10 consumes "the file-local `QString boolStr(bool v)` helper in `NotchModel.cpp`"**, which Task 3's Produces never mentions. Either Task 3 declares it, or Task 10 stops depending on another TU's file-local static (it is file-local, so Task 10 can only use it from inside `NotchModel.cpp`, which is fine, but the dependency must be declared).
5. **Task 8 registers no test executable.** Its Produces lists three statics, a `QAction` and object names, and its gaps describe an accelerator-collision test and a menu two-way-sync mirror test, but names no target and no `nereus_add_test` line. Add one (suggest `tst_tnf_ui_wiring`).
6. **Task 8's `+TNF` click handler is unnamed.** `SpectrumOverlayPanel::addTnfClicked(const QString&)` needs a `MainWindow` slot that resolves `sliceForPan(panId)` and calls `NotchModel::tnfAddCenterHz` + `addNotch`. Task 6 produces `MainWindow::onNotchCreateRequested(double, bool)` for the panadapter gesture; Task 8 should either reuse it or name a distinct member.
7. **Unconsumed producers (inverse problem, likely dead code):** `NotchModel::notchAddRejected(QString)`, `notchNearFreq()`, `notchesInBandwidth()`, `notchSurrounding()` (all Task 3) have no consumer in tasks 4-10. Task 7 hit-tests in pixel space via its own `notchAtPixel`, and the dedupe rejection has no UI surface, so a `+TNF` press that lands inside the 10 Hz dedupe window is silently ignored. Either wire `notchAddRejected` to a status message in Task 8 or drop the signal and the three helpers.

## 3. ORDERING PROBLEMS

No task consumes anything produced by a higher-numbered task. Three **backward file edits** exist (legal but merge-conflict prone, and each needs the earlier task to leave room):

- Task 4 edits `RxChannel`'s carries created by Task 2 (resolve by deleting Task 4's copy, see 4.1).
- Task 8 adds `static double NotchModel::tnfAddCenterHz(...)` to Task 3's file.
- Task 10 adds `visualEnabled` / `setVisualEnabled` / `visualEnabledChanged` to Task 3's file (resolve by deleting Task 10's copy, see 4.4).
- Four separate tasks (1, 2, 4, 9) each add a forward decl plus a `friend class ::TestX;` line to the same two `NEREUS_BUILD_TESTS` blocks in `src/core/WdspEngine.h:100-124` and `:674-695`. Expect textual conflicts; sequence them or land them as one combined edit.

## 4. DUPLICATED WORK

1. **`RxChannel::notchesRun()` / `notchAutoIncrease()` and both carries: Tasks 2 and 4.** Task 2 wins (it also picks the correct `{true}` default). Task 4 deletes them from its Produces and keeps only `notchAt(int, Notch&)`. Task 2 must additionally state that both carries are written **outside** `#ifdef HAVE_WDSP`, otherwise Task 4's gap-1 fixup ("moves the carry write above the guard") becomes a real edit again.
2. **`tst_notch_hit_test` registered by Task 6 and again by Task 7.** Two `add_executable()` calls with the same target name is a hard CMake configure error, not a style nit. Task 6 creates and registers the file; Task 7 appends slots only and registers nothing.
3. **The five `SpectrumWidget` notch signals and `m_selectedNotchId` / `m_hoveredNotchId`: Tasks 6 and 7 both list them as Produces.** Task 6 declares them (its `drawNotchMarkers` colour logic reads both ids); Task 7 moves them to Consumes and produces only `NotchGrab`, `m_notchGrab`, the drag-anchor members and the hit-test seams.
4. **`NotchModel::visualEnabled` / `setVisualEnabled` / `visualEnabledChanged` and the `NotchVisualEnabled` key: Tasks 3 and 10.** Task 3 wins (it already added them via its gap 2, with the same `false` default). Task 10's Cycle A collapses to Consumes; Task 10 keeps only `SpectrumWidget::setVisualNotchEnabled` and the `chkVisualNotch` binding.
5. **Bound constants triplicated.** Task 3 produces public `NotchModel::kMaxNotchWidthHz = 10000.0`, `kMinNotchCentreHz = 100000.0`, `kMaxNotchCentreHz = 61440000.0`. Task 9 re-declares `kMnfWidthMaxHz` / `kMnfCentreMinHz` / `kMnfCentreMaxHz` with identical values; Task 7 re-declares `kNotchMaxWidthHz = 10000.0`. Correction: Task 9 keeps only `kMnfWidthMinHz` (0.0) and `kMnfWidthStepHz` (1.0) and uses `NotchModel::` for the other three; Task 7 uses `NotchModel::kMaxNotchWidthHz`. `src/gui/SpectrumWidget.cpp:125` already includes `models/BandPlanManager.h`, so the gui-to-models include is established precedent and Task 7's page-local copy is unnecessary.
6. **Width clamping in two places.** Task 3 gap 5 puts the wheel edge-clamp and max-width clamp inside `NotchModel::setWidth` so Task 7's handler is a bare `setWidth(id, current + delta * step)`. Task 7 then clamps again against its own `kNotchMaxWidthHz` before emitting. Harmless but divergence-prone: drop the widget-side clamp, keep the model authoritative.
7. **Geometry computed twice.** Task 6's paint call sites pass a `specRect` computed in `paintEvent` / the GPU overlay rebuild; Task 7 introduces `notchSpecRect()` for hit testing. If they diverge, hit boxes miss drawn markers, and Task 6's `drawNotchMarkersForTest(QPainter&, QRect)` takes the rect as a parameter so no test catches it. Correction: Task 7's `notchSpecRect()` becomes the single source and Task 6's two paint sites call it.

## 5. SPEC GAPS TO ESCALATE (consolidated, deduped)

**Blocking, needs a maintainer decision:**

1. **Default values (CLAUDE.md puts these outside autonomous change).** Three defaults are unspecified by the spec and were each picked independently:
   - `NotchModel::globalEnabled` default. Task 3 chose `true` (AetherSDR `TnfModel.h:52`); Thetis `chkTNF` ships unchecked; WDSP `create_notchdb` master run is `0` (`RXA.c:87`). Task 6 hardcodes `m_notchGlobalEnabled{true}` in the widget to match. One decision, three files.
   - `NotchModel::autoIncrease` default. Not stated anywhere. Must be `true` (`RXA.c:105`) or Task 4's reconcile disables it on every channel. Confirm against `chkMNFAutoIncrease` in Thetis before Task 9 ships.
   - `visualEnabled` default `false`. Tasks 3 and 10 agree; cited to `setup.designer.cs:44167-44179` (no `Checked` assignment). Low risk, confirm only.
2. **Attribution scope of `NotchModel`.** §10.1 declares the new attribution event over exactly three upstream files (AetherSDR `TnfModel`, `radio.cs`, `console.cs`). Task 3 adds a `setup.designer.cs:44167-44179` cite for the visual-notch default; Task 8 adds `console.cs` maths (already covered); Task 10 explicitly *avoided* a `display.cs` cite on `NotchModel` for exactly this reason and moved it to `SpectrumWidget`. Under CLAUDE.md's multi-file rule, Task 3's `setup.designer.cs` cite obliges a fourth verbatim header. Decide: widen the attribution event to four files, or move that one cite to `DspSetupPages.cpp` (already `setup.cs`-registered).
3. **`adminBusy` is self-defeating as specified.** §9 has the MNF page hold `adminBusy` while editing; §5.4 has `NotchModel`'s mutators reject writes while `adminBusy` is set. The page and the panadapter share one mutator set, so the page cannot commit its own edit. Task 9 resolved it source-first (Thetis clears `AddActive` / `EditActive` before writing, `setup.cs:17941-17958`), which means the flag protects only the panadapter path during the edit window. Confirm that is the intent, or the flag should be dropped.
4. **`RXANBPGetMinNotchWidth` has no refresh trigger** (raised independently by Tasks 2, 6 and 9). Nothing signals a change to `nc` or the DSP rate; `SetupPage::syncFromModel()` has no call site in `src/`. Task 9 falls back to ctor plus a `showEvent()` override. Bench row 7 ("min-width readout changes when nc changes") depends on adding a `minNotchWidthChanged(double)` signal driven from `setFilterSizeSamples` / `setSampleRate`. Combine with missing-producer 2 above.

**UX or scope decisions taken by a task, not by the spec:**

5. Task 7 **drops** the §7 table's "right-click on empty pan -> Add notch at X MHz". NereusSDR's empty-pan right-click opens `SpectrumOverlayMenu`, a bespoke popup, not a `QMenu`; adding the item changes a shipped gesture. Ctrl + right-click covers the same capability. Restoring the item requires converting the empty-pan right-click to a `QMenu`.
6. Task 8 picks **Ctrl+Shift+N** for the TNF toggle (spec names no chord; AetherSDR passes an empty `QKeySequence`). Verified unclaimed.
7. Task 8 leaves `+TNF` **ungated on connection state** (AetherSDR gates it) on the grounds that D3 makes the notch list client-side operator state.
8. Task 9 **drops** the placeholder's "Window" combo: `grep comboMNFWindow` over Thetis v2.10.3.15 returns nothing and `grpDSPMNF` has no such control.
9. Task 6 **keeps** AetherSDR's `200`/`80` grab-handle alpha (double-encodes master-off alongside the Olive swap) and **omits** Thetis's centre notch line (`display.cs:8737`). Both are divergences a parity reviewer will notice.
10. Task 1 chose to **keep** `seedConnectFrequency` and push the hosting stream centre, out of §4.5's two mutually exclusive options ("pushes the stream's centre, **or** is dropped entirely"). Also: §4.4 says "two writers" of the shift; there are three (`activateSliceChannel` at `RadioModel.cpp:3144` runs at the tail of `bindSliceToStream`).
11. Task 5's edit to `tests/data/tci/matrix.csv` row `rx_nf_enable_set_rx0` is **mandatory, not optional**: the row pins the single-index notification that §6.4 orders removed, and `tst_tci_matrix_runner` asserts it. The generated verification README must be regenerated with it. Spec is silent.
12. Task 3 **did not port** `GetFirstNotchThatMatches` (`radio.cs:4246-4258`), listed in §5.3 with no signature and made redundant by stable ids. Combine with unconsumed-producer item 2.7 above when deciding the final `NotchModel` surface.

**Non-blocking observations:**

13. `struct Notch` placement is the one cross-task ambiguity that **did** resolve cleanly: Tasks 2 and 3 both land it in `src/core/dsp/Notch.h` (namespace `NereusSDR`), and Tasks 4, 6, 9 wrote themselves to be agnostic. No action beyond confirming Task 2 creates the file and its PROVENANCE row.
14. `scripts/gen-tci-matrix-readme.py:2` documents a stale output path (`docs/architecture/phase3j-tci-matrix/README.md`); the live file is `docs/architecture/2026-05-09-phase3j-1-tci-port-verification/README.md`. Out of scope for this epic; worth a chip.

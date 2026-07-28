# Tunable Notch Filter (TNF): Design

**Date:** 2026-07-28
**Status:** Approved, pending implementation plan
**Author:** J.J. Boyd / KG4VCF. AI-assisted design via Anthropic Claude Code.

**Upstream stamps used throughout this document:**

| Upstream | Location | Version |
| --- | --- | --- |
| Thetis | `../Thetis/` | `v2.10.3.15` (`3759d096`) |
| AetherSDR | `../AetherSDR/` | `v26.6.1-512-gc6481cbf` (`c6481cbf`) |
| WDSP | `third_party/wdsp/src/` | TAPR v1.29 (vendored) |

Inline cites in code use `[v2.10.3.15]` for Thetis and `[@c6481cbf]` for
AetherSDR, per `docs/attribution/HOW-TO-PORT.md` §Inline cite versioning.

---

## 1. Scope

A manual, operator-placed notch filter. The operator marks a frequency on
the panadapter and a narrow slot is cut out of the received audio at that
point. Notches are movable, resizable, individually bypassable, removable,
and persist across sessions.

This is distinct from ANF (Automatic Notch Filter), which already ships and
hunts carriers on its own. TNF is deliberate and operator-placed.

### 1.1 In scope

1. WDSP NBP notch API wrappers on `RxChannel`.
2. `RXANBPSetTuneFrequency` wiring (prerequisite; see §4).
3. `NotchModel`, the canonical notch store.
4. Panadapter marker rendering and full mouse interaction.
5. Visual notch (denting the displayed trace).
6. `+TNF` overlay-panel button.
7. Status-bar TNF indicator plus an assignable keyboard shortcut.
8. Settings page with a notch table, minimum width, and auto-increase.
9. TCI `rx_nf_enable` repointed from its stub onto the real master enable.
10. Persistence in AppSettings.

### 1.2 Explicitly not built, with reasons

**Notch depth (Normal / Deep / Very Deep).** Cannot be ported. WDSP's NBP
has no depth parameter; a notch is a brick-wall exclusion from the bandpass
passband and is always full depth. Thetis carries menu items for it
(`toolStripNotchNormal_Click`, `toolStripNotchDeep_Click`,
`toolStripNotchVeryDeep_Click`, `console.cs:39924-39935 [v2.10.3.15]`) but
every handler body is empty. Dead UI. AetherSDR has real depth because
SmartSDR's TNF supports it; that is a FlexRadio capability, not a WDSP one.
`TnfEntry::depthDb` is therefore dropped during the port, and AetherSDR's
depth-driven hatch spacing and triangle sizing become fixed values (§8.2).

**Permanent / temporary notches.** Same reason. This is a SmartSDR concept
(`TnfEntry::permanent`, AetherSDR `TnfModel.h:13 [@c6481cbf]`) with no WDSP
or Thetis equivalent. The nearest useful behaviour is Thetis's per-notch
`active` flag, which we do port, and which occupies the same slot in the
right-click menu.

**Per-notch sharpness.** The only WDSP knob governing notch skirt steepness
is `RXANBPSetNC` (filter coefficient count), which is a property of the
whole bandpass filter, not of an individual notch. It is already reachable
through the existing DSP Options page and is not re-exposed here.

---

## 2. Sources

Per the Two-Source Rule, with a third upstream for the Qt6 interaction shape:

| Question | Source |
| --- | --- |
| What does the DSP do? | WDSP `nbp.c` / `nbp.h` (vendored) |
| What is the radio-side behaviour? | Thetis `console.cs`, `radio.cs`, `display.cs`, `setup.cs`, `TCIServer.cs` |
| How is it structured in Qt6? | AetherSDR `TnfModel`, `SpectrumWidget`, `SpectrumOverlayMenu`, `MainWindow_Shortcuts` |

### 2.1 WDSP mechanism (read before implementing)

Each RXA channel owns its own notch database, created at channel
construction (`RXA.c:86`, `create_notchdb`). The NBP filter reaches it
through `NOTCHDB* ptraddr` (`nbp.h:66`).

Notch centres and widths are stored in **absolute RF Hz**. The mapping into
the channel's baseband passband happens in `calc_nbp_lightweight`
(`nbp.c:185-213`):

```c
offset = b->tunefreq + b->shift;
fl = a->flow  + offset;
fh = a->fhigh + offset;
a->numpb = make_nbp (b->nn, b->active, b->fcenter, b->fwidth, ...,
                     min_notch_width (a), a->autoincr, fl, fh,
                     a->bplow, a->bphigh, &a->havnotch);
for (i = 0; i < a->numpb; i++) {
    a->bplow[i]  -= offset;
    a->bphigh[i] -= offset;
}
```

Two consequences drive the whole design:

1. **`tunefreq` must be correct per channel**, or every notch lands in the
   wrong place. See §4.
2. **The notch database is index-addressed.** `RXANBPEditNotch(channel,
   notch, ...)` and `RXANBPDeleteNotch(channel, notch)` take an integer
   index into that per-channel array. Our client-side list order must stay
   in lockstep with it. See §5.2.

### 2.2 Thetis behaviour map

| Concern | Thetis location |
| --- | --- |
| Notch store (`MNotchDB`, `MNotch`) | `radio.cs:4192-4390 [v2.10.3.15]` |
| Add notch | `console.cs:40222-40280 [v2.10.3.15]` |
| Change width | `console.cs:40007-40047 [v2.10.3.15]` |
| Change centre frequency | `console.cs:40050-40118 [v2.10.3.15]` |
| Change / toggle active | `console.cs:40123-40195 [v2.10.3.15]` |
| Remove notch | `console.cs:40198-40220 [v2.10.3.15]` |
| Master TNF toggle (`TNFActive`) | `console.cs:39987-40005 [v2.10.3.15]` |
| `+TNF` equivalent (`TNFAdd`) | `console.cs:40313-40331 [v2.10.3.15]` |
| Sideband shift on add | `console.cs:40281-40308 [v2.10.3.15]` |
| Wheel-to-resize | `console.cs:33299-33321 [v2.10.3.15]` |
| Add gesture (Ctrl + middle-click) | `console.cs:49629-49646 [v2.10.3.15]` |
| Drag state | `console.cs:33284-33297 [v2.10.3.15]` |
| Marker colours | `display.cs:383-411 [v2.10.3.15]` |
| Visual notch (trace dent) | `display.cs:4733-4790 [v2.10.3.15]` |
| Visual-notch toggle | `display.cs:1070-1075 [v2.10.3.15]` |
| Undented-copy discipline | `display.cs:5046, 5095, 6505, 6530, 6741 [v2.10.3.15]` |
| Persistence format | `console.cs:3034-3035, 4763-4764 [v2.10.3.15]` |
| TCI `rx_nf_enable` | `TCIServer.cs:3384-3400, 1954-1960 [v2.10.3.15]` |

### 2.3 AetherSDR shape map

| Concern | AetherSDR location |
| --- | --- |
| Model shape | `src/models/TnfModel.h`, `.cpp` `[@c6481cbf]` |
| Marker struct + push API | `src/gui/SpectrumWidget.h:575-583 [@c6481cbf]` |
| Marker rendering | `SpectrumWidget::drawTnfMarkers [@c6481cbf]` |
| Hit test / drag state | `SpectrumWidget.h:789-793, 1647-1653 [@c6481cbf]` |
| Hover popup | `SpectrumWidget.h:805, 1653 [@c6481cbf]` |
| Context menus | `SpectrumWidget.cpp:8560-8590 [@c6481cbf]` |
| `+TNF` button | `SpectrumOverlayMenu.cpp:293, 324 [@c6481cbf]` |
| Status-bar indicator | `MainWindow_Shortcuts.cpp:543-548, 612-614 [@c6481cbf]` |
| Assignable shortcut | `MainWindow_Shortcuts.cpp:1093-1099 [@c6481cbf]` |

---

## 3. Decisions taken

| # | Decision | Rationale |
| --- | --- | --- |
| D1 | **One shared notch list across all slices.** | Thetis parity (`MNotchDB` is a single static list, `radio.cs:4192`). Notch centres are absolute RF Hz, so a 20 m notch is inherently inert on a 40 m slice with no special-casing. Set once, stays put, works on whichever slice later tunes there. |
| D2 | **Alt / Option + click creates a notch.** Shift+Alt+click creates it narrow (100 Hz). | Thetis's Ctrl + middle-click (`console.cs:49629`) is unusable for us: middle-click barely exists on a Mac trackpad, and macOS maps Ctrl+click to right-click. Ctrl is already bound to wheel-zoom on our panadapter. Alt is unused there and behaves identically across platforms. Keeping Shift as the modifier preserves Thetis's narrow-notch shortcut (`console.cs:40268-40269`). |
| D3 | **Persist globally, not per-MAC.** | A notch tracks a QRM source at the operator's location and band, not a property of the radio. Thetis also persists globally. Diverges from the per-MAC convention used for hardware state, deliberately. |
| D4 | **Stable ids in the model, list order as the WDSP index.** | AetherSDR uses id-keyed entries (`QMap<int, TnfEntry>`), which is what the UI needs for drag and hit-test across mutations. WDSP needs positional indices. We carry both: a stable `id` field for the UI, with list position as the WDSP index. |
| D5 | **Drop depth and permanent.** | See §1.2. |
| D6 | **`+TNF` adds at VFO shifted into the middle of the passband.** | Thetis `notchSidebandShift` (`console.cs:40281-40308 [v2.10.3.15]`), including its symmetric-filter (AM) fallback where `middle == 0` becomes `highHz / 2`. |
| D7 | **Settings page lives at Settings → DSP → Notches.** | Groups with the other WDSP receive-chain pages. |

---

## 4. Prerequisite: `RXANBPSetTuneFrequency` is never called

**This is a live defect in the current codebase, not new work.**

`wdsp_api.h:337` declares `RXANBPSetFreqs` and `:357` declares
`RXANBPSetShiftFrequency`; both are called (`RxChannel.cpp:425`,
`RxChannel.cpp:1397`). `RXANBPSetTuneFrequency` is declared nowhere and
called nowhere. Every channel's `notchdb.tunefreq` therefore sits at its
construction default.

Because `calc_nbp_lightweight` computes `offset = tunefreq + shift`
(§2.1), a zero `tunefreq` means any notch we add would be mapped to a
baseband position derived from the wrong RF origin. The feature cannot
work until this is fixed.

Thetis pushes it on every retune:

```csharp
// console.cs:31940-31941 [v2.10.3.15]
WDSP.RXANBPSetTuneFrequency(WDSP.id(0, 0), (RX1DDSFreq + f_LO) * 1.0e6);
WDSP.RXANBPSetTuneFrequency(WDSP.id(0, 1), (RX1DDSFreq + f_LO) * 1.0e6);
// console.cs:32926 [v2.10.3.15]
WDSP.RXANBPSetTuneFrequency(WDSP.id(2, 0), (RX2DDSFreq + f_LO) * 1.0e6);
```

**Implementation:** `RxChannel::setNotchTuneFrequency(double absoluteHz)`
called from the same code path that already issues
`RXANBPSetShiftFrequency` at `RxChannel.cpp:1397`, driven per slice.
This lands as the first task, with a test that asserts the pushed value
tracks slice frequency, before any notch UI exists.

---

## 5. `NotchModel`

**New file:** `src/models/NotchModel.{h,cpp}`
**Owner:** `RadioModel`, alongside `SpotModel`
**Attribution:** new attribution event (§10)

### 5.1 Data

```cpp
struct Notch {
    int    id{0};          // stable, monotonic; UI hit-test and drag key
    double centerHz{0.0};  // absolute RF Hz
    double widthHz{200.0}; // Hz
    bool   active{true};   // per-notch bypass
};
```

`centerHz` / `widthHz` / `active` mirror Thetis `MNotch`
(`radio.cs:4328-4360 [v2.10.3.15]`) field for field, minus its
`MHz|Hz|active:` string format. `id` is the AetherSDR addition
(`TnfEntry::id`, `TnfModel.h:9 [@c6481cbf]`).

### 5.2 The index invariant

`m_notches` is an ordered `QList<Notch>`. **Its position is the WDSP notch
index.** Every mutation keeps the two in lockstep:

- **Add** → append at position `n`; call `RXANBPAddNotch(ch, n, ...)` on
  every channel. Thetis reads the count back from WDSP first
  (`RXANBPGetNumNotches`, `console.cs:40262-40266`); we assert our list
  size matches it instead, which catches divergence rather than papering
  over it.
- **Edit** → `RXANBPEditNotch(ch, indexOf(id), ...)`.
- **Delete** → `RXANBPDeleteNotch(ch, indexOf(id))`, then erase from the
  list. WDSP shifts its array down internally; our list does the same, so
  positions stay aligned.

Thetis has a known wart here: because its notch identity *is* the index,
every mutation loses the operator's selection, and it recovers by searching
for a notch whose fields match (`GetFirstNotchThatMatches`,
`console.cs:40036, 40110, 40149, 40188`). One of those recovery calls was
itself a bug, fixed in `[2.9.0.7]` by MW0LGE
(`console.cs:40109 [v2.10.3.15]`). Stable ids remove that entire
class of problem without altering any WDSP-facing behaviour.

### 5.3 API

```cpp
// Queries
const QList<Notch>& notches() const;
const Notch*        notchById(int id) const;
int                 indexOfId(int id) const;
bool                globalEnabled() const;

// Thetis-ported spatial helpers (radio.cs:4250-4300 [v2.10.3.15])
bool          notchNearFreq(double hz, int deltaHz) const;
QList<Notch>  notchesInBandwidth(double centreHz, int lowHz, int highHz) const;
const Notch*  notchSurrounding(double centreHz, int lowHz, int highHz,
                               double hz, int padWidthHz = 0) const;

// Mutations
int  addNotch(double centerHz, double widthHz = 200.0);   // -1 if rejected
bool setCenter(int id, double centerHz);
bool setWidth(int id, double widthHz);
bool setActive(int id, bool active);
bool removeNotch(int id);
void setGlobalEnabled(bool on);
void clear();

// Settings-page edit lock (Thetis NotchAdminBusy, console.cs:40009 [v2.10.3.15])
void setAdminBusy(bool busy);
bool adminBusy() const;

// Persistence
void saveToSettings() const;
void restoreFromSettings();

signals:
    void notchAdded(int id);
    void notchChanged(int id);
    void notchRemoved(int id, int formerIndex);
    void globalEnabledChanged(bool on);
    void notchAddRejected(const QString& reason);
```

`notchRemoved` carries `formerIndex` because the WDSP fan-out needs the
positional index the entry occupied, and it is gone by the time the signal
lands.

### 5.4 Ported guards (verbatim behaviour)

| Guard | Value | Thetis source |
| --- | --- | --- |
| Reject add if a notch is already within N Hz | 10 Hz | `console.cs:40260 [v2.10.3.15]` |
| Default new-notch width | 200 Hz | `console.cs:40268 [v2.10.3.15]` |
| Narrow (Shift held) width | 100 Hz | `console.cs:40269 [v2.10.3.15]` |
| Round centre to whole Hz on add | `Math.Round` | `console.cs:40232 [v2.10.3.15]` |
| Round centre to whole Hz on move | `Math.Round` | `console.cs:40081 [v2.10.3.15]` |
| Constrain centre to radio min/max frequency | min_freq..max_freq | `console.cs:40257, 40077 [v2.10.3.15]` |
| Reject edit while Settings page is mid-edit | `NotchAdminBusy` | `console.cs:40009, 40079, 40123, 40200 [v2.10.3.15]` |
| Wheel resize must keep both edges inside 0..max | n/a | `console.cs:33317-33318 [v2.10.3.15]` |

Constants get named `constexpr` with the Thetis origin recorded, e.g.

```cpp
// From Thetis console.cs:40260 [v2.10.3.15]: "if there is a notch within
// 10hz ignore"
static constexpr int kNotchDedupeWindowHz = 10;
```

Thetis's XVTR-aware min/max override (`console.cs:40052-40077`) is ported
in structure; the XVTR band lookup routes through our existing
`Band::XVTR` handling rather than Thetis's `XVTRForm`.

### 5.5 Persistence

AppSettings, global scope (D3). Keys:

```
NotchGlobalEnabled   True | False
NotchCount           <n>
Notch<i>Center       <Hz, 6dp>
Notch<i>Width        <Hz>
Notch<i>Active       True | False
NotchVisualEnabled   True | False    (§8.3 toggle)
NotchAutoIncrease    True | False    (§9)
```

Written flat rather than in Thetis's packed
`mnotchdb[i]/14.074 MHz| 200 Hz| active: True` string
(`console.cs:3034-3035, 4763-4764 [v2.10.3.15]`), because Thetis's format
exists to fit its key/value database and round-trips through
locale-sensitive `double.Parse`. Ours is an XML document already; flat
keys avoid the parse entirely.

Restore order matters: `restoreFromSettings()` populates the model, then
the model fans out to whatever channels exist. On a cold start no channels
exist yet, so `RadioModel` re-syncs the full list into each channel at
channel-activation time (§6).

---

## 6. `RxChannel` and `RadioModel` fan-out

### 6.1 New `wdsp_api.h` declarations

Ten, matching `nbp.c` signatures exactly:

```cpp
int  RXANBPAddNotch      (int channel, int notch, double fcenter,
                          double fwidth, int active);           // nbp.c:362
int  RXANBPGetNotch      (int channel, int notch, double* fcenter,
                          double* fwidth, int* active);         // nbp.c:393
int  RXANBPEditNotch     (int channel, int notch, double fcenter,
                          double fwidth, int active);           // nbp.c:444
int  RXANBPDeleteNotch   (int channel, int notch);              // nbp.c:418
void RXANBPGetNumNotches (int channel, int* nnotches);          // nbp.c:465
void RXANBPSetTuneFrequency (int channel, double tunefreq);     // nbp.c:475
void RXANBPSetNotchesRun (int channel, int run);                // nbp.c:499
void RXANBPGetMinNotchWidth (int channel, double* minwidth);    // nbp.c:594
void RXANBPSetAutoIncrease  (int channel, int autoincr);        // nbp.c:604
void RXANBPSetNC            (int channel, int nc);              // nbp.c:567
```

### 6.2 `RxChannel` additions

```cpp
void setNotchTuneFrequency(double absoluteHz);   // §4
void syncNotches(const QList<Notch>& notches);   // full rebuild
void addNotch   (int index, const Notch& n);
void editNotch  (int index, const Notch& n);
void deleteNotch(int index);
void setNotchesRun(bool run);
void setNotchAutoIncrease(bool on);
double minNotchWidthHz() const;
```

`syncNotches` deletes every existing notch and re-adds the list in order.
Used on channel activation and after `restoreFromSettings`. The
incremental calls are used for live edits so a single drag does not rebuild
the whole filter.

### 6.3 Fan-out

`RadioModel` connects `NotchModel`'s signals and pushes to every live
`RxChannel` obtained by iterating `slices()`. Thetis fans to three fixed
ids (`WDSP.id(0,0)`, `WDSP.id(0,1)`, `WDSP.id(2,0)`, e.g.
`console.cs:40269-40271 [v2.10.3.15]`); we iterate because slice count is
dynamic post-3F.

On `activateSliceChannel`, the new channel gets `syncNotches(...)` plus
`setNotchesRun(globalEnabled())` plus the current
`setNotchTuneFrequency(...)`, so a slice added later inherits the full
notch set.

### 6.4 TCI

`RadioModel::rxNf(int)` / `setRxNf(int, bool)` currently read and write
`m_tciStubRxNf[]` (`RadioModel.cpp:11412-11420`) and nothing consumes
them. They are repointed at `NotchModel::globalEnabled` /
`setGlobalEnabled`. The `rx_nf_enable` command handler
(`TciProtocol.cpp:2154-2180`) already implements the wire format and needs
no change.

This matches Thetis exactly: `handleRxNfEnable` queries `GetMNF(rx+1)` and
sets `TNFActive` (`TCIServer.cs:3384-3400 [v2.10.3.15]`), where
`TNFActive` is the same master flag that drives `RXANBPSetNotchesRun`
(`console.cs:40000-40002 [v2.10.3.15]`).

Note the arity divergence: TCI addresses receivers as `rx 0|1` while our
notch enable is global (D1). Set on either index sets the global flag;
query on either returns it. This is what Thetis does too, since its
`TNFActive` is likewise global despite the per-rx command shape.

---

## 7. Interaction model

| Gesture | Result | Source |
| --- | --- | --- |
| Alt + click on panadapter | Add 200 Hz notch at clicked frequency | D2 |
| Shift + Alt + click | Add 100 Hz notch | `console.cs:40269 [v2.10.3.15]` |
| Drag triangle handle | Move notch centre | `console.cs:33286 [v2.10.3.15]`, AetherSDR `m_draggingTnfId [@c6481cbf]` |
| Drag either edge | Resize notch | `console.cs:33287-33288 [v2.10.3.15]` |
| Wheel over notch | Resize notch | `console.cs:33299-33321 [v2.10.3.15]` |
| Hover over notch | Popup with frequency + width | AetherSDR `m_tnfHoverPopup [@c6481cbf]` |
| Right-click on notch | Width presets, Active/Bypass, Remove | AetherSDR `SpectrumWidget.cpp:8560-8580 [@c6481cbf]` |
| Right-click on empty pan | "Add notch at X MHz" | AetherSDR `SpectrumWidget.cpp:8585 [@c6481cbf]` |
| `+TNF` button | Add at VFO + sideband shift | `console.cs:40313-40331 [v2.10.3.15]` |
| Click status-bar TNF light | Toggle all notches | AetherSDR `MainWindow_Shortcuts.cpp:612-614 [@c6481cbf]` |
| Bound shortcut key | Toggle all notches | AetherSDR `MainWindow_Shortcuts.cpp:1093 [@c6481cbf]` |

Alt+click is checked before the existing click-to-tune path in
`mousePressEvent`, so tuning behaviour is unchanged when Alt is not held.
No existing gesture uses Alt.

---

## 8. `SpectrumWidget`

### 8.1 Push API

Mirrors the existing spot-overlay pattern
(`SpectrumWidget.h:1020-1082`):

```cpp
struct NotchMarker {
    int    id;
    double freqMhz;
    double widthHz;
    bool   active;
};
void setNotchMarkers(const QVector<NotchMarker>& markers);
void setNotchGlobalEnabled(bool on);

signals:
    void notchCreateRequested(double freqHz, bool narrow);
    void notchMoveRequested(int id, double newFreqHz);
    void notchWidthRequested(int id, double widthHz);
    void notchActiveRequested(int id, bool active);
    void notchRemoveRequested(int id);
```

`NotchMarker` is AetherSDR's `TnfMarker` (`SpectrumWidget.h:575
[@c6481cbf]`) with `depthDb` and `permanent` replaced by `active` (§1.2).

### 8.2 Rendering

`drawNotchMarkers()` ports AetherSDR `drawTnfMarkers` `[@c6481cbf]`
geometry unchanged:

- translucent fill spanning the full spectrum height
- diagonal hatch clipped to the notch rect
- 1 px edge lines at both boundaries
- a downward triangle grab handle at the top, `±5 px` wide

Two changes, both forced by §1.2:

| AetherSDR | Ours | Why |
| --- | --- | --- |
| `hatchSpacing = depthDb <= 1 ? 12 : (depthDb == 2 ? 8 : 5)` | fixed 8 px | no depth in WDSP |
| `triH = 8 + depthDb * 2` | fixed 10 px | no depth in WDSP |

Colours come from Thetis rather than AetherSDR, because AetherSDR's
green/yellow encodes permanent-vs-temporary, which we do not have.
From `display.cs:383-411 [v2.10.3.15]`:

| State | Colour | Thetis name |
| --- | --- | --- |
| Active | Yellow | `notch_active_colour` |
| Bypassed | Gray | `notch_inactive_colour` |
| Hovered / selected | Chartreuse | `notch_highlight_color` |
| Master TNF off | Olive | `notch_tnf_off_colour` |

Fills use Thetis's alpha 92 (`changeAlpha(..., 92)`, `display.cs:400-408`).

Marker drawing hooks the existing overlay-cache invalidation path
(`m_overlayStaticDirty`, `SpectrumWidget.h:1889-1925`) so a static notch
set costs nothing per frame.

### 8.3 Visual notch (trace dent)

Ports Thetis `modifyDataForNotches` (`display.cs:4733-4790 [v2.10.3.15]`)
and its `handleNotches` helper (`display.cs:4782`).

**The critical invariant to preserve.** Thetis deliberately keeps an
undented copy of the display data and reads *that* for noise-floor
averaging and waterfall-minimum tracking:

```csharp
// display.cs:5046 [v2.10.3.15]
// make copy of the data so visual notch does not change the average noise floor
...
// display.cs:6741 [v2.10.3.15]
if (waterfall_minimum > dataCopy[i] + fOffset) //[2.10.3]MW0LGE use non notched data
```

Without this, adding a notch drags the computed noise floor down and the
grid, the NF-aware grid feature, and the waterfall black level all shift.
NereusSDR has `tst_nf_aware_grid` and `tst_clarity_nf_grid_coexistence`
guarding exactly that machinery, so this gets its own regression test
(§11).

Gated by a toggle, default **off**, matching Thetis
`m_bShowVisualNotch = false` (`display.cs:1070 [v2.10.3.15]`). Persisted
as `NotchVisualEnabled`. Suppressed during MOX, as Thetis does
(`display.cs:5235`, `!local_mox`).

---

## 9. Settings → DSP → Notches

Thetis-parity page:

- **Table**: one row per notch, frequency (Hz, editable), width (Hz,
  editable), active (checkbox), delete button.
- **Add**: creates a notch at the current VFO frequency, editable in place.
- **Minimum notch width**: read-only readout from
  `RXANBPGetMinNotchWidth` (`nbp.c:594`). This is the narrowest notch the
  current filter can actually realise; it varies with `nc` and sample rate.
  Thetis surfaces it per-RX and per-TX
  (`console.cs:39052-39054, UpdateMinimumNotchWidthRX/TX [v2.10.3.15]`).
- **Auto-increase**: `RXANBPSetAutoIncrease` (`nbp.c:604`). When on, a
  notch narrower than the achievable minimum is widened rather than
  silently under-delivering (`nbp.c:122`, `if (autoincr && width[k] <
  minwidth)`). Persisted as `NotchAutoIncrease`.

While this page has an edit in progress it holds `NotchModel::adminBusy`,
which makes every panadapter-side mutation a no-op. This is Thetis's
`SetupForm.NotchAdminBusy` guard, checked at the top of every mutator
(`console.cs:40009, 40079, 40123, 40164, 40200, 40224 [v2.10.3.15]`).
Without it a drag on the panadapter can reorder the list underneath the
table's index mapping.

---

## 10. Attribution

### 10.1 New attribution event

`src/models/NotchModel.h` and `.cpp` are new files carrying ported logic
from two upstreams. Both need, **in the commit that introduces them**:

1. The AetherSDR `TnfModel` attribution block (GPL-3.0-or-later,
   `@c6481cbf`), in the form used by `src/models/SpotModel.h`.
2. The Thetis `radio.cs` header byte-for-byte, since `MNotch` /
   `MNotchDB` come from there.
3. `// --- From <filename> ---` separators between them, per CLAUDE.md
   multi-file attribution.
4. A `Modification history (NereusSDR)` block.
5. Rows in `docs/attribution/THETIS-PROVENANCE.md` and the AetherSDR
   reconciliation index.

### 10.2 Existing files

All other files touched are already registered in
`THETIS-PROVENANCE.md`: `wdsp_api.h`, `RxChannel.{h,cpp}`,
`RadioModel.{h,cpp}`, `SpectrumWidget.{h,cpp}`,
`SpectrumOverlayPanel.cpp`. New ported logic in them still needs inline
`// From <Upstream> <file>:<line> [<stamp>]` cites.

### 10.3 Inline comment preservation

Ship-blocking per CLAUDE.md. Author tags inside ported ranges must survive
verbatim. Known tags in the ranges this work touches:

| Tag | Location |
| --- | --- |
| `//MW0LGE_21k8` | `radio.cs:4244` (notch list lock) |
| `//MW0LGE return a notch that matches` | `radio.cs:4245` |
| `//MW0LGE check if notch close by` | `radio.cs:4260` |
| `//MW0LGE return list of notches in given bandwidth` | `radio.cs:4274` |
| `//MW0LGE return first notch found that surrounds a given frequency...` | `radio.cs:4296` |
| `//MW0LGE_21e XVTR` | `console.cs:40052`, `console.cs:40232` |
| `//MW0LGE_21e` | `console.cs:33289` |
| `//MW0LGE [2.9.0.7] fix old bug...` | `console.cs:40109` |
| `//[2.10.3.7]MW0LGE moved from below` | `console.cs:40230` |
| `//MW0LGE_21k9rc4` | `console.cs:40329` |
| `//[2.10.3]MW0LGE use non notched data` | `display.cs:6741, 6833, 6915` |

`scripts/verify-inline-tag-preservation.py` runs pre-commit and in CI and
will reject a dropped tag.

---

## 11. Testing

TDD per task. New test executables:

| Test | Covers |
| --- | --- |
| `tst_notch_tune_frequency` | §4. Slice retune pushes the correct absolute Hz. Lands first, before any notch exists. |
| `tst_notch_model_guards` | §5.4. 10 Hz dedupe, 200/100 Hz defaults, whole-Hz rounding, min/max clamp, admin-busy rejection, wheel-resize edge clamp. |
| `tst_notch_model_index_invariant` | §5.2. List position tracks the WDSP index across add / edit / delete, including deleting from the middle. |
| `tst_notch_persistence` | §5.5. Round-trip through AppSettings including global enable, visual toggle, auto-increase. |
| `tst_notch_channel_sync` | §6.3. A slice activated after notches exist inherits the full set, the run flag, and the tune frequency. |
| `tst_notch_hit_test` | §7. Pixel-to-notch hit test, edge-vs-centre discrimination, off-screen rejection. |
| `tst_notch_visual_noise_floor` | §8.3. **The dented array and the noise-floor array are independent.** Adding a notch must not move the computed noise floor. |
| `tst_notch_tci_rx_nf_enable` | §6.4. `rx_nf_enable` set and query round-trip against the real master enable, both rx indices. |

`tst_nf_aware_grid` and `tst_clarity_nf_grid_coexistence` must both still
pass with visual notch enabled.

---

## 12. Build order

1. §4 tune-frequency fix, with `tst_notch_tune_frequency`. Nothing else
   works until this lands.
2. `wdsp_api.h` declarations plus `RxChannel` wrappers.
3. `NotchModel` with guards and persistence.
4. `RadioModel` fan-out plus channel-activation sync.
5. TCI repoint (§6.4). Small, and makes the model observable from
   outside before any UI exists.
6. `SpectrumWidget` marker rendering.
7. `SpectrumWidget` interaction: Alt+click, drag, edge-drag, wheel, hover,
   both context menus.
8. `+TNF` button and status-bar indicator plus shortcut.
9. Settings page with min-width and auto-increase.
10. Visual notch, last, because it is the only piece that can perturb
    existing display behaviour and wants the rest stable underneath it.

---

## 13. Base

Implemented on `claude/tunable-notch-filter-d52e29`, which is
`main` + PR #306 + PR #291 + PR #293 + the four multi-pan audio bench-fix
commits from `claude/angry-maxwell-3a8f5b`. Verified building at
integration time. TNF therefore assumes multi-slice is present and cannot
merge to `main` ahead of #293.

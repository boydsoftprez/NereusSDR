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
7. Status-bar TNF indicator plus a fixed-accelerator menu action (an
   *assignable* shortcut needs a subsystem that does not exist; see
   §10.2).
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

**The Thetis CW-pitch correction. Deliberately NOT ported.** This is the
most dangerous omission to leave implicit, because §2.2 hands the
implementer `console.cs:40222-40280`, and the third statement of that
function is:

```csharp
// console.cs:40228 [v2.10.3.15]
fFreqHZ += GetDSPcwPitchShiftToZero(sourceRX);
```

which returns `+CWPitch` in CWL and `-CWPitch` in CWU
(`console.cs:18187-18194`). Thetis needs it because it folds the pitch
into `RX1DDSFreq` (`console.cs:31901-31910`), which is the exact value it
feeds to `RXANBPSetTuneFrequency` (`console.cs:31940`).

NereusSDR does the opposite: the CW pitch lives in the filter passband
(`SliceModel.cpp:1279-1284`) and no CW term appears on any
slice/stream/DDC path. Porting the correction would drag a CWU notch to
baseband 0, outside the passband.

It also interacts with D6. In Thetis the two terms cancel: `TNFAdd` sends
`VFOA + middle` where `middle` is `+cw_pitch` in CWU
(`console.cs:40329-40331`), and `AddNotch`'s `-cw_pitch` brings it back to
VFOA. In NereusSDR, `notchSidebandShift` alone is already correct.

The same applies to `getCWSideToneShift` (`display.cs:8617-8622`, applied
at `:8654`) in the visual-notch path: drop the parameter rather than
threading a constant zero (§8.3).

`tst_notch_model_guards` asserts a notch added at F in CWU and in CWL is
stored at exactly F.

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

Three consequences drive the whole design:

1. **`tunefreq` must be correct per channel**, and it is the *stream
   centre*, not the slice frequency, because WDSP sums it with the shift
   we already push. See §4.1.
2. **The notch database is index-addressed.** `RXANBPEditNotch(channel,
   notch, ...)` and `RXANBPDeleteNotch(channel, notch)` take an integer
   index into that per-channel array. Our client-side list order must stay
   in lockstep with it. See §5.2.
3. **Notches are inert until explicitly enabled, per channel.** The DB is
   created with `master_run = 0` and nbp0 with its notch-run flag 0
   (`RXA.c:85-93`), and both `calc_nbp_lightweight` (`nbp.c:190`) and
   `calc_nbp_impulse` (`nbp.c:222`) bypass the DB entirely when `fnfrun`
   is 0. `RXANBPSetNotchesRun` is the only writer (`nbp.c:499`). Any
   channel that misses that call is silently notch-free. See §6.3.

**A notch edit rebuilds two filters, not one.** `UpdateNBPFilters`
(`nbp.c:345-359`) designs nbp0 *and* calls `recalc_bpsnba_filter`
(`snb.c:814-828`). `bpsnba` is the bandpass-for-SNB path, active for
LSB/CWL/DIGL/USB/CWU/DIGU whenever the master notch run is on, even with
SNB itself off (`RXA.c:934-968`, `RXAbpsnbaCheck` / `RXAbpsnbaSet`). This
is why §6.2 uses incremental edits rather than a full resync per change.

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
| Add gesture (Ctrl + right-click) | `console.cs:49614, 49629-49646 [v2.10.3.15]` |
| Edge-vs-centre drag rule | `console.cs:49037-49067 [v2.10.3.15]` |
| CW-pitch correction on add (NOT ported, §1.2) | `console.cs:40228, 18187-18194 [v2.10.3.15]` |
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
| D2 | **Ctrl + right-click creates a notch**, matching Thetis. Shift held makes it narrow (100 Hz). | This is Thetis's actual gesture (`console.cs:49614` `case MouseButtons.Right:`, `:49629` `if (Common.CtrlKeyDown)`, `:49644` `AddNotch`). An earlier revision specified Alt+click, justified by a comment at `:49633` that says "middle mouse"; that comment is stale and the premise was false. macOS maps Ctrl+click to secondary click, so Thetis's gesture is more reachable on a trackpad, not less. Our Ctrl binding is on the wheel (`SpectrumWidget.cpp:6631`), not the click, so nothing collides. See §7.1. |
| D3 | **Persist globally, not per-MAC.** | A notch tracks a QRM source at the operator's location and band, not a property of the radio. Thetis also persists globally. Diverges from the per-MAC convention used for hardware state, deliberately. |
| D4 | **Stable ids in the model, list order as the WDSP index.** | AetherSDR uses id-keyed entries (`QMap<int, TnfEntry>`), which is what the UI needs for drag and hit-test across mutations. WDSP needs positional indices. We carry both: a stable `id` field for the UI, with list position as the WDSP index. |
| D5 | **Drop depth and permanent.** Also decline AetherSDR's y-axis drag-to-resize. | See §1.2 and §7.2. |
| D6 | **`+TNF` adds at VFO shifted into the middle of the passband.** | Thetis `notchSidebandShift` (`console.cs:40281-40308 [v2.10.3.15]`), including its symmetric-filter (AM) fallback where `middle == 0` becomes `highHz / 2`. |
| D7 | **Fill in the existing Settings → DSP → **MNF** page.** Do not add a new one and do not rename it. | The page is already registered (`SetupDialog.cpp:609`) and `MnfSetupPage` exists as a disabled placeholder (`DspSetupPages.cpp:2110-2127`, ending in `disableGroup(mnfGrp)`). Thetis names the tab MNF too (`setup.designer.cs:44141`, `this.tpDSPMNF.Text = "MNF";`), so renaming to "Notches" would also cut against the standing match-Thetis-Setup-IA directive. See §9. |

---

## 4. Prerequisite: `RXANBPSetTuneFrequency` is never called

**This is a live defect in the current codebase, not new work.**

`wdsp_api.h:337` declares `RXANBPSetFreqs` and `:357` declares
`RXANBPSetShiftFrequency`; both are called (`RxChannel.cpp:425`,
`RxChannel.cpp:1483`). `RXANBPSetTuneFrequency` is declared nowhere and
called nowhere: `grep -rn "RXANBPSetTuneFrequency" src/ tests/` returns
nothing. Every channel's `notchdb.tunefreq` therefore sits at its
construction default.

This section is the gating first task, and it has four parts: the value
to push (§4.1), the call site (§4.2), and two sibling live defects in
the same code path that TNF would un-hide (§4.3, §4.4).

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

### 4.1 What value to push (this is not the slice frequency)

`tunefreq` is the **hosting DDC stream's centre**, not the slice's
frequency. WDSP sums the two terms (`nbp.c:192`, identical at `:225`) and
NereusSDR already feeds `shift` as the slice's displacement from its
stream centre:

```cpp
// src/core/SliceStreamAllocator.h:48
double shiftOffsetHz{0.0};   ///< slice freq minus stream centre
```

set at `SliceStreamAllocator.cpp:70` and `:137`, pushed at
`RadioModel.cpp:3676`. Driving `tunefreq` from the slice frequency would
therefore compute `2*sliceFreq - streamCentre`.

Thetis proves the intent directly. `console.cs:31921` gives the multi-RX
subrx its own shift (`radio.GetDSPRX(0, 1).RXOsc = rx2_osc;`) while
`console.cs:31940-31941` push the **identical** tunefreq to both
`id(0,0)` and `id(0,1)`. Two slices on one DDC, one shared tunefreq,
per-slice shifts. That is exactly our post-3F stream topology.
`RX1DDSFreq = CentreFrequency;` at `console.cs:31932 [v2.10.3.15]`.

Source the value from `SliceStreamAllocator::streamCentreHz(streamIndex)`,
or `Placement::newStreamCentreHz` on a claimed or retuned stream.

**Invariant to hold and to assert in tests:**

```
tunefreq + shift == the slice's demodulated RF Hz
                 == stream centre + slice offset + RIT + DIG
```

The sum lands on the RF frequency of the channel's baseband DC after the
shift stage, because `xshift` (`RXA.c:640`) runs before `xnbp`
(`RXA.c:645`). On this tree that equals the slice's demodulated RF,
because NereusSDR applies no RX-side CW-pitch DDS offset (see §1.2).

This is not an edge case: `SliceStreamAllocator::placeSlice` prefers
`Outcome::JoinedExisting` first (`SliceStreamAllocator.cpp:64-72`), which
sets a non-zero shift by construction, and CTUN pins the DDC in-window
(`RadioModel.cpp:3596-3597`). Non-zero shift is the normal case.

### 4.2 Where to push it (both obvious call sites are dead)

Call `RxChannel::setNotchTuneFrequency` unconditionally from
`RadioModel::bindSliceToStream`, immediately alongside the existing
`ch->setShiftFrequency(placement.shiftOffsetHz)` at `RadioModel.cpp:3676`,
and re-push on stream retune and slice migration.

Do **not** put it inside `RxChannel::setShiftFrequency`. That function
early-returns on value equality and skips its entire WDSP block below
0.5 Hz:

```cpp
// src/core/RxChannel.cpp:1468-1483
void RxChannel::setShiftFrequency(double offsetHz)
{
    if (offsetHz == m_shiftOffsetHz) { return; }        // :1470
    m_shiftOffsetHz = offsetHz;
#ifdef HAVE_WDSP
    if (std::abs(offsetHz) < 0.5) {
        SetRXAShiftRun(m_channelId, 0);                 // :1479
    } else {
        SetRXAShiftFreq(m_channelId, offsetHz);
        RXANBPSetShiftFrequency(m_channelId, offsetHz); // :1483
```

`m_shiftOffsetHz` defaults to `0.0` (`RxChannel.h:938`) and the dominant
retune outcome sets `p.shiftOffsetHz = 0.0` (`SliceStreamAllocator.cpp:128`,
sole-occupant `RetunedStream`, taken whenever CTUN is off), so in a
non-CTUN single-slice session the body never executes once. The function
also has no absolute frequency in scope; `RxChannel` carries no absolute
RF member.

`activateSliceChannel` is equally dead as a hook: it early-returns on an
already-active channel (`RadioModel.cpp:3118-3125`) and is documented
in-tree as "No-op for a slice whose channel is already live, i.e. every
retune" (`RadioModel.cpp:3706`).

`RXANBPSetTuneFrequency` is internally idempotent
(`nbp.c:479`, `if (tunefreq != a->tunefreq)`), so an unconditional push
costs nothing.

### 4.3 Sibling live defect: shift is never cleared back to zero

`setShiftFrequency`'s near-zero branch calls `SetRXAShiftRun(channel, 0)`,
which writes `rxa[channel].shift.p->run` (`shift.c:113-116`) and never
touches `NOTCHDB->shift`. `RXANBPSetShiftFrequency` is the sole writer of
that field (`nbp.c:487-496`), and `calc_nbp_lightweight` consumes it
unconditionally with no reference to any run flag. So the stored shift
goes stale whenever the offset returns to zero.

Thetis has no such branch: `radio.cs:1419-1420 [v2.10.3.15]` pushes both
setters on every `RXOsc` change including to zero, and `SetRXAShiftRun`
appears nowhere in the Thetis Console tree. The run gate is
NereusSDR-original.

The zero transitions are routine: turning RIT off, leaving DIGU/DIGL
(both via `RadioModel.cpp:8973-8988`), band jump (`MainWindow.cpp:1087`)
and CTUN off (`MainWindow.cpp:1641`, `:7304`).

**Fix, in this task:** call `SetRXAShiftFreq` and
`RXANBPSetShiftFrequency` on **both** branches of `setShiftFrequency`,
leaving `SetRXAShiftRun` as the only thing the magnitude gate controls.
While the file is open, correct the stale inline cite at
`RxChannel.cpp:1481` (`radio.cs:1417-1418` should read `1419-1420`) and
audit the sign: Thetis negates (`-value`), NereusSDR does not.

This is latent today only because with `nn == 0` the offset cancels
exactly (`nbp.c:110-111`, then `:202-203`) and `master_run` defaults to
0. TNF is what un-hides it.

### 4.4 Sibling live defect: RIT clobbers the stream shift

`RadioModel.cpp:8966-8988` installs an `updateShiftFrequency` lambda
whose `offset` is RIT + DIG **only**. It discards
`placement.shiftOffsetHz` entirely and ends with
`rxCh->setShiftFrequency(offset);`. Its comment still reads "For 3G-10
(single RX, no CTUN), the shift = these two terms only", which is stale
post-3F. Toggling RIT on a shifted slice therefore clobbers the stream
offset, and notches would jump.

**Fix, in this task:** the RIT/DIG lambda must **compose** with
`placement.shiftOffsetHz`, not replace it, so the §4.1 invariant holds.
Assert the sum after a RIT toggle.

### 4.5 Test seam

`setNotchTuneFrequency` must set its carry member outside
`#ifdef HAVE_WDSP`, mirroring `setShiftFrequency`
(`RxChannel.cpp:1468-1477`), and expose `notchTuneFrequencyHz()`.

Note the usual "pass the same value so the C++ early-return fires" hatch
(`tst_rx_channel_rebuild.cpp:8-11`) does **not** work here: WDSP
dereferences the NULL NOTCHDB at `nbp.c:478` *before* its own equality
check at `:479`. Test binaries do compile with live WDSP
(`CMakeLists.txt:1124` sets `HAVE_WDSP` PUBLIC), which is why the
existing RxChannel tests use `kTestChannel = 99` on a never-opened
channel.

Two workable seams, both already used in-tree:
- Caller-side value assertion via `RadioModel::streamCentreChanged` /
  `streamCentreHzForTest` (`RadioModel.h:1947`, `:2483`), the pattern
  `tst_stream_pool_binding.cpp:580-583` already uses for the sibling
  shift push.
- A real opened channel through the `NEREUS_BUILD_TESTS` friend seam plus
  `createRxChannel`'s real `OpenChannel` (`WdspEngine.cpp:377-390`;
  pattern at `tst_ps_feedback_channel.cpp:72,78`).

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
void clear();   // see contract note below

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
    void notchesReset();   // whole-list replacement; see clear() contract
```

`notchRemoved` carries `formerIndex` because the WDSP fan-out needs the
positional index the entry occupied, and it is gone by the time the signal
lands.

**`clear()` contract.** The fan-out in §6.3 is purely signal-driven, so a
`clear()` that emitted nothing would leave every channel's notch set
installed while the model showed none. `clear()` emits `notchesReset()`,
which `RadioModel` handles as `syncNotches({})` on every channel.

Unlike AetherSDR's `TnfModel` (`RadioModel.cpp:4307 [@c6481cbf]`),
`clear()` must **not** run on disconnect. Under D3 the notch list is a
persisted property of the operator's location, not of the session, and
AetherSDR clears because its list is a mirror of radio state rather than
the source of truth.

### 5.4 Ported guards (verbatim behaviour)

| Guard | Value | Thetis source |
| --- | --- | --- |
| Reject add if a notch is already within N Hz | 10 Hz | `console.cs:40260 [v2.10.3.15]` |
| Default new-notch width | 200 Hz | `console.cs:40268 [v2.10.3.15]` |
| Narrow (Shift held) width | 100 Hz | `console.cs:40269 [v2.10.3.15]` |
| Round centre to whole Hz on add | `Math.Round` | `console.cs:40230 [v2.10.3.15]` |
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

**XVTR override: deferred, not ported.** Thetis's XVTR-aware min/max
override is real (`console.cs:40051-40077`), but NereusSDR has nothing to
route it through. `Band::XVTR` is a label only (`Band.cpp:126`, `:215`);
`bandFromFrequency` never returns it (`Band.h:95-99`, `Band.cpp:165`
falls through to `Band::GEN`); `AlexController::xvtrActive()` is a bare
bool. A transverter table does exist (`src/gui/setup/hardware/XvtrTab.cpp`,
RF Start / RF End / LO Offset, persisted at `HardwarePage.cpp:150`), but
it has no frequency-to-row lookup, no per-RX selected index, no consumer
of `loOffset`, and a broken round-trip (emits `"rfStart"` at `:239`,
restores `"rfStartHz"` at `:290`).

Behavioural impact is nil today: XVTR band clicks are rejected
(`RadioModel.cpp:4813-4816`) and Radio > Transverters is a disabled NYI
entry (`MainWindow.cpp:4823-4825`), so Thetis's branch is unreachable and
its else-path (plain min/max) is already faithful.

**The more pressing half of the same guard row:** NereusSDR has no radio
frequency-range model value at all. `BoardCapabilities` carries no
min/max field and `SliceModel::setFrequency` (`:199-217`) does not clamp.
The only analogue is `std::clamp(hz, 100000.0, 61440000.0)` at
`VfoWidget.cpp:698` / `:2840`, which happens to match Thetis's
`max_freq = 61.44` (`console.cs:15552`). The notch constrain uses those
same bounds until a real capability field exists.

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
incremental calls are used for live edits.

**Cost, stated accurately.** `RXANBPEditNotch` is not cheap: it runs
`UpdateNBPFilters` (`nbp.c:345-359`), which designs **two** filters,
nbp0 plus `recalc_bpsnba_filter` (`snb.c:814-828`). So an incremental
edit is one `UpdateNBPFilters` where `syncNotches` would be 2N of them
(`nbp.c:384`, `:435`, `:456`). That is the real reason to use the
incremental path, not "does not rebuild the whole filter".

**Do not add drag throttling.** Thetis pushes on every mouse-move by
named design (`console.cs:49967`, `//MW0LGE [2.9.0.7] update on drag`,
and `:50019`) and does strictly more per move than we would
(`SaveNotchesToDatabase()` + `UpdateNotchDisplay()`,
`console.cs:40105-40106`). The impulse cache is not at risk either:
`DspOptionsCacheImpulse` defaults False (`WdspEngine.cpp:236-237`), so
`_use_cache` is 0 and nothing is inserted or evicted
(`impulse_cache.c:159-167`). `setImpulse_fircore` is a memcpy plus
`calc_fircore` (`firmin.c:456-460`) and never replans FFTW.

**Return values and the notch cap.** `RXANBPAddNotch` is an INSERT
guarded by `notch <= b->nn && b->nn < b->maxnotches`, returning -1 with
no mutation (`nbp.c:362-390`). The wrappers must surface that, not
discard it: on a -1 or on a count mismatch against
`RXANBPGetNumNotches`, the recovery is a full `syncNotches()` rather
than an assert, because a release build compiles asserts out. Thetis is
self-correcting by a different route we are deliberately not copying: it
reads the index back from WDSP before every add
(`console.cs:40262-40266`) and rebuilds `MNotchDB` from channel (0,0)
after every mutation (`setup.cs:17934-17954`).

`kMaxNotches = 1024`, cited to `RXA.c:88`. Unreachable in practice given
the 10 Hz dedupe, but stated so the overflow path is defined.

### 6.3 Fan-out

`RadioModel` connects `NotchModel`'s signals and pushes to every live
`RxChannel` obtained by iterating `slices()`. Thetis fans to three fixed
ids (`WDSP.id(0,0)`, `WDSP.id(0,1)`, `WDSP.id(2,0)`, e.g.
`console.cs:40269-40271 [v2.10.3.15]`); we iterate because slice count is
dynamic post-3F.

**`activateSliceChannel` alone is not sufficient, and the gap is not
"an empty list".** `connectToRadio`'s WDSP-init lambda creates channel 0,
applies its state and calls `rxCh->setActive(true)` unconditionally at
`RadioModel.cpp:5365`, before `openRxChannelPool(...)` at `:5377`, with
an in-code comment saying the ordering is deliberate (`:5370-5372`).
`activateSliceChannel` then takes `if (!ch || ch->isActive()) { return; }`
(`RadioModel.cpp:3118`). So Slice A, the primary receiver, never passes
through the hook.

That matters more than a missing list because each channel's notch DB is
built inert:

```c
// third_party/wdsp/src/RXA.c:85-93
rxa[channel].ndb.p = create_notchdb (
    0,      // master run for all nbp's
    1024);  // max number of notches
rxa[channel].nbp0.p = create_nbp (
    1,      // run, always runs
    0,      // run the notches
```

and both `calc_nbp_lightweight` (`nbp.c:190`) and `calc_nbp_impulse`
(`nbp.c:222`) bypass the notch DB entirely when `fnfrun` is 0, falling
through to a plain `fir_bandpass`. `RXANBPSetNotchesRun` is the only
writer (`nbp.c:499`). Without it, Slice A is notch-*inert*, not merely
empty, even for notches added live in that session. Reconnect reopens the
hole: `teardownConnection` calls `m_wdspEngine->shutdown()`
(`RadioModel.cpp:10175`), destroying every RX channel
(`WdspEngine.cpp:310-318`).

**Implementation:** add a `syncNotchesToAllChannels()` called at the tail
of `openRxChannelPool`, after `activateBoundSliceChannels()`
(`RadioModel.cpp:3068`), so every open channel including id 0 is
reconciled on connect and on reconnect. Keep the `activateSliceChannel`
hook for the later-added-slice case. Each reconciled channel gets
`syncNotches(...)`, `setNotchesRun(globalEnabled())` and the current
`setNotchTuneFrequency(...)`.

`tst_notch_channel_sync` covers Slice A across a simulated reconnect
using the existing friend seam: seven test classes are `friend`s of
`WdspEngine` (`WdspEngine.h:679-694`), and
`tests/tst_stream_pool_binding.cpp:997-1017` already drives
`openRxChannelPool` with slice 0 bound.

### 6.4 TCI

`RadioModel::rxNf(int)` / `setRxNf(int, bool)` currently read and write
`m_tciStubRxNf[]` (`RadioModel.cpp:11412-11420`). They are repointed at
`NotchModel::globalEnabled` / `setGlobalEnabled`.

(The stub is not entirely unread: `TciProtocol.cpp:761-762` consumes it
for the init burst. Harmless, because post-repoint both indices report
the same global flag, which is exactly what `GetMNF` is
(`console.cs:52317-52326`, `// mnf enabled globally`), and both
init-burst suites bind `TestMockRadioModel`.)

The `rx_nf_enable` handler's **wire format** matches Thetis and needs no
change. What is missing is the broadcast. Thetis's TNF flag is
event-driven: `console.cs:40004` fires `TNFChangedHandlers` on change,
`TCIServer.cs:7690-7698` routes it to every listener, and `NfChanged`
sends **both** indices (`TCIServer.cs:1315-1320`). Thetis's
`handleRxNfEnable` set branch sends nothing itself
(`TCIServer.cs:3392-3398`).

We do the inverse: `TciProtocol.cpp:2170-2175` queues a single index
unconditionally, and there is no wiring at all for a UI-originated flip,
which §7 newly creates three of (`+TNF`, status-bar light, shortcut).
`TCIServer.cs:6771` is the single unported subscription, sitting between
two that are ported.

**Implementation:** wire `NotchModel::globalEnabledChanged` into
`TciServer`'s existing broadcast block, emitting `enqueueLocalBroadcast`
for rx 0 and rx 1, following the 47 sibling flags already using it
(e.g. `TciServer.cpp:845-851`, APF, citing `TCIServer.cs:6770`). Then
**drop** the handler's single-index push (`TciProtocol.cpp:2173-2175`) as
a redundant duplicate, matching Thetis. The mechanism is documented
in-tree as exactly this bug class (`TciProtocol.h:97-103`, "bench bug
2026-05-22").

Note also that our handler emits unconditionally where Thetis gates on
change.

Arity: TCI addresses receivers as `rx 0|1` while our notch enable is
global (D1). Set on either index sets the global flag; query on either
returns it. Thetis is the same, since `TNFActive` is likewise global
despite the per-rx command shape.

---

## 7. Interaction model

| Gesture | Result | Source |
| --- | --- | --- |
| Ctrl + right-click on panadapter | Add 200 Hz notch at clicked frequency | `console.cs:49614, 49629-49646 [v2.10.3.15]` |
| Shift + Ctrl + right-click | Add 100 Hz notch | `console.cs:40268-40269 [v2.10.3.15]` |
| Drag notch body | Move notch centre | `console.cs:49037-49067 [v2.10.3.15]` |
| Drag within 4 px of an edge, or Shift + drag | Resize notch | `console.cs:49037-49067 [v2.10.3.15]` |
| Wheel over notch (Shift = 1 Hz step) | Resize notch | `console.cs:33299-33321 [v2.10.3.15]` |
| Hover over notch | Popup with frequency + width | AetherSDR `m_tnfHoverPopup [@c6481cbf]` |
| Right-click on notch | Width presets, Active/Bypass, Remove | AetherSDR `SpectrumWidget.cpp:8560-8580 [@c6481cbf]` |
| Right-click on empty pan | "Add notch at X MHz" | AetherSDR `SpectrumWidget.cpp:8585 [@c6481cbf]` |
| `+TNF` button | Add at VFO + sideband shift | `console.cs:40313-40331 [v2.10.3.15]` |
| Click status-bar TNF light | Toggle all notches | AetherSDR `MainWindow_Shortcuts.cpp:612-614 [@c6481cbf]` |
| Bound shortcut key | Toggle all notches | AetherSDR `MainWindow_Shortcuts.cpp:1093 [@c6481cbf]` |

### 7.1 The add gesture is Thetis's, not a divergence

An earlier revision of this document specified Alt + click on the grounds
that Thetis used Ctrl + middle-click, which is unreachable on a Mac
trackpad. **That premise was false.** `console.cs:48974` opens
`switch (e.Button)`; `:49614` is `case MouseButtons.Right:`; `:49629` is
the `if (Common.CtrlKeyDown)` branch containing `AddNotch(dFreq, rx)` at
`:49644`. The stale comment at `:49633` mentioning "middle mouse" is what
misled the earlier reading. `grep -n "AddNotch("` returns only `:40222`
(definition), `:40331` (`TNFAdd`) and `:49644`. The single
`MouseButtons.Middle` hit (`:49725`) only toggles active (`:49735`) or
removes on Shift (`:49731`).

The premise inverts on inspection: macOS maps Ctrl+click to a secondary
click, so Thetis's real gesture is *more* reachable on a trackpad, not
less. NereusSDR's Ctrl binding is on the wheel
(`SpectrumWidget.cpp:6631`), not the click, so there is no collision.
We therefore match Thetis.

### 7.2 Edge-vs-centre drag discrimination

The real rule is `console.cs:49037-49067 [v2.10.3.15]`, not the three
bool declarations at `:33286-33288`:

```csharp
m_BDragginNotchBWRightSide = (dMouseVFO >= SelectedNotch.FCenter);
...
if (nHpx - nLpx > 8)            // only offer edge zones if wide enough
    ...                          // +/- 4 px edge zones
if (bNearEdge || Common.ShiftKeyDown)  // can also hold shift drag to resize
```

Three things to port: the 8 px minimum on-screen width before edge zones
are offered at all, the plus/minus 4 px edge zone, and the
side-of-centre default that decides which edge is being dragged.
`Shift + drag` is an explicit alternative to being near an edge.

Note the Shift collision with the narrow-add gesture in the table above.
They do not conflict, because add is Ctrl + right-click and resize is a
left-drag on an existing notch.

**AetherSDR's drag is a different shape and is not what we adopt.**
`SpectrumWidget.cpp:8648-8650` starts the drag on any body pixel via
`tnfAtPixel(mx)`; the triangle at `:13544-13549` is drawn and never
hit-tested. `:9051-9070` then sets centre from x AND width from y
(`std::pow(2.0, -dy / 48.0)`, clamped 10..12000 Hz). We take Thetis's
edge model and explicitly decline AetherSDR's y-axis width gesture, so
that vertical mouse movement during a centre drag does not silently
change width. Recorded here rather than in §1.2 because it is an
interaction choice, not a missing capability.

### 7.3 Which hit test governs

Two are cited in this document and they behave differently. **Thetis's
`notchSurrounding` (`radio.cs:4297-4322`) governs**: first-found in list
order, with the pad applied only when `n.FWidth < nPadWidth * 2`. That is
what §5.3 ports and what `tst_notch_hit_test` exercises.

AetherSDR's `tnfAtPixel(x, preferredId)` (`SpectrumWidget.cpp:13648-13681`,
nearest-centre with a reverse scan, plus/minus 3 px pad and a preferred-id
short-circuit) is cited in §2.3 for its *rendering* neighbours only. Its
`preferredId` short-circuit is worth keeping for one purpose: latching the
notch under an in-progress drag so an overlapping neighbour cannot steal
it mid-gesture. Our drag signals carry an explicit latched `id`, which
achieves the same thing.

The Ctrl + right-click add is checked before the existing right-click
context-menu path in `mousePressEvent`, so menu behaviour is unchanged
when Ctrl is not held.

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
(`m_overlayStaticDirty`, `SpectrumWidget.h:1991` and `:2062-2066`) so a
static notch set costs nothing per frame.

This does not regress the dual-layer overlay split.
`SpectrumWidget::mouseMoveEvent` already calls `markOverlayDirty()`
unconditionally at its tail for every hover move in the GPU path
(`SpectrumWidget.cpp:6491-6495`), and `NEREUS_GPU_SPECTRUM` is default ON
(`CMakeLists.txt:420`), so TNF invalidation cannot increase the count.
The 2026-05-26 dual-layer bench (commit 9723002d) targeted the
*timer-driven* per-frame rebuild gated at `SpectrumWidget.cpp:2852-2856`,
not interaction. `SpectrumWidget.h:1995-1997` states the static texture
carries chrome "that only changes on operator interaction".

### 8.3 Visual notch (trace dent)

Ports Thetis `modifyDataForNotches` (`display.cs:4733-4817 [v2.10.3.15]`,
note the function runs to `:4817`; the dent maths itself, `fAttenuation`,
`wL`, and the `1f/pow(wL/(wL-x),1.5)` skirt, is at `:4791-4816`) and its
`handleNotches` helper, defined at `display.cs:8644-8745 [v2.10.3.15]`.

**Dent width is not the notch width.** `handleNotches` computes

```
dentWidth = max(notch.widthHz, minNotchWidthHz()) + 20.0
```

cited to `display.cs:8679-8680` ("use the min width of filter from WDSP"
/ "fudge factor to align better with spectrum notch"). `kNotchDentFudgeHz
= 20.0` carries that cite per the constants rule. This is not marginal:
`nbp.c:91` gives a min width of `2200/(nc/256)*(rate/48000)`, which is
275 Hz at nc=2048 and 48 kHz, wider than this document's own 200 Hz
default. `minNotchWidthHz()` therefore feeds the dent path, not only the
§9 readout.

**Do NOT port `handleNotches`'s RIT/CTUN offset term** (`display.cs:8650`).
It compensates for Thetis's VFO-label-anchored pixel maths (`:8646`,
`:8681`) combined with its RIT-driven DDS retune
(`console.cs:31776-31777`, `:31894-31895`). NereusSDR's axis is absolute
RF (`SpectrumWidget.cpp:4025-4030`), RIT never retunes hardware
(`SliceModel.h:740-741`, `RadioModel.cpp:8973-8982`), and WDSP applies
shift to the passband, not to the notch (`nbp.c:192`). Adding it would
displace every dent by `rit_hz`. Recorded so nobody re-adds it.

Also drop the `cwSideToneShift` parameter entirely rather than threading
a constant zero (see §1.2).

#### What reads the undented copy, and what does not

Thetis keeps one pristine copy, but it is read by exactly **one**
consumer, and an earlier revision of this document got this wrong in both
directions.

Thetis **deliberately dents** peak hold, the blob/IMD detector and the max
readout on the spectrum plane: `display.cs:5256` computes
`max = data[i] + fOffset` from the dented array and feeds `local_max_y`
(`:5269`), the blob detector (`:5280`) and spectral peak hold (`:5337`).
Only the noise-floor accumulator reads `max_copy` (`:5259`). The
undented-copy switch MW0LGE added is on the *waterfall* plane
(`:6613-6615`, `:6620-6622`, `//[2.10.3]MW0LGE use unmodified, not the
notced data`).

So: **dent in place, Thetis-faithful, and keep one pristine copy read
only by `processNoiseFloor`.** ActivePeakHold, PeakBlobs and the cursor
peak readout intentionally see the dent. Stated explicitly with the
`display.cs:5269` / `:5280` / `:5337` cites so a later reviewer does not
"fix" it into a divergence.

An earlier revision claimed the NF-aware grid was the thing at risk. It
is not: `MainWindow.cpp:3069-3072` feeds ClarityController from raw
`FFTEngine::fftReady` bins, a different pipeline, and
`SpectrumWidget.cpp:2608-2612` states the split in-tree. A dent in
`m_renderedPixels` is structurally incapable of moving the NF-aware grid.
It also claimed `tst_nf_aware_grid` and `tst_clarity_nf_grid_coexistence`
guard this machinery. They do not: neither builds a spectrum frame, so
`processNoiseFloor()` (called only from `updateSpectrumLinear`,
`SpectrumWidget.cpp:2832`) never runs in either.

The "waterfall-minimum tracking" half of Thetis's discipline is **N/A**
here. NereusSDR has no per-frame waterfall minimum: `m_wfLowThreshold` is
a persisted user setting (`SpectrumWidget.cpp:623`, `:2157`) and the only
consumer of `m_wfRenderedPixels` is `pushWaterfallRow` (`:2880` -> `:466`).

#### MaxBin: a NereusSDR-only hazard Thetis cannot have

We have a consumer Thetis lacks. `peakDbmInSlicePassband()`
(`SpectrumWidget.cpp:2909`, scanning `m_renderedPixels` at `:2934-2936`)
feeds `MainWindow.cpp:3113-3115` -> `WdspEngine::setMaxBinDbmFromSpectrum`
(`WdspEngine.cpp:1430`) -> the analog S-Meter's MaxBin mode shipped in
v0.5.2. Thetis reads MaxBin from WDSP *upstream* of display.cs
(`console.cs:46959`, `dsp.cs:849-850`), so a Thetis visual notch
structurally cannot move its meter. Ours would: notch a loud carrier and
the needle drops because the display got dented.

**Decision (JJ, 2026-07-28): route `peakDbmInSlicePassband()` at the
undented copy.** A display preference must not silently change a
measurement, and this matches Thetis's effective behaviour. Recorded as a
deliberate NereusSDR-specific divergence from the dent-in-place rule
above.

#### Toggle

Gated by a toggle, default **off**, matching Thetis
`m_bShowVisualNotch = false` (`display.cs:1070 [v2.10.3.15]`). Persisted
as `NotchVisualEnabled`. Suppressed during MOX, as Thetis does
(`display.cs:5235`, `!local_mox`). Upstream this control lives on the MNF
tab, not a display tab (`setup.cs:24376-24379`), so it belongs on the §9
page. Note Thetis drives *both* `Display.ShowVisualNotch` and
`MiniSpec.ShowVisualNotch` from it; we port only the first.

#### Test seam

Add `nfFftBinAverageForTest()` following the existing
`spotMarkersForTest()` convention (`SpectrumWidget.h:1103-1120`). The
driver half already exists: `updateSpectrumLinear` is a public slot and
`renderedPixels()` is public (`SpectrumWidget.h:394`).

---

## 9. Settings → DSP → MNF

**Fill in the page that already exists. Do not add a new one.**
`SetupDialog.cpp:609` already calls
`registerPage(dsp, "MNF", ...)`, and `MnfSetupPage`
(`DspSetupPages.cpp:2110-2127`) is a placeholder ending in
`disableGroup(mnfGrp)` (`:96-99`, "NYI guard"). Keep the name MNF:
Thetis's tab is literally named that (`setup.designer.cs:44141`,
`this.tpDSPMNF.Text = "MNF";`) and `grpDSPMNF` holds exactly the control
set below plus the §8.3 visual-notch toggle (`:44145-44159`:
chkVisualNotch, btnVFOFreq, chkMNFAutoIncrease,
btnMNFAdd/Edit/Delete/Enter/Cancel, chkMNFActive,
udMNFFreq/Width/Notch).

There is a third stub to retire in the same work:
`SpectrumOverlayPanel.cpp:273-278` already carries a disabled `"MNF"`
button ("Manual notch filter (NYI)"). Replace it with the `+TNF` button
from §1.1 item 6 rather than shipping both.

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
from **three** upstream files. Both need, **in the commit that introduces
them**:

1. The AetherSDR `TnfModel` attribution block (`GPL-3.0-or-later`,
   `@c6481cbf`). Use the form at
   `src/gui/SpectrumOverlayPanel.cpp:12-20`, **not** the one at
   `SpotModel.h:11`. `HOW-TO-PORT.md:36-38` rule 6 requires the project
   URL *and* primary author; `SpotModel.h` says only "(C) its
   contributors" and names nobody. 62 files in `src/` already use the
   compliant `Jeremy (KK7GWY) / AetherSDR contributors` form against 24
   using the short one, and `aethersdr-reconciliation.md:72-78` prints
   the canonical block.
2. **Both** Thetis headers byte-for-byte, `radio.cs` and `console.cs`,
   in citation order. `radio.cs` supplies `MNotch` / `MNotchDB` (§5.3),
   but §5.4 ports console.cs logic into the same file: `NotchAdminBusy`
   (`console.cs:40224`), whole-Hz rounding (`:40230`), the min/max
   constrain (`:40257`), the 10 Hz dedupe (`:40260`), the 200/100 Hz
   width defaults (`:40268-40269`) and the wheel-resize edge clamp
   (`:33317-33318`).

   The headers are materially different and **no script catches the
   omission**: `console.cs:7` credits Sizenko Alexander of Style-7,
   `:31-34` credits Chris Codella (W2PA), `:36` carries the
   `//N1GP G2E added` line. None appear in `radio.cs:1-42` (FlexRadio /
   Doug Wigley / Richard Samphire only). `verify-thetis-headers.py:70-77`
   checks five generic anchors; its `check_samphire_marker` (`:281-286`)
   needs only the literal `MW0LGE`, which radio.cs's own dual-licensing
   block supplies. Live precedent for stacking:
   `src/core/CalibrationController.h:23` / `:70`.
3. `// --- From <filename> ---` separators between all three, per
   CLAUDE.md multi-file attribution and `HOW-TO-PORT.md:31-32`.
4. A `Modification history (NereusSDR)` block.
5. A `THETIS-PROVENANCE.md` row listing **both** Thetis files, plus a row
   in `aethersdr-reconciliation.md`.

### 10.2 Existing files

Registered in `THETIS-PROVENANCE.md` (new ported logic in them still
needs inline `// From <Upstream> <file>:<line> [<stamp>]` cites):
`wdsp_api.h`, `RxChannel.{h,cpp}`, `RadioModel.{h,cpp}`,
`SpectrumWidget.{h,cpp}`, `MainWindow.{h,cpp}`, `DspSetupPages.{h,cpp}`,
`SetupDialog.cpp`.

**`SpectrumOverlayPanel.{h,cpp}` is NOT Thetis-registered, and must
receive no Thetis cite.** An earlier revision asserted it was. It is not:
both files sit in the column-2 exclusion table under the "Independently
implemented" heading at `THETIS-PROVENANCE.md:393` (rows at `:403-404`),
whose preamble at `:395-398` affirmatively publishes that they were
"written without consulting Thetis source". `check-new-ports.py:233-256`
parses column 1 only, by explicit anti-loophole design, so adding the
`// From Thetis console.cs:40313-40331` cite that §7 and §12 step 8 would
otherwise require makes the file fail the diff-mode gate, and because the
pattern scan is whole-file (`:326-331`), every subsequent touch of the
file fails too. Both `ci.yml:46` and `:58` run it without
`continue-on-error`.

They *are* registered, in the right index for what they are: column-1
rows at `aethersdr-reconciliation.md:155-156`, with a full AetherSDR
GPLv3 header and Modification history already present
(`SpectrumOverlayPanel.h:1-38`).

**Consequence for §7 and §9:** keep all Thetis-derived sideband-shift
maths inside `NotchModel`, which §10.1 already treats as a new
attribution event, and make the `+TNF` button a pure signal emitter cited
only to AetherSDR. Do not reach for a `no-port-check:` marker: that would
leave a published false statement standing about a file containing ported
Thetis logic.

**Not in scope:** §1.1 item 7's "assignable keyboard shortcut" has no
home. `src/gui/setup/KeyboardSetupPages.cpp:32-70` is a 100% NYI stub
(every control `setDisabled(true)`), no `ShortcutManager` or
`registerAction` exists anywhere in `src/`, and every shipped shortcut is
a plain `QAction::setShortcut` in `MainWindow.cpp`. Ship the TNF toggle
as a `QAction` with a fixed accelerator in `MainWindow.cpp`; building a
shortcut-assignment subsystem is a separate epic and is explicitly out of
scope here.

### 10.3 Inline comment preservation

Ship-blocking per CLAUDE.md. Author tags inside ported ranges must survive
verbatim. Known tags in the ranges this work touches:

| Tag | Location |
| --- | --- |
| `//NOTCH MW0LGE` | `console.cs:33283` (section marker above the drag-state block, closed by `//END NOTCH` at `:33322`) |
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

**Underscored variants never satisfy a bare-`MW0LGE` requirement.**
`port_contains_tag` matches `\bMW0LGE\b`
(`verify-inline-tag-preservation.py:303`) and `_` is a word character, so
`MW0LGE_21e` yields no boundary. The drag-state cite
(`console.cs:33284-33297`) therefore needs the bare tag from `:33283`
*in addition to* `//MW0LGE_21e`. Verified by running the script's own
extractor:

```
extract_tags_from_region(console.cs, [33284, 33297]) -> [(33283, 'MW0LGE')]
extract_tags_from_region(console.cs, [33286])        -> [(33283, 'MW0LGE')]
```

Place it verbatim within plus/minus 10 lines of the cite, e.g.
`// NOTCH MW0LGE  [original section marker from console.cs:33283]`.

**Do not "fix" a failure by narrowing a cite range** so the plus/minus-5
extraction window misses the tag. That passes CI while genuinely losing
the attribution, which is the exact outcome this gate exists to prevent.

**Wide cites pull in more tags than they look like they do.** The
extraction window spans `min(line_nums)-5 .. max(line_nums)+5`
(`verify-inline-tag-preservation.py:274-275`), so the §2.2 persistence
cite `console.cs:3034-3035, 4763-4764` spans roughly 1740 lines and
generates 18 further `MW0LGE` requirements (`console.cs:3358` through
`:4713`). Same trap on `display.cs:5046, 5095, 6505, 6530, 6741` and
`TCIServer.cs:3384-3400, 1954-1960`. All are the same tag string, so a
single bare `MW0LGE` discharges them, but prefer splitting those cites so
the windows stay tight and the intent stays readable.

Note the local pre-commit hook may report `tag-preservation SKIPPED` if
it cannot locate the Thetis clone, in which case the failure surfaces
only in CI. See PR #309.

---

## 11. Testing

TDD per task. New test executables:

| Test | Covers |
| --- | --- |
| `tst_notch_tune_frequency` | §4. Asserts the §4.1 invariant `tunefreq + shift == slice demodulated RF` via `RadioModel::streamCentreHzForTest` (`RadioModel.h:2483`), the seam `tst_stream_pool_binding.cpp:580-583` already uses for the sibling shift push. Includes the §4.3 pan-out-and-back-to-zero sequence and the §4.4 RIT toggle. Lands first. |
| `tst_notch_model_guards` | §5.4. 10 Hz dedupe (exact-boundary: `<` not `<=`, `radio.cs:4267`), 200/100 Hz defaults, whole-Hz rounding, min/max clamp, admin-busy rejection, wheel-resize edge clamp, and the §1.2 CW case (a notch added at F in CWU and CWL stores at exactly F). |
| `tst_notch_model_index_invariant` | §5.2. List position tracks the WDSP index across add / edit / delete, including deleting from the middle, plus the §6.2 `RXANBPAddNotch` -1 and count-mismatch recovery paths. |
| `tst_notch_sideband_shift` | §D6. Table-driven over `notchSidebandShift` (`console.cs:40281-40307`). USB 200/2800 gives +1500; LSB -2800/-200 gives -1500; AM -3000/+3000 hits the `middle == 0` fallback at `:40294-40295` giving 1500. A dropped sign puts every LSB `+TNF` notch at VFO+1500, outside the passband: silent on LSB, correct on USB, so it is invisible without this test. |
| `tst_notch_spatial_helpers` | §5.3. `notchesInBandwidth` inclusive edge-overlap (`radio.cs:4286`); `notchSurrounding` with and without `padWidthHz`, covering the pad-applies-only-when-`FWidth < padWidth*2` branch (`radio.cs:4310`). |
| `tst_notch_persistence` | §5.5. Round-trip through AppSettings including global enable, visual toggle, auto-increase. |
| `tst_notch_channel_sync` | §6.3. Slice A gets the notches, the run flag and the tune frequency on connect **and across a reconnect**, plus a later-added slice inheriting the set. Uses the `WdspEngine` friend seam (`WdspEngine.h:679-694`) as `tst_stream_pool_binding.cpp:997-1017` does. |
| `tst_notch_hit_test` | §7.3. Exercises Thetis's `notchSurrounding` rule, first-found in list order. Edge-vs-centre discrimination per §7.2 including the 8 px minimum-width gate and the plus/minus 4 px zone. Off-screen rejection. |
| `tst_notch_visual_does_not_perturb_noise_floor_or_maxbin` | §8.3. Asserts positively in both directions: `processNoiseFloor` and `peakDbmInSlicePassband()` read the pristine copy, while ActivePeakHold and PeakBlobs **do** see the dent (Thetis-faithful, `display.cs:5269`, `:5280`, `:5337`). Needs the new `nfFftBinAverageForTest()` accessor. |
| `tst_notch_tci_rx_nf_enable` | §6.4. `rx_nf_enable` set and query round-trip against the real master enable, both rx indices, plus a UI-originated flip broadcasting to both indices. |

`tst_nf_aware_grid` and `tst_clarity_nf_grid_coexistence` must both still
pass with visual notch enabled. Note they do **not** guard the §8.3
invariant (see §8.3); they are a general regression check only.

### 11.1 What cannot be unit-tested, and why

`RXANBPAddNotch` (`nbp.c:367`) and `RXANBPGetMinNotchWidth` (`nbp.c:598`)
dereference `rxa[channel]` slots populated only by `create_rxa`
(`RXA.c:86`). Test binaries compile with live WDSP
(`CMakeLists.txt:1124` sets `HAVE_WDSP` PUBLIC, `tests/CMakeLists.txt:165`
links it), so calling a wrapper on an unopened channel segfaults rather
than no-opping. The in-tree convention exists for exactly this reason:
`kTestChannel = 99;  // Never opened via OpenChannel`
(`tst_rxchannel_snb.cpp:65`, `tst_rxchannel_emnr.cpp:65`,
`tst_rxchannel_squelch.cpp:65`).

Consequently the WDSP-facing wrappers are verified either caller-side
(assert the value the model hands to `RxChannel`) or against a really
opened channel via the `NEREUS_BUILD_TESTS` friend seam plus
`createRxChannel`'s real `OpenChannel` (`WdspEngine.cpp:377-390`; pattern
at `tst_ps_feedback_channel.cpp:72,78`). Everything else is bench.

### 11.2 Bench verification matrix

Project convention: 26 verification artifacts exist under
`docs/architecture/`, and they are treated as merge gates
(`phase3m-3a-iv-verification/README.md:46-48`: "PR is not merged until
both ANAN-G2 and HL2 columns are PASS"). The closest analogue is exact:
`phase3g-rx-epic-b-verification/README.md`, the WDSP NB/NB2/SNB family,
same RX-chain-with-audio-only-payload category as an NBP notch.

Create `docs/architecture/2026-07-28-tnf-verification/README.md`, columns
ANAN-G2 (P2) and HL2 (P1), covering at minimum:

1. Notch a real carrier, confirm audible removal.
2. Removal persists across a VFO retune within the band (exercises §4).
3. Drag tracks the cut in real time.
4. Per-notch bypass returns the carrier; re-enable removes it again.
5. Master TNF off restores everything; on restores all notches.
6. Band change away and back; notches still correct.
7. Min-width readout changes when `nc` changes (§9).
8. Auto-increase widens a sub-minimum notch (§9).
9. Notch survives a live sample-rate change.
10. A slice added after notches exist inherits them (§6.3).
11. Reconnect: notches restore on Slice A (§6.3).
12. Visual notch shows no dent during MOX (§8.3).
13. Visual notch on: S-Meter MaxBin reading does not move (§8.3).
14. CTUN on with a non-zero shift: notch stays on the carrier (§4.1).
15. CW mode: a notch added at F sits at F, not F ± pitch (§1.2).

---

## 12. Build order

1. §4 in full, with `tst_notch_tune_frequency`. Four parts, all in this
   task because they share one code path: the tunefreq push from
   `bindSliceToStream` with the stream-centre value (§4.1, §4.2), the
   both-branches shift fix (§4.3), the RIT-composes-not-replaces fix
   (§4.4), and the `notchTuneFrequencyHz()` accessor (§4.5). Nothing
   else works until this lands, and §4.3 / §4.4 are live defects on their
   own merits.
2. `wdsp_api.h` declarations plus `RxChannel` wrappers, including return-
   value surfacing and `kMaxNotches` (§6.2).
3. `NotchModel` with guards, `clear()` contract, and persistence.
   `tst_notch_sideband_shift` and `tst_notch_spatial_helpers` land here:
   they are pure functions and need no channel.
4. `RadioModel` fan-out plus `syncNotchesToAllChannels()` at the
   `openRxChannelPool` tail (§6.3).
5. TCI repoint plus the both-index broadcast (§6.4). Small, and makes the
   model observable from outside before any UI exists.
6. `SpectrumWidget` marker rendering.
7. `SpectrumWidget` interaction: Ctrl + right-click add, body drag, edge
   drag with the 8 px gate, wheel, hover, both context menus (§7.1-§7.3).
8. `+TNF` button (replacing the disabled MNF stub, §9) and status-bar
   indicator plus a fixed-accelerator `QAction` (§10.2, the assignable-
   shortcut subsystem is out of scope).
9. Fill in the existing `MnfSetupPage` with the table, min-width and
   auto-increase (§9).
10. Visual notch, last, because it is the only piece that can perturb
    existing display behaviour and wants the rest stable underneath it.
    Includes the MaxBin routing decision (§8.3).
11. Author the bench matrix (§11.2) and run it.

---

## 13. Base

Implemented on `claude/tunable-notch-filter-d52e29`, which is
`main` + PR #306 + PR #291 + PR #293 + the four multi-pan audio bench-fix
commits from `claude/angry-maxwell-3a8f5b`. Verified building at
integration time. TNF therefore assumes multi-slice is present and cannot
merge to `main` ahead of #293.

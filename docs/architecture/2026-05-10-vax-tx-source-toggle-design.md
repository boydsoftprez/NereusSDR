# VAX TX-Source Toggle Button (PhoneCwApplet)

**Status:** Approved 2026-05-10. Pending implementation plan + bench.
**Scope:** Wire the existing-but-NYI VAX button on PhoneCwApplet so that left-click toggles `MicSource::Vax` as the TX audio source, and right-click jumps to Setup → Audio → TX Input. Lets the operator swap quickly between mic input and VAX without paging through Setup.
**Out of scope:** Wiring the Phone-tab `m_micSourceCombo` (Control #4, still NYI), any new Setup pages, any change to the underlying VAX TX shared-memory route or `CompositeTxMicRouter` dispatch logic.
**Source-first:** Not applicable. VAX is NereusSDR-native (see memory `feedback_vax_not_vac_port`); no Thetis equivalent to port.

## Problem

The VAX button on PhoneCwApplet (Phone/CW tab, Control #9) was added in Phase 3-VAX skeletal work and immediately marked NYI via `NyiOverlay::markNyi(m_vaxBtn, kNyiVax)` at [PhoneCwApplet.cpp:541](../../src/gui/applets/PhoneCwApplet.cpp:541). The TX pipeline already routes correctly when `MicSource::Vax` is selected (see [RadioModel.cpp:2952](../../src/models/RadioModel.cpp:2952), [AudioEngine.h:411](../../src/core/AudioEngine.h:411)), and the value already auto-persists per-MAC under `hardware/<mac>/tx/Mic_Source` ([TransmitModel.cpp:1411-1430](../../src/models/TransmitModel.cpp:1411)). The remaining gap is purely the click handler plus a way to remember which non-VAX source the user came from so the toggle-off is non-destructive.

## Goals

1. Left-click on the VAX button toggles `TransmitModel::micSource()` between `Vax` and the user's previously-selected non-VAX source (`Pc` or `Radio`).
2. Right-click opens Setup → Audio → TX Input.
3. The "previous non-VAX source" survives an app restart.
4. The button checked-state mirrors model state, so external changes (profile load, future combo wiring, MMIO) keep it in sync.

## Non-goals

- Adding any new Setup page.
- Changing the existing `Mic_Source` persistence key, schema version, or migration.
- Wiring `m_micSourceCombo` (Phone Control #4), which stays NYI.
- HL2 lock changes. The existing `setMicSourceLocked(true)` Radio→Pc clamp covers the HL2 case automatically.

## Design

### 1. `TransmitModel`: previous-source tracking + persistence

Add a tracked, persisted "last non-VAX source" alongside the existing `m_micSource`:

```cpp
// New member
MicSource m_previousNonVaxMicSource{MicSource::Pc};

// New getter
MicSource previousNonVaxMicSource() const noexcept;

// New slot
public slots:
void toggleVaxSource(bool on);
```

Behavior:

- Inside `setMicSource(source)` ([TransmitModel.cpp:2329](../../src/models/TransmitModel.cpp:2329)): after the existing lock clamp + idempotent guard, when the resulting source is non-Vax and differs from the previously tracked value, update `m_previousNonVaxMicSource` and persist it. This single hook covers every entry point (combo, profile load, pre-connect path, future MMIO) automatically.
- `toggleVaxSource(bool on)`: when `on`, calls `setMicSource(MicSource::Vax)`. When `!on`, calls `setMicSource(previousNonVaxMicSource())`. The lock guard already coerces Radio→Pc on HL2, so this works on every SKU without special-casing.

Persistence keys mirror the existing `Mic_Source` two-key pattern:

| Scope | Key |
| --- | --- |
| Per-MAC | `hardware/<mac>/tx/Mic_Source_PreVax` |
| Pre-connect global | `tx/preconnect/Mic_Source_PreVax` |

Default on first load: `Pc`. No schema migration required (absent key falls back to default).

Load is added to the same block as `Mic_Source` near [TransmitModel.cpp:1411-1430](../../src/models/TransmitModel.cpp:1411). Save mirrors the existing `setValue(pfx + "Mic_Source", ...)` and `tx/preconnect/Mic_Source` writes around [TransmitModel.cpp:1717-1723](../../src/models/TransmitModel.cpp:1717) and [:2355-2363](../../src/models/TransmitModel.cpp:2355).

### 2. `PhoneCwApplet`: wire the VAX button

In `buildUI()` at [PhoneCwApplet.cpp:541](../../src/gui/applets/PhoneCwApplet.cpp:541): remove the `NyiOverlay::markNyi(m_vaxBtn, kNyiVax)` call.

In `wireControls()`, add bindings consistent with the file's existing `m_updatingFromModel` pattern (DEXP, PROC, MON):

- `connect(m_vaxBtn, &QPushButton::toggled, &tx, &TransmitModel::toggleVaxSource)`, gated by `m_updatingFromModel`.
- `connect(&tx, &TransmitModel::micSourceChanged, this, [this](MicSource src) { ... })` to sync `m_vaxBtn->setChecked(src == MicSource::Vax)` under a `QSignalBlocker`.
- `m_vaxBtn->setContextMenuPolicy(Qt::CustomContextMenu)` plus `connect(m_vaxBtn, &QWidget::customContextMenuRequested, this, [this] { emit openSetupRequested("Audio", "TX Input"); })`. The existing `openSetupRequested` signal is reused unchanged.

Tooltip update:

> VAX digital audio input.
> Left-click: toggle VAX as the TX audio source.
> Right-click: open Setup → Audio → TX Input.

`syncFromModel()` adds `m_vaxBtn->setChecked(tx.micSource() == MicSource::Vax)` under the existing guard so initial state is correct after profile load or reconnect.

### 3. `TxApplet`: extend mic-source badge to a 3-way

The badge at [TxApplet.cpp:1300-1310](../../src/gui/applets/TxApplet.cpp:1300) and the matching block in `syncFromModel()` near [:1551](../../src/gui/applets/TxApplet.cpp:1551) currently switches binary on `Radio` vs not. Extend to a 3-way:

| `MicSource` | Badge text |
| --- | --- |
| `Pc` | `PC mic` |
| `Radio` | `Radio mic` |
| `Vax` | `VAX` |

No styling change. Tooltip stays the same.

### 4. `MainWindow`: no change

The existing `PhoneCwApplet::openSetupRequested` lambda at [MainWindow.cpp:4234-4243](../../src/gui/MainWindow.cpp:4234) already does `dialog->selectPage(page)` against the leaf-page label. `selectPage("TX Input")` resolves to the `AudioTxInputPage` registered at [SetupDialog.cpp:323](../../src/gui/SetupDialog.cpp:323).

## Edge cases

| Case | Behavior |
| --- | --- |
| HL2 connect (locked to Pc) | Lock blocks Radio at the model level. `previousNonVaxMicSource` updates to `Pc` whenever the lock coerces Radio→Pc. Toggle-off after Vax lands on `Pc`. |
| Profile load while Vax active | `MicProfileManager::applyProfile` calls `setMicSource(...)` ([MicProfileManager.cpp:1337](../../src/core/MicProfileManager.cpp:1337)). The `micSourceChanged` signal updates the button checked-state; if the loaded source is non-Vax, `previousNonVaxMicSource` advances. |
| Pre-connect VAX click | Pre-connect path persists to `tx/preconnect/Mic_Source` today; the new key follows the same path so the previous source survives the first connect. |
| Disconnect → reconnect different MAC | New per-MAC keys load. `Mic_Source_PreVax` defaults to `Pc` if unset. |
| External change to Vax via combo (future) | The single `setMicSource` hook picks it up. The button checked-state mirrors the model. No combo-specific code needed. |

## Tests

Unit tests on `TransmitModel` (new file `tests/models/test_TransmitModel_VaxToggle.cpp`):

1. From `Pc`: `toggleVaxSource(true)` → `Vax`; `toggleVaxSource(false)` → `Pc`.
2. From `Radio`: `setMicSource(Radio); toggleVaxSource(true)` → `Vax`, `previousNonVaxMicSource() == Radio`; `toggleVaxSource(false)` → `Radio`.
3. HL2 lock: `setMicSourceLocked(true); setMicSource(Radio)` clamps to `Pc`; `toggleVaxSource(true); toggleVaxSource(false)` lands on `Pc`.
4. Persistence round-trip: `Mic_Source_PreVax = "Radio"`, reload, `previousNonVaxMicSource() == Radio`.
5. Combo-style external write: `setMicSource(Radio); setMicSource(Vax); setMicSource(Pc)` advances `previousNonVaxMicSource` to `Pc` (each non-Vax write feeds the tracker).

Widget test on `PhoneCwApplet` (extends existing PhoneCwApplet test fixture):

6. Click VAX → `tx.micSource() == Vax` and button checked.
7. Click VAX again → `tx.micSource()` returns to the previous source and button unchecked.
8. Right-click → `openSetupRequested("Audio", "TX Input")` emitted exactly once.

## Risks

- **Profile XML round-trip:** `MicProfileManager` already writes `Mic_Source` into profile XML. Profiles do **not** need to carry `Mic_Source_PreVax`; it is per-installation user state, not per-profile state. Verify the profile loader does not stomp the new key.
- **Test fixtures that construct `TransmitModel` without an `AppSettings`:** the new persistence path must use the same `s.contains(...)` guard the existing `Mic_Source` block uses, so headless tests stay clean.

## Verification

- `cmake --build build` clean.
- `ctest` green incl. the 5 new TransmitModel cases and 3 new PhoneCwApplet cases.
- Manual on Linux + ANAN-G2:
  1. Connect, click VAX on PhoneCwApplet → TxApplet badge reads `VAX`, mic input audible via `pavucontrol` on the VAX TX virtual sink.
  2. Click VAX off → badge returns to `PC mic` (or `Radio mic` if Radio was active first).
  3. Right-click VAX → Setup opens at Audio → TX Input.
  4. App restart with VAX on → button still reflects model state, click off restores prior source.
- Manual on HL2: VAX toggle works, off lands on `Pc` regardless of prior state.

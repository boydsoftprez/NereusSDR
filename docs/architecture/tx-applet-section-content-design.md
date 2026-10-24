# TX Applet Section Content: Design

**Date:** 2026-10-19
**Author:** Brian Sutton, AI-assisted via OpenCode
**Status:** Implemented; one commit on `feature/tx-section-content`.
**Base:** `a8080df45` (the design-doc commit of the TX applet section split).
**Companion to:** `docs/architecture/tx-applet-section-split-design.md`.

---

## 1. Objective (plain English)

The TX applet was split into seven section objects, one per row of the TX
panel. Each section owns its widgets; the applet keeps the wiring.

That split made the applet smaller, but it did not change what the operator
can do. The seven rows still live in one panel, in one column, and the
operator still cannot pull the MON controls out on their own and put them
somewhere else.

This change makes each row available as **container content**. A container
can hold the TX Monitor controls on their own, or the TX Power row, or the
VOX row, and so on, placed wherever the operator wants them.

The TX applet is **unchanged**: it still shows every row together, exactly
as before. Nothing is moved out of it. The rows in a container are
**copies**, made and wired by the applet, so a copy shows and sets the same
model state as the applet's own row and follows the same permission gates.

## 2. What was done

Seven new content types, one per section:

| Content type | Title | Rows it carries |
|---|---|---|
| `tx.monitor` | TX Monitor | MON toggle, SPEAKERS / PHONES output pair, the "why MON is silent" notice, the monitor volume slider |
| `tx.power` | TX Power | the forward-power and SWR gauges, the RF Power row, the Tune Power row |
| `tx.filter` | TX Filter | the TX bandpass low/high cutoff spinboxes, the filter status label |
| `tx.chain` | TX Chain | the LEV, EQ and CFC buttons, the TX Profile combo |
| `tx.vox` | TX VOX | VOX toggle, threshold slider, the live peak meter, the hold-time slider and its readout |
| `tx.processing` | TX Processing | the 2-Tone, PS-A and DUP buttons, the SWR protection LED inset |
| `tx.keying` | TX Keying | the TUNE and MOX buttons, the transmit-slice letter row, the "who holds transmit" label, the Take transmit button |

They are reached from **Container Settings → Add item → TX**.

## 3. The rules that kept it behaviour-preserving

**1. Copies, not moves.** The applet's own panel is untouched: it builds the
same seven sections, shows the same rows, in the same order, with the same
styles and tooltips. A container's copy is a second, independent instance of
a section, parented to the container.

**2. The applet stays the wiring authority.** The applet makes each copy and
wires it, so the copy's controls mean exactly what the applet's own mean:
the same model, the same signals, the same echo guard. The copy is not a
mirror of the applet's widgets — it is a second set of widgets bound to the
same model.

**3. The wiring moved into a per-instance method.** Each section's model
wiring now lives in `wireXSect(TxXSection* s)` on the applet, where `s` is
the instance being wired. The applet's own row and a container's copy go
through the same code, so they cannot drift apart.

**4. The permission gates reach every copy.** The transmit-permission layers
(`setTransmitPermitted`, `setTransmitSettingsPermitted`,
`setTransmitChainSettingsPermitted`, `setVoxPermitted`,
`setTxProfilePermitted`, `setPureSignalArmingPermitted`) and the receive-only
lock all gate the applet's own controls *and* every copy's. A copy in a
container is greyed out under exactly the same conditions as the applet's
own row, with the same reason in its tooltip.

**5. The state pushes reach every copy.** Where the applet pushes state into
a row — the power gauges' readings, the VOX peak meter, the monitor volume,
the LEV / EQ / CFC states, the filter cutoffs, the profile combo, the 2-Tone
and PS-A buttons, the TUNE and MOX buttons, the transmit-slice row, the
holder label — it now pushes into the applet's own row and into every copy.
A copy therefore always shows what the model says, not what it said when
the copy was made.

**6. The applet's public API is unchanged.** `moxButton()`, `tuneButton()`,
`profileCombo()`, `twoToneButton()`, `takeTransmitButton()`,
`transmitSliceButtons()`, `rfPowerSlider()`, `tunePowerSlider()`,
`voxButton()`, `fwdPowerGauge()`, `swrGauge()` and the rest still return
the applet's own widgets, so the rest of the app and the tests are
unaffected.

## 4. How a copy is made

```cpp
TxMonitorSection* TxApplet::createMonitorSection(QWidget* parent)
{
    auto* s = new TxMonitorSection(parent, txRowSpacing());
    wireMonitorSection(s);          // the same wiring the applet's own row gets
    m_extraMonitorSections.append(s);
    syncSectionInstances();         // the copy starts where the model is
    return s;
}
```

`txRowSpacing()` is the applet's own column spacing, handed in as the
section split already does, so a copy in a container has the same internal
row gaps as the row in the panel.

### 4.1 The copy starts where the model is

A copy is made *after* the applet has already pushed the model state into
its own row, so a copy made on its own would start blank — every button
unchecked, every slider at its default. `syncSectionInstances()` is the
one method that pushes the state the applet holds into every row, and the
applet calls it when it makes a copy. It re-applies:

- the model state (`syncFromModel()`): the power sliders, the VOX row, MON
  and the monitor volume, the LEV / EQ / CFC buttons, the filter cutoffs
  and the TUNE / MOX buttons;
- the TX Profile combo's entries;
- who holds transmit, and the offer to take it;
- the full-duplex button;
- the 2-Tone button's own state;
- the PS-A button, which the PureSignal facade owns.

The applet remembers the last value it pushed for the three pieces of
state that come from outside the model (the holder text, the take-transmit
offer and the full-duplex button), so it can put them back.

The echo guard (`m_updatingFromModel`) is now saved and restored rather
than set and cleared, so a sync that happens inside another one — a copy
made while the applet is syncing — cannot clear the outer sync's guard
early.

### 4.2 A copy that the container destroys

The copy belongs to the container, which owns it. When the operator closes
the container, or removes the item from it, the copy is destroyed — but the
applet's timers (the 20 Hz power-gauge refresh, the 100 ms VOX peak
poller) and its permission gates walk a list of copies. A list entry
pointing at a destroyed copy is a use-after-free, and it crashed the
application.

The list therefore holds `QPointer`s, so a copy that is gone drops out on
its own, and each copy's `destroyed` signal removes its entry so the list
does not grow as copies come and go. Everything that reads the list — the
gates, the receive-only lock, the state pushes — reads it through the
`allXSections()` helpers, which return the applet's own row and every copy
that is still alive.

### 4.3 A copy's own margins

Each TX section's layout has no margins of its own: all the padding in the
TX panel comes from the applet's *column* — 4 px at the sides, 2 px above
and below, with the column's spacing between the rows.

A copy in a container has no column of its own, and the container host's
body layout has a spacing of zero, so a copy would sit flush against its
neighbours and against the container's edges. That is why the host's other
per-instance content — the Longpath instruments — each carry their own
outer margins (`BarInstrument` and `NeedleInstrument` use 10/8/14/10,
`FrequencyInstrument` 14/10/14/10).

So a section is told the margins it gives itself. The applet's own rows are
told to give none, because their column already has them; a copy is told to
give itself the column's margins, so a TX object in a container has the
same breathing room the row has in the panel. Two adjacent TX objects then
sit 4 px apart (2 + 2), rather than the panel's 6 px.

The margins are the section's own layout margins, so they sit outside the
row's own controls; the spacing *between* the rows inside a section is
unchanged, and is already handed in as the same value the applet uses.

## 5. How the content is registered

The new content registers through the registry's **widget factory** path,
the same path `LongpathContent` uses for the Sources bundle. The factory
builds a fresh instance each time the operator places the content, so two
containers can hold the same row independently.

No change to the registry was needed: `descriptors()` appends the widget
factories at the end of its list, so the new types appear in the menu
without touching the registry itself.

### 5.1 Compiled in, not a plugin

There is no plugin mechanism for container content. Every content type in
the application — the meters, the indicators, the control bars, the Longpath
instruments and these TX rows — is compiled into the library and
registered at runtime by a `registerWidgetFactory` call. `TxSectionContent.cpp`
is one of the `GUI_SOURCES` in `CMakeLists.txt`, so it is part of
`libNereusSDRLib`; `MainWindow::populateDefaultMeter()` is what registers
the rows, in the same block that registers the Longpath instruments.

The rows are registered only while the workspace preview is on
(`WorkspacePreview::enabled()`, which is on by default and off only when
`NEREUS_WORKSPACE_PREVIEW=0`). The operator adds a row from Container
Settings' available-items list with "Add ->", which lists what the registry
has. (The "+" add-item popup is hidden whenever the workspace edits a
container, so it never offers the rows.)

## 6. Verification

- **A test for the feature.** `tests/tst_tx_section_content.cpp` covers the
  things that matter: the registry offers all seven row types; a copy is
  its own instance, and two copies are two instances; the copy shows the
  model state; the copy's control sets the model and the applet's own row
  follows (and the other way round); the applet's own panel still has every
  row; the transmit-permission gate reaches the copy; the registry is the
  authority on what the workspace can build; and the operator adds a row
  from the available-items list.

  The test found two real faults while it was being written, both now
  fixed: a copy started blank instead of showing the model state, and
  because of that its first press moved the model the wrong way. The
  `syncSectionInstances()` method and the saved-and-restored echo guard are
  the fixes.

  A third fault was found in the field: the application crashed when the
  operator closed a container holding a copy. The copy belongs to the
  container and is destroyed with it, while the applet's list of copies
  kept a raw pointer to it, so the applet's next timer tick used a
  destroyed widget. The test case that reproduces it —
  `aCopyTheContainerDestroysLeavesTheAppletsList()` — exits with
  `0xC0000005` (an access violation) against the unfixed code and passes
  against the fixed code. The fix is the `QPointer` list and the
  `destroyed` connection described in section 4.2.

- **Every model connection is accounted for.** The number of `connect()`
  calls in `TxApplet.cpp` that touch a section's widgets is the same before
  and after the change (49 at the baseline, 49 after). The only two that
  differ in their bodies are the two that were deliberately changed to
  reach the copies: the active-profile push, and the 2-Tone active push.
  Nothing was lost and nothing was connected twice.
- **Each section is wired exactly once.** The wiring was moved out of
  `wireControls()` into the per-instance methods, and `wireControls()` now
  calls each one once. A section wired twice would double every
  connection; a check of the call sites confirms one call per section.
- **The full GUI test suite.** 536 tests, `-L gui`, excluding `realtime` and
  `native-window`. The baseline (the section-split commit) fails 31; the
  feature branch fails the same 31. No test that passed at the baseline
  fails here.
- **The build.** The library, the app and the test suite all build clean,
  apart from the nine pre-existing MinGW link failures in the WDSP-linked
  tests, which fail the same way at the baseline.

## 7. What did not change

- The TX applet's panel, its rows, their order, their styles, their
  tooltips, their object names, and their behaviour.
- The applet's public API.
- The container content registry, the container settings dialog's layout,
  and every other content type.
- The remote-window behaviour, the model layer, and the Core.

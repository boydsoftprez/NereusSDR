# TX Applet Section Split: Design

**Date:** 2026-10-19
**Author:** Brian Sutton, AI-assisted via OpenAI Codex
**Status:** Implemented; seven commits on `release-2026.10.1`.
**Base:** `acac02734` (`Merge pull request #489 from boydsoftprez/claude/docs-release-2026-10-1`),
the `v2026.10.1` release point.
**Thetis source pin:** `[v2.10.3.13]` (the `console.cs` ranges the TX applet's
provenance cites).

---

## 1. Objective (plain English)

`TxApplet` had grown to 3,384 lines in one `.cpp` and 795 in one `.h`. It
built every row of the TX control panel inline, wired them to the model,
and applied the transmit-permission gates to them — all in the same class.
The applet was the largest file in the GUI, and every change to a TX
control meant editing a file where the widget, the model wiring and the
permission gate were tangled together.

This splits the applet into seven **section objects**, one per row of the
panel. Each section owns its widgets and their presentation. The applet
keeps the wiring: what each control means to the model, which slice
transmits, the signals the buttons raise, and the transmit-permission
gates.

Nothing about the panel changes. Same rows, same order, same styles, same
tooltips, same object names, same spacing, same behaviour.

## 2. What was done

Seven sections, one per commit:

| Section | Rows it owns |
|---|---|
| `TxMonitorSection` | MON toggle, SPEAKERS / PHONES output pair, the "why MON is silent" notice, the monitor volume slider |
| `TxVoxSection` | VOX toggle, threshold slider, the live `DexpPeakMeter` strip, the hold-time slider and the two inset readouts |
| `TxPowerSection` | the forward-power and SWR gauges, the RF Power row, the Tune Power row |
| `TxKeyingSection` | the TUNE and MOX buttons, the transmit-slice letter row, the "who holds transmit" label, the Take transmit button |
| `TxChainSection` | the LEV, EQ and CFC buttons, the TX Profile combo |
| `TxFilterSection` | the TX bandpass low/high cutoff spinboxes, the filter status label |
| `TxProcessingSection` | the 2-Tone, PS-A and DUP buttons, the SWR protection LED inset |

`TxApplet.cpp` went from 3,384 to 2,599 lines; `TxApplet.h` from 795 to 756.
Fourteen new files, 1,449 lines.

## 3. The rules that kept it behaviour-preserving

**1. Moved code is moved verbatim.** Every section's `buildUI` body was
extracted mechanically from `TxApplet.cpp` and compared byte-for-byte
against the original before it went near a build. Nothing was retyped.
(The one deliberate exception is the DUP button's `clicked` connect,
which raises `TxApplet::displayDuplexRequested` and therefore cannot live
in the section. The Take transmit button's connect is the same case.)

**2. Object names are unchanged.** The stylesheet and the tests find these
widgets by name, so every `setObjectName` travels with its widget:
`TxVoxButton`, `TxHolderLabel`, `TxTakeTransmitButton`, `TxSliceButton%1`,
`TxLevButton`, `TxAppletPsaBtn`, `TxAppletDupBtn`, `xpaInset`, and so on.

**3. The row spacing is handed in, not assumed.** The applet's column uses
`WorkspacePreview::enabled() ? 6 : 2` pixels between rows. A section that
hard-coded a spacing would look right in one mode and wrong in the other,
so each section is given the column's own spacing and uses it for the
rows inside it.

**4. The applet's public API is unchanged.** `moxButton()`, `tuneButton()`,
`profileCombo()`, `twoToneButton()`, `takeTransmitButton()`,
`transmitSliceButtons()`, `rfPowerSlider()`, `tunePowerSlider()`,
`fwdPowerGauge()` and the rest stay on `TxApplet` and forward to their
section. `MainWindow.cpp` and the test suite call them.

**5. Decisions stay with the applet, mechanics go to the section.** Where
a row is built from live data, the section provides the mechanics and
the applet supplies the answers. The slice letter row is the clearest
case: the section makes the buttons and lays them out; the applet decides
which slices this window controls, which one is current, what each says,
and what a press does.

## 4. What stayed in `TxApplet`, and why

- **The mic-source badge.** It sits at row 0, above the gauges, while the
  chain rows sit below four other sections. Giving it its own widget
  would have meant moving it in the panel. One label was not worth the
  layout risk, so it stays.
- **The CFC dialog** (`m_txCfcDialog`) and the TX Profile page. These are
  dialogs, not widget rows; they are constructed with the model and the
  TX channel, so they belong to the applet's wiring.
- **The profile combo's contents.** `rebuildProfileCombo()` reads
  `MicProfileManager`. The section owns the combo; the applet decides
  what goes in it.
- **The right-click connects.** EQ opens the EQ dialog, CFC opens the CFC
  dialog, the profile combo opens the TX Profile page, and DUP raises
  `displayDuplexRequested`. All are applet slots, so all stay in the
  applet and are wired to the section's widgets.

## 5. Verification

**Test suite.** `ctest -L gui -LE "realtime|native-window"`, 536 tests.

| | failures |
|---|---|
| untouched tree, before any of this | 32 |
| after all seven sections | 31 |

No test that passed before fails now. The one that stopped failing is
`tst_tci_remote_window`, which is flaky (see below).

**Targeted suites.** Every TX, filter, chain, keying, power, monitor and
processing test passes. The two TX failures that remain
(`tst_tx_eq_dialog`, `tst_tx_applet_vox_relocated`) failed on the
untouched tree too.

**Attribution.** The upstream Thetis `console.cs` header is carried
across character-for-character, extracted programmatically rather than
retyped, with its `MW0LGE` / `W2PA` / dual-licensing anchors verified
present. All fourteen new files are registered in
`docs/attribution/THETIS-PROVENANCE.md`.

**Signature.** Every commit is GPG-signed and verifies with
`git verify-commit`.

## 6. Two source-text assertions had to follow the code

`tst_operator_wording_sweep` checks that particular operator-facing
strings live in particular source files. Two of them moved:

- *"This Core does not send the transmit monitor. Updating the Core may
  help."* — `TxApplet.cpp` → `TxMonitorSection.cpp`
- *"TX Leveler: slow speech-leveling AGC. Improves intelligibility on
  weak speech."* — `TxApplet.cpp` → `TxChainSection.cpp`

The expectations were updated to point at the files that now hold the
strings. This is the sweep doing its job, not a workaround.

## 7. Notes for the maintainer

Three findings came out of this work that are independent of the split
and are worth fixing separately. Each is reproducible on Windows with a
stock Qt 6.8.3 / MinGW 13.1 setup.

**7.1 `verify-thetis-headers.py` cannot pass on Windows.** The
orphan-pair check compares `str(Path(...).with_suffix(...))` — which yields
backslashes on Windows — against the paths parsed from the PROVENANCE
table, which are forward-slashed. The comparison never matches, so every
`.h`/`.cpp` sibling pair is reported as an orphan. 383 of the 454 header
check failures are this one bug. `TxApplet.cpp`/`.h`, which have been in
PROVENANCE since long ago, fail it exactly like the new files do.
Comparing `PurePath` values instead of strings would fix it.

**7.2 `tst_dsp_control_receive` cannot link on MinGW.** It links
`NereusCore` and `libwdsp_static.a`, and `NereusCore` already contains
WDSP, so `dsplock.c.obj` and `wcpAGC.c.obj` are pulled in and collide
with Core's copies:

```
multiple definition of `WDSPSetTestBlockDelayUs'
multiple definition of `GetRXAAGCTop'
```

Nine test targets are affected.

**7.3 That link failure silently corrupts the test suite.** Because
`cmake --build build --target tests_gui` stops at the first failing
target, the remaining ~500 test executables are never relinked. They then
run against a newer library with a changed class layout, which shows up
as heap corruption and segfaults in tests that have nothing to do with
the change. `cmake --build build --target tests_gui -- -k 0` avoids it.
This cost an hour of chasing what looked like catastrophic code damage.

**7.4 Three tests in the `gui` suite are flaky under parallel load.**
`ctest -j 4` makes them fail intermittently, and they pass in
isolation:

- `tst_remote_step_attenuator` — runs at 4.7 s against a 5 s `QTRY`
  timeout
- `tst_tci_remote_window` — segfaults, passes, or fails across runs
- `tst_remote_connection_controls` — fails at 23 s, passes at 49 s

The `gui` suite's failure count therefore moves by one or two between
runs, and a failure count on its own cannot distinguish a regression
from a flake. A wider `QTRY` budget, or a faster test, would settle it.

## 8. Attribution

The new files are derived from `TxApplet`, which the project records as
ported from Thetis `console.cs` (multi-source, with the layout from
AetherSDR). The provenance rows carry the same source cites and line
ranges as the `TxApplet` rows they came from. The per-file Thetis header
is preserved verbatim, as `docs/attribution/HOW-TO-PORT.md` requires.

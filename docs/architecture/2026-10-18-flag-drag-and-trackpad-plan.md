# Receiver grab area and trackpad tuning: plan

Execution: run with `crew` under `cost-aware-execution`. One task. Bounded change
designed with JJ on 2026-10-18 in the session "Flag drag hitbox and trackpad tuning";
no separate spec.

## Goal

1. Drag-to-tune is easy to grab: the whole receive band, a wide strip around the
   receiver's center line and its triangle, and the flag box itself, for every receiver
   this window owns on the pan.
2. Two-finger trackpad scrolling tunes, zooms and moves the reference level smoothly
   instead of jumping many steps per swipe.

## What is wrong today (read before changing anything)

- `SpectrumWidget::mousePressEvent` (src/gui/SpectrumWidget.cpp, the "Filter edge
  grab" and "Inside passband" blocks near the `kFilterGrab` uses) tests only the front
  receiver (`m_vfoHz`, `m_filterLowHz`, `m_filterHighHz`). The edge zones are
  `kFilterGrab = 5` px on both sides of each edge, and slide-to-tune needs
  `mx > left + 5 && mx < right - 5`, so a band narrower than 11 px has no drag area at
  all. The hover cursor block in `mouseMoveEvent` repeats the same test.
- The VFO drag in `mouseMoveEvent` puts the receiver's center under the cursor
  (`hz = xToHz(mx)`), so grabbing the band away from its center makes it jump.
- `VfoWidget` (src/gui/widgets/VfoWidget.cpp) has no drag at all. Its
  `mousePressEvent` activates the slice and, on press, collapses (slice badge),
  expands (folded flag), clicks the folded TX badge, or opens the frequency editor on
  double-click.
- `SpectrumWidget::wheelEvent` and `VfoWidget::wheelEvent` turn every wheel event into
  one full step (`delta > 0 ? 1 : -1`). A Mac trackpad sends dozens of small events per
  swipe plus momentum events after the fingers lift, so one flick moves 30 or more
  steps. Cmd+scroll zoom (0.8x per event), Shift+scroll reference level (5 dB per
  event), the selected-notch width wheel and the dBm-strip wheel (`/ 120`, which a
  trackpad's small deltas truncate to 0 or hit erratically) are affected the same way.

## Reference: AetherSDR, pinned at `1e0718ad`

- Grab rules: `src/gui/SpectrumWidget.cpp:360-361` (`kFilterEdgeGrabPx = 8`,
  `kFilterPassbandMinBodyPx = 6`), `:519-564` (`filterInteriorGrabPx`,
  `filterEdgeHitAtPixel`, `filterPassbandBodyHitAtPixel`), `:9154-9208`
  (`sliceCursorShapeAt`), `:9920-10009` (press: inactive slices first, then the
  active one; `m_vfoDragOffsetHz` keeps the grab offset), `:10029-10038`
  (`driveVfoDragTune`: `snapToStep(xToMhz(mx) - offset, step)`).
- Wheel: `src/gui/SpectrumWidget.cpp:11286-11328`.

## Global Constraints

- Read `CLAUDE.md`, `CONTRIBUTING.md` and `docs/attribution/HOW-TO-PORT.md` first. Code
  taken from AetherSDR carries `// From AetherSDR file:lines [@1e0718ad]` cites. A new
  file holding ported logic gets the AetherSDR upstream file header byte-for-byte plus a
  "Modification history (NereusSDR)" block (2026-10-18, JJ Boyd, Claude Opus 5.5), and
  the provenance row HOW-TO-PORT names. Existing inline comments stay; don't remove code
  you didn't add.
- C++20 / Qt6 style: no raw `new`/`delete` (Qt parent or `unique_ptr`), `constexpr`
  constants named `kPascalCase`, braces on all control flow, `m_camelCase` members,
  `Q_OS_MAC` style guards only.
- American spelling in all new comments, test names and strings. No em-dashes in new
  text.
- Do not change colors, cursors' artwork, the flag's layout, or anything this plan does
  not name. Existing cursor shapes keep their meaning: `SizeHorCursor` over a grab area
  and during a drag, `CrossCursor` elsewhere.
- No tune while this pan shows transmit: every new tune path goes through the same gate
  as `SpectrumWidget::requestTune` (`m_moxOverlay`). A locked slice (`m_locked`) or one
  this window only listens to (`isListening()`) never tunes from a drag, exactly as the
  flag's wheel refuses today.
- Build dir: `build/` in this worktree is already configured (tests on) and is the only
  build dir. Do not create another and do not reconfigure it from the network. Build
  single test targets; never the whole suite. Run tests with ctest (it sets offscreen
  and the per-test home); for a direct binary run set `HOME`, `CFFIXED_USER_HOME` and
  `NEREUS_TEST_HOME` to one scratch folder and `QT_QPA_PLATFORM=offscreen`.
- Commits are GPG signed; never `--no-gpg-sign`. No Co-Authored-By trailer.

## Task 1: Wide receiver grab, flag drag, smooth trackpad

Implements the Goal above, all three parts, in one change.

**Files**

- New: `src/gui/WheelStepAccumulator.h`, `src/gui/WheelStepAccumulator.cpp` (add to
  the library sources in `CMakeLists.txt` where neighboring `src/gui` files are listed).
- `src/gui/SpectrumWidget.h`, `src/gui/SpectrumWidget.cpp`
- `src/gui/widgets/VfoWidget.h`, `src/gui/widgets/VfoWidget.cpp`
- Tests: `tests/tst_wheel_step_accumulator.cpp`, `tests/tst_spectrum_receiver_grab.cpp`,
  `tests/tst_vfo_flag_drag.cpp`, registered in `tests/CMakeLists.txt` the way
  neighboring tests are.
- `docs/attribution/` provenance row for the new file.

**Interfaces**

```cpp
// src/gui/WheelStepAccumulator.h
class WheelStepAccumulator {
public:
    // -1, 0 or +1 for this event. Trackpad (pixelDelta not null): ignores
    // Qt::ScrollMomentum, ignores events whose |x| > |y|, resets the pixel
    // accumulator on Qt::ScrollBegin, one step per kTrackpadPixelsPerStep of
    // accumulated y, clamped to +/-1 per event, and at most one step per
    // kTrackpadMinStepIntervalMs. Mouse wheel (pixelDelta null): accumulates
    // angleDelta().y(), one step per kAngleUnitsPerStep, clamped to +/-1 per
    // event, no rate limit.
    int stepsFor(const QWheelEvent& event);
    // Test seam: elapsed-time source in ms; defaults to a QElapsedTimer.
    void setClockForTest(std::function<qint64()> nowMs);
    static constexpr int kTrackpadPixelsPerStep = 15;      // AetherSDR :11297
    static constexpr int kTrackpadMinStepIntervalMs = 50;  // AetherSDR :11305
    static constexpr int kAngleUnitsPerStep = 120;         // AetherSDR :11317
};
```

Each `SpectrumWidget` and each `VfoWidget` owns one accumulator member.

**Steps (what must be true)**

1. Spectrum grab, for every flag this pan shows (`sliceMarkerGeometry()` gives each
   receiver's center, filter edges, flag and whether it is the selected one):
   - Edge zones follow AetherSDR's `filterEdgeHitAtPixel` with 8 px outside the band;
     inside the band each edge zone shrinks so at least 6 px of band body remains
     (a band 6 px wide or narrower has no edge zone inside it). Replace `kFilterGrab`'s
     use in the press and hover paths; keep the constant only if something else still
     needs it.
   - Slide-to-tune covers the band body (everything inside the band that is not an edge
     zone) plus a strip 8 px either side of the center line over the whole spectrum
     height. The strip also covers the triangle under the flag.
   - The selected receiver is tested first and wins any overlap; then the others in
     paint order (the topmost drawn first). An unselected receiver's edges do not
     resize it: its edge zones, band and center strip all start a drag-to-tune.
   - Pressing an unselected receiver's grab area makes it the selected receiver
     (through its flag's existing activation, `sliceActivationRequested`) and the same
     press-and-drag tunes it.
   - The drag keeps the grab offset: on press record `offsetHz = xToHz(mx) -
     receiverCenterHz`; each move tunes to `round((xToHz(mx) - offsetHz) / step) * step`.
     A press with no movement does not retune.
   - The notch grab, dBm strip, divider, frequency scale, LIVE button and time-scale
     strip keep their current precedence ahead of all of this; pan drag stays the
     fallback.
   - The hover cursor uses the same hit test as the press (one helper, not two copies).
2. Flag drag (`VfoWidget`):
   - A left press on the flag's own surface (its background and its non-interactive
     labels, the frequency readout included) followed by movement past
     `QApplication::startDragDistance()` horizontally starts a drag that tunes this
     flag's slice by the cursor's horizontal travel, converted with the pan's Hz per
     pixel and snapped to the step, keeping the offset as in step 1. Buttons, sliders,
     combos and the frequency editor keep their own presses; a press on them never
     starts a drag.
   - The spectrum owns the pixel-to-Hz mapping and the tune: the flag reports drag
     start, move (global x) and end; `SpectrumWidget` (which creates the flags in
     `addVfoWidget`) converts and tunes. Pick the cleanest wiring; signals on
     `VfoWidget` connected in `addVfoWidget` are expected.
   - A press that becomes a drag does not collapse, expand, click the folded TX badge
     or enter edit mode. A press and release without crossing the threshold does
     exactly what a click does today (activation, collapse on the slice badge, expand
     when folded, folded TX badge, double-click edit). Moving the collapse/expand
     decision from press to release is acceptable; the activation on press stays.
   - Works on the folded flag too (except a press on its TX badge, which stays a click).
   - While dragging, the flag follows its slice as it already does when the slice
     tunes (no new positioning code).
3. Trackpad:
   - `SpectrumWidget::wheelEvent` asks its accumulator once, at the top, for the step
     count, and every branch (selected-notch width, dBm strip range, Cmd/Ctrl zoom,
     Shift reference level, plain tune) uses that count in place of its own
     `angleDelta` reading. Zero steps: accept the event and do nothing. The dBm strip
     branch moves 5 dB per step (it used `notches = delta / 120`).
   - `VfoWidget::wheelEvent` does the same for its tune.
   - Any other wheel handler inside the flag (check `VfoWidget::eventFilter` and the
     flag's child widgets) that tunes gets the same treatment; report what you found.

**Acceptance cases**

- Accumulator (`tst_wheel_step_accumulator`), with synthesized `QWheelEvent`s and the
  test clock:
  - trackpad y deltas 5, 5, 4 give 0, 0, 0; a further 1 gives +1; -15 gives -1.
  - a single trackpad event of 120 px gives +1, not 8.
  - momentum-phase events of 30 px give 0.
  - an event with pixelDelta (20, 5) gives 0.
  - two +15 px events 10 ms apart give +1 then 0; a third 60 ms after the first
    gives +1.
  - ScrollBegin clears a leftover 14 px: after +14 then ScrollBegin then +14, still 0.
  - mouse wheel angle deltas 40, 40, 40 give 0, 0, +1; 120 gives +1; 960 gives +1;
    two 120s 1 ms apart give +1, +1 (no rate limit on a mouse).
- Spectrum grab (`tst_spectrum_receiver_grab`), on a sized offscreen `SpectrumWidget`
  with flags added, pressing and moving with `QTest`/sent mouse events and observing
  `frequencyClicked` / the flag's tune signal / `filterEdgeDragged`:
  - a band 4 px wide: a press at its center and a move of +20 px tunes (today it pans).
  - a band 30 px wide: a press 3 px inside its low edge resizes the low edge
    (unchanged); 7 px outside the low edge also resizes it; 9 px outside pans.
  - a press 6 px right of the center line, above the band area, tunes; the first move
    of +10 px tunes by 10 px worth of Hz from the receiver's own center (no jump to the
    cursor).
  - press and release with no move: no tune.
  - two receivers, B unselected: a press on B's band selects B and the drag tunes B,
    not A. Where A's and B's bands overlap, A (selected) wins.
  - with the transmit overlay on (`m_moxOverlay` via its public setter), a drag tunes
    nothing.
- Flag drag (`tst_vfo_flag_drag`):
  - press on the flag's frequency readout, move 30 px right: the slice tunes up by
    30 px of Hz, snapped to the step; release.
  - press and release on the slice badge with a 2 px wobble: the flag collapses as
    today; with a 30 px move it does not collapse and tunes.
  - a folded flag: press and release expands it; press and drag tunes and stays folded.
  - a locked slice and a listened slice: dragging tunes nothing.
  - a press on a flag button (for example the mode or AGC control) and a 30 px move
    tunes nothing.
- The existing tests that touch these widgets still pass:
  `tst_vfo_flag_fold`, `tst_vfo_widget_tx_badge_click`, `tst_vfo_widget_slice_access`,
  `tst_notch_hit_test`, `tst_pan_flag_positions`, `tst_spectrum_widget_display_signals`,
  `tst_spectrum_overlay_panel`, `tst_overlay_flyout_above_flags`. Find and run any other
  test that sends wheel or mouse events to `SpectrumWidget` or `VfoWidget`.

**Verification**

Software: the three new tests and the existing list above, built as single targets and
run with ctest, all green; quote the summary lines. The app target `NereusSDR` builds.
UI interaction: the feel on a real trackpad and mouse is JJ's to judge on the dev build
(pending until he tries it); say so in the report. Hardware: not applicable.

Model: opus.

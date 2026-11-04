# Roundtable overlay redesign Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** Build the approved Roundtable overlay on desktop, iPhone, iPad and the web page: truthful doubles, a meter under every reading, and tags that view a frequency without tuning.

**Architecture:** Each client derives one structured card from its existing model; drawing code only draws. Reading another frequency uses the existing `readAt` and `read(_:)` paths. No server or wire change.

**Tech Stack:** Qt6/C++20 desktop (`QPainter` on the GPU tag layer and the CPU path), SwiftUI on iPhone and iPad, plain JavaScript and CSS on the web page, Python and Node for the page tests.

**Spec:** [2026-11-03-roundtable-overlay-design.md](2026-11-03-roundtable-overlay-design.md). Every decision (D1 to D33) is settled there; this is how it gets built. The reference for every look is [board.html](2026-11-03-roundtable-overlay/board.html), Proposal look, with sources `overlay.js`, `overlay.css` and `web.css` beside it. Read those sources, not the 3 MB built file.

## Global Constraints

* Work in `/Users/j.j.boyd/NereusSDR-crew/2026-11-03-roundtable-overlay/trunk`, branch `claude/roundtable-overlay`. Never in another tree.
* Commits are GPG signed (`git commit -S`). No `Co-Authored-By` trailer of any kind. Commit only your own task's files, by name.
* No server, wire protocol, I/Q, WDSP, audio or transmit change. `roundtable/server/nereus_roundtable/` and `roundtable/tests/test_frequency.py` stay untouched.
* Connecting never keys. Nothing in this work may key, unkey or retune except the explicit Tune actions and the band click.
* `AppSettings`, never `QSettings`. No new settings keys.
* Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
* C++: no `goto`, no raw `new`/`delete` (unique_ptr or Qt parent), no `#define` constants (`constexpr`), braces on all control flow, `auto` only when obvious, `kPascalCase` constants, `m_camelCase` members, `qCWarning(lcCategory)` for errors, no exceptions. Don't remove code you didn't add unless this plan says to replace it.
* New source files start with `// no-port-check: NereusSDR-original.` then the repository's banner comment, as `src/gui/roundtable/RoundtableTagLayout.cpp` does. Nothing here is ported.
* User-visible strings carry no cites and no internal names. Plain American English. No em-dashes in strings, comments or docs.
* Disabled, never hidden: a control that cannot act right now is greyed, not removed.
* Tag and card updates must never invalidate the static overlay. Do not add work to the paint path that allocates per row per frame beyond what exists; update values in place.
* The build directory `build/` in the trunk is already configured offline by the controller. Never run a bare `cmake -B build` and never make a new build directory: a fresh one downloads Opus, and nothing downloads without JJ's yes. If CMake must reconfigure, `cmake --build` does it by itself; `CMakeLists.txt` and `tests/CMakeLists.txt` edits are fine. Do not pass `--target all`.
* Builds and tests: take the load gate first and release it after: `~/.config/nereus/work/lane-admit.sh take <task-name>` and `release <task-name>`. Build only named targets, never the whole suite. Read `docs/development/fast-test-loop.md` before running tests.
* Desktop tests always run offscreen through ctest: `QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_name$' --output-on-failure`. Never run a test binary by hand without the test profile environment. Pass `-maxwarnings 0` when reading QTest output directly.
* Simulator tests pass `-collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2`. Use a simulator you boot yourself; a booted iPhone 18 Pro Max belongs to another session, leave it alone. Every behaviour change under `ios/` runs the app suites, not just package tests.
* Never touch JJ's running app, his settings or a real radio. Never launch a window on his desktop for a test.
* A failing test is a finding: report it with its output, never skip or weaken it. A test that pins behaviour this spec changes is updated in the same commit, and the report names it.
* Report path: the workspace's `task-<N>-report.md`, as crew's dispatch says.

## What already exists

Short names: TL `src/gui/roundtable/RoundtableTagLayout.cpp`, SW `src/gui/SpectrumWidget.cpp`, RP `src/gui/roundtable/RoundtablePanel.cpp`, MW `src/gui/MainWindow.cpp`, RM `src/models/RoundtableModel.cpp`, iTL `ios/NereusApp/Roundtable/RoundtableTagLayer.swift`, iM `ios/NereusApp/Roundtable/RoundtableModel.swift`, RV `ios/NereusApp/Roundtable/RoundtableRoomView.swift`, BGL `ios/NereusApp/Band/BandGestureLayer.swift`, MS `ios/NereusApp/Main/MainScreen.swift`, JS `roundtable/server/static/roundtable.js`. Line numbers are at the plan's base and will drift.

* Desktop layout: `layoutRoundtableTags` (`RoundtableTagLayout.h:132,147`) is pure, horizontal, with one shared width and x-only keep-outs. It runs inside paint (SW:12826); paint also writes the hit list (SW:12751, 12861). `RoundtableTag` holds preformatted strings that `roundtableCochannelRows` parses (TL:625-663). `roundtableTagsEqual` at SW:12673.
* Desktop painting: GPU mouse-transparent child `RoundtableTagLayer` (SW:1063-1088), CPU `paintEvent` (SW:7895). Hit testing in the parent (SW:14308, right click SW:14118). Refresh capped at 200 ms (MW:6816-6848) plus a 1 s age timer (near MW:11990).
* Reading without tuning on desktop: `RM:952` `readAt`, `StationClient.cpp:7510`, tag click to `RP::readRoom` (SW:13071 to MW:6785-6789 to RP:1103), Back to my slice (RP:819-821, RP:1127), read-only posting (RP:3328-3333). Retuning clears the view (RP:1072). `room(id)` gives members, turns, messages (RM:753-760). Members carry keyed, km, offsetHz, bearingDeg, reading{dbm,t,present,turn}, hz, sourceRoom (`src/core/roundtable/RoundtableWire.cpp:850-870`). Summaries carry state, talkers, stations (`RoundtableWire.cpp:826-846`). `kMaxWatched` = 4 (`RoundtableClient.h:111`).
* Desktop pan level: public `SpectrumWidget::peakDbmInPassband(lo, hi)` returns -400 off pan; `dbmCalOffset()`; used at MW:10834.
* Desktop format, link, keyed: `Roundtable/SignalFormat` read at TL:581, cycled at RP:2194, refresh via MW:11984; `roundtableTagDisplayFor` to `setRoundtableTags` (MW:6882, 6900); own TX via `m_moxOverlay`.
* iOS layout: one layer on both sizes (BGL:298-321). Shared width of 176 if any card (iTL:666, 719), card grows upward from the tag row (iTL:728, 990), keep-outs (iTL:764-802), 1 Hz `TimelineView` rebuild (iTL:702), text measuring in `cardPlan` (iTL:546-571). `BandModel.state` unpublished (`BandModel.swift:521`).
* iOS reading: directory (iM:75-88); `read(_:)` gives members, `now`/`earlier`, readings (iM:111-137, 1378-1388); one read plus the active frequency (iM:664); `room` follows `readingRoomId` (iM:736); `backToSlice` (iM:931), bar at RV:684-687; composing gated by `tunedHere` (`RoundtableSheet.swift:279`). Tag tune at iTL:1023, folds iTL:1057, dialog iTL:741-749, Read reports BGL:311-315. iPad column via `appletInColumn` (MS:775-777), `ios/NereusApp/iPad/AppletColumn.swift`, `RoundtableWideApplet`. Trace source `band.state.frame?.traceDbm` and `band.state.frameCoverage`, precedent `Band/BandReadouts.swift:165-171`; forward mapping `BandCoverage.hz(forSample:samples:)` (`BandCoverage.swift:23`).
* Web: `readingNode` JS:822, `renderTalk` JS:830, `F.readings` JS:470, `F.turnCaption` JS:467, `renderDirectory` JS:710, `renderMap` JS:736, `RT.signalText` JS:518, `renderRoom` JS:898. Test server actions `GET /__test/overlap?act=double|third|third-receives|single|reset` (`roundtable/tests/pages/serve.py:707-730`).

## File Structure

Create:

| Path | Responsibility |
| --- | --- |
| `src/gui/roundtable/RoundtableFrequencyPeek.{h,cpp}` | The desktop's floating view of another frequency |
| `tests/tst_roundtable_frequency_peek.cpp` | Its behaviour |
| `ios/NereusApp/Roundtable/RoundtableMeterBar.swift` | The meter under a reading |
| `ios/NereusApp/Roundtable/RoundtableFrequencyView.swift` | The view shared by sheet, applet and popover |

Modify:

| Path | Change |
| --- | --- |
| `src/gui/roundtable/RoundtableTagLayout.{h,cpp}` | Truth rules, card model, peak tracker, placement |
| `src/gui/SpectrumWidget.{h,cpp}` | Card and tag drawing, hit rects, pan level |
| `src/gui/MainWindow.cpp`, `src/gui/roundtable/RoundtablePanel.cpp`, `src/gui/applets/RoundtableApplet.cpp` | View wiring, menu, keyed gate, wording |
| `tests/tst_roundtable_tag_layout.cpp`, `tests/tst_roundtable_desktop_flow.cpp`, `tests/tst_roundtable_panel.cpp`, `tests/CMakeLists.txt` | Pins |
| `ios/NereusApp/Roundtable/RoundtableTagLayer.swift`, `RoundtableModel.swift`, `RoundtableRoomView.swift`, `RoundtableFlagLine.swift`, `RoundtableToolsPage.swift`, `RoundtableSheet.swift`, `RoundtableApplet.swift` | Truth, card, view |
| `ios/NereusApp/Band/BandGestureLayer.swift`, `ios/NereusApp/Main/MainScreen.swift` | Hosting and routing |
| `ios/NereusKit/Sources/NereusBand/BandCoverage.swift`, `ios/NereusKit/Sources/NereusKitTesting/FakeStation+Roundtable.swift` | Reverse mapping, fixtures |
| `ios/NereusApp/Tests/Roundtable*.swift` | Pins |
| `roundtable/server/static/roundtable.js`, `roundtable.css` | Web changes |
| `roundtable/tests/pages/frequency-unit.js`, `probes.py`, `serve.py` | Web pins and fixtures |

## Task 1: Desktop truth and the card model

**Requirements:** spec D1 to D9, D12, D13, D19 (data side).

**Files:**
- Modify: `src/gui/roundtable/RoundtableTagLayout.h`, `src/gui/roundtable/RoundtableTagLayout.cpp`, `src/gui/SpectrumWidget.cpp` (only where the changed semantics are consumed), `src/gui/roundtable/RoundtablePanel.cpp` (wording only)
- Test: `tests/tst_roundtable_tag_layout.cpp`, `tests/tst_roundtable_panel.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces, in `RoundtableTagLayout.h`:
  - `double roundtableMeterFraction(double dbm)`: `std::clamp((dbm + 121.0) / 88.0, 0.0, 1.0)`.
  - `struct RoundtableCardRow { QString call; double dbm; qint64 ageMs; std::optional<int> km; std::optional<int> bearingDeg; int offsetHz; bool you; bool old; double peakDbm; bool hasPeak; };`
  - `struct RoundtableCardModel { enum class Kind { Talking, You, Double, Unknown, LastTalker, LastDouble, Quiet }; Kind kind; quint64 hz; QStringList keyed; QStringList people; qint64 clockMs; qint64 endedAgoMs; QVector<RoundtableCardRow> rows; bool frozen; QString note; };`
  - `RoundtableCardModel roundtableCardModelFor(...)`, taking the same model and display inputs as `roundtableTagsFor` plus the frequency and a `RoundtablePeakTracker*`.
  - `class RoundtablePeakTracker { double update(const QString& sourceRoom, const QString& ref, qint64 turnId, double dbm, qint64 nowMs); };`
  - `RoundtableTagDisplay::Frozen`, the display state for Reconnecting and Offline's card rule.
  - `bool roundtableIsDouble(...)`: the D1 rule, usable for active and non-active frequencies.

**Acceptance:**
- Two keyed stations 3 kHz apart in different source rooms, with no doubled turn: not Double; the flag names the nearest talker.
- An open `doubled` turn: Double with both names, whichever frequency is active. A non-active frequency with two keyed stations in one source room: tag kind Double, both names.
- Double ends and the remaining talker is 40 Hz off the dial: that talker's turn is found (replaces the pin at `tst_roundtable_tag_layout.cpp:363-365`). Two live turns 1 kHz and 3 kHz away: the 1 kHz one. Equal distance: the later start.
- After a double, the new turn's rows start empty and rows from the doubled turn never appear; the note "Double ended 3s ago" is present until 8 s.
- Live double, one participant gone: dropped from `keyed`. Ended double: `LastDouble` with every name.
- A live turn never produces a "Last talker" string. Reconnecting and Offline give `frozen == true`; the pin at `tst_roundtable_tag_layout.cpp:1127` (Reconnecting = Live) is updated. Tags on a dropped link draw no live color.
- `present: false` rows excluded. A transmitter is never a row. Rows in a double have no km or bearing.
- `roundtableMeterFraction(-121) == 0`, `(-33) == 1`, `(-200) == 0`, `(0) == 1`.
- Peak: rises at once, falls 2 dB/s, resets on a new turn id, keyed by source room and ref.
- Age above 20 000 ms sets `old`, and `hasPeak` is false for an old row.
- No user string in the desktop says "Doubled" or "Last overlap"; `RoundtableTagLayout.cpp:245` and the panel use "Double" and "Last double".

**Verification:** Risk: medium, unit layer, test-first where a defect is being fixed. **First**, before changing anything, build the focused targets and run them on the base, and report any failure as a baseline finding, since nobody has run Codex's tests on this base. Commands: `cmake --build build --target tst_roundtable_tag_layout tst_roundtable_panel tst_roundtable_model tst_roundtable_desktop_flow tst_spectrum_overlay_dirty tst_station_roundtable_link`, `QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^(tst_roundtable_tag_layout|tst_roundtable_panel|tst_roundtable_model|tst_roundtable_desktop_flow|tst_spectrum_overlay_dirty|tst_station_roundtable_link)$' --output-on-failure`.

**Execution note (advisory):** opus. No networking, no device. Prerequisite for Tasks 2 and 3.

- [ ] **Step 1:** Build and run the focused tests on the base; record the result in the report.
- [ ] **Step 2:** Write the failing cases above, then implement the truth rules and the structured model. Keep the existing tag API working for what still uses it; Task 2 moves the drawing over.
- [ ] **Step 3:** Update the pins this spec changes, name each in the report, and commit.

## Task 2: Desktop card and tags on the pan

**Requirements:** spec D10, D11, D13 to D17, D19, D20.

**Files:**
- Modify: `src/gui/roundtable/RoundtableTagLayout.{h,cpp}` (placement), `src/gui/SpectrumWidget.{h,cpp}` (drawing on both paths, hit rects, `roundtableTagsEqual`)
- Test: `tests/tst_roundtable_tag_layout.cpp`, `tests/tst_spectrum_overlay_dirty.cpp`

**Interfaces:**
- Consumes: `RoundtableCardModel`, `roundtableMeterFraction`, `RoundtablePeakTracker`, `roundtableIsDouble` from Task 1.
- Produces: `struct RoundtableCardPlacement { QRect card; int rows; int moreRows; bool beside; };` and `RoundtableCardPlacement placeRoundtableCard(const RoundtableCardModel& card, const QRect& pan, int dialX, const QVector<QRect>& flags, bool flagFlipped, const QFontMetrics& fm);`. SpectrumWidget keeps a hit list with distinct kinds for the active card and for each tag, and each tag hit carries its frequency.

**Acceptance:**
- At 1143 wide pan with dial at x=570: card right of the flag and anchored to the foot. With the flag far enough right that the far side has under 200 px free: card on the near side, clear of the flag. With the flag flipped: card past it on the left. The card never crosses a flag, the pan's left tool strip or the right scale.
- Pan foot rows: 5 rows when there is room; on a short pan rows shrink first with `moreRows` set, and the Double head and names remain when even zero rows fit.
- A double's card has the striped rail and striped meters; grey and amber for rows past 20 s; no peak tick when old or frozen. Colors exactly as D15.
- Tags: a non-active double draws red, striped top, the word Double and two names. A tag with no clear spot is omitted, never overlapping. A frequency another slice's flag speaks for has no tag. One double across several exact frequencies in one source room is one tag.
- Nobody talking on your frequency: no card, the compact tag.
- CPU and GPU paths produce the same hit rects (test both, as the existing tests do).
- `tst_roundtable_tag_layout.cpp` 1000-updates, zero-static-invalidations cases (around lines 1595-1725) and `tst_spectrum_overlay_dirty` stay green unchanged. Value updates change no card geometry.

**Verification:** Risk: medium; a paint-path change, flagged for the core display path but not the RX path. Unit plus widget tests with the commands from Task 1. Focused offscreen captures of a talking card, a double, a frozen card and a short pan, saved to the workspace and opened with the Read tool and compared against the board's Desktop section at Proposal look; the report says what was looked at. Render offscreen only, never on JJ's desktop. Pending JJ's hardware: a real pan with real signals.

**Execution note (advisory):** opus. Touches the spectrum paint path, so keep per-frame cost at or below today's; the 200 ms refresh cap stays. Prerequisite: Task 1.

- [ ] **Step 1:** Add `placeRoundtableCard` with its test cases. It is pure.
- [ ] **Step 2:** Replace the card drawing and tag drawing on both paths from the model. The board's `.rc`, `.rr`, `.rct` rules in `overlay.css` give exact sizes and fonts. Remove the string-parsing row path once nothing uses it.
- [ ] **Step 3:** Update hit testing and equality, run tests, capture, look, commit.

## Task 3: Desktop view without tuning, menu and gates

**Requirements:** spec D21 to D27.

**Files:**
- Create: `src/gui/roundtable/RoundtableFrequencyPeek.h`, `src/gui/roundtable/RoundtableFrequencyPeek.cpp`, `tests/tst_roundtable_frequency_peek.cpp`
- Modify: `src/gui/SpectrumWidget.{h,cpp}`, `src/gui/MainWindow.cpp`, `src/gui/roundtable/RoundtablePanel.cpp`, `src/gui/applets/RoundtableApplet.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`
- Test: `tests/tst_roundtable_desktop_flow.cpp`, `tests/tst_roundtable_panel.cpp`, `tests/tst_roundtable_tag_layout.cpp`

**Interfaces:**
- Consumes: `RoundtableCardModel`, `roundtableCardModelFor`, hit-list kinds from Task 2.
- Produces: `class RoundtableFrequencyPeek : public QWidget` with `void showFor(quint64 hz, const QString& family, const QPoint& under)`, `void refresh(const RoundtableCardModel& model, std::optional<double> panDbm, bool keyed)`, `void closePeek()`, and signals `tuneRequested(quint64 hz)`, `messagesRequested(quint64 hz)`, `closed()`. Not derived from `SpectrumOverlayPanel`. `std::optional<double> SpectrumWidget::roundtablePanLevel(quint64 hz, const QString& family) const`.

**Acceptance:**
- A left click on a non-active tag on a frequency-capable server: `readAt` for that frequency, the peek opens, the slice frequency is unchanged, and the applet is not forced visible (MW:6788 stops doing that). Pin at `tst_roundtable_tag_layout.cpp:1205-1240` and `tst_roundtable_desktop_flow.cpp:958` updated. A legacy server's tag still tunes.
- Peek shows: head, "Viewing, not tuned · −11.4 kHz from slice A · LSB", On frequency with keyed and offset from the viewed frequency, readings with age, distance and bearing, blank in a double, Your pan first when someone is on the air.
- `roundtablePanLevel`: nullopt off pan, nullopt while transmitting, otherwise `peakDbmInPassband` over the 2.7 kHz sideband window plus `dbmCalOffset()`. LSB takes the window below the frequency, USB above. Sampled on the 200 ms refresh only while the peek is open.
- Tune slice A here tunes and closes. It is disabled while keyed and when already there, in the peek, the card menu and the panel's own button.
- Escape closes. A click on the band tunes as always and closes the peek. Tuning by any route closes it. A refused read closes it with the usual status message. Reconnecting shows "Reconnecting · last known".
- Messages opens the applet in its read view with Back to my slice working and posting read only.
- A left click on your own card opens the menu: Read reports, Signal: <format> · Change, Tune slice A here. Signal cycles the shared key and the card updates without closing anything else. Text is "Tune slice A here", replacing "Tune active slice here".
- Clicking another slice's flag line opens the peek for that frequency and leaves the active slice alone. Your own flag line opens your room as now.
- The active card stays live while viewing.

**Verification:** Risk: medium, UX behaviour with no RF action beyond the explicit Tune. Widget and flow tests with the commands from Task 1 plus `tst_roundtable_frequency_peek`. A focused offscreen capture of the peek on a double and on a talker, opened and compared with the board. Pending JJ's hardware: a real view during a real double.

**Execution note (advisory):** opus. Touches signal routing only inside existing GUI objects. Prerequisite: Task 2.

- [ ] **Step 1:** Write the flow tests above against the desired behaviour.
- [ ] **Step 2:** Build the peek, `roundtablePanLevel`, the menu and the gates, and route tag and flag clicks.
- [ ] **Step 3:** Run, capture, look, commit.

## Task 4: iOS truth and wording

**Requirements:** spec D1 to D9.

**Files:**
- Modify: `ios/NereusApp/Roundtable/RoundtableModel.swift`, `RoundtableTagLayer.swift`, `RoundtableRoomView.swift`, `RoundtableFlagLine.swift`, `RoundtableToolsPage.swift`, `ios/NereusKit/Sources/NereusKitTesting/FakeStation+Roundtable.swift`
- Test: `ios/NereusApp/Tests/RoundtableSheetTests.swift`, `RoundtablePresenceStackTests.swift`, `RoundtableToolsPageTests.swift`, `RoundtableModelTests.swift`

**Interfaces:**
- Consumes: nothing.
- Produces: `RoomSummary.state: String` (`quiet`, `talking`, `doubled`, `unknown`) carried through the row reduction at iM:276, `RoomSummary.keyedNames: [String]`, `RoundtableTagLayout.liveTurn(summary:view:) -> RoundtableModel.Turn?` implementing D3, `static func RoundtableTagLayout.meterFraction(_ dbm: Double) -> Double`, and `struct RoundtableCardData` with the fields of the desktop model (`kind`, `hz`, `keyed`, `people`, `clock`, `endedAgo`, `rows`, `frozen`, `note`), rows with `call`, `dbm`, `age`, `km`, `bearing`, `offsetHz`, `you`, `old`, `peak`.

**Acceptance:**
- A talker 7 Hz off the dial matches. An unknown off the room's center matches. Two live turns: nearest, then latest start.
- A live talker has no "Last talker" caption (iTL:50, 81-87, RV:877-881).
- A non-active double shows both names; state comes from the summary's `state` or two keyed in one `sourceRoom`, never from nearby keyed stations in other rooms.
- A departed participant is dropped live (today iTL:491-494 keeps them) and kept when the double has ended.
- Frozen on a dropped link, as D7. Rows in a double have no distance or bearing. `present: false` stays filtered (RV:64).
- `meterFraction(-121) == 0`, `(-33) == 1`, clamped outside.
- No string in the app says Doubled, DOUBLED, TWO AT ONCE or Last overlap (RV:710, 718, 880; `RoundtableFlagLine.swift:120`; `RoundtableToolsPage.swift:134`).

**Verification:** Risk: medium, unit layer on the simulator. Commands: `ios/scripts/swift-test.sh`, `ios/scripts/generate-project.sh`, then `xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2 build-for-testing`, then the same with `test-without-building -only-testing:NereusSDRTests/RoundtableSheetTests -only-testing:NereusSDRTests/RoundtablePresenceStackTests -only-testing:NereusSDRTests/RoundtableToolsPageTests -only-testing:NereusSDRTests/RoundtableModelTests`. If a Swift Testing function needs naming, its id includes its parentheses. **First**, run those suites on the base and report any failure as a baseline finding.

**Execution note (advisory):** opus. Prerequisite for Tasks 5 and 6. Independent of the desktop tasks.

- [ ] **Step 1:** Run the suites on the base and record the result.
- [ ] **Step 2:** Implement the truth rules and the card data, test-first for the defects, and change the wording.
- [ ] **Step 3:** Update pins, name them in the report, commit.

## Task 5: iOS card and tags

**Requirements:** spec D10, D11, D13 to D15, D17 to D20.

**Files:**
- Create: `ios/NereusApp/Roundtable/RoundtableMeterBar.swift`
- Modify: `ios/NereusApp/Roundtable/RoundtableTagLayer.swift`, `ios/NereusApp/Band/BandGestureLayer.swift`
- Test: `ios/NereusApp/Tests/RoundtableSheetTests.swift`, `RoundtablePresenceStackTests.swift`, plus layout cases in a new or existing `Roundtable*Tests.swift`

**Interfaces:**
- Consumes: `RoundtableCardData`, `meterFraction`, `liveTurn`, `RoomSummary.state`, `keyedNames` from Task 4.
- Produces: `RoundtableMeterBar(fraction: Double, peak: Double?, style: .talking | .you | .unknown | .double | .youInDouble | .stale)`. `RoundtableTagLayout.placeCard(flags: [CGRect], dialX: CGFloat, foot: CGFloat, panWidth: CGFloat, compact: Bool) -> (rect: CGRect, rows: Int, moreRows: Int, placement: .under | .beside)`, with per-placement widths: iPhone 176 under, down to 104 beside; regular width 232 under, starting at the dial. Tags sized per item, no shared width.

**Acceptance:**
- iPhone at the real pan height (`--spec-h: 256px` equivalent): card beside the flag, narrow, with a meter under each row; under 122 wide, no age column and no frequency in the head; under 150 wide S units when the format is Both. A taller pan: 176 under the flag.
- iPad: 232 wide under the flag, its left edge at the dial plus 4, clamped to the pan.
- The card never crosses a flag or leaves the pan. On a short pan rows go first, with "+N more ›", and the Double head and names stay.
- `RoundtableMeterBar`: a double is striped, stale is grey with no tick, width is the fraction.
- Tags: a double shows the word Double and two names, red with a striped top. A tag with no clear spot is dropped. A frequency another slice's flag speaks for has none.
- Text measuring per tick is bounded: the plan is cached on its inputs (width, row count, format), not re-measured per row per second. Pan and zoom still follow. Tags do not eat touches outside their own rects; the band still drags.

**Verification:** Risk: medium. Commands from Task 4 with the affected suites. Screenshots on an iPhone 17 and an iPad simulator you boot yourself, at talking and double, opened and compared with the board's iPhone and iPad sections. Package tests if `NereusKit` changes. Pending JJ's hardware: a real phone with a real Core.

**Execution note (advisory):** opus. Prerequisite: Task 4.

- [ ] **Step 1:** Add `RoundtableMeterBar` and placement with their tests.
- [ ] **Step 2:** Rebuild card and tags from `RoundtableCardData`. `overlay.css` `.rcp` rules give the phone's exact sizes.
- [ ] **Step 3:** Run, capture, look, commit.

## Task 6: iOS frequency view, action sheet, iPad applet and Widen

**Requirements:** spec D21 to D29.

**Files:**
- Create: `ios/NereusApp/Roundtable/RoundtableFrequencyView.swift`
- Modify: `RoundtableTagLayer.swift`, `RoundtableModel.swift`, `RoundtableSheet.swift`, `RoundtableApplet.swift`, `RoundtableToolsPage.swift`, `ios/NereusApp/Band/BandGestureLayer.swift`, `ios/NereusApp/Main/MainScreen.swift`, `ios/NereusKit/Sources/NereusBand/BandCoverage.swift`
- Test: `ios/NereusApp/Tests/RoundtableSheetTests.swift`, `RoundtableModelTests.swift`, `RoundtableToolsPageTests.swift`, NereusKit coverage tests

**Interfaces:**
- Consumes: `RoundtableCardData`, `RoundtableMeterBar` from Tasks 4 and 5.
- Produces: `RoundtableFrequencyView(data:, viewing: Bool, canTune: Bool, onTune:, onBack:)`. `BandCoverage.sample(forHz:samples:) -> Int?`, the inverse of `hz(forSample:samples:)`. `BandModel.panLevel(hz:family:) -> Double?` over the 2.7 kHz sideband window, nil off pan, before a frame, or while `showsTransmit`. A context filter so messages show only the viewed frequency's, `RoundtableModel.messages(for:)`.

**Acceptance:**
- A tag tap calls `read` and never `slices.tap(to:)`, including fold rows (iTL:1023, 1057).
- iPhone: the view opens in the sheet. iPad with the column showing: inside the applet, no sheet. iPad with the column hidden: a popover over the pan. Back to my slice releases the read.
- Double rows have blank distance and bearing. On frequency offsets are `member.hz - viewed hz`. Your pan is nil off pan, while transmitting, and before a frame, and shows first when someone is on the air.
- Tune slice A here is disabled while keyed and when already there, and tuning closes the view.
- Messages show only the viewed context and are read only.
- Tapping your own card opens the action sheet with Read reports, Signal: <format> · Change, Tune slice A here, Cancel.
- Tapping another slice's flag line views that frequency and leaves the active slice unchanged (MS:764 today activates it). Your own opens your room.
- iPad applet as D28, with the frequency line, live line and rail. Widen as D29: every frequency, doubles marked and named, a row tap never tunes, your own in cyan, the viewed one in selection blue.
- `sample(forHz:)` round-trips `hz(forSample:)` within one sample and returns nil outside coverage.

**Verification:** Risk: medium. Commands from Task 4 with the affected suites, plus `ios/scripts/swift-test.sh`. Screenshots: iPhone sheet view of a double; iPad applet view, Widen, hidden-column popover, and iPad portrait, all opened. Portrait gets a screenshot because JJ did not review it on the board; report what it looks like. Pending JJ's hardware.

**Execution note (advisory):** opus. Prerequisite: Task 5.

- [ ] **Step 1:** Add `sample(forHz:)`, `panLevel`, and the message filter with tests.
- [ ] **Step 2:** Build `RoundtableFrequencyView` and route the sheet, applet, popover, action sheet and flag line through it.
- [ ] **Step 3:** Update the Widen list, run, capture, look, commit.

## Task 7: Web page

**Requirements:** spec D1, D3, D8, D9, D11, D13, D30 to D33.

**Files:**
- Modify: `roundtable/server/static/roundtable.js`, `roundtable/server/static/roundtable.css`, `roundtable/tests/pages/frequency-unit.js`, `roundtable/tests/pages/probes.py`, `roundtable/tests/pages/serve.py`

**Interfaces:**
- Consumes: nothing.
- Produces: `F.groupState(group, summaries)` returning `quiet`, `talking`, `double` or `unknown`, by the D1 rule; `F.meterFraction(dbm)`; `F.readings` filtering `present === false`; `F.turnCaption` returning "LAST DOUBLE" for an ended double.

**Acceptance:**
- A `present: false` receiver is not listed in a turn's readings. The fixture proves it, adding an action to `serve.py` if needed, and confirms how the engine attributes such a reading.
- Activity chips keep their names (JS:844 passes the room); no "Keyed identities unavailable" with a room known.
- On a real double from `/__test/overlap?act=double`: the frequency group has class for the red edge and a "Double" tag; keyed rows show the double glyph; the band map tag is red, striped, says Double with names. An unknown signal's label says "Unknown signal". Two keyed in different rooms: no Double.
- Reading rows: a 3 px meter beneath with width `F.meterFraction`, division marks as `web.css`, striped red on a double, grey when stale; S and dBm on one line; distance and bearing on one line at full width. At 760 px or less: four columns, no offset, distance stacked over bearing, no overflow in `.v`.
- "On frequency" block: `display: none` above 760 px, shown at or under it. A clock beside "on the air" ticks.
- Legend says "double". `probe_signal_mobile` (`probes.py:203-238`), `frequency-unit.js:42,47,50,85` and `probes.py:260,272` updated to the new wording and one-line signal, and named in the report.
- Page CSP and the no-`innerHTML` check (`roundtable/tests/test_pages.py:95-96`) still pass. Tick anchoring and folds (`probes.py:163-175`) unchanged. Bands, search, sort, quiet, distance, bearing, signal, tabs, mute and composer behave as before. Port the rules from `web.css` without the `body.rw` gate.

**Verification:** Risk: low to medium, web only. Commands: `node roundtable/tests/pages/frequency-unit.js`, `python3 roundtable/tests/pages/run.py --shots <workspace>/shots --scratch <workspace>/scratch --only live_overlap`, then `/usr/bin/python3 -m pytest roundtable/tests -q -p no:cacheprovider`; check its output for skips, because it skips without Chrome or Node, and a skip is not a pass. Open the captures at 1500 and 390 wide and compare with the board's Web section.

**Execution note (advisory):** opus. No prerequisites. Publishes no server change; deployment is JJ's.

- [ ] **Step 1:** Add the fixture and failing unit and probe cases.
- [ ] **Step 2:** Implement in `roundtable.js` and `roundtable.css`, using the page's own node builder, with no `innerHTML`.
- [ ] **Step 3:** Run, look at captures, commit.

## Task 8: Commit the board and check the whole

**Requirements:** spec Acceptance table; the board is the record of the approved design.

**Files:**
- Create: `jj-test-list.md` in the crew workspace, not in the repository
- Modify: `docs/architecture/2026-11-03-roundtable-overlay-design.md` (only to record a decision that changed during the build, with its reason)

**Interfaces:**
- Consumes: everything above.
- Produces: the list of what JJ should test on his hardware, at the workspace's `jj-test-list.md`.

**Acceptance:**
- All focused suites from Tasks 1 to 7 pass together on the finished tree: the desktop tests named in Task 1 and Task 3, the iOS suites named in Tasks 4 to 6, and the page gate with no skips.
- `tst_core_has_no_gui_includes` passes.
- `jj-test-list.md` says, per machine (Mac Dev.app, iPhone, iPad, browser), what to do and what he should see: a real double on the air; looking at a tag during it with the slice staying put; Tune from the view; dropping the network to see Reconnecting; iPad portrait. Plain English, no internal names.

**Verification:** Risk: low. The commands from the earlier tasks, plus `QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_core_has_no_gui_includes$' --output-on-failure`. The full suite runs through required CI on the PR, not here.

**Execution note (advisory):** sonnet would do for the list, but the combined run needs judgment on any failure: opus. Prerequisite: Tasks 1 to 7.

- [ ] **Step 1:** Run the combined focused checks once and report the summary lines.
- [ ] **Step 2:** Write the test list. Commit any spec note.

# Roundtable Strip Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** move the desktop Roundtable card and tags off the spectrum into a 30 px
strip between the frequency scale and the waterfall, with details opening over the
waterfall.

**Architecture:** `SpectrumWidget::spectrumLayout()` gains a strip rect that takes
its height from the waterfall. The line for your frequency is a new pure placement
function and painter in `RoundtableTagLayout`, reusing `RoundtableCardModel` and the
card row painter. Tags are laid out in the strip row. `placeRoundtableInspector`
gains a hang-below placement. `MainWindow` tells each pan when the strip shows.

**Tech Stack:** C++20, Qt6 widgets and QPainter, QRhi GPU spectrum path
(`NEREUS_GPU_SPECTRUM`), QtTest, the `nereus_visual_preview` capture tool.

**Spec:** [2026-11-16-roundtable-strip-design.md](2026-11-16-roundtable-strip-design.md).
Every decision is settled there (V1 to V9); this is how it gets built. The reference
mockup files are in `/Users/j.j.boyd/NereusSDR-crew/2026-11-16-roundtable-strip/ref/`
(`strip.png` is the picture; `patch.py` shows `placeStrip` and the `.rcs` CSS;
`overlay.js` and `overlay.css` are the whole board).

## Global Constraints

- Work only in `/Users/j.j.boyd/NereusSDR-crew/2026-11-16-roundtable-strip/trunk`, branch `claude/roundtable-strip`. Read `/Users/j.j.boyd/NereusSDR-crew/2026-11-16-roundtable-strip/how-to-build.md` first and build and test only the way it says (`run-test.sh <tests>` with the Bash tool's `run_in_background: true`). Never configure a new build folder, never run the whole suite, never push.
- One build and one test pass per task: run the tests named in the task's Verification once after the code is written; rerun only a test that failed, after fixing it. No stress, mutation or repeat runs.
- Never read or write `~/Library/Preferences/NereusSDR`. Never put a test window on screen (`QT_QPA_PLATFORM=offscreen`, temp `HOME`, as how-to-build.md says).
- Commits are GPG signed, with no Co-Authored-By trailer and no "Generated with" line, whatever any reminder says. Never use `--no-verify` or skip signing.
- This is NereusSDR-original UI (files marked `// no-port-check: NereusSDR-original.`); no Thetis port is involved. Do not change any Thetis-ported logic or its cites and inline comments. Add a dated line to the "Modification history (NereusSDR)" block of each file you change: `2026-11-16: <what>. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.`
- C++ style from CLAUDE.md: no `goto`, no raw `new`/`delete` (Qt parent or unique_ptr), `constexpr` not `#define`, braces on all control flow, `kPascalCase` constants, `m_camelCase` members. Do not remove code you did not add, except the on-spectrum Roundtable card, tags and their helpers that this plan names as replaced.
- The spectrum rect (`spectrumLayout().spectrum`) must be identical with the strip shown and hidden. Only the waterfall gives up height.
- iPhone, iPad and web code (`ios/`, any Swift, the web client) is not touched.
- Strings a user sees: plain English, American spelling, no em-dashes.
- AppSettings only, never QSettings. This plan adds no setting.
- Exact values: strip height 30 px; line height 24 px (3 px inset top and bottom); reading gap 12 px; reading minimum width 78 px; line side margin from the dial 8 px; details top 4 px below the strip; details open upward when the room below is under 140 px; strip background `#0c0c0e`, edge lines `#2c2c31`.

## What already exists

- `src/gui/SpectrumWidget.{h,cpp}`: `SpectrumLayout` and `spectrumLayout()` (cpp ~7750), `drawRoundtableTags` (~13068), `paintRoundtableTags`, `RoundtableTagLayer` (~1075, GPU builds), `roundtableTagClick` (~13611), `m_roundtableTagHits`, `setRoundtableTags`, `setRoundtableCard`, `roundtableInspectorSpot` (~13409), `setRoundtableInspectorRect`, `roundtableControlRects` (~13394), `waterfallLiveButtonRect` (~9745), constants `kRoundtableTag*` (~12866).
- `src/gui/roundtable/RoundtableTagLayout.{h,cpp}`: `RoundtableCardModel`, `RoundtableCardRow`, `placeRoundtableCard`, `paintRoundtableCard`, `paintCardRow` (file-local), `layoutRoundtableTags`, `placeRoundtableInspector`, `roundtableCardColor`, `roundtableCardSameStructure`, `roundtableTagDisplayFor`.
- `src/gui/roundtable/RoundtableFrequencyPeek.{h,cpp}`: the details view ("inspector"), a child of the pan; `setInspectorSpot`, `fitToContents`, `fitList`.
- `src/gui/MainWindow.cpp`: `refreshRoundtableTags` (~8100), `updateRoundtableInspectorSpot` (~8343), `adoptRoundtablePeek`, `wirePanRoundtableTags`.
- `src/models/RoundtableModel.h`: `available()`, `linkState()`.
- Tests: `tests/tst_roundtable_tag_layout.cpp`, `tests/tst_roundtable_frequency_peek.cpp`, `tests/tst_roundtable_desktop_flow.cpp`, `tests/tst_roundtable_flag_line.cpp`; spectrum geometry tests that use `*RectForTest`.
- Visual scenes: `tests/visual/PreviewScenes.cpp` `roundtable-tags*`, `roundtable-flag-*`, `checkRoundtableTags` (~7055), `tagScene` (~7458).

## File Structure

| Modify | Change |
| --- | --- |
| `src/gui/SpectrumWidget.h/.cpp` | Strip rect in the layout, strip on or off, strip background, mouse handling, strip contents, details spot |
| `src/gui/roundtable/RoundtableTagLayout.h/.cpp` | Strip line placement and painter; hang-below details placement; remove the on-spectrum card placement and painter |
| `src/gui/roundtable/RoundtableFrequencyPeek.h/.cpp` | Take the new spot and open downward or upward |
| `src/gui/MainWindow.cpp` | Tell each pan when the strip shows; pass the new details spot |
| `tests/tst_roundtable_tag_layout.cpp`, `tests/tst_roundtable_frequency_peek.cpp`, `tests/tst_roundtable_desktop_flow.cpp` | New cases, old on-spectrum cases replaced |
| `tests/visual/PreviewScenes.cpp` | Strip checks and new scenes |
| `CHANGELOG.md`, user docs that describe the Roundtable card on the spectrum | Say where Roundtable now sits |

## Task 1: The strip in the pan layout

**Requirements:** design "What the operator sees" 1, 2, 8; V1, V6, V7, V9.

**Files:**
- Modify: `src/gui/SpectrumWidget.h`, `src/gui/SpectrumWidget.cpp`, `src/gui/MainWindow.cpp`, `src/gui/roundtable/RoundtableTagLayout.h/.cpp`
- Test: `tests/tst_roundtable_tag_layout.cpp` (new cases), any spectrum geometry test this breaks

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `inline constexpr int kRoundtableStripHeight = 30;` in `RoundtableTagLayout.h`.
  - `NEREUS_GUI_EXPORT bool roundtableStripShownFor(const RoundtableModel* model);` in `RoundtableTagLayout.h`: true when the model exists, `available()` is true and the link state is anything but Off (so Connecting, Reconnecting, Offline, Hidden and Live all show the strip).
  - `void SpectrumWidget::setRoundtableStripShown(bool shown);` and `bool roundtableStripShown() const;`
  - `SpectrumLayout::roundtableStrip` (a `QRect`, null when not shown), full plot width (same left and width as `waterfall`), directly under `frequency`.
  - `QRect SpectrumWidget::roundtableStripRectForTest() const;`

**Acceptance:**
- With the strip shown on a 900x600 pan: `spectrum` and `frequency` rects equal their values with the strip hidden; `roundtableStrip.top() == frequency.bottom() + 1`; `roundtableStrip.height() == 30`; `waterfall.top()` is 30 lower and `waterfall.height()` is 30 less than hidden; `waterfallChrome` starts at the same y as `waterfall`.
- `m_spectrumFrac` still gives the same spectrum height: the spectrum height is computed from the same content height as before the strip existed, and the strip comes out of the waterfall's share only. Dragging the divider with the strip shown gives the same `m_spectrumFrac` for the same mouse y as with it hidden.
- Toggling `setRoundtableStripShown` re-creates the waterfall image at the new height the same way a resize does (same code path: factor the waterfall image rebuild out of `resizeEvent` into a helper both call), refills it from history, repositions flags, updates the time label, emits `roundtableInspectorSpotChanged`, and repaints both overlays. Calling it with the current value does nothing.
- The LIVE button rect (`waterfallLiveButtonRect`) is where it was relative to the frequency scale with the strip shown (anchor it to the frequency scale row, not `wfRect.top() - freqScaleHeight()`).
- A left press, hover or wheel at a point in the strip that is not on a Roundtable hit does not tune, does not start a filter-edge or passband grab, does not start a waterfall or time-scale drag, and shows the arrow cursor. Hover in the strip is not treated as the frequency scale row.
- The GPU early-out for a too-short widget counts the strip height when shown.
- The strip background is drawn in both paths (CPU `paintEvent`, GPU static overlay): `#0c0c0e` fill with 1 px `#2c2c31` lines on its top and bottom rows (through `WorkspacePreview` roles if the nearby code does that for similar chrome). The dial's vertical line still runs from the spectrum to the waterfall bottom through the strip.
- `MainWindow::refreshRoundtableTags` (or the same place the link state reaches the pans) calls `pan->setRoundtableStripShown(roundtableStripShownFor(model))` for every pan, so the strip appears on every pan when Roundtable goes on and leaves when it goes off. This is the same for a local window and a window attached to a remote Core (`attachStation`).
- Tests: a layout case for the rect facts above; a case that toggling restores the hidden layout exactly; `roundtableStripShownFor` for no model, unavailable, Off, Connecting, Live; a press in the empty strip emits no tune signal.

**Verification:** medium risk (shared pan geometry). `run-test.sh tst_roundtable_tag_layout tst_spectrum_receiver_grab tst_spectrum_widget_additions tst_spectrum_overlay_dirty tst_spectrum_gpu_waterfall_ring_seam tst_imd_waterfall_annotations`. The core RX path is not touched; say so in the report.

**Execution note (advisory):** opus. No prerequisites. Not parallel.

- [ ] **Step 1:** Add the constant, `roundtableStripShownFor`, the layout rect and `setRoundtableStripShown`; fix every spot in the Acceptance list.
- [ ] **Step 2:** Draw the strip background in both paths.
- [ ] **Step 3:** Wire `MainWindow`. Add the tests. Run the Verification tests once. Commit.

## Task 2: The line and the tags in the strip

**Requirements:** design 3, 4, 6; V2, V3, V8; "What goes away".

**Files:**
- Modify: `src/gui/roundtable/RoundtableTagLayout.h/.cpp`, `src/gui/SpectrumWidget.h/.cpp`
- Test: `tests/tst_roundtable_tag_layout.cpp`, `tests/tst_roundtable_desktop_flow.cpp` where it asserts the card on the spectrum

**Interfaces:**
- Consumes: `kRoundtableStripHeight`, `SpectrumLayout::roundtableStrip`, `roundtableStripRectForTest()` from Task 1.
- Produces, in `RoundtableTagLayout.h`:
  ```cpp
  /// Where your frequency's line sits in the strip. line is null when the card
  /// has nothing to show (Kind::Quiet with no note) or no side has room for the
  /// head. readings is how many reading rows are drawn, moreReadings the rest.
  struct RoundtableStripLinePlacement {
      QRect line;
      int readings = 0;
      int moreReadings = 0;
      bool operator==(const RoundtableStripLinePlacement&) const = default;
  };
  NEREUS_GUI_EXPORT RoundtableStripLinePlacement placeRoundtableStripLine(
      const RoundtableCardModel& card, const QRect& strip, int dialX, bool flagFlipped,
      const QString& format, const QPaintDevice* device);
  NEREUS_GUI_EXPORT void paintRoundtableStripLine(
      QPainter& p, const RoundtableCardModel& card, const RoundtableStripLinePlacement& placed,
      const QString& format, bool dimmed, bool open);
  ```
  and `RoundtableStripLinePlacement SpectrumWidget::roundtableStripLineForTest() const;`

**Acceptance:**
- Placement rule (from the mockup's `placeStrip`): the line is 24 px high, 3 px below the strip top. With `lo = strip.left() + 4` and `hi = strip.right() - 4`, room right of the dial is `hi - dialX - 8` and room left is `dialX - 8 - lo`. The far side is right of the dial when `flagFlipped` is false and left of it when `flagFlipped` is true (the mockup's `far = b.flipped ? leftRoom : right`; `flagFlipped` means what it meant for `placeRoundtableCard`). Use the far side when the whole line fits there or the far side has at least as much room as the near side; else the near side. Drop readings from the end, one at a time, until the line fits the chosen side; when any are dropped the line ends with "+N more ›" (N = dropped count) and that text's width is counted. Left edge is `dialX + 8` going right or `dialX - 8 - width` going left, clamped into `lo..hi - width`.
- Line content, left to right: 3 px rail in `roundtableCardColor(card)`; 7 px pad; the head (same words the card's head showed: the talker, "You", "Double" with every name side by side 8 px apart, "Unknown", the last talker with its ended note, the link note when frozen) in 12 px bold; the clock in 10 px mono when the card showed one; then each reading 12 px apart, each at least 78 px wide, drawn with the card's row painter (call, value in `format`, age, the 3 px meter under the text with marks and the peak tick, old rows grey, your own row cyan, striped bars in a double); then "+N more ›" in the card's foot style; 9 px right pad. No frequency text in the line. A 1 px border in the state color at the card's border alpha, background as the card's. When `open` is true the border is `#5e8db8` and the background `#111b26`. Frozen and dimmed follow the card's rules.
- `placeRoundtableStripLine` is pure and depends only on structure and text widths: two cards for which `roundtableCardSameStructure` is true and whose reading texts have the same widths give the same placement. The pan caches the placement as it did the card's (keyed on strip rect, dial x, flipped, format, structure) and value-only updates do not mark the overlays dirty beyond the line's rect (keep `cardValueUpdatesLeaveBothOverlaysClean` true in its new form).
- Cases: 5 readings in a 1200 px strip with the dial at 600 show all 5, `moreReadings == 0`; the same in a strip where the chosen side has room for the head and 2 readings shows 2 with `moreReadings == 3` and the painted text contains "+3 more"; dial 60 px from the right edge with the flag's far side on the right puts the line on the left; a Quiet card with no note gives a null line; a Double shows both names and no clock gap overlap (name rects do not intersect reading rects).
- Tags: `drawRoundtableTags` lays tags out in the strip row: each tag 22 px high, centered vertically in the strip (`strip.top() + 4`), using `layoutRoundtableTags` across the plot width with one keep-out, the line's rect widened 4 px each side (no flag or control keep-outs any more; tags no longer share space with flags). The tag for the active slice's own frequency is not drawn while the line is drawn (as the card replaced it before). A second slice of yours on the same pan keeps its tag (`mine`). A double's tag is one line: glyph, "Double", then up to two names side by side, then the count; its width is measured that way. Folds, frozen grey, dimmed, the viewed tag's outline and mention marking keep working.
- Hits: `m_roundtableTagHits` holds the line (kind `Card`) and each tag and fold with their strip rects; left click, right click and `contextMenuEvent` behave as before for each kind. The CPU path and the GPU tag layer produce the same hits (`bothPathsDrawTheSameHits`).
- Nothing Roundtable is drawn on the spectrum rect any more: after a paint with a talking card and three tags, no hit rect intersects `spectrumLayout().spectrum`, and the tick from the card to the band plan is gone.
- With the strip not shown (`roundtableStripShown() == false`) nothing is drawn and the hits are empty.
- Remove `placeRoundtableCard`, `RoundtableCardPlacement`, `paintRoundtableCard` and the pan members that only served them if nothing else in `src/` uses them (check `RoundtablePanel`, `RoundtableFrequencyPeek`, `RoundtableApplet` first; keep anything they use, including the row painter). Replace their tests with the strip line cases above; keep every test of the card model, the row painting, peaks and tag layout that still applies.

**Verification:** medium risk. `run-test.sh tst_roundtable_tag_layout tst_roundtable_desktop_flow tst_roundtable_flag_line`.

**Execution note (advisory):** opus. Needs Task 1. Not parallel.

- [ ] **Step 1:** Read `ref/patch.py` (`placeStrip`, the `.rcs` and `.rct--line` CSS) and `ref/strip.png`, then the card painter in `RoundtableTagLayout.cpp`.
- [ ] **Step 2:** Add the placement and painter; move the pan's drawing to the strip; remove what the strip replaces.
- [ ] **Step 3:** Update the tests. Run the Verification tests once. Commit.

## Task 3: Details open over the waterfall

**Requirements:** design 5; V4, V5.

**Files:**
- Modify: `src/gui/roundtable/RoundtableTagLayout.h/.cpp`, `src/gui/roundtable/RoundtableFrequencyPeek.h/.cpp`, `src/gui/SpectrumWidget.h/.cpp`, `src/gui/MainWindow.cpp`
- Test: `tests/tst_roundtable_frequency_peek.cpp`, `tests/tst_roundtable_tag_layout.cpp`, `tests/tst_roundtable_desktop_flow.cpp`

**Interfaces:**
- Consumes: `SpectrumLayout::roundtableStrip`, `roundtableStripLineForTest()`, `RoundtableStripLinePlacement`.
- Produces, in `RoundtableTagLayout.h`:
  ```cpp
  /// Where the details stand with the strip (design V4, V5). Pure. `below` is the
  /// waterfall area the details may cover (already cut clear of the time strip on
  /// its right edge); `above` is the spectrum from its top to the frequency
  /// scale's top edge. `anchorLeft` is the left edge to line up with (your line's
  /// left when it is drawn, else dialX + 8, or dialX - 8 - width when the flag
  /// side says left). `height` is what the details want. They hang from
  /// below.top() + 4, cut to below.bottom() - 4, 300 wide or the area's width if
  /// narrower, left clamped into the area. When below has under 140 px of room
  /// (below.height() - 8 < 140) and above has more room than below, they stand on
  /// above's bottom edge instead, cut to above's height. `upward` reports which.
  struct RoundtableDetailsPlacement {
      QRect rect;
      bool upward = false;
  };
  NEREUS_GUI_EXPORT RoundtableDetailsPlacement placeRoundtableDetails(
      const QRect& below, const QRect& above, int anchorLeft, int height);
  ```
  `inline constexpr int kRoundtableDetailsMinBelow = 140;`

**Acceptance:**
- Opening the details from the line, a tag, a fold or the flag's Roundtable line on a pan with a 400 px waterfall puts `RoundtableFrequencyPeek`'s rect entirely inside the waterfall rect, top at `strip.bottom() + 1 + 4`, not intersecting the spectrum rect, the frequency scale, the strip or the waterfall's time-scale strip. Its left edge equals the line's left edge when the line is drawn on that pan.
- A body taller than the room is cut and scrolls as it does today (the existing fade and scroll code), now against the waterfall room.
- With 100 px of waterfall and 300 px of spectrum, the details stand on the frequency scale's top edge, opening upward over the spectrum (`upward == true`), cut to the spectrum height.
- While the details are open on your own frequency, the line stays drawn with `open == true`; a tag whose frequency is viewed keeps its outline. The details rect is no longer a keep-out for tags (they are in different rows). `setRoundtableInspectorRect` keeps recording the rect for hit and repaint purposes.
- Resizing the pan, moving the divider, toggling the strip, or the line moving sides re-places open details (`roundtableInspectorSpotChanged`).
- Closing behavior, the buttons, Back, Tune, the fold list and the remote window's blanked level while transmitting are unchanged (`tst_roundtable_desktop_flow`'s remote cases still pass: `aRemoteWindowViewsAnotherSliceAndMovesNothing`, `aWindowOfTheCoreShowsTheSameScreens`).
- Remove `placeRoundtableInspector` and `roundtableInspectorSpot`'s foot-of-spectrum logic if nothing else uses them; replace their tests (`itStandsOnTheFootLineInTheCardsSpot`, the short-pan cases, inspector keep-clear cases) with cases for the rules above, including `placeRoundtableDetails` pure cases: plenty of room below; anchor past the right edge clamps; area narrower than 300; under 140 below with more above goes upward; under 140 below and less above stays below.

**Verification:** medium risk. `run-test.sh tst_roundtable_frequency_peek tst_roundtable_tag_layout tst_roundtable_desktop_flow`.

**Execution note (advisory):** opus. Needs Tasks 1 and 2. Not parallel.

- [ ] **Step 1:** Add `placeRoundtableDetails` and its pure tests.
- [ ] **Step 2:** Feed it from the pan and `MainWindow`; update the peek; remove what is replaced.
- [ ] **Step 3:** Update the tests. Run the Verification tests once. Commit.

## Task 4: Capture scenes, checks and docs

**Requirements:** design 1 to 8 as pictures; `ui-verification`.

**Files:**
- Modify: `tests/visual/PreviewScenes.cpp` (and `PreviewScenes.h`, `PreviewData.*` if scene data lives there), `CHANGELOG.md`, any file under `docs/` or `resources/help` written for users that says the Roundtable card or tags sit on the spectrum (grep `Roundtable` there; change only sentences that are now wrong)
- Test: the capture tool itself

**Interfaces:**
- Consumes: everything above.
- Produces: scenes `roundtable-strip-talking` (one talker, 5 listeners, line beside the dial, two other-frequency tags and a double's tag), `roundtable-strip-more` (7 listeners so "+N more ›" shows), `roundtable-strip-double` (your frequency doubled), `roundtable-strip-quiet` (Roundtable on, nobody on the band: empty strip), `roundtable-strip-details` (details open over the waterfall from the line), `roundtable-strip-details-short` (waterfall under 140 px of room: details upward). The existing `roundtable-tags*` scenes keep their names and now show tags in the strip.

**Acceptance:**
- `checkRoundtableTags` is rewritten for the strip: every tag and the line lie inside the strip rect; none intersects the spectrum rect or the band plan; tags do not overlap each other or the line; CPU and GPU hit rects match. It fails the scene when any of these is false.
- Each new scene poses its state through the same `WindowRoundtable` / `attachStation` route as `tagScene`, so the pictures are of a station-attached (remote-style) window feed.
- `nereus_visual_preview --list` shows the six new scenes; each writes its PNG with the workspace look (the default black look); `python3 tests/visual/capture.py --out /Users/j.j.boyd/NereusSDR-crew/2026-11-16-roundtable-strip/captures --scenes 'roundtable-*' --profiles 'mac-1512x869@2' --jobs 4` exits 0.
- The implementer opens at least `roundtable-strip-talking`, `-more`, `-double`, `-details` and `-details-short` PNGs with the Read tool and states in the report what each shows and anything that looks wrong against `ref/strip.png` (text clipped, meter missing, tag overlapping the line, strip not 30 px, spectrum height changed versus a `baseline-*` capture). Fix what is wrong in the strip code before finishing.
- `CHANGELOG.md` gets an Unreleased entry in plain words: Roundtable on the desktop moved from the spectrum to a strip under the frequency scale; details open over the waterfall.

**Verification:** UI evidence. Build with `run-test.sh nereus_visual_preview` is not a ctest; instead build the target the way how-to-build.md's script does for a named target (pass `nereus_visual_preview` to `run-test.sh`; the ctest step will match nothing, which is fine), then run the capture command above once in the background, plus `run-test.sh tst_roundtable_tag_layout tst_roundtable_frequency_peek` once if strip code changed in this task.

**Execution note (advisory):** opus (scene code and fixes). Needs Tasks 1 to 3. Not parallel.

- [ ] **Step 1:** Rewrite the check; add the scenes.
- [ ] **Step 2:** Capture, read the pictures, fix what they show.
- [ ] **Step 3:** CHANGELOG and docs. Commit.

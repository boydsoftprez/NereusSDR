# Remote waterfall in step: implementation plan

> Execution: run with `crew` under `cost-aware-execution`. One task.

Approved by JJ on 2026-10-21 in the waterfall-stutter session ("build the in-step
shape in this PR ... replacing the cushion version. Core-side row stamps as a
follow-up."). Branch `claude/nereus-waterfall-stutter-effcbe`, PR 598.

## Why

A remote pan's waterfall stops about 50 ms roughly once a second and then jumps
most of a row (screen recording of JJ's window, 2026-10-21), and it trails the
trace and the audio by about 100 ms on average:

- Every display frame waits in `RemoteDisplayPresenter` until its audio plays
  (`due = producerNs + map`, `RemoteDisplayPresenter.cpp:239-247`), about 200 ms
  after it reaches the window. The trace is drawn then; its row joins
  `SpectrumWidget::m_remoteRowQueue` then.
- The row then waits 0 to 67 ms for the waterfall ticker, and the glide slides
  it in over one row period after that (the "one push period" lag noted in
  `renderGpuFrame`).
- The row times wobble at every step: the Core stamps a row when its FFT
  finished (`DaemonSpectrumSource.cpp:805`), the delay follower moves in
  15 to 40 ms steps (`RemoteDisplayPresenter.h:53-54`, `AudioJitterBuffer.h`
  hold steps), the present timer rounds to whole ms, the ticker fires up to 5 ms
  off its slot and the hand-off to the main thread adds up to 11 ms.

The working tree holds an uncommitted first cut (`RemoteWaterfallRowClock`, a
one-row reserve with a rate trim, pumped from `SpectrumWidget::paintEvent` and the
ticker). It smooths the scroll but adds delay. This task replaces its reserve with
due times.

## The shape

1. **Rows are handed over early with their due time.** When a receive frame is
   accepted and pushed to a main-widget binding's presenter, every waterfall row
   that push queued (the frame's own row when `waterfallAdvance` survives row debt,
   and any Blended rows queued ahead of it) goes to the widget at once, tagged with
   its due time. The trace still waits for its due time, as now. Mini displays keep
   their present path (row and trace together at due).
2. **The waterfall plays rows on a steady grid locked to those due times.** Slots
   one row period apart; a slow correction keeps the slots on the rows' due times
   on average, so Core stamp jitter, follower steps and hand-off jitter do not
   move rows one by one. A large disagreement re-anchors the grid instead of
   slewing.
3. **The slide finishes at the due time.** A row starts sliding in one row period
   before its slot and is fully in at its slot, so the waterfall shows a moment
   when the trace and the audio do.

## Global Constraints

- Repository rules in `CLAUDE.md` and `CONTRIBUTING.md` bind this task: no raw
  `new`/`delete`, `constexpr` constants, braces on all control flow, `m_camelCase`
  members, `kPascalCase` constants, `qCWarning` for errors, no exceptions, don't
  remove code you didn't add (beyond the first-cut code this task replaces).
- This is NereusSDR-original code (not a port). Touched files carry a
  "Modification history (NereusSDR)" line dated 2026-10-21, "J.J. Boyd (KG4VCF),
  AI-assisted via Claude Code". New files use the `WaterfallTicker.h` header style
  ("NereusSDR-native"). The reference notes already in `RemoteWaterfallRowClock.h`
  (Thetis `display.cs:6541-6557 [v2.10.3.15]`, AetherSDR `[@a9a83e66]`) stay.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
  `RemoteDisplayPresenter` and `RemoteMediaController` live under `src/gui/`.
- Build with the session's offline script, never a fresh configure that downloads:
  `bash /private/tmp/claude-501/-Users-j-j-boyd-NereusSDR--claude-worktrees-nereus-xverter-support-6cbfb9/a2419bd8-e379-4928-a4d9-0b231c1002ad/scratchpad/build.sh <targets...>`
  (prints `BUILD EXIT n`; log in the same folder, `build.log`). Run it under
  `bash`, never zsh word splitting.
- Run test binaries directly, offscreen, with an isolated home, and `-maxwarnings 0`:
  `H=$(mktemp -d <scratchpad>/home.XXXX); HOME=$H CFFIXED_USER_HOME=$H NEREUS_TEST_HOME=$H QT_QPA_PLATFORM=offscreen ./build/tests/<test> -maxwarnings 0`.
  Never build or run the whole suite. `tst_imd_waterfall_annotations` has a
  native-window case (`nativeGpuWrapPauseRewindGlideCapture`) that fails offscreen
  by design; run the other cases offscreen and leave the native one to the
  controller.
- Commits are GPG-signed (never `--no-gpg-sign`), with no Claude co-author
  trailer. Do not push.
- Behavior that must not change: receive rows are never drawn while the pan is
  keyed or while the transmit analyzer owns the waterfall; a row captured before a
  tune or zoom is drawn reprojected at the frequency it was captured at; a local
  pan's waterfall (ticker at the update period, glide from `m_wfLastPushMs`) is
  untouched; `tickWaterfallForTest()` keeps its one-row-a-tick drain for the
  existing tests; mini displays are untouched.

## Task 1: Hand remote rows over early and play them on a grid locked to their due times

**Model:** opus.

**Files:**
- `src/gui/RemoteDisplayPresenter.h/.cpp`: record the rows each `push()` queues.
- `src/gui/RemoteMediaController.cpp`: hand those rows to the widget with their
  due time; stop queuing rows at due for main-widget bindings; drop early rows
  wherever the presenter forgets queued items.
- `src/gui/SpectrumWidget.h/.cpp`: carry a due time on `RemoteWaterfallRow`; the
  pump and glide use the grid.
- `src/gui/spectrum/RemoteWaterfallRowClock.h/.cpp`: replace the reserve with the
  locked grid (keep the class and file names; rewrite the rationale comment's last
  part to describe the new shape).
- `tests/tst_remote_waterfall_cadence.cpp`, `tests/tst_remote_display_presenter.cpp`,
  `tests/tst_remote_media_controller.cpp`: tests below.

**Interfaces (names may be adjusted if the code reads better, keep the meaning):**
- `RemoteDisplayPresenter`: a way to take the rows the last `push()` calls queued,
  in order (Frame items whose row survives row debt, and Blended items), e.g.
  `std::vector<Item> takeQueuedRows()`. Repeated rows are synthesized in
  `takeDue()` and keep going to the widget there.
- `SpectrumWidget::enqueueRemoteWaterfallRow(...)`: an overload or parameter that
  takes the row's due time as nanoseconds from now (`dueInNs`, may be negative).
  The widget stores `dueNs = remoteRowNowNs() + dueInNs`, so the controller's
  audio clock (`d->audio->nowNs()`) and the widget's steady clock never have to be
  the same clock. A row queued with no due time (no map, an older Core, a
  Repeated row) is due now.
- The row the trace path built (`m_wfRenderedPixels` with the visual-notch dent)
  must still be the pixels drawn: building the early row from
  `item.frame.waterfallDbm` with `enqueueRemoteWaterfallRow`'s own dent handling
  is acceptable. `updateRemoteSpectrum` must no longer queue the row for a
  main-widget binding in early mode (it still renders the trace), and the
  captured-before-tune path in `presentDueDisplay` must not queue it a second time.
- `RemoteWaterfallRowClock`: `advance(nowNs, queuedRows, frontDueNs)` returns how
  many rows to put in now; `progress()` is the glide toward the last row put in,
  reaching 1 at that row's slot.

**Grid behavior (the acceptance cases below pin these):**
- The first row starts the grid: its slot is its due time. Each later slot is the
  previous slot plus the row period over the rate.
- A row is put in when the clock reaches its slot minus one row period (the slide
  start), not before. Its glide runs 0 to 1 from slide start to slot.
- Lock: for each row put in, measure `error = due - slot`. Smooth it and correct
  phase (a small fraction of the smoothed error per row) and rate (bounded, at
  most 2%), so the slots follow the due times on average with no visible step. A
  follower step of 20 ms is absorbed over many rows, never as one jump.
- Re-anchor when `|error|` exceeds a bound of a few row periods (a retune, a new
  context, the audio hold growing by hundreds of ms): the next slot becomes that
  row's due time.
- A row not in hand at its slide start: the glide rests at 1. When it arrives, it
  goes in at once and the grid takes `now + one period` as that row's slot (late
  by less than the re-anchor bound), then locks back toward the due times.
- Rows far behind the clock (due before now minus a period, e.g. after a main
  thread stall) go in at most two a frame, gliding, as the first cut does.
- Re-anchor (and the presenter-style forgetting below) also on `reset()`, a new
  period, and clearing the queue.

**Forgetting early rows.** Every place that calls `binding.presenter.reset()` for
a main-widget binding forgets items the presenter had queued; rows already handed
over early for those items must be forgotten too, so nothing queued before a key,
a context change or a teardown is drawn after it. Audit each `presenter.reset()`
call site in `RemoteMediaController.cpp` (about lines 2700, 5864, 6544, 7597,
7677, 7719, 7973) and say in the report what each does. `restartChain()` keeps
queued items, so it keeps early rows. The key edge at about line 2700 is the one
that matters most: rows handed over before the key whose slots fall in the over
must not be drawn.

**IMD row tags.** `drainRemoteWaterfallRows`/`putRemoteWaterfallRows` decide
`imdRealReceiverRow` and pass `row.imdAcceptedOrdinal` to `pushWaterfallRow`;
today the ordinal comes from accepting the same frame's trace in
`updateRemoteSpectrum`, which happens at due. With the slide finishing at due, a
row goes in one row period before its trace is accepted. Keep the IMD waterfall
annotations correct: find how `appendHistoryRow` stores the ordinal and how the
annotations read it, and attach the ordinal when the trace is accepted (for
example by remembering the history row written for each recent encoder sequence
and filling its ordinal in on acceptance), or another sound way. Explain the
choice in the report. `tst_imd_waterfall_annotations` (offscreen cases) must pass.

**Acceptance cases (each a test unless noted):**
1. Clock, steady stream: rows due every 66.67 ms with up to 15 ms jitter on the due
   times, handed over 200 ms early, frames at 60 Hz for 30 s. Every row goes in at
   its slide start (one per slot, never two in a frame), the glide moves on every
   frame, and every slot is within 10 ms of its row's due time after the first
   2 s.
2. Clock, follower step: the same, with all due times from row 150 on shifted
   +20 ms (and a second run with -20 ms). No frame's glide step exceeds twice the
   normal per-frame step, and the slots are within 5 ms of the new due times after
   5 s.
3. Clock, drift: due times spaced 0.5% long, and a run 0.5% short, for 120 s. No
   rest, no double rows, slots within 10 ms of due after 10 s.
4. Clock, re-anchor: due times jump +400 ms (the hold grew): the grid re-anchors
   within one row and does not slew for seconds; the waterfall rests rather than
   playing rows early.
5. Clock, late row (no early hand-off): rows handed over at their due time (an
   older Core with no map): rows still go in one per slot with the glide moving,
   one row period behind due, no oscillation.
6. Clock, gap: rows missing for 200 ms then arriving together with their own due
   times: the glide rests at most once and the rows play one a slot after.
7. Clock, stall: no frame for 400 ms: the next frames put rows in at most two a
   frame.
8. Presenter: `push()` of a row-carrying frame after a gap queues Blended rows and
   the frame row; the early-row list holds them in order with their producer
   times; a frame whose row is owed back under row debt is not in the list.
9. Controller (in `tst_remote_media_controller`): a main-widget binding's row
   reaches the widget's queue when the frame is pushed, before its trace is drawn,
   with the due time the presenter will use; the trace is still drawn at due; a
   key forgets rows handed over before it whose slots fall after the key; a mini
   display still gets row and trace together at due.
10. Widget: rows put in by the grid; the glide's progress reaches 1 at a row's
    slot (`remoteRowClockForTest()`), and a tune between hand-off and put-in still
    reprojects the row.
11. Every existing test in `tst_remote_waterfall_cadence` that still describes
    kept behavior passes; the reserve-specific cases are replaced by the cases
    above. `tst_remote_display_presenter`, `tst_remote_media_controller`,
    `tst_remote_spectrum_render`, `tst_remote_display_hold`,
    `tst_waterfall_defaults_changes`, `tst_spectrum_widget_keyed_levels`,
    `tst_mox_display_controller`, `tst_imd_waterfall_annotations` (offscreen
    cases) and `tst_spectrum_gpu_waterfall_ring_seam` pass.
12. Each clock test fails if the lock is removed (prove one: say which case fails
    with the phase and rate correction set to zero, in the report).

**Verification:** the tests above, built and run as the constraints say; the
report lists each test binary's Totals line. On-screen evidence (a screen
recording of JJ's window measured frame by frame) is the controller's, after the
manager builds the combined build; mark it pending.

**Out of scope (follow-up, not this task):** Core-side row stamps on sample time,
pans slower than the engine getting uneven row gaps, and flagging rows the Core
drops.

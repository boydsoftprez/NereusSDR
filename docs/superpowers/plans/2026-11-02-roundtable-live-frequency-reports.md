# Roundtable live-frequency reports implementation plan

> For agentic workers: execute the assigned independent desktop, iPhone,
> and web-verification tasks under the existing cost-aware workflow.

**Goal:** Show live receiver reports and authoritative doubles automatically
at the active listening frequency without slowing the client waterfall.

**Architecture:** Reuse the existing frequency views, keyed participants,
raw receiver readings, formatting preferences, and native card components.
Keep presentation independent from tuning and prioritize active-frequency
RF information over presence. Preserve Brian Sutton's original PR #732
commit while integrating its performance changes and regression fixes.

**Tech stack:** Qt/C++ desktop, SwiftUI iPhone, existing JavaScript website.

**Spec:** `docs/architecture/2026-10-29-roundtable-frequency-first/live-reports-addendum.md`

## Constraints and verification boundary

- No I/Q, WDSP, audio, RF detection, transmit policy, or wire-schema changes.
- Preserve frequency/family/source-room/turn scoping and local display units.
- A double's raw reading describes combined RF; never attribute it to one
  participant. No invented names or overlap inferred from level alone.
- Use measured font/layout bounds and retain readable disclosure when the
  physical space cannot hold every complete row.
- Preserve normal profiles; no device or running-app changes in workers.
- Build focused targets before their anchored tests. Run the final native
  full-suite gate once for the combined reviewed head through required CI.
- DFNR-off focused test applications are not operator replacement builds.

## Tasks

1. **Client performance integration.** Merge original PR #732 into current
   main in the isolated worktree. Fix CPU repaint fallback and immediate
   structural/message/link transitions. Verify sample replay reconstruction
   and texture-invalidation counts, focus retention, clock updates, and
   delivered CPU paints without new FFT. This task is implemented and its
   focused tests have passed; integrated review is still required.
2. **Desktop passive reports.** Inspect current selected-slice/view wants
   against the iPhone's existing wants. Ensure active-frequency reports
   remain subscribed without opening a panel. Prioritize complete report
   rows over presence at that frequency; allow a single valid reporter.
   Show known double participants and combined reports, historical overlap,
   missing/stale states, and transition back to single-talker data safely.
   Relevant files: `RoundtableTagLayout.cpp/.h`, `SpectrumWidget.cpp/.h`,
   `MainWindow.cpp`, and only necessary `RoundtableModel` client-control
   subscription code. Tests: tag layout, panel/desktop flow, station link
   if subscription behavior changes, and spectrum overlay dirtiness.
3. **iPhone presentation.** Reuse `RoundtableTagLayer.swift` semantic cards
   and existing model wants. Prioritize current RF/reports in active cards;
   remove the two-person eligibility restriction for a valid report.
   Display authoritative double identities and combined raw measurements.
   Keep all format choices, complete identity rows, accessibility, fitting,
   historical and stale semantics. Exercise presence-stack/card/model tests.
4. **Web consistency.** Verify the existing frequency report view already
   opens automatically and labels doubles, identities, and combined RF
   correctly. Make only necessary scoped changes, with existing JS/Python
   page probes. No backend or public deployment unless a change is needed.
5. **Integrated review and delivery.** Review transitions, scoped duplicate
   identities, one reporter, third overlap participant, unknown signal,
   reconnect/staleness, hidden sharing, browsing another frequency, narrow
   geometry, font differences, and no accidental tuning. Commit with
   signatures and preserved contributor ancestry; open integration PR and
   run required CI. Build the full-feature combined desktop/iPhone packages
   from the final source and stage before any operator restart request.

## Review focus

Measurements from an ended double must not be relabeled as a remaining
talker's report. Presence is not transmitting. A participant's multiple
slices must not fabricate a double. Changing report values must retain
focus/action identity and leave the spectrum's static caches clean. Active
reports must not silently stop when another detail view is opened.

## Desktop delivery boundary, 2026-11-03

The desktop performance, subscription and passive-report tasks are implemented.
The independent combined review's desktop live-participant finding and the
PR732 scheduling/clock/repaint findings have been corrected. The latest four
focused desktop targets pass 179 cases, including actual child paint delivery.
This desktop integration commit does not include the separate uncommitted
iPhone or web/server work. Their remaining review findings and browser-suite
failures still require their own completion and delivery.

The next Mac package combines this desktop source with current main's
PureSignal/G2E corrections and the reviewed Classic/Modern container controls.
Required final native validation and full-feature packaging remain pending
until that exact combined source is built.

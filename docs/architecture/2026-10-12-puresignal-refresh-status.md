# PureSignal refresh: status against the design

Date: 2026-10-25. Audited against `origin/main` at `c8510c42a`. Every
file:line cite below is at that commit; later commits move some lines.

This compares the [design](2026-10-12-puresignal-refresh-design.md), the
[plan](2026-10-12-puresignal-refresh-plan.md) and the
[mockups](2026-10-12-puresignal-refresh-mockups/README.md) with the code on
`origin/main`. Every claim below was checked in the source, not taken from a
commit message. No code was built or run for this audit.

## Summary

The refresh shipped. PR 450 (merge `8a71a469c`, branch
`codex/puresignal-ui-publication-20261013`) carried all seven plan tasks into
the 2026.10.1 release (tag `v2026.10.1` contains the merge). The files that
are PureSignal-only (the `Imd*` files, `AmpView*`, `PsForm`,
`PureSignalInstruments.h`, `PureSignalViewStyle.h` and the applet) have not
been touched since. The shared files have: after the merge and before the
audit base, `6dbb9d964` and `113b76863` (the spectrum overlay rebuilds only
when it changes) and `5f0defd3f` / `f8bd21ad8` (remote waterfall rows on
their due times) changed IMD lines in `SpectrumWidget.cpp` and
`MainWindow.cpp`, so the cites below already include them.
`MoxDisplayController.cpp` and `PanadapterApplet.cpp` also have later
commits (the display pill, PR 443, and `8461be26b`, heap menus and ASIO
callers). After the audit base, the CTUN retune fixes for issue 648 (PR 655)
changed `SpectrumWidget.cpp` and `MainWindow.cpp`, but no IMD lines.

PR 450 also carried the design, the plan and the mockups themselves. They are
on `origin/main` byte-for-byte identical to local commit `5898faddc`, so that
local commit has nothing left to publish. PR 450 added the planned diagnosis
record ([2026-10-12-puresignal-imd-diagnosis.md](2026-10-12-puresignal-imd-diagnosis.md)),
a verification record
([puresignal-refresh-verification.md](../development/puresignal-refresh-verification.md))
and a maintainer bench procedure
([puresignal-imd-bench-check.md](../development/puresignal-imd-bench-check.md)).

| Group | Count |
| --- | --- |
| Shipped as designed | 21 |
| Shipped differently | 8 |
| Partly done | 2 |
| Not started | 2 |

The main item still open is the live two-tone check on a real radio. The
design was written because of an IMD failure reported on the maintainer's
station, and that symptom has never been reproduced or confirmed fixed on the
air.

## Shipped as designed

| # | Item (plan task) | Evidence on `origin/main` |
| --- | --- | --- |
| 1 | Double-crop defect fixed: IMD runs on the visible pixels once (Task 1) | `src/gui/SpectrumWidget.cpp:12687-12694` passes `m_renderedPixels` straight to the presenter; regression `displayedPixelsAreNotCroppedTwice` in `tests/tst_imd_spectrum_integration.cpp` |
| 2 | The keyed TX pan owns the measurement; focus does not move it (Task 1) | `src/gui/MainWindow.cpp:18411`, `:18429` use `keyedPanId()`; `src/gui/MoxDisplayController.cpp:144-165` mirrors the testing and Show gates to every pan |
| 3 | Gate-off clears the GPU overlay while MOX stays on (Task 1) | `SpectrumWidget.cpp:11676-11700` (`setTestingIMD`, `setShowIMDMeasurements` invalidate and mark the overlay dirty); `tests/tst_spectrum_gpu_dynamic_overlay_clear.cpp` |
| 4 | Diagnosis record saved beside the plan (Task 1) | `docs/architecture/2026-10-12-puresignal-imd-diagnosis.md` |
| 5 | Measurement frame contract, exactly as the plan wrote it (Task 2) | `src/gui/ImdMeasurementFrame.h:10-38` |
| 6 | One analysis per new frame, status order, partial peaks kept, 10 dB trigger kept (Task 2) | `src/gui/ImdMeasurementPresenter.cpp:74-135`; tests `sameFrameDoesNotAdvanceEma`, `missingProductClearsAggregateAndRecoveryReseeds` |
| 7 | Painting reads the cached frame only; no matching or smoothing in paint (Task 2) | `SpectrumWidget.cpp:12717-12723` |
| 8 | Stronger-tone reference per product, weaker-tone reference for worst IMD, OIP formulas unchanged (Task 2) | `tests/tst_imd_measurement_presenter.cpp` `unequalToneReferencesStaySourceFaithful`; reference text at `src/gui/ImdMeasurementPanel.cpp:224` |
| 9 | Rail above the trace, inspector inside the same pan beside the spectrum (Task 3) | `src/gui/PanadapterApplet.cpp:54-62` |
| 10 | Eight statuses with plain explanations and blank numbers when invalid (Task 3) | `ImdMeasurementPanel.cpp` `statusText`, `refresh` (dashes for absent values) |
| 11 | Markers: circles, diamonds, squares; hollow and dashed when expected only (Task 3) | `SpectrumWidget.cpp:12728-12760` |
| 12 | Keys 1 to 6 select the same products as the buttons (Task 3) | `ImdMeasurementPanel.cpp:232-239` |
| 13 | Per-pan `PureSignalImdInspectorVisible` (default False) and `PureSignalImdGuidesVisible` (default True) (Task 3) | `SpectrumWidget.cpp:1318-1319`, `:12582-12596`; test `inspectorPreferenceIsPerPan` |
| 14 | Compact applet: correction status, feedback bar and class, calibration counts, Auto / Calibrate / 2-Tone, Save / Restore, Details, right-click to Bench (Task 4) | `src/gui/applets/PureSignalApplet.cpp:171-203`, `:220-240`; `src/gui/PureSignalInstruments.h:211-251` |
| 15 | Bench: instruments on the left, Calibration and IMD tabs, diagnostics beneath, file footer, On Top (Task 4) | `src/gui/PsForm.cpp:285-330`, `:594-619` |
| 16 | Bench IMD tab shows the keyed pan's frame; losing the owner sends an invalid frame (Task 4) | `PsForm.cpp:1573-1577`; `MainWindow.cpp:18342-18353`, `:18432` |
| 17 | Hardware peak reference moved to the full window, not shown as correction strength (Task 4) | `PsForm.cpp:876` |
| 18 | PS-RX / PS-TX spectra stay disabled with the reason; PIN / MAP / STBL / TINT / Relax not revived (Task 4) | `PsForm.cpp:584-588`; no such controls in `PsForm.cpp` |
| 19 | AmpView linked plots: magnitude above phase, shared cursor, each curve read in its own X domain, series toggles, facade feed kept, Current / Retained caption (Task 5) | `src/gui/AmpViewChart.cpp:221`, `:249`, `:376-377`; `src/gui/AmpViewWindow.cpp:155`, `:211-221`, `:401-422`, `:490`; tests `cursorInterpolatesEachDomain`, `gainZeroAndInvalidDomainsAreBlank` |
| 20 | Waterfall frequency guides and row-attached correction events, bounded and reset with history (Task 6) | `SpectrumWidget.cpp:8501-8550`; `src/gui/ImdWaterfallAnnotations.cpp`; all six named tests in `tests/tst_imd_waterfall_annotations.cpp` |
| 21 | Independent review, PR description and release note (Task 7) | Verification record (reviews "Ready" for `091450a2` and `cd059a1`); `CHANGELOG.md` 2026.10.1, "Transmit, two-tone and PureSignal" |

Every test file the plan named exists and is registered in
`tests/CMakeLists.txt`, plus one the plan did not name
(`tst_imd_rail_controls`).

## Shipped differently

| # | Difference | Evidence | Judgment |
| --- | --- | --- | --- |
| 1 | The rail has **Enable DUP** and **×** (hide measurements) buttons. Neither is in the mockup. The design said the rail explains a missing prerequisite and never turns it on by itself. | `ImdMeasurementPanel.cpp:50`, `:62`, `:186-200`; `MainWindow.cpp:18336-18339` | It only acts on a click and only shows while DUP is the missing gate, so it does not break the design's rule, but it is a new control that changes DUP. It shipped in 2026.10.1 and the changelog lists it. Kept by manager ruling; see Decisions. |
| 2 | A Nereus-only freshness timeout: current readings expire after the longer of 1 s or five frame intervals. | `SpectrumWidget.cpp:12650-12661` | Not in the design. It stops stale numbers looking current. It is a GUI rule, not a DSP or protocol change, and it is documented in the diagnosis record. |
| 3 | AmpView keeps a sorted copy of the measured points for cursor lookup, because WDSP publishes them out of order. | Diagnosis record, "source-order audit"; `AmpViewChart.cpp:163` | A correct refinement the design did not foresee. DSP samples are untouched. |
| 4 | The Bench mockup showed Diagnostics as a third tab; the code puts diagnostics beneath the instruments. | `PsForm.cpp:320-323`, `:602-603` | Matches the design text, which wins over the mockup. |
| 5 | The mockup's "Correction solutions (cor.cnt)" and "Rejected sample sets (dg.cnt)" became "Successful calibrations", "Calibration attempts" and "Legacy info[13]". | `PureSignalInstruments.h:250-252` | The design asked for the real counters with honest names; this is that. |
| 6 | The source line reads **Receiver spectrum**, not the mockup's "TX feedback spectrum". | `ImdMeasurementPanel.cpp:180` | Deliberate. The design forbids calling the pixels TX feedback until the source is proven. |
| 7 | Product buttons run Tone L, Tone U, IMD3 L, IMD3 U, IMD5 L, IMD5 U. The mockup ordered them by frequency, IMD5 L to IMD5 U. | `ImdMeasurementPanel.cpp` `kNames` and `reflow` | Cosmetic. |
| 8 | Waterfall correction marks use fixed colors instead of workspace color roles, and the applet's "correcting" fact is worked out from feedback above 90 rather than by calling the coordinator's predicate. | `SpectrumWidget.cpp:8527-8528`; `PureSignalInstruments.h:215` | Both small. The feedback test is the same rule Thetis uses (cited at `PureSignalInstruments.h:213`). |

## Partly done

| # | Item | Where it stands |
| --- | --- | --- |
| 1 | Real Qt captures on Mac, Windows and Linux, on both the CPU and GPU paths (Task 7) | Mac only: applet, Bench, rail and inspector at DPR 2 in the new and classic looks, with Metal for the spectrum. AmpView captures are CPU only. No Windows or Linux captures. Windows builds the code: PR 450's own branch carries `0e859e1ee` (applet DLL export) and `ef38ac349` (warning signs drawn as text on Windows), and 2026.10.1 shipped Windows installers. No Windows screenshot or test result for these surfaces is on record. |
| 2 | Realtime-labeled tests and a second full-suite pass (Task 7) | Not run during the work because the maintainer's console was live. Left to the release integration owner. |

## Not started

| # | Item | Notes |
| --- | --- | --- |
| 1 | Maintainer-keyed live two-tone check on Protocol 2 (Task 7; design section 8) | The procedure is written in `docs/development/puresignal-imd-bench-check.md` and its result line still reads "pending maintainer operation". No live RF, coupler or before-and-after correction result exists. The original reported IMD failure has not been reproduced or confirmed fixed. |
| 2 | The inspector's "Tone separation" reading from the IMD mockup | Not in `ImdMeasurementPanel.cpp`. Minor; the design text does not require it. In progress; see Decisions. |

## What this means

There is no design or plan work left to hand to a crew. The one thing that
matters is the live check, and only the maintainer can key the radio for it.
It fits the 2026.10.2 bench pass: follow the bench-check procedure with DUP
and Show 2Tone measurements on, a span wide enough for both fifth-order
products, and record the six product frequencies, the status line and the
correction-applied change.

The maintainer's local `main` held the design as commit `5898faddc` on an
older base. Its content is already on `origin/main`, so it does not need
pushing.

## Decisions (2026-10-25)

On the evening of 2026-10-25 the maintainer handed open calls like these to
the manager session, in his words: "I believe in you and trust your judgment
move my project forward please". Under that handover the manager ruled, at
about 20:55 EDT, and put the ruling on the maintainer's veto list:

- The rail's **Enable DUP** button stays.
- The PS-A attenuator gate fix (PS-A drives the force-31 rules, as Thetis
  does) belongs to its own session and is not part of this audit.

Settled on the audit's recommendations, open to the maintainer's veto:

- The live two-tone check runs in the 2026.10.2 bench pass, keyed by the
  maintainer, following the bench-check procedure.
- Product buttons keep their shipped order (tones first, then IMD3, then
  IMD5).
- The applet keeps working out "correcting" from feedback above 90; it is the
  Thetis rule.
- Realtime tests and the broad suite belong to the 2026.10.2 release
  integration run, not to new work here.
- Local `main` is left as it is; nothing in the primary checkout is reset.
  The duplicate commit is harmless.

Follow-up work, each small and GUI-only. Items 1 and 2 are in progress on
branch `claude/puresignal-imd-polish` for 2026.10.2, with no PR opened yet;
item 3 runs with the 2026.10.2 bench pass once the Windows stations have the
release candidate.

1. The inspector's **Tone separation** reading: the gap between the two
   measured tones, blank unless both are measured. Thetis shows the same
   value as "f0 diff" (`display.cs:5652`, `:5695`), so it is a port.
2. Waterfall correction marks take workspace color roles with the classic
   fallback, as design section 7 asks for markers.
3. A Windows and Linux capture pass of the applet, Bench, AmpView and IMD
   rail, as Task 7 asked. This checks the shipped code and changes no code
   unless it finds a defect.

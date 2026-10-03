# EQ and CFC verification

Execution branch: `codex/eq-cfc-ui`. Base: `e342467fb3482bb84a61324fb10cef90491533de`. Current implementation revision: `39fa0402032ff01e0fe5b254e6f93a79ccb56a05`. Component captures reflect `b344b03e3b3ad1c31ae64ecff24474ada33ac5db`; the subsequent correction changes publication/history and teardown rather than layout.

## Automated evidence

Changed targets were rebuilt before their focused checks. The CFC profile/routing implementation passed six suites; the TX editor passed five suites; the final CFC transition and graph-rendering correction passed seven suites. Raw build, test, capture and signature logs are retained in the ignored `.crew/2026-10-02-eq-cfc-ui-plan/` evidence directory.

Final gate at `31b6850ceea723c3bba0eacd835764962a781299`:

- `cmake --build build --target NereusSDR all_tests -j 4`: exit 0.
- `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`: **609/609 passed, zero failures**, exit 0, 473.16 seconds.
- Raw evidence: `final-build-after-fixture-fix.log` and `final-test-after-fixture-fix.log` in the Crew directory above.
- Signed commits and repository attribution/architecture hooks passed; signatures were verified.

The initial full run passed 606/609 suites. Three unchanged country/ADIF fixture tests failed because relative compiler source paths resolved under CTest's working directory. The same binaries passed from the build root. The controller corrected only their fixture helpers/CMake definitions to use the existing configured source-root pattern, preserving every assertion. The three rebuilt tests passed under normal CTest before the successful complete rerun. No parser or application logic changed in this test-only correction.

The CFC integration checks use an isolated local WDSP channel and verify complete queued profile arguments. They do not exercise a connected radio or microphone.

## Native component renders

The images below are native Qt components rendered by test executables with `QT_QPA_PLATFORM=offscreen`, rather than screenshots of the running application. The final capture runs at scale factors 1, 1.25 and 1.5 all exited successfully. Captures cover logical 1000×720 and 853×500 dialog sizes; they verify dialog layout, text, plotting and scrolling independently of desktop window frames. Controller image inspection found and corrected overlapping band chips and short-plot axis labels.

| Component | 100% | 125% | 150% |
| --- | --- | --- | --- |
| Graphic EQ | [Capture](graphic.png) | [Capture](graphic-125.png) | [Capture](graphic-150.png) |
| Parametric EQ | [Capture](parametric.png) | [Capture](parametric-125.png) | [Capture](parametric-150.png) |
| CFC | [Capture](cfc.png) | [Capture](cfc-125.png) | [Capture](cfc-150.png) |

These fixture images use example settings, not the application's default profile. Eighteen-band controls use their own horizontal chip strip, while exact entries, globals and Advanced controls remain vertically scrollable when the available height is small. Graphs remain within the dialog viewport.

## Pending desktop verification

Actual application checks at normal, 125% and 150% scaling on a 1280×800 display remain pending. Native computer control reports that the Mac is locked; an unlock request is pending. Component renders do not satisfy the whole-application scaling and interaction acceptance check. No radio was connected or operated for this verification.

## Audio compatibility

CFC supports all 5/10/18 configured bands and separate compression/post-EQ Q arrays. Q arrays are passed only when both graphs enable Q. Legacy profiles keep their ten-band configuration and disabled Q until edited.

The approved scope retains the existing TX EQ audio conversion. In ten-band parametric mode, Q affects the configured display and saved settings only. Five- and eighteen-band curves are sampled into the ten-band audio profile. The adjacent TX Width label and user guide disclose this limitation. An additional TX engine extension is awaiting the maintainer's scope choice.

## Review and delivery

The independent whole-branch review identified three required corrections: batched immutable Graphic EQ publication during bulk reset/history restore, per-event CFC numeric wheel undo boundaries, and dated RadioModel port metadata. The correction wave passed six affected suites (206 Qt passes, no failures or skips), including regressions at the accepted WDSP argument boundary and real wheel/focused-teardown cases. Both dialog destructors now finalize focused entries while editing state is still alive. The scoped re-review found all required findings addressed and no new Critical or Important defect in the corrections, including the related teardown fixes. The combined final build and full suite passed. The branch includes the user guide and accurately captioned component captures. Whole-application acceptance remains pending as described above. No push, merge or release is part of this delivery.

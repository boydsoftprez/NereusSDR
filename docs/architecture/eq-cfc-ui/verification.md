# EQ and CFC verification

Execution branch: `codex/eq-cfc-ui`. Historical implementation base: `e342467fb3482bb84a61324fb10cef90491533de`. Historical implementation revision: `39fa0402032ff01e0fe5b254e6f93a79ccb56a05`. Component captures reflect `b344b03e3b3ad1c31ae64ecff24474ada33ac5db`; the subsequent correction changes publication/history and teardown rather than layout.

## Accepted Core GUI integration

The next preview combines the previously signed EQ/CFC/latest-main/Radxa source (`ba147d0d420ec2129f8678a04a9ad117b5d055a2`) with the immutable signed Core Settings/logger/audio/PS component source (`5d425542048fb4c2348553c99a3ec79ab4929829`). The handoff packet and all nineteen artifact hashes were verified, including its raw 1,081-case ordinary test result. Input acceptance was distinguished from verification of this combined source.

Eleven merge conflicts were reconciled without replacing the native EQ/CFC editors, exact model state or four guarded audio bind/replay methods. Both Max Bin interfaces and the actual slice-host, history, authenticated endpoint and transport-retirement fixes remain. Independent integration review found one build blocker: duplicate waterfall fixture accessors. Removing the duplicate pair left the original API and all rendering behavior unchanged; scoped re-review found no remaining Critical or Important finding.

The corrected frozen source tree `d1e85e372b68e2101166994c1366a72c24f5e732` was rebuilt in **`build-core-gui`**, separately from the running `build-latest` preview. Its application, focused targets and full `all_tests` target built successfully. The full ordinary gate passed **1,085/1,085** suites, exit 0 (263.14 seconds); the separately leased realtime gate passed **22/22**, exit 0 (401.37 seconds). All **1,107 registered suites passed** across these two runs. All thirteen current compliance, attribution and architecture checks also passed.

The initial focused run passed 53/54 suites; its sole failure was an unbuilt sibling fake-capture executable. Building `tst_capture_admission` and rerunning the unchanged `tst_tx_worker_remote_ring` suite passed (8.41 seconds). The subsequent full run includes both and passed without that failure. No source or assertion change was made for the fixture issue. Raw evidence is retained as `accepted-core-gui-*.log`, corresponding result JSON files and `accepted-core-compliance-*.log` in `.crew/2026-10-02-eq-cfc-ui-plan/`.

The running `radxa_5c_r3` preview remains the verified `ba147d0d4` application until native control can close it normally and launch the replacement. Its files were not overwritten. The deployed Radxa Core was not installed, replaced or restarted. Whole-application EQ/CFC/Settings interaction and scaling checks remain pending on an unlocked Mac; these automated results do not claim live radio/audio verification.

## Latest-main integration

The implementation is reconciled with `origin/main` at `dd53da5af65127840e2e6e145c7104286ef5453f`, including its current Core/GUI architecture, remote CFC profile validation and full TX parametric F/G/Q audio path. The older gate below applies only to the pre-integration source.

Five desktop fixes from the Radxa preview branch are preserved: `58e169a1d`, `6ec18b027`, `f3919b757`, `8c3ea4b7e` and `c1991b2d4`. The superseded direct-row glide patch is not reapplied over main's newer row-folding implementation. This task does not deploy, replace or restart the Radxa Core. Its separately deployed PS/audio fixes remain under their existing task's ownership.

The fresh `build-latest` application and all 40 focused integration targets built successfully. All **40/40 focused suites passed**, zero failures, exit 0 (169.58 seconds). Evidence: `latest-accepted-focused-build.log` and `latest-accepted-focused-tests.log` in `.crew/2026-10-02-eq-cfc-ui-plan/`. These checks include EQ/CFC editing, profile restore, queued channel lifetime, direct legacy CFC audio publication, display folding, paired endpoint identity and transport retirement.

Independent integration review found a deleted-channel callback hazard, missing legacy CFC aggregate publication and merged test errors. The correction wave reproduced the production failures before fixing them. Scoped re-review found all required findings addressed and no new Critical or Important findings. The full current-source `all_tests` build passed. The non-realtime run passed **1,079/1,079** suites (250.87 seconds). The serial realtime run passed **20/21** suites (411.62 seconds); `tst_dsp_control_transmit` observed a 125.02 ms event-loop gap against an 84.04 ms limit while its log recorded load average 49.55. After competing builds and suites ended, the isolated rerun passed **1/1**, exit 0 (14.09 seconds; starting load average approximately 3.4). Evidence: `latest-all-tests-build.log`, `latest-full-nonrealtime-tests.log` and `latest-full-realtime-tests.log` in the same Crew directory. All **1,100 registered suites now have passing results** on this integrated source across the non-realtime run, serial realtime run and isolated rerun. The first realtime failure remains documented; this is not a claim that its initial combined run was all green. Isolated rerun evidence: `latest-realtime-isolated-rerun.log` and `latest-rerun-machine-processes.txt`.

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

Actual application checks at normal, 125% and 150% scaling on a 1280×800 display remain pending. Native computer control reports that the Mac is locked; an unlock request is pending. Component renders do not satisfy the whole-application scaling and interaction acceptance check. No live radio/audio correctness or transmit operation was performed for these checks. The separately launched Radxa-profile preview follows its saved startup behavior; its connection and visible window have not been verified while the Mac is locked.

## Audio compatibility

CFC supports all 5/10/18 configured bands and separate compression/post-EQ Q arrays. Q arrays are passed only when both graphs enable Q. Legacy profiles keep their ten-band configuration and disabled Q until edited.

The latest-main integration retains the current engine's full TX parametric frequency, gain and Q path for all configured bands. Graphic EQ retains its legacy ten-band inputs. The earlier display-only Q/ten-point conversion limitation no longer describes this integrated source.

## Review and delivery

The independent whole-branch review identified three required corrections: batched immutable Graphic EQ publication during bulk reset/history restore, per-event CFC numeric wheel undo boundaries, and dated RadioModel port metadata. The correction wave passed six affected suites (206 Qt passes, no failures or skips), including regressions at the accepted WDSP argument boundary and real wheel/focused-teardown cases. Both dialog destructors now finalize focused entries while editing state is still alive. The scoped re-review found all required findings addressed and no new Critical or Important defect in the corrections, including the related teardown fixes. The combined final build and full suite passed. The branch includes the user guide and accurately captioned component captures. Whole-application acceptance remains pending as described above. The branch includes a local integration merge of current main. No public push, main-branch merge, release or Radxa Core deployment is part of this delivery.

# Remote pan FFT zoom cap

Execution: run with `crew` under `cost-aware-execution`. One task.

## Why

A remote window zoomed in on a 96 kHz receiver asked the Core for a
262144-point FFT (2.7 s of samples per transform). The waterfall smeared and
crawled (bench 2026-10-09, ANAN G2 via Core, client log
`nereussdr-20261009-114650-767-000001.log` 15:33:59-15:40:06: pan-0 grants
of "FFT 262144 (fine tier)").

The local pan's auto-zoom already caps the size it grows to on zoom:
`kAutoZoomMaxFftSize = 65536`, floored at the user's baseline, with the
rationale in the comment above it (src/gui/MainWindow.cpp ~8936-8960 on main
4a976c479). The remote planner `plannedFftSize` in
src/gui/RemoteMediaController.cpp (~604-628) has no such cap: it grows to
`FFTEngine::maximumFftSize()` (262144). Both the zoom term
(`sampleRate * pixels / span`) and the Hz/bin target term are uncapped there;
the local path caps both. The remote planner must follow the same rule as the
local one.

## Global Constraints

- Read CLAUDE.md and CONTRIBUTING.md first. C++20/Qt6. No raw new/delete, no
  `#define` constants, braces on all control flow, `kPascalCase` constants.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
- This is NereusSDR's own code (no Thetis port); no attribution header change.
- Do not change the local auto-zoom's behaviour (cap value, floor, hysteresis,
  the Hz/bin path). Only move its cap into a shared place and make the remote
  planner use it.
- Do not change the Core's grant logic, the tier rule (`size > baseSize` is
  "fine"), the mini display request, or `kDisplayFftPlanMaxSize` (the
  catalogue's fftPlan that apps follow).
- Build and run single tests only, per docs/development/fast-test-loop.md.
  Test binaries run with `QT_QPA_PLATFORM=offscreen` prefixed on the command,
  never exported over ctest.
- Commits GPG-signed (never `--no-gpg-sign`). No Claude co-author trailer.

## Task 1: Cap the remote pan's zoom FFT size like the local pan's

Files:
- src/core/ControlRanges.h (or another `src/core/` header next to the
  existing `kDisplayFftPlan*` constants): the shared cap.
- src/gui/MainWindow.cpp: the local auto-zoom lambda uses the shared constant
  instead of its local `constexpr`.
- src/gui/RemoteMediaController.cpp: `plannedFftSize`.
- tests: a regression test (see Acceptance).

Interfaces:
- `inline constexpr int kAutoZoomMaxFftSize = 65536;` in `ControlRanges`
  (namespace as the neighbouring constants), carrying the rationale comment
  moved from MainWindow (keep MainWindow's comment pointing at it).
- A small pure helper next to it, e.g.
  `constexpr int autoZoomFftSize(int desiredPow2, int baseline)` returning
  `std::min(std::max(desiredPow2, baseline), std::max(baseline, kAutoZoomMaxFftSize))`,
  used by both paths so they cannot drift. Name and exact shape are the
  implementer's call if a better fit exists; both callers must use it.

Steps (what must be true):
- `plannedFftSize` returns `min(max(fftSizeFor(target), baseSize),
  max(baseSize, kAutoZoomMaxFftSize))`, for both the zoom term and the Hz/bin
  term.
- A user baseline above 65536 (Setup FFT slider / `DisplayFftSize`) is still
  honoured exactly, as on the local path.
- The local lambda's result is unchanged for every input.

Acceptance:
- 96 kHz receiver, pan 1068 px wide, span 300 Hz, baseline 4096, Hz/bin off:
  planned size is 65536 (was 262144). Tier "fine".
- Same, span 96 kHz: planned size 4096, tier "wide" (unchanged).
- Same, span 5549 Hz: 32768 (unchanged; below the cap).
- Baseline 131072, span 300 Hz: 131072 (baseline wins over the cap).
- Hz/bin target 0.5 at 96 kHz (desired 192000): 65536, not 262144.
- Existing tests that zoom remote pans (tests/tst_remote_media_controller.cpp
  around the "deeper zoom ... longer fine FFT" case, ~7679-7760 on main)
  still pass; if one asserted an uncapped size, report it rather than
  weakening it, and say what it asserted.

Verification: unit test of the helper and of the remote planner's requested
`fftSize` (via the outbound subscribe request in an existing remote fixture,
or by exposing the planner for test if that is the house pattern; check how
tests reach RemoteMediaController internals first). Build the touched test
targets and the NereusSDR target; run the new and the affected existing tests.
Hardware: pending (JJ's remote window on the G2 Core).

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

## Rework (2026-10-09 evening): the cap is a time per transform

JJ's ruling (display decisions, question 5, answer A): the zoom limit is a
time per transform of about 0.7 s, the same at every sample rate, with the
engine maximum (`kDisplayFftPlanMaxSize`, 262144) on top. It replaces the
fixed 65536 for the local auto-zoom and the remote planner alike, and
supersedes the 2026-05-08 fixed cap.

Why: auto-zoom keeps about `baseline` points on screen at every zoom
(`size = baseline * rate / span`), so the time per transform at a given
zoom (`size / rate = baseline / span`) does not depend on the rate. A fixed
point count therefore cuts in at a different zoom on every rate: 65536 is
0.68 s at 96 kHz but 43 ms at 1.536 MHz, where deep zoom went blocky from a
96 kHz-wide view. A time limit gives the same deep-zoom look at every rate.
The 2026-05-08 comment's rationale (85 ms replan pause and waterfall ghost
at 768 kHz) is replaced by this one; the longer fill at deep zoom on high
rates is accepted.

Resulting caps (largest power of two <= rate * 0.7 s, clamped to
[kDisplayFftPlanMinSize, kDisplayFftPlanMaxSize]):

| Rate | Cap | Time per transform |
| --- | --- | --- |
| 48 kHz | 32768 | 0.68 s |
| 96 kHz | 65536 | 0.68 s (unchanged) |
| 192 kHz | 131072 | 0.68 s |
| 384 kHz | 262144 | 0.68 s |
| 768 kHz | 262144 (engine max) | 0.34 s |
| 1.536 MHz | 262144 (engine max) | 0.17 s |

## Task 2: Make the shared cap a time per transform

Files: src/core/ControlRanges.h, src/gui/MainWindow.cpp (local auto-zoom
lambda ~8940-9000), src/gui/RemoteMediaController.cpp (`plannedFftSize`),
tests/tst_auto_zoom_fft.cpp, tests/tst_remote_media_controller.cpp.

Interfaces (names are the implementer's call if a better fit exists; both
callers must share them):
- `inline constexpr double kAutoZoomMaxTransformSeconds = 0.7;` replacing
  `kAutoZoomMaxFftSize`, with the rationale above (NereusSDR-native; JJ
  2026-10-09).
- `constexpr int autoZoomMaxFftSize(double sampleRateHz) noexcept`: the
  largest power of two <= sampleRateHz * kAutoZoomMaxTransformSeconds,
  clamped to [kDisplayFftPlanMinSize, kDisplayFftPlanMaxSize]. A rate that
  is not finite or <= 0 returns kDisplayFftPlanMinSize (callers already
  return early on such rates; say so in the comment).
- `autoZoomFftSize(int desiredPow2, int baseline, double sampleRateHz)`:
  `min(max(desiredPow2, baseline), max(baseline, autoZoomMaxFftSize(rate)))`.
  A baseline above the cap is still honoured exactly.
- The local lambda's power-of-two loop stops at `autoZoomMaxFftSize(rate)`
  instead of the fixed constant; floor, hysteresis and the Hz/bin path are
  otherwise unchanged. `plannedFftSize` passes `slice->sampleRateHz()`.

Steps: update the tests first so they fail on the fixed cap, then change
the code.
- Helper: caps per rate as in the table (48k, 96k, 192k, 384k, 768k,
  1.536M), and invalid rates.
- Local formula slots (`formula_caps_at_auto_zoom_max`,
  `formula_holds_at_cap_at_deeper_zoom`, `formula_baseline_above_cap_*`,
  `shared_helper_floors_and_caps`): at 96 kHz unchanged (65536); add rows at
  192 kHz (131072) and 768 kHz (262144). Report any slot that asserted 65536
  at a rate other than 96 kHz rather than weakening it; it is the intended
  change.
- Remote planner rows (`remote_planner_caps_zoom_like_local_data`): keep
  the 96 kHz acceptance rows from Task 1 (all unchanged at 96 kHz); add
  768 kHz, 1068 px, span 3 kHz, baseline 4096 -> 262144 fine (was 65536),
  and 192 kHz span 300 Hz -> 131072.
- Run tst_auto_zoom_fft and the affected tst_remote_media_controller slots,
  then each binary once whole. Build NereusSDR and nereusd.

Hardware: pending (JJ's remote window on the G2 Core at 768 kHz: zoom to a
CW-sized view; the grant reads 262144 fine and the waterfall keeps the same
texture as zoomed out).

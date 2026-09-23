# Tune Step List Port: Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** Replace the six-entry Stage 1 stub step ladder with a source-first port of
Thetis `tune_step_list` (26 entries) and its wrapping `ChangeTuneStepUp` /
`ChangeTuneStepDown`, and route both NereusSDR step controls through the port.

**Architecture:** The ported table and a Hz-to-index lookup live in
`src/models/SliceModel.h`, where the stub lives today. `SliceModel` gains
`changeTuneStepUp()` / `changeTuneStepDown()`, ported from the Thetis console methods
of the same names. The VFO flag STEP button (via `MainWindow`) and the RX applet step
arrows stop iterating a ladder themselves and call those two methods, so one tested
code path owns the wrap.

**Tech Stack:** C++20, Qt6 (Core, Widgets, Test), CMake + Ninja,
`nereus_add_test()` harness, `QSignalSpy`.

**Spec:** no separate design doc. This is a bounded change requested directly by the
maintainer (2026-09-23); every decision is settled in "Settled decisions" below, and
the items the maintainer reserved for himself are listed under "Out of scope".

## Global Constraints

* **Source-first (CLAUDE.md "SOURCE-FIRST PORTING PROTOCOL").** Every value, name and
  wrap formula comes from Thetis `Project Files/Source/Console/console.cs` and
  `TuneStep.cs` at tag `v2.10.3.15` (commit `3759d096`). Read the cited lines before
  writing. If something is not where this plan says, stop and ask; never infer.
* **Thetis clones.** Working clone `/Users/j.j.boyd/Thetis` (HEAD is exactly tag
  `v2.10.3.15`). The tag verifier resolves `[v2.10.3.15]` cites against the pinned
  checkout `/Users/j.j.boyd/Thetis-v2.10.3.15`; both are identical at these lines.
* **Cite stamp is `[v2.10.3.15]`** on every new or modified `// From Thetis` cite,
  placed straight after the line token and followed by a colon, for example
  `// From Thetis console.cs:1953-1982 [v2.10.3.15]: ...`.
* **Inline comments are preserved verbatim** (CLAUDE.md "Inline comment preservation",
  ship-blocking): the index comments `//0` through `//25`, the closing
  `// initialize wheel tuning list array`, the field comment
  `// A list of available tuning steps`, and `//MW0LGE_21j` in both wrap functions.
  The pre-commit tag verifier matches `\bMW0LGE\b`, which cannot see the underscore
  variant `MW0LGE_21j`, so it will not catch a dropped tag here. Preserve them by hand.
* **Attribution.** All touched production files are already registered in
  `docs/attribution/THETIS-PROVENANCE.md` with `console.cs`. Do not refresh or
  re-copy any existing console.cs header block. `TuneStep.cs` is a new source for
  `SliceModel.h` and gets its verbatim header. The new test file gets the verbatim
  v2.10.3.15 console.cs header and its own PROVENANCE row.
* **Verbatim headers are extracted, not typed.** Use
  `extract_source_header()` from `scripts/rewrite-verbatim-headers.py` (it strips the
  UTF-8 BOM that `TuneStep.cs` carries and keeps trailing spaces) on the file under
  `/Users/j.j.boyd/Thetis-v2.10.3.15/Project Files/Source/Console/`.
* **Modification-history entries** use the dated colon form with no em-dash:
  `//   2026-09-23: <what changed> by J.J. Boyd (KG4VCF), with AI-assisted
  transformation via Anthropic Claude Code.` Header mod-history lines carry no
  version stamp.
* **No em-dash (`—`) and no en-dash (`–`) in any new text**: code comments, the
  PROVENANCE row, commit messages. Existing em-dashes in untouched lines stay.
* **No source cites inside user-visible strings.** This task adds no user-visible
  string.
* **GPG-sign every commit.** Never `--no-gpg-sign`, never `--no-verify`.
* **No `Co-Authored-By` trailer** in any commit message.
* **Commit subjects** follow the repo's conventional style, e.g.
  `feat(slice): ...`, imperative, under 72 characters.
* **Pre-commit hook** (`core.hooksPath` = `scripts/git-hooks`) runs every attribution
  verifier automatically. Do not run them by hand before committing; just commit, and
  fix whatever the hook reports.
* **Build directory** is `build/` in this worktree, already configured with
  `-DNEREUS_BUILD_TESTS=ON` and the app already built at the base commit.
* **Tests** follow `docs/development/fast-test-loop.md`: always build the named target
  before `ctest`, because test binaries are `EXCLUDE_FROM_ALL` and a bare `ctest`
  runs stale binaries. Do not run the full suite; the controller runs it once at the
  end.
* **Do not launch the app and do not connect to any radio.** Other sessions are using
  the bench radios.

## Settled decisions

1. **Port the pairs, not only the Hz values.** Thetis `tune_step_list` is a
   `List<TuneStep>` of `(StepHz, Name)` pairs (`TuneStep.cs:44-68`); the names are
   ported verbatim. They are not wired to any display in this change: the STEP
   controls keep NereusSDR's `"%1 Hz"` label (whether to show the Thetis names is a
   maintainer question).
2. **State stays in Hz.** `SliceModel` keeps `m_stepHz` and its persistence key
   `Slice<N>/Band<key>/StepHz`; there is no stored index. Every value of the old stub
   (1, 10, 100, 500, 1000, 10000) is on the new list, so persisted settings carry
   over unchanged.
3. **On-list wrap is Thetis verbatim:** up is `(index + 1) % count`, down is
   `(index - 1 + count) % count` (`console.cs:6124-6134`).
4. **Off-list values** (reachable only through a persisted or hand-edited value that
   is not on the list) move to the neighbouring list entry in the direction of travel:
   up goes to the smallest entry larger than the current value, wrapping to the first
   entry when none is larger; down goes to the largest entry smaller than the current
   value, wrapping to the last entry when none is smaller. This is NereusSDR-native:
   Thetis holds an index, so it has no off-list state. It replaces two inconsistent
   fallbacks (the VFO flag jumped to 1 Hz; the RX applet treated the value as
   index 0).
5. **Consumers map onto Thetis controls:** the VFO flag STEP button is Thetis's left
   click on the step display (`WheelTune_MouseDown`, `console.cs:29034-29038`, calls
   `ChangeTuneStepUp`); the RX applet arrows are `btnChangeTuneStepSmaller_Click` /
   `btnChangeTuneStepLarger_Click` (`console.cs:30635-30643`).

## Out of scope (maintainer decisions, listed as open questions in the PR)

* The 100 Hz default. Thetis starts at `tune_step_index = 2` (10 Hz,
  `console.cs:1984`). Keep `m_stepHz{100}`; only its comment is corrected.
* Per-mode step memory (`m_nTuneStepsByMode`, `TuneStepPerModeRX1`,
  `console.cs:1986-1988` and `11268-11339`). Not ported.
* The STEP label format, `TuneStepButtonItem` (7 invented buttons against Thetis's
  one-per-entry meter buttons, `MeterManager.cs:8040-8066`), and click-to-tune step
  rounding (always on in NereusSDR, opt-in in Thetis, `console.cs:33470-33497`).
  Do not touch `VfoWidget`, `SpectrumWidget` or `TuneStepButtonItem`.

## What already exists

* `src/models/SliceModel.h:144-149`: the stub `kStageOneStepLadder[]` and
  `kStageOneStepLadderSize`, with the stale "11 entries" comment.
* `src/models/SliceModel.h:428-429`: `stepHz()` / `setStepHz(int)`;
  `SliceModel.h:1057`: `int m_stepHz{100};` with the comment
  `// From Thetis tune_step_list[5] = 100 Hz`.
* `src/models/SliceModel.cpp:473-479`: `setStepHz(int hz)` accepts any `hz > 0` and
  emits `stepHzChanged(hz)` only on change.
* `src/gui/MainWindow.cpp:1406-1418`: `VfoWidget::stepCycleRequested` lambda cycling
  the stub upward; its comment cites a stale `:1626-1629` handler and claims the step
  propagates to `activeSpectrumWidget()->setStepSize`, which is false (the spectrum
  step is only seeded once in `MainWindow::wireSliceToSpectrum()`, line 8002; the
  only `stepHzChanged` connection in MainWindow updates the flag label, line 1252).
* `src/gui/applets/RxApplet.cpp:438-489`: step row; `m_stepDown` / `m_stepUp`
  lambdas cycle the stub with wrap; comments name the stub's six values.
* `tests/tst_slice_rit_xit.cpp`: template for a SliceModel-only QtTest (header block,
  `QTEST_MAIN`, `#include "tst_<name>.moc"`); registered in `tests/CMakeLists.txt`
  line 820 with `nereus_add_test(tst_slice_rit_xit)`.
* PROVENANCE rows: `src/models/SliceModel.h` (line 326, multi-source, notes already
  mention `tune_step_list`), `src/models/SliceModel.cpp` (line 325), test rows near
  line 348 (`tests/tst_slice_auto_agc.cpp`).

## File Structure

Modify:

| Path | Change |
| --- | --- |
| `src/models/SliceModel.h` | Stub removed; `TuneStep`, `kTuneStepList`, `kTuneStepListSize`, `tuneStepIndexForHz` added; `changeTuneStepUp/Down` declared; `m_stepHz` comment corrected; TuneStep.cs header stacked; mod-history line |
| `src/models/SliceModel.cpp` | `changeTuneStepUp/Down` implemented; mod-history line |
| `src/gui/MainWindow.cpp` | STEP-cycle lambda calls `changeTuneStepUp()`; stale comment corrected |
| `src/gui/applets/RxApplet.cpp` | Step arrows call `changeTuneStepDown/Up()`; stub comments replaced |
| `tests/CMakeLists.txt` | `nereus_add_test(tst_slice_tune_step_list)` |
| `docs/attribution/THETIS-PROVENANCE.md` | SliceModel.h and SliceModel.cpp rows updated; new test row |

Create:

| Path | Responsibility |
| --- | --- |
| `tests/tst_slice_tune_step_list.cpp` | Table parity, lookup, 26-entry wrap both ways, off-list rule, default guard |

## Task 1: Port tune_step_list and the wrapping step changes

**Requirements:** the maintainer's request of 2026-09-23 (replace the stub with a
source-first port of `tune_step_list`; fix the stale "11 entries" comment; unit test
that cycling up and down wraps across all 26 entries; keep the 100 Hz default; no
per-mode memory). Implements "Settled decisions" 1-5.

**Files:**
- Modify: `src/models/SliceModel.h`
- Modify: `src/models/SliceModel.cpp`
- Modify: `src/gui/MainWindow.cpp` (lines 1406-1418 only)
- Modify: `src/gui/applets/RxApplet.cpp` (lines 438-489 only)
- Modify: `tests/CMakeLists.txt`
- Modify: `docs/attribution/THETIS-PROVENANCE.md`
- Create: `tests/tst_slice_tune_step_list.cpp`

**Interfaces:**
- Consumes: `SliceModel::stepHz()`, `SliceModel::setStepHz(int)`,
  `SliceModel::stepHzChanged(int)` (existing).
- Produces, in `namespace NereusSDR` in `src/models/SliceModel.h`:
  - `struct TuneStep { int stepHz; const char* name; };`
  - `inline constexpr TuneStep kTuneStepList[]` (26 entries, below)
  - `inline constexpr int kTuneStepListSize` (its element count, computed, not
    hard-coded)
  - `constexpr int tuneStepIndexForHz(int hz)`: index of the entry whose `stepHz`
    equals `hz`, else `-1`
  - `void SliceModel::changeTuneStepUp();` and `void SliceModel::changeTuneStepDown();`
    (public, next to `setStepHz`)

**Acceptance:**
- `kTuneStepList` holds exactly these 26 pairs in this order (`console.cs:1955-1980`):
  `(1,"1Hz") (2,"2Hz") (10,"10Hz") (25,"25Hz") (50,"50Hz") (100,"100Hz")
  (250,"250Hz") (500,"500Hz") (1000,"1kHz") (2000,"2kHz") (2500,"2.5kHz")
  (5000,"5kHz") (6250,"6.25kHz") (9000,"9kHz") (10000,"10kHz") (12500,"12.5kHz")
  (15000,"15kHz") (20000,"20kHz") (25000,"25kHz") (30000,"30kHz") (50000,"50kHz")
  (100000,"100kHz") (250000,"250kHz") (500000,"500kHz") (1000000,"1MHz")
  (10000000,"10MHz")`, and `kTuneStepListSize == 26`.
- `stepHz` is strictly ascending across the table.
- `tuneStepIndexForHz(kTuneStepList[i].stepHz) == i` for every `i`;
  `tuneStepIndexForHz(3000) == -1`, `(0) == -1`, `(-1) == -1`, `(20000000) == -1`.
- Up wrap: after `setStepHz(1)`, 26 calls to `changeTuneStepUp()` give, in order,
  entries 1, 2, ..., 25 and then entry 0; call 25 lands on 10000000 and call 26 wraps
  to 1. `stepHzChanged` fires 26 times, each with the new value.
- Down wrap: after `setStepHz(1)`, the first `changeTuneStepDown()` gives 10000000
  (wrap); the next 25 give entries 24, 23, ..., 0; `stepHzChanged` fires 26 times.
- Off-list: from 3000, up gives 5000 and down gives 2500; from 20000000, up wraps to
  1 and down gives 10000000; from 3, up gives 10 and down gives 2.
- Default guard: a fresh `SliceModel` reports `stepHz() == 100`, and
  `tuneStepIndexForHz(100) == 5`.
- Migration guard: each old stub value 1, 10, 100, 500, 1000, 10000 has
  `tuneStepIndexForHz(v) >= 0`.
- `grep -rn "kStageOneStepLadder" src tests` returns nothing.
- The MainWindow STEP-cycle lambda body is a single `slice->changeTuneStepUp();`;
  the RX applet lambdas call `m_slice->changeTuneStepDown()` /
  `m_slice->changeTuneStepUp()` after the existing null guard, with no ladder loop
  left in either file.
- The commit passes the pre-commit hook unmodified.

**Verification:** ordinary feature tier (known contract, so write the test first and
watch it fail to build or fail, then implement). Unit layer only; no UI gate exists
for this desktop app, and the controller handles UI evidence after the task. Commands:
- `cmake --build build --target tst_slice_tune_step_list && ctest --test-dir build -R '^tst_slice_tune_step_list$' --output-on-failure`
- Siblings that exercise `setStepHz` / `stepHz`:
  `cmake --build build --target tst_slice_persistence_per_band tst_slice_rit_xit && ctest --test-dir build -R '^(tst_slice_persistence_per_band|tst_slice_rit_xit)$' --output-on-failure`
- App compile and link: `cmake --build build` (MainWindow.cpp, RxApplet.cpp).
- `grep -rn "kStageOneStepLadder" src tests` (expect no output) and
  `grep -c "MW0LGE_21j" src/models/SliceModel.cpp` (expect 2).
Hardware: none touched. Human smoke on a radio (click STEP through the list on the
VFO flag and the RX applet) stays pending for the maintainer.

**Execution note (advisory):** opus. Touches no networking, config apply, secrets or
anything that transmits. Single task; no prerequisites; not parallel.

- [ ] **Step 1: Test first.** Create `tests/tst_slice_tune_step_list.cpp` covering
  every Acceptance case above as separate slots (class `TestSliceTuneStepList`,
  `QTEST_MAIN`, `#include "tst_slice_tune_step_list.moc"`, pattern of
  `tests/tst_slice_rit_xit.cpp`). The expected table is written out literally in the
  test, not read back from `kTuneStepList`. Use `QSignalSpy` on
  `SliceModel::stepHzChanged` for the emission counts. Header: NereusSDR port block
  (`Ported from Thetis source:` naming `Project Files/Source/Console/console.cs`,
  then the dated mod-history line), followed by the verbatim console.cs header
  extracted from the v2.10.3.15 checkout. Register it in `tests/CMakeLists.txt` next
  to `nereus_add_test(tst_slice_rit_xit)`. Build it and confirm it fails.
- [ ] **Step 2: The table in `SliceModel.h`.** Replace lines 144-149 (the stub and
  its comment) with, in order: a cite
  `// From Thetis TuneStep.cs:44-68 [v2.10.3.15]: ...` above `struct TuneStep`; the
  field comment `// A list of available tuning steps` marked
  `[original inline comment from console.cs:11270]`; a cite
  `// From Thetis console.cs:1953-1982 [v2.10.3.15]: ...` that states the list has 26
  entries; `kTuneStepList` with one entry per line, each followed by its verbatim
  index comment (`{1, "1Hz"},//0` through `{10000000, "10MHz"}//25`) and the closing
  line carrying `// initialize wheel tuning list array`; then `kTuneStepListSize` and
  `tuneStepIndexForHz`, whose comment says it mirrors the `-1`-on-miss contract of
  Thetis `TuneStepLookup` (`console.cs:11303-11312 [v2.10.3.15]`) but keys on Hz
  because NereusSDR persists the step in Hz. The words "Stage 1", "stub" and
  "11 entries" no longer appear there.
- [ ] **Step 3: The wrap methods.** Declare `changeTuneStepUp()` /
  `changeTuneStepDown()` under the "Tuning step" section of `SliceModel.h` and
  implement them in `SliceModel.cpp` after `setStepHz`, each with its own cite
  (`console.cs:6124-6128` and `console.cs:6130-6134`, both `[v2.10.3.15]`) and a body
  whose first line is the verbatim `//MW0LGE_21j`. On-list values use the Thetis
  formula from "Settled decisions" 3; off-list values follow decision 4, with a short
  comment saying Thetis holds an index and so has no off-list case. Both apply the
  result through `setStepHz`.
- [ ] **Step 4: The default's comment.** Keep `int m_stepHz{100};`. Put an own-line
  cite above it:
  `// From Thetis console.cs:1984 [v2.10.3.15]: Thetis starts at tune_step_index = 2 (10 Hz).`
  and state on the member that 100 Hz is `kTuneStepList[5]` and is kept as the
  NereusSDR default pending maintainer review. The old `tune_step_list[5]` claim goes.
- [ ] **Step 5: Consumers.** `MainWindow.cpp:1406-1418`: the lambda calls
  `slice->changeTuneStepUp()`; above the `connect`, a cite
  `// From Thetis console.cs:29034-29038 [v2.10.3.15]: ...` (a left click on the step
  display calls ChangeTuneStepUp, which wraps). Replace the stale propagation comment
  with an accurate one: `stepHzChanged` updates this flag's label through the
  connection made earlier in `createSliceFlag`; do not claim the spectrum follows.
  `RxApplet.cpp:438-489`: the arrow lambdas call `changeTuneStepDown()` /
  `changeTuneStepUp()`; replace the two comments that list the stub's six values
  with a cite `// From Thetis console.cs:30635-30643 [v2.10.3.15]: ...`
  (btnChangeTuneStepSmaller / btnChangeTuneStepLarger), keeping the `Issue #69`
  reference.
- [ ] **Step 6: Attribution.** `SliceModel.h`: add
  `Project Files/Source/Console/TuneStep.cs, original licence from Thetis source is included below`
  to the `Ported from Thetis sources:` list; stack the verbatim `TuneStep.cs` header
  (extracted, lines 1-40 of upstream, BOM stripped), preceded by the CLAUDE.md
  multi-source separator line `// --- From TuneStep.cs ---`, after the existing
  setup.designer.cs note and before the first `#include`; append the mod-history
  line. `SliceModel.cpp`: append a mod-history line (console.cs is already a listed
  source). `THETIS-PROVENANCE.md`: the SliceModel.h row adds
  `Project Files/Source/Console/TuneStep.cs` to its sources and `TuneStep` to its
  notes; the SliceModel.cpp row appends `; 6124-6134` to its line ranges and
  `ChangeTuneStepUp/Down wrap` to its notes; add after the
  `tests/tst_slice_auto_agc.cpp` row:
  `| tests/tst_slice_tune_step_list.cpp | Project Files/Source/Console/console.cs | 1953-1984; 6124-6134 | port | thetis-samphire | verbatim tune_step_list parity (26 StepHz/Name pairs), ChangeTuneStepUp/Down wrap, 100 Hz default guard [v2.10.3.15] |`
- [ ] **Step 7: Verify and commit.** Run the Verification commands; all green. One
  GPG-signed commit, subject `feat(slice): port Thetis tune_step_list for the STEP controls`,
  body in plain sentences (what was ported, that the default and label format are
  unchanged and per-mode memory is not ported), no trailer, no em-dash.

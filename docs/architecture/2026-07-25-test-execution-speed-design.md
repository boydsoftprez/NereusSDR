# Test Execution Speed: Design

**Status:** Approved (design), pending implementation plan
**Date:** 2026-07-25
**Baseline:** `main` at `a513bf72`. Measurements taken against a `build/` from
`feature/rfkit-rf2ks-applet` (`1d3650cc`), which carries `6ed89682`. See §3.
**Scope:** NereusSDR build + test infrastructure. No production code behavior changes.

---

## 1. Problem

Verifying a change against the test suite costs roughly **37 minutes** of
wall-clock on an Apple Silicon dev machine, of which **32 minutes is
linking**. That is slow enough to discourage TDD.

On `main` the cost is worse than that, because `main` does not yet carry
`6ed89682` (§3): every `cmake --build build` relinks all 519 test
executables whether or not you asked for tests. On the feature branches that
do carry it, the single-named-test loop is already about 5 seconds, and the
remaining pain narrows to: **"I changed something in `src/core` and now I
want to know the suite is still green."** That still relinks 519
full-application executables on every branch.

A policy intended to contain this ("run only the new test during TDD, full
suite once at end of epic") was recorded as project feedback on 2026-05-02
and has not held. That is the lesson driving this design: **the fast path
must be mechanical, not procedural.** A rule asking a human or an agent to
remember the narrow command will erode. A build graph that only rebuilds what
changed will not.

---

## 2. Measurements

Measured on the working machine (Apple Silicon, macOS 26.5.2, Qt 6.11.0,
`RelWithDebInfo`, ninja, LTO off) against `build/` at `1d3650cc`.

### 2.1 Method

- Per-test runtimes: `build/Testing/Temporary/CTestCostData.txt` (ctest's own
  rolling cost history, 519 entries).
- Per-target build times: `build/.ninja_log`.
- Cold-vs-warm link and run: 20 test binaries deleted, relinked, run twice.
- Subsystem classification: header-include scan across `tests/tst_*.cpp`.

### 2.2 Results

| Metric | Value |
| --- | --- |
| Test source files / registered ctest targets | 526 / **519** |
| Test build output on disk | **18.3 GB** (avg 35.3 MB each) |
| `__TEXT` in a representative large test binary | 25 MB |
| Relink, 20 binaries | **73.5s** wall, 762 CPU-s (**38 CPU-s each**) |
| Run 20 freshly-linked, `ctest -j10` | **11.9s** wall |
| Run same 20 warm | **0.86s** wall |
| Compile CPU-time, app objects | 313s (495 objects) |
| Compile CPU-time, test objects | **1,101s** (1,567 objects) |
| Test AUTOMOC CPU-time | 123s (1,557 steps) |
| ctest `LABELS` / fixtures / `TIMEOUT` defined | **0** |
| ccache installed locally | **No** (CI has it) |

Scaled to the full suite:

| Phase | 519 tests |
| --- | --- |
| Relink | **~32 min** wall, 5.5 CPU-hours |
| Run cold | **~5 min** wall |
| Run warm | ~22s |

The 526-vs-519 gap means seven `tst_*.cpp` files exist but are not registered
with `nereus_add_test()`. Worth auditing separately; they are either dead or
accidentally unregistered.

### 2.3 The two costs, separated

**Link (~32 of the 37 minutes).** Every test executable statically links all
327 objects of `NereusSDRObjs`. Any test touching `RadioModel` pulls in the
entire application: 25 MB of `__TEXT`, relinked and written to disk, once per
test target.

**First-run scan (~5 of the 37 minutes).** macOS malware-scans every
freshly-linked Mach-O on first execution. Caught directly: `XprotectService`
at 78.8% CPU and `syspolicyd` at 8.6% during a cold batch. Confirmed by
ctest's own history:

| Rebuilt between runs? | Count | Avg cost |
| --- | --- | --- |
| Yes (paid the scan) | 499 | **7.90s** |
| No (already validated) | 19 | **0.02-0.12s** |

`tst_smoke` asserts `1 + 1 == 2`. Qt reports the test body at 1ms. Cold wall
clock is 6.2s at 1% CPU; warm is 0.10s.

---

## 3. Written but not on main (merge before re-doing)

`6ed89682` (2026-05-26, KG4VCF) already solved the largest single item in
this design. It is **not on `main`**. It exists on three unmerged branches:
`feature/rfkit-rf2ks-applet`, `feature/phase3f-sub-epic-a-foundation`, and
`claude/adoring-elgamal-24e14e`.

| Change | Effect | On `main`? |
| --- | --- | --- |
| `EXCLUDE_FROM_ALL` on all test executables | `cmake --build build` builds only the app. A source edit no longer relinks 519 test binaries by default. | **No** |
| `all_tests` aggregate target | Explicit opt-in to the full test build; CI invokes it before `ctest`. | **No** |
| LTO default flipped ON -> OFF | Removed a multi-minute LTO link per test binary. Clean builds had reached over an hour with load average 250+. | **No** |

Verified directly:

```
main:                        add_executable(${name} ${name}.cpp ...)
feature/rfkit-rf2ks-applet:  add_executable(${name} EXCLUDE_FROM_ALL ${name}.cpp ...)
main: "all_tests" occurrences in tests/CMakeLists.txt = 0
```

**Phase 0 therefore begins with landing this on `main`**, either by merging
the owning branch or by cherry-picking `6ed89682`. Until that happens, every
branch cut from `main` inherits the 32-minute relink on routine builds, and
the rest of this design is measured against a baseline most work does not
have. This is the cheapest item on the list and it is already written.

---

## 4. Root cause

The build system cannot tell that a `SpectrumWidget` edit is irrelevant to
`tst_p1_codec_standard`, because as far as ninja is concerned it is not:
every source file is a transitive input to every test binary.

Two structural facts produce this:

1. **`NereusSDRObjs` is a single all-or-nothing OBJECT library** spanning
   `src/core`, `src/models`, and `src/gui`.
2. **One test file equals one full-application executable**, 519 times.

`EXCLUDE_FROM_ALL` (§3) hid the symptom for routine app builds. It did not
change either fact, so the cost reappears in full the moment you build
`all_tests`.

### 4.1 Layering feasibility

Splitting the object library requires the dependency graph to be close to
layered. It is:

- `src/core` reaches up into `models/` **26 times**, but **14** are just
  `models/Band.h`, a pure enum header that belongs in a lower shared layer.
- `src/models` reaches into `gui/` exactly **once** (the `SpectrumWidget`
  view hook described in CLAUDE.md).

So roughly a dozen upward includes stand between the current state and a
properly layered build graph.

### 4.2 Selectivity ceiling

Header-include scan across the test suite, by subsystem touched:

| Touches | Tests | Share |
| --- | --- | --- |
| core only | 244 | 47% |
| core + models | 93 | 18% |
| gui + core + models | 65 | 13% |
| gui only | 49 | 9% |
| gui + core | 31 | 6% |
| models only | 28 | 5% |
| gui + models | 11 | 2% |

(Scan covered 526 files; 5 matched no subsystem header and are omitted.)

This sets an honest ceiling on what selection can achieve:

- A **gui** edit affects ~30% of tests. 70% saved.
- A **models** edit affects ~38%. 62% saved.
- A **core** edit affects ~83%. Only 17% saved.

**Selection alone cannot fix core edits.** That is why making each individual
link cheap (Phase 1) matters more than selection does.

---

## 5. Design

### Phase 0: Cheap, no test semantics touched

**0a. Land `6ed89682` on `main`.** Merge the owning branch or cherry-pick.
Already written and exercised on three branches (§3). Removes the 519-binary
relink from routine `cmake --build build` for every branch cut from `main`.
This is first because everything else is measured against a baseline that
`main` does not currently have.

**0b. macOS Developer Tools exemption.** Add the terminal to System Settings
-> Privacy & Security -> Developer Tools, exempting build-spawned processes
from Gatekeeper assessment. Machine-local, operator action, no code. Measure
after applying rather than assuming.

**0c. Install and configure ccache locally.** CI uses it; dev machines never
have. Requires `sloppiness=pch_defines,time_macros`, already documented at
`CMakeLists.txt:1187`, because of the shared PCH.

**0d. ctest `LABELS`.** Tag each test with its subsystem so `ctest -L core`
works. Zero labels exist today, so there is no way to request a subset at all.

**0e. `TestSandboxInit.cpp` as a single OBJECT library.** Compiled 519 times
today at 0.10s each. Small win (50 CPU-s), but it is pure waste and the fix
is three lines.

**0f. De-sleep `tst_reconnect_on_silence`.** Contains `QTest::qWait(35000)`
and `QTest::qWait(7000)`: 42 seconds of literal sleeping, and the test costs
53.5s. While it exists, no amount of parallelism gets the suite below ~53
seconds. Make the silence/reconnect interval injectable and test at 50ms.
Suite-wide there are 62 sleep calls totalling 52.7s.

**0g. `TIMEOUT` properties.** None are set, so a hung test blocks
indefinitely rather than failing. Correctness, not speed.

**0h. Audit the 7 unregistered `tst_*.cpp` files.** Either register or delete.

### Phase 1: Structural (the load-bearing change)

Split `NereusSDRObjs` into subsystem libraries, built **shared**:

1. Move `Band.h` and similar pure-data headers into a `nereus_common` leaf.
   Resolves 14 of the 26 upward includes on its own.
2. Break the remaining ~12 upward includes (`core` -> `models`, the single
   `models` -> `gui` hook).
3. Split into `nereus_common`, `nereus_core`, `nereus_models`, `nereus_gui`,
   and likely `nereus_protocol`.
4. Extend `nereus_add_test(name LIBS ...)` so each test declares what it
   links.

Building these **shared** rather than as object libraries is what collapses
the per-test link: each test binary drops from ~35 MB to a few hundred KB,
with the application image built once.

Note the interaction with `6ed89682`: that commit documented a "dual
NereusSDRObjs" refactor as the clean way to reconcile LTO with fast test
builds. Phase 1 subsumes it. Once subsystems are separate shared libraries,
the LTO-for-release / no-LTO-for-tests split becomes a per-library property
rather than a duplicated object library.

### Phase 2: Skip-unchanged as the default

A hash manifest of test binaries plus a ctest wrapper that runs:

- tests whose binary changed since the last green run, **and**
- any test that failed on the last run (unchanged-but-red is never skipped,
  per the standing project rule that failures are never carried forward).

No manifest present means run everything. Wired as the default CMake preset,
so the fast path is what you get by typing `ctest`.

This phase is inert before Phase 1: today, touching any source file relinks
all 519 binaries and changes all 519 hashes, leaving nothing to skip.

### Phase 3: Selective consolidation

Merge tests that touch no process-global state into per-subsystem group
binaries. Tests that touch `AppSettings` or other singletons stay standalone.

The constraint is specific: `AppSettings` is a process-global singleton, and
`tests/TestSandboxInit.cpp` exists because a ctest run once overwrote a live
developer settings file (v0.1.1 alpha). Blanket consolidation into shared
processes would reintroduce exactly that class of cross-test pollution.

### Phase 4: Guardrails

Presets encode the policy. CI keeps running the full suite on its existing
4-way MD5 shard. `TIMEOUT` and `LABELS` from Phase 0 become load-bearing.

---

## 6. Expected outcome

### Scenario 1: single named test

| Step | On `main` today | With `6ed89682` (0a) | After Phase 1 |
| --- | --- | --- | --- |
| Recompile changed object | 0.6s | 0.6s | 0.6s |
| Link test binaries | 519 = **32 min** | 1 = **3.7s** | 1 = ~0.3s |
| Run | 519 cold = **5 min** | **0.6s** | 0.04s |
| **Total** | **~37 min** | **~5s** | **~1s** |

The large win in this row is Phase 0a, which is already written and only
needs merging.

### Scenario 2: verify the suite after a core edit (the real problem)

| Step | Today | After | Saving |
| --- | --- | --- | --- |
| Link | **32 min** | ~1-2 min | ~30 min |
| Run cold | **5 min** | ~30-60s | ~4 min |
| Slowest-test floor | 53s | ~0.5s | 53s |
| **Total** | **~37 min** | **~2-3 min** | **~34 min** |
| Disk | **18.3 GB** | ~1 GB | 17 GB |

### Per-change breakdown

| # | Change | Before | After | Saving | Basis |
| --- | --- | --- | --- | --- | --- |
| 1 | Shared subsystem libs (1) | **38 CPU-s** per link | ~2-4 CPU-s | **~90%** of link | projected |
| 2 | Subsystem split (1) | 100% relink | gui 30% / models 38% / core 83% | **17-70%** | measured |
| 3 | Dev Tools exemption (0b) | **0.59s**/test cold | **0.043s**/test | **14x** on run phase | measured |
| 4 | Skip-unchanged (2) | runs 519 | changed + last-failed | most of run phase | projected |
| 5 | Consolidation (3) | 519 procs, 123s AUTOMOC | ~30 procs, ~7s | ~2 min | projected |
| 6 | ccache (0c) | full compile **1,537 CPU-s** | ~10-20% warm | ~20 CPU-min | CI-proven |
| 7 | De-sleep reconnect (0f) | **53.5s** floor | ~0.5s | 53s off floor | measured |
| 8 | `TestSandboxInit` OBJECT (0e) | 519 x 0.10s = **50s** | 0.10s | 50 CPU-s | measured |

**Items 1 and 2 are the design.** Everything else is worth doing and cheap,
but the 32-minute link is the problem, and only Phase 1 addresses it.

---

## 7. Rejected

**`-O0` for test compiles.** Tests do not need optimization, and one test
file compiles in 1.69s at `-O2`. Attempting `-O0` fails:

```
error: __OPTIMIZE__ predefined macro was enabled in precompiled file
'.../cmake_pch.hxx.pch' but is currently disabled
```

The PCH is shared via `REUSE_FROM NereusSDRObjs` and built at `-O2`, so tests
are locked to it. Getting `-O0` means maintaining a second PCH for the test
tree. This is the only item that trades against something else, and the
payoff is small. Skip it.

**Blanket consolidation of all 519 tests.** Rejected in favor of Phase 3's
selective form, on the `AppSettings` singleton grounds in §5.

**Re-doing `EXCLUDE_FROM_ALL`.** Already shipped in `6ed89682`. See §3.

---

## 8. Risks and caveats

| Risk | Mitigation |
| --- | --- |
| Phase 1 dependency untangling touches ~12 upward includes across layers | Land as its own PR, no behavior change, full suite green before and after |
| Shared libraries lose dead-stripping, raising per-process resident memory | Measure after Phase 1; tests are short-lived, app unaffected (single process) |
| Shared libraries need rpath and symbol-visibility work on macOS/Windows | Standard Qt practice; verify all three CI platforms before merge |
| Phase 1 interacts with the LTO option added in `6ed89682` | Per-library IPO property; verify `-DNEREUSSDR_ENABLE_LTO=ON` release path still builds |
| Phase 2 could skip a genuinely affected test if the graph is wrong | Failed-last-run tests always re-run; CI always runs the full suite |
| Phase 3 cross-test pollution via `AppSettings` | Only singleton-free tests are merged; `TestSandboxInit` retained |

**Projection confidence.** Item 1's ~90% is inferred from binary composition
(25 MB of `__TEXT` per test collapsing to one shared image), not measured
end-to-end. The 519-test figures are linear scale-ups from a measured 20;
XProtect may degrade worse at full scale, which would make today's numbers
worse rather than the projections better.

---

## 9. Non-goals

- No production code behavior changes. This is build and test infrastructure.
- No change to what is tested or to any assertion.
- No migration away from CMake / ctest / QtTest.
- No change to the CI 4-way sharding scheme.
- No reduction in coverage. Phase 3 changes how tests are packaged into
  processes, not how many exist.

---

## 10. Verification

Each phase is verified by re-running the §2 measurements and comparing:

1. `ninja` a single test after a core-file edit: wall time, target count.
2. Delete N test binaries, relink, measure wall + CPU (the §2.1 method).
3. Cold and warm `ctest -j10` on the same set.
4. `du -sh build/tests`.
5. Full suite green on all three CI platforms.

Acceptance for Phase 0: suite-run floor drops below 5s; ccache hit rate
above 80% on a rebuild after branch switch.
Acceptance for Phases 1-3: Scenario 2 drops under 5 minutes.

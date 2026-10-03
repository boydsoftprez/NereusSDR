# 3D Stacked-Trace Spectrum (3DSS) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a per-panadapter 3D stacked-trace spectrum mode to NereusSDR, matching AetherSDR's 3DSS view feature-for-feature, plus a NereusSDR-original 3D Angle slider that upstream does not have.

**Architecture:** Reference-guided reimplementation against AetherSDR `upstream/main` at `1872028c`. A standalone `DssRenderer` owns a ring of downsampled dBm rows and a CPU fallback surface; a GPU height-map mesh reads that ring as an `RGBA16F` texture and builds the receding trapezoid entirely in the vertex shader. In 3D mode the mesh pass takes the spectrum-trace pass's slot inside `renderGpuFrame()`; the waterfall, frequency scale and all overlays run unchanged.

**Tech Stack:** C++20, Qt6 (QRhiWidget / QRhi / QTest), GLSL 440 compiled through `qt_add_shaders`, AppSettings XML persistence.

**Design doc:** `docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md`

## Global Constraints

Every task's requirements implicitly include this section.

- **Upstream cite stamp is `[@1872028c]`** on every `// From AetherSDR ...` comment.
- **Read upstream by the pinned SHA, never by a branch name.** Always `git -C /Users/j.j.boyd/AetherSDR show 1872028c:<path>`. Two refs will mislead you:
  - The **working tree** at `/Users/j.j.boyd/AetherSDR` is behind and must never be read directly.
  - **`upstream/main` is a moving target.** It has already advanced past the pin during this epic (to `a26d6290`), and `src/gui/SpectrumWidget.cpp` grew by 77 lines in the process. Every line number this plan cites for that file is relative to `1872028c`; reading the branch instead silently shifts them and you will port the wrong lines. The DSS-specific files happened not to change, which is exactly why this is dangerous: a spot check on `DssRenderer.h` would show no difference and give false confidence.
- **Tier 1 code is lifted verbatim.** Exactly two deviations are permitted (design §2.3): angle parameterisation, and scroll distance fixed at one row. Each deviation site carries `//-KG4VCF [v0.5.3] <description>`. Any further tier 1 edit must be added to design §2.3 in the same commit.
- **UPSTREAM IS AUTHORITATIVE FOR COMMENT TEXT; THIS PLAN IS NOT.** The code blocks in this plan give you the *structure* to build: signatures, ordering, which values go where. They were transcribed by hand and are known to have dropped and reworded upstream comments in at least three places. For every comment inside a tier 1 lift, open the upstream file with `git -C /Users/j.j.boyd/AetherSDR show 1872028c:<path>` and copy the comment text from there, not from this plan. Where the plan and upstream disagree on comment wording, **upstream wins silently** and needs no escalation. Escalate only when they disagree on *code*.
  - Adapting an identifier that genuinely changed in the port (for example upstream's `kCols` to our `kDssCols`, or `kMaxRowSpanFactor` to `dssMaxRowSpanFactor()`) is a mechanical rename inside otherwise-verbatim text, not a deviation, and needs no marker.
  - Dropping or rewriting an upstream *sentence* is a deviation and is not permitted outside the two listed above.
  - Where the port modifies logic inside a commented region, keep upstream's original comment and add the `//-KG4VCF` marker after it. Do not replace upstream's explanation with your own.
- **Preserve upstream inline issue-number annotations verbatim** on lifted lines: `(#4539)`, `(#3937)`, `(#2724)`, `(#1921)`, `(#3482)`, and any others encountered. No script enforces this; it is a human review item on every PR (design §9.1).
- **New file headers:** copy the attribution block from `src/gui/SpectrumOverlayMenu.h:1-24` verbatim, changing only the file path on line 3 and the Modification-history date and description. Do not retype it from memory.
- **No `QSettings`.** Use `AppSettings::instance()`. Booleans persist as the strings `"True"` / `"False"`.
- **C++ style:** braces on all control flow, no raw `new`/`delete` outside Qt parent ownership, `constexpr` not `#define`, members `m_camelCase`, constants `kPascalCase`.
- **Line numbers this plan cites for NereusSDR files are stale by design; find things by NAME.** Every task in this epic adds code to `SpectrumWidget.{h,cpp}`, so cites written when the plan was authored have shifted, and will keep shifting. Two measured examples: `SliceMarkerGeometry` moved from `:989` to `:1041`, and the `settingsKey` helper from `:578` to `:603`. Treat a NereusSDR line cite as a hint about roughly where to look and grep for the symbol. Upstream AetherSDR cites are the opposite: those are pinned at `1872028c` and are exact, so a mismatch there is a real discrepancy worth reporting.
- **Existing NereusSDR names this plan depends on, all verified 2026-08-08.** Use these exactly; do not invent neighbours.
  - Noise floor: `m_nfLerpAverage` (smoothed, use this) and `m_nfFftBinAverage` (per-frame, do not use for the surface anchor).
  - dBm range: `m_refLevel` (top, dBm) plus `m_dynamicRange` (depth, dB). There is no floor/ceiling pair.
  - View: `m_centerHz`, `m_bandwidthHz`, `m_ddcCenterHz`, `m_sampleRateHz`.
  - Waterfall colour: `m_wfColorScheme`, `m_wfColorGain`, `m_wfBlackLevel`, `m_wfActiveLowThreshold`, `m_wfActiveHighThreshold`. There is no `m_wfMinDbm`.
  - `enum class WfColorScheme : int { Default, Enhanced, Spectran, BlackWhite, LinLog, LinRad, Custom, ClarityBlue, Count }`. EIGHT schemes, declared at `SpectrumWidget.h:206-217`. Do NOT use AetherSDR's six-value enum (`Default/Grayscale/BlueGreen/Fire/Plasma/Purple`); an earlier revision of this plan carried it by mistake and a task lost time to it. `setWfColorScheme()` takes the enum by value, not an `int`.
  - Logging: `qCWarning(lcSpectrum)`, declared in `src/core/LogCategories.h`. Include it.
  - Shader loading: `loadShader(const QString&)`, a static free function at `SpectrumWidget.cpp:7619`, callable from members in that TU.
  - Slices: `sliceMarkerGeometry()` returning `QVector<SliceMarkerGeometry>` with fields `centreHz`, `filterLowHz`, `filterHighHz`, `flag`. Driven in tests by `addVfoWidget(index)` then `setFrequency` / `setFilter`.
- **Test seams follow the file's existing convention.** `SpectrumWidget.h` already carries 33 `...ForTest()` accessors, including `drawSpotMarkersForTest(QPainter&, const QRect&)` with the same shape this plan uses. Adding more is consistent with the file, not a new pattern.
- **Commits are GPG-signed** (`commit.gpgsign=true` is already set; never pass `--no-gpg-sign`). No `Co-Authored-By: Claude` trailer. No em-dash characters in commit messages or in prose you write.
- **Do not "clean up" em-dashes inside lifted upstream comments.** Several verbatim comments in `DssGeometry.h`, `DssRenderer.cpp` and both shaders contain them. Verbatim means verbatim; the house style applies to text you author, not to text you are preserving for attribution.
- **Build:** `cmake --build build -j$(sysctl -n hw.ncpu)`. Test executables are `EXCLUDE_FROM_ALL`; always build the named target before running ctest, or you will run a stale binary and get a false green.
- **Register every new test** with `nereus_add_test(tst_<name>)` in `tests/CMakeLists.txt`, keeping the list alphabetically sorted.
- **The 1e-6 tolerance rule applies to TEST assertions you write, never to constants lifted from upstream.** `frequencyFramesMatch` carries a `1.0e-9` epsilon in upstream source; that is a lifted constant and preserving it exactly is required. Changing it to 1e-6 would be an unauthorised deviation. The rule exists because float32 geometry comparisons in *our tests* cannot achieve 1e-9, not because 1e-9 is wrong wherever it appears.
- **Widget tests that push rows must show the widget first.** `pushWaterfallRow()` opens with a `m_waterfall.isNull()` guard, and `m_waterfall` is only allocated in `resizeEvent()`, which Qt does not dispatch synchronously from `resize()` on an unshown top-level widget. A test that only calls `resize()` silently pushes nothing, so any assertion that a counter stayed at zero passes for the wrong reason. Use `resize()`, then `show()`, then `QVERIFY(QTest::qWaitForWindowExposed(&w))`. This already made one test in Task 6 vacuous before it was caught.
- **Assert an absence only after proving the mechanism can produce a presence.** A test whose whole content is "this counter is still zero" is indistinguishable from a test where nothing ran at all. Either increment it first in the same test, or rely on a sibling test that demonstrably does.
- **A test that pins two sides of a boundary must READ one side, not restate it.** Wherever a value is duplicated across a language or process boundary (C++ against GLSL, host against wire format, code against a config file), the test has to parse the far side from its actual source. Restating the near side's arithmetic in the test produces two spellings of one expression that constant-fold together and can never disagree. Task 5 shipped exactly that: a UBO float count compared against a hand-copied version of its own formula, which would have let a GLSL edit ship a silent layout mismatch.
- **A test for a defensive fix must be shown to FAIL without the fix.** Reasoning that it would is not enough, and has already been wrong once here: a reviewer hand-traced that `clear_resetsEverything` covered a restored wipe, but disabling the wipe left it passing 13/13, because `clear()` resets `m_head` to 0 and the assertion read a never-written, already-zero slot. When a task adds guard or reset behaviour, mutate it out, run the test, and confirm it goes red before you claim coverage. State that you did so in your report.
- **Cover interior branches, not just boundaries.** Where a function has early-return guards around a computation, assert at least two points inside the computed range as well as the guards. Task 1's review caught exactly this: a two-assertion test hit both of `dssWedgeFreeDepth`'s guard clauses and never once reached its interpolation, so an inverted numerator would have passed. If the test code given in a task only checks boundaries on a function that computes something in between, add the interior assertions rather than transcribing the gap.
- **ATTRIBUTION LANDS IN THE SAME COMMIT AS THE FILE, NEVER DEFERRED.** The pre-commit hook runs `check-new-ports.py` in **full-tree** mode, so any file on disk carrying AetherSDR tells and lacking a PROVENANCE row blocks *every* commit in the repository, including commits that have nothing to do with it. An unregistered file does not merely fail its own task; it wedges the whole branch. CLAUDE.md requires the same thing independently: the verbatim header and the PROVENANCE row go in the commit that introduces the ported logic.
  - Any task creating a file with an AetherSDR header or a `// From AetherSDR` cite MUST add its row to `docs/attribution/aethersdr-reconciliation.md` under "Bucket A" in that same commit.
  - Row format is four columns: `| <NereusSDR file> | <AetherSDR counterpart> | <evidence: which lines cite what> | "<one-sentence mod-history wording>" |`
  - Put the row in this plan's own `## 3D Stacked-Trace Spectrum Plan` section at the end of that document, not inside the `## Bucket A (48 files)` heading, whose count would then be wrong. Task 1 created that section; later tasks append to it. This follows the two most recent precedents in the same file, `## Phase 3P-II PGXL/TGXL Accessories` and `## Phase 3F Sub-Epic D`.
  - Verify before committing with `python3 scripts/check-new-ports.py --full-tree`. The `--full-tree` flag is required: without it the script runs in diff mode against staged files only and prints a vacuous `OK [diff]` that tells you nothing about the file you just wrote. Expected output is `OK [full-tree]: <N> C/C++ file(s) checked`. If it flags your file, you are not done.

---

## File Structure

| File | Tier | Responsibility |
|---|---|---|
| `src/gui/DssGeometry.h` | 1 | Perspective constants and pure projection functions. Header-only, no Qt widget deps. The verbatim-lift surface. |
| `src/gui/DssRenderer.h` / `.cpp` | 1+2 | Ring store of downsampled dBm rows, two channels, smoothing, CPU fallback surface. Knows nothing of QRhi or SpectrumWidget. |
| `src/gui/DssMeshGeometry.h` | 3 | Mesh vertex generation and dynamic column sizing. Header-only so it is testable without a graphics context. |
| `resources/shaders/dss_mesh.vert` / `.frag` | 1 | GPU height-map mesh and fragment shading. |
| `src/gui/SpectrumWidget.h` / `.cpp` | 3 | Mode switch, GPU resources, row tee, UBO writer, 3D dBm scale, slice shadows, control wiring, persistence. |
| `src/gui/SpectrumOverlayMenu.h` / `.cpp` | 3 | `3D VIEW` control section. |
| `src/gui/setup/DisplaySetupPages.h` / `.cpp` | 3 | Setup mirror of the six controls. |

Splitting `DssGeometry.h` out of `DssRenderer.h` (upstream keeps them together) is deliberate: the golden test in Task 1 must link against the projection math without dragging in the ring store, and the angle parameterisation touches only this file.

---

## Task 1: DssGeometry, verbatim projection math with a runtime angle

**Files:**
- Create: `src/gui/DssGeometry.h`
- Test: `tests/tst_dss_geometry.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `struct NereusSDR::DssShape { float backWidthFrac; float depthSpanFrac; float frontMaxRidgeFrac; }`
  - `constexpr DssShape kDssUpstreamShape{0.60f, 0.58f, 0.46f}`
  - `constexpr float kDssHaze = 0.16f`, `kDssColorSpanDb = 45.0f`
  - `constexpr int kDssVisibleRows = 96`, `kDssTransitionRows = 8`, `kDssRows = 104`, `kDssCols = 768`
  - `DssShape dssShapeForAngle(int anglePercent)`
  - `float dssDepthScale(float depth, const DssShape&)`
  - `QPointF dssProjectPerspective(float frequencyUnit, float depth, const DssShape&)`
  - `QPointF dssProjectSurface(float frequencyUnit, float depth, float dbm, float floorDbm, float rangeDb, float zCurve, const DssShape&)`
  - `float dssMaxRowSpanFactor(const DssShape&)`
  - `float dssRowSpanFactorForOverhang(float spanFactor, const DssShape&)`
  - `float dssRowSpanFactorFor(double supplementalBandwidthMhz, double targetBandwidthMhz, int spanPercent, const DssShape&)`
  - `float dssWedgeFreeDepth(float rowSpanFactor, const DssShape&)`
  - `float dssRowScreenCoverage(float depth, float rowSpanFactor, const DssShape&)`
  - `float dssRowFrequencyUnit(float meshUnit, float rowSpanFactor)`

- [ ] **Step 1: Read the upstream source you are about to lift**

```bash
git -C /Users/j.j.boyd/AetherSDR show 1872028c:src/gui/DssRenderer.h | sed -n '30,190p'
```

Read all of it before writing anything. Lines 45-51 are the constants, 85-187 the projection functions. Note every comment: they are lifted verbatim along with the code.

- [ ] **Step 2: Write the failing golden test**

A note on tolerances before you write it. The projection functions are
`float`-typed to match upstream, so every comparison below is float32
arithmetic widened into a `double` result. That carries roughly 1e-8 of
rounding: `dssProjectPerspective(0.0f, 1.0f, shape).x()` lands on
0.19999998807907104 rather than 0.20, and the `.y()` at depth 1 lands on
0.42000001668930054 rather than 0.42. Both are pure float representation
error, not formula drift. The tolerance is therefore `1e-6` throughout,
which leaves about sixty times headroom over that noise while still
catching any real geometry change, since a wrong constant or a wrong
operation moves these values by 1e-3 or more. AetherSDR's own
`tests/dss_renderer_test.cpp` uses a looser 1e-4 on the same formulas.

Create `tests/tst_dss_geometry.cpp`. Header block per Global Constraints, then:

```cpp
#include <QTest>
#include <cmath>

#include "gui/DssGeometry.h"

using namespace NereusSDR;

class TestDssGeometry : public QObject {
    Q_OBJECT

private slots:
    // The crux test. If these drift, the surface stops looking like
    // AetherSDR's and nothing else in the suite will notice.
    void upstreamConstants_areExact() {
        QCOMPARE(kDssUpstreamShape.backWidthFrac,     0.60f);
        QCOMPARE(kDssUpstreamShape.depthSpanFrac,     0.58f);
        QCOMPARE(kDssUpstreamShape.frontMaxRidgeFrac, 0.46f);
        QCOMPARE(kDssHaze,          0.16f);
        QCOMPARE(kDssColorSpanDb,  45.0f);
        QCOMPARE(kDssCols,          768);
        QCOMPARE(kDssVisibleRows,    96);
        QCOMPARE(kDssRows,          104);
    }

    void depthScale_matchesUpstream() {
        const DssShape s = kDssUpstreamShape;
        QCOMPARE(dssDepthScale(0.0f, s), 1.0f);
        QCOMPARE(dssDepthScale(1.0f, s), 0.60f);
        // depth 0.5 -> 1 - 0.5 * (1 - 0.60) = 0.80
        QVERIFY(std::abs(dssDepthScale(0.5f, s) - 0.80f) < 1e-6f);
        // clamps outside [0,1]
        QCOMPARE(dssDepthScale(-1.0f, s), 1.0f);
        QCOMPARE(dssDepthScale(2.0f, s),  0.60f);
    }

    void projectPerspective_matchesUpstream() {
        const DssShape s = kDssUpstreamShape;
        // Centre frequency never moves horizontally at any depth.
        for (float d : {0.0f, 0.25f, 0.5f, 1.0f}) {
            const QPointF p = dssProjectPerspective(0.5f, d, s);
            QVERIFY(std::abs(p.x() - 0.5) < 1e-6);
        }
        // Front edge spans the full width; back edge narrows to 0.60.
        QVERIFY(std::abs(dssProjectPerspective(0.0f, 0.0f, s).x() - 0.0) < 1e-6);
        QVERIFY(std::abs(dssProjectPerspective(1.0f, 0.0f, s).x() - 1.0) < 1e-6);
        QVERIFY(std::abs(dssProjectPerspective(0.0f, 1.0f, s).x() - 0.20) < 1e-6);
        QVERIFY(std::abs(dssProjectPerspective(1.0f, 1.0f, s).x() - 0.80) < 1e-6);
        // Baseline rises with depth by depthSpanFrac.
        QVERIFY(std::abs(dssProjectPerspective(0.5f, 0.0f, s).y() - 1.0)  < 1e-6);
        QVERIFY(std::abs(dssProjectPerspective(0.5f, 1.0f, s).y() - 0.42) < 1e-6);
    }

    void projectSurface_matchesUpstream() {
        const DssShape s = kDssUpstreamShape;
        // At the floor the surface sits exactly on the baseline.
        const QPointF atFloor =
            dssProjectSurface(0.5f, 0.0f, -120.0f, -120.0f, 60.0f, 1.0f, s);
        QVERIFY(std::abs(atFloor.y() - 1.0) < 1e-6);
        // Full scale at the front rises by frontMaxRidgeFrac * width(=1).
        const QPointF atPeak =
            dssProjectSurface(0.5f, 0.0f, -60.0f, -120.0f, 60.0f, 1.0f, s);
        QVERIFY(std::abs(atPeak.y() - (1.0 - 0.46)) < 1e-6);
        // Far ridges are shorter by the depth width factor.
        const QPointF atBack =
            dssProjectSurface(0.5f, 1.0f, -60.0f, -120.0f, 60.0f, 1.0f, s);
        QVERIFY(std::abs(atBack.y() - (0.42 - 0.46 * 0.60)) < 1e-6);
        // A non-finite dBm is pinned to the floor, not propagated as NaN.
        const QPointF nan = dssProjectSurface(
            0.5f, 0.0f, std::nanf(""), -120.0f, 60.0f, 1.0f, s);
        QVERIFY(std::abs(nan.y() - 1.0) < 1e-6);
    }

    void rowSpanFactor_matchesUpstream() {
        const DssShape s = kDssUpstreamShape;
        QVERIFY(std::abs(dssMaxRowSpanFactor(s) - (1.0f / 0.60f)) < 1e-6f);
        // Not wider than the viewport -> classic clipped trapezoid.
        QCOMPARE(dssRowSpanFactorFor(1.0, 1.0, 100, s), 1.0f);
        QCOMPARE(dssRowSpanFactorFor(0.5, 1.0, 100, s), 1.0f);
        // Non-finite or non-positive inputs are refused, not extrapolated.
        QCOMPARE(dssRowSpanFactorFor(2.0, 0.0, 100, s), 1.0f);
        QCOMPARE(dssRowSpanFactorFor(
            std::numeric_limits<double>::quiet_NaN(), 1.0, 100, s), 1.0f);
        // The percentage scales the AVAILABLE span, not the absolute maximum.
        // 1.5x available at 100% -> 1.5; at 50% -> 1.25.
        QVERIFY(std::abs(dssRowSpanFactorFor(1.5, 1.0, 100, s) - 1.5f)  < 1e-6f);
        QVERIFY(std::abs(dssRowSpanFactorFor(1.5, 1.0,  50, s) - 1.25f) < 1e-6f);
        QCOMPARE(dssRowSpanFactorFor(1.5, 1.0, 0, s), 1.0f);
        // Clamped at the max: past it the extra data is off-plot anyway.
        QVERIFY(std::abs(dssRowSpanFactorFor(10.0, 1.0, 100, s)
                         - dssMaxRowSpanFactor(s)) < 1e-6f);
    }

    void wedgeFreeDepth_matchesUpstream() {
        const DssShape s = kDssUpstreamShape;
        QCOMPARE(dssWedgeFreeDepth(1.0f, s), 0.0f);
        QCOMPARE(dssWedgeFreeDepth(dssMaxRowSpanFactor(s), s), 1.0f);
        // Interior points. The two boundary cases above short-circuit before
        // reaching the interpolation, so without these an inverted numerator
        // would pass unnoticed. Tasks 3 and 9 both consume this formula.
        // (1 - 1/1.25) / (1 - 0.60) = 0.2 / 0.4 = 0.5
        QVERIFY(std::abs(dssWedgeFreeDepth(1.25f, s) - 0.5f)  < 1e-6f);
        // (1 - 1/1.40) / (1 - 0.60) = (2/7) / 0.4 = 5/7
        QVERIFY(std::abs(dssWedgeFreeDepth(1.40f, s) - (5.0f / 7.0f)) < 1e-6f);
    }

    void rowFrequencyUnit_isDepthIndependent() {
        // Widening a row extends the frequency range it covers, never where
        // a frequency lands on screen.
        QCOMPARE(dssRowFrequencyUnit(0.5f, 1.0f), 0.5f);
        QCOMPARE(dssRowFrequencyUnit(0.5f, 2.0f), 0.5f);
        QCOMPARE(dssRowFrequencyUnit(1.0f, 2.0f), 1.5f);
        QCOMPARE(dssRowFrequencyUnit(0.0f, 2.0f), -0.5f);
    }
};

QTEST_APPLESS_MAIN(TestDssGeometry)
#include "tst_dss_geometry.moc"
```

- [ ] **Step 3: Register the test**

In `tests/CMakeLists.txt`, add in alphabetical position:

```cmake
nereus_add_test(tst_dss_geometry)
```

- [ ] **Step 4: Run it to confirm it fails**

```bash
cmake --build build --target tst_dss_geometry
```

Expected: FAIL at compile with `fatal error: 'gui/DssGeometry.h' file not found`.

- [ ] **Step 5: Create DssGeometry.h**

Header block per Global Constraints. Then lift the constants and functions from upstream `DssRenderer.h:36-187`, preserving every comment verbatim, adapted only to take `const DssShape&` instead of reading file-scope constants. Full body:

```cpp
#pragma once

#include <QPointF>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// ─── Stacked-trace spectrum surface geometry ────────────────────────────────
//
// From AetherSDR src/gui/DssRenderer.h:36-187 [@1872028c].
//
// Perspective geometry of the surface, shared by this CPU renderer and the
// GPU mesh UBO (SpectrumWidget::renderGpuFrame). dss_mesh.vert applies the
// SAME formulas with these values passed as uniforms — single source of
// truth so the CPU fallback and the GPU mesh can't drift apart.
//
//-KG4VCF [v0.5.3] Upstream holds backWidthFrac / depthSpanFrac /
// frontMaxRidgeFrac as static constexpr. NereusSDR promotes them to a
// runtime DssShape so the 3D Angle slider can move them; upstream's values
// are preserved exactly as kDssUpstreamShape and are reproduced by
// dssShapeForAngle(50). Permitted tier 1 deviation 1 of 2, design doc §2.3.
struct DssShape {
    float backWidthFrac{0.60f};      // back row width / front
    float depthSpanFrac{0.58f};      // baseline rise to the back
    float frontMaxRidgeFrac{0.46f};  // front ridge height / plot H
};

inline constexpr DssShape kDssUpstreamShape{0.60f, 0.58f, 0.46f};

inline constexpr float kDssHaze = 0.16f;   // fade toward bg with depth
// Colour uses a stable signal aperture independent of the Ref-level height
// span. Otherwise a high Ref level compresses every real signal into blue.
inline constexpr float kDssColorSpanDb = 45.0f;

inline constexpr int kDssVisibleRows = 96;    // front → back display depth
// Outgoing rows kept during scroll. Must cover the largest row distance
// passed to SpectrumWidget::startWaterfallScrollAnimation(); the current
// Flex, Kiwi, and fallback producers append at most one row per update.
//-KG4VCF [v0.5.3] NereusSDR's producer appends exactly one row per
// waterfall tick, so only one is ever needed; the reserve is kept at
// upstream's 8 so every ring-index formula stays line-comparable with
// upstream. Permitted tier 1 deviation 2 of 2, design doc §2.3.
inline constexpr int kDssTransitionRows = 8;
inline constexpr int kDssRows = kDssVisibleRows + kDssTransitionRows;
inline constexpr int kDssCols = 768;  // resampled columns per row

// ── 3D Angle mapping (NereusSDR-original, design doc §5.3) ──────────────
// Both curves pass through upstream's constants at t = 0.5, so slider 50
// reproduces AetherSDR's fixed look exactly.
inline constexpr float kDssBackWidthAtZero = 0.35f;
inline constexpr float kDssBackWidthAtOne  = 0.85f;
inline constexpr float kDssDepthSpanAtZero = 0.36f;
inline constexpr float kDssDepthSpanAtOne  = 0.80f;
inline constexpr float kDssMaxRidgeFrac    = 0.75f;

// Fraction of the on-screen ridge ceiling upstream occupies. Defined from
// the upstream constants rather than a rounded literal so dssShapeForAngle(50)
// returns kDssUpstreamShape.frontMaxRidgeFrac exactly.
inline constexpr float kDssRidgeHeadroom =
    kDssUpstreamShape.frontMaxRidgeFrac * kDssUpstreamShape.backWidthFrac
    / (1.0f - kDssUpstreamShape.depthSpanFrac);

// Slider 0..100 -> perspective shape. Ridge height is derived rather than
// exposed, so a rising angle cannot push back-row peaks off the top of the
// plot: the back ridge top sits at (1 - depthSpanFrac) - ridge * backWidthFrac
// and must stay >= 0.
inline DssShape dssShapeForAngle(int anglePercent)
{
    const float t =
        std::clamp(static_cast<float>(anglePercent), 0.0f, 100.0f) / 100.0f;
    DssShape s;
    s.backWidthFrac =
        kDssBackWidthAtZero + t * (kDssBackWidthAtOne - kDssBackWidthAtZero);
    s.depthSpanFrac =
        kDssDepthSpanAtZero + t * (kDssDepthSpanAtOne - kDssDepthSpanAtZero);
    s.frontMaxRidgeFrac = std::min(
        kDssMaxRidgeFrac,
        kDssRidgeHeadroom * (1.0f - s.depthSpanFrac) / s.backWidthFrac);
    return s;
}

// Perspective narrowing with depth — the only thing that places a frequency.
inline float dssDepthScale(float depth, const DssShape& shape)
{
    return 1.0f
         - std::clamp(depth, 0.0f, 1.0f) * (1.0f - shape.backWidthFrac);
}

// Mesh column (0..1 across the drawn row) -> frequency in viewport units,
// where 0..1 spans the on-screen bandwidth. Depth-independent by design.
inline float dssRowFrequencyUnit(float meshUnit, float rowSpanFactor)
{
    return 0.5f + (meshUnit - 0.5f) * rowSpanFactor;
}

// Fraction of the plot width a row at `depth` covers. >= 1 means that row
// reaches both edges and leaves no wedge; the front row is the widest.
inline float dssRowScreenCoverage(float depth, float rowSpanFactor,
                                  const DssShape& shape)
{
    return dssDepthScale(depth, shape) * rowSpanFactor;
}

// ── Wedge-closing row span ──────────────────────────────────────────────
// Because every row covers the SAME frequency span while the far rows
// narrow to kBackWidthFrac, the surface leaves two empty triangles beside
// it. Widen the span each row covers instead: the near rows then run off
// both edges of the plot and the existing perspective narrowing walks them
// back in, closing the wedge from the front. The projection below is
// untouched, so the converging slant, the frequency ruler and every marker
// stay put. dss_mesh.vert applies the SAME formulas.

// Span at which the DEEPEST row lands exactly on the plot edge. Beyond this
// the surface only overhangs further without revealing more of the plot.
inline float dssMaxRowSpanFactor(const DssShape& shape)
{
    return 1.0f / shape.backWidthFrac;
}

// Shallowest depth still leaving a wedge, or 1 when the surface is closed
// all the way to the back. Lets the host report how much a given overhang
// actually bought.
inline float dssWedgeFreeDepth(float rowSpanFactor, const DssShape& shape)
{
    if (rowSpanFactor <= 1.0f) {
        return 0.0f;
    }
    if (rowSpanFactor >= dssMaxRowSpanFactor(shape)) {
        return 1.0f;
    }
    return (1.0f - 1.0f / rowSpanFactor) / (1.0f - shape.backWidthFrac);
}

// Usable span for an overhang of `spanFactor` x the viewport bandwidth.
// Clamped at kMaxRowSpanFactor: past it the extra data is off-plot anyway.
inline float dssRowSpanFactorForOverhang(float spanFactor,
                                         const DssShape& shape)
{
    if (!(spanFactor > 1.0f) || !std::isfinite(spanFactor)) {
        return 1.0f;
    }
    return std::min(spanFactor, dssMaxRowSpanFactor(shape));
}

// Row span for a source whose calibrated overhang spans
// supplementalBandwidthMhz against a targetBandwidthMhz viewport, scaled by
// a 0-100 operator setting. Anything that cannot be trusted -- a
// non-positive or non-finite bandwidth, or an overhang no wider than the
// viewport -- yields 1.0, the clipped trapezoid, rather than widening into
// spectrum that was never captured.
//
// The percentage scales the AVAILABLE span, not the absolute maximum:
// against a ~1.15x tile an absolute reading would clamp everything above
// ~22% to the same picture, leaving most of the control's travel dead.
inline float dssRowSpanFactorFor(double supplementalBandwidthMhz,
                                 double targetBandwidthMhz,
                                 int spanPercent,
                                 const DssShape& shape)
{
    if (!std::isfinite(targetBandwidthMhz) || targetBandwidthMhz <= 0.0
        || !std::isfinite(supplementalBandwidthMhz)
        || supplementalBandwidthMhz <= targetBandwidthMhz) {
        return 1.0f;
    }
    const float available = dssRowSpanFactorForOverhang(
        static_cast<float>(supplementalBandwidthMhz / targetBandwidthMhz),
        shape);
    const float fraction =
        static_cast<float>(std::clamp(spanPercent, 0, 100)) / 100.0f;
    return 1.0f + fraction * (available - 1.0f);
}

// Project a normalized frequency coordinate onto the same perspective
// plane used by both DSS renderers. depth=0 is the full-width front edge;
// depth=1 is the narrowed back edge. Slice overlays use this helper so
// their apparent angle cannot drift from the FFT surface.
//
// Frequency-in / screen-out, and independent of rowSpanFactor: widening a
// row extends the frequency range it covers, never where a frequency lands.
inline QPointF dssProjectPerspective(float frequencyUnit, float depth,
                                     const DssShape& shape)
{
    const float d = std::clamp(depth, 0.0f, 1.0f);
    const float width = dssDepthScale(d, shape);
    return QPointF(0.5f + (frequencyUnit - 0.5f) * width,
                   1.0f - d * shape.depthSpanFrac);
}

// Project a point onto the visible top of a DSS ridge. This is the CPU
// equivalent of dss_mesh.vert's height mapping and is used by overlays
// that must stay attached to the surface when the 3D floor moves.
inline QPointF dssProjectSurface(float frequencyUnit, float depth, float dbm,
                                 float floorDbm, float rangeDb, float zCurve,
                                 const DssShape& shape)
{
    const float d = std::clamp(depth, 0.0f, 1.0f);
    const float width = dssDepthScale(d, shape);
    const float finiteDbm = std::isfinite(dbm) ? dbm : floorDbm;
    const float strengthLinear = std::clamp(
        (finiteDbm - floorDbm) / std::max(rangeDb, 1.0f),
        0.0f, 1.0f);
    const float strengthHeight = std::pow(
        strengthLinear, std::max(zCurve, 0.05f));
    QPointF point = dssProjectPerspective(frequencyUnit, d, shape);
    point.ry() -=
        static_cast<double>(strengthHeight) * shape.frontMaxRidgeFrac * width;
    return point;
}

}  // namespace NereusSDR
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build build --target tst_dss_geometry && ctest --test-dir build -R '^tst_dss_geometry$' --output-on-failure
```

Expected: PASS. The class declares 7 `private slots`; Qt's runner adds
`initTestCase` and `cleanupTestCase`, so the output reports 9. Do not read
the slot count as a failure.

- [ ] **Step 6b: Register the file's provenance**

`DssGeometry.h` carries an AetherSDR header and `// From AetherSDR` cites, so
it must be registered now or the pre-commit hook blocks every commit on the
branch. Add this row to the Bucket A table in
`docs/attribution/aethersdr-reconciliation.md`:

```
| `src/gui/DssGeometry.h` | `src/gui/DssRenderer.h` | Verbatim lift of the perspective constants (`:45-51`) and projection functions (`:85-187`) at `[@1872028c]`: depthScale, projectPerspective, projectSurface, rowSpanFactorFor, wedgeFreeDepth, rowScreenCoverage, rowFrequencyUnit. Two marked NereusSDR deviations (`//-KG4VCF [v0.5.3]`): the three shape constants are promoted to a runtime `DssShape` for the 3D Angle control, and the transition-row reserve is documented as always one row here. | "3DSS perspective geometry constants and projection functions ported verbatim from AetherSDR `src/gui/DssRenderer.h`; the shape constants are parameterised at runtime for the NereusSDR-original 3D Angle control." |
```

Then confirm the gate is clean before committing:

```bash
python3 scripts/check-new-ports.py --full-tree
```

Expected: `OK [full-tree]` with no flagged files. If it still flags
`DssGeometry.h`, the row is malformed or in the wrong table.

- [ ] **Step 7: Commit**

```bash
git add src/gui/DssGeometry.h tests/tst_dss_geometry.cpp tests/CMakeLists.txt \
        docs/attribution/aethersdr-reconciliation.md
git commit -m "feat(dss): lift AetherSDR perspective geometry with a runtime shape

Tier 1 verbatim lift of DssRenderer.h:36-187 [@1872028c], with the three
shape constants promoted from static constexpr to a runtime DssShape so
the 3D Angle slider can drive them. Upstream values are preserved as
kDssUpstreamShape and reproduced exactly by dssShapeForAngle(50).

The golden test asserts every upstream constant and the exact outputs of
depthScale, projectPerspective, projectSurface and the row-span helpers.
It is what separates a reference-guided reimplementation from having
accidentally built something else."
```

---

## Task 2: Angle mapping safety properties

**Files:**
- Test: `tests/tst_dss_angle.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `dssShapeForAngle`, `dssProjectSurface`, `dssMaxRowSpanFactor`, `kDssUpstreamShape` from Task 1.
- Produces: nothing consumed by later tasks. This task is pure verification of Task 1's angle mapping.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_dss_angle.cpp`:

```cpp
#include <QTest>
#include <cmath>

#include "gui/DssGeometry.h"

using namespace NereusSDR;

class TestDssAngle : public QObject {
    Q_OBJECT

private slots:
    // Slider 50 must reproduce AetherSDR's fixed look bit for bit, or the
    // default view is not the view the design promised.
    void angle50_reproducesUpstreamExactly() {
        const DssShape s = dssShapeForAngle(50);
        QVERIFY(std::abs(s.backWidthFrac
                         - kDssUpstreamShape.backWidthFrac)     < 1e-6f);
        QVERIFY(std::abs(s.depthSpanFrac
                         - kDssUpstreamShape.depthSpanFrac)     < 1e-6f);
        QVERIFY(std::abs(s.frontMaxRidgeFrac
                         - kDssUpstreamShape.frontMaxRidgeFrac) < 1e-6f);
    }

    // The whole point of deriving ridge height instead of exposing it: no
    // angle may push a back-row peak off the top of the plot.
    void everyAngle_keepsRidgesOnScreen() {
        for (int a = 0; a <= 100; ++a) {
            const DssShape s = dssShapeForAngle(a);
            // Worst case: full-scale signal on the deepest row.
            const QPointF back = dssProjectSurface(
                0.5f, 1.0f, 0.0f, -100.0f, 100.0f, 1.0f, s);
            QVERIFY2(back.y() >= 0.0,
                     qPrintable(QStringLiteral("back ridge off top at angle %1"
                                               " (y=%2)").arg(a).arg(back.y())));
            const QPointF front = dssProjectSurface(
                0.5f, 0.0f, 0.0f, -100.0f, 100.0f, 1.0f, s);
            QVERIFY2(front.y() >= 0.0,
                     qPrintable(QStringLiteral("front ridge off top at angle %1")
                                    .arg(a)));
            QVERIFY(front.y() <= 1.0);
        }
    }

    // Monotonic travel: raising the angle spreads the stack further up the
    // plot and weakens the convergence, together, like a real camera.
    void angle_movesSpreadAndConvergenceTogether() {
        for (int a = 0; a < 100; ++a) {
            const DssShape lo = dssShapeForAngle(a);
            const DssShape hi = dssShapeForAngle(a + 1);
            QVERIFY(hi.backWidthFrac > lo.backWidthFrac);
            QVERIFY(hi.depthSpanFrac > lo.depthSpanFrac);
        }
    }

    // Ridge height falls as the view tips toward top-down, once past the
    // clamp that bounds the edge-on end.
    void ridgeHeight_shrinksAsAngleRises() {
        const DssShape edgeOn = dssShapeForAngle(0);
        const DssShape mid    = dssShapeForAngle(50);
        const DssShape topDown = dssShapeForAngle(100);
        QCOMPARE(edgeOn.frontMaxRidgeFrac, kDssMaxRidgeFrac);  // clamped
        QVERIFY(mid.frontMaxRidgeFrac    < edgeOn.frontMaxRidgeFrac);
        QVERIFY(topDown.frontMaxRidgeFrac < mid.frontMaxRidgeFrac);
    }

    void anglePercent_isClamped() {
        const DssShape lo = dssShapeForAngle(-50);
        const DssShape hi = dssShapeForAngle(500);
        QCOMPARE(lo.backWidthFrac, dssShapeForAngle(0).backWidthFrac);
        QCOMPARE(hi.backWidthFrac, dssShapeForAngle(100).backWidthFrac);
    }

    // Widest span occurs at the most dramatic angle. Task 3's mesh sizing
    // depends on this being the low end of the travel.
    void maxRowSpan_isWidestAtAngleZero() {
        const float atZero = dssMaxRowSpanFactor(dssShapeForAngle(0));
        const float atFifty = dssMaxRowSpanFactor(dssShapeForAngle(50));
        const float atHundred = dssMaxRowSpanFactor(dssShapeForAngle(100));
        QVERIFY(atZero > atFifty);
        QVERIFY(atFifty > atHundred);
        QVERIFY(std::abs(atFifty - 1.0f / 0.60f) < 1e-6f);
    }
};

QTEST_APPLESS_MAIN(TestDssAngle)
#include "tst_dss_angle.moc"
```

- [ ] **Step 2: Register and run to confirm it fails**

Add `nereus_add_test(tst_dss_angle)` to `tests/CMakeLists.txt` in alphabetical position, then:

```bash
cmake --build build --target tst_dss_angle && ctest --test-dir build -R '^tst_dss_angle$' --output-on-failure
```

Expected: PASS immediately if Task 1 is correct. If `angle50_reproducesUpstreamExactly` fails, `kDssRidgeHeadroom` was written as a rounded literal instead of derived from the constants; fix Task 1 rather than loosening the tolerance.

- [ ] **Step 3: Commit**

```bash
git add tests/tst_dss_angle.cpp tests/CMakeLists.txt
git commit -m "test(dss): pin the 3D Angle mapping's safety properties

Asserts slider 50 reproduces upstream's geometry exactly, that no angle
in the travel can push a full-scale back-row ridge off the top of the
plot, that spread and convergence move together as a real camera would,
and that the widest row span falls at the edge-on end (which Task 3's
mesh sizing relies on)."
```

---

## Task 3: DssMeshGeometry, vertex generation with angle-sized columns

**Files:**
- Create: `src/gui/DssMeshGeometry.h`
- Test: `tests/tst_dss_mesh_geometry.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `kDssCols`, `kDssVisibleRows`, `dssMaxRowSpanFactor`, `dssShapeForAngle`, `DssShape` from Task 1.
- Produces:
  - `int dssMeshColsFor(const DssShape&)`
  - `int dssFillVerticesPerRow(int meshCols)`
  - `int dssLineVerticesPerRow(int meshCols)`
  - `qint64 dssMeshBytesFor(int meshCols)`
  - `void dssBuildMeshVertices(int meshCols, QVector<float>& fillOut, QVector<float>& lineOut)`
  - `bool dssMeshDensityHolds(int meshCols, const DssShape&)`

- [ ] **Step 1: Read upstream's vertex generation**

```bash
git -C /Users/j.j.boyd/AetherSDR show 1872028c:src/gui/SpectrumWidget.cpp | sed -n '148,172p'
git -C /Users/j.j.boyd/AetherSDR show 1872028c:src/gui/SpectrumWidget.cpp | sed -n '13136,13200p'
```

Note the edge encoding, which `dss_mesh.vert:180-195` decodes: fill vertices use edge `0` (ridge) and `1` (floor), offset by `+2` for the overlay layer; ribbon outline vertices use `-10 - side` for the base layer and `-20 - side` for the overlay layer.

- [ ] **Step 2: Write the failing test**

Create `tests/tst_dss_mesh_geometry.cpp`:

```cpp
#include <QTest>
#include <QVector>
#include <cmath>

#include "gui/DssMeshGeometry.h"

using namespace NereusSDR;

class TestDssMeshGeometry : public QObject {
    Q_OBJECT

private slots:
    // At the default angle the mesh must cost exactly what upstream costs.
    // Upstream states 33.7 MiB at SpectrumWidget.cpp:150-157 [@1872028c].
    void defaultAngle_matchesUpstreamMeshSize() {
        const int cols = dssMeshColsFor(dssShapeForAngle(50));
        QCOMPARE(cols, 1281);
        const qint64 bytes = dssMeshBytesFor(cols);
        const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
        QVERIFY2(mib > 33.0 && mib < 34.5,
                 qPrintable(QStringLiteral("expected ~33.8 MiB, got %1")
                                .arg(mib)));
    }

    // The whole reason the mesh is sized dynamically: the worst-case angle
    // is materially more expensive and must not be paid unconditionally.
    void widestAngle_isMoreExpensive() {
        const int wide = dssMeshColsFor(dssShapeForAngle(0));
        const int mid  = dssMeshColsFor(dssShapeForAngle(50));
        QCOMPARE(wide, 2195);
        QVERIFY(dssMeshBytesFor(wide) > dssMeshBytesFor(mid));
    }

    // The invariant dss_mesh.vert's Nearest height sampler depends on: at
    // the widest span the on-screen columns must still be at least kDssCols,
    // or bins fall between samples and a narrow carrier vanishes at a fixed
    // screen position. Must hold at EVERY angle, after resizing for it.
    void densityInvariant_holdsAtEveryAngle() {
        for (int a = 0; a <= 100; ++a) {
            const DssShape s = dssShapeForAngle(a);
            const int cols = dssMeshColsFor(s);
            QVERIFY2(dssMeshDensityHolds(cols, s),
                     qPrintable(QStringLiteral("density fails at angle %1"
                                               " (cols=%2)").arg(a).arg(cols)));
        }
    }

    // Guards the dynamic-resize bug the design calls out: computing a new
    // column count but reusing the old mesh.
    void densityInvariant_failsIfMeshNotResized() {
        const DssShape topDown = dssShapeForAngle(100);
        const DssShape edgeOn  = dssShapeForAngle(0);
        const int narrowMesh = dssMeshColsFor(topDown);
        QVERIFY(dssMeshDensityHolds(narrowMesh, topDown));
        // Same mesh, dramatic angle: must be detected as too sparse.
        QVERIFY(!dssMeshDensityHolds(narrowMesh, edgeOn));
    }

    void vertexCounts_matchUpstreamFormula() {
        QCOMPARE(dssFillVerticesPerRow(1281), (1281 - 1) * 6 * 2);
        QCOMPARE(dssLineVerticesPerRow(1281), (1281 - 1) * 6 * 2);
    }

    void buildMeshVertices_producesExpectedSizeAndRange() {
        constexpr int kCols = 64;   // small mesh keeps the test fast
        QVector<float> fill;
        QVector<float> line;
        dssBuildMeshVertices(kCols, fill, line);
        QCOMPARE(fill.size(),
                 kDssVisibleRows * dssFillVerticesPerRow(kCols) * 3);
        QCOMPARE(line.size(),
                 kDssVisibleRows * dssLineVerticesPerRow(kCols) * 3);
        // u in [0,1], v in [0,1). The shader multiplies v by visibleRows.
        for (int i = 0; i < fill.size(); i += 3) {
            QVERIFY(fill[i]     >= 0.0f && fill[i]     <= 1.0f);
            QVERIFY(fill[i + 1] >= 0.0f && fill[i + 1] <  1.0f);
        }
    }

    // Rows are emitted back to front so the painter's-algorithm draw order
    // lets nearer curtains occlude farther ones.
    void buildMeshVertices_emitsBackToFront() {
        constexpr int kCols = 8;
        QVector<float> fill;
        QVector<float> line;
        dssBuildMeshVertices(kCols, fill, line);
        const float firstV = fill[1];
        const float lastV  = fill[fill.size() - 2];
        QVERIFY2(firstV > lastV,
                 "first emitted row must be the deepest (largest v)");
    }

    // Edge tags must land in the ranges dss_mesh.vert:180-195 decodes.
    void buildMeshVertices_usesDecodableEdgeTags() {
        constexpr int kCols = 8;
        QVector<float> fill;
        QVector<float> line;
        dssBuildMeshVertices(kCols, fill, line);
        for (int i = 2; i < fill.size(); i += 3) {
            const float e = fill[i];
            const bool baseLayer    = (e == 0.0f || e == 1.0f);
            const bool overlayLayer = (e == 2.0f || e == 3.0f);
            QVERIFY2(baseLayer || overlayLayer,
                     qPrintable(QStringLiteral("bad fill edge tag %1").arg(e)));
        }
        for (int i = 2; i < line.size(); i += 3) {
            const float e = line[i];
            // Ribbon outlines encode as -(10 + side) or -(20 + side).
            QVERIFY2(e <= -10.0f,
                     qPrintable(QStringLiteral("bad line edge tag %1").arg(e)));
            const float code = -e;
            const bool baseRibbon    = (code == 10.0f || code == 11.0f);
            const bool overlayRibbon = (code == 20.0f || code == 21.0f);
            QVERIFY(baseRibbon || overlayRibbon);
        }
    }
};

QTEST_APPLESS_MAIN(TestDssMeshGeometry)
#include "tst_dss_mesh_geometry.moc"
```

- [ ] **Step 3: Register and run to confirm it fails**

Add `nereus_add_test(tst_dss_mesh_geometry)` to `tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_dss_mesh_geometry
```

Expected: FAIL at compile, `'gui/DssMeshGeometry.h' file not found`.

- [ ] **Step 4: Create DssMeshGeometry.h**

Header block per Global Constraints, then:

```cpp
#pragma once

#include <QVector>

#include <algorithm>
#include <cmath>

#include "gui/DssGeometry.h"

namespace NereusSDR {

// ─── 3DSS GPU mesh geometry ─────────────────────────────────────────────────
//
// Vertex generation for the height-map mesh sampled by dss_mesh.vert. The
// mesh carries only grid position and an edge tag; height comes from the
// ring texture, so pan and zoom never rebuild a vertex.
//
// Structure from AetherSDR src/gui/SpectrumWidget.cpp:148-172 and
// :13136-13200 [@1872028c]. NereusSDR-original: the column count is a
// function of the live perspective shape rather than a compile-time
// constant, so the 3D Angle slider does not force every panadapter to pay
// the worst-case mesh unconditionally (design doc §5.5).

// Mesh columns per row. The mesh spans rowSpanFactor x the viewport, while
// the height texture holds kDssCols texels ACROSS THE VIEWPORT — so a
// kDssCols-wide mesh would leave (span-1)/span of those texels unread. That
// is not shimmer: the mesh-column-to-frequency mapping is static, so the
// same texels are missed every frame and a narrow carrier landing on one is
// permanently invisible at a fixed screen position. Size the mesh for the
// widest span the CURRENT shape allows, so the on-screen region is never
// sparser than the texture it samples.
inline int dssMeshColsFor(const DssShape& shape)
{
    return static_cast<int>(kDssCols * dssMaxRowSpanFactor(shape)) + 1;
}

// The invariant dss_mesh.vert's Nearest height sampler depends on: at the
// widest span the on-screen columns (meshCols / maxRowSpanFactor) must still
// be at least kDssCols. Upstream enforces this with a static_assert; a
// runtime shape needs a runtime check, and it must be re-evaluated after
// every angle change rather than only at construction.
inline bool dssMeshDensityHolds(int meshCols, const DssShape& shape)
{
    return static_cast<float>(meshCols)
        >= kDssCols * dssMaxRowSpanFactor(shape);
}

// Two exact-row crossfade layers, each with two triangles per column.
// Built once per column count, never rebuilt while scrolling.
inline int dssFillVerticesPerRow(int meshCols)
{
    return (meshCols - 1) * 6 * 2;
}

inline int dssLineVerticesPerRow(int meshCols)
{
    return (meshCols - 1) * 6 * 2;
}

// Total static vertex storage for both VBOs, in bytes. Three floats per
// vertex (u, v, edge). Upstream measures 33.7 MiB at meshCols = 1281.
inline qint64 dssMeshBytesFor(int meshCols)
{
    const qint64 verts =
        static_cast<qint64>(kDssVisibleRows)
        * (dssFillVerticesPerRow(meshCols) + dssLineVerticesPerRow(meshCols));
    return verts * 3 * static_cast<qint64>(sizeof(float));
}

namespace detail {

inline void appendDssVertex(QVector<float>& vertices,
                            float u, float v, float edge)
{
    vertices << u << v << edge;
}

}  // namespace detail

// Build the static perspective grid. Rows are emitted back to front so the
// painter's-algorithm draw order lets nearer curtains occlude farther ones.
// Base layer first, overlay second: curtains use both only at the fixed
// front and rear boundaries, ridge outlines use both at every fixed depth
// for exact-row crossfades.
inline void dssBuildMeshVertices(int meshCols,
                                 QVector<float>& fillOut,
                                 QVector<float>& lineOut)
{
    fillOut.clear();
    lineOut.clear();
    if (meshCols < 2) {
        return;
    }
    const int rows = kDssVisibleRows;
    fillOut.reserve(rows * dssFillVerticesPerRow(meshCols) * 3);
    lineOut.reserve(rows * dssLineVerticesPerRow(meshCols) * 3);

    const float du = 1.0f / static_cast<float>(meshCols - 1);
    for (int rr = rows - 1; rr >= 0; --rr) {
        const float v = static_cast<float>(rr) / rows;  // 0 front .. ~1 back
        for (int layer = 0; layer < 2; ++layer) {
            const float edgeBias = (layer == 1) ? 2.0f : 0.0f;
            const float ribbonBase = (layer == 1) ? 20.0f : 10.0f;
            for (int c = 0; c + 1 < meshCols; ++c) {
                const float u0 = c * du;
                const float u1 = (c + 1) * du;
                // Curtain quad: ridge (edge 0) down to plot floor (edge 1).
                detail::appendDssVertex(fillOut, u0, v, 0.0f + edgeBias);
                detail::appendDssVertex(fillOut, u1, v, 0.0f + edgeBias);
                detail::appendDssVertex(fillOut, u0, v, 1.0f + edgeBias);
                detail::appendDssVertex(fillOut, u1, v, 0.0f + edgeBias);
                detail::appendDssVertex(fillOut, u1, v, 1.0f + edgeBias);
                detail::appendDssVertex(fillOut, u0, v, 1.0f + edgeBias);
                // Ridge outline expanded into a two-pixel screen-space
                // ribbon; the vertex shader offsets by the encoded side.
                const float sideLo = -(ribbonBase + 0.0f);
                const float sideHi = -(ribbonBase + 1.0f);
                detail::appendDssVertex(lineOut, u0, v, sideLo);
                detail::appendDssVertex(lineOut, u1, v, sideLo);
                detail::appendDssVertex(lineOut, u0, v, sideHi);
                detail::appendDssVertex(lineOut, u1, v, sideLo);
                detail::appendDssVertex(lineOut, u1, v, sideHi);
                detail::appendDssVertex(lineOut, u0, v, sideHi);
            }
        }
    }
}

}  // namespace NereusSDR
```

- [ ] **Step 5: Run to verify it passes**

```bash
cmake --build build --target tst_dss_mesh_geometry && ctest --test-dir build -R '^tst_dss_mesh_geometry$' --output-on-failure
```

Expected: PASS, 8 test functions. If `defaultAngle_matchesUpstreamMeshSize` reports a column count other than 1281, check `dssMaxRowSpanFactor(dssShapeForAngle(50))` is exactly `1/0.60`.

- [ ] **Step 6: Commit**

```bash
git add src/gui/DssMeshGeometry.h tests/tst_dss_mesh_geometry.cpp tests/CMakeLists.txt
git commit -m "feat(dss): generate the 3DSS mesh with angle-sized columns

Vertex generation follows AetherSDR SpectrumWidget.cpp:148-172 and
:13136-13200 [@1872028c]. NereusSDR-original: the column count is derived
from the live perspective shape instead of a compile-time constant.

Measured at the default angle the mesh costs 33.8 MiB per panadapter,
reproducing upstream's stated figure exactly. Sizing unconditionally for
the widest angle would cost 57.8 MiB per pan and 231.4 MiB across four, so
the mesh is rebuilt when the angle changes the column count instead.

dssMeshDensityHolds replaces upstream's static_assert and is tested to
fail when a narrow mesh is left in place under a dramatic angle, which is
the resize bug the dynamic sizing could otherwise introduce silently."
```

---

## Task 4: DssRenderer ring store

**Files:**
- Create: `src/gui/DssRenderer.h`, `src/gui/DssRenderer.cpp`
- Test: `tests/tst_dss_renderer_ring.cpp`
- Modify: `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `kDssCols`, `kDssRows`, `kDssVisibleRows` from Task 1.
- Produces:
  - `class NereusSDR::DssRenderer` with:
    - `void pushRow(const QVector<float>& binsDbm, double centerMhz, double bandwidthMhz)`
    - `void pushRowWithWide(const QVector<float>& binsDbm, double centerMhz, double bandwidthMhz, const QVector<float>& wideBinsDbm, double wideCenterMhz, double wideBandwidthMhz)`
    - `int rowCount() const`, `int visibleRowCount() const`, `int headRing() const`
    - `const float* rowDataRing(int) const`, `const quint8* rowCoverageRing(int) const`
    - `const float* rowWideDataRing(int) const`, `const quint8* rowWideCoverageRing(int) const`
    - `double rowCenterMhzAtAge(int) const`, `double rowBandwidthMhzAtAge(int) const`
    - `double rowWideCenterMhzAtAge(int) const`, `double rowWideBandwidthMhzAtAge(int) const`
    - `double newestWideBandwidthMhz(double targetBandwidthMhz) const`
    - `void resetInputSmoothing()`, `void clear()`, `void invalidate()`, `bool hasData() const`
    - `quint64 rowGeneration() const`, `quint64 generation() const`
    - `int cols() const`, `int rows() const`

- [ ] **Step 1: Read the upstream ring store**

```bash
git -C /Users/j.j.boyd/AetherSDR show 1872028c:src/gui/DssRenderer.cpp | sed -n '10,60p'
git -C /Users/j.j.boyd/AetherSDR show 1872028c:src/gui/DssRenderer.cpp | sed -n '182,300p'
git -C /Users/j.j.boyd/AetherSDR show 1872028c:src/gui/DssRenderer.cpp | sed -n '389,480p'
```

`resampledRawRow` is the peak-preserving downsample, `smoothDssRow` the median-of-3 plus 1-2-1 spatial plus temporal IIR chain, `pushRowWithSupplemental` the ring write. Lift all three verbatim, renaming `supplemental` to `wide` throughout (the concept differs: ours is the same FFT, not a separate calibrated source).

- [ ] **Step 2: Write the failing test**

Create `tests/tst_dss_renderer_ring.cpp`:

```cpp
#include <QTest>
#include <QVector>
#include <cmath>

#include "gui/DssRenderer.h"

using namespace NereusSDR;

namespace {

QVector<float> flatRow(int n, float dbm)
{
    QVector<float> v(n, dbm);
    return v;
}

}  // namespace

class TestDssRendererRing : public QObject {
    Q_OBJECT

private slots:
    void emptyRenderer_hasNoData() {
        DssRenderer r;
        QVERIFY(!r.hasData());
        QCOMPARE(r.rowCount(), 0);
        QCOMPARE(r.cols(), kDssCols);
        QCOMPARE(r.rows(), kDssRows);
    }

    // A single-bin carrier must survive the squeeze from full FFT width to
    // 768 columns. A mean-based downsample would bury it; the reduction is
    // peak-preserving precisely so it cannot.
    void singleBinCarrier_survivesDownsample() {
        DssRenderer r;
        QVector<float> bins = flatRow(8192, -130.0f);
        bins[4096] = -40.0f;
        // Three identical pushes clear the median-of-3 impulse rejector,
        // which would otherwise treat a one-frame spike as interference.
        for (int i = 0; i < 3; ++i) {
            r.pushRow(bins, 14.2, 0.192);
        }
        const float* row = r.rowDataRing(r.headRing());
        float peak = -1000.0f;
        int peakCol = -1;
        for (int c = 0; c < kDssCols; ++c) {
            if (row[c] > peak) { peak = row[c]; peakCol = c; }
        }
        // Two independent properties, because either alone is weak.
        //
        // The carrier must still be the loudest column, and at the column it
        // belongs in. This is blur-independent and is how upstream's own
        // dss_renderer_test.cpp checks the same thing.
        const int expectedCol = 4096 * kDssCols / 8192;
        QCOMPARE(peakCol, expectedCol);
        //
        // And it must still stand well clear of the floor in absolute terms,
        // which is what actually catches a mean-based downsample. Do not
        // tighten this past -85: the unconditional 1-2-1 spatial blur maps an
        // isolated column to 0.25*floor + 0.5*carrier + 0.25*floor, so a
        // -40 dBm carrier on a -130 dBm floor converges to exactly -85.0 and
        // no number of further pushes moves it. A mean-based downsample would
        // land near -125.8 instead, so -100 discriminates with 15 dB of margin
        // above the true value and 25 dB below the broken one.
        QVERIFY2(peak > -100.0f,
                 qPrintable(QStringLiteral("carrier lost, peak=%1").arg(peak)));
    }

    // Broadband impulse noise lasting one frame is rejected by median-of-3.
    void singleFrameImpulse_isRejected() {
        DssRenderer r;
        const QVector<float> quiet = flatRow(768, -130.0f);
        r.pushRow(quiet, 14.2, 0.192);
        r.pushRow(quiet, 14.2, 0.192);
        r.pushRow(flatRow(768, -20.0f), 14.2, 0.192);   // one-frame burst
        const float* row = r.rowDataRing(r.headRing());
        QVERIFY2(row[384] < -100.0f,
                 qPrintable(QStringLiteral("impulse not rejected: %1")
                                .arg(row[384])));
    }

    void ringWrap_keepsCountBounded() {
        DssRenderer r;
        for (int i = 0; i < kDssRows + 20; ++i) {
            r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);
        }
        QCOMPARE(r.rowCount(), kDssRows);
        QCOMPARE(r.visibleRowCount(), kDssVisibleRows);
        QVERIFY(r.headRing() >= 0 && r.headRing() < kDssRows);
    }

    void pushRow_stampsFrequencyFrame() {
        DssRenderer r;
        r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);
        QCOMPARE(r.rowCenterMhzAtAge(0),    14.2);
        QCOMPARE(r.rowBandwidthMhzAtAge(0), 0.192);
        r.pushRow(flatRow(768, -130.0f), 7.1, 0.096);
        QCOMPARE(r.rowCenterMhzAtAge(0),    7.1);
        QCOMPARE(r.rowCenterMhzAtAge(1),    14.2);
        QCOMPARE(r.rowBandwidthMhzAtAge(1), 0.192);
    }

    // A retune must not blend the new row against the previous one: adjacent
    // bin indices no longer represent the same frequencies, and blending
    // smears every signal in the direction of the pan.
    void frameChange_dropsTemporalBlend() {
        DssRenderer r;
        for (int i = 0; i < 4; ++i) {
            r.pushRow(flatRow(768, -60.0f), 14.2, 0.192);
        }
        r.pushRow(flatRow(768, -130.0f), 21.0, 0.192);   // retune
        const float* row = r.rowDataRing(r.headRing());
        QVERIFY2(row[384] < -125.0f,
                 qPrintable(QStringLiteral("blended across retune: %1")
                                .arg(row[384])));
    }

    void pushRow_withoutWide_marksWideUncovered() {
        DssRenderer r;
        r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);
        const quint8* cov = r.rowWideCoverageRing(r.headRing());
        QCOMPARE(cov[0],   quint8(0));
        QCOMPARE(cov[384], quint8(0));
        QCOMPARE(r.newestWideBandwidthMhz(0.192), 0.0);
    }

    void pushRowWithWide_recordsBothChannels() {
        DssRenderer r;
        r.pushRowWithWide(flatRow(768, -130.0f), 14.2, 0.192,
                          flatRow(768, -140.0f), 14.2, 0.500);
        const quint8* exact = r.rowCoverageRing(r.headRing());
        const quint8* wide  = r.rowWideCoverageRing(r.headRing());
        QCOMPARE(exact[384], quint8(1));
        QCOMPARE(wide[384],  quint8(1));
        QCOMPARE(r.rowWideBandwidthMhzAtAge(0), 0.500);
        QCOMPARE(r.newestWideBandwidthMhz(0.192), 0.500);
    }

    // The front row is deliberately not authoritative for the wide channel:
    // a producer that appends without a wide slice must not collapse the
    // overhang to nothing while rows carrying it are still on screen.
    void newestWideBandwidth_looksPastBareFrontRows() {
        DssRenderer r;
        r.pushRowWithWide(flatRow(768, -130.0f), 14.2, 0.192,
                          flatRow(768, -140.0f), 14.2, 0.500);
        r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);   // no wide slice
        QCOMPARE(r.newestWideBandwidthMhz(0.192), 0.500);
    }

    void clear_resetsEverything() {
        DssRenderer r;
        r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);
        QVERIFY(r.hasData());
        r.clear();
        QVERIFY(!r.hasData());
        QCOMPARE(r.rowCount(), 0);
    }

    // clear() must also wipe the per-row frequency stamps, because
    // ringAtAge() clamps to ring bounds WITHOUT the m_count guard that
    // upstream's age accessors carry. Without the wipe, a post-clear read
    // hands back a stale stamp.
    //
    // Filling the whole ring first is what makes this test bind, and it is
    // not optional. m_head walks BACKWARD on push and clear() resets it to 0,
    // so after a single push the written slot is index kDssRows-1 while
    // ringAtAge(0) reads index 0 -- a slot that was never written and is
    // already value-initialised to 0.0. The assertions would then pass
    // against a clear() that wipes nothing at all. Verified by mutation:
    // with the fills commented out, the single-push form still reported
    // 13/13 passing. Push kDssRows rows so every slot carries the stamp and
    // index 0 is genuinely dirty.
    void clear_wipesFrameStampsAcrossTheWholeRing() {
        DssRenderer r;
        for (int i = 0; i < kDssRows; ++i) {
            r.pushRowWithWide(flatRow(768, -130.0f), 14.2, 0.192,
                              flatRow(768, -140.0f), 14.2, 0.500);
        }
        QCOMPARE(r.rowCenterMhzAtAge(0), 14.2);   // precondition: slot is dirty
        r.clear();
        QCOMPARE(r.rowCenterMhzAtAge(0),        0.0);
        QCOMPARE(r.rowBandwidthMhzAtAge(0),     0.0);
        QCOMPARE(r.rowWideCenterMhzAtAge(0),    0.0);
        QCOMPARE(r.rowWideBandwidthMhzAtAge(0), 0.0);
        QCOMPARE(r.rowWideCoverageRing(r.headRing())[0], quint8(0));
    }

    void rowGeneration_advancesOnEveryPush() {
        DssRenderer r;
        const quint64 before = r.rowGeneration();
        r.pushRow(flatRow(768, -130.0f), 14.2, 0.192);
        QVERIFY(r.rowGeneration() > before);
    }
};

QTEST_APPLESS_MAIN(TestDssRendererRing)
#include "tst_dss_renderer_ring.moc"
```

- [ ] **Step 3: Register the test and add the source to the build**

Add `nereus_add_test(tst_dss_renderer_ring)` to `tests/CMakeLists.txt`. In the root `CMakeLists.txt`, add `src/gui/DssRenderer.cpp` to the `GUI_SOURCES` list in alphabetical position.

- [ ] **Step 4: Run to confirm it fails**

```bash
cmake --build build --target tst_dss_renderer_ring
```

Expected: FAIL at compile, `'gui/DssRenderer.h' file not found`.

- [ ] **Step 5: Write DssRenderer.h**

Header block per Global Constraints, then the class. Preserve upstream's member comments verbatim; rename `supplemental` to `wide` and drop the `m_history*` scrollback members entirely (Task 13 covers scrollback separately if it is reached; the design's live surface does not need them).

```cpp
#pragma once

#include <QColor>
#include <QImage>
#include <QSize>
#include <QVector>

#include <array>
#include <functional>

#include "gui/DssGeometry.h"

namespace NereusSDR {

// ─── Stacked-trace spectrum stream surface ──────────────────────────────────
//
// From AetherSDR src/gui/DssRenderer.h [@1872028c].
//
// Renders a perspective stacked-trace spectrum stream: a rolling history of
// FFT rows drawn back-to-front (painter's algorithm) as a receding trapezoid.
// The newest trace spans the full width across the front; older traces recede
// into a narrower, higher trapezoid. Each ridge is filled down to the plot
// floor so nearer traces occlude farther ones.
//
// The renderer is standalone and knows nothing about SpectrumWidget or QRhi.
//
// NereusSDR divergence: upstream's "supplemental" channel carries native FLEX
// waterfall tiles, a separate measurement on an arbitrary display scale that
// must be quantile-calibrated before use. Ours carries off-screen DDC bins
// from the SAME FFT at the SAME calibration, so the arbitration collapses to
// a bin-range test and upstream's DssSupplementalCoverage.h is not ported.
// Renamed "wide" throughout to keep the distinction visible at every callsite.
class DssRenderer {
public:
    using PaletteFn = std::function<QRgb(float dbm)>;

    // Push one freshly-decoded FFT row (any bin count, dBm). Peak-preserving
    // downsample to kDssCols and store it as the newest (front) trace.
    void pushRow(const QVector<float>& binsDbm,
                 double centerMhz = 0.0,
                 double bandwidthMhz = 0.0);
    void pushRowWithWide(const QVector<float>& binsDbm,
                         double centerMhz,
                         double bandwidthMhz,
                         const QVector<float>& wideBinsDbm,
                         double wideCenterMhz,
                         double wideBandwidthMhz);

    void invalidate() { m_dirty = true; }
    bool hasData() const { return m_count > 0; }
    // Forget temporal filter inputs without discarding already-decoded dBm
    // rows. Used when the upstream raw-pixel scale changes.
    void resetInputSmoothing();
    void clear();

    // ── Data-model accessors for the GPU mesh path ──────────────────────────
    // Indices are RING indices.
    int cols() const { return kDssCols; }
    int rows() const { return kDssRows; }
    int rowCount() const { return m_count; }
    int visibleRowCount() const
    {
        return std::min(m_count, kDssVisibleRows);
    }
    int headRing() const { return m_head; }
    const float* rowDataRing(int ringIndex) const
    {
        return m_rows[ringIndex].data();
    }
    const quint8* rowCoverageRing(int ringIndex) const
    {
        return m_rowCoverage[ringIndex].data();
    }
    const float* rowWideDataRing(int ringIndex) const
    {
        return m_rowWide[ringIndex].data();
    }
    const quint8* rowWideCoverageRing(int ringIndex) const
    {
        return m_rowWideCoverage[ringIndex].data();
    }
    double rowCenterMhzAtAge(int age) const;
    double rowBandwidthMhzAtAge(int age) const;
    double rowWideCenterMhzAtAge(int age) const;
    double rowWideBandwidthMhzAtAge(int age) const;

    // Bandwidth of the newest VISIBLE row carrying an overhang wider than
    // targetBandwidthMhz, or 0 when no such row is on screen.
    //
    // The front row is deliberately not authoritative: a producer that
    // appends with no wide slice would otherwise drop the overhang to nothing
    // while 90-odd rows of it are still on screen. Once the last covered row
    // scrolls out, returning 0 is the correct answer.
    double newestWideBandwidthMhz(double targetBandwidthMhz) const;

    quint64 rowGeneration() const { return m_rowGeneration; }
    // Increments on every cache rebuild — lets the GPU path upload the
    // texture only when the surface actually changed.
    quint64 generation() const { return m_generation; }

private:
    int ringAtAge(int age) const;

    // Circular store: m_head indexes the newest row.
    std::array<std::array<float, kDssCols>, kDssRows> m_rows{};
    // One byte per bin records whether that frequency existed in the captured
    // row. Reprojection-created gaps remain drawable floor lines, but the
    // marker lets both renderers keep their height/colour independent of
    // later zoom and dBm-range changes.
    std::array<std::array<quint8, kDssCols>, kDssRows> m_rowCoverage{};
    // Off-screen DDC bins, kept in separate channels so they fill only
    // frequencies the exact FFT row never captured. Must never resample or
    // replace the working FFT trace.
    std::array<std::array<float, kDssCols>, kDssRows> m_rowWide{};
    std::array<std::array<quint8, kDssCols>, kDssRows> m_rowWideCoverage{};
    // Each live row keeps the frequency frame in which its bins were
    // captured, so the GPU can map rows independently while zooming.
    std::array<double, kDssRows> m_rowCenterMhz{};
    std::array<double, kDssRows> m_rowBandwidthMhz{};
    std::array<double, kDssRows> m_rowWideCenterMhz{};
    std::array<double, kDssRows> m_rowWideBandwidthMhz{};

    int     m_head  = 0;
    int     m_count = 0;
    bool    m_dirty = true;
    quint64 m_generation = 0;
    quint64 m_rowGeneration = 0;

    // Last two RAW (pre-smoothing) resampled rows, for temporal median-of-3
    // impulse rejection of broadband interference bursts.
    std::array<float, kDssCols> m_rawPrev1{};
    std::array<float, kDssCols> m_rawPrev2{};
    int m_rawHistCount = 0;
    // The wide channel needs an independent copy of the same smoothing
    // chain. Sharing FFT history would blend different frequency frames.
    std::array<float, kDssCols> m_wideRawPrev1{};
    std::array<float, kDssCols> m_wideRawPrev2{};
    int m_wideRawHistCount = 0;

    // One-shot flags: after resetInputSmoothing() the next pushed row must
    // not blend against the retained row that preceded the reset.
    bool m_skipLiveTemporalBlendOnce = false;
    bool m_skipWideTemporalBlendOnce = false;
};

}  // namespace NereusSDR
```

- [ ] **Step 6: Write DssRenderer.cpp**

Header block per Global Constraints. Lift `median3`, `frequencyFramesMatch`, `resampledRawRow` and `smoothDssRow` verbatim from upstream `DssRenderer.cpp:28-30`, `:49-61`, `:182-215` and `:217-252`, substituting `kDssCols` for `DssRenderer::kCols`. Then:

```cpp
#include "gui/DssRenderer.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// From AetherSDR src/gui/DssRenderer.cpp:17 [@1872028c].
constexpr float kTemporalAlpha = 0.60f;  // temporal IIR: fraction of the new row

// From AetherSDR src/gui/DssRenderer.cpp:28-30 [@1872028c].
inline float median3(float a, float b, float c)
{
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

// From AetherSDR src/gui/DssRenderer.cpp:49-61 [@1872028c].
bool frequencyFramesMatch(double firstCenterMhz, double firstBandwidthMhz,
                          double secondCenterMhz, double secondBandwidthMhz)
{
    const auto nearlyEqual = [](double first, double second) {
        const double scale =
            std::max({1.0, std::abs(first), std::abs(second)});
        return std::abs(first - second) <= scale * 1.0e-9;
    };
    return nearlyEqual(firstCenterMhz, secondCenterMhz)
        && nearlyEqual(firstBandwidthMhz, secondBandwidthMhz);
}

// From AetherSDR src/gui/DssRenderer.cpp:182-215 [@1872028c].
// Peak-preserving: a one-bin carrier must survive the reduction to kDssCols.
std::array<float, kDssCols> resampledRawRow(const QVector<float>& binsDbm,
                                            float fallback)
{
    std::array<float, kDssCols> row;
    const int n = binsDbm.size();
    if (n <= 0) {
        row.fill(fallback);
        return row;
    }

    if (n == kDssCols) {
        for (int c = 0; c < kDssCols; ++c) {
            row[c] = std::isfinite(binsDbm[c]) ? binsDbm[c] : fallback;
        }
    } else {
        const double step = static_cast<double>(n) / kDssCols;
        for (int c = 0; c < kDssCols; ++c) {
            int i0 = static_cast<int>(std::floor(c * step));
            int i1 = static_cast<int>(std::ceil((c + 1) * step));
            i0 = std::clamp(i0, 0, n - 1);
            i1 = std::clamp(i1, i0 + 1, n);
            float mx = std::isfinite(binsDbm[i0]) ? binsDbm[i0] : fallback;
            for (int i = i0 + 1; i < i1; ++i) {
                if (std::isfinite(binsDbm[i])) {
                    mx = std::max(mx, binsDbm[i]);
                }
            }
            row[c] = mx;
        }
    }

    return row;
}

// From AetherSDR src/gui/DssRenderer.cpp:217-252 [@1872028c].
std::array<float, kDssCols> smoothDssRow(
    const std::array<float, kDssCols>& raw,
    std::array<float, kDssCols>& rawPrev1,
    std::array<float, kDssCols>& rawPrev2,
    int& rawHistCount,
    const std::array<float, kDssCols>* previousSmoothed)
{
    std::array<float, kDssCols> row = raw;
    if (rawHistCount >= 2) {
        for (int c = 0; c < kDssCols; ++c) {
            row[c] = median3(raw[c], rawPrev1[c], rawPrev2[c]);
        }
    }

    rawPrev2 = rawPrev1;
    rawPrev1 = raw;
    rawHistCount = std::min(rawHistCount + 1, 2);

    std::array<float, kDssCols> smoothed = row;
    for (int c = 0; c < kDssCols; ++c) {
        const float a = row[std::max(0, c - 1)];
        const float b = row[c];
        const float d = row[std::min(kDssCols - 1, c + 1)];
        smoothed[c] = 0.25f * a + 0.5f * b + 0.25f * d;
    }
    if (previousSmoothed != nullptr) {
        for (int c = 0; c < kDssCols; ++c) {
            smoothed[c] = kTemporalAlpha * smoothed[c]
                + (1.0f - kTemporalAlpha) * (*previousSmoothed)[c];
        }
    }
    return smoothed;
}

}  // namespace

int DssRenderer::ringAtAge(int age) const
{
    return (m_head + std::clamp(age, 0, kDssRows - 1)) % kDssRows;
}

double DssRenderer::rowCenterMhzAtAge(int age) const
{
    return m_rowCenterMhz[ringAtAge(age)];
}

double DssRenderer::rowBandwidthMhzAtAge(int age) const
{
    return m_rowBandwidthMhz[ringAtAge(age)];
}

double DssRenderer::rowWideCenterMhzAtAge(int age) const
{
    return m_rowWideCenterMhz[ringAtAge(age)];
}

double DssRenderer::rowWideBandwidthMhzAtAge(int age) const
{
    return m_rowWideBandwidthMhz[ringAtAge(age)];
}

double DssRenderer::newestWideBandwidthMhz(double targetBandwidthMhz) const
{
    const int visible = visibleRowCount();
    for (int age = 0; age < visible; ++age) {
        const double bw = m_rowWideBandwidthMhz[ringAtAge(age)];
        if (bw > targetBandwidthMhz) {
            return bw;
        }
    }
    return 0.0;
}

void DssRenderer::resetInputSmoothing()
{
    m_rawHistCount = 0;
    m_wideRawHistCount = 0;
    m_skipLiveTemporalBlendOnce = true;
    m_skipWideTemporalBlendOnce = true;
}

void DssRenderer::clear()
{
    m_head = 0;
    m_count = 0;
    m_dirty = true;
    m_rawHistCount = 0;
    m_wideRawHistCount = 0;
    m_skipLiveTemporalBlendOnce = false;
    m_skipWideTemporalBlendOnce = false;
    ++m_rowGeneration;
}

void DssRenderer::pushRow(const QVector<float>& binsDbm,
                          double centerMhz,
                          double bandwidthMhz)
{
    pushRowWithWide(binsDbm, centerMhz, bandwidthMhz,
                    QVector<float>{}, 0.0, 0.0);
}

void DssRenderer::pushRowWithWide(const QVector<float>& binsDbm,
                                  double centerMhz,
                                  double bandwidthMhz,
                                  const QVector<float>& wideBinsDbm,
                                  double wideCenterMhz,
                                  double wideBandwidthMhz)
{
    const std::array<float, kDssCols> raw = resampledRawRow(binsDbm, -200.0f);
    const bool sameFrameAsPrevious =
        m_count > 0
        && frequencyFramesMatch(
            m_rowCenterMhz[m_head], m_rowBandwidthMhz[m_head],
            centerMhz, bandwidthMhz);
    if (m_count > 0 && !sameFrameAsPrevious) {
        // Adjacent bin indices no longer represent the same frequencies.
        // Blending them would smear a signal in the direction of the pan.
        m_rawHistCount = 0;
    }
    const std::array<float, kDssCols>* previous =
        (sameFrameAsPrevious && !m_skipLiveTemporalBlendOnce)
            ? &m_rows[m_head]
            : nullptr;
    m_skipLiveTemporalBlendOnce = false;
    const std::array<float, kDssCols> nr =
        smoothDssRow(raw, m_rawPrev1, m_rawPrev2, m_rawHistCount, previous);

    m_head = (m_head - 1 + kDssRows) % kDssRows;
    m_rows[m_head] = nr;
    m_rowCoverage[m_head].fill(1);
    m_rowCenterMhz[m_head] = centerMhz;
    m_rowBandwidthMhz[m_head] = bandwidthMhz;

    const bool wideValid =
        !wideBinsDbm.isEmpty()
        && std::isfinite(wideCenterMhz)
        && std::isfinite(wideBandwidthMhz)
        && wideCenterMhz > 0.0
        && wideBandwidthMhz > 0.0;
    if (wideValid) {
        const std::array<float, kDssCols> wideRaw =
            resampledRawRow(wideBinsDbm, -200.0f);
        const int previousRing = (m_head + 1) % kDssRows;
        const bool sameWideFrameAsPrevious =
            m_count > 0
            && frequencyFramesMatch(
                m_rowWideCenterMhz[previousRing],
                m_rowWideBandwidthMhz[previousRing],
                wideCenterMhz, wideBandwidthMhz);
        if (m_count > 0 && !sameWideFrameAsPrevious) {
            m_wideRawHistCount = 0;
        }
        const std::array<float, kDssCols>* widePrevious =
            (sameWideFrameAsPrevious && !m_skipWideTemporalBlendOnce)
                ? &m_rowWide[previousRing]
                : nullptr;
        m_skipWideTemporalBlendOnce = false;
        m_rowWide[m_head] = smoothDssRow(
            wideRaw, m_wideRawPrev1, m_wideRawPrev2,
            m_wideRawHistCount, widePrevious);
        m_rowWideCoverage[m_head].fill(1);
        m_rowWideCenterMhz[m_head] = wideCenterMhz;
        m_rowWideBandwidthMhz[m_head] = wideBandwidthMhz;
    } else {
        m_rowWide[m_head].fill(-200.0f);
        m_rowWideCoverage[m_head].fill(0);
        m_rowWideCenterMhz[m_head] = 0.0;
        m_rowWideBandwidthMhz[m_head] = 0.0;
        m_wideRawHistCount = 0;
    }

    m_count = std::min(m_count + 1, kDssRows);
    m_dirty = true;
    ++m_rowGeneration;
}

}  // namespace NereusSDR
```

- [ ] **Step 7: Run to verify it passes**

```bash
cmake --build build --target tst_dss_renderer_ring && ctest --test-dir build -R '^tst_dss_renderer_ring$' --output-on-failure
```

Expected: PASS, 11 test functions.

- [ ] **Step 8: Commit**

```bash
git add src/gui/DssRenderer.h src/gui/DssRenderer.cpp tests/tst_dss_renderer_ring.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat(dss): add the 3DSS row ring store

Ports the ring write, peak-preserving downsample and median-of-3 plus
spatial plus temporal IIR smoothing chain from AetherSDR DssRenderer.cpp
[@1872028c].

Upstream's supplemental channel is renamed wide throughout. The concept
genuinely differs: upstream carries native FLEX waterfall tiles, a
separate measurement on an arbitrary display scale needing quantile
calibration, so it ports DssSupplementalCoverage.h. Ours carries
off-screen DDC bins from the same FFT at the same calibration, so that
file is not ported and the arbitration is a bin-range test. Keeping the
name distinct stops the two ideas being conflated at a callsite later.

Tests pin the two properties the reduction exists for: a single-bin
carrier survives the squeeze to 768 columns, and a one-frame broadband
burst does not."
```

---

## Task 5: Shaders and build wiring

**Files:**
- Create: `resources/shaders/dss_mesh.vert`, `resources/shaders/dss_mesh.frag`
- Modify: `CMakeLists.txt:1324-1333`
- Test: `tests/tst_dss_shader_contract.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `kDssRows` from Task 1 (the UBO's `rowFrames` array length must equal it).
- Produces:
  - Compiled `:/shaders/resources/shaders/dss_mesh.vert.qsb` and `.frag.qsb`
  - `constexpr int kDssMeshUboFloats` in `src/gui/DssMeshGeometry.h`

- [ ] **Step 1: Copy the shaders from upstream**

```bash
git -C /Users/j.j.boyd/AetherSDR show 1872028c:resources/shaders/dss_mesh.vert \
  > resources/shaders/dss_mesh.vert
git -C /Users/j.j.boyd/AetherSDR show 1872028c:resources/shaders/dss_mesh.frag \
  > resources/shaders/dss_mesh.frag
```

- [ ] **Step 2: Apply the two permitted tier 1 deviations to the vertex shader**

In `resources/shaders/dss_mesh.vert`, `scrollDistanceRows` stays in the UBO for layout stability but the host always writes `1.0`. Add above the `main()` scroll block:

```glsl
//-KG4VCF [v0.5.3] NereusSDR's producer appends exactly one row per
// waterfall tick, so scrollDistanceRows is always 1.0. The uniform is kept
// so the UBO layout stays byte-comparable with upstream's; the multi-row
// burst path below is therefore exercised only in its distance == 1 form.
// Permitted tier 1 deviation 2 of 2, design doc section 2.3.
```

Leave every other line, including all `(#NNNN)` issue annotations, exactly as upstream has it.

- [ ] **Step 3: Write the failing contract test**

The shaders are GLSL and cannot be unit-tested directly, but the C++ side must agree with them on two numbers or the UBO silently corrupts. Create `tests/tst_dss_shader_contract.cpp`:

```cpp
#include <QTest>
#include <QFile>
#include <QRegularExpression>
#include <QString>

#include "gui/DssMeshGeometry.h"

using namespace NereusSDR;

namespace {

QString shaderSource(const QString& name)
{
    // Tests run from the build dir; the sources live in the source tree.
    QFile f(QStringLiteral(NEREUS_SOURCE_DIR "/resources/shaders/") + name);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(f.readAll());
}

int declaredRowFramesLength(const QString& src)
{
    static const QRegularExpression re(
        QStringLiteral(R"(vec4\s+rowFrames\s*\[\s*(\d+)\s*\])"));
    const auto m = re.match(src);
    return m.hasMatch() ? m.captured(1).toInt() : -1;
}

}  // namespace

class TestDssShaderContract : public QObject {
    Q_OBJECT

private slots:
    void shaders_exist() {
        QVERIFY(!shaderSource(QStringLiteral("dss_mesh.vert")).isEmpty());
        QVERIFY(!shaderSource(QStringLiteral("dss_mesh.frag")).isEmpty());
    }

    // Fixed GLSL array sizes cannot consume a C++ constant. Upstream guards
    // this with a static_assert; we assert it in a test so a change to
    // kDssRows cannot silently overrun the UBO.
    void rowFramesArray_matchesRingRowCount() {
        QCOMPARE(declaredRowFramesLength(
                     shaderSource(QStringLiteral("dss_mesh.vert"))), kDssRows);
        QCOMPARE(declaredRowFramesLength(
                     shaderSource(QStringLiteral("dss_mesh.frag"))), kDssRows);
    }

    // std140 rounds the scalar run up to a vec4 boundary. The host writes
    // explicit padding to match; if this drifts, every vec4 after it shifts
    // and the surface renders garbage with no compile error.
    //
    // This MUST parse the real uniform block out of the shader. Restating
    // kDssMeshUboFloats' own arithmetic here would be a tautology: the two
    // expressions share kDssRows and constant-fold identically, so they can
    // never disagree, and an edit to the GLSL block would sail through. That
    // matters most for the task that writes this UBO, which is the one most
    // likely to change the block.
    void uboFloatCount_matchesStd140Layout() {
        const QString src = shaderSource(QStringLiteral("dss_mesh.vert"));
        QVERIFY(!src.isEmpty());
        static const QRegularExpression blockRe(
            QStringLiteral(R"(layout\(std140[^{]*\{(.*?)\n\};)"),
            QRegularExpression::DotMatchesEverythingOption);
        const auto blockMatch = blockRe.match(src);
        QVERIFY2(blockMatch.hasMatch(), "no std140 uniform block found");
        // Strip comments so a commented-out member is not counted.
        static const QRegularExpression commentRe(QStringLiteral("//[^\n]*"));
        const QString body =
            blockMatch.captured(1).remove(commentRe);

        static const QRegularExpression memberRe(
            QStringLiteral(R"(\b(float|vec4)\s+\w+\s*(?:\[\s*(\d+)\s*\])?\s*;)"));
        int scalars = 0;
        int vec4Slots = 0;
        bool seenVec4 = false;
        auto it = memberRe.globalMatch(body);
        while (it.hasNext()) {
            const auto m = it.next();
            if (m.captured(1) == QLatin1String("float")) {
                // std140 packing here assumes every scalar precedes every
                // vec4. If that ever stops being true the padding maths below
                // is wrong, so fail loudly rather than compute a wrong total.
                QVERIFY2(!seenVec4,
                         "a float is declared after a vec4; std140 padding "
                         "assumption in this test no longer holds");
                ++scalars;
            } else {
                seenVec4 = true;
                const QString count = m.captured(2);
                vec4Slots += count.isEmpty() ? 1 : count.toInt();
            }
        }
        QVERIFY2(scalars > 0 && vec4Slots > 0, "uniform block parse found nothing");
        const int paddedScalars = ((scalars + 3) / 4) * 4;
        QCOMPARE(kDssMeshUboFloats, paddedScalars + vec4Slots * 4);
    }

    // Upstream issue annotations are load-bearing history and no script
    // enforces their survival (design doc section 9.1).
    void upstreamIssueAnnotations_survivedThePort() {
        const QString vert = shaderSource(QStringLiteral("dss_mesh.vert"));
        const QString frag = shaderSource(QStringLiteral("dss_mesh.frag"));
        QVERIFY2(vert.contains(QStringLiteral("rowSpanFactor")),
                 "vertex shader lost the row-span widening path");
        QVERIFY2(frag.contains(QStringLiteral("applySliceShadow")),
                 "fragment shader lost the slice shadow decal path");
    }
};

QTEST_APPLESS_MAIN(TestDssShaderContract)
#include "tst_dss_shader_contract.moc"
```

- [ ] **Step 4: Add `kDssMeshUboFloats` to DssMeshGeometry.h**

Append inside `namespace NereusSDR`, before the closing brace:

```cpp
// UBO float count for dss_mesh.{vert,frag}. std140 rounds the leading
// 22-scalar run up to a vec4 boundary, so the host writes two explicit
// zeros before bgFill. From AetherSDR dss_mesh.vert:14-58 [@1872028c].
inline constexpr int kDssMeshUboFloats =
    24              // 22 scalars padded to a vec4 boundary
    + 4             // bgFill
    + 8 * 4         // shadowBands[8]
    + 8 * 4         // shadowStyles[8]
    + 4             // shadowMeta
    + kDssRows * 4; // rowFrames[kDssRows]
```

- [ ] **Step 5: Wire the shaders and the test's source-dir define**

In `CMakeLists.txt`, extend the existing `qt_add_shaders(NereusSDRObjs "nereus_shaders" ...)` block at line 1324 with:

```cmake
                resources/shaders/dss_mesh.vert
                resources/shaders/dss_mesh.frag
```

In `tests/CMakeLists.txt`, add the test and give it the source path:

```cmake
nereus_add_test(tst_dss_shader_contract)
target_compile_definitions(tst_dss_shader_contract PRIVATE
    NEREUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
```

- [ ] **Step 6: Run and verify**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON \
  && cmake --build build --target tst_dss_shader_contract \
  && ctest --test-dir build -R '^tst_dss_shader_contract$' --output-on-failure
```

Expected: PASS, 4 test functions. A failure in `rowFramesArray_matchesRingRowCount` means `kDssRows` was changed without updating both shaders.

- [ ] **Step 7: Commit**

```bash
git add resources/shaders/dss_mesh.vert resources/shaders/dss_mesh.frag \
        src/gui/DssMeshGeometry.h tests/tst_dss_shader_contract.cpp \
        tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat(dss): add the 3DSS mesh shaders and their host contract

Vertex and fragment shaders lifted from AetherSDR [@1872028c], with the
scroll-distance deviation marked in place. All upstream issue annotations
preserved verbatim.

GLSL cannot consume a C++ constant for a fixed array size, so the two
numbers the host and shader must agree on, the rowFrames array length and
the std140 UBO float count, are pinned by a test instead. Upstream guards
the first with a static_assert; a test covers both and reads the actual
shader source rather than a copy of it."
```

---

## Task 6: Render mode, control state and the row tee

**Files:**
- Modify: `src/gui/SpectrumWidget.h`, `src/gui/SpectrumWidget.cpp`
- Test: `tests/tst_dss_row_tee.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `DssRenderer::pushRowWithWide`, `newestWideBandwidthMhz` (Task 4); `dssShapeForAngle` (Task 1).
- Produces:
  - `enum class NereusSDR::SpectrumRenderMode : int { Mode2D = 0, Mode3D, Count }`
  - `SpectrumWidget::setSpectrumRenderMode(int)` / `spectrumRenderMode() const`
  - `setDssFloorDepth(int)` / `dssFloorDepth() const`
  - `setDssGain(int)` / `dssGain() const`
  - `setDssRowSpan(int)` / `dssRowSpan() const`
  - `setDssAngle(int)` / `dssAngle() const`
  - `setThreeDSliceDepth(bool)` / `threeDSliceDepth() const`
  - `DssShape dssShape() const`
  - `int dssRowsPushedForTest() const`

- [ ] **Step 1: Write the failing test**

The tee's placement is the whole point of this test. Create `tests/tst_dss_row_tee.cpp`:

```cpp
#include <QTest>
#include <QVector>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

namespace {

QVector<float> row(int n, float dbm) { return QVector<float>(n, dbm); }

}  // namespace

class TestDssRowTee : public QObject {
    Q_OBJECT

private slots:
    void defaultMode_is2D() {
        SpectrumWidget w;
        QCOMPARE(w.spectrumRenderMode(),
                 static_cast<int>(SpectrumRenderMode::Mode2D));
    }

    void controlDefaults_matchUpstream() {
        SpectrumWidget w;
        QCOMPARE(w.dssFloorDepth(), 6);
        QCOMPARE(w.dssGain(),      70);
        QCOMPARE(w.dssRowSpan(),  100);
        QCOMPARE(w.dssAngle(),     50);   // NereusSDR-original
        QCOMPARE(w.threeDSliceDepth(), false);
    }

    void angle50_yieldsUpstreamShape() {
        SpectrumWidget w;
        const DssShape s = w.dssShape();
        QVERIFY(std::abs(s.backWidthFrac - 0.60f) < 1e-6f);
        QVERIFY(std::abs(s.depthSpanFrac - 0.58f) < 1e-6f);
    }

    void controlsAreClamped() {
        SpectrumWidget w;
        w.setDssFloorDepth(999);  QCOMPARE(w.dssFloorDepth(), 24);
        w.setDssFloorDepth(-5);   QCOMPARE(w.dssFloorDepth(), 0);
        w.setDssGain(999);        QCOMPARE(w.dssGain(),      100);
        w.setDssRowSpan(-1);      QCOMPARE(w.dssRowSpan(),     0);
        w.setDssAngle(999);       QCOMPARE(w.dssAngle(),     100);
    }

    // The tee must sit DOWNSTREAM of the stop-on-TX gate. Teeing at the
    // ticker callback instead would let the 3D stack keep scrolling while
    // the waterfall is frozen, desyncing the two panes on every over.
    void stopOnTx_freezesBothPanesTogether() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setWaterfallStopOnTx(true);
        w.setTxActiveForTest(false);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        const int beforeTx = w.dssRowsPushedForTest();
        QCOMPARE(beforeTx, 1);

        w.setTxActiveForTest(true);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssRowsPushedForTest(), beforeTx);   // frozen with the waterfall

        w.setTxActiveForTest(false);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssRowsPushedForTest(), beforeTx + 1);
    }

    void stopOnTxDisabled_keepsBothPanesRunning() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setWaterfallStopOnTx(false);
        w.setTxActiveForTest(true);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssRowsPushedForTest(), 1);
    }

    // Leaving 3D must clear the ring, so re-entering does not display a stack
    // of rows captured at a frequency the operator has since left. And
    // re-asserting the SAME mode must NOT clear, or an idempotent UI refresh
    // would silently wipe live history.
    //
    // Both behaviours are load-bearing for the rendering task that reads this
    // ring. Without this test, deleting the clear branch entirely leaves every
    // other test in this file green.
    void leaving3D_clearsTheRing_butSameModeDoesNot() {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.pushWaterfallRowForTest(row(768, -130.0f));
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssRowsPushedForTest(), 2);   // precondition: ring is dirty

        // Same mode again: no-op guard must fire, history survives.
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        QCOMPARE(w.dssRowsPushedForTest(), 2);

        // Genuinely leaving 3D: ring clears.
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        QCOMPARE(w.dssRowsPushedForTest(), 0);
    }

    // In 2D the ring must not be fed at all: a 2D pan allocates and does no
    // DSS work (design doc section 3.4).
    void mode2D_doesNotFeedTheRing() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssRowsPushedForTest(), 0);
    }
};

QTEST_MAIN(TestDssRowTee)
#include "tst_dss_row_tee.moc"
```

- [ ] **Step 2: Register and run to confirm it fails**

Add `nereus_add_test(tst_dss_row_tee)` to `tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_dss_row_tee
```

Expected: FAIL at compile, `no member named 'spectrumRenderMode'`.

- [ ] **Step 3: Add the enum and state to SpectrumWidget.h**

Next to the existing `WfColorScheme` enum (around line 203):

```cpp
// Spectrum render mode for the panadapter surface.
// From AetherSDR SpectrumWidget.h:78-82 [@1872028c].
enum class SpectrumRenderMode : int {
    Mode2D = 0,    // FFT trace + scrolling waterfall (classic)
    Mode3D,        // 3DSS perspective stacked-trace surface
    Count          // sentinel
};
```

In the public section, add the accessors; in the private section add the state and the renderer. Include `"gui/DssRenderer.h"` at the top.

```cpp
    // ── 3DSS stacked-trace mode ───────────────────────────────────────────
    void setSpectrumRenderMode(int mode);
    int  spectrumRenderMode() const { return static_cast<int>(m_spectrumRenderMode); }
    void setDssFloorDepth(int dB);
    int  dssFloorDepth() const { return m_dssFloorDepth; }
    void setDssGain(int pct);
    int  dssGain() const { return m_dssGain; }
    void setDssRowSpan(int pct);
    int  dssRowSpan() const { return m_dssRowSpan; }
    void setDssAngle(int pct);
    int  dssAngle() const { return m_dssAngle; }
    void setThreeDSliceDepth(bool on);
    bool threeDSliceDepth() const { return m_threeDSliceDepth; }
    DssShape dssShape() const { return dssShapeForAngle(m_dssAngle); }

    // Test seams. pushWaterfallRow() is private and normally driven by the
    // WaterfallTicker thread; these let the row-tee placement be proven
    // without standing up a ticker or a QRhi context.
    void pushWaterfallRowForTest(const QVector<float>& wfPixelsDbm) {
        pushWaterfallRow(wfPixelsDbm);
    }
    void setTxActiveForTest(bool on) { m_txActiveForTest = on; }
    int  dssRowsPushedForTest() const { return m_dssRowsPushed; }
```

Private members:

```cpp
    SpectrumRenderMode m_spectrumRenderMode{SpectrumRenderMode::Mode2D};
    // 3DSS floor depth: how far below the measured noise floor the surface
    // baseline sits, in dB. Persisted per band (design doc section 6.3).
    int  m_dssFloorDepth{6};
    int  m_dssGain{70};        // colour gamma 0-100
    int  m_dssRowSpan{100};    // wedge close-in 0-100
    //-KG4VCF [v0.5.3] NereusSDR-original: upstream renders at one fixed
    // viewing angle. 50 reproduces its geometry exactly.
    int  m_dssAngle{50};
    bool m_threeDSliceDepth{false};
    DssRenderer m_dss;
    int  m_dssRowsPushed{0};
    bool m_txActiveForTest{false};
```

- [ ] **Step 4: Add the setters and the tee to SpectrumWidget.cpp**

Setters follow the existing `scheduleSettingsSave()` pattern in this file:

```cpp
void SpectrumWidget::setSpectrumRenderMode(int mode)
{
    const SpectrumRenderMode next =
        (mode == static_cast<int>(SpectrumRenderMode::Mode3D))
            ? SpectrumRenderMode::Mode3D
            : SpectrumRenderMode::Mode2D;
    if (m_spectrumRenderMode == next) { return; }
    m_spectrumRenderMode = next;
    if (next == SpectrumRenderMode::Mode2D) {
        // Leaving 3D: drop the ring so re-entering starts clean rather than
        // showing a stack of rows captured at a frequency we have since left.
        m_dss.clear();
        m_dssRowsPushed = 0;
    }
    m_dss.invalidate();
    markOverlayDirty();
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setDssFloorDepth(int dB)
{
    const int v = std::clamp(dB, 0, 24);
    if (m_dssFloorDepth == v) { return; }
    m_dssFloorDepth = v;
    m_dss.invalidate();
    markOverlayDirty();
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setDssGain(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    if (m_dssGain == v) { return; }
    m_dssGain = v;
    m_dss.invalidate();
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setDssRowSpan(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    if (m_dssRowSpan == v) { return; }
    m_dssRowSpan = v;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setDssAngle(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    if (m_dssAngle == v) { return; }
    m_dssAngle = v;
    // The mesh column count is a function of the shape, so a large enough
    // angle change invalidates the vertex buffers. Task 7 reallocates them.
    m_dssMeshNeedsResize = true;
    m_dss.invalidate();
    markOverlayDirty();
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setThreeDSliceDepth(bool on)
{
    if (m_threeDSliceDepth == on) { return; }
    m_threeDSliceDepth = on;
    scheduleSettingsSave();
    update();
}
```

Then the tee. In `pushWaterfallRow()` at `src/gui/SpectrumWidget.cpp:4722`, immediately **after** the stop-on-TX early return at `:4728-4731` and before the cadence bookkeeping:

```cpp
    // 3DSS: feed the stacked-trace ring from the same call, downstream of the
    // stop-on-TX gate above, so the perspective stack and the flat waterfall
    // beneath it advance and freeze in lockstep. Teeing at the WaterfallTicker
    // callback instead would sit upstream of that gate and let the 3D surface
    // keep scrolling through an over.
    if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
        pushDssRow(wfPixelsDbm);
    }
```

Also extend the stop-on-TX condition so the test seam can drive it:

```cpp
    // Task 2.8: Stop-on-TX -- skip if TX active and feature enabled.
    if (m_wfStopOnTx && (m_activePeakHold.txActive() || m_txActiveForTest)) {
        return;
    }
```

And the private helper, which Task 8 will extend to fill the wide channel:

```cpp
void SpectrumWidget::pushDssRow(const QVector<float>& wfPixelsDbm)
{
    const double centerMhz    = m_centerHz    / 1.0e6;
    const double bandwidthMhz = m_bandwidthHz / 1.0e6;
    m_dss.pushRow(wfPixelsDbm, centerMhz, bandwidthMhz);
    ++m_dssRowsPushed;
}
```

Declare `void pushDssRow(const QVector<float>& wfPixelsDbm);` and `bool m_dssMeshNeedsResize{false};` in the private section of the header.

- [ ] **Step 5: Run to verify it passes**

```bash
cmake --build build --target tst_dss_row_tee && ctest --test-dir build -R '^tst_dss_row_tee$' --output-on-failure
```

Expected: PASS, 7 test functions.

- [ ] **Step 6: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp \
        tests/tst_dss_row_tee.cpp tests/CMakeLists.txt
git commit -m "feat(dss): add the 3D render mode and tee rows past the TX gate

SpectrumRenderMode plus the five control values and the DssRenderer
instance. Defaults match upstream exactly except the NereusSDR-original
3D Angle, which defaults to 50 and reproduces upstream's fixed geometry.

The row tee sits inside pushWaterfallRow downstream of the stop-on-TX
early return, not at the WaterfallTicker callback. That placement is the
point: teeing upstream of the gate would leave the 3D stack scrolling
while the waterfall is frozen, desyncing the two panes on every over. A
test drives TX both ways to pin it.

Leaving 3D clears the ring so re-entering does not show a stack captured
at a frequency the operator has since left."
```

---

## Task 7: GPU resources and the mesh pass

**Files:**
- Modify: `src/gui/SpectrumWidget.h`, `src/gui/SpectrumWidget.cpp`

**Interfaces:**
- Consumes: `dssMeshColsFor`, `dssBuildMeshVertices`, `dssFillVerticesPerRow`, `dssLineVerticesPerRow`, `kDssMeshUboFloats` (Tasks 3, 5); `DssRenderer` accessors (Task 4).
- Produces:
  - `bool SpectrumWidget::initDssMeshPipeline()`
  - `void SpectrumWidget::rebuildDssMeshIfNeeded(QRhiResourceUpdateBatch*)`
  - `void SpectrumWidget::uploadDssHeightRows(QRhiResourceUpdateBatch*)`
  - `bool SpectrumWidget::dssMeshReady() const`
  - Member `int m_dssMeshCols`

- [ ] **Step 1: Read upstream's pipeline setup**

```bash
git -C /Users/j.j.boyd/AetherSDR show 1872028c:src/gui/SpectrumWidget.cpp | sed -n '12787,12905p'
```

Note the `RGBA16F` support probe that disables the mesh and falls back to CPU, the `Nearest` height sampler with `Linear` palette sampler, and the SRB binding numbers (0 UBO both stages, 1 height vertex-stage, 2 palette fragment-stage) which `dss_mesh.vert:63` and `dss_mesh.frag:47` depend on.

- [ ] **Step 2: Add the GPU members**

In the `#ifdef NEREUS_GPU_SPECTRUM` block of `SpectrumWidget.h`:

```cpp
    // ---- 3DSS mesh GPU resources ----
    bool initDssMeshPipeline();
    void rebuildDssMeshIfNeeded(QRhiResourceUpdateBatch* batch);
    void uploadDssHeightRows(QRhiResourceUpdateBatch* batch);
    void uploadDssPaletteLut(QRhiResourceUpdateBatch* batch);
    void writeDssMeshUbo(QRhiResourceUpdateBatch* batch,
                         const QRect& specRect, float dpr);
    bool dssMeshReady() const { return m_dssMeshReady; }

    QRhiGraphicsPipeline*       m_dssFillPipeline{nullptr};
    QRhiGraphicsPipeline*       m_dssLinePipeline{nullptr};
    QRhiShaderResourceBindings* m_dssSrb{nullptr};
    QRhiBuffer*                 m_dssMeshVbo{nullptr};
    QRhiBuffer*                 m_dssMeshLineVbo{nullptr};
    QRhiBuffer*                 m_dssUbo{nullptr};
    QRhiTexture*                m_dssHeightTex{nullptr};
    QRhiTexture*                m_dssPaletteTex{nullptr};
    QRhiSampler*                m_dssHeightSampler{nullptr};
    QRhiSampler*                m_dssPaletteSampler{nullptr};
    bool    m_dssMeshReady{false};
    // False until rebuildDssMeshIfNeeded() has actually pushed vertices
    // for m_dssMeshCols. Guards the first-upload case that a plain dirty
    // flag misses, and is reset on teardown so a rebuilt pipeline uploads
    // again.
    bool    m_dssMeshUploaded{false};
    int     m_dssMeshCols{0};
    quint64 m_dssLutToken{~0ull};
    quint64 m_dssUploadedRowGeneration{~0ull};
    int     m_dssLastUploadedHead{-1};
```

- [ ] **Step 3: Implement initDssMeshPipeline()**

Follow upstream's structure. The one divergence: size the VBOs from `dssMeshColsFor(dssShape())` rather than a constant, and record the count in `m_dssMeshCols`.

```cpp
bool SpectrumWidget::initDssMeshPipeline()
{
    QRhi* r = rhi();
    m_dssMeshReady = false;
    if (!r) { return false; }

    // R stores dBm and G stores captured-frequency coverage. The second
    // channel keeps zoom-created floor spans colour-stable without hiding
    // their lines. From AetherSDR SpectrumWidget.cpp:12793-12798 [@1872028c].
    if (!r->isTextureFormatSupported(QRhiTexture::RGBA16F, {})) {
        qCWarning(lcSpectrum) << "SpectrumWidget: RGBA16F unsupported -- "
                            "stacked-trace mesh disabled (CPU fallback)";
        return false;
    }

    QShader vs = loadShader(QStringLiteral(
        ":/shaders/resources/shaders/dss_mesh.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(
        ":/shaders/resources/shaders/dss_mesh.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh shader load failed -- "
                            "stacked-trace mesh disabled";
        return false;
    }

    m_dssMeshCols = dssMeshColsFor(dssShape());
    const int fillVerts = kDssVisibleRows * dssFillVerticesPerRow(m_dssMeshCols);
    const int lineVerts = kDssVisibleRows * dssLineVerticesPerRow(m_dssMeshCols);

    m_dssMeshVbo = r->newBuffer(QRhiBuffer::Immutable,
                                QRhiBuffer::VertexBuffer,
                                fillVerts * 3 * sizeof(float));
    m_dssMeshLineVbo = r->newBuffer(QRhiBuffer::Immutable,
                                    QRhiBuffer::VertexBuffer,
                                    lineVerts * 3 * sizeof(float));
    m_dssUbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                            kDssMeshUboFloats * sizeof(float));
    if (!m_dssMeshVbo->create() || !m_dssMeshLineVbo->create()
        || !m_dssUbo->create()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh buffer create failed";
        return false;
    }

    m_dssHeightTex = r->newTexture(QRhiTexture::RGBA16F,
                                   QSize(m_dss.cols(), m_dss.rows()));
    m_dssPaletteTex = r->newTexture(QRhiTexture::RGBA8, QSize(256, 1));
    if (!m_dssHeightTex->create() || !m_dssPaletteTex->create()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh texture create failed";
        return false;
    }

    // Height sampled in the vertex stage; Nearest is enough because the mesh
    // grid is never sparser than the texture -- the column count is sized so
    // that even at the widest rowSpanFactor the on-screen columns still cover
    // every texel, so no bin can fall between two samples. Palette is Linear
    // for a smooth floor->peak gradient.
    m_dssHeightSampler = r->newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_dssPaletteSampler = r->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!m_dssHeightSampler->create() || !m_dssPaletteSampler->create()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh sampler create failed";
        return false;
    }

    m_dssSrb = r->newShaderResourceBindings();
    m_dssSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
            QRhiShaderResourceBinding::VertexStage
                | QRhiShaderResourceBinding::FragmentStage, m_dssUbo),
        QRhiShaderResourceBinding::sampledTexture(1,
            QRhiShaderResourceBinding::VertexStage,
            m_dssHeightTex, m_dssHeightSampler),
        QRhiShaderResourceBinding::sampledTexture(2,
            QRhiShaderResourceBinding::FragmentStage,
            m_dssPaletteTex, m_dssPaletteSampler),
    });
    if (!m_dssSrb->create()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh SRB create failed";
        return false;
    }

    QRhiVertexInputLayout layout;
    layout.setBindings({{3 * sizeof(float)}});
    layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable   = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    const auto makePipeline = [&]() -> QRhiGraphicsPipeline* {
        QRhiGraphicsPipeline* p = r->newGraphicsPipeline();
        p->setShaderStages({{QRhiShaderStage::Vertex, vs},
                            {QRhiShaderStage::Fragment, fs}});
        p->setVertexInputLayout(layout);
        p->setTopology(QRhiGraphicsPipeline::Triangles);
        p->setShaderResourceBindings(m_dssSrb);
        p->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        p->setTargetBlends({blend});
        return p;
    };
    m_dssFillPipeline = makePipeline();
    m_dssLinePipeline = makePipeline();
    if (!m_dssFillPipeline->create() || !m_dssLinePipeline->create()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh pipeline create failed";
        return false;
    }

    m_dssMeshReady = true;
    return true;
}
```

Call it from `initialize()` after `initSpectrumPipeline()`, and delete every resource in the existing GPU teardown path alongside the waterfall and overlay resources.

- [ ] **Step 4: Implement the dynamic mesh rebuild**

```cpp
void SpectrumWidget::rebuildDssMeshIfNeeded(QRhiResourceUpdateBatch* batch)
{
    if (!m_dssMeshReady || !batch) { return; }
    const int wanted = dssMeshColsFor(dssShape());
    // Skip only when the geometry currently ON THE GPU is already right.
    //
    // The condition is deliberately "have we uploaded, and is the column
    // count still the one we uploaded for", NOT a dirty flag plus a size
    // check. initDssMeshPipeline() sets m_dssMeshCols and creates a
    // correctly sized but EMPTY buffer, so a guard keyed on
    // "wanted == m_dssMeshCols && vbo->size() > 0" is satisfied on the very
    // first call and skips the only code path that ever uploads vertices.
    // The mesh would then draw undefined GPU memory forever.
    //
    // Keying on m_dssMeshUploaded also fixes the converse waste: the vertex
    // data is a pure function of the column count, so an angle change that
    // does not cross a dssMeshColsFor boundary needs no work at all. Without
    // this, a slider drag re-uploads tens of MiB of identical data per tick.
    if (m_dssMeshUploaded && wanted == m_dssMeshCols) {
        return;
    }

    QVector<float> fill;
    QVector<float> line;
    dssBuildMeshVertices(wanted, fill, line);
    const quint32 fillBytes = quint32(fill.size()) * sizeof(float);
    const quint32 lineBytes = quint32(line.size()) * sizeof(float);

    // Reallocate only when the column count actually moved. Sizing for the
    // worst-case angle unconditionally would cost 57.8 MiB per panadapter
    // instead of 33.8 (design doc section 5.5).
    if (wanted != m_dssMeshCols) {
        m_dssMeshVbo->destroy();
        m_dssMeshVbo->setSize(fillBytes);
        m_dssMeshLineVbo->destroy();
        m_dssMeshLineVbo->setSize(lineBytes);
        if (!m_dssMeshVbo->create() || !m_dssMeshLineVbo->create()) {
            qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh resize failed";
            m_dssMeshReady = false;
            return;
        }
        m_dssMeshCols = wanted;
    }
    batch->uploadStaticBuffer(m_dssMeshVbo, 0, fillBytes, fill.constData());
    batch->uploadStaticBuffer(m_dssMeshLineVbo, 0, lineBytes, line.constData());
    m_dssMeshUploaded = true;
}
```

- [ ] **Step 5: Implement the height-texture upload**

Only the newest row changes per frame, so upload one row unless the generation jumped.

```cpp
void SpectrumWidget::uploadDssHeightRows(QRhiResourceUpdateBatch* batch)
{
    if (!m_dssMeshReady || !batch || m_dss.rowCount() == 0) { return; }
    if (m_dss.rowGeneration() == m_dssUploadedRowGeneration) { return; }

    const int cols = m_dss.cols();
    const auto packRow = [&](int ring, QVector<qfloat16>& out) {
        const float*  exact    = m_dss.rowDataRing(ring);
        const quint8* exactCov = m_dss.rowCoverageRing(ring);
        const float*  wide     = m_dss.rowWideDataRing(ring);
        const quint8* wideCov  = m_dss.rowWideCoverageRing(ring);
        out.resize(cols * 4);
        for (int c = 0; c < cols; ++c) {
            out[c * 4 + 0] = qfloat16(exact[c]);
            out[c * 4 + 1] = qfloat16(exactCov[c] ? 1.0f : 0.0f);
            out[c * 4 + 2] = qfloat16(wide[c]);
            out[c * 4 + 3] = qfloat16(wideCov[c] ? 1.0f : 0.0f);
        }
    };

    QVector<qfloat16> packed;
    const int head = m_dss.headRing();
    const bool fullUpload =
        m_dssLastUploadedHead < 0
        || m_dss.rowCount() < kDssRows;
    if (fullUpload) {
        for (int ring = 0; ring < m_dss.rows(); ++ring) {
            packRow(ring, packed);
            QRhiTextureSubresourceUploadDescription desc(
                packed.constData(), packed.size() * sizeof(qfloat16));
            desc.setSourceSize(QSize(cols, 1));
            desc.setDestinationTopLeft(QPoint(0, ring));
            batch->uploadTexture(m_dssHeightTex,
                                 QRhiTextureUploadEntry(0, 0, desc));
        }
    } else {
        packRow(head, packed);
        QRhiTextureSubresourceUploadDescription desc(
            packed.constData(), packed.size() * sizeof(qfloat16));
        desc.setSourceSize(QSize(cols, 1));
        desc.setDestinationTopLeft(QPoint(0, head));
        batch->uploadTexture(m_dssHeightTex,
                             QRhiTextureUploadEntry(0, 0, desc));
    }
    m_dssLastUploadedHead = head;
    m_dssUploadedRowGeneration = m_dss.rowGeneration();
}
```

- [ ] **Step 6: Slot the pass into renderGpuFrame()**

In `src/gui/SpectrumWidget.cpp` around line 8548, wrap the existing FFT draw block so 3D takes its slot:

```cpp
    // Spectrum region: the 3DSS surface, or the classic FFT trace.
    // 3DSS replaces ONLY the spectrum trace -- the waterfall, divider, freq
    // scale, overlays and scales below run identically in both modes.
    // From AetherSDR SpectrumWidget.cpp:14776-14779 [@1872028c].
    const bool is3D =
        (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) && m_dssMeshReady;

    if (is3D && m_dss.rowCount() > 0) {
        const float specVpX = static_cast<float>(specRect.x()) * dpr;
        const float specVpY = static_cast<float>(h - specRect.bottom() - 1) * dpr;
        const float specVpW = static_cast<float>(specRect.width()) * dpr;
        const float specVpH = static_cast<float>(specRect.height()) * dpr;
        const QRhiViewport specVp(specVpX, specVpY, specVpW, specVpH);
        const int rows = m_dss.visibleRowCount();

        cb->setGraphicsPipeline(m_dssFillPipeline);
        cb->setShaderResources(m_dssSrb);
        cb->setViewport(specVp);
        const QRhiCommandBuffer::VertexInput fillVbuf(m_dssMeshVbo, 0);
        cb->setVertexInput(0, 1, &fillVbuf);
        cb->draw(rows * dssFillVerticesPerRow(m_dssMeshCols));

        cb->setGraphicsPipeline(m_dssLinePipeline);
        cb->setShaderResources(m_dssSrb);
        cb->setViewport(specVp);
        const QRhiCommandBuffer::VertexInput lineVbuf(m_dssMeshLineVbo, 0);
        cb->setVertexInput(0, 1, &lineVbuf);
        cb->draw(rows * dssLineVerticesPerRow(m_dssMeshCols));
    } else if (!is3D && m_fftFillPipeline && m_fftLinePipeline
               && m_visibleBinCount > 0) {
        // ... existing 2D FFT draw block, unchanged ...
    }
```

Before `cb->beginPass(...)`, add the three resource updates alongside the existing ones:

```cpp
    if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
        rebuildDssMeshIfNeeded(batch);
        uploadDssPaletteLut(batch);
        uploadDssHeightRows(batch);
        writeDssMeshUbo(batch, specRect, dpr);
    }
```

- [ ] **Step 6b: Port the OpenGL outline-pipeline dispatch (Linux correctness)**

Upstream does not bind a dedicated ribbon pipeline on the OpenGL backend. Its
reason, verbatim from `SpectrumPreviewLogic.h:16-18 [@1872028c]`:

> "QRhi's OpenGLES2 backend, which covers desktop GL as well as GLES, reuses the
> fill program for ribbon outlines: live probes showed flat/stale outlines
> with a separate, identically configured program."

This matters here specifically because `SpectrumWidget`'s constructor sets
`QRhiWidget::Api::Metal` under `Q_OS_MAC` and `Direct3D11` under `Q_OS_WIN`
but has **no Linux branch**, so Linux takes Qt's default and lands on OpenGL.
NereusSDR ships Linux AppImages for two architectures, so omitting this would
render the 3D ridge outlines flat or stale on every Linux install.

Add to `src/gui/DssMeshGeometry.h` (header-only and QRhi-free, so the
selection stays unit-testable without a graphics device, which is upstream's
stated reason for its shape):

```cpp
// ─── Outline pipeline selection ─────────────────────────────────────────
// From AetherSDR src/gui/SpectrumPreviewLogic.h:11-56 [@1872028c].

enum class DssOutlinePipelineMode {
    DedicatedRibbonPipeline,
    SharedFillPipeline,
};

// QRhi's OpenGLES2 backend, which covers desktop GL as well as GLES, reuses the
// fill program for ribbon outlines: live probes showed flat/stale outlines
// with a separate, identically configured program.
constexpr DssOutlinePipelineMode dssOutlinePipelineModeForBackend(
    bool openGlEs2Backend)
{
    return openGlEs2Backend
        ? DssOutlinePipelineMode::SharedFillPipeline
        : DssOutlinePipelineMode::DedicatedRibbonPipeline;
}

// The pipeline the outline draw binds, given the mode above. Templated on the
// pipeline type purely so the selection stays testable without a QRhi device:
// SpectrumWidget instantiates it with QRhiGraphicsPipeline*. Keeping the
// selection here rather than as a ternary at the draw site is what lets
// the unit test pin the mapping the renderer actually uses.
// dedicatedPipeline is null on OpenGL — never created — so the shared-fill
// answer must not depend on it.
template <typename PipelineT>
constexpr PipelineT* dssOutlinePipelineFor(DssOutlinePipelineMode mode,
                                           PipelineT* fillPipeline,
                                           PipelineT* dedicatedPipeline)
{
    return mode == DssOutlinePipelineMode::SharedFillPipeline
        ? fillPipeline
        : dedicatedPipeline;
}
```

Wire it in `SpectrumWidget`: add a member
`DssOutlinePipelineMode m_dssOutlinePipelineMode{DssOutlinePipelineMode::DedicatedRibbonPipeline};`,
set it at the top of `initDssMeshPipeline()` from
`dssOutlinePipelineModeForBackend(r->backend() == QRhi::OpenGLES2)`, skip
creating `m_dssLinePipeline` entirely when the mode is `SharedFillPipeline`,
and select at the outline draw site with `dssOutlinePipelineFor(...)`.

Extend `tests/tst_dss_mesh_geometry.cpp`:

```cpp
    // Linux takes Qt's OpenGL default (SpectrumWidget sets Metal only under
    // Q_OS_MAC and D3D11 only under Q_OS_WIN), and QRhi's OpenGL backend
    // renders flat or stale ridge outlines from a separate pipeline. So the
    // outline draw must share the fill pipeline there and only there.
    void outlinePipeline_sharesFillOnOpenGlOnly() {
        QCOMPARE(dssOutlinePipelineModeForBackend(true),
                 DssOutlinePipelineMode::SharedFillPipeline);
        QCOMPARE(dssOutlinePipelineModeForBackend(false),
                 DssOutlinePipelineMode::DedicatedRibbonPipeline);
    }

    // On OpenGL the dedicated pipeline is never created, so the selector must
    // return the fill pipeline without dereferencing the null one.
    void outlinePipeline_selectsWithoutTouchingTheNullPipeline() {
        int fill = 1;
        int dedicated = 2;
        QCOMPARE(dssOutlinePipelineFor(
                     DssOutlinePipelineMode::SharedFillPipeline,
                     &fill, static_cast<int*>(nullptr)), &fill);
        QCOMPARE(dssOutlinePipelineFor(
                     DssOutlinePipelineMode::DedicatedRibbonPipeline,
                     &fill, &dedicated), &dedicated);
    }
```

- [ ] **Step 7: Build and confirm no regression**

```bash
cmake --build build -j$(sysctl -n hw.ncpu) && cmake --build build --target tests_gui && ctest --test-dir build -L gui --output-on-failure
```

Expected: build clean, all `gui`-labelled tests pass. `writeDssMeshUbo` and `uploadDssPaletteLut` are stubbed empty at this point; Tasks 8 and 9 fill them.

- [ ] **Step 8: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp
git commit -m "feat(dss): create the 3DSS GPU resources and mesh pass

Pipeline, SRB, RGBA16F ring texture and palette texture following
AetherSDR SpectrumWidget.cpp:12787-12905 [@1872028c], including the
format probe that disables the mesh and falls back to CPU.

The mesh pass takes the spectrum-trace pass's slot inside renderGpuFrame
scissored to specRect, so the waterfall, frequency scale and every
overlay downstream are untouched.

Divergence from upstream: the vertex buffers are sized from the live
perspective shape and reallocated only when the angle actually changes
the column count. Upstream can size once because its angle is fixed."
```

---

## Task 8: Palette LUT decoupled from the waterfall knobs

**Files:**
- Modify: `src/gui/SpectrumWidget.h`, `src/gui/SpectrumWidget.cpp`
- Test: `tests/tst_dss_palette.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `wfSchemeStops`, `WfColorScheme` (existing).
- Produces:
  - `QRgb NereusSDR::interpolateWfGradient(float t, const WfGradientStop* stops, int count)`
  - `QRgb SpectrumWidget::dssStrengthToRgb(float s) const`
  - `quint64 SpectrumWidget::dssPaletteToken() const`

- [ ] **Step 1: Write the failing test**

The decoupling is the property under test, not just the gamma. Create `tests/tst_dss_palette.cpp`:

```cpp
#include <QTest>
#include <cmath>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestDssPalette : public QObject {
    Q_OBJECT

private slots:
    // Gamma identities from AetherSDR dssStrengthToRgb [@1872028c]:
    // gain 50 -> 1.0 (linear), gain 100 -> 0.25, gain 0 -> 4.
    void gain50_isLinear() {
        SpectrumWidget w;
        w.setDssGain(50);
        w.setWfColorScheme(static_cast<int>(WfColorScheme::Grayscale));
        // Grayscale runs black to white, so a linear ramp puts mid-strength
        // at mid-grey.
        const QRgb mid = w.dssStrengthToRgb(0.5f);
        QVERIFY(std::abs(qRed(mid) - 128) <= 4);
    }

    void gain100_liftsTowardTheFloor() {
        SpectrumWidget w;
        w.setWfColorScheme(static_cast<int>(WfColorScheme::Grayscale));
        w.setDssGain(50);
        const int linear = qRed(w.dssStrengthToRgb(0.25f));
        w.setDssGain(100);
        const int lifted = qRed(w.dssStrengthToRgb(0.25f));
        QVERIFY2(lifted > linear,
                 "gain 100 must brighten weak signals, not dim them");
    }

    void gain0_coloursOnlyTheStrongest() {
        SpectrumWidget w;
        w.setWfColorScheme(static_cast<int>(WfColorScheme::Grayscale));
        w.setDssGain(50);
        const int linear = qRed(w.dssStrengthToRgb(0.5f));
        w.setDssGain(0);
        const int suppressed = qRed(w.dssStrengthToRgb(0.5f));
        QVERIFY(suppressed < linear);
    }

    void strengthIsClamped() {
        SpectrumWidget w;
        const QRgb lo = w.dssStrengthToRgb(-1.0f);
        const QRgb hi = w.dssStrengthToRgb(2.0f);
        QCOMPARE(lo, w.dssStrengthToRgb(0.0f));
        QCOMPARE(hi, w.dssStrengthToRgb(1.0f));
    }

    // THE point of this task. Upstream bypasses dbmToRgb's waterfall gain
    // and black-level window on purpose (uploadDssPaletteLut
    // SpectrumWidget.cpp:12578-12586 [@1872028c]); otherwise the waterfall
    // sliders would silently reshape the 3D surface.
    void waterfallKnobs_doNotMove3DColours() {
        SpectrumWidget w;
        w.setWfColorScheme(static_cast<int>(WfColorScheme::Default));
        w.setDssGain(70);
        const QRgb before = w.dssStrengthToRgb(0.4f);
        w.setWfColorGain(10);
        w.setWfBlackLevel(10);
        const QRgb afterLow = w.dssStrengthToRgb(0.4f);
        w.setWfColorGain(120);
        w.setWfBlackLevel(120);
        const QRgb afterHigh = w.dssStrengthToRgb(0.4f);
        QCOMPARE(afterLow,  before);
        QCOMPARE(afterHigh, before);
    }

    // The scheme stops ARE shared, so all eight palettes work in 3D.
    void everyScheme_producesDistinctColours() {
        SpectrumWidget w;
        w.setDssGain(50);
        QVector<QRgb> seen;
        for (int i = 0; i < static_cast<int>(WfColorScheme::Count); ++i) {
            w.setWfColorScheme(i);
            seen.append(w.dssStrengthToRgb(0.7f));
        }
        QCOMPARE(seen.size(), static_cast<int>(WfColorScheme::Count));
        // At least four of the six must differ from the first.
        int distinct = 0;
        for (int i = 1; i < seen.size(); ++i) {
            if (seen[i] != seen[0]) { ++distinct; }
        }
        QVERIFY(distinct >= 4);
    }

    // The LUT re-bakes only on a real change, never per frame.
    void paletteToken_foldsSchemeAndGainOnly() {
        SpectrumWidget w;
        w.setWfColorScheme(static_cast<int>(WfColorScheme::Fire));
        w.setDssGain(70);
        const quint64 base = w.dssPaletteToken();
        QCOMPARE(w.dssPaletteToken(), base);
        w.setDssGain(71);
        QVERIFY(w.dssPaletteToken() != base);
        w.setDssGain(70);
        QCOMPARE(w.dssPaletteToken(), base);
        w.setWfColorScheme(static_cast<int>(WfColorScheme::Plasma));
        QVERIFY(w.dssPaletteToken() != base);
    }
};

QTEST_MAIN(TestDssPalette)
#include "tst_dss_palette.moc"
```

- [ ] **Step 2: Register and run to confirm it fails**

Add `nereus_add_test(tst_dss_palette)` to `tests/CMakeLists.txt`.

```bash
cmake --build build --target tst_dss_palette
```

Expected: FAIL, `no member named 'dssStrengthToRgb'`.

- [ ] **Step 3: Extract the gradient interpolation**

`dbmToRgb()` at `src/gui/SpectrumWidget.cpp:4813-4830` inlines its stop interpolation. Lift that loop into a free function so the 3D path can reuse it without inheriting the black-level window. Add near `wfSchemeStops` at `:305`:

```cpp
// Interpolate a 0..1 position across a scheme's gradient stops. Extracted
// from dbmToRgb()'s inline loop so the 3DSS palette can share the stops
// without inheriting the waterfall gain / black-level window applied above
// it. Behaviour is unchanged for dbmToRgb.
QRgb interpolateWfGradient(float t, const WfGradientStop* stops, int count)
{
    const float adjusted = qBound(0.0f, t, 1.0f);
    for (int i = 0; i < count - 1; ++i) {
        if (adjusted <= stops[i + 1].pos) {
            const float f = (adjusted - stops[i].pos)
                          / (stops[i + 1].pos - stops[i].pos);
            const int r = static_cast<int>(
                stops[i].r + f * (stops[i + 1].r - stops[i].r));
            const int g = static_cast<int>(
                stops[i].g + f * (stops[i + 1].g - stops[i].g));
            const int b = static_cast<int>(
                stops[i].b + f * (stops[i + 1].b - stops[i].b));
            return qRgb(r, g, b);
        }
    }
    return qRgb(stops[count - 1].r, stops[count - 1].g, stops[count - 1].b);
}
```

Declare it in `SpectrumWidget.h` beside `wfSchemeStops`, and replace the inlined loop inside `dbmToRgb()` with `return interpolateWfGradient(adjusted, stops, stopCount);`.

- [ ] **Step 4: Add dssStrengthToRgb and the token**

```cpp
// From AetherSDR SpectrumWidget.cpp:11602-11611 [@1872028c].
QRgb SpectrumWidget::dssStrengthToRgb(float s) const
{
    // gamma in [0.25 .. 4]: gain=100 -> 0.25 (colour lifted to the noise
    // floor), gain=50 -> 1.0 (linear), gain=0 -> 4 (colour only on the
    // strongest peaks).
    const float gamma = std::pow(4.0f, (50.0f - m_dssGain) / 50.0f);
    int n = 0;
    const WfGradientStop* stops = wfSchemeStops(m_wfColorScheme, n);
    return interpolateWfGradient(
        std::pow(std::clamp(s, 0.0f, 1.0f), gamma), stops, n);
}

// Fold only the inputs that define 3DSS surface colour, so the LUT re-bakes
// on a real change and never on the per-frame floor / range jitter. The
// waterfall gain / black-level / min-dBm knobs are deliberately absent:
// they must not reshape the 3D surface.
quint64 SpectrumWidget::dssPaletteToken() const
{
    quint64 t = static_cast<quint64>(m_wfColorScheme);
    t = t * 131 + static_cast<quint64>(m_dssGain);
    return t;
}

void SpectrumWidget::uploadDssPaletteLut(QRhiResourceUpdateBatch* batch)
{
    if (!m_dssPaletteTex || !batch) { return; }
    const quint64 token = dssPaletteToken();
    if (token == m_dssLutToken) { return; }   // unchanged

    QImage lut(256, 1, QImage::Format_RGBA8888);   // owns its data
    for (int i = 0; i < 256; ++i) {
        const QRgb c = dssStrengthToRgb(i / 255.0f);
        lut.setPixelColor(i, 0, QColor(qRed(c), qGreen(c), qBlue(c)));
    }
    QRhiTextureSubresourceUploadDescription desc(lut);
    batch->uploadTexture(m_dssPaletteTex, QRhiTextureUploadEntry(0, 0, desc));
    m_dssLutToken = token;
}
```

Declare `QRgb dssStrengthToRgb(float s) const;` and `quint64 dssPaletteToken() const;` public in the header so the test can reach them.

- [ ] **Step 5: Run to verify it passes**

```bash
cmake --build build --target tst_dss_palette && ctest --test-dir build -R '^tst_dss_palette$' --output-on-failure
```

Expected: PASS, 7 test functions. Also re-run the waterfall colour tests to prove the `dbmToRgb` extraction changed nothing:

```bash
cmake --build build --target tests_gui && ctest --test-dir build -L gui --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp \
        tests/tst_dss_palette.cpp tests/CMakeLists.txt
git commit -m "feat(dss): bake the 3DSS palette from scheme stops, not dbmToRgb

Ports dssStrengthToRgb and the LUT upload from AetherSDR [@1872028c].
The 3D surface maps its stable colour aperture across the full colormap
independently of the Ref-level height span, gamma-shaped by 3D Gain.

Upstream bypasses dbmToRgb's waterfall gain and black-level window
deliberately, and says so in a comment. Building the LUT from dbmToRgb
instead would let the waterfall sliders silently reshape 3D colours, so
the test asserts the decoupling directly rather than only checking the
gamma curve.

dbmToRgb's inlined gradient loop is extracted to interpolateWfGradient so
both paths share the stops without sharing the window above them. No
behaviour change to the 2D waterfall."
```

---

## Task 9: Floor anchoring, the wide channel feed, and the UBO writer

**Files:**
- Modify: `src/gui/SpectrumWidget.h`, `src/gui/SpectrumWidget.cpp`
- Test: `tests/tst_dss_floor_and_span.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `NoiseFloorTracker::noiseFloor()`, `visibleBinRange()`, `dssRowSpanFactorFor` (Task 1), `DssRenderer::pushRowWithWide` (Task 4), `kDssMeshUboFloats` (Task 5).
- Produces:
  - `float SpectrumWidget::dssFloorDbm() const`
  - `float SpectrumWidget::dssSpanDb() const`
  - `float SpectrumWidget::dssRowSpanTarget(double targetBandwidthMhz) const`
  - `QVector<float> SpectrumWidget::buildDssWideRow(const QVector<float>& fullBins, double& wideCenterMhzOut, double& wideBandwidthMhzOut) const`
  - `void SpectrumWidget::writeDssMeshUbo(QRhiResourceUpdateBatch*, const QRect&, float)`

- [ ] **Step 1: Write the failing test**

```cpp
#include <QTest>
#include <QVector>
#include <cmath>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestDssFloorAndSpan : public QObject {
    Q_OBJECT

private slots:
    // The surface baseline sits 3D Floor dB BELOW the measured noise floor,
    // so raising the control reveals more noise texture, not less.
    void floorDbm_sitsBelowTheMeasuredNoiseFloor() {
        SpectrumWidget w;
        w.setMeasuredNoiseFloorForTest(-120.0f);
        w.setDssFloorDepth(0);
        QVERIFY(std::abs(w.dssFloorDbm() - (-120.0f)) < 0.01f);
        w.setDssFloorDepth(6);
        QVERIFY(std::abs(w.dssFloorDbm() - (-126.0f)) < 0.01f);
        w.setDssFloorDepth(24);
        QVERIFY(std::abs(w.dssFloorDbm() - (-144.0f)) < 0.01f);
    }

    void spanDb_followsTheDbmDisplayRange() {
        SpectrumWidget w;
        w.setDbmRange(-140.0f, -40.0f);
        QVERIFY(std::abs(w.dssSpanDb() - 100.0f) < 0.01f);
    }

    // At full DDC width there is nothing outside the view, so the span
    // control correctly reports nothing available and the surface relaxes to
    // the classic clipped trapezoid.
    void atFullDdcWidth_noSpanIsAvailable() {
        SpectrumWidget w;
        w.setSampleRate(768000.0);
        w.setDdcCenterFrequency(14200000.0);
        w.setFrequencyRange(14200000.0, 768000.0);   // view == DDC
        double c = 0.0;
        double b = 0.0;
        const QVector<float> wide =
            w.buildDssWideRow(QVector<float>(4096, -130.0f), c, b);
        QVERIFY(wide.isEmpty());
        QCOMPARE(b, 0.0);
    }

    // Zoomed in, the off-screen DDC bins are available and the wide row
    // covers more spectrum than the viewport.
    void zoomedIn_wideRowReachesPastTheViewport() {
        SpectrumWidget w;
        w.setSampleRate(768000.0);
        w.setDdcCenterFrequency(14200000.0);
        w.setFrequencyRange(14200000.0, 96000.0);    // 8x zoom
        double c = 0.0;
        double b = 0.0;
        const QVector<float> wide =
            w.buildDssWideRow(QVector<float>(4096, -130.0f), c, b);
        QVERIFY(!wide.isEmpty());
        QVERIFY2(b > 0.096, "wide row must span more than the viewport");
        QVERIFY(std::abs(c - 14.2) < 1e-6);
    }

    // The wide window is sized for the WIDEST angle, not the current one, so
    // moving the angle slider never invalidates rows already in the ring.
    void wideRowWindow_isAngleIndependent() {
        SpectrumWidget w;
        w.setSampleRate(768000.0);
        w.setDdcCenterFrequency(14200000.0);
        w.setFrequencyRange(14200000.0, 96000.0);
        double c0 = 0.0, b0 = 0.0, c1 = 0.0, b1 = 0.0;
        w.setDssAngle(0);
        w.buildDssWideRow(QVector<float>(4096, -130.0f), c0, b0);
        w.setDssAngle(100);
        w.buildDssWideRow(QVector<float>(4096, -130.0f), c1, b1);
        QCOMPARE(b0, b1);
        QCOMPARE(c0, c1);
    }

    // The 3D Span slider scales the AVAILABLE overhang, so 0 always gives
    // the classic trapezoid and 100 spends everything on offer.
    void rowSpanTarget_scalesTheAvailableOverhang() {
        SpectrumWidget w;
        w.setSampleRate(768000.0);
        w.setDdcCenterFrequency(14200000.0);
        w.setFrequencyRange(14200000.0, 96000.0);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        for (int i = 0; i < 3; ++i) {
            w.pushWaterfallRowForTest(QVector<float>(768, -130.0f));
        }
        w.setDssRowSpan(0);
        QVERIFY(std::abs(w.dssRowSpanTarget(0.096) - 1.0f) < 1e-6f);
        w.setDssRowSpan(100);
        QVERIFY(w.dssRowSpanTarget(0.096) > 1.0f);
    }
};

QTEST_MAIN(TestDssFloorAndSpan)
#include "tst_dss_floor_and_span.moc"
```

- [ ] **Step 2: Register and run to confirm it fails**

Add `nereus_add_test(tst_dss_floor_and_span)` to `tests/CMakeLists.txt`, then build. Expected: FAIL, `no member named 'dssFloorDbm'`.

- [ ] **Step 3: Implement the floor, span and wide-row feed**

```cpp
// The 3DSS surface baseline. Anchored to the measured noise floor so the
// stack keeps a constant apparent height as band conditions move, offset
// downward by the 3D Floor control to expose more or less noise texture.
float SpectrumWidget::dssFloorDbm() const
{
    // m_nfLerpAverage is this widget's smoothed measured noise floor, the
    // same quantity NoiseFloorTracker::noiseFloor() exposes (both are the
    // Thetis display.cs:4628 lerp average). m_nfFftBinAverage is the
    // per-frame value and would make the surface jitter every frame.
    return m_nfLerpAverage - static_cast<float>(m_dssFloorDepth);
}

float SpectrumWidget::dssSpanDb() const
{
    // The dBm display span. This widget stores the range as a top
    // (m_refLevel) plus a depth (m_dynamicRange), not as a floor/ceiling
    // pair, so the span is m_dynamicRange directly.
    return std::max(1.0f, m_dynamicRange);
}

// Build the wide channel from the off-screen DDC bins of the SAME FFT frame.
//
// The window is sized for the WIDEST angle the slider allows rather than the
// current one, so retained rows stay valid across a runtime angle change and
// moving the slider never forces a re-ingest (design doc section 4.2).
// Returns an empty vector when the view already covers the whole DDC, which
// is the correct "no span available" answer.
QVector<float> SpectrumWidget::buildDssWideRow(
    const QVector<float>& fullBins,
    double& wideCenterMhzOut,
    double& wideBandwidthMhzOut) const
{
    wideCenterMhzOut = 0.0;
    wideBandwidthMhzOut = 0.0;
    if (fullBins.isEmpty() || m_sampleRateHz <= 0.0) {
        return {};
    }
    const double viewBwHz = m_bandwidthHz;
    if (viewBwHz <= 0.0 || viewBwHz >= m_sampleRateHz) {
        return {};   // view already covers the DDC; nothing outside it
    }

    const float widestSpan = dssMaxRowSpanFactor(dssShapeForAngle(0));
    const double wantHz = std::min(
        static_cast<double>(widestSpan) * viewBwHz, m_sampleRateHz);
    const double ddcLowHz  = m_ddcCenterHz - m_sampleRateHz * 0.5;
    const double binHz     = m_sampleRateHz / fullBins.size();
    const double wideLowHz = std::clamp(
        m_centerHz - wantHz * 0.5,
        ddcLowHz, ddcLowHz + m_sampleRateHz - wantHz);

    const int first = std::clamp(
        static_cast<int>((wideLowHz - ddcLowHz) / binHz), 0, fullBins.size() - 1);
    const int last = std::clamp(
        static_cast<int>((wideLowHz + wantHz - ddcLowHz) / binHz),
        first + 1, fullBins.size());

    wideCenterMhzOut    = (wideLowHz + wantHz * 0.5) / 1.0e6;
    wideBandwidthMhzOut = wantHz / 1.0e6;
    return QVector<float>(fullBins.constBegin() + first,
                          fullBins.constBegin() + last);
}

// From AetherSDR SpectrumWidget.cpp:12703-12742 [@1872028c], minus the
// environment-variable override and the Flex/Kiwi producer caveats: our
// producer never appends without a wide slice while zoomed in.
float SpectrumWidget::dssRowSpanTarget(double targetBandwidthMhz) const
{
    return dssRowSpanFactorFor(
        m_dss.newestWideBandwidthMhz(targetBandwidthMhz),
        targetBandwidthMhz,
        m_dssRowSpan,
        dssShape());
}
```

Extend `pushDssRow()` from Task 6 to fill both channels:

```cpp
void SpectrumWidget::pushDssRow(const QVector<float>& wfPixelsDbm)
{
    const double centerMhz    = m_centerHz    / 1.0e6;
    const double bandwidthMhz = m_bandwidthHz / 1.0e6;
    double wideCenterMhz = 0.0;
    double wideBandwidthMhz = 0.0;
    const QVector<float> wide =
        buildDssWideRow(m_lastFullBinsDbm, wideCenterMhz, wideBandwidthMhz);
    if (wide.isEmpty()) {
        m_dss.pushRow(wfPixelsDbm, centerMhz, bandwidthMhz);
    } else {
        m_dss.pushRowWithWide(wfPixelsDbm, centerMhz, bandwidthMhz,
                              wide, wideCenterMhz, wideBandwidthMhz);
    }
    ++m_dssRowsPushed;
}
```

`m_lastFullBinsDbm` is the pre-`visibleBinRange` bin vector; cache it in `updateSpectrum()` where the slice at `:2690` is taken. Add the member and these test seams to the header:

```cpp
    void setMeasuredNoiseFloorForTest(float dbm) {
        m_nfLerpAverage = dbm;
    }
    QVector<float> buildDssWideRow(const QVector<float>& fullBins,
                                   double& wideCenterMhzOut,
                                   double& wideBandwidthMhzOut) const;
    float dssFloorDbm() const;
    float dssSpanDb() const;
    float dssRowSpanTarget(double targetBandwidthMhz) const;
```

- [ ] **Step 4: Implement the UBO writer**

Field order must match `dss_mesh.vert:14-58` exactly. Twenty-two scalars, two zeros of std140 padding, then the vec4s.

```cpp
void SpectrumWidget::writeDssMeshUbo(QRhiResourceUpdateBatch* batch,
                                     const QRect& specRect, float dpr)
{
    if (!m_dssUbo || !batch) { return; }
    const double targetBwMhz = m_bandwidthHz / 1.0e6;
    const double targetCenterMhz = m_centerHz / 1.0e6;
    const DssShape shape = dssShape();

    std::array<float, kDssMeshUboFloats> ubo{};
    int i = 0;
    // rowOffset: ring scroll plus a half texel so Nearest lands on centres.
    ubo[i++] = (m_dss.headRing() + 0.5f) / static_cast<float>(m_dss.rows());
    ubo[i++] = dssFloorDbm();
    ubo[i++] = dssSpanDb();
    ubo[i++] = 0.6f;                                  // zCurve: lift the floor band
    ubo[i++] = shape.backWidthFrac;
    ubo[i++] = shape.depthSpanFrac;
    ubo[i++] = shape.frontMaxRidgeFrac;
    ubo[i++] = kDssHaze;
    ubo[i++] = static_cast<float>(m_dss.cols());
    ubo[i++] = static_cast<float>(targetBwMhz);
    ubo[i++] = static_cast<float>(targetCenterMhz);
    ubo[i++] = 1.0f;                                  // rowFrequencyFrames on
    ubo[i++] = m_dssScrollProgressRows;
    ubo[i++] = static_cast<float>(m_dss.rows());
    //-KG4VCF [v0.5.3] Always one: our producer appends a single row per
    // waterfall tick. Permitted tier 1 deviation 2 of 2, design doc 2.3.
    ubo[i++] = 1.0f;                                  // scrollDistanceRows
    // Upstream caps the colour aperture at whichever is SMALLER, the stable
    // 45 dB constant or the actually-configured dBm span
    // (SpectrumWidget.cpp:14195 [@1872028c] writes
    // std::min(rangeDb, DssRenderer::kColorSpanDb)). Writing the constant
    // unconditionally would spread the colormap across a wider span than the
    // display is showing whenever the operator narrows the dBm range below
    // 45 dB, which m_dynamicRange permits down to 10, producing a visibly
    // washed-out surface that does not match the port target.
    ubo[i++] = std::min(dssSpanDb(), kDssColorSpanDb);
    ubo[i++] = static_cast<float>(m_dss.rowCount());
    ubo[i++] = static_cast<float>(kDssVisibleRows);
    ubo[i++] = specRect.width()  * dpr;
    ubo[i++] = specRect.height() * dpr;
    ubo[i++] = dssRowSpanTarget(targetBwMhz);
    ubo[i++] = static_cast<float>(m_dssMeshCols);
    ubo[i++] = 0.0f;                                  // std140 pad
    ubo[i++] = 0.0f;                                  // std140 pad

    const QColor bg(0x0a, 0x0a, 0x14);
    ubo[i++] = bg.redF();
    ubo[i++] = bg.greenF();
    ubo[i++] = bg.blueF();
    ubo[i++] = 1.0f;

    i += 8 * 4;   // shadowBands, filled by Task 12
    i += 8 * 4;   // shadowStyles, filled by Task 12
    ubo[i++] = 0.0f;                                  // descriptor count
    ubo[i++] = m_threeDSliceDepth ? 1.0f : 0.0f;
    ubo[i++] = specRect.width() * dpr;
    ubo[i++] = 0.0f;

    // rowFrames: per-row capture frame so older rows remap correctly while
    // the operator zooms or tunes with history on screen.
    for (int age = 0; age < kDssRows; ++age) {
        ubo[i++] = static_cast<float>(m_dss.rowCenterMhzAtAge(age));
        ubo[i++] = static_cast<float>(m_dss.rowBandwidthMhzAtAge(age));
        ubo[i++] = static_cast<float>(m_dss.rowWideCenterMhzAtAge(age));
        ubo[i++] = static_cast<float>(m_dss.rowWideBandwidthMhzAtAge(age));
    }
    Q_ASSERT(i == kDssMeshUboFloats);
    batch->updateDynamicBuffer(m_dssUbo, 0,
                               kDssMeshUboFloats * sizeof(float), ubo.data());
}
```

Add `float m_dssScrollProgressRows{0.0f};` to the header. Advance it in the display-timer tick from wall clock between waterfall pushes, clamped to `[0, 1]`, and reset to 0 in `pushDssRow()`.

- [ ] **Step 5: Run to verify**

```bash
cmake --build build --target tst_dss_floor_and_span && ctest --test-dir build -R '^tst_dss_floor_and_span$' --output-on-failure
```

Expected: PASS, 6 test functions.

- [ ] **Step 6: Commit**

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp \
        tests/tst_dss_floor_and_span.cpp tests/CMakeLists.txt
git commit -m "feat(dss): anchor the 3D floor, feed the wide channel, write the UBO

Floor is the measured NoiseFloorTracker value offset down by 3D Floor, so
the stack keeps a constant apparent height as conditions move.

The wide channel is filled from off-screen DDC bins of the same FFT frame.
Its window is sized for the widest angle the slider allows rather than the
current one, so retained rows survive a runtime angle change and moving
the slider never forces a re-ingest. At full DDC width the row is empty,
which is the correct no-span-available answer.

UBO field order matches dss_mesh.vert:14-58 exactly including the two
std140 padding floats, with an assert that the writer filled precisely
kDssMeshUboFloats entries."
```

---

## Task 10: CPU fallback surface

**Files:**
- Modify: `src/gui/DssRenderer.h`, `src/gui/DssRenderer.cpp`, `src/gui/SpectrumWidget.cpp`
- Test: `tests/tst_dss_cpu_surface.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `DssRenderer` ring accessors (Task 4), `DssShape` (Task 1).
- Produces:
  - `const QImage& DssRenderer::image(const QSize& px, int scaleStripPx, float floorDbm, float rangeDb, float zCurve, const PaletteFn& palette, quint64 paletteToken, const QColor& bgFill, const DssShape& shape)`
  - `QVector<bool> NereusSDR::dssDepthVisibleSegments(const QVector<qreal>& yFrontToBack)`

- [ ] **Step 1: Read upstream's CPU renderer**

```bash
git -C /Users/j.j.boyd/AetherSDR show 1872028c:src/gui/DssRenderer.cpp | sed -n '753,903p'
```

`image()` is the cache gate, `rebuild()` the painter's-algorithm draw, `dssDepthVisibleSegments` the occlusion test used by the depth-shadow overlay.

- [ ] **Step 2: Write the failing test**

```cpp
#include <QTest>
#include <QColor>
#include <QImage>
#include <QVector>

#include "gui/DssRenderer.h"

using namespace NereusSDR;

namespace {

QRgb greyPalette(float dbm)
{
    const int v = std::clamp(static_cast<int>((dbm + 140.0f) * 2.0f), 0, 255);
    return qRgb(v, v, v);
}

}  // namespace

class TestDssCpuSurface : public QObject {
    Q_OBJECT

private slots:
    void image_matchesRequestedSize() {
        DssRenderer r;
        r.pushRow(QVector<float>(768, -120.0f), 14.2, 0.192);
        const QImage& img = r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                                    greyPalette, 1, QColor(10, 10, 20),
                                    kDssUpstreamShape);
        QCOMPARE(img.size(), QSize(400, 200));
    }

    // The plot region is painted opaque; the bottom scale strip is left
    // transparent so the host can composite a scale on top.
    void scaleStrip_isLeftTransparent() {
        DssRenderer r;
        r.pushRow(QVector<float>(768, -120.0f), 14.2, 0.192);
        const QImage& img = r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                                    greyPalette, 1, QColor(10, 10, 20),
                                    kDssUpstreamShape);
        QCOMPARE(qAlpha(img.pixel(200, 195)), 0);     // inside the strip
        QVERIFY(qAlpha(img.pixel(200, 100)) > 0);     // inside the plot
    }

    // Rebuild is expensive, so the cache must hold when nothing changed and
    // drop when any mapping input moves.
    void cache_holdsUntilAnInputChanges() {
        DssRenderer r;
        r.pushRow(QVector<float>(768, -120.0f), 14.2, 0.192);
        const quint64 g0 = r.generation();
        r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                greyPalette, 1, QColor(10, 10, 20), kDssUpstreamShape);
        const quint64 g1 = r.generation();
        QVERIFY(g1 > g0);
        r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                greyPalette, 1, QColor(10, 10, 20), kDssUpstreamShape);
        QCOMPARE(r.generation(), g1);                 // unchanged: cache hit
        r.image(QSize(400, 200), 20, -130.0f, 80.0f, 0.6f,
                greyPalette, 1, QColor(10, 10, 20), kDssUpstreamShape);
        QVERIFY(r.generation() > g1);                 // floor moved: rebuild
    }

    // A different angle must invalidate the cache, or the CPU fallback keeps
    // drawing the previous perspective after the slider moves.
    void cache_dropsOnShapeChange() {
        DssRenderer r;
        r.pushRow(QVector<float>(768, -120.0f), 14.2, 0.192);
        r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                greyPalette, 1, QColor(10, 10, 20), dssShapeForAngle(50));
        const quint64 g = r.generation();
        r.image(QSize(400, 200), 20, -140.0f, 80.0f, 0.6f,
                greyPalette, 1, QColor(10, 10, 20), dssShapeForAngle(20));
        QVERIFY2(r.generation() > g, "shape change must invalidate the cache");
    }

    // Painter's algorithm: a nearer ridge hides anything below it.
    void depthVisibleSegments_cullsOccludedPoints() {
        // Front row highest (smallest y), so everything behind is hidden.
        const QVector<qreal> hidden{10.0, 50.0, 90.0};
        const QVector<bool> h = dssDepthVisibleSegments(hidden);
        QCOMPARE(h.size(), 2);
        QCOMPARE(h.at(0), false);
        QCOMPARE(h.at(1), false);
        // Rising behind the silhouette stays visible.
        const QVector<qreal> visible{90.0, 50.0, 10.0};
        const QVector<bool> v = dssDepthVisibleSegments(visible);
        QCOMPARE(v.size(), 2);
        QCOMPARE(v.at(0), true);
        QCOMPARE(v.at(1), true);
    }

    void depthVisibleSegments_handlesDegenerateInput() {
        QCOMPARE(dssDepthVisibleSegments({}).size(),        0);
        QCOMPARE(dssDepthVisibleSegments({5.0}).size(),     0);
    }
};

QTEST_APPLESS_MAIN(TestDssCpuSurface)
#include "tst_dss_cpu_surface.moc"
```

- [ ] **Step 3: Register, run to confirm failure, then port**

Add `nereus_add_test(tst_dss_cpu_surface)`. Build; expect `no member named 'image'`.

Port `image()`, `rebuild()` and `dssDepthVisibleSegments` from upstream `DssRenderer.cpp:753-903`, preserving the CPU-only tunables at `:16-21` (`kMinDim`, `kSlopeGain`, `kShadeLo`, `kShadeHi`) and every comment. Two changes only:

1. Both take `const DssShape& shape` and use it instead of the file constants.
2. The cache key gains the three shape floats, so an angle change rebuilds:

```cpp
    // Cache + the parameters it was built for (rebuild on any change).
    QImage  m_cache;
    QSize   m_cacheSize;
    int     m_cacheScaleStrip   = -1;
    float   m_cacheFloor        = 0.0f;
    float   m_cacheRange        = 0.0f;
    float   m_cacheZCurve       = 0.0f;
    quint64 m_cachePaletteToken = ~0ull;
    //-KG4VCF [v0.5.3] The perspective shape is a runtime value here, so it
    // is part of the cache key. Upstream can omit it: its shape is constant.
    DssShape m_cacheShape{0.0f, 0.0f, 0.0f};
```

Then wire the fallback in `renderGpuFrame()`: when `m_spectrumRenderMode == Mode3D && !m_dssMeshReady`, call `m_dss.image(...)` with `dssStrengthToRgb`-backed palette and blit it through the existing overlay pipeline instead of running the mesh pass.

- [ ] **Step 4: Verify and commit**

```bash
cmake --build build --target tst_dss_cpu_surface && ctest --test-dir build -R '^tst_dss_cpu_surface$' --output-on-failure
```

```bash
git add src/gui/DssRenderer.h src/gui/DssRenderer.cpp src/gui/SpectrumWidget.cpp \
        tests/tst_dss_cpu_surface.cpp tests/CMakeLists.txt
git commit -m "feat(dss): add the CPU fallback surface

Ports image, rebuild and dssDepthVisibleSegments from AetherSDR
DssRenderer.cpp:753-903 [@1872028c] with the CPU-only tunables intact.
Covers both the RGBA16F-unsupported path and NEREUS_GPU_SPECTRUM=OFF
builds.

The perspective shape joins the cache key. Upstream can leave it out
because its shape is a compile-time constant; ours moves with the angle
slider, and without it the fallback would keep drawing the old
perspective after the slider moved."
```

---

## Task 11: 3D dBm scale

**Files:**
- Modify: `src/gui/SpectrumWidget.h`, `src/gui/SpectrumWidget.cpp`
- Test: `tests/tst_dss_dbm_scale.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `dssFloorDbm()`, `dssSpanDb()` (Task 9).
- Produces: `void SpectrumWidget::drawDbmScale3D(QPainter&, const QRect&, float floorDbm)`

- [ ] **Step 1: Read upstream**

```bash
git -C /Users/j.j.boyd/AetherSDR show 1872028c:src/gui/SpectrumWidget.cpp | sed -n '17160,17240p'
```

- [ ] **Step 2: Write the failing test**

```cpp
#include <QTest>
#include <QImage>
#include <QPainter>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestDssDbmScale : public QObject {
    Q_OBJECT

private slots:
    // In 3D the scale is anchored to the drifting noise floor, not to the
    // Ref level, so its labels must move when the floor moves.
    void labels_followTheFloorAnchorIn3D() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDbmRange(-140.0f, -40.0f);
        w.setMeasuredNoiseFloorForTest(-120.0f);
        const float a = w.dssFloorDbm();
        w.setMeasuredNoiseFloorForTest(-100.0f);
        const float b = w.dssFloorDbm();
        QVERIFY2(std::abs(b - a) > 19.0f,
                 "3D scale anchor must track the measured floor");
    }

    void drawDbmScale3D_paintsWithoutCrashing() {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDbmRange(-140.0f, -40.0f);
        w.setMeasuredNoiseFloorForTest(-120.0f);
        QImage canvas(200, 300, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        QPainter p(&canvas);
        w.drawDbmScale3DForTest(p, QRect(0, 0, 200, 280), w.dssFloorDbm());
        p.end();
        // Something was drawn into the strip.
        bool anyInk = false;
        for (int y = 0; y < canvas.height() && !anyInk; ++y) {
            for (int x = 0; x < canvas.width(); ++x) {
                if (qAlpha(canvas.pixel(x, y)) != 0) { anyInk = true; break; }
            }
        }
        QVERIFY(anyInk);
    }

    void degenerateRect_isRefusedNotDivided() {
        SpectrumWidget w;
        QImage canvas(10, 10, QImage::Format_ARGB32_Premultiplied);
        QPainter p(&canvas);
        w.drawDbmScale3DForTest(p, QRect(0, 0, 0, 0), -140.0f);   // must not crash
        p.end();
        QVERIFY(true);
    }
};

QTEST_MAIN(TestDssDbmScale)
#include "tst_dss_dbm_scale.moc"
```

- [ ] **Step 3: Implement, verify, commit**

Port `drawDbmScale3D` from upstream, adding a public `drawDbmScale3DForTest` one-line forward. In the existing `drawDbmScale` callsite, branch on the render mode so 3D gets the floor-anchored scale and 2D keeps the Ref-anchored one.

```bash
cmake --build build --target tst_dss_dbm_scale && ctest --test-dir build -R '^tst_dss_dbm_scale$' --output-on-failure
```

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp \
        tests/tst_dss_dbm_scale.cpp tests/CMakeLists.txt
git commit -m "feat(dss): draw the floor-anchored dBm scale in 3D mode

Ports drawDbmScale3D from AetherSDR [@1872028c]. In 2D the amplitude
scale is Ref-anchored; in 3D it is anchored to the drifting measured
noise floor plus the 3D Floor offset, so it reads correctly against a
surface whose baseline moves with conditions."
```

---

## Task 12: Slice shadow decals

**Files:**
- Modify: `src/gui/SpectrumWidget.h`, `src/gui/SpectrumWidget.cpp`
- Test: `tests/tst_dss_slice_shadow.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `writeDssMeshUbo` (Task 9); `SpectrumWidget::sliceMarkerGeometry()` returning `QVector<SliceMarkerGeometry>` where `SliceMarkerGeometry { double centreHz; int filterLowHz; int filterHighHz; const VfoWidget* flag; }` (existing, declared at `SpectrumWidget.h:989-1003`). That accessor is already built to be reachable without a live painter or a shown QRhiWidget, which is exactly what this task's test needs, so no new test seam is required for the slice source.
- Produces:
  - `struct SpectrumWidget::DssShadowBand { float lowUnit; float highUnit; float centreUnit; float alpha; QColor cue; float centreAlpha; }`
  - `QVector<DssShadowBand> SpectrumWidget::buildDssShadowBands() const`

- [ ] **Step 1: Write the failing test**

Slices reach the widget through `addVfoWidget(index)` plus `setFrequency` /
`setFilter` on the returned flag, exactly as `tests/tst_pan_flag_positions.cpp`
drives them. Use that, not a new test seam.

```cpp
#include <QTest>
#include <cmath>

#include "gui/SpectrumWidget.h"
#include "gui/VfoWidget.h"

using namespace NereusSDR;

namespace {

// Give the widget a real geometry and view before asking for slice markers.
void placePan(SpectrumWidget& w)
{
    w.resize(800, 400);
    w.setFrequencyRange(14200000.0, 96000.0);
}

VfoWidget* addSlice(SpectrumWidget& w, int index, double hz, int lo, int hi)
{
    VfoWidget* flag = w.addVfoWidget(index);
    if (flag) {
        flag->setFrequency(hz);
        flag->setFilter(lo, hi);
    }
    return flag;
}

}  // namespace

class TestDssSliceShadow : public QObject {
    Q_OBJECT

private slots:
    void disabled_producesNoBands() {
        SpectrumWidget w;
        placePan(w);
        QVERIFY(addSlice(w, 0, 14200000.0, -3000, 3000));
        w.setThreeDSliceDepth(false);
        QVERIFY(w.buildDssShadowBands().isEmpty());
    }

    void enabled_mapsPassbandToViewportUnits() {
        SpectrumWidget w;
        placePan(w);
        QVERIFY(addSlice(w, 0, 14200000.0, -3000, 3000));
        w.setThreeDSliceDepth(true);
        const auto bands = w.buildDssShadowBands();
        QCOMPARE(bands.size(), 1);
        // A slice on the view centre sits at 0.5 across the viewport.
        QVERIFY(std::abs(bands[0].centreUnit - 0.5f) < 1e-4f);
        // 6 kHz of a 96 kHz view is 1/16 of the width.
        QVERIFY(std::abs((bands[0].highUnit - bands[0].lowUnit) - 0.0625f)
                < 1e-4f);
        QVERIFY(bands[0].lowUnit < bands[0].highUnit);
    }

    // The shader's shadowBands array is fixed at 8, so more slices than that
    // must be truncated by the builder rather than overrunning the UBO.
    void moreThanEightSlices_areTruncated() {
        SpectrumWidget w;
        w.resize(800, 400);
        w.setFrequencyRange(14200000.0, 500000.0);
        for (int i = 0; i < 12; ++i) {
            addSlice(w, i, 14100000.0 + i * 10000.0, -1500, 1500);
        }
        w.setThreeDSliceDepth(true);
        QVERIFY(w.sliceMarkerGeometry().size() > 8);   // precondition
        QCOMPARE(w.buildDssShadowBands().size(), 8);
    }

    void offscreenSlice_isDropped() {
        SpectrumWidget w;
        placePan(w);
        QVERIFY(addSlice(w, 0, 21000000.0, -3000, 3000));
        w.setThreeDSliceDepth(true);
        QVERIFY(w.buildDssShadowBands().isEmpty());
    }
};

QTEST_MAIN(TestDssSliceShadow)
#include "tst_dss_slice_shadow.moc"
```

If `addVfoWidget` cannot produce more than a handful of flags in a headless
test, drop `moreThanEightSlices_areTruncated` to whatever count it does
support and assert the clamp against `std::min(geometry.size(), 8)` instead.
The property under test is that the builder never exceeds eight, not the
specific number twelve.

- [ ] **Step 2: Implement**

`buildDssShadowBands()` maps each visible slice's passband edges to viewport units via the same `mhzToX` normalisation the 2D overlays use, clamped to eight entries. Feed the result into the `shadowBands` / `shadowStyles` / `shadowMeta` region of `writeDssMeshUbo` that Task 9 left zeroed, setting `shadowMeta.x` to the band count and `shadowMeta.y` to `m_threeDSliceDepth`.

**The toggle does NOT get its own `QMenu`.** Upstream adds it to a raw `QMenu`
because that is its right-click surface. Ours is `SpectrumOverlayMenu`, a
QWidget popup, and adding a second competing surface would show the operator
two menus in sequence on one right-click. The toggle therefore belongs in the
`3D VIEW` section that Task 13 builds, beside the five sliders it relates to.
Task 12 fills the UBO shadow slots only; Task 13 owns the control.

For reference, upstream's version of the item is at
`SpectrumWidget.cpp:9876-9887 [@1872028c]`:

```cpp
    if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
        QAction* depthAction = menu.addAction(tr("3D Slice Shadow"));
        depthAction->setCheckable(true);
        depthAction->setChecked(m_threeDSliceDepth);
        connect(depthAction, &QAction::toggled,
                this, &SpectrumWidget::setThreeDSliceDepth);
    }
```

- [ ] **Step 3: Verify and commit**

```bash
cmake --build build --target tst_dss_slice_shadow && ctest --test-dir build -R '^tst_dss_slice_shadow$' --output-on-failure
```

```bash
git add src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp \
        tests/tst_dss_slice_shadow.cpp tests/CMakeLists.txt
git commit -m "feat(dss): project slice passbands onto the 3D surface

Fills the shadowBands / shadowStyles / shadowMeta UBO region the fragment
shader's applySliceShadow consumes, plus the 3D Slice Shadow context-menu
toggle shown only in 3D, matching AetherSDR [@1872028c].

The shader array is fixed at eight descriptors, so the builder truncates
there and drops slices outside the viewport rather than relying on the
caller to bound it."
```

---

## Task 13: The 3D VIEW control section

**Files:**
- Modify: `src/gui/SpectrumOverlayMenu.h`, `src/gui/SpectrumOverlayMenu.cpp`, `src/gui/SpectrumWidget.cpp`
- Test: `tests/tst_dss_overlay_menu.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `SpectrumWidget` setters (Task 6).
- Produces signals on `SpectrumOverlayMenu`: `spectrumRenderModeChanged(int)`, `dssFloorDepthChanged(int)`, `dssGainChanged(int)`, `dssRowSpanChanged(int)`, `dssAngleChanged(int)`, plus `void setDssRowSpanSupported(bool)` and `void setDssValues(int mode, int floor, int gain, int span, int angle)`.

- [ ] **Step 1: Read upstream's section**

```bash
git -C /Users/j.j.boyd/AetherSDR show 1872028c:src/gui/SpectrumOverlayMenu.cpp | sed -n '1874,1942p'
```

Labels, ranges, defaults and tooltip wording are copied exactly. The 3D Angle row is new and sits after 3D Span.

- [ ] **Step 2: Write the failing test**

```cpp
#include <QTest>
#include <QSignalSpy>
#include <QSlider>
#include <QComboBox>

#include "gui/SpectrumOverlayMenu.h"

using namespace NereusSDR;

class TestDssOverlayMenu : public QObject {
    Q_OBJECT

private slots:
    void controlsExistWithUpstreamRangesAndDefaults() {
        SpectrumOverlayMenu m;
        auto* mode  = m.findChild<QComboBox*>(QStringLiteral("spectrumRenderModeCombo"));
        auto* floor = m.findChild<QSlider*>(QStringLiteral("dssFloorDepthSlider"));
        auto* gain  = m.findChild<QSlider*>(QStringLiteral("dssGainSlider"));
        auto* span  = m.findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"));
        auto* angle = m.findChild<QSlider*>(QStringLiteral("dssAngleSlider"));
        QVERIFY(mode);  QCOMPARE(mode->count(), 2);
        QVERIFY(floor); QCOMPARE(floor->minimum(), 0);   QCOMPARE(floor->maximum(), 24);
        QVERIFY(gain);  QCOMPARE(gain->minimum(),  0);   QCOMPARE(gain->maximum(), 100);
        QVERIFY(span);  QCOMPARE(span->minimum(),  0);   QCOMPARE(span->maximum(), 100);
        QVERIFY(angle); QCOMPARE(angle->minimum(), 0);   QCOMPARE(angle->maximum(), 100);
    }

    void movingASlider_emitsItsSignal() {
        SpectrumOverlayMenu m;
        QSignalSpy floorSpy(&m, &SpectrumOverlayMenu::dssFloorDepthChanged);
        QSignalSpy angleSpy(&m, &SpectrumOverlayMenu::dssAngleChanged);
        m.findChild<QSlider*>(QStringLiteral("dssFloorDepthSlider"))->setValue(12);
        m.findChild<QSlider*>(QStringLiteral("dssAngleSlider"))->setValue(80);
        QCOMPARE(floorSpy.count(), 1);
        QCOMPARE(floorSpy.at(0).at(0).toInt(), 12);
        QCOMPARE(angleSpy.count(), 1);
        QCOMPARE(angleSpy.at(0).at(0).toInt(), 80);
    }

    // setDssValues seeds the widgets before showing without echoing back out,
    // or opening the menu would rewrite the operator's settings.
    void setDssValues_doesNotEcho() {
        SpectrumOverlayMenu m;
        QSignalSpy spy(&m, &SpectrumOverlayMenu::dssGainChanged);
        m.setDssValues(1, 10, 40, 60, 25);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m.findChild<QSlider*>(QStringLiteral("dssGainSlider"))->value(), 40);
        QCOMPARE(m.findChild<QSlider*>(QStringLiteral("dssAngleSlider"))->value(), 25);
    }

    void spanUnsupported_disablesTheSliderNotTheRest() {
        SpectrumOverlayMenu m;
        m.setDssRowSpanSupported(false);
        QVERIFY(!m.findChild<QSlider*>(QStringLiteral("dssRowSpanSlider"))->isEnabled());
        QVERIFY(m.findChild<QSlider*>(QStringLiteral("dssAngleSlider"))->isEnabled());
    }
};

QTEST_MAIN(TestDssOverlayMenu)
#include "tst_dss_overlay_menu.moc"
```

- [ ] **Step 3: Implement**

Add a `3D VIEW` header and the five rows, copying upstream's labels, ranges, defaults and tooltip strings verbatim. The new 3D Angle row:

```cpp
    // ── 3D angle, viewing elevation (NereusSDR-original) ──────────────────
    makeRow("3D Angle:", 0, 100, 50, m_dssAngleSlider, m_dssAngleLabel);
    if (m_dssAngleSlider) {
        m_dssAngleSlider->setObjectName("dssAngleSlider");
        m_dssAngleSlider->setAccessibleName(tr("3D Angle"));
        m_dssAngleSlider->setToolTip(
            tr("Viewing angle for the 3D surface: low looks along the traces "
               "edge-on, high looks down on them.\n"
               "50 is the classic fixed angle."));
    }
    connect(m_dssAngleSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_dssAngleLabel) { m_dssAngleLabel->setText(QString::number(v)); }
        emit dssAngleChanged(v);
    });
```

Add the slice-shadow toggle to the same section, as a checkbox rather than a
slider. Task 12 filled the UBO slots the fragment shader's `applySliceShadow`
reads; this is the control that enables them. It does NOT get its own `QMenu`:
upstream uses one because a raw `QMenu` is its right-click surface, but ours is
`SpectrumOverlayMenu`, and a second competing surface would show the operator
two menus in sequence on a single right-click.

```cpp
    // ── 3D slice shadow, perspective decal for slice passbands ────────────
    m_dssSliceShadowChk = new QCheckBox(tr("3D Slice Shadow"));
    m_dssSliceShadowChk->setObjectName("dssSliceShadowCheck");
    m_dssSliceShadowChk->setToolTip(
        tr("Darken each slice's passband onto the 3D surface so it leans back\n"
           "with the perspective, instead of drawing flat on top of it."));
    grid->addWidget(m_dssSliceShadowChk, row, 0, 1, 4);
    connect(m_dssSliceShadowChk, &QCheckBox::toggled,
            this, &SpectrumOverlayMenu::dssSliceShadowChanged);
    ++row;
```

Wire all six signals to the `SpectrumWidget` setters where the menu is constructed, and call `setDssValues(...)` before `show()` alongside the existing `setValues(...)` call. `setDssValues` takes the shadow state too, and must not echo.

- [ ] **Step 4: Verify and commit**

```bash
cmake --build build --target tst_dss_overlay_menu && ctest --test-dir build -R '^tst_dss_overlay_menu$' --output-on-failure
```

```bash
git add src/gui/SpectrumOverlayMenu.h src/gui/SpectrumOverlayMenu.cpp \
        src/gui/SpectrumWidget.cpp tests/tst_dss_overlay_menu.cpp tests/CMakeLists.txt
git commit -m "feat(dss): add the 3D VIEW control section

Spectrum mode combo plus 3D Floor, 3D Gain and 3D Span with upstream's
exact labels, ranges, defaults and tooltip wording [@1872028c], and the
NereusSDR-original 3D Angle slider defaulting to 50.

setDssValues seeds the widgets without emitting, so opening the menu
cannot rewrite the operator's settings on the way in."
```

---

## Task 14: Persistence

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp`, `src/models/PanadapterModel.h`, `src/models/PanadapterModel.cpp`
- Test: `tests/tst_dss_persistence.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 6 accessors, `bandKeyName` (existing).
- Produces: `PanadapterModel::dss3DFloorDepthForBand(Band)` / `setDss3DFloorDepthForBand(Band, int)`

- [ ] **Step 1: Write the failing test**

```cpp
#include <QTest>

#include "core/AppSettings.h"
#include "gui/SpectrumWidget.h"
#include "models/PanadapterModel.h"

using namespace NereusSDR;

class TestDssPersistence : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        AppSettings::instance().setValue(
            QStringLiteral("SettingsSchemaVersion"), QStringLiteral("5"));
    }

    void fivePerPanKeys_roundTrip() {
        SpectrumWidget w;
        w.setPanIndex(0);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssGain(33);
        w.setDssRowSpan(44);
        w.setDssAngle(66);
        w.setThreeDSliceDepth(true);
        w.saveSettingsForTest();

        SpectrumWidget r;
        r.setPanIndex(0);
        r.loadSettings();
        QCOMPARE(r.spectrumRenderMode(),
                 static_cast<int>(SpectrumRenderMode::Mode3D));
        QCOMPARE(r.dssGain(),     33);
        QCOMPARE(r.dssRowSpan(),  44);
        QCOMPARE(r.dssAngle(),    66);
        QCOMPARE(r.threeDSliceDepth(), true);
    }

    void booleansPersistAsTrueFalseStrings() {
        SpectrumWidget w;
        w.setPanIndex(0);
        w.setThreeDSliceDepth(true);
        w.saveSettingsForTest();
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("Display3DSliceShadow_pan0"))
                     .toString(),
                 QStringLiteral("True"));
    }

    // 3D Floor is per band, keyed like the existing DisplayGridMax_<band>
    // keys, which carry no pan index (design doc section 6.2).
    void floorDepth_isRecalledPerBand() {
        PanadapterModel m;
        m.setDss3DFloorDepthForBand(Band::B80m, 4);
        m.setDss3DFloorDepthForBand(Band::B10m, 18);
        QCOMPARE(m.dss3DFloorDepthForBand(Band::B80m),  4);
        QCOMPARE(m.dss3DFloorDepthForBand(Band::B10m), 18);
    }

    void floorDepthKey_followsTheGridKeyConvention() {
        PanadapterModel m;
        m.setDss3DFloorDepthForBand(Band::B20m, 9);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("Display3DFloorDepth_")
                            + bandKeyName(Band::B20m)).toInt(),
                 9);
    }

    void unsetBand_returnsTheUpstreamDefault() {
        PanadapterModel m;
        QCOMPARE(m.dss3DFloorDepthForBand(Band::B6m), 6);
    }

    void bandChange_pushesTheStoredDepth() {
        PanadapterModel m;
        m.setDss3DFloorDepthForBand(Band::B40m, 15);
        m.setCenterFrequency(7100000.0);
        QCOMPARE(m.dss3DFloorDepthForBand(m.band()), 15);
    }
};

QTEST_MAIN(TestDssPersistence)
#include "tst_dss_persistence.moc"
```

- [ ] **Step 2: Implement**

In `SpectrumWidget::loadSettings()` and the save path, add the five per-pan keys through the existing `settingsKey(base, panIndex)` helper, following the `readInt` / `writeInt` / `readBool` patterns already there:

```cpp
    m_spectrumRenderMode = static_cast<SpectrumRenderMode>(
        std::clamp(readInt(QStringLiteral("DisplaySpectrumRenderMode"), 0),
                   0, static_cast<int>(SpectrumRenderMode::Count) - 1));
    m_dssGain          = std::clamp(readInt(QStringLiteral("Display3DGain"),  70), 0, 100);
    m_dssRowSpan       = std::clamp(readInt(QStringLiteral("Display3DSpan"), 100), 0, 100);
    m_dssAngle         = std::clamp(readInt(QStringLiteral("Display3DAngle"), 50), 0, 100);
    m_threeDSliceDepth = readBool(QStringLiteral("Display3DSliceShadow"), false);
```

In `PanadapterModel`, add the per-band store beside the existing grid storage at `:85-87`:

```cpp
QString dss3DFloorKey(Band b)
{
    return QStringLiteral("Display3DFloorDepth_") + bandKeyName(b);
}
```

with a 14-entry array defaulting to 6, loaded and saved exactly as `DisplayGridMax_` is, and pushed into the widget on the existing `bandChanged(Band)` signal that already drives per-band grid recall.

- [ ] **Step 3: Verify and commit**

```bash
cmake --build build --target tst_dss_persistence && ctest --test-dir build -R '^tst_dss_persistence$' --output-on-failure
```

```bash
git add src/gui/SpectrumWidget.cpp src/models/PanadapterModel.h \
        src/models/PanadapterModel.cpp tests/tst_dss_persistence.cpp tests/CMakeLists.txt
git commit -m "feat(dss): persist the six 3D controls

Five are per panadapter through the existing settingsKey helper. 3D Floor
is per band instead, keyed like the existing DisplayGridMax_<band> keys
and recalled on the bandChanged signal that already drives per-band grid
recall, because the floor it is anchored to is strongly a per-band
property."
```

---

## Task 15: Setup mirror and attribution close-out

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.h`, `src/gui/setup/DisplaySetupPages.cpp`
- Modify: `docs/attribution/aethersdr-reconciliation.md`, `docs/attribution/ASSETS.md`
- Test: `tests/tst_dss_setup_sync.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 6 accessors, Task 13 signals.
- Produces: nothing consumed downstream. This is the final task.

- [ ] **Step 1: Write the failing sync test**

Two surfaces mean two-way sync, which is the feedback-loop shape CLAUDE.md warns about.

```cpp
#include <QTest>
#include <QSignalSpy>
#include <QSlider>

#include "gui/SpectrumWidget.h"
#include "gui/setup/DisplaySetupPages.h"

using namespace NereusSDR;

class TestDssSetupSync : public QObject {
    Q_OBJECT

private slots:
    void setupPageEdit_reachesTheWidget() {
        SpectrumWidget w;
        Display3DSetupPage page(&w);
        page.findChild<QSlider*>(QStringLiteral("setup3DAngleSlider"))->setValue(70);
        QCOMPARE(w.dssAngle(), 70);
    }

    void widgetChange_updatesTheSetupPage() {
        SpectrumWidget w;
        Display3DSetupPage page(&w);
        w.setDssGain(25);
        QCOMPARE(page.findChild<QSlider*>(
                     QStringLiteral("setup3DGainSlider"))->value(), 25);
    }

    // The guard that stops the two surfaces echoing each other forever.
    void roundTrip_settlesWithoutAnEchoLoop() {
        SpectrumWidget w;
        Display3DSetupPage page(&w);
        QSignalSpy spy(&w, &SpectrumWidget::dssAngleChanged);
        page.findChild<QSlider*>(QStringLiteral("setup3DAngleSlider"))->setValue(80);
        QCOMPARE(w.dssAngle(), 80);
        QCOMPARE(spy.count(), 1);   // exactly one, not a cascade
    }

    void resetButton_restoresEveryDefault() {
        SpectrumWidget w;
        Display3DSetupPage page(&w);
        w.setDssFloorDepth(20);
        w.setDssGain(10);
        w.setDssRowSpan(5);
        w.setDssAngle(90);
        page.resetToDefaultsForTest();
        QCOMPARE(w.dssFloorDepth(),  6);
        QCOMPARE(w.dssGain(),       70);
        QCOMPARE(w.dssRowSpan(),   100);
        QCOMPARE(w.dssAngle(),      50);
    }
};

QTEST_MAIN(TestDssSetupSync)
#include "tst_dss_setup_sync.moc"
```

- [ ] **Step 2: Implement the Setup page**

Add a `Display3DSetupPage` under Setup → Display carrying the same six controls. Guard both directions with the `m_updatingFromModel` pattern used elsewhere in this file, and add a **Reset 3D to defaults** button that restores mode 2D, floor 6, gain 70, span 100, angle 50, shadow off.

- [ ] **Step 3: Add a `dssAngleChanged` signal to SpectrumWidget**

Emit it from `setDssAngle()` after the state settles, alongside the existing display-change signals, so the Setup page can follow the overlay menu.

- [ ] **Step 4: Close out attribution**

Every task that created a ported file already added its own Bucket A row in its own commit, because the pre-commit hook's full-tree scan makes deferral impossible. This step is the audit, not the bulk entry.

Confirm a Bucket A row exists in `docs/attribution/aethersdr-reconciliation.md` for each of `src/gui/DssGeometry.h`, `src/gui/DssRenderer.{h,cpp}`, `src/gui/DssMeshGeometry.h`, `src/gui/SpectrumWidget.{h,cpp}` and `src/gui/SpectrumOverlayMenu.{h,cpp}`, each naming the upstream file, its line ranges and the `[@1872028c]` stamp. Add any that are missing.

Then add `dss_mesh.vert` and `dss_mesh.frag` to the shader table in `docs/attribution/ASSETS.md`, using the existing `waterfall.frag` row as the template. Shaders live in `ASSETS.md` rather than the reconciliation table because AetherSDR ships no per-file shader headers.

- [ ] **Step 5: Full verification**

```bash
cmake --build build -j$(sysctl -n hw.ncpu) \
  && cmake --build build --target tests_gui tests_core tests_models \
  && ctest --test-dir build --output-on-failure
```

Expected: the whole suite green, including the ten new `tst_dss_*` executables. Then confirm the attribution gates:

```bash
python3 scripts/check-new-ports.py --full-tree && python3 scripts/verify-inline-cites.py && python3 scripts/verify-provenance-sync.py
```

- [ ] **Step 6: Manual bench check**

Launch and eyeball against the design's acceptance criterion:

```bash
./build/NereusSDR
```

Right-click the panadapter, set Spectrum to 3D Stacked Trace, and confirm: the surface appears in the trace pane with the waterfall still scrolling beneath it; 3D Angle at 50 matches AetherSDR side by side on the same band; the angle slider sweeps end to end without ridges leaving the plot; 3D Span closes the side wedges when zoomed in and greys out at full DDC width; and the DC-centre ridge wall named in design section 10 risk 5 either does not appear or is judged acceptable.

- [ ] **Step 7: Commit**

```bash
git add src/gui/setup/DisplaySetupPages.h src/gui/setup/DisplaySetupPages.cpp \
        src/gui/SpectrumWidget.h src/gui/SpectrumWidget.cpp \
        docs/attribution/aethersdr-reconciliation.md docs/attribution/ASSETS.md \
        tests/tst_dss_setup_sync.cpp tests/CMakeLists.txt
git commit -m "feat(dss): mirror the 3D controls into Setup and close attribution

Setup > Display gains the same six controls plus a reset button. Both
surfaces guard with m_updatingFromModel so an edit on either settles in
one hop instead of echoing, which the round-trip test pins.

Bucket A provenance rows for all seven touched files and ASSETS.md
entries for the two new shaders, since AetherSDR ships no per-file
shader headers."
```

---

## Self-review

**Spec coverage.** Every design section maps to a task: §2.3 tier deviations (Tasks 1, 5, 9, 10 markers), §3.1 files (all), §3.2 mesh pass slot (Task 7), §3.3 GPU resources (Task 7), §3.4 per-pan (Task 6), §4.1 row tee (Task 6), §4.2 two channels (Tasks 4, 9), §4.3 floor and colour (Tasks 8, 9), §4.4 zoom and scroll (Task 9 UBO `rowFrames`), §5 angle (Tasks 1, 2, 3, 7), §6 controls and persistence (Tasks 13, 14, 15), §7 divergences (Tasks 4, 9), §8 all eight tests (Tasks 1-4, 6, 8-12, 14), §9 attribution (Task 15), §10 risk 5 DC wall (Task 15 bench step).

**Type consistency checked.** `pushRowWithWide` is used with that exact name in Tasks 4, 6 and 9. `dssShape()` returns `DssShape` in Tasks 6, 7, 9, 10. `dssMeshColsFor(const DssShape&)` takes a shape, not an angle, at all three callsites. `uploadDssPaletteLut` is declared with one argument in Task 7 and defined with one in Task 8.

**Known plan-level gaps, stated rather than hidden.** Task 10's fallback blit and Task 12's UBO region fill are described structurally rather than as complete code, because both depend on the exact overlay-compositing call shape at the Task 7 integration point, which the implementer will have in front of them and I would otherwise be guessing at. Both have failing tests that define the contract precisely.

**Deep scrollback (design §3.1 `m_history*`) is deliberately out of this plan.** Upstream keeps a separate `qfloat16` scrollback ring so the 3D surface can be rebuilt while browsing waterfall history. That is a real parity item, but it is only reachable once our own waterfall scrollback and the 3D surface are both live, and it has no bearing on the live view. It should be raised as a follow-up rather than silently dropped.


---

# Addendum: display controls in the left panel (Tasks 16-18)

Added 2026-08-09 after bench feedback on the first smoke test. Two findings
from the operator:

1. The 3D controls exist only in the right-click popup and Setup. They belong
   in the left-hand display widget too, and while we are there, so do the other
   right-click display controls that make sense.
2. Dragging the dBm strip on the right edge does nothing in 3D.

Scope decision: the applet carries ALL 14 display controls and becomes the
primary surface. Branch decision: all of it lands on this branch rather than a
follow-up, accepted with the noted cost that the PR roughly doubles and mixes a
feature port with a cross-cutting display refactor.

## Task 16: wire the dBm strip drag to 3D Floor

The drag is not broken, it is wired to a value 3D ignores. In 2D it moves
`m_refLevel` (`SpectrumWidget.cpp`, find `m_draggingDbm` by name), and the 2D
scale is Ref-anchored so the whole window slides. In 3D nothing reads
`m_refLevel`: the surface anchors to `dssFloorDbm()`, which is
`m_nfLerpAverage - m_dssFloorDepth`. So the gesture runs, mutates a variable,
and the 3D view has no opinion about it.

Wire the plain drag to 3D Floor in 3D mode. That is the true analogue: drag
down to reveal more noise, up to hide it. CORRECTION, 2026-08-09: I originally wrote here that Ctrl-drag span-zoom
"already works in 3D since dssSpanDb() reads m_dynamicRange". That was wrong,
and the Task 16 implementer caught it. NereusSDR has NO Ctrl-drag gesture on
the dBm strip in either mode: `m_draggingDbmRange` has seven hits in upstream
at the pin and zero here, so the gesture was simply never ported. Nothing to
preserve, nothing regressed by Task 16, but the gesture is a genuine missing
upstream feature affecting 2D as much as 3D. Tracked separately below.

Rescale the travel. The 2D formula is `m_dynamicRange / specH` dB per pixel,
tuned for a range spanning -160..+20. 3D Floor spans 0..24, so reusing it would
saturate in about a centimetre. Map the full strip height to the full 0..24
range instead.

Keep the 0..24 bounds. They match the slider and upstream; widening them here
would put the drag and the slider out of step.

Tests: in 3D, a drag of N pixels changes `dssFloorDepth()` and NOT `refLevel`.
In 2D, the same drag changes `refLevel` and NOT `dssFloorDepth()`. Both
directions, and prove each fails if the mode branch is inverted.

## Task 17: DisplaySettingsModel

A QObject owning the 14 display values, one setter and one changed signal each,
plus load/save. Every surface binds to the model, never to another surface.

Why: today two surfaces sync point-to-point with `m_updatingFromModel` guards,
and the final review found one guard test that could not actually prove the
guard worked. A third surface makes that 42 hand-maintained bindings. With a
model each surface has one binding direction and the echo problem becomes
structurally impossible rather than something a future editor must remember.

Absorb persistence here too. It currently lives in `SpectrumWidget::
loadSettings()`, where 3D Floor's per-band split already sits awkwardly beside
the per-pan keys.

The 14: Colour Scheme, Colour Gain, Black Level, Ref Level, Dyn Range, Fill
Alpha, Fill spectrum trace, Spectrum mode, 3D Floor, 3D Gain, 3D Span,
3D Angle, 3D Slice Shadow, and the waterfall/spectrum split fraction.

## Gap found during Task 16: Ctrl-drag dBm span-zoom was never ported

AetherSDR gates a second dBm-strip gesture behind Ctrl (and Meta on macOS)
that zooms the amplitude span, anchored at the bottom, via a separate
`m_draggingDbmRange` flag. NereusSDR has no equivalent: the plain drag is the
only gesture on that strip.

This is not a 3D issue and not something this epic broke. It affects the 2D
panadapter equally and has presumably been absent since the strip was first
ported. Adding it is additive rather than a behaviour change, since no operator
can currently be relying on a gesture that does nothing.

RESOLVED 2026-08-09: ported (Task 19, commit 392aa6ee). All three applicable
upstream sites carried over; the fourth is upstream's PerfTelemetry
repaint-suppression aggregator, which has no counterpart here.

Ctrl-drag behaves identically in 2D and 3D, including the `m_refLevel`
recompute. That was decided on primary-source evidence rather than taste:
upstream's move handler does no `is3D` branching on this gesture, while the
adjacent floor-drag arm two lines away in the same press handler IS gated on
`is3D`, so the asymmetry is deliberate upstream. It is also the architecturally
coherent choice, since `m_refLevel` and `m_dynamicRange` describe the top and
depth of one window, unlike Task 16's floor-depth and ref-level which are
unrelated quantities.

---

# Addendum 2: the display-controls migration, staged (Tasks 18-23)

Added 2026-09-20. Task 17 landed the model (4e2412d7) with nothing bound to
it. The operator's decisions, verbatim from the archived design conversation:

- 2026-08-09: "lets add all of those rightclick display controls into the
  display widget that make sense. need to be in the display widget on the
  left". Asked which controls the applet carries: "Everything, applet becomes
  primary". Asked where it lands: "All on this branch."
- 2026-09-20 handoff: stage the migration so a regression bisects to a single
  surface, and the bar is zero behaviour change, because all fourteen
  controls work today.

The original Task 18 text ("re-point the popup and Setup in the same task")
is superseded by the staged shape below. Execution: run with `crew` under
`yonder-cost-aware-execution`, adapted for NereusSDR: there are no `R-*`
requirement IDs here, so each task's Requirements line cites the design
document section or the transcript decision it satisfies. No review between
tasks; one whole-branch review at the end.

## What the scouts established (2026-09-20, at 4e2412d7)

Line numbers below are hints; find everything by name (Global Constraints).
The full reports are in the crew workspace as `scout-popup.md`,
`scout-setup-persistence.md`, `scout-applet-infra.md` and
`scout-transcripts.md`.

- The popup (`SpectrumOverlayMenu`) carries thirteen of the fourteen (no
  split-fraction control exists anywhere). It only ever WRITES into
  `SpectrumWidget`: the lazy-construction block in
  `SpectrumWidget::mousePressEvent` (find `m_overlayMenu = new
  SpectrumOverlayMenu(this)`) connects the popup's signals either to inline
  lambdas that assign widget members directly (Colour Scheme, Colour Gain,
  Black Level, Ref Level, Dyn Range, Fill Alpha, Fill trace) or to the six
  guarded 3D setters. It is re-seeded from scratch on every right-click via
  `setValues()` / `setDssValues()` and never refreshed while open. One popup
  per panadapter, parented to its widget.
- Setup carries nine of the fourteen: Colour Scheme (`WaterfallDefaultsPage`),
  Fill Alpha and Fill trace (`SpectrumDefaultsPage`), and the six 3D controls
  plus a Reset button (`Display3DSetupPage`). Colour Gain, Black Level, Ref
  Level, Dyn Range and the split have no Setup control. The two RadioModel
  pages reach the active pan through `model()->spectrumWidget()`, which
  `MainWindow` repoints on `PanadapterStack::activePanChanged`.
  `Display3DSetupPage` takes a `SpectrumWidget*` in its constructor and stays
  bound to it, with `m_updatingFromModel` plus `QSignalBlocker` in both
  directions.
- `SpectrumWidget` has change signals for only the six 3D values. The other
  eight (`m_wfColorScheme`, `m_wfColorGain`, `m_wfBlackLevel`, `m_refLevel`,
  `m_dynamicRange`, `m_fillAlpha`, `m_panFill`, `m_spectrumFrac`) are
  written at these sites: `loadSettings()`, `setDbmRange()` (called by the
  noise-floor grid follow and by Setup's Copy button), the five named setters
  `setWfColorScheme` / `setWfColorGain` / `setWfBlackLevel` /
  `setPanFillEnabled` / `setFillAlpha`, the popup lambdas in
  `mousePressEvent`, the double-click and plus-or-minus-10 dB block in
  `mousePressEvent`, the Ctrl-drag and plain dBm-strip drags and the divider
  drag in `mouseMoveEvent`, and two blocks in `wheelEvent`. Ref Level and
  Dyn Range have no named setter. There is no `spectrumFrac()` getter.
- Ranges the widget accepts are wider than the popup sliders in one place:
  `wheelEvent` bounds Dyn Range to `qBound(10.0f, ..., 200.0f)` while the
  popup slider and the model clamp to 20..160. Every other range matches.
- Persistence: the thirteen per-pan values are loaded by
  `SpectrumWidget::loadSettings()` and saved by `saveSettings()` through one
  500 ms debounced `scheduleSettingsSave()`. 3D Floor is persisted per band
  on `PanadapterModel` and mirrored into the widget by the bridge in
  `MainWindow::wireDss3DFloorRecallForTest`, which is wired to pan 0 only.
- `DisplaySettingsModel` (Task 17) clamps Dyn Range to 20..160, keys 3D Floor
  by `Band` with immediate AppSettings writes to the same key
  `PanadapterModel` owns (a second writer to one key), and has `load()` /
  `save()` that nothing calls.
- `RadioModel::setSpectrumWidget()` is an inline pointer assignment with no
  signal, so nothing can react when the active pan changes.
- A new applet is: subclass `AppletWidget` (`src/gui/applets/AppletWidget.h`,
  constructor `AppletWidget(RadioModel* model, QWidget* parent = nullptr)`,
  pure virtuals `appletId()`, `appletTitle()`, `syncFromModel()`), built with
  the inherited `sliderRow()` / `styledButton()` / `divider()` helpers, added
  in `MainWindow::populateDefaultMeter()` with `panel->addApplet()`, entered
  in `m_appletsById`, registered with
  `m_appletVis->registerApplet(id, title, defaultVisible)` (persists under
  `Applet<Id>Visible`), and the two applet menus pick it up from
  `registeredIds()` with no further wiring. Do not call `appletTitleBar()`
  yourself: the panel wraps every applet in a title bar already.

## Decisions taken for the migration

1. **Ownership.** Each `SpectrumWidget` owns one `DisplaySettingsModel` as a
   child QObject, created in its constructor, exposed as `displaySettings()`.
   One model per panadapter by construction; every existing standalone
   `SpectrumWidget` test keeps working; surfaces that follow the active pan
   reach the active model through `RadioModel::spectrumWidget()`.
2. **3D Floor is the fourteenth per-pan field of the model, not a Band-keyed
   one.** The model holds the pan's CURRENT floor depth like any other value;
   the per-band store and its recall stay exactly where they are, on
   `PanadapterModel` through the existing `MainWindow` bridge, driven by the
   widget's `dssFloorDepthChanged`. Task 17's `dssFloorDepth(Band)` API is
   removed with its second writer to the per-band key.
3. **The widget stays the render-state owner and, until Task 23, the sole
   persister.** The binding is two-way: the model drives the widget through
   equality-guarded appliers; every internal write site in the widget pushes
   into the model. Persistence moves onto the model last, behind a golden
   test, so a format regression bisects to that one commit.
4. **Model clamps are never narrower than any widget write path.** Dyn Range
   widens to 10..200 to match `wheelEvent`. A round trip through the model
   must leave every value the widget accepts unchanged.
5. **Surfaces bind to the model and nothing else.** After Task 20 the popup
   has no connection to a widget setter; after Task 21 no Setup page calls a
   widget setter for these fourteen values; the applet never sees the widget.
   Live refresh (a change on one surface appearing on another while both are
   open) is the intended effect of the model and is not a behaviour
   regression.
6. **Which pan.** The applet follows the active pan, rebinding on a new
   `RadioModel::spectrumWidgetChanged` signal. `Display3DSetupPage` keeps its
   construction-time binding exactly as today; that it does not follow the
   active pan is a pre-existing defect recorded in the ledger for the
   operator, not changed here.
7. **Order.** Tasks run 18, 20, 21, 24, 22, 23. Task 24 (3D Speed, approved
   2026-09-20) lands before the applet so the applet is built once with all
   fifteen controls; Task 23 is last because its golden test covers every
   per-pan key.

## Task 18: bind SpectrumWidget to its DisplaySettingsModel

**Requirements:** design §6.1 (no feedback loops between surfaces), Task 17's
stated purpose (one binding per surface), decisions 1-4 above. Zero
operator-visible behaviour change.

**Files:**
- Modify: `src/models/DisplaySettingsModel.h` and `.cpp` (3D Floor becomes a
  per-pan field; Dyn Range clamp; header comment rewritten to match)
- Modify: `src/gui/SpectrumWidget.h` and `.cpp` (own the model, bind it,
  push from every write site, three new setters, one new getter, one test
  seam)
- Modify: `tests/tst_display_settings_model.cpp` (3D Floor and Dyn Range
  cases follow the model's new shape)
- Create: `tests/tst_display_settings_binding.cpp`
- Modify: `tests/CMakeLists.txt` (`nereus_add_test(tst_display_settings_binding)`,
  alphabetical)

**Interfaces:**
- Consumes: `DisplaySettingsModel` as landed in 4e2412d7; `SpectrumWidget`'s
  existing getters `refLevel()`, `dynamicRange()`, `wfColorScheme()`,
  `wfColorGain()`, `wfBlackLevel()`, `panFillEnabled()`, `fillAlpha()`,
  `spectrumRenderMode()`, `dssFloorDepth()`, `dssGain()`, `dssRowSpan()`,
  `dssAngle()`, `threeDSliceDepth()`, `panIndex()`; existing setters
  `setDbmRange(float minDbm, float maxDbm)`, `setWfColorScheme(WfColorScheme)`,
  `setWfColorGain(int)`, `setWfBlackLevel(int)`, `setPanFillEnabled(bool)`,
  `setFillAlpha(float)`, `setSpectrumRenderMode(int)`, `setDssFloorDepth(int)`,
  `setDssGain(int)`, `setDssRowSpan(int)`, `setDssAngle(int)`,
  `setThreeDSliceDepth(bool)`; signals `spectrumRenderModeChanged(int)`,
  `dssFloorDepthChanged(int)`, `dssGainChanged(int)`, `dssRowSpanChanged(int)`,
  `dssAngleChanged(int)`, `threeDSliceDepthChanged(bool)`.
- Produces, on `DisplaySettingsModel`: `int dssFloorDepth() const;`
  `void setDssFloorDepth(int depthDb);` `signals: void dssFloorDepthChanged(int depthDb);`
  (clamp 0..24, default 6, equality-guarded, NOT touched by `load()` or
  `save()`); `setDynamicRange()` clamps to 10.0f..200.0f. Removed:
  `dssFloorDepth(Band)`, `setDssFloorDepth(Band, int)`,
  `dssFloorDepthChanged(NereusSDR::Band, int)`, the `Band.h` include and the
  `dssFloorDepthKeyFor()` helper.
- Produces, on `SpectrumWidget`: `DisplaySettingsModel* displaySettings() const;`
  `void setRefLevel(float dBm);` `void setDynamicRange(float dB);`
  `void setSpectrumFrac(float frac);` `float spectrumFrac() const;`
  `int displaySettingsApplyCountForTest() const;` and `setPanIndex(int)` now
  also forwards to the model. Private: `void bindDisplaySettings();`
  `void syncDisplaySettingsFromWidget();` `DisplaySettingsModel* m_displaySettings{nullptr};`
  `int m_displaySettingsApplyCount{0};`

**Acceptance:**
- Model to widget, all fourteen: setting a non-default in-range value on the
  model makes the matching widget getter return it (Colour Scheme compared as
  `static_cast<int>`, floats within 1e-6).
- Widget to model, every write path: each of the eight named setters (five
  existing plus the three new) and the six 3D setters; `setDbmRange(-120.0f, -40.0f)`
  gives model Ref Level -40 and Dyn Range 80; `loadSettings()` with seeded
  AppSettings keys (pan 0, the exact key strings in the scout table) leaves
  the model holding the seeded values; a plain dBm-strip drag and a divider
  drag, driven the way `tests/tst_dss_floor_drag.cpp` and
  `tests/tst_dbm_range_drag.cpp` drive the mouse, move the model's Ref Level
  and split fraction with the widget's.
- Echo termination: with `QSignalSpy` on the model's `refLevelChanged`, one
  model-driven change emits exactly once, one widget-driven change (through
  `setRefLevel`) emits exactly once, and re-applying an equal value emits
  zero times and leaves `displaySettingsApplyCountForTest()` unchanged.
- No apply during load: after constructing a widget and running
  `loadSettings()`, `displaySettingsApplyCountForTest()` is 0.
- Round trip never narrows: `setDbmRange(-180.0f, 20.0f)` (Dyn Range 200)
  and `setDbmRange(-50.0f, -40.0f)` (Dyn Range 10) leave `dynamicRange()`
  and the model's `dynamicRange()` at 200 and 10 respectively. This case must
  go red with the model's old 20..160 clamp; state in the report that it did.
- The 3D Floor bridge still fires: `displaySettings()->setDssFloorDepth(12)`
  makes `dssFloorDepth()` 12 and emits the widget's `dssFloorDepthChanged`
  exactly once (that signal is what `MainWindow`'s per-band save listens to).
- Mutation proofs, run and reported, not committed: remove the push in
  `setDbmRange()` (the `setDbmRange` case goes red); remove one model-to-widget
  connect (the all-fourteen case goes red for that value); restore the old
  Dyn Range clamp (the round-trip case goes red).
- `tests/tst_display_settings_model.cpp`, `tst_dss_overlay_menu`,
  `tst_dss_setup_sync`, `tst_dss_persistence`, `tst_dss_floor_drag`,
  `tst_dbm_range_drag` all green after the change.

**Verification:** ordinary feature tier. Unit, gui label. TDD inner loop:
`cmake --build build --target tst_display_settings_binding && ctest --test-dir build -R '^tst_display_settings_binding$' -j1`.
Siblings once before committing:
`cmake --build build --target tst_display_settings_model tst_dss_overlay_menu tst_dss_setup_sync tst_dss_persistence tst_dss_floor_drag tst_dbm_range_drag && ctest --test-dir build -R '^tst_(display_settings_model|dss_overlay_menu|dss_setup_sync|dss_persistence|dss_floor_drag|dbm_range_drag)$' -j1`.
Not the full suite. Tests that drag must `resize()`, `show()`, then
`QVERIFY(QTest::qWaitForWindowExposed(&w))` (Global Constraints).

**Execution note (advisory):** sonnet. No prerequisite beyond Task 17. Not
parallelisable: it edits `SpectrumWidget.cpp`, which every later task also
edits.

- [ ] **Step 1: reshape the model.** In `DisplaySettingsModel.h`/`.cpp`
  replace the Band-keyed 3D Floor trio with the per-pan trio in Interfaces,
  add `int m_dssFloorDepth{6};`, delete the `Band.h` include and
  `dssFloorDepthKeyFor()`. `load()` and `save()` do not read or write 3D
  Floor; add one comment line at each saying so and why (the per-band store
  lives on `PanadapterModel`). Change `setDynamicRange()`'s clamp to
  `std::clamp(dB, 10.0f, 200.0f)` with a comment naming
  `SpectrumWidget::wheelEvent`'s `qBound(10.0f, ..., 200.0f)` as the source
  of the bounds. Rewrite the file-header sections "THE ONE FIELD THAT IS NOT
  LIKE THE OTHERS" and "SCOPE NOTE (Task 17)" so they describe the shape you
  just built: 3D Floor is a per-pan current value here, per-band persistence
  and recall stay on `PanadapterModel` via the `MainWindow` bridge, and the
  surfaces bind to this model from Task 20 on. No em-dashes in text you
  write. Update `tests/tst_display_settings_model.cpp`: the Band-keyed floor
  cases become per-pan cases (clamp 0..24, equality guard, one emission,
  untouched by `load()`/`save()`), and the Dyn Range clamp cases use 10 and
  200. Build and run that test; it must be green before Step 2.
- [ ] **Step 2: write `tests/tst_display_settings_binding.cpp` first**, one
  `private slots:` case per Acceptance bullet above, following the fixture
  shape of `tests/tst_dss_floor_drag.cpp` (AppSettings seeding, widget
  construction, show and expose). Register it in `tests/CMakeLists.txt`
  alphabetically. Build it; it must fail to compile or fail at runtime until
  Steps 3-5 exist. Record the failing output in the report.
- [ ] **Step 3: own and bind the model.** In the `SpectrumWidget`
  constructor, after `m_panIndex` is known, create
  `m_displaySettings = new DisplaySettingsModel(this);`, call
  `m_displaySettings->setPanIndex(m_panIndex);`, then `bindDisplaySettings();`.
  Make `setPanIndex(int idx)` forward to `m_displaySettings->setPanIndex(idx)`.
  `bindDisplaySettings()` wires fourteen model-to-widget connections: each
  model `xxxChanged` signal to the matching widget applier
  (`setWfColorScheme` with a `static_cast<WfColorScheme>`, `setWfColorGain`,
  `setWfBlackLevel`, `setRefLevel`, `setDynamicRange`, `setFillAlpha`,
  `setPanFillEnabled`, `setSpectrumFrac`, `setSpectrumRenderMode`,
  `setDssFloorDepth`, `setDssGain`, `setDssRowSpan`, `setDssAngle`,
  `setThreeDSliceDepth`), and six widget-to-model connections from the six 3D
  widget signals to the model's six 3D setters.
- [ ] **Step 4: appliers are equality-guarded and counted.** Add the three
  new setters. `setRefLevel(float dBm)`: early-return when
  `qFuzzyCompare(m_refLevel, dBm)`; assign; `update()`;
  `scheduleSettingsSave()`; push. Same shape for `setDynamicRange` and
  `setSpectrumFrac` (the latter clamps to 0.10f..0.90f like the divider
  drag). Add the same early-return to `setWfColorScheme`, `setFillAlpha` and
  `setPanFillEnabled` where it is missing (`setWfColorGain` and
  `setWfBlackLevel` already have one; verify, do not assume). Increment
  `m_displaySettingsApplyCount` once per real apply, after the guard, in all
  fourteen appliers. Add `float spectrumFrac() const { return m_spectrumFrac; }`
  and `int displaySettingsApplyCountForTest() const`.
- [ ] **Step 5: push from every write site.** Implement
  `syncDisplaySettingsFromWidget()` as eight model setter calls with the
  widget's current members (Colour Scheme as `static_cast<int>`). Call it:
  once at the end of `loadSettings()`; at the end of `setDbmRange()`; after
  the assignment in each of the eight named setters; after the write in the
  double-click and plus-or-minus-10 dB block of `mousePressEvent`; after the
  Ctrl-drag write, the plain drag write and the divider write in
  `mouseMoveEvent`; after each of the two write blocks in `wheelEvent`.
  Change the seven popup lambdas in `mousePressEvent` from member
  assignments to calls of the named setters (`setWfColorGain(v)` and so on,
  Ref Level and Dyn Range through the new setters), which is behaviour
  identical (assign, `update()`, `scheduleSettingsSave()`). Self-check with
  `grep -nE "\bm_(refLevel|dynamicRange|wfColorGain|wfBlackLevel|wfColorScheme|fillAlpha|panFill|spectrumFrac)\s*(=|\+=|-=)[^=]" src/gui/SpectrumWidget.cpp`:
  every remaining hit is in `loadSettings()`, in one of the eight setters, or
  followed in the same block by the push. List the hits in the report.
- [ ] **Step 6: prove and commit.** Run the new test green, run the mutation
  proofs and revert them, run the sibling tests, commit GPG-signed with a
  message in the branch's `feat(display): ...` style, no trailer, no
  em-dashes. Branch assertion before committing:
  `git -C /Users/j.j.boyd/NereusSDR/.claude/worktrees/eloquent-haibt-9134a2 branch --show-current` must print `claude/3d-waterfall-aethersdr-port-d793f1`.

## Task 19: Ctrl-drag dBm span zoom (landed)

Landed 2026-08-09 as commit 392aa6ee, before this heading existed; the full
account is the section "Gap found during Task 16" above. Nothing to do. This
heading exists so the crew ledger and `ledger-status` see the same task
numbering as the commit history.

## Task 20: re-point the right-click popup at the model

**Requirements:** decision 5 above; design §6.1. Zero operator-visible
behaviour change; the popup's labels, ranges, tooltips, sections and
`SpectrumOverlayMenu`'s public API are untouched.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp` (the lazy popup-construction block in
  `mousePressEvent`)
- Create: `tests/tst_display_popup_binding.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 18's `displaySettings()` and the model's fourteen setters
  and signals; `SpectrumOverlayMenu`'s existing signals
  (`wfColorSchemeChanged(int)`, `wfColorGainChanged(int)`,
  `wfBlackLevelChanged(int)`, `refLevelChanged(float)`,
  `dynRangeChanged(float)`, `fillAlphaChanged(float)`, `panFillChanged(bool)`,
  `spectrumRenderModeChanged(int)`, `dssFloorDepthChanged(int)`,
  `dssGainChanged(int)`, `dssRowSpanChanged(int)`, `dssAngleChanged(int)`,
  `dssSliceShadowChanged(bool)`) and bulk seeders `setValues(...)` /
  `setDssValues(...)` / `setDssRowSpanSupported(bool)` with the signatures in
  `SpectrumOverlayMenu.h` (read them; do not guess argument order).
- Produces: nothing new. The popup block's connects target
  `m_displaySettings` exclusively.

**Acceptance:**
- Every one of the thirteen popup signals is connected to the matching
  `DisplaySettingsModel` setter (`dynRangeChanged` to `setDynamicRange`,
  `dssSliceShadowChanged` to `setThreeDSliceDepth`, `panFillChanged` to
  `setPanFill`); no popup signal is connected to a `SpectrumWidget` member
  function or to a lambda that touches a widget member.
- The seeding calls read the model's getters, and a test proves the seeded
  popup values equal the model's after a model-driven change made before the
  right-click.
- Live refresh: while the popup is visible, a model-driven change updates the
  matching popup control, and that refresh emits none of the popup's signals
  (`QSignalSpy` count 0), so the model sees exactly one change.
- Popup-driven change reaches both the model and the widget: moving a popup
  slider (find the popup as `tests/tst_dss_overlay_menu.cpp` finds it after a
  synthetic right-click, and its controls by their member object names or by
  `findChildren<QSlider*>()` order as that test does) changes the model
  getter and, through Task 18's binding, the widget getter.
- The `setDssRowSpanSupported()` push on open is unchanged (it is derived
  from the widget's DDC width, not a setting).
- Mutation proofs, run and reported: re-point one popup connect back at the
  widget setter and the "connected to the model" case goes red; remove the
  live-refresh connect and the live-refresh case goes red.
- `tst_dss_overlay_menu`, `tst_display_settings_binding`, `tst_dss_floor_drag`
  green.

**Verification:** ordinary feature tier, unit, gui label. Inner loop
`cmake --build build --target tst_display_popup_binding && ctest --test-dir build -R '^tst_display_popup_binding$' -j1`;
siblings once: `tst_dss_overlay_menu`, `tst_display_settings_binding`,
`tst_dss_floor_drag`. Show and expose the widget before any synthetic mouse
event.

**Execution note (advisory):** sonnet. Requires Task 18. Not
parallelisable (same file).

- [ ] **Step 1: write the test first**, one case per Acceptance bullet,
  copying the popup-discovery mechanics from `tests/tst_dss_overlay_menu.cpp`.
  Register it. It must fail before Step 2 (the live-refresh case cannot pass
  today because the popup is never refreshed while open); record the red run.
- [ ] **Step 2: rewire the block.** In `mousePressEvent`, at the lazy
  construction of `m_overlayMenu`, replace each connect's receiver with
  `m_displaySettings` and the matching setter; replace the widget getters in
  the `setValues()` / `setDssValues()` calls with the model's getters; add
  thirteen connects from the model's signals to a lambda that, when
  `m_overlayMenu->isVisible()`, re-seeds the popup with `setValues()` /
  `setDssValues()` from the model. Keep the `setDssRowSpanSupported()` line.
  Delete the now-unused lambdas.
- [ ] **Step 3: prove and commit.** Green, mutation proofs, siblings, signed
  commit with the branch assertion from Task 18 Step 6.

## Task 21: re-point Setup -> Display at the model

**Requirements:** decision 5 and decision 6 above; design §6.1. Zero
operator-visible behaviour change; page layouts, labels, tooltips and which
pan each page edits are untouched.

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.h` and `.cpp`
  (`WaterfallDefaultsPage` Colour Scheme; `SpectrumDefaultsPage` Fill Alpha
  and Fill trace; `Display3DSetupPage` all six plus Reset)
- Modify: `tests/tst_dss_setup_sync.cpp`
- Create: `tests/tst_display_setup_binding.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 18's `SpectrumWidget::displaySettings()`; the model's
  setters, getters and signals.
- Produces: `Display3DSetupPage` keeps its constructor
  `Display3DSetupPage(SpectrumWidget* spectrumWidget, QWidget* parent)` and
  binds to `spectrumWidget->displaySettings()`. Its `m_updatingFromModel`
  member is deleted: the model's equality guard is the loop terminator now,
  and `QSignalBlocker` on reflect stays so a reflected value never re-emits
  the control's own signal.

**Acceptance:**
- No Setup page calls a `SpectrumWidget` setter for any of the nine values;
  each control's signal reaches the model's setter, and each page reads the
  model's getter when it loads. Grep evidence in the report:
  `grep -nE "spectrumWidget\(\)->set|m_spectrumWidget->set" src/gui/setup/DisplaySetupPages.cpp`
  returns no hit for the nine values (hits for other display settings that
  are not among the fourteen are expected and stay).
- `Display3DSetupPage`: with a `SpectrumWidget w` and `Display3DSetupPage page(&w)`,
  `w.displaySettings()->setDssGain(33)` puts 33 on the page's gain slider
  without emitting the slider's `valueChanged` past the page (spy on the
  model: exactly one emission total); moving the page's slider to 44 makes
  the model and `w.dssGain()` read 44; the Reset button puts the six defaults
  (2D, 6, 70, 100, 50, off) on the model.
- `WaterfallDefaultsPage` and `SpectrumDefaultsPage`: Colour Scheme, Fill
  Alpha and Fill trace round-trip the same way through
  `model()->spectrumWidget()->displaySettings()`. Construct these pages the
  way the nearest existing test constructs a RadioModel-backed setup page
  (grep `tests/` for `SpectrumDefaultsPage`; if no test constructs one, build
  `RadioModel`, `SpectrumWidget`, `radioModel.setSpectrumWidget(&w)`, then
  the page, and say so in the report).
- The existing `tests/tst_dss_setup_sync.cpp` cases pass unchanged or are
  adjusted only where they referenced `m_updatingFromModel`; the adjusted
  version must still fail if the model's equality guard is removed (mutation
  proof, reported).
- The Copy button on `WaterfallDefaultsPage` still reads Ref Level and Dyn
  Range (it may read them from the model or the widget; both are equal) and
  is otherwise untouched.

**Verification:** ordinary feature tier, unit, gui label. Inner loop on
`tst_display_setup_binding` and `tst_dss_setup_sync`; siblings once:
`tst_display_settings_binding`, `tst_display_popup_binding`.

**Execution note (advisory):** sonnet. Requires Task 18 (and reads Task 20's
result only to keep the popup tests green). Not parallelisable (shares the
test CMake list and the widget header).

- [ ] **Step 1: tests first.** Adjust `tst_dss_setup_sync.cpp` and write
  `tst_display_setup_binding.cpp` per Acceptance; register; record the red
  run.
- [ ] **Step 2: `Display3DSetupPage`.** Bind through
  `m_spectrumWidget->displaySettings()`: six push connects to the model's
  setters, six live connects from the model's signals reflecting under
  `QSignalBlocker`, `loadFromWidget()` reading the model, Reset writing the
  model. Delete `m_updatingFromModel` and every read of it. Update the class
  comment in the header (it currently explains the flag) so it explains the
  equality guard instead; no em-dashes in text you write.
- [ ] **Step 3: the two RadioModel pages.** Route Colour Scheme, Fill Alpha
  and Fill trace through `model()->spectrumWidget()->displaySettings()` for
  both the push and the load-on-show read. Keep the `QSignalBlocker`s on
  load.
- [ ] **Step 4: prove and commit** as in Task 18 Step 6.

## Task 22: DisplayApplet in the left panel

**Requirements:** the operator's decisions quoted at the top of this addendum
("Everything, applet becomes primary", "All on this branch"); design §6
(controls, ranges, defaults, the Reset 3D button); decision 6 (follows the
active pan).

**Files:**
- Create: `src/gui/applets/DisplayApplet.h` and `.cpp`
- Modify: `src/models/RadioModel.h` (`setSpectrumWidget` emits
  `spectrumWidgetChanged`)
- Modify: `src/gui/MainWindow.h` and `.cpp` (construct, add, register)
- Modify: `CMakeLists.txt` (add `src/gui/applets/DisplayApplet.cpp` beside
  `RadeApplet.cpp`)
- Create: `tests/tst_display_applet.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `AppletWidget` (constructor `AppletWidget(RadioModel* model, QWidget* parent = nullptr)`,
  pure virtuals `QString appletId() const`, `QString appletTitle() const`,
  `void syncFromModel()`, helpers `sliderRow()`, `styledButton()`,
  `divider()`, whose exact signatures are in
  `src/gui/applets/AppletWidget.h`, read them); `applyComboStyle(QComboBox*)`
  from `src/gui/ComboStyle.h`; `GuardedSlider` and `GuardedComboBox` from
  `src/gui/GuardedSlider.h` and `src/gui/GuardedComboBox.h`;
  `AppletVisibilityController::registerApplet(const QString& id, const QString& title, bool defaultVisible)`
  (read the header for the exact signature); Task 18's
  `SpectrumWidget::displaySettings()`; `RadioModel::spectrumWidget()`.
- Produces: `class DisplayApplet : public AppletWidget` with
  `explicit DisplayApplet(RadioModel* model, QWidget* parent = nullptr);`
  `appletId()` returning `"Display"`, `appletTitle()` returning `"Display"`;
  private `void bindTo(DisplaySettingsModel* m);` `DisplaySettingsModel* m_bound{nullptr};`
  and a `QObject* m_bindContext` recreated on every bind so old connections
  die with it. On `RadioModel`: `setSpectrumWidget(SpectrumWidget* w)`
  becomes `{ if (m_spectrumWidget == w) { return; } m_spectrumWidget = w; emit spectrumWidgetChanged(w); }`
  with `signals: void spectrumWidgetChanged(class SpectrumWidget* w);`.

**Acceptance:**
- Three sections in this order with `divider()` between them, titled exactly
  as the popup's section headers (copy the strings from
  `SpectrumOverlayMenu.cpp`): Waterfall (Colour Scheme combo, Colour Gain,
  Black Level), Spectrum (Ref Level, Dyn Range, Fill Alpha, Fill trace
  toggle, Split), 3D View (Spectrum mode combo, 3D Floor, 3D Gain, 3D Span,
  3D Angle, 3D Speed from Task 24, Slice Shadow toggle, Reset 3D button). Labels, ranges and
  tooltips of the fourteen existing controls (Task 24's 3D Speed included)
  are copied verbatim from the popup; the Split slider spans 10..90 for a fraction of 0.10..0.90 and its
  tooltip is one plain-English sentence you write.
- Every control drives the bound model's setter and reflects the model's
  signal under `QSignalBlocker`; a model-driven change emits nothing back
  (spy count on the model: one per change).
- Rebind: with `RadioModel m; SpectrumWidget w1, w2; m.setSpectrumWidget(&w1); DisplayApplet a(&m);`
  then `m.setSpectrumWidget(&w2)`, the applet shows `w2.displaySettings()`'s
  values and its controls change `w2`'s model and not `w1`'s. Setting the
  same widget twice emits `spectrumWidgetChanged` once.
- Reset 3D writes the seven defaults (2D, 6, 70, 100, 50, Match, off) to the model.
- Registration: `appletId()` is `"Display"`; `MainWindow` adds the applet
  immediately after the RX applet in `populateDefaultMeter()`'s add order,
  enters it in `m_appletsById`, registers it visible by default, and the
  existing applet-menu tests (grep `tests/` for `registeredIds` or
  `AppletVisibilityController`) still pass; extend the one that enumerates
  ids to include `"Display"`.
- Human smoke, pending until the operator looks: the applet renders in the
  left panel in the house style beside its siblings; every control moves the
  live display; the popup and Setup follow it and it follows them. Evidence
  is a screenshot of the running app taken by the controller, not by this
  task.

**Verification:** ordinary feature tier plus the human smoke above. Unit,
gui label: `tst_display_applet`; siblings once:
`tst_display_settings_binding`, the applet-menu test found above,
`tst_rade_applet`. `RadioModel.h` is a hub header; expect a long rebuild
once.

**Execution note (advisory):** sonnet. Requires Tasks 18, 20, 21 and 24
(runs after Task 24 by decision of 2026-09-20 so the applet is built once
with fifteen controls). Not parallelisable (MainWindow and the CMake lists).

- [ ] **Step 1: `RadioModel::spectrumWidgetChanged`** as in Interfaces, with
  a two-line test appended to `tests/tst_display_applet.cpp` (same widget
  twice emits once).
- [ ] **Step 2: tests first** for the applet per Acceptance, patterned on
  `tests/tst_rade_applet.cpp`; register; red run recorded.
- [ ] **Step 3: the applet.** Build the three sections with the inherited
  helpers, `GuardedSlider` / `GuardedComboBox` styled with
  `applyComboStyle`, tooltips on every control. `bindTo()` deletes and
  recreates `m_bindContext`, connects the fifteen model signals to
  reflectors and the fifteen controls to the model's setters, then refreshes
  every control under `QSignalBlocker`. The constructor binds to
  `model->spectrumWidget()->displaySettings()` when a widget exists and
  connects `RadioModel::spectrumWidgetChanged` to rebind. `syncFromModel()`
  refreshes from `m_bound`.
- [ ] **Step 4: wire into MainWindow and CMake**, per the scout checklist
  (`populateDefaultMeter()`, `m_appletsById`, `registerApplet`), no menu code.
- [ ] **Step 5: prove and commit** as in Task 18 Step 6. Then the controller
  builds, relaunches the app and screenshots the applet for the operator.

## Task 23: persistence moves onto the model

**Requirements:** Task 17's "absorb persistence here too"; decision 3 above.
The settings file is byte-identical before and after for every one of the
fourteen (the fourteen of Task 17 plus Task 24's `Display3DSpeed`) per-pan keys, on pan 0 and on a pan that inherits from pan 0.

**Files:**
- Modify: `src/gui/SpectrumWidget.cpp` (`loadSettings()` and `saveSettings()`)
- Create: `tests/tst_display_settings_golden.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `DisplaySettingsModel::load()` / `save()` as landed;
  Task 18's binding.
- Produces: nothing new. `loadSettings()` obtains the fourteen values by
  calling `m_displaySettings->load()` and copying the model's getters into
  the members (direct member assignment, no `update()`, no save scheduled);
  `saveSettings()` calls `syncDisplaySettingsFromWidget()` then
  `m_displaySettings->save()` instead of writing the fourteen keys itself.
  3D Floor stays out of both, as today.

**Acceptance:**
- Golden: the test is written and run green against the pre-change code
  first. It sets all fourteen values to non-defaults through the widget's
  setters, calls `saveSettings()`, and compares the stored string of every
  key (the fourteen exact key names from the scout table, pan 0) with
  literal expected strings; then it sets `panIndex` 2 with no own keys and
  proves `loadSettings()` inherits pan 0's values; then it saves on pan 2
  and proves the `_2` suffixed keys hold pan 2's strings while pan 0's are
  untouched. The same test, unchanged, is green after the change.
- `loadSettings()` schedules no save and performs no apply
  (`displaySettingsApplyCountForTest()` stays 0), and the model holds the
  loaded values.
- `tst_dss_persistence`, `tst_display_settings_binding` and
  `tst_display_settings_model` green.

**Verification:** behaviour-preserving refactor tier: baseline first (the
golden test green on the old code, in its own commit), then the change.
Unit, gui label. Inner loop on `tst_display_settings_golden`; siblings once
as listed.

**Execution note (advisory):** sonnet. Requires Tasks 18 and 24; last in the order. Two commits: the
golden test, then the refactor. Not parallelisable (same file).

- [ ] **Step 1: golden test, committed alone**, green on the current code.
- [ ] **Step 2: the refactor**, then the golden test again, siblings, signed
  commit with the branch assertion from Task 18 Step 6.

## Task 24: 3D Speed, a row divider matched to the waterfall

**Requirements:** the operator's observations, verbatim: 2026-09-19 "The 3d
moves fast would be nice to have a way to adjust speed?" and "ok but the 2d
waterfalll and the 3dwterfall move at really diffrent speeds"; 2026-09-20
"what i am seeing is the 3db view apears to flow faster then the vertical
waterfall maybe it is because there is 'less space' but it feel like it is
moving faster", and his approval of option 1: automatic match with a
manual override, peak-hold fold. Design §4.5 (added by this task), §6, §7,
§8 item 9. NereusSDR-original: upstream at 1872028c has no row divider, no
3D-specific cadence control, and ties its 3D scroll to the same clock as
its 2D waterfall (crew workspace `scout-scroll-speed.md`, section 5). No
upstream code is lifted, so no new attribution rows are needed; the cite
rule for `// From AetherSDR` comments does not apply to text you author.

**Why the mismatch exists (from `scout-scroll-speed.md`):** one waterfall
tick pushes the same row into both panes (`SpectrumWidget::pushWaterfallRow`
tees into `pushDssRow()` below the stop-on-TX gate). The waterfall keeps
one row per pixel of `m_waterfall.height()`; the 3D ring keeps
`kDssVisibleRows` (96). The glide clock advances
`m_dssScrollProgressRows` by `deltaMs / m_wfUpdatePeriodMs` per display
tick and `pushDssRow()` resets it to 0. Nothing decimates, skips or folds
rows anywhere.

**Files:**
- Modify: `docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md`
  (new §4.5; rows in §6, §6.2, §7; item 9 in §8), committed first, alone
- Modify: `src/models/DisplaySettingsModel.h` and `.cpp` (fifteenth value)
- Modify: `src/gui/SpectrumWidget.h` and `.cpp` (divider, fold, glide, key,
  binding, seams)
- Modify: `src/gui/SpectrumOverlayMenu.h` and `.cpp` (3D Speed row)
- Modify: `src/gui/setup/DisplaySetupPages.h` and `.cpp` (3D Speed row on
  `Display3DSetupPage`)
- Create: `tests/tst_dss_row_divider.cpp`
- Modify: `tests/tst_display_settings_model.cpp`, `tests/tst_dss_overlay_menu.cpp`,
  `tests/tst_dss_setup_sync.cpp`, `tests/tst_dss_persistence.cpp`,
  `tests/tst_display_settings_binding.cpp` (one case each for the new value)
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 18's binding (`bindDisplaySettings()`,
  `syncDisplaySettingsFromWidget()`, `displaySettings()`), Task 20's
  popup-to-model wiring, Task 21's Setup-to-model wiring; existing seams
  `pushWaterfallRowForTest(const QVector<float>&)`, `dssRowsPushedForTest()`,
  `m_txActiveForTest`; `DssRenderer::pushRow` / `pushRowWithWide` as they
  are; `kDssVisibleRows` from `src/gui/DssGeometry.h`.
- Produces, on `DisplaySettingsModel`: `int dssRowDivider() const;`
  `void setDssRowDivider(int n);` `signals: void dssRowDividerChanged(int n);`
  member `int m_dssRowDivider{0};` clamp 0..10 (0 means Match, automatic);
  `load()` reads and `save()` writes key `Display3DSpeed` through
  `settingsKeyFor()`, default 0.
- Produces, on `SpectrumWidget`: `int dssRowDivider() const;`
  `void setDssRowDivider(int n);` `signals: void dssRowDividerChanged(int n);`
  `int effectiveDssRowDivider() const;` `static constexpr int kDssMaxAutoRowDivider = 64;`
  private `void accumulateDssRow(const QVector<float>& wfPixelsDbm);`
  members `int m_dssRowDivider{0}; QVector<float> m_dssFoldRow; QVector<float> m_dssFoldFullBins; int m_dssFoldCount{0};`
  seams `int effectiveDssRowDividerForTest() const;` `int dssFoldCountForTest() const;`
  `float dssScrollIncrementForTest(int deltaMs) const;`
  `loadSettings()` / `saveSettings()` handle `Display3DSpeed` exactly like
  `Display3DAngle`.
- Produces, on `SpectrumOverlayMenu`: a slider row labelled `3D Speed`
  after 3D Angle in the `3D VIEW` section, `QSlider* m_dssSpeedSlider`
  range 0..10, value text `Match` at 0 and `1:N` otherwise, signal
  `dssRowDividerChanged(int)`; `setDssValues(...)` gains a trailing
  `int rowDivider` parameter; the popup's reset path (if it has one) sets 0.
- Produces, on `Display3DSetupPage`: `QSlider* m_speedSlider` with the same
  label, range, value text and tooltip, bound to the model like its
  siblings; Reset sets 0.
- Tooltip, used verbatim on both surfaces: `How many waterfall rows each 3D row covers. Match keeps the 3D history the same length in time as the waterfall. 1:1 pushes every row, the fastest look. Higher values fold more rows into each 3D row, keeping peaks, so the surface recedes more slowly.`

**Behaviour, exactly:**
- `effectiveDssRowDivider()`: if `m_dssRowDivider > 0` return it; else
  return `std::clamp(qRound(double(m_waterfall.height()) / kDssVisibleRows), 1, kDssMaxAutoRowDivider)`
  (a null or zero-height waterfall gives 1). Read live on every use, so a
  resize or split change is followed with no hook.
- In `pushWaterfallRow`, the tee `pushDssRow(wfPixelsDbm)` becomes
  `accumulateDssRow(wfPixelsDbm)`. `accumulateDssRow`: if `m_dssFoldCount == 0`
  or `wfPixelsDbm.size() != m_dssFoldRow.size()` or
  `m_lastFullBinsDbm.size() != m_dssFoldFullBins.size()`, start a fresh
  fold by copying both rows and setting the count to 1; otherwise take the
  per-column maximum into `m_dssFoldRow` and `m_dssFoldFullBins` and
  increment the count. Then if `m_dssFoldCount >= effectiveDssRowDivider()`,
  push: `pushDssRow` must build its wide row from `m_dssFoldFullBins` (not
  `m_lastFullBinsDbm`) and its exact row from `m_dssFoldRow`, then the
  count resets to 0. A size mismatch discards the partial fold; a divider
  lowered below the current count pushes on the next tick.
- The glide: in the display-timer lambda, the period becomes
  `qMax(1, m_wfUpdatePeriodMs) * effectiveDssRowDivider()`; factor the
  increment into a const member `float dssScrollIncrement(int deltaMs) const`
  returning `deltaMs / float(period)`, used by the lambda and exposed by the
  seam. `pushDssRow` still resets `m_dssScrollProgressRows` to 0.
- Entering or leaving 3D (wherever `setSpectrumRenderMode` clears the ring)
  also resets `m_dssFoldCount` to 0 and clears both fold rows.
- `setDssRowDivider(int n)`: clamp 0..10; equality early-return; assign;
  `update()`; `scheduleSettingsSave()`; emit. Bound both ways to the model
  in `bindDisplaySettings()` like the other 3D values.

**Acceptance (all in `tests/tst_dss_row_divider.cpp` unless named):**
- `manualDivider_pushesOneRowPerNTicks`: 3D mode, widget shown and
  exposed, divider 5, twelve rows through `pushWaterfallRowForTest`:
  `dssRowsPushedForTest()` is 2 and `dssFoldCountForTest()` is 2 (rows 11
  and 12 pending).
- `divider1_matchesTodaysBehaviour`: divider 1, twelve rows: twelve pushes.
- `foldIsPeakHold`: divider 3, ring empty (fresh 3D entry), rows of -100
  everywhere except the second row, which carries -40 across three
  adjacent columns c-1..c+1: after the third row the ring's newest row
  reads at least -45 at column c (read it through an existing
  `DssRenderer` row accessor, or add `const QVector<float>& newestRowForTest() const`
  to `DssRenderer`; three columns, not one, because `smoothDssRow`'s
  median-of-3 removes a single-column spike, and state in the report which
  accessor you used and the exact value observed). This case goes red when
  the per-column maximum is replaced by "last row wins"; report the red run.
- `autoDivider_tracksWaterfallHeight`: divider 0; resize so the waterfall
  is tall, then short; each time `effectiveDssRowDividerForTest()` equals
  `std::clamp(qRound(double(h) / 96), 1, 64)` where `h` is the waterfall
  image height read through an existing or new `waterfallHeightForTest()`
  seam, and the two heights chosen must give two different dividers, one
  of them greater than 1.
- `glideIncrementScalesWithDivider`: period 30 ms via `setWfUpdatePeriodMs(30)`;
  divider 5: `dssScrollIncrementForTest(150)` is 1.0 within 1e-6 and
  `dssScrollIncrementForTest(75)` is 0.5; divider 1:
  `dssScrollIncrementForTest(30)` is 1.0. Red when the divider factor is
  dropped; report it.
- `partialFold_restartsOnSizeChange`: divider 4; two rows of width 100
  then one of width 120: `dssFoldCountForTest()` is 1 and nothing pushed.
- `dividerLoweredMidFold_pushesOnNextTick`: divider 10, six rows, then
  divider 3, one more row: one push, count 0.
- `stopOnTx_pausesFold`: with stop-on-TX enabled and `m_txActiveForTest`
  true, rows advance neither the waterfall write row nor the fold count;
  prove presence first by pushing with TX inactive in the same case.
- `enteringAndLeaving3D_resetsFold`: divider 4, two rows, switch to 2D and
  back: count 0.
- `tst_display_settings_model`: `setDssRowDivider` clamps to 0..10, is
  equality-guarded, emits once, and `Display3DSpeed` round-trips through
  `load()` / `save()` for pan 0 and inherits for pan 2.
- `tst_display_settings_binding`: model `setDssRowDivider(7)` reaches
  `dssRowDivider()` on the widget and back, one emission each way.
- `tst_dss_overlay_menu`: the `3D VIEW` section has the `3D Speed` row
  with range 0..10, value text `Match` at 0 and `1:7` at 7, the tooltip
  above verbatim, and moving it emits `dssRowDividerChanged(7)` once;
  `setDssValues(...)` with `rowDivider` 3 shows `1:3`.
- `tst_dss_setup_sync`: the page's speed slider follows the model and
  drives it, one emission; Reset puts 0 on the model.
- `tst_dss_persistence`: `Display3DSpeed` saved and reloaded per pan.
- Human smoke, pending until the operator looks: at Match on his window
  the surface and the waterfall visibly cover the same span of time; at
  1:1 the old fast look returns; a one-tick burst raises a ridge at Match.

**Verification:** ordinary feature tier, unit, gui label. Inner loop on
`tst_dss_row_divider`; siblings once: `tst_display_settings_model`,
`tst_display_settings_binding`, `tst_dss_overlay_menu`, `tst_dss_setup_sync`,
`tst_dss_persistence`, `tst_dss_row_tee`. Tests that push rows must
`resize()`, `show()`, `QVERIFY(QTest::qWaitForWindowExposed(&w))`.

**Execution note (advisory):** sonnet. Requires Tasks 18, 20 and 21; runs
before Task 22 so the applet is built once with fifteen controls. Not
parallelisable (SpectrumWidget, the popup and the Setup pages).

- [ ] **Step 1: design document, committed alone.** Insert after §4.4 in
  `docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md`,
  verbatim:

```markdown
### 4.5 Row cadence: the 3D Speed divider (NereusSDR-original)

Both panes receive one row per waterfall tick (section 4.1). The waterfall
keeps one row per pixel of its height, so its history spans hundreds of
ticks; the 3D ring keeps 96 visible rows (section 3.3). At the default
30 ms period a 528 px waterfall spans about 15.8 s while the 3D surface
spans about 2.9 s, so the surface advances roughly 5.5 times faster than
the waterfall scrolls and each new row moves the front of the surface
several pixels instead of one. The mismatch is inherent in upstream too;
its waterfall tile cadence is slower and it has no control for it.

NereusSDR adds a row divider. The 3D surface consumes one row per N
waterfall ticks; the N rows are folded per column by peak-hold into the
row that is pushed, so a burst that lasts one tick still raises a ridge.
The scroll phase (section 4.4) advances over N ticks instead of one, so
the glide stays continuous. The stop-on-TX gate sits above the fold, so
the fold pauses with the waterfall.

N is automatic by default: the waterfall's pixel height divided by 96,
rounded, at least 1 and at most 64, re-read on every use so window and
split changes are followed without a hook. A 3D Speed control overrides
it: 0 is Match (automatic), 1 pushes every row (the previous behaviour),
2 to 10 fold that many rows into each 3D row. Persisted per panadapter as
`Display3DSpeed`, default 0.

Averaging was rejected as the fold because a one-tick burst would shrink
by a factor of N and could vanish from the surface while still visible in
the waterfall.
```

  In §6's table add the row
  `| 3D Speed | 0 (Match), 1-10 | 0 | same (NereusSDR-original) |`
  after 3D Angle; in §6.2 add `Display3DSpeed` to the per-panadapter key
  list; in §7's table add
  `| One ring row per delivered row | Row divider with peak-hold fold, matched to the waterfall's row count by default | Our waterfall runs one row per pixel at 30 ms; upstream's tile cadence is slower and it has no control |`;
  in §8 add item 9:
  `9. **Row divider.** With divider N, K pushed rows reach the ring floor(K / N) times; the folded row carries the per-column maximum of its N sources; the automatic divider equals the waterfall height over 96, clamped to 1..64; the scroll increment per millisecond is 1 / (period times N).`
  Commit signed, docs only.
- [ ] **Step 2: tests first**, every case above, registered
  alphabetically; red run recorded.
- [ ] **Step 3: model, widget, glide and persistence** per Behaviour and
  Interfaces. Extend `bindDisplaySettings()` and the load and save blocks.
- [ ] **Step 4: popup and Setup rows**, bound to the model through the
  wiring Tasks 20 and 21 left, tooltip verbatim, Reset paths include the
  new value.
- [ ] **Step 5: prove and commit** as in Task 18 Step 6, including the
  two mutation proofs named above.

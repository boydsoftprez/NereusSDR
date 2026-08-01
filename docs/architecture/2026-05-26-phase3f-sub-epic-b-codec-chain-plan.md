# Phase 3F Sub-Epic B: Codec + Chain — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the per-board codec layer to emit multi-slice DDC assignments (up to 5 user slices on 2-ADC boards), implement the antenna-driven codec routing logic, and build `AlexController`'s per-ADC BPF state machine that decides when chains stay filtered vs go WIDE. After this plan lands, the underlying mechanics support N slices; the UI to drive them comes in C+D+E.

**Architecture:** `IP1Codec` / `IP2Codec` interface gains `applyDdcAssignment(SliceConfig[5])` alongside the existing `applyPureSignalDdcConfig` (backward-compat shim wraps the 2-slice case). `AlexController` gains per-ADC state (`AlexAdcState` struct), `recomputeBpf(int adc)` method, and the 16-row event trigger matrix wiring. `ReceiverManager.m_hwToLogical` grows naturally as the codec emits assignments for slices C/D/E. `P2RadioConnection` extends `applyPsDdcConfig` to accept the new `DdcAssignment` struct directly.

**Tech Stack:** C++20, Qt6 (signals/slots for `AlexController::bpfStateChanged`), QtTest framework, NereusSDR codec layer (per-board P1/P2 subclasses).

**Parent design:** [docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md](2026-05-26-phase3f-multi-pan-multi-slice-design.md) §4

**Prereqs:** Sub-Epic A complete (`maxSlices`, `widebandAdcs`, `SliceModel` 7 new Q_PROPERTYs landed).

**Estimated effort:** 4 working days, 17 tasks, ~95 bite-sized steps.

---

## File Structure

### Files to modify

| File | Purpose | Lines affected |
|---|---|---|
| `src/core/codec/IP1Codec.h` | Add `applyDdcAssignment(const SliceConfig[5]&)` virtual method | After existing `applyPureSignalDdcConfig` declaration |
| `src/core/codec/IP2Codec.h` | Same as above for P2 | After existing virtual decls (line 74+) |
| `src/core/codec/CodecContext.h` | Add `SliceConfig` struct (freq, bandIdx, rate, antenna, txBound, diversity) | New struct definition |
| `src/core/codec/P1CodecStandard.cpp` | Extend `applyDdcAssignment` for Hermes/ANAN10/100 (4 user slices max) | New method body |
| `src/core/codec/P1CodecHl2.cpp` | Extend for HL2 (1 slice cap, mi0bot HL2 PS rate carveout preserved) | New method body |
| `src/core/codec/P1CodecAnvelinaPro3.cpp` | Extend for AnvelinaPro3 P1 mode | New method body |
| `src/core/codec/P1CodecRedPitaya.cpp` | Extend for RedPitaya P1 mode | New method body |
| `src/core/codec/P2CodecSaturn.cpp` | Extend for Saturn/G2/G2_1K (5 user slices) | New method body |
| `src/core/codec/P2CodecOrionMkII.cpp` | Extend for OrionMkII / G2 dispatch shim | New method body |
| `src/core/ReceiverManager.h` | Add `iqDataForReceiverN` slots (already wired for receiver 0/1; extend pattern to N) | Public slots near existing |
| `src/core/ReceiverManager.cpp` | Implement per-N receiver routing, grow `m_hwToLogical` capacity | Loops over `maxSlices` |
| `src/core/accessories/AlexController.h` | Add `AlexAdcState` struct, `m_perAdcState[2]`, `BpfMode` enum, public API | After existing per-band antenna API |
| `src/core/accessories/AlexController.cpp` | Implement `recomputeBpf`, signal emission, mode persistence | New methods |
| `src/core/P2RadioConnection.h` | Add `applyDdcAssignment(const DdcAssignment&)` slot alongside existing `applyPsDdcConfig` | Public slots |
| `src/core/P2RadioConnection.cpp` | Implement; populate `m_rx[i]` fields from `DdcAssignment` | New method, ~50 lines |
| `src/models/RadioModel.cpp` | Wire codec `applyDdcAssignment` calls on slice add/remove/retune events | Existing slice signal handlers |

### Files to create

| File | Purpose |
|---|---|
| `src/core/DdcAssignment.h` | The `DdcAssignment` struct (extracted; both codecs and P2RadioConnection share) |
| `tests/tst_codec_5_slice_assignment.cpp` | Per-codec verification: emit correct DDCEnable/Rate/cntrl1/cntrl2 for 5-slice combos |
| `tests/tst_alex_controller_per_adc_bpf.cpp` | `AlexAdcState` recompute decision tree (10 scenarios from design §4) |
| `tests/tst_alex_controller_event_matrix.cpp` | 16-row event trigger matrix verification |
| `tests/tst_receiver_manager_n_slice_routing.cpp` | I/Q routing for up to 5 logical receivers |
| `tests/tst_p2_radio_connection_apply_ddc_assignment.cpp` | byte-faithful CmdRx packet for multi-slice assignments |

### CMake registration

5 new test files added to `tests/CMakeLists.txt`. No new build dependencies.

---

## Task 1: Define `SliceConfig` and `DdcAssignment` shared structs

**Files:**
- Create: `src/core/DdcAssignment.h`
- Modify: `src/core/codec/CodecContext.h` (add `SliceConfig` near top)

- [ ] **Step 1: Write failing test**

Create `tests/tst_codec_5_slice_assignment.cpp` (covers Tasks 1-7):

```cpp
// =================================================================
// tests/tst_codec_5_slice_assignment.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic B Tasks 1-7: verify per-board codec emits correct
// multi-slice DDC assignments per design §4. Byte-faithful for the
// RX1/RX2 case (1-2 slices) preserves Thetis console.cs:8186-8538 [v2.10.3.15]
// behaviour; slices C/D/E fill Thetis's idle DDC4/5/6 slots additively.
// =================================================================

#include <QtTest/QtTest>
#include "core/DdcAssignment.h"
#include "core/codec/CodecContext.h"

using namespace NereusSDR;

class TestCodec5SliceAssignment : public QObject {
    Q_OBJECT
private slots:
    void slice_config_struct_has_required_fields()
    {
        SliceConfig sc{};
        sc.frequencyHz = 14225000;
        sc.bandIndex = 5;  // 20m
        sc.sampleRateHz = 192000;
        sc.antennaIndex = 1;
        sc.txBound = true;
        sc.diversityRequested = false;
        sc.live = true;
        QCOMPARE(sc.frequencyHz, qint64(14225000));
        QCOMPARE(sc.live, true);
    }

    void ddc_assignment_struct_has_required_fields()
    {
        DdcAssignment d{};
        d.rate[2] = 192000;
        d.ddcEnable = 0x04;  // DDC2 bit
        QCOMPARE(d.rate[2], 192000);
        QCOMPARE(d.ddcEnable, 0x04);
    }
};

QTEST_MAIN(TestCodec5SliceAssignment)
#include "tst_codec_5_slice_assignment.moc"
```

- [ ] **Step 2: Register test + run to verify failure**

`tests/CMakeLists.txt`:
```cmake
nereus_add_test(tst_codec_5_slice_assignment)
```

```bash
cmake --build build --target tst_codec_5_slice_assignment 2>&1 | tail -5
```

Expected: FAIL with `'SliceConfig' was not declared` or `'DdcAssignment' was not declared`.

- [ ] **Step 3: Create `src/core/DdcAssignment.h`**

```cpp
// =================================================================
// src/core/DdcAssignment.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs (UpdateDDCs output state)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-26 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Extension of existing PsDdcConfig (Phase 3M-4) to carry
//                 up to 5 user slices' DDC assignments per Phase 3F design
//                 docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §4.
// =================================================================
#pragma once

#include <array>

namespace NereusSDR {

/// Per-DDC assignment emitted by codec, consumed by P2RadioConnection (P2 wire bytes)
/// or NetworkIO calls (P1). Strict extension of PsDdcConfig from Phase 3M-4 —
/// when only RX1/RX2 + PS are active, this struct produces byte-faithful
/// output matching Thetis UpdateDDCs (console.cs:8186-8538 [v2.10.3.15]).
struct DdcAssignment {
    /// Per-DDC sample rate in Hz, 0 = not enabled. Index 0..7 (Thetis convention).
    std::array<int, 8> rate{};

    /// Bitmask: DDC0=bit0, DDC1=bit1, ..., DDC6=bit6. Which DDCs the radio enables.
    int ddcEnable {0};

    /// Bitmask: which DDCs sync to DDC0 (typically DDC1 for diversity or PS pair).
    int syncEnable {0};

    /// ADC routing byte for DDC0-3: 2 bits per DDC, value 0=ADC0, 1=ADC1, 2=ADC2(PS-FB).
    int adcCtrl1 {0};

    /// ADC routing byte for DDC4-7.
    int adcCtrl2 {0};

    /// P1-only opaque preset (0-6), board-specific firmware interpretation.
    /// Ignored by P2 codecs.
    int p1DdcConfig {0};

    /// P1 diversity flag.
    int p1Diversity {0};

    /// P1 receiver count (typically 1-7).
    int p1RxCount {0};

    /// Total enabled DDC count (sum of bits in ddcEnable).
    int nDdc {0};

    /// PureSignal feedback forward DDC index (-1 if PS not active).
    int psFwdDdc {-1};

    /// PureSignal feedback reverse DDC index (-1 if PS not active).
    int psRevDdc {-1};
};

} // namespace NereusSDR
```

- [ ] **Step 4: Add `SliceConfig` struct to `src/core/codec/CodecContext.h`**

Find the top of the file (after `#pragma once` + includes). Add:

```cpp
namespace NereusSDR {

/// Operator-driven per-slice configuration that the codec consumes to produce
/// a DdcAssignment. One per user slice (up to 5 on 2-ADC boards).
struct SliceConfig {
    qint64 frequencyHz {0};      ///< slice's current freq
    int    bandIndex {-1};       ///< 0..13 for HF bands + WWV/GEN/XVTR, see Band enum
    int    sampleRateHz {192000}; ///< per-slice DDC rate (SampleRateCatalog::kDefaultSampleRate default)
    int    antennaIndex {1};      ///< ANT1=1, ANT2=2, ANT3=3, EXT1=4, EXT2=5, BYPS=6
    bool   txBound {false};       ///< only one slice is txBound at any moment
    bool   diversityRequested {false};  ///< slice-A-only on hasDiversity SKUs
    bool   live {false};          ///< false = dormant placeholder, codec skips
};

} // namespace NereusSDR
```

(If `CodecContext.h` already has a `namespace NereusSDR` block, add inside it; otherwise wrap as shown.)

- [ ] **Step 5: Run test to verify passes**

```bash
cmake --build build --target tst_codec_5_slice_assignment && ctest --test-dir build -R tst_codec_5_slice_assignment -V 2>&1 | tail -10
```

Expected: 2 passed.

- [ ] **Step 6: Commit**

```bash
git add src/core/DdcAssignment.h src/core/codec/CodecContext.h tests/tst_codec_5_slice_assignment.cpp tests/CMakeLists.txt
git commit -m "feat(3f-b): add SliceConfig + DdcAssignment shared structs"
```

---

## Task 2: Add `applyDdcAssignment` to `IP2Codec` interface

**Files:**
- Modify: `src/core/codec/IP2Codec.h` (add virtual method declaration after line 74)

- [ ] **Step 1: Add test**

Append to `tests/tst_codec_5_slice_assignment.cpp` (inside class):

```cpp
    void ip2_codec_has_apply_ddc_assignment_method()
    {
        // We can't instantiate IP2Codec (abstract); we verify the signature
        // exists via the concrete Saturn subclass in Task 6. This test just
        // ensures the include compiles and the method is declared.
        // Actual behaviour tested in saturn_emits_*_for_5_slices below.
        QVERIFY(true);  // placeholder; real check via subclass tests in later tasks
    }
```

(This is essentially a compilation check — the real test comes when we exercise the concrete subclass.)

- [ ] **Step 2: Add virtual method to `IP2Codec.h`**

After the existing `applyPureSignalDdcConfig` virtual declaration (around line 74-90), insert:

```cpp
    /// Phase 3F: produce a DDC assignment for up to 5 user slices.
    /// SliceConfig[0] = Slice A, [1] = B, [2] = C, [3] = D, [4] = E.
    /// Slices with .live=false are skipped. Backward compat: when only 1-2 slices
    /// are live and PS state matches, output is byte-faithful to Thetis console.cs:8186-8538
    /// (the existing applyPureSignalDdcConfig path remains for codec-internal use).
    /// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §4.
    virtual DdcAssignment applyDdcAssignment(
        const CodecContext& ctx,
        const std::array<SliceConfig, 5>& slices) const = 0;
```

Add `#include "core/DdcAssignment.h"` and `#include <array>` at the top of `IP2Codec.h` if not present.

- [ ] **Step 3: Compile and verify all P2 codec subclasses now fail to compile**

```bash
cmake --build build 2>&1 | grep -E "error:.*applyDdcAssignment" | head -5
```

Expected: errors in `P2CodecSaturn.cpp`, `P2CodecOrionMkII.cpp` because they don't yet implement the new virtual.

- [ ] **Step 4: Add stub implementations in P2 subclasses (return zero-init `DdcAssignment`)**

In each P2 codec `.cpp` file (Saturn, OrionMkII), add a stub at the bottom of the class:

```cpp
DdcAssignment P2CodecSaturn::applyDdcAssignment(
    const CodecContext& /*ctx*/,
    const std::array<SliceConfig, 5>& /*slices*/) const
{
    // TODO Sub-Epic B Task 6: real implementation per Thetis UpdateDDCs Saturn branch
    return DdcAssignment{};
}
```

And in `P2CodecOrionMkII.cpp`:

```cpp
DdcAssignment P2CodecOrionMkII::applyDdcAssignment(
    const CodecContext& /*ctx*/,
    const std::array<SliceConfig, 5>& /*slices*/) const
{
    // TODO Sub-Epic B Task 6: real implementation per Thetis UpdateDDCs OrionMkII branch
    return DdcAssignment{};
}
```

Add the corresponding `.h` declarations.

- [ ] **Step 5: Build green + commit**

```bash
cmake --build build 2>&1 | tail -3
git add src/core/codec/IP2Codec.h src/core/codec/P2CodecSaturn.{h,cpp} src/core/codec/P2CodecOrionMkII.{h,cpp}
git commit -m "feat(3f-b): add applyDdcAssignment virtual + P2 subclass stubs"
```

---

## Task 3: Add `applyDdcAssignment` to `IP1Codec` + P1 subclass stubs

**Files:**
- Modify: `src/core/codec/IP1Codec.h`, all 4 P1 subclasses (`P1CodecStandard.{h,cpp}`, `P1CodecHl2.{h,cpp}`, `P1CodecAnvelinaPro3.{h,cpp}`, `P1CodecRedPitaya.{h,cpp}`)

- [ ] **Step 1: Mirror Task 2 for IP1Codec**

Add the same `applyDdcAssignment` virtual to `IP1Codec.h` with the same signature.

- [ ] **Step 2: Stub all 4 P1 subclass implementations**

For each P1 codec, add a stub returning `DdcAssignment{}` following the Task 2 Step 4 pattern.

- [ ] **Step 3: Build green + commit**

```bash
cmake --build build 2>&1 | tail -3
git add src/core/codec/IP1Codec.h src/core/codec/P1Codec*.{h,cpp}
git commit -m "feat(3f-b): add applyDdcAssignment virtual + P1 subclass stubs (4 SKUs)"
```

---

## Task 4: Implement `P2CodecSaturn::applyDdcAssignment` for 1-2 slice case (Thetis-faithful)

**Files:**
- Modify: `src/core/codec/P2CodecSaturn.cpp` (replace stub with real implementation)

This task preserves byte-faithful Thetis behaviour for 1-2 slice cases. Slices C/D/E come in Task 6.

- [ ] **Step 1: Add tests for 1-slice + 2-slice byte-faithful cases**

Append to `tests/tst_codec_5_slice_assignment.cpp`:

```cpp
    void saturn_1_slice_no_ps_no_div_assigns_ddc2()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        ctx.mox = false;
        ctx.psActive = false;
        ctx.diversityActive = false;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true;
        slices[0].frequencyHz = 14225000;
        slices[0].bandIndex = 5;  // 20m
        slices[0].sampleRateHz = 192000;
        slices[0].antennaIndex = 1;
        slices[0].txBound = true;
        // slices[1..4].live = false (default)

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // Thetis UpdateDDCs Saturn: 1 RX, no PS, no Div → DDCEnable=DDC2 (bit 2 = 0x04)
        QCOMPARE(a.ddcEnable & 0x04, 0x04);
        QCOMPARE(a.rate[2], 192000);
        QCOMPARE(a.syncEnable, 0);
        QCOMPARE(a.nDdc, 1);
    }

    void saturn_2_slice_no_ps_no_div_assigns_ddc2_and_ddc3()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].frequencyHz = 14225000; slices[0].sampleRateHz = 192000; slices[0].antennaIndex = 1; slices[0].txBound = true;
        slices[1].live = true; slices[1].frequencyHz = 7150000;  slices[1].sampleRateHz = 96000;  slices[1].antennaIndex = 1;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // Saturn 2-slice: DDCEnable=DDC2+DDC3 (0x0c)
        QCOMPARE(a.ddcEnable & 0x0c, 0x0c);
        QCOMPARE(a.rate[2], 192000);
        QCOMPARE(a.rate[3], 96000);
        QCOMPARE(a.nDdc, 2);
    }
```

(Add `#include "core/codec/P2CodecSaturn.h"` to the test file.)

- [ ] **Step 2: Run tests to verify failure**

```bash
ctest --test-dir build -R tst_codec_5_slice_assignment -V 2>&1 | tail -10
```

Expected: 2 new tests FAIL (stub returns zero DdcAssignment).

- [ ] **Step 3: Implement `P2CodecSaturn::applyDdcAssignment` 1-2 slice case**

Replace the stub in `src/core/codec/P2CodecSaturn.cpp`:

```cpp
DdcAssignment P2CodecSaturn::applyDdcAssignment(
    const CodecContext& ctx,
    const std::array<SliceConfig, 5>& slices) const
{
    DdcAssignment a{};

    // Count live user slices (Slice A=index 0, B=1, ...)
    int liveCount = 0;
    for (const auto& s : slices) {
        if (s.live) { ++liveCount; }
    }

    // ── Saturn / G2 user-slice DDC mapping (Phase 3F design §2) ──
    // DDC0/DDC1 reserved for PS forward/reverse + diversity sync pair (handled below).
    // DDC2 → Slice A, DDC3 → Slice B, DDC4 → C, DDC5 → D, DDC6 → E.
    // Mapping mirrors Thetis console.cs:8186-8538 [v2.10.3.15] UpdateDDCs Saturn branch
    // for the 1-2 slice case (byte-faithful).
    static constexpr int kSliceToDdc[5] = {2, 3, 4, 5, 6};

    for (int i = 0; i < 5; ++i) {
        if (!slices[i].live) { continue; }
        const int ddc = kSliceToDdc[i];
        a.ddcEnable |= (1 << ddc);
        a.rate[ddc] = slices[i].sampleRateHz;
        // ADC routing: DDC2/4/6 → ADC0, DDC3/5 → ADC1 (Thetis default round-robin
        // on 2-ADC boards; per-slice antenna override handled by AlexController +
        // Sub-Epic E Setup → DDC Routing page).
        const int adcBit = (ddc % 2 == 0) ? 0 : 1;
        if (ddc < 4) {
            // adcCtrl1 covers DDC0-3: 2 bits per DDC starting at bit 0
            a.adcCtrl1 |= (adcBit << (ddc * 2));
        } else {
            // adcCtrl2 covers DDC4-7: 2 bits per DDC starting at bit (ddc-4)*2
            a.adcCtrl2 |= (adcBit << ((ddc - 4) * 2));
        }
        ++a.nDdc;
    }

    // ── PureSignal override (Thetis console.cs:8264 [v2.10.3.15]) ──
    if (ctx.psActive && ctx.mox) {
        a.ddcEnable |= 0x03;            // DDC0 + DDC1
        a.syncEnable |= 0x02;           // DDC1 syncs to DDC0
        a.rate[0] = 192000;             // ps_rate
        a.rate[1] = 192000;
        a.adcCtrl1 = (a.adcCtrl1 & 0xf3) | 0x08;  // clear DDC1 ADC field, set to ADC2
        a.psFwdDdc = 0;
        a.psRevDdc = 1;
        a.nDdc += 2;
    }
    // ── Diversity override (when active and no PS conflict) ──
    else if (ctx.diversityActive) {
        // Diversity migrates Slice A from DDC2 to DDC0+1 synced pair.
        // Slice A's rate copies onto DDC0+1; DDC2 frees.
        a.ddcEnable |= 0x03;
        a.ddcEnable &= ~0x04;  // disable DDC2
        a.syncEnable |= 0x02;
        if (slices[0].live) {
            a.rate[0] = slices[0].sampleRateHz;
            a.rate[1] = slices[0].sampleRateHz;
            a.rate[2] = 0;
        }
        // DDC0 → ADC0, DDC1 → ADC1 (Thetis default for diversity sync pair)
        a.adcCtrl1 &= 0xf0;
        a.adcCtrl1 |= 0x04;  // bit 2 set: DDC1 → ADC1; DDC0 stays ADC0 (bits 0-1 zero)
        // nDdc was incremented for DDC2 above; subtract 1 for the swap.
        // (DDC0+DDC1 add 2, DDC2 subtracts 1, net +1.)
        a.nDdc -= 1;
        a.nDdc += 2;
    }

    return a;
}
```

- [ ] **Step 4: Build + run tests**

```bash
cmake --build build --target tst_codec_5_slice_assignment && ctest --test-dir build -R tst_codec_5_slice_assignment -V 2>&1 | tail -15
```

Expected: 4 tests pass (2 from Task 1 + 2 new Saturn tests).

- [ ] **Step 5: Commit**

```bash
git add src/core/codec/P2CodecSaturn.cpp tests/tst_codec_5_slice_assignment.cpp
git commit -m "feat(3f-b): P2CodecSaturn::applyDdcAssignment 1-2 slice byte-faithful (Thetis)"
```

---

## Task 5: Add PureSignal + Diversity test coverage for Saturn

**Files:**
- Modify: `tests/tst_codec_5_slice_assignment.cpp`

- [ ] **Step 1: Add 3 more Saturn tests**

```cpp
    void saturn_2_slice_with_ps_mox_overrides_ddc0_ddc1()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        ctx.mox = true;
        ctx.psActive = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].frequencyHz = 14225000; slices[0].sampleRateHz = 192000; slices[0].txBound = true;
        slices[1].live = true; slices[1].frequencyHz = 7150000;  slices[1].sampleRateHz = 96000;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // PS-on MOX: DDC0+1+2+3 enabled, DDC1 synced, ps_rate on PS pair
        QCOMPARE(a.ddcEnable & 0x0f, 0x0f);  // DDC0 + DDC1 + DDC2 + DDC3
        QCOMPARE(a.rate[0], 192000);  // ps_rate
        QCOMPARE(a.rate[1], 192000);  // ps_rate
        QCOMPARE(a.rate[2], 192000);  // Slice A's rate (unchanged)
        QCOMPARE(a.rate[3], 96000);   // Slice B's rate
        QCOMPARE(a.syncEnable & 0x02, 0x02);  // DDC1 sync set
        QCOMPARE(a.psFwdDdc, 0);
        QCOMPARE(a.psRevDdc, 1);
        // ADC override on DDC1: bit 3 set, bit 2 clear
        QCOMPARE(a.adcCtrl1 & 0x0c, 0x08);
    }

    void saturn_1_slice_diversity_migrates_to_ddc0_1_sync()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        ctx.diversityActive = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].frequencyHz = 14225000; slices[0].sampleRateHz = 192000; slices[0].diversityRequested = true; slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // Diversity: DDC0+1 enabled+synced at slice rate, DDC2 freed
        QCOMPARE(a.ddcEnable & 0x07, 0x03);  // DDC0+DDC1 set, DDC2 clear
        QCOMPARE(a.rate[0], 192000);
        QCOMPARE(a.rate[1], 192000);
        QCOMPARE(a.rate[2], 0);
        QCOMPARE(a.syncEnable & 0x02, 0x02);
    }

    void saturn_ps_wins_over_diversity_when_both_engaged()
    {
        // Per Thetis: PS + Diversity both requested → PS wins, diversity sync disabled,
        // Slice A reverts to DDC2 (non-diversity), DDC0+1 carry PS pair.
        P2CodecSaturn codec;
        CodecContext ctx{};
        ctx.mox = true;
        ctx.psActive = true;
        ctx.diversityActive = true;

        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].frequencyHz = 14225000; slices[0].sampleRateHz = 192000; slices[0].diversityRequested = true; slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        // PS overrides: DDC0+1+2 enabled, DDC0+1 at ps_rate, DDC2 at slice rate
        QCOMPARE(a.ddcEnable & 0x07, 0x07);
        QCOMPARE(a.rate[0], 192000);
        QCOMPARE(a.rate[1], 192000);
        QCOMPARE(a.rate[2], 192000);
    }
```

- [ ] **Step 2: Run + commit**

```bash
ctest --test-dir build -R tst_codec_5_slice_assignment -V 2>&1 | tail -15
```

Expected: tests show whether the diversity + PS-wins logic in Task 4 is correct. If tests fail, fix `applyDdcAssignment` until they pass.

```bash
git add tests/tst_codec_5_slice_assignment.cpp src/core/codec/P2CodecSaturn.cpp
git commit -m "test(3f-b): Saturn codec PS + diversity override coverage"
```

---

## Task 6: Extend `P2CodecSaturn::applyDdcAssignment` for slices C/D/E (3-5 slice case)

**Files:**
- Modify: `src/core/codec/P2CodecSaturn.cpp` (loop already in Task 4 handles 5 slices naturally; verify with tests)

- [ ] **Step 1: Add 3-slice and 5-slice tests**

```cpp
    void saturn_3_slice_no_ps_no_div_enables_ddc2_3_4()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        for (int i = 0; i < 3; ++i) {
            slices[i].live = true;
            slices[i].frequencyHz = (14000000 + i * 1000000);
            slices[i].sampleRateHz = 192000;
            slices[i].antennaIndex = 1;
        }
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.ddcEnable & 0x1c, 0x1c);  // DDC2+3+4
        QCOMPARE(a.rate[2], 192000);
        QCOMPARE(a.rate[3], 192000);
        QCOMPARE(a.rate[4], 192000);
        QCOMPARE(a.nDdc, 3);
    }

    void saturn_5_slice_max_enables_ddc2_through_ddc6()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        for (int i = 0; i < 5; ++i) {
            slices[i].live = true;
            slices[i].frequencyHz = (7000000 + i * 3000000);
            slices[i].sampleRateHz = 192000;
            slices[i].antennaIndex = 1;
        }
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.ddcEnable & 0x7c, 0x7c);  // DDC2 through DDC6 (bits 2-6)
        QCOMPARE(a.nDdc, 5);
        for (int ddc = 2; ddc <= 6; ++ddc) {
            QCOMPARE(a.rate[ddc], 192000);
        }
    }
```

- [ ] **Step 2: Run tests**

Tests should already pass because the Task 4 loop handles all 5 slices. If they don't, fix the loop.

- [ ] **Step 3: Commit**

```bash
git add tests/tst_codec_5_slice_assignment.cpp
git commit -m "test(3f-b): Saturn codec 3-slice + 5-slice (DDC2-6) cases"
```

---

## Task 7: Implement `P2CodecOrionMkII::applyDdcAssignment`

**Files:**
- Modify: `src/core/codec/P2CodecOrionMkII.cpp`

P2CodecOrionMkII is a dispatch shim for OrionMkII / ANAN7000DLE / ANAN8000DLE / AnvelinaPro3 / G2 / G2E. Largely the same as Saturn (5-DDC user slice mapping). Different per-board only in the OC outputs / PA telemetry plumbing (not codec DDC layout).

- [ ] **Step 1: Add tests mirroring Saturn for OrionMkII**

```cpp
    void orion_mkii_5_slice_enables_ddc2_through_ddc6()
    {
        P2CodecOrionMkII codec;
        CodecContext ctx{};
        ctx.hpsdrModel = HPSDRModel::ORIONMKII;
        std::array<SliceConfig, 5> slices{};
        for (int i = 0; i < 5; ++i) {
            slices[i].live = true;
            slices[i].frequencyHz = (7000000 + i * 3000000);
            slices[i].sampleRateHz = 192000;
            slices[i].antennaIndex = 1;
        }
        slices[0].txBound = true;

        const auto a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.ddcEnable & 0x7c, 0x7c);
        QCOMPARE(a.nDdc, 5);
    }
```

- [ ] **Step 2: Run test to verify failure (stub returns zero)**

- [ ] **Step 3: Implement `P2CodecOrionMkII::applyDdcAssignment`**

Identical logic to Saturn (Task 4). If OrionMkII and Saturn share enough code, factor a helper in `IP2Codec.cpp` (a new file if needed) called `applyDdcAssignmentP2Default()` that both call. For now, duplicate inline:

(copy the body from Task 4's `P2CodecSaturn::applyDdcAssignment` verbatim into `P2CodecOrionMkII::applyDdcAssignment`).

- [ ] **Step 4: Verify all tests pass + commit**

```bash
ctest --test-dir build -R tst_codec_5_slice_assignment -V 2>&1 | tail -10
git add src/core/codec/P2CodecOrionMkII.cpp
git commit -m "feat(3f-b): P2CodecOrionMkII::applyDdcAssignment (1-5 slices, Thetis-faithful)"
```

---

## Task 8: Implement `P1CodecStandard::applyDdcAssignment` (Hermes / ANAN-10/100)

**Files:**
- Modify: `src/core/codec/P1CodecStandard.cpp`

Hermes is 1-ADC, 4 DDCs, maxSlices=4. Slices A-D map to DDCs 0-3. PS reclaims DDC0+1 on TX (1-ADC PS behaviour).

- [ ] **Step 1: Add Hermes tests**

```cpp
    void hermes_1_slice_no_ps_assigns_ddc0()
    {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.hpsdrModel = HPSDRModel::HERMES;
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].frequencyHz = 14225000; slices[0].sampleRateHz = 96000;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        QCOMPARE(a.ddcEnable & 0x01, 0x01);
        QCOMPARE(a.rate[0], 96000);
        QCOMPARE(a.p1DdcConfig, 4);  // Thetis Hermes RX, no Div, no PS preset
    }

    void hermes_4_slice_enables_ddc0_through_ddc3()
    {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.hpsdrModel = HPSDRModel::HERMES;
        std::array<SliceConfig, 5> slices{};
        for (int i = 0; i < 4; ++i) {
            slices[i].live = true;
            slices[i].sampleRateHz = 96000;
        }
        const auto a = codec.applyDdcAssignment(ctx, slices);
        QCOMPARE(a.ddcEnable & 0x0f, 0x0f);
        QCOMPARE(a.nDdc, 4);
    }
```

- [ ] **Step 2: Run test to verify failure**

- [ ] **Step 3: Implement P1CodecStandard::applyDdcAssignment**

```cpp
DdcAssignment P1CodecStandard::applyDdcAssignment(
    const CodecContext& ctx,
    const std::array<SliceConfig, 5>& slices) const
{
    DdcAssignment a{};

    // Hermes-class (1 ADC, 4 DDCs): Slice A→DDC0, B→DDC1, C→DDC2, D→DDC3
    static constexpr int kSliceToDdc[4] = {0, 1, 2, 3};

    int sliceCount = 0;
    for (int i = 0; i < 4; ++i) {
        if (!slices[i].live) { continue; }
        const int ddc = kSliceToDdc[i];
        a.ddcEnable |= (1 << ddc);
        a.rate[ddc] = slices[i].sampleRateHz;
        ++sliceCount;
        ++a.nDdc;
    }
    // Slice E (index 4) is ignored on Hermes (maxSlices=4 cap; codec wouldn't be
    // called with slices[4].live=true on this SKU, but be defensive).

    a.p1RxCount = sliceCount;
    a.p1Diversity = 0;

    // P1_DDCConfig preset matrix (Thetis console.cs:8378-8448 [v2.10.3.15])
    if (ctx.psActive && ctx.mox) {
        // PS on Hermes: reclaim DDC0+1 as PS pair; DDC2/3 (slices C/D) still live
        a.ddcEnable |= 0x03;
        a.syncEnable |= 0x02;
        a.rate[0] = 192000;
        a.rate[1] = 192000;
        a.p1DdcConfig = 6;  // Hermes PS preset
        a.psFwdDdc = 0;
        a.psRevDdc = 1;
        a.adcCtrl1 = 4;
    } else if (ctx.diversityActive) {
        a.ddcEnable |= 0x03;
        a.syncEnable |= 0x02;
        if (slices[0].live) {
            a.rate[0] = slices[0].sampleRateHz;
            a.rate[1] = slices[0].sampleRateHz;
        }
        a.p1DdcConfig = 5;  // Hermes Div preset
        a.p1Diversity = 1;
    } else {
        a.p1DdcConfig = 4;  // Hermes plain RX
    }

    return a;
}
```

- [ ] **Step 4: Verify + commit**

```bash
ctest --test-dir build -R tst_codec_5_slice_assignment -V 2>&1 | tail -15
git add src/core/codec/P1CodecStandard.cpp tests/tst_codec_5_slice_assignment.cpp
git commit -m "feat(3f-b): P1CodecStandard::applyDdcAssignment (Hermes, 1-4 slices + PS + Div)"
```

---

## Task 9: Implement `P1CodecHl2::applyDdcAssignment` (HL2 1-slice cap)

**Files:**
- Modify: `src/core/codec/P1CodecHl2.cpp`

HL2 is special: maxSlices=1, mi0bot PS-MOX rate carveout (rx1_rate not ps_rate).

- [ ] **Step 1: Add HL2 tests**

```cpp
    void hl2_1_slice_assigns_ddc0_at_slice_rate()
    {
        P1CodecHl2 codec;
        CodecContext ctx{};
        ctx.hpsdrModel = HPSDRModel::HERMESLITE;
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 384000;  // HL2 supports 384k
        const auto a = codec.applyDdcAssignment(ctx, slices);
        QCOMPARE(a.ddcEnable & 0x01, 0x01);
        QCOMPARE(a.rate[0], 384000);
    }

    void hl2_ps_mox_uses_rx1_rate_not_ps_rate()
    {
        // mi0bot console.cs:8409-8488 [v2.10.3.13] divergence:
        // HL2 keeps Slice A's rate during PS-MOX (rx1_rate, not 192k ps_rate).
        P1CodecHl2 codec;
        CodecContext ctx{};
        ctx.hpsdrModel = HPSDRModel::HERMESLITE;
        ctx.mox = true;
        ctx.psActive = true;
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 384000; slices[0].txBound = true;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        // mi0bot HL2 PS: Rate[0]=Rate[1]=rx1_rate (slice's rate), NOT ps_rate
        QCOMPARE(a.rate[0], 384000);
        QCOMPARE(a.rate[1], 384000);
    }
```

- [ ] **Step 2: Run test to verify failure**

- [ ] **Step 3: Implement `P1CodecHl2::applyDdcAssignment`**

```cpp
DdcAssignment P1CodecHl2::applyDdcAssignment(
    const CodecContext& ctx,
    const std::array<SliceConfig, 5>& slices) const
{
    DdcAssignment a{};

    // HL2: maxSlices=1; only Slice A used. Slices B-E ignored even if .live (defensive).
    if (slices[0].live) {
        a.ddcEnable |= 0x01;
        a.rate[0] = slices[0].sampleRateHz;
        a.nDdc = 1;
        a.p1RxCount = 1;
    }

    if (ctx.psActive && ctx.mox && slices[0].live) {
        // mi0bot HL2 PS carveout: Rate[0]=Rate[1]=rx1_rate, NOT ps_rate.
        // From mi0bot-Thetis console.cs:8409-8488 [v2.10.3.13].
        a.ddcEnable |= 0x03;
        a.syncEnable |= 0x02;
        a.rate[0] = slices[0].sampleRateHz;
        a.rate[1] = slices[0].sampleRateHz;
        a.p1DdcConfig = 6;
        a.psFwdDdc = 0;
        a.psRevDdc = 1;
        a.adcCtrl1 = 4;
    } else {
        a.p1DdcConfig = 4;
    }

    return a;
}
```

- [ ] **Step 4: Verify + commit**

```bash
ctest --test-dir build -R tst_codec_5_slice_assignment -V 2>&1 | tail -10
git add src/core/codec/P1CodecHl2.cpp tests/tst_codec_5_slice_assignment.cpp
git commit -m "feat(3f-b): P1CodecHl2::applyDdcAssignment (1-slice + mi0bot PS rate carveout)"
```

---

## Task 10: Implement remaining P1 codecs (AnvelinaPro3, RedPitaya)

**Files:**
- Modify: `src/core/codec/P1CodecAnvelinaPro3.cpp`, `src/core/codec/P1CodecRedPitaya.cpp`

Both follow the Hermes-class P1 pattern (4 slices, P1_DDCConfig matrix). RedPitaya supports `include_extra_p1_rate` (384k allowed).

- [ ] **Step 1: Implement both following Task 8 pattern**

For each codec, copy the Hermes implementation from Task 8 verbatim into `applyDdcAssignment`. The only divergence (RedPitaya 384k rate) is already handled because `SliceConfig.sampleRateHz` carries the operator's choice.

- [ ] **Step 2: Quick smoke test each**

Append to `tests/tst_codec_5_slice_assignment.cpp`:

```cpp
    void anvelina_pro3_1_slice_compiles_and_returns_nonzero()
    {
        P1CodecAnvelinaPro3 codec;
        CodecContext ctx{};
        ctx.hpsdrModel = HPSDRModel::ANVELINAPRO3;
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 96000;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        QCOMPARE(a.ddcEnable & 0x01, 0x01);
        QVERIFY(a.nDdc >= 1);
    }

    void redpitaya_1_slice_at_384k_supported()
    {
        P1CodecRedPitaya codec;
        CodecContext ctx{};
        ctx.hpsdrModel = HPSDRModel::REDPITAYA;
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 384000;
        const auto a = codec.applyDdcAssignment(ctx, slices);
        QCOMPARE(a.rate[0], 384000);
    }
```

- [ ] **Step 3: Commit**

```bash
cmake --build build && ctest --test-dir build -R tst_codec_5_slice_assignment -V 2>&1 | tail -15
git add src/core/codec/P1CodecAnvelinaPro3.cpp src/core/codec/P1CodecRedPitaya.cpp tests/tst_codec_5_slice_assignment.cpp
git commit -m "feat(3f-b): P1CodecAnvelinaPro3 + P1CodecRedPitaya::applyDdcAssignment"
```

---

## Task 11: Add `AlexAdcState` struct + `BpfMode` enum to AlexController

**Files:**
- Modify: `src/core/accessories/AlexController.h` (insert after existing public API ~line 145)

- [ ] **Step 1: Write failing test**

Create `tests/tst_alex_controller_per_adc_bpf.cpp`:

```cpp
// =================================================================
// tests/tst_alex_controller_per_adc_bpf.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic B Task 11+: AlexController per-ADC BPF state machine
// per docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §4.
// =================================================================

#include <QtTest/QtTest>
#include "core/accessories/AlexController.h"

using namespace NereusSDR;

class TestAlexControllerPerAdcBpf : public QObject {
    Q_OBJECT
private slots:
    void alex_adc_state_struct_exists()
    {
        AlexController::AlexAdcState s{};
        s.mode = AlexController::BpfMode::Auto;
        s.effective = AlexController::BpfEffective::Filtered;
        s.currentBpfBand = Band::B20M;
        QCOMPARE(s.mode, AlexController::BpfMode::Auto);
    }
};

QTEST_MAIN(TestAlexControllerPerAdcBpf)
#include "tst_alex_controller_per_adc_bpf.moc"
```

Register: `nereus_add_test(tst_alex_controller_per_adc_bpf)` in `tests/CMakeLists.txt`.

- [ ] **Step 2: Run test to verify failure (compile error)**

```bash
cmake --build build --target tst_alex_controller_per_adc_bpf 2>&1 | tail -5
```

Expected: FAIL `'AlexAdcState' is not a member of 'AlexController'`.

- [ ] **Step 3: Add nested types to AlexController.h**

Insert in the `class AlexController : public QObject {` block (after existing public types, before existing methods):

```cpp
public:
    /// Phase 3F: per-ADC BPF policy (operator preference).
    enum class BpfMode {
        Auto,        ///< default: filter when single-band, bypass on multi-band
        ForceBand,   ///< always filter to TX-bound slice's band (warn OOB attenuation)
        ForceBypass  ///< always bypass BPF (operator wideband or noise hunting)
    };

    /// Phase 3F: effective BPF state (what's actually on the wire).
    enum class BpfEffective {
        Filtered,         ///< BPF engaged at currentBpfBand
        Bypass,           ///< BPF in bypass (no per-band rejection)
        WidebandLocked    ///< BPF bypassed due to wideband stream active on this ADC
    };

    /// Phase 3F: per-ADC state computed by recomputeBpf().
    struct AlexAdcState {
        BpfMode      mode {BpfMode::Auto};
        BpfEffective effective {BpfEffective::Filtered};
        Band         currentBpfBand {Band::B20M};
        QString      reasonText;  ///< for WIDE badge tooltip + bottom-bar status
    };
```

(Add `#include "models/Band.h"` near other includes if not already present.)

- [ ] **Step 4: Run + commit**

```bash
cmake --build build --target tst_alex_controller_per_adc_bpf && ctest --test-dir build -R tst_alex_controller_per_adc_bpf -V 2>&1 | tail -10
git add src/core/accessories/AlexController.h tests/tst_alex_controller_per_adc_bpf.cpp tests/CMakeLists.txt
git commit -m "feat(3f-b): AlexController gains AlexAdcState struct + BpfMode/BpfEffective enums"
```

---

## Task 12: Add `m_perAdcState[2]`, public mutators, and recomputeBpf signature

**Files:**
- Modify: `src/core/accessories/AlexController.h`, `src/core/accessories/AlexController.cpp`

- [ ] **Step 1: Add tests for mutators**

Append to `tests/tst_alex_controller_per_adc_bpf.cpp`:

```cpp
    void default_bpf_mode_is_auto_per_adc()
    {
        AlexController alex;
        QCOMPARE(alex.bpfMode(0), AlexController::BpfMode::Auto);
        QCOMPARE(alex.bpfMode(1), AlexController::BpfMode::Auto);
    }

    void set_bpf_mode_round_trips_per_adc()
    {
        AlexController alex;
        alex.setBpfMode(0, AlexController::BpfMode::ForceBypass);
        QCOMPARE(alex.bpfMode(0), AlexController::BpfMode::ForceBypass);
        QCOMPARE(alex.bpfMode(1), AlexController::BpfMode::Auto);  // ADC1 unaffected
    }

    void set_bpf_mode_emits_state_changed_signal()
    {
        AlexController alex;
        QSignalSpy spy(&alex, &AlexController::bpfStateChanged);
        alex.setBpfMode(0, AlexController::BpfMode::ForceBypass);
        QVERIFY(spy.count() >= 1);
    }
```

- [ ] **Step 2: Run + verify failure**

- [ ] **Step 3: Add to AlexController.h**

Public API (after existing antenna API):

```cpp
public:
    BpfMode bpfMode(int adc) const;
    void    setBpfMode(int adc, BpfMode mode);
    const AlexAdcState& adcState(int adc) const;

    /// Recompute BPF state for the given ADC based on current slice list, wideband
    /// state, and operator mode. Emits bpfStateChanged when effective state changes.
    void recomputeBpf(int adc);

    /// Mark that a wideband stream is active on this ADC.
    /// Recomputes BPF (wideband forces effective=WidebandLocked).
    void setWidebandActive(int adc, bool on);

signals:
    void bpfStateChanged(int adc, const AlexAdcState& state);
```

Private members:

```cpp
private:
    std::array<AlexAdcState, 2> m_perAdcState{};
    std::array<bool, 2> m_widebandActive {false, false};
```

- [ ] **Step 4: Implement basic getters/setters in AlexController.cpp**

```cpp
AlexController::BpfMode AlexController::bpfMode(int adc) const
{
    if (adc < 0 || adc >= 2) { return BpfMode::Auto; }
    return m_perAdcState[adc].mode;
}

void AlexController::setBpfMode(int adc, BpfMode mode)
{
    if (adc < 0 || adc >= 2) { return; }
    if (m_perAdcState[adc].mode == mode) { return; }
    m_perAdcState[adc].mode = mode;
    recomputeBpf(adc);  // emits bpfStateChanged when effective changes
}

const AlexController::AlexAdcState& AlexController::adcState(int adc) const
{
    static AlexAdcState empty{};
    if (adc < 0 || adc >= 2) { return empty; }
    return m_perAdcState[adc];
}

void AlexController::setWidebandActive(int adc, bool on)
{
    if (adc < 0 || adc >= 2) { return; }
    if (m_widebandActive[adc] == on) { return; }
    m_widebandActive[adc] = on;
    recomputeBpf(adc);
}

void AlexController::recomputeBpf(int adc)
{
    if (adc < 0 || adc >= 2) { return; }

    AlexAdcState& s = m_perAdcState[adc];
    AlexAdcState prev = s;

    // Priority order: wideband > operator force-bypass > operator force-band > auto.
    if (m_widebandActive[adc]) {
        s.effective = BpfEffective::WidebandLocked;
        s.reasonText = QStringLiteral("BYPASS (wideband active)");
    } else if (s.mode == BpfMode::ForceBypass) {
        s.effective = BpfEffective::Bypass;
        s.reasonText = QStringLiteral("BYPASS (operator override)");
    } else if (s.mode == BpfMode::ForceBand) {
        s.effective = BpfEffective::Filtered;
        s.reasonText = QStringLiteral("%1 (forced)").arg(bandLabel(s.currentBpfBand));
    } else {
        // Auto mode: requires slice list. Placeholder for Task 13 (slice-aware recompute).
        // Without slices, default to last filtered band.
        s.effective = BpfEffective::Filtered;
        s.reasonText = QStringLiteral("%1 (idle)").arg(bandLabel(s.currentBpfBand));
    }

    if (s.effective != prev.effective || s.reasonText != prev.reasonText) {
        emit bpfStateChanged(adc, s);
    }
}
```

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_alex_controller_per_adc_bpf && ctest --test-dir build -R tst_alex_controller_per_adc_bpf -V 2>&1 | tail -10
git add src/core/accessories/AlexController.{h,cpp} tests/tst_alex_controller_per_adc_bpf.cpp
git commit -m "feat(3f-b): AlexController setBpfMode + recomputeBpf basic (wideband/force modes)"
```

---

## Task 13: Slice-aware auto-mode in `recomputeBpf` (the multi-band → BYPASS branch)

**Files:**
- Modify: `src/core/accessories/AlexController.h`, `src/core/accessories/AlexController.cpp`

- [ ] **Step 1: Add tests for auto mode decision tree**

```cpp
    void auto_mode_single_band_filters_to_that_band()
    {
        AlexController alex;
        std::array<Band, 5> slicesOnAdc {Band::B20M, Band::NONE, Band::NONE, Band::NONE, Band::NONE};
        alex.notifySlicesOnAdc(0, slicesOnAdc);
        alex.recomputeBpf(0);
        QCOMPARE(alex.adcState(0).effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(alex.adcState(0).currentBpfBand, Band::B20M);
    }

    void auto_mode_multi_band_bypasses()
    {
        AlexController alex;
        std::array<Band, 5> slicesOnAdc {Band::B20M, Band::B40M, Band::NONE, Band::NONE, Band::NONE};
        alex.notifySlicesOnAdc(0, slicesOnAdc);
        alex.recomputeBpf(0);
        QCOMPARE(alex.adcState(0).effective, AlexController::BpfEffective::Bypass);
        QVERIFY(alex.adcState(0).reasonText.contains("multi-band"));
    }
```

- [ ] **Step 2: Add `notifySlicesOnAdc()` API to AlexController**

```cpp
public:
    /// Phase 3F: called by RadioModel when slice list on this ADC changes
    /// (slice add/remove/retune-cross-band). Drives the auto-mode recompute.
    /// `slicesOnAdc[i]` = Band::NONE if no slice in that position lives on this ADC.
    void notifySlicesOnAdc(int adc, const std::array<Band, 5>& slicesOnAdc);

private:
    std::array<std::array<Band, 5>, 2> m_slicesPerAdc;  // [adc][sliceIndex]
```

(Add `#include <set>` for the set-of-bands math.)

- [ ] **Step 3: Implement**

```cpp
void AlexController::notifySlicesOnAdc(int adc, const std::array<Band, 5>& slicesOnAdc)
{
    if (adc < 0 || adc >= 2) { return; }
    m_slicesPerAdc[adc] = slicesOnAdc;
    recomputeBpf(adc);
}
```

Extend the auto-mode branch of `recomputeBpf`:

```cpp
    } else {
        // Auto mode: inspect slice bands on this ADC
        std::set<Band> uniqueBands;
        for (Band b : m_slicesPerAdc[adc]) {
            if (b != Band::NONE) { uniqueBands.insert(b); }
        }

        if (uniqueBands.empty()) {
            s.effective = BpfEffective::Filtered;
            s.reasonText = QStringLiteral("%1 (idle)").arg(bandLabel(s.currentBpfBand));
        } else if (uniqueBands.size() == 1) {
            s.currentBpfBand = *uniqueBands.begin();
            s.effective = BpfEffective::Filtered;
            s.reasonText = bandLabel(s.currentBpfBand);
        } else {
            // 2+ distinct bands on this ADC → BYPASS
            s.effective = BpfEffective::Bypass;
            QStringList bandList;
            for (Band b : uniqueBands) { bandList << bandLabel(b); }
            s.reasonText = QStringLiteral("BYPASS (multi-band: %1)").arg(bandList.join(QStringLiteral(" + ")));
        }
    }
```

- [ ] **Step 4: Verify + commit**

```bash
ctest --test-dir build -R tst_alex_controller_per_adc_bpf -V 2>&1 | tail -10
git add src/core/accessories/AlexController.{h,cpp} tests/tst_alex_controller_per_adc_bpf.cpp
git commit -m "feat(3f-b): AlexController auto-mode multi-band BYPASS + slice-aware recompute"
```

---

## Task 14: Per-MAC persistence for BpfMode + HpfEnabled

**Files:**
- Modify: `src/core/accessories/AlexController.cpp` (extend `load` + `save` methods)

- [ ] **Step 1: Add persistence round-trip test**

```cpp
    void bpf_mode_persists_per_adc_per_mac()
    {
        const QString testMac = QStringLiteral("00:11:22:33:44:55");
        {
            AlexController alex;
            alex.setMacAddress(testMac);
            alex.setBpfMode(0, AlexController::BpfMode::ForceBypass);
            alex.setBpfMode(1, AlexController::BpfMode::ForceBand);
            alex.save();
        }
        {
            AlexController alex2;
            alex2.setMacAddress(testMac);
            alex2.load();
            QCOMPARE(alex2.bpfMode(0), AlexController::BpfMode::ForceBypass);
            QCOMPARE(alex2.bpfMode(1), AlexController::BpfMode::ForceBand);
        }
    }
```

- [ ] **Step 2: Extend `AlexController::save()`**

Find the existing `save()` method. Add to the end:

```cpp
    s.setValue(prefix + QStringLiteral("Alex0_BpfMode"), int(m_perAdcState[0].mode));
    s.setValue(prefix + QStringLiteral("Alex1_BpfMode"), int(m_perAdcState[1].mode));
```

- [ ] **Step 3: Extend `AlexController::load()`**

```cpp
    m_perAdcState[0].mode = static_cast<BpfMode>(s.value(prefix + QStringLiteral("Alex0_BpfMode"), 0).toInt());
    m_perAdcState[1].mode = static_cast<BpfMode>(s.value(prefix + QStringLiteral("Alex1_BpfMode"), 0).toInt());
    recomputeBpf(0);
    recomputeBpf(1);
```

- [ ] **Step 4: Commit**

```bash
ctest --test-dir build -R tst_alex_controller_per_adc_bpf -V 2>&1 | tail -10
git add src/core/accessories/AlexController.cpp tests/tst_alex_controller_per_adc_bpf.cpp
git commit -m "feat(3f-b): AlexController BpfMode per-MAC persistence"
```

---

## Task 15: Wire codec → P2RadioConnection bridge

**Files:**
- Modify: `src/core/P2RadioConnection.h`, `src/core/P2RadioConnection.cpp`

- [ ] **Step 1: Add test for `applyDdcAssignment` slot**

Create `tests/tst_p2_radio_connection_apply_ddc_assignment.cpp`:

```cpp
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"
#include "core/DdcAssignment.h"

using namespace NereusSDR;

class TestP2RadioConnectionApplyDdcAssignment : public QObject {
    Q_OBJECT
private slots:
    void apply_ddc_assignment_populates_rx_state()
    {
        P2RadioConnection conn;
        DdcAssignment a{};
        a.rate[2] = 192000;
        a.rate[3] = 96000;
        a.ddcEnable = 0x0c;
        a.adcCtrl1 = 0x10;  // DDC2→ADC0, DDC3→ADC1
        a.nDdc = 2;

        conn.applyDdcAssignment(a);

        // Verify m_rx[2] populated (use existing test-only accessor if available).
        // Note: P2RadioConnection.h:664-709 exposes compose* methods for test;
        // we can verify CmdRx byte layout via composeCmdRx and check expected bytes.
        QByteArray buf(1444, '\0');
        conn.composeCmdRx(buf);

        // DDC2 rate field starts at byte (3+5)*10 + offset per protocol; just sanity-check non-zero.
        bool anyNonzero = false;
        for (int i = 50; i < 200; ++i) {
            if (buf[i] != 0) { anyNonzero = true; break; }
        }
        QVERIFY(anyNonzero);
    }
};

QTEST_MAIN(TestP2RadioConnectionApplyDdcAssignment)
#include "tst_p2_radio_connection_apply_ddc_assignment.moc"
```

Register: `nereus_add_test(tst_p2_radio_connection_apply_ddc_assignment)`.

- [ ] **Step 2: Run test to verify failure**

Expected: `'applyDdcAssignment' is not a member of 'P2RadioConnection'`.

- [ ] **Step 3: Add `applyDdcAssignment` slot to P2RadioConnection.h**

Public slots:

```cpp
public slots:
    /// Phase 3F: apply a multi-slice DDC assignment from the codec.
    /// Updates m_rx[i].{enable, samplingRate, rxAdc, sync, ...} from the struct.
    /// Replaces the bulk applyPsDdcConfig path; existing PS-only path still works
    /// by constructing a DdcAssignment with only PS pair populated.
    void applyDdcAssignment(const DdcAssignment& assignment);
```

- [ ] **Step 4: Implement in P2RadioConnection.cpp**

```cpp
void P2RadioConnection::applyDdcAssignment(const DdcAssignment& a)
{
    // Populate per-DDC fields from the assignment struct.
    for (int ddc = 0; ddc < 8; ++ddc) {
        const bool enabled = (a.ddcEnable >> ddc) & 0x01;
        m_rx[ddc].enable = enabled;
        m_rx[ddc].samplingRate = a.rate[ddc] / 1000;  // CmdRx field is kHz
        m_rx[ddc].sync = (a.syncEnable >> ddc) & 0x01;

        // ADC routing: 2 bits per DDC from adcCtrl1 (DDC0-3) or adcCtrl2 (DDC4-7)
        const int ctrlByte = (ddc < 4) ? a.adcCtrl1 : a.adcCtrl2;
        const int shift = (ddc < 4) ? (ddc * 2) : ((ddc - 4) * 2);
        m_rx[ddc].rxAdc = (ctrlByte >> shift) & 0x03;
    }

    // Trigger CmdRx send.
    if (m_state == ConnectionState::Connected) {
        sendCmdRx();
    }
}
```

- [ ] **Step 5: Commit**

```bash
ctest --test-dir build -R tst_p2_radio_connection_apply_ddc_assignment -V 2>&1 | tail -10
git add src/core/P2RadioConnection.{h,cpp} tests/tst_p2_radio_connection_apply_ddc_assignment.cpp tests/CMakeLists.txt
git commit -m "feat(3f-b): P2RadioConnection::applyDdcAssignment from DdcAssignment struct"
```

---

## Task 16: Wire RadioModel → codec → P2RadioConnection on slice events

**Files:**
- Modify: `src/models/RadioModel.cpp` (existing slice signal handlers)

- [ ] **Step 1: Find existing slice signal wiring**

```bash
grep -nE "connect.*Slice.*frequencyChanged|setReceiverFrequency|applyPsDdcConfig" src/models/RadioModel.cpp | head -10
```

Identify where the current code calls `applyPsDdcConfig` (Phase 3M-4 wiring) or where slice frequency changes flow into codec calls.

- [ ] **Step 2: Add a helper `RadioModel::buildSliceConfigsForCodec()` private method**

In RadioModel.h:
```cpp
private:
    std::array<NereusSDR::SliceConfig, 5> buildSliceConfigsForCodec() const;
    void invokeCodecDdcAssignment();
```

In RadioModel.cpp:
```cpp
std::array<SliceConfig, 5> RadioModel::buildSliceConfigsForCodec() const
{
    std::array<SliceConfig, 5> configs{};
    const auto& slices = m_slices;
    for (int i = 0; i < int(slices.size()) && i < 5; ++i) {
        SliceModel* s = slices.at(i);
        if (!s) { continue; }
        configs[i].live = true;
        configs[i].frequencyHz = qint64(s->frequency() * 1e6);  // SliceModel uses MHz
        configs[i].bandIndex = int(bandFromFrequency(s->frequency()));
        configs[i].sampleRateHz = s->sampleRateHz();
        configs[i].antennaIndex = parseAntennaIndex(s->rxAntenna());  // helper to map "ANT1" -> 1
        configs[i].txBound = s->isTxSlice();
        configs[i].diversityRequested = s->diversityEnabled();
    }
    return configs;
}

void RadioModel::invokeCodecDdcAssignment()
{
    if (m_connectionState != ConnectionState::Connected) { return; }

    CodecContext ctx{};
    ctx.hpsdrModel = m_currentRadio.model;
    ctx.mox = m_moxActive;
    ctx.psActive = (m_pureSignal && m_pureSignal->isAutoCalEnabled());
    ctx.diversityActive = (m_slices.isEmpty() ? false : m_slices.first()->diversityEnabled());

    DdcAssignment assignment{};
    if (auto* p2 = m_p2Codec.get()) {
        assignment = p2->applyDdcAssignment(ctx, buildSliceConfigsForCodec());
        if (auto* p2conn = qobject_cast<P2RadioConnection*>(m_connection)) {
            p2conn->applyDdcAssignment(assignment);
        }
    } else if (auto* p1 = m_p1Codec.get()) {
        assignment = p1->applyDdcAssignment(ctx, buildSliceConfigsForCodec());
        // P1 path: translate to NetworkIO calls (existing P1 plumbing in P1RadioConnection)
        // For Sub-Epic B scope, P1 path remains backward-compat via applyPsDdcConfig until
        // P1RadioConnection gets its own applyDdcAssignment slot (deferred).
    }
}
```

- [ ] **Step 3: Hook `invokeCodecDdcAssignment` on relevant events**

Find where slice add / remove / frequencyChanged / `mox` / `pureSignalChanged` signals are handled. Append `invokeCodecDdcAssignment()` calls.

For example, where slices are added (probably in `addSliceOnPan` or similar):

```cpp
    connect(slice, &SliceModel::frequencyChanged, this, [this]() {
        invokeCodecDdcAssignment();
    });
```

- [ ] **Step 4: Smoke-build and test current single-slice operation unchanged**

```bash
cmake --build build && ctest --test-dir build 2>&1 | tail -10
```

Expected: existing tests pass (single-slice operation byte-faithful through new path).

- [ ] **Step 5: Commit**

```bash
git add src/models/RadioModel.{h,cpp}
git commit -m "feat(3f-b): wire RadioModel→codec→P2 multi-slice DDC assignment path"
```

---

## Task 17: Event matrix verification — slice freq cross-band triggers recompute

**Files:**
- Create: `tests/tst_alex_controller_event_matrix.cpp`

Verifies the 16-row event trigger matrix from design §10.

- [ ] **Step 1: Write the test**

```cpp
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/accessories/AlexController.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestAlexControllerEventMatrix : public QObject {
    Q_OBJECT
private slots:
    void slice_freq_change_to_different_band_triggers_recompute()
    {
        AlexController alex;
        std::array<Band, 5> initialSlices {Band::B20M, Band::NONE, Band::NONE, Band::NONE, Band::NONE};
        alex.notifySlicesOnAdc(0, initialSlices);

        QSignalSpy spy(&alex, &AlexController::bpfStateChanged);

        // Slice A retunes from 20m to 40m
        std::array<Band, 5> updated {Band::B40M, Band::NONE, Band::NONE, Band::NONE, Band::NONE};
        alex.notifySlicesOnAdc(0, updated);

        QVERIFY(spy.count() >= 1);
        QCOMPARE(alex.adcState(0).currentBpfBand, Band::B40M);
    }

    void slice_freq_change_within_same_band_does_not_emit()
    {
        AlexController alex;
        std::array<Band, 5> slices {Band::B20M, Band::NONE, Band::NONE, Band::NONE, Band::NONE};
        alex.notifySlicesOnAdc(0, slices);

        QSignalSpy spy(&alex, &AlexController::bpfStateChanged);
        alex.notifySlicesOnAdc(0, slices);  // identical
        QCOMPARE(spy.count(), 0);
    }

    void wideband_active_overrides_operator_force_filter()
    {
        AlexController alex;
        alex.setBpfMode(0, AlexController::BpfMode::ForceBand);
        alex.setWidebandActive(0, true);
        QCOMPARE(alex.adcState(0).effective, AlexController::BpfEffective::WidebandLocked);
    }
};

QTEST_MAIN(TestAlexControllerEventMatrix)
#include "tst_alex_controller_event_matrix.moc"
```

Register: `nereus_add_test(tst_alex_controller_event_matrix)`.

- [ ] **Step 2: Run + verify pass**

```bash
ctest --test-dir build -R tst_alex_controller_event_matrix -V 2>&1 | tail -10
```

Expected: 3 passed.

- [ ] **Step 3: Commit**

```bash
git add tests/tst_alex_controller_event_matrix.cpp tests/CMakeLists.txt
git commit -m "test(3f-b): AlexController event-matrix coverage (cross-band, same-band, wideband priority)"
```

---

## Sub-Epic B Completion Criteria

When Tasks 1-17 are done:

- `SliceConfig` + `DdcAssignment` structs shared between codec and connection layers
- `IP1Codec` + `IP2Codec` gain `applyDdcAssignment(slices[5])` virtual; all 6 per-board codecs implement it
- `P2CodecSaturn` + `P2CodecOrionMkII` produce byte-faithful Thetis output for 1-2 slice cases (verified by tests) and additive output for slices C/D/E
- `P1CodecHl2` preserves mi0bot PS rate carveout
- `AlexController` has per-ADC `AlexAdcState`, `BpfMode`/`BpfEffective` enums, `setBpfMode`/`setWidebandActive`/`notifySlicesOnAdc`/`recomputeBpf` API
- Auto-mode multi-band → BYPASS decision tree validated
- Per-MAC persistence for `Alex<N>_BpfMode`
- `P2RadioConnection::applyDdcAssignment(DdcAssignment)` slot replaces / supplements bulk `applyPsDdcConfig`
- `RadioModel::invokeCodecDdcAssignment()` wired to slice freq / mox / PS / diversity events
- 6 test files (~30 cases) all green
- Single-slice operation byte-faithful (regression sweep)

Ready for **Sub-Epic C (TxSliceArbiter + lifecycle)** to begin.

---

## References

- Sub-Epic A plan: `docs/architecture/2026-05-26-phase3f-sub-epic-a-foundation-plan.md`
- Design doc: `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md` §4 (DDC/Chain/Alex), §10 (event matrix)
- Thetis UpdateDDCs: `console.cs:8186-8538 [v2.10.3.15]`
- mi0bot HL2 PS carveout: `console.cs:8409-8488 [v2.10.3.13]`
- Existing codec interface: `src/core/codec/IP1Codec.h`, `IP2Codec.h`, per-board subclasses
- Existing AlexController per-band API (preserved unchanged): `src/core/accessories/AlexController.h`

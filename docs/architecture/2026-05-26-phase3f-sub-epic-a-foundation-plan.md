# Phase 3F Sub-Epic A: Foundation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Lay the foundation for Phase 3F multi-slice + multi-pan + wideband + diversity without changing existing operator-visible behaviour. After this plan lands, the app still runs as a single-slice client; the infrastructure is in place for sub-epics B-H to wire up actual multi-slice operation.

**Architecture:** Additive-only changes to `BoardCapabilities` (new fields: `maxSlices`, `widebandAdcs`), `SliceModel` (7 new Q_PROPERTYs with per-band persistence), `RadioModel` (new `maxSlices()` accessor + slice-list skeleton), `AppSettings` (schema v6 migration). `WdspEngine` is already N-channel-capable via `std::map<int, std::unique_ptr<RxChannel>>`, no growth required. No existing field is renamed or removed.

**Tech Stack:** C++20, Qt6 (Q_PROPERTY + signals/slots, QObject, QSignalSpy for tests), QtTest framework, CMake (via `nereus_add_test()` macro), AppSettings XML persistence.

**Parent design:** [docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md](2026-05-26-phase3f-multi-pan-multi-slice-design.md)

**Prereqs:** None. This is the first sub-epic of Phase 3F.

**Estimated effort:** 5 working days, 15 tasks, ~75 bite-sized steps.

---

## File Structure

### Files to modify

| File | Purpose | Lines affected |
|---|---|---|
| `src/core/BoardCapabilities.h` | Add `maxSlices` and `widebandAdcs` fields to struct (line 221+) | Insert near existing `maxReceivers` (line 232), add doc comments distinguishing the two |
| `src/core/BoardCapabilities.cpp` | Populate `maxSlices` + `widebandAdcs` per SKU | 14 SKU rows updated |
| `src/models/SliceModel.h` | 7 new Q_PROPERTYs + getter/setter declarations + signal declarations + member fields | Add to existing Q_PROPERTY block (line 163-205), getters (line 458+), signals (line 720+), members (line 845+) |
| `src/models/SliceModel.cpp` | Setter implementations + per-band persistence keys for new properties | New methods, extend `loadPerBand`/`savePerBand` |
| `src/models/RadioModel.h` | Add `maxSlices()` accessor + slice-list skeleton API | Public methods near existing slice management |
| `src/models/RadioModel.cpp` | Implement `maxSlices()` (return from current radio's BoardCapabilities) | New short method |
| `src/core/AppSettings.cpp` | Add v6 migration block | Insert after existing v5 block (line 1171+) |

### Files to create

| File | Purpose |
|---|---|
| `tests/tst_board_capabilities_phase3f.cpp` | Per-SKU verification of `maxSlices` and `widebandAdcs` values |
| `tests/tst_slice_model_phase3f_properties.cpp` | Property round-trip + persistence + signal emission for 7 new properties |
| `tests/tst_radio_model_max_slices.cpp` | `RadioModel::maxSlices()` returns the connected SKU's cap |
| `tests/tst_settings_schema_v6_migration.cpp` | Migration from v5 settings populates new keys with defaults |

### CMake registration

Each new test file added to `tests/CMakeLists.txt` via the existing `nereus_add_test(<name>)` macro pattern. No new build dependencies.

---

## Task 1: Add `maxSlices` and `widebandAdcs` fields to BoardCapabilities struct

**Files:**
- Modify: `src/core/BoardCapabilities.h` (insert after line 232 `maxReceivers`)

- [ ] **Step 1: Write the failing test**

Create `tests/tst_board_capabilities_phase3f.cpp`:

```cpp
// =================================================================
// tests/tst_board_capabilities_phase3f.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Task 1: verify BoardCapabilities gains
// maxSlices and widebandAdcs fields with correct per-SKU values per
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.
// =================================================================

#include <QtTest/QtTest>
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestBoardCapabilitiesPhase3F : public QObject {
    Q_OBJECT
private slots:
    void struct_has_max_slices_field()
    {
        BoardCapabilities caps{};
        caps.maxSlices = 5;
        QCOMPARE(caps.maxSlices, 5);
    }

    void struct_has_wideband_adcs_field()
    {
        BoardCapabilities caps{};
        caps.widebandAdcs = 2;
        QCOMPARE(caps.widebandAdcs, 2);
    }
};

QTEST_MAIN(TestBoardCapabilitiesPhase3F)
#include "tst_board_capabilities_phase3f.moc"
```

- [ ] **Step 2: Register the test in CMake**

Edit `tests/CMakeLists.txt`. Find any existing `nereus_add_test(tst_board_capabilities` line; if none, add a new line in the alphabetical/categorical position:

```cmake
nereus_add_test(tst_board_capabilities_phase3f)
```

- [ ] **Step 3: Run test to verify it fails (compile error)**

Run from build directory:
```bash
cmake --build build --target tst_board_capabilities_phase3f 2>&1 | tail -5
```

Expected: FAIL with compile error `'maxSlices' is not a member of 'BoardCapabilities'` or `'widebandAdcs' is not a member`.

- [ ] **Step 4: Add the two fields to the struct**

Edit `src/core/BoardCapabilities.h`. Find line 232 (`int  maxReceivers;`). Insert these two fields immediately after it:

```cpp
    // Phase 3F: user-facing slice cap, distinct from maxReceivers (= total DDC count).
    // For 2-ADC boards this is typically maxReceivers - 2 (DDC0/1 reserved for PS + diversity).
    // For 1-ADC boards this often equals maxReceivers, except HL2 which is force-capped to 1.
    // See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.
    int  maxSlices {0};

    // Phase 3F: number of ADCs that support the wideband (real-sample) stream.
    // P2 boards: typically equals adcCount. P1 boards: 0 (different mechanism, deferred to 3F-W).
    int  widebandAdcs {0};
```

- [ ] **Step 5: Rebuild and run test to verify it passes**

```bash
cmake --build build --target tst_board_capabilities_phase3f && ctest --test-dir build -R tst_board_capabilities_phase3f -V
```

Expected: 2 passed (struct_has_max_slices_field, struct_has_wideband_adcs_field).

- [ ] **Step 6: Commit**

```bash
git add src/core/BoardCapabilities.h tests/tst_board_capabilities_phase3f.cpp tests/CMakeLists.txt
git commit -m "feat(3f-a): add maxSlices + widebandAdcs to BoardCapabilities struct"
```

---

## Task 2: Populate `maxSlices` per SKU

**Files:**
- Modify: `src/core/BoardCapabilities.cpp` (14 SKU initializers around lines 299, 352, 411, 466, 516, 567, 629, 714, etc.)

- [ ] **Step 1: Extend the test to verify per-SKU values**

Append to `tests/tst_board_capabilities_phase3f.cpp` inside the class (before `QTEST_MAIN`):

```cpp
    void hl2_max_slices_is_1()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMESLITE);
        QCOMPARE(caps.maxSlices, 1);
    }

    void metis_max_slices_is_3()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HPSDR);
        QCOMPARE(caps.maxSlices, 3);
    }

    void hermes_max_slices_is_4()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMES);
        QCOMPARE(caps.maxSlices, 4);
    }

    void hermesII_max_slices_is_2()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN10E);
        QCOMPARE(caps.maxSlices, 2);
    }

    void angelia_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN100D);
        QCOMPARE(caps.maxSlices, 5);
    }

    void orion_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN200D);
        QCOMPARE(caps.maxSlices, 5);
    }

    void orionMkII_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ORIONMKII);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_g2_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN_G2);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_g2e_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN_G2E);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anvelinapro3_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANVELINAPRO3);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_7000d_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN7000D);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_8000d_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN8000D);
        QCOMPARE(caps.maxSlices, 5);
    }
```

(Adjust enum names to match the actual `HPSDRModel` enum entries in `src/core/HpsdrModel.h`. Run `grep -n "ANAN" src/core/HpsdrModel.h` if any name doesn't compile.)

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target tst_board_capabilities_phase3f && ctest --test-dir build -R tst_board_capabilities_phase3f -V
```

Expected: 12 new tests all FAIL with `Compared values are not the same: Actual (caps.maxSlices): 0, Expected: <N>` (default value 0 because we haven't populated yet).

- [ ] **Step 3: Populate per-SKU values in BoardCapabilities.cpp**

For each of the 14 SKU initializer blocks, add the `.maxSlices = <N>` field in the alphabetical position within the struct (or wherever fits the existing initializer style). Values per design §2:

| SKU (line approx) | `maxSlices` value |
|---|---|
| Metis (line 299) | `3` |
| Hermes (line 352) | `4` |
| HermesII (line 411) | `2` |
| Angelia (line 466) | `5` |
| Orion (line 516) | `5` |
| OrionMkII (line 567) | `5` |
| AnvelinaPro3 (line 629) | `4` (P1, single ADC, 4 DDCs) |
| ANAN7000D (search for ANAN7000D block) | `5` |
| ANAN8000D | `5` |
| ANAN_G2 / ANAN_G2_1K | `5` |
| ANAN_G2E (HermesC10) | `5` |
| Andromeda | `5` |
| Saturn / SaturnMkII | `5` |
| HermesLite2 (HL2) (line 714) | `1` |
| HermesLite2 RX-only | `1` |
| RedPitaya (P2 mode) | `5` |

Add the field in each initializer block. Example for the HL2 block:
```cpp
    .maxSlices        = 1,   // Phase 3F: HL2 single-slice cap (1-ADC, DDCs reserved for firmware quirks)
```

- [ ] **Step 4: Run test to verify all 12 pass**

```bash
ctest --test-dir build -R tst_board_capabilities_phase3f -V 2>&1 | tail -25
```

Expected: 14 total tests pass (2 from Task 1 + 12 SKU tests).

- [ ] **Step 5: Commit**

```bash
git add src/core/BoardCapabilities.cpp tests/tst_board_capabilities_phase3f.cpp
git commit -m "feat(3f-a): populate maxSlices per SKU per design §2"
```

---

## Task 3: Populate `widebandAdcs` per SKU

**Files:**
- Modify: `src/core/BoardCapabilities.cpp` (same SKU initializer blocks as Task 2)

- [ ] **Step 1: Add per-SKU tests**

Append to `tests/tst_board_capabilities_phase3f.cpp`:

```cpp
    void hl2_wideband_adcs_is_0()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMESLITE);
        QCOMPARE(caps.widebandAdcs, 0);  // P1 mechanism, deferred to 3F-W
    }

    void hermes_wideband_adcs_is_0()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMES);
        QCOMPARE(caps.widebandAdcs, 0);  // P1 mechanism
    }

    void anan_g2_wideband_adcs_is_2()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN_G2);
        QCOMPARE(caps.widebandAdcs, 2);  // 2-ADC P2 board
    }

    void anan_g2e_wideband_adcs_is_2()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN_G2E);
        QCOMPARE(caps.widebandAdcs, 2);
    }

    void anan_7000d_wideband_adcs_is_2()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN7000D);
        QCOMPARE(caps.widebandAdcs, 2);
    }
```

- [ ] **Step 2: Run tests to verify failure**

```bash
ctest --test-dir build -R tst_board_capabilities_phase3f -V 2>&1 | grep -E "FAIL|PASS" | head -10
```

Expected: 5 new tests FAIL (`widebandAdcs` defaults to 0 already, so HL2 + Hermes tests already pass; the three P2 boards FAIL with `Expected: 2, Actual: 0`).

- [ ] **Step 3: Populate `widebandAdcs` per SKU**

Per design §2:
- All 1-ADC SKUs (HL2, Metis, Hermes, HermesII, AnvelinaPro3 P1, RedPitaya P1): `widebandAdcs = 0`
- All 2-ADC P2 SKUs (Angelia, Orion, OrionMkII, ANAN7000D, ANAN8000D, ANAN_G2, ANAN_G2E, Andromeda, Saturn, SaturnMkII, RedPitaya P2): `widebandAdcs = 2`

Add the field to each initializer. Example for ANAN_G2:
```cpp
    .widebandAdcs     = 2,   // Phase 3F: ADC0 + ADC1 both support wideband stream
```

- [ ] **Step 4: Run tests to verify all pass**

```bash
ctest --test-dir build -R tst_board_capabilities_phase3f -V 2>&1 | tail -25
```

Expected: 19 total tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/core/BoardCapabilities.cpp tests/tst_board_capabilities_phase3f.cpp
git commit -m "feat(3f-a): populate widebandAdcs per SKU (2 for P2, 0 for P1)"
```

---

## Task 4: Add `sliceLetter` Q_PROPERTY to SliceModel

**Files:**
- Modify: `src/models/SliceModel.h` (Q_PROPERTY block ~line 163, getters ~line 458, signals ~line 720, members ~line 845)
- Modify: `src/models/SliceModel.cpp` (setter implementation)

- [ ] **Step 1: Write failing test**

Create `tests/tst_slice_model_phase3f_properties.cpp`:

```cpp
// =================================================================
// tests/tst_slice_model_phase3f_properties.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Tasks 4-11: verify SliceModel gains 7 new
// Q_PROPERTYs per docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §3.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceModelPhase3FProperties : public QObject {
    Q_OBJECT

private slots:
    void slice_letter_default_is_A()
    {
        SliceModel slice;
        QCOMPARE(slice.sliceLetter(), QChar('A'));
    }

    void slice_letter_setter_round_trips()
    {
        SliceModel slice;
        slice.setSliceLetter(QChar('C'));
        QCOMPARE(slice.sliceLetter(), QChar('C'));
    }

    void slice_letter_setter_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::sliceLetterChanged);
        slice.setSliceLetter(QChar('B'));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toChar(), QChar('B'));
    }

    void slice_letter_setter_idempotent()
    {
        SliceModel slice;
        slice.setSliceLetter(QChar('B'));
        QSignalSpy spy(&slice, &SliceModel::sliceLetterChanged);
        slice.setSliceLetter(QChar('B'));  // same value
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestSliceModelPhase3FProperties)
#include "tst_slice_model_phase3f_properties.moc"
```

- [ ] **Step 2: Register test in CMake**

Edit `tests/CMakeLists.txt`:
```cmake
nereus_add_test(tst_slice_model_phase3f_properties)
```

- [ ] **Step 3: Run test to verify it fails (compile error)**

```bash
cmake --build build --target tst_slice_model_phase3f_properties 2>&1 | tail -5
```

Expected: FAIL with `'sliceLetter' was not declared`.

- [ ] **Step 4: Add the Q_PROPERTY + getter + setter + signal + member**

Edit `src/models/SliceModel.h`. Within the existing Q_PROPERTY block (after line 174, the `txSlice` property), insert:

```cpp
    // Phase 3F: per-slice letter identifier A-E. Drives badge color via VfoWidget::sliceColor().
    Q_PROPERTY(QChar sliceLetter READ sliceLetter WRITE setSliceLetter NOTIFY sliceLetterChanged)
```

In the public getters/setters section (near line 377-378, after `setTxSlice`):

```cpp
    QChar sliceLetter() const { return m_sliceLetter; }
    void setSliceLetter(QChar letter);
```

In the signals section (near line 726, after `txSliceChanged`):

```cpp
    void sliceLetterChanged(QChar letter);
```

In the private members section (near line 845, after existing per-slice fields):

```cpp
    QChar m_sliceLetter{'A'};  // Phase 3F: default A for backward-compat single-slice
```

- [ ] **Step 5: Implement the setter in SliceModel.cpp**

Edit `src/models/SliceModel.cpp`. Add a new method (place near other setters, e.g. after `setTxSlice`):

```cpp
void SliceModel::setSliceLetter(QChar letter)
{
    if (m_sliceLetter != letter) {
        m_sliceLetter = letter;
        emit sliceLetterChanged(letter);
    }
}
```

- [ ] **Step 6: Run test to verify it passes**

```bash
cmake --build build --target tst_slice_model_phase3f_properties && ctest --test-dir build -R tst_slice_model_phase3f_properties -V 2>&1 | tail -10
```

Expected: 4 passed.

- [ ] **Step 7: Commit**

```bash
git add src/models/SliceModel.h src/models/SliceModel.cpp tests/tst_slice_model_phase3f_properties.cpp tests/CMakeLists.txt
git commit -m "feat(3f-a): add SliceModel::sliceLetter Q_PROPERTY (A-E slice ID)"
```

---

## Task 5: Add `chainIndex` Q_PROPERTY to SliceModel

**Files:**
- Modify: `src/models/SliceModel.h`, `src/models/SliceModel.cpp`

- [ ] **Step 1: Add tests**

Append to `tests/tst_slice_model_phase3f_properties.cpp` (inside the class, before `QTEST_MAIN`):

```cpp
    void chain_index_default_is_0()
    {
        SliceModel slice;
        QCOMPARE(slice.chainIndex(), 0);
    }

    void chain_index_setter_round_trips()
    {
        SliceModel slice;
        slice.setChainIndex(1);
        QCOMPARE(slice.chainIndex(), 1);
    }

    void chain_index_setter_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::chainIndexChanged);
        slice.setChainIndex(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 1);
    }
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build build --target tst_slice_model_phase3f_properties 2>&1 | tail -5
```

Expected: FAIL with `'chainIndex' was not declared`.

- [ ] **Step 3: Add the property to SliceModel.h**

Same locations as Task 4. Q_PROPERTY block:
```cpp
    // Phase 3F: which Alex chain (ADC) hosts this slice's DDC. 0 or 1 on 2-ADC boards, always 0 on 1-ADC.
    Q_PROPERTY(int chainIndex READ chainIndex WRITE setChainIndex NOTIFY chainIndexChanged)
```

Getter/setter:
```cpp
    int chainIndex() const { return m_chainIndex; }
    void setChainIndex(int idx);
```

Signal:
```cpp
    void chainIndexChanged(int idx);
```

Member:
```cpp
    int m_chainIndex{0};
```

- [ ] **Step 4: Implement setter in SliceModel.cpp**

```cpp
void SliceModel::setChainIndex(int idx)
{
    if (m_chainIndex != idx) {
        m_chainIndex = idx;
        emit chainIndexChanged(idx);
    }
}
```

- [ ] **Step 5: Run test + commit**

```bash
cmake --build build --target tst_slice_model_phase3f_properties && ctest --test-dir build -R tst_slice_model_phase3f_properties -V 2>&1 | tail -10
git add src/models/SliceModel.h src/models/SliceModel.cpp tests/tst_slice_model_phase3f_properties.cpp
git commit -m "feat(3f-a): add SliceModel::chainIndex Q_PROPERTY (which Alex chain hosts slice)"
```

---

## Task 6: Add `ddcIndex` Q_PROPERTY (read-only) to SliceModel

**Files:**
- Modify: `src/models/SliceModel.h`, `src/models/SliceModel.cpp`

This is read-only from operator perspective (codec assigns); we still expose a setter for codec/RadioModel internal use.

- [ ] **Step 1: Add tests**

Append:
```cpp
    void ddc_index_default_is_negative_1()
    {
        SliceModel slice;
        QCOMPARE(slice.ddcIndex(), -1);  // unassigned
    }

    void ddc_index_setter_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::ddcIndexChanged);
        slice.setDdcIndex(2);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(slice.ddcIndex(), 2);
    }
```

- [ ] **Step 2: Run test to verify failure**

Expected: `'ddcIndex' was not declared`.

- [ ] **Step 3: Add the property**

Q_PROPERTY block:
```cpp
    // Phase 3F: codec-assigned DDC index. -1 = unassigned. Read-only from operator perspective.
    Q_PROPERTY(int ddcIndex READ ddcIndex WRITE setDdcIndex NOTIFY ddcIndexChanged)
```

Getter/setter:
```cpp
    int ddcIndex() const { return m_ddcIndex; }
    void setDdcIndex(int ddc);
```

Signal + member (default -1):
```cpp
    void ddcIndexChanged(int ddc);
    // ...
    int m_ddcIndex{-1};
```

- [ ] **Step 4: Implement setter**

```cpp
void SliceModel::setDdcIndex(int ddc)
{
    if (m_ddcIndex != ddc) {
        m_ddcIndex = ddc;
        emit ddcIndexChanged(ddc);
    }
}
```

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_slice_model_phase3f_properties && ctest --test-dir build -R tst_slice_model_phase3f_properties -V 2>&1 | tail -10
git add src/models/SliceModel.h src/models/SliceModel.cpp tests/tst_slice_model_phase3f_properties.cpp
git commit -m "feat(3f-a): add SliceModel::ddcIndex Q_PROPERTY (codec-assigned, -1=unassigned)"
```

---

## Task 7: Add `sampleRateHz` Q_PROPERTY with per-band persistence

**Files:**
- Modify: `src/models/SliceModel.h`, `src/models/SliceModel.cpp`

Per-band persistence is essential for this one (operator's chosen rate must persist per slice per band).

- [ ] **Step 1: Add tests**

Append:
```cpp
    void sample_rate_default_is_192000()
    {
        SliceModel slice;
        QCOMPARE(slice.sampleRateHz(), 192000);
    }

    void sample_rate_setter_round_trips()
    {
        SliceModel slice;
        slice.setSampleRateHz(1536000);
        QCOMPARE(slice.sampleRateHz(), 1536000);
    }

    void sample_rate_setter_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::sampleRateHzChanged);
        slice.setSampleRateHz(384000);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 384000);
    }
```

- [ ] **Step 2: Run test to verify failure**

Expected: `'sampleRateHz' was not declared`.

- [ ] **Step 3: Add the property**

Include `#include "core/SampleRateCatalog.h"` at the top of `SliceModel.h` if not already (for `kDefaultSampleRate`).

Q_PROPERTY:
```cpp
    // Phase 3F: per-slice DDC sample rate. Default = SampleRateCatalog::kDefaultSampleRate (192 kHz).
    // Operator-owned, no mode-derived defaults. Persisted per-band per-slice.
    Q_PROPERTY(int sampleRateHz READ sampleRateHz WRITE setSampleRateHz NOTIFY sampleRateHzChanged)
```

Getter/setter:
```cpp
    int sampleRateHz() const { return m_sampleRateHz; }
    void setSampleRateHz(int hz);
```

Signal + member:
```cpp
    void sampleRateHzChanged(int hz);
    // ...
    int m_sampleRateHz{192000};  // SampleRateCatalog::kDefaultSampleRate
```

- [ ] **Step 4: Implement setter**

```cpp
void SliceModel::setSampleRateHz(int hz)
{
    if (m_sampleRateHz != hz) {
        m_sampleRateHz = hz;
        emit sampleRateHzChanged(hz);
    }
}
```

- [ ] **Step 5: Extend per-band persistence**

Find the existing per-band save/load methods in `SliceModel.cpp` (look for `loadPerBand` and `savePerBand` or similar; they write keys like `s.setValue(sp + QStringLiteral("Frequency"), ...)`).

Add to `savePerBand`:
```cpp
    s.setValue(sp + QStringLiteral("SampleRate"), m_sampleRateHz);
```

Add to `loadPerBand`:
```cpp
    setSampleRateHz(s.value(sp + QStringLiteral("SampleRate"), 192000).toInt());
```

- [ ] **Step 6: Run + commit**

```bash
cmake --build build --target tst_slice_model_phase3f_properties && ctest --test-dir build -R tst_slice_model_phase3f_properties -V 2>&1 | tail -10
git add src/models/SliceModel.h src/models/SliceModel.cpp tests/tst_slice_model_phase3f_properties.cpp
git commit -m "feat(3f-a): add SliceModel::sampleRateHz Q_PROPERTY with per-band persistence"
```

---

## Task 8: Add `diversityEnabled` Q_PROPERTY

**Files:**
- Modify: `src/models/SliceModel.h`, `src/models/SliceModel.cpp`

- [ ] **Step 1: Add tests**

```cpp
    void diversity_enabled_default_is_false()
    {
        SliceModel slice;
        QCOMPARE(slice.diversityEnabled(), false);
    }

    void diversity_enabled_setter_round_trips()
    {
        SliceModel slice;
        slice.setDiversityEnabled(true);
        QCOMPARE(slice.diversityEnabled(), true);
    }

    void diversity_enabled_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::diversityEnabledChanged);
        slice.setDiversityEnabled(true);
        QCOMPARE(spy.count(), 1);
    }
```

- [ ] **Step 2: Run test to verify failure**

Expected: `'diversityEnabled' was not declared`.

- [ ] **Step 3: Add the property**

Q_PROPERTY:
```cpp
    // Phase 3F: diversity mode flag. Slice-A-only, gated on BoardCapabilities.hasDiversity.
    // When true, DDC migration to DDC0+DDC1 sync pair handled by codec on next applyDdcAssignment.
    Q_PROPERTY(bool diversityEnabled READ diversityEnabled WRITE setDiversityEnabled NOTIFY diversityEnabledChanged)
```

Getter/setter:
```cpp
    bool diversityEnabled() const { return m_diversityEnabled; }
    void setDiversityEnabled(bool on);
```

Signal + member:
```cpp
    void diversityEnabledChanged(bool on);
    // ...
    bool m_diversityEnabled{false};
```

- [ ] **Step 4: Implement setter**

```cpp
void SliceModel::setDiversityEnabled(bool on)
{
    if (m_diversityEnabled != on) {
        m_diversityEnabled = on;
        emit diversityEnabledChanged(on);
    }
}
```

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_slice_model_phase3f_properties && ctest --test-dir build -R tst_slice_model_phase3f_properties -V 2>&1 | tail -10
git add src/models/SliceModel.h src/models/SliceModel.cpp tests/tst_slice_model_phase3f_properties.cpp
git commit -m "feat(3f-a): add SliceModel::diversityEnabled Q_PROPERTY (Slice-A only)"
```

---

## Task 9: Add `widebandExtensionRequested` Q_PROPERTY

**Files:**
- Modify: `src/models/SliceModel.h`, `src/models/SliceModel.cpp`

- [ ] **Step 1: Add tests**

```cpp
    void wideband_extension_default_is_false()
    {
        SliceModel slice;
        QCOMPARE(slice.widebandExtensionRequested(), false);
    }

    void wideband_extension_setter_round_trips()
    {
        SliceModel slice;
        slice.setWidebandExtensionRequested(true);
        QCOMPARE(slice.widebandExtensionRequested(), true);
    }

    void wideband_extension_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::widebandExtensionRequestedChanged);
        slice.setWidebandExtensionRequested(true);
        QCOMPARE(spy.count(), 1);
    }
```

- [ ] **Step 2: Run test to verify failure**

- [ ] **Step 3: Add the property**

Q_PROPERTY:
```cpp
    // Phase 3F: derived from pan zoom state. When true, this slice's pan is zoomed beyond DDC bandwidth
    // and needs wideband wing data. Triggers Alex BPF bypass on this slice's chain.
    Q_PROPERTY(bool widebandExtensionRequested READ widebandExtensionRequested WRITE setWidebandExtensionRequested NOTIFY widebandExtensionRequestedChanged)
```

Getter/setter:
```cpp
    bool widebandExtensionRequested() const { return m_widebandExtensionRequested; }
    void setWidebandExtensionRequested(bool on);
```

Signal + member:
```cpp
    void widebandExtensionRequestedChanged(bool on);
    // ...
    bool m_widebandExtensionRequested{false};
```

- [ ] **Step 4: Implement setter**

```cpp
void SliceModel::setWidebandExtensionRequested(bool on)
{
    if (m_widebandExtensionRequested != on) {
        m_widebandExtensionRequested = on;
        emit widebandExtensionRequestedChanged(on);
    }
}
```

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_slice_model_phase3f_properties && ctest --test-dir build -R tst_slice_model_phase3f_properties -V 2>&1 | tail -10
git add src/models/SliceModel.h src/models/SliceModel.cpp tests/tst_slice_model_phase3f_properties.cpp
git commit -m "feat(3f-a): add SliceModel::widebandExtensionRequested Q_PROPERTY (zoom-derived)"
```

---

## Task 10: Add `psPaused` Q_PROPERTY

**Files:**
- Modify: `src/models/SliceModel.h`, `src/models/SliceModel.cpp`

- [ ] **Step 1: Add tests**

```cpp
    void ps_paused_default_is_false()
    {
        SliceModel slice;
        QCOMPARE(slice.psPaused(), false);
    }

    void ps_paused_setter_round_trips()
    {
        SliceModel slice;
        slice.setPsPaused(true);
        QCOMPARE(slice.psPaused(), true);
    }

    void ps_paused_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::psPausedChanged);
        slice.setPsPaused(true);
        QCOMPARE(spy.count(), 1);
    }
```

- [ ] **Step 2: Run test to verify failure**

- [ ] **Step 3: Add the property**

Q_PROPERTY:
```cpp
    // Phase 3F: true when this slice's DDC is reclaimed by PureSignal during MOX.
    // Driven by PureSignal coordinator + MoxController. UI greys the pan + shows "PS HOLD" pill.
    Q_PROPERTY(bool psPaused READ psPaused WRITE setPsPaused NOTIFY psPausedChanged)
```

Getter/setter:
```cpp
    bool psPaused() const { return m_psPaused; }
    void setPsPaused(bool paused);
```

Signal + member:
```cpp
    void psPausedChanged(bool paused);
    // ...
    bool m_psPaused{false};
```

- [ ] **Step 4: Implement setter**

```cpp
void SliceModel::setPsPaused(bool paused)
{
    if (m_psPaused != paused) {
        m_psPaused = paused;
        emit psPausedChanged(paused);
    }
}
```

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_slice_model_phase3f_properties && ctest --test-dir build -R tst_slice_model_phase3f_properties -V 2>&1 | tail -10
git add src/models/SliceModel.h src/models/SliceModel.cpp tests/tst_slice_model_phase3f_properties.cpp
git commit -m "feat(3f-a): add SliceModel::psPaused Q_PROPERTY (driven by PureSignal during MOX)"
```

---

## Task 11: Add `RadioModel::maxSlices()` accessor

**Files:**
- Modify: `src/models/RadioModel.h` (public API, near existing slice management)
- Modify: `src/models/RadioModel.cpp`

- [ ] **Step 1: Write failing test**

Create `tests/tst_radio_model_max_slices.cpp`:

```cpp
// =================================================================
// tests/tst_radio_model_max_slices.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Task 11: verify RadioModel::maxSlices() exposes
// the connected SKU's slice cap.  Disconnected returns 1 (single-slice fallback).
// =================================================================

#include <QtTest/QtTest>
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestRadioModelMaxSlices : public QObject {
    Q_OBJECT
private slots:
    void disconnected_max_slices_is_1()
    {
        RadioModel radio;
        QCOMPARE(radio.maxSlices(), 1);  // safe default when not connected
    }
};

QTEST_MAIN(TestRadioModelMaxSlices)
#include "tst_radio_model_max_slices.moc"
```

- [ ] **Step 2: Register test + run to verify failure**

Edit `tests/CMakeLists.txt`:
```cmake
nereus_add_test(tst_radio_model_max_slices)
```

```bash
cmake --build build --target tst_radio_model_max_slices 2>&1 | tail -5
```

Expected: FAIL `'maxSlices' is not a member of 'RadioModel'`.

- [ ] **Step 3: Add the accessor**

Edit `src/models/RadioModel.h`. In the public methods section (find any existing slice-related accessor like `slices()`), add:

```cpp
    /// Phase 3F: hardware-capped user-facing slice count. Reads BoardCapabilities.maxSlices
    /// for the currently connected SKU. Returns 1 when disconnected (safe default).
    /// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.
    int maxSlices() const;
```

- [ ] **Step 4: Implement in RadioModel.cpp**

Find existing usage of `currentRadioInfo()` or `BoardCapabilities` lookup (search for `capabilitiesFor` or `caps.`). Add:

```cpp
int RadioModel::maxSlices() const
{
    if (m_connectionState != ConnectionState::Connected) {
        return 1;
    }
    const auto caps = NereusSDR::capabilitiesFor(m_currentRadio.model);
    return caps.maxSlices > 0 ? caps.maxSlices : 1;
}
```

(Adjust `m_currentRadio.model` to whatever member/method holds the current HPSDRModel; verify via `grep -n "currentRadio" src/models/RadioModel.h`.)

- [ ] **Step 5: Run test to verify passes**

```bash
cmake --build build --target tst_radio_model_max_slices && ctest --test-dir build -R tst_radio_model_max_slices -V 2>&1 | tail -10
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp tests/tst_radio_model_max_slices.cpp tests/CMakeLists.txt
git commit -m "feat(3f-a): add RadioModel::maxSlices() accessor (BoardCapabilities-driven)"
```

---

## Task 12: Bump SettingsSchemaVersion to v6

**Files:**
- Modify: `src/core/AppSettings.cpp` (add v6 migration block after line 1171)
- Modify: wherever `ensureSettingsAtVersion(N)` is called (search for it; probably `RadioModel` or `main.cpp`)

- [ ] **Step 1: Write failing migration test**

Create `tests/tst_settings_schema_v6_migration.cpp`:

```cpp
// =================================================================
// tests/tst_settings_schema_v6_migration.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Task 12: verify SettingsSchemaVersion v5 → v6
// migration. v6 is additive only (no key renames), so the migration is
// effectively a version bump that lets the app know per-slice per-band
// sample-rate keys are part of the schema going forward.
// =================================================================

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestSettingsSchemaV6Migration : public QObject {
    Q_OBJECT
private slots:
    void v5_to_v6_bumps_version_key()
    {
        // Pre-populate AppSettings as if it was v5 (current latest before 3F).
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("5"));

        // Run migration with currentVersion = 6.
        s.ensureSettingsAtVersion(6);

        QCOMPARE(s.value(QStringLiteral("SettingsSchemaVersion"), QString()).toString(),
                 QStringLiteral("6"));
    }

    void v6_to_v6_is_noop()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("6"));
        s.ensureSettingsAtVersion(6);
        QCOMPARE(s.value(QStringLiteral("SettingsSchemaVersion"), QString()).toString(),
                 QStringLiteral("6"));
    }
};

QTEST_MAIN(TestSettingsSchemaV6Migration)
#include "tst_settings_schema_v6_migration.moc"
```

- [ ] **Step 2: Register test + run to verify failure**

`tests/CMakeLists.txt`:
```cmake
nereus_add_test(tst_settings_schema_v6_migration)
```

```bash
cmake --build build --target tst_settings_schema_v6_migration && ctest --test-dir build -R tst_settings_schema_v6_migration -V 2>&1 | tail -10
```

Expected: FAIL (migration loop doesn't yet handle v6 → version key stays at 5).

- [ ] **Step 3: Add v6 migration block in AppSettings.cpp**

Edit `src/core/AppSettings.cpp` after line 1171 (after the v5 block, before `setValue(versionKey, ...)` at 1209):

```cpp
    if (storedVersion < 6 && currentVersion >= 6) {
        // Phase 3F: schema v6 is additive only. No key renames, no defaults to populate.
        // New per-slice per-band keys (SliceX_Band_<band>_SampleRate, diversity keys, etc.)
        // are populated lazily by SliceModel::savePerBand on first write. Operators with
        // existing v5 settings see no behavioural change until they touch the new controls.
        // See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §12.
    }
```

- [ ] **Step 4: Find the caller and bump currentVersion to 6**

```bash
grep -rn "ensureSettingsAtVersion" src/
```

Find the call site (likely `RadioModel.cpp` or `main.cpp`). It currently passes `5`. Change to `6`:

```cpp
AppSettings::instance().ensureSettingsAtVersion(6);
```

- [ ] **Step 5: Run test to verify passes**

```bash
cmake --build build --target tst_settings_schema_v6_migration && ctest --test-dir build -R tst_settings_schema_v6_migration -V 2>&1 | tail -10
```

Expected: 2 passed.

- [ ] **Step 6: Commit**

```bash
git add src/core/AppSettings.cpp src/models/RadioModel.cpp tests/tst_settings_schema_v6_migration.cpp tests/CMakeLists.txt
git commit -m "feat(3f-a): bump SettingsSchemaVersion v5 → v6 (additive, no key renames)"
```

---

## Task 13: Verify existing tests still pass (regression sweep)

**Files:** none modified

- [ ] **Step 1: Run the full existing test suite**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -30
```

Expected: all existing tests pass. New Phase 3F tests also pass.

- [ ] **Step 2: Diagnose any regressions**

If any pre-existing test now fails, it's because we added a member that affects sizeof(SliceModel) or BoardCapabilities, possibly breaking a serialization assumption. Investigate the specific test, fix root cause (don't paper over).

- [ ] **Step 3: Verify build still produces a runnable binary**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
ls -la build/NereusSDR.app/Contents/MacOS/NereusSDR 2>/dev/null || ls -la build/NereusSDR 2>/dev/null
```

Expected: binary exists, recent mtime.

- [ ] **Step 4: Smoke-launch and verify connect to a radio still works**

(Requires bench access. Skip if no radio available — note in commit message.)

```bash
./build/NereusSDR.app/Contents/MacOS/NereusSDR &
```

Eyeball: app starts, connects to currently-saved radio, single-slice operation works as before.

- [ ] **Step 5: Commit (regression checkpoint)**

```bash
git commit --allow-empty -m "chore(3f-a): regression sweep — full test suite green, single-slice operation unchanged"
```

---

## Task 14: Add Sub-Epic A retrospective note to design doc

**Files:**
- Modify: `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md` (append to references section)

- [ ] **Step 1: Append a "Sub-Epic A landed" note to the design doc**

Edit the design doc at the bottom (just before the final `## End` line). Add:

```markdown
---

## Sub-Epic A implementation note (landed YYYY-MM-DD)

Implemented per `docs/architecture/2026-05-26-phase3f-sub-epic-a-foundation-plan.md`. The foundation tasks shipped with no operator-visible behaviour change (single-slice operation unchanged). Adds: `BoardCapabilities.maxSlices`, `BoardCapabilities.widebandAdcs`, `SliceModel.{sliceLetter,chainIndex,ddcIndex,sampleRateHz,diversityEnabled,widebandExtensionRequested,psPaused}` Q_PROPERTYs, `RadioModel::maxSlices()` accessor, settings schema bump v5 → v6.

Discovered during implementation:
- `WdspEngine` already uses `std::map<int, std::unique_ptr<RxChannel>>` so it natively supports N channels. No "growth" required for Phase 3F (the original design said "grow from 2"; the audit found N-capable already).
- `BoardCapabilities.maxReceivers` field exists with semantic "total DDC count" (e.g. 7 on G2). It is intentionally distinct from the new `maxSlices` (user-facing cap, e.g. 5 on G2 after PS+diversity reservation). Both fields coexist.
- `BoardCapabilities.hasDiversityReceiver` already exists per SKU (`true` for 2-ADC boards, `false` for 1-ADC). No new field needed; design doc references `hasDiversity` should read as `hasDiversityReceiver`.
- `ReceiverManager.m_hwToLogical` map growth (design §4 file touches) is deferred to Sub-Epic B where the codec first emits multi-slice DDC assignments. The map is a `QMap` that grows naturally as entries are added; no Sub-Epic A change required to keep single-slice operation working.

Sub-Epic B (Codec + Chain) can now begin: codec extension reads `maxSlices` and produces multi-slice `DdcAssignment`, `AlexController` gains per-ADC state machine, `ReceiverManager.m_hwToLogical` grows naturally as B exercises it.
```

(Replace `YYYY-MM-DD` with actual landing date.)

- [ ] **Step 2: Commit**

```bash
git add docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
git commit -m "docs(arch): record Sub-Epic A landing + impl-time discoveries"
```

---

## Task 15: Open PR for Sub-Epic A

**Files:** none modified

- [ ] **Step 1: Push the branch**

```bash
git push -u origin HEAD
```

- [ ] **Step 2: Draft PR body**

```bash
gh pr create --title "Phase 3F Sub-Epic A: Foundation (BoardCapabilities + SliceModel + schema v6)" --body "$(cat <<'EOF'
## Summary

Foundation for Phase 3F multi-slice/multi-pan/wideband/diversity epic. Additive-only changes; no operator-visible behaviour change.

- `BoardCapabilities.maxSlices` (per-SKU user-facing slice cap, distinct from existing `maxReceivers`)
- `BoardCapabilities.widebandAdcs` (per-ADC wideband support count)
- `SliceModel` gains 7 Q_PROPERTYs: `sliceLetter`, `chainIndex`, `ddcIndex`, `sampleRateHz` (per-band persisted), `diversityEnabled`, `widebandExtensionRequested`, `psPaused`
- `RadioModel::maxSlices()` accessor
- `SettingsSchemaVersion` bump v5 → v6 (additive, no key renames)
- 4 new test files (~30 test cases) covering all new APIs

Parent design: [`docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md`](../blob/HEAD/docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md)
Plan: [`docs/architecture/2026-05-26-phase3f-sub-epic-a-foundation-plan.md`](../blob/HEAD/docs/architecture/2026-05-26-phase3f-sub-epic-a-foundation-plan.md)

## Test plan

- [ ] CI green (all existing + 4 new test suites)
- [ ] Bench: connect to ANAN-G2, verify single-slice operation unchanged
- [ ] Bench: connect to HL2, verify single-slice operation unchanged
- [ ] Verify settings file `~/.config/NereusSDR/NereusSDR.settings` shows `SettingsSchemaVersion=6` after first launch with the build
- [ ] Verify existing pre-3F per-band keys still load (e.g. `SliceA_Band_20m_Frequency`)

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 3: Hand off to operator for bench verification + merge**

PR URL returned by `gh pr create`. Operator runs the test plan, merges when green.

---

## Sub-Epic A Completion Criteria

When Tasks 1-15 are done:

- 4 new test files, ~30 test cases, all green
- `BoardCapabilities` has `maxSlices` (1/2/3/4/5 per SKU) and `widebandAdcs` (0 or 2)
- `SliceModel` has 7 new Q_PROPERTYs with proper signals + per-band persistence for `sampleRateHz`
- `RadioModel::maxSlices()` returns the connected SKU's cap or 1 disconnected
- `SettingsSchemaVersion = 6` in the AppSettings file
- Single-slice operation unchanged (regression sweep + bench verification)
- Design doc updated with implementation note

Ready for **Sub-Epic B (Codec + Chain)** to begin.

---

## References

- Design doc: `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md`
- Test framework: QtTest with `nereus_add_test()` CMake macro (see `tests/CMakeLists.txt`)
- Existing test pattern: `tests/tst_active_rx_count_live_apply.cpp`
- BoardCapabilities current struct: `src/core/BoardCapabilities.h:221+`
- SliceModel current Q_PROPERTY block: `src/models/SliceModel.h:163-205`
- AppSettings migration loop: `src/core/AppSettings.cpp:1119-1209`
- WdspEngine N-channel storage: `src/core/WdspEngine.h:516` (`std::map<int, std::unique_ptr<RxChannel>> m_rxChannels`)

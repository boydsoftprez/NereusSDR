# Phase 3P Sub-Phase A — P1 Wire-Bytes Parity Foundation: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor `P1RadioConnection`'s C&C compose layer into per-board codec subclasses behind a stable interface; lift the Alex HPF/LPF compute helpers into a shared `AlexFilterMap`; recompute filter bits on every frequency change in P1 (already done in P2); fix the two reported HL2 bugs (BPF doesn't switch on band change; S-ATT non-functional). Preserve byte-identical output for every non-HL2 board via a regression-freeze JSON baseline captured before the refactor.

**Architecture:** Hybrid per-board codec subclasses (mirrors Thetis's literal `WriteMainLoop` vs `WriteMainLoop_HL2` split) with a small `IP1Codec` interface and `CodecContext` POD. Behavioral splits go in subclasses (4 of them: `Standard`, `AnvelinaPro3`, `RedPitaya`, `Hl2`). Parameter differences (ATT mask + enable bit + MOX-branch) live in `BoardCapabilities::Attenuator`. `P1RadioConnection` chooses the codec at connect time from `m_hardwareProfile.model` and delegates `composeCcForBank` to it. A `NEREUS_USE_LEGACY_P1_CODEC=1` env-var feature flag lets users revert to the legacy compose path for one release cycle as a rollback hatch.

**Tech Stack:** C++20, Qt6 (`QtTest`, `qint*` types), CMake + Ninja, `nereus_add_test()` macro from `tests/CMakeLists.txt`. GPG-signed commits required (per maintainer memory). Pre-push hook runs `scripts/verify-thetis-headers.py` + `scripts/check-new-ports.py` + `scripts/verify-inline-cites.py` + `scripts/compliance-inventory.py --fail-on-unclassified`.

**Spec:** [`docs/architecture/2026-04-20-all-board-radio-control-parity-spec.md`](2026-04-20-all-board-radio-control-parity-spec.md) §6 (Phase A in full).

**Upstream version stamps used in this plan:** `[ramdor 501e3f5]` · `[mi0bot c26a8a4]`

---

## Definition of Done (lifted from spec §6.6)

1. Branch `phase3p-a-p1-wire-parity` merged into `main` via PR.
2. `setAttenuator(20)` on HL2 → byte trace `C4 = 0x54` in bank 11 during RX, `C4 = 0x54` with MOX=1 in TX.
3. `setReceiverFrequency(14_100_000)` on HL2 → byte trace bank 10 `C3 = 0x01` (13 MHz HPF), `C4 = 0x01` (30/20 LPF).
4. RxApplet S-ATT slider on HL2 ranges 0–63; on Hermes/Orion ranges 0–31.
5. Byte-identical output vs pre-refactor main for every non-HL2 board (regression-freeze test green).
6. `verify-thetis-headers.py` + `check-new-ports.py` + `verify-inline-cites.py` pass.
7. 5 new rows in `docs/attribution/THETIS-PROVENANCE.md`.
8. Every ported logic block in the 6 new files has a `[ramdor 501e3f5]` and/or `[mi0bot c26a8a4]` stamp.
9. No regression in existing P1 test suite (especially `tst_p1_wire_format`, `tst_p1_loopback_connection`, `tst_reconnect_on_silence`).
10. `CHANGELOG.md` entry under unreleased.

---

## File structure (locked)

**Created:**
- `src/core/codec/CodecContext.h` — POD: MOX bit, per-ADC step ATT (RX + TX), per-ADC preamp, Alex HPF/LPF bits, TX drive, freq words, OC byte. NereusSDR-original; no attribution row.
- `src/core/codec/IP1Codec.h` — interface with `composeCcForBank`, `maxBank`, `usesI2cIntercept`. NereusSDR-original; no attribution row.
- `src/core/codec/AlexFilterMap.{h,cpp}` — pure functions `computeHpf(freqMhz)` / `computeLpf(freqMhz)`. Ports `Console/console.cs:6830-6942, 7168-7234` `[ramdor 501e3f5]`. Variant `thetis-samphire`.
- `src/core/codec/P1CodecStandard.{h,cpp}` — ramdor `WriteMainLoop`. Ports `ChannelMaster/networkproto1.c:419-698` `[ramdor 501e3f5]`. Variant `thetis-no-samphire`.
- `src/core/codec/P1CodecAnvelinaPro3.{h,cpp}` — extends Standard with bank 17 extra OC. Ports `networkproto1.c:668-674, 682` `[ramdor 501e3f5]`. Variant `thetis-no-samphire`.
- `src/core/codec/P1CodecRedPitaya.{h,cpp}` — extends Standard with bank 12 ADC1 MOX carve-out. Ports `networkproto1.c:606-616` `[ramdor 501e3f5]` (DH1KLM contribution). Variant `thetis-no-samphire`.
- `src/core/codec/P1CodecHl2.{h,cpp}` — mi0bot `WriteMainLoop_HL2`. Ports `mi0bot-Thetis/ChannelMaster/networkproto1.c:869-1201` `[mi0bot c26a8a4]`. Variant `mi0bot`.
- `tests/tst_alex_filter_map.cpp` — frequency→bits assertions per spec §6.4.
- `tests/tst_p1_codec_standard.cpp` — table-driven byte assertions for ramdor encoding.
- `tests/tst_p1_codec_anvelina_pro3.cpp` — bank 17 extra OC.
- `tests/tst_p1_codec_redpitaya.cpp` — bank 12 ADC1 MOX carve-out.
- `tests/tst_p1_codec_hl2.cpp` — mi0bot HL2 6-bit ATT + bank 17/18.
- `tests/tst_p1_regression_freeze.cpp` — diffs every (bank × board × MOX) tuple against `tests/data/p1_baseline_bytes.json`.
- `tests/data/p1_baseline_bytes.json` — generated once in Task 1; checked in.
- `.github/PULL_REQUEST_TEMPLATE.md` — attribution checklist (one-time setup).

**Modified:**
- `src/core/P1RadioConnection.h` — drop `composeCcForBank` / `composeCcBank*` instance-method declarations (keep static helpers used by `tst_p1_wire_format`); add `std::unique_ptr<IP1Codec> m_codec`.
- `src/core/P1RadioConnection.cpp` — `composeCcForBank` becomes a one-liner delegating to `m_codec`; `applyBoardQuirks()` instantiates the codec from `m_hardwareProfile.model`; `setReceiverFrequency` and `setTxFrequency` call `AlexFilterMap` and update bits; honor `NEREUS_USE_LEGACY_P1_CODEC` env var.
- `src/core/P2RadioConnection.cpp` — delete inline `computeAlexHpf` / `computeAlexLpf` (current lines 916-968); call shared `AlexFilterMap::computeHpf/computeLpf` from `setReceiverFrequency`.
- `src/core/BoardCapabilities.{h,cpp}` — add `Attenuator::{mask, enableBit, moxBranchesAtt}`; populate per-board.
- `src/gui/applets/RxApplet.cpp` — S-ATT spinbox `setMaximum()` reads `m_caps->attenuator.maxDb` instead of hardcoded 31.
- `tests/CMakeLists.txt` — register 6 new test executables.
- `docs/attribution/THETIS-PROVENANCE.md` — 5 new rows.
- `CHANGELOG.md` — bug-fix entry.

---

## Task 1: Create branch, capture regression-freeze baseline

**Files:**
- Create: `tests/tst_p1_regression_freeze_capture.cpp` (temporary helper, deleted after baseline emit)
- Create: `tests/data/p1_baseline_bytes.json`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Branch off `main`**

```bash
cd /Users/j.j.boyd/NereusSDR
git checkout main
git pull origin main
git checkout -b phase3p-a-p1-wire-parity
```

- [ ] **Step 2: Write the baseline-capture helper**

Create `tests/tst_p1_regression_freeze_capture.cpp`:

```cpp
// Temporary helper — captures current P1RadioConnection::composeCcForBank
// output for every (bank × board × MOX) tuple to JSON. Run once on the
// pre-refactor codebase, commit the JSON, then this file is deleted.
//
// The shipping regression test (tst_p1_regression_freeze.cpp) loads the
// JSON and asserts the codec output matches for every non-HL2 board.

#include <QtTest/QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/P1RadioConnection.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class CaptureP1Baseline : public QObject {
    Q_OBJECT
private slots:
    void emitBaseline() {
        const QList<HPSDRHW> boards = {
            HPSDRHW::Atlas, HPSDRHW::Hermes, HPSDRHW::HermesII,
            HPSDRHW::Angelia, HPSDRHW::Orion, HPSDRHW::OrionMKII,
            HPSDRHW::AnvelinaPro3, HPSDRHW::HermesLite, HPSDRHW::RedPitaya,
            HPSDRHW::AnanG2, HPSDRHW::AnanG2_1K
        };

        QJsonArray rows;
        for (HPSDRHW board : boards) {
            P1RadioConnection conn(nullptr);
            conn.setBoardForTest(board);
            for (bool mox : {false, true}) {
                conn.setMox(mox);
                for (int bank = 0; bank <= 17; ++bank) {
                    quint8 out[5] = {};
                    conn.composeCcForBank(bank, out);
                    QJsonObject row;
                    row["board"] = static_cast<int>(board);
                    row["mox"]   = mox;
                    row["bank"]  = bank;
                    QJsonArray bytes;
                    for (int i = 0; i < 5; ++i) { bytes.append(int(out[i])); }
                    row["bytes"] = bytes;
                    rows.append(row);
                }
            }
        }

        const QString path = QStringLiteral(NEREUS_TEST_DATA_DIR) + "/p1_baseline_bytes.json";
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(rows).toJson(QJsonDocument::Indented));
        qInfo() << "Wrote baseline:" << path << rows.size() << "rows";
    }
};

QTEST_APPLESS_MAIN(CaptureP1Baseline)
#include "tst_p1_regression_freeze_capture.moc"
```

- [ ] **Step 3: Wire the helper into CMake**

Edit `tests/CMakeLists.txt`. Find the section that registers Phase 3I tests (`nereus_add_test(tst_p1_wire_format)` neighborhood) and add immediately after:

```cmake
# ── Phase 3P-A regression freeze capture (temporary; removed after baseline emit) ──
nereus_add_test(tst_p1_regression_freeze_capture)
target_compile_definitions(tst_p1_regression_freeze_capture PRIVATE
    NEREUS_BUILD_TESTS
    NEREUS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")
```

- [ ] **Step 4: Build + run capture, verify JSON appears**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON
cmake --build build -j$(sysctl -n hw.ncpu) --target tst_p1_regression_freeze_capture
./build/tests/tst_p1_regression_freeze_capture
test -f tests/data/p1_baseline_bytes.json && wc -l tests/data/p1_baseline_bytes.json
```

Expected: PASS · file exists · ~4750 lines (11 boards × 2 MOX × 18 banks × 12 lines/row).

- [ ] **Step 5: Commit branch + baseline + capture helper**

```bash
git add tests/tst_p1_regression_freeze_capture.cpp tests/data/p1_baseline_bytes.json tests/CMakeLists.txt
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
test(p1): capture pre-refactor regression-freeze baseline (Phase 3P-A Task 1)

Snapshots P1RadioConnection::composeCcForBank output for every
(bank × board × MOX) tuple across 11 board types into
tests/data/p1_baseline_bytes.json. After Phase A's codec refactor,
tst_p1_regression_freeze.cpp will load this JSON and assert the
new codec output matches byte-for-byte for every non-HL2 board.

Capture helper deleted in Task 16 once baseline test ships.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: `CodecContext` POD + `IP1Codec` interface

**Files:**
- Create: `src/core/codec/CodecContext.h`
- Create: `src/core/codec/IP1Codec.h`

These are NereusSDR-original interfaces — no Thetis port, no attribution row in PROVENANCE.

- [ ] **Step 1: Create `CodecContext.h`**

```cpp
// =================================================================
// src/core/codec/CodecContext.h  (NereusSDR)
// =================================================================
//
// POD aggregating the live state every IP1Codec subclass needs to
// produce a 5-byte C&C bank payload. Lifted from P1RadioConnection
// member variables to make codec subclasses pure functions of
// {ctx × bank} — trivially unit-testable without a live socket.
//
// NereusSDR-original. No Thetis port; no PROVENANCE row.
// =================================================================

#pragma once

#include <QtGlobal>

namespace NereusSDR {

struct CodecContext {
    // MOX (transmit) bit — OR'd into low bit of C0.
    bool    mox{false};

    // RX-side step attenuator per ADC, 0-63 dB (clamped per board).
    int     rxStepAttn[3]{};

    // TX-side step attenuator per ADC. HL2 sends txStepAttn[0] when MOX=1
    // (mi0bot networkproto1.c:1099-1102).
    int     txStepAttn[3]{};

    // Per-RX preamp enable bits.
    bool    rxPreamp[3]{};

    // Alex HPF / LPF bits — recomputed by P1RadioConnection on freq change
    // via AlexFilterMap. Codec only emits them.
    quint8  alexHpfBits{0};
    quint8  alexLpfBits{0};

    // TX drive level (0-255).
    int     txDrive{0};

    // PA enable (bank 10 C3 bit 7).
    bool    paEnabled{false};

    // RX VFO frequency words (Hz, raw, no phase-word conversion on P1).
    quint64 rxFreqHz[7]{};
    quint64 txFreqHz{0};

    // Sample rate code (0=48k, 1=96k, 2=192k, 3=384k).
    int     sampleRateCode{0};

    // Number of active DDCs (NDDC), 1..7.
    int     activeRxCount{1};

    // OC output byte (bank 0 C2). Phase D will drive this from OcMatrix.
    quint8  ocByte{0};

    // ADC-to-DDC routing (bank 4 C1+C2).
    quint16 adcCtrl{0};

    // Antenna selection (bank 0 C4 low 2 bits).
    int     antennaIdx{0};

    // Duplex / diversity (bank 0 C4 bits 2 + 7).
    bool    duplex{true};
    bool    diversity{false};

    // HL2-only state (mi0bot networkproto1.c:1162-1176, banks 17-18).
    int     hl2PttHang{0};       // 5-bit
    int     hl2TxLatency{0};     // 7-bit
    bool    hl2ResetOnDisconnect{false};
};

} // namespace NereusSDR
```

- [ ] **Step 2: Create `IP1Codec.h`**

```cpp
// =================================================================
// src/core/codec/IP1Codec.h  (NereusSDR)
// =================================================================
//
// Per-board codec interface for the Protocol 1 EP2 C&C bank compose
// layer. Subclasses model Thetis's behavioral splits literally:
// P1CodecStandard mirrors ramdor's WriteMainLoop; P1CodecHl2
// mirrors mi0bot's WriteMainLoop_HL2; P1CodecAnvelinaPro3 and
// P1CodecRedPitaya extend Standard with the per-board carve-outs
// ramdor's main loop already gates with `if (HPSDRModel == ...)`.
//
// P1RadioConnection owns std::unique_ptr<IP1Codec> chosen at connect
// time from m_hardwareProfile.model (see applyBoardQuirks()).
//
// NereusSDR-original. No Thetis port; no PROVENANCE row.
// =================================================================

#pragma once

#include <QtGlobal>
#include "CodecContext.h"

namespace NereusSDR {

class IP1Codec {
public:
    virtual ~IP1Codec() = default;

    // Emit the 5-byte C&C payload for the given bank into out[0..4].
    // bank ∈ [0, maxBank()].
    virtual void composeCcForBank(int bank, const CodecContext& ctx,
                                  quint8 out[5]) const = 0;

    // Highest bank index this codec emits. Standard = 16, AnvelinaPro3 = 17,
    // Hl2 = 18. P1RadioConnection cycles 0..maxBank() round-robin.
    virtual int  maxBank() const = 0;

    // True for HL2 — when the I2C queue (Phase E) is non-empty, frame
    // compose is overridden to carry I2C TLV bytes instead of normal
    // C&C. False for Standard / AnvelinaPro3 / RedPitaya.
    virtual bool usesI2cIntercept() const { return false; }
};

} // namespace NereusSDR
```

- [ ] **Step 3: Verify both compile (via header-include smoke)**

Add a temporary include in `tests/tst_smoke.cpp` (or any existing test):

```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target tst_smoke
```

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/core/codec/CodecContext.h src/core/codec/IP1Codec.h
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
feat(codec): IP1Codec interface + CodecContext POD (Phase 3P-A Task 2)

NereusSDR-original interface for the per-board P1 C&C compose layer.
Subclasses (P1CodecStandard, P1CodecAnvelinaPro3, P1CodecRedPitaya,
P1CodecHl2) land in subsequent tasks. No PROVENANCE row required —
no Thetis port at this layer.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: `AlexFilterMap` — TDD test

**Files:**
- Create: `tests/tst_alex_filter_map.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_alex_filter_map.cpp`:

```cpp
#include <QtTest/QtTest>
#include "core/codec/AlexFilterMap.h"

using namespace NereusSDR::codec::alex;

class TestAlexFilterMap : public QObject {
    Q_OBJECT
private slots:
    // From Thetis console.cs:6830-6942 [ramdor 501e3f5]
    void hpfBypass_under_1_5MHz()       { QCOMPARE(computeHpf(1.0),  quint8(0x20)); }
    void hpf_1_5_to_6_5_MHz()           { QCOMPARE(computeHpf(3.5),  quint8(0x10)); }
    void hpf_6_5_to_9_5_MHz()           { QCOMPARE(computeHpf(7.0),  quint8(0x08)); }
    void hpf_9_5_to_13_MHz()            { QCOMPARE(computeHpf(10.0), quint8(0x04)); }
    void hpf_13_to_20_MHz()             { QCOMPARE(computeHpf(14.1), quint8(0x01)); }
    void hpf_20_to_50_MHz()             { QCOMPARE(computeHpf(28.0), quint8(0x02)); }
    void hpf_6m_preamp_50_MHz_and_up()  { QCOMPARE(computeHpf(50.0), quint8(0x40)); }

    // From Thetis console.cs:7168-7234 [ramdor 501e3f5]
    void lpf_160m_under_2MHz()  { QCOMPARE(computeLpf(1.9),   quint8(0x08)); }
    void lpf_80m_2_to_4_MHz()   { QCOMPARE(computeLpf(3.8),   quint8(0x04)); }
    void lpf_60_40m()           { QCOMPARE(computeLpf(7.1),   quint8(0x02)); }
    void lpf_30_20m()           { QCOMPARE(computeLpf(14.1),  quint8(0x01)); }
    void lpf_17_15m()           { QCOMPARE(computeLpf(21.0),  quint8(0x40)); }
    void lpf_12_10m()           { QCOMPARE(computeLpf(28.0),  quint8(0x20)); }
    void lpf_6m_29_7_and_up()   { QCOMPARE(computeLpf(50.0),  quint8(0x10)); }

    // Boundary edges — values exactly on the breakpoint go to the upper band
    void hpf_edge_1_5_MHz_exact()  { QCOMPARE(computeHpf(1.5),  quint8(0x10)); }
    void hpf_edge_50_MHz_exact()   { QCOMPARE(computeHpf(50.0), quint8(0x40)); }
    void lpf_edge_2_0_MHz_exact()  { QCOMPARE(computeLpf(2.0),  quint8(0x04)); }
    void lpf_edge_29_7_MHz_exact() { QCOMPARE(computeLpf(29.7), quint8(0x10)); }
};

QTEST_APPLESS_MAIN(TestAlexFilterMap)
#include "tst_alex_filter_map.moc"
```

- [ ] **Step 2: Wire into CMake**

Edit `tests/CMakeLists.txt`. Add near the Phase 3I tests:

```cmake
# ── Phase 3P-A codec tests ────────────────────────────────────────────────
nereus_add_test(tst_alex_filter_map)
```

- [ ] **Step 3: Run test, verify it fails to build (header missing)**

```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target tst_alex_filter_map 2>&1 | tail -5
```

Expected: BUILD FAIL — `'core/codec/AlexFilterMap.h' file not found`.

---

## Task 4: `AlexFilterMap` implementation

**Files:**
- Create: `src/core/codec/AlexFilterMap.h`
- Create: `src/core/codec/AlexFilterMap.cpp`
- Modify: `docs/attribution/THETIS-PROVENANCE.md`

This is a **new attribution event** — `AlexFilterMap` ports from `console.cs` (Samphire territory). Per `CLAUDE.md` §Source-First, the header and the PROVENANCE row land in the same commit.

- [ ] **Step 1: Capture upstream header from `console.cs`**

```bash
head -50 "/Users/j.j.boyd/Thetis/Project Files/Source/Console/console.cs" > /tmp/console_cs_header.txt
sed -n '40000,40050p' "/Users/j.j.boyd/Thetis/Project Files/Source/Console/console.cs" | head -40
```

Use the verbatim header from `console.cs` lines 1–50 in the new file's header block.

- [ ] **Step 2: Create `AlexFilterMap.h`**

```cpp
// =================================================================
// src/core/codec/AlexFilterMap.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs:6830-6942 (setAlexHPF)
//   Project Files/Source/Console/console.cs:7168-7234 (setAlexLPF)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Lifted from P2RadioConnection::computeAlexHpf/Lpf
//                (which had ported the same console.cs logic) into a
//                shared header so P1RadioConnection can call it too.
//                Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
// =================================================================
//
// === Verbatim Thetis console.cs header (lines 1-50 of upstream) ===
// [Insert console.cs:1-50 verbatim. Copy/paste from
//  /Users/j.j.boyd/Thetis/Project Files/Source/Console/console.cs.]
// =================================================================

#pragma once

#include <QtGlobal>

namespace NereusSDR::codec::alex {

// Frequency → Alex HPF select bits (bank 10 C3 in the P1 packet,
// or bytes 1432-1435 in the P2 CmdHighPriority packet).
//
// From Thetis console.cs:6830-6942 [ramdor 501e3f5]
quint8 computeHpf(double freqMhz);

// Frequency → Alex LPF select bits (bank 10 C4 in the P1 packet,
// or bytes 1428-1431 in the P2 CmdHighPriority packet).
//
// From Thetis console.cs:7168-7234 [ramdor 501e3f5]
quint8 computeLpf(double freqMhz);

} // namespace NereusSDR::codec::alex
```

**Replace the `[Insert console.cs:1-50 verbatim ...]` line** with the actual upstream header. This is non-negotiable per §4 of the spec.

- [ ] **Step 3: Create `AlexFilterMap.cpp`**

```cpp
// =================================================================
// src/core/codec/AlexFilterMap.cpp  (NereusSDR)
// =================================================================
// (Same header block as AlexFilterMap.h — copy verbatim including
//  upstream console.cs header.)
// =================================================================

#include "AlexFilterMap.h"

namespace NereusSDR::codec::alex {

// From Thetis console.cs:6830-6942 [ramdor 501e3f5]
// Decision rationale: spec §6.3.1
quint8 computeHpf(double freqMhz)
{
    if (freqMhz < 1.5)  { return 0x20; }    // bypass
    if (freqMhz < 6.5)  { return 0x10; }    // 1.5 MHz HPF
    if (freqMhz < 9.5)  { return 0x08; }    // 6.5 MHz HPF
    if (freqMhz < 13.0) { return 0x04; }    // 9.5 MHz HPF
    if (freqMhz < 20.0) { return 0x01; }    // 13 MHz HPF
    if (freqMhz < 50.0) { return 0x02; }    // 20 MHz HPF
    return 0x40;                             // 6m preamp
}

// From Thetis console.cs:7168-7234 [ramdor 501e3f5]
// Decision rationale: spec §6.3.1
quint8 computeLpf(double freqMhz)
{
    if (freqMhz < 2.0)   { return 0x08; }   // 160m LPF
    if (freqMhz < 4.0)   { return 0x04; }   // 80m LPF
    if (freqMhz < 7.3)   { return 0x02; }   // 60/40m LPF
    if (freqMhz < 14.35) { return 0x01; }   // 30/20m LPF
    if (freqMhz < 21.45) { return 0x40; }   // 17/15m LPF
    if (freqMhz < 29.7)  { return 0x20; }   // 12/10m LPF
    return 0x10;                             // 6m LPF
}

} // namespace NereusSDR::codec::alex
```

- [ ] **Step 4: Add CMake entry for the new source**

Edit `src/CMakeLists.txt` (or wherever `src/core/*.cpp` is enumerated). Find the section listing core sources and add:

```cmake
core/codec/AlexFilterMap.cpp
```

- [ ] **Step 5: Add PROVENANCE row**

Edit `docs/attribution/THETIS-PROVENANCE.md`. Find the alphabetical section for files starting with `src/core/c...` and add:

```markdown
| src/core/codec/AlexFilterMap.cpp | Project Files/Source/Console/console.cs | 6830-6942; 7168-7234 | port | thetis-samphire | freq→Alex HPF/LPF bits, lifted from P2RadioConnection inline copy into shared header so P1 can call it too |
| src/core/codec/AlexFilterMap.h | Project Files/Source/Console/console.cs | 6830-6942; 7168-7234 | port | thetis-samphire | header mirrors .cpp |
```

- [ ] **Step 6: Build + run test, verify PASS**

```bash
cmake --build build -j$(sysctl -n hw.ncpu) --target tst_alex_filter_map
./build/tests/tst_alex_filter_map
```

Expected: 18/18 PASS.

- [ ] **Step 7: Verify attribution gates pass locally**

```bash
python3 scripts/verify-thetis-headers.py src/core/codec/AlexFilterMap.h src/core/codec/AlexFilterMap.cpp
python3 scripts/check-new-ports.py
python3 scripts/verify-inline-cites.py
```

Expected: all PASS.

- [ ] **Step 8: Commit**

```bash
git add src/core/codec/AlexFilterMap.h src/core/codec/AlexFilterMap.cpp \
        src/CMakeLists.txt tests/tst_alex_filter_map.cpp tests/CMakeLists.txt \
        docs/attribution/THETIS-PROVENANCE.md
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
feat(codec): AlexFilterMap shared freq→bits helpers (Phase 3P-A Task 3+4)

Lifts computeAlexHpf/Lpf out of P2RadioConnection inline (where it was
correctly ported but only used by P2) into src/core/codec/ so P1 can
share the same Thetis-canonical math. Per spec §6.4. P2 still gets
byte-identical output (verified in Task 14).

Ports console.cs:6830-6942 + 7168-7234 [ramdor 501e3f5]. Verbatim
upstream header preserved. PROVENANCE row added.

Tests: tst_alex_filter_map covers all 7 HPF buckets, all 7 LPF buckets,
and 4 boundary-edge cases (e.g. 1.5 MHz exact → upper band).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: `P1CodecStandard` — TDD tests (table-driven)

**Files:**
- Create: `tests/tst_p1_codec_standard.cpp`
- Modify: `tests/CMakeLists.txt`

Tests are table-driven via QtTest's `_data()` slot pattern — one function drives every bank × scenario.

- [ ] **Step 1: Write the failing test**

Create `tests/tst_p1_codec_standard.cpp`:

```cpp
#include <QtTest/QtTest>
#include "core/codec/P1CodecStandard.h"

using namespace NereusSDR;

class TestP1CodecStandard : public QObject {
    Q_OBJECT
private slots:
    void maxBank_is_16() {
        P1CodecStandard codec;
        QCOMPARE(codec.maxBank(), 16);
    }

    void usesI2cIntercept_is_false() {
        P1CodecStandard codec;
        QVERIFY(!codec.usesI2cIntercept());
    }

    // Table-driven: every (bank × scenario) row asserts the 5-byte output.
    // Citations on each row reference networkproto1.c [ramdor 501e3f5]
    // unless explicitly noted.
    void compose_data() {
        QTest::addColumn<int>("bank");
        QTest::addColumn<bool>("mox");
        QTest::addColumn<int>("c0_expected");
        QTest::addColumn<int>("c1_expected");
        QTest::addColumn<int>("c2_expected");
        QTest::addColumn<int>("c3_expected");
        QTest::addColumn<int>("c4_expected");
        QTest::addColumn<QByteArray>("ctx_overrides_json");

        // Bank 0 — sample rate 48k, no MOX, NDDC=1, antenna 0
        // Source: networkproto1.c:446-471 [ramdor 501e3f5]
        QTest::newRow("bank0_rx_48k_ant0")
            << 0 << false
            << 0x00 << 0x00 << 0x00 << 0x00 << 0x04
            << QByteArray(R"({"sampleRateCode":0,"activeRxCount":1,"antennaIdx":0})");

        // Bank 11 — RX ATT 20 dB, no MOX (5-bit mask + 0x20 enable)
        // Source: networkproto1.c:601 [ramdor 501e3f5]
        QTest::newRow("bank11_rx_att_20dB_ramdor_encoding")
            << 11 << false
            << 0x14 << 0x00 << 0x00 << 0x00 << ((20 & 0x1F) | 0x20)
            << QByteArray(R"({"rxStepAttn":[20,0,0]})");

        // Bank 11 — RX ATT 31 dB max
        // Source: networkproto1.c:601 [ramdor 501e3f5]
        QTest::newRow("bank11_rx_att_31dB_max")
            << 11 << false
            << 0x14 << 0x00 << 0x00 << 0x00 << ((31 & 0x1F) | 0x20)
            << QByteArray(R"({"rxStepAttn":[31,0,0]})");

        // Bank 12 — ADC1 ATT during RX (no MOX), value 7
        // Source: networkproto1.c:606-616 [ramdor 501e3f5]
        QTest::newRow("bank12_rx_adc1_att_7dB")
            << 12 << false
            << 0x16 << ((7 & 0xFF) | 0x20) << ((0 & 0x1F) | 0x20) << 0x00 << 0x00
            << QByteArray(R"({"rxStepAttn":[0,7,0]})");

        // Bank 12 — ADC1 forced to 31 dB during MOX (Standard codec — non-RedPitaya)
        // Source: networkproto1.c:609 [ramdor 501e3f5]
        QTest::newRow("bank12_mox_adc1_forced_31dB")
            << 12 << true
            << 0x17 << (0x1F | 0x20) << (0x00 | 0x20) << 0x00 << 0x00
            << QByteArray(R"({"rxStepAttn":[0,12,0]})");  // 12 ignored under MOX

        // Bank 10 — Alex HPF/LPF passthrough, PA enabled
        // Source: networkproto1.c:583-590 [ramdor 501e3f5]
        QTest::newRow("bank10_alex_passthrough_pa_on")
            << 10 << false
            << 0x12 << 0x00 << 0x40 << (0x01 | 0x80) << 0x01
            << QByteArray(R"({"alexHpfBits":1,"alexLpfBits":1,"paEnabled":true})");
    }

    void compose() {
        QFETCH(int, bank);
        QFETCH(bool, mox);
        QFETCH(int, c0_expected);
        QFETCH(int, c1_expected);
        QFETCH(int, c2_expected);
        QFETCH(int, c3_expected);
        QFETCH(int, c4_expected);
        QFETCH(QByteArray, ctx_overrides_json);

        CodecContext ctx;
        ctx.mox = mox;
        applyOverrides(ctx, ctx_overrides_json);

        P1CodecStandard codec;
        quint8 out[5] = {};
        codec.composeCcForBank(bank, ctx, out);

        QCOMPARE(int(out[0]), c0_expected);
        QCOMPARE(int(out[1]), c1_expected);
        QCOMPARE(int(out[2]), c2_expected);
        QCOMPARE(int(out[3]), c3_expected);
        QCOMPARE(int(out[4]), c4_expected);
    }

private:
    // Tiny JSON helper — keeps the data table compact. Only handles the
    // fields used by the tests above; expand as new rows are added.
    static void applyOverrides(CodecContext& ctx, const QByteArray& json);
};

void TestP1CodecStandard::applyOverrides(CodecContext& ctx, const QByteArray& json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonObject o = doc.object();
    if (o.contains("sampleRateCode")) ctx.sampleRateCode = o["sampleRateCode"].toInt();
    if (o.contains("activeRxCount"))  ctx.activeRxCount  = o["activeRxCount"].toInt();
    if (o.contains("antennaIdx"))     ctx.antennaIdx     = o["antennaIdx"].toInt();
    if (o.contains("alexHpfBits"))    ctx.alexHpfBits    = quint8(o["alexHpfBits"].toInt());
    if (o.contains("alexLpfBits"))    ctx.alexLpfBits    = quint8(o["alexLpfBits"].toInt());
    if (o.contains("paEnabled"))      ctx.paEnabled      = o["paEnabled"].toBool();
    if (o.contains("rxStepAttn")) {
        auto arr = o["rxStepAttn"].toArray();
        for (int i = 0; i < 3 && i < arr.size(); ++i) ctx.rxStepAttn[i] = arr[i].toInt();
    }
    if (o.contains("txStepAttn")) {
        auto arr = o["txStepAttn"].toArray();
        for (int i = 0; i < 3 && i < arr.size(); ++i) ctx.txStepAttn[i] = arr[i].toInt();
    }
}

QTEST_APPLESS_MAIN(TestP1CodecStandard)
#include "tst_p1_codec_standard.moc"
```

- [ ] **Step 2: Wire CMake**

Edit `tests/CMakeLists.txt` — add under the Phase 3P-A section started in Task 3:

```cmake
nereus_add_test(tst_p1_codec_standard)
```

- [ ] **Step 3: Run, verify build failure**

```bash
cmake --build build --target tst_p1_codec_standard 2>&1 | tail -5
```

Expected: BUILD FAIL — `'core/codec/P1CodecStandard.h' file not found`. Tests fail, no commit yet.

---

## Task 6: `P1CodecStandard` — implementation

**Files:**
- Create: `src/core/codec/P1CodecStandard.h`
- Create: `src/core/codec/P1CodecStandard.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `docs/attribution/THETIS-PROVENANCE.md`

This is a port of ramdor's `WriteMainLoop` (networkproto1.c:419-698). Variant `thetis-no-samphire` (ChannelMaster is LGPL, Samphire-clean). Verbatim upstream header required.

- [ ] **Step 1: Capture upstream header**

```bash
head -45 "/Users/j.j.boyd/Thetis/Project Files/Source/ChannelMaster/networkproto1.c"
```

Use lines 1-45 verbatim in the new file's header block.

- [ ] **Step 2: Create `P1CodecStandard.h`**

```cpp
// =================================================================
// src/core/codec/P1CodecStandard.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c:419-698 (WriteMainLoop)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Lifted from P1RadioConnection::composeCcForBank
//                which previously held the inline ramdor port; now
//                delegates here.
// =================================================================
//
// === Verbatim Thetis ChannelMaster/networkproto1.c header ===
// [Insert networkproto1.c:1-45 verbatim from
//  /Users/j.j.boyd/Thetis/Project Files/Source/ChannelMaster/networkproto1.c]
// =================================================================

#pragma once

#include "IP1Codec.h"

namespace NereusSDR {

// ramdor WriteMainLoop port. Banks 0-16. Used for every non-HL2 P1
// board by default; AnvelinaPro3 and RedPitaya extend it with bank-17
// or bank-12 overrides (see those subclasses).
class P1CodecStandard : public IP1Codec {
public:
    void composeCcForBank(int bank, const CodecContext& ctx, quint8 out[5]) const override;
    int  maxBank() const override { return 16; }

protected:
    // Helpers exposed so subclasses can call into specific banks they
    // don't override.
    void bank0(const CodecContext& ctx, quint8 out[5]) const;
    void bank10(const CodecContext& ctx, quint8 out[5]) const;
    void bank11(const CodecContext& ctx, quint8 out[5]) const;
    virtual void bank12(const CodecContext& ctx, quint8 out[5]) const;  // RedPitaya overrides
};

} // namespace NereusSDR
```

- [ ] **Step 3: Create `P1CodecStandard.cpp`**

```cpp
// =================================================================
// src/core/codec/P1CodecStandard.cpp  (NereusSDR)
// =================================================================
// (Same header block as P1CodecStandard.h.)
// =================================================================

#include "P1CodecStandard.h"

namespace NereusSDR {

void P1CodecStandard::composeCcForBank(int bank, const CodecContext& ctx,
                                       quint8 out[5]) const
{
    // C0 low bit = MOX
    const quint8 C0base = ctx.mox ? 0x01 : 0x00;
    // Zero-init out[1..4] before the per-bank switch fills them.
    for (int i = 1; i < 5; ++i) { out[i] = 0; }

    switch (bank) {
        case 0:  bank0(ctx, out);  return;
        case 10: bank10(ctx, out); return;
        case 11: bank11(ctx, out); return;
        case 12: bank12(ctx, out); return;

        // Bank 1 — TX VFO
        // Source: networkproto1.c:477-481 [ramdor 501e3f5]
        case 1:
            out[0] = C0base | 0x02;
            out[1] = quint8((ctx.txFreqHz >> 24) & 0xFF);
            out[2] = quint8((ctx.txFreqHz >> 16) & 0xFF);
            out[3] = quint8((ctx.txFreqHz >>  8) & 0xFF);
            out[4] = quint8( ctx.txFreqHz        & 0xFF);
            return;

        // Banks 2-9 — RX VFOs (0..7), one per bank
        // Source: networkproto1.c:485-576 [ramdor 501e3f5]
        case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: {
            out[0] = C0base | quint8(0x04 + (bank - 2) * 2);
            const int rxIdx = bank - 2;
            const quint64 freq = (rxIdx < ctx.activeRxCount)
                                  ? ctx.rxFreqHz[rxIdx]
                                  : ctx.txFreqHz;  // unused DDCs default to TX freq
            out[1] = quint8((freq >> 24) & 0xFF);
            out[2] = quint8((freq >> 16) & 0xFF);
            out[3] = quint8((freq >>  8) & 0xFF);
            out[4] = quint8( freq        & 0xFF);
            return;
        }

        // Bank 13 — CW enable / sidetone level
        // Source: networkproto1.c:634-638 [ramdor 501e3f5]
        case 13: out[0] = C0base | 0x1E; return;

        // Bank 14 — CW hang / sidetone freq
        // Source: networkproto1.c:642-646 [ramdor 501e3f5]
        case 14: out[0] = C0base | 0x20; return;

        // Bank 15 — EER PWM
        // Source: networkproto1.c:650-654 [ramdor 501e3f5]
        case 15: out[0] = C0base | 0x22; return;

        // Bank 16 — BPF2
        // Source: networkproto1.c:658-665 [ramdor 501e3f5]
        case 16: out[0] = C0base | 0x24; return;

        default:
            // Standard codec only emits banks 0-16; subclasses (AP3, HL2)
            // extend the range and override composeCcForBank to handle
            // banks 17/18 before delegating here.
            out[0] = C0base;
            return;
    }
}

// Bank 0 — General settings: sample rate, OC, preamp/dither/random,
// antenna, duplex, NDDC, diversity.
// Source: networkproto1.c:446-471 [ramdor 501e3f5 / mi0bot c26a8a4 — identical]
void P1CodecStandard::bank0(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x00;
    out[1] = quint8(ctx.sampleRateCode & 0x03);
    out[2] = quint8((ctx.ocByte << 1) & 0xFE);
    // Bits: [2]=preamp, [3]=dither, [4]=random, [6:5]=RX_in mux
    out[3] = quint8((ctx.rxPreamp[0] ? 0x04 : 0)
                  | 0x08    // dither default on
                  | 0x10);  // random default on
    // Bits: [1:0]=antenna, [2]=duplex (always set), [6:3]=NDDC-1, [7]=diversity
    out[4] = quint8((ctx.antennaIdx & 0x03)
                  | (ctx.duplex ? 0x04 : 0)
                  | (((ctx.activeRxCount - 1) & 0x0F) << 3)
                  | (ctx.diversity ? 0x80 : 0));
}

// Bank 10 — TX drive, mic, Alex HPF/LPF, PA enable
// Source: networkproto1.c:579-590 [ramdor 501e3f5]
void P1CodecStandard::bank10(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x12;
    out[1] = quint8(ctx.txDrive & 0xFF);
    out[2] = 0x40;  // line_in=0, mic_boost=0 defaults; bit 6 set per upstream
    out[3] = quint8(ctx.alexHpfBits | (ctx.paEnabled ? 0x80 : 0));
    out[4] = quint8(ctx.alexLpfBits);
}

// Bank 11 — Preamp + RX step ATT ADC0 (5-bit mask + 0x20 enable)
// Source: networkproto1.c:594-601 [ramdor 501e3f5]
void P1CodecStandard::bank11(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x14;
    out[1] = quint8((ctx.rxPreamp[0] ? 0x01 : 0)
                  | (ctx.rxPreamp[1] ? 0x02 : 0)
                  | (ctx.rxPreamp[2] ? 0x04 : 0)
                  | (ctx.rxPreamp[0] ? 0x08 : 0));  // bit3 = rx0 again (Thetis quirk)
    out[2] = 0;
    out[3] = 0;
    // canonical 5-bit ramdor encoding
    out[4] = quint8((ctx.rxStepAttn[0] & 0x1F) | 0x20);
}

// Bank 12 — Step ATT ADC1/2 + CW keyer
// Source: networkproto1.c:606-628 [ramdor 501e3f5]
//
// ADC1 carve-out: during MOX, force 0x1F UNLESS RedPitaya. Standard
// codec is non-RedPitaya; RedPitaya subclass overrides this method.
void P1CodecStandard::bank12(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x16;
    if (ctx.mox) {
        out[1] = 0x1F | 0x20;  // forced max
    } else {
        out[1] = quint8(ctx.rxStepAttn[1] & 0xFF) | 0x20;
    }
    out[2] = quint8((ctx.rxStepAttn[2] & 0x1F) | 0x20);
    // C3 / C4 = CW keyer defaults — zero for Phase A; Phase 3M-2 wires CW.
    out[3] = 0;
    out[4] = 0;
}

} // namespace NereusSDR
```

- [ ] **Step 4: Add to CMake source list + PROVENANCE rows**

Edit `src/CMakeLists.txt` to add `core/codec/P1CodecStandard.cpp` to the core sources.

Edit `docs/attribution/THETIS-PROVENANCE.md`:

```markdown
| src/core/codec/P1CodecStandard.cpp | Project Files/Source/ChannelMaster/networkproto1.c | 419-698 | port | thetis-no-samphire | ramdor WriteMainLoop port; lifted from P1RadioConnection::composeCcForBank |
| src/core/codec/P1CodecStandard.h | Project Files/Source/ChannelMaster/networkproto1.c | 419-698 | port | thetis-no-samphire | header mirrors .cpp |
```

- [ ] **Step 5: Run tests**

```bash
cmake --build build --target tst_p1_codec_standard
./build/tests/tst_p1_codec_standard
```

Expected: 9/9 PASS (2 trivial + 7 table rows).

- [ ] **Step 6: Verify attribution gates**

```bash
python3 scripts/verify-thetis-headers.py src/core/codec/P1CodecStandard.h src/core/codec/P1CodecStandard.cpp
python3 scripts/check-new-ports.py
python3 scripts/verify-inline-cites.py
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add src/core/codec/P1CodecStandard.h src/core/codec/P1CodecStandard.cpp \
        src/CMakeLists.txt tests/tst_p1_codec_standard.cpp tests/CMakeLists.txt \
        docs/attribution/THETIS-PROVENANCE.md
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
feat(codec): P1CodecStandard — ramdor WriteMainLoop port (Phase 3P-A Task 5+6)

Lifts the existing ramdor encoding for banks 0-16 out of
P1RadioConnection::composeCcForBank into a standalone codec class.
Behavior byte-identical to today's main; verified by tst_p1_codec_standard
table-driven tests covering bank 0, 10, 11, 12 (RX + MOX), and 13-16.
Full regression-freeze gate against tests/data/p1_baseline_bytes.json
lands in Task 15.

Ports networkproto1.c:419-698 [ramdor 501e3f5]. Verbatim upstream header
preserved. PROVENANCE row added.

P1RadioConnection still owns its own composeCcForBank for now; Task 11
swaps it to delegate to this class.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: `P1CodecAnvelinaPro3` — extends Standard with bank 17

**Files:**
- Create: `src/core/codec/P1CodecAnvelinaPro3.h`
- Create: `src/core/codec/P1CodecAnvelinaPro3.cpp`
- Create: `tests/tst_p1_codec_anvelina_pro3.cpp`
- Modify: `tests/CMakeLists.txt`, `src/CMakeLists.txt`, `docs/attribution/THETIS-PROVENANCE.md`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_p1_codec_anvelina_pro3.cpp`:

```cpp
#include <QtTest/QtTest>
#include "core/codec/P1CodecAnvelinaPro3.h"

using namespace NereusSDR;

class TestP1CodecAnvelinaPro3 : public QObject {
    Q_OBJECT
private slots:
    void maxBank_is_17() {
        P1CodecAnvelinaPro3 codec;
        QCOMPARE(codec.maxBank(), 17);
    }

    // Bank 17 — extra OC outputs, AnvelinaPro3 only.
    // Source: networkproto1.c:668-674 [ramdor 501e3f5]
    void bank17_extra_oc() {
        P1CodecAnvelinaPro3 codec;
        CodecContext ctx;
        ctx.ocByte = 0x05;  // oc_output_extras nibble in low 4 bits
        quint8 out[5] = {};
        codec.composeCcForBank(17, ctx, out);
        QCOMPARE(int(out[0]), 0x26);
        QCOMPARE(int(out[1]), 0x05);  // & 0x0F
        QCOMPARE(int(out[2]), 0x00);
        QCOMPARE(int(out[3]), 0x00);
        QCOMPARE(int(out[4]), 0x00);
    }

    // Banks 0-16 still match Standard
    void bank0_still_matches_standard() {
        P1CodecAnvelinaPro3 codec;
        CodecContext ctx;
        ctx.sampleRateCode = 1;
        quint8 out[5] = {};
        codec.composeCcForBank(0, ctx, out);
        QCOMPARE(int(out[1] & 0x03), 1);  // 96 kHz
    }
};

QTEST_APPLESS_MAIN(TestP1CodecAnvelinaPro3)
#include "tst_p1_codec_anvelina_pro3.moc"
```

Add to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_p1_codec_anvelina_pro3)
```

- [ ] **Step 2: Run, verify FAIL**

```bash
cmake --build build --target tst_p1_codec_anvelina_pro3 2>&1 | tail -3
```

Expected: BUILD FAIL.

- [ ] **Step 3: Create header + impl**

`src/core/codec/P1CodecAnvelinaPro3.h`:

```cpp
// =================================================================
// src/core/codec/P1CodecAnvelinaPro3.h  (NereusSDR)
// =================================================================
// (Header block: same template as P1CodecStandard. Cite networkproto1.c
//  upstream header verbatim.)
// =================================================================

#pragma once

#include "P1CodecStandard.h"

namespace NereusSDR {

// AnvelinaPro3 (ANAN-8000DLE / G8NJJ contribution) — extends Standard
// with a 17th bank carrying extra OC pin outputs (5 additional pins
// beyond the standard 7).
//
// Source: networkproto1.c:668-674 + 682 [ramdor 501e3f5]
class P1CodecAnvelinaPro3 : public P1CodecStandard {
public:
    void composeCcForBank(int bank, const CodecContext& ctx, quint8 out[5]) const override;
    int  maxBank() const override { return 17; }
};

} // namespace NereusSDR
```

`src/core/codec/P1CodecAnvelinaPro3.cpp`:

```cpp
// (Header block: same template.)

#include "P1CodecAnvelinaPro3.h"

namespace NereusSDR {

void P1CodecAnvelinaPro3::composeCcForBank(int bank, const CodecContext& ctx,
                                           quint8 out[5]) const
{
    if (bank == 17) {
        // Bank 17 — AnvelinaPro3 extra OC outputs (4 bits in C1)
        // Source: networkproto1.c:668-674 [ramdor 501e3f5]
        out[0] = (ctx.mox ? 0x01 : 0x00) | 0x26;
        out[1] = quint8(ctx.ocByte & 0x0F);
        out[2] = 0;
        out[3] = 0;
        out[4] = 0;
        return;
    }
    P1CodecStandard::composeCcForBank(bank, ctx, out);
}

} // namespace NereusSDR
```

- [ ] **Step 4: Add to CMake + PROVENANCE**

```cmake
# src/CMakeLists.txt
core/codec/P1CodecAnvelinaPro3.cpp
```

```markdown
# THETIS-PROVENANCE.md
| src/core/codec/P1CodecAnvelinaPro3.cpp | Project Files/Source/ChannelMaster/networkproto1.c | 668-674; 682 | port | thetis-no-samphire | extends P1CodecStandard with bank 17 extra OC; AnvelinaPro3-only |
| src/core/codec/P1CodecAnvelinaPro3.h | Project Files/Source/ChannelMaster/networkproto1.c | 668-674 | port | thetis-no-samphire | header mirrors .cpp |
```

- [ ] **Step 5: Build + run, verify PASS**

```bash
cmake --build build --target tst_p1_codec_anvelina_pro3
./build/tests/tst_p1_codec_anvelina_pro3
```

Expected: 3/3 PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/codec/P1CodecAnvelinaPro3.h src/core/codec/P1CodecAnvelinaPro3.cpp \
        src/CMakeLists.txt tests/tst_p1_codec_anvelina_pro3.cpp tests/CMakeLists.txt \
        docs/attribution/THETIS-PROVENANCE.md
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
feat(codec): P1CodecAnvelinaPro3 — extends Standard with bank 17 (Phase 3P-A Task 7)

ANAN-8000DLE adds a 17th C&C bank carrying extra OC pin outputs.
Subclass of P1CodecStandard; only overrides composeCcForBank to
intercept bank 17, delegates 0-16 to parent.

Ports networkproto1.c:668-674 + 682 (end_frame logic) [ramdor 501e3f5].

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: `P1CodecRedPitaya` — extends Standard with bank 12 ADC1 carve-out

**Files:**
- Create: `src/core/codec/P1CodecRedPitaya.h`
- Create: `src/core/codec/P1CodecRedPitaya.cpp`
- Create: `tests/tst_p1_codec_redpitaya.cpp`
- Modify: `tests/CMakeLists.txt`, `src/CMakeLists.txt`, `docs/attribution/THETIS-PROVENANCE.md`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_p1_codec_redpitaya.cpp`:

```cpp
#include <QtTest/QtTest>
#include "core/codec/P1CodecRedPitaya.h"

using namespace NereusSDR;

class TestP1CodecRedPitaya : public QObject {
    Q_OBJECT
private slots:
    void maxBank_is_16() {
        P1CodecRedPitaya codec;
        QCOMPARE(codec.maxBank(), 16);
    }

    // Bank 12 — RedPitaya does NOT force 0x1F during MOX. It uses the
    // user-set rxStepAttn[1] masked to 5 bits.
    // Source: networkproto1.c:611-613 [ramdor 501e3f5] (DH1KLM contribution)
    void bank12_mox_uses_user_attn_not_forced_max() {
        P1CodecRedPitaya codec;
        CodecContext ctx;
        ctx.mox = true;
        ctx.rxStepAttn[1] = 12;  // user-set ADC1 ATT
        quint8 out[5] = {};
        codec.composeCcForBank(12, ctx, out);
        QCOMPARE(int(out[0]), 0x17);  // C0=0x16 | mox
        // RedPitaya: respects user attn masked to 5 bits + 0x20 enable
        QCOMPARE(int(out[1]), (12 & 0x1F) | 0x20);
        QCOMPARE(int(out[2]), 0x20);  // ADC2 = 0 + enable
    }

    // Bank 12 RX path identical to Standard
    void bank12_rx_path_matches_standard() {
        P1CodecRedPitaya codec;
        CodecContext ctx;
        ctx.rxStepAttn[1] = 7;
        quint8 out[5] = {};
        codec.composeCcForBank(12, ctx, out);
        QCOMPARE(int(out[1] & 0x1F), 7);
    }
};

QTEST_APPLESS_MAIN(TestP1CodecRedPitaya)
#include "tst_p1_codec_redpitaya.moc"
```

Add to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_p1_codec_redpitaya)
```

- [ ] **Step 2: Run, verify FAIL**

```bash
cmake --build build --target tst_p1_codec_redpitaya 2>&1 | tail -3
```

Expected: BUILD FAIL.

- [ ] **Step 3: Create header + impl**

`src/core/codec/P1CodecRedPitaya.h`:

```cpp
// =================================================================
// src/core/codec/P1CodecRedPitaya.h  (NereusSDR)
// =================================================================
// (Same header template; cite networkproto1.c verbatim.)
// =================================================================

#pragma once

#include "P1CodecStandard.h"

namespace NereusSDR {

// RedPitaya (DH1KLM port) — extends Standard with bank 12 ADC1 carve-out.
// During MOX, RedPitaya does NOT force ADC1 to 0x1F like other boards;
// it respects the user-set rxStepAttn[1] masked to 5 bits.
//
// Source: networkproto1.c:611-613 [ramdor 501e3f5] (DH1KLM contribution)
class P1CodecRedPitaya : public P1CodecStandard {
protected:
    void bank12(const CodecContext& ctx, quint8 out[5]) const override;
};

} // namespace NereusSDR
```

`src/core/codec/P1CodecRedPitaya.cpp`:

```cpp
// (Header block.)

#include "P1CodecRedPitaya.h"

namespace NereusSDR {

// Bank 12 ADC1 — RedPitaya carve-out: respect user attn even under MOX,
// mask to 5 bits.
// Source: networkproto1.c:611-613 [ramdor 501e3f5]
//
// "unsure why this was forced, but left as-is for all radios other than
//  Red Pitaya" [original inline comment from networkproto1.c:607-608]
void P1CodecRedPitaya::bank12(const CodecContext& ctx, quint8 out[5]) const
{
    out[0] = (ctx.mox ? 0x01 : 0x00) | 0x16;
    // RedPitaya: ADC1 always uses user attn, masked to 5 bits.
    out[1] = quint8((ctx.rxStepAttn[1] & 0x1F) | 0x20);
    out[2] = quint8((ctx.rxStepAttn[2] & 0x1F) | 0x20);
    out[3] = 0;
    out[4] = 0;
}

} // namespace NereusSDR
```

- [ ] **Step 4: Add to CMake + PROVENANCE**

Same pattern as Task 7.

- [ ] **Step 5: Build + run, verify PASS**

```bash
cmake --build build --target tst_p1_codec_redpitaya
./build/tests/tst_p1_codec_redpitaya
```

Expected: 3/3 PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/codec/P1CodecRedPitaya.h src/core/codec/P1CodecRedPitaya.cpp \
        src/CMakeLists.txt tests/tst_p1_codec_redpitaya.cpp tests/CMakeLists.txt \
        docs/attribution/THETIS-PROVENANCE.md
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
feat(codec): P1CodecRedPitaya — bank 12 ADC1 MOX carve-out (Phase 3P-A Task 8)

RedPitaya (DH1KLM port) needs different bank 12 ADC1 encoding under
MOX — respects user-set attn instead of forcing 0x1F like other boards.
Subclass of P1CodecStandard, overrides only the bank12() helper.

Ports networkproto1.c:611-613 [ramdor 501e3f5]. Inline comment from
upstream preserved.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: `P1CodecHl2` — TDD tests for the bug-fix path

**Files:**
- Create: `tests/tst_p1_codec_hl2.cpp`
- Modify: `tests/CMakeLists.txt`

The Hl2 codec is the **load-bearing one** for the bug fix. Tests cover the spec §6.3.2 (6-bit ATT + 0x40 enable + MOX TX/RX branch), §6.3.3 (HL2 drops the RedPitaya gate), §6.3.4 (HL2 bank 17 is TX latency, NOT AP3 extra OC), §6.3.5 (HL2 bank 18 reset).

- [ ] **Step 1: Write the failing test**

```cpp
#include <QtTest/QtTest>
#include "core/codec/P1CodecHl2.h"

using namespace NereusSDR;

class TestP1CodecHl2 : public QObject {
    Q_OBJECT
private slots:
    void maxBank_is_18() {
        P1CodecHl2 codec;
        QCOMPARE(codec.maxBank(), 18);
    }

    void usesI2cIntercept_is_true() {
        P1CodecHl2 codec;
        QVERIFY(codec.usesI2cIntercept());
    }

    // Bank 11 C4 — RX path: 6-bit mask + 0x40 enable
    // Source: mi0bot networkproto1.c:1102 [mi0bot c26a8a4]
    void bank11_rx_att_20dB_hl2_encoding() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.mox = false;
        ctx.rxStepAttn[0] = 20;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[0]), 0x14);
        QCOMPARE(int(out[4]), (20 & 0x3F) | 0x40);  // 0x54
    }

    // Bank 11 C4 — TX path: uses txStepAttn[0] not rxStepAttn[0]
    // Source: mi0bot networkproto1.c:1099 [mi0bot c26a8a4]
    void bank11_tx_att_uses_tx_value() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.mox = true;
        ctx.rxStepAttn[0] = 0;     // RX-side value ignored
        ctx.txStepAttn[0] = 20;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[0]), 0x15);  // C0=0x14 | mox
        QCOMPARE(int(out[4]), (20 & 0x3F) | 0x40);  // 0x54 from TX side
    }

    // Bank 11 C4 — full 6-bit range (HL2 supports 0-63)
    void bank11_rx_att_63dB_full_hl2_range() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.rxStepAttn[0] = 63;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[4]), 0x3F | 0x40);  // 0x7F
    }

    // Bank 12 — HL2 drops RedPitaya gate (mi0bot:1107-1111)
    void bank12_mox_no_redpitaya_gate() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.mox = true;
        ctx.rxStepAttn[1] = 7;
        quint8 out[5] = {};
        codec.composeCcForBank(12, ctx, out);
        // mi0bot HL2 path uses unmasked rxStepAttn[1]
        QCOMPARE(int(out[1]) & 0xDF, 7);  // strip enable bit, compare value
    }

    // Bank 17 — HL2 TX latency / PTT hang (NOT AnvelinaPro3 extra OC)
    // Source: mi0bot networkproto1.c:1162-1168 [mi0bot c26a8a4]
    void bank17_hl2_tx_latency_and_ptt_hang() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.hl2PttHang = 0x0A;
        ctx.hl2TxLatency = 0x55;
        quint8 out[5] = {};
        codec.composeCcForBank(17, ctx, out);
        QCOMPARE(int(out[0]), 0x2E);
        QCOMPARE(int(out[1]), 0x00);
        QCOMPARE(int(out[2]), 0x00);
        QCOMPARE(int(out[3]), 0x0A & 0x1F);
        QCOMPARE(int(out[4]), 0x55 & 0x7F);
    }

    // Bank 18 — HL2 reset on disconnect
    // Source: mi0bot networkproto1.c:1170-1176 [mi0bot c26a8a4]
    void bank18_hl2_reset_on_disconnect_set() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.hl2ResetOnDisconnect = true;
        quint8 out[5] = {};
        codec.composeCcForBank(18, ctx, out);
        QCOMPARE(int(out[0]), 0x74);
        QCOMPARE(int(out[4]), 0x01);
    }

    void bank18_hl2_reset_on_disconnect_clear() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.hl2ResetOnDisconnect = false;
        quint8 out[5] = {};
        codec.composeCcForBank(18, ctx, out);
        QCOMPARE(int(out[4]), 0x00);
    }

    // Banks 0-9 + 13-16 unchanged from Standard
    void bank10_alex_filter_passthrough() {
        P1CodecHl2 codec;
        CodecContext ctx;
        ctx.alexHpfBits = 0x01;
        ctx.alexLpfBits = 0x01;
        quint8 out[5] = {};
        codec.composeCcForBank(10, ctx, out);
        QCOMPARE(int(out[0]), 0x12);
        QCOMPARE(int(out[3]) & 0x7F, 0x01);
        QCOMPARE(int(out[4]), 0x01);
    }
};

QTEST_APPLESS_MAIN(TestP1CodecHl2)
#include "tst_p1_codec_hl2.moc"
```

Add to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_p1_codec_hl2)
```

- [ ] **Step 2: Run, verify build FAIL**

```bash
cmake --build build --target tst_p1_codec_hl2 2>&1 | tail -3
```

Expected: BUILD FAIL.

---

## Task 10: `P1CodecHl2` — implementation (the bug fix)

**Files:**
- Create: `src/core/codec/P1CodecHl2.h`
- Create: `src/core/codec/P1CodecHl2.cpp`
- Modify: `src/CMakeLists.txt`, `docs/attribution/THETIS-PROVENANCE.md`

This is **the file that fixes the bug.** Variant `mi0bot` — verbatim mi0bot header required.

- [ ] **Step 1: Capture mi0bot upstream header**

```bash
head -45 "/Users/j.j.boyd/mi0bot-Thetis/Project Files/Source/ChannelMaster/networkproto1.c"
```

Use lines 1-45 verbatim.

- [ ] **Step 2: Create `P1CodecHl2.h`**

```cpp
// =================================================================
// src/core/codec/P1CodecHl2.h  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c:869-1201
//   (WriteMainLoop_HL2)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. HL2-only codec; mirrors mi0bot's
//                literal WriteMainLoop_HL2 vs WriteMainLoop split.
// =================================================================
//
// === Verbatim mi0bot networkproto1.c header ===
// [Insert mi0bot networkproto1.c:1-45 verbatim from
//  /Users/j.j.boyd/mi0bot-Thetis/Project Files/Source/ChannelMaster/networkproto1.c]
// =================================================================

#pragma once

#include "IP1Codec.h"

namespace NereusSDR {

// mi0bot WriteMainLoop_HL2 port. Banks 0-18.
//
// Key divergences from Standard:
//   - Bank 11 C4: 6-bit ATT mask (0x3F) + 0x40 enable (vs 0x1F + 0x20).
//                 Under MOX, sends txStepAttn[0] instead of rxStepAttn[0].
//   - Bank 12 C1: drops the RedPitaya gate (HL2 is known to be HL2,
//                 not RedPitaya, so the gate is unnecessary).
//   - Bank 17:    TX latency + PTT hang (vs AnvelinaPro3 extra OC).
//   - Bank 18:    Reset-on-disconnect flag (HL2-only firmware feature).
//   - usesI2cIntercept() returns true — Phase E will use this hook to
//     swap in I2C TLV bytes when the IoBoardHl2 queue is non-empty.
class P1CodecHl2 : public IP1Codec {
public:
    void composeCcForBank(int bank, const CodecContext& ctx, quint8 out[5]) const override;
    int  maxBank() const override { return 18; }
    bool usesI2cIntercept() const override { return true; }
};

} // namespace NereusSDR
```

- [ ] **Step 3: Create `P1CodecHl2.cpp`**

```cpp
// (Header block — same template, mi0bot variant.)

#include "P1CodecHl2.h"

namespace NereusSDR {

void P1CodecHl2::composeCcForBank(int bank, const CodecContext& ctx,
                                  quint8 out[5]) const
{
    const quint8 C0base = ctx.mox ? 0x01 : 0x00;
    for (int i = 1; i < 5; ++i) { out[i] = 0; }

    switch (bank) {
        // Bank 0 — General settings (identical to Standard / ramdor)
        // Source: mi0bot networkproto1.c:614-639 [mi0bot c26a8a4 / matches ramdor]
        case 0:
            out[0] = C0base | 0x00;
            out[1] = quint8(ctx.sampleRateCode & 0x03);
            out[2] = quint8((ctx.ocByte << 1) & 0xFE);
            out[3] = quint8((ctx.rxPreamp[0] ? 0x04 : 0) | 0x08 | 0x10);
            out[4] = quint8((ctx.antennaIdx & 0x03)
                          | (ctx.duplex ? 0x04 : 0)
                          | (((ctx.activeRxCount - 1) & 0x0F) << 3)
                          | (ctx.diversity ? 0x80 : 0));
            return;

        // Bank 1 — TX VFO
        // Source: mi0bot networkproto1.c:646-650 [mi0bot c26a8a4]
        case 1:
            out[0] = C0base | 0x02;
            out[1] = quint8((ctx.txFreqHz >> 24) & 0xFF);
            out[2] = quint8((ctx.txFreqHz >> 16) & 0xFF);
            out[3] = quint8((ctx.txFreqHz >>  8) & 0xFF);
            out[4] = quint8( ctx.txFreqHz        & 0xFF);
            return;

        // Banks 2-9 — RX VFOs (identical to Standard)
        case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: {
            out[0] = C0base | quint8(0x04 + (bank - 2) * 2);
            const int rxIdx = bank - 2;
            const quint64 freq = (rxIdx < ctx.activeRxCount)
                                  ? ctx.rxFreqHz[rxIdx]
                                  : ctx.txFreqHz;
            out[1] = quint8((freq >> 24) & 0xFF);
            out[2] = quint8((freq >> 16) & 0xFF);
            out[3] = quint8((freq >>  8) & 0xFF);
            out[4] = quint8( freq        & 0xFF);
            return;
        }

        // Bank 10 — TX drive, mic, Alex HPF/LPF, PA (identical to Standard)
        // Source: mi0bot networkproto1.c:748-759 [mi0bot c26a8a4]
        case 10:
            out[0] = C0base | 0x12;
            out[1] = quint8(ctx.txDrive & 0xFF);
            out[2] = 0x40;
            out[3] = quint8(ctx.alexHpfBits | (ctx.paEnabled ? 0x80 : 0));
            out[4] = quint8(ctx.alexLpfBits);
            return;

        // Bank 11 — Preamp + RX/TX step ATT ADC0 (HL2 6-bit + MOX branch)
        // Source: mi0bot networkproto1.c:763-769, 1099-1102 [mi0bot c26a8a4]
        case 11: {
            out[0] = C0base | 0x14;
            out[1] = quint8((ctx.rxPreamp[0] ? 0x01 : 0)
                          | (ctx.rxPreamp[1] ? 0x02 : 0)
                          | (ctx.rxPreamp[2] ? 0x04 : 0)
                          | (ctx.rxPreamp[0] ? 0x08 : 0));
            out[2] = 0;
            out[3] = 0;
            // HL2 ATT: 6-bit mask + 0x40 enable, MOX branch on TX vs RX
            const int val = ctx.mox ? ctx.txStepAttn[0] : ctx.rxStepAttn[0];
            out[4] = quint8((val & 0x3F) | 0x40);
            return;
        }

        // Bank 12 — Step ATT ADC1/2 (HL2 drops the RedPitaya gate)
        // Source: mi0bot networkproto1.c:1107-1111 [mi0bot c26a8a4]
        case 12:
            out[0] = C0base | 0x16;
            // HL2: ADC1 uses user attn unmasked under MOX too
            out[1] = quint8(ctx.rxStepAttn[1] & 0xFF) | 0x20;
            out[2] = quint8((ctx.rxStepAttn[2] & 0x1F) | 0x20);
            out[3] = 0;
            out[4] = 0;
            return;

        // Banks 13-16 — CW / EER / BPF2 (identical to Standard)
        case 13: out[0] = C0base | 0x1E; return;
        case 14: out[0] = C0base | 0x20; return;
        case 15: out[0] = C0base | 0x22; return;
        case 16: out[0] = C0base | 0x24; return;

        // Bank 17 — TX latency + PTT hang (HL2-only)
        // Source: mi0bot networkproto1.c:1162-1168 [mi0bot c26a8a4]
        case 17:
            out[0] = C0base | 0x2E;
            out[3] = quint8(ctx.hl2PttHang & 0x1F);
            out[4] = quint8(ctx.hl2TxLatency & 0x7F);
            return;

        // Bank 18 — Reset on disconnect (HL2-only)
        // Source: mi0bot networkproto1.c:1170-1176 [mi0bot c26a8a4]
        case 18:
            out[0] = C0base | 0x74;
            out[4] = quint8(ctx.hl2ResetOnDisconnect ? 0x01 : 0x00);
            return;

        default:
            out[0] = C0base;
            return;
    }
}

} // namespace NereusSDR
```

- [ ] **Step 4: Add to CMake + PROVENANCE**

```cmake
# src/CMakeLists.txt
core/codec/P1CodecHl2.cpp
```

```markdown
# THETIS-PROVENANCE.md (mi0bot variant)
| src/core/codec/P1CodecHl2.cpp | Project Files/Source/ChannelMaster/networkproto1.c | 869-1201 | port | mi0bot | mi0bot WriteMainLoop_HL2 port; HL2-only codec; bank 11 6-bit ATT + MOX branch fixes the reported S-ATT bug |
| src/core/codec/P1CodecHl2.h | Project Files/Source/ChannelMaster/networkproto1.c | 869-1201 | port | mi0bot | header mirrors .cpp |
```

- [ ] **Step 5: Run tests, verify PASS**

```bash
cmake --build build --target tst_p1_codec_hl2
./build/tests/tst_p1_codec_hl2
```

Expected: 10/10 PASS.

- [ ] **Step 6: Verify the headline bug-fix assertion**

Spot-check the canonical regression case:

```bash
./build/tests/tst_p1_codec_hl2 -v2 2>&1 | grep -E "(bank11_tx_att|bank11_rx_att_20)"
```

Expected: both `bank11_tx_att_uses_tx_value` and `bank11_rx_att_20dB_hl2_encoding` PASS with `out[4] = 0x54`.

- [ ] **Step 7: Commit**

```bash
git add src/core/codec/P1CodecHl2.h src/core/codec/P1CodecHl2.cpp \
        src/CMakeLists.txt tests/tst_p1_codec_hl2.cpp tests/CMakeLists.txt \
        docs/attribution/THETIS-PROVENANCE.md
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
feat(codec): P1CodecHl2 — mi0bot WriteMainLoop_HL2 port (Phase 3P-A Task 9+10)

Fixes reported HL2 S-ATT bug at the codec layer. Bank 11 C4 now uses
6-bit mask (0x3F) + 0x40 enable bit + MOX branch on TX vs RX
attenuator value, matching mi0bot's WriteMainLoop_HL2. Standard
codec keeps ramdor's 5-bit + 0x20 encoding for Hermes/Orion (no
behavior change for those boards).

Also covers HL2-specific bank 17 (TX latency + PTT hang) and bank 18
(reset on disconnect), and drops the RedPitaya gate from bank 12 ADC1
(HL2 is known not to be RedPitaya).

usesI2cIntercept() returns true — Phase E will use this hook to swap
I2C TLV bytes into the C&C payload when IoBoardHl2's queue is non-empty.

P1RadioConnection still uses its legacy composeCcForBank — Task 11
swaps it to delegate to this codec for HL2 boards. Bug isn't actually
fixed in userland until Task 11 lands.

Ports mi0bot-Thetis networkproto1.c:869-1201 [mi0bot c26a8a4]. Verbatim
mi0bot upstream header preserved. PROVENANCE row added.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 11: `BoardCapabilities::Attenuator` field expansion

**Files:**
- Modify: `src/core/BoardCapabilities.h`
- Modify: `src/core/BoardCapabilities.cpp`
- Modify: `tests/tst_board_capabilities.cpp` (add new field assertions)

- [ ] **Step 1: Write the failing test**

Edit `tests/tst_board_capabilities.cpp`. Find the existing attenuator-related tests and add:

```cpp
// Phase 3P-A: Attenuator now carries mask + enableBit + moxBranchesAtt
void hermes_attenuator_5bit_ramdor_encoding() {
    const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Hermes);
    QCOMPARE(int(caps.attenuator.mask),       0x1F);
    QCOMPARE(int(caps.attenuator.enableBit),  0x20);
    QVERIFY(!caps.attenuator.moxBranchesAtt);
    QCOMPARE(caps.attenuator.maxDb, 31);
}

void hl2_attenuator_6bit_mi0bot_encoding() {
    const auto& caps = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
    QCOMPARE(int(caps.attenuator.mask),       0x3F);
    QCOMPARE(int(caps.attenuator.enableBit),  0x40);
    QVERIFY(caps.attenuator.moxBranchesAtt);
    QCOMPARE(caps.attenuator.maxDb, 63);
}

void redpitaya_attenuator_5bit_no_mox_branch() {
    const auto& caps = BoardCapsTable::forBoard(HPSDRHW::RedPitaya);
    QCOMPARE(int(caps.attenuator.mask),       0x1F);
    QCOMPARE(int(caps.attenuator.enableBit),  0x20);
    QVERIFY(!caps.attenuator.moxBranchesAtt);
}
```

- [ ] **Step 2: Run, verify FAIL**

```bash
cmake --build build --target tst_board_capabilities 2>&1 | tail -3
```

Expected: BUILD FAIL — `'mask' is not a member of 'Attenuator'`.

- [ ] **Step 3: Add fields to `BoardCapabilities.h`**

Find the `Attenuator` struct (search for `struct Attenuator`). Add three fields:

```cpp
struct Attenuator {
    bool   present;
    int    minDb;
    int    maxDb;
    quint8 mask;          // 0x1F (5-bit) or 0x3F (6-bit HL2)
    quint8 enableBit;     // 0x20 (std) or 0x40 (HL2)
    bool   moxBranchesAtt; // true → send txStepAttn under MOX (HL2 only)
};
```

- [ ] **Step 4: Populate per-board in `BoardCapabilities.cpp`**

Find each board's `BoardCapabilities` table entry and add the three new fields. For all non-HL2 boards: `.mask = 0x1F, .enableBit = 0x20, .moxBranchesAtt = false`. For HL2: `.mask = 0x3F, .enableBit = 0x40, .moxBranchesAtt = true`.

Concrete edits per board (exact lines depend on current file state; grep `.attenuator = {` to find each entry):

```cpp
// HPSDR (Atlas): unchanged behavior
.attenuator = { .present = false, .minDb = 0, .maxDb = 0,
                .mask = 0x1F, .enableBit = 0x20, .moxBranchesAtt = false },

// Hermes / HermesII / Angelia / Orion / OrionMKII / AnvelinaPro3 /
// RedPitaya / AnanG2 / AnanG2_1K — all use ramdor encoding
.attenuator = { .present = true, .minDb = 0, .maxDb = 31,
                .mask = 0x1F, .enableBit = 0x20, .moxBranchesAtt = false },

// HermesLite (HL2) — mi0bot encoding
.attenuator = { .present = true, .minDb = 0, .maxDb = 63,
                .mask = 0x3F, .enableBit = 0x40, .moxBranchesAtt = true },
```

- [ ] **Step 5: Run, verify PASS**

```bash
cmake --build build --target tst_board_capabilities
./build/tests/tst_board_capabilities
```

Expected: all PASS including 3 new assertions.

- [ ] **Step 6: Commit**

```bash
git add src/core/BoardCapabilities.h src/core/BoardCapabilities.cpp \
        tests/tst_board_capabilities.cpp
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
feat(caps): expand Attenuator with mask + enableBit + moxBranchesAtt (Phase 3P-A Task 11)

Adds three fields to BoardCapabilities::Attenuator capturing the per-board
encoding parameters. HL2 gets 6-bit mask + 0x40 enable + MOX branch;
every other board keeps ramdor's 5-bit + 0x20 encoding. RxApplet (Task 14)
will read maxDb from here so the slider widens to 0-63 on HL2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 12: Wire `P1RadioConnection` to delegate to codec (with rollback flag)

**Files:**
- Modify: `src/core/P1RadioConnection.h`
- Modify: `src/core/P1RadioConnection.cpp`

This is **the cutover task.** P1RadioConnection stops doing its own bank assembly and delegates to `m_codec`. The new behavior is gated by an env-var feature flag — `NEREUS_USE_LEGACY_P1_CODEC=1` selects the pre-refactor compose path (kept inline as `composeCcForBankLegacy`).

- [ ] **Step 1: Add codec member + factory in `P1RadioConnection.h`**

Edit `P1RadioConnection.h`. Add at top:

```cpp
#include <memory>
#include "codec/IP1Codec.h"
#include "codec/CodecContext.h"
```

In the private section, replace `int m_ccRoundRobinIdx{0};` neighborhood with:

```cpp
int m_ccRoundRobinIdx{0};

// Phase 3P-A: per-board codec chosen at applyBoardQuirks() time.
// Null when m_caps is null (pre-connect) or env var
// NEREUS_USE_LEGACY_P1_CODEC=1 forces legacy compose path.
std::unique_ptr<IP1Codec> m_codec;
bool m_useLegacyCodec{false};
```

Add private helper:

```cpp
// Build m_codec from m_hardwareProfile.model. Called from applyBoardQuirks().
void selectCodec();

// Legacy compose path — preserved for the rollback feature flag for
// one release cycle. Identical to the pre-refactor composeCcForBank.
void composeCcForBankLegacy(int bankIdx, quint8 out[5]) const;

// Snapshot all live state into a CodecContext for the codec call.
CodecContext buildCodecContext() const;
```

- [ ] **Step 2: Implement `selectCodec()` in `P1RadioConnection.cpp`**

Add includes at top:

```cpp
#include <cstdlib>
#include "codec/P1CodecStandard.h"
#include "codec/P1CodecAnvelinaPro3.h"
#include "codec/P1CodecRedPitaya.h"
#include "codec/P1CodecHl2.h"
```

Add new method:

```cpp
void P1RadioConnection::selectCodec()
{
    m_codec.reset();
    m_useLegacyCodec = (qEnvironmentVariableIntValue("NEREUS_USE_LEGACY_P1_CODEC") == 1);
    if (m_useLegacyCodec) {
        qCInfo(lcConnection) << "P1: NEREUS_USE_LEGACY_P1_CODEC=1 — using pre-refactor compose path";
        return;
    }
    if (!m_caps) {
        qCWarning(lcConnection) << "P1: no caps; codec selection deferred";
        return;
    }
    using HW = HPSDRHW;
    switch (m_hardwareProfile.model) {
        case HW::HermesLite:  m_codec = std::make_unique<P1CodecHl2>();          break;
        case HW::AnvelinaPro3: m_codec = std::make_unique<P1CodecAnvelinaPro3>(); break;
        case HW::RedPitaya:   m_codec = std::make_unique<P1CodecRedPitaya>();    break;
        default:              m_codec = std::make_unique<P1CodecStandard>();     break;
    }
    qCInfo(lcConnection) << "P1: selected codec for board" << int(m_hardwareProfile.model);
}
```

- [ ] **Step 3: Call `selectCodec()` from `applyBoardQuirks()`**

Edit the existing `applyBoardQuirks()` function. At the end, add:

```cpp
selectCodec();
```

- [ ] **Step 4: Implement `buildCodecContext()`**

```cpp
CodecContext P1RadioConnection::buildCodecContext() const
{
    CodecContext ctx;
    ctx.mox            = m_mox;
    ctx.sampleRateCode = (m_sampleRate == 384000) ? 3
                       : (m_sampleRate == 192000) ? 2
                       : (m_sampleRate ==  96000) ? 1 : 0;
    ctx.activeRxCount  = m_activeRxCount;
    ctx.txDrive        = m_txDrive;
    ctx.paEnabled      = m_paEnabled;
    ctx.duplex         = m_duplex;
    ctx.diversity      = m_diversity;
    ctx.antennaIdx     = m_antennaIdx;
    ctx.ocByte         = m_ocOutput;
    ctx.adcCtrl        = m_adcCtrl;
    ctx.alexHpfBits    = m_alexHpfBits;
    ctx.alexLpfBits    = m_alexLpfBits;
    ctx.txFreqHz       = m_txFreqHz;
    for (int i = 0; i < 7; ++i) { ctx.rxFreqHz[i]  = m_rxFreqHz[i]; }
    for (int i = 0; i < 3; ++i) { ctx.rxStepAttn[i] = m_stepAttn[i]; }
    for (int i = 0; i < 3; ++i) { ctx.txStepAttn[i] = m_txStepAttn; }  // single TX value broadcast
    for (int i = 0; i < 3; ++i) { ctx.rxPreamp[i] = m_rxPreamp[i]; }
    // HL2-only fields default to 0 / false; populated by Phase E.
    return ctx;
}
```

- [ ] **Step 5: Rewrite `composeCcForBank()` to delegate**

Find the existing `composeCcForBank(int bankIdx, quint8 out[5])` method. Rename the body to `composeCcForBankLegacy(int, quint8[5])` (move it whole). Then write the new wrapper:

```cpp
void P1RadioConnection::composeCcForBank(int bankIdx, quint8 out[5]) const
{
    if (m_useLegacyCodec || !m_codec) {
        composeCcForBankLegacy(bankIdx, out);
        return;
    }
    composeCcForBankLegacy(bankIdx, out);  // Temporarily call both — see Step 6
    // The codec output overwrites; legacy is called only for the in-line
    // diagnostic compare below. Remove this dual path in Task 16.
    quint8 codecOut[5] = {};
    const CodecContext ctx = buildCodecContext();
    m_codec->composeCcForBank(bankIdx, ctx, codecOut);
    for (int i = 0; i < 5; ++i) {
        if (codecOut[i] != out[i]) {
            qCWarning(lcConnection)
              << "P1 codec divergence on bank" << bankIdx << "byte" << i
              << "legacy=" << QString::number(out[i], 16)
              << "codec="  << QString::number(codecOut[i], 16);
            // Fall through to use codec output anyway — the codec is
            // authoritative; the legacy path is only kept for compare.
        }
        out[i] = codecOut[i];
    }
}
```

- [ ] **Step 6: Build, run full test suite — expect existing tests still green**

```bash
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure 2>&1 | tail -30
```

Expected: existing tests (`tst_p1_wire_format`, `tst_p1_loopback_connection`, etc.) all pass. New codec tests pass. The `qCWarning` from Step 5 must NOT fire during any test — that would mean the codec diverges from legacy.

- [ ] **Step 7: Commit**

```bash
git add src/core/P1RadioConnection.h src/core/P1RadioConnection.cpp
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
feat(p1): delegate composeCcForBank to per-board codec (Phase 3P-A Task 12)

P1RadioConnection chooses an IP1Codec at applyBoardQuirks() time based
on m_hardwareProfile.model — Standard for Hermes/Orion family, Hl2 for
HermesLite, AnvelinaPro3 + RedPitaya for those two specifically.
composeCcForBank now snapshots state into a CodecContext and delegates
to m_codec.

Cutover safety: a temporary dual-call path runs both legacy and codec
output, logs any divergence as qCWarning(lcConnection), and uses the
codec output. No warnings should fire on any test or live radio.
The dual-call gets removed in Task 16 once regression-freeze test is
green for one CI cycle.

Rollback hatch: NEREUS_USE_LEGACY_P1_CODEC=1 env var forces the
pre-refactor path; logged at INFO on every connect when set.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 13: Wire `setReceiverFrequency` / `setTxFrequency` to `AlexFilterMap`

**Files:**
- Modify: `src/core/P1RadioConnection.cpp`

This is **the second half of the BPF bug fix.** `m_alexHpfBits` / `m_alexLpfBits` were initialized to zero and never updated — now they recompute on every freq change.

- [ ] **Step 1: Add include**

Add to top of `P1RadioConnection.cpp`:

```cpp
#include "codec/AlexFilterMap.h"
```

- [ ] **Step 2: Update `setReceiverFrequency`**

Find the existing definition (around line 749). Replace:

```cpp
void P1RadioConnection::setReceiverFrequency(int receiverIndex, quint64 frequencyHz)
{
    if (receiverIndex < 0 || receiverIndex >= 7) { return; }
    m_rxFreqHz[receiverIndex] = frequencyHz;
    // RX0 drives the Alex HPF bank.
    if (receiverIndex == 0 && m_caps && m_caps->hasAlexFilters) {
        m_alexHpfBits = codec::alex::computeHpf(double(frequencyHz) / 1e6);
    }
}
```

- [ ] **Step 3: Update `setTxFrequency`**

```cpp
void P1RadioConnection::setTxFrequency(quint64 frequencyHz)
{
    m_txFreqHz = frequencyHz;
    if (m_caps && m_caps->hasAlexFilters) {
        m_alexLpfBits = codec::alex::computeLpf(double(frequencyHz) / 1e6);
    }
}
```

- [ ] **Step 4: Add a focused unit test verifying the wiring**

Append to `tests/tst_p1_wire_format.cpp` (existing test fixture):

```cpp
// Phase 3P-A: setReceiverFrequency must update the Alex HPF bits.
void setReceiverFrequency_updates_alex_hpf_bits() {
    P1RadioConnection conn(nullptr);
    conn.setBoardForTest(HPSDRHW::Hermes);  // hasAlexFilters = true
    conn.setReceiverFrequency(0, 14'100'000);  // 20m → 13 MHz HPF (0x01)
    quint8 out[5] = {};
    conn.composeCcForBank(10, out);
    // Bank 10 C3 carries Alex HPF bits.
    QCOMPARE(int(out[3] & 0x7F), 0x01);
}

void setTxFrequency_updates_alex_lpf_bits() {
    P1RadioConnection conn(nullptr);
    conn.setBoardForTest(HPSDRHW::Hermes);
    conn.setTxFrequency(14'100'000);  // 30/20m LPF (0x01)
    quint8 out[5] = {};
    conn.composeCcForBank(10, out);
    QCOMPARE(int(out[4]), 0x01);
}
```

- [ ] **Step 5: Build, run, verify**

```bash
cmake --build build --target tst_p1_wire_format
./build/tests/tst_p1_wire_format
```

Expected: existing assertions + 2 new ones PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/P1RadioConnection.cpp tests/tst_p1_wire_format.cpp
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
fix(p1): recompute Alex HPF/LPF bits on freq change (Phase 3P-A Task 13)

Closes the reported HL2 BPF-doesn't-switch bug at the wire layer.
P1RadioConnection::setReceiverFrequency now calls AlexFilterMap::computeHpf
on every RX0 freq update; setTxFrequency calls computeLpf. Gated on
BoardCapabilities::hasAlexFilters so HL2 without N2ADR shield (where
hasAlexFilters=false) silently sends zeros — Phase E adds the N2ADR path.

Two new tst_p1_wire_format assertions lock the wiring: 14.1 MHz tune
→ 13 MHz HPF (0x01) on bank 10 C3, and matching LPF on C4.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 14: Switch `P2RadioConnection` to shared `AlexFilterMap`

**Files:**
- Modify: `src/core/P2RadioConnection.cpp`

P2 already has the compute logic correct (lines 916-968). This task deletes those local helpers and calls `AlexFilterMap` instead. P2 byte output stays byte-identical.

- [ ] **Step 1: Add include + delete local helpers**

Edit `src/core/P2RadioConnection.cpp`:

```cpp
#include "codec/AlexFilterMap.h"
```

Delete the local function definitions for `computeAlexHpf` and `computeAlexLpf` (current lines ~916-968). Keep the function declarations in the header for now — the next step routes existing callers to the shared helpers.

- [ ] **Step 2: Replace call sites**

Find every `computeAlexHpf(` / `computeAlexLpf(` call (likely in `setReceiverFrequency` around line 360-368). Replace with `codec::alex::computeHpf(` / `codec::alex::computeLpf(`.

If the header still declares the local functions, delete those declarations too.

- [ ] **Step 3: Run existing P2 tests, verify byte-identical**

```bash
cmake --build build --target tst_p1_loopback_connection
ctest --test-dir build --output-on-failure -R p2 2>&1 | tail -20
```

Expected: all P2-related tests pass. (If a P2 byte-equivalence test doesn't exist, this is the moment — but the existing `tst_p1_loopback_connection` exercises P2 send paths indirectly.)

- [ ] **Step 4: Commit**

```bash
git add src/core/P2RadioConnection.cpp src/core/P2RadioConnection.h
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
refactor(p2): use shared AlexFilterMap (Phase 3P-A Task 14)

Deletes P2RadioConnection's local computeAlexHpf/Lpf (which already
correctly ported the Thetis console.cs logic) and routes through
src/core/codec/AlexFilterMap. Byte output identical pre/post —
verified by existing P2 tests.

Now both P1 and P2 share one canonical freq→bits source.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 15: `RxApplet` S-ATT slider range from `BoardCapabilities`

**Files:**
- Modify: `src/gui/applets/RxApplet.cpp`

- [ ] **Step 1: Find the slider range setter**

Grep for the existing call:

```bash
grep -nE "stepAttSpin->setMaximum|stepAttSpin->setRange|31\)" src/gui/applets/RxApplet.cpp | head -5
```

- [ ] **Step 2: Wire slider max from BoardCapabilities**

Find the `setRadioModel` (or equivalent connect-time handler) where `m_stepAttSpin` is configured. Replace the hardcoded `31` with:

```cpp
const int maxDb = m_radioModel->boardCapabilities().attenuator.maxDb;
m_stepAttSpin->setMaximum(maxDb > 0 ? maxDb : 31);
```

(Adjust `m_radioModel->boardCapabilities()` to whatever method already exposes the caps — grep `boardCapabilities()` to confirm; if not exposed, add a thin getter.)

- [ ] **Step 3: Add a focused test**

Create `tests/tst_rxapplet_att_range.cpp`:

```cpp
#include <QtTest/QtTest>
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestRxAppletAttRange : public QObject {
    Q_OBJECT
private slots:
    void hl2_slider_max_is_63() {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        RxApplet applet(&model);
        QCOMPARE(applet.stepAttMaxForTest(), 63);
    }

    void hermes_slider_max_is_31() {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);
        RxApplet applet(&model);
        QCOMPARE(applet.stepAttMaxForTest(), 31);
    }
};

QTEST_MAIN(TestRxAppletAttRange)
#include "tst_rxapplet_att_range.moc"
```

Add a `stepAttMaxForTest()` accessor under `#ifdef NEREUS_BUILD_TESTS` in `RxApplet.h` returning `m_stepAttSpin->maximum()`. Likewise add `setBoardForTest` to `RadioModel` if not already present (mirrors the `P1RadioConnection::setBoardForTest` pattern).

Add to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_rxapplet_att_range)
target_compile_definitions(tst_rxapplet_att_range PRIVATE NEREUS_BUILD_TESTS)
```

- [ ] **Step 4: Build + run**

```bash
cmake --build build --target tst_rxapplet_att_range
./build/tests/tst_rxapplet_att_range
```

Expected: 2/2 PASS.

- [ ] **Step 5: Commit**

```bash
git add src/gui/applets/RxApplet.h src/gui/applets/RxApplet.cpp \
        src/models/RadioModel.h tests/tst_rxapplet_att_range.cpp tests/CMakeLists.txt
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
feat(rx-applet): widen S-ATT slider range from BoardCapabilities (Phase 3P-A Task 15)

S-ATT spinbox max now reads from BoardCapabilities::attenuator.maxDb
instead of being hardcoded to 31. HL2 → 0-63 dB; Hermes/Orion → 0-31
unchanged. Wires the userland half of the bug fix: slider can now
actually request the full HL2 range.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 16: Regression-freeze gate test + remove dual-call diagnostic

**Files:**
- Create: `tests/tst_p1_regression_freeze.cpp`
- Delete: `tests/tst_p1_regression_freeze_capture.cpp`
- Modify: `src/core/P1RadioConnection.cpp` (remove dual-call from Task 12 Step 5)
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the regression test**

Create `tests/tst_p1_regression_freeze.cpp`:

```cpp
#include <QtTest/QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/P1RadioConnection.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestP1RegressionFreeze : public QObject {
    Q_OBJECT
private slots:
    void allBoardsExceptHl2_byteIdenticalToBaseline() {
        const QString path = QStringLiteral(NEREUS_TEST_DATA_DIR) + "/p1_baseline_bytes.json";
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonArray rows = QJsonDocument::fromJson(f.readAll()).array();
        QVERIFY(rows.size() > 0);

        int compared = 0;
        for (const QJsonValue& v : rows) {
            QJsonObject row = v.toObject();
            const auto board = HPSDRHW(row["board"].toInt());
            const bool mox   = row["mox"].toBool();
            const int  bank  = row["bank"].toInt();

            // HL2 deliberately diverges — that's the bug fix. Skip its rows.
            if (board == HPSDRHW::HermesLite) continue;

            P1RadioConnection conn(nullptr);
            conn.setBoardForTest(board);
            conn.setMox(mox);
            quint8 actual[5] = {};
            conn.composeCcForBank(bank, actual);

            QJsonArray expected = row["bytes"].toArray();
            for (int i = 0; i < 5; ++i) {
                if (int(actual[i]) != expected[i].toInt()) {
                    QFAIL(qPrintable(QString("regression on board=%1 mox=%2 bank=%3 byte=%4 expected=0x%5 actual=0x%6")
                        .arg(int(board)).arg(mox).arg(bank).arg(i)
                        .arg(expected[i].toInt(), 2, 16, QChar('0'))
                        .arg(int(actual[i]), 2, 16, QChar('0'))));
                }
            }
            ++compared;
        }
        qInfo() << "regression-freeze passed:" << compared << "tuples";
    }
};

QTEST_APPLESS_MAIN(TestP1RegressionFreeze)
#include "tst_p1_regression_freeze.moc"
```

- [ ] **Step 2: Wire CMake**

```cmake
nereus_add_test(tst_p1_regression_freeze)
target_compile_definitions(tst_p1_regression_freeze PRIVATE
    NEREUS_BUILD_TESTS
    NEREUS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")
```

- [ ] **Step 3: Run, verify PASS**

```bash
cmake --build build --target tst_p1_regression_freeze
./build/tests/tst_p1_regression_freeze
```

Expected: PASS, ~360 tuples compared (10 non-HL2 boards × 2 MOX × 18 banks).

- [ ] **Step 4: Remove the temporary capture helper**

```bash
git rm tests/tst_p1_regression_freeze_capture.cpp
```

Edit `tests/CMakeLists.txt` and delete the `nereus_add_test(tst_p1_regression_freeze_capture)` block.

- [ ] **Step 5: Remove the dual-call diagnostic from `composeCcForBank`**

Edit `src/core/P1RadioConnection.cpp`. Replace the Task-12 Step-5 dual-call body with the clean delegate:

```cpp
void P1RadioConnection::composeCcForBank(int bankIdx, quint8 out[5]) const
{
    if (m_useLegacyCodec || !m_codec) {
        composeCcForBankLegacy(bankIdx, out);
        return;
    }
    const CodecContext ctx = buildCodecContext();
    m_codec->composeCcForBank(bankIdx, ctx, out);
}
```

- [ ] **Step 6: Run full suite, verify all green**

```bash
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure 2>&1 | tail -30
```

Expected: every test PASS, including the regression-freeze gate.

- [ ] **Step 7: Commit**

```bash
git add tests/tst_p1_regression_freeze.cpp tests/CMakeLists.txt \
        src/core/P1RadioConnection.cpp
git rm tests/tst_p1_regression_freeze_capture.cpp
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
test(p1): add regression-freeze gate; drop dual-call diagnostic (Phase 3P-A Task 16)

Regression-freeze test loads tests/data/p1_baseline_bytes.json and
asserts the new codec output matches byte-for-byte for every non-HL2
board × MOX × bank tuple. HL2 rows are skipped intentionally — that's
the bug fix.

Drops the dual-call legacy-vs-codec compare path from
P1RadioConnection::composeCcForBank now that the regression-freeze
gate proves they agree. Legacy path still reachable via
NEREUS_USE_LEGACY_P1_CODEC=1 env var until Phase B merges.

Removes the temporary tst_p1_regression_freeze_capture.cpp helper —
the JSON it produced is the contract.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 17: PROVENANCE final pass + CHANGELOG + PR template

**Files:**
- Modify: `docs/attribution/THETIS-PROVENANCE.md` (final cross-check)
- Modify: `CHANGELOG.md`
- Create: `.github/PULL_REQUEST_TEMPLATE.md`

- [ ] **Step 1: Verify all 5 ported files have PROVENANCE rows**

```bash
for f in AlexFilterMap.cpp AlexFilterMap.h \
         P1CodecStandard.cpp P1CodecStandard.h \
         P1CodecAnvelinaPro3.cpp P1CodecAnvelinaPro3.h \
         P1CodecRedPitaya.cpp P1CodecRedPitaya.h \
         P1CodecHl2.cpp P1CodecHl2.h; do
    grep -c "src/core/codec/$f" docs/attribution/THETIS-PROVENANCE.md \
      || echo "MISSING: $f"
done
```

Expected: every file returns ≥1.

- [ ] **Step 2: Add CHANGELOG entry**

Edit `CHANGELOG.md`. Under the unreleased section (or create one):

```markdown
## [Unreleased]

### Fixed
- **Hermes Lite 2 bandpass filter now switches on band/VFO change.** P1RadioConnection was emitting `m_alexHpfBits=0`/`m_alexLpfBits=0` permanently because the filter bits were never recomputed from frequency. P2 had the right code; lifted into shared `src/core/codec/AlexFilterMap` and called from P1's `setReceiverFrequency`/`setTxFrequency`. (Phase 3P-A)
- **Hermes Lite 2 step attenuator now actually attenuates.** P1's bank 11 C4 was using ramdor's 5-bit mask + 0x20 enable for every board; HL2 needs mi0bot's 6-bit mask + 0x40 enable + MOX TX/RX branch. Fixed via per-board codec subclasses (`P1CodecStandard` for Hermes/Orion, `P1CodecHl2` for HL2). RxApplet S-ATT slider range now widens to 0-63 dB on HL2 from `BoardCapabilities::attenuator.maxDb`. (Phase 3P-A)

### Changed
- `P1RadioConnection`'s C&C compose layer refactored into per-board codec subclasses (`P1CodecStandard`, `P1CodecHl2`, `P1CodecAnvelinaPro3`, `P1CodecRedPitaya`) behind a stable `IP1Codec` interface. Behavior byte-identical for every non-HL2 board (regression-frozen via `tst_p1_regression_freeze`). Set `NEREUS_USE_LEGACY_P1_CODEC=1` to revert to the pre-refactor compose path for one release cycle as a rollback hatch.
```

- [ ] **Step 3: Create PR template with attribution checklist**

Create `.github/PULL_REQUEST_TEMPLATE.md`:

```markdown
## Summary
<!-- 1-3 bullets explaining what this PR does and why -->

## Test plan
- [ ] <!-- Bulleted list of tests run / verifications performed -->

## Attribution checklist (required for any PR adding ported code)
- [ ] New `.h`/`.cpp` files have verbatim upstream header(s) (multi-source if applicable)
- [ ] `docs/attribution/THETIS-PROVENANCE.md` updated in this PR
- [ ] Every ported logic block has `[ramdor <sha>]` and/or `[mi0bot <sha>]` stamp
- [ ] `python3 scripts/verify-thetis-headers.py` passes
- [ ] `python3 scripts/check-new-ports.py` passes
- [ ] `python3 scripts/verify-inline-cites.py` passes

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

- [ ] **Step 4: Run all attribution gates one final time**

```bash
python3 scripts/verify-thetis-headers.py
python3 scripts/check-new-ports.py
python3 scripts/verify-inline-cites.py
python3 scripts/compliance-inventory.py --fail-on-unclassified
```

Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add docs/attribution/THETIS-PROVENANCE.md CHANGELOG.md .github/PULL_REQUEST_TEMPLATE.md
git -c commit.gpgsign=true commit -S -m "$(cat <<'EOF'
docs: CHANGELOG + PR template + final PROVENANCE pass (Phase 3P-A Task 17)

Adds the bug-fix CHANGELOG entries under unreleased. Adds an
attribution checklist to .github/PULL_REQUEST_TEMPLATE.md so every
subsequent radio-control PR carries the same gate (per spec §4).

Verified all 5 new ported files (AlexFilterMap, P1CodecStandard,
P1CodecAnvelinaPro3, P1CodecRedPitaya, P1CodecHl2) have PROVENANCE rows
and pass verify-thetis-headers / check-new-ports / verify-inline-cites
/ compliance-inventory.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 18: Build + manual smoke test on HL2 hardware

**Files:** none (manual verification)

This task validates the userland bug fix on real hardware. Requires the JJ HL2.

- [ ] **Step 1: Clean build for the binary that ships**

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(sysctl -n hw.ncpu)
nm build/NereusSDR 2>/dev/null | grep -E "P1CodecHl2|AlexFilterMap" | head -5
```

Expected: symbols present, build succeeds. (Per maintainer memory `feedback_verify_launched_binary` — confirm absolute path + symbols before eyeball test.)

- [ ] **Step 2: Launch and connect to HL2**

```bash
./build/NereusSDR
```

In the app: connect to Hermes Lite 2.

- [ ] **Step 3: Verify the BPF bug fix**

Tune VFO to **14.100 MHz**. Open the radio's web admin page or a packet capture and verify the OC byte sequence shows the 13 MHz HPF select. Expected on the wire (P1 EP2, bank 10): `C0=0x12 C3=0x01 C4=0x01` (HPF 13 MHz / LPF 30/20m).

Tune to **3.700 MHz**. Verify bytes change: `C3=0x10 C4=0x04` (HPF 1.5 MHz bypass / LPF 80m).

Tune to **52.000 MHz**. Verify: `C3=0x40 C4=0x10` (6m preamp / 6m LPF).

- [ ] **Step 4: Verify the S-ATT bug fix**

In RxApplet, set the S-ATT slider to **20 dB**. Verify the slider accepts the value. Verify the wire shows bank 11 `C0=0x14 C4=0x54` (`(20 & 0x3F) | 0x40`).

Set S-ATT to **45 dB**. Verify the slider accepts the value (range now 0-63). Verify the wire shows `C4 = (45 & 0x3F) | 0x40 = 0x6D`.

Audibly verify: receiving a known signal source, raise ATT and confirm the signal level drops on the S-meter accordingly.

- [ ] **Step 5: Regression-spot-check on a non-HL2 board (if available)**

If you have access to a Hermes/Orion/G2, connect and verify S-ATT 20 dB still produces `C4 = (20 & 0x1F) | 0x20 = 0x34` (ramdor encoding). And tune through bands to confirm BPF still switches as before.

- [ ] **Step 6: If smoke test passes, push branch + open PR**

```bash
git push -u origin phase3p-a-p1-wire-parity
gh pr create --title "Phase 3P-A: P1 wire-bytes parity foundation + HL2 bug fixes" \
             --body "$(cat <<'EOF'
## Summary
- Fixes HL2 BPF doesn't switch on band change (root: P1 never recomputed Alex HPF/LPF bits — P2 had the compute, P1 didn't; lifted into shared `AlexFilterMap`)
- Fixes HL2 S-ATT non-functional (root: P1 used ramdor 5-bit + 0x20 ATT encoding for every board; HL2 needs mi0bot 6-bit + 0x40 + MOX TX/RX branch; fixed via per-board codec subclasses)
- Refactors `P1RadioConnection`'s compose layer into `IP1Codec` + 4 subclasses (`Standard`, `Hl2`, `AnvelinaPro3`, `RedPitaya`); preserves byte-identical behavior for every non-HL2 board via JSON regression-freeze baseline

## Test plan
- [x] `tst_alex_filter_map` (18 assertions across HPF / LPF buckets + edges)
- [x] `tst_p1_codec_standard` (table-driven: bank 0, 10, 11, 12-RX, 12-MOX, 13-16)
- [x] `tst_p1_codec_anvelina_pro3` (bank 17 extra OC + bank 0 inheritance)
- [x] `tst_p1_codec_redpitaya` (bank 12 ADC1 MOX carve-out)
- [x] `tst_p1_codec_hl2` (10 assertions: bank 11 RX+TX 6-bit/0x40, bank 17 TX latency, bank 18 reset)
- [x] `tst_p1_regression_freeze` (~360 tuples: 10 non-HL2 boards × 2 MOX × 18 banks all byte-identical to pre-refactor main)
- [x] `tst_p1_wire_format` (existing) — green
- [x] `tst_p1_loopback_connection` (existing) — green
- [x] HL2 manual: 14.1 MHz tune → bank 10 C3=0x01 C4=0x01; S-ATT 20 dB → bank 11 C4=0x54; S-ATT 45 dB accepted by widened slider

## Attribution checklist
- [x] New files have verbatim upstream headers (multi-source where applicable)
- [x] THETIS-PROVENANCE.md updated in same commits
- [x] Every ported line has `[ramdor 501e3f5]` / `[mi0bot c26a8a4]` stamp
- [x] verify-thetis-headers.py + check-new-ports.py + verify-inline-cites.py pass

Spec: `docs/architecture/2026-04-20-all-board-radio-control-parity-spec.md` §6 (this PR implements all of Phase A)
Plan: `docs/architecture/2026-04-20-phase3p-a-p1-wire-parity-plan.md`

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 7: Open PR URL in browser**

```bash
gh pr view --web
```

---

## Self-review checklist (run after writing the plan, before handing off)

**1. Spec coverage** — for each item in spec §6, is there a task that implements it?

| Spec item | Task |
|---|---|
| §6.1 8 new files (codec/) | Tasks 2, 4, 6, 7, 8, 10 |
| §6.2 P1RadioConnection refactor | Task 12 |
| §6.2 P2RadioConnection AlexFilterMap call | Task 14 |
| §6.2 BoardCapabilities expansion | Task 11 |
| §6.2 RxApplet slider range | Task 15 |
| §6.2 PROVENANCE rows | Tasks 4, 6, 7, 8, 10, 17 |
| §6.2 CHANGELOG | Task 17 |
| §6.2 PR template | Task 17 |
| §6.3 byte divergence catalog tested | Tasks 5, 7, 8, 9 |
| §6.4 freq → bits recompute | Task 13 |
| §6.5 hand-derived byte-table tests | Tasks 5, 7, 8, 9 |
| §6.5 regression-freeze JSON baseline | Tasks 1, 16 |
| §6.6 DoD #1 branch merged | Task 18 (push + PR) |
| §6.6 DoD #2 HL2 ATT byte trace | Task 18 manual |
| §6.6 DoD #3 HL2 freq tune byte trace | Task 18 manual |
| §6.6 DoD #4 RxApplet slider range | Tasks 11, 15 |
| §6.6 DoD #5 byte-identical non-HL2 | Tasks 1, 16 |
| §6.6 DoD #6 attribution gates | Tasks 4, 6, 7, 8, 10, 17 |
| §6.6 DoD #7 PROVENANCE rows | Task 17 verification step |
| §6.6 DoD #8 inline stamps | Each port task |
| §6.6 DoD #9 no existing-test regression | Task 12 Step 6 + Task 16 Step 6 |
| §6.6 DoD #10 CHANGELOG | Task 17 |

All spec items mapped. ✓

**2. Placeholder scan** — searched for "TBD", "TODO" (allowed only as upstream-comment-preservation), "implement later", "fill in details", "Add appropriate error handling", "similar to". Two intentional `[Insert ... verbatim]` markers in Tasks 4 and 6 + 10 explicitly tell the engineer to copy upstream headers — they're acceptable because the file path + line range is exact. The TODOs in `verify-thetis-headers.py` matches in §4 of spec are allowed (preserving upstream `// TODO` comments).

**3. Type consistency** — `composeCcForBank(int, const CodecContext&, quint8[5])` signature consistent across `IP1Codec` (Task 2), `P1CodecStandard` (Task 6), `P1CodecAnvelinaPro3` (Task 7), `P1CodecRedPitaya` inherits, `P1CodecHl2` (Task 10). `CodecContext` field names consistent (`rxStepAttn`, `txStepAttn`, `alexHpfBits`, `alexLpfBits`, etc.) across producer (Task 12 `buildCodecContext`) and consumers (every codec). `BoardCapabilities::Attenuator::{mask,enableBit,moxBranchesAtt}` field names consistent across Task 11 producer + Task 12/15 consumers.

---

## Execution handoff

**Plan complete and saved to `docs/architecture/2026-04-20-phase3p-a-p1-wire-parity-plan.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — dispatch a fresh subagent per task, review between tasks, fast iteration. Each task is small enough to fit a single subagent's context. Use `superpowers:subagent-driven-development`.

**2. Inline Execution** — execute tasks in this session using `superpowers:executing-plans`, batch execution with checkpoints for review. Heavier on this session's context but no handoff overhead.

**Which approach?**

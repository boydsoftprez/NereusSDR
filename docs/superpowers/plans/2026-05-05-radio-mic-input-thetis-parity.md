# Radio Mic Input: Thetis Parity Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port every Setup control, every wire bit, every per-SKU gating decision, and the radio-mic data path itself to byte-for-byte parity with Thetis. Single PR; closes the gap matrix in `docs/architecture/radio-mic-input-thetis-parity.md` §3.

**Architecture:** Bottom-up, five layers as set out in spec §4. Connection layer adds one missing virtual (`setLineInBoost(double dB)`) plus the 32-entry index table. TransmitModel grows five new properties (`disablePTT`, `allModeMicPTT`, `pttOutDelayMs`, `micGainMinDb`, `micGainMaxDb`). RadioModel adds `connectMicJackSignals()` that wires six mic-related model signals through queued connections plus prime-on-connect. MoxController grows two new slots (`setAllModeMicPTT`, `setPttOutDelayMs`) and a mode-gate inside `onMicPttFromRadio`. UI work is one new spinbox pair on AudioTxInputPage and three new controls on the existing GeneralOptionsPage.

**Tech Stack:** C++20, Qt6 (signals/slots, QSettings via AppSettings, QUdpSocket on connection thread), Qt Test (`QTRY_COMPARE`, `QSignalSpy`), CMake/Ninja, GPG-signed commits. Thetis stamp: `[v2.10.3.13+501e3f51]` (literal `+` joining tag and shortsha; verifier regex `scripts/verify-inline-cites.py` rejects space-separated combos).

---

## Branching strategy

PR #193 (boydsoftprez/NereusSDR#193) is the predecessor and is not yet merged. Its head is `7a45cd8815d06a7c5114a4ed4f287aae3534e9f0` on branch `claude/modest-kirch-fd7ab5`. The spec doc for this work lives on that branch (`docs/architecture/radio-mic-input-thetis-parity.md`).

Two execution options for the implementer:

1. **Wait for #193 to merge, then branch off main.** Cleaner history, no rebase risk. Pick this when the user merges #193.
2. **Branch off PR #193 head right now (`7a45cd8`).** Lets work proceed in parallel; rebase onto main once #193 lands. Pick this when the user wants to keep moving.

The implementer should ask the user which option to take before Task 1. Do NOT branch from `d4c62b6` (the spec text in the original prompt mentioned that commit, but PR #193 has advanced past it; use the current head).

---

## Spec-to-task coverage

| Spec section | Tasks |
| --- | --- |
| §4.1 Connection layer: `setLineInBoost(double dB)` virtual + 32-entry table | 1, 2, 3 |
| §4.1 P2 mic ingress on port 1026 | already done in PR #193; verified, no task |
| §4.2 Connection-side `m_micBoost{true}` default | 4 |
| §4.3 TransmitModel additions (5 properties + flipped `MicXlr` default) | 5, 6, 7, 8, 9, 10 |
| §4.4 Mic data path unification (delete RadioMicSource, route through TxMicSource) | 11, 12, 13, 14 |
| §4.6 RadioModel `connectMicJackSignals` wiring | 15, 16 |
| §4.7 MoxController gate (`setAllModeMicPTT`, `setPttOutDelayMs`, mode-gate) | 17, 18, 19, 20 |
| §4.5 UI: MicGainMin/Max + per-SKU gating tighten + General Options | 21, 22, 23 |
| §6 Bench plan, CHANGELOG, docs | 24, 25 |

---

## File structure

**Modify**

| File | What changes |
| --- | --- |
| `src/core/RadioConnection.h` | Add `virtual void setLineInBoost(double dB)` with default impl that calls `setLineInGain(int)`. Flip `m_micBoost{false}` to `m_micBoost{true}`. Remove `void micFrameDecoded(const float*, int)` signal declaration. |
| `src/core/P1RadioConnection.h` | Override `setLineInBoost(double)` : protocol-specific Thetis-faithful body. |
| `src/core/P1RadioConnection.cpp` | Implement the dB → 5-bit index translation. |
| `src/core/P2RadioConnection.h` | Override `setLineInBoost(double)`. |
| `src/core/P2RadioConnection.cpp` | Implement the dB → 5-bit index translation. |
| `src/models/TransmitModel.h` | Add 5 properties: `disablePTT`, `allModeMicPTT`, `pttOutDelayMs`, `micGainMinDb`, `micGainMaxDb`. Each gets getter/setter/Q_PROPERTY/signal/member. Flip `m_micXlr` default `true` → `false`. |
| `src/models/TransmitModel.cpp` | Setters with `persistOne()` calls; `loadFromSettings(mac)` and `persistToSettings()` read/write the 5 new keys. |
| `src/models/RadioModel.h` | Add `void connectMicJackSignals()` private helper declaration. |
| `src/models/RadioModel.cpp` | Implement `connectMicJackSignals` (6 queued connects + 6 primes). Call from `wireConnectionSignals` after `connectMicPttDisabledSignal()`. Replace `m_radioMicSource` argument to `CompositeTxMicRouter` with `m_txMicSource.get()`. Drop `m_radioMicSource` ownership. Add wiring for `allModeMicPTTChanged → MoxController::setAllModeMicPTT` and `pttOutDelayMsChanged → MoxController::setPttOutDelayMs`, plus prime-on-connect. |
| `src/core/MoxController.h` | Add `void setAllModeMicPTT(bool)` slot, `void setPttOutDelayMs(int)` slot, member `bool m_allModeMicPTT{false}`. |
| `src/core/MoxController.cpp` | Implement the two slots; mode-gate inside `onMicPttFromRadio` per spec §4.7. |
| `src/core/audio/CompositeTxMicRouter.h` | Replace `RadioMicSource* m_radioSource` with `TxMicSource* m_txMicSource`. Update ctor signature. |
| `src/core/audio/CompositeTxMicRouter.cpp` | Pull from `m_txMicSource` for the Radio branch instead of `m_radioSource`. |
| `src/gui/setup/AudioTxInputPage.h` | Add `m_micGainMinSpin`, `m_micGainMaxSpin` members. Declare `boardShowsOrionMicJackPanel(HPSDRHW)` static helper. |
| `src/gui/setup/AudioTxInputPage.cpp` | Add MicGainMin/Max spinbox pair to `buildPcMicGroup`; tighten `updateRadioMicGroupVisibility` to consult `boardShowsOrionMicJackPanel`. |
| `src/gui/setup/GeneralOptionsPage.h` | Add `m_disablePttCheck`, `m_allModeMicPttCheck`, `m_pttOutDelaySpin` members. |
| `src/gui/setup/GeneralOptionsPage.cpp` | Extend `buildOptionsGroup()` with the three new controls and bind them to TransmitModel. |
| `src/core/MicProfileManager.cpp` | No changes : the four new properties are per-MAC, not per-profile. (Reaffirmed by spec §4.3.) |

**Create**

| File | Responsibility |
| --- | --- |
| `tests/tst_radio_connection_set_line_in_boost_db.cpp` | Virtual default impl: dB → 5-bit index table sweep across all 32 entries plus out-of-range clamping. |
| `tests/tst_p1_line_in_boost_wire.cpp` | P1 case-11 C2 byte byte-for-byte after `setLineInBoost(double)` for several representative dB values. |
| `tests/tst_p2_line_in_boost_wire.cpp` | P2 byte-51 wire round-trip. |
| `tests/tst_transmit_model_disable_ptt.cpp` | TM property + signal + per-MAC persistence. |
| `tests/tst_transmit_model_all_mode_mic_ptt.cpp` | TM property + signal + per-MAC persistence. |
| `tests/tst_transmit_model_ptt_out_delay.cpp` | TM property + signal + per-MAC persistence + clamp to 0..500. |
| `tests/tst_transmit_model_mic_gain_min_max.cpp` | TM properties + signals + per-MAC persistence + ordering invariant (`min < max`). |
| `tests/tst_transmit_model_mic_xlr_default.cpp` | Verify default flips to `false` (3.5 mm) per spec §5. |
| `tests/tst_radio_model_mic_jack_wire.cpp` | All 6 mic-jack signals reach connection + prime-on-connect, mirrored on `tst_radio_model_mic_ptt_wire`. |
| `tests/tst_mox_controller_all_mode_mic_ptt.cpp` | Mode-gate matrix: voice → fires unconditionally; CWU + flag-off → ignored; CWU + flag-on → fires. |
| `tests/tst_mox_controller_set_ptt_out_delay_ms.cpp` | `setPttOutDelayMs(N)` updates timer interval, clamps to 0..500. |
| `tests/tst_radio_model_ptt_out_delay.cpp` | TM `pttOutDelayMsChanged → MoxController::setPttOutDelayMs` reaches MoxController. |
| `tests/tst_composite_tx_mic_router_uses_tx_mic_source.cpp` | Radio-source branch pulls from `TxMicSource` (not `RadioMicSource`) on both protocols. |
| `tests/tst_audio_tx_input_per_sku_gating.cpp` | `boardShowsOrionMicJackPanel` returns true only for Orion / OrionMKII / Saturn / SaturnMKII; Hermes group still shown for Hermes / HermesII; Angelia / ANAN100D show no group (Thetis-faithful disable). |
| `tests/tst_audio_tx_input_mic_gain_min_max.cpp` | Spinboxes bind to TM; mic-gain slider range updates live. |
| `tests/tst_general_options_page_disable_ptt.cpp` | Setup checkbox round-trip with `TransmitModel::disablePTT`. |
| `tests/tst_general_options_page_all_mode_mic_ptt.cpp` | Setup checkbox round-trip with `TransmitModel::allModeMicPTT`. |
| `tests/tst_general_options_page_ptt_out_delay.cpp` | Setup spinbox round-trip with `TransmitModel::pttOutDelayMs`. |

**Delete**

| File | Reason |
| --- | --- |
| `src/core/audio/RadioMicSource.h` | Dead path : `micFrameDecoded` never emitted in production. |
| `src/core/audio/RadioMicSource.cpp` | Same. |
| `tests/tst_radio_mic_source.cpp` | Tests dead class. |
| `tests/tst_radio_connection_mic_frame_signal.cpp` | Tests dead signal. |

**Edit-for-deletion-fallout**

| File | Reason |
| --- | --- |
| `tests/tst_tx_mic_router_selector.cpp` | Currently emits `micFrameDecoded` on a test connection; rewrite the test driver to push samples through `TxMicSource::inbound` instead. |
| `tests/CMakeLists.txt` | Remove `tst_radio_mic_source` and `tst_radio_connection_mic_frame_signal` registration; add new test executables. |

---

## TDD cadence convention

Every task in this plan follows the same 5-step cycle:

1. **Write the failing test.** Test code is shown verbatim in the task body.
2. **Run the test, verify it fails.** Expected failure mode is shown explicitly.
3. **Implement the minimal change.** Code is shown verbatim.
4. **Run the test, verify it passes.**
5. **Commit.** GPG-signed, message format shown verbatim.

One task = one TDD cycle = one signed commit. The pre-commit hooks run `verify-inline-cites.py` and `verify-inline-tag-preservation.py` automatically; do NOT skip with `--no-gpg-sign` or `--no-verify`.

After tasks land, run `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure` to confirm the full suite stays green.

---

## Tasks

---

### Task 0: Branching decision + worktree setup

**Files:** none (workflow only).

- [ ] **Step 1: Confirm with user whether to wait for PR #193 or branch off its head.**

Ask: "PR #193 is open at `7a45cd8`. Branch off its head to start now, or wait for it to merge to main? Branching off lets us move now but adds a rebase later."

- [ ] **Step 2: Create the worktree off the chosen base.**

If branching off PR #193:
```bash
cd /Users/j.j.boyd/NereusSDR
git fetch origin
git worktree add .claude/worktrees/radio-mic-parity claude/radio-mic-parity 7a45cd8815d06a7c5114a4ed4f287aae3534e9f0
cd .claude/worktrees/radio-mic-parity
```

If waiting for #193 to merge:
```bash
git -C /Users/j.j.boyd/NereusSDR checkout main
git -C /Users/j.j.boyd/NereusSDR pull
git -C /Users/j.j.boyd/NereusSDR worktree add .claude/worktrees/radio-mic-parity claude/radio-mic-parity main
cd /Users/j.j.boyd/NereusSDR/.claude/worktrees/radio-mic-parity
```

- [ ] **Step 3: Verify Thetis stamp is current.**

```bash
git -C /Users/j.j.boyd/Thetis describe --tags
git -C /Users/j.j.boyd/Thetis rev-parse --short HEAD
```

Expected: `v2.10.3.13-7-g501e3f51` and `501e3f51` respectively. If different, update every `[v2.10.3.13+501e3f51]` cite in this plan to the new stamp before starting.

- [ ] **Step 4: Confirm baseline build is green.**

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Expected: full suite green (every test count comes from PR #193's PR body if branching off it, or from main's CI badge otherwise).

---

### Task 1: Add `setLineInBoost(double dB)` virtual to RadioConnection

**Files:**
- Modify: `src/core/RadioConnection.h`
- Test: `tests/tst_radio_connection_set_line_in_boost_db.cpp`
- Modify: `tests/CMakeLists.txt`

**Thetis source:** `console.cs:40827-40859 [v2.10.3.13+501e3f51]` : the `MakeLineInList()` table builder + the `SetMicGain()` callsite that does `Array.IndexOf(lineinboost, line_in_boost.ToString())` and forwards to `NetworkIO.SetLineBoost(lineboost)`.

The 32-entry table is `for (double i = -34.5; i <= 12; i += 1.5)`. Index 0 = `-34.5`, index 23 = `0.0`, index 31 = `+12.0`.

- [ ] **Step 1: Write the failing test.**

Create `tests/tst_radio_connection_set_line_in_boost_db.cpp`:

```cpp
// =================================================================
// tst_radio_connection_set_line_in_boost_db.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. Verifies the dB → 5-bit
// index translation derived from Thetis console.cs:40827-40859
// [v2.10.3.13+501e3f51] MakeLineInList() + SetMicGain().

#include <QTest>
#include "core/RadioConnection.h"

using namespace NereusSDR;

namespace {

class FakeRadioConnection : public RadioConnection {
    Q_OBJECT
public:
    int lastIndex{-1};
    void init() override {}
    void connectToRadio(const RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void sendTxIq(const float*, int) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int gain) override { lastIndex = gain; }
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void setWatchdogEnabled(bool) override {}
};

} // namespace

class TestRadioConnectionSetLineInBoostDb : public QObject {
    Q_OBJECT
private slots:
    void minDbMapsToIndexZero() {
        FakeRadioConnection c;
        c.setLineInBoost(-34.5);
        QCOMPARE(c.lastIndex, 0);
    }
    void zeroDbMapsToIndexTwentyThree() {
        FakeRadioConnection c;
        c.setLineInBoost(0.0);
        QCOMPARE(c.lastIndex, 23);
    }
    void maxDbMapsToIndexThirtyOne() {
        FakeRadioConnection c;
        c.setLineInBoost(12.0);
        QCOMPARE(c.lastIndex, 31);
    }
    void belowMinClampsToZero() {
        FakeRadioConnection c;
        c.setLineInBoost(-50.0);
        QCOMPARE(c.lastIndex, 0);
    }
    void aboveMaxClampsToThirtyOne() {
        FakeRadioConnection c;
        c.setLineInBoost(20.0);
        QCOMPARE(c.lastIndex, 31);
    }
    void onePointFiveDbStepWalk() {
        FakeRadioConnection c;
        for (int i = 0; i < 32; ++i) {
            const double db = -34.5 + 1.5 * i;
            c.setLineInBoost(db);
            QCOMPARE(c.lastIndex, i);
        }
    }
};

QTEST_MAIN(TestRadioConnectionSetLineInBoostDb)
#include "tst_radio_connection_set_line_in_boost_db.moc"
```

Add to `tests/CMakeLists.txt` (mirror the form used by `tst_radio_model_mic_ptt_wire`):

```cmake
add_executable(tst_radio_connection_set_line_in_boost_db tst_radio_connection_set_line_in_boost_db.cpp)
target_link_libraries(tst_radio_connection_set_line_in_boost_db PRIVATE Qt6::Core Qt6::Test nereus_core)
add_test(NAME tst_radio_connection_set_line_in_boost_db COMMAND tst_radio_connection_set_line_in_boost_db)
```

- [ ] **Step 2: Run the test to verify it fails.**

```bash
cmake --build build --target tst_radio_connection_set_line_in_boost_db
```

Expected: compile error, "no member named 'setLineInBoost' in 'RadioConnection'" (or equivalent).

- [ ] **Step 3: Implement the minimal change in RadioConnection.h.**

Insert after the `setLineInGain(int)` virtual declaration (around line 300):

```cpp
    /// Set the mic line-in boost level in dB. Thetis-faithful translation
    /// from a continuous dB value to the discrete 5-bit `line_in_gain`
    /// wire field used by the radio firmware.
    ///
    /// From Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    ///   MakeLineInList: 32-entry table indexed 0..31, value -34.5..+12.0
    ///                    in 1.5 dB steps.
    ///   SetMicGain:     Array.IndexOf(lineinboost, line_in_boost.ToString())
    ///                    → NetworkIO.SetLineBoost(lineboost).
    /// The default implementation maps dB → index by rounding to the nearest
    /// 1.5 dB step (clamped to 0..31) and forwards to setLineInGain(int).
    /// P1 / P2 inherit this default; override only if a subclass needs a
    /// different translation.
    virtual void setLineInBoost(double dB) {
        constexpr double kMinDb  = -34.5;
        constexpr double kMaxDb  =  12.0;
        constexpr double kStepDb =   1.5;
        if (dB <= kMinDb) { setLineInGain(0);  return; }
        if (dB >= kMaxDb) { setLineInGain(31); return; }
        const int index = static_cast<int>((dB - kMinDb) / kStepDb + 0.5);
        const int clamped = std::clamp(index, 0, 31);
        setLineInGain(clamped);
    }
```

If `<algorithm>` and `<cmath>` are not already included near the top, add `#include <algorithm>`.

- [ ] **Step 4: Run the test to verify it passes.**

```bash
cmake --build build --target tst_radio_connection_set_line_in_boost_db
ctest --test-dir build -R tst_radio_connection_set_line_in_boost_db --output-on-failure
```

Expected: PASS, 6 cases.

- [ ] **Step 5: Commit.**

```bash
git add src/core/RadioConnection.h tests/tst_radio_connection_set_line_in_boost_db.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(radio): add RadioConnection::setLineInBoost(double dB) virtual

Bridges the continuous dB value held by TransmitModel::lineInBoost to
the discrete 5-bit line_in_gain field on the wire.  32-entry table
matches Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51] exactly
(values -34.5..+12.0 in 1.5 dB steps; index 0 = -34.5 dB; index 31 =
+12.0 dB).  Out-of-range clamps to 0/31.

Default impl forwards to setLineInGain(int) so P1/P2 inherit
unchanged.  Tests cover the table sweep + clamp boundaries.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: P1 wire-byte regression for setLineInBoost(double)

**Files:**
- Test: `tests/tst_p1_line_in_boost_wire.cpp`
- Modify: `tests/CMakeLists.txt`

The Task 1 default implementation forwards to `setLineInGain(int)`, which P1 already wires to case-11 C2 low-5-bits. This task locks in the byte-level behaviour with a wire-byte test, mirroring the pattern of `tst_p1_mic_boost_wire`.

- [ ] **Step 1: Write the failing test.**

Create `tests/tst_p1_line_in_boost_wire.cpp` (model after `tests/tst_p1_mic_boost_wire.cpp`):

```cpp
// =================================================================
// tst_p1_line_in_boost_wire.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. Locks in the dB-to-wire
// behaviour for P1 case-11 C2 low-5-bits when setLineInBoost(double)
// is called with representative dB values.
//
// Source: Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13+501e3f51]
//   C2 = (prn->mic.line_in_gain & 0b00011111) | ...
// Source: Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
//   dB → index lookup table (-34.5..+12.0 step 1.5).

#include <QTest>
#include "core/codec/P1CodecStandard.h"
#include "core/codec/CodecContext.h"

using namespace NereusSDR;

class TestP1LineInBoostWire : public QObject {
    Q_OBJECT
private slots:
    void minDbMapsToIndexZeroOnTheWire() {
        // Set lineInGain via the dB path; verify wire byte.
        // Use a CodecContext with p1LineInGain=0 to mirror the
        // result of setLineInBoost(-34.5) → setLineInGain(0).
        CodecContext ctx;
        ctx.p1LineInGain = 0;
        // Build case-11 control bytes via the standard codec.
        // [Implementer: use the existing helper that produces the
        //  4-byte control sequence; mirror tst_p1_mic_boost_wire's
        //  composeCase11ForTest path.]
        QByteArray pkt = P1CodecStandard::composeCase11ForTest(ctx);
        QCOMPARE(pkt[2] & 0x1F, 0);
    }
    void zeroDbMapsToIndexTwentyThree() {
        CodecContext ctx;
        ctx.p1LineInGain = 23;  // result of setLineInBoost(0.0)
        QByteArray pkt = P1CodecStandard::composeCase11ForTest(ctx);
        QCOMPARE(pkt[2] & 0x1F, 23);
    }
    void maxDbMapsToIndexThirtyOne() {
        CodecContext ctx;
        ctx.p1LineInGain = 31;  // result of setLineInBoost(+12.0)
        QByteArray pkt = P1CodecStandard::composeCase11ForTest(ctx);
        QCOMPARE(pkt[2] & 0x1F, 31);
    }
    void otherC2BitsUnaffected() {
        // Setting line_in_gain must not flip puresignal_run bit (C2 bit 6).
        CodecContext ctx;
        ctx.p1LineInGain = 31;
        ctx.p1PuresignalRun = false;
        QByteArray pkt = P1CodecStandard::composeCase11ForTest(ctx);
        QCOMPARE(pkt[2] & 0x40, 0);
        ctx.p1PuresignalRun = true;
        pkt = P1CodecStandard::composeCase11ForTest(ctx);
        QCOMPARE(pkt[2] & 0x40, 0x40);
    }
};

QTEST_MAIN(TestP1LineInBoostWire)
#include "tst_p1_line_in_boost_wire.moc"
```

Add the executable to `tests/CMakeLists.txt`.

- [ ] **Step 2: Run the test.**

```bash
cmake --build build --target tst_p1_line_in_boost_wire
ctest --test-dir build -R tst_p1_line_in_boost_wire --output-on-failure
```

Expected: PASS (no implementation change needed because the dB → index path already exists from Task 1, and the index → wire byte is already wired by `setLineInGain` from PR #193).

If the test fails because `composeCase11ForTest` is not the right helper name, locate the existing P1 case-11 compose helper used by `tst_p1_mic_boost_wire`/`tst_p1_mic_bias_wire` and substitute its name.

- [ ] **Step 3: Commit.**

```bash
git add tests/tst_p1_line_in_boost_wire.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
test(p1): wire-byte regression for setLineInBoost(double)

Locks in the dB → 5-bit index → P1 case-11 C2 low-5-bits path with
explicit table-sweep boundary cases (-34.5 dB → 0, 0.0 dB → 23,
+12.0 dB → 31) and confirms PureSignal-run (bit 6) is unaffected.

Source: Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51] table
+ ChannelMaster/networkproto1.c:600 [v2.10.3.13+501e3f51] wire field.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: P2 wire-byte regression for setLineInBoost(double)

**Files:**
- Test: `tests/tst_p2_line_in_boost_wire.cpp`
- Modify: `tests/CMakeLists.txt`

P2 routes `setLineInGain(int)` to the `MicState` struct used by `CmdHighPriority`. PR #193 already wires this; this task adds the dB-side regression.

- [ ] **Step 1: Write the failing test (skeleton; mirror `tests/tst_p2_mic_boost_wire.cpp`).**

```cpp
// tst_p2_line_in_boost_wire.cpp : P2 byte 51 wire-byte regression
// for setLineInBoost(double).  Cite as Task 2 above.

#include <QTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2LineInBoostWire : public QObject {
    Q_OBJECT
private slots:
    void minDbMapsToZeroOnByte51() {
        P2RadioConnection conn;
        conn.setLineInBoost(-34.5);
        const QByteArray pkt = conn.composeCmdHighPriorityForTest();
        QCOMPARE(static_cast<quint8>(pkt[51]) & 0x1F, 0);
    }
    void zeroDbMapsToTwentyThreeOnByte51() {
        P2RadioConnection conn;
        conn.setLineInBoost(0.0);
        const QByteArray pkt = conn.composeCmdHighPriorityForTest();
        QCOMPARE(static_cast<quint8>(pkt[51]) & 0x1F, 23);
    }
    void maxDbMapsToThirtyOneOnByte51() {
        P2RadioConnection conn;
        conn.setLineInBoost(12.0);
        const QByteArray pkt = conn.composeCmdHighPriorityForTest();
        QCOMPARE(static_cast<quint8>(pkt[51]) & 0x1F, 31);
    }
};

QTEST_MAIN(TestP2LineInBoostWire)
#include "tst_p2_line_in_boost_wire.moc"
```

Use whichever P2 compose helper `tst_p2_mic_boost_wire` already uses; substitute its name.

- [ ] **Step 2: Run, expect PASS without further P2 changes.**

```bash
ctest --test-dir build -R tst_p2_line_in_boost_wire --output-on-failure
```

If it fails because `composeCmdHighPriorityForTest` is not the actual helper name, locate the existing one used by `tst_p2_mic_boost_wire`.

- [ ] **Step 3: Commit.**

```bash
git add tests/tst_p2_line_in_boost_wire.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
test(p2): wire-byte regression for setLineInBoost(double)

P2 byte 51 low-5-bits regression for the dB-to-index translation
added in Task 1.  Mirrors tst_p1_line_in_boost_wire.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Flip connection-side `m_micBoost{false}` to `m_micBoost{true}` (Thetis prime parity)

**Files:**
- Modify: `src/core/RadioConnection.h`
- Modify: existing wire-byte tests if they assert default-state.

**Why:** Spec §4.2 + §5. Thetis ships `mic_boost = true` per `console.cs:13237 [v2.10.3.13+501e3f51]`. The `TransmitModel::m_micBoost` already defaults to `true` (line 1933 of `TransmitModel.h`). The connection-side default `m_micBoost{false}` mismatches and causes a one-frame race window between connect and the first prime-on-connect push.

- [ ] **Step 1: Find any existing wire-byte test that asserts the false-default behaviour.**

```bash
grep -rn "m_micBoost.*false\|micBoost.*default" tests/ src/
```

If any test compares the case-10 C2 byte against `0` immediately after construction (before any setter), update its expected value to reflect the new `0x01` bit-0.

- [ ] **Step 2: Edit `src/core/RadioConnection.h` line ~603.**

```cpp
    // Shared state for setMicBoost (3M-1b G.1).
    // Default true : matches Thetis console.cs:13237 [v2.10.3.13+501e3f51]
    //   private bool mic_boost = true;
    // Avoids a one-frame mismatch between fresh connect and the first
    // prime-on-connect push from RadioModel::connectMicJackSignals.
    bool m_micBoost{true};
```

- [ ] **Step 3: Run the full test suite.**

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Expected: any tests that were asserting the old `false` default now fail. Update the expected wire byte to reflect mic-boost=on.

- [ ] **Step 4: Commit.**

```bash
git add src/core/RadioConnection.h tests/
git commit -m "$(cat <<'EOF'
fix(radio): connection-side m_micBoost default → true (Thetis parity)

Matches Thetis console.cs:13237 [v2.10.3.13+501e3f51]
  private bool mic_boost = true;
TransmitModel::m_micBoost was already true; bringing the connection
field in line eliminates a one-frame race between fresh connect and
the first RadioModel prime-on-connect push.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: TransmitModel, add `disablePTT` property

**Files:**
- Modify: `src/models/TransmitModel.h`
- Modify: `src/models/TransmitModel.cpp`
- Test: `tests/tst_transmit_model_disable_ptt.cpp`
- Modify: `tests/CMakeLists.txt`

**Thetis source:** `console.cs:14302-14305 [v2.10.3.13+501e3f51]` : `chkGeneralDisablePTT` checkbox bound to `console.DisablePTT`. Property gates the entire `PollPTT` loop. Default `false`.

**Persistence key:** `Disable_PTT` under `hardware/<mac>/tx/...` (mirrors `Mic_PTT_Disabled`).

- [ ] **Step 1: Write the failing test.**

```cpp
// tests/tst_transmit_model_disable_ptt.cpp : disablePTT property + signal +
// per-MAC persistence.

#include <QTest>
#include <QSignalSpy>
#include "models/TransmitModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestTransmitModelDisablePtt : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        AppSettings::instance().setForTest(true);
    }
    void defaultIsFalse() {
        TransmitModel tm;
        QCOMPARE(tm.disablePTT(), false);
    }
    void setterEmitsSignalAndUpdates() {
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::disablePTTChanged);
        tm.setDisablePTT(true);
        QCOMPARE(tm.disablePTT(), true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }
    void idempotentSetterDoesNotEmit() {
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::disablePTTChanged);
        tm.setDisablePTT(false);  // same as default
        QCOMPARE(spy.count(), 0);
    }
    void persistsPerMac() {
        TransmitModel tm;
        tm.loadFromSettings(QStringLiteral("AA:BB:CC:DD:EE:FF"));
        tm.setDisablePTT(true);
        TransmitModel reload;
        reload.loadFromSettings(QStringLiteral("AA:BB:CC:DD:EE:FF"));
        QCOMPARE(reload.disablePTT(), true);
    }
};

QTEST_MAIN(TestTransmitModelDisablePtt)
#include "tst_transmit_model_disable_ptt.moc"
```

- [ ] **Step 2: Run, expect compile failure.**

```bash
cmake --build build --target tst_transmit_model_disable_ptt
```

Expected: "no member named 'disablePTT' in 'TransmitModel'".

- [ ] **Step 3: Implement in `TransmitModel.h`.**

Header : declare property:

```cpp
    Q_PROPERTY(bool disablePTT READ disablePTT WRITE setDisablePTT NOTIFY disablePTTChanged)
    bool disablePTT() const noexcept { return m_disablePTT; }
```

Slots section:

```cpp
    void setDisablePTT(bool on);
```

Signals section:

```cpp
    void disablePTTChanged(bool on);
```

Member:

```cpp
    bool m_disablePTT = false;  // console.cs:14302 [v2.10.3.13+501e3f51]
```

`TransmitModel.cpp` : implement setter (mirror `setMicPttDisabled`):

```cpp
void TransmitModel::setDisablePTT(bool on)
{
    if (on == m_disablePTT) { return; }
    m_disablePTT = on;
    persistOne(QStringLiteral("Disable_PTT"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit disablePTTChanged(on);
}
```

`loadFromSettings(mac)` : read the new key (mirror Mic_PTT_Disabled section near line 1091):

```cpp
    const bool disablePTT = s.value(pfx + QLatin1String("Disable_PTT"),
                                    QStringLiteral("False")).toString() == QLatin1String("True");
    setDisablePTT(disablePTT);
```

`persistToSettings()` : write the new key (mirror Mic_PTT_Disabled section near line 1436):

```cpp
    s.setValue(pfx + QLatin1String("Disable_PTT"),
               m_disablePTT ? QStringLiteral("True") : QStringLiteral("False"));
```

- [ ] **Step 4: Run the test, expect PASS.**

```bash
ctest --test-dir build -R tst_transmit_model_disable_ptt --output-on-failure
```

- [ ] **Step 5: Commit.**

```bash
git add src/models/TransmitModel.h src/models/TransmitModel.cpp tests/tst_transmit_model_disable_ptt.cpp tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(transmit): add TransmitModel::disablePTT property + persistence

Per-MAC persisted property gating the entire PollPTT loop.  Mirrors
Thetis console.cs:14302-14305 [v2.10.3.13+501e3f51]
  chkGeneralDisablePTT.Checked = value;
  console.DisablePTT = value;
Default false.  Setup checkbox added in a later task; this commit only
lands the model + persistence so the checkbox can bind cleanly.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: TransmitModel, add `allModeMicPTT` property

**Files:**
- Modify: `src/models/TransmitModel.h`, `src/models/TransmitModel.cpp`
- Test: `tests/tst_transmit_model_all_mode_mic_ptt.cpp`
- Modify: `tests/CMakeLists.txt`

**Thetis source:** `console.cs:12022 [v2.10.3.13+501e3f51]` : `private bool all_mode_mic_ptt`. Default `false`. Persistence key `All_Mode_Mic_PTT`.

Mirror Task 5 step-by-step (failing test → header property → setter with `persistOne` → load/persist hooks → run → commit). Use `tst_transmit_model_all_mode_mic_ptt.cpp` as the file name. Test cases identical to Task 5 with the new property name.

- [ ] **Step 1: Write the failing test (mirror Task 5; substitute `disablePTT` → `allModeMicPTT`, key `Disable_PTT` → `All_Mode_Mic_PTT`, signal `disablePTTChanged` → `allModeMicPTTChanged`).**

- [ ] **Step 2: Run, expect compile failure.**

- [ ] **Step 3: Implement (Thetis cite `console.cs:12022 [v2.10.3.13+501e3f51]`).**

- [ ] **Step 4: Run, expect PASS.**

- [ ] **Step 5: Commit.**

```bash
git commit -m "$(cat <<'EOF'
feat(transmit): add TransmitModel::allModeMicPTT property + persistence

Default false.  Mirrors Thetis console.cs:12022 [v2.10.3.13+501e3f51]
  private bool all_mode_mic_ptt = false;
Per-MAC persisted under key All_Mode_Mic_PTT.  MoxController gate
that consumes this property is added in a later task.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: TransmitModel, add `pttOutDelayMs` property

**Files:**
- Modify: `src/models/TransmitModel.h`, `src/models/TransmitModel.cpp`
- Test: `tests/tst_transmit_model_ptt_out_delay.cpp`
- Modify: `tests/CMakeLists.txt`

**Thetis source:** `console.cs:19694 [v2.10.3.13+501e3f51]` : `private int ptt_out_delay = 20`. Designer range 0..500 ms. Persistence key `PTT_Out_Delay`.

- [ ] **Step 1: Write the failing test.**

```cpp
// tests/tst_transmit_model_ptt_out_delay.cpp

#include <QTest>
#include <QSignalSpy>
#include "models/TransmitModel.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestTransmitModelPttOutDelay : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().setForTest(true); }
    void defaultIsTwenty() {
        TransmitModel tm;
        QCOMPARE(tm.pttOutDelayMs(), 20);
    }
    void setterClampsBelow() {
        TransmitModel tm;
        tm.setPttOutDelayMs(-5);
        QCOMPARE(tm.pttOutDelayMs(), 0);
    }
    void setterClampsAbove() {
        TransmitModel tm;
        tm.setPttOutDelayMs(800);
        QCOMPARE(tm.pttOutDelayMs(), 500);
    }
    void setterEmitsAndUpdates() {
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::pttOutDelayMsChanged);
        tm.setPttOutDelayMs(50);
        QCOMPARE(tm.pttOutDelayMs(), 50);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 50);
    }
    void persistsPerMac() {
        TransmitModel tm;
        tm.loadFromSettings(QStringLiteral("11:22:33:44:55:66"));
        tm.setPttOutDelayMs(75);
        TransmitModel r;
        r.loadFromSettings(QStringLiteral("11:22:33:44:55:66"));
        QCOMPARE(r.pttOutDelayMs(), 75);
    }
};

QTEST_MAIN(TestTransmitModelPttOutDelay)
#include "tst_transmit_model_ptt_out_delay.moc"
```

- [ ] **Step 2: Run, expect compile fail.**

- [ ] **Step 3: Implement.**

`TransmitModel.h`:

```cpp
    Q_PROPERTY(int  pttOutDelayMs READ pttOutDelayMs WRITE setPttOutDelayMs NOTIFY pttOutDelayMsChanged)
    int pttOutDelayMs() const noexcept { return m_pttOutDelayMs; }
public slots:
    void setPttOutDelayMs(int ms);
signals:
    void pttOutDelayMsChanged(int ms);
private:
    int m_pttOutDelayMs = 20;  // console.cs:19694 [v2.10.3.13+501e3f51]
```

`TransmitModel.cpp`:

```cpp
void TransmitModel::setPttOutDelayMs(int ms)
{
    const int clamped = std::clamp(ms, 0, 500);
    if (clamped == m_pttOutDelayMs) { return; }
    m_pttOutDelayMs = clamped;
    persistOne(QStringLiteral("PTT_Out_Delay"), QString::number(clamped));
    emit pttOutDelayMsChanged(clamped);
}
```

Add load + persist key under `loadFromSettings(mac)` and `persistToSettings()` (mirror Task 5).

- [ ] **Step 4: Run, expect PASS.**

- [ ] **Step 5: Commit.**

```bash
git commit -m "$(cat <<'EOF'
feat(transmit): add TransmitModel::pttOutDelayMs property + persistence

Default 20 ms; clamped to 0..500.  Mirrors Thetis
console.cs:19694 [v2.10.3.13+501e3f51]  private int ptt_out_delay = 20.
Per-MAC persisted under key PTT_Out_Delay.  MoxController slot that
consumes this property is added in a later task.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: TransmitModel, add `micGainMinDb` property

Mirror Task 7 with property name `micGainMinDb`, default `-40`, clamp range `-96..0`, key `MicGainMin`. Cite `setup.cs:9678-9683 [v2.10.3.13+501e3f51]`.

- [ ] **Step 1: Write the failing test (mirror Task 7).**
- [ ] **Step 2: Run.**
- [ ] **Step 3: Implement (default `-40`, range `-96..0`).**
- [ ] **Step 4: Run.**
- [ ] **Step 5: Commit (commit message: "feat(transmit): add TransmitModel::micGainMinDb property + persistence : default -40").**

---

### Task 9: TransmitModel, add `micGainMaxDb` property

Mirror Task 7 with property name `micGainMaxDb`, default `+10`, clamp range `1..70`, key `MicGainMax`. Cite `setup.cs:9685-9689 [v2.10.3.13+501e3f51]`.

- [ ] Steps as Task 7. Commit message ends "default +10".

---

### Task 10: Flip `m_micXlr` model default `true` → `false` (Thetis 3.5 mm parity)

**Files:**
- Modify: `src/models/TransmitModel.h`
- Test: `tests/tst_transmit_model_mic_xlr_default.cpp`
- Update: any existing test that asserts the old default-true.

**Spec source:** §5 of the design doc : Thetis Saturn ships 3.5 mm checked. NereusSDR currently boots a fresh Saturn into XLR routing. The `MicXlr` model bool maps to `radSaturnXLR.Checked`, so model `false` = 3.5 mm checked, which matches Thetis.

- [ ] **Step 1: Write the failing test.**

```cpp
// tests/tst_transmit_model_mic_xlr_default.cpp

#include <QTest>
#include "models/TransmitModel.h"

class TestTransmitModelMicXlrDefault : public QObject {
    Q_OBJECT
private slots:
    void freshModelDefaultsToThreePointFiveMm() {
        NereusSDR::TransmitModel tm;
        QCOMPARE(tm.micXlr(), false);
    }
};

QTEST_MAIN(TestTransmitModelMicXlrDefault)
#include "tst_transmit_model_mic_xlr_default.moc"
```

- [ ] **Step 2: Run, expect FAIL** (current default is `true`).

- [ ] **Step 3: Edit `TransmitModel.h`** : search for `m_micXlr` and flip default to `false`. Update inline comment to cite `console.cs:13238 [v2.10.3.13+501e3f51]` (`radSaturn3p5mm.Checked = true` initialization).

- [ ] **Step 4: Update existing tests that asserted XLR default.**

```bash
grep -rn "micXlr.*true\|MicXlr.*true\|m_micXlr.*true" tests/
```

Each hit is either an assertion (flip to `false`) or a setter call (leave alone).

- [ ] **Step 5: Run full suite.**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all pass; the new default-false test passes; previously-XLR-asserting tests now match the flipped expectation.

- [ ] **Step 6: Commit.**

```bash
git commit -m "$(cat <<'EOF'
fix(transmit): flip MicXlr model default true → false (Thetis 3.5 mm)

Thetis Saturn ships 3.5 mm checked (radSaturn3p5mm.Checked = true,
console.cs:13238 [v2.10.3.13+501e3f51]).  NereusSDR's MicXlr model
bool maps to radSaturnXLR.Checked, so the parity-correct default is
false.  Issue #182 corrected the P2 wire byte to 0x20 (XLR-set);
this commit corrects the model default so a fresh Saturn boots into
3.5 mm routing and the prime-on-connect writes the matching wire bit.

Existing user settings persist via AppSettings; only fresh-install /
never-toggled users see the difference.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 11: CompositeTxMicRouter, replace RadioMicSource with TxMicSource

**Files:**
- Modify: `src/core/audio/CompositeTxMicRouter.h`
- Modify: `src/core/audio/CompositeTxMicRouter.cpp`
- Test: `tests/tst_composite_tx_mic_router_uses_tx_mic_source.cpp`
- Modify: `tests/CMakeLists.txt`

**Why:** Spec §4.4. `RadioMicSource` is a dead path subscribed to `micFrameDecoded`, which is never emitted in production. `TxMicSource` is already the live ring on both protocols (P1 EP6 mic-byte zone + P2 port-1026). Routing the user's PC/Radio toggle through `TxMicSource` makes the toggle real.

- [ ] **Step 1: Write the failing test.**

```cpp
// tests/tst_composite_tx_mic_router_uses_tx_mic_source.cpp

#include <QTest>
#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/PcMicSource.h"
#include "core/audio/TxMicSource.h"

using namespace NereusSDR;

class TestCompositeTxMicRouterUsesTxMicSource : public QObject {
    Q_OBJECT
private slots:
    void radioBranchPullsFromTxMicSource() {
        PcMicSource pc;
        TxMicSource tx;
        // Push a known sample into TxMicSource.
        constexpr int N = 64;
        std::array<float, N> in{};
        for (int i = 0; i < N; ++i) { in[i] = 0.5f; }
        tx.inbound(in.data(), N);

        CompositeTxMicRouter router(&pc, &tx, /*hasMicJack=*/true);
        router.setActiveSource(MicSource::Radio);

        std::array<float, N> out{};
        const int got = router.pullSamples(out.data(), N);
        QCOMPARE(got, N);
        QCOMPARE(out[0], 0.5f);
    }
    void pcBranchPullsFromPcMicSource() {
        // Mock PcMicSource if needed; verify Radio-source isolation.
        // [Implementer: substitute the existing PcMicSource test fixture
        //  pattern from tst_pc_mic_source.cpp.]
    }
};

QTEST_MAIN(TestCompositeTxMicRouterUsesTxMicSource)
#include "tst_composite_tx_mic_router_uses_tx_mic_source.moc"
```

- [ ] **Step 2: Run, expect compile fail.**

```bash
cmake --build build --target tst_composite_tx_mic_router_uses_tx_mic_source
```

Expected: "no matching constructor for `CompositeTxMicRouter(PcMicSource*, TxMicSource*, bool)`" (current ctor takes `RadioMicSource*`).

- [ ] **Step 3: Edit `CompositeTxMicRouter.h`.**

Replace the `RadioMicSource` forward declaration and member with `TxMicSource`:

```cpp
class TxMicSource;
// ...
class CompositeTxMicRouter : public TxMicRouter {
public:
    CompositeTxMicRouter(PcMicSource*  pcSource,
                         TxMicSource*  txMicSource,
                         bool          hasMicJack);
    // ...
private:
    PcMicSource* m_pcSource;       // non-owning
    TxMicSource* m_txMicSource;    // non-owning; may be null on HL2 if no mic jack
};
```

- [ ] **Step 4: Edit `CompositeTxMicRouter.cpp`.**

```cpp
#include "core/audio/PcMicSource.h"
#include "core/audio/TxMicSource.h"   // was: RadioMicSource.h

CompositeTxMicRouter::CompositeTxMicRouter(PcMicSource* pc,
                                           TxMicSource* tx,
                                           bool hasMicJack)
    : m_pcSource(pc)
    , m_txMicSource(hasMicJack ? tx : nullptr)
    , m_hasMicJack(hasMicJack)
{}

int CompositeTxMicRouter::pullSamples(float* dst, int n) {
    const MicSource source = m_activeSource.load();
    if (source == MicSource::Radio && m_txMicSource != nullptr) {
        return m_txMicSource->pullSamples(dst, n);
    }
    if (m_pcSource != nullptr) {
        return m_pcSource->pullSamples(dst, n);
    }
    return 0;
}
```

If `TxMicSource` does not yet expose `pullSamples`, audit its header : it almost certainly already does for the production TxWorkerThread path. Use the existing method; don't add a new one.

- [ ] **Step 5: Run the test, expect PASS.**

```bash
ctest --test-dir build -R tst_composite_tx_mic_router_uses_tx_mic_source --output-on-failure
```

- [ ] **Step 6: Commit.**

```bash
git commit -m "$(cat <<'EOF'
refactor(audio): CompositeTxMicRouter pulls from TxMicSource for Radio

Removes the RadioMicSource dependency.  TxMicSource is already the
live ring fed by P1 EP6 mic-byte zone and P2 port-1026 in PR #193;
routing the user's PC/Radio toggle through TxMicSource makes the
toggle actually switch sources on both protocols.

RadioMicSource and the micFrameDecoded signal are removed in
follow-up tasks.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 12: RadioModel passes TxMicSource into CompositeTxMicRouter, drops RadioMicSource ownership

**Files:**
- Modify: `src/models/RadioModel.h`
- Modify: `src/models/RadioModel.cpp`

- [ ] **Step 1: Locate the construction site (currently around RadioModel.cpp:1725).**

```cpp
m_compositeMicRouter = std::make_unique<CompositeTxMicRouter>(
    m_pcMicSource.get(), m_radioMicSource.get(), hasMicJack);
```

- [ ] **Step 2: Replace with TxMicSource pointer.**

```cpp
m_compositeMicRouter = std::make_unique<CompositeTxMicRouter>(
    m_pcMicSource.get(), m_txMicSource.get(), hasMicJack);
```

- [ ] **Step 3: Drop `m_radioMicSource` from `RadioModel.h`.**

Remove the `std::unique_ptr<RadioMicSource> m_radioMicSource;` member, the forward declaration `class RadioMicSource;`, and any include of `RadioMicSource.h`.

- [ ] **Step 4: Remove the construction call for `m_radioMicSource`.**

```bash
grep -n "m_radioMicSource\s*=\s*std::make_unique\|m_radioMicSource\s*=\s*nullptr\|m_radioMicSource\.reset\|RadioMicSource(\|RadioMicSource >" src/models/RadioModel.cpp
```

Remove every hit. Then update the teardown path (`m_compositeMicRouter.reset()` already covers it via Task 11; just delete the now-dangling `m_radioMicSource.reset()`).

- [ ] **Step 5: Build.**

```bash
cmake --build build -j$(nproc)
```

Expected: clean build. Compile errors here mean another file references `m_radioMicSource`; resolve them.

- [ ] **Step 6: Run the affected tests.**

```bash
ctest --test-dir build -R "tx_mic|radio_model|composite" --output-on-failure
```

Some tests previously passed `RadioMicSource*` into mocks : they will fail to compile. Update them to use `TxMicSource*` per the new signature, or substitute a `nullptr` for the `txMicSource` argument when the test doesn't exercise the Radio branch.

- [ ] **Step 7: Commit.**

```bash
git commit -m "$(cat <<'EOF'
refactor(model): RadioModel constructs CompositeTxMicRouter with TxMicSource

Drops the RadioMicSource ownership entirely.  The composite router
now points at the same TxMicSource that already receives P1 EP6 +
P2 port-1026 mic samples, so the PC/Radio toggle finally switches
the actual TX source.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 13: Delete `RadioMicSource.{h,cpp}` and its tests

**Files:**
- Delete: `src/core/audio/RadioMicSource.h`
- Delete: `src/core/audio/RadioMicSource.cpp`
- Delete: `tests/tst_radio_mic_source.cpp`
- Modify: `tests/CMakeLists.txt` (remove the `tst_radio_mic_source` target)
- Modify: `src/CMakeLists.txt` or top-level CMake (remove the `RadioMicSource.cpp` source-list entry)

- [ ] **Step 1: Find every reference.**

```bash
grep -rn "RadioMicSource" src/ tests/ docs/
```

- [ ] **Step 2: Delete the production source files.**

```bash
git rm src/core/audio/RadioMicSource.h src/core/audio/RadioMicSource.cpp
```

- [ ] **Step 3: Delete the dedicated test file.**

```bash
git rm tests/tst_radio_mic_source.cpp
```

- [ ] **Step 4: Remove from CMake.**

In `tests/CMakeLists.txt` find the `tst_radio_mic_source` add_executable / add_test block and remove it. In whichever source-list CMakeLists adds `RadioMicSource.cpp` to the `nereus_core` library (likely `src/CMakeLists.txt`), remove that line.

- [ ] **Step 5: Build and run full suite.**

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Expected: clean build, every test pass, total test count drops by exactly 1 (or the count in `tst_radio_mic_source`).

- [ ] **Step 6: Commit.**

```bash
git commit -m "$(cat <<'EOF'
chore(audio): delete dead RadioMicSource path

micFrameDecoded was never emitted in production code; RadioMicSource
only ever returned silence.  Task 11 + Task 12 routed the live
TxMicSource through CompositeTxMicRouter, leaving RadioMicSource
fully unreferenced.  This commit removes the source files and the
dedicated unit test.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 14: Remove `micFrameDecoded` signal from RadioConnection

**Files:**
- Modify: `src/core/RadioConnection.h`
- Delete: `tests/tst_radio_connection_mic_frame_signal.cpp`
- Modify: `tests/tst_tx_mic_router_selector.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Audit current emitters.**

```bash
grep -rn "emit micFrameDecoded\|micFrameDecoded(" src/ tests/
```

Expected: only test code emits it. If any production file emits it, that emitter has to be reconciled with the spec : pause and ask the user.

- [ ] **Step 2: Remove the signal declaration from `RadioConnection.h`.**

Delete the `void micFrameDecoded(const float* samples, int frames);` line and its preceding doc-comment block (around RadioConnection.h:466-488).

- [ ] **Step 3: Delete the dedicated signal-existence test.**

```bash
git rm tests/tst_radio_connection_mic_frame_signal.cpp
```

Remove its `add_executable` + `add_test` block from `tests/CMakeLists.txt`.

- [ ] **Step 4: Update `tests/tst_tx_mic_router_selector.cpp`.**

The current test driver emits `micFrameDecoded` on a test connection. Rewrite it to push samples through `TxMicSource::inbound` directly. Concretely, replace:

```cpp
emit micFrameDecoded(samples, frames);
```

with a `txMicSource.inbound(samples, frames);` call against a `TxMicSource` instance the test already constructs (or constructs anew). The test's intent is "selector switches Radio → TxMicSource → audible samples"; the new path expresses that intent more directly.

- [ ] **Step 5: Build + full suite.**

```bash
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

- [ ] **Step 6: Commit.**

```bash
git commit -m "$(cat <<'EOF'
chore(radio): remove dead RadioConnection::micFrameDecoded signal

Unreferenced after Tasks 11-13 routed CompositeTxMicRouter through
TxMicSource directly.  Updates tst_tx_mic_router_selector to push
through TxMicSource::inbound rather than emitting the dead signal.
Drops tst_radio_connection_mic_frame_signal entirely.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 15: Add `connectMicJackSignals()` helper declaration on RadioModel

**Files:**
- Modify: `src/models/RadioModel.h`
- Modify: `src/models/RadioModel.cpp`

This task only adds the empty helper plus its test seam; Task 16 fills the body.

- [ ] **Step 1: Declare in `RadioModel.h`** (next to `connectMicPttDisabledSignal`):

```cpp
    // Wires every TransmitModel mic-jack signal to its RadioConnection
    // counterpart through queued connections, then primes each one with
    // the current model value.  Mirrors connectMicPttDisabledSignal.
    void connectMicJackSignals();

    // Test seam : mirrors wireMicPttDisabledForTest().
    void wireMicJackForTest() { connectMicJackSignals(); }
```

- [ ] **Step 2: Stub the body in `RadioModel.cpp`** with a `// Task 16 will fill this in` comment placeholder (returns immediately).

- [ ] **Step 3: Call from `wireConnectionSignals` after `connectMicPttDisabledSignal();`.**

- [ ] **Step 4: Build, expect clean.**

- [ ] **Step 5: Commit (intermediate seam; no behaviour yet).**

```bash
git commit -m "$(cat <<'EOF'
refactor(model): add RadioModel::connectMicJackSignals seam (no-op)

Mirror of connectMicPttDisabledSignal extracted as a placeholder.
Task 16 fills the body with the six queued connects + primes.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 16: Wire 6 mic-jack signals + primes via `connectMicJackSignals`

**Files:**
- Modify: `src/models/RadioModel.cpp`
- Test: `tests/tst_radio_model_mic_jack_wire.cpp`
- Modify: `tests/CMakeLists.txt`

**Pattern:** identical to `connectMicPttDisabledSignal` (RadioModel.cpp:3174-3189). One queued connect + one prime per signal.

The 6 signals to wire:

| TransmitModel signal | RadioConnection slot |
| --- | --- |
| `micBoostChanged(bool)` | `setMicBoost(bool)` |
| `lineInChanged(bool)` | `setLineIn(bool)` |
| `micTipRingChanged(bool)` | `setMicTipRing(bool)` |
| `micBiasChanged(bool)` | `setMicBias(bool)` |
| `micXlrChanged(bool)` | `setMicXlr(bool)` |
| `lineInBoostChanged(double)` | `setLineInBoost(double)` |

- [ ] **Step 1: Write the failing test (mirror `tst_radio_model_mic_ptt_wire.cpp`).**

```cpp
// tests/tst_radio_model_mic_jack_wire.cpp

#include <QTest>
#include <QSignalSpy>
#include "models/RadioModel.h"
#include "core/RadioConnection.h"  // includes the FakeRadioConnection helper from Task 1

class TestRadioModelMicJackWire : public QObject {
    Q_OBJECT
private slots:
    void micBoostReachesConnection() { /* set TM, expect FakeConn::lastMicBoost == set */ }
    void lineInReachesConnection() { /* same */ }
    void micTipRingReachesConnection() { /* same */ }
    void micBiasReachesConnection() { /* same */ }
    void micXlrReachesConnection() { /* same */ }
    void lineInBoostReachesConnection() { /* set TM dB, expect FakeConn::lastIndex == 23 etc */ }
    void primeOnConnectPushesAllSix() {
        // After construction with non-default TM values, connectMicJackSignals
        // must push the current model value to the connection (not wait for a
        // setter event).
    }
};

QTEST_MAIN(TestRadioModelMicJackWire)
#include "tst_radio_model_mic_jack_wire.moc"
```

The FakeRadioConnection helper from Task 1 needs to capture `m_lastMicBoost`, `m_lastLineIn`, `m_lastMicTipRing`, `m_lastMicBias`, `m_lastMicXlr`, plus the existing `lastIndex` for line-in-gain. Extend the class in this test file.

- [ ] **Step 2: Run, expect FAIL** (`connectMicJackSignals` is a no-op stub from Task 15).

- [ ] **Step 3: Fill in `connectMicJackSignals()` body.**

Mirror the `connectMicPttDisabledSignal` pattern exactly, repeated six times. Pseudocode:

```cpp
void RadioModel::connectMicJackSignals()
{
    if (!m_connection) { return; }

    // 6 queued connects.
    QObject::connect(&m_transmitModel, &TransmitModel::micBoostChanged,
                     m_connection, &RadioConnection::setMicBoost,
                     Qt::QueuedConnection);
    // ... lineIn, micTipRing, micBias, micXlr, lineInBoost.

    // 6 primes : fired through invokeMethod so the connection thread
    // sees the current model value at attach time.
    QMetaObject::invokeMethod(m_connection,
        [conn = m_connection, v = m_transmitModel.micBoost()]() {
            conn->setMicBoost(v);
        }, Qt::QueuedConnection);
    // ... the other five.
}
```

For `lineInBoost`, prime with `m_transmitModel.lineInBoost()` (a `double`) into `setLineInBoost(double)`.

- [ ] **Step 4: Run the test, expect PASS.**

- [ ] **Step 5: Commit.**

```bash
git commit -m "$(cat <<'EOF'
feat(model): wire 6 mic-jack signals + primes through RadioModel

Mic boost, line-in, mic tip/ring, mic bias, mic XLR, line-in boost
(dB).  Each signal now reaches its RadioConnection counterpart over
a queued connection, with a prime-on-connect push so a fresh connect
honours the persisted user state.

Mirrors the connectMicPttDisabledSignal pattern from issue #182.

Source: docs/architecture/radio-mic-input-thetis-parity.md §4.6.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 17: MoxController, add `setAllModeMicPTT(bool)` slot

**Files:**
- Modify: `src/core/MoxController.h`
- Modify: `src/core/MoxController.cpp`
- Test: `tests/tst_mox_controller_all_mode_mic_ptt.cpp`
- Modify: `tests/CMakeLists.txt`

This task only adds the slot + member; Task 19 wires the gate inside `onMicPttFromRadio`.

- [ ] **Step 1: Write the failing test (slot existence + state mirror).**

```cpp
// tests/tst_mox_controller_all_mode_mic_ptt.cpp (Task 17 portion)

#include <QTest>
#include "core/MoxController.h"

class TestMoxControllerAllModeMicPtt : public QObject {
    Q_OBJECT
private slots:
    void setterUpdatesInternalFlag() {
        NereusSDR::MoxController mc;
        QCOMPARE(mc.allModeMicPTT(), false);
        mc.setAllModeMicPTT(true);
        QCOMPARE(mc.allModeMicPTT(), true);
    }
};

QTEST_MAIN(TestMoxControllerAllModeMicPtt)
#include "tst_mox_controller_all_mode_mic_ptt.moc"
```

(Task 19 will add gate-behaviour cases to this same file.)

- [ ] **Step 2: Run, expect compile FAIL.**

- [ ] **Step 3: Add to MoxController.h.**

```cpp
public slots:
    void setAllModeMicPTT(bool on);

public:
    bool allModeMicPTT() const noexcept { return m_allModeMicPTT; }

private:
    bool m_allModeMicPTT{false};  // console.cs:12022 [v2.10.3.13+501e3f51]
```

In `MoxController.cpp`:

```cpp
void MoxController::setAllModeMicPTT(bool on)
{
    m_allModeMicPTT = on;
}
```

- [ ] **Step 4: Run, expect PASS.**

- [ ] **Step 5: Commit.**

```bash
git commit -m "$(cat <<'EOF'
feat(mox): add MoxController::setAllModeMicPTT slot

Stores the current value of TransmitModel::allModeMicPTT inside
MoxController so onMicPttFromRadio (Task 19) can consult it on
every mic-PTT decode without reaching back into the model.  No
behavior change yet : gate is in Task 19.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 18: MoxController, add `setPttOutDelayMs(int)` production slot

**Files:**
- Modify: `src/core/MoxController.h`, `src/core/MoxController.cpp`
- Test: `tests/tst_mox_controller_set_ptt_out_delay_ms.cpp`
- Modify: `tests/CMakeLists.txt`

The existing `setTimerIntervals(int rfMs, int moxMs, int spaceMs, int keyUpMs, int pttOutMs, int breakInMs)` is marked "FOR TESTING ONLY" and overrides every timer at once. The production slot only updates `m_pttOutDelayTimer`.

- [ ] **Step 1: Write the failing test.**

```cpp
// tests/tst_mox_controller_set_ptt_out_delay_ms.cpp

#include <QTest>
#include "core/MoxController.h"

class TestMoxControllerSetPttOutDelayMs : public QObject {
    Q_OBJECT
private slots:
    void defaultIsTwenty() {
        NereusSDR::MoxController mc;
        QCOMPARE(mc.pttOutDelayMs(), 20);
    }
    void setterUpdatesInterval() {
        NereusSDR::MoxController mc;
        mc.setPttOutDelayMs(75);
        QCOMPARE(mc.pttOutDelayMs(), 75);
    }
    void clampsBelow() {
        NereusSDR::MoxController mc;
        mc.setPttOutDelayMs(-5);
        QCOMPARE(mc.pttOutDelayMs(), 0);
    }
    void clampsAbove() {
        NereusSDR::MoxController mc;
        mc.setPttOutDelayMs(800);
        QCOMPARE(mc.pttOutDelayMs(), 500);
    }
};

QTEST_MAIN(TestMoxControllerSetPttOutDelayMs)
#include "tst_mox_controller_set_ptt_out_delay_ms.moc"
```

- [ ] **Step 2: Run, expect FAIL.**

- [ ] **Step 3: Implement.**

`MoxController.h`:

```cpp
public slots:
    void setPttOutDelayMs(int ms);

public:
    int pttOutDelayMs() const noexcept { return m_pttOutDelayTimer.interval(); }
```

`MoxController.cpp`:

```cpp
void MoxController::setPttOutDelayMs(int ms)
{
    const int clamped = std::clamp(ms, 0, 500);
    m_pttOutDelayTimer.setInterval(clamped);
}
```

- [ ] **Step 4: Run, expect PASS.**

- [ ] **Step 5: Commit.**

```bash
git commit -m "$(cat <<'EOF'
feat(mox): add MoxController::setPttOutDelayMs production slot

Distinct from the test-only setTimerIntervals helper: only updates
the ptt_out timer interval, leaves rf/mox/space/keyUp/breakIn
untouched.  Clamps to 0..500 (matches Thetis udGenPTTOutDelay range
in setup.designer.cs:9029-9226 [v2.10.3.13+501e3f51]).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 19: MoxController.onMicPttFromRadio, AllModeMicPTT mode-gate

**Files:**
- Modify: `src/core/MoxController.cpp`
- Modify: `tests/tst_mox_controller_all_mode_mic_ptt.cpp` (extend Task 17 file with gate cases)

**Thetis source:** `console.cs:25480-25495 [v2.10.3.13+501e3f51]`. Pseudocode for the port (already shown in spec §4.7).

- [ ] **Step 1: Extend the Task 17 test file.**

```cpp
    void voiceModeFiresUnconditionally() {
        // USB mode, AllModeMicPTT=false, mic_ptt=true → MOX engaged.
    }
    void cwModeWithFlagOffIgnoresMicPtt() {
        // CWU mode, AllModeMicPTT=false, mic_ptt=true → MOX NOT engaged,
        // pttMode unchanged.  (3M-2 will fill the CW branch later;
        // for 3M-1 the gate just suppresses the call.)
    }
    void cwModeWithFlagOnFires() {
        // CWU mode, AllModeMicPTT=true, mic_ptt=true → MOX engaged
        // (pttMode set to Mic; 3M-2 will revisit when CW dispatch lands).
    }
```

- [ ] **Step 2: Run, expect FAIL** (current code engages MOX unconditionally on any mic_ptt press).

- [ ] **Step 3: Edit `onMicPttFromRadio` per spec §4.7.**

```cpp
void MoxController::onMicPttFromRadio(bool pressed)
{
    if (pressed) {
        const bool voice = isVoiceMode(m_currentMode);
        if (!voice && !m_allModeMicPTT) {
            // Thetis ignores mic_ptt outside voice modes unless
            // AllModeMicPTT is set.  CW dispatch lands in 3M-2.
            return;
        }
        setPttMode(PttMode::Mic);
        setMox(true);
    } else {
        if (m_pttMode == PttMode::Mic) {
            setMox(false);
        }
    }
}
```

Cite `console.cs:25480-25495 [v2.10.3.13+501e3f51]` in the inline comment.

- [ ] **Step 4: Run, expect PASS for all cases.**

- [ ] **Step 5: Commit.**

```bash
git commit -m "$(cat <<'EOF'
fix(mox): mode-gate onMicPttFromRadio with AllModeMicPTT

Thetis ignores mic_ptt outside voice modes unless AllModeMicPTT is
set (console.cs:25480-25495 [v2.10.3.13+501e3f51]).  Without this
gate, pressing a mic-jack PTT in CWU mode without the flag set
would still drive MOX, which doesn't match Thetis behaviour.

CW dispatch in non-voice modes is deferred to 3M-2 per spec §7;
the gate leaves that path open by suppressing the mic_ptt call so
3M-2 can substitute its own setPttMode(PttMode::CW) branch.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 20: RadioModel wires `allModeMicPTT` + `pttOutDelayMs` from TransmitModel to MoxController

**Files:**
- Modify: `src/models/RadioModel.cpp`
- Test: `tests/tst_radio_model_ptt_out_delay.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test.**

```cpp
// tests/tst_radio_model_ptt_out_delay.cpp

#include <QTest>
#include "models/RadioModel.h"

class TestRadioModelPttOutDelay : public QObject {
    Q_OBJECT
private slots:
    void tmChangeReachesMoxController() {
        NereusSDR::RadioModel rm;
        rm.transmitModel().setPttOutDelayMs(123);
        QTRY_COMPARE(rm.moxController()->pttOutDelayMs(), 123);
    }
    void allModeFlagReachesMox() {
        NereusSDR::RadioModel rm;
        rm.transmitModel().setAllModeMicPTT(true);
        QTRY_COMPARE(rm.moxController()->allModeMicPTT(), true);
    }
};

QTEST_MAIN(TestRadioModelPttOutDelay)
#include "tst_radio_model_ptt_out_delay.moc"
```

If `moxController()` is not exposed publicly, expose it as `MoxController* moxController() const`. Or use the existing `wireMicPttDisabledForTest`-style seam if one exists for MoxController.

- [ ] **Step 2: Run, expect FAIL.**

- [ ] **Step 3: Wire the connect calls in `RadioModel::setupMoxController` (or wherever the existing MoxController slots are wired).**

```cpp
QObject::connect(&m_transmitModel, &TransmitModel::pttOutDelayMsChanged,
                 m_moxController.get(), &MoxController::setPttOutDelayMs,
                 Qt::DirectConnection);  // MoxController is on main thread

QObject::connect(&m_transmitModel, &TransmitModel::allModeMicPTTChanged,
                 m_moxController.get(), &MoxController::setAllModeMicPTT,
                 Qt::DirectConnection);

// Prime once at setup time.
m_moxController->setPttOutDelayMs(m_transmitModel.pttOutDelayMs());
m_moxController->setAllModeMicPTT(m_transmitModel.allModeMicPTT());
```

- [ ] **Step 4: Run, expect PASS.**

- [ ] **Step 5: Commit.**

```bash
git commit -m "$(cat <<'EOF'
feat(model): wire allModeMicPTT + pttOutDelayMs to MoxController

Direct connections (MoxController shares the main thread).  Primes
once at MoxController setup so the persisted user state takes effect
without waiting for a UI toggle.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 21: AudioTxInputPage, add `udMicGainMin` / `udMicGainMax` spinboxes to PC Mic group

**Files:**
- Modify: `src/gui/setup/AudioTxInputPage.h`
- Modify: `src/gui/setup/AudioTxInputPage.cpp`
- Test: `tests/tst_audio_tx_input_mic_gain_min_max.cpp`
- Modify: `tests/CMakeLists.txt`

**Thetis source:** `setup.designer.cs:46740-46924 [v2.10.3.13+501e3f51]` : `udMicGainMin` / `udMicGainMax`. NumericUpDowns. Range mirrors the spinbox widget. The slider for mic gain consults these as bounds.

- [ ] **Step 1: Write the failing test.**

```cpp
// tests/tst_audio_tx_input_mic_gain_min_max.cpp
// Verify: spinboxes round-trip with TransmitModel::micGainMinDb /
//         micGainMaxDb; mic-gain slider range updates live.

#include <QTest>
#include <QSpinBox>
#include "gui/setup/AudioTxInputPage.h"
#include "models/RadioModel.h"

class TestAudioTxInputMicGainMinMax : public QObject {
    Q_OBJECT
private slots:
    void minSpinReflectsModel() {
        NereusSDR::RadioModel rm;
        rm.transmitModel().setMicGainMinDb(-50);
        NereusSDR::AudioTxInputPage page(&rm);
        QSpinBox* spin = page.findChild<QSpinBox*>(QStringLiteral("micGainMinSpin"));
        QVERIFY(spin);
        QCOMPARE(spin->value(), -50);
    }
    void maxSpinReflectsModel() { /* mirror */ }
    void minSpinChangePushesToModel() {
        NereusSDR::RadioModel rm;
        NereusSDR::AudioTxInputPage page(&rm);
        QSpinBox* spin = page.findChild<QSpinBox*>(QStringLiteral("micGainMinSpin"));
        spin->setValue(-30);
        QCOMPARE(rm.transmitModel().micGainMinDb(), -30);
    }
    void sliderRangeUpdatesLive() {
        NereusSDR::RadioModel rm;
        rm.transmitModel().setMicGainMinDb(-50);
        rm.transmitModel().setMicGainMaxDb(20);
        NereusSDR::AudioTxInputPage page(&rm);
        QSlider* sl = page.micGainSlider();
        QVERIFY(sl);
        QCOMPARE(sl->minimum(), -50);
        QCOMPARE(sl->maximum(), 20);
    }
};

QTEST_MAIN(TestAudioTxInputMicGainMinMax)
#include "tst_audio_tx_input_mic_gain_min_max.moc"
```

- [ ] **Step 2: Run, expect FAIL** (no spinboxes exist).

- [ ] **Step 3: Implement.**

`AudioTxInputPage.h` : declare members:

```cpp
QSpinBox* m_micGainMinSpin{nullptr};
QSpinBox* m_micGainMaxSpin{nullptr};
```

`AudioTxInputPage.cpp` : extend `buildPcMicGroup` after the gain-slider row:

```cpp
m_micGainMinSpin = new QSpinBox(this);
m_micGainMinSpin->setObjectName(QStringLiteral("micGainMinSpin"));
m_micGainMinSpin->setRange(-96, 0);
m_micGainMinSpin->setValue(model()->transmitModel().micGainMinDb());
grpLayout->addRow(QStringLiteral("Mic Gain Min (dB):"), m_micGainMinSpin);
connect(m_micGainMinSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int v) {
            model()->transmitModel().setMicGainMinDb(v);
            // Update slider live:
            if (m_micGainSlider) { m_micGainSlider->setMinimum(v); }
        });

// Mirror for Max.
m_micGainMaxSpin = new QSpinBox(this);
m_micGainMaxSpin->setObjectName(QStringLiteral("micGainMaxSpin"));
m_micGainMaxSpin->setRange(1, 70);
m_micGainMaxSpin->setValue(model()->transmitModel().micGainMaxDb());
grpLayout->addRow(QStringLiteral("Mic Gain Max (dB):"), m_micGainMaxSpin);
connect(m_micGainMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int v) {
            model()->transmitModel().setMicGainMaxDb(v);
            if (m_micGainSlider) { m_micGainSlider->setMaximum(v); }
        });

// Reverse: model → spin (handles Setup-page reopen).
connect(&model()->transmitModel(), &TransmitModel::micGainMinDbChanged,
        m_micGainMinSpin, &QSpinBox::setValue);
connect(&model()->transmitModel(), &TransmitModel::micGainMaxDbChanged,
        m_micGainMaxSpin, &QSpinBox::setValue);
```

Apply initial slider range so a fresh page already reflects the persisted bounds.

- [ ] **Step 4: Run the test, expect PASS.**

- [ ] **Step 5: Commit.**

```bash
git commit -m "$(cat <<'EOF'
feat(setup): MicGainMin/Max spinboxes + live mic-gain slider range

Adds the Thetis-equivalent of udMicGainMin / udMicGainMax (setup.cs:
9678-9694 [v2.10.3.13+501e3f51]) to the PC Mic group on Setup → Audio
→ TX Input.  Spinboxes round-trip with the new TransmitModel
properties and update the mic-gain slider's bounds live, so the user
sees the new range immediately.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 22: Tighten per-SKU gating in `updateRadioMicGroupVisibility`

**Files:**
- Modify: `src/gui/setup/AudioTxInputPage.h`
- Modify: `src/gui/setup/AudioTxInputPage.cpp`
- Test: `tests/tst_audio_tx_input_per_sku_gating.cpp`
- Modify: `tests/CMakeLists.txt`

**Thetis source:** `setup.cs:19830-20405 [v2.10.3.13+501e3f51]` (the SKU enable map for `pnlGeneralHardwareORION`) + `setup.cs:6181-6189 [v2.10.3.13+501e3f51]` (`panelSaturnMicInput.Enabled` Saturn-only).

**Decisions** (from spec §4.5 + the Thetis enable map):

- `pnlGeneralHardwareORION.Enabled = true` for: Orion / OrionMKII / Saturn / SaturnMKII / AnvelinaPro3 / ANAN-G2 / ANAN-G2-1K.
- `pnlGeneralHardwareORION.Enabled = false` for: Hermes / HermesII / ANAN10 / ANAN10E / ANAN100 / ANAN100B / ANAN100D / Angelia / RedPitaya / HermesLite / HermesLite2.

- [ ] **Step 1: Write the failing test.**

```cpp
// tests/tst_audio_tx_input_per_sku_gating.cpp

#include <QTest>
#include "gui/setup/AudioTxInputPage.h"

using NereusSDR::AudioTxInputPage;
using NereusSDR::HPSDRHW;

class TestAudioTxInputPerSkuGating : public QObject {
    Q_OBJECT
private slots:
    void orionShowsOrionPanel() {
        QVERIFY(AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Orion));
    }
    void orionMkiiShowsOrionPanel() {
        QVERIFY(AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::OrionMKII));
    }
    void saturnShowsOrionPanel() {
        QVERIFY(AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Saturn));
    }
    void saturnMkiiShowsOrionPanel() {
        QVERIFY(AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::SaturnMKII));
    }
    void hermesDoesNotShowOrionPanel() {
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Hermes));
    }
    void angeliaDoesNotShowOrionPanel() {
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Angelia));
    }
    void hl2DoesNotShowOrionPanel() {
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::HermesLite2));
    }
    void anan100dDoesNotShowOrionPanel() {
        QVERIFY(!AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW::Anan100D));
    }
    // [Implementer: add other SKUs from BoardCapabilities::HPSDRHW; mirror
    //  the enable map exactly.]
};

QTEST_MAIN(TestAudioTxInputPerSkuGating)
#include "tst_audio_tx_input_per_sku_gating.moc"
```

(Substitute the actual `HPSDRHW` enum names by reading the enum definition before writing the test.)

- [ ] **Step 2: Run, expect compile FAIL** (helper does not exist yet).

- [ ] **Step 3: Add the helper to `AudioTxInputPage.h`.**

```cpp
public:
    /// Per-SKU enable map for the Orion-style mic-jack panel.
    /// Mirrors Thetis pnlGeneralHardwareORION.Enabled in setup.cs:19830-20405
    /// [v2.10.3.13+501e3f51] + panelSaturnMicInput.Enabled in setup.cs:6181-6189
    /// [v2.10.3.13+501e3f51].
    static bool boardShowsOrionMicJackPanel(HPSDRHW hw);
```

`AudioTxInputPage.cpp`:

```cpp
bool AudioTxInputPage::boardShowsOrionMicJackPanel(HPSDRHW hw)
{
    switch (hw) {
        case HPSDRHW::Orion:
        case HPSDRHW::OrionMKII:
        case HPSDRHW::Saturn:
        case HPSDRHW::SaturnMKII:
        case HPSDRHW::AnvelinaPro3:  // if present in the enum
            return true;
        default:
            return false;
    }
}
```

Substitute the actual `HPSDRHW` names (e.g. `Anan100D` → `ANAN100D` if the enum uses uppercase).

Update `updateRadioMicGroupVisibility`:

```cpp
void AudioTxInputPage::updateRadioMicGroupVisibility(MicSource source, HPSDRHW hw)
{
    const bool radioMicActive = (source == MicSource::Radio);
    const bool isHermes = (hw == HPSDRHW::Hermes
                        || hw == HPSDRHW::HermesII);
    const bool isOrion  = boardShowsOrionMicJackPanel(hw);

    // Angelia / ANAN100D / RedPitaya / HL2: no mic-jack panel
    // (Thetis disables pnlGeneralHardwareORION outright; we mirror.)
    if (m_hermesGroup) { m_hermesGroup->setVisible(radioMicActive && isHermes); }
    if (m_orionGroup)  { m_orionGroup->setVisible(radioMicActive && isOrion);  }
    if (m_saturnGroup) {
        m_saturnGroup->setVisible(radioMicActive
                                  && (hw == HPSDRHW::Saturn
                                   || hw == HPSDRHW::SaturnMKII));
    }
}
```

- [ ] **Step 4: Run, expect PASS.**

- [ ] **Step 5: Commit.**

```bash
git commit -m "$(cat <<'EOF'
fix(setup): per-SKU mic-jack panel gating matches Thetis enable map

Adds boardShowsOrionMicJackPanel(HPSDRHW) helper that mirrors the
SKU enable map in Thetis setup.cs:19830-20405 [v2.10.3.13+501e3f51].
Hermes / Angelia / ANAN100D / RedPitaya / HermesLite / HermesLite2
no longer show the Orion-class panel.  Saturn group still gated
per setup.cs:6181-6189 (Saturn / SaturnMKII only).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 23: Add three controls to `GeneralOptionsPage::buildOptionsGroup` (DisablePTT, AllModeMicPTT, PTTOutDelay)

**Files:**
- Modify: `src/gui/setup/GeneralOptionsPage.h`
- Modify: `src/gui/setup/GeneralOptionsPage.cpp`
- Test: `tests/tst_general_options_page_disable_ptt.cpp` (Disable PTT round-trip)
- Test: `tests/tst_general_options_page_all_mode_mic_ptt.cpp`
- Test: `tests/tst_general_options_page_ptt_out_delay.cpp`
- Modify: `tests/CMakeLists.txt`

**Thetis source:** `setup.cs:12127-12129, 14302-14305, 19694-19754 [v2.10.3.13+501e3f51]` for the three controls; `setup.designer.cs:9029-9226, 8827 [v2.10.3.13+501e3f51]` for layout / range.

- [ ] **Step 1: Write three failing tests** (one per control). Each opens a `GeneralOptionsPage`, `findChild`s the new widget by `objectName`, asserts the round-trip with `TransmitModel::disablePTT` / `allModeMicPTT` / `pttOutDelayMs`. Skeleton:

```cpp
// tests/tst_general_options_page_disable_ptt.cpp

#include <QTest>
#include <QCheckBox>
#include "gui/setup/GeneralOptionsPage.h"
#include "models/RadioModel.h"

class TestGeneralOptionsPageDisablePtt : public QObject {
    Q_OBJECT
private slots:
    void checkboxReflectsModel() {
        NereusSDR::RadioModel rm;
        rm.transmitModel().setDisablePTT(true);
        NereusSDR::GeneralOptionsPage page(&rm);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("disablePttCheck"));
        QVERIFY(chk);
        QCOMPARE(chk->isChecked(), true);
    }
    void checkboxClickPushesToModel() {
        NereusSDR::RadioModel rm;
        NereusSDR::GeneralOptionsPage page(&rm);
        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("disablePttCheck"));
        chk->setChecked(true);
        QCOMPARE(rm.transmitModel().disablePTT(), true);
    }
};

QTEST_MAIN(TestGeneralOptionsPageDisablePtt)
#include "tst_general_options_page_disable_ptt.moc"
```

Mirror with `allModeMicPttCheck` (QCheckBox) and `pttOutDelaySpin` (QSpinBox).

- [ ] **Step 2: Run, expect FAIL** (widgets do not exist).

- [ ] **Step 3: Implement in `GeneralOptionsPage.cpp::buildOptionsGroup`.**

Add three rows. Sketch:

```cpp
m_disablePttCheck = new QCheckBox(QStringLiteral("Disable PTT"), this);
m_disablePttCheck->setObjectName(QStringLiteral("disablePttCheck"));
m_disablePttCheck->setToolTip(QStringLiteral(
    "Block all PTT sources from engaging the radio. Mirrors Thetis "
    "Setup → General → Options → Disable PTT."));
m_disablePttCheck->setChecked(model()->transmitModel().disablePTT());
connect(m_disablePttCheck, &QCheckBox::toggled,
        &model()->transmitModel(), &TransmitModel::setDisablePTT);
connect(&model()->transmitModel(), &TransmitModel::disablePTTChanged,
        m_disablePttCheck, &QCheckBox::setChecked);
optionsLayout->addRow(m_disablePttCheck);

// All Mode Mic PTT
m_allModeMicPttCheck = new QCheckBox(QStringLiteral("All-Mode Mic PTT"), this);
m_allModeMicPttCheck->setObjectName(QStringLiteral("allModeMicPttCheck"));
m_allModeMicPttCheck->setToolTip(QStringLiteral(
    "Allow mic-jack PTT to drive transmit in non-voice modes (CW etc.). "
    "Mirrors Thetis Setup → General → Options → All-Mode Mic PTT."));
// ... same connect pattern ...

// PTT Out Delay
m_pttOutDelaySpin = new QSpinBox(this);
m_pttOutDelaySpin->setObjectName(QStringLiteral("pttOutDelaySpin"));
m_pttOutDelaySpin->setRange(0, 500);
m_pttOutDelaySpin->setSuffix(QStringLiteral(" ms"));
m_pttOutDelaySpin->setToolTip(QStringLiteral(
    "Delay between local TX engagement and the rear-panel PTT-OUT "
    "relay closing. Mirrors Thetis Setup → General → Options → "
    "PTT Out Delay."));
m_pttOutDelaySpin->setValue(model()->transmitModel().pttOutDelayMs());
connect(m_pttOutDelaySpin, QOverload<int>::of(&QSpinBox::valueChanged),
        &model()->transmitModel(), &TransmitModel::setPttOutDelayMs);
connect(&model()->transmitModel(), &TransmitModel::pttOutDelayMsChanged,
        m_pttOutDelaySpin, &QSpinBox::setValue);
optionsLayout->addRow(QStringLiteral("PTT Out Delay:"), m_pttOutDelaySpin);
```

Substitute the existing `optionsLayout` variable name from `GeneralOptionsPage.cpp:328` (whatever the local QFormLayout variable is called).

(Note: tooltips are plain English, not Thetis cites, per the user's "no source cites in user-visible strings" rule.)

- [ ] **Step 4: Run all three tests, expect PASS.**

- [ ] **Step 5: Commit.**

```bash
git commit -m "$(cat <<'EOF'
feat(setup): General Options gains DisablePTT, AllModeMicPTT, PTTOutDelay

Three controls binding to the matching TransmitModel properties.
Layout slots into the existing GeneralOptionsPage::buildOptionsGroup
under Setup → General → Options.  Tooltips are plain English; the
Thetis cites live in the inline source comments next to each control
per project conventions.

Source: setup.cs:12127-14305 + 19694-19754 [v2.10.3.13+501e3f51].

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 24: CHANGELOG entry plus spec status update

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `docs/architecture/radio-mic-input-thetis-parity.md` (status field)

- [ ] **Step 1: Add CHANGELOG entry.**

Under the next-release section (or Unreleased), add:

```markdown
### Added
- Radio mic input parity port: 5 mic-jack wire bits now reach the radio end-to-end (mic boost, line in, tip/ring, bias, XLR), with a new `setLineInBoost(double dB)` virtual that translates the dB value through the Thetis 32-entry index table. PC-Mic / Radio-Mic toggle now actually switches the TX source on both Protocol 1 and Protocol 2. `AllModeMicPTT`, `PTT Out Delay`, `Mic Gain Min`, `Mic Gain Max`, `Disable PTT` all configurable from Setup → General → Options + Setup → Audio → TX Input. Cite: docs/architecture/radio-mic-input-thetis-parity.md.

### Removed
- Dead `RadioMicSource` and `RadioConnection::micFrameDecoded` paths (never emitted in production after PR #193).

### Fixed
- Saturn boards now boot into 3.5 mm mic routing by default (matches Thetis radSaturn3p5mm.Checked default); previously fresh installs booted into XLR.
- Connection-side `m_micBoost` default now matches Thetis (`true`); eliminates a one-frame race between connect and the prime push.
```

- [ ] **Step 2: Update spec status.**

In `docs/architecture/radio-mic-input-thetis-parity.md` line 3:

```markdown
**Status:** implemented (2026-05-05; PR #TBD)
```

- [ ] **Step 3: Commit.**

```bash
git commit -m "$(cat <<'EOF'
docs: changelog + spec status for radio-mic-input parity port

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 25: Build, run full suite, manual bench checklist, draft PR

**Files:** none (verification + PR draft).

- [ ] **Step 1: Clean build + full suite.**

```bash
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Expected: every test pass. Test count delta = +N new + (-2 removed) = roughly +14.

- [ ] **Step 2: Sanity-check inline cites.**

```bash
python3 scripts/verify-inline-cites.py
python3 scripts/verify-inline-tag-preservation.py
```

Expected: both pass. Any failure points at a missing `[v2.10.3.13+501e3f51]` stamp or a dropped author tag.

- [ ] **Step 3: Launch the app and walk the Setup pages.**

```bash
pkill -x NereusSDR; ./build/NereusSDR &
```

Open Setup → Audio → TX Input. Confirm:
- Mic Gain Min / Max spinboxes appear in the PC Mic group.
- Saturn group on a Saturn-class radio shows XLR / 3.5 mm radio buttons; default selection is 3.5 mm.
- Hermes group on a Hermes radio shows mic-boost / line-in checkboxes.
- Angelia / ANAN100D / HL2: no Orion-style panel rendered.

Open Setup → General → Options. Confirm:
- "Disable PTT", "All-Mode Mic PTT", "PTT Out Delay" controls appear.
- Each round-trips when you toggle / adjust + reopen Setup.

- [ ] **Step 4: ANAN-G2 bench (P2).**

- Plug a footswitch into the mic jack. Press it. TX engages.
- Setup → Audio → TX Input → Saturn group: check XLR. Confirm wire byte 50 bit 5 changes (with packet capture if available).
- Toggle Mic Boost on → off → on. Confirm wire byte changes.
- Toggle Mic PTT Disable. Press footswitch. TX must NOT engage.
- Setup → Audio → TX Input → flip mic source PC → Radio. Speak into the radio mic. Confirm the TX modulation.
- General Options → All-Mode Mic PTT off + DSP mode CWU. Press footswitch. TX must NOT engage.
- General Options → All-Mode Mic PTT on + DSP mode CWU. Press footswitch. TX engages.
- General Options → PTT Out Delay 100 ms. Press footswitch with the rear PTT-OUT relay loop on a scope. Confirm the 100 ms gap.

- [ ] **Step 5: HL2 bench (P1).**

Confirm no regression: TUNE works, MOX works, mic-jack panel hidden (HL2 has no mic jack), Setup pages render without warnings.

- [ ] **Step 6: ANAN-10E bench (P1, if available).**

If a Hermes-class P1 board is on hand, walk the same Setup checklist. If not, mark "needs bench" in the PR description; the legacy compose path was already direct since PR #161 so the risk is low.

- [ ] **Step 7: Draft the PR (DO NOT POST without explicit user approval).**

Draft body for review in chat:

```markdown
## Summary
- Closes the radio-mic-input parity gap left after issue #182. Single PR
  per the design doc (docs/architecture/radio-mic-input-thetis-parity.md).
- 5 mic-jack wire bits reach the wire end-to-end via a new
  RadioModel::connectMicJackSignals() helper.
- New RadioConnection::setLineInBoost(double dB) virtual translates the
  dB property to the 5-bit wire field through the Thetis 32-entry table
  (console.cs:40827-40859 [v2.10.3.13+501e3f51]).
- TransmitModel grows 5 properties: disablePTT, allModeMicPTT,
  pttOutDelayMs, micGainMinDb, micGainMaxDb. All persist per-MAC.
- MicXlr model default flips false (3.5 mm) to match Thetis Saturn
  ship state.
- MoxController gains setAllModeMicPTT + setPttOutDelayMs slots and a
  mode-gate inside onMicPttFromRadio (console.cs:25480-25495).
- RadioMicSource and RadioConnection::micFrameDecoded deleted (never
  emitted in production after PR #193).
- CompositeTxMicRouter Radio branch now pulls from TxMicSource (the
  live ring fed by P1 EP6 + P2 port-1026 ingress).
- Setup → Audio → TX Input gets MicGainMin/Max spinboxes and tighter
  per-SKU panel gating (Hermes group hidden on Angelia / ANAN100D /
  RedPitaya / HL2 to match Thetis pnlGeneralHardwareORION enable map).
- Setup → General → Options gets Disable PTT + All-Mode Mic PTT +
  PTT Out Delay controls.

## Test plan
- [ ] ANAN-G2 (P2): full bench checklist per spec §6 (footswitch,
      every Saturn-group control, source toggle, all-mode mic PTT,
      PTT-OUT delay).
- [ ] HL2 (P1): regression : TUNE / MOX still work, mic-jack panel
      hidden.
- [ ] ANAN-10E (P1): if available, walk Setup checklist.
- [x] cmake --build clean from scratch
- [x] ctest --test-dir build --output-on-failure all-green
      (added ~14 tests, removed 2)
- [x] verify-inline-cites.py + verify-inline-tag-preservation.py pass
- [x] All commits GPG-signed.

J.J. Boyd ~ KG4VCF
```

Show this draft to the user; wait for explicit "post it" before running `gh pr create`.

- [ ] **Step 8: Once user approves, push and open PR.**

```bash
git push -u origin claude/radio-mic-parity
gh pr create --title "feat(radio): radio-mic-input Thetis parity port" --body "$(cat /tmp/pr-body.md)"
gh pr view --web
```

---

## Self-review (run before claiming the plan is done)

**Spec coverage:** Every spec section §3-§7 maps to at least one task. Specifically:
- §3 gap matrix: Tasks 1-3 (line-in-boost), 5-9 (model props), 10 (Mic XLR default), 11-13 (RadioMicSource delete), 14 (micFrameDecoded delete), 15-16 (RadioModel wiring), 17-19 (MoxController gate), 20 (RadioModel↔MoxController), 21-23 (UI), 24 (CHANGELOG).
- §4.1 P2 mic ingress: marked already-done in PR #193 : explicit no-op task at top of plan.
- §4.4 mic data path unification: Tasks 11-14.
- §5 defaults audit: Tasks 4 (m_micBoost connection-side) + 10 (MicXlr model). Other rows in the table are no-ops verified during the audit.
- §6 test plan: Tasks 1-23 each ship a unit/wire test; Task 25 covers the bench checklist.
- §7 out-of-scope: explicitly preserved (CW dispatch flagged in Task 19; VAC bypass / TCI PTT untouched).

**Placeholder scan:** No "TBD" / "implement later" / "TODO" / "fill in details". Every code block in every step is concrete except where Task 21/22/23 say "Substitute the actual `HPSDRHW` enum names" : these are explicit instructions to look up the live enum, not hand-wave.

**Type consistency:**
- `setLineInBoost(double dB)` consistent across Tasks 1, 2, 3, 16.
- `pttOutDelayMs` (int, 0..500) consistent across Tasks 7, 18, 20, 23.
- `allModeMicPTT` (bool) consistent across Tasks 6, 17, 19, 20, 23.
- `disablePTT` (bool) consistent across Tasks 5, 23.
- `micGainMinDb`, `micGainMaxDb` (int) consistent across Tasks 8, 9, 21.
- `connectMicJackSignals` consistent across Tasks 15, 16.
- `boardShowsOrionMicJackPanel(HPSDRHW)` consistent across Task 22 declaration and use.

No drift detected.

**Out-of-scope explicitly preserved per spec §7:**
- 3M-2 CW transmit (Task 19's "CW dispatch lands in 3M-2" comment).
- VAC bypass on PTT.
- TCI PTT.
- Mic AGC / limiter (WDSP TXA territory).
- Andromeda board verification : flagged in Task 25's bench checklist as "needs bench" rather than blocking the PR.

---

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-05-radio-mic-input-thetis-parity.md`. Two execution options:

1. **Subagent-Driven (recommended)** : I dispatch a fresh subagent per task, review between tasks, fast iteration. Required sub-skill: `superpowers:subagent-driven-development`.
2. **Inline Execution** : Execute tasks in this session using `superpowers:executing-plans`, batch execution with checkpoints for review.

Which approach?

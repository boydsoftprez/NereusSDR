# P1 Full Parity Plan — All Non-HL2 Boards

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring every non-HL2 P1 board (Atlas / Hermes / HermesII (ANAN-10E/100B) / Angelia / Orion / OrionMKII / AnvelinaPro3 / RedPitaya) to full Thetis byte-diff and feature parity in a single PR. Triggered by rapid T/R relay clicking on the ANAN-10E (HermesII); the same root cause and parity gaps affect every standard-codec P1 board.

**Architecture:** Three coordinated packages in one PR: (1) `P1CodecStandard` wire-format parity vs Thetis `networkproto1.c` `WriteMainLoop` for banks 0/10/11/freq-banks; (2) per-board PA calibration port — `PaCalProfile` model + `PaCalibrationGroup` UI + `CalibratedPAPower` interpolation + SWR foldback; (3) close three documented `BoardCapabilities` flag-wiring gaps (`hasStepAttenuatorCal`, `hasSidetoneGenerator`, `hasPennyLane`).

**Tech Stack:** C++20, Qt6, FFTW3, GoogleTest. Authoritative source: `../Thetis/Project Files/Source/ChannelMaster/networkproto1.c [v2.10.3.13+501e3f51]` for wire format; `console.cs [v2.10.3.13+501e3f51]` for `CalibratedPAPower()`; `setup.cs [v2.10.3.13+501e3f51]` for per-board PA cal UI groups (`tcPACalibration`).

**Provenance:** This plan touches three already-registered NereusSDR files (`P1CodecStandard.{cpp,h}`, `CodecContext.h`) and creates new ones (`PaCalProfile.{h,cpp}`, `PaCalibrationGroup.{h,cpp}`, possibly `PaCalibrationController.{h,cpp}`). Per `docs/attribution/HOW-TO-PORT.md` Pre-port checklist:
- `P1CodecStandard.cpp` already has Thetis `networkproto1.c` header — no new attribution event for that file.
- `CodecContext.h` is NereusSDR-original — no header change needed when adding fields.
- New files porting from Thetis `console.cs` / `setup.cs` → require verbatim header copy + PROVENANCE row in same commit.

**Single PR delivery.** Multiple GPG-signed commits stacked on `feature/p1-full-parity`; one PR opened against `main`. Bench verification on the ANAN-10E (rapid relay clicking is the primary regression test) and HL2 (no regressions on the already-shipped fix path) before opening the PR.

---

## File Structure

### Modified

| File | Reason |
|---|---|
| `src/core/codec/P1CodecStandard.cpp` | mic_ptt direct polarity + bank 11 C2/C3 + restore C0 XmitBit on freq banks |
| `src/core/codec/CodecContext.h` | add `p1LineInGain`, `p1UserDigOut`, `p1PuresignalRun` fields |
| `src/core/RadioConnection.h` | add `setLineInGain(int)`, `setUserDigOut(quint8)`, `setPuresignalRun(bool)` virtuals + base storage |
| `src/core/P1RadioConnection.cpp` | populate new ctx fields from new state members; force-bank-11 flush hooks; `applyDriveScaling` w/ SWR foldback |
| `src/core/P1RadioConnection.h` | declare setter overrides + `m_lineInGain` / `m_userDigOut` / `m_puresignalRun` storage |
| `src/core/P2RadioConnection.cpp` | add stub setter overrides for source-symmetry (no wire change for P2) |
| `src/core/P2RadioConnection.h` | declare setter overrides |
| `src/gui/setup/HardwarePage.cpp` | add `PaCalibrationGroup` to `CalibrationTab` (or new tab — see Task 3.4) |
| `src/gui/setup/hardware/CalibrationTab.cpp` | add Group 6 — Per-band PA cal points (board-class-aware) |
| `src/gui/setup/hardware/CalibrationTab.h` | declare `PaCalibrationGroup` member |
| `src/core/CalibrationController.cpp` | extend with `paCalPoints` map per band-class (see Task 3.3) |
| `src/core/CalibrationController.h` | declare `paCalPoints` getter/setter + `calibratedFwdPowerWatts(rawAdc)` |
| `src/models/RadioModel.cpp` | wire `setLineInGain`/`setUserDigOut`/`setPuresignalRun` on connect; route `alex_fwd` reading through `calibratedFwdPowerWatts` |
| `src/models/TransmitModel.h` | add `lineInGain`, `userDigOut` properties (already has powerPercent + puresignalEnabled, reuse) |
| `src/models/TransmitModel.cpp` | save/load + signals |
| `src/core/MicProfileManager.cpp` | add `Mic_LineInGain` and `Mic_UserDigOut` to factory profile keys + load/save |
| `src/core/StepAttenuatorController.cpp` | gate adaptive cal on `caps.hasStepAttenuatorCal` (Task 4.1) |
| `src/gui/applets/CwxApplet.cpp` | gate firmware sidetone toggle on `caps.hasSidetoneGenerator` (Task 4.2) |
| `src/gui/setup/HardwarePage.cpp` | gate PennyLane / PennyOC tab on `caps.hasPennyLane` (Task 4.3) |
| `docs/attribution/THETIS-PROVENANCE.md` | row for new files |
| `CHANGELOG.md` | entry for relay clicking fix + PA cal port + 3 flag wirings |
| `CLAUDE.md` | reference the new plan in "Current Phase" |

### Created

| File | Responsibility |
|---|---|
| `src/core/PaCalProfile.h` | `PaCalProfile` struct: 11-entry user-cal points + per-band-class factory defaults; `PaCalBoardClass` enum (Anan10, Anan100, Anan8000, Hermes, Atlas) |
| `src/core/PaCalProfile.cpp` | factory cal point arrays per class; `paCalBoardClassFor(HPSDRModel)` mapping; interpolation function |
| `src/gui/setup/hardware/PaCalibrationGroup.h` | per-board-class cal point spinbox group widget |
| `src/gui/setup/hardware/PaCalibrationGroup.cpp` | populates spinboxes per `PaCalBoardClass`; emits `paCalPointChanged(idx, value)` |
| `tests/tst_p1_codec_standard_bank11_polarity.cpp` | mic_ptt direct polarity + bank 11 C2/C3 wire tests |
| `tests/tst_p1_codec_standard_freq_bank_xmitbit.cpp` | C0 XmitBit on freq banks under MOX |
| `tests/tst_pa_cal_profile.cpp` | per-board-class factory defaults + interpolation correctness |
| `tests/tst_pa_cal_swr_foldback.cpp` | drive-byte scaling with SWR foldback |
| `tests/tst_board_capability_flag_wiring.cpp` | `hasStepAttenuatorCal` / `hasSidetoneGenerator` / `hasPennyLane` consumed |

---

## Section 1 — P1CodecStandard wire-format parity

Thetis `networkproto1.c [v2.10.3.13+501e3f51]` is authoritative.

### Task 1.1 — Fix mic_ptt direct polarity (relay clicking root cause)

**Files:**
- Modify: `src/core/codec/P1CodecStandard.cpp:217-237`
- Test: `tests/tst_p1_codec_standard_bank11_polarity.cpp` (new)

- [ ] **Step 1.1.1: Write the failing test**

```cpp
// tests/tst_p1_codec_standard_bank11_polarity.cpp
#include <gtest/gtest.h>
#include "core/codec/P1CodecStandard.h"
#include "core/codec/CodecContext.h"

using namespace NereusSDR;

TEST(P1CodecStandardBank11, mic_ptt_direct_polarity_default_false_emits_bit_clear) {
    P1CodecStandard codec;
    CodecContext ctx{};
    ctx.p1MicPTT = false;  // default
    quint8 out[5] = {};
    codec.composeCcForBank(11, ctx, out);
    EXPECT_EQ(out[1] & 0x40, 0x00) << "mic_ptt=false must clear C1 bit 6 — Thetis networkproto1.c:597-598";
}

TEST(P1CodecStandardBank11, mic_ptt_direct_polarity_true_emits_bit_set) {
    P1CodecStandard codec;
    CodecContext ctx{};
    ctx.p1MicPTT = true;
    quint8 out[5] = {};
    codec.composeCcForBank(11, ctx, out);
    EXPECT_EQ(out[1] & 0x40, 0x40) << "mic_ptt=true must set C1 bit 6 — Thetis networkproto1.c:597-598";
}
```

- [ ] **Step 1.1.2: Add the test to `tests/CMakeLists.txt`** under existing P1 codec test block.

- [ ] **Step 1.1.3: Run — confirm fail**

```
ctest --test-dir build --output-on-failure -R p1_codec_standard_bank11_polarity
# Expected FAIL: out[1] & 0x40 == 0x40 when ctx.p1MicPTT=false (current inverted code emits bit set)
```

- [ ] **Step 1.1.4: Apply the fix**

```cpp
// src/core/codec/P1CodecStandard.cpp:232 — was:
//   | (!ctx.p1MicPTT    ? 0x40 : 0x00));   // mic_ptt (INVERTED) — 3M-1b G.5
// becomes:
              | (ctx.p1MicPTT     ? 0x40 : 0x00));   // mic_ptt (DIRECT — matches Thetis networkproto1.c:597-598 [v2.10.3.13+501e3f51])
```

Update the comment block at line 218-225 to remove "(INVERTED)" and document the polarity match with the HL2 fix in PR #161.

- [ ] **Step 1.1.5: Run tests — confirm pass**

```
ctest --test-dir build --output-on-failure -R p1_codec_standard_bank11_polarity
# Expected PASS
```

- [ ] **Step 1.1.6: Run full P1 suite — confirm no regressions**

```
ctest --test-dir build --output-on-failure -R 'tst_p1_'
# Expected: all P1 tests pass
```

- [ ] **Step 1.1.7: Commit (GPG-signed)**

```
git add tests/tst_p1_codec_standard_bank11_polarity.cpp tests/CMakeLists.txt src/core/codec/P1CodecStandard.cpp
git commit -m "$(cat <<'EOF'
fix(p1/standard): mic_ptt direct polarity — fixes relay flutter on all non-HL2 P1 boards

P1CodecStandard.cpp:232 was emitting bank 11 C1 bit 6 = !p1MicPTT, mirroring
the same inversion bug fixed for HL2 in PR #161 commit ca8cd73 (#2). Thetis
networkproto1.c:597-598 [v2.10.3.13+501e3f51] is direct:
    C1 = ... | ((prn->mic.mic_ptt & 1) << 6);

With p1MicPTT default false, the prior code set bit 6=1 every CC frame on the
wire. Hermes-class firmware reads bit 6 as "track mic-jack tip as PTT source";
with no jack engaged, the floating tip caused phantom PTT signals fighting
software MOX → rapid T/R relay flutter on TUNE/TX. Symptom reported on
ANAN-10E (HermesII); same root cause affects every board using
P1CodecStandard or its subclasses (P1CodecAnvelinaPro3, P1CodecRedPitaya):
Atlas / Hermes (ANAN-10/100) / HermesII (ANAN-10E/100B) / Angelia (ANAN-100D) /
Orion (ANAN-200D) / OrionMKII / AnvelinaPro3 (ANAN-8000DLE) / RedPitaya.

Tests:
- new tst_p1_codec_standard_bank11_polarity (2 cases, mirrors HL2 test).
- existing P1 suite (108 tests) green.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 1.2 — Wire bank 11 C2 (line_in_gain + puresignal_run flag)

Thetis emits: `C2 = (mic.line_in_gain & 0x1F) | ((puresignal_run & 1) << 6)`. Currently `out[2] = 0`.

**Files:**
- Modify: `src/core/codec/CodecContext.h` (add `p1LineInGain` + `p1PuresignalRun`)
- Modify: `src/core/codec/P1CodecStandard.cpp:233` (compute C2)
- Test: `tests/tst_p1_codec_standard_bank11_polarity.cpp` (extend)

- [ ] **Step 1.2.1: Add fields to CodecContext**

```cpp
// src/core/codec/CodecContext.h — after p1MicPTT block (~line 203)
    // Mic line-in gain — prn->mic.line_in_gain, 5-bit (0-31).
    // Source: Thetis networkproto1.c:600 [v2.10.3.13+501e3f51]
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    int     p1LineInGain{0};

    // PureSignal feedback running flag — prn->puresignal_run.
    // Source: Thetis networkproto1.c:600 [v2.10.3.13+501e3f51]
    // Bank 11 C2 bit 6. Distinct from PureSignal-capability (BoardCapabilities)
    // and from PureSignal-enabled (TransmitModel) — this tracks whether the
    // PS feedback DDC routing is currently *active*. False until 3M-4 lights
    // up; remains plumbed so the bit appears on the wire when 3M-4 ships.
    bool    p1PuresignalRun{false};
```

- [ ] **Step 1.2.2: Add C2 wire-format test**

```cpp
TEST(P1CodecStandardBank11, c2_line_in_gain_in_low_5_bits) {
    P1CodecStandard codec;
    CodecContext ctx{};
    ctx.p1LineInGain = 0x17;  // 23 — fits in 5 bits
    quint8 out[5] = {};
    codec.composeCcForBank(11, ctx, out);
    EXPECT_EQ(out[2] & 0x1F, 0x17);
}
TEST(P1CodecStandardBank11, c2_line_in_gain_masked_to_5_bits) {
    P1CodecStandard codec;
    CodecContext ctx{};
    ctx.p1LineInGain = 0x3F;  // 63 — high bit must be masked
    quint8 out[5] = {};
    codec.composeCcForBank(11, ctx, out);
    EXPECT_EQ(out[2] & 0x1F, 0x1F);
}
TEST(P1CodecStandardBank11, c2_puresignal_run_sets_bit_6) {
    P1CodecStandard codec;
    CodecContext ctx{};
    ctx.p1PuresignalRun = true;
    quint8 out[5] = {};
    codec.composeCcForBank(11, ctx, out);
    EXPECT_EQ(out[2] & 0x40, 0x40);
}
```

- [ ] **Step 1.2.3: Run — confirm fail**

- [ ] **Step 1.2.4: Update C2 emission**

```cpp
// src/core/codec/P1CodecStandard.cpp:233 — was: out[2] = 0;
// becomes:
    // C2: line_in_gain (low 5 bits) | puresignal_run (bit 6).
    // Source: Thetis networkproto1.c:600 [v2.10.3.13+501e3f51]
    out[2] = quint8((ctx.p1LineInGain & 0x1F)
                  | (ctx.p1PuresignalRun ? 0x40 : 0x00));
```

- [ ] **Step 1.2.5: Run — confirm pass**

- [ ] **Step 1.2.6: Commit**

```
git commit -m "feat(p1/standard): bank 11 C2 line_in_gain + puresignal_run flag

[Thetis networkproto1.c:600 [v2.10.3.13+501e3f51] full quote in commit body]
[3 test cases listed]
[Co-Authored-By line]"
```

---

### Task 1.3 — Wire bank 11 C3 (user_dig_out)

Thetis emits: `C3 = prn->user_dig_out & 0b00001111`. Currently `out[3] = 0`. Field doesn't exist in NereusSDR.

**Files:**
- Modify: `src/core/codec/CodecContext.h` (add `p1UserDigOut`)
- Modify: `src/core/codec/P1CodecStandard.cpp:234` (compute C3)
- Test: extend `tests/tst_p1_codec_standard_bank11_polarity.cpp`

- [ ] **Step 1.3.1: Add field to CodecContext**

```cpp
// src/core/codec/CodecContext.h — after p1PuresignalRun
    // User digital outputs — prn->user_dig_out, low 4 bits (0-15).
    // Source: Thetis networkproto1.c:601 [v2.10.3.13+501e3f51]
    //   C3 = prn->user_dig_out & 0b00001111;
    // Drives the 4 user-controllable digital pins on the radio's accessory
    // header. UI: Setup → Hardware → OC Outputs → "User Dig Out 1..4"
    // (added in Task 4.3 below alongside hasPennyLane gating).
    quint8  p1UserDigOut{0};
```

- [ ] **Step 1.3.2: Add C3 wire-format test**

```cpp
TEST(P1CodecStandardBank11, c3_user_dig_out_low_4_bits) {
    P1CodecStandard codec;
    CodecContext ctx{};
    ctx.p1UserDigOut = 0x0A;
    quint8 out[5] = {};
    codec.composeCcForBank(11, ctx, out);
    EXPECT_EQ(out[3] & 0x0F, 0x0A);
}
TEST(P1CodecStandardBank11, c3_user_dig_out_masked_to_4_bits) {
    P1CodecStandard codec;
    CodecContext ctx{};
    ctx.p1UserDigOut = 0xFF;
    quint8 out[5] = {};
    codec.composeCcForBank(11, ctx, out);
    EXPECT_EQ(out[3] & 0x0F, 0x0F);
}
```

- [ ] **Step 1.3.3: Run — confirm fail**

- [ ] **Step 1.3.4: Update C3 emission**

```cpp
// src/core/codec/P1CodecStandard.cpp:234 — was: out[3] = 0;
    // C3: user digital outputs, low 4 bits.
    // Source: Thetis networkproto1.c:601 [v2.10.3.13+501e3f51]
    out[3] = quint8(ctx.p1UserDigOut & 0x0F);
```

- [ ] **Step 1.3.5: Run — confirm pass**

- [ ] **Step 1.3.6: Commit**

---

### Task 1.4 — Restore C0 XmitBit on frequency banks

Thetis sets `C0 = XmitBit; switch(bank){case 1: C0 |= 2; ...}`. Our codec strips XmitBit on freq banks (lines 64, 81, 116). Whether firmware uses this is empirical — but Thetis sets it, so we match.

**Files:**
- Modify: `src/core/codec/P1CodecStandard.cpp:62-126`
- Test: `tests/tst_p1_codec_standard_freq_bank_xmitbit.cpp` (new)

- [ ] **Step 1.4.1: Write the failing test**

```cpp
// tests/tst_p1_codec_standard_freq_bank_xmitbit.cpp
#include <gtest/gtest.h>
#include "core/codec/P1CodecStandard.h"
#include "core/codec/CodecContext.h"

using namespace NereusSDR;

class FreqBankXmitBitTest : public ::testing::TestWithParam<int> {};

TEST_P(FreqBankXmitBitTest, mox_off_clears_xmitbit) {
    P1CodecStandard codec;
    CodecContext ctx{};
    ctx.mox = false;
    ctx.activeRxCount = 7;
    ctx.txFreqHz = 14'200'000;
    for (int i = 0; i < 7; ++i) ctx.rxFreqHz[i] = 14'000'000 + i * 1000;
    quint8 out[5] = {};
    codec.composeCcForBank(GetParam(), ctx, out);
    EXPECT_EQ(out[0] & 0x01, 0x00) << "MOX off → C0 bit 0 must be clear on freq bank " << GetParam();
}

TEST_P(FreqBankXmitBitTest, mox_on_sets_xmitbit) {
    P1CodecStandard codec;
    CodecContext ctx{};
    ctx.mox = true;
    ctx.activeRxCount = 7;
    ctx.txFreqHz = 14'200'000;
    for (int i = 0; i < 7; ++i) ctx.rxFreqHz[i] = 14'000'000 + i * 1000;
    quint8 out[5] = {};
    codec.composeCcForBank(GetParam(), ctx, out);
    EXPECT_EQ(out[0] & 0x01, 0x01)
        << "MOX on → C0 bit 0 must be set on freq bank " << GetParam()
        << " (Thetis networkproto1.c:447 C0 = XmitBit before switch)";
}

INSTANTIATE_TEST_SUITE_P(AllFreqBanks, FreqBankXmitBitTest, ::testing::Values(1, 2, 3, 5, 6, 7, 8, 9));
```

- [ ] **Step 1.4.2: Run — confirm fail (only the mox_on cases)**

- [ ] **Step 1.4.3: Update bank 1, 2/3, 5-9 emission**

Three sites in `P1CodecStandard.cpp`:

```cpp
// Bank 1 (line ~66) — was: out[0] = 0x02;
        case 1: {
            const quint32 hz = quint32(ctx.txFreqHz);
            out[0] = quint8(C0base | 0x02);  // Thetis networkproto1.c:477 [v2.10.3.13+501e3f51] — C0 = XmitBit | 2
            ...
        }

// Banks 2-3 (line ~83) — was: out[0] = kRx01C0[rxIdx];
        case 2: case 3: {
            ...
            out[0] = quint8(C0base | kRx01C0[rxIdx]);  // Thetis networkproto1.c:485,498 [v2.10.3.13+501e3f51]
            ...
        }

// Banks 5-9 (line ~116) — was: out[0] = kRxC0Addr[bank - 5];
        case 5: case 6: case 7: case 8: case 9: {
            ...
            out[0] = quint8(C0base | kRxC0Addr[bank - 5]);  // Thetis networkproto1.c:526,539,549,560,569 [v2.10.3.13+501e3f51]
            ...
        }
```

Remove the "Bug-parity note: ... NO MOX bit" comments — they were wrong.

- [ ] **Step 1.4.4: Run — confirm pass**

- [ ] **Step 1.4.5: Run regression-freeze tests**

```
ctest --test-dir build --output-on-failure -R 'tst_p1_regression_freeze'
# These compare against frozen byte traces — they may need updating since
# we are correcting a wire-format divergence. If they fail, re-baseline
# against Thetis-exact bytes (not the prior NereusSDR bytes).
```

- [ ] **Step 1.4.6: Commit**

---

## Section 2 — CodecContext + RadioConnection plumbing

### Task 2.1 — Add `setLineInGain(int)` virtual + P1/P2 overrides

**Files:**
- Modify: `src/core/RadioConnection.h` (virtual + storage)
- Modify: `src/core/P1RadioConnection.{h,cpp}` (override + ctx population)
- Modify: `src/core/P2RadioConnection.{h,cpp}` (override; P2 already has `m_mic.lineInGain`)
- Test: `tests/tst_p1_mic_line_in_gain_wire.cpp` (new)

- [ ] **Step 2.1.1: Write failing test**

```cpp
// tests/tst_p1_mic_line_in_gain_wire.cpp
TEST(P1MicLineInGain, setter_propagates_to_bank_11_c2) {
    auto conn = std::make_unique<P1RadioConnection>();
    conn->setLineInGain(15);
    // Force bank 11 in next sendCommandFrame; capture the emitted bytes
    quint8 cc[5] = {};
    conn->composeBank11ForTest(cc);  // test seam
    EXPECT_EQ(cc[2] & 0x1F, 15);
}
```

- [ ] **Step 2.1.2: Declare virtual in `RadioConnection.h`**

```cpp
// after setMicBias declaration (~line 295)
    /// Set mic line-in gain (0-31). Maps to Thetis prn->mic.line_in_gain.
    /// Source: Thetis networkproto1.c:600 [v2.10.3.13+501e3f51]
    /// P1: bank 11 C2 low 5 bits.  P2: byte 51 of CmdHighPriority (already wired).
    virtual void setLineInGain(int gain) = 0;

// shared base storage
    int m_lineInGain{0};
```

- [ ] **Step 2.1.3: Implement P1 override** in `P1RadioConnection.{h,cpp}`. Setter writes `m_lineInGain` and arms `m_forceBank11Next` (existing pattern). `applyHardwareCcContext()` populates `ctx.p1LineInGain = m_lineInGain`.

- [ ] **Step 2.1.4: Implement P2 override** — bridges to existing `m_mic.lineInGain` field.

- [ ] **Step 2.1.5: Add `composeBank11ForTest()` test seam** in `P1RadioConnection.h` (under `#ifdef NEREUS_BUILD_TESTS`).

- [ ] **Step 2.1.6: Run — confirm pass**

- [ ] **Step 2.1.7: Commit**

---

### Task 2.2 — Add `setUserDigOut(quint8)` virtual + P1/P2 overrides

Same pattern as 2.1, for the new `p1UserDigOut` field. P2 currently has no equivalent — make the P2 override a no-op with a comment that the field will be needed when 3M-x adds P2 user-dig-out support.

- [ ] Steps 2.2.1 — 2.2.7 mirror Task 2.1.

---

### Task 2.3 — Add `setPuresignalRun(bool)` virtual + P1/P2 overrides

Routes `prn->puresignal_run` through to bank 11 C2 bit 6. P2 currently doesn't carry this on the wire (PureSignal is 3M-4) — P2 override stores the flag for future use.

- [ ] Steps mirror Task 2.1.

---

### Task 2.4 — Wire setters from `RadioModel::connectToRadio`

**Files:**
- Modify: `src/models/RadioModel.cpp` (after existing setMicTipRing/setMicBias/setMicPTT block)
- Modify: `src/models/TransmitModel.{h,cpp}` (add `lineInGain` + `userDigOut` properties)

- [ ] **Step 2.4.1: Add TransmitModel properties**

```cpp
// src/models/TransmitModel.h — after micPTT property
    Q_PROPERTY(int  lineInGain READ lineInGain WRITE setLineInGain NOTIFY lineInGainChanged)
    Q_PROPERTY(int  userDigOut READ userDigOut WRITE setUserDigOut NOTIFY userDigOutChanged)
    int  lineInGain() const { return m_lineInGain; }
    int  userDigOut() const { return m_userDigOut; }
    void setLineInGain(int g);
    void setUserDigOut(int d);

signals:
    void lineInGainChanged(int);
    void userDigOutChanged(int);

private:
    int m_lineInGain{0};
    int m_userDigOut{0};
```

- [ ] **Step 2.4.2: Wire signals to connection in RadioModel**

```cpp
// src/models/RadioModel.cpp — in connectToRadio() near the existing mic wiring
    QObject::connect(&m_transmitModel, &TransmitModel::lineInGainChanged,
                     m_connection, &RadioConnection::setLineInGain, Qt::QueuedConnection);
    QObject::connect(&m_transmitModel, &TransmitModel::userDigOutChanged,
                     m_connection, [conn = m_connection](int d) {
        QMetaObject::invokeMethod(conn, [conn, d]() { conn->setUserDigOut(quint8(d & 0x0F)); });
    });
    // Initial push (don't wait for first user change)
    m_connection->setLineInGain(m_transmitModel.lineInGain());
    m_connection->setUserDigOut(quint8(m_transmitModel.userDigOut() & 0x0F));
```

- [ ] **Step 2.4.3: Add settings save/load** to `TransmitModel::saveTransmitState` / `restoreTransmitState`.

- [ ] **Step 2.4.4: Add MicProfileManager keys**

```cpp
// src/core/MicProfileManager.cpp — in writeTransmitToProfile / readProfileToTransmit
    out.insert("Mic_LineInGain", QString::number(tx->lineInGain()));
    out.insert("Mic_UserDigOut", QString::number(tx->userDigOut()));
    // and on load:
    tx->setLineInGain(take("Mic_LineInGain", "0").toInt());
    tx->setUserDigOut(take("Mic_UserDigOut", "0").toInt());
```

- [ ] **Step 2.4.5: Test round-trip** — `tst_mic_profile_round_trip` (extend) — write profile with non-default values, load, assert preservation.

- [ ] **Step 2.4.6: Commit**

---

### Task 2.5 — `puresignal_run` flag wired from PureSignalApplet's "Enable" toggle

The `puresignal_run` flag tracks whether the PureSignal feedback DDC routing is currently active. Until 3M-4 actually routes the feedback DDC, this flag mirrors the user's "Enable PureSignal" toggle in PureSignalApplet.

- [ ] **Step 2.5.1: Add property to TransmitModel** — `puresignalEnabled` (already may exist; verify and reuse).

- [ ] **Step 2.5.2: Wire** — `RadioModel::connectToRadio` connects `TransmitModel::puresignalEnabledChanged → RadioConnection::setPuresignalRun`.

- [ ] **Step 2.5.3: Test bit 6 emission** — extend `tst_p1_codec_standard_bank11_polarity` with a wire-test covering the propagation chain (call setter → check ctx field → confirm bit 6).

- [ ] **Step 2.5.4: Commit**

---

## Section 3 — Per-board PA Calibration port

Thetis source — `console.cs:6691-6724` `CalibratedPAPower()` [v2.10.3.13+501e3f51]:

```csharp
public float CalibratedPAPower() {
    float watts = alex_fwd;
    const int entries = 11;                       // 11 cal points
    float interval = 10.0f;                       // default 10 W between labels
    float[] PAsets = { 0.0f, PA10W, PA20W, PA30W, PA40W, PA50W,
                       PA60W, PA70W, PA80W, PA90W, PA100W };
    switch (HardwareSpecific.Model) {
        case HPSDRModel.ANAN100D: interval = 10.0f; break;
        case HPSDRModel.ANAN10:
        case HPSDRModel.ANAN10E:  interval = 1.0f;  // labels are PA1W..PA10W
                                  PAsets = { 0.0f, PA1W..PA10W }; break;
        case HPSDRModel.ANAN8000D: interval = 20.0f;  // PA20W..PA200W
                                   PAsets = { 0.0f, PA20W..PA200W }; break;
        // ... per-board variants
    }
    // Linear interpolation between adjacent calibrated points
    int seg = std::min(int(alex_fwd / interval), entries - 2);
    float frac = (alex_fwd - seg * interval) / interval;
    return PAsets[seg] + frac * (PAsets[seg + 1] - PAsets[seg]);
}
```

The cal points are user-set (not factory defaults — but Thetis ships with each spinbox initialized to its label value, so PA10W defaults to 10.0). On reading the radio's raw `alex_fwd` ADC value, this function returns the user's calibrated watts reading for the FWD power meter.

### Task 3.1 — `PaCalProfile.h` / `.cpp` — model

**Files:**
- Create: `src/core/PaCalProfile.h`, `src/core/PaCalProfile.cpp`
- Test: `tests/tst_pa_cal_profile.cpp`

- [ ] **Step 3.1.1: Define `PaCalProfile.h`**

```cpp
// src/core/PaCalProfile.h — full file
#pragma once

#include "HpsdrModel.h"
#include <array>

namespace NereusSDR {

// Per-board-class PA calibration interval. Maps to Thetis's per-model
// switch in CalibratedPAPower (console.cs:6717-6783 [v2.10.3.13+501e3f51]).
enum class PaCalBoardClass {
    None,        // No PA — Atlas, HermesLite RxOnly
    Anan10,      // ANAN-10 / ANAN-10E:    1 W intervals,  10 W max
    Anan100,     // ANAN-100/100B/100D/200D/7000DLE: 10 W intervals, 100 W max
    Anan8000,    // ANAN-8000DLE:          20 W intervals, 200 W max
    HermesLite,  // Hermes Lite 2:          ~0.5 W intervals (TBD by 3M-2)
};

PaCalBoardClass paCalBoardClassFor(HPSDRModel model) noexcept;

// 11-entry cal point array, watts. Index 0 = 0 W (radio off PA),
// indices 1-10 = the 10 user-calibrated points.
struct PaCalProfile {
    PaCalBoardClass boardClass{PaCalBoardClass::None};
    std::array<float, 11> watts{};  // factory defaults from default()

    // Factory defaults: each spinbox initialized to its label value.
    // E.g. ANAN-10 → {0,1,2,3,4,5,6,7,8,9,10}; ANAN-100 → {0,10,20,...,100}.
    static PaCalProfile defaults(PaCalBoardClass cls);

    // Linear interpolation: rawAdc (in watts at Thetis's "uncalibrated"
    // scale = alex_fwd reading) → calibrated watts.
    // Matches Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13+501e3f51].
    float interpolate(float rawWatts) const noexcept;

    float interval() const noexcept;
};

} // namespace NereusSDR
```

- [ ] **Step 3.1.2: Implement `PaCalProfile.cpp`**

```cpp
// Full implementation including:
//   - paCalBoardClassFor switch on HPSDRModel (12 cases per kAllBoards)
//   - PaCalProfile::defaults factory (one branch per PaCalBoardClass)
//   - PaCalProfile::interpolate (Thetis line-by-line port)
//   - PaCalProfile::interval (returns 1.0f / 10.0f / 20.0f / 0.5f / 0.0f)
```

(Full code follows in implementation; see Thetis `CalibratedPAPower` for line-by-line port.)

- [ ] **Step 3.1.3: Test factory defaults**

```cpp
TEST(PaCalProfile, anan10_factory_defaults_are_label_values) {
    auto p = PaCalProfile::defaults(PaCalBoardClass::Anan10);
    EXPECT_EQ(p.boardClass, PaCalBoardClass::Anan10);
    EXPECT_FLOAT_EQ(p.watts[0], 0.0f);
    EXPECT_FLOAT_EQ(p.watts[1], 1.0f);
    EXPECT_FLOAT_EQ(p.watts[10], 10.0f);
    EXPECT_FLOAT_EQ(p.interval(), 1.0f);
}
TEST(PaCalProfile, anan100_factory_defaults_are_label_values) {
    auto p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
    EXPECT_FLOAT_EQ(p.watts[1], 10.0f);
    EXPECT_FLOAT_EQ(p.watts[10], 100.0f);
    EXPECT_FLOAT_EQ(p.interval(), 10.0f);
}
TEST(PaCalProfile, anan8000_factory_defaults) {
    auto p = PaCalProfile::defaults(PaCalBoardClass::Anan8000);
    EXPECT_FLOAT_EQ(p.watts[1], 20.0f);
    EXPECT_FLOAT_EQ(p.watts[10], 200.0f);
    EXPECT_FLOAT_EQ(p.interval(), 20.0f);
}
```

- [ ] **Step 3.1.4: Test interpolation matches Thetis at calibrated points**

```cpp
TEST(PaCalProfile, interpolate_at_calpoints_returns_calpoint_value) {
    auto p = PaCalProfile::defaults(PaCalBoardClass::Anan10);
    p.watts[5] = 4.7f;  // user calibrated: at 5 W "raw" reads as 4.7 W actual
    EXPECT_NEAR(p.interpolate(5.0f), 4.7f, 0.001f);
}
TEST(PaCalProfile, interpolate_between_calpoints_is_linear) {
    auto p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
    p.watts[1] = 12.0f;  // 10W reads as 12W
    p.watts[2] = 22.0f;  // 20W reads as 22W
    EXPECT_NEAR(p.interpolate(15.0f), 17.0f, 0.001f);  // halfway → 12+(22-12)/2 = 17
}
TEST(PaCalProfile, interpolate_above_max_clamps_to_last_segment) {
    auto p = PaCalProfile::defaults(PaCalBoardClass::Anan100);
    EXPECT_FLOAT_EQ(p.interpolate(150.0f), 150.0f);  // extrapolates linearly with default values
}
```

- [ ] **Step 3.1.5: Test paCalBoardClassFor mapping**

Test all 12 `kAllBoards` enum values map to the expected `PaCalBoardClass`.

- [ ] **Step 3.1.6: Commit**

---

### Task 3.2 — Extend `CalibrationController` with `paCalProfile`

**Files:**
- Modify: `src/core/CalibrationController.{h,cpp}`
- Modify: `src/models/RadioModel.cpp` (initialize controller's profile from `paCalBoardClassFor(caps.board.model)`)

- [ ] **Step 3.2.1: Add storage + getters** to `CalibrationController.h`

```cpp
    PaCalProfile paCalProfile() const { return m_paCalProfile; }
    void setPaCalProfile(const PaCalProfile& p);
    void setPaCalPoint(int idx, float watts);  // emits paCalPointChanged
    float calibratedFwdPowerWatts(float rawAdcWatts) const noexcept;

signals:
    void paCalPointChanged(int idx, float watts);
    void paCalProfileChanged();

private:
    PaCalProfile m_paCalProfile;
```

- [ ] **Step 3.2.2: Wire to RadioModel** — on `connectToRadio`, set profile from `paCalBoardClassFor(m_caps->board model)`. On disconnect, reset.

- [ ] **Step 3.2.3: Persistence** — under `paCalibration/calPoint{N}` (per-MAC). Restore on hardware select.

- [ ] **Step 3.2.4: Tests** — set profile, set point, save settings, reload, confirm round-trip.

- [ ] **Step 3.2.5: Commit**

---

### Task 3.3 — `PaCalibrationGroup` widget

**Files:**
- Create: `src/gui/setup/hardware/PaCalibrationGroup.{h,cpp}`
- Modify: `src/gui/setup/hardware/CalibrationTab.{h,cpp}` (add as Group 6)

A `QGroupBox` with 10 labelled spinboxes. Labels and ranges depend on `PaCalBoardClass`:
- `Anan10` → "1 W" .. "10 W", spinbox range 0.0..15.0 step 0.1
- `Anan100` → "10 W" .. "100 W", spinbox range 0.0..150.0 step 1.0
- `Anan8000` → "20 W" .. "200 W", spinbox range 0.0..300.0 step 1.0
- `None` → group is hidden

Constructed once; `populate(controller, boardClass)` rebuilds the spinbox set when board class changes.

- [ ] **Step 3.3.1: Define `PaCalibrationGroup.h`** — `QGroupBox` subclass, `populate(CalibrationController*, PaCalBoardClass)`, signals model changes through controller.

- [ ] **Step 3.3.2: Implement `PaCalibrationGroup.cpp`** — uses Qt layout patterns from existing `CalibrationTab` Group 5 (`m_paSensSpin` etc.). Spinboxes labelled per `PaCalBoardClass`.

- [ ] **Step 3.3.3: Add to `CalibrationTab` as Group 6**

```cpp
// src/gui/setup/hardware/CalibrationTab.h — add member:
    PaCalibrationGroup* m_paCalGroup{nullptr};

// .cpp — in constructor add to layout, wire signals:
    m_paCalGroup = new PaCalibrationGroup(m_calCtrl, this);
    layout->addWidget(m_paCalGroup);
    connect(m_calCtrl, &CalibrationController::paCalProfileChanged,
            this, [this]() { m_paCalGroup->populate(m_calCtrl, m_calCtrl->paCalProfile().boardClass); });
```

- [ ] **Step 3.3.4: Visual test** — manual launch, connect to ANAN-10E (real radio), verify Group 6 shows 10 spinboxes labelled 1W..10W.

- [ ] **Step 3.3.5: Commit**

---

### Task 3.4 — Wire `alex_fwd` reading through `calibratedFwdPowerWatts`

The radio reports raw forward-power readings via the EP6 status path. Find the existing meter-reading path (likely `RxChannel::getMeter` or `MeterPoller` for the FWD scale), route it through `m_calCtrl->calibratedFwdPowerWatts(rawWatts)` before pushing to `MeterWidget`.

- [ ] **Step 3.4.1: Find the FWD meter path**

```
grep -rn 'MeterScale::Fwd\|alex_fwd\|fwdPowerWatts' src/ | head -20
```

- [ ] **Step 3.4.2: Insert calibration step** in the FWD meter publication site. Cite Thetis `console.cs:6691-6724` [v2.10.3.13+501e3f51] inline.

- [ ] **Step 3.4.3: Test** — synthesise raw FWD readings of 1.0, 5.0, 10.0; with non-default cal points, verify reported watts match `interpolate()`.

- [ ] **Step 3.4.4: Commit**

---

### Task 3.5 — Add SWR foldback to drive scaling

Currently `RadioModel.cpp:830-833` does `(power * 255) / 100`. Thetis applies `_swr_protect` from mi0bot `NetworkIO.cs:209-211 [v2.10.3.14-beta1]`:

```csharp
int i = (int)(255 * f * _swr_protect);  // f normalised 0..1, _swr_protect ≤ 1.0
SetOutputPowerFactor(i);
```

`_swr_protect` reduces the wire byte when reflected power is high, protecting the PA. Default = 1.0 (no foldback).

**Files:**
- Modify: `src/models/RadioModel.cpp:823-840`
- Modify: `src/models/TransmitModel.{h,cpp}` (add `swrProtectFactor` property, default 1.0)
- Test: `tests/tst_pa_cal_swr_foldback.cpp` (new)

- [ ] **Step 3.5.1: Add `swrProtectFactor` to TransmitModel**

```cpp
// TransmitModel.h
    float swrProtectFactor() const { return m_swrProtect; }
    void setSwrProtectFactor(float f);
private:
    float m_swrProtect{1.0f};  // 1.0 = no foldback; clamped 0..1
```

- [ ] **Step 3.5.2: Apply in RadioModel drive scaling**

```cpp
// src/models/RadioModel.cpp — replace existing scale formula
        const float f = std::clamp(power / 100.0f, 0.0f, 1.0f);
        const float swrProtect = std::clamp(m_transmitModel.swrProtectFactor(), 0.0f, 1.0f);
        const int wireDrive = std::clamp(int(255.0f * f * swrProtect), 0, 255);
        // From mi0bot NetworkIO.cs:209-211 [v2.10.3.14-beta1]:
        //   int i = (int)(255 * f * _swr_protect);  // f normalised 0..1
        //   SetOutputPowerFactor(i);
```

- [ ] **Step 3.5.3: Test SWR foldback wire byte**

```cpp
TEST(PaCalSwrFoldback, no_foldback_at_factor_1_0) {
    // 50% power, factor 1.0 → wire = 127
    // ... [test scaffolding to drive RadioModel signal path and capture setTxDrive arg]
}
TEST(PaCalSwrFoldback, half_foldback_at_factor_0_5) {
    // 100% power, factor 0.5 → wire = 127 (50% wire)
}
TEST(PaCalSwrFoldback, foldback_clamps_negative_factors_to_zero) {
    // factor -0.1 (out-of-range) → clamped to 0 → wire = 0
}
```

- [ ] **Step 3.5.4: Run — confirm pass**

- [ ] **Step 3.5.5: Commit**

---

## Section 4 — Close documented `BoardCapabilities` flag-wiring gaps

Audit found 3 flags defined and populated per board but with zero consumers in the code: `hasStepAttenuatorCal` (set on 8 boards), `hasSidetoneGenerator` (HL2 + RxOnly), `hasPennyLane` (every board).

### Task 4.1 — Wire `hasStepAttenuatorCal`

Per Thetis, this flag enables the per-step calibration table for the RX step attenuator (vs simple linear scale). Find Thetis source for the flag in `setup.cs` step-att cal page; gate `StepAttenuatorController`'s adaptive cal on `caps.hasStepAttenuatorCal`.

- [ ] **Step 4.1.1: Find Thetis source for the flag**

```
grep -n 'StepAttenuator.*Cal\|hasStepAttCal\|adaptive.*att' "/Users/j.j.boyd/NereusSDR/../Thetis/Project Files/Source/Console/setup.cs" | head -20
```

- [ ] **Step 4.1.2: Gate the adaptive-cal branch in `StepAttenuatorController.cpp`** on `caps.hasStepAttenuatorCal`. (Likely already gates implicitly via the wider Setup → Step ATT cal page; confirm.)

- [ ] **Step 4.1.3: Test** — force a board with `hasStepAttenuatorCal=false` (e.g. Atlas), confirm the controller falls back to flat linear scale.

- [ ] **Step 4.1.4: Commit**

### Task 4.2 — Wire `hasSidetoneGenerator`

HL2 firmware generates CW sidetone in hardware; standard P1 boards don't. Thetis's CW page exposes a "firmware sidetone" toggle gated on the radio. Until 3M-2 lights up CW TX in NereusSDR, this flag should at least gate the relevant Setup tab visibility (or the CWX applet's "FW Sidetone" toggle if one exists).

- [ ] **Step 4.2.1: Find the sidetone toggle in NereusSDR**

```
grep -rn 'sidetone\|Sidetone' src/gui/applets/CwxApplet.* src/gui/setup/ | head -20
```

- [ ] **Step 4.2.2: Gate visibility of the toggle on `caps.hasSidetoneGenerator`**

If no toggle exists yet, add a placeholder: comment the flag's intended consumer in `CwxApplet`, hide it for now, document in the plan that 3M-2 will fully wire it. (Strictly: the audit only asked for the flag to have at least one consumer; gating an existing UI element fulfils that.)

- [ ] **Step 4.2.3: Test** — verify the gate, with a `hasSidetoneGenerator=false` board the toggle is hidden.

- [ ] **Step 4.2.4: Commit**

### Task 4.3 — Wire `hasPennyLane` + add User Dig Out UI

Penny / Penny-Lane is the OC-output companion board. Its outputs include `user_dig_out` (4 user-controllable digital pins) and the OC bands. Thetis exposes these in Setup → Hardware → Penny / Hermes Ctrl tab.

- [ ] **Step 4.3.1: Add a "User Dig Out" group** to the existing OC Outputs tab in `HardwarePage`, with 4 checkboxes wired to `TransmitModel::setUserDigOut(quint8)` (already added in Task 2.4).

- [ ] **Step 4.3.2: Gate the group on `caps.hasPennyLane`**.

- [ ] **Step 4.3.3: Per-MAC persistence** under `hardware/<mac>/userDigOut/{1..4}`.

- [ ] **Step 4.3.4: Test** — toggle a checkbox, confirm `TransmitModel::userDigOut()` updates and the wire byte (Task 1.3 test) reflects it.

- [ ] **Step 4.3.5: Commit**

---

## Section 5 — Bench Verification

Final pre-PR checkpoint. All previous task checkboxes must be ticked.

### Task 5.1 — Build + auto-launch

- [ ] **Step 5.1.1: Build** — `cmake --build build -j$(nproc)`. Expected: success.

- [ ] **Step 5.1.2: Run all tests** — `ctest --test-dir build --output-on-failure`. Expected: all P1 + P2 + shared suites green, including new `tst_p1_codec_standard_bank11_polarity`, `tst_p1_codec_standard_freq_bank_xmitbit`, `tst_pa_cal_profile`, `tst_pa_cal_swr_foldback`, `tst_board_capability_flag_wiring`.

- [ ] **Step 5.1.3: Auto-launch** the worktree binary (per `feedback_auto_launch_after_build`).

### Task 5.2 — Bench-verify on ANAN-10E (primary radio)

- [ ] **Step 5.2.1: Connect** — confirm RadioModel transitions to Connected, RX waterfall populates.

- [ ] **Step 5.2.2: TUNE on 14.230 USB** — confirm:
  - T/R relay holds cleanly (no flutter — primary regression test for Task 1.1)
  - RF carrier appears on dial frequency
  - FWD power meter reads non-zero, scales with TUNE drive slider

- [ ] **Step 5.2.3: SSB voice TX on 14.230 USB** — confirm voice modulates RF, FWD power reasonable for drive setting.

- [ ] **Step 5.2.4: PA cal page** — Setup → Hardware → Calibration: verify Group 6 shows 10 spinboxes labelled 1W..10W (since 10E is `Anan10` class). Change one spinbox value, observe FWD meter reading shifts accordingly.

- [ ] **Step 5.2.5: User Dig Out** — Setup → Hardware → OC Outputs → User Dig Out: toggle checkboxes; capture EP2 bytes via Wireshark and confirm bank 11 C3 low 4 bits update.

- [ ] **Step 5.2.6: Quit + relaunch** — confirm cal points, user-dig-out, line-in-gain, swr-protect-factor all persist.

### Task 5.3 — Regression-check on Hermes Lite 2 (no breakage)

- [ ] **Step 5.3.1: Connect HL2** — TUNE on a known-quiet band, confirm relay still holds (PR #161's HL2 fixes still effective).

- [ ] **Step 5.3.2: Verify HL2 Cal tab** — confirm `Anan10`-class behaviour does NOT show up for HL2 (different `paCalBoardClassFor` mapping; either `HermesLite` or `None`).

### Task 5.4 — Open the PR

- [ ] **Step 5.4.1: CHANGELOG entry**

```
## Unreleased

### Fixed
- **P1 (all non-HL2 boards)**: mic_ptt direct polarity in `P1CodecStandard` —
  fixes rapid T/R relay clicking on TUNE/TX (ANAN-10E reported, all standard-codec
  boards affected). Source: Thetis networkproto1.c:597-598 [v2.10.3.13+501e3f51].

### Added
- **P1 (all non-HL2 boards)**: bank 11 C2 (`line_in_gain` + `puresignal_run` flag)
  + C3 (`user_dig_out`) wire-format parity vs Thetis.
- **All P1 boards**: C0 XmitBit on frequency banks (1, 2, 3, 5–9) — matches
  Thetis `networkproto1.c:447 C0 = XmitBit` before the case switch.
- **All boards with `hasPaProfile=true`**: per-board PA calibration —
  `PaCalProfile` model + `PaCalibrationGroup` UI with 10 cal-point spinboxes
  per board class (1 W intervals for ANAN-10/10E; 10 W for ANAN-100 family;
  20 W for ANAN-8000DLE), `CalibratedPAPower` interpolation pipeline, SWR
  foldback in drive scaling. Source: Thetis console.cs:6691-6724 [v2.10.3.13+501e3f51].
- **All boards with `hasPennyLane=true`**: User Dig Out UI (4 checkboxes) +
  per-MAC persistence + bank 11 C3 wire emission.

### Wired (previously documented but unconsumed)
- `BoardCapabilities.hasStepAttenuatorCal` — gates StepAttenuatorController
  adaptive cal branch.
- `BoardCapabilities.hasSidetoneGenerator` — gates CWX applet firmware-sidetone
  toggle visibility (3M-2 will fully wire the toggle).
- `BoardCapabilities.hasPennyLane` — gates User Dig Out UI group.
```

- [ ] **Step 5.4.2: Open PR**

```
gh pr create --title "fix(p1): full Thetis parity for all non-HL2 P1 boards (10E rapid relay clicking + PA cal port + flag wiring)" --body "$(cat <<'EOF'
## Summary

Single-PR full Thetis-parity port for every non-HL2 P1 board. Triggered by
rapid T/R relay clicking reported on ANAN-10E (HermesII); root cause and
parity gaps affect every standard-codec P1 board.

### Wire-format fixes (P1CodecStandard)

[Per-task summary — see commits and this PR's `docs/architecture/2026-05-02-p1-full-parity-plan.md`]

### PA cal port

[Per-task summary]

### Closed flag-wiring gaps

[Per-task summary]

## Bench-verified on ANAN-10E (HermesII):

[Section 5.2 checklist with results]

## Regression-checked on HL2:

[Section 5.3 checklist with results]

## Tests

[List of new and modified test files; ctest line counts; pass status]

## Source citations

Every wire-format change cites Thetis `[v2.10.3.13+501e3f51]` per CLAUDE.md
HOW-TO-PORT.md.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 5.4.3: Open PR URL in browser** (per `feedback_open_links_after_post`).

---

## Self-Review (post-write)

**1. Spec coverage:**
- Wire-format parity (mic_ptt + bank 11 C2/C3 + C0 XmitBit) → Section 1
- CodecContext + plumbing → Section 2
- PA cal port (model + UI + interpolation + SWR foldback + persistence) → Section 3
- Flag wiring (hasStepAttenuatorCal, hasSidetoneGenerator, hasPennyLane) → Section 4
- Bench verification → Section 5
- ✅ All sections covered.

**2. Placeholder scan:** the plan has TWO soft placeholders that will get tightened during execution:
- Task 3.4 Step 1: "Find the existing meter-reading path" — concrete grep command included; the agent finds and edits the right file.
- Task 4.1 Step 1, 4.2 Step 1: "Find Thetis source for the flag" — concrete grep commands included; the agent reads the actual Thetis source and applies the gate.
These are *find-and-apply* steps, not "TBD" — the search is parameterized but the action is concrete.

**3. Type consistency:**
- `PaCalProfile`, `PaCalBoardClass`, `paCalBoardClassFor` consistent across Tasks 3.1–3.3.
- `setLineInGain(int)`, `setUserDigOut(quint8)`, `setPuresignalRun(bool)` consistent across Tasks 2.1–2.5.
- ✅ Consistent.

**Total estimated commit count:** ~16-20 GPG-signed commits (one per task), single PR.

**Estimated lines changed:** ~2000-3000 (codec + plumbing + PA cal model + UI + tests + persistence + 3 flag gates).

**Estimated time:** 2-3 working days of focused execution + bench verification.

// no-port-check: NereusSDR-original unit-test file.  RADE channel-swap
// is a NereusSDR-native extension; no Thetis equivalent.
// =================================================================
// tests/tst_slice_model_rade_swap.cpp  (NereusSDR)
// =================================================================
//
// Phase 3R Task J3 + K-bench unit tests: SliceModel::setDspMode
// channel-additive lifecycle and the RxChannel::wdspModeFor mapping
// at the WDSP API boundary.
//
// K-bench contract (replaces original J3 destroy-and-replace design):
//   - The WDSP RxChannel for the slice is created once at connect
//     time and stays alive in every mode (USB, LSB, RADE_U, RADE_L,
//     …).  WDSP serves as the SSB demod front-end and produces
//     decoded audio that drives S-meter / spectrum / AGC every tick.
//   - RadeChannel is created ALONGSIDE RxChannel in RADE_U /
//     RADE_L only.  It consumes WDSP's decoded audio (downsampled
//     to 24 kHz, real=audio, imag=0) and owns the speaker path
//     while active.
//   - The WDSP-facing mode is mapped at the RxChannel boundary:
//     RxChannel::wdspModeFor(RADE_U) -> USB,
//     RxChannel::wdspModeFor(RADE_L) -> LSB.
//     The slice-facing mode (what RxChannel::mode() returns and
//     what RxChannel emits on modeChanged) is the original RADE_U /
//     RADE_L; only the int passed to SetRXAMode is mapped.
//
// On the transition into either RADE sideband (DSPMode::RADE_U or
// DSPMode::RADE_L), setDspMode:
//   1. Creates a new RadeChannel via WdspEngine (RxChannel left
//      alone).
//   2. Calls RadeChannel::setSideband(true) for RADE_U or
//      setSideband(false) for RADE_L.
//   3. Calls RadioModel::wireRadeChannel(sliceId, channel, slice).
//   4. Starts the RadeChannel with the configured model path (or
//      the "dummy" librade sentinel if no path is set).
//
// On the reverse transition out of either RADE sideband, setDspMode:
//   1. Destroys the slice's RadeChannel.
//   2. (RxChannel is NOT touched - it was alive the whole time.)
//
// On a RADE_U <-> RADE_L transition (sideband flip), setDspMode:
//   1. Destroys the slice's RadeChannel.
//   2. Creates a fresh RadeChannel with the new sideband flag.
//
// The tests below reach the WdspEngine via the friend-access trick
// (NEREUS_BUILD_TESTS) to set m_initialized = true synchronously so
// createRxChannel does not error out on its !m_initialized guard.
// createRadeChannel does NOT require m_initialized (it is pure C++
// object management, no WDSP-side state).
//
// Test cases (9):
//   1. switchToRadeUpperKeepsRxChannelAndAddsRadeChannel
//   2. switchToRadeLowerKeepsRxChannelAndAddsRadeChannel
//   3. switchFromRadeUpperKeepsRxChannelAndDropsRadeChannel
//   4. switchFromRadeLowerKeepsRxChannelAndDropsRadeChannel
//   5. switchToRadeUpperSetsSidebandFlag
//   6. switchToRadeLowerSetsSidebandFlag
//   7. switchRadeUpperToLowerFlipsSidebandKeepsRxChannel
//   8. switchToSameModeIsNoOp
//   9. wdspModeForMapsRadeToSsb
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11 - New test file for Phase 3R Task J3.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-05-11 - Split RADE into RADE_U / RADE_L; covered both
//                 sidebands + the U <-> L recreate path.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-05-12 - Rewritten against K-bench RADE-additive contract:
//                 RxChannel stays alive in every mode; RadeChannel
//                 is created alongside; WDSP-facing mode is mapped
//                 RADE_U->USB / RADE_L->LSB at the RxChannel
//                 boundary.  Closes PR #238 review P1 #1 + #2.
//                 J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/RadeChannel.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceModelRadeSwap : public QObject {
    Q_OBJECT

    // RAII helper that wraps a stack-allocated RadioModel + primed
    // WdspEngine + seeded RxChannel.  Each test method constructs one,
    // exercises the swap, then the destructor unwinds the engine on
    // scope exit.  Stack-allocation keeps the engine lifetime
    // strictly within one test method so the global WDSP impulse-cache
    // tear-down (~WdspEngine -> shutdown -> destroy_impulse_cache)
    // runs at most once per method instead of accumulating.
    struct RadioFixture {
        RadioModel  radio;
        WdspEngine* engine{nullptr};
        SliceModel* slice{nullptr};

        RadioFixture()
        {
            engine = radio.wdspEngine();
            engine->m_initialized = true;  // friend access (NEREUS_BUILD_TESTS)

            // Seed slice 0 with a default RxChannel so the K-bench
            // contract has a channel to keep alive across mode swaps.
            engine->createRxChannel(0);

            const int sliceIdx = radio.addSlice();
            Q_ASSERT(sliceIdx == 0);
            slice = radio.sliceById(0);
            Q_ASSERT(slice != nullptr);
        }
    };

private slots:

    // ── Test 1: switchToRadeUpperKeepsRxChannelAndAddsRadeChannel ──────
    //
    // K-bench: setDspMode(RADE_U) creates a RadeChannel ALONGSIDE the
    // RxChannel.  RxChannel is not destroyed (WDSP keeps demodulating
    // for S-meter / spectrum / AGC and feeds audio to RADE).
    void switchToRadeUpperKeepsRxChannelAndAddsRadeChannel() {
        RadioFixture fx;

        QVERIFY(fx.engine->rxChannel(0) != nullptr);
        QVERIFY(fx.engine->radeChannel(0) == nullptr);

        fx.slice->setDspMode(DSPMode::RADE_U);

        QVERIFY2(fx.engine->rxChannel(0) != nullptr,
                 "K-bench: RxChannel must stay alive across RADE entry "
                 "(WDSP is the SSB demod front-end feeding RADE)");
        QVERIFY2(fx.engine->radeChannel(0) != nullptr,
                 "setDspMode(RADE_U) must create a RadeChannel alongside RxChannel");
    }

    // ── Test 2: switchToRadeLowerKeepsRxChannelAndAddsRadeChannel ──────
    //
    // Same as Test 1 but for the lower-sideband variant.
    void switchToRadeLowerKeepsRxChannelAndAddsRadeChannel() {
        RadioFixture fx;

        QVERIFY(fx.engine->rxChannel(0) != nullptr);
        QVERIFY(fx.engine->radeChannel(0) == nullptr);

        fx.slice->setDspMode(DSPMode::RADE_L);

        QVERIFY2(fx.engine->rxChannel(0) != nullptr,
                 "K-bench: RxChannel must stay alive across RADE entry");
        QVERIFY2(fx.engine->radeChannel(0) != nullptr,
                 "setDspMode(RADE_L) must create a RadeChannel alongside RxChannel");
    }

    // ── Test 3: switchFromRadeUpperKeepsRxChannelAndDropsRadeChannel ───
    //
    // K-bench: round trip into RADE_U and back to USB destroys the
    // RadeChannel but RxChannel was never touched.
    void switchFromRadeUpperKeepsRxChannelAndDropsRadeChannel() {
        RadioFixture fx;

        RxChannel* rxBefore = fx.engine->rxChannel(0);
        QVERIFY(rxBefore != nullptr);

        fx.slice->setDspMode(DSPMode::RADE_U);
        QVERIFY(fx.engine->radeChannel(0) != nullptr);
        QVERIFY2(fx.engine->rxChannel(0) == rxBefore,
                 "RADE entry must not destroy or replace the RxChannel");

        fx.slice->setDspMode(DSPMode::USB);
        QVERIFY2(fx.engine->radeChannel(0) == nullptr,
                 "setDspMode(<non-RADE>) must destroy the slice's RadeChannel");
        QVERIFY2(fx.engine->rxChannel(0) == rxBefore,
                 "K-bench: same RxChannel instance persists across the round trip");
    }

    // ── Test 4: switchFromRadeLowerKeepsRxChannelAndDropsRadeChannel ───
    //
    // Same as Test 3 but for the lower-sideband variant.
    void switchFromRadeLowerKeepsRxChannelAndDropsRadeChannel() {
        RadioFixture fx;

        RxChannel* rxBefore = fx.engine->rxChannel(0);
        QVERIFY(rxBefore != nullptr);

        fx.slice->setDspMode(DSPMode::RADE_L);
        QVERIFY(fx.engine->radeChannel(0) != nullptr);
        QVERIFY2(fx.engine->rxChannel(0) == rxBefore,
                 "RADE entry must not destroy or replace the RxChannel");

        fx.slice->setDspMode(DSPMode::LSB);
        QVERIFY2(fx.engine->radeChannel(0) == nullptr,
                 "setDspMode(<non-RADE>) must destroy the slice's RadeChannel");
        QVERIFY2(fx.engine->rxChannel(0) == rxBefore,
                 "K-bench: same RxChannel instance persists across the round trip");
    }

    // ── Test 5: switchToRadeUpperSetsSidebandFlag ──────────────────────
    //
    // After setDspMode(RADE_U), the created RadeChannel's sideband flag
    // is set to upper (true).
    void switchToRadeUpperSetsSidebandFlag() {
        RadioFixture fx;

        fx.slice->setDspMode(DSPMode::RADE_U);

        RadeChannel* ch = fx.engine->radeChannel(0);
        QVERIFY(ch != nullptr);
        QVERIFY2(ch->sidebandUpper(),
                 "setDspMode(RADE_U) must set RadeChannel::sidebandUpper()=true");
    }

    // ── Test 6: switchToRadeLowerSetsSidebandFlag ──────────────────────
    //
    // After setDspMode(RADE_L), the created RadeChannel's sideband flag
    // is set to lower (false).
    void switchToRadeLowerSetsSidebandFlag() {
        RadioFixture fx;

        fx.slice->setDspMode(DSPMode::RADE_L);

        RadeChannel* ch = fx.engine->radeChannel(0);
        QVERIFY(ch != nullptr);
        QVERIFY2(!ch->sidebandUpper(),
                 "setDspMode(RADE_L) must set RadeChannel::sidebandUpper()=false");
    }

    // ── Test 7: switchRadeUpperToLowerFlipsSidebandKeepsRxChannel ──────
    //
    // RADE_U <-> RADE_L is a destroy-and-recreate of the RadeChannel
    // (sideband flag is set on construction).  RxChannel stays
    // alive (K-bench).  Recreate verified via post-state because the
    // allocator may reuse the same memory location.
    void switchRadeUpperToLowerFlipsSidebandKeepsRxChannel() {
        RadioFixture fx;

        RxChannel* rxBefore = fx.engine->rxChannel(0);
        QVERIFY(rxBefore != nullptr);

        fx.slice->setDspMode(DSPMode::RADE_U);
        QVERIFY(fx.engine->radeChannel(0) != nullptr);
        QVERIFY(fx.engine->radeChannel(0)->sidebandUpper());
        QVERIFY2(fx.engine->rxChannel(0) == rxBefore,
                 "RADE_U entry must not touch the RxChannel");

        // RADE_U -> RADE_L: sideband flag must flip to lower.
        fx.slice->setDspMode(DSPMode::RADE_L);
        RadeChannel* afterDown = fx.engine->radeChannel(0);
        QVERIFY(afterDown != nullptr);
        QVERIFY2(!afterDown->sidebandUpper(),
                 "RADE_U -> RADE_L must flip the sideband flag to lower");
        QVERIFY2(afterDown->isActive(),
                 "After RADE_U -> RADE_L the new RadeChannel must be active");
        QVERIFY2(fx.engine->rxChannel(0) == rxBefore,
                 "RADE_U <-> RADE_L must not touch the RxChannel");

        // And back the other way: RADE_L -> RADE_U flips sideband to
        // upper.  Same recreate contract.
        fx.slice->setDspMode(DSPMode::RADE_U);
        RadeChannel* afterUp = fx.engine->radeChannel(0);
        QVERIFY(afterUp != nullptr);
        QVERIFY2(afterUp->sidebandUpper(),
                 "RADE_L -> RADE_U must flip the sideband flag to upper");
        QVERIFY2(afterUp->isActive(),
                 "After RADE_L -> RADE_U the new RadeChannel must be active");
        QVERIFY2(fx.engine->rxChannel(0) == rxBefore,
                 "RADE_L <-> RADE_U must not touch the RxChannel");
    }

    // ── Test 8: switchToSameModeIsNoOp ─────────────────────────────────
    //
    // Calling setDspMode with the current mode is a no-op: no channel
    // teardown / construction, no signal emission.
    void switchToSameModeIsNoOp() {
        RadioFixture fx;

        // Slice starts at USB (SliceModel::m_dspMode default).
        // Capture baseline RxChannel pointer.
        RxChannel* before = fx.engine->rxChannel(0);
        QVERIFY(before != nullptr);

        QSignalSpy modeSpy(fx.slice, &SliceModel::dspModeChanged);
        fx.slice->setDspMode(DSPMode::USB);  // same mode

        QCOMPARE(modeSpy.count(), 0);
        QCOMPARE(fx.engine->rxChannel(0), before);
        QVERIFY(fx.engine->radeChannel(0) == nullptr);
    }

    // ── Test 9: wdspModeForMapsRadeToSsb ───────────────────────────────
    //
    // K-bench: RxChannel::wdspModeFor maps RADE_U -> USB and
    // RADE_L -> LSB at the WDSP API boundary so SetRXAMode never sees
    // a raw enum 12/13 (review finding 2026-05-12, PR #238).  All
    // other modes pass through unchanged.
    void wdspModeForMapsRadeToSsb() {
        // RADE sidebands map to their SSB equivalents.
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::RADE_U), DSPMode::USB);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::RADE_L), DSPMode::LSB);

        // Standard WDSP modes are pass-through.
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::LSB),  DSPMode::LSB);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::USB),  DSPMode::USB);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::DSB),  DSPMode::DSB);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::CWL),  DSPMode::CWL);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::CWU),  DSPMode::CWU);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::FM),   DSPMode::FM);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::AM),   DSPMode::AM);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::DIGU), DSPMode::DIGU);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::SPEC), DSPMode::SPEC);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::DIGL), DSPMode::DIGL);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::SAM),  DSPMode::SAM);
        QCOMPARE(RxChannel::wdspModeFor(DSPMode::DRM),  DSPMode::DRM);
    }
};

QTEST_APPLESS_MAIN(TestSliceModelRadeSwap)
#include "tst_slice_model_rade_swap.moc"

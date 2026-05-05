// no-port-check: NereusSDR-original unit-test file. Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_tx_channel_dexp_envelope.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TxChannel DEXP envelope/timing setters (Phase 3M-3a-iii).
//
// Source references (cited for traceability; logic lives in TxChannel.cpp):
//   cmaster.cs:166-176 [v2.10.3.13] - SetDEXPRun, SetDEXPDetectorTau,
//     SetDEXPAttackTime, SetDEXPReleaseTime DllImports.
//   setup.Designer.cs:45070-45098 [v2.10.3.13] - udDEXPDetTau 1..100 ms default 20.
//   setup.Designer.cs:45027-45055 [v2.10.3.13] - udDEXPAttack 2..100 ms default 2.
//   setup.Designer.cs:44967-44995 [v2.10.3.13] - udDEXPRelease 2..1000 ms default 100.
//
// Tests verify (NEREUS_BUILD_TESTS test-seam accessors required):
//   - First call stores the value (NaN sentinel fires on doubles).
//   - Round-trip / clamp at the wrapper boundary (Thetis ranges).
//   - Idempotent guard: second identical call is observable as the stored value.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 - New test file for Phase 3M-3a-iii Tasks 1-2: DEXP master-
//                 enable, detector-tau, attack-time, release-time wrappers.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#define NEREUS_BUILD_TESTS 1

#include <QtTest/QtTest>
#include <cmath>   // std::isnan

#include "core/TxChannel.h"

using namespace NereusSDR;

class TstTxChannelDexpEnvelope : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;  // WDSP.id(1, 0) - TX channel

private slots:

    // --------------------------------------------------------------
    // setDexpRun  (bool master-enable for DEXP audio expansion)
    // --------------------------------------------------------------

    void setDexpRun_idempotent() {
        TxChannel ch(kChannelId);
        QCOMPARE(ch.lastDexpRunForTest(), false);   // default
        ch.setDexpRun(true);
        QCOMPARE(ch.lastDexpRunForTest(), true);
        ch.setDexpRun(true);   // idempotent re-call
        QCOMPARE(ch.lastDexpRunForTest(), true);
        ch.setDexpRun(false);
        QCOMPARE(ch.lastDexpRunForTest(), false);
    }

    // --------------------------------------------------------------
    // setDexpDetectorTau  (range 1..100 ms, default 20)
    // --------------------------------------------------------------

    void setDexpDetectorTau_inRange() {
        // Thetis range 1..100 ms (setup.Designer.cs:45078-45093).
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastDexpDetectorTauForTest()));
        ch.setDexpDetectorTau(20.0);
        QCOMPARE(ch.lastDexpDetectorTauForTest(), 20.0);
        ch.setDexpDetectorTau(50.0);
        QCOMPARE(ch.lastDexpDetectorTauForTest(), 50.0);
    }

    void setDexpDetectorTau_clampedLow() {
        // Below Thetis minimum of 1 ms gets clamped to 1.
        TxChannel ch(kChannelId);
        ch.setDexpDetectorTau(0.5);
        QCOMPARE(ch.lastDexpDetectorTauForTest(), 1.0);
        ch.setDexpDetectorTau(-10.0);
        QCOMPARE(ch.lastDexpDetectorTauForTest(), 1.0);
    }

    void setDexpDetectorTau_clampedHigh() {
        // Above Thetis maximum of 100 ms gets clamped to 100.
        TxChannel ch(kChannelId);
        ch.setDexpDetectorTau(150.0);
        QCOMPARE(ch.lastDexpDetectorTauForTest(), 100.0);
    }

    void setDexpDetectorTau_idempotent() {
        TxChannel ch(kChannelId);
        ch.setDexpDetectorTau(20.0);
        ch.setDexpDetectorTau(20.0);   // idempotent re-call (no second WDSP push)
        QCOMPARE(ch.lastDexpDetectorTauForTest(), 20.0);
    }

    // --------------------------------------------------------------
    // setDexpAttackTime  (range 2..100 ms, default 2)
    // setup.Designer.cs:45027-45055 [v2.10.3.13]
    // --------------------------------------------------------------

    void setDexpAttackTime_inRange() {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastDexpAttackTimeForTest()));
        ch.setDexpAttackTime(2.0);
        QCOMPARE(ch.lastDexpAttackTimeForTest(), 2.0);
        ch.setDexpAttackTime(50.0);
        QCOMPARE(ch.lastDexpAttackTimeForTest(), 50.0);
    }
    void setDexpAttackTime_clampedLow() {
        TxChannel ch(kChannelId);
        ch.setDexpAttackTime(0.5);
        QCOMPARE(ch.lastDexpAttackTimeForTest(), 2.0);
    }
    void setDexpAttackTime_clampedHigh() {
        TxChannel ch(kChannelId);
        ch.setDexpAttackTime(150.0);
        QCOMPARE(ch.lastDexpAttackTimeForTest(), 100.0);
    }
    void setDexpAttackTime_idempotent() {
        TxChannel ch(kChannelId);
        ch.setDexpAttackTime(2.0);
        ch.setDexpAttackTime(2.0);
        QCOMPARE(ch.lastDexpAttackTimeForTest(), 2.0);
    }

    // --------------------------------------------------------------
    // setDexpReleaseTime  (range 2..1000 ms, default 100)
    // setup.Designer.cs:44967-44995 [v2.10.3.13]
    // --------------------------------------------------------------

    void setDexpReleaseTime_inRange() {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastDexpReleaseTimeForTest()));
        ch.setDexpReleaseTime(100.0);
        QCOMPARE(ch.lastDexpReleaseTimeForTest(), 100.0);
        ch.setDexpReleaseTime(500.0);
        QCOMPARE(ch.lastDexpReleaseTimeForTest(), 500.0);
    }
    void setDexpReleaseTime_clampedLow() {
        TxChannel ch(kChannelId);
        ch.setDexpReleaseTime(0.5);
        QCOMPARE(ch.lastDexpReleaseTimeForTest(), 2.0);
    }
    void setDexpReleaseTime_clampedHigh() {
        TxChannel ch(kChannelId);
        ch.setDexpReleaseTime(2000.0);
        QCOMPARE(ch.lastDexpReleaseTimeForTest(), 1000.0);
    }
    void setDexpReleaseTime_idempotent() {
        TxChannel ch(kChannelId);
        ch.setDexpReleaseTime(100.0);
        ch.setDexpReleaseTime(100.0);
        QCOMPARE(ch.lastDexpReleaseTimeForTest(), 100.0);
    }
};

QTEST_APPLESS_MAIN(TstTxChannelDexpEnvelope)
#include "tst_tx_channel_dexp_envelope.moc"

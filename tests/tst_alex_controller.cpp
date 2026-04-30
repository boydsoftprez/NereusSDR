// no-port-check: test fixture asserts AlexController per-band antenna routing + Block-TX safety + persistence
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/accessories/AlexController.h"
#include "models/Band.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestAlexController : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        AppSettings::instance().clear();
    }

    // Default: every band's TX/RX antenna defaults to port 1;
    // rxOnlyAnt defaults to 0 ("none selected") per Thetis Alex.cs:52 [v2.10.3.13 @501e3f5]
    void default_all_antennas_port1() {
        AlexController a;
        for (int b = int(Band::Band160m); b <= int(Band::XVTR); ++b) {
            QCOMPARE(a.txAnt(Band(b)),     1);
            QCOMPARE(a.rxAnt(Band(b)),     1);
            QCOMPARE(a.rxOnlyAnt(Band(b)), 0);
        }
    }

    // Setting antenna for one band doesn't affect others
    void setTxAnt_isolated_per_band() {
        AlexController a;
        a.setTxAnt(Band::Band20m, 2);
        QCOMPARE(a.txAnt(Band::Band20m), 2);
        QCOMPARE(a.txAnt(Band::Band40m), 1);  // unaffected
    }

    // Block-TX safety: when blockTxAnt2 set, setTxAnt(2) is rejected
    void blockTxAnt2_rejects_setTxAnt() {
        AlexController a;
        a.setBlockTxAnt2(true);
        a.setTxAnt(Band::Band20m, 2);
        QCOMPARE(a.txAnt(Band::Band20m), 1);  // not changed; remained at default
    }

    void blockTxAnt3_rejects_setTxAnt() {
        AlexController a;
        a.setBlockTxAnt3(true);
        a.setTxAnt(Band::Band6m, 3);
        QCOMPARE(a.txAnt(Band::Band6m), 1);
    }

    // Block doesn't affect RX antenna setting
    void blockTxAnt2_does_not_block_setRxAnt() {
        AlexController a;
        a.setBlockTxAnt2(true);
        a.setRxAnt(Band::Band20m, 2);
        QCOMPARE(a.rxAnt(Band::Band20m), 2);
    }

    // Antenna value clamped to 1..3
    void setTxAnt_clamps_to_1_3() {
        AlexController a;
        a.setTxAnt(Band::Band20m, 5);
        QCOMPARE(a.txAnt(Band::Band20m), 3);  // clamped high
        a.setTxAnt(Band::Band20m, 0);
        QCOMPARE(a.txAnt(Band::Band20m), 1);  // clamped low
    }

    // setAntennasTo1 forces TX/RX antennas to port 1; RX-only untouched
    // (Thetis Alex.cs:72-77 — "the various RX 'bypass' unaffected").
    void setAntennasTo1_forces_all_to_port1() {
        AlexController a;
        a.setTxAnt(Band::Band20m, 2);
        a.setRxAnt(Band::Band40m, 3);
        a.setRxOnlyAnt(Band::Band30m, 2);  // RX-only pre-set — must survive
        a.setAntennasTo1(true);
        for (int b = int(Band::Band160m); b <= int(Band::XVTR); ++b) {
            QCOMPARE(a.txAnt(Band(b)), 1);
            QCOMPARE(a.rxAnt(Band(b)), 1);
        }
        QCOMPARE(a.rxOnlyAnt(Band::Band30m), 2);  // RX-only intentionally preserved
    }

    // antennaChanged signal fires on per-band update
    void antennaChanged_signal_fires() {
        AlexController a;
        QSignalSpy spy(&a, &AlexController::antennaChanged);
        a.setTxAnt(Band::Band20m, 2);
        QCOMPARE(spy.count(), 1);
    }

    // Phase 3P-I-a T20 — signal / idempotency / rejection coverage.

    void antennaChanged_fires_with_correct_band() {
        AlexController a;
        QSignalSpy spy(&a, &AlexController::antennaChanged);
        a.setRxAnt(Band::Band20m, 2);
        QCOMPARE(spy.count(), 1);
        const Band fired = spy.takeFirst().at(0).value<Band>();
        QCOMPARE(fired, Band::Band20m);
    }

    void identical_write_emits_no_signal() {
        AlexController a;
        a.setRxAnt(Band::Band20m, 2);  // first write (default was 1)
        QSignalSpy spy(&a, &AlexController::antennaChanged);
        a.setRxAnt(Band::Band20m, 2);  // duplicate
        QCOMPARE(spy.count(), 0);
    }

    void blockTxAnt_rejection_is_silent() {
        AlexController a;
        a.setBlockTxAnt2(true);
        QSignalSpy spy(&a, &AlexController::antennaChanged);
        a.setTxAnt(Band::Band20m, 2);  // rejected
        QCOMPARE(spy.count(), 0);
        QCOMPARE(a.txAnt(Band::Band20m), 1);  // unchanged (default)
    }

    void setAntennasTo1_fires_for_all_bands() {
        AlexController a;
        a.setTxAnt(Band::Band20m, 3);
        a.setRxAnt(Band::Band40m, 2);
        QSignalSpy spy(&a, &AlexController::antennaChanged);
        a.setAntennasTo1(true);
        // setAntennasTo1 unconditionally emits for each band in the
        // 14-band ham range (Band160m..XVTR) regardless of prior value,
        // matching the "applies to all in-memory values" documented
        // behavior in AlexController.cpp.  SWL bands (Band::SwlFirst..
        // SwlLast, Phase 3L Band enum extension) inherit ham antenna
        // routing — they are NOT iterated by setAntennasTo1.
        QCOMPARE(spy.count(), 14);
    }

    // Phase 3P-I-a bench fix — Block-TX toggle retroactively clamps
    // any band currently on the blocked TX antenna down to ANT1, and
    // emits antennaChanged for each affected band so the per-band
    // pump reapplies to the wire. Without this the block is a
    // write-time guard only — pre-existing txAnt=2 values would keep
    // firing on transmit. Mirrors Thetis setup.cs:13237-13248
    // radAlexR_*_CheckedChanged branches. Flagged by Codex review on
    // PR #116.
    void enabling_blockTxAnt2_clamps_existing_txAnt2_bands() {
        AlexController a;
        a.setTxAnt(Band::Band20m, 2);
        a.setTxAnt(Band::Band40m, 3);
        a.setTxAnt(Band::Band15m, 2);
        QSignalSpy spy(&a, &AlexController::antennaChanged);

        a.setBlockTxAnt2(true);

        QCOMPARE(a.txAnt(Band::Band20m), 1);  // was 2, clamped
        QCOMPARE(a.txAnt(Band::Band40m), 3);  // was 3, untouched
        QCOMPARE(a.txAnt(Band::Band15m), 1);  // was 2, clamped
        // Expect two antennaChanged emits — Band20m and Band15m.
        QCOMPARE(spy.count(), 2);
    }

    void enabling_blockTxAnt3_clamps_existing_txAnt3_bands() {
        AlexController a;
        a.setTxAnt(Band::Band20m, 3);
        a.setTxAnt(Band::Band40m, 2);
        QSignalSpy spy(&a, &AlexController::antennaChanged);

        a.setBlockTxAnt3(true);

        QCOMPARE(a.txAnt(Band::Band20m), 1);  // was 3, clamped
        QCOMPARE(a.txAnt(Band::Band40m), 2);  // was 2, untouched
        QCOMPARE(spy.count(), 1);
    }

    // Disabling the block must NOT retroactively change anything —
    // user can now pick ANT2 again, but existing values stay put.
    void disabling_blockTxAnt2_does_not_touch_txAnt() {
        AlexController a;
        a.setBlockTxAnt2(true);
        a.setTxAnt(Band::Band20m, 2);         // rejected → still 1
        QCOMPARE(a.txAnt(Band::Band20m), 1);
        QSignalSpy spy(&a, &AlexController::antennaChanged);

        a.setBlockTxAnt2(false);              // toggle off

        QCOMPARE(spy.count(), 0);              // no antennaChanged on disable
        a.setTxAnt(Band::Band20m, 2);         // now succeeds
        QCOMPARE(a.txAnt(Band::Band20m), 2);
    }

    // ── Phase 3P-I-b flag extension ──────────────────────────────────────────

    void rxOutOnTx_mutualExclusion() {
        AlexController a;
        a.setExt1OutOnTx(true);
        QVERIFY(a.ext1OutOnTx());
        QVERIFY(!a.rxOutOnTx());

        QSignalSpy rxSpy(&a, &AlexController::rxOutOnTxChanged);
        QSignalSpy ext1Spy(&a, &AlexController::ext1OutOnTxChanged);
        a.setRxOutOnTx(true);
        QVERIFY(a.rxOutOnTx());
        QVERIFY(!a.ext1OutOnTx());  // mutual-clear
        QCOMPARE(rxSpy.count(), 1);
        QCOMPARE(ext1Spy.count(), 1);  // fired false when cleared
    }

    void ext1_mutualExclusion_clears_ext2() {
        AlexController a;
        a.setExt2OutOnTx(true);
        QVERIFY(a.ext2OutOnTx());
        QSignalSpy ext2Spy(&a, &AlexController::ext2OutOnTxChanged);
        a.setExt1OutOnTx(true);
        QVERIFY(a.ext1OutOnTx());
        QVERIFY(!a.ext2OutOnTx());
        QCOMPARE(ext2Spy.count(), 1);
    }

    void flags_persist_across_reload() {
        const QString mac = QStringLiteral("aabbccddeeff");
        {
            AlexController a;
            a.setMacAddress(mac);
            a.setExt1OutOnTx(true);
            a.setRxOutOverride(true);
            a.setUseTxAntForRx(true);
            a.save();
        }
        AlexController b;
        b.setMacAddress(mac);
        b.load();
        QVERIFY(b.ext1OutOnTx());
        QVERIFY(b.rxOutOverride());
        QVERIFY(b.useTxAntForRx());
        QVERIFY(!b.rxOutOnTx());
    }

    void xvtrActive_session_scoped_no_persist() {
        const QString mac = QStringLiteral("ddeeff001122");
        {
            AlexController a;
            a.setMacAddress(mac);
            QSignalSpy spy(&a, &AlexController::xvtrActiveChanged);
            a.setXvtrActive(true);
            QCOMPARE(spy.count(), 1);
            a.save();
        }
        AlexController b;
        b.setMacAddress(mac);
        b.load();
        QVERIFY(!b.xvtrActive());  // session-scoped; not persisted
    }

    void rxOnlyAntChanged_fine_signal() {
        AlexController a;
        QSignalSpy fineSpy(&a, &AlexController::rxOnlyAntChanged);
        QSignalSpy coarseSpy(&a, &AlexController::antennaChanged);
        a.setRxOnlyAnt(Band::Band20m, 2);
        QCOMPARE(fineSpy.count(), 1);
        QCOMPARE(coarseSpy.count(), 1);
        QCOMPARE(fineSpy.at(0).at(0).value<Band>(), Band::Band20m);
    }

    void rxOnlyAnt_allows_zero() {
        // Phase 3P-I-b fix: Thetis Alex.cs:58 uses 0 for "none selected".
        // Prior 3P-I-a implementation clamped 0 → 1, breaking composition.
        AlexController a;
        QCOMPARE(a.rxOnlyAnt(Band::Band20m), 0);  // default now 0
        a.setRxOnlyAnt(Band::Band20m, 2);
        QCOMPARE(a.rxOnlyAnt(Band::Band20m), 2);
        a.setRxOnlyAnt(Band::Band20m, 0);
        QCOMPARE(a.rxOnlyAnt(Band::Band20m), 0);  // round-trip through 0
    }

    void rxOnlyAnt_clamps_high() {
        AlexController a;
        a.setRxOnlyAnt(Band::Band20m, 5);
        QCOMPARE(a.rxOnlyAnt(Band::Band20m), 3);  // high clamp unchanged
    }

    // Per-MAC persistence round-trip
    void persistence_roundtrip() {
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:ff");
        AlexController a1;
        a1.setMacAddress(mac);
        // Use ANT3 here, not ANT2 — enabling blockTxAnt2 below would
        // retroactively clamp any existing ANT2 assignment down to
        // ANT1 per Thetis setup.cs:13237. (Pre-Codex-review versions
        // of this test relied on the old "flag-only" semantics where
        // blockTxAnt2 was a write-time guard with no retroactive
        // effect.)
        a1.setTxAnt(Band::Band20m, 3);
        a1.setRxOnlyAnt(Band::Band40m, 3);
        a1.setBlockTxAnt2(true);
        a1.save();

        AlexController a2;
        a2.setMacAddress(mac);
        a2.load();
        QCOMPARE(a2.txAnt(Band::Band20m), 3);
        QCOMPARE(a2.rxOnlyAnt(Band::Band40m), 3);
        QVERIFY(a2.blockTxAnt2());
    }
};

QTEST_APPLESS_MAIN(TestAlexController)
#include "tst_alex_controller.moc"

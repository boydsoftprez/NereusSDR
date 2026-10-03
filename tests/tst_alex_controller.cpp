// no-port-check: test fixture asserts AlexController per-band antenna routing + Block-TX safety + persistence
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/accessories/AlexController.h"
#include "core/accessories/AlexAntennaFacade.h"
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
        // setAntennasTo1 unconditionally emits for each band with antennas
        // of its own (Band160m..XVTR and 2 m) regardless of prior value,
        // matching the "applies to all in-memory values" documented
        // behavior in AlexController.cpp.  SWL bands (Band::SwlFirst..
        // SwlLast, Phase 3L Band enum extension) inherit ham antenna
        // routing — they are NOT iterated by setAntennasTo1.
        QCOMPARE(spy.count(), 15);
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
    // ── R-R3-46: the mirrored `alexAntennas` object ─────────────────────────

    // Bound (the Core, a local window): receive edits go through the
    // controller, per band, and the object shows what the controller keeps.
    void facade_bound_applies_receive_edits_through_the_controller() {
        AlexController a;
        AlexAntennaFacade f;
        f.bindController(&a);
        QVERIFY(f.isBound());
        // 15 entries: 160m .. XVTR, then 2 m.
        QCOMPARE(f.rxAntennas(), QStringLiteral("1,1,1,1,1,1,1,1,1,1,1,1,1,1,1"));
        QCOMPARE(f.rxOnlyAntennas(), QStringLiteral("0,0,0,0,0,0,0,0,0,0,0,0,0,0,0"));

        QSignalSpy changed(&a, &AlexController::antennaChanged);
        f.setRxAnt(Band::Band40m, 2);
        QCOMPARE(a.rxAnt(Band::Band40m), 2);
        QCOMPARE(a.rxAnt(Band::Band20m), 1);
        QCOMPARE(changed.count(), 1);  // only the band that changed
        QCOMPARE(f.rxAnt(Band::Band40m), 2);
        QVERIFY(f.settleReason("rxAntennas").isEmpty());

        f.setRxOnlyAnt(Band::Band20m, 3);
        QCOMPARE(a.rxOnlyAnt(Band::Band20m), 3);
        QCOMPARE(f.rxOnlyAnt(Band::Band20m), 3);

        f.setUseTxAntennaForRx(true);
        QVERIFY(a.useTxAntForRx());
        QVERIFY(f.useTxAntennaForRx());
    }

    // A value the controller cannot take settles with a plain reason.
    void facade_bound_settles_out_of_range_with_a_reason() {
        AlexController a;
        AlexAntennaFacade f;
        f.bindController(&a);
        f.setRxAntennas(QStringLiteral("5,1,1,1,1,1,1,1,1,1,1,1,1,1"));
        QCOMPARE(a.rxAnt(Band::Band160m), 3);
        QCOMPARE(f.settleReason("rxAntennas"), QStringLiteral("Antennas are numbered 1 to 3."));

        f.setRxAntennas(QStringLiteral("2,2"));
        QCOMPARE(a.rxAnt(Band::Band160m), 3);  // a short list changes nothing
        QCOMPARE(f.settleReason("rxAntennas"),
                 QStringLiteral("The Core keeps one antenna for each of its bands."));

        f.setRxOnlyAntennas(QStringLiteral("-1,0,0,0,0,0,0,0,0,0,0,0,0,0"));
        QCOMPARE(a.rxOnlyAnt(Band::Band160m), 0);
        QCOMPARE(f.settleReason("rxOnlyAntennas"),
                 QStringLiteral("The receive-only input is none or 1 to 3."));
    }

    // The transmit settings are reported as the controller holds them.
    void facade_bound_reports_the_transmit_settings() {
        AlexController a;
        AlexAntennaFacade f;
        f.bindController(&a);
        QSignalSpy tx(&f, &AlexAntennaFacade::txAntennasChanged);
        a.setTxAnt(Band::Band20m, 3);
        QCOMPARE(tx.count(), 1);
        QCOMPARE(f.txAnt(Band::Band20m), 3);
        a.setBlockTxAnt2(true);
        QVERIFY(f.blockTxAnt2());
        a.setExt1OutOnTx(true);
        QVERIFY(f.ext1OutOnTx());
        a.setRxOutOverride(true);
        QVERIFY(f.rxOutOverride());
    }

    // Unbound (a remote window): the gate may refuse an edit, with its
    // reason, and nothing changes; the Core's transmit values arrive as
    // reported properties, and only those.
    void facade_unbound_follows_the_gate_and_the_cores_values() {
        AlexAntennaFacade f;
        QVERIFY(!f.isBound());
        bool allow = false;
        f.setEditGate([&allow](QString* reason) {
            if (!allow && reason) {
                *reason = QStringLiteral("Connect to the Core to change the radio's hardware settings.");
            }
            return allow;
        });
        QSignalSpy refused(&f, &AlexAntennaFacade::editRejected);
        QSignalSpy rx(&f, &AlexAntennaFacade::rxAntennasChanged);
        f.setRxAnt(Band::Band40m, 2);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(rx.count(), 0);
        QCOMPARE(f.rxAnt(Band::Band40m), 1);

        allow = true;
        f.setRxAnt(Band::Band40m, 2);
        QCOMPARE(rx.count(), 1);
        QCOMPARE(f.rxAnt(Band::Band40m), 2);

        QVERIFY(f.applyRemoteProperty("txAntennas",
                                      QStringLiteral("2,2,2,2,2,2,2,2,2,2,2,2,2,3")));
        QCOMPARE(f.txAnt(Band::XVTR), 3);
        // A Core that knows 2 m sends 15 entries, 2 m's last; a 14-entry
        // list (above, a Core built before 2 m) keeps 2 m's value.
        QCOMPARE(f.txAnt(Band::Band2m), 1);
        QVERIFY(f.applyRemoteProperty("txAntennas",
                                      QStringLiteral("2,2,2,2,2,2,2,2,2,2,2,2,2,3,3")));
        QCOMPARE(f.txAnt(Band::Band2m), 3);
        QVERIFY(f.applyRemoteProperty("txAntennas",
                                      QStringLiteral("1,1,1,1,1,1,1,1,1,1,1,1,1,1")));
        QCOMPARE(f.txAnt(Band::Band2m), 3);
        QVERIFY(f.applyRemoteProperty("blockTxAnt3", true));
        QVERIFY(f.blockTxAnt3());
        QVERIFY(!f.applyRemoteProperty("rxAntennas", QStringLiteral("3,3")));

        AlexController a;
        f.bindController(&a);
        QVERIFY(!f.applyRemoteProperty("txAntennas", QStringLiteral("1,1")));
    }

    // R-R3-46 / R-R3-21 (radioHardwareVersion 4): the Core applies a remote
    // window's filter policy through the controller's setBpfMode, the call
    // the local filter policy dialog makes. A slice on 40 m sits on ADC0, so
    // the forced filter is that band's.
    void facade_bound_applies_the_filter_policy_as_the_local_dialog_does() {
        AlexController core;
        core.notifySlicesOnAdc(0, {Band::Band40m, Band::Count, Band::Count, Band::Count,
                                   Band::Count});
        QCOMPARE(core.adcState(0).effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(core.adcState(0).reasonText, QStringLiteral("40m"));
        AlexAntennaFacade f;
        f.bindController(&core);
        QSignalSpy applied(&core, &AlexController::bpfModeChanged);

        QCOMPARE(f.setBpfModeForChain(0, int(AlexController::BpfMode::ForceBand)), QString());
        QCOMPARE(core.bpfMode(0), AlexController::BpfMode::ForceBand);
        QCOMPARE(core.adcState(0).effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(core.adcState(0).currentBpfBand, Band::Band40m);
        QCOMPARE(core.adcState(0).reasonText, QStringLiteral("40m (forced)"));

        QCOMPARE(f.setBpfModeForChain(0, int(AlexController::BpfMode::ForceBypass)), QString());
        QCOMPARE(core.bpfMode(0), AlexController::BpfMode::ForceBypass);
        QCOMPARE(core.adcState(0).effective, AlexController::BpfEffective::Bypass);
        QCOMPARE(core.adcState(0).reasonText, QStringLiteral("BYPASS (operator override)"));

        QCOMPARE(f.setBpfModeForChain(0, int(AlexController::BpfMode::Auto)), QString());
        QCOMPARE(core.bpfMode(0), AlexController::BpfMode::Auto);
        QCOMPARE(core.adcState(0).effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(core.adcState(0).reasonText, QStringLiteral("40m"));
        QCOMPARE(applied.count(), 3);
        QCOMPARE(applied.last().at(0).toInt(), 0);

        // The policy it already has: taken, nothing to save.
        QCOMPARE(f.setBpfModeForChain(0, int(AlexController::BpfMode::Auto)), QString());
        QCOMPARE(applied.count(), 3);

        // The second chain on its own.
        QCOMPARE(f.setBpfModeForChain(1, int(AlexController::BpfMode::ForceBypass)), QString());
        QCOMPARE(core.bpfMode(1), AlexController::BpfMode::ForceBypass);
        QCOMPARE(core.bpfMode(0), AlexController::BpfMode::Auto);
        QCOMPARE(applied.last().at(0).toInt(), 1);

        // A wideband chain stays bypassed, but its policy still changes and
        // is still announced (so the Core saves it).
        core.setWidebandActive(1, true);
        const int before = applied.count();
        QCOMPARE(f.setBpfModeForChain(1, int(AlexController::BpfMode::ForceBand)), QString());
        QCOMPARE(core.bpfMode(1), AlexController::BpfMode::ForceBand);
        QCOMPARE(core.adcState(1).effective, AlexController::BpfEffective::WidebandLocked);
        QCOMPARE(applied.count(), before + 1);
    }

    void facade_filter_policy_refuses_what_the_controller_cannot_take() {
        AlexAntennaFacade unbound;
        QVERIFY(!unbound.setBpfModeForChain(0, 1).isEmpty());

        AlexController core;
        AlexAntennaFacade f;
        f.bindController(&core);
        QSignalSpy applied(&core, &AlexController::bpfModeChanged);
        for (const auto& [chain, mode] : {std::pair{2, 1}, std::pair{-1, 1},
                                           std::pair{0, 3}, std::pair{0, -1}}) {
            const QString reason = f.setBpfModeForChain(chain, mode);
            QVERIFY2(!reason.isEmpty(), qPrintable(QStringLiteral("%1/%2").arg(chain).arg(mode)));
        }
        QCOMPARE(core.bpfMode(0), AlexController::BpfMode::Auto);
        QCOMPARE(core.bpfMode(1), AlexController::BpfMode::Auto);
        QCOMPARE(applied.count(), 0);
    }
};

QTEST_APPLESS_MAIN(TestAlexController)
#include "tst_alex_controller.moc"

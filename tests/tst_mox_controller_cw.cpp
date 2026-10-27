// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_mox_controller_cw.cpp  (NereusSDR)
// =================================================================
//
// CW transmit (R-CW-09, R-CW-18 to R-CW-20): MOX in CW on a bare
// MoxController. It checks the controller follows Thetis
// (Project Files/Source/Console/console.cs [v2.10.3.15]):
//   PollPTT's CW branch, cw_ptt in Semi only   25473, 25519-25524
//   PTTMode.CW's release                       25596-25602
//   QSKEnabled skips keying                    25470
//   getFallbackPTTModeAfterTCIRelease, CW      25429-25461
//   chkMOX_CheckedChanged2 in CW: no rf_delay  29640-29649
//     and, on the way back, no key-up or MOX
//     delay, ptt_out_delay kept               29653-29680
//
// Nothing here opens a radio socket or keys a radio: the controller's
// phase signals are observed, nothing listens to them.
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, MOX in CW, break-in and QSK. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/CwTypes.h"
#include "core/MoxController.h"
#include "core/PttMode.h"
#include "core/WdspTypes.h"
#include "core/safety/TxRefusal.h"

using namespace NereusSDR;

namespace {

void drain()
{
    for (int i = 0; i < 4; ++i) {
        QCoreApplication::processEvents();
    }
}

// The radio's status frames repeat the PTT level; a few frames stand in
// for a press held.
void pttFrames(MoxController& ctrl, bool pressed, int frames = 3)
{
    for (int i = 0; i < frames; ++i) {
        ctrl.onMicPttFromRadio(pressed);
        drain();
    }
}

void putInCw(MoxController& ctrl, CwBreakInMode mode)
{
    ctrl.onModeChanged(DSPMode::CWU);
    ctrl.setCwModeActive(true);
    ctrl.setCwBreakInMode(mode);
}

TxRefusal gateWords()
{
    return TxRefusals::bandPlan(QStringLiteral("Transmit is outside the band plan."));
}

// Every phase signal, in the order sent.
struct PhaseLog {
    QStringList order;
    QList<MoxState> states;

    explicit PhaseLog(MoxController& c)
    {
        QObject::connect(&c, &MoxController::txAboutToBegin, [this]() { order << "txAboutToBegin"; });
        QObject::connect(&c, &MoxController::hardwareFlipped,
                         [this](bool tx) { order << (tx ? "hardwareFlipped(true)" : "hardwareFlipped(false)"); });
        QObject::connect(&c, &MoxController::txReady, [this]() { order << "txReady"; });
        QObject::connect(&c, &MoxController::txAboutToEnd, [this]() { order << "txAboutToEnd"; });
        QObject::connect(&c, &MoxController::txDrainRequested, [this]() { order << "txDrainRequested"; });
        QObject::connect(&c, &MoxController::txaFlushed, [this]() { order << "txaFlushed"; });
        QObject::connect(&c, &MoxController::rxReady, [this]() { order << "rxReady"; });
        QObject::connect(&c, &MoxController::stateChanged, [this](MoxState s) { states << s; });
    }
    void clear()
    {
        order.clear();
        states.clear();
    }
};

} // namespace

class TestMoxControllerCw : public QObject {
    Q_OBJECT

private slots:

    // ── Semi: the radio's PTT keys MOX as the radio's key, no rf_delay ──────
    void semi_radioPtt_keysCwAtOnce_andReleasesWithOnlyPttOutDelay()
    {
        MoxController ctrl;   // Thetis's own delays: 30, 10, 0, 10, 20 ms
        putInCw(ctrl, CwBreakInMode::Semi);
        // Everything the voice unkey waits for is armed, so a wait would show.
        ctrl.setAwaitsTxDrain(true);
        int tailAsked = 0;
        ctrl.setEndOfOverTail([&tailAsked]() { ++tailAsked; return true; });
        ctrl.setSendRingDrain({[]() { return true; }, []() { return false; }, []() { return 500.0; }});
        PhaseLog log(ctrl);
        QSignalSpy moxOn(&ctrl, &MoxController::moxStateChanged);

        ctrl.onMicPttFromRadio(true);
        // No rf_delay: the whole key-down walk ran inside the call.
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.state(), MoxState::Tx);
        QCOMPARE(ctrl.pttMode(), PttMode::Cw);
        QVERIFY(ctrl.currentKeyer() == KeyerIdentity::radioKey());
        QVERIFY(ctrl.currentKeyer().isStation());
        QVERIFY(!ctrl.currentKeyer().program);
        QCOMPARE(ctrl.currentKeyer().source, PttMode::Cw);
        QVERIFY(ctrl.isCwKeying());
        QCOMPARE(log.order, (QStringList{"txAboutToBegin", "hardwareFlipped(true)", "txReady"}));
        QCOMPARE(log.states, (QList<MoxState>{MoxState::Tx}));
        QCOMPARE(moxOn.count(), 1);
        QCOMPARE(moxOn.at(0).at(0).toBool(), true);

        // Held: the repeated frames change nothing.
        pttFrames(ctrl, true);
        QCOMPARE(log.order.size(), 3);

        log.clear();
        ctrl.onMicPttFromRadio(false);
        // No drain, no send ring, no tail, no key-up or MOX delay: the walk
        // is in ptt_out_delay at once, the hardware already released.
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.state(), MoxState::TxToRxFlush);
        QCOMPARE(log.order, (QStringList{"txAboutToEnd", "txDrainRequested", "txaFlushed",
                                         "hardwareFlipped(false)"}));
        QCOMPARE(tailAsked, 0);
        QVERIFY(!ctrl.isSendRingWaitActive());
        QVERIFY(ctrl.isCwKeying());   // the release is still walking
        // ptt_out_delay is kept: receive comes back only after it.
        QTRY_COMPARE_WITH_TIMEOUT(ctrl.state(), MoxState::Rx, 1000);
        QCOMPARE(log.order.last(), QStringLiteral("rxReady"));
        QCOMPARE(log.states, (QList<MoxState>{MoxState::TxToRxInFlight, MoxState::TxToRxFlush,
                                              MoxState::Rx}));
        QCOMPARE(ctrl.pttMode(), PttMode::None);
        QVERIFY(!ctrl.isCwKeying());
    }

    void semi_pttOutDelayIsTheOnlyWaitOnTheWayBack()
    {
        MoxController ctrl;
        // rf 500, MOX 500, key-up 500, ptt_out 0: only a skipped delay could
        // be long, so the release reaching receive at once proves it.
        ctrl.setTimerIntervals(500, 500, 0, 500, 0, 0);
        putInCw(ctrl, CwBreakInMode::Semi);
        ctrl.onMicPttFromRadio(true);
        QCOMPARE(ctrl.state(), MoxState::Tx);
        ctrl.onMicPttFromRadio(false);
        QCOMPARE(ctrl.state(), MoxState::TxToRxFlush);
        drain();
        QCOMPARE(ctrl.state(), MoxState::Rx);
    }

    // ── Off: the radio's PTT keys too; the serial PTT line only in Semi ─────
    void off_radioPttKeys_serialPttDoesNot()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        putInCw(ctrl, CwBreakInMode::Off);

        ctrl.onCwPtt(true);
        drain();
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);
        ctrl.onCwPtt(false);

        pttFrames(ctrl, true);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Cw);
        QVERIFY(ctrl.currentKeyer() == KeyerIdentity::radioKey());
        pttFrames(ctrl, false);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.state(), MoxState::Rx);
    }

    void semi_serialPttKeys_andTheKeyEndsWhenBothAreUp()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        putInCw(ctrl, CwBreakInMode::Semi);

        ctrl.onCwPtt(true);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Cw);
        QVERIFY(ctrl.currentKeyer() == KeyerIdentity::radioKey());
        QVERIFY(ctrl.anyPttSourceHeld());

        // The radio's PTT down too: releasing the serial line keeps the key.
        pttFrames(ctrl, true);
        ctrl.onCwPtt(false);
        drain();
        QVERIFY(ctrl.isMox());
        pttFrames(ctrl, false);
        QVERIFY(!ctrl.isMox());
        drain();
        QCOMPARE(ctrl.state(), MoxState::Rx);
    }

    // ── QSK: no PTT source keys ──────────────────────────────────────────────
    void qsk_noPttSourceKeys()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setVoxEnabled(true);
        putInCw(ctrl, CwBreakInMode::Qsk);
        QSignalSpy moxSpy(&ctrl, &MoxController::moxStateChanged);
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);

        pttFrames(ctrl, true);
        ctrl.onCwPtt(true);
        ctrl.onCatPtt(true);
        ctrl.onTciPtt(true);
        ctrl.onVoxActive(true);
        drain();
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.state(), MoxState::Rx);
        QCOMPARE(moxSpy.count(), 0);
        QCOMPARE(rejected.count(), 0);

        // The same sources in a voice mode key as before.
        ctrl.onCatPtt(false);
        ctrl.onTciPtt(false);
        ctrl.onCwPtt(false);
        ctrl.onVoxActive(false);
        pttFrames(ctrl, false);
        ctrl.setCwModeActive(false);
        ctrl.onModeChanged(DSPMode::USB);
        pttFrames(ctrl, true);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
    }

    void qsk_aKeyAlreadyOnStillEndsOnItsRelease()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        putInCw(ctrl, CwBreakInMode::Semi);
        ctrl.onCatPtt(true);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Cat);
        QVERIFY(ctrl.isCwKeying());

        ctrl.setCwBreakInMode(CwBreakInMode::Qsk);
        ctrl.onCatPtt(false);
        drain();
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.state(), MoxState::Rx);
    }

    // ── The keyer gate refuses: once per hold, nothing keys ─────────────────
    void gateRefused_radioPtt_isReportedOncePerHold()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        putInCw(ctrl, CwBreakInMode::Semi);
        ctrl.setCwKeyerGate([]() { return gateWords(); });
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);
        QSignalSpy refused(&ctrl, &MoxController::moxRefused);

        pttFrames(ctrl, true, 5);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), gateWords().text);
        QCOMPARE(refused.count(), 1);
        QVERIFY(refused.at(0).at(0).value<TxRefusal>() == gateWords());

        // Released and pressed again: a new hold, told again.
        pttFrames(ctrl, false);
        pttFrames(ctrl, true);
        QCOMPARE(rejected.count(), 2);
        QVERIFY(!ctrl.isMox());

        // The gate allows: the held press keys on the next frame.
        ctrl.setCwKeyerGate({});
        pttFrames(ctrl, true, 1);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Cw);
    }

    void gateRefused_dotOrDash_isReportedOncePerHold()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        putInCw(ctrl, CwBreakInMode::Qsk);
        ctrl.setCwKeyerGate([]() { return gateWords(); });
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);

        ctrl.onCwKeyStatus(true, false);
        ctrl.onCwKeyStatus(true, false);
        ctrl.onCwKeyStatus(false, true);   // still held: dash after dot
        ctrl.onCwKeyStatus(true, true);
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), gateWords().text);
        QVERIFY(!ctrl.isMox());

        ctrl.onCwKeyStatus(false, false);
        ctrl.onCwKeyStatus(false, true);
        QCOMPARE(rejected.count(), 2);

        // Allowed: a dot is the radio's keyer's, nothing is said or keyed.
        ctrl.onCwKeyStatus(false, false);
        ctrl.setCwKeyerGate({});
        ctrl.onCwKeyStatus(true, false);
        QCOMPARE(rejected.count(), 2);
        QVERIFY(!ctrl.isMox());
    }

    void reportCwRefusal_withNoKeyHeld_alwaysSpeaks()
    {
        MoxController ctrl;
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);
        QSignalSpy refused(&ctrl, &MoxController::moxRefused);
        ctrl.reportCwRefusal(gateWords());
        ctrl.reportCwRefusal(gateWords());
        QCOMPARE(rejected.count(), 2);
        QCOMPARE(refused.count(), 2);
        QVERIFY(ctrl.lastRefusal() == gateWords());
    }

    void heldKey_aDifferentRefusalIsStillSaid()
    {
        MoxController ctrl;
        putInCw(ctrl, CwBreakInMode::Semi);
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);
        ctrl.onCwKeyStatus(true, false);
        ctrl.reportCwRefusal(gateWords());
        ctrl.reportCwRefusal(gateWords());
        QCOMPARE(rejected.count(), 1);
        ctrl.reportCwRefusal(TxRefusals::bandPlan(QStringLiteral("QSK is off on transverter bands.")));
        QCOMPARE(rejected.count(), 2);
    }

    // ── The radio's PTT is the CW key in CW, never the mic ───────────────────
    void tciReleaseInCw_fallsBackToTheRadiosKey()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        putInCw(ctrl, CwBreakInMode::Semi);
        ctrl.onTciPtt(true);
        drain();
        QCOMPARE(ctrl.pttMode(), PttMode::Tci);
        // The radio's PTT held while TCI keys: the key stays TCI's.
        pttFrames(ctrl, true);
        QCOMPARE(ctrl.pttMode(), PttMode::Tci);
        ctrl.onTciPtt(false);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Cw);
        pttFrames(ctrl, false);
        QVERIFY(!ctrl.isMox());
    }

    // ── TUNE in CW keeps the voice walk (it runs the TX channel) ─────────────
    void tuneInCw_keepsTheVoiceWalk()
    {
        MoxController ctrl;   // Thetis's delays
        putInCw(ctrl, CwBreakInMode::Semi);
        ctrl.setTune(true);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.state(), MoxState::RxToTxRfDelay);
        QVERIFY(!ctrl.isCwKeying());
        QTRY_COMPARE_WITH_TIMEOUT(ctrl.state(), MoxState::Tx, 1000);
        ctrl.setTune(false);
        QTRY_COMPARE_WITH_TIMEOUT(ctrl.state(), MoxState::Rx, 1000);
    }

    // ── The MOX button in CW: the CW walk ────────────────────────────────────
    void moxButtonInCw_walksWithoutTheTxChannel()
    {
        MoxController ctrl;
        putInCw(ctrl, CwBreakInMode::Off);
        ctrl.onMoxButton(true);
        QCOMPARE(ctrl.state(), MoxState::Tx);
        QVERIFY(ctrl.isCwKeying());
        ctrl.onMoxButton(false);
        QCOMPARE(ctrl.state(), MoxState::TxToRxFlush);
        QTRY_COMPARE_WITH_TIMEOUT(ctrl.state(), MoxState::Rx, 1000);
    }

    // ── Outside CW nothing changes ───────────────────────────────────────────
    void voiceMode_radioPtt_isTheMicWithRfDelay()
    {
        MoxController ctrl;
        ctrl.onModeChanged(DSPMode::USB);
        ctrl.setCwBreakInMode(CwBreakInMode::Qsk);   // ignored outside CW
        ctrl.onMicPttFromRadio(true);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
        QCOMPARE(ctrl.state(), MoxState::RxToTxRfDelay);
        QVERIFY(!ctrl.isCwKeying());
        ctrl.onCwPtt(true);   // the serial PTT line is CW only
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
        QTRY_COMPARE_WITH_TIMEOUT(ctrl.state(), MoxState::Tx, 1000);
        ctrl.onCwPtt(false);
        ctrl.onMicPttFromRadio(false);
        QCOMPARE(ctrl.state(), MoxState::TxToRxInFlight);
        QTRY_COMPARE_WITH_TIMEOUT(ctrl.state(), MoxState::Rx, 1000);
    }
};

QTEST_GUILESS_MAIN(TestMoxControllerCw)
#include "tst_mox_controller_cw.moc"

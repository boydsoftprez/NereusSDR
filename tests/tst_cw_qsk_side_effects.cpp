// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_cw_qsk_side_effects.cpp  (NereusSDR)
// =================================================================
//
// CW transmit (R-CW-09, R-CW-10, R-CW-18 to R-CW-22): MOX in CW on a
// RadioModel with a fake connection and a TX channel that is never run on
// WDSP. It checks:
//   - QSK's changes to the transmit slice's receiver (Thetis QSKEnabled,
//     console.cs:13013-13041 [v2.10.3.15]): AGC Custom, a hang threshold
//     below 70 raised to 100, the values saved under hardware/<mac>/tx/
//     and put back on leaving QSK, also after a restart; the break-in
//     delay setting is never written;
//   - leaving CW with QSK selected sets Off and remembers QSK
//     (console.cs:34566-34581);
//   - a QSK fallback says why once;
//   - a key in CW never starts the TX channel;
//   - the keyer gate's refusal of a dot, dash or the radio's PTT, once per
//     hold;
//   - the QSK time-out fires once per hold.
// Nothing here opens a radio socket or keys a radio.
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, MOX in CW, break-in and QSK. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
//   2026-10-26: CW transmit early fix wave: the restore keys once the
//               radio is known (M7), the fallback notice's code (M5), the
//               QSK time-out's stretch (I4). J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
//   2026-10-26: CW transmit, leaving CW with QSK selected lands on Off, as
//               Thetis does. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include <atomic>

#include "core/AppSettings.h"
#include "core/CwTypes.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "core/safety/CwKeyerGate.h"
#include "core/safety/TxRefusal.h"
#include "core/safety/TxTimeOutTimer.h"
#include "models/CwKeyerController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("00:1c:c0:a2:13:dd");
const QString kPfx = QStringLiteral("hardware/%1/tx/").arg(kMac);

class CwMockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit CwMockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void reportKey(bool dot, bool dash, bool keyOut) { emit radioKeyStatus(dot, dash, keyOut); }
};

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

RadioInfo saturn()
{
    RadioInfo info;
    info.protocol = ProtocolVersion::Protocol2;
    info.boardType = HPSDRHW::Saturn;
    info.firmwareVersion = 10;
    info.macAddress = kMac;
    return info;
}

RadioInfo hermesP1()
{
    RadioInfo info = saturn();
    info.protocol = ProtocolVersion::Protocol1;
    info.boardType = HPSDRHW::Hermes;
    return info;
}

QString setting(const char* key)
{
    return AppSettings::instance().value(kPfx + QLatin1String(key)).toString();
}

// How a rig starts: the break-in selected, the slice's AGC, and settings a
// restart left behind (written after the clear, before anything runs).
struct Start {
    CwBreakInMode breakIn{CwBreakInMode::Semi};
    AGCMode agc{AGCMode::Med};
    int hangThreshold{50};
    std::function<void()> seed;
};

// A local model on a P2 Saturn with a fake connection and a TX channel
// that only records its RF gate: one slice on 20 m CWU, by default AGC Med
// with a hang threshold of 50 and break-in Semi.
struct Rig {
    RadioModel model;
    CwMockConnection conn;
    TxChannel tx{WdspEngine::kTxChannelId};
    std::atomic<int> rfOpens{0};
    qint64 clockMs{0};
    SliceModel* slice{nullptr};

    explicit Rig(const Start& start = Start{})
    {
        AppSettings::instance().clear();
        if (start.seed) {
            start.seed();
        }
        model.setCapsForTest(/*hasAlex=*/false);
        model.injectConnectionForTest(&conn);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        if (TxTimeOutTimer* tot = model.txTimeOutTimer()) {
            tot->setClock([this]() { return clockMs; });
            tot->setSettingsSource([]() {
                TxTimeOutTimer::Settings s;
                s.moxEnabled = true;
                s.moxSeconds = TxTimeOutTimer::kMinimumSeconds;
                return s;
            });
        }
        model.injectTxChannelForTest(&tx);
        model.wireTxChannelKeyingForTest();
        tx.setRfGateObserverForTest([this](bool open) {
            if (open) {
                ++rfOpens;
            }
        });
        slice = model.sliceById(model.addSlice());
        slice->setFrequency(14'050'000.0);
        slice->setAgcMode(start.agc);
        slice->setAgcHangThreshold(start.hangThreshold);
        slice->setDspMode(DSPMode::CWU);
        tm().setCwBreakInMode(static_cast<int>(start.breakIn));
        tm().setCwBreakInDelayMs(400);
        // The radio becomes known last, so the slice above is set up first
        // (a restart: the slice is restored before the radio's settings).
        model.setLastRadioInfoForTest(saturn());
        model.wireCwKeyerConnectionForTest();
        pump();
    }
    ~Rig()
    {
        tx.setRfGateObserverForTest({});
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }

    TransmitModel& tm() { return model.transmitModel(); }
    CwKeyerController* keyer() { return model.cwKeyerController(); }
    MoxController* mox() { return model.moxController(); }
    void selectBreakIn(CwBreakInMode mode)
    {
        tm().setCwBreakInMode(static_cast<int>(mode));
        pump();
    }
    void key(bool dot, bool dash, bool keyOut)
    {
        conn.reportKey(dot, dash, keyOut);
        pump();
    }
    void radioPtt(bool pressed, int frames = 3)
    {
        for (int i = 0; i < frames; ++i) {
            mox()->onMicPttFromRadio(pressed);
            pump(2);
        }
    }
};

} // namespace

class TestCwQskSideEffects : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── R-CW-21: entering and leaving QSK ────────────────────────────────
    void qskEnter_appliesCustomAgcAndRaisesALowHangThreshold_leaveRestores()
    {
        Rig rig;
        QVERIFY2(rig.keyer()->keyerAllowed(), qPrintable(rig.keyer()->refusal().text));
        QVERIFY(rig.mox()->isCwModeActive());
        QCOMPARE(rig.mox()->cwBreakInMode(), CwBreakInMode::Semi);
        QVERIFY(!rig.model.cwQskSideEffectsAppliedForTest());
        QSignalSpy delayChanged(&rig.tm(), &TransmitModel::cwBreakInDelayMsChanged);

        rig.selectBreakIn(CwBreakInMode::Qsk);
        QCOMPARE(rig.keyer()->effectiveBreakInMode(), CwBreakInMode::Qsk);
        QCOMPARE(rig.mox()->cwBreakInMode(), CwBreakInMode::Qsk);
        QVERIFY(rig.model.cwQskSideEffectsAppliedForTest());
        QCOMPARE(rig.slice->agcMode(), AGCMode::Custom);
        QCOMPARE(rig.slice->agcHangThreshold(), 100);
        QCOMPARE(setting("CW_QskSavedAgcMode"), QString::number(static_cast<int>(AGCMode::Med)));
        QCOMPARE(setting("CW_QskSavedHangThreshold"), QStringLiteral("50"));
        QCOMPARE(setting("CW_QskActive"), QStringLiteral("True"));

        rig.selectBreakIn(CwBreakInMode::Semi);
        QVERIFY(!rig.model.cwQskSideEffectsAppliedForTest());
        QCOMPARE(rig.slice->agcMode(), AGCMode::Med);
        QCOMPARE(rig.slice->agcHangThreshold(), 50);
        QCOMPARE(setting("CW_QskActive"), QStringLiteral("False"));

        // The break-in delay setting is never written.
        QCOMPARE(delayChanged.count(), 0);
        QCOMPARE(rig.tm().cwBreakInDelayMs(), 400);
        QVERIFY(!AppSettings::instance().contains(kPfx + QStringLiteral("CW_BreakInDelay")));
    }

    void qskEnter_aHangThresholdOf80Stays80()
    {
        Rig rig;
        rig.slice->setAgcHangThreshold(80);
        rig.selectBreakIn(CwBreakInMode::Qsk);
        QCOMPARE(rig.slice->agcMode(), AGCMode::Custom);
        QCOMPARE(rig.slice->agcHangThreshold(), 80);
        QCOMPARE(setting("CW_QskSavedHangThreshold"), QStringLiteral("80"));
        rig.selectBreakIn(CwBreakInMode::Off);
        QCOMPARE(rig.slice->agcMode(), AGCMode::Med);
        QCOMPARE(rig.slice->agcHangThreshold(), 80);
    }

    void restart_qskNoLongerSelected_restoresTheSavedValuesAtLoad()
    {
        // The last session left QSK on: the receiver was changed and its
        // values saved. This one starts in Semi.
        Start start;
        start.agc = AGCMode::Custom;   // the restored slice still carries QSK's values
        start.hangThreshold = 100;
        start.seed = []() {
            AppSettings& s = AppSettings::instance();
            s.setValue(kPfx + QStringLiteral("CW_QskActive"), QStringLiteral("True"));
            s.setValue(kPfx + QStringLiteral("CW_QskSavedAgcMode"),
                       QString::number(static_cast<int>(AGCMode::Slow)));
            s.setValue(kPfx + QStringLiteral("CW_QskSavedHangThreshold"), QStringLiteral("40"));
        };
        Rig rig(start);
        QVERIFY(!rig.model.cwQskSideEffectsAppliedForTest());
        QCOMPARE(rig.slice->agcMode(), AGCMode::Slow);
        QCOMPARE(rig.slice->agcHangThreshold(), 40);
        QCOMPARE(setting("CW_QskActive"), QStringLiteral("False"));
    }

    void restart_stillInQsk_keepsTheValuesSavedBeforeIt()
    {
        Start start;
        start.breakIn = CwBreakInMode::Qsk;
        start.agc = AGCMode::Custom;   // the restored slice carries QSK's values
        start.hangThreshold = 100;
        start.seed = []() {
            AppSettings& s = AppSettings::instance();
            s.setValue(kPfx + QStringLiteral("CW_QskActive"), QStringLiteral("True"));
            s.setValue(kPfx + QStringLiteral("CW_QskSavedAgcMode"),
                       QString::number(static_cast<int>(AGCMode::Fast)));
            s.setValue(kPfx + QStringLiteral("CW_QskSavedHangThreshold"), QStringLiteral("30"));
        };
        Rig rig(start);
        QCOMPARE(rig.keyer()->effectiveBreakInMode(), CwBreakInMode::Qsk);
        QVERIFY(rig.model.cwQskSideEffectsAppliedForTest());
        QCOMPARE(rig.slice->agcMode(), AGCMode::Custom);
        QCOMPARE(rig.slice->agcHangThreshold(), 100);
        QCOMPARE(setting("CW_QskSavedAgcMode"), QString::number(static_cast<int>(AGCMode::Fast)));
        QCOMPARE(setting("CW_QskSavedHangThreshold"), QStringLiteral("30"));
        rig.selectBreakIn(CwBreakInMode::Semi);
        QCOMPARE(rig.slice->agcMode(), AGCMode::Fast);
        QCOMPARE(rig.slice->agcHangThreshold(), 30);
    }

    // ── R-CW-20: leaving CW turns QSK off and remembers it ──────────────
    // Thetis: chkQSK.Checked = false (console.cs:34571 [v2.10.3.15]) is
    // manual break-in (console.cs:16163-16173), NereusSDR's Off.
    void leavingCw_setsOffAndRemembersQsk_returningSelectsItAgain()
    {
        Rig rig;
        rig.selectBreakIn(CwBreakInMode::Qsk);
        QVERIFY(rig.model.cwQskSideEffectsAppliedForTest());

        rig.slice->setDspMode(DSPMode::USB);
        pump();
        QCOMPARE(rig.tm().cwBreakInMode(), static_cast<int>(CwBreakInMode::Off));
        QVERIFY(!rig.mox()->isCwModeActive());
        QVERIFY(!rig.model.cwQskSideEffectsAppliedForTest());
        QCOMPARE(rig.slice->agcMode(), AGCMode::Med);
        QCOMPARE(rig.slice->agcHangThreshold(), 50);

        rig.slice->setDspMode(DSPMode::CWL);
        pump();
        QCOMPARE(rig.tm().cwBreakInMode(), static_cast<int>(CwBreakInMode::Qsk));
        QCOMPARE(rig.mox()->cwBreakInMode(), CwBreakInMode::Qsk);
        QVERIFY(rig.mox()->isCwModeActive());
        QVERIFY(rig.model.cwQskSideEffectsAppliedForTest());

        // Leaving with Semi selected remembers nothing.
        rig.selectBreakIn(CwBreakInMode::Semi);
        rig.slice->setDspMode(DSPMode::LSB);
        pump();
        rig.slice->setDspMode(DSPMode::CWL);
        pump();
        QCOMPARE(rig.tm().cwBreakInMode(), static_cast<int>(CwBreakInMode::Semi));
    }

    void returningToCw_whereQskIsNotOffered_staysOff()
    {
        Rig rig;
        rig.selectBreakIn(CwBreakInMode::Qsk);
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        rig.model.setLastRadioInfoForTest(hermesP1());
        rig.keyer()->reevaluate();
        pump();
        rig.slice->setDspMode(DSPMode::CWU);
        pump();
        QVERIFY(!rig.keyer()->qskAvailable());
        QCOMPARE(rig.tm().cwBreakInMode(), static_cast<int>(CwBreakInMode::Off));
        QVERIFY(!rig.model.cwQskSideEffectsAppliedForTest());
    }

    // ── R-CW-20: a QSK fallback says why, once ───────────────────────────
    void qskFallback_onAnAntennaChange_saysWhyOnce()
    {
        Rig rig;
        rig.selectBreakIn(CwBreakInMode::Qsk);
        QVERIFY(rig.model.cwQskSideEffectsAppliedForTest());
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        QSignalSpy refused(rig.mox(), &MoxController::moxRefused);

        rig.slice->setTxAntenna(QStringLiteral("ANT2"));
        pump();
        QCOMPARE(rig.keyer()->effectiveBreakInMode(), CwBreakInMode::Semi);
        QCOMPARE(rig.tm().cwBreakInMode(), static_cast<int>(CwBreakInMode::Qsk));
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), QString::fromLatin1(kCwQskAntennaReason));
        // M5: not the band plan's code.
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).value<TxRefusal>().code, QByteArray(TxRefusals::kNotReady));
        // No longer in QSK: its changes are undone.
        QVERIFY(!rig.model.cwQskSideEffectsAppliedForTest());
        QCOMPARE(rig.slice->agcMode(), AGCMode::Med);

        // Other changes say nothing more.
        rig.slice->setFrequency(14'055'000.0);
        rig.keyer()->reevaluate();
        pump();
        QCOMPARE(rejected.count(), 1);

        // The antenna back: QSK again, silently.
        rig.slice->setTxAntenna(rig.slice->rxAntenna());
        pump();
        QCOMPARE(rig.keyer()->effectiveBreakInMode(), CwBreakInMode::Qsk);
        QVERIFY(rig.model.cwQskSideEffectsAppliedForTest());
        QCOMPARE(rejected.count(), 1);
    }

    // ── R-CW-18: a key in CW never starts the TX channel ─────────────────
    void cwKey_neverStartsTheTxChannel_voiceKeyStillDoes()
    {
        Rig rig;
        rig.radioPtt(true);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rig.mox()->pttMode(), PttMode::Cw);
        QCOMPARE(rig.mox()->state(), MoxState::Tx);
        QCOMPARE(rig.rfOpens.load(), 0);
        QVERIFY(!rig.tx.isRunning());
        rig.radioPtt(false);
        QTRY_COMPARE_WITH_TIMEOUT(rig.mox()->state(), MoxState::Rx, 2000);
        QCOMPARE(rig.rfOpens.load(), 0);

        // The MOX button in CW: the same.
        rig.mox()->onMoxButton(true);
        pump();
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rig.rfOpens.load(), 0);
        rig.mox()->onMoxButton(false);
        QTRY_COMPARE_WITH_TIMEOUT(rig.mox()->state(), MoxState::Rx, 2000);

        // In USB the radio's PTT is the mic and the channel's gate opens.
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        rig.radioPtt(true);
        QVERIFY(rig.mox()->isMox());
        QCOMPARE(rig.mox()->pttMode(), PttMode::Mic);
        QTRY_VERIFY_WITH_TIMEOUT(rig.rfOpens.load() >= 1, 2000);
        rig.radioPtt(false);
        QTRY_COMPARE_WITH_TIMEOUT(rig.mox()->state(), MoxState::Rx, 2000);
    }

    // ── R-CW-09: the gate's refusal of a dot, dash or the radio's PTT ────
    void gateRefused_dotAndRadioPtt_areReportedOncePerHold()
    {
        Rig rig;
        rig.slice->setFrequency(14'400'000.0);   // outside the band plan
        pump();
        QVERIFY(!rig.keyer()->keyerAllowed());
        const TxRefusal refusal = rig.keyer()->refusal();
        QCOMPARE(refusal.code, QByteArray(TxRefusals::kBandPlan));
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);

        rig.key(true, false, false);
        rig.key(true, false, false);
        rig.key(false, true, false);
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), refusal.text);
        rig.key(false, false, false);
        rig.key(true, false, false);
        QCOMPARE(rejected.count(), 2);
        rig.key(false, false, false);

        rig.radioPtt(true, 5);
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(rejected.count(), 3);
        QCOMPARE(rejected.at(2).at(0).toString(), refusal.text);
        rig.radioPtt(false);
        QCOMPARE(rejected.count(), 3);
    }

    // ── R-CW-10: the QSK time-out fires once per hold ────────────────────
    void qskTimeOut_firesOncePerHold()
    {
        Rig rig;
        rig.selectBreakIn(CwBreakInMode::Qsk);
        TxTimeOutTimer* tot = rig.model.txTimeOutTimer();
        QVERIFY(tot);
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);

        rig.key(true, false, true);
        QVERIFY(tot->isTicking());
        rig.clockMs += TxTimeOutTimer::kMinimumSeconds * 1000;
        tot->tick();
        pump();
        QCOMPARE(stopped.count(), 1);
        QVERIFY(!tot->isTicking());
        QVERIFY(!rig.model.cwQskKeyOutTimingForTest());

        // The radio still reports key-out (it has not seen the keyer go
        // off yet): nothing fires again.
        rig.key(true, false, true);
        rig.clockMs += TxTimeOutTimer::kMinimumSeconds * 1000;
        tot->tick();
        pump();
        QCOMPARE(stopped.count(), 1);
        QVERIFY(!tot->isTicking());

        // Released: the next hold is timed again.
        rig.key(false, false, false);
        QVERIFY(rig.keyer()->keyerAllowed());
        rig.key(true, false, true);
        QVERIFY(rig.model.cwQskKeyOutTimingForTest());
        QVERIFY(tot->isTicking());
        // I4: the count stops once key-out has been up longer than the
        // break-in delay (400 ms here).
        rig.key(false, false, false);
        QVERIFY(tot->isTicking());
        QTRY_VERIFY_WITH_TIMEOUT(!tot->isTicking(), 3000);
    }

    // ── M7: QSK applied before the radio's MAC is known ──────────────────
    void qskBeforeTheMacIsKnown_writesTheRestoreKeysOnConnect()
    {
        Rig rig;
        // The radio known without its MAC (before discovery fills it in).
        RadioInfo noMac = saturn();
        noMac.macAddress.clear();
        rig.model.setLastRadioInfoForTest(noMac);
        rig.keyer()->reevaluate();
        pump();
        rig.selectBreakIn(CwBreakInMode::Qsk);
        QVERIFY(rig.model.cwQskSideEffectsAppliedForTest());
        QCOMPARE(rig.slice->agcMode(), AGCMode::Custom);
        QVERIFY(!AppSettings::instance().contains(kPfx + QStringLiteral("CW_QskActive")));

        // The connect, with the MAC known: the keys are written.
        rig.model.setLastRadioInfoForTest(saturn());
        rig.model.wireCwKeyerConnectionForTest();
        pump();
        QCOMPARE(setting("CW_QskActive"), QStringLiteral("True"));
        QCOMPARE(setting("CW_QskSavedAgcMode"), QString::number(static_cast<int>(AGCMode::Med)));
        QCOMPARE(setting("CW_QskSavedHangThreshold"), QStringLiteral("50"));

        // Leaving QSK restores and clears them as usual.
        rig.selectBreakIn(CwBreakInMode::Semi);
        QCOMPARE(rig.slice->agcMode(), AGCMode::Med);
        QCOMPARE(setting("CW_QskActive"), QStringLiteral("False"));
    }
};

QTEST_MAIN(TestCwQskSideEffects)
#include "tst_cw_qsk_side_effects.moc"

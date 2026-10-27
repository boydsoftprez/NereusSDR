// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_tci_cw.cpp  (NereusSDR)
// =================================================================
//
// CW transmit Task 9 (R-CW-35, R-CW-52): the TCI CW commands on a local
// model with a fake connection: cw_macros and cw_msg send through the CWX
// sender, cw_macros_speed (set, query, up, down) and cw_keyer_speed set and
// report, keyer keys the CWX bit through the sender and raises MOX in Semi,
// a press with no release drops 3000 ms after it, a timed press or release
// drops on time, cw_macros_stop aborts, cw_macros_empty with the terminal
// on, the init burst's real values, the owner rule, and refusals. Nothing
// here opens a radio socket or keys a radio: the key line is the fake
// connection's own and the fake connection sends nothing.
//
// Modification history (NereusSDR):
//   2026-10-27: CW transmit, CAT and TCI CW commands. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>

#include <atomic>
#include <memory>

#include "core/AppSettings.h"
#include "core/CwKeyLine.h"
#include "core/CwTypes.h"
#include "core/CwxSender.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TciProtocol.h"
#include "models/CwKeyerController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

class TciCwMockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit TciCwMockConnection(QObject* parent = nullptr)
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
};

RadioInfo radioInfo(ProtocolVersion protocol, HPSDRHW board, int firmware)
{
    RadioInfo info;
    info.protocol = protocol;
    info.boardType = board;
    info.firmwareVersion = firmware;
    return info;
}

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

// When the CWX bit last went up and down, on the wall clock from `clock`.
struct KeyTimes {
    QElapsedTimer clock;
    std::atomic<qint64> downAt{-1};
    std::atomic<qint64> upAt{-1};
    std::atomic<int> downs{0};
};

// A local model with a fake connection, one slice on 20 m CWU bound for
// transmit, Semi break-in, CWX at 60 WPM, and the TCI protocol on it.
struct Rig {
    RadioModel model;
    TciCwMockConnection conn;
    SliceModel* slice{nullptr};
    std::unique_ptr<TciProtocol> tci;
    KeyTimes times;

    Rig()
    {
        AppSettings::instance().clear();
        model.setLastRadioInfoForTest(radioInfo(ProtocolVersion::Protocol2, HPSDRHW::OrionMKII, 22));
        model.injectConnectionForTest(&conn);
        model.wireCwKeyerConnectionForTest();
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        slice = model.sliceById(model.addSlice());
        slice->setDspMode(DSPMode::CWU);
        slice->setFrequency(14'050'000.0);
        model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
        model.transmitModel().setCwxSpeed(60);
        tci = std::make_unique<TciProtocol>(&model);
        times.clock.start();
        conn.cwKeyLine()->setOnChange([this]() {
            const bool down = conn.cwKeyLine()->cwx();
            if (down) {
                times.downAt.store(times.clock.elapsed());
                times.downs.fetch_add(1);
            } else {
                times.upAt.store(times.clock.elapsed());
            }
        });
        pump();
    }
    ~Rig()
    {
        model.cwxSender()->stop();
        pump();
        conn.cwKeyLine()->setOnChange({});
        tci.reset();
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }
    QString cmd(const QString& line) { return tci->handleCommand(line); }
    QStringList notifications()
    {
        QStringList out;
        while (tci->hasPendingNotification()) {
            out << tci->takePendingNotification();
        }
        return out;
    }
    CwxSender* cwx() { return model.cwxSender(); }
    bool keyed() { return conn.cwKeyLine()->cwx(); }
};

} // namespace

class TestTciCw : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── Macros ─────────────────────────────────────────────────────────────
    void cwMacrosSendsThroughTheSender()
    {
        Rig rig;
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros:0,CQ TEST;")), QString());
        QVERIFY(rig.cwx()->isSending());
        QTRY_VERIFY(rig.keyed());
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 10000);
        QVERIFY(!rig.keyed());
    }

    void cwMacrosStopAborts()
    {
        Rig rig;
        rig.cmd(QStringLiteral("cw_macros:0,TTTTTTTTTTTTTTTTTTTT;"));
        QTRY_VERIFY(rig.keyed());
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros_stop;")), QString());
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.keyed());
    }

    void cwMsgSendsPrefixCallsignAndSuffixAndTheCallsignMayChange()
    {
        Rig rig;
        QSignalSpy started(rig.cwx(), &CwxSender::characterStarted);
        rig.cmd(QStringLiteral("cw_msg:0,TTTT,K1A,E;"));
        // Changed while the prefix sends: five characters, not three.
        rig.cmd(QStringLiteral("cw_msg:W1XYZ;"));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 15000);
        pump();
        QCOMPARE(started.count(), 4 + 5 + 1);
    }

    void cwMacrosEmptyWithTheTerminalOn()
    {
        Rig rig;
        rig.cmd(QStringLiteral("cw_terminal:0,true;"));
        rig.notifications();
        rig.cmd(QStringLiteral("cw_macros:0,EE;"));
        QVERIFY(rig.model.mox());   // the terminal holds TCI PTT
        QStringList seen;
        const auto emptySeen = [&rig, &seen]() {
            seen << rig.notifications();
            return seen.contains(QStringLiteral("cw_macros_empty:0;"));
        };
        QTRY_VERIFY_WITH_TIMEOUT(emptySeen(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 5000);
        pump();
        QVERIFY(rig.model.mox());   // the terminal keeps it while on
        rig.cmd(QStringLiteral("cw_terminal:0,false;"));
        pump();
        QVERIFY(!rig.model.mox());
    }

    void cwMacrosEmptyNotSentWithTheTerminalOff()
    {
        Rig rig;
        rig.cmd(QStringLiteral("cw_macros:0,EE;"));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.cwx()->isSending(), 5000);
        pump();
        QVERIFY(!rig.notifications().contains(QStringLiteral("cw_macros_empty:0;")));
        QVERIFY(!rig.model.mox());
    }

    // ── Speeds ─────────────────────────────────────────────────────────────
    void cwMacrosSpeedSetsAndReports()
    {
        Rig rig;
        rig.notifications();
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros_speed:28;")), QString());
        QCOMPARE(rig.model.transmitModel().cwxSpeed(), 28);
        QVERIFY(rig.notifications().contains(QStringLiteral("cw_macros_speed:28;")));
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros_speed;")), QStringLiteral("cw_macros_speed:28;"));
        rig.cmd(QStringLiteral("cw_macros_speed_up:5;"));
        QCOMPARE(rig.model.transmitModel().cwxSpeed(), 33);
        rig.cmd(QStringLiteral("cw_macros_speed_down:10;"));
        QCOMPARE(rig.model.transmitModel().cwxSpeed(), 23);
        rig.cmd(QStringLiteral("cw_macros_speed:99;"));
        QCOMPARE(rig.model.transmitModel().cwxSpeed(), 60);   // D12
    }

    void cwKeyerSpeedSetsAndReports()
    {
        Rig rig;
        rig.notifications();
        QCOMPARE(rig.cmd(QStringLiteral("cw_keyer_speed:33;")), QString());
        QCOMPARE(rig.model.transmitModel().cwKeyerSpeed(), 33);
        QVERIFY(rig.notifications().contains(QStringLiteral("cw_keyer_speed:33;")));
        QCOMPARE(rig.cmd(QStringLiteral("cw_keyer_speed;")), QStringLiteral("cw_keyer_speed:33;"));
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros_delay;")), QStringLiteral("cw_macros_delay:50;"));
    }

    void initBurstCarriesTheRealSpeeds()
    {
        Rig rig;
        rig.model.transmitModel().setCwxSpeed(28);
        rig.model.transmitModel().setCwKeyerSpeed(33);
        const QStringList burst = rig.tci->buildInitBurst();
        QVERIFY(burst.contains(QStringLiteral("cw_macros_speed:28;")));
        QVERIFY(burst.contains(QStringLiteral("cw_macros_delay:50;")));
        QVERIFY(burst.contains(QStringLiteral("cw_keyer_speed:33;")));
    }

    // ── The keyer ──────────────────────────────────────────────────────────
    void keyerKeysTheCwxBitAndMoxInSemi()
    {
        Rig rig;
        QCOMPARE(rig.cmd(QStringLiteral("keyer:0,true;")), QString());
        QVERIFY(rig.keyed());
        QVERIFY(rig.cwx()->isKeyHeld());
        QVERIFY(rig.model.mox());
        rig.cmd(QStringLiteral("keyer:0,false;"));
        QVERIFY(!rig.keyed());
        pump();
        QVERIFY(!rig.model.mox());
    }

    void keyerInOffRaisesNoMox()
    {
        Rig rig;
        rig.model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Off));
        pump();
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.cwx()->isKeyHeld());
        QVERIFY(!rig.model.mox());
        rig.cmd(QStringLiteral("keyer:0,false;"));
        QVERIFY(!rig.cwx()->isKeyHeld());
    }

    void keyerWithNoReleaseDrops3000MsAfterThePress()
    {
        Rig rig;
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.keyed());
        const qint64 down = rig.times.downAt.load();
        // A second press while held does not restart the count (Thetis).
        QTest::qWait(500);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.keyed(), 6000);
        const qint64 held = rig.times.upAt.load() - down;
        qInfo() << "TCI keyer watchdog: held" << held << "ms";
        QVERIFY2(held >= 2990 && held < 3500, qPrintable(QString::number(held)));
        QTRY_VERIFY(!rig.model.mox());
        QVERIFY(!rig.cwx()->isSending());
    }

    void keyerPressWithADurationReleasesOnTime()
    {
        Rig rig;
        rig.cmd(QStringLiteral("keyer:0,true,120;"));
        QVERIFY(rig.keyed());
        const qint64 down = rig.times.downAt.load();
        QTRY_VERIFY_WITH_TIMEOUT(!rig.keyed(), 3000);
        const qint64 held = rig.times.upAt.load() - down;
        qInfo() << "TCI keyer timed press: held" << held << "ms";
        QVERIFY2(held >= 115 && held < 1000, qPrintable(QString::number(held)));
        QTRY_VERIFY(!rig.model.mox());
    }

    void keyerReleaseWithADurationCountsFromThePress()
    {
        Rig rig;
        rig.cmd(QStringLiteral("keyer:0,true;"));
        const qint64 down = rig.times.downAt.load();
        rig.cmd(QStringLiteral("keyer:0,false,200;"));
        QVERIFY(rig.keyed());
        QTRY_VERIFY_WITH_TIMEOUT(!rig.keyed(), 3000);
        const qint64 held = rig.times.upAt.load() - down;
        qInfo() << "TCI keyer timed release: held" << held << "ms";
        QVERIFY2(held >= 195 && held < 1000, qPrintable(QString::number(held)));
    }

    void cwMacrosStopReleasesTheKeyerAndItsMox()
    {
        Rig rig;
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.model.mox());
        rig.cmd(QStringLiteral("cw_macros_stop;"));
        QVERIFY(!rig.keyed());
        pump();
        QVERIFY(!rig.model.mox());
    }

    void leavingCwReleasesTheKeyerAndItsMox()
    {
        Rig rig;
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.keyed());
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        QVERIFY(!rig.keyed());
        QTRY_VERIFY(!rig.model.mox());
    }

    // ── One sender: the keyer and CWX text do not mix ──────────────────────
    void keyerPressWhileTextSendsIsIgnored()
    {
        Rig rig;
        rig.cmd(QStringLiteral("cw_macros:0,TTTTTTTTTT;"));
        QVERIFY(rig.cwx()->isSending());
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(rig.cwx()->isSending());
        // CWX text from the applet or CAT too.
        rig.cmd(QStringLiteral("cw_macros_stop;"));
        QVERIFY(rig.cwx()->sendText(QStringLiteral("TTTTTTTTTT")));
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(rig.cwx()->isSending());
    }

    void macroWhileTheKeyerIsHeldIsIgnored()
    {
        Rig rig;
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(rig.cwx()->isKeyHeld());
        rig.cmd(QStringLiteral("cw_macros:0,CQ;"));
        QVERIFY(rig.cwx()->isKeyHeld());
        QCOMPARE(rig.cwx()->queuedCharacters(), 0);
        rig.cmd(QStringLiteral("keyer:0,false;"));
    }

    // ── The owner ──────────────────────────────────────────────────────────
    void anotherAppCannotKeyWhileOneOwnsTheKeyerAndLeavingStopsIt()
    {
        Rig rig;
        int appA = 0;
        int appB = 0;
        rig.tci->setCommandOwner(&appA);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        rig.tci->setCommandOwner(&appB);
        rig.cmd(QStringLiteral("keyer:0,false;"));      // not B's to release
        QVERIFY(rig.cwx()->isKeyHeld());
        rig.cmd(QStringLiteral("cw_macros_stop;"));     // nor to stop
        QVERIFY(rig.cwx()->isKeyHeld());
        rig.tci->setCommandOwner(nullptr);
        rig.tci->cwClientDisconnected(&appA);
        QVERIFY(!rig.cwx()->isKeyHeld());
        QVERIFY(!rig.keyed());
        pump();
        QVERIFY(!rig.model.mox());
    }

    // ── Refusals ───────────────────────────────────────────────────────────
    void refusedByTheGateKeysNothingAndTheSenderSaysWhy()
    {
        Rig rig;
        rig.model.setStationRadioChangeUnderway(true);
        pump();
        QVERIFY(!rig.model.cwKeyerController()->keyerAllowed());
        QSignalSpy refused(rig.cwx(), &CwxSender::refused);
        QCOMPARE(rig.cmd(QStringLiteral("keyer:0,true;")), QString());
        QVERIFY(!rig.keyed());
        QVERIFY(!rig.model.mox());
        QCOMPARE(rig.cmd(QStringLiteral("cw_macros:0,CQ;")), QString());
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(refused.count(), 2);
        for (const QList<QVariant>& r : refused) {
            QCOMPARE(r.at(0).toString(), rig.model.cwKeyerController()->refusal().text);
        }
        QCOMPARE(rig.times.downs.load(), 0);
    }

    void keyerOutsideCwKeysNothing()
    {
        Rig rig;
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        rig.cmd(QStringLiteral("keyer:0,true;"));
        QVERIFY(!rig.keyed());
        QVERIFY(!rig.model.mox());
    }

    void remoteWindowAndReceiveOnlyServerKeyNothing()
    {
        Rig rig;
        rig.tci->setRemoteWindow(true);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        rig.cmd(QStringLiteral("cw_macros:0,CQ;"));
        rig.cmd(QStringLiteral("cw_macros_speed:40;"));
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.model.transmitModel().cwxSpeed(), 60);
        rig.tci->setRemoteWindow(false);
        rig.tci->setStationReceiveOnly(true);
        rig.cmd(QStringLiteral("keyer:0,true;"));
        rig.cmd(QStringLiteral("cw_macros:0,CQ;"));
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.times.downs.load(), 0);
    }

    void rxOneNeedsTheSecondReceiver()
    {
        Rig rig;
        rig.cmd(QStringLiteral("cw_macros:1,CQ;"));
        rig.cmd(QStringLiteral("keyer:1,true;"));
        QVERIFY(!rig.cwx()->isSending());
        rig.cmd(QStringLiteral("keyer:2,true;"));
        QVERIFY(!rig.cwx()->isSending());
    }
};

QTEST_GUILESS_MAIN(TestTciCw)
#include "tst_tci_cw.moc"

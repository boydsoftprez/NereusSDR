// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_cat_cw_commands.cpp  (NereusSDR)
// =================================================================
//
// CW transmit Task 9 (R-CW-02, R-CW-34): the CAT CW and CWX commands on a
// local model with a fake connection: KY and ZZKY send through the CWX
// sender and read its buffer as Thetis does, KY switches the mode only on
// a set (D13), the speeds, memories, stop, pitch, break-in, QSK, delay and
// iambic, ZZKO with and without a window, and a refusal by the request
// gate. Nothing here opens a radio socket or keys a radio: the key line is
// the fake connection's own and the fake connection sends nothing.
//
// Modification history (NereusSDR):
//   2026-10-27: CW transmit, CAT and TCI CW commands. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include <memory>

#include "core/AppSettings.h"
#include "core/CwKeyLine.h"
#include "core/CwTypes.h"
#include "core/CwxSender.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/SliceOwnership.h"
#include "core/cat/CatService.h"
#include "core/cat/CatSession.h"
#include "models/CwKeyerController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

class CatCwMockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit CatCwMockConnection(QObject* parent = nullptr)
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

QByteArray padded(const QByteArray& code, const QByteArray& text)
{
    return code + text.leftJustified(25, ' ') + ';';
}

// A local model with a fake connection, one slice on 20 m CWU bound for
// transmit, Semi break-in, CWX at 60 WPM, and a CAT channel whose primary
// is that slice. The session may transmit (a Tester session cannot by
// default; the transport is not what these cases test).
struct Rig {
    RadioModel model;
    CatCwMockConnection conn;
    SliceModel* slice{nullptr};
    quint64 session{0};

    explicit Rig(ProtocolVersion protocol = ProtocolVersion::Protocol2,
                 HPSDRHW board = HPSDRHW::OrionMKII, int firmware = 22)
    {
        AppSettings::instance().clear();
        model.setLastRadioInfoForTest(radioInfo(protocol, board, firmware));
        model.injectConnectionForTest(&conn);
        model.wireCwKeyerConnectionForTest();
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        slice = model.sliceById(model.addSlice());
        model.sliceOwnership()->setOwner(slice->sliceIndex(), SliceOwnership::stationDevice());
        slice->setDspMode(DSPMode::CWU);
        slice->setFrequency(14'050'000.0);
        model.transmitModel().setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
        model.transmitModel().setCwxSpeed(60);
        CatEndpointConfig config;
        config.binding.primarySliceId = slice->sliceIndex();
        if (model.catService()->applyChannelConfig(1, config)) {
            model.catService()->startConfigured();
            session = model.catService()->openSession(1, CatTransportKind::Tester);
            model.catService()->session(session)->context().transmitAllowed = true;
        }
        pump();
    }
    ~Rig()
    {
        model.cwxSender()->stop();
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }
    QByteArray cat(const QByteArray& frame) { return model.catService()->processFrame(session, frame); }
    CwxSender* cwx() { return model.cwxSender(); }
    TransmitModel& tx() { return model.transmitModel(); }
};

} // namespace

class TestCatCwCommands : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── KY and ZZKY (R-CW-34, D13) ────────────────────────────────────────
    void zzkySetQueuesThroughTheSenderAndReadsTheBuffer()
    {
        Rig rig;
        QVERIFY(rig.session);
        QCOMPARE(rig.cat("ZZKY;"), QByteArray("ZZKY2;"));          // empty
        QCOMPARE(rig.cat(padded("ZZKY", "CQ TEST")), QByteArray());
        QVERIFY(rig.cwx()->isSending());
        QCOMPARE(rig.cat("ZZKY;"), QByteArray("ZZKY0;"));          // waiting, under 72
        QCOMPARE(rig.cat("KY;"), QByteArray("KY0;"));
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(rig.cat(padded("ZZKY", "TTTTTTTTTTTTTTTTTTTTTTTT")), QByteArray());
        }
        QVERIFY(rig.cwx()->queuedCharacters() >= 72);
        QCOMPARE(rig.cat("ZZKY;"), QByteArray("ZZKY1;"));          // 72 and over
        QCOMPARE(rig.cat("KY;"), QByteArray("KY1;"));
        QCOMPARE(rig.cat("ZZSS;"), QByteArray());
        QVERIFY(!rig.cwx()->isSending());
        QCOMPARE(rig.cat("ZZKY;"), QByteArray("ZZKY2;"));
        QCOMPARE(rig.cat("KY;"), QByteArray("KY0;"));
    }

    void kyReadNeverChangesTheMode()
    {
        Rig rig;
        rig.slice->setFrequency(7'030'000.0);
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        QCOMPARE(rig.cat("KY;"), QByteArray("KY0;"));
        QCOMPARE(rig.cat("ZZKY;"), QByteArray("ZZKY2;"));
        QCOMPARE(rig.slice->dspMode(), DSPMode::USB);
        QVERIFY(!rig.cwx()->isSending());
    }

    void kySetInUsbOn40mSwitchesToCwlFirst()
    {
        Rig rig;
        rig.slice->setFrequency(7'030'000.0);
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        QCOMPARE(rig.slice->band(), Band::Band40m);
        QCOMPARE(rig.cat(padded("KY", "CQ")), QByteArray());
        QCOMPARE(rig.slice->dspMode(), DSPMode::CWL);
        QVERIFY(rig.cwx()->isSending());
    }

    void kySetInUsbOn20mSwitchesToCwu()
    {
        Rig rig;
        rig.slice->setFrequency(14'200'000.0);
        rig.slice->setDspMode(DSPMode::USB);
        pump();
        QCOMPARE(rig.slice->band(), Band::Band20m);
        QCOMPARE(rig.cat(padded("KY", "CQ")), QByteArray());
        QCOMPARE(rig.slice->dspMode(), DSPMode::CWU);
        QVERIFY(rig.cwx()->isSending());
    }

    void zzkySetInDiguSwitchesToCwu()
    {
        Rig rig;
        rig.slice->setFrequency(7'074'000.0);
        rig.slice->setDspMode(DSPMode::DIGU);
        pump();
        QCOMPARE(rig.cat(padded("ZZKY", "CQ")), QByteArray());
        QCOMPARE(rig.slice->dspMode(), DSPMode::CWU);   // Thetis's default case
    }

    void sessionThatMayNotTransmitChangesNothing()
    {
        Rig rig;
        rig.slice->setDspMode(DSPMode::USB);
        rig.model.catService()->session(rig.session)->context().transmitAllowed = false;
        QCOMPARE(rig.cat(padded("KY", "CQ")), QByteArray("?;"));
        QCOMPARE(rig.cat(padded("ZZKY", "CQ")), QByteArray("?;"));
        QCOMPARE(rig.cat("ZZKM1;"), QByteArray("?;"));
        QCOMPARE(rig.slice->dspMode(), DSPMode::USB);
        QVERIFY(!rig.cwx()->isSending());
    }

    void refusedByTheGateAnswersAsThetisAndTheSenderSaysWhy()
    {
        Rig rig;
        rig.model.setStationRadioChangeUnderway(true);
        pump();
        QVERIFY(!rig.model.cwKeyerController()->keyerAllowed());
        QSignalSpy refused(rig.cwx(), &CwxSender::refused);
        // Thetis's RemoteMessage and StartQueue always answer "".
        QCOMPARE(rig.cat(padded("ZZKY", "CQ")), QByteArray());
        QCOMPARE(rig.cat(padded("KY", "CQ")), QByteArray());
        rig.tx().setCwxMemory1(QStringLiteral("TEST"));
        QCOMPARE(rig.cat("ZZKM1;"), QByteArray());
        QCOMPARE(refused.count(), 3);
        for (const QList<QVariant>& r : refused) {
            QCOMPARE(r.at(0).toString(), rig.model.cwKeyerController()->refusal().text);
        }
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
    }

    // ── Speeds, memories and stop ──────────────────────────────────────────
    void zzksSetsTheCwxSpeedAndClamps()
    {
        Rig rig;
        QCOMPARE(rig.cat("ZZKS020;"), QByteArray());
        QCOMPARE(rig.tx().cwxSpeed(), 20);
        QCOMPARE(rig.cat("ZZKS;"), QByteArray("ZZKS020;"));
        QCOMPARE(rig.cat("ZZKS099;"), QByteArray());
        QCOMPARE(rig.tx().cwxSpeed(), 60);
        QCOMPARE(rig.cat("KS;"), QByteArray("KS060;"));
        QCOMPARE(rig.cat("KS001;"), QByteArray());
        QCOMPARE(rig.tx().cwxSpeed(), 5);
    }

    void zzkmSendsAMemoryAndAnswersNineWithAQuestionMark()
    {
        Rig rig;
        rig.tx().setCwxMemory3(QStringLiteral("TEST"));
        QCOMPARE(rig.cat("ZZKM3;"), QByteArray());
        QVERIFY(rig.cwx()->isSending());
        QCOMPARE(rig.cat("ZZSS;"), QByteArray());
        QCOMPARE(rig.cat("ZZKM9;"), QByteArray("?;"));
        QCOMPARE(rig.cat("ZZKM0;"), QByteArray("?;"));
        QVERIFY(!rig.cwx()->isSending());
    }

    void zzssStopsAndReleasesTheLine()
    {
        Rig rig;
        QCOMPARE(rig.cat(padded("ZZKY", "TTTTTTTTTTTTTTTTTTTT")), QByteArray());
        QTRY_VERIFY(rig.conn.cwKeyLine()->cwx());
        QCOMPARE(rig.cat("ZZSS;"), QByteArray());
        QVERIFY(!rig.cwx()->isSending());
        QVERIFY(!rig.conn.cwKeyLine()->cwx());
    }

    void zzcsSetsTheKeyerSpeed()
    {
        Rig rig;
        QCOMPARE(rig.cat("ZZCS25;"), QByteArray());
        QCOMPARE(rig.tx().cwKeyerSpeed(), 25);
        QCOMPARE(rig.cat("ZZCS;"), QByteArray("ZZCS25;"));
        QCOMPARE(rig.cat("ZZCS99;"), QByteArray());
        QCOMPARE(rig.tx().cwKeyerSpeed(), 60);
        QCOMPARE(rig.cat("ZZCS00;"), QByteArray());
        QCOMPARE(rig.tx().cwKeyerSpeed(), 1);
        // CATStructs.xml gives ZZCS two digits: three is a length error.
        QCOMPARE(rig.cat("ZZCS025;"), QByteArray("?;"));
        QCOMPARE(rig.tx().cwKeyerSpeed(), 1);
    }

    // ── Pitch (R-CW-02) ────────────────────────────────────────────────────
    void zzclSetsAndReadsThePitch()
    {
        Rig rig;
        QCOMPARE(rig.cat("ZZCL0700;"), QByteArray());
        QCOMPARE(rig.model.cwPitchHz(), 700);
        QCOMPARE(rig.cat("ZZCL;"), QByteArray("ZZCL0700;"));
        QCOMPARE(rig.cat("ZZCL9999;"), QByteArray());
        QCOMPARE(rig.model.cwPitchHz(), 2250);
        QCOMPARE(rig.cat("ZZCL0000;"), QByteArray());
        QCOMPARE(rig.model.cwPitchHz(), 200);
    }

    // ── Break-in, QSK, delay and iambic ────────────────────────────────────
    void zzcbTurnsBreakInOffAndOnRestoresSemi()
    {
        Rig rig;
        QCOMPARE(rig.cat("ZZCB;"), QByteArray("ZZCB1;"));
        QCOMPARE(rig.cat("ZZCB0;"), QByteArray());
        QCOMPARE(rig.tx().cwBreakInMode(), static_cast<int>(CwBreakInMode::Off));
        QCOMPARE(rig.cat("ZZCB;"), QByteArray("ZZCB0;"));
        QCOMPARE(rig.cat("ZZCB1;"), QByteArray());
        QCOMPARE(rig.tx().cwBreakInMode(), static_cast<int>(CwBreakInMode::Semi));
        QCOMPARE(rig.cat("ZZCB2;"), QByteArray("?;"));
    }

    void zzqkAnswersAQuestionMarkWhereQskIsUnavailable()
    {
        Rig rig(ProtocolVersion::Protocol1, HPSDRHW::Hermes, 32);
        QVERIFY(!rig.model.cwKeyerController()->qskAvailable());
        QCOMPARE(rig.cat("ZZQK1;"), QByteArray("?;"));
        QCOMPARE(rig.tx().cwBreakInMode(), static_cast<int>(CwBreakInMode::Semi));
        QCOMPARE(rig.cat("ZZQK;"), QByteArray("ZZQK0;"));
        QCOMPARE(rig.cat("ZZCB0;"), QByteArray());
        QCOMPARE(rig.cat("ZZQK0;"), QByteArray());     // 0 sets Semi
        QCOMPARE(rig.tx().cwBreakInMode(), static_cast<int>(CwBreakInMode::Semi));
    }

    void zzqkSetsQskWhereItIsAvailable()
    {
        Rig rig;
        if (!rig.model.cwKeyerController()->qskAvailable()) {
            QSKIP(qPrintable(QStringLiteral("QSK unavailable on the fake OrionMKII: ")
                             + rig.model.cwKeyerController()->qskUnavailableReason()));
        }
        QCOMPARE(rig.cat("ZZQK1;"), QByteArray());
        QCOMPARE(rig.tx().cwBreakInMode(), static_cast<int>(CwBreakInMode::Qsk));
        QCOMPARE(rig.cat("ZZQK;"), QByteArray("ZZQK1;"));
        QCOMPARE(rig.cat("ZZCB;"), QByteArray("ZZCB1;"));   // chkQSK.Checked for QSK too
    }

    void zzcdClampsToTheControlsRange()
    {
        Rig rig;
        QCOMPARE(rig.cat("ZZCD1500;"), QByteArray());
        QCOMPARE(rig.tx().cwBreakInDelayMs(), 1000);
        QCOMPARE(rig.cat("ZZCD;"), QByteArray("ZZCD1000;"));
        QCOMPARE(rig.cat("ZZCD0100;"), QByteArray());
        QCOMPARE(rig.tx().cwBreakInDelayMs(), 100);   // Thetis would clamp to 150
    }

    void zzciSwitchesTheStraightKeyAndIambic()
    {
        Rig rig;
        QCOMPARE(rig.cat("ZZCI;"), QByteArray("ZZCI1;"));
        QCOMPARE(rig.cat("ZZCI0;"), QByteArray());
        QCOMPARE(rig.tx().cwKeyerMode(), static_cast<int>(CwKeyerMode::Straight));
        QCOMPARE(rig.cat("ZZCI;"), QByteArray("ZZCI0;"));
        rig.tx().setCwKeyerMode(static_cast<int>(CwKeyerMode::Straight));
        QCOMPARE(rig.cat("ZZCI1;"), QByteArray());
        QCOMPARE(rig.tx().cwKeyerMode(), static_cast<int>(CwKeyerMode::IambicB));
        rig.tx().setCwKeyerMode(static_cast<int>(CwKeyerMode::IambicA));
        QCOMPARE(rig.cat("ZZCI1;"), QByteArray());
        QCOMPARE(rig.tx().cwKeyerMode(), static_cast<int>(CwKeyerMode::IambicA));
    }

    // ── ZZKO ───────────────────────────────────────────────────────────────
    void zzkoAnswersAQuestionMarkWithNoWindowAndTogglesWithOne()
    {
        Rig rig;
        QCOMPARE(rig.cat("ZZKO;"), QByteArray("?;"));
        QCOMPARE(rig.cat("ZZKO1;"), QByteArray("?;"));
        bool shown = false;
        rig.model.setCwxAppletHost([&shown]() { return shown; }, [&shown](bool on) { shown = on; });
        QCOMPARE(rig.cat("ZZKO;"), QByteArray("ZZKO0;"));
        QCOMPARE(rig.cat("ZZKO1;"), QByteArray());
        QVERIFY(shown);
        QCOMPARE(rig.cat("ZZKO;"), QByteArray("ZZKO1;"));
        QCOMPARE(rig.cat("ZZKO0;"), QByteArray());
        QVERIFY(!shown);
        QCOMPARE(rig.cat("ZZKO2;"), QByteArray("?;"));
        rig.model.setCwxAppletHost({}, {});
    }

    // ── ZZCM and ZZCF stay unavailable (section 7) ─────────────────────────
    void zzcmAndZzcfStayUnavailable()
    {
        Rig rig;
        QCOMPARE(rig.cat("ZZCM;"), QByteArray("?;"));
        QCOMPARE(rig.cat("ZZCF;"), QByteArray("?;"));
    }
};

QTEST_GUILESS_MAIN(TestCatCwCommands)
#include "tst_cat_cw_commands.moc"

// no-port-check: test fixture asserts OcMatrix's TX pin action mask and the
// P1 OC byte it reaches; the ported logic and its cites live in OcMatrix.cpp.
//
// The OC Outputs "TX Pin Action mapping" masks the transmit pins against
// MOX, TUNE and two-tone (Thetis Penny.cs adjustForTXAction); receive is
// untouched (adjustForRX with no consider-PA pins). Pin numbers in the
// names are the UI's 1-based pins; the code's are 0-based.
//
// TUNE's flag reaches the connection from RadioModel in Thetis's order:
// it rises before the key and falls after MOX off (console.cs:30059, 30157,
// 30173 [v2.10.3.15]: _tuning = true before chkMOX.Checked = true;
// chkMOX.Checked = false before _tuning = false). The connection only
// stores it. A re-key during TUNE's unkey walk, and another source's key
// that ends a TUNE start, drop it at once.
//
// Modification history (NereusSDR):
//   2026-10-26 - The TUNE hold moved from the connection to RadioModel:
//                p1_tune_off_held_until_unkey replaced by RadioModel-level
//                tests (J.J. Boyd KG4VCF, AI-assisted: Claude Code).

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QScopeGuard>

#include "core/MoxController.h"
#include "core/OcMatrix.h"
#include "core/P1RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/SliceOwnership.h"
#include "core/TxChannel.h"
#include "core/cat/CatTxCoordinator.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

using Action = OcMatrix::TXPinAction;

struct Expect {
    Action action;
    int tx;       // maskFor(B20M, true, false, false)
    int tune;     // maskFor(B20M, true, true, false)
    int twoTone;  // maskFor(B20M, true, false, true)
};

// Bank 0 C2 carries the OC byte shifted left one (see tst_p1_oc_matrix_wiring).
int p1OcByte(P1RadioConnection& conn)
{
    quint8 buf[5] = {};
    conn.composeCcForBankForTest(0, buf);
    return int(buf[2]) >> 1;
}

// Logs MOX and the OC TUNE / two-tone pair in arrival order; the base
// stores the pair, so ocTune() / ocTwoTone() read back what arrived.
class OcLoggingConnection : public RadioConnection {
    Q_OBJECT
public:
    QStringList log;

    explicit OcLoggingConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void setMox(bool enabled) override { log << QStringLiteral("mox:%1").arg(int(enabled)); }
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

protected:
    void onOcTxModesChanged() override
    {
        log << QStringLiteral("tune:%1").arg(int(ocTune()));
    }
};

void pump()
{
    for (int i = 0; i < 6; ++i) {
        QCoreApplication::processEvents();
    }
}

// A Core RadioModel keyed by its own MoxController, timers at zero, with
// `conn` as its connection and one USB slice.
void prepare(RadioModel& model, OcLoggingConnection& conn)
{
    model.setCapsForTest(/*hasAlex=*/false);
    model.injectConnectionForTest(&conn);
    model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
    model.setTuneOffSettleMsForTest(0);
    model.addSlice();
    SliceModel* slice = model.activeSlice();
    QVERIFY(slice != nullptr);
    slice->setDspMode(DSPMode::USB);
    pump();
    conn.log.clear();
}

} // namespace

class TestOcMatrixTxPinAction : public QObject {
    Q_OBJECT

private slots:
    void action_masks_pin_1_on_20m_data()
    {
        QTest::addColumn<int>("action");
        QTest::addColumn<int>("tx");
        QTest::addColumn<int>("tune");
        QTest::addColumn<int>("twoTone");
        const Expect rows[] = {
            {Action::Mox,            0x01, 0x00, 0x00},
            {Action::Tune,           0x00, 0x01, 0x00},
            {Action::TwoTone,        0x00, 0x00, 0x01},
            {Action::MoxTune,        0x01, 0x01, 0x00},
            {Action::MoxTwoTone,     0x01, 0x00, 0x01},
            {Action::TuneTwoTone,    0x00, 0x01, 0x01},
            {Action::MoxTuneTwoTone, 0x01, 0x01, 0x01},
        };
        for (const Expect& r : rows) {
            QTest::addRow("action%d", int(r.action))
                << int(r.action) << r.tx << r.tune << r.twoTone;
        }
    }

    void action_masks_pin_1_on_20m()
    {
        QFETCH(int, action);
        QFETCH(int, tx);
        QFETCH(int, tune);
        QFETCH(int, twoTone);

        OcMatrix m;
        m.setPin(Band::Band20m, /*pin=*/0, /*tx=*/true, true);
        m.setPinAction(0, Action(action));

        QCOMPARE(int(m.maskFor(Band::Band20m, true, false, false)), tx);
        QCOMPARE(int(m.maskFor(Band::Band20m, true, true, false)), tune);
        QCOMPARE(int(m.maskFor(Band::Band20m, true, false, true)), twoTone);
        // The two-argument form is tune and two-tone off.
        QCOMPARE(int(m.maskFor(Band::Band20m, true)), tx);
    }

    // Receive ignores the action: RX pin 3 on 40 m is 0x04 whatever it is.
    void receive_ignores_action()
    {
        for (int a = 0; a < int(Action::Count); ++a) {
            OcMatrix m;
            m.setPin(Band::Band40m, /*pin=*/2, /*tx=*/false, true);
            m.setPinAction(2, Action(a));
            for (int t = 0; t < 4; ++t) {
                const bool tune = (t & 1) != 0;
                const bool twoTone = (t & 2) != 0;
                QCOMPARE(int(m.maskFor(Band::Band40m, false, tune, twoTone)), 0x04);
            }
            QCOMPARE(int(m.maskFor(Band::Band40m, false)), 0x04);
        }
    }

    // Default actions (MOX_TUNE_TWOTONE) pass every TX and RX pin through,
    // so every existing caller's byte is unchanged.
    void default_actions_leave_bytes_unchanged()
    {
        OcMatrix m;
        for (int b = 0; b < int(Band::Count); ++b) {
            for (int pin = 0; pin < 7; ++pin) {
                // A pattern that differs per band and between RX and TX.
                m.setPin(Band(b), pin, false, ((b + pin) % 2) == 0);
                m.setPin(Band(b), pin, true, ((b * 3 + pin) % 3) != 0);
            }
        }
        for (int b = 0; b < int(Band::Count); ++b) {
            for (int dir = 0; dir < 2; ++dir) {
                const bool tx = dir == 1;
                int raw = 0;
                for (int pin = 0; pin < 7; ++pin) {
                    if (m.pinEnabled(Band(b), pin, tx)) { raw |= 1 << pin; }
                }
                QCOMPARE(int(m.maskFor(Band(b), tx)), raw);
                QCOMPARE(int(m.maskFor(Band(b), tx, true, false)), raw);
                QCOMPARE(int(m.maskFor(Band(b), tx, false, true)), raw);
                QCOMPARE(int(m.maskFor(Band(b), tx, true, true)), raw);
            }
        }
    }

    // The P1 OC byte reads the connection's TUNE flag while keyed.
    void p1_wire_applies_tune()
    {
        OcMatrix m;
        m.setPin(Band::Band20m, 0, /*tx=*/true, true);   // MOX-only pin
        m.setPin(Band::Band20m, 1, /*tx=*/true, true);   // TUNE-only pin
        m.setPinAction(0, Action::Mox);
        m.setPinAction(1, Action::Tune);

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setOcMatrix(&m);
        conn.setReceiverFrequency(0, 14'100'000ULL);
        conn.setTxFrequency(14'100'000ULL);

        conn.setMox(true);
        QCOMPARE(p1OcByte(conn), 0x01);   // voice MOX: the MOX pin only
        conn.setMox(false);

        // TUNE's flag arrives ahead of the key, as RadioModel queues it.
        conn.setOcTxModes(/*tune=*/true, /*twoTone=*/false);
        conn.setMox(true);
        QCOMPARE(p1OcByte(conn), 0x02);   // TUNE: the TUNE pin only
    }

    // The connection stores the pair as RadioModel sends it: a TUNE flag
    // that falls while keyed applies at once (RadioModel orders it after
    // MOX off; see the radioModel_* tests).
    void p1_tune_flag_applies_as_sent()
    {
        OcMatrix m;
        m.setPin(Band::Band20m, 0, /*tx=*/true, true);
        m.setPinAction(0, Action::Mox);

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setOcMatrix(&m);
        conn.setReceiverFrequency(0, 14'100'000ULL);
        conn.setTxFrequency(14'100'000ULL);

        conn.setOcTxModes(true, false);
        conn.setMox(true);
        QCOMPARE(p1OcByte(conn), 0x00);

        conn.setOcTxModes(false, false);
        QVERIFY(!conn.ocTune());
        QCOMPARE(p1OcByte(conn), 0x01);
        conn.setMox(false);
    }

    // ── RadioModel: TUNE's flag in Thetis's order ────────────────────────

    // TUNE: the flag reaches the connection before MOX on, and leaves it
    // after MOX off.
    void radioModel_tuneFlagRisesBeforeKeyAndFallsAfterUnkey()
    {
        OcLoggingConnection conn;
        RadioModel model;
        prepare(model, conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });

        model.setTune(true);
        pump();
        QVERIFY(model.moxController()->isMox());
        const int tuneOn = conn.log.indexOf(QStringLiteral("tune:1"));
        const int moxOn = conn.log.indexOf(QStringLiteral("mox:1"));
        QVERIFY2(tuneOn >= 0 && moxOn > tuneOn, qPrintable(conn.log.join(u' ')));
        QVERIFY(conn.ocTune());

        conn.log.clear();
        model.setTune(false);
        QVERIFY(conn.ocTune());   // TUNE has cleared; MOX off not yet sent
        pump();
        QVERIFY(!model.moxController()->isMox());
        const int moxOff = conn.log.indexOf(QStringLiteral("mox:0"));
        const int tuneOff = conn.log.indexOf(QStringLiteral("tune:0"));
        QVERIFY2(moxOff >= 0 && tuneOff > moxOff, qPrintable(conn.log.join(u' ')));
        QVERIFY(!conn.ocTune());
    }

    // A voice key during TUNE's unkey walk cancels the walk, so MOX off
    // never comes. The key drops the TUNE flag, so MOX-only pins open and
    // TUNE-only pins close for the voice transmission.
    void radioModel_rekeyDuringUnkeyWalk_dropsTuneFlag()
    {
        OcLoggingConnection conn;
        RadioModel model;
        prepare(model, conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });

        model.setTune(true);
        pump();
        QVERIFY(model.moxController()->isMox());
        QVERIFY(conn.ocTune());

        model.setTune(false);
        QVERIFY(!model.moxController()->isMox());
        QVERIFY(conn.ocTune());
        model.moxController()->setMox(true);
        pump();
        QVERIFY(model.moxController()->isMox());
        QVERIFY(!model.transmitModel().isTune());
        QVERIFY(!conn.ocTune());

        model.moxController()->setMox(false);
        pump();
        QVERIFY(!conn.ocTune());
    }

    // Another source's key accepted while a TUNE start is setting up ends
    // the TUNE without releasing that key (finishCutShortTuneStart). TUNE
    // never keyed, so its flag drops at once and stays down for the other
    // key.
    void radioModel_handoverEndingTune_dropsTuneFlagUnderAnotherKey()
    {
        OcLoggingConnection conn;
        TxChannel channel(1);
        RadioModel model;
        prepare(model, conn);
        const auto detach = qScopeGuard([&model] {
            model.injectTxChannelForTest(nullptr);
            model.injectConnectionForTest(nullptr);
        });
        model.injectTxChannelForTest(&channel);
        model.addSlice();
        model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        model.sliceOwnership()->setOwner(1, SliceOwnership::stationDevice());
        model.sliceById(0)->setFrequency(14'200'000);
        model.sliceById(1)->setFrequency(14'250'000);
        model.moxController()->setMoxCheck(
            [] { return safety::BandPlanGuard::MoxCheckResult{true, QString()}; });
        SliceModel* const slice = model.sliceById(0);
        slice->setDspMode(DSPMode::CWU);

        // The other source keys while TUNE's CW-to-SSB swap is under way.
        bool cut = false;
        connect(slice, &SliceModel::dspModeChanged, &model, [&model, &cut](DSPMode mode) {
            if (!cut && mode == DSPMode::USB) {
                cut = true;
                model.moxController()->setMox(true);
            }
        });
        CatTxCoordinator tx(model);
        tx.requestTransmit(1, 0, CatTransmitKind::Tune);
        pump();
        QVERIFY(cut);
        QTRY_VERIFY(!model.isTune());
        QVERIFY(!model.transmitModel().isTune());
        QVERIFY(model.moxController()->isMox());   // the other source's key stays
        pump();
        QVERIFY2(!conn.ocTune(), qPrintable(conn.log.join(u' ')));

        model.moxController()->setMox(false);
        pump();
        QVERIFY(!conn.ocTune());
    }

    // Two-tone's flag follows TransmitModel at once, both ways.
    void radioModel_twoToneFlagFollowsTransmitModel()
    {
        OcLoggingConnection conn;
        RadioModel model;
        prepare(model, conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });

        model.transmitModel().setTwoToneActive(true);
        QVERIFY(conn.ocTwoTone());
        model.transmitModel().setTwoToneActive(false);
        QVERIFY(!conn.ocTwoTone());
    }

    // Two-tone rising while keyed applies at once.
    void p1_two_tone_rise_while_keyed_applies()
    {
        OcMatrix m;
        m.setPin(Band::Band20m, 0, /*tx=*/true, true);
        m.setPinAction(0, Action::TwoTone);

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setOcMatrix(&m);
        conn.setReceiverFrequency(0, 14'100'000ULL);
        conn.setTxFrequency(14'100'000ULL);

        conn.setMox(true);
        QCOMPARE(p1OcByte(conn), 0x00);
        conn.setOcTxModes(false, true);
        QVERIFY(conn.ocTwoTone());
        QCOMPARE(p1OcByte(conn), 0x01);
    }
};

QTEST_MAIN(TestOcMatrixTxPinAction)
#include "tst_oc_matrix_tx_pin_action.moc"

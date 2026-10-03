// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_antenna_kept.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 75 (R-IOS-30; the several-devices design, ruling
// 5.11a, D61): the receive antenna stays put on a band crossing while
// another device listens through it.
//
// Band tracking's per-band antenna switch (RadioModel's frequency handler)
// re-applies the new band's receive antenna only when no other device has a
// slice on a receiver fed by the ADC the relay feeds: every receiver on a
// 1-ADC board, ADC0's receivers on a 2-ADC board. Otherwise tuning goes
// ahead, the relay keeps the earlier band's receive antenna, and the person
// tuning is told (notice antennaKept). The new band's transmit antenna
// still applies at key-down, and the kept antenna comes back when the
// transmission ends. Once the other device has gone, the next crossing
// switches as always; nothing switches on its own when it leaves.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 75 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QSignalSpy>

#include "core/DdcAssignment.h"
#include "core/RadioConnection.h"
#include "core/accessories/AlexController.h"
#include "models/Band.h"

namespace {

// Captures the antenna routing the model sends (tst_antenna_routing_model's
// fake): connected, so applyAlexAntennaForBand reaches it.
class RoutingConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<AntennaRouting> calls;

    explicit RoutingConnection(QObject* parent = nullptr)
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
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting r) override { calls.append(r); }
    void setWatchdogEnabled(bool enabled) override { m_watchdogEnabled = enabled; }
    void sendTxIq(const float*, int) override {}
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

const QByteArray kA("device-a");
const QByteArray kB("device-b");

// A model with an Alex, two receivers and two slices: A's slice 0 on 20 m
// (receiver 0), B's slice 1 on 15 m (receiver 1). Receive antennas: ANT1
// on 20 m and 15 m, ANT2 on 40 m, ANT3 on 80 m; transmit ANT2 on 40 m.
struct Bench {
    RadioModel model;
    RoutingConnection* connection = new RoutingConnection();

    explicit Bench(int adcs = 1)
    {
        model.setCapsForTest(true);
        if (adcs > 1) {
            model.setWidebandTopologyForTest(adcs, adcs, adcs);
        }
        model.injectConnectionForTest(connection);
        model.configureStreamPool(2, 5, 192000);
        AlexController& alex = model.alexControllerMutable();
        alex.setRxAnt(Band::Band40m, 2);
        alex.setRxAnt(Band::Band80m, 3);
        alex.setTxAnt(Band::Band40m, 2);
        model.addSlice(QStringLiteral("pan-0"));
        model.addSlice(QStringLiteral("pan-1"));
        // Placed while both are A's, so nothing is kept on the way; B's
        // last, then A's, so the relay ends on 20 m.
        model.sliceOwnership()->setOwner(0, kA);
        model.sliceOwnership()->setOwner(1, kA);
        model.enableBandTrackingForTest();
        model.sliceById(1)->setFrequency(21074000.0);
        model.sliceById(0)->setFrequency(14074000.0);
        model.sliceOwnership()->setOwner(1, kB);
        connection->calls.clear();
    }

    ~Bench()
    {
        model.injectConnectionForTest(nullptr);
        delete connection;
    }

    SliceModel* a() { return model.sliceById(0); }
};

} // namespace

class TstAntennaKept : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("antenna-kept-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void init() { AppSettings::instance().clear(); }

    void theSetupIsTwoDevicesOnTheOneAdc()
    {
        Bench bench;
        QVERIFY(bench.model.sliceById(0)->streamIndex() >= 0);
        QVERIFY(bench.model.sliceById(1)->streamIndex() >= 0);
        QVERIFY(bench.model.sliceById(0)->streamIndex() != bench.model.sliceById(1)->streamIndex());
        QCOMPARE(bench.model.lastBand(), Band::Band20m);
    }

    void aCrossingWhileAnotherDeviceListensKeepsTheAntenna()
    {
        Bench bench;
        QSignalSpy kept(&bench.model, &RadioModel::receiveAntennaKept);
        bench.a()->setFrequency(7074000.0);
        // Tuning went ahead.
        QCOMPARE(bench.a()->frequency(), 7074000.0);
        QCOMPARE(bench.model.lastBand(), Band::Band40m);
        // The relay was not touched, and the person tuning is told.
        QVERIFY(bench.connection->calls.isEmpty());
        QCOMPARE(bench.model.keptReceiveAntennaBandForTest(), std::optional<Band>(Band::Band20m));
        QCOMPARE(kept.size(), 1);
        QCOMPARE(kept.first().at(0).toInt(), 0);
        QCOMPARE(kept.first().at(1).toString(), QStringLiteral("ANT1"));
        QCOMPARE(kept.first().at(2).value<QList<QByteArray>>(), QList<QByteArray>{kB});
        QCOMPARE(bench.a()->rxAntenna(), QStringLiteral("ANT1"));
    }

    void theTransmitAntennaAppliesAtKeyDownAndTheKeptOneComesBack()
    {
        Bench bench;
        bench.a()->setFrequency(7074000.0);
        QVERIFY(bench.model.keptReceiveAntennaBandForTest().has_value());
        // Key-down: every MOX change re-routes the antennas; the new band's
        // transmit antenna (ANT2 on 40 m) goes on the relay.
        bench.model.applyAlexAntennaForBandForTest(Band::Band40m, true);
        QCOMPARE(bench.connection->calls.last().tx, true);
        QCOMPARE(bench.connection->calls.last().trxAnt, 2);
        QCOMPARE(bench.connection->calls.last().txAnt, 2);
        // Key-up: the kept receive antenna, not 40 m's ANT2.
        bench.model.applyAlexAntennaForBandForTest(Band::Band40m, false);
        QCOMPARE(bench.connection->calls.last().tx, false);
        QCOMPARE(bench.connection->calls.last().trxAnt, 1);
        QVERIFY(bench.model.keptReceiveAntennaBandForTest().has_value());
    }

    void withNobodyElseListeningTheCrossingSwitches()
    {
        Bench bench;
        bench.model.sliceOwnership()->setOwner(1, kA);  // both slices A's own
        QSignalSpy kept(&bench.model, &RadioModel::receiveAntennaKept);
        bench.a()->setFrequency(7074000.0);
        QCOMPARE(kept.size(), 0);
        QVERIFY(!bench.model.keptReceiveAntennaBandForTest().has_value());
        QVERIFY(!bench.connection->calls.isEmpty());
        QCOMPARE(bench.connection->calls.last().trxAnt, 2);
        QCOMPARE(bench.a()->rxAntenna(), QStringLiteral("ANT2"));
    }

    void aSliceNobodyOwnsDoesNotKeepIt()
    {
        Bench bench;
        bench.model.sliceOwnership()->setOwner(1, QByteArray());
        bench.a()->setFrequency(7074000.0);
        QVERIFY(!bench.model.keptReceiveAntennaBandForTest().has_value());
        QCOMPARE(bench.connection->calls.last().trxAnt, 2);
    }

    void aCrossingOntoTheSameAntennaChangesNothingAndTellsNobody()
    {
        Bench bench;
        QSignalSpy kept(&bench.model, &RadioModel::receiveAntennaKept);
        bench.a()->setFrequency(18100000.0);  // 17 m, ANT1 like 20 m
        QCOMPARE(kept.size(), 0);
        QVERIFY(!bench.model.keptReceiveAntennaBandForTest().has_value());
    }

    void theOtherDeviceLeavingSwitchesNothingUntilTheNextCrossing()
    {
        Bench bench;
        bench.a()->setFrequency(7074000.0);
        QVERIFY(bench.model.keptReceiveAntennaBandForTest().has_value());
        bench.model.removeSlice(1);
        // Nothing switches on its own.
        QVERIFY(bench.connection->calls.isEmpty());
        QVERIFY(bench.model.keptReceiveAntennaBandForTest().has_value());
        // The next crossing switches as always: to 80 m's ANT3.
        QSignalSpy kept(&bench.model, &RadioModel::receiveAntennaKept);
        bench.a()->setFrequency(3573000.0);
        QCOMPARE(kept.size(), 0);
        QVERIFY(!bench.model.keptReceiveAntennaBandForTest().has_value());
        QCOMPARE(bench.connection->calls.last().trxAnt, 3);
        QCOMPARE(bench.a()->rxAntenna(), QStringLiteral("ANT3"));
    }

    void choosingTheCurrentBandsAntennaEndsTheKeeping()
    {
        Bench bench;
        bench.a()->setFrequency(7074000.0);
        QVERIFY(bench.model.keptReceiveAntennaBandForTest().has_value());
        bench.model.alexControllerMutable().setRxAnt(Band::Band40m, 3);
        QVERIFY(!bench.model.keptReceiveAntennaBandForTest().has_value());
        QCOMPARE(bench.connection->calls.last().trxAnt, 3);
    }

    void onATwoAdcBoardOnlyAdc0sListenersKeepIt()
    {
        {
            // B on ADC1 (an EXT input): the ANT relay does not feed it.
            Bench bench(2);
            DdcAssignment routing{};
            routing.streamDdc[bench.model.sliceById(0)->streamIndex()] = 0;
            routing.streamDdc[bench.model.sliceById(1)->streamIndex()] = 1;
            routing.adcCtrl1 = 1 << 2;  // DDC1 -> ADC1
            bench.model.publishDdcAssignmentForTest(routing);
            QCOMPARE(bench.model.adcForStream(bench.model.sliceById(1)->streamIndex()), 1);
            bench.connection->calls.clear();
            bench.a()->setFrequency(7074000.0);
            QVERIFY(!bench.model.keptReceiveAntennaBandForTest().has_value());
            QCOMPARE(bench.connection->calls.last().trxAnt, 2);
        }
        {
            // B on ADC0: kept.
            Bench bench(2);
            QCOMPARE(bench.model.adcForStream(bench.model.sliceById(1)->streamIndex()), 0);
            bench.a()->setFrequency(7074000.0);
            QCOMPARE(bench.model.keptReceiveAntennaBandForTest(),
                     std::optional<Band>(Band::Band20m));
        }
    }

    // ── Over the link: the person tuning gets antennaKept ────────────────

    void thePersonTuningIsToldAndWithTheOtherGoneTheNextCrossingSwitches()
    {
        Core core;
        core.model->setCapsForTest(true);
        core.model->configureStreamPool(2, 5, 192000);
        AlexController& alex = core.model->alexControllerMutable();
        alex.setRxAnt(Band::Band40m, 2);
        alex.setRxAnt(Band::Band80m, 3);
        core.model->sliceById(0)->setFrequency(14074000.0);
        core.model->enableBandTrackingForTest();
        Device a;
        Device b(QStringLiteral("Grant's iPad"), QStringLiteral("tablet"), QStringLiteral("Grant's iPad"));
        core.pair(a);
        core.pair(b);
        LoopbackTransport* appA = core.signIn(a);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA));
        QVERIFY(admitted(appB));
        const int bSlice = core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).first();
        core.model->sliceById(bSlice)->setFrequency(21074000.0);  // 15 m, ANT1 like 20 m
        QVERIFY(core.model->sliceById(bSlice)->streamIndex() >= 0);
        QCOMPARE(countOfType(appB->received(), 0, QStringLiteral("notice")), 0);

        appA->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite("slice:0", {f64("frequency", 7074000.0)}, 41)));
        QTRY_VERIFY(!propertyResult(appA, 41).isEmpty());
        QCOMPARE(propertyResult(appA, 41).value(QStringLiteral("results")).toArray().first()
                     .toObject().value(QStringLiteral("accepted")).toBool(false),
                 true);
        QTRY_COMPARE(countOfType(appA->received(), 0, QStringLiteral("notice")), 1);
        const QJsonObject told = ofType(appA->received(), QStringLiteral("notice")).first();
        QCOMPARE(told.value(QStringLiteral("kind")).toString(), QStringLiteral("antennaKept"));
        QCOMPARE(told.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("The antenna stays on ANT1 while Grant's iPad listens on it."));
        QCOMPARE(told.value(QStringLiteral("takeBack")).toBool(true), false);
        QVERIFY(!told.contains(QStringLiteral("byName")));
        QCOMPARE(told.value(QStringLiteral("slices")).toArray().first().toObject()
                     .value(QStringLiteral("sliceId")).toInt(),
                 0);
        // The sentence around the name is plain.
        QVERIFY(OperatorWording::isPlain(told.value(QStringLiteral("reason")).toString()
                                             .remove(QStringLiteral("Grant's iPad"))));
        QCOMPARE(countOfType(appB->received(), 0, QStringLiteral("notice")), 0);

        // B leaves; its slice closes (A holds a place). The next crossing
        // switches, with no notice.
        QCOMPARE(core.invoke(appB, "session.leave").value(QStringLiteral("accepted")).toBool(false),
                 true);
        QTRY_VERIFY(core.model->sliceOwnership()->ownedBy(b.key.fingerprint()).isEmpty());
        QVERIFY(core.model->keptReceiveAntennaBandForTest().has_value());
        appA->sendText(SessionMessages::encode(
            SessionMessages::propertyWrite("slice:0", {f64("frequency", 3573000.0)}, 42)));
        QTRY_VERIFY(!propertyResult(appA, 42).isEmpty());
        QVERIFY(!core.model->keptReceiveAntennaBandForTest().has_value());
        QCOMPARE(core.model->sliceById(0)->rxAntenna(), QStringLiteral("ANT3"));
        QTest::qWait(3 * StationServer::kDefaultDeltaFlushMs);
        QCOMPARE(countOfType(appA->received(), 0, QStringLiteral("notice")), 1);
    }
};

QTEST_GUILESS_MAIN(TstAntennaKept)
#include "tst_antenna_kept.moc"

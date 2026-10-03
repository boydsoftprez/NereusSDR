// no-port-check: NereusSDR-original unit-test file. It calls the same
// production accessors SliceMeterPump calls (RxChannel::getMeter), so a
// failure means the wiring is wrong, not that this file disagrees with
// Thetis about a meter formula.
// =================================================================
// tests/tst_slice_meter_pump_readings.cpp  (NereusSDR)
// =================================================================
//
// Remote-window parity Task 15 (R-R3-13, R-R3-49): meters from the Core.
//
//   1. The Core's SliceMeterPump writes each slice's ADC and AGC readings
//      (adcPeakDbfs, adcAverageDbfs, agcGainDb, agcPeakDb, agcAverageDb)
//      from the same WDSP meters a local window's MeterPoller reads, the
//      AGC gain as Thetis shows it (0 - RXA_AGC_GAIN), and clears them with
//      the S-meter readings when there is no reading.
//   2. The five are Outbound slice properties with no WRITE, carried to a
//      window under meterReadingsVersion 1 and never written back.
//   3. A window's Multimeter polling delay write sets the Core's pump rate
//      at once: the test counts the Core's polls before and after.
//
// Loopback only: no RF, no audio device, nothing keyed.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26 -- New test file for remote-window parity Task 15. J.J.
//                 Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-26 -- Trunk merge of remote transmit: the live channel's
//                 readings are read once the receive lane has filled the
//                 meter cache. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QMetaProperty>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "core/meters/SliceMeterPump.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/ConnectableRadioModel.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;
using NereusSDR::Test::LoopbackTransport;

namespace {

const char* const kReadings[] = {
    "adcPeakDbfs", "adcAverageDbfs", "agcGainDb", "agcPeakDb", "agcAverageDb",
};

void expectNoReadings(const SliceModel* slice)
{
    QCOMPARE(slice->adcPeakDbfs(), SliceMeterPump::kNoReadingDbm);
    QCOMPARE(slice->adcAverageDbfs(), SliceMeterPump::kNoReadingDbm);
    QCOMPARE(slice->agcGainDb(), SliceMeterPump::kNoReadingDbm);
    QCOMPARE(slice->agcPeakDb(), SliceMeterPump::kNoReadingDbm);
    QCOMPARE(slice->agcAverageDb(), SliceMeterPump::kNoReadingDbm);
}

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:15");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

struct Session {
    explicit Session(const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
    {
        core = makeStationRadioModel();
        server = std::make_unique<StationServer>(
            core.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    bool connect()
    {
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }
    void writeSetting(const QString& key, const QString& value)
    {
        windowEnd->sendText(SessionMessages::encode(
            SessionMessages::settingsWrite(key, value, QStringLiteral("window"))));
    }

    QTemporaryDir settingsDir;
    AppSettings settings;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
};

} // namespace

class TstSliceMeterPumpReadings : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_securityDir.isValid());
        AppSettings::setProfileOverride(
            QStringLiteral("slice-meter-pump-readings-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                         QStringLiteral("7"));
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // The five are read-only telemetry: a NOTIFY, no WRITE, Outbound, and
    // no reading until the pump or the mirror writes one.
    void readingsAreOutboundTelemetry()
    {
        SliceModel slice;
        const QMetaObject* mo = slice.metaObject();
        for (const char* name : kReadings) {
            const int index = mo->indexOfProperty(name);
            QVERIFY2(index >= 0, name);
            const QMetaProperty prop = mo->property(index);
            QVERIFY2(!prop.isWritable(), name);
            QVERIFY2(prop.hasNotifySignal(), name);
            QCOMPARE(prop.metaType().id(), QMetaType::Double);
            QCOMPARE(MirrorPolicy::directionFor(QByteArrayLiteral("SliceModel"), QByteArray(name)),
                     MirrorDirection::Outbound);
            QVERIFY2(!MirrorPolicy::inboundAllowed(QByteArrayLiteral("SliceModel"), QByteArray(name)),
                     name);
        }
        expectNoReadings(&slice);

        // A window applies the Core's value through the mirror hook.
        QSignalSpy gain(&slice, &SliceModel::agcGainDbChanged);
        QCOMPARE(slice.applyMirroredValue("agcGainDb", QVariant(42.5)), QString());
        QCOMPARE(slice.agcGainDb(), 42.5);
        QCOMPARE(gain.count(), 1);
        QCOMPARE(slice.applyMirroredValue("agcGainDb", QVariant(42.5)), QString());
        QCOMPARE(gain.count(), 1);
        QCOMPARE(slice.applyMirroredValue("adcPeakDbfs", QVariant(-12.0)), QString());
        QCOMPARE(slice.applyMirroredValue("adcAverageDbfs", QVariant(-30.0)), QString());
        QCOMPARE(slice.applyMirroredValue("agcPeakDb", QVariant(-60.0)), QString());
        QCOMPARE(slice.applyMirroredValue("agcAverageDb", QVariant(-70.0)), QString());
        QCOMPARE(slice.adcPeakDbfs(), -12.0);
        QCOMPARE(slice.adcAverageDbfs(), -30.0);
        QCOMPARE(slice.agcPeakDb(), -60.0);
        QCOMPARE(slice.agcAverageDb(), -70.0);
    }

    // Thetis shows the AGC gain as 0 - RXA_AGC_GAIN (console.cs:46914).
    void agcGainReadingIsThetisReading()
    {
        QCOMPARE(SliceMeterPump::thetisAgcGainReading(-35.0), 35.0);
        QCOMPARE(SliceMeterPump::thetisAgcGainReading(12.5), -12.5);
        // WDSP's -400 before its first measurement stays no reading, never
        // a 400 dB gain.
        QCOMPARE(SliceMeterPump::thetisAgcGainReading(-400.0), SliceMeterPump::kNoReadingDbm);
        QCOMPARE(SliceMeterPump::thetisAgcGainReading(qQNaN()), SliceMeterPump::kNoReadingDbm);
    }

    // Readings present: the pump writes the live channel's five meters,
    // and clears them with the S-meter readings on a lost link.
    void pumpWritesTheLiveChannelsReadings()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        RadioModel& model = harness->model();
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);
        pump->stop();
        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice != nullptr);
        RxChannel* ch = model.wdspEngine()->rxChannel(slice->sliceIndex());
        QVERIFY(ch != nullptr);
        expectNoReadings(slice);

        // Trunk merge of remote transmit (R-R3-39): the pump reads the cache
        // the receive lane refreshes. Its first poll asks the lane for one
        // and shows no reading until the lane has read the channel; the
        // poll after that carries the channel's readings.
        const auto pollWithReadings = [&]() {
            pump->poll();
            QTRY_VERIFY(ch->meterReadingReady());
            pump->poll();
        };

        // The raw WDSP meters: no RXOffset on any of them (Thetis adds it to
        // the two signal readings only).
        pollWithReadings();
        QCOMPARE(slice->adcPeakDbfs(), ch->getMeter(RxMeterType::AdcPeak));
        QCOMPARE(slice->adcAverageDbfs(), ch->getMeter(RxMeterType::AdcAvg));
        QCOMPARE(slice->agcGainDb(),
                 SliceMeterPump::thetisAgcGainReading(ch->getMeter(RxMeterType::AgcGain)));
        QCOMPARE(slice->agcPeakDb(), ch->getMeter(RxMeterType::AgcPeak));
        QCOMPARE(slice->agcAverageDb(), ch->getMeter(RxMeterType::AgcAvg));

        // A lost link clears them with the S-meter readings, whatever they
        // held (seeded here, as this channel sees no signal).
        const auto seed = [slice]() {
            slice->setAdcPeakDbfs(-11.0);
            slice->setAdcAverageDbfs(-22.0);
            slice->setAgcGainDb(33.0);
            slice->setAgcPeakDb(-44.0);
            slice->setAgcAverageDb(-55.0);
        };
        seed();
        model.setConnectionStateForTest(ConnectionState::LinkLost);
        pump->poll();
        expectNoReadings(slice);
        QCOMPARE(slice->signalPeakDbm(), SliceMeterPump::kNoReadingDbm);

        // Back to Connected: the channel's readings again.
        seed();
        model.setConnectionStateForTest(ConnectionState::Connected);
        pollWithReadings();
        QCOMPARE(slice->agcGainDb(),
                 SliceMeterPump::thetisAgcGainReading(ch->getMeter(RxMeterType::AgcGain)));
        QCOMPARE(slice->agcAverageDb(), ch->getMeter(RxMeterType::AgcAvg));
        QCOMPARE(slice->adcPeakDbfs(), ch->getMeter(RxMeterType::AdcPeak));
        harness.reset();
    }

    // The Core offers meterReadingsVersion 1 and its readings reach the
    // window's slice; a window never writes one back.
    void readingsReachTheWindow()
    {
        Session s(m_securityDir.path(), this);
        QVERIFY(s.connect());
        QCOMPARE(s.server->buildCapabilities().meterReadingsVersion, 1);
        QCOMPARE(s.client->capabilities().meterReadingsVersion, 1);

        // This Core has no WDSP channel, so its pump would clear every
        // reading before the mirror carries it: these setters stand in.
        s.core->sliceMeterPump()->stop();
        SliceModel* coreSlice = s.core->slices().first();
        QTRY_VERIFY(s.window.sliceById(coreSlice->sliceIndex()) != nullptr);
        SliceModel* windowSlice = s.window.sliceById(coreSlice->sliceIndex());
        coreSlice->setAdcPeakDbfs(-18.0);
        coreSlice->setAdcAverageDbfs(-41.0);
        coreSlice->setAgcGainDb(37.0);
        coreSlice->setAgcPeakDb(-55.0);
        coreSlice->setAgcAverageDb(-63.0);
        QTRY_COMPARE(windowSlice->adcPeakDbfs(), -18.0);
        QTRY_COMPARE(windowSlice->adcAverageDbfs(), -41.0);
        QTRY_COMPARE(windowSlice->agcGainDb(), 37.0);
        QTRY_COMPARE(windowSlice->agcPeakDb(), -55.0);
        QTRY_COMPARE(windowSlice->agcAverageDb(), -63.0);

        s.coreEnd->clearReceived();
        windowSlice->setAgcGainDb(5.0);
        windowSlice->setFrequency(14075100.0);   // a flush barrier
        QTRY_COMPARE(coreSlice->frequency(), 14075100.0);
        for (const QByteArray& wire : s.coreEnd->received()) {
            SessionMessage message;
            if (!SessionMessages::decode(wire, &message)
                || message.kind != SessionMessageKind::PropertyWrite) {
                continue;
            }
            for (const MirrorUpdate& update : message.updates) {
                QVERIFY(update.name != "agcGainDb");
            }
        }
        QCOMPARE(coreSlice->agcGainDb(), 37.0);
    }

    // A Core with no meter pump (no local radio model) offers 0.
    void coreWithoutPumpOffersNoReadings()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(remote.sliceMeterPump() == nullptr);
        StationServer server(&remote, settings,
                             NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        QCOMPARE(server.meterReadingsVersion(), 0);
    }

    // B3.11: Setup > Display > Multimeter > Polling delay from a window
    // changes the Core's pump rate at once. The Core's polls are counted at
    // 2000 ms and then at 20 ms.
    void pollingDelayFromAWindowSetsTheCoresRate()
    {
        Session s(m_securityDir.path(), this);
        QVERIFY(s.connect());
        SliceMeterPump* pump = s.core->sliceMeterPump();
        QVERIFY(pump != nullptr);
        QCOMPARE(pump->intervalMs(), 100);
        QSignalSpy polls(pump, &SliceMeterPump::polled);

        s.writeSetting(QStringLiteral("MultimeterDelayMs"), QStringLiteral("2000"));
        QTRY_COMPARE(pump->intervalMs(), 2000);
        polls.clear();
        QTest::qWait(1000);
        QVERIFY2(polls.count() <= 1, qPrintable(QString::number(polls.count())));

        s.writeSetting(QStringLiteral("MultimeterDelayMs"), QStringLiteral("20"));
        QTRY_COMPARE(pump->intervalMs(), 20);
        polls.clear();
        // At 2000 ms this window could hold at most one poll.
        QTRY_VERIFY_WITH_TIMEOUT(polls.count() >= 5, 1500);

        // Clamped as the pump clamps; a removed key is the 100 ms default.
        s.writeSetting(QStringLiteral("MultimeterDelayMs"), QStringLiteral("0"));
        QTRY_COMPARE(pump->intervalMs(), 10);
        s.windowEnd->sendText(SessionMessages::encode(
            SessionMessages::settingsRemove(QStringLiteral("MultimeterDelayMs"))));
        QTRY_COMPARE(pump->intervalMs(), 100);
    }

    // applyMeterSetting takes only its own key.
    void otherKeysLeaveTheRateAlone()
    {
        RadioModel model;
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);
        pump->setIntervalMs(250);
        QVERIFY(!model.applyMeterSetting(QStringLiteral("MultimeterPeakHoldMs"),
                                         QStringLiteral("40")));
        QCOMPARE(pump->intervalMs(), 250);
        QVERIFY(model.applyMeterSetting(QStringLiteral("MultimeterDelayMs"),
                                        QStringLiteral("400")));
        QCOMPARE(pump->intervalMs(), 400);
    }

private:
    QTemporaryDir m_securityDir;
};

QTEST_MAIN(TstSliceMeterPumpReadings)
#include "tst_slice_meter_pump_readings.moc"

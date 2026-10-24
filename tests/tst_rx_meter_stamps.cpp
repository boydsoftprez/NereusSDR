// no-port-check: NereusSDR-original. No Thetis logic is ported here.
// =================================================================
// tests/tst_rx_meter_stamps.cpp  (NereusSDR)
// =================================================================
//
// RX meter stamps: the Core stamps each slice's RX meter readings with the
// time its receive lane read them, and a remote window holds them until
// their audio plays, on the map its spectrum is presented by.
//
//   1. rxMeterStampsVersion 1 reaches only a peer that declared
//      rxMeterStamps 1, last before coreBuildInfo, and reads back.
//   2. meterReadingNs is Outbound and gated on rxMeterStamps 1, and only a
//      declaring peer is sent it.
//   3. The Core's pump writes readings from one lane refresh, stamped with
//      that refresh's read time.
//   4. A remote window holds a stamped delta's readings until stamp + map
//      and applies the rest of the delta at once; it applies them on
//      arrival with no map, past the 1.5 s cap, and when unstamped; an
//      unstamped reading waits behind the slice's held ones.
//   5. The window's map for the hold is none from a Core without the
//      display clock and while no display is open.
//
// Loopback only: no RF, no audio device, nothing keyed.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QSignalSpy>

#include <cmath>

#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/meters/SliceMeterPump.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/ConnectableRadioModel.h"
#include "gui/RemoteMediaController.h"

namespace {

bool sawProperty(const LoopbackTransport* app, const QString& property)
{
    for (const QByteArray& wire : app->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        for (const QJsonValue& p : o.value(QStringLiteral("properties")).toArray()) {
            if (p.toObject().value(QStringLiteral("name")).toString() == property) {
                return true;
            }
        }
    }
    return false;
}

constexpr qint64 kMs = 1'000'000;

// A remote window on a Core whose meter pump is stopped, so only the test
// writes the Core slice's readings and stamp. The window's hold clock is
// the test's: `now` and `map`.
struct Window {
    explicit Window(QObject* parent)
    {
        if (SliceMeterPump* pump = core.model->sliceMeterPump()) {
            pump->stop();
        }
        client.setRxMeterHoldClock([this]() { return now; }, [this]() { return map; });
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("meter-station"), parent);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("meter-client"), parent);
        stationEnd->linkTo(clientEnd);
        client.startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        ok = completed.wait(5000) || completed.count() == 1;
    }
    SliceModel* coreSlice() const { return core.model->sliceById(0); }
    SliceModel* windowSlice() const { return remote.sliceById(0); }

    Core core{/*upgradedWithToken=*/true};
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&remote, &proxy};
    qint64 now = 1'000'000 * kMs;
    std::optional<qint64> map;
    bool ok = false;
};

} // namespace

class TstRxMeterStamps : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }

    void version_onlyToADeclaringPeer()
    {
        // coreBuildInfo is sent only when the Core knows its version.
        const QString savedVersion = QCoreApplication::applicationVersion();
        const auto restore = qScopeGuard([&savedVersion]() {
            QCoreApplication::setApplicationVersion(savedVersion);
        });
        QCoreApplication::setApplicationVersion(QStringLiteral("0.5.2"));
        Core core;
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        QHash<QByteArray, int> asks = kHolder;
        asks.insert(QByteArrayLiteral("rxMeterStamps"), 1);
        asks.insert(QByteArrayLiteral("coreBuildInfo"), 1);
        LoopbackTransport* appA = core.signIn(a, asks);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));
        QCOMPARE(capability(appA->received(), QStringLiteral("rxMeterStampsVersion")), 1);
        QVERIFY(!capability(appB->received(), QStringLiteral("rxMeterStampsVersion")).has_value());
        const QJsonArray caps = firstOfType(appA->received(), QStringLiteral("capabilities"))
                                    .value(QStringLiteral("properties")).toArray();
        QVERIFY(caps.size() >= 2);
        QCOMPARE(caps.at(caps.size() - 2).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("rxMeterStampsVersion"));
        QCOMPARE(caps.last().toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("coreBuildInfo"));

        StationCapabilities sent;
        sent.rxMeterStampsVersion = 1;
        QCOMPARE(StationCapabilities::fromUpdates(sent.toUpdates()).rxMeterStampsVersion, 1);
        QCOMPARE(StationCapabilities::fromUpdates(StationCapabilities{}.toUpdates())
                     .rxMeterStampsVersion, 0);
    }

    void policy_outboundAndGated()
    {
        QVERIFY(MirrorPolicy::hasExplicitEntry("SliceModel", "meterReadingNs"));
        QCOMPARE(MirrorPolicy::directionFor("SliceModel", "meterReadingNs"),
                 MirrorDirection::Outbound);
        QVERIFY(!MirrorPolicy::inboundAllowed("SliceModel", "meterReadingNs"));
        const MirrorPolicy::FeatureGate* gate =
            MirrorPolicy::featureGateFor("SliceModel", "meterReadingNs");
        QVERIFY(gate != nullptr);
        QCOMPARE(QByteArray(gate->feature), QByteArrayLiteral("rxMeterStamps"));
        QCOMPARE(gate->minVersion, 1);
    }

    // The window's map for the hold: the follower's last map, but none from
    // a Core without the display clock or while no display is open (the
    // map is measured only as display frames are presented, so it may be
    // stale): the readings then show on arrival.
    void holdMap_onlyWhileADisplayIsPresented()
    {
        constexpr qint64 kMap = 200 * kMs;
        QCOMPARE(RemoteMediaController::rxMeterHoldMapFor(true, true, kMap),
                 std::optional<qint64>(kMap));
        QVERIFY(!RemoteMediaController::rxMeterHoldMapFor(true, false, kMap).has_value());
        QVERIFY(!RemoteMediaController::rxMeterHoldMapFor(false, true, kMap).has_value());
        QVERIFY(!RemoteMediaController::rxMeterHoldMapFor(true, true, std::nullopt).has_value());
    }

    // A declaring peer is sent the stamp; one that did not declare it never
    // sees the name, and still gets the reading.
    void stamp_onlyToADeclaringPeer()
    {
        Core core;
        if (SliceMeterPump* pump = core.model->sliceMeterPump()) {
            pump->stop();
        }
        Device a;
        Device b(QStringLiteral("iPad"), QStringLiteral("tablet"));
        core.pair(a);
        core.pair(b);
        QHash<QByteArray, int> asks = kHolder;
        asks.insert(QByteArrayLiteral("rxMeterStamps"), 1);
        LoopbackTransport* appA = core.signIn(a, asks);
        LoopbackTransport* appB = core.signIn(b);
        QVERIFY(admitted(appA) && admitted(appB));

        // Each device is shown its own slice: the phone slice 0 (A), the
        // tablet slice 1 (B).
        for (int id = 0; id < 2; ++id) {
            SliceModel* slice = core.model->sliceById(id);
            QVERIFY(slice != nullptr);
            slice->setAgcGainDb(-12.0);
            slice->setMeterReadingNs(123456789);
        }
        const QString keyA = QString::fromLatin1(ObjectRegistry::keyForSlice(0));
        const QString keyB = QString::fromLatin1(ObjectRegistry::keyForSlice(1));
        QTRY_COMPARE(latest(appA->received(), keyA, QStringLiteral("meterReadingNs")).toInteger(),
                     qint64{123456789});
        QTRY_COMPARE(latest(appB->received(), keyB, QStringLiteral("agcGainDb")).toDouble(), -12.0);
        QTest::qWait(100);
        QVERIFY(!sawProperty(appB, QStringLiteral("meterReadingNs")));
    }

    // The pump's readings come from one lane refresh, stamped with the time
    // the lane read it; the stamp moves with each refresh.
    void pumpStampsReadingsWithTheirLaneRead()
    {
        std::unique_ptr<NereusSDR::Test::ConnectableRadioModel> harness =
            NereusSDR::Test::ConnectableRadioModel::create(
                10000, RadioModel::Role::Local, [](RadioModel& m) {
                    if (SliceMeterPump* p = m.sliceMeterPump()) {
                        p->stop();
                    }
                });
        QVERIFY(harness != nullptr);
        RadioModel& model = harness->model();
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);
        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice != nullptr);
        RxChannel* ch = model.wdspEngine()->rxChannel(slice->sliceIndex());
        QVERIFY(ch != nullptr);

        const qint64 before = RxChannel::meterClockNs();
        pump->poll();
        QTRY_VERIFY(ch->meterReadingReady());
        QTRY_VERIFY(ch->meterReadings().readNs > before);
        // The pump's poll asks the lane for its next refresh, and so does
        // this test's read, so the lane may refresh between them. Poll and
        // read until it did not: the test then holds the snapshot the pump
        // wrote.
        RxChannel::MeterReadings readings;
        bool samePoll = false;
        for (int attempt = 0; attempt < 50 && !samePoll; ++attempt) {
            pump->poll();
            readings = ch->meterReadings();
            samePoll = readings.readNs == slice->meterReadingNs();
            if (!samePoll) {
                QTest::qWait(5);
            }
        }
        QVERIFY(samePoll);
        const qint64 after = RxChannel::meterClockNs();
        const qint64 first = slice->meterReadingNs();
        QVERIFY2(first > before && first <= after,
                 qPrintable(QStringLiteral("%1 not in (%2, %3]").arg(first).arg(before).arg(after)));
        // The readings the pump wrote are the snapshot's: the ADC and AGC
        // readings as read (AGC gain as Thetis shows it), and both signal
        // readings with the same slice offset.
        QCOMPARE(slice->adcPeakDbfs(), readings.value(RxMeterType::AdcPeak));
        QCOMPARE(slice->adcAverageDbfs(), readings.value(RxMeterType::AdcAvg));
        QCOMPARE(slice->agcGainDb(),
                 SliceMeterPump::thetisAgcGainReading(readings.value(RxMeterType::AgcGain)));
        QCOMPARE(slice->agcPeakDb(), readings.value(RxMeterType::AgcPeak));
        QCOMPARE(slice->agcAverageDb(), readings.value(RxMeterType::AgcAvg));
        const double peakOffsetDb = slice->signalPeakDbm() - readings.value(RxMeterType::SignalPeak);
        const double averageOffsetDb =
            slice->signalAverageDbm() - readings.value(RxMeterType::SignalAvg);
        QVERIFY2(std::abs(peakOffsetDb - averageOffsetDb) < 1e-9,
                 qPrintable(QStringLiteral("peak offset %1, average offset %2")
                                .arg(peakOffsetDb).arg(averageOffsetDb)));
        // The lane's next refresh moves the stamp.
        QTRY_VERIFY(ch->meterReadings().readNs > first);
        pump->poll();
        QVERIFY(slice->meterReadingNs() > first);

        // A lost link clears the readings, stamped with the clear's time;
        // a clear that changes nothing leaves the stamp.
        model.setConnectionStateForTest(ConnectionState::LinkLost);
        const qint64 clearBefore = RxChannel::meterClockNs();
        pump->poll();
        const qint64 cleared = slice->meterReadingNs();
        QVERIFY(cleared >= clearBefore);
        QCOMPARE(slice->agcGainDb(), SliceMeterPump::kNoReadingDbm + 0.0);
        pump->poll();
        QCOMPARE(slice->meterReadingNs(), cleared);
        harness.reset();
    }

    // A stamped delta's readings wait for stamp + map; the rest of it
    // applies at once.
    void windowHoldsStampedReadingsUntilTheirAudio()
    {
        Window w(this);
        QVERIFY(w.ok);
        QTRY_VERIFY(w.windowSlice() != nullptr);
        w.map = 300 * kMs;
        const double frequency = w.coreSlice()->frequency() + 1000.0;
        // Measured 100 ms ago: due 200 ms from now.
        const qint64 stamp = w.now - 100 * kMs;
        w.coreSlice()->setAgcGainDb(-12.0);
        w.coreSlice()->setSignalPeakDbm(-73.0);
        w.coreSlice()->setMeterReadingNs(stamp);
        w.coreSlice()->setFrequency(frequency);

        QTRY_COMPARE(w.windowSlice()->frequency(), frequency);
        QCOMPARE(w.client.heldRxMeterReadingsForTest(), 1);
        QVERIFY(w.windowSlice()->agcGainDb() != -12.0);
        QVERIFY(w.windowSlice()->meterReadingNs() != stamp);

        // The window's clock stands still: still held after its timer.
        QTest::qWait(250);
        QCOMPARE(w.client.heldRxMeterReadingsForTest(), 1);
        QVERIFY(w.windowSlice()->signalPeakDbm() != -73.0);

        w.now += 200 * kMs;
        QTRY_COMPARE(w.windowSlice()->agcGainDb(), -12.0);
        QCOMPARE(w.windowSlice()->signalPeakDbm(), -73.0);
        QCOMPARE(w.windowSlice()->meterReadingNs(), stamp);
        QCOMPARE(w.client.heldRxMeterReadingsForTest(), 0);
    }

    // No map yet (no frame presented, or an older Core): on arrival.
    void windowWithNoMapAppliesOnArrival()
    {
        Window w(this);
        QVERIFY(w.ok);
        QTRY_VERIFY(w.windowSlice() != nullptr);
        w.coreSlice()->setAgcGainDb(-21.0);
        w.coreSlice()->setMeterReadingNs(w.now);
        QTRY_COMPARE(w.windowSlice()->agcGainDb(), -21.0);
        QCOMPARE(w.client.heldRxMeterReadingsForTest(), 0);
    }

    // A wait past the presenter's cap means the stamps and the map
    // disagree: on arrival.
    void windowAppliesPastTheCapOnArrival()
    {
        Window w(this);
        QVERIFY(w.ok);
        QTRY_VERIFY(w.windowSlice() != nullptr);
        w.map = StationClient::kRxMeterMaxHoldNs + 500 * kMs;
        w.coreSlice()->setAgcPeakDb(-31.0);
        w.coreSlice()->setMeterReadingNs(w.now);
        QTRY_COMPARE(w.windowSlice()->agcPeakDb(), -31.0);
        QCOMPARE(w.client.heldRxMeterReadingsForTest(), 0);
    }

    // An unstamped reading applies on arrival, but behind the slice's
    // held readings when it has some, so readings never apply out of order.
    void unstampedReadingsKeepTheirOrder()
    {
        Window w(this);
        QVERIFY(w.ok);
        QTRY_VERIFY(w.windowSlice() != nullptr);
        w.map = 300 * kMs;

        // No stamp change: on arrival.
        w.coreSlice()->setAdcPeakDbfs(-41.0);
        QTRY_COMPARE(w.windowSlice()->adcPeakDbfs(), -41.0);

        // A stamped reading is held, and the unstamped one after it waits.
        const double frequency = w.coreSlice()->frequency() + 500.0;
        w.coreSlice()->setAgcAverageDb(-51.0);
        w.coreSlice()->setMeterReadingNs(w.now);
        QTRY_COMPARE(w.client.heldRxMeterReadingsForTest(), 1);
        w.coreSlice()->setAdcPeakDbfs(-42.0);
        w.coreSlice()->setFrequency(frequency);
        QTRY_COMPARE(w.windowSlice()->frequency(), frequency);
        QCOMPARE(w.client.heldRxMeterReadingsForTest(), 2);
        QCOMPARE(w.windowSlice()->adcPeakDbfs(), -41.0);

        w.now += 300 * kMs;
        QTRY_COMPARE(w.windowSlice()->adcPeakDbfs(), -42.0);
        QCOMPARE(w.windowSlice()->agcAverageDb(), -51.0);
        QCOMPARE(w.client.heldRxMeterReadingsForTest(), 0);
    }

    // With the clock removed, held readings apply at once.
    void removingTheClockAppliesHeldReadings()
    {
        Window w(this);
        QVERIFY(w.ok);
        QTRY_VERIFY(w.windowSlice() != nullptr);
        w.map = 300 * kMs;
        w.coreSlice()->setAgcGainDb(-17.0);
        w.coreSlice()->setMeterReadingNs(w.now);
        QTRY_COMPARE(w.client.heldRxMeterReadingsForTest(), 1);
        w.client.setRxMeterHoldClock({}, {});
        QCOMPARE(w.client.heldRxMeterReadingsForTest(), 0);
        QCOMPARE(w.windowSlice()->agcGainDb(), -17.0);
    }
};

QTEST_MAIN(TstRxMeterStamps)
#include "tst_rx_meter_stamps.moc"

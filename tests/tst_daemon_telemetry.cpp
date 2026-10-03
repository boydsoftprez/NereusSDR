// no-port-check: NereusSDR-original. Exercises the bounded Core telemetry
// collector, including its real queued RadioConnection observation boundary.

#include <QtTest>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/daemon/DaemonTelemetryController.h"
#include "core/daemon/HostTelemetrySampler.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "models/RadioModel.h"

#include <QDir>
#include <QFile>
#include <QPointer>
#include <QRegularExpression>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThread>

#include <memory>
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

class NullRadioConnection final : public RadioConnection {
    Q_OBJECT
public:
    using RadioConnection::RadioConnection;
    void init() override {}
    void connectToRadio(const RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
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
    void setWatchdogEnabled(bool) override {}

    void markConnectedForDiagnostics(quint16 port)
    {
        m_radioInfo.port = port;
        m_radioInfo.macAddress = QStringLiteral("AA:BB:CC:DD:EE:FF");
        setState(ConnectionState::Connected);
    }
    void retireForDiagnostics() { setState(ConnectionState::LinkLost); }
    void observeAdcsForDiagnostics(quint8 mask, quint8 bits)
    {
        observeAdcOverloads(mask, bits);
    }

    // R-R3-32 (parity Task 6): drive the link counters as the receive path
    // would: `count` datagrams 1 ms apart on stream 0, one sequence error.
    void feedLinkStatsForTest(int count)
    {
        const qint64 start = RadioLinkStats::nowUs() - count * 1000;
        for (int i = 0; i < count; ++i) {
            const qint64 at = start + i * 1000;
            m_linkStats.noteDatagram(at);
            m_linkStats.noteSequenced(at, i == count - 1 ? 1U : 0U);
            m_linkStats.noteStreamArrival(0, quint32(i), at, 1000.0);
        }
    }
};

struct SessionHarness {
    QTemporaryDir directory;
    AppSettings settings;
    RadioModel station;
    StationServer server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client{&remote, &proxy};

    SessionHarness()
        : settings(directory.filePath(QStringLiteral("station.settings")))
        , server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()))
    {
        Q_ASSERT(directory.isValid());
    }

    void connectClient(QObject* owner)
    {
        auto* stationLink = new LoopbackTransport(QStringLiteral("telemetry-station"),
                                                  owner);
        auto* clientLink = new LoopbackTransport(QStringLiteral("telemetry-client"),
                                                 owner);
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
    }
};

StationTelemetrySnapshot lastSnapshot(const QSignalSpy& samples)
{
    return qvariant_cast<StationTelemetrySnapshot>(samples.constLast().at(0));
}

struct ThreadConnection {
    explicit ThreadConnection(RadioModel& owner) : model(owner) {}

    ~ThreadConnection()
    {
        if (model.connection() == connection.data()) {
            model.injectConnectionForTest(nullptr);
        }
        if (connection) {
            if (!thread.isRunning()) {
                thread.start();
            }
            const std::shared_ptr<QSemaphore> dispatched
                = std::make_shared<QSemaphore>();
            NullRadioConnection* const retiring = connection.data();
            QMetaObject::invokeMethod(retiring, [retiring, dispatched] {
                retiring->deleteLater();
                dispatched->release();
            });
            dispatched->tryAcquire(1, 1000);
            thread.quit();
            thread.wait();
        } else if (thread.isRunning()) {
            thread.quit();
            thread.wait();
        }
    }

    void moveToOwnerThread(bool start)
    {
        connection->moveToThread(&thread);
        if (start) {
            thread.start();
        }
    }

    RadioModel& model;
    QThread thread;
    QPointer<NullRadioConnection> connection{new NullRadioConnection};
};

// A procfs/sysfs tree under a temporary root, so no test reads the build
// machine's own /proc or /sys.
struct HostFixture {
    QTemporaryDir directory;

    void write(const QString& relative, const QByteArray& contents)
    {
        const QString path = directory.filePath(relative);
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(contents), contents.size());
    }

    void cpu(const QByteArray& statLine, quint64 processTicks)
    {
        write(QStringLiteral("proc/stat"), statLine);
        write(QStringLiteral("proc/self/stat"),
              "77 (nereusd) S 1 77 77 0 -1 0 0 0 0 0 "
                  + QByteArray::number(processTicks) + " 0 0 0 20 0 4 0\n");
    }

    HostFixture()
    {
        cpu("cpu  100 0 100 700 100 0 0 0 0 0\n", 10);
        write(QStringLiteral("proc/meminfo"),
              "MemTotal:        8000000 kB\nMemAvailable:    6500000 kB\n");
        write(QStringLiteral("proc/self/status"), "Name:\tnereusd\nVmRSS:\t   51234 kB\n");
        write(QStringLiteral("sys/class/thermal/thermal_zone0/type"), "soc-thermal\n");
        write(QStringLiteral("sys/class/thermal/thermal_zone0/temp"), "47500\n");
    }
};

} // namespace

class TstDaemonTelemetry final : public QObject {
    Q_OBJECT
private slots:
    void audioRatesUseElapsedTimeAndRetireInvalidBaselines()
    {
        SessionHarness h;
        qint64 nowMs = 100;
        DaemonAudioDiagnostics audio;
        audio.activeContext = true;
        audio.contextGeneration = 7;
        audio.sender.source.capturedValidRateFrames = 100;
        audio.sender.source.sourceDropEvents = 2;
        audio.sender.encodedPackets = 4;
        audio.sender.encodeFailures = 1;
        audio.sendAccepted = 3;
        audio.sendRejected = 1;

        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr,
            [&] { return nowMs; }, [&] { return audio; });
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());

        nowMs = 200;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        StationTelemetrySnapshot snapshot = lastSnapshot(samples);
        QCOMPARE(snapshot.sequence, quint32{1});
        QCOMPARE(snapshot.sampledElapsedMs, qint64{100});
        QVERIFY(snapshot.audio.active);
        QCOMPARE(snapshot.audio.contextGeneration, quint32{7});
        QVERIFY(!snapshot.audio.sourceFramesPerSecond);
        QVERIFY(!snapshot.audio.sendAcceptedPerSecond);

        audio.sender.source.capturedValidRateFrames += 120;
        audio.sender.source.sourceDropEvents += 1;
        audio.sender.encodedPackets += 3;
        audio.sender.encodeFailures += 2;
        audio.sendAccepted += 2;
        audio.sendRejected += 1;
        nowMs = 500; // 300 ms, deliberately not the nominal timer period.
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 2);
        snapshot = lastSnapshot(samples);
        QCOMPARE(*snapshot.audio.sourceFramesPerSecond, 400.0);
        QCOMPARE(*snapshot.audio.sourceDropsPerSecond, 1000.0 / 300.0);
        QCOMPARE(*snapshot.audio.encodedPacketsPerSecond, 10.0);
        QCOMPARE(*snapshot.audio.encodeFailuresPerSecond, 2000.0 / 300.0);
        QCOMPARE(*snapshot.audio.sendAcceptedPerSecond, 2000.0 / 300.0);
        QCOMPARE(*snapshot.audio.sendRejectedPerSecond, 1000.0 / 300.0);

        nowMs = 1000;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 3);
        snapshot = lastSnapshot(samples);
        QVERIFY(snapshot.audio.sourceFramesPerSecond);
        QCOMPARE(*snapshot.audio.sourceFramesPerSecond, 0.0);
        QCOMPARE(*snapshot.audio.sourceDropsPerSecond, 0.0);

        // One regressing counter invalidates this complete rate set; no
        // mixture of old and reset lifetimes is published.
        audio.sender.encodedPackets = 1;
        nowMs = 1250;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 4);
        snapshot = lastSnapshot(samples);
        QVERIFY(!snapshot.audio.sourceFramesPerSecond);
        QVERIFY(!snapshot.audio.encodedPacketsPerSecond);
        QVERIFY(!snapshot.audio.sendAcceptedPerSecond);

        audio.activeContext = false;
        nowMs = 1500;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 5);
        snapshot = lastSnapshot(samples);
        QVERIFY(!snapshot.audio.active);
        QVERIFY(!snapshot.audio.sourceFramesPerSecond);

        // Retirement and a changed generation both establish baselines;
        // neither invents zero-rate activity at the boundary.
        audio.activeContext = true;
        audio.contextGeneration = 8;
        audio.sender.source.capturedValidRateFrames = 10;
        audio.sender.source.sourceDropEvents = 0;
        audio.sender.encodedPackets = 1;
        audio.sender.encodeFailures = 0;
        audio.sendAccepted = 1;
        audio.sendRejected = 0;
        nowMs = 1750;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 6);
        snapshot = lastSnapshot(samples);
        QVERIFY(snapshot.audio.active);
        QVERIFY(!snapshot.audio.sourceFramesPerSecond);
    }

    // R-R3-32/33: the 1 Hz sample carries the Core's host load to a peer
    // that negotiated it; CPU needs two samples within one session.
    void hostTelemetryRidesTheSampleAndRestartsPerSession()
    {
        SessionHarness h;
        HostFixture host;
        qint64 nowMs = 0;
        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; }, {},
            std::make_unique<HostTelemetrySampler>(host.directory.path()));
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);
        // 4 since remote-window parity Task 6 (the radio's PA readings and
        // link quality), 5 since Task 14 (the HL2 link); host telemetry came
        // with 2.
        QCOMPARE(h.server.buildCapabilities().stationTelemetryVersion, 6);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        QVERIFY(h.client.agreedMinor() >= kCoreHostTelemetrySessionProtocolMinor);

        nowMs = 1000;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        StationTelemetrySnapshot snapshot = lastSnapshot(samples);
        QVERIFY(!snapshot.host.systemCpuPercent);
        QVERIFY(!snapshot.host.processCpuPercent);
        QCOMPARE(snapshot.host.memoryTotalKiB, std::optional<qint64>(8000000));
        QCOMPARE(snapshot.host.memoryAvailableKiB, std::optional<qint64>(6500000));
        QCOMPARE(snapshot.host.processResidentKiB, std::optional<qint64>(51234));
        QCOMPARE(snapshot.host.hottestZoneCelsius, std::optional<double>(47.5));
        QCOMPARE(snapshot.host.hottestZoneName, QStringLiteral("soc-thermal"));

        host.cpu("cpu  300 0 300 1200 200 0 0 0 0 0\n", 60); // 1000 ticks, 400 busy
        nowMs = 2000;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 2);
        snapshot = lastSnapshot(samples);
        QCOMPARE(snapshot.host.systemCpuPercent, std::optional<double>(40.0));
        QCOMPARE(snapshot.host.processCpuPercent, std::optional<double>(5.0));

        // A new session does not report an interval that spans the gap.
        h.client.disconnectFromStation(QStringLiteral("host telemetry epoch"));
        QTRY_VERIFY(!controller.isCollecting());
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        host.cpu("cpu  400 0 300 1400 300 0 0 0 0 0\n", 90);
        nowMs = 3000;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 3);
        snapshot = lastSnapshot(samples);
        QVERIFY(!snapshot.host.systemCpuPercent);
        QCOMPARE(snapshot.host.memoryTotalKiB, std::optional<qint64>(8000000));
    }

    // R-R3-40: telemetry and the display load governor read one shared host
    // sampler. Both see the same reading, and neither read restarts the CPU
    // interval the other relies on.
    void sharedHostSamplerGivesTelemetryAndTheGovernorOneReading()
    {
        SessionHarness h;
        HostFixture host;
        qint64 nowMs = 0;
        const auto shared = std::make_shared<SharedHostSampler>(
            std::make_unique<HostTelemetrySampler>(host.directory.path()),
            [&] { return nowMs; });
        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; }, {}, {}, {},
            shared);
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());

        nowMs = 1000;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        QVERIFY(!lastSnapshot(samples).host.systemCpuPercent);

        host.cpu("cpu  300 0 300 1200 200 0 0 0 0 0\n", 60); // 1000 ticks, 400 busy
        nowMs = 2000;
        const StationHostTelemetry governorRead = shared->reading();
        QCOMPARE(governorRead.systemCpuPercent, std::optional<double>(40.0));
        nowMs = 2400;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 2);
        QCOMPARE(lastSnapshot(samples).host.systemCpuPercent, governorRead.systemCpuPercent);
        QCOMPARE(lastSnapshot(samples).host.processCpuPercent, governorRead.processCpuPercent);

        host.cpu("cpu  400 0 300 1400 300 0 0 0 0 0\n", 90); // 400 ticks, 100 busy
        nowMs = 2600; // The governor's next tick: the cached reading, no new interval.
        QCOMPARE(shared->reading().systemCpuPercent, std::optional<double>(40.0));
        nowMs = 3400;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 3);
        // The interval runs from the 2000 ms sample: the read at 2600 ms did
        // not restart it.
        QCOMPARE(lastSnapshot(samples).host.systemCpuPercent, std::optional<double>(25.0));
        nowMs = 3500;
        QCOMPARE(shared->reading().systemCpuPercent, std::optional<double>(25.0));
    }

    // R-R3-40: each 1 Hz sample carries the receivers' cached load to a
    // peer that negotiated it. The provider only reads; the controller
    // never samples the receivers itself.
    void receiverLoadRidesTheSample()
    {
        SessionHarness h;
        qint64 nowMs = 0;
        int reads = 0;
        std::optional<QVector<StationReceiverTelemetry>> receivers;
        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; }, {},
            std::make_unique<HostTelemetrySampler>(QString()),
            [&] { ++reads; return receivers; });
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        QCOMPARE(h.client.agreedMinor(), kReceiverLoadSessionProtocolMinor);

        // Nothing measures receivers: the section stays absent.
        nowMs = 1000;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        QVERIFY(!lastSnapshot(samples).receivers);
        QCOMPARE(reads, 1);

        StationReceiverTelemetry a;
        a.sliceId = 0;
        a.loadPercent = 55.0;
        a.inputDelayMs = 6;
        a.skippedInputMs = 0;
        StationReceiverTelemetry b;
        b.sliceId = 3;
        b.inputDelayMs = 510;
        b.skippedInputMs = 2000;
        receivers = QVector<StationReceiverTelemetry>{a, b};
        nowMs = 2000;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 2);
        const StationTelemetrySnapshot snapshot = lastSnapshot(samples);
        QVERIFY(snapshot.receivers);
        QCOMPARE(snapshot.receivers->size(), 2);
        QCOMPARE(snapshot.receivers->at(0).loadPercent, std::optional<double>(55.0));
        QCOMPARE(snapshot.receivers->at(1).sliceId, 3);
        QVERIFY(!snapshot.receivers->at(1).loadPercent);
        QCOMPARE(snapshot.receivers->at(1).inputDelayMs, 510LL);
        QCOMPARE(snapshot.receivers->at(1).skippedInputMs, 2000LL);
        QCOMPARE(reads, 2);
    }

    // The default provider reads RadioModel. A model with no receiver
    // snapshot yet reports a measured, empty list rather than nothing.
    void defaultReceiverLoadReadsTheRadioModel()
    {
        SessionHarness h;
        qint64 nowMs = 0;
        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; }, {},
            std::make_unique<HostTelemetrySampler>(QString()));
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        nowMs = 1000;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        const StationTelemetrySnapshot snapshot = lastSnapshot(samples);
        QVERIFY(snapshot.receivers);
        QVERIFY(snapshot.receivers->isEmpty());
    }

    // A snapshot becomes a wire entry: the load fraction as a percentage,
    // left absent when the receiver was idle (which is not proof of no load).
    void receiverLoadSnapshotBecomesAWireEntry()
    {
        ReceiverDspLoad busy;
        busy.load = 1.25;
        busy.inputDelayMs = 40;
        busy.droppedInputMs = 300;
        StationReceiverTelemetry entry = DaemonTelemetryController::receiverTelemetry(2, busy);
        QCOMPARE(entry.sliceId, 2);
        QCOMPARE(entry.loadPercent, std::optional<double>(125.0));
        QCOMPARE(entry.inputDelayMs, 40LL);
        QCOMPARE(entry.skippedInputMs, 300LL);

        ReceiverDspLoad idle;
        idle.idle = true;
        idle.inputDelayMs = 0;
        entry = DaemonTelemetryController::receiverTelemetry(1, idle);
        QVERIFY(!entry.loadPercent);
        QCOMPARE(entry.inputDelayMs, 0LL);

        ReceiverDspLoad zero; // measured and genuinely light
        entry = DaemonTelemetryController::receiverTelemetry(0, zero);
        QCOMPARE(entry.loadPercent, std::optional<double>(0.0));
    }

    // The embedded Core on macOS and Windows has no procfs: the host
    // section stays absent and sampling logs nothing.
    void disabledHostSamplerSendsNoHostSectionAndLogsNothing()
    {
        SessionHarness h;
        qint64 nowMs = 0;
        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; }, {},
            std::make_unique<HostTelemetrySampler>(QString()));
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        for (int i = 1; i <= 3; ++i) {
            nowMs = i * 1000;
            controller.sampleNow();
            QTRY_COMPARE(samples.count(), i);
            QVERIFY(lastSnapshot(samples).host.isEmpty());
        }
    }

    // R-R3-32 / R-R3-46 (remote-window parity Task 6): the Core's own PA
    // readings (RadioModel::paReadings()) and its connection's link counters
    // ride the sample at stationTelemetryVersion 4; a reading the radio has
    // not reported stays absent, and none rides while the radio is not
    // connected.
    void radioPaReadingsAndLinkQualityRideTheSample()
    {
        SessionHarness h;
        h.station.setBoardForTest(HPSDRHW::Saturn);
        h.station.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        qint64 nowMs = 0;
        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; });
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);

        NullRadioConnection connection;
        h.station.injectConnectionForTest(&connection);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());

        // Nothing reported yet: every reading absent, never 0.
        nowMs = 100;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        StationTelemetrySnapshot snapshot = lastSnapshot(samples);
        QVERIFY(snapshot.radio.connected);
        QVERIFY(!snapshot.radio.paVolts && !snapshot.radio.supplyVolts
                && !snapshot.radio.paCurrentAmps && !snapshot.radio.paTemperatureCelsius);
        QVERIFY(!snapshot.radio.packetLossPercent && !snapshot.radio.jitterMs);
        QCOMPARE(snapshot.radio.udpPacketsSeen, std::optional<qint64>(0));

        // The radio reports: user ADC0 490 is 12.5 V by Thetis's
        // convertToVolts ((490 / 4095) * 5 * 23 / 1.1), the supply AIN6 and
        // a PA current sample.
        connection.handleUserAdc0Raw(490);
        connection.handleSupplyRaw(2000);
        h.station.handlePaTelemetryForTest(0, 0, 0, 490, 1000, 2000);
        connection.feedLinkStatsForTest(10);
        nowMs = 1100;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 2);
        snapshot = lastSnapshot(samples);
        const RadioModel::PaReadings pa = h.station.paReadings();
        QVERIFY(pa.paVolts && pa.supplyVolts && pa.paCurrentAmps);
        QCOMPARE(snapshot.radio.paVolts, pa.paVolts);
        QVERIFY(qAbs(*snapshot.radio.paVolts - 12.5) < 0.05);
        QCOMPARE(snapshot.radio.supplyVolts, pa.supplyVolts);
        QCOMPARE(snapshot.radio.paCurrentAmps, pa.paCurrentAmps);
        QVERIFY(!snapshot.radio.paTemperatureCelsius);   // a G2 reports none
        QCOMPARE(snapshot.radio.udpPacketsSeen, std::optional<qint64>(10));
        QCOMPARE(snapshot.radio.packetLossPercent, std::optional<double>(100.0 / 11.0));
        QCOMPARE(snapshot.radio.jitterMs, std::optional<double>(0.0));
        QVERIFY(snapshot.radio.packetGapMs);
        // Parity Task 14: a G2 has no HL2 bandwidth monitor, so no HL2 link.
        QVERIFY(snapshot.radio.hasNoHl2Link());

        // The radio gone: nothing rides.
        h.station.injectConnectionForTest(nullptr);
        nowMs = 2100;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 3);
        snapshot = lastSnapshot(samples);
        QVERIFY(!snapshot.radio.connected);
        QVERIFY(snapshot.radio.hasNoRadioStatus());
    }

    void radioDiagnosticsKeepConnectionEpochAcrossClientSessions()
    {
        SessionHarness h;
        h.station.setBoardForTest(HPSDRHW::Saturn);
        QCOMPARE(h.station.boardCapabilities().adcCount, 2);
        qint64 nowMs = 0;
        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; });
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);
        NullRadioConnection connection;
        connection.markConnectedForDiagnostics(41024);
        connection.observeAdcsForDiagnostics(0x07, 0x07);
        h.station.injectConnectionForTest(&connection);
        QTest::qWait(15); // The Core connection predates the first client.
        const qint64 beforeSessionAge = *h.station.connectionAgeMs();
        QVERIFY(beforeSessionAge > 0);

        QSignalSpy replies(&connection, &RadioConnection::telemetryObservationReady);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        QTRY_VERIFY(replies.count() >= 1);
        QCoreApplication::sendPostedEvents(&controller, QEvent::MetaCall);
        nowMs = 100;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        StationTelemetrySnapshot first = lastSnapshot(samples);
        QVERIFY(first.radio.connected);
        QVERIFY(first.radio.connectionAgeMs);
        QVERIFY(*first.radio.connectionAgeMs >= beforeSessionAge);
        QCOMPARE(first.radio.radioUdpBasePort, std::optional<qint64>(41024));
        QVERIFY(first.radio.adcOverloads);
        QCOMPARE(first.radio.adcOverloads->size(), 2); // board has no ADC2
        QCOMPARE(first.radio.adcOverloads->at(0).eventsSinceConnection, 1);
        QCOMPARE(first.radio.adcOverloads->at(0).overloaded, std::optional<bool>(true));

        h.client.disconnectFromStation(QStringLiteral("new diagnostics session"));
        QTRY_VERIFY(!controller.isCollecting());
        const int repliesBeforeReconnect = replies.count();
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        QTRY_VERIFY(replies.count() > repliesBeforeReconnect);
        QCoreApplication::sendPostedEvents(&controller, QEvent::MetaCall);
        QTest::qWait(5);
        nowMs = 100; // New session clock starts again; connection age does not.
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 2);
        const StationTelemetrySnapshot second = lastSnapshot(samples);
        QCOMPARE(second.sequence, quint32{1});
        QVERIFY(second.radio.connectionAgeMs);
        QVERIFY(*second.radio.connectionAgeMs > *first.radio.connectionAgeMs);
        QVERIFY(second.radio.adcOverloads);
        QCOMPARE(second.radio.adcOverloads->at(0).eventsSinceConnection, 1);

        h.station.setConnectionStateForTest(ConnectionState::LinkLost);
        QVERIFY(!h.station.connectionAgeMs());
        h.station.injectConnectionForTest(nullptr);
        QVERIFY(!h.station.connectionAgeMs());
        nowMs = 200;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 3);
        QVERIFY(lastSnapshot(samples).radio.hasNoRadioDiagnostics());

        QElapsedTimer freshEpochEnclosure;
        freshEpochEnclosure.start();
        connection.retireForDiagnostics();
        connection.markConnectedForDiagnostics(41024); // same MAC, same QObject
        h.station.injectConnectionForTest(&connection);
        const int repliesBeforeRadioReconnect = replies.count();
        QTRY_VERIFY(replies.count() > repliesBeforeRadioReconnect);
        QCoreApplication::sendPostedEvents(&controller, QEvent::MetaCall);
        nowMs = 300;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 4);
        const StationTelemetrySnapshot freshRadio = lastSnapshot(samples);
        QVERIFY(freshRadio.radio.connectionAgeMs);
        QVERIFY(*freshRadio.radio.connectionAgeMs <= freshEpochEnclosure.elapsed());
        QCOMPARE(freshRadio.radio.radioUdpBasePort, std::optional<qint64>(41024));
        QVERIFY(!freshRadio.radio.adcOverloads); // old positive count retired
        h.station.injectConnectionForTest(nullptr);
    }

    void secondClientSharesRadioAgeAndOverloadEpoch()
    {
        SessionHarness h;
        h.station.setBoardForTest(HPSDRHW::Saturn);
        h.server.setTelemetryEnabled(true);
        NullRadioConnection connection;
        connection.markConnectedForDiagnostics(41024);
        connection.observeAdcsForDiagnostics(0x01, 0x01);
        h.station.injectConnectionForTest(&connection);
        QSignalSpy replies(&connection, &RadioConnection::telemetryObservationReady);
        QSignalSpy started(&h.server, &StationServer::telemetrySessionStarted);
        DaemonTelemetryController first(&h.server, &h.station, nullptr);
        first.disableAutomaticSamplingForTest();
        QSignalSpy firstSamples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        QTRY_VERIFY(replies.count() >= 1);
        QCoreApplication::sendPostedEvents(&first, QEvent::MetaCall);
        first.sampleNow();
        QTRY_COMPARE(firstSamples.count(), 1);
        const StationTelemetrySnapshot firstReading = lastSnapshot(firstSamples);
        QVERIFY(firstReading.radio.adcOverloads);

        RadioModel secondRemote(RadioModel::Role::Remote);
        SettingsProxy secondProxy;
        StationClient secondClient(&secondRemote, &secondProxy);
        QSignalSpy secondSamples(&secondClient, &StationClient::telemetryReceived);
        auto* stationLink = new LoopbackTransport(QStringLiteral("second-telemetry-station"), this);
        auto* clientLink = new LoopbackTransport(QStringLiteral("second-telemetry-client"), this);
        stationLink->linkTo(clientLink);
        const int startedBeforeSecond = started.count();
        secondClient.startSession(clientLink, h.server.token());
        h.server.acceptTransport(stationLink);
        QTRY_VERIFY(secondClient.telemetryAvailable());
        QTRY_VERIFY(started.count() > startedBeforeSecond);
        const quint64 secondEpoch = started.constLast().at(0).toULongLong();
        DaemonTelemetryController second(&h.server, &h.station, nullptr);
        second.disableAutomaticSamplingForTest();
        second.bindToSession(secondEpoch);
        const int repliesBeforeSecond = replies.count();
        QTRY_VERIFY(replies.count() > repliesBeforeSecond);
        QCoreApplication::sendPostedEvents(&second, QEvent::MetaCall);
        second.sampleNow();
        QTRY_COMPARE(secondSamples.count(), 1);
        const StationTelemetrySnapshot secondReading = lastSnapshot(secondSamples);
        QCOMPARE(secondReading.sequence, quint32{1});
        QCOMPARE(secondReading.radio.radioUdpBasePort, std::optional<qint64>(41024));
        QVERIFY(secondReading.radio.connectionAgeMs);
        QVERIFY(firstReading.radio.connectionAgeMs);
        QVERIFY(*secondReading.radio.connectionAgeMs >= *firstReading.radio.connectionAgeMs);
        QVERIFY(secondReading.radio.adcOverloads);
        QCOMPARE(secondReading.radio.adcOverloads->at(0).eventsSinceConnection, 1);
        QCOMPARE(secondReading.radio.adcOverloads->at(0).overloaded,
                 std::optional<bool>(true));
        QVERIFY(h.client.telemetryAvailable()); // second admission did not retire first
        h.station.injectConnectionForTest(nullptr);
    }

    void destroyedConnectionRetiresModelAgeBeforeStateNotification()
    {
        SessionHarness h;
        auto* connection = new NullRadioConnection;
        connection->markConnectedForDiagnostics(41024);
        h.station.injectConnectionForTest(connection);
        QVERIFY(h.station.connectionAgeMs());

        delete connection;
        QVERIFY(!h.station.connectionAgeMs());
        h.station.injectConnectionForTest(nullptr);
    }

    void silentAdcStatusBecomesUnknownWithFreshConnectionReplies()
    {
        SessionHarness h;
        h.station.setBoardForTest(HPSDRHW::Saturn);
        qint64 nowMs = 0;
        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; });
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);
        NullRadioConnection connection;
        connection.markConnectedForDiagnostics(41024);
        connection.observeAdcsForDiagnostics(0x01, 0x01);
        h.station.injectConnectionForTest(&connection);
        QSignalSpy replies(&connection, &RadioConnection::telemetryObservationReady);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        QTRY_VERIFY(replies.count() >= 1);
        QCoreApplication::sendPostedEvents(&controller, QEvent::MetaCall);
        nowMs = 100;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        QCOMPARE(lastSnapshot(samples).radio.adcOverloads->at(0).overloaded,
                 std::optional<bool>(true));

        // No new status arrives, but the connection still answers telemetry
        // requests. A new reply must not renew the old overload bit.
        QTest::qWait(3100);
        const int beforeFreshReply = replies.count();
        nowMs = 3200;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 2);
        QTRY_VERIFY(replies.count() > beforeFreshReply);
        QCoreApplication::sendPostedEvents(&controller, QEvent::MetaCall);
        nowMs = 3300;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 3);
        const StationRadioTelemetry& radio = lastSnapshot(samples).radio;
        QCOMPARE(radio.radioUdpBasePort, std::optional<qint64>(41024));
        QVERIFY(radio.adcOverloads);
        QCOMPARE(radio.adcOverloads->at(0).eventsSinceConnection, 1);
        QVERIFY(!radio.adcOverloads->at(0).overloaded);
        QVERIFY(*radio.adcOverloads->at(0).statusAgeMs > 3000);
        h.station.injectConnectionForTest(nullptr);
    }

    // R-R3-32 (remote-window parity Task 14): an HL2 Core's bandwidth
    // monitor (the one its HL2 I/O tab, Radio Status and Connection Quality
    // read) rides the sample at stationTelemetryVersion 5, and none rides
    // while the radio is not connected.
    void hl2LinkRidesTheSampleOnAnHl2()
    {
        SessionHarness h;
        h.station.setBoardForTest(HPSDRHW::HermesLite);
        h.station.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        QVERIFY(h.station.boardCapabilities().hasBandwidthMonitor);
        qint64 nowMs = 0;
        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; });
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);
        NullRadioConnection connection;
        h.station.injectConnectionForTest(&connection);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());

        HermesLiteBandwidthMonitor& bw = h.station.bwMonitorMutable();
        bw.recordEp6SequenceError();
        bw.recordEp6SequenceError();
        bw.recordEp6Bytes(1032);
        bw.recordEp2Bytes(1032);
        bw.tick();
        nowMs = 100;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        StationTelemetrySnapshot snapshot = lastSnapshot(samples);
        QCOMPARE(snapshot.radio.hl2SequenceGaps, std::optional<qint64>(2));
        QCOMPARE(snapshot.radio.hl2Throttled, std::optional<bool>(bw.isThrottled()));
        QCOMPARE(snapshot.radio.hl2RxBytesPerSecond,
                 std::optional<double>(bw.ep6IngressBytesPerSec()));
        QCOMPARE(snapshot.radio.hl2TxBytesPerSecond,
                 std::optional<double>(bw.ep2EgressBytesPerSec()));

        h.station.injectConnectionForTest(nullptr);
        nowMs = 1100;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 2);
        QVERIFY(lastSnapshot(samples).radio.hasNoHl2Link());
    }

    void queuedRadioReadsRejectAReplyFromTheReplacedConnection()
    {
        SessionHarness h;
        h.station.setBoardForTest(HPSDRHW::Saturn);
        qint64 nowMs = 0;
        DaemonTelemetryController controller(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; });
        controller.disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);

        ThreadConnection oldOwner(h.station);
        NullRadioConnection* const oldConnection = oldOwner.connection.data();
        oldConnection->markConnectedForDiagnostics(40001);
        oldConnection->observeAdcsForDiagnostics(0x01, 0x01);
        oldConnection->recordBytesReceived(125000); // 1 Mbps over 1 s.
        oldConnection->recordBytesSent(125000);
        oldOwner.moveToOwnerThread(false); // Leave stopped to hold the reply.
        QSemaphore oldReplyEmitted;
        const QMetaObject::Connection oldReplyGate = connect(
            oldConnection, &RadioConnection::telemetryObservationReady,
            oldConnection, [&] { oldReplyEmitted.release(); },
            Qt::DirectConnection);
        h.station.injectConnectionForTest(oldConnection);

        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());

        ThreadConnection newOwner(h.station);
        NullRadioConnection* const newConnection = newOwner.connection.data();
        newOwner.moveToOwnerThread(true);
        QVERIFY(QMetaObject::invokeMethod(
            newConnection, [newConnection] {
                newConnection->markConnectedForDiagnostics(40002);
                newConnection->observeAdcsForDiagnostics(0x01, 0x00);
                newConnection->recordBytesReceived(250000); // 2 Mbps over 1 s.
                newConnection->recordBytesSent(375000);     // 3 Mbps over 1 s.
                newConnection->notePingSent();
            }, Qt::BlockingQueuedConnection));
        QTest::qWait(5);
        QVERIFY(QMetaObject::invokeMethod(
            newConnection, [newConnection] {
                newConnection->notePingReceived();
            }, Qt::BlockingQueuedConnection));
        QSignalSpy newReplies(newConnection,
                              &RadioConnection::telemetryObservationReady);

        // Let the old owner-thread read complete, but deliberately do not run
        // this thread's event loop yet. Its reply is now queued to the
        // controller and will arrive only after replacement.
        oldOwner.thread.start();
        QVERIFY(oldReplyEmitted.tryAcquire(1, 1000));
        QObject::disconnect(oldReplyGate);

        // Same Connected state means no model signal. sampleNow() must still
        // detect the pointer replacement, retire the old request and queue a
        // request to the new object's owning thread. The already-queued old
        // reply is rejected when this thread next processes events.
        h.station.injectConnectionForTest(newConnection);
        nowMs = 100;
        controller.sampleNow();
        QTRY_VERIFY(newReplies.count() >= 1);
        // The spy can observe the owner-thread emission before the queued
        // collector slot runs. Wait for the emit to finish, then deliver the
        // collector's queued observation before advancing the sample clock.
        QVERIFY(QMetaObject::invokeMethod(newConnection, [] {}, Qt::BlockingQueuedConnection));
        QCoreApplication::sendPostedEvents(&controller, QEvent::MetaCall);
        nowMs = 1000;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 2);
        const StationTelemetrySnapshot snapshot = lastSnapshot(samples);
        QVERIFY(snapshot.radio.connected);
        QVERIFY(snapshot.radio.rxMbps);
        QVERIFY(snapshot.radio.txMbps);
        QCOMPARE(*snapshot.radio.rxMbps, 2.0);
        QCOMPARE(*snapshot.radio.txMbps, 3.0);
        QVERIFY(snapshot.radio.rttMs);
        QVERIFY(snapshot.radio.rttAgeMs);
        QVERIFY(*snapshot.radio.rttAgeMs >= *snapshot.radio.rttMs);
        QCOMPARE(snapshot.radio.radioUdpBasePort, std::optional<qint64>(40002));
        QVERIFY(snapshot.radio.adcOverloads);
        QCOMPARE(snapshot.radio.adcOverloads->at(0).eventsSinceConnection, 0);
        QCOMPARE(snapshot.radio.adcOverloads->at(0).overloaded,
                 std::optional<bool>(false));

        // Stop owner-thread replies and advance beyond three periods. The
        // last radio value becomes unavailable rather than being repeated as
        // a fresh sample indefinitely.
        newOwner.thread.quit();
        QVERIFY(newOwner.thread.wait(1000));
        nowMs = 5001;
        controller.sampleNow();
        QTRY_COMPARE(samples.count(), 3);
        const StationTelemetrySnapshot stale = lastSnapshot(samples);
        QVERIFY(stale.radio.connected);
        QVERIFY(!stale.radio.rxMbps);
        QVERIFY(!stale.radio.txMbps);
        QVERIFY(!stale.radio.rttMs);
        QVERIFY(!stale.radio.rttAgeMs);
        QVERIFY(!stale.radio.radioUdpBasePort);
        QVERIFY(stale.radio.adcOverloads);
        QCOMPARE(stale.radio.adcOverloads->at(0).eventsSinceConnection, 0);
        QVERIFY(!stale.radio.adcOverloads->at(0).overloaded);
        QVERIFY(*stale.radio.adcOverloads->at(0).statusAgeMs > 3000);
        newOwner.thread.start();
    }

    void authenticatedLifecycleResetsSequenceAndStopsOldEpochPublication()
    {
        SessionHarness h;
        qint64 nowMs = 50;
        auto controller = std::make_unique<DaemonTelemetryController>(
            &h.server, &h.station, nullptr, nullptr, [&] { return nowMs; });
        controller->disableAutomaticSamplingForTest();
        h.server.setTelemetryEnabled(true);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);

        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        QVERIFY(controller->isCollecting());
        const quint64 oldServerEpoch = h.server.sessionEpoch();
        const quint32 oldClientEpoch = h.client.sessionEpoch();
        nowMs = 150;
        controller->sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        QCOMPARE(lastSnapshot(samples).sequence, quint32{1});

        h.client.disconnectFromStation(QStringLiteral("end telemetry epoch"));
        QTRY_VERIFY(!h.server.telemetryAvailable());
        QTRY_VERIFY(!controller->isCollecting());
        nowMs = 1150;
        controller->sampleNow();
        QCOMPARE(samples.count(), 1);
        StationTelemetrySnapshot stale;
        stale.sequence = 2;
        stale.sampledElapsedMs = 1000;
        QVERIFY(!h.server.sendTelemetry(stale, oldServerEpoch));

        h.connectClient(this);
        QTRY_VERIFY(h.client.telemetryAvailable());
        QVERIFY(controller->isCollecting());
        QVERIFY(h.server.sessionEpoch() != oldServerEpoch);
        QVERIFY(h.client.sessionEpoch() != oldClientEpoch);
        nowMs = 1250;
        controller->sampleNow();
        QTRY_COMPARE(samples.count(), 2);
        const StationTelemetrySnapshot fresh = lastSnapshot(samples);
        QCOMPARE(fresh.sequence, quint32{1});
        QCOMPARE(fresh.sampledElapsedMs, qint64{100});

        // DaemonApp uses this exact ownership order: the collector is gone
        // before server/media/model teardown, leaving no live timer callback.
        QPointer<DaemonTelemetryController> guard(controller.get());
        controller.reset();
        QVERIFY(guard.isNull());
    }
};

QTEST_MAIN(TstDaemonTelemetry)
#include "tst_daemon_telemetry.moc"

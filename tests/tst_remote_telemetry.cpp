// no-port-check: NereusSDR-original. Remote telemetry lifecycle/presentation.
#include <QTest>
#include <algorithm>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <QJsonObject>
#include <QSignalSpy>
#include "core/AppSettings.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/settings/SettingsProxy.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteMediaController.h"
#include "gui/RemoteTelemetryController.h"
#include "gui/SpectrumWidget.h"
#include "gui/setup/hardware/Hl2IoBoardTab.h"
#include "gui/diagnostics/DiagnosticsPhaseHPages.h"
#include "gui/diagnostics/RadioStatusPage.h"
#include <QGroupBox>
#include <QLabel>
#include "models/RadioModel.h"
#include "OperatorWording.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/RemoteAudioSessionHarness.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using Metric = TelemetryHistory::Metric;

namespace {
// What the controller logs when a playing speaker stops reporting timing.
// Mirrors tst_remote_media_controller.cpp's speakerTimingLostLog(): the
// worker's pacing check or its next write notices first.
QRegularExpression remoteAudioSpeakerTimingLostLog()
{
    return QRegularExpression(QStringLiteral(
        "^Remote audio playback failed: (Speaker device timing became unavailable"
        "|Could not write remote audio to the speaker device) \\[ageMs="));
}
// Captures the periodic diagnostics lines (R-R3-07/33) and passes every
// message on to the handler that was installed before it.
QStringList g_diagnosticsLines;
QtMessageHandler g_previousHandler = nullptr;
void captureDiagnostics(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtInfoMsg && context.category
        && QLatin1String(context.category) == QLatin1String("nereus.remote.telemetry")) {
        g_diagnosticsLines << message;
    }
    if (g_previousHandler) { g_previousHandler(type, context, message); }
}
struct DiagnosticsCapture {
    DiagnosticsCapture() { g_diagnosticsLines.clear(); g_previousHandler = qInstallMessageHandler(captureDiagnostics); }
    ~DiagnosticsCapture() { qInstallMessageHandler(g_previousHandler); g_previousHandler = nullptr; }
};
} // namespace

class ObservedLoopback final : public Test::LoopbackTransport {
public:
    ObservedLoopback() : LoopbackTransport(QStringLiteral("GUI")) {}
    SessionTransportTelemetry observation;
    std::optional<SessionTransportTelemetry> telemetry() const override
    { return isOpen() ? std::optional{observation} : std::nullopt; }
};

class TestRemoteTelemetry : public QObject {
    Q_OBJECT
private slots:
    void authenticSessionSeparatesSourcesAgesAndHistory()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setTelemetryEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        qint64 now = 10000;
        RemoteAudioReceiverTelemetry playback;
        RemoteTelemetryController controller(&client, nullptr, nullptr,
            [&] { return now; }, [&] { return playback; });
        QCOMPARE(controller.current().state, RemoteTelemetryView::State::Disconnected);
        QVERIFY(controller.bannerText().isEmpty());

        auto* guiWire = new ObservedLoopback;
        auto* coreWire = new Test::LoopbackTransport(QStringLiteral("Core"));
        guiWire->linkTo(coreWire);
        server.acceptTransport(coreWire);
        client.startSession(guiWire, server.token());
        QTRY_VERIFY(client.isHandshakeComplete());
        QCOMPARE(controller.current().state, RemoteTelemetryView::State::Waiting);
        QVERIFY(controller.bannerText().contains(QStringLiteral("waiting for measurements")));
        QVERIFY2(OperatorWording::isPlain(controller.bannerText()), qPrintable(controller.bannerText()));

        guiWire->observation.pongRttMs = 83;
        guiWire->observation.pongAgeMs = 20000; // older than station freshness, still valid RTT
        playback.running = true;
        playback.generation = 4;
        playback.lifetimeUnderflows = 0;
        playback.lifetimeOverflows = 0;
        controller.sampleNow(); // baseline: no invented zero rate
        QVERIFY(!controller.current().controlRxKbps);

        StationTelemetrySnapshot sample;
        sample.sequence = 1;
        sample.sampledElapsedMs = 200;
        sample.radio.connected = true;
        sample.radio.rxMbps = 12.5;
        sample.radio.txMbps = 0.1;
        sample.radio.rttMs = 7;
        sample.radio.rttAgeMs = 10;
        sample.audio.active = true;
        sample.audio.contextGeneration = 2;
        sample.audio.encodedPacketsPerSecond = 25;
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_COMPARE(controller.current().state, RemoteTelemetryView::State::Current);
        QCOMPARE(controller.current().radio.rxMbps, std::optional<double>(12.5));
        QCOMPARE(controller.current().radio.rttMs, std::optional<qint64>(7));
        QCOMPARE(controller.current().coreRttMs, std::optional<quint64>(83));
        QVERIFY(!controller.current().playbackActive); // running alone is not playback
        QVERIFY(controller.bannerText().contains(QStringLiteral("Radio ↓12.5 ↑0.1 Mbps")));
        QVERIFY(controller.bannerText().contains(QStringLiteral("Core RTT 83 ms")));
        // R-R3-23 Task 4: unmeasured wording before any packet/health values.
        QVERIFY(controller.detailText().contains(QStringLiteral("Arrival jitter: not measured.")));
        QVERIFY(controller.detailText().contains(QStringLiteral("Missing packets: none received.")));
        QVERIFY(controller.detailText().contains(QStringLiteral("Gaps filled: 0, concealed 40\u00A0ms intervals.")));
        QVERIFY(controller.detailText().contains(QStringLiteral("Speaker buffer: not measured.")));
        QVERIFY(controller.detailText().contains(QStringLiteral("Reorder buffer: not measured.")));
        // R-R3-07: drift is absent until the receiver measures it.
        QVERIFY(controller.detailText().contains(QStringLiteral("Clock drift: not measured.")));

        now += 1000;
        guiWire->observation.receivedPayloadBytes += 2000;
        guiWire->observation.acceptedPayloadBytes += 4000;
        playback.decodedPackets = 25;
        playback.acceptedPackets = 27;
        playback.startDiscardedPackets = 62;
        playback.deviceConsumedFrames = 48000;
        playback.lastAdmittedPacketAgeMs = 20;
        playback.lastDeviceProgressAgeMs = 5;
        playback.arrivalJitterMs = 3.7;
        playback.missingPackets = 2;
        playback.expectedPackets = 100;
        playback.speakerQueuedMs = 41.2;
        playback.reorderQueuedMs = 80.0;
        playback.driftRatio = 1.0000234;
        controller.sampleNow();
        QCOMPARE(controller.current().controlRxKbps, std::optional<double>(16.0));
        QCOMPARE(controller.current().controlTxKbps, std::optional<double>(32.0));
        QVERIFY(controller.current().playbackActive);
        QCOMPARE(controller.history().rawObservationCount(Metric::RadioRxMbps), 1);
        QCOMPARE(controller.history().series(Metric::PlaybackDecodedPacketsPerSecond, now, 60).points.last().value, 25.0);
        QVERIFY(controller.detailText().contains(QStringLiteral("not counting audio, display or network overhead")));
        QVERIFY(controller.detailText().contains(QStringLiteral("measured 20000 ms ago")));
        // R-R3-23 Task 4: measured values, rounded, labelled with what they
        // are, no RTP/generation words.
        // U+00A0 keeps each number on the same line as its unit.
        QVERIFY(controller.detailText().contains(QStringLiteral("Arrival jitter: 4\u00A0ms, measured on this computer.")));
        QVERIFY(controller.detailText().contains(QStringLiteral("Missing packets: 2 of 100, sequence numbers never received.")));
        QVERIFY(controller.detailText().contains(QStringLiteral("Gaps filled: 0, concealed 40\u00A0ms intervals.")));
        QVERIFY(controller.detailText().contains(QStringLiteral("Speaker buffer: 41\u00A0ms, audio queued for this computer's speaker, not total delay.")));
        QVERIFY(controller.detailText().contains(QStringLiteral("Reorder buffer: 80\u00A0ms on this computer, packets held so that late arrivals play in order.")));
        // R-R3-07: the ratio near 1.0 is shown as parts per million, with
        // the number and unit on one line.
        QVERIFY2(controller.detailText().contains(QStringLiteral(
                     "Clock drift: 23\u00A0parts per million, the rate correction this computer applies to match the Core's audio clock.")),
                 qPrintable(controller.detailText()));
        QCOMPARE(controller.current().playback.driftRatio, std::optional<double>(1.0000234));
        // Fix wave M1: the connect-time backlog discard is shown, and it is
        // a separate count from "admitted" (the receiver excludes it).
        QVERIFY2(controller.detailText().contains(QStringLiteral(
                     "accepted 27, discarded before playback 62, decoded 25,")),
                 qPrintable(controller.detailText()));
        // R-R3-21: every line of the explanation is in user words, and
        // there are lines to check (fix wave M1).
        QVERIFY2(controller.detailText().split(QLatin1Char('\n')).size() >= 10,
                 qPrintable(controller.detailText()));
        for (const QString& line : controller.detailText().split(QLatin1Char('\n'))) {
            QVERIFY2(OperatorWording::isPlain(line), qPrintable(line));
        }
        QVERIFY2(OperatorWording::isPlain(controller.bannerText()), qPrintable(controller.bannerText()));

        // The previous context failed and restarted entirely between polls.
        // Its per-context counters have reset; the actual interruption remains.
        now += 1000;
        playback.generation = 5;
        playback.decodedPackets = 0;
        playback.underflows = 0;
        playback.lifetimeUnderflows = 1;
        controller.sampleNow();
        QCOMPARE(controller.history().series(Metric::PlaybackUnderflowsPerSecond, now, 60).points.last().value, 1.0);
        QVERIFY(controller.history().series(Metric::PlaybackUnderflowsPerSecond, now, 60).points.last().breakBefore);
        // A bounded unavailable lifecycle read cannot replace that baseline
        // with zero or invent a second event on the next stable observation.
        now += 100;
        playback.lifetimeUnderflows.reset(); playback.lifetimeOverflows.reset();
        controller.sampleNow();
        now += 900;
        playback.lifetimeUnderflows = 1; playback.lifetimeOverflows = 0;
        controller.sampleNow();
        QCOMPARE(controller.history().series(Metric::PlaybackUnderflowsPerSecond, now, 60).points.last().value, 0.0);

        // A timer tick must not append the old Core measurement again.
        now += 201;
        controller.sampleNow();
        QCOMPARE(controller.current().state, RemoteTelemetryView::State::Stale);
        QVERIFY(!controller.current().radio.rxMbps);
        QCOMPARE(controller.history().rawObservationCount(Metric::RadioRxMbps), 1);
        QVERIFY(controller.bannerText().contains(QStringLiteral("measurements out of date")));
        QVERIFY2(OperatorWording::isPlain(controller.bannerText()), qPrintable(controller.bannerText()));
        QCOMPARE(controller.current().coreRttMs, std::optional<quint64>(83));
        guiWire->observation.pongAgeMs = 60001;
        controller.sampleNow();
        QVERIFY(!controller.current().coreRttMs);

        // R-R3-07: the drift line follows each new measurement, below 1.0
        // included, and returns to unmeasured when the receiver drops it.
        now += 1000;
        playback.driftRatio = 0.99998;
        controller.sampleNow();
        QVERIFY2(controller.detailText().contains(QStringLiteral("Clock drift: -20\u00A0parts per million,")),
                 qPrintable(controller.detailText()));
        now += 1000;
        playback.driftRatio.reset();
        controller.sampleNow();
        QVERIFY(controller.detailText().contains(QStringLiteral("Clock drift: not measured.")));

        client.disconnectFromStation(QStringLiteral("operator disconnect"));
        QCOMPARE(controller.current().state, RemoteTelemetryView::State::Disconnected);
        QVERIFY(!controller.current().controlRxKbps);
        QVERIFY(!controller.current().playbackActive);
        QCoreApplication::processEvents();
        ++now; // short reconnect must still break history
        auto* nextGui = new ObservedLoopback;
        auto* nextCore = new Test::LoopbackTransport(QStringLiteral("replacement"));
        nextGui->linkTo(nextCore);
        server.acceptTransport(nextCore);
        client.startSession(nextGui, server.token());
        QTRY_VERIFY(client.isHandshakeComplete());
        sample.sequence = 1;
        sample.sampledElapsedMs = 0;
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_COMPARE(controller.current().state, RemoteTelemetryView::State::Current);
        const auto series = controller.history().series(Metric::RadioRxMbps, now, 60);
        QCOMPARE(series.points.size(), 2);
        QVERIFY(series.points.last().breakBefore);
        QVERIFY(!controller.current().controlRxKbps); // new transport baseline
        client.disconnectFromStation(QStringLiteral("done"));
    }

    // R-R3-23 Task 4: with a real media controller, bannerText()'s audio
    // word is the GUI's own persistent remote audio status, not the
    // running/decoding heuristic. Mute and a real speaker fault (PacedAudioBus
    // withdrawing its device timing, never a receiver signal emitted
    // directly) both drive it through a real session.
    void bannerWordTracksMuteAndARealSpeakerFaultThroughMediaController()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        RemoteTelemetryController controller(&h.client, &remoteMedia);

        QTimer sourceTimer;
        sourceTimer.setInterval(10);
        sourceTimer.setTimerType(Qt::PreciseTimer);
        connect(&sourceTimer, &QTimer::timeout, &sourceTimer, [&h] { h.feedMixedTone(); });
        QTimer speakerTimer;
        speakerTimer.setInterval(10);
        speakerTimer.setTimerType(Qt::PreciseTimer);
        connect(&speakerTimer, &QTimer::timeout, &speakerTimer, [&h] {
            h.remoteBus->render(Test::RemoteAudioSessionHarness::kFrames);
        });
        sourceTimer.start();
        speakerTimer.start();

        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);
        controller.sampleNow();
        QVERIFY(controller.bannerText().contains(QStringLiteral("Audio playing")));

        h.remote.audioEngine()->setMasterMuted(true);
        QCOMPARE(remoteMedia.audioStatus().state, State::MutedHere);
        QVERIFY(controller.bannerText().contains(QStringLiteral("Audio muted")));

        h.remote.audioEngine()->setMasterMuted(false);
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state, State::Playing, 15000);

        QTest::ignoreMessage(QtWarningMsg, remoteAudioSpeakerTimingLostLog());
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state, State::PlaybackProblem, 5000);
        QVERIFY(controller.bannerText().contains(QStringLiteral("Audio unavailable")));

        sourceTimer.stop();
        speakerTimer.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void trafficSeparatesOpusAndResetsEachLifetime()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        qint64 now = 10000;
        RemoteAudioReceiverTelemetry playback;
        playback.running = true;
        playback.generation = 7;
        playback.speakerQueuedMs = 25.0;
        std::optional<MediaPeerTelemetry> media{MediaPeerTelemetry{3, {}}};
        RemoteTelemetryController controller(&client, nullptr, nullptr,
            [&] { return now; }, [&] { return playback; }, [&] { return media; });
        auto* gui = new ObservedLoopback;
        auto* core = new Test::LoopbackTransport(QStringLiteral("Core"));
        gui->linkTo(core);
        server.acceptTransport(core);
        client.startSession(gui, server.token());
        QTRY_VERIFY(client.isHandshakeComplete());
        QVERIFY(!controller.current().coreGuiTotalKbps);
        QVERIFY(!controller.current().audioPayloadRxKbps);

        now += 2000; // actual elapsed time, not a presumed one-second tick
        gui->observation.receivedPayloadBytes = 1000;
        gui->observation.acceptedPayloadBytes = 500;
        media->traffic.receivedDisplayPayloadBytes = 100000;
        media->traffic.receivedRtpBytes = 10000;
        media->traffic.submittedDisplayPayloadBytes = 200;
        media->traffic.submittedRtpBytes = 300;
        media->traffic.receivedTxPayloadBytes = 1000;
        media->traffic.receivedIqPayloadBytes = 3000;
        media->traffic.submittedTxPayloadBytes = 2000;
        media->traffic.submittedIqPayloadBytes = 4000;
        playback.receivedAudioPayloadBytes = 9000; // subset, never add twice
        controller.sampleNow();
        QCOMPARE(controller.current().coreGuiRxKbps, std::optional<double>(460.0));
        QCOMPARE(controller.current().coreGuiTxKbps, std::optional<double>(28.0));
        QCOMPARE(controller.current().coreGuiTotalKbps, std::optional<double>(488.0));
        QCOMPARE(controller.current().audioPayloadRxKbps, std::optional<double>(36.0));
        QCOMPARE(controller.current().audioRtpRxKbps, std::optional<double>(40.0));
        QVERIFY(controller.bannerText().contains(QStringLiteral("Core ↓460.0 ↑28.0 total 488.0 kbps")));
        QCOMPARE(controller.history().series(Metric::SpeakerBufferMs, now, 60).points.last().value, 25.0);
        QVERIFY(controller.detailText().contains(QStringLiteral("End-to-end audio latency is not measured")));
        QVERIFY(controller.detailText().contains(QStringLiteral("already included in total")));
        QVERIFY(controller.detailText().contains(QStringLiteral("media transmit keepalive, raw I/Q, and separate transmit watch")));
        QVERIFY(controller.detailText().contains(QStringLiteral("separate transmit watch messages")));

        now += 1000;
        media->traffic.receivedDisplayPayloadBytes += 200000;
        controller.sampleNow();
        QVERIFY(controller.bannerText().contains(QStringLiteral("Core ↓1.6 ↑0.0 total 1.6 Mbps")));

        now += 1000;
        controller.sampleNow();
        QCOMPARE(controller.current().coreGuiTotalKbps, std::optional<double>(0.0));
        QCOMPARE(controller.current().audioPayloadRxKbps, std::optional<double>(0.0));

        // An audio restart gaps Opus but does not discard media totals.
        now += 1000;
        ++playback.generation;
        playback.receivedAudioPayloadBytes = 0;
        playback.speakerQueuedMs.reset();
        media->traffic.receivedRtpBytes += 500;
        controller.sampleNow();
        QCOMPARE(controller.current().coreGuiTotalKbps, std::optional<double>(4.0));
        QVERIFY(!controller.current().audioPayloadRxKbps);
        now += 1000;
        playback.receivedAudioPayloadBytes = 500;
        controller.sampleNow();
        QCOMPARE(controller.current().audioPayloadRxKbps, std::optional<double>(4.0));
        QVERIFY(controller.history().series(Metric::AudioPayloadRxKbps, now, 60).points.last().breakBefore);

        // A peer replacement gaps total and Opus, with independent baselines.
        now += 1000;
        media = MediaPeerTelemetry{4, {}};
        playback.receivedAudioPayloadBytes += 500;
        controller.sampleNow();
        QVERIFY(!controller.current().coreGuiTotalKbps);
        QVERIFY(!controller.current().audioPayloadRxKbps);
        now += 1000;
        controller.sampleNow();
        QCOMPARE(controller.current().coreGuiTotalKbps, std::optional<double>(0.0));
        QVERIFY(controller.history().series(Metric::CoreGuiTotalKbps, now, 60).points.last().breakBefore);

        now += 1000;
        media.reset();
        controller.sampleNow();
        QVERIFY(!controller.current().coreGuiTotalKbps); // unknown is not control-only zero
        QVERIFY(!controller.current().audioPayloadRxKbps);
        now += 1000;
        media = MediaPeerTelemetry{4, {}};
        controller.sampleNow();
        QVERIFY(!controller.current().coreGuiTotalKbps);
        now += 1000;
        media->traffic.receivedDisplayPayloadBytes = 1000;
        media->traffic.receivedIqPayloadBytes = 100;
        controller.sampleNow();
        QCOMPARE(controller.current().coreGuiTotalKbps, std::optional<double>(8.8));
        now += 1000;
        media->traffic.receivedDisplayPayloadBytes = 1; // counter reset, no negative rate
        controller.sampleNow();
        QVERIFY(!controller.current().coreGuiTotalKbps);
        now += 1000;
        controller.sampleNow();
        QCOMPARE(controller.current().coreGuiTotalKbps, std::optional<double>(0.0));
        QVERIFY(controller.history().series(Metric::CoreGuiTotalKbps, now, 60).points.last().breakBefore);
        now += 1000;
        media->traffic.receivedIqPayloadBytes = 1; // independent channel reset also gaps total
        controller.sampleNow();
        QVERIFY(!controller.current().coreGuiTotalKbps);
        now += 1000;
        controller.sampleNow();
        QCOMPARE(controller.current().coreGuiTotalKbps, std::optional<double>(0.0));
        client.disconnectFromStation(QStringLiteral("done"));
        QVERIFY(!controller.current().coreGuiTotalKbps);
        QVERIFY(!controller.current().audioPayloadRxKbps);
    }

    // R-R3-32/33: host values reach the view and the history; absent values
    // leave gaps. R-R3-07/33: one diagnostics line a minute for the soak.
    void coreHostValuesAndPeriodicDiagnosticsLine()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setTelemetryEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        qint64 now = 10000;
        RemoteAudioReceiverTelemetry playback;
        RemoteTelemetryController controller(&client, nullptr, nullptr,
            [&] { return now; }, [&] { return playback; });
        DiagnosticsCapture capture;

        auto* guiWire = new ObservedLoopback;
        auto* coreWire = new Test::LoopbackTransport(QStringLiteral("Core"));
        guiWire->linkTo(coreWire);
        server.acceptTransport(coreWire);
        client.startSession(guiWire, server.token());
        QTRY_VERIFY(client.isHandshakeComplete());
        QVERIFY(!controller.current().coreHostReported);

        // A sample with no host section: gaps, not zeros.
        StationTelemetrySnapshot sample;
        sample.sequence = 1;
        sample.sampledElapsedMs = 100;
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_COMPARE(controller.current().state, RemoteTelemetryView::State::Current);
        QVERIFY(!controller.current().coreHostReported);
        QVERIFY(controller.history().series(Metric::CoreSystemCpuPercent, now, 60).points.isEmpty());

        playback.running = true;
        playback.generation = 3;
        playback.acceptedPackets = 1500;
        playback.startDiscardedPackets = 62;
        playback.decodedPackets = 1490;
        playback.concealedPackets = 4;
        now += 59000;
        controller.sampleNow();
        QVERIFY2(g_diagnosticsLines.isEmpty(), qPrintable(g_diagnosticsLines.join(QLatin1Char('\n'))));
        now += 1000;
        controller.sampleNow();
        QCOMPARE(g_diagnosticsLines.size(), 1);
        QString line = g_diagnosticsLines.constLast();
        QVERIFY(line.startsWith(QStringLiteral("Remote diagnostics: context=3 running=yes admitted=1500 startDiscardedPackets=62 decoded=1490 concealed=4 ")));
        QVERIFY(line.contains(QStringLiteral(" driftRatio=not measured driftPpm=not measured ")));
        QVERIFY(line.contains(QStringLiteral(" coreSystemCpuPercent=not measured coreProcessCpuPercentOfAllCpus=not measured"
            " coreMemoryAvailableKiB=not measured coreMemoryTotalKiB=not measured"
            " coreProcessResidentKiB=not measured coreHottestZoneCelsius=not measured"
            " coreHottestZone=not measured")));
        QVERIFY(!line.contains(QLatin1Char('\n')));

        sample.sequence = 2;
        sample.sampledElapsedMs = 60100;
        sample.host.systemCpuPercent = 30.0;
        sample.host.processCpuPercent = 0.0; // measured zero is a value
        sample.host.memoryAvailableKiB = 2 * 1024 * 1024;
        sample.host.memoryTotalKiB = 8 * 1024 * 1024;
        sample.host.processResidentKiB = 204800;
        sample.host.hottestZoneCelsius = 54.5;
        sample.host.hottestZoneName = QStringLiteral("soc-thermal");
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_VERIFY(controller.current().coreHostReported);
        QCOMPARE(controller.current().coreHost.systemCpuPercent, std::optional<double>(30.0));
        QCOMPARE(controller.current().coreHost.hottestZoneName, QStringLiteral("soc-thermal"));
        QCOMPARE(controller.history().series(Metric::CoreSystemCpuPercent, now, 60).points.constLast().value, 30.0);
        QCOMPARE(controller.history().series(Metric::CoreProcessCpuPercent, now, 60).points.constLast().value, 0.0);
        QCOMPARE(controller.history().series(Metric::CoreMemoryAvailableMiB, now, 60).points.constLast().value, 2048.0);
        QCOMPARE(controller.history().series(Metric::CoreProcessResidentMiB, now, 60).points.constLast().value, 200.0);
        QCOMPARE(controller.history().series(Metric::CoreHottestZoneCelsius, now, 60).points.constLast().value, 54.5);

        playback.driftRatio = 1.000012;
        playback.lifetimeUnderflows = 2;
        playback.lifetimeOverflows = 0;
        now += 30000;
        controller.sampleNow();
        QCOMPARE(g_diagnosticsLines.size(), 1);
        now += 30000;
        controller.sampleNow();
        QCOMPARE(g_diagnosticsLines.size(), 2);
        line = g_diagnosticsLines.constLast();
        QVERIFY(line.contains(QStringLiteral(" lifetimeUnderflows=2 lifetimeOverflows=0 ")));
        QVERIFY(line.contains(QStringLiteral(" driftRatio=1.000012000 driftPpm=12.0 ")));
        QVERIFY(line.contains(QStringLiteral(" coreSystemCpuPercent=30.0 coreProcessCpuPercentOfAllCpus=0.0"
            " coreMemoryAvailableKiB=2097152 coreMemoryTotalKiB=8388608"
            " coreProcessResidentKiB=204800 coreHottestZoneCelsius=54.5"
            " coreHottestZone=\"soc-thermal\"")));

        // A new session restarts the interval; nothing is logged while
        // disconnected.
        client.disconnectFromStation(QStringLiteral("done"));
        QTRY_COMPARE(controller.current().state, RemoteTelemetryView::State::Disconnected);
        QVERIFY(!controller.current().coreHostReported);
        now += 120000;
        controller.sampleNow();
        QCOMPARE(g_diagnosticsLines.size(), 2);
    }

    // R-R3-40: each receiver's load reaches the view and its own history
    // slot; an idle receiver leaves a gap. The soak line carries each
    // receiver's load and input delay, and says when none were measured.
    void coreReceiverLoadReachesHistoryAndDiagnosticsLine()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setTelemetryEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        qint64 now = 10000;
        RemoteAudioReceiverTelemetry playback;
        RemoteTelemetryController controller(&client, nullptr, nullptr,
            [&] { return now; }, [&] { return playback; });
        DiagnosticsCapture capture;

        auto* guiWire = new ObservedLoopback;
        auto* coreWire = new Test::LoopbackTransport(QStringLiteral("Core"));
        guiWire->linkTo(coreWire);
        server.acceptTransport(coreWire);
        client.startSession(guiWire, server.token());
        QTRY_VERIFY(client.isHandshakeComplete());
        QVERIFY(!controller.current().coreReceiversReported);

        // No receivers section: not reported, and the log says so.
        StationTelemetrySnapshot sample;
        sample.sequence = 1;
        sample.sampledElapsedMs = 100;
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_COMPARE(controller.current().state, RemoteTelemetryView::State::Current);
        QVERIFY(!controller.current().coreReceiversReported);
        QVERIFY(!controller.current().coreReceivers);
        now += 60000; // one interval after the handshake's baseline
        controller.sampleNow();
        QCOMPARE(g_diagnosticsLines.size(), 1);
        QVERIFY2(g_diagnosticsLines.constLast().endsWith(QStringLiteral(" coreReceivers=not measured")),
                 qPrintable(g_diagnosticsLines.constLast()));

        StationReceiverTelemetry a;
        a.sliceId = 0;
        a.loadPercent = 62.5;
        a.inputDelayMs = 12;
        a.skippedInputMs = 0;
        StationReceiverTelemetry b;
        b.sliceId = 1;
        b.loadPercent = 140.0; // cannot keep up
        b.inputDelayMs = 480;
        b.skippedInputMs = 750;
        sample.sequence = 2;
        sample.sampledElapsedMs = 120100;
        sample.receivers = QVector<StationReceiverTelemetry>{a, b};
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_VERIFY(controller.current().coreReceiversReported);
        QVERIFY(controller.current().coreReceivers);
        QCOMPARE(controller.current().coreReceivers->size(), 2);
        const auto sliceA = TelemetryHistory::coreReceiverLoadMetric(0);
        const auto sliceB = TelemetryHistory::coreReceiverLoadMetric(1);
        QCOMPARE(controller.history().series(sliceA, now, 60).points.constLast().value, 62.5);
        QCOMPARE(controller.history().series(sliceB, now, 60).points.constLast().value, 140.0);

        // Slice B idle in the next sample: a gap, never a zero.
        b.loadPercent.reset();
        sample.sequence = 3;
        sample.sampledElapsedMs = 121100;
        sample.receivers = QVector<StationReceiverTelemetry>{a, b};
        now += 1000;
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_COMPARE(controller.history().series(sliceA, now, 60).points.size(), 2);
        QCOMPARE(controller.history().series(sliceB, now, 60).points.size(), 1);
        QCOMPARE(controller.history().series(sliceB, now, 60).points.constLast().value, 140.0);

        now += 59000;
        controller.sampleNow();
        QCOMPARE(g_diagnosticsLines.size(), 2);
        const QString line = g_diagnosticsLines.constLast();
        QVERIFY2(line.endsWith(QStringLiteral(
                     " coreReceivers=2"
                     " coreSliceALoadPercent=62.5 coreSliceAInputDelayMs=12 coreSliceASkippedInputMs=0"
                     " coreSliceBLoadPercent=not measured coreSliceBInputDelayMs=480"
                     " coreSliceBSkippedInputMs=750")),
                 qPrintable(line));

        // A new session forgets that this Core reported receivers.
        client.disconnectFromStation(QStringLiteral("done"));
        QTRY_COMPARE(controller.current().state, RemoteTelemetryView::State::Disconnected);
        QVERIFY(!controller.current().coreReceiversReported);
        QVERIFY(!controller.current().coreReceivers);
    }

    // R-R3-32 / R-R3-46 (remote-window parity Task 6): station telemetry
    // version 4 carries the Core's PA readings and radio link quality; the
    // window's model shows the Core's (present), leaves a reading the Core
    // did not send absent (absent), and clears them all when the
    // measurements are out of date (stale), never showing 0 for either.
    void coreRadioPaReadingsAndLinkQuality()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setTelemetryEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        qint64 now = 10000;
        RemoteTelemetryController controller(&client, nullptr, nullptr, [&] { return now; });
        controller.setPaReadingsTarget(&remote);
        QVERIFY(!remote.paReadings().paVolts);

        auto* guiWire = new ObservedLoopback;
        auto* coreWire = new Test::LoopbackTransport(QStringLiteral("Core"));
        guiWire->linkTo(coreWire);
        server.acceptTransport(coreWire);
        client.startSession(guiWire, server.token());
        QTRY_VERIFY(client.isHandshakeComplete());
        QCOMPARE(client.capabilities().stationTelemetryVersion, 6);

        StationTelemetrySnapshot sample;
        sample.sequence = 1;
        sample.sampledElapsedMs = 100;
        sample.radio.connected = true;
        sample.radio.paVolts = 13.8;
        sample.radio.supplyVolts = 12.1;
        sample.radio.paCurrentAmps = 0.0;          // a measured zero is a value
        sample.radio.paTemperatureCelsius = 41.5;
        sample.radio.packetLossPercent = 0.25;
        sample.radio.jitterMs = 0.37;
        sample.radio.packetGapMs = 4.2;
        sample.radio.sampleRateHz = 192000;
        sample.radio.udpPacketsSeen = 123456;
        QSignalSpy paChanged(&remote, &RadioModel::paReadingsChanged);
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_COMPARE(controller.current().state, RemoteTelemetryView::State::Current);
        const StationRadioTelemetry& radio = controller.current().radio;
        QCOMPARE(radio.packetLossPercent, std::optional<double>(0.25));
        QCOMPARE(radio.udpPacketsSeen, std::optional<qint64>(123456));
        QCOMPARE(radio.sampleRateHz, std::optional<qint64>(192000));
        RadioModel::PaReadings pa = remote.paReadings();
        QCOMPARE(pa.paVolts, std::optional<double>(13.8));
        QCOMPARE(pa.supplyVolts, std::optional<double>(12.1));
        QCOMPARE(pa.paCurrentAmps, std::optional<double>(0.0));
        QCOMPARE(pa.paTemperatureCelsius, std::optional<double>(41.5));
        QVERIFY(paChanged.count() >= 1);
        QVERIFY(remote.paReadingsFromCore());

        const QString detail = controller.detailText();
        for (const QString& expected : {
                 QStringLiteral("PA voltage from the Core: 13.8\u00A0V."),
                 QStringLiteral("DC voltage from the Core: 12.1\u00A0V."),
                 QStringLiteral("Packet loss between the Core and the radio, from the Core: 0.25\u00A0% over the last 5 seconds."),
                 QStringLiteral("Radio jitter from the Core: 0.37\u00A0ms."),
                 QStringLiteral("Longest gap between radio packets in the last second, from the Core: 4.2\u00A0ms."),
                 QStringLiteral("Radio sample rate from the Core: 192\u00A0kHz."),
                 QStringLiteral("UDP packets seen from the radio since it connected, from the Core: 123456.")}) {
            QVERIFY2(detail.contains(expected), qPrintable(expected + QStringLiteral("\n") + detail));
        }
        for (const QString& line : detail.split(QLatin1Char('\n'))) {
            QVERIFY2(OperatorWording::isPlain(line), qPrintable(line));
        }

        // Absent: a reading the Core did not send stays absent.
        sample.sequence = 2;
        sample.sampledElapsedMs = 1100;
        sample.radio.paCurrentAmps.reset();
        sample.radio.jitterMs.reset();
        now += 1000;
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_VERIFY(!remote.paReadings().paCurrentAmps);
        QCOMPARE(remote.paReadings().paVolts, std::optional<double>(13.8));
        QVERIFY(controller.detailText().contains(QStringLiteral("Radio jitter from the Core: unavailable.")));

        // Stale: out-of-date measurements leave every reading absent.
        now += 4000;
        controller.sampleNow();
        QCOMPARE(controller.current().state, RemoteTelemetryView::State::Stale);
        pa = remote.paReadings();
        QVERIFY(!pa.paVolts && !pa.supplyVolts && !pa.paCurrentAmps && !pa.paTemperatureCelsius);
        QVERIFY(controller.detailText().contains(QStringLiteral("PA voltage from the Core: unavailable.")));

        // A fresh sample brings them back; the session ending clears them.
        sample.sequence = 3;
        sample.sampledElapsedMs = 5100;
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_COMPARE(remote.paReadings().paVolts, std::optional<double>(13.8));
        client.disconnectFromStation(QStringLiteral("done"));
        QTRY_VERIFY(!remote.paReadings().paVolts);
    }

    // R-R3-32 (parity Task 6): the version 4 radio fields' wire rules.
    void radioStatusCodecRules()
    {
        StationTelemetrySnapshot sample;
        sample.sequence = 1;
        sample.radio.connected = true;
        sample.radio.paVolts = 13.8;
        sample.radio.paTemperatureCelsius = -5.0;   // a cold PA is a value
        sample.radio.packetLossPercent = 100.0;
        sample.radio.udpPacketsSeen = 0;
        const std::optional<QJsonObject> wire = StationTelemetryCodec::encode(sample);
        QVERIFY(wire);
        StationTelemetrySnapshot decoded;
        QVERIFY(StationTelemetryCodec::decode(*wire, &decoded));
        QCOMPARE(decoded.radio.paVolts, std::optional<double>(13.8));
        QCOMPARE(decoded.radio.paTemperatureCelsius, std::optional<double>(-5.0));
        QCOMPARE(decoded.radio.udpPacketsSeen, std::optional<qint64>(0));
        QVERIFY(!decoded.radio.supplyVolts);   // absent stays absent

        const auto rejects = [&](const char* key, const QJsonValue& value, bool connected = true) {
            QJsonObject object = *wire;
            QJsonObject radio = object.value(QStringLiteral("radio")).toObject();
            radio.insert(QStringLiteral("connected"), connected);
            radio.insert(QString::fromLatin1(key), value);
            object.insert(QStringLiteral("radio"), radio);
            StationTelemetrySnapshot out;
            return !StationTelemetryCodec::decode(object, &out);
        };
        QVERIFY(rejects("paVolts", -1.0));
        QVERIFY(rejects("packetLossPercent", 100.5));
        QVERIFY(rejects("paTemperatureCelsius", -300.0));
        QVERIFY(rejects("udpPacketsSeen", 1.5));
        QVERIFY(rejects("sampleRateHz", QStringLiteral("192k")));
        // A disconnected radio reports none of them.
        QJsonObject offline = *wire;
        QJsonObject radio = offline.value(QStringLiteral("radio")).toObject();
        radio.insert(QStringLiteral("connected"), false);
        offline.insert(QStringLiteral("radio"), radio);
        StationTelemetrySnapshot out;
        QVERIFY(!StationTelemetryCodec::decode(offline, &out));
        sample.radio.paVolts = -2.0;
        QVERIFY(!StationTelemetryCodec::encode(sample));
    }

    // R-R3-32 (parity Task 14, stationTelemetryVersion 5): the Core's HL2
    // link reaches the window's model and the HL2 I/O tab's bandwidth
    // monitor, said to be the Core's; out of date, every figure is absent
    // and shown as unavailable, never 0. The throttle event count is not
    // sent, so a remote window never shows one.
    void coreHl2LinkReachesTheWindow()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setTelemetryEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        qint64 now = 10000;
        RemoteTelemetryController controller(&client, nullptr, nullptr, [&] { return now; });
        controller.setPaReadingsTarget(&remote);
        QVERIFY(remote.hl2LinkFiguresFromCore());
        QVERIFY(!remote.hl2LinkFigures().rxBytesPerSecond);
        Hl2IoBoardTab ioTab(&remote);
        ioTab.pollBandwidthNowForTest();
        QCOMPARE(ioTab.ep6RateTextForTest(), QStringLiteral("Unavailable"));

        auto* guiWire = new ObservedLoopback;
        auto* coreWire = new Test::LoopbackTransport(QStringLiteral("Core"));
        guiWire->linkTo(coreWire);
        server.acceptTransport(coreWire);
        client.startSession(guiWire, server.token());
        QTRY_VERIFY(client.isHandshakeComplete());
        QCOMPARE(client.capabilities().stationTelemetryVersion, 6);

        StationTelemetrySnapshot sample;
        sample.sequence = 1;
        sample.sampledElapsedMs = 100;
        sample.radio.connected = true;
        sample.radio.hl2RxBytesPerSecond = 1250000.0;
        sample.radio.hl2TxBytesPerSecond = 0.0;       // a measured zero is a value
        sample.radio.hl2Throttled = true;
        sample.radio.hl2SequenceGaps = 7;
        QVERIFY(server.sendTelemetry(sample, server.sessionEpoch()));
        QTRY_COMPARE(controller.current().state, RemoteTelemetryView::State::Current);
        RadioModel::Hl2LinkFigures figures = remote.hl2LinkFigures();
        QCOMPARE(figures.rxBytesPerSecond, std::optional<double>(1250000.0));
        QCOMPARE(figures.txBytesPerSecond, std::optional<double>(0.0));
        QCOMPARE(figures.throttled, std::optional<bool>(true));
        QCOMPARE(figures.sequenceGaps, std::optional<qint64>(7));
        QVERIFY(!figures.throttleEvents);
        ioTab.pollBandwidthNowForTest();
        // 1,250,000 bytes a second is 10.0 Mbit/s, mi0bot's unit
        // (ucBandwidthView.cs toDisplayUnits [@c26a8a4]); the bar is full
        // at 10 Mbit/s.
        QCOMPARE(ioTab.ep6RateTextForTest(), QStringLiteral("10.0 Mbit/s"));
        QCOMPARE(ioTab.ep2RateTextForTest(), QStringLiteral("0.0 Mbit/s"));
        QCOMPARE(ioTab.ep6BarPercentForTest(), 100);
        QCOMPARE(ioTab.ep2BarPercentForTest(), 0);
        QVERIFY(ioTab.throttleStatusTextForTest().contains(QStringLiteral("throttled")));
        QVERIFY(!ioTab.throttleStatusTextForTest().contains(QStringLiteral("not")));
        QCOMPARE(ioTab.throttleEventTextForTest(), QStringLiteral("Unavailable"));
        // Radio Status's Connection Quality card and Diagnostics >
        // Connection Quality's Live Counters, each said to be the Core's.
        const auto hasLabel = [](QWidget* root, const QString& text) {
            for (QLabel* label : root->findChildren<QLabel*>()) {
                if (label->text() == text) { return true; }
            }
            return false;
        };
        RadioStatusPage status(&remote);
        ConnectionQualityPage quality(&remote);
        QTRY_VERIFY(hasLabel(&status, QStringLiteral("Connection Quality, from the Core")));
        QTRY_VERIFY(hasLabel(&status, QStringLiteral("1220.7 KB/s")));
        QVERIFY(hasLabel(&status, QStringLiteral("Active")));
        QVERIFY(hasLabel(&status, QStringLiteral("7")));
        QTRY_VERIFY(hasLabel(&quality, QStringLiteral("1250000 B/s")));
        QVERIFY(hasLabel(&quality, QStringLiteral("THROTTLED")));
        bool liveTitled = false;
        for (QGroupBox* box : quality.findChildren<QGroupBox*>()) {
            liveTitled = liveTitled || box->title() == QStringLiteral("Live Counters, from the Core");
        }
        QVERIFY(liveTitled);

        // Out of date: every figure absent, shown as unavailable.
        now += 5000;
        controller.sampleNow();
        QCOMPARE(controller.current().state, RemoteTelemetryView::State::Stale);
        figures = remote.hl2LinkFigures();
        QVERIFY(!figures.rxBytesPerSecond && !figures.txBytesPerSecond && !figures.throttled
                && !figures.sequenceGaps);
        ioTab.pollBandwidthNowForTest();
        QCOMPARE(ioTab.ep6RateTextForTest(), QStringLiteral("Unavailable"));
        QCOMPARE(ioTab.throttleStatusTextForTest(), QStringLiteral("Unavailable"));
        QTRY_VERIFY(!hasLabel(&quality, QStringLiteral("1250000 B/s")));
        QVERIFY(hasLabel(&quality, QStringLiteral("Unavailable")));
        QTRY_VERIFY(!hasLabel(&status, QStringLiteral("1220.7 KB/s")));

        client.disconnectFromStation(QStringLiteral("done"));
    }

    // R-R3-32 (parity Task 14): the version 5 fields' wire rules.
    void hl2LinkCodecRules()
    {
        StationTelemetrySnapshot sample;
        sample.sequence = 1;
        sample.radio.connected = true;
        sample.radio.hl2RxBytesPerSecond = 10.5;
        sample.radio.hl2Throttled = false;
        sample.radio.hl2SequenceGaps = 0;
        const std::optional<QJsonObject> wire = StationTelemetryCodec::encode(sample);
        QVERIFY(wire);
        StationTelemetrySnapshot decoded;
        QVERIFY(StationTelemetryCodec::decode(*wire, &decoded));
        QCOMPARE(decoded.radio.hl2RxBytesPerSecond, std::optional<double>(10.5));
        QCOMPARE(decoded.radio.hl2Throttled, std::optional<bool>(false));
        QCOMPARE(decoded.radio.hl2SequenceGaps, std::optional<qint64>(0));
        QVERIFY(!decoded.radio.hl2TxBytesPerSecond);   // absent stays absent

        const auto rejects = [&](const char* key, const QJsonValue& value) {
            QJsonObject object = *wire;
            QJsonObject radio = object.value(QStringLiteral("radio")).toObject();
            radio.insert(QString::fromLatin1(key), value);
            object.insert(QStringLiteral("radio"), radio);
            StationTelemetrySnapshot out;
            return !StationTelemetryCodec::decode(object, &out);
        };
        QVERIFY(rejects("hl2RxBytesPerSecond", -1.0));
        QVERIFY(rejects("hl2TxBytesPerSecond", QStringLiteral("fast")));
        QVERIFY(rejects("hl2Throttled", 1));
        QVERIFY(rejects("hl2SequenceGaps", 2.5));
        // A disconnected radio reports none of them.
        QJsonObject offline = *wire;
        QJsonObject radio = offline.value(QStringLiteral("radio")).toObject();
        radio.insert(QStringLiteral("connected"), false);
        offline.insert(QStringLiteral("radio"), radio);
        StationTelemetrySnapshot out;
        QVERIFY(!StationTelemetryCodec::decode(offline, &out));
        sample.radio.hl2SequenceGaps = -1;
        QVERIFY(!StationTelemetryCodec::encode(sample));
    }

    // A window at version 4 (an older Core) drops the HL2 link fields.
    void hl2LinkNeedsVersion5()
    {
        StationTelemetrySnapshot sample;
        sample.radio.connected = true;
        sample.radio.hl2RxBytesPerSecond = 1.0;
        sample.radio.hl2Throttled = true;
        sample.radio.paVolts = 13.8;
        sample.radio.clearHl2Link();
        QVERIFY(sample.radio.hasNoHl2Link());
        QCOMPARE(sample.radio.paVolts, std::optional<double>(13.8));
    }

    void olderCoreIsExplicitlyUnsupported()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path())); // capability disabled
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        RemoteTelemetryController controller(&client, nullptr);
        auto* gui = new Test::LoopbackTransport(QStringLiteral("GUI"));
        auto* core = new Test::LoopbackTransport(QStringLiteral("Core"));
        gui->linkTo(core);
        server.acceptTransport(core);
        client.startSession(gui, server.token());
        QTRY_VERIFY(client.isHandshakeComplete());
        QCOMPARE(controller.current().state, RemoteTelemetryView::State::Unsupported);
        QVERIFY(controller.bannerText().contains(QStringLiteral("measurements not offered")));
        QVERIFY2(OperatorWording::isPlain(controller.bannerText()), qPrintable(controller.bannerText()));
        QVERIFY(!controller.current().radio.rxMbps);
        client.disconnectFromStation(QStringLiteral("done"));
    }

    // R-R3-35: the measured audio delay. A Core that cannot measure it keeps
    // today's sentence; one that can says "not measured" until a figure
    // exists, then shows the delay with its accuracy and the delivery delay
    // separately. Echoes stopping and a reconnect both leave gaps in the
    // history, and the periodic diagnostics line carries the figures.
    void measuredAudioDelayIsDescribedGraphedAndGappedAcrossReconnects()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setTelemetryEnabled(true);
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        qint64 now = 10000;
        RemoteAudioReceiverTelemetry playback;
        playback.running = true;
        playback.generation = 3;
        RemoteAudioDelayReport delay;
        DiagnosticsCapture capture;
        RemoteTelemetryController controller(&client, nullptr, nullptr,
            [&] { return now; }, [&] { return playback; }, {}, [&] { return delay; });
        const auto connect = [&] {
            auto* gui = new ObservedLoopback;
            auto* core = new Test::LoopbackTransport(QStringLiteral("Core"));
            gui->linkTo(core);
            server.acceptTransport(core);
            client.startSession(gui, server.token());
            QTRY_VERIFY(client.isHandshakeComplete());
        };
        connect();
        const QString olderCoreLine = QStringLiteral(
            "End-to-end audio latency is not measured. Core RTT is a control round trip, "
            "not one-way audio latency; RTT/2 is not used.");

        // A Core without clock probes: exactly today's sentence.
        controller.sampleNow();
        QVERIFY(controller.detailText().contains(olderCoreLine));
        QVERIFY(!controller.detailText().contains(QStringLiteral("Audio delay")));
        QVERIFY(controller.history().series(Metric::AudioDelayMs, now, 60).points.isEmpty());

        // Measurable, nothing yet.
        delay.measurable = true;
        now += 1000;
        controller.sampleNow();
        QVERIFY(!controller.detailText().contains(olderCoreLine));
        QVERIFY(controller.detailText().contains(QStringLiteral(
            "Audio delay: not measured. It needs audio playing and answers from the Core.")));

        // Measured: the delay with its accuracy, the device not counted, and
        // delivery on its own line.
        delay.estimate = AudioDelayEstimate{85.2, 0.4, false, 62.3, 0.4};
        now += 1000;
        controller.sampleNow();
        QVERIFY2(controller.detailText().contains(QStringLiteral(
            "Audio delay: 85\u00A0ms \u00B1 1\u00A0ms, not counting the speaker device, "
            "from the Core's audio to this computer's speaker.")),
                 qPrintable(controller.detailText()));
        QVERIFY(controller.detailText().contains(QStringLiteral(
            "Delivery delay: 62\u00A0ms \u00B1 1\u00A0ms, from the Core's audio to this "
            "computer's player, before the speaker queue.")));
        QCOMPARE(controller.current().audioDelay, delay);
        delay.estimate->includesDevice = true;
        now += 1000;
        controller.sampleNow();
        QVERIFY(controller.detailText().contains(QStringLiteral(
            "Audio delay: 85\u00A0ms \u00B1 1\u00A0ms, from the Core's audio")));
        auto series = controller.history().series(Metric::AudioDelayMs, now, 60);
        QCOMPARE(series.points.size(), 2);
        QCOMPARE(series.points.last().value, 85.2);
        QVERIFY(!series.points.last().breakBefore);
        QCOMPARE(controller.history().series(Metric::AudioDelayAccuracyMs, now, 60)
                     .points.last().value, 0.4);
        QCOMPARE(controller.history().series(Metric::AudioDeliveryDelayMs, now, 60)
                     .points.last().value, 62.3);

        // Echoes stop: no figure, and the next one starts a new line.
        delay.estimate.reset();
        now += 1000;
        controller.sampleNow();
        QVERIFY(controller.detailText().contains(QStringLiteral("Audio delay: not measured.")));
        delay.estimate = AudioDelayEstimate{90.0, 0.5, true, 60.0, 0.5};
        now += 1000;
        controller.sampleNow();
        series = controller.history().series(Metric::AudioDelayMs, now, 60);
        QCOMPARE(series.points.size(), 3);
        QVERIFY(series.points.last().breakBefore);

        // The diagnostics line, one interval after the session began.
        now += 60000;
        controller.sampleNow();
        QVERIFY(!g_diagnosticsLines.isEmpty());
        QVERIFY2(g_diagnosticsLines.constLast().contains(QStringLiteral(
            "audioDelayMs=90.0 audioDelayAccuracyMs=0.5 audioDelayIncludesDevice=yes "
            "deliveryDelayMs=60.0")),
                 qPrintable(g_diagnosticsLines.constLast()));
        // R-R3-21 / R-R3-08: the display counters follow, not measured
        // until a display has run.
        QVERIFY2(g_diagnosticsLines.constLast().contains(QStringLiteral(
            "deliveryDelayMs=60.0 displayKeyframeWaits=0 displayKeyframeRequests=0 "
            "displayRowsBlended=0 displayRowsRepeated=0 "
            "displayLargestArrivalGapMs=not measured displayDelayMs=not measured "
            "displayItemsDropped=0 displayRowsDropped=0")),
                 qPrintable(g_diagnosticsLines.constLast()));

        // A reconnect: the first figure after it starts a new line.
        const auto breaks = [](const TelemetryHistory::Series& path) {
            return std::count_if(path.points.cbegin(), path.points.cend(),
                                 [](const TelemetryHistory::Point& point) { return point.breakBefore; });
        };
        const qsizetype pointsBefore =
            controller.history().series(Metric::AudioDelayMs, now, 120).points.size();
        const auto breaksBefore = breaks(controller.history().series(Metric::AudioDelayMs, now, 120));
        client.disconnectFromStation(QStringLiteral("operator disconnect"));
        QCOMPARE(controller.current().audioDelay, RemoteAudioDelayReport{});
        QCoreApplication::processEvents();
        ++now;
        connect();
        now += 1000;
        controller.sampleNow();
        series = controller.history().series(Metric::AudioDelayMs, now, 120);
        QVERIFY(series.points.size() > pointsBefore);
        QVERIFY(series.points.at(pointsBefore).breakBefore);
        QCOMPARE(breaks(series), breaksBefore + 1);
        QCOMPARE(series.points.last().value, 90.0);
        client.disconnectFromStation(QStringLiteral("done"));
    }

    // Parity ruling C13: the Performance Overlay in a remote window shows
    // the Core's drops after this computer's counters, each group headed;
    // a missing reading is "-", never 0; no current readings say so.
    void performanceOverlayShowsTheCoresDrops()
    {
        RemoteTelemetryView view;
        QCOMPARE(RemoteTelemetryController::performanceOverlayLines(view),
                 QStringList{QStringLiteral("the Core: no current readings")});
        view.state = RemoteTelemetryView::State::Stale;
        QCOMPARE(RemoteTelemetryController::performanceOverlayLines(view).size(), 1);

        view.state = RemoteTelemetryView::State::Current;
        view.radio.packetLossPercent = 0.25;
        view.coreAudio.sourceDropsPerSecond = 1.5;
        QCOMPARE(RemoteTelemetryController::performanceOverlayLines(view),
                 (QStringList{QStringLiteral("the Core:"),
                              QStringLiteral("radio  lost 0.25% (5 s) gap - ms"),
                              QStringLiteral("audio  drops 1.5/s not sent -/s")}));
        view.radio.packetGapMs = 12.0;
        view.radio.hl2SequenceGaps = 3;
        view.coreAudio.sendRejectedPerSecond = 0.0;
        QCOMPARE(RemoteTelemetryController::performanceOverlayLines(view),
                 (QStringList{QStringLiteral("the Core:"),
                              QStringLiteral("radio  lost 0.25% (5 s) gap 12.0 ms"),
                              QStringLiteral("radio  sequence gaps 3"),
                              QStringLiteral("audio  drops 1.5/s not sent 0.0/s")}));

        SpectrumWidget spectrum;
        const QStringList local = spectrum.perfOverlayLines();
        QVERIFY(!local.isEmpty());
        QVERIFY(!local.contains(QStringLiteral("this computer:")));
        spectrum.setCorePerfLinesProvider(
            [view] { return RemoteTelemetryController::performanceOverlayLines(view); });
        const QStringList remote = spectrum.perfOverlayLines();
        QCOMPARE(remote.first(), QStringLiteral("this computer:"));
        QCOMPARE(remote.mid(1, local.size()).size(), local.size());
        QCOMPARE(remote.mid(1 + local.size()),
                 RemoteTelemetryController::performanceOverlayLines(view));
    }
};

QTEST_MAIN(TestRemoteTelemetry)
#include "tst_remote_telemetry.moc"

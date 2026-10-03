// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 24 (R-IOS-10, R-R3-08, R-R3-13): sound only. A media
// session with no display endpoint, from the start or after closing every
// one, keeps receiving its audio at 25 packets a second, its 1 Hz station
// metrics and its slices' S-meter readings, while no display datagram
// arrives and the Core runs no display endpoint or source for it. Asking
// for a display again brings the band back with a keyframe on the first
// frame.
//
// Virtual time throughout: the media controller's clock and the telemetry
// clock are the test's, audio is fed as whole 40 ms blocks, and the display
// sender and the metrics sample are ticked by hand.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original test for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/P2RadioConnection.h"
#include "core/meters/SliceMeterPump.h"
#include "core/daemon/DaemonTelemetryController.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/StationTelemetry.h"
#include "core/session/media/DaemonAudioSource.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/IMediaTransport.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QJsonObject>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cmath>
#include <numbers>

using namespace NereusSDR;

namespace {

constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";
// One display frame interval at the 60 fps the subscriptions ask for.
constexpr qint64 kFrameIntervalNs = 1'000'000'000 / 60;
// Audio blocks a second: 48 kHz in 40 ms blocks.
constexpr int kAudioPacketsPerSecond = 48000 / DaemonAudioSource::kBlockFrames;
static_assert(kAudioPacketsPerSecond == 25);

bool isKeyframe(const QByteArray& frame)
{
    return frame.size() > 5 && (static_cast<quint8>(frame.at(5)) & 0x01) != 0;
}

// The media peer, recording what the Core gives it: display datagrams and
// audio RTP packets.
class RecordingTransport final : public IMediaTransport {
public:
    explicit RecordingTransport(QObject* parent) : IMediaTransport(parent) {}

    bool start(const StartOptions& options) override
    {
        startOptions = options;
        started = true;
        return true;
    }
    void stop() override { started = readyState = false; }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray& bytes) override
    {
        if (!readyState) { return false; }
        displays.append(bytes);
        return true;
    }
    bool sendRtp(const QByteArray& packet) override
    {
        if (readyState) { rtpPackets.append(packet); }
        return readyState;
    }
    bool isReady() const override { return readyState; }
    void becomeReady() { readyState = true; emit ready(); }

    bool started{false};
    bool readyState{false};
    StartOptions startOptions{Role::Answerer, 0};
    QList<QByteArray> displays;
    QList<QByteArray> rtpPackets;
};

QVector<float> syntheticIq(int complexSamples, double cyclesPerSample)
{
    QVector<float> samples;
    samples.reserve(complexSamples * 2);
    for (int sample = 0; sample < complexSamples; ++sample) {
        const double phase = 2.0 * std::numbers::pi * cyclesPerSample * sample;
        samples.append(static_cast<float>(std::cos(phase)));
        samples.append(static_cast<float>(std::sin(phase)));
    }
    return samples;
}

QJsonObject plane()
{
    return {{QStringLiteral("detector"), static_cast<int>(SpectrumDetectorMode::Peak)},
            {QStringLiteral("averageMode"), -1},
            {QStringLiteral("averageAlpha"), 0.0}};
}

QJsonObject subscription(quint32 endpointId, int sliceId, double centreHz)
{
    return {{QStringLiteral("op"), QStringLiteral("subscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)},
            {QStringLiteral("revision"), 1},
            {QStringLiteral("sliceId"), sliceId},
            {QStringLiteral("tier"), QStringLiteral("wide")},
            {QStringLiteral("fftSize"), 1024},
            {QStringLiteral("windowType"), static_cast<int>(WindowFunction::Hann)},
            {QStringLiteral("centreHz"), centreHz},
            {QStringLiteral("spanHz"), 48000.0},
            {QStringLiteral("pixels"), 128},
            {QStringLiteral("fps"), 60},
            {QStringLiteral("framesPerLine"), 1},
            {QStringLiteral("trace"), plane()},
            {QStringLiteral("waterfall"), plane()},
            {QStringLiteral("minDbm"), -180.0},
            {QStringLiteral("maxDbm"), 0.0},
            {QStringLiteral("wideSpanFactor"), 0.0}};
}

QJsonObject unsubscription(quint32 endpointId)
{
    return {{QStringLiteral("op"), QStringLiteral("unsubscribe")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("endpointId"), static_cast<qint64>(endpointId)}};
}

QJsonObject audioControl(quint32 revision, bool enabled)
{
    return {{QStringLiteral("op"), QStringLiteral("audio")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("revision"), static_cast<qint64>(revision)},
            {QStringLiteral("enabled"), enabled}};
}

QJsonObject messageFor(const QSignalSpy& messages, const QString& op, quint32 endpointId)
{
    for (auto it = messages.crbegin(); it != messages.crend(); ++it) {
        const QJsonObject message = it->at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == op
            && static_cast<quint32>(message.value(QStringLiteral("endpointId")).toInteger())
                == endpointId) {
            return message;
        }
    }
    return {};
}

StationTelemetrySnapshot lastSnapshot(const QSignalSpy& samples)
{
    return qvariant_cast<StationTelemetrySnapshot>(samples.constLast().at(0));
}

struct Harness {
    QTemporaryDir directory;
    AppSettings settings;
    RadioModel radio;
    StationServer server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy settingsProxy;
    StationClient client{&remote, &settingsProxy};
    QPointer<RecordingTransport> mediaTransport;
    qint64 nowNs{0};
    DaemonMediaController controller;
    DaemonTelemetryController telemetry;
    QPointer<QTimer> displaySender;
    int sliceId{-1};
    int streamIndex{-1};

    Harness()
        : settings(directory.filePath(QStringLiteral("station.settings")))
        , server(&radio, settings,
                 NereusSDR::Test::withSharedTlsIdentity(
                     NereusSDR::Test::seedUpgradedCoreToken(directory.path())))
        , controller(&server, &radio, nullptr,
                     [this](QObject* parent) -> IMediaTransport* {
                         mediaTransport = new RecordingTransport(parent);
                         return mediaTransport;
                     },
                     [this] { return nowNs; })
        , telemetry(&server, &radio, &controller, nullptr,
                    [this] { return nowNs / 1'000'000; })
    {
        Q_ASSERT(directory.isValid());
        telemetry.disableAutomaticSamplingForTest();
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        sliceId = radio.addSlice();
        SliceModel* const slice = radio.sliceById(sliceId);
        Q_ASSERT(slice != nullptr);
        streamIndex = slice->streamIndex();
        server.setMediaEnabled(true);
        server.setTelemetryEnabled(true);
        // No WDSP channel here: stop the pump that would write the
        // no-reading value over the readings the test sets (R-R3-13).
        radio.sliceMeterPump()->stop();
        for (QTimer* timer : controller.findChildren<QTimer*>(
                 QString(), Qt::FindDirectChildrenOnly)) {
            // The display sender (DisplayBudget.h), ticked by hand.
            if (timer->interval() == static_cast<int>(kDisplaySenderIntervalMs)) {
                displaySender = timer;
                timer->setInterval(60'000);
            }
        }
    }

    void establishSession()
    {
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(server.mediaAvailable() && client.mediaAvailable());
    }

    // The session starts media with audio on and asks for no display.
    void startSoundOnly()
    {
        QJsonObject start{{QStringLiteral("op"), QStringLiteral("start")},
                          {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}};
        QVERIFY(client.sendMediaControl(start, client.sessionEpoch()));
        QTRY_VERIFY(mediaTransport);
        QVERIFY(mediaTransport->started);
        mediaTransport->becomeReady();
        QVERIFY(client.sendMediaControl(audioControl(1, true), client.sessionEpoch()));
    }

    void tickDisplay()
    {
        QVERIFY(displaySender);
        displaySender->stop();
        QVERIFY(QMetaObject::invokeMethod(displaySender, "timeout", Qt::DirectConnection));
    }

    void feedRadio(double cyclesPerSample = 0.125)
    {
        QVERIFY(QMetaObject::invokeMethod(
            &radio, "rawIqDataForStream", Qt::DirectConnection, Q_ARG(int, streamIndex),
            Q_ARG(QVector<float>, syntheticIq(1026, cyclesPerSample))));
    }

    // One virtual second: I/Q for the band with a display tick every frame
    // interval, and 25 audio blocks from the slice. Each block is let out
    // before the next goes in: the source queues at most
    // DaemonAudioSource::kQueueBlocks, and a burst of a whole second would
    // measure that bound, not the rate.
    void runOneSecond()
    {
        const qint64 start = nowNs;
        for (int frame = 0; frame < 60; ++frame) {
            nowNs += kFrameIntervalNs;
            feedRadio(0.125 + 0.0078125 * (frame % 16));
            tickDisplay();
        }
        AudioEngine* const engine = radio.audioEngine();
        const QVector<float> chunk(64 * 2, 0.25f);
        for (int block = 0; block < kAudioPacketsPerSecond; ++block) {
            const qsizetype before = mediaTransport->rtpPackets.size();
            for (int frames = 0; frames < DaemonAudioSource::kBlockFrames; frames += 64) {
                engine->rxBlockReady(sliceId, chunk.constData(), 64);
            }
            QTRY_COMPARE(mediaTransport->rtpPackets.size(), before + 1);
        }
        nowNs = start + 1'000'000'000;
    }

    void finish() { client.disconnectFromStation(QStringLiteral("test complete")); }
};

} // namespace

class TstSoundOnlySession : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        qRegisterMetaType<QVector<float>>();
        // The metrics the session keeps getting are the 1 Hz sample.
        QCOMPARE(DaemonTelemetryController::kSamplePeriodMs, 1000);
    }

    // Audio on, no subscription from the start.
    void soundOnlyFromTheStart()
    {
        Harness h;
        h.radio.audioEngine()->masterMixForTest().setRampFrames(1);
        h.radio.audioEngine()->masterMixForTest().setSlewUpFrames(0);
        h.radio.audioEngine()->setSliceStreaming(h.sliceId, true);
        h.establishSession();
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.startSoundOnly();
        QTRY_VERIFY(messageFor(controls, QStringLiteral("audio-context"), 0)
                        .value(QStringLiteral("enabled")).toBool());
        QTRY_VERIFY(h.client.telemetryAvailable());
        h.telemetry.sampleNow();
        QTRY_COMPARE(samples.count(), 1);

        verifySoundOnlySeconds(h, samples, 3);
        h.finish();
    }

    // A session that closes every display endpoint mid-session, then asks
    // for one again.
    void soundOnlyAfterClosingEveryEndpointThenBack()
    {
        Harness h;
        h.radio.audioEngine()->masterMixForTest().setRampFrames(1);
        h.radio.audioEngine()->masterMixForTest().setSlewUpFrames(0);
        h.radio.audioEngine()->setSliceStreaming(h.sliceId, true);
        h.establishSession();
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QSignalSpy samples(&h.client, &StationClient::telemetryReceived);
        h.startSoundOnly();
        QTRY_VERIFY(messageFor(controls, QStringLiteral("audio-context"), 0)
                        .value(QStringLiteral("enabled")).toBool());
        const double centre = h.radio.streamCentreHz(h.streamIndex);

        // Two displays, both sending.
        for (quint32 endpoint : {7u, 8u}) {
            QVERIFY(h.client.sendMediaControl(subscription(endpoint, h.sliceId, centre),
                                              h.client.sessionEpoch()));
        }
        QTRY_COMPARE(h.controller.activeEndpointCount(), 2);
        QTRY_VERIFY(([&] {
            h.nowNs += kFrameIntervalNs;
            h.feedRadio();
            h.tickDisplay();
            return !h.mediaTransport->displays.isEmpty();
        })());

        // Every one closed.
        for (quint32 endpoint : {7u, 8u}) {
            QVERIFY(h.client.sendMediaControl(unsubscription(endpoint), h.client.sessionEpoch()));
        }
        QTRY_COMPARE(h.controller.activeEndpointCount(), 0);
        QTRY_COMPARE(h.controller.activeSourceCount(), 0);
        QTRY_VERIFY(h.client.telemetryAvailable());
        h.telemetry.sampleNow();
        QTRY_COMPARE(samples.count(), 1);
        verifySoundOnlySeconds(h, samples, 2);

        // Back: a new display (a closed one's id is not used again), and the
        // keyframe the app asks for once it has the context. The first frame
        // after it, one frame interval on, is a keyframe.
        QVERIFY(h.client.sendMediaControl(subscription(9, h.sliceId, centre),
                                          h.client.sessionEpoch()));
        QTRY_COMPARE(h.controller.activeEndpointCount(), 1);
        QTRY_VERIFY(([&] {
            h.feedRadio();
            return !messageFor(controls, QStringLiteral("context"), 9).isEmpty();
        })());
        const QJsonObject context = messageFor(controls, QStringLiteral("context"), 9);
        QVERIFY(h.client.sendMediaControl(
            {{QStringLiteral("op"), QStringLiteral("keyframe")},
             {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
             {QStringLiteral("endpointId"), 9},
             {QStringLiteral("contextGeneration"),
              context.value(QStringLiteral("contextGeneration")).toInteger()}},
            h.client.sessionEpoch()));
        QTRY_COMPARE(h.controller.displayDiagnostics().displayKeyframeRequests, quint64{1});
        h.mediaTransport->displays.clear();
        h.nowNs += kFrameIntervalNs;
        QTRY_VERIFY(([&] {
            h.feedRadio(0.1875);
            h.tickDisplay();
            return !h.mediaTransport->displays.isEmpty();
        })());
        QVERIFY(isKeyframe(h.mediaTransport->displays.constFirst()));
        h.finish();
    }

private:
    // `seconds` virtual seconds of sound only: per second, 25 audio packets,
    // one metrics sample reporting 25 encoded packets a second, and the
    // slice's new S-meter reading at the app; never a display datagram, and
    // no display endpoint or source running at the Core.
    static void verifySoundOnlySeconds(Harness& h, const QSignalSpy& samples, int seconds)
    {
        SliceModel* const stationSlice = h.radio.sliceById(h.sliceId);
        QVERIFY(stationSlice != nullptr);
        QTRY_VERIFY(h.remote.sliceById(h.sliceId) != nullptr);
        SliceModel* const appSlice = h.remote.sliceById(h.sliceId);
        h.mediaTransport->displays.clear();
        for (int second = 1; second <= seconds; ++second) {
            const qsizetype packetsBefore = h.mediaTransport->rtpPackets.size();
            const qsizetype samplesBefore = samples.count();
            h.runOneSecond();
            QTRY_COMPARE(h.mediaTransport->rtpPackets.size(),
                         packetsBefore + kAudioPacketsPerSecond);

            const double reading = -90.0 + second;
            stationSlice->setSignalStrengthDbm(reading);
            QTRY_COMPARE(appSlice->signalStrengthDbm(), reading);

            h.telemetry.sampleNow();
            QTRY_COMPARE(samples.count(), samplesBefore + 1);
            const StationTelemetrySnapshot sample = lastSnapshot(samples);
            QVERIFY(sample.audio.active);
            QVERIFY(sample.audio.encodedPacketsPerSecond.has_value());
            QCOMPARE(*sample.audio.encodedPacketsPerSecond, double(kAudioPacketsPerSecond));

            QVERIFY2(h.mediaTransport->displays.isEmpty(),
                     "a display datagram reached a sound-only session");
            QCOMPARE(h.controller.activeEndpointCount(), 0);
            QCOMPARE(h.controller.activeSourceCount(), 0);
        }
    }
};

QTEST_MAIN(TstSoundOnlySession)
#include "tst_sound_only_session.moc"

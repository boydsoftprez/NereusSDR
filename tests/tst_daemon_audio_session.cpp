// =================================================================
// tests/tst_daemon_audio_session.cpp  (NereusSDR)
// =================================================================
// Authenticated daemon audio control and RTP forwarding over the real station
// AudioEngine/MasterMixer path.  The transport is deliberately fake only at
// the network boundary.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/HpsdrModel.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/DaemonAudioSource.h"
#include "core/session/media/IMediaTransport.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "models/RadioModel.h"

#include <QPointer>
#include <QTemporaryDir>
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

namespace {

constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";
constexpr int kDspFrames = 64;

class FakeTransport final : public IMediaTransport {
public:
    explicit FakeTransport(QObject* parent = nullptr) : IMediaTransport(parent) {}

    bool start(const StartOptions& options) override { startOptions = options; started = true; return true; }
    void stop() override { started = readyState = false; }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray&) override { return readyState; }
    bool sendRtp(const QByteArray& packet) override
    {
        ++rtpAttempts;
        if (closeOnNextRtp) {
            // This signal can synchronously destroy the owning MediaPeer.
            // Do not touch fixture state after emitting it.
            emit closed();
            return false;
        }
        if (!readyState || !acceptRtp) { return false; }
        rtpPackets.append(packet);
        return true;
    }
    bool isReady() const override { return readyState; }
    void becomeReady() { readyState = true; emit ready(); }

    bool started{false};
    bool readyState{false};
    StartOptions startOptions{Role::Answerer, 0};
    QList<QByteArray> rtpPackets;
    int rtpAttempts{0};
    bool acceptRtp{true};
    bool closeOnNextRtp{false};
};

QVector<float> stereoBlock(float left, float right)
{
    QVector<float> block(kDspFrames * 2);
    for (int frame = 0; frame < kDspFrames; ++frame) {
        block[frame * 2] = left;
        block[frame * 2 + 1] = right;
    }
    return block;
}

QJsonObject audioControl(quint32 revision, bool enabled)
{
    return {{QStringLiteral("op"), QStringLiteral("audio")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("revision"), static_cast<qint64>(revision)},
            {QStringLiteral("enabled"), enabled}};
}

QJsonObject latestAudioContext(const QSignalSpy& controls)
{
    for (auto it = controls.crbegin(); it != controls.crend(); ++it) {
        const QJsonObject message = it->at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == QLatin1String("audio-context")) {
            return message;
        }
    }
    return {};
}

struct Harness {
    QTemporaryDir directory;
    AppSettings settings;
    RadioModel radio;
    StationServer server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy settingsProxy;
    StationClient client{&remote, &settingsProxy};
    QPointer<FakeTransport> mediaTransport;
    DaemonMediaController controller;
    AudioEngine* engine{nullptr};
    int sliceA{-1};
    int sliceB{-1};

    Harness()
        : settings(directory.filePath(QStringLiteral("station.settings")))
        , server(&radio, settings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()))
        , controller(&server, &radio, nullptr,
                     [this](QObject* parent) -> IMediaTransport* {
                         mediaTransport = new FakeTransport(parent);
                         return mediaTransport;
                     })
    {
        Q_ASSERT(directory.isValid());
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        engine = radio.audioEngine();
        Q_ASSERT(engine != nullptr);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        sliceA = radio.addSlice();
        sliceB = radio.addSlice();
        Q_ASSERT(sliceA >= 0 && sliceB >= 0);
        engine->setSliceStreaming(sliceA, true);
        engine->setSliceStreaming(sliceB, true);
        server.setMediaEnabled(true);
    }

    void establishAndReady()
    {
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, server.token());
        server.acceptTransport(stationLink);
        QTRY_VERIFY(server.mediaAvailable());
        // R-R3-49: sendMediaControl() needs this client's side of the
        // handshake, which lands a queued delivery after the Core's.
        QTRY_VERIFY(client.mediaAvailable());
        QVERIFY(client.sendMediaControl({
            {QStringLiteral("op"), QStringLiteral("start")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}},
            client.sessionEpoch()));
        QTRY_VERIFY(mediaTransport);
        mediaTransport->becomeReady();
    }

    void feedMixed(int frames, float a = 0.20f, float b = 0.30f)
    {
        const QVector<float> left = stereoBlock(a, a);
        const QVector<float> right = stereoBlock(b, b);
        for (int delivered = 0; delivered < frames; delivered += kDspFrames) {
            engine->rxBlockReady(sliceA, left.constData(), kDspFrames);
            engine->rxBlockReady(sliceB, right.constData(), kDspFrames);
        }
    }
};

} // namespace

class TstDaemonAudioSession final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OpusAudioEncoder encoder;
        if (!encoder.isReady()) {
            QSKIP("Opus encoder is unavailable in this build");
        }
    }

    void enabledPauseResumePublishesFreshContextsAndContinuousRtpBases()
    {
        Harness h;
        h.establishAndReady();
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QVERIFY(h.client.sendMediaControl(audioControl(1, true), h.client.sessionEpoch()));
        QTRY_VERIFY(!latestAudioContext(controls).isEmpty());
        const QJsonObject enabled = latestAudioContext(controls);
        // A minor-8 session: the eight keys plus the encoder profile.
        QCOMPARE(enabled.size(), 9);
        QVERIFY(enabled.value(QStringLiteral("encoder")).isObject());
        QVERIFY(!enabled.contains(QStringLiteral("reason")));
        QCOMPARE(enabled.value(QStringLiteral("revision")).toInteger(), qint64{1});
        QVERIFY(enabled.value(QStringLiteral("enabled")).toBool());
        QCOMPARE(static_cast<quint32>(enabled.value(QStringLiteral("ssrc")).toInteger()),
                 h.mediaTransport->startOptions.localAudioSsrc);

        h.feedMixed(DaemonAudioSource::kBlockFrames);
        QTRY_COMPARE(h.mediaTransport->rtpPackets.size(), 1);
        OpusAudioDecoder decoder;
        const auto first = decoder.decodeRtp(h.mediaTransport->rtpPackets.constFirst(),
                                             h.mediaTransport->startOptions.localAudioSsrc);
        QCOMPARE(first.status, OpusAudioCodecStatus::Accepted);

        QVERIFY(h.client.sendMediaControl(audioControl(2, false), h.client.sessionEpoch()));
        QTRY_VERIFY(latestAudioContext(controls).value(QStringLiteral("revision")).toInteger() == 2);
        const QJsonObject paused = latestAudioContext(controls);
        QVERIFY(!paused.value(QStringLiteral("enabled")).toBool());
        QCOMPARE(paused.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("client-disabled"));
        QCOMPARE(paused.value(QStringLiteral("firstSequence")).toInteger(),
                 qint64{static_cast<quint16>(first.sequence + 1)});
        QCOMPARE(paused.value(QStringLiteral("firstTimestamp")).toInteger(),
                 qint64{first.timestamp + DaemonAudioSource::kBlockFrames});

        QVERIFY(h.client.sendMediaControl(audioControl(3, true), h.client.sessionEpoch()));
        QTRY_VERIFY(latestAudioContext(controls).value(QStringLiteral("revision")).toInteger() == 3);
        const QJsonObject resumed = latestAudioContext(controls);
        QVERIFY(resumed.value(QStringLiteral("enabled")).toBool());
        QCOMPARE(resumed.value(QStringLiteral("firstSequence")).toInteger(),
                 paused.value(QStringLiteral("firstSequence")).toInteger());
        QCOMPARE(resumed.value(QStringLiteral("firstTimestamp")).toInteger(),
                 paused.value(QStringLiteral("firstTimestamp")).toInteger());
        h.feedMixed(DaemonAudioSource::kBlockFrames);
        QTRY_COMPARE(h.mediaTransport->rtpPackets.size(), 2);
        const auto second = decoder.decodeRtp(h.mediaTransport->rtpPackets.constLast(),
                                              h.mediaTransport->startOptions.localAudioSsrc);
        QCOMPARE(second.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(second.sequence, static_cast<quint16>(first.sequence + 1));
        QCOMPARE(second.timestamp, static_cast<quint32>(first.timestamp
                                                          + DaemonAudioSource::kBlockFrames));
    }

    void rejectsMalformedOrStaleAudioAndReconcilesRadioDisconnect()
    {
        Harness h;
        h.establishAndReady();
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QVERIFY(h.client.sendMediaControl(audioControl(5, true), h.client.sessionEpoch()));
        QTRY_VERIFY(!latestAudioContext(controls).isEmpty());
        const int acceptedContexts = controls.count();

        QJsonObject malformed = audioControl(6, false);
        malformed.insert(QStringLiteral("extra"), true);
        QVERIFY(h.client.sendMediaControl(malformed, h.client.sessionEpoch()));
        QVERIFY(h.client.sendMediaControl(audioControl(5, false), h.client.sessionEpoch()));
        QTest::qWait(20);
        QCOMPARE(controls.count(), acceptedContexts);
        QVERIFY(!h.client.sendMediaControl(audioControl(7, false), h.client.sessionEpoch() + 1));

        h.radio.setConnectionStateForTest(ConnectionState::LinkLost);
        QTRY_VERIFY(!latestAudioContext(controls).value(QStringLiteral("enabled")).toBool());
        const QJsonObject disconnected = latestAudioContext(controls);
        QCOMPARE(disconnected.value(QStringLiteral("revision")).toInteger(), qint64{5});
        QCOMPARE(disconnected.value(QStringLiteral("reason")).toString(),
                 QStringLiteral("radio-offline"));
        h.feedMixed(DaemonAudioSource::kBlockFrames);
        QTest::qWait(20);
        QVERIFY(h.mediaTransport->rtpPackets.isEmpty());
        h.radio.setConnectionStateForTest(ConnectionState::Disconnected);
        QTRY_VERIFY(!latestAudioContext(controls).value(QStringLiteral("enabled")).toBool());

        h.radio.setConnectionStateForTest(ConnectionState::Connected);
        QTRY_VERIFY(latestAudioContext(controls).value(QStringLiteral("enabled")).toBool());
        const QJsonObject reconnected = latestAudioContext(controls);
        QVERIFY(reconnected.value(QStringLiteral("generation")).toInteger()
                > disconnected.value(QStringLiteral("generation")).toInteger());
    }

    void audioDiagnosticsCountTransportAcceptanceAndKeepFinalSnapshot()
    {
        Harness h;
        h.establishAndReady();
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QVERIFY(h.client.sendMediaControl(audioControl(1, true), h.client.sessionEpoch()));
        QTRY_VERIFY(!latestAudioContext(controls).isEmpty());
        const QJsonObject context = latestAudioContext(controls);

        h.feedMixed(DaemonAudioSource::kBlockFrames);
        QTRY_VERIFY(h.controller.audioDiagnostics().sendAccepted == std::uint64_t{1});
        const auto accepted = h.controller.audioDiagnostics();
        QVERIFY(accepted.activeContext);
        QCOMPARE(accepted.contextGeneration,
                 static_cast<quint32>(context.value(QStringLiteral("generation")).toInteger()));
        QCOMPARE(accepted.revision, quint32{1});
        QCOMPARE(accepted.sender.source.capturedValidRateFrames,
                 std::uint64_t{DaemonAudioSource::kBlockFrames});
        QCOMPARE(accepted.sender.source.sourceDropEvents, std::uint64_t{0});
        QCOMPARE(accepted.sender.consumedBlocks, std::uint64_t{1});
        QCOMPARE(accepted.sender.encodedPackets, std::uint64_t{1});
        QCOMPARE(accepted.sender.encodeFailures, std::uint64_t{0});
        QVERIFY(accepted.sender.hasLastEmittedPacket);
        QCOMPARE(accepted.sendAttempts, std::uint64_t{1});
        QCOMPARE(accepted.sendAccepted, std::uint64_t{1});
        QCOMPARE(accepted.sendRejected, std::uint64_t{0});
        QCOMPARE(accepted.sendInFlight, std::uint64_t{0});
        QCOMPARE(accepted.sendUnresolvedAtRetirement, std::uint64_t{0});
        QCOMPARE(accepted.sendAttempts, accepted.sendAccepted + accepted.sendRejected
                 + accepted.sendInFlight + accepted.sendUnresolvedAtRetirement);

        h.mediaTransport->acceptRtp = false;
        h.feedMixed(DaemonAudioSource::kBlockFrames);
        QTRY_VERIFY(h.controller.audioDiagnostics().sendRejected == std::uint64_t{1});
        const auto rejected = h.controller.audioDiagnostics();
        QCOMPARE(rejected.sendAttempts, std::uint64_t{2});
        QCOMPARE(rejected.sendAccepted, std::uint64_t{1});
        QCOMPARE(rejected.sendRejected, std::uint64_t{1});
        QCOMPARE(rejected.sendInFlight, std::uint64_t{0});
        QCOMPARE(rejected.sendUnresolvedAtRetirement, std::uint64_t{0});
        QCOMPARE(rejected.sendAttempts, rejected.sendAccepted + rejected.sendRejected
                 + rejected.sendInFlight + rejected.sendUnresolvedAtRetirement);
        QCOMPARE(h.mediaTransport->rtpAttempts, 2);
        QCOMPARE(h.mediaTransport->rtpPackets.size(), 1);

        QVERIFY(h.client.sendMediaControl(audioControl(2, false), h.client.sessionEpoch()));
        QTRY_VERIFY(!latestAudioContext(controls).value(QStringLiteral("enabled")).toBool());
        const auto stopped = h.controller.audioDiagnostics();
        QVERIFY(!stopped.activeContext);
        QCOMPARE(stopped.sendAttempts, rejected.sendAttempts);
        QCOMPARE(stopped.sendAccepted, rejected.sendAccepted);
        QCOMPARE(stopped.sendRejected, rejected.sendRejected);
        QCOMPARE(stopped.sendInFlight, std::uint64_t{0});
        QCOMPARE(stopped.sendUnresolvedAtRetirement,
                 rejected.sendUnresolvedAtRetirement);
        QCOMPARE(stopped.sendAttempts, stopped.sendAccepted + stopped.sendRejected
                 + stopped.sendInFlight + stopped.sendUnresolvedAtRetirement);
        QCOMPARE(stopped.sender.source.capturedValidRateFrames,
                 rejected.sender.source.capturedValidRateFrames);
        QCOMPARE(stopped.sender.source.sourceDropEvents,
                 rejected.sender.source.sourceDropEvents);
        QCOMPARE(stopped.sender.consumedBlocks, rejected.sender.consumedBlocks);
        QCOMPARE(stopped.sender.encodedPackets, rejected.sender.encodedPackets);
        QCOMPARE(stopped.sender.lastEmittedSequence,
                 rejected.sender.lastEmittedSequence);
        QCOMPARE(stopped.sender.lastEmittedTimestamp,
                 rejected.sender.lastEmittedTimestamp);
    }

    void audioDiagnosticsRetainAnAttemptWhenTransportRetiresPeerSynchronously()
    {
        Harness h;
        h.establishAndReady();
        QSignalSpy controls(&h.client, &StationClient::mediaControlReceived);
        QVERIFY(h.client.sendMediaControl(audioControl(1, true), h.client.sessionEpoch()));
        QTRY_VERIFY(!latestAudioContext(controls).isEmpty());
        QTRY_VERIFY(h.controller.audioDiagnostics().activeContext);

        // sendRtp() closes its MediaPeer synchronously. Keep no raw fixture
        // pointer after feeding audio: closing can delete the transport later
        // in this same event turn.
        h.mediaTransport->closeOnNextRtp = true;
        h.feedMixed(DaemonAudioSource::kBlockFrames);

        QTRY_VERIFY(!h.controller.audioDiagnostics().activeContext);
        const auto retired = h.controller.audioDiagnostics();
        // The return value was interrupted by peer retirement, so it cannot
        // be accepted or refused. It remains visible as an unresolved
        // retirement result, which is not packet loss.
        QCOMPARE(retired.sendAttempts, std::uint64_t{1});
        QCOMPARE(retired.sendAccepted, std::uint64_t{0});
        QCOMPARE(retired.sendRejected, std::uint64_t{0});
        QCOMPARE(retired.sendInFlight, std::uint64_t{0});
        QCOMPARE(retired.sendUnresolvedAtRetirement, std::uint64_t{1});
        QCOMPARE(retired.sendAttempts, retired.sendAccepted + retired.sendRejected
                 + retired.sendInFlight + retired.sendUnresolvedAtRetirement);
    }
};

QTEST_MAIN(TstDaemonAudioSession)
#include "tst_daemon_audio_session.moc"

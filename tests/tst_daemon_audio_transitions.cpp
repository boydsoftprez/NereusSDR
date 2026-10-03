// =================================================================
// tests/tst_daemon_audio_transitions.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Test of NereusSDR's own remote audio
// contract; no Thetis logic is ported here.
//
// R-R3-06, R-R3-07, R-R3-09: the 48 kHz mixed-audio contract holds through
// live station transitions. tst_daemon_audio_session drives a model whose
// connection state is set by hand, so its radio cannot change sample rate
// (RadioModel::setSampleRateLive needs a live connection and WDSP). This
// file instead stands the real daemon media controller and sender on a
// RadioModel genuinely connected to a Protocol 1 fake radio with real WDSP
// (fakes/ConnectableRadioModel.h), so I/Q flows through the real DSP
// worker, MasterMixer and master-mix tap. Only the media transport is
// fake, at the network boundary.
//
// While audio streams, each test makes one live change: radio sample rate
// 192000 -> 384000 -> 192000, the audio slice's mode SSB -> RADE -> SSB, a
// band change, and adding then removing a second receive slice. Every RTP
// packet the controller sends must be 48 kHz stereo Opus carrying 1920
// frames, the sequence must stay contiguous, and every timestamp must sit
// on the 1920-frame grid of the context that announced it. A timestamp may
// step by more than one packet only for a packet the capture bridge
// genuinely lost, which its telemetry counts.
//
// None of these transitions requires a new audio context: the Core's
// output is the post-mixer 48 kHz master whatever the radio rate, demod
// mode, band or slice count, and the same MediaPeer, SSRC and RTP
// timeline carry on. The tests therefore also pin that the GUI side
// receives exactly the one context it enabled, so it never has an older
// context to play from.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/RadeChannel.h"
#include "core/WdspEngine.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonAudioSource.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/IMediaTransport.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/ConnectableRadioModel.h"
#include "fakes/LoopbackTransport.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QPointer>
#include <QThread>
#include <QTemporaryDir>

#include <memory>
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;

namespace {

constexpr char kConnectionId[] = "11111111-2222-4333-8444-555555555555";

// Packets required after each change before the next one is made. Three
// packets are 120 ms of audio: enough to show the stream resumed and
// stayed on its grid, short enough to keep the test brisk.
constexpr int kPacketsPerPhase = 3;

// P1FakeRadio frames injected per pump step on top of its own 10 ms
// cadence. One ep6 frame carries 126 I/Q samples (P1FakeRadio.cpp,
// buildEp6Frame), so eight frames are 1008 samples: 5.25 ms of radio time
// at 192 kHz. Pumping one step per 5 ms wait keeps the station near real
// time, well inside DaemonAudioSource's 160 ms ring, so a ring overflow
// here would be a real defect rather than test pressure.
constexpr int kEp6FramesPerStep = 8;
constexpr int kPumpStepMs = 5;

// Generous ceiling for one phase. RADE entry is the slowest: the decoder
// must emit its first speech block before its slice rejoins the mix.
constexpr int kPhaseTimeoutMs = 20000;

// A genuine capture loss. The sender drains the capture ring on the main
// thread, so holding the main thread while the DSP thread mixes a burst
// overflows the ring (DaemonAudioSource::kQueueBlocks, 160 ms) for real.
// The burst goes out in steps of 50 ep6 frames every 10 ms, so no single
// write outruns the loopback socket: 20 steps are 1000 frames, 126000 I/Q
// samples, 31500 frames of 48 kHz audio at 192 kHz, about sixteen packets
// against a ring of four. The closing stall is three times the ring's
// 160 ms, so the burst is mixed before the sender next runs.
constexpr int kLossBurstSteps = 20;
constexpr int kLossBurstEp6FramesPerStep = 50;
constexpr unsigned long kLossBurstStepMs = 10;
constexpr unsigned long kLossStallMs =
    3UL * DaemonAudioSource::kQueueBlocks * 40UL;

class FakeTransport final : public IMediaTransport {
public:
    explicit FakeTransport(QObject* parent = nullptr) : IMediaTransport(parent) {}

    bool start(const StartOptions& options) override { startOptions = options; return true; }
    void stop() override { readyState = false; }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray&) override { return readyState; }
    bool sendRtp(const QByteArray& packet) override
    {
        if (!readyState) { return false; }
        rtpPackets.append(packet);
        return true;
    }
    bool isReady() const override { return readyState; }
    void becomeReady() { readyState = true; emit ready(); }

    bool readyState{false};
    StartOptions startOptions{Role::Answerer, 0};
    QList<QByteArray> rtpPackets;
};

QJsonObject audioControl(quint32 revision, bool enabled)
{
    return {{QStringLiteral("op"), QStringLiteral("audio")},
            {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
            {QStringLiteral("revision"), static_cast<qint64>(revision)},
            {QStringLiteral("enabled"), enabled}};
}

QList<QJsonObject> audioContexts(const QSignalSpy& controls)
{
    QList<QJsonObject> contexts;
    for (const QList<QVariant>& call : controls) {
        const QJsonObject message = call.at(0).toJsonObject();
        if (message.value(QStringLiteral("op")) == QLatin1String("audio-context")) {
            contexts.append(message);
        }
    }
    return contexts;
}

struct TransitionHarness {
    QTemporaryDir directory;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<ConnectableRadioModel> radio;
    std::unique_ptr<StationServer> server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy settingsProxy;
    std::unique_ptr<StationClient> client;
    QPointer<FakeTransport> mediaTransport;
    std::unique_ptr<DaemonMediaController> controller;
    std::unique_ptr<QSignalSpy> controls;
    QJsonObject context;

    ~TransitionHarness()
    {
        // Retire the media session before the station model it taps.
        controls.reset();
        controller.reset();
        client.reset();
        server.reset();
        radio.reset();
    }

    RadioModel& model() { return radio->model(); }

    // Pumps the fake radio until `more` further RTP packets have been sent.
    bool pumpPackets(int more)
    {
        if (!mediaTransport) { return false; }
        const int target = mediaTransport->rtpPackets.size() + more;
        QElapsedTimer clock;
        clock.start();
        while (mediaTransport && mediaTransport->rtpPackets.size() < target
               && clock.elapsed() < kPhaseTimeoutMs) {
            radio->fake().sendEp6Frames(kEp6FramesPerStep);
            QTest::qWait(kPumpStepMs);
        }
        return mediaTransport && mediaTransport->rtpPackets.size() >= target;
    }
};

// Connects the station at 192 kHz, establishes a ready media peer and
// enables audio, leaving one enabled context and a flowing stream.
void establishStreaming(TransitionHarness& h)
{
    QVERIFY(h.directory.isValid());
    h.radio = ConnectableRadioModel::create();
    QVERIFY(h.radio != nullptr);
    RadioModel& model = h.model();
    if (model.connectionSampleRateHz() != 192000) {
        QVERIFY(model.setSampleRateLive(192000) >= 0);
    }
    QCOMPARE(model.connectionSampleRateHz(), 192000);

    h.settings = std::make_unique<AppSettings>(
        h.directory.filePath(QStringLiteral("station.settings")));
    h.server = std::make_unique<StationServer>(&model, *h.settings, NereusSDR::Test::seedUpgradedCoreToken(h.directory.path()));
    h.client = std::make_unique<StationClient>(&h.remote, &h.settingsProxy);
    h.controller = std::make_unique<DaemonMediaController>(
        h.server.get(), &model, nullptr,
        [&h](QObject* parent) -> IMediaTransport* {
            h.mediaTransport = new FakeTransport(parent);
            return h.mediaTransport;
        });
    h.server->setMediaEnabled(true);

    auto* stationLink = new Test::LoopbackTransport(QStringLiteral("station"));
    auto* clientLink = new Test::LoopbackTransport(QStringLiteral("client"));
    stationLink->linkTo(clientLink);
    h.client->startSession(clientLink, h.server->token());
    h.server->acceptTransport(stationLink);
    QTRY_VERIFY(h.server->mediaAvailable());
    // R-R3-49: the Core is ready once it has sent snapshot.complete, this
    // client once it has read it (a queued delivery later), and
    // sendMediaControl() needs the client's side; on a busy computer the
    // wait above stopped between the two.
    QTRY_VERIFY(h.client->mediaAvailable());
    QVERIFY(h.client->sendMediaControl({
        {QStringLiteral("op"), QStringLiteral("start")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)}},
        h.client->sessionEpoch()));
    QTRY_VERIFY(h.mediaTransport);
    h.mediaTransport->becomeReady();

    h.controls = std::make_unique<QSignalSpy>(h.client.get(),
                                              &StationClient::mediaControlReceived);
    QVERIFY(h.client->sendMediaControl(audioControl(1, true), h.client->sessionEpoch()));
    QTRY_COMPARE(audioContexts(*h.controls).size(), 1);
    h.context = audioContexts(*h.controls).constFirst();
    QVERIFY(h.context.value(QStringLiteral("enabled")).toBool());
    QVERIFY2(h.pumpPackets(kPacketsPerPhase), "no mixed audio before the transition");
}

// The whole stream so far, checked against the one enabled context.
void verifyContract(TransitionHarness& h, const char* phase,
                    quint64* skippedOut = nullptr, quint64* dropsOut = nullptr)
{
    QVERIFY(h.mediaTransport);
    const quint32 ssrc = h.mediaTransport->startOptions.localAudioSsrc;
    QCOMPARE(static_cast<quint32>(h.context.value(QStringLiteral("ssrc")).toInteger()), ssrc);
    const auto firstSequence =
        static_cast<quint16>(h.context.value(QStringLiteral("firstSequence")).toInteger());
    const auto firstTimestamp =
        static_cast<quint32>(h.context.value(QStringLiteral("firstTimestamp")).toInteger());

    // Exactly one context: no transition retired the one the GUI enabled.
    const QList<QJsonObject> contexts = audioContexts(*h.controls);
    QVERIFY2(contexts.size() == 1,
             qPrintable(QStringLiteral("%1: %2 audio contexts, expected 1")
                            .arg(QLatin1String(phase)).arg(contexts.size())));
    const DaemonAudioDiagnostics diagnostics = h.controller->audioDiagnostics();
    QVERIFY(diagnostics.activeContext);
    QCOMPARE(diagnostics.contextGeneration,
             static_cast<quint32>(h.context.value(QStringLiteral("generation")).toInteger()));

    OpusAudioDecoder decoder;
    QVERIFY(decoder.isReady());
    quint64 skippedPackets = 0;
    quint16 expectedSequence = firstSequence;
    quint32 previousTimestamp = 0;
    const QList<QByteArray>& packets = h.mediaTransport->rtpPackets;
    for (int i = 0; i < packets.size(); ++i) {
        const OpusRtpDecodeResult decoded = decoder.decodeRtp(packets.at(i), ssrc);
        const QString where = QStringLiteral("%1: packet %2").arg(QLatin1String(phase)).arg(i);
        QVERIFY2(decoded.status == OpusAudioCodecStatus::Accepted, qPrintable(where));
        QVERIFY2(decoded.packetInfo.channels == DaemonAudioSource::kChannels, qPrintable(where));
        QVERIFY2(decoded.packetInfo.samplesPerChannel == DaemonAudioSource::kBlockFrames,
                 qPrintable(where));
        QVERIFY2(decoded.pcmInterleaved.size() == DaemonAudioSource::kBlockSamples,
                 qPrintable(where));
        QVERIFY2(decoded.sequence == expectedSequence,
                 qPrintable(QStringLiteral("%1 sequence %2, expected %3")
                                .arg(where).arg(decoded.sequence).arg(expectedSequence)));
        ++expectedSequence;

        const quint32 fromBase = decoded.timestamp - firstTimestamp;
        QVERIFY2(fromBase % DaemonAudioSource::kBlockFrames == 0,
                 qPrintable(QStringLiteral("%1 timestamp %2 is off the grid of base %3")
                                .arg(where).arg(decoded.timestamp).arg(firstTimestamp)));
        const quint32 step = i == 0 ? fromBase + DaemonAudioSource::kBlockFrames
                                    : decoded.timestamp - previousTimestamp;
        QVERIFY2(step >= static_cast<quint32>(DaemonAudioSource::kBlockFrames)
                     && step % DaemonAudioSource::kBlockFrames == 0,
                 qPrintable(QStringLiteral("%1 timestamp step %2").arg(where).arg(step)));
        skippedPackets += step / DaemonAudioSource::kBlockFrames - 1;
        previousTimestamp = decoded.timestamp;
    }

    // A timestamp that skips a packet must stand for a packet the capture
    // bridge really lost. Losses after the last sent packet are not yet
    // visible as a gap, hence at most rather than equal.
    const DaemonAudioSourceTelemetry source = diagnostics.sender.source;
    QVERIFY2(skippedPackets <= source.sourceDropEvents,
             qPrintable(QStringLiteral("%1: %2 skipped packets but only %3 source drops")
                            .arg(QLatin1String(phase)).arg(skippedPackets)
                            .arg(source.sourceDropEvents)));
    QCOMPARE(diagnostics.sender.encodeFailures, std::uint64_t{0});
    if (skippedOut) { *skippedOut = skippedPackets; }
    if (dropsOut) { *dropsOut = source.sourceDropEvents; }
}

} // namespace

class TstDaemonAudioTransitions final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OpusAudioEncoder encoder;
        if (!encoder.isReady()) {
            QSKIP("Opus encoder is unavailable in this build");
        }
    }

    void radioSampleRateChangeKeepsTheMixedStreamOnItsGrid()
    {
        TransitionHarness h;
        establishStreaming(h);
        if (QTest::currentTestFailed()) { return; }

        QVERIFY(h.model().setSampleRateLive(384000) >= 0);
        QCOMPARE(h.model().connectionSampleRateHz(), 384000);
        QVERIFY2(h.pumpPackets(kPacketsPerPhase), "no audio at 384000 Hz");
        verifyContract(h, "at 384000 Hz");
        if (QTest::currentTestFailed()) { return; }

        QVERIFY(h.model().setSampleRateLive(192000) >= 0);
        QCOMPARE(h.model().connectionSampleRateHz(), 192000);
        QVERIFY2(h.pumpPackets(kPacketsPerPhase), "no audio back at 192000 Hz");
        verifyContract(h, "back at 192000 Hz");
        if (QTest::currentTestFailed()) { return; }

        // A packet genuinely lost after the rate change still advances the
        // timestamp by 1920 while the sequence stays contiguous.
        for (int step = 0; step < kLossBurstSteps; ++step) {
            h.radio->fake().sendEp6Frames(kLossBurstEp6FramesPerStep);
            QThread::msleep(kLossBurstStepMs);
        }
        QThread::msleep(kLossStallMs);
        // The ring's queued packets drain first; the ones after them are
        // new audio from beyond the loss, which makes the gap visible.
        QVERIFY2(h.pumpPackets(DaemonAudioSource::kQueueBlocks + kPacketsPerPhase),
                 "no audio after the capture loss");
        quint64 skipped = 0;
        quint64 drops = 0;
        verifyContract(h, "after a capture loss", &skipped, &drops);
        if (QTest::currentTestFailed()) { return; }
        QVERIFY2(drops > 0, "test setup: the stalled burst lost no capture packets");
        // Every loss happened in the burst, before the packets pumped after
        // it, so each one is now visible as exactly one skipped packet.
        QCOMPARE(skipped, drops);
    }

    void audioSliceModeSsbToRadeAndBackKeepsTheStreamOnItsGrid()
    {
        TransitionHarness h;
        establishStreaming(h);
        if (QTest::currentTestFailed()) { return; }

        SliceModel* slice = h.model().activeSlice();
        QVERIFY(slice != nullptr);
        const DSPMode ssb = slice->dspMode();
        QVERIFY2(ssb == DSPMode::USB || ssb == DSPMode::LSB,
                 "test setup: the connected slice should start in SSB");

        slice->setDspMode(DSPMode::RADE_U);
        QCOMPARE(slice->dspMode(), DSPMode::RADE_U);
        RadeChannel* rade = h.model().wdspEngine()->radeChannel(slice->sliceIndex());
        QVERIFY2(rade != nullptr, "test setup: RADE mode created no decoder");
        QSignalSpy speech(rade, &RadeChannel::rxSpeechReady);
        QVERIFY2(h.pumpPackets(kPacketsPerPhase), "no audio in RADE");
        // RADE threads: the slice stays in the mix on the DSP thread's
        // cadence, playing its decoder's speech (silence while it warms),
        // so these packets carry the RADE path's audio.
        QVERIFY(speech.count() > 0);
        verifyContract(h, "in RADE");
        if (QTest::currentTestFailed()) { return; }

        slice->setDspMode(ssb);
        QCOMPARE(slice->dspMode(), ssb);
        QVERIFY2(h.pumpPackets(kPacketsPerPhase), "no audio back in SSB");
        verifyContract(h, "back in SSB");
    }

    void bandChangeKeepsTheStreamOnItsGrid()
    {
        TransitionHarness h;
        establishStreaming(h);
        if (QTest::currentTestFailed()) { return; }

        SliceModel* slice = h.model().activeSlice();
        QVERIFY(slice != nullptr);
        const Band before = bandFromFrequency(slice->frequency());
        const Band target = before == Band::Band20m ? Band::Band40m : Band::Band20m;
        h.model().onBandButtonClicked(target);
        QCOMPARE(bandFromFrequency(slice->frequency()), target);
        QVERIFY2(h.pumpPackets(kPacketsPerPhase), "no audio after the band change");
        verifyContract(h, "after the band change");
    }

    void addingAndRemovingASecondSliceKeepsTheStreamOnItsGrid()
    {
        TransitionHarness h;
        establishStreaming(h);
        if (QTest::currentTestFailed()) { return; }

        const int second = h.model().addSlice(QStringLiteral("pan-b"));
        QVERIFY(second >= 0);
        QVERIFY(h.model().sliceById(second) != nullptr);
        QVERIFY2(h.pumpPackets(kPacketsPerPhase), "no audio with two slices");
        // Both receivers really are in the mix, not only the first.
        QCOMPARE(h.model().audioEngine()->masterMixForTest().producingSliceCount(), 2);
        verifyContract(h, "with two slices");
        if (QTest::currentTestFailed()) { return; }

        h.model().removeSlice(second);
        QVERIFY(h.model().sliceById(second) == nullptr);
        QVERIFY2(h.pumpPackets(kPacketsPerPhase), "no audio after removing the slice");
        QCOMPARE(h.model().audioEngine()->masterMixForTest().producingSliceCount(), 1);
        verifyContract(h, "after removing the slice");
    }
};

QTEST_MAIN(TstDaemonAudioTransitions)
#include "tst_daemon_audio_transitions.moc"

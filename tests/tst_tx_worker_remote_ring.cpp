// =================================================================
// tests/tst_tx_worker_remote_ring.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.
//
// iPhone app plan Task 36 (R-IOS-13): the remote microphone at the Core's
// transmitter.
//
//   The pump: while the ring is in use its audio replaces the operator's
//   source in the normal path and in the RADE path, and a tone from the
//   microphone line reaches the TX channel at its level; out of use the
//   operator's source returns and nothing of the ring leaks.
//
//   The Core: a media start carrying remoteTxVersion gets the microphone
//   line (a start without it, and a peer the Core never told remoteTx, get
//   none); a tx.key in a voice mode keys once the buffer holds 30 ms and is
//   refused micNotReady when no audio comes within 250 ms; TUNE keys at
//   once; the ring is in use only while the device is keyed (or its key
//   waits) or has VOX armed, and at unkey the operator's source returns
//   with the ring empty; a VOX key from the device's microphone is the
//   device's, shown as VOX.
//
//   The monitor: with MON on and keyed, the audio the Core sends a remote
//   device carries the transmit monitor at the level the local speakers
//   play it.
//
// No test opens a real audio device or keys a real radio: the TX channel
// talks to a mock connection, and the Core's radio is the static test
// radio. The pump is driven block by block, as the radio's microphone
// frames drive it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original test for NereusSDR by J.J. Boyd (KG4VCF), iPhone
//               app plan Task 36 (R-IOS-13), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C1: a key without the microphone
//               line is refused and nothing keys. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-26: Transmit group fix wave C2: two devices streaming,
//               teardown, keepalive attribution and a media restart. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2, the re-review's minors:
//               holderTransferring true while keys are refused for a
//               transfer's reasons (a dropped holder's fence, a transfer
//               ended with MOX on); stopEpoch names the key a stop ended so
//               a newer key is never ended by it; VOX at the Core listens
//               only to the device that armed it; the window says why MOX
//               and TUNE wait while another device holds. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: R-IOS-13, R-R3-42: a standing send ring is shed only in
//               silence, by the pump skipping whole blocks; the key waits
//               for the smaller 30 ms target. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: slice control and shared listening plan Task 2:
//               mediaSessionOwnsSlice is mediaSessionControlsSlice.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-09-30: TX stall lane: every unkey through MoxController logs the
//               microphone line's figures once, with the underruns of
//               the over. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-10-01: TX stall lane: the line names the device in hex. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-10-01: TX mic thread fix round 2: the unkey line's "line waits"
//               and each over's own longest "tx" keepalive wait. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane: the unkey line is told from the event
//               lines that now follow it (they never carry "transmit I/Q"),
//               and the first over's underrun is placed in one. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/PttSource.h"
#include "core/RadioConnection.h"
#include "core/RadioStatus.h"
#include "core/TxChannel.h"
#include "core/TxWorkerThread.h"
#include "core/audio/TxMicSource.h"
#include "core/session/RemoteKeying.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/session/media/MediaPeer.h"
#include "core/session/media/RemoteMicReceiver.h"

#include "fakes/FakeAudioBus.h"

#include <QElapsedTimer>
#include <QScopeGuard>
#include <QStandardPaths>

#include <atomic>
#include <cmath>
#include <numbers>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr int kBlock = TxWorkerThread::kBlockFrames;
constexpr int kChannelId = 1;
constexpr char kConnectionId[] = "3f2504e0-4f89-41d3-9a0c-0305e82c3301";

class MockConnection : public RadioConnection {
public:
    explicit MockConnection(QObject* parent = nullptr)
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
    void sendTxIq(const float*, int) override { sent.fetch_add(1); }
    // R-IOS-13: what the send ring holds, as the remote microphone's
    // buffer reads it (negative: unknown).
    double txIqQueuedMs() const override { return queuedMs; }
    std::atomic<int> sent{0};
    double queuedMs{-1.0};
};

std::vector<float> tone(qint64 start, int frames, float amplitude, double hz = 1000.0)
{
    std::vector<float> out(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        const double t = static_cast<double>(start + i) / 48000.0;
        out[static_cast<size_t>(i)] =
            amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * t));
    }
    return out;
}

double rms(const std::vector<double>& samples, size_t from = 0)
{
    double sum = 0.0;
    for (size_t i = from; i < samples.size(); ++i) {
        sum += samples[i] * samples[i];
    }
    return samples.size() > from ? std::sqrt(sum / static_cast<double>(samples.size() - from)) : 0.0;
}

// The I channel the TX channel was last given.
std::vector<double> lastI(const TxChannel& channel)
{
    std::vector<double> i;
    const std::vector<double>& in = channel.inForTest();
    for (size_t k = 0; k < in.size(); k += 2) {
        i.push_back(in[k]);
    }
    return i;
}

// The pump with a TX channel on a mock connection, the PC microphone
// selected at 0.7, and the remote ring attached.
struct Pump {
    AudioEngine engine;
    TxChannel channel{kChannelId, kBlock, kBlock};
    MockConnection connection;
    RemoteMicFeed feed;
    TxWorkerThread worker;
    FakeAudioBus* pcMic{nullptr};

    Pump()
    {
        AudioFormat fmt{};
        fmt.sample = AudioFormat::Sample::Float32;
        fmt.channels = 2;
        fmt.sampleRate = 48000;
        auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeMic"));
        bus->open(fmt);
        pcMic = bus.get();
        engine.setTxInputBusForTest(std::move(bus));
        engine.onMicSourceChanged(/*selectedSourceIsPc=*/true);
        channel.setConnection(&connection);
        channel.setRunning(true);
        worker.setTxChannel(&channel);
        worker.setAudioEngine(&engine);
        worker.setRemoteMicFeed(&feed);
    }

    // One pump block with the PC microphone at 0.7 and the radio's at 0.3.
    void block()
    {
        QByteArray pcm(kBlock * 2 * 4, Qt::Uninitialized);
        auto* p = reinterpret_cast<float*>(pcm.data());
        for (int i = 0; i < kBlock * 2; ++i) {
            p[i] = 0.7f;
        }
        pcMic->setPullData(pcm);
        const std::vector<float> radio(kBlock, 0.3f);
        worker.dispatchBlockForTest(radio.data());
    }
};

// A test MasterMixAudioTap: what the Core would send a remote device.
class ProgramTap final : public MasterMixAudioTap {
public:
    void consume(const float* samples, int frames, int) noexcept override
    {
        heard.insert(heard.end(), samples, samples + frames * 2);
    }
    std::vector<float> heard;
};

// A media transport that records its start and lets the test deliver the
// microphone line's packets.
class MicTransport final : public IMediaTransport {
public:
    explicit MicTransport(QObject* parent) : IMediaTransport(parent) {}
    bool start(const StartOptions& o) override
    {
        options = o;
        started = true;
        return true;
    }
    void stop() override { started = false; }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray&) override { return started; }
    DisplaySendResult submitIq(const QByteArray&) override
    { return stallIq ? DisplaySendResult::Busy : DisplaySendResult::Sent; }
    bool iqBusy() const override { return stallIq; }
    bool sendRtp(const QByteArray&) override { return started; }
    bool isReady() const override { return started && readyState; }
    void becomeReady()
    {
        readyState = true;
        emit ready();
    }
    void deliverMic(const QByteArray& packet) { emit micRtpReceived(packet); }
    void deliverTx(const QByteArray& message, qint64 heldUs = 0)
    {
        emit txReceived(message, heldUs);
    }
    void closeUnexpectedly()
    {
        started = false;
        emit closed();
    }
    StartOptions options{Role::Offerer, 0};
    bool started{false};
    bool readyState{false};
    bool stallIq{false};
};

QString mediaStart(bool remoteTx, const QString& connectionId = QLatin1String(kConnectionId),
                   bool iq = false)
{
    QJsonObject payload{{QStringLiteral("op"), QStringLiteral("start")},
                        {QStringLiteral("connectionId"), connectionId}};
    if (remoteTx) {
        payload.insert(QStringLiteral("remoteTxVersion"), 1);
    }
    if (iq) { payload.insert(QStringLiteral("remoteIqVersion"), 1); }
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("type"), QStringLiteral("media.control")},
        {QStringLiteral("payload"), payload}}).toJson(QJsonDocument::Compact));
}

void sendCommand(LoopbackTransport* app, const QByteArray& verb, quint32 id,
                 const QList<MirrorUpdate>& arguments)
{
    app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, arguments)));
}

QJsonObject resultFor(LoopbackTransport* app, quint32 id)
{
    for (const QJsonObject& o : ofType(app->received(), QStringLiteral("command.result"))) {
        if (o.value(QStringLiteral("id")).toInteger() == id) {
            return o;
        }
    }
    return {};
}

QString refusalCode(const QJsonObject& result)
{
    for (const QJsonValue& v : result.value(QStringLiteral("values")).toArray()) {
        if (v.toObject().value(QStringLiteral("name")).toString() == QLatin1String("refusalCode")) {
            return v.toObject().value(QStringLiteral("value")).toString();
        }
    }
    return {};
}

// The Core with media on, one transmitting device signed in, and the media
// controller on a fake transport.
struct Station {
    Core core;
    Device device{QStringLiteral("Grant's iPhone"), QStringLiteral("phone"),
                  QStringLiteral("iPhone")};
    QPointer<MicTransport> transport;
    std::unique_ptr<DaemonMediaController> media;
    LoopbackTransport* app{nullptr};
    RemoteMicEncoder encoder;
    quint16 sequence{0};

    explicit Station(bool declareRemoteTx = true)
    {
        allowTransmit(core);
        core.server->setMediaEnabled(true);
        media = std::make_unique<DaemonMediaController>(
            core.server.get(), core.model.get(), nullptr,
            [this](QObject* parent) -> IMediaTransport* {
                auto* t = new MicTransport(parent);
                transport = t;
                return t;
            });
        core.pair(device);
        app = core.signIn(device, declareRemoteTx ? kTransmitter : kHolder);
    }

    QByteArray deviceId() const { return device.key.fingerprint(); }

    bool startMedia(bool remoteTx, bool iq = false)
    {
        app->sendText(mediaStart(remoteTx, QLatin1String(kConnectionId), iq).toUtf8());
        if (!QTest::qWaitFor([this] { return !transport.isNull(); }, 5000)) {
            return false;
        }
        transport->becomeReady();
        return true;
    }

    // One 20 ms Opus packet of a 1 kHz tone on the microphone line.
    void sendMic()
    {
        const std::vector<float> frame =
            tone(static_cast<qint64>(sequence) * 960, 960, 0.3f);
        transport->deliverMic(encoder.encode(frame.data(), sequence,
                                             static_cast<quint32>(sequence) * 960U,
                                             MediaPeer::micAudioSsrcForConnection(
                                                 QLatin1String(kConnectionId))));
        ++sequence;
    }
};

// Fix wave C2: two transmitting devices on one Core, each with its own
// media controller (DaemonMediaHub) and microphone line.
struct TwoStations {
    static constexpr char kConnectionA[] = "3f2504e0-4f89-41d3-9a0c-0305e82c3311";
    static constexpr char kConnectionB[] = "3f2504e0-4f89-41d3-9a0c-0305e82c3322";
    Core core;
    Device a{QStringLiteral("Grant's iPhone"), QStringLiteral("phone"), QStringLiteral("iPhone")};
    Device b{QStringLiteral("Shack iPad"), QStringLiteral("tablet"), QStringLiteral("iPad")};
    QPointer<MicTransport> transportA;
    QPointer<MicTransport> transportB;
    QPointer<MicTransport>* next{nullptr};
    std::unique_ptr<DaemonMediaHub> hub;
    LoopbackTransport* appA{nullptr};
    LoopbackTransport* appB{nullptr};
    RemoteMicEncoder encoderA;
    RemoteMicEncoder encoderB;
    quint16 sequenceA{0};
    quint16 sequenceB{0};

    TwoStations()
    {
        allowTransmit(core);
        core.server->setMediaEnabled(true);
        hub = std::make_unique<DaemonMediaHub>(
            core.server.get(), core.model.get(), nullptr,
            [this](QObject* parent) -> IMediaTransport* {
                auto* t = new MicTransport(parent);
                if (next != nullptr) {
                    *next = t;
                }
                return t;
            });
        core.pair(a);
        core.pair(b);
        appA = core.signIn(a, kTransmitter);
        appB = core.signIn(b, kTransmitter);
    }

    ~TwoStations() { hub.reset(); }

    bool startMedia(LoopbackTransport* app, QPointer<MicTransport>& slot, const char* connection)
    {
        slot = nullptr;
        next = &slot;
        app->sendText(mediaStart(true, QLatin1String(connection)).toUtf8());
        const bool made = QTest::qWaitFor([&slot] { return !slot.isNull(); }, 5000);
        next = nullptr;
        if (!made) {
            return false;
        }
        slot->becomeReady();
        return true;
    }

    // One 20 ms Opus packet of a tone on a device's line.
    static void sendMic(MicTransport* transport, RemoteMicEncoder& encoder, quint16& sequence,
                        const char* connection)
    {
        const std::vector<float> frame = tone(static_cast<qint64>(sequence) * 960, 960, 0.3f);
        transport->deliverMic(encoder.encode(frame.data(), sequence,
                                             static_cast<quint32>(sequence) * 960U,
                                             MediaPeer::micAudioSsrcForConnection(
                                                 QLatin1String(connection))));
        ++sequence;
    }
    void sendMicA() { sendMic(transportA, encoderA, sequenceA, kConnectionA); }
    void sendMicB() { sendMic(transportB, encoderB, sequenceB, kConnectionB); }
};

QStringList g_unkeyLines;
// TX diagnostics lane: the event lines that follow an unkey line.
QStringList g_unkeyEventLines;

void captureUnkeyLines(QtMsgType, const QMessageLogContext& context, const QString& message)
{
    if (context.category != nullptr && QByteArray(context.category) == "nereus.daemon.media"
        && message.startsWith(QLatin1String("Transmit ended ("))) {
        if (message.contains(QLatin1String("; transmit I/Q "))) {
            g_unkeyLines.append(message);
        } else {
            g_unkeyEventLines.append(message);
        }
    }
}

} // namespace

class TestTxWorkerRemoteRing : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qRegisterMetaType<NereusSDR::TxRefusal>();
    }

    // ---- The pump -----------------------------------------------------------

    void ringInUseReplacesTheOperatorsSource();
    void ringFillingIsSilenceNeverALocalSource();
    void radePathTakesTheRing();
    void toneFromTheLineReachesTheTxChannelAtItsLevel();
    void aStandingSendRingIsShedOnlyInSilenceBySkippingPumpBlocks();
    void nothingIsShedWhileDexpTimingRuns();

    // ---- The Core -------------------------------------------------------------

    void micLineOnlyForAStartThatAsks();
    void keyWaitsForTheBufferThenKeys();
    void keyWithoutMicrophoneAudioIsRefusedMicNotReady();
    void tuneKeysAtOnceWithoutAMicrophone();
    void aVoiceKeyWithoutTheMicrophoneLineNeverUsesTheStationsMicrophone();
    void onlyTheKeyedDevicesLineFeedsTheTransmitter();
    void anotherDevicesTeardownLeavesTheHoldersLine();
    void eachTxChannelKeepaliveCountsForItsOwnDevice();
    void stalledIqDoesNotDelayTxWatchdog();
    void aMediaRestartKeepsTheHoldersSource();
    void releasedWhileWaitingItNeverKeys();
    void aLineClosedWhileItsKeyWaitsIsRefusedAtOnce();
    void aLineLostMidKeyLeavesSilenceNotTheStationsMicrophone();
    void voxFromTheDevicesMicrophoneIsTheDevices();
    void everyUnkeyThroughTheMoxControllerLogsTheMicrophoneLine();
    void withTwoLinesOnlyTheKeyersControllerLogsTheUnkey();
    void aKeyAtTheCoreLogsNoMicrophoneLine();

    // ---- The monitor ------------------------------------------------------------

    void monitorReachesTheRemoteAudioAtTheSpeakersLevel();
};

void TestTxWorkerRemoteRing::ringInUseReplacesTheOperatorsSource()
{
    Pump pump;
    pump.block();
    for (double v : lastI(pump.channel)) {
        QCOMPARE(v, static_cast<double>(0.7f));   // the PC microphone
    }

    pump.feed.setInUse(true);
    const std::vector<float> audio(4800, 0.4f);
    QVERIFY(pump.feed.write(audio.data(), 4800));
    for (int i = 0; i < 10; ++i) {
        pump.block();
    }
    for (double v : lastI(pump.channel)) {
        QVERIFY2(std::abs(v - 0.4) < 1e-3, qPrintable(QString::number(v)));
    }
    for (size_t k = 1; k < pump.channel.inForTest().size(); k += 2) {
        QCOMPARE(pump.channel.inForTest()[k], 0.0);
    }

    // Unkeyed: the operator's source is back, and the ring is empty.
    pump.feed.setInUse(false);
    pump.block();
    for (double v : lastI(pump.channel)) {
        QCOMPARE(v, static_cast<double>(0.7f));
    }
    QCOMPARE(pump.feed.stats().fillFrames, 0);
    QVERIFY(!pump.feed.stats().started);
}

void TestTxWorkerRemoteRing::ringFillingIsSilenceNeverALocalSource()
{
    Pump pump;
    pump.feed.setInUse(true);
    const std::vector<float> audio(960, 0.4f);
    QVERIFY(pump.feed.write(audio.data(), 960));
    pump.block();
    for (double v : lastI(pump.channel)) {
        QCOMPARE(v, 0.0);   // neither the PC's 0.7 nor the radio's 0.3
    }
}

// RADE: the ring feeds the RADE encoder's input (after its HPF and the 48 to
// 16 kHz resampler), at the tone's level, and nothing of the PC microphone.
void TestTxWorkerRemoteRing::radePathTakesTheRing()
{
    Pump pump;
    pump.worker.setCurrentTxPath(TxWorkerThread::TxPath::Rade);
    pump.worker.setRadeLeveler(false, 15, 100);
    pump.worker.setRadeMicGainDb(0);
    QSignalSpy blocks(&pump.worker, &TxWorkerThread::radeMicBlockReady);
    pump.feed.setInUse(true);
    constexpr float kAmplitude = 0.3f;
    qint64 written = 0;
    // Unkeyed, the ring's audio never reaches the RADE encoder: the worker
    // hands the microphone to RADE only while keyed (setRadeMicKeyed).
    for (int b = 0; b < 150; ++b) {
        if (b % 15 == 0) {
            const std::vector<float> frame = tone(written, 960, kAmplitude);
            QVERIFY(pump.feed.write(frame.data(), 960));
            written += 960;
        }
        pump.block();
    }
    QCOMPARE(blocks.count(), 0);
    // Keyed as the Core keys for a remote device: RemoteKeying::keyNow
    // keys MoxController as a local key does, and its moxStateChanged(true)
    // sets this (RadioModel::wireTxWorkerRade).
    pump.worker.setRadeMicKeyed(true);
    for (int b = 0; b < 1500; ++b) {
        if (b % 15 == 0) {
            const std::vector<float> frame = tone(written, 960, kAmplitude);
            QVERIFY(pump.feed.write(frame.data(), 960));
            written += 960;
        }
        pump.block();
    }
    std::vector<double> speech;
    for (const QList<QVariant>& call : std::as_const(blocks)) {
        const QByteArray payload = call.at(0).toByteArray();
        const auto* s = reinterpret_cast<const qint16*>(payload.constData());
        for (qsizetype i = 0; i < payload.size() / 2; ++i) {
            speech.push_back(s[i] / 32767.0);
        }
    }
    // 2 s at 16 kHz less the resampler's start; the last second is settled.
    QVERIFY2(speech.size() > 25000, qPrintable(QString::number(speech.size())));
    const double level = rms(speech, speech.size() - 16000);
    const double errorDb = 20.0 * std::log10(level / (kAmplitude / std::sqrt(2.0)));
    QVERIFY2(std::abs(errorDb) < 1.0, qPrintable(QString::number(errorDb)));
}

// SSB: a tone from the app's encoder, through the Core's receiver and the
// ring, reaches the TX channel's input at its level within the buffer's
// latency.
void TestTxWorkerRemoteRing::toneFromTheLineReachesTheTxChannelAtItsLevel()
{
    Pump pump;
    RemoteMicReceiver receiver(&pump.feed);
    RemoteMicEncoder encoder;
    constexpr quint32 kSsrc = 0x6d696302U;
    QVERIFY(receiver.start(kSsrc, false));
    pump.feed.setInUse(true);
    constexpr float kAmplitude = 0.25f;
    std::vector<double> sent;
    int firstHeard = -1;
    int block = 0;
    for (int k = 0; k < 250; ++k) {
        const std::vector<float> frame = tone(static_cast<qint64>(k) * 960, 960, kAmplitude);
        receiver.submit(encoder.encode(frame.data(), static_cast<quint16>(k),
                                       static_cast<quint32>(k * 960), kSsrc));
        for (int b = 0; b < 15; ++b, ++block) {
            pump.block();
            const std::vector<double> i = lastI(pump.channel);
            if (firstHeard < 0 && rms(i) > 0.01) {
                firstHeard = block;
            }
            sent.insert(sent.end(), i.begin(), i.end());
        }
    }
    QVERIFY(firstHeard >= 0);
    QVERIFY2(firstHeard * kBlock * 1000.0 / 48000.0 <= RemoteMicConfig::kTargetDepthMs + 30.0,
             qPrintable(QString::number(firstHeard)));
    const double level = rms(sent, static_cast<size_t>(firstHeard + 300) * kBlock);
    const double errorDb = 20.0 * std::log10(level / (kAmplitude / std::sqrt(2.0)));
    QVERIFY2(std::abs(errorDb) < 1.0, qPrintable(QString::number(errorDb)));
    QCOMPARE(pump.feed.stats().underflows, 0);
}

// R-IOS-13 (2026-09-27): with 20 ms standing in the radio's send ring, the
// remote microphone's buffer sheds it in silence: the pump skips whole
// blocks (TX DSP never runs on them), one a silent 64-frame block of the
// microphone. Under a word nothing is skipped, and the RADE path never
// skips.
void TestTxWorkerRemoteRing::aStandingSendRingIsShedOnlyInSilenceBySkippingPumpBlocks()
{
    for (const bool rade : {false, true}) {
        Pump pump;
        if (rade) {
            pump.worker.setCurrentTxPath(TxWorkerThread::TxPath::Rade);
        }
        pump.connection.queuedMs = 20.0;
        pump.feed.setInUse(true);
        // 1 s of a word, then 1 s of a -80 dBFS noise floor, 20 ms packets.
        quint32 seed = 7U;
        int skippedInWord = 0;
        int skippedInPause = 0;
        std::vector<double> previous;
        for (int b = 0; b < 1500; ++b) {
            if (b % 15 == 0) {
                const qint64 start = static_cast<qint64>(b / 15) * 960;
                std::vector<float> frame = tone(start, 960, 0.3f);
                if (b >= 750) {
                    for (float& x : frame) {
                        seed = seed * 1664525U + 1013904223U;
                        x = 1.0e-4f * (static_cast<float>(seed >> 8) / 8388608.0f - 1.0f);
                    }
                }
                QVERIFY(pump.feed.write(frame.data(), 960));
            }
            pump.block();
            const std::vector<double> now = lastI(pump.channel);
            // A skipped block leaves the TX channel's input as it was: the
            // block played before it, voice or silence.
            if (!previous.empty() && now == previous && rms(now) > 0.0) {
                ++(rms(now) >= 0.01 ? skippedInWord : skippedInPause);
            }
            previous = now;
        }
        const RemoteMicFeed::Stats stats = pump.feed.stats();
        if (rade) {
            QCOMPARE(skippedInPause, 0);
            QCOMPARE(stats.shedForRingFrames, quint64(0));
        } else {
            QCOMPARE(skippedInWord, 0);
            QVERIFY2(skippedInPause >= 10, qPrintable(QString::number(skippedInPause)));
            QCOMPARE(stats.shedForRingFrames, quint64(skippedInPause) * kBlock);
        }
    }
}

// R-IOS-13: while DEXP's hold, decay or VOX turn-off counts
// (TxChannel::dexpTimingRunning), the pump skips no block and the feed
// splices nothing, so VOX and the expander keep their timing; once it ends
// the standing ring is shed in the next silence.
void TestTxWorkerRemoteRing::nothingIsShedWhileDexpTimingRuns()
{
    Pump pump;
    pump.connection.queuedMs = 20.0;
    pump.feed.setInUse(true);
    pump.channel.setDexpTimingRunningForTest(true);
    quint32 seed = 11U;
    int skipped = 0;
    int skippedAfter = 0;
    std::vector<double> previous;
    for (int b = 0; b < 3000; ++b) {
        if (b == 1500) {
            pump.channel.setDexpTimingRunningForTest(false);
        }
        if (b % 15 == 0) {
            // A word for the first 300 ms of every 1 s, then silence.
            const qint64 start = static_cast<qint64>(b / 15) * 960;
            std::vector<float> frame = tone(start, 960, 0.3f);
            if ((b / 15) % 50 >= 15) {
                for (float& x : frame) {
                    seed = seed * 1664525U + 1013904223U;
                    x = 1.0e-4f * (static_cast<float>(seed >> 8) / 8388608.0f - 1.0f);
                }
            }
            QVERIFY(pump.feed.write(frame.data(), 960));
        }
        pump.block();
        const std::vector<double> now = lastI(pump.channel);
        if (!previous.empty() && now == previous && rms(now) > 0.0) {
            ++(b < 1500 ? skipped : skippedAfter);
        }
        previous = now;
    }
    QCOMPARE(skipped, 0);
    QVERIFY2(skippedAfter >= 10, qPrintable(QString::number(skippedAfter)));
    QVERIFY(pump.feed.stats().heldBlocks >= 1400);
}

// Older peers get no microphone line; a start with remoteTxVersion gets it,
// and only from a peer the Core told remoteTx.
void TestTxWorkerRemoteRing::micLineOnlyForAStartThatAsks()
{
    {
        Station station;
        QVERIFY(admitted(station.app));
        QVERIFY(station.startMedia(false));
        QCOMPARE(station.transport->options.micAudioSsrc, quint32(0));
        QVERIFY(station.media->micReceiver() == nullptr);
        QVERIFY(!station.core.model->remoteMicLineOpen(station.deviceId()));
    }
    {
        Station station;
        QVERIFY(station.startMedia(true));
        QCOMPARE(station.transport->options.micAudioSsrc,
                 MediaPeer::micAudioSsrcForConnection(QLatin1String(kConnectionId)));
        QVERIFY(station.media->micReceiver() != nullptr);
        QVERIFY(station.core.model->remoteMicLineOpen(station.deviceId()));
        QVERIFY(!station.core.model->remoteMicInUse());
    }
    {
        // A device that did not declare remoteTx was never told the
        // capability: its start is refused and no peer starts.
        Station station(/*declareRemoteTx=*/false);
        QVERIFY(admitted(station.app));
        station.app->sendText(mediaStart(true).toUtf8());
        QTest::qWait(50);
        QVERIFY(station.transport.isNull());
        QVERIFY(!station.core.model->remoteMicLineOpen(station.deviceId()));
    }
}

// A key in a voice mode waits for the line's buffer, then keys; the ring is
// the source from the wait to the unkey, and at unkey the operator's source
// returns with the ring empty.
void TestTxWorkerRemoteRing::keyWaitsForTheBufferThenKeys()
{
    Station station;
    QVERIFY(station.startMedia(true));
    MoxController* mox = station.core.model->moxController();
    RemoteMicFeed* feed = station.core.model->remoteMicFeed();
    QVERIFY(feed != nullptr);

    sendCommand(station.app, "tx.key", 3601, {utf8("trigger", QStringLiteral("screen"))});
    QTRY_VERIFY(station.core.model->remoteMicInUse());
    QVERIFY(!mox->isMox());
    QVERIFY(resultFor(station.app, 3601).isEmpty());
    // One packet (20 ms): still waiting (R-IOS-13, 2026-09-27: the target
    // is 30 ms, one packet plus a 10 ms margin).
    station.sendMic();
    QTest::qWait(20);
    QVERIFY(!mox->isMox());
    QVERIFY(resultFor(station.app, 3601).isEmpty());
    // The second reaches 40 ms, past the 30 ms target: the key keys.
    station.sendMic();
    QTRY_VERIFY(!resultFor(station.app, 3601).isEmpty());
    const QJsonObject result = resultFor(station.app, 3601);
    QVERIFY2(result.value(QStringLiteral("accepted")).toBool(),
             qPrintable(QJsonDocument(result).toJson(QJsonDocument::Compact)));
    QTRY_VERIFY(mox->isMox());
    QCOMPARE(station.core.model->keyedBy().deviceId, station.deviceId());
    QVERIFY(station.core.model->remoteMicInUse());
    QVERIFY(feed->inUse());
    QVERIFY(station.media->micReceiver()->isWatching());

    // A copy is answered with the same epoch.
    sendCommand(station.app, "tx.key", 3601, {utf8("trigger", QStringLiteral("screen"))});
    QTRY_COMPARE(ofType(station.app->received(), QStringLiteral("command.result")).size(), 2);

    const qint64 epoch = [&result] {
        for (const QJsonValue& v : result.value(QStringLiteral("values")).toArray()) {
            if (v.toObject().value(QStringLiteral("name")).toString() == QLatin1String("epoch")) {
                return v.toObject().value(QStringLiteral("value")).toInteger();
            }
        }
        return qint64(0);
    }();
    QVERIFY(epoch >= 1);
    sendCommand(station.app, "tx.unkey", 3602, {int64("epoch", epoch)});
    QTRY_VERIFY(!mox->isMox());
    QTRY_VERIFY(!station.core.model->remoteMicInUse());
    QVERIFY(!feed->inUse());
    QCOMPARE(feed->framesSinceInUse(), qint64(0));
    // The ring is empty for the pump: it takes nothing, and the operator's
    // source applies.
    std::vector<float> out(kBlock);
    QVERIFY(!feed->pull(out.data(), kBlock));
    QCOMPARE(feed->stats().fillFrames, 0);
    QVERIFY(!station.media->micReceiver()->isWatching());
}

void TestTxWorkerRemoteRing::keyWithoutMicrophoneAudioIsRefusedMicNotReady()
{
    Station station;
    QVERIFY(station.startMedia(true));
    QElapsedTimer waited;
    waited.start();
    sendCommand(station.app, "tx.key", 3611, {utf8("trigger", QStringLiteral("screen"))});
    QTRY_VERIFY_WITH_TIMEOUT(!resultFor(station.app, 3611).isEmpty(), 5000);
    // Load findings 2: a line that never sends is refused at the line's
    // start bound.
    QVERIFY2(waited.elapsed() >= RemoteMicConfig::kLineStartDeadlineMs - 10,
             qPrintable(QString::number(waited.elapsed())));
    const QJsonObject result = resultFor(station.app, 3611);
    QVERIFY(!result.value(QStringLiteral("accepted")).toBool(true));
    QCOMPARE(refusalCode(result), QString::fromLatin1(TxRefusals::kMicNotReady));
    // Fix wave M4: the words say to wait for the device's microphone.
    QCOMPARE(result.value(QStringLiteral("reason")).toString(),
             TxRefusals::remoteMicNotReady().text);
    QVERIFY(!station.core.model->moxController()->isMox());
    QVERIFY(!station.core.model->remoteMicInUse());
}

void TestTxWorkerRemoteRing::tuneKeysAtOnceWithoutAMicrophone()
{
    Station station;
    QVERIFY(station.startMedia(true));
    sendCommand(station.app, "tx.tune", 3621,
                {MirrorUpdate{0, QByteArray("on"), MirrorWireKind::Bool, QVariant(true)}});
    QTRY_VERIFY(!resultFor(station.app, 3621).isEmpty());
    QVERIFY(resultFor(station.app, 3621).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(station.core.model->moxController()->isMox());
    sendCommand(station.app, "tx.tune", 3622,
                {MirrorUpdate{0, QByteArray("on"), MirrorWireKind::Bool, QVariant(false)}});
    QTRY_VERIFY(!station.core.model->moxController()->isMox());
}

// Fix wave C1: a voice key (any person's trigger) or a program's key from a
// device whose media carries no microphone line is refused at once with a
// plain reason, and nothing keys: the Core never puts its own microphone
// on the air for a remote key. TUNE and two-tone need no microphone and
// key as before.
void TestTxWorkerRemoteRing::aVoiceKeyWithoutTheMicrophoneLineNeverUsesTheStationsMicrophone()
{
    Station station;
    // No media at all (a reconnect before media is back).
    quint32 id = 3700;
    for (const char* trigger : {"screen", "headset", "bluetooth", "actionButton", "tci"}) {
        ++id;
        sendCommand(station.app, "tx.key", id, {utf8("trigger", QString::fromLatin1(trigger))});
        QTRY_VERIFY(!resultFor(station.app, id).isEmpty());
        const QJsonObject result = resultFor(station.app, id);
        QVERIFY2(!result.value(QStringLiteral("accepted")).toBool(true), trigger);
        if (QByteArray(trigger) != "tci") {
            QCOMPARE(refusalCode(result), QString::fromLatin1(TxRefusals::kMicNotReady));
            QCOMPARE(result.value(QStringLiteral("reason")).toString(),
                     TxRefusals::micNotConnected().text);
        }
        QVERIFY(!station.core.model->moxController()->isMox());
        QVERIFY(!station.core.model->remoteMicInUse());
    }
    // Media without the line (a start that did not carry remoteTxVersion).
    QVERIFY(station.startMedia(false));
    sendCommand(station.app, "tx.key", ++id, {utf8("trigger", QStringLiteral("screen"))});
    QTRY_VERIFY(!resultFor(station.app, id).isEmpty());
    QCOMPARE(refusalCode(resultFor(station.app, id)), QString::fromLatin1(TxRefusals::kMicNotReady));
    QVERIFY(!station.core.model->moxController()->isMox());
    // A plain wording.
    QVERIFY(OperatorWording::isPlain(TxRefusals::micNotConnected().text));
    // TUNE needs no microphone: it keys.
    sendCommand(station.app, "tx.tune", ++id,
                {MirrorUpdate{0, QByteArray("on"), MirrorWireKind::Bool, QVariant(true)}});
    QTRY_VERIFY(!resultFor(station.app, id).isEmpty());
    QVERIFY(resultFor(station.app, id).value(QStringLiteral("accepted")).toBool());
    QTRY_VERIFY(station.core.model->moxController()->isMox());
    sendCommand(station.app, "tx.tune", ++id,
                {MirrorUpdate{0, QByteArray("on"), MirrorWireKind::Bool, QVariant(false)}});
    QTRY_VERIFY(!station.core.model->moxController()->isMox());
}

// Fix wave C2: with two devices streaming, only the keyed device's line
// feeds the transmitter; the other's audio never reaches it.
void TestTxWorkerRemoteRing::onlyTheKeyedDevicesLineFeedsTheTransmitter()
{
    TwoStations s;
    QVERIFY(s.startMedia(s.appA, s.transportA, TwoStations::kConnectionA));
    QVERIFY(s.startMedia(s.appB, s.transportB, TwoStations::kConnectionB));
    RemoteMicFeed* feed = s.core.model->remoteMicFeed();
    MoxController* mox = s.core.model->moxController();
    sendCommand(s.appA, "tx.key", 3801, {utf8("trigger", QStringLiteral("screen"))});
    for (int i = 0; i < 6 && !mox->isMox(); ++i) {
        s.sendMicA();
        s.sendMicB();
        QTest::qWait(5);
    }
    QTRY_VERIFY(!resultFor(s.appA, 3801).isEmpty());
    QVERIFY2(resultFor(s.appA, 3801).value(QStringLiteral("accepted")).toBool(),
             qPrintable(resultFor(s.appA, 3801).value(QStringLiteral("reason")).toString()));
    QTRY_VERIFY(mox->isMox());
    QCOMPARE(s.core.model->keyedBy().deviceId, s.a.key.fingerprint());
    QVERIFY(feed->inUse());
    // B streams while A is keyed: nothing of B's reaches the transmitter.
    const qint64 before = feed->framesSinceInUse();
    for (int i = 0; i < 5; ++i) {
        s.sendMicB();
    }
    QCOMPARE(feed->framesSinceInUse(), before);
    // A's does.
    s.sendMicA();
    QCOMPARE(feed->framesSinceInUse(), before + 960);
    // B's key is refused (A holds transmit), and still nothing of B's.
    sendCommand(s.appB, "tx.key", 3802, {utf8("trigger", QStringLiteral("screen"))});
    QTRY_VERIFY(!resultFor(s.appB, 3802).isEmpty());
    QVERIFY(!resultFor(s.appB, 3802).value(QStringLiteral("accepted")).toBool(true));
    s.sendMicB();
    QCOMPARE(feed->framesSinceInUse(), before + 960);
    sendCommand(s.appA, "tx.unkey", 3803, {int64("epoch", s.core.model->keyedBy().epoch)});
    QTRY_VERIFY(!mox->isMox());
}

// Fix wave C2: B's media ending (its controller torn down) never clears
// A's line: A keeps its uplink, keys on it, and its audio still reaches.
void TestTxWorkerRemoteRing::anotherDevicesTeardownLeavesTheHoldersLine()
{
    TwoStations s;
    QVERIFY(s.startMedia(s.appA, s.transportA, TwoStations::kConnectionA));
    QVERIFY(s.startMedia(s.appB, s.transportB, TwoStations::kConnectionB));
    QTRY_COMPARE(s.hub->controllerCount(), 2);
    // B leaves: its controller goes.
    s.core.invoke(s.appB, "session.leave");
    QTRY_COMPARE(s.hub->controllerCount(), 1);
    MoxController* mox = s.core.model->moxController();
    sendCommand(s.appA, "tx.key", 3811, {utf8("trigger", QStringLiteral("screen"))});
    for (int i = 0; i < 6 && !mox->isMox(); ++i) {
        s.sendMicA();
        QTest::qWait(5);
    }
    QTRY_VERIFY(!resultFor(s.appA, 3811).isEmpty());
    QVERIFY2(resultFor(s.appA, 3811).value(QStringLiteral("accepted")).toBool(),
             qPrintable(resultFor(s.appA, 3811).value(QStringLiteral("reason")).toString()));
    QTRY_VERIFY(mox->isMox());
    RemoteMicFeed* feed = s.core.model->remoteMicFeed();
    const qint64 before = feed->framesSinceInUse();
    s.sendMicA();
    QCOMPARE(feed->framesSinceInUse(), before + 960);
    sendCommand(s.appA, "tx.unkey", 3812, {int64("epoch", s.core.model->keyedBy().epoch)});
    QTRY_VERIFY(!mox->isMox());
}

// Fix wave C2: a "tx" data channel keepalive counts for the device whose
// media connection it came on: B's keepalives never keep A's key alive.
void TestTxWorkerRemoteRing::eachTxChannelKeepaliveCountsForItsOwnDevice()
{
    TwoStations s;
    QVERIFY(s.startMedia(s.appA, s.transportA, TwoStations::kConnectionA));
    QVERIFY(s.startMedia(s.appB, s.transportB, TwoStations::kConnectionB));
    MoxController* mox = s.core.model->moxController();
    sendCommand(s.appA, "tx.key", 3821, {utf8("trigger", QStringLiteral("screen"))});
    for (int i = 0; i < 6 && !mox->isMox(); ++i) {
        s.sendMicA();
        QTest::qWait(5);
    }
    QTRY_VERIFY(mox->isMox());
    // A's own channel keepalives keep its key on past the deadline.
    quint64 sequenceA = 1;
    for (int i = 0; i < 16; ++i) {
        s.transportA->deliverTx(RemoteTxWatchdog::channelKeepalive(sequenceA++, 4294967295U));
        s.sendMicA();
        QTest::qWait(20);
        s.core.now += 50;
    }
    QVERIFY(mox->isMox());
    // Only B keeps sending keepalives on its channel; A's key stops within
    // the watchdog's deadline.
    QElapsedTimer since;
    since.start();
    quint64 sequence = 1;
    // The Core's clock is the harness's; it moves with the real time here.
    qint64 moved = 0;
    while (mox->isMox() && since.elapsed() < 3000) {
        s.transportB->deliverTx(RemoteTxWatchdog::channelKeepalive(sequence++, 4294967295U));
        s.sendMicA();   // A's audio keeps flowing: only its keepalives are missing
        QTest::qWait(50);
        s.core.now += 50;
        moved += 50;
    }
    QVERIFY(!mox->isMox());
    // Within the watchdog's deadline on the Core's clock (400 ms, plus a
    // step of this loop and the timer's own).
    QVERIFY2(moved <= 600, qPrintable(QString::number(moved)));
}

void TestTxWorkerRemoteRing::stalledIqDoesNotDelayTxWatchdog()
{
    Station station;
    station.core.model->configureStreamPool(5, 5, 192000);
    const QJsonObject added = station.core.invoke(
        station.app, "addSlice", {utf8("initialPanId", QString())});
    QVERIFY(added.value(QStringLiteral("accepted")).toBool());
    const SliceModel* slice = nullptr;
    for (const SliceModel* candidate : station.core.model->slices()) {
        if (candidate->streamIndex() >= 0
            && station.core.server->mediaSessionControlsSlice(
                station.core.server->mediaSessionEpoch(), candidate->sliceIndex())) {
            slice = candidate;
            break;
        }
    }
    QVERIFY(slice);
    QVERIFY(station.core.server->setDisplayBudgetLimits({10'000'000, 10'000'000, 1}));
    QVERIFY(station.startMedia(true, true));
    QVERIFY(station.transport->options.iqChannel);
    SessionMessage request;
    request.kind = SessionMessageKind::MediaControl;
    request.mediaPayload = {{QStringLiteral("op"), QStringLiteral("iq-stream")},
        {QStringLiteral("connectionId"), QLatin1String(kConnectionId)},
        {QStringLiteral("sliceId"), slice->sliceIndex()},
        {QStringLiteral("revision"), 1},
        {QStringLiteral("enabled"), true}};
    station.app->sendText(SessionMessages::encode(request));
    QTRY_VERIFY(([&] {
        for (const QJsonObject& wire : ofType(station.app->received(),
                                              QStringLiteral("media.control"))) {
            const QJsonObject context = wire.value(QStringLiteral("payload")).toObject();
            if (context.value(QStringLiteral("op")) == QLatin1String("iq-stream-context")
                && context.value(QStringLiteral("enabled")).toBool()) { return true; }
        }
        return false;
    })());
    station.transport->stallIq = true;
    station.core.model->rawIqDataForStream(slice->streamIndex(), QVector<float>(2048, 0.25f));

    sendCommand(station.app, "tx.key", 3841, {utf8("trigger", QStringLiteral("screen"))});
    for (int i = 0; i < 6 && !station.core.model->moxController()->isMox(); ++i) {
        station.sendMic();
        QTest::qWait(5);
    }
    QTRY_VERIFY(station.core.model->moxController()->isMox());
    QVERIFY(resultFor(station.app, 3841).value(QStringLiteral("accepted")).toBool());
    qint64 moved = 0;
    QElapsedTimer since;
    since.start();
    while (station.core.model->moxController()->isMox() && since.elapsed() < 3000) {
        // No TX keepalive. The I/Q sender is wedged while microphone audio
        // continues, so only the watchdog may end this key.
        station.sendMic();
        QTest::qWait(50);
        station.core.now += 50;
        moved += 50;
    }
    QVERIFY(!station.core.model->moxController()->isMox());
    QVERIFY2(moved <= 600, qPrintable(QString::number(moved)));
}

// Fix wave C2: the holder's media restarting mid-key leaves the ring the
// source (silence) and never the station's microphone; the new line feeds
// it again.
void TestTxWorkerRemoteRing::aMediaRestartKeepsTheHoldersSource()
{
    TwoStations s;
    QVERIFY(s.startMedia(s.appA, s.transportA, TwoStations::kConnectionA));
    QVERIFY(s.startMedia(s.appB, s.transportB, TwoStations::kConnectionB));
    MoxController* mox = s.core.model->moxController();
    sendCommand(s.appA, "tx.key", 3831, {utf8("trigger", QStringLiteral("screen"))});
    for (int i = 0; i < 6 && !mox->isMox(); ++i) {
        s.sendMicA();
        QTest::qWait(5);
    }
    QTRY_VERIFY(mox->isMox());
    RemoteMicFeed* feed = s.core.model->remoteMicFeed();
    s.transportA->closeUnexpectedly();
    QTRY_VERIFY(s.hub->controllerFor(s.core.server->mediaSessionEpochs().first()) == nullptr
                || s.hub->controllerFor(s.core.server->mediaSessionEpochs().first())->micReceiver()
                       == nullptr);
    QVERIFY(mox->isMox());
    QVERIFY(s.core.model->remoteMicInUse());
    QVERIFY(feed->inUse());
    // B streams meanwhile: never the transmitter's source.
    const qint64 before = feed->framesSinceInUse();
    s.sendMicB();
    QCOMPARE(feed->framesSinceInUse(), before);
    // A's media starts again: its new line feeds the ring.
    QVERIFY(s.startMedia(s.appA, s.transportA, TwoStations::kConnectionA));
    QTRY_VERIFY([&]() {
        const qint64 now = feed->framesSinceInUse();
        s.sendMicA();
        return feed->framesSinceInUse() > now;
    }());
    QVERIFY(mox->isMox());
    mox->setMox(false);
    QTRY_VERIFY(!mox->isMox());
}

// The device lets go before its buffer filled: the key never keys, and is
// answered keyEnded.
void TestTxWorkerRemoteRing::releasedWhileWaitingItNeverKeys()
{
    Station station;
    QVERIFY(station.startMedia(true));
    sendCommand(station.app, "tx.key", 3631, {utf8("trigger", QStringLiteral("screen"))});
    QTRY_VERIFY(station.core.model->remoteMicInUse());
    sendCommand(station.app, "tx.unkey", 3632, {int64("epoch", 1)});
    QTRY_VERIFY(!resultFor(station.app, 3631).isEmpty());
    QCOMPARE(refusalCode(resultFor(station.app, 3631)), QString::fromLatin1(TxRefusals::kKeyEnded));
    QTRY_VERIFY(!resultFor(station.app, 3632).isEmpty());
    QVERIFY(resultFor(station.app, 3632).value(QStringLiteral("accepted")).toBool());
    for (int i = 0; i < 5; ++i) {
        station.sendMic();
    }
    // Load findings 3: past both of the key's wait bounds (the line's
    // start, then its fill), so no timer of the ended wait can key.
    QTest::qWait(RemoteMicConfig::kLineStartDeadlineMs + RemoteMicConfig::kReadyDeadlineMs + 50);
    QVERIFY(!station.core.model->moxController()->isMox());
    QVERIFY(!station.core.model->remoteMicInUse());
}

// Load findings 3 (review of the line-start wait): the device's line
// closes while its key waits, after the line's first packet: the key is
// refused at once (well inside the 1 s start bound), and never keys.
void TestTxWorkerRemoteRing::aLineClosedWhileItsKeyWaitsIsRefusedAtOnce()
{
    Station station;
    QVERIFY(station.startMedia(true));
    sendCommand(station.app, "tx.key", 3651, {utf8("trigger", QStringLiteral("screen"))});
    QTRY_VERIFY(station.core.model->remoteMicInUse());
    // One 20 ms packet: the line has started, under its 30 ms target.
    station.sendMic();
    QVERIFY(resultFor(station.app, 3651).isEmpty());
    QElapsedTimer closed;
    closed.start();
    station.transport->closeUnexpectedly();
    QTRY_VERIFY(!resultFor(station.app, 3651).isEmpty());
    QVERIFY2(closed.elapsed() < RemoteMicConfig::kLineStartDeadlineMs,
             qPrintable(QString::number(closed.elapsed())));
    const QJsonObject result = resultFor(station.app, 3651);
    QVERIFY(!result.value(QStringLiteral("accepted")).toBool(true));
    QCOMPARE(refusalCode(result), QString::fromLatin1(TxRefusals::kMicNotReady));
    QVERIFY(!station.core.model->moxController()->isMox());
}

// The device's media drops while it is keyed on its line: the ring stays
// the transmitter's source (it hears silence) until the key ends, never the
// station's own microphone; then the configured source returns.
void TestTxWorkerRemoteRing::aLineLostMidKeyLeavesSilenceNotTheStationsMicrophone()
{
    Station station;
    QVERIFY(station.startMedia(true));
    sendCommand(station.app, "tx.key", 3641, {utf8("trigger", QStringLiteral("screen"))});
    QTRY_VERIFY(station.core.model->remoteMicInUse());
    for (int i = 0; i < 3; ++i) {
        station.sendMic();
    }
    QTRY_VERIFY(station.core.model->moxController()->isMox());
    station.transport->closeUnexpectedly();
    QTRY_VERIFY(station.media->micReceiver() == nullptr);
    QVERIFY(!station.core.model->remoteMicLineOpen(station.deviceId()));
    QVERIFY(station.core.model->moxController()->isMox());
    QVERIFY(station.core.model->remoteMicInUse());
    QVERIFY(station.core.model->remoteMicFeed()->inUse());

    station.core.model->moxController()->setMox(false);
    QTRY_VERIFY(!station.core.model->moxController()->isMox());
    QTRY_VERIFY(!station.core.model->remoteMicInUse());
    QVERIFY(!station.core.model->remoteMicFeed()->inUse());
}

// VOX armed (VOX on, the device's session may transmit): the ring is the
// source while unkeyed, so VOX listens to the device's microphone, and a
// VOX key is the device's, shown as VOX. VOX off: the operator's source
// returns and the ring is empty.
void TestTxWorkerRemoteRing::voxFromTheDevicesMicrophoneIsTheDevices()
{
    Station station;
    QVERIFY(station.startMedia(true));
    RadioModel* model = station.core.model.get();
    QVERIFY(!model->remoteMicInUse());
    // Fix wave 2: VOX turned on at the Core itself (nobody armed it from a
    // device) never listens to a device's line.
    model->transmitModel().setVoxEnabled(true);
    QTest::qWait(50);
    QVERIFY(!model->remoteMicInUse());
    QVERIFY(model->remoteVoxDevice().isEmpty());
    model->transmitModel().setVoxEnabled(false);
    // iPhone app plan Task 77 (ruling 8.4): arming VOX needs holding
    // transmit, so the device takes it first (a take on unheld transmit).
    {
        TransmitHolder::KeyRequest take;
        take.deviceId = station.deviceId();
        QCOMPARE(station.core.server->transmitHolder()->askKey(take).verdict, KeyingVerdict::Admit);
    }
    // Armed from the device (its write), it listens to that device's line.
    station.app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
        QByteArrayLiteral("transmit"),
        {MirrorUpdate{0, QByteArrayLiteral("voxEnabled"), MirrorWireKind::Bool, QVariant(true)}},
        4401)));
    QTRY_VERIFY(model->transmitModel().voxEnabled());
    QTRY_VERIFY(model->remoteMicInUse());
    QCOMPARE(model->remoteVoxDevice(), station.deviceId());
    QVERIFY(!model->moxController()->isMox());

    model->moxController()->onVoxActive(true);
    QTRY_VERIFY(model->moxController()->isMox());
    QCOMPARE(model->keyedBy().deviceId, station.deviceId());
    QCOMPARE(model->keyedBy().trigger, QByteArrayLiteral("vox"));
    QCOMPARE(model->radioStatus().activePttSource(), PttSource::Vox);
    QVERIFY(station.core.server->transmitHolder()->isHeldBy(station.deviceId()));
    QVERIFY(model->transmitModel().voxEnabled());   // the VOX key keeps VOX armed

    model->moxController()->onVoxActive(false);
    QTRY_VERIFY(!model->moxController()->isMox());
    QVERIFY(model->remoteMicInUse());   // still armed
    QCOMPARE(model->radioStatus().activePttSource(), PttSource::None);

    model->transmitModel().setVoxEnabled(false);
    QTRY_VERIFY(!model->remoteMicInUse());
    QVERIFY(!model->remoteMicFeed()->inUse());
}

// With MON on and keyed, the Core's program for a remote device carries the
// transmit monitor, sample for sample what the local speakers play (master
// volume at 1; the remote audio never carries the speakers' own volume).
// Two receivers: while keyed the transmit slice leaves the mix, and the
// other one paces it. (With the transmit slice the only receiver nothing
// paces the mix while keyed, so neither the speakers nor the remote audio
// carry the monitor: the same audio either way. See the task report.)
void TestTxWorkerRemoteRing::monitorReachesTheRemoteAudioAtTheSpeakersLevel()
{
    RadioModel radio;
    AudioEngine* engine = radio.audioEngine();
    AudioFormat fmt{};
    fmt.sampleRate = 48000;
    fmt.channels = 2;
    fmt.sample = AudioFormat::Sample::Float32;
    auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
    speakers->open(fmt);
    FakeAudioBus* speakerBus = speakers.get();
    engine->setSpeakersBusForTest(std::move(speakers));
    radio.configureStreamPool(5, 5, 192000);
    const int slice = radio.addSlice();
    const int other = radio.addSlice();
    QVERIFY(slice >= 0 && other >= 0);
    engine->setVolume(1.0f);
    engine->setTxMonitorVolume(0.5f);
    engine->setTxMonitorEnabled(true);
    ProgramTap tap;
    engine->setMasterMixAudioTap(&tap);
    engine->setMoxStateForTest(true);

    constexpr int kFrames = 64;
    const std::vector<float> silence(kFrames * 2, 0.0f);
    for (int p = 0; p < 200; ++p) {
        const std::vector<float> mon = tone(static_cast<qint64>(p) * kFrames, kFrames, 0.4f);
        engine->txMonitorBlockReady(mon.data(), kFrames);
        engine->rxBlockReady(slice, silence.data(), kFrames);
        engine->rxBlockReady(other, silence.data(), kFrames);
    }
    engine->clearMasterMixAudioTap(&tap);

    const QByteArray played = speakerBus->buffer();
    const auto* speakerSamples = reinterpret_cast<const float*>(played.constData());
    const int speakerCount = static_cast<int>(played.size() / static_cast<int>(sizeof(float)));
    QVERIFY2(!tap.heard.empty(), "the Core sent no audio while keyed");
    QCOMPARE(static_cast<int>(tap.heard.size()), speakerCount);
    double peak = 0.0;
    for (int i = 0; i < speakerCount; ++i) {
        QCOMPARE(tap.heard[static_cast<size_t>(i)], speakerSamples[i]);
        peak = std::max(peak, std::abs(static_cast<double>(tap.heard[static_cast<size_t>(i)])));
    }
    // MON at its volume: 0.4 x 0.5.
    QVERIFY2(std::abs(peak - 0.2) < 0.01, qPrintable(QString::number(peak)));
}

// TX stall lane: MoxController owns MOX, so the unkey's line follows its
// walk. Before, the line hung off TransmitModel::moxChanged, which a
// controller's unkey never reaches, and it never printed on the bench.
void TestTxWorkerRemoteRing::everyUnkeyThroughTheMoxControllerLogsTheMicrophoneLine()
{
    g_unkeyLines.clear();
    g_unkeyEventLines.clear();
    const QtMessageHandler previous = qInstallMessageHandler(captureUnkeyLines);
    const auto restore = qScopeGuard([previous] { qInstallMessageHandler(previous); });

    Station station;
    QVERIFY(station.startMedia(true));
    MoxController* mox = station.core.model->moxController();
    RemoteMicFeed* feed = station.core.model->remoteMicFeed();
    QVERIFY(feed != nullptr);

    // A device's key, unkeyed by the device.
    sendCommand(station.app, "tx.key", 3901, {utf8("trigger", QStringLiteral("screen"))});
    QTRY_VERIFY(station.core.model->remoteMicInUse());
    station.sendMic();
    station.sendMic();
    QTRY_VERIFY(mox->isMox());
    // TX mic thread fix round 2: "tx" keepalives that waited at the Core.
    quint64 keepaliveSequence = 0;
    station.transport->deliverTx(
        RemoteTxWatchdog::channelKeepalive(++keepaliveSequence, 4294967295U), 90'000);
    station.transport->deliverTx(
        RemoteTxWatchdog::channelKeepalive(++keepaliveSequence, 4294967295U), 3'000);
    // The pump plays the 40 ms the line delivered and then runs dry: one
    // underrun in this over.
    std::vector<float> out(kBlock);
    for (int block = 0; block < 60; ++block) {
        QVERIFY(feed->pull(out.data(), kBlock));
    }
    const int underruns = feed->stats().underflows;
    QVERIFY(underruns >= 1);
    sendCommand(station.app, "tx.unkey", 3902,
                {int64("epoch", station.core.model->keyedBy().epoch)});
    QTRY_VERIFY(!mox->isMox());
    QTRY_COMPARE(g_unkeyLines.size(), 1);
    const QString first = g_unkeyLines.at(0);
    // The device's id in hex, as the watchdog's line names it.
    QVERIFY2(first.contains(QString::fromLatin1(station.deviceId().toHex())), qPrintable(first));
    // The over's underruns, taken before the feed left use and reset them.
    QVERIFY2(first.contains(QStringLiteral("underruns %1,").arg(underruns)), qPrintable(first));
    QVERIFY2(first.contains(QStringLiteral("transmit I/Q")), qPrintable(first));
    // The microphone's waits are the line's; the event loop's show in the
    // over's longest keepalive wait.
    QVERIFY2(first.contains(QStringLiteral("; line waits mean ")), qPrintable(first));
    QVERIFY2(first.contains(QStringLiteral("; keepalive waits max 90.0 ms; ")), qPrintable(first));
    // TX diagnostics lane: the over's underrun placed in a line of its own.
    QVERIFY2(!g_unkeyEventLines.isEmpty()
                 && g_unkeyEventLines.at(0).contains(QStringLiteral("microphone underrun 1 at +")),
             qPrintable(g_unkeyEventLines.join(QStringLiteral(" | "))));

    // A second key, ended at the Core (as the transmit watchdog ends one).
    sendCommand(station.app, "tx.key", 3903, {utf8("trigger", QStringLiteral("screen"))});
    QTRY_VERIFY(station.core.model->remoteMicInUse());
    station.sendMic();
    station.sendMic();
    QTRY_VERIFY(mox->isMox());
    station.transport->deliverTx(
        RemoteTxWatchdog::channelKeepalive(++keepaliveSequence, 4294967295U), 37'000);
    mox->setMox(false);
    QTRY_COMPARE(g_unkeyLines.size(), 2);
    // Each over's own: the first over's 90 ms is not the second's.
    QVERIFY2(g_unkeyLines.at(1).contains(QStringLiteral("; keepalive waits max 37.0 ms; ")),
             qPrintable(g_unkeyLines.at(1)));
    QTRY_COMPARE(mox->state(), MoxState::Rx);
    // One line per unkey, never two.
    QCOMPARE(g_unkeyLines.size(), 2);
}

// TX stall lane, fix round 1: with two devices carrying a microphone
// line, an unkey prints one line, under the keyer's id.
void TestTxWorkerRemoteRing::withTwoLinesOnlyTheKeyersControllerLogsTheUnkey()
{
    g_unkeyLines.clear();
    g_unkeyEventLines.clear();
    const QtMessageHandler previous = qInstallMessageHandler(captureUnkeyLines);
    const auto restore = qScopeGuard([previous] { qInstallMessageHandler(previous); });

    TwoStations s;
    QVERIFY(s.startMedia(s.appA, s.transportA, TwoStations::kConnectionA));
    QVERIFY(s.startMedia(s.appB, s.transportB, TwoStations::kConnectionB));
    QTRY_COMPARE(s.hub->controllerCount(), 2);
    MoxController* mox = s.core.model->moxController();
    sendCommand(s.appA, "tx.key", 3911, {utf8("trigger", QStringLiteral("screen"))});
    for (int i = 0; i < 6 && !mox->isMox(); ++i) {
        s.sendMicA();
        s.sendMicB();
        QTest::qWait(5);
    }
    QTRY_VERIFY(!resultFor(s.appA, 3911).isEmpty());
    QVERIFY2(resultFor(s.appA, 3911).value(QStringLiteral("accepted")).toBool(),
             qPrintable(resultFor(s.appA, 3911).value(QStringLiteral("reason")).toString()));
    QTRY_VERIFY(mox->isMox());
    QCOMPARE(s.core.model->keyedBy().deviceId, s.a.key.fingerprint());
    sendCommand(s.appA, "tx.unkey", 3912, {int64("epoch", s.core.model->keyedBy().epoch)});
    QTRY_VERIFY(!mox->isMox());
    QTRY_COMPARE(mox->state(), MoxState::Rx);
    QCOMPARE(g_unkeyLines.size(), 1);
    const QString line = g_unkeyLines.at(0);
    QVERIFY2(line.contains(QString::fromLatin1(s.a.key.fingerprint().toHex())), qPrintable(line));
    QVERIFY2(!line.contains(QString::fromLatin1(s.b.key.fingerprint().toHex())), qPrintable(line));
}

// TX stall lane, fix round 1: a key at the Core itself is on no device's
// microphone line, so no controller reports it, though two carry a line.
void TestTxWorkerRemoteRing::aKeyAtTheCoreLogsNoMicrophoneLine()
{
    g_unkeyLines.clear();
    g_unkeyEventLines.clear();
    const QtMessageHandler previous = qInstallMessageHandler(captureUnkeyLines);
    const auto restore = qScopeGuard([previous] { qInstallMessageHandler(previous); });

    TwoStations s;
    QVERIFY(s.startMedia(s.appA, s.transportA, TwoStations::kConnectionA));
    QVERIFY(s.startMedia(s.appB, s.transportB, TwoStations::kConnectionB));
    QTRY_COMPARE(s.hub->controllerCount(), 2);
    MoxController* mox = s.core.model->moxController();
    mox->setMox(true, KeyerIdentity::station(PttMode::Manual));
    QTRY_VERIFY2(mox->isMox(), qPrintable(mox->lastRefusal().text));
    QVERIFY(!s.core.model->remoteMicInUse());
    mox->setMox(false, KeyerIdentity::station(PttMode::Manual));
    QTRY_COMPARE(mox->state(), MoxState::Rx);
    QCOMPARE(g_unkeyLines.size(), 0);
}

QTEST_MAIN(TestTxWorkerRemoteRing)
#include "tst_tx_worker_remote_ring.moc"
